// SPDX-License-Identifier: MIT
#include <algorithm>
#include <functional>

#include "doctest.h"
#include "test_support.hpp"

using namespace dawproject;

namespace {

Project sampleProject() {
  Project project;
  project.application = {"Archive Test", "1.0"};
  Track track;
  track.id = "track";
  track.name = "Audio";
  track.contentType = {ContentType::Audio};
  track.channel.emplace();
  track.channel->id = "channel";
  auto plugin = std::make_unique<Vst3Plugin>();
  plugin->deviceName = "Reverb";
  plugin->state = FileReference{"plugins/reverb.vstpreset", std::nullopt};
  track.channel->devices.push_back(std::move(plugin));
  project.structure.emplace_back(std::move(track));
  project.arrangement.emplace();
  project.arrangement->lanes = std::make_unique<Lanes>();
  auto clips = std::make_unique<Clips>();
  clips->track = "track";
  Clip clip;
  clip.duration = 1.0;
  auto audio = std::make_unique<Audio>();
  audio->file = FileReference{"audio/loop.wav", false};
  audio->channels = 2;
  audio->sampleRate = 48000;
  audio->duration = 1.0;
  clip.content = std::move(audio);
  clips->clips.push_back(std::move(clip));
  project.arrangement->lanes->lanes.push_back(std::move(clips));
  return project;
}

ErrorKind errorKindOf(const std::function<void()>& action) {
  try {
    action();
  } catch (const Error& error) {
    return error.kind();
  }
  FAIL("no error thrown");
  return ErrorKind::Io;
}

}  // namespace

