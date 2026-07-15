#include "hyperverse/targeting.hpp"

#include <limits>

namespace {

void clear_lock(hyperverse::TargetLockModel& lock) {
  lock.phase = hyperverse::TargetLockPhase::Unlocked;
  lock.target = entt::null;
  lock.relative_position = {};
  lock.wrapped_distance = 0.0F;
  lock.scan_confidence = 0.0F;
}

void refresh_lock(
  hyperverse::TargetLockModel& lock,
  const hyperverse::AsteroidBody& target,
  hyperverse::Vec2 observer_position,
  const hyperverse::SectorTuning& sector
) {
  lock.relative_position = hyperverse::wrapped_delta(observer_position, target.position, sector);
  lock.wrapped_distance = hyperverse::length(lock.relative_position);
  lock.scan_confidence = target.scan_confidence;
}

[[nodiscard]] entt::entity nearest_asteroid(
  entt::registry& registry,
  hyperverse::Vec2 observer_position,
  const hyperverse::SectorTuning& sector,
  float lock_range
) {
  entt::entity nearest = entt::null;
  float nearest_distance = std::numeric_limits<float>::max();

  for (auto [entity, asteroid] : registry.view<hyperverse::AsteroidBody>().each()) {
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

    refresh_lock(lock, registry.get<AsteroidBody>(lock.target), observer_position, sector);
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
  refresh_lock(lock, registry.get<AsteroidBody>(target), observer_position, sector);
}

}  // namespace hyperverse
