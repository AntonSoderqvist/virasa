#ifndef VIRASA_RENDERER_SCENE_SCENE_RENDERER_H
#define VIRASA_RENDERER_SCENE_SCENE_RENDERER_H

#include <cstdint>
#include <unordered_map>

#include "virasa/ecs/Types.h"
#include "virasa/ecs/World.h"
#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Context.h"
#include "virasa/renderer/core/Device.h"
#include "virasa/renderer/graph/BufferRegistry.h"
#include "virasa/renderer/graph/Graph.h"
#include "virasa/renderer/graph/ImageRegistry.h"
#include "virasa/renderer/graph/Pass.h"
#include "virasa/renderer/graph/Types.h"
#include "virasa/renderer/lighting/LightTable.h"
#include "virasa/renderer/material/Visual.h"
#include "virasa/renderer/resources/BindlessTextureArray.h"
#include "virasa/renderer/resources/Image.h"
#include "virasa/renderer/resources/Mesh.h"
#include "virasa/renderer/resources/MeshRegistry.h"
#include "virasa/renderer/resources/Pipeline.h"
#include "virasa/renderer/resources/Sampler.h"
#include "virasa/renderer/resources/ShaderModule.h"
#include "virasa/renderer/text/UiPass.h"
#include "virasa/ui/FontAtlas.h"
#include "virasa/ui/Types.h"
#include "vulkan/vulkan.h"

namespace virasa::renderer::scene
{

/**
 * @brief Owns the complete set of renderer-internal resources required to draw an ECS world.
 *
 * SceneRenderer manages a MeshRegistry, VisualMaterialTable, LightTable, BindlessTextureArray,
 * default sampler, white image, forward shaders and pipeline, a render graph, and a UI pass.
 * It is the only contract that performs forward-rendering command recording from ECS data.
 * Applications consuming SceneRenderer do not record Vulkan commands themselves.
 */
class SceneRenderer final
{
	public:
	SceneRenderer() = default;
	SceneRenderer(const SceneRenderer&) = delete;
	SceneRenderer& operator=(const SceneRenderer&) = delete;
	SceneRenderer(SceneRenderer&&) = default;
	SceneRenderer& operator=(SceneRenderer&&) = default;
	~SceneRenderer() = default;

	/**
	 * @brief Initialize the SceneRenderer, creating all owned Vulkan resources.
	 * @param device The Device to borrow (must outlive this SceneRenderer).
	 * @param context The Context to borrow (must outlive this SceneRenderer).
	 * @param atlas The FontAtlas used to initialize the UI pass.
	 * @return RenderError::None on success, or a specific error code on failure.
	 */
	[[nodiscard]] virasa::RenderError Initialize(
		const Device& device, const Context& context, const virasa::ui::FontAtlas& atlas);

	/**
	 * @brief Returns a mutable reference to the owned MeshRegistry.
	 * @return Reference to _meshRegistry.
	 */
	[[nodiscard]] MeshRegistry& GetMeshRegistry() noexcept;

	/**
	 * @brief Returns a mutable reference to the owned VisualMaterialTable.
	 * @return Reference to _materialTable.
	 */
	[[nodiscard]] VisualMaterialTable& GetMaterialTable() noexcept;

	/**
	 * @brief Creates a default cube mesh and registers it in the MeshRegistry.
	 * @return The assigned mesh id, or 0xFFFFFFFFu on failure.
	 */
	[[nodiscard]] uint32_t CreateDefaultCubeMesh();

	/**
	 * @brief Allocates a default VisualMaterial in the material table.
	 * @return The assigned material id, or 0xFFFFFFFFu on failure.
	 */
	[[nodiscard]] uint32_t CreateDefaultMaterial();

	/**
	 * @brief Begins a new frame, acquiring a swapchain image and starting the render graph.
	 * @param sceneWidth Width of the scene render target in pixels.
	 * @param sceneHeight Height of the scene render target in pixels.
	 * @return SwapchainStatus indicating the result of the frame begin.
	 */
	[[nodiscard]] virasa::SwapchainStatus BeginFrame(uint32_t sceneWidth, uint32_t sceneHeight);

	/**
	 * @brief Appends the forward pass to the graph and returns the bindless scene color slot.
	 * @param world The ECS world to iterate for rendering.
	 * @param cameraEntity The entity whose CameraComponent and TransformComponent drive the view.
	 * @return The bindless texture slot of the scene color image, or 0xFFFFFFFFu on failure.
	 */
	[[nodiscard]] uint32_t RenderWorld(
		const virasa::ecs::World& world, virasa::ecs::Entity cameraEntity);

	/**
	 * @brief Appends the UI pass, compiles and executes the graph, then presents the frame.
	 * @param drawList The UI draw list to render.
	 * @param atlas The font atlas used by the UI pass.
	 * @param windowWidth Width of the window in pixels.
	 * @param windowHeight Height of the window in pixels.
	 * @return SwapchainStatus from EndFrame.
	 */
	[[nodiscard]] virasa::SwapchainStatus SubmitFrame(const virasa::ui::DrawList& drawList,
		const virasa::ui::FontAtlas& atlas, uint32_t windowWidth, uint32_t windowHeight);

	/**
	 * @brief Blocks until the GPU has drained all submitted work.
	 */
	void WaitIdle();

	private:
	const Device* _device = nullptr;
	const Context* _context = nullptr;

	MeshRegistry _meshRegistry = {};
	virasa::VisualMaterialTable _materialTable = {};
	virasa::LightTable _lightTable = {};
	virasa::BindlessTextureArray _bindlessTextures = {};
	virasa::Sampler _defaultSampler = {};
	virasa::Image _whiteImage = {};

	virasa::ShaderModule _forwardVertexShader = {};
	virasa::ShaderModule _forwardFragmentShader = {};
	virasa::Pipeline _forwardPipeline = {};

	virasa::renderer::graph::ImageRegistry _imageRegistry = {};
	virasa::renderer::graph::BufferRegistry _bufferRegistry = {};
	virasa::renderer::graph::Graph _graph = {};

	virasa::renderer::text::UiPass _uiPass = {};

	virasa::renderer::graph::ImageHandle _frameSwapchainHandle = {};
	virasa::renderer::graph::ImageHandle _frameSceneColorHandle = {};
	virasa::renderer::graph::ImageHandle _frameSceneDepthHandle = {};

	uint32_t _frameSceneWidth = 0u;
	uint32_t _frameSceneHeight = 0u;
	uint32_t _frameSceneSlot = 0xFFFFFFFFu;

	std::unordered_map<VkImageView, uint32_t> _sceneSlotCache = {};

	bool _initialized = false;
};

} // namespace virasa::renderer::scene

#endif // VIRASA_RENDERER_SCENE_SCENE_RENDERER_H
