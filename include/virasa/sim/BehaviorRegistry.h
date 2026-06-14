#ifndef VIRASA_SIM_BEHAVIORREGISTRY_H
#define VIRASA_SIM_BEHAVIORREGISTRY_H

#include "virasa/ecs/Scheduler.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace virasa::sim
{

using BehaviorFactory = std::function<std::unique_ptr<virasa::ecs::Behavior>()>;

class BehaviorRegistry final
{
public:
	BehaviorRegistry() = default;
	~BehaviorRegistry() = default;

	BehaviorRegistry(const BehaviorRegistry&) = delete;
	BehaviorRegistry& operator=(const BehaviorRegistry&) = delete;

	BehaviorRegistry(BehaviorRegistry&&) noexcept = default;
	BehaviorRegistry& operator=(BehaviorRegistry&&) noexcept = default;

	void Register(std::string_view name, virasa::ecs::Phase phase, virasa::sim::BehaviorFactory factory);
	[[nodiscard]] bool Contains(std::string_view name) const;
	[[nodiscard]] size_t Size() const noexcept;
	[[nodiscard]] std::vector<std::string> Names() const;
	bool Build(std::string_view name, virasa::ecs::Scheduler& scheduler);

private:
	struct Entry
	{
		std::string name;
		virasa::ecs::Phase phase = virasa::ecs::Phase::Step;
		virasa::sim::BehaviorFactory factory;
	};

	std::vector<Entry> _entries;
};

} // namespace virasa::sim

#endif // VIRASA_SIM_BEHAVIORREGISTRY_H
