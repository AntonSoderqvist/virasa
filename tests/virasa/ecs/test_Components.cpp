#include <gtest/gtest.h>

#include "virasa/ecs/Components.h"
#include "virasa/math/Types.h"
#include "virasa/renderer/Types.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

TEST(Components, test_name_component_holds_owned_utf8_string)
{
	virasa::ecs::NameComponent defaultComp;
	EXPECT_TRUE(defaultComp.name.empty());
	EXPECT_TRUE(std::is_default_constructible_v<virasa::ecs::NameComponent>);
	EXPECT_TRUE(std::is_copy_constructible_v<virasa::ecs::NameComponent>);
	EXPECT_TRUE(std::is_copy_assignable_v<virasa::ecs::NameComponent>);
	EXPECT_TRUE(std::is_move_constructible_v<virasa::ecs::NameComponent>);
	EXPECT_TRUE(std::is_move_assignable_v<virasa::ecs::NameComponent>);

	const std::string utf8Name = reinterpret_cast<const char*>(u8"Player 🚀 Καλημέρα");
	virasa::ecs::NameComponent comp;
	comp.name = utf8Name;
	EXPECT_EQ(comp.name, utf8Name);
	EXPECT_EQ(comp.name.size(), utf8Name.size());

	virasa::ecs::NameComponent sameBytes;
	sameBytes.name = utf8Name;
	EXPECT_EQ(comp.name, sameBytes.name);

	virasa::ecs::NameComponent copy = comp;
	ASSERT_EQ(copy.name, utf8Name);
	ASSERT_FALSE(copy.name.empty());
	EXPECT_NE(copy.name.data(), comp.name.data());

	comp.name[0] = 'p';
	EXPECT_EQ(copy.name, utf8Name);
	EXPECT_NE(copy.name, comp.name);

	virasa::ecs::NameComponent moved = std::move(copy);
	EXPECT_EQ(moved.name, utf8Name);
	EXPECT_TRUE(copy.name.empty());
}

TEST(Components, test_mesh_component_holds_mesh_registry_id)
{
	virasa::ecs::MeshComponent defaultComp;
	EXPECT_EQ(defaultComp.meshId, 0xFFFFFFFFu);

	virasa::ecs::MeshComponent comp;
	comp.meshId = 42u;
	EXPECT_EQ(comp.meshId, 42u);
	EXPECT_NE(comp.meshId, 0xFFFFFFFFu);

	virasa::ecs::MeshComponent copy = comp;
	EXPECT_EQ(copy.meshId, 42u);
	virasa::ecs::MeshComponent assigned;
	assigned = comp;
	EXPECT_EQ(assigned.meshId, 42u);

	virasa::ecs::MeshComponent moved = std::move(copy);
	EXPECT_EQ(moved.meshId, 42u);

	EXPECT_EQ(sizeof(virasa::ecs::MeshComponent), sizeof(uint32_t));
	EXPECT_EQ(sizeof(virasa::ecs::MeshComponent), 4u);
	EXPECT_EQ(alignof(virasa::ecs::MeshComponent), alignof(uint32_t));
	EXPECT_EQ(alignof(virasa::ecs::MeshComponent), 4u);
	EXPECT_TRUE(std::is_trivially_destructible_v<virasa::ecs::MeshComponent>);
	EXPECT_TRUE(std::is_default_constructible_v<virasa::ecs::MeshComponent>);
	EXPECT_TRUE(std::is_copy_constructible_v<virasa::ecs::MeshComponent>);
	EXPECT_TRUE(std::is_copy_assignable_v<virasa::ecs::MeshComponent>);
	EXPECT_TRUE(std::is_move_constructible_v<virasa::ecs::MeshComponent>);
	EXPECT_TRUE(std::is_move_assignable_v<virasa::ecs::MeshComponent>);
	EXPECT_TRUE(std::is_standard_layout_v<virasa::ecs::MeshComponent>);
	EXPECT_EQ(offsetof(virasa::ecs::MeshComponent, meshId), 0u);
}

