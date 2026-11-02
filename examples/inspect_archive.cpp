// SPDX-License-Identifier: MIT
// Writes a .dawproject with an embedded audio file and a plugin state, then lists the archive
// entries and extracts the embedded files.
#include <dawproject/dawproject.hpp>

#include <filesystem>
#include <iostream>

int main() {
  using namespace dawproject;
  try {
    const auto dir = std::filesystem::temp_directory_path() / "dawproject-inspect-example";
    std::filesystem::create_directories(dir);
    const auto file = dir / "embedded.dawproject";

    Project project;
    project.application = {"inspect_archive example", "1.0"};
    Track track;
    track.id = "vox";
    track.name = "Vocals";
    track.contentType = {ContentType::Audio};
    track.channel.emplace();
    auto plugin = std::make_unique<ClapPlugin>();
    plugin->deviceName = "Reverb";
    plugin->deviceRole = DeviceRole::AudioFX;
    plugin->state = FileReference{"plugins/reverb.clap-preset", std::nullopt};
    track.channel->devices.push_back(std::move(plugin));
    project.structure.emplace_back(std::move(track));

    const std::vector<std::uint8_t> silence(64, 0);
    const std::vector<std::uint8_t> state = {'s', 't', 'a', 't', 'e'};
    save(file, project, {}, {},
         {EmbeddedBytes{"audio/silence.raw", silence},
          EmbeddedBytes{"plugins/reverb.clap-preset", state}});

    {
      // The archive keeps its file open until it goes out of scope (Windows cannot delete an
      // open file, so the cleanup below runs after this block).
      const Archive archive = Archive::open(file);
      for (const std::string& entry : archive.entries()) {
        std::cout << entry << " (" << archive.read(entry).size() << " bytes)\n";
      }
      const auto out = dir / "extracted";
      for (const std::string& entry : archive.entries()) {
        if (entry != kProjectEntry && entry != kMetaDataEntry) {
          std::cout << "extracted " << archive.extract(entry, out).string() << "\n";
        }
      }
    }
    std::filesystem::remove_all(dir);
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << "\n";
    return 1;
  }
}
