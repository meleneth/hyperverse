#include "hyperverse/projectile.hpp"

#include "jolt_shape_queries.hpp"

#include <boost/sml.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace hyperverse {
namespace {

namespace sml = boost::sml;

constexpr float RaiderCollisionRadius = 52.0F;

struct ready_phase {};
struct cooling_phase {};
struct trigger_held {};
struct cooldown_elapsed {};

struct ParticleCannonMachine {
  auto operator()() const {
    using namespace sml;
    return make_transition_table(
      *state<ready_phase> + event<trigger_held> = state<cooling_phase>,
      state<cooling_phase> + event<cooldown_elapsed> = state<ready_phase>
    );
  }
};

[[nodiscard]] Vec2 facing_direction(float radians) {
  return {.x = std::cos(radians), .y = std::sin(radians)};
}

void replay_phase(sml::sm<ParticleCannonMachine>& machine, ParticleCannonPhase phase) {
  if (phase == ParticleCannonPhase::Cooling) {
    machine.process_event(trigger_held{});
  }
}

[[nodiscard]] ParticleCannonPhase read_phase(const sml::sm<ParticleCannonMachine>& machine) {
  return machine.is(sml::state<cooling_phase>) ? ParticleCannonPhase::Cooling : ParticleCannonPhase::Ready;
}

[[nodiscard]] bool advance_particle_cannon_fsm(ParticleCannonModel& model, bool trigger_active, float dt_seconds, const ParticleCannonTuning& tuning) {
  sml::sm<ParticleCannonMachine> machine;
  replay_phase(machine, model.phase);

  model.cooldown_seconds = std::max(0.0F, model.cooldown_seconds - std::max(0.0F, dt_seconds));
  if (model.phase == ParticleCannonPhase::Cooling && model.cooldown_seconds <= 0.0F) {
    machine.process_event(cooldown_elapsed{});
    model.phase = read_phase(machine);
  }

  if (model.phase == ParticleCannonPhase::Ready && trigger_active) {
    machine.process_event(trigger_held{});
    model.phase = read_phase(machine);
    model.cooldown_seconds = std::max(0.0F, tuning.fire_interval_seconds);
    return true;
  }

  model.phase = read_phase(machine);
  return false;
}

void apply_projectile_damage(AsteroidBody& asteroid, MiningResource& resource, const ParticleShot& projectile, const ParticleCannonTuning& tuning) {
  resource.integrity = std::max(0.0F, resource.integrity - projectile.damage);
  const float remaining_fraction = std::clamp(resource.integrity / 100.0F, tuning.asteroid_min_radius_fraction, 1.0F);
  asteroid.radius = std::max(MinimumPlayableAsteroidRadius, asteroid.base_radius * remaining_fraction);
}

void fragment_depleted_asteroid(
  entt::registry& registry,
  entt::entity asteroid,
  const ParticleShot& projectile,
  const ParticleCannonTuning& tuning
) {
  if (!registry.valid(asteroid) || !registry.all_of<MiningResource>(asteroid) || registry.get<MiningResource>(asteroid).integrity > 0.0F) {
    return;
  }

  (void)fragment_asteroid(
    registry,
    asteroid,
    AsteroidFragmentationRequest{
      .impact_kind = tuning.impact_kind,
      .impact_position = projectile.position,
      .impact_velocity = projectile.velocity,
      .pieces = 4,
    }
  );
}

void apply_ship_damage(ShipHealth& health, float damage) {
  const float shield_damage = std::min(health.shields, damage);
  health.shields -= shield_damage;
  health.armor = std::max(0.0F, health.armor - (damage - shield_damage));
}

void spawn_particle_pair(
  entt::registry& registry,
  DomainEventBus& event_bus,
  Vec2 origin,
  Vec2 direction,
  Vec2 source_velocity,
  ProjectileOwner owner,
  const ParticleCannonTuning& tuning
) {
  const Vec2 normalized_direction = normalize_or_zero(direction);
  const Vec2 right{.x = -normalized_direction.y, .y = normalized_direction.x};
  const Vec2 velocity = (normalized_direction * tuning.projectile_speed) + source_velocity;

  for (const float side : {-1.0F, 1.0F}) {
    const entt::entity projectile = registry.create();
    registry.emplace<ParticleShot>(
      projectile,
      ParticleShot{
        .position = origin + (normalized_direction * tuning.muzzle_forward_offset) + (right * tuning.muzzle_side_offset * side),
        .velocity = velocity,
        .damage = tuning.damage,
        .radius = tuning.projectile_radius,
        .owner = owner,
      }
    );
    event_bus.enqueue(
      DomainEventType::ParticleFired,
      DomainEvent{
        .type = DomainEventType::ParticleFired,
        .subject = projectile,
        .position = origin,
        .amount = tuning.projectile_speed,
      }
    );
  }
}

}  // namespace

