#include "virasa/renderer/scene/SceneRenderer.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>
#include <vector>

#include "virasa/core/Logger.h"
#include "virasa/ecs/ComponentSystem.h"
#include "virasa/ecs/Components.h"
#include "virasa/math/Projection.h"
#include "virasa/math/Transform.h"
#include "virasa/renderer/geometry/Primitives.h"
#include "virasa/renderer/lighting/ShadowTable.h"
#include "virasa/renderer/resources/Buffer.h"

namespace virasa::renderer::scene
{

namespace
{
constexpr uint32_t kInvalidSlot = 0xFFFFFFFFu;

size_t GetTexelSize(VkFormat format) noexcept
{
	switch (format)
	{
		case VK_FORMAT_R8_UNORM:
			return 1;
		case VK_FORMAT_R8G8_UNORM:
			return 2;
		case VK_FORMAT_R8G8B8A8_UNORM:
		case VK_FORMAT_R8G8B8A8_SRGB:
		case VK_FORMAT_B8G8R8A8_UNORM:
		case VK_FORMAT_B8G8R8A8_SRGB:
			return 4;
		default:
			return 0;
	}
}

RenderError UploadImageWithStaging(const Device& device, const Context& context, Image& image,
	const void* data, size_t size_bytes)
{
	Buffer stagingBuffer;
	BufferConfig stagingConfig{};
	stagingConfig.size = static_cast<VkDeviceSize>(size_bytes);
	stagingConfig.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	stagingConfig.memoryProperties =
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

	RenderError error = stagingBuffer.Initialize(device, stagingConfig);
	if (error != RenderError::None)
	{
		return error;
	}

	error = stagingBuffer.Upload(data, size_bytes);
	if (error != RenderError::None)
	{
		return error;
	}

	VkCommandBufferAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.commandPool = context.GetGraphicsCommandPool();
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
	VkResult result = vkAllocateCommandBuffers(device.GetHandle(), &allocateInfo, &commandBuffer);
	if (result != VK_SUCCESS)
	{
		return RenderError::CommandPoolCreateFailed;
	}

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
	if (result != VK_SUCCESS)
	{
		vkFreeCommandBuffers(
			device.GetHandle(), context.GetGraphicsCommandPool(), 1, &commandBuffer);
		return RenderError::CommandPoolCreateFailed;
	}

	VkImageMemoryBarrier toTransfer{};
	toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	toTransfer.image = image.GetHandle();
	toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	toTransfer.subresourceRange.baseMipLevel = 0;
	toTransfer.subresourceRange.levelCount = 1;
	toTransfer.subresourceRange.baseArrayLayer = 0;
	toTransfer.subresourceRange.layerCount = 1;
	toTransfer.srcAccessMask = 0;
	toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

	vkCmdPipelineBarrier(commandBuffer,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		0,
		nullptr,
		0,
		nullptr,
		1,
		&toTransfer);

	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = {0, 0, 0};
	region.imageExtent = {image.GetWidth(), image.GetHeight(), 1};

	vkCmdCopyBufferToImage(commandBuffer,
		stagingBuffer.GetHandle(),
		image.GetHandle(),
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&region);

	VkImageMemoryBarrier toSampled{};
	toSampled.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	toSampled.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	toSampled.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	toSampled.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	toSampled.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	toSampled.image = image.GetHandle();
	toSampled.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	toSampled.subresourceRange.baseMipLevel = 0;
	toSampled.subresourceRange.levelCount = 1;
	toSampled.subresourceRange.baseArrayLayer = 0;
	toSampled.subresourceRange.layerCount = 1;
	toSampled.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	toSampled.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0,
		0,
		nullptr,
		0,
		nullptr,
		1,
		&toSampled);

