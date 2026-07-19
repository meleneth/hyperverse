#include "hyperverse/radar_control.hpp"

#include <boost/sml.hpp>

namespace hyperverse {
namespace {

namespace sml = boost::sml;

struct released {};
struct single_held {};
struct chord_blocked {};
struct shoulders_released {};
struct single_pressed {};
struct chord_pressed {};

struct RadarControlMachine {
  auto operator()() const {
    using namespace sml;
    return make_transition_table(
      *state<released> + event<single_pressed> = state<single_held>,
      state<released> + event<chord_pressed> = state<chord_blocked>,
      state<single_held> + event<shoulders_released> = state<released>,
      state<single_held> + event<chord_pressed> = state<chord_blocked>,
      state<chord_blocked> + event<shoulders_released> = state<released>,
      state<chord_blocked> + event<single_pressed> = state<chord_blocked>
    );
  }
};

void replay_phase(sml::sm<RadarControlMachine>& machine, RadarControlPhase phase) {
  switch (phase) {
    case RadarControlPhase::Released:
      return;
    case RadarControlPhase::SingleHeld:
      machine.process_event(single_pressed{});
      return;
    case RadarControlPhase::ChordBlocked:
      machine.process_event(chord_pressed{});
      return;
  }
}

[[nodiscard]] RadarControlPhase read_phase(const sml::sm<RadarControlMachine>& machine) {
  if (machine.is(sml::state<chord_blocked>)) {
    return RadarControlPhase::ChordBlocked;
  }
  if (machine.is(sml::state<single_held>)) {
    return RadarControlPhase::SingleHeld;
  }
  return RadarControlPhase::Released;
}

void emit(DomainEventBus* event_bus, DomainEventType type) {
  if (event_bus == nullptr) {
    return;
  }
  event_bus->enqueue(type, DomainEvent{.type = type});
}

}  // namespace

RadarControlFrame update_radar_control(
  RadarControlModel& control,
  const SemanticInputFrame& input,
  DomainEventBus* event_bus
) {
  const RadarControlPhase previous_phase = control.phase;
  const bool chord_active = input.clear_targets_active || (input.target_cycle_active && input.enemy_target_cycle_active);
  const bool single_active = !chord_active && (input.target_cycle_active || input.enemy_target_cycle_active);

  sml::sm<RadarControlMachine> machine;
  replay_phase(machine, control.phase);
  if (chord_active) {
    machine.process_event(chord_pressed{});
  } else if (single_active) {
    machine.process_event(single_pressed{});
  } else {
    machine.process_event(shoulders_released{});
  }
  control.phase = read_phase(machine);

  RadarControlFrame frame{};
  if (control.phase == RadarControlPhase::ChordBlocked && previous_phase != RadarControlPhase::ChordBlocked) {
    frame.clear_targets_requested = true;
    control.focus = RadarFocus::None;
    emit(event_bus, DomainEventType::RadarTargetsCleared);
    return frame;
  }

  if (control.phase == RadarControlPhase::SingleHeld && previous_phase == RadarControlPhase::Released) {
    frame.mining_target_cycle_requested = input.target_cycle_active;
    frame.enemy_target_cycle_requested = input.enemy_target_cycle_active;
    if (frame.mining_target_cycle_requested) {
      control.focus = RadarFocus::Mining;
      emit(event_bus, DomainEventType::MiningTargetCycleRequested);
    }
    if (frame.enemy_target_cycle_requested) {
      control.focus = RadarFocus::Combat;
      emit(event_bus, DomainEventType::EnemyTargetCycleRequested);
    }
  }

  return frame;
}

}  // namespace hyperverse
