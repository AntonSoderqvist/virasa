#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <gtest/gtest.h>
#include <vector>

#include "virasa/ecs/ComponentSystem.h"
#include "virasa/ecs/Types.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{

// A trivially-copyable component used throughout the tests.
struct Vec3
{
	float x, y, z;
};

// A trivially-copyable component used for second-system tests.
struct IntComp
{
	int value;
};

// Factory helpers.
virasa::ecs::Entity MakeEntity(uint32_t index, uint32_t generation = 1u)
{
	virasa::ecs::Entity e;
	e.index = index;
	e.generation = generation;
	return e;
}

// Concrete SparseComponentSystem for Vec3.
class Vec3System : public virasa::ecs::SparseComponentSystem
{
	public:
	explicit Vec3System(virasa::ecs::ComponentId id = 0u)
	    : virasa::ecs::SparseComponentSystem(id, "Vec3", sizeof(Vec3))
	{
	}
};

// Subclass that records hook calls so we can verify the hooks fire.
class HookedVec3System : public virasa::ecs::SparseComponentSystem
{
	public:
	explicit HookedVec3System(virasa::ecs::ComponentId id = 0u)
	    : virasa::ecs::SparseComponentSystem(id, "HookedVec3", sizeof(Vec3))
	{
	}

	struct OnAddCall
	{
		uint32_t denseIndex;
	};
	struct OnRemoveSwapCall
	{
		uint32_t removedDenseIndex;
		uint32_t lastDenseIndex;
	};
	struct OnSetCall
	{
		virasa::ecs::Entity entity;
	};

	std::vector<OnAddCall> onAddCalls;
	std::vector<OnRemoveSwapCall> onRemoveSwapCalls;
	std::vector<OnSetCall> onSetCalls;

	protected:
	void OnAdd(uint32_t denseIndex) override
	{
		onAddCalls.push_back({denseIndex});
	}

	void OnRemoveSwap(uint32_t removedDenseIndex, uint32_t lastDenseIndex) override
	{
		onRemoveSwapCalls.push_back({removedDenseIndex, lastDenseIndex});
	}

	void OnSet(virasa::ecs::Entity entity) override
	{
		onSetCalls.push_back({entity});
	}
};

} // namespace

// ===========================================================================
// component_id_is_runtime_registration_handle
// ===========================================================================
TEST(SparseComponentSystem, test_component_id_is_runtime_registration_handle)
{
	// kInvalidComponentId must equal 0xFFFFFFFFu and be of type ComponentId (uint32_t).
	static_assert(
		std::is_same_v<virasa::ecs::ComponentId, uint32_t>, "ComponentId must be uint32_t");
	EXPECT_EQ(virasa::ecs::kInvalidComponentId, 0xFFFFFFFFu);

	// A system constructed with a given id returns that id from Id().
	Vec3System sysA(42u);
	EXPECT_EQ(sysA.Id(), 42u);

	Vec3System sysB(0u);
	EXPECT_EQ(sysB.Id(), 0u);

	// kInvalidComponentId is never a valid system id — verify it is distinct from
	// any id a World would assign (0-based dense ids).
	EXPECT_NE(virasa::ecs::kInvalidComponentId, 0u);
}

// ===========================================================================
// component_system_is_abstract_type_erased_interface
// ===========================================================================
TEST(SparseComponentSystem, test_component_system_is_abstract_type_erased_interface)
{
	// SparseComponentSystem is a concrete subclass of ComponentSystem.
	// We can hold it through a ComponentSystem pointer.
	std::unique_ptr<virasa::ecs::ComponentSystem> sys = std::make_unique<Vec3System>(7u);

	// Identity methods.
	EXPECT_EQ(sys->Id(), 7u);
	EXPECT_STREQ(sys->Name(), "Vec3");

	// Initially empty.
	EXPECT_EQ(sys->Size(), 0u);
	EXPECT_TRUE(sys->Entities().empty());
	EXPECT_TRUE(sys->Dirty().empty());

	// Has() returns false for an entity not yet added.
	virasa::ecs::Entity e = MakeEntity(0u);
	EXPECT_FALSE(sys->Has(e));

	// AddRaw returns a non-null pointer.
	Vec3 v{1.0f, 2.0f, 3.0f};
	void* ptr = sys->AddRaw(e, &v);
	ASSERT_NE(ptr, nullptr);
	EXPECT_TRUE(sys->Has(e));
	EXPECT_EQ(sys->Size(), 1u);

	// GetRaw returns the stored bytes.
	const void* raw = sys->GetRaw(e);
	ASSERT_NE(raw, nullptr);
	Vec3 got;
	std::memcpy(&got, raw, sizeof(Vec3));
	EXPECT_FLOAT_EQ(got.x, 1.0f);
	EXPECT_FLOAT_EQ(got.y, 2.0f);
	EXPECT_FLOAT_EQ(got.z, 3.0f);

	// SetRaw overwrites the value.
	Vec3 v2{9.0f, 8.0f, 7.0f};
	sys->SetRaw(e, &v2);
	const void* raw2 = sys->GetRaw(e);
	Vec3 got2;
	std::memcpy(&got2, raw2, sizeof(Vec3));
	EXPECT_FLOAT_EQ(got2.x, 9.0f);
	EXPECT_FLOAT_EQ(got2.y, 8.0f);
	EXPECT_FLOAT_EQ(got2.z, 7.0f);

	// ForEachRaw visits the entity.
	int visitCount = 0;
	sys->ForEachRaw(
		[&](virasa::ecs::Entity visited, const void* bytes)
		{
			++visitCount;
			EXPECT_EQ(visited, e);
			Vec3 fv;
			std::memcpy(&fv, bytes, sizeof(Vec3));
			EXPECT_FLOAT_EQ(fv.x, 9.0f);
		});
	EXPECT_EQ(visitCount, 1);

	// Remove erases the entity.
	sys->Remove(e);
	EXPECT_FALSE(sys->Has(e));
	EXPECT_EQ(sys->Size(), 0u);

	// Update() on a plain SparseComponentSystem is a no-op (must not crash).
	sys->Update();
}

