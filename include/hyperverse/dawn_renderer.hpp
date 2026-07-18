#pragma once

#include "hyperverse/render_frame.hpp"

#include <SDL3/SDL_video.h>

#include <cstdint>
#include <memory>

namespace hyperverse {

class DawnRenderer {
public:
  explicit DawnRenderer(SDL_Window& window);
  ~DawnRenderer();

  DawnRenderer(const DawnRenderer&) = delete;
  DawnRenderer& operator=(const DawnRenderer&) = delete;
  DawnRenderer(DawnRenderer&&) = delete;
  DawnRenderer& operator=(DawnRenderer&&) = delete;

  void draw_frame(const FrameSnapshot& frame = {});
  void draw_frame(const SpriteFrame& frame);
  void refresh_extent();
  void wait_idle() const;
  [[nodiscard]] std::uint32_t width() const;
  [[nodiscard]] std::uint32_t height() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace hyperverse
