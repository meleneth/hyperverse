#pragma once

#include "hyperverse/asteroid_fragmentation.hpp"
#include "hyperverse/flight.hpp"
#include "hyperverse/game_context.hpp"
#include "hyperverse/input.hpp"
#include "hyperverse/mining.hpp"
#include "hyperverse/raider.hpp"
#include "hyperverse/sector.hpp"
#include "hyperverse/ship_status.hpp"
#include "hyperverse/targeting.hpp"

#include <optional>

namespace hyperverse {

enum class ParticleCannonPhase {
  Ready,
  Cooling,
};

enum class ProjectileOwner {
  Player,
  Raider,
};

struct ParticleCannonModel {
  ParticleCannonPhase phase{ParticleCannonPhase::Ready};
  float cooldown_seconds{0.0F};
};

struct ParticleShot {
  Vec2 position{};
  Vec2 velocity{};
  float ttl_seconds{1.8F};
  float damage{22.0F};
  float radius{10.0F};
  ProjectileOwner owner{ProjectileOwner::Player};
};

struct ParticleCannonTuning {
  float projectile_speed{1450.0F};
  float projectile_radius{10.0F};
  float damage{22.0F};
  float fire_interval_seconds{0.25F};
  float muzzle_forward_offset{46.0F};
  float muzzle_side_offset{14.0F};
  float raider_fire_range{1800.0F};
  float asteroid_kinetic_impulse_scale{0.18F};
  float asteroid_angular_impulse_scale{0.12F};
  AsteroidImpactKind impact_kind{AsteroidImpactKind::Kinetic};
};

struct ParticleCannonHudSnapshot {
  int active_particles{0};
  int impacts{0};
};

struct WeaponTrigger {
  Vec2 aim{};
  bool active{false};
};

struct ParticleCannonFireCommand {
  Vec2 origin{};
  Vec2 direction{};
  Vec2 source_velocity{};
  ProjectileOwner owner{ProjectileOwner::Player};
};

class WeaponCtx {
public:
  explicit WeaponCtx(EntityCtx entity);

  [[nodiscard]] EntityCtx entity_context() const;
  [[nodiscard]] entt::entity self() const;
  [[nodiscard]] entt::registry& registry() const;
  [[nodiscard]] DomainEventBus& event_bus() const;
  [[nodiscard]] const SectorTuning& sector() const;
  [[nodiscard]] float dt() const;
  [[nodiscard]] ParticleCannonModel& cannon() const;

private:
  EntityCtx entity_;
};

[[nodiscard]] std::optional<ParticleCannonFireCommand> request_player_particle_fire(
  WeaponCtx ctx,
  WeaponTrigger trigger,
  const ParticleCannonTuning& tuning = {}
);

[[nodiscard]] std::optional<ParticleCannonFireCommand> request_raider_particle_fire(
  WeaponCtx ctx,
  EntityCtx target,
  WeaponTrigger trigger,
  const ParticleCannonTuning& tuning = {}
);

void spawn_requested_particle_fire(
  WeaponCtx ctx,
  const ParticleCannonFireCommand& command,
  const ParticleCannonTuning& tuning = {}
);

class ProjectileSimCtx {
public:
  ProjectileSimCtx(SectorTickCtx tick, entt::entity player);

  [[nodiscard]] entt::registry& registry() const;
  [[nodiscard]] DomainEventBus& event_bus() const;
  [[nodiscard]] const SectorTuning& sector() const;
  [[nodiscard]] float dt() const;
  [[nodiscard]] entt::entity player() const;
  [[nodiscard]] const ShipMotion& player_motion() const;
  [[nodiscard]] ShipHealth& player_health() const;

private:
  SectorTickCtx tick_;
  entt::entity player_;
};

void update_player_particle_cannon(
  WeaponCtx ctx,
  WeaponTrigger trigger,
  const ParticleCannonTuning& tuning = {}
);

void update_raider_particle_cannon(
  WeaponCtx ctx,
  EntityCtx target,
  WeaponTrigger trigger,
  const ParticleCannonTuning& tuning = {}
);

[[nodiscard]] ParticleCannonHudSnapshot update_particle_projectiles(
  ProjectileSimCtx ctx,
  const ParticleCannonTuning& tuning = {}
);

}  // namespace hyperverse
