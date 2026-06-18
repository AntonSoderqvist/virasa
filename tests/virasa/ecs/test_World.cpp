#include <gtest/gtest.h>

#include "virasa/ecs/World.h"
#include "virasa/ecs/Types.h"
#include "virasa/ecs/Components.h"
#include "virasa/ecs/ComponentSystem.h"
#include "virasa/ecs/TransformSystem.h"
#include "virasa/math/Transform.h"
#include "virasa/math/Types.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <typeindex>
#include <vector>
#include <algorithm>

using namespace virasa::ecs;
using namespace virasa::math;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace
{

// A minimal concrete ComponentSystem for use in RegisterSystem tests.
class TestSystem final : public ComponentSystem
{
public:
	explicit TestSystem(const char* name) : mName(name) {}

	const char* Name() const noexcept override { return mName; }
	ComponentId Id()   const noexcept override { return mId; }

	bool Has(Entity) const noexcept override { return false; }
	void* AddRaw(Entity, const void*) override { return nullptr; }
	void Remove(Entity) override {}
	size_t Size() const noexcept override { return 0; }
	const std::vector<Entity>& Entities() const noexcept override { return mEntities; }
	const void* GetRaw(Entity) const override { return nullptr; }
	void SetRaw(Entity, const void*) override {}
	void ForEachRaw(const std::function<void(Entity, const void*)>&) const override {}
	const std::vector<Entity>& Dirty() const noexcept override { return mDirty; }
	void ClearDirty(Entity) override {}
	void ClearAllDirty() override {}
	void Update() override {}
	std::unique_ptr<ComponentSystem> Clone() const override
	{
		auto clone = std::make_unique<TestSystem>(mName);
		clone->SetId(mId);
		return clone;
	}

	void SetId(ComponentId id) { mId = id; }

private:
	const char* mName;
	ComponentId mId = kInvalidComponentId;
	std::vector<Entity> mEntities;
	std::vector<Entity> mDirty;
};

// Convenience: add a Transform to an entity via the World's TransformSystem.
void AddTransform(World& world, Entity entity, const Transform& t = {})
{
	world.Transforms().Add(entity, t);
}

} // namespace

// ===========================================================================
// world_owns_entity_state_and_system_registry
// ===========================================================================
TEST(World, test_world_owns_entity_state_and_system_registry)
{
	// World is default-constructible, not copyable, and movable.
	static_assert(!std::is_copy_constructible_v<World>, "World must not be copy-constructible");
	static_assert(!std::is_copy_assignable_v<World>,    "World must not be copy-assignable");
	static_assert(std::is_move_constructible_v<World>,  "World must be move-constructible");
	static_assert(std::is_move_assignable_v<World>,     "World must be move-assignable");
	static_assert(std::is_default_constructible_v<World>, "World must be default-constructible");

	World w;
	Entity e = w.CreateEntity("A");
	Entity f = w.CreateEntity("B");
	EXPECT_TRUE(w.IsValid(e));
	EXPECT_TRUE(w.IsValid(f));

	// Move-construct: source entities are no longer valid in the new world
	// (the new world owns the state).
	World w2(std::move(w));
	EXPECT_TRUE(w2.IsValid(e));
	EXPECT_TRUE(w2.IsValid(f));

	// Move-assign into a world that already has state: old state is torn down.
	World w3;
	(void)w3.CreateEntity("C");
	(void)w3.CreateEntity("D");
	// g sits at slot index 2, beyond the two entities (slots 0 and 1) that w2
	// carries. After the move w3's entity table holds only w2's state, so g's
	// index is out of range and the stale handle is no longer valid. (Entity
	// handles carry no world identity, so a handle whose index/generation
	// happens to alias a live slot in the moved-in state would still validate;
	// choosing an out-of-range index tests the teardown without that ambiguity.)
	Entity g = w3.CreateEntity("E");
	w3 = std::move(w2);
	EXPECT_TRUE(w3.IsValid(e));
	EXPECT_TRUE(w3.IsValid(f));
	EXPECT_FALSE(w3.IsValid(g));
}

// ===========================================================================
// world_default_constructed_registers_builtin_systems
// ===========================================================================
TEST(World, test_world_default_constructed_registers_builtin_systems)
{
	World world;

	// Entity count is 0.
	EXPECT_EQ(world.GetEntityCount(), 0u);

	// IsValid returns false for Entity::Invalid().
	EXPECT_FALSE(world.IsValid(Entity::Invalid()));

	// GetRoots is empty.
	EXPECT_TRUE(world.GetRoots().empty());

	// FindEntityByName returns Invalid for any name.
	EXPECT_EQ(world.FindEntityByName("anything"), Entity::Invalid());

	// Built-in system names are registered.
	const char* builtins[] = {
		"Transform", "Mesh", "Visual",
		"DirectionalLight", "PointLight", "SpotLight", "Camera", "Highlight"
	};
	for (const char* name : builtins)
	{
		ComponentId id = world.GetSystemId(name);
		EXPECT_NE(id, kInvalidComponentId) << "Expected valid id for built-in system: " << name;
		EXPECT_EQ(world.GetSystem(id).Size(), 0u) << "Expected size 0 for: " << name;
	}

	// GetSystemId returns kInvalidComponentId for an unregistered name.
	EXPECT_EQ(world.GetSystemId("NonExistent"), kInvalidComponentId);

	// Transform system is accessible via Transforms().
	ComponentId transformId = world.GetSystemId("Transform");
	EXPECT_NE(transformId, kInvalidComponentId);
	EXPECT_EQ(&world.Transforms(), &world.GetSystem(transformId));
}

