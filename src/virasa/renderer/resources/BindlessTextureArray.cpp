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
	_freeSlots(std::move(other._freeSlots)),
	_shadowFreeSlots(std::move(other._shadowFreeSlots)),
	_storageFreeSlots(std::move(other._storageFreeSlots))
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
		_shadowFreeSlots = std::move(other._shadowFreeSlots);
		_storageFreeSlots = std::move(other._storageFreeSlots);

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
	_shadowFreeSlots.clear();
	_storageFreeSlots.clear();
}

RenderError BindlessTextureArray::Initialize(const Device& device, uint32_t max_textures)
{
	// Tear down any prior state to allow re-initialization without leaking resources.
	Teardown();

	auto* log = Logger::GetLogger("renderer");
	VkDevice vkDevice = device.GetHandle();

	// Step 1: Create descriptor pool.
	VkDescriptorPoolSize poolSizes[2]{};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[0].descriptorCount = max_textures + kMaxShadowMaps;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	poolSizes[1].descriptorCount = kMaxStorageImages;

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
	poolInfo.maxSets = 1;
	poolInfo.poolSizeCount = 2;
	poolInfo.pPoolSizes = poolSizes;

	VkDescriptorPool pool = VK_NULL_HANDLE;
	VkResult result = vkCreateDescriptorPool(vkDevice, &poolInfo, nullptr, &pool);
	if (result != VK_SUCCESS)
	{
		LOG_ERROR(log,
			"BindlessTextureArray: vkCreateDescriptorPool failed (VkResult {})",
			static_cast<int>(result));
		return RenderError::DescriptorPoolCreateFailed;
	}

	// Step 2: Create descriptor set layout with three bindings.
	VkDescriptorSetLayoutBinding bindings[3]{};
	// Binding 0: texture array
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[0].descriptorCount = max_textures;
	bindings[0].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
	bindings[0].pImmutableSamplers = nullptr;
	// Binding 1: shadow-map array
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[1].descriptorCount = kMaxShadowMaps;
	bindings[1].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
	bindings[1].pImmutableSamplers = nullptr;
	// Binding 2: storage-image array
	bindings[2].binding = 2;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[2].descriptorCount = kMaxStorageImages;
	bindings[2].stageFlags = VK_SHADER_STAGE_ALL;
	bindings[2].pImmutableSamplers = nullptr;

	VkDescriptorBindingFlags bindingFlagsArr[3] = {
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
	};

	VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
	bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	bindingFlagsInfo.bindingCount = 3;
	bindingFlagsInfo.pBindingFlags = bindingFlagsArr;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.pNext = &bindingFlagsInfo;
	layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
	layoutInfo.bindingCount = 3;
	layoutInfo.pBindings = bindings;

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

	// Step 4: Populate free-slot lists.
	// Both lists are maintained as stacks (pop from back).
	// Push in descending order so that slot 0 is at the back and claimed first.
	_freeSlots.clear();
	_freeSlots.reserve(max_textures);
	for (uint32_t i = max_textures; i > 0; --i)
	{
		_freeSlots.push_back(i - 1);
	}

	_shadowFreeSlots.clear();
	_shadowFreeSlots.reserve(kMaxShadowMaps);
	for (uint32_t i = kMaxShadowMaps; i > 0; --i)
	{
		_shadowFreeSlots.push_back(i - 1);
	}

	_storageFreeSlots.clear();
	_storageFreeSlots.reserve(kMaxStorageImages);
	for (uint32_t i = kMaxStorageImages; i > 0; --i)
	{
		_storageFreeSlots.push_back(i - 1);
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

uint32_t BindlessTextureArray::RegisterShadowMap(VkImageView image_view, VkSampler sampler)
{
	if (_shadowFreeSlots.empty())
	{
		auto* log = Logger::GetLogger("renderer");
		LOG_ERROR(log,
			"BindlessTextureArray::RegisterShadowMap: no free shadow-map slots available "
			"(kMaxShadowMaps={})",
			kMaxShadowMaps);
		return UINT32_MAX;
	}

	uint32_t slot = _shadowFreeSlots.back();
	_shadowFreeSlots.pop_back();

	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = image_view;
	imageInfo.sampler = sampler;

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = _descriptorSet;
	write.dstBinding = 1;
	write.dstArrayElement = slot;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(_device, 1, &write, 0, nullptr);

	return slot;
}

void BindlessTextureArray::UnregisterShadowMap(uint32_t slot)
{
	_shadowFreeSlots.push_back(slot);
}

uint32_t BindlessTextureArray::RegisterStorageImage(VkImageView image_view)
{
	if (_storageFreeSlots.empty())
	{
		auto* log = Logger::GetLogger("renderer");
		LOG_ERROR(log,
			"BindlessTextureArray::RegisterStorageImage: no free storage-image slots available "
			"(kMaxStorageImages={})",
			kMaxStorageImages);
		return UINT32_MAX;
	}

	uint32_t slot = _storageFreeSlots.back();
	_storageFreeSlots.pop_back();

	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageInfo.imageView = image_view;
	imageInfo.sampler = VK_NULL_HANDLE;

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = _descriptorSet;
	write.dstBinding = 2;
	write.dstArrayElement = slot;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	write.pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(_device, 1, &write, 0, nullptr);

	return slot;
}

void BindlessTextureArray::UnregisterStorageImage(uint32_t slot)
{
	_storageFreeSlots.push_back(slot);
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

uint32_t BindlessTextureArray::GetMaxShadowMaps() const noexcept
{
	if (_descriptorSet == VK_NULL_HANDLE)
		return 0;
	return kMaxShadowMaps;
}

uint32_t BindlessTextureArray::GetMaxStorageImages() const noexcept
{
	if (_descriptorSet == VK_NULL_HANDLE)
		return 0;
	return kMaxStorageImages;
}

bool BindlessTextureArray::IsInitialized() const noexcept
{
	return _descriptorSet != VK_NULL_HANDLE;
}

} // namespace virasa
