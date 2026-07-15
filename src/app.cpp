#include "hyperverse/app.hpp"

#include "hyperverse/camera.hpp"
#include "hyperverse/fixed_timestep.hpp"
#include "hyperverse/flight.hpp"
#include "hyperverse/grand_central.hpp"
#include "hyperverse/input.hpp"
#include "hyperverse/targeting.hpp"
#include "hyperverse/version.hpp"
#include "hyperverse/vulkan_renderer.hpp"

#include <SDL3/SDL.h>

#include <chrono>
#include <exception>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

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
    }

    return raw;
  }

private:
  [[nodiscard]] float axis(SDL_GamepadAxis axis_id) const {
    constexpr float axis_max = 32767.0F;
    return static_cast<float>(SDL_GetGamepadAxis(gamepad_, axis_id)) / axis_max;
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
  const hyperverse::TargetLockModel& target_lock
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
  } else {
    title << " | target none";
  }
  title << " | " << mapping;
  return title.str();
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
    CameraState camera{.position = ship.position};

    const entt::entity primary_asteroid = account.registry().create();
    account.registry().emplace<AsteroidBody>(
      primary_asteroid,
      AsteroidBody{.position = {.x = 5650.0F, .y = 3850.0F}, .radius = 220.0F, .scan_confidence = 0.34F}
    );
    const entt::entity secondary_asteroid = account.registry().create();
    account.registry().emplace<AsteroidBody>(
      secondary_asteroid,
      AsteroidBody{.position = {.x = 3825.0F, .y = 5200.0F}, .radius = 150.0F, .scan_confidence = 0.58F}
    );
    TargetLockModel target_lock{};

    GamepadSlot gamepad;
    gamepad.open_first_available();
    FixedTimestep timestep{1.0F / 60.0F};
    const SectorTuning sector{};
    const FlightTuning flight{};
    const CameraTuning camera_tuning{};
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
        update_camera_anchor(camera, ship, sector, camera_tuning, timestep.tick_seconds());
        update_target_lock(target_lock, account.registry(), ship.position, ship.velocity, latest_intent, sector);
      }

      const FlightHudSnapshot hud = make_flight_hud_snapshot(ship, latest_intent, flight, sector);

      if (hud_title_accumulator >= 0.25F) {
        window.set_title(make_title(hud, camera, target_lock));
        hud_title_accumulator = 0.0F;
      }

      renderer.draw_frame({
        .speed_fraction = hud.speed_fraction,
        .wrap_warning = hud.wrap_warning,
        .target_locked = has_locked_target(target_lock),
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
