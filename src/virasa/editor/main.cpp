// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Anton Soderqvist

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include <vulkan/vulkan.h>

#include <quill/LogMacros.h>
#include <quill/Logger.h>

#include <array>
#include <cstdint>
#include <glm/gtc/matrix_transform.hpp>
#include <span>

#include "virasa/core/Logger.h"
#include "virasa/math/Projection.h"
#include "virasa/math/Types.h"
#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Context.h"
#include "virasa/renderer/core/Device.h"
#include "virasa/renderer/geometry/Primitives.h"
#include "virasa/renderer/resources/Mesh.h"
#include "virasa/renderer/resources/Pipeline.h"
#include "virasa/renderer/resources/ShaderModule.h"
#include "virasa/window/Events.h"
#include "virasa/window/Platform.h"
#include "virasa/window/Types.h"

int main(int argc, char** argv)
{
	(void)argc;
	(void)argv;

	quill::Logger* logger = virasa::Logger::GetLogger("Main");
	LOG_INFO(logger, "Entry point reached.");

	virasa::window::Platform platform;
	virasa::ErrorCode platformResult = platform.Initialize("Virasa Editor", 800, 800);
	if (platformResult != virasa::ErrorCode::None)
	{
		LOG_ERROR(logger,
			"Platform initialization failed with error code: {}",
			static_cast<int>(platformResult));
		return -1;
	}

	virasa::Context context;
	virasa::RendererConfig rendererConfig;
	uint32_t extensionCount = 0;
	rendererConfig.requiredInstanceExtensions =
		virasa::window::Platform::GetRequiredVulkanExtensions(&extensionCount);
	rendererConfig.requiredInstanceExtensionCount = extensionCount;

	virasa::RenderError contextResult = context.Initialize(platform, rendererConfig);
	if (contextResult != virasa::RenderError::None)
	{
		LOG_ERROR(logger,
			"Renderer context initialization failed with error code: {}",
			static_cast<int>(contextResult));
		return -1;
	}

	virasa::MeshData cubeData = virasa::CreateCube();
	virasa::Mesh mesh;
	virasa::RenderError meshResult = mesh.Initialize(context.GetDevice(), context, cubeData);
	if (meshResult != virasa::RenderError::None)
	{
		LOG_ERROR(logger,
			"Mesh initialization failed with error code: {}",
			static_cast<int>(meshResult));
		return -1;
	}

	virasa::ShaderModule vertShader;
	virasa::RenderError vertResult =
		vertShader.Initialize(context.GetDevice(), "shaders/cube.vert.spv");
	if (vertResult != virasa::RenderError::None)
	{
		LOG_ERROR(logger,
			"Failed to initialize vertex shader shaders/cube.vert.spv with error code: {}",
			static_cast<int>(vertResult));
		return -1;
	}

	virasa::ShaderModule fragShader;
	virasa::RenderError fragResult =
		fragShader.Initialize(context.GetDevice(), "shaders/cube.frag.spv");
	if (fragResult != virasa::RenderError::None)
	{
		LOG_ERROR(logger,
			"Failed to initialize fragment shader shaders/cube.frag.spv with error code: {}",
			static_cast<int>(fragResult));
		return -1;
	}

	VkExtent2D swapExtent = context.GetSwapchainExtent();
	virasa::math::Mat4 model = virasa::math::Mat4(1.0f);
	virasa::math::Mat4 view = virasa::math::LookAtRH_ZUp(
		virasa::math::Vec3(2.0f, 2.0f, 2.0f), virasa::math::Vec3(0.0f, 0.0f, 0.0f));
	float aspect = static_cast<float>(swapExtent.width) / static_cast<float>(swapExtent.height);
	virasa::math::Mat4 proj =
		virasa::math::PerspectiveRH_ZO(glm::radians(45.0f), aspect, 0.1f, 10.0f);
	virasa::math::Mat4 mvp = proj * view * model;

	virasa::Pipeline pipeline;
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
		.AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, 80);

	virasa::RenderError pipelineResult = builder.Build(context.GetDevice(), pipeline);
	if (pipelineResult != virasa::RenderError::None)
	{
		LOG_ERROR(logger,
			"Pipeline build failed with error code: {}",
			static_cast<int>(pipelineResult));
		return -1;
	}

	struct PushConstants
	{
		virasa::math::Mat4 mvp;
		VkDeviceAddress vertexBufferAddress;
		VkDeviceAddress indexBufferAddress;
	};

	while (true)
	{
		std::span<const virasa::Event> events = platform.PollEvents();

		for (const virasa::Event& event : events)
		{
			if (event.type == virasa::EventType::Quit)
			{
				LOG_INFO(logger, "Quit event received, exiting.");
				context.GetDevice().WaitIdle();
				return 0;
			}

			if (event.type == virasa::EventType::KeyDown &&
				event.keyboard.key == virasa::KeyCode::Escape)
			{
				LOG_INFO(logger, "Escape pressed, exiting.");
				context.GetDevice().WaitIdle();
				return 0;
			}
		}

		virasa::SwapchainStatus beginStatus = context.BeginFrame();
		if (beginStatus == virasa::SwapchainStatus::NotReady)
		{
			continue;
		}
		if (beginStatus == virasa::SwapchainStatus::Error)
		{
			LOG_ERROR(logger, "BeginFrame returned an error, exiting.");
			return -1;
		}

		VkCommandBuffer cmd = context.GetCurrentCommandBuffer();
		VkImage currentImage = context.GetCurrentImage();
		VkImageView currentImageView = context.GetCurrentImageView();
		VkImageView depthImageView = context.GetDepthImageView();
		VkExtent2D extent = context.GetSwapchainExtent();

		{
			VkImageMemoryBarrier colorBarrier{};
			colorBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			colorBarrier.srcAccessMask = 0;
			colorBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			colorBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			colorBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			colorBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			colorBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			colorBarrier.image = currentImage;
			colorBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			colorBarrier.subresourceRange.baseMipLevel = 0;
			colorBarrier.subresourceRange.levelCount = 1;
			colorBarrier.subresourceRange.baseArrayLayer = 0;
			colorBarrier.subresourceRange.layerCount = 1;

			vkCmdPipelineBarrier(cmd,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				0,
				0,
				nullptr,
				0,
				nullptr,
				1,
				&colorBarrier);
		}

		{
			VkClearValue colorClear{};
			colorClear.color = {{0.1f, 0.1f, 0.15f, 1.0f}};

			VkRenderingAttachmentInfo colorAttachment{};
			colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			colorAttachment.imageView = currentImageView;
			colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			colorAttachment.clearValue = colorClear;

			VkClearValue depthClear{};
			depthClear.depthStencil.depth = 1.0f;

			VkRenderingAttachmentInfo depthAttachment{};
			depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			depthAttachment.imageView = depthImageView;
			depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
			depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			depthAttachment.clearValue = depthClear;

			VkRenderingInfo renderingInfo{};
			renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
			renderingInfo.renderArea.offset = {0, 0};
			renderingInfo.renderArea.extent = extent;
			renderingInfo.layerCount = 1;
			renderingInfo.viewMask = 0;
			renderingInfo.colorAttachmentCount = 1;
			renderingInfo.pColorAttachments = &colorAttachment;
			renderingInfo.pDepthAttachment = &depthAttachment;
			renderingInfo.pStencilAttachment = nullptr;

			vkCmdBeginRendering(cmd, &renderingInfo);
		}

		{
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
		}

		pipeline.Bind(cmd);

		PushConstants pushConstants{
			.mvp = mvp,
			.vertexBufferAddress = mesh.GetVertexBufferAddress(context.GetDevice()),
			.indexBufferAddress = mesh.GetIndexBufferAddress(context.GetDevice()),
		};
		vkCmdPushConstants(
			cmd, pipeline.GetLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, 80, &pushConstants);

		vkCmdDraw(cmd, mesh.GetIndexCount(), 1, 0, 0);

		vkCmdEndRendering(cmd);

		{
			VkImageMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			barrier.dstAccessMask = 0;
			barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = currentImage;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.levelCount = 1;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;

			vkCmdPipelineBarrier(cmd,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
				0,
				0,
				nullptr,
				0,
				nullptr,
				1,
				&barrier);
		}

		virasa::SwapchainStatus endStatus = context.EndFrame();
		if (endStatus == virasa::SwapchainStatus::Error)
		{
			LOG_ERROR(logger, "EndFrame returned an error, exiting.");
			return -1;
		}
	}
}
