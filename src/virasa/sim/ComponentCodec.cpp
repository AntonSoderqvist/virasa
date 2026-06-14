#include "virasa/sim/ComponentCodec.h"

#include "virasa/ecs/Components.h"
#include "virasa/math/Transform.h"
#include "virasa/math/Types.h"
#include "virasa/renderer/Types.h"
#include "virasa/sim/GameplayComponents.h"

#include <cstdint>
#include <utility>

namespace virasa::sim
{
namespace
{

[[nodiscard]] nlohmann::json Vec3ToJson(const virasa::math::Vec3& value)
{
	return nlohmann::json::array({value.x, value.y, value.z});
}

[[nodiscard]] nlohmann::json QuatToJson(const virasa::math::Quat& value)
{
	return nlohmann::json::array({value.w, value.x, value.y, value.z});
}

[[nodiscard]] bool JsonToVec3(const nlohmann::json& json, virasa::math::Vec3& value)
{
	if (!json.is_array() || json.size() != 3u)
	{
		return false;
	}

	for (size_t i = 0u; i < 3u; ++i)
	{
		if (!json[i].is_number())
		{
			return false;
		}
	}

	value = virasa::math::Vec3(
		json[0].get<float>(),
		json[1].get<float>(),
		json[2].get<float>());
	return true;
}

[[nodiscard]] bool JsonToQuat(const nlohmann::json& json, virasa::math::Quat& value)
{
	if (!json.is_array() || json.size() != 4u)
	{
		return false;
	}

	for (size_t i = 0u; i < 4u; ++i)
	{
		if (!json[i].is_number())
		{
			return false;
		}
	}

	value = virasa::math::Quat(
		json[0].get<float>(),
		json[1].get<float>(),
		json[2].get<float>(),
		json[3].get<float>());
	return true;
}

template<typename ValueType>
[[nodiscard]] bool ReadField(const nlohmann::json& json, const char* name, ValueType& value)
{
	const auto it = json.find(name);
	if (it == json.end())
	{
		return true;
	}
	if (!it->is_number())
	{
		return false;
	}

	value = it->get<ValueType>();
	return true;
}

[[nodiscard]] bool ReadField(const nlohmann::json& json, const char* name, bool& value)
{
	const auto it = json.find(name);
	if (it == json.end())
	{
		return true;
	}
	if (!it->is_boolean())
	{
		return false;
	}

	value = it->get<bool>();
	return true;
}

[[nodiscard]] bool ReadField(
	const nlohmann::json& json,
	const char* name,
	virasa::math::Vec3& value)
{
	const auto it = json.find(name);
	if (it == json.end())
	{
		return true;
	}

	return JsonToVec3(*it, value);
}

[[nodiscard]] bool ReadField(
	const nlohmann::json& json,
	const char* name,
	virasa::math::Quat& value)
{
	const auto it = json.find(name);
	if (it == json.end())
	{
		return true;
	}

	return JsonToQuat(*it, value);
}

[[nodiscard]] bool ReadAssetField(
	const nlohmann::json& json,
	const char* name,
	virasa::sim::AssetKind kind,
	const virasa::sim::AssetCatalog& catalog,
	uint32_t& id)
{
	const auto it = json.find(name);
	if (it == json.end() || it->is_null())
	{
		id = virasa::sim::kInvalidAssetId;
		return true;
	}
	if (!it->is_string())
	{
		return false;
	}

	id = catalog.IdForKey(kind, it->get<std::string>());
	return true;
}

void WriteAssetField(
	nlohmann::json& json,
	const char* name,
	virasa::sim::AssetKind kind,
	uint32_t id,
	const virasa::sim::AssetCatalog& catalog)
{
	const std::string_view key = catalog.KeyForId(kind, id);
	if (key.empty())
	{
		json[name] = nullptr;
		return;
	}

	json[name] = std::string(key);
}

template<typename ComponentType>
class ConcreteComponentCodec : public virasa::sim::ComponentCodec
{
public:
	explicit ConcreteComponentCodec(std::string_view name) noexcept
		: _name(name)
	{
	}

	[[nodiscard]] std::string_view ComponentName() const noexcept override
	{
		return _name;
	}

	[[nodiscard]] size_t ElementSize() const noexcept override
	{
		return sizeof(ComponentType);
	}

private:
	std::string_view _name;
};

class TransformCodec final : public ConcreteComponentCodec<virasa::math::Transform>
{
public:
	TransformCodec()
		: ConcreteComponentCodec("Transform")
	{
	}

