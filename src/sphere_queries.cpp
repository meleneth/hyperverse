#include "sphere_queries.hpp"

#include <algorithm>
#include <cmath>

namespace hyperverse {
namespace {

[[nodiscard]] SphereCastHit solve_circle_intersection(float a, float b, float c, float max_fraction) {
  if (c <= 0.0F) {
    return {.hit = true, .fraction = 0.0F};
  }
  if (a <= 0.0F) {
    return {};
  }

  const float discriminant = (b * b) - (4.0F * a * c);
  if (discriminant < 0.0F) {
    return {};
  }

  const float root = std::sqrt(discriminant);
  const float denominator = 2.0F * a;
  const float first = (-b - root) / denominator;
  const float second = (-b + root) / denominator;
  const float fraction = first >= 0.0F ? first : second;
  if (fraction < 0.0F || fraction > max_fraction) {
    return {};
  }

  return {.hit = true, .fraction = fraction};
}

}  // namespace

bool circles_overlap(Vec2 center_delta, float combined_radius) {
  return dot(center_delta, center_delta) <= combined_radius * combined_radius;
}

SphereCastHit cast_circle(Vec2 target_delta, Vec2 motion, float combined_radius) {
  const float a = dot(motion, motion);
  const float b = -2.0F * dot(motion, target_delta);
  const float c = dot(target_delta, target_delta) - (combined_radius * combined_radius);
  return solve_circle_intersection(a, b, c, 1.0F);
}

SphereCastHit raycast_circle(Vec2 target_delta, Vec2 direction, float range, float radius) {
  const Vec2 normalized_direction = normalize_or_zero(direction);
  if (length(normalized_direction) <= 0.0F || range <= 0.0F) {
    return {};
  }

  const float a = dot(normalized_direction, normalized_direction);
  const float b = -2.0F * dot(normalized_direction, target_delta);
  const float c = dot(target_delta, target_delta) - (radius * radius);
  SphereCastHit hit = solve_circle_intersection(a, b, c, range);
  if (!hit.hit) {
    return {};
  }

  hit.fraction = std::clamp(hit.fraction / range, 0.0F, 1.0F);
  return hit;
}

}  // namespace hyperverse
