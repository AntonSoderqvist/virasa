#include <gtest/gtest.h>

#include "virasa/ecs/World.h"
#include "virasa/ecs/Types.h"
#include "virasa/ecs/Components.h"
#include "virasa/math/Transform.h"

#include <cstdint>
#include <vector>

using namespace virasa::ecs;
using namespace virasa::math;

// ---------------------------------------------------------------------------
// Helper utilities
// ---------------------------------------------------------------------------
namespace
{

// Build a default Transform with a recognisable translation so tests can
// distinguish different component values.
Transform MakeTransform(float x, float y, float z)
{
	Transform t;
	t.translation = {x, y, z};
	return t;
}

} // namespace

// ===========================================================================
// world_is_movable_non_copyable_owns_entity_state
// ===========================================================================
TEST(World, test_world_is_movable_non_copyable_owns_entity_state)
{
	// Verify non-copyable at compile time via type traits.
	EXPECT_FALSE(std::is_copy_constructible_v<World>);
	EXPECT_FALSE(std::is_copy_assignable_v<World>);

	// Verify movable.
	EXPECT_TRUE(std::is_move_constructible_v<World>);
	EXPECT_TRUE(std::is_move_assignable_v<World>);

	// Verify final.
	EXPECT_TRUE(std::is_final_v<World>);

	// Build a world with some state.
	World src;
	Entity e1 = src.CreateEntity();
	Entity e2 = src.CreateEntity();
	src.AddTransformComponent(e1, MakeTransform(1.f, 2.f, 3.f));
	src.AddMeshComponent(e2, MeshComponent{42u});
	src.AddVisualComponent(e2, VisualComponent{7u});
	EXPECT_EQ(src.GetEntityCount(), 2u);

	// Move-construct.
	World dst(std::move(src));
	EXPECT_EQ(dst.GetEntityCount(), 2u);
	EXPECT_TRUE(dst.IsValid(e1));
	EXPECT_TRUE(dst.IsValid(e2));
	EXPECT_TRUE(dst.HasTransformComponent(e1));
	EXPECT_TRUE(dst.HasMeshComponent(e2));
	EXPECT_TRUE(dst.HasVisualComponent(e2));

	// Source is left in a moved-from (default-constructed-equivalent) state.
	EXPECT_EQ(src.GetEntityCount(), 0u);
	EXPECT_TRUE(src.GetTransformComponentEntities().empty());
	EXPECT_TRUE(src.GetMeshComponentEntities().empty());
	EXPECT_TRUE(src.GetVisualComponentEntities().empty());

	// Move-assign into a world that already owns state.
	World dst2;
	Entity e3 = dst2.CreateEntity();
	dst2.AddVisualComponent(e3, VisualComponent{99u});
	EXPECT_EQ(dst2.GetEntityCount(), 1u);

	dst2 = std::move(dst);
	EXPECT_EQ(dst2.GetEntityCount(), 2u);
	EXPECT_TRUE(dst2.IsValid(e1));
	EXPECT_TRUE(dst2.IsValid(e2));

	// dst is now moved-from.
	EXPECT_EQ(dst.GetEntityCount(), 0u);
	EXPECT_TRUE(dst.GetTransformComponentEntities().empty());
	EXPECT_TRUE(dst.GetMeshComponentEntities().empty());
	EXPECT_TRUE(dst.GetVisualComponentEntities().empty());

	// Destroying a moved-from World is well-defined (implicit at scope exit).
}

// ===========================================================================
// world_default_constructed_is_ready
// ===========================================================================
TEST(World, test_world_default_constructed_is_ready)
{
	World w;

	// No Initialize required; all accessors are immediately callable.
	EXPECT_EQ(w.GetEntityCount(), 0u);
	EXPECT_FALSE(w.IsValid(Entity::Invalid()));
	EXPECT_FALSE(w.IsValid(Entity{}));
	EXPECT_TRUE(w.GetTransformComponentEntities().empty());
	EXPECT_TRUE(w.GetMeshComponentEntities().empty());
	EXPECT_TRUE(w.GetVisualComponentEntities().empty());

	// Reserve and CreateEntity are callable without prior Init.
	w.Reserve(64u);
	Entity e = w.CreateEntity();
	EXPECT_TRUE(w.IsValid(e));
	EXPECT_EQ(w.GetEntityCount(), 1u);
}