TEST(Components, test_visual_component_holds_visual_material_id)
{
	virasa::ecs::VisualComponent defaultComp;
	EXPECT_EQ(defaultComp.materialId, 0xFFFFFFFFu);

	virasa::ecs::VisualComponent comp;
	comp.materialId = 7u;
	EXPECT_EQ(comp.materialId, 7u);
	EXPECT_NE(comp.materialId, 0xFFFFFFFFu);

	virasa::ecs::VisualComponent copy = comp;
	EXPECT_EQ(copy.materialId, 7u);
	virasa::ecs::VisualComponent assigned;
	assigned = comp;
	EXPECT_EQ(assigned.materialId, 7u);

	virasa::ecs::VisualComponent moved = std::move(copy);
	EXPECT_EQ(moved.materialId, 7u);

	EXPECT_EQ(sizeof(virasa::ecs::VisualComponent), sizeof(uint32_t));
	EXPECT_EQ(sizeof(virasa::ecs::VisualComponent), 4u);
	EXPECT_EQ(alignof(virasa::ecs::VisualComponent), alignof(uint32_t));
	EXPECT_EQ(alignof(virasa::ecs::VisualComponent), 4u);
	EXPECT_TRUE(std::is_trivially_destructible_v<virasa::ecs::VisualComponent>);
	EXPECT_TRUE(std::is_default_constructible_v<virasa::ecs::VisualComponent>);
	EXPECT_TRUE(std::is_copy_constructible_v<virasa::ecs::VisualComponent>);
	EXPECT_TRUE(std::is_copy_assignable_v<virasa::ecs::VisualComponent>);
	EXPECT_TRUE(std::is_move_constructible_v<virasa::ecs::VisualComponent>);
	EXPECT_TRUE(std::is_move_assignable_v<virasa::ecs::VisualComponent>);
	EXPECT_TRUE(std::is_standard_layout_v<virasa::ecs::VisualComponent>);
	EXPECT_EQ(offsetof(virasa::ecs::VisualComponent, materialId), 0u);
}

