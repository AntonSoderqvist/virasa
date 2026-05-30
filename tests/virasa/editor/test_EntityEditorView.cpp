#include <gtest/gtest.h>

#include "virasa/editor/EntityEditorView.h"
#include "virasa/ui/EntityEditorPanel.h"
#include "virasa/ui/Types.h"
#include "virasa/ui/FontAtlas.h"
#include "virasa/ecs/ComponentAccess.h"
#include "virasa/ecs/World.h"
#include "virasa/ecs/Components.h"
#include "virasa/math/Transform.h"
#include "virasa/renderer/Types.h"
#include "virasa/window/Events.h"

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>

namespace
{

using virasa::editor::EntityEditorView;
using virasa::ecs::CameraComponent;
using virasa::ecs::DirectionalLightComponent;
using virasa::ecs::Entity;
using virasa::ecs::MeshComponent;
using virasa::ecs::PointLightComponent;
using virasa::ecs::SpotLightComponent;
using virasa::ecs::VisualComponent;
using virasa::ecs::World;
using virasa::editor::EntityEditorViewKeyResult;
using virasa::math::Transform;
using virasa::CameraDomain;
using virasa::ui::DrawList;
using virasa::ui::EntityEditorPanel;
using virasa::ui::EntityEditorPanelConfig;
using virasa::ui::FontAtlas;
using virasa::ui::TextCommand;
using virasa::KeyCode;

constexpr float kX = 10.0f;
constexpr float kY = 20.0f;
constexpr float kWidth = 320.0f;
constexpr float kHeight = 240.0f;

std::string_view ExtractText(const DrawList& out, const TextCommand& command)
{
    const std::string_view buffer = out.GetTextBuffer();
    return buffer.substr(command.textOffset, command.textLength);
}

} // namespace