// ===========================================================================
// world_create_entity_allocates_index_and_generation
// ===========================================================================
TEST(World, test_world_create_entity_allocates_index_and_generation)
{
	World w;

	// First entity: growth path — index should be 0, generation 0.
	Entity e0 = w.CreateEntity();
	EXPECT_EQ(e0.index, 0u);
	EXPECT_EQ(e0.generation, 0u);
	EXPECT_TRUE(w.IsValid(e0));
	EXPECT_EQ(w.GetEntityCount(), 1u);

	// Second entity: growth path — index should be 1.
	Entity e1 = w.CreateEntity();
	EXPECT_EQ(e1.index, 1u);
	EXPECT_EQ(e1.generation, 0u);
	EXPECT_TRUE(w.IsValid(e1));
	EXPECT_EQ(w.GetEntityCount(), 2u);

	// Destroy e0 to push its slot onto the free-list.
	w.DestroyEntity(e0);
	EXPECT_EQ(w.GetEntityCount(), 1u);

	// Next CreateEntity should reuse slot 0 with bumped generation.
	Entity e0b = w.CreateEntity();
	EXPECT_EQ(e0b.index, 0u);
	EXPECT_EQ(e0b.generation, 1u); // generation was bumped by DestroyEntity
	EXPECT_TRUE(w.IsValid(e0b));
	EXPECT_FALSE(w.IsValid(e0)); // stale handle
	EXPECT_EQ(w.GetEntityCount(), 2u);

	// New entity has no parent, no children, no components.
	EXPECT_EQ(w.GetParent(e0b), Entity::Invalid());
	EXPECT_TRUE(w.GetChildren(e0b).empty());
	EXPECT_FALSE(w.HasTransformComponent(e0b));
	EXPECT_FALSE(w.HasMeshComponent(e0b));
	EXPECT_FALSE(w.HasVisualComponent(e0b));

	// CreateEntity never returns Entity::Invalid().
	EXPECT_NE(e0b, Entity::Invalid());
	EXPECT_NE(e0b.index, 0xFFFFFFFFu);
}

// ===========================================================================
// world_destroy_entity_cascades_to_descendants
// ===========================================================================
TEST(World, test_world_destroy_entity_cascades_to_descendants)
{
	World w;

	// Build a small hierarchy:
	//   root
	//   ├── child1
	//   │   └── grandchild
	//   └── child2
	Entity root       = w.CreateEntity();
	Entity child1     = w.CreateEntity();
	Entity grandchild = w.CreateEntity();
	Entity child2     = w.CreateEntity();

	EXPECT_EQ(w.SetParent(child1, root),     EcsError::None);
	EXPECT_EQ(w.SetParent(grandchild, child1), EcsError::None);
	EXPECT_EQ(w.SetParent(child2, root),     EcsError::None);

	// Add components to several entities.
	w.AddTransformComponent(root,       MakeTransform(0,0,0));
	w.AddTransformComponent(child1,     MakeTransform(1,0,0));
	w.AddMeshComponent(grandchild,      MeshComponent{10u});
	w.AddVisualComponent(child2,        VisualComponent{20u});

	EXPECT_EQ(w.GetEntityCount(), 4u);

	// Destroy root — should cascade to all descendants.
	w.DestroyEntity(root);

	EXPECT_EQ(w.GetEntityCount(), 0u);
	EXPECT_FALSE(w.IsValid(root));
	EXPECT_FALSE(w.IsValid(child1));
	EXPECT_FALSE(w.IsValid(grandchild));
	EXPECT_FALSE(w.IsValid(child2));

	// Component storages must be empty.
	EXPECT_TRUE(w.GetTransformComponentEntities().empty());
	EXPECT_TRUE(w.GetMeshComponentEntities().empty());
	EXPECT_TRUE(w.GetVisualComponentEntities().empty());

	// Verify that destroying a non-root entity detaches it from its parent.
	Entity parent = w.CreateEntity();
	Entity childA = w.CreateEntity();
	Entity childB = w.CreateEntity();
	EXPECT_EQ(w.SetParent(childA, parent), EcsError::None);
	EXPECT_EQ(w.SetParent(childB, parent), EcsError::None);
	EXPECT_EQ(w.GetChildren(parent).size(), 2u);

	w.DestroyEntity(childA);
	EXPECT_FALSE(w.IsValid(childA));
	EXPECT_TRUE(w.IsValid(parent));
	// parent's children vector should no longer contain childA.
	const auto& children = w.GetChildren(parent);
	for (const Entity& ch : children)
	{
		EXPECT_NE(ch, childA);
	}
	EXPECT_EQ(children.size(), 1u);
	EXPECT_EQ(children[0], childB);
}

