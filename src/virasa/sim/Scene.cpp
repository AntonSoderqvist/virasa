#include "virasa/sim/Scene.h"

namespace virasa::sim
{

virasa::ecs::World& Scene::GetWorld() noexcept
{
	return _world;
}

const virasa::ecs::World& Scene::GetWorld() const noexcept
{
	return _world;
}

void Scene::SetName(std::string_view name)
{
	_name = name;
}

std::string_view Scene::GetName() const noexcept
{
	return _name;
}

void Scene::AddBehavior(std::string_view name)
{
	_behaviors.emplace_back(name);
}

bool Scene::HasBehavior(std::string_view name) const
{
	for (const std::string& behavior : _behaviors)
	{
		if (behavior == name)
		{
			return true;
		}
	}

	return false;
}

const std::vector<std::string>& Scene::Behaviors() const noexcept
{
	return _behaviors;
}

void Scene::SetDefaultCamera(virasa::ecs::Entity camera)
{
	_defaultCamera = camera;
}

virasa::ecs::Entity Scene::GetDefaultCamera() const noexcept
{
	return _defaultCamera;
}

virasa::sim::Scene Scene::Instantiate() const
{
	virasa::sim::Scene scene;
	scene._world = _world.Clone();
	scene._behaviors = _behaviors;
	scene._name = _name;
	scene._defaultCamera = _defaultCamera;
	return scene;
}

bool Scene::BuildScheduler(
	const virasa::sim::BehaviorRegistry& registry,
	virasa::ecs::Scheduler& scheduler) const
{
	bool builtAll = true;
	auto& mutableRegistry = const_cast<virasa::sim::BehaviorRegistry&>(registry);
	for (const std::string& behavior : _behaviors)
	{
		if (!mutableRegistry.Build(behavior, scheduler))
		{
			builtAll = false;
		}
	}

	return builtAll;
}

} // namespace virasa::sim
