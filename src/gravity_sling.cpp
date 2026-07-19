#include "hyperverse/gravity_sling.hpp"

#include <boost/sml.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace hyperverse {
namespace {

namespace sml = boost::sml;

constexpr float Pi = 3.14159265358979323846F;

struct free_flight {};
struct engaging {};
struct active {};
struct begin_engage {};
struct complete_engage {};
struct player_release {};
struct target_lost {};
struct radius_break {};
struct force_free_flight {};

struct GravitySlingMachine {
  auto operator()() const {
    using namespace sml;
    return make_transition_table(
      *state<free_flight> + event<begin_engage> = state<engaging>,
      state<engaging> + event<complete_engage> = state<active>,
      state<engaging> + event<player_release> = state<free_flight>,
      state<engaging> + event<target_lost> = state<free_flight>,
      state<engaging> + event<radius_break> = state<free_flight>,
      state<engaging> + event<force_free_flight> = state<free_flight>,
      state<active> + event<player_release> = state<free_flight>,
      state<active> + event<target_lost> = state<free_flight>,
      state<active> + event<radius_break> = state<free_flight>,
      state<active> + event<force_free_flight> = state<free_flight>
    );
  }
};

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

[[nodiscard]] float shortest_angle_delta(float from, float to) {
  return wrap_angle(to - from);
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

[[nodiscard]] GravitySlingTargetFrame target_frame(const AsteroidBody& asteroid) {
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

void steer_sling_facing(ShipMotion& ship, const SemanticInputFrame& input, const GravitySlingTuning& tuning, float dt) {
  const Vec2 facing_intent = length(input.desired_movement) > 0.0001F ? input.desired_movement : input.primary_aim;
  if (length(facing_intent) <= 0.0001F) {
    return;
  }

  const float target_angle = angle_of(facing_intent);
  const float max_turn = std::max(0.0F, tuning.thrust_turn_rate) * dt;
  ship.facing_radians = wrap_angle(ship.facing_radians + std::clamp(shortest_angle_delta(ship.facing_radians, target_angle), -max_turn, max_turn));
}

void transfer_sling_thrust_to_target(AsteroidBody& asteroid, const SemanticInputFrame& input, const GravitySlingTuning& tuning, float dt) {
  const Vec2 thrust = clamp_length(input.desired_movement, 1.0F);
  if (length(thrust) <= 0.0001F) {
    return;
  }

  asteroid.velocity += normalize_or_zero(thrust) * std::max(0.0F, tuning.asteroid_thrust_acceleration) * length(thrust) * dt;
}

void replay_phase(sml::sm<GravitySlingMachine>& machine, GravitySlingPhase phase) {
  switch (phase) {
    case GravitySlingPhase::FreeFlight:
      return;
    case GravitySlingPhase::Engaging:
      machine.process_event(begin_engage{});
      return;
    case GravitySlingPhase::Active:
      machine.process_event(begin_engage{});
      machine.process_event(complete_engage{});
      return;
  }
}

[[nodiscard]] GravitySlingPhase read_phase(const sml::sm<GravitySlingMachine>& machine) {
  if (machine.is(sml::state<engaging>)) {
    return GravitySlingPhase::Engaging;
  }
  if (machine.is(sml::state<active>)) {
    return GravitySlingPhase::Active;
  }
  return GravitySlingPhase::FreeFlight;
}

[[nodiscard]] GravitySlingDisengageReason disengage_reason_for(GravitySlingTransition transition) {
  switch (transition) {
    case GravitySlingTransition::BeginEngage:
    case GravitySlingTransition::CompleteEngage:
      return GravitySlingDisengageReason::None;
    case GravitySlingTransition::PlayerRelease:
      return GravitySlingDisengageReason::PlayerReleased;
    case GravitySlingTransition::TargetLost:
      return GravitySlingDisengageReason::TargetDestroyed;
    case GravitySlingTransition::RadiusBreak:
      return GravitySlingDisengageReason::RadiusOutOfBounds;
    case GravitySlingTransition::ForceFreeFlight:
      return GravitySlingDisengageReason::IncompatibleState;
  }
  return GravitySlingDisengageReason::None;
}

void emit_gravity_sling_phase_changed(
  DomainEventBus* event_bus,
  entt::entity subject,
  const GravitySlingModel& model
) {
  if (event_bus == nullptr) {
    return;
  }
  event_bus->enqueue(
    DomainEventType::GravitySlingPhaseChanged,
    DomainEvent{
      .type = DomainEventType::GravitySlingPhaseChanged,
      .subject = subject,
      .target = model.target,
      .amount = static_cast<float>(model.disengage_reason),
      .count = static_cast<int>(model.phase),
    }
  );
}

void clear_sling_session(GravitySlingModel& model) {
  model.target = entt::null;
  model.radius = 0.0F;
  model.local_angle_radians = 0.0F;
  model.relative_angular_velocity = 0.0F;
  model.engagement_elapsed_seconds = 0.0F;
  model.engagement_start_position = {};
  model.previous_position = {};
  model.target_velocity_at_engage = {};
  model.target_angular_velocity_at_engage = 0.0F;
  model.entry_velocity = {};
}

void disengage(
  GravitySlingModel& model,
  ShipMotion& ship,
  const Vec2 release_velocity,
  const GravitySlingTransition transition,
  entt::entity subject,
  DomainEventBus* event_bus
) {
  const Vec2 last_release_velocity = release_velocity;
  (void)transition_gravity_sling(model, transition, subject, event_bus);
  clear_sling_session(model);
  model.last_release_velocity = last_release_velocity;
  model.current_world_velocity = last_release_velocity;
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

bool transition_gravity_sling(
  GravitySlingModel& model,
  GravitySlingTransition transition,
  entt::entity subject,
  DomainEventBus* event_bus
) {
  sml::sm<GravitySlingMachine> machine;
  replay_phase(machine, model.phase);
  const GravitySlingPhase previous_phase = model.phase;
  const GravitySlingDisengageReason previous_reason = model.disengage_reason;
  bool accepted = false;
  switch (transition) {
    case GravitySlingTransition::BeginEngage:
      accepted = machine.process_event(begin_engage{});
      break;
    case GravitySlingTransition::CompleteEngage:
      accepted = machine.process_event(complete_engage{});
      break;
    case GravitySlingTransition::PlayerRelease:
      accepted = machine.process_event(player_release{});
      break;
    case GravitySlingTransition::TargetLost:
      accepted = machine.process_event(target_lost{});
      break;
    case GravitySlingTransition::RadiusBreak:
      accepted = machine.process_event(radius_break{});
      break;
    case GravitySlingTransition::ForceFreeFlight:
      accepted = machine.process_event(force_free_flight{});
      break;
  }
  if (!accepted) {
    return false;
  }

  model.phase = read_phase(machine);
  model.disengage_reason = disengage_reason_for(transition);
  if (model.phase == previous_phase && model.disengage_reason == previous_reason) {
    return false;
  }

  emit_gravity_sling_phase_changed(event_bus, subject, model);
  return true;
}

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
  const GravitySlingTuning& tuning,
  entt::entity subject,
  DomainEventBus* event_bus
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
    const float radius = length(radial);
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
    (void)transition_gravity_sling(model, GravitySlingTransition::BeginEngage, subject, event_bus);
    return snapshot(model, &target_state, tuning);
  }

  if (!is_gravity_sling_target_eligible(registry, model.target)) {
    disengage(model, ship, model.current_world_velocity, GravitySlingTransition::TargetLost, subject, event_bus);
    return snapshot(model, nullptr, tuning);
  }

  AsteroidBody& target_asteroid = registry.get<AsteroidBody>(model.target);
  GravitySlingTargetFrame target_state = target_frame(target_asteroid);

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
      GravitySlingTransition::PlayerRelease,
      subject,
      event_bus
    );
    return snapshot(model, nullptr, tuning);
  }

  steer_sling_facing(ship, input, tuning, dt);
  transfer_sling_thrust_to_target(target_asteroid, input, tuning, dt);
  target_state = target_frame(target_asteroid);

  const float world_angle = model.local_angle_radians;
  const Vec2 tangent_direction = tangent_from_angle(world_angle);
  const float tangential_input = dot(input.desired_movement, tangent_direction);

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
      (void)transition_gravity_sling(model, GravitySlingTransition::CompleteEngage, subject, event_bus);
    }
  } else {
    const float current_radius = wrapped_distance(target_state.position, ship.position, sector);
    if (std::abs(current_radius - model.radius) > tuning.radius_break_tolerance) {
      disengage(model, ship, model.current_world_velocity, GravitySlingTransition::RadiusBreak, subject, event_bus);
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
