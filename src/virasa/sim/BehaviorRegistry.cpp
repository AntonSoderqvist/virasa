#include "virasa/sim/BehaviorRegistry.h"

#include <utility>

namespace virasa::sim
{

void BehaviorRegistry::Register(
	std::string_view name,
	virasa::ecs::Phase phase,
	virasa::sim::BehaviorFactory factory)
{
	_entries.push_back(Entry{
		.name = std::string(name),
		.phase = phase,
		.factory = std::move(factory),
	});
}

bool BehaviorRegistry::Contains(std::string_view name) const
{
	for (const Entry& entry : _entries)
	{
		if (entry.name == name)
		{
			return true;
		}
	}

	return false;
}

size_t BehaviorRegistry::Size() const noexcept
{
	return _entries.size();
}

std::vector<std::string> BehaviorRegistry::Names() const
{
	std::vector<std::string> names;
	names.reserve(_entries.size());
	for (const Entry& entry : _entries)
	{
		names.push_back(entry.name);
	}

	return names;
}

bool BehaviorRegistry::Build(std::string_view name, virasa::ecs::Scheduler& scheduler)
{
	for (Entry& entry : _entries)
	{
		if (entry.name == name)
		{
			scheduler.Register(entry.phase, entry.factory());
			return true;
		}
	}

	return false;
}

} // namespace virasa::sim