// ===========================================================================
// component_system_dirty_lifecycle_owned_by_system
// ===========================================================================
TEST(SparseComponentSystem, test_component_system_dirty_lifecycle_owned_by_system)
{
	Vec3System sys(1u);

	virasa::ecs::Entity e0 = MakeEntity(0u);
	virasa::ecs::Entity e1 = MakeEntity(1u);

	Vec3 v{0.f, 0.f, 0.f};
	sys.AddRaw(e0, &v);
	sys.AddRaw(e1, &v);

	// AddRaw marks entities dirty.
	const auto& dirty = sys.Dirty();
	EXPECT_EQ(dirty.size(), 2u);

	// Reading Dirty() does not clear it.
	EXPECT_EQ(sys.Dirty().size(), 2u);

	// ClearDirty removes exactly one entity.
	sys.ClearDirty(e0);
	EXPECT_EQ(sys.Dirty().size(), 1u);

	// SetRaw on e0 re-adds it to dirty.
	Vec3 v2{1.f, 2.f, 3.f};
	sys.SetRaw(e0, &v2);
	EXPECT_EQ(sys.Dirty().size(), 2u);

	// An entity appears at most once in the dirty set even after multiple SetRaw calls.
	sys.SetRaw(e0, &v);
	sys.SetRaw(e0, &v2);
	size_t countE0 = 0u;
	for (const auto& de : sys.Dirty())
		if (de == e0)
			++countE0;
	EXPECT_EQ(countE0, 1u);

	// ClearAllDirty empties the set.
	sys.ClearAllDirty();
	EXPECT_TRUE(sys.Dirty().empty());

	// After clearing, SetRaw re-adds to dirty.
	sys.SetRaw(e1, &v2);
	EXPECT_EQ(sys.Dirty().size(), 1u);
	EXPECT_EQ(sys.Dirty()[0], e1);
}

// ===========================================================================
// sparse_component_system_is_byte_backed_sparse_set
// ===========================================================================
TEST(SparseComponentSystem, test_sparse_component_system_is_byte_backed_sparse_set)
{
	Vec3System sys(2u);

	EXPECT_STREQ(sys.Name(), "Vec3");
	EXPECT_EQ(sys.Id(), 2u);
	EXPECT_EQ(sys.Size(), 0u);
	EXPECT_TRUE(sys.Entities().empty());

	virasa::ecs::Entity e0 = MakeEntity(0u);
	virasa::ecs::Entity e1 = MakeEntity(1u);
	virasa::ecs::Entity e2 = MakeEntity(2u);

	Vec3 v0{1.f, 0.f, 0.f};
	Vec3 v1{0.f, 1.f, 0.f};
	Vec3 v2{0.f, 0.f, 1.f};

	sys.AddRaw(e0, &v0);
	sys.AddRaw(e1, &v1);
	sys.AddRaw(e2, &v2);

	EXPECT_EQ(sys.Size(), 3u);
	EXPECT_TRUE(sys.Has(e0));
	EXPECT_TRUE(sys.Has(e1));
	EXPECT_TRUE(sys.Has(e2));

	// Entities() contains all three.
	const auto& entities = sys.Entities();
	EXPECT_EQ(entities.size(), 3u);

	// GetRaw returns correct bytes for each entity.
	Vec3 got0, got1, got2;
	std::memcpy(&got0, sys.GetRaw(e0), sizeof(Vec3));
	std::memcpy(&got1, sys.GetRaw(e1), sizeof(Vec3));
	std::memcpy(&got2, sys.GetRaw(e2), sizeof(Vec3));
	EXPECT_FLOAT_EQ(got0.x, 1.f);
	EXPECT_FLOAT_EQ(got1.y, 1.f);
	EXPECT_FLOAT_EQ(got2.z, 1.f);

	// Has() returns false for an entity not added.
	virasa::ecs::Entity eOther = MakeEntity(99u);
	EXPECT_FALSE(sys.Has(eOther));
}

