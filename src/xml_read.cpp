// SPDX-License-Identifier: MIT
// Parsing of project.xml and metadata.xml into the model.
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "dawproject/error.hpp"
#include "dawproject/xml.hpp"
#include "numbers.hpp"
#include "pugixml.hpp"

namespace dawproject {
namespace {

using pugi::xml_node;

[[noreturn]] void schemaError(const std::string& path, const std::string& message) {
  throw Error(ErrorKind::Schema, path + ": " + message);
}

bool isNamespaceAttribute(std::string_view name) {
  return name == "xmlns" || name.substr(0, 6) == "xmlns:" || name.substr(0, 4) == "xsi:";
}

// Deepest element nesting the reader follows. Timelines and tracks nest recursively, so a
// hostile document could otherwise exhaust the stack.
constexpr int kMaxDepth = 256;

// Number of Elem objects alive on this thread: the nesting depth of the element being read.
thread_local int elementDepth = 0;

// One XML element being read. Every attribute read is remembered so that, in strict mode,
// finish() can reject attributes nobody asked for.
class Elem {
 public:
  Elem(xml_node node, std::string path, bool strict)
      : node_(node), path_(std::move(path)), strict_(strict) {
    if (++elementDepth > kMaxDepth) {
      --elementDepth;
      schemaError(path_.substr(0, 200) + "...", "nesting too deep");
    }
  }
  ~Elem() { --elementDepth; }

  Elem(const Elem&) = delete;
  Elem& operator=(const Elem&) = delete;

  [[nodiscard]] xml_node node() const { return node_; }
  [[nodiscard]] const std::string& path() const { return path_; }
  [[nodiscard]] bool strict() const { return strict_; }

  [[nodiscard]] std::string childPath(xml_node child) const { return path_ + "/" + child.name(); }

  // Raw attribute text, marking the attribute as known.
  std::optional<std::string_view> raw(const char* name) {
    used_.push_back(name);
    const pugi::xml_attribute attribute = node_.attribute(name);
    if (!attribute) {
      return std::nullopt;
    }
    return std::string_view(attribute.value());
  }

  std::string_view requiredRaw(const char* name) {
    const auto value = raw(name);
    if (!value) {
      schemaError(path_, std::string("missing required attribute '") + name + "'");
    }
    return *value;
  }

  std::optional<std::string> optString(const char* name) {
    const auto value = raw(name);
    return value ? std::optional<std::string>(std::string(*value)) : std::nullopt;
  }

  std::string requiredString(const char* name) { return std::string(requiredRaw(name)); }

  double toDouble(const char* name, std::string_view text) const {
    double value = 0.0;
    if (!parseDouble(text, value)) {
      schemaError(path_, std::string("attribute '") + name + "' is not a number: '" +
                             std::string(text) + "'");
    }
    return value;
  }

  std::optional<double> optDouble(const char* name) {
    const auto value = raw(name);
    if (!value) {
      return std::nullopt;
    }
    return toDouble(name, *value);
  }

  double requiredDouble(const char* name) { return toDouble(name, requiredRaw(name)); }

  int toInt(const char* name, std::string_view text) const {
    int value = 0;
    if (!detail::parseInt(text, value)) {
      schemaError(path_, std::string("attribute '") + name + "' is not an integer: '" +
                             std::string(text) + "'");
    }
    return value;
  }

  std::optional<int> optInt(const char* name) {
    const auto value = raw(name);
    if (!value) {
      return std::nullopt;
    }
    return toInt(name, *value);
  }

  int requiredInt(const char* name) { return toInt(name, requiredRaw(name)); }

  bool toBool(const char* name, std::string_view text) const {
    bool value = false;
    if (!detail::parseBool(text, value)) {
      schemaError(path_, std::string("attribute '") + name + "' is not a boolean: '" +
                             std::string(text) + "'");
    }
    return value;
  }

  std::optional<bool> optBool(const char* name) {
    const auto value = raw(name);
    if (!value) {
      return std::nullopt;
    }
    return toBool(name, *value);
  }

  template <class E>
  std::optional<E> optEnum(const char* name) {
    const auto value = raw(name);
    if (!value) {
      return std::nullopt;
    }
    const std::optional<E> parsed = from_string<E>(*value);
    if (!parsed && strict_) {
      schemaError(path_, std::string("attribute '") + name + "' has an unknown value '" +
                             std::string(*value) + "'");
    }
    return parsed;
  }

