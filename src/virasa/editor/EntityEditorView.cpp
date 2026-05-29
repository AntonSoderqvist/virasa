#include "virasa/editor/EntityEditorView.h"

#include "virasa/ecs/Components.h"
#include "virasa/math/Transform.h"
#include "virasa/renderer/Types.h"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glm/common.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat3x3.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>

namespace virasa::editor
{

struct RowDescriptor
{
	enum class Kind : uint8_t
	{
		Name,
		SectionLabel,
		TransformTranslation,
		TransformRotation,
		TransformScale,
		MeshId,
		VisualMaterialId,
		DirectionalLightDirection,
		DirectionalLightColor,
		DirectionalLightIntensity,
		PointLightColor,
		PointLightIntensity,
		PointLightRange,
		SpotLightColor,
		SpotLightIntensity,
		SpotLightRange,
		SpotLightInnerConeCos,
		SpotLightOuterConeCos,
		CameraDomain,
		CameraFovY,
		CameraAspect,
		CameraNearPlane,
		CameraFarPlane,
	};

	Kind kind = Kind::Name;
	uint8_t sectionIndex = 0u;
};

struct BuiltRows
{
	std::vector<virasa::ui::EntityEditorPanelCell> cells;
	std::vector<virasa::ui::EntityEditorPanelRow> rows;
	std::vector<RowDescriptor> descriptors;
	std::vector<std::string> scratch;
};

namespace
{

using virasa::ui::EntityEditorCellKind;
using virasa::ui::EntityEditorPanelCell;
using virasa::ui::EntityEditorPanelRow;

constexpr uint8_t kTransformSection = 0u;
constexpr uint8_t kMeshSection = 1u;
constexpr uint8_t kVisualSection = 2u;
constexpr uint8_t kDirectionalLightSection = 3u;
constexpr uint8_t kPointLightSection = 4u;
constexpr uint8_t kSpotLightSection = 5u;
constexpr uint8_t kCameraSection = 6u;

std::string FormatFloat(float v)
{
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%.3f", v);
	if (std::strcmp(buf, "-0.000") == 0)
	{
		return "0.000";
	}
	return std::string(buf);
}

std::string FormatRegistryId(uint32_t id)
{
	if (id == 0xFFFFFFFFu)
	{
		return "<none>";
	}
	return std::to_string(id);
}

std::string FormatCameraDomain(virasa::CameraDomain domain)
{
	switch (domain)
	{
		case virasa::CameraDomain::Main:
			return "Main";
		case virasa::CameraDomain::Editor:
			return "Editor";
		default:
		{
			char buf[32];
			std::snprintf(
				buf,
				sizeof(buf),
				"CameraDomain(%u)",
				static_cast<unsigned>(static_cast<uint8_t>(domain)));
			return std::string(buf);
		}
	}
}

std::string_view AddScratch(BuiltRows& built, std::string value)
{
	built.scratch.push_back(std::move(value));
	return std::string_view(built.scratch.back());
}

void AddRow(
	BuiltRows& built,
		RowDescriptor descriptor,
		std::initializer_list<EntityEditorPanelCell> rowCells)
{
	const std::size_t begin = built.cells.size();
	built.cells.insert(built.cells.end(), rowCells.begin(), rowCells.end());
	built.rows.push_back(EntityEditorPanelRow{
		.cells = std::span<const EntityEditorPanelCell>(built.cells.data() + begin, rowCells.size())
	});
	built.descriptors.push_back(descriptor);
}

void AddScalarRow(BuiltRows& built, RowDescriptor descriptor, std::string_view caption, std::string_view value)
{
	AddRow(
		built,
		descriptor,
		{
			EntityEditorPanelCell{EntityEditorCellKind::Label, caption, 0u},
			EntityEditorPanelCell{EntityEditorCellKind::Value, value, 2u},
		});
}

void AddVec3Row(
	BuiltRows& built,
		RowDescriptor descriptor,
		std::string_view caption,
		std::string_view x,
		std::string_view y,
		std::string_view z)
{
	AddRow(
		built,
		descriptor,
		{
			EntityEditorPanelCell{EntityEditorCellKind::Label, caption, 0u},
			EntityEditorPanelCell{EntityEditorCellKind::Label, "X:", 1u},
			EntityEditorPanelCell{EntityEditorCellKind::Value, x, 2u},
			EntityEditorPanelCell{EntityEditorCellKind::Label, "Y:", 3u},
			EntityEditorPanelCell{EntityEditorCellKind::Value, y, 4u},
			EntityEditorPanelCell{EntityEditorCellKind::Label, "Z:", 5u},
			EntityEditorPanelCell{EntityEditorCellKind::Value, z, 6u},
		});
}

glm::vec3 DecomposeQuatToEulerDegreesXYZ(const virasa::math::Quat& q)
{
	const virasa::math::Quat n = glm::normalize(q);
	const glm::mat3 m = glm::mat3_cast(n);
	const float x = std::atan2(m[1][2], m[2][2]);
	const float y = std::asin(std::clamp(-m[0][2], -1.0f, 1.0f));
	const float z = std::atan2(m[0][1], m[0][0]);
	return glm::vec3(glm::degrees(x), glm::degrees(y), glm::degrees(z));
}

virasa::math::Quat ComposeEulerDegreesXYZToQuat(const glm::vec3& degrees)
{
	return virasa::math::Quat(glm::vec3(
		glm::radians(degrees.x), glm::radians(degrees.y), glm::radians(degrees.z)));
}

void AddSectionRow(BuiltRows& built, uint8_t sectionIndex, std::string_view label)
{
	AddRow(
		built,
		RowDescriptor{RowDescriptor::Kind::SectionLabel, sectionIndex},
		{
			EntityEditorPanelCell{EntityEditorCellKind::SectionLabel, label, 0u},
		});
}

std::size_t FindFirstValueCell(const EntityEditorPanelRow& row) noexcept
{
	for (std::size_t i = 0; i < row.cells.size(); ++i)
	{
		if (row.cells[i].kind == EntityEditorCellKind::Value)
		{
			return i;
		}
	}
	return 0u;
}

bool SectionHasVisibleFieldRow(const BuiltRows& built, std::size_t rowIndex)
{
	if (rowIndex >= built.descriptors.size())
	{
		return false;
	}
	const RowDescriptor& descriptor = built.descriptors[rowIndex];
	if (descriptor.kind != RowDescriptor::Kind::SectionLabel)
	{
		return false;
	}
	for (std::size_t i = rowIndex + 1; i < built.descriptors.size(); ++i)
	{
		if (built.descriptors[i].kind == RowDescriptor::Kind::SectionLabel)
		{
			break;
		}
		return true;
	}
	return false;
}

} // namespace

void ClampCursorToBuiltRows(EntityEditorView& view, const BuiltRows& built)
{
	if (built.rows.empty())
	{
		view._cursorRow = 0u;
		view._cursorCell = 0u;
		return;
	}

	view._cursorRow = std::min(view._cursorRow, built.rows.size() - 1u);
	const std::size_t cellCount = built.rows[view._cursorRow].cells.size();
	if (cellCount == 0u)
	{
		view._cursorCell = 0u;
	}
	else
	{
		view._cursorCell = std::min(view._cursorCell, cellCount - 1u);
	}
}

bool ParseFloatStrict(std::string_view text, float& outValue)
{
	while (!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '\n' || text.front() == '\r'))
	{
		text.remove_prefix(1u);
	}
	while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\n' || text.back() == '\r'))
	{
		text.remove_suffix(1u);
	}
	if (text.empty())
	{
		return false;
	}

