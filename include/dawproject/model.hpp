// SPDX-License-Identifier: MIT
/// @file model.hpp
/// The typed object model of a DAWproject document. Every type mirrors one complex type of
/// spec/Project.xsd or spec/MetaData.xsd; optional XML attributes are std::optional members.
///
/// Types that own polymorphic children through std::unique_ptr are move-only and offer a
/// `clone()` that makes a deep copy.
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include "dawproject/enums.hpp"
#include "dawproject/export.hpp"

namespace dawproject {

// ---------------------------------------------------------------------------------------------
// Base fields

/// Fields shared by every named element: `name`, `color` and `comment` attributes.
struct Nameable {
  std::optional<std::string> name;     ///< Display name.
  std::optional<std::string> color;    ///< Color, usually "#rrggbb".
  std::optional<std::string> comment;  ///< Free text comment.
};

/// A named element that other elements can reference through its `id` attribute.
struct Referenceable : Nameable {
  std::optional<std::string> id;  ///< Unique identifier (xs:ID), referenced by IDREF attributes.
};

/// A reference to a file, inside the archive (`external` false or absent) or on disk.
struct FileReference {
  std::string path;              ///< Path inside the archive, or a path on disk when external.
  std::optional<bool> external;  ///< True when the file is outside the archive.
};

// ---------------------------------------------------------------------------------------------
// Parameters

/// Concrete type of a Parameter.
enum class ParameterKind { Real, Bool, Integer, Enum, TimeSignature };

/// Abstract base of every parameter.
class DAWPROJECT_EXPORT Parameter : public Referenceable {
 public:
  virtual ~Parameter() = default;

  /// The concrete type of this parameter.
  [[nodiscard]] virtual ParameterKind kind() const noexcept = 0;
  /// Deep copy of this parameter, keeping its concrete type.
  [[nodiscard]] virtual std::unique_ptr<Parameter> clone() const = 0;

  std::optional<int> parameterID;  ///< Plugin-specific parameter index.

 protected:
  Parameter() = default;
  Parameter(const Parameter&) = default;
  Parameter(Parameter&&) = default;
  Parameter& operator=(const Parameter&) = default;
  Parameter& operator=(Parameter&&) = default;
};

/// A real valued parameter (`<RealParameter>`, also `<Volume>`, `<Pan>`, `<Tempo>`, ...).
class DAWPROJECT_EXPORT RealParameter final : public Parameter {
 public:
  RealParameter() = default;
  /// A parameter with a unit and a value.
  RealParameter(Unit unitValue, double v) : value(v), unit(unitValue) {}

  [[nodiscard]] ParameterKind kind() const noexcept override { return ParameterKind::Real; }
  [[nodiscard]] std::unique_ptr<Parameter> clone() const override;

  std::optional<double> value;  ///< Current value.
  std::optional<double> min;    ///< Lower bound.
  std::optional<double> max;    ///< Upper bound.
  Unit unit = Unit::Linear;     ///< Unit of value, min and max (required).
};

/// A boolean parameter (`<BoolParameter>`, also `<Mute>`, `<Enabled>`, ...).
class DAWPROJECT_EXPORT BoolParameter final : public Parameter {
 public:
  BoolParameter() = default;
  /// A parameter with a value.
  explicit BoolParameter(bool v) : value(v) {}

  [[nodiscard]] ParameterKind kind() const noexcept override { return ParameterKind::Bool; }
  [[nodiscard]] std::unique_ptr<Parameter> clone() const override;

  std::optional<bool> value;  ///< Current value.
};

/// An integer parameter (`<IntegerParameter>`).
class DAWPROJECT_EXPORT IntegerParameter final : public Parameter {
 public:
  [[nodiscard]] ParameterKind kind() const noexcept override { return ParameterKind::Integer; }
  [[nodiscard]] std::unique_ptr<Parameter> clone() const override;

  std::optional<int> value;  ///< Current value.
  std::optional<int> min;    ///< Lower bound.
  std::optional<int> max;    ///< Upper bound.
};

/// A parameter choosing one of `count` labelled values (`<EnumParameter>`).
class DAWPROJECT_EXPORT EnumParameter final : public Parameter {
 public:
  [[nodiscard]] ParameterKind kind() const noexcept override { return ParameterKind::Enum; }
  [[nodiscard]] std::unique_ptr<Parameter> clone() const override;

