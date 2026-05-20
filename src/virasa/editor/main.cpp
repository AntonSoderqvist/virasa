// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Anton Soderqvist

#include <vulkan/vulkan.h>

#include <quill/LogMacros.h>
#include <quill/Logger.h>

#include <span>

#include "virasa/core/Logger.h"
#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Context.h"
#include "virasa/renderer/resources/Pipeline.h"
#include "virasa/renderer/resources/ShaderModule.h"
#include "virasa/window/Events.h"
#include "virasa/window/Platform.h"
#include "virasa/window/Types.h"

int main(int argc, char** argv)
{
	quill::Logger* logger = virasa::Logger::GetLogger("Main");
	LOG_INFO(logger, "Entry point reached.");

	// --- Platform ---
	virasa::window::Platform platform;
	virasa::ErrorCode platformResult = platform.Initialize("Virasa", 800, 800);
	if (platformResult != virasa::ErrorCode::None)
	{
		LOG_ERROR(logger,
			"Platform initialization failed with error code: {}",
			static_cast<int>(platformResult));
		return -1;
	}

	// --- Renderer Context ---
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

	// --- Shaders ---
	virasa::ShaderModule vertShader;
	virasa::RenderError vertResult =
		vertShader.Initialize(context.GetDevice(), "shaders/triangle.vert.spv");
	if (vertResult != virasa::RenderError::None)
	{
		LOG_ERROR(logger,
			"Vertex shader initialization failed with error code: {}",
			static_cast<int>(vertResult));
		return -1;
	}

	virasa::ShaderModule fragShader;
	virasa::RenderError fragResult =
		fragShader.Initialize(context.GetDevice(), "shaders/triangle.frag.spv");
	if (fragResult != virasa::RenderError::None)
	{
		LOG_ERROR(logger,
			"Fragment shader initialization failed with error code: {}",
			static_cast<int>(fragResult));
		return -1;
	}

	// --- Pipeline ---
	virasa::Pipeline pipeline;
	virasa::PipelineBuilder builder;
	builder.SetVertexShader(vertShader)
		.SetFragmentShader(fragShader)
		.SetColorFormat(context.GetSwapchainFormat())
		.SetExtent(context.GetSwapchainExtent());
	// No vertex layout: triangle vertices generated in vertex shader from gl_VertexIndex.
	// No depth testing, depth writing, stencil testing, or blending.

	virasa::RenderError pipelineResult = builder.Build(context.GetDevice(), pipeline);
	if (pipelineResult != virasa::RenderError::None)
	{
		LOG_ERROR(logger,
			"Pipeline build failed with error code: {}",
			static_cast<int>(pipelineResult));
		return -1;
	}

	// --- Main Loop ---
	while (true)
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
				LOG_INFO(logger, "Escape pressed, exiting.");
				shouldExit = true;
				break;
			}
		}

		if (shouldExit)
		{
			break;
		}

		// --- Render Frame ---
		virasa::SwapchainStatus beginStatus = context.BeginFrame();
		if (beginStatus == virasa::SwapchainStatus::NotReady)
		{
			// Window is minimized or swapchain not usable; skip this frame.
			continue;
		}
		if (beginStatus == virasa::SwapchainStatus::Error)
		{
			LOG_ERROR(logger, "BeginFrame returned an error, exiting.");
			return -1;
		}

		// BeginFrame returned Success or Recreated — record commands.
		VkCommandBuffer cmd = context.GetCurrentCommandBuffer();
		VkImage currentImage = context.GetCurrentImage();
		VkImageView currentImageView = context.GetCurrentImageView();
		VkExtent2D extent = context.GetSwapchainExtent();

		// Transition swapchain image to color attachment layout.
		{
			VkImageMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = currentImage;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.levelCount = 1;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;

			vkCmdPipelineBarrier(cmd,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				0,
				0,
				nullptr,
				0,
				nullptr,
				1,
				&barrier);
		}

		// Begin dynamic rendering.
		{
			VkClearValue clearValue{};
			clearValue.color = {{0.1f, 0.1f, 0.15f, 1.0f}};

			VkRenderingAttachmentInfo colorAttachment{};
			colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			colorAttachment.imageView = currentImageView;
			colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			colorAttachment.clearValue = clearValue;

			VkRenderingInfo renderingInfo{};
			renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
			renderingInfo.renderArea.offset = {0, 0};
			renderingInfo.renderArea.extent = extent;
			renderingInfo.layerCount = 1;
			renderingInfo.colorAttachmentCount = 1;
			renderingInfo.pColorAttachments = &colorAttachment;
			renderingInfo.pDepthAttachment = nullptr;
			renderingInfo.pStencilAttachment = nullptr;

			vkCmdBeginRendering(cmd, &renderingInfo);
		}

		// Set dynamic viewport and scissor.
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

		// Bind pipeline and draw triangle.
		pipeline.Bind(cmd);
		vkCmdDraw(cmd, 3, 1, 0, 0);

		// End dynamic rendering.
		vkCmdEndRendering(cmd);

		// Transition swapchain image to present layout.
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
		// Recreated is handled inside Context; we simply continue.
	}

	return 0;
}
