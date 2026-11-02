// SPDX-License-Identifier: MIT
// Internal number parsing helpers (formatDouble and parseDouble are public, in xml.hpp).
#pragma once

#include <string_view>

namespace dawproject::detail {

/// Parses an xs:int; false when the text is not an integer in range.
bool parseInt(std::string_view text, int& out) noexcept;

/// Parses an xs:boolean ("true", "false", "1", "0").
bool parseBool(std::string_view text, bool& out) noexcept;

}  // namespace dawproject::detail
