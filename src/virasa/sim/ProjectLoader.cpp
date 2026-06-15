#include "virasa/sim/ProjectLoader.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>

namespace virasa::sim
{
namespace
{

constexpr const char* kProjectManifestFileName = "project.json";

[[nodiscard]] std::optional<std::string> ReadTextFile(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::in | std::ios::binary);
	if (!file)
	{
		return std::nullopt;
	}

	std::string text(
		(std::istreambuf_iterator<char>(file)),
		std::istreambuf_iterator<char>());
	if (file.bad())
	{
		return std::nullopt;
	}

	return text;
}

[[nodiscard]] bool WriteTextFile(const std::filesystem::path& path, std::string_view text)
{
	std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
	if (!file)
	{
		return false;
	}

	file.write(text.data(), static_cast<std::streamsize>(text.size()));
	return file.good();
}

} // namespace

LoadedProject::LoadedProject(
	virasa::sim::ProjectManifest manifestValue,
	virasa::sim::Scene&& sceneValue)
	: manifest(std::move(manifestValue))
	, scene(std::move(sceneValue))
{
}

std::optional<virasa::sim::LoadedProject> OpenProject(
	std::string_view rootDir,
	virasa::sim::AssetResolver& resolver,
	const virasa::sim::ComponentCodecRegistry& codecs,
	virasa::sim::AssetCatalog& catalog)
{
	const std::filesystem::path rootPath{std::string(rootDir)};
	const std::filesystem::path manifestPath = rootPath / kProjectManifestFileName;

	const std::optional<std::string> manifestText = ReadTextFile(manifestPath);
	if (!manifestText.has_value())
	{
		return std::nullopt;
	}

	std::optional<virasa::sim::ProjectManifest> manifest =
		virasa::sim::DeserializeManifest(*manifestText);
	if (!manifest.has_value())
	{
		return std::nullopt;
	}

	for (const virasa::sim::ProjectAssetEntry& entry : manifest->assets)
	{
		const std::filesystem::path assetPath =
			rootPath / std::filesystem::path(entry.source);
		const std::string assetPathString = assetPath.string();
		const uint32_t id = resolver.Resolve(entry.kind, assetPathString);
		if (id != virasa::sim::kInvalidAssetId)
		{
			catalog.Bind(entry.kind, entry.key, id);
		}
	}

	const std::filesystem::path scenePath =
		rootPath / std::filesystem::path(manifest->startupScene);
	const std::optional<std::string> sceneText = ReadTextFile(scenePath);
	if (!sceneText.has_value())
	{
		return std::nullopt;
	}

	std::optional<virasa::sim::Scene> scene =
		virasa::sim::DeserializeScene(*sceneText, codecs, catalog);
	if (!scene.has_value())
	{
		return std::nullopt;
	}

	return virasa::sim::LoadedProject(std::move(*manifest), std::move(*scene));
}

bool SaveProject(
	std::string_view rootDir,
	const virasa::sim::ProjectManifest& manifest,
	const virasa::sim::Scene& scene,
	const virasa::sim::ComponentCodecRegistry& codecs,
	const virasa::sim::AssetCatalog& catalog)
{
	try
	{
		const std::filesystem::path rootPath{std::string(rootDir)};
		const std::filesystem::path manifestPath = rootPath / kProjectManifestFileName;
		const std::filesystem::path scenePath =
			rootPath / std::filesystem::path(manifest.startupScene);

		std::filesystem::create_directories(rootPath);
		const std::filesystem::path sceneParent = scenePath.parent_path();
		if (!sceneParent.empty())
		{
			std::filesystem::create_directories(sceneParent);
		}

		const std::string manifestText = virasa::sim::SerializeManifest(manifest);
		const std::string sceneText =
			virasa::sim::SerializeScene(scene, codecs, catalog);

		return WriteTextFile(manifestPath, manifestText)
			&& WriteTextFile(scenePath, sceneText);
	}
	catch (...)
	{
		return false;
	}
}

} // namespace virasa::sim
