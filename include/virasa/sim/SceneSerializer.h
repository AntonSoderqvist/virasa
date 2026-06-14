#ifndef VIRASA_SIM_SCENESERIALIZER_H
#define VIRASA_SIM_SCENESERIALIZER_H

#include "json.hpp"
#include "virasa/sim/AssetCatalog.h"
#include "virasa/sim/ComponentCodec.h"
#include "virasa/sim/Scene.h"

#include <optional>
#include <string>
#include <string_view>

namespace virasa::sim
{

/**
 * @brief Serialize a Scene to its structured JSON document form.
 * @param scene Scene to serialize.
 * @param codecs Component codec registry used to serialize component bytes.
 * @param catalog Asset catalog used by codecs that translate runtime ids to stable keys.
 * @return JSON document describing the scene.
 */
[[nodiscard]] nlohmann::json SerializeSceneToJson(
	const virasa::sim::Scene& scene,
	const virasa::sim::ComponentCodecRegistry& codecs,
	const virasa::sim::AssetCatalog& catalog);

/**
 * @brief Deserialize a Scene from its structured JSON document form.
 * @param json JSON document to read.
 * @param codecs Component codec registry used to decode component bytes.
 * @param catalog Asset catalog used by codecs that translate stable keys to runtime ids.
 * @return Reconstructed Scene, or std::nullopt when the document shape is unsupported.
 */
[[nodiscard]] std::optional<virasa::sim::Scene> DeserializeSceneFromJson(
	const nlohmann::json& json,
	const virasa::sim::ComponentCodecRegistry& codecs,
	const virasa::sim::AssetCatalog& catalog);

/**
 * @brief Serialize a Scene to JSON text.
 * @param scene Scene to serialize.
 * @param codecs Component codec registry used to serialize component bytes.
 * @param catalog Asset catalog used by codecs that translate runtime ids to stable keys.
 * @return JSON text describing the scene.
 */
[[nodiscard]] std::string SerializeScene(
	const virasa::sim::Scene& scene,
	const virasa::sim::ComponentCodecRegistry& codecs,
	const virasa::sim::AssetCatalog& catalog);

/**
 * @brief Deserialize a Scene from JSON text.
 * @param text JSON text to parse.
 * @param codecs Component codec registry used to decode component bytes.
 * @param catalog Asset catalog used by codecs that translate stable keys to runtime ids.
 * @return Reconstructed Scene, or std::nullopt when parsing or validation fails.
 */
[[nodiscard]] std::optional<virasa::sim::Scene> DeserializeScene(
	std::string_view text,
	const virasa::sim::ComponentCodecRegistry& codecs,
	const virasa::sim::AssetCatalog& catalog);

} // namespace virasa::sim

#endif // VIRASA_SIM_SCENESERIALIZER_H
