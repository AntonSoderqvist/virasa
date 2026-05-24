#include <vulkan/vulkan.h>

#include <quill/LogMacros.h>

#include <cstdint>
#include <cstring>

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

int main(int argc, char** argv)
{
	// --- Logger ---
	auto* logger = virasa::Logger::GetLogger("Main");
	LOG_INFO(logger, "Entry point reached.");

	// --- Platform ---
	virasa::window::Platform platform;
	{
		virasa::ErrorCode ec = platform.Initialize("Virasa", 800, 800);
		if (ec != virasa::ErrorCode::None)
		{
			LOG_ERROR(logger, "Platform::Initialize failed: {}", static_cast<int>(ec));
			return -1;
		}
	}

	// --- Renderer Context ---
	virasa::Context context;
	{
		uint32_t extCount = 0;
		const char* const* exts =
			virasa::window::Platform::GetRequiredVulkanExtensions(&extCount);

		virasa::RendererConfig rendererConfig;
		rendererConfig.requiredInstanceExtensions = exts;
		rendererConfig.requiredInstanceExtensionCount = extCount;

		virasa::RenderError re = context.Initialize(platform, rendererConfig);
		if (re != virasa::RenderError::None)
		{
			LOG_ERROR(logger, "Context::Initialize failed: {}", static_cast<int>(re));
			return -1;
		}
	}

	const virasa::Device& device = context.GetDevice();

	// --- Cube Mesh ---
	virasa::renderer::MeshRegistry meshRegistry;
	uint32_t cubeMeshId = 0xFFFFFFFFu;
	{
		virasa::MeshData cubeData = virasa::CreateCube();

		virasa::Mesh localMesh;
		virasa::RenderError re = localMesh.Initialize(device, context, cubeData);
		if (re != virasa::RenderError::None)
		{
			LOG_ERROR(logger, "Mesh::Initialize failed: {}", static_cast<int>(re));
			return -1;
		}

		cubeMeshId = meshRegistry.Allocate(std::move(localMesh));
		if (cubeMeshId == 0xFFFFFFFFu)
		{
			LOG_ERROR(logger, "MeshRegistry::Allocate failed.");
			return -1;
		}
	}

	// --- Default White Texture ---
	virasa::Image whiteImage;
	{
		virasa::ImageConfig cfg;
		cfg.width = 1;
		cfg.height = 1;
		cfg.format = VK_FORMAT_R8G8B8A8_UNORM;
		cfg.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		cfg.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		cfg.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		virasa::RenderError re = whiteImage.Initialize(device, cfg);
		if (re != virasa::RenderError::None)
		{
			LOG_ERROR(logger, "White Image::Initialize failed: {}", static_cast<int>(re));
			return -1;
		}

		// Upload 4 bytes of 0xFF via a staging buffer
		{
			const uint8_t whitePixel[4] = {0xFF, 0xFF, 0xFF, 0xFF};

			// Create a host-visible staging buffer
			virasa::Buffer stagingBuffer;
			virasa::BufferConfig stagingCfg;
			stagingCfg.size = 4;
			stagingCfg.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			stagingCfg.memoryProperties =
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			virasa::RenderError sre = stagingBuffer.Initialize(device, stagingCfg);
			if (sre != virasa::RenderError::None)
			{
				LOG_ERROR(logger,
					"Staging buffer for white texture failed: {}",
					static_cast<int>(sre));
				return -1;
			}
			stagingBuffer.Upload(whitePixel, 4);

			// Record a one-time command buffer to copy and transition
			// Use the graphics pool/queue so the final TRANSFER->FRAGMENT_SHADER
			// barrier's dstStageMask is supported by the command buffer's queue
			// family (a transfer-only queue does not support FRAGMENT_SHADER).
			VkCommandPool uploadPool = context.GetGraphicsCommandPool();
			VkQueue uploadQueue = device.GetGraphicsQueue();

			VkCommandBufferAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.commandPool = uploadPool;
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocInfo.commandBufferCount = 1;

			VkCommandBuffer cmd = VK_NULL_HANDLE;
			vkAllocateCommandBuffers(device.GetHandle(), &allocInfo, &cmd);

			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			vkBeginCommandBuffer(cmd, &beginInfo);

			// Transition UNDEFINED -> TRANSFER_DST_OPTIMAL
			{
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
			}

			// Copy buffer to image
			{
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
					stagingBuffer.GetHandle(),
					whiteImage.GetHandle(),
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1,
					&region);
			}

			// Transition TRANSFER_DST_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL
			{
				VkImageMemoryBarrier barrier{};
				barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.image = whiteImage.GetHandle();
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				barrier.subresourceRange.baseMipLevel = 0;
				barrier.subresourceRange.levelCount = 1;
				barrier.subresourceRange.baseArrayLayer = 0;
				barrier.subresourceRange.layerCount = 1;
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
			}

			vkEndCommandBuffer(cmd);

			VkSubmitInfo submitInfo{};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &cmd;
			vkQueueSubmit(uploadQueue, 1, &submitInfo, VK_NULL_HANDLE);
			vkQueueWaitIdle(uploadQueue);

			vkFreeCommandBuffers(device.GetHandle(), uploadPool, 1, &cmd);
		}
	}

	// --- Default Sampler ---
	virasa::Sampler defaultSampler;
	{
		virasa::SamplerConfig samplerCfg;
		samplerCfg.magFilter = VK_FILTER_LINEAR;
		samplerCfg.minFilter = VK_FILTER_LINEAR;
		samplerCfg.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCfg.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCfg.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCfg.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

		virasa::RenderError re = defaultSampler.Initialize(device, samplerCfg);
		if (re != virasa::RenderError::None)
		{
			LOG_ERROR(logger, "Sampler::Initialize failed: {}", static_cast<int>(re));
			return -1;
		}
	}

	// --- Bindless Texture Array ---
	virasa::BindlessTextureArray bindlessArray;
	{
		virasa::RenderError re = bindlessArray.Initialize(device, 256);
		if (re != virasa::RenderError::None)
		{
			LOG_ERROR(logger,
				"BindlessTextureArray::Initialize failed: {}",
				static_cast<int>(re));
			return -1;
		}

		uint32_t slot =
			bindlessArray.RegisterTexture(whiteImage.GetView(), defaultSampler.GetHandle());
		if (slot == 0xFFFFFFFFu)
		{
			LOG_ERROR(
				logger, "BindlessTextureArray::RegisterTexture failed for default white.");
			return -1;
		}
		// slot must be 0 per contract
	}

	// --- Visual Material Table ---
	virasa::VisualMaterialTable materialTable;
	uint32_t cubeMaterialId = 0xFFFFFFFFu;
	{
		virasa::RenderError re = materialTable.Initialize(device, 64);
		if (re != virasa::RenderError::None)
		{
			LOG_ERROR(
				logger, "VisualMaterialTable::Initialize failed: {}", static_cast<int>(re));
			return -1;
		}

		virasa::VisualMaterial mat;
		mat.factors.baseColorFactor = virasa::math::Vec4(0.9f, 0.3f, 0.2f, 1.0f);

		cubeMaterialId = materialTable.Allocate(mat);
		if (cubeMaterialId == 0xFFFFFFFFu)
		{
			LOG_ERROR(logger, "VisualMaterialTable::Allocate failed.");
			return -1;
		}
	}

	// --- ECS World ---
	virasa::ecs::World world;
	{
		virasa::ecs::Entity entity = world.CreateEntity();

		world.AddTransformComponent(entity, virasa::math::Transform::Identity());

		virasa::ecs::MeshComponent meshComp;
		meshComp.meshId = cubeMeshId;
		world.AddMeshComponent(entity, meshComp);

		virasa::ecs::VisualComponent visualComp;
		visualComp.materialId = cubeMaterialId;
		world.AddVisualComponent(entity, visualComp);
	}

	// --- Graph Registries and Graph ---
	virasa::renderer::graph::ImageRegistry imageRegistry;
	virasa::renderer::graph::BufferRegistry bufferRegistry;
	virasa::renderer::graph::Graph graph;
	{
		virasa::RenderError re = imageRegistry.Initialize(device);
		if (re != virasa::RenderError::None)
		{
			LOG_ERROR(logger, "ImageRegistry::Initialize failed: {}", static_cast<int>(re));
			return -1;
		}

		re = bufferRegistry.Initialize(device);
		if (re != virasa::RenderError::None)
		{
			LOG_ERROR(logger, "BufferRegistry::Initialize failed: {}", static_cast<int>(re));
			return -1;
		}

		re = graph.Initialize(device, imageRegistry, bufferRegistry);
		if (re != virasa::RenderError::None)
		{
			LOG_ERROR(logger, "Graph::Initialize failed: {}", static_cast<int>(re));
			return -1;
		}
	}

	// --- Shaders ---
	virasa::ShaderModule vertShader;
	virasa::ShaderModule fragShader;
	{
		virasa::RenderError re = vertShader.Initialize(device, "shaders/cube.vert.spv");
		if (re != virasa::RenderError::None)
		{
			LOG_ERROR(
				logger, "Vertex ShaderModule::Initialize failed: {}", static_cast<int>(re));
			return -1;
		}

		re = fragShader.Initialize(device, "shaders/cube.frag.spv");
		if (re != virasa::RenderError::None)
		{
			LOG_ERROR(logger,
				"Fragment ShaderModule::Initialize failed: {}",
				static_cast<int>(re));
			return -1;
		}
	}

	// --- View / Projection ---
	virasa::math::Mat4 viewMatrix = virasa::math::LookAtRH_ZUp(
		virasa::math::Vec3(2.0f, 2.0f, 2.0f), virasa::math::Vec3(0.0f, 0.0f, 0.0f));

	VkExtent2D swapExtent = context.GetSwapchainExtent();
	virasa::math::Mat4 projMatrix = virasa::math::PerspectiveRH_ZO(glm::radians(45.0f),
		static_cast<float>(swapExtent.width) / static_cast<float>(swapExtent.height),
		0.1f,
		10.0f);

	// --- Pipeline ---
	virasa::Pipeline cubePipeline;
	{
		virasa::PipelineBuilder builder;
		builder.SetVertexShader(vertShader)
			.SetFragmentShader(fragShader)
			.SetColorFormat(context.GetSwapchainFormat())
			.SetDepthFormat(context.GetDepthFormat())
			.SetExtent(context.GetSwapchainExtent())
			.SetCullMode(VK_CULL_MODE_BACK_BIT)
			.SetFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
			.SetDepthTest(true)
			.SetDepthWrite(true)
			.SetDepthCompareOp(VK_COMPARE_OP_LESS)
			.AddPushConstantRange(
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 96)
			.AddDescriptorSetLayout(bindlessArray.GetLayout());

		virasa::RenderError re = builder.Build(device, cubePipeline);
		if (re != virasa::RenderError::None)
		{
			LOG_ERROR(logger, "PipelineBuilder::Build failed: {}", static_cast<int>(re));
			return -1;
		}
	}

	// --- Render Loop ---
	bool running = true;
	while (running)
	{
		// Poll events
		auto events = platform.PollEvents();
		for (const auto& ev : events)
		{
			if (ev.type == virasa::EventType::Quit)
			{
				LOG_INFO(logger, "Quit event received, exiting.");
				running = false;
				break;
			}
			if (ev.type == virasa::EventType::KeyDown &&
				ev.keyboard.key == virasa::KeyCode::Escape)
			{
				LOG_INFO(logger, "Escape key pressed, exiting.");
				running = false;
				break;
			}
		}

		if (!running)
			break;

		// Begin frame
		virasa::SwapchainStatus status = context.BeginFrame();
		if (status == virasa::SwapchainStatus::NotReady)
			continue;
		if (status == virasa::SwapchainStatus::Error)
		{
			LOG_ERROR(logger, "Context::BeginFrame returned Error.");
			return -1;
		}

		// Build frame graph
		graph.Begin();

		// Import swapchain image
		virasa::renderer::graph::ImageHandle swapchainHandle =
			graph.ImportImage(context.GetCurrentImage(),
				context.GetCurrentImageView(),
				context.GetSwapchainFormat(),
				context.GetSwapchainExtent(),
				virasa::renderer::graph::ResourceUsage::Undefined);

		// Import depth image
		virasa::renderer::graph::ImageHandle depthHandle = graph.ImportImage(
			context.GetDepthImage(),
			context.GetDepthImageView(),
			context.GetDepthFormat(),
			context.GetSwapchainExtent(),
			virasa::renderer::graph::ResourceUsage::Undefined);

		// Capture everything the record callback needs
		const virasa::Device& capturedDevice = device;
		const virasa::ecs::World& capturedWorld = world;
		const virasa::renderer::MeshRegistry& capturedMeshRegistry = meshRegistry;
		const virasa::VisualMaterialTable& capturedMaterialTable = materialTable;
		const virasa::BindlessTextureArray& capturedBindless = bindlessArray;
		const virasa::Pipeline& capturedPipeline = cubePipeline;
		const virasa::math::Mat4& capturedView = viewMatrix;
		const virasa::math::Mat4& capturedProj = projMatrix;

		graph.AddPass("forward")
			.ColorAttachment(swapchainHandle,
				virasa::renderer::graph::LoadOp::Clear,
				virasa::renderer::graph::ClearColor{0.1f, 0.1f, 0.15f, 1.0f})
			.DepthAttachment(depthHandle, virasa::renderer::graph::LoadOp::Clear, 1.0f)
			.Record(
				[&capturedDevice,
					&capturedWorld,
					&capturedMeshRegistry,
					&capturedMaterialTable,
					&capturedBindless,
					&capturedPipeline,
					&capturedView,
					&capturedProj](const virasa::renderer::graph::GraphContext& gc)
				{
					VkCommandBuffer cmd = gc.GetCommandBuffer();
					VkExtent2D renderExtent = gc.GetRenderExtent();

					// Dynamic viewport and scissor
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

					// Bind pipeline
					capturedPipeline.Bind(cmd);

					// Bind bindless descriptor set
					VkDescriptorSet bindlessSet = capturedBindless.GetDescriptorSet();
					vkCmdBindDescriptorSets(cmd,
						VK_PIPELINE_BIND_POINT_GRAPHICS,
						capturedPipeline.GetLayout(),
						0,
						1,
						&bindlessSet,
						0,
						nullptr);

					// Per-entity draw loop
					const std::vector<virasa::ecs::Entity>& drawEntities =
						capturedWorld.GetVisualComponentEntities();

					for (const virasa::ecs::Entity& entity : drawEntities)
					{
						// MeshComponent
						if (!capturedWorld.HasMeshComponent(entity))
							continue;
						const virasa::ecs::MeshComponent& meshComp =
							capturedWorld.GetMeshComponent(entity);

						// Mesh
						if (!capturedMeshRegistry.IsAllocated(meshComp.meshId))
							continue;
						const virasa::Mesh& mesh =
							capturedMeshRegistry.Get(meshComp.meshId);

						// Transform
						virasa::math::Mat4 modelMatrix(1.0f);
						if (capturedWorld.HasTransformComponent(entity))
						{
							const virasa::math::Transform& transform =
								capturedWorld.GetTransformComponent(entity);
							modelMatrix = transform.ToMatrix();
						}

						// Material id
						uint32_t materialId =
							capturedWorld.GetVisualComponent(entity).materialId;

						// MVP
						virasa::math::Mat4 mvp =
							capturedProj * capturedView * modelMatrix;

						// Push constant payload (96 bytes)
						struct PushConstants
						{
							virasa::math::Mat4 mvp;
							uint64_t vertexBufferAddress;
							uint64_t indexBufferAddress;
							uint64_t materialBufferAddress;
							uint32_t materialId;
							uint32_t pad;
						};

						PushConstants pc{};
						pc.mvp = mvp;
						pc.vertexBufferAddress =
							mesh.GetVertexBufferAddress(capturedDevice);
						pc.indexBufferAddress =
							mesh.GetIndexBufferAddress(capturedDevice);
						pc.materialBufferAddress =
							capturedMaterialTable.GetBufferAddress();
						pc.materialId = materialId;
						pc.pad = 0u;

						vkCmdPushConstants(cmd,
							capturedPipeline.GetLayout(),
							VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
							0,
							96,
							&pc);

						vkCmdDraw(cmd, mesh.GetIndexCount(), 1, 0, 0);
					}
				});

		// Compile
		{
			virasa::RenderError re = graph.Compile();
			if (re != virasa::RenderError::None)
			{
				LOG_ERROR(logger, "Graph::Compile failed: {}", static_cast<int>(re));
				return -1;
			}
		}

		// Execute
		{
			virasa::RenderError re = graph.Execute(context.GetCurrentCommandBuffer());
			if (re != virasa::RenderError::None)
			{
				LOG_ERROR(logger, "Graph::Execute failed: {}", static_cast<int>(re));
				return -1;
			}
		}

		graph.End();

		// End frame
		context.EndFrame();
	}

	// Ensure all submitted GPU work has completed before any owned
	// resources (pipeline, descriptor pool, etc.) begin destruction.
	device.WaitIdle();

	LOG_INFO(logger, "Exiting normally.");
	return 0;
}
