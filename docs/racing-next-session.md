# Racing game — next-session handoff (Phase C + beyond)

## Where things stand (done)

A small racing game is being built on the virasa engine. **Phase A (sim wiring) and Phase B
(sample project) are complete and on `main`-equivalent working tree; full build + 669 tests
green.** What exists:

**Contracts + impl + tests (all lint-clean, codex-implemented, tested):**
- `virasa.physics.PhysicsWorld` 0.2 — `CastHit`, `RayCast`/`SphereCast` (with `ignore` entity),
  `AddForce`/`AddForceAtPoint`/`AddTorque`, `GetLinearVelocity`/`GetPointVelocity`, reverse
  BodyID→Entity map.
- `virasa.sim.VehicleComponent` — `WheelConfig[4]` + suspension/engine/steer params +
  throttle/steer/brake/handbrake `ActionId`s (defaults 0/1/2/3). Self-contained value data.
- `virasa.sim.behaviors.VehicleBehavior` — `Name()=="Vehicle"`, **Phase::PreStep**. Reads the
  shared `PhysicsWorld*` from the World resource store + `ActionState` via
  `StepBridge::GetActions`; per-wheel `SphereCast` → suspension + drive/brake + lateral-grip
  forces via `AddForceAtPoint`. Does NOT step/sync the world. Grip scales by the hit entity's
  `RigidBodyComponent.friction`.
- `virasa.sim.behaviors.ChaseCameraBehavior` — also defines `ChaseCameraComponent`;
  `Name()=="ChaseCamera"`, **Phase::PostStep**. Follows first Vehicle entity, smoothed eye +
  Z-up/Y-forward look-at, writes camera Transform.
