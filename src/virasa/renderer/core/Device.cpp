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

/// Evaluate the queue families available on a physical device for a given surface.
QueueFamilies SelectQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
	uint32_t familyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, nullptr);

	std::vector<VkQueueFamilyProperties> families(familyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, families.data());

	QueueFamilies result{};

	const bool headless = (surface == VK_NULL_HANDLE);

	for (uint32_t i = 0; i < familyCount; ++i)
	{
		const VkQueueFamilyProperties& props = families[i];

		const bool hasGraphics = (props.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
		const bool hasTransfer = (props.queueFlags & VK_QUEUE_TRANSFER_BIT) != 0;

		bool hasPresent = false;
		if (!headless)
		{
			VkBool32 presentSupport = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
			hasPresent = (presentSupport == VK_TRUE);
		}

		// Single-family override: a family that supports both graphics and present wins.
		if (hasGraphics && hasPresent)
		{
			result.graphicsFamily = i;
			result.presentFamily = i;
			result.graphicsFound = true;
			result.presentFound = true;
		}
		else
		{
			// Graphics resolution (only if not yet found).
			if (!result.graphicsFound && hasGraphics)
			{
				result.graphicsFamily = i;
				result.graphicsFound = true;
			}

			// Present resolution (only if not yet found).
			if (!result.presentFound && hasPresent)
			{
				result.presentFamily = i;
				result.presentFound = true;
			}
		}

		// Dedicated transfer: transfer but NOT graphics.
		if (!result.dedicatedTransfer && hasTransfer && !hasGraphics)
		{
			result.transferFamily = i;
			result.dedicatedTransfer = true;
		}
	}

	// Transfer fallback: use the graphics family when no dedicated transfer family exists.
	if (!result.dedicatedTransfer && result.graphicsFound)
	{
		result.transferFamily = result.graphicsFamily;
		// dedicatedTransfer remains false to distinguish from the dedicated case.
	}

	// Headless fallback: no surface means no present queue is needed; use the
	// graphics family so that IsComplete() succeeds and the device can be created.
	if (headless && result.graphicsFound && !result.presentFound)
	{
		result.presentFamily = result.graphicsFamily;
		result.presentFound = true;
	}

	return result;
}

/// Compute the score for a candidate physical device.
int ScorePhysicalDevice(const VkPhysicalDeviceProperties& props)
{
	int score = 0;

	switch (props.deviceType)
	{
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
			score += 1000;
			break;
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
			score += 100;
			break;
		default:
			break;
	}

	score += static_cast<int>(props.limits.maxImageDimension2D);
	return score;
}

/// Check whether a physical device meets all hard requirements.
bool MeetsHardRequirements(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
	const VkPhysicalDeviceProperties& props, QueueFamilies& outQueueFamilies,
	quill::Logger* logger)
{
	// Hard requirement 1: Vulkan 1.3+
	if (props.apiVersion < VK_API_VERSION_1_3)
	{
		LOG_INFO(logger,
			"  '{}': rejected — Vulkan {}.{} < 1.3 required.",
			props.deviceName,
			VK_API_VERSION_MAJOR(props.apiVersion),
			VK_API_VERSION_MINOR(props.apiVersion));
		return false;
	}

	// Hard requirement 2: VK_KHR_swapchain extension.
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
		LOG_INFO(logger, "  '{}': rejected — missing VK_KHR_swapchain.", props.deviceName);
		return false;
	}

	// Hard requirement 3: queue families complete.
	QueueFamilies qf = SelectQueueFamilies(physicalDevice, surface);
	if (!qf.IsComplete())
	{
		LOG_INFO(logger,
			"  '{}': rejected — incomplete queue families (graphicsFound={}, "
			"presentFound={}).",
			props.deviceName,
			qf.graphicsFound,
			qf.presentFound);
		return false;
	}

	// Hard requirement 4: Vulkan 1.3 core features (dynamicRendering + synchronization2).
	VkPhysicalDeviceVulkan13Features features13{};
	features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	features13.pNext = nullptr;

	VkPhysicalDeviceFeatures2 features2{};
	features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.pNext = &features13;

	vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);

	if (features13.dynamicRendering != VK_TRUE || features13.synchronization2 != VK_TRUE)
	{
		LOG_INFO(logger,
			"  '{}': rejected — missing 1.3 features (dynamicRendering={}, "
			"synchronization2={}).",
			props.deviceName,
			features13.dynamicRendering == VK_TRUE,
			features13.synchronization2 == VK_TRUE);
		return false;
	}

	outQueueFamilies = qf;
	return true;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Special members
// ---------------------------------------------------------------------------

