// SPDX-License-Identifier: MIT
#include "doctest.h"
#include "test_support.hpp"

using namespace dawproject;

namespace {

Project cleanProject() {
  Project project;
  project.application = {"Test", "1"};
  Track track;
  track.id = "track";
  track.channel.emplace();
  track.channel->id = "channel";
  track.channel->volume = RealParameter(Unit::Linear, 1.0);
  track.channel->volume->id = "volume";
  project.structure.emplace_back(std::move(track));
  Channel master;
  master.id = "master";
  project.structure.emplace_back(std::move(master));
  project.arrangement.emplace();
  project.arrangement->lanes = std::make_unique<Lanes>();
  return project;
}

bool hasIssue(const std::vector<Issue>& issues, const std::string& needle) {
  for (const Issue& issue : issues) {
    if (issue.message.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_SUITE("validate") {
  TEST_CASE("a clean project has no issue") { CHECK(validate(cleanProject()).empty()); }

  TEST_CASE("duplicate ids") {
    Project project = cleanProject();
    std::get<Channel>(project.structure[1]).id = "track";
    const auto issues = validate(project);
    REQUIRE(issues.size() == 1);
    CHECK(hasIssue(issues, "duplicate id 'track'"));
    CHECK(issues[0].path == "Structure[1]/Channel");
  }

  TEST_CASE("references naming no id") {
    Project project = cleanProject();
    auto& channel = *std::get<Track>(project.structure[0]).channel;
    channel.destination = "nowhere";
    Send send;
    send.destination = "missingSend";
    channel.sends.push_back(send);
    auto clips = std::make_unique<Clips>();
    clips->track = "missingTrack";
    Clip alias;
    alias.reference = "missingClip";
    clips->clips.push_back(std::move(alias));
    project.arrangement->lanes->lanes.push_back(std::move(clips));
    auto points = std::make_unique<Points>();
    points->target.parameter = "missingParameter";
    project.arrangement->lanes->lanes.push_back(std::move(points));

    const auto issues = validate(project);
    CHECK(issues.size() == 5);
    CHECK(hasIssue(issues, "'destination' references unknown id 'nowhere'"));
    CHECK(hasIssue(issues, "unknown id 'missingSend'"));
    CHECK(hasIssue(issues, "'track' references unknown id 'missingTrack'"));
    CHECK(hasIssue(issues, "'reference' references unknown id 'missingClip'"));
    CHECK(hasIssue(issues, "'parameter' references unknown id 'missingParameter'"));

    // Valid references produce no issue.
    Project valid = cleanProject();
    auto lane = std::make_unique<Points>();
    lane->track = "track";
    lane->target.parameter = "volume";
    valid.arrangement->lanes->lanes.push_back(std::move(lane));
    std::get<Track>(valid.structure[0]).channel->destination = "master";
    CHECK(validate(valid).empty());
  }

  TEST_CASE("markers without a marker, warps without content, scene without content") {
    Project project = cleanProject();
    project.arrangement->markers.emplace();
    auto warps = std::make_unique<Warps>();
    project.arrangement->lanes->lanes.push_back(std::move(warps));
    project.scenes.emplace_back();
    const auto issues = validate(project);
    CHECK(hasIssue(issues, "markers timeline has no marker"));
    CHECK(hasIssue(issues, "warps timeline has no content"));
    CHECK(hasIssue(issues, "warps timeline has no warp point"));
    CHECK(hasIssue(issues, "scene has no content"));
  }

  TEST_CASE("ids inside devices and nested timelines are collected") {
    Project project = cleanProject();
    auto device = std::make_unique<Compressor>();
    device->deviceName = "Comp";
    device->threshold = RealParameter(Unit::Decibel, -10.0);
    device->threshold->id = "threshold";
    std::get<Track>(project.structure[0]).channel->devices.push_back(std::move(device));
    auto clips = std::make_unique<Clips>();
    Clip clip;
    auto automation = std::make_unique<Points>();
    automation->target.parameter = "threshold";
    clip.content = std::move(automation);
    clips->clips.push_back(std::move(clip));
    project.arrangement->lanes->lanes.push_back(std::move(clips));
    CHECK(validate(project).empty());
  }

  TEST_CASE("ids and references must be NCNames") {
    Project project = cleanProject();
    std::get<Track>(project.structure.at(0)).id = "1";
    std::get<Channel>(project.structure.at(1)).destination = "has space";
    project.arrangement->lanes->id = "a:b";
    const std::vector<Issue> issues = validate(project);
    CHECK(hasIssue(issues, "'1' is not a valid id"));
    CHECK(hasIssue(issues, "'a:b' is not a valid id"));
    CHECK(hasIssue(issues, "'has space' is not a valid id"));

    Project fine = cleanProject();
    std::get<Track>(fine.structure.at(0)).id = "_t-1.x\xC3\xA9";
    CHECK(validate(fine).empty());
  }

  TEST_CASE("enum labels that cannot round trip are reported") {
    Project project = cleanProject();
    auto device = std::make_unique<GenericDevice>();
    device->deviceName = "Synth";
    auto parameter = std::make_unique<EnumParameter>();
    parameter->count = 3;
    parameter->labels = {"Low", "", "High pass"};
    device->parameters.push_back(std::move(parameter));
    std::get<Channel>(project.structure.at(1)).devices.push_back(std::move(device));
    const std::vector<Issue> issues = validate(project);
    CHECK(hasIssue(issues, "empty label"));
    CHECK(hasIssue(issues, "'High pass'"));
  }

  TEST_CASE("IdGenerator counts like the Java library") {
    IdGenerator ids;
    CHECK(ids.next() == "id0");
    CHECK(ids.next() == "id1");
    ids.reset();
    CHECK(ids.next() == "id0");
  }
}
