# dawproject-cpp-library

A C++17 library that reads and writes DAWproject files, the open project exchange format
published by Bitwig. It is for C++ DAWs, plugin hosts, converters and tools that import or export
`.dawproject` files, including JUCE apps.

[![CI](https://github.com/DylanMerigaud/dawproject-cpp-library/actions/workflows/ci.yml/badge.svg)](https://github.com/DylanMerigaud/dawproject-cpp-library/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

## Why this library

| Option | Language | Difference from this library |
| --- | --- | --- |
| [bitwig/dawproject](https://github.com/bitwig/dawproject) | Java | The reference implementation. Calling it from a C++ program means embedding a JVM. |
| [roex-audio/dawproject-py](https://github.com/roex-audio/dawproject-py) | Python | Needs a Python interpreter at run time. |
| `@volter/dawproject`, `@opendaw/lib-dawproject` | TypeScript | Run on a JavaScript runtime, not linkable into a C++ binary. |
| `dawproject` crate | Rust | Needs a Rust toolchain and a C ABI layer to call from C++. |
| [zarfld/dawproject_c](https://github.com/zarfld/dawproject_c) | C++ | No licence file, no release, last commit 2025-12-04. |
| Tracktion Engine's dawproject module, Microsoft's MIDI sequencer `DawProject.cpp` | C++ | Code written inside one engine or app, not a standalone library you can link on its own. |
| **dawproject-cpp-library** | C++17 | MIT, a CMake target with no dependency to install (pugixml and miniz are compiled in, pugixml under a private namespace and miniz under prefixed names), a typed model of the whole `Project.xsd`, every XML the tests write validated against the official XSD. |

## Features

- The `.dawproject` zip container: `project.xml` and `metadata.xml` at the root, embedded files
  (audio, plugin states) at any path. Archives are streamed: opening reads only the zip
  directory, entries are decompressed on demand, extraction and embedding go file to file
  without holding a whole entry in memory, and extraction refuses entry names escaping the
  destination directory. `save()` writes a temporary file and renames it over the target, so a
  failed save leaves the previous file intact.
- `project.xml` and `metadata.xml` parsed into, and written from, plain C++ structs.
- Typed model of `Project.xsd`: tracks (including folder tracks), channels with volume, pan, mute,
  sends and device chains, clips, notes with per-note expressions, audio and video with warps,
  automation points (real, enum, bool, integer, time signature), devices (VST2, VST3, CLAP, AU,
  built-in equalizer, compressor, noise gate, limiter) with plugin state file references, scenes
  and clip slots, cue markers, transport tempo and time signature.
- Lenient parsing by default (unknown elements, unknown attributes and unknown values of optional
  enumeration attributes are skipped, so files from a newer format version still load) and
  strict parsing that throws on those, on repeated single elements and on misnamed markers
  elements. Nesting deeper than 256 elements throws instead of exhausting the stack.
- `validate()`: duplicate ids, ids and references that are not XML NCNames, references (`track`,
  `destination`, `parameter`, `reference`) to ids that do not exist, empty markers, warps or
  scenes, enum labels the space separated list cannot hold.
- Doubles written in the shortest form that reads back to the same value, independent of the C
  locale. Infinities are written `INF` and `-INF` in the attributes the XSD types `xs:double`,
  and `inf` and `-inf` in the `xs:string` ones like the Java reference; all spellings are read.
- `serialize()` refuses strings XML cannot carry (control characters other than tab, line feed
  and carriage return) instead of writing a file no parser accepts.

## Install

Requirements: a C++17 compiler and CMake 3.16 or newer. Nothing else to install.

### FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
  dawproject
  GIT_REPOSITORY https://github.com/DylanMerigaud/dawproject-cpp-library.git
  GIT_TAG v0.1.0
)
FetchContent_MakeAvailable(dawproject)

add_executable(app main.cpp)
target_link_libraries(app PRIVATE dawproject::dawproject)
```

### find_package after an install

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix /path/to/prefix
```

```cmake
find_package(dawproject CONFIG REQUIRED)
target_link_libraries(app PRIVATE dawproject::dawproject)
```

Configure the consuming project with `-DCMAKE_PREFIX_PATH=/path/to/prefix`.

### add_subdirectory

```cmake
add_subdirectory(third_party/dawproject-cpp-library)
target_link_libraries(app PRIVATE dawproject::dawproject)
```

Tests and examples are only built when the library is the top level project.

### JUCE

JUCE projects built with CMake link the target next to the JUCE modules:

```cmake
juce_add_gui_app(MyApp PRODUCT_NAME "My App")
target_sources(MyApp PRIVATE Main.cpp)
target_link_libraries(MyApp PRIVATE dawproject::dawproject juce::juce_gui_extra)
```

The same line works for a target made with `juce_add_plugin`. The bundled pugixml lives in a
private namespace, every miniz function carries a `dawproject_` prefix and miniz is built without
zlib compatible names, so neither clashes with a pugixml, miniz or zlib the program links on its
own. Built as a shared library (`-DBUILD_SHARED_LIBS=ON`), the library is compiled with hidden
visibility and exports only its public API, marked `DAWPROJECT_EXPORT`.

## Quickstart

This program writes a project with a tempo and one instrument track holding three notes, saves it
as `hello.dawproject` in the current directory, loads it back and prints what it finds.

```cpp
#include <dawproject/dawproject.hpp>

#include <iostream>
#include <memory>

int main() {
  using namespace dawproject;

  Project project;
  project.application = {"My App", "1.0"};
  project.transport.emplace();
  project.transport->tempo = RealParameter(Unit::Bpm, 120.0);
  project.transport->timeSignature = TimeSignatureParameter(4, 4);

  Track piano;
  piano.id = "piano";
  piano.name = "Piano";
  piano.contentType = {ContentType::Notes};
  piano.channel.emplace();
  piano.channel->id = "piano-channel";
  piano.channel->role = MixerRole::Regular;
  piano.channel->volume = RealParameter(Unit::Linear, 0.8);
  project.structure.emplace_back(std::move(piano));

  auto notes = std::make_unique<Notes>();
  for (int key : {60, 64, 67}) {
    Note note;
    note.time = (key - 60) / 4.0;
    note.duration = 1.0;
    note.key = key;
    note.vel = 0.8;
    notes->notes.push_back(std::move(note));
  }
  Clip clip;
  clip.name = "Chord";
  clip.time = 0.0;
  clip.duration = 4.0;
  clip.content = std::move(notes);

  auto clips = std::make_unique<Clips>();
  clips->track = "piano";
  clips->clips.push_back(std::move(clip));
  project.arrangement.emplace();
  project.arrangement->lanes = std::make_unique<Lanes>();
  project.arrangement->lanes->timeUnit = TimeUnit::Beats;
  project.arrangement->lanes->lanes.push_back(std::move(clips));

  save("hello.dawproject", project);

  const Document loaded = load("hello.dawproject");
  const Track& track = std::get<Track>(loaded.project.structure.at(0));
  const auto* lane = as<Clips>(loaded.project.arrangement->lanes->lanes.at(0).get());
  const auto* chord = as<Notes>(lane->clips.at(0).content.get());
  std::cout << "tempo " << *loaded.project.transport->tempo->value << " bpm, track "
            << *track.name << ", " << chord->notes.size() << " notes\n";
  return 0;
}
```

It prints `tempo 120 bpm, track Piano, 3 notes`.

## Usage

### Read a file and walk its tracks and clips

`Archive::open` reads the zip lazily; `load()` is the one call shortcut that returns the project
and its metadata. Timelines are polymorphic: `as<T>()` returns the concrete type or `nullptr`.

```cpp
#include <dawproject/dawproject.hpp>

#include <iostream>
#include <memory>
#include <string>

using namespace dawproject;

// Writes a small file so the program has something to read.
void writeSample(const std::string& path) {
  Project project;
  project.application = {"sample", "1.0"};
  Track bass;
  bass.id = "bass";
  bass.name = "Bass";
  bass.contentType = {ContentType::Notes};
  project.structure.emplace_back(std::move(bass));
  auto notes = std::make_unique<Notes>();
  Note note;
  note.key = 36;
  note.duration = 2.0;
  notes->notes.push_back(std::move(note));
  Clip clip;
  clip.name = "Riff";
  clip.duration = 2.0;
  clip.content = std::move(notes);
  auto clips = std::make_unique<Clips>();
  clips->track = "bass";
  clips->clips.push_back(std::move(clip));
  project.arrangement.emplace();
  project.arrangement->lanes = std::make_unique<Lanes>();
  project.arrangement->lanes->lanes.push_back(std::move(clips));
  MetaData metadata;
  metadata.title = "Sample song";
  save(path, project, metadata);
}

void walk(const Timeline& timeline, int depth) {
  const std::string indent(static_cast<std::size_t>(depth) * 2, ' ');
  if (const auto* lanes = as<Lanes>(&timeline)) {
    for (const auto& lane : lanes->lanes) walk(*lane, depth);
  } else if (const auto* clips = as<Clips>(&timeline)) {
    for (const Clip& clip : clips->clips) {
      std::cout << indent << "clip " << clip.name.value_or("") << " at " << clip.time
                << " on track " << clips->track.value_or("?") << "\n";
      if (clip.content) walk(*clip.content, depth + 1);
    }
  } else if (const auto* notes = as<Notes>(&timeline)) {
    for (const Note& note : notes->notes) {
      std::cout << indent << "note " << note.key << " for " << note.duration << "\n";
    }
  }
}

int main() {
  writeSample("sample.dawproject");

  const Archive archive = Archive::open("sample.dawproject");
  for (const std::string& entry : archive.entries()) std::cout << "entry " << entry << "\n";
  std::cout << "title " << archive.metadata().title.value_or("(none)") << "\n";

  const Project project = archive.project();
  for (const StructureItem& item : project.structure) {
    if (const Track* track = std::get_if<Track>(&item)) {
      std::cout << "track " << track->name.value_or("") << "\n";
    }
  }
  if (project.arrangement && project.arrangement->lanes) walk(*project.arrangement->lanes, 0);
  return 0;
}
```

### Automation points and note expressions

A `Points` lane targets a parameter by its id, or an expression. Points are a
`std::variant` of `RealPoint`, `EnumPoint`, `BoolPoint`, `IntegerPoint` and
`TimeSignaturePoint`. Tempo automation has its own slot on the arrangement.

```cpp
#include <dawproject/dawproject.hpp>

#include <iostream>
#include <memory>

using namespace dawproject;

RealPoint point(double time, double value) {
  RealPoint p;
  p.time = time;
  p.value = value;
  p.interpolation = Interpolation::Linear;
  return p;
}

int main() {
  Project project;
  project.application = {"automation", "1.0"};

  Track synth;
  synth.id = "synth";
  synth.contentType = {ContentType::Notes, ContentType::Automation};
  synth.channel.emplace();
  synth.channel->id = "synth-channel";
  synth.channel->volume = RealParameter(Unit::Linear, 1.0);
  synth.channel->volume->id = "synth-volume";
  project.structure.emplace_back(std::move(synth));

  // A volume fade on the track's channel fader.
  auto fade = std::make_unique<Points>();
  fade->track = "synth";
  fade->target.parameter = "synth-volume";
  fade->unit = Unit::Linear;
  fade->points = {point(0.0, 1.0), point(8.0, 0.0)};

  // A note with a pitch bend of two semitones over its length.
  auto bend = std::make_unique<Points>();
  bend->target.expression = ExpressionType::Transpose;
  bend->points = {point(0.0, 0.0), point(1.0, 2.0)};
  auto expressions = std::make_unique<Lanes>();
  expressions->lanes.push_back(std::move(bend));
  Note note;
  note.duration = 1.0;
  note.content = std::move(expressions);
  auto notes = std::make_unique<Notes>();
  notes->track = "synth";
  notes->notes.push_back(std::move(note));

  project.arrangement.emplace();
  project.arrangement->lanes = std::make_unique<Lanes>();
  project.arrangement->lanes->timeUnit = TimeUnit::Beats;
  project.arrangement->lanes->lanes.push_back(std::move(fade));
  project.arrangement->lanes->lanes.push_back(std::move(notes));

  // Tempo going from 120 to 140 bpm over eight beats.
  project.arrangement->tempoAutomation.emplace();
  project.arrangement->tempoAutomation->unit = Unit::Bpm;
  project.arrangement->tempoAutomation->points = {point(0.0, 120.0), point(8.0, 140.0)};

  if (!validate(project).empty()) return 1;
  const Project parsed = parseProject(serialize(project));
  const auto* lane = as<Points>(parsed.arrangement->lanes->lanes.at(0).get());
  const auto& last = std::get<RealPoint>(lane->points.back());
  std::cout << "fade ends at " << last.value << " on beat " << last.time << "\n";
  return 0;
}
```

### Embed an audio file and a plugin state

`save()` takes files to copy from disk (`EmbeddedFile`) and bytes already in memory
(`EmbeddedBytes`). The model points at them through a `FileReference` with the same path.

```cpp
#include <dawproject/dawproject.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

using namespace dawproject;

int main() {
  // A stand-in for a real recording on disk.
  { std::ofstream("loop.wav", std::ios::binary) << "RIFF....WAVE"; }
  // A stand-in for the state a plugin returned.
  const std::vector<std::uint8_t> reverbState = {0x56, 0x53, 0x54, 0x33};

  Project project;
  project.application = {"embedding", "1.0"};

  Track track;
  track.id = "drums";
  track.contentType = {ContentType::Audio};
  track.channel.emplace();
  track.channel->id = "drums-channel";
  auto reverb = std::make_unique<Vst3Plugin>();
  reverb->deviceName = "Reverb";
  reverb->deviceRole = DeviceRole::AudioFX;
  reverb->state = FileReference{"plugins/reverb.vstpreset", std::nullopt};
  track.channel->devices.push_back(std::move(reverb));
  project.structure.emplace_back(std::move(track));

  auto audio = std::make_unique<Audio>();
  audio->file = FileReference{"audio/loop.wav", false};
  audio->channels = 2;
  audio->sampleRate = 48000;
  audio->duration = 2.0;
  Clip clip;
  clip.duration = 4.0;
  clip.content = std::move(audio);
  auto clips = std::make_unique<Clips>();
  clips->track = "drums";
  clips->clips.push_back(std::move(clip));
  project.arrangement.emplace();
  project.arrangement->lanes = std::make_unique<Lanes>();
  project.arrangement->lanes->lanes.push_back(std::move(clips));

  save("drums.dawproject", project, MetaData{}, {EmbeddedFile{"audio/loop.wav", "loop.wav"}},
       {EmbeddedBytes{"plugins/reverb.vstpreset", reverbState}});

  const Archive archive = Archive::open("drums.dawproject");
  std::cout << "has audio: " << archive.contains("audio/loop.wav") << "\n";
  std::cout << "state bytes: " << archive.read("plugins/reverb.vstpreset").size() << "\n";
  const std::filesystem::path out = archive.extract("audio/loop.wav", "extracted");
  std::cout << "extracted to " << out.string() << "\n";
  return 0;
}
```

### Check a project with validate()

The XSD cannot check that a reference names an existing id. `validate()` does, and returns one
`Issue` per problem with its location. `IdGenerator` hands out `id0`, `id1`, ... for writers.

```cpp
#include <dawproject/dawproject.hpp>

#include <iostream>

using namespace dawproject;

int main() {
  IdGenerator ids;
  Project project;
  project.application = {"validate", "1.0"};

  Track track;
  track.id = ids.next();
  track.channel.emplace();
  track.channel->id = ids.next();
  track.channel->destination = "master";  // no element has this id
  project.structure.emplace_back(std::move(track));

  const std::vector<Issue> issues = validate(project);
  for (const Issue& issue : issues) {
    std::cout << issue.path << ": " << issue.message << "\n";
  }
  return issues.size() == 1 ? 0 : 1;
}
```

### Lenient and strict parsing

By default the parser skips elements and attributes it does not know and drops an unknown value
of an optional enumeration attribute, so a file from a newer format version still loads.
`ParseOptions{true}` turns each of those into an `Error` of kind `ErrorKind::Schema` naming the
element path, and also rejects a repeated element the schema allows once and `<Markers>` used
where the schema says `<markers>`. Missing required attributes, and unknown values of required
enumeration attributes (a RealParameter `unit`, a Device `deviceRole`), throw in both modes.

```cpp
#include <dawproject/dawproject.hpp>

#include <iostream>

using namespace dawproject;

int main() {
  const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Project version="1.0">
  <Application name="Some DAW" version="2.0" buildNumber="42"/>
  <Transport>
    <Tempo unit="bpm" value="98.5"/>
  </Transport>
</Project>)";

  const Project lenient = parseProject(xml);
  std::cout << "lenient: tempo " << *lenient.transport->tempo->value << "\n";

  ParseOptions strict;
  strict.strict = true;
  try {
    (void)parseProject(xml, strict);
    return 1;
  } catch (const Error& error) {
    std::cout << "strict: " << error.what() << "\n";
    return error.kind() == ErrorKind::Schema ? 0 : 1;
  }
}
```

`Archive::project()`, `Archive::metadata()` and `load()` take the same `ParseOptions`.

## Format coverage and spec version

The library implements DAWproject format version 1.0 as defined by `Project.xsd` and
`MetaData.xsd` at bitwig/dawproject commit `ee4dcdde75940f30e14e55401a26955a58b8322b`. Both files
are kept in [`spec/`](spec). Every complex type of both schemas has a C++ type (see
[docs/API.md](docs/API.md)), and every enumeration value maps both ways with its exact XML
spelling. Elements are written in the order of the schema's sequences. The `version` attribute
written is `kFormatVersion` (`"1.0"`).

## Building and running the tests

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

The tests use doctest (vendored under `third_party/doctest`). They cover enum spellings read from
the XSD at run time, the scenarios of the Java reference rebuilt in C++, hand-written fixtures in
`tests/data`, a zip made by Python's `zipfile`, zip slip refusal, number formatting and every
`validate()` check. The `xsd_validate` test checks every XML file the unit tests wrote against
`spec/Project.xsd` and `spec/MetaData.xsd` with Python and lxml (`pip install lxml`); it is
skipped when lxml is missing. The examples in `examples/` are also run by ctest, and so is every
`cpp` block of this README: each one is extracted at configure time, built against the library
and run in an empty directory (tests `readme.block1` and up).

The library builds as C++17 or any newer standard the including project sets
(`-DCMAKE_CXX_STANDARD=20` is checked in CI), and as a static or shared library
(`-DBUILD_SHARED_LIBS=ON`).

CMake options: `DAWPROJECT_BUILD_TESTS`, `DAWPROJECT_BUILD_EXAMPLES`, `DAWPROJECT_INSTALL` (on
when top level), `DAWPROJECT_USE_SYSTEM_PUGIXML` and `DAWPROJECT_WARNINGS_AS_ERRORS` (off).

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). A file that does not load is the most useful report:
attach the `.dawproject` or its `project.xml`.

## License

MIT, see [LICENSE](LICENSE). The schema files under `spec/` are Bitwig's, also under the MIT
licence ([spec/LICENSE.bitwig](spec/LICENSE.bitwig)). The vendored pugixml and miniz keep their
own MIT licences in `third_party/`.