WeaponCtx::WeaponCtx(EntityCtx entity) : entity_{entity} {}

EntityCtx WeaponCtx::entity_context() const {
  return entity_;
}

entt::entity WeaponCtx::self() const {
  return entity_.self();
}

entt::registry& WeaponCtx::registry() const {
  return entity_.registry();
}

DomainEventBus& WeaponCtx::event_bus() const {
  return entity_.event_bus();
}

const SectorTuning& WeaponCtx::sector() const {
  return entity_.sector();
}

float WeaponCtx::dt() const {
  return entity_.dt();
}

ParticleCannonModel& WeaponCtx::cannon() const {
  return entity_.get<ParticleCannonModel>();
}

ProjectileSimCtx::ProjectileSimCtx(SectorTickCtx tick, entt::entity player) : tick_{tick}, player_{player} {}

entt::registry& ProjectileSimCtx::registry() const {
  return tick_.registry();
}

DomainEventBus& ProjectileSimCtx::event_bus() const {
  return tick_.event_bus();
}

const SectorTuning& ProjectileSimCtx::sector() const {
  return tick_.sector();
}

float ProjectileSimCtx::dt() const {
  return tick_.dt();
}

entt::entity ProjectileSimCtx::player() const {
  return player_;
}

const ShipMotion& ProjectileSimCtx::player_motion() const {
  return registry().get<ShipMotion>(player_);
}

ShipHealth& ProjectileSimCtx::player_health() const {
  return registry().get<ShipHealth>(player_);
}

void update_player_particle_cannon(
  WeaponCtx ctx,
  WeaponTrigger trigger,
  const ParticleCannonTuning& tuning
) {
  if (!advance_particle_cannon_fsm(ctx.cannon(), trigger.active, ctx.dt(), tuning)) {
    return;
  }

  const EntityCtx owner = ctx.entity_context();
  const ShipMotion& ship = owner.get<ShipMotion>();
  const Vec2 direction = length(trigger.aim) > 0.0F ? normalize_or_zero(trigger.aim) : facing_direction(ship.facing_radians);
  spawn_particle_pair(ctx.registry(), ctx.event_bus(), ship.position, direction, ship.velocity, ProjectileOwner::Player, tuning);
}

void update_raider_particle_cannon(
  WeaponCtx ctx,
  EntityCtx target,
  WeaponTrigger trigger,
  const ParticleCannonTuning& tuning
) {
  const EntityCtx raider_entity = ctx.entity_context();
  const RaiderShip& raider = raider_entity.get<RaiderShip>();
  const ShipMotion& target_motion = target.get<ShipMotion>();
  const Vec2 to_target = wrapped_delta(raider.position, target_motion.position, ctx.sector());
  const bool trigger_active = trigger.active && raider.integrity > 0.0F && length(to_target) <= tuning.raider_fire_range;
  if (!advance_particle_cannon_fsm(ctx.cannon(), trigger_active, ctx.dt(), tuning) || length(to_target) <= 0.0001F) {
    return;
  }

  spawn_particle_pair(ctx.registry(), ctx.event_bus(), raider.position, normalize_or_zero(to_target), raider.velocity, ProjectileOwner::Raider, tuning);
}

