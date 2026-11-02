# dawproject-cpp-library: implementation plan

Goal: a MIT C++17 library that reads and writes DAWproject files (the open DAW exchange format
published by Bitwig, https://github.com/bitwig/dawproject, format version 1.0), usable from any
CMake project and from JUCE projects, with no dependency the user has to install.

Why it is needed: the maintained DAWproject libraries are Java (bitwig/dawproject), Python
(roex-audio/dawproject-py), TypeScript (@volter/dawproject, @opendaw/lib-dawproject) and Rust (the
dawproject crate). C++ DAWs hand-write their own code (Tracktion Engine's dawproject module,
Microsoft's MIDI sequencer DawProject.cpp) and LMMS has an open request for it (LMMS issue 6909).
The only C++ attempt found, zarfld/dawproject_c, has no licence and no release.

## 1. Sources of truth

- `spec/Project.xsd`, `spec/MetaData.xsd`: copied from bitwig/dawproject at commit
  ee4dcdde75940f30e14e55401a26955a58b8322b (MIT, `spec/LICENSE.bitwig`).
- The Java reference implementation (read for behaviour only, never copied):
  `src/main/java/com/bitwig/dawproject/*.java` in that repository, in particular
  `DoubleAdapter.java` (doubles accept `inf`, `-inf`, written as `inf`) and
  `DawProject.java` (zip layout: `project.xml` and `metadata.xml` at the archive root, embedded
  files at the path named by a `FileReference` with `external="false"`).
- XML reference: https://htmlpreview.github.io/?https://github.com/bitwig/dawproject/blob/main/Reference.html

## 2. Layout

```
CMakeLists.txt                 project(dawproject VERSION 0.1.0 LANGUAGES CXX), C++17
cmake/dawprojectConfig.cmake.in
include/dawproject/dawproject.hpp   umbrella header (includes everything below)
include/dawproject/model.hpp        the typed object model
include/dawproject/enums.hpp        enums + to_string / from_string
include/dawproject/xml.hpp          parse / serialize project.xml and metadata.xml
include/dawproject/archive.hpp      the .dawproject zip container
include/dawproject/validate.hpp     reference and id checks
include/dawproject/error.hpp        dawproject::Error
include/dawproject/version.hpp      DAWPROJECT_VERSION_* macros, format version "1.0"
src/*.cpp
third_party/pugixml/  (MIT, v1.14, pugixml.hpp pugiconfig.hpp pugixml.cpp LICENSE.md)
third_party/miniz/    (MIT, 3.0.2 amalgamated miniz.h miniz.c LICENSE)
third_party/doctest/  (MIT, 2.4.11 doctest.h LICENSE.txt), tests only
tests/                doctest unit and contract tests, tests/data fixtures, tests/xsd_validate.py
examples/             write_project.cpp, read_project.cpp, inspect_archive.cpp
.github/workflows/ci.yml, .clang-format, .clang-tidy, README.md, CHANGELOG.md, LICENSE,
CONTRIBUTING.md, .github/ISSUE_TEMPLATE/{bug_report.md,feature_request.md}, docs/
```

The third party sources are compiled INTO the library as private implementation details
(their symbols are not exported by the headers; pugixml and miniz headers are never included from
`include/`). Option `DAWPROJECT_USE_SYSTEM_PUGIXML` (OFF) lets a packager use a system pugixml.

CMake targets: `dawproject` with alias `dawproject::dawproject`; options `DAWPROJECT_BUILD_TESTS`
and `DAWPROJECT_BUILD_EXAMPLES` default ON only when it is the top level project;
`DAWPROJECT_INSTALL` default ON when top level. Install exports `dawproject::dawproject` and a
`dawprojectConfig.cmake` + version file so `find_package(dawproject CONFIG REQUIRED)` works. Works
with `add_subdirectory` and `FetchContent` (the JUCE path: JUCE projects use CMake and link
`dawproject::dawproject` next to `juce::juce_core`).

## 3. Public API (namespace `dawproject`)

### 3.1 Model (`model.hpp`), mirrors the XSD

- Base fields: `Nameable { std::optional<std::string> name, color, comment; }`,
  `Referenceable : Nameable { std::optional<std::string> id; }`.
- Parameters: `Parameter : Referenceable { std::optional<int> parameterID; virtual kind }`,
  `RealParameter { std::optional<double> value, min, max; Unit unit; }`,
  `BoolParameter { std::optional<bool> value; }`, `IntegerParameter { optional<int> value,min,max }`,
  `EnumParameter { optional<int> value; int count; std::vector<std::string> labels; }`,
  `TimeSignatureParameter { int numerator; int denominator; }`.
  Fixed slots (Channel volume, Transport tempo, ...) hold the concrete type by value in
  `std::optional<...>`. Heterogeneous lists (Device Parameters) hold
  `std::vector<std::unique_ptr<Parameter>>`.
- Timelines: abstract `Timeline : Referenceable { std::optional<TimeUnit> timeUnit;
  std::optional<std::string> track; virtual TimelineKind kind() const; virtual
  std::unique_ptr<Timeline> clone() const; }` with `Lanes { std::vector<std::unique_ptr<Timeline>>
  lanes; }`, `Notes { std::vector<Note> notes; }`, `Clips { std::vector<Clip> clips; }`,
  `ClipSlot { std::optional<Clip> clip; std::optional<bool> hasStop; }`,
  `Markers { std::vector<Marker> markers; }`, `Warps { std::unique_ptr<Timeline> content;
  std::vector<Warp> warps; TimeUnit contentTimeUnit; }`, `Audio`, `Video` (MediaFile: `FileReference
  file; double duration; int channels; int sampleRate; std::optional<std::string> algorithm;`),
  `Points { AutomationTarget target; std::vector<Point> points; std::optional<Unit> unit; }`
  where `Point = std::variant<RealPoint, EnumPoint, BoolPoint, IntegerPoint, TimeSignaturePoint>`.
  `Note { double time, duration; int channel, key; std::optional<double> vel, rel;
  std::unique_ptr<Timeline> content; }` (note expressions). `Clip : Nameable { double time;
  optional<double> duration, playStart, playStop, loopStart, loopEnd, fadeInTime, fadeOutTime;
  optional<TimeUnit> contentTimeUnit, fadeTimeUnit; optional<bool> enable; optional<string>
  reference; std::unique_ptr<Timeline> content; }`.
  Helper: `template <class T> T* as(Timeline*)` / const overload (dynamic_cast) and
  `TimelineKind` enum.
- Structure: `Track : Referenceable { std::vector<ContentType> contentType; std::optional<bool>
  loaded; std::optional<Channel> channel; std::vector<Track> tracks; }`, `Channel : Referenceable
  { std::optional<MixerRole> role; std::optional<int> audioChannels; std::optional<std::string>
  destination; std::optional<bool> solo; std::vector<std::unique_ptr<Device>> devices;
  std::optional<BoolParameter> mute; std::optional<RealParameter> pan, volume; std::vector<Send>
  sends; }`, `using StructureItem = std::variant<Track, Channel>`.
- Devices: abstract `Device : Referenceable { std::string deviceName; DeviceRole deviceRole;
  optional<string> deviceID, deviceVendor; optional<bool> loaded; std::vector<unique_ptr<Parameter>>
  parameters; optional<BoolParameter> enabled; optional<FileReference> state; virtual DeviceKind
  kind() const; virtual unique_ptr<Device> clone() const; }`; `GenericDevice` (element `Device`),
  `Plugin` (pluginVersion) -> `Vst2Plugin`, `Vst3Plugin`, `ClapPlugin`, `AuPlugin`;
  `BuiltinDevice` -> `Equalizer { vector<EqBand> bands; optional<RealParameter> inputGain,
  outputGain; }`, `Compressor`, `NoiseGate`, `Limiter` with their XSD children.
- `Arrangement : Referenceable { std::unique_ptr<Lanes> lanes; std::optional<Markers> markers;
  std::optional<Points> tempoAutomation, timeSignatureAutomation; }`, `Scene : Referenceable {
  std::unique_ptr<Timeline> content; }`.
- `Project { std::string version = "1.0"; Application application; std::optional<Transport>
  transport; std::vector<StructureItem> structure; std::optional<Arrangement> arrangement;
  std::vector<Scene> scenes; }`; `MetaData` with the 13 optional strings of MetaData.xsd.
- Types holding `unique_ptr` are move-only; every polymorphic type has `clone()`; `Project` has
  `Project clone() const`.

### 3.2 Enums (`enums.hpp`)

`Unit, TimeUnit, DeviceRole, EqBandType, MixerRole, SendType, ContentType, Interpolation,
ExpressionType` with `std::string_view to_string(E)` and `std::optional<E> from_string<E>(
std::string_view)` using the exact XSD spellings.

### 3.3 XML (`xml.hpp`)

```cpp
struct ParseOptions { bool strict = false; };  // strict: unknown element/attribute or enum throws
Project parseProject(std::string_view xml, const ParseOptions& = {});
MetaData parseMetaData(std::string_view xml, const ParseOptions& = {});
std::string serialize(const Project&);
std::string serialize(const MetaData&);
```
Lenient mode ignores unknown elements and attributes (forward compatibility), strict mode throws
`Error` naming the element path. Missing required attributes throw in both modes. Element order
written follows the XSD sequence. Doubles: parse accepts `inf`, `-inf`, `Infinity`; written with
the shortest representation that round-trips (`std::to_chars` when available, else `%.17g`
trimmed), locale independent, infinities written `INF` / `-INF` in xs:double attributes and
`inf` / `-inf` in xs:string ones. Output: UTF-8,
`<?xml version="1.0" encoding="UTF-8" standalone="yes"?>`, two-space indent.
Invariant: `serialize(parseProject(serialize(p))) == serialize(p)`.

### 3.4 Archive (`archive.hpp`)

```cpp
class Archive {  // read side, lazy
 public:
  static Archive open(const std::filesystem::path&);
  Project project(const ParseOptions& = {}) const;
  MetaData metadata(const ParseOptions& = {}) const;  // empty MetaData when absent
  std::vector<std::string> entries() const;
  bool contains(std::string_view) const;
  std::vector<std::uint8_t> read(std::string_view entry) const;
  std::filesystem::path extract(std::string_view entry, const std::filesystem::path& dest) const;
};
struct EmbeddedFile { std::string archivePath; std::filesystem::path source; };
struct EmbeddedBytes { std::string archivePath; std::vector<std::uint8_t> bytes; };
void save(const std::filesystem::path&, const Project&, const MetaData& = {},
          const std::vector<EmbeddedFile>& = {}, const std::vector<EmbeddedBytes>& = {});
struct Document { Project project; MetaData metadata; };
Document load(const std::filesystem::path&, const ParseOptions& = {});
```
Zip entries `project.xml` and `metadata.xml` at the root, deflate. Streamed through file
callbacks: no archive or embedded file is held in memory whole; `save` writes a temporary sibling
file and renames it over the target. `extract` refuses an entry name
that escapes the destination (zip slip). Missing `project.xml` throws `Error`.

### 3.5 Validation (`validate.hpp`)

`std::vector<Issue> validate(const Project&)`: duplicate ids, IDREFs (`Timeline.track`,
`Channel.destination`, `Send.destination`, `AutomationTarget.parameter`, `Clip.reference`) that
name no id, Markers without a marker, Warps with no content. `Issue { std::string path; std::string
message; }`. Plus `IdGenerator` (`id0`, `id1`, ... like the Java library) for writers.

### 3.6 Errors

`class Error : public std::runtime_error` with `ErrorKind kind()` (`Io, Zip, Xml, Schema`).

## 4. Tests (doctest, run by ctest)

1. Enum spellings: every XSD enumeration value maps both ways (list read from spec/Project.xsd by
   the test at runtime, so a new XSD value fails it).
2. Port of the Java `DawProjectTest` scenarios, rebuilt in C++ (empty project, a project with
   tracks, channels, devices incl. a Vst3Plugin with state, notes with expressions, audio clips
   with warps, automation Points, markers, scenes with clip slots): serialize, re-parse, assert
   field values and the round trip invariant.
3. Schema contract: `tests/xsd_validate.py` (Python + lxml) validates every XML the tests write
   (they are dumped to `${CMAKE_BINARY_DIR}/xsd-out/`) against spec/Project.xsd and
   spec/MetaData.xsd; registered as a ctest that depends on the unit test run; skipped with a
   message (ctest SKIP_RETURN_CODE 77) when lxml is missing; CI installs lxml so it always runs
   there.
4. Fixtures `tests/data/*.xml` written by hand from the spec in the shape DAW exports take
   (a Bitwig-like full project, a minimal project, a project with unknown future elements):
   parsed values asserted, lenient mode accepts unknown elements, strict mode rejects them.
5. Archive: save with embedded audio and plugin state, open, entries, read, extract; a zip made by
   Python's zipfile (tests/data/python-made.dawproject) opens; zip slip refused; missing
   project.xml throws.
6. Numbers: `inf`, `-inf`, `0.1`, `1e-300`, round trip of 1000 random doubles exact.
7. validate(): each issue kind detected, a clean project has none.
8. Examples are built and run by ctest (`example_*` tests).

## 5. Quality bar and how this plan meets it

- Installable: `cmake --install` to a prefix, then a fresh consumer project
  `find_package(dawproject CONFIG REQUIRED)` + `target_link_libraries(app dawproject::dawproject)`
  compiles and runs. The same consumer also works through `add_subdirectory` and `FetchContent`.
- README programs work: every README ```cpp block is a complete program (`#include
  <dawproject/dawproject.hpp>` + `int main`) that compiles against the library and exits 0, run
  in an empty directory (it writes its own files). ctest builds and runs each block.
- Tests: `ctest` passes, including the XSD validation of every written XML.
- Headers: the public C++ headers under include/ are installed, each compiles on its own, every
  public function is typed, and only the API marked `DAWPROJECT_EXPORT` leaves a shared build.
- Examples: every examples/*.cpp builds and exits 0 with no arguments.
- CI on Linux, macOS and Windows; a `.clang-format` checked in CI and a `.clang-tidy`.
- README: opens with what it is and for whom (C++ DAWs, plugin hosts, converters), gives a first
  result in minutes (FetchContent snippet + a short program), and compares the existing options
  (Java reference, Python, TypeScript, Rust, Tracktion's built-in module, zarfld/dawproject_c),
  saying why this one for C++. It names the integration paths (a small C++ DAW or tool, a JUCE
  app, a converter) without claiming any project uses it.

## 6. CI

`.github/workflows/ci.yml`: matrix ubuntu-latest (gcc, clang), macos-latest, windows-latest; steps
configure, build, ctest (which also builds and runs the README blocks), `pip install lxml` so the
XSD check always runs. On ubuntu: a `find_package` consumer after `cmake --install`, an
`add_subdirectory` consumer, a `FetchContent` consumer pointed at the checkout, a C++20 build
(`-DCMAKE_CXX_STANDARD=20`), a shared library build (`-DBUILD_SHARED_LIBS=ON`) with its tests, and
`clang-format --dry-run --Werror`. Warnings: `-Wall -Wextra -Wpedantic` (`/W4` on MSVC) on the
library target, warnings as errors in CI.

## 7. Done criteria

- `cmake -S . -B build && cmake --build build -j2 && ctest --test-dir build --output-on-failure`
  passes locally (gcc 13), every test listed in section 4 present.
- The same passes with `-DCMAKE_CXX_STANDARD=20` and with `-DBUILD_SHARED_LIBS=ON`.
- XSD validation test ran (not skipped) locally.
- Install + external `find_package` consumer builds and runs.
- README blocks compile and run; examples run.
- No unprefixed miniz symbol in the built library
  (`nm -g --defined-only libdawproject.a | grep -E ' (mz_|tdefl_|tinfl_)'` prints nothing).