// ===========================================================================
// world_register_system_assigns_component_id
// ===========================================================================
TEST(World, test_world_register_system_assigns_component_id)
{
	World world;

	// Count how many built-ins were registered.
	const char* builtins[] = {
		"Transform", "Mesh", "Visual",
		"DirectionalLight", "PointLight", "SpotLight", "Camera"
	};
	ComponentId maxBuiltin = 0;
	for (const char* name : builtins)
	{
		ComponentId id = world.GetSystemId(name);
		if (id != kInvalidComponentId && id > maxBuiltin)
			maxBuiltin = id;
	}

	// Register a custom system; its id must be past the built-in range.
	auto sys1 = std::make_unique<TestSystem>("MySystem");
	TestSystem* raw1 = sys1.get();
	ComponentId id1 = world.RegisterSystem(std::move(sys1));
	EXPECT_NE(id1, kInvalidComponentId);
	EXPECT_GT(id1, maxBuiltin);

	// The system's own Id() reflects the assigned id.
	EXPECT_EQ(world.GetSystem(id1).Id(), id1);

	// GetSystemId by name returns the assigned id.
	EXPECT_EQ(world.GetSystemId("MySystem"), id1);

	// Register a second system; its id is consecutive.
	auto sys2 = std::make_unique<TestSystem>("MySystem2");
	ComponentId id2 = world.RegisterSystem(std::move(sys2));
	EXPECT_EQ(id2, id1 + 1u);

	// GetSystem returns the correct system by id.
	EXPECT_EQ(world.GetSystem(id1).Name(), std::string_view("MySystem"));
	EXPECT_EQ(world.GetSystem(id2).Name(), std::string_view("MySystem2"));
}

// ===========================================================================
// world_system_lookup_by_id_and_name
// ===========================================================================
TEST(World, test_world_system_lookup_by_id_and_name)
{
	World world;

	// GetSystemId for a registered built-in.
	ComponentId meshId = world.GetSystemId("Mesh");
	EXPECT_NE(meshId, kInvalidComponentId);

	// GetSystem (non-const) returns a reference.
	ComponentSystem& sys = world.GetSystem(meshId);
	EXPECT_EQ(sys.Name(), std::string_view("Mesh"));

	// GetSystem (const) returns a const reference to the same system.
	const World& cworld = world;
	const ComponentSystem& csys = cworld.GetSystem(meshId);
	EXPECT_EQ(&sys, &csys);

	// GetSystemId for unknown name returns kInvalidComponentId.
	EXPECT_EQ(world.GetSystemId("Unknown"), kInvalidComponentId);

	// FindSystem returns a non-null pointer for a known name.
	ComponentSystem* found = world.FindSystem("Mesh");
	ASSERT_NE(found, nullptr);
	EXPECT_EQ(found, &sys);

	// FindSystem returns nullptr for an unknown name.
	EXPECT_EQ(world.FindSystem("NotRegistered"), nullptr);

	// Register a custom system and verify all three lookup paths.
	auto custom = std::make_unique<TestSystem>("Custom");
	ComponentId customId = world.RegisterSystem(std::move(custom));
	EXPECT_EQ(world.GetSystemId("Custom"), customId);
	EXPECT_NE(world.FindSystem("Custom"), nullptr);
	EXPECT_EQ(world.GetSystem(customId).Name(), std::string_view("Custom"));
}

// ===========================================================================
// world_get_entities_intersects_component_systems
// ===========================================================================
TEST(World, test_world_get_entities_intersects_component_systems)
{
	World world;

	ComponentId transformId = world.GetSystemId("Transform");
	ComponentId meshId      = world.GetSystemId("Mesh");
	ASSERT_NE(transformId, kInvalidComponentId);
	ASSERT_NE(meshId, kInvalidComponentId);

	Entity eA = world.CreateEntity("A");
	Entity eB = world.CreateEntity("B");
	Entity eC = world.CreateEntity("C");

	// Add Transform to A and B; Mesh to A and C.
	Transform t{};
	world.Transforms().Add(eA, t);
	world.Transforms().Add(eB, t);

	MeshComponent mesh{};
	mesh.meshId = 1u;
	world.GetSystem(meshId).AddRaw(eA, &mesh);
	world.GetSystem(meshId).AddRaw(eC, &mesh);

	// Intersection of Transform and Mesh should be {eA} only.
	std::vector<Entity> result = world.GetEntities({transformId, meshId});
	EXPECT_EQ(result.size(), 1u);
	if (!result.empty())
		EXPECT_EQ(result[0], eA);

	// Single-system query: all entities with Transform = {eA, eB}.
	std::vector<Entity> transforms = world.GetEntities({transformId});
	EXPECT_EQ(transforms.size(), 2u);
	bool hasA = std::find(transforms.begin(), transforms.end(), eA) != transforms.end();
	bool hasB = std::find(transforms.begin(), transforms.end(), eB) != transforms.end();
	EXPECT_TRUE(hasA);
	EXPECT_TRUE(hasB);

	// All returned entities must be live.
	for (Entity e : result)
		EXPECT_TRUE(world.IsValid(e));
}

