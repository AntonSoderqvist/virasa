#include "virasa/renderer/core/Surface.h"

#include <quill/LogMacros.h>

#include <vector>

#include "virasa/core/Logger.h"
#include "vulkan/vulkan.h"

namespace virasa
{

Surface::~Surface() noexcept
{
	if (_surface != VK_NULL_HANDLE)
	{
		vkDestroySurfaceKHR(_instance, _surface, nullptr);
		_surface = VK_NULL_HANDLE;
	}
}

Surface::Surface(Surface&& other) noexcept
    : _instance(other._instance), _surface(other._surface), _format(other._format),
	_presentMode(other._presentMode), _capabilities(other._capabilities),
	_formatsQueried(other._formatsQueried)
{
	other._instance = VK_NULL_HANDLE;
	other._surface = VK_NULL_HANDLE;
	other._formatsQueried = false;
}

Surface& Surface::operator=(Surface&& other) noexcept
{
	if (this != &other)
	{
		if (_surface != VK_NULL_HANDLE)
		{
			vkDestroySurfaceKHR(_instance, _surface, nullptr);
		}

		_instance = other._instance;
		_surface = other._surface;
		_format = other._format;
		_presentMode = other._presentMode;
		_capabilities = other._capabilities;
		_formatsQueried = other._formatsQueried;

		other._instance = VK_NULL_HANDLE;
		other._surface = VK_NULL_HANDLE;
		other._formatsQueried = false;
	}
	return *this;
}

RenderError Surface::Initialize(const Instance& instance, virasa::window::Platform& platform)
{
	auto* logger = virasa::Logger::GetLogger("renderer");

	VkInstance vkInstance = instance.GetHandle();
	VkSurfaceKHR outSurface = VK_NULL_HANDLE;

	virasa::ErrorCode result = platform.CreateSurface(vkInstance, &outSurface);
	if (result != virasa::ErrorCode::None)
	{
		LOG_ERROR(logger, "Surface::Initialize: platform.CreateSurface failed");
		return RenderError::SurfaceCreateFailed;
	}

	_instance = vkInstance;
	_surface = outSurface;
	_formatsQueried = false;

	LOG_INFO(logger, "Surface::Initialize: VkSurfaceKHR created successfully");
	return RenderError::None;
}

void Surface::QueryFormatsAndModes(VkPhysicalDevice physical_device, const RendererConfig& config)
{
	// Query capabilities
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, _surface, &_capabilities);

	// Query formats
	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, _surface, &formatCount, nullptr);
	std::vector<VkSurfaceFormatKHR> formats(formatCount);
	if (formatCount > 0)
	{
		vkGetPhysicalDeviceSurfaceFormatsKHR(
			physical_device, _surface, &formatCount, formats.data());
	}

	// Select format: first priority B8G8R8A8_SRGB + SRGB_NONLINEAR
	VkSurfaceFormatKHR selectedFormat = formats.empty() ? VkSurfaceFormatKHR{} : formats[0];
	bool found = false;

	for (const auto& fmt : formats)
	{
		if (fmt.format == VK_FORMAT_B8G8R8A8_SRGB &&
			fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			selectedFormat = fmt;
			found = true;
			break;
		}
	}

	if (!found)
	{
		// Second priority: SRGB_NONLINEAR with one of the acceptable SRGB formats
		const VkFormat acceptableSrgbFormats[] = {VK_FORMAT_R8G8B8A8_SRGB,
			VK_FORMAT_B8G8R8A8_SRGB,
			VK_FORMAT_A8B8G8R8_SRGB_PACK32,
			VK_FORMAT_R8G8B8_SRGB,
			VK_FORMAT_B8G8R8_SRGB};

		for (const auto& fmt : formats)
		{
			if (fmt.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				continue;
			}
			for (VkFormat acceptable : acceptableSrgbFormats)
			{
				if (fmt.format == acceptable)
				{
					selectedFormat = fmt;
					found = true;
					break;
				}
			}
			if (found)
			{
				break;
			}
		}
	}

	_format = selectedFormat;

	// Query present modes
	uint32_t presentModeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(
		physical_device, _surface, &presentModeCount, nullptr);
	std::vector<VkPresentModeKHR> presentModes(presentModeCount);
	if (presentModeCount > 0)
	{
		vkGetPhysicalDeviceSurfacePresentModesKHR(
			physical_device, _surface, &presentModeCount, presentModes.data());
	}

	// Select present mode
	_presentMode = VK_PRESENT_MODE_FIFO_KHR;
	if (config.preferMailbox)
	{
		for (const auto& mode : presentModes)
		{
			if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				_presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
				break;
			}
		}
	}

	_formatsQueried = true;
}

void Surface::RefreshCapabilities(VkPhysicalDevice physical_device)
{
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, _surface, &_capabilities);
}

VkSurfaceKHR Surface::GetHandle() const noexcept
{
	return _surface;
}

VkSurfaceFormatKHR Surface::GetFormat() const noexcept
{
	return _format;
}

VkPresentModeKHR Surface::GetPresentMode() const noexcept
{
	return _presentMode;
}

const VkSurfaceCapabilitiesKHR& Surface::GetCapabilities() const noexcept
{
	return _capabilities;
}

bool Surface::IsInitialized() const noexcept
{
	return _surface != VK_NULL_HANDLE;
}

bool Surface::AreFormatsQueried() const noexcept
{
	return _formatsQueried;
}

} // namespace virasa