	[[nodiscard]] nlohmann::json ToJson(
		const void* component,
		const virasa::sim::AssetCatalog& /*catalog*/) const override
	{
		const auto& transform = *static_cast<const virasa::math::Transform*>(component);
		return {
			{"translation", Vec3ToJson(transform.translation)},
			{"rotation", QuatToJson(transform.rotation)},
			{"scale", Vec3ToJson(transform.scale)},
		};
	}

	bool FromJson(
		const nlohmann::json& json,
		const virasa::sim::AssetCatalog& /*catalog*/,
		void* dst) const override
	{
		virasa::math::Transform transform{};
		if (!json.is_object() ||
			!ReadField(json, "translation", transform.translation) ||
			!ReadField(json, "rotation", transform.rotation) ||
			!ReadField(json, "scale", transform.scale))
		{
			*static_cast<virasa::math::Transform*>(dst) = virasa::math::Transform{};
			return false;
		}

		*static_cast<virasa::math::Transform*>(dst) = transform;
		return true;
	}
};

class MeshCodec final : public ConcreteComponentCodec<virasa::ecs::MeshComponent>
{
public:
	MeshCodec()
		: ConcreteComponentCodec("Mesh")
	{
	}

	[[nodiscard]] nlohmann::json ToJson(
		const void* component,
		const virasa::sim::AssetCatalog& catalog) const override
	{
		const auto& mesh = *static_cast<const virasa::ecs::MeshComponent*>(component);
		nlohmann::json json = nlohmann::json::object();
		WriteAssetField(json, "meshId", virasa::sim::AssetKind::Mesh, mesh.meshId, catalog);
		return json;
	}

	bool FromJson(
		const nlohmann::json& json,
		const virasa::sim::AssetCatalog& catalog,
		void* dst) const override
	{
		virasa::ecs::MeshComponent mesh{};
		if (!json.is_object())
		{
			*static_cast<virasa::ecs::MeshComponent*>(dst) = virasa::ecs::MeshComponent{};
			return false;
		}

		if (!ReadAssetField(json, "meshId", virasa::sim::AssetKind::Mesh, catalog, mesh.meshId))
		{
			*static_cast<virasa::ecs::MeshComponent*>(dst) = virasa::ecs::MeshComponent{};
			return false;
		}

		*static_cast<virasa::ecs::MeshComponent*>(dst) = mesh;
		return true;
	}
};

class VisualCodec final : public ConcreteComponentCodec<virasa::ecs::VisualComponent>
{
public:
	VisualCodec()
		: ConcreteComponentCodec("Visual")
	{
	}

	[[nodiscard]] nlohmann::json ToJson(
		const void* component,
		const virasa::sim::AssetCatalog& catalog) const override
	{
		const auto& visual = *static_cast<const virasa::ecs::VisualComponent*>(component);
		nlohmann::json json = nlohmann::json::object();
		WriteAssetField(
			json,
			"materialId",
			virasa::sim::AssetKind::Material,
			visual.materialId,
			catalog);
		return json;
	}

	bool FromJson(
		const nlohmann::json& json,
		const virasa::sim::AssetCatalog& catalog,
		void* dst) const override
	{
		virasa::ecs::VisualComponent visual{};
		if (!json.is_object())
		{
			*static_cast<virasa::ecs::VisualComponent*>(dst) = virasa::ecs::VisualComponent{};
			return false;
		}

		if (!ReadAssetField(
			json,
			"materialId",
			virasa::sim::AssetKind::Material,
			catalog,
			visual.materialId))
		{
			*static_cast<virasa::ecs::VisualComponent*>(dst) =
				virasa::ecs::VisualComponent{};
			return false;
		}

		*static_cast<virasa::ecs::VisualComponent*>(dst) = visual;
		return true;
	}
};

class DirectionalLightCodec final :
	public ConcreteComponentCodec<virasa::ecs::DirectionalLightComponent>
{
public:
	DirectionalLightCodec()
		: ConcreteComponentCodec("DirectionalLight")
	{
	}

	[[nodiscard]] nlohmann::json ToJson(
		const void* component,
		const virasa::sim::AssetCatalog& /*catalog*/) const override
	{
		const auto& light =
			*static_cast<const virasa::ecs::DirectionalLightComponent*>(component);
		return {
			{"direction", Vec3ToJson(light.direction)},
			{"color", Vec3ToJson(light.color)},
			{"intensity", light.intensity},
			{"castsShadows", light.castsShadows},
		};
	}

	bool FromJson(
		const nlohmann::json& json,
		const virasa::sim::AssetCatalog& /*catalog*/,
		void* dst) const override
	{
		virasa::ecs::DirectionalLightComponent light{};
		if (!json.is_object() ||
			!ReadField(json, "direction", light.direction) ||
			!ReadField(json, "color", light.color) ||
			!ReadField(json, "intensity", light.intensity) ||
			!ReadField(json, "castsShadows", light.castsShadows))
		{
			*static_cast<virasa::ecs::DirectionalLightComponent*>(dst) =
				virasa::ecs::DirectionalLightComponent{};
			return false;
		}

		*static_cast<virasa::ecs::DirectionalLightComponent*>(dst) = light;
		return true;
	}
};

class PointLightCodec final : public ConcreteComponentCodec<virasa::ecs::PointLightComponent>
{
public:
	PointLightCodec()
		: ConcreteComponentCodec("PointLight")
	{
	}

