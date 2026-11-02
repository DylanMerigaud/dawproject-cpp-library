// SPDX-License-Identifier: MIT
// Scenarios of the Java reference DawProjectTest, rebuilt with this library: each project is
// serialized, re-parsed in strict mode and checked field by field, and the round trip invariant
// serialize(parse(serialize(p))) == serialize(p) is asserted.
#include <set>

#include "doctest.h"
#include "test_support.hpp"

using namespace dawproject;

namespace {

enum class Feature { CueMarkers, Clips, Audio, Notes, Automation, AliasClips, Plugins };
using Features = std::set<Feature>;

const Features kSimpleFeatures = {Feature::Clips, Feature::Notes, Feature::Audio};
const Features kAllFeatures = {Feature::CueMarkers, Feature::Clips,      Feature::Audio,
                               Feature::Notes,      Feature::Automation, Feature::AliasClips,
                               Feature::Plugins};

Track makeTrack(IdGenerator& ids, const std::string& name, std::vector<ContentType> types,
                MixerRole role, double volume, double pan) {
  Track track;
  track.id = ids.next();
  track.name = name;
  track.contentType = std::move(types);
  Channel channel;
  channel.id = ids.next();
  channel.role = role;
  channel.volume = RealParameter(Unit::Linear, volume);
  channel.volume->id = ids.next();
  channel.pan = RealParameter(Unit::Normalized, pan);
  channel.pan->id = ids.next();
  track.channel = std::move(channel);
  return track;
}

Project emptyProject() {
  Project project;
  project.application = {"Test", "1.0"};
  return project;
}

RealPoint point(double time, double value, Interpolation interpolation) {
  RealPoint p;
  p.time = time;
  p.value = value;
  p.interpolation = interpolation;
  return p;
}

Project dummyProject(int numTracks, const Features& features) {
  IdGenerator ids;
  Project project = emptyProject();
  Track master = makeTrack(ids, "Master", {}, MixerRole::Master, 1, 0.5);

  if (features.count(Feature::Plugins)) {
    auto device = std::make_unique<Vst3Plugin>();
    device->id = ids.next();
    device->deviceName = "Limiter";
    device->deviceRole = DeviceRole::AudioFX;
    device->state = FileReference{"plugin-states/12323545.vstpreset", std::nullopt};
    master.channel->devices.push_back(std::move(device));
  }
  const std::string masterChannelId = *master.channel->id;
  project.structure.emplace_back(std::move(master));

  project.arrangement.emplace();
  project.arrangement->id = ids.next();
  auto arrangementLanes = std::make_unique<Lanes>();
  arrangementLanes->id = ids.next();
  arrangementLanes->timeUnit = TimeUnit::Beats;

  if (features.count(Feature::CueMarkers)) {
    Markers cueMarkers;
    cueMarkers.id = ids.next();
    Marker verse;
    verse.name = "Verse";
    verse.time = 0;
    Marker chorus;
    chorus.name = "Chorus";
    chorus.time = 24;
    cueMarkers.markers = {verse, chorus};
    project.arrangement->markers = std::move(cueMarkers);
  }

  for (int i = 0; i < numTracks; ++i) {
    Track track = makeTrack(ids, "Track " + std::to_string(i + 1), {ContentType::Notes},
                            MixerRole::Regular, 1, 0.5);
    const std::string digit = std::to_string(i);
    track.color = "#" + digit + digit + digit + digit + digit + digit;
    track.channel->destination = masterChannelId;

    auto trackLanes = std::make_unique<Lanes>();
    trackLanes->id = ids.next();
    trackLanes->track = track.id;

    if (features.count(Feature::Clips)) {
      auto clips = std::make_unique<Clips>();
      clips->id = ids.next();

      Clip clip;
      clip.name = "Clip " + std::to_string(i);
      clip.time = 8.0 * i;
      clip.duration = 4.0;

      auto notes = std::make_unique<Notes>();
      notes->id = ids.next();
      for (int j = 0; j < 8; ++j) {
        Note note;
        note.key = 36 + 12 * (j % (1 + i));
        note.vel = 0.8;
        note.rel = 0.5;
        note.time = 0.5 * j;
        note.duration = 0.5;
        notes->notes.push_back(std::move(note));
      }
      const std::string notesId = *notes->id;
      clip.content = std::move(notes);
      clips->clips.push_back(std::move(clip));

      if (features.count(Feature::AliasClips)) {
        Clip alias;
        alias.name = "Alias Clip " + std::to_string(i);
        alias.time = 32.0 + 8.0 * i;
        alias.duration = 4.0;
        alias.reference = notesId;
        clips->clips.push_back(std::move(alias));
      }
      trackLanes->lanes.push_back(std::move(clips));

      if (i == 0 && features.count(Feature::Automation)) {
        auto points = std::make_unique<Points>();
        points->id = ids.next();
        points->target.parameter = track.channel->volume->id;
        // Fade in over 8 quarter notes.
        points->points.emplace_back(point(0.0, 0.0, Interpolation::Linear));
        points->points.emplace_back(point(8.0, 1.0, Interpolation::Linear));
        trackLanes->lanes.push_back(std::move(points));
      }
    }
    arrangementLanes->lanes.push_back(std::move(trackLanes));
    project.structure.emplace_back(std::move(track));
  }
  project.arrangement->lanes = std::move(arrangementLanes);
  return project;
}

enum class AudioScenario {
  Warped,
  RawBeats,
  RawSeconds,
  FileWithAbsolutePath,
  FileWithRelativePath
};

const char* scenarioName(AudioScenario scenario) {
  switch (scenario) {
    case AudioScenario::Warped:
      return "WARPED";
    case AudioScenario::RawBeats:
      return "RAW_BEATS";
    case AudioScenario::RawSeconds:
      return "RAW_SECONDS";
    case AudioScenario::FileWithAbsolutePath:
      return "FILE_WITH_ABSOLUTE_PATH";
    case AudioScenario::FileWithRelativePath:
      return "FILE_WITH_RELATIVE_PATH";
  }
  return "";
}

std::unique_ptr<Audio> makeAudio(const std::string& path, int sampleRate, int channels,
                                 double duration) {
  auto audio = std::make_unique<Audio>();
  audio->timeUnit = TimeUnit::Seconds;
  audio->file = FileReference{path, false};
  audio->sampleRate = sampleRate;
  audio->channels = channels;
  audio->duration = duration;
  return audio;
}

Project audioExample(double playStartOffset, double clipTime, AudioScenario scenario,
                     bool withFades) {
  IdGenerator ids;
  Project project = emptyProject();
  Track master = makeTrack(ids, "Master", {}, MixerRole::Master, 1, 0.5);
  Track audioTrack = makeTrack(ids, "Audio", {ContentType::Audio}, MixerRole::Regular, 1, 0.5);
  audioTrack.channel->destination = master.channel->id;
  const std::string audioTrackId = *audioTrack.id;
  project.structure.emplace_back(std::move(master));
  project.structure.emplace_back(std::move(audioTrack));

  project.transport.emplace();
  project.transport->tempo = RealParameter(Unit::Bpm, 155.0);
  project.arrangement.emplace();
  auto lanes = std::make_unique<Lanes>();
  const bool inSeconds = scenario == AudioScenario::RawSeconds;
  lanes->timeUnit = inSeconds ? TimeUnit::Seconds : TimeUnit::Beats;

  const std::string sample = "white-glasses.wav";
  const double sampleDuration = 3.097;
  auto audio = makeAudio(sample, 44100, 2, sampleDuration);
  if (scenario == AudioScenario::FileWithAbsolutePath) {
    audio->file.external = true;
    audio->file.path = "/home/user/test-data/" + sample;
  } else if (scenario == AudioScenario::FileWithRelativePath) {
    audio->file.external = true;
    audio->file.path = "../test-data/" + sample;
  }

  Clip clip;
  if (scenario == AudioScenario::Warped) {
    auto warps = std::make_unique<Warps>();
    warps->content = std::move(audio);
    warps->contentTimeUnit = TimeUnit::Seconds;
    warps->warps.push_back(Warp{0.0, 0.0});
    warps->warps.push_back(Warp{8.0, sampleDuration});
    clip.content = std::move(warps);
    clip.time = clipTime;
    clip.duration = 8.0;
    clip.contentTimeUnit = TimeUnit::Beats;
    clip.playStart = playStartOffset;
    if (withFades) {
      clip.fadeTimeUnit = TimeUnit::Beats;
      clip.fadeInTime = 0.25;
      clip.fadeOutTime = 0.25;
    }
  } else {
    clip.content = std::move(audio);
    clip.time = clipTime;
    clip.duration = inSeconds ? sampleDuration : 8.0;
    clip.contentTimeUnit = TimeUnit::Seconds;
    clip.playStart = playStartOffset;
    clip.playStop = sampleDuration;
    if (withFades) {
      clip.fadeTimeUnit = TimeUnit::Seconds;
      clip.fadeInTime = 0.1;
      clip.fadeOutTime = 0.1;
    }
  }
  auto clips = std::make_unique<Clips>();
  clips->track = audioTrackId;
  clips->clips.push_back(std::move(clip));
  lanes->lanes.push_back(std::move(clips));
  project.arrangement->lanes = std::move(lanes);
  return project;
}

Project midiAutomationExample(bool inClips, bool isPitchBend) {
  IdGenerator ids;
  Project project = emptyProject();
  Track master = makeTrack(ids, "Master", {}, MixerRole::Master, 1, 0.5);
  Track instrument = makeTrack(ids, "Notes", {ContentType::Notes}, MixerRole::Regular, 1, 0.5);
  instrument.channel->destination = master.channel->id;
  const std::string instrumentId = *instrument.id;
  project.structure.emplace_back(std::move(master));
  project.structure.emplace_back(std::move(instrument));

  project.transport.emplace();
  project.transport->tempo = RealParameter(Unit::Bpm, 123.0);
  project.arrangement.emplace();
  auto lanes = std::make_unique<Lanes>();
  lanes->timeUnit = TimeUnit::Beats;

  auto automation = std::make_unique<Points>();
  automation->unit = Unit::Normalized;
  automation->target.channel = 0;
  if (isPitchBend) {
    automation->target.expression = ExpressionType::PitchBend;
  } else {
    automation->target.expression = ExpressionType::ChannelController;
    automation->target.controller = 1;
  }
  const double values[] = {0.0, 0.0, 0.5, 0.5, 1.0, 1.0, 0.5};
  for (int i = 0; i < 7; ++i) {
    automation->points.emplace_back(point(i, values[i], Interpolation::Linear));
  }
  automation->points.emplace_back(point(7, 1.0, Interpolation::Hold));
  automation->points.emplace_back(point(8, 0.5, Interpolation::Hold));

  if (inClips) {
    Clip clip;
    clip.time = 0;
    clip.duration = 8;
    clip.content = std::move(automation);
    auto clips = std::make_unique<Clips>();
    clips->track = instrumentId;
    clips->clips.push_back(std::move(clip));
    lanes->lanes.push_back(std::move(clips));
  } else {
    automation->track = instrumentId;
    lanes->lanes.push_back(std::move(automation));
  }
  project.arrangement->lanes = std::move(lanes);
  return project;
}

const Track& trackAt(const Project& project, std::size_t index) {
  return std::get<Track>(project.structure.at(index));
}

}  // namespace

