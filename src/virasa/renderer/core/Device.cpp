#include "virasa/renderer/core/Device.h"

#include <quill/LogMacros.h>

#include <algorithm>
#include <cstring>
#include <set>
#include <vector>

#include "virasa/core/Logger.h"
#include "vulkan/vulkan.h"

namespace virasa
{

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace
{

/// Evaluate the hard requirements for a physical device candidate.
/// Returns true if the device satisfies all six hard requirements.
bool EvaluateHardRequirements(
	VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, const QueueFamilies& families)
{
	// Hard requirement 1: API version >= 1.3
	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties(physicalDevice, &props);
	if (props.apiVersion < VK_API_VERSION_1_3)
	{
		return false;
	}

	// Hard requirement 2: VK_KHR_swapchain extension
	uint32_t extCount = 0;
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, nullptr);
	std::vector<VkExtensionProperties> extensions(extCount);
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, extensions.data());

	bool hasSwapchain = false;
	for (const auto& ext : extensions)
	{
		if (std::strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
		{
			hasSwapchain = true;
			break;
		}
	}
	if (!hasSwapchain)
	{
		return false;
	}

	// Hard requirement 3: QueueFamilies IsComplete
	if (!families.IsComplete())
	{
		return false;
	}

	// Hard requirements 4, 5, 6: query features via pNext chain
	VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures = {};
	descriptorIndexingFeatures.sType =
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
	descriptorIndexingFeatures.pNext = nullptr;

	VkPhysicalDeviceVulkan12Features vk12Features = {};
	vk12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	vk12Features.pNext = &descriptorIndexingFeatures;

	VkPhysicalDeviceVulkan13Features vk13Features = {};
	vk13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	vk13Features.pNext = &vk12Features;

	VkPhysicalDeviceFeatures2 features2 = {};
	features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.pNext = &vk13Features;

	vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);

	// Hard requirement 4: dynamicRendering and synchronization2
	if (vk13Features.dynamicRendering != VK_TRUE || vk13Features.synchronization2 != VK_TRUE)
	{
		return false;
	}

	// Hard requirement 5: bufferDeviceAddress
	if (vk12Features.bufferDeviceAddress != VK_TRUE)
	{
		return false;
	}

	// Hard requirement 6: descriptor indexing sub-features
	if (descriptorIndexingFeatures.descriptorBindingPartiallyBound != VK_TRUE ||
		descriptorIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind != VK_TRUE ||
		descriptorIndexingFeatures.descriptorBindingStorageImageUpdateAfterBind != VK_TRUE ||
		descriptorIndexingFeatures.shaderStorageImageArrayNonUniformIndexing != VK_TRUE ||
		descriptorIndexingFeatures.runtimeDescriptorArray != VK_TRUE)
	{
		return false;
	}

	return true;
}

/// Compute the score for a candidate physical device.
int ScorePhysicalDevice(const VkPhysicalDeviceProperties& props)
{
	int score = 0;

	if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
	{
		score += 1000;
	}
	else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
	{
		score += 100;
	}

	score += static_cast<int>(props.limits.maxImageDimension2D);
	return score;
}

