#include <gtest/gtest.h>

#include "virasa/ecs/TransformSystem.h"
#include "virasa/ecs/ComponentSystem.h"
#include "virasa/ecs/Types.h"
#include "virasa/math/Transform.h"
#include "virasa/math/Types.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cmath>
#include <cstdint>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace
{

constexpr virasa::ecs::ComponentId kTestComponentId = 1u;

// Build a simple entity with a given index and generation = 1.
virasa::ecs::Entity MakeEntity(uint32_t index)
{
	virasa::ecs::Entity e;
	e.index = index;
	e.generation = 1u;
	return e;
}

// Build a non-identity Transform for testing.
virasa::math::Transform MakeTestTransform(float tx, float ty, float tz)
{
	virasa::math::Transform t;
	t.translation = virasa::math::Vec3(tx, ty, tz);
	t.rotation    = virasa::math::Quat(1.0f, 0.0f, 0.0f, 0.0f); // identity rotation
	t.scale       = virasa::math::Vec3(1.0f, 1.0f, 1.0f);
	return t;
}

// Tolerance for floating-point comparisons.
constexpr float kEps = 1e-5f;

} // namespace

// ---------------------------------------------------------------------------
// transform_system_stores_local_transform_and_caches_world_matrix
// ---------------------------------------------------------------------------
TEST(TransformSystem, test_transform_system_stores_local_transform_and_caches_world_matrix)
{
	// TransformSystem must be constructible with a ComponentId.
	virasa::ecs::TransformSystem sys(kTestComponentId);

	// Id() and Name() must be wired up correctly.
	EXPECT_EQ(sys.Id(), kTestComponentId);
	EXPECT_STREQ(sys.Name(), "Transform");

	// Verify it is not copyable / movable by checking it compiles as a
	// concrete object and that the base interface is satisfied.
	// (Static assertions are in the implementation; here we just verify
	// the system is usable through the ComponentSystem pointer.)
	virasa::ecs::ComponentSystem* base = &sys;
	EXPECT_EQ(base->Id(), kTestComponentId);
	EXPECT_STREQ(base->Name(), "Transform");

	// Add an entity and verify the system stores the local transform.
	virasa::ecs::Entity e0 = MakeEntity(0);
	virasa::math::Transform local = MakeTestTransform(1.0f, 2.0f, 3.0f);
	sys.Add(e0, local);

	EXPECT_EQ(sys.Size(), 1u);
	EXPECT_TRUE(sys.Has(e0));

	// The local transform round-trips.
	const virasa::math::Transform& stored = sys.GetLocal(e0);
	EXPECT_NEAR(stored.translation.x, 1.0f, kEps);
	EXPECT_NEAR(stored.translation.y, 2.0f, kEps);
	EXPECT_NEAR(stored.translation.z, 3.0f, kEps);

	// The world cache slot exists (OnAdd was called); default world is identity.
	const virasa::math::Mat4& world = sys.GetWorld(e0);
	// Identity matrix diagonal is 1.
	EXPECT_NEAR(world[0][0], 1.0f, kEps);
	EXPECT_NEAR(world[1][1], 1.0f, kEps);
	EXPECT_NEAR(world[2][2], 1.0f, kEps);
	EXPECT_NEAR(world[3][3], 1.0f, kEps);
}

// ---------------------------------------------------------------------------
// transform_system_local_accessors_are_write_through
// ---------------------------------------------------------------------------
TEST(TransformSystem, test_transform_system_local_accessors_are_write_through)
{
	virasa::ecs::TransformSystem sys(kTestComponentId);

	virasa::ecs::Entity e0 = MakeEntity(0);
	virasa::math::Transform initial = MakeTestTransform(0.0f, 0.0f, 0.0f);
	sys.Add(e0, initial);

	// GetLocal returns the value that was added.
	{
		const virasa::math::Transform& got = sys.GetLocal(e0);
		EXPECT_NEAR(got.translation.x, 0.0f, kEps);
		EXPECT_NEAR(got.translation.y, 0.0f, kEps);
		EXPECT_NEAR(got.translation.z, 0.0f, kEps);
	}

	// SetLocal overwrites the stored value immediately (write-through).
	virasa::math::Transform updated = MakeTestTransform(10.0f, 20.0f, 30.0f);
	sys.SetLocal(e0, updated);

	{
		const virasa::math::Transform& got = sys.GetLocal(e0);
		EXPECT_NEAR(got.translation.x, 10.0f, kEps);
		EXPECT_NEAR(got.translation.y, 20.0f, kEps);
		EXPECT_NEAR(got.translation.z, 30.0f, kEps);
	}

	// SetLocal marks the entity dirty.
	// Clear dirty first (Add also marks dirty), then set and verify dirty again.
	sys.ClearAllDirty();
	EXPECT_TRUE(sys.Dirty().empty());

	virasa::math::Transform updated2 = MakeTestTransform(5.0f, 5.0f, 5.0f);
	sys.SetLocal(e0, updated2);

	bool foundDirty = false;
	for (const auto& d : sys.Dirty())
	{
		if (d == e0)
		{
			foundDirty = true;
			break;
		}
	}
	EXPECT_TRUE(foundDirty) << "SetLocal should mark the entity dirty";

	// Add requires the entity not already have a component — verify Has() is true
	// (we do not call Add again to avoid UB, but we confirm the precondition).
	EXPECT_TRUE(sys.Has(e0));
}

