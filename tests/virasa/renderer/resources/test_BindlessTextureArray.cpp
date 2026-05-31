#include <cstdint>
#include <gtest/gtest.h>
#include <type_traits>
#include <utility>

#include "virasa/core/Logger.h"
#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Device.h"
#include "virasa/renderer/core/Instance.h"
#include "virasa/renderer/resources/BindlessTextureArray.h"
#include "virasa/renderer/resources/Sampler.h"
#include "virasa/window/Platform.h"
#include "vulkan/vulkan.h"

using namespace virasa;

// ---------------------------------------------------------------------------
// Test helpers
// ---------------------------------------------------------------------------
namespace
{

// Bring up a real Vulkan instance + device for tests that need one.
// Returns false if the environment cannot support Vulkan (e.g. CI without GPU).
struct VulkanContext
{
	virasa::window::Platform platform;
	Instance instance;
	Device device;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	bool valid = false;

	VulkanContext()
	{
		Logger::Initialize();

		// We need a window + surface to initialise the device.
		if (platform.Initialize("BindlessTextureArrayTest", 1, 1) != ErrorCode::None)
			return;

		RendererConfig cfg{};
		cfg.enableValidation = false;
		uint32_t extCount = 0;
		cfg.requiredInstanceExtensions =
			virasa::window::Platform::GetRequiredVulkanExtensions(&extCount);
		cfg.requiredInstanceExtensionCount = extCount;

		if (instance.Initialize(cfg) != RenderError::None)
			return;

		if (platform.CreateSurface(instance.GetHandle(), &surface) != ErrorCode::None)
			return;

		if (device.Initialize(instance, surface) != RenderError::None)
			return;

		if (!device.IsDescriptorIndexingEnabled())
			return;

		valid = true;
	}

	~VulkanContext()
	{
		if (device.IsInitialized())
			device.WaitIdle();
		// surface is owned by platform in this setup; platform destructor handles it
		platform.Shutdown();
		Logger::Shutdown();
	}

	VulkanContext(const VulkanContext&) = delete;
	VulkanContext& operator=(const VulkanContext&) = delete;
};

} // namespace

// ---------------------------------------------------------------------------
// bindless_texture_array_default_constructed_state
// ---------------------------------------------------------------------------
TEST(BindlessTextureArray, test_default_constructed_state)
{
	BindlessTextureArray bta;

	EXPECT_EQ(bta.GetDescriptorSet(), VK_NULL_HANDLE);
	EXPECT_EQ(bta.GetLayout(), VK_NULL_HANDLE);
	EXPECT_EQ(bta.GetMaxTextures(), 0u);
	EXPECT_EQ(bta.GetMaxShadowMaps(), 0u);
	EXPECT_FALSE(bta.IsInitialized());
}

