#include <vulkan/vulkan.h>

#include <quill/LogMacros.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <glm/trigonometric.hpp>
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
#include "virasa/renderer/core/Device.h"
#include "virasa/renderer/geometry/Primitives.h"
#include "virasa/renderer/graph/BufferRegistry.h"
#include "virasa/renderer/graph/Graph.h"
#include "virasa/renderer/graph/ImageRegistry.h"
#include "virasa/renderer/graph/Pass.h"
#include "virasa/renderer/graph/Types.h"
#include "virasa/renderer/lighting/LightTable.h"
#include "virasa/renderer/material/Visual.h"
#include "virasa/renderer/resources/BindlessTextureArray.h"
#include "virasa/renderer/resources/Buffer.h"
#include "virasa/renderer/resources/Image.h"
#include "virasa/renderer/resources/Mesh.h"
#include "virasa/renderer/resources/MeshRegistry.h"
#include "virasa/renderer/resources/Pipeline.h"
#include "virasa/renderer/resources/Sampler.h"
#include "virasa/renderer/resources/ShaderModule.h"
#include "virasa/window/Events.h"
#include "virasa/window/Platform.h"
#include "virasa/window/Types.h"

namespace
{
struct PushConstants
{
	virasa::math::Mat4 mvp;
	virasa::math::Mat4 model;
	uint64_t vertexBufferAddress;
	uint64_t indexBufferAddress;
	uint64_t materialBufferAddress;
	uint64_t lightBufferAddress;
	uint32_t materialId;
	uint32_t lightCount;
};

static_assert(sizeof(PushConstants) == 168);
} // namespace