	[[nodiscard]] nlohmann::json ToJson(
		const void* component,
		const virasa::sim::AssetCatalog& /*catalog*/) const override
	{
		const auto& light = *static_cast<const virasa::ecs::PointLightComponent*>(component);
		return {
			{"color", Vec3ToJson(light.color)},
			{"intensity", light.intensity},
			{"range", light.range},
		};
	}

	bool FromJson(
		const nlohmann::json& json,
		const virasa::sim::AssetCatalog& /*catalog*/,
		void* dst) const override
	{
		virasa::ecs::PointLightComponent light{};
		if (!json.is_object() ||
			!ReadField(json, "color", light.color) ||
			!ReadField(json, "intensity", light.intensity) ||
			!ReadField(json, "range", light.range))
		{
			*static_cast<virasa::ecs::PointLightComponent*>(dst) =
				virasa::ecs::PointLightComponent{};
			return false;
		}

		*static_cast<virasa::ecs::PointLightComponent*>(dst) = light;
		return true;
	}
};

class SpotLightCodec final : public ConcreteComponentCodec<virasa::ecs::SpotLightComponent>
{
public:
	SpotLightCodec()
		: ConcreteComponentCodec("SpotLight")
	{
	}

	[[nodiscard]] nlohmann::json ToJson(
		const void* component,
		const virasa::sim::AssetCatalog& /*catalog*/) const override
	{
		const auto& light = *static_cast<const virasa::ecs::SpotLightComponent*>(component);
		return {
			{"color", Vec3ToJson(light.color)},
			{"intensity", light.intensity},
			{"range", light.range},
			{"innerConeCos", light.innerConeCos},
			{"outerConeCos", light.outerConeCos},
			{"castsShadows", light.castsShadows},
		};
	}

	bool FromJson(
		const nlohmann::json& json,
		const virasa::sim::AssetCatalog& /*catalog*/,
		void* dst) const override
	{
		virasa::ecs::SpotLightComponent light{};
		if (!json.is_object() ||
			!ReadField(json, "color", light.color) ||
			!ReadField(json, "intensity", light.intensity) ||
			!ReadField(json, "range", light.range) ||
			!ReadField(json, "innerConeCos", light.innerConeCos) ||
			!ReadField(json, "outerConeCos", light.outerConeCos) ||
			!ReadField(json, "castsShadows", light.castsShadows))
		{
			*static_cast<virasa::ecs::SpotLightComponent*>(dst) =
				virasa::ecs::SpotLightComponent{};
			return false;
		}

		*static_cast<virasa::ecs::SpotLightComponent*>(dst) = light;
		return true;
	}
};

class CameraCodec final : public ConcreteComponentCodec<virasa::ecs::CameraComponent>
{
public:
	CameraCodec()
		: ConcreteComponentCodec("Camera")
	{
	}

	[[nodiscard]] nlohmann::json ToJson(
		const void* component,
		const virasa::sim::AssetCatalog& /*catalog*/) const override
	{
		const auto& camera = *static_cast<const virasa::ecs::CameraComponent*>(component);
		return {
			{"domain", static_cast<int32_t>(camera.domain)},
			{"fovY", camera.fovY},
			{"aspect", camera.aspect},
			{"nearPlane", camera.nearPlane},
			{"farPlane", camera.farPlane},
		};
	}

