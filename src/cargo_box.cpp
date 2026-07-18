#include "hyperverse/cargo_box.hpp"

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

}  // namespace hyperverse