	const char* begin = text.data();
	const char* end = text.data() + text.size();
	auto result = std::from_chars(begin, end, outValue);
	return result.ec == std::errc() && result.ptr == end;
}

void RemoveLastUtf8Codepoint(std::string& text)
{
	if (text.empty())
	{
		return;
	}

	std::size_t pos = text.size() - 1u;
	while (pos > 0u && (static_cast<unsigned char>(text[pos]) & 0xC0u) == 0x80u)
	{
		--pos;
	}
	text.erase(pos);
}

std::vector<char32_t> DecodeUtf8(std::string_view utf8)
{
	std::vector<char32_t> codepoints;
	for (std::size_t i = 0; i < utf8.size();)
	{
		const unsigned char c0 = static_cast<unsigned char>(utf8[i]);
		char32_t cp = 0;
		std::size_t count = 1u;
		if ((c0 & 0x80u) == 0u)
		{
			cp = c0;
		}
		else if ((c0 & 0xE0u) == 0xC0u && i + 1u < utf8.size())
		{
			cp = static_cast<char32_t>(c0 & 0x1Fu);
			cp = (cp << 6) | static_cast<char32_t>(static_cast<unsigned char>(utf8[i + 1u]) & 0x3Fu);
			count = 2u;
		}
		else if ((c0 & 0xF0u) == 0xE0u && i + 2u < utf8.size())
		{
			cp = static_cast<char32_t>(c0 & 0x0Fu);
			cp = (cp << 6) | static_cast<char32_t>(static_cast<unsigned char>(utf8[i + 1u]) & 0x3Fu);
			cp = (cp << 6) | static_cast<char32_t>(static_cast<unsigned char>(utf8[i + 2u]) & 0x3Fu);
			count = 3u;
		}
		else if ((c0 & 0xF8u) == 0xF0u && i + 3u < utf8.size())
		{
			cp = static_cast<char32_t>(c0 & 0x07u);
			cp = (cp << 6) | static_cast<char32_t>(static_cast<unsigned char>(utf8[i + 1u]) & 0x3Fu);
			cp = (cp << 6) | static_cast<char32_t>(static_cast<unsigned char>(utf8[i + 2u]) & 0x3Fu);
			cp = (cp << 6) | static_cast<char32_t>(static_cast<unsigned char>(utf8[i + 3u]) & 0x3Fu);
			count = 4u;
		}
		else
		{
			cp = c0;
		}
		codepoints.push_back(cp);
		i += count;
	}
	return codepoints;
}

