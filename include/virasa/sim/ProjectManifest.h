#ifndef VIRASA_SIM_PROJECTMANIFEST_H
#define VIRASA_SIM_PROJECTMANIFEST_H

#include "json.hpp"
#include "virasa/sim/AssetCatalog.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace virasa::sim
{

struct ProjectAssetEntry
{
	virasa::sim::AssetKind kind = virasa::sim::AssetKind::Mesh;
	std::string key;
	std::string source;
};

struct ProjectManifest
{
	std::string name;
	std::string startupScene;
	std::vector<virasa::sim::ProjectAssetEntry> assets;
};

/**
 * @brief Serialize a project manifest to its structured JSON document form.
 * @param manifest Manifest to serialize.
 * @return JSON document describing the manifest.
 */
[[nodiscard]] nlohmann::json SerializeManifestToJson(
	const virasa::sim::ProjectManifest& manifest);

/**
 * @brief Deserialize a project manifest from its structured JSON document form.
 * @param json JSON document to read.
 * @return Reconstructed manifest, or std::nullopt when the document version is unsupported.
 */
[[nodiscard]] std::optional<virasa::sim::ProjectManifest> DeserializeManifestFromJson(
	const nlohmann::json& json);

/**
 * @brief Serialize a project manifest to JSON text.
 * @param manifest Manifest to serialize.
 * @return JSON text describing the manifest.
 */
[[nodiscard]] std::string SerializeManifest(const virasa::sim::ProjectManifest& manifest);

/**
 * @brief Deserialize a project manifest from JSON text.
 * @param text JSON text to parse.
 * @return Reconstructed manifest, or std::nullopt when parsing or validation fails.
 */
[[nodiscard]] std::optional<virasa::sim::ProjectManifest> DeserializeManifest(
	std::string_view text);

} // namespace virasa::sim

#endif // VIRASA_SIM_PROJECTMANIFEST_H
