#ifndef VIRASA_RENDERER_TEXT_UIPASS_H
#define VIRASA_RENDERER_TEXT_UIPASS_H

#include <cstdint>
#include <span>

#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Context.h"
#include "virasa/renderer/core/Device.h"
#include "virasa/renderer/graph/Graph.h"
#include "virasa/renderer/graph/Pass.h"
#include "virasa/renderer/graph/Types.h"
#include "virasa/renderer/resources/BindlessTextureArray.h"
#include "virasa/renderer/resources/Buffer.h"
#include "virasa/renderer/resources/Image.h"
#include "virasa/renderer/resources/Pipeline.h"
#include "virasa/renderer/resources/Sampler.h"
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
	UiPass(UiPass&&) noexcept;
	UiPass& operator=(UiPass&&) noexcept;
	~UiPass();

	/**
	 * @brief Initialize the UI pass resources.
	 * @param device Renderer device used to create resources.
	 * @param context Renderer context supplying swapchain format and transfer resources.
	 * @param atlas Font atlas whose bitmap is uploaded once for text rendering.
	 * @param bindless Bindless texture array whose layout is borrowed during initialization.
	 * @return virasa::RenderError::None on success, otherwise a specific failure code.
	 */
	[[nodiscard]] RenderError Initialize(const Device& device, const Context& context,
		const virasa::ui::FontAtlas& atlas, const BindlessTextureArray& bindless);

	/**
	 * @brief Rebuild UI geometry and append one overlay pass to the graph.
	 * @param graph Graph receiving the appended pass.
	 * @param swapchainTarget Target image handle to composite onto.
	 * @param list Draw list containing quads, image quads, and text commands.
	 * @param atlas Font atlas used for glyph metrics and UV normalization.
	 * @param sampledImages Images that must be declared as fragment-sampled for this pass.
	 * @param bindlessSet Descriptor set bound for image-quad sampling.
	 * @param windowWidth Window width in framebuffer pixels.
	 * @param windowHeight Window height in framebuffer pixels.
	 */
	void Submit(virasa::renderer::graph::Graph& graph,
		virasa::renderer::graph::ImageHandle swapchainTarget, const virasa::ui::DrawList& list,
		const virasa::ui::FontAtlas& atlas,
		std::span<const virasa::renderer::graph::ImageHandle> sampledImages,
		VkDescriptorSet bindlessSet, uint32_t windowWidth, uint32_t windowHeight,
		uint32_t frameIndex);

	private:
	Image _atlasImage = {};
	Sampler _linearSampler = {};
	Pipeline _quadPipeline = {};
	Pipeline _textPipeline = {};
	Pipeline _imageQuadPipeline = {};
	Buffer _geometryBuffer = {};
	VkDeviceSize _perFrameSliceSize = 0;
	uint32_t _framesInFlight = 0;
	const Device* _device = nullptr;
	VkDescriptorSetLayout _textSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool _textDescriptorPool = VK_NULL_HANDLE;
	VkDescriptorSet _textDescriptorSet = VK_NULL_HANDLE;
	bool _initialized = false;
};
} // namespace virasa::renderer::text

#endif
