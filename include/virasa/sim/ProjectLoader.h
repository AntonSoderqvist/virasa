#ifndef VIRASA_SIM_PROJECTLOADER_H
#define VIRASA_SIM_PROJECTLOADER_H

#include "virasa/sim/AssetCatalog.h"
#include "virasa/sim/ComponentCodec.h"
#include "virasa/sim/ProjectManifest.h"
#include "virasa/sim/Scene.h"
#include "virasa/sim/SceneSerializer.h"

#include <cstdint>
#include <optional>
#include <string_view>

namespace virasa::sim
{

class AssetResolver
{
public:
	AssetResolver() = default;
	virtual ~AssetResolver() = default;

	AssetResolver(const AssetResolver&) = delete;
	AssetResolver& operator=(const AssetResolver&) = delete;

	AssetResolver(AssetResolver&&) = delete;
	AssetResolver& operator=(AssetResolver&&) = delete;

	[[nodiscard]] virtual uint32_t Resolve(
		virasa::sim::AssetKind kind,
		std::string_view path) = 0;
};

struct LoadedProject
{
	LoadedProject() = delete;
	LoadedProject(virasa::sim::ProjectManifest manifest, virasa::sim::Scene&& scene);
	~LoadedProject() = default;

	LoadedProject(const LoadedProject&) = delete;
	LoadedProject& operator=(const LoadedProject&) = delete;

	LoadedProject(LoadedProject&&) noexcept = default;
	LoadedProject& operator=(LoadedProject&&) noexcept = default;

	virasa::sim::ProjectManifest manifest;
	virasa::sim::Scene scene;
};

[[nodiscard]] std::optional<virasa::sim::LoadedProject> OpenProject(
	std::string_view rootDir,
	virasa::sim::AssetResolver& resolver,
	const virasa::sim::ComponentCodecRegistry& codecs,
	virasa::sim::AssetCatalog& catalog);

[[nodiscard]] bool SaveProject(
	std::string_view rootDir,
	const virasa::sim::ProjectManifest& manifest,
	const virasa::sim::Scene& scene,
	const virasa::sim::ComponentCodecRegistry& codecs,
	const virasa::sim::AssetCatalog& catalog);

} // namespace virasa::sim

#endif // VIRASA_SIM_PROJECTLOADER_H
