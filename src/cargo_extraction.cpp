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
    hud.complete = escort.phase == CargoEscortPhase::Complete;
    return hud;
  }

  for (entt::entity entity : boxes) {
    CargoBox& box = registry.get<CargoBox>(entity);
    if (box.state == CargoBoxState::Linked || box.state == CargoBoxState::BeingHauled || box.state == CargoBoxState::PendingPickup) {
      box.state = CargoBoxState::GateBound;
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
    escort.phase = CargoEscortPhase::Complete;
    hud.active = false;
    hud.complete = true;
    emit_extraction_complete(event_bus, route.gate_position, hud.total_boxes);
    return hud;
  }

  const entt::entity active_box = queued_boxes.front();
  for (std::size_t queue_index = 1; queue_index < queued_boxes.size(); ++queue_index) {
    CargoBox& queued = registry.get<CargoBox>(queued_boxes[queue_index]);
    queued.state = CargoBoxState::GateBound;
    move_box_toward(queued, queue_position(route, tuning, sector, static_cast<int>(queue_index - 1U)), sector, dt_seconds, tuning);
  }

  CargoBox& box = registry.get<CargoBox>(active_box);
  hud.active_box_index = box.index;
  if (length(wrapped_delta(box.position, route.gate_position, sector)) > tuning.gate_radius) {
    box.state = CargoBoxState::GateBound;
    move_box_toward(box, route.gate_position, sector, dt_seconds, tuning);
  } else {
    box.state = CargoBoxState::Extracting;
    box.velocity = {};
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
