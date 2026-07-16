#include "hyperverse/app.hpp"

#include "hyperverse/camera.hpp"
#include "hyperverse/cargo_box.hpp"
#include "hyperverse/cargo_escort.hpp"
#include "hyperverse/cargo_extraction.hpp"
#include "hyperverse/cargo_manifest.hpp"
#include "hyperverse/cargo_route.hpp"
#include "hyperverse/cargo_train.hpp"
#include "hyperverse/collision.hpp"
#include "hyperverse/drone.hpp"
#include "hyperverse/fixed_timestep.hpp"
#include "hyperverse/flight.hpp"
#include "hyperverse/game_session.hpp"
#include "hyperverse/harpoon.hpp"
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
#include "hyperverse/vulkan_renderer.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <exception>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace hyperverse {

int App::run(AccountCtx& account) {
  try {
    SdlRuntime sdl;
    Window window;
    VulkanRenderer renderer{window.get()};
    account.log().info("starting flight laboratory");

    const VerticalSliceEntities entities = seed_vertical_slice(account);
    const entt::entity player = entities.player;
    ShipMotion& ship = account.registry().get<ShipMotion>(player);

    GamepadSlot gamepad;
    gamepad.open_first_available();
    FixedTimestep timestep{UniverseClock::FixedTickSeconds};
    const SectorTuning sector = sector_from_viewport(static_cast<float>(renderer.width()), static_cast<float>(renderer.height()));
    const FlightTuning flight{};
    const CameraTuning camera_tuning{};
    const MiningLaserTuning mining_laser{};
    const ContractQuotaTuning quota{};
    const ExtractionSite gathering_site{.position = {.x = 4300.0F, .y = 4300.0F}};
    const CargoBoxTuning cargo_box_tuning{.box_mass = quota.cargo_box_mass};
    const CargoEscortRoute escort_route = extraction_route_from_gathering(gathering_site.position, sector);
    const CargoExtractionTuning cargo_extraction_tuning{};
    account.registry().emplace_or_replace<ExtractionSite>(player, gathering_site);
    const SectorPressureTuning pressure_tuning{.escalation_interval_seconds = 60.0F};
    const MiningDroneTuning mining_drone_tuning{};
    const HarpoonTuning harpoon_tuning{};
    const ParticleCannonTuning particle_cannon_tuning{};
    const RaiderTuning raider_tuning{};
    const RadarHudTuning radar_tuning{
      .max_targets = 10,
      .update_interval_seconds = 0.25F,
      .reveal_seconds = 0.5F,
      .range_world = (static_cast<float>(std::min(renderer.width(), renderer.height())) * 0.75F) / 0.35F,
    };
    FlightInputMapper input_mapper;
    SemanticInputFrame latest_intent{};
    GameSessionModel game_session{};
    install_game_session_event_handlers(game_session, account.event_bus());
    account.event_bus().appendListener(DomainEventType::CargoArrivedAtGate, [&account, &ship, sector](const DomainEvent& event) {
      spawn_gate_combat_raiders(account.registry(), event.position, ship.position, sector, 3);
    });

    std::cout << application_name() << " " << version() << "\n";
    window.set_title(std::string{application_name()} + " " + std::string{version()});
    log_gamepad_state();

    bool running = true;
    auto previous_time = std::chrono::steady_clock::now();

    while (running) {
      const auto current_time = std::chrono::steady_clock::now();
      const float elapsed_seconds = std::chrono::duration<float>(current_time - previous_time).count();
      previous_time = current_time;
      timestep.accumulate(elapsed_seconds);

      SDL_Event event{};
      while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
          running = false;
        }

        if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
          gamepad.open(event.gdevice.which);
        }

        if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
          gamepad.close_if_removed(event.gdevice.which);
        }

        if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE) {
          running = false;
        }
      }

      while (timestep.consume_tick()) {
        SectorTickCtx tick_ctx{account, sector, timestep.tick_seconds()};
        latest_intent = input_mapper.map(gamepad.sample());
        simulate_assisted_flight(account, ship, latest_intent, flight, sector, timestep.tick_seconds());
        if (latest_intent.boost_requested && account.registry().get<CargoEscortState>(player).phase == CargoEscortPhase::EscortActive) {
          (void)detach_linked_cargo(account.registry(), ship.velocity);
        }
        update_asteroid_motion(account, sector, timestep.tick_seconds());

        CameraState& camera = account.registry().get<CameraState>(player);
        TargetLockModel& target_lock = account.registry().get<TargetLockModel>(player);
        HarpoonModel& harpoon = account.registry().get<HarpoonModel>(player);
        HarpoonHudSnapshot& harpoon_hud = account.registry().get<HarpoonHudSnapshot>(player);
        HudNotice& hud_notice = account.registry().get<HudNotice>(player);
        ShipHealth& ship_health = account.registry().get<ShipHealth>(player);
        RoundTimer& round_timer = account.registry().get<RoundTimer>(player);
        MiningHudSnapshot& mining_hud = account.registry().get<MiningHudSnapshot>(player);
        CargoManifest& cargo_manifest = account.registry().get<CargoManifest>(player);
        CargoHudSnapshot& cargo_hud = account.registry().get<CargoHudSnapshot>(player);
        CargoEscortState& cargo_escort = account.registry().get<CargoEscortState>(player);
        CargoEscortHudSnapshot& escort_hud = account.registry().get<CargoEscortHudSnapshot>(player);
        CargoTrainHudSnapshot& train_hud = account.registry().get<CargoTrainHudSnapshot>(player);
        CargoEscortRouteHudSnapshot& route_hud = account.registry().get<CargoEscortRouteHudSnapshot>(player);
        SectorPressureModel& pressure = account.registry().get<SectorPressureModel>(player);
        SectorPressureHudSnapshot& pressure_hud = account.registry().get<SectorPressureHudSnapshot>(player);
        MiningDroneHudSnapshot& drone_hud = account.registry().get<MiningDroneHudSnapshot>(player);
        RadarHudModel& radar_model = account.registry().get<RadarHudModel>(player);
        ParticleCannonHudSnapshot& particle_hud = account.registry().get<ParticleCannonHudSnapshot>(player);
        RaiderHudSnapshot& raider_hud = account.registry().get<RaiderHudSnapshot>(player);
        CargoRecoveryHudSnapshot& recovery_hud = account.registry().get<CargoRecoveryHudSnapshot>(player);
        CollisionHudSnapshot& collision_hud = account.registry().get<CollisionHudSnapshot>(player);

        update_camera_anchor(camera, ship, sector, camera_tuning, timestep.tick_seconds());
        update_radar_hud(radar_model, account.registry(), ship, sector, timestep.tick_seconds(), radar_tuning);
        std::vector<entt::entity> tracked_targets;
        tracked_targets.reserve(radar_model.tracked_targets.size());
        for (const RadarTrackedTarget& tracked : radar_model.tracked_targets) {
          tracked_targets.push_back(tracked.target);
        }
        update_ship_status(ship_health, round_timer, timestep.tick_seconds());
        update_hud_notice(hud_notice, timestep.tick_seconds());
        update_target_lock(target_lock, account.registry(), ship.position, ship.velocity, latest_intent, sector, {}, tracked_targets);
        mining_hud =
          update_mining_laser(account.registry(), target_lock, ship, latest_intent, sector, mining_laser, timestep.tick_seconds());
        harpoon_hud =
          update_harpoon(harpoon, account.registry(), target_lock, ship, latest_intent, sector, timestep.tick_seconds(), harpoon_tuning);

        drone_hud = {};
        for (entt::entity drone_entity : entities.mining_drones) {
          const MiningDroneHudSnapshot current_drone = update_mining_drone(
            account.registry().get<MiningDrone>(drone_entity),
            account.registry(),
            target_lock,
            ship,
            sector,
            timestep.tick_seconds(),
            mining_drone_tuning,
            &account.event_bus()
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

        cargo_hud = update_cargo_manifest(cargo_manifest, account.registry(), quota);
        escort_hud = update_cargo_escort_state(cargo_escort, cargo_hud, latest_intent, &account.event_bus());
        if (escort_hud.cargo_train_active && hud_notice.message.empty()) {
          push_hud_notice(hud_notice, "GET TO THE TRANSPORT GATE NOW");
        }
        if (escort_hud.phase == CargoEscortPhase::Mining || escort_hud.phase == CargoEscortPhase::Authorized) {
          (void)sync_cargo_boxes(account.registry(), cargo_manifest, gathering_site, cargo_box_tuning);
        }
        route_hud = update_cargo_escort_route(cargo_escort, escort_route, ship, sector);
        escort_hud = update_cargo_escort_arrival(cargo_escort, cargo_hud, route_hud, &account.event_bus());
        train_hud = update_cargo_train(account.registry(), cargo_escort, ship, sector, timestep.tick_seconds());
        const CargoExtractionHudSnapshot extraction_hud = update_cargo_extraction(
          account.registry(),
          cargo_escort,
          escort_route,
          sector,
          timestep.tick_seconds(),
          &account.event_bus(),
          cargo_extraction_tuning
        );
        (void)extraction_hud;
        raider_hud = {};
        std::vector<std::pair<entt::entity, RaiderHudSnapshot>> active_raiders;
        for (auto [raider_entity, raider] : account.registry().view<RaiderShip>().each()) {
          RaiderHudSnapshot current_raider = update_raider_threat(
            raider,
            account.registry(),
            cargo_escort,
            ship,
            sector,
            timestep.tick_seconds(),
            raider_tuning
          );
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
        recovery_hud = recover_stolen_cargo(
          account.registry(),
          account.registry().get<RaiderShip>(entities.raider),
          ship,
          latest_intent,
          sector
        );
        if (recovery_hud.recovered) {
          raider_hud = {};
        }
        WeaponCtx player_weapon{tick_ctx.entity_context(player)};
        if (const std::optional<ParticleCannonFireCommand> player_fire = request_player_particle_fire(
              player_weapon,
              WeaponTrigger{.aim = latest_intent.primary_aim, .active = latest_intent.particle_fire_active},
              particle_cannon_tuning
            )) {
          spawn_requested_particle_fire(player_weapon, *player_fire, particle_cannon_tuning);
        }
        for (const auto& [raider_entity, current_raider] : active_raiders) {
          WeaponCtx raider_weapon{tick_ctx.entity_context(raider_entity)};
          if (const std::optional<ParticleCannonFireCommand> raider_fire = request_raider_particle_fire(
                raider_weapon,
                tick_ctx.entity_context(player),
                WeaponTrigger{.active = current_raider.active},
                particle_cannon_tuning
              )) {
            spawn_requested_particle_fire(raider_weapon, *raider_fire, particle_cannon_tuning);
          }
        }
        particle_hud = update_particle_projectiles(ProjectileSimCtx{tick_ctx, player}, particle_cannon_tuning);
        account.event_bus().process();
        pressure_hud = update_sector_pressure(pressure, timestep.tick_seconds(), pressure_tuning);
        collision_hud = predict_ship_asteroid_collision(ship, account.registry(), sector);
      }

      const FlightHudSnapshot hud = make_flight_hud_snapshot(ship, latest_intent, flight, sector);

      renderer.draw_frame(build_sprite_frame(
        account,
        player,
        entities.mining_drones,
        entities.raider,
        hud,
        latest_intent,
        sector,
        renderer.width(),
        renderer.height()
      ));
      SDL_Delay(1);
    }

    renderer.wait_idle();
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "Fatal error: " << error.what() << "\n";
    return 1;
  }
}

}  // namespace hyperverse
