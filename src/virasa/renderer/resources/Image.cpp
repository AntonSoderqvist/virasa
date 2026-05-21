#include "virasa/renderer/resources/Image.h"

#include <cstdint>
#include <limits>

#include "quill/LogMacros.h"
#include "virasa/core/Logger.h"

namespace virasa
{

// ---------------------------------------------------------------------------
// Destructor / move
// ---------------------------------------------------------------------------

Image::~Image()
{
	Teardown();
}

Image::Image(Image&& other) noexcept
    : _device(other._device), _image(other._image), _memory(other._memory), _view(other._view),
	_config(other._config), _memProps(other._memProps)
{
	other._device = VK_NULL_HANDLE;
	other._image = VK_NULL_HANDLE;
	other._memory = VK_NULL_HANDLE;
	other._view = VK_NULL_HANDLE;
	other._config = {};
	other._memProps = {};
}

Image& Image::operator=(Image&& other) noexcept
{
	if (this != &other)
	{
		Teardown();

		_device = other._device;
		_image = other._image;
		_memory = other._memory;
		_view = other._view;
		_config = other._config;
		_memProps = other._memProps;

		other._device = VK_NULL_HANDLE;
		other._image = VK_NULL_HANDLE;
		other._memory = VK_NULL_HANDLE;
		other._view = VK_NULL_HANDLE;
		other._config = {};
		other._memProps = {};
	}
	return *this;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void Image::Teardown()
{
	if (_device == VK_NULL_HANDLE)
		return;

	if (_view != VK_NULL_HANDLE)
	{
		vkDestroyImageView(_device, _view, nullptr);
		_view = VK_NULL_HANDLE;
	}

	if (_image != VK_NULL_HANDLE)
	{
		vkDestroyImage(_device, _image, nullptr);
		_image = VK_NULL_HANDLE;
	}

	if (_memory != VK_NULL_HANDLE)
	{
		vkFreeMemory(_device, _memory, nullptr);
		_memory = VK_NULL_HANDLE;
	}
}

uint32_t Image::SelectMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags required) const
{
	for (uint32_t i = 0; i < _memProps.memoryTypeCount; ++i)
	{
		if ((typeFilter & (1u << i)) == 0)
			continue;

		if ((_memProps.memoryTypes[i].propertyFlags & required) == required)
			return i;
	}
	return std::numeric_limits<uint32_t>::max();
}

RenderError Image::CreateResources()
{
	auto* logger = Logger::GetLogger("renderer");

	// Step 1: create image
	{
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent = {_config.width, _config.height, 1};
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = _config.format;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = _config.usage;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.flags = _config.flags;

		VkResult result = vkCreateImage(_device, &imageInfo, nullptr, &_image);
		if (result != VK_SUCCESS)
		{
			LOG_ERROR(
				logger, "vkCreateImage failed with VkResult {}", static_cast<int>(result));
			Teardown();
			return RenderError::ImageCreateFailed;
		}
	}

	// Step 2: allocate memory
	{
		VkMemoryRequirements memReqs{};
		vkGetImageMemoryRequirements(_device, _image, &memReqs);

		uint32_t memTypeIndex =
			SelectMemoryType(memReqs.memoryTypeBits, _config.memoryProperties);
		if (memTypeIndex == std::numeric_limits<uint32_t>::max())
		{
			LOG_ERROR(logger, "No suitable memory type found for image allocation");
			Teardown();
			return RenderError::MemoryAllocFailed;
		}

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memReqs.size;
		allocInfo.memoryTypeIndex = memTypeIndex;

		VkResult result = vkAllocateMemory(_device, &allocInfo, nullptr, &_memory);
		if (result != VK_SUCCESS)
		{
			LOG_ERROR(logger,
				"vkAllocateMemory failed with VkResult {}",
				static_cast<int>(result));
			Teardown();
			return RenderError::MemoryAllocFailed;
		}
	}

	// Step 3: bind memory
	{
		VkResult result = vkBindImageMemory(_device, _image, _memory, 0);
		if (result != VK_SUCCESS)
		{
			LOG_ERROR(logger,
				"vkBindImageMemory failed with VkResult {}",
				static_cast<int>(result));
			Teardown();
			return RenderError::ImageCreateFailed;
		}
	}

	// Step 4: create image view
	{
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = _image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = _config.format;
		viewInfo.subresourceRange.aspectMask = _config.aspect;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		VkResult result = vkCreateImageView(_device, &viewInfo, nullptr, &_view);
		if (result != VK_SUCCESS)
		{
			LOG_ERROR(logger,
				"vkCreateImageView failed with VkResult {}",
				static_cast<int>(result));
			Teardown();
			return RenderError::ImageViewCreateFailed;
		}
	}

	return RenderError::None;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

RenderError Image::Initialize(const Device& device, const ImageConfig& config)
{
	// Teardown any existing resources (re-initialization path)
	Teardown();

	_device = device.GetHandle();
	_config = config;
	_memProps = device.GetMemoryProperties();

	return CreateResources();
}

RenderError Image::Resize(uint32_t new_width, uint32_t new_height)
{
	if (_device == VK_NULL_HANDLE)
		return RenderError::NotInitialized;

	_config.width = new_width;
	_config.height = new_height;

	Teardown();

	return CreateResources();
}

VkImage Image::GetHandle() const noexcept
{
	return _image;
}

VkImageView Image::GetView() const noexcept
{
	return _view;
}

VkFormat Image::GetFormat() const noexcept
{
	return _config.format;
}

uint32_t Image::GetWidth() const noexcept
{
	return _config.width;
}

uint32_t Image::GetHeight() const noexcept
{
	return _config.height;
}

VkExtent2D Image::GetExtent() const noexcept
{
	return {_config.width, _config.height};
}

bool Image::IsInitialized() const noexcept
{
	return _image != VK_NULL_HANDLE;
}

} // namespace virasa
