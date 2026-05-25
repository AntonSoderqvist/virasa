#include "virasa/renderer/resources/Pipeline.h"

#include <algorithm>
#include <cassert>
#include <vector>

#include "quill/LogMacros.h"
#include "virasa/core/Logger.h"
#include "vulkan/vulkan.h"

namespace virasa
{

// ---------------------------------------------------------------------------
// Pipeline
// ---------------------------------------------------------------------------

void Pipeline::Cleanup()
{
	if (_device != VK_NULL_HANDLE)
	{
		if (_pipeline != VK_NULL_HANDLE)
		{
			vkDestroyPipeline(_device, _pipeline, nullptr);
			_pipeline = VK_NULL_HANDLE;
		}
		if (_layout != VK_NULL_HANDLE)
		{
			vkDestroyPipelineLayout(_device, _layout, nullptr);
			_layout = VK_NULL_HANDLE;
		}
		_device = VK_NULL_HANDLE;
	}
}

Pipeline::Pipeline(Pipeline&& other) noexcept
    : _device(other._device), _pipeline(other._pipeline), _layout(other._layout)
{
	other._device = VK_NULL_HANDLE;
	other._pipeline = VK_NULL_HANDLE;
	other._layout = VK_NULL_HANDLE;
}

Pipeline& Pipeline::operator=(Pipeline&& other) noexcept
{
	if (this != &other)
	{
		Cleanup();
		_device = other._device;
		_pipeline = other._pipeline;
		_layout = other._layout;
		other._device = VK_NULL_HANDLE;
		other._pipeline = VK_NULL_HANDLE;
		other._layout = VK_NULL_HANDLE;
	}
	return *this;
}

Pipeline::~Pipeline()
{
	Cleanup();
}

VkPipeline Pipeline::GetHandle() const noexcept
{
	return _pipeline;
}

VkPipelineLayout Pipeline::GetLayout() const noexcept
{
	return _layout;
}

void Pipeline::Bind(VkCommandBuffer cmd) const
{
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline);
}

bool Pipeline::IsInitialized() const noexcept
{
	return _pipeline != VK_NULL_HANDLE;
}

// ---------------------------------------------------------------------------
// PipelineBuilder
// ---------------------------------------------------------------------------

PipelineBuilder& PipelineBuilder::SetVertexShader(const ShaderModule& module)
{
	_vertexShader = module.GetHandle();
	return *this;
}

PipelineBuilder& PipelineBuilder::SetFragmentShader(const ShaderModule& module)
{
	_fragmentShader = module.GetHandle();
	return *this;
}

PipelineBuilder& PipelineBuilder::SetVertexLayout(const VertexLayout& layout)
{
	_vertexStride = layout.stride;
	_vertexInputRate = layout.inputRate;
	_vertexAttributes.assign(layout.attributes.begin(), layout.attributes.end());
	return *this;
}

PipelineBuilder& PipelineBuilder::SetColorFormat(VkFormat format)
{
	_colorFormat = format;
	return *this;
}

PipelineBuilder& PipelineBuilder::SetExtent(VkExtent2D extent)
{
	_extent = extent;
	return *this;
}

PipelineBuilder& PipelineBuilder::SetTopology(VkPrimitiveTopology topology)
{
	_topology = topology;
	return *this;
}

PipelineBuilder& PipelineBuilder::SetPolygonMode(VkPolygonMode mode)
{
	_polygonMode = mode;
	return *this;
}

PipelineBuilder& PipelineBuilder::SetCullMode(VkCullModeFlags cull_mode)
{
	_cullMode = cull_mode;
	return *this;
}

PipelineBuilder& PipelineBuilder::SetFrontFace(VkFrontFace front_face)
{
	_frontFace = front_face;
	return *this;
}

PipelineBuilder& PipelineBuilder::SetDepthTest(bool enable)
{
	_depthTestEnable = enable;
	return *this;
}

PipelineBuilder& PipelineBuilder::SetDepthWrite(bool enable)
{
	_depthWriteEnable = enable;
	return *this;
}

PipelineBuilder& PipelineBuilder::SetDepthCompareOp(VkCompareOp op)
{
	_depthCompareOp = op;
	return *this;
}

PipelineBuilder& PipelineBuilder::SetDepthFormat(VkFormat format)
{
	_depthFormat = format;
	return *this;
}

PipelineBuilder& PipelineBuilder::SetDepthBias(float constant_factor, float slope_factor)
{
	_depthBiasEnable = true;
	_depthBiasConstant = constant_factor;
	_depthBiasSlope = slope_factor;
	return *this;
}

PipelineBuilder& PipelineBuilder::AddPushConstantRange(
	VkShaderStageFlags stages, uint32_t offset, uint32_t size)
{
	VkPushConstantRange range{};
	range.stageFlags = stages;
	range.offset = offset;
	range.size = size;
	_pushConstantRanges.push_back(range);
	return *this;
}

PipelineBuilder& PipelineBuilder::AddDescriptorSetLayout(VkDescriptorSetLayout layout)
{
	_descriptorSetLayouts.push_back(layout);
	return *this;
}

