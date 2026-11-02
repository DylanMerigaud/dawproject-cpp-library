// SPDX-License-Identifier: MIT
// Hand-written fixtures in the shape DAW exports take.
#include <cmath>

#include "doctest.h"
#include "test_support.hpp"

using namespace dawproject;

namespace {

std::string fixture(const char* name) { return testing::readText(testing::dataDir() / name); }

ParseOptions strictOptions() {
  ParseOptions options;
  options.strict = true;
  return options;
}

}  // namespace

TEST_SUITE("fixtures") {
  TEST_CASE("a Bitwig-like full project parses strictly with every value") {
    const std::string text = fixture("bitwig-like-full.xml");
    testing::dumpForSchemaCheck("fixture-bitwig-like-full-raw", text);
    const Project project = parseProject(text, strictOptions());

    CHECK(project.application.name == "Bitwig Studio");
    CHECK(project.application.version == "5.0.4");
    CHECK(project.transport->tempo->value == 124.0);
    CHECK(project.transport->tempo->max == 666.0);
    CHECK(project.transport->tempo->name == "Tempo");
    CHECK(project.transport->timeSignature->numerator == 4);
    REQUIRE(project.structure.size() == 4);

    const Track& bass = std::get<Track>(project.structure[0]);
    CHECK(bass.name == "Bass");
    CHECK(bass.color == "#a2eabf");
    CHECK(bass.loaded == true);
    const Channel& bassChannel = *bass.channel;
    CHECK(bassChannel.audioChannels == 2);
    CHECK(bassChannel.destination == "id20");
    CHECK(bassChannel.solo == false);
    REQUIRE(bassChannel.devices.size() == 2);
    const auto& diva = dynamic_cast<const ClapPlugin&>(*bassChannel.devices[0]);
    CHECK(diva.deviceID == "com.u-he.Diva");
    CHECK(diva.deviceVendor == "u-he");
    CHECK(diva.pluginVersion == "1.4.7");
    CHECK(diva.deviceRole == DeviceRole::Instrument);
    CHECK(diva.state->path == "plugins/7c2a4f1e-diva.clap-preset");
    REQUIRE(diva.parameters.size() == 2);
    CHECK(diva.parameters[0]->parameterID == 12);
    const auto& shape = dynamic_cast<const EnumParameter&>(*diva.parameters[1]);
    CHECK(shape.labels == std::vector<std::string>{"Saw", "Square", "Triangle"});
    const auto& eq = dynamic_cast<const Equalizer&>(*bassChannel.devices[1]);
    REQUIRE(eq.bands.size() == 2);
    CHECK(eq.bands[0].type == EqBandType::HighPass);
    CHECK(eq.bands[0].order == 2);
    CHECK(eq.bands[1].gain->value == -3.5);
    CHECK(std::isinf(*eq.outputGain->min));
    CHECK(bassChannel.mute->value == false);
    REQUIRE(bassChannel.sends.size() == 1);
    CHECK(bassChannel.sends[0].type == SendType::Post);
    CHECK(bassChannel.sends[0].volume.value == 0.2);
    CHECK(bassChannel.volume->value == 0.65914);

    const Track& fx = std::get<Track>(project.structure[2]);
    CHECK(fx.contentType == std::vector<ContentType>{ContentType::Audio, ContentType::Notes});
    CHECK(fx.channel->role == MixerRole::Effect);
    const Track& master = std::get<Track>(project.structure[3]);
    CHECK(master.channel->devices.at(0)->kind() == DeviceKind::Vst3Plugin);

    const Lanes& lanes = *project.arrangement->lanes;
    CHECK(lanes.timeUnit == TimeUnit::Beats);
    const auto* bassLanes = as<Lanes>(lanes.lanes.at(0).get());
    REQUIRE(bassLanes != nullptr);
    CHECK(bassLanes->track == "id2");
    const Clip& bassline = as<Clips>(bassLanes->lanes.at(0).get())->clips.at(0);
    CHECK(bassline.loopEnd == 4.0);
    CHECK(bassline.enable == true);
    const auto* notes = as<Notes>(bassline.content.get());
    REQUIRE(notes->notes.size() == 3);
    CHECK(notes->notes[2].key == 43);
    CHECK(notes->notes[2].vel == 0.629921);
    const auto* expression =
        as<Points>(as<Lanes>(notes->notes[2].content.get())->lanes.at(0).get());
    CHECK(expression->target.expression == ExpressionType::Transpose);
    const auto* cutoff = as<Points>(bassLanes->lanes.at(1).get());
    CHECK(cutoff->target.parameter == "id5");
    CHECK(std::get<RealPoint>(cutoff->points[1]).interpolation == Interpolation::Hold);

    const auto* drumLanes = as<Lanes>(lanes.lanes.at(1).get());
    const Clip& loop = as<Clips>(drumLanes->lanes.at(0).get())->clips.at(0);
    CHECK(loop.fadeOutTime == 0.125);
    const Clip& inner = as<Clips>(loop.content.get())->clips.at(0);
    const auto* warps = as<Warps>(inner.content.get());
    REQUIRE(warps != nullptr);
    CHECK(warps->warps.at(1).contentTime == 7.741497);
    const auto* audio = as<Audio>(warps->content.get());
    CHECK(audio->algorithm == "stretch");
    CHECK(audio->file.path == "audio/drum-loop.wav");
    CHECK_FALSE(audio->file.external.has_value());

    CHECK(project.arrangement->markers->markers.at(1).name == "Drop");
    CHECK(project.arrangement->tempoAutomation->unit == Unit::Bpm);
    REQUIRE(project.scenes.size() == 1);
    const auto* slot = as<ClipSlot>(as<Lanes>(project.scenes[0].content.get())->lanes.at(0).get());
    CHECK(slot->clip->reference == "id38");

    CHECK(validate(project).empty());
    testing::roundTrip(project, "fixture-bitwig-like-full");
  }

  TEST_CASE("a minimal project") {
    const std::string text = fixture("minimal.xml");
    testing::dumpForSchemaCheck("fixture-minimal-raw", text);
    const Project project = parseProject(text, strictOptions());
    CHECK(project.application.name == "Minimal DAW");
    CHECK_FALSE(project.transport.has_value());
    CHECK(project.structure.empty());
    testing::roundTrip(project, "fixture-minimal");
  }

  TEST_CASE("future elements: lenient mode reads what it knows, strict mode refuses") {
    const std::string text = fixture("future-elements.xml");
    const Project project = parseProject(text);
    CHECK(project.version == "1.1");
    CHECK(project.transport->tempo->value == 98.5);
    REQUIRE(project.structure.size() == 1);
    const Track& keys = std::get<Track>(project.structure[0]);
    CHECK(keys.contentType == std::vector<ContentType>{ContentType::Notes});
    REQUIRE(keys.channel->devices.size() == 1);
    CHECK(keys.channel->devices[0]->deviceName == "Piano");
    CHECK(keys.channel->volume->value == 0.8);
    const auto* notes = as<Notes>(project.arrangement->lanes->lanes.at(0).get());
    REQUIRE(project.arrangement->lanes->lanes.size() == 1);
    CHECK(notes->notes.at(0).vel == 0.8);

    CHECK_THROWS_AS((void)parseProject(text, strictOptions()), Error);
    try {
      (void)parseProject(text, strictOptions());
    } catch (const Error& error) {
      CHECK(error.kind() == ErrorKind::Schema);
      CHECK(std::string(error.what()).find("Project/Application") != std::string::npos);
    }
    // What was kept is valid 1.0 output.
    testing::dumpForSchemaCheck("fixture-future-elements-kept", serialize(project));
  }
}
