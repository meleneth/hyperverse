#include "hyperverse/cargo_box.hpp"

#include <algorithm>
#include <limits>

namespace hyperverse {
namespace {

[[nodiscard]] OreTier ore_tier_for_box(const CargoManifest& manifest, int box_index, float box_mass) {
  const float midpoint_mass = (static_cast<float>(box_index) + 0.5F) * box_mass;
  float cumulative_mass = 0.0F;
  for (int tier_index = 0; tier_index < OreTierCount; ++tier_index) {
    cumulative_mass += manifest.delivered_mass_by_tier[static_cast<std::size_t>(tier_index)];
    if (midpoint_mass <= cumulative_mass) {
      return static_cast<OreTier>(tier_index);
    }
  }
  return OreTier::Common;
}

void emit_box_created(DomainEventBus* event_bus, entt::entity box, const CargoBox& cargo_box) {
  if (event_bus == nullptr) {
    return;
  }
  event_bus->enqueue(
    DomainEventType::CargoBoxCreated,
    DomainEvent{
      .type = DomainEventType::CargoBoxCreated,
      .subject = box,
      .position = cargo_box.position,
      .amount = cargo_box.mass,
      .count = cargo_box.index,
    }
  );
}

[[nodiscard]] Vec2 cargo_slot_position(const ExtractionSite& gathering_site, const CargoBoxTuning& tuning, const SectorTuning& sector, int box_index) {
  const int columns = std::max(1, tuning.gathering_columns);
  const int column = box_index % columns;
  const int row = box_index / columns;
  const float centered_column = static_cast<float>(column) - ((static_cast<float>(columns) - 1.0F) * 0.5F);
  return wrap_position(
    {
      .x = gathering_site.position.x + (centered_column * tuning.box_spacing),
      .y = gathering_site.position.y + (static_cast<float>(row) * tuning.box_spacing),
    },
    sector
  );
}

void move_box_toward(CargoBox& box, Vec2 target, const SectorTuning& sector, float dt_seconds, const CargoBoxTuning& tuning) {
  const float scaled_dt = std::max(0.0F, dt_seconds);
  const Vec2 delta = wrapped_delta(box.position, target, sector);
  const float distance = length(delta);
  if (distance <= 0.001F || scaled_dt <= 0.0F) {
    box.velocity = {};
    return;
  }

  const Vec2 desired_velocity = clamp_length(delta * tuning.gathering_follow_rate, tuning.gathering_max_speed);
  const Vec2 step = desired_velocity * scaled_dt;
  if (length(step) >= distance) {
    box.velocity = delta * (1.0F / std::max(scaled_dt, std::numeric_limits<float>::epsilon()));
    box.position = target;
  } else {
    box.velocity = desired_velocity;
    box.position = wrap_position(box.position + step, sector);
  }
}

}  // namespace

int sync_cargo_boxes(
  entt::registry& registry,
  const CargoManifest& manifest,
  const ExtractionSite& extraction_site,
  const CargoBoxTuning& tuning,
  Vec2 pickup_origin,
  DomainEventBus* event_bus
) {
  int existing_boxes = 0;
  for (auto [entity, box] : registry.view<CargoBox>().each()) {
    (void)entity;
    box.index = existing_boxes;
    box.mass = tuning.box_mass;
    box.tier = ore_tier_for_box(manifest, existing_boxes, tuning.box_mass);
    ++existing_boxes;
  }

  while (existing_boxes < manifest.cargo_boxes) {
    const bool has_pickup_origin = pickup_origin.x != 0.0F || pickup_origin.y != 0.0F;
    const Vec2 spawn_position = has_pickup_origin ? pickup_origin : Vec2{.x = extraction_site.position.x + (static_cast<float>(existing_boxes) * tuning.box_spacing), .y = extraction_site.position.y};
    const entt::entity box_entity = registry.create();
    registry.emplace<CargoBox>(
      box_entity,
      CargoBox{
        .position = spawn_position,
        .mass = tuning.box_mass,
        .index = existing_boxes,
        .tier = ore_tier_for_box(manifest, existing_boxes, tuning.box_mass),
        .state = has_pickup_origin ? CargoBoxState::PendingPickup : CargoBoxState::Linked,
      }
    );
    emit_box_created(event_bus, box_entity, registry.get<CargoBox>(box_entity));
    ++existing_boxes;
  }

  return existing_boxes;
}

int update_gathered_cargo_boxes(
  entt::registry& registry,
  const ExtractionSite& gathering_site,
  const SectorTuning& sector,
  float dt_seconds,
  const CargoBoxTuning& tuning
) {
  int moved_boxes = 0;
  for (auto [entity, box] : registry.view<CargoBox>().each()) {
    (void)entity;
    if (box.state != CargoBoxState::Linked) {
      continue;
    }
    move_box_toward(box, cargo_slot_position(gathering_site, tuning, sector, box.index), sector, dt_seconds, tuning);
    ++moved_boxes;
  }
  return moved_boxes;
}

}  // namespace hyperverse
