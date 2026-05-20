#ifndef VIRASA_RENDERER_RESOURCES_PIPELINE_H
#define VIRASA_RENDERER_RESOURCES_PIPELINE_H

#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Device.h"
#include "virasa/renderer/resources/ShaderModule.h"

#include "vulkan/vulkan.h"

#include <cstdint>
#include <vector>

namespace virasa
{

/**
 * @brief Owns a VkPipeline and VkPipelineLayout created by PipelineBuilder::Build.
 *
 * Pipeline is a RAII handle-owner. It is movable but not copyable. A default-constructed
 * Pipeline owns no Vulkan handles. The VkDevice used to create the pipeline is borrowed;
 * the Device passed to PipelineBuilder::Build must outlive this Pipeline.
 */
class Pipeline final
{
public:
	/**
	 * @brief Default-constructs a Pipeline that owns no Vulkan handles.
	 */
	Pipeline() noexcept = default;

	/// Not copyable.
	Pipeline(const Pipeline&) = delete;
	Pipeline& operator=(const Pipeline&) = delete;

	/**
	 * @brief Move-constructs a Pipeline, transferring ownership of all handles.
	 * @param other The source Pipeline; left in the default-constructed state.
	 */
	Pipeline(Pipeline&& other) noexcept;

	/**
	 * @brief Move-assigns a Pipeline, destroying prior handles then taking ownership.
	 * @param other The source Pipeline; left in the default-constructed state.
	 * @return *this
	 */
	Pipeline& operator=(Pipeline&& other) noexcept;

	/**
	 * @brief Destroys owned VkPipeline and VkPipelineLayout, if any.
	 */
	~Pipeline();

	/**
	 * @brief Returns the owned VkPipeline handle, or VK_NULL_HANDLE if none.
	 * @return The VkPipeline handle.
	 */
	[[nodiscard]] VkPipeline GetHandle() const noexcept;

	/**
	 * @brief Returns the owned VkPipelineLayout handle, or VK_NULL_HANDLE if none.
	 * @return The VkPipelineLayout handle.
	 */
	[[nodiscard]] VkPipelineLayout GetLayout() const noexcept;

	/**
	 * @brief Records a vkCmdBindPipeline call into the given command buffer.
	 * @param cmd The command buffer to record into.
	 */
	void Bind(VkCommandBuffer cmd) const;

	/**
	 * @brief Returns true if this Pipeline owns a valid VkPipeline handle.
	 * @return true if initialized, false otherwise.
	 */
	[[nodiscard]] bool IsInitialized() const noexcept;

private:
	friend class PipelineBuilder;

	void Cleanup();

	VkDevice		_device		= VK_NULL_HANDLE;
	VkPipeline		_pipeline	= VK_NULL_HANDLE;
	VkPipelineLayout	_layout		= VK_NULL_HANDLE;
};

/**
 * @brief Accumulates VkGraphicsPipeline configuration and produces a Pipeline via Build.
 *
 * PipelineBuilder uses a fluent setter-chain API. A default-constructed PipelineBuilder
 * holds sensible defaults for all fields. Call Build() as the terminal step to create
 * the Vulkan pipeline objects.
 *
 * PipelineBuilder owns no Vulkan resources; all Vulkan calls happen inside Build.
 */
class PipelineBuilder final
{
public:
	/**
	 * @brief Default-constructs a PipelineBuilder with default state.
	 */
	PipelineBuilder() = default;

	/**
	 * @brief Sets the vertex shader stage from a ShaderModule.
	 * @param module The ShaderModule whose handle is stored.
	 * @return *this for chaining.
	 */
	PipelineBuilder& SetVertexShader(const ShaderModule& module);

	/**
	 * @brief Sets the fragment shader stage from a ShaderModule.
	 * @param module The ShaderModule whose handle is stored.
	 * @return *this for chaining.
	 */
	PipelineBuilder& SetFragmentShader(const ShaderModule& module);

