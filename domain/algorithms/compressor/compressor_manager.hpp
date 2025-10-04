#ifndef VECTORFS_COMRESSION_MANAGER
#define VECTORFS_COMRESSION_MANAGER

#include "compressor.hpp"

namespace owl {

using namespace compression;

using CompressorVariant = std::variant<Compressor>;

template <typename T> class CompressorManager {
public:
  CompressorManager() = default;

  core::Result<T &> compressor() {
    if constexpr (std::is_same_v<T, Compressor>) {
      if (std::holds_alternative<Compressor>(compressor_)) {
        return core::Result<T &>::Ok(std::get<Compressor>(compressor_));
      }
    }
    return core::Result<T &>::Error("Compressor not found or wrong type");
  }

  core::Result<bool> set() {
    if constexpr (std::is_same_v<T, Compressor>) {
      compressor_.emplace<Compressor>();
      return core::Result<bool>::Ok(true);
    }

    return core::Result<bool>::Error("Unsupported compression type");
  }

  CompressorManager(const CompressorManager &) = delete;
  CompressorManager(CompressorManager &&) = delete;
  CompressorManager &operator=(const CompressorManager &) = delete;
  CompressorManager &operator=(CompressorManager &&) = delete;

private:
  CompressorVariant compressor_;
};
} // namespace owl

#endif // VECTORFS_COMRESSION_MANAGER