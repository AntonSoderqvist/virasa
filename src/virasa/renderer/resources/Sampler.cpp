#include "virasa/renderer/resources/Sampler.h"

#include "quill/LogMacros.h"
#include "virasa/core/Logger.h"
#include "vulkan/vulkan.h"

namespace virasa
{

// ---------------------------------------------------------------------------
// Destructor
// ---------------------------------------------------------------------------

Sampler::~Sampler() noexcept
{
	Teardown();
}

// ---------------------------------------------------------------------------
// Move semantics
// ---------------------------------------------------------------------------

Sampler::Sampler(Sampler&& other) noexcept : _sampler(other._sampler), _device(other._device)
{
	other._sampler = VK_NULL_HANDLE;
	other._device = VK_NULL_HANDLE;
}

Sampler& Sampler::operator=(Sampler&& other) noexcept
{
	if (this != &other)
	{
		Teardown();
		_sampler = other._sampler;
		_device = other._device;
		other._sampler = VK_NULL_HANDLE;
		other._device = VK_NULL_HANDLE;
	}
	return *this;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

RenderError Sampler::Initialize(const Device& device, const SamplerConfig& config)
{
	// Destroy any previously owned handle before creating a new one.
	Teardown();

	VkDevice vkDevice = device.GetHandle();

	VkSamplerCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;
	createInfo.magFilter = config.magFilter;
	createInfo.minFilter = config.minFilter;
	createInfo.mipmapMode = config.mipmapMode;
	createInfo.addressModeU = config.addressModeU;
	createInfo.addressModeV = config.addressModeV;
	createInfo.addressModeW = config.addressModeW;
	createInfo.mipLodBias = config.mipLodBias;
	createInfo.anisotropyEnable = config.anisotropyEnable ? VK_TRUE : VK_FALSE;
	createInfo.maxAnisotropy = config.maxAnisotropy;
	createInfo.compareEnable = config.compareEnable ? VK_TRUE : VK_FALSE;
	createInfo.compareOp = config.compareOp;
	createInfo.minLod = config.minLod;
	createInfo.maxLod = config.maxLod;
	createInfo.borderColor = config.borderColor;
	createInfo.unnormalizedCoordinates = VK_FALSE;

	VkSampler sampler = VK_NULL_HANDLE;
	VkResult result = vkCreateSampler(vkDevice, &createInfo, nullptr, &sampler);
	if (result != VK_SUCCESS)
	{
		auto* logger = Logger::GetLogger("renderer");
		LOG_ERROR(logger, "vkCreateSampler failed with VkResult {}", static_cast<int>(result));
		// Leave _device as VK_NULL_HANDLE (already cleared by Teardown).
		return RenderError::SamplerCreateFailed;
	}

	_sampler = sampler;
	_device = vkDevice;
	return RenderError::None;
}

VkSampler Sampler::GetHandle() const noexcept
{
	return _sampler;
}

bool Sampler::IsInitialized() const noexcept
{
	return _sampler != VK_NULL_HANDLE;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void Sampler::Teardown() noexcept
{
	if (_device == VK_NULL_HANDLE)
	{
		return;
	}
	if (_sampler != VK_NULL_HANDLE)
	{
		vkDestroySampler(_device, _sampler, nullptr);
		_sampler = VK_NULL_HANDLE;
	}
	_device = VK_NULL_HANDLE;
}

} // namespace virasa
