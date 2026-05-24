#include <cstdint>
#include <gtest/gtest.h>
#include <type_traits>

#include "virasa/ecs/Components.h"

TEST(Components, test_mesh_component_holds_mesh_registry_id)
{
	// Default construction yields sentinel value 0xFFFFFFFFu.
	virasa::ecs::MeshComponent defaultComp;
	EXPECT_EQ(defaultComp.meshId, 0xFFFFFFFFu);

	// meshId can be overwritten with a registry-returned id.
	virasa::ecs::MeshComponent comp;
	comp.meshId = 42u;
	EXPECT_EQ(comp.meshId, 42u);

	// Copyable: copy-construction and copy-assignment preserve meshId.
	virasa::ecs::MeshComponent copy = comp;
	EXPECT_EQ(copy.meshId, 42u);
	virasa::ecs::MeshComponent assigned;
	assigned = comp;
	EXPECT_EQ(assigned.meshId, 42u);

	// Movable: move-construction preserves meshId.
	virasa::ecs::MeshComponent moved = std::move(copy);
	EXPECT_EQ(moved.meshId, 42u);

	// Size and alignment are exactly four bytes.
	EXPECT_EQ(sizeof(virasa::ecs::MeshComponent), 4u);
	EXPECT_EQ(alignof(virasa::ecs::MeshComponent), 4u);

	// Trivially destructible.
	EXPECT_TRUE(std::is_trivially_destructible_v<virasa::ecs::MeshComponent>);

	// Default-constructible via trait.
	EXPECT_TRUE(std::is_default_constructible_v<virasa::ecs::MeshComponent>);

	// Copy and move traits.
	EXPECT_TRUE(std::is_copy_constructible_v<virasa::ecs::MeshComponent>);
	EXPECT_TRUE(std::is_copy_assignable_v<virasa::ecs::MeshComponent>);
	EXPECT_TRUE(std::is_move_constructible_v<virasa::ecs::MeshComponent>);
	EXPECT_TRUE(std::is_move_assignable_v<virasa::ecs::MeshComponent>);
}

TEST(Components, test_visual_component_holds_visual_material_id)
{
	// Default construction yields sentinel value 0xFFFFFFFFu.
	virasa::ecs::VisualComponent defaultComp;
	EXPECT_EQ(defaultComp.materialId, 0xFFFFFFFFu);

	// materialId can be overwritten with a table-returned id.
	virasa::ecs::VisualComponent comp;
	comp.materialId = 7u;
	EXPECT_EQ(comp.materialId, 7u);

	// Copyable: copy-construction and copy-assignment preserve materialId.
	virasa::ecs::VisualComponent copy = comp;
	EXPECT_EQ(copy.materialId, 7u);
	virasa::ecs::VisualComponent assigned;
	assigned = comp;
	EXPECT_EQ(assigned.materialId, 7u);

	// Movable: move-construction preserves materialId.
	virasa::ecs::VisualComponent moved = std::move(copy);
	EXPECT_EQ(moved.materialId, 7u);

	// Size and alignment are exactly four bytes.
	EXPECT_EQ(sizeof(virasa::ecs::VisualComponent), 4u);
	EXPECT_EQ(alignof(virasa::ecs::VisualComponent), 4u);

	// Trivially destructible.
	EXPECT_TRUE(std::is_trivially_destructible_v<virasa::ecs::VisualComponent>);

	// Default-constructible via trait.
	EXPECT_TRUE(std::is_default_constructible_v<virasa::ecs::VisualComponent>);

	// Copy and move traits.
	EXPECT_TRUE(std::is_copy_constructible_v<virasa::ecs::VisualComponent>);
	EXPECT_TRUE(std::is_copy_assignable_v<virasa::ecs::VisualComponent>);
	EXPECT_TRUE(std::is_move_constructible_v<virasa::ecs::VisualComponent>);
	EXPECT_TRUE(std::is_move_assignable_v<virasa::ecs::VisualComponent>);
}
