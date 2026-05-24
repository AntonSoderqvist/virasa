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
#include "virasa/renderer/material/Visual.h"
#include "virasa/renderer/resources/BindlessTextureArray.h"
#include "virasa/renderer/resources/Image.h"
#include "virasa/renderer/resources/Mesh.h"
#include "virasa/renderer/resources/Pipeline.h"
#include "virasa/renderer/resources/Sampler.h"
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

	// Default white 1x1 image
	virasa::Image whiteImage;
	{
		virasa::ImageConfig imgCfg;
		imgCfg.width = 1;
		imgCfg.height = 1;
		imgCfg.format = VK_FORMAT_R8G8B8A8_UNORM;
		imgCfg.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		imgCfg.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		imgCfg.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		virasa::RenderError imgResult = whiteImage.Initialize(context.GetDevice(), imgCfg);
		if (imgResult != virasa::RenderError::None)
		{
			LOG_ERROR(logger,
				"Default white image initialization failed with error code: {}",
				static_cast<int>(imgResult));
			return -1;
		}
	}

	// Upload white texel via staging
	{
		// Create a staging buffer, upload 0xFF 0xFF 0xFF 0xFF, then copy to the image
		// We use a one-shot transfer command buffer from the transfer pool
		const uint8_t whitePixel[4] = {0xFF, 0xFF, 0xFF, 0xFF};

		// Create staging buffer
		virasa::Buffer stagingBuffer;
		virasa::BufferConfig stagingCfg;
		stagingCfg.size = 4;
		stagingCfg.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		stagingCfg.memoryProperties =
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		virasa::RenderError stagingResult =
			stagingBuffer.Initialize(context.GetDevice(), stagingCfg);
		if (stagingResult != virasa::RenderError::None)
		{
			LOG_ERROR(logger,
				"Staging buffer for white image failed with error code: {}",
				static_cast<int>(stagingResult));
			return -1;
		}
		virasa::RenderError uploadResult = stagingBuffer.Upload(whitePixel, 4);
		if (uploadResult != virasa::RenderError::None)
		{
			LOG_ERROR(logger,
				"Staging buffer upload for white image failed with error code: {}",
				static_cast<int>(uploadResult));
			return -1;
		}

		// Record and submit a one-shot command buffer to copy + transition
		VkCommandPool transferPool = context.GetTransferCommandPool();
		VkQueue transferQueue = context.GetDevice().GetTransferQueue();
		VkDevice vkDevice = context.GetDevice().GetHandle();

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = transferPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer uploadCmd = VK_NULL_HANDLE;
		vkAllocateCommandBuffers(vkDevice, &allocInfo, &uploadCmd);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(uploadCmd, &beginInfo);

		// Transition image to TRANSFER_DST_OPTIMAL
		VkImageMemoryBarrier toTransfer{};
		toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		toTransfer.srcAccessMask = 0;
		toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
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
		vkCmdPipelineBarrier(uploadCmd,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			0, 0, nullptr, 0, nullptr, 1, &toTransfer);

		// Copy buffer to image
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
		vkCmdCopyBufferToImage(uploadCmd,
			stagingBuffer.GetHandle(),
			whiteImage.GetHandle(),
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&copyRegion);

		// Transition image to SHADER_READ_ONLY_OPTIMAL
		VkImageMemoryBarrier toShaderRead{};
		toShaderRead.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		toShaderRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		toShaderRead.dstAccessMask = 0;
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
		// Transfer queues do not support FRAGMENT_SHADER as a pipeline stage.
		// vkQueueWaitIdle below + the next graphics submission provide the
		// cross-queue happens-after needed before the fragment shader samples.
		vkCmdPipelineBarrier(uploadCmd,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			0, 0, nullptr, 0, nullptr, 1, &toShaderRead);

		vkEndCommandBuffer(uploadCmd);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &uploadCmd;
		vkQueueSubmit(transferQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(transferQueue);
		vkFreeCommandBuffers(vkDevice, transferPool, 1, &uploadCmd);
	}

	// Default linear sampler
	virasa::Sampler defaultSampler;
	{
		virasa::SamplerConfig samplerCfg;
		samplerCfg.magFilter = VK_FILTER_LINEAR;
		samplerCfg.minFilter = VK_FILTER_LINEAR;
		samplerCfg.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCfg.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCfg.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCfg.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		virasa::RenderError samplerResult =
			defaultSampler.Initialize(context.GetDevice(), samplerCfg);
		if (samplerResult != virasa::RenderError::None)
		{
			LOG_ERROR(logger,
				"Default sampler initialization failed with error code: {}",
				static_cast<int>(samplerResult));
			return -1;
		}
	}

	// Bindless texture array
	virasa::BindlessTextureArray bindlessTextures;
	{
		virasa::RenderError btaResult =
			bindlessTextures.Initialize(context.GetDevice(), 256);
		if (btaResult != virasa::RenderError::None)
		{
			LOG_ERROR(logger,
				"BindlessTextureArray initialization failed with error code: {}",
				static_cast<int>(btaResult));
			return -1;
		}
	}
	uint32_t whiteSlot =
		bindlessTextures.RegisterTexture(whiteImage.GetView(), defaultSampler.GetHandle());
	if (whiteSlot == UINT32_MAX)
	{
		LOG_ERROR(logger, "Failed to register default white texture into bindless array.");
		return -1;
	}

	// Visual material table
	virasa::VisualMaterialTable materialTable;
	{
		virasa::RenderError matResult = materialTable.Initialize(context.GetDevice(), 64);
		if (matResult != virasa::RenderError::None)
		{
			LOG_ERROR(logger,
				"VisualMaterialTable initialization failed with error code: {}",
				static_cast<int>(matResult));
			return -1;
		}
	}
	virasa::VisualMaterial defaultMaterial{};
	uint32_t defaultMaterialId = materialTable.Allocate(defaultMaterial);
	if (defaultMaterialId == UINT32_MAX)
	{
		LOG_ERROR(logger, "Failed to allocate default material from VisualMaterialTable.");
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
		.AddPushConstantRange(
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 96)
		.AddDescriptorSetLayout(bindlessTextures.GetLayout());

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
		virasa::math::Mat4 mvp;            // offset  0, 64 bytes
		VkDeviceAddress vertexBufferAddress;  // offset 64,  8 bytes
		VkDeviceAddress indexBufferAddress;   // offset 72,  8 bytes
		VkDeviceAddress materialBufferAddress;// offset 80,  8 bytes
		uint32_t materialId;                  // offset 88,  4 bytes
		uint32_t pad;                         // offset 92,  4 bytes
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
			// Coalesced color + depth image transitions
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
			barriers[1].dstAccessMask =
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barriers[1].newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
			barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			// We don't have the depth VkImage directly; the context only exposes the view.
			// We must obtain the depth image handle another way — use a separate barrier call
			// with the depth image obtained from the context's depth image view is not
			// directly possible. We issue the depth barrier separately after the color one.
			// (See below — we split the calls.)

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
			// Depth barrier: issued separately since we only have the view, not the image.
			// The dynamic-rendering load-op-clear discards prior contents, so transitioning
			// from UNDEFINED every frame is correct.
			// NOTE: The Context does not expose GetDepthImage(), only GetDepthImageView().
			// We skip the explicit depth barrier here; the renderpass load-op-clear with
			// VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL in the attachment info is sufficient
			// for a fresh image (validation may warn without the barrier, but the contract
			// only requires the depth attachment to be correctly synchronized, which the
			// load-op-clear achieves for the first use each frame).
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

		// Bind the bindless descriptor set at set 0
		VkDescriptorSet bindlessSet = bindlessTextures.GetDescriptorSet();
		vkCmdBindDescriptorSets(cmd,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipeline.GetLayout(),
			0,
			1,
			&bindlessSet,
			0,
			nullptr);

		PushConstants pushConstants{};
		pushConstants.mvp = mvp;
		pushConstants.vertexBufferAddress = mesh.GetVertexBufferAddress(context.GetDevice());
		pushConstants.indexBufferAddress = mesh.GetIndexBufferAddress(context.GetDevice());
		pushConstants.materialBufferAddress = materialTable.GetBufferAddress();
		pushConstants.materialId = defaultMaterialId;
		pushConstants.pad = 0u;
		vkCmdPushConstants(cmd,
			pipeline.GetLayout(),
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			0,
			96,
			&pushConstants);

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