  std::optional<int> value;  ///< Index of the current value.
  int count = 0;             ///< Number of values (required).
  /// Value labels. The attribute is a whitespace separated list, so a label cannot hold spaces.
  std::vector<std::string> labels;
};

/// A time signature parameter (`<TimeSignatureParameter>`, also `<TimeSignature>`).
class DAWPROJECT_EXPORT TimeSignatureParameter final : public Parameter {
 public:
  TimeSignatureParameter() = default;
  /// A time signature such as 4/4.
  TimeSignatureParameter(int num, int den) : numerator(num), denominator(den) {}

  [[nodiscard]] ParameterKind kind() const noexcept override {
    return ParameterKind::TimeSignature;
  }
  [[nodiscard]] std::unique_ptr<Parameter> clone() const override;

  int numerator = 4;    ///< Beats per bar (required).
  int denominator = 4;  ///< Beat value (required).
};

// ---------------------------------------------------------------------------------------------
// Timelines

/// Concrete type of a Timeline.
enum class TimelineKind { Lanes, Notes, Clips, ClipSlot, Markers, Warps, Audio, Video, Points };

/// Abstract base of every timeline (content of arrangements, clips, notes and scenes).
class DAWPROJECT_EXPORT Timeline : public Referenceable {
 public:
  virtual ~Timeline() = default;

  /// The concrete type of this timeline.
  [[nodiscard]] virtual TimelineKind kind() const noexcept = 0;
  /// Deep copy of this timeline, keeping its concrete type.
  [[nodiscard]] virtual std::unique_ptr<Timeline> clone() const = 0;

  std::optional<TimeUnit> timeUnit;  ///< Time unit of the events; inherited when absent.
  std::optional<std::string> track;  ///< IDREF of the Track this timeline belongs to.

 protected:
  Timeline() = default;
  Timeline(const Timeline&) = default;
  Timeline(Timeline&&) = default;
  Timeline& operator=(const Timeline&) = default;
  Timeline& operator=(Timeline&&) = default;
  /// Copies the fields of this base class into `target`.
  void copyTimelineFieldsTo(Timeline& target) const { target = *this; }
};

/// Dynamic cast of a timeline to a concrete type; nullptr when it is another type.
template <class T>
[[nodiscard]] T* as(Timeline* timeline) noexcept {
  static_assert(std::is_base_of_v<Timeline, T>, "T must derive from Timeline");
  return dynamic_cast<T*>(timeline);
}

/// Const overload of as().
template <class T>
[[nodiscard]] const T* as(const Timeline* timeline) noexcept {
  static_assert(std::is_base_of_v<Timeline, T>, "T must derive from Timeline");
  return dynamic_cast<const T*>(timeline);
}

/// A container of timelines played in parallel (`<Lanes>`).
class DAWPROJECT_EXPORT Lanes final : public Timeline {
 public:
  Lanes() = default;
  Lanes(Lanes&&) = default;
  Lanes& operator=(Lanes&&) = default;
  Lanes(const Lanes&) = delete;
  Lanes& operator=(const Lanes&) = delete;

  [[nodiscard]] TimelineKind kind() const noexcept override { return TimelineKind::Lanes; }
  [[nodiscard]] std::unique_ptr<Timeline> clone() const override;
  /// Deep copy with the concrete type.
  [[nodiscard]] Lanes cloneLanes() const;

  std::vector<std::unique_ptr<Timeline>> lanes;  ///< Child timelines.
};

/// A single note (`<Note>`).
struct Note {
  Note() = default;
  Note(Note&&) = default;
  Note& operator=(Note&&) = default;
  Note(const Note&) = delete;
  Note& operator=(const Note&) = delete;

  /// Deep copy, including the expression content.
  [[nodiscard]] DAWPROJECT_EXPORT Note clone() const;

