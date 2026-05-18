#include "virasa/renderer/core/Instance.h"

#include <vulkan/vulkan.h>

#include <quill/LogMacros.h>

#include <cstring>
#include <vector>

#include "virasa/core/Logger.h"

namespace virasa
{

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace
{

static constexpr const char* kValidationLayerName = "VK_LAYER_KHRONOS_validation";
static constexpr const char* kDebugUtilsExtName = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
static constexpr const char* kLoggerModuleName = "renderer.vulkan";

/// @brief Vulkan debug messenger callback — forwards messages to virasa Logger.
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugMessengerCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT /*messageType*/,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* /*pUserData*/)
{
	if (!pCallbackData || !pCallbackData->pMessage)
		return VK_FALSE;

	quill::Logger* logger = Logger::GetLogger(kLoggerModuleName);

	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
	{
		LOG_ERROR(logger, "{}", pCallbackData->pMessage);
	}
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
	{
		LOG_WARNING(logger, "{}", pCallbackData->pMessage);
	}
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
	{
		LOG_INFO(logger, "{}", pCallbackData->pMessage);
	}
	else
	{
		// VERBOSE
		LOG_TRACE_L1(logger, "{}", pCallbackData->pMessage);
	}

	return VK_FALSE;
}

/// @brief Fills out a VkDebugUtilsMessengerCreateInfoEXT for all severities and types.
static VkDebugUtilsMessengerCreateInfoEXT MakeDebugMessengerCreateInfo()
{
	VkDebugUtilsMessengerCreateInfoEXT info{};
	info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
				     VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
				     VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
				     VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
				 VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
				 VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	info.pfnUserCallback = DebugMessengerCallback;
	info.pUserData = nullptr;
	return info;
}

/// @brief Checks whether a layer name is present in the enumerated layer list.
static bool LayerAvailable(const std::vector<VkLayerProperties>& layers, const char* name)
{
	for (const auto& layer : layers)
	{
		if (std::strcmp(layer.layerName, name) == 0)
			return true;
	}
	return false;
}

/// @brief Checks whether an extension name is present in the enumerated extension list.
static bool ExtensionAvailable(
	const std::vector<VkExtensionProperties>& extensions, const char* name)
{
	for (const auto& ext : extensions)
	{
		if (std::strcmp(ext.extensionName, name) == 0)
			return true;
	}
	return false;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Instance — special members
// ---------------------------------------------------------------------------

Instance::Instance(Instance&& other) noexcept
    : _instance(other._instance), _debugMessenger(other._debugMessenger),
	_validationEnabled(other._validationEnabled)
{
	other._instance = VK_NULL_HANDLE;
	other._debugMessenger = VK_NULL_HANDLE;
	other._validationEnabled = false;
}

Instance& Instance::operator=(Instance&& other) noexcept
{
	if (this != &other)
	{
		Destroy();

		_instance = other._instance;
		_debugMessenger = other._debugMessenger;
		_validationEnabled = other._validationEnabled;

		other._instance = VK_NULL_HANDLE;
		other._debugMessenger = VK_NULL_HANDLE;
		other._validationEnabled = false;
	}
	return *this;
}

Instance::~Instance()
{
	Destroy();
}

// ---------------------------------------------------------------------------
// Instance — private helpers
// ---------------------------------------------------------------------------

void Instance::Destroy() noexcept
{
	if (_instance != VK_NULL_HANDLE)
	{
		if (_debugMessenger != VK_NULL_HANDLE)
		{
			auto vkDestroyDebugUtilsMessengerEXT =
				reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
					vkGetInstanceProcAddr(_instance, "vkDestroyDebugUtilsMessengerEXT"));
			if (vkDestroyDebugUtilsMessengerEXT)
				vkDestroyDebugUtilsMessengerEXT(_instance, _debugMessenger, nullptr);
			_debugMessenger = VK_NULL_HANDLE;
		}
		vkDestroyInstance(_instance, nullptr);
		_instance = VK_NULL_HANDLE;
	}
	_validationEnabled = false;
}

// ---------------------------------------------------------------------------
// Instance — public API
// ---------------------------------------------------------------------------

RenderError Instance::Initialize(const RendererConfig& config)
{
	// Step 0: guard against double-initialization.
	if (_instance != VK_NULL_HANDLE)
		return RenderError::AlreadyInitialized;

	// Step 1: compute effective extension list.
	std::vector<const char*> extensions;
	extensions.reserve(static_cast<std::size_t>(config.requiredInstanceExtensionCount) +
				 (config.enableValidation ? 1u : 0u));

	for (uint32_t i = 0; i < config.requiredInstanceExtensionCount; ++i)
		extensions.push_back(config.requiredInstanceExtensions[i]);

	if (config.enableValidation)
	{
		// Add VK_EXT_debug_utils only if not already present.
		bool alreadyPresent = false;
		for (const char* ext : extensions)
		{
			if (std::strcmp(ext, kDebugUtilsExtName) == 0)
			{
				alreadyPresent = true;
				break;
			}
		}
		if (!alreadyPresent)
			extensions.push_back(kDebugUtilsExtName);
	}

	// Step 2: compute effective layer list.
	std::vector<const char*> layers;
	if (config.enableValidation)
		layers.push_back(kValidationLayerName);

	// Step 3: query available layers and extensions, verify availability.
	{
		uint32_t layerCount = 0;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* required : layers)
		{
			if (!LayerAvailable(availableLayers, required))
				return RenderError::VulkanNotAvailable;
		}

		uint32_t extCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
		std::vector<VkExtensionProperties> availableExts(extCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extCount, availableExts.data());

		for (const char* required : extensions)
		{
			if (!ExtensionAvailable(availableExts, required))
				return RenderError::VulkanNotAvailable;
		}
	}

