#include "virasa/renderer/text/UiPass.h"

#include <array>
#include <cassert>
#include <cstring>
#include <string_view>
#include <utility>
#include <vector>

#include "virasa/renderer/resources/ShaderModule.h"

namespace virasa::renderer::text
{
namespace
{
constexpr VkDeviceSize kInitialGeometryBufferSize = 65536;

struct QuadVertex
{
	float x;
	float y;
	float r;
	float g;
	float b;
	float a;
};

struct TextVertex
{
	float x;
	float y;
	float u;
	float v;
	float r;
	float g;
	float b;
	float a;
};

struct PushConstants
{
	float invHalfW;
	float invHalfH;
};

struct GeometryLayout
{
	VkDeviceSize quadVertexOffset = 0;
	VkDeviceSize quadIndexOffset = 0;
	uint32_t quadIndexCount = 0;
	VkDeviceSize textVertexOffset = 0;
	VkDeviceSize textIndexOffset = 0;
	uint32_t textIndexCount = 0;
	VkBuffer buffer = VK_NULL_HANDLE;
};

[[nodiscard]] uint32_t DecodeUtf8Codepoint(std::string_view text, size_t& offset) noexcept
{
	if (offset >= text.size())
	{
		return 0;
	}

	const uint8_t first = static_cast<uint8_t>(text[offset]);
	if ((first & 0x80u) == 0)
	{
		++offset;
		return first;
	}

	if ((first & 0xE0u) == 0xC0u && offset + 1 < text.size())
	{
		const uint8_t b1 = static_cast<uint8_t>(text[offset + 1]);
		offset += 2;
		return static_cast<uint32_t>(((first & 0x1Fu) << 6) | (b1 & 0x3Fu));
	}

	if ((first & 0xF0u) == 0xE0u && offset + 2 < text.size())
	{
		const uint8_t b1 = static_cast<uint8_t>(text[offset + 1]);
		const uint8_t b2 = static_cast<uint8_t>(text[offset + 2]);
		offset += 3;
		return static_cast<uint32_t>(
			((first & 0x0Fu) << 12) | ((b1 & 0x3Fu) << 6) | (b2 & 0x3Fu));
	}

	if ((first & 0xF8u) == 0xF0u && offset + 3 < text.size())
	{
		const uint8_t b1 = static_cast<uint8_t>(text[offset + 1]);
		const uint8_t b2 = static_cast<uint8_t>(text[offset + 2]);
		const uint8_t b3 = static_cast<uint8_t>(text[offset + 3]);
		offset += 4;
		return static_cast<uint32_t>(((first & 0x07u) << 18) | ((b1 & 0x3Fu) << 12) |
						     ((b2 & 0x3Fu) << 6) | (b3 & 0x3Fu));
	}

	++offset;
	return 0;
}
} // namespace

UiPass::~UiPass()
{
	if (_device == VK_NULL_HANDLE)
	{
		return;
	}
	if (_descriptorPool != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(_device, _descriptorPool, nullptr);
	}
	if (_atlasSampler != VK_NULL_HANDLE)
	{
		vkDestroySampler(_device, _atlasSampler, nullptr);
	}
	if (_textSetLayout != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorSetLayout(_device, _textSetLayout, nullptr);
	}
}

RenderError UiPass::Initialize(
	const Device& device, const Context& context, const virasa::ui::FontAtlas& atlas)
{
	if (_initialized)
	{
		return RenderError::AlreadyInitialized;
	}

	Image atlasImage;
	Pipeline quadPipeline;
	Pipeline textPipeline;

	ImageConfig imageConfig = {};
	imageConfig.width = atlas.GetAtlasWidth();
	imageConfig.height = atlas.GetAtlasHeight();
	imageConfig.format = VK_FORMAT_R8_UNORM;
	imageConfig.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imageConfig.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	imageConfig.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	imageConfig.flags = 0;

	RenderError error = atlasImage.Initialize(device, imageConfig);
	if (error != RenderError::None)
	{
		return error;
	}

	const std::span<const uint8_t> bitmap = atlas.GetBitmap();
	if (!bitmap.empty())
	{
		Buffer stagingBuffer;
		BufferConfig stagingConfig = {};
		stagingConfig.size = static_cast<VkDeviceSize>(bitmap.size());
		stagingConfig.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		stagingConfig.memoryProperties =
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

		error = stagingBuffer.Initialize(device, stagingConfig);
		if (error != RenderError::None)
		{
			return error;
		}

		void* mapped = stagingBuffer.Map();
		if (mapped == nullptr)
		{
			return RenderError::MemoryMapFailed;
		}

		std::memcpy(mapped, bitmap.data(), bitmap.size());

		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = context.GetGraphicsCommandPool();
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		if (vkAllocateCommandBuffers(device.GetHandle(), &allocInfo, &commandBuffer) !=
			VK_SUCCESS)
		{
			return RenderError::CommandPoolCreateFailed;
		}

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
		{
			vkFreeCommandBuffers(
				device.GetHandle(), context.GetGraphicsCommandPool(), 1, &commandBuffer);
			return RenderError::CommandPoolCreateFailed;
		}

		VkImageMemoryBarrier toTransfer = {};
		toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		toTransfer.image = atlasImage.GetHandle();
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

		VkBufferImageCopy copyRegion = {};
		copyRegion.bufferOffset = 0;
		copyRegion.bufferRowLength = 0;
		copyRegion.bufferImageHeight = 0;
		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.mipLevel = 0;
		copyRegion.imageSubresource.baseArrayLayer = 0;
		copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageOffset = {0, 0, 0};
		copyRegion.imageExtent = {atlas.GetAtlasWidth(), atlas.GetAtlasHeight(), 1};

		vkCmdCopyBufferToImage(commandBuffer,
			stagingBuffer.GetHandle(),
			atlasImage.GetHandle(),
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&copyRegion);

		VkImageMemoryBarrier toShaderRead = {};
		toShaderRead.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		toShaderRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		toShaderRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		toShaderRead.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		toShaderRead.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		toShaderRead.image = atlasImage.GetHandle();
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

		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
		{
			vkFreeCommandBuffers(
				device.GetHandle(), context.GetGraphicsCommandPool(), 1, &commandBuffer);
			return RenderError::CommandPoolCreateFailed;
		}

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		if (vkQueueSubmit(device.GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE) !=
			VK_SUCCESS)
		{
			vkFreeCommandBuffers(
				device.GetHandle(), context.GetGraphicsCommandPool(), 1, &commandBuffer);
			return RenderError::CommandPoolCreateFailed;
		}

		vkQueueWaitIdle(device.GetGraphicsQueue());
		vkFreeCommandBuffers(
			device.GetHandle(), context.GetGraphicsCommandPool(), 1, &commandBuffer);
	}

	ShaderModule quadVertexShader;
	ShaderModule quadFragmentShader;
	ShaderModule textVertexShader;
	ShaderModule textFragmentShader;

	error = quadVertexShader.Initialize(device, "shaders/ui_quad.vert.spv");
	if (error != RenderError::None)
	{
		return error;
	}

	error = quadFragmentShader.Initialize(device, "shaders/ui_quad.frag.spv");
	if (error != RenderError::None)
	{
		return error;
	}

	error = textVertexShader.Initialize(device, "shaders/ui_text.vert.spv");
	if (error != RenderError::None)
	{
		return error;
	}

	error = textFragmentShader.Initialize(device, "shaders/ui_text.frag.spv");
	if (error != RenderError::None)
	{
		return error;
	}

	const std::array<VertexAttribute, 2> quadAttributes = {
		{{0, VK_FORMAT_R32G32_SFLOAT, 0}, {1, VK_FORMAT_R32G32B32A32_SFLOAT, 8}}};
	VertexLayout quadLayout = {};
	quadLayout.stride = sizeof(QuadVertex);
	quadLayout.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	quadLayout.attributes = std::span<const VertexAttribute>(quadAttributes);

	PipelineBuilder quadBuilder;
	quadBuilder.SetVertexShader(quadVertexShader)
		.SetFragmentShader(quadFragmentShader)
		.SetVertexLayout(quadLayout)
		.SetColorFormat(context.GetSwapchainFormat())
		.SetDepthFormat(VK_FORMAT_UNDEFINED)
		.SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
		.SetCullMode(VK_CULL_MODE_NONE)
		.SetBlendEnabled(true)
		.AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants))
		.AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
		.AddDynamicState(VK_DYNAMIC_STATE_SCISSOR);

	error = quadBuilder.Build(device, quadPipeline);
	if (error != RenderError::None)
	{
		return error;
	}

	const std::array<VertexAttribute, 3> textAttributes = {{{0, VK_FORMAT_R32G32_SFLOAT, 0},
		{1, VK_FORMAT_R32G32_SFLOAT, 8},
		{2, VK_FORMAT_R32G32B32A32_SFLOAT, 16}}};
	VertexLayout textLayout = {};
	textLayout.stride = sizeof(TextVertex);
	textLayout.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	textLayout.attributes = std::span<const VertexAttribute>(textAttributes);

	VkDescriptorSetLayoutBinding atlasBinding = {};
	atlasBinding.binding = 0;
	atlasBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	atlasBinding.descriptorCount = 1;
	atlasBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	atlasBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &atlasBinding;

	VkDescriptorSetLayout textSetLayout = VK_NULL_HANDLE;
	if (vkCreateDescriptorSetLayout(device.GetHandle(), &layoutInfo, nullptr, &textSetLayout) !=
		VK_SUCCESS)
	{
		return RenderError::DescriptorPoolCreateFailed;
	}

	PipelineBuilder textBuilder;
	textBuilder.SetVertexShader(textVertexShader)
		.SetFragmentShader(textFragmentShader)
		.SetVertexLayout(textLayout)
		.SetColorFormat(context.GetSwapchainFormat())
		.SetDepthFormat(VK_FORMAT_UNDEFINED)
		.SetTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
		.SetCullMode(VK_CULL_MODE_NONE)
		.SetBlendEnabled(true)
		.AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants))
		.AddDescriptorSetLayout(textSetLayout)
		.AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT)
		.AddDynamicState(VK_DYNAMIC_STATE_SCISSOR);

	error = textBuilder.Build(device, textPipeline);
	if (error != RenderError::None)
	{
		vkDestroyDescriptorSetLayout(device.GetHandle(), textSetLayout, nullptr);
		return error;
	}

	VkSamplerCreateInfo samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

	VkSampler atlasSampler = VK_NULL_HANDLE;
	if (vkCreateSampler(device.GetHandle(), &samplerInfo, nullptr, &atlasSampler) != VK_SUCCESS)
	{
		vkDestroyDescriptorSetLayout(device.GetHandle(), textSetLayout, nullptr);
		return RenderError::DescriptorPoolCreateFailed;
	}

	VkDescriptorPoolSize poolSize = {};
	poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSize.descriptorCount = 1;

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.maxSets = 1;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;

	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	if (vkCreateDescriptorPool(device.GetHandle(), &poolInfo, nullptr, &descriptorPool) !=
		VK_SUCCESS)
	{
		vkDestroySampler(device.GetHandle(), atlasSampler, nullptr);
		vkDestroyDescriptorSetLayout(device.GetHandle(), textSetLayout, nullptr);
		return RenderError::DescriptorPoolCreateFailed;
	}

	VkDescriptorSetAllocateInfo setAllocInfo = {};
	setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocInfo.descriptorPool = descriptorPool;
	setAllocInfo.descriptorSetCount = 1;
	setAllocInfo.pSetLayouts = &textSetLayout;

	VkDescriptorSet atlasDescriptorSet = VK_NULL_HANDLE;
	if (vkAllocateDescriptorSets(device.GetHandle(), &setAllocInfo, &atlasDescriptorSet) !=
		VK_SUCCESS)
	{
		vkDestroyDescriptorPool(device.GetHandle(), descriptorPool, nullptr);
		vkDestroySampler(device.GetHandle(), atlasSampler, nullptr);
		vkDestroyDescriptorSetLayout(device.GetHandle(), textSetLayout, nullptr);
		return RenderError::DescriptorPoolCreateFailed;
	}

	VkDescriptorImageInfo imageInfo = {};
	imageInfo.sampler = atlasSampler;
	imageInfo.imageView = atlasImage.GetView();
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = atlasDescriptorSet;
	write.dstBinding = 0;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(device.GetHandle(), 1, &write, 0, nullptr);

	BufferConfig geometryConfig = {};
	geometryConfig.size = kInitialGeometryBufferSize;
	geometryConfig.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	geometryConfig.memoryProperties =
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

	const uint32_t framesInFlight = context.GetMaxFramesInFlight();
	std::vector<Buffer> geometryBuffers(framesInFlight);
	for (uint32_t i = 0; i < framesInFlight; ++i)
	{
		error = geometryBuffers[i].Initialize(device, geometryConfig);
		if (error != RenderError::None)
		{
			return error;
		}
	}

	_device = device.GetHandle();
	_atlasSampler = atlasSampler;
	_textSetLayout = textSetLayout;
	_descriptorPool = descriptorPool;
	_atlasDescriptorSet = atlasDescriptorSet;
	_atlasImage = std::move(atlasImage);
	_quadPipeline = std::move(quadPipeline);
	_textPipeline = std::move(textPipeline);
	_geometryBuffers = std::move(geometryBuffers);
	_initialized = true;
	return RenderError::None;
}

