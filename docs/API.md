# API reference

Everything is in namespace `dawproject`. Include `<dawproject/dawproject.hpp>` for the whole API,
or the single headers below. Every failing call throws `dawproject::Error`.

## `dawproject/model.hpp`: the object model

Each type mirrors a complex type of `spec/Project.xsd` or `spec/MetaData.xsd`. Optional XML
attributes and elements are `std::optional` members; required ones are plain members with a
default. Types that own polymorphic children through `std::unique_ptr` are move-only and have a
`clone()` that makes a deep copy.

| Group | Types |
| --- | --- |
| Base fields | `Nameable` (`name`, `color`, `comment`), `Referenceable : Nameable` (`id`), `FileReference` (`path`, `external`) |
| Parameters | abstract `Parameter` (`parameterID`, `kind()`, `clone()`); `RealParameter` (`value`, `min`, `max`, `unit`), `BoolParameter`, `IntegerParameter`, `EnumParameter` (`count`, `labels`), `TimeSignatureParameter` (`numerator`, `denominator`); `ParameterKind` |
| Timelines | abstract `Timeline` (`timeUnit`, `track`, `kind()`, `clone()`); `Lanes`, `Notes`, `Clips`, `ClipSlot`, `Markers`, `Warps`, `Audio`, `Video` (through `MediaFile`: `file`, `duration`, `channels`, `sampleRate`, `algorithm`), `Points`; `TimelineKind` |
| Timeline contents | `Note` (`time`, `duration`, `channel`, `key`, `vel`, `rel`, `content` for expressions), `Clip` (timing, loop, fades, `enable`, `reference`, `content`), `Marker`, `Warp` |
| Automation | `RealPoint`, `EnumPoint`, `BoolPoint`, `IntegerPoint`, `TimeSignaturePoint`, `using Point = std::variant<...>`, `AutomationTarget` (`parameter`, `expression`, `channel`, `key`, `controller`) |
| Devices | abstract `Device` (`deviceName`, `deviceRole`, `deviceID`, `deviceVendor`, `loaded`, `parameters`, `enabled`, `state`); `GenericDevice`; `Plugin` (`pluginVersion`) with `Vst2Plugin`, `Vst3Plugin`, `ClapPlugin`, `AuPlugin`; `BuiltinDevice` with `Equalizer` (`EqBand`), `Compressor`, `NoiseGate`, `Limiter`; `DeviceKind` |
| Structure | `Channel` (`role`, `audioChannels`, `destination`, `solo`, `devices`, `mute`, `pan`, `sends`, `volume`), `Send`, `Track` (`contentType`, `loaded`, `channel`, child `tracks`), `using StructureItem = std::variant<Track, Channel>` |
| Project | `Project` (`version`, `application`, `transport`, `structure`, `arrangement`, `scenes`), `Application`, `Transport` (`tempo`, `timeSignature`), `Arrangement` (`lanes`, `markers`, `tempoAutomation`, `timeSignatureAutomation`), `Scene` (`content`) |
| Metadata | `MetaData`: 13 optional strings (`title`, `artist`, `album`, ... `comment`) |

```cpp
template <class T> T* as(Timeline* timeline) noexcept;              // dynamic_cast, nullptr when another type
template <class T> const T* as(const Timeline* timeline) noexcept;
```

## `dawproject/enums.hpp`: enumerations

`Unit`, `TimeUnit`, `DeviceRole`, `EqBandType`, `MixerRole`, `SendType`, `ContentType`,
`Interpolation`, `ExpressionType`.

```cpp
std::string_view to_string(E value) noexcept;                // exact XML spelling, e.g. "audioFX"
template <class E> std::optional<E> from_string(std::string_view text) noexcept;
template <class E> const std::vector<E>& all_values();       // schema order
```

## `dawproject/xml.hpp`: project.xml and metadata.xml

