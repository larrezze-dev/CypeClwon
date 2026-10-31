#pragma once
#include <string_view>

namespace cc::core {

/// Returns the application version as "major.minor.patch".
std::string_view versionString() noexcept;

/// Returns the product name shown in title bars and the about box.
std::string_view productName() noexcept;

}  // namespace cc::core

