// SPDX-License-Identifier: MIT
// The .dawproject zip container, built on miniz. Archives are streamed from and to disk through
// std::fstream (so that std::filesystem paths work on every platform): opening reads only the
// central directory, entries are decompressed on demand, and embedded files are compressed
// straight from their source file.
#include "dawproject/archive.hpp"

#include <ctime>
#include <fstream>
#include <mutex>
#include <random>
#include <set>
#include <string>
#include <utility>

#include "dawproject/error.hpp"

// miniz_rename.h first: it renames every miniz function declared by miniz.h.
// clang-format off
#include "miniz_rename.h"
#include "miniz.h"
// clang-format on

namespace dawproject {
namespace {

// A path as UTF-8 text in a std::string, in C++17 (where u8string() is a std::string) and in
// C++20 (where it is a std::u8string).
std::string pathText(const std::filesystem::path& path) {
  const auto text = path.u8string();
  return std::string(text.begin(), text.end());
}

// A path from UTF-8 text, without the u8path() deprecated in C++20.
std::filesystem::path pathFromUtf8(std::string_view text) {
#if defined(__cpp_char8_t)
  return std::filesystem::path(std::u8string(text.begin(), text.end()));
#else
  return std::filesystem::u8path(text.begin(), text.end());
#endif
}

// True when an archive entry name stays inside the directory it is extracted to: not empty, not
// absolute, no drive letter, no ".." component.
bool isSafeEntryName(std::string_view name) {
  if (name.empty() || name.front() == '/' || name.front() == '\\' ||
      name.find(':') != std::string_view::npos) {
    return false;
  }
  std::size_t start = 0;
  while (start <= name.size()) {
    std::size_t end = name.find_first_of("/\\", start);
    if (end == std::string_view::npos) {
      end = name.size();
    }
    if (name.substr(start, end - start) == "..") {
      return false;
    }
    start = end + 1;
  }
  return true;
}

std::string zipError(mz_zip_archive& zip) {
  return mz_zip_get_error_string(mz_zip_get_last_error(&zip));
}

// miniz callbacks over std::fstream. Each one seeks first: miniz addresses absolute offsets.
std::size_t readAt(void* opaque, mz_uint64 offset, void* buffer, std::size_t size) {
  auto& in = *static_cast<std::ifstream*>(opaque);
  in.clear();
  if (!in.seekg(static_cast<std::streamoff>(offset))) {
    return 0;
  }
  in.read(static_cast<char*>(buffer), static_cast<std::streamsize>(size));
  return static_cast<std::size_t>(in.gcount());
}

std::size_t writeAt(void* opaque, mz_uint64 offset, const void* buffer, std::size_t size) {
  auto& out = *static_cast<std::ofstream*>(opaque);
  if (!out.seekp(static_cast<std::streamoff>(offset))) {
    return 0;
  }
  out.write(static_cast<const char*>(buffer), static_cast<std::streamsize>(size));
  return out ? size : 0;
}

// Appends decompressed bytes to a std::vector (offsets arrive in order).
std::size_t appendTo(void* opaque, mz_uint64 /*offset*/, const void* buffer, std::size_t size) {
  auto& bytes = *static_cast<std::vector<std::uint8_t>*>(opaque);
  const auto* begin = static_cast<const std::uint8_t*>(buffer);
  bytes.insert(bytes.end(), begin, begin + size);
  return size;
}

// Appends decompressed bytes to an output file.
std::size_t appendToFile(void* opaque, mz_uint64 /*offset*/, const void* buffer, std::size_t size) {
  auto& out = *static_cast<std::ofstream*>(opaque);
  out.write(static_cast<const char*>(buffer), static_cast<std::streamsize>(size));
  return out ? size : 0;
}

std::ifstream openForReading(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    throw Error(ErrorKind::Io, "cannot open '" + pathText(path) + "' for reading");
  }
  return in;
}

std::uint64_t sizeOf(std::ifstream& in, const std::filesystem::path& path) {
  in.seekg(0, std::ios::end);
  const std::streamoff size = in.tellg();
  if (size < 0) {
    throw Error(ErrorKind::Io, "cannot read '" + pathText(path) + "'");
  }
  in.seekg(0, std::ios::beg);
  return static_cast<std::uint64_t>(size);
}

// A temporary file next to the target, removed unless commit() renamed it over the target.
class TemporarySibling {
 public:
  explicit TemporarySibling(std::filesystem::path target) : target_(std::move(target)) {
    std::random_device random;
    path_ = target_;
    path_ += pathFromUtf8(".tmp" + std::to_string(random()));
  }
  TemporarySibling(const TemporarySibling&) = delete;
  TemporarySibling& operator=(const TemporarySibling&) = delete;
  ~TemporarySibling() {
    if (!committed_) {
      std::error_code ec;
      std::filesystem::remove(path_, ec);
    }
  }

