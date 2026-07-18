#include "hyperverse/app.hpp"

#include "hyperverse/asteroid_geometry.hpp"
#include "hyperverse/camera.hpp"
#include "hyperverse/cargo_box.hpp"
#include "hyperverse/cargo_dispatch.hpp"
#include "hyperverse/cargo_escort.hpp"
#include "hyperverse/cargo_extraction.hpp"
#include "hyperverse/cargo_manifest.hpp"
#include "hyperverse/cargo_route.hpp"
#include "hyperverse/cargo_train.hpp"
#include "hyperverse/collision.hpp"
#include "hyperverse/dawn_renderer.hpp"
#include "hyperverse/drone.hpp"
#include "hyperverse/engine_trail.hpp"
#include "hyperverse/fixed_timestep.hpp"
#include "hyperverse/flight.hpp"
#include "hyperverse/game_session.hpp"
#include "hyperverse/gravity_sling.hpp"
#include "hyperverse/hud_notice.hpp"
#include "hyperverse/input.hpp"
#include "hyperverse/mining.hpp"
#include "hyperverse/pressure.hpp"
#include "hyperverse/projectile.hpp"
#include "hyperverse/raider.hpp"
#include "hyperverse/radar_hud.hpp"
#include "hyperverse/sdl_platform.hpp"
#include "hyperverse/ship_status.hpp"
#include "hyperverse/sprite_frame_builder.hpp"
#include "hyperverse/targeting.hpp"
#include "hyperverse/universe_clock.hpp"
#include "hyperverse/version.hpp"
#include "hyperverse/vertical_slice_seed.hpp"

