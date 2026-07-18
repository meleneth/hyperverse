#pragma once

#include "hyperverse/flight.hpp"
#include "hyperverse/input.hpp"
#include "hyperverse/sector.hpp"
#include "hyperverse/targeting.hpp"

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>

namespace hyperverse {

enum class GravitySlingPhase {
  FreeFlight,
  Engaging,
  Active,
};

enum class GravitySlingDisengageReason {
  None,
  PlayerReleased,
  TargetDestroyed,
  TargetInvalid,
  RadiusOutOfBounds,
  IncompatibleState,
};

struct GravitySlingModel {
  GravitySlingPhase phase{GravitySlingPhase::FreeFlight};
  entt::entity target{entt::null};
  float radius{0.0F};
  float local_angle_radians{0.0F};
  float relative_angular_velocity{0.0F};
  float engagement_elapsed_seconds{0.0F};
  Vec2 engagement_start_position{};
  Vec2 previous_position{};
  Vec2 target_velocity_at_engage{};
  float target_angular_velocity_at_engage{0.0F};
  Vec2 entry_velocity{};
  Vec2 current_world_velocity{};
  Vec2 last_release_velocity{};
  GravitySlingDisengageReason disengage_reason{GravitySlingDisengageReason::None};
};

struct GravitySlingTuning {
  float acquisition_range{1600.0F};
  float engagement_seconds{0.22F};
  float min_radius_padding{72.0F};
  float max_radius_padding{920.0F};
  float radial_adjust_speed{280.0F};
  float angular_adjust_speed{1.7F};
  float relative_angular_damping{0.0F};
  float release_entry_velocity_fraction{0.0F};
  float radius_break_tolerance{180.0F};
};

struct GravitySlingTargetFrame {
  Vec2 position{};
  Vec2 velocity{};
  float rotation_radians{0.0F};
  float angular_velocity{0.0F};
  float radius{0.0F};
};

struct GravitySlingHudSnapshot {
  GravitySlingPhase phase{GravitySlingPhase::FreeFlight};
  entt::entity target{entt::null};
  float radius{0.0F};
  float min_radius{0.0F};
  float max_radius{0.0F};
  float local_angle_radians{0.0F};
  float target_angular_velocity{0.0F};
  float relative_angular_velocity{0.0F};
  Vec2 release_velocity{};
  GravitySlingDisengageReason disengage_reason{GravitySlingDisengageReason::None};
  bool acquisition_failed{false};
};

[[nodiscard]] bool is_gravity_sling_target_eligible(entt::registry& registry, entt::entity target);

[[nodiscard]] entt::entity acquire_gravity_sling_target(
  entt::registry& registry,
  Vec2 player_position,
  Vec2 aim_direction,
  const SectorTuning& sector,
  const GravitySlingTuning& tuning = {}
);

[[nodiscard]] Vec2 gravity_sling_release_velocity(
  const GravitySlingTargetFrame& target,
  float world_angle_radians,
  float radius,
  float relative_angular_velocity,
  Vec2 entry_velocity = {},
  float entry_velocity_fraction = 0.0F
);

[[nodiscard]] GravitySlingHudSnapshot update_gravity_sling(
  GravitySlingModel& model,
  entt::registry& registry,
  ShipMotion& ship,
  const SemanticInputFrame& input,
  const SectorTuning& sector,
  float dt_seconds,
  const GravitySlingTuning& tuning = {}
);

}  // namespace hyperverse