- `virasa.sim.Builtins` 0.2 — registers the 5 component systems (Spin/RigidBody/Collider/
  **Vehicle**/**ChaseCamera**) and 4 behaviors (Spin/Physics Step, **Vehicle PreStep**,
  **ChaseCamera PostStep**).
- `virasa.sim.behaviors.PhysicsBehavior` 0.2 — publishes its owned `PhysicsWorld*` into the
  World resource store (key `std::type_index(typeid(virasa::physics::PhysicsWorld))`) at the
  start of `Step`.
- `virasa.sim.ComponentCodec` 0.2 — `"Vehicle"` + `"ChaseCamera"` codecs (13 builtins). Scenes
  round-trip.

**Sample project (data only):** `examples/racing/project.json` +
`examples/racing/scenes/main.scene.json` — Ground (static box), Car (Dynamic + Vehicle, AWD/
front-steer), ChaseCamera entity (`Camera` domain Main + `ChaseCamera`), DirectionalLight,
`"behaviors": ["Physics","Vehicle","ChaseCamera"]`.

## Why it doesn't drive yet — Phase C is host integration in `EditorApp`

The behaviors are registered and the scene exists, but **nothing feeds input into the sim and
the editor renders its own camera, not the scene's.** Both are in `src/virasa/editor/EditorApp.cpp`
and gated by the large `virasa.editor.EditorApp` contract.

### Task C1 — Input plumbing (the car can't read controls)

`EditorApp` has NO `Bindings`/`Resolver`/`StepBridge` in its loop today (grep confirms). The
fixed-step loop runs at roughly:
```
auto events = platform.PollEvents();           // ~line 405
...
const uint32_t simSteps = simStepper.Advance(rawDelta);   // ~line 749
for each step: scheduler.Step(world, simStepper.NextStep());  // ~line 752
```
`virasa::InputState inputState;` (~329) and `virasa::sim::Stepper simStepper` (~331) already exist.

Wire it (only meaningful while playing, but harmless always):
1. Once before the loop: build a `virasa::input::Bindings` and a host-owned
   `virasa::input::StepBridge stepBridge;` and a `virasa::input::Resolver resolver;`.
   Bindings (action ids match VehicleComponent defaults):
   - `BindAxis(0 /*throttle*/, KeyCode::W, KeyCode::S, 1.0f)` (and/or Up/Down)
   - `BindAxis(1 /*steer*/, KeyCode::D, KeyCode::A, 1.0f)`  (verify sign: +steer should turn the way ChaseCameraBehavior/VehicleBehavior expect; flip scale if needed)
   - `BindDigital(2 /*brake*/, KeyCode::Space)`
   - `BindDigital(3 /*handbrake*/, KeyCode::LeftShift)`
   Check exact `KeyCode` enum names in `virasa/window/Events.h`.
2. Each frame, after input is sampled into `inputState`:
   `ActionState frameActions = resolver.Resolve(inputState, bindings);`
   `stepBridge.LatchFrame(frameActions);`
3. In the fixed-step loop, before each `scheduler.Step(...)`:
   `stepBridge.Publish(world, /*firstStep=*/ i == 0);` then `scheduler.Step(world, simStepper.NextStep());`
   (`Publish` binds the per-step `ActionState*` into the World resource store; `VehicleBehavior`
   reads it via `StepBridge::GetActions`. Edge-once-per-frame is handled by `firstStep`.)
   Note: `StepBridge` is non-copyable/non-movable and owns the storage the World borrows — keep
   the single instance alive for the whole loop.

### Task C2 — Render the scene's Main camera in play mode

Today `EditorApp` creates its own orbit camera entity (`cameraEntity`, ~279) with
`cam.domain = CameraDomain::Editor` (~286), drives it from `_cameraYaw/_cameraPitch/_cameraPosition`
(transform written ~738), and always renders it: `sceneRenderer.RenderWorld(world, cameraEntity)`
(~1022). `ChaseCameraBehavior` writes the **scene's Main** camera entity, which is never shown.

Change: while `playing`, pick the runtime world's camera entity whose `CameraComponent.domain ==
Main` (query the Camera system / `World::GetEntities`) and pass THAT to `RenderWorld`; also skip
the orbit-camera transform write (~734-740) while playing so the chase behavior owns the camera.
When not playing, keep the current orbit-camera behavior unchanged. Beware: in play mode `world`
is `runtimeScene->GetWorld()` (~346) — find the Main camera in that world each frame.

### Contract work for Phase C
- Bump `virasa.editor.EditorApp`: add `contract_dependencies` on `virasa.input.Bindings`,
  `virasa.input.Resolver`, `virasa.input.StepBridge` (+ semantic pins). Amend the run-loop
  semantics — `run_loop_drives_one_frame_at_a_time` (currently v14) and/or
  `editor_app_drives_fixed_step_simulation` (v2) — to describe input resolve+latch+publish and
  the play-mode Main-camera selection. Possibly add a new semantic for each rather than
  overloading the giant run-loop one. Then codex implement + write-tests, build, test.
- Watch the ripple: `EditorApp` is a leaf (only tests depend on it), so bumps shouldn't cascade.
- Pre-existing unrelated debt to ignore: SceneRenderer pins `UiPass` 0.2 but current is 0.3
  (graph conflict shows in EditorApp lint; not introduced here).

### Verifying C (it's a GUI app)
Use the `/run` or `/verify` skill to launch `virasa_editor examples/racing` and confirm: scene
loads, pressing `:play` starts sim, WASD drives the box-car, chase camera follows. Headless
tests can cover the input resolve→publish path and Main-camera selection logic, but actual
driving needs the app.

## Phase D and beyond (after C)
- **Mesh colliders** for a real track: add `ColliderShape::Mesh` to `PhysicsComponents` and a
  Jolt `MeshShape` build path in `PhysicsWorld::AddBody` (+ codec/loader for mesh asset key).
  Flat box floor needs none of this.
- Tuning pass on `VehicleComponent` defaults (grip/engine/suspension) once drivable.
- Gameplay: checkpoints, lap timing, reset-to-track (new components + behaviors).
- Optional: dedicated surface-material component if tyre grip needs to diverge from rigid-body
  friction (deferred — currently reuses `RigidBodyComponent.friction`).

## Working conventions (from CLAUDE.md / project memory)
- Contracts via the contracts-cpp MCP only (never read `.contracts/` with FS tools except the
  mechanical Read-before-Edit). Author → `refresh` → `lint_contract` until clean.
- Implement via **codex** (`mcp__codex__codex`, sandbox `danger-full-access`, cwd repo root):
  `write_handoff` then one codex call per role per contract — **implement and write-tests
  separately, never batched**.
- Engine conventions: Z-up, Y-forward, right-handed. Behaviors read input via
  `StepBridge::GetActions(world)` and the shared `PhysicsWorld*` via the resource store.
- See memory `racing-vehicle-foundation.md` for the running state.
