#ifndef VIRASA_SIM_COMPONENTCODEC_H
#define VIRASA_SIM_COMPONENTCODEC_H

#include "json.hpp"
#include "virasa/ecs/Types.h"
#include "virasa/sim/AssetCatalog.h"

#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace virasa::sim
{

/**
 * @brief Resolves transient entity handles to stable serialized ids and back.
 */
class EntityResolver
{
public:
	EntityResolver() = default;
	virtual ~EntityResolver() noexcept = default;

	EntityResolver(const EntityResolver&) = delete;
	EntityResolver& operator=(const EntityResolver&) = delete;

	EntityResolver(EntityResolver&&) = delete;
	EntityResolver& operator=(EntityResolver&&) = delete;

	[[nodiscard]] virtual int32_t StableIdForEntity(virasa::ecs::Entity entity) const = 0;
	[[nodiscard]] virtual virasa::ecs::Entity EntityForStableId(int32_t stableId) const = 0;
};

/**
 * @brief Shared lookup context used while translating components to or from JSON.
 */
struct SerializationContext
{
public:
	SerializationContext(
		const virasa::sim::AssetCatalog& catalog,
		const virasa::sim::EntityResolver& entities)
		: catalog(catalog)
		, entities(entities)
	{
	}

	SerializationContext() = delete;
	~SerializationContext() = default;

	SerializationContext(const SerializationContext&) = delete;
	SerializationContext& operator=(const SerializationContext&) = delete;

	SerializationContext(SerializationContext&&) = delete;
	SerializationContext& operator=(SerializationContext&&) = delete;

	const virasa::sim::AssetCatalog& catalog;
	const virasa::sim::EntityResolver& entities;
};

/**
 * @brief Type-erased translator between component bytes and JSON fields.
 */
class ComponentCodec
{
public:
	ComponentCodec() = default;
	virtual ~ComponentCodec() noexcept = default;

	ComponentCodec(const ComponentCodec&) = delete;
	ComponentCodec& operator=(const ComponentCodec&) = delete;

	ComponentCodec(ComponentCodec&&) = delete;
	ComponentCodec& operator=(ComponentCodec&&) = delete;

	/**
	 * @brief Get the stable component system name handled by this codec.
	 * @return Component system name.
	 */
	[[nodiscard]] virtual std::string_view ComponentName() const noexcept = 0;

	/**
	 * @brief Get the byte size of one component value handled by this codec.
	 * @return Component element size in bytes.
	 */
	[[nodiscard]] virtual size_t ElementSize() const noexcept = 0;

	/**
	 * @brief Convert one component value to JSON.
	 * @param component Pointer to a component value of ElementSize() bytes.
	 * @param context Serialization lookup context.
	 * @return JSON object describing the component.
	 */
	[[nodiscard]] virtual nlohmann::json ToJson(
		const void* component,
		const virasa::sim::SerializationContext& context) const = 0;

	/**
	 * @brief Decode one component value from JSON.
	 * @param json JSON object describing the component.
	 * @param context Serialization lookup context.
	 * @param dst Destination buffer of at least ElementSize() bytes.
	 * @return True when the JSON was readable.
	 */
	virtual bool FromJson(
		const nlohmann::json& json,
		const virasa::sim::SerializationContext& context,
		void* dst) const = 0;
};

/**
 * @brief Owns component codecs and resolves them by stable component name.
 */
class ComponentCodecRegistry final
{
public:
	ComponentCodecRegistry() = default;
	~ComponentCodecRegistry() = default;

	ComponentCodecRegistry(const ComponentCodecRegistry&) = delete;
	ComponentCodecRegistry& operator=(const ComponentCodecRegistry&) = delete;

	ComponentCodecRegistry(ComponentCodecRegistry&&) noexcept = default;
	ComponentCodecRegistry& operator=(ComponentCodecRegistry&&) noexcept = default;

	/**
	 * @brief Register and take ownership of a component codec.
	 * @param codec Codec to store under its ComponentName().
	 */
	void Register(std::unique_ptr<virasa::sim::ComponentCodec> codec);

	/**
	 * @brief Find a registered codec by component name.
	 * @param name Exact, case-sensitive component name.
	 * @return Non-owning codec pointer, or nullptr when absent.
	 */
	[[nodiscard]] const virasa::sim::ComponentCodec* Find(std::string_view name) const;

	/**
	 * @brief Check whether a codec is registered by component name.
	 * @param name Exact, case-sensitive component name.
	 * @return True when a codec exists for name.
	 */
	[[nodiscard]] bool Contains(std::string_view name) const;

	/**
	 * @brief Count registered codecs.
	 * @return Number of registered component codecs.
	 */
	[[nodiscard]] size_t Size() const noexcept;

	/**
	 * @brief Get all registered component names in registration order.
	 * @return Fresh vector containing each registered name once.
	 */
	[[nodiscard]] std::vector<std::string> Names() const;

private:
	std::vector<std::unique_ptr<virasa::sim::ComponentCodec>> _codecs;
};

/**
 * @brief Register codecs for all serializable built-in components.
 * @param registry Registry to receive the built-in codecs.
 */
void RegisterBuiltinComponentCodecs(virasa::sim::ComponentCodecRegistry& registry);

} // namespace virasa::sim

#endif // VIRASA_SIM_COMPONENTCODEC_H
