// SPDX-License-Identifier: MIT
// Builds a small project (two tracks, a MIDI clip, a tempo) and saves it as a .dawproject file
// in a temporary directory.
#include <dawproject/dawproject.hpp>

#include <filesystem>
#include <iostream>

int main() {
  using namespace dawproject;
  try {
    IdGenerator ids;
    Project project;
    project.application = {"write_project example", "1.0"};
    project.transport.emplace();
    project.transport->tempo = RealParameter(Unit::Bpm, 120.0);
    project.transport->timeSignature = TimeSignatureParameter(4, 4);

    Track master;
    master.id = ids.next();
    master.name = "Master";
    master.channel.emplace();
    master.channel->id = ids.next();
    master.channel->role = MixerRole::Master;
    master.channel->volume = RealParameter(Unit::Linear, 1.0);

    Track piano;
    piano.id = ids.next();
    piano.name = "Piano";
    piano.contentType = {ContentType::Notes};
    piano.channel.emplace();
    piano.channel->id = ids.next();
    piano.channel->destination = master.channel->id;
    piano.channel->volume = RealParameter(Unit::Linear, 0.8);

    auto notes = std::make_unique<Notes>();
    for (const int key : {60, 64, 67, 72}) {
      Note note;
      note.time = (key - 60) / 4.0;
      note.duration = 1.0;
      note.key = key;
      note.vel = 0.8;
      notes->notes.push_back(std::move(note));
    }
    Clip clip;
    clip.name = "C major";
    clip.time = 0.0;
    clip.duration = 4.0;
    clip.content = std::move(notes);
    auto clips = std::make_unique<Clips>();
    clips->track = piano.id;
    clips->clips.push_back(std::move(clip));

    project.arrangement.emplace();
    project.arrangement->lanes = std::make_unique<Lanes>();
    project.arrangement->lanes->timeUnit = TimeUnit::Beats;
    project.arrangement->lanes->lanes.push_back(std::move(clips));
    project.structure.emplace_back(std::move(master));
    project.structure.emplace_back(std::move(piano));

    if (const auto issues = validate(project); !issues.empty()) {
      for (const Issue& issue : issues) {
        std::cerr << issue.path << ": " << issue.message << "\n";
      }
      return 1;
    }

    MetaData metadata;
    metadata.title = "Example song";
    metadata.artist = "dawproject";

    const auto dir = std::filesystem::temp_directory_path() / "dawproject-write-example";
    std::filesystem::create_directories(dir);
    const auto file = dir / "example.dawproject";
    save(file, project, metadata);
    std::cout << "wrote " << file.string() << " (" << std::filesystem::file_size(file)
              << " bytes)\n";
    std::filesystem::remove_all(dir);
    return 0;
  } catch (const Error& error) {
    std::cerr << "error: " << error.what() << "\n";
    return 1;
  }
}
