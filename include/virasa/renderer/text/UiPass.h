#ifndef VIRASA_RENDERER_TEXT_UIPASS_H
#define VIRASA_RENDERER_TEXT_UIPASS_H

#include <cstdint>
#include <vector>

#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Context.h"
#include "virasa/renderer/core/Device.h"
#include "virasa/renderer/graph/Graph.h"
#include "virasa/renderer/graph/Pass.h"
#include "virasa/renderer/graph/Types.h"
#include "virasa/renderer/resources/Buffer.h"
#include "virasa/renderer/resources/Image.h"
#include "virasa/renderer/resources/Pipeline.h"
#include "virasa/ui/FontAtlas.h"
#include "virasa/ui/Types.h"
#include "vulkan/vulkan.h"

namespace virasa::renderer::text
{
/**
 * @brief Records and submits UI quad and text rendering.
 *
 * UiPass owns the uploaded font atlas image, the quad and text pipelines,
 * and the host-visible geometry buffer used to rebuild per-frame UI data.
 */
class UiPass final
{
	public:
	UiPass() = default;
	UiPass(const UiPass&) = delete;
	UiPass& operator=(const UiPass&) = delete;
	UiPass(UiPass&&) noexcept = delete;
	UiPass& operator=(UiPass&&) noexcept = delete;
	~UiPass();


	/**
	 * @brief Initialize the UI pass resources.
	 * @param device Renderer device used to create resources.
	 * @param context Renderer context supplying swapchain format and transfer pool.
	 * @param atlas Font atlas whose bitmap is uploaded once for text rendering.
	 * @return virasa::RenderError::None on success, otherwise a specific failure code.
	 */
	[[nodiscard]] RenderError Initialize(
		const Device& device, const Context& context, const virasa::ui::FontAtlas& atlas);

	/**
	 * @brief Rebuild UI geometry and append one overlay pass to the graph.
	 * @param graph Graph receiving the appended pass.
	 * @param swapchainTarget Target image handle to composite onto.
	 * @param list Draw list containing quads and text commands.
	 * @param atlas Font atlas used for glyph metrics and UV normalization.
	 * @param windowWidth Window width in framebuffer pixels.
	 * @param windowHeight Window height in framebuffer pixels.
	 */
	void Submit(virasa::renderer::graph::Graph& graph,
		virasa::renderer::graph::ImageHandle swapchainTarget, const virasa::ui::DrawList& list,
		const virasa::ui::FontAtlas& atlas, uint32_t frameIndex, uint32_t windowWidth,
		uint32_t windowHeight);

	private:
	VkDevice _device = VK_NULL_HANDLE;
	VkSampler _atlasSampler = VK_NULL_HANDLE;
	VkDescriptorSetLayout _textSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool _descriptorPool = VK_NULL_HANDLE;
	VkDescriptorSet _atlasDescriptorSet = VK_NULL_HANDLE;
	Image _atlasImage = {};
	Pipeline _quadPipeline = {};
	Pipeline _textPipeline = {};
	std::vector<Buffer> _geometryBuffers;
	bool _initialized = false;
};
} // namespace virasa::renderer::text

#endif