	result = vkEndCommandBuffer(commandBuffer);
	if (result != VK_SUCCESS)
	{
		vkFreeCommandBuffers(
			device.GetHandle(), context.GetGraphicsCommandPool(), 1, &commandBuffer);
		return RenderError::CommandPoolCreateFailed;
	}

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	result = vkQueueSubmit(device.GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
	if (result != VK_SUCCESS)
	{
		vkFreeCommandBuffers(
			device.GetHandle(), context.GetGraphicsCommandPool(), 1, &commandBuffer);
		return RenderError::CommandPoolCreateFailed;
	}

	result = vkQueueWaitIdle(device.GetGraphicsQueue());
	vkFreeCommandBuffers(device.GetHandle(), context.GetGraphicsCommandPool(), 1, &commandBuffer);
	if (result != VK_SUCCESS)
	{
		return RenderError::CommandPoolCreateFailed;
	}

	return RenderError::None;
}

void LogRendererError(const char* message)
{
	(void)virasa::Logger::GetLogger("renderer");
	(void)message;
}

virasa::VisualMaterial MakeDefaultMaterial() noexcept
{
	virasa::VisualMaterial material{};
	material.factors.baseColorFactor = virasa::math::Vec4(0.9f, 0.3f, 0.2f, 1.0f);
	return material;
}

const virasa::ecs::ComponentSystem* FindSystem(
	const virasa::ecs::World& world, std::string_view name)
{
	const virasa::ecs::ComponentId id = world.GetSystemId(name);
	if (id == 0xFFFFFFFFu)
	{
		return nullptr;
	}

	return &world.GetSystem(id);
}

} // namespace

// ---------------------------------------------------------------------------
// Internal push-constant layout (192 bytes)
// ---------------------------------------------------------------------------
struct PushConstants
{
	virasa::math::Mat4 mvp;
	virasa::math::Mat4 model;
	uint64_t vertexBufferAddress;
	uint64_t indexBufferAddress;
	uint64_t materialBufferAddress;
	uint64_t lightBufferAddress;
	uint64_t shadowBufferAddress;
	uint32_t materialId;
	uint32_t lightCount;
	virasa::math::Vec4 highlight;
};
static_assert(sizeof(PushConstants) == 192, "PushConstants must be exactly 192 bytes");

// ---------------------------------------------------------------------------
// Shadow push-constant layout (80 bytes)
// ---------------------------------------------------------------------------
struct ShadowPushConstants
{
	virasa::math::Mat4 lightMvp;	// 64
	uint64_t vertexBufferAddress; //  8
	uint64_t indexBufferAddress;	//  8
};
static_assert(sizeof(ShadowPushConstants) == 80, "ShadowPushConstants must be exactly 80 bytes");

// ---------------------------------------------------------------------------
// Pick push-constant layout (80 bytes)
// ---------------------------------------------------------------------------
struct PickPushConstants
{
	virasa::math::Mat4 mvp;
	uint64_t vertexBufferAddress;
	uint64_t indexBufferAddress;
	uint32_t entityIndex;
	uint32_t entityGeneration;
};
static_assert(sizeof(PickPushConstants) == 88, "PickPushConstants must be exactly 88 bytes");

// Shadow rendering constants
constexpr float kShadowNearPlane = 0.05f;
constexpr float kDirectionalShadowDistance = 100.0f;
constexpr float kDirectionalShadowExtent = 25.0f;
constexpr uint32_t kShadowMapResolution = 2048u;
constexpr uint32_t kMaxShadowMaps = 8u;
constexpr float kShadowDepthBias = 0.0015f;
constexpr float kShadowSlopeBias = 0.0025f;

// Outline-highlight constants
constexpr float kOutlineWidthPixels = 3.0f;

// Outline compute push constants (32 bytes)
struct OutlineComputePushConstants
{
	uint32_t extentX;
	uint32_t extentY;
	uint32_t srcSlot;
	uint32_t dstSlot;
	uint32_t maskSlot;
	int32_t stepSize;
	uint32_t _pad0;
	uint32_t _pad1;
};
static_assert(sizeof(OutlineComputePushConstants) == 32,
	"OutlineComputePushConstants must be exactly 32 bytes");

// Outline composite push constants (32 bytes)
struct OutlineCompositePushConstants
{
	uint32_t extentX;
	uint32_t extentY;
	uint32_t jfaSlot;
	uint32_t maskSlot;
	float widthPixels;
	float _pad0;
	float _pad1;
	float _pad2;
};
static_assert(sizeof(OutlineCompositePushConstants) == 32,
	"OutlineCompositePushConstants must be exactly 32 bytes");

// ---------------------------------------------------------------------------
// Initialize
// ---------------------------------------------------------------------------
virasa::RenderError SceneRenderer::Initialize(const virasa::Device& device,
	const virasa::Context& context, const virasa::ui::FontAtlas& atlas)
{
	// Step 1: store borrowed pointers
	_device = &device;
	_context = &context;

	// Step 2: initialize registries
	{
		// MeshRegistry has no Initialize — default-constructed is ready.
		auto err = _imageRegistry.Initialize(device);
		if (err != virasa::RenderError::None)
		{
			_initialized = false;
			return err;
		}
		err = _bufferRegistry.Initialize(device);
		if (err != virasa::RenderError::None)
		{
			_initialized = false;
			return err;
		}
	}

	// Step 3: initialize material and light tables
	{
		auto err = _materialTable.Initialize(device, 64);
		if (err != virasa::RenderError::None)
		{
			_initialized = false;
			return err;
		}
		const uint32_t framesInFlight = context.GetMaxFramesInFlight();
		err = _lightTable.Initialize(device, 256, framesInFlight);
		if (err != virasa::RenderError::None)
		{
			_initialized = false;
			return err;
		}
		err = _shadowTable.Initialize(device, kMaxShadowMaps, framesInFlight);
		if (err != virasa::RenderError::None)
		{
			_initialized = false;
			return err;
		}
	}

	// Step 4: initialize bindless texture array
	{
		auto err = _bindlessTextures.Initialize(device, 256);
		if (err != virasa::RenderError::None)
		{
			_initialized = false;
			return err;
		}
	}

	// Step 5: create 1x1 white image and upload
	{
		virasa::ImageConfig cfg{};
		cfg.width = 1;
		cfg.height = 1;
		cfg.format = VK_FORMAT_R8G8B8A8_UNORM;
		cfg.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		cfg.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		cfg.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		cfg.flags = 0;

		auto err = _whiteImage.Initialize(device, cfg);
		if (err != virasa::RenderError::None)
		{
			_initialized = false;
			return err;
		}

		const uint32_t whitePixel = 0xFFFFFFFFu;
		err = UploadImageWithStaging(
			device, context, _whiteImage, &whitePixel, sizeof(whitePixel));
		if (err != virasa::RenderError::None)
		{
			_initialized = false;
			return err;
		}
	}

	// Step 6: initialize default sampler, shadow sampler, and outline sampler
	{
		virasa::SamplerConfig samplerCfg{};
		samplerCfg.magFilter = VK_FILTER_LINEAR;
		samplerCfg.minFilter = VK_FILTER_LINEAR;
		samplerCfg.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCfg.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCfg.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCfg.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

		auto err = _defaultSampler.Initialize(device, samplerCfg);
		if (err != virasa::RenderError::None)
		{
			_initialized = false;
			return err;
		}

		virasa::SamplerConfig shadowCfg{};
		shadowCfg.magFilter = VK_FILTER_LINEAR;
		shadowCfg.minFilter = VK_FILTER_LINEAR;
		shadowCfg.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		shadowCfg.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		shadowCfg.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		shadowCfg.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		shadowCfg.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		shadowCfg.compareEnable = true;
		shadowCfg.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

		err = _shadowSampler.Initialize(device, shadowCfg);
		if (err != virasa::RenderError::None)
		{
			_initialized = false;
			return err;
		}

		virasa::SamplerConfig outlineCfg{};
		outlineCfg.magFilter = VK_FILTER_NEAREST;
		outlineCfg.minFilter = VK_FILTER_NEAREST;
		outlineCfg.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		outlineCfg.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		outlineCfg.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		outlineCfg.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		outlineCfg.compareEnable = false;

		err = _outlineSampler.Initialize(device, outlineCfg);
		if (err != virasa::RenderError::None)
		{
			_initialized = false;
			return err;
		}
	}

	// Step 7: register white image into bindless at slot 0
	{
		uint32_t slot = _bindlessTextures.RegisterTexture(
			_whiteImage.GetView(), _defaultSampler.GetHandle());
		if (slot == 0xFFFFFFFFu || slot != 0u)
		{
			_initialized = false;
			return virasa::RenderError::PipelineCreateFailed;
		}
		assert(slot == 0u);
	}

	// Step 8: allocate default material at slot 0
	{
		uint32_t id = _materialTable.Allocate(MakeDefaultMaterial());
		if (id == 0xFFFFFFFFu || id != 0u)
		{
			_initialized = false;
			return virasa::RenderError::PipelineCreateFailed;
		}
		assert(id == 0u);
	}

	// Step 9: load forward, shadow, outline, and picking shaders
	{
		auto err = _forwardVertexShader.Initialize(device, "shaders/cube.vert.spv");
		if (err != virasa::RenderError::None)
		{
			_initialized = false;
			return err;
		}
		err = _forwardFragmentShader.Initialize(device, "shaders/cube.frag.spv");
		if (err != virasa::RenderError::None)
		{
			_initialized = false;
			return err;
		}
		err = _terrainFragmentShader.Initialize(device, "shaders/terrain.frag.spv");
		if (err != virasa::RenderError::None)
		{
			_initialized = false;
			return err;
		}
		err = _shadowVertexShader.Initialize(device, "shaders/shadow_depth.vert.spv");
		if (err != virasa::RenderError::None)
		{
			_initialized = false;
			return err;
		}
		err = _shadowFragmentShader.Initialize(device, "shaders/shadow_depth.frag.spv");
		if (err != virasa::RenderError::None)
		{
			_initialized = false;
			return err;
		}
		err = _outlineMaskFragmentShader.Initialize(device, "shaders/outline_mask.frag.spv");
		if (err != virasa::RenderError::None)
		{
			_initialized = false;
			return err;
		}
		err = _fullscreenVertexShader.Initialize(device, "shaders/fullscreen.vert.spv");
		if (err != virasa::RenderError::None)
		{
			_initialized = false;
			return err;
		}
		err = _outlineCompositeFragmentShader.Initialize(
			device, "shaders/outline_composite.frag.spv");
		if (err != virasa::RenderError::None)
		{
			_initialized = false;
			return err;
		}
		err = _jfaSeedShader.Initialize(device, "shaders/jfa_seed.comp.spv");
		if (err != virasa::RenderError::None)
		{
			_initialized = false;
			return err;
		}
		err = _jfaStepShader.Initialize(device, "shaders/jfa_step.comp.spv");
		if (err != virasa::RenderError::None)
		{
			_initialized = false;
			return err;
		}
		err = _pickIdVertexShader.Initialize(device, "shaders/pick_id.vert.spv");
		if (err != virasa::RenderError::None)
		{
			_initialized = false;
			return err;
		}
		err = _pickIdFragmentShader.Initialize(device, "shaders/pick_id.frag.spv");
		if (err != virasa::RenderError::None)
		{
			_initialized = false;
			return err;
		}
		err = _pickClearFragmentShader.Initialize(device, "shaders/pick_clear.frag.spv");
		if (err != virasa::RenderError::None)
		{
			_initialized = false;
			return err;
		}
	}

	// Step 10: build four forward pipeline permutations
	{
		// Base builder configuration shared by all permutations
		auto makeBase = [&]() -> virasa::PipelineBuilder
		{
			virasa::PipelineBuilder b;
			b.SetVertexShader(_forwardVertexShader)
				.SetFragmentShader(_forwardFragmentShader)
				.SetColorFormat(context.GetSwapchainFormat())
				.SetDepthFormat(context.GetDepthFormat())
				.SetFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
				.SetDepthTest(true)
				.SetDepthCompareOp(VK_COMPARE_OP_LESS)
				.AddPushConstantRange(
					VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 192)
				.AddDescriptorSetLayout(_bindlessTextures.GetLayout());
			return b;
		};

		// _opaquePipeline: back-face cull, depth write on, blend off
		{
			auto b = makeBase();
			b.SetCullMode(VK_CULL_MODE_BACK_BIT).SetDepthWrite(true).SetBlendEnabled(false);
			auto err = b.Build(device, _opaquePipeline);
			if (err != virasa::RenderError::None)
			{
				_initialized = false;
				return err;
			}
		}

		// _terrainPipeline: back-face cull, depth write on, blend off
		{
			virasa::PipelineBuilder b;
			b.SetVertexShader(_forwardVertexShader)
				.SetFragmentShader(_terrainFragmentShader)
				.SetColorFormat(context.GetSwapchainFormat())
				.SetDepthFormat(context.GetDepthFormat())
				.SetFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
				.SetDepthTest(true)
				.SetDepthCompareOp(VK_COMPARE_OP_LESS)
				.AddPushConstantRange(
					VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 192)
				.AddDescriptorSetLayout(_bindlessTextures.GetLayout());
			b.SetCullMode(VK_CULL_MODE_BACK_BIT).SetDepthWrite(true).SetBlendEnabled(false);
			auto err = b.Build(device, _terrainPipeline);
			if (err != virasa::RenderError::None)
			{
				_initialized = false;
				return err;
			}
		}

		// _opaqueDoubleSidedPipeline: no cull, depth write on, blend off
		{
			auto b = makeBase();
			b.SetCullMode(VK_CULL_MODE_NONE).SetDepthWrite(true).SetBlendEnabled(false);
			auto err = b.Build(device, _opaqueDoubleSidedPipeline);
			if (err != virasa::RenderError::None)
			{
				_initialized = false;
				return err;
			}
		}

		// _blendPipeline: back-face cull, depth write OFF, blend on
		{
			auto b = makeBase();
			b.SetCullMode(VK_CULL_MODE_BACK_BIT).SetDepthWrite(false).SetBlendEnabled(true);
			auto err = b.Build(device, _blendPipeline);
			if (err != virasa::RenderError::None)
			{
				_initialized = false;
				return err;
			}
		}

		// _blendDoubleSidedPipeline: no cull, depth write OFF, blend on
		{
			auto b = makeBase();
			b.SetCullMode(VK_CULL_MODE_NONE).SetDepthWrite(false).SetBlendEnabled(true);
			auto err = b.Build(device, _blendDoubleSidedPipeline);
			if (err != virasa::RenderError::None)
			{
				_initialized = false;
				return err;
			}
		}
	}

	// Step 10b: build shadow pipeline (depth-only)
	{
		virasa::PipelineBuilder b;
		b.SetVertexShader(_shadowVertexShader)
			.SetFragmentShader(_shadowFragmentShader)
			.SetDepthFormat(VK_FORMAT_D32_SFLOAT)
			.SetFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
			.SetCullMode(VK_CULL_MODE_BACK_BIT)
			.SetDepthTest(true)
			.SetDepthWrite(true)
			.SetDepthCompareOp(VK_COMPARE_OP_LESS)
			.SetBlendEnabled(false)
			.AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, 80);
		auto err = b.Build(device, _shadowPipeline);
		if (err != virasa::RenderError::None)
		{
			_initialized = false;
			return err;
		}
	}

	// Step 10c: build outline-highlight pipelines
	{
		// _maskPipeline: graphics, color-only (no depth), depth-test-free, no cull, blend off
		{
			virasa::PipelineBuilder b;
			b.SetVertexShader(_forwardVertexShader)
				.SetFragmentShader(_outlineMaskFragmentShader)
				.SetColorFormat(VK_FORMAT_R8G8B8A8_UNORM)
				.SetFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
				.SetCullMode(VK_CULL_MODE_NONE)
				.SetDepthTest(false)
				.SetDepthWrite(false)
				.SetBlendEnabled(false)
				.AddPushConstantRange(
					VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 192)
				.AddDescriptorSetLayout(_bindlessTextures.GetLayout());
			auto err = b.Build(device, _maskPipeline);
			if (err != virasa::RenderError::None)
			{
				_initialized = false;
				return err;
			}
		}

		// _jfaSeedPipeline: compute
		{
			virasa::PipelineBuilder b;
			b.SetComputeShader(_jfaSeedShader)
				.AddDescriptorSetLayout(_bindlessTextures.GetLayout())
				.AddPushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, 32);
			auto err = b.BuildCompute(device, _jfaSeedPipeline);
			if (err != virasa::RenderError::None)
			{
				_initialized = false;
				return err;
			}
		}

