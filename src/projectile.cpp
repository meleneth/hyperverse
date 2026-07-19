#include "hyperverse/projectile.hpp"

#include "hyperverse/asteroid_collision.hpp"
#include "hyperverse/asteroid_mass.hpp"
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
struct ejected_phase {};
struct ignited_phase {};
struct ignition_elapsed {};

struct ParticleCannonMachine {
  auto operator()() const {
    using namespace sml;
    return make_transition_table(
      *state<ready_phase> + event<trigger_held> = state<cooling_phase>,
      state<cooling_phase> + event<cooldown_elapsed> = state<ready_phase>
    );
  }
};

struct HomingMissileLauncherMachine {
  auto operator()() const {
    using namespace sml;
    return make_transition_table(
      *state<ready_phase> + event<trigger_held> = state<cooling_phase>,
      state<cooling_phase> + event<cooldown_elapsed> = state<ready_phase>
    );
  }
};

struct HomingMissileFlightMachine {
  auto operator()() const {
    using namespace sml;
    return make_transition_table(*state<ejected_phase> + event<ignition_elapsed> = state<ignited_phase>);
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

void replay_phase(sml::sm<HomingMissileLauncherMachine>& machine, HomingMissileLauncherPhase phase) {
  if (phase == HomingMissileLauncherPhase::Cooling) {
    machine.process_event(trigger_held{});
  }
}

[[nodiscard]] HomingMissileLauncherPhase read_phase(const sml::sm<HomingMissileLauncherMachine>& machine) {
  return machine.is(sml::state<cooling_phase>) ? HomingMissileLauncherPhase::Cooling : HomingMissileLauncherPhase::Ready;
}

void replay_phase(sml::sm<HomingMissileFlightMachine>& machine, HomingMissilePhase phase) {
  if (phase == HomingMissilePhase::Ignited) {
    machine.process_event(ignition_elapsed{});
  }
}

[[nodiscard]] HomingMissilePhase read_phase(const sml::sm<HomingMissileFlightMachine>& machine) {
  return machine.is(sml::state<ignited_phase>) ? HomingMissilePhase::Ignited : HomingMissilePhase::Ejected;
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

[[nodiscard]] bool advance_homing_missile_launcher_fsm(
  HomingMissileLauncherModel& model,
  bool trigger_active,
  float dt_seconds,
  const HomingMissileTuning& tuning
) {
  sml::sm<HomingMissileLauncherMachine> machine;
  replay_phase(machine, model.phase);

  model.cooldown_seconds = std::max(0.0F, model.cooldown_seconds - std::max(0.0F, dt_seconds));
  if (model.phase == HomingMissileLauncherPhase::Cooling && model.cooldown_seconds <= 0.0F) {
    machine.process_event(cooldown_elapsed{});
    model.phase = read_phase(machine);
  }

  if (model.phase == HomingMissileLauncherPhase::Ready && trigger_active) {
    machine.process_event(trigger_held{});
    model.phase = read_phase(machine);
    model.cooldown_seconds = std::max(0.0F, tuning.cooldown_seconds);
    return true;
  }

  model.phase = read_phase(machine);
  return false;
}

[[nodiscard]] bool advance_homing_missile_flight_fsm(HomingMissile& missile, float dt_seconds) {
  sml::sm<HomingMissileFlightMachine> machine;
  replay_phase(machine, missile.phase);

  if (missile.phase == HomingMissilePhase::Ejected) {
    missile.ignition_seconds_remaining = std::max(0.0F, missile.ignition_seconds_remaining - std::max(0.0F, dt_seconds));
    if (missile.ignition_seconds_remaining <= 0.0F) {
      machine.process_event(ignition_elapsed{});
      missile.phase = read_phase(machine);
      return true;
    }
  }

  missile.phase = read_phase(machine);
  return false;
}

[[nodiscard]] ParticleCannonTuning raider_cannon_tuning(ParticleCannonTuning tuning) {
  tuning.fire_interval_seconds = tuning.raider_fire_interval_seconds;
  return tuning;
}

[[nodiscard]] ParticleCannonTuning drone_cannon_tuning(ParticleCannonTuning tuning) {
  tuning.fire_interval_seconds = tuning.drone_fire_interval_seconds;
  return tuning;
}

[[nodiscard]] Vec2 perpendicular(Vec2 direction) {
  return {.x = -direction.y, .y = direction.x};
}

[[nodiscard]] float cross(Vec2 lhs, Vec2 rhs) {
  return (lhs.x * rhs.y) - (lhs.y * rhs.x);
}

[[nodiscard]] Vec2 lead_target_direction(Vec2 origin, Vec2 source_velocity, const ShipMotion& target, const SectorTuning& sector, float projectile_speed) {
  const Vec2 to_target = wrapped_delta(origin, target.position, sector);
  const Vec2 relative_velocity = target.velocity - source_velocity;
  const float speed_squared = projectile_speed * projectile_speed;
  const float a = dot(relative_velocity, relative_velocity) - speed_squared;
  const float b = 2.0F * dot(to_target, relative_velocity);
  const float c = dot(to_target, to_target);
  float intercept_seconds = 0.0F;

  if (std::abs(a) <= 0.0001F) {
    if (std::abs(b) > 0.0001F) {
      intercept_seconds = std::max(0.0F, -c / b);
    }
  } else {
    const float discriminant = (b * b) - (4.0F * a * c);
    if (discriminant >= 0.0F) {
      const float root = std::sqrt(discriminant);
      const float first = (-b - root) / (2.0F * a);
      const float second = (-b + root) / (2.0F * a);
      if (first > 0.0F && second > 0.0F) {
        intercept_seconds = std::min(first, second);
      } else {
        intercept_seconds = std::max(first, second);
      }
      intercept_seconds = std::clamp(intercept_seconds, 0.0F, 1.4F);
    }
  }

  Vec2 direction = normalize_or_zero(to_target + (relative_velocity * intercept_seconds));
  if (length(direction) <= 0.0001F) {
    direction = normalize_or_zero(to_target);
  }
  return direction;
}

[[nodiscard]] Vec2 drone_clear_muzzle_origin(
  const MiningDrone& drone,
  const ShipMotion& player,
  Vec2 fire_direction,
  const SectorTuning& sector,
  const ParticleCannonTuning& tuning
) {
  const Vec2 right = perpendicular(fire_direction);
  const Vec2 drone_to_player = wrapped_delta(drone.position, player.position, sector);
  const float lateral = dot(drone_to_player, right);
  const float required_clearance = tuning.drone_player_clearance + tuning.muzzle_side_offset;
  if (std::abs(lateral) >= required_clearance) {
    return drone.position;
  }

  float side = lateral < 0.0F ? -1.0F : 1.0F;
  if (std::abs(lateral) <= 0.001F) {
    side = cross(fire_direction, drone_to_player) < 0.0F ? -1.0F : 1.0F;
    if (std::abs(cross(fire_direction, drone_to_player)) <= 0.001F) {
      side = std::sin(drone.work_angle_radians) < 0.0F ? -1.0F : 1.0F;
    }
  }
  return wrap_position(drone.position + (right * side * (required_clearance - std::abs(lateral))), sector);
}

void apply_projectile_damage(
  entt::registry& registry,
  entt::entity asteroid_entity,
  AsteroidBody& asteroid,
  MiningResource& resource,
  const ParticleShot& projectile,
  const SectorTuning& sector,
  const ParticleCannonTuning& tuning
) {
  resource.integrity = std::max(0.0F, resource.integrity - projectile.damage);
  if (tuning.impact_kind == AsteroidImpactKind::Kinetic) {
    const AsteroidMass* mass = registry.try_get<AsteroidMass>(asteroid_entity);
    const float impulse_mass = std::max(mass != nullptr ? mass->remaining_mass : asteroid.radius, 1.0F);
    asteroid.velocity += projectile.velocity * ((projectile.damage * tuning.asteroid_kinetic_impulse_scale) / impulse_mass);
    const Vec2 impact_offset = wrapped_delta(asteroid.position, projectile.position, sector);
    const float angular_lever = (impact_offset.x * projectile.velocity.y) - (impact_offset.y * projectile.velocity.x);
    const float moment = impulse_mass * std::max(asteroid.radius * asteroid.radius, 1.0F);
    asteroid.angular_velocity += (angular_lever * projectile.damage * tuning.asteroid_angular_impulse_scale) / moment;
  }
}

void fragment_depleted_asteroid(
  entt::registry& registry,
  DomainEventBus& event_bus,
  entt::entity asteroid,
  const ParticleShot& projectile,
  const ParticleCannonTuning& tuning
) {
  if (!registry.valid(asteroid) || !registry.all_of<MiningResource>(asteroid) || registry.get<MiningResource>(asteroid).integrity > 0.0F) {
    return;
  }

  (void)fragment_asteroid(
    registry,
    event_bus,
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
  const ParticleCannonFireCommand& command,
  const ParticleCannonTuning& tuning
) {
  const Vec2 normalized_direction = normalize_or_zero(command.direction);
  const Vec2 right{.x = -normalized_direction.y, .y = normalized_direction.x};
  const Vec2 velocity = (normalized_direction * tuning.projectile_speed) + command.source_velocity;

  for (const float side : {-1.0F, 1.0F}) {
    const entt::entity projectile = registry.create();
    registry.emplace<ParticleShot>(
      projectile,
      ParticleShot{
        .position = command.origin + (normalized_direction * tuning.muzzle_forward_offset) + (right * tuning.muzzle_side_offset * side),
        .velocity = velocity,
        .damage = tuning.damage,
        .radius = tuning.projectile_radius,
        .owner = command.owner,
      }
    );
    event_bus.enqueue(
      DomainEventType::ParticleFired,
      DomainEvent{
        .type = DomainEventType::ParticleFired,
        .subject = projectile,
        .position = command.origin,
        .amount = tuning.projectile_speed,
      }
    );
  }
}

void spawn_homing_missile_pair(
  entt::registry& registry,
  DomainEventBus& event_bus,
  const ShipMotion& ship,
  entt::entity target,
  const HomingMissileTuning& tuning
) {
  const Vec2 forward = facing_direction(ship.facing_radians);
  const Vec2 right = perpendicular(forward);
  for (const float side : {-1.0F, 1.0F}) {
    const entt::entity missile_entity = registry.create();
    const Vec2 origin = ship.position + (forward * tuning.eject_forward_offset) + (right * tuning.eject_side_offset * side);
    registry.emplace<HomingMissile>(
      missile_entity,
      HomingMissile{
        .position = origin,
        .velocity = ship.velocity + (forward * tuning.eject_forward_speed) + (right * tuning.eject_side_speed * side),
        .target = target,
        .ttl_seconds = tuning.ttl_seconds,
        .ignition_seconds_remaining = tuning.ignition_delay_seconds,
        .damage = tuning.damage,
        .radius = tuning.radius,
      }
    );
    event_bus.enqueue(
      DomainEventType::HomingMissileFired,
      DomainEvent{.type = DomainEventType::HomingMissileFired, .subject = missile_entity, .target = target, .position = origin}
    );
  }
}

void steer_homing_missile(HomingMissile& missile, const RaiderShip& target, const SectorTuning& sector, float dt_seconds, const HomingMissileTuning& tuning) {
  const Vec2 to_target = wrapped_delta(missile.position, target.position, sector);
  const Vec2 desired_direction = normalize_or_zero(to_target);
  if (length(desired_direction) <= 0.0001F) {
    return;
  }

  const Vec2 current_direction = normalize_or_zero(missile.velocity);
  const float blend = std::clamp(tuning.turn_responsiveness * std::max(0.0F, dt_seconds), 0.0F, 1.0F);
  const Vec2 steered_direction = normalize_or_zero((current_direction * (1.0F - blend)) + (desired_direction * blend));
  missile.velocity += steered_direction * tuning.motor_acceleration * std::max(0.0F, dt_seconds);
  missile.velocity = clamp_length(missile.velocity, tuning.max_speed);
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

HomingMissileLauncherModel& WeaponCtx::missile_launcher() const {
  return entity_.get<HomingMissileLauncherModel>();
}

std::optional<ParticleCannonFireCommand> request_player_particle_fire(
  WeaponCtx ctx,
  WeaponTrigger trigger,
  const ParticleCannonTuning& tuning
) {
  if (!advance_particle_cannon_fsm(ctx.cannon(), trigger.active, ctx.dt(), tuning)) {
    return std::nullopt;
  }

  const EntityCtx owner = ctx.entity_context();
  const ShipMotion& ship = owner.get<ShipMotion>();
  const Vec2 direction = length(trigger.aim) > 0.0F ? normalize_or_zero(trigger.aim) : facing_direction(ship.facing_radians);
  return ParticleCannonFireCommand{
    .origin = ship.position,
    .direction = direction,
    .source_velocity = ship.velocity,
    .owner = ProjectileOwner::Player,
  };
}

std::optional<ParticleCannonFireCommand> request_raider_particle_fire(
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
  if (!advance_particle_cannon_fsm(ctx.cannon(), trigger_active, ctx.dt(), raider_cannon_tuning(tuning)) || length(to_target) <= 0.0001F) {
    return std::nullopt;
  }

  return ParticleCannonFireCommand{
    .origin = raider.position,
    .direction = lead_target_direction(raider.position, raider.velocity, target_motion, ctx.sector(), tuning.projectile_speed),
    .source_velocity = raider.velocity,
    .owner = ProjectileOwner::Raider,
  };
}

std::optional<ParticleCannonFireCommand> request_drone_particle_fire(
  WeaponCtx ctx,
  EntityCtx player,
  WeaponTrigger trigger,
  const ParticleCannonTuning& tuning
) {
  const EntityCtx drone_entity = ctx.entity_context();
  const MiningDrone& drone = drone_entity.get<MiningDrone>();
  if (drone.phase != MiningDronePhase::Idle) {
    (void)advance_particle_cannon_fsm(ctx.cannon(), false, ctx.dt(), drone_cannon_tuning(tuning));
    return std::nullopt;
  }

  const ShipMotion& ship = player.get<ShipMotion>();
  const Vec2 fallback = facing_direction(ship.facing_radians);
  const Vec2 direction = length(trigger.aim) > 0.0F ? normalize_or_zero(trigger.aim) : fallback;
  if (!advance_particle_cannon_fsm(ctx.cannon(), trigger.active && length(direction) > 0.0001F, ctx.dt(), drone_cannon_tuning(tuning))) {
    return std::nullopt;
  }

  return ParticleCannonFireCommand{
    .origin = drone_clear_muzzle_origin(drone, ship, direction, ctx.sector(), tuning),
    .direction = direction,
    .source_velocity = drone.velocity,
    .owner = ProjectileOwner::Player,
  };
}

void spawn_requested_particle_fire(
  WeaponCtx ctx,
  const ParticleCannonFireCommand& command,
  const ParticleCannonTuning& tuning
) {
  spawn_particle_pair(ctx.registry(), ctx.event_bus(), command, tuning);
}

void update_player_homing_missile_launcher(
  WeaponCtx ctx,
  const EnemyTargetLockModel& enemy_lock,
  WeaponTrigger trigger,
  const HomingMissileTuning& tuning
) {
  if (!has_locked_enemy(enemy_lock) || !ctx.registry().valid(enemy_lock.target) || !ctx.registry().all_of<RaiderShip>(enemy_lock.target)) {
    (void)advance_homing_missile_launcher_fsm(ctx.missile_launcher(), false, ctx.dt(), tuning);
    return;
  }
  const RaiderShip& target = ctx.registry().get<RaiderShip>(enemy_lock.target);
  const bool trigger_active = trigger.active && target.integrity > 0.0F && target.phase != RaiderPhase::Escaped;
  if (!advance_homing_missile_launcher_fsm(ctx.missile_launcher(), trigger_active, ctx.dt(), tuning)) {
    return;
  }

  spawn_homing_missile_pair(ctx.registry(), ctx.event_bus(), ctx.entity_context().get<ShipMotion>(), enemy_lock.target, tuning);
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
  if (const std::optional<ParticleCannonFireCommand> command = request_player_particle_fire(ctx, trigger, tuning)) {
    spawn_requested_particle_fire(ctx, *command, tuning);
  }
}

void update_raider_particle_cannon(
  WeaponCtx ctx,
  EntityCtx target,
  WeaponTrigger trigger,
  const ParticleCannonTuning& tuning
) {
  if (const std::optional<ParticleCannonFireCommand> command = request_raider_particle_fire(ctx, target, trigger, tuning)) {
    spawn_requested_particle_fire(ctx, *command, tuning);
  }
}

void update_drone_particle_cannon(
  WeaponCtx ctx,
  EntityCtx player,
  WeaponTrigger trigger,
  const ParticleCannonTuning& tuning
) {
  if (const std::optional<ParticleCannonFireCommand> command = request_drone_particle_fire(ctx, player, trigger, tuning)) {
    spawn_requested_particle_fire(ctx, *command, tuning);
  }
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
              asteroid_solid_radius(asteroid.radius),
              relative_position
            )) {
          apply_projectile_damage(registry, asteroid_entity, asteroid, resource, projectile, ctx.sector(), tuning);
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
          ctx.event_bus().enqueue(
            DomainEventType::AsteroidDamaged,
            DomainEvent{
              .type = DomainEventType::AsteroidDamaged,
              .subject = asteroid_entity,
              .target = projectile_entity,
              .position = projectile.position,
              .amount = projectile.damage,
            }
          );
          fragment_depleted_asteroid(registry, ctx.event_bus(), asteroid_entity, projectile, tuning);
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

HomingMissileHudSnapshot update_homing_missiles(
  ProjectileSimCtx ctx,
  const HomingMissileTuning& tuning
) {
  entt::registry& registry = ctx.registry();
  HomingMissileHudSnapshot hud{};
  std::vector<entt::entity> expired;

  for (auto [missile_entity, missile] : registry.view<HomingMissile>().each()) {
    missile.ttl_seconds -= std::max(0.0F, ctx.dt());
    const bool ignited_now = advance_homing_missile_flight_fsm(missile, ctx.dt());
    if (ignited_now) {
      ctx.event_bus().enqueue(
        DomainEventType::HomingMissileIgnited,
        DomainEvent{.type = DomainEventType::HomingMissileIgnited, .subject = missile_entity, .target = missile.target, .position = missile.position}
      );
    }

    if (missile.phase == HomingMissilePhase::Ignited && registry.valid(missile.target) && registry.all_of<RaiderShip>(missile.target)) {
      RaiderShip& target = registry.get<RaiderShip>(missile.target);
      if (target.integrity > 0.0F && target.phase != RaiderPhase::Escaped) {
        steer_homing_missile(missile, target, ctx.sector(), ctx.dt(), tuning);
      }
    }

    missile.position = wrap_position(missile.position + (missile.velocity * std::max(0.0F, ctx.dt())), ctx.sector());
    bool hit = false;

    if (registry.valid(missile.target) && registry.all_of<RaiderShip>(missile.target)) {
      RaiderShip& target = registry.get<RaiderShip>(missile.target);
      if (target.integrity > 0.0F && target.phase != RaiderPhase::Escaped) {
        const Vec2 relative_position = wrapped_delta(missile.position, target.position, ctx.sector());
        if (jolt_shapes_overlap(
              SpriteCollisionShape::Particle,
              missile.radius,
              SpriteCollisionShape::Ship,
              RaiderCollisionRadius,
              relative_position
            )) {
          target.integrity = std::max(0.0F, target.integrity - missile.damage);
          ctx.event_bus().enqueue(
            DomainEventType::HomingMissileImpact,
            DomainEvent{
              .type = DomainEventType::HomingMissileImpact,
              .subject = missile_entity,
              .target = missile.target,
              .position = missile.position,
              .amount = missile.damage,
            }
          );
          const entt::entity explosion = registry.create();
          registry.emplace<ExplosionBurst>(
            explosion,
            ExplosionBurst{.position = missile.position, .ttl_seconds = tuning.explosion_ttl_seconds, .radius = tuning.explosion_radius}
          );
          (void)explosion;
          ++hud.impacts;
          hit = true;
        }
      }
    }

    if (hit || missile.ttl_seconds <= 0.0F) {
      expired.push_back(missile_entity);
    } else {
      ++hud.active_missiles;
    }
  }

  for (entt::entity missile : expired) {
    registry.destroy(missile);
  }

  return hud;
}

void update_explosion_bursts(entt::registry& registry, float dt_seconds) {
  std::vector<entt::entity> expired;
  for (auto [entity, burst] : registry.view<ExplosionBurst>().each()) {
    burst.age_seconds += std::max(0.0F, dt_seconds);
    if (burst.age_seconds >= burst.ttl_seconds) {
      expired.push_back(entity);
    }
  }

  for (entt::entity entity : expired) {
    registry.destroy(entity);
  }
}

}  // namespace hyperverse