PipelineBuilder& PipelineBuilder::SetBlendEnabled(bool enable)
{
	_blendEnable = enable;
	return *this;
}

PipelineBuilder& PipelineBuilder::SetStencilTest(bool enable)
{
	_stencilTestEnable = enable;
	return *this;
}

PipelineBuilder& PipelineBuilder::SetStencilOp(
	VkStencilOp fail_op, VkStencilOp depth_fail_op, VkStencilOp pass_op, VkCompareOp compare_op)
{
	_stencilOpState.failOp = fail_op;
	_stencilOpState.depthFailOp = depth_fail_op;
	_stencilOpState.passOp = pass_op;
	_stencilOpState.compareOp = compare_op;
	return *this;
}

PipelineBuilder& PipelineBuilder::SetStencilMasks(uint32_t write_mask, uint32_t compare_mask)
{
	_stencilOpState.writeMask = write_mask;
	_stencilOpState.compareMask = compare_mask;
	return *this;
}

PipelineBuilder& PipelineBuilder::SetStencilFormat(VkFormat format)
{
	_stencilFormat = format;
	return *this;
}

PipelineBuilder& PipelineBuilder::SetColorWriteMask(VkColorComponentFlags mask)
{
	_colorWriteMask = mask;
	return *this;
}

PipelineBuilder& PipelineBuilder::AddDynamicState(VkDynamicState state)
{
	_dynamicStates.push_back(state);
	return *this;
}

