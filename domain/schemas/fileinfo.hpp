#ifndef OWL_SCHEMAS_FILEINFO
#define OWL_SCHEMAS_FILEINFO

#include <infrastructure/result.hpp>
#include <optional>
#include <pipeline/pipeline.hpp>

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

    serializeOptional(data, file_info.mode);
    serializeOptional(data, file_info.size);
    serializeOptional(data, file_info.uid);
    serializeOptional(data, file_info.gid);
    serializeOptional(data, file_info.access_time);
    serializeOptional(data, file_info.modification_time);
    serializeOptional(data, file_info.create_time);

    serializeString(data, file_info.name);
    serializeString(data, file_info.path);

    serializeVector(data, file_info.content.value());
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
      file_info.mode = deserializeOptional<mode_t>(data, offset);
      file_info.size = deserializeOptional<size_t>(data, offset);
      file_info.uid = deserializeOptional<uid_t>(data, offset);
      file_info.gid = deserializeOptional<gid_t>(data, offset);
      file_info.access_time = deserializeOptional<time_t>(data, offset);
      file_info.modification_time = deserializeOptional<time_t>(data, offset);
      file_info.create_time = deserializeOptional<time_t>(data, offset);

      file_info.name = deserializeString(data, offset);
      file_info.path = deserializeString(data, offset);

      file_info.content = deserializeVector<uint8_t>(data, offset);
      file_info.embedding = deserializeVector<float>(data, offset);
      file_info.pq_codes = deserializeVector<uint8_t>(data, offset);
      file_info.sq_codes = deserializeVector<uint8_t>(data, offset);

      file_info.embedding_updated = deserializeBool(data, offset);
      file_info.is_quantized = deserializeBool(data, offset);
      file_info.created = deserializeBool(data, offset);

    } catch (const std::exception &e) {
      return core::Result<schemas::FileInfo>::Error(e.what());
    }

    return core::Result<schemas::FileInfo>::Ok(file_info);
  }

private:
  template <typename T>
  static void serializeOptional(std::vector<uint8_t> &data,
                                const std::optional<T> &value) {
    bool has_value = value.has_value();
    data.push_back(static_cast<uint8_t>(has_value));

    if (has_value) {
      const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&value.value());
      data.insert(data.end(), bytes, bytes + sizeof(T));
    }
  }

  static void serializeString(std::vector<uint8_t> &data,
                              const std::optional<std::string> &str) {
    bool has_value = str.has_value();
    data.push_back(static_cast<uint8_t>(has_value));

    if (has_value) {
      size_t length = str->length();
      const uint8_t *length_bytes = reinterpret_cast<const uint8_t *>(&length);
      data.insert(data.end(), length_bytes, length_bytes + sizeof(size_t));

      data.insert(data.end(), str->begin(), str->end());
    }
  }

  template <typename T>
  static void serializeVector(std::vector<uint8_t> &data,
                              const std::vector<T> &vec) {
    size_t size = vec.size();
    const uint8_t *size_bytes = reinterpret_cast<const uint8_t *>(&size);
    data.insert(data.end(), size_bytes, size_bytes + sizeof(size_t));

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
      throw std::runtime_error("Insufficient data for deserialization");
    }

    T value;
    std::memcpy(&value, data.data() + offset, sizeof(T));
    offset += sizeof(T);

    return value;
  }

  static std::optional<std::string>
  deserializeString(const std::vector<uint8_t> &data, size_t &offset) {
    if (offset >= data.size()) {
      throw std::runtime_error("Unexpected end of data");
    }

    bool has_value = static_cast<bool>(data[offset++]);
    if (!has_value) {
      return std::nullopt;
    }

    if (offset + sizeof(size_t) > data.size()) {
      throw std::runtime_error("Insufficient data for string length");
    }

    size_t length;
    std::memcpy(&length, data.data() + offset, sizeof(size_t));
    offset += sizeof(size_t);

    if (offset + length > data.size()) {
      throw std::runtime_error("Insufficient data for string content");
    }

    std::string str(data.begin() + offset, data.begin() + offset + length);
    offset += length;

    return str;
  }

  template <typename T>
  static std::vector<T> deserializeVector(const std::vector<uint8_t> &data,
                                          size_t &offset) {
    if (offset + sizeof(size_t) > data.size()) {
      throw std::runtime_error("Insufficient data for vector size");
    }

    size_t size;
    std::memcpy(&size, data.data() + offset, sizeof(size_t));
    offset += sizeof(size_t);

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
