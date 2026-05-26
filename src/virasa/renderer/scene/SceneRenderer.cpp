#include "virasa/renderer/scene/SceneRenderer.h"

#include <cassert>
#include <cstring>
#include <span>
#include <vector>

#include "virasa/renderer/geometry/Primitives.h"
#include "virasa/ecs/Components.h"
#include "virasa/math/Transform.h"
#include "virasa/math/Projection.h"
#include "virasa/core/Logger.h"

namespace virasa::renderer::scene
{

// ---------------------------------------------------------------------------
// Internal push-constant layout (168 bytes)
// ---------------------------------------------------------------------------
struct PushConstants
{
	virasa::math::Mat4 mvp;                  // 64
	virasa::math::Mat4 model;                // 64
	uint64_t vertexBufferAddress;            //  8
	uint64_t indexBufferAddress;             //  8
	uint64_t materialBufferAddress;          //  8
	uint64_t lightBufferAddress;             //  8
	uint32_t materialId;                     //  4
	uint32_t lightCount;                     //  4
};
static_assert(sizeof(PushConstants) == 168, "PushConstants must be exactly 168 bytes");

// ---------------------------------------------------------------------------
// Initialize
// ---------------------------------------------------------------------------
virasa::RenderError SceneRenderer::Initialize(
	const virasa::Device& device,
	const virasa::Context& context,
	const virasa::ui::FontAtlas& atlas)
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

		// Upload 0xFFFFFFFF pixel via staging
		const uint32_t whitePixel = 0xFFFFFFFFu;

		// Create staging buffer
		virasa::Buffer stagingBuffer;
		virasa::BufferConfig stagingCfg{};
		stagingCfg.size = sizeof(uint32_t);
		stagingCfg.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		stagingCfg.memoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

		err = stagingBuffer.Initialize(device, stagingCfg);
		if (err != virasa::RenderError::None)
		{
			_initialized = false;
			return err;
		}

		err = stagingBuffer.Upload(&whitePixel, sizeof(uint32_t));
		if (err != virasa::RenderError::None)
		{
			_initialized = false;
			return err;
		}

		// One-shot command buffer
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = context.GetGraphicsCommandPool();
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer cmd = VK_NULL_HANDLE;
		vkAllocateCommandBuffers(device.GetHandle(), &allocInfo, &cmd);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(cmd, &beginInfo);

		// Transition image: UNDEFINED -> TRANSFER_DST_OPTIMAL
		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = _whiteImage.GetHandle();
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		vkCmdPipelineBarrier(cmd,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			0, 0, nullptr, 0, nullptr, 1, &barrier);

		// Copy buffer to image
		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = {0, 0, 0};
		region.imageExtent = {1, 1, 1};