  double time = 0.0;          ///< Start time (required).
  double duration = 0.0;      ///< Length (required).
  int channel = 0;            ///< MIDI channel, 0 to 15 (required).
  int key = 60;               ///< MIDI key, 0 to 127 (required).
  std::optional<double> vel;  ///< Note-on velocity, 0 to 1.
  std::optional<double> rel;  ///< Note-off velocity, 0 to 1.
  /// Per-note expressions, usually Lanes of Points targeting an ExpressionType.
  std::unique_ptr<Timeline> content;
};

/// A timeline of notes (`<Notes>`).
class DAWPROJECT_EXPORT Notes final : public Timeline {
 public:
  Notes() = default;
  Notes(Notes&&) = default;
  Notes& operator=(Notes&&) = default;
  Notes(const Notes&) = delete;
  Notes& operator=(const Notes&) = delete;

  [[nodiscard]] TimelineKind kind() const noexcept override { return TimelineKind::Notes; }
  [[nodiscard]] std::unique_ptr<Timeline> clone() const override;

  std::vector<Note> notes;  ///< The notes, in any order.
};

/// A clip placed on a timeline (`<Clip>`).
struct Clip : Nameable {
  Clip() = default;
  Clip(Clip&&) = default;
  Clip& operator=(Clip&&) = default;
  Clip(const Clip&) = delete;
  Clip& operator=(const Clip&) = delete;

  /// Deep copy, including the content.
  [[nodiscard]] DAWPROJECT_EXPORT Clip clone() const;

  double time = 0.0;                        ///< Start on the parent timeline (required).
  std::optional<double> duration;           ///< Length on the parent timeline.
  std::optional<TimeUnit> contentTimeUnit;  ///< Time unit of the content.
  std::optional<double> playStart;          ///< Content time at which playback starts.
  std::optional<double> playStop;           ///< Content time at which playback stops.
  std::optional<double> loopStart;          ///< Content time of the loop start.
  std::optional<double> loopEnd;            ///< Content time of the loop end.
  std::optional<TimeUnit> fadeTimeUnit;     ///< Time unit of the fades.
  std::optional<double> fadeInTime;         ///< Fade-in length.
  std::optional<double> fadeOutTime;        ///< Fade-out length.
  std::optional<bool> enable;               ///< False when the clip is muted.
  std::optional<std::string> reference;     ///< IDREF of a timeline this clip aliases.
  std::unique_ptr<Timeline> content;        ///< The clip content (absent for an alias clip).
};

/// A timeline of clips (`<Clips>`).
class DAWPROJECT_EXPORT Clips final : public Timeline {
 public:
  Clips() = default;
  Clips(Clips&&) = default;
  Clips& operator=(Clips&&) = default;
  Clips(const Clips&) = delete;
  Clips& operator=(const Clips&) = delete;

  [[nodiscard]] TimelineKind kind() const noexcept override { return TimelineKind::Clips; }
  [[nodiscard]] std::unique_ptr<Timeline> clone() const override;

  std::vector<Clip> clips;  ///< The clips.
};

/// A clip launcher slot holding at most one clip (`<ClipSlot>`).
class DAWPROJECT_EXPORT ClipSlot final : public Timeline {
 public:
  ClipSlot() = default;
  ClipSlot(ClipSlot&&) = default;
  ClipSlot& operator=(ClipSlot&&) = default;
  ClipSlot(const ClipSlot&) = delete;
  ClipSlot& operator=(const ClipSlot&) = delete;

  [[nodiscard]] TimelineKind kind() const noexcept override { return TimelineKind::ClipSlot; }
  [[nodiscard]] std::unique_ptr<Timeline> clone() const override;

  std::optional<Clip> clip;     ///< The clip in the slot.
  std::optional<bool> hasStop;  ///< True when the slot has a stop button.
};

/// A cue marker (`<Marker>`).
struct Marker : Nameable {
  double time = 0.0;  ///< Position (required).
};

/// A timeline of cue markers (`<Markers>` in an arrangement, `<markers>` elsewhere).
class DAWPROJECT_EXPORT Markers final : public Timeline {
 public:
  [[nodiscard]] TimelineKind kind() const noexcept override { return TimelineKind::Markers; }
  [[nodiscard]] std::unique_ptr<Timeline> clone() const override;

  std::vector<Marker> markers;  ///< The markers; the schema requires at least one.
};

/// One warp point mapping a time to a content time (`<Warp>`).
struct Warp {
  double time = 0.0;         ///< Time on the warped timeline (required).
  double contentTime = 0.0;  ///< Matching time in the content (required).
};

/// A timeline warping its content in time (`<Warps>`).
class DAWPROJECT_EXPORT Warps final : public Timeline {
 public:
  Warps() = default;
  Warps(Warps&&) = default;
  Warps& operator=(Warps&&) = default;
  Warps(const Warps&) = delete;
  Warps& operator=(const Warps&) = delete;

