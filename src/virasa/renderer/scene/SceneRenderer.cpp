#include "virasa/renderer/scene/SceneRenderer.h"

#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

#include "virasa/core/Logger.h"
#include "virasa/ecs/Components.h"
#include "virasa/math/Projection.h"
#include "virasa/math/Transform.h"
#include "virasa/renderer/geometry/Primitives.h"
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
	(void)message;
	auto* logger = virasa::Logger::GetLogger("renderer");
	(void)logger;
}

} // namespace

// ---------------------------------------------------------------------------
// Internal push-constant layout (168 bytes)
// ---------------------------------------------------------------------------
struct PushConstants
{
	virasa::math::Mat4 mvp;		  // 64
	virasa::math::Mat4 model;	  // 64
	uint64_t vertexBufferAddress;	  //  8
	uint64_t indexBufferAddress;	  //  8
	uint64_t materialBufferAddress; //  8
	uint64_t lightBufferAddress;	  //  8
	uint32_t materialId;		  //  4
	uint32_t lightCount;		  //  4
};
static_assert(sizeof(PushConstants) == 168, "PushConstants must be exactly 168 bytes");

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
		err = _lightTable.Initialize(device, 256);
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

	// Step 6: initialize default sampler
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
		virasa::VisualMaterial defaultMat{};
		defaultMat.factors.baseColorFactor = virasa::math::Vec4(0.9f, 0.3f, 0.2f, 1.0f);

		uint32_t id = _materialTable.Allocate(defaultMat);
		if (id == 0xFFFFFFFFu || id != 0u)
		{
			_initialized = false;
			return virasa::RenderError::PipelineCreateFailed;
		}
		assert(id == 0u);
	}

	// Step 9: load forward shaders
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
					VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 168)
				.AddDescriptorSetLayout(_bindlessTextures.GetLayout());
			return b;
		};

		// _opaquePipeline: back-face cull, depth write on, blend off
		{
			auto b = makeBase();
			b.SetCullMode(VK_CULL_MODE_BACK_BIT)
				.SetDepthWrite(true)
				.SetBlendEnabled(false);
			auto err = b.Build(device, _opaquePipeline);
			if (err != virasa::RenderError::None)
			{
				_initialized = false;
				return err;
			}
		}

		// _opaqueDoubleSidedPipeline: no cull, depth write on, blend off
		{
			auto b = makeBase();
			b.SetCullMode(VK_CULL_MODE_NONE)
				.SetDepthWrite(true)
				.SetBlendEnabled(false);
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
			b.SetCullMode(VK_CULL_MODE_BACK_BIT)
				.SetDepthWrite(false)
				.SetBlendEnabled(true);
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
			b.SetCullMode(VK_CULL_MODE_NONE)
				.SetDepthWrite(false)
				.SetBlendEnabled(true);
			auto err = b.Build(device, _blendDoubleSidedPipeline);
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
// CreateDefaultMaterial
// ---------------------------------------------------------------------------
uint32_t SceneRenderer::CreateDefaultMaterial()
{
	virasa::VisualMaterial mat{};
	mat.factors.baseColorFactor = virasa::math::Vec4(0.9f, 0.3f, 0.2f, 1.0f);

	uint32_t id = _materialTable.Allocate(mat);
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
		virasa::SamplerConfig samplerConfig{};
		samplerConfig.magFilter = upload.sampler.magFilter;
		samplerConfig.minFilter = upload.sampler.minFilter;
		samplerConfig.mipmapMode = upload.sampler.mipmapMode;
		samplerConfig.addressModeU = upload.sampler.addressModeU;
		samplerConfig.addressModeV = upload.sampler.addressModeV;
		samplerConfig.addressModeW = upload.sampler.addressModeW;
		samplerConfig.anisotropyEnable = upload.sampler.anisotropyEnable;
		samplerConfig.maxAnisotropy = upload.sampler.maxAnisotropy;

		renderError = sampler.Initialize(*_device, samplerConfig);
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
	// Const-correct system lookup: World::FindSystem is non-const, but RenderWorld
	// takes the world by const reference, so resolve systems through the const
	// GetSystemId/GetSystem pair (mirrors FindSystem's nullptr-on-miss contract).
	auto findSystem = [&world](std::string_view name) -> const virasa::ecs::ComponentSystem* {
		const virasa::ecs::ComponentId id = world.GetSystemId(name);
		if (id == 0xFFFFFFFFu)
			return nullptr;
		return &world.GetSystem(id);
	};

	// --- Derive camera matrices ---
	virasa::math::Mat4 viewMatrix(1.0f);
	virasa::math::Mat4 projMatrix(1.0f);
	virasa::math::Vec3 cameraEye(0.0f);

	{
		const virasa::ecs::TransformSystem& transforms = world.GetTransforms();
		// Retrieve camera component from the world's camera system
		// We access it via the World's component systems directly
		// The CameraComponent is stored in a system registered under "Camera"
		const virasa::ecs::ComponentSystem* camSys = findSystem("Camera");
		if (camSys != nullptr && camSys->Has(cameraEntity) && transforms.Has(cameraEntity))
		{
			const auto* camPtr =
				static_cast<const virasa::ecs::CameraComponent*>(camSys->GetRaw(cameraEntity));
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
		const virasa::ecs::ComponentSystem* dlSys = findSystem("DirectionalLight");
		if (dlSys != nullptr)
		{
			for (auto entity : dlSys->Entities())
			{
				const auto* dlPtr = static_cast<const virasa::ecs::DirectionalLightComponent*>(
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
		const virasa::ecs::ComponentSystem* plSys = findSystem("PointLight");
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
		const virasa::ecs::ComponentSystem* slSys = findSystem("SpotLight");
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

	_lightTable.UploadFrame(std::span<const virasa::LightGPU>(lights));

	// --- Build drawable list ---
	struct Drawable
	{
		virasa::ecs::Entity entity;
		uint32_t meshId;
		uint32_t materialId;
		virasa::AlphaMode alphaMode;
		bool doubleSided;
		float eyeDistance;		   // used for blend bucket sorting
		virasa::math::Mat4 model; // cached world matrix for push constants
	};

	std::vector<Drawable> opaqueDrawables;
	std::vector<Drawable> blendDrawables;

	{
		const virasa::ecs::TransformSystem& transforms = world.GetTransforms();
		const virasa::ecs::ComponentSystem* visualSys = findSystem("Visual");
		const virasa::ecs::ComponentSystem* meshSys = findSystem("Mesh");

		if (visualSys != nullptr && meshSys != nullptr)
		{
			for (auto entity : visualSys->Entities())
			{
				if (!meshSys->Has(entity))
					continue;
				const auto* meshCompPtr =
					static_cast<const virasa::ecs::MeshComponent*>(meshSys->GetRaw(entity));
				if (meshCompPtr == nullptr)
					continue;
				if (!_meshRegistry.IsAllocated(meshCompPtr->meshId))
					continue;
				if (!transforms.Has(entity))
					continue;

				const auto* visualCompPtr =
					static_cast<const virasa::ecs::VisualComponent*>(visualSys->GetRaw(entity));
				if (visualCompPtr == nullptr)
					continue;

				const virasa::VisualMaterialRasterState rasterState =
					_materialTable.GetRasterState(visualCompPtr->materialId);

				Drawable d{};
				d.entity = entity;
				d.meshId = meshCompPtr->meshId;
				d.materialId = visualCompPtr->materialId;
				d.alphaMode = rasterState.alphaMode;
				d.doubleSided = rasterState.doubleSided;
				d.model = transforms.GetWorld(entity);

				if (rasterState.alphaMode == virasa::AlphaMode::Blend)
				{
					virasa::math::Vec3 worldPos = transforms.GetWorldPosition(entity);
					virasa::math::Vec3 diff = worldPos - cameraEye;
					d.eyeDistance = glm::dot(diff, diff); // squared distance, sufficient for sort
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

	// --- Capture state for the record callback ---
	virasa::math::Mat4 capturedView = viewMatrix;
	virasa::math::Mat4 capturedProj = projMatrix;

	// Capture pointers to owned resources for the lambda
	virasa::Pipeline* opaquePipeline = &_opaquePipeline;
	virasa::Pipeline* opaqueDoubleSidedPipeline = &_opaqueDoubleSidedPipeline;
	virasa::Pipeline* blendPipeline = &_blendPipeline;
	virasa::Pipeline* blendDoubleSidedPipeline = &_blendDoubleSidedPipeline;
	virasa::BindlessTextureArray* bindless = &_bindlessTextures;
	virasa::renderer::MeshRegistry* meshReg = &_meshRegistry;
	virasa::VisualMaterialTable* matTable = &_materialTable;
	virasa::LightTable* lightTable = &_lightTable;
	const virasa::Device* dev = _device;

	// --- Add forward pass ---
	_graph.AddPass("forward")
		.ColorAttachment(_frameSceneColorHandle,
			virasa::renderer::graph::LoadOp::Clear,
			virasa::renderer::graph::ClearColor{0.1f, 0.1f, 0.15f, 1.0f})
		.DepthAttachment(_frameSceneDepthHandle, virasa::renderer::graph::LoadOp::Clear, 1.0f)
		.Record(
			[opaquePipeline,
				opaqueDoubleSidedPipeline,
				blendPipeline,
				blendDoubleSidedPipeline,
				bindless,
				meshReg,
				matTable,
				lightTable,
				dev,
				capturedView,
				capturedProj,
				allDrawables = std::move(allDrawables)](const virasa::renderer::graph::GraphContext& gc)
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
					if (d.alphaMode == virasa::AlphaMode::Blend)
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
					pc.model = {}; // will be set below — placeholder
					pc.vertexBufferAddress = mesh.GetVertexBufferAddress(*dev);
					pc.indexBufferAddress = mesh.GetIndexBufferAddress(*dev);
					pc.materialBufferAddress = matTable->GetBufferAddress();
					pc.lightBufferAddress = lightTable->GetBufferAddress();
					pc.materialId = d.materialId;
					pc.lightCount = lightTable->GetLightCount();

					// We need the world matrix — it was pre-gathered; store it in the drawable
					// We stored it via allDrawables which captured world matrices at build time.
					// Since we can't call world.GetTransforms() here (world not captured),
					// we need to capture the model matrix in the Drawable struct.
					// NOTE: model matrix is stored in d.model (see below — we add it to Drawable)
					pc.model = d.model;
					pc.mvp = capturedProj * capturedView * d.model;

					vkCmdPushConstants(cmd,
						selectedPipeline->GetLayout(),
						VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
						0,
						sizeof(PushConstants),
						&pc);

					vkCmdDraw(cmd, mesh.GetIndexCount(), 1, 0, 0);
				}
			});

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

} // namespace virasa::renderer::scene