Device::Device(Device&& other) noexcept
    : _device(other._device), _physicalDevice(other._physicalDevice),
	_graphicsQueue(other._graphicsQueue), _presentQueue(other._presentQueue),
	_transferQueue(other._transferQueue), _queueFamilies(other._queueFamilies),
	_properties(other._properties), _memoryProperties(other._memoryProperties)
{
	other._device = VK_NULL_HANDLE;
	other._physicalDevice = VK_NULL_HANDLE;
	other._graphicsQueue = VK_NULL_HANDLE;
	other._presentQueue = VK_NULL_HANDLE;
	other._transferQueue = VK_NULL_HANDLE;
	other._queueFamilies = {};
	other._properties = {};
	other._memoryProperties = {};
}

Device& Device::operator=(Device&& other) noexcept
{
	if (this == &other)
		return *this;

	// Destroy any currently owned handle.
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
	_properties = other._properties;
	_memoryProperties = other._memoryProperties;

	other._device = VK_NULL_HANDLE;
	other._physicalDevice = VK_NULL_HANDLE;
	other._graphicsQueue = VK_NULL_HANDLE;
	other._presentQueue = VK_NULL_HANDLE;
	other._transferQueue = VK_NULL_HANDLE;
	other._queueFamilies = {};
	other._properties = {};
	other._memoryProperties = {};

	return *this;
}

Device::~Device()
{
	if (_device != VK_NULL_HANDLE)
	{
		vkDestroyDevice(_device, nullptr);
		_device = VK_NULL_HANDLE;
	}
}

// ---------------------------------------------------------------------------
// Initialize
// ---------------------------------------------------------------------------