TEST(Components, test_directional_light_component_describes_infinite_light)
{
	virasa::ecs::DirectionalLightComponent defaultComp;
	EXPECT_FLOAT_EQ(defaultComp.direction.x, 0.0f);
	EXPECT_FLOAT_EQ(defaultComp.direction.y, 0.0f);
	EXPECT_FLOAT_EQ(defaultComp.direction.z, -1.0f);
	EXPECT_FLOAT_EQ(defaultComp.color.x, 1.0f);
	EXPECT_FLOAT_EQ(defaultComp.color.y, 1.0f);
	EXPECT_FLOAT_EQ(defaultComp.color.z, 1.0f);
	EXPECT_FLOAT_EQ(defaultComp.intensity, 1.0f);
	EXPECT_TRUE(defaultComp.castsShadows);

	virasa::ecs::DirectionalLightComponent comp;
	comp.direction = virasa::math::Vec3(3.0f, 4.0f, 0.0f);
	comp.color = virasa::math::Vec3(2.5f, 0.5f, 4.0f);
	comp.intensity = 2.5f;
	comp.castsShadows = false;
	EXPECT_FLOAT_EQ(comp.direction.x, 3.0f);
	EXPECT_FLOAT_EQ(comp.direction.y, 4.0f);
	EXPECT_FLOAT_EQ(comp.direction.z, 0.0f);
	EXPECT_FLOAT_EQ(comp.color.x, 2.5f);
	EXPECT_FLOAT_EQ(comp.color.y, 0.5f);
	EXPECT_FLOAT_EQ(comp.color.z, 4.0f);
	EXPECT_FLOAT_EQ(comp.intensity, 2.5f);
	EXPECT_FALSE(comp.castsShadows);

	virasa::ecs::DirectionalLightComponent copy = comp;
	EXPECT_FLOAT_EQ(copy.direction.x, comp.direction.x);
	EXPECT_FLOAT_EQ(copy.direction.y, comp.direction.y);
	EXPECT_FLOAT_EQ(copy.direction.z, comp.direction.z);
	EXPECT_FLOAT_EQ(copy.color.x, comp.color.x);
	EXPECT_FLOAT_EQ(copy.color.y, comp.color.y);
	EXPECT_FLOAT_EQ(copy.color.z, comp.color.z);
	EXPECT_FLOAT_EQ(copy.intensity, comp.intensity);
	EXPECT_EQ(copy.castsShadows, comp.castsShadows);

	virasa::ecs::DirectionalLightComponent assigned;
	assigned = comp;
	EXPECT_FLOAT_EQ(assigned.direction.x, comp.direction.x);
	EXPECT_FLOAT_EQ(assigned.direction.y, comp.direction.y);
	EXPECT_FLOAT_EQ(assigned.direction.z, comp.direction.z);
	EXPECT_FLOAT_EQ(assigned.color.x, comp.color.x);
	EXPECT_FLOAT_EQ(assigned.color.y, comp.color.y);
	EXPECT_FLOAT_EQ(assigned.color.z, comp.color.z);
	EXPECT_FLOAT_EQ(assigned.intensity, comp.intensity);
	EXPECT_EQ(assigned.castsShadows, comp.castsShadows);

	virasa::ecs::DirectionalLightComponent moved = std::move(copy);
	EXPECT_FLOAT_EQ(moved.direction.x, comp.direction.x);
	EXPECT_FLOAT_EQ(moved.direction.y, comp.direction.y);
	EXPECT_FLOAT_EQ(moved.direction.z, comp.direction.z);
	EXPECT_FLOAT_EQ(moved.color.x, comp.color.x);
	EXPECT_FLOAT_EQ(moved.color.y, comp.color.y);
	EXPECT_FLOAT_EQ(moved.color.z, comp.color.z);
	EXPECT_FLOAT_EQ(moved.intensity, comp.intensity);
	EXPECT_EQ(moved.castsShadows, comp.castsShadows);

	EXPECT_TRUE(std::is_trivially_destructible_v<virasa::ecs::DirectionalLightComponent>);
	EXPECT_TRUE(std::is_default_constructible_v<virasa::ecs::DirectionalLightComponent>);
	EXPECT_TRUE(std::is_copy_constructible_v<virasa::ecs::DirectionalLightComponent>);
	EXPECT_TRUE(std::is_copy_assignable_v<virasa::ecs::DirectionalLightComponent>);
	EXPECT_TRUE(std::is_move_constructible_v<virasa::ecs::DirectionalLightComponent>);
	EXPECT_TRUE(std::is_move_assignable_v<virasa::ecs::DirectionalLightComponent>);
}
TEST(Components, test_point_light_component_describes_omnidirectional_light)
{
	virasa::ecs::PointLightComponent defaultComp;
	EXPECT_FLOAT_EQ(defaultComp.color.x, 1.0f);
	EXPECT_FLOAT_EQ(defaultComp.color.y, 1.0f);
	EXPECT_FLOAT_EQ(defaultComp.color.z, 1.0f);
	EXPECT_FLOAT_EQ(defaultComp.intensity, 1.0f);
	EXPECT_FLOAT_EQ(defaultComp.range, 10.0f);

	virasa::ecs::PointLightComponent comp;
	comp.color = virasa::math::Vec3(4.0f, 3.0f, 2.0f);
	comp.intensity = 5.0f;
	comp.range = 25.0f;
	EXPECT_FLOAT_EQ(comp.color.x, 4.0f);
	EXPECT_FLOAT_EQ(comp.color.y, 3.0f);
	EXPECT_FLOAT_EQ(comp.color.z, 2.0f);
	EXPECT_FLOAT_EQ(comp.intensity, 5.0f);
	EXPECT_FLOAT_EQ(comp.range, 25.0f);

	virasa::ecs::PointLightComponent copy = comp;
	EXPECT_FLOAT_EQ(copy.color.x, comp.color.x);
	EXPECT_FLOAT_EQ(copy.color.y, comp.color.y);
	EXPECT_FLOAT_EQ(copy.color.z, comp.color.z);
	EXPECT_FLOAT_EQ(copy.intensity, comp.intensity);
	EXPECT_FLOAT_EQ(copy.range, comp.range);

	virasa::ecs::PointLightComponent assigned;
	assigned = comp;
	EXPECT_FLOAT_EQ(assigned.color.x, comp.color.x);
	EXPECT_FLOAT_EQ(assigned.color.y, comp.color.y);
	EXPECT_FLOAT_EQ(assigned.color.z, comp.color.z);
	EXPECT_FLOAT_EQ(assigned.intensity, comp.intensity);
	EXPECT_FLOAT_EQ(assigned.range, comp.range);

	virasa::ecs::PointLightComponent moved = std::move(copy);
	EXPECT_FLOAT_EQ(moved.color.x, comp.color.x);
	EXPECT_FLOAT_EQ(moved.color.y, comp.color.y);
	EXPECT_FLOAT_EQ(moved.color.z, comp.color.z);
	EXPECT_FLOAT_EQ(moved.intensity, comp.intensity);
	EXPECT_FLOAT_EQ(moved.range, comp.range);

	EXPECT_TRUE(std::is_trivially_destructible_v<virasa::ecs::PointLightComponent>);
	EXPECT_TRUE(std::is_default_constructible_v<virasa::ecs::PointLightComponent>);
	EXPECT_TRUE(std::is_copy_constructible_v<virasa::ecs::PointLightComponent>);
	EXPECT_TRUE(std::is_copy_assignable_v<virasa::ecs::PointLightComponent>);
	EXPECT_TRUE(std::is_move_constructible_v<virasa::ecs::PointLightComponent>);
	EXPECT_TRUE(std::is_move_assignable_v<virasa::ecs::PointLightComponent>);
}