  [[nodiscard]] TimelineKind kind() const noexcept override { return TimelineKind::Warps; }
  [[nodiscard]] std::unique_ptr<Timeline> clone() const override;

  std::unique_ptr<Timeline> content;             ///< The warped content (required).
  std::vector<Warp> warps;                       ///< Warp points; the schema requires one.
  TimeUnit contentTimeUnit = TimeUnit::Seconds;  ///< Time unit of the content (required).
};

/// Fields shared by Audio and Video.
class DAWPROJECT_EXPORT MediaFile : public Timeline {
 public:
  FileReference file;                    ///< The media file (required).
  double duration = 0.0;                 ///< Length of the media in seconds (required).
  int channels = 0;                      ///< Number of audio channels (required).
  int sampleRate = 0;                    ///< Audio sample rate in Hz (required).
  std::optional<std::string> algorithm;  ///< Time stretching algorithm name.

 protected:
  MediaFile() = default;
};

/// An audio file timeline (`<Audio>`).
class DAWPROJECT_EXPORT Audio final : public MediaFile {
 public:
  [[nodiscard]] TimelineKind kind() const noexcept override { return TimelineKind::Audio; }
  [[nodiscard]] std::unique_ptr<Timeline> clone() const override;
};

/// A video file timeline (`<Video>`).
class DAWPROJECT_EXPORT Video final : public MediaFile {
 public:
  [[nodiscard]] TimelineKind kind() const noexcept override { return TimelineKind::Video; }
  [[nodiscard]] std::unique_ptr<Timeline> clone() const override;
};

/// An automation point with a real value (`<RealPoint>`).
struct RealPoint {
  double time = 0.0;                           ///< Position (required).
  double value = 0.0;                          ///< Value (required).
  std::optional<Interpolation> interpolation;  ///< Interpolation to the next point.
};

/// An automation point selecting an enumeration index (`<EnumPoint>`).
struct EnumPoint {
  double time = 0.0;  ///< Position (required).
  int value = 0;      ///< Value (required).
};

/// An automation point with a boolean value (`<BoolPoint>`).
struct BoolPoint {
  double time = 0.0;   ///< Position (required).
  bool value = false;  ///< Value (required).
};

/// An automation point with an integer value (`<IntegerPoint>`).
struct IntegerPoint {
  double time = 0.0;  ///< Position (required).
  int value = 0;      ///< Value (required).
};

/// An automation point with a time signature (`<TimeSignaturePoint>`).
struct TimeSignaturePoint {
  double time = 0.0;    ///< Position (required).
  int numerator = 4;    ///< Beats per bar (required).
  int denominator = 4;  ///< Beat value (required).
};

/// Any automation point.
using Point = std::variant<RealPoint, EnumPoint, BoolPoint, IntegerPoint, TimeSignaturePoint>;

/// What an automation lane controls: a parameter or an expression (`<Target>`).
struct AutomationTarget {
  std::optional<std::string> parameter;      ///< IDREF of the automated Parameter.
  std::optional<ExpressionType> expression;  ///< Automated expression.
  std::optional<int> channel;                ///< MIDI channel of the expression.
  std::optional<int> key;                    ///< MIDI key of a per-note expression.
  std::optional<int> controller;             ///< MIDI controller number.
};

/// An automation lane (`<Points>`).
class DAWPROJECT_EXPORT Points final : public Timeline {
 public:
  [[nodiscard]] TimelineKind kind() const noexcept override { return TimelineKind::Points; }
  [[nodiscard]] std::unique_ptr<Timeline> clone() const override;

  AutomationTarget target;    ///< What is automated (required).
  std::vector<Point> points;  ///< The points, in time order.
  std::optional<Unit> unit;   ///< Unit of the values.
};

// ---------------------------------------------------------------------------------------------
// Devices

/// Concrete type of a Device.
enum class DeviceKind {
  Device,
  Vst2Plugin,
  Vst3Plugin,
  ClapPlugin,
  AuPlugin,
  BuiltinDevice,
  Equalizer,
  Compressor,
  NoiseGate,
  Limiter,
};

/// Abstract base of every device in a channel's device chain.
class DAWPROJECT_EXPORT Device : public Referenceable {
 public:
  virtual ~Device() = default;
  Device(const Device&) = delete;
  Device& operator=(const Device&) = delete;