BuiltRows BuildRows(const virasa::ecs::World& world, virasa::ecs::Entity entity, const EntityEditorView& view)
{
	BuiltRows built;
	built.cells.reserve(128);
	built.rows.reserve(32);
	built.descriptors.reserve(32);
	built.scratch.reserve(64);

	std::string_view nameValue = "<unnamed>";
	if (world.HasNameComponent(entity))
	{
		nameValue = world.GetNameComponent(entity).name;
	}
	AddScalarRow(built, RowDescriptor{RowDescriptor::Kind::Name, 0u}, "Name", nameValue);

	if (world.HasTransformComponent(entity))
	{
		const virasa::math::Transform& transform = world.GetTransformComponent(entity);
		AddSectionRow(built, kTransformSection, "Transform");
		if (view._collapsed.count(kTransformSection) == 0u)
		{
			AddVec3Row(
				built,
				RowDescriptor{RowDescriptor::Kind::TransformTranslation, kTransformSection},
				"Translation",
				AddScratch(built, FormatFloat(transform.translation.x)),
				AddScratch(built, FormatFloat(transform.translation.y)),
				AddScratch(built, FormatFloat(transform.translation.z)));
			const glm::vec3 eulerDeg = DecomposeQuatToEulerDegreesXYZ(transform.rotation);
			AddVec3Row(
				built,
				RowDescriptor{RowDescriptor::Kind::TransformRotation, kTransformSection},
				"Rotation",
				AddScratch(built, FormatFloat(eulerDeg.x)),
				AddScratch(built, FormatFloat(eulerDeg.y)),
				AddScratch(built, FormatFloat(eulerDeg.z)));
			AddVec3Row(
				built,
				RowDescriptor{RowDescriptor::Kind::TransformScale, kTransformSection},
				"Scale",
				AddScratch(built, FormatFloat(transform.scale.x)),
				AddScratch(built, FormatFloat(transform.scale.y)),
				AddScratch(built, FormatFloat(transform.scale.z)));
		}
	}

	if (world.HasMeshComponent(entity))
	{
		const virasa::ecs::MeshComponent& component = world.GetMeshComponent(entity);
		AddSectionRow(built, kMeshSection, "Mesh");
		if (view._collapsed.count(kMeshSection) == 0u)
		{
			AddScalarRow(
				built,
				RowDescriptor{RowDescriptor::Kind::MeshId, kMeshSection},
				"Mesh Id",
				AddScratch(built, FormatRegistryId(component.meshId)));
		}
	}

	if (world.HasVisualComponent(entity))
	{
		const virasa::ecs::VisualComponent& component = world.GetVisualComponent(entity);
		AddSectionRow(built, kVisualSection, "Visual");
		if (view._collapsed.count(kVisualSection) == 0u)
		{
			AddScalarRow(
				built,
				RowDescriptor{RowDescriptor::Kind::VisualMaterialId, kVisualSection},
				"Material Id",
				AddScratch(built, FormatRegistryId(component.materialId)));
		}
	}

	if (world.HasDirectionalLightComponent(entity))
	{
		const virasa::ecs::DirectionalLightComponent& component =
			world.GetDirectionalLightComponent(entity);
		AddSectionRow(built, kDirectionalLightSection, "DirectionalLight");
		if (view._collapsed.count(kDirectionalLightSection) == 0u)
		{
			AddVec3Row(
				built,
				RowDescriptor{RowDescriptor::Kind::DirectionalLightDirection, kDirectionalLightSection},
				"Direction",
				AddScratch(built, FormatFloat(component.direction.x)),
				AddScratch(built, FormatFloat(component.direction.y)),
				AddScratch(built, FormatFloat(component.direction.z)));
			AddVec3Row(
				built,
				RowDescriptor{RowDescriptor::Kind::DirectionalLightColor, kDirectionalLightSection},
				"Color",
				AddScratch(built, FormatFloat(component.color.x)),
				AddScratch(built, FormatFloat(component.color.y)),
				AddScratch(built, FormatFloat(component.color.z)));
			AddScalarRow(
				built,
				RowDescriptor{RowDescriptor::Kind::DirectionalLightIntensity, kDirectionalLightSection},
				"Intensity",
				AddScratch(built, FormatFloat(component.intensity)));
		}
	}

	if (world.HasPointLightComponent(entity))
	{
		const virasa::ecs::PointLightComponent& component = world.GetPointLightComponent(entity);
		AddSectionRow(built, kPointLightSection, "PointLight");
		if (view._collapsed.count(kPointLightSection) == 0u)
		{
			AddVec3Row(
				built,
				RowDescriptor{RowDescriptor::Kind::PointLightColor, kPointLightSection},
				"Color",
				AddScratch(built, FormatFloat(component.color.x)),
				AddScratch(built, FormatFloat(component.color.y)),
				AddScratch(built, FormatFloat(component.color.z)));
			AddScalarRow(
				built,
				RowDescriptor{RowDescriptor::Kind::PointLightIntensity, kPointLightSection},
				"Intensity",
				AddScratch(built, FormatFloat(component.intensity)));
			AddScalarRow(
				built,
				RowDescriptor{RowDescriptor::Kind::PointLightRange, kPointLightSection},
				"Range",
				AddScratch(built, FormatFloat(component.range)));
		}
	}

	if (world.HasSpotLightComponent(entity))
	{
		const virasa::ecs::SpotLightComponent& component = world.GetSpotLightComponent(entity);
		AddSectionRow(built, kSpotLightSection, "SpotLight");
		if (view._collapsed.count(kSpotLightSection) == 0u)
		{
			AddVec3Row(
				built,
				RowDescriptor{RowDescriptor::Kind::SpotLightColor, kSpotLightSection},
				"Color",
				AddScratch(built, FormatFloat(component.color.x)),
				AddScratch(built, FormatFloat(component.color.y)),
				AddScratch(built, FormatFloat(component.color.z)));
			AddScalarRow(
				built,
				RowDescriptor{RowDescriptor::Kind::SpotLightIntensity, kSpotLightSection},
				"Intensity",
				AddScratch(built, FormatFloat(component.intensity)));
			AddScalarRow(
				built,
				RowDescriptor{RowDescriptor::Kind::SpotLightRange, kSpotLightSection},
				"Range",
				AddScratch(built, FormatFloat(component.range)));
			AddScalarRow(
				built,
				RowDescriptor{RowDescriptor::Kind::SpotLightInnerConeCos, kSpotLightSection},
				"Inner Cone Cos",
				AddScratch(built, FormatFloat(component.innerConeCos)));
			AddScalarRow(
				built,
				RowDescriptor{RowDescriptor::Kind::SpotLightOuterConeCos, kSpotLightSection},
				"Outer Cone Cos",
				AddScratch(built, FormatFloat(component.outerConeCos)));
		}
	}

	if (world.HasCameraComponent(entity))
	{
		const virasa::ecs::CameraComponent& component = world.GetCameraComponent(entity);
		AddSectionRow(built, kCameraSection, "Camera");
		if (view._collapsed.count(kCameraSection) == 0u)
		{
			AddScalarRow(
				built,
				RowDescriptor{RowDescriptor::Kind::CameraDomain, kCameraSection},
				"Domain",
				AddScratch(built, FormatCameraDomain(component.domain)));
			AddScalarRow(
				built,
				RowDescriptor{RowDescriptor::Kind::CameraFovY, kCameraSection},
				"Fov Y",
				AddScratch(built, FormatFloat(component.fovY)));
			AddScalarRow(
				built,
				RowDescriptor{RowDescriptor::Kind::CameraAspect, kCameraSection},
				"Aspect",
				AddScratch(built, FormatFloat(component.aspect)));
			AddScalarRow(
				built,
				RowDescriptor{RowDescriptor::Kind::CameraNearPlane, kCameraSection},
				"Near Plane",
				AddScratch(built, FormatFloat(component.nearPlane)));
			AddScalarRow(
				built,
				RowDescriptor{RowDescriptor::Kind::CameraFarPlane, kCameraSection},
				"Far Plane",
				AddScratch(built, FormatFloat(component.farPlane)));
		}
	}

	return built;
}