  template <class E>
  E requiredEnum(const char* name) {
    const std::string_view value = requiredRaw(name);
    const std::optional<E> parsed = from_string<E>(value);
    if (!parsed) {
      schemaError(path_, std::string("attribute '") + name + "' has an unknown value '" +
                             std::string(value) + "'");
    }
    return *parsed;
  }

  // Unknown child element: an error in strict mode, ignored otherwise.
  void unknownChild(xml_node child) const {
    if (strict_) {
      schemaError(childPath(child), "unknown element");
    }
  }

  // A child element the schema allows at most once: in strict mode a second one throws.
  void single(xml_node child) {
    if (!strict_) {
      return;
    }
    const std::string name = child.name();
    for (const std::string& seen : seen_) {
      if (seen == name) {
        schemaError(childPath(child), "element appears more than once");
      }
    }
    seen_.push_back(name);
  }

  // In strict mode, rejects attributes that were never read.
  void finish() const {
    if (!strict_) {
      return;
    }
    for (const pugi::xml_attribute attribute : node_.attributes()) {
      const std::string_view name = attribute.name();
      if (isNamespaceAttribute(name)) {
        continue;
      }
      bool known = false;
      for (const char* used : used_) {
        if (name == used) {
          known = true;
          break;
        }
      }
      if (!known) {
        schemaError(path_, "unknown attribute '" + std::string(name) + "'");
      }
    }
  }

