#pragma once

#include "hyperverse/input.hpp"

#include <SDL3/SDL.h>

#include <string>

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
  [[nodiscard]] RawInputFrame sample() const;

private:
  [[nodiscard]] float axis(SDL_GamepadAxis axis_id) const;
  [[nodiscard]] float trigger(SDL_GamepadAxis axis_id) const;
  void close();

  SDL_Gamepad* gamepad_{nullptr};
  SDL_JoystickID joystick_id_{0};
};

void log_gamepad_state();

}  // namespace hyperverse