void OverlayEditBuffer(EntityEditorView& view, BuiltRows& built)
{
	if (!view._editing || view._cursorRow >= built.rows.size())
	{
		return;
	}
	EntityEditorPanelRow& row = built.rows[view._cursorRow];
	if (view._cursorCell >= row.cells.size())
	{
		return;
	}
	EntityEditorPanelCell& cell = built.cells[row.cells.data() - built.cells.data() + view._cursorCell];
	if (cell.kind != EntityEditorCellKind::Value)
	{
		return;
	}
	cell.text = view._editBuffer;
}

void EnterEditMode(EntityEditorView& view, const BuiltRows& built, const virasa::ecs::World& world, virasa::ecs::Entity entity)
{
	if (view._cursorRow >= built.rows.size())
	{
		view._pendingG = false;
		return;
	}
	const EntityEditorPanelRow& row = built.rows[view._cursorRow];
	if (view._cursorCell >= row.cells.size())
	{
		view._pendingG = false;
		return;
	}
	const EntityEditorPanelCell& cell = row.cells[view._cursorCell];
	if (cell.kind != EntityEditorCellKind::Value)
	{
		view._pendingG = false;
		return;
	}

	view._editing = true;
	if (built.descriptors[view._cursorRow].kind == RowDescriptor::Kind::Name && !world.HasNameComponent(entity))
	{
		view._editBuffer.clear();
	}
	else
	{
		view._editBuffer.assign(cell.text.data(), cell.text.size());
	}
	view._pendingG = false;
}

