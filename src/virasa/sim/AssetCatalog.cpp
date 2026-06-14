#include "virasa/sim/AssetCatalog.h"

#include <cassert>

namespace virasa::sim
{

namespace
{

constexpr size_t ToIndex(virasa::sim::AssetKind kind) noexcept
{
	const auto index = static_cast<size_t>(kind);
	assert(index < 2);
	return index;
}

} // namespace

void AssetCatalog::Bind(virasa::sim::AssetKind kind, std::string_view key, uint32_t id)
{
	Entries& entries = _entriesByKind[ToIndex(kind)];

	size_t keyIndex = entries.size();
	size_t idIndex = entries.size();
	for (size_t index = 0; index < entries.size(); ++index)
	{
		if (entries[index].key == key)
		{
			keyIndex = index;
		}
		if (entries[index].id == id)
		{
			idIndex = index;
		}
	}

	if (keyIndex == entries.size() && idIndex == entries.size())
	{
		entries.push_back(Entry{
			.key = std::string(key),
			.id = id,
		});
		return;
	}

	if (keyIndex != entries.size() && idIndex != entries.size() && keyIndex != idIndex)
	{
		entries[keyIndex].id = id;
		entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(idIndex));
		return;
	}

	const size_t targetIndex = keyIndex != entries.size() ? keyIndex : idIndex;
	entries[targetIndex].key = std::string(key);
	entries[targetIndex].id = id;
}

uint32_t AssetCatalog::IdForKey(virasa::sim::AssetKind kind, std::string_view key) const
{
	const Entries& entries = _entriesByKind[ToIndex(kind)];
	for (const Entry& entry : entries)
	{
		if (entry.key == key)
		{
			return entry.id;
		}
	}

	return virasa::sim::kInvalidAssetId;
}

std::string_view AssetCatalog::KeyForId(virasa::sim::AssetKind kind, uint32_t id) const
{
	const Entries& entries = _entriesByKind[ToIndex(kind)];
	for (const Entry& entry : entries)
	{
		if (entry.id == id)
		{
			return entry.key;
		}
	}

	return {};
}

bool AssetCatalog::HasKey(virasa::sim::AssetKind kind, std::string_view key) const
{
	const Entries& entries = _entriesByKind[ToIndex(kind)];
	for (const Entry& entry : entries)
	{
		if (entry.key == key)
		{
			return true;
		}
	}

	return false;
}

bool AssetCatalog::HasId(virasa::sim::AssetKind kind, uint32_t id) const
{
	const Entries& entries = _entriesByKind[ToIndex(kind)];
	for (const Entry& entry : entries)
	{
		if (entry.id == id)
		{
			return true;
		}
	}

	return false;
}

size_t AssetCatalog::Size(virasa::sim::AssetKind kind) const noexcept
{
	return _entriesByKind[ToIndex(kind)].size();
}

void AssetCatalog::Clear() noexcept
{
	for (Entries& entries : _entriesByKind)
	{
		entries.clear();
	}
}

} // namespace virasa::sim
