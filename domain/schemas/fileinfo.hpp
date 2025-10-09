#ifndef OWL_SCHEMAS_FILEINFO
#define OWL_SCHEMAS_FILEINFO

#include <cstring>
#include <infrastructure/result.hpp>
#include <optional>
#include <pipeline/pipeline.hpp>
#include <stdexcept>

#include "utils/constants.hpp"

namespace owl::schemas {

struct FileInfo {
  std::optional<mode_t> mode;
  std::optional<size_t> size;
  std::optional<std::vector<uint8_t>> content;
  std::optional<uid_t> uid;
  std::optional<gid_t> gid;
  std::optional<time_t> access_time;
  std::optional<time_t> modification_time;
  std::optional<time_t> create_time;
  std::optional<std::string> name;
  std::optional<std::string> path;
  std::vector<float> embedding;
  std::vector<uint8_t> pq_codes;
  std::vector<uint8_t> sq_codes;
  bool embedding_updated{false};
  bool is_quantized{false};
  bool created{false};
};

class FileInfoSerializer {
public:
  static std::vector<uint8_t> serialize(const schemas::FileInfo &file_info) {
    std::vector<uint8_t> data;

    const uint32_t VERSION = 1;
    serializeUint32(data, VERSION);

    serializeOptional(data, file_info.mode);
    serializeOptional(data, file_info.size);
    serializeOptionalVector(data, file_info.content);
    serializeOptional(data, file_info.uid);
    serializeOptional(data, file_info.gid);
    serializeOptional(data, file_info.access_time);
    serializeOptional(data, file_info.modification_time);
    serializeOptional(data, file_info.create_time);

    serializeOptionalString(data, file_info.name);
    serializeOptionalString(data, file_info.path);

    serializeVector(data, file_info.embedding);
    serializeVector(data, file_info.pq_codes);
    serializeVector(data, file_info.sq_codes);

    serializeBool(data, file_info.embedding_updated);
    serializeBool(data, file_info.is_quantized);
    serializeBool(data, file_info.created);

    return data;
  }

  static core::Result<schemas::FileInfo>
  deserialize(const std::vector<uint8_t> &data) {
    if (data.empty()) {
      return core::Result<schemas::FileInfo>::Error(
          "Empty data for deserialization");
    }

    schemas::FileInfo file_info;
    size_t offset = 0;

    try {
      uint32_t version = deserializeUint32(data, offset);
      if (version != 1) {
        return core::Result<schemas::FileInfo>::Error(
            "Unsupported serialization version");
      }

      file_info.mode = deserializeOptional<mode_t>(data, offset);
      file_info.size = deserializeOptional<size_t>(data, offset);
      file_info.content = deserializeOptionalVector<uint8_t>(data, offset);
      file_info.uid = deserializeOptional<uid_t>(data, offset);
      file_info.gid = deserializeOptional<gid_t>(data, offset);
      file_info.access_time = deserializeOptional<time_t>(data, offset);
      file_info.modification_time = deserializeOptional<time_t>(data, offset);
      file_info.create_time = deserializeOptional<time_t>(data, offset);

      file_info.name = deserializeOptionalString(data, offset);
      file_info.path = deserializeOptionalString(data, offset);

      file_info.embedding = deserializeVector<float>(data, offset);
      file_info.pq_codes = deserializeVector<uint8_t>(data, offset);
      file_info.sq_codes = deserializeVector<uint8_t>(data, offset);

      file_info.embedding_updated = deserializeBool(data, offset);
      file_info.is_quantized = deserializeBool(data, offset);
      file_info.created = deserializeBool(data, offset);

      if (offset != data.size()) {
        spdlog::warn("Deserialization: {} bytes read, {} bytes total", offset,
                     data.size());
      }

    } catch (const std::exception &e) {
      return core::Result<schemas::FileInfo>::Error(e.what());
    }

    return core::Result<schemas::FileInfo>::Ok(file_info);
  }

private:
  static void serializeUint32(std::vector<uint8_t> &data, uint32_t value) {
    data.push_back(static_cast<uint8_t>(value & 0xFF));
    data.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    data.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
  }

  static uint32_t deserializeUint32(const std::vector<uint8_t> &data,
                                    size_t &offset) {
    if (offset + sizeof(uint32_t) > data.size()) {
      throw std::runtime_error("Insufficient data for uint32");
    }

    uint32_t value = data[offset] | (data[offset + 1] << 8) |
                     (data[offset + 2] << 16) | (data[offset + 3] << 24);
    offset += sizeof(uint32_t);
    return value;
  }

