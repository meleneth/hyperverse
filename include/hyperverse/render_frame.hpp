#pragma once

#include <vector>

namespace hyperverse {

struct RenderColor {
  float r{0.02F};
  float g{0.025F};
  float b{0.04F};
  float a{1.0F};
};

struct FrameSnapshot {
  float speed_fraction{0.0F};
  bool wrap_warning{false};
  bool target_locked{false};
  bool mining_active{false};
};

[[nodiscard]] RenderColor make_clear_color(const FrameSnapshot& frame);

enum class SpriteTexture {
  Ship,
  Rock,
  Reticle,
  Laser,
  Drone,
  Raider,
  Particle,
  GlyphA,
  GlyphB,
  GlyphC,
  GlyphD,
  GlyphE,
  GlyphF,
  GlyphG,
  GlyphH,
  GlyphI,
  GlyphJ,
  GlyphK,
  GlyphL,
  GlyphM,
  GlyphN,
  GlyphO,
  GlyphP,
  GlyphQ,
  GlyphR,
  GlyphS,
  GlyphT,
  GlyphU,
  GlyphV,
  GlyphW,
  GlyphX,
  GlyphY,
  GlyphZ,
  Glyph0,
  Glyph1,
  Glyph2,
  Glyph3,
  Glyph4,
  Glyph5,
  Glyph6,
  Glyph7,
  Glyph8,
  Glyph9,
};

struct SpriteDraw {
  SpriteTexture texture{SpriteTexture::Ship};
  float center_x_ndc{0.0F};
  float center_y_ndc{0.0F};
  float half_width_ndc{0.05F};
  float half_height_ndc{0.05F};
  float rotation_radians{0.0F};
  float tint_r{1.0F};
  float tint_g{1.0F};
  float tint_b{1.0F};
  float tint_a{1.0F};
};

struct LineDraw {
  float start_x_ndc{0.0F};
  float start_y_ndc{0.0F};
  float end_x_ndc{0.0F};
  float end_y_ndc{0.0F};
  float r{1.0F};
  float g{1.0F};
  float b{1.0F};
  float a{1.0F};
};

struct TriangleVertexDraw {
  float x_ndc{0.0F};
  float y_ndc{0.0F};
  float u{0.0F};
  float v{0.0F};
  float r{1.0F};
  float g{1.0F};
  float b{1.0F};
  float a{1.0F};
};

struct TriangleDraw {
  TriangleVertexDraw a{};
  TriangleVertexDraw b{};
  TriangleVertexDraw c{};
};

struct EngineTrailVertexDraw {
  float x_ndc{0.0F};
  float y_ndc{0.0F};
  float normalized_age{0.0F};
  float intensity{0.0F};
  float side{0.0F};
};

struct SpriteFrame {
  FrameSnapshot state{};
  std::vector<EngineTrailVertexDraw> engine_trails{};
  std::vector<TriangleDraw> triangles{};
  std::vector<SpriteDraw> sprites{};
  std::vector<LineDraw> lines{};
};

}  // namespace hyperverse