void CommitEdit(EntityEditorView& view, virasa::ecs::World& world, virasa::ecs::Entity entity)
{
	if (!world.IsValid(entity))
	{
		view._editing = false;
		view._editBuffer.clear();
		return;
	}

	BuiltRows built = BuildRows(world, entity, view);
	ClampCursorToBuiltRows(view, built);
	if (view._cursorRow >= built.rows.size())
	{
		view._editing = false;
		view._editBuffer.clear();
		return;
	}
	const RowDescriptor& descriptor = built.descriptors[view._cursorRow];
	const EntityEditorPanelRow& row = built.rows[view._cursorRow];
	if (view._cursorCell >= row.cells.size() || row.cells[view._cursorCell].kind != EntityEditorCellKind::Value)
	{
		view._editing = false;
		view._editBuffer.clear();
		return;
	}

	auto componentIndexFromSlot = [cell = row.cells[view._cursorCell]]() -> int
	{
		switch (cell.slotIndex)
		{
			case 2u: return 0;
			case 4u: return 1;
			case 6u: return 2;
			default: return -1;
		}
	};

	if (descriptor.kind == RowDescriptor::Kind::Name)
	{
		if (world.HasNameComponent(entity))
		{
			const_cast<std::string&>(world.GetNameComponent(entity).name) = view._editBuffer;
		}
	}
	else
	{
		float parsed = 0.0f;
		const bool parsedOk = ParseFloatStrict(view._editBuffer, parsed);
		if (parsedOk)
		{
			switch (descriptor.kind)
			{
				case RowDescriptor::Kind::TransformTranslation:
				{
					virasa::math::Transform& transform = world.GetTransformComponent(entity);
					switch (componentIndexFromSlot())
					{
						case 0: transform.translation.x = parsed; break;
						case 1: transform.translation.y = parsed; break;
						case 2: transform.translation.z = parsed; break;
						default: break;
					}
					break;
				}
				case RowDescriptor::Kind::TransformRotation:
				{
					virasa::math::Transform& transform = world.GetTransformComponent(entity);
					glm::vec3 eulerDeg = DecomposeQuatToEulerDegreesXYZ(transform.rotation);
					switch (componentIndexFromSlot())
					{
						case 0: eulerDeg.x = parsed; break;
						case 1: eulerDeg.y = parsed; break;
						case 2: eulerDeg.z = parsed; break;
						default: break;
					}
					transform.rotation = ComposeEulerDegreesXYZToQuat(eulerDeg);
					break;
				}
				case RowDescriptor::Kind::TransformScale:
				{
					virasa::math::Transform& transform = world.GetTransformComponent(entity);
					switch (componentIndexFromSlot())
					{
						case 0: transform.scale.x = parsed; break;
						case 1: transform.scale.y = parsed; break;
						case 2: transform.scale.z = parsed; break;
						default: break;
					}
					break;
				}
				case RowDescriptor::Kind::DirectionalLightDirection:
				{
					auto& component = world.GetDirectionalLightComponent(entity);
					switch (componentIndexFromSlot())
					{
						case 0: component.direction.x = parsed; break;
						case 1: component.direction.y = parsed; break;
						case 2: component.direction.z = parsed; break;
						default: break;
					}
					break;
				}
				case RowDescriptor::Kind::DirectionalLightColor:
				{
					auto& component = world.GetDirectionalLightComponent(entity);
					switch (componentIndexFromSlot())
					{
						case 0: component.color.x = parsed; break;
						case 1: component.color.y = parsed; break;
						case 2: component.color.z = parsed; break;
						default: break;
					}
					break;
				}
				case RowDescriptor::Kind::DirectionalLightIntensity:
					world.GetDirectionalLightComponent(entity).intensity = parsed;
					break;
				case RowDescriptor::Kind::PointLightColor:
				{
					auto& component = world.GetPointLightComponent(entity);
					switch (componentIndexFromSlot())
					{
						case 0: component.color.x = parsed; break;
						case 1: component.color.y = parsed; break;
						case 2: component.color.z = parsed; break;
						default: break;
					}
					break;
				}
				case RowDescriptor::Kind::PointLightIntensity:
					world.GetPointLightComponent(entity).intensity = parsed;
					break;
				case RowDescriptor::Kind::PointLightRange:
					world.GetPointLightComponent(entity).range = parsed;
					break;
				case RowDescriptor::Kind::SpotLightColor:
				{
					auto& component = world.GetSpotLightComponent(entity);
					switch (componentIndexFromSlot())
					{
						case 0: component.color.x = parsed; break;
						case 1: component.color.y = parsed; break;
						case 2: component.color.z = parsed; break;
						default: break;
					}
					break;
				}
				case RowDescriptor::Kind::SpotLightIntensity:
					world.GetSpotLightComponent(entity).intensity = parsed;
					break;
				case RowDescriptor::Kind::SpotLightRange:
					world.GetSpotLightComponent(entity).range = parsed;
					break;
				case RowDescriptor::Kind::SpotLightInnerConeCos:
					world.GetSpotLightComponent(entity).innerConeCos = parsed;
					break;
				case RowDescriptor::Kind::SpotLightOuterConeCos:
					world.GetSpotLightComponent(entity).outerConeCos = parsed;
					break;
				case RowDescriptor::Kind::CameraFovY:
					world.GetCameraComponent(entity).fovY = parsed;
					break;
				case RowDescriptor::Kind::CameraAspect:
					world.GetCameraComponent(entity).aspect = parsed;
					break;
				case RowDescriptor::Kind::CameraNearPlane:
					world.GetCameraComponent(entity).nearPlane = parsed;
					break;
				case RowDescriptor::Kind::CameraFarPlane:
					world.GetCameraComponent(entity).farPlane = parsed;
					break;
				case RowDescriptor::Kind::MeshId:
				case RowDescriptor::Kind::VisualMaterialId:
				case RowDescriptor::Kind::CameraDomain:
				case RowDescriptor::Kind::Name:
				case RowDescriptor::Kind::SectionLabel:
					break;
			}
		}
	}

	view._editing = false;
	view._editBuffer.clear();
}

