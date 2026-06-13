#include <functional>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "virasa/ecs/ComponentSystem.h"
#include "virasa/ecs/Components.h"
#include "virasa/ecs/Scheduler.h"
#include "virasa/ecs/TransformSystem.h"
#include "virasa/ecs/World.h"
#include "virasa/math/Transform.h"
#include "virasa/sim/Tick.h"

using namespace virasa::ecs;
using namespace virasa::math;

namespace
{

constexpr float kEps = 1e-5f;

HighlightComponent MakeHighlight(int32_t priority = 1)
{
	HighlightComponent highlight;
	highlight.priority = priority;
	return highlight;
}

class RecordingBehavior final : public Behavior
{
	public:
	RecordingBehavior(const char* tag, std::vector<std::string>* log) : _tag(tag), _log(log) {}

	const char* Name() const noexcept override
	{
		return _tag.c_str();
	}

	void Step(World&, const virasa::sim::TickContext&, CommandBuffer&) override
	{
		_log->push_back(_tag);
	}

	private:
	std::string _tag;
	std::vector<std::string>* _log = nullptr;
};

class FunctionBehavior final : public Behavior
{
	public:
	using Fn = std::function<void(World&, const virasa::sim::TickContext&, CommandBuffer&)>;

	FunctionBehavior(const char* name, Fn fn) : _name(name), _fn(std::move(fn)) {}

	const char* Name() const noexcept override
	{
		return _name.c_str();
	}

	void Step(
		World& world, const virasa::sim::TickContext& tick, CommandBuffer& commands) override
	{
		_fn(world, tick, commands);
	}

	private:
	std::string _name;
	Fn _fn;
};

} // namespace

TEST(Phase, test_phase_enum_orders_behavior_execution)
{
	static_assert(static_cast<uint8_t>(Phase::PreStep) == 0u);
	static_assert(static_cast<uint8_t>(Phase::Step) == 1u);
	static_assert(static_cast<uint8_t>(Phase::PostStep) == 2u);

	World world;
	Scheduler scheduler;
	virasa::sim::TickContext tick;
	std::vector<std::string> log;

	scheduler.Register(Phase::PostStep, std::make_unique<RecordingBehavior>("post", &log));
	scheduler.Register(Phase::PreStep, std::make_unique<RecordingBehavior>("pre", &log));
	scheduler.Register(Phase::Step, std::make_unique<RecordingBehavior>("step", &log));

	scheduler.Step(world, tick);

	const std::vector<std::string> expected = {"pre", "step", "post"};
	EXPECT_EQ(log, expected);
}

TEST(Behavior, test_behavior_is_per_tick_system_interface)
{
	static_assert(!std::is_copy_constructible_v<Behavior>);
	static_assert(!std::is_copy_assignable_v<Behavior>);
	static_assert(!std::is_move_constructible_v<Behavior>);
	static_assert(!std::is_move_assignable_v<Behavior>);

	World world;
	Scheduler scheduler;
	virasa::sim::TickContext tick;
	tick.tickIndex = 42u;

	Entity doomed = world.CreateEntity("Doomed");
	ComponentId highlightId = world.GetSystemId("Highlight");
	ASSERT_NE(highlightId, kInvalidComponentId);

	uint64_t observedTick = 0u;
	size_t invokeCount = 0u;
	bool sawEntity = false;

	auto behavior = std::make_unique<FunctionBehavior>("PerTickBehavior",
		[&](World& w, const virasa::sim::TickContext& t, CommandBuffer& commands)
		{
			observedTick = t.tickIndex;
			++invokeCount;
			sawEntity = w.IsValid(doomed) && !w.GetSystem(highlightId).Has(doomed);
			commands.DestroyEntity(doomed);
		});
	EXPECT_EQ(std::string(behavior->Name()), "PerTickBehavior");

	scheduler.Register(Phase::Step, std::move(behavior));
	scheduler.Step(world, tick);

	EXPECT_EQ(observedTick, 42u);
	EXPECT_EQ(invokeCount, 1u);
	EXPECT_TRUE(sawEntity);
	EXPECT_FALSE(world.IsValid(doomed));
}

