#ifndef VIRASA_SIM_ASSETCATALOG_H
#define VIRASA_SIM_ASSETCATALOG_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace virasa::sim
{

/**
 * @brief Runtime asset category with an independent key/id namespace.
 */
enum class AssetKind : uint32_t
{
	Mesh = 0,
	Material = 1,
};

/**
 * @brief Sentinel value used when an asset key has no runtime id binding.
 */
inline constexpr uint32_t kInvalidAssetId = 0xFFFFFFFFu;

/**
 * @brief Maintains stable asset keys and runtime registry ids per asset kind.
 *
 * AssetCatalog owns copied string keys and stores no renderer or registry handles.
 */
class AssetCatalog final
{
public:
	AssetCatalog() = default;
	~AssetCatalog() = default;

	AssetCatalog(const AssetCatalog&) = default;
	AssetCatalog& operator=(const AssetCatalog&) = default;

	AssetCatalog(AssetCatalog&&) noexcept = default;
	AssetCatalog& operator=(AssetCatalog&&) noexcept = default;

	/**
	 * @brief Bind a stable key to a runtime id within an asset kind.
	 * @param kind Asset namespace to update.
	 * @param key Stable asset key to copy into the catalog.
	 * @param id Runtime registry id to associate with the key.
	 */
	void Bind(virasa::sim::AssetKind kind, std::string_view key, uint32_t id);

	/**
	 * @brief Find the runtime id for a stable key.
	 * @param kind Asset namespace to search.
	 * @param key Stable asset key to look up.
	 * @return Bound runtime id, or kInvalidAssetId when the key is absent.
	 */
	[[nodiscard]] uint32_t IdForKey(
		virasa::sim::AssetKind kind,
		std::string_view key) const;

	/**
	 * @brief Find the stable key for a runtime id.
	 * @param kind Asset namespace to search.
	 * @param id Runtime registry id to look up.
	 * @return Catalog-owned key view, or an empty view when the id is absent.
	 */
	[[nodiscard]] std::string_view KeyForId(virasa::sim::AssetKind kind, uint32_t id) const;

	/**
	 * @brief Check whether a key is bound within an asset kind.
	 * @param kind Asset namespace to search.
	 * @param key Stable asset key to look up.
	 * @return True when the key is bound.
	 */
	[[nodiscard]] bool HasKey(virasa::sim::AssetKind kind, std::string_view key) const;

	/**
	 * @brief Check whether an id is bound within an asset kind.
	 * @param kind Asset namespace to search.
	 * @param id Runtime registry id to look up.
	 * @return True when the id is bound.
	 */
	[[nodiscard]] bool HasId(virasa::sim::AssetKind kind, uint32_t id) const;

	/**
	 * @brief Count bindings within an asset kind.
	 * @param kind Asset namespace to count.
	 * @return Number of key/id pairs recorded for the kind.
	 */
	[[nodiscard]] size_t Size(virasa::sim::AssetKind kind) const noexcept;

	/**
	 * @brief Remove all bindings from every asset kind.
	 */
	void Clear() noexcept;

private:
	struct Entry
	{
		std::string key;
		uint32_t id = virasa::sim::kInvalidAssetId;
	};

	using Entries = std::vector<Entry>;

	std::array<Entries, 2> _entriesByKind;
};

} // namespace virasa::sim

#endif // VIRASA_SIM_ASSETCATALOG_H
