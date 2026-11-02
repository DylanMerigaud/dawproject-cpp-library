// SPDX-License-Identifier: MIT
/// @file validate.hpp
/// Consistency checks the XML schema cannot express on its own, and an id generator.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "dawproject/export.hpp"
#include "dawproject/model.hpp"

namespace dawproject {

/// One problem found by validate().
struct Issue {
  std::string path;     ///< Location, such as "Structure/Track[1]/Channel".
  std::string message;  ///< What is wrong.
};

/// Checks a project for duplicate ids, ids and references that are not XML NCNames (xs:ID and
/// xs:IDREF: "1" or "a b" are not), references (Timeline::track, Channel::destination,
/// Send::destination, AutomationTarget::parameter, Clip::reference) naming no id, Markers
/// without a marker, Warps without content or warp points, Scenes without content, and
/// EnumParameter labels that are empty or contain whitespace. Returns an empty vector for a
/// consistent project.
[[nodiscard]] DAWPROJECT_EXPORT std::vector<Issue> validate(const Project& project);

/// Generates the ids "id0", "id1", ... for writers.
class DAWPROJECT_EXPORT IdGenerator {
 public:
  /// The next id.
  [[nodiscard]] std::string next();
  /// Starts again from "id0".
  void reset() noexcept { counter_ = 0; }

 private:
  std::size_t counter_ = 0;
};

}  // namespace dawproject
