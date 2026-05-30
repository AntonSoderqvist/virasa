#ifndef VIRASA_ECS_COMPONENTACCESS_H
#define VIRASA_ECS_COMPONENTACCESS_H

#include "virasa/ecs/Components.h"
#include "virasa/ecs/ComponentSystem.h"
#include "virasa/ecs/World.h"

#include <vector>

// Thin, non-template convenience accessors for the built-in plain-old-data
// component types that are stored in byte-backed SparseComponentSystems
// (Mesh, Visual, the three light types, and Camera). They wrap the type-erased
// World::GetSystem + GetRaw/SetRaw/AddRaw surface with a typed cast, resolving
// the system by its stable registered name.
//
// Transform is intentionally absent: it has its own typed system, reached via
// World::Transforms(). Name is World-intrinsic, reached via the World name API.
//
// Each accessor resolves the ComponentId by name on every call; for built-in
// systems that lookup is a short linear scan and is cheap enough for editor and
// per-entity renderer use. Callers in tight loops may hoist GetSystemId/GetSystem
// themselves if profiling warrants it.

namespace virasa::ecs
{

#define VIRASA_DEFINE_COMPONENT_ACCESS(Type, NameLiteral)                              \
	/** @brief True if the entity has a Type##Component. */                            \
	inline bool Has##Type(const World& world, Entity entity) noexcept                  \
	{                                                                                  \
		ComponentId id = world.GetSystemId(NameLiteral);                               \
		return id != kInvalidComponentId && world.GetSystem(id).Has(entity);           \
	}                                                                                  \
	/** @brief Const reference to the entity's Type##Component (must exist). */         \
	[[nodiscard]] inline const Type##Component& Get##Type(const World& world, Entity entity) \
	{                                                                                  \
		return *static_cast<const Type##Component*>(                                   \
			world.GetSystem(world.GetSystemId(NameLiteral)).GetRaw(entity));           \
	}                                                                                  \
	/** @brief Inserts a Type##Component for an entity that does not yet have one. */   \
	inline void Add##Type(World& world, Entity entity, const Type##Component& value)    \
	{                                                                                  \
		world.GetSystem(world.GetSystemId(NameLiteral)).AddRaw(entity, &value);        \
	}                                                                                  \
	/** @brief Overwrites the entity's Type##Component (must exist), marking it dirty. */\
	inline void Set##Type(World& world, Entity entity, const Type##Component& value)    \
	{                                                                                  \
		world.GetSystem(world.GetSystemId(NameLiteral)).SetRaw(entity, &value);        \
	}                                                                                  \
	/** @brief Dense list of entities owning a Type##Component. */                      \
	[[nodiscard]] inline const std::vector<Entity>& Type##Entities(const World& world)  \
	{                                                                                  \
		return world.GetSystem(world.GetSystemId(NameLiteral)).Entities();             \
	}

VIRASA_DEFINE_COMPONENT_ACCESS(Mesh, "Mesh")
VIRASA_DEFINE_COMPONENT_ACCESS(Visual, "Visual")
VIRASA_DEFINE_COMPONENT_ACCESS(DirectionalLight, "DirectionalLight")
VIRASA_DEFINE_COMPONENT_ACCESS(PointLight, "PointLight")
VIRASA_DEFINE_COMPONENT_ACCESS(SpotLight, "SpotLight")
VIRASA_DEFINE_COMPONENT_ACCESS(Camera, "Camera")

#undef VIRASA_DEFINE_COMPONENT_ACCESS

} // namespace virasa::ecs

#endif // VIRASA_ECS_COMPONENTACCESS_H
