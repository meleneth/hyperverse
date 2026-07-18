#include "hyperverse/sdl_platform.hpp"

#include "hyperverse/version.hpp"
#include "png_rgba.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace hyperverse {

SdlRuntime::SdlRuntime() {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS)) {
    throw std::runtime_error(std::string{"SDL_Init failed: "} + SDL_GetError());
  }
}

SdlRuntime::~SdlRuntime() {
  SDL_Quit();
}

Window::Window() {
  window_ = SDL_CreateWindow(application_name().data(), 1280, 720, SDL_WINDOW_RESIZABLE);

  if (window_ == nullptr) {
    throw std::runtime_error(std::string{"SDL_CreateWindow failed: "} + SDL_GetError());
  }

  const SpriteAlphaMask icon = load_png_rgba("assets/sector7/sprites/ship.png");
  SDL_Surface* icon_surface = SDL_CreateSurfaceFrom(
    static_cast<int>(icon.width),
    static_cast<int>(icon.height),
    SDL_PIXELFORMAT_RGBA32,
    const_cast<std::uint8_t*>(icon.rgba.data()),
    static_cast<int>(icon.width * 4U)
  );
  if (icon_surface != nullptr) {
    (void)SDL_SetWindowIcon(window_, icon_surface);
    SDL_DestroySurface(icon_surface);
  }

#if !defined(__EMSCRIPTEN__)
  (void)SDL_SetWindowFullscreen(window_, true);
#endif
}

Window::~Window() {
  if (window_ != nullptr) {
    SDL_DestroyWindow(window_);
  }
}

SDL_Window& Window::get() const {
  return *window_;
}

void Window::set_title(const std::string& title) {
  SDL_SetWindowTitle(window_, title.c_str());
}

GamepadSlot::~GamepadSlot() {
  close();
}

void GamepadSlot::open_first_available() {
  SDL_UpdateGamepads();

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

void GamepadSlot::open(SDL_JoystickID joystick_id) {
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

void GamepadSlot::close_if_removed(SDL_JoystickID joystick_id) {
  if (gamepad_ != nullptr && joystick_id == joystick_id_) {
    close();
  }
}

RawInputFrame GamepadSlot::sample() const {
  RawInputFrame raw{};

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
    raw.gravity_sling = key_down(SDL_SCANCODE_Q);
    raw.particle_fire = key_down(SDL_SCANCODE_E);
    raw.tool_intensity = key_down(SDL_SCANCODE_F) ? 1.0F : 0.0F;
  }

  if (gamepad_ != nullptr) {
    raw.control_mapping = ControlMapping::Gamepad;
    raw.movement_axis = {
      .x = axis(SDL_GAMEPAD_AXIS_LEFTX),
      .y = axis(SDL_GAMEPAD_AXIS_LEFTY),
    };
    raw.aim_axis = {
      .x = axis(SDL_GAMEPAD_AXIS_RIGHTX),
      .y = axis(SDL_GAMEPAD_AXIS_RIGHTY),
    };
    raw.confirm = raw.confirm || SDL_GetGamepadButton(gamepad_, SDL_GAMEPAD_BUTTON_SOUTH);
    raw.boost = raw.boost || SDL_GetGamepadButton(gamepad_, SDL_GAMEPAD_BUTTON_EAST);
    raw.gravity_sling = raw.gravity_sling || SDL_GetGamepadButton(gamepad_, SDL_GAMEPAD_BUTTON_NORTH);
    raw.target_cycle = raw.target_cycle || SDL_GetGamepadButton(gamepad_, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) ||
                       SDL_GetGamepadButton(gamepad_, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
    raw.particle_fire = raw.particle_fire || SDL_GetGamepadButton(gamepad_, SDL_GAMEPAD_BUTTON_WEST);
    raw.tool_intensity = std::max(raw.tool_intensity, trigger(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));
  }

  return raw;
}

float GamepadSlot::axis(SDL_GamepadAxis axis_id) const {
  constexpr float axis_max = 32767.0F;
  return static_cast<float>(SDL_GetGamepadAxis(gamepad_, axis_id)) / axis_max;
}

float GamepadSlot::trigger(SDL_GamepadAxis axis_id) const {
  constexpr float axis_max = 32767.0F;
  return std::clamp(static_cast<float>(SDL_GetGamepadAxis(gamepad_, axis_id)) / axis_max, 0.0F, 1.0F);
}

void GamepadSlot::close() {
  if (gamepad_ != nullptr) {
    SDL_CloseGamepad(gamepad_);
    gamepad_ = nullptr;
    joystick_id_ = 0;
  }
}

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

}  // namespace hyperverse
