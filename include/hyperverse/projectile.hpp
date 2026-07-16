#pragma once

#include "hyperverse/flight.hpp"
#include "hyperverse/input.hpp"
#include "hyperverse/mining.hpp"
#include "hyperverse/sector.hpp"
#include "hyperverse/targeting.hpp"

namespace hyperverse {

class AccountCtx;

struct ParticleShot {
  Vec2 position{};
  Vec2 velocity{};
  float ttl_seconds{1.8F};
};

struct ParticleCannonTuning {
  float projectile_speed{1450.0F};
  float projectile_radius{10.0F};
  float damage{22.0F};
  float asteroid_min_radius_fraction{0.12F};
};

struct ParticleCannonHudSnapshot {
  int active_particles{0};
  int impacts{0};
};

[[nodiscard]] ParticleCannonHudSnapshot update_particle_cannon(
  AccountCtx& ctx,
  const ShipMotion& ship,
  const SemanticInputFrame& input,
  const SectorTuning& sector,
  float dt_seconds,
  const ParticleCannonTuning& tuning = {}
);

}  // namespace hyperverse
