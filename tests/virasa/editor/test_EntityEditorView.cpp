#include <gtest/gtest.h>

#include "virasa/editor/EntityEditorView.h"
#include "virasa/ui/EntityEditorPanel.h"
#include "virasa/ui/Types.h"
#include "virasa/ui/FontAtlas.h"
#include "virasa/ecs/World.h"
#include "virasa/ecs/Components.h"
#include "virasa/math/Transform.h"
#include "virasa/renderer/Types.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

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
using virasa::math::Transform;
using virasa::CameraDomain;
using virasa::ui::DrawList;
using virasa::ui::EntityEditorPanel;
using virasa::ui::EntityEditorPanelConfig;
using virasa::ui::FontAtlas;
using virasa::ui::TextCommand;

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

TEST(EntityEditorView, test_entity_editor_view_owns_panel)
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

    EntityEditorPanelConfig updatedConfig = view.GetPanel().GetConfig();
    updatedConfig.paddingX = 17.0f;
    updatedConfig.captionColumnWidth = 222.0f;
    view.GetPanel().SetConfig(updatedConfig);

    EntityEditorView copied = view;
    EXPECT_FLOAT_EQ(copied.GetPanel().GetConfig().paddingX, 17.0f);
    EXPECT_FLOAT_EQ(copied.GetPanel().GetConfig().captionColumnWidth, 222.0f);

    EntityEditorView moved = std::move(view);
    EXPECT_FLOAT_EQ(moved.GetPanel().GetConfig().paddingX, 17.0f);
    EXPECT_FLOAT_EQ(moved.GetPanel().GetConfig().captionColumnWidth, 222.0f);
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

    world.AddCameraComponent(entity, CameraComponent{});
    world.AddSpotLightComponent(entity, SpotLightComponent{});
    world.AddPointLightComponent(entity, PointLightComponent{});
    world.AddDirectionalLightComponent(entity, DirectionalLightComponent{});
    world.AddVisualComponent(entity, VisualComponent{});
    world.AddMeshComponent(entity, MeshComponent{});
    world.AddTransformComponent(entity, Transform{});

    view.Render(out, world, entity, atlas, kX, kY, kWidth, kHeight);

    const auto texts = out.GetTexts();
    ASSERT_EQ(texts.size(), 51u);

    EXPECT_EQ(ExtractText(out, texts[2]), "Transform");
    EXPECT_EQ(ExtractText(out, texts[9]), "Mesh");
    EXPECT_EQ(ExtractText(out, texts[12]), "Visual");
    EXPECT_EQ(ExtractText(out, texts[15]), "DirectionalLight");
    EXPECT_EQ(ExtractText(out, texts[22]), "PointLight");
    EXPECT_EQ(ExtractText(out, texts[29]), "SpotLight");
    EXPECT_EQ(ExtractText(out, texts[40]), "Camera");
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
    transform.rotation = virasa::math::Quat(1.0f, -0.0f, 0.25f, -0.5f);
    transform.scale = virasa::math::Vec3(2.0f, -0.0f, 3.125f);
    world.AddTransformComponent(entity, transform);

    world.AddMeshComponent(entity, MeshComponent{7u});
    world.AddVisualComponent(entity, VisualComponent{0xFFFFFFFFu});

    DirectionalLightComponent directional{};
    directional.direction = virasa::math::Vec3(-0.0f, 2.0f, -3.25f);
    directional.color = virasa::math::Vec3(4.0f, -0.0f, 5.5f);
    directional.intensity = -0.0f;
    world.AddDirectionalLightComponent(entity, directional);

    PointLightComponent point{};
    point.color = virasa::math::Vec3(0.0f, -1.25f, 2.5f);
    point.intensity = 6.0f;
    point.range = -0.0f;
    world.AddPointLightComponent(entity, point);

    SpotLightComponent spot{};
    spot.color = virasa::math::Vec3(-0.0f, 0.0f, 9.0f);
    spot.intensity = 1.2349f;
    spot.range = 10.0f;
    spot.innerConeCos = -0.0f;
    spot.outerConeCos = 0.875f;
    world.AddSpotLightComponent(entity, spot);

    CameraComponent camera{};
    camera.domain = static_cast<CameraDomain>(7);
    camera.fovY = -0.0f;
    camera.aspect = 1.7777f;
    camera.nearPlane = 0.1f;
    camera.farPlane = 4294967294.0f;
    world.AddCameraComponent(entity, camera);

    view.Render(out, world, entity, atlas, kX, kY, kWidth, kHeight);

    const auto texts = out.GetTexts();
    ASSERT_EQ(texts.size(), 51u);

    EXPECT_EQ(ExtractText(out, texts[4]), "(0.000, 1.500, -0.100)");
    EXPECT_EQ(ExtractText(out, texts[6]), "(1.000, 0.000, 0.250, -0.500)");
    EXPECT_EQ(ExtractText(out, texts[8]), "(2.000, 0.000, 3.125)");
    EXPECT_EQ(ExtractText(out, texts[11]), "7");
    EXPECT_EQ(ExtractText(out, texts[14]), "<none>");
    EXPECT_EQ(ExtractText(out, texts[17]), "(0.000, 2.000, -3.250)");
    EXPECT_EQ(ExtractText(out, texts[19]), "(4.000, 0.000, 5.500)");
    EXPECT_EQ(ExtractText(out, texts[21]), "0.000");
    EXPECT_EQ(ExtractText(out, texts[24]), "(0.000, -1.250, 2.500)");
    EXPECT_EQ(ExtractText(out, texts[26]), "6.000");
    EXPECT_EQ(ExtractText(out, texts[28]), "0.000");
    EXPECT_EQ(ExtractText(out, texts[31]), "(0.000, 0.000, 9.000)");
    EXPECT_EQ(ExtractText(out, texts[33]), "1.235");
    EXPECT_EQ(ExtractText(out, texts[35]), "10.000");
    EXPECT_EQ(ExtractText(out, texts[37]), "0.000");
    EXPECT_EQ(ExtractText(out, texts[39]), "0.875");
    EXPECT_EQ(ExtractText(out, texts[42]), "CameraDomain(7)");
    EXPECT_EQ(ExtractText(out, texts[44]), "0.000");
    EXPECT_EQ(ExtractText(out, texts[46]), "1.778");
    EXPECT_EQ(ExtractText(out, texts[48]), "0.100");
    EXPECT_EQ(ExtractText(out, texts[50]), "4294967296.000");
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
    world.AddTransformComponent(entity, transform);
    world.AddMeshComponent(entity, MeshComponent{42u});
    world.AddVisualComponent(entity, VisualComponent{99u});

    CameraComponent camera{};
    camera.domain = CameraDomain::Editor;
    camera.fovY = 0.5f;
    camera.aspect = 1.25f;
    camera.nearPlane = 0.2f;
    camera.farPlane = 500.0f;
    world.AddCameraComponent(entity, camera);

    view.Render(out, world, entity, atlas, kX, kY, kWidth, kHeight);

    ASSERT_EQ(out.GetQuads().size(), 1u);
    const auto texts = out.GetTexts();
    ASSERT_EQ(texts.size(), 26u);

    EXPECT_EQ(ExtractText(out, texts[0]), "Name");
    EXPECT_EQ(ExtractText(out, texts[1]), "InspectorTarget");

    EXPECT_EQ(ExtractText(out, texts[2]), "Transform");
    EXPECT_EQ(ExtractText(out, texts[3]), "Translation");
    EXPECT_EQ(ExtractText(out, texts[4]), "(1.000, 2.000, 3.000)");
    EXPECT_EQ(ExtractText(out, texts[5]), "Rotation");
    EXPECT_EQ(ExtractText(out, texts[6]), "(1.000, 0.000, 0.000, 0.000)");
    EXPECT_EQ(ExtractText(out, texts[7]), "Scale");
    EXPECT_EQ(ExtractText(out, texts[8]), "(4.000, 5.000, 6.000)");

    EXPECT_EQ(ExtractText(out, texts[9]), "Mesh");
    EXPECT_EQ(ExtractText(out, texts[10]), "Mesh Id");
    EXPECT_EQ(ExtractText(out, texts[11]), "42");

    EXPECT_EQ(ExtractText(out, texts[12]), "Visual");
    EXPECT_EQ(ExtractText(out, texts[13]), "Material Id");
    EXPECT_EQ(ExtractText(out, texts[14]), "99");

    EXPECT_EQ(ExtractText(out, texts[15]), "Camera");
    EXPECT_EQ(ExtractText(out, texts[16]), "Domain");
    EXPECT_EQ(ExtractText(out, texts[17]), "Editor");
    EXPECT_EQ(ExtractText(out, texts[18]), "Fov Y");
    EXPECT_EQ(ExtractText(out, texts[19]), "0.500");
    EXPECT_EQ(ExtractText(out, texts[20]), "Aspect");
    EXPECT_EQ(ExtractText(out, texts[21]), "1.250");
    EXPECT_EQ(ExtractText(out, texts[22]), "Near Plane");
    EXPECT_EQ(ExtractText(out, texts[23]), "0.200");
    EXPECT_EQ(ExtractText(out, texts[24]), "Far Plane");
    EXPECT_EQ(ExtractText(out, texts[25]), "500.000");
}

TEST(EntityEditorView, test_render_consumes_references_for_duration_of_call)
{
    EntityEditorView view;
    DrawList out;
    World world;
    FontAtlas atlas;

    const Entity entity = world.CreateEntity("PersistentName");
    world.AddMeshComponent(entity, MeshComponent{123u});

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