// ---------------------------------------------------------------------------
// transform_system_world_accessors_and_extractors
// ---------------------------------------------------------------------------
TEST(TransformSystem, test_transform_system_world_accessors_and_extractors)
{
	virasa::ecs::TransformSystem sys(kTestComponentId);

	virasa::ecs::Entity e0 = MakeEntity(0);
	virasa::math::Transform local = MakeTestTransform(0.0f, 0.0f, 0.0f);
	sys.Add(e0, local);

	// Build a world matrix: translation (3, 7, 11), identity rotation, unit scale.
	virasa::math::Mat4 worldMat = glm::mat4(1.0f);
	worldMat[3][0] = 3.0f;
	worldMat[3][1] = 7.0f;
	worldMat[3][2] = 11.0f;

	sys.SetWorld(e0, worldMat);

	// GetWorld returns the matrix we wrote.
	{
		const virasa::math::Mat4& got = sys.GetWorld(e0);
		EXPECT_NEAR(got[3][0], 3.0f, kEps);
		EXPECT_NEAR(got[3][1], 7.0f, kEps);
		EXPECT_NEAR(got[3][2], 11.0f, kEps);
	}

	// GetWorldPosition returns the translation column (column 3, xyz).
	{
		virasa::math::Vec3 pos = sys.GetWorldPosition(e0);
		EXPECT_NEAR(pos.x, 3.0f, kEps);
		EXPECT_NEAR(pos.y, 7.0f, kEps);
		EXPECT_NEAR(pos.z, 11.0f, kEps);
	}

	// GetWorldForward: with identity rotation upper-left, forward is +Y in world space.
	// Our worldMat has identity upper-left 3x3, so forward = (0,1,0).
	{
		virasa::math::Vec3 fwd = sys.GetWorldForward(e0);
		EXPECT_NEAR(fwd.x, 0.0f, kEps);
		EXPECT_NEAR(fwd.y, 1.0f, kEps);
		EXPECT_NEAR(fwd.z, 0.0f, kEps);
	}

	// SetWorld does NOT mark the entity dirty (it is the resolution of dirtiness).
	sys.ClearAllDirty();
	virasa::math::Mat4 worldMat2 = glm::mat4(1.0f);
	sys.SetWorld(e0, worldMat2);
	EXPECT_TRUE(sys.Dirty().empty()) << "SetWorld must not mark the entity dirty";
}

