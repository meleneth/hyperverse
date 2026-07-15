#pragma once

#include "hyperverse/flight.hpp"
#include "hyperverse/sector.hpp"
#include "hyperverse/targeting.hpp"

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>

namespace hyperverse {

struct CollisionProbe {
  float ship_radius{28.0F};
  float warning_seconds{2.0F};
};

struct CollisionHudSnapshot {
  bool contact{false};
  bool warning{false};
  entt::entity asteroid{entt::null};
  float separation{0.0F};
  float impact_speed{0.0F};
  float time_to_contact_seconds{0.0F};
};

[[nodiscard]] CollisionHudSnapshot predict_ship_asteroid_collision(
  const ShipMotion& ship,
  entt::registry& registry,
  const SectorTuning& sector,
  const CollisionProbe& probe = {}
);

}  // namespace hyperverse
