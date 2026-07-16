#include "jolt_shape_queries.hpp"

#include "png_rgba.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/Memory.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/CollisionDispatch.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/RegisterTypes.h>
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include <algorithm>
#include <cmath>
#include <mutex>
#include <stdexcept>
#include <unordered_map>

namespace hyperverse {
namespace {

void ensure_jolt_shape_queries_ready() {
  static std::once_flag jolt_registration;
  std::call_once(jolt_registration, [] {
    JPH::RegisterDefaultAllocator();
    if (JPH::Factory::sInstance == nullptr) {
      JPH::Factory::sInstance = new JPH::Factory();
    }
    JPH::RegisterTypes();
  });
}

[[nodiscard]] const char* sprite_path(SpriteCollisionShape shape) {
  switch (shape) {
    case SpriteCollisionShape::Ship:
      return "assets/sector7/sprites/ship.png";
    case SpriteCollisionShape::Rock:
      return "assets/sector7/sprites/rock1.png";
    case SpriteCollisionShape::Particle:
      return "assets/sector7/sprites/particle.png";
  }
  return "assets/sector7/sprites/rock1.png";
}

[[nodiscard]] int shape_key(SpriteCollisionShape shape, float radius) {
  return (static_cast<int>(shape) * 1'000'000'000) + static_cast<int>(std::lround(radius * 1000.0F));
}

[[nodiscard]] JPH::Array<JPH::Vec3> extruded_hull_points(const SpriteSilhouette& silhouette) {
  JPH::Array<JPH::Vec3> points;
  constexpr float half_thickness = 0.04F;
  points.reserve(static_cast<JPH::uint>(silhouette.hull.size() * 2U));
  for (Vec2 point : silhouette.hull) {
    points.push_back(JPH::Vec3(point.x, point.y, -half_thickness));
    points.push_back(JPH::Vec3(point.x, point.y, half_thickness));
  }
  return points;
}

[[nodiscard]] JPH::ShapeRefC create_sprite_shape(SpriteCollisionShape shape) {
  const SpriteSilhouette silhouette = extract_sprite_silhouette(load_png_rgba(sprite_path(shape)));
  if (silhouette.hull.size() < 3U) {
    return new JPH::SphereShape(1.0F);
  }

  const JPH::Array<JPH::Vec3> points = extruded_hull_points(silhouette);
  const JPH::ConvexHullShapeSettings settings{points, 0.0F};
  JPH::ShapeSettings::ShapeResult result = settings.Create();
  if (!result.IsValid()) {
    throw std::runtime_error("failed to create sprite collision shape");
  }
  return result.Get();
}

[[nodiscard]] const JPH::Shape* cached_sprite_shape(SpriteCollisionShape shape, float radius) {
  ensure_jolt_shape_queries_ready();

  static std::mutex shape_mutex;
  static std::unordered_map<int, JPH::ShapeRefC> shapes;

  const int key = shape_key(shape, radius);
  std::lock_guard lock{shape_mutex};
  auto [iterator, inserted] = shapes.try_emplace(key);
  if (inserted) {
    const JPH::ShapeRefC sprite_shape = create_sprite_shape(shape);
    iterator->second = sprite_shape->ScaleShape(JPH::Vec3::sReplicate(std::max(0.001F, radius))).Get();
  }
  return iterator->second.GetPtr();
}

[[nodiscard]] JPH::Vec3 to_jolt(Vec2 position) {
  return JPH::Vec3(position.x, position.y, 0.0F);
}

[[nodiscard]] JPH::Mat44 translated(Vec2 position) {
  return JPH::Mat44::sTranslation(to_jolt(position));
}

class AnyHitCollector final : public JPH::CollideShapeCollector {
public:
  void AddHit(const JPH::CollideShapeResult& result) override {
    (void)result;
    has_hit = true;
    ForceEarlyOut();
  }

  bool has_hit{false};
};

}  // namespace

bool jolt_shapes_overlap(
  SpriteCollisionShape first_shape,
  float first_radius,
  SpriteCollisionShape second_shape,
  float second_radius,
  Vec2 second_position
) {
  AnyHitCollector collector;
  JPH::CollisionDispatch::sCollideShapeVsShape(
    cached_sprite_shape(first_shape, first_radius),
    cached_sprite_shape(second_shape, second_radius),
    JPH::Vec3::sReplicate(1.0F),
    JPH::Vec3::sReplicate(1.0F),
    JPH::Mat44::sIdentity(),
    translated(second_position),
    JPH::SubShapeIDCreator(),
    JPH::SubShapeIDCreator(),
    JPH::CollideShapeSettings{},
    collector
  );
  return collector.has_hit;
}

ShapeQueryHit jolt_cast_shape(
  SpriteCollisionShape moving_shape,
  float moving_radius,
  SpriteCollisionShape target_shape,
  float target_radius,
  Vec2 target_position,
  Vec2 motion
) {
  if (length(motion) <= 0.0F) {
    return {};
  }

  JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
  const JPH::ShapeCast shape_cast{
    cached_sprite_shape(moving_shape, moving_radius),
    JPH::Vec3::sReplicate(1.0F),
    translated({.x = -target_position.x, .y = -target_position.y}),
    to_jolt(motion)
  };
  JPH::CollisionDispatch::sCastShapeVsShapeLocalSpace(
    shape_cast,
    JPH::ShapeCastSettings{},
    cached_sprite_shape(target_shape, target_radius),
    JPH::Vec3::sReplicate(1.0F),
    JPH::ShapeFilter{},
    JPH::Mat44::sIdentity(),
    JPH::SubShapeIDCreator(),
    JPH::SubShapeIDCreator(),
    collector
  );

  if (!collector.HadHit()) {
    return {};
  }
  return {.hit = true, .fraction = collector.mHit.mFraction};
}

ShapeQueryHit jolt_raycast_shape(
  SpriteCollisionShape target_shape,
  float target_radius,
  Vec2 target_position,
  Vec2 direction,
  float range
) {
  const Vec2 normalized_direction = normalize_or_zero(direction);
  if (length(normalized_direction) <= 0.0F || range <= 0.0F) {
    return {};
  }

  JPH::RayCastResult hit;
  const JPH::RayCast ray{
    JPH::Vec3(-target_position.x, -target_position.y, 0.0F),
    JPH::Vec3(normalized_direction.x * range, normalized_direction.y * range, 0.0F)
  };
  if (!cached_sprite_shape(target_shape, target_radius)->CastRay(ray, JPH::SubShapeIDCreator(), hit)) {
    return {};
  }

  return {.hit = true, .fraction = std::clamp(hit.mFraction, 0.0F, 1.0F)};
}

}  // namespace hyperverse