/// Run the queue family selection algorithm for a physical device and surface.
QueueFamilies SelectQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
	QueueFamilies families = {};

	uint32_t familyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, nullptr);
	std::vector<VkQueueFamilyProperties> familyProps(familyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, familyProps.data());

	for (uint32_t i = 0; i < familyCount; ++i)
	{
		const auto& props = familyProps[i];
		bool hasGraphics = (props.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
		bool hasTransfer = (props.queueFlags & VK_QUEUE_TRANSFER_BIT) != 0;

		VkBool32 presentSupport = VK_FALSE;
		if (surface != VK_NULL_HANDLE)
			vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);

		// Single-family override: supports both graphics and present
		if (hasGraphics && presentSupport == VK_TRUE)
		{
			families.graphicsFamily = i;
			families.presentFamily = i;
			families.graphicsFound = true;
			families.presentFound = true;
		}
		else
		{
			// Graphics resolution
			if (!families.graphicsFound && hasGraphics)
			{
				families.graphicsFamily = i;
				families.graphicsFound = true;
			}

			// Present resolution
			if (!families.presentFound && presentSupport == VK_TRUE)
			{
				families.presentFamily = i;
				families.presentFound = true;
			}
		}

		// Dedicated transfer resolution: transfer but NOT graphics
		if (!families.dedicatedTransfer && hasTransfer && !hasGraphics)
		{
			families.transferFamily = i;
			families.dedicatedTransfer = true;
		}
	}

	// Headless fallback: no surface means present queue is irrelevant; alias graphics.
	if (surface == VK_NULL_HANDLE && families.graphicsFound && !families.presentFound)
	{
		families.presentFamily = families.graphicsFamily;
		families.presentFound = true;
	}

	// Transfer fallback: use graphics family if no dedicated transfer
	if (!families.dedicatedTransfer && families.graphicsFound)
	{
		families.transferFamily = families.graphicsFamily;
		// dedicatedTransfer remains false
	}

	return families;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Device implementation
// ---------------------------------------------------------------------------

Device::~Device()
{
	if (_device != VK_NULL_HANDLE)
	{
		vkDestroyDevice(_device, nullptr);
		_device = VK_NULL_HANDLE;
	}
}

Device::Device(Device&& other) noexcept
    : _device(other._device), _physicalDevice(other._physicalDevice),
	_graphicsQueue(other._graphicsQueue), _presentQueue(other._presentQueue),
	_transferQueue(other._transferQueue), _queueFamilies(other._queueFamilies),
	_deviceProperties(other._deviceProperties), _memoryProperties(other._memoryProperties)
{
	other._device = VK_NULL_HANDLE;
	other._physicalDevice = VK_NULL_HANDLE;
	other._graphicsQueue = VK_NULL_HANDLE;
	other._presentQueue = VK_NULL_HANDLE;
	other._transferQueue = VK_NULL_HANDLE;
	other._queueFamilies = {};
	other._deviceProperties = {};
	other._memoryProperties = {};
}

Device& Device::operator=(Device&& other) noexcept
{
	if (this == &other)
	{
		return *this;
	}

	if (_device != VK_NULL_HANDLE)
	{
		vkDestroyDevice(_device, nullptr);
	}

	_device = other._device;
	_physicalDevice = other._physicalDevice;
	_graphicsQueue = other._graphicsQueue;
	_presentQueue = other._presentQueue;
	_transferQueue = other._transferQueue;
	_queueFamilies = other._queueFamilies;
	_deviceProperties = other._deviceProperties;
	_memoryProperties = other._memoryProperties;

	other._device = VK_NULL_HANDLE;
	other._physicalDevice = VK_NULL_HANDLE;
	other._graphicsQueue = VK_NULL_HANDLE;
	other._presentQueue = VK_NULL_HANDLE;
	other._transferQueue = VK_NULL_HANDLE;
	other._queueFamilies = {};
	other._deviceProperties = {};
	other._memoryProperties = {};

	return *this;
}