virasa::ui::EntityEditorPanel& EntityEditorView::GetPanel() noexcept
{
	return _panel;
}

const virasa::ui::EntityEditorPanel& EntityEditorView::GetPanel() const noexcept
{
	return _panel;
}

std::size_t EntityEditorView::GetCursorRow() const noexcept
{
	return _cursorRow;
}

std::size_t EntityEditorView::GetCursorCell() const noexcept
{
	return _cursorCell;
}

bool EntityEditorView::IsEditing() const noexcept
{
	return _editing;
}

EntityEditorViewKeyResult EntityEditorView::HandleKey(
	virasa::KeyCode key,
	virasa::ecs::World& world,
	virasa::ecs::Entity entity)
{
	if (!_editing)
	{
		if (key == virasa::KeyCode::Enter)
		{
			if (entity != virasa::ecs::Entity::Invalid() && world.IsValid(entity))
			{
				BuiltRows built = BuildRows(world, entity, *this);
				ClampCursorToBuiltRows(*this, built);
				EnterEditMode(*this, built, world, entity);
			}
			else
			{
				_pendingG = false;
			}
			return EntityEditorViewKeyResult::Consumed;
		}
		if (key == virasa::KeyCode::Escape)
		{
			_pendingG = false;
			return EntityEditorViewKeyResult::RequestHierarchy;
		}
		_pendingG = false;
		return EntityEditorViewKeyResult::Consumed;
	}

	if (key == virasa::KeyCode::Enter)
	{
		CommitEdit(*this, world, entity);
		_pendingG = false;
		return EntityEditorViewKeyResult::Consumed;
	}
	if (key == virasa::KeyCode::Escape)
	{
		_editing = false;
		_editBuffer.clear();
		_pendingG = false;
		return EntityEditorViewKeyResult::Consumed;
	}
	if (key == virasa::KeyCode::Backspace)
	{
		RemoveLastUtf8Codepoint(_editBuffer);
		return EntityEditorViewKeyResult::Consumed;
	}
	_pendingG = false;
	return EntityEditorViewKeyResult::Consumed;
}

