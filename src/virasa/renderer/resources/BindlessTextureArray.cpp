#include "virasa/renderer/resources/BindlessTextureArray.h"

#include <cstdint>
#include <vector>

#include "quill/LogMacros.h"
#include "virasa/core/Logger.h"
#include "vulkan/vulkan.h"

namespace virasa
{

BindlessTextureArray::~BindlessTextureArray()
{
	Teardown();
}

BindlessTextureArray::BindlessTextureArray(BindlessTextureArray&& other) noexcept
    : _device(other._device), _pool(other._pool), _layout(other._layout),
	_descriptorSet(other._descriptorSet), _maxTextures(other._maxTextures),
	_freeSlots(std::move(other._freeSlots))
{
	other._device = VK_NULL_HANDLE;
	other._pool = VK_NULL_HANDLE;
	other._layout = VK_NULL_HANDLE;
	other._descriptorSet = VK_NULL_HANDLE;
	other._maxTextures = 0;
}

BindlessTextureArray& BindlessTextureArray::operator=(BindlessTextureArray&& other) noexcept
{
	if (this != &other)
	{
		Teardown();

		_device = other._device;
		_pool = other._pool;
		_layout = other._layout;
		_descriptorSet = other._descriptorSet;
		_maxTextures = other._maxTextures;
		_freeSlots = std::move(other._freeSlots);

		other._device = VK_NULL_HANDLE;
		other._pool = VK_NULL_HANDLE;
		other._layout = VK_NULL_HANDLE;
		other._descriptorSet = VK_NULL_HANDLE;
		other._maxTextures = 0;
	}
	return *this;
}

void BindlessTextureArray::Teardown()
{
	if (_device == VK_NULL_HANDLE)
		return;

	if (_layout != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorSetLayout(_device, _layout, nullptr);
		_layout = VK_NULL_HANDLE;
	}

	if (_pool != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(_device, _pool, nullptr);
		_pool = VK_NULL_HANDLE;
	}

	_descriptorSet = VK_NULL_HANDLE;
	_device = VK_NULL_HANDLE;
	_maxTextures = 0;
	_freeSlots.clear();
}

RenderError BindlessTextureArray::Initialize(const Device& device, uint32_t max_textures)
{
	// Tear down any prior state to allow re-initialization without leaking resources.
	Teardown();

	auto* log = Logger::GetLogger("renderer");
	VkDevice vkDevice = device.GetHandle();

	// Step 1: Create descriptor pool.
	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSize.descriptorCount = max_textures;

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
	poolInfo.maxSets = 1;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;

	VkDescriptorPool pool = VK_NULL_HANDLE;
	VkResult result = vkCreateDescriptorPool(vkDevice, &poolInfo, nullptr, &pool);
	if (result != VK_SUCCESS)
	{
		LOG_ERROR(log,
			"BindlessTextureArray: vkCreateDescriptorPool failed (VkResult {})",
			static_cast<int>(result));
		return RenderError::DescriptorPoolCreateFailed;
	}

	// Step 2: Create descriptor set layout.
	VkDescriptorSetLayoutBinding binding{};
	binding.binding = 0;
	binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding.descriptorCount = max_textures;
	binding.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
	binding.pImmutableSamplers = nullptr;

	VkDescriptorBindingFlags bindingFlags =
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

	VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
	bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	bindingFlagsInfo.bindingCount = 1;
	bindingFlagsInfo.pBindingFlags = &bindingFlags;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.pNext = &bindingFlagsInfo;
	layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &binding;

	VkDescriptorSetLayout layout = VK_NULL_HANDLE;
	result = vkCreateDescriptorSetLayout(vkDevice, &layoutInfo, nullptr, &layout);
	if (result != VK_SUCCESS)
	{
		LOG_ERROR(log,
			"BindlessTextureArray: vkCreateDescriptorSetLayout failed (VkResult {})",
			static_cast<int>(result));
		vkDestroyDescriptorPool(vkDevice, pool, nullptr);
		return RenderError::DescriptorPoolCreateFailed;
	}

	// Step 3: Allocate descriptor set.
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = pool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &layout;

	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	result = vkAllocateDescriptorSets(vkDevice, &allocInfo, &descriptorSet);
	if (result != VK_SUCCESS)
	{
		LOG_ERROR(log,
			"BindlessTextureArray: vkAllocateDescriptorSets failed (VkResult {})",
			static_cast<int>(result));
		vkDestroyDescriptorSetLayout(vkDevice, layout, nullptr);
		vkDestroyDescriptorPool(vkDevice, pool, nullptr);
		return RenderError::DescriptorPoolCreateFailed;
	}

	// Step 4: Populate free-slot list with [0, max_textures) in ascending order.
	// We use a stack (pop from back), so push in ascending order so that slot 0
	// is at the back and will be claimed first.
	_freeSlots.clear();
	_freeSlots.reserve(max_textures);
	for (uint32_t i = max_textures; i > 0; --i)
	{
		_freeSlots.push_back(i - 1);
	}

	_device = vkDevice;
	_pool = pool;
	_layout = layout;
	_descriptorSet = descriptorSet;
	_maxTextures = max_textures;

	return RenderError::None;
}

uint32_t BindlessTextureArray::RegisterTexture(VkImageView image_view, VkSampler sampler)
{
	if (_freeSlots.empty())
	{
		auto* log = Logger::GetLogger("renderer");
		LOG_ERROR(log,
			"BindlessTextureArray::RegisterTexture: no free slots available "
			"(max_textures={})",
			_maxTextures);
		return UINT32_MAX;
	}

	uint32_t slot = _freeSlots.back();
	_freeSlots.pop_back();

	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = image_view;
	imageInfo.sampler = sampler;

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = _descriptorSet;
	write.dstBinding = 0;
	write.dstArrayElement = slot;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(_device, 1, &write, 0, nullptr);

	return slot;
}

void BindlessTextureArray::UnregisterTexture(uint32_t slot)
{
	_freeSlots.push_back(slot);
}

VkDescriptorSet BindlessTextureArray::GetDescriptorSet() const noexcept
{
	return _descriptorSet;
}

VkDescriptorSetLayout BindlessTextureArray::GetLayout() const noexcept
{
	return _layout;
}

uint32_t BindlessTextureArray::GetMaxTextures() const noexcept
{
	return _maxTextures;
}

bool BindlessTextureArray::IsInitialized() const noexcept
{
	return _descriptorSet != VK_NULL_HANDLE;
}

} // namespace virasa