RenderError Device::Initialize(const Instance& instance, VkSurfaceKHR surface)
{
	auto* logger = Logger::GetLogger("renderer");

	if (!instance.IsInitialized())
	{
		LOG_ERROR(logger, "Device::Initialize — instance is not initialized.");
		return RenderError::DeviceCreateFailed;
	}

	// Enumerate physical devices
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance.GetHandle(), &deviceCount, nullptr);

	if (deviceCount == 0)
	{
		LOG_ERROR(logger, "No physical devices found.");
		return RenderError::NoSuitableDevice;
	}

	std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
	vkEnumeratePhysicalDevices(instance.GetHandle(), &deviceCount, physicalDevices.data());

	// Evaluate and score each physical device
	VkPhysicalDevice selectedDevice = VK_NULL_HANDLE;
	QueueFamilies selectedFamilies = {};
	VkPhysicalDeviceProperties selectedProperties = {};
	int bestScore = -1;

	for (uint32_t i = 0; i < deviceCount; ++i)
	{
		VkPhysicalDevice candidate = physicalDevices[i];

		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(candidate, &props);

		QueueFamilies families = SelectQueueFamilies(candidate, surface);

		if (!EvaluateHardRequirements(candidate, surface, families))
		{
			LOG_INFO(logger,
				"Physical device '{}' disqualified (failed hard requirements).",
				props.deviceName);
			continue;
		}

		int score = ScorePhysicalDevice(props);
		LOG_INFO(logger,
			"Physical device '{}' is a candidate with score {}.",
			props.deviceName,
			score);

		if (score > bestScore)
		{
			bestScore = score;
			selectedDevice = candidate;
			selectedFamilies = families;
			selectedProperties = props;
		}
	}

	if (selectedDevice == VK_NULL_HANDLE)
	{
		LOG_ERROR(logger, "No suitable physical device found.");
		return RenderError::NoSuitableDevice;
	}

	LOG_INFO(logger,
		"Selected physical device '{}' with score {}.",
		selectedProperties.deviceName,
		bestScore);

	// Cache physical device state (cleared on vkCreateDevice failure)
	VkPhysicalDeviceMemoryProperties memProps = {};
	vkGetPhysicalDeviceMemoryProperties(selectedDevice, &memProps);

	// Query optional features
	VkPhysicalDeviceFeatures2 availableFeatures2 = {};
	availableFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	vkGetPhysicalDeviceFeatures2(selectedDevice, &availableFeatures2);
	bool samplerAnisotropy = (availableFeatures2.features.samplerAnisotropy == VK_TRUE);

	if (samplerAnisotropy)
	{
		LOG_INFO(logger, "Optional feature samplerAnisotropy: enabled.");
	}
	else
	{
		LOG_INFO(logger, "Optional feature samplerAnisotropy: skipped (not supported).");
	}

	// Build unique queue family set
	std::set<uint32_t> uniqueFamilies;
	uniqueFamilies.insert(selectedFamilies.graphicsFamily);
	uniqueFamilies.insert(selectedFamilies.presentFamily);
	if (selectedFamilies.dedicatedTransfer)
	{
		uniqueFamilies.insert(selectedFamilies.transferFamily);
	}

	const float queuePriority = 1.0f;
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	queueCreateInfos.reserve(uniqueFamilies.size());
	for (uint32_t familyIndex : uniqueFamilies)
	{
		VkDeviceQueueCreateInfo queueInfo = {};
		queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueInfo.queueFamilyIndex = familyIndex;
		queueInfo.queueCount = 1;
		queueInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueInfo);
	}

	// Build feature chain — all descriptor indexing knobs live in vk12Features;
	// using a separate VkPhysicalDeviceDescriptorIndexingFeatures alongside
	// VkPhysicalDeviceVulkan12Features in the same pNext chain violates VUID-02830.
	VkPhysicalDeviceVulkan13Features vk13Features = {};
	vk13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	vk13Features.pNext = nullptr;
	vk13Features.dynamicRendering = VK_TRUE;
	vk13Features.synchronization2 = VK_TRUE;

	VkPhysicalDeviceVulkan12Features vk12Features = {};
	vk12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	vk12Features.pNext = &vk13Features;
	vk12Features.bufferDeviceAddress = VK_TRUE;
	vk12Features.descriptorIndexing = VK_TRUE;
	vk12Features.descriptorBindingPartiallyBound = VK_TRUE;
	vk12Features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
	vk12Features.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
	vk12Features.runtimeDescriptorArray = VK_TRUE;
	vk12Features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
	vk12Features.shaderStorageImageArrayNonUniformIndexing = VK_TRUE;
	vk12Features.scalarBlockLayout = VK_TRUE;

	VkPhysicalDeviceFeatures2 enabledFeatures2 = {};
	enabledFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	enabledFeatures2.pNext = &vk12Features;
	enabledFeatures2.features.samplerAnisotropy = samplerAnisotropy ? VK_TRUE : VK_FALSE;
	enabledFeatures2.features.shaderInt64 = VK_TRUE;

	const char* enabledExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pNext = &enabledFeatures2;
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
	deviceCreateInfo.enabledExtensionCount = 1;
	deviceCreateInfo.ppEnabledExtensionNames = enabledExtensions;
	deviceCreateInfo.pEnabledFeatures = nullptr; // using pNext chain

	VkDevice newDevice = VK_NULL_HANDLE;
	VkResult result = vkCreateDevice(selectedDevice, &deviceCreateInfo, nullptr, &newDevice);
	if (result != VK_SUCCESS)
	{
		LOG_ERROR(logger, "vkCreateDevice failed with result {}.", static_cast<int>(result));
		// Leave Device in default-constructed state
		return RenderError::DeviceCreateFailed;
	}

	// Retrieve queue handles
	VkQueue graphicsQueue = VK_NULL_HANDLE;
	VkQueue presentQueue = VK_NULL_HANDLE;
	VkQueue transferQueue = VK_NULL_HANDLE;

	vkGetDeviceQueue(newDevice, selectedFamilies.graphicsFamily, 0, &graphicsQueue);
	vkGetDeviceQueue(newDevice, selectedFamilies.presentFamily, 0, &presentQueue);

	if (selectedFamilies.dedicatedTransfer)
	{
		vkGetDeviceQueue(newDevice, selectedFamilies.transferFamily, 0, &transferQueue);
		LOG_INFO(logger,
			"Using dedicated transfer queue (family {}).",
			selectedFamilies.transferFamily);
	}
	else
	{
		transferQueue = graphicsQueue;
		LOG_INFO(logger,
			"Using graphics queue as transfer-queue fallback (family {}).",
			selectedFamilies.graphicsFamily);
	}

	// Commit state
	_device = newDevice;
	_physicalDevice = selectedDevice;
	_graphicsQueue = graphicsQueue;
	_presentQueue = presentQueue;
	_transferQueue = transferQueue;
	_queueFamilies = selectedFamilies;
	_deviceProperties = selectedProperties;
	_memoryProperties = memProps;

	return RenderError::None;
}