// ===========================================================================
// world_is_valid_checks_generation_against_slot
// ===========================================================================
TEST(World, test_world_is_valid_checks_generation_against_slot)
{
	World w;

	// Entity::Invalid() is always invalid.
	EXPECT_FALSE(w.IsValid(Entity::Invalid()));

	// Default-constructed Entity (same bit pattern as Invalid) is invalid.
	EXPECT_FALSE(w.IsValid(Entity{}));

	// An out-of-range index is invalid.
	Entity outOfRange;
	outOfRange.index = 9999u;
	outOfRange.generation = 0u;
	EXPECT_FALSE(w.IsValid(outOfRange));

	// A freshly created entity is valid.
	Entity e = w.CreateEntity();
	EXPECT_TRUE(w.IsValid(e));

	// After destruction the handle is invalid.
	w.DestroyEntity(e);
	EXPECT_FALSE(w.IsValid(e));

	// A handle with the old generation is invalid even if the slot is reused.
	Entity eNew = w.CreateEntity();
	EXPECT_EQ(eNew.index, e.index); // same slot reused
	EXPECT_NE(eNew.generation, e.generation); // different generation
	EXPECT_TRUE(w.IsValid(eNew));
	EXPECT_FALSE(w.IsValid(e)); // stale

	// A handle with a wrong (future) generation is invalid.
	Entity future = eNew;
	future.generation += 1u;
	EXPECT_FALSE(w.IsValid(future));
}

// ===========================================================================
// world_reserve_resizes_internal_storage
// ===========================================================================
TEST(World, test_world_reserve_resizes_internal_storage)
{
	World w;

	// Reserve should not change entity count.
	w.Reserve(1024u);
	EXPECT_EQ(w.GetEntityCount(), 0u);

	// After Reserve, CreateEntity calls should still work correctly.
	std::vector<Entity> entities;
	entities.reserve(100u);
	for (uint32_t i = 0; i < 100u; ++i)
	{
		entities.push_back(w.CreateEntity());
	}
	EXPECT_EQ(w.GetEntityCount(), 100u);
	for (const Entity& e : entities)
	{
		EXPECT_TRUE(w.IsValid(e));
	}

	// Calling Reserve with a smaller value is a no-op (must not crash).
	w.Reserve(1u);
	EXPECT_EQ(w.GetEntityCount(), 100u);
}

// ===========================================================================
// world_get_entity_count_returns_live_count
// ===========================================================================
TEST(World, test_world_get_entity_count_returns_live_count)
{
	World w;
	EXPECT_EQ(w.GetEntityCount(), 0u);

	Entity e1 = w.CreateEntity();
	EXPECT_EQ(w.GetEntityCount(), 1u);

	Entity e2 = w.CreateEntity();
	EXPECT_EQ(w.GetEntityCount(), 2u);

	Entity e3 = w.CreateEntity();
	EXPECT_EQ(w.GetEntityCount(), 3u);

	// Destroy one — count decreases by 1.
	w.DestroyEntity(e2);
	EXPECT_EQ(w.GetEntityCount(), 2u);

	// Reuse the freed slot — count goes back up.
	Entity e2b = w.CreateEntity();
	(void)e2b;
	EXPECT_EQ(w.GetEntityCount(), 3u);

	// Build a small hierarchy and destroy the root — count decreases by subtree size.
	Entity root   = w.CreateEntity(); // count = 4
	Entity child  = w.CreateEntity(); // count = 5
	w.SetParent(child, root);
	EXPECT_EQ(w.GetEntityCount(), 5u);
	w.DestroyEntity(root); // destroys root + child
	EXPECT_EQ(w.GetEntityCount(), 3u);

	// Moved-from World has count 0.
	World w2(std::move(w));
	EXPECT_EQ(w.GetEntityCount(), 0u);
	EXPECT_EQ(w2.GetEntityCount(), 3u);
}

