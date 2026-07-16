#pragma once

#include "hyperverse/flight.hpp"
#include "hyperverse/input.hpp"
#include "hyperverse/sector.hpp"
#include "hyperverse/targeting.hpp"

#include <entt/entity/registry.hpp>

namespace hyperverse {

enum class OreTier {
  Common,
  Industrial,
  Rare,
  Exotic,
  Anomalous,
};

struct OreTint {
  float r{0.78F};
  float g{0.80F};
  float b{0.82F};
};

struct MineralComposition {
  float silicate{0.0F};
  float ferrite{0.0F};
  float nickel{0.0F};
  float cobalt{0.0F};
  float iridium{0.0F};
  float exotic_crystal{0.0F};
  float anomalous_matter{0.0F};
};

struct MiningResource {
  OreTier tier{OreTier::Common};
  float integrity{100.0F};
  float heat{0.0F};
  float structural_stress{0.0F};
  float volatile_pressure{0.0F};
  float extracted_mass{0.0F};
  bool venting{false};
};

[[nodiscard]] MineralComposition mineral_composition_for_tier(OreTier tier);
[[nodiscard]] OreTint ore_tint(OreTier tier);
[[nodiscard]] OreTint ore_tint(const MineralComposition& composition);

struct MiningLaserTuning {
  float range{1250.0F};
  float integrity_damage_per_second{18.0F};
  float extraction_per_second{8.0F};
  float heat_per_second{26.0F};
  float heat_decay_per_second{10.0F};
  float stress_per_second{18.0F};
  float stress_relief_per_second{6.0F};
  float pressure_per_second{12.0F};
  float pressure_vent_per_second{35.0F};
  float unstable_heat{70.0F};
  float unstable_stress{65.0F};
  float volatile_pressure_limit{55.0F};
  float blowout_integrity_damage{28.0F};
};

struct MiningHudSnapshot {
  bool beam_active{false};
  bool target_in_range{false};
  entt::entity target{entt::null};
  Vec2 beam_end_position{};
  float beam_intensity{0.0F};
  float target_integrity{0.0F};
  float target_heat{0.0F};
  float target_structural_stress{0.0F};
  float target_volatile_pressure{0.0F};
  float extracted_mass{0.0F};
  bool unstable{false};
  bool gas_venting{false};
  bool blowout{false};
};

[[nodiscard]] MiningHudSnapshot update_mining_laser(
  entt::registry& registry,
  const TargetLockModel& target_lock,
  const ShipMotion& ship,
  const SemanticInputFrame& input,
  const SectorTuning& sector,
  const MiningLaserTuning& tuning,
  float dt_seconds
);

}  // namespace hyperverse