// ---------------------------------------------------------------------------
// transform_system_keeps_world_cache_aligned_via_hooks
// ---------------------------------------------------------------------------
TEST(TransformSystem, test_transform_system_keeps_world_cache_aligned_via_hooks)
{
	virasa::ecs::TransformSystem sys(kTestComponentId);

	virasa::ecs::Entity e0 = MakeEntity(0);
	virasa::ecs::Entity e1 = MakeEntity(1);
	virasa::ecs::Entity e2 = MakeEntity(2);

	virasa::math::Transform t0 = MakeTestTransform(1.0f, 0.0f, 0.0f);
	virasa::math::Transform t1 = MakeTestTransform(2.0f, 0.0f, 0.0f);
	virasa::math::Transform t2 = MakeTestTransform(3.0f, 0.0f, 0.0f);

	sys.Add(e0, t0);
	sys.Add(e1, t1);
	sys.Add(e2, t2);

	// After three Adds, Size() == 3 and world cache has 3 slots (all identity).
	EXPECT_EQ(sys.Size(), 3u);

	// Write distinct world matrices so we can track which entity's world survived.
	virasa::math::Mat4 w0 = glm::mat4(1.0f); w0[3][0] = 100.0f;
	virasa::math::Mat4 w1 = glm::mat4(1.0f); w1[3][0] = 200.0f;
	virasa::math::Mat4 w2 = glm::mat4(1.0f); w2[3][0] = 300.0f;
	sys.SetWorld(e0, w0);
	sys.SetWorld(e1, w1);
	sys.SetWorld(e2, w2);

	// Remove e0 (dense index 0). e2 (last) is swapped into slot 0.
	sys.Remove(e0);
	EXPECT_EQ(sys.Size(), 2u);
	EXPECT_FALSE(sys.Has(e0));
	EXPECT_TRUE(sys.Has(e1));
	EXPECT_TRUE(sys.Has(e2));

	// e2's world matrix must still be accessible and correct after the swap.
	{
		const virasa::math::Mat4& got = sys.GetWorld(e2);
		EXPECT_NEAR(got[3][0], 300.0f, kEps)
			<< "e2's world matrix must survive the swap-and-pop of e0";
	}

	// e1's world matrix must be unchanged.
	{
		const virasa::math::Mat4& got = sys.GetWorld(e1);
		EXPECT_NEAR(got[3][0], 200.0f, kEps)
			<< "e1's world matrix must be unaffected by removing e0";
	}

	// Remove e2 (now in slot 0). e1 (last) is swapped into slot 0.
	sys.Remove(e2);
	EXPECT_EQ(sys.Size(), 1u);
	EXPECT_FALSE(sys.Has(e2));
	EXPECT_TRUE(sys.Has(e1));

	// e1's world matrix must still be correct.
	{
		const virasa::math::Mat4& got = sys.GetWorld(e1);
		EXPECT_NEAR(got[3][0], 200.0f, kEps)
			<< "e1's world matrix must survive the second swap-and-pop";
	}

	// Remove the last entity; size drops to 0.
	sys.Remove(e1);
	EXPECT_EQ(sys.Size(), 0u);
}

// ---------------------------------------------------------------------------
// transform_system_does_not_own_hierarchy_propagation
// ---------------------------------------------------------------------------
TEST(TransformSystem, test_transform_system_does_not_own_hierarchy_propagation)
{
	virasa::ecs::TransformSystem sys(kTestComponentId);

	virasa::ecs::Entity e0 = MakeEntity(0);
	virasa::math::Transform local = MakeTestTransform(5.0f, 6.0f, 7.0f);
	sys.Add(e0, local);

	// Update() is a no-op: calling it must not change any observable state.
	// Verify that the world matrix is still the identity (default) after Update().
	sys.Update();
	{
		const virasa::math::Mat4& world = sys.GetWorld(e0);
		EXPECT_NEAR(world[0][0], 1.0f, kEps);
		EXPECT_NEAR(world[1][1], 1.0f, kEps);
		EXPECT_NEAR(world[2][2], 1.0f, kEps);
		EXPECT_NEAR(world[3][3], 1.0f, kEps);
		// Translation column should still be zero (no propagation ran).
		EXPECT_NEAR(world[3][0], 0.0f, kEps);
		EXPECT_NEAR(world[3][1], 0.0f, kEps);
		EXPECT_NEAR(world[3][2], 0.0f, kEps);
	}

	// The local transform is unchanged after Update().
	{
		const virasa::math::Transform& got = sys.GetLocal(e0);
		EXPECT_NEAR(got.translation.x, 5.0f, kEps);
		EXPECT_NEAR(got.translation.y, 6.0f, kEps);
		EXPECT_NEAR(got.translation.z, 7.0f, kEps);
	}

	// Dirty set is populated by Add (and SetLocal), not by Update.
	// Simulate the propagation owner's workflow:
	//   1. Read dirty set.
	//   2. Compose world matrix from local transform (root entity: world = local.ToMatrix()).
	//   3. Write through SetWorld.
	//   4. Clear dirty.
	const auto& dirty = sys.Dirty();
	bool foundDirty = false;
	for (const auto& d : dirty)
	{
		if (d == e0) { foundDirty = true; break; }
	}
	EXPECT_TRUE(foundDirty) << "Add should have marked e0 dirty for the propagation owner";

	// Propagation owner composes the world matrix.
	virasa::math::Mat4 composed = sys.GetLocal(e0).ToMatrix();
	sys.SetWorld(e0, composed);
	sys.ClearDirty(e0);

	// After propagation, world matrix reflects the local transform.
	{
		const virasa::math::Mat4& world = sys.GetWorld(e0);
		EXPECT_NEAR(world[3][0], 5.0f, kEps);
		EXPECT_NEAR(world[3][1], 6.0f, kEps);
		EXPECT_NEAR(world[3][2], 7.0f, kEps);
	}

	// Dirty set is now empty.
	EXPECT_TRUE(sys.Dirty().empty());
}
