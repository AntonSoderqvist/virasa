#ifndef VIRASA_SIM_SCENE_H
#define VIRASA_SIM_SCENE_H

#include "virasa/ecs/Scheduler.h"
#include "virasa/ecs/Types.h"
#include "virasa/ecs/World.h"
#include "virasa/sim/BehaviorRegistry.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace virasa::sim
{

class Scene final
{
public:
	Scene() = default;
	~Scene() = default;

	Scene(const Scene&) = delete;
	Scene& operator=(const Scene&) = delete;

	Scene(Scene&&) noexcept = default;
	Scene& operator=(Scene&&) noexcept = default;

	[[nodiscard]] virasa::ecs::World& GetWorld() noexcept;
	[[nodiscard]] const virasa::ecs::World& GetWorld() const noexcept;

	void SetName(std::string_view name);
	[[nodiscard]] std::string_view GetName() const noexcept;

	void AddBehavior(std::string_view name);
	[[nodiscard]] bool HasBehavior(std::string_view name) const;
	[[nodiscard]] const std::vector<std::string>& Behaviors() const noexcept;

	void SetDefaultCamera(virasa::ecs::Entity camera);
	[[nodiscard]] virasa::ecs::Entity GetDefaultCamera() const noexcept;

	[[nodiscard]] virasa::sim::Scene Instantiate() const;
	bool BuildScheduler(
		const virasa::sim::BehaviorRegistry& registry,
		virasa::ecs::Scheduler& scheduler) const;

private:
	virasa::ecs::World _world;
	std::vector<std::string> _behaviors;
	std::string _name;
	virasa::ecs::Entity _defaultCamera = virasa::ecs::Entity::Invalid();
};

} // namespace virasa::sim

#endif // VIRASA_SIM_SCENE_H
