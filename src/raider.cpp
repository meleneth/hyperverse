#include "hyperverse/raider.hpp"

#include "hyperverse/engine_trail.hpp"
#include "hyperverse/projectile.hpp"

#include <boost/sml.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace hyperverse {

namespace {

namespace sml = boost::sml;

struct idle {};
struct approaching {};
struct disrupting {};
struct towing {};
struct escaped {};
struct deactivate {};
struct approach {};
struct begin_disruption {};
struct begin_tow {};
struct escape {};

struct harassing {};
struct covering {};
struct stealing {};
struct full_aggression {};
struct assign_cargo_theft {};
struct harass_player {};
struct cover_thief {};
struct escalate_full_aggression {};

struct RaiderPhaseMachine {
  auto operator()() const {
    using namespace sml;
    return make_transition_table(
      *state<idle> + event<approach> = state<approaching>,
      state<idle> + event<begin_disruption> = state<disrupting>,
      state<idle> + event<begin_tow> = state<towing>,
      state<approaching> + event<deactivate> = state<idle>,
      state<approaching> + event<begin_disruption> = state<disrupting>,
      state<approaching> + event<begin_tow> = state<towing>,
      state<disrupting> + event<deactivate> = state<idle>,
      state<disrupting> + event<approach> = state<approaching>,
      state<disrupting> + event<begin_tow> = state<towing>,
      state<towing> + event<deactivate> = state<idle>,
      state<towing> + event<escape> = state<escaped>,
      state<escaped> + event<deactivate> = state<escaped>
    );
  }
};

struct RaiderTaskMachine {
  auto operator()() const {
    using namespace sml;
    return make_transition_table(
      *state<stealing> + event<harass_player> = state<harassing>,
      state<stealing> + event<cover_thief> = state<covering>,
      state<stealing> + event<escalate_full_aggression> = state<full_aggression>,
      state<harassing> + event<assign_cargo_theft> = state<stealing>,
      state<harassing> + event<cover_thief> = state<covering>,
      state<harassing> + event<escalate_full_aggression> = state<full_aggression>,
      state<covering> + event<assign_cargo_theft> = state<stealing>,
      state<covering> + event<harass_player> = state<harassing>,
      state<covering> + event<escalate_full_aggression> = state<full_aggression>,
      state<full_aggression> + event<assign_cargo_theft> = state<stealing>,
      state<full_aggression> + event<harass_player> = state<harassing>,
      state<full_aggression> + event<cover_thief> = state<covering>
    );
  }
};

void replay_phase(sml::sm<RaiderPhaseMachine>& machine, RaiderPhase phase) {
  switch (phase) {
    case RaiderPhase::Idle:
      return;
    case RaiderPhase::Approaching:
      machine.process_event(approach{});
      return;
    case RaiderPhase::Disrupting:
      machine.process_event(begin_disruption{});
      return;
    case RaiderPhase::Towing:
      machine.process_event(begin_tow{});
      return;
    case RaiderPhase::Escaped:
      machine.process_event(begin_tow{});
      machine.process_event(escape{});
      return;
  }
}

void replay_task(sml::sm<RaiderTaskMachine>& machine, RaiderTask task) {
  switch (task) {
    case RaiderTask::StealCargo:
      return;
    case RaiderTask::HarassPlayer:
      machine.process_event(harass_player{});
      return;
    case RaiderTask::CoverThief:
      machine.process_event(cover_thief{});
      return;
    case RaiderTask::FullAggression:
      machine.process_event(escalate_full_aggression{});
      return;
  }
}

[[nodiscard]] RaiderPhase read_phase(const sml::sm<RaiderPhaseMachine>& machine) {
  if (machine.is(sml::state<approaching>)) {
    return RaiderPhase::Approaching;
  }
  if (machine.is(sml::state<disrupting>)) {
    return RaiderPhase::Disrupting;
  }
  if (machine.is(sml::state<towing>)) {
    return RaiderPhase::Towing;
  }
  if (machine.is(sml::state<escaped>)) {
    return RaiderPhase::Escaped;
  }
  return RaiderPhase::Idle;
}

[[nodiscard]] RaiderTask read_task(const sml::sm<RaiderTaskMachine>& machine) {
  if (machine.is(sml::state<harassing>)) {
    return RaiderTask::HarassPlayer;
  }
  if (machine.is(sml::state<covering>)) {
    return RaiderTask::CoverThief;
  }
  if (machine.is(sml::state<full_aggression>)) {
    return RaiderTask::FullAggression;
  }
  return RaiderTask::StealCargo;
}

void emit_raider_phase_changed(DomainEventBus* event_bus, entt::entity raider_entity, const RaiderShip& raider) {
  if (event_bus == nullptr) {
    return;
  }
  event_bus->enqueue(
    DomainEventType::RaiderPhaseChanged,
    DomainEvent{
      .type = DomainEventType::RaiderPhaseChanged,
      .subject = raider_entity,
      .target = raider.target_box,
      .position = raider.position,
      .amount = raider.integrity,
      .count = static_cast<int>(raider.phase),
    }
  );
}

void emit_raider_task_changed(DomainEventBus* event_bus, entt::entity raider_entity, const RaiderShip& raider) {
  if (event_bus == nullptr) {
    return;
  }
  event_bus->enqueue(
    DomainEventType::RaiderTaskChanged,
    DomainEvent{
      .type = DomainEventType::RaiderTaskChanged,
      .subject = raider_entity,
      .target = raider.target_box,
      .position = raider.position,
      .amount = raider.integrity,
      .count = static_cast<int>(raider.task),
    }
  );
}

[[nodiscard]] entt::entity rearmost_cargo_box(entt::registry& registry) {
  entt::entity selected = entt::null;
  int selected_index = std::numeric_limits<int>::min();

  for (auto [entity, box] : registry.view<CargoBox>().each()) {
    if (box.state == CargoBoxState::Linked && box.index > selected_index) {
      selected = entity;
      selected_index = box.index;
    }
  }

  return selected;
}

[[nodiscard]] bool cargo_thief_needs_cover(entt::registry& registry) {
  for (auto [entity, raider] : registry.view<RaiderShip>().each()) {
    (void)entity;
    if (
      raider.role == RaiderRole::CargoThief &&
      (raider.phase == RaiderPhase::Disrupting || raider.phase == RaiderPhase::Towing)
    ) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] RaiderTaskTransition select_raider_task_transition(const RaiderShip& raider, entt::registry& registry, const CargoEscortState& escort) {
  if (raider.role == RaiderRole::CargoThief) {
    return RaiderTaskTransition::AssignCargoTheft;
  }
  if (raider.task == RaiderTask::FullAggression) {
    return RaiderTaskTransition::EscalateToFullAggression;
  }
  if (escort.phase == CargoEscortPhase::Extracting || escort.phase == CargoEscortPhase::Complete) {
    return RaiderTaskTransition::EscalateToFullAggression;
  }
  if (cargo_thief_needs_cover(registry)) {
    return RaiderTaskTransition::CoverThief;
  }
  return RaiderTaskTransition::HarassPlayer;
}

void spawn_combat_raider(entt::registry& registry, Vec2 position, RaiderTask task, float integrity) {
  const entt::entity raider = registry.create();
  registry.emplace<RaiderShip>(
    raider,
    RaiderShip{
      .position = position,
      .role = RaiderRole::Combat,
      .task = task,
      .integrity = integrity,
      .max_integrity = integrity,
      .orbit_radians = std::atan2(position.y, position.x),
    }
  );
  registry.emplace<ParticleCannonModel>(raider);
  registry.emplace<EngineTrailModel>(raider);
}

[[nodiscard]] float shortest_angle_delta(float from, float to) {
  float delta = std::fmod(to - from, std::numbers::pi_v<float> * 2.0F);
  if (delta > std::numbers::pi_v<float>) {
    delta -= std::numbers::pi_v<float> * 2.0F;
  } else if (delta < -std::numbers::pi_v<float>) {
    delta += std::numbers::pi_v<float> * 2.0F;
  }
  return delta;
}

void rotate_raider_toward(RaiderShip& raider, Vec2 direction, float turn_rate, float dt_seconds) {
  if (length(direction) <= 0.0001F) {
    return;
  }
  const float target_angle = std::atan2(direction.y, direction.x);
  const float max_turn = std::max(0.0F, turn_rate) * std::max(0.0F, dt_seconds);
  raider.facing_radians += std::clamp(shortest_angle_delta(raider.facing_radians, target_angle), -max_turn, max_turn);
}

void accelerate_raider_toward(RaiderShip& raider, Vec2 target, const SectorTuning& sector, float dt_seconds, float max_speed, const RaiderTuning& tuning) {
  const float scaled_dt = std::max(0.0F, dt_seconds);
  if (scaled_dt <= 0.0F) {
    return;
  }

  const Vec2 delta = wrapped_delta(raider.position, target, sector);
  const float distance = length(delta);
  const Vec2 thrust_direction = distance > 0.001F ? normalize_or_zero(delta) : normalize_or_zero(raider.velocity) * -1.0F;
  if (length(thrust_direction) > 0.0001F) {
    raider.velocity += thrust_direction * std::max(0.0F, tuning.combat_acceleration) * scaled_dt;
    rotate_raider_toward(raider, thrust_direction, tuning.turn_rate, scaled_dt);
  }
  raider.velocity = clamp_length(raider.velocity, max_speed);
  raider.position = wrap_position(raider.position + (raider.velocity * scaled_dt), sector);
}

[[nodiscard]] float combat_orbit_speed_scale(RaiderTask task) {
  switch (task) {
    case RaiderTask::FullAggression:
      return 1.25F;
    case RaiderTask::CoverThief:
      return 0.85F;
    case RaiderTask::HarassPlayer:
    case RaiderTask::StealCargo:
      return 1.0F;
  }
  return 1.0F;
}

[[nodiscard]] float combat_orbit_radius_scale(RaiderTask task) {
  switch (task) {
    case RaiderTask::FullAggression:
      return 0.74F;
    case RaiderTask::CoverThief:
      return 1.18F;
    case RaiderTask::HarassPlayer:
    case RaiderTask::StealCargo:
      return 1.0F;
  }
  return 1.0F;
}

void update_combat_raider(
  RaiderShip& raider,
  const ShipMotion& ship,
  const SectorTuning& sector,
  float dt_seconds,
  const RaiderTuning& tuning,
  entt::entity raider_entity,
  DomainEventBus* event_bus
) {
  const float scaled_dt = std::max(0.0F, dt_seconds);
  const Vec2 to_player = wrapped_delta(raider.position, ship.position, sector);
  const float player_distance = length(to_player);
  if (std::abs(raider.orbit_radians) <= 0.0001F && player_distance > 0.001F) {
    raider.orbit_radians = std::atan2(-to_player.y, -to_player.x);
  }

  const float speed_scale = combat_orbit_speed_scale(raider.task);
  const float radius_scale = combat_orbit_radius_scale(raider.task);
  raider.orbit_radians = std::fmod(
    raider.orbit_radians + (tuning.combat_orbit_radians_per_second * speed_scale * scaled_dt),
    std::numbers::pi_v<float> * 2.0F
  );
  if (raider.orbit_radians < 0.0F) {
    raider.orbit_radians += std::numbers::pi_v<float> * 2.0F;
  }

  const Vec2 forward = length(ship.velocity) > 24.0F ? normalize_or_zero(ship.velocity) : Vec2{.x = std::cos(ship.facing_radians), .y = std::sin(ship.facing_radians)};
  const Vec2 right{.x = -forward.y, .y = forward.x};
  const Vec2 orbit_offset =
    (forward * std::cos(raider.orbit_radians) * tuning.combat_orbit_x_radius * radius_scale) +
    (right * std::sin(raider.orbit_radians) * tuning.combat_orbit_y_radius * radius_scale);
  const Vec2 desired_position = wrap_position(ship.position + orbit_offset, sector);
  const float distance_to_orbit = length(wrapped_delta(raider.position, desired_position, sector));

  (void)transition_raider_phase(
    raider,
    distance_to_orbit > tuning.combat_orbit_arrival_tolerance ? RaiderTransition::Approach : RaiderTransition::BeginDisruption,
    raider_entity,
    event_bus
  );
  accelerate_raider_toward(raider, desired_position, sector, scaled_dt, tuning.max_speed * speed_scale, tuning);
}

}  // namespace

bool transition_raider_phase(
  RaiderShip& raider,
  RaiderTransition transition,
  entt::entity raider_entity,
  DomainEventBus* event_bus
) {
  sml::sm<RaiderPhaseMachine> machine;
  replay_phase(machine, raider.phase);
  const RaiderPhase previous = raider.phase;
  switch (transition) {
    case RaiderTransition::Deactivate:
      machine.process_event(deactivate{});
      break;
    case RaiderTransition::Approach:
      machine.process_event(approach{});
      break;
    case RaiderTransition::BeginDisruption:
      machine.process_event(begin_disruption{});
      break;
    case RaiderTransition::BeginTow:
      machine.process_event(begin_tow{});
      break;
    case RaiderTransition::Escape:
      machine.process_event(escape{});
      break;
  }
  raider.phase = read_phase(machine);
  if (raider.phase == previous) {
    return false;
  }
  emit_raider_phase_changed(event_bus, raider_entity, raider);
  return true;
}

bool transition_raider_task(
  RaiderShip& raider,
  RaiderTaskTransition transition,
  entt::entity raider_entity,
  DomainEventBus* event_bus
) {
  sml::sm<RaiderTaskMachine> machine;
  replay_task(machine, raider.task);
  const RaiderTask previous = raider.task;
  switch (transition) {
    case RaiderTaskTransition::AssignCargoTheft:
      machine.process_event(assign_cargo_theft{});
      break;
    case RaiderTaskTransition::HarassPlayer:
      machine.process_event(harass_player{});
      break;
    case RaiderTaskTransition::CoverThief:
      machine.process_event(cover_thief{});
      break;
    case RaiderTaskTransition::EscalateToFullAggression:
      machine.process_event(escalate_full_aggression{});
      break;
  }
  raider.task = read_task(machine);
  if (raider.task == previous) {
    return false;
  }
  emit_raider_task_changed(event_bus, raider_entity, raider);
  return true;
}

void spawn_gate_combat_raiders(
  entt::registry& registry,
  Vec2 gate_position,
  Vec2 player_position,
  const SectorTuning& sector,
  int count
) {
  const int spawn_count = std::max(0, count);
  const Vec2 away_from_player = normalize_or_zero(wrapped_delta(player_position, gate_position, sector));
  const Vec2 base_direction = length(away_from_player) > 0.0F ? away_from_player : Vec2{.x = 1.0F, .y = 0.0F};
  const float base_angle = std::atan2(base_direction.y, base_direction.x);

  for (int index = 0; index < spawn_count; ++index) {
    const float offset_angle = base_angle + ((static_cast<float>(index) - ((static_cast<float>(spawn_count) - 1.0F) * 0.5F)) * 0.55F);
    const Vec2 offset{.x = std::cos(offset_angle) * 720.0F, .y = std::sin(offset_angle) * 720.0F};
    spawn_combat_raider(registry, wrap_position(gate_position + offset, sector), RaiderTask::FullAggression, 90.0F);
  }
}

int spawn_pressure_raiders(
  entt::registry& registry,
  Vec2 player_position,
  const SectorTuning& sector,
  int threat_level
) {
  if (threat_level <= 0) {
    return 0;
  }

  const int spawn_count = std::clamp(1 + (threat_level / 2), 1, 5);
  const RaiderTask task = threat_level >= 5 ? RaiderTask::FullAggression : RaiderTask::HarassPlayer;
  const float integrity = static_cast<float>(70 + (threat_level * 6));
  const float radius = 1200.0F + (static_cast<float>(threat_level) * 90.0F);
  const float base_angle = static_cast<float>(threat_level) * 0.73F;

  for (int index = 0; index < spawn_count; ++index) {
    const float angle = base_angle + ((static_cast<float>(index) / static_cast<float>(spawn_count)) * std::numbers::pi_v<float> * 2.0F);
    const Vec2 offset{.x = std::cos(angle) * radius, .y = std::sin(angle) * radius};
    spawn_combat_raider(registry, wrap_position(player_position + offset, sector), task, integrity);
  }

  return spawn_count;
}

RaiderHudSnapshot update_raider_threat(
  RaiderShip& raider,
  entt::registry& registry,
  const CargoEscortState& escort,
  const ShipMotion& ship,
  const SectorTuning& sector,
  float dt_seconds,
  const RaiderTuning& tuning,
  entt::entity raider_entity,
  DomainEventBus* event_bus
) {
  if (raider.integrity <= 0.0F) {
    (void)transition_raider_phase(raider, RaiderTransition::Deactivate, raider_entity, event_bus);
    raider.velocity = {};
    raider.target_box = entt::null;
    raider.disruption_seconds = 0.0F;
    return {};
  }

  (void)transition_raider_task(raider, select_raider_task_transition(raider, registry, escort), raider_entity, event_bus);
  raider.cloak_fade_seconds = std::min(tuning.cloak_fade_seconds, raider.cloak_fade_seconds + std::max(0.0F, dt_seconds));
  if (raider.role == RaiderRole::Combat) {
    const Vec2 to_player = wrapped_delta(raider.position, ship.position, sector);
    const float player_distance = length(to_player);
    update_combat_raider(raider, ship, sector, dt_seconds, tuning, raider_entity, event_bus);
    return {
      .phase = raider.phase,
      .task = raider.task,
      .target_distance = player_distance,
      .active = true,
    };
  }

  if (escort.phase != CargoEscortPhase::EscortActive) {
    if (raider.phase != RaiderPhase::Escaped) {
      (void)transition_raider_phase(raider, RaiderTransition::Deactivate, raider_entity, event_bus);
    }
    raider.velocity = {};
    raider.target_box = entt::null;
    raider.disruption_seconds = 0.0F;
    return {};
  }

  if (raider.target_box == entt::null || !registry.valid(raider.target_box) || !registry.all_of<CargoBox>(raider.target_box)) {
    raider.target_box = rearmost_cargo_box(registry);
  }

  if (raider.target_box == entt::null) {
    (void)transition_raider_phase(raider, RaiderTransition::Deactivate, raider_entity, event_bus);
    raider.velocity = {};
    raider.disruption_seconds = 0.0F;
    return {.active = true};
  }

  CargoBox& target = registry.get<CargoBox>(raider.target_box);
  const Vec2 to_target = wrapped_delta(raider.position, target.position, sector);
  const float target_distance = length(to_target);

  if (target.state == CargoBoxState::Stolen || raider.phase == RaiderPhase::Towing) {
    (void)transition_raider_phase(raider, RaiderTransition::BeginTow, raider_entity, event_bus);
  } else if (target_distance > tuning.disruption_range) {
    (void)transition_raider_phase(raider, RaiderTransition::Approach, raider_entity, event_bus);
    raider.disruption_seconds = 0.0F;
    accelerate_raider_toward(raider, target.position, sector, dt_seconds, tuning.max_speed, tuning);
  } else {
    (void)transition_raider_phase(raider, RaiderTransition::BeginDisruption, raider_entity, event_bus);
    raider.velocity = {};
    raider.disruption_seconds = std::min(tuning.disruption_seconds, raider.disruption_seconds + dt_seconds);
    if (raider.disruption_seconds >= tuning.disruption_seconds) {
      (void)transition_cargo_box(target, CargoBoxTransition::Steal, raider.target_box, event_bus);
      (void)transition_raider_phase(raider, RaiderTransition::BeginTow, raider_entity, event_bus);
    }
  }

  if (raider.phase == RaiderPhase::Towing) {
    Vec2 escape_direction = normalize_or_zero(wrapped_delta(ship.position, raider.position, sector));
    if (length(escape_direction) <= 0.0001F) {
      escape_direction = {.x = 1.0F, .y = 0.0F};
    }
    accelerate_raider_toward(raider, wrap_position(raider.position + (escape_direction * tuning.escape_distance), sector), sector, dt_seconds, tuning.max_speed, tuning);
    target.position = raider.position;
    target.velocity = raider.velocity;
    if (wrapped_distance(ship.position, target.position, sector) >= tuning.escape_distance) {
      (void)transition_cargo_box(target, CargoBoxTransition::Lose, raider.target_box, event_bus);
      (void)transition_raider_phase(raider, RaiderTransition::Escape, raider_entity, event_bus);
      raider.target_box = entt::null;
      raider.velocity = {};
    }
  }

  return {
    .target_box = raider.target_box,
    .phase = raider.phase,
    .task = raider.task,
    .target_distance = target_distance,
    .disruption_fraction = raider.disruption_seconds / std::max(tuning.disruption_seconds, std::numeric_limits<float>::epsilon()),
    .escape_distance =
      (raider.phase == RaiderPhase::Towing || raider.phase == RaiderPhase::Escaped) ? wrapped_distance(ship.position, target.position, sector) : 0.0F,
    .active = true,
  };
}

CargoRecoveryHudSnapshot recover_stolen_cargo(
  entt::registry& registry,
  RaiderShip& raider,
  const ShipMotion& ship,
  const SemanticInputFrame& input,
  const SectorTuning& sector,
  const CargoRecoveryTuning& tuning,
  entt::entity raider_entity,
  DomainEventBus* event_bus
) {
  entt::entity nearest = entt::null;
  float nearest_distance = std::numeric_limits<float>::max();

  for (auto [entity, box] : registry.view<CargoBox>().each()) {
    if (box.state != CargoBoxState::Stolen) {
      continue;
    }

    const float distance = wrapped_distance(ship.position, box.position, sector);
    if (distance < nearest_distance) {
      nearest = entity;
      nearest_distance = distance;
    }
  }

  if (nearest == entt::null) {
    return {};
  }

  CargoRecoveryHudSnapshot hud{
    .recovered_box = nearest,
    .nearest_stolen_distance = nearest_distance,
    .stolen_box_near = nearest_distance <= tuning.recovery_range,
  };

  if (hud.stolen_box_near && input.confirm_requested) {
    CargoBox& box = registry.get<CargoBox>(nearest);
    (void)transition_cargo_box(box, CargoBoxTransition::Link, nearest, event_bus);
    box.velocity = ship.velocity;

    if (raider.target_box == nearest) {
      raider.target_box = entt::null;
      (void)transition_raider_phase(raider, RaiderTransition::Deactivate, raider_entity, event_bus);
      raider.disruption_seconds = 0.0F;
    }

    hud.recovered = true;
  }

  return hud;
}

}  // namespace hyperverse
