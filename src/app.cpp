#include "hyperverse/app.hpp"

#include "hyperverse/camera.hpp"
#include "hyperverse/cargo.hpp"
#include "hyperverse/collision.hpp"
#include "hyperverse/drone.hpp"
#include "hyperverse/fixed_timestep.hpp"
#include "hyperverse/flight.hpp"
#include "hyperverse/grand_central.hpp"
#include "hyperverse/input.hpp"
#include "hyperverse/mining.hpp"
#include "hyperverse/pressure.hpp"
#include "hyperverse/targeting.hpp"
#include "hyperverse/version.hpp"
#include "hyperverse/vulkan_renderer.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

class SdlRuntime {
public:
  SdlRuntime() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS)) {
      throw std::runtime_error(std::string{"SDL_Init failed: "} + SDL_GetError());
    }
  }

  ~SdlRuntime() {
    SDL_Quit();
  }

  SdlRuntime(const SdlRuntime&) = delete;
  SdlRuntime& operator=(const SdlRuntime&) = delete;
  SdlRuntime(SdlRuntime&&) = delete;
  SdlRuntime& operator=(SdlRuntime&&) = delete;
};

class Window {
public:
  Window() {
    window_ = SDL_CreateWindow(
      hyperverse::application_name().data(),
      1280,
      720,
      SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );

    if (window_ == nullptr) {
      throw std::runtime_error(std::string{"SDL_CreateWindow failed: "} + SDL_GetError());
    }
  }

  ~Window() {
    if (window_ != nullptr) {
      SDL_DestroyWindow(window_);
    }
  }

  [[nodiscard]] SDL_Window& get() const {
    return *window_;
  }

  void set_title(const std::string& title) {
    SDL_SetWindowTitle(window_, title.c_str());
  }

  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;
  Window(Window&&) = delete;
  Window& operator=(Window&&) = delete;

private:
  SDL_Window* window_{nullptr};
};

class GamepadSlot {
public:
  GamepadSlot() = default;

  ~GamepadSlot() {
    close();
  }

  GamepadSlot(const GamepadSlot&) = delete;
  GamepadSlot& operator=(const GamepadSlot&) = delete;
  GamepadSlot(GamepadSlot&&) = delete;
  GamepadSlot& operator=(GamepadSlot&&) = delete;

  void open_first_available() {
    int gamepad_count = 0;
    SDL_JoystickID* gamepads = SDL_GetGamepads(&gamepad_count);
    if (gamepads == nullptr) {
      return;
    }

    for (int index = 0; index < gamepad_count && gamepad_ == nullptr; ++index) {
      open(gamepads[index]);
    }

    SDL_free(gamepads);
  }

  void open(SDL_JoystickID joystick_id) {
    if (gamepad_ != nullptr) {
      return;
    }

    gamepad_ = SDL_OpenGamepad(joystick_id);
    if (gamepad_ != nullptr) {
      joystick_id_ = SDL_GetGamepadID(gamepad_);
      const char* name = SDL_GetGamepadName(gamepad_);
      std::cout << "Gamepad active: " << (name != nullptr ? name : "Unknown gamepad") << "\n";
    }
  }

  void close_if_removed(SDL_JoystickID joystick_id) {
    if (gamepad_ != nullptr && joystick_id == joystick_id_) {
      close();
    }
  }