// ===========================================================================
// sparse_component_system_add_and_set_mark_dirty
// ===========================================================================
TEST(SparseComponentSystem, test_sparse_component_system_add_and_set_mark_dirty)
{
	HookedVec3System sys(3u);

	virasa::ecs::Entity e0 = MakeEntity(0u);
	virasa::ecs::Entity e1 = MakeEntity(1u);

	Vec3 v0{1.f, 2.f, 3.f};
	Vec3 v1{4.f, 5.f, 6.f};

	// AddRaw: returns non-null, marks dirty, invokes OnAdd with the dense index.
	void* ptr0 = sys.AddRaw(e0, &v0);
	ASSERT_NE(ptr0, nullptr);
	EXPECT_TRUE(sys.Has(e0));
	EXPECT_EQ(sys.Size(), 1u);

	// OnAdd called with dense index 0.
	ASSERT_EQ(sys.onAddCalls.size(), 1u);
	EXPECT_EQ(sys.onAddCalls[0].denseIndex, 0u);

	// e0 is in dirty.
	{
		bool found = false;
		for (const auto& de : sys.Dirty())
			if (de == e0)
				found = true;
		EXPECT_TRUE(found);
	}

	void* ptr1 = sys.AddRaw(e1, &v1);
	ASSERT_NE(ptr1, nullptr);
	ASSERT_EQ(sys.onAddCalls.size(), 2u);
	EXPECT_EQ(sys.onAddCalls[1].denseIndex, 1u);

	// SetRaw: overwrites value, marks dirty, invokes OnSet.
	sys.ClearAllDirty();
	sys.onSetCalls.clear();

	Vec3 newV{9.f, 8.f, 7.f};
	sys.SetRaw(e0, &newV);

	// Value is immediately visible via GetRaw (write-through).
	Vec3 readBack;
	std::memcpy(&readBack, sys.GetRaw(e0), sizeof(Vec3));
	EXPECT_FLOAT_EQ(readBack.x, 9.f);
	EXPECT_FLOAT_EQ(readBack.y, 8.f);
	EXPECT_FLOAT_EQ(readBack.z, 7.f);

	// e0 is dirty again.
	{
		bool found = false;
		for (const auto& de : sys.Dirty())
			if (de == e0)
				found = true;
		EXPECT_TRUE(found);
	}

	// OnSet called with e0.
	ASSERT_EQ(sys.onSetCalls.size(), 1u);
	EXPECT_EQ(sys.onSetCalls[0].entity, e0);

	// Multiple SetRaw calls on same entity: entity appears at most once in dirty.
	sys.SetRaw(e0, &v0);
	sys.SetRaw(e0, &newV);
	size_t countE0 = 0u;
	for (const auto& de : sys.Dirty())
		if (de == e0)
			++countE0;
	EXPECT_EQ(countE0, 1u);
}

// ===========================================================================
// sparse_component_system_remove_is_swap_pop
// ===========================================================================
TEST(SparseComponentSystem, test_sparse_component_system_remove_is_swap_pop)
{
	HookedVec3System sys(4u);

	virasa::ecs::Entity e0 = MakeEntity(0u);
	virasa::ecs::Entity e1 = MakeEntity(1u);
	virasa::ecs::Entity e2 = MakeEntity(2u);

	Vec3 v0{1.f, 0.f, 0.f};
	Vec3 v1{0.f, 1.f, 0.f};
	Vec3 v2{0.f, 0.f, 1.f};

	sys.AddRaw(e0, &v0);
	sys.AddRaw(e1, &v1);
	sys.AddRaw(e2, &v2);
	sys.ClearAllDirty();
	sys.onRemoveSwapCalls.clear();

	// Remove e0 (not the last): swap-and-pop should move e2 into slot 0.
	sys.Remove(e0);

	EXPECT_FALSE(sys.Has(e0));
	EXPECT_EQ(sys.Size(), 2u);

	// OnRemoveSwap called with removedDenseIndex=0, lastDenseIndex=2.
	ASSERT_EQ(sys.onRemoveSwapCalls.size(), 1u);
	EXPECT_EQ(sys.onRemoveSwapCalls[0].removedDenseIndex, 0u);
	EXPECT_EQ(sys.onRemoveSwapCalls[0].lastDenseIndex, 2u);

	// e2 is still accessible with its original value.
	EXPECT_TRUE(sys.Has(e2));
	Vec3 got2;
	std::memcpy(&got2, sys.GetRaw(e2), sizeof(Vec3));
	EXPECT_FLOAT_EQ(got2.z, 1.f);

	// e1 is still accessible.
	EXPECT_TRUE(sys.Has(e1));

	// e0 is not in dirty after removal.
	for (const auto& de : sys.Dirty())
		EXPECT_NE(de, e0);

	// Remove the last element (no swap needed): OnRemoveSwap not called again
	// (or called with equal indices — implementation detail; what matters is
	// the entity is gone and size decreases).
	size_t prevSwapCalls = sys.onRemoveSwapCalls.size();
	sys.Remove(e1);
	EXPECT_FALSE(sys.Has(e1));
	EXPECT_EQ(sys.Size(), 1u);

	// Update() is a no-op on plain SparseComponentSystem — must not crash.
	sys.Update();

	// ForEachRaw visits remaining entity.
	int visitCount = 0;
	sys.ForEachRaw(
		[&](virasa::ecs::Entity visited, const void*)
		{
			++visitCount;
			EXPECT_EQ(visited, e2);
		});
	EXPECT_EQ(visitCount, 1);

	// ClearDirty and ClearAllDirty work after removals.
	sys.ClearAllDirty();
	EXPECT_TRUE(sys.Dirty().empty());
}

