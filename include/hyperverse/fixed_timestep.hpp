#pragma once

namespace hyperverse {

class FixedTimestep {
public:
  explicit constexpr FixedTimestep(float tick_seconds) : tick_seconds_{tick_seconds} {}

  void accumulate(float elapsed_seconds);
  [[nodiscard]] bool consume_tick();
  [[nodiscard]] constexpr float tick_seconds() const {
    return tick_seconds_;
  }
  [[nodiscard]] float alpha() const;

private:
  float tick_seconds_{1.0F / 60.0F};
  float accumulator_{0.0F};
};

}  // namespace hyperverse