#include <SDL3/SDL.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace hyperverse {
namespace {

constexpr float TauRadians = 6.28318530718F;

[[nodiscard]] Vec2 direction_from_angle(float radians) {
  return {.x = std::cos(radians), .y = std::sin(radians)};
}

[[nodiscard]] Vec2 rotate(Vec2 value, float radians) {
  const float cosine = std::cos(radians);
  const float sine = std::sin(radians);
  return {.x = (value.x * cosine) - (value.y * sine), .y = (value.x * sine) + (value.y * cosine)};
}

[[nodiscard]] float movement_intensity(Vec2 velocity, float full_speed) {
  return std::clamp(length(velocity) / std::max(full_speed, 1.0F), 0.0F, 1.0F);
}

void update_drone_engine_trail(
  EngineTrailModel& trail,
  const MiningDrone& drone,
  const SectorTuning& sector,
  float dt_seconds,
  const MiningDroneTuning& tuning,
  const EngineTrailTuning& trail_tuning
) {
  const float intensity = movement_intensity(drone.velocity, tuning.max_speed);
  const Vec2 exhaust_direction = direction_from_angle(drone.facing_radians + (TauRadians * 0.5F));
  std::array<EngineTrailNozzle, 2> nozzles{};
  for (std::size_t index = 0; index < nozzles.size(); ++index) {
    const float side = index == 0U ? -1.0F : 1.0F;
    nozzles[index] = EngineTrailNozzle{
      .world_position = wrap_position(drone.position + rotate({.x = -12.0F, .y = side * 5.5F}, drone.facing_radians), sector),
      .exhaust_direction = exhaust_direction,
      .intensity = intensity,
    };
  }
  (void)update_engine_trail_from_nozzles(trail, nozzles, sector, dt_seconds, trail_tuning);
}

void update_raider_engine_trail(
  EngineTrailModel& trail,
  const RaiderShip& raider,
  const SectorTuning& sector,
  float dt_seconds,
  const RaiderTuning& tuning,
  const EngineTrailTuning& trail_tuning
) {
  if (raider.integrity <= 0.0F) {
    reset_engine_trail(trail);
    return;
  }

  const float intensity = std::clamp(0.22F + (movement_intensity(raider.velocity, tuning.max_speed) * 0.78F), 0.0F, 1.0F);
  const Vec2 exhaust_direction = direction_from_angle(raider.facing_radians + (TauRadians * 0.5F));
  std::array<EngineTrailNozzle, 2> nozzles{};
  for (std::size_t index = 0; index < nozzles.size(); ++index) {
    const float side = index == 0U ? -1.0F : 1.0F;
    nozzles[index] = EngineTrailNozzle{
      .world_position = wrap_position(raider.position + rotate({.x = -18.0F, .y = side * 34.0F}, raider.facing_radians), sector),
      .exhaust_direction = exhaust_direction,
      .intensity = intensity,
    };
  }
  (void)update_engine_trail_from_nozzles(trail, nozzles, sector, dt_seconds, trail_tuning);
}

class AppRuntime {
public:
  explicit AppRuntime(AccountCtx& source_account)
    : account_{source_account},
      renderer_{window_.get()},
      entities_{seed_vertical_slice(account_)},
      player_{entities_.player},
      ship_{account_.registry().get<ShipMotion>(player_)},
      timestep_{UniverseClock::FixedTickSeconds},
      sector_{default_sector()},
      gathering_site_{.position = {.x = 4300.0F, .y = 4300.0F}},
      cargo_box_tuning_{.box_mass = quota_.cargo_box_mass},
      escort_route_{extraction_route_from_gathering(gathering_site_.position, sector_)},
      radar_tuning_{
        .max_targets = 10,
        .update_interval_seconds = 0.25F,
        .reveal_seconds = 0.5F,
        .range_world = (static_cast<float>(std::min(renderer_.width(), renderer_.height())) * 0.75F) / 0.35F,
      },
      previous_time_{std::chrono::steady_clock::now()} {
    account_.log().info("starting flight laboratory");
    gamepad_.open_first_available();
    account_.registry().emplace_or_replace<ExtractionSite>(player_, gathering_site_);
    install_game_session_event_handlers(game_session_, account_.event_bus());
    install_cargo_dispatch_event_handlers(
      cargo_dispatch_,
      account_.registry(),
      entities_.mining_drones,
      gathering_site_,
      cargo_box_tuning_,
      sector_,
      account_.event_bus()
    );
    account_.event_bus().appendListener(DomainEventType::CargoArrivedAtGate, [this](const DomainEvent& event) {
      spawn_gate_combat_raiders(account_.registry(), event.position, ship_.position, sector_, 3);
      push_hud_notice(account_.registry().get<HudNotice>(player_), "Cargo accepted - thank you");
    });

    std::cout << application_name() << " " << version() << "\n";
    window_.set_title(std::string{application_name()} + " " + std::string{version()});
    log_gamepad_state();
  }

  [[nodiscard]] bool frame_from_clock() {
    const auto current_time = std::chrono::steady_clock::now();
    const float elapsed_seconds = std::chrono::duration<float>(current_time - previous_time_).count();
    previous_time_ = current_time;
    return run_frame(elapsed_seconds);
  }

#if defined(__EMSCRIPTEN__)
  [[nodiscard]] bool frame_from_animation_timestamp(const double timestamp_milliseconds) {
    if (!previous_animation_timestamp_.has_value()) {
      previous_animation_timestamp_ = timestamp_milliseconds;
      return run_frame(0.0F);
    }

    const float elapsed_seconds = static_cast<float>((timestamp_milliseconds - *previous_animation_timestamp_) / 1000.0);
    previous_animation_timestamp_ = timestamp_milliseconds;
    return run_frame(std::max(elapsed_seconds, 0.0F));
  }
#endif

  void wait_idle() const {
    renderer_.wait_idle();
  }

private:
  [[nodiscard]] bool run_frame(const float elapsed_seconds) {
    timestep_.accumulate(elapsed_seconds);
    pump_events();

    while (timestep_.consume_tick()) {
      tick();
    }

    renderer_.refresh_extent();
    const FlightHudSnapshot hud = make_flight_hud_snapshot(ship_, latest_intent_, flight_, sector_);
    renderer_.draw_frame(build_sprite_frame(
      account_,
      player_,
      entities_.mining_drones,
      entities_.raider,
      hud,
      latest_intent_,
      sector_,
      renderer_.width(),
      renderer_.height()
    ));

    return running_;
  }

