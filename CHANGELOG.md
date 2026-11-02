# Changelog

All notable changes to this project are documented in this file. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.1.0] - 2026-11-02

### Added

- Typed C++17 model of DAWproject 1.0 (`Project.xsd` and `MetaData.xsd` at bitwig/dawproject
  commit ee4dcdde75940f30e14e55401a26955a58b8322b): parameters, tracks, channels, sends, devices
  and plugins, clips, notes and note expressions, audio, video, warps, automation points,
  markers, scenes and clip slots, transport, metadata. Deep copies through `clone()`; the
  polymorphic bases (`Parameter`, `Timeline`, `Device`) cannot be copied or moved by value, so
  they cannot be sliced.
- Enumerations with their exact XML spellings: `to_string`, `from_string`, `all_values`.
- `parseProject`, `parseMetaData` and `serialize`. Lenient parsing (the default) skips unknown
  elements, unknown attributes and unknown values of optional enumeration attributes; strict
  parsing rejects those, repeated single elements and misnamed markers elements. Nesting deeper
  than 256 elements throws. `serialize` refuses characters XML cannot carry and enum labels the
  space separated list cannot hold.
- Doubles written in the shortest round-trip form, independent of the C locale. Infinities
  written `INF` / `-INF` in xs:double attributes and `inf` / `-inf` in xs:string attributes;
  every spelling read; out of range numbers saturate to infinity or zero.
- The `.dawproject` zip container: `Archive` (open, project, metadata, entries, contains, read,
  extract with zip slip refusal), `save` with embedded files and bytes, `load`. Archives and
  embedded files are streamed from and to disk; `save` replaces the target only once the new
  archive is complete.
- `validate()` for duplicate ids, ids and references that are not NCNames, dangling
  references, empty markers, warps and scenes, and enum labels that cannot be written;
  `IdGenerator`.
- `Error` with `ErrorKind` (`Io`, `Zip`, `Xml`, `Schema`).
- CMake package `dawproject::dawproject` usable through `find_package`, `add_subdirectory` and
  `FetchContent`, as C++17 or a newer standard, static or shared (hidden visibility, only the
  public API exported). pugixml and miniz compiled in, pugixml under a private namespace and
  every miniz function under a `dawproject_` prefix.
- Tests (doctest) including validation of every written XML against the official XSD, every
  README program built and run, and three examples run by ctest.

[Unreleased]: https://github.com/DylanMerigaud/dawproject-cpp-library/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/DylanMerigaud/dawproject-cpp-library/releases/tag/v0.1.0