TEST_SUITE("scenarios") {
  TEST_CASE("empty project") {
    const Project parsed = testing::roundTrip(emptyProject(), "scenario-empty");
    CHECK(parsed.application.name == "Test");
    CHECK(parsed.application.version == "1.0");
    CHECK(validate(parsed).empty());
  }

  TEST_CASE("simple dummy project: tracks, channels, clips and notes") {
    const Project project = dummyProject(3, kSimpleFeatures);
    CHECK(validate(project).empty());
    const Project parsed = testing::roundTrip(project, "scenario-simple");
    REQUIRE(parsed.structure.size() == 4);
    const Track& master = trackAt(parsed, 0);
    CHECK(master.name == "Master");
    CHECK(master.contentType.empty());
    CHECK(master.channel->role == MixerRole::Master);
    CHECK(master.channel->volume->value == 1.0);
    CHECK(master.channel->volume->unit == Unit::Linear);
    CHECK(master.channel->pan->value == 0.5);
    CHECK(master.channel->pan->unit == Unit::Normalized);

    const Track& second = trackAt(parsed, 2);
    CHECK(second.name == "Track 2");
    CHECK(second.color == "#111111");
    CHECK(second.contentType == std::vector<ContentType>{ContentType::Notes});
    CHECK(second.channel->destination == master.channel->id);

    const Lanes& arrangementLanes = *parsed.arrangement->lanes;
    CHECK(arrangementLanes.timeUnit == TimeUnit::Beats);
    REQUIRE(arrangementLanes.lanes.size() == 3);
    const auto* trackLanes = as<Lanes>(arrangementLanes.lanes[1].get());
    REQUIRE(trackLanes != nullptr);
    CHECK(trackLanes->track == second.id);
    const auto* clips = as<Clips>(trackLanes->lanes.at(0).get());
    REQUIRE(clips != nullptr);
    const Clip& clip = clips->clips.at(0);
    CHECK(clip.name == "Clip 1");
    CHECK(clip.time == 8.0);
    CHECK(clip.duration == 4.0);
    const auto* notes = as<Notes>(clip.content.get());
    REQUIRE(notes != nullptr);
    REQUIRE(notes->notes.size() == 8);
    CHECK(notes->notes[1].key == 48);
    CHECK(notes->notes[1].time == 0.5);
    CHECK(notes->notes[1].duration == 0.5);
    CHECK(notes->notes[1].vel == 0.8);
    CHECK(notes->notes[1].rel == 0.5);
    CHECK(notes->notes[1].channel == 0);
    CHECK_FALSE(parsed.arrangement->markers.has_value());
  }

  TEST_CASE("complex dummy project: markers, plugins, automation and alias clips") {
    const Project project = dummyProject(5, kAllFeatures);
    CHECK(validate(project).empty());
    const Project parsed = testing::roundTrip(project, "scenario-complex");
    CHECK(parsed.structure.size() == project.structure.size());
    CHECK(parsed.scenes.size() == project.scenes.size());
    CHECK(parsed.arrangement->lanes->kind() == TimelineKind::Lanes);

    const Markers& markers = *parsed.arrangement->markers;
    REQUIRE(markers.markers.size() == 2);
    CHECK(markers.markers[1].name == "Chorus");
    CHECK(markers.markers[1].time == 24.0);

    const Track& master = trackAt(parsed, 0);
    REQUIRE(master.channel->devices.size() == 1);
    const Device& device = *master.channel->devices[0];
    CHECK(device.kind() == DeviceKind::Vst3Plugin);
    CHECK(device.deviceName == "Limiter");
    CHECK(device.deviceRole == DeviceRole::AudioFX);
    CHECK(device.state->path == "plugin-states/12323545.vstpreset");

    const auto* firstLanes = as<Lanes>(parsed.arrangement->lanes->lanes.at(0).get());
    REQUIRE(firstLanes->lanes.size() == 2);
    const auto* clips = as<Clips>(firstLanes->lanes[0].get());
    REQUIRE(clips->clips.size() == 2);
    const Clip& alias = clips->clips[1];
    CHECK(alias.name == "Alias Clip 0");
    CHECK(alias.time == 32.0);
    CHECK(alias.content == nullptr);
    CHECK(alias.reference == clips->clips[0].content->id);

    const auto* automation = as<Points>(firstLanes->lanes[1].get());
    REQUIRE(automation != nullptr);
    CHECK(automation->target.parameter == trackAt(parsed, 1).channel->volume->id);
    REQUIRE(automation->points.size() == 2);
    const auto& end = std::get<RealPoint>(automation->points[1]);
    CHECK(end.time == 8.0);
    CHECK(end.value == 1.0);
    CHECK(end.interpolation == Interpolation::Linear);
  }

  TEST_CASE("audio clips: warped, raw, external files, offsets and fades") {
    const AudioScenario scenarios[] = {
        AudioScenario::Warped, AudioScenario::RawBeats, AudioScenario::RawSeconds,
        AudioScenario::FileWithAbsolutePath, AudioScenario::FileWithRelativePath};
    for (const AudioScenario scenario : scenarios) {
      struct Variant {
        double playStart;
        double clipTime;
        bool fades;
        const char* suffix;
      };
      std::vector<Variant> variants = {{0, 0, false, ""}};
      const bool offsetsAndFades = scenario == AudioScenario::Warped ||
                                   scenario == AudioScenario::RawBeats ||
                                   scenario == AudioScenario::RawSeconds;
      if (offsetsAndFades) {
        variants.push_back({0, 0, true, "WithFades"});
        variants.push_back({1, 0, false, "Offset"});
        variants.push_back({0, 1, false, "Clipstart"});
      }
      for (const Variant& variant : variants) {
        const std::string name =
            std::string("scenario-Audio") + scenarioName(scenario) + variant.suffix;
        CAPTURE(name);
        const Project project =
            audioExample(variant.playStart, variant.clipTime, scenario, variant.fades);
        CHECK(validate(project).empty());
        const Project parsed = testing::roundTrip(project, name);
        CHECK(parsed.transport->tempo->value == 155.0);
        CHECK(parsed.transport->tempo->unit == Unit::Bpm);
        const auto* clips = as<Clips>(parsed.arrangement->lanes->lanes.at(0).get());
        REQUIRE(clips != nullptr);
        CHECK(clips->track == trackAt(parsed, 1).id);
        const Clip& clip = clips->clips.at(0);
        CHECK(clip.time == variant.clipTime);
        CHECK(clip.playStart == variant.playStart);
        CHECK(clip.fadeInTime.has_value() == variant.fades);
        const Audio* audio = nullptr;
        if (scenario == AudioScenario::Warped) {
          const auto* warps = as<Warps>(clip.content.get());
          REQUIRE(warps != nullptr);
          CHECK(warps->contentTimeUnit == TimeUnit::Seconds);
          REQUIRE(warps->warps.size() == 2);
          CHECK(warps->warps[1].time == 8.0);
          CHECK(warps->warps[1].contentTime == 3.097);
          audio = as<Audio>(warps->content.get());
        } else {
          CHECK(clip.playStop == 3.097);
          audio = as<Audio>(clip.content.get());
        }
        REQUIRE(audio != nullptr);
        CHECK(audio->sampleRate == 44100);
        CHECK(audio->channels == 2);
        CHECK(audio->duration == 3.097);
        CHECK(audio->timeUnit == TimeUnit::Seconds);
        const bool external = scenario == AudioScenario::FileWithAbsolutePath ||
                              scenario == AudioScenario::FileWithRelativePath;
        CHECK(audio->file.external == external);
      }
    }
  }

  TEST_CASE("MIDI automation on tracks and in clips") {
    for (const bool inClips : {false, true}) {
      for (const bool pitchBend : {false, true}) {
        const std::string name = std::string("scenario-MIDI-") + (pitchBend ? "PitchBend" : "CC1") +
                                 "-AutomationOn" + (inClips ? "Clips" : "Track");
        CAPTURE(name);
        const Project project = midiAutomationExample(inClips, pitchBend);
        CHECK(validate(project).empty());
        const Project parsed = testing::roundTrip(project, name);
        const Timeline* lane = parsed.arrangement->lanes->lanes.at(0).get();
        const Points* points = nullptr;
        if (inClips) {
          const auto* clips = as<Clips>(lane);
          REQUIRE(clips != nullptr);
          points = as<Points>(clips->clips.at(0).content.get());
        } else {
          points = as<Points>(lane);
        }
        REQUIRE(points != nullptr);
        CHECK(points->unit == Unit::Normalized);
        CHECK(points->target.channel == 0);
        CHECK(points->target.expression ==
              (pitchBend ? ExpressionType::PitchBend : ExpressionType::ChannelController));
        CHECK(points->target.controller.has_value() == !pitchBend);
        REQUIRE(points->points.size() == 9);
        CHECK(std::get<RealPoint>(points->points[7]).interpolation == Interpolation::Hold);
      }
    }
  }

  TEST_CASE("notes with per-note expressions") {
    Project project = emptyProject();
    project.arrangement.emplace();
    project.arrangement->lanes = std::make_unique<Lanes>();
    auto notes = std::make_unique<Notes>();
    Note note;
    note.time = 0;
    note.duration = 2;
    note.key = 60;
    note.channel = 1;
    note.vel = 0.75;
    auto expressions = std::make_unique<Lanes>();
    for (const ExpressionType type : {ExpressionType::Transpose, ExpressionType::Pressure}) {
      auto points = std::make_unique<Points>();
      points->target.expression = type;
      points->target.key = 60;
      points->target.channel = 1;
      points->points.emplace_back(point(0.0, 0.0, Interpolation::Linear));
      points->points.emplace_back(point(1.0, 0.5, Interpolation::Linear));
      expressions->lanes.push_back(std::move(points));
    }
    note.content = std::move(expressions);
    notes->notes.push_back(std::move(note));
    project.arrangement->lanes->lanes.push_back(std::move(notes));

    const Project parsed = testing::roundTrip(project, "scenario-note-expressions");
    const auto* parsedNotes = as<Notes>(parsed.arrangement->lanes->lanes.at(0).get());
    REQUIRE(parsedNotes != nullptr);
    const Note& parsedNote = parsedNotes->notes.at(0);
    CHECK(parsedNote.channel == 1);
    CHECK(parsedNote.vel == 0.75);
    CHECK_FALSE(parsedNote.rel.has_value());
    const auto* parsedExpressions = as<Lanes>(parsedNote.content.get());
    REQUIRE(parsedExpressions != nullptr);
    REQUIRE(parsedExpressions->lanes.size() == 2);
    const auto* pressure = as<Points>(parsedExpressions->lanes[1].get());
    CHECK(pressure->target.expression == ExpressionType::Pressure);
    CHECK(pressure->target.key == 60);
  }

  TEST_CASE("scenes with clip slots") {
    IdGenerator ids;
    Project project = emptyProject();
    Track track = makeTrack(ids, "Synth", {ContentType::Notes}, MixerRole::Regular, 0.8, 0.5);
    const std::string trackId = *track.id;
    project.structure.emplace_back(std::move(track));
    for (int s = 0; s < 2; ++s) {
      Scene scene;
      scene.id = ids.next();
      scene.name = "Scene " + std::to_string(s + 1);
      auto lanes = std::make_unique<Lanes>();
      auto slot = std::make_unique<ClipSlot>();
      slot->track = trackId;
      slot->hasStop = true;
      if (s == 0) {
        Clip clip;
        clip.time = 0;
        clip.duration = 4;
        clip.loopStart = 0;
        clip.loopEnd = 4;
        clip.enable = true;
        auto notes = std::make_unique<Notes>();
        Note note;
        note.duration = 1;
        notes->notes.push_back(std::move(note));
        clip.content = std::move(notes);
        slot->clip = std::move(clip);
      }
      lanes->lanes.push_back(std::move(slot));
      scene.content = std::move(lanes);
      project.scenes.push_back(std::move(scene));
    }
    CHECK(validate(project).empty());
    const Project parsed = testing::roundTrip(project, "scenario-scenes");
    REQUIRE(parsed.scenes.size() == 2);
    CHECK(parsed.scenes[1].name == "Scene 2");
    const auto* lanes = as<Lanes>(parsed.scenes[0].content.get());
    REQUIRE(lanes != nullptr);
    const auto* slot = as<ClipSlot>(lanes->lanes.at(0).get());
    REQUIRE(slot != nullptr);
    CHECK(slot->hasStop == true);
    CHECK(slot->track == trackId);
    REQUIRE(slot->clip.has_value());
    CHECK(slot->clip->loopEnd == 4.0);
    CHECK(slot->clip->enable == true);
    const auto* emptySlot = as<ClipSlot>(as<Lanes>(parsed.scenes[1].content.get())->lanes[0].get());
    CHECK_FALSE(emptySlot->clip.has_value());
  }

  TEST_CASE("every device, parameter and point type") {
    Project project = emptyProject();
    project.transport.emplace();
    project.transport->tempo = RealParameter(Unit::Bpm, 128.0);
    project.transport->tempo->min = 20;
    project.transport->tempo->max = 999;
    project.transport->tempo->id = "tempo";
    project.transport->timeSignature = TimeSignatureParameter(7, 8);
    project.transport->timeSignature->id = "ts";

    Channel bus;
    bus.id = "bus";
    bus.role = MixerRole::Effect;
    bus.audioChannels = 2;
    bus.solo = false;
    bus.mute = BoolParameter(false);
    bus.volume = RealParameter(Unit::Decibel, -6.0);

    auto generic = std::make_unique<GenericDevice>();
    generic->deviceName = "Generic";
    generic->deviceRole = DeviceRole::Analyzer;
    generic->deviceVendor = "Vendor";
    generic->loaded = false;
    auto real = std::make_unique<RealParameter>(Unit::Percent, 50.0);
    real->id = "mix";
    real->parameterID = 3;
    generic->parameters.push_back(std::move(real));
    auto boolean = std::make_unique<BoolParameter>(true);
    boolean->name = "Bypass";
    generic->parameters.push_back(std::move(boolean));
    auto integer = std::make_unique<IntegerParameter>();
    integer->value = 3;
    integer->min = 0;
    integer->max = 10;
    generic->parameters.push_back(std::move(integer));
    auto enumeration = std::make_unique<EnumParameter>();
    enumeration->id = "mode";
    enumeration->value = 1;
    enumeration->count = 3;
    enumeration->labels = {"Low", "Mid", "High"};
    generic->parameters.push_back(std::move(enumeration));
    auto signature = std::make_unique<TimeSignatureParameter>(3, 4);
    generic->parameters.push_back(std::move(signature));
    generic->enabled = BoolParameter(true);
    bus.devices.push_back(std::move(generic));

    auto vst2 = std::make_unique<Vst2Plugin>();
    vst2->deviceName = "Old Synth";
    vst2->deviceRole = DeviceRole::Instrument;
    vst2->deviceID = "1234";
    vst2->pluginVersion = "2.4";
    bus.devices.push_back(std::move(vst2));
    auto clap = std::make_unique<ClapPlugin>();
    clap->deviceName = "Clap Arp";
    clap->deviceRole = DeviceRole::NoteFX;
    clap->deviceID = "com.example.arp";
    bus.devices.push_back(std::move(clap));
    auto au = std::make_unique<AuPlugin>();
    au->deviceName = "AU Delay";
    au->state = FileReference{"plugins/au.aupreset", false};
    bus.devices.push_back(std::move(au));
    auto builtin = std::make_unique<BuiltinDevice>();
    builtin->deviceName = "Chorus";
    bus.devices.push_back(std::move(builtin));

    auto eq = std::make_unique<Equalizer>();
    eq->deviceName = "EQ";
    EqBand low;
    low.type = EqBandType::LowShelf;
    low.order = 2;
    low.freq = RealParameter(Unit::Hertz, 120.0);
    low.gain = RealParameter(Unit::Decibel, 3.0);
    low.q = RealParameter(Unit::Linear, 0.707);
    low.enabled = BoolParameter(true);
    eq->bands.push_back(low);
    EqBand notch;
    notch.type = EqBandType::Notch;
    notch.freq = RealParameter(Unit::Hertz, 3000.0);
    eq->bands.push_back(notch);
    eq->inputGain = RealParameter(Unit::Decibel, 0.0);
    eq->outputGain = RealParameter(Unit::Decibel, -1.0);
    bus.devices.push_back(std::move(eq));

    auto compressor = std::make_unique<Compressor>();
    compressor->deviceName = "Comp";
    compressor->attack = RealParameter(Unit::Seconds, 0.01);
    compressor->autoMakeup = BoolParameter(true);
    compressor->inputGain = RealParameter(Unit::Decibel, 0.0);
    compressor->outputGain = RealParameter(Unit::Decibel, 2.0);
    compressor->ratio = RealParameter(Unit::Linear, 4.0);
    compressor->release = RealParameter(Unit::Seconds, 0.1);
    compressor->threshold = RealParameter(Unit::Decibel, -18.0);
    bus.devices.push_back(std::move(compressor));

    auto gate = std::make_unique<NoiseGate>();
    gate->deviceName = "Gate";
    gate->attack = RealParameter(Unit::Seconds, 0.001);
    gate->range = RealParameter(Unit::Decibel, -40.0);
    gate->ratio = RealParameter(Unit::Linear, 10.0);
    gate->release = RealParameter(Unit::Seconds, 0.05);
    gate->threshold = RealParameter(Unit::Decibel, -50.0);
    bus.devices.push_back(std::move(gate));

    auto limiter = std::make_unique<Limiter>();
    limiter->deviceName = "Limit";
    limiter->attack = RealParameter(Unit::Seconds, 0.0005);
    limiter->inputGain = RealParameter(Unit::Decibel, 1.0);
    limiter->outputGain = RealParameter(Unit::Decibel, -0.3);
    limiter->release = RealParameter(Unit::Seconds, 0.2);
    limiter->threshold = RealParameter(Unit::Decibel, -1.0);
    bus.devices.push_back(std::move(limiter));
    project.structure.emplace_back(std::move(bus));

    Track folder;
    folder.id = "folder";
    folder.contentType = {ContentType::Tracks};
    folder.loaded = true;
    Track child;
    child.id = "child";
    child.contentType = {ContentType::Audio, ContentType::Automation, ContentType::Video,
                         ContentType::Markers};
    child.channel.emplace();
    child.channel->id = "childChannel";
    child.channel->destination = "bus";
    Send send;
    send.id = "send";
    send.destination = "bus";
    send.type = SendType::Pre;
    send.enable = BoolParameter(true);
    send.pan = RealParameter(Unit::Normalized, 0.5);
    send.volume = RealParameter(Unit::Linear, 0.3);
    child.channel->sends.push_back(send);
    folder.tracks.push_back(std::move(child));
    project.structure.emplace_back(std::move(folder));

    project.arrangement.emplace();
    project.arrangement->lanes = std::make_unique<Lanes>();
    auto parameterLane = std::make_unique<Points>();
    parameterLane->target.parameter = "mode";
    parameterLane->points.emplace_back(EnumPoint{0.0, 2});
    parameterLane->points.emplace_back(IntegerPoint{1.0, 7});
    parameterLane->points.emplace_back(BoolPoint{2.0, true});
    project.arrangement->lanes->lanes.push_back(std::move(parameterLane));
    auto video = std::make_unique<Video>();
    video->file = FileReference{"video/clip.mp4", std::nullopt};
    video->duration = 12.5;
    video->channels = 2;
    video->sampleRate = 48000;
    video->algorithm = "none";
    project.arrangement->lanes->lanes.push_back(std::move(video));
    project.arrangement->tempoAutomation.emplace();
    project.arrangement->tempoAutomation->target.parameter = "tempo";
    project.arrangement->tempoAutomation->unit = Unit::Bpm;
    project.arrangement->tempoAutomation->points.emplace_back(
        point(0.0, 120.0, Interpolation::Linear));
    project.arrangement->timeSignatureAutomation.emplace();
    project.arrangement->timeSignatureAutomation->target.parameter = "ts";
    TimeSignaturePoint tsPoint;
    tsPoint.time = 16;
    tsPoint.numerator = 3;
    tsPoint.denominator = 4;
    project.arrangement->timeSignatureAutomation->points.emplace_back(tsPoint);

    CHECK(validate(project).empty());
    const Project parsed = testing::roundTrip(project, "scenario-all-types");
    CHECK(parsed.transport->timeSignature->numerator == 7);
    CHECK(parsed.transport->timeSignature->denominator == 8);
    CHECK(parsed.transport->tempo->max == 999.0);
    const Channel& parsedBus = std::get<Channel>(parsed.structure.at(0));
    REQUIRE(parsedBus.devices.size() == 9);
    const DeviceKind kinds[] = {
        DeviceKind::Device,     DeviceKind::Vst2Plugin,    DeviceKind::ClapPlugin,
        DeviceKind::AuPlugin,   DeviceKind::BuiltinDevice, DeviceKind::Equalizer,
        DeviceKind::Compressor, DeviceKind::NoiseGate,     DeviceKind::Limiter};
    for (std::size_t i = 0; i < parsedBus.devices.size(); ++i) {
      CHECK(parsedBus.devices[i]->kind() == kinds[i]);
    }
    const Device& parsedGeneric = *parsedBus.devices[0];
    REQUIRE(parsedGeneric.parameters.size() == 5);
    const auto& parsedEnum = dynamic_cast<const EnumParameter&>(*parsedGeneric.parameters[3]);
    CHECK(parsedEnum.labels == std::vector<std::string>{"Low", "Mid", "High"});
    CHECK(parsedEnum.count == 3);
    CHECK(dynamic_cast<const RealParameter&>(*parsedGeneric.parameters[0]).parameterID == 3);
    CHECK(dynamic_cast<const Vst2Plugin&>(*parsedBus.devices[1]).pluginVersion == "2.4");
    const auto& parsedEq = dynamic_cast<const Equalizer&>(*parsedBus.devices[5]);
    REQUIRE(parsedEq.bands.size() == 2);
    CHECK(parsedEq.bands[0].q->value == 0.707);
    CHECK(parsedEq.bands[1].type == EqBandType::Notch);
    const auto& parsedComp = dynamic_cast<const Compressor&>(*parsedBus.devices[6]);
    CHECK(parsedComp.threshold->value == -18.0);
    CHECK(parsedComp.autoMakeup->value == true);
    CHECK(dynamic_cast<const NoiseGate&>(*parsedBus.devices[7]).range->value == -40.0);
    CHECK(dynamic_cast<const Limiter&>(*parsedBus.devices[8]).outputGain->value == -0.3);

    const Track& parsedFolder = std::get<Track>(parsed.structure.at(1));
    REQUIRE(parsedFolder.tracks.size() == 1);
    const Channel& childChannel = *parsedFolder.tracks[0].channel;
    REQUIRE(childChannel.sends.size() == 1);
    CHECK(childChannel.sends[0].type == SendType::Pre);
    CHECK(childChannel.sends[0].volume.value == 0.3);
    CHECK(parsedFolder.tracks[0].contentType.size() == 4);

    const auto* lane = as<Points>(parsed.arrangement->lanes->lanes.at(0).get());
    REQUIRE(lane->points.size() == 3);
    CHECK(std::get<EnumPoint>(lane->points[0]).value == 2);
    CHECK(std::get<IntegerPoint>(lane->points[1]).value == 7);
    CHECK(std::get<BoolPoint>(lane->points[2]).value == true);
    const auto* parsedVideo = as<Video>(parsed.arrangement->lanes->lanes.at(1).get());
    REQUIRE(parsedVideo != nullptr);
    CHECK(parsedVideo->algorithm == "none");
    CHECK(std::get<TimeSignaturePoint>(parsed.arrangement->timeSignatureAutomation->points[0])
              .numerator == 3);
  }
}
