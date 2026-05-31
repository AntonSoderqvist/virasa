#include "virasa/editor/EditorApp.h"

#include <quill/LogMacros.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <future>
#include <glm/common.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <string_view>
#include <vector>

#include "virasa/core/Logger.h"
#include "virasa/ecs/Components.h"
#include "virasa/ecs/Types.h"
#include "virasa/ecs/World.h"
#include "virasa/editor/ViewManager.h"
#include "virasa/editor/io/GltfLoader.h"
#include "virasa/math/Transform.h"
#include "virasa/math/Types.h"
#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Context.h"
#include "virasa/renderer/scene/SceneRenderer.h"
#include "virasa/ui/CommandBar.h"
#include "virasa/ui/FontAtlas.h"
#include "virasa/ui/Types.h"
#include "virasa/window/Events.h"
#include "virasa/window/InputState.h"
#include "virasa/window/Platform.h"

namespace virasa::editor
{

int EditorApp::Run(int argc, char** argv)
{
	(void)argc;
	(void)argv;

	// -------------------------------------------------------------------------
	// Stage 1: Logger
	// -------------------------------------------------------------------------
	auto* logger = virasa::Logger::GetLogger("Editor");
	LOG_INFO(logger, "Entry point reached.");

	// -------------------------------------------------------------------------
	// Stage 2: Platform
	// -------------------------------------------------------------------------
	virasa::window::Platform platform;
	{
		virasa::ErrorCode err = platform.Initialize("VirasaEditor", 800, 800);
		if (err != virasa::ErrorCode::None)
		{
			LOG_ERROR(logger, "Platform initialization failed.");
			return -1;
		}
	}

	// -------------------------------------------------------------------------
	// Stage 3: Renderer context
	// -------------------------------------------------------------------------
	virasa::Context context;
	{
		uint32_t extCount = 0;
		const char* const* exts =
			virasa::window::Platform::GetRequiredVulkanExtensions(&extCount);

		virasa::RendererConfig rendererConfig;
		rendererConfig.requiredInstanceExtensions = exts;
		rendererConfig.requiredInstanceExtensionCount = extCount;
		// Two frames in flight. The transient render targets (scene color/depth,
		// shadow maps) and the per-frame light/shadow buffers are now
		// per-frame-in-flight (ImageRegistry frame partitions + LightTable/
		// ShadowTable buffer rings), so frame N and N+1 never share a resource
		// and the earlier cross-frame black-flash hazard is gone.
		rendererConfig.maxFramesInFlight = 2;

		virasa::RenderError err = context.Initialize(platform, rendererConfig);
		if (err != virasa::RenderError::None)
		{
			LOG_ERROR(logger, "Context initialization failed.");
			return -1;
		}
	}

	// -------------------------------------------------------------------------
	// Stage 4: Font atlas
	// -------------------------------------------------------------------------
	virasa::ui::FontAtlas fontAtlas;
	{
		virasa::ui::FontAtlasError err =
			fontAtlas.Initialize("fonts/JetBrainsMono-Regular.ttf", 20u);
		if (err != virasa::ui::FontAtlasError::None)
		{
			LOG_ERROR(logger, "FontAtlas initialization failed.");
			return -1;
		}
	}

	// -------------------------------------------------------------------------
	// Stage 5: Scene renderer
	// -------------------------------------------------------------------------
	virasa::renderer::scene::SceneRenderer sceneRenderer;
	{
		virasa::RenderError err =
			sceneRenderer.Initialize(context.GetDevice(), context, fontAtlas);
		if (err != virasa::RenderError::None)
		{
			LOG_ERROR(logger, "SceneRenderer initialization failed.");
			return -1;
		}
	}

	uint32_t cubeMeshId = sceneRenderer.CreateDefaultCubeMesh();
	if (cubeMeshId == 0xFFFFFFFFu)
	{
		LOG_ERROR(logger, "CreateDefaultCubeMesh failed.");
		return -1;
	}

	constexpr uint32_t kDefaultMaterialId = 0u;

	// -------------------------------------------------------------------------
	// Stage 6: World seeding
	// -------------------------------------------------------------------------
	virasa::ecs::World world;

	// Cube entity
	{
		virasa::ecs::Entity cubeEntity = world.CreateEntity("Cube");
		world.Transforms().Add(cubeEntity, virasa::math::Transform::Identity());

		virasa::ecs::ComponentId meshSysId = world.GetSystemId("Mesh");
		virasa::ecs::MeshComponent meshComp;
		meshComp.meshId = cubeMeshId;
		world.GetSystem(meshSysId).AddRaw(cubeEntity, &meshComp);

		virasa::ecs::ComponentId visualSysId = world.GetSystemId("Visual");
		virasa::ecs::VisualComponent visualComp;
		visualComp.materialId = kDefaultMaterialId;
		world.GetSystem(visualSysId).AddRaw(cubeEntity, &visualComp);
	}

	// Directional light entity
	{
		virasa::ecs::Entity lightEntity = world.CreateEntity("DirectionalLight");
		virasa::ecs::DirectionalLightComponent light;
		light.direction = virasa::math::Vec3(-1.0f, -1.0f, -1.0f);
		light.color = virasa::math::Vec3(1.0f, 1.0f, 1.0f);
		light.intensity = 1.0f;
		virasa::ecs::ComponentId lightSysId = world.GetSystemId("DirectionalLight");
		world.GetSystem(lightSysId).AddRaw(lightEntity, &light);
	}

	// Camera entity
	virasa::ecs::Entity cameraEntity;
	{
		cameraEntity = world.CreateEntity("Camera");

		virasa::math::Transform camTransform;
		camTransform.translation = _cameraPosition;
		world.Transforms().Add(cameraEntity, camTransform);

		virasa::ecs::CameraComponent cam;
		cam.domain = virasa::CameraDomain::Editor;
		cam.fovY = glm::radians(45.0f);
		cam.nearPlane = 0.1f;
		cam.farPlane = 100.0f;
		virasa::ecs::ComponentId camSysId = world.GetSystemId("Camera");
		world.GetSystem(camSysId).AddRaw(cameraEntity, &cam);
	}

	// Initialise camera yaw/pitch
	_cameraYaw = glm::radians(-135.0f);
	_cameraPitch = glm::radians(-30.0f);

	// -------------------------------------------------------------------------
	// Stage 7: View manager
	// -------------------------------------------------------------------------
	virasa::editor::ViewManager viewManager;
	{
		virasa::ui::CommandBarConfig cbConfig =
			viewManager.GetCommandBarView().GetPanel().GetConfig();
		cbConfig.paddingY = 2.0f;
		viewManager.GetCommandBarView().GetPanel().SetConfig(cbConfig);
	}
	viewManager.GetEditorView().GetBuffer().SetText("// scratch\nhello editor\n");

	// Seed the EditorView buffer text per the contract
	// (already done above; the contract specifies "// scratch\nhello editor\n")

	// -------------------------------------------------------------------------
	// Stage 8: Input state
	// -------------------------------------------------------------------------
	virasa::InputState inputState;

	// -------------------------------------------------------------------------
	// Stage 9: Main loop
	// -------------------------------------------------------------------------
	std::vector<std::future<virasa::editor::io::GltfCpuAsset>> _pendingLoads;
	std::vector<virasa::ecs::Entity> hoverHighlighted;
	std::vector<virasa::ecs::Entity> selectionHighlighted;

	bool running = true;
	while (running)
	{
		// ------------------------------------------------------------------
		// Step 1: Events
		// ------------------------------------------------------------------
		auto events = platform.PollEvents();
		bool clickPending = false;
		int32_t clickX = 0;
		int32_t clickY = 0;

		for (const auto& event : events)
		{
			bool shouldExit = false;

			if (event.type == virasa::EventType::Quit)
			{
				LOG_INFO(logger, "Quit event received.");
				shouldExit = true;
			}
			else if (event.type == virasa::EventType::KeyDown &&
				 event.keyboard.key == virasa::KeyCode::Escape)
			{
				if (viewManager.GetRightPanelMode() ==
						virasa::editor::RightPanelMode::Closed &&
					viewManager.GetCommandBarView().GetText().empty())
				{
					LOG_INFO(logger, "Escape exit condition met.");
					shouldExit = true;
				}
			}

			virasa::editor::EventResult result = virasa::editor::EventResult::Consumed;
			if (!shouldExit)
			{
				result = viewManager.HandleEvent(event, world);
				if (result == virasa::editor::EventResult::QuitRequested)
				{
					LOG_INFO(logger, "QuitRequested from ViewManager.");
					shouldExit = true;
				}
				else if (result == virasa::editor::EventResult::LoadModelRequested)
				{
					std::string path(viewManager.GetPendingLoadPath());
					if (!path.empty() && path.front() == '~' &&
						(path.size() == 1u || path[1] == '/'))
					{
						if (const char* home = std::getenv("HOME"))
						{
							path.replace(0u, 1u, home);
						}
					}
					_pendingLoads.push_back(std::async(std::launch::async,
						[p = std::move(path)]() mutable
						{
							return virasa::editor::io::ParseGlb(p);
						}));
				}
			}

			if (event.type == virasa::EventType::MouseButtonDown &&
				event.mouseButton.button == virasa::MouseButton::Left)
			{
				clickPending = true;
				clickX = event.mouseButton.x;
				clickY = event.mouseButton.y;
			}

			if (shouldExit)
			{
				running = false;
				break;
			}
		}

		if (!running)
		{
			break;
		}

		// ------------------------------------------------------------------
		// Step 1b: Commit completed parses
		// ------------------------------------------------------------------
		{
			auto it = _pendingLoads.begin();
			while (it != _pendingLoads.end())
			{
				if (it->wait_for(std::chrono::seconds(0)) == std::future_status::ready)
				{
					virasa::editor::io::GltfCpuAsset asset = it->get();
					it = _pendingLoads.erase(it);

					if (asset.error == virasa::editor::io::GltfLoadError::None)
					{
						virasa::editor::io::GltfLoadResult commitResult =
							virasa::editor::io::CommitGlb(
								asset, world, sceneRenderer);
						if (commitResult.error !=
							virasa::editor::io::GltfLoadError::None)
						{
							LOG_ERROR(logger,
								"CommitGlb failed with error {}.",
								static_cast<uint32_t>(commitResult.error));
						}
					}
					else
					{
						LOG_ERROR(logger,
							"ParseGlb failed with error {}.",
							static_cast<uint32_t>(asset.error));
					}
				}
				else
				{
					++it;
				}
			}
		}

		// ------------------------------------------------------------------
		// Step 2: Input state
		// ------------------------------------------------------------------
		inputState.Update(events);

		float wheelScrollY = 0.0f;
		for (const auto& event : events)
		{
			if (event.type == virasa::EventType::MouseWheel)
			{
				wheelScrollY += event.mouseWheel.scrollY;
			}
		}

		// ------------------------------------------------------------------
		// Step 3: Camera control
		// ------------------------------------------------------------------
		{
			const float kRotateSensitivity = 0.005f;
			const float kPanSensitivity = 0.01f;
			const float kZoomSensitivity = 0.5f;
			const float kPitchLimit = glm::radians(89.0f);

			virasa::math::Quat yawQuat =
				glm::angleAxis(_cameraYaw, virasa::math::Vec3(0.0f, 0.0f, 1.0f));
			virasa::math::Quat pitchQuat =
				glm::angleAxis(_cameraPitch, virasa::math::Vec3(1.0f, 0.0f, 0.0f));
			virasa::math::Quat orientation = yawQuat * pitchQuat;

			if (inputState.IsMouseButtonDown(virasa::MouseButton::Middle))
			{
				auto [dx, dy] = inputState.GetMouseDelta();

				const bool ctrlHeld = inputState.IsKeyDown(virasa::KeyCode::LCtrl) ||
							    inputState.IsKeyDown(virasa::KeyCode::RCtrl);

				if (ctrlHeld)
				{
					_cameraPosition +=
						(orientation * virasa::math::Vec3(1.0f, 0.0f, 0.0f)) *
						(-static_cast<float>(dx) * kPanSensitivity);
					_cameraPosition +=
						(orientation * virasa::math::Vec3(0.0f, 0.0f, 1.0f)) *
						(static_cast<float>(dy) * kPanSensitivity);
				}
				else
				{
					_cameraYaw -= static_cast<float>(dx) * kRotateSensitivity;
					_cameraPitch -= static_cast<float>(dy) * kRotateSensitivity;
					_cameraPitch = std::clamp(_cameraPitch, -kPitchLimit, kPitchLimit);

					yawQuat = glm::angleAxis(
						_cameraYaw, virasa::math::Vec3(0.0f, 0.0f, 1.0f));
					pitchQuat = glm::angleAxis(
						_cameraPitch, virasa::math::Vec3(1.0f, 0.0f, 0.0f));
					orientation = yawQuat * pitchQuat;
				}
			}

			if (wheelScrollY != 0.0f)
			{
				_cameraPosition += (orientation * virasa::math::Vec3(0.0f, -1.0f, 0.0f)) *
							 (wheelScrollY * kZoomSensitivity);
			}

			virasa::math::Transform camTransform = world.Transforms().GetLocal(cameraEntity);
			camTransform.translation = _cameraPosition;
			camTransform.rotation = orientation;
			world.Transforms().SetLocal(cameraEntity, camTransform);
		}

		// ------------------------------------------------------------------
		// Step 4: Viewport layout
		// ------------------------------------------------------------------
		VkExtent2D extent = context.GetSwapchainExtent();

		const float commandBarPaddingY =
			viewManager.GetCommandBarView().GetPanel().GetConfig().paddingY;
		const float ascender = fontAtlas.GetAscender();
		const float descender = fontAtlas.GetDescender();
		const uint32_t barHeightPixels = static_cast<uint32_t>(std::min(
			static_cast<float>(extent.height - 1u),
			ascender - descender + 2.0f * commandBarPaddingY));

		uint32_t sceneWidth;
		if (viewManager.GetRightPanelMode() != virasa::editor::RightPanelMode::Closed)
		{
			sceneWidth = extent.width / 2u;
		}
		else
		{
			sceneWidth = extent.width;
		}
		const uint32_t sceneHeight = extent.height - barHeightPixels;

		// ------------------------------------------------------------------
		// Step 5: Begin frame
		// ------------------------------------------------------------------
		virasa::SwapchainStatus beginStatus = sceneRenderer.BeginFrame(sceneWidth, sceneHeight);
		if (beginStatus == virasa::SwapchainStatus::NotReady)
		{
			continue;
		}
		if (beginStatus == virasa::SwapchainStatus::Error)
		{
			LOG_ERROR(logger, "BeginFrame returned Error.");
			return -1;
		}

		// ------------------------------------------------------------------
		// Step 5c: Request viewport pick
		// ------------------------------------------------------------------
		if (clickPending && clickX >= 0 && clickY >= 0 &&
			static_cast<uint32_t>(clickX) < sceneWidth &&
			static_cast<uint32_t>(clickY) < sceneHeight)
		{
			sceneRenderer.RequestPick(static_cast<uint32_t>(clickX),
				static_cast<uint32_t>(clickY));
		}

		// ------------------------------------------------------------------
		// Step 5d: Consume pick result
		// ------------------------------------------------------------------
		{
			virasa::PickResult result = sceneRenderer.GetPickResult();
			if (result.valid)
			{
				viewManager.SetSelection(result.entity);
				viewManager.GetHierarchyView().SetCursorToEntity(world, result.entity);
			}
		}

		// ------------------------------------------------------------------
		// Step 5e: Selection highlight
		// ------------------------------------------------------------------
		{
			const virasa::math::Vec3 kSelectionHighlightColor{1.0f, 0.6f, 0.1f};
			const float kSelectionHighlightIntensity = 1.0f;
			const int32_t kSelectionHighlightPriority = 100;
			const int32_t kHoverHighlightPriority = 0;

			virasa::ecs::ComponentSystem* highlightSys = world.FindSystem("Highlight");
			if (highlightSys)
			{
				auto selection = viewManager.GetSelection();

				auto inDesired = [&](virasa::ecs::Entity e) {
					for (const virasa::ecs::Entity selected : selection)
					{
						if (selected == e)
						{
							return true;
						}
					}
					return false;
				};

				for (virasa::ecs::Entity e : selectionHighlighted)
				{
					if (inDesired(e))
					{
						continue;
					}
					if (world.IsValid(e) && highlightSys->Has(e))
					{
						const auto* existing =
							static_cast<const virasa::ecs::HighlightComponent*>(
								highlightSys->GetRaw(e));
						if (existing && existing->priority > kHoverHighlightPriority &&
							existing->priority <= kSelectionHighlightPriority)
						{
							highlightSys->Remove(e);
						}
					}
				}

				std::vector<virasa::ecs::Entity> nextSelection;
				for (const virasa::ecs::Entity e : selection)
				{
					if (!world.IsValid(e))
					{
						continue;
					}

					virasa::ecs::HighlightComponent newHighlight;
					newHighlight.color = kSelectionHighlightColor;
					newHighlight.intensity = kSelectionHighlightIntensity;
					newHighlight.priority = kSelectionHighlightPriority;

					if (!highlightSys->Has(e))
					{
						highlightSys->AddRaw(e, &newHighlight);
						nextSelection.push_back(e);
					}
					else
					{
						const auto* existing =
							static_cast<const virasa::ecs::HighlightComponent*>(
								highlightSys->GetRaw(e));
						if (existing && existing->priority <= kSelectionHighlightPriority)
						{
							highlightSys->SetRaw(e, &newHighlight);
							nextSelection.push_back(e);
						}
					}
				}

				selectionHighlighted = std::move(nextSelection);
			}
		}

		// ------------------------------------------------------------------
		// Step 5b: Hierarchy hover highlight
		// ------------------------------------------------------------------
		{
			const virasa::math::Vec3 kHoverHighlightColor{0.1f, 0.4f, 1.0f};
			const float kHoverHighlightIntensity = 1.0f;
			const int32_t kHoverHighlightPriority = 0;
			const float kHoverDescendantIntensity = 0.4f;
			const int32_t kHoverDescendantPriority = -1;

			virasa::ecs::ComponentSystem* highlightSys = world.FindSystem("Highlight");
			if (highlightSys)
			{
				virasa::ecs::Entity cursorEntity =
					viewManager.GetHierarchyView().GetCursorEntity(world);

				struct HoverMark
				{
					virasa::ecs::Entity entity;
					float intensity;
					int32_t priority;
				};
				std::vector<HoverMark> desired;
				if (cursorEntity != virasa::ecs::Entity::Invalid() &&
					world.IsValid(cursorEntity))
				{
					desired.push_back(
						{cursorEntity, kHoverHighlightIntensity, kHoverHighlightPriority});

					const auto& roots = world.GetChildren(cursorEntity);
					std::vector<virasa::ecs::Entity> stack(roots.begin(), roots.end());
					while (!stack.empty())
					{
						virasa::ecs::Entity e = stack.back();
						stack.pop_back();
						if (!world.IsValid(e))
						{
							continue;
						}
						desired.push_back(
							{e, kHoverDescendantIntensity, kHoverDescendantPriority});
						const auto& children = world.GetChildren(e);
						stack.insert(stack.end(), children.begin(), children.end());
					}
				}

				auto inDesired = [&](virasa::ecs::Entity e) {
					for (const auto& m : desired)
					{
						if (m.entity == e)
						{
							return true;
						}
					}
					return false;
				};

				for (virasa::ecs::Entity e : hoverHighlighted)
				{
					if (inDesired(e))
					{
						continue;
					}
					if (world.IsValid(e) && highlightSys->Has(e))
					{
						const auto* existing =
							static_cast<const virasa::ecs::HighlightComponent*>(
								highlightSys->GetRaw(e));
						if (existing && existing->priority <= kHoverHighlightPriority)
						{
							highlightSys->Remove(e);
						}
					}
				}

				std::vector<virasa::ecs::Entity> nextHover;
				for (const auto& m : desired)
				{
					if (!world.IsValid(m.entity))
					{
						continue;
					}
					virasa::ecs::HighlightComponent newHighlight;
					newHighlight.color = kHoverHighlightColor;
					newHighlight.intensity = m.intensity;
					newHighlight.priority = m.priority;

					if (!highlightSys->Has(m.entity))
					{
						highlightSys->AddRaw(m.entity, &newHighlight);
						nextHover.push_back(m.entity);
					}
					else
					{
						const auto* existing =
							static_cast<const virasa::ecs::HighlightComponent*>(
								highlightSys->GetRaw(m.entity));
						if (existing && existing->priority <= kHoverHighlightPriority)
						{
							highlightSys->SetRaw(m.entity, &newHighlight);
							nextHover.push_back(m.entity);
						}
					}
				}

				hoverHighlighted = std::move(nextHover);
			}
		}

		// ------------------------------------------------------------------
		// Step 6: Render world
		// ------------------------------------------------------------------
		// Resolve cached world matrices before the renderer reads them;
		// SceneRenderer::RenderWorld consumes TransformSystem's world cache
		// and no longer walks the hierarchy itself.
		world.UpdateTransforms();
		uint32_t sceneSlot = sceneRenderer.RenderWorld(world, cameraEntity);
		if (sceneSlot == 0xFFFFFFFFu)
		{
			LOG_ERROR(logger, "RenderWorld returned invalid scene slot.");
			return -1;
		}

		// ------------------------------------------------------------------
		// Step 7: Compose UI
		// ------------------------------------------------------------------
		virasa::ui::DrawList drawList;
		drawList.Clear();

		virasa::ui::ImageQuadCommand sceneQuad;
		sceneQuad.x = 0.0f;
		sceneQuad.y = 0.0f;
		sceneQuad.width = static_cast<float>(sceneWidth);
		sceneQuad.height = static_cast<float>(sceneHeight);
		sceneQuad.u0 = 0.0f;
		sceneQuad.v0 = 0.0f;
		sceneQuad.u1 = 1.0f;
		sceneQuad.v1 = 1.0f;
		sceneQuad.textureSlot = sceneSlot;
		drawList.AddImageQuad(sceneQuad);

		viewManager.Render(drawList, world, fontAtlas, extent.width, extent.height);

		// ------------------------------------------------------------------
		// Step 8: Submit
		// ------------------------------------------------------------------
		virasa::SwapchainStatus submitStatus =
			sceneRenderer.SubmitFrame(drawList, fontAtlas, extent.width, extent.height);
		if (submitStatus == virasa::SwapchainStatus::Error)
		{
			LOG_ERROR(logger, "SubmitFrame returned Error.");
			return -1;
		}
	}

	sceneRenderer.WaitIdle();
	return 0;
}

} // namespace virasa::editor
