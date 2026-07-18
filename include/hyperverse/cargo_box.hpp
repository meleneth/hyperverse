#pragma once

#include "hyperverse/cargo_manifest.hpp"
#include "hyperverse/domain_events.hpp"
#include "hyperverse/math.hpp"
#include "hyperverse/sector.hpp"

#include <entt/entity/registry.hpp>

namespace hyperverse {

struct ExtractionSite {
  Vec2 position{};
};

enum class CargoBoxState {
  PendingPickup,
  BeingHauled,
  Linked,
  GateBound,
  Extracting,
  Extracted,
  Detached,
  Stolen,
  Lost,
};

struct CargoBox {
  Vec2 position{};
  Vec2 velocity{};
  float mass{0.0F};
  int index{0};
  OreTier tier{OreTier::Common};
  CargoBoxState state{CargoBoxState::Linked};
  float extraction_seconds{0.0F};
};

struct CargoBoxTuning {
  float box_mass{10.0F};
  float box_spacing{156.0F};
  int gathering_columns{5};
  float gathering_follow_rate{4.0F};
  float gathering_max_speed{420.0F};
};

int sync_cargo_boxes(
  entt::registry& registry,
  const CargoManifest& manifest,
  const ExtractionSite& extraction_site,
  const CargoBoxTuning& tuning = {},
  Vec2 pickup_origin = {},
  DomainEventBus* event_bus = nullptr
);

int update_gathered_cargo_boxes(
  entt::registry& registry,
  const ExtractionSite& gathering_site,
  const SectorTuning& sector,
  float dt_seconds,
  const CargoBoxTuning& tuning = {}
);

}  // namespace hyperverse