  /// The concrete type of this device.
  [[nodiscard]] virtual DeviceKind kind() const noexcept = 0;
  /// Deep copy of this device, keeping its concrete type.
  [[nodiscard]] virtual std::unique_ptr<Device> clone() const = 0;

  std::string deviceName;                       ///< Display name of the device (required).
  DeviceRole deviceRole = DeviceRole::AudioFX;  ///< Role in the chain (required).
  std::optional<std::string> deviceID;      ///< Vendor specific identifier, such as a plugin id.
  std::optional<std::string> deviceVendor;  ///< Vendor name.
  std::optional<bool> loaded;               ///< False when the device could not be loaded.
  std::vector<std::unique_ptr<Parameter>> parameters;  ///< Exposed parameters.
  std::optional<BoolParameter> enabled;                ///< Bypass state.
  std::optional<FileReference> state;  ///< Saved device state, such as a .vstpreset.

 protected:
  Device() = default;
  // Moves are for the concrete types: moving through a Device& would slice.
  Device(Device&&) = default;
  Device& operator=(Device&&) = default;
  /// Copies the fields of this base class into `target`, deep copying the parameters.
  void copyDeviceFieldsTo(Device& target) const;
};

/// A device without a more specific type (`<Device>`).
class DAWPROJECT_EXPORT GenericDevice final : public Device {
 public:
  [[nodiscard]] DeviceKind kind() const noexcept override { return DeviceKind::Device; }
  [[nodiscard]] std::unique_ptr<Device> clone() const override;
};

/// Abstract base of plugin devices.
class DAWPROJECT_EXPORT Plugin : public Device {
 public:
  std::optional<std::string> pluginVersion;  ///< Version of the plugin.

 protected:
  Plugin() = default;
  /// Copies the fields of this class and of Device into `target`.
  void copyPluginFieldsTo(Plugin& target) const;
};

/// A VST2 plugin (`<Vst2Plugin>`).
class DAWPROJECT_EXPORT Vst2Plugin final : public Plugin {
 public:
  [[nodiscard]] DeviceKind kind() const noexcept override { return DeviceKind::Vst2Plugin; }
  [[nodiscard]] std::unique_ptr<Device> clone() const override;
};

/// A VST3 plugin (`<Vst3Plugin>`).
class DAWPROJECT_EXPORT Vst3Plugin final : public Plugin {
 public:
  [[nodiscard]] DeviceKind kind() const noexcept override { return DeviceKind::Vst3Plugin; }
  [[nodiscard]] std::unique_ptr<Device> clone() const override;
};

/// A CLAP plugin (`<ClapPlugin>`).
class DAWPROJECT_EXPORT ClapPlugin final : public Plugin {
 public:
  [[nodiscard]] DeviceKind kind() const noexcept override { return DeviceKind::ClapPlugin; }
  [[nodiscard]] std::unique_ptr<Device> clone() const override;
};

/// An Audio Unit plugin (`<AuPlugin>`).
class DAWPROJECT_EXPORT AuPlugin final : public Plugin {
 public:
  [[nodiscard]] DeviceKind kind() const noexcept override { return DeviceKind::AuPlugin; }
  [[nodiscard]] std::unique_ptr<Device> clone() const override;
};

/// A device built into the host (`<BuiltinDevice>`); base of the standard devices below.
class DAWPROJECT_EXPORT BuiltinDevice : public Device {
 public:
  BuiltinDevice() = default;
  [[nodiscard]] DeviceKind kind() const noexcept override { return DeviceKind::BuiltinDevice; }
  [[nodiscard]] std::unique_ptr<Device> clone() const override;
};

/// One band of an Equalizer (`<Band>`).
struct EqBand {
  EqBandType type = EqBandType::Bell;    ///< Filter shape (required).
  std::optional<int> order;              ///< Filter order.
  RealParameter freq;                    ///< Frequency (required).
  std::optional<RealParameter> gain;     ///< Gain.
  std::optional<RealParameter> q;        ///< Quality factor.
  std::optional<BoolParameter> enabled;  ///< Whether the band is active.
};

/// A standard equalizer (`<Equalizer>`).
class DAWPROJECT_EXPORT Equalizer final : public BuiltinDevice {
 public:
  [[nodiscard]] DeviceKind kind() const noexcept override { return DeviceKind::Equalizer; }
  [[nodiscard]] std::unique_ptr<Device> clone() const override;