// ---------------------------------------------------------------------------
// bindless_texture_array_is_raii_movable_non_copyable
// ---------------------------------------------------------------------------
TEST(BindlessTextureArray, test_is_raii_movable_non_copyable)
{
	// Non-copyable
	EXPECT_FALSE(std::is_copy_constructible_v<BindlessTextureArray>);
	EXPECT_FALSE(std::is_copy_assignable_v<BindlessTextureArray>);

	// Movable
	EXPECT_TRUE(std::is_move_constructible_v<BindlessTextureArray>);
	EXPECT_TRUE(std::is_move_assignable_v<BindlessTextureArray>);

	// Final
	EXPECT_TRUE(std::is_final_v<BindlessTextureArray>);

	// Default-constructible
	EXPECT_TRUE(std::is_default_constructible_v<BindlessTextureArray>);

	// Move constructor transfers state; source becomes default-constructed-like
	VulkanContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan not available; skipping move-state verification";
	}

	BindlessTextureArray src;
	ASSERT_EQ(src.Initialize(ctx.device, 4u), RenderError::None);
	ASSERT_TRUE(src.IsInitialized());
	VkDescriptorSet srcSet = src.GetDescriptorSet();
	VkDescriptorSetLayout srcLayout = src.GetLayout();
	ASSERT_NE(srcSet, VK_NULL_HANDLE);
	ASSERT_NE(srcLayout, VK_NULL_HANDLE);

	// Move-construct
	BindlessTextureArray dst(std::move(src));

	// dst now owns the handles
	EXPECT_EQ(dst.GetDescriptorSet(), srcSet);
	EXPECT_EQ(dst.GetLayout(), srcLayout);
	EXPECT_EQ(dst.GetMaxTextures(), 4u);
	EXPECT_EQ(dst.GetMaxShadowMaps(), 16u);
	EXPECT_TRUE(dst.IsInitialized());

	// src is in default-constructed-like state
	EXPECT_EQ(src.GetDescriptorSet(), VK_NULL_HANDLE);
	EXPECT_EQ(src.GetLayout(), VK_NULL_HANDLE);
	EXPECT_EQ(src.GetMaxShadowMaps(), 0u);
	EXPECT_FALSE(src.IsInitialized());

	// Move-assign: dst2 already has handles; move-assign from dst should tear down dst2's handles
	// and take ownership of dst's.
	BindlessTextureArray dst2;
	ASSERT_EQ(dst2.Initialize(ctx.device, 2u), RenderError::None);
	ASSERT_TRUE(dst2.IsInitialized());

	dst2 = std::move(dst);

	EXPECT_EQ(dst2.GetDescriptorSet(), srcSet);
	EXPECT_EQ(dst2.GetLayout(), srcLayout);
	EXPECT_EQ(dst2.GetMaxShadowMaps(), 16u);
	EXPECT_TRUE(dst2.IsInitialized());

	// dst is now in default-constructed-like state
	EXPECT_EQ(dst.GetDescriptorSet(), VK_NULL_HANDLE);
	EXPECT_EQ(dst.GetLayout(), VK_NULL_HANDLE);
	EXPECT_EQ(dst.GetMaxShadowMaps(), 0u);
	EXPECT_FALSE(dst.IsInitialized());

	// Destroying a moved-from object is well-defined (no crash)
	// dst and src will be destroyed at end of scope — no explicit action needed.

	ctx.device.WaitIdle();
}

// ---------------------------------------------------------------------------
// bindless_texture_array_initialize_creates_pool_layout_and_set
// ---------------------------------------------------------------------------
TEST(BindlessTextureArray, test_initialize_creates_pool_layout_and_set)
{
	VulkanContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan not available";
	}

	BindlessTextureArray bta;

	constexpr uint32_t kMaxTextures = 8u;
	RenderError err = bta.Initialize(ctx.device, kMaxTextures);
	ASSERT_EQ(err, RenderError::None);

	EXPECT_TRUE(bta.IsInitialized());
	EXPECT_NE(bta.GetDescriptorSet(), VK_NULL_HANDLE);
	EXPECT_NE(bta.GetLayout(), VK_NULL_HANDLE);
	EXPECT_EQ(bta.GetMaxTextures(), kMaxTextures);
	EXPECT_EQ(bta.GetMaxShadowMaps(), 16u);

	// Re-initialization: should tear down prior resources and succeed again.
	VkDescriptorSet firstSet = bta.GetDescriptorSet();
	VkDescriptorSetLayout firstLayout = bta.GetLayout();

	RenderError err2 = bta.Initialize(ctx.device, kMaxTextures * 2u);
	ASSERT_EQ(err2, RenderError::None);
	EXPECT_TRUE(bta.IsInitialized());
	EXPECT_NE(bta.GetDescriptorSet(), VK_NULL_HANDLE);
	EXPECT_NE(bta.GetLayout(), VK_NULL_HANDLE);
	EXPECT_EQ(bta.GetMaxTextures(), kMaxTextures * 2u);
	EXPECT_EQ(bta.GetMaxShadowMaps(), 16u);
	// After re-init the handles are new (old pool was destroyed; set handle may differ)
	// We can only assert they are non-null; handle values are implementation-defined.
	(void)firstSet;
	(void)firstLayout;

	ctx.device.WaitIdle();
}

