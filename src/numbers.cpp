// SPDX-License-Identifier: MIT
// Locale independent number formatting and parsing.
#include "numbers.hpp"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <locale>
#include <sstream>
#include <system_error>

#include "dawproject/xml.hpp"

// std::to_chars / std::from_chars for floating point arrived late in some standard libraries
// (libc++ before 17, for example); fall back to stream based code there.
#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
#define DAWPROJECT_HAS_FLOAT_CHARCONV 1
#elif defined(_MSC_VER) && _MSC_VER >= 1924
#define DAWPROJECT_HAS_FLOAT_CHARCONV 1
#elif defined(__GLIBCXX__) && defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE >= 11
#define DAWPROJECT_HAS_FLOAT_CHARCONV 1
#else
#define DAWPROJECT_HAS_FLOAT_CHARCONV 0
#endif

namespace dawproject {
namespace {

bool equalsIgnoreCase(std::string_view a, std::string_view b) noexcept {
  if (a.size() != b.size()) {
    return false;
  }
  for (std::size_t i = 0; i < a.size(); ++i) {
    const char x = (a[i] >= 'A' && a[i] <= 'Z') ? static_cast<char>(a[i] - 'A' + 'a') : a[i];
    const char y = (b[i] >= 'A' && b[i] <= 'Z') ? static_cast<char>(b[i] - 'A' + 'a') : b[i];
    if (x != y) {
      return false;
    }
  }
  return true;
}

std::string_view trim(std::string_view text) noexcept {
  const auto isSpace = [](char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; };
  while (!text.empty() && isSpace(text.front())) {
    text.remove_prefix(1);
  }
  while (!text.empty() && isSpace(text.back())) {
    text.remove_suffix(1);
  }
  return text;
}

bool isDigit(char c) noexcept { return c >= '0' && c <= '9'; }

// For unsigned decimal text too large or too small for a double: true and the saturated value
// (infinity on overflow, zero on underflow), like strtod. False when the text is not a number.
bool saturate(std::string_view text, double& out) noexcept {
  std::size_t i = 0;
  long long magnitude = 0;  // decimal exponent of the first significant digit
  bool seenSignificant = false;
  bool anyDigit = false;
  long long digitsBeforePoint = 0;
  long long zerosAfterPoint = 0;
  for (; i < text.size() && isDigit(text[i]); ++i) {
    anyDigit = true;
    if (text[i] != '0' || seenSignificant) {
      seenSignificant = true;
      ++digitsBeforePoint;
    }
  }
  if (i < text.size() && text[i] == '.') {
    for (++i; i < text.size() && isDigit(text[i]); ++i) {
      anyDigit = true;
      if (!seenSignificant) {
        if (text[i] == '0') {
          ++zerosAfterPoint;
        } else {
          seenSignificant = true;
        }
      }
    }
  }
  if (!anyDigit) {
    return false;
  }
  long long exponent = 0;
  if (i < text.size() && (text[i] == 'e' || text[i] == 'E')) {
    ++i;
    bool negative = false;
    if (i < text.size() && (text[i] == '+' || text[i] == '-')) {
      negative = text[i] == '-';
      ++i;
    }
    if (i == text.size()) {
      return false;
    }
    for (; i < text.size() && isDigit(text[i]); ++i) {
      if (exponent < 1000000000LL) {
        exponent = exponent * 10 + (text[i] - '0');
      }
    }
    exponent = negative ? -exponent : exponent;
  }
  if (i != text.size()) {
    return false;
  }
  if (!seenSignificant) {
    out = 0.0;
    return true;
  }
  magnitude = (digitsBeforePoint > 0 ? digitsBeforePoint - 1 : -(zerosAfterPoint + 1)) + exponent;
  out = magnitude > 0 ? std::numeric_limits<double>::infinity() : 0.0;
  return true;
}

#if !DAWPROJECT_HAS_FLOAT_CHARCONV
bool streamParse(std::string_view text, double& out) {
  std::istringstream in{std::string(text)};
  in.imbue(std::locale::classic());
  double value = 0.0;
  in >> value;
  if (in.fail() || !in.eof()) {
    return false;
  }
  out = value;
  return true;
}

std::string streamFormat(double value, int precision) {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out.precision(precision);
  out << value;
  return out.str();
}
#endif

}  // namespace

std::string formatDouble(double value) {
  if (std::isnan(value)) {
    return "NaN";
  }
  if (std::isinf(value)) {
    return value > 0 ? "inf" : "-inf";
  }
  if (value == 0.0) {
    return std::signbit(value) ? "-0" : "0";
  }
#if DAWPROJECT_HAS_FLOAT_CHARCONV
  char buffer[64];
  const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
  return std::string(buffer, result.ptr);
#else
  for (int precision = 1; precision < 17; ++precision) {
    std::string text = streamFormat(value, precision);
    double back = 0.0;
    if (streamParse(text, back) && back == value) {
      return text;
    }
  }
  return streamFormat(value, 17);
#endif
}

bool parseDouble(std::string_view text, double& out) noexcept {
  text = trim(text);
  if (text.empty()) {
    return false;
  }
  bool negative = false;
  std::string_view body = text;
  if (body.front() == '+' || body.front() == '-') {
    negative = body.front() == '-';
    body.remove_prefix(1);
  }
  if (equalsIgnoreCase(body, "inf") || equalsIgnoreCase(body, "infinity")) {
    out = negative ? -std::numeric_limits<double>::infinity()
                   : std::numeric_limits<double>::infinity();
    return true;
  }
  if (equalsIgnoreCase(body, "nan")) {
    out = std::numeric_limits<double>::quiet_NaN();
    return true;
  }
  if (body.empty() || !((body.front() >= '0' && body.front() <= '9') || body.front() == '.')) {
    return false;
  }
#if DAWPROJECT_HAS_FLOAT_CHARCONV
  double value = 0.0;
  const auto result = std::from_chars(body.data(), body.data() + body.size(), value);
  if (result.ec == std::errc::result_out_of_range) {
    if (!saturate(body, value)) {
      return false;
    }
  } else if (result.ec != std::errc() || result.ptr != body.data() + body.size()) {
    return false;
  }
  out = negative ? -value : value;
  return true;
#else
  try {
    double value = 0.0;
    if (!streamParse(body, value)) {
      // A syntactically valid number the stream rejects is out of range: saturate it.
      if (!saturate(body, value)) {
        return false;
      }
    }
    out = negative ? -value : value;
    return true;
  } catch (...) {
    return false;
  }
#endif
}

namespace detail {

bool parseInt(std::string_view text, int& out) noexcept {
  text = trim(text);
  if (!text.empty() && text.front() == '+') {
    text.remove_prefix(1);
  }
  int value = 0;
  const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
  if (text.empty() || result.ec != std::errc() || result.ptr != text.data() + text.size()) {
    return false;
  }
  out = value;
  return true;
}

bool parseBool(std::string_view text, bool& out) noexcept {
  text = trim(text);
  if (text == "true" || text == "1") {
    out = true;
    return true;
  }
  if (text == "false" || text == "0") {
    out = false;
    return true;
  }
  return false;
}

}  // namespace detail
}  // namespace dawproject