RenderError Device::Initialize(const Instance& instance, VkSurfaceKHR surface)
{
	auto* logger = Logger::GetLogger("renderer");

	VkInstance vkInstance = instance.GetHandle();

	// Enumerate physical devices.
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(vkInstance, &deviceCount, nullptr);

	if (deviceCount == 0)
	{
		LOG_ERROR(logger, "No Vulkan physical devices found.");
		return RenderError::NoSuitableDevice;
	}

	std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
	vkEnumeratePhysicalDevices(vkInstance, &deviceCount, physicalDevices.data());

	// Evaluate each physical device.
	struct Candidate
	{
		VkPhysicalDevice physicalDevice;
		QueueFamilies queueFamilies;
		VkPhysicalDeviceProperties properties;
		VkPhysicalDeviceMemoryProperties memoryProperties;
		int score;
		size_t enumerationIndex;
	};

	std::vector<Candidate> candidates;

	for (size_t i = 0; i < physicalDevices.size(); ++i)
	{
		VkPhysicalDevice pd = physicalDevices[i];

		VkPhysicalDeviceProperties props{};
		vkGetPhysicalDeviceProperties(pd, &props);

		QueueFamilies qf{};
		if (!MeetsHardRequirements(pd, surface, props, qf, logger))
		{
			LOG_INFO(logger,
				"Physical device '{}' does not meet hard requirements; skipping.",
				props.deviceName);
			continue;
		}

		const int score = ScorePhysicalDevice(props);
		LOG_INFO(logger, "Candidate physical device '{}' score: {}.", props.deviceName, score);

		VkPhysicalDeviceMemoryProperties memProps{};
		vkGetPhysicalDeviceMemoryProperties(pd, &memProps);

		candidates.push_back(Candidate{pd, qf, props, memProps, score, i});
	}

	if (candidates.empty())
	{
		LOG_ERROR(logger, "No suitable Vulkan physical device found.");
		return RenderError::NoSuitableDevice;
	}

	// Select the candidate with the highest score; earlier enumeration wins ties.
	const Candidate* best = &candidates[0];
	for (size_t i = 1; i < candidates.size(); ++i)
	{
		if (candidates[i].score > best->score)
			best = &candidates[i];
	}

	LOG_INFO(logger,
		"Selected physical device '{}' with score {}.",
		best->properties.deviceName,
		best->score);

	// Query optional features on the selected device.
	VkPhysicalDeviceVulkan12Features features12{};
	features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	features12.pNext = nullptr;

	VkPhysicalDeviceVulkan13Features features13{};
	features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	features13.pNext = nullptr;

	VkPhysicalDeviceFeatures2 features2{};
	features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.pNext = &features12;
	features12.pNext = &features13;

	vkGetPhysicalDeviceFeatures2(best->physicalDevice, &features2);

	const bool enableBufferDeviceAddress = (features12.bufferDeviceAddress == VK_TRUE);
	const bool enableDescriptorIndexing = (features12.descriptorIndexing == VK_TRUE);
	const bool enableSamplerAnisotropy = (features2.features.samplerAnisotropy == VK_TRUE);
	const bool enableGeometryShader = (features2.features.geometryShader == VK_TRUE);

	LOG_INFO(logger,
		"Optional feature bufferDeviceAddress: {}.",
		enableBufferDeviceAddress ? "enabled" : "skipped");
	LOG_INFO(logger,
		"Optional feature descriptorIndexing: {}.",
		enableDescriptorIndexing ? "enabled" : "skipped");
	LOG_INFO(logger,
		"Optional feature samplerAnisotropy: {}.",
		enableSamplerAnisotropy ? "enabled" : "skipped");
	LOG_INFO(logger,
		"Optional feature geometryShader: {}.",
		enableGeometryShader ? "enabled" : "skipped");

	// Build unique queue family set.
	const QueueFamilies& qf = best->queueFamilies;
	std::set<uint32_t> uniqueFamilies;
	uniqueFamilies.insert(qf.graphicsFamily);
	uniqueFamilies.insert(qf.presentFamily);
	if (qf.dedicatedTransfer)
		uniqueFamilies.insert(qf.transferFamily);

	const float queuePriority = 1.0f;
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	queueCreateInfos.reserve(uniqueFamilies.size());

	for (uint32_t familyIndex : uniqueFamilies)
	{
		VkDeviceQueueCreateInfo qci{};
		qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		qci.queueFamilyIndex = familyIndex;
		qci.queueCount = 1;
		qci.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(qci);
	}

	// Build feature chain for device creation.
	VkPhysicalDeviceVulkan12Features enabledFeatures12{};
	enabledFeatures12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	enabledFeatures12.pNext = nullptr;
	enabledFeatures12.bufferDeviceAddress = enableBufferDeviceAddress ? VK_TRUE : VK_FALSE;
	enabledFeatures12.descriptorIndexing = enableDescriptorIndexing ? VK_TRUE : VK_FALSE;

	VkPhysicalDeviceVulkan13Features enabledFeatures13{};
	enabledFeatures13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	enabledFeatures13.pNext = nullptr;
	enabledFeatures13.dynamicRendering = VK_TRUE;
	enabledFeatures13.synchronization2 = VK_TRUE;

	VkPhysicalDeviceFeatures2 enabledFeatures2{};
	enabledFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	enabledFeatures2.features.samplerAnisotropy = enableSamplerAnisotropy ? VK_TRUE : VK_FALSE;
	enabledFeatures2.features.geometryShader = enableGeometryShader ? VK_TRUE : VK_FALSE;
	enabledFeatures2.pNext = &enabledFeatures12;
	enabledFeatures12.pNext = &enabledFeatures13;

	const char* deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

	VkDeviceCreateInfo deviceCI{};
	deviceCI.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCI.pNext = &enabledFeatures2;
	deviceCI.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	deviceCI.pQueueCreateInfos = queueCreateInfos.data();
	deviceCI.enabledExtensionCount = 1;
	deviceCI.ppEnabledExtensionNames = deviceExtensions;
	deviceCI.pEnabledFeatures = nullptr; // Using pNext chain instead.

	VkDevice newDevice = VK_NULL_HANDLE;
	const VkResult result = vkCreateDevice(best->physicalDevice, &deviceCI, nullptr, &newDevice);

	if (result != VK_SUCCESS)
	{
		LOG_ERROR(logger, "vkCreateDevice failed with VkResult {}.", static_cast<int>(result));
		return RenderError::DeviceCreateFailed;
	}

	// Commit state.
	_device = newDevice;
	_physicalDevice = best->physicalDevice;
	_queueFamilies = best->queueFamilies;
	_properties = best->properties;
	_memoryProperties = best->memoryProperties;

	// Retrieve queue handles.
	vkGetDeviceQueue(_device, _queueFamilies.graphicsFamily, 0, &_graphicsQueue);
	vkGetDeviceQueue(_device, _queueFamilies.presentFamily, 0, &_presentQueue);

	if (_queueFamilies.dedicatedTransfer)
	{
		vkGetDeviceQueue(_device, _queueFamilies.transferFamily, 0, &_transferQueue);
		LOG_INFO(logger,
			"Using dedicated transfer queue (family {}).",
			_queueFamilies.transferFamily);
	}
	else
	{
		_transferQueue = _graphicsQueue;
		LOG_INFO(logger,
			"No dedicated transfer queue; using graphics queue as transfer-queue fallback.");
	}

	return RenderError::None;
}

// ---------------------------------------------------------------------------
// Observers
// ---------------------------------------------------------------------------

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
	return _properties;
}

bool Device::IsInitialized() const noexcept
{
	return _device != VK_NULL_HANDLE;
}

void Device::WaitIdle() const
{
	if (_device != VK_NULL_HANDLE)
		vkDeviceWaitIdle(_device);
}

} // namespace virasa
