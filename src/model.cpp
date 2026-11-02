// SPDX-License-Identifier: MIT
#include "dawproject/model.hpp"

#include <utility>

namespace dawproject {
namespace {

std::unique_ptr<Timeline> cloneTimeline(const std::unique_ptr<Timeline>& timeline) {
  return timeline ? timeline->clone() : nullptr;
}

}  // namespace

// Parameters ---------------------------------------------------------------------------------

std::unique_ptr<Parameter> RealParameter::clone() const {
  return std::make_unique<RealParameter>(*this);
}

std::unique_ptr<Parameter> BoolParameter::clone() const {
  return std::make_unique<BoolParameter>(*this);
}

std::unique_ptr<Parameter> IntegerParameter::clone() const {
  return std::make_unique<IntegerParameter>(*this);
}

std::unique_ptr<Parameter> EnumParameter::clone() const {
  return std::make_unique<EnumParameter>(*this);
}

std::unique_ptr<Parameter> TimeSignatureParameter::clone() const {
  return std::make_unique<TimeSignatureParameter>(*this);
}

// Timelines ----------------------------------------------------------------------------------

Lanes Lanes::cloneLanes() const {
  Lanes copy;
  copyTimelineFieldsTo(copy);
  copy.lanes.reserve(lanes.size());
  for (const auto& lane : lanes) {
    copy.lanes.push_back(cloneTimeline(lane));
  }
  return copy;
}

std::unique_ptr<Timeline> Lanes::clone() const { return std::make_unique<Lanes>(cloneLanes()); }

Note Note::clone() const {
  Note copy;
  copy.time = time;
  copy.duration = duration;
  copy.channel = channel;
  copy.key = key;
  copy.vel = vel;
  copy.rel = rel;
  copy.content = cloneTimeline(content);
  return copy;
}

std::unique_ptr<Timeline> Notes::clone() const {
  auto copy = std::make_unique<Notes>();
  copyTimelineFieldsTo(*copy);
  copy->notes.reserve(notes.size());
  for (const Note& note : notes) {
    copy->notes.push_back(note.clone());
  }
  return copy;
}

Clip Clip::clone() const {
  Clip copy;
  static_cast<Nameable&>(copy) = static_cast<const Nameable&>(*this);
  copy.time = time;
  copy.duration = duration;
  copy.contentTimeUnit = contentTimeUnit;
  copy.playStart = playStart;
  copy.playStop = playStop;
  copy.loopStart = loopStart;
  copy.loopEnd = loopEnd;
  copy.fadeTimeUnit = fadeTimeUnit;
  copy.fadeInTime = fadeInTime;
  copy.fadeOutTime = fadeOutTime;
  copy.enable = enable;
  copy.reference = reference;
  copy.content = cloneTimeline(content);
  return copy;
}

std::unique_ptr<Timeline> Clips::clone() const {
  auto copy = std::make_unique<Clips>();
  copyTimelineFieldsTo(*copy);
  copy->clips.reserve(clips.size());
  for (const Clip& clip : clips) {
    copy->clips.push_back(clip.clone());
  }
  return copy;
}

std::unique_ptr<Timeline> ClipSlot::clone() const {
  auto copy = std::make_unique<ClipSlot>();
  copyTimelineFieldsTo(*copy);
  if (clip) {
    copy->clip = clip->clone();
  }
  copy->hasStop = hasStop;
  return copy;
}

std::unique_ptr<Timeline> Markers::clone() const { return std::make_unique<Markers>(*this); }

std::unique_ptr<Timeline> Warps::clone() const {
  auto copy = std::make_unique<Warps>();
  copyTimelineFieldsTo(*copy);
  copy->content = cloneTimeline(content);
  copy->warps = warps;
  copy->contentTimeUnit = contentTimeUnit;
  return copy;
}

std::unique_ptr<Timeline> Audio::clone() const { return std::make_unique<Audio>(*this); }

std::unique_ptr<Timeline> Video::clone() const { return std::make_unique<Video>(*this); }

std::unique_ptr<Timeline> Points::clone() const { return std::make_unique<Points>(*this); }

// Devices ------------------------------------------------------------------------------------

void Device::copyDeviceFieldsTo(Device& target) const {
  static_cast<Referenceable&>(target) = static_cast<const Referenceable&>(*this);
  target.deviceName = deviceName;
  target.deviceRole = deviceRole;
  target.deviceID = deviceID;
  target.deviceVendor = deviceVendor;
  target.loaded = loaded;
  target.parameters.clear();
  target.parameters.reserve(parameters.size());
  for (const auto& parameter : parameters) {
    if (parameter) {
      target.parameters.push_back(parameter->clone());
    }
  }
  target.enabled = enabled;
  target.state = state;
}

void Plugin::copyPluginFieldsTo(Plugin& target) const {
  copyDeviceFieldsTo(target);
  target.pluginVersion = pluginVersion;
}

std::unique_ptr<Device> GenericDevice::clone() const {
  auto copy = std::make_unique<GenericDevice>();
  copyDeviceFieldsTo(*copy);
  return copy;
}

std::unique_ptr<Device> Vst2Plugin::clone() const {
  auto copy = std::make_unique<Vst2Plugin>();
  copyPluginFieldsTo(*copy);
  return copy;
}

std::unique_ptr<Device> Vst3Plugin::clone() const {
  auto copy = std::make_unique<Vst3Plugin>();
  copyPluginFieldsTo(*copy);
  return copy;
}

std::unique_ptr<Device> ClapPlugin::clone() const {
  auto copy = std::make_unique<ClapPlugin>();
  copyPluginFieldsTo(*copy);
  return copy;
}

std::unique_ptr<Device> AuPlugin::clone() const {
  auto copy = std::make_unique<AuPlugin>();
  copyPluginFieldsTo(*copy);
  return copy;
}

std::unique_ptr<Device> BuiltinDevice::clone() const {
  auto copy = std::make_unique<BuiltinDevice>();
  copyDeviceFieldsTo(*copy);
  return copy;
}

std::unique_ptr<Device> Equalizer::clone() const {
  auto copy = std::make_unique<Equalizer>();
  copyDeviceFieldsTo(*copy);
  copy->bands = bands;
  copy->inputGain = inputGain;
  copy->outputGain = outputGain;
  return copy;
}

std::unique_ptr<Device> Compressor::clone() const {
  auto copy = std::make_unique<Compressor>();
  copyDeviceFieldsTo(*copy);
  copy->attack = attack;
  copy->autoMakeup = autoMakeup;
  copy->inputGain = inputGain;
  copy->outputGain = outputGain;
  copy->ratio = ratio;
  copy->release = release;
  copy->threshold = threshold;
  return copy;
}

std::unique_ptr<Device> NoiseGate::clone() const {
  auto copy = std::make_unique<NoiseGate>();
  copyDeviceFieldsTo(*copy);
  copy->attack = attack;
  copy->range = range;
  copy->ratio = ratio;
  copy->release = release;
  copy->threshold = threshold;
  return copy;
}

std::unique_ptr<Device> Limiter::clone() const {
  auto copy = std::make_unique<Limiter>();
  copyDeviceFieldsTo(*copy);
  copy->attack = attack;
  copy->inputGain = inputGain;
  copy->outputGain = outputGain;
  copy->release = release;
  copy->threshold = threshold;
  return copy;
}

// Structure ----------------------------------------------------------------------------------

Channel Channel::clone() const {
  Channel copy;
  static_cast<Referenceable&>(copy) = static_cast<const Referenceable&>(*this);
  copy.role = role;
  copy.audioChannels = audioChannels;
  copy.destination = destination;
  copy.solo = solo;
  copy.devices.reserve(devices.size());
  for (const auto& device : devices) {
    if (device) {
      copy.devices.push_back(device->clone());
    }
  }
  copy.mute = mute;
  copy.pan = pan;
  copy.sends = sends;
  copy.volume = volume;
  return copy;
}

Track Track::clone() const {
  Track copy;
  static_cast<Referenceable&>(copy) = static_cast<const Referenceable&>(*this);
  copy.contentType = contentType;
  copy.loaded = loaded;
  if (channel) {
    copy.channel = channel->clone();
  }
  copy.tracks.reserve(tracks.size());
  for (const Track& child : tracks) {
    copy.tracks.push_back(child.clone());
  }
  return copy;
}

// Project ------------------------------------------------------------------------------------

Arrangement Arrangement::clone() const {
  Arrangement copy;
  static_cast<Referenceable&>(copy) = static_cast<const Referenceable&>(*this);
  if (lanes) {
    copy.lanes = std::make_unique<Lanes>(lanes->cloneLanes());
  }
  copy.markers = markers;
  copy.tempoAutomation = tempoAutomation;
  copy.timeSignatureAutomation = timeSignatureAutomation;
  return copy;
}

Scene Scene::clone() const {
  Scene copy;
  static_cast<Referenceable&>(copy) = static_cast<const Referenceable&>(*this);
  copy.content = cloneTimeline(content);
  return copy;
}

Project Project::clone() const {
  Project copy;
  copy.version = version;
  copy.application = application;
  copy.transport = transport;
  copy.structure.reserve(structure.size());
  for (const StructureItem& item : structure) {
    std::visit([&copy](const auto& entry) { copy.structure.emplace_back(entry.clone()); }, item);
  }
  if (arrangement) {
    copy.arrangement = arrangement->clone();
  }
  copy.scenes.reserve(scenes.size());
  for (const Scene& scene : scenes) {
    copy.scenes.push_back(scene.clone());
  }
  return copy;
}

}  // namespace dawproject
