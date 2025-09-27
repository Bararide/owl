#ifndef VECTORFS_COMPRESSION_HPP
#define VECTORFS_COMPRESSION_HPP

#include "compressor_base.hpp"
#include <lz4.h>
#include <lz4hc.h>
#include <spdlog/spdlog.h>

namespace owl::compression {

static constexpr size_t kBlockSize = 64 * 1024;
static constexpr int kCompressionLevel = 9;

class Compressor : public CompressionBase<Compressor> {
public:
  auto compress_impl(const std::vector<uint8_t> data)
      -> core::Result<std::vector<uint8_t>> {
    try {
      if (data.empty()) {
        return core::Result<std::vector<uint8_t>>::Ok({});
      }

      std::vector<uint8_t> compressed_data;

      const uint32_t magic = 0x4C5A3432;
      const uint16_t version = 0x0100; 

      size_t total_blocks = (data.size() + kBlockSize - 1) / kBlockSize;
      std::vector<uint32_t> kBlockSizes;
      std::vector<uint32_t> compressed_kBlockSizes;

      size_t header_size = sizeof(magic) + sizeof(version) + sizeof(uint32_t) +
                           total_blocks * (sizeof(uint32_t) * 2);
      compressed_data.reserve(header_size +
                              data.size() / 2);

      std::vector<std::vector<uint8_t>> compressed_blocks;
      compressed_blocks.reserve(total_blocks);

      for (size_t i = 0; i < total_blocks; ++i) {
        size_t block_start = i * kBlockSize;
        size_t kBlockSize = std::min(kBlockSize, data.size() - block_start);

        std::vector<uint8_t> block(data.begin() + block_start,
                                   data.begin() + block_start + kBlockSize);

        auto compressed_block = compress_block(block);
        compressed_blocks.push_back(compressed_block);

        kBlockSizes.push_back(static_cast<uint32_t>(kBlockSize));
        compressed_kBlockSizes.push_back(
            static_cast<uint32_t>(compressed_block.size()));
      }

      compressed_data.resize(header_size);
      size_t offset = 0;

      std::memcpy(compressed_data.data() + offset, &magic, sizeof(magic));
      offset += sizeof(magic);

      std::memcpy(compressed_data.data() + offset, &version, sizeof(version));
      offset += sizeof(version);

      uint32_t block_count = static_cast<uint32_t>(total_blocks);
      std::memcpy(compressed_data.data() + offset, &block_count,
                  sizeof(block_count));
      offset += sizeof(block_count);

      for (auto size : kBlockSizes) {
        std::memcpy(compressed_data.data() + offset, &size, sizeof(size));
        offset += sizeof(size);
      }

      for (auto size : compressed_kBlockSizes) {
        std::memcpy(compressed_data.data() + offset, &size, sizeof(size));
        offset += sizeof(size);
      }

      for (const auto &block : compressed_blocks) {
        compressed_data.insert(compressed_data.end(), block.begin(),
                               block.end());
      }

      spdlog::info("Compressed {} bytes to {} bytes (ratio: {:.2f}%)",
                    data.size(), compressed_data.size(),
                    (compressed_data.size() * 100.0) / data.size());

      return core::Result<std::vector<uint8_t>>::Ok(compressed_data);

    } catch (const std::exception &e) {
      spdlog::error("Compression failed: {}", e.what());
      return core::Result<std::vector<uint8_t>>::Error("Compression failed");
    }
  }

