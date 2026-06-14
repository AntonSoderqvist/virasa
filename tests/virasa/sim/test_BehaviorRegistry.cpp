#include <gtest/gtest.h>

#include "virasa/ecs/Scheduler.h"
#include "virasa/ecs/World.h"
#include "virasa/sim/BehaviorRegistry.h"
#include "virasa/sim/Tick.h"

#include <memory>
#include <string>
#include <type_traits>
#include <vector>

using namespace virasa::ecs;
using namespace virasa::sim;

namespace
{

class TestBehavior final : public Behavior
{
public:
	TestBehavior(const char* name, std::vector<std::string>* log)
		: _name(name)
		, _log(log)
	{
	}

	const char* Name() const noexcept override
	{
		return _name.c_str();
	}

	void Step(World&, const TickContext&, CommandBuffer&) override
	{
		if (_log != nullptr)
		{
			_log->push_back(_name);
		}
	}

private:
	std::string _name;
	std::vector<std::string>* _log = nullptr;
};

} // namespace

TEST(BehaviorRegistry, test_behavior_registry_maps_names_to_phase_tagged_factories)
{
	static_assert(!std::is_copy_constructible_v<BehaviorRegistry>);
	static_assert(!std::is_copy_assignable_v<BehaviorRegistry>);
	static_assert(std::is_move_constructible_v<BehaviorRegistry>);
	static_assert(std::is_move_assignable_v<BehaviorRegistry>);
	static_assert(std::is_default_constructible_v<BehaviorRegistry>);

	BehaviorRegistry registry;
	EXPECT_EQ(registry.Size(), 0u);
	EXPECT_FALSE(registry.Contains("Spin"));

	registry.Register("Spin", Phase::PreStep, []()
	{
		return std::make_unique<TestBehavior>("Spin", nullptr);
	});
	registry.Register("Move", Phase::PostStep, []()
	{
		return std::make_unique<TestBehavior>("Move", nullptr);
	});

	Scheduler scheduler;
	EXPECT_TRUE(registry.Build("Spin", scheduler));
	EXPECT_TRUE(registry.Build("Move", scheduler));
	EXPECT_EQ(scheduler.BehaviorCount(Phase::PreStep), 1u);
	EXPECT_EQ(scheduler.BehaviorCount(Phase::Step), 0u);
	EXPECT_EQ(scheduler.BehaviorCount(Phase::PostStep), 1u);
}

TEST(BehaviorRegistry, test_behavior_registry_register_binds_name_phase_and_factory)
{
	BehaviorRegistry registry;
	int constructCount = 0;

	registry.Register("Counter", Phase::Step, [&]()
	{
		++constructCount;
		return std::make_unique<TestBehavior>("Counter", nullptr);
	});

	EXPECT_TRUE(registry.Contains("Counter"));
	EXPECT_FALSE(registry.Contains("counter"));
	EXPECT_EQ(registry.Size(), 1u);
	EXPECT_EQ(constructCount, 0);

	Scheduler scheduler;
	EXPECT_TRUE(registry.Build("Counter", scheduler));
	EXPECT_EQ(constructCount, 1);
	EXPECT_EQ(scheduler.BehaviorCount(Phase::Step), 1u);
}

TEST(BehaviorRegistry, test_behavior_registry_observers_report_registered_names)
{
	BehaviorRegistry registry;
	registry.Register("First", Phase::Step, []()
	{
		return std::make_unique<TestBehavior>("First", nullptr);
	});
	registry.Register("Second", Phase::Step, []()
	{
		return std::make_unique<TestBehavior>("Second", nullptr);
	});
	registry.Register("Third", Phase::Step, []()
	{
		return std::make_unique<TestBehavior>("Third", nullptr);
	});

	EXPECT_TRUE(registry.Contains("First"));
	EXPECT_TRUE(registry.Contains("Second"));
	EXPECT_FALSE(registry.Contains("second"));
	EXPECT_EQ(registry.Size(), 3u);

	const std::vector<std::string> expected = {"First", "Second", "Third"};
	EXPECT_EQ(registry.Names(), expected);
}

TEST(BehaviorRegistry, test_behavior_registry_build_instantiates_into_scheduler)
{
	BehaviorRegistry registry;
	std::vector<std::string> log;
	int constructCount = 0;

	registry.Register("Runtime", Phase::Step, [&]()
	{
		++constructCount;
		return std::make_unique<TestBehavior>("Runtime", &log);
	});

	Scheduler first;
	Scheduler second;
	EXPECT_FALSE(registry.Build("Missing", first));
	EXPECT_EQ(first.BehaviorCount(Phase::Step), 0u);

	EXPECT_TRUE(registry.Build("Runtime", first));
	EXPECT_TRUE(registry.Build("Runtime", second));
	EXPECT_EQ(constructCount, 2);
	EXPECT_EQ(first.BehaviorCount(Phase::Step), 1u);
	EXPECT_EQ(second.BehaviorCount(Phase::Step), 1u);

	World world;
	TickContext tick;
	first.Step(world, tick);
	second.Step(world, tick);

	const std::vector<std::string> expected = {"Runtime", "Runtime"};
	EXPECT_EQ(log, expected);
}
