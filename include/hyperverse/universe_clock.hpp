#pragma once

namespace hyperverse {

struct UniverseClock {
  static constexpr float FixedTickSeconds{1.0F / 60.0F};
  static constexpr int FixedTickHertz{60};
};

}  // namespace hyperverse