// ===========================================================================
// world_set_parent_manages_hierarchy_and_rejects_cycles
// ===========================================================================
TEST(World, test_world_set_parent_manages_hierarchy_and_rejects_cycles)
{
	World w;
	Entity a = w.CreateEntity();
	Entity b = w.CreateEntity();
	Entity c = w.CreateEntity();

	// (1) Invalid child → EcsError::InvalidEntity.
	EXPECT_EQ(w.SetParent(Entity::Invalid(), a), EcsError::InvalidEntity);

	// (2) Invalid parent (non-Invalid but not live) → EcsError::InvalidEntity.
	Entity dead = a;
	w.DestroyEntity(dead); // dead is now invalid
	Entity aNew = w.CreateEntity(); // reuses slot
	(void)aNew;
	// dead.generation is stale
	EXPECT_EQ(w.SetParent(b, dead), EcsError::InvalidEntity);

	// (3) Self-parent → EcsError::SelfParent.
	EXPECT_EQ(w.SetParent(b, b), EcsError::SelfParent);

	// (4) Cycle detection: make b a child of c, then try to make c a child of b.
	EXPECT_EQ(w.SetParent(b, c), EcsError::None);
	EXPECT_EQ(w.SetParent(c, b), EcsError::CycleDetected);

	// (5) Normal reparent.
	EXPECT_EQ(w.SetParent(b, Entity::Invalid()), EcsError::None); // detach b
	EXPECT_EQ(w.GetParent(b), Entity::Invalid());

	EXPECT_EQ(w.SetParent(b, c), EcsError::None);
	EXPECT_EQ(w.GetParent(b), c);
	const auto& cChildren = w.GetChildren(c);
	EXPECT_FALSE(cChildren.empty());
	bool foundB = false;
	for (const Entity& ch : cChildren)
	{
		if (ch == b) { foundB = true; }
	}
	EXPECT_TRUE(foundB);

	// Reparent b to aNew — b should be removed from c's children.
	EXPECT_EQ(w.SetParent(b, aNew), EcsError::None);
	EXPECT_EQ(w.GetParent(b), aNew);
	for (const Entity& ch : w.GetChildren(c))
	{
		EXPECT_NE(ch, b);
	}

	// SetParent with same parent is a no-op returning None.
	EXPECT_EQ(w.SetParent(b, aNew), EcsError::None);
	EXPECT_EQ(w.GetParent(b), aNew);
}

// ===========================================================================
// world_get_parent_returns_invalid_for_roots
// ===========================================================================
TEST(World, test_world_get_parent_returns_invalid_for_roots)
{
	World w;
	Entity root  = w.CreateEntity();
	Entity child = w.CreateEntity();

	// A freshly created entity is a root.
	EXPECT_EQ(w.GetParent(root),  Entity::Invalid());
	EXPECT_EQ(w.GetParent(child), Entity::Invalid());

	// After SetParent, GetParent returns the parent.
	EXPECT_EQ(w.SetParent(child, root), EcsError::None);
	EXPECT_EQ(w.GetParent(child), root);

	// After making child a root again, GetParent returns Invalid.
	EXPECT_EQ(w.SetParent(child, Entity::Invalid()), EcsError::None);
	EXPECT_EQ(w.GetParent(child), Entity::Invalid());

	// The returned value is a copy — it is safe to store.
	Entity storedParent = w.GetParent(root);
	EXPECT_EQ(storedParent, Entity::Invalid());
}

// ===========================================================================
// world_get_children_returns_stable_reference
// ===========================================================================
TEST(World, test_world_get_children_returns_stable_reference)
{
	World w;
	Entity parent = w.CreateEntity();
	Entity c1     = w.CreateEntity();
	Entity c2     = w.CreateEntity();
	Entity c3     = w.CreateEntity();

	// No children initially.
	EXPECT_TRUE(w.GetChildren(parent).empty());

	// Children are appended in SetParent order.
	EXPECT_EQ(w.SetParent(c1, parent), EcsError::None);
	EXPECT_EQ(w.SetParent(c2, parent), EcsError::None);
	EXPECT_EQ(w.SetParent(c3, parent), EcsError::None);

	const auto& children = w.GetChildren(parent);
	ASSERT_EQ(children.size(), 3u);
	EXPECT_EQ(children[0], c1);
	EXPECT_EQ(children[1], c2);
	EXPECT_EQ(children[2], c3);

	// Removing a child via SetParent(child, Invalid) removes it from the vector.
	EXPECT_EQ(w.SetParent(c2, Entity::Invalid()), EcsError::None);
	const auto& childrenAfter = w.GetChildren(parent);
	EXPECT_EQ(childrenAfter.size(), 2u);
	for (const Entity& ch : childrenAfter)
	{
		EXPECT_NE(ch, c2);
	}

	// DestroyEntity on a child removes it from the parent's children vector.
	w.DestroyEntity(c1);
	const auto& childrenFinal = w.GetChildren(parent);
	EXPECT_EQ(childrenFinal.size(), 1u);
	EXPECT_EQ(childrenFinal[0], c3);
}