// ===========================================================================
// world_transforms_accessor_and_drives_propagation
// ===========================================================================
TEST(World, test_world_transforms_accessor_and_drives_propagation)
{
	World world;

	// Transforms() and GetTransforms() return the same system.
	TransformSystem& ts        = world.Transforms();
	const World& cworld        = world;
	const TransformSystem& cts = cworld.GetTransforms();
	EXPECT_EQ(&ts, &cts);
	EXPECT_EQ(ts.Name(), std::string_view("Transform"));

	// Create a parent-child pair with Transforms.
	Entity parent = world.CreateEntity("Parent");
	Entity child  = world.CreateEntity("Child");
	EXPECT_EQ(world.SetParent(child, parent), EcsError::None);

	Transform parentLocal{};
	parentLocal.translation = Vec3(1.0f, 0.0f, 0.0f);
	world.Transforms().Add(parent, parentLocal);

	Transform childLocal{};
	childLocal.translation = Vec3(0.0f, 2.0f, 0.0f);
	world.Transforms().Add(child, childLocal);

	// UpdateTransforms should propagate.
	world.UpdateTransforms();

	// Parent world matrix: translation (1,0,0).
	Mat4 parentWorld = world.Transforms().GetWorld(parent);
	EXPECT_NEAR(parentWorld[3][0], 1.0f, 1e-5f);
	EXPECT_NEAR(parentWorld[3][1], 0.0f, 1e-5f);
	EXPECT_NEAR(parentWorld[3][2], 0.0f, 1e-5f);

	// Child world matrix: parent (1,0,0) + child local (0,2,0) = (1,2,0).
	Mat4 childWorld = world.Transforms().GetWorld(child);
	EXPECT_NEAR(childWorld[3][0], 1.0f, 1e-5f);
	EXPECT_NEAR(childWorld[3][1], 2.0f, 1e-5f);
	EXPECT_NEAR(childWorld[3][2], 0.0f, 1e-5f);

	// After UpdateTransforms, dirty set is cleared; a second call does no harm.
	world.UpdateTransforms();
	EXPECT_TRUE(world.Transforms().Dirty().empty());
}

// ===========================================================================
// world_create_entity_allocates_index_and_generation
// ===========================================================================
TEST(World, test_world_create_entity_allocates_index_and_generation)
{
	World world;

	// First entity: generation 0, index not the sentinel.
	Entity e1 = world.CreateEntity("E1");
	EXPECT_NE(e1.index, 0xFFFFFFFFu);
	EXPECT_TRUE(world.IsValid(e1));
	EXPECT_EQ(world.GetEntityCount(), 1u);

	// No parent, no children.
	EXPECT_EQ(world.GetParent(e1), Entity::Invalid());
	EXPECT_TRUE(world.GetChildren(e1).empty());

	// Appears in roots.
	const auto& roots = world.GetRoots();
	bool inRoots = std::find(roots.begin(), roots.end(), e1) != roots.end();
	EXPECT_TRUE(inRoots);

	// No components in built-in systems.
	EXPECT_FALSE(world.Transforms().Has(e1));

	// Slot reuse: destroy e1 and create again; new entity reuses the slot
	// but has a bumped generation.
	uint32_t oldIndex = e1.index;
	uint32_t oldGen   = e1.generation;
	world.DestroyEntity(e1);

	Entity e2 = world.CreateEntity("E2");
	EXPECT_EQ(e2.index, oldIndex);         // slot reused
	EXPECT_GT(e2.generation, oldGen);      // generation bumped
	EXPECT_TRUE(world.IsValid(e2));
	EXPECT_FALSE(world.IsValid(e1));        // stale handle
}

// ===========================================================================
// world_destroy_entity_cascades_to_descendants
// ===========================================================================
TEST(World, test_world_destroy_entity_cascades_to_descendants)
{
	World world;

	// Build a small tree: root -> child -> grandchild.
	Entity root       = world.CreateEntity("Root");
	Entity child      = world.CreateEntity("Child");
	Entity grandchild = world.CreateEntity("GC");
	EXPECT_EQ(world.SetParent(child, root), EcsError::None);
	EXPECT_EQ(world.SetParent(grandchild, child), EcsError::None);

	// Add a Transform to each.
	Transform t{};
	world.Transforms().Add(root,       t);
	world.Transforms().Add(child,      t);
	world.Transforms().Add(grandchild, t);

	EXPECT_EQ(world.GetEntityCount(), 3u);

	// Destroy the root; all three entities must be gone.
	world.DestroyEntity(root);

	EXPECT_EQ(world.GetEntityCount(), 0u);
	EXPECT_FALSE(world.IsValid(root));
	EXPECT_FALSE(world.IsValid(child));
	EXPECT_FALSE(world.IsValid(grandchild));

	// Root is no longer in roots.
	const auto& roots = world.GetRoots();
	bool rootPresent = std::find(roots.begin(), roots.end(), root) != roots.end();
	EXPECT_FALSE(rootPresent);

	// Transform system has no entries.
	EXPECT_EQ(world.Transforms().Size(), 0u);

	// Name storage is cleared.
	EXPECT_EQ(world.FindEntityByName("Root"),  Entity::Invalid());
	EXPECT_EQ(world.FindEntityByName("Child"), Entity::Invalid());
	EXPECT_EQ(world.FindEntityByName("GC"),    Entity::Invalid());
}