  [[nodiscard]] hyperverse::RawInputFrame sample() const {
    hyperverse::RawInputFrame raw{};

    int key_count = 0;
    const bool* keyboard = SDL_GetKeyboardState(&key_count);
    const auto key_down = [keyboard, key_count](SDL_Scancode scancode) {
      return keyboard != nullptr && static_cast<int>(scancode) < key_count && keyboard[scancode];
    };

    if (keyboard != nullptr) {
      raw.movement_axis.x += key_down(SDL_SCANCODE_D) ? 1.0F : 0.0F;
      raw.movement_axis.x -= key_down(SDL_SCANCODE_A) ? 1.0F : 0.0F;
      raw.movement_axis.y += key_down(SDL_SCANCODE_S) ? 1.0F : 0.0F;
      raw.movement_axis.y -= key_down(SDL_SCANCODE_W) ? 1.0F : 0.0F;
      raw.aim_axis.x += key_down(SDL_SCANCODE_RIGHT) ? 1.0F : 0.0F;
      raw.aim_axis.x -= key_down(SDL_SCANCODE_LEFT) ? 1.0F : 0.0F;
      raw.aim_axis.y += key_down(SDL_SCANCODE_DOWN) ? 1.0F : 0.0F;
      raw.aim_axis.y -= key_down(SDL_SCANCODE_UP) ? 1.0F : 0.0F;
      raw.confirm = key_down(SDL_SCANCODE_SPACE);
      raw.cancel = key_down(SDL_SCANCODE_ESCAPE);
      raw.target_cycle = key_down(SDL_SCANCODE_TAB);
      raw.tool_intensity = key_down(SDL_SCANCODE_F) ? 1.0F : 0.0F;
    }

    if (gamepad_ != nullptr) {
      raw.control_mapping = hyperverse::ControlMapping::Gamepad;
      raw.movement_axis = {
        .x = axis(SDL_GAMEPAD_AXIS_LEFTX),
        .y = axis(SDL_GAMEPAD_AXIS_LEFTY),
      };
      raw.aim_axis = {
        .x = axis(SDL_GAMEPAD_AXIS_RIGHTX),
        .y = axis(SDL_GAMEPAD_AXIS_RIGHTY),
      };
      raw.confirm = raw.confirm || SDL_GetGamepadButton(gamepad_, SDL_GAMEPAD_BUTTON_SOUTH);
      raw.cancel = raw.cancel || SDL_GetGamepadButton(gamepad_, SDL_GAMEPAD_BUTTON_EAST);
      raw.target_cycle = raw.target_cycle || SDL_GetGamepadButton(gamepad_, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
      raw.tool_intensity = std::max(raw.tool_intensity, trigger(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));
    }

    return raw;
  }

private:
  [[nodiscard]] float axis(SDL_GamepadAxis axis_id) const {
    constexpr float axis_max = 32767.0F;
    return static_cast<float>(SDL_GetGamepadAxis(gamepad_, axis_id)) / axis_max;
  }

  [[nodiscard]] float trigger(SDL_GamepadAxis axis_id) const {
    constexpr float axis_max = 32767.0F;
    return std::clamp(static_cast<float>(SDL_GetGamepadAxis(gamepad_, axis_id)) / axis_max, 0.0F, 1.0F);
  }

  void close() {
    if (gamepad_ != nullptr) {
      SDL_CloseGamepad(gamepad_);
      gamepad_ = nullptr;
      joystick_id_ = 0;
    }
  }

  SDL_Gamepad* gamepad_{nullptr};
  SDL_JoystickID joystick_id_{0};
};

void log_gamepad_state() {
  int gamepad_count = 0;
  SDL_JoystickID* gamepads = SDL_GetGamepads(&gamepad_count);
  if (gamepads == nullptr || gamepad_count == 0) {
    std::cout << "No gamepad detected at startup.\n";
    SDL_free(gamepads);
    return;
  }

  std::cout << "Detected " << gamepad_count << " gamepad";
  if (gamepad_count != 1) {
    std::cout << "s";
  }
  std::cout << " at startup.\n";

  for (int index = 0; index < gamepad_count; ++index) {
    SDL_Gamepad* gamepad = SDL_OpenGamepad(gamepads[index]);
    if (gamepad != nullptr) {
      const char* name = SDL_GetGamepadName(gamepad);
      std::cout << "  - " << (name != nullptr ? name : "Unknown gamepad") << "\n";
      SDL_CloseGamepad(gamepad);
    }
  }
  SDL_free(gamepads);
}

[[nodiscard]] std::string make_title(
  const hyperverse::FlightHudSnapshot& hud,
  const hyperverse::CameraState& camera,
  const hyperverse::TargetLockModel& target_lock,
  const hyperverse::MiningHudSnapshot& mining,
  const hyperverse::CargoHudSnapshot& cargo,
  const hyperverse::CargoEscortHudSnapshot& escort,
  const hyperverse::CargoTrainHudSnapshot& train,
  const hyperverse::CargoEscortRouteHudSnapshot& route,
  const hyperverse::SectorPressureHudSnapshot& pressure,
  const hyperverse::MiningDroneHudSnapshot& drone,
  const hyperverse::CollisionHudSnapshot& collision
) {
  const char* mapping = hud.control_mapping == hyperverse::ControlMapping::Gamepad ? "gamepad" : "keyboard";
  std::ostringstream title;
  title << hyperverse::application_name() << " " << hyperverse::version() << " | pos " << std::fixed << std::setprecision(0)
        << hud.position.x << "," << hud.position.y << " | speed " << hud.speed << " (" << std::setprecision(0)
        << (hud.speed_fraction * 100.0F) << "%)"
        << " | cam " << camera.position.x << "," << camera.position.y << " | edge " << hud.nearest_wrap_edge_distance;
  if (hud.wrap_warning) {
    title << " WRAP";
  }
  if (hyperverse::has_locked_target(target_lock)) {
    title << " | target " << target_lock.wrapped_distance << " scan " << std::setprecision(0)
          << (target_lock.scan_confidence * 100.0F) << "%"
          << " close " << target_lock.closing_speed;
  } else if (mining.target != entt::null) {
    title << " | mining target";
  }
  if (hyperverse::has_locked_target(target_lock) || mining.target != entt::null) {
    title << " | rock integrity " << mining.target_integrity << " heat " << mining.target_heat << " stress "
          << mining.target_structural_stress << " gas " << mining.target_volatile_pressure << " ore " << mining.extracted_mass;
    if (!mining.target_in_range) {
      title << " OUT OF RANGE";
    }
  } else {
    title << " | target none";
  }
  if (mining.beam_active) {
    title << " | ZAP";
  }
  if (mining.blowout) {
    title << " | BLOWOUT";
  } else if (mining.unstable) {
    title << " | UNSTABLE";
  } else if (mining.gas_venting) {
    title << " | VENTING";
  }
  title << " | cargo " << cargo.delivered_mass << "/" << cargo.required_mass << " boxes " << cargo.cargo_boxes
        << " payout x" << std::setprecision(2) << cargo.payout_multiplier;
  if (escort.cargo_train_active) {
    title << " TRAIN ACTIVE";
    title << " len " << train.train_length << " stress " << train.max_coupling_stress;
    title << " gate " << route.gate_distance;
    if (route.gate_reached) {
      title << " ARRIVED";
    }
  } else if (escort.phase == hyperverse::CargoEscortPhase::Complete) {
    title << " DELIVERED";
  } else if (escort.phase == hyperverse::CargoEscortPhase::Authorized) {
    title << " ESCORT ARMED";
  } else if (cargo.extraction_authorized) {
    title << " EXTRACT";
  }
  title << " | pressure L" << pressure.escalation_level << " " << (pressure.pressure_fraction * 100.0F) << "%"
        << " next " << pressure.next_escalation_seconds << "s";
  if (pressure.escalation_announced) {
    title << " ESCALATION";
  }
  const char* drone_phase = "idle";
  if (drone.phase == hyperverse::MiningDronePhase::Travelling) {
    drone_phase = "travel";
  } else if (drone.phase == hyperverse::MiningDronePhase::Mining) {
    drone_phase = "mine";
  }
  title << " | drone " << drone_phase << " d" << drone.target_distance << " ore " << drone.extracted_mass;
  if (collision.contact) {
    title << " | COLLISION " << collision.impact_speed;
  } else if (collision.warning) {
    title << " | IMPACT " << collision.time_to_contact_seconds << "s";
  }
  title << " | " << mapping;
  return title.str();
}

[[nodiscard]] hyperverse::SpriteDraw make_world_sprite(
  hyperverse::SpriteTexture texture,
  hyperverse::Vec2 world_position,
  hyperverse::Vec2 camera_position,
  const hyperverse::SectorTuning& sector,
  std::uint32_t width,
  std::uint32_t height,
  float pixel_size,
  float pixel_height,
  float rotation_radians
) {
  constexpr float pixels_per_world_unit = 0.35F;
  constexpr float screen_anchor_y_fraction = 0.75F;
  const hyperverse::Vec2 relative = hyperverse::wrapped_delta(camera_position, world_position, sector) * pixels_per_world_unit;
  const float screen_x = (static_cast<float>(width) * 0.5F) + relative.x;
  const float screen_y = (static_cast<float>(height) * screen_anchor_y_fraction) + relative.y;

  return {
    .texture = texture,
    .center_x_ndc = ((screen_x / static_cast<float>(width)) * 2.0F) - 1.0F,
    .center_y_ndc = ((screen_y / static_cast<float>(height)) * 2.0F) - 1.0F,
    .half_width_ndc = pixel_size / static_cast<float>(width),
    .half_height_ndc = pixel_height / static_cast<float>(height),
    .rotation_radians = rotation_radians,
  };
}

[[nodiscard]] float ship_sprite_rotation(float facing_radians) {
  return facing_radians + (std::numbers::pi_v<float> * 0.5F);
}

void tint_sprite(hyperverse::SpriteDraw& sprite, float r, float g, float b, float a = 1.0F) {
  sprite.tint_r = r;
  sprite.tint_g = g;
  sprite.tint_b = b;
  sprite.tint_a = a;
}

void add_target_bracket_lines(
  std::vector<hyperverse::LineDraw>& lines,
  const hyperverse::SpriteDraw& bounds,
  float r,
  float g,
  float b
) {
  const float left = bounds.center_x_ndc - bounds.half_width_ndc;
  const float right = bounds.center_x_ndc + bounds.half_width_ndc;
  const float bottom = bounds.center_y_ndc - bounds.half_height_ndc;
  const float top = bounds.center_y_ndc + bounds.half_height_ndc;
  const float horizontal = bounds.half_width_ndc * 0.38F;
  const float vertical = bounds.half_height_ndc * 0.38F;
  const auto add_line = [&](float x0, float y0, float x1, float y1) {
    lines.push_back({.start_x_ndc = x0, .start_y_ndc = y0, .end_x_ndc = x1, .end_y_ndc = y1, .r = r, .g = g, .b = b});
  };

  add_line(left, top, left + horizontal, top);
  add_line(left, top, left, top - vertical);
  add_line(right, top, right - horizontal, top);
  add_line(right, top, right, top - vertical);
  add_line(left, bottom, left + horizontal, bottom);
  add_line(left, bottom, left, bottom + vertical);
  add_line(right, bottom, right - horizontal, bottom);
  add_line(right, bottom, right, bottom + vertical);
}

void add_box_lines(std::vector<hyperverse::LineDraw>& lines, const hyperverse::SpriteDraw& bounds, float r, float g, float b) {
  const float left = bounds.center_x_ndc - bounds.half_width_ndc;
  const float right = bounds.center_x_ndc + bounds.half_width_ndc;
  const float bottom = bounds.center_y_ndc - bounds.half_height_ndc;
  const float top = bounds.center_y_ndc + bounds.half_height_ndc;
  const auto add_line = [&](float x0, float y0, float x1, float y1) {
    lines.push_back({.start_x_ndc = x0, .start_y_ndc = y0, .end_x_ndc = x1, .end_y_ndc = y1, .r = r, .g = g, .b = b});
  };

  add_line(left, top, right, top);
  add_line(right, top, right, bottom);
  add_line(right, bottom, left, bottom);
  add_line(left, bottom, left, top);
}

void add_world_link_line(
  std::vector<hyperverse::LineDraw>& lines,
  hyperverse::Vec2 from,
  hyperverse::Vec2 to,
  hyperverse::Vec2 camera_position,
  const hyperverse::SectorTuning& sector,
  std::uint32_t width,
  std::uint32_t height,
  float r,
  float g,
  float b
) {
  const hyperverse::SpriteDraw from_point =
    make_world_sprite(hyperverse::SpriteTexture::Reticle, from, camera_position, sector, width, height, 1.0F, 1.0F, 0.0F);
  const hyperverse::SpriteDraw to_point =
    make_world_sprite(hyperverse::SpriteTexture::Reticle, to, camera_position, sector, width, height, 1.0F, 1.0F, 0.0F);
  lines.push_back({
    .start_x_ndc = from_point.center_x_ndc,
    .start_y_ndc = from_point.center_y_ndc,
    .end_x_ndc = to_point.center_x_ndc,
    .end_y_ndc = to_point.center_y_ndc,
    .r = r,
    .g = g,
    .b = b,
  });
}

[[nodiscard]] hyperverse::SpriteDraw make_world_sprite(
  hyperverse::SpriteTexture texture,
  hyperverse::Vec2 world_position,
  hyperverse::Vec2 camera_position,
  const hyperverse::SectorTuning& sector,
  std::uint32_t width,
  std::uint32_t height,
  float pixel_size,
  float rotation_radians = 0.0F
) {
  return make_world_sprite(texture, world_position, camera_position, sector, width, height, pixel_size, pixel_size, rotation_radians);
}

[[nodiscard]] float asteroid_sprite_size(const hyperverse::AsteroidBody& asteroid, const hyperverse::MiningResource* resource) {
  constexpr float depleted_scale = 0.24F;
  constexpr float fresh_scale = 0.45F;
  if (resource == nullptr) {
    return asteroid.radius * fresh_scale;
  }

  const float integrity_fraction = std::clamp(resource->integrity / 100.0F, 0.0F, 1.0F);
  return asteroid.radius * (depleted_scale + ((fresh_scale - depleted_scale) * integrity_fraction));
}

[[nodiscard]] hyperverse::SpriteDraw make_laser_sprite(
  hyperverse::Vec2 from_world,
  hyperverse::Vec2 to_world,
  hyperverse::Vec2 camera_position,
  const hyperverse::SectorTuning& sector,
  std::uint32_t width,
  std::uint32_t height
) {
  const hyperverse::Vec2 delta = hyperverse::wrapped_delta(from_world, to_world, sector);
  const hyperverse::Vec2 midpoint = hyperverse::wrap_position(from_world + (delta * 0.5F), sector);
  constexpr float pixels_per_world_unit = 0.35F;
  const float pixel_length = hyperverse::length(delta) * pixels_per_world_unit;
  return make_world_sprite(
    hyperverse::SpriteTexture::Laser,
    midpoint,
    camera_position,
    sector,
    width,
    height,
    std::max(24.0F, pixel_length),
    18.0F,
    std::atan2(delta.y, delta.x)
  );
}

}  // namespace