// ===========================================================================
// world_transform_component_storage_is_sparse_set
// ===========================================================================
TEST(World, test_world_transform_component_storage_is_sparse_set)
{
	World w;
	Entity e1 = w.CreateEntity();
	Entity e2 = w.CreateEntity();
	Entity e3 = w.CreateEntity();

	// Initially no components.
	EXPECT_FALSE(w.HasTransformComponent(e1));
	EXPECT_TRUE(w.GetTransformComponentEntities().empty());

	// Add components.
	Transform t1 = MakeTransform(1.f, 0.f, 0.f);
	Transform t2 = MakeTransform(2.f, 0.f, 0.f);
	Transform t3 = MakeTransform(3.f, 0.f, 0.f);
	w.AddTransformComponent(e1, t1);
	w.AddTransformComponent(e2, t2);
	w.AddTransformComponent(e3, t3);

	EXPECT_TRUE(w.HasTransformComponent(e1));
	EXPECT_TRUE(w.HasTransformComponent(e2));
	EXPECT_TRUE(w.HasTransformComponent(e3));
	EXPECT_EQ(w.GetTransformComponentEntities().size(), 3u);

	// GetTransformComponent returns the stored value (const overload).
	const World& cw = w;
	EXPECT_FLOAT_EQ(cw.GetTransformComponent(e1).translation.x, 1.f);
	EXPECT_FLOAT_EQ(cw.GetTransformComponent(e2).translation.x, 2.f);
	EXPECT_FLOAT_EQ(cw.GetTransformComponent(e3).translation.x, 3.f);

	// Non-const overload allows mutation.
	w.GetTransformComponent(e1).translation.x = 99.f;
	EXPECT_FLOAT_EQ(cw.GetTransformComponent(e1).translation.x, 99.f);

	// Remove middle entity (swap-and-pop): e2 removed, e3 should move into its slot.
	w.RemoveTransformComponent(e2);
	EXPECT_FALSE(w.HasTransformComponent(e2));
	EXPECT_TRUE(w.HasTransformComponent(e1));
	EXPECT_TRUE(w.HasTransformComponent(e3));
	EXPECT_EQ(w.GetTransformComponentEntities().size(), 2u);

	// Remove last remaining components.
	w.RemoveTransformComponent(e1);
	w.RemoveTransformComponent(e3);
	EXPECT_TRUE(w.GetTransformComponentEntities().empty());

	// GetTransformComponentEntities contains exactly the entities with components.
	w.AddTransformComponent(e1, t1);
	w.AddTransformComponent(e3, t3);
	const auto& ents = w.GetTransformComponentEntities();
	EXPECT_EQ(ents.size(), 2u);
	bool hasE1 = false, hasE3 = false;
	for (const Entity& e : ents)
	{
		if (e == e1) hasE1 = true;
		if (e == e3) hasE3 = true;
	}
	EXPECT_TRUE(hasE1);
	EXPECT_TRUE(hasE3);
}

