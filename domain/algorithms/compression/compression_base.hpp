#ifndef VECTORFS_COMPRESSION_BASE_HPP
#define VECTORFS_COMPRESSION_BASE_HPP

#include <infrastructure/result.hpp>

namespace owl::compression {

template <typename Derived> class CompressionBase;

template <typename T> struct CompressionTraits {
  using CompressionType = CompressionBase<T>;
  static constexpr const char *Name = "Unknown";
};

template <typename Derived> class CompressionBase {
public:
  auto compress(const std::vector<uint8_t> data) -> core::Result<std::vector<uint8_t>> {
    return static_cast<Derived *>(this)->compress_impl(data);
  }

  auto decompress(const std::vector<uint8_t> &data) -> core::Result<std::vector<uint8_t>> {
    return static_cast<Derived *>(this)->decompress_impl(data);
  }
};
} // namespace owl::compression

#endif // VECTORFS_COMPRESSION_BASE_HPP