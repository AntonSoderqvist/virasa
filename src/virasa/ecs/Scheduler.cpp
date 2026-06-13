#include "virasa/ecs/Scheduler.h"

#include <cstring>
#include <utility>

#include "virasa/ecs/ComponentSystem.h"
#include "virasa/ecs/World.h"

namespace virasa::ecs
{

namespace
{

[[nodiscard]] std::vector<uint8_t> CopyBytes(const void* value, size_t byteSize)
{
	std::vector<uint8_t> bytes(byteSize);
	if (byteSize > 0u)
	{
		std::memcpy(bytes.data(), value, byteSize);
	}
	return bytes;
}

} // namespace

uint32_t CommandBuffer::CreateEntity(std::string_view name)
{
	const uint32_t ticket = static_cast<uint32_t>(_creations.size());
	_creations.push_back(PendingCreation{std::string(name)});
	return ticket;
}

void CommandBuffer::AddComponentToSpawnRaw(uint32_t spawnTicket,
	virasa::ecs::ComponentId componentId, const void* value, size_t byteSize)
{
	ComponentAdd add;
	add.targetsSpawn = true;
	add.spawnTicket = spawnTicket;
	add.componentId = componentId;
	add.bytes = CopyBytes(value, byteSize);
	_componentAdds.push_back(std::move(add));
}

void CommandBuffer::DestroyEntity(virasa::ecs::Entity entity)
{
	_destructions.push_back(entity);
}

void CommandBuffer::AddComponentRaw(virasa::ecs::Entity entity,
	virasa::ecs::ComponentId componentId, const void* value, size_t byteSize)
{
	ComponentAdd add;
	add.targetsSpawn = false;
	add.entity = entity;
	add.componentId = componentId;
	add.bytes = CopyBytes(value, byteSize);
	_componentAdds.push_back(std::move(add));
}

void CommandBuffer::RemoveComponent(
	virasa::ecs::Entity entity, virasa::ecs::ComponentId componentId)
{
	_componentRemovals.push_back(ComponentRemoval{entity, componentId});
}

void CommandBuffer::Flush(virasa::ecs::World& world)
{
	std::vector<virasa::ecs::Entity> ticketEntities;
	ticketEntities.reserve(_creations.size());

	for (const PendingCreation& creation : _creations)
	{
		ticketEntities.push_back(world.CreateEntity(creation.name));
	}

	for (const ComponentAdd& add : _componentAdds)
	{
		const virasa::ecs::Entity entity =
			add.targetsSpawn ? ticketEntities[add.spawnTicket] : add.entity;
		world.GetSystem(add.componentId).AddRaw(entity, add.bytes.data());
	}

	for (const ComponentRemoval& removal : _componentRemovals)
	{
		virasa::ecs::ComponentSystem& system = world.GetSystem(removal.componentId);
		if (system.Has(removal.entity))
		{
			system.Remove(removal.entity);
		}
	}

	for (virasa::ecs::Entity entity : _destructions)
	{
		if (world.IsValid(entity))
		{
			world.DestroyEntity(entity);
		}
	}

	Clear();
}

void CommandBuffer::Clear()
{
	_creations.clear();
	_componentAdds.clear();
	_componentRemovals.clear();
	_destructions.clear();
}

bool CommandBuffer::IsEmpty() const noexcept
{
	return _creations.empty() && _componentAdds.empty() && _componentRemovals.empty() &&
		 _destructions.empty();
}

void Scheduler::Register(virasa::ecs::Phase phase, std::unique_ptr<virasa::ecs::Behavior> behavior)
{
	_behaviors[PhaseIndex(phase)].push_back(std::move(behavior));
}

void Scheduler::Step(virasa::ecs::World& world, const virasa::sim::TickContext& tick)
{
	if (!_enabled)
	{
		return;
	}

	for (size_t phaseIndex = 0u; phaseIndex < kPhaseCount; ++phaseIndex)
	{
		for (const std::unique_ptr<virasa::ecs::Behavior>& behavior : _behaviors[phaseIndex])
		{
			behavior->Step(world, tick, _commands);
		}

		_commands.Flush(world);
	}

	world.UpdateTransforms();
}

void Scheduler::SetEnabled(bool enabled)
{
	_enabled = enabled;
}

bool Scheduler::IsEnabled() const noexcept
{
	return _enabled;
}

size_t Scheduler::BehaviorCount(virasa::ecs::Phase phase) const noexcept
{
	return _behaviors[PhaseIndex(phase)].size();
}

size_t Scheduler::PhaseIndex(virasa::ecs::Phase phase) noexcept
{
	return static_cast<size_t>(phase);
}

} // namespace virasa::ecs
