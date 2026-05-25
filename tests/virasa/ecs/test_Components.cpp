#include <gtest/gtest.h>

#include "virasa/ecs/Components.h"
#include "virasa/math/Types.h"

#include <cstdint>
#include <type_traits>

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

TEST(Components, test_directional_light_component_describes_infinite_light)
{
	// Default construction yields the specified default values.
	virasa::ecs::DirectionalLightComponent defaultComp;
	EXPECT_FLOAT_EQ(defaultComp.direction.x, 0.0f);
	EXPECT_FLOAT_EQ(defaultComp.direction.y, 0.0f);
	EXPECT_FLOAT_EQ(defaultComp.direction.z, -1.0f);
	EXPECT_FLOAT_EQ(defaultComp.color.x, 1.0f);
	EXPECT_FLOAT_EQ(defaultComp.color.y, 1.0f);
	EXPECT_FLOAT_EQ(defaultComp.color.z, 1.0f);
	EXPECT_FLOAT_EQ(defaultComp.intensity, 1.0f);

	// Fields can be overwritten independently.
	virasa::ecs::DirectionalLightComponent comp;
	comp.direction = virasa::math::Vec3(0.0f, 1.0f, 0.0f);
	comp.color = virasa::math::Vec3(0.5f, 0.5f, 0.5f);
	comp.intensity = 2.5f;
	EXPECT_FLOAT_EQ(comp.direction.x, 0.0f);
	EXPECT_FLOAT_EQ(comp.direction.y, 1.0f);
	EXPECT_FLOAT_EQ(comp.direction.z, 0.0f);
	EXPECT_FLOAT_EQ(comp.color.x, 0.5f);
	EXPECT_FLOAT_EQ(comp.color.y, 0.5f);
	EXPECT_FLOAT_EQ(comp.color.z, 0.5f);
	EXPECT_FLOAT_EQ(comp.intensity, 2.5f);

	// HDR values (outside [0,1]) are valid.
	comp.color = virasa::math::Vec3(3.0f, 2.0f, 1.5f);
	comp.intensity = 10.0f;
	EXPECT_FLOAT_EQ(comp.color.x, 3.0f);
	EXPECT_FLOAT_EQ(comp.intensity, 10.0f);

	// Copyable.
	virasa::ecs::DirectionalLightComponent copy = comp;
	EXPECT_FLOAT_EQ(copy.intensity, comp.intensity);
	virasa::ecs::DirectionalLightComponent assigned;
	assigned = comp;
	EXPECT_FLOAT_EQ(assigned.intensity, comp.intensity);

	// Movable.
	virasa::ecs::DirectionalLightComponent moved = std::move(copy);
	EXPECT_FLOAT_EQ(moved.intensity, comp.intensity);

	// Trivially destructible.
	EXPECT_TRUE(std::is_trivially_destructible_v<virasa::ecs::DirectionalLightComponent>);

	// Default-constructible via trait.
	EXPECT_TRUE(std::is_default_constructible_v<virasa::ecs::DirectionalLightComponent>);

	// Copy and move traits.
	EXPECT_TRUE(std::is_copy_constructible_v<virasa::ecs::DirectionalLightComponent>);
	EXPECT_TRUE(std::is_copy_assignable_v<virasa::ecs::DirectionalLightComponent>);
	EXPECT_TRUE(std::is_move_constructible_v<virasa::ecs::DirectionalLightComponent>);
	EXPECT_TRUE(std::is_move_assignable_v<virasa::ecs::DirectionalLightComponent>);
}

TEST(Components, test_point_light_component_describes_omnidirectional_light)
{
	// Default construction yields the specified default values.
	virasa::ecs::PointLightComponent defaultComp;
	EXPECT_FLOAT_EQ(defaultComp.color.x, 1.0f);
	EXPECT_FLOAT_EQ(defaultComp.color.y, 1.0f);
	EXPECT_FLOAT_EQ(defaultComp.color.z, 1.0f);
	EXPECT_FLOAT_EQ(defaultComp.intensity, 1.0f);
	EXPECT_FLOAT_EQ(defaultComp.range, 10.0f);

	// Fields can be overwritten independently.
	virasa::ecs::PointLightComponent comp;
	comp.color = virasa::math::Vec3(0.8f, 0.6f, 0.4f);
	comp.intensity = 5.0f;
	comp.range = 25.0f;
	EXPECT_FLOAT_EQ(comp.color.x, 0.8f);
	EXPECT_FLOAT_EQ(comp.color.y, 0.6f);
	EXPECT_FLOAT_EQ(comp.color.z, 0.4f);
	EXPECT_FLOAT_EQ(comp.intensity, 5.0f);
	EXPECT_FLOAT_EQ(comp.range, 25.0f);

	// HDR values (outside [0,1]) are valid.
	comp.color = virasa::math::Vec3(4.0f, 3.0f, 2.0f);
	EXPECT_FLOAT_EQ(comp.color.x, 4.0f);

	// Copyable.
	virasa::ecs::PointLightComponent copy = comp;
	EXPECT_FLOAT_EQ(copy.range, comp.range);
	virasa::ecs::PointLightComponent assigned;
	assigned = comp;
	EXPECT_FLOAT_EQ(assigned.range, comp.range);

	// Movable.
	virasa::ecs::PointLightComponent moved = std::move(copy);
	EXPECT_FLOAT_EQ(moved.range, comp.range);

	// Trivially destructible.
	EXPECT_TRUE(std::is_trivially_destructible_v<virasa::ecs::PointLightComponent>);

	// Default-constructible via trait.
	EXPECT_TRUE(std::is_default_constructible_v<virasa::ecs::PointLightComponent>);

	// Copy and move traits.
	EXPECT_TRUE(std::is_copy_constructible_v<virasa::ecs::PointLightComponent>);
	EXPECT_TRUE(std::is_copy_assignable_v<virasa::ecs::PointLightComponent>);
	EXPECT_TRUE(std::is_move_constructible_v<virasa::ecs::PointLightComponent>);
	EXPECT_TRUE(std::is_move_assignable_v<virasa::ecs::PointLightComponent>);
}