// ===========================================================================
// world_is_valid_checks_generation_against_slot
// ===========================================================================
TEST(World, test_world_is_valid_checks_generation_against_slot)
{
	World world;

	// Entity::Invalid() is always invalid.
	EXPECT_FALSE(world.IsValid(Entity::Invalid()));

	// Default-constructed Entity is also invalid.
	EXPECT_FALSE(world.IsValid(Entity{}));

	// A freshly created entity is valid.
	Entity e = world.CreateEntity("E");
	EXPECT_TRUE(world.IsValid(e));

	// After destroy, the same handle is invalid.
	world.DestroyEntity(e);
	EXPECT_FALSE(world.IsValid(e));

	// A new entity reusing the same slot has a different generation.
	Entity e2 = world.CreateEntity("E2");
	EXPECT_EQ(e2.index, e.index);
	EXPECT_TRUE(world.IsValid(e2));
	EXPECT_FALSE(world.IsValid(e));  // stale handle still invalid

	// An entity with an out-of-range index is invalid.
	Entity outOfRange;
	outOfRange.index      = 0xFFFFFFFEu;
	outOfRange.generation = 0u;
	EXPECT_FALSE(world.IsValid(outOfRange));
}

// ===========================================================================
// world_reserve_resizes_internal_storage
// ===========================================================================
TEST(World, test_world_reserve_resizes_internal_storage)
{
	World world;

	// Reserve should not change entity count.
	world.Reserve(1000u);
	EXPECT_EQ(world.GetEntityCount(), 0u);

	// Entities can still be created after Reserve.
	Entity e = world.CreateEntity("AfterReserve");
	EXPECT_TRUE(world.IsValid(e));
	EXPECT_EQ(world.GetEntityCount(), 1u);

	// Reserve with a value less than current capacity is a no-op.
	world.Reserve(1u);
	EXPECT_EQ(world.GetEntityCount(), 1u);
	EXPECT_TRUE(world.IsValid(e));
}

// ===========================================================================
// world_get_entity_count_returns_live_count
// ===========================================================================
TEST(World, test_world_get_entity_count_returns_live_count)
{
	World world;
	EXPECT_EQ(world.GetEntityCount(), 0u);

	Entity e1 = world.CreateEntity("A");
	EXPECT_EQ(world.GetEntityCount(), 1u);

	Entity e2 = world.CreateEntity("B");
	EXPECT_EQ(world.GetEntityCount(), 2u);

	Entity e3 = world.CreateEntity("C");
	EXPECT_EQ(world.GetEntityCount(), 3u);

	// Destroy one leaf.
	world.DestroyEntity(e3);
	EXPECT_EQ(world.GetEntityCount(), 2u);

	// Build a subtree: e1 -> e2 (child). Destroy e1 cascades to e2.
	EXPECT_EQ(world.SetParent(e2, e1), EcsError::None);
	world.DestroyEntity(e1);
	EXPECT_EQ(world.GetEntityCount(), 0u);
}

// ===========================================================================
// world_set_parent_manages_hierarchy_and_rejects_cycles
// ===========================================================================
TEST(World, test_world_set_parent_manages_hierarchy_and_rejects_cycles)
{
	World world;

	Entity a = world.CreateEntity("A");
	Entity b = world.CreateEntity("B");
	Entity c = world.CreateEntity("C");

	// (1) Invalid child => InvalidEntity.
	EXPECT_EQ(world.SetParent(Entity::Invalid(), a), EcsError::InvalidEntity);

	// (2) Invalid parent (not Entity::Invalid but a destroyed entity) => InvalidEntity.
	Entity dead = world.CreateEntity("Dead");
	world.DestroyEntity(dead);
	EXPECT_EQ(world.SetParent(a, dead), EcsError::InvalidEntity);

	// (3) Self-parent => SelfParent.
	EXPECT_EQ(world.SetParent(a, a), EcsError::SelfParent);

	// (4) Cycle detection: a -> b -> c, then try c as parent of a.
	EXPECT_EQ(world.SetParent(b, a), EcsError::None);
	EXPECT_EQ(world.SetParent(c, b), EcsError::None);
	EXPECT_EQ(world.SetParent(a, c), EcsError::CycleDetected);

	// (5) Valid reparent: c's parent is b; reparent c to a.
	EXPECT_EQ(world.SetParent(c, a), EcsError::None);
	EXPECT_EQ(world.GetParent(c), a);

	// Roots maintenance: a is a root; b is child of a; c is now child of a.
	const auto& roots = world.GetRoots();
	bool aInRoots = std::find(roots.begin(), roots.end(), a) != roots.end();
	bool bInRoots = std::find(roots.begin(), roots.end(), b) != roots.end();
	bool cInRoots = std::find(roots.begin(), roots.end(), c) != roots.end();
	EXPECT_TRUE(aInRoots);
	EXPECT_FALSE(bInRoots);
	EXPECT_FALSE(cInRoots);

	// Reparent a to Entity::Invalid() is a no-op (already a root).
	EXPECT_EQ(world.SetParent(a, Entity::Invalid()), EcsError::None);
	EXPECT_EQ(world.GetParent(a), Entity::Invalid());
}

