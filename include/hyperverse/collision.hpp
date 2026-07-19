#pragma once

#include "hyperverse/flight.hpp"
#include "hyperverse/sector.hpp"
#include "hyperverse/targeting.hpp"

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>

#include <cstddef>

namespace hyperverse {

struct CollisionProbe {
  float ship_radius{28.0F};
  float warning_seconds{2.0F};
  float view_radius_world{1800.0F};
  float zoom_scale{1.0F};
  std::size_t min_swept_checks{8U};
  std::size_t max_swept_checks{64U};
  float speed_checks_per_world_unit{0.025F};
  float zoom_checks_per_scale{12.0F};
};

struct CollisionHudSnapshot {
  bool contact{false};
  bool warning{false};
  entt::entity asteroid{entt::null};
  float separation{0.0F};
  float impact_speed{0.0F};
  float time_to_contact_seconds{0.0F};
  std::size_t candidate_count{0U};
  std::size_t swept_checks{0U};
};

struct SweptCircleHit {
  bool hit{false};
  float fraction{0.0F};
  float separation{0.0F};
};

[[nodiscard]] SweptCircleHit swept_circle_intersection(Vec2 relative_position, Vec2 motion, float combined_radius);

[[nodiscard]] CollisionHudSnapshot predict_ship_asteroid_collision(
  const ShipMotion& ship,
  entt::registry& registry,
  const SectorTuning& sector,
  const CollisionProbe& probe = {}
);

}  // namespace hyperverse
