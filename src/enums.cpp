// SPDX-License-Identifier: MIT
#include "dawproject/enums.hpp"

#include <array>
#include <cstddef>
#include <utility>

namespace dawproject {
namespace {

template <class E, std::size_t N>
using Table = std::array<std::pair<E, std::string_view>, N>;

constexpr Table<Unit, 9> kUnits{{
    {Unit::Linear, "linear"},
    {Unit::Normalized, "normalized"},
    {Unit::Percent, "percent"},
    {Unit::Decibel, "decibel"},
    {Unit::Hertz, "hertz"},
    {Unit::Semitones, "semitones"},
    {Unit::Seconds, "seconds"},
    {Unit::Beats, "beats"},
    {Unit::Bpm, "bpm"},
}};

constexpr Table<TimeUnit, 2> kTimeUnits{{
    {TimeUnit::Beats, "beats"},
    {TimeUnit::Seconds, "seconds"},
}};

constexpr Table<DeviceRole, 4> kDeviceRoles{{
    {DeviceRole::Instrument, "instrument"},
    {DeviceRole::NoteFX, "noteFX"},
    {DeviceRole::AudioFX, "audioFX"},
    {DeviceRole::Analyzer, "analyzer"},
}};

constexpr Table<EqBandType, 7> kEqBandTypes{{
    {EqBandType::HighPass, "highPass"},
    {EqBandType::LowPass, "lowPass"},
    {EqBandType::BandPass, "bandPass"},
    {EqBandType::HighShelf, "highShelf"},
    {EqBandType::LowShelf, "lowShelf"},
    {EqBandType::Bell, "bell"},
    {EqBandType::Notch, "notch"},
}};

constexpr Table<MixerRole, 5> kMixerRoles{{
    {MixerRole::Regular, "regular"},
    {MixerRole::Master, "master"},
    {MixerRole::Effect, "effect"},
    {MixerRole::Submix, "submix"},
    {MixerRole::Vca, "vca"},
}};

constexpr Table<SendType, 2> kSendTypes{{
    {SendType::Pre, "pre"},
    {SendType::Post, "post"},
}};

constexpr Table<ContentType, 6> kContentTypes{{
    {ContentType::Audio, "audio"},
    {ContentType::Automation, "automation"},
    {ContentType::Notes, "notes"},
    {ContentType::Video, "video"},
    {ContentType::Markers, "markers"},
    {ContentType::Tracks, "tracks"},
}};

constexpr Table<Interpolation, 2> kInterpolations{{
    {Interpolation::Hold, "hold"},
    {Interpolation::Linear, "linear"},
}};

constexpr Table<ExpressionType, 11> kExpressionTypes{{
    {ExpressionType::Gain, "gain"},
    {ExpressionType::Pan, "pan"},
    {ExpressionType::Transpose, "transpose"},
    {ExpressionType::Timbre, "timbre"},
    {ExpressionType::Formant, "formant"},
    {ExpressionType::Pressure, "pressure"},
    {ExpressionType::ChannelController, "channelController"},
    {ExpressionType::ChannelPressure, "channelPressure"},
    {ExpressionType::PolyPressure, "polyPressure"},
    {ExpressionType::PitchBend, "pitchBend"},
    {ExpressionType::ProgramChange, "programChange"},
}};

template <class E, std::size_t N>
std::string_view nameOf(const Table<E, N>& table, E value) noexcept {
  for (const auto& [entry, name] : table) {
    if (entry == value) {
      return name;
    }
  }
  return {};
}

template <class E, std::size_t N>
std::optional<E> valueOf(const Table<E, N>& table, std::string_view text) noexcept {
  for (const auto& [entry, name] : table) {
    if (name == text) {
      return entry;
    }
  }
  return std::nullopt;
}

template <class E, std::size_t N>
std::vector<E> valuesOf(const Table<E, N>& table) {
  std::vector<E> values;
  values.reserve(N);
  for (const auto& entry : table) {
    values.push_back(entry.first);
  }
  return values;
}

}  // namespace

#define DAWPROJECT_DEFINE_ENUM_FUNCTIONS(E, TABLE)                              \
  std::string_view to_string(E value) noexcept { return nameOf(TABLE, value); } \
  template <>                                                                   \
  std::optional<E> from_string<E>(std::string_view text) noexcept {             \
    return valueOf(TABLE, text);                                                \
  }                                                                             \
  template <>                                                                   \
  const std::vector<E>& all_values<E>() {                                       \
    static const std::vector<E> values = valuesOf(TABLE);                       \
    return values;                                                              \
  }

DAWPROJECT_DEFINE_ENUM_FUNCTIONS(Unit, kUnits)
DAWPROJECT_DEFINE_ENUM_FUNCTIONS(TimeUnit, kTimeUnits)
DAWPROJECT_DEFINE_ENUM_FUNCTIONS(DeviceRole, kDeviceRoles)
DAWPROJECT_DEFINE_ENUM_FUNCTIONS(EqBandType, kEqBandTypes)
DAWPROJECT_DEFINE_ENUM_FUNCTIONS(MixerRole, kMixerRoles)
DAWPROJECT_DEFINE_ENUM_FUNCTIONS(SendType, kSendTypes)
DAWPROJECT_DEFINE_ENUM_FUNCTIONS(ContentType, kContentTypes)
DAWPROJECT_DEFINE_ENUM_FUNCTIONS(Interpolation, kInterpolations)
DAWPROJECT_DEFINE_ENUM_FUNCTIONS(ExpressionType, kExpressionTypes)

#undef DAWPROJECT_DEFINE_ENUM_FUNCTIONS

}  // namespace dawproject