		vkCmdCopyBufferToImage(cmd, stagingBuffer.GetHandle(), _whiteImage.GetHandle(),
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		// Transition image: TRANSFER_DST_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(cmd,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0, 0, nullptr, 0, nullptr, 1, &barrier);

		vkEndCommandBuffer(cmd);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmd;

		vkQueueSubmit(device.GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(device.GetGraphicsQueue());

		vkFreeCommandBuffers(device.GetHandle(), context.GetGraphicsCommandPool(), 1, &cmd);
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
		uint32_t slot = _bindlessTextures.RegisterTexture(_whiteImage.GetView(), _defaultSampler.GetHandle());
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

	// Step 10: build forward pipeline
	{
		virasa::PipelineBuilder builder;
		builder
			.SetVertexShader(_forwardVertexShader)
			.SetFragmentShader(_forwardFragmentShader)
			.SetColorFormat(context.GetSwapchainFormat())
			.SetDepthFormat(context.GetDepthFormat())
			.SetCullMode(VK_CULL_MODE_BACK_BIT)
			.SetFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
			.SetDepthTest(true)
			.SetDepthWrite(true)
			.SetDepthCompareOp(VK_COMPARE_OP_LESS)
			.AddPushConstantRange(
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0,
				168)
			.AddDescriptorSetLayout(_bindlessTextures.GetLayout());

		auto err = builder.Build(device, _forwardPipeline);
		if (err != virasa::RenderError::None)
		{
			_initialized = false;
			return err;
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
	_frameSwapchainHandle = _graph.ImportImage(
		_context->GetCurrentImage(),
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
uint32_t SceneRenderer::RenderWorld(const virasa::ecs::World& world, virasa::ecs::Entity cameraEntity)
{
	// --- Derive camera matrices ---
	virasa::math::Mat4 viewMatrix(1.0f);
	virasa::math::Mat4 projMatrix(1.0f);

	if (world.HasTransformComponent(cameraEntity) && world.HasCameraComponent(cameraEntity))
	{
		const auto& transform = world.GetTransformComponent(cameraEntity);
		const auto& cam = world.GetCameraComponent(cameraEntity);

		virasa::math::Vec3 eye = transform.translation;
		virasa::math::Vec3 target = transform.translation + transform.Forward();
		viewMatrix = virasa::math::LookAtRH_ZUp(eye, target);

		float aspect = (cam.aspect > 0.0f)
			? cam.aspect
			: static_cast<float>(_frameSceneWidth) / static_cast<float>(_frameSceneHeight);

		projMatrix = virasa::math::PerspectiveRH_ZO(cam.fovY, aspect, cam.nearPlane, cam.farPlane);
	}

	// --- Gather lights ---
	std::vector<virasa::LightGPU> lights;

	// Directional lights
	for (auto entity : world.GetDirectionalLightComponentEntities())
	{
		const auto& dl = world.GetDirectionalLightComponent(entity);
		virasa::LightGPU light{};
		light.type = static_cast<uint32_t>(virasa::LightType::Directional);
		light.direction = glm::normalize(dl.direction);
		light.color = dl.color * dl.intensity;
		lights.push_back(light);
	}

	// Point lights
	for (auto entity : world.GetPointLightComponentEntities())
	{
		if (!world.HasTransformComponent(entity))
			continue;
		const auto& pl = world.GetPointLightComponent(entity);
		const auto& tr = world.GetTransformComponent(entity);
		virasa::LightGPU light{};
		light.type = static_cast<uint32_t>(virasa::LightType::Point);
		light.position = tr.translation;
		light.range = pl.range;
		light.color = pl.color * pl.intensity;
		lights.push_back(light);
	}

	// Spot lights
	for (auto entity : world.GetSpotLightComponentEntities())
	{
		if (!world.HasTransformComponent(entity))
			continue;
		const auto& sl = world.GetSpotLightComponent(entity);
		const auto& tr = world.GetTransformComponent(entity);
		virasa::LightGPU light{};
		light.type = static_cast<uint32_t>(virasa::LightType::Spot);
		light.position = tr.translation;
		virasa::math::Vec3 dir = glm::normalize(tr.rotation * virasa::math::Vec3(0.0f, 1.0f, 0.0f));
		light.direction = dir;
		light.range = sl.range;
		light.color = sl.color * sl.intensity;
		light.innerConeCos = sl.innerConeCos;
		light.outerConeCos = sl.outerConeCos;
		lights.push_back(light);
	}

	_lightTable.UploadFrame(std::span<const virasa::LightGPU>(lights));

	// --- Capture state for the record callback ---
	virasa::math::Mat4 capturedView = viewMatrix;
	virasa::math::Mat4 capturedProj = projMatrix;
	uint32_t capturedSceneWidth = _frameSceneWidth;
	uint32_t capturedSceneHeight = _frameSceneHeight;

	// Capture pointers to owned resources for the lambda
	virasa::Pipeline* pipeline = &_forwardPipeline;
	virasa::BindlessTextureArray* bindless = &_bindlessTextures;
	virasa::renderer::MeshRegistry* meshReg = &_meshRegistry;
	virasa::VisualMaterialTable* matTable = &_materialTable;
	virasa::LightTable* lightTable = &_lightTable;
	const virasa::Device* dev = _device;

	// --- Add forward pass ---
	_graph.AddPass("forward")
		.ColorAttachment(
			_frameSceneColorHandle,
			virasa::renderer::graph::LoadOp::Clear,
			virasa::renderer::graph::ClearColor{0.1f, 0.1f, 0.15f, 1.0f})
		.DepthAttachment(
			_frameSceneDepthHandle,
			virasa::renderer::graph::LoadOp::Clear,
			1.0f)
		.Record([pipeline, bindless, meshReg, matTable, lightTable, dev,
				capturedView, capturedProj, &world](const virasa::renderer::graph::GraphContext& gc)
		{
			VkCommandBuffer cmd = gc.GetCommandBuffer();
			VkExtent2D extent = gc.GetRenderExtent();

			// Viewport and scissor
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

			// Bind pipeline
			pipeline->Bind(cmd);

			// Bind bindless descriptor set
			VkDescriptorSet descSet = bindless->GetDescriptorSet();
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipeline->GetLayout(), 0, 1, &descSet, 0, nullptr);

			// Draw each entity with a visual component
			for (auto entity : world.GetVisualComponentEntities())
			{
				if (!world.HasMeshComponent(entity))
					continue;

				const auto& meshComp = world.GetMeshComponent(entity);
				if (!meshReg->IsAllocated(meshComp.meshId))
					continue;

				const auto& mesh = meshReg->Get(meshComp.meshId);
				const auto& visualComp = world.GetVisualComponent(entity);

				// Model matrix
				virasa::math::Mat4 model(1.0f);
				if (world.HasTransformComponent(entity))
				{
					model = world.GetTransformComponent(entity).ToMatrix();
				}

				virasa::math::Mat4 mvp = capturedProj * capturedView * model;

				PushConstants pc{};
				pc.mvp = mvp;
				pc.model = model;
				pc.vertexBufferAddress = mesh.GetVertexBufferAddress(*dev);
				pc.indexBufferAddress = mesh.GetIndexBufferAddress(*dev);
				pc.materialBufferAddress = matTable->GetBufferAddress();
				pc.lightBufferAddress = lightTable->GetBufferAddress();
				pc.materialId = visualComp.materialId;
				pc.lightCount = lightTable->GetLightCount();

				vkCmdPushConstants(cmd, pipeline->GetLayout(),
					VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
					0, sizeof(PushConstants), &pc);

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
		uint32_t slot = _bindlessTextures.RegisterTexture(sceneView, _defaultSampler.GetHandle());
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
virasa::SwapchainStatus SceneRenderer::SubmitFrame(
	const virasa::ui::DrawList& drawList,
	const virasa::ui::FontAtlas& atlas,
	uint32_t windowWidth,
	uint32_t windowHeight)
{
	// Step 1: submit UI pass
	std::array<virasa::renderer::graph::ImageHandle, 1> sampledImages = {_frameSceneColorHandle};
	_uiPass.Submit(
		_graph,
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