// ===========================================================================
// world_mesh_component_storage_is_sparse_set
// ===========================================================================
TEST(World, test_world_mesh_component_storage_is_sparse_set)
{
	World w;
	Entity e1 = w.CreateEntity();
	Entity e2 = w.CreateEntity();
	Entity e3 = w.CreateEntity();

	// Initially no components.
	EXPECT_FALSE(w.HasMeshComponent(e1));
	EXPECT_TRUE(w.GetMeshComponentEntities().empty());

	// Add components.
	w.AddMeshComponent(e1, MeshComponent{10u});
	w.AddMeshComponent(e2, MeshComponent{20u});
	w.AddMeshComponent(e3, MeshComponent{30u});

	EXPECT_TRUE(w.HasMeshComponent(e1));
	EXPECT_TRUE(w.HasMeshComponent(e2));
	EXPECT_TRUE(w.HasMeshComponent(e3));
	EXPECT_EQ(w.GetMeshComponentEntities().size(), 3u);

	// Const overload.
	const World& cw = w;
	EXPECT_EQ(cw.GetMeshComponent(e1).meshId, 10u);
	EXPECT_EQ(cw.GetMeshComponent(e2).meshId, 20u);
	EXPECT_EQ(cw.GetMeshComponent(e3).meshId, 30u);

	// Non-const overload allows mutation.
	w.GetMeshComponent(e1).meshId = 11u;
	EXPECT_EQ(cw.GetMeshComponent(e1).meshId, 11u);

	// Remove (swap-and-pop).
	w.RemoveMeshComponent(e2);
	EXPECT_FALSE(w.HasMeshComponent(e2));
	EXPECT_TRUE(w.HasMeshComponent(e1));
	EXPECT_TRUE(w.HasMeshComponent(e3));
	EXPECT_EQ(w.GetMeshComponentEntities().size(), 2u);

	w.RemoveMeshComponent(e1);
	w.RemoveMeshComponent(e3);
	EXPECT_TRUE(w.GetMeshComponentEntities().empty());

	// GetMeshComponentEntities lists exactly the entities with components.
	w.AddMeshComponent(e2, MeshComponent{200u});
	w.AddMeshComponent(e3, MeshComponent{300u});
	const auto& ents = w.GetMeshComponentEntities();
	EXPECT_EQ(ents.size(), 2u);
	bool hasE2 = false, hasE3 = false;
	for (const Entity& e : ents)
	{
		if (e == e2) hasE2 = true;
		if (e == e3) hasE3 = true;
	}
	EXPECT_TRUE(hasE2);
	EXPECT_TRUE(hasE3);
}

// ===========================================================================
// world_visual_component_storage_is_sparse_set
// ===========================================================================
TEST(World, test_world_visual_component_storage_is_sparse_set)
{
	World w;
	Entity e1 = w.CreateEntity();
	Entity e2 = w.CreateEntity();
	Entity e3 = w.CreateEntity();

	// Initially no components.
	EXPECT_FALSE(w.HasVisualComponent(e1));
	EXPECT_TRUE(w.GetVisualComponentEntities().empty());

	// Add components.
	w.AddVisualComponent(e1, VisualComponent{100u});
	w.AddVisualComponent(e2, VisualComponent{200u});
	w.AddVisualComponent(e3, VisualComponent{300u});

	EXPECT_TRUE(w.HasVisualComponent(e1));
	EXPECT_TRUE(w.HasVisualComponent(e2));
	EXPECT_TRUE(w.HasVisualComponent(e3));
	EXPECT_EQ(w.GetVisualComponentEntities().size(), 3u);

	// Const overload.
	const World& cw = w;
	EXPECT_EQ(cw.GetVisualComponent(e1).materialId, 100u);
	EXPECT_EQ(cw.GetVisualComponent(e2).materialId, 200u);
	EXPECT_EQ(cw.GetVisualComponent(e3).materialId, 300u);

	// Non-const overload allows mutation.
	w.GetVisualComponent(e1).materialId = 101u;
	EXPECT_EQ(cw.GetVisualComponent(e1).materialId, 101u);

	// Remove (swap-and-pop).
	w.RemoveVisualComponent(e2);
	EXPECT_FALSE(w.HasVisualComponent(e2));
	EXPECT_TRUE(w.HasVisualComponent(e1));
	EXPECT_TRUE(w.HasVisualComponent(e3));
	EXPECT_EQ(w.GetVisualComponentEntities().size(), 2u);

	w.RemoveVisualComponent(e1);
	w.RemoveVisualComponent(e3);
	EXPECT_TRUE(w.GetVisualComponentEntities().empty());

	// GetVisualComponentEntities lists exactly the entities with components.
	w.AddVisualComponent(e1, VisualComponent{1u});
	w.AddVisualComponent(e3, VisualComponent{3u});
	const auto& ents = w.GetVisualComponentEntities();
	EXPECT_EQ(ents.size(), 2u);
	bool hasE1 = false, hasE3 = false;
	for (const Entity& e : ents)
	{
		if (e == e1) hasE1 = true;
		if (e == e3) hasE3 = true;
	}
	EXPECT_TRUE(hasE1);
	EXPECT_TRUE(hasE3);
}

