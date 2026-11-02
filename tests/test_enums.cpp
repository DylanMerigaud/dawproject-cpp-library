// SPDX-License-Identifier: MIT
#include <functional>
#include <map>
#include <regex>
#include <set>

#include "doctest.h"
#include "test_support.hpp"

using namespace dawproject;

namespace {

// simpleType name in the XSD -> list of values read from the XSD.
std::map<std::string, std::vector<std::string>> xsdEnumerations() {
  const std::string xsd = testing::readText(testing::specDir() / "Project.xsd");
  std::map<std::string, std::vector<std::string>> result;
  const std::regex typeRe(R"re(<xs:simpleType name="([A-Za-z]+)">([\s\S]*?)</xs:simpleType>)re");
  const std::regex valueRe(R"re(<xs:enumeration value="([^"]+)"/>)re");
  for (auto it = std::sregex_iterator(xsd.begin(), xsd.end(), typeRe); it != std::sregex_iterator();
       ++it) {
    const std::string body = (*it)[2].str();
    auto& values = result[(*it)[1].str()];
    for (auto v = std::sregex_iterator(body.begin(), body.end(), valueRe);
         v != std::sregex_iterator(); ++v) {
      values.push_back((*v)[1].str());
    }
  }
  return result;
}

template <class E>
void checkEnum(const std::vector<std::string>& xsdValues) {
  CHECK(all_values<E>().size() == xsdValues.size());
  std::set<std::string> written;
  for (const std::string& spelling : xsdValues) {
    const std::optional<E> parsed = from_string<E>(spelling);
    REQUIRE_MESSAGE(parsed.has_value(), "no enum value for XSD spelling " << spelling);
    CHECK(to_string(*parsed) == spelling);
  }
  for (const E value : all_values<E>()) {
    written.insert(std::string(to_string(value)));
  }
  CHECK(written == std::set<std::string>(xsdValues.begin(), xsdValues.end()));
}

}  // namespace

TEST_SUITE("enums") {
  TEST_CASE("every XSD enumeration value maps both ways") {
    const auto enumerations = xsdEnumerations();
    const std::map<std::string, std::function<void(const std::vector<std::string>&)>> checks = {
        {"unit", checkEnum<Unit>},
        {"timeUnit", checkEnum<TimeUnit>},
        {"deviceRole", checkEnum<DeviceRole>},
        {"eqBandType", checkEnum<EqBandType>},
        {"mixerRole", checkEnum<MixerRole>},
        {"sendType", checkEnum<SendType>},
        {"contentType", checkEnum<ContentType>},
        {"interpolation", checkEnum<Interpolation>},
        {"expressionType", checkEnum<ExpressionType>},
    };
    REQUIRE(enumerations.size() >= checks.size());
    for (const auto& enumeration : enumerations) {
      // Named copies: doctest's macros capture them in lambdas, which structured bindings
      // cannot be before C++20.
      const std::string name = enumeration.first;
      const auto& values = enumeration.second;
      CAPTURE(name);
      const auto check = checks.find(name);
      REQUIRE_MESSAGE(check != checks.end(), "XSD enumeration without a C++ enum: " << name);
      REQUIRE_FALSE(values.empty());
      check->second(values);
    }
  }

  TEST_CASE("unknown and differently cased spellings are rejected") {
    CHECK_FALSE(from_string<Unit>("Decibel").has_value());
    CHECK_FALSE(from_string<Unit>("").has_value());
    CHECK_FALSE(from_string<DeviceRole>("audiofx").has_value());
    CHECK(from_string<DeviceRole>("audioFX") == DeviceRole::AudioFX);
    CHECK(to_string(ExpressionType::PitchBend) == "pitchBend");
  }
}

TEST_SUITE("version") {
  TEST_CASE("library and format versions") {
    CHECK(kLibraryVersion == DAWPROJECT_TEST_PROJECT_VERSION);
    CHECK(kFormatVersion == "1.0");
    CHECK(Project{}.version == "1.0");
  }
}