	/**
	 * @brief Sets the vertex input layout, copying attributes at call time.
	 * @param layout The vertex layout to record.
	 * @return *this for chaining.
	 */
	PipelineBuilder& SetVertexLayout(const VertexLayout& layout);

	/**
	 * @brief Sets the color attachment format for dynamic rendering.
	 * @param format The VkFormat to use.
	 * @return *this for chaining.
	 */
	PipelineBuilder& SetColorFormat(VkFormat format);

	/**
	 * @brief Sets the viewport/scissor extent used at pipeline creation time.
	 * @param extent The VkExtent2D to use.
	 * @return *this for chaining.
	 */
	PipelineBuilder& SetExtent(VkExtent2D extent);

	/**
	 * @brief Sets the primitive topology.
	 * @param topology The VkPrimitiveTopology to use.
	 * @return *this for chaining.
	 */
	PipelineBuilder& SetTopology(VkPrimitiveTopology topology);

	/**
	 * @brief Sets the polygon mode.
	 * @param mode The VkPolygonMode to use.
	 * @return *this for chaining.
	 */
	PipelineBuilder& SetPolygonMode(VkPolygonMode mode);

	/**
	 * @brief Sets the cull mode flags.
	 * @param cull_mode The VkCullModeFlags to use.
	 * @return *this for chaining.
	 */
	PipelineBuilder& SetCullMode(VkCullModeFlags cull_mode);

	/**
	 * @brief Sets the front face winding order.
	 * @param front_face The VkFrontFace to use.
	 * @return *this for chaining.
	 */
	PipelineBuilder& SetFrontFace(VkFrontFace front_face);

	/**
	 * @brief Enables or disables depth testing.
	 * @param enable True to enable depth testing.
	 * @return *this for chaining.
	 */
	PipelineBuilder& SetDepthTest(bool enable);

	/**
	 * @brief Enables or disables depth writes.
	 * @param enable True to enable depth writes.
	 * @return *this for chaining.
	 */
	PipelineBuilder& SetDepthWrite(bool enable);

	/**
	 * @brief Sets the depth comparison operator.
	 * @param op The VkCompareOp to use.
	 * @return *this for chaining.
	 */
	PipelineBuilder& SetDepthCompareOp(VkCompareOp op);

	/**
	 * @brief Sets the depth attachment format for dynamic rendering.
	 * @param format The VkFormat to use.
	 * @return *this for chaining.
	 */
	PipelineBuilder& SetDepthFormat(VkFormat format);

	/**
	 * @brief Enables depth bias and sets its constant and slope factors.
	 * @param constant_factor The constant depth bias factor.
	 * @param slope_factor The slope-scaled depth bias factor.
	 * @return *this for chaining.
	 */
	PipelineBuilder& SetDepthBias(float constant_factor, float slope_factor);

	/**
	 * @brief Appends a push constant range to the pipeline layout.
	 * @param stages The shader stage flags.
	 * @param offset The byte offset of the range.
	 * @param size The byte size of the range.
	 * @return *this for chaining.
	 */
	PipelineBuilder& AddPushConstantRange(VkShaderStageFlags stages, uint32_t offset, uint32_t size);

	/**
	 * @brief Appends a descriptor set layout to the pipeline layout.
	 * @param layout The VkDescriptorSetLayout to append.
	 * @return *this for chaining.
	 */
	PipelineBuilder& AddDescriptorSetLayout(VkDescriptorSetLayout layout);

	/**
	 * @brief Enables or disables alpha blending on the single color attachment.
	 * @param enable True to enable blending.
	 * @return *this for chaining.
	 */
	PipelineBuilder& SetBlendEnabled(bool enable);

	/**
	 * @brief Enables or disables stencil testing.
	 * @param enable True to enable stencil testing.
	 * @return *this for chaining.
	 */
	PipelineBuilder& SetStencilTest(bool enable);

	/**
	 * @brief Sets the stencil operation state for front (and back) faces.
	 * @param fail_op Operation when stencil test fails.
	 * @param depth_fail_op Operation when depth test fails.
	 * @param pass_op Operation when both tests pass.
	 * @param compare_op Comparison function for the stencil test.
	 * @return *this for chaining.
	 */
	PipelineBuilder& SetStencilOp(
		VkStencilOp fail_op,
		VkStencilOp depth_fail_op,
		VkStencilOp pass_op,
		VkCompareOp compare_op);