TEST(CommandBuffer, test_command_buffer_records_deferred_structural_mutations)
{
	World world;
	CommandBuffer cb;
	EXPECT_TRUE(cb.IsEmpty());

	ComponentId highlightId = world.GetSystemId("Highlight");
	ASSERT_NE(highlightId, kInvalidComponentId);

	Entity target = world.CreateEntity("Target");
	Entity highlighted = world.CreateEntity("Highlighted");
	HighlightComponent highlight = MakeHighlight();
	world.GetSystem(highlightId).AddRaw(highlighted, &highlight);

	const uint32_t entityCountBefore = world.GetEntityCount();
	const bool targetHadHighlightBefore = world.GetSystem(highlightId).Has(target);
	const bool highlightedHadHighlightBefore = world.GetSystem(highlightId).Has(highlighted);

	const uint32_t ticket = cb.CreateEntity("Deferred");
	cb.AddComponentToSpawnRaw(ticket, highlightId, &highlight, sizeof(highlight));
	cb.AddComponentRaw(target, highlightId, &highlight, sizeof(highlight));
	cb.RemoveComponent(highlighted, highlightId);
	cb.DestroyEntity(target);

	EXPECT_FALSE(cb.IsEmpty());
	EXPECT_EQ(world.GetEntityCount(), entityCountBefore);
	EXPECT_TRUE(world.IsValid(target));
	EXPECT_EQ(world.GetSystem(highlightId).Has(target), targetHadHighlightBefore);
	EXPECT_EQ(world.GetSystem(highlightId).Has(highlighted), highlightedHadHighlightBefore);

	cb.Clear();

	EXPECT_TRUE(cb.IsEmpty());
	EXPECT_EQ(world.GetEntityCount(), entityCountBefore);
	EXPECT_TRUE(world.IsValid(target));
	EXPECT_EQ(world.GetSystem(highlightId).Has(target), targetHadHighlightBefore);
	EXPECT_EQ(world.GetSystem(highlightId).Has(highlighted), highlightedHadHighlightBefore);
}

TEST(CommandBuffer, test_command_buffer_flush_applies_in_pinned_order_then_clears)
{
	World world;
	CommandBuffer cb;

	ComponentId highlightId = world.GetSystemId("Highlight");
	ASSERT_NE(highlightId, kInvalidComponentId);
	ComponentSystem& highlights = world.GetSystem(highlightId);

	Entity existing = world.CreateEntity("Existing");
	Entity hasHl = world.CreateEntity("HasHighlight");
	Entity victim = world.CreateEntity("Victim");
	Entity keep = world.CreateEntity("Keep");
	(void)keep;

	HighlightComponent highlight = MakeHighlight();
	highlights.AddRaw(hasHl, &highlight);

	cb.DestroyEntity(victim);
	cb.RemoveComponent(hasHl, highlightId);
	const uint32_t ticket = cb.CreateEntity("Spawned");
	cb.AddComponentRaw(existing, highlightId, &highlight, sizeof(highlight));
	cb.AddComponentToSpawnRaw(ticket, highlightId, &highlight, sizeof(highlight));

	cb.Flush(world);

	Entity spawned = world.FindEntityByName("Spawned");
	EXPECT_TRUE(spawned.IsValid());
	EXPECT_TRUE(world.IsValid(spawned));
	EXPECT_TRUE(highlights.Has(spawned));
	EXPECT_TRUE(highlights.Has(existing));
	EXPECT_FALSE(highlights.Has(hasHl));
	EXPECT_FALSE(world.IsValid(victim));
	EXPECT_TRUE(cb.IsEmpty());
}