		// _jfaStepPipeline: compute
		{
			virasa::PipelineBuilder b;
			b.SetComputeShader(_jfaStepShader)
				.AddDescriptorSetLayout(_bindlessTextures.GetLayout())
				.AddPushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, 32);
			auto err = b.BuildCompute(device, _jfaStepPipeline);
			if (err != virasa::RenderError::None)
			{
				_initialized = false;
				return err;
			}
		}

		// _outlineCompositePipeline: graphics, color-only, blend enabled
		{
			virasa::PipelineBuilder b;
			b.SetVertexShader(_fullscreenVertexShader)
				.SetFragmentShader(_outlineCompositeFragmentShader)
				.SetColorFormat(context.GetSwapchainFormat())
				.SetFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
				.SetCullMode(VK_CULL_MODE_NONE)
				.SetDepthTest(false)
				.SetDepthWrite(false)
				.SetBlendEnabled(true)
				.AddPushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, 0, 32)
				.AddDescriptorSetLayout(_bindlessTextures.GetLayout());
			auto err = b.Build(device, _outlineCompositePipeline);
			if (err != virasa::RenderError::None)
			{
				_initialized = false;
				return err;
			}
		}
	}

	// Step 10d: build picking pipelines
	{
		{
			virasa::PipelineBuilder b;
			b.SetVertexShader(_fullscreenVertexShader)
				.SetFragmentShader(_pickClearFragmentShader)
				.SetColorFormat(VK_FORMAT_R32G32_UINT)
				.SetFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
				.SetCullMode(VK_CULL_MODE_NONE)
				.SetDepthTest(false)
				.SetDepthWrite(false)
				.SetBlendEnabled(false);
			auto err = b.Build(device, _pickClearPipeline);
			if (err != virasa::RenderError::None)
			{
				_initialized = false;
				return err;
			}
		}

		{
			virasa::PipelineBuilder b;
			b.SetVertexShader(_pickIdVertexShader)
				.SetFragmentShader(_pickIdFragmentShader)
				.SetColorFormat(VK_FORMAT_R32G32_UINT)
				.SetDepthFormat(context.GetDepthFormat())
				.SetFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
				.SetCullMode(VK_CULL_MODE_BACK_BIT)
				.SetDepthTest(true)
				.SetDepthWrite(true)
				.SetDepthCompareOp(VK_COMPARE_OP_LESS)
				.SetBlendEnabled(false)
				.AddPushConstantRange(
					VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
					sizeof(PickPushConstants));
			auto err = b.Build(device, _pickIdPipeline);
			if (err != virasa::RenderError::None)
			{
				_initialized = false;
				return err;
			}
		}
	}

	// Step 11: initialize graph
	{
		auto err = _graph.Initialize(device, _imageRegistry, _bufferRegistry);
		if (err != virasa::RenderError::None)
		{
			_initialized = false;
			return err;
		}
	}

	// Step 12: initialize UI pass
	{
		auto err = _uiPass.Initialize(device, context, atlas, _bindlessTextures);
		if (err != virasa::RenderError::None)
		{
			_initialized = false;
			return err;
		}
	}

	// Step 13: create picking readback ring
	{
		const uint32_t frameCount = context.GetMaxFramesInFlight();
		_pickReadbackBuffers.resize(frameCount);
		for (uint32_t i = 0; i < frameCount; ++i)
		{
			virasa::BufferConfig cfg{};
			cfg.size = 2u * sizeof(uint32_t);
			cfg.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			cfg.memoryProperties =
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			auto err = _pickReadbackBuffers[i].Initialize(device, cfg);
			if (err != virasa::RenderError::None)
			{
				_initialized = false;
				return err;
			}
		}
		_pickPending.assign(frameCount, 0u);
		_pickRequested = false;
		_pickX = 0u;
		_pickY = 0u;
		_pickResult = virasa::PickResult{};
	}

	_initialized = true;
	return virasa::RenderError::None;
}

// ---------------------------------------------------------------------------
// Registry accessors
// ---------------------------------------------------------------------------
virasa::renderer::MeshRegistry& SceneRenderer::GetMeshRegistry() noexcept
{
	return _meshRegistry;
}

virasa::VisualMaterialTable& SceneRenderer::GetMaterialTable() noexcept
{
	return _materialTable;
}

// ---------------------------------------------------------------------------
// CreateDefaultCubeMesh
// ---------------------------------------------------------------------------
uint32_t SceneRenderer::CreateDefaultCubeMesh()
{
	virasa::MeshData meshData = virasa::CreateCube();

	virasa::Mesh mesh;
	auto err = mesh.Initialize(*_device, *_context, meshData);
	if (err != virasa::RenderError::None)
	{
		return 0xFFFFFFFFu;
	}

	uint32_t id = _meshRegistry.Allocate(std::move(mesh));
	return id;
}

// ---------------------------------------------------------------------------
// CreateDefaultCylinderMesh
// ---------------------------------------------------------------------------
uint32_t SceneRenderer::CreateDefaultCylinderMesh()
{
	virasa::MeshData meshData = virasa::CreateCylinder();

	virasa::Mesh mesh;
	auto err = mesh.Initialize(*_device, *_context, meshData);
	if (err != virasa::RenderError::None)
	{
		return 0xFFFFFFFFu;
	}

	uint32_t id = _meshRegistry.Allocate(std::move(mesh));
	return id;
}

// ---------------------------------------------------------------------------
// CreateDefaultMaterial
// ---------------------------------------------------------------------------
uint32_t SceneRenderer::CreateDefaultMaterial()
{
	uint32_t id = _materialTable.Allocate(MakeDefaultMaterial());
	return id;
}

// ---------------------------------------------------------------------------
// RegisterMesh
// ---------------------------------------------------------------------------
virasa::RegisterError SceneRenderer::RegisterMesh(
	const virasa::MeshData& mesh_data, uint32_t& out_mesh_id)
{
	if (mesh_data.vertices.empty() || mesh_data.indices.empty() ||
		(mesh_data.indices.size() % 3u) != 0u)
	{
		LogRendererError("RegisterMesh invalid input");
		return virasa::RegisterError::InvalidInput;
	}

	virasa::Mesh mesh;
	const virasa::RenderError error = mesh.Initialize(*_device, *_context, mesh_data);
	if (error != virasa::RenderError::None)
	{
		LogRendererError("RegisterMesh upload failed");
		return virasa::RegisterError::UploadFailed;
	}

	const uint32_t id = _meshRegistry.Allocate(std::move(mesh));
	if (id == kInvalidSlot)
	{
		LogRendererError("RegisterMesh out of slots");
		return virasa::RegisterError::OutOfSlots;
	}

	out_mesh_id = id;
	return virasa::RegisterError::None;
}

// ---------------------------------------------------------------------------
// RegisterMaterial
// ---------------------------------------------------------------------------
virasa::RegisterError SceneRenderer::RegisterMaterial(
	const virasa::VisualMaterial& material, uint32_t& out_material_id)
{
	const uint32_t id = _materialTable.Allocate(material);
	if (id == kInvalidSlot)
	{
		LogRendererError("RegisterMaterial out of slots");
		return virasa::RegisterError::OutOfSlots;
	}

	out_material_id = id;
	return virasa::RegisterError::None;
}

// ---------------------------------------------------------------------------
// RegisterTexture
// ---------------------------------------------------------------------------
virasa::RegisterError SceneRenderer::RegisterTexture(
	const virasa::TextureUpload& upload, uint32_t& out_slot)
{
	const size_t texelSize = GetTexelSize(upload.format);
	const size_t expectedSize =
		static_cast<size_t>(upload.width) * static_cast<size_t>(upload.height) * texelSize;
	if (upload.width == 0u || upload.height == 0u || upload.pixels.data() == nullptr ||
		texelSize == 0u || upload.pixels.size() != expectedSize)
	{
		LogRendererError("RegisterTexture invalid input");
		return virasa::RegisterError::InvalidInput;
	}

	virasa::Image image;
	virasa::ImageConfig imageConfig{};
	imageConfig.width = upload.width;
	imageConfig.height = upload.height;
	imageConfig.format = upload.format;
	imageConfig.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imageConfig.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	imageConfig.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	virasa::RenderError renderError = image.Initialize(*_device, imageConfig);
	if (renderError != virasa::RenderError::None)
	{
		LogRendererError("RegisterTexture image create failed");
		return virasa::RegisterError::UploadFailed;
	}

	renderError = UploadImageWithStaging(
		*_device, *_context, image, upload.pixels.data(), upload.pixels.size());
	if (renderError != virasa::RenderError::None)
	{
		LogRendererError("RegisterTexture upload failed");
		return virasa::RegisterError::UploadFailed;
	}

	VkSampler samplerHandle = VK_NULL_HANDLE;
	for (auto& entry : _samplerCache)
	{
		if (entry.first == upload.sampler)
		{
			samplerHandle = entry.second.GetHandle();
			break;
		}
	}

	if (samplerHandle == VK_NULL_HANDLE)
	{
		virasa::Sampler sampler;
		renderError = sampler.Initialize(*_device, upload.sampler);
		if (renderError != virasa::RenderError::None)
		{
			LogRendererError("RegisterTexture sampler create failed");
			return virasa::RegisterError::SamplerCreateFailed;
		}

		samplerHandle = sampler.GetHandle();
		_samplerCache.emplace_back(upload.sampler, std::move(sampler));
	}

	_textureImages.push_back(std::move(image));
	const virasa::Image& storedImage = _textureImages.back();
	const uint32_t slot = _bindlessTextures.RegisterTexture(storedImage.GetView(), samplerHandle);
	if (slot == kInvalidSlot)
	{
		_textureImages.pop_back();
		LogRendererError("RegisterTexture out of slots");
		return virasa::RegisterError::OutOfSlots;
	}

	out_slot = slot;
	return virasa::RegisterError::None;
}

// ---------------------------------------------------------------------------
// BeginFrame
// ---------------------------------------------------------------------------
virasa::SwapchainStatus SceneRenderer::BeginFrame(uint32_t sceneWidth, uint32_t sceneHeight)
{
	// const_cast is justified: the Context is the same object the application
	// initialized and is mutably reachable from the application's call site.
	auto* mutableContext = const_cast<virasa::Context*>(_context);

	virasa::SwapchainStatus status = mutableContext->BeginFrame();
	if (status == virasa::SwapchainStatus::NotReady || status == virasa::SwapchainStatus::Error)
	{
		return status;
	}

	// Propagate current frame index to per-frame resources before Begin/DeclareImage
	const uint32_t frameIndex = _context->GetCurrentFrameIndex();
	_imageRegistry.SetFrameIndex(frameIndex);
	_lightTable.SetFrameIndex(frameIndex);
	_shadowTable.SetFrameIndex(frameIndex);

	if (frameIndex < _pickPending.size() && _pickPending[frameIndex] != 0u)
	{
		void* mapped = _pickReadbackBuffers[frameIndex].Map();
		if (mapped != nullptr)
		{
			const uint32_t* words = static_cast<const uint32_t*>(mapped);
			_pickResult.entity = virasa::DecodePickId(words[0], words[1]);
			_pickResult.valid = true;
		}
		_pickPending[frameIndex] = 0u;
	}

	_graph.Begin();

	// Import swapchain image
	_frameSwapchainHandle = _graph.ImportImage(_context->GetCurrentImage(),
		_context->GetCurrentImageView(),
		_context->GetSwapchainFormat(),
		_context->GetSwapchainExtent(),
		virasa::renderer::graph::ResourceUsage::Undefined);

	// Declare transient scene color
	virasa::renderer::graph::GraphImageDesc colorDesc{};
	colorDesc.width = sceneWidth;
	colorDesc.height = sceneHeight;
	colorDesc.format = _context->GetSwapchainFormat();
	colorDesc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	colorDesc.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	_frameSceneColorHandle = _graph.DeclareImage(colorDesc);

	// Declare transient scene depth
	virasa::renderer::graph::GraphImageDesc depthDesc{};
	depthDesc.width = sceneWidth;
	depthDesc.height = sceneHeight;
	depthDesc.format = _context->GetDepthFormat();
	depthDesc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	depthDesc.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	_frameSceneDepthHandle = _graph.DeclareImage(depthDesc);

	_frameSceneWidth = sceneWidth;
	_frameSceneHeight = sceneHeight;
	_frameSceneSlot = 0xFFFFFFFFu;

	return status;
}

