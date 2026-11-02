// SPDX-License-Identifier: MIT
#include <type_traits>

#include "doctest.h"
#include "test_support.hpp"

using namespace dawproject;

static_assert(!std::is_copy_constructible_v<Project>, "Project is move-only");
static_assert(std::is_nothrow_move_constructible_v<Project>, "Project moves cheaply");
static_assert(!std::is_copy_constructible_v<Lanes>, "Lanes is move-only");
static_assert(std::is_copy_constructible_v<Points>, "Points holds no owning pointer");
static_assert(std::is_copy_constructible_v<RealParameter>, "parameters are values");
static_assert(!std::is_move_assignable_v<Device>, "assigning through a Device& would slice");
static_assert(!std::is_constructible_v<Device, Device&&>, "Device cannot be sliced by a move");
static_assert(std::is_move_constructible_v<Equalizer>, "concrete devices still move");
static_assert(std::is_move_assignable_v<Vst3Plugin>, "concrete devices still move");

TEST_SUITE("model") {
  TEST_CASE("as<> casts to the concrete timeline type") {
    auto notes = std::make_unique<Notes>();
    Timeline* timeline = notes.get();
    CHECK(timeline->kind() == TimelineKind::Notes);
    CHECK(as<Notes>(timeline) == notes.get());
    CHECK(as<Clips>(timeline) == nullptr);
    const Timeline* constTimeline = timeline;
    CHECK(as<Notes>(constTimeline) != nullptr);
  }

  TEST_CASE("clone makes a deep copy of nested timelines") {
    Lanes lanes;
    lanes.id = "lanes";
    lanes.timeUnit = TimeUnit::Beats;
    auto clips = std::make_unique<Clips>();
    Clip clip;
    clip.time = 4;
    auto notes = std::make_unique<Notes>();
    Note note;
    note.key = 64;
    auto expression = std::make_unique<Points>();
    expression->target.expression = ExpressionType::Pressure;
    expression->points.emplace_back(RealPoint{0.0, 0.5, Interpolation::Linear});
    note.content = std::move(expression);
    notes->notes.push_back(std::move(note));
    clip.content = std::move(notes);
    clips->clips.push_back(std::move(clip));
    lanes.lanes.push_back(std::move(clips));

    std::unique_ptr<Timeline> copy = lanes.clone();
    auto* copiedLanes = as<Lanes>(copy.get());
    REQUIRE(copiedLanes != nullptr);
    CHECK(copiedLanes->id == "lanes");
    CHECK(copiedLanes->timeUnit == TimeUnit::Beats);
    auto* copiedClips = as<Clips>(copiedLanes->lanes.at(0).get());
    REQUIRE(copiedClips != nullptr);
    auto* copiedNotes = as<Notes>(copiedClips->clips.at(0).content.get());
    REQUIRE(copiedNotes != nullptr);
    CHECK(copiedNotes->notes.at(0).key == 64);
    auto* copiedPoints = as<Points>(copiedNotes->notes.at(0).content.get());
    REQUIRE(copiedPoints != nullptr);
    CHECK(std::get<RealPoint>(copiedPoints->points.at(0)).value == 0.5);

    // The copy is independent of the original.
    copiedNotes->notes.at(0).key = 10;
    auto* originalNotes = as<Notes>(as<Clips>(lanes.lanes[0].get())->clips[0].content.get());
    CHECK(originalNotes->notes.at(0).key == 64);
  }

  TEST_CASE("device clone keeps the concrete type and parameters") {
    Equalizer eq;
    eq.deviceName = "EQ";
    eq.deviceRole = DeviceRole::AudioFX;
    EqBand band;
    band.type = EqBandType::HighShelf;
    band.freq = RealParameter(Unit::Hertz, 8000.0);
    eq.bands.push_back(band);
    auto gain = std::make_unique<RealParameter>(Unit::Decibel, -3.0);
    gain->id = "gain";
    eq.parameters.push_back(std::move(gain));

    std::unique_ptr<Device> copy = eq.clone();
    REQUIRE(copy->kind() == DeviceKind::Equalizer);
    const auto& copiedEq = dynamic_cast<const Equalizer&>(*copy);
    CHECK(copiedEq.deviceName == "EQ");
    CHECK(copiedEq.bands.at(0).type == EqBandType::HighShelf);
    REQUIRE(copiedEq.parameters.size() == 1);
    CHECK(copiedEq.parameters[0]->kind() == ParameterKind::Real);
    CHECK(copiedEq.parameters[0].get() != eq.parameters[0].get());
    CHECK(copiedEq.parameters[0]->id == "gain");

    Vst3Plugin plugin;
    plugin.pluginVersion = "1.2";
    plugin.state = FileReference{"plugins/state.vstpreset", std::nullopt};
    auto pluginCopy = plugin.clone();
    REQUIRE(pluginCopy->kind() == DeviceKind::Vst3Plugin);
    CHECK(dynamic_cast<const Vst3Plugin&>(*pluginCopy).pluginVersion == "1.2");
    CHECK(pluginCopy->state->path == "plugins/state.vstpreset");
  }

  TEST_CASE("project clone copies structure, arrangement and scenes") {
    Project project;
    project.application = {"App", "2"};
    Track track;
    track.id = "t1";
    track.channel.emplace();
    track.channel->devices.push_back(std::make_unique<ClapPlugin>());
    Track child;
    child.id = "t2";
    track.tracks.push_back(std::move(child));
    project.structure.emplace_back(std::move(track));
    project.structure.emplace_back(Channel{});
    project.arrangement.emplace();
    project.arrangement->lanes = std::make_unique<Lanes>();
    project.arrangement->lanes->lanes.push_back(std::make_unique<Markers>());
    Scene scene;
    scene.content = std::make_unique<ClipSlot>();
    project.scenes.push_back(std::move(scene));

    const Project copy = project.clone();
    CHECK(copy.application.name == "App");
    REQUIRE(copy.structure.size() == 2);
    const auto& copiedTrack = std::get<Track>(copy.structure[0]);
    CHECK(copiedTrack.tracks.at(0).id == "t2");
    CHECK(copiedTrack.channel->devices.at(0)->kind() == DeviceKind::ClapPlugin);
    CHECK(std::holds_alternative<Channel>(copy.structure[1]));
    CHECK(copy.arrangement->lanes->lanes.at(0)->kind() == TimelineKind::Markers);
    CHECK(copy.scenes.at(0).content->kind() == TimelineKind::ClipSlot);
    CHECK(serialize(copy) == serialize(project));
  }
}
