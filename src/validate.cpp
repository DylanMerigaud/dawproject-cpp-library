// SPDX-License-Identifier: MIT
#include "dawproject/validate.hpp"

#include <map>
#include <string>
#include <utility>

namespace dawproject {
namespace {

// True for an xs:NCName, the lexical space of xs:ID and xs:IDREF: a letter or '_' followed by
// letters, digits, '.', '-' and '_', no ':' and no whitespace. Non ASCII UTF-8 bytes are
// accepted as letters.
bool isNCName(const std::string& text) {
  if (text.empty()) {
    return false;
  }
  const auto letter = [](unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c >= 0x80;
  };
  if (!letter(static_cast<unsigned char>(text.front()))) {
    return false;
  }
  for (const char ch : text) {
    const auto c = static_cast<unsigned char>(ch);
    if (!letter(c) && !(c >= '0' && c <= '9') && c != '.' && c != '-') {
      return false;
    }
  }
  return true;
}

// Walks the whole project once, recording ids, references and structural problems.
class Validator {
 public:
  std::vector<Issue> run(const Project& project) {
    if (project.transport) {
      const std::string path = "Transport";
      if (project.transport->tempo) {
        id(*project.transport->tempo, path + "/Tempo");
      }
      if (project.transport->timeSignature) {
        id(*project.transport->timeSignature, path + "/TimeSignature");
      }
    }
    for (std::size_t i = 0; i < project.structure.size(); ++i) {
      const std::string path = "Structure[" + std::to_string(i) + "]";
      if (const Track* track = std::get_if<Track>(&project.structure[i])) {
        visitTrack(*track, path + "/Track");
      } else {
        visitChannel(std::get<Channel>(project.structure[i]), path + "/Channel");
      }
    }
    if (project.arrangement) {
      const Arrangement& arrangement = *project.arrangement;
      const std::string path = "Arrangement";
      id(arrangement, path);
      if (arrangement.lanes) {
        visitTimeline(*arrangement.lanes, path + "/Lanes");
      }
      if (arrangement.markers) {
        visitTimeline(*arrangement.markers, path + "/Markers");
      }
      if (arrangement.tempoAutomation) {
        visitTimeline(*arrangement.tempoAutomation, path + "/TempoAutomation");
      }
      if (arrangement.timeSignatureAutomation) {
        visitTimeline(*arrangement.timeSignatureAutomation, path + "/TimeSignatureAutomation");
      }
    }
    for (std::size_t i = 0; i < project.scenes.size(); ++i) {
      const Scene& scene = project.scenes[i];
      const std::string path = "Scenes/Scene[" + std::to_string(i) + "]";
      id(scene, path);
      if (scene.content) {
        visitTimeline(*scene.content, path);
      } else {
        issue(path, "scene has no content");
      }
    }

    for (const auto& [target, where] : references_) {
      if (ids_.find(target.second) == ids_.end()) {
        issue(where, "'" + target.first + "' references unknown id '" + target.second + "'");
      }
    }
    return std::move(issues_);
  }

 private:
  void issue(const std::string& path, const std::string& message) {
    issues_.push_back(Issue{path, message});
  }

  void id(const Referenceable& value, const std::string& path) {
    if (!value.id) {
      return;
    }
    if (!isNCName(*value.id)) {
      issue(path, "'" + *value.id + "' is not a valid id (an XML NCName: a letter or '_' first, " +
                      "then letters, digits, '.', '-' or '_')");
    }
    const auto [it, inserted] = ids_.emplace(*value.id, path);
    if (!inserted) {
      issue(path, "duplicate id '" + *value.id + "', first used at " + it->second);
    }
  }

  void reference(const std::optional<std::string>& target, const char* attribute,
                 const std::string& path) {
    if (target && !isNCName(*target)) {
      issue(path, "'" + std::string(attribute) + "' value '" + *target +
                      "' is not a valid id (an XML NCName)");
    }
    if (target) {
      references_.emplace_back(std::make_pair(std::string(attribute), *target), path);
    }
  }

  void parameter(const std::optional<RealParameter>& value, const std::string& path) {
    if (value) {
      id(*value, path);
    }
  }

  void parameter(const std::optional<BoolParameter>& value, const std::string& path) {
    if (value) {
      id(*value, path);
    }
  }

  void visitDevice(const Device& device, const std::string& path) {
    id(device, path);
    for (std::size_t i = 0; i < device.parameters.size(); ++i) {
      if (device.parameters[i]) {
        const std::string parameterPath = path + "/Parameters[" + std::to_string(i) + "]";
        id(*device.parameters[i], parameterPath);
        if (const auto* e = dynamic_cast<const EnumParameter*>(device.parameters[i].get())) {
          labels(*e, parameterPath);
        }
      }
    }
    parameter(device.enabled, path + "/Enabled");
    if (const auto* eq = dynamic_cast<const Equalizer*>(&device)) {
      for (std::size_t i = 0; i < eq->bands.size(); ++i) {
        const std::string band = path + "/Band[" + std::to_string(i) + "]";
        id(eq->bands[i].freq, band + "/Freq");
        parameter(eq->bands[i].gain, band + "/Gain");
        parameter(eq->bands[i].q, band + "/Q");
        parameter(eq->bands[i].enabled, band + "/Enabled");
      }
      parameter(eq->inputGain, path + "/InputGain");
      parameter(eq->outputGain, path + "/OutputGain");
    } else if (const auto* c = dynamic_cast<const Compressor*>(&device)) {
      parameter(c->attack, path + "/Attack");
      parameter(c->autoMakeup, path + "/AutoMakeup");
      parameter(c->inputGain, path + "/InputGain");
      parameter(c->outputGain, path + "/OutputGain");
      parameter(c->ratio, path + "/Ratio");
      parameter(c->release, path + "/Release");
      parameter(c->threshold, path + "/Threshold");
    } else if (const auto* g = dynamic_cast<const NoiseGate*>(&device)) {
      parameter(g->attack, path + "/Attack");
      parameter(g->range, path + "/Range");
      parameter(g->ratio, path + "/Ratio");
      parameter(g->release, path + "/Release");
      parameter(g->threshold, path + "/Threshold");
    } else if (const auto* l = dynamic_cast<const Limiter*>(&device)) {
      parameter(l->attack, path + "/Attack");
      parameter(l->inputGain, path + "/InputGain");
      parameter(l->outputGain, path + "/OutputGain");
      parameter(l->release, path + "/Release");
      parameter(l->threshold, path + "/Threshold");
    }
  }

