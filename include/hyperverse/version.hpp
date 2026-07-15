#pragma once

#include <string_view>

namespace hyperverse {

[[nodiscard]] std::string_view version() noexcept;
[[nodiscard]] std::string_view application_name() noexcept;

}  // namespace hyperverse