ParticleCannonHudSnapshot update_particle_projectiles(
  ProjectileSimCtx ctx,
  const ParticleCannonTuning& tuning
) {
  entt::registry& registry = ctx.registry();
  ParticleCannonHudSnapshot hud{};
  std::vector<entt::entity> expired;
  for (auto [projectile_entity, projectile] : registry.view<ParticleShot>().each()) {
    projectile.ttl_seconds -= std::max(0.0F, ctx.dt());
    projectile.position = wrap_position(projectile.position + (projectile.velocity * std::max(0.0F, ctx.dt())), ctx.sector());
    bool hit = false;

    if (projectile.owner == ProjectileOwner::Player) {
      for (auto [asteroid_entity, asteroid, resource] : registry.view<AsteroidBody, MiningResource>().each()) {
        if (resource.integrity <= 0.0F) {
          continue;
        }

        const Vec2 relative_position = wrapped_delta(projectile.position, asteroid.position, ctx.sector());
        if (jolt_shapes_overlap(
              SpriteCollisionShape::Particle,
              projectile.radius,
              SpriteCollisionShape::Rock,
              asteroid.radius,
              relative_position
            )) {
          apply_projectile_damage(asteroid, resource, projectile, tuning);
          ctx.event_bus().enqueue(
            DomainEventType::ParticleImpact,
            DomainEvent{
              .type = DomainEventType::ParticleImpact,
              .subject = projectile_entity,
              .target = asteroid_entity,
              .position = projectile.position,
              .amount = projectile.damage,
            }
          );
          fragment_depleted_asteroid(registry, asteroid_entity, projectile, tuning);
          hit = true;
          break;
        }
      }

      if (!hit) {
        for (auto [raider_entity, raider] : registry.view<RaiderShip>().each()) {
          if (raider.integrity <= 0.0F) {
            continue;
          }

          const Vec2 relative_position = wrapped_delta(projectile.position, raider.position, ctx.sector());
          if (jolt_shapes_overlap(
                SpriteCollisionShape::Particle,
                projectile.radius,
                SpriteCollisionShape::Ship,
                RaiderCollisionRadius,
                relative_position
              )) {
            raider.integrity = std::max(0.0F, raider.integrity - projectile.damage);
            ctx.event_bus().enqueue(
              DomainEventType::ParticleImpact,
              DomainEvent{
                .type = DomainEventType::ParticleImpact,
                .subject = projectile_entity,
                .target = raider_entity,
                .position = projectile.position,
                .amount = projectile.damage,
              }
            );
            hit = true;
            break;
          }
        }
      }
    } else {
      const Vec2 relative_position = wrapped_delta(projectile.position, ctx.player_motion().position, ctx.sector());
      if (jolt_shapes_overlap(
            SpriteCollisionShape::Particle,
            projectile.radius,
            SpriteCollisionShape::Ship,
            RaiderCollisionRadius,
            relative_position
          )) {
        apply_ship_damage(ctx.player_health(), projectile.damage);
        ctx.event_bus().enqueue(
          DomainEventType::ParticleImpact,
          DomainEvent{
            .type = DomainEventType::ParticleImpact,
            .subject = projectile_entity,
            .target = entt::null,
            .position = projectile.position,
            .amount = projectile.damage,
          }
        );
        hit = true;
      }
    }

    if (hit || projectile.ttl_seconds <= 0.0F) {
      expired.push_back(projectile_entity);
    } else {
      ++hud.active_particles;
    }
  }

  for (entt::entity projectile : expired) {
    registry.destroy(projectile);
  }

  return hud;
}

}  // namespace hyperverse