TEST(Components, test_spot_light_component_describes_cone_light)
{
	virasa::ecs::SpotLightComponent defaultComp;
	EXPECT_FLOAT_EQ(defaultComp.color.x, 1.0f);
	EXPECT_FLOAT_EQ(defaultComp.color.y, 1.0f);
	EXPECT_FLOAT_EQ(defaultComp.color.z, 1.0f);
	EXPECT_FLOAT_EQ(defaultComp.intensity, 1.0f);
	EXPECT_FLOAT_EQ(defaultComp.range, 10.0f);
	EXPECT_FLOAT_EQ(defaultComp.innerConeCos, 0.95f);
	EXPECT_FLOAT_EQ(defaultComp.outerConeCos, 0.85f);
	EXPECT_GE(defaultComp.innerConeCos, defaultComp.outerConeCos);
	EXPECT_TRUE(defaultComp.castsShadows);

	virasa::ecs::SpotLightComponent comp;
	comp.color = virasa::math::Vec3(5.0f, 4.0f, 3.0f);
	comp.intensity = 3.0f;
	comp.range = 15.0f;
	comp.innerConeCos = 0.90f;
	comp.outerConeCos = 0.75f;
	comp.castsShadows = false;
	EXPECT_FLOAT_EQ(comp.color.x, 5.0f);
	EXPECT_FLOAT_EQ(comp.color.y, 4.0f);
	EXPECT_FLOAT_EQ(comp.color.z, 3.0f);
	EXPECT_FLOAT_EQ(comp.intensity, 3.0f);
	EXPECT_FLOAT_EQ(comp.range, 15.0f);
	EXPECT_FLOAT_EQ(comp.innerConeCos, 0.90f);
	EXPECT_FLOAT_EQ(comp.outerConeCos, 0.75f);
	EXPECT_GE(comp.innerConeCos, comp.outerConeCos);
	EXPECT_FALSE(comp.castsShadows);

	virasa::ecs::SpotLightComponent copy = comp;
	EXPECT_FLOAT_EQ(copy.color.x, comp.color.x);
	EXPECT_FLOAT_EQ(copy.color.y, comp.color.y);
	EXPECT_FLOAT_EQ(copy.color.z, comp.color.z);
	EXPECT_FLOAT_EQ(copy.intensity, comp.intensity);
	EXPECT_FLOAT_EQ(copy.range, comp.range);
	EXPECT_FLOAT_EQ(copy.innerConeCos, comp.innerConeCos);
	EXPECT_FLOAT_EQ(copy.outerConeCos, comp.outerConeCos);
	EXPECT_EQ(copy.castsShadows, comp.castsShadows);

	virasa::ecs::SpotLightComponent assigned;
	assigned = comp;
	EXPECT_FLOAT_EQ(assigned.color.x, comp.color.x);
	EXPECT_FLOAT_EQ(assigned.color.y, comp.color.y);
	EXPECT_FLOAT_EQ(assigned.color.z, comp.color.z);
	EXPECT_FLOAT_EQ(assigned.intensity, comp.intensity);
	EXPECT_FLOAT_EQ(assigned.range, comp.range);
	EXPECT_FLOAT_EQ(assigned.innerConeCos, comp.innerConeCos);
	EXPECT_FLOAT_EQ(assigned.outerConeCos, comp.outerConeCos);
	EXPECT_EQ(assigned.castsShadows, comp.castsShadows);

	virasa::ecs::SpotLightComponent moved = std::move(copy);
	EXPECT_FLOAT_EQ(moved.color.x, comp.color.x);
	EXPECT_FLOAT_EQ(moved.color.y, comp.color.y);
	EXPECT_FLOAT_EQ(moved.color.z, comp.color.z);
	EXPECT_FLOAT_EQ(moved.intensity, comp.intensity);
	EXPECT_FLOAT_EQ(moved.range, comp.range);
	EXPECT_FLOAT_EQ(moved.innerConeCos, comp.innerConeCos);
	EXPECT_FLOAT_EQ(moved.outerConeCos, comp.outerConeCos);
	EXPECT_EQ(moved.castsShadows, comp.castsShadows);

	EXPECT_TRUE(std::is_trivially_destructible_v<virasa::ecs::SpotLightComponent>);
	EXPECT_TRUE(std::is_default_constructible_v<virasa::ecs::SpotLightComponent>);
	EXPECT_TRUE(std::is_copy_constructible_v<virasa::ecs::SpotLightComponent>);
	EXPECT_TRUE(std::is_copy_assignable_v<virasa::ecs::SpotLightComponent>);
	EXPECT_TRUE(std::is_move_constructible_v<virasa::ecs::SpotLightComponent>);
	EXPECT_TRUE(std::is_move_assignable_v<virasa::ecs::SpotLightComponent>);
}