// ===========================================================================
// world_iteration_forbids_concurrent_mutation
// ===========================================================================
// This semantic is a contract-level prohibition (undefined behaviour if
// violated). We cannot safely trigger UB in a test, so we verify the
// positive side: that completing an iteration before mutating produces
// correct results, and that buffering mutations and applying them after
// the loop leaves the storage consistent.
TEST(World, test_world_iteration_forbids_concurrent_mutation)
{
	World w;
	Entity e1 = w.CreateEntity();
	Entity e2 = w.CreateEntity();
	Entity e3 = w.CreateEntity();
	w.AddTransformComponent(e1, MakeTransform(1.f, 0.f, 0.f));
	w.AddTransformComponent(e2, MakeTransform(2.f, 0.f, 0.f));
	w.AddTransformComponent(e3, MakeTransform(3.f, 0.f, 0.f));

	// Iterate and collect entities to remove — do NOT mutate during iteration.
	std::vector<Entity> toRemove;
	for (const Entity& e : w.GetTransformComponentEntities())
	{
		if (w.GetTransformComponent(e).translation.x >= 2.f)
		{
			toRemove.push_back(e);
		}
	}

	// Apply buffered mutations after the loop.
	for (const Entity& e : toRemove)
	{
		w.RemoveTransformComponent(e);
	}

	// Only e1 should remain.
	EXPECT_EQ(w.GetTransformComponentEntities().size(), 1u);
	EXPECT_TRUE(w.HasTransformComponent(e1));
	EXPECT_FALSE(w.HasTransformComponent(e2));
	EXPECT_FALSE(w.HasTransformComponent(e3));

	// Same pattern for GetChildren.
	Entity parent = w.CreateEntity();
	Entity ca     = w.CreateEntity();
	Entity cb     = w.CreateEntity();
	w.SetParent(ca, parent);
	w.SetParent(cb, parent);

	std::vector<Entity> childSnapshot;
	for (const Entity& ch : w.GetChildren(parent))
	{
		childSnapshot.push_back(ch);
	}
	// Mutate after iteration.
	for (const Entity& ch : childSnapshot)
	{
		w.SetParent(ch, Entity::Invalid());
	}
	EXPECT_TRUE(w.GetChildren(parent).empty());
}

// ===========================================================================
// world_is_not_thread_safe_per_instance
// ===========================================================================
// Thread-safety is a per-instance contract: distinct World objects may be
// used from different threads without synchronization. We verify the
// positive (safe) case: two independent World objects operated from the
// same thread each maintain correct state, confirming no shared global
// state exists between instances.
TEST(World, test_world_is_not_thread_safe_per_instance)
{
	World w1;
	World w2;

	Entity e1a = w1.CreateEntity();
	Entity e1b = w1.CreateEntity();
	Entity e2a = w2.CreateEntity();

	w1.AddTransformComponent(e1a, MakeTransform(1.f, 0.f, 0.f));
	w2.AddMeshComponent(e2a, MeshComponent{55u});

	// w1 and w2 are fully independent — mutations in one do not affect the other.
	EXPECT_EQ(w1.GetEntityCount(), 2u);
	EXPECT_EQ(w2.GetEntityCount(), 1u);

	EXPECT_TRUE(w1.HasTransformComponent(e1a));
	EXPECT_FALSE(w1.HasMeshComponent(e1a));

	EXPECT_TRUE(w2.HasMeshComponent(e2a));
	EXPECT_FALSE(w2.HasTransformComponent(e2a));

	w1.DestroyEntity(e1b);
	EXPECT_EQ(w1.GetEntityCount(), 1u);
	EXPECT_EQ(w2.GetEntityCount(), 1u); // w2 unaffected

	// Const observers may be called concurrently on distinct instances
	// (verified here sequentially as a correctness check).
	EXPECT_FALSE(w1.IsValid(Entity::Invalid()));
	EXPECT_FALSE(w2.IsValid(Entity::Invalid()));
}
