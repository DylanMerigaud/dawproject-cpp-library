// SPDX-License-Identifier: MIT
/// @file error.hpp
/// The single exception type thrown by the library.
#pragma once

#include <stdexcept>
#include <string>

#include "dawproject/export.hpp"

namespace dawproject {

/// Category of an Error.
enum class ErrorKind {
  Io,      ///< A file could not be read or written.
  Zip,     ///< The zip container is malformed or misses a required entry.
  Xml,     ///< The XML text is not well formed.
  Schema,  ///< The XML is well formed but does not match the DAWproject schema.
};

/// Exception thrown by every failing library call.
class DAWPROJECT_EXPORT Error : public std::runtime_error {
 public:
  /// Creates an error of the given kind with a human readable message.
  Error(ErrorKind kind, const std::string& message);

  /// The category of this error.
  [[nodiscard]] ErrorKind kind() const noexcept { return kind_; }

 private:
  ErrorKind kind_;
};

}  // namespace dawproject