VkDevice Device::GetHandle() const noexcept
{
	return _device;
}

VkPhysicalDevice Device::GetPhysicalDevice() const noexcept
{
	return _physicalDevice;
}

VkQueue Device::GetGraphicsQueue() const noexcept
{
	return _graphicsQueue;
}

VkQueue Device::GetPresentQueue() const noexcept
{
	return _presentQueue;
}

VkQueue Device::GetTransferQueue() const noexcept
{
	return _transferQueue;
}

const QueueFamilies& Device::GetQueueFamilies() const noexcept
{
	return _queueFamilies;
}

uint32_t Device::GetGraphicsQueueFamilyIndex() const noexcept
{
	return _queueFamilies.graphicsFamily;
}

const VkPhysicalDeviceMemoryProperties& Device::GetMemoryProperties() const noexcept
{
	return _memoryProperties;
}

const VkPhysicalDeviceProperties& Device::GetProperties() const noexcept
{
	return _deviceProperties;
}

bool Device::IsInitialized() const noexcept
{
	return _device != VK_NULL_HANDLE;
}

VkDeviceAddress Device::GetBufferDeviceAddress(VkBuffer buffer) const noexcept
{
	VkBufferDeviceAddressInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	info.buffer = buffer;
	return vkGetBufferDeviceAddress(_device, &info);
}

bool Device::IsBufferDeviceAddressEnabled() const noexcept
{
	return IsInitialized();
}

bool Device::IsDescriptorIndexingEnabled() const noexcept
{
	return IsInitialized();
}

void Device::WaitIdle() const
{
	if (_device != VK_NULL_HANDLE)
	{
		vkDeviceWaitIdle(_device);
	}
}

} // namespace virasa