// ---------------------------------------------------------------------------
// RenderWorld
// ---------------------------------------------------------------------------
uint32_t SceneRenderer::RenderWorld(
	const virasa::ecs::World& world, virasa::ecs::Entity cameraEntity)
{
	// --- Derive camera matrices ---
	virasa::math::Mat4 viewMatrix(1.0f);
	virasa::math::Mat4 projMatrix(1.0f);
	virasa::math::Vec3 cameraEye(0.0f);

	{
		const virasa::ecs::TransformSystem& transforms = world.GetTransforms();
		// Retrieve camera component from the world's camera system
		// We access it via the World's component systems directly
		// The CameraComponent is stored in a system registered under "Camera"
		const virasa::ecs::ComponentSystem* camSys = FindSystem(world, "Camera");
		if (camSys != nullptr && camSys->Has(cameraEntity) && transforms.Has(cameraEntity))
		{
			const auto* camPtr = static_cast<const virasa::ecs::CameraComponent*>(
				camSys->GetRaw(cameraEntity));
			if (camPtr != nullptr)
			{
				const virasa::ecs::CameraComponent& cam = *camPtr;
				cameraEye = transforms.GetWorldPosition(cameraEntity);
				virasa::math::Vec3 forward = transforms.GetWorldForward(cameraEntity);
				viewMatrix = virasa::math::LookAtRH_ZUp(cameraEye, cameraEye + forward);

				float aspect = (cam.aspect > 0.0f)
							   ? cam.aspect
							   : static_cast<float>(_frameSceneWidth) /
								     static_cast<float>(_frameSceneHeight);

				projMatrix = virasa::math::PerspectiveRH_ZO(
					cam.fovY, aspect, cam.nearPlane, cam.farPlane);
			}
		}
	}

	// --- Gather lights ---
	std::vector<virasa::LightGPU> lights;

	{
		const virasa::ecs::TransformSystem& transforms = world.GetTransforms();

		// Directional lights
		const virasa::ecs::ComponentSystem* dlSys = FindSystem(world, "DirectionalLight");
		if (dlSys != nullptr)
		{
			for (auto entity : dlSys->Entities())
			{
				const auto* dlPtr =
					static_cast<const virasa::ecs::DirectionalLightComponent*>(
						dlSys->GetRaw(entity));
				if (dlPtr == nullptr)
					continue;
				const auto& dl = *dlPtr;
				virasa::LightGPU light{};
				light.type = static_cast<uint32_t>(virasa::LightType::Directional);
				light.direction = glm::normalize(dl.direction);
				light.color = dl.color * dl.intensity;
				lights.push_back(light);
			}
		}

		// Point lights
		const virasa::ecs::ComponentSystem* plSys = FindSystem(world, "PointLight");
		if (plSys != nullptr)
		{
			for (auto entity : plSys->Entities())
			{
				if (!transforms.Has(entity))
					continue;
				const auto* plPtr = static_cast<const virasa::ecs::PointLightComponent*>(
					plSys->GetRaw(entity));
				if (plPtr == nullptr)
					continue;
				const auto& pl = *plPtr;
				virasa::LightGPU light{};
				light.type = static_cast<uint32_t>(virasa::LightType::Point);
				light.position = transforms.GetWorldPosition(entity);
				light.range = pl.range;
				light.color = pl.color * pl.intensity;
				lights.push_back(light);
			}
		}

		// Spot lights
		const virasa::ecs::ComponentSystem* slSys = FindSystem(world, "SpotLight");
		if (slSys != nullptr)
		{
			for (auto entity : slSys->Entities())
			{
				if (!transforms.Has(entity))
					continue;
				const auto* slPtr = static_cast<const virasa::ecs::SpotLightComponent*>(
					slSys->GetRaw(entity));
				if (slPtr == nullptr)
					continue;
				const auto& sl = *slPtr;
				virasa::LightGPU light{};
				light.type = static_cast<uint32_t>(virasa::LightType::Spot);
				light.position = transforms.GetWorldPosition(entity);
				light.direction = transforms.GetWorldForward(entity);
				light.range = sl.range;
				light.color = sl.color * sl.intensity;
				light.innerConeCos = sl.innerConeCos;
				light.outerConeCos = sl.outerConeCos;
				lights.push_back(light);
			}
		}
	}

	// (light upload deferred until after shadow index assignment)

	// --- Build drawable list ---
	struct Drawable
	{
		virasa::ecs::Entity entity;
		uint32_t meshId;
		uint32_t materialId;
		virasa::AlphaMode alphaMode;
		virasa::MaterialModel materialModel;
		bool doubleSided;
		float eyeDistance;	  // used for blend bucket sorting
		virasa::math::Mat4 model; // cached world matrix for push constants
	};

	std::vector<Drawable> opaqueDrawables;
	std::vector<Drawable> blendDrawables;

	{
		const virasa::ecs::TransformSystem& transforms = world.GetTransforms();
		const virasa::ecs::ComponentSystem* visualSys = FindSystem(world, "Visual");
		const virasa::ecs::ComponentSystem* meshSys = FindSystem(world, "Mesh");

		if (visualSys != nullptr && meshSys != nullptr)
		{
			for (auto entity : visualSys->Entities())
			{
				if (!meshSys->Has(entity))
					continue;
				const auto* meshCompPtr = static_cast<const virasa::ecs::MeshComponent*>(
					meshSys->GetRaw(entity));
				if (meshCompPtr == nullptr)
					continue;
				if (!_meshRegistry.IsAllocated(meshCompPtr->meshId))
					continue;
				if (!transforms.Has(entity))
					continue;

				const auto* visualCompPtr =
					static_cast<const virasa::ecs::VisualComponent*>(
						visualSys->GetRaw(entity));
				if (visualCompPtr == nullptr)
					continue;
				if (!_materialTable.IsAllocated(visualCompPtr->materialId))
					continue;

				const virasa::VisualMaterialRasterState rasterState =
					_materialTable.GetRasterState(visualCompPtr->materialId);

				Drawable d{};
				d.entity = entity;
				d.meshId = meshCompPtr->meshId;
				d.materialId = visualCompPtr->materialId;
				d.alphaMode = rasterState.alphaMode;
				d.materialModel = rasterState.materialModel;
				d.doubleSided = rasterState.doubleSided;
				d.model = transforms.GetWorld(entity);

				if (rasterState.alphaMode == virasa::AlphaMode::Blend)
				{
					virasa::math::Vec3 worldPos = transforms.GetWorldPosition(entity);
					virasa::math::Vec3 diff = worldPos - cameraEye;
					d.eyeDistance =
						glm::dot(diff, diff); // squared distance, sufficient for sort
					blendDrawables.push_back(d);
				}
				else
				{
					d.eyeDistance = 0.0f;
					opaqueDrawables.push_back(d);
				}
			}
		}
	}

	// Sort blend bucket back-to-front (decreasing distance)
	std::sort(blendDrawables.begin(),
		blendDrawables.end(),
		[](const Drawable& a, const Drawable& b) { return a.eyeDistance > b.eyeDistance; });

	// Concatenate: opaque first, then blend
	std::vector<Drawable> allDrawables;
	allDrawables.reserve(opaqueDrawables.size() + blendDrawables.size());
	for (auto& d : opaqueDrawables)
		allDrawables.push_back(d);
	for (auto& d : blendDrawables)
		allDrawables.push_back(d);

	// --- Shadow rendering ---
	// Select shadow-casting lights and render depth maps
	std::vector<virasa::ShadowGPU> shadowRecords;
	// Shadow-map images the forward pass samples; declared as reads on the
	// forward pass below so the graph transitions each from depth-attachment
	// to shader-read-only and barriers the depth write before the sample.
	std::vector<virasa::renderer::graph::ImageHandle> shadowReadHandles;

	{
		auto* logger = virasa::Logger::GetLogger("renderer");
		(void)logger;

		const virasa::ecs::TransformSystem& transforms = world.GetTransforms();

		// We need to find directional and spot lights that cast shadows
		// Walk the lights vector we already built and match back to components
		// We need to re-walk the component systems to get castsShadows flag
		const virasa::ecs::ComponentSystem* dlSys = FindSystem(world, "DirectionalLight");
		const virasa::ecs::ComponentSystem* slSys = FindSystem(world, "SpotLight");

		// Build a list of shadow-casting light candidates in the same order lights[] was built
		// We need to correlate lights[] entries back to their source components.
		// We rebuild the shadow candidate list by re-walking the same component systems
		// in the same order used to build lights[].
		struct ShadowCandidate
		{
			virasa::math::Mat4 lightViewProj;
			size_t lightIndex; // index into lights[]
		};

		uint32_t shadowBudgetUsed = 0u;
		size_t lightIdx = 0u;

		// Process directional lights first (same order as light gathering above)
		if (dlSys != nullptr)
		{
			for (auto entity : dlSys->Entities())
			{
				const auto* dlPtr =
					static_cast<const virasa::ecs::DirectionalLightComponent*>(
						dlSys->GetRaw(entity));
				if (dlPtr == nullptr)
					continue;
				const auto& dl = *dlPtr;

				if (dl.castsShadows)
				{
					if (shadowBudgetUsed >= kMaxShadowMaps)
					{
						// Log warning: shadow budget exceeded
						(void)logger;
					}
					else
					{
						// Compute light-space view-projection for directional light
						virasa::math::Vec3 dir = glm::normalize(dl.direction);
						virasa::math::Vec3 eye =
							cameraEye - dir * kDirectionalShadowDistance;
						virasa::math::Mat4 lightView =
							virasa::math::LookAtRH_ZUp(eye, cameraEye);
						const float H = kDirectionalShadowExtent;
						virasa::math::Mat4 lightProj = virasa::math::OrthoRH_ZO(-H,
							H,
							-H,
							H,
							kShadowNearPlane,
							2.0f * kDirectionalShadowDistance);
						virasa::math::Mat4 lightViewProj = lightProj * lightView;

						// Declare transient shadow map
						virasa::renderer::graph::GraphImageDesc shadowDesc{};
						shadowDesc.width = kShadowMapResolution;
						shadowDesc.height = kShadowMapResolution;
						shadowDesc.format = VK_FORMAT_D32_SFLOAT;
						shadowDesc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
									 VK_IMAGE_USAGE_SAMPLED_BIT;
						shadowDesc.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
						virasa::renderer::graph::ImageHandle shadowMapHandle =
							_graph.DeclareImage(shadowDesc);

						// Capture for shadow pass record callback
						virasa::math::Mat4 capturedLVP = lightViewProj;
						virasa::Pipeline* shadowPipeline = &_shadowPipeline;
						virasa::renderer::MeshRegistry* meshRegPtr = &_meshRegistry;
						const virasa::Device* devPtr = _device;
						std::vector<Drawable> opaqueForShadow;
						for (const auto& d : opaqueDrawables)
							opaqueForShadow.push_back(d);

						_graph.AddPass("shadow")
							.DepthAttachment(shadowMapHandle,
								virasa::renderer::graph::LoadOp::Clear,
								virasa::renderer::graph::StoreOp::Store,
								1.0f)
							.Record(
								[shadowPipeline,
									meshRegPtr,
									devPtr,
									capturedLVP,
									opaqueForShadow = std::move(opaqueForShadow)](
									const virasa::renderer::graph::GraphContext&
										gc)
								{
									VkCommandBuffer cmd = gc.GetCommandBuffer();

									VkViewport vp{};
									vp.x = 0.0f;
									vp.y = 0.0f;
									vp.width = static_cast<float>(
										kShadowMapResolution);
									vp.height = static_cast<float>(
										kShadowMapResolution);
									vp.minDepth = 0.0f;
									vp.maxDepth = 1.0f;
									vkCmdSetViewport(cmd, 0, 1, &vp);

									VkRect2D sc{};
									sc.offset = {0, 0};
									sc.extent = {kShadowMapResolution,
										kShadowMapResolution};
									vkCmdSetScissor(cmd, 0, 1, &sc);

									shadowPipeline->Bind(cmd);

									for (const auto& d : opaqueForShadow)
									{
										if (!meshRegPtr->IsAllocated(d.meshId))
											continue;
										const auto& mesh =
											meshRegPtr->Get(d.meshId);

										ShadowPushConstants spc{};
										spc.lightMvp = capturedLVP * d.model;
										spc.vertexBufferAddress =
											mesh.GetVertexBufferAddress(
												*devPtr);
										spc.indexBufferAddress =
											mesh.GetIndexBufferAddress(
												*devPtr);

										vkCmdPushConstants(cmd,
											shadowPipeline->GetLayout(),
											VK_SHADER_STAGE_VERTEX_BIT,
											0,
											sizeof(ShadowPushConstants),
											&spc);

										vkCmdDraw(cmd,
											mesh.GetIndexCount(),
											1,
											0,
											0);
									}
								});

						// Register shadow map view into bindless shadow-map slot (with
						// caching)
						VkImageView shadowView =
							_imageRegistry.Get(shadowMapHandle).GetView();
						uint32_t shadowSlot = kInvalidSlot;
						auto shadowIt = _shadowSlotCache.find(shadowView);
						if (shadowIt != _shadowSlotCache.end())
						{
							shadowSlot = shadowIt->second;
						}
						else
						{
							shadowSlot = _bindlessTextures.RegisterShadowMap(
								shadowView, _shadowSampler.GetHandle());
							if (shadowSlot != kInvalidSlot)
							{
								_shadowSlotCache[shadowView] = shadowSlot;
							}
						}

						if (shadowSlot != kInvalidSlot)
						{
							// Assign shadow index to the light
							const uint32_t shadowRecordIdx =
								static_cast<uint32_t>(shadowRecords.size());
							lights[lightIdx].shadowIndex =
								static_cast<int32_t>(shadowRecordIdx);

							virasa::ShadowGPU shadowRec{};
							shadowRec.lightViewProj = lightViewProj;
							shadowRec.shadowMapSlot = shadowSlot;
							shadowRec.depthBias = kShadowDepthBias;
							shadowRec.slopeBias = kShadowSlopeBias;
							shadowRec._pad0 = 0u;
							shadowRecords.push_back(shadowRec);

							// Forward pass samples this map; record it so the
							// graph transitions + barriers it before the read.
							shadowReadHandles.push_back(shadowMapHandle);
						}

						++shadowBudgetUsed;
					}
				}

				++lightIdx;
			}
		}

		// Process spot lights (same order as light gathering above)
		if (slSys != nullptr)
		{
			for (auto entity : slSys->Entities())
			{
				if (!transforms.Has(entity))
					continue;
				const auto* slPtr = static_cast<const virasa::ecs::SpotLightComponent*>(
					slSys->GetRaw(entity));
				if (slPtr == nullptr)
					continue;
				const auto& sl = *slPtr;

				if (sl.castsShadows)
				{
					if (shadowBudgetUsed >= kMaxShadowMaps)
					{
						// Log warning: shadow budget exceeded
						(void)logger;
					}
					else
					{
						virasa::math::Vec3 pos = transforms.GetWorldPosition(entity);
						virasa::math::Vec3 dir = transforms.GetWorldForward(entity);
						virasa::math::Mat4 lightView =
							virasa::math::LookAtRH_ZUp(pos, pos + dir);
						const float outerCos = glm::clamp(sl.outerConeCos, -1.0f, 1.0f);
						const float fovY = 2.0f * std::acos(outerCos);
						virasa::math::Mat4 lightProj = virasa::math::PerspectiveRH_ZO(
							fovY, 1.0f, kShadowNearPlane, sl.range);
						virasa::math::Mat4 lightViewProj = lightProj * lightView;

						// Declare transient shadow map
						virasa::renderer::graph::GraphImageDesc shadowDesc{};
						shadowDesc.width = kShadowMapResolution;
						shadowDesc.height = kShadowMapResolution;
						shadowDesc.format = VK_FORMAT_D32_SFLOAT;
						shadowDesc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
									 VK_IMAGE_USAGE_SAMPLED_BIT;
						shadowDesc.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
						virasa::renderer::graph::ImageHandle shadowMapHandle =
							_graph.DeclareImage(shadowDesc);

						// Capture for shadow pass record callback
						virasa::math::Mat4 capturedLVP = lightViewProj;
						virasa::Pipeline* shadowPipeline = &_shadowPipeline;
						virasa::renderer::MeshRegistry* meshRegPtr = &_meshRegistry;
						const virasa::Device* devPtr = _device;
						std::vector<Drawable> opaqueForShadow;
						for (const auto& d : opaqueDrawables)
							opaqueForShadow.push_back(d);

						_graph.AddPass("shadow")
							.DepthAttachment(shadowMapHandle,
								virasa::renderer::graph::LoadOp::Clear,
								virasa::renderer::graph::StoreOp::Store,
								1.0f)
							.Record(
								[shadowPipeline,
									meshRegPtr,
									devPtr,
									capturedLVP,
									opaqueForShadow = std::move(opaqueForShadow)](
									const virasa::renderer::graph::GraphContext&
										gc)
								{
									VkCommandBuffer cmd = gc.GetCommandBuffer();

									VkViewport vp{};
									vp.x = 0.0f;
									vp.y = 0.0f;
									vp.width = static_cast<float>(
										kShadowMapResolution);
									vp.height = static_cast<float>(
										kShadowMapResolution);
									vp.minDepth = 0.0f;
									vp.maxDepth = 1.0f;
									vkCmdSetViewport(cmd, 0, 1, &vp);

									VkRect2D sc{};
									sc.offset = {0, 0};
									sc.extent = {kShadowMapResolution,
										kShadowMapResolution};
									vkCmdSetScissor(cmd, 0, 1, &sc);

									shadowPipeline->Bind(cmd);

									for (const auto& d : opaqueForShadow)
									{
										if (!meshRegPtr->IsAllocated(d.meshId))
											continue;
										const auto& mesh =
											meshRegPtr->Get(d.meshId);

										ShadowPushConstants spc{};
										spc.lightMvp = capturedLVP * d.model;
										spc.vertexBufferAddress =
											mesh.GetVertexBufferAddress(
												*devPtr);
										spc.indexBufferAddress =
											mesh.GetIndexBufferAddress(
												*devPtr);

										vkCmdPushConstants(cmd,
											shadowPipeline->GetLayout(),
											VK_SHADER_STAGE_VERTEX_BIT,
											0,
											sizeof(ShadowPushConstants),
											&spc);

										vkCmdDraw(cmd,
											mesh.GetIndexCount(),
											1,
											0,
											0);
									}
								});

						// Register shadow map view into bindless shadow-map slot (with
						// caching)
						VkImageView shadowView =
							_imageRegistry.Get(shadowMapHandle).GetView();
						uint32_t shadowSlot = kInvalidSlot;
						auto shadowIt = _shadowSlotCache.find(shadowView);
						if (shadowIt != _shadowSlotCache.end())
						{
							shadowSlot = shadowIt->second;
						}
						else
						{
							shadowSlot = _bindlessTextures.RegisterShadowMap(
								shadowView, _shadowSampler.GetHandle());
							if (shadowSlot != kInvalidSlot)
							{
								_shadowSlotCache[shadowView] = shadowSlot;
							}
						}

						if (shadowSlot != kInvalidSlot)
						{
							const uint32_t shadowRecordIdx =
								static_cast<uint32_t>(shadowRecords.size());
							lights[lightIdx].shadowIndex =
								static_cast<int32_t>(shadowRecordIdx);

							virasa::ShadowGPU shadowRec{};
							shadowRec.lightViewProj = lightViewProj;
							shadowRec.shadowMapSlot = shadowSlot;
							shadowRec.depthBias = kShadowDepthBias;
							shadowRec.slopeBias = kShadowSlopeBias;
							shadowRec._pad0 = 0u;
							shadowRecords.push_back(shadowRec);

							// Forward pass samples this map; record it so the
							// graph transitions + barriers it before the read.
							shadowReadHandles.push_back(shadowMapHandle);
						}

						++shadowBudgetUsed;
					}
				}

				++lightIdx;
			}
		}
	}

	// Upload shadow and light tables (shadow indices now assigned)
	_shadowTable.UploadFrame(std::span<const virasa::ShadowGPU>(shadowRecords));
	_lightTable.UploadFrame(std::span<const virasa::LightGPU>(lights));

	const virasa::ecs::ComponentSystem* highlightSystem = FindSystem(world, "Highlight");

	// --- Capture state for the record callback ---
	virasa::math::Mat4 capturedView = viewMatrix;
	virasa::math::Mat4 capturedProj = projMatrix;

	// Capture pointers to owned resources for the lambda
	virasa::Pipeline* opaquePipeline = &_opaquePipeline;
	virasa::Pipeline* terrainPipeline = &_terrainPipeline;
	virasa::Pipeline* opaqueDoubleSidedPipeline = &_opaqueDoubleSidedPipeline;
	virasa::Pipeline* blendPipeline = &_blendPipeline;
	virasa::Pipeline* blendDoubleSidedPipeline = &_blendDoubleSidedPipeline;
	virasa::BindlessTextureArray* bindless = &_bindlessTextures;
	virasa::renderer::MeshRegistry* meshReg = &_meshRegistry;
	virasa::VisualMaterialTable* matTable = &_materialTable;
	virasa::LightTable* lightTable = &_lightTable;
	virasa::ShadowTable* shadowTable = &_shadowTable;
	const virasa::Device* dev = _device;
	const virasa::ecs::ComponentSystem* capturedHighlightSystem = highlightSystem;

	// --- Add forward pass ---
	auto& forwardPass = _graph.AddPass("forward");
	forwardPass
		.ColorAttachment(_frameSceneColorHandle,
			virasa::renderer::graph::LoadOp::Clear,
			virasa::renderer::graph::ClearColor{0.1f, 0.1f, 0.15f, 1.0f})
		.DepthAttachment(_frameSceneDepthHandle, virasa::renderer::graph::LoadOp::Clear, 1.0f);

	// Declare every shadow map this pass samples as a read, so the graph
	// transitions it from depth-attachment to shader-read-only and inserts the
	// shadow-write -> forward-read barrier (required by the render_world
	// contract; its omission caused stale/black shadow reads under fast motion).
	for (const auto& shadowReadHandle : shadowReadHandles)
	{
		forwardPass.Read(
			shadowReadHandle, virasa::renderer::graph::ResourceUsage::SampledFragment);
	}

	forwardPass.Record(
		[opaquePipeline,
			terrainPipeline,
			opaqueDoubleSidedPipeline,
			blendPipeline,
			blendDoubleSidedPipeline,
			bindless,
			meshReg,
			matTable,
			lightTable,
			shadowTable,
			dev,
			capturedView,
			capturedProj,
			capturedHighlightSystem,
			allDrawables = std::move(allDrawables)](
			const virasa::renderer::graph::GraphContext& gc)
		{
			VkCommandBuffer cmd = gc.GetCommandBuffer();
			VkExtent2D extent = gc.GetRenderExtent();

			VkViewport viewport{};
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width = static_cast<float>(extent.width);
			viewport.height = static_cast<float>(extent.height);
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			vkCmdSetViewport(cmd, 0, 1, &viewport);

			VkRect2D scissor{};
			scissor.offset = {0, 0};
			scissor.extent = extent;
			vkCmdSetScissor(cmd, 0, 1, &scissor);

			// Bind descriptor set once
			VkDescriptorSet descSet = bindless->GetDescriptorSet();

			virasa::Pipeline* lastBoundPipeline = nullptr;

			for (const auto& d : allDrawables)
			{
				if (!meshReg->IsAllocated(d.meshId))
					continue;

				const auto& mesh = meshReg->Get(d.meshId);

				// Select pipeline
				virasa::Pipeline* selectedPipeline = nullptr;
				if (d.materialModel == virasa::MaterialModel::TerrainHeight)
				{
					selectedPipeline = terrainPipeline;
				}
				else if (d.alphaMode == virasa::AlphaMode::Blend)
				{
					selectedPipeline =
						d.doubleSided ? blendDoubleSidedPipeline : blendPipeline;
				}
				else
				{
					selectedPipeline =
						d.doubleSided ? opaqueDoubleSidedPipeline : opaquePipeline;
				}

				if (selectedPipeline != lastBoundPipeline)
				{
					selectedPipeline->Bind(cmd);
					vkCmdBindDescriptorSets(cmd,
						VK_PIPELINE_BIND_POINT_GRAPHICS,
						selectedPipeline->GetLayout(),
						0,
						1,
						&descSet,
						0,
						nullptr);
					lastBoundPipeline = selectedPipeline;
				}

				PushConstants pc{};
				pc.vertexBufferAddress = mesh.GetVertexBufferAddress(*dev);
				pc.indexBufferAddress = mesh.GetIndexBufferAddress(*dev);
				pc.materialBufferAddress = matTable->GetBufferAddress();
				pc.lightBufferAddress = lightTable->GetBufferAddress();
				pc.shadowBufferAddress = shadowTable->GetBufferAddress();
				pc.materialId = d.materialId;
				pc.lightCount = lightTable->GetLightCount();
				pc.model = d.model;
				pc.mvp = capturedProj * capturedView * d.model;
				pc.highlight = virasa::math::Vec4(0.0f, 0.0f, 0.0f, 0.0f);
				if (capturedHighlightSystem != nullptr &&
					capturedHighlightSystem->Has(d.entity))
				{
					const auto* highlight =
						static_cast<const virasa::ecs::HighlightComponent*>(
							capturedHighlightSystem->GetRaw(d.entity));
					if (highlight != nullptr)
					{
						pc.highlight = virasa::math::Vec4(highlight->color.x,
							highlight->color.y,
							highlight->color.z,
							highlight->intensity);
					}
				}

				vkCmdPushConstants(cmd,
					selectedPipeline->GetLayout(),
					VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
					0,
					sizeof(PushConstants),
					&pc);

				vkCmdDraw(cmd, mesh.GetIndexCount(), 1, 0, 0);
			}
		});

	// --- Outline-highlight subsystem ---
	// Collect highlighted drawables (highlight.w > 0)
	// We need to re-build the highlighted subset from allDrawables, but allDrawables was
	// moved into the forward pass lambda. We must collect highlighted drawables BEFORE
	// moving allDrawables into the lambda. We do this by rebuilding from the world.
	// NOTE: allDrawables was moved above; we need a separate highlighted list.
	// We rebuild the highlighted drawable list here from the same world query.
	std::vector<Drawable> highlightedDrawables;
	{
		const virasa::ecs::TransformSystem& transforms = world.GetTransforms();
		const virasa::ecs::ComponentSystem* visualSys2 = FindSystem(world, "Visual");
		const virasa::ecs::ComponentSystem* meshSys2 = FindSystem(world, "Mesh");

		if (highlightSystem != nullptr && visualSys2 != nullptr && meshSys2 != nullptr)
		{
			for (auto entity : visualSys2->Entities())
			{
				if (!meshSys2->Has(entity))
					continue;
				const auto* meshCompPtr = static_cast<const virasa::ecs::MeshComponent*>(
					meshSys2->GetRaw(entity));
				if (meshCompPtr == nullptr)
					continue;
				if (!_meshRegistry.IsAllocated(meshCompPtr->meshId))
					continue;
				if (!transforms.Has(entity))
					continue;
				if (!highlightSystem->Has(entity))
					continue;

				const auto* hlPtr = static_cast<const virasa::ecs::HighlightComponent*>(
					highlightSystem->GetRaw(entity));
				if (hlPtr == nullptr || hlPtr->intensity <= 0.0f)
					continue;

				const auto* visualCompPtr =
					static_cast<const virasa::ecs::VisualComponent*>(
						visualSys2->GetRaw(entity));
				if (visualCompPtr == nullptr)
					continue;
				if (!_materialTable.IsAllocated(visualCompPtr->materialId))
					continue;

				Drawable d{};
				d.entity = entity;
				d.meshId = meshCompPtr->meshId;
				d.materialId = visualCompPtr->materialId;
				d.model = transforms.GetWorld(entity);
				d.eyeDistance = 0.0f;
				const virasa::VisualMaterialRasterState rs =
					_materialTable.GetRasterState(visualCompPtr->materialId);
				d.alphaMode = rs.alphaMode;
				d.materialModel = rs.materialModel;
				d.doubleSided = rs.doubleSided;
				// Embed highlight vec4
				// (stored separately via the HighlightComponent pointer above)
				highlightedDrawables.push_back(d);
			}
		}
	}

	if (!highlightedDrawables.empty())
	{
		// Declare transient outline images
		virasa::renderer::graph::GraphImageDesc maskDesc{};
		maskDesc.width = _frameSceneWidth;
		maskDesc.height = _frameSceneHeight;
		maskDesc.format = VK_FORMAT_R8G8B8A8_UNORM;
		maskDesc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
				     VK_IMAGE_USAGE_SAMPLED_BIT;
		maskDesc.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		virasa::renderer::graph::ImageHandle maskHandle = _graph.DeclareImage(maskDesc);

		virasa::renderer::graph::GraphImageDesc jfaDesc{};
		jfaDesc.width = _frameSceneWidth;
		jfaDesc.height = _frameSceneHeight;
		jfaDesc.format = VK_FORMAT_R32G32_SFLOAT;
		jfaDesc.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		jfaDesc.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		virasa::renderer::graph::ImageHandle jfaAHandle = _graph.DeclareImage(jfaDesc);
		virasa::renderer::graph::ImageHandle jfaBHandle = _graph.DeclareImage(jfaDesc);

		// --- Mask pass ---
		{
			virasa::Pipeline* maskPipeline = &_maskPipeline;
			virasa::BindlessTextureArray* bindless2 = &_bindlessTextures;
			virasa::renderer::MeshRegistry* meshReg2 = &_meshRegistry;
			const virasa::Device* dev2 = _device;
			const virasa::ecs::ComponentSystem* hlSys2 = highlightSystem;
			virasa::math::Mat4 capturedView2 = viewMatrix;
			virasa::math::Mat4 capturedProj2 = projMatrix;

			_graph.AddPass("outline_mask")
				.ColorAttachment(maskHandle,
					virasa::renderer::graph::LoadOp::Clear,
					virasa::renderer::graph::ClearColor{0.0f, 0.0f, 0.0f, 0.0f})
				.Record(
					[maskPipeline,
						bindless2,
						meshReg2,
						dev2,
						hlSys2,
						capturedView2,
						capturedProj2,
						highlightedDrawables](
						const virasa::renderer::graph::GraphContext& gc)
					{
						VkCommandBuffer cmd = gc.GetCommandBuffer();
						VkExtent2D ext = gc.GetRenderExtent();

						VkViewport vp{};
						vp.x = 0.0f;
						vp.y = 0.0f;
						vp.width = static_cast<float>(ext.width);
						vp.height = static_cast<float>(ext.height);
						vp.minDepth = 0.0f;
						vp.maxDepth = 1.0f;
						vkCmdSetViewport(cmd, 0, 1, &vp);

						VkRect2D sc{};
						sc.offset = {0, 0};
						sc.extent = ext;
						vkCmdSetScissor(cmd, 0, 1, &sc);

						VkDescriptorSet descSet = bindless2->GetDescriptorSet();
						maskPipeline->Bind(cmd);
						vkCmdBindDescriptorSets(cmd,
							VK_PIPELINE_BIND_POINT_GRAPHICS,
							maskPipeline->GetLayout(),
							0,
							1,
							&descSet,
							0,
							nullptr);

						for (const auto& d : highlightedDrawables)
						{
							if (!meshReg2->IsAllocated(d.meshId))
								continue;
							const auto& mesh = meshReg2->Get(d.meshId);

							virasa::math::Vec4 highlight(0.0f);
							if (hlSys2 != nullptr && hlSys2->Has(d.entity))
							{
								const auto* hlPtr = static_cast<
									const virasa::ecs::HighlightComponent*>(
									hlSys2->GetRaw(d.entity));
								if (hlPtr != nullptr)
								{
									highlight = virasa::math::Vec4(hlPtr->color.x,
										hlPtr->color.y,
										hlPtr->color.z,
										hlPtr->intensity);
								}
							}

							PushConstants pc{};
							pc.mvp = capturedProj2 * capturedView2 * d.model;
							pc.model = d.model;
							pc.vertexBufferAddress =
								mesh.GetVertexBufferAddress(*dev2);
							pc.indexBufferAddress = mesh.GetIndexBufferAddress(*dev2);
							pc.materialBufferAddress = 0;
							pc.lightBufferAddress = 0;
							pc.shadowBufferAddress = 0;
							pc.materialId = d.materialId;
							pc.lightCount = 0;
							pc.highlight = highlight;

							vkCmdPushConstants(cmd,
								maskPipeline->GetLayout(),
								VK_SHADER_STAGE_VERTEX_BIT |
									VK_SHADER_STAGE_FRAGMENT_BIT,
								0,
								sizeof(PushConstants),
								&pc);

							vkCmdDraw(cmd, mesh.GetIndexCount(), 1, 0, 0);
						}
					});
		}

		// --- Register mask and jfaA storage slots ---
		VkImageView maskView = _imageRegistry.Get(maskHandle).GetView();
		uint32_t maskStorageSlot = kInvalidSlot;
		{
			auto it2 = _storageSlotCache.find(maskView);
			if (it2 != _storageSlotCache.end())
			{
				maskStorageSlot = it2->second;
			}
			else
			{
				maskStorageSlot = _bindlessTextures.RegisterStorageImage(maskView);
				if (maskStorageSlot != kInvalidSlot)
					_storageSlotCache[maskView] = maskStorageSlot;
			}
		}

		VkImageView jfaAView = _imageRegistry.Get(jfaAHandle).GetView();
		uint32_t jfaAStorageSlot = kInvalidSlot;
		{
			auto it2 = _storageSlotCache.find(jfaAView);
			if (it2 != _storageSlotCache.end())
			{
				jfaAStorageSlot = it2->second;
			}
			else
			{
				jfaAStorageSlot = _bindlessTextures.RegisterStorageImage(jfaAView);
				if (jfaAStorageSlot != kInvalidSlot)
					_storageSlotCache[jfaAView] = jfaAStorageSlot;
			}
		}

		VkImageView jfaBView = _imageRegistry.Get(jfaBHandle).GetView();
		uint32_t jfaBStorageSlot = kInvalidSlot;
		{
			auto it2 = _storageSlotCache.find(jfaBView);
			if (it2 != _storageSlotCache.end())
			{
				jfaBStorageSlot = it2->second;
			}
			else
			{
				jfaBStorageSlot = _bindlessTextures.RegisterStorageImage(jfaBView);
				if (jfaBStorageSlot != kInvalidSlot)
					_storageSlotCache[jfaBView] = jfaBStorageSlot;
			}
		}

		// --- JFA seed pass ---
		{
			virasa::Pipeline* jfaSeedPipeline = &_jfaSeedPipeline;
			virasa::BindlessTextureArray* bindless2 = &_bindlessTextures;
			const uint32_t w = _frameSceneWidth;
			const uint32_t h = _frameSceneHeight;
			const uint32_t capturedMaskSlot = maskStorageSlot;
			const uint32_t capturedDstSlot = jfaAStorageSlot;

			_graph.AddPass("outline_jfa_seed")
				.Read(maskHandle,
					virasa::renderer::graph::ResourceUsage::StorageReadCompute)
				.Write(jfaAHandle,
					virasa::renderer::graph::ResourceUsage::StorageWriteCompute)
				.Record(
					[jfaSeedPipeline, bindless2, w, h, capturedMaskSlot, capturedDstSlot](
						const virasa::renderer::graph::GraphContext& gc)
					{
						VkCommandBuffer cmd = gc.GetCommandBuffer();
						VkDescriptorSet descSet = bindless2->GetDescriptorSet();
						jfaSeedPipeline->BindCompute(cmd);
						vkCmdBindDescriptorSets(cmd,
							VK_PIPELINE_BIND_POINT_COMPUTE,
							jfaSeedPipeline->GetLayout(),
							0,
							1,
							&descSet,
							0,
							nullptr);

						OutlineComputePushConstants opc{};
						opc.extentX = w;
						opc.extentY = h;
						opc.srcSlot = 0;
						opc.dstSlot = capturedDstSlot;
						opc.maskSlot = capturedMaskSlot;
						opc.stepSize = 0;
						opc._pad0 = 0;
						opc._pad1 = 0;

						vkCmdPushConstants(cmd,
							jfaSeedPipeline->GetLayout(),
							VK_SHADER_STAGE_COMPUTE_BIT,
							0,
							sizeof(OutlineComputePushConstants),
							&opc);

						const uint32_t gx = (w + 7u) / 8u;
						const uint32_t gy = (h + 7u) / 8u;
						vkCmdDispatch(cmd, gx, gy, 1);
					});
		}

		// --- JFA step passes ---
		const uint32_t maxDim = std::max(_frameSceneWidth, _frameSceneHeight);
		const uint32_t stepCount =
			(maxDim > 1u)
				? static_cast<uint32_t>(std::ceil(std::log2(static_cast<float>(maxDim))))
				: 1u;

		virasa::renderer::graph::ImageHandle jfaSrc = jfaAHandle;
		virasa::renderer::graph::ImageHandle jfaDst = jfaBHandle;
		uint32_t srcStorageSlot = jfaAStorageSlot;
		uint32_t dstStorageSlot = jfaBStorageSlot;

		for (uint32_t step = 0u; step < stepCount; ++step)
		{
			const int32_t jumpDist = static_cast<int32_t>(1u << (stepCount - 1u - step));

			// Ensure dst storage slot is registered
			VkImageView dstView = _imageRegistry.Get(jfaDst).GetView();
			{
				auto it2 = _storageSlotCache.find(dstView);
				if (it2 != _storageSlotCache.end())
				{
					dstStorageSlot = it2->second;
				}
				else
				{
					dstStorageSlot = _bindlessTextures.RegisterStorageImage(dstView);
					if (dstStorageSlot != kInvalidSlot)
						_storageSlotCache[dstView] = dstStorageSlot;
				}
			}

			virasa::Pipeline* jfaStepPipeline = &_jfaStepPipeline;
			virasa::BindlessTextureArray* bindless2 = &_bindlessTextures;
			const uint32_t w = _frameSceneWidth;
			const uint32_t h = _frameSceneHeight;
			const uint32_t capturedSrc = srcStorageSlot;
			const uint32_t capturedDst = dstStorageSlot;
			const int32_t capturedStep = jumpDist;

			_graph.AddPass("outline_jfa_step")
				.Read(jfaSrc, virasa::renderer::graph::ResourceUsage::StorageReadCompute)
				.Write(jfaDst, virasa::renderer::graph::ResourceUsage::StorageWriteCompute)
				.Record(
					[jfaStepPipeline,
						bindless2,
						w,
						h,
						capturedSrc,
						capturedDst,
						capturedStep](const virasa::renderer::graph::GraphContext& gc)
					{
						VkCommandBuffer cmd = gc.GetCommandBuffer();
						VkDescriptorSet descSet = bindless2->GetDescriptorSet();
						jfaStepPipeline->BindCompute(cmd);
						vkCmdBindDescriptorSets(cmd,
							VK_PIPELINE_BIND_POINT_COMPUTE,
							jfaStepPipeline->GetLayout(),
							0,
							1,
							&descSet,
							0,
							nullptr);

						OutlineComputePushConstants opc{};
						opc.extentX = w;
						opc.extentY = h;
						opc.srcSlot = capturedSrc;
						opc.dstSlot = capturedDst;
						opc.maskSlot = 0;
						opc.stepSize = capturedStep;
						opc._pad0 = 0;
						opc._pad1 = 0;

						vkCmdPushConstants(cmd,
							jfaStepPipeline->GetLayout(),
							VK_SHADER_STAGE_COMPUTE_BIT,
							0,
							sizeof(OutlineComputePushConstants),
							&opc);

						const uint32_t gx = (w + 7u) / 8u;
						const uint32_t gy = (h + 7u) / 8u;
						vkCmdDispatch(cmd, gx, gy, 1);
					});

			// Ping-pong
			std::swap(jfaSrc, jfaDst);
			std::swap(srcStorageSlot, dstStorageSlot);
		}

		// jfaSrc now holds the final JFA result handle
		virasa::renderer::graph::ImageHandle jfaFinalHandle = jfaSrc;

		// --- Register jfaFinal and mask into binding-0 texture slots (outlineSlotCache) ---
		VkImageView jfaFinalView = _imageRegistry.Get(jfaFinalHandle).GetView();
		uint32_t jfaFinalTextureSlot = kInvalidSlot;
		{
			auto it2 = _outlineSlotCache.find(jfaFinalView);
			if (it2 != _outlineSlotCache.end())
			{
				jfaFinalTextureSlot = it2->second;
			}
			else
			{
				jfaFinalTextureSlot = _bindlessTextures.RegisterTexture(
					jfaFinalView, _outlineSampler.GetHandle());
				if (jfaFinalTextureSlot != kInvalidSlot)
					_outlineSlotCache[jfaFinalView] = jfaFinalTextureSlot;
			}
		}

		uint32_t maskTextureSlot = kInvalidSlot;
		{
			auto it2 = _outlineSlotCache.find(maskView);
			if (it2 != _outlineSlotCache.end())
			{
				maskTextureSlot = it2->second;
			}
			else
			{
				maskTextureSlot = _bindlessTextures.RegisterTexture(
					maskView, _outlineSampler.GetHandle());
				if (maskTextureSlot != kInvalidSlot)
					_outlineSlotCache[maskView] = maskTextureSlot;
			}
		}

		// --- Composite pass ---
		{
			virasa::Pipeline* compositePipeline = &_outlineCompositePipeline;
			virasa::BindlessTextureArray* bindless2 = &_bindlessTextures;
			const uint32_t w = _frameSceneWidth;
			const uint32_t h = _frameSceneHeight;
			const uint32_t capturedJfaSlot = jfaFinalTextureSlot;
			const uint32_t capturedMaskSlot2 = maskTextureSlot;

			_graph.AddPass("outline_composite")
				.ColorAttachment(_frameSceneColorHandle,
					virasa::renderer::graph::LoadOp::Load,
					virasa::renderer::graph::ClearColor{})
				.Read(jfaFinalHandle,
					virasa::renderer::graph::ResourceUsage::SampledFragment)
				.Read(maskHandle, virasa::renderer::graph::ResourceUsage::SampledFragment)
				.Record(
					[compositePipeline,
						bindless2,
						w,
						h,
						capturedJfaSlot,
						capturedMaskSlot2](
						const virasa::renderer::graph::GraphContext& gc)
					{
						VkCommandBuffer cmd = gc.GetCommandBuffer();
						VkExtent2D ext = gc.GetRenderExtent();

						VkViewport vp{};
						vp.x = 0.0f;
						vp.y = 0.0f;
						vp.width = static_cast<float>(ext.width);
						vp.height = static_cast<float>(ext.height);
						vp.minDepth = 0.0f;
						vp.maxDepth = 1.0f;
						vkCmdSetViewport(cmd, 0, 1, &vp);

						VkRect2D sc{};
						sc.offset = {0, 0};
						sc.extent = ext;
						vkCmdSetScissor(cmd, 0, 1, &sc);

						VkDescriptorSet descSet = bindless2->GetDescriptorSet();
						compositePipeline->Bind(cmd);
						vkCmdBindDescriptorSets(cmd,
							VK_PIPELINE_BIND_POINT_GRAPHICS,
							compositePipeline->GetLayout(),
							0,
							1,
							&descSet,
							0,
							nullptr);

						OutlineCompositePushConstants ocp{};
						ocp.extentX = w;
						ocp.extentY = h;
						ocp.jfaSlot = capturedJfaSlot;
						ocp.maskSlot = capturedMaskSlot2;
						ocp.widthPixels = kOutlineWidthPixels;
						ocp._pad0 = 0.0f;
						ocp._pad1 = 0.0f;
						ocp._pad2 = 0.0f;

						vkCmdPushConstants(cmd,
							compositePipeline->GetLayout(),
							VK_SHADER_STAGE_FRAGMENT_BIT,
							0,
							sizeof(OutlineCompositePushConstants),
							&ocp);

						vkCmdDraw(cmd, 3, 1, 0, 0);
					});
		}
	} // end if (!highlightedDrawables.empty())

	// --- Deferred-picking subsystem ---
	if (_pickRequested)
	{
		const uint32_t frameIndex = _context->GetCurrentFrameIndex();

		virasa::renderer::graph::GraphImageDesc idDesc{};
		idDesc.width = _frameSceneWidth;
		idDesc.height = _frameSceneHeight;
		idDesc.format = VK_FORMAT_R32G32_UINT;
		idDesc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		idDesc.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		virasa::renderer::graph::ImageHandle idTargetHandle = _graph.DeclareImage(idDesc);

		virasa::renderer::graph::GraphImageDesc pickDepthDesc{};
		pickDepthDesc.width = _frameSceneWidth;
		pickDepthDesc.height = _frameSceneHeight;
		pickDepthDesc.format = _context->GetDepthFormat();
		pickDepthDesc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		pickDepthDesc.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
		virasa::renderer::graph::ImageHandle pickDepthHandle =
			_graph.DeclareImage(pickDepthDesc);

		{
			virasa::Pipeline* pickClearPipeline = &_pickClearPipeline;
			_graph.AddPass("pick_clear")
				.ColorAttachment(idTargetHandle,
					virasa::renderer::graph::LoadOp::DontCare,
					virasa::renderer::graph::ClearColor{})
				.Record(
					[pickClearPipeline](const virasa::renderer::graph::GraphContext& gc)
					{
						VkCommandBuffer cmd = gc.GetCommandBuffer();
						VkExtent2D ext = gc.GetRenderExtent();

						VkViewport vp{};
						vp.x = 0.0f;
						vp.y = 0.0f;
						vp.width = static_cast<float>(ext.width);
						vp.height = static_cast<float>(ext.height);
						vp.minDepth = 0.0f;
						vp.maxDepth = 1.0f;
						vkCmdSetViewport(cmd, 0, 1, &vp);

						VkRect2D sc{};
						sc.offset = {0, 0};
						sc.extent = ext;
						vkCmdSetScissor(cmd, 0, 1, &sc);

						pickClearPipeline->Bind(cmd);
						vkCmdDraw(cmd, 3, 1, 0, 0);
					});
		}

		{
			virasa::Pipeline* pickIdPipeline = &_pickIdPipeline;
			virasa::renderer::MeshRegistry* meshReg2 = &_meshRegistry;
			const virasa::Device* dev2 = _device;
			virasa::math::Mat4 capturedView2 = viewMatrix;
			virasa::math::Mat4 capturedProj2 = projMatrix;

			std::vector<Drawable> pickDrawables;
			pickDrawables.reserve(opaqueDrawables.size() + blendDrawables.size());
			for (const auto& d : opaqueDrawables)
			{
				pickDrawables.push_back(d);
			}
			for (const auto& d : blendDrawables)
			{
				pickDrawables.push_back(d);
			}

			_graph.AddPass("pick_id")
				.ColorAttachment(idTargetHandle,
					virasa::renderer::graph::LoadOp::Load,
					virasa::renderer::graph::ClearColor{})
				.DepthAttachment(
					pickDepthHandle, virasa::renderer::graph::LoadOp::Clear, 1.0f)
				.Record(
					[pickIdPipeline,
						meshReg2,
						dev2,
						capturedView2,
						capturedProj2,
						pickDrawables = std::move(pickDrawables)](
						const virasa::renderer::graph::GraphContext& gc)
					{
						VkCommandBuffer cmd = gc.GetCommandBuffer();
						VkExtent2D ext = gc.GetRenderExtent();

						VkViewport vp{};
						vp.x = 0.0f;
						vp.y = 0.0f;
						vp.width = static_cast<float>(ext.width);
						vp.height = static_cast<float>(ext.height);
						vp.minDepth = 0.0f;
						vp.maxDepth = 1.0f;
						vkCmdSetViewport(cmd, 0, 1, &vp);

						VkRect2D sc{};
						sc.offset = {0, 0};
						sc.extent = ext;
						vkCmdSetScissor(cmd, 0, 1, &sc);

						pickIdPipeline->Bind(cmd);

						for (const auto& d : pickDrawables)
						{
							if (!meshReg2->IsAllocated(d.meshId))
							{
								continue;
							}
							const auto& mesh = meshReg2->Get(d.meshId);

							PickPushConstants ppc{};
							ppc.mvp = capturedProj2 * capturedView2 * d.model;
							ppc.vertexBufferAddress =
								mesh.GetVertexBufferAddress(*dev2);
							ppc.indexBufferAddress =
								mesh.GetIndexBufferAddress(*dev2);
							ppc.entityIndex = d.entity.index;
							ppc.entityGeneration = d.entity.generation;

							vkCmdPushConstants(cmd,
								pickIdPipeline->GetLayout(),
								VK_SHADER_STAGE_VERTEX_BIT |
									VK_SHADER_STAGE_FRAGMENT_BIT,
								0,
								sizeof(PickPushConstants),
								&ppc);

							vkCmdDraw(cmd, mesh.GetIndexCount(), 1, 0, 0);
						}
					});
		}

		{
			const uint32_t copyX =
				(_frameSceneWidth == 0u) ? 0u : std::min(_pickX, _frameSceneWidth - 1u);
			const uint32_t copyY =
				(_frameSceneHeight == 0u) ? 0u : std::min(_pickY, _frameSceneHeight - 1u);
			virasa::Buffer* readback = &_pickReadbackBuffers[frameIndex];

			_graph.AddPass("pick_copy")
				.Read(idTargetHandle, virasa::renderer::graph::ResourceUsage::TransferSrc)
				.Record(
					[idTargetHandle, copyX, copyY, readback](
						const virasa::renderer::graph::GraphContext& gc)
					{
						VkCommandBuffer cmd = gc.GetCommandBuffer();

						VkBufferImageCopy region{};
						region.bufferOffset = 0;
						region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
						region.imageSubresource.mipLevel = 0;
						region.imageSubresource.baseArrayLayer = 0;
						region.imageSubresource.layerCount = 1;
						region.imageOffset = {static_cast<int32_t>(copyX),
							static_cast<int32_t>(copyY),
							0};
						region.imageExtent = {1, 1, 1};

						vkCmdCopyImageToBuffer(cmd,
							gc.GetImage(idTargetHandle),
							VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
							readback->GetHandle(),
							1,
							&region);

						VkBufferMemoryBarrier2 barrier{};
						barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
						barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
						barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
						barrier.dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
						barrier.dstAccessMask = VK_ACCESS_2_HOST_READ_BIT;
						barrier.buffer = readback->GetHandle();
						barrier.offset = 0;
						barrier.size = VK_WHOLE_SIZE;

						VkDependencyInfo dep{};
						dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
						dep.bufferMemoryBarrierCount = 1;
						dep.pBufferMemoryBarriers = &barrier;
						vkCmdPipelineBarrier2(cmd, &dep);
					});
		}

		if (frameIndex < _pickPending.size())
		{
			_pickPending[frameIndex] = 1u;
		}
		_pickRequested = false;
	}

	// --- Register scene color in bindless (with caching) ---
	VkImageView sceneView = _imageRegistry.Get(_frameSceneColorHandle).GetView();

	auto it = _sceneSlotCache.find(sceneView);
	if (it != _sceneSlotCache.end())
	{
		_frameSceneSlot = it->second;
	}
	else
	{
		uint32_t slot =
			_bindlessTextures.RegisterTexture(sceneView, _defaultSampler.GetHandle());
		if (slot == 0xFFFFFFFFu)
		{
			return 0xFFFFFFFFu;
		}
		_sceneSlotCache[sceneView] = slot;
		_frameSceneSlot = slot;
	}

	return _frameSceneSlot;
}

