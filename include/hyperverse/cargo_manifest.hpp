#pragma once

#include "hyperverse/mining.hpp"

#include <entt/entity/registry.hpp>

#include <array>

namespace hyperverse {

struct CargoManifest {
  float delivered_mass{0.0F};
  std::array<float, OreTierCount> delivered_mass_by_tier{};
  float cash{0.0F};
  int score{0};
  int cargo_boxes{0};
};

struct ContractQuotaTuning {
  float required_mass{40.0F};
  float cargo_box_mass{10.0F};
  float over_quota_bonus_step_mass{20.0F};
  float bonus_per_step{0.15F};
  std::array<float, OreTierCount> cash_per_mass{1.0F, 2.0F, 5.0F, 12.0F, 30.0F};
  float score_per_cash{10.0F};
};

struct CargoHudSnapshot {
  float delivered_mass{0.0F};
  std::array<float, OreTierCount> delivered_mass_by_tier{};
  float cash{0.0F};
  int score{0};
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