int main(int argc, char** argv)
{
	(void)argc;
	(void)argv;

	auto* logger = virasa::Logger::GetLogger("Main");
	LOG_INFO(logger, "Entry point reached.");

	virasa::window::Platform platform;
	{
		virasa::ErrorCode error = platform.Initialize("VirasaEditor", 800, 800);
		if (error != virasa::ErrorCode::None)
		{
			LOG_ERROR(logger, "Platform::Initialize failed: {}", static_cast<int>(error));
			return -1;
		}
	}

	virasa::Context context;
	{
		uint32_t requiredExtensionCount = 0;
		const char* const* requiredExtensions =
			virasa::window::Platform::GetRequiredVulkanExtensions(&requiredExtensionCount);

		virasa::RendererConfig rendererConfig;
		rendererConfig.requiredInstanceExtensions = requiredExtensions;
		rendererConfig.requiredInstanceExtensionCount = requiredExtensionCount;

		virasa::RenderError error = context.Initialize(platform, rendererConfig);
		if (error != virasa::RenderError::None)
		{
			LOG_ERROR(logger, "Context::Initialize failed: {}", static_cast<int>(error));
			return -1;
		}
	}

	const virasa::Device& device = context.GetDevice();

	virasa::renderer::MeshRegistry meshRegistry;
	uint32_t cubeMeshId = 0xFFFFFFFFu;
	{
		virasa::MeshData cubeData = virasa::CreateCube();
		virasa::Mesh localMesh;

		virasa::RenderError error = localMesh.Initialize(device, context, cubeData);
		if (error != virasa::RenderError::None)
		{
			LOG_ERROR(logger, "Mesh::Initialize failed: {}", static_cast<int>(error));
			return -1;
		}

		cubeMeshId = meshRegistry.Allocate(std::move(localMesh));
		if (cubeMeshId == 0xFFFFFFFFu)
		{
			LOG_ERROR(logger, "MeshRegistry::Allocate failed.");
			return -1;
		}
	}

	virasa::Image whiteImage;
	{
		virasa::ImageConfig imageConfig;
		imageConfig.width = 1;
		imageConfig.height = 1;
		imageConfig.format = VK_FORMAT_R8G8B8A8_UNORM;
		imageConfig.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		imageConfig.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		imageConfig.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		virasa::RenderError error = whiteImage.Initialize(device, imageConfig);
		if (error != virasa::RenderError::None)
		{
			LOG_ERROR(logger, "Image::Initialize failed: {}", static_cast<int>(error));
			return -1;
		}

		const uint8_t whitePixel[4] = {0xFF, 0xFF, 0xFF, 0xFF};
		virasa::Buffer stagingBuffer;
		virasa::BufferConfig stagingConfig;
		stagingConfig.size = 4;
		stagingConfig.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		stagingConfig.memoryProperties =
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

		error = stagingBuffer.Initialize(device, stagingConfig);
		if (error != virasa::RenderError::None)
		{
			LOG_ERROR(logger,
				"White texture staging buffer initialization failed: {}",
				static_cast<int>(error));
			return -1;
		}

		error = stagingBuffer.Upload(whitePixel, 4);
		if (error != virasa::RenderError::None)
		{
			LOG_ERROR(
				logger, "White texture staging upload failed: {}", static_cast<int>(error));
			return -1;
		}

		VkCommandBufferAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocateInfo.commandPool = context.GetGraphicsCommandPool();
		allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocateInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		if (vkAllocateCommandBuffers(device.GetHandle(), &allocateInfo, &commandBuffer) !=
			VK_SUCCESS)
		{
			LOG_ERROR(logger, "Failed to allocate white texture upload command buffer.");
			return -1;
		}

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(commandBuffer, &beginInfo);

		VkImageMemoryBarrier toTransfer{};
		toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		toTransfer.image = whiteImage.GetHandle();
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

		VkBufferImageCopy copyRegion{};
		copyRegion.bufferOffset = 0;
		copyRegion.bufferRowLength = 0;
		copyRegion.bufferImageHeight = 0;
		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.mipLevel = 0;
		copyRegion.imageSubresource.baseArrayLayer = 0;
		copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageOffset = {0, 0, 0};
		copyRegion.imageExtent = {1, 1, 1};
		vkCmdCopyBufferToImage(commandBuffer,
			stagingBuffer.GetHandle(),
			whiteImage.GetHandle(),
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&copyRegion);

		VkImageMemoryBarrier toShaderRead{};
		toShaderRead.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		toShaderRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		toShaderRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		toShaderRead.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		toShaderRead.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		toShaderRead.image = whiteImage.GetHandle();
		toShaderRead.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		toShaderRead.subresourceRange.baseMipLevel = 0;
		toShaderRead.subresourceRange.levelCount = 1;
		toShaderRead.subresourceRange.baseArrayLayer = 0;
		toShaderRead.subresourceRange.layerCount = 1;
		toShaderRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		toShaderRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0,
			0,
			nullptr,
			0,
			nullptr,
			1,
			&toShaderRead);

		vkEndCommandBuffer(commandBuffer);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;
		vkQueueSubmit(device.GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(device.GetGraphicsQueue());
		vkFreeCommandBuffers(
			device.GetHandle(), context.GetGraphicsCommandPool(), 1, &commandBuffer);
	}

	virasa::Sampler sampler;
	{
		virasa::SamplerConfig samplerConfig;
		samplerConfig.magFilter = VK_FILTER_LINEAR;
		samplerConfig.minFilter = VK_FILTER_LINEAR;
		samplerConfig.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerConfig.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerConfig.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerConfig.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

		virasa::RenderError error = sampler.Initialize(device, samplerConfig);
		if (error != virasa::RenderError::None)
		{
			LOG_ERROR(logger, "Sampler::Initialize failed: {}", static_cast<int>(error));
			return -1;
		}
	}

	virasa::BindlessTextureArray bindlessTextureArray;
	{
		virasa::RenderError error = bindlessTextureArray.Initialize(device, 256);
		if (error != virasa::RenderError::None)
		{
			LOG_ERROR(logger,
				"BindlessTextureArray::Initialize failed: {}",
				static_cast<int>(error));
			return -1;
		}

		uint32_t slot =
			bindlessTextureArray.RegisterTexture(whiteImage.GetView(), sampler.GetHandle());
		if (slot == 0xFFFFFFFFu)
		{
			LOG_ERROR(logger, "RegisterTexture failed for default white texture.");
			return -1;
		}
		if (slot != 0u)
		{
			LOG_ERROR(logger, "Default white texture was not registered at slot 0.");
			return -1;
		}
	}

	virasa::VisualMaterialTable materialTable;
	uint32_t cubeMaterialId = 0xFFFFFFFFu;
	{
		virasa::RenderError error = materialTable.Initialize(device, 64);
		if (error != virasa::RenderError::None)
		{
			LOG_ERROR(logger,
				"VisualMaterialTable::Initialize failed: {}",
				static_cast<int>(error));
			return -1;
		}

		virasa::VisualMaterial material;
		material.factors.baseColorFactor = virasa::math::Vec4(0.9f, 0.3f, 0.2f, 1.0f);
		cubeMaterialId = materialTable.Allocate(material);
		if (cubeMaterialId == 0xFFFFFFFFu)
		{
			LOG_ERROR(logger, "VisualMaterialTable::Allocate failed.");
			return -1;
		}
		if (cubeMaterialId != 0u)
		{
			LOG_ERROR(logger, "Default material was not allocated at id 0.");
			return -1;
		}
	}

	virasa::ecs::World world;
	{
		virasa::ecs::Entity cubeEntity = world.CreateEntity();
		world.AddTransformComponent(cubeEntity, virasa::math::Transform::Identity());
		world.AddMeshComponent(cubeEntity, virasa::ecs::MeshComponent{cubeMeshId});
		world.AddVisualComponent(cubeEntity, virasa::ecs::VisualComponent{cubeMaterialId});
	}

	virasa::LightTable lightTable;
	{
		virasa::RenderError error = lightTable.Initialize(device, 256);
		if (error != virasa::RenderError::None)
		{
			LOG_ERROR(logger, "LightTable::Initialize failed: {}", static_cast<int>(error));
			return -1;
		}

		virasa::ecs::Entity lightEntity = world.CreateEntity();
		virasa::ecs::DirectionalLightComponent lightComponent;
		lightComponent.direction = virasa::math::Vec3(0.0f, 0.0f, -1.0f);
		lightComponent.color = virasa::math::Vec3(1.0f, 1.0f, 1.0f);
		lightComponent.intensity = 1.0f;
		world.AddDirectionalLightComponent(lightEntity, lightComponent);
	}

	virasa::renderer::graph::ImageRegistry imageRegistry;
	virasa::renderer::graph::BufferRegistry bufferRegistry;
	virasa::renderer::graph::Graph graph;
	{
		virasa::RenderError error = imageRegistry.Initialize(device);
		if (error != virasa::RenderError::None)
		{
			LOG_ERROR(
				logger, "ImageRegistry::Initialize failed: {}", static_cast<int>(error));
			return -1;
		}

		error = bufferRegistry.Initialize(device);
		if (error != virasa::RenderError::None)
		{
			LOG_ERROR(
				logger, "BufferRegistry::Initialize failed: {}", static_cast<int>(error));
			return -1;
		}

		error = graph.Initialize(device, imageRegistry, bufferRegistry);
		if (error != virasa::RenderError::None)
		{
			LOG_ERROR(logger, "Graph::Initialize failed: {}", static_cast<int>(error));
			return -1;
		}
	}

	virasa::ShaderModule vertexShader;
	virasa::ShaderModule fragmentShader;
	{
		virasa::RenderError error = vertexShader.Initialize(device, "shaders/cube.vert.spv");
		if (error != virasa::RenderError::None)
		{
			LOG_ERROR(
				logger, "Vertex shader initialization failed: {}", static_cast<int>(error));
			return -1;
		}

		error = fragmentShader.Initialize(device, "shaders/cube.frag.spv");
		if (error != virasa::RenderError::None)
		{
			LOG_ERROR(logger,
				"Fragment shader initialization failed: {}",
				static_cast<int>(error));
			return -1;
		}
	}

	virasa::math::Mat4 viewMatrix = virasa::math::LookAtRH_ZUp(
		virasa::math::Vec3(2.0f, 2.0f, 2.0f), virasa::math::Vec3(0.0f, 0.0f, 0.0f));
	VkExtent2D swapchainExtent = context.GetSwapchainExtent();
	virasa::math::Mat4 projectionMatrix = virasa::math::PerspectiveRH_ZO(glm::radians(45.0f),
		static_cast<float>(swapchainExtent.width) / static_cast<float>(swapchainExtent.height),
		0.1f,
		10.0f);

	virasa::Pipeline cubePipeline;
	{
		virasa::PipelineBuilder builder;
		builder.SetVertexShader(vertexShader)
			.SetFragmentShader(fragmentShader)
			.SetColorFormat(context.GetSwapchainFormat())
			.SetDepthFormat(context.GetDepthFormat())
			.SetExtent(context.GetSwapchainExtent())
			.SetCullMode(VK_CULL_MODE_BACK_BIT)
			.SetFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
			.SetDepthTest(true)
			.SetDepthWrite(true)
			.SetDepthCompareOp(VK_COMPARE_OP_LESS)
			.AddPushConstantRange(
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 168)
			.AddDescriptorSetLayout(bindlessTextureArray.GetLayout());

		virasa::RenderError error = builder.Build(device, cubePipeline);
		if (error != virasa::RenderError::None)
		{
			LOG_ERROR(logger, "PipelineBuilder::Build failed: {}", static_cast<int>(error));
			return -1;
		}
	}

	for (;;)
	{
		std::span<const virasa::Event> events = platform.PollEvents();
		bool shouldExit = false;
		for (const virasa::Event& event : events)
		{
			if (event.type == virasa::EventType::Quit)
			{
				LOG_INFO(logger, "Quit event received, exiting.");
				shouldExit = true;
				break;
			}
			if (event.type == virasa::EventType::KeyDown &&
				event.keyboard.key == virasa::KeyCode::Escape)
			{
				LOG_INFO(logger, "Escape key pressed, exiting.");
				shouldExit = true;
				break;
			}
		}

		if (shouldExit)
		{
			context.GetDevice().WaitIdle();
			return 0;
		}

		virasa::SwapchainStatus frameStatus = context.BeginFrame();
		if (frameStatus == virasa::SwapchainStatus::NotReady)
		{
			continue;
		}
		if (frameStatus == virasa::SwapchainStatus::Error)
		{
			LOG_ERROR(logger, "Context::BeginFrame returned Error.");
			return -1;
		}

		std::vector<virasa::LightGPU> frameLights;
		for (const virasa::ecs::Entity& entity : world.GetDirectionalLightComponentEntities())
		{
			const virasa::ecs::DirectionalLightComponent& component =
				world.GetDirectionalLightComponent(entity);
			virasa::LightGPU light{};
			light.type = static_cast<uint32_t>(virasa::LightType::Directional);
			light.direction = glm::normalize(component.direction);
			light.color = component.color * component.intensity;
			frameLights.push_back(light);
		}

		for (const virasa::ecs::Entity& entity : world.GetPointLightComponentEntities())
		{
			if (!world.HasTransformComponent(entity))
			{
				continue;
			}

			const virasa::ecs::PointLightComponent& component =
				world.GetPointLightComponent(entity);
			const virasa::math::Transform& transform = world.GetTransformComponent(entity);
			virasa::LightGPU light{};
			light.type = static_cast<uint32_t>(virasa::LightType::Point);
			light.position = transform.translation;
			light.range = component.range;
			light.color = component.color * component.intensity;
			frameLights.push_back(light);
		}

		for (const virasa::ecs::Entity& entity : world.GetSpotLightComponentEntities())
		{
			if (!world.HasTransformComponent(entity))
			{
				continue;
			}

			const virasa::ecs::SpotLightComponent& component =
				world.GetSpotLightComponent(entity);
			const virasa::math::Transform& transform = world.GetTransformComponent(entity);
			virasa::LightGPU light{};
			light.type = static_cast<uint32_t>(virasa::LightType::Spot);
			light.position = transform.translation;
			light.direction =
				glm::normalize(transform.rotation * virasa::math::Vec3(0.0f, 1.0f, 0.0f));
			light.range = component.range;
			light.color = component.color * component.intensity;
			light.innerConeCos = component.innerConeCos;
			light.outerConeCos = component.outerConeCos;
			frameLights.push_back(light);
		}

		lightTable.UploadFrame(
			std::span<const virasa::LightGPU>(frameLights.data(), frameLights.size()));

		graph.Begin();

		virasa::renderer::graph::ImageHandle swapchainHandle =
			graph.ImportImage(context.GetCurrentImage(),
				context.GetCurrentImageView(),
				context.GetSwapchainFormat(),
				context.GetSwapchainExtent(),
				virasa::renderer::graph::ResourceUsage::Undefined);

		virasa::renderer::graph::ImageHandle depthHandle =
			graph.ImportImage(context.GetDepthImage(),
				context.GetDepthImageView(),
				context.GetDepthFormat(),
				context.GetSwapchainExtent(),
				virasa::renderer::graph::ResourceUsage::Undefined);

		graph.AddPass("forward")
			.ColorAttachment(swapchainHandle,
				virasa::renderer::graph::LoadOp::Clear,
				virasa::renderer::graph::ClearColor{0.1f, 0.1f, 0.15f, 1.0f})
			.DepthAttachment(depthHandle, virasa::renderer::graph::LoadOp::Clear, 1.0f)
			.Record(
				[&world,
					&meshRegistry,
					&materialTable,
					&lightTable,
					&bindlessTextureArray,
					&cubePipeline,
					&viewMatrix,
					&projectionMatrix,
					&device](const virasa::renderer::graph::GraphContext& graphContext)
				{
					VkCommandBuffer commandBuffer = graphContext.GetCommandBuffer();
					VkExtent2D renderExtent = graphContext.GetRenderExtent();

					VkViewport viewport{};
					viewport.x = 0.0f;
					viewport.y = 0.0f;
					viewport.width = static_cast<float>(renderExtent.width);
					viewport.height = static_cast<float>(renderExtent.height);
					viewport.minDepth = 0.0f;
					viewport.maxDepth = 1.0f;
					vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

					VkRect2D scissor{};
					scissor.offset = {0, 0};
					scissor.extent = renderExtent;
					vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

					cubePipeline.Bind(commandBuffer);

					VkDescriptorSet descriptorSet =
						bindlessTextureArray.GetDescriptorSet();
					vkCmdBindDescriptorSets(commandBuffer,
						VK_PIPELINE_BIND_POINT_GRAPHICS,
						cubePipeline.GetLayout(),
						0,
						1,
						&descriptorSet,
						0,
						nullptr);

					for (const virasa::ecs::Entity& entity :
						world.GetVisualComponentEntities())
					{
						if (!world.HasMeshComponent(entity))
						{
							continue;
						}

						const virasa::ecs::MeshComponent& meshComponent =
							world.GetMeshComponent(entity);
						if (!meshRegistry.IsAllocated(meshComponent.meshId))
						{
							continue;
						}

						const virasa::Mesh& mesh =
							meshRegistry.Get(meshComponent.meshId);
						virasa::math::Mat4 modelMatrix(1.0f);
						if (world.HasTransformComponent(entity))
						{
							modelMatrix =
								world.GetTransformComponent(entity).ToMatrix();
						}

						uint32_t materialId =
							world.GetVisualComponent(entity).materialId;
						PushConstants pushConstants{};
						pushConstants.mvp = projectionMatrix * viewMatrix * modelMatrix;
						pushConstants.model = modelMatrix;
						pushConstants.vertexBufferAddress =
							mesh.GetVertexBufferAddress(device);
						pushConstants.indexBufferAddress =
							mesh.GetIndexBufferAddress(device);
						pushConstants.materialBufferAddress =
							materialTable.GetBufferAddress();
						pushConstants.lightBufferAddress =
							lightTable.GetBufferAddress();
						pushConstants.materialId = materialId;
						pushConstants.lightCount = lightTable.GetLightCount();

						vkCmdPushConstants(commandBuffer,
							cubePipeline.GetLayout(),
							VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
							0,
							sizeof(PushConstants),
							&pushConstants);

						vkCmdDraw(commandBuffer, mesh.GetIndexCount(), 1, 0, 0);
					}
				});

		virasa::RenderError compileError = graph.Compile();
		if (compileError != virasa::RenderError::None)
		{
			LOG_ERROR(logger, "Graph::Compile failed: {}", static_cast<int>(compileError));
			return -1;
		}

		virasa::RenderError executeError = graph.Execute(context.GetCurrentCommandBuffer());
		if (executeError != virasa::RenderError::None)
		{
			LOG_ERROR(logger, "Graph::Execute failed: {}", static_cast<int>(executeError));
			return -1;
		}

		graph.End();

		virasa::SwapchainStatus endStatus = context.EndFrame();
		if (endStatus == virasa::SwapchainStatus::Error)
		{
			LOG_ERROR(logger, "Context::EndFrame returned Error.");
			return -1;
		}
	}
}
