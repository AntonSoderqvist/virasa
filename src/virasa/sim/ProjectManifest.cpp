#include "virasa/sim/ProjectManifest.h"

#include <cstdint>
#include <utility>

namespace virasa::sim
{
namespace
{

constexpr uint32_t kProjectManifestFormatVersion = 1u;
constexpr const char* kFormatVersionField = "formatVersion";
constexpr const char* kNameField = "name";
constexpr const char* kStartupSceneField = "startupScene";
constexpr const char* kAssetsField = "assets";
constexpr const char* kKindField = "kind";
constexpr const char* kKeyField = "key";
constexpr const char* kSourceField = "source";
constexpr const char* kMeshKindToken = "mesh";
constexpr const char* kMaterialKindToken = "material";

[[nodiscard]] const char* AssetKindToToken(virasa::sim::AssetKind kind) noexcept
{
	switch (kind)
	{
	case virasa::sim::AssetKind::Mesh:
		return kMeshKindToken;
	case virasa::sim::AssetKind::Material:
		return kMaterialKindToken;
	}

	return kMeshKindToken;
}

[[nodiscard]] std::optional<virasa::sim::AssetKind> AssetKindFromToken(
	std::string_view token) noexcept
{
	if (token == kMeshKindToken)
	{
		return virasa::sim::AssetKind::Mesh;
	}
	if (token == kMaterialKindToken)
	{
		return virasa::sim::AssetKind::Material;
	}

	return std::nullopt;
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

	return version == kProjectManifestFormatVersion;
}

} // namespace

nlohmann::json SerializeManifestToJson(const virasa::sim::ProjectManifest& manifest)
{
	nlohmann::json document = nlohmann::json::object();
	document[kFormatVersionField] = kProjectManifestFormatVersion;
	document[kNameField] = manifest.name;
	document[kStartupSceneField] = manifest.startupScene;

	nlohmann::json assets = nlohmann::json::array();
	for (const virasa::sim::ProjectAssetEntry& entry : manifest.assets)
	{
		nlohmann::json asset = nlohmann::json::object();
		asset[kKindField] = AssetKindToToken(entry.kind);
		asset[kKeyField] = entry.key;
		asset[kSourceField] = entry.source;
		assets.push_back(std::move(asset));
	}
	document[kAssetsField] = std::move(assets);

	return document;
}

std::optional<virasa::sim::ProjectManifest> DeserializeManifestFromJson(
	const nlohmann::json& json)
{
	if (!ValidateDocumentEnvelope(json))
	{
		return std::nullopt;
	}

	virasa::sim::ProjectManifest manifest;

	const auto nameIt = json.find(kNameField);
	if (nameIt != json.end() && nameIt->is_string())
	{
		manifest.name = nameIt->get<std::string>();
	}

	const auto startupSceneIt = json.find(kStartupSceneField);
	if (startupSceneIt != json.end() && startupSceneIt->is_string())
	{
		manifest.startupScene = startupSceneIt->get<std::string>();
	}

	const auto assetsIt = json.find(kAssetsField);
	if (assetsIt != json.end() && assetsIt->is_array())
	{
		manifest.assets.reserve(assetsIt->size());
		for (const nlohmann::json& assetJson : *assetsIt)
		{
			if (!assetJson.is_object())
			{
				continue;
			}

			const auto kindIt = assetJson.find(kKindField);
			if (kindIt == assetJson.end() || !kindIt->is_string())
			{
				continue;
			}

			const std::optional<virasa::sim::AssetKind> kind =
				AssetKindFromToken(kindIt->get<std::string>());
			if (!kind.has_value())
			{
				continue;
			}

			virasa::sim::ProjectAssetEntry entry;
			entry.kind = *kind;

			const auto keyIt = assetJson.find(kKeyField);
			if (keyIt != assetJson.end() && keyIt->is_string())
			{
				entry.key = keyIt->get<std::string>();
			}

			const auto sourceIt = assetJson.find(kSourceField);
			if (sourceIt != assetJson.end() && sourceIt->is_string())
			{
				entry.source = sourceIt->get<std::string>();
			}

			manifest.assets.push_back(std::move(entry));
		}
	}

	return manifest;
}

std::string SerializeManifest(const virasa::sim::ProjectManifest& manifest)
{
	return SerializeManifestToJson(manifest).dump();
}

std::optional<virasa::sim::ProjectManifest> DeserializeManifest(std::string_view text)
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

	return DeserializeManifestFromJson(json);
}

} // namespace virasa::sim
