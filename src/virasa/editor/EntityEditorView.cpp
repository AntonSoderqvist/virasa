#include "virasa/editor/EntityEditorView.h"

#include "virasa/ecs/Components.h"
#include "virasa/math/Transform.h"
#include "virasa/renderer/Types.h"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace virasa::editor
{

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace
{

/// Format a float with "%.3f", stripping "-0.000" to "0.000".
std::string FormatFloat(float v)
{
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%.3f", v);
	// Strip negative zero
	if (std::strcmp(buf, "-0.000") == 0)
	{
		return "0.000";
	}
	return std::string(buf);
}

/// Format a Vec3 as "(x, y, z)" using float formatting.
std::string FormatVec3(const virasa::math::Vec3& v)
{
	return "(" + FormatFloat(v.x) + ", " + FormatFloat(v.y) + ", " + FormatFloat(v.z) + ")";
}

/// Format a Quat as "(w, x, y, z)" using float formatting.
std::string FormatQuat(const virasa::math::Quat& q)
{
	return "(" + FormatFloat(q.w) + ", " + FormatFloat(q.x) + ", " + FormatFloat(q.y) + ", " + FormatFloat(q.z) + ")";
}

/// Format a registry id: 0xFFFFFFFFu -> "<none>", otherwise decimal.
std::string FormatRegistryId(uint32_t id)
{
	if (id == 0xFFFFFFFFu)
	{
		return "<none>";
	}
	return std::to_string(id);
}

/// Format a CameraDomain value.
std::string FormatCameraDomain(virasa::CameraDomain domain)
{
	switch (domain)
	{
		case virasa::CameraDomain::Main:   return "Main";
		case virasa::CameraDomain::Editor: return "Editor";
		default:
		{
			char buf[32];
			std::snprintf(buf, sizeof(buf), "CameraDomain(%u)",
				static_cast<unsigned>(static_cast<uint8_t>(domain)));
			return std::string(buf);
		}
	}
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// EntityEditorView
// ---------------------------------------------------------------------------

virasa::ui::EntityEditorPanel& EntityEditorView::GetPanel() noexcept
{
	return _panel;
}

const virasa::ui::EntityEditorPanel& EntityEditorView::GetPanel() const noexcept
{
	return _panel;
}

void EntityEditorView::Render(
	virasa::ui::DrawList& out,
	const virasa::ecs::World& world,
	virasa::ecs::Entity entity,
	const virasa::ui::FontAtlas& atlas,
	float x,
	float y,
	float width,
	float height)
{
	// Check for invalid or stale entity
	if (entity == virasa::ecs::Entity::Invalid() || !world.IsValid(entity))
	{
		_panel.Render(out, std::span<const virasa::ui::EntityEditorPanelRow>{}, atlas, x, y, width, height);
		return;
	}

	// Per-call scratch storage for formatted strings and rows
	std::vector<std::string> scratch;
	scratch.reserve(32);

	std::vector<virasa::ui::EntityEditorPanelRow> rows;
	rows.reserve(32);

	// Helper to add a string to scratch and return a string_view over it
	auto addScratch = [&scratch](std::string s) -> std::string_view
	{
		scratch.push_back(std::move(s));
		return std::string_view(scratch.back());
	};

	// Helper to emit a SectionHeader row
	auto emitSection = [&rows](std::string_view caption)
	{
		virasa::ui::EntityEditorPanelRow row;
		row.caption = caption;
		row.value   = {};
		row.kind    = virasa::ui::EntityEditorRowKind::SectionHeader;
		rows.push_back(row);
	};

	// Helper to emit a Field row
	auto emitField = [&rows](std::string_view caption, std::string_view value)
	{
		virasa::ui::EntityEditorPanelRow row;
		row.caption = caption;
		row.value   = value;
		row.kind    = virasa::ui::EntityEditorRowKind::Field;
		rows.push_back(row);
	};

	// Step 1: Name row
	if (world.HasNameComponent(entity))
	{
		const virasa::ecs::NameComponent& nc = world.GetNameComponent(entity);
		emitField("Name", std::string_view(nc.name));
	}
	else
	{
		emitField("Name", "<unnamed>");
	}

	// Step 2: Transform
	if (world.HasTransformComponent(entity))
	{
		const virasa::math::Transform& t = world.GetTransformComponent(entity);
		emitSection("Transform");
		emitField("Translation", addScratch(FormatVec3(t.translation)));
		emitField("Rotation",    addScratch(FormatQuat(t.rotation)));
		emitField("Scale",       addScratch(FormatVec3(t.scale)));
	}

	// Step 3: Mesh
	if (world.HasMeshComponent(entity))
	{
		const virasa::ecs::MeshComponent& mc = world.GetMeshComponent(entity);
		emitSection("Mesh");
		emitField("Mesh Id", addScratch(FormatRegistryId(mc.meshId)));
	}

	// Step 4: Visual
	if (world.HasVisualComponent(entity))
	{
		const virasa::ecs::VisualComponent& vc = world.GetVisualComponent(entity);
		emitSection("Visual");
		emitField("Material Id", addScratch(FormatRegistryId(vc.materialId)));
	}

	// Step 5: DirectionalLight
	if (world.HasDirectionalLightComponent(entity))
	{
		const virasa::ecs::DirectionalLightComponent& dl = world.GetDirectionalLightComponent(entity);
		emitSection("DirectionalLight");
		emitField("Direction", addScratch(FormatVec3(dl.direction)));
		emitField("Color",     addScratch(FormatVec3(dl.color)));
		emitField("Intensity", addScratch(FormatFloat(dl.intensity)));
	}

	// Step 6: PointLight
	if (world.HasPointLightComponent(entity))
	{
		const virasa::ecs::PointLightComponent& pl = world.GetPointLightComponent(entity);
		emitSection("PointLight");
		emitField("Color",     addScratch(FormatVec3(pl.color)));
		emitField("Intensity", addScratch(FormatFloat(pl.intensity)));
		emitField("Range",     addScratch(FormatFloat(pl.range)));
	}

	// Step 7: SpotLight
	if (world.HasSpotLightComponent(entity))
	{
		const virasa::ecs::SpotLightComponent& sl = world.GetSpotLightComponent(entity);
		emitSection("SpotLight");
		emitField("Color",          addScratch(FormatVec3(sl.color)));
		emitField("Intensity",      addScratch(FormatFloat(sl.intensity)));
		emitField("Range",          addScratch(FormatFloat(sl.range)));
		emitField("Inner Cone Cos", addScratch(FormatFloat(sl.innerConeCos)));
		emitField("Outer Cone Cos", addScratch(FormatFloat(sl.outerConeCos)));
	}

	// Step 8: Camera
	if (world.HasCameraComponent(entity))
	{
		const virasa::ecs::CameraComponent& cam = world.GetCameraComponent(entity);
		emitSection("Camera");
		emitField("Domain",     addScratch(FormatCameraDomain(cam.domain)));
		emitField("Fov Y",      addScratch(FormatFloat(cam.fovY)));
		emitField("Aspect",     addScratch(FormatFloat(cam.aspect)));
		emitField("Near Plane", addScratch(FormatFloat(cam.nearPlane)));
		emitField("Far Plane",  addScratch(FormatFloat(cam.farPlane)));
	}

	_panel.Render(out, std::span<const virasa::ui::EntityEditorPanelRow>(rows), atlas, x, y, width, height);
}

} // namespace virasa::editor