  [[nodiscard]] const std::filesystem::path& path() const { return path_; }

  void commit() {
    std::error_code ec;
    std::filesystem::rename(path_, target_, ec);
    if (ec) {
      throw Error(ErrorKind::Io, "cannot replace '" + pathText(target_) + "': " + ec.message());
    }
    committed_ = true;
  }

 private:
  std::filesystem::path target_;
  std::filesystem::path path_;
  bool committed_ = false;
};

}  // namespace

// The opened archive: the file stream and the miniz reader over it.
struct Archive::Impl {
  mutable std::ifstream file;
  mutable mz_zip_archive zip{};
  bool open = false;
  mutable std::mutex mutex;  // the miniz reader and the stream keep state between calls

  Impl() = default;
  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;
  ~Impl() {
    if (open) {
      mz_zip_reader_end(&zip);
    }
  }

  // Index of a file entry, or -1.
  int find(std::string_view entry) const {
    const std::string name(entry);
    const int index =
        mz_zip_reader_locate_file(&zip, name.c_str(), nullptr, MZ_ZIP_FLAG_CASE_SENSITIVE);
    if (index < 0 || mz_zip_reader_is_file_a_directory(&zip, static_cast<mz_uint>(index))) {
      return -1;
    }
    return index;
  }

  int require(std::string_view entry) const {
    const int index = find(entry);
    if (index < 0) {
      throw Error(ErrorKind::Zip, "archive has no entry '" + std::string(entry) + "'");
    }
    return index;
  }
};

Archive::Archive(std::shared_ptr<const Impl> impl) : impl_(std::move(impl)) {}

Archive Archive::open(const std::filesystem::path& path) {
  auto impl = std::make_shared<Impl>();
  impl->file = openForReading(path);
  const std::uint64_t size = sizeOf(impl->file, path);
  impl->zip.m_pRead = readAt;
  impl->zip.m_pIO_opaque = &impl->file;
  if (!mz_zip_reader_init(&impl->zip, size, 0)) {
    throw Error(ErrorKind::Zip,
                "'" + pathText(path) + "' is not a zip archive (" + zipError(impl->zip) + ")");
  }
  impl->open = true;
  return Archive(std::move(impl));
}

std::vector<std::string> Archive::entries() const {
  const std::lock_guard<std::mutex> lock(impl_->mutex);
  std::vector<std::string> names;
  const mz_uint count = mz_zip_reader_get_num_files(&impl_->zip);
  for (mz_uint i = 0; i < count; ++i) {
    mz_zip_archive_file_stat stat;
    if (mz_zip_reader_file_stat(&impl_->zip, i, &stat) && !stat.m_is_directory) {
      names.emplace_back(stat.m_filename);
    }
  }
  return names;
}

bool Archive::contains(std::string_view entry) const {
  const std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->find(entry) >= 0;
}

std::vector<std::uint8_t> Archive::read(std::string_view entry) const {
  const std::lock_guard<std::mutex> lock(impl_->mutex);
  const int index = impl_->require(entry);
  std::vector<std::uint8_t> bytes;
  if (!mz_zip_reader_extract_to_callback(&impl_->zip, static_cast<mz_uint>(index), appendTo, &bytes,
                                         0)) {
    throw Error(ErrorKind::Zip,
                "cannot decompress '" + std::string(entry) + "' (" + zipError(impl_->zip) + ")");
  }
  return bytes;
}

std::filesystem::path Archive::extract(std::string_view entry,
                                       const std::filesystem::path& destinationDir) const {
  const auto refuse = [&entry]() {
    return Error(ErrorKind::Zip,
                 "refusing to extract '" + std::string(entry) + "': it leaves the destination");
  };
  if (!isSafeEntryName(entry)) {
    throw refuse();
  }
  const std::filesystem::path root = std::filesystem::absolute(destinationDir).lexically_normal();
  const std::filesystem::path target = (root / pathFromUtf8(entry)).lexically_normal();
  const std::filesystem::path inside = target.lexically_relative(root);
  if (inside.empty() || inside == "." || *inside.begin() == "..") {
    throw refuse();
  }

  const std::lock_guard<std::mutex> lock(impl_->mutex);
  const int index = impl_->require(entry);
  std::error_code ec;
  std::filesystem::create_directories(target.parent_path(), ec);
  if (ec) {
    throw Error(ErrorKind::Io,
                "cannot create '" + pathText(target.parent_path()) + "': " + ec.message());
  }
  std::ofstream out(target, std::ios::binary | std::ios::trunc);
  if (!out) {
    throw Error(ErrorKind::Io, "cannot open '" + pathText(target) + "' for writing");
  }
  const bool extracted = mz_zip_reader_extract_to_callback(&impl_->zip, static_cast<mz_uint>(index),
                                                           appendToFile, &out, 0);
  out.close();
  if (!out) {
    throw Error(ErrorKind::Io, "cannot write '" + pathText(target) + "'");
  }
  if (!extracted) {
    throw Error(ErrorKind::Zip,
                "cannot decompress '" + std::string(entry) + "' (" + zipError(impl_->zip) + ")");
  }
  return target;
}