  void pump_events() {
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) {
        running_ = false;
      }

      if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
        gamepad_.open(event.gdevice.which);
      }

      if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
        gamepad_.close_if_removed(event.gdevice.which);
      }

      if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE) {
        running_ = false;
      }
    }
  }

  void tick() {
    SectorTickCtx tick_ctx{account_, sector_, timestep_.tick_seconds()};
    gamepad_.open_first_available();
    latest_intent_ = input_mapper_.map(gamepad_.sample());
    if (latest_intent_.boost_requested && account_.registry().get<CargoEscortState>(player_).phase == CargoEscortPhase::EscortActive) {
      (void)detach_linked_cargo(account_.registry(), ship_.velocity);
    }
    update_asteroid_motion(account_, sector_, timestep_.tick_seconds());
    for (auto [entity, geometry] : account_.registry().view<AsteroidGeometry>().each()) {
      (void)entity;
      update_asteroid_tumble(geometry, timestep_.tick_seconds());
    }

    CameraState& camera = account_.registry().get<CameraState>(player_);
    TargetLockModel& target_lock = account_.registry().get<TargetLockModel>(player_);
    GravitySlingModel& gravity_sling = account_.registry().get<GravitySlingModel>(player_);
    GravitySlingHudSnapshot& gravity_sling_hud = account_.registry().get<GravitySlingHudSnapshot>(player_);
    HudNotice& hud_notice = account_.registry().get<HudNotice>(player_);
    ShipHealth& ship_health = account_.registry().get<ShipHealth>(player_);
    EngineTrailModel& engine_trail = account_.registry().get<EngineTrailModel>(player_);
    RoundTimer& round_timer = account_.registry().get<RoundTimer>(player_);
    MiningHudSnapshot& mining_hud = account_.registry().get<MiningHudSnapshot>(player_);
    CargoManifest& cargo_manifest = account_.registry().get<CargoManifest>(player_);
    CargoHudSnapshot& cargo_hud = account_.registry().get<CargoHudSnapshot>(player_);
    CargoEscortState& cargo_escort = account_.registry().get<CargoEscortState>(player_);
    CargoEscortHudSnapshot& escort_hud = account_.registry().get<CargoEscortHudSnapshot>(player_);
    CargoTrainHudSnapshot& train_hud = account_.registry().get<CargoTrainHudSnapshot>(player_);
    CargoEscortRouteHudSnapshot& route_hud = account_.registry().get<CargoEscortRouteHudSnapshot>(player_);
    SectorPressureModel& pressure = account_.registry().get<SectorPressureModel>(player_);
    SectorPressureHudSnapshot& pressure_hud = account_.registry().get<SectorPressureHudSnapshot>(player_);
    MiningDroneHudSnapshot& drone_hud = account_.registry().get<MiningDroneHudSnapshot>(player_);
    RadarHudModel& radar_model = account_.registry().get<RadarHudModel>(player_);
    ParticleCannonHudSnapshot& particle_hud = account_.registry().get<ParticleCannonHudSnapshot>(player_);
    RaiderHudSnapshot& raider_hud = account_.registry().get<RaiderHudSnapshot>(player_);
    CargoRecoveryHudSnapshot& recovery_hud = account_.registry().get<CargoRecoveryHudSnapshot>(player_);
    CollisionHudSnapshot& collision_hud = account_.registry().get<CollisionHudSnapshot>(player_);

    SemanticInputFrame sling_input = latest_intent_;
    if (sling_input.gravity_sling_requested && length(sling_input.primary_aim) <= 0.0001F && has_locked_target(target_lock)) {
      sling_input.primary_aim = normalize_or_zero(target_lock.relative_position);
    }
    gravity_sling_hud = update_gravity_sling(gravity_sling, account_.registry(), ship_, sling_input, sector_, timestep_.tick_seconds(), gravity_sling_tuning_);
    if (gravity_sling.phase == GravitySlingPhase::FreeFlight) {
      simulate_assisted_flight(account_, ship_, latest_intent_, flight_, sector_, timestep_.tick_seconds());
    }
    if (ship_health.armor <= 0.0F) {
      reset_engine_trail(engine_trail);
    } else {
      (void)update_engine_trail(engine_trail, ship_, latest_intent_, sector_, timestep_.tick_seconds(), engine_trail_tuning_);
    }
    update_camera_anchor(camera, ship_, sector_, camera_tuning_, timestep_.tick_seconds());
    update_radar_hud(radar_model, account_.registry(), ship_, sector_, timestep_.tick_seconds(), radar_tuning_);
    std::vector<entt::entity> tracked_targets;
    tracked_targets.reserve(radar_model.tracked_targets.size());
    for (const RadarTrackedTarget& tracked : radar_model.tracked_targets) {
      tracked_targets.push_back(tracked.target);
    }
    update_ship_status(ship_health, round_timer, timestep_.tick_seconds());
    update_hud_notice(hud_notice, timestep_.tick_seconds());
    update_target_lock(target_lock, account_.registry(), ship_.position, ship_.velocity, latest_intent_, sector_, {}, tracked_targets);
    mining_hud = update_mining_laser(account_.registry(), target_lock, ship_, latest_intent_, sector_, mining_laser_, timestep_.tick_seconds());

    drone_hud = {};
    (void)dispatch_cargo_drone_jobs(cargo_dispatch_, account_.registry(), entities_.mining_drones, &account_.event_bus());
    for (entt::entity drone_entity : entities_.mining_drones) {
      MiningDrone& drone = account_.registry().get<MiningDrone>(drone_entity);
      const MiningDroneHudSnapshot current_drone = update_mining_drone(
        drone,
        account_.registry(),
        target_lock,
        ship_,
        sector_,
        timestep_.tick_seconds(),
        mining_drone_tuning_,
        &account_.event_bus()
      );
      update_drone_engine_trail(
        account_.registry().get<EngineTrailModel>(drone_entity),
        drone,
        sector_,
        timestep_.tick_seconds(),
        mining_drone_tuning_,
        engine_trail_tuning_
      );
      drone_hud.extracted_mass += current_drone.extracted_mass;
      if (current_drone.phase == MiningDronePhase::Mining) {
        drone_hud.phase = MiningDronePhase::Mining;
        drone_hud.target = current_drone.target;
        drone_hud.target_distance = current_drone.target_distance;
      } else if (drone_hud.phase == MiningDronePhase::Idle && current_drone.phase == MiningDronePhase::Travelling) {
        drone_hud.phase = MiningDronePhase::Travelling;
        drone_hud.target = current_drone.target;
        drone_hud.target_distance = current_drone.target_distance;
      }
    }

    cargo_hud = update_cargo_manifest(cargo_manifest, account_.registry(), quota_);
    escort_hud = update_cargo_escort_state(cargo_escort, cargo_hud, latest_intent_, &account_.event_bus());
    if (escort_hud.cargo_train_active && hud_notice.message.empty()) {
      push_hud_notice(hud_notice, "GET TO THE TRANSPORT GATE NOW");
    }
    if (escort_hud.phase == CargoEscortPhase::Mining || escort_hud.phase == CargoEscortPhase::Authorized) {
      Vec2 cargo_pickup_origin{};
      if (mining_hud.beam_active && mining_hud.target != entt::null && account_.registry().valid(mining_hud.target) &&
          account_.registry().all_of<AsteroidBody>(mining_hud.target)) {
        cargo_pickup_origin = account_.registry().get<AsteroidBody>(mining_hud.target).position;
      } else if (drone_hud.target != entt::null && account_.registry().valid(drone_hud.target) &&
                 account_.registry().all_of<AsteroidBody>(drone_hud.target)) {
        cargo_pickup_origin = account_.registry().get<AsteroidBody>(drone_hud.target).position;
      }
      (void)sync_cargo_boxes(account_.registry(), cargo_manifest, gathering_site_, cargo_box_tuning_, cargo_pickup_origin, &account_.event_bus());
      (void)update_gathered_cargo_boxes(account_.registry(), gathering_site_, sector_, timestep_.tick_seconds(), cargo_box_tuning_);
    }
    route_hud = update_cargo_escort_route(cargo_escort, escort_route_, ship_, sector_);
    escort_hud = update_cargo_escort_arrival(cargo_escort, cargo_hud, route_hud, &account_.event_bus());
    train_hud = update_cargo_train(account_.registry(), cargo_escort, ship_, sector_, timestep_.tick_seconds());
    const CargoExtractionHudSnapshot extraction_hud = update_cargo_extraction(
      account_.registry(),
      cargo_escort,
      escort_route_,
      sector_,
      timestep_.tick_seconds(),
      &account_.event_bus(),
      cargo_extraction_tuning_
    );
    (void)extraction_hud;
    raider_hud = {};
    std::vector<std::pair<entt::entity, RaiderHudSnapshot>> active_raiders;
    for (auto [raider_entity, raider] : account_.registry().view<RaiderShip>().each()) {
      RaiderHudSnapshot current_raider =
        update_raider_threat(raider, account_.registry(), cargo_escort, ship_, sector_, timestep_.tick_seconds(), raider_tuning_);
      if (EngineTrailModel* trail = account_.registry().try_get<EngineTrailModel>(raider_entity); trail != nullptr) {
        if (current_raider.active) {
          update_raider_engine_trail(*trail, raider, sector_, timestep_.tick_seconds(), raider_tuning_, engine_trail_tuning_);
        } else {
          reset_engine_trail(*trail);
        }
      }
      if (current_raider.active) {
        active_raiders.emplace_back(raider_entity, current_raider);
        if (!raider_hud.active || raider.role == RaiderRole::CargoThief) {
          raider_hud = current_raider;
        }
      }
    }
    if (raider_hud.active && hud_notice.message != "CONVOY UNDER ATTACK") {
      push_hud_notice(hud_notice, "CONVOY UNDER ATTACK");
    }
    recovery_hud =
      recover_stolen_cargo(account_.registry(), account_.registry().get<RaiderShip>(entities_.raider), ship_, latest_intent_, sector_);
    if (recovery_hud.recovered) {
      raider_hud = {};
    }
    WeaponCtx player_weapon{tick_ctx.entity_context(player_)};
    if (const std::optional<ParticleCannonFireCommand> player_fire = request_player_particle_fire(
          player_weapon,
          WeaponTrigger{.aim = latest_intent_.primary_aim, .active = latest_intent_.particle_fire_active},
          particle_cannon_tuning_
        )) {
      spawn_requested_particle_fire(player_weapon, *player_fire, particle_cannon_tuning_);
    }
    for (entt::entity drone_entity : entities_.mining_drones) {
      WeaponCtx drone_weapon{tick_ctx.entity_context(drone_entity)};
      if (const std::optional<ParticleCannonFireCommand> drone_fire = request_drone_particle_fire(
            drone_weapon,
            tick_ctx.entity_context(player_),
            WeaponTrigger{.aim = latest_intent_.primary_aim, .active = latest_intent_.particle_fire_active},
            particle_cannon_tuning_
          )) {
        spawn_requested_particle_fire(drone_weapon, *drone_fire, particle_cannon_tuning_);
      }
    }
    for (const auto& [raider_entity, current_raider] : active_raiders) {
      WeaponCtx raider_weapon{tick_ctx.entity_context(raider_entity)};
      if (const std::optional<ParticleCannonFireCommand> raider_fire = request_raider_particle_fire(
            raider_weapon,
            tick_ctx.entity_context(player_),
            WeaponTrigger{.active = current_raider.active},
            particle_cannon_tuning_
          )) {
        spawn_requested_particle_fire(raider_weapon, *raider_fire, particle_cannon_tuning_);
      }
    }
    particle_hud = update_particle_projectiles(ProjectileSimCtx{tick_ctx, player_}, particle_cannon_tuning_);
    account_.event_bus().process();
    const int previous_pressure_level = pressure.escalation_level;
    pressure_hud = update_sector_pressure(pressure, timestep_.tick_seconds(), pressure_tuning_);
    if (pressure.escalation_level > previous_pressure_level) {
      const int spawned_raiders = spawn_pressure_raiders(account_.registry(), ship_.position, sector_, pressure.escalation_level);
      push_hud_notice(hud_notice, spawned_raiders > 0 ? "RAIDER CONTACTS INBOUND" : "THREAT LEVEL INCREASED");
    }
    if (pressure_hud.universe_tear_open) {
      push_hud_notice(hud_notice, "SPACE TEAR CONSUMING YOU");
      ship_health.shields = 0.0F;
      ship_health.armor = 0.0F;
    }
    collision_hud = predict_ship_asteroid_collision(ship_, account_.registry(), sector_);
  }

  AccountCtx account_;
  SdlRuntime sdl_;
  Window window_;
  DawnRenderer renderer_;
  VerticalSliceEntities entities_;
  entt::entity player_;
  ShipMotion& ship_;
  GamepadSlot gamepad_;
  FixedTimestep timestep_;
  SectorTuning sector_;
  FlightTuning flight_{};
  CameraTuning camera_tuning_{};
  MiningLaserTuning mining_laser_{};
  ContractQuotaTuning quota_{};
  ExtractionSite gathering_site_;
  CargoBoxTuning cargo_box_tuning_;
  CargoDispatchModel cargo_dispatch_{};
  CargoEscortRoute escort_route_;
  CargoExtractionTuning cargo_extraction_tuning_{};
  SectorPressureTuning pressure_tuning_{.escalation_interval_seconds = 60.0F};
  MiningDroneTuning mining_drone_tuning_{};
  EngineTrailTuning engine_trail_tuning_{};
  GravitySlingTuning gravity_sling_tuning_{};
  ParticleCannonTuning particle_cannon_tuning_{};
  RaiderTuning raider_tuning_{};
  RadarHudTuning radar_tuning_;
  FlightInputMapper input_mapper_;
  SemanticInputFrame latest_intent_{};
  GameSessionModel game_session_{};
  bool running_{true};
  std::chrono::steady_clock::time_point previous_time_;
