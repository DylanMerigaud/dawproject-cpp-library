// SPDX-License-Identifier: MIT
/// @file xml.hpp
/// Parsing and serialization of `project.xml` and `metadata.xml`.
#pragma once

#include <string>
#include <string_view>

#include "dawproject/export.hpp"
#include "dawproject/model.hpp"

namespace dawproject {

/// Options of parseProject() and parseMetaData().
struct ParseOptions {
  /// When false (the default) the parser is lenient about three things, so files written by a
  /// newer version of the format still load: unknown elements are skipped, unknown attributes
  /// are ignored, and an unknown value of an optional enumeration attribute is dropped. An
  /// unknown value of a required enumeration attribute (such as RealParameter `unit` or Device
  /// `deviceRole`) throws in both modes, since the model has no value to put there.
  ///
  /// When true, each of those throws Error (ErrorKind::Schema) naming the element path, and so
  /// do: a second occurrence of a child the schema allows once (two `<Application>`, two
  /// `<Volume>` in a Channel, ...), and `<Markers>` used as a timeline (only the Arrangement
  /// child is `<Markers>`; timelines elsewhere are `<markers>`).
  bool strict = false;
};

/// Parses the text of a `project.xml` document. Throws Error (ErrorKind::Xml) when the text is
/// not well formed and Error (ErrorKind::Schema) when a required element or attribute is missing,
/// a value has the wrong type, or elements nest more than 256 levels deep.
[[nodiscard]] DAWPROJECT_EXPORT Project parseProject(std::string_view xml,
                                                     const ParseOptions& options = {});

/// Parses the text of a `metadata.xml` document. Throws like parseProject().
[[nodiscard]] DAWPROJECT_EXPORT MetaData parseMetaData(std::string_view xml,
                                                       const ParseOptions& options = {});

/// Serializes a project to `project.xml` text: UTF-8, two space indent, elements in the order of
/// the schema, numbers in the shortest form that reads back to the same value. Infinities are
/// written "INF" and "-INF" in the attributes the schema types xs:double (Clip time, duration,
/// playStart, playStop, loopStart, loopEnd, fadeInTime, fadeOutTime; Marker time; Warp time and
/// contentTime; Audio and Video duration) and "inf" and "-inf" in the xs:string ones
/// (RealParameter value, min and max; Note time, duration, vel and rel; Point time; RealPoint
/// value), the spelling of the Java reference.
///
/// Throws Error (ErrorKind::Schema) naming the field when a string holds a character XML cannot
/// carry (a control character other than tab, line feed and carriage return, or U+FFFE or
/// U+FFFF), or when an EnumParameter label is empty or contains whitespace (the labels are
/// written as one space separated list). For every project that serializes,
/// serialize(parseProject(serialize(p))) == serialize(p) holds.
[[nodiscard]] DAWPROJECT_EXPORT std::string serialize(const Project& project);

/// Serializes metadata to `metadata.xml` text. Throws like serialize(const Project&) on a
/// character XML cannot carry.
[[nodiscard]] DAWPROJECT_EXPORT std::string serialize(const MetaData& metadata);

/// Formats a double the way the serializer writes it: the shortest text that reads back to the
/// same value, independent of the C locale; infinities are written "inf" and "-inf".
[[nodiscard]] DAWPROJECT_EXPORT std::string formatDouble(double value);

/// Parses a double the way the parser reads it: decimal or exponent notation, "inf", "-inf",
/// "INF", "-INF", "Infinity", "-Infinity" and "NaN" (infinity spellings in any case). A number
/// too large for a double gives an infinity and one too small gives zero, like strtod. Returns
/// false when the text is not a number.
[[nodiscard]] DAWPROJECT_EXPORT bool parseDouble(std::string_view text, double& out) noexcept;

}  // namespace dawproject