Project Archive::project(const ParseOptions& options) const {
  if (!contains(kProjectEntry)) {
    throw Error(ErrorKind::Zip, "archive has no project.xml");
  }
  const std::vector<std::uint8_t> bytes = read(kProjectEntry);
  return parseProject(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()),
                      options);
}

MetaData Archive::metadata(const ParseOptions& options) const {
  if (!contains(kMetaDataEntry)) {
    return MetaData{};
  }
  const std::vector<std::uint8_t> bytes = read(kMetaDataEntry);
  return parseMetaData(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()),
                       options);
}

void save(const std::filesystem::path& path, const Project& project, const MetaData& metadata,
          const std::vector<EmbeddedFile>& files, const std::vector<EmbeddedBytes>& bytes) {
  // Everything that can be checked without touching the disk is checked first.
  std::set<std::string> names;
  const auto checkName = [&names](const std::string& name) {
    if (!isSafeEntryName(name) || name.back() == '/') {
      throw Error(ErrorKind::Zip, "invalid archive path '" + name + "'");
    }
    if (!names.insert(name).second) {
      throw Error(ErrorKind::Zip, "archive path '" + name + "' is used twice");
    }
  };
  checkName(std::string(kProjectEntry));
  checkName(std::string(kMetaDataEntry));
  for (const EmbeddedFile& file : files) {
    checkName(file.archivePath);
  }
  for (const EmbeddedBytes& entry : bytes) {
    checkName(entry.archivePath);
  }
  const std::string projectXml = serialize(project);
  const std::string metadataXml = serialize(metadata);

  // The archive is written next to the target and renamed over it once complete, so a failure
  // leaves any previous file intact.
  TemporarySibling temporary(path);
  std::ofstream out(temporary.path(), std::ios::binary | std::ios::trunc);
  if (!out) {
    throw Error(ErrorKind::Io, "cannot open '" + pathText(temporary.path()) + "' for writing");
  }

  // Owns the miniz writer so it is released on every exit path.
  struct Writer {
    mz_zip_archive zip{};
    bool open = false;
    ~Writer() {
      if (open) {
        mz_zip_writer_end(&zip);
      }
    }
  } writer;
  writer.zip.m_pWrite = writeAt;
  writer.zip.m_pIO_opaque = &out;
  if (!mz_zip_writer_init_v2(&writer.zip, 0, 0)) {
    throw Error(ErrorKind::Zip, "cannot start a zip archive (" + zipError(writer.zip) + ")");
  }
  writer.open = true;
  const auto failed = [&](const std::string& name) {
    if (!out) {
      return Error(ErrorKind::Io, "cannot write '" + pathText(temporary.path()) + "'");
    }
    return Error(ErrorKind::Zip,
                 "cannot add '" + name + "' to the archive (" + zipError(writer.zip) + ")");
  };
  const auto addMemory = [&](const std::string& name, const void* data, std::size_t size) {
    if (!mz_zip_writer_add_mem(&writer.zip, name.c_str(), data, size,
                               static_cast<mz_uint>(MZ_DEFAULT_COMPRESSION))) {
      throw failed(name);
    }
  };

  addMemory(std::string(kProjectEntry), projectXml.data(), projectXml.size());
  addMemory(std::string(kMetaDataEntry), metadataXml.data(), metadataXml.size());
  const MZ_TIME_T now = std::time(nullptr);
  for (const EmbeddedFile& file : files) {
    std::ifstream in = openForReading(file.source);
    const std::uint64_t size = sizeOf(in, file.source);
    if (!mz_zip_writer_add_read_buf_callback(
            &writer.zip, file.archivePath.c_str(), readAt, &in, size, &now, nullptr, 0,
            static_cast<mz_uint>(MZ_DEFAULT_COMPRESSION), nullptr, 0, nullptr, 0)) {
      if (in.bad()) {
        throw Error(ErrorKind::Io, "cannot read '" + pathText(file.source) + "'");
      }
      throw failed(file.archivePath);
    }
  }
  for (const EmbeddedBytes& entry : bytes) {
    addMemory(entry.archivePath, entry.bytes.data(), entry.bytes.size());
  }

  if (!mz_zip_writer_finalize_archive(&writer.zip)) {
    throw failed("the central directory");
  }
  out.close();
  if (!out) {
    throw Error(ErrorKind::Io, "cannot write '" + pathText(temporary.path()) + "'");
  }
  temporary.commit();
}

Document load(const std::filesystem::path& path, const ParseOptions& options) {
  const Archive archive = Archive::open(path);
  Document document;
  document.project = archive.project(options);
  document.metadata = archive.metadata(options);
  return document;
}

}  // namespace dawproject