// ---------------------------------------------------------------------------
// SubmitFrame
// ---------------------------------------------------------------------------
virasa::SwapchainStatus SceneRenderer::SubmitFrame(const virasa::ui::DrawList& drawList,
	const virasa::ui::FontAtlas& atlas, uint32_t windowWidth, uint32_t windowHeight)
{
	// Step 1: submit UI pass
	std::array<virasa::renderer::graph::ImageHandle, 1> sampledImages = {_frameSceneColorHandle};
	_uiPass.Submit(_graph,
		_frameSwapchainHandle,
		drawList,
		atlas,
		std::span<const virasa::renderer::graph::ImageHandle>(sampledImages),
		_bindlessTextures.GetDescriptorSet(),
		windowWidth,
		windowHeight,
		_context->GetCurrentFrameIndex());

	// Step 2: compile
	{
		auto err = _graph.Compile();
		if (err != virasa::RenderError::None)
		{
			_graph.End();
			return virasa::SwapchainStatus::Error;
		}
	}

	// Step 3: execute
	{
		auto err = _graph.Execute(_context->GetCurrentCommandBuffer());
		if (err != virasa::RenderError::None)
		{
			_graph.End();
			return virasa::SwapchainStatus::Error;
		}
	}

	// Step 4: end graph
	_graph.End();

	// Step 5: end frame
	auto* mutableContext = const_cast<virasa::Context*>(_context);
	return mutableContext->EndFrame();
}

// ---------------------------------------------------------------------------
// WaitIdle
// ---------------------------------------------------------------------------
void SceneRenderer::WaitIdle()
{
	_context->GetDevice().WaitIdle();
}

void SceneRenderer::RequestPick(uint32_t x, uint32_t y)
{
	_pickRequested = true;
	_pickX = x;
	_pickY = y;
}

virasa::PickResult SceneRenderer::GetPickResult()
{
	virasa::PickResult result = _pickResult;
	if (result.valid)
	{
		_pickResult.valid = false;
	}
	return result;
}

} // namespace virasa::renderer::scene
