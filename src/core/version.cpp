#include "core/version.h"

namespace cc::core {

std::string_view versionString() noexcept { return CC_VERSION_STRING; }

std::string_view productName() noexcept { return "CypeClwon 3D"; }

}  // namespace cc::core