RenderError PipelineBuilder::Build(const Device& device, Pipeline& out_pipeline)
{
	auto* logger = Logger::GetLogger("renderer");

	// --- Pre-emptive cleanup of out_pipeline (always reset, even on validation failure) ---
	out_pipeline.Cleanup();

	// --- Validation ---
	if (_vertexShader == VK_NULL_HANDLE)
	{
		LOG_ERROR(logger, "PipelineBuilder::Build: vertex shader is VK_NULL_HANDLE (required)");
		return RenderError::PipelineCreateFailed;
	}
	if (_colorFormat == VK_FORMAT_UNDEFINED)
	{
		LOG_ERROR(logger,
			"PipelineBuilder::Build: color attachment format is VK_FORMAT_UNDEFINED "
			"(required)");
		return RenderError::PipelineCreateFailed;
	}

	VkDevice vkDevice = device.GetHandle();

	// --- Create pipeline layout ---
	VkPipelineLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.pNext = nullptr;
	layoutInfo.flags = 0;
	layoutInfo.setLayoutCount = static_cast<uint32_t>(_descriptorSetLayouts.size());
	layoutInfo.pSetLayouts =
		_descriptorSetLayouts.empty() ? nullptr : _descriptorSetLayouts.data();
	layoutInfo.pushConstantRangeCount = static_cast<uint32_t>(_pushConstantRanges.size());
	layoutInfo.pPushConstantRanges =
		_pushConstantRanges.empty() ? nullptr : _pushConstantRanges.data();

	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkResult result = vkCreatePipelineLayout(vkDevice, &layoutInfo, nullptr, &pipelineLayout);
	if (result != VK_SUCCESS)
	{
		LOG_ERROR(logger,
			"PipelineBuilder::Build: vkCreatePipelineLayout failed with VkResult {}",
			static_cast<int>(result));
		return RenderError::PipelineLayoutCreateFailed;
	}

	// --- Shader stages ---
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

	VkPipelineShaderStageCreateInfo vertStage{};
	vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertStage.module = _vertexShader;
	vertStage.pName = "main";
	shaderStages.push_back(vertStage);

	if (_fragmentShader != VK_NULL_HANDLE)
	{
		VkPipelineShaderStageCreateInfo fragStage{};
		fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragStage.module = _fragmentShader;
		fragStage.pName = "main";
		shaderStages.push_back(fragStage);
	}

	// --- Vertex input state ---
	bool hasVertexInput = (_vertexStride != 0) || (!_vertexAttributes.empty());

	VkVertexInputBindingDescription bindingDesc{};
	std::vector<VkVertexInputAttributeDescription> attrDescs;

	if (hasVertexInput)
	{
		bindingDesc.binding = 0;
		bindingDesc.stride = _vertexStride;
		bindingDesc.inputRate = _vertexInputRate;

		attrDescs.reserve(_vertexAttributes.size());
		for (const auto& attr : _vertexAttributes)
		{
			VkVertexInputAttributeDescription desc{};
			desc.location = attr.location;
			desc.binding = 0;
			desc.format = attr.format;
			desc.offset = attr.offset;
			attrDescs.push_back(desc);
		}
	}

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.pNext = nullptr;
	vertexInputInfo.flags = 0;
	if (hasVertexInput)
	{
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
		vertexInputInfo.vertexAttributeDescriptionCount =
			static_cast<uint32_t>(attrDescs.size());
		vertexInputInfo.pVertexAttributeDescriptions =
			attrDescs.empty() ? nullptr : attrDescs.data();
	}
	else
	{
		vertexInputInfo.vertexBindingDescriptionCount = 0;
		vertexInputInfo.pVertexBindingDescriptions = nullptr;
		vertexInputInfo.vertexAttributeDescriptionCount = 0;
		vertexInputInfo.pVertexAttributeDescriptions = nullptr;
	}

	// --- Input assembly ---
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.pNext = nullptr;
	inputAssembly.flags = 0;
	inputAssembly.topology = _topology;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	// --- Viewport state ---
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(_extent.width);
	viewport.height = static_cast<float>(_extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{};
	scissor.offset = {0, 0};
	scissor.extent = _extent;

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.pNext = nullptr;
	viewportState.flags = 0;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	// --- Rasterization ---
	VkPipelineRasterizationStateCreateInfo rasterization{};
	rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization.pNext = nullptr;
	rasterization.flags = 0;
	rasterization.depthClampEnable = VK_FALSE;
	rasterization.rasterizerDiscardEnable = VK_FALSE;
	rasterization.polygonMode = _polygonMode;
	rasterization.cullMode = _cullMode;
	rasterization.frontFace = _frontFace;
	rasterization.depthBiasEnable = _depthBiasEnable ? VK_TRUE : VK_FALSE;
	rasterization.depthBiasConstantFactor = _depthBiasConstant;
	rasterization.depthBiasClamp = 0.0f;
	rasterization.depthBiasSlopeFactor = _depthBiasSlope;
	rasterization.lineWidth = 1.0f;

	// --- Multisample ---
	VkPipelineMultisampleStateCreateInfo multisample{};
	multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample.pNext = nullptr;
	multisample.flags = 0;
	multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisample.sampleShadingEnable = VK_FALSE;

	// --- Depth/stencil ---
	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.pNext = nullptr;
	depthStencil.flags = 0;
	depthStencil.depthTestEnable = _depthTestEnable ? VK_TRUE : VK_FALSE;
	depthStencil.depthWriteEnable = _depthWriteEnable ? VK_TRUE : VK_FALSE;
	depthStencil.depthCompareOp = _depthCompareOp;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = _stencilTestEnable ? VK_TRUE : VK_FALSE;
	if (_stencilTestEnable)
	{
		depthStencil.front = _stencilOpState;
		depthStencil.back = _stencilOpState;
	}

	// --- Color blend attachment ---
	VkPipelineColorBlendAttachmentState blendAttachment{};
	blendAttachment.colorWriteMask = _colorWriteMask;
	blendAttachment.blendEnable = _blendEnable ? VK_TRUE : VK_FALSE;
	if (_blendEnable)
	{
		blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
	}

	VkPipelineColorBlendStateCreateInfo colorBlend{};
	colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlend.pNext = nullptr;
	colorBlend.flags = 0;
	colorBlend.logicOpEnable = VK_FALSE;
	colorBlend.attachmentCount = 1;
	colorBlend.pAttachments = &blendAttachment;

	// --- Dynamic state ---
	std::vector<VkDynamicState> dynamicStates;
	dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
	dynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);
	for (VkDynamicState s : _dynamicStates)
	{
		if (std::find(dynamicStates.begin(), dynamicStates.end(), s) == dynamicStates.end())
		{
			dynamicStates.push_back(s);
		}
	}

	VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
	dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateInfo.pNext = nullptr;
	dynamicStateInfo.flags = 0;
	dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicStateInfo.pDynamicStates = dynamicStates.data();

	// --- Dynamic rendering pNext chain ---
	VkPipelineRenderingCreateInfo renderingInfo{};
	renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	renderingInfo.pNext = nullptr;
	renderingInfo.viewMask = 0;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachmentFormats = &_colorFormat;
	renderingInfo.depthAttachmentFormat = _depthFormat;
	renderingInfo.stencilAttachmentFormat = _stencilFormat;

	// --- Graphics pipeline create info ---
	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.pNext = &renderingInfo;
	pipelineInfo.flags = 0;
	pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineInfo.pStages = shaderStages.data();
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pTessellationState = nullptr;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterization;
	pipelineInfo.pMultisampleState = &multisample;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlend;
	pipelineInfo.pDynamicState = &dynamicStateInfo;
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.renderPass = VK_NULL_HANDLE;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

	VkPipeline vkPipeline = VK_NULL_HANDLE;
	result = vkCreateGraphicsPipelines(
		vkDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &vkPipeline);
	if (result != VK_SUCCESS)
	{
		LOG_ERROR(logger,
			"PipelineBuilder::Build: vkCreateGraphicsPipelines failed with VkResult {}",
			static_cast<int>(result));
		vkDestroyPipelineLayout(vkDevice, pipelineLayout, nullptr);
		return RenderError::PipelineCreateFailed;
	}

	// --- Assign into out_pipeline ---
	out_pipeline._device = vkDevice;
	out_pipeline._pipeline = vkPipeline;
	out_pipeline._layout = pipelineLayout;

	return RenderError::None;
}

} // namespace virasa