EntityEditorViewKeyResult EntityEditorView::HandleTextInput(
	std::string_view utf8,
	virasa::ecs::World& world,
	virasa::ecs::Entity entity)
{
	if (_editing)
	{
		_editBuffer.append(utf8.data(), utf8.size());
		return EntityEditorViewKeyResult::Consumed;
	}

	if (entity == virasa::ecs::Entity::Invalid() || !world.IsValid(entity))
	{
		_pendingG = false;
		return EntityEditorViewKeyResult::Consumed;
	}

	BuiltRows built = BuildRows(world, entity, *this);
	ClampCursorToBuiltRows(*this, built);
	bool requestCommandBar = false;
	const std::vector<char32_t> codepoints = DecodeUtf8(utf8);

	for (std::size_t i = 0; i < codepoints.size(); ++i)
	{
		char32_t cp = codepoints[i];
		if (_pendingG && cp != U'g')
		{
			_pendingG = false;
		}

		if (cp == U':')
		{
			_pendingG = false;
			requestCommandBar = true;
			break;
		}
		if (cp == U'g')
		{
			if (!_pendingG)
			{
				_pendingG = true;
			}
			else
			{
				if (built.rows.empty())
				{
					_cursorRow = 0u;
					_cursorCell = 0u;
				}
				else
				{
					_cursorRow = 0u;
					_cursorCell = FindFirstValueCell(built.rows[_cursorRow]);
				}
				_pendingG = false;
			}
			continue;
		}
		if (cp == U'G')
		{
			if (built.rows.empty())
			{
				_cursorRow = 0u;
				_cursorCell = 0u;
			}
			else
			{
				_cursorRow = built.rows.size() - 1u;
				_cursorCell = FindFirstValueCell(built.rows[_cursorRow]);
			}
			_pendingG = false;
			continue;
		}
		if (cp == U'j')
		{
			if (_cursorRow + 1u < built.rows.size())
			{
				++_cursorRow;
				_cursorCell = FindFirstValueCell(built.rows[_cursorRow]);
			}
			_pendingG = false;
			continue;
		}
		if (cp == U'k')
		{
			if (_cursorRow > 0u)
			{
				--_cursorRow;
				_cursorCell = FindFirstValueCell(built.rows[_cursorRow]);
			}
			_pendingG = false;
			continue;
		}
		if (cp == U'h')
		{
			const EntityEditorPanelRow& row = built.rows[_cursorRow];
			bool moved = false;
			for (std::size_t idx = _cursorCell; idx > 0u; --idx)
			{
				if (row.cells[idx - 1u].kind == EntityEditorCellKind::Value)
				{
					_cursorCell = idx - 1u;
					moved = true;
					break;
				}
			}
			if (!moved && !row.cells.empty() && _cursorCell == 0u && row.cells[0].kind == EntityEditorCellKind::SectionLabel)
			{
				const uint8_t sectionIndex = built.descriptors[_cursorRow].sectionIndex;
				if (_collapsed.count(sectionIndex) == 0u && SectionHasVisibleFieldRow(built, _cursorRow))
				{
					_collapsed.insert(sectionIndex);
				}
			}
			_pendingG = false;
			built = BuildRows(world, entity, *this);
			ClampCursorToBuiltRows(*this, built);
			continue;
		}
		if (cp == U'l')
		{
			const EntityEditorPanelRow& row = built.rows[_cursorRow];
			if (!row.cells.empty() && row.cells[0].kind == EntityEditorCellKind::SectionLabel)
			{
				const uint8_t sectionIndex = built.descriptors[_cursorRow].sectionIndex;
				if (_collapsed.count(sectionIndex) != 0u)
				{
					_collapsed.erase(sectionIndex);
					_pendingG = false;
					built = BuildRows(world, entity, *this);
					ClampCursorToBuiltRows(*this, built);
					continue;
				}
			}
			for (std::size_t idx = _cursorCell + 1u; idx < row.cells.size(); ++idx)
			{
				if (row.cells[idx].kind == EntityEditorCellKind::Value)
				{
					_cursorCell = idx;
					break;
				}
			}
			_pendingG = false;
			continue;
		}
		if (cp == U'i')
		{
			EnterEditMode(*this, built, world, entity);
			continue;
		}
		_pendingG = false;
	}

	return requestCommandBar ? EntityEditorViewKeyResult::RequestCommandBar
		: EntityEditorViewKeyResult::Consumed;
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
	if (entity == virasa::ecs::Entity::Invalid() || !world.IsValid(entity))
	{
		_panel.Render(
			out,
			std::span<const virasa::ui::EntityEditorPanelRow>{},
			std::numeric_limits<std::size_t>::max(),
			std::numeric_limits<std::size_t>::max(),
			atlas,
			x,
			y,
			width,
			height);
		return;
	}

	BuiltRows built = BuildRows(world, entity, *this);
	ClampCursorToBuiltRows(*this, built);
	OverlayEditBuffer(*this, built);
	_panel.Render(
		out,
		std::span<const virasa::ui::EntityEditorPanelRow>(built.rows),
		_cursorRow,
		_cursorCell,
		atlas,
		x,
		y,
		width,
		height);
}

} // namespace virasa::editor