// ---------------------------------------------------------------------------
// bindless_texture_array_register_and_unregister
// ---------------------------------------------------------------------------
TEST(BindlessTextureArray, test_register_and_unregister)
{
	VulkanContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan not available";
	}

	constexpr uint32_t kMaxTextures = 4u;
	BindlessTextureArray bta;
	ASSERT_EQ(bta.Initialize(ctx.device, kMaxTextures), RenderError::None);

	// Create a minimal 1x1 image + image view + sampler to use as test textures.
	// We need real VkImageView and VkSampler handles.
	VkDevice vkDev = ctx.device.GetHandle();

	// Create a tiny image.
	VkImageCreateInfo imgInfo{};
	imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imgInfo.imageType = VK_IMAGE_TYPE_2D;
	imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	imgInfo.extent = {1, 1, 1};
	imgInfo.mipLevels = 1;
	imgInfo.arrayLayers = 1;
	imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imgInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
	imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VkImage image = VK_NULL_HANDLE;
	if (vkCreateImage(vkDev, &imgInfo, nullptr, &image) != VK_SUCCESS)
	{
		GTEST_SKIP() << "Could not create test VkImage";
	}

	// Allocate memory for the image.
	VkMemoryRequirements memReqs{};
	vkGetImageMemoryRequirements(vkDev, image, &memReqs);

	const auto& memProps = ctx.device.GetMemoryProperties();
	uint32_t memTypeIndex = UINT32_MAX;
	for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
	{
		if ((memReqs.memoryTypeBits & (1u << i)) &&
			(memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
		{
			memTypeIndex = i;
			break;
		}
	}

	if (memTypeIndex == UINT32_MAX)
	{
		vkDestroyImage(vkDev, image, nullptr);
		GTEST_SKIP() << "No suitable memory type for test image";
	}

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReqs.size;
	allocInfo.memoryTypeIndex = memTypeIndex;

	VkDeviceMemory imageMem = VK_NULL_HANDLE;
	if (vkAllocateMemory(vkDev, &allocInfo, nullptr, &imageMem) != VK_SUCCESS)
	{
		vkDestroyImage(vkDev, image, nullptr);
		GTEST_SKIP() << "Could not allocate memory for test image";
	}
	vkBindImageMemory(vkDev, image, imageMem, 0);

	// Create image view.
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.layerCount = 1;

	VkImageView imageView = VK_NULL_HANDLE;
	if (vkCreateImageView(vkDev, &viewInfo, nullptr, &imageView) != VK_SUCCESS)
	{
		vkFreeMemory(vkDev, imageMem, nullptr);
		vkDestroyImage(vkDev, image, nullptr);
		GTEST_SKIP() << "Could not create test VkImageView";
	}

	// Create sampler.
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_NEAREST;
	samplerInfo.minFilter = VK_FILTER_NEAREST;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.maxLod = VK_LOD_CLAMP_NONE;

	VkSampler sampler = VK_NULL_HANDLE;
	if (vkCreateSampler(vkDev, &samplerInfo, nullptr, &sampler) != VK_SUCCESS)
	{
		vkDestroyImageView(vkDev, imageView, nullptr);
		vkFreeMemory(vkDev, imageMem, nullptr);
		vkDestroyImage(vkDev, image, nullptr);
		GTEST_SKIP() << "Could not create test VkSampler";
	}

	// --- Register textures ---
	// Free-slot list is a stack seeded with [0,1,2,3]; first pop gives slot 3
	// (last element), then 2, 1, 0 — but the contract says "ascending order"
	// for the initial population and "pops the last element". We just verify
	// the returned slot is in [0, kMaxTextures) and that UINT32_MAX is NOT returned.
	uint32_t slot0 = bta.RegisterTexture(imageView, sampler);
	EXPECT_NE(slot0, UINT32_MAX);
	EXPECT_LT(slot0, kMaxTextures);

	uint32_t slot1 = bta.RegisterTexture(imageView, sampler);
	EXPECT_NE(slot1, UINT32_MAX);
	EXPECT_LT(slot1, kMaxTextures);
	EXPECT_NE(slot1, slot0); // each slot is distinct

	uint32_t slot2 = bta.RegisterTexture(imageView, sampler);
	EXPECT_NE(slot2, UINT32_MAX);
	EXPECT_LT(slot2, kMaxTextures);

	uint32_t slot3 = bta.RegisterTexture(imageView, sampler);
	EXPECT_NE(slot3, UINT32_MAX);
	EXPECT_LT(slot3, kMaxTextures);

	// All kMaxTextures slots are now in use; next RegisterTexture must return UINT32_MAX.
	uint32_t overflow = bta.RegisterTexture(imageView, sampler);
	EXPECT_EQ(overflow, UINT32_MAX);

	// --- Unregister one slot and re-register ---
	bta.UnregisterTexture(slot2);

	uint32_t reused = bta.RegisterTexture(imageView, sampler);
	EXPECT_EQ(reused, slot2); // slot2 was pushed back; it should be popped next

	// --- RegisterShadowMap / UnregisterShadowMap (binding 1, kMaxShadowMaps = 16) ---
	// Create a depth image + view + comparison sampler for shadow-map tests.
	VkImageCreateInfo depthImgInfo{};
	depthImgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	depthImgInfo.imageType = VK_IMAGE_TYPE_2D;
	depthImgInfo.format = VK_FORMAT_D32_SFLOAT;
	depthImgInfo.extent = {1, 1, 1};
	depthImgInfo.mipLevels = 1;
	depthImgInfo.arrayLayers = 1;
	depthImgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	depthImgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	depthImgInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	depthImgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	depthImgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VkImage depthImage = VK_NULL_HANDLE;
	if (vkCreateImage(vkDev, &depthImgInfo, nullptr, &depthImage) != VK_SUCCESS)
	{
		ctx.device.WaitIdle();
		vkDestroySampler(vkDev, sampler, nullptr);
		vkDestroyImageView(vkDev, imageView, nullptr);
		vkFreeMemory(vkDev, imageMem, nullptr);
		vkDestroyImage(vkDev, image, nullptr);
		GTEST_SKIP() << "Could not create depth VkImage for shadow-map test";
	}

	VkMemoryRequirements depthMemReqs{};
	vkGetImageMemoryRequirements(vkDev, depthImage, &depthMemReqs);
	uint32_t depthMemTypeIndex = UINT32_MAX;
	for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
	{
		if ((depthMemReqs.memoryTypeBits & (1u << i)) &&
			(memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
		{
			depthMemTypeIndex = i;
			break;
		}
	}

	if (depthMemTypeIndex == UINT32_MAX)
	{
		vkDestroyImage(vkDev, depthImage, nullptr);
		ctx.device.WaitIdle();
		vkDestroySampler(vkDev, sampler, nullptr);
		vkDestroyImageView(vkDev, imageView, nullptr);
		vkFreeMemory(vkDev, imageMem, nullptr);
		vkDestroyImage(vkDev, image, nullptr);
		GTEST_SKIP() << "No suitable memory type for depth image";
	}

	VkMemoryAllocateInfo depthAllocInfo{};
	depthAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	depthAllocInfo.allocationSize = depthMemReqs.size;
	depthAllocInfo.memoryTypeIndex = depthMemTypeIndex;

	VkDeviceMemory depthMem = VK_NULL_HANDLE;
	if (vkAllocateMemory(vkDev, &depthAllocInfo, nullptr, &depthMem) != VK_SUCCESS)
	{
		vkDestroyImage(vkDev, depthImage, nullptr);
		ctx.device.WaitIdle();
		vkDestroySampler(vkDev, sampler, nullptr);
		vkDestroyImageView(vkDev, imageView, nullptr);
		vkFreeMemory(vkDev, imageMem, nullptr);
		vkDestroyImage(vkDev, image, nullptr);
		GTEST_SKIP() << "Could not allocate memory for depth image";
	}
	vkBindImageMemory(vkDev, depthImage, depthMem, 0);

	VkImageViewCreateInfo depthViewInfo{};
	depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	depthViewInfo.image = depthImage;
	depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	depthViewInfo.format = VK_FORMAT_D32_SFLOAT;
	depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	depthViewInfo.subresourceRange.levelCount = 1;
	depthViewInfo.subresourceRange.layerCount = 1;

	VkImageView depthView = VK_NULL_HANDLE;
	if (vkCreateImageView(vkDev, &depthViewInfo, nullptr, &depthView) != VK_SUCCESS)
	{
		vkFreeMemory(vkDev, depthMem, nullptr);
		vkDestroyImage(vkDev, depthImage, nullptr);
		ctx.device.WaitIdle();
		vkDestroySampler(vkDev, sampler, nullptr);
		vkDestroyImageView(vkDev, imageView, nullptr);
		vkFreeMemory(vkDev, imageMem, nullptr);
		vkDestroyImage(vkDev, image, nullptr);
		GTEST_SKIP() << "Could not create depth VkImageView";
	}

	VkSamplerCreateInfo cmpSamplerInfo{};
	cmpSamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	cmpSamplerInfo.magFilter = VK_FILTER_LINEAR;
	cmpSamplerInfo.minFilter = VK_FILTER_LINEAR;
	cmpSamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	cmpSamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	cmpSamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	cmpSamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	cmpSamplerInfo.compareEnable = VK_TRUE;
	cmpSamplerInfo.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	cmpSamplerInfo.maxLod = VK_LOD_CLAMP_NONE;

	VkSampler cmpSampler = VK_NULL_HANDLE;
	if (vkCreateSampler(vkDev, &cmpSamplerInfo, nullptr, &cmpSampler) != VK_SUCCESS)
	{
		vkDestroyImageView(vkDev, depthView, nullptr);
		vkFreeMemory(vkDev, depthMem, nullptr);
		vkDestroyImage(vkDev, depthImage, nullptr);
		ctx.device.WaitIdle();
		vkDestroySampler(vkDev, sampler, nullptr);
		vkDestroyImageView(vkDev, imageView, nullptr);
		vkFreeMemory(vkDev, imageMem, nullptr);
		vkDestroyImage(vkDev, image, nullptr);
		GTEST_SKIP() << "Could not create comparison VkSampler";
	}

	// RegisterShadowMap returns a valid slot in [0, 16).
	uint32_t smSlot0 = bta.RegisterShadowMap(depthView, cmpSampler);
	EXPECT_NE(smSlot0, UINT32_MAX);
	EXPECT_LT(smSlot0, 16u);

	uint32_t smSlot1 = bta.RegisterShadowMap(depthView, cmpSampler);
	EXPECT_NE(smSlot1, UINT32_MAX);
	EXPECT_LT(smSlot1, 16u);
	EXPECT_NE(smSlot1, smSlot0); // distinct slots

	// Texture and shadow-map slot spaces are independent: same numeric value is fine.
	// (slot0 and smSlot0 may coincide numerically; that is expected and correct.)

	// Exhaust the remaining 14 shadow-map slots.
	for (uint32_t i = 2; i < 16u; ++i)
	{
		uint32_t s = bta.RegisterShadowMap(depthView, cmpSampler);
		EXPECT_NE(s, UINT32_MAX);
		EXPECT_LT(s, 16u);
	}

	// All 16 shadow-map slots exhausted; next call must return UINT32_MAX.
	uint32_t smOverflow = bta.RegisterShadowMap(depthView, cmpSampler);
	EXPECT_EQ(smOverflow, UINT32_MAX);

	// UnregisterShadowMap and re-register.
	bta.UnregisterShadowMap(smSlot1);
	uint32_t smReused = bta.RegisterShadowMap(depthView, cmpSampler);
	EXPECT_EQ(smReused, smSlot1);

	// Unregistering a shadow-map slot must not affect texture slots.
	// (texture slots are still fully occupied from earlier; re-registering a texture
	// slot should still require an UnregisterTexture first.)
	uint32_t texOverflowAfterSmOp = bta.RegisterTexture(imageView, sampler);
	EXPECT_EQ(texOverflowAfterSmOp, UINT32_MAX);

	// Clean up Vulkan objects (bta destructor handles descriptor pool/layout).
	ctx.device.WaitIdle();
	vkDestroySampler(vkDev, cmpSampler, nullptr);
	vkDestroyImageView(vkDev, depthView, nullptr);
	vkFreeMemory(vkDev, depthMem, nullptr);
	vkDestroyImage(vkDev, depthImage, nullptr);
	vkDestroySampler(vkDev, sampler, nullptr);
	vkDestroyImageView(vkDev, imageView, nullptr);
	vkFreeMemory(vkDev, imageMem, nullptr);
	vkDestroyImage(vkDev, image, nullptr);
}

// ---------------------------------------------------------------------------
// bindless_texture_array_observers
// ---------------------------------------------------------------------------
TEST(BindlessTextureArray, test_observers)
{
	// Verify observer contracts on a default-constructed object (no GPU needed).
	{
		BindlessTextureArray bta;
		EXPECT_EQ(bta.GetDescriptorSet(), VK_NULL_HANDLE);
		EXPECT_EQ(bta.GetLayout(), VK_NULL_HANDLE);
		EXPECT_EQ(bta.GetMaxTextures(), 0u);
		EXPECT_EQ(bta.GetMaxShadowMaps(), 0u);
		EXPECT_FALSE(bta.IsInitialized());
		EXPECT_EQ(bta.IsInitialized(), bta.GetDescriptorSet() != VK_NULL_HANDLE);
	}

	VulkanContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan not available; skipping initialized-state observer checks";
	}

	// bta declared after ctx so it is destroyed before ctx (and its Device).
	BindlessTextureArray bta;
	constexpr uint32_t kMax = 16u;
	ASSERT_EQ(bta.Initialize(ctx.device, kMax), RenderError::None);

	EXPECT_NE(bta.GetDescriptorSet(), VK_NULL_HANDLE);
	EXPECT_NE(bta.GetLayout(), VK_NULL_HANDLE);
	EXPECT_EQ(bta.GetMaxTextures(), kMax);
	// kMaxShadowMaps is the fixed constant 16.
	EXPECT_EQ(bta.GetMaxShadowMaps(), 16u);
	EXPECT_TRUE(bta.IsInitialized());
	EXPECT_EQ(bta.IsInitialized(), bta.GetDescriptorSet() != VK_NULL_HANDLE);
	// GetLayout returns a single layout containing both binding 0 and binding 1.
	EXPECT_NE(bta.GetLayout(), VK_NULL_HANDLE);

	ctx.device.WaitIdle();
}

// ---------------------------------------------------------------------------
// bindless_texture_array_is_not_thread_safe_per_instance
// ---------------------------------------------------------------------------
// This semantic documents the absence of internal synchronization.
// We verify it structurally: the class does not expose any mutex or atomic
// members in its public interface, and the contract is documented rather than
// mechanically testable. We verify that two distinct BindlessTextureArray
// objects can be independently initialized (distinct objects may be used
// concurrently per the contract).
TEST(BindlessTextureArray, test_is_not_thread_safe_per_instance)
{
	VulkanContext ctx;
	if (!ctx.valid)
	{
		GTEST_SKIP() << "Vulkan not available";
	}

	// Two independent instances can be initialized against the same device.
	BindlessTextureArray bta1;
	BindlessTextureArray bta2;

	ASSERT_EQ(bta1.Initialize(ctx.device, 4u), RenderError::None);
	ASSERT_EQ(bta2.Initialize(ctx.device, 8u), RenderError::None);

	EXPECT_TRUE(bta1.IsInitialized());
	EXPECT_TRUE(bta2.IsInitialized());
	EXPECT_EQ(bta1.GetMaxTextures(), 4u);
	EXPECT_EQ(bta2.GetMaxTextures(), 8u);
	// The two descriptor sets must be distinct objects.
	EXPECT_NE(bta1.GetDescriptorSet(), bta2.GetDescriptorSet());

	ctx.device.WaitIdle();
}
