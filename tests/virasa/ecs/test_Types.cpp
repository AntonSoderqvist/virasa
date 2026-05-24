#include <cstdint>
#include <gtest/gtest.h>

#include "virasa/ecs/Types.h"

using namespace virasa::ecs;

TEST(Types, test_ecs_error_enum_values_in_declared_order)
{
	// EcsError underlying type is uint8_t
	EcsError none = EcsError::None;
	EcsError invalidEntity = EcsError::InvalidEntity;
	EcsError cycleDetected = EcsError::CycleDetected;
	EcsError selfParent = EcsError::SelfParent;

	// None is explicitly 0
	EXPECT_EQ(static_cast<uint8_t>(none), uint8_t{0});

	// Values appear in declaration order (each one greater than the previous)
	EXPECT_LT(static_cast<uint8_t>(none), static_cast<uint8_t>(invalidEntity));
	EXPECT_LT(static_cast<uint8_t>(invalidEntity), static_cast<uint8_t>(cycleDetected));
	EXPECT_LT(static_cast<uint8_t>(cycleDetected), static_cast<uint8_t>(selfParent));

	// All four values are distinct
	EXPECT_NE(static_cast<uint8_t>(none), static_cast<uint8_t>(invalidEntity));
	EXPECT_NE(static_cast<uint8_t>(none), static_cast<uint8_t>(cycleDetected));
	EXPECT_NE(static_cast<uint8_t>(none), static_cast<uint8_t>(selfParent));
	EXPECT_NE(static_cast<uint8_t>(invalidEntity), static_cast<uint8_t>(cycleDetected));
	EXPECT_NE(static_cast<uint8_t>(invalidEntity), static_cast<uint8_t>(selfParent));
	EXPECT_NE(static_cast<uint8_t>(cycleDetected), static_cast<uint8_t>(selfParent));

	// Verify the underlying type width (uint8_t)
	EXPECT_EQ(sizeof(EcsError), sizeof(uint8_t));
}

TEST(Types, test_ecs_error_none_is_unique_success_value)
{
	// None compares equal only to itself
	EXPECT_EQ(EcsError::None, EcsError::None);
	EXPECT_NE(EcsError::None, EcsError::InvalidEntity);
	EXPECT_NE(EcsError::None, EcsError::CycleDetected);
	EXPECT_NE(EcsError::None, EcsError::SelfParent);

	// None is the zero value; all other values are non-zero (truthy when cast)
	EXPECT_EQ(static_cast<uint8_t>(EcsError::None), uint8_t{0});
	EXPECT_NE(static_cast<uint8_t>(EcsError::InvalidEntity), uint8_t{0});
	EXPECT_NE(static_cast<uint8_t>(EcsError::CycleDetected), uint8_t{0});
	EXPECT_NE(static_cast<uint8_t>(EcsError::SelfParent), uint8_t{0});
}

TEST(Types, test_entity_handle_is_generational_index)
{
	// Default-constructed Entity has index == 0xFFFFFFFFu and generation == 0
	Entity defaultEntity;
	EXPECT_EQ(defaultEntity.index, 0xFFFFFFFFu);
	EXPECT_EQ(defaultEntity.generation, 0u);

	// Entity is a 64-bit struct (two uint32_t fields)
	EXPECT_EQ(sizeof(Entity), sizeof(uint32_t) * 2);

	// Fields are individually accessible and assignable
	Entity e;
	e.index = 7u;
	e.generation = 3u;
	EXPECT_EQ(e.index, 7u);
	EXPECT_EQ(e.generation, 3u);

	// Default-constructed Entity compares equal to Entity::Invalid()
	EXPECT_EQ(defaultEntity, Entity::Invalid());

	// IsValid() returns false for a default-constructed Entity
	EXPECT_FALSE(defaultEntity.IsValid());

	// Entity is copyable
	Entity copy = e;
	EXPECT_EQ(copy.index, e.index);
	EXPECT_EQ(copy.generation, e.generation);

	// Entity is movable
	Entity moved = std::move(copy);
	EXPECT_EQ(moved.index, 7u);
	EXPECT_EQ(moved.generation, 3u);
}

TEST(Types, test_entity_invalid_is_unique_sentinel)
{
	Entity invalid = Entity::Invalid();

	// Invalid() returns index == 0xFFFFFFFFu and generation == 0
	EXPECT_EQ(invalid.index, 0xFFFFFFFFu);
	EXPECT_EQ(invalid.generation, 0u);

	// Invalid() compares equal to a default-constructed Entity
	Entity defaultEntity;
	EXPECT_EQ(invalid, defaultEntity);

	// Calling Invalid() multiple times returns equal values
	EXPECT_EQ(Entity::Invalid(), Entity::Invalid());

	// IsValid() returns false for the invalid sentinel
	EXPECT_FALSE(invalid.IsValid());

	// A non-sentinel entity is not equal to Invalid()
	Entity live;
	live.index = 0u;
	live.generation = 1u;
	EXPECT_NE(live, Entity::Invalid());
}

TEST(Types, test_entity_equality_compares_index_and_generation)
{
	// Two entities with same index and generation are equal
	Entity a;
	a.index = 5u;
	a.generation = 2u;

	Entity b;
	b.index = 5u;
	b.generation = 2u;

	EXPECT_TRUE(a == b);
	EXPECT_FALSE(a != b);

	// Different generation (same index) => not equal (stale handle detection)
	Entity stale;
	stale.index = 5u;
	stale.generation = 1u;

	EXPECT_FALSE(a == stale);
	EXPECT_TRUE(a != stale);

	// Different index (same generation) => not equal
	Entity differentIndex;
	differentIndex.index = 6u;
	differentIndex.generation = 2u;

	EXPECT_FALSE(a == differentIndex);
	EXPECT_TRUE(a != differentIndex);

	// IsValid() returns true when index != 0xFFFFFFFFu
	EXPECT_TRUE(a.IsValid());

	// IsValid() returns false when index == 0xFFFFFFFFu (regardless of generation)
	Entity sentinelIndex;
	sentinelIndex.index = 0xFFFFFFFFu;
	sentinelIndex.generation = 99u;
	EXPECT_FALSE(sentinelIndex.IsValid());

	// IsValid() returns true for index == 0 (a valid slot index)
	Entity zeroIndex;
	zeroIndex.index = 0u;
	zeroIndex.generation = 0u;
	EXPECT_TRUE(zeroIndex.IsValid());
}
