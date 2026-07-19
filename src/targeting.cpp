#include "hyperverse/targeting.hpp"

#include "hyperverse/account_context.hpp"
#include "hyperverse/raider.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <span>

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

void clear_enemy_lock(hyperverse::EnemyTargetLockModel& lock) {
  lock.phase = hyperverse::TargetLockPhase::Unlocked;
  lock.target = entt::null;
  lock.relative_position = {};
  lock.relative_velocity = {};
  lock.wrapped_distance = 0.0F;
  lock.integrity_fraction = 0.0F;
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

void refresh_enemy_lock(
  hyperverse::EnemyTargetLockModel& lock,
  const hyperverse::RaiderShip& target,
  hyperverse::Vec2 observer_position,
  hyperverse::Vec2 observer_velocity,
  const hyperverse::SectorTuning& sector
) {
  lock.relative_position = hyperverse::wrapped_delta(observer_position, target.position, sector);
  lock.relative_velocity = target.velocity - observer_velocity;
  lock.wrapped_distance = hyperverse::length(lock.relative_position);
  lock.integrity_fraction = target.max_integrity > 0.0001F ? std::clamp(target.integrity / target.max_integrity, 0.0F, 1.0F) : 0.0F;
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

[[nodiscard]] entt::entity nearest_enemy(
  entt::registry& registry,
  hyperverse::Vec2 observer_position,
  const hyperverse::SectorTuning& sector,
  float lock_range,
  entt::entity ignored = entt::null
) {
  entt::entity nearest = entt::null;
  float nearest_distance = std::numeric_limits<float>::max();

  for (auto [entity, raider] : registry.view<hyperverse::RaiderShip>().each()) {
    if (
      entity == ignored || raider.integrity <= 0.0F || raider.phase == hyperverse::RaiderPhase::Escaped ||
      (raider.role == hyperverse::RaiderRole::CargoThief && raider.phase == hyperverse::RaiderPhase::Idle)
    ) {
      continue;
    }

    const float distance = hyperverse::wrapped_distance(observer_position, raider.position, sector);
    if (distance <= lock_range && distance < nearest_distance) {
      nearest = entity;
      nearest_distance = distance;
    }
  }

  return nearest;
}

[[nodiscard]] entt::entity next_tracked_target(
  entt::registry& registry,
  std::span<const entt::entity> tracked_targets,
  entt::entity current_target
) {
  if (tracked_targets.empty()) {
    return entt::null;
  }

  auto valid_tracked = [&registry](entt::entity target) {
    return target != entt::null && registry.valid(target) && registry.all_of<hyperverse::AsteroidBody>(target);
  };

  const auto current = std::ranges::find(tracked_targets, current_target);
  if (current != tracked_targets.end()) {
    for (auto candidate = std::next(current); candidate != tracked_targets.end(); ++candidate) {
      if (valid_tracked(*candidate)) {
        return *candidate;
      }
    }
  }

  for (entt::entity candidate : tracked_targets) {
    if (candidate != current_target && valid_tracked(candidate)) {
      return candidate;
    }
  }

  return entt::null;
}

[[nodiscard]] entt::entity next_tracked_enemy(
  entt::registry& registry,
  std::span<const entt::entity> tracked_targets,
  entt::entity current_target
) {
  if (tracked_targets.empty()) {
    return entt::null;
  }

  auto valid_tracked = [&registry](entt::entity target) {
    if (target == entt::null || !registry.valid(target) || !registry.all_of<hyperverse::RaiderShip>(target)) {
      return false;
    }
    const hyperverse::RaiderShip& raider = registry.get<hyperverse::RaiderShip>(target);
    return raider.integrity > 0.0F && raider.phase != hyperverse::RaiderPhase::Escaped &&
           !(raider.role == hyperverse::RaiderRole::CargoThief && raider.phase == hyperverse::RaiderPhase::Idle);
  };

  const auto current = std::ranges::find(tracked_targets, current_target);
  if (current != tracked_targets.end()) {
    for (auto candidate = std::next(current); candidate != tracked_targets.end(); ++candidate) {
      if (valid_tracked(*candidate)) {
        return *candidate;
      }
    }
  }

  for (entt::entity candidate : tracked_targets) {
    if (candidate != current_target && valid_tracked(candidate)) {
      return candidate;
    }
  }

  return entt::null;
}

}  // namespace

namespace hyperverse {

bool has_locked_target(const TargetLockModel& lock) {
  return lock.phase == TargetLockPhase::Locked && lock.target != entt::null;
}

bool has_locked_enemy(const EnemyTargetLockModel& lock) {
  return lock.phase == TargetLockPhase::Locked && lock.target != entt::null;
}

void update_target_lock(
  TargetLockModel& lock,
  entt::registry& registry,
  Vec2 observer_position,
  Vec2 observer_velocity,
  const SemanticInputFrame& input,
  const SectorTuning& sector,
  const TargetingTuning& tuning,
  std::span<const entt::entity> tracked_targets
) {
  if (input.cancel_requested || input.clear_targets_requested) {
    clear_lock(lock);
    return;
  }

  if (has_locked_target(lock)) {
    if (!registry.valid(lock.target) || !registry.all_of<AsteroidBody>(lock.target)) {
      clear_lock(lock);
      return;
    }

    if (input.target_cycle_requested) {
      entt::entity next_target = next_tracked_target(registry, tracked_targets, lock.target);
      if (next_target == entt::null) {
        next_target = nearest_asteroid(registry, observer_position, sector, tuning.lock_range, lock.target);
      }
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

  entt::entity target = next_tracked_target(registry, tracked_targets, entt::null);
  if (target == entt::null) {
    target = nearest_asteroid(registry, observer_position, sector, tuning.lock_range);
  }
  if (target == entt::null) {
    return;
  }

  lock.phase = TargetLockPhase::Locked;
  lock.target = target;
  refresh_lock(lock, registry.get<AsteroidBody>(target), observer_position, observer_velocity, sector);
}

void update_asteroid_motion(AccountCtx& ctx, const SectorTuning& sector, float dt_seconds) {
  ctx.physics().integrate_asteroids(ctx.registry(), sector, dt_seconds);
}

void update_enemy_target_lock(
  EnemyTargetLockModel& lock,
  entt::registry& registry,
  Vec2 observer_position,
  Vec2 observer_velocity,
  const SemanticInputFrame& input,
  const SectorTuning& sector,
  const TargetingTuning& tuning,
  std::span<const entt::entity> tracked_targets
) {
  if (input.cancel_requested || input.clear_targets_requested) {
    clear_enemy_lock(lock);
    return;
  }

  if (has_locked_enemy(lock)) {
    if (!registry.valid(lock.target) || !registry.all_of<RaiderShip>(lock.target)) {
      clear_enemy_lock(lock);
      return;
    }
    const RaiderShip& current = registry.get<RaiderShip>(lock.target);
    if (current.integrity <= 0.0F || current.phase == RaiderPhase::Escaped) {
      clear_enemy_lock(lock);
      return;
    }

    if (input.enemy_target_cycle_requested) {
      entt::entity next_target = next_tracked_enemy(registry, tracked_targets, lock.target);
      if (next_target == entt::null) {
        next_target = nearest_enemy(registry, observer_position, sector, tuning.lock_range, lock.target);
      }
      if (next_target != entt::null) {
        lock.target = next_target;
      }
    }

    refresh_enemy_lock(lock, registry.get<RaiderShip>(lock.target), observer_position, observer_velocity, sector);
    if (lock.wrapped_distance > tuning.release_range) {
      clear_enemy_lock(lock);
    }
    return;
  }

  if (!input.enemy_target_cycle_requested) {
    return;
  }

  entt::entity target = next_tracked_enemy(registry, tracked_targets, entt::null);
  if (target == entt::null) {
    target = nearest_enemy(registry, observer_position, sector, tuning.lock_range);
  }
  if (target == entt::null) {
    return;
  }

  lock.phase = TargetLockPhase::Locked;
  lock.target = target;
  refresh_enemy_lock(lock, registry.get<RaiderShip>(target), observer_position, observer_velocity, sector);
}

}  // namespace hyperverse