 private:
  xml_node node_;
  std::string path_;
  bool strict_;
  std::vector<const char*> used_;
  std::vector<std::string> seen_;
};

// Calls `visit(child)` for every child element.
template <class Visit>
void forEachChild(const Elem& elem, Visit&& visit) {
  for (xml_node child : elem.node().children()) {
    if (child.type() == pugi::node_element) {
      visit(child);
    }
  }
}

bool is(xml_node node, const char* name) { return std::strcmp(node.name(), name) == 0; }

void readNameable(Elem& e, Nameable& value) {
  value.name = e.optString("name");
  value.color = e.optString("color");
  value.comment = e.optString("comment");
}

void readReferenceable(Elem& e, Referenceable& value) {
  readNameable(e, value);
  value.id = e.optString("id");
}

// Elements without children: in strict mode any child is unknown.
void noChildren(const Elem& e) {
  forEachChild(e, [&e](xml_node child) { e.unknownChild(child); });
}

// Parameters ----------------------------------------------------------------------------------

void readParameterBase(Elem& e, Parameter& value) {
  readReferenceable(e, value);
  value.parameterID = e.optInt("parameterID");
}

RealParameter readRealParameter(xml_node node, const std::string& path, bool strict) {
  Elem e(node, path, strict);
  RealParameter value;
  readParameterBase(e, value);
  value.value = e.optDouble("value");
  value.unit = e.requiredEnum<Unit>("unit");
  value.min = e.optDouble("min");
  value.max = e.optDouble("max");
  noChildren(e);
  e.finish();
  return value;
}

BoolParameter readBoolParameter(xml_node node, const std::string& path, bool strict) {
  Elem e(node, path, strict);
  BoolParameter value;
  readParameterBase(e, value);
  value.value = e.optBool("value");
  noChildren(e);
  e.finish();
  return value;
}

IntegerParameter readIntegerParameter(xml_node node, const std::string& path, bool strict) {
  Elem e(node, path, strict);
  IntegerParameter value;
  readParameterBase(e, value);
  value.value = e.optInt("value");
  value.min = e.optInt("min");
  value.max = e.optInt("max");
  noChildren(e);
  e.finish();
  return value;
}

std::vector<std::string_view> splitList(std::string_view text) {
  std::vector<std::string_view> items;
  std::size_t pos = 0;
  while (pos < text.size()) {
    while (pos < text.size() &&
           (text[pos] == ' ' || text[pos] == '\t' || text[pos] == '\n' || text[pos] == '\r')) {
      ++pos;
    }
    std::size_t end = pos;
    while (end < text.size() && text[end] != ' ' && text[end] != '\t' && text[end] != '\n' &&
           text[end] != '\r') {
      ++end;
    }
    if (end > pos) {
      items.push_back(text.substr(pos, end - pos));
    }
    pos = end;
  }
  return items;
}

EnumParameter readEnumParameter(xml_node node, const std::string& path, bool strict) {
  Elem e(node, path, strict);
  EnumParameter value;
  readParameterBase(e, value);
  value.value = e.optInt("value");
  value.count = e.requiredInt("count");
  if (const auto labels = e.raw("labels")) {
    for (const std::string_view label : splitList(*labels)) {
      value.labels.emplace_back(label);
    }
  }
  noChildren(e);
  e.finish();
  return value;
}

TimeSignatureParameter readTimeSignatureParameter(xml_node node, const std::string& path,
                                                  bool strict) {
  Elem e(node, path, strict);
  TimeSignatureParameter value;
  readParameterBase(e, value);
  value.numerator = e.requiredInt("numerator");
  value.denominator = e.requiredInt("denominator");
  noChildren(e);
  e.finish();
  return value;
}

std::unique_ptr<Parameter> readParameter(xml_node node, const std::string& path, bool strict) {
  if (is(node, "RealParameter")) {
    return std::make_unique<RealParameter>(readRealParameter(node, path, strict));
  }
  if (is(node, "BoolParameter")) {
    return std::make_unique<BoolParameter>(readBoolParameter(node, path, strict));
  }
  if (is(node, "IntegerParameter")) {
    return std::make_unique<IntegerParameter>(readIntegerParameter(node, path, strict));
  }
  if (is(node, "EnumParameter")) {
    return std::make_unique<EnumParameter>(readEnumParameter(node, path, strict));
  }
  if (is(node, "TimeSignatureParameter")) {
    return std::make_unique<TimeSignatureParameter>(readTimeSignatureParameter(node, path, strict));
  }
  return nullptr;
}

FileReference readFileReference(xml_node node, const std::string& path, bool strict) {
  Elem e(node, path, strict);
  FileReference value;
  value.path = e.requiredString("path");
  value.external = e.optBool("external");
  noChildren(e);
  e.finish();
  return value;
}

// Timelines -----------------------------------------------------------------------------------

std::unique_ptr<Timeline> readTimeline(xml_node node, const std::string& path, bool strict);

void readTimelineBase(Elem& e, Timeline& value) {
  readReferenceable(e, value);
  value.timeUnit = e.optEnum<TimeUnit>("timeUnit");
  value.track = e.optString("track");
}

// Reads the single timeline child of a Clip, Note or Scene.
void readContentChild(const Elem& e, xml_node child, std::unique_ptr<Timeline>& content) {
  std::unique_ptr<Timeline> timeline = readTimeline(child, e.childPath(child), e.strict());
  if (!timeline) {
    e.unknownChild(child);
    return;
  }
  if (content) {
    if (e.strict()) {
      schemaError(e.childPath(child), "more than one content timeline");
    }
    return;
  }
  content = std::move(timeline);
}

Clip readClip(xml_node node, const std::string& path, bool strict) {
  Elem e(node, path, strict);
  Clip clip;
  readNameable(e, clip);
  clip.time = e.requiredDouble("time");
  clip.duration = e.optDouble("duration");
  clip.contentTimeUnit = e.optEnum<TimeUnit>("contentTimeUnit");
  clip.playStart = e.optDouble("playStart");
  clip.playStop = e.optDouble("playStop");
  clip.loopStart = e.optDouble("loopStart");
  clip.loopEnd = e.optDouble("loopEnd");
  clip.fadeTimeUnit = e.optEnum<TimeUnit>("fadeTimeUnit");
  clip.fadeInTime = e.optDouble("fadeInTime");
  clip.fadeOutTime = e.optDouble("fadeOutTime");
  clip.enable = e.optBool("enable");
  clip.reference = e.optString("reference");
  forEachChild(e, [&](xml_node child) { readContentChild(e, child, clip.content); });
  e.finish();
  return clip;
}

Note readNote(xml_node node, const std::string& path, bool strict) {
  Elem e(node, path, strict);
  Note note;
  note.time = e.requiredDouble("time");
  note.duration = e.requiredDouble("duration");
  note.channel = e.requiredInt("channel");
  note.key = e.requiredInt("key");
  note.vel = e.optDouble("vel");
  note.rel = e.optDouble("rel");
  forEachChild(e, [&](xml_node child) { readContentChild(e, child, note.content); });
  e.finish();
  return note;
}

Point readPoint(xml_node node, const std::string& path, bool strict, bool& known) {
  Elem e(node, path, strict);
  known = true;
  Point point;
  if (is(node, "RealPoint")) {
    RealPoint p;
    p.time = e.requiredDouble("time");
    p.value = e.requiredDouble("value");
    p.interpolation = e.optEnum<Interpolation>("interpolation");
    point = p;
  } else if (is(node, "EnumPoint")) {
    point = EnumPoint{e.requiredDouble("time"), e.requiredInt("value")};
  } else if (is(node, "BoolPoint")) {
    const double time = e.requiredDouble("time");
    point = BoolPoint{time, e.toBool("value", e.requiredRaw("value"))};
  } else if (is(node, "IntegerPoint")) {
    point = IntegerPoint{e.requiredDouble("time"), e.requiredInt("value")};
  } else if (is(node, "TimeSignaturePoint")) {
    TimeSignaturePoint p;
    p.time = e.requiredDouble("time");
    p.numerator = e.requiredInt("numerator");
    p.denominator = e.requiredInt("denominator");
    point = p;
  } else {
    known = false;
    return point;
  }
  noChildren(e);
  e.finish();
  return point;
}

std::unique_ptr<Points> readPoints(xml_node node, const std::string& path, bool strict) {
  Elem e(node, path, strict);
  auto points = std::make_unique<Points>();
  readTimelineBase(e, *points);
  points->unit = e.optEnum<Unit>("unit");
  bool hasTarget = false;
  forEachChild(e, [&](xml_node child) {
    if (is(child, "Target")) {
      e.single(child);
      Elem t(child, e.childPath(child), strict);
      points->target.parameter = t.optString("parameter");
      points->target.expression = t.optEnum<ExpressionType>("expression");
      points->target.channel = t.optInt("channel");
      points->target.key = t.optInt("key");
      points->target.controller = t.optInt("controller");
      noChildren(t);
      t.finish();
      hasTarget = true;
      return;
    }
    bool known = false;
    Point point = readPoint(child, e.childPath(child), strict, known);
    if (known) {
      points->points.push_back(point);
    } else {
      e.unknownChild(child);
    }
  });
  if (!hasTarget) {
    schemaError(path, "missing required element 'Target'");
  }
  e.finish();
  return points;
}

std::unique_ptr<Markers> readMarkers(xml_node node, const std::string& path, bool strict) {
  Elem e(node, path, strict);
  auto markers = std::make_unique<Markers>();
  readTimelineBase(e, *markers);
  forEachChild(e, [&](xml_node child) {
    if (!is(child, "Marker")) {
      e.unknownChild(child);
      return;
    }
    Elem m(child, e.childPath(child), strict);
    Marker marker;
    readNameable(m, marker);
    marker.time = m.requiredDouble("time");
    noChildren(m);
    m.finish();
    markers->markers.push_back(std::move(marker));
  });
  e.finish();
  return markers;
}

std::unique_ptr<Lanes> readLanes(xml_node node, const std::string& path, bool strict) {
  Elem e(node, path, strict);
  auto lanes = std::make_unique<Lanes>();
  readTimelineBase(e, *lanes);
  forEachChild(e, [&](xml_node child) {
    std::unique_ptr<Timeline> lane = readTimeline(child, e.childPath(child), strict);
    if (lane) {
      lanes->lanes.push_back(std::move(lane));
    } else {
      e.unknownChild(child);
    }
  });
  e.finish();
  return lanes;
}

template <class Media>
std::unique_ptr<Timeline> readMedia(xml_node node, const std::string& path, bool strict) {
  Elem e(node, path, strict);
  auto media = std::make_unique<Media>();
  readTimelineBase(e, *media);
  media->duration = e.requiredDouble("duration");
  media->channels = e.requiredInt("channels");
  media->sampleRate = e.requiredInt("sampleRate");
  media->algorithm = e.optString("algorithm");
  bool hasFile = false;
  forEachChild(e, [&](xml_node child) {
    if (is(child, "File")) {
      e.single(child);
      media->file = readFileReference(child, e.childPath(child), strict);
      hasFile = true;
    } else {
      e.unknownChild(child);
    }
  });
  if (!hasFile) {
    schemaError(path, "missing required element 'File'");
  }
  e.finish();
  return media;
}

std::unique_ptr<Timeline> readTimeline(xml_node node, const std::string& path, bool strict) {
  if (is(node, "Lanes")) {
    return readLanes(node, path, strict);
  }
  if (is(node, "Notes")) {
    Elem e(node, path, strict);
    auto notes = std::make_unique<Notes>();
    readTimelineBase(e, *notes);
    forEachChild(e, [&](xml_node child) {
      if (is(child, "Note")) {
        notes->notes.push_back(readNote(child, e.childPath(child), strict));
      } else {
        e.unknownChild(child);
      }
    });
    e.finish();
    return notes;
  }
  if (is(node, "Clips")) {
    Elem e(node, path, strict);
    auto clips = std::make_unique<Clips>();
    readTimelineBase(e, *clips);
    forEachChild(e, [&](xml_node child) {
      if (is(child, "Clip")) {
        clips->clips.push_back(readClip(child, e.childPath(child), strict));
      } else {
        e.unknownChild(child);
      }
    });
    e.finish();
    return clips;
  }
  if (is(node, "ClipSlot")) {
    Elem e(node, path, strict);
    auto slot = std::make_unique<ClipSlot>();
    readTimelineBase(e, *slot);
    slot->hasStop = e.optBool("hasStop");
    forEachChild(e, [&](xml_node child) {
      if (is(child, "Clip") && !slot->clip) {
        slot->clip = readClip(child, e.childPath(child), strict);
      } else {
        e.unknownChild(child);
      }
    });
    e.finish();
    return slot;
  }
  // The schema names the timeline element "markers"; "Markers" is only the Arrangement child,
  // tolerated here in lenient mode.
  if (is(node, "markers") || (!strict && is(node, "Markers"))) {
    return readMarkers(node, path, strict);
  }
  if (is(node, "Warps")) {
    Elem e(node, path, strict);
    auto warps = std::make_unique<Warps>();
    readTimelineBase(e, *warps);
    warps->contentTimeUnit = e.requiredEnum<TimeUnit>("contentTimeUnit");
    forEachChild(e, [&](xml_node child) {
      if (is(child, "Warp")) {
        Elem w(child, e.childPath(child), strict);
        Warp warp;
        warp.time = w.requiredDouble("time");
        warp.contentTime = w.requiredDouble("contentTime");
        noChildren(w);
        w.finish();
        warps->warps.push_back(warp);
      } else {
        readContentChild(e, child, warps->content);
      }
    });
    e.finish();
    return warps;
  }
  if (is(node, "Audio")) {
    return readMedia<Audio>(node, path, strict);
  }
  if (is(node, "Video")) {
    return readMedia<Video>(node, path, strict);
  }
  if (is(node, "Points")) {
    return readPoints(node, path, strict);
  }
  return nullptr;
}

// Devices -------------------------------------------------------------------------------------

std::unique_ptr<Device> newDevice(xml_node node) {
  if (is(node, "Device")) return std::make_unique<GenericDevice>();
  if (is(node, "Vst2Plugin")) return std::make_unique<Vst2Plugin>();
  if (is(node, "Vst3Plugin")) return std::make_unique<Vst3Plugin>();
  if (is(node, "ClapPlugin")) return std::make_unique<ClapPlugin>();
  if (is(node, "AuPlugin")) return std::make_unique<AuPlugin>();
  if (is(node, "BuiltinDevice")) return std::make_unique<BuiltinDevice>();
  if (is(node, "Equalizer")) return std::make_unique<Equalizer>();
  if (is(node, "Compressor")) return std::make_unique<Compressor>();
  if (is(node, "NoiseGate")) return std::make_unique<NoiseGate>();
  if (is(node, "Limiter")) return std::make_unique<Limiter>();
  return nullptr;
}

// Reads a child of a built-in device; false when the element is not one of its fields.
bool readBuiltinChild(Device& device, xml_node child, const std::string& path, bool strict) {
  const auto real = [&](std::optional<RealParameter>& slot) {
    slot = readRealParameter(child, path, strict);
    return true;
  };
  switch (device.kind()) {
    case DeviceKind::Equalizer: {
      auto& eq = static_cast<Equalizer&>(device);
      if (is(child, "Band")) {
        Elem b(child, path, strict);
        EqBand band;
        band.type = b.requiredEnum<EqBandType>("type");
        band.order = b.optInt("order");
        bool hasFreq = false;
        forEachChild(b, [&](xml_node c) {
          const std::string p = b.childPath(c);
          if (is(c, "Freq") || is(c, "Gain") || is(c, "Q") || is(c, "Enabled")) {
            b.single(c);
          }
          if (is(c, "Freq")) {
            band.freq = readRealParameter(c, p, strict);
            hasFreq = true;
          } else if (is(c, "Gain")) {
            band.gain = readRealParameter(c, p, strict);
          } else if (is(c, "Q")) {
            band.q = readRealParameter(c, p, strict);
          } else if (is(c, "Enabled")) {
            band.enabled = readBoolParameter(c, p, strict);
          } else {
            b.unknownChild(c);
          }
        });
        if (!hasFreq) {
          schemaError(path, "missing required element 'Freq'");
        }
        b.finish();
        eq.bands.push_back(std::move(band));
        return true;
      }
      if (is(child, "InputGain")) return real(eq.inputGain);
      if (is(child, "OutputGain")) return real(eq.outputGain);
      return false;
    }
    case DeviceKind::Compressor: {
      auto& c = static_cast<Compressor&>(device);
      if (is(child, "Attack")) return real(c.attack);
      if (is(child, "AutoMakeup")) {
        c.autoMakeup = readBoolParameter(child, path, strict);
        return true;
      }
      if (is(child, "InputGain")) return real(c.inputGain);
      if (is(child, "OutputGain")) return real(c.outputGain);
      if (is(child, "Ratio")) return real(c.ratio);
      if (is(child, "Release")) return real(c.release);
      if (is(child, "Threshold")) return real(c.threshold);
      return false;
    }
    case DeviceKind::NoiseGate: {
      auto& g = static_cast<NoiseGate&>(device);
      if (is(child, "Attack")) return real(g.attack);
      if (is(child, "Range")) return real(g.range);
      if (is(child, "Ratio")) return real(g.ratio);
      if (is(child, "Release")) return real(g.release);
      if (is(child, "Threshold")) return real(g.threshold);
      return false;
    }
    case DeviceKind::Limiter: {
      auto& l = static_cast<Limiter&>(device);
      if (is(child, "Attack")) return real(l.attack);
      if (is(child, "InputGain")) return real(l.inputGain);
      if (is(child, "OutputGain")) return real(l.outputGain);
      if (is(child, "Release")) return real(l.release);
      if (is(child, "Threshold")) return real(l.threshold);
      return false;
    }
    default:
      return false;
  }
}

std::unique_ptr<Device> readDevice(xml_node node, const std::string& path, bool strict) {
  std::unique_ptr<Device> device = newDevice(node);
  if (!device) {
    return nullptr;
  }
  Elem e(node, path, strict);
  readReferenceable(e, *device);
  device->deviceID = e.optString("deviceID");
  device->deviceName = e.requiredString("deviceName");
  device->deviceRole = e.requiredEnum<DeviceRole>("deviceRole");
  device->deviceVendor = e.optString("deviceVendor");
  device->loaded = e.optBool("loaded");
  if (auto* plugin = dynamic_cast<Plugin*>(device.get())) {
    plugin->pluginVersion = e.optString("pluginVersion");
  }
  forEachChild(e, [&](xml_node child) {
    const std::string childPath = e.childPath(child);
    if (is(child, "Parameters") || is(child, "Enabled") || is(child, "State")) {
      e.single(child);
    }
    if (is(child, "Parameters")) {
      Elem p(child, childPath, strict);
      forEachChild(p, [&](xml_node parameterNode) {
        auto parameter = readParameter(parameterNode, p.childPath(parameterNode), strict);
        if (parameter) {
          device->parameters.push_back(std::move(parameter));
        } else {
          p.unknownChild(parameterNode);
        }
      });
      p.finish();
    } else if (is(child, "Enabled")) {
      device->enabled = readBoolParameter(child, childPath, strict);
    } else if (is(child, "State")) {
      device->state = readFileReference(child, childPath, strict);
    } else if (readBuiltinChild(*device, child, childPath, strict)) {
      if (!is(child, "Band")) {
        e.single(child);
      }
    } else {
      e.unknownChild(child);
    }
  });
  e.finish();
  return device;
}

// Structure -----------------------------------------------------------------------------------

Send readSend(xml_node node, const std::string& path, bool strict) {
  Elem e(node, path, strict);
  Send send;
  readReferenceable(e, send);
  send.destination = e.optString("destination");
  send.type = e.optEnum<SendType>("type");
  bool hasVolume = false;
  forEachChild(e, [&](xml_node child) {
    const std::string childPath = e.childPath(child);
    e.single(child);
    if (is(child, "Enable")) {
      send.enable = readBoolParameter(child, childPath, strict);
    } else if (is(child, "Pan")) {
      send.pan = readRealParameter(child, childPath, strict);
    } else if (is(child, "Volume")) {
      send.volume = readRealParameter(child, childPath, strict);
      hasVolume = true;
    } else {
      e.unknownChild(child);
    }
  });
  if (!hasVolume) {
    schemaError(path, "missing required element 'Volume'");
  }
  e.finish();
  return send;
}

Channel readChannel(xml_node node, const std::string& path, bool strict) {
  Elem e(node, path, strict);
  Channel channel;
  readReferenceable(e, channel);
  channel.role = e.optEnum<MixerRole>("role");
  channel.audioChannels = e.optInt("audioChannels");
  channel.destination = e.optString("destination");
  channel.solo = e.optBool("solo");
  forEachChild(e, [&](xml_node child) {
    const std::string childPath = e.childPath(child);
    e.single(child);
    if (is(child, "Devices")) {
      Elem d(child, childPath, strict);
      forEachChild(d, [&](xml_node deviceNode) {
        auto device = readDevice(deviceNode, d.childPath(deviceNode), strict);
        if (device) {
          channel.devices.push_back(std::move(device));
        } else {
          d.unknownChild(deviceNode);
        }
      });
      d.finish();
    } else if (is(child, "Mute")) {
      channel.mute = readBoolParameter(child, childPath, strict);
    } else if (is(child, "Pan")) {
      channel.pan = readRealParameter(child, childPath, strict);
    } else if (is(child, "Sends")) {
      Elem s(child, childPath, strict);
      forEachChild(s, [&](xml_node sendNode) {
        if (is(sendNode, "Send")) {
          channel.sends.push_back(readSend(sendNode, s.childPath(sendNode), strict));
        } else {
          s.unknownChild(sendNode);
        }
      });
      s.finish();
    } else if (is(child, "Volume")) {
      channel.volume = readRealParameter(child, childPath, strict);
    } else {
      e.unknownChild(child);
    }
  });
  e.finish();
  return channel;
}

Track readTrack(xml_node node, const std::string& path, bool strict) {
  Elem e(node, path, strict);
  Track track;
  readReferenceable(e, track);
  if (const auto types = e.raw("contentType")) {
    for (const std::string_view item : splitList(*types)) {
      const std::optional<ContentType> type = from_string<ContentType>(item);
      if (type) {
        track.contentType.push_back(*type);
      } else if (strict) {
        schemaError(path,
                    "attribute 'contentType' has an unknown value '" + std::string(item) + "'");
      }
    }
  }
  track.loaded = e.optBool("loaded");
  forEachChild(e, [&](xml_node child) {
    const std::string childPath = e.childPath(child);
    if (is(child, "Channel")) {
      e.single(child);
      track.channel = readChannel(child, childPath, strict);
    } else if (is(child, "Track")) {
      track.tracks.push_back(readTrack(child, childPath, strict));
    } else {
      e.unknownChild(child);
    }
  });
  e.finish();
  return track;
}

// Project -------------------------------------------------------------------------------------

Arrangement readArrangement(xml_node node, const std::string& path, bool strict) {
  Elem e(node, path, strict);
  Arrangement arrangement;
  readReferenceable(e, arrangement);
  forEachChild(e, [&](xml_node child) {
    const std::string childPath = e.childPath(child);
    e.single(child);
    if (is(child, "Lanes")) {
      arrangement.lanes = readLanes(child, childPath, strict);
    } else if (is(child, "Markers")) {
      arrangement.markers = std::move(*readMarkers(child, childPath, strict));
    } else if (is(child, "TempoAutomation")) {
      arrangement.tempoAutomation = std::move(*readPoints(child, childPath, strict));
    } else if (is(child, "TimeSignatureAutomation")) {
      arrangement.timeSignatureAutomation = std::move(*readPoints(child, childPath, strict));
    } else {
      e.unknownChild(child);
    }
  });
  e.finish();
  return arrangement;
}

Scene readScene(xml_node node, const std::string& path, bool strict) {
  Elem e(node, path, strict);
  Scene scene;
  readReferenceable(e, scene);
  forEachChild(e, [&](xml_node child) { readContentChild(e, child, scene.content); });
  e.finish();
  return scene;
}

xml_node loadRoot(pugi::xml_document& document, std::string_view xml, const char* rootName) {
  const pugi::xml_parse_result result =
      document.load_buffer(xml.data(), xml.size(), pugi::parse_default, pugi::encoding_utf8);
  if (!result) {
    throw Error(ErrorKind::Xml, std::string("XML parse error at offset ") +
                                    std::to_string(result.offset) + ": " + result.description());
  }
  xml_node root = document.document_element();
  if (!is(root, rootName)) {
    throw Error(ErrorKind::Schema,
                std::string("root element is <") + root.name() + ">, expected <" + rootName + ">");
  }
  return root;
}

}  // namespace

Project parseProject(std::string_view xml, const ParseOptions& options) {
  pugi::xml_document document;
  const xml_node root = loadRoot(document, xml, "Project");
  const bool strict = options.strict;
  Elem e(root, "Project", strict);
  Project project;
  project.version = e.requiredString("version");
  bool hasApplication = false;
  forEachChild(e, [&](xml_node child) {
    const std::string path = e.childPath(child);
    e.single(child);
    if (is(child, "Application")) {
      Elem a(child, path, strict);
      project.application.name = a.requiredString("name");
      project.application.version = a.requiredString("version");
      noChildren(a);
      a.finish();
      hasApplication = true;
    } else if (is(child, "Transport")) {
      Elem t(child, path, strict);
      Transport transport;
      forEachChild(t, [&](xml_node c) {
        t.single(c);
        if (is(c, "Tempo")) {
          transport.tempo = readRealParameter(c, t.childPath(c), strict);
        } else if (is(c, "TimeSignature")) {
          transport.timeSignature = readTimeSignatureParameter(c, t.childPath(c), strict);
        } else {
          t.unknownChild(c);
        }
      });
      t.finish();
      project.transport = std::move(transport);
    } else if (is(child, "Structure")) {
      Elem s(child, path, strict);
      forEachChild(s, [&](xml_node c) {
        if (is(c, "Track")) {
          project.structure.emplace_back(readTrack(c, s.childPath(c), strict));
        } else if (is(c, "Channel")) {
          project.structure.emplace_back(readChannel(c, s.childPath(c), strict));
        } else {
          s.unknownChild(c);
        }
      });
      s.finish();
    } else if (is(child, "Arrangement")) {
      project.arrangement = readArrangement(child, path, strict);
    } else if (is(child, "Scenes")) {
      Elem s(child, path, strict);
      forEachChild(s, [&](xml_node c) {
        if (is(c, "Scene")) {
          project.scenes.push_back(readScene(c, s.childPath(c), strict));
        } else {
          s.unknownChild(c);
        }
      });
      s.finish();
    } else {
      e.unknownChild(child);
    }
  });
  if (!hasApplication) {
    schemaError("Project", "missing required element 'Application'");
  }
  e.finish();
  return project;
}

MetaData parseMetaData(std::string_view xml, const ParseOptions& options) {
  pugi::xml_document document;
  const xml_node root = loadRoot(document, xml, "MetaData");
  Elem e(root, "MetaData", options.strict);
  MetaData metadata;
  const std::pair<const char*, std::optional<std::string> MetaData::*> fields[] = {
      {"Title", &MetaData::title},         {"Artist", &MetaData::artist},
      {"Album", &MetaData::album},         {"OriginalArtist", &MetaData::originalArtist},
      {"Composer", &MetaData::composer},   {"Songwriter", &MetaData::songwriter},
      {"Producer", &MetaData::producer},   {"Arranger", &MetaData::arranger},
      {"Year", &MetaData::year},           {"Genre", &MetaData::genre},
      {"Copyright", &MetaData::copyright}, {"Website", &MetaData::website},
      {"Comment", &MetaData::comment},
  };
  forEachChild(e, [&](xml_node child) {
    for (const auto& [name, member] : fields) {
      if (is(child, name)) {
        e.single(child);
        metadata.*member = std::string(child.text().get());
        return;
      }
    }
    e.unknownChild(child);
  });
  e.finish();
  return metadata;
}

}  // namespace dawproject