// ===========================================================================
// world_get_parent_returns_invalid_for_roots
// ===========================================================================
TEST(World, test_world_get_parent_returns_invalid_for_roots)
{
	World world;

	Entity root  = world.CreateEntity("Root");
	Entity child = world.CreateEntity("Child");

	// Freshly created entity is a root.
	EXPECT_EQ(world.GetParent(root),  Entity::Invalid());
	EXPECT_EQ(world.GetParent(child), Entity::Invalid());

	// After SetParent, GetParent returns the parent.
	EXPECT_EQ(world.SetParent(child, root), EcsError::None);
	EXPECT_EQ(world.GetParent(child), root);

	// Reparent child back to no parent.
	EXPECT_EQ(world.SetParent(child, Entity::Invalid()), EcsError::None);
	EXPECT_EQ(world.GetParent(child), Entity::Invalid());
}

// ===========================================================================
// world_get_children_returns_stable_reference
// ===========================================================================
TEST(World, test_world_get_children_returns_stable_reference)
{
	World world;

	Entity parent = world.CreateEntity("Parent");
	Entity c1     = world.CreateEntity("C1");
	Entity c2     = world.CreateEntity("C2");

	// No children initially.
	EXPECT_TRUE(world.GetChildren(parent).empty());

	// Add first child.
	EXPECT_EQ(world.SetParent(c1, parent), EcsError::None);
	{
		const auto& children = world.GetChildren(parent);
		EXPECT_EQ(children.size(), 1u);
		EXPECT_EQ(children[0], c1);
	}

	// Add second child.
	EXPECT_EQ(world.SetParent(c2, parent), EcsError::None);
	{
		const auto& children = world.GetChildren(parent);
		EXPECT_EQ(children.size(), 2u);
		bool hasC1 = std::find(children.begin(), children.end(), c1) != children.end();
		bool hasC2 = std::find(children.begin(), children.end(), c2) != children.end();
		EXPECT_TRUE(hasC1);
		EXPECT_TRUE(hasC2);
	}

	// After removing c1 from parent, children contains only c2.
	EXPECT_EQ(world.SetParent(c1, Entity::Invalid()), EcsError::None);
	{
		const auto& children = world.GetChildren(parent);
		EXPECT_EQ(children.size(), 1u);
		EXPECT_EQ(children[0], c2);
	}
}

// ===========================================================================
// world_name_component_storage_is_readonly_sparse_set
// ===========================================================================
TEST(World, test_world_name_component_storage_is_readonly_sparse_set)
{
	World world;

	// HasNameComponent is false for Entity::Invalid().
	EXPECT_FALSE(world.HasNameComponent(Entity::Invalid()));

	// Every live entity has a NameComponent.
	Entity e1 = world.CreateEntity("Alpha");
	Entity e2 = world.CreateEntity("Beta");
	EXPECT_TRUE(world.HasNameComponent(e1));
	EXPECT_TRUE(world.HasNameComponent(e2));

	// GetNameComponent returns the correct name.
	EXPECT_EQ(world.GetNameComponent(e1).name, "Alpha");
	EXPECT_EQ(world.GetNameComponent(e2).name, "Beta");

	// GetNameComponentEntities contains both entities.
	const auto& entities = world.GetNameComponentEntities();
	EXPECT_EQ(entities.size(), 2u);
	bool hasE1 = std::find(entities.begin(), entities.end(), e1) != entities.end();
	bool hasE2 = std::find(entities.begin(), entities.end(), e2) != entities.end();
	EXPECT_TRUE(hasE1);
	EXPECT_TRUE(hasE2);

	// After DestroyEntity, HasNameComponent returns false.
	world.DestroyEntity(e1);
	EXPECT_FALSE(world.HasNameComponent(e1));

	// e2 is still present.
	EXPECT_TRUE(world.HasNameComponent(e2));
	const auto& entitiesAfter = world.GetNameComponentEntities();
	EXPECT_EQ(entitiesAfter.size(), 1u);
}

