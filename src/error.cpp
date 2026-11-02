// SPDX-License-Identifier: MIT
#include "dawproject/error.hpp"

namespace dawproject {

Error::Error(ErrorKind kind, const std::string& message)
    : std::runtime_error(message), kind_(kind) {}

}  // namespace dawproject
