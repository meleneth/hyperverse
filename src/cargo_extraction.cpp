#include "hyperverse/cargo_extraction.hpp"

#include <boost/sml.hpp>

#include <algorithm>
#include <limits>
#include <vector>

namespace hyperverse {
namespace {

namespace sml = boost::sml;

struct idle_phase {};
struct queueing_phase {};
struct moving_active_to_gate_phase {};
struct extracting_active_phase {};
struct complete_phase {};
struct begin_queue {};
struct active_needs_gate {};
struct active_at_gate {};
struct active_extracted {};
struct queue_empty {};
struct reset_extraction {};

struct CargoExtractionMachine {
  auto operator()() const {
    using namespace sml;
    return make_transition_table(
      *state<idle_phase> + event<begin_queue> = state<queueing_phase>,
      state<queueing_phase> + event<active_needs_gate> = state<moving_active_to_gate_phase>,
      state<queueing_phase> + event<active_at_gate> = state<extracting_active_phase>,
      state<queueing_phase> + event<queue_empty> = state<complete_phase>,
      state<moving_active_to_gate_phase> + event<active_at_gate> = state<extracting_active_phase>,
      state<moving_active_to_gate_phase> + event<queue_empty> = state<complete_phase>,
      state<extracting_active_phase> + event<active_extracted> = state<queueing_phase>,
      state<extracting_active_phase> + event<queue_empty> = state<complete_phase>,
      state<complete_phase> + event<reset_extraction> = state<idle_phase>
    );
  }
};

void replay_phase(sml::sm<CargoExtractionMachine>& machine, CargoExtractionPhase phase) {
  switch (phase) {
    case CargoExtractionPhase::Idle:
      return;
    case CargoExtractionPhase::Queueing:
      machine.process_event(begin_queue{});
      return;
    case CargoExtractionPhase::MovingActiveToGate:
      machine.process_event(begin_queue{});
      machine.process_event(active_needs_gate{});
      return;
    case CargoExtractionPhase::ExtractingActive:
      machine.process_event(begin_queue{});
      machine.process_event(active_at_gate{});
      return;
    case CargoExtractionPhase::Complete:
      machine.process_event(begin_queue{});
      machine.process_event(queue_empty{});
      return;
  }
}

[[nodiscard]] CargoExtractionPhase read_phase(const sml::sm<CargoExtractionMachine>& machine) {
  if (machine.is(sml::state<complete_phase>)) {
    return CargoExtractionPhase::Complete;
  }
  if (machine.is(sml::state<extracting_active_phase>)) {
    return CargoExtractionPhase::ExtractingActive;
  }
  if (machine.is(sml::state<moving_active_to_gate_phase>)) {
    return CargoExtractionPhase::MovingActiveToGate;
  }
  if (machine.is(sml::state<queueing_phase>)) {
    return CargoExtractionPhase::Queueing;
  }
  return CargoExtractionPhase::Idle;
}

[[nodiscard]] bool transition_extraction(CargoExtractionModel& model, CargoExtractionTransition transition) {
  sml::sm<CargoExtractionMachine> machine;
  replay_phase(machine, model.phase);
  const CargoExtractionPhase previous = model.phase;
  switch (transition) {
    case CargoExtractionTransition::BeginQueue:
      machine.process_event(begin_queue{});
      break;
    case CargoExtractionTransition::ActiveNeedsGate:
      machine.process_event(active_needs_gate{});
      break;
    case CargoExtractionTransition::ActiveAtGate:
      machine.process_event(active_at_gate{});
      break;
    case CargoExtractionTransition::ActiveExtracted:
      machine.process_event(active_extracted{});
      break;
    case CargoExtractionTransition::QueueEmpty:
      machine.process_event(queue_empty{});
      break;
    case CargoExtractionTransition::Reset:
      machine.process_event(reset_extraction{});
      break;
  }
  model.phase = read_phase(machine);
  if (model.phase == CargoExtractionPhase::Complete || model.phase == CargoExtractionPhase::Idle) {
    model.active_box = entt::null;
  }
  return model.phase != previous;
}

void emit_box_extracted(DomainEventBus* event_bus, entt::entity box, const CargoBox& cargo_box, Vec2 gate_position) {
  if (event_bus == nullptr) {
    return;
  }
  event_bus->enqueue(
    DomainEventType::CargoBoxExtracted,
    DomainEvent{
      .type = DomainEventType::CargoBoxExtracted,
      .subject = box,
      .position = gate_position,
      .amount = cargo_box.mass,
      .count = cargo_box.index,
    }
  );
}

void emit_extraction_complete(DomainEventBus* event_bus, Vec2 gate_position, int box_count) {
  if (event_bus == nullptr) {
    return;
  }
  event_bus->enqueue(
    DomainEventType::CargoExtractionComplete,
    DomainEvent{
      .type = DomainEventType::CargoExtractionComplete,
      .position = gate_position,
      .count = box_count,
    }
  );
  event_bus->enqueue(
    DomainEventType::ContractRoundCompleted,
    DomainEvent{
      .type = DomainEventType::ContractRoundCompleted,
      .position = gate_position,
      .count = box_count,
    }
  );
}

[[nodiscard]] Vec2 queue_position(const CargoEscortRoute& route, const CargoExtractionTuning& tuning, const SectorTuning& sector, int queue_index) {
  const int columns = std::max(1, tuning.formation_columns);
  const int column = queue_index % columns;
  const int row = queue_index / columns;
  const float centered_column = static_cast<float>(column) - ((static_cast<float>(columns) - 1.0F) * 0.5F);
  return wrap_position(
    {
      .x = route.gate_position.x + (centered_column * tuning.staging_spacing),
      .y = route.gate_position.y + tuning.staging_spacing + (static_cast<float>(row) * tuning.staging_spacing),
    },
    sector
  );
}

void move_box_toward(CargoBox& box, Vec2 target, const SectorTuning& sector, float dt_seconds, const CargoExtractionTuning& tuning) {
  const float scaled_dt = std::max(0.0F, dt_seconds);
  const Vec2 delta = wrapped_delta(box.position, target, sector);
  const float distance = length(delta);
  if (distance <= 0.001F || scaled_dt <= 0.0F) {
    box.velocity = {};
    return;
  }

  const Vec2 velocity = clamp_length(delta * tuning.approach_rate, tuning.max_speed);
  const Vec2 step = velocity * scaled_dt;
  if (length(step) >= distance) {
    box.velocity = delta * (1.0F / std::max(scaled_dt, std::numeric_limits<float>::epsilon()));
    box.position = target;
  } else {
    box.velocity = velocity;
    box.position = wrap_position(box.position + step, sector);
  }
}

}  // namespace

CargoExtractionHudSnapshot update_cargo_extraction(
  CargoExtractionModel& model,
  entt::registry& registry,
  CargoEscortState& escort,
  const CargoEscortRoute& route,
  const SectorTuning& sector,
  float dt_seconds,
  DomainEventBus* event_bus,
  const CargoExtractionTuning& tuning
) {
  std::vector<entt::entity> boxes;
  for (auto [entity, box] : registry.view<CargoBox>().each()) {
    if (box.state != CargoBoxState::Lost && box.state != CargoBoxState::Stolen && box.state != CargoBoxState::Detached) {
      boxes.push_back(entity);
    }
  }
  std::ranges::sort(boxes, [&registry](entt::entity lhs, entt::entity rhs) {
    return registry.get<CargoBox>(lhs).index < registry.get<CargoBox>(rhs).index;
  });

  CargoExtractionHudSnapshot hud{
    .total_boxes = static_cast<int>(boxes.size()),
    .active = escort.phase == CargoEscortPhase::Extracting,
  };

  for (entt::entity entity : boxes) {
    if (registry.get<CargoBox>(entity).state == CargoBoxState::Extracted) {
      ++hud.extracted_boxes;
    }
  }

  if (escort.phase != CargoEscortPhase::Extracting) {
    if (model.phase != CargoExtractionPhase::Idle && escort.phase != CargoEscortPhase::Complete) {
      (void)transition_extraction(model, CargoExtractionTransition::Reset);
    }
    hud.complete = escort.phase == CargoEscortPhase::Complete;
    return hud;
  }

  if (model.phase == CargoExtractionPhase::Idle) {
    (void)transition_extraction(model, CargoExtractionTransition::BeginQueue);
  }

  for (entt::entity entity : boxes) {
    CargoBox& box = registry.get<CargoBox>(entity);
    if (box.state == CargoBoxState::Linked || box.state == CargoBoxState::BeingHauled || box.state == CargoBoxState::PendingPickup) {
      (void)transition_cargo_box(box, CargoBoxTransition::SendToGate, entity, event_bus);
      box.velocity = {};
    }
  }

  std::vector<entt::entity> queued_boxes;
  for (entt::entity entity : boxes) {
    CargoBox& box = registry.get<CargoBox>(entity);
    if (box.state != CargoBoxState::Extracted) {
      queued_boxes.push_back(entity);
    }
  }

  if (queued_boxes.empty()) {
    (void)transition_extraction(model, CargoExtractionTransition::QueueEmpty);
    (void)transition_cargo_escort(escort, CargoEscortTransition::ExtractionComplete);
    hud.active = false;
    hud.complete = true;
    emit_extraction_complete(event_bus, route.gate_position, hud.total_boxes);
    return hud;
  }

  const entt::entity active_box = queued_boxes.front();
  model.active_box = active_box;
  for (std::size_t queue_index = 1; queue_index < queued_boxes.size(); ++queue_index) {
    CargoBox& queued = registry.get<CargoBox>(queued_boxes[queue_index]);
    (void)transition_cargo_box(queued, CargoBoxTransition::SendToGate, queued_boxes[queue_index], event_bus);
    move_box_toward(queued, queue_position(route, tuning, sector, static_cast<int>(queue_index - 1U)), sector, dt_seconds, tuning);
  }

  CargoBox& box = registry.get<CargoBox>(active_box);
  hud.active_box_index = box.index;
  if (length(wrapped_delta(box.position, route.gate_position, sector)) > tuning.gate_radius) {
    (void)transition_extraction(model, CargoExtractionTransition::ActiveNeedsGate);
    (void)transition_cargo_box(box, CargoBoxTransition::SendToGate, active_box, event_bus);
    move_box_toward(box, route.gate_position, sector, dt_seconds, tuning);
  } else {
    (void)transition_extraction(model, CargoExtractionTransition::ActiveAtGate);
    (void)transition_cargo_box(box, CargoBoxTransition::StartExtraction, active_box, event_bus);
    box.velocity = {};
    box.extraction_seconds = std::min(tuning.seconds_per_box, box.extraction_seconds + std::max(0.0F, dt_seconds));
    if (box.extraction_seconds >= tuning.seconds_per_box) {
      (void)transition_cargo_box(box, CargoBoxTransition::FinishExtraction, active_box, event_bus);
      (void)transition_extraction(model, CargoExtractionTransition::ActiveExtracted);
      ++hud.extracted_boxes;
      emit_box_extracted(event_bus, active_box, box, route.gate_position);
    }
  }

  hud.active_box_fraction = box.state == CargoBoxState::Extracted ? 1.0F : box.extraction_seconds / std::max(tuning.seconds_per_box, std::numeric_limits<float>::epsilon());
  return hud;
}

}  // namespace hyperverse