// ===========================================================================
// sparse_component_system_provides_subclass_hooks
// ===========================================================================
TEST(SparseComponentSystem, test_sparse_component_system_provides_subclass_hooks)
{
	// Verify that a subclass with overridden hooks receives the correct calls
	// and can use MarkDirty to flag additional entities.

	class ExtendedSystem : public virasa::ecs::SparseComponentSystem
	{
		public:
		explicit ExtendedSystem()
		    : virasa::ecs::SparseComponentSystem(10u, "Extended", sizeof(Vec3))
		{
		}

		std::vector<uint32_t> addedDenseIndices;
		std::vector<std::pair<uint32_t, uint32_t>> swapCalls;
		std::vector<virasa::ecs::Entity> setCalls;

		// Expose MarkDirty to tests via a public wrapper.
		void PublicMarkDirty(virasa::ecs::Entity e)
		{
			MarkDirty(e);
		}

		protected:
		void OnAdd(uint32_t denseIndex) override
		{
			addedDenseIndices.push_back(denseIndex);
		}
		void OnRemoveSwap(uint32_t removed, uint32_t last) override
		{
			swapCalls.emplace_back(removed, last);
		}
		void OnSet(virasa::ecs::Entity entity) override
		{
			setCalls.push_back(entity);
		}
	};

	ExtendedSystem sys;

	virasa::ecs::Entity e0 = MakeEntity(0u);
	virasa::ecs::Entity e1 = MakeEntity(1u);
	virasa::ecs::Entity e2 = MakeEntity(2u);

	Vec3 v{0.f, 0.f, 0.f};

	// OnAdd fires for each AddRaw.
	sys.AddRaw(e0, &v);
	sys.AddRaw(e1, &v);
	sys.AddRaw(e2, &v);
	EXPECT_EQ(sys.addedDenseIndices.size(), 3u);
	EXPECT_EQ(sys.addedDenseIndices[0], 0u);
	EXPECT_EQ(sys.addedDenseIndices[1], 1u);
	EXPECT_EQ(sys.addedDenseIndices[2], 2u);

	// OnSet fires for SetRaw.
	Vec3 v2{1.f, 1.f, 1.f};
	sys.SetRaw(e1, &v2);
	ASSERT_EQ(sys.setCalls.size(), 1u);
	EXPECT_EQ(sys.setCalls[0], e1);

	// OnRemoveSwap fires for a non-last removal.
	sys.Remove(e0);
	ASSERT_EQ(sys.swapCalls.size(), 1u);
	EXPECT_EQ(sys.swapCalls[0].first, 0u);  // removedDenseIndex
	EXPECT_EQ(sys.swapCalls[0].second, 2u); // lastDenseIndex

	// MarkDirty adds an entity to the dirty set.
	sys.ClearAllDirty();
	sys.PublicMarkDirty(e1);
	{
		bool found = false;
		for (const auto& de : sys.Dirty())
			if (de == e1)
				found = true;
		EXPECT_TRUE(found);
	}

	// Manipulating through ComponentSystem* still dispatches to subclass hooks.
	ExtendedSystem sys2;
	virasa::ecs::ComponentSystem* base = &sys2;
	base->AddRaw(e0, &v);
	EXPECT_EQ(sys2.addedDenseIndices.size(), 1u);
	base->SetRaw(e0, &v2);
	EXPECT_EQ(sys2.setCalls.size(), 1u);
	base->AddRaw(e1, &v);
	base->Remove(e0);
	EXPECT_EQ(sys2.swapCalls.size(), 1u);
}
