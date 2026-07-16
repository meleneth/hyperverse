#pragma once

#include "hyperverse/sector.hpp"
#include "hyperverse/math.hpp"

#include <entt/entity/registry.hpp>

namespace hyperverse {

struct ShipMotion;

class PhysicsWorld {
public:
  PhysicsWorld();

  void integrate_ship(ShipMotion& ship, const SectorTuning& sector, float dt_seconds);
  void integrate_asteroids(entt::registry& registry, const SectorTuning& sector, float dt_seconds);
};

}  // namespace hyperverse
