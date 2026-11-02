// SPDX-License-Identifier: MIT
// Shared helpers for the test suites.
#pragma once

#include <dawproject/dawproject.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <string_view>

namespace testing {

/// Directory holding spec/Project.xsd and spec/MetaData.xsd.
inline std::filesystem::path specDir() { return DAWPROJECT_SPEC_DIR; }

/// Directory holding the hand-written fixtures.
inline std::filesystem::path dataDir() { return DAWPROJECT_TEST_DATA_DIR; }

/// Reads a whole file as text.
inline std::string readText(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

/// Writes an XML document the tests produced to the directory checked by tests/xsd_validate.py.
inline void dumpForSchemaCheck(const std::string& name, const std::string& xml) {
  const std::filesystem::path dir = DAWPROJECT_XSD_OUT_DIR;
  std::filesystem::create_directories(dir);
  std::ofstream(dir / (name + ".xml"), std::ios::binary) << xml;
}

/// A fresh, empty directory removed when the object goes out of scope.
class TempDir {
 public:
  TempDir() {
    static std::atomic<unsigned> counter{0};
    std::random_device rd;
    path_ = std::filesystem::temp_directory_path() /
            ("dawproject-test-" + std::to_string(rd()) + "-" + std::to_string(counter++));
    std::filesystem::create_directories(path_);
  }
  ~TempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }
  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;

  [[nodiscard]] const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

/// Serializes, re-parses and serializes again; checks the round trip invariant and returns the
/// re-parsed project. Every scenario also lands in the schema check directory.
inline dawproject::Project roundTrip(const dawproject::Project& project, const std::string& name) {
  const std::string first = dawproject::serialize(project);
  dumpForSchemaCheck(name, first);
  dawproject::ParseOptions strict;
  strict.strict = true;
  dawproject::Project reparsed = dawproject::parseProject(first, strict);
  const std::string second = dawproject::serialize(reparsed);
  if (first != second) {
    std::ofstream(std::filesystem::path(DAWPROJECT_XSD_OUT_DIR) / (name + ".mismatch.txt"))
        << first << "\n----\n"
        << second;
  }
  REQUIRE(first == second);
  return reparsed;
}

}  // namespace testing