	bool FromJson(
		const nlohmann::json& json,
		const virasa::sim::AssetCatalog& /*catalog*/,
		void* dst) const override
	{
		virasa::ecs::CameraComponent camera{};
		int32_t domain = static_cast<int32_t>(camera.domain);
		if (!json.is_object() ||
			!ReadField(json, "domain", domain) ||
			!ReadField(json, "fovY", camera.fovY) ||
			!ReadField(json, "aspect", camera.aspect) ||
			!ReadField(json, "nearPlane", camera.nearPlane) ||
			!ReadField(json, "farPlane", camera.farPlane))
		{
			*static_cast<virasa::ecs::CameraComponent*>(dst) =
				virasa::ecs::CameraComponent{};
			return false;
		}

		camera.domain = static_cast<virasa::CameraDomain>(domain);
		*static_cast<virasa::ecs::CameraComponent*>(dst) = camera;
		return true;
	}
};

class HighlightCodec final : public ConcreteComponentCodec<virasa::ecs::HighlightComponent>
{
public:
	HighlightCodec()
		: ConcreteComponentCodec("Highlight")
	{
	}

	[[nodiscard]] nlohmann::json ToJson(
		const void* component,
		const virasa::sim::AssetCatalog& /*catalog*/) const override
	{
		const auto& highlight =
			*static_cast<const virasa::ecs::HighlightComponent*>(component);
		return {
			{"color", Vec3ToJson(highlight.color)},
			{"intensity", highlight.intensity},
			{"priority", highlight.priority},
		};
	}

	bool FromJson(
		const nlohmann::json& json,
		const virasa::sim::AssetCatalog& /*catalog*/,
		void* dst) const override
	{
		virasa::ecs::HighlightComponent highlight{};
		if (!json.is_object() ||
			!ReadField(json, "color", highlight.color) ||
			!ReadField(json, "intensity", highlight.intensity) ||
			!ReadField(json, "priority", highlight.priority))
		{
			*static_cast<virasa::ecs::HighlightComponent*>(dst) =
				virasa::ecs::HighlightComponent{};
			return false;
		}

		*static_cast<virasa::ecs::HighlightComponent*>(dst) = highlight;
		return true;
	}
};

class SpinCodec final : public ConcreteComponentCodec<virasa::sim::SpinComponent>
{
public:
	SpinCodec()
		: ConcreteComponentCodec("Spin")
	{
	}

	[[nodiscard]] nlohmann::json ToJson(
		const void* component,
		const virasa::sim::AssetCatalog& /*catalog*/) const override
	{
		const auto& spin = *static_cast<const virasa::sim::SpinComponent*>(component);
		return {
			{"angularVelocity", Vec3ToJson(spin.angularVelocity)},
		};
	}

	bool FromJson(
		const nlohmann::json& json,
		const virasa::sim::AssetCatalog& /*catalog*/,
		void* dst) const override
	{
		virasa::sim::SpinComponent spin{};
		if (!json.is_object() || !ReadField(json, "angularVelocity", spin.angularVelocity))
		{
			*static_cast<virasa::sim::SpinComponent*>(dst) =
				virasa::sim::SpinComponent{};
			return false;
		}

		*static_cast<virasa::sim::SpinComponent*>(dst) = spin;
		return true;
	}
};

} // namespace

void ComponentCodecRegistry::Register(std::unique_ptr<virasa::sim::ComponentCodec> codec)
{
	_codecs.push_back(std::move(codec));
}

const virasa::sim::ComponentCodec* ComponentCodecRegistry::Find(std::string_view name) const
{
	for (const std::unique_ptr<virasa::sim::ComponentCodec>& codec : _codecs)
	{
		if (codec->ComponentName() == name)
		{
			return codec.get();
		}
	}

	return nullptr;
}

bool ComponentCodecRegistry::Contains(std::string_view name) const
{
	return Find(name) != nullptr;
}

size_t ComponentCodecRegistry::Size() const noexcept
{
	return _codecs.size();
}

std::vector<std::string> ComponentCodecRegistry::Names() const
{
	std::vector<std::string> names;
	names.reserve(_codecs.size());
	for (const std::unique_ptr<virasa::sim::ComponentCodec>& codec : _codecs)
	{
		names.emplace_back(codec->ComponentName());
	}

	return names;
}

void RegisterBuiltinComponentCodecs(virasa::sim::ComponentCodecRegistry& registry)
{
	registry.Register(std::make_unique<TransformCodec>());
	registry.Register(std::make_unique<MeshCodec>());
	registry.Register(std::make_unique<VisualCodec>());
	registry.Register(std::make_unique<DirectionalLightCodec>());
	registry.Register(std::make_unique<PointLightCodec>());
	registry.Register(std::make_unique<SpotLightCodec>());
	registry.Register(std::make_unique<CameraCodec>());
	registry.Register(std::make_unique<HighlightCodec>());
	registry.Register(std::make_unique<SpinCodec>());
}

} // namespace virasa::sim
