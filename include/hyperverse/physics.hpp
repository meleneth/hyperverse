#pragma once

#include "hyperverse/sector.hpp"
#include "hyperverse/math.hpp"

#include <entt/entity/registry.hpp>

#include <memory>

namespace hyperverse {

struct ShipMotion;

class PhysicsWorld {
public:
  PhysicsWorld();
  ~PhysicsWorld();

  PhysicsWorld(const PhysicsWorld&) = delete;
  PhysicsWorld& operator=(const PhysicsWorld&) = delete;
  PhysicsWorld(PhysicsWorld&&) = delete;
  PhysicsWorld& operator=(PhysicsWorld&&) = delete;

  void integrate_ship(ShipMotion& ship, const SectorTuning& sector, float dt_seconds);
  void integrate_asteroids(entt::registry& registry, const SectorTuning& sector, float dt_seconds);

private:
  struct Runtime;
  std::unique_ptr<Runtime> runtime_;
};

}  // namespace hyperverse