  // Labels are written as a space separated list, so they cannot be empty or hold whitespace.
  void labels(const EnumParameter& parameter, const std::string& path) {
    for (const std::string& label : parameter.labels) {
      if (label.empty()) {
        issue(path, "empty label: the space separated 'labels' list cannot hold it");
      } else if (label.find_first_of(" \t\n\r") != std::string::npos) {
        issue(path, "label '" + label + "' contains whitespace: the space separated 'labels' " +
                        "list cannot hold it");
      }
    }
  }

  void visitChannel(const Channel& channel, const std::string& path) {
    id(channel, path);
    reference(channel.destination, "destination", path);
    for (std::size_t i = 0; i < channel.devices.size(); ++i) {
      if (channel.devices[i]) {
        visitDevice(*channel.devices[i], path + "/Devices[" + std::to_string(i) + "]");
      }
    }
    parameter(channel.mute, path + "/Mute");
    parameter(channel.pan, path + "/Pan");
    parameter(channel.volume, path + "/Volume");
    for (std::size_t i = 0; i < channel.sends.size(); ++i) {
      const Send& send = channel.sends[i];
      const std::string sendPath = path + "/Sends/Send[" + std::to_string(i) + "]";
      id(send, sendPath);
      reference(send.destination, "destination", sendPath);
      parameter(send.enable, sendPath + "/Enable");
      parameter(send.pan, sendPath + "/Pan");
      id(send.volume, sendPath + "/Volume");
    }
  }

  void visitTrack(const Track& track, const std::string& path) {
    id(track, path);
    if (track.channel) {
      visitChannel(*track.channel, path + "/Channel");
    }
    for (std::size_t i = 0; i < track.tracks.size(); ++i) {
      visitTrack(track.tracks[i], path + "/Track[" + std::to_string(i) + "]");
    }
  }

  void visitClip(const Clip& clip, const std::string& path) {
    reference(clip.reference, "reference", path);
    if (clip.content) {
      visitTimeline(*clip.content, path + "/Content");
    }
  }

  void visitTimeline(const Timeline& timeline, const std::string& path) {
    id(timeline, path);
    reference(timeline.track, "track", path);
    switch (timeline.kind()) {
      case TimelineKind::Lanes: {
        const auto& lanes = static_cast<const Lanes&>(timeline);
        for (std::size_t i = 0; i < lanes.lanes.size(); ++i) {
          if (lanes.lanes[i]) {
            visitTimeline(*lanes.lanes[i], path + "/Lane[" + std::to_string(i) + "]");
          }
        }
        break;
      }
      case TimelineKind::Notes: {
        const auto& notes = static_cast<const Notes&>(timeline);
        for (std::size_t i = 0; i < notes.notes.size(); ++i) {
          if (notes.notes[i].content) {
            visitTimeline(*notes.notes[i].content,
                          path + "/Note[" + std::to_string(i) + "]/Content");
          }
        }
        break;
      }
      case TimelineKind::Clips: {
        const auto& clips = static_cast<const Clips&>(timeline);
        for (std::size_t i = 0; i < clips.clips.size(); ++i) {
          visitClip(clips.clips[i], path + "/Clip[" + std::to_string(i) + "]");
        }
        break;
      }
      case TimelineKind::ClipSlot: {
        const auto& slot = static_cast<const ClipSlot&>(timeline);
        if (slot.clip) {
          visitClip(*slot.clip, path + "/Clip");
        }
        break;
      }
      case TimelineKind::Markers:
        if (static_cast<const Markers&>(timeline).markers.empty()) {
          issue(path, "markers timeline has no marker");
        }
        break;
      case TimelineKind::Warps: {
        const auto& warps = static_cast<const Warps&>(timeline);
        if (warps.content) {
          visitTimeline(*warps.content, path + "/Content");
        } else {
          issue(path, "warps timeline has no content");
        }
        if (warps.warps.empty()) {
          issue(path, "warps timeline has no warp point");
        }
        break;
      }
      case TimelineKind::Points:
        reference(static_cast<const Points&>(timeline).target.parameter, "parameter",
                  path + "/Target");
        break;
      case TimelineKind::Audio:
      case TimelineKind::Video:
        break;
    }
  }

  std::map<std::string, std::string> ids_;
  // ((attribute, target id), path of the referencing element)
  std::vector<std::pair<std::pair<std::string, std::string>, std::string>> references_;
  std::vector<Issue> issues_;
};

}  // namespace

std::vector<Issue> validate(const Project& project) { return Validator().run(project); }

std::string IdGenerator::next() { return "id" + std::to_string(counter_++); }

}  // namespace dawproject
