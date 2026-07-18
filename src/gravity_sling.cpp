#include "hyperverse/gravity_sling.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace hyperverse {
namespace {

constexpr float Pi = 3.14159265358979323846F;

[[nodiscard]] float angle_of(const Vec2 value) {
  return std::atan2(value.y, value.x);
}

[[nodiscard]] Vec2 unit_from_angle(const float radians) {
  return {.x = std::cos(radians), .y = std::sin(radians)};
}

[[nodiscard]] Vec2 tangent_from_angle(const float radians) {
  return {.x = -std::sin(radians), .y = std::cos(radians)};
}

[[nodiscard]] float wrap_angle(float radians) {
  while (radians > Pi) {
    radians -= 2.0F * Pi;
  }
  while (radians < -Pi) {
    radians += 2.0F * Pi;
  }
  return radians;
}

[[nodiscard]] float smoothstep(const float value) {
  const float clamped = std::clamp(value, 0.0F, 1.0F);
  return clamped * clamped * (3.0F - (2.0F * clamped));
}

[[nodiscard]] GravitySlingTargetFrame target_frame(entt::registry& registry, const entt::entity target) {
  const AsteroidBody& asteroid = registry.get<AsteroidBody>(target);
  return {
    .position = asteroid.position,
    .velocity = asteroid.velocity,
    .rotation_radians = asteroid.rotation_radians,
    .angular_velocity = asteroid.angular_velocity,
    .radius = asteroid.radius,
  };
}

[[nodiscard]] float min_sling_radius(const GravitySlingTargetFrame& target, const GravitySlingTuning& tuning) {
  return std::max(1.0F, target.radius + tuning.min_radius_padding);
}

[[nodiscard]] float max_sling_radius(const GravitySlingTargetFrame& target, const GravitySlingTuning& tuning) {
  return std::max(min_sling_radius(target, tuning), target.radius + tuning.max_radius_padding);
}

[[nodiscard]] Vec2 local_offset_to_world(const GravitySlingTargetFrame& target, const float local_angle, const float radius) {
  return target.position + (unit_from_angle(local_angle) * radius);
}

void disengage(
  GravitySlingModel& model,
  ShipMotion& ship,
  const Vec2 release_velocity,
  const GravitySlingDisengageReason reason
) {
  const Vec2 last_release_velocity = release_velocity;
  model = {};
  model.last_release_velocity = last_release_velocity;
  model.current_world_velocity = last_release_velocity;
  model.disengage_reason = reason;
  ship.velocity = last_release_velocity;
}

[[nodiscard]] GravitySlingHudSnapshot snapshot(
  const GravitySlingModel& model,
  const GravitySlingTargetFrame* target,
  const GravitySlingTuning& tuning
) {
  GravitySlingHudSnapshot hud{
    .phase = model.phase,
    .target = model.target,
    .radius = model.radius,
    .local_angle_radians = model.local_angle_radians,
    .relative_angular_velocity = model.relative_angular_velocity,
    .release_velocity = model.last_release_velocity,
    .disengage_reason = model.disengage_reason,
  };
  if (target != nullptr) {
    hud.min_radius = min_sling_radius(*target, tuning);
    hud.max_radius = max_sling_radius(*target, tuning);
    hud.target_angular_velocity = target->angular_velocity;
    hud.release_velocity = gravity_sling_release_velocity(
      *target,
      model.local_angle_radians,
      model.radius,
      model.relative_angular_velocity,
      model.entry_velocity,
      tuning.release_entry_velocity_fraction
    );
  }
  return hud;
}

}  // namespace

bool is_gravity_sling_target_eligible(entt::registry& registry, const entt::entity target) {
  if (target == entt::null || !registry.valid(target) || !registry.all_of<AsteroidBody>(target)) {
    return false;
  }

  const AsteroidBody& asteroid = registry.get<AsteroidBody>(target);
  return asteroid.radius > 0.0F && std::isfinite(asteroid.position.x) && std::isfinite(asteroid.position.y) &&
         std::isfinite(asteroid.rotation_radians) && std::isfinite(asteroid.angular_velocity);
}

entt::entity acquire_gravity_sling_target(
  entt::registry& registry,
  const Vec2 player_position,
  const Vec2 aim_direction,
  const SectorTuning& sector,
  const GravitySlingTuning& tuning
) {
  const Vec2 normalized_aim = normalize_or_zero(aim_direction);
  const bool has_aim = length(normalized_aim) > 0.0F;
  entt::entity best = entt::null;
  float best_alignment = -std::numeric_limits<float>::max();
  float best_distance = std::numeric_limits<float>::max();

  for (auto [entity, asteroid] : registry.view<AsteroidBody>().each()) {
    if (!is_gravity_sling_target_eligible(registry, entity)) {
      continue;
    }

    const Vec2 delta = wrapped_delta(player_position, asteroid.position, sector);
    const float distance = length(delta);
    if (distance > tuning.acquisition_range) {
      continue;
    }

    const float alignment = has_aim ? dot(normalize_or_zero(delta), normalized_aim) : 1.0F;
    if (alignment > best_alignment + 0.0001F || (std::abs(alignment - best_alignment) <= 0.0001F && distance < best_distance)) {
      best = entity;
      best_alignment = alignment;
      best_distance = distance;
    }
  }

  return best;
}

