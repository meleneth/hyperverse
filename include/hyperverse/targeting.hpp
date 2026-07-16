#pragma once

#include "hyperverse/input.hpp"
#include "hyperverse/math.hpp"
#include "hyperverse/sector.hpp"

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>

namespace hyperverse {

struct AsteroidBody {
  Vec2 position{};
  Vec2 velocity{};
  float radius{160.0F};
  float base_radius{160.0F};
  float rotation_radians{0.0F};
  float angular_velocity{0.0F};
  float scan_confidence{0.25F};
};

enum class TargetLockPhase {
  Unlocked,
  Locked,
};

struct TargetingTuning {
  float lock_range{2500.0F};
  float release_range{3200.0F};
};

struct TargetLockModel {
  TargetLockPhase phase{TargetLockPhase::Unlocked};
  entt::entity target{entt::null};
  Vec2 relative_position{};
  Vec2 relative_velocity{};
  float wrapped_distance{0.0F};
  float closing_speed{0.0F};
  float time_to_contact_seconds{0.0F};
  float scan_confidence{0.0F};
};

[[nodiscard]] bool has_locked_target(const TargetLockModel& lock);

void update_target_lock(
  TargetLockModel& lock,
  entt::registry& registry,
  Vec2 observer_position,
  Vec2 observer_velocity,
  const SemanticInputFrame& input,
  const SectorTuning& sector,
  const TargetingTuning& tuning = {}
);

void update_asteroid_motion(entt::registry& registry, const SectorTuning& sector, float dt_seconds);

}  // namespace hyperverse