  auto decompress_impl(const std::vector<uint8_t> &data)
      -> core::Result<std::vector<uint8_t>> {
    try {
      if (data.empty()) {
        return core::Result<std::vector<uint8_t>>::Ok({});
      }

      if (data.size() < sizeof(uint32_t) * 3) {
        return core::Result<std::vector<uint8_t>>::Error(
            "Invalid compressed data format");
      }

      size_t offset = 0;
      uint32_t magic;
      std::memcpy(&magic, data.data() + offset, sizeof(magic));
      offset += sizeof(magic);

      if (magic != 0x4C5A3432) {
        return core::Result<std::vector<uint8_t>>::Error(
            "Invalid magic number");
      }

      offset += sizeof(uint16_t);

      uint32_t block_count;
      std::memcpy(&block_count, data.data() + offset, sizeof(block_count));
      offset += sizeof(block_count);

      if (block_count == 0) {
        return core::Result<std::vector<uint8_t>>::Ok({});
      }

      std::vector<uint32_t> kBlockSizes(block_count);
      for (uint32_t i = 0; i < block_count; ++i) {
        std::memcpy(&kBlockSizes[i], data.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
      }

      std::vector<uint32_t> compressed_kBlockSizes(block_count);
      for (uint32_t i = 0; i < block_count; ++i) {
        std::memcpy(&compressed_kBlockSizes[i], data.data() + offset,
                    sizeof(uint32_t));
        offset += sizeof(uint32_t);
      }

      std::vector<uint8_t> decompressed_data;
      size_t total_original_size = 0;
      for (auto size : kBlockSizes) {
        total_original_size += size;
      }
      decompressed_data.reserve(total_original_size);

      for (uint32_t i = 0; i < block_count; ++i) {
        if (offset + compressed_kBlockSizes[i] > data.size()) {
          return core::Result<std::vector<uint8_t>>::Error(
              "Compressed data corrupted");
        }

        std::vector<uint8_t> compressed_block(data.begin() + offset,
                                              data.begin() + offset +
                                                  compressed_kBlockSizes[i]);
        offset += compressed_kBlockSizes[i];

        auto decompressed_block =
            decompress_block(compressed_block, kBlockSizes[i]);
        decompressed_data.insert(decompressed_data.end(),
                                 decompressed_block.begin(),
                                 decompressed_block.end());
      }

      spdlog::debug("Decompressed {} bytes to {} bytes", data.size(),
                    decompressed_data.size());

      return core::Result<std::vector<uint8_t>>::Ok(decompressed_data);

    } catch (const std::exception &e) {
      spdlog::error("Decompression failed: {}", e.what());
      return core::Result<std::vector<uint8_t>>::Error("Decompression failed");
    }
  }

private:
  std::vector<uint8_t>
  decompress_block(const std::vector<uint8_t> &compressed_block,
                               size_t original_size) {
    std::vector<uint8_t> decompressed(original_size);

    int decompressed_size = LZ4_decompress_safe(
        reinterpret_cast<const char *>(compressed_block.data()),
        reinterpret_cast<char *>(decompressed.data()),
        static_cast<int>(compressed_block.size()),
        static_cast<int>(original_size));

    if (decompressed_size < 0 ||
        static_cast<size_t>(decompressed_size) != original_size) {
      throw std::runtime_error("LZ4 decompression failed");
    }

    return decompressed;
  }

  std::vector<uint8_t>
  compress_block(const std::vector<uint8_t> &block) {
    int max_compressed_size = LZ4_compressBound(static_cast<int>(block.size()));
    if (max_compressed_size <= 0) {
      throw std::runtime_error("LZ4 compression bound calculation failed");
    }

    std::vector<uint8_t> compressed(max_compressed_size);

    int compressed_size = LZ4_compress_HC(
        reinterpret_cast<const char *>(block.data()),
        reinterpret_cast<char *>(compressed.data()),
        static_cast<int>(block.size()), max_compressed_size, kCompressionLevel);

    if (compressed_size <= 0) {
      throw std::runtime_error("LZ4 compression failed");
    }

    compressed.resize(compressed_size);
    return compressed;
  }
};

template <> struct CompressionTraits<Compressor> {
  using CompressionType = CompressionBase<Compressor>;
  static constexpr const char *Name = "LZ4HC";
};

} // namespace owl::compression

#endif // VECTORFS_COMPRESSION_HPP