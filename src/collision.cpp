#include "hyperverse/collision.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/Memory.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/CollisionDispatch.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/RegisterTypes.h>
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include <algorithm>
#include <mutex>

namespace hyperverse {
namespace {

void ensure_jolt_collision_dispatch_ready() {
  static std::once_flag jolt_registration;
  std::call_once(jolt_registration, [] {
    JPH::RegisterDefaultAllocator();
    if (JPH::Factory::sInstance == nullptr) {
      JPH::Factory::sInstance = new JPH::Factory();
    }
    JPH::RegisterTypes();
  });
}

[[nodiscard]] JPH::Vec3 to_jolt(Vec2 point) {
  return {point.x, point.y, 0.0F};
}

[[nodiscard]] bool spheres_overlap(
  const JPH::Shape* ship_shape,
  const JPH::Shape* asteroid_shape,
  JPH::Vec3 asteroid_position
) {
  JPH::CollideShapeSettings settings;
  settings.mBackFaceMode = JPH::EBackFaceMode::CollideWithBackFaces;

  JPH::AnyHitCollisionCollector<JPH::CollideShapeCollector> collector;
  JPH::CollisionDispatch::sCollideShapeVsShape(
    ship_shape,
    asteroid_shape,
    JPH::Vec3::sOne(),
    JPH::Vec3::sOne(),
    JPH::Mat44::sIdentity(),
    JPH::Mat44::sTranslation(asteroid_position),
    JPH::SubShapeIDCreator(),
    JPH::SubShapeIDCreator(),
    settings,
    collector
  );

  return collector.HadHit();
}

[[nodiscard]] JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> cast_ship_shape(
  const JPH::Shape* ship_shape,
  const JPH::Shape* asteroid_shape,
  JPH::Vec3 asteroid_position,
  JPH::Vec3 ship_motion
) {
  JPH::ShapeCastSettings settings;
  settings.SetBackFaceMode(JPH::EBackFaceMode::CollideWithBackFaces);
  settings.mReturnDeepestPoint = true;

  JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
  const JPH::ShapeCast cast(ship_shape, JPH::Vec3::sOne(), JPH::Mat44::sIdentity(), ship_motion);
  JPH::CollisionDispatch::sCastShapeVsShapeWorldSpace(
    cast,
    settings,
    asteroid_shape,
    JPH::Vec3::sOne(),
    JPH::ShapeFilter(),
    JPH::Mat44::sTranslation(asteroid_position),
    JPH::SubShapeIDCreator(),
    JPH::SubShapeIDCreator(),
    collector
  );
  return collector;
}

}  // namespace

CollisionHudSnapshot predict_ship_asteroid_collision(
  const ShipMotion& ship,
  entt::registry& registry,
  const SectorTuning& sector,
  const CollisionProbe& probe
) {
  ensure_jolt_collision_dispatch_ready();

  CollisionHudSnapshot nearest{};
  bool found_hit = false;
  float best_time = probe.warning_seconds;

  const JPH::Ref<JPH::Shape> ship_shape = new JPH::SphereShape(probe.ship_radius);

  for (auto [entity, asteroid] : registry.view<AsteroidBody>().each()) {
    const Vec2 relative_position = wrapped_delta(ship.position, asteroid.position, sector);
    const Vec2 relative_velocity = asteroid.velocity - ship.velocity;
    const Vec2 ship_motion = (ship.velocity - asteroid.velocity) * probe.warning_seconds;
    const float motion_length = length(ship_motion);
    const JPH::Vec3 asteroid_position = to_jolt(relative_position);
    const JPH::Ref<JPH::Shape> asteroid_shape = new JPH::SphereShape(asteroid.radius);

    const bool contact = spheres_overlap(ship_shape, asteroid_shape, asteroid_position);
    CollisionHudSnapshot candidate{.contact = contact, .warning = contact, .asteroid = entity};
    if (contact) {
      candidate.impact_speed = length(relative_velocity);
    } else if (motion_length > 0.0001F) {
      const auto collector = cast_ship_shape(ship_shape, asteroid_shape, asteroid_position, to_jolt(ship_motion));
      if (collector.HadHit()) {
        candidate.warning = true;
        candidate.separation = collector.mHit.mFraction * motion_length;
        candidate.time_to_contact_seconds = collector.mHit.mFraction * probe.warning_seconds;
        candidate.impact_speed = motion_length / probe.warning_seconds;
      }
    }

    const bool better_contact = candidate.contact && !nearest.contact;
    const bool first_hit = candidate.warning && !found_hit;
    const bool sooner_hit = candidate.warning && found_hit && candidate.time_to_contact_seconds < best_time;
    if (better_contact || first_hit || sooner_hit) {
      nearest = candidate;
      found_hit = candidate.warning;
      best_time = candidate.time_to_contact_seconds;
    }
  }

  return nearest;
}

}  // namespace hyperverse
