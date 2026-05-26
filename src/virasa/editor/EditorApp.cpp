#include "virasa/editor/EditorApp.h"

#include <quill/LogMacros.h>

#include <algorithm>
#include <cstdint>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "virasa/core/Logger.h"
#include "virasa/ecs/Components.h"
#include "virasa/ecs/Types.h"
#include "virasa/ecs/World.h"
#include "virasa/editor/ViewManager.h"
#include "virasa/math/Transform.h"
#include "virasa/math/Types.h"
#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Context.h"
#include "virasa/renderer/scene/SceneRenderer.h"
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
		world.AddTransformComponent(cubeEntity, virasa::math::Transform::Identity());
		world.AddMeshComponent(cubeEntity, virasa::ecs::MeshComponent{cubeMeshId});
		world.AddVisualComponent(cubeEntity, virasa::ecs::VisualComponent{kDefaultMaterialId});
	}

	// Directional light entity
	{
		virasa::ecs::Entity lightEntity = world.CreateEntity("DirectionalLight");
		virasa::ecs::DirectionalLightComponent light;
		light.direction = virasa::math::Vec3(-1.0f, -1.0f, -1.0f);
		light.color = virasa::math::Vec3(1.0f, 1.0f, 1.0f);
		light.intensity = 1.0f;
		world.AddDirectionalLightComponent(lightEntity, light);
	}

	// Camera entity
	virasa::ecs::Entity cameraEntity;
	{
		cameraEntity = world.CreateEntity("Camera");

		virasa::math::Transform camTransform;
		camTransform.translation = _cameraPosition;
		world.AddTransformComponent(cameraEntity, camTransform);

		virasa::ecs::CameraComponent cam;
		cam.domain = virasa::CameraDomain::Editor;
		cam.fovY = glm::radians(45.0f);
		cam.nearPlane = 0.1f;
		cam.farPlane = 100.0f;
		world.AddCameraComponent(cameraEntity, cam);
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

	// -------------------------------------------------------------------------
	// Stage 8: Input state
	// -------------------------------------------------------------------------
	virasa::InputState inputState;

	// -------------------------------------------------------------------------
	// Stage 9: Main loop
	// -------------------------------------------------------------------------
	const float kCommandBarPaddingY =
		viewManager.GetCommandBarView().GetPanel().GetConfig().paddingY;

	bool running = true;
	while (running)
	{
		// ------------------------------------------------------------------
		// Step 1: Events
		// ------------------------------------------------------------------
		auto events = platform.PollEvents();

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

			if (!shouldExit)
			{
				if (viewManager.HandleEvent(event, world) ==
					virasa::editor::EventResult::QuitRequested)
				{
					LOG_INFO(logger, "QuitRequested from ViewManager.");
					shouldExit = true;
				}
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
		// Step 2: Input state
		// ------------------------------------------------------------------
		inputState.Update(events);

		// ------------------------------------------------------------------
		// Step 3: Camera control
		// ------------------------------------------------------------------
		if (inputState.IsMouseButtonDown(virasa::MouseButton::Middle))
		{
			auto [dx, dy] = inputState.GetMouseDelta();

			const float kRotateSensitivity = 0.005f;
			const float kPanSensitivity = 0.01f;
			const float kPitchLimit = glm::radians(89.0f);

			virasa::math::Quat yawQuat =
				glm::angleAxis(_cameraYaw, virasa::math::Vec3(0.0f, 0.0f, 1.0f));
			virasa::math::Quat pitchQuat =
				glm::angleAxis(_cameraPitch, virasa::math::Vec3(1.0f, 0.0f, 0.0f));
			virasa::math::Quat orientation = yawQuat * pitchQuat;

			const bool ctrlHeld = inputState.IsKeyDown(virasa::KeyCode::LCtrl) ||
						    inputState.IsKeyDown(virasa::KeyCode::RCtrl);

			if (ctrlHeld)
			{
				_cameraPosition += (orientation * virasa::math::Vec3(1.0f, 0.0f, 0.0f)) *
							 (-static_cast<float>(dx) * kPanSensitivity);
				_cameraPosition += (orientation * virasa::math::Vec3(0.0f, 0.0f, 1.0f)) *
							 (static_cast<float>(dy) * kPanSensitivity);
			}
			else
			{
				_cameraYaw -= static_cast<float>(dx) * kRotateSensitivity;
				_cameraPitch -= static_cast<float>(dy) * kRotateSensitivity;
				_cameraPitch = std::clamp(_cameraPitch, -kPitchLimit, kPitchLimit);
			}

			// Recompute orientation after potential yaw/pitch change
			yawQuat = glm::angleAxis(_cameraYaw, virasa::math::Vec3(0.0f, 0.0f, 1.0f));
			pitchQuat = glm::angleAxis(_cameraPitch, virasa::math::Vec3(1.0f, 0.0f, 0.0f));
			orientation = yawQuat * pitchQuat;

			auto& camTransform = world.GetTransformComponent(cameraEntity);
			camTransform.translation = _cameraPosition;
			camTransform.rotation = orientation;
		}
		else
		{
			// Still update translation in case position changed elsewhere
			auto& camTransform = world.GetTransformComponent(cameraEntity);
			camTransform.translation = _cameraPosition;
		}

		// ------------------------------------------------------------------
		// Step 4: Viewport layout
		// ------------------------------------------------------------------
		VkExtent2D extent = context.GetSwapchainExtent();

		const float ascender = fontAtlas.GetAscender();
		const float descender = fontAtlas.GetDescender();
		const uint32_t barHeightPixels =
			static_cast<uint32_t>(std::min(static_cast<float>(extent.height - 1u),
				ascender - descender + 2.0f * kCommandBarPaddingY));

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
		// Step 6: Render world
		// ------------------------------------------------------------------
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