TEST_SUITE("archive") {
  TEST_CASE("save with embedded audio and plugin state, then open, read and extract") {
    testing::TempDir dir;
    const std::filesystem::path audioSource = dir.path() / "loop.wav";
    const std::string audioBytes = std::string("RIFF") + std::string(1000, '\x01');
    std::ofstream(audioSource, std::ios::binary) << audioBytes;

    MetaData metadata;
    metadata.title = "Archive";
    metadata.artist = "Tester";
    const std::vector<std::uint8_t> state = {0x56, 0x53, 0x54, 0x33, 0x00, 0xff};
    const std::filesystem::path file = dir.path() / "test.dawproject";
    save(file, sampleProject(), metadata, {EmbeddedFile{"audio/loop.wav", audioSource}},
         {EmbeddedBytes{"plugins/reverb.vstpreset", state}});
    REQUIRE(std::filesystem::exists(file));

    const Archive archive = Archive::open(file);
    const std::vector<std::string> entries = archive.entries();
    CHECK(entries == std::vector<std::string>{"project.xml", "metadata.xml", "audio/loop.wav",
                                              "plugins/reverb.vstpreset"});
    CHECK(archive.contains("project.xml"));
    CHECK(archive.contains("audio/loop.wav"));
    CHECK_FALSE(archive.contains("audio"));
    CHECK_FALSE(archive.contains("Project.xml"));
    CHECK(archive.read("plugins/reverb.vstpreset") == state);
    const std::vector<std::uint8_t> audio = archive.read("audio/loop.wav");
    CHECK(std::string(audio.begin(), audio.end()) == audioBytes);

    const Project project = archive.project();
    CHECK(project.application.name == "Archive Test");
    CHECK(archive.metadata().title == "Archive");

    const std::vector<std::uint8_t> xml = archive.read("project.xml");
    testing::dumpForSchemaCheck("archive-project", std::string(xml.begin(), xml.end()));
    const std::vector<std::uint8_t> meta = archive.read("metadata.xml");
    testing::dumpForSchemaCheck("archive-metadata", std::string(meta.begin(), meta.end()));

    const std::filesystem::path out = dir.path() / "extracted";
    const std::filesystem::path written = archive.extract("audio/loop.wav", out);
    CHECK(written == (out / "audio" / "loop.wav").lexically_normal());
    CHECK(testing::readText(written) == audioBytes);

    const Document document = load(file);
    CHECK(document.metadata.artist == "Tester");
    CHECK(serialize(document.project) == serialize(sampleProject()));

    // Copies share the opened archive.
    const Archive copy = archive;
    CHECK(copy.entries().size() == 4);
  }

  TEST_CASE("a zip written by Python's zipfile opens") {
    const Archive archive = Archive::open(testing::dataDir() / "python-made.dawproject");
    CHECK(archive.entries() ==
          std::vector<std::string>{"project.xml", "metadata.xml", "audio/take1.wav"});
    ParseOptions strict;
    strict.strict = true;
    const Project project = archive.project(strict);
    CHECK(project.application.name == "Python zipfile");
    CHECK(std::get<Track>(project.structure.at(0)).name == "Vocals");
    CHECK(archive.metadata().title == "Zipped by Python");
    const std::vector<std::uint8_t> wav = archive.read("audio/take1.wav");
    REQUIRE(wav.size() == 44 + 160);
    CHECK(std::string(wav.begin(), wav.begin() + 4) == "RIFF");
    testing::roundTrip(project, "archive-python-made");
  }

  TEST_CASE("zip slip is refused") {
    testing::TempDir dir;
    const Archive archive = Archive::open(testing::dataDir() / "zip-slip.dawproject");
    CHECK(archive.contains("../escaped.txt"));
    const std::filesystem::path out = dir.path() / "out";
    CHECK(errorKindOf([&] { archive.extract("../escaped.txt", out); }) == ErrorKind::Zip);
    CHECK_FALSE(std::filesystem::exists(dir.path() / "escaped.txt"));
    CHECK(errorKindOf([&] { archive.extract("/etc/passwd", out); }) == ErrorKind::Zip);
    CHECK(errorKindOf([&] { archive.extract("a/../../b", out); }) == ErrorKind::Zip);
    CHECK(errorKindOf([&] { archive.extract("C:/windows", out); }) == ErrorKind::Zip);
    CHECK(errorKindOf([&] { archive.extract("..\\escaped.txt", out); }) == ErrorKind::Zip);
    // A safe entry still extracts.
    CHECK(std::filesystem::exists(archive.extract("project.xml", out)));
  }

  TEST_CASE("missing project.xml throws") {
    const Archive archive = Archive::open(testing::dataDir() / "no-project.dawproject");
    CHECK(archive.metadata().year == "2026");
    CHECK(errorKindOf([&] { (void)archive.project(); }) == ErrorKind::Zip);
    CHECK(errorKindOf([&] { (void)load(testing::dataDir() / "no-project.dawproject"); }) ==
          ErrorKind::Zip);
  }

  TEST_CASE("metadata.xml is optional when reading") {
    const Archive archive = Archive::open(testing::dataDir() / "zip-slip.dawproject");
    const MetaData metadata = archive.metadata();
    CHECK_FALSE(metadata.title.has_value());
  }

  TEST_CASE("I/O, zip and path errors") {
    testing::TempDir dir;
    CHECK(errorKindOf([&] { (void)Archive::open(dir.path() / "missing.dawproject"); }) ==
          ErrorKind::Io);
    const std::filesystem::path notZip = dir.path() / "not-a-zip.dawproject";
    std::ofstream(notZip) << "this is not a zip file";
    CHECK(errorKindOf([&] { (void)Archive::open(notZip); }) == ErrorKind::Zip);

    const Archive archive = Archive::open(testing::dataDir() / "python-made.dawproject");
    CHECK(errorKindOf([&] { (void)archive.read("nothing.bin"); }) == ErrorKind::Zip);

    const std::filesystem::path out = dir.path() / "out.dawproject";
    const Project project = sampleProject();
    CHECK(errorKindOf([&] { save(out, project, {}, {}, {EmbeddedBytes{"project.xml", {1}}}); }) ==
          ErrorKind::Zip);
    CHECK(errorKindOf([&] { save(out, project, {}, {}, {EmbeddedBytes{"../x", {1}}}); }) ==
          ErrorKind::Zip);
    CHECK(errorKindOf([&] { save(out, project, {}, {}, {EmbeddedBytes{"", {1}}}); }) ==
          ErrorKind::Zip);
    CHECK(errorKindOf([&] {
            save(out, project, {}, {EmbeddedFile{"audio/a.wav", dir.path() / "absent.wav"}});
          }) == ErrorKind::Io);
    CHECK(errorKindOf([&] { save(dir.path() / "no" / "such" / "dir.dawproject", project); }) ==
          ErrorKind::Io);
  }

  TEST_CASE("extract into the current directory and into a relative directory") {
    testing::TempDir dir;
    const Archive archive = Archive::open(testing::dataDir() / "python-made.dawproject");
    const std::filesystem::path previous = std::filesystem::current_path();
    std::filesystem::current_path(dir.path());
    struct Restore {
      std::filesystem::path path;
      ~Restore() { std::filesystem::current_path(path); }
    } restore{previous};

    const std::filesystem::path here = archive.extract("audio/take1.wav", ".");
    CHECK(std::filesystem::file_size(dir.path() / "audio" / "take1.wav") == 44 + 160);
    CHECK(std::filesystem::equivalent(here, dir.path() / "audio" / "take1.wav"));

    const std::filesystem::path relative = archive.extract("project.xml", "out/nested");
    CHECK(std::filesystem::exists(dir.path() / "out" / "nested" / "project.xml"));
    CHECK(std::filesystem::equivalent(relative, dir.path() / "out" / "nested" / "project.xml"));

    CHECK(archive.extract("project.xml", "./x/../y").filename() == "project.xml");
    CHECK(std::filesystem::exists(dir.path() / "y" / "project.xml"));
    CHECK(errorKindOf([&] { archive.extract("../escaped.txt", "."); }) == ErrorKind::Zip);
  }

  TEST_CASE("a large embedded file streams through save, read and extract") {
    testing::TempDir dir;
    const std::filesystem::path big = dir.path() / "big.bin";
    constexpr std::uintmax_t kSize = 64u * 1024u * 1024u;
    std::ofstream(big, std::ios::binary).close();
    std::filesystem::resize_file(big, kSize);  // sparse zeros, nothing to commit

    const std::filesystem::path file = dir.path() / "big.dawproject";
    save(file, sampleProject(), {}, {EmbeddedFile{"audio/big.bin", big}});
    CHECK(std::filesystem::file_size(file) < kSize / 100);

    const Archive archive = Archive::open(file);
    const std::filesystem::path extracted = archive.extract("audio/big.bin", dir.path() / "out");
    CHECK(std::filesystem::file_size(extracted) == kSize);
    std::ifstream in(extracted, std::ios::binary);
    std::vector<char> chunk(1 << 20);
    bool zeros = true;
    while (in.read(chunk.data(), static_cast<std::streamsize>(chunk.size())) || in.gcount() > 0) {
      const auto n = static_cast<std::size_t>(in.gcount());
      zeros = zeros && std::all_of(chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(n),
                                   [](char c) { return c == 0; });
    }
    CHECK(zeros);
    const std::vector<std::uint8_t> bytes = archive.read("audio/big.bin");
    CHECK(bytes.size() == kSize);
  }

  TEST_CASE("a failing save leaves the existing file untouched") {
    testing::TempDir dir;
    const std::filesystem::path file = dir.path() / "keep.dawproject";
    save(file, sampleProject());
    const std::string before = testing::readText(file);

    CHECK(errorKindOf([&] {
            save(file, sampleProject(), {},
                 {EmbeddedFile{"audio/a.wav", dir.path() / "absent.wav"}});
          }) == ErrorKind::Io);
    CHECK(testing::readText(file) == before);
    CHECK(errorKindOf([&] { save(file, sampleProject(), {}, {}, {EmbeddedBytes{"../x", {1}}}); }) ==
          ErrorKind::Zip);
    CHECK(testing::readText(file) == before);
    // No temporary file is left next to the target.
    std::size_t count = 0;
    for ([[maybe_unused]] const auto& entry : std::filesystem::directory_iterator(dir.path())) {
      ++count;
    }
    CHECK(count == 1);
    CHECK(Archive::open(file).project().application.name == "Archive Test");
  }
}
