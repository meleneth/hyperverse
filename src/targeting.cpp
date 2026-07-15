#include "hyperverse/targeting.hpp"

#include <algorithm>
#include <limits>

namespace {

void clear_lock(hyperverse::TargetLockModel& lock) {
  lock.phase = hyperverse::TargetLockPhase::Unlocked;
  lock.target = entt::null;
  lock.relative_position = {};
  lock.relative_velocity = {};
  lock.wrapped_distance = 0.0F;
  lock.closing_speed = 0.0F;
  lock.time_to_contact_seconds = 0.0F;
  lock.scan_confidence = 0.0F;
}

void refresh_lock(
  hyperverse::TargetLockModel& lock,
  const hyperverse::AsteroidBody& target,
  hyperverse::Vec2 observer_position,
  hyperverse::Vec2 observer_velocity,
  const hyperverse::SectorTuning& sector
) {
  lock.relative_position = hyperverse::wrapped_delta(observer_position, target.position, sector);
  lock.relative_velocity = target.velocity - observer_velocity;
  lock.wrapped_distance = hyperverse::length(lock.relative_position);
  const hyperverse::Vec2 target_direction = hyperverse::normalize_or_zero(lock.relative_position);
  lock.closing_speed = std::max(0.0F, -hyperverse::dot(target_direction, lock.relative_velocity));
  const float clearance = std::max(0.0F, lock.wrapped_distance - target.radius);
  lock.time_to_contact_seconds = lock.closing_speed > 0.0001F ? clearance / lock.closing_speed : 0.0F;
  lock.scan_confidence = target.scan_confidence;
}

[[nodiscard]] entt::entity nearest_asteroid(
  entt::registry& registry,
  hyperverse::Vec2 observer_position,
  const hyperverse::SectorTuning& sector,
  float lock_range,
  entt::entity ignored = entt::null
) {
  entt::entity nearest = entt::null;
  float nearest_distance = std::numeric_limits<float>::max();

  for (auto [entity, asteroid] : registry.view<hyperverse::AsteroidBody>().each()) {
    if (entity == ignored) {
      continue;
    }

    const float distance = hyperverse::wrapped_distance(observer_position, asteroid.position, sector);
    if (distance <= lock_range && distance < nearest_distance) {
      nearest = entity;
      nearest_distance = distance;
    }
  }

  return nearest;
}

}  // namespace

namespace hyperverse {

bool has_locked_target(const TargetLockModel& lock) {
  return lock.phase == TargetLockPhase::Locked && lock.target != entt::null;
}

void update_target_lock(
  TargetLockModel& lock,
  entt::registry& registry,
  Vec2 observer_position,
  Vec2 observer_velocity,
  const SemanticInputFrame& input,
  const SectorTuning& sector,
  const TargetingTuning& tuning
) {
  if (input.cancel_requested) {
    clear_lock(lock);
    return;
  }

  if (has_locked_target(lock)) {
    if (!registry.valid(lock.target) || !registry.all_of<AsteroidBody>(lock.target)) {
      clear_lock(lock);
      return;
    }

    if (input.target_cycle_requested) {
      const entt::entity next_target = nearest_asteroid(registry, observer_position, sector, tuning.lock_range, lock.target);
      if (next_target != entt::null) {
        lock.target = next_target;
      }
    }

    refresh_lock(lock, registry.get<AsteroidBody>(lock.target), observer_position, observer_velocity, sector);
    if (lock.wrapped_distance > tuning.release_range) {
      clear_lock(lock);
    }
    return;
  }

  if (!input.target_cycle_requested) {
    return;
  }

  const entt::entity target = nearest_asteroid(registry, observer_position, sector, tuning.lock_range);
  if (target == entt::null) {
    return;
  }

  lock.phase = TargetLockPhase::Locked;
  lock.target = target;
  refresh_lock(lock, registry.get<AsteroidBody>(target), observer_position, observer_velocity, sector);
}

}  // namespace hyperverse