TEST(Components, test_camera_component_describes_viewpoint)
{
	virasa::ecs::CameraComponent defaultComp;
	EXPECT_EQ(defaultComp.domain, virasa::CameraDomain::Main);
	EXPECT_FLOAT_EQ(defaultComp.fovY, 0.7853981633974483f);
	EXPECT_FLOAT_EQ(defaultComp.aspect, 0.0f);
	EXPECT_FLOAT_EQ(defaultComp.nearPlane, 0.1f);
	EXPECT_FLOAT_EQ(defaultComp.farPlane, 1000.0f);
	EXPECT_GE(defaultComp.aspect, 0.0f);
	EXPECT_GT(defaultComp.nearPlane, 0.0f);
	EXPECT_GT(defaultComp.farPlane, 0.0f);
	EXPECT_LT(defaultComp.nearPlane, defaultComp.farPlane);
	EXPECT_EQ(static_cast<std::underlying_type_t<virasa::CameraDomain>>(defaultComp.domain), 0u);

	virasa::ecs::CameraComponent comp;
	comp.domain = virasa::CameraDomain::Editor;
	comp.fovY = 1.2f;
	comp.aspect = 16.0f / 9.0f;
	comp.nearPlane = 0.25f;
	comp.farPlane = 250.0f;
	EXPECT_EQ(comp.domain, virasa::CameraDomain::Editor);
	EXPECT_FLOAT_EQ(comp.fovY, 1.2f);
	EXPECT_FLOAT_EQ(comp.aspect, 16.0f / 9.0f);
	EXPECT_FLOAT_EQ(comp.nearPlane, 0.25f);
	EXPECT_FLOAT_EQ(comp.farPlane, 250.0f);
	EXPECT_GT(comp.aspect, 0.0f);
	EXPECT_GT(comp.nearPlane, 0.0f);
	EXPECT_GT(comp.farPlane, 0.0f);
	EXPECT_LT(comp.nearPlane, comp.farPlane);

	virasa::ecs::CameraComponent copy = comp;
	EXPECT_EQ(copy.domain, comp.domain);
	EXPECT_FLOAT_EQ(copy.fovY, comp.fovY);
	EXPECT_FLOAT_EQ(copy.aspect, comp.aspect);
	EXPECT_FLOAT_EQ(copy.nearPlane, comp.nearPlane);
	EXPECT_FLOAT_EQ(copy.farPlane, comp.farPlane);

	virasa::ecs::CameraComponent assigned;
	assigned = comp;
	EXPECT_EQ(assigned.domain, comp.domain);
	EXPECT_FLOAT_EQ(assigned.fovY, comp.fovY);
	EXPECT_FLOAT_EQ(assigned.aspect, comp.aspect);
	EXPECT_FLOAT_EQ(assigned.nearPlane, comp.nearPlane);
	EXPECT_FLOAT_EQ(assigned.farPlane, comp.farPlane);

	virasa::ecs::CameraComponent moved = std::move(copy);
	EXPECT_EQ(moved.domain, comp.domain);
	EXPECT_FLOAT_EQ(moved.fovY, comp.fovY);
	EXPECT_FLOAT_EQ(moved.aspect, comp.aspect);
	EXPECT_FLOAT_EQ(moved.nearPlane, comp.nearPlane);
	EXPECT_FLOAT_EQ(moved.farPlane, comp.farPlane);

	EXPECT_TRUE(std::is_trivially_destructible_v<virasa::ecs::CameraComponent>);
	EXPECT_TRUE(std::is_default_constructible_v<virasa::ecs::CameraComponent>);
	EXPECT_TRUE(std::is_copy_constructible_v<virasa::ecs::CameraComponent>);
	EXPECT_TRUE(std::is_copy_assignable_v<virasa::ecs::CameraComponent>);
	EXPECT_TRUE(std::is_move_constructible_v<virasa::ecs::CameraComponent>);
	EXPECT_TRUE(std::is_move_assignable_v<virasa::ecs::CameraComponent>);
}
