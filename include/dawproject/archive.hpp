// SPDX-License-Identifier: MIT
/// @file archive.hpp
/// The `.dawproject` zip container: `project.xml`, `metadata.xml` and embedded files.
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "dawproject/export.hpp"
#include "dawproject/model.hpp"
#include "dawproject/xml.hpp"

namespace dawproject {

/// Name of the project entry at the root of the archive.
inline constexpr std::string_view kProjectEntry = "project.xml";
/// Name of the metadata entry at the root of the archive.
inline constexpr std::string_view kMetaDataEntry = "metadata.xml";

/// Read access to a `.dawproject` file. Opening reads only the zip directory; the file stays open
/// while any copy of the Archive exists and entries are decompressed on demand. Copies share the
/// same opened archive.
class DAWPROJECT_EXPORT Archive {
 public:
  /// Opens an archive. Throws Error (ErrorKind::Io) when the file cannot be read and
  /// Error (ErrorKind::Zip) when it is not a zip file.
  [[nodiscard]] static Archive open(const std::filesystem::path& path);

  /// Parses `project.xml`. Throws Error (ErrorKind::Zip) when the entry is missing.
  [[nodiscard]] Project project(const ParseOptions& options = {}) const;

  /// Parses `metadata.xml`; an empty MetaData when the entry is absent.
  [[nodiscard]] MetaData metadata(const ParseOptions& options = {}) const;

  /// Names of every file entry, in archive order.
  [[nodiscard]] std::vector<std::string> entries() const;

  /// True when a file entry has exactly this name.
  [[nodiscard]] bool contains(std::string_view entry) const;

  /// Decompressed content of an entry. Throws Error (ErrorKind::Zip) when it is missing.
  [[nodiscard]] std::vector<std::uint8_t> read(std::string_view entry) const;

  /// Writes an entry to `destinationDir / entry`, creating directories; the entry is decompressed
  /// straight to the file, never held in memory as a whole. Throws Error
  /// (ErrorKind::Zip) when the entry name is absolute or climbs out of the destination with
  /// "..", and Error (ErrorKind::Io) when the file cannot be written. Returns the written path.
  std::filesystem::path extract(std::string_view entry,
                                const std::filesystem::path& destinationDir) const;

 private:
  struct Impl;
  explicit Archive(std::shared_ptr<const Impl> impl);
  std::shared_ptr<const Impl> impl_;
};

/// A file on disk stored in the archive at `archivePath` (for example "audio/kick.wav").
struct EmbeddedFile {
  std::string archivePath;       ///< Path inside the archive, '/' separated.
  std::filesystem::path source;  ///< File to copy into the archive.
};

/// Bytes stored in the archive at `archivePath`.
struct EmbeddedBytes {
  std::string archivePath;          ///< Path inside the archive, '/' separated.
  std::vector<std::uint8_t> bytes;  ///< Content.
};

/// Writes a `.dawproject` file, replacing any existing file. Embedded files are compressed
/// straight from their source file. The archive is written to a temporary file next to `path`
/// and renamed over it once complete, so on failure an existing file at `path` is left as it
/// was. Throws Error (ErrorKind::Io) when a file cannot be read or written, Error
/// (ErrorKind::Zip) when an archive path is empty, absolute, contains "..", or is used twice,
/// and Error (ErrorKind::Schema) when the project or metadata cannot be serialized (see
/// serialize()).
DAWPROJECT_EXPORT void save(const std::filesystem::path& path, const Project& project,
                            const MetaData& metadata = {},
                            const std::vector<EmbeddedFile>& files = {},
                            const std::vector<EmbeddedBytes>& bytes = {});

/// A loaded project with its metadata.
struct Document {
  Project project;    ///< Content of `project.xml`.
  MetaData metadata;  ///< Content of `metadata.xml`, empty when absent.
};

/// Opens a `.dawproject` file and parses both documents.
[[nodiscard]] DAWPROJECT_EXPORT Document load(const std::filesystem::path& path,
                                              const ParseOptions& options = {});

}  // namespace dawproject