void UiPass::Submit(virasa::renderer::graph::Graph& graph,
	virasa::renderer::graph::ImageHandle swapchainTarget, const virasa::ui::DrawList& list,
	const virasa::ui::FontAtlas& atlas, uint32_t frameIndex, uint32_t windowWidth,
	uint32_t windowHeight)
{
	assert(_initialized);

	std::vector<QuadVertex> quadVertices;
	std::vector<uint16_t> quadIndices;
	quadVertices.reserve(list.GetQuads().size() * 4);
	quadIndices.reserve(list.GetQuads().size() * 6);

	for (const virasa::ui::QuadCommand& quad : list.GetQuads())
	{
		if (quad.width <= 0.0f || quad.height <= 0.0f)
		{
			continue;
		}

		const uint16_t baseIndex = static_cast<uint16_t>(quadVertices.size());
		const float x0 = quad.x;
		const float y0 = quad.y;
		const float x1 = quad.x + quad.width;
		const float y1 = quad.y + quad.height;

		quadVertices.push_back(
			{x0, y0, quad.color.r, quad.color.g, quad.color.b, quad.color.a});
		quadVertices.push_back(
			{x1, y0, quad.color.r, quad.color.g, quad.color.b, quad.color.a});
		quadVertices.push_back(
			{x1, y1, quad.color.r, quad.color.g, quad.color.b, quad.color.a});
		quadVertices.push_back(
			{x0, y1, quad.color.r, quad.color.g, quad.color.b, quad.color.a});

		quadIndices.push_back(baseIndex + 0);
		quadIndices.push_back(baseIndex + 1);
		quadIndices.push_back(baseIndex + 2);
		quadIndices.push_back(baseIndex + 0);
		quadIndices.push_back(baseIndex + 2);
		quadIndices.push_back(baseIndex + 3);
	}

	std::vector<TextVertex> textVertices;
	std::vector<uint16_t> textIndices;
	const std::string_view textBuffer = list.GetTextBuffer();
	const float atlasWidth = static_cast<float>(atlas.GetAtlasWidth());
	const float atlasHeight = static_cast<float>(atlas.GetAtlasHeight());

	for (const virasa::ui::TextCommand& text : list.GetTexts())
	{
		if (text.textLength == 0)
		{
			continue;
		}

		const std::string_view run = textBuffer.substr(text.textOffset, text.textLength);
		float penX = text.x;
		const float penY = text.y;
		size_t offset = 0;

		while (offset < run.size())
		{
			const uint32_t codepoint = DecodeUtf8Codepoint(run, offset);
			const virasa::ui::GlyphMetrics& glyph = atlas.GetGlyph(codepoint);

			if (glyph.width > 0 && glyph.height > 0)
			{
				const uint16_t baseIndex = static_cast<uint16_t>(textVertices.size());
				const float x0 = penX + static_cast<float>(glyph.bearingX);
				const float y0 = penY - static_cast<float>(glyph.bearingY);
				const float x1 = x0 + static_cast<float>(glyph.width);
				const float y1 = y0 + static_cast<float>(glyph.height);
				const float u0 = static_cast<float>(glyph.u0) / atlasWidth;
				const float v0 = static_cast<float>(glyph.v0) / atlasHeight;
				const float u1 = static_cast<float>(glyph.u1) / atlasWidth;
				const float v1 = static_cast<float>(glyph.v1) / atlasHeight;

				textVertices.push_back({x0,
					y0,
					u0,
					v0,
					text.color.r,
					text.color.g,
					text.color.b,
					text.color.a});
				textVertices.push_back({x1,
					y0,
					u1,
					v0,
					text.color.r,
					text.color.g,
					text.color.b,
					text.color.a});
				textVertices.push_back({x1,
					y1,
					u1,
					v1,
					text.color.r,
					text.color.g,
					text.color.b,
					text.color.a});
				textVertices.push_back({x0,
					y1,
					u0,
					v1,
					text.color.r,
					text.color.g,
					text.color.b,
					text.color.a});

				textIndices.push_back(baseIndex + 0);
				textIndices.push_back(baseIndex + 1);
				textIndices.push_back(baseIndex + 2);
				textIndices.push_back(baseIndex + 0);
				textIndices.push_back(baseIndex + 2);
				textIndices.push_back(baseIndex + 3);
			}

			penX += glyph.advance;
		}
	}

	GeometryLayout geometry = {};
	geometry.quadVertexOffset = 0;
	geometry.quadIndexOffset =
		geometry.quadVertexOffset +
		static_cast<VkDeviceSize>(quadVertices.size() * sizeof(QuadVertex));
	geometry.quadIndexCount = static_cast<uint32_t>(quadIndices.size());
	geometry.textVertexOffset = geometry.quadIndexOffset +
					    static_cast<VkDeviceSize>(quadIndices.size() * sizeof(uint16_t));
	geometry.textIndexOffset =
		geometry.textVertexOffset +
		static_cast<VkDeviceSize>(textVertices.size() * sizeof(TextVertex));
	geometry.textIndexCount = static_cast<uint32_t>(textIndices.size());

	const VkDeviceSize totalSize =
		geometry.textIndexOffset +
		static_cast<VkDeviceSize>(textIndices.size() * sizeof(uint16_t));
	Buffer& geometryBuffer = _geometryBuffers[frameIndex % _geometryBuffers.size()];
	assert(totalSize <= geometryBuffer.GetSize());

	void* mapped = geometryBuffer.Map();
	assert(mapped != nullptr);

	uint8_t* bytes = static_cast<uint8_t*>(mapped);
	if (!quadVertices.empty())
	{
		std::memcpy(bytes + geometry.quadVertexOffset,
			quadVertices.data(),
			quadVertices.size() * sizeof(QuadVertex));
	}
	if (!quadIndices.empty())
	{
		std::memcpy(bytes + geometry.quadIndexOffset,
			quadIndices.data(),
			quadIndices.size() * sizeof(uint16_t));
	}
	if (!textVertices.empty())
	{
		std::memcpy(bytes + geometry.textVertexOffset,
			textVertices.data(),
			textVertices.size() * sizeof(TextVertex));
	}
	if (!textIndices.empty())
	{
		std::memcpy(bytes + geometry.textIndexOffset,
			textIndices.data(),
			textIndices.size() * sizeof(uint16_t));
	}

	geometry.buffer = geometryBuffer.GetHandle();

	const PushConstants pushConstants = {
		2.0f / static_cast<float>(windowWidth), 2.0f / static_cast<float>(windowHeight)};

	const VkPipeline quadPipelineHandle = _quadPipeline.GetHandle();
	const VkPipelineLayout quadPipelineLayout = _quadPipeline.GetLayout();
	const VkPipeline textPipelineHandle = _textPipeline.GetHandle();
	const VkPipelineLayout textPipelineLayout = _textPipeline.GetLayout();
	const VkDescriptorSet textDescriptorSet = _atlasDescriptorSet;

	graph.AddPass("ui")
		.ColorAttachment(swapchainTarget, virasa::renderer::graph::LoadOp::Load, {})
		.Record(
			[pushConstants,
				geometry,
				quadPipelineHandle,
				quadPipelineLayout,
				textPipelineHandle,
				textPipelineLayout,
				textDescriptorSet,
				this](const virasa::renderer::graph::GraphContext& ctx)
			{
				const VkCommandBuffer cmd = ctx.GetCommandBuffer();
				const VkExtent2D extent = ctx.GetRenderExtent();

				VkViewport viewport = {};
				viewport.x = 0.0f;
				viewport.y = 0.0f;
				viewport.width = static_cast<float>(extent.width);
				viewport.height = static_cast<float>(extent.height);
				viewport.minDepth = 0.0f;
				viewport.maxDepth = 1.0f;

				VkRect2D scissor = {};
				scissor.offset = {0, 0};
				scissor.extent = extent;

				if (geometry.quadIndexCount > 0)
				{
					vkCmdSetViewport(cmd, 0, 1, &viewport);
					vkCmdSetScissor(cmd, 0, 1, &scissor);
					this->_quadPipeline.Bind(cmd);

					const VkDeviceSize vertexOffset = geometry.quadVertexOffset;
					vkCmdBindVertexBuffers(cmd, 0, 1, &geometry.buffer, &vertexOffset);
					vkCmdBindIndexBuffer(cmd,
						geometry.buffer,
						geometry.quadIndexOffset,
						VK_INDEX_TYPE_UINT16);
					vkCmdPushConstants(cmd,
						quadPipelineLayout,
						VK_SHADER_STAGE_VERTEX_BIT,
						0,
						sizeof(PushConstants),
						&pushConstants);
					vkCmdDrawIndexed(cmd, geometry.quadIndexCount, 1, 0, 0, 0);
				}

				if (geometry.textIndexCount > 0)
				{
					vkCmdSetViewport(cmd, 0, 1, &viewport);
					vkCmdSetScissor(cmd, 0, 1, &scissor);
					this->_textPipeline.Bind(cmd);

					const VkDeviceSize vertexOffset = geometry.textVertexOffset;
					vkCmdBindVertexBuffers(cmd, 0, 1, &geometry.buffer, &vertexOffset);
					vkCmdBindIndexBuffer(cmd,
						geometry.buffer,
						geometry.textIndexOffset,
						VK_INDEX_TYPE_UINT16);
					vkCmdPushConstants(cmd,
						textPipelineLayout,
						VK_SHADER_STAGE_VERTEX_BIT,
						0,
						sizeof(PushConstants),
						&pushConstants);
					if (textDescriptorSet != VK_NULL_HANDLE)
					{
						vkCmdBindDescriptorSets(cmd,
							VK_PIPELINE_BIND_POINT_GRAPHICS,
							textPipelineLayout,
							0,
							1,
							&textDescriptorSet,
							0,
							nullptr);
					}
					vkCmdDrawIndexed(cmd, geometry.textIndexCount, 1, 0, 0, 0);
				}
			});
}
} // namespace virasa::renderer::text
