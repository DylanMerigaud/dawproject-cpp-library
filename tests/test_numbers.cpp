// SPDX-License-Identifier: MIT
#include <clocale>
#include <cmath>
#include <cstring>
#include <limits>
#include <random>

#include "doctest.h"
#include "test_support.hpp"

using namespace dawproject;

namespace {
double parsed(std::string_view text) {
  double value = 0.0;
  REQUIRE_MESSAGE(parseDouble(text, value), "could not parse " << text);
  return value;
}
}  // namespace

TEST_SUITE("numbers") {
  TEST_CASE("infinities read and write like the Java reference") {
    const double inf = std::numeric_limits<double>::infinity();
    CHECK(parsed("inf") == inf);
    CHECK(parsed("-inf") == -inf);
    CHECK(parsed("Infinity") == inf);
    CHECK(parsed("-Infinity") == -inf);
    CHECK(parsed("INF") == inf);
    CHECK(formatDouble(inf) == "inf");
    CHECK(formatDouble(-inf) == "-inf");
    CHECK(std::isnan(parsed("NaN")));
    CHECK(formatDouble(std::numeric_limits<double>::quiet_NaN()) == "NaN");
  }

  TEST_CASE("decimal values use the shortest exact form") {
    CHECK(formatDouble(0.1) == "0.1");
    CHECK(parsed("0.1") == 0.1);
    CHECK(formatDouble(8.0) == "8");
    CHECK(formatDouble(-0.5) == "-0.5");
    CHECK(formatDouble(0.0) == "0");
    CHECK(parsed("1e-300") == 1e-300);
    CHECK(parsed(formatDouble(1e-300)) == 1e-300);
    CHECK(parsed("155.000000") == 155.0);
    CHECK(parsed(" 2.5 ") == 2.5);
    CHECK(parsed("+3") == 3.0);
  }

  TEST_CASE("out of range numbers saturate like strtod") {
    const double inf = std::numeric_limits<double>::infinity();
    CHECK(parsed("1e400") == inf);
    CHECK(parsed("-1e400") == -inf);
    CHECK(parsed("1E+99999999999999999999") == inf);
    CHECK(parsed(std::string(400, '9')) == inf);
    const double tiny = parsed("1e-400");
    CHECK(tiny == 0.0);
    CHECK_FALSE(std::signbit(tiny));
    CHECK(std::signbit(parsed("-1e-400")));
    CHECK(parsed("0." + std::string(400, '0') + "1") == 0.0);
    CHECK(parsed("4.9406564584124654e-324") > 0.0);
    CHECK(parsed("4.9406564584124654e-324") == std::numeric_limits<double>::denorm_min());
    CHECK(parsed("2.2250738585072009e-308") > 0.0);
    CHECK(parsed("-5e-324") == -std::numeric_limits<double>::denorm_min());
    double value = 0.0;
    CHECK_FALSE(parseDouble("1e", value));
    CHECK_FALSE(parseDouble("1e400x", value));
  }

  TEST_CASE("invalid numbers are rejected") {
    double value = 0.0;
    CHECK_FALSE(parseDouble("", value));
    CHECK_FALSE(parseDouble("abc", value));
    CHECK_FALSE(parseDouble("1.5x", value));
    CHECK_FALSE(parseDouble("1,5", value));
    CHECK_FALSE(parseDouble("--1", value));
  }

  TEST_CASE("1000 random doubles round trip exactly") {
    std::mt19937_64 rng(20261102);
    std::uniform_int_distribution<std::uint64_t> bits;
    int checked = 0;
    while (checked < 1000) {
      const std::uint64_t raw = bits(rng);
      double value = 0.0;
      std::memcpy(&value, &raw, sizeof value);
      if (!std::isfinite(value)) {
        continue;
      }
      const std::string text = formatDouble(value);
      double back = 0.0;
      REQUIRE(parseDouble(text, back));
      REQUIRE_MESSAGE(std::memcmp(&back, &value, sizeof value) == 0, text);
      ++checked;
    }
    std::uniform_real_distribution<double> small(-1000.0, 1000.0);
    for (int i = 0; i < 1000; ++i) {
      const double value = small(rng);
      CHECK(parsed(formatDouble(value)) == value);
    }
  }

  TEST_CASE("formatting ignores the C locale") {
    const char* previous = std::setlocale(LC_NUMERIC, nullptr);
    const std::string saved = previous ? previous : "C";
    // A comma decimal locale, when the system has one.
    const bool switched = std::setlocale(LC_NUMERIC, "de_DE.UTF-8") != nullptr ||
                          std::setlocale(LC_NUMERIC, "fr_FR.UTF-8") != nullptr;
    CHECK(formatDouble(0.25) == "0.25");
    CHECK(parsed("0.25") == 0.25);
    std::setlocale(LC_NUMERIC, saved.c_str());
    if (!switched) {
      MESSAGE("no comma decimal locale installed, checked with the default locale only");
    }
  }
}
