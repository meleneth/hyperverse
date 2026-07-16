#pragma once

#include <entt/entity/registry.hpp>

namespace hyperverse {

struct CargoManifest {
  float delivered_mass{0.0F};
  int cargo_boxes{0};
};

struct ContractQuotaTuning {
  float required_mass{40.0F};
  float cargo_box_mass{10.0F};
  float over_quota_bonus_step_mass{20.0F};
  float bonus_per_step{0.15F};
};

struct CargoHudSnapshot {
  float delivered_mass{0.0F};
  float required_mass{0.0F};
  float quota_fraction{0.0F};
  float over_quota_mass{0.0F};
  float payout_multiplier{1.0F};
  int cargo_boxes{0};
  bool extraction_authorized{false};
};

[[nodiscard]] CargoHudSnapshot update_cargo_manifest(
  CargoManifest& manifest,
  entt::registry& registry,
  const ContractQuotaTuning& tuning = {}
);

}  // namespace hyperverse
