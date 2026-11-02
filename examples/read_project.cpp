// SPDX-License-Identifier: MIT
// Saves a project, loads it back and prints its tracks, clips and notes.
#include <dawproject/dawproject.hpp>

#include <filesystem>
#include <iostream>

namespace {

void printTimeline(const dawproject::Timeline& timeline, int depth) {
  using namespace dawproject;
  const std::string indent(static_cast<std::size_t>(depth) * 2, ' ');
  if (const auto* lanes = as<Lanes>(&timeline)) {
    std::cout << indent << "Lanes (" << lanes->lanes.size() << ")\n";
    for (const auto& lane : lanes->lanes) {
      printTimeline(*lane, depth + 1);
    }
  } else if (const auto* clips = as<Clips>(&timeline)) {
    for (const Clip& clip : clips->clips) {
      std::cout << indent << "Clip '" << clip.name.value_or("") << "' at " << clip.time
                << " on track " << clips->track.value_or("?") << "\n";
      if (clip.content) {
        printTimeline(*clip.content, depth + 1);
      }
    }
  } else if (const auto* notes = as<Notes>(&timeline)) {
    for (const Note& note : notes->notes) {
      std::cout << indent << "Note key " << note.key << " at " << note.time << " for "
                << note.duration << "\n";
    }
  }
}

}  // namespace

int main() {
  using namespace dawproject;
  try {
    const auto dir = std::filesystem::temp_directory_path() / "dawproject-read-example";
    std::filesystem::create_directories(dir);
    const auto file = dir / "song.dawproject";

    {
      Project project;
      project.application = {"read_project example", "1.0"};
      Track track;
      track.id = "bass";
      track.name = "Bass";
      track.contentType = {ContentType::Notes};
      project.structure.emplace_back(std::move(track));
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
      metadata.title = "Read me";
      save(file, project, metadata);
    }

    const Document document = load(file);
    std::cout << "Title: " << document.metadata.title.value_or("(none)") << "\n";
    std::cout << "Written by " << document.project.application.name << "\n";
    for (const StructureItem& item : document.project.structure) {
      if (const Track* track = std::get_if<Track>(&item)) {
        std::cout << "Track '" << track->name.value_or("") << "'\n";
      }
    }
    if (document.project.arrangement && document.project.arrangement->lanes) {
      printTimeline(*document.project.arrangement->lanes, 0);
    }
    std::filesystem::remove_all(dir);
    return 0;
  } catch (const Error& error) {
    std::cerr << "error: " << error.what() << "\n";
    return 1;
  }
}
