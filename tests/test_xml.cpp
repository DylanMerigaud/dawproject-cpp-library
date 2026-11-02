// SPDX-License-Identifier: MIT
#include <cmath>
#include <limits>

#include "doctest.h"
#include "test_support.hpp"

using namespace dawproject;

namespace {

ErrorKind errorKindOf(std::string_view xml, bool strict) {
  ParseOptions options;
  options.strict = strict;
  try {
    (void)parseProject(xml, options);
  } catch (const Error& error) {
    return error.kind();
  }
  FAIL("no error thrown");
  return ErrorKind::Io;
}

std::string errorMessageOf(std::string_view xml, bool strict) {
  ParseOptions options;
  options.strict = strict;
  try {
    (void)parseProject(xml, options);
  } catch (const Error& error) {
    return error.what();
  }
  return {};
}

}  // namespace

TEST_SUITE("xml") {
  TEST_CASE("an empty project serializes to the expected document") {
    Project project;
    project.application = {"Test", "1.0"};
    const std::string xml = serialize(project);
    CHECK(xml ==
          "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
          "<Project version=\"1.0\">\n"
          "  <Application name=\"Test\" version=\"1.0\" />\n"
          "</Project>\n");
    testing::dumpForSchemaCheck("xml-empty-project", xml);
    const Project parsed = parseProject(xml);
    CHECK(parsed.version == "1.0");
    CHECK(parsed.application.name == "Test");
    CHECK(parsed.structure.empty());
    CHECK_FALSE(parsed.arrangement.has_value());
  }

  TEST_CASE("metadata round trip keeps every field and the schema order") {
    MetaData metadata;
    metadata.title = "Song & Dance";
    metadata.artist = "Artist";
    metadata.album = "Album";
    metadata.originalArtist = "Original";
    metadata.composer = "Composer";
    metadata.songwriter = "Songwriter";
    metadata.producer = "Producer";
    metadata.arranger = "Arranger";
    metadata.year = "2026";
    metadata.genre = "Electronic";
    metadata.copyright = "(c) 2026";
    metadata.website = "https://example.com";
    metadata.comment = "<comment>";
    const std::string xml = serialize(metadata);
    testing::dumpForSchemaCheck("xml-metadata-full", xml);
    CHECK(xml.find("<Title>") < xml.find("<Artist>"));
    CHECK(xml.find("<Website>") < xml.find("<Comment>"));
    ParseOptions strict;
    strict.strict = true;
    const MetaData parsed = parseMetaData(xml, strict);
    CHECK(parsed.title == "Song & Dance");
    CHECK(parsed.comment == "<comment>");
    CHECK(parsed.year == "2026");
    CHECK(serialize(parsed) == xml);

    const MetaData empty = parseMetaData("<MetaData/>");
    CHECK_FALSE(empty.title.has_value());
    testing::dumpForSchemaCheck("xml-metadata-empty", serialize(MetaData{}));
  }

  TEST_CASE("malformed XML throws an Xml error") {
    CHECK(errorKindOf("<Project version=\"1.0\"><Application", false) == ErrorKind::Xml);
    CHECK(errorKindOf("", false) == ErrorKind::Xml);
    CHECK_THROWS_AS((void)parseMetaData("<MetaData><Title></MetaData>"), Error);
  }

  TEST_CASE("missing required attributes throw in both modes") {
    const char* noVersion = R"(<Project><Application name="a" version="1"/></Project>)";
    CHECK(errorKindOf(noVersion, false) == ErrorKind::Schema);
    CHECK(errorKindOf(noVersion, true) == ErrorKind::Schema);

    const char* noUnit = R"(<Project version="1.0"><Application name="a" version="1"/>
      <Transport><Tempo value="120"/></Transport></Project>)";
    CHECK(errorKindOf(noUnit, false) == ErrorKind::Schema);
    CHECK(errorMessageOf(noUnit, false).find("Project/Transport/Tempo") != std::string::npos);
    CHECK(errorMessageOf(noUnit, false).find("unit") != std::string::npos);

    const char* noApplication = R"(<Project version="1.0"/>)";
    CHECK(errorKindOf(noApplication, false) == ErrorKind::Schema);

    const char* wrongRoot = R"(<MetaData/>)";
    CHECK(errorKindOf(wrongRoot, false) == ErrorKind::Schema);

    const char* noteWithoutKey = R"(<Project version="1.0"><Application name="a" version="1"/>
      <Arrangement><Lanes><Notes><Note time="0" duration="1" channel="0"/></Notes></Lanes>
      </Arrangement></Project>)";
    CHECK(errorKindOf(noteWithoutKey, false) == ErrorKind::Schema);
  }

  TEST_CASE("values of the wrong type throw") {
    const char* badDouble = R"(<Project version="1.0"><Application name="a" version="1"/>
      <Transport><Tempo unit="bpm" value="fast"/></Transport></Project>)";
    CHECK(errorKindOf(badDouble, false) == ErrorKind::Schema);
    const char* badInt = R"(<Project version="1.0"><Application name="a" version="1"/>
      <Structure><Channel audioChannels="two"/></Structure></Project>)";
    CHECK(errorKindOf(badInt, false) == ErrorKind::Schema);
    const char* badBool = R"(<Project version="1.0"><Application name="a" version="1"/>
      <Structure><Channel solo="yes"/></Structure></Project>)";
    CHECK(errorKindOf(badBool, false) == ErrorKind::Schema);
  }

  TEST_CASE("unknown elements, attributes and enum values: lenient ignores, strict throws") {
    const char* unknownElement = R"(<Project version="1.0"><Application name="a" version="1"/>
      <Structure><Track id="t"><Hologram/></Track></Structure></Project>)";
    CHECK_NOTHROW((void)parseProject(unknownElement));
    CHECK(errorKindOf(unknownElement, true) == ErrorKind::Schema);
    CHECK(errorMessageOf(unknownElement, true).find("Project/Structure/Track/Hologram") !=
          std::string::npos);

    const char* unknownAttribute = R"(<Project version="1.0"><Application name="a" version="1"/>
      <Structure><Channel id="c" sparkle="1"/></Structure></Project>)";
    const Project lenient = parseProject(unknownAttribute);
    CHECK(std::get<Channel>(lenient.structure.at(0)).id == "c");
    CHECK(errorMessageOf(unknownAttribute, true).find("sparkle") != std::string::npos);

    const char* unknownEnum = R"(<Project version="1.0"><Application name="a" version="1"/>
      <Structure><Channel role="quantum"/></Structure></Project>)";
    CHECK_FALSE(std::get<Channel>(parseProject(unknownEnum).structure.at(0)).role.has_value());
    CHECK(errorKindOf(unknownEnum, true) == ErrorKind::Schema);

    const char* unknownRequiredEnum = R"(<Project version="1.0"><Application name="a" version="1"/>
      <Transport><Tempo unit="furlongs" value="1"/></Transport></Project>)";
    CHECK(errorKindOf(unknownRequiredEnum, false) == ErrorKind::Schema);

    const char* namespaced = R"(<Project version="1.0"
      xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"><Application name="a" version="1"/>
      </Project>)";
    ParseOptions strict;
    strict.strict = true;
    CHECK_NOTHROW((void)parseProject(namespaced, strict));
  }

  TEST_CASE("markers use the schema element names") {
    Project project;
    project.application = {"Test", "1"};
    project.arrangement.emplace();
    project.arrangement->markers.emplace();
    Marker intro;
    intro.name = "Intro";
    project.arrangement->markers->markers.push_back(intro);
    project.arrangement->lanes = std::make_unique<Lanes>();
    auto laneMarkers = std::make_unique<Markers>();
    Marker drop;
    drop.name = "Drop";
    drop.time = 16.0;
    laneMarkers->markers.push_back(drop);
    project.arrangement->lanes->lanes.push_back(std::move(laneMarkers));
    const std::string xml = serialize(project);
    CHECK(xml.find("<Markers>") != std::string::npos);
    CHECK(xml.find("<markers>") != std::string::npos);
    CHECK(xml.find("<Lanes>") < xml.find("<Markers>"));
    const Project parsed = testing::roundTrip(project, "xml-markers");
    CHECK(parsed.arrangement->markers->markers.at(0).name == "Intro");
    const auto* lane = as<Markers>(parsed.arrangement->lanes->lanes.at(0).get());
    REQUIRE(lane != nullptr);
    CHECK(lane->markers.at(0).time == 16.0);
  }

  TEST_CASE("special characters and infinities survive a round trip") {
    Project project;
    project.application = {"App \"quoted\" & <tagged>", "1"};
    Track track;
    track.id = "t0";
    track.name = "Bass & Drums";
    track.comment = "line one\nline two";
    track.channel.emplace();
    RealParameter volume(Unit::Decibel, -std::numeric_limits<double>::infinity());
    volume.min = -std::numeric_limits<double>::infinity();
    volume.max = 6.0;
    track.channel->volume = volume;
    project.structure.emplace_back(std::move(track));
    const Project parsed = testing::roundTrip(project, "xml-escaping");
    CHECK(parsed.application.name == "App \"quoted\" & <tagged>");
    const Track& t = std::get<Track>(parsed.structure.at(0));
    CHECK(t.name == "Bass & Drums");
    CHECK(t.comment == "line one\nline two");
    CHECK(std::isinf(*t.channel->volume->value));
    CHECK(*t.channel->volume->value < 0);
    CHECK(serialize(parsed).find("value=\"-inf\"") != std::string::npos);
  }

  TEST_CASE("deep nesting throws a Schema error instead of exhausting the stack") {
    const auto nested = [](int depth) {
      std::string xml =
          R"(<Project version="1.0"><Application name="a" version="1"/><Arrangement>)";
      for (int i = 0; i < depth; ++i) {
        xml += "<Lanes>";
      }
      for (int i = 0; i < depth; ++i) {
        xml += "</Lanes>";
      }
      return xml + "</Arrangement></Project>";
    };
    CHECK_NOTHROW((void)parseProject(nested(100)));
    const std::string deep = nested(20000);
    CHECK(errorKindOf(deep, false) == ErrorKind::Schema);
    CHECK(errorKindOf(deep, true) == ErrorKind::Schema);
    CHECK(errorMessageOf(deep, false).find("nesting too deep") != std::string::npos);

    std::string tracks = R"(<Project version="1.0"><Application name="a" version="1"/><Structure>)";
    for (int i = 0; i < 20000; ++i) {
      tracks += "<Track>";
    }
    for (int i = 0; i < 20000; ++i) {
      tracks += "</Track>";
    }
    tracks += "</Structure></Project>";
    CHECK(errorMessageOf(tracks, false).find("nesting too deep") != std::string::npos);
  }

  TEST_CASE("infinite xs:double attributes use the XSD spelling INF") {
    const double inf = std::numeric_limits<double>::infinity();
    Project project;
    project.application = {"Test", "1"};
    project.arrangement.emplace();
    project.arrangement->lanes = std::make_unique<Lanes>();
    auto clips = std::make_unique<Clips>();
    Clip clip;
    clip.time = -inf;
    clip.duration = inf;
    clip.playStop = inf;
    clip.loopEnd = inf;
    auto notes = std::make_unique<Notes>();
    Note note;
    note.duration = inf;
    note.key = 60;
    notes->notes.push_back(std::move(note));
    clip.content = std::move(notes);
    clips->clips.push_back(std::move(clip));
    project.arrangement->lanes->lanes.push_back(std::move(clips));
    project.arrangement->markers.emplace();
    Marker marker;
    marker.time = inf;
    project.arrangement->markers->markers.push_back(marker);
    const std::string xml = serialize(project);
    CHECK(xml.find("time=\"-INF\" duration=\"INF\"") != std::string::npos);
    CHECK(xml.find("playStop=\"INF\"") != std::string::npos);
    CHECK(xml.find("<Marker time=\"INF\"") != std::string::npos);
    // xs:string attributes keep the spelling of the reference implementation.
    CHECK(xml.find("duration=\"inf\"") != std::string::npos);
    const Project parsed = testing::roundTrip(project, "xml-infinite-clip");
    const auto* lane = as<Clips>(parsed.arrangement->lanes->lanes.at(0).get());
    REQUIRE(lane != nullptr);
    CHECK(*lane->clips.at(0).duration == inf);
    CHECK(lane->clips.at(0).time == -inf);
  }

  TEST_CASE("control characters in written strings throw a Schema error naming the field") {
    const auto messageOf = [](const Project& project) -> std::string {
      try {
        (void)serialize(project);
      } catch (const Error& error) {
        CHECK(error.kind() == ErrorKind::Schema);
        return error.what();
      }
      return {};
    };
    Project project;
    project.application = {"App\x01", "1"};
    CHECK(messageOf(project).find("name") != std::string::npos);

    project.application = {"App", "1"};
    Track track;
    track.comment = std::string("tab\there\r\nok");
    project.structure.emplace_back(std::move(track));
    CHECK_NOTHROW((void)serialize(project));
    std::get<Track>(project.structure.at(0)).comment = std::string("nul\0inside", 10);
    CHECK(messageOf(project).find("comment") != std::string::npos);
    std::get<Track>(project.structure.at(0)).comment = "escape\x1b";
    CHECK(messageOf(project).find("comment") != std::string::npos);
    std::get<Track>(project.structure.at(0)).comment = "noncharacter \xEF\xBF\xBE";
    CHECK(messageOf(project).find("comment") != std::string::npos);
    std::get<Track>(project.structure.at(0)).comment = "noncharacter \xEF\xBF\xBF";
    CHECK(messageOf(project).find("comment") != std::string::npos);
    std::get<Track>(project.structure.at(0)).comment = "fine \xEF\xBF\xBD \xC3\xA9";
    CHECK_NOTHROW((void)serialize(project));

    MetaData metadata;
    metadata.title = "bell\x07";
    try {
      (void)serialize(metadata);
      FAIL("no error thrown");
    } catch (const Error& error) {
      CHECK(error.kind() == ErrorKind::Schema);
      CHECK(std::string(error.what()).find("Title") != std::string::npos);
    }
  }

  TEST_CASE("enum labels that cannot round trip through the list are refused") {
    Project project;
    project.application = {"App", "1"};
    Channel channel;
    auto device = std::make_unique<GenericDevice>();
    device->deviceName = "Synth";
    auto parameter = std::make_unique<EnumParameter>();
    parameter->count = 2;
    parameter->labels = {"Low", "High pass"};
    device->parameters.push_back(std::move(parameter));
    channel.devices.push_back(std::move(device));
    project.structure.emplace_back(std::move(channel));
    CHECK_THROWS_AS((void)serialize(project), Error);
    auto& labels = static_cast<EnumParameter&>(
                       *std::get<Channel>(project.structure.at(0)).devices.at(0)->parameters.at(0))
                       .labels;
    labels = {"Low", ""};
    CHECK_THROWS_AS((void)serialize(project), Error);
    labels = {"Low", "High"};
    testing::roundTrip(project, "xml-enum-labels");
  }

  TEST_CASE("strict mode: markers element names and repeated single children") {
    const auto project = [](const std::string& body) {
      return R"(<Project version="1.0"><Application name="a" version="1"/>)" + body + "</Project>";
    };
    // <Markers> is the Arrangement child only; timelines elsewhere are <markers>.
    const std::string laneMarkers = project(
        R"(<Arrangement><Lanes><Markers><Marker time="0"/></Markers></Lanes></Arrangement>)");
    CHECK(errorKindOf(laneMarkers, true) == ErrorKind::Schema);
    CHECK_NOTHROW((void)parseProject(laneMarkers));
    const std::string arrangementMarkers =
        project(R"(<Arrangement><markers><Marker time="0"/></markers></Arrangement>)");
    CHECK(errorKindOf(arrangementMarkers, true) == ErrorKind::Schema);
    const std::string good =
        project(R"(<Arrangement><Lanes><markers><Marker time="0"/></markers></Lanes>)"
                R"(<Markers><Marker time="1"/></Markers></Arrangement>)");
    ParseOptions strict;
    strict.strict = true;
    CHECK_NOTHROW((void)parseProject(good, strict));

    const std::string repeated[] = {
        R"(<Project version="1.0"><Application name="a" version="1"/>)"
        R"(<Application name="b" version="2"/></Project>)",
        project("<Transport/><Transport/>"),
        project(
            R"(<Transport><Tempo unit="bpm" value="1"/><Tempo unit="bpm" value="2"/></Transport>)"),
        project(R"(<Structure><Channel><Volume unit="linear"/><Volume unit="linear"/></Channel>)"
                "</Structure>"),
        project(R"(<Structure><Track><Channel/><Channel/></Track></Structure>)"),
        project("<Arrangement/><Arrangement/>"),
        project("<Arrangement><Lanes/><Lanes/></Arrangement>"),
        project(R"(<Structure><Channel><Devices><Device deviceName="d" deviceRole="audioFX">)"
                R"(<Enabled/><Enabled/></Device></Devices></Channel></Structure>)"),
    };
    for (const std::string& xml : repeated) {
      CAPTURE(xml);
      CHECK(errorKindOf(xml, true) == ErrorKind::Schema);
      CHECK(errorMessageOf(xml, true).find("more than once") != std::string::npos);
      CHECK_NOTHROW((void)parseProject(xml));
    }

    ParseOptions strictMeta;
    strictMeta.strict = true;
    CHECK_THROWS_AS(
        (void)parseMetaData("<MetaData><Title>a</Title><Title>b</Title></MetaData>", strictMeta),
        Error);
  }
}