TEST(Scheduler, test_scheduler_owns_phase_ordered_behaviors)
{
	World world;
	Scheduler scheduler;
	virasa::sim::TickContext tick;
	std::vector<std::string> log;

	EXPECT_EQ(scheduler.BehaviorCount(Phase::PreStep), 0u);
	EXPECT_EQ(scheduler.BehaviorCount(Phase::Step), 0u);
	EXPECT_EQ(scheduler.BehaviorCount(Phase::PostStep), 0u);

	scheduler.Register(Phase::Step, std::make_unique<RecordingBehavior>("first", &log));
	scheduler.Register(Phase::Step, std::make_unique<RecordingBehavior>("second", &log));

	EXPECT_EQ(scheduler.BehaviorCount(Phase::PreStep), 0u);
	EXPECT_EQ(scheduler.BehaviorCount(Phase::Step), 2u);
	EXPECT_EQ(scheduler.BehaviorCount(Phase::PostStep), 0u);
	EXPECT_TRUE(log.empty());

	scheduler.Step(world, tick);

	const std::vector<std::string> expected = {"first", "second"};
	EXPECT_EQ(log, expected);
}

TEST(Scheduler, test_scheduler_step_runs_enabled_phases_then_resolves_transforms)
{
	World world;
	Scheduler scheduler;
	virasa::sim::TickContext tick;
	std::vector<std::string> log;

	EXPECT_TRUE(scheduler.IsEnabled());

	Entity e = world.CreateEntity("Mover");
	Transform initial;
	world.Transforms().Add(e, initial);
	world.UpdateTransforms();

	Transform dirty;
	dirty.translation = Vec3(1.0f, 2.0f, 3.0f);
	world.Transforms().SetLocal(e, dirty);

	scheduler.Register(Phase::Step, std::make_unique<RecordingBehavior>("disabled", &log));
	scheduler.SetEnabled(false);
	scheduler.Step(world, tick);

	EXPECT_TRUE(log.empty());
	Vec3 stalePos = world.Transforms().GetWorldPosition(e);
	EXPECT_NEAR(stalePos.x, 0.0f, kEps);
	EXPECT_NEAR(stalePos.y, 0.0f, kEps);
	EXPECT_NEAR(stalePos.z, 0.0f, kEps);

	scheduler.SetEnabled(true);
	scheduler.Register(Phase::Step,
		std::make_unique<FunctionBehavior>("move",
			[&](World& w, const virasa::sim::TickContext&, CommandBuffer&)
			{
				log.push_back("move");
				Transform moved;
				moved.translation = Vec3(4.0f, 5.0f, 6.0f);
				w.Transforms().SetLocal(e, moved);
			}));

	scheduler.Step(world, tick);

	EXPECT_EQ(log.size(), 2u);
	EXPECT_EQ(log[0], "disabled");
	EXPECT_EQ(log[1], "move");
	Vec3 updatedPos = world.Transforms().GetWorldPosition(e);
	EXPECT_NEAR(updatedPos.x, 4.0f, kEps);
	EXPECT_NEAR(updatedPos.y, 5.0f, kEps);
	EXPECT_NEAR(updatedPos.z, 6.0f, kEps);

	log.clear();
	ComponentId highlightId = world.GetSystemId("Highlight");
	ASSERT_NE(highlightId, kInvalidComponentId);
	HighlightComponent highlight = MakeHighlight();
	bool stepSawPreStepSpawn = false;

	scheduler.Register(Phase::PreStep,
		std::make_unique<FunctionBehavior>("spawn highlighted",
			[&](World&, const virasa::sim::TickContext&, CommandBuffer& commands)
			{
				const uint32_t ticket = commands.CreateEntity("PreStepSpawn");
				commands.AddComponentToSpawnRaw(
					ticket, highlightId, &highlight, sizeof(highlight));
			}));
	scheduler.Register(Phase::Step,
		std::make_unique<FunctionBehavior>("observe highlighted",
			[&](World& w, const virasa::sim::TickContext&, CommandBuffer&)
			{
				Entity spawned = w.FindEntityByName("PreStepSpawn");
				stepSawPreStepSpawn =
					w.IsValid(spawned) && w.GetSystem(highlightId).Has(spawned);
			}));

	scheduler.Step(world, tick);

	EXPECT_TRUE(stepSawPreStepSpawn);
}