```cpp
struct ParseOptions { bool strict = false; };
Project parseProject(std::string_view xml, const ParseOptions& options = {});
MetaData parseMetaData(std::string_view xml, const ParseOptions& options = {});
std::string serialize(const Project& project);
std::string serialize(const MetaData& metadata);
std::string formatDouble(double value);
bool parseDouble(std::string_view text, double& out) noexcept;
```

Lenient parsing (the default) skips unknown elements and attributes and drops an unknown value of
an optional enumeration attribute; strict parsing throws `ErrorKind::Schema` for each of them, for
a repeated element the schema allows once, and for `<Markers>` used as a timeline (the schema
says `<markers>` there). A missing required attribute and an unknown value of a required
enumeration attribute throw in both modes, as does nesting deeper than 256 elements.

`serialize` writes UTF-8 with a two space indent, elements in schema order. Infinities are `INF`
/ `-INF` in xs:double attributes (Clip times, Marker time, Warp times, Audio and Video duration)
and `inf` / `-inf` in xs:string ones (parameter values, Note and Point fields). It throws
`ErrorKind::Schema` naming the field for a control character other than tab, line feed and
carriage return (or U+FFFE, U+FFFF) and for an EnumParameter label that is empty or contains
whitespace. For a project that serializes, `serialize(parseProject(serialize(p))) ==
serialize(p)`. `parseDouble` saturates out of range numbers to infinity or zero.

## `dawproject/archive.hpp`: the .dawproject container

```cpp
inline constexpr std::string_view kProjectEntry = "project.xml";
inline constexpr std::string_view kMetaDataEntry = "metadata.xml";

class Archive {
 public:
  static Archive open(const std::filesystem::path& path);
  Project project(const ParseOptions& options = {}) const;
  MetaData metadata(const ParseOptions& options = {}) const;  // empty when absent
  std::vector<std::string> entries() const;
  bool contains(std::string_view entry) const;
  std::vector<std::uint8_t> read(std::string_view entry) const;
  std::filesystem::path extract(std::string_view entry,
                                const std::filesystem::path& destinationDir) const;
};

struct EmbeddedFile { std::string archivePath; std::filesystem::path source; };
struct EmbeddedBytes { std::string archivePath; std::vector<std::uint8_t> bytes; };

void save(const std::filesystem::path& path, const Project& project, const MetaData& metadata = {},
          const std::vector<EmbeddedFile>& files = {},
          const std::vector<EmbeddedBytes>& bytes = {});

struct Document { Project project; MetaData metadata; };
Document load(const std::filesystem::path& path, const ParseOptions& options = {});
```

`extract` refuses an absolute entry name or one that climbs out with `..`, and streams the entry
to the file. `save` refuses an archive path that is empty, absolute, contains `..` or is used
twice, compresses embedded files straight from disk, and writes a temporary file next to `path`
that replaces it only once complete.

## `dawproject/validate.hpp`: consistency checks

```cpp
struct Issue { std::string path; std::string message; };
std::vector<Issue> validate(const Project& project);

class IdGenerator {
 public:
  std::string next();      // "id0", "id1", ...
  void reset() noexcept;
};
```

`validate` reports duplicate ids, ids and references that are not XML NCNames, references
(`Timeline::track`, `Channel::destination`, `Send::destination`, `AutomationTarget::parameter`,
`Clip::reference`) naming no id, `Markers` without a marker, `Warps` without content or warp
points, `Scene`s without content, and EnumParameter labels that are empty or contain whitespace.

## `dawproject/error.hpp`: errors

```cpp
enum class ErrorKind { Io, Zip, Xml, Schema };
class Error : public std::runtime_error {
 public:
  Error(ErrorKind kind, const std::string& message);
  ErrorKind kind() const noexcept;
};
```

## `dawproject/version.hpp`: versions

Macros `DAWPROJECT_VERSION_MAJOR`, `DAWPROJECT_VERSION_MINOR`, `DAWPROJECT_VERSION_PATCH`,
`DAWPROJECT_VERSION_STRING` (`"0.1.0"`), `DAWPROJECT_FORMAT_VERSION` (`"1.0"`), and the constants
`kLibraryVersion` and `kFormatVersion`.
