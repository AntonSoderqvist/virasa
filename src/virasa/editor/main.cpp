#include <vulkan/vulkan.h>

#include <quill/LogMacros.h>

#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

#include "virasa/core/Logger.h"
#include "virasa/ecs/Components.h"
#include "virasa/ecs/Types.h"
#include "virasa/ecs/World.h"
#include "virasa/math/Projection.h"
#include "virasa/math/Transform.h"
#include "virasa/math/Types.h"
#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Context.h"
#include "virasa/renderer/geometry/Primitives.h"
#include "virasa/renderer/graph/BufferRegistry.h"
#include "virasa/renderer/graph/Graph.h"
#include "virasa/renderer/graph/ImageRegistry.h"
#include "virasa/renderer/graph/Pass.h"
#include "virasa/renderer/graph/Types.h"
#include "virasa/renderer/lighting/LightTable.h"
#include "virasa/renderer/material/Visual.h"
#include "virasa/renderer/resources/BindlessTextureArray.h"
#include "virasa/renderer/resources/Image.h"
#include "virasa/renderer/resources/Mesh.h"
#include "virasa/renderer/resources/MeshRegistry.h"
#include "virasa/renderer/resources/Pipeline.h"
#include "virasa/renderer/resources/Sampler.h"
#include "virasa/renderer/resources/ShaderModule.h"
#include "virasa/window/Events.h"
#include "virasa/window/Platform.h"
#include "virasa/window/Types.h"

using namespace virasa;
using namespace virasa::math;
using namespace virasa::ecs;
using namespace virasa::renderer;
using namespace virasa::renderer::graph;

