#ifndef VECTORFS_COMPRESSION_BASE_HPP
#define VECTORFS_COMPRESSION_BASE_HPP

#include "schemas/fileinfo.hpp"

namespace owl::compression {

template <typename Derived> class CompressionBase;

template <typename T> struct CompressionTraits {
  using CompressionType = CompressionBase<T>;
  static constexpr const char *Name = "Unknown";
};

template <typename Derived>
class CompressionBase
    : public core::pipeline::PipelineHandler<Derived, schemas::FileInfo> {
public:
  auto compress(const std::vector<uint8_t> data)
      -> core::Result<std::vector<uint8_t>> {
    return static_cast<Derived *>(this)->compress(data);
  }

  auto decompress(const std::vector<uint8_t> &data)
      -> core::Result<std::vector<uint8_t>> {
    return static_cast<Derived *>(this)->decompress(data);
  }

  void await() {
    if constexpr (requires(Derived d) { d.await(); }) {
      static_cast<Derived *>(this)->await();
    }
  }

  auto
  handle(schemas::FileInfo &file) -> core::Result<schemas::FileInfo> {
    return static_cast<Derived *>(this)->handle(file);
  }
};
} // namespace owl::compression

#endif // VECTORFS_COMPRESSION_BASE_HPP