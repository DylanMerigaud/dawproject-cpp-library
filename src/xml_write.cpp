// SPDX-License-Identifier: MIT
// Serialization of the model to project.xml and metadata.xml. Element order follows the sequences
// of spec/Project.xsd; inherited attributes come first, then the type's own attributes.
#include <cmath>
#include <sstream>
#include <string>

#include "dawproject/error.hpp"
#include "dawproject/xml.hpp"
#include "pugixml.hpp"

namespace dawproject {
namespace {

using pugi::xml_node;

// Attribute helpers ---------------------------------------------------------------------------

// XML 1.0 cannot carry control characters other than tab, line feed and carriage return, nor
// the noncharacters U+FFFE and U+FFFF, not even escaped: such a string is refused.
void checkText(const std::string& text, const char* element, const char* field) {
  static const char* const kHex = "0123456789ABCDEF";
  for (std::size_t i = 0; i < text.size(); ++i) {
    const auto c = static_cast<unsigned char>(text[i]);
    std::string code;
    if (c < 0x20 && c != '\t' && c != '\n' && c != '\r') {
      code = std::string("U+00") + kHex[c >> 4] + kHex[c & 0xF];
    } else if (c == 0xEF && i + 2 < text.size() &&
               static_cast<unsigned char>(text[i + 1]) == 0xBF &&
               (static_cast<unsigned char>(text[i + 2]) & 0xFE) == 0xBE) {
      code = static_cast<unsigned char>(text[i + 2]) == 0xBE ? "U+FFFE" : "U+FFFF";
    } else {
      continue;
    }
    throw Error(ErrorKind::Schema, std::string("<") + element + "> '" + field +
                                       "' contains the character " + code +
                                       ", which XML cannot represent");
  }
}

void attr(xml_node node, const char* name, const std::string& value) {
  checkText(value, node.name(), name);
  node.append_attribute(name).set_value(value.c_str());
}

void attr(xml_node node, const char* name, const std::optional<std::string>& value) {
  if (value) {
    attr(node, name, *value);
  }
}

void attrDouble(xml_node node, const char* name, double value) {
  attr(node, name, formatDouble(value));
}

void attrDouble(xml_node node, const char* name, const std::optional<double>& value) {
  if (value) {
    attrDouble(node, name, *value);
  }
}

// An xs:double attribute: the XSD spells infinities "INF" and "-INF". (xs:string attributes
// keep "inf" and "-inf", the spelling of the reference implementation.)
void attrXsDouble(xml_node node, const char* name, double value) {
  if (std::isinf(value)) {
    attr(node, name, std::string(value > 0 ? "INF" : "-INF"));
  } else {
    attrDouble(node, name, value);
  }
}

void attrXsDouble(xml_node node, const char* name, const std::optional<double>& value) {
  if (value) {
    attrXsDouble(node, name, *value);
  }
}

void attrInt(xml_node node, const char* name, int value) {
  attr(node, name, std::to_string(value));
}

void attrInt(xml_node node, const char* name, const std::optional<int>& value) {
  if (value) {
    attrInt(node, name, *value);
  }
}

void attrBool(xml_node node, const char* name, bool value) {
  attr(node, name, std::string(value ? "true" : "false"));
}

void attrBool(xml_node node, const char* name, const std::optional<bool>& value) {
  if (value) {
    attrBool(node, name, *value);
  }
}

template <class E>
void attrEnum(xml_node node, const char* name, E value) {
  attr(node, name, std::string(to_string(value)));
}

template <class E>
void attrEnum(xml_node node, const char* name, const std::optional<E>& value) {
  if (value) {
    attrEnum(node, name, *value);
  }
}

void writeNameable(xml_node node, const Nameable& value) {
  attr(node, "name", value.name);
  attr(node, "color", value.color);
  attr(node, "comment", value.comment);
}

void writeReferenceable(xml_node node, const Referenceable& value) {
  attr(node, "id", value.id);
  writeNameable(node, value);
}

// Parameters ----------------------------------------------------------------------------------

void writeParameterBase(xml_node node, const Parameter& value) {
  writeReferenceable(node, value);
  attrInt(node, "parameterID", value.parameterID);
}

void writeRealParameter(xml_node parent, const char* element, const RealParameter& value) {
  xml_node node = parent.append_child(element);
  writeParameterBase(node, value);
  attrDouble(node, "value", value.value);
  attrEnum(node, "unit", value.unit);
  attrDouble(node, "min", value.min);
  attrDouble(node, "max", value.max);
}

void writeRealParameter(xml_node parent, const char* element,
                        const std::optional<RealParameter>& value) {
  if (value) {
    writeRealParameter(parent, element, *value);
  }
}

void writeBoolParameter(xml_node parent, const char* element, const BoolParameter& value) {
  xml_node node = parent.append_child(element);
  writeParameterBase(node, value);
  attrBool(node, "value", value.value);
}

void writeBoolParameter(xml_node parent, const char* element,
                        const std::optional<BoolParameter>& value) {
  if (value) {
    writeBoolParameter(parent, element, *value);
  }
}

void writeIntegerParameter(xml_node parent, const char* element, const IntegerParameter& value) {
  xml_node node = parent.append_child(element);
  writeParameterBase(node, value);
  attrInt(node, "value", value.value);
  attrInt(node, "min", value.min);
  attrInt(node, "max", value.max);
}

void writeEnumParameter(xml_node parent, const char* element, const EnumParameter& value) {
  xml_node node = parent.append_child(element);
  writeParameterBase(node, value);
  attrInt(node, "value", value.value);
  attrInt(node, "count", value.count);
  if (!value.labels.empty()) {
    std::string joined;
    for (const std::string& label : value.labels) {
      if (label.empty() || label.find_first_of(" \t\n\r") != std::string::npos) {
        throw Error(ErrorKind::Schema, "<" + std::string(element) + "> label '" + label +
                                           "' is empty or contains whitespace, which the "
                                           "space separated 'labels' list cannot hold");
      }
      if (!joined.empty()) {
        joined += ' ';
      }
      joined += label;
    }
    attr(node, "labels", joined);
  }
}

void writeTimeSignatureParameter(xml_node parent, const char* element,
                                 const TimeSignatureParameter& value) {
  xml_node node = parent.append_child(element);
  writeParameterBase(node, value);
  attrInt(node, "numerator", value.numerator);
  attrInt(node, "denominator", value.denominator);
}

void writeParameter(xml_node parent, const Parameter& value) {
  switch (value.kind()) {
    case ParameterKind::Real:
      writeRealParameter(parent, "RealParameter", static_cast<const RealParameter&>(value));
      break;
    case ParameterKind::Bool:
      writeBoolParameter(parent, "BoolParameter", static_cast<const BoolParameter&>(value));
      break;
    case ParameterKind::Integer:
      writeIntegerParameter(parent, "IntegerParameter",
                            static_cast<const IntegerParameter&>(value));
      break;
    case ParameterKind::Enum:
      writeEnumParameter(parent, "EnumParameter", static_cast<const EnumParameter&>(value));
      break;
    case ParameterKind::TimeSignature:
      writeTimeSignatureParameter(parent, "TimeSignatureParameter",
                                  static_cast<const TimeSignatureParameter&>(value));
      break;
  }
}

void writeFileReference(xml_node parent, const char* element, const FileReference& value) {
  xml_node node = parent.append_child(element);
  attr(node, "path", value.path);
  attrBool(node, "external", value.external);
}

// Timelines -----------------------------------------------------------------------------------

void writeTimeline(xml_node parent, const Timeline& value, const char* elementOverride = nullptr);

void writeContent(xml_node parent, const std::unique_ptr<Timeline>& content) {
  if (content) {
    writeTimeline(parent, *content);
  }
}

void writeTimelineBase(xml_node node, const Timeline& value) {
  writeReferenceable(node, value);
  attrEnum(node, "timeUnit", value.timeUnit);
  attr(node, "track", value.track);
}

void writeClip(xml_node parent, const Clip& clip) {
  xml_node node = parent.append_child("Clip");
  writeNameable(node, clip);
  attrXsDouble(node, "time", clip.time);
  attrXsDouble(node, "duration", clip.duration);
  attrEnum(node, "contentTimeUnit", clip.contentTimeUnit);
  attrXsDouble(node, "playStart", clip.playStart);
  attrXsDouble(node, "playStop", clip.playStop);
  attrXsDouble(node, "loopStart", clip.loopStart);
  attrXsDouble(node, "loopEnd", clip.loopEnd);
  attrEnum(node, "fadeTimeUnit", clip.fadeTimeUnit);
  attrXsDouble(node, "fadeInTime", clip.fadeInTime);
  attrXsDouble(node, "fadeOutTime", clip.fadeOutTime);
  attrBool(node, "enable", clip.enable);
  attr(node, "reference", clip.reference);
  writeContent(node, clip.content);
}

void writeNote(xml_node parent, const Note& note) {
  xml_node node = parent.append_child("Note");
  attrDouble(node, "time", note.time);
  attrDouble(node, "duration", note.duration);
  attrInt(node, "channel", note.channel);
  attrInt(node, "key", note.key);
  attrDouble(node, "vel", note.vel);
  attrDouble(node, "rel", note.rel);
  writeContent(node, note.content);
}

void writePoint(xml_node parent, const Point& point) {
  std::visit(
      [&parent](const auto& p) {
        using T = std::decay_t<decltype(p)>;
        if constexpr (std::is_same_v<T, RealPoint>) {
          xml_node node = parent.append_child("RealPoint");
          attrDouble(node, "time", p.time);
          attrDouble(node, "value", p.value);
          attrEnum(node, "interpolation", p.interpolation);
        } else if constexpr (std::is_same_v<T, EnumPoint>) {
          xml_node node = parent.append_child("EnumPoint");
          attrDouble(node, "time", p.time);
          attrInt(node, "value", p.value);
        } else if constexpr (std::is_same_v<T, BoolPoint>) {
          xml_node node = parent.append_child("BoolPoint");
          attrDouble(node, "time", p.time);
          attrBool(node, "value", p.value);
        } else if constexpr (std::is_same_v<T, IntegerPoint>) {
          xml_node node = parent.append_child("IntegerPoint");
          attrDouble(node, "time", p.time);
          attrInt(node, "value", p.value);
        } else {
          static_assert(std::is_same_v<T, TimeSignaturePoint>);
          xml_node node = parent.append_child("TimeSignaturePoint");
          attrDouble(node, "time", p.time);
          attrInt(node, "numerator", p.numerator);
          attrInt(node, "denominator", p.denominator);
        }
      },
      point);
}

void writePointsBody(xml_node node, const Points& points) {
  writeTimelineBase(node, points);
  attrEnum(node, "unit", points.unit);
  xml_node target = node.append_child("Target");
  attr(target, "parameter", points.target.parameter);
  attrEnum(target, "expression", points.target.expression);
  attrInt(target, "channel", points.target.channel);
  attrInt(target, "key", points.target.key);
  attrInt(target, "controller", points.target.controller);
  for (const Point& point : points.points) {
    writePoint(node, point);
  }
}

void writeMarkersBody(xml_node node, const Markers& markers) {
  writeTimelineBase(node, markers);
  for (const Marker& marker : markers.markers) {
    xml_node child = node.append_child("Marker");
    writeNameable(child, marker);
    attrXsDouble(child, "time", marker.time);
  }
}

void writeLanesBody(xml_node node, const Lanes& lanes) {
  writeTimelineBase(node, lanes);
  for (const auto& lane : lanes.lanes) {
    writeContent(node, lane);
  }
}

void writeMediaFile(xml_node node, const MediaFile& media) {
  writeTimelineBase(node, media);
  attrXsDouble(node, "duration", media.duration);
  attrInt(node, "channels", media.channels);
  attrInt(node, "sampleRate", media.sampleRate);
  attr(node, "algorithm", media.algorithm);
  writeFileReference(node, "File", media.file);
}

void writeTimeline(xml_node parent, const Timeline& value, const char* elementOverride) {
  switch (value.kind()) {
    case TimelineKind::Lanes:
      writeLanesBody(parent.append_child(elementOverride ? elementOverride : "Lanes"),
                     static_cast<const Lanes&>(value));
      break;
    case TimelineKind::Notes: {
      const auto& notes = static_cast<const Notes&>(value);
      xml_node node = parent.append_child("Notes");
      writeTimelineBase(node, notes);
      for (const Note& note : notes.notes) {
        writeNote(node, note);
      }
      break;
    }
    case TimelineKind::Clips: {
      const auto& clips = static_cast<const Clips&>(value);
      xml_node node = parent.append_child("Clips");
      writeTimelineBase(node, clips);
      for (const Clip& clip : clips.clips) {
        writeClip(node, clip);
      }
      break;
    }
    case TimelineKind::ClipSlot: {
      const auto& slot = static_cast<const ClipSlot&>(value);
      xml_node node = parent.append_child("ClipSlot");
      writeTimelineBase(node, slot);
      attrBool(node, "hasStop", slot.hasStop);
      if (slot.clip) {
        writeClip(node, *slot.clip);
      }
      break;
    }
    case TimelineKind::Markers:
      // The schema names the global element "markers"; the Arrangement child is "Markers".
      writeMarkersBody(parent.append_child(elementOverride ? elementOverride : "markers"),
                       static_cast<const Markers&>(value));
      break;
    case TimelineKind::Warps: {
      const auto& warps = static_cast<const Warps&>(value);
      xml_node node = parent.append_child("Warps");
      writeTimelineBase(node, warps);
      attrEnum(node, "contentTimeUnit", warps.contentTimeUnit);
      writeContent(node, warps.content);
      for (const Warp& warp : warps.warps) {
        xml_node child = node.append_child("Warp");
        attrXsDouble(child, "time", warp.time);
        attrXsDouble(child, "contentTime", warp.contentTime);
      }
      break;
    }
    case TimelineKind::Audio:
      writeMediaFile(parent.append_child("Audio"), static_cast<const MediaFile&>(value));
      break;
    case TimelineKind::Video:
      writeMediaFile(parent.append_child("Video"), static_cast<const MediaFile&>(value));
      break;
    case TimelineKind::Points:
      writePointsBody(parent.append_child(elementOverride ? elementOverride : "Points"),
                      static_cast<const Points&>(value));
      break;
  }
}

// Devices -------------------------------------------------------------------------------------

const char* deviceElement(DeviceKind kind) {
  switch (kind) {
    case DeviceKind::Device:
      return "Device";
    case DeviceKind::Vst2Plugin:
      return "Vst2Plugin";
    case DeviceKind::Vst3Plugin:
      return "Vst3Plugin";
    case DeviceKind::ClapPlugin:
      return "ClapPlugin";
    case DeviceKind::AuPlugin:
      return "AuPlugin";
    case DeviceKind::BuiltinDevice:
      return "BuiltinDevice";
    case DeviceKind::Equalizer:
      return "Equalizer";
    case DeviceKind::Compressor:
      return "Compressor";
    case DeviceKind::NoiseGate:
      return "NoiseGate";
    case DeviceKind::Limiter:
      return "Limiter";
  }
  return "Device";
}

void writeDevice(xml_node parent, const Device& device) {
  xml_node node = parent.append_child(deviceElement(device.kind()));
  writeReferenceable(node, device);
  attr(node, "deviceID", device.deviceID);
  attr(node, "deviceName", device.deviceName);
  attrEnum(node, "deviceRole", device.deviceRole);
  attr(node, "deviceVendor", device.deviceVendor);
  attrBool(node, "loaded", device.loaded);
  if (const auto* plugin = dynamic_cast<const Plugin*>(&device)) {
    attr(node, "pluginVersion", plugin->pluginVersion);
  }

  if (!device.parameters.empty()) {
    xml_node parameters = node.append_child("Parameters");
    for (const auto& parameter : device.parameters) {
      if (parameter) {
        writeParameter(parameters, *parameter);
      }
    }
  }
  writeBoolParameter(node, "Enabled", device.enabled);
  if (device.state) {
    writeFileReference(node, "State", *device.state);
  }

  switch (device.kind()) {
    case DeviceKind::Equalizer: {
      const auto& eq = static_cast<const Equalizer&>(device);
      for (const EqBand& band : eq.bands) {
        xml_node child = node.append_child("Band");
        attrEnum(child, "type", band.type);
        attrInt(child, "order", band.order);
        writeRealParameter(child, "Freq", band.freq);
        writeRealParameter(child, "Gain", band.gain);
        writeRealParameter(child, "Q", band.q);
        writeBoolParameter(child, "Enabled", band.enabled);
      }
      writeRealParameter(node, "InputGain", eq.inputGain);
      writeRealParameter(node, "OutputGain", eq.outputGain);
      break;
    }
    case DeviceKind::Compressor: {
      const auto& c = static_cast<const Compressor&>(device);
      writeRealParameter(node, "Attack", c.attack);
      writeBoolParameter(node, "AutoMakeup", c.autoMakeup);
      writeRealParameter(node, "InputGain", c.inputGain);
      writeRealParameter(node, "OutputGain", c.outputGain);
      writeRealParameter(node, "Ratio", c.ratio);
      writeRealParameter(node, "Release", c.release);
      writeRealParameter(node, "Threshold", c.threshold);
      break;
    }
    case DeviceKind::NoiseGate: {
      const auto& g = static_cast<const NoiseGate&>(device);
      writeRealParameter(node, "Attack", g.attack);
      writeRealParameter(node, "Range", g.range);
      writeRealParameter(node, "Ratio", g.ratio);
      writeRealParameter(node, "Release", g.release);
      writeRealParameter(node, "Threshold", g.threshold);
      break;
    }
    case DeviceKind::Limiter: {
      const auto& l = static_cast<const Limiter&>(device);
      writeRealParameter(node, "Attack", l.attack);
      writeRealParameter(node, "InputGain", l.inputGain);
      writeRealParameter(node, "OutputGain", l.outputGain);
      writeRealParameter(node, "Release", l.release);
      writeRealParameter(node, "Threshold", l.threshold);
      break;
    }
    default:
      break;
  }
}

// Structure -----------------------------------------------------------------------------------

void writeChannel(xml_node parent, const Channel& channel) {
  xml_node node = parent.append_child("Channel");
  writeReferenceable(node, channel);
  attrEnum(node, "role", channel.role);
  attrInt(node, "audioChannels", channel.audioChannels);
  attr(node, "destination", channel.destination);
  attrBool(node, "solo", channel.solo);
  if (!channel.devices.empty()) {
    xml_node devices = node.append_child("Devices");
    for (const auto& device : channel.devices) {
      if (device) {
        writeDevice(devices, *device);
      }
    }
  }
  writeBoolParameter(node, "Mute", channel.mute);
  writeRealParameter(node, "Pan", channel.pan);
  if (!channel.sends.empty()) {
    xml_node sends = node.append_child("Sends");
    for (const Send& send : channel.sends) {
      xml_node child = sends.append_child("Send");
      writeReferenceable(child, send);
      attr(child, "destination", send.destination);
      attrEnum(child, "type", send.type);
      writeBoolParameter(child, "Enable", send.enable);
      writeRealParameter(child, "Pan", send.pan);
      writeRealParameter(child, "Volume", send.volume);
    }
  }
  writeRealParameter(node, "Volume", channel.volume);
}

void writeTrack(xml_node parent, const Track& track) {
  xml_node node = parent.append_child("Track");
  writeReferenceable(node, track);
  if (!track.contentType.empty()) {
    std::string joined;
    for (const ContentType type : track.contentType) {
      if (!joined.empty()) {
        joined += ' ';
      }
      joined += to_string(type);
    }
    attr(node, "contentType", joined);
  }
  attrBool(node, "loaded", track.loaded);
  if (track.channel) {
    writeChannel(node, *track.channel);
  }
  for (const Track& child : track.tracks) {
    writeTrack(node, child);
  }
}

// Documents -----------------------------------------------------------------------------------

std::string toText(const pugi::xml_document& document) {
  std::ostringstream out;
  document.save(out, "  ", pugi::format_indent, pugi::encoding_utf8);
  return out.str();
}

pugi::xml_document newDocument() {
  pugi::xml_document document;
  xml_node declaration = document.append_child(pugi::node_declaration);
  declaration.append_attribute("version").set_value("1.0");
  declaration.append_attribute("encoding").set_value("UTF-8");
  declaration.append_attribute("standalone").set_value("yes");
  return document;
}

}  // namespace

std::string serialize(const Project& project) {
  pugi::xml_document document = newDocument();
  xml_node root = document.append_child("Project");
  attr(root, "version", project.version);

  xml_node application = root.append_child("Application");
  attr(application, "name", project.application.name);
  attr(application, "version", project.application.version);

  if (project.transport) {
    xml_node transport = root.append_child("Transport");
    writeRealParameter(transport, "Tempo", project.transport->tempo);
    if (project.transport->timeSignature) {
      writeTimeSignatureParameter(transport, "TimeSignature", *project.transport->timeSignature);
    }
  }

  if (!project.structure.empty()) {
    xml_node structure = root.append_child("Structure");
    for (const StructureItem& item : project.structure) {
      if (const Track* track = std::get_if<Track>(&item)) {
        writeTrack(structure, *track);
      } else {
        writeChannel(structure, std::get<Channel>(item));
      }
    }
  }

  if (project.arrangement) {
    const Arrangement& arrangement = *project.arrangement;
    xml_node node = root.append_child("Arrangement");
    writeReferenceable(node, arrangement);
    if (arrangement.lanes) {
      writeTimeline(node, *arrangement.lanes);
    }
    if (arrangement.markers) {
      writeTimeline(node, *arrangement.markers, "Markers");
    }
    if (arrangement.tempoAutomation) {
      writeTimeline(node, *arrangement.tempoAutomation, "TempoAutomation");
    }
    if (arrangement.timeSignatureAutomation) {
      writeTimeline(node, *arrangement.timeSignatureAutomation, "TimeSignatureAutomation");
    }
  }

  if (!project.scenes.empty()) {
    xml_node scenes = root.append_child("Scenes");
    for (const Scene& scene : project.scenes) {
      xml_node node = scenes.append_child("Scene");
      writeReferenceable(node, scene);
      writeContent(node, scene.content);
    }
  }
  return toText(document);
}

std::string serialize(const MetaData& metadata) {
  pugi::xml_document document = newDocument();
  xml_node root = document.append_child("MetaData");
  const auto field = [&root](const char* name, const std::optional<std::string>& value) {
    if (value) {
      checkText(*value, "MetaData", name);
      root.append_child(name).text().set(value->c_str());
    }
  };
  field("Title", metadata.title);
  field("Artist", metadata.artist);
  field("Album", metadata.album);
  field("OriginalArtist", metadata.originalArtist);
  field("Composer", metadata.composer);
  field("Songwriter", metadata.songwriter);
  field("Producer", metadata.producer);
  field("Arranger", metadata.arranger);
  field("Year", metadata.year);
  field("Genre", metadata.genre);
  field("Copyright", metadata.copyright);
  field("Website", metadata.website);
  field("Comment", metadata.comment);
  return toText(document);
}

}  // namespace dawproject