int main(int argc, char** argv)
{
	// -------------------------------------------------------------------------
	// Logger
	// -------------------------------------------------------------------------
	auto* logger = Logger::GetLogger("Main");
	LOG_INFO(logger, "Main entry point reached.");

	// -------------------------------------------------------------------------
	// Platform
	// -------------------------------------------------------------------------
	virasa::window::Platform platform;
	{
		auto err = platform.Initialize("Virasa Editor", 800, 800);
		if (err != ErrorCode::None)
		{
			LOG_ERROR(logger, "Platform::Initialize failed.");
			return -1;
		}
	}

	// -------------------------------------------------------------------------
	// Renderer Context
	// -------------------------------------------------------------------------
	RendererConfig rendererConfig;
	{
		uint32_t extCount = 0;
		const char* const* exts =
			virasa::window::Platform::GetRequiredVulkanExtensions(&extCount);
		rendererConfig.requiredInstanceExtensions = exts;
		rendererConfig.requiredInstanceExtensionCount = extCount;
		// depthFormat defaults to VK_FORMAT_D32_SFLOAT
	}

	Context context;
	{
		auto err = context.Initialize(platform, rendererConfig);
		if (err != RenderError::None)
		{
			LOG_ERROR(logger, "Context::Initialize failed.");
			return -1;
		}
	}

	// -------------------------------------------------------------------------
	// Mesh Registry + Cube Mesh
	// -------------------------------------------------------------------------
	MeshRegistry meshRegistry;
	uint32_t cubeMeshId = 0xFFFFFFFFu;
	{
		MeshData cubeData = CreateCube(0.5f);

		Mesh cubeMesh;
		{
			auto err = cubeMesh.Initialize(context.GetDevice(), context, cubeData);
			if (err != RenderError::None)
			{
				LOG_ERROR(logger, "Mesh::Initialize failed for cube.");
				return -1;
			}
		}

		cubeMeshId = meshRegistry.Allocate(std::move(cubeMesh));
		if (cubeMeshId == 0xFFFFFFFFu)
		{
			LOG_ERROR(logger, "MeshRegistry::Allocate failed for cube.");
			return -1;
		}
	}

	// -------------------------------------------------------------------------
	// Default White Image
	// -------------------------------------------------------------------------
	Image whiteImage;
	{
		ImageConfig cfg;
		cfg.width = 1;
		cfg.height = 1;
		cfg.format = VK_FORMAT_R8G8B8A8_UNORM;
		cfg.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		cfg.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		cfg.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		auto err = whiteImage.Initialize(context.GetDevice(), cfg);
		if (err != RenderError::None)
		{
			LOG_ERROR(logger, "Image::Initialize failed for default white texture.");
			return -1;
		}

		// Upload 0xFF,0xFF,0xFF,0xFF via staging
		const uint8_t whitePixel[4] = {0xFF, 0xFF, 0xFF, 0xFF};
		// We need a staging buffer to upload the pixel data
		{
			Buffer stagingBuf;
			BufferConfig stagingCfg;
			stagingCfg.size = 4;
			stagingCfg.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			stagingCfg.memoryProperties =
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			auto serr = stagingBuf.Initialize(context.GetDevice(), stagingCfg);
			if (serr != RenderError::None)
			{
				LOG_ERROR(logger, "Staging buffer init failed for white image.");
				return -1;
			}
			serr = stagingBuf.Upload(whitePixel, 4);
			if (serr != RenderError::None)
			{
				LOG_ERROR(logger, "Staging buffer upload failed for white image.");
				return -1;
			}

			// Record a one-time command buffer to transition + copy
			VkCommandPool graphicsPool = context.GetGraphicsCommandPool();
			VkQueue graphicsQueue = context.GetDevice().GetGraphicsQueue();
			VkDevice vkDevice = context.GetDevice().GetHandle();

			VkCommandBufferAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.commandPool = graphicsPool;
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocInfo.commandBufferCount = 1;

			VkCommandBuffer cmd = VK_NULL_HANDLE;
			vkAllocateCommandBuffers(vkDevice, &allocInfo, &cmd);

			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			vkBeginCommandBuffer(cmd, &beginInfo);

			// Transition UNDEFINED -> TRANSFER_DST
			VkImageMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = whiteImage.GetHandle();
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
				0,
				0,
				nullptr,
				0,
				nullptr,
				1,
				&barrier);

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
			vkCmdCopyBufferToImage(cmd,
				stagingBuf.GetHandle(),
				whiteImage.GetHandle(),
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1,
				&region);

			// Transition TRANSFER_DST -> SHADER_READ_ONLY
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			vkCmdPipelineBarrier(cmd,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0,
				0,
				nullptr,
				0,
				nullptr,
				1,
				&barrier);

			vkEndCommandBuffer(cmd);

			VkSubmitInfo submitInfo{};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &cmd;
			vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
			vkQueueWaitIdle(graphicsQueue);
			vkFreeCommandBuffers(vkDevice, graphicsPool, 1, &cmd);
		}
	}

	// -------------------------------------------------------------------------
	// Default Sampler
	// -------------------------------------------------------------------------
	Sampler defaultSampler;
	{
		SamplerConfig samplerCfg;
		samplerCfg.magFilter = VK_FILTER_LINEAR;
		samplerCfg.minFilter = VK_FILTER_LINEAR;
		samplerCfg.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCfg.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCfg.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCfg.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

		auto err = defaultSampler.Initialize(context.GetDevice(), samplerCfg);
		if (err != RenderError::None)
		{
			LOG_ERROR(logger, "Sampler::Initialize failed for default sampler.");
			return -1;
		}
	}

	// -------------------------------------------------------------------------
	// Bindless Texture Array
	// -------------------------------------------------------------------------
	BindlessTextureArray bindlessTextures;
	{
		auto err = bindlessTextures.Initialize(context.GetDevice(), 256);
		if (err != RenderError::None)
		{
			LOG_ERROR(logger, "BindlessTextureArray::Initialize failed.");
			return -1;
		}

		uint32_t slot = bindlessTextures.RegisterTexture(
			whiteImage.GetView(), defaultSampler.GetHandle());
		if (slot == 0xFFFFFFFFu)
		{
			LOG_ERROR(logger,
				"BindlessTextureArray::RegisterTexture failed for default white texture.");
			return -1;
		}
		// slot must be 0
	}

	// -------------------------------------------------------------------------
	// Visual Material Table + Default Material
	// -------------------------------------------------------------------------
	VisualMaterialTable materialTable;
	uint32_t cubeMaterialId = 0xFFFFFFFFu;
	{
		auto err = materialTable.Initialize(context.GetDevice(), 64);
		if (err != RenderError::None)
		{
			LOG_ERROR(logger, "VisualMaterialTable::Initialize failed.");
			return -1;
		}

		VisualMaterial mat;
		mat.factors.baseColorFactor = Vec4(0.9f, 0.3f, 0.2f, 1.0f);
		// All other fields at defaults (roughness 1.0, metallic 0.0, etc.)

		cubeMaterialId = materialTable.Allocate(mat);
		if (cubeMaterialId == 0xFFFFFFFFu)
		{
			LOG_ERROR(logger, "VisualMaterialTable::Allocate failed for default material.");
			return -1;
		}
	}

	// -------------------------------------------------------------------------
	// ECS World + Cube Entity
	// -------------------------------------------------------------------------
	World world;
	{
		Entity cubeEntity = world.CreateEntity();

		world.AddTransformComponent(cubeEntity, Transform::Identity());

		MeshComponent meshComp;
		meshComp.meshId = cubeMeshId;
		world.AddMeshComponent(cubeEntity, meshComp);

		VisualComponent visualComp;
		visualComp.materialId = cubeMaterialId;
		world.AddVisualComponent(cubeEntity, visualComp);
	}

	// -------------------------------------------------------------------------
	// Light Table
	// -------------------------------------------------------------------------
	LightTable lightTable;
	{
		auto err = lightTable.Initialize(context.GetDevice(), 256);
		if (err != RenderError::None)
		{
			LOG_ERROR(logger, "LightTable::Initialize failed.");
			return -1;
		}
	}

	// Directional light entity
	{
		Entity lightEntity = world.CreateEntity();
		DirectionalLightComponent dlc;
		dlc.direction = Vec3(0.0f, 0.0f, -1.0f);
		dlc.color = Vec3(1.0f, 1.0f, 1.0f);
		dlc.intensity = 1.0f;
		world.AddDirectionalLightComponent(lightEntity, dlc);
	}

	// -------------------------------------------------------------------------
	// Render Graph Registries + Graph
	// -------------------------------------------------------------------------
	ImageRegistry imageRegistry;
	{
		auto err = imageRegistry.Initialize(context.GetDevice());
		if (err != RenderError::None)
		{
			LOG_ERROR(logger, "ImageRegistry::Initialize failed.");
			return -1;
		}
	}

	BufferRegistry bufferRegistry;
	{
		auto err = bufferRegistry.Initialize(context.GetDevice());
		if (err != RenderError::None)
		{
			LOG_ERROR(logger, "BufferRegistry::Initialize failed.");
			return -1;
		}
	}

	Graph graph;
	{
		auto err = graph.Initialize(context.GetDevice(), imageRegistry, bufferRegistry);
		if (err != RenderError::None)
		{
			LOG_ERROR(logger, "Graph::Initialize failed.");
			return -1;
		}
	}

	// -------------------------------------------------------------------------
	// Shader Modules
	// -------------------------------------------------------------------------
	ShaderModule vertShader;
	{
		auto err = vertShader.Initialize(context.GetDevice(), "shaders/cube.vert.spv");
		if (err != RenderError::None)
		{
			LOG_ERROR(logger, "ShaderModule::Initialize failed for cube.vert.spv.");
			return -1;
		}
	}

	ShaderModule fragShader;
	{
		auto err = fragShader.Initialize(context.GetDevice(), "shaders/cube.frag.spv");
		if (err != RenderError::None)
		{
			LOG_ERROR(logger, "ShaderModule::Initialize failed for cube.frag.spv.");
			return -1;
		}
	}

	// -------------------------------------------------------------------------
	// View / Projection matrices (static, precomputed once)
	// -------------------------------------------------------------------------
	Mat4 viewMatrix = LookAtRH_ZUp(Vec3(2.0f, 2.0f, 2.0f), Vec3(0.0f, 0.0f, 0.0f));

	VkExtent2D swapExtent = context.GetSwapchainExtent();
	Mat4 projMatrix = PerspectiveRH_ZO(glm::radians(45.0f),
		static_cast<float>(swapExtent.width) / static_cast<float>(swapExtent.height),
		0.1f,
		10.0f);

	// -------------------------------------------------------------------------
	// Pipeline
	// -------------------------------------------------------------------------
	Pipeline cubePipeline;
	{
		PipelineBuilder builder;
		builder.SetVertexShader(vertShader);
		builder.SetFragmentShader(fragShader);
		builder.SetColorFormat(context.GetSwapchainFormat());
		builder.SetDepthFormat(context.GetDepthFormat());
		builder.SetExtent(context.GetSwapchainExtent());
		builder.SetCullMode(VK_CULL_MODE_BACK_BIT);
		builder.SetFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE);
		builder.SetDepthTest(true);
		builder.SetDepthWrite(true);
		builder.SetDepthCompareOp(VK_COMPARE_OP_LESS);
		builder.AddPushConstantRange(
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 168);
		builder.AddDescriptorSetLayout(bindlessTextures.GetLayout());

		auto err = builder.Build(context.GetDevice(), cubePipeline);
		if (err != RenderError::None)
		{
			LOG_ERROR(logger, "PipelineBuilder::Build failed for cube pipeline.");
			return -1;
		}
	}

	// -------------------------------------------------------------------------
	// Render Loop
	// -------------------------------------------------------------------------
	while (true)
	{
		// Poll events and check exit conditions
		std::span<const Event> events = platform.PollEvents();
		bool shouldExit = false;
		for (const auto& ev : events)
		{
			if (ev.type == EventType::Quit)
			{
				LOG_INFO(logger, "Exit: Quit event received.");
				shouldExit = true;
				break;
			}
			if (ev.type == EventType::KeyDown && ev.keyboard.key == KeyCode::Escape)
			{
				LOG_INFO(logger, "Exit: Escape key pressed.");
				shouldExit = true;
				break;
			}
		}
		if (shouldExit)
		{
			break;
		}

		// Begin frame
		SwapchainStatus beginStatus = context.BeginFrame();
		if (beginStatus == SwapchainStatus::NotReady)
		{
			continue;
		}
		if (beginStatus == SwapchainStatus::Error)
		{
			LOG_ERROR(logger, "Context::BeginFrame returned Error.");
			return -1;
		}

		// ---- Step 0: Per-frame light upload ----
		std::vector<LightGPU> gpuLights;

		// Directional lights
		for (const Entity& entity : world.GetDirectionalLightComponentEntities())
		{
			const DirectionalLightComponent& dlc = world.GetDirectionalLightComponent(entity);
			LightGPU lg;
			lg.type = static_cast<uint32_t>(LightType::Directional);
			lg.direction = glm::normalize(dlc.direction);
			lg.color = dlc.color * dlc.intensity;
			gpuLights.push_back(lg);
		}

		// Point lights
		for (const Entity& entity : world.GetPointLightComponentEntities())
		{
			if (!world.HasTransformComponent(entity))
			{
				continue;
			}
			const PointLightComponent& plc = world.GetPointLightComponent(entity);
			const Transform& xform = world.GetTransformComponent(entity);
			LightGPU lg;
			lg.type = static_cast<uint32_t>(LightType::Point);
			lg.position = xform.translation;
			lg.range = plc.range;
			lg.color = plc.color * plc.intensity;
			gpuLights.push_back(lg);
		}

		// Spot lights
		for (const Entity& entity : world.GetSpotLightComponentEntities())
		{
			if (!world.HasTransformComponent(entity))
			{
				continue;
			}
			const SpotLightComponent& slc = world.GetSpotLightComponent(entity);
			const Transform& xform = world.GetTransformComponent(entity);
			LightGPU lg;
			lg.type = static_cast<uint32_t>(LightType::Spot);
			lg.position = xform.translation;
			// Y-forward convention: forward = rotation * (0,1,0)
			lg.direction = glm::normalize(xform.rotation * Vec3(0.0f, 1.0f, 0.0f));
			lg.range = slc.range;
			lg.color = slc.color * slc.intensity;
			lg.innerConeCos = slc.innerConeCos;
			lg.outerConeCos = slc.outerConeCos;
			gpuLights.push_back(lg);
		}

		lightTable.UploadFrame(std::span<const LightGPU>(gpuLights.data(), gpuLights.size()));

		// ---- Step 1: Begin graph ----
		graph.Begin();

		// ---- Step 2: Import swapchain image ----
		ImageHandle swapchainHandle = graph.ImportImage(context.GetCurrentImage(),
			context.GetCurrentImageView(),
			context.GetSwapchainFormat(),
			context.GetSwapchainExtent(),
			ResourceUsage::Undefined);

		// ---- Step 3: Import depth image ----
		ImageHandle depthHandle = graph.ImportImage(context.GetDepthImage(),
			context.GetDepthImageView(),
			context.GetDepthFormat(),
			context.GetSwapchainExtent(),
			ResourceUsage::Undefined);

		// ---- Step 4: Declare forward pass ----
		// Capture everything the record callback needs
		const Device& capturedDevice = context.GetDevice();
		Mat4 capturedView = viewMatrix;
		Mat4 capturedProj = projMatrix;
		VkDeviceAddress matBufAddr = materialTable.GetBufferAddress();
		VkDeviceAddress lightBufAddr = lightTable.GetBufferAddress();
		uint32_t lightCount = lightTable.GetLightCount();

		graph.AddPass("forward")
			.ColorAttachment(
				swapchainHandle, LoadOp::Clear, ClearColor{0.1f, 0.1f, 0.15f, 1.0f})
			.DepthAttachment(depthHandle, LoadOp::Clear, 1.0f)
			.Record(
				[&world,
					&meshRegistry,
					&bindlessTextures,
					&cubePipeline,
					capturedView,
					capturedProj,
					matBufAddr,
					lightBufAddr,
					lightCount,
					&capturedDevice](const GraphContext& gc)
				{
					VkCommandBuffer cmd = gc.GetCommandBuffer();
					VkExtent2D renderExtent = gc.GetRenderExtent();

					// (i) Dynamic viewport/scissor
					VkViewport viewport{};
					viewport.x = 0.0f;
					viewport.y = 0.0f;
					viewport.width = static_cast<float>(renderExtent.width);
					viewport.height = static_cast<float>(renderExtent.height);
					viewport.minDepth = 0.0f;
					viewport.maxDepth = 1.0f;
					vkCmdSetViewport(cmd, 0, 1, &viewport);

					VkRect2D scissor{};
					scissor.offset = {0, 0};
					scissor.extent = renderExtent;
					vkCmdSetScissor(cmd, 0, 1, &scissor);

					// (ii) Bind pipeline
					cubePipeline.Bind(cmd);

					// (iii) Bind bindless descriptor set at set 0
					VkDescriptorSet bindlessSet = bindlessTextures.GetDescriptorSet();
					vkCmdBindDescriptorSets(cmd,
						VK_PIPELINE_BIND_POINT_GRAPHICS,
						cubePipeline.GetLayout(),
						0,
						1,
						&bindlessSet,
						0,
						nullptr);

					// (iv) Per-entity draw loop
					const std::vector<Entity>& visualEntities =
						world.GetVisualComponentEntities();
					for (const Entity& entity : visualEntities)
					{
						// (a) Check mesh component
						if (!world.HasMeshComponent(entity))
						{
							continue;
						}
						const MeshComponent& meshComp = world.GetMeshComponent(entity);

						// (b) Check mesh registry
						if (!meshRegistry.IsAllocated(meshComp.meshId))
						{
							continue;
						}
						const Mesh& mesh = meshRegistry.Get(meshComp.meshId);

						// (c) Model matrix from transform (or identity)
						Mat4 modelMatrix = Mat4(1.0f);
						if (world.HasTransformComponent(entity))
						{
							const Transform& xform =
								world.GetTransformComponent(entity);
							modelMatrix = xform.ToMatrix();
						}

						// (d) Material id
						uint32_t matId = world.GetVisualComponent(entity).materialId;

						// (e) Compose MVP and push constants
						Mat4 mvp = capturedProj * capturedView * modelMatrix;

						VkDeviceAddress vertAddr =
							mesh.GetVertexBufferAddress(capturedDevice);
						VkDeviceAddress idxAddr =
							mesh.GetIndexBufferAddress(capturedDevice);

						// Build 168-byte push constant payload
						struct PushConstants
						{
							Mat4 mvp;				  // offset 0,   64 bytes
							Mat4 model;				  // offset 64,  64 bytes
							uint64_t vertexBufferAddress;	  // offset 128, 8 bytes
							uint64_t indexBufferAddress;	  // offset 136, 8 bytes
							uint64_t materialBufferAddress; // offset 144, 8 bytes
							uint64_t lightBufferAddress;	  // offset 152, 8 bytes
							uint32_t materialId;		  // offset 160, 4 bytes
							uint32_t lightCount;		  // offset 164, 4 bytes
						};
						static_assert(sizeof(PushConstants) == 168,
							"PushConstants must be 168 bytes");

						PushConstants pc{};
						pc.mvp = mvp;
						pc.model = modelMatrix;
						pc.vertexBufferAddress = static_cast<uint64_t>(vertAddr);
						pc.indexBufferAddress = static_cast<uint64_t>(idxAddr);
						pc.materialBufferAddress = static_cast<uint64_t>(matBufAddr);
						pc.lightBufferAddress = static_cast<uint64_t>(lightBufAddr);
						pc.materialId = matId;
						pc.lightCount = lightCount;

						vkCmdPushConstants(cmd,
							cubePipeline.GetLayout(),
							VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
							0,
							168,
							&pc);

						// (f) Draw
						vkCmdDraw(cmd, mesh.GetIndexCount(), 1, 0, 0);
					}
				});

		// ---- Step 5: Compile and Execute ----
		{
			auto err = graph.Compile();
			if (err != RenderError::None)
			{
				LOG_ERROR(logger, "Graph::Compile failed.");
				return -1;
			}
		}

		{
			auto err = graph.Execute(context.GetCurrentCommandBuffer());
			if (err != RenderError::None)
			{
				LOG_ERROR(logger, "Graph::Execute failed.");
				return -1;
			}
		}

		// ---- Step 6: End graph ----
		graph.End();

		// End frame
		SwapchainStatus endStatus = context.EndFrame();
		(void)endStatus; // Recreated is not an error
		if (endStatus == SwapchainStatus::Error)
		{
			LOG_ERROR(logger, "Context::EndFrame returned Error.");
			return -1;
		}
	}

	return 0;
}