  template <typename T>
  static void serializeOptional(std::vector<uint8_t> &data,
                                const std::optional<T> &value) {
    bool has_value = value.has_value();
    data.push_back(static_cast<uint8_t>(has_value));

    if (has_value) {
      const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&value.value());
      for (size_t i = 0; i < sizeof(T); ++i) {
        data.push_back(bytes[i]);
      }
    }
  }

  static void serializeOptionalString(std::vector<uint8_t> &data,
                                      const std::optional<std::string> &str) {
    bool has_value = str.has_value();
    data.push_back(static_cast<uint8_t>(has_value));

    if (has_value) {
      serializeString(data, str.value());
    }
  }

  static void serializeString(std::vector<uint8_t> &data,
                              const std::string &str) {
    uint32_t length = static_cast<uint32_t>(str.length());
    serializeUint32(data, length);
    data.insert(data.end(), str.begin(), str.end());
  }

  template <typename T>
  static void
  serializeOptionalVector(std::vector<uint8_t> &data,
                          const std::optional<std::vector<T>> &vec) {
    bool has_value = vec.has_value();
    data.push_back(static_cast<uint8_t>(has_value));

    if (has_value) {
      serializeVector(data, vec.value());
    }
  }

  template <typename T>
  static void serializeVector(std::vector<uint8_t> &data,
                              const std::vector<T> &vec) {
    uint32_t size = static_cast<uint32_t>(vec.size());
    serializeUint32(data, size);

    if (!vec.empty()) {
      const uint8_t *vec_data = reinterpret_cast<const uint8_t *>(vec.data());
      data.insert(data.end(), vec_data, vec_data + vec.size() * sizeof(T));
    }
  }

  static void serializeBool(std::vector<uint8_t> &data, bool value) {
    data.push_back(static_cast<uint8_t>(value));
  }

  template <typename T>
  static std::optional<T> deserializeOptional(const std::vector<uint8_t> &data,
                                              size_t &offset) {
    if (offset >= data.size()) {
      throw std::runtime_error("Unexpected end of data");
    }

    bool has_value = static_cast<bool>(data[offset++]);
    if (!has_value) {
      return std::nullopt;
    }

    if (offset + sizeof(T) > data.size()) {
      throw std::runtime_error("Insufficient data for optional value");
    }

    T value;
    uint8_t *value_bytes = reinterpret_cast<uint8_t *>(&value);
    for (size_t i = 0; i < sizeof(T); ++i) {
      value_bytes[i] = data[offset + i];
    }
    offset += sizeof(T);

    return value;
  }

  static std::optional<std::string>
  deserializeOptionalString(const std::vector<uint8_t> &data, size_t &offset) {
    if (offset >= data.size()) {
      throw std::runtime_error("Unexpected end of data");
    }

    bool has_value = static_cast<bool>(data[offset++]);
    if (!has_value) {
      return std::nullopt;
    }

    return deserializeString(data, offset);
  }

  static std::string deserializeString(const std::vector<uint8_t> &data,
                                       size_t &offset) {
    uint32_t length = deserializeUint32(data, offset);

    if (offset + length > data.size()) {
      throw std::runtime_error("Insufficient data for string content");
    }

    std::string str(data.begin() + offset, data.begin() + offset + length);
    offset += length;

    return str;
  }

  template <typename T>
  static std::optional<std::vector<T>>
  deserializeOptionalVector(const std::vector<uint8_t> &data, size_t &offset) {
    if (offset >= data.size()) {
      throw std::runtime_error("Unexpected end of data");
    }

    bool has_value = static_cast<bool>(data[offset++]);
    if (!has_value) {
      return std::nullopt;
    }

    return deserializeVector<T>(data, offset);
  }

  template <typename T>
  static std::vector<T> deserializeVector(const std::vector<uint8_t> &data,
                                          size_t &offset) {
    uint32_t size = deserializeUint32(data, offset);

    if (size == 0) {
      return {};
    }

    if (offset + size * sizeof(T) > data.size()) {
      throw std::runtime_error("Insufficient data for vector content");
    }

    std::vector<T> vec(size);
    std::memcpy(vec.data(), data.data() + offset, size * sizeof(T));
    offset += size * sizeof(T);

    return vec;
  }

  static bool deserializeBool(const std::vector<uint8_t> &data,
                              size_t &offset) {
    if (offset >= data.size()) {
      throw std::runtime_error("Unexpected end of data");
    }

    return static_cast<bool>(data[offset++]);
  }
};

} // namespace owl::schemas

#endif // OWL_SCHEMAS_FILEINFO