// ===========================================================================
// world_create_entity_resolves_name_collision_with_dot_suffix
// ===========================================================================
TEST(World, test_world_create_entity_resolves_name_collision_with_dot_suffix)
{
	World world;

	// First entity with name "Cube" gets exactly "Cube".
	Entity e1 = world.CreateEntity("Cube");
	EXPECT_EQ(world.GetNameComponent(e1).name, "Cube");

	// Second entity with same name gets "Cube.001".
	Entity e2 = world.CreateEntity("Cube");
	EXPECT_EQ(world.GetNameComponent(e2).name, "Cube.001");

	// Third entity gets "Cube.002".
	Entity e3 = world.CreateEntity("Cube");
	EXPECT_EQ(world.GetNameComponent(e3).name, "Cube.002");

	// Empty name argument defaults to "Entity".
	Entity e4 = world.CreateEntity("");
	EXPECT_EQ(world.GetNameComponent(e4).name, "Entity");

	// Second empty-name entity gets "Entity.001".
	Entity e5 = world.CreateEntity("");
	EXPECT_EQ(world.GetNameComponent(e5).name, "Entity.001");

	// A name that already contains a dot: "foo.001" collides with itself
	// and produces "foo.001.001".
	Entity e6 = world.CreateEntity("foo.001");
	EXPECT_EQ(world.GetNameComponent(e6).name, "foo.001");
	Entity e7 = world.CreateEntity("foo.001");
	EXPECT_EQ(world.GetNameComponent(e7).name, "foo.001.001");

	// Case sensitivity: "cube" does not collide with "Cube".
	Entity e8 = world.CreateEntity("cube");
	EXPECT_EQ(world.GetNameComponent(e8).name, "cube");
}

// ===========================================================================
// world_get_roots_returns_root_entity_list
// ===========================================================================
TEST(World, test_world_get_roots_returns_root_entity_list)
{
	World world;

	// Empty on default construction.
	EXPECT_TRUE(world.GetRoots().empty());

	Entity a = world.CreateEntity("A");
	Entity b = world.CreateEntity("B");
	Entity c = world.CreateEntity("C");

	// All three are roots initially.
	{
		const auto& roots = world.GetRoots();
		EXPECT_EQ(roots.size(), 3u);
	}

	// Reparent b under a: b leaves roots.
	EXPECT_EQ(world.SetParent(b, a), EcsError::None);
	{
		const auto& roots = world.GetRoots();
		EXPECT_EQ(roots.size(), 2u);
		bool bPresent = std::find(roots.begin(), roots.end(), b) != roots.end();
		EXPECT_FALSE(bPresent);
	}

	// Reparent b back to no parent: b re-enters roots.
	EXPECT_EQ(world.SetParent(b, Entity::Invalid()), EcsError::None);
	{
		const auto& roots = world.GetRoots();
		EXPECT_EQ(roots.size(), 3u);
		bool bPresent = std::find(roots.begin(), roots.end(), b) != roots.end();
		EXPECT_TRUE(bPresent);
	}

	// Destroy a root: it leaves roots.
	world.DestroyEntity(c);
	{
		const auto& roots = world.GetRoots();
		EXPECT_EQ(roots.size(), 2u);
		bool cPresent = std::find(roots.begin(), roots.end(), c) != roots.end();
		EXPECT_FALSE(cPresent);
	}
}

// ===========================================================================
// world_find_entity_by_name_returns_first_match_or_invalid
// ===========================================================================
TEST(World, test_world_find_entity_by_name_returns_first_match_or_invalid)
{
	World world;

	// Not found in empty world.
	EXPECT_EQ(world.FindEntityByName("X"), Entity::Invalid());

	Entity e1 = world.CreateEntity("Alpha");
	Entity e2 = world.CreateEntity("Beta");

	// Exact match.
	EXPECT_EQ(world.FindEntityByName("Alpha"), e1);
	EXPECT_EQ(world.FindEntityByName("Beta"),  e2);

	// Case-sensitive: "alpha" does not match "Alpha".
	EXPECT_EQ(world.FindEntityByName("alpha"), Entity::Invalid());

	// After destroy, name is no longer found.
	world.DestroyEntity(e1);
	EXPECT_EQ(world.FindEntityByName("Alpha"), Entity::Invalid());

	// e2 is still findable.
	EXPECT_EQ(world.FindEntityByName("Beta"), e2);

	// Returned entity satisfies IsValid.
	Entity found = world.FindEntityByName("Beta");
	if (found != Entity::Invalid())
		EXPECT_TRUE(world.IsValid(found));
}

// ===========================================================================
// world_iteration_forbids_concurrent_mutation
// ===========================================================================
// This semantic is a contract rule (undefined behavior on violation), not a
// runtime-observable invariant. We verify the positive case: iterating a
// snapshot (GetEntities) while mutating is safe, and iterating a reference
// before mutating is safe.
TEST(World, test_world_iteration_forbids_concurrent_mutation)
{
	World world;

	Entity e1 = world.CreateEntity("E1");
	Entity e2 = world.CreateEntity("E2");
	Transform t{};
	world.Transforms().Add(e1, t);
	world.Transforms().Add(e2, t);

	ComponentId transformId = world.GetSystemId("Transform");

	// GetEntities returns a private copy; mutating after the call is safe.
	std::vector<Entity> snapshot = world.GetEntities({transformId});
	EXPECT_EQ(snapshot.size(), 2u);

	// Mutate after snapshot is taken — no UB.
	Entity e3 = world.CreateEntity("E3");
	world.Transforms().Add(e3, t);

	// Snapshot is unaffected by the mutation.
	EXPECT_EQ(snapshot.size(), 2u);

	// A fresh query reflects the mutation.
	std::vector<Entity> fresh = world.GetEntities({transformId});
	EXPECT_EQ(fresh.size(), 3u);
}

