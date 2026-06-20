#include "virasa/editor/EditorApp.h"

#include <quill/LogMacros.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <glm/common.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "virasa/core/Logger.h"
#include "virasa/ecs/Components.h"
#include "virasa/ecs/Scheduler.h"
#include "virasa/ecs/Types.h"
#include "virasa/ecs/World.h"
#include "virasa/editor/ViewManager.h"
#include "virasa/editor/io/GltfLoader.h"
#include "virasa/input/Bindings.h"
#include "virasa/input/Resolver.h"
#include "virasa/input/StepBridge.h"
#include "virasa/math/Transform.h"
#include "virasa/math/Types.h"
#include "virasa/renderer/Types.h"
#include "virasa/renderer/core/Context.h"
#include "virasa/renderer/scene/SceneRenderer.h"
#include "virasa/sim/AssetCatalog.h"
#include "virasa/sim/BehaviorRegistry.h"
#include "virasa/sim/Builtins.h"
#include "virasa/sim/ComponentCodec.h"
#include "virasa/sim/GameplayComponents.h"
#include "virasa/sim/ProjectLoader.h"
#include "virasa/sim/Scene.h"
#include "virasa/sim/SceneSerializer.h"
#include "virasa/sim/Tick.h"
#include "virasa/ui/CommandBar.h"
#include "virasa/ui/FontAtlas.h"
#include "virasa/ui/Types.h"
#include "virasa/window/Events.h"
#include "virasa/window/InputState.h"
#include "virasa/window/Platform.h"

namespace
{

class NullAssetResolver final : public virasa::sim::AssetResolver
{
	public:
	uint32_t Resolve(virasa::sim::AssetKind kind, std::string_view path) override
	{
		(void)kind;
		(void)path;
		return virasa::sim::kInvalidAssetId;
	}
};

} // namespace