	// Step 4: call vkCreateInstance.
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = config.applicationName;
	appInfo.applicationVersion = config.applicationVersion;
	appInfo.pEngineName = "Virasa";
	appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
	appInfo.apiVersion = VK_API_VERSION_1_3;

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
	createInfo.ppEnabledLayerNames = layers.empty() ? nullptr : layers.data();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.empty() ? nullptr : extensions.data();

	// Chain debug messenger create info so validation covers instance creation/destruction.
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	if (config.enableValidation)
	{
		debugCreateInfo = MakeDebugMessengerCreateInfo();
		createInfo.pNext = &debugCreateInfo;
	}

	VkInstance newInstance = VK_NULL_HANDLE;
	VkResult result = vkCreateInstance(&createInfo, nullptr, &newInstance);
	if (result != VK_SUCCESS)
		return RenderError::InstanceCreateFailed;

	// Step 5: optionally create the debug messenger.
	if (config.enableValidation)
	{
		auto vkCreateDebugUtilsMessengerEXT =
			reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
				vkGetInstanceProcAddr(newInstance, "vkCreateDebugUtilsMessengerEXT"));

		VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
		if (!vkCreateDebugUtilsMessengerEXT ||
			vkCreateDebugUtilsMessengerEXT(
				newInstance, &debugCreateInfo, nullptr, &messenger) != VK_SUCCESS)
		{
			vkDestroyInstance(newInstance, nullptr);
			return RenderError::InstanceCreateFailed;
		}

		_debugMessenger = messenger;
	}

	// Commit state only on full success.
	_instance = newInstance;
	_validationEnabled = config.enableValidation;
	return RenderError::None;
}

VkInstance Instance::GetHandle() const noexcept
{
	return _instance;
}

bool Instance::IsValidationEnabled() const noexcept
{
	return _validationEnabled;
}

bool Instance::IsInitialized() const noexcept
{
	return _instance != VK_NULL_HANDLE;
}

} // namespace virasa