TEST(Components, test_spot_light_component_describes_cone_light)
{
	// Default construction yields the specified default values.
	virasa::ecs::SpotLightComponent defaultComp;
	EXPECT_FLOAT_EQ(defaultComp.color.x, 1.0f);
	EXPECT_FLOAT_EQ(defaultComp.color.y, 1.0f);
	EXPECT_FLOAT_EQ(defaultComp.color.z, 1.0f);
	EXPECT_FLOAT_EQ(defaultComp.intensity, 1.0f);
	EXPECT_FLOAT_EQ(defaultComp.range, 10.0f);
	EXPECT_FLOAT_EQ(defaultComp.innerConeCos, 0.95f);
	EXPECT_FLOAT_EQ(defaultComp.outerConeCos, 0.85f);

	// innerConeCos >= outerConeCos for well-defined falloff (default satisfies this).
	EXPECT_GE(defaultComp.innerConeCos, defaultComp.outerConeCos);

	// Fields can be overwritten independently.
	virasa::ecs::SpotLightComponent comp;
	comp.color = virasa::math::Vec3(1.0f, 0.9f, 0.8f);
	comp.intensity = 3.0f;
	comp.range = 15.0f;
	comp.innerConeCos = 0.90f;
	comp.outerConeCos = 0.75f;
	EXPECT_FLOAT_EQ(comp.color.x, 1.0f);
	EXPECT_FLOAT_EQ(comp.color.y, 0.9f);
	EXPECT_FLOAT_EQ(comp.color.z, 0.8f);
	EXPECT_FLOAT_EQ(comp.intensity, 3.0f);
	EXPECT_FLOAT_EQ(comp.range, 15.0f);
	EXPECT_FLOAT_EQ(comp.innerConeCos, 0.90f);
	EXPECT_FLOAT_EQ(comp.outerConeCos, 0.75f);

	// Well-defined ordering: innerConeCos >= outerConeCos.
	EXPECT_GE(comp.innerConeCos, comp.outerConeCos);

	// HDR values (outside [0,1]) are valid for color.
	comp.color = virasa::math::Vec3(5.0f, 4.0f, 3.0f);
	EXPECT_FLOAT_EQ(comp.color.x, 5.0f);

	// Copyable.
	virasa::ecs::SpotLightComponent copy = comp;
	EXPECT_FLOAT_EQ(copy.innerConeCos, comp.innerConeCos);
	EXPECT_FLOAT_EQ(copy.outerConeCos, comp.outerConeCos);
	virasa::ecs::SpotLightComponent assigned;
	assigned = comp;
	EXPECT_FLOAT_EQ(assigned.innerConeCos, comp.innerConeCos);
	EXPECT_FLOAT_EQ(assigned.outerConeCos, comp.outerConeCos);

	// Movable.
	virasa::ecs::SpotLightComponent moved = std::move(copy);
	EXPECT_FLOAT_EQ(moved.innerConeCos, comp.innerConeCos);

	// Trivially destructible.
	EXPECT_TRUE(std::is_trivially_destructible_v<virasa::ecs::SpotLightComponent>);

	// Default-constructible via trait.
	EXPECT_TRUE(std::is_default_constructible_v<virasa::ecs::SpotLightComponent>);

	// Copy and move traits.
	EXPECT_TRUE(std::is_copy_constructible_v<virasa::ecs::SpotLightComponent>);
	EXPECT_TRUE(std::is_copy_assignable_v<virasa::ecs::SpotLightComponent>);
	EXPECT_TRUE(std::is_move_constructible_v<virasa::ecs::SpotLightComponent>);
	EXPECT_TRUE(std::is_move_assignable_v<virasa::ecs::SpotLightComponent>);
}