Vec2 gravity_sling_release_velocity(
  const GravitySlingTargetFrame& target,
  const float world_angle_radians,
  const float radius,
  const float relative_angular_velocity,
  const Vec2 entry_velocity,
  const float entry_velocity_fraction
) {
  const Vec2 tangent = tangent_from_angle(world_angle_radians);
  (void)target.angular_velocity;
  return target.velocity + (tangent * radius * relative_angular_velocity) + (entry_velocity * entry_velocity_fraction);
}

GravitySlingHudSnapshot update_gravity_sling(
  GravitySlingModel& model,
  entt::registry& registry,
  ShipMotion& ship,
  const SemanticInputFrame& input,
  const SectorTuning& sector,
  const float dt_seconds,
  const GravitySlingTuning& tuning
) {
  const float dt = std::max(0.0F, dt_seconds);

  if (model.phase == GravitySlingPhase::FreeFlight) {
    if (!input.gravity_sling_requested) {
      return snapshot(model, nullptr, tuning);
    }

    const entt::entity target = acquire_gravity_sling_target(registry, ship.position, input.primary_aim, sector, tuning);
    if (target == entt::null) {
      GravitySlingHudSnapshot hud = snapshot(model, nullptr, tuning);
      hud.acquisition_failed = true;
      return hud;
    }

    const GravitySlingTargetFrame target_state = target_frame(registry, target);
    const Vec2 radial = wrapped_delta(target_state.position, ship.position, sector);
    const float radius = std::clamp(length(radial), min_sling_radius(target_state, tuning), max_sling_radius(target_state, tuning));
    model.phase = GravitySlingPhase::Engaging;
    model.target = target;
    model.radius = radius;
    model.local_angle_radians = wrap_angle(angle_of(radial));
    const Vec2 tangent = tangent_from_angle(model.local_angle_radians);
    const Vec2 relative_velocity = ship.velocity - target_state.velocity;
    model.relative_angular_velocity = radius > 0.0001F ? dot(relative_velocity, tangent) / radius : 0.0F;
    model.engagement_elapsed_seconds = 0.0F;
    model.engagement_start_position = ship.position;
    model.previous_position = ship.position;
    model.entry_velocity = ship.velocity;
    model.target_velocity_at_engage = target_state.velocity;
    model.target_angular_velocity_at_engage = target_state.angular_velocity;
    model.current_world_velocity = ship.velocity;
    model.last_release_velocity = {};
    model.disengage_reason = GravitySlingDisengageReason::None;
    return snapshot(model, &target_state, tuning);
  }

  if (!is_gravity_sling_target_eligible(registry, model.target)) {
    disengage(model, ship, model.current_world_velocity, GravitySlingDisengageReason::TargetDestroyed);
    return snapshot(model, nullptr, tuning);
  }

  const GravitySlingTargetFrame target_state = target_frame(registry, model.target);
  const float minimum_radius = min_sling_radius(target_state, tuning);
  const float maximum_radius = max_sling_radius(target_state, tuning);

  if (input.gravity_sling_requested) {
    disengage(
      model,
      ship,
      gravity_sling_release_velocity(
        target_state,
        model.local_angle_radians,
        model.radius,
        model.relative_angular_velocity,
        model.entry_velocity,
        tuning.release_entry_velocity_fraction
      ),
      GravitySlingDisengageReason::PlayerReleased
    );
    return snapshot(model, nullptr, tuning);
  }

  const float world_angle = model.local_angle_radians;
  const Vec2 tangent_direction = tangent_from_angle(world_angle);
  const float tangential_input = dot(input.desired_movement, tangent_direction);

  model.radius = std::clamp(model.radius, minimum_radius, maximum_radius);
  model.relative_angular_velocity += tangential_input * tuning.angular_adjust_speed * dt;
  if (tuning.relative_angular_damping > 0.0F && length(input.desired_movement) <= 0.0001F) {
    const float damping = std::clamp(tuning.relative_angular_damping * dt, 0.0F, 1.0F);
    model.relative_angular_velocity += (0.0F - model.relative_angular_velocity) * damping;
  }
  model.local_angle_radians = wrap_angle(model.local_angle_radians + (model.relative_angular_velocity * dt));

  const Vec2 target_position = local_offset_to_world(target_state, model.local_angle_radians, model.radius);
  Vec2 next_position = target_position;
  if (model.phase == GravitySlingPhase::Engaging) {
    model.engagement_elapsed_seconds += dt;
    const float fraction = tuning.engagement_seconds > 0.0001F ? model.engagement_elapsed_seconds / tuning.engagement_seconds : 1.0F;
    next_position = wrap_position(model.engagement_start_position + (wrapped_delta(model.engagement_start_position, target_position, sector) * smoothstep(fraction)), sector);
    if (fraction >= 1.0F) {
      model.phase = GravitySlingPhase::Active;
    }
  } else {
    const float current_radius = wrapped_distance(target_state.position, ship.position, sector);
    if (current_radius < minimum_radius - tuning.radius_break_tolerance || current_radius > maximum_radius + tuning.radius_break_tolerance) {
      disengage(model, ship, model.current_world_velocity, GravitySlingDisengageReason::RadiusOutOfBounds);
      return snapshot(model, nullptr, tuning);
    }
  }

  model.current_world_velocity = dt > 0.0001F ? wrapped_delta(model.previous_position, next_position, sector) * (1.0F / dt) : ship.velocity;
  model.previous_position = next_position;
  ship.position = wrap_position(next_position, sector);
  ship.velocity = model.current_world_velocity;
  return snapshot(model, &target_state, tuning);
}

}  // namespace hyperverse
