#include <cstdint>
#include <gtest/gtest.h>
#include <utility>

#include "virasa/renderer/resources/Mesh.h"
#include "virasa/renderer/resources/MeshRegistry.h"

using namespace virasa;
using namespace virasa::renderer;

// ---------------------------------------------------------------------------
// Helper: produce a default-constructed (non-initialized) Mesh.
// The MeshRegistry contract explicitly allows allocating non-initialized
// Meshes; the registry simply owns whatever state the Mesh is in.
// ---------------------------------------------------------------------------
static Mesh MakeDefaultMesh()
{
	return Mesh{};
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST(MeshRegistry, test_mesh_registry_is_raii_movable_non_copyable)
{
	// MeshRegistry must be default-constructible.
	MeshRegistry reg;
	EXPECT_EQ(reg.GetSlotCount(), 0u);
	EXPECT_EQ(reg.GetAllocatedCount(), 0u);

	// Allocate one mesh so the registry owns something.
	uint32_t id = reg.Allocate(MakeDefaultMesh());
	EXPECT_NE(id, 0xFFFFFFFFu);
	EXPECT_EQ(reg.GetSlotCount(), 1u);
	EXPECT_EQ(reg.GetAllocatedCount(), 1u);

	// Move construction transfers ownership.
	MeshRegistry moved(std::move(reg));
	EXPECT_EQ(moved.GetSlotCount(), 1u);
	EXPECT_EQ(moved.GetAllocatedCount(), 1u);
	EXPECT_TRUE(moved.IsAllocated(id));

	// Source is left in default-constructed state.
	EXPECT_EQ(reg.GetSlotCount(), 0u);
	EXPECT_EQ(reg.GetAllocatedCount(), 0u);

	// Move assignment: assign into a registry that already owns slots.
	MeshRegistry target;
	uint32_t targetId = target.Allocate(MakeDefaultMesh());
	EXPECT_NE(targetId, 0xFFFFFFFFu);
	EXPECT_EQ(target.GetSlotCount(), 1u);

	target = std::move(moved);
	// target now owns moved's slots.
	EXPECT_EQ(target.GetSlotCount(), 1u);
	EXPECT_EQ(target.GetAllocatedCount(), 1u);
	EXPECT_TRUE(target.IsAllocated(id));

	// moved (source of move-assignment) is now in default-constructed state.
	EXPECT_EQ(moved.GetSlotCount(), 0u);
	EXPECT_EQ(moved.GetAllocatedCount(), 0u);

	// Verify non-copyable: static assertions (compile-time).
	static_assert(!std::is_copy_constructible_v<MeshRegistry>,
		"MeshRegistry must not be copy-constructible");
	static_assert(
		!std::is_copy_assignable_v<MeshRegistry>, "MeshRegistry must not be copy-assignable");

	// Verify movable.
	static_assert(
		std::is_move_constructible_v<MeshRegistry>, "MeshRegistry must be move-constructible");
	static_assert(
		std::is_move_assignable_v<MeshRegistry>, "MeshRegistry must be move-assignable");

	// Destroying a moved-from MeshRegistry is well-defined (no crash).
	// reg and moved go out of scope here; if the destructor is well-behaved
	// the test will not crash or sanitizer-fault.
}

TEST(MeshRegistry, test_mesh_registry_default_constructed_state)
{
	MeshRegistry reg;

	EXPECT_EQ(reg.GetSlotCount(), 0u);
	EXPECT_EQ(reg.GetAllocatedCount(), 0u);

	// IsAllocated must return false for any id on a default-constructed registry.
	EXPECT_FALSE(reg.IsAllocated(0u));
	EXPECT_FALSE(reg.IsAllocated(1u));
	EXPECT_FALSE(reg.IsAllocated(0xFFFFFFFFu));
	EXPECT_FALSE(reg.IsAllocated(0xFFFFFFFEu));

	// No Initialize method exists; the registry is immediately usable.
	// Allocate should work right after default construction.
	uint32_t id = reg.Allocate(MakeDefaultMesh());
	EXPECT_NE(id, 0xFFFFFFFFu);
	EXPECT_EQ(id, 0u); // First allocation on empty registry must be id 0.
}

TEST(MeshRegistry, test_mesh_registry_allocate_takes_ownership_and_returns_id)
{
	MeshRegistry reg;

	// First allocation must return 0 (no prior Free calls).
	Mesh mesh0 = MakeDefaultMesh();
	uint32_t id0 = reg.Allocate(std::move(mesh0));
	EXPECT_EQ(id0, 0u);
	EXPECT_TRUE(reg.IsAllocated(id0));
	EXPECT_EQ(reg.GetAllocatedCount(), 1u);
	EXPECT_EQ(reg.GetSlotCount(), 1u);

	// Second allocation must return 1.
	uint32_t id1 = reg.Allocate(MakeDefaultMesh());
	EXPECT_EQ(id1, 1u);
	EXPECT_TRUE(reg.IsAllocated(id1));
	EXPECT_EQ(reg.GetAllocatedCount(), 2u);
	EXPECT_EQ(reg.GetSlotCount(), 2u);

	// Third allocation must return 2.
	uint32_t id2 = reg.Allocate(MakeDefaultMesh());
	EXPECT_EQ(id2, 2u);
	EXPECT_EQ(reg.GetAllocatedCount(), 3u);
	EXPECT_EQ(reg.GetSlotCount(), 3u);

	// Free id1, then allocate again — must reuse id1 (last-freed-first-allocated).
	reg.Free(id1);
	EXPECT_EQ(reg.GetAllocatedCount(), 2u);
	EXPECT_EQ(reg.GetSlotCount(), 3u); // Slot count must not shrink.

	uint32_t reusedId = reg.Allocate(MakeDefaultMesh());
	EXPECT_EQ(reusedId, id1); // Slot reuse.
	EXPECT_TRUE(reg.IsAllocated(reusedId));
	EXPECT_EQ(reg.GetAllocatedCount(), 3u);
	EXPECT_EQ(reg.GetSlotCount(), 3u); // Still 3; no new slot was added.

	// Get must return a const reference to the Mesh in the allocated slot.
	// For a default-constructed (non-initialized) Mesh, IsInitialized is false.
	const Mesh& ref = reg.Get(id0);
	EXPECT_FALSE(ref.IsInitialized());
	EXPECT_EQ(ref.GetIndexCount(), 0u);
}

TEST(MeshRegistry, test_mesh_registry_free_returns_slot_to_freelist)
{
	MeshRegistry reg;

	uint32_t id0 = reg.Allocate(MakeDefaultMesh());
	uint32_t id1 = reg.Allocate(MakeDefaultMesh());
	uint32_t id2 = reg.Allocate(MakeDefaultMesh());

	EXPECT_EQ(reg.GetSlotCount(), 3u);
	EXPECT_EQ(reg.GetAllocatedCount(), 3u);

	// Free id0.
	reg.Free(id0);
	EXPECT_FALSE(reg.IsAllocated(id0));
	EXPECT_EQ(reg.GetAllocatedCount(), 2u);
	EXPECT_EQ(reg.GetSlotCount(), 3u); // Slot count must not shrink.

	// Free id2.
	reg.Free(id2);
	EXPECT_FALSE(reg.IsAllocated(id2));
	EXPECT_EQ(reg.GetAllocatedCount(), 1u);
	EXPECT_EQ(reg.GetSlotCount(), 3u);

	// id1 is still allocated.
	EXPECT_TRUE(reg.IsAllocated(id1));

	// Next Allocate reuses id2 (last freed = last on free-list stack).
	uint32_t reused = reg.Allocate(MakeDefaultMesh());
	EXPECT_EQ(reused, id2);
	EXPECT_TRUE(reg.IsAllocated(reused));
	EXPECT_EQ(reg.GetAllocatedCount(), 2u);
	EXPECT_EQ(reg.GetSlotCount(), 3u);

	// Next Allocate reuses id0.
	uint32_t reused2 = reg.Allocate(MakeDefaultMesh());
	EXPECT_EQ(reused2, id0);
	EXPECT_TRUE(reg.IsAllocated(reused2));
	EXPECT_EQ(reg.GetAllocatedCount(), 3u);
	EXPECT_EQ(reg.GetSlotCount(), 3u);
}

TEST(MeshRegistry, test_mesh_registry_get_returns_const_reference)
{
	MeshRegistry reg;

	uint32_t id = reg.Allocate(MakeDefaultMesh());
	ASSERT_NE(id, 0xFFFFFFFFu);
	ASSERT_TRUE(reg.IsAllocated(id));

	// Get must be callable on a const MeshRegistry.
	const MeshRegistry& cReg = reg;
	const Mesh& ref = cReg.Get(id);

	// For a default-constructed Mesh, IsInitialized is false and GetIndexCount is 0.
	EXPECT_FALSE(ref.IsInitialized());
	EXPECT_EQ(ref.GetIndexCount(), 0u);

	// The reference must be stable across a second Get call for the same id.
	const Mesh& ref2 = cReg.Get(id);
	EXPECT_EQ(&ref, &ref2);

	// Allocate a second mesh and verify Get for the second id is independent.
	uint32_t id2 = reg.Allocate(MakeDefaultMesh());
	ASSERT_NE(id2, 0xFFFFFFFFu);
	const Mesh& ref3 = cReg.Get(id2);
	EXPECT_NE(&ref3, &ref); // Different slots → different addresses.
	EXPECT_FALSE(ref3.IsInitialized());
	EXPECT_EQ(ref3.GetIndexCount(), 0u);
}

TEST(MeshRegistry, test_mesh_registry_observers)
{
	MeshRegistry reg;

	// Default-constructed state.
	EXPECT_EQ(reg.GetSlotCount(), 0u);
	EXPECT_EQ(reg.GetAllocatedCount(), 0u);
	EXPECT_FALSE(reg.IsAllocated(0u));
	EXPECT_FALSE(reg.IsAllocated(0xFFFFFFFFu));

	// After one Allocate.
	uint32_t id0 = reg.Allocate(MakeDefaultMesh());
	EXPECT_EQ(reg.GetSlotCount(), 1u);
	EXPECT_EQ(reg.GetAllocatedCount(), 1u);
	EXPECT_TRUE(reg.IsAllocated(id0));
	EXPECT_FALSE(reg.IsAllocated(1u));		  // Out-of-range.
	EXPECT_FALSE(reg.IsAllocated(0xFFFFFFFFu)); // Sentinel.

	// After second Allocate.
	uint32_t id1 = reg.Allocate(MakeDefaultMesh());
	EXPECT_EQ(reg.GetSlotCount(), 2u);
	EXPECT_EQ(reg.GetAllocatedCount(), 2u);
	EXPECT_TRUE(reg.IsAllocated(id1));

	// After Free.
	reg.Free(id0);
	EXPECT_EQ(reg.GetSlotCount(), 2u); // Slot count never decreases.
	EXPECT_EQ(reg.GetAllocatedCount(), 1u);
	EXPECT_FALSE(reg.IsAllocated(id0));
	EXPECT_TRUE(reg.IsAllocated(id1));

	// GetAllocatedCount == GetSlotCount - free-list size.
	// After one Free, free-list has 1 entry, so allocated = 2 - 1 = 1.
	EXPECT_EQ(reg.GetAllocatedCount(), reg.GetSlotCount() - 1u);

	// After move-from, source reports 0 for both observers.
	MeshRegistry other(std::move(reg));
	EXPECT_EQ(reg.GetSlotCount(), 0u);
	EXPECT_EQ(reg.GetAllocatedCount(), 0u);
	EXPECT_FALSE(reg.IsAllocated(0u));

	// Destination has the slots.
	EXPECT_EQ(other.GetSlotCount(), 2u);
	EXPECT_EQ(other.GetAllocatedCount(), 1u);
}

// Thread-safety is a documentation/contract property; we verify the
// observable part: concurrent reads on the same const MeshRegistry are
// allowed. We do a basic smoke test with two threads reading observers.
#include <atomic>
#include <thread>

TEST(MeshRegistry, test_mesh_registry_is_not_thread_safe_per_instance)
{
	// The contract states that const observers MAY be called concurrently
	// with each other. We verify this does not crash or produce obviously
	// wrong results under light concurrent read load.
	MeshRegistry reg;
	reg.Allocate(MakeDefaultMesh());
	reg.Allocate(MakeDefaultMesh());

	const MeshRegistry& cReg = reg;

	std::atomic<bool> start{false};
	std::atomic<uint32_t> slotCountA{0};
	std::atomic<uint32_t> slotCountB{0};

	std::thread tA(
		[&]
		{
			while (!start.load(std::memory_order_acquire))
			{
			}
			slotCountA.store(cReg.GetSlotCount(), std::memory_order_relaxed);
		});

	std::thread tB(
		[&]
		{
			while (!start.load(std::memory_order_acquire))
			{
			}
			slotCountB.store(cReg.GetAllocatedCount(), std::memory_order_relaxed);
		});

	start.store(true, std::memory_order_release);
	tA.join();
	tB.join();

	EXPECT_EQ(slotCountA.load(), 2u);
	EXPECT_EQ(slotCountB.load(), 2u);
}