TEST(EntityEditorView, test_entity_editor_view_owns_panel_cursor_collapsed_and_edit_state)
{
    static_assert(std::is_default_constructible_v<EntityEditorView>);
    static_assert(std::is_copy_constructible_v<EntityEditorView>);
    static_assert(std::is_copy_assignable_v<EntityEditorView>);
    static_assert(std::is_move_constructible_v<EntityEditorView>);
    static_assert(std::is_move_assignable_v<EntityEditorView>);
    static_assert(std::is_final_v<EntityEditorView>);

    EntityEditorView view;
    const EntityEditorPanelConfig defaultConfig;

    EXPECT_EQ(&view.GetPanel(), &std::as_const(view).GetPanel());
    EXPECT_FLOAT_EQ(view.GetPanel().GetConfig().background.r, defaultConfig.background.r);
    EXPECT_FLOAT_EQ(view.GetPanel().GetConfig().background.g, defaultConfig.background.g);
    EXPECT_FLOAT_EQ(view.GetPanel().GetConfig().background.b, defaultConfig.background.b);
    EXPECT_FLOAT_EQ(view.GetPanel().GetConfig().background.a, defaultConfig.background.a);
    EXPECT_EQ(view.GetCursorRow(), 0u);
    EXPECT_EQ(view.GetCursorCell(), 0u);
    EXPECT_FALSE(view.IsEditing());

    EntityEditorPanelConfig updatedConfig = view.GetPanel().GetConfig();
    updatedConfig.paddingX = 17.0f;
    updatedConfig.paddingY = 9.0f;
    view.GetPanel().SetConfig(updatedConfig);

    World world;
    const Entity entity = world.CreateEntity("OwnedState");
    world.Transforms().Add(entity, Transform{});

    EXPECT_EQ(view.HandleTextInput("j", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.GetCursorRow(), 1u);
    EXPECT_EQ(view.GetCursorCell(), 0u);

    EXPECT_EQ(view.HandleTextInput("j", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.GetCursorCell(), 2u);

    EXPECT_EQ(view.HandleTextInput("i", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_TRUE(view.IsEditing());

    EntityEditorView copied = view;
    EXPECT_FLOAT_EQ(copied.GetPanel().GetConfig().paddingX, 17.0f);
    EXPECT_FLOAT_EQ(copied.GetPanel().GetConfig().paddingY, 9.0f);
    EXPECT_EQ(copied.GetCursorRow(), 2u);
    EXPECT_EQ(copied.GetCursorCell(), 2u);
    EXPECT_TRUE(copied.IsEditing());

    EntityEditorView moved = std::move(view);
    EXPECT_FLOAT_EQ(moved.GetPanel().GetConfig().paddingX, 17.0f);
    EXPECT_FLOAT_EQ(moved.GetPanel().GetConfig().paddingY, 9.0f);
    EXPECT_EQ(moved.GetCursorRow(), 2u);
    EXPECT_EQ(moved.GetCursorCell(), 2u);
    EXPECT_TRUE(moved.IsEditing());
}

TEST(EntityEditorView, test_entity_editor_view_key_result_enum_values_in_declared_order)
{
    static_assert(std::is_same_v<std::underlying_type_t<EntityEditorViewKeyResult>, uint8_t>);

    EXPECT_EQ(static_cast<uint8_t>(EntityEditorViewKeyResult::Consumed), 0u);
    EXPECT_EQ(static_cast<uint8_t>(EntityEditorViewKeyResult::RequestHierarchy), 1u);
    EXPECT_EQ(static_cast<uint8_t>(EntityEditorViewKeyResult::RequestCommandBar), 2u);
}

TEST(EntityEditorView, test_get_panel_returns_owned_entity_editor_panel)
{
    EntityEditorView view;

    EntityEditorPanel& panel = view.GetPanel();
    const EntityEditorView& constView = view;
    const EntityEditorPanel& constPanel = constView.GetPanel();

    EXPECT_EQ(&panel, &constPanel);

    EntityEditorPanelConfig config = panel.GetConfig();
    config.paddingY = 9.0f;
    config.indentX = 33.0f;
    panel.SetConfig(config);

    EXPECT_FLOAT_EQ(view.GetPanel().GetConfig().paddingY, 9.0f);
    EXPECT_FLOAT_EQ(constView.GetPanel().GetConfig().indentX, 33.0f);
}

TEST(EntityEditorView, test_component_section_order_is_fixed)
{
    EntityEditorView view;
    DrawList out;
    World world;
    FontAtlas atlas;

    const Entity entity = world.CreateEntity("OrderedEntity");

    virasa::ecs::AddCamera(world, entity, CameraComponent{});
    virasa::ecs::AddSpotLight(world, entity, SpotLightComponent{});
    virasa::ecs::AddPointLight(world, entity, PointLightComponent{});
    virasa::ecs::AddDirectionalLight(world, entity, DirectionalLightComponent{});
    virasa::ecs::AddVisual(world, entity, VisualComponent{});
    virasa::ecs::AddMesh(world, entity, MeshComponent{});
    world.Transforms().Add(entity, Transform{});

    view.Render(out, world, entity, atlas, kX, kY, kWidth, kHeight);

    const auto texts = out.GetTexts();
    ASSERT_EQ(texts.size(), 86u);

    EXPECT_EQ(ExtractText(out, texts[2]), "Transform");
    EXPECT_EQ(ExtractText(out, texts[24]), "Mesh");
    EXPECT_EQ(ExtractText(out, texts[27]), "Visual");
    EXPECT_EQ(ExtractText(out, texts[30]), "DirectionalLight");
    EXPECT_EQ(ExtractText(out, texts[47]), "PointLight");
    EXPECT_EQ(ExtractText(out, texts[59]), "SpotLight");
    EXPECT_EQ(ExtractText(out, texts[75]), "Camera");
}

TEST(EntityEditorView, test_cell_layout_per_field_is_pinned)
{
    EntityEditorView view;
    DrawList out;
    World world;
    FontAtlas atlas;

    const Entity entity = world.CreateEntity("LayoutEntity");

    Transform transform{};
    transform.translation = virasa::math::Vec3(1.0f, 2.0f, 3.0f);
    transform.rotation = virasa::math::Quat(1.0f, 0.0f, 0.0f, 0.0f);
    transform.scale = virasa::math::Vec3(4.0f, 5.0f, 6.0f);
    world.Transforms().Add(entity, transform);
    virasa::ecs::AddMesh(world, entity, MeshComponent{42u});
    virasa::ecs::AddVisual(world, entity, VisualComponent{99u});

    DirectionalLightComponent directional{};
    directional.direction = virasa::math::Vec3(7.0f, 8.0f, 9.0f);
    directional.color = virasa::math::Vec3(0.1f, 0.2f, 0.3f);
    directional.intensity = 1.5f;
    virasa::ecs::AddDirectionalLight(world, entity, directional);

    PointLightComponent point{};
    point.color = virasa::math::Vec3(0.4f, 0.5f, 0.6f);
    point.intensity = 2.5f;
    point.range = 12.0f;
    virasa::ecs::AddPointLight(world, entity, point);

    SpotLightComponent spot{};
    spot.color = virasa::math::Vec3(0.7f, 0.8f, 0.9f);
    spot.intensity = 3.5f;
    spot.range = 13.0f;
    spot.innerConeCos = 0.95f;
    spot.outerConeCos = 0.85f;
    virasa::ecs::AddSpotLight(world, entity, spot);

    CameraComponent camera{};
    camera.domain = CameraDomain::Editor;
    camera.fovY = 0.5f;
    camera.aspect = 1.25f;
    camera.nearPlane = 0.2f;
    camera.farPlane = 500.0f;
    virasa::ecs::AddCamera(world, entity, camera);

    view.Render(out, world, entity, atlas, kX, kY, kWidth, kHeight);

    const auto texts = out.GetTexts();
    ASSERT_EQ(texts.size(), 86u);

    EXPECT_EQ(ExtractText(out, texts[0]), "Name");
    EXPECT_EQ(ExtractText(out, texts[1]), "LayoutEntity");

    EXPECT_EQ(ExtractText(out, texts[3]), "Translation");
    EXPECT_EQ(ExtractText(out, texts[4]), "X:");
    EXPECT_EQ(ExtractText(out, texts[5]), "1.000");
    EXPECT_EQ(ExtractText(out, texts[6]), "Y:");
    EXPECT_EQ(ExtractText(out, texts[7]), "2.000");
    EXPECT_EQ(ExtractText(out, texts[8]), "Z:");
    EXPECT_EQ(ExtractText(out, texts[9]), "3.000");

    EXPECT_EQ(ExtractText(out, texts[10]), "Rotation");
    EXPECT_EQ(ExtractText(out, texts[11]), "X:");
    EXPECT_EQ(ExtractText(out, texts[12]), "0.000");
    EXPECT_EQ(ExtractText(out, texts[13]), "Y:");
    EXPECT_EQ(ExtractText(out, texts[14]), "0.000");
    EXPECT_EQ(ExtractText(out, texts[15]), "Z:");
    EXPECT_EQ(ExtractText(out, texts[16]), "0.000");

    EXPECT_EQ(ExtractText(out, texts[17]), "Scale");
    EXPECT_EQ(ExtractText(out, texts[18]), "X:");
    EXPECT_EQ(ExtractText(out, texts[19]), "4.000");
    EXPECT_EQ(ExtractText(out, texts[20]), "Y:");
    EXPECT_EQ(ExtractText(out, texts[21]), "5.000");
    EXPECT_EQ(ExtractText(out, texts[22]), "Z:");
    EXPECT_EQ(ExtractText(out, texts[23]), "6.000");

    EXPECT_EQ(ExtractText(out, texts[25]), "Mesh Id");
    EXPECT_EQ(ExtractText(out, texts[26]), "42");
    EXPECT_EQ(ExtractText(out, texts[28]), "Material Id");
    EXPECT_EQ(ExtractText(out, texts[29]), "99");

    EXPECT_EQ(ExtractText(out, texts[31]), "Direction");
    EXPECT_EQ(ExtractText(out, texts[32]), "X:");
    EXPECT_EQ(ExtractText(out, texts[33]), "7.000");
    EXPECT_EQ(ExtractText(out, texts[34]), "Y:");
    EXPECT_EQ(ExtractText(out, texts[35]), "8.000");
    EXPECT_EQ(ExtractText(out, texts[36]), "Z:");
    EXPECT_EQ(ExtractText(out, texts[37]), "9.000");

    EXPECT_EQ(ExtractText(out, texts[38]), "Color");
    EXPECT_EQ(ExtractText(out, texts[39]), "X:");
    EXPECT_EQ(ExtractText(out, texts[40]), "0.100");
    EXPECT_EQ(ExtractText(out, texts[41]), "Y:");
    EXPECT_EQ(ExtractText(out, texts[42]), "0.200");
    EXPECT_EQ(ExtractText(out, texts[43]), "Z:");
    EXPECT_EQ(ExtractText(out, texts[44]), "0.300");
    EXPECT_EQ(ExtractText(out, texts[45]), "Intensity");
    EXPECT_EQ(ExtractText(out, texts[46]), "1.500");

    EXPECT_EQ(ExtractText(out, texts[48]), "Color");
    EXPECT_EQ(ExtractText(out, texts[55]), "Intensity");
    EXPECT_EQ(ExtractText(out, texts[56]), "2.500");
    EXPECT_EQ(ExtractText(out, texts[57]), "Range");
    EXPECT_EQ(ExtractText(out, texts[58]), "12.000");

    EXPECT_EQ(ExtractText(out, texts[59]), "SpotLight");
    EXPECT_EQ(ExtractText(out, texts[60]), "Color");
    EXPECT_EQ(ExtractText(out, texts[67]), "Intensity");
    EXPECT_EQ(ExtractText(out, texts[68]), "3.500");
    EXPECT_EQ(ExtractText(out, texts[69]), "Range");
    EXPECT_EQ(ExtractText(out, texts[70]), "13.000");
    EXPECT_EQ(ExtractText(out, texts[71]), "Inner Cone Cos");
    EXPECT_EQ(ExtractText(out, texts[72]), "0.950");
    EXPECT_EQ(ExtractText(out, texts[73]), "Outer Cone Cos");
    EXPECT_EQ(ExtractText(out, texts[74]), "0.850");

    EXPECT_EQ(ExtractText(out, texts[76]), "Domain");
    EXPECT_EQ(ExtractText(out, texts[77]), "Editor");
    EXPECT_EQ(ExtractText(out, texts[78]), "Fov Y");
    EXPECT_EQ(ExtractText(out, texts[79]), "0.500");
    EXPECT_EQ(ExtractText(out, texts[80]), "Aspect");
    EXPECT_EQ(ExtractText(out, texts[81]), "1.250");
    EXPECT_EQ(ExtractText(out, texts[82]), "Near Plane");
    EXPECT_EQ(ExtractText(out, texts[83]), "0.200");
    EXPECT_EQ(ExtractText(out, texts[84]), "Far Plane");
    EXPECT_EQ(ExtractText(out, texts[85]), "500.000");
}

TEST(EntityEditorView, test_field_value_formatting_rules)
{
    EntityEditorView view;
    DrawList out;
    World world;
    FontAtlas atlas;

    const Entity entity = world.CreateEntity("FormatEntity");

    Transform transform{};
    transform.translation = virasa::math::Vec3(-0.0f, 1.5f, -0.1f);
    transform.rotation = virasa::math::Quat(glm::vec3(
        glm::radians(0.0f), glm::radians(1.5f), glm::radians(-0.1f)));
    transform.scale = virasa::math::Vec3(2.0f, -0.0f, 3.125f);
    world.Transforms().Add(entity, transform);

    virasa::ecs::AddMesh(world, entity, MeshComponent{7u});
    virasa::ecs::AddVisual(world, entity, VisualComponent{0xFFFFFFFFu});

    DirectionalLightComponent directional{};
    directional.direction = virasa::math::Vec3(-0.0f, 2.0f, -3.25f);
    directional.color = virasa::math::Vec3(4.0f, -0.0f, 5.5f);
    directional.intensity = -0.0f;
    virasa::ecs::AddDirectionalLight(world, entity, directional);

    PointLightComponent point{};
    point.color = virasa::math::Vec3(0.0f, -1.25f, 2.5f);
    point.intensity = 6.0f;
    point.range = -0.0f;
    virasa::ecs::AddPointLight(world, entity, point);

    SpotLightComponent spot{};
    spot.color = virasa::math::Vec3(-0.0f, 0.0f, 9.0f);
    spot.intensity = 1.2349f;
    spot.range = 10.0f;
    spot.innerConeCos = -0.0f;
    spot.outerConeCos = 0.875f;
    virasa::ecs::AddSpotLight(world, entity, spot);

    CameraComponent camera{};
    camera.domain = static_cast<CameraDomain>(7);
    camera.fovY = -0.0f;
    camera.aspect = 1.7777f;
    camera.nearPlane = 0.1f;
    camera.farPlane = 4294967294.0f;
    virasa::ecs::AddCamera(world, entity, camera);

    view.Render(out, world, entity, atlas, kX, kY, kWidth, kHeight);

    const auto texts = out.GetTexts();
    ASSERT_EQ(texts.size(), 86u);

    EXPECT_EQ(ExtractText(out, texts[5]), "0.000");
    EXPECT_EQ(ExtractText(out, texts[7]), "1.500");
    EXPECT_EQ(ExtractText(out, texts[9]), "-0.100");
    EXPECT_EQ(ExtractText(out, texts[12]), "0.000");
    EXPECT_EQ(ExtractText(out, texts[14]), "1.500");
    EXPECT_EQ(ExtractText(out, texts[16]), "-0.100");
    EXPECT_EQ(ExtractText(out, texts[19]), "2.000");
    EXPECT_EQ(ExtractText(out, texts[21]), "0.000");
    EXPECT_EQ(ExtractText(out, texts[23]), "3.125");
    EXPECT_EQ(ExtractText(out, texts[26]), "7");
    EXPECT_EQ(ExtractText(out, texts[29]), "<none>");
    EXPECT_EQ(ExtractText(out, texts[33]), "0.000");
    EXPECT_EQ(ExtractText(out, texts[35]), "2.000");
    EXPECT_EQ(ExtractText(out, texts[37]), "-3.250");
    EXPECT_EQ(ExtractText(out, texts[40]), "4.000");
    EXPECT_EQ(ExtractText(out, texts[42]), "0.000");
    EXPECT_EQ(ExtractText(out, texts[44]), "5.500");
    EXPECT_EQ(ExtractText(out, texts[46]), "0.000");
    EXPECT_EQ(ExtractText(out, texts[50]), "0.000");
    EXPECT_EQ(ExtractText(out, texts[52]), "-1.250");
    EXPECT_EQ(ExtractText(out, texts[54]), "2.500");
    EXPECT_EQ(ExtractText(out, texts[56]), "6.000");
    EXPECT_EQ(ExtractText(out, texts[58]), "0.000");
    EXPECT_EQ(ExtractText(out, texts[62]), "0.000");
    EXPECT_EQ(ExtractText(out, texts[64]), "0.000");
    EXPECT_EQ(ExtractText(out, texts[66]), "9.000");
    EXPECT_EQ(ExtractText(out, texts[68]), "1.235");
    EXPECT_EQ(ExtractText(out, texts[70]), "10.000");
    EXPECT_EQ(ExtractText(out, texts[72]), "0.000");
    EXPECT_EQ(ExtractText(out, texts[74]), "0.875");
    EXPECT_EQ(ExtractText(out, texts[77]), "CameraDomain(7)");
    EXPECT_EQ(ExtractText(out, texts[79]), "0.000");
    EXPECT_EQ(ExtractText(out, texts[81]), "1.778");
    EXPECT_EQ(ExtractText(out, texts[83]), "0.100");
    EXPECT_EQ(ExtractText(out, texts[85]), "4294967296.000");
}

TEST(EntityEditorView, test_render_with_invalid_entity_draws_background_only)
{
    EntityEditorView view;
    World world;
    FontAtlas atlas;

    DrawList invalidOut;
    view.Render(invalidOut, world, Entity::Invalid(), atlas, kX, kY, kWidth, kHeight);

    ASSERT_EQ(invalidOut.GetQuads().size(), 1u);
    EXPECT_EQ(invalidOut.GetTexts().size(), 0u);
    EXPECT_EQ(invalidOut.GetImageQuads().size(), 0u);
    EXPECT_FLOAT_EQ(invalidOut.GetQuads()[0].x, kX);
    EXPECT_FLOAT_EQ(invalidOut.GetQuads()[0].y, kY);
    EXPECT_FLOAT_EQ(invalidOut.GetQuads()[0].width, kWidth);
    EXPECT_FLOAT_EQ(invalidOut.GetQuads()[0].height, kHeight);

    DrawList staleOut;
    const Entity entity = world.CreateEntity("StaleEntity");
    world.DestroyEntity(entity);
    view.Render(staleOut, world, entity, atlas, kX, kY, kWidth, kHeight);

    ASSERT_EQ(staleOut.GetQuads().size(), 1u);
    EXPECT_EQ(staleOut.GetTexts().size(), 0u);
    EXPECT_EQ(staleOut.GetImageQuads().size(), 0u);
    EXPECT_FLOAT_EQ(staleOut.GetQuads()[0].x, kX);
    EXPECT_FLOAT_EQ(staleOut.GetQuads()[0].y, kY);
    EXPECT_FLOAT_EQ(staleOut.GetQuads()[0].width, kWidth);
    EXPECT_FLOAT_EQ(staleOut.GetQuads()[0].height, kHeight);
}

TEST(EntityEditorView, test_render_emits_name_row_then_sections_per_present_component)
{
    EntityEditorView view;
    DrawList out;
    World world;
    FontAtlas atlas;

    const Entity entity = world.CreateEntity("InspectorTarget");

    Transform transform{};
    transform.translation = virasa::math::Vec3(1.0f, 2.0f, 3.0f);
    transform.rotation = virasa::math::Quat(1.0f, 0.0f, 0.0f, 0.0f);
    transform.scale = virasa::math::Vec3(4.0f, 5.0f, 6.0f);
    world.Transforms().Add(entity, transform);
    virasa::ecs::AddMesh(world, entity, MeshComponent{42u});
    virasa::ecs::AddVisual(world, entity, VisualComponent{99u});

    CameraComponent camera{};
    camera.domain = CameraDomain::Editor;
    camera.fovY = 0.5f;
    camera.aspect = 1.25f;
    camera.nearPlane = 0.2f;
    camera.farPlane = 500.0f;
    virasa::ecs::AddCamera(world, entity, camera);

    view.Render(out, world, entity, atlas, kX, kY, kWidth, kHeight);

    ASSERT_EQ(out.GetQuads().size(), 1u);
    const auto texts = out.GetTexts();
    ASSERT_EQ(texts.size(), 41u);

    EXPECT_EQ(ExtractText(out, texts[0]), "Name");
    EXPECT_EQ(ExtractText(out, texts[1]), "InspectorTarget");

    EXPECT_EQ(ExtractText(out, texts[2]), "Transform");
    EXPECT_EQ(ExtractText(out, texts[3]), "Translation");
    EXPECT_EQ(ExtractText(out, texts[5]), "1.000");
    EXPECT_EQ(ExtractText(out, texts[7]), "2.000");
    EXPECT_EQ(ExtractText(out, texts[9]), "3.000");
    EXPECT_EQ(ExtractText(out, texts[10]), "Rotation");
    EXPECT_EQ(ExtractText(out, texts[12]), "0.000");
    EXPECT_EQ(ExtractText(out, texts[14]), "0.000");
    EXPECT_EQ(ExtractText(out, texts[16]), "0.000");
    EXPECT_EQ(ExtractText(out, texts[17]), "Scale");
    EXPECT_EQ(ExtractText(out, texts[19]), "4.000");
    EXPECT_EQ(ExtractText(out, texts[21]), "5.000");
    EXPECT_EQ(ExtractText(out, texts[23]), "6.000");

    EXPECT_EQ(ExtractText(out, texts[24]), "Mesh");
    EXPECT_EQ(ExtractText(out, texts[25]), "Mesh Id");
    EXPECT_EQ(ExtractText(out, texts[26]), "42");

    EXPECT_EQ(ExtractText(out, texts[27]), "Visual");
    EXPECT_EQ(ExtractText(out, texts[28]), "Material Id");
    EXPECT_EQ(ExtractText(out, texts[29]), "99");

    EXPECT_EQ(ExtractText(out, texts[30]), "Camera");
    EXPECT_EQ(ExtractText(out, texts[31]), "Domain");
    EXPECT_EQ(ExtractText(out, texts[32]), "Editor");
    EXPECT_EQ(ExtractText(out, texts[33]), "Fov Y");
    EXPECT_EQ(ExtractText(out, texts[34]), "0.500");
    EXPECT_EQ(ExtractText(out, texts[35]), "Aspect");
    EXPECT_EQ(ExtractText(out, texts[36]), "1.250");
    EXPECT_EQ(ExtractText(out, texts[37]), "Near Plane");
}

TEST(EntityEditorView, test_render_passes_cursor_coordinate_to_panel)
{
    EntityEditorView view;
    DrawList out;
    World world;
    FontAtlas atlas;

    const Entity entity = world.CreateEntity("ClampEntity");
    virasa::ecs::AddMesh(world, entity, MeshComponent{42u});

    EXPECT_EQ(view.HandleTextInput("G", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.GetCursorRow(), 2u);
    EXPECT_EQ(view.GetCursorCell(), 1u);

    view.Render(out, world, entity, atlas, kX, kY, kWidth, kHeight);

    EXPECT_EQ(view.GetCursorRow(), 2u);
    EXPECT_EQ(view.GetCursorCell(), 1u);
    ASSERT_EQ(out.GetQuads().size(), 2u);

    DrawList invalidOut;
    view.Render(invalidOut, world, Entity::Invalid(), atlas, kX, kY, kWidth, kHeight);
    EXPECT_EQ(view.GetCursorRow(), 2u);
    EXPECT_EQ(view.GetCursorCell(), 1u);
}

TEST(EntityEditorView, test_visible_rows_dfs_traversal_excludes_collapsed_field_rows)
{
    EntityEditorView view;
    DrawList out;
    World world;
    FontAtlas atlas;

    const Entity entity = world.CreateEntity("CollapsedEntity");
    world.Transforms().Add(entity, Transform{});
    virasa::ecs::AddMesh(world, entity, MeshComponent{7u});

    view.Render(out, world, entity, atlas, kX, kY, kWidth, kHeight);
    ASSERT_EQ(out.GetTexts().size(), 27u);

    EXPECT_EQ(view.HandleTextInput("j", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.GetCursorRow(), 1u);
    EXPECT_EQ(view.HandleTextInput("h", world, entity), EntityEditorViewKeyResult::Consumed);

    DrawList collapsedOut;
    view.Render(collapsedOut, world, entity, atlas, kX, kY, kWidth, kHeight);
    ASSERT_EQ(collapsedOut.GetTexts().size(), 6u);
    EXPECT_EQ(ExtractText(collapsedOut, collapsedOut.GetTexts()[0]), "Name");
    EXPECT_EQ(ExtractText(collapsedOut, collapsedOut.GetTexts()[1]), "CollapsedEntity");
    EXPECT_EQ(ExtractText(collapsedOut, collapsedOut.GetTexts()[2]), "Transform");
    EXPECT_EQ(ExtractText(collapsedOut, collapsedOut.GetTexts()[3]), "Mesh");
    EXPECT_EQ(ExtractText(collapsedOut, collapsedOut.GetTexts()[4]), "Mesh Id");
    EXPECT_EQ(ExtractText(collapsedOut, collapsedOut.GetTexts()[5]), "7");
}

TEST(EntityEditorView, test_handle_key_handles_enter_escape_and_edit_keys)
{
    World world;
    const Entity entity = world.CreateEntity("KeyEntity");
    virasa::ecs::AddMesh(world, entity, MeshComponent{7u});
    world.Transforms().Add(entity, Transform{});

    EntityEditorView view;

    EXPECT_EQ(view.HandleKey(KeyCode::Escape, world, entity),
              EntityEditorViewKeyResult::RequestHierarchy);
    EXPECT_EQ(view.GetCursorRow(), 0u);
    EXPECT_EQ(view.GetCursorCell(), 0u);
    EXPECT_FALSE(view.IsEditing());

    EXPECT_EQ(view.HandleKey(KeyCode::Enter, world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_FALSE(view.IsEditing());

    EXPECT_EQ(view.HandleTextInput("j", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.HandleTextInput("j", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.GetCursorRow(), 2u);
    EXPECT_EQ(view.GetCursorCell(), 2u);

    EXPECT_EQ(view.HandleKey(KeyCode::Enter, world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_TRUE(view.IsEditing());

    EXPECT_EQ(view.HandleTextInput("12", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.HandleKey(KeyCode::Backspace, world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_TRUE(view.IsEditing());

    EXPECT_EQ(view.HandleKey(KeyCode::Escape, world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_FALSE(view.IsEditing());
    EXPECT_EQ(world.Transforms().GetLocal(entity).translation.x, 0.0f);

    EXPECT_EQ(view.HandleKey(KeyCode::Enter, world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_TRUE(view.IsEditing());
    EXPECT_EQ(view.HandleTextInput("2.5", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.HandleKey(KeyCode::Enter, world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_FALSE(view.IsEditing());
    EXPECT_FLOAT_EQ(world.Transforms().GetLocal(entity).translation.x, 0.0f);

    EXPECT_EQ(view.HandleKey(KeyCode::Unknown, world, entity), EntityEditorViewKeyResult::Consumed);
}

TEST(EntityEditorView, test_handle_text_input_dispatches_by_codepoint)
{
    World world;
    const Entity entity = world.CreateEntity("DispatchEntity");
    world.Transforms().Add(entity, Transform{});

    EntityEditorView view;

    EXPECT_EQ(view.HandleTextInput("gX", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.GetCursorRow(), 0u);
    EXPECT_EQ(view.GetCursorCell(), 0u);

    EXPECT_EQ(view.HandleTextInput("j", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.GetCursorRow(), 1u);

    EXPECT_EQ(view.HandleTextInput(":tail", world, entity),
              EntityEditorViewKeyResult::RequestCommandBar);
    EXPECT_EQ(view.GetCursorRow(), 1u);
    EXPECT_EQ(view.GetCursorCell(), 0u);

    EXPECT_EQ(view.HandleTextInput("j", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.GetCursorCell(), 2u);
    EXPECT_EQ(view.HandleTextInput("i", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_TRUE(view.IsEditing());

    EXPECT_EQ(view.HandleTextInput("hj:kG", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_TRUE(view.IsEditing());

    DrawList out;
    FontAtlas atlas;
    view.Render(out, world, entity, atlas, kX, kY, kWidth, kHeight);
    const auto texts = out.GetTexts();
    ASSERT_GE(texts.size(), 10u);
    EXPECT_EQ(ExtractText(out, texts[5]), "0.000hj:kG");
}

TEST(EntityEditorView, test_entity_editor_text_input_h_collapses_or_walks_left)
{
    World world;
    const Entity entity = world.CreateEntity("HBinding");
    world.Transforms().Add(entity, Transform{});

    EntityEditorView view;

    EXPECT_EQ(view.HandleTextInput("j", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.GetCursorRow(), 1u);
    EXPECT_EQ(view.GetCursorCell(), 0u);

    EXPECT_EQ(view.HandleTextInput("h", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.GetCursorRow(), 1u);
    EXPECT_EQ(view.GetCursorCell(), 0u);

    DrawList out;
    FontAtlas atlas;
    view.Render(out, world, entity, atlas, kX, kY, kWidth, kHeight);
    EXPECT_EQ(out.GetTexts().size(), 3u);

    EXPECT_EQ(view.HandleTextInput("l", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.HandleTextInput("j", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.HandleTextInput("l", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.GetCursorCell(), 4u);
    EXPECT_EQ(view.HandleTextInput("h", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.GetCursorCell(), 2u);
}

TEST(EntityEditorView, test_entity_editor_text_input_l_expands_or_walks_right)
{
    World world;
    const Entity entity = world.CreateEntity("LBinding");
    world.Transforms().Add(entity, Transform{});

    EntityEditorView view;

    EXPECT_EQ(view.HandleTextInput("j", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.HandleTextInput("h", world, entity), EntityEditorViewKeyResult::Consumed);

    DrawList collapsedOut;
    FontAtlas atlas;
    view.Render(collapsedOut, world, entity, atlas, kX, kY, kWidth, kHeight);
    EXPECT_EQ(collapsedOut.GetTexts().size(), 3u);

    EXPECT_EQ(view.HandleTextInput("l", world, entity), EntityEditorViewKeyResult::Consumed);
    DrawList expandedOut;
    view.Render(expandedOut, world, entity, atlas, kX, kY, kWidth, kHeight);
    EXPECT_GT(expandedOut.GetTexts().size(), collapsedOut.GetTexts().size());

    EXPECT_EQ(view.HandleTextInput("j", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.GetCursorCell(), 2u);
    EXPECT_EQ(view.HandleTextInput("l", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.GetCursorCell(), 4u);
}

TEST(EntityEditorView, test_entity_editor_text_input_j_moves_cursor_down_to_next_row)
{
    World world;
    const Entity entity = world.CreateEntity("JBinding");
    world.Transforms().Add(entity, Transform{});

    EntityEditorView view;

    EXPECT_EQ(view.HandleTextInput("j", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.GetCursorRow(), 1u);
    EXPECT_EQ(view.GetCursorCell(), 0u);

    EXPECT_EQ(view.HandleTextInput("j", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.GetCursorRow(), 2u);
    EXPECT_EQ(view.GetCursorCell(), 2u);

    EXPECT_EQ(view.HandleTextInput("j", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.GetCursorRow(), 3u);
    EXPECT_EQ(view.GetCursorCell(), 2u);

    EXPECT_EQ(view.HandleTextInput("j", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.GetCursorRow(), 4u);
    EXPECT_EQ(view.GetCursorCell(), 2u);

    EXPECT_EQ(view.HandleTextInput("j", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.GetCursorRow(), 4u);
}

TEST(EntityEditorView, test_entity_editor_text_input_k_moves_cursor_up_to_previous_row)
{
    World world;
    const Entity entity = world.CreateEntity("KBinding");
    world.Transforms().Add(entity, Transform{});

    EntityEditorView view;
    EXPECT_EQ(view.HandleTextInput("G", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.GetCursorRow(), 4u);

    EXPECT_EQ(view.HandleTextInput("k", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.GetCursorRow(), 3u);
    EXPECT_EQ(view.GetCursorCell(), 2u);

    EXPECT_EQ(view.HandleTextInput("k", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.GetCursorRow(), 2u);
    EXPECT_EQ(view.GetCursorCell(), 2u);

    EXPECT_EQ(view.HandleTextInput("k", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.GetCursorRow(), 1u);
    EXPECT_EQ(view.GetCursorCell(), 0u);

    EXPECT_EQ(view.HandleTextInput("k", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.GetCursorRow(), 0u);
    EXPECT_EQ(view.GetCursorCell(), 1u);

    EXPECT_EQ(view.HandleTextInput("k", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.GetCursorRow(), 0u);
}

TEST(EntityEditorView, test_entity_editor_text_input_gg_moves_cursor_to_first_row)
{
    World world;
    const Entity entity = world.CreateEntity("GGEntity");
    world.Transforms().Add(entity, Transform{});

    EntityEditorView view;
    EXPECT_EQ(view.HandleTextInput("G", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.GetCursorRow(), 4u);

    EXPECT_EQ(view.HandleTextInput("g", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.GetCursorRow(), 4u);

    EXPECT_EQ(view.HandleTextInput("g", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.GetCursorRow(), 0u);
    EXPECT_EQ(view.GetCursorCell(), 1u);
}

TEST(EntityEditorView, test_entity_editor_text_input_capital_G_moves_cursor_to_last_row)
{
    World world;
    const Entity entity = world.CreateEntity("GEntity");
    world.Transforms().Add(entity, Transform{});

    EntityEditorView view;
    EXPECT_EQ(view.HandleTextInput("G", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.GetCursorRow(), 4u);
    EXPECT_EQ(view.GetCursorCell(), 2u);

    World emptyWorld;
    const Entity emptyEntity = emptyWorld.CreateEntity("OnlyName");
    EntityEditorView emptyView;
    EXPECT_EQ(emptyView.HandleTextInput("G", emptyWorld, emptyEntity),
              EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(emptyView.GetCursorRow(), 0u);
    EXPECT_EQ(emptyView.GetCursorCell(), 1u);
}

TEST(EntityEditorView, test_entity_editor_text_input_i_enters_edit_mode_on_value_cell)
{
    World world;
    const Entity entity = world.CreateEntity("IEntity");
    world.Transforms().Add(entity, Transform{});

    EntityEditorView view;
    EXPECT_EQ(view.HandleTextInput("j", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_FALSE(view.IsEditing());

    EXPECT_EQ(view.HandleTextInput("i", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_FALSE(view.IsEditing());

    EXPECT_EQ(view.HandleTextInput("j", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.HandleTextInput("i", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_TRUE(view.IsEditing());
}

TEST(EntityEditorView, test_entity_editor_text_input_colon_requests_command_bar)
{
    World world;
    const Entity entity = world.CreateEntity("ColonEntity");
    world.Transforms().Add(entity, Transform{});

    EntityEditorView view;
    EXPECT_EQ(view.HandleTextInput("g", world, entity), EntityEditorViewKeyResult::Consumed);

    EXPECT_EQ(view.HandleTextInput(":ignored", world, entity),
              EntityEditorViewKeyResult::RequestCommandBar);
    EXPECT_EQ(view.GetCursorRow(), 0u);
    EXPECT_EQ(view.GetCursorCell(), 0u);

    EXPECT_EQ(view.HandleTextInput("g", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.HandleTextInput("g", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.GetCursorRow(), 0u);
    EXPECT_EQ(view.GetCursorCell(), 1u);
}

TEST(EntityEditorView, test_edit_mode_enter_seeds_buffer_from_value_cell)
{
    World world;
    const Entity entity = world.CreateEntity("SeedName");
    world.Transforms().Add(entity, Transform{});

    EntityEditorView view;
    EXPECT_EQ(view.HandleTextInput("l", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.HandleTextInput("i", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_TRUE(view.IsEditing());

    DrawList out;
    FontAtlas atlas;
    view.Render(out, world, entity, atlas, kX, kY, kWidth, kHeight);
    ASSERT_GE(out.GetTexts().size(), 2u);
    EXPECT_EQ(ExtractText(out, out.GetTexts()[1]), "SeedName");

    EXPECT_EQ(view.HandleKey(KeyCode::Escape, world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_FALSE(view.IsEditing());

    EXPECT_EQ(view.HandleTextInput("j", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.HandleTextInput("j", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.HandleTextInput("i", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_TRUE(view.IsEditing());

    DrawList transformOut;
    view.Render(transformOut, world, entity, atlas, kX, kY, kWidth, kHeight);
    EXPECT_EQ(ExtractText(transformOut, transformOut.GetTexts()[5]), "0.000");
}

TEST(EntityEditorView, test_edit_mode_buffers_keystrokes_until_commit_or_cancel)
{
    World world;
    const Entity entity = world.CreateEntity("BufferEntity");
    world.Transforms().Add(entity, Transform{});

    EntityEditorView view;
    EXPECT_EQ(view.HandleTextInput("j", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.HandleTextInput("j", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.HandleTextInput("i", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_TRUE(view.IsEditing());

    EXPECT_EQ(view.HandleTextInput("hjg:G", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.GetCursorRow(), 2u);
    EXPECT_EQ(view.GetCursorCell(), 2u);

    DrawList out;
    FontAtlas atlas;
    view.Render(out, world, entity, atlas, kX, kY, kWidth, kHeight);
    EXPECT_EQ(ExtractText(out, out.GetTexts()[5]), "0.000hjg:G");

    EXPECT_EQ(view.HandleKey(KeyCode::Backspace, world, entity), EntityEditorViewKeyResult::Consumed);
    DrawList afterBackspace;
    view.Render(afterBackspace, world, entity, atlas, kX, kY, kWidth, kHeight);
    EXPECT_EQ(ExtractText(afterBackspace, afterBackspace.GetTexts()[5]), "0.000hjg:");
}

TEST(EntityEditorView, test_render_overlays_edit_buffer_on_focused_value_cell)
{
    World world;
    const Entity entity = world.CreateEntity("OverlayEntity");
    world.Transforms().Add(entity, Transform{});

    EntityEditorView view;
    EXPECT_EQ(view.HandleTextInput("j", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.HandleTextInput("j", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.HandleTextInput("i", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.HandleTextInput("abc", world, entity), EntityEditorViewKeyResult::Consumed);

    DrawList out;
    FontAtlas atlas;
    view.Render(out, world, entity, atlas, kX, kY, kWidth, kHeight);
    const auto texts = out.GetTexts();
    ASSERT_GE(texts.size(), 10u);
    EXPECT_EQ(ExtractText(out, texts[5]), "0.000abc");
    EXPECT_EQ(ExtractText(out, texts[7]), "0.000");
    EXPECT_EQ(ExtractText(out, texts[9]), "0.000");
}

TEST(EntityEditorView, test_edit_mode_commit_writes_buffer_to_component)
{
    World world;
    const Entity entity = world.CreateEntity("CommitEntity");
    world.Transforms().Add(entity, Transform{});
    virasa::ecs::AddMesh(world, entity, MeshComponent{7u});

    EntityEditorView view;

    EXPECT_EQ(view.HandleTextInput("l", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.HandleTextInput("i", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.HandleTextInput("Renamed", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.HandleKey(KeyCode::Enter, world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(world.GetNameComponent(entity).name, "CommitEntityRenamed");

    EXPECT_EQ(view.HandleTextInput("j", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.HandleTextInput("j", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.HandleTextInput("i", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.HandleTextInput(" 2.5 ", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.HandleKey(KeyCode::Enter, world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_FLOAT_EQ(world.Transforms().GetLocal(entity).translation.x, 0.0f);

    EXPECT_EQ(view.HandleTextInput("j", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.HandleTextInput("j", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.HandleTextInput("j", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.HandleTextInput("i", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.HandleTextInput("123", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.HandleKey(KeyCode::Enter, world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(virasa::ecs::GetMesh(world, entity).meshId, 7u);
}

TEST(EntityEditorView, test_edit_mode_cancel_discards_buffer)
{
    World world;
    const Entity entity = world.CreateEntity("CancelEntity");
    world.Transforms().Add(entity, Transform{});

    EntityEditorView view;
    EXPECT_EQ(view.HandleTextInput("j", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.HandleTextInput("j", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.HandleTextInput("i", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_EQ(view.HandleTextInput("9.5", world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_TRUE(view.IsEditing());

    EXPECT_EQ(view.HandleKey(KeyCode::Escape, world, entity), EntityEditorViewKeyResult::Consumed);
    EXPECT_FALSE(view.IsEditing());
    EXPECT_FLOAT_EQ(world.Transforms().GetLocal(entity).translation.x, 0.0f);
    EXPECT_EQ(view.GetCursorRow(), 2u);
    EXPECT_EQ(view.GetCursorCell(), 2u);
}

TEST(EntityEditorView, test_render_consumes_references_for_duration_of_call)
{
    EntityEditorView view;
    DrawList out;
    World world;
    FontAtlas atlas;

    const Entity entity = world.CreateEntity("PersistentName");
    virasa::ecs::AddMesh(world, entity, MeshComponent{123u});

    out.AddText(1.0f, 2.0f, "prefix", {1.0f, 1.0f, 1.0f, 1.0f});
    view.Render(out, world, entity, atlas, kX, kY, kWidth, kHeight);

    ASSERT_EQ(out.GetQuads().size(), 1u);
    const auto texts = out.GetTexts();
    ASSERT_EQ(texts.size(), 6u);

    EXPECT_EQ(ExtractText(out, texts[0]), "prefix");
    EXPECT_EQ(ExtractText(out, texts[1]), "Name");
    EXPECT_EQ(ExtractText(out, texts[2]), "PersistentName");
    EXPECT_EQ(ExtractText(out, texts[3]), "Mesh");
    EXPECT_EQ(ExtractText(out, texts[4]), "Mesh Id");
    EXPECT_EQ(ExtractText(out, texts[5]), "123");

    const std::size_t textBufferSizeAfterRender = out.GetTextBuffer().size();
    EXPECT_GT(textBufferSizeAfterRender, std::string_view("prefix").size());

    DrawList secondOut;
    view.Render(secondOut, world, entity, atlas, kX, kY, kWidth, kHeight);
    ASSERT_EQ(secondOut.GetQuads().size(), 1u);
    EXPECT_EQ(secondOut.GetTexts().size(), 5u);
    EXPECT_EQ(ExtractText(secondOut, secondOut.GetTexts()[0]), "Name");
    EXPECT_EQ(ExtractText(secondOut, secondOut.GetTexts()[1]), "PersistentName");
    EXPECT_EQ(ExtractText(secondOut, secondOut.GetTexts()[2]), "Mesh");
    EXPECT_EQ(ExtractText(secondOut, secondOut.GetTexts()[3]), "Mesh Id");
    EXPECT_EQ(ExtractText(secondOut, secondOut.GetTexts()[4]), "123");
}
