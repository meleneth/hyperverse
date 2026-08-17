#pragma once

#include "hyperverse/input.hpp"

#include <SDL3/SDL.h>

#include <string>
#include <vector>

namespace hyperverse {

class SdlRuntime {
public:
  SdlRuntime();
  ~SdlRuntime();

  SdlRuntime(const SdlRuntime&) = delete;
  SdlRuntime& operator=(const SdlRuntime&) = delete;
  SdlRuntime(SdlRuntime&&) = delete;
  SdlRuntime& operator=(SdlRuntime&&) = delete;
};

class Window {
public:
  Window();
  ~Window();

  [[nodiscard]] SDL_Window& get() const;
  void set_title(const std::string& title);

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
  ~GamepadSlot();

  GamepadSlot(const GamepadSlot&) = delete;
  GamepadSlot& operator=(const GamepadSlot&) = delete;
  GamepadSlot(GamepadSlot&&) = delete;
  GamepadSlot& operator=(GamepadSlot&&) = delete;

  void open_first_available();
  void open(SDL_JoystickID joystick_id);
  void close_if_removed(SDL_JoystickID joystick_id);
  [[nodiscard]] RawInputFrame sample();

private:
  [[nodiscard]] static float axis(SDL_Gamepad* gamepad, SDL_GamepadAxis axis_id);
  [[nodiscard]] static float trigger(SDL_Gamepad* gamepad, SDL_GamepadAxis axis_id);
  void close();

  std::vector<SDL_Gamepad*> gamepads_;
  SDL_Gamepad* active_gamepad_{nullptr};
};

void log_gamepad_state();

}  // namespace hyperverse
