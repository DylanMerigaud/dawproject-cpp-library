// SPDX-License-Identifier: MIT
/// @file enums.hpp
/// Enumerations of the DAWproject schema and their exact XML spellings.
#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include "dawproject/export.hpp"

namespace dawproject {

/// Unit of a RealParameter or of automation Points.
enum class Unit { Linear, Normalized, Percent, Decibel, Hertz, Semitones, Seconds, Beats, Bpm };

/// Unit of time of a timeline.
enum class TimeUnit { Beats, Seconds };

/// Role of a device in a channel.
enum class DeviceRole { Instrument, NoteFX, AudioFX, Analyzer };

/// Filter shape of an equalizer band.
enum class EqBandType { HighPass, LowPass, BandPass, HighShelf, LowShelf, Bell, Notch };

/// Role of a mixer channel.
enum class MixerRole { Regular, Master, Effect, Submix, Vca };

/// Whether a send taps the signal before or after the channel fader.
enum class SendType { Pre, Post };

/// Kind of content a track holds.
enum class ContentType { Audio, Automation, Notes, Video, Markers, Tracks };

/// Interpolation from one automation point to the next.
enum class Interpolation { Hold, Linear };

/// Per-note or MIDI expression an automation lane targets.
enum class ExpressionType {
  Gain,
  Pan,
  Transpose,
  Timbre,
  Formant,
  Pressure,
  ChannelController,
  ChannelPressure,
  PolyPressure,
  PitchBend,
  ProgramChange,
};

/// XML spelling of a Unit, for example "decibel".
[[nodiscard]] DAWPROJECT_EXPORT std::string_view to_string(Unit value) noexcept;
/// XML spelling of a TimeUnit, for example "beats".
[[nodiscard]] DAWPROJECT_EXPORT std::string_view to_string(TimeUnit value) noexcept;
/// XML spelling of a DeviceRole, for example "audioFX".
[[nodiscard]] DAWPROJECT_EXPORT std::string_view to_string(DeviceRole value) noexcept;
/// XML spelling of an EqBandType, for example "highShelf".
[[nodiscard]] DAWPROJECT_EXPORT std::string_view to_string(EqBandType value) noexcept;
/// XML spelling of a MixerRole, for example "master".
[[nodiscard]] DAWPROJECT_EXPORT std::string_view to_string(MixerRole value) noexcept;
/// XML spelling of a SendType, for example "post".
[[nodiscard]] DAWPROJECT_EXPORT std::string_view to_string(SendType value) noexcept;
/// XML spelling of a ContentType, for example "notes".
[[nodiscard]] DAWPROJECT_EXPORT std::string_view to_string(ContentType value) noexcept;
/// XML spelling of an Interpolation, for example "hold".
[[nodiscard]] DAWPROJECT_EXPORT std::string_view to_string(Interpolation value) noexcept;
/// XML spelling of an ExpressionType, for example "pitchBend".
[[nodiscard]] DAWPROJECT_EXPORT std::string_view to_string(ExpressionType value) noexcept;

/// Parses the exact XML spelling of an enumeration value; std::nullopt when it is not one.
/// Specialised for every enumeration of this header.
template <class E>
[[nodiscard]] std::optional<E> from_string(std::string_view text) noexcept;

/// Every value of an enumeration, in schema order. Specialised for every enumeration of this
/// header.
template <class E>
[[nodiscard]] const std::vector<E>& all_values();

/// @cond
#define DAWPROJECT_DECLARE_ENUM_FUNCTIONS(E)                                                  \
  template <>                                                                                 \
  [[nodiscard]] DAWPROJECT_EXPORT std::optional<E> from_string<E>(std::string_view) noexcept; \
  template <>                                                                                 \
  [[nodiscard]] DAWPROJECT_EXPORT const std::vector<E>& all_values<E>();

DAWPROJECT_DECLARE_ENUM_FUNCTIONS(Unit)
DAWPROJECT_DECLARE_ENUM_FUNCTIONS(TimeUnit)
DAWPROJECT_DECLARE_ENUM_FUNCTIONS(DeviceRole)
DAWPROJECT_DECLARE_ENUM_FUNCTIONS(EqBandType)
DAWPROJECT_DECLARE_ENUM_FUNCTIONS(MixerRole)
DAWPROJECT_DECLARE_ENUM_FUNCTIONS(SendType)
DAWPROJECT_DECLARE_ENUM_FUNCTIONS(ContentType)
DAWPROJECT_DECLARE_ENUM_FUNCTIONS(Interpolation)
DAWPROJECT_DECLARE_ENUM_FUNCTIONS(ExpressionType)

#undef DAWPROJECT_DECLARE_ENUM_FUNCTIONS
/// @endcond

}  // namespace dawproject
