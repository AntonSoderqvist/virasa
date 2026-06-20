#include "virasa/sim/SceneSerializer.h"

#include "virasa/ecs/ComponentSystem.h"
#include "virasa/ecs/Types.h"
#include "virasa/ecs/World.h"
#include "virasa/sim/Builtins.h"

#include <cstdint>
#include <cstddef>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace virasa::sim
{
namespace
{

constexpr uint32_t kSceneSerializerFormatVersion = 1u;
constexpr const char* kFormatVersionField = "formatVersion";
constexpr const char* kNameField = "name";
constexpr const char* kBehaviorsField = "behaviors";
constexpr const char* kDefaultCameraField = "defaultCamera";
constexpr const char* kEntitiesField = "entities";
constexpr const char* kEntityIdField = "id";
constexpr const char* kEntityParentField = "parent";
constexpr const char* kEntityComponentsField = "components";

struct EntityRecord
{
	uint32_t id = 0u;
	virasa::ecs::Entity entity = virasa::ecs::Entity::Invalid();
};

[[nodiscard]] const uint32_t* FindSerializedId(
	virasa::ecs::Entity entity,
	const std::vector<std::pair<virasa::ecs::Entity, uint32_t>>& entityToId);

[[nodiscard]] virasa::ecs::Entity FindEntityBySerializedId(
	uint32_t id,
	const std::vector<EntityRecord>& idToEntity);

class SaveEntityResolver final : public virasa::sim::EntityResolver
{
public:
	explicit SaveEntityResolver(
		const std::vector<std::pair<virasa::ecs::Entity, uint32_t>>& entityToId)
		: _entityToId(entityToId)
	{
	}

	[[nodiscard]] int32_t StableIdForEntity(virasa::ecs::Entity entity) const override
	{
		if (entity == virasa::ecs::Entity::Invalid())
		{
			return -1;
		}

		const uint32_t* id = FindSerializedId(entity, _entityToId);
		if (id == nullptr || *id > static_cast<uint32_t>(std::numeric_limits<int32_t>::max()))
		{
			return -1;
		}

		return static_cast<int32_t>(*id);
	}

	[[nodiscard]] virasa::ecs::Entity EntityForStableId(int32_t stableId) const override
	{
		(void)stableId;
		return virasa::ecs::Entity::Invalid();
	}

private:
	const std::vector<std::pair<virasa::ecs::Entity, uint32_t>>& _entityToId;
};

class LoadEntityResolver final : public virasa::sim::EntityResolver
{
public:
	explicit LoadEntityResolver(const std::vector<EntityRecord>& idToEntity)
		: _idToEntity(idToEntity)
	{
	}

	[[nodiscard]] int32_t StableIdForEntity(virasa::ecs::Entity entity) const override
	{
		(void)entity;
		return -1;
	}

	[[nodiscard]] virasa::ecs::Entity EntityForStableId(int32_t stableId) const override
	{
		if (stableId < 0)
		{
			return virasa::ecs::Entity::Invalid();
		}

		return FindEntityBySerializedId(static_cast<uint32_t>(stableId), _idToEntity);
	}

private:
	const std::vector<EntityRecord>& _idToEntity;
};

void EmitEntity(
	const virasa::ecs::World& world,
	virasa::ecs::Entity entity,
	std::vector<virasa::ecs::Entity>& entities)
{
	entities.push_back(entity);

	for (virasa::ecs::Entity child : world.GetChildren(entity))
	{
		EmitEntity(world, child, entities);
	}
}

[[nodiscard]] std::vector<virasa::ecs::Entity> CollectEntitiesDepthFirst(
	const virasa::ecs::World& world)
{
	std::vector<virasa::ecs::Entity> entities;
	entities.reserve(world.GetEntityCount());

	for (virasa::ecs::Entity root : world.GetRoots())
	{
		EmitEntity(world, root, entities);
	}

	return entities;
}

[[nodiscard]] const uint32_t* FindSerializedId(
	virasa::ecs::Entity entity,
	const std::vector<std::pair<virasa::ecs::Entity, uint32_t>>& entityToId)
{
	for (const auto& [storedEntity, id] : entityToId)
	{
		if (storedEntity == entity)
		{
			return &id;
		}
	}

	return nullptr;
}

[[nodiscard]] virasa::ecs::Entity FindEntityBySerializedId(
	uint32_t id,
	const std::vector<EntityRecord>& idToEntity)
{
	for (const EntityRecord& record : idToEntity)
	{
		if (record.id == id)
		{
			return record.entity;
		}
	}

	return virasa::ecs::Entity::Invalid();
}

[[nodiscard]] bool ReadRequiredUint32(
	const nlohmann::json& json,
	const char* field,
	uint32_t& value)
{
	const auto it = json.find(field);
	if (it == json.end() || !it->is_number_unsigned())
	{
		return false;
	}

	const uint64_t number = it->get<uint64_t>();
	if (number > UINT32_MAX)
	{
		return false;
	}

	value = static_cast<uint32_t>(number);
	return true;
}

[[nodiscard]] bool ReadNullableUint32(const nlohmann::json& json, uint32_t& value)
{
	if (json.is_null())
	{
		value = UINT32_MAX;
		return true;
	}
	if (!json.is_number_unsigned())
	{
		return false;
	}

	const uint64_t number = json.get<uint64_t>();
	if (number > UINT32_MAX)
	{
		return false;
	}

	value = static_cast<uint32_t>(number);
	return true;
}

[[nodiscard]] bool ValidateBehaviorManifest(const nlohmann::json& json)
{
	const auto it = json.find(kBehaviorsField);
	if (it == json.end() || !it->is_array())
	{
		return false;
	}

	for (const nlohmann::json& behavior : *it)
	{
		if (!behavior.is_string())
		{
			return false;
		}
	}

	return true;
}

[[nodiscard]] bool ValidateDocumentEnvelope(const nlohmann::json& json)
{
	if (!json.is_object())
	{
		return false;
	}

	uint32_t version = 0u;
	if (!ReadRequiredUint32(json, kFormatVersionField, version))
	{
		return false;
	}
	if (version != kSceneSerializerFormatVersion)
	{
		return false;
	}

	const auto nameIt = json.find(kNameField);
	if (nameIt == json.end() || !nameIt->is_string())
	{
		return false;
	}

	if (!ValidateBehaviorManifest(json))
	{
		return false;
	}

	const auto defaultCameraIt = json.find(kDefaultCameraField);
	if (defaultCameraIt == json.end())
	{
		return false;
	}

	uint32_t defaultCamera = UINT32_MAX;
	if (!ReadNullableUint32(*defaultCameraIt, defaultCamera))
	{
		return false;
	}

	const auto entitiesIt = json.find(kEntitiesField);
	return entitiesIt != json.end() && entitiesIt->is_array();
}

[[nodiscard]] bool ValidateEntityRecordShape(const nlohmann::json& entity)
{
	if (!entity.is_object())
	{
		return false;
	}

	uint32_t id = 0u;
	if (!ReadRequiredUint32(entity, kEntityIdField, id))
	{
		return false;
	}

	const auto nameIt = entity.find(kNameField);
	if (nameIt == entity.end() || !nameIt->is_string())
	{
		return false;
	}

	const auto parentIt = entity.find(kEntityParentField);
	if (parentIt == entity.end())
	{
		return false;
	}

	uint32_t parent = UINT32_MAX;
	if (!ReadNullableUint32(*parentIt, parent))
	{
		return false;
	}

	const auto componentsIt = entity.find(kEntityComponentsField);
	return componentsIt != entity.end() && componentsIt->is_object();
}

[[nodiscard]] bool HasDuplicateSerializedId(
	uint32_t id,
	const std::vector<EntityRecord>& records)
{
	for (const EntityRecord& record : records)
	{
		if (record.id == id)
		{
			return true;
		}
	}

	return false;
}

} // namespace

nlohmann::json SerializeSceneToJson(
	const virasa::sim::Scene& scene,
	const virasa::sim::ComponentCodecRegistry& codecs,
	const virasa::sim::AssetCatalog& catalog)
{
	const virasa::ecs::World& world = scene.GetWorld();
	const std::vector<virasa::ecs::Entity> entities = CollectEntitiesDepthFirst(world);

	std::vector<std::pair<virasa::ecs::Entity, uint32_t>> entityToId;
	entityToId.reserve(entities.size());
	for (size_t i = 0u; i < entities.size(); ++i)
	{
		entityToId.emplace_back(entities[i], static_cast<uint32_t>(i));
	}
	const SaveEntityResolver entityResolver(entityToId);
	const virasa::sim::SerializationContext context(catalog, entityResolver);

	nlohmann::json document = nlohmann::json::object();
	document[kFormatVersionField] = kSceneSerializerFormatVersion;
	document[kNameField] = std::string(scene.GetName());

	nlohmann::json behaviors = nlohmann::json::array();
	for (const std::string& behavior : scene.Behaviors())
	{
		behaviors.push_back(behavior);
	}
	document[kBehaviorsField] = std::move(behaviors);

	if (const uint32_t* defaultCameraId = FindSerializedId(scene.GetDefaultCamera(), entityToId))
	{
		document[kDefaultCameraField] = *defaultCameraId;
	}
	else
	{
		document[kDefaultCameraField] = nullptr;
	}

	const std::vector<std::string> componentNames = codecs.Names();
	nlohmann::json entityArray = nlohmann::json::array();
	for (size_t i = 0u; i < entities.size(); ++i)
	{
		const virasa::ecs::Entity entity = entities[i];

		nlohmann::json entityJson = nlohmann::json::object();
		entityJson[kEntityIdField] = static_cast<uint32_t>(i);
		entityJson[kNameField] = world.HasNameComponent(entity)
			? world.GetNameComponent(entity).name
			: std::string();

		const virasa::ecs::Entity parent = world.GetParent(entity);
		if (const uint32_t* parentId = FindSerializedId(parent, entityToId))
		{
			entityJson[kEntityParentField] = *parentId;
		}
		else
		{
			entityJson[kEntityParentField] = nullptr;
		}

		nlohmann::json components = nlohmann::json::object();
		for (const std::string& componentName : componentNames)
		{
			const virasa::sim::ComponentCodec* codec = codecs.Find(componentName);
			if (codec == nullptr)
			{
				continue;
			}

			const virasa::ecs::ComponentId systemId = world.GetSystemId(componentName);
			if (systemId == virasa::ecs::kInvalidComponentId)
			{
				continue;
			}

			const virasa::ecs::ComponentSystem& system = world.GetSystem(systemId);
			if (!system.Has(entity))
			{
				continue;
			}

			components[componentName] = codec->ToJson(system.GetRaw(entity), context);
		}

		entityJson[kEntityComponentsField] = std::move(components);
		entityArray.push_back(std::move(entityJson));
	}

	document[kEntitiesField] = std::move(entityArray);
	return document;
}

std::optional<virasa::sim::Scene> DeserializeSceneFromJson(
	const nlohmann::json& json,
	const virasa::sim::ComponentCodecRegistry& codecs,
	const virasa::sim::AssetCatalog& catalog)
{
	if (!ValidateDocumentEnvelope(json))
	{
		return std::nullopt;
	}

	const nlohmann::json& entitiesJson = json.at(kEntitiesField);
	for (const nlohmann::json& entityJson : entitiesJson)
	{
		if (!ValidateEntityRecordShape(entityJson))
		{
			return std::nullopt;
		}
	}

	virasa::sim::Scene scene;
	scene.SetName(json.at(kNameField).get<std::string>());
	for (const nlohmann::json& behavior : json.at(kBehaviorsField))
	{
		scene.AddBehavior(behavior.get<std::string>());
	}

	virasa::ecs::World& world = scene.GetWorld();
	virasa::sim::RegisterGameplayComponents(world);

	std::vector<EntityRecord> idToEntity;
	idToEntity.reserve(entitiesJson.size());
	for (const nlohmann::json& entityJson : entitiesJson)
	{
		uint32_t id = 0u;
		(void)ReadRequiredUint32(entityJson, kEntityIdField, id);
		if (HasDuplicateSerializedId(id, idToEntity))
		{
			return std::nullopt;
		}

		virasa::ecs::Entity entity = world.CreateEntity(entityJson.at(kNameField).get<std::string>());
		idToEntity.push_back({id, entity});
	}

	for (const nlohmann::json& entityJson : entitiesJson)
	{
		uint32_t id = 0u;
		(void)ReadRequiredUint32(entityJson, kEntityIdField, id);
		const virasa::ecs::Entity entity = FindEntityBySerializedId(id, idToEntity);

		uint32_t parentId = UINT32_MAX;
		(void)ReadNullableUint32(entityJson.at(kEntityParentField), parentId);
		if (parentId != UINT32_MAX)
		{
			const virasa::ecs::Entity parent = FindEntityBySerializedId(parentId, idToEntity);
			if (parent == virasa::ecs::Entity::Invalid())
			{
				return std::nullopt;
			}

			if (world.SetParent(entity, parent) != virasa::ecs::EcsError::None)
			{
				return std::nullopt;
			}
		}
	}

	const LoadEntityResolver entityResolver(idToEntity);
	const virasa::sim::SerializationContext context(catalog, entityResolver);

	for (const nlohmann::json& entityJson : entitiesJson)
	{
		uint32_t id = 0u;
		(void)ReadRequiredUint32(entityJson, kEntityIdField, id);
		const virasa::ecs::Entity entity = FindEntityBySerializedId(id, idToEntity);
		const nlohmann::json& components = entityJson.at(kEntityComponentsField);

		for (auto it = components.begin(); it != components.end(); ++it)
		{
			const std::string& componentName = it.key();
			const virasa::sim::ComponentCodec* codec = codecs.Find(componentName);
			if (codec == nullptr)
			{
				continue;
			}

			const virasa::ecs::ComponentId systemId = world.GetSystemId(componentName);
			if (systemId == virasa::ecs::kInvalidComponentId)
			{
				continue;
			}

			std::vector<std::byte> scratch(codec->ElementSize());
			if (!codec->FromJson(*it, context, scratch.data()))
			{
				continue;
			}

			virasa::ecs::ComponentSystem& system = world.GetSystem(systemId);
			(void)system.AddRaw(entity, scratch.data());
		}
	}

	uint32_t defaultCameraId = UINT32_MAX;
	(void)ReadNullableUint32(json.at(kDefaultCameraField), defaultCameraId);
	scene.SetDefaultCamera(FindEntityBySerializedId(defaultCameraId, idToEntity));

	return scene;
}

std::string SerializeScene(
	const virasa::sim::Scene& scene,
	const virasa::sim::ComponentCodecRegistry& codecs,
	const virasa::sim::AssetCatalog& catalog)
{
	return SerializeSceneToJson(scene, codecs, catalog).dump();
}

std::optional<virasa::sim::Scene> DeserializeScene(
	std::string_view text,
	const virasa::sim::ComponentCodecRegistry& codecs,
	const virasa::sim::AssetCatalog& catalog)
{
	const nlohmann::json json = nlohmann::json::parse(
		text.begin(),
		text.end(),
		nullptr,
		false);
	if (json.is_discarded())
	{
		return std::nullopt;
	}

	return DeserializeSceneFromJson(json, codecs, catalog);
}

} // namespace virasa::sim