// ===========================================================================
// world_is_not_thread_safe_per_instance
// ===========================================================================
// Thread-safety is a contract rule about concurrent access; we verify the
// positive case: two distinct World instances can be used independently.
TEST(World, test_world_is_not_thread_safe_per_instance)
{
	World w1;
	World w2;

	Entity e1 = w1.CreateEntity("InW1");
	Entity e2 = w2.CreateEntity("InW2");

	// Each world is independent.
	EXPECT_TRUE(w1.IsValid(e1));
	EXPECT_TRUE(w2.IsValid(e2));

	// e1 is not valid in w2 and vice-versa (different worlds; indices may
	// coincide but the contract says distinct worlds are independent).
	// We can only assert that each world's own entity is valid in its own world.
	EXPECT_EQ(w1.GetEntityCount(), 1u);
	EXPECT_EQ(w2.GetEntityCount(), 1u);
}

// ===========================================================================
// world_set_name_renames_entity_with_collision_resolution
// ===========================================================================
TEST(World, test_world_set_name_renames_entity_with_collision_resolution)
{
	World world;

	Entity e1 = world.CreateEntity("Alpha");
	Entity e2 = world.CreateEntity("Beta");

	// SetName to a unique name: stored verbatim.
	world.SetName(e1, "Gamma");
	EXPECT_EQ(world.GetNameComponent(e1).name, "Gamma");

	// FindEntityByName reflects the new name.
	EXPECT_EQ(world.FindEntityByName("Gamma"), e1);

	// Old name is no longer findable.
	EXPECT_EQ(world.FindEntityByName("Alpha"), Entity::Invalid());

	// SetName to the entity's own current name is a no-op (no suffix appended).
	world.SetName(e1, "Gamma");
	EXPECT_EQ(world.GetNameComponent(e1).name, "Gamma");

	// SetName to a name held by another entity: suffix is appended.
	// e2 holds "Beta"; rename e1 to "Beta" -> should become "Beta.001".
	world.SetName(e1, "Beta");
	EXPECT_EQ(world.GetNameComponent(e1).name, "Beta.001");

	// e2 still holds "Beta".
	EXPECT_EQ(world.GetNameComponent(e2).name, "Beta");

	// FindEntityByName resolves the suffixed name to e1.
	EXPECT_EQ(world.FindEntityByName("Beta.001"), e1);
	EXPECT_EQ(world.FindEntityByName("Beta"),     e2);

	// SetName with collision chain: create entities to occupy "Delta",
	// "Delta.001", then rename e1 to "Delta" -> should become "Delta.002".
	Entity e3 = world.CreateEntity("Delta");     // holds "Delta"
	Entity e4 = world.CreateEntity("Delta");     // holds "Delta.001"
	world.SetName(e1, "Delta");
	EXPECT_EQ(world.GetNameComponent(e1).name, "Delta.002");

	// Renaming e3 (which holds "Delta") to its own name is a no-op.
	world.SetName(e3, "Delta");
	EXPECT_EQ(world.GetNameComponent(e3).name, "Delta");

	// After SetName the entity count is unchanged (no add/remove).
	EXPECT_EQ(world.GetEntityCount(), 4u);

	// Every live entity still has exactly one NameComponent.
	EXPECT_TRUE(world.HasNameComponent(e1));
	EXPECT_TRUE(world.HasNameComponent(e2));
	EXPECT_TRUE(world.HasNameComponent(e3));
	EXPECT_TRUE(world.HasNameComponent(e4));
}

// ===========================================================================
// world_resource_store_holds_typed_non_owning_singletons
// ===========================================================================
TEST(World, test_world_resource_store_holds_typed_non_owning_singletons)
{
	struct InputSnapshot
	{
		int frame = 0;
	};

	struct TimingService
	{
		double deltaSeconds = 0.0;
	};

	World world;
	const std::type_index inputType(typeid(InputSnapshot));
	const std::type_index timingType(typeid(TimingService));

	EXPECT_EQ(world.GetResource(inputType), nullptr);
	EXPECT_EQ(world.GetResource(timingType), nullptr);

	InputSnapshot firstInput{1};
	InputSnapshot secondInput{2};
	TimingService timing{0.016};

	world.SetResource(inputType, &firstInput);
	EXPECT_EQ(world.GetResource(inputType), &firstInput);
	EXPECT_EQ(static_cast<InputSnapshot*>(world.GetResource(inputType))->frame, 1);
	EXPECT_EQ(world.GetResource(timingType), nullptr);

	world.SetResource(inputType, &secondInput);
	world.SetResource(timingType, &timing);
	const World& constWorld = world;
	EXPECT_EQ(constWorld.GetResource(inputType), &secondInput);
	EXPECT_EQ(constWorld.GetResource(timingType), &timing);

	world.SetResource(inputType, nullptr);
	EXPECT_EQ(world.GetResource(inputType), nullptr);

	world.RemoveResource(inputType);
	EXPECT_EQ(world.GetResource(inputType), nullptr);
	world.RemoveResource(inputType);
	EXPECT_EQ(world.GetResource(inputType), nullptr);
	EXPECT_EQ(world.GetResource(timingType), &timing);

	World clone = world.Clone();
	EXPECT_EQ(clone.GetResource(inputType), nullptr);
	EXPECT_EQ(clone.GetResource(timingType), nullptr);
	EXPECT_EQ(world.GetResource(timingType), &timing);

	World moved(std::move(world));
	EXPECT_EQ(moved.GetResource(timingType), &timing);

	World moveAssigned;
	moveAssigned.SetResource(inputType, &firstInput);
	moveAssigned = std::move(moved);
	EXPECT_EQ(moveAssigned.GetResource(timingType), &timing);
	EXPECT_EQ(moveAssigned.GetResource(inputType), nullptr);
}

