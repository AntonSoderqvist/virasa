#ifndef VIRASA_SIM_BUILTINS_H
#define VIRASA_SIM_BUILTINS_H

namespace virasa::ecs
{
class World;
}

namespace virasa::sim
{
class BehaviorRegistry;

/**
 * @brief Register sim-layer gameplay component systems on a World.
 * @param world World to receive gameplay component systems.
 */
void RegisterGameplayComponents(virasa::ecs::World& world);

/**
 * @brief Register built-in gameplay behavior factories.
 * @param registry Registry to populate with built-in behavior factories.
 */
void RegisterBuiltinBehaviors(virasa::sim::BehaviorRegistry& registry);

} // namespace virasa::sim

#endif // VIRASA_SIM_BUILTINS_H