	/**
	 * @brief Sets the stencil write and compare masks.
	 * @param write_mask Bits written by the stencil test.
	 * @param compare_mask Bits used in the stencil comparison.
	 * @return *this for chaining.
	 */
	PipelineBuilder& SetStencilMasks(uint32_t write_mask, uint32_t compare_mask);

	/**
	 * @brief Sets the stencil attachment format for dynamic rendering.
	 * @param format The VkFormat to use.
	 * @return *this for chaining.
	 */
	PipelineBuilder& SetStencilFormat(VkFormat format);

	/**
	 * @brief Sets the color component write mask for the single color attachment.
	 * @param mask The VkColorComponentFlags to use.
	 * @return *this for chaining.
	 */
	PipelineBuilder& SetColorWriteMask(VkColorComponentFlags mask);

	/**
	 * @brief Appends an extra dynamic state to the pipeline's dynamic state list.
	 * @param state The VkDynamicState to append.
	 * @return *this for chaining.
	 */
	PipelineBuilder& AddDynamicState(VkDynamicState state);

	/**
	 * @brief Creates the VkPipelineLayout and VkGraphicsPipeline, writing them into out_pipeline.
	 *
	 * Validates that a vertex shader and color format have been set. Pre-emptively cleans up
	 * any prior state in out_pipeline. On failure, out_pipeline is left in the default-constructed
	 * state.
	 *
	 * @param device The Device to create Vulkan objects against.
	 * @param out_pipeline The Pipeline to write results into.
	 * @return RenderError::None on success, or a specific error code on failure.
	 */
	[[nodiscard]] RenderError Build(const Device& device, Pipeline& out_pipeline);

private:
	// Shader stages
	VkShaderModule				_vertexShader		= VK_NULL_HANDLE;
	VkShaderModule				_fragmentShader		= VK_NULL_HANDLE;

	// Vertex input
	uint32_t				_vertexStride		= 0;
	VkVertexInputRate			_vertexInputRate	= VK_VERTEX_INPUT_RATE_VERTEX;
	std::vector<VertexAttribute>		_vertexAttributes;

	// Dynamic rendering formats
	VkFormat				_colorFormat		= VK_FORMAT_UNDEFINED;
	VkFormat				_depthFormat		= VK_FORMAT_UNDEFINED;
	VkFormat				_stencilFormat		= VK_FORMAT_UNDEFINED;

	// Viewport/scissor extent
	VkExtent2D				_extent			= {0, 0};

	// Rasterization
	VkPrimitiveTopology			_topology		= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	VkPolygonMode				_polygonMode		= VK_POLYGON_MODE_FILL;
	VkCullModeFlags				_cullMode		= VK_CULL_MODE_BACK_BIT;
	VkFrontFace				_frontFace		= VK_FRONT_FACE_COUNTER_CLOCKWISE;
	bool					_depthBiasEnable	= false;
	float					_depthBiasConstant	= 0.0f;
	float					_depthBiasSlope		= 0.0f;

	// Depth
	bool					_depthTestEnable	= false;
	bool					_depthWriteEnable	= false;
	VkCompareOp				_depthCompareOp		= VK_COMPARE_OP_LESS;

	// Stencil
	bool					_stencilTestEnable	= false;
	VkStencilOpState			_stencilOpState		= {};

	// Blending
	bool					_blendEnable		= false;
	VkColorComponentFlags			_colorWriteMask		=
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;

	// Layout inputs
	std::vector<VkPushConstantRange>	_pushConstantRanges;
	std::vector<VkDescriptorSetLayout>	_descriptorSetLayouts;

	// Extra dynamic states
	std::vector<VkDynamicState>		_dynamicStates;
};

} // namespace virasa

#endif // VIRASA_RENDERER_RESOURCES_PIPELINE_H