namespace hyperverse {

int App::run() {
  try {
    SdlRuntime sdl;
    Window window;
    VulkanRenderer renderer{window.get()};
    GrandCentral grand_central{std::cout};
    AccountCtx account = grand_central.account_context();
    account.log().info("starting flight laboratory");

    const entt::entity player = account.registry().create();
    auto& ship = account.registry().emplace<ShipMotion>(player);
    ship.position = {.x = 4500.0F, .y = 4500.0F};
    account.registry().emplace<CameraState>(player, CameraState{.position = ship.position});
    account.registry().emplace<TargetLockModel>(player);
    account.registry().emplace<MiningHudSnapshot>(player);
    account.registry().emplace<CargoManifest>(player);
    account.registry().emplace<CargoHudSnapshot>(player);
    account.registry().emplace<CargoEscortState>(player);
    account.registry().emplace<CargoEscortHudSnapshot>(player);
    account.registry().emplace<CargoTrainHudSnapshot>(player);
    account.registry().emplace<CargoEscortRouteHudSnapshot>(player);
    account.registry().emplace<SectorPressureModel>(player);
    account.registry().emplace<SectorPressureHudSnapshot>(player);
    account.registry().emplace<MiningDroneHudSnapshot>(player);
    account.registry().emplace<CollisionHudSnapshot>(player);

    const entt::entity mining_drone = account.registry().create();
    account.registry().emplace<MiningDrone>(
      mining_drone,
      MiningDrone{.position = {.x = ship.position.x - 160.0F, .y = ship.position.y + 120.0F}}
    );

    const entt::entity primary_asteroid = account.registry().create();
    account.registry().emplace<AsteroidBody>(
      primary_asteroid,
      AsteroidBody{.position = {.x = 5650.0F, .y = 3850.0F}, .radius = 220.0F, .scan_confidence = 0.34F}
    );
    account.registry().emplace<MiningResource>(primary_asteroid);
    const entt::entity secondary_asteroid = account.registry().create();
    account.registry().emplace<AsteroidBody>(
      secondary_asteroid,
      AsteroidBody{.position = {.x = 3825.0F, .y = 5200.0F}, .radius = 150.0F, .scan_confidence = 0.58F}
    );
    account.registry().emplace<MiningResource>(secondary_asteroid);
    GamepadSlot gamepad;
    gamepad.open_first_available();
    FixedTimestep timestep{1.0F / 60.0F};
    const SectorTuning sector{};
    const FlightTuning flight{};
    const CameraTuning camera_tuning{};
    const MiningLaserTuning mining_laser{};
    const ContractQuotaTuning quota{};
    const ExtractionSite extraction_site{.position = {.x = 4300.0F, .y = 4300.0F}};
    const CargoBoxTuning cargo_box_tuning{.box_mass = quota.cargo_box_mass};
    const CargoEscortRoute escort_route{.gate_position = {.x = 7600.0F, .y = 1600.0F}};
    const SectorPressureTuning pressure_tuning{.escalation_interval_seconds = 30.0F};
    const MiningDroneTuning mining_drone_tuning{};
    FlightInputMapper input_mapper;
    SemanticInputFrame latest_intent{};

    std::cout << application_name() << " " << version() << "\n";
    log_gamepad_state();

    bool running = true;
    auto previous_time = std::chrono::steady_clock::now();
    float hud_title_accumulator = 0.0F;

    while (running) {
      const auto current_time = std::chrono::steady_clock::now();
      const float elapsed_seconds = std::chrono::duration<float>(current_time - previous_time).count();
      previous_time = current_time;
      timestep.accumulate(elapsed_seconds);
      hud_title_accumulator += elapsed_seconds;

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
        latest_intent = input_mapper.map(gamepad.sample());
        simulate_assisted_flight(ship, latest_intent, flight, sector, timestep.tick_seconds());
        CameraState& camera = account.registry().get<CameraState>(player);
        TargetLockModel& target_lock = account.registry().get<TargetLockModel>(player);
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
        CollisionHudSnapshot& collision_hud = account.registry().get<CollisionHudSnapshot>(player);
        update_camera_anchor(camera, ship, sector, camera_tuning, timestep.tick_seconds());
        update_target_lock(target_lock, account.registry(), ship.position, ship.velocity, latest_intent, sector);
        mining_hud =
          update_mining_laser(account.registry(), target_lock, ship, latest_intent, sector, mining_laser, timestep.tick_seconds());
        drone_hud = update_mining_drone(
          account.registry().get<MiningDrone>(mining_drone),
          account.registry(),
          target_lock,
          sector,
          timestep.tick_seconds(),
          mining_drone_tuning
        );
        cargo_hud = update_cargo_manifest(cargo_manifest, account.registry(), quota);
        (void)sync_cargo_boxes(account.registry(), cargo_manifest, extraction_site, cargo_box_tuning);
        escort_hud = update_cargo_escort_state(cargo_escort, cargo_hud, latest_intent);
        route_hud = update_cargo_escort_route(cargo_escort, escort_route, ship, sector);
        escort_hud = update_cargo_escort_arrival(cargo_escort, cargo_hud, route_hud);
        train_hud = update_cargo_train(account.registry(), cargo_escort, ship, sector, timestep.tick_seconds());
        pressure_hud = update_sector_pressure(pressure, timestep.tick_seconds(), pressure_tuning);
        collision_hud = predict_ship_asteroid_collision(ship, account.registry(), sector);
      }

      const FlightHudSnapshot hud = make_flight_hud_snapshot(ship, latest_intent, flight, sector);
      const CameraState& camera = account.registry().get<CameraState>(player);
      const TargetLockModel& target_lock = account.registry().get<TargetLockModel>(player);
      const MiningHudSnapshot& mining_hud = account.registry().get<MiningHudSnapshot>(player);
      const CargoHudSnapshot& cargo_hud = account.registry().get<CargoHudSnapshot>(player);
      const CargoEscortHudSnapshot& escort_hud = account.registry().get<CargoEscortHudSnapshot>(player);
      const CargoTrainHudSnapshot& train_hud = account.registry().get<CargoTrainHudSnapshot>(player);
      const CargoEscortRouteHudSnapshot& route_hud = account.registry().get<CargoEscortRouteHudSnapshot>(player);
      const SectorPressureHudSnapshot& pressure_hud = account.registry().get<SectorPressureHudSnapshot>(player);
      const MiningDroneHudSnapshot& drone_hud = account.registry().get<MiningDroneHudSnapshot>(player);
      const CollisionHudSnapshot& collision_hud = account.registry().get<CollisionHudSnapshot>(player);

      if (hud_title_accumulator >= 0.25F) {
        window.set_title(make_title(hud, camera, target_lock, mining_hud, cargo_hud, escort_hud, train_hud, route_hud, pressure_hud, drone_hud, collision_hud));
        hud_title_accumulator = 0.0F;
      }

      renderer.draw_frame({
        .state = {
          .speed_fraction = hud.speed_fraction,
          .wrap_warning = hud.wrap_warning,
          .target_locked = has_locked_target(target_lock),
          .mining_active = mining_hud.beam_active,
        },
        .sprites = [&] {
          std::vector<SpriteDraw> sprites;
          for (auto [entity, asteroid] : account.registry().view<AsteroidBody>().each()) {
            SpriteDraw asteroid_sprite = make_world_sprite(
              SpriteTexture::Rock,
              asteroid.position,
              camera.position,
              sector,
              renderer.width(),
              renderer.height(),
              asteroid_sprite_size(asteroid, account.registry().try_get<MiningResource>(entity))
            );
            if (entity == mining_hud.target && mining_hud.blowout) {
              tint_sprite(asteroid_sprite, 1.0F, 0.24F, 0.12F);
            } else if (entity == mining_hud.target && mining_hud.unstable) {
              tint_sprite(asteroid_sprite, 1.0F, 0.68F, 0.18F);
            }
            sprites.push_back(asteroid_sprite);
          }
          if (has_locked_target(target_lock) && account.registry().valid(target_lock.target)) {
            const AsteroidBody& target = account.registry().get<AsteroidBody>(target_lock.target);
            SpriteDraw reticle = make_world_sprite(
              SpriteTexture::Reticle,
              target.position,
              camera.position,
              sector,
              renderer.width(),
              renderer.height(),
              (target.radius * 0.55F) + 24.0F
            );
            if (collision_hud.contact) {
              tint_sprite(reticle, 1.0F, 0.20F, 0.15F);
            } else if (collision_hud.warning) {
              tint_sprite(reticle, 1.0F, 0.85F, 0.20F);
            }
            sprites.push_back(reticle);
          }
          if (mining_hud.beam_active) {
            SpriteDraw laser = make_laser_sprite(
              ship.position,
              mining_hud.beam_end_position,
              camera.position,
              sector,
              renderer.width(),
              renderer.height()
            );
            tint_sprite(laser, 1.0F, 0.72F, 0.22F);
            sprites.push_back(laser);
          }
          const MiningDrone& drone = account.registry().get<MiningDrone>(mining_drone);
          SpriteDraw drone_sprite = make_world_sprite(
            SpriteTexture::Drone,
            drone.position,
            camera.position,
            sector,
            renderer.width(),
            renderer.height(),
            42.0F
          );
          if (drone.phase == MiningDronePhase::Mining) {
            tint_sprite(drone_sprite, 0.55F, 1.0F, 0.65F);
          }
          sprites.push_back(drone_sprite);
          sprites.push_back(make_world_sprite(
            SpriteTexture::Ship,
            ship.position,
            camera.position,
            sector,
            renderer.width(),
            renderer.height(),
            56.0F,
            ship_sprite_rotation(ship.facing_radians)
          ));
          return sprites;
        }(),
        .lines = [&] {
          std::vector<LineDraw> lines;
          if (has_locked_target(target_lock) && account.registry().valid(target_lock.target)) {
            const AsteroidBody& target = account.registry().get<AsteroidBody>(target_lock.target);
            const SpriteDraw reticle_bounds = make_world_sprite(
              SpriteTexture::Reticle,
              target.position,
              camera.position,
              sector,
              renderer.width(),
              renderer.height(),
              (target.radius * 0.55F) + 24.0F
            );
            if (collision_hud.contact) {
              add_target_bracket_lines(lines, reticle_bounds, 1.0F, 0.2F, 0.15F);
            } else if (collision_hud.warning) {
              add_target_bracket_lines(lines, reticle_bounds, 1.0F, 0.85F, 0.2F);
            } else {
              add_target_bracket_lines(lines, reticle_bounds, 0.45F, 0.9F, 1.0F);
            }
          }
          if (route_hud.active) {
            const SpriteDraw gate_bounds = make_world_sprite(
              SpriteTexture::Reticle,
              route_hud.gate_position,
              camera.position,
              sector,
              renderer.width(),
              renderer.height(),
              96.0F
            );
            if (route_hud.gate_reached) {
              add_box_lines(lines, gate_bounds, 0.65F, 1.0F, 0.45F);
            } else {
              add_box_lines(lines, gate_bounds, 0.35F, 0.9F, 1.0F);
            }
            add_world_link_line(
              lines,
              ship.position,
              route_hud.gate_position,
              camera.position,
              sector,
              renderer.width(),
              renderer.height(),
              0.2F,
              0.55F,
              0.85F
            );
          }
          for (auto [entity, box] : account.registry().view<CargoBox>().each()) {
            (void)entity;
            const SpriteDraw box_bounds = make_world_sprite(
              SpriteTexture::Reticle,
              box.position,
              camera.position,
              sector,
              renderer.width(),
              renderer.height(),
              28.0F
            );
            if (escort_hud.cargo_train_active) {
              add_box_lines(lines, box_bounds, 0.35F, 0.9F, 1.0F);
            } else {
              add_box_lines(lines, box_bounds, 0.4F, 1.0F, 0.52F);
            }
          }
          if (escort_hud.cargo_train_active) {
            std::vector<const CargoBox*> linked_boxes;
            for (auto [entity, box] : account.registry().view<CargoBox>().each()) {
              (void)entity;
              linked_boxes.push_back(&box);
            }
            std::ranges::sort(linked_boxes, [](const CargoBox* lhs, const CargoBox* rhs) { return lhs->index < rhs->index; });

            Vec2 anchor = ship.position;
            for (const CargoBox* box : linked_boxes) {
              add_world_link_line(lines, anchor, box->position, camera.position, sector, renderer.width(), renderer.height(), 0.35F, 0.9F, 1.0F);
              anchor = box->position;
            }
          }
          return lines;
        }(),
      });
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
