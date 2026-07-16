#include "hyperverse/cargo_extraction.hpp"

#include <algorithm>
#include <limits>
#include <vector>

namespace hyperverse {
namespace {

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

}  // namespace

CargoExtractionHudSnapshot update_cargo_extraction(
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
    if (box.state != CargoBoxState::Lost && box.state != CargoBoxState::Stolen) {
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
    hud.complete = escort.phase == CargoEscortPhase::Complete;
    return hud;
  }

  for (entt::entity entity : boxes) {
    CargoBox& box = registry.get<CargoBox>(entity);
    if (box.state == CargoBoxState::Linked) {
      box.state = CargoBoxState::GateBound;
    }
  }

  entt::entity active_box = entt::null;
  for (entt::entity entity : boxes) {
    CargoBox& box = registry.get<CargoBox>(entity);
    if (box.state != CargoBoxState::Extracted) {
      active_box = entity;
      hud.active_box_index = box.index;
      break;
    }
  }

  if (active_box == entt::null) {
    escort.phase = CargoEscortPhase::Complete;
    hud.active = false;
    hud.complete = true;
    emit_extraction_complete(event_bus, route.gate_position, hud.total_boxes);
    return hud;
  }

  CargoBox& box = registry.get<CargoBox>(active_box);
  const Vec2 to_gate = wrapped_delta(box.position, route.gate_position, sector);
  const float gate_distance = length(to_gate);
  if (gate_distance > tuning.gate_radius) {
    box.state = CargoBoxState::GateBound;
    box.velocity = clamp_length(to_gate * tuning.approach_rate, tuning.max_speed);
    box.position = wrap_position(box.position + (box.velocity * std::max(0.0F, dt_seconds)), sector);
  } else {
    box.state = CargoBoxState::Extracting;
    box.velocity = {};
    box.position = route.gate_position;
    box.extraction_seconds = std::min(tuning.seconds_per_box, box.extraction_seconds + std::max(0.0F, dt_seconds));
    if (box.extraction_seconds >= tuning.seconds_per_box) {
      box.state = CargoBoxState::Extracted;
      ++hud.extracted_boxes;
      emit_box_extracted(event_bus, active_box, box, route.gate_position);
    }
  }

  hud.active_box_fraction = box.state == CargoBoxState::Extracted ? 1.0F : box.extraction_seconds / std::max(tuning.seconds_per_box, std::numeric_limits<float>::epsilon());
  return hud;
}

}  // namespace hyperverse