namespace virasa::editor
{

int EditorApp::Run(int argc, char** argv)
{
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

	// -------------------------------------------------------------------------
	// Stage 5b: Runtime registries and catalogs
	// -------------------------------------------------------------------------
	virasa::sim::ComponentCodecRegistry codecRegistry;
	virasa::sim::RegisterBuiltinComponentCodecs(codecRegistry);

	virasa::sim::AssetCatalog assetCatalog;

	uint32_t cubeMeshId = sceneRenderer.CreateDefaultCubeMesh();
	assetCatalog.Bind(virasa::sim::AssetKind::Mesh, "builtin:cube", cubeMeshId);
	if (cubeMeshId == 0xFFFFFFFFu)
	{
		LOG_ERROR(logger, "CreateDefaultCubeMesh failed.");
		return -1;
	}

	uint32_t cylinderMeshId = sceneRenderer.CreateDefaultCylinderMesh();
	assetCatalog.Bind(virasa::sim::AssetKind::Mesh, "builtin:cylinder", cylinderMeshId);
	if (cylinderMeshId == 0xFFFFFFFFu)
	{
		LOG_ERROR(logger, "CreateDefaultCylinderMesh failed.");
		return -1;
	}

	constexpr uint32_t kDefaultMaterialId = 0u;
	assetCatalog.Bind(virasa::sim::AssetKind::Material, "builtin:default", kDefaultMaterialId);

	// -------------------------------------------------------------------------
	// Stage 6: Scene seeding
	// -------------------------------------------------------------------------
	virasa::sim::Scene authoredScene;
	virasa::ecs::Entity cameraEntity;

	auto restoreCameraFromScene = [&](virasa::sim::Scene& scene) -> bool
	{
		virasa::ecs::World& loadedWorld = scene.GetWorld();

		// Recover the viewport camera from the loaded scene's default camera,
		// falling back to the first entity carrying a CameraComponent when the
		// scene records no (valid) default camera.
		cameraEntity = scene.GetDefaultCamera();
		if (!loadedWorld.IsValid(cameraEntity))
		{
			virasa::ecs::ComponentId camSysId = loadedWorld.GetSystemId("Camera");
			if (camSysId != virasa::ecs::kInvalidComponentId)
			{
				const auto& camEntities = loadedWorld.GetSystem(camSysId).Entities();
				if (!camEntities.empty())
					cameraEntity = camEntities.front();
			}
		}

		const bool hasCamera = loadedWorld.IsValid(cameraEntity);
		// Restore the saved camera view into the editor navigation state so the
		// viewport jumps to where the camera was when the scene was saved. The
		// editor only ever writes yaw*pitch rotations, so the rotation decomposes
		// cleanly back into them.
		if (hasCamera && loadedWorld.Transforms().Has(cameraEntity))
		{
			const virasa::math::Transform camT =
				loadedWorld.Transforms().GetLocal(cameraEntity);
			_cameraPosition = camT.translation;
			const glm::mat3 r = glm::mat3_cast(camT.rotation);
			_cameraYaw = std::atan2(r[0][1], r[0][0]);
			_cameraPitch = std::atan2(r[1][2], r[2][2]);
			const float pitchLimit = glm::radians(89.0f);
			_cameraPitch = std::clamp(_cameraPitch, -pitchLimit, pitchLimit);
		}
		return hasCamera;
	};

	auto adoptAuthoredScene = [&](virasa::sim::Scene&& scene, bool resetCameraIfMissing) -> bool
	{
		authoredScene = std::move(scene);
		const bool hasCamera = restoreCameraFromScene(authoredScene);
		if (!hasCamera && resetCameraIfMissing)
		{
			_cameraYaw = glm::radians(-135.0f);
			_cameraPitch = glm::radians(-30.0f);
		}
		return hasCamera;
	};

	bool seedStarterScene = true;
	if (argc >= 2 && argv != nullptr && argv[1] != nullptr && argv[1][0] != '\0')
	{
		NullAssetResolver resolver;
		auto loaded = virasa::sim::OpenProject(argv[1], resolver, codecRegistry, assetCatalog);
		if (loaded.has_value())
		{
			adoptAuthoredScene(std::move(loaded->scene), true);
			seedStarterScene = false;
		}
		else
		{
			LOG_ERROR(logger, "Failed to open project directory: {}.", argv[1]);
		}
	}

	if (seedStarterScene)
	{
		virasa::ecs::World& world = authoredScene.GetWorld();
		virasa::sim::RegisterGameplayComponents(world);

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

			virasa::sim::SpinComponent spinComp;
			spinComp.angularVelocity = virasa::math::Vec3(0.0f, 0.0f, 1.0f);
			virasa::ecs::ComponentSystem* spinSys = world.FindSystem("Spin");
			if (spinSys)
			{
				spinSys->AddRaw(cubeEntity, &spinComp);
			}
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

		// Register the editor camera as the scene's default viewpoint so it is
		// persisted on save and recovered on load.
		authoredScene.SetDefaultCamera(cameraEntity);

		// Initialise camera yaw/pitch
		_cameraYaw = glm::radians(-135.0f);
		_cameraPitch = glm::radians(-30.0f);

		authoredScene.AddBehavior("Spin");
	}

	// -------------------------------------------------------------------------
	// Stage 6b: Behavior registry
	// -------------------------------------------------------------------------
	virasa::sim::BehaviorRegistry behaviorRegistry;
	virasa::sim::RegisterBuiltinBehaviors(behaviorRegistry);

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
	virasa::input::Bindings inputBindings;
	inputBindings.BindAxis(
		0, virasa::window::KeyCode::W, virasa::window::KeyCode::S, 1.0f);
	inputBindings.BindAxis(
		1, virasa::window::KeyCode::D, virasa::window::KeyCode::A, 1.0f);
	inputBindings.BindDigital(2, virasa::window::KeyCode::Space);
	inputBindings.BindDigital(3, virasa::window::KeyCode::LShift);
	virasa::input::Resolver inputResolver;
	virasa::input::StepBridge stepBridge;
	virasa::sim::Clock simClock;
	virasa::sim::Stepper simStepper(1.0f / 60.0f, 5u);
	virasa::ecs::Scheduler scheduler;
	scheduler.SetEnabled(false);

	// -------------------------------------------------------------------------
	// Stage 9: Main loop
	// -------------------------------------------------------------------------
	std::vector<std::future<virasa::editor::io::GltfCpuAsset>> _pendingLoads;
	std::vector<virasa::ecs::Entity> hoverHighlighted;
	std::vector<virasa::ecs::Entity> selectionHighlighted;

	bool playing = false;
	std::optional<virasa::sim::Scene> runtimeScene;
	auto activeWorld = [&]() -> virasa::ecs::World&
	{
		return playing && runtimeScene.has_value() ? runtimeScene->GetWorld()
									 : authoredScene.GetWorld();
	};

	auto expandHomePath = [](std::string path) -> std::string
	{
		if (!path.empty() && path.front() == '~' && (path.size() == 1u || path[1] == '/'))
		{
			if (const char* home = std::getenv("HOME"))
			{
				path.replace(0u, 1u, home);
			}
		}
		return path;
	};

	auto pathIsDirectory = [](const std::string& path) -> bool
	{
		std::error_code ec;
		return std::filesystem::is_directory(path, ec);
	};

	auto clearCommittedSelection = [&]()
	{
		viewManager.ClearSelection();
		selectionHighlighted.clear();
		hoverHighlighted.clear();
	};

	auto openProjectAtRuntime = [&](const std::string& path) -> bool
	{
		if (playing)
		{
			LOG_WARNING(logger,
				"Ignoring project load request while play mode is active: {}.",
				path);
			return false;
		}

		NullAssetResolver resolver;
		auto loaded = virasa::sim::OpenProject(path, resolver, codecRegistry, assetCatalog);
		if (!loaded.has_value())
		{
			LOG_ERROR(logger, "Failed to open project directory: {}.", path);
			return false;
		}

		adoptAuthoredScene(std::move(loaded->scene), false);
		clearCommittedSelection();
		return true;
	};

	bool running = true;
	simClock.Reset(); // discard initialization time so the first frame's delta is small
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
				result = viewManager.HandleEvent(event, activeWorld());
				if (result == virasa::editor::EventResult::QuitRequested)
				{
					LOG_INFO(logger, "QuitRequested from ViewManager.");
					shouldExit = true;
				}
				else if (result == virasa::editor::EventResult::LoadModelRequested)
				{
					std::string path =
						expandHomePath(std::string(viewManager.GetPendingLoadPath()));
					if (pathIsDirectory(path))
					{
						openProjectAtRuntime(path);
					}
					else
					{
						_pendingLoads.push_back(std::async(std::launch::async,
							[p = std::move(path)]() mutable
							{ return virasa::editor::io::ParseGlb(p); }));
					}
				}
				else if (result == virasa::editor::EventResult::LoadSceneRequested)
				{
					std::string path =
						expandHomePath(std::string(viewManager.GetPendingLoadPath()));
					if (pathIsDirectory(path))
					{
						openProjectAtRuntime(path);
					}
					else if (playing)
					{
						LOG_WARNING(logger,
							"Ignoring scene load request while play mode is active: "
							"{}.",
							path);
					}
					else
					{
						std::ifstream file(path, std::ios::binary);
						if (!file)
						{
							LOG_ERROR(logger, "Failed to read scene file: {}.", path);
						}
						else
						{
							std::ostringstream buffer;
							buffer << file.rdbuf();

							std::optional<virasa::sim::Scene> loadedScene =
								virasa::sim::DeserializeScene(
									buffer.str(), codecRegistry, assetCatalog);
							if (!loadedScene.has_value())
							{
								LOG_ERROR(logger,
									"Failed to deserialize scene file: {}.",
									path);
							}
							else
							{
								adoptAuthoredScene(std::move(*loadedScene), false);
								clearCommittedSelection();
							}
						}
					}
				}
				else if (result == virasa::editor::EventResult::SaveSceneRequested)
				{
					std::string path(viewManager.GetPendingSavePath());
					std::ofstream file(path, std::ios::binary | std::ios::trunc);
					if (!file)
					{
						LOG_ERROR(logger,
							"Failed to open scene file for writing: {}.",
							path);
					}
					else
					{
						const std::string document = virasa::sim::SerializeScene(
							authoredScene, codecRegistry, assetCatalog);
						file << document;
						if (!file)
						{
							LOG_ERROR(
								logger, "Failed to write scene file: {}.", path);
						}
					}
				}
				else if (result == virasa::editor::EventResult::PlayRequested)
				{
					if (!playing)
					{
						runtimeScene = authoredScene.Instantiate();
						if (!runtimeScene->BuildScheduler(behaviorRegistry, scheduler))
						{
							LOG_ERROR(logger,
								"Failed to build complete play-mode scheduler.");
						}
						scheduler.SetEnabled(true);
						playing = true;
					}
				}
				else if (result == virasa::editor::EventResult::StopRequested)
				{
					if (playing)
					{
						scheduler.SetEnabled(false);
						scheduler = virasa::ecs::Scheduler();
						scheduler.SetEnabled(false);
						runtimeScene.reset();
						playing = false;
					}
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
		virasa::ecs::World& world = activeWorld();
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
		const virasa::input::ActionState frameActions =
			inputResolver.Resolve(inputState, inputBindings);
		stepBridge.LatchFrame(frameActions);

		if (_touchpadScrollLatch > 0)
			--_touchpadScrollLatch;

		float wheelDollyY = 0.0f;
		float padScrollX = 0.0f;
		float padScrollY = 0.0f;

		for (const auto& event : events)
		{
			if (event.type == virasa::EventType::MouseWheel)
			{
				const bool hasTouchpadX = event.mouseWheel.scrollX != 0.0f;
				if (hasTouchpadX || _touchpadScrollLatch > 0)
				{
					padScrollX += event.mouseWheel.scrollX;
					padScrollY += event.mouseWheel.scrollY;
					if (hasTouchpadX)
						_touchpadScrollLatch = 4;
				}
				else
				{
					wheelDollyY += event.mouseWheel.scrollY;
				}
			}
		}

		// ------------------------------------------------------------------
		// Step 3: Camera control
		// ------------------------------------------------------------------
		{
			const float kRotateSensitivity = 0.005f;
			const float kPanSensitivity = 0.01f;
			const float kZoomSensitivity = 0.5f;
			const float kTouchpadRotateSensitivity = 0.05f;
			const float kTouchpadPanSensitivity = 1.0f;
			const float kTouchpadZoomSensitivity = 0.5f;
			const float kPitchLimit = glm::radians(89.0f);

			virasa::math::Quat yawQuat =
				glm::angleAxis(_cameraYaw, virasa::math::Vec3(0.0f, 0.0f, 1.0f));
			virasa::math::Quat pitchQuat =
				glm::angleAxis(_cameraPitch, virasa::math::Vec3(1.0f, 0.0f, 0.0f));
			virasa::math::Quat orientation = yawQuat * pitchQuat;

			// Middle-mouse: orbit or pan (Shift)
			if (inputState.IsMouseButtonDown(virasa::MouseButton::Middle))
			{
				auto [dx, dy] = inputState.GetMouseDelta();

				const bool shiftHeld = inputState.IsKeyDown(virasa::KeyCode::LShift) ||
							     inputState.IsKeyDown(virasa::KeyCode::RShift);

				if (shiftHeld)
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

			// Touchpad two-finger drag: orbit, pan (Shift), or zoom (Alt)
			if (padScrollX != 0.0f || padScrollY != 0.0f)
			{
				const bool shiftHeld = inputState.IsKeyDown(virasa::KeyCode::LShift) ||
							     inputState.IsKeyDown(virasa::KeyCode::RShift);
				const bool altHeld = inputState.IsKeyDown(virasa::KeyCode::LAlt) ||
							   inputState.IsKeyDown(virasa::KeyCode::RAlt);

				if (shiftHeld)
				{
					_cameraPosition +=
						(orientation * virasa::math::Vec3(1.0f, 0.0f, 0.0f)) *
						(-padScrollX * kTouchpadPanSensitivity);
					_cameraPosition +=
						(orientation * virasa::math::Vec3(0.0f, 0.0f, 1.0f)) *
						(padScrollY * kTouchpadPanSensitivity);
				}
				else if (altHeld)
				{
					// Alt + two-finger vertical scroll: zoom/dolly.
					_cameraPosition +=
						(orientation * virasa::math::Vec3(0.0f, -1.0f, 0.0f)) *
						(padScrollY * kTouchpadZoomSensitivity);
				}
				else
				{
					// Two-finger scroll: horizontal rotates yaw, vertical rotates pitch.
					_cameraYaw -= padScrollX * kTouchpadRotateSensitivity;
					_cameraPitch -= padScrollY * kTouchpadRotateSensitivity;
					_cameraPitch = std::clamp(_cameraPitch, -kPitchLimit, kPitchLimit);

					yawQuat = glm::angleAxis(
						_cameraYaw, virasa::math::Vec3(0.0f, 0.0f, 1.0f));
					pitchQuat = glm::angleAxis(
						_cameraPitch, virasa::math::Vec3(1.0f, 0.0f, 0.0f));
					orientation = yawQuat * pitchQuat;
				}
			}

			// Mouse wheel: zoom (dolly)
			if (wheelDollyY != 0.0f)
			{
				_cameraPosition += (orientation * virasa::math::Vec3(0.0f, -1.0f, 0.0f)) *
							 (wheelDollyY * kZoomSensitivity);
			}

			if (!playing && world.IsValid(cameraEntity) &&
				world.Transforms().Has(cameraEntity))
			{
				virasa::math::Transform camTransform =
					world.Transforms().GetLocal(cameraEntity);
				camTransform.translation = _cameraPosition;
				camTransform.rotation = orientation;
				world.Transforms().SetLocal(cameraEntity, camTransform);
			}
		}

		// ------------------------------------------------------------------
		// Step 3b: Fixed-step simulation
		// ------------------------------------------------------------------
		{
			const double rawDelta = simClock.Sample();
			const uint32_t simSteps = simStepper.Advance(rawDelta);
			for (uint32_t i = 0u; i < simSteps; ++i)
			{
				stepBridge.Publish(world, i == 0u);
				scheduler.Step(world, simStepper.NextStep());
			}
		}

		// ------------------------------------------------------------------
		// Step 4: Viewport layout
		// ------------------------------------------------------------------
		VkExtent2D extent = context.GetSwapchainExtent();

		const float commandBarPaddingY =
			viewManager.GetCommandBarView().GetPanel().GetConfig().paddingY;
		const float ascender = fontAtlas.GetAscender();
		const float descender = fontAtlas.GetDescender();
		const uint32_t barHeightPixels =
			static_cast<uint32_t>(std::min(static_cast<float>(extent.height - 1u),
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
			sceneRenderer.RequestPick(
				static_cast<uint32_t>(clickX), static_cast<uint32_t>(clickY));
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

				auto inDesired = [&](virasa::ecs::Entity e)
				{
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
						if (existing &&
							existing->priority <= kSelectionHighlightPriority)
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
				// The hierarchy hover highlight only tracks the cursor while the
				// hierarchy view holds input focus. When focus is elsewhere, the
				// cursor entity is treated as invalid so existing hover highlights
				// are cleared below.
				virasa::ecs::Entity cursorEntity =
					viewManager.GetFocus() == virasa::editor::Focus::Hierarchy
						? viewManager.GetHierarchyView().GetCursorEntity(world)
						: virasa::ecs::Entity::Invalid();

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
					desired.push_back({cursorEntity,
						kHoverHighlightIntensity,
						kHoverHighlightPriority});

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

				auto inDesired = [&](virasa::ecs::Entity e)
				{
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
		virasa::ecs::Entity renderCamera = cameraEntity;
		if (playing)
		{
			virasa::ecs::ComponentId camSysId = world.GetSystemId("Camera");
			if (camSysId != virasa::ecs::kInvalidComponentId)
			{
				const auto& camEntities = world.GetSystem(camSysId).Entities();
				for (virasa::ecs::Entity e : camEntities)
				{
					const auto* cam = static_cast<const virasa::ecs::CameraComponent*>(
						world.GetSystem(camSysId).GetRaw(e));
					if (cam != nullptr && cam->domain == virasa::CameraDomain::Main)
					{
						renderCamera = e;
						break;
					}
				}
			}
		}
		uint32_t sceneSlot = sceneRenderer.RenderWorld(world, renderCamera);
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