#if defined(__EMSCRIPTEN__)
  std::optional<double> previous_animation_timestamp_;
#endif
};

#if defined(__EMSCRIPTEN__)
EM_BOOL run_animation_frame(const double timestamp_milliseconds, void* user_data) {
  auto* runtime = static_cast<AppRuntime*>(user_data);
  try {
    if (!runtime->frame_from_animation_timestamp(timestamp_milliseconds)) {
      runtime->wait_idle();
      delete runtime;
      return EM_FALSE;
    }
  } catch (const std::exception& error) {
    std::cerr << "Fatal error: " << error.what() << "\n";
    delete runtime;
    return EM_FALSE;
  }

  return EM_TRUE;
}
#endif

}  // namespace

int App::run(AccountCtx& account) {
  try {
#if defined(__EMSCRIPTEN__)
    auto* runtime = new AppRuntime{account};
    emscripten_request_animation_frame_loop(run_animation_frame, runtime);
    emscripten_exit_with_live_runtime();
    return 0;
#else
    AppRuntime runtime{account};
    while (runtime.frame_from_clock()) {
      SDL_Delay(1);
    }

    runtime.wait_idle();
    return 0;
#endif
  } catch (const std::exception& error) {
    std::cerr << "Fatal error: " << error.what() << "\n";
    return 1;
  }
}

}  // namespace hyperverse