// ===========================================================================
// world_clone_deep_copies_state
// ===========================================================================
TEST(World, test_world_clone_deep_copies_state)
{
	World world;

	Entity root = world.CreateEntity("Root");
	Entity child = world.CreateEntity("Child");
	Entity sibling = world.CreateEntity("Sibling");
	Entity recycled = world.CreateEntity("Recycled");
	world.DestroyEntity(recycled);
	EXPECT_EQ(world.SetParent(child, root), EcsError::None);

	Transform rootLocal;
	rootLocal.translation = Vec3(1.0f, 0.0f, 0.0f);
	Transform childLocal;
	childLocal.translation = Vec3(0.0f, 2.0f, 0.0f);
	world.Transforms().Add(root, rootLocal);
	world.Transforms().Add(child, childLocal);
	world.UpdateTransforms();

	ComponentId meshId = world.GetSystemId("Mesh");
	ComponentId visualId = world.GetSystemId("Visual");
	ASSERT_NE(meshId, kInvalidComponentId);
	ASSERT_NE(visualId, kInvalidComponentId);

	MeshComponent mesh;
	mesh.meshId = 123u;
	world.GetSystem(meshId).AddRaw(root, &mesh);

	VisualComponent visual;
	visual.materialId = 456u;
	world.GetSystem(visualId).AddRaw(child, &visual);

	World clone = world.Clone();

	EXPECT_EQ(clone.GetEntityCount(), world.GetEntityCount());
	EXPECT_TRUE(clone.IsValid(root));
	EXPECT_TRUE(clone.IsValid(child));
	EXPECT_TRUE(clone.IsValid(sibling));
	EXPECT_FALSE(clone.IsValid(recycled));
	EXPECT_EQ(clone.GetParent(root), Entity::Invalid());
	EXPECT_EQ(clone.GetParent(child), root);
	ASSERT_EQ(clone.GetChildren(root).size(), 1u);
	EXPECT_EQ(clone.GetChildren(root)[0], child);
	EXPECT_EQ(clone.FindEntityByName("Root"), root);
	EXPECT_EQ(clone.FindEntityByName("Child"), child);
	EXPECT_EQ(clone.GetNameComponent(root).name, "Root");

	ComponentId transformId = clone.GetSystemId("Transform");
	ASSERT_NE(transformId, kInvalidComponentId);
	EXPECT_EQ(&clone.Transforms(), &clone.GetSystem(transformId));
	EXPECT_NE(&clone.Transforms(), &world.Transforms());
	EXPECT_EQ(clone.GetSystemId("Mesh"), meshId);
	EXPECT_EQ(clone.GetSystemId("Visual"), visualId);
	EXPECT_TRUE(clone.GetSystem(meshId).Has(root));
	EXPECT_TRUE(clone.GetSystem(visualId).Has(child));

	MeshComponent clonedMesh;
	std::memcpy(&clonedMesh, clone.GetSystem(meshId).GetRaw(root), sizeof(MeshComponent));
	EXPECT_EQ(clonedMesh.meshId, 123u);

	VisualComponent clonedVisual;
	std::memcpy(&clonedVisual, clone.GetSystem(visualId).GetRaw(child), sizeof(VisualComponent));
	EXPECT_EQ(clonedVisual.materialId, 456u);

	EXPECT_NEAR(clone.Transforms().GetLocal(root).translation.x, 1.0f, 1e-5f);
	EXPECT_NEAR(clone.Transforms().GetWorld(child)[3][0], 1.0f, 1e-5f);
	EXPECT_NEAR(clone.Transforms().GetWorld(child)[3][1], 2.0f, 1e-5f);

	world.SetName(root, "OriginalRoot");
	world.GetSystem(meshId).Remove(root);
	Transform movedRoot;
	movedRoot.translation = Vec3(10.0f, 0.0f, 0.0f);
	world.Transforms().SetLocal(root, movedRoot);
	world.UpdateTransforms();

	EXPECT_EQ(clone.GetNameComponent(root).name, "Root");
	EXPECT_TRUE(clone.GetSystem(meshId).Has(root));
	EXPECT_NEAR(clone.Transforms().GetLocal(root).translation.x, 1.0f, 1e-5f);

	clone.SetName(child, "CloneChild");
	clone.GetSystem(visualId).Remove(child);
	EXPECT_EQ(world.GetNameComponent(child).name, "Child");
	EXPECT_TRUE(world.GetSystem(visualId).Has(child));
}