  std::vector<EqBand> bands;                ///< The bands.
  std::optional<RealParameter> inputGain;   ///< Input gain.
  std::optional<RealParameter> outputGain;  ///< Output gain.
};

/// A standard compressor (`<Compressor>`).
class DAWPROJECT_EXPORT Compressor final : public BuiltinDevice {
 public:
  [[nodiscard]] DeviceKind kind() const noexcept override { return DeviceKind::Compressor; }
  [[nodiscard]] std::unique_ptr<Device> clone() const override;

  std::optional<RealParameter> attack;      ///< Attack time.
  std::optional<BoolParameter> autoMakeup;  ///< Automatic makeup gain.
  std::optional<RealParameter> inputGain;   ///< Input gain.
  std::optional<RealParameter> outputGain;  ///< Output gain.
  std::optional<RealParameter> ratio;       ///< Compression ratio.
  std::optional<RealParameter> release;     ///< Release time.
  std::optional<RealParameter> threshold;   ///< Threshold.
};

/// A standard noise gate (`<NoiseGate>`).
class DAWPROJECT_EXPORT NoiseGate final : public BuiltinDevice {
 public:
  [[nodiscard]] DeviceKind kind() const noexcept override { return DeviceKind::NoiseGate; }
  [[nodiscard]] std::unique_ptr<Device> clone() const override;

  std::optional<RealParameter> attack;     ///< Attack time.
  std::optional<RealParameter> range;      ///< Attenuation range.
  std::optional<RealParameter> ratio;      ///< Expansion ratio.
  std::optional<RealParameter> release;    ///< Release time.
  std::optional<RealParameter> threshold;  ///< Threshold.
};

/// A standard limiter (`<Limiter>`).
class DAWPROJECT_EXPORT Limiter final : public BuiltinDevice {
 public:
  [[nodiscard]] DeviceKind kind() const noexcept override { return DeviceKind::Limiter; }
  [[nodiscard]] std::unique_ptr<Device> clone() const override;

  std::optional<RealParameter> attack;      ///< Attack time.
  std::optional<RealParameter> inputGain;   ///< Input gain.
  std::optional<RealParameter> outputGain;  ///< Output gain.
  std::optional<RealParameter> release;     ///< Release time.
  std::optional<RealParameter> threshold;   ///< Threshold.
};

// ---------------------------------------------------------------------------------------------
// Structure

/// A send from a channel to another channel (`<Send>`).
struct Send : Referenceable {
  std::optional<BoolParameter> enable;     ///< Whether the send is active.
  std::optional<RealParameter> pan;        ///< Send pan.
  RealParameter volume;                    ///< Send level (required).
  std::optional<std::string> destination;  ///< IDREF of the destination Channel.
  std::optional<SendType> type;            ///< Pre or post fader.
};

/// A mixer channel (`<Channel>`).
struct Channel : Referenceable {
  Channel() = default;
  Channel(Channel&&) = default;
  Channel& operator=(Channel&&) = default;
  Channel(const Channel&) = delete;
  Channel& operator=(const Channel&) = delete;

  /// Deep copy, including the devices.
  [[nodiscard]] DAWPROJECT_EXPORT Channel clone() const;

  std::optional<MixerRole> role;                 ///< Mixer role.
  std::optional<int> audioChannels;              ///< Number of audio channels (2 for stereo).
  std::optional<std::string> destination;        ///< IDREF of the Channel this one feeds.
  std::optional<bool> solo;                      ///< Solo state.
  std::vector<std::unique_ptr<Device>> devices;  ///< Device chain, in signal order.
  std::optional<BoolParameter> mute;             ///< Mute state.
  std::optional<RealParameter> pan;              ///< Pan.
  std::vector<Send> sends;                       ///< Sends.
  std::optional<RealParameter> volume;           ///< Fader level.
};

/// A track, possibly a folder of nested tracks (`<Track>`).
struct Track : Referenceable {
  Track() = default;
  Track(Track&&) = default;
  Track& operator=(Track&&) = default;
  Track(const Track&) = delete;
  Track& operator=(const Track&) = delete;

