// SPDX-License-Identifier: MIT
/// @file version.hpp
/// Library version macros and the DAWproject format version this library writes.
#pragma once

#include <string_view>

/// Major version of the dawproject library.
#define DAWPROJECT_VERSION_MAJOR 0
/// Minor version of the dawproject library.
#define DAWPROJECT_VERSION_MINOR 1
/// Patch version of the dawproject library.
#define DAWPROJECT_VERSION_PATCH 0
/// Full library version as a string literal.
#define DAWPROJECT_VERSION_STRING "0.1.0"
/// DAWproject format version written in the `version` attribute of `<Project>`.
#define DAWPROJECT_FORMAT_VERSION "1.0"

namespace dawproject {

/// Library version, for example "0.1.0".
inline constexpr std::string_view kLibraryVersion = DAWPROJECT_VERSION_STRING;

/// DAWproject format version written by this library ("1.0").
inline constexpr std::string_view kFormatVersion = DAWPROJECT_FORMAT_VERSION;

}  // namespace dawproject
