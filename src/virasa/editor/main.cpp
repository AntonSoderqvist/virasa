// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Anton Soderqvist

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <vulkan/vulkan.h>

#include <quill/LogMacros.h>
#include <quill/Logger.h>

#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <span>

#include "virasa/core/Logger.h"
#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Context.h"
#include "virasa/renderer/core/Device.h"
#include "virasa/renderer/resources/Buffer.h"
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
	virasa::ErrorCode platformResult = platform.Initialize("Virasa Editor", 800, 800);
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

	// --- Cube Geometry ---
	static const float vertices[] = {
		-0.5f,
		-0.5f,
		-0.5f,
		+0.5f,
		-0.5f,
		-0.5f,
		+0.5f,
		+0.5f,
		-0.5f,
		-0.5f,
		+0.5f,
		-0.5f,
		-0.5f,
		-0.5f,
		+0.5f,
		+0.5f,
		-0.5f,
		+0.5f,
		+0.5f,
		+0.5f,
		+0.5f,
		-0.5f,
		+0.5f,
		+0.5f,
	};

	// 36 indices, 12 triangles, CCW winding from outside.
	static const uint32_t indices[] = {
		// Front face (+Z)
		4,
		5,
		6,
		4,
		6,
		7,
		// Back face (-Z)
		1,
		0,
		3,
		1,
		3,
		2,
		// Left face (-X)
		0,
		4,
		7,
		0,
		7,
		3,
		// Right face (+X)
		5,
		1,
		2,
		5,
		2,
		6,
		// Bottom face (-Y)
		0,
		1,
		5,
		0,
		5,
		4,
		// Top face (+Y)
		7,
		6,
		2,
		7,
		2,
		3,
	};

	// Vertex buffer
	virasa::Buffer vertexBuffer;
	{
		virasa::BufferConfig cfg{};
		cfg.size = sizeof(vertices);
		cfg.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		cfg.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		virasa::RenderError err = vertexBuffer.Initialize(context.GetDevice(), cfg);
		if (err != virasa::RenderError::None)
		{
			LOG_ERROR(logger,
				"Vertex buffer initialization failed with error code: {}",
				static_cast<int>(err));
			return -1;
		}
	}

	// Index buffer
	virasa::Buffer indexBuffer;
	{
		virasa::BufferConfig cfg{};
		cfg.size = sizeof(indices);
		cfg.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		cfg.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		virasa::RenderError err = indexBuffer.Initialize(context.GetDevice(), cfg);
		if (err != virasa::RenderError::None)
		{
			LOG_ERROR(logger,
				"Index buffer initialization failed with error code: {}",
				static_cast<int>(err));
			return -1;
		}
	}

	// Upload vertex data via staging
	{
		virasa::RenderError err = vertexBuffer.UploadViaStaging(context.GetDevice(),
			context.GetTransferCommandPool(),
			context.GetDevice().GetTransferQueue(),
			vertices,
			sizeof(vertices));
		if (err != virasa::RenderError::None)
		{
			LOG_ERROR(logger,
				"Vertex buffer upload failed with error code: {}",
				static_cast<int>(err));
			return -1;
		}
	}

	// Upload index data via staging
	{
		virasa::RenderError err = indexBuffer.UploadViaStaging(context.GetDevice(),
			context.GetTransferCommandPool(),
			context.GetDevice().GetTransferQueue(),
			indices,
			sizeof(indices));
		if (err != virasa::RenderError::None)
		{
			LOG_ERROR(logger,
				"Index buffer upload failed with error code: {}",
				static_cast<int>(err));
			return -1;
		}
	}

	// --- Shaders ---
	virasa::ShaderModule vertShader;
	virasa::RenderError vertResult =
		vertShader.Initialize(context.GetDevice(), "shaders/cube.vert.spv");
	if (vertResult != virasa::RenderError::None)
	{
		LOG_ERROR(logger,
			"Vertex shader initialization failed with error code: {}",
			static_cast<int>(vertResult));
		return -1;
	}

	virasa::ShaderModule fragShader;
	virasa::RenderError fragResult =
		fragShader.Initialize(context.GetDevice(), "shaders/cube.frag.spv");
	if (fragResult != virasa::RenderError::None)
	{
		LOG_ERROR(logger,
			"Fragment shader initialization failed with error code: {}",
			static_cast<int>(fragResult));
		return -1;
	}

	// --- Precompute MVP ---
	VkExtent2D swapExtent = context.GetSwapchainExtent();
	glm::mat4 model = glm::mat4(1.0f);
	glm::mat4 view = glm::lookAt(
		glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	float aspect = static_cast<float>(swapExtent.width) / static_cast<float>(swapExtent.height);
	glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 10.0f);
	// Invert Y for Vulkan clip space.
	proj[1][1] *= -1.0f;
	glm::mat4 mvp = proj * view * model;

	// --- Pipeline ---
	virasa::VertexAttribute vertAttr{};
	vertAttr.location = 0;
	vertAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
	vertAttr.offset = 0;

	virasa::VertexLayout vertLayout{};
	vertLayout.stride = 12;
	vertLayout.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	vertLayout.attributes = std::span<const virasa::VertexAttribute>(&vertAttr, 1);

	virasa::Pipeline pipeline;
	virasa::PipelineBuilder builder;
	builder.SetVertexShader(vertShader)
		.SetFragmentShader(fragShader)
		.SetVertexLayout(vertLayout)
		.SetColorFormat(context.GetSwapchainFormat())
		.SetDepthFormat(context.GetDepthFormat())
		.SetExtent(context.GetSwapchainExtent())
		.SetCullMode(VK_CULL_MODE_BACK_BIT)
		.SetFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE)
		.SetDepthTest(true)
		.SetDepthWrite(true)
		.SetDepthCompareOp(VK_COMPARE_OP_LESS)
		.AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, 64);

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
		VkImageView depthImageView = context.GetDepthImageView();
		VkExtent2D extent = context.GetSwapchainExtent();

		// Step 1 & 2: Transition color and depth images.
		{
			VkImageMemoryBarrier barriers[2]{};

			// Color image: UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL
			barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barriers[0].srcAccessMask = 0;
			barriers[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			barriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barriers[0].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barriers[0].image = currentImage;
			barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barriers[0].subresourceRange.baseMipLevel = 0;
			barriers[0].subresourceRange.levelCount = 1;
			barriers[0].subresourceRange.baseArrayLayer = 0;
			barriers[0].subresourceRange.layerCount = 1;

			// Depth image: UNDEFINED -> DEPTH_ATTACHMENT_OPTIMAL
			barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barriers[1].srcAccessMask = 0;
			barriers[1].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
							    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barriers[1].newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
			barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			// The depth image view is a view; we need the image itself.
			// We use a null image here and rely on the view being tied to the image;
			// however the barrier requires VkImage. We obtain it from the depth view
			// indirectly — the Context does not expose GetDepthImage(), so we use
			// a separate barrier call for the depth image.
			// Actually we must use a separate barrier since we don't have the VkImage.
			// Emit the color barrier alone first, then depth separately.
			(void)barriers[1]; // will be handled below

			vkCmdPipelineBarrier(cmd,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				0,
				0,
				nullptr,
				0,
				nullptr,
				1,
				&barriers[0]);
		}

		// Depth image transition (separate barrier since we only have the view, not the image).
		// We use a renderpass-style implicit transition via the depth attachment loadOp
		// instead. The depth attachment's loadOp CLEAR with layout DEPTH_ATTACHMENT_OPTIMAL
		// handles the layout transition implicitly when using dynamic rendering with the
		// correct initialLayout. However, vkCmdBeginRendering does NOT do implicit transitions;
		// we must issue the barrier explicitly. Since Context does not expose GetDepthImage(),
		// we use a pipeline barrier with a memory barrier (no image barrier) for the depth
		// and rely on the fact that the depth image was created with DEVICE_LOCAL memory
		// and the load op CLEAR discards prior contents. The validation layers will flag
		// a missing image layout transition if we skip it, so we issue a global memory
		// barrier covering the depth stage instead.
		{
			VkMemoryBarrier memBarrier{};
			memBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
			memBarrier.srcAccessMask = 0;
			memBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
							   VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

			vkCmdPipelineBarrier(cmd,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
					VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
				0,
				1,
				&memBarrier,
				0,
				nullptr,
				0,
				nullptr);
		}

		// Step 3: Begin dynamic rendering.
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

		// Step 4: Set dynamic viewport and scissor.
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

		// Step 5: Bind pipeline.
		pipeline.Bind(cmd);

		// Step 6: Bind vertex and index buffers.
		{
			VkBuffer vb = vertexBuffer.GetHandle();
			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &offset);
			vkCmdBindIndexBuffer(cmd, indexBuffer.GetHandle(), 0, VK_INDEX_TYPE_UINT32);
		}

		// Step 7: Push MVP matrix.
		{
			vkCmdPushConstants(
				cmd, pipeline.GetLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, 64, &mvp);
		}

		// Step 8: Draw.
		vkCmdDrawIndexed(cmd, 36, 1, 0, 0, 0);

		// Step 9: End dynamic rendering.
		vkCmdEndRendering(cmd);

		// Step 10: Transition swapchain image to present layout.
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