  /// Deep copy, including the channel and nested tracks.
  [[nodiscard]] DAWPROJECT_EXPORT Track clone() const;

  std::vector<ContentType> contentType;  ///< Kinds of content the track holds.
  std::optional<bool> loaded;            ///< False when the track is disabled.
  std::optional<Channel> channel;        ///< The track's mixer channel.
  std::vector<Track> tracks;             ///< Child tracks of a folder track.
};

/// One top level entry of `<Structure>`: a Track or a lone Channel.
using StructureItem = std::variant<Track, Channel>;

// ---------------------------------------------------------------------------------------------
// Project

/// The application that wrote the file (`<Application>`).
struct Application {
  std::string name;     ///< Application name (required).
  std::string version;  ///< Application version (required).
};

/// Global transport settings (`<Transport>`).
struct Transport {
  std::optional<RealParameter> tempo;                   ///< Tempo, usually in Unit::Bpm.
  std::optional<TimeSignatureParameter> timeSignature;  ///< Time signature.
};

/// The arrangement timeline (`<Arrangement>`).
struct Arrangement : Referenceable {
  Arrangement() = default;
  Arrangement(Arrangement&&) = default;
  Arrangement& operator=(Arrangement&&) = default;
  Arrangement(const Arrangement&) = delete;
  Arrangement& operator=(const Arrangement&) = delete;

  /// Deep copy.
  [[nodiscard]] DAWPROJECT_EXPORT Arrangement clone() const;

  std::unique_ptr<Lanes> lanes;                   ///< The track lanes.
  std::optional<Markers> markers;                 ///< Cue markers.
  std::optional<Points> tempoAutomation;          ///< Tempo automation.
  std::optional<Points> timeSignatureAutomation;  ///< Time signature automation.
};

/// A clip launcher scene (`<Scene>`).
struct Scene : Referenceable {
  Scene() = default;
  Scene(Scene&&) = default;
  Scene& operator=(Scene&&) = default;
  Scene(const Scene&) = delete;
  Scene& operator=(const Scene&) = delete;

  /// Deep copy.
  [[nodiscard]] DAWPROJECT_EXPORT Scene clone() const;

  /// The scene content, usually Lanes of ClipSlot. The schema requires one.
  std::unique_ptr<Timeline> content;
};

/// A whole DAWproject document (`project.xml`).
struct Project {
  Project() = default;
  Project(Project&&) = default;
  Project& operator=(Project&&) = default;
  Project(const Project&) = delete;
  Project& operator=(const Project&) = delete;

  /// Deep copy of the whole project.
  [[nodiscard]] DAWPROJECT_EXPORT Project clone() const;

  std::string version = "1.0";             ///< Format version (required).
  Application application;                 ///< Writing application (required).
  std::optional<Transport> transport;      ///< Transport.
  std::vector<StructureItem> structure;    ///< Tracks and channels.
  std::optional<Arrangement> arrangement;  ///< Arrangement.
  std::vector<Scene> scenes;               ///< Clip launcher scenes.
};

/// Song metadata (`metadata.xml`); every field is optional.
struct MetaData {
  std::optional<std::string> title;           ///< `<Title>`.
  std::optional<std::string> artist;          ///< `<Artist>`.
  std::optional<std::string> album;           ///< `<Album>`.
  std::optional<std::string> originalArtist;  ///< `<OriginalArtist>`.
  std::optional<std::string> composer;        ///< `<Composer>`.
  std::optional<std::string> songwriter;      ///< `<Songwriter>`.
  std::optional<std::string> producer;        ///< `<Producer>`.
  std::optional<std::string> arranger;        ///< `<Arranger>`.
  std::optional<std::string> year;            ///< `<Year>`.
  std::optional<std::string> genre;           ///< `<Genre>`.
  std::optional<std::string> copyright;       ///< `<Copyright>`.
  std::optional<std::string> website;         ///< `<Website>`.
  std::optional<std::string> comment;         ///< `<Comment>`.
};

}  // namespace dawproject
