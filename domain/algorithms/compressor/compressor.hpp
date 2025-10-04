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
  auto compress(const std::vector<uint8_t> &data)
      -> core::Result<std::vector<uint8_t>> {
    return core::Result<std::vector<uint8_t>>::Ok(data)
        .and_then([this](const std::vector<uint8_t> &input_data)
                      -> core::Result<std::vector<uint8_t>> {
          if (input_data.empty()) {
            return core::Result<std::vector<uint8_t>>::Ok({});
          }

          const uint32_t magic = 0x4C5A3432;
          const uint16_t version = 0x0100;

          size_t total_blocks =
              (input_data.size() + kBlockSize - 1) / kBlockSize;
          std::vector<uint32_t> block_sizes;
          std::vector<uint32_t> compressed_block_sizes;

          size_t header_size = sizeof(magic) + sizeof(version) +
                               sizeof(uint32_t) +
                               total_blocks * (sizeof(uint32_t) * 2);

          std::vector<std::vector<uint8_t>> compressed_blocks;
          compressed_blocks.reserve(total_blocks);

          for (size_t i = 0; i < total_blocks; ++i) {
            size_t block_start = i * kBlockSize;
            size_t current_block_size =
                std::min(kBlockSize, input_data.size() - block_start);

            std::vector<uint8_t> block(input_data.begin() + block_start,
                                       input_data.begin() + block_start +
                                           current_block_size);

            auto compressed_block_result = compress_block(block);
            if (!compressed_block_result.is_ok()) {
              return core::Result<std::vector<uint8_t>>::Error(
                  "Block compression failed");
            }

            compressed_blocks.push_back(compressed_block_result.unwrap());
            block_sizes.push_back(static_cast<uint32_t>(current_block_size));
            compressed_block_sizes.push_back(
                static_cast<uint32_t>(compressed_blocks.back().size()));
          }

          std::vector<uint8_t> compressed_data;
          compressed_data.reserve(header_size + input_data.size() / 2);
          compressed_data.resize(header_size);

          size_t offset = 0;

          std::memcpy(compressed_data.data() + offset, &magic, sizeof(magic));
          offset += sizeof(magic);

          std::memcpy(compressed_data.data() + offset, &version,
                      sizeof(version));
          offset += sizeof(version);

          uint32_t block_count = static_cast<uint32_t>(total_blocks);
          std::memcpy(compressed_data.data() + offset, &block_count,
                      sizeof(block_count));
          offset += sizeof(block_count);

          for (auto size : block_sizes) {
            std::memcpy(compressed_data.data() + offset, &size, sizeof(size));
            offset += sizeof(size);
          }

          for (auto size : compressed_block_sizes) {
            std::memcpy(compressed_data.data() + offset, &size, sizeof(size));
            offset += sizeof(size);
          }

          for (const auto &block : compressed_blocks) {
            compressed_data.insert(compressed_data.end(), block.begin(),
                                   block.end());
          }

          spdlog::info("Compressed {} bytes to {} bytes (ratio: {:.2f}%)",
                       input_data.size(), compressed_data.size(),
                       (compressed_data.size() * 100.0) / input_data.size());

          return core::Result<std::vector<uint8_t>>::Ok(compressed_data);
        })
        .match(
            [](std::vector<uint8_t> result)
                -> core::Result<std::vector<uint8_t>> {
              return core::Result<std::vector<uint8_t>>::Ok(std::move(result));
            },
            [](const auto &error) -> core::Result<std::vector<uint8_t>> {
              spdlog::error("Compression failed: {}", error.what());
              return core::Result<std::vector<uint8_t>>::Error(
                  "Compression failed");
            });
  }

  auto decompress(const std::vector<uint8_t> &data)
      -> core::Result<std::vector<uint8_t>> {
    return core::Result<std::vector<uint8_t>>::Ok(data)
        .and_then([](const std::vector<uint8_t> &input_data)
                      -> core::Result<std::vector<uint8_t>> {
          if (input_data.empty()) {
            return core::Result<std::vector<uint8_t>>::Ok({});
          }

          if (input_data.size() < sizeof(uint32_t) * 3) {
            return core::Result<std::vector<uint8_t>>::Error(
                "Invalid compressed data format");
          }

          return core::Result<std::vector<uint8_t>>::Ok(input_data);
        })
        .and_then([this](const std::vector<uint8_t> &input_data)
                      -> core::Result<std::vector<uint8_t>> {
          size_t offset = 0;
          uint32_t magic;
          std::memcpy(&magic, input_data.data() + offset, sizeof(magic));
          offset += sizeof(magic);

          if (magic != 0x4C5A3432) {
            return core::Result<std::vector<uint8_t>>::Error(
                "Invalid magic number");
          }

          offset += sizeof(uint16_t);

          uint32_t block_count;
          std::memcpy(&block_count, input_data.data() + offset,
                      sizeof(block_count));
          offset += sizeof(block_count);

          if (block_count == 0) {
            return core::Result<std::vector<uint8_t>>::Ok({});
          }

          std::vector<uint32_t> block_sizes(block_count);
          for (uint32_t i = 0; i < block_count; ++i) {
            std::memcpy(&block_sizes[i], input_data.data() + offset,
                        sizeof(uint32_t));
            offset += sizeof(uint32_t);
          }

          std::vector<uint32_t> compressed_block_sizes(block_count);
          for (uint32_t i = 0; i < block_count; ++i) {
            std::memcpy(&compressed_block_sizes[i], input_data.data() + offset,
                        sizeof(uint32_t));
            offset += sizeof(uint32_t);
          }

          std::vector<uint8_t> decompressed_data;
          size_t total_original_size = 0;
          for (auto size : block_sizes) {
            total_original_size += size;
          }
          decompressed_data.reserve(total_original_size);

          for (uint32_t i = 0; i < block_count; ++i) {
            if (offset + compressed_block_sizes[i] > input_data.size()) {
              return core::Result<std::vector<uint8_t>>::Error(
                  "Compressed data corrupted");
            }

            std::vector<uint8_t> compressed_block(
                input_data.begin() + offset,
                input_data.begin() + offset + compressed_block_sizes[i]);
            offset += compressed_block_sizes[i];

            auto decompressed_block_result =
                decompress_block(compressed_block, block_sizes[i]);
            if (!decompressed_block_result.is_ok()) {
              return core::Result<std::vector<uint8_t>>::Error(
                  "Block decompression failed");
            }

            auto decompressed_block = decompressed_block_result.unwrap();
            decompressed_data.insert(decompressed_data.end(),
                                     decompressed_block.begin(),
                                     decompressed_block.end());
          }

          spdlog::debug("Decompressed {} bytes to {} bytes", input_data.size(),
                        decompressed_data.size());

          return core::Result<std::vector<uint8_t>>::Ok(decompressed_data);
        })
        .match(
            [](std::vector<uint8_t> result)
                -> core::Result<std::vector<uint8_t>> {
              return core::Result<std::vector<uint8_t>>::Ok(std::move(result));
            },
            [](const auto &error) -> core::Result<std::vector<uint8_t>> {
              spdlog::error("Decompression failed: {}", error.what());
              return core::Result<std::vector<uint8_t>>::Error(
                  "Decompression failed");
            });
  }

  auto handle(schemas::FileInfo &file) -> core::Result<schemas::FileInfo> {
    return core::Result<schemas::FileInfo>::Ok(file).and_then(
        [this](schemas::FileInfo file_info) -> core::Result<schemas::FileInfo> {
          if (!file_info.content.has_value()) {
            spdlog::warn("File has no content, skipping compression");
            return core::Result<schemas::FileInfo>::Ok(file_info);
          }

          auto &content = file_info.content.value();

          spdlog::info("Original content size: {}", content.size());

          if (content.size() <= kBlockSize) {
            spdlog::info("File too small, skipping compression");
            return core::Result<schemas::FileInfo>::Ok(file_info);
          }

          return compress(content).and_then(
              [&file_info](std::vector<uint8_t> compressed_data)
                  -> core::Result<schemas::FileInfo> {
                file_info.content = std::move(compressed_data);
                file_info.size = file_info.content.value().size();

                spdlog::info("Compressed file to {} bytes",
                             file_info.size.value());

                return core::Result<schemas::FileInfo>::Ok(file_info);
              });
        });
  }

  void await() { spdlog::debug("await method in compressor"); }

private:
  auto
  decompress_block(const std::vector<uint8_t> &compressed_block,
                   size_t original_size) -> core::Result<std::vector<uint8_t>> {
    return core::Result<std::vector<uint8_t>>::Ok(compressed_block)
        .map([original_size](const std::vector<uint8_t> &block) {
          std::vector<uint8_t> decompressed(original_size);

          int decompressed_size = LZ4_decompress_safe(
              reinterpret_cast<const char *>(block.data()),
              reinterpret_cast<char *>(decompressed.data()),
              static_cast<int>(block.size()), static_cast<int>(original_size));

          if (decompressed_size < 0 ||
              static_cast<size_t>(decompressed_size) != original_size) {
            throw std::runtime_error("LZ4 decompression failed");
          }

          return decompressed;
        })
        .match(
            [](std::vector<uint8_t> result)
                -> core::Result<std::vector<uint8_t>> {
              return core::Result<std::vector<uint8_t>>::Ok(std::move(result));
            },
            [](const auto &error) -> core::Result<std::vector<uint8_t>> {
              return core::Result<std::vector<uint8_t>>::Error(
                  "Block decompression failed");
            });
  }

  auto compress_block(const std::vector<uint8_t> &block)
      -> core::Result<std::vector<uint8_t>> {
    return core::Result<std::vector<uint8_t>>::Ok(block)
        .map([](const std::vector<uint8_t> &input_block) {
          int max_compressed_size =
              LZ4_compressBound(static_cast<int>(input_block.size()));
          if (max_compressed_size <= 0) {
            throw std::runtime_error(
                "LZ4 compression bound calculation failed");
          }

          std::vector<uint8_t> compressed(max_compressed_size);

          int compressed_size = LZ4_compress_HC(
              reinterpret_cast<const char *>(input_block.data()),
              reinterpret_cast<char *>(compressed.data()),
              static_cast<int>(input_block.size()), max_compressed_size,
              kCompressionLevel);

          if (compressed_size <= 0) {
            throw std::runtime_error("LZ4 compression failed");
          }

          compressed.resize(compressed_size);
          return compressed;
        })
        .match(
            [](std::vector<uint8_t> result)
                -> core::Result<std::vector<uint8_t>> {
              return core::Result<std::vector<uint8_t>>::Ok(std::move(result));
            },
            [](const auto &error) -> core::Result<std::vector<uint8_t>> {
              return core::Result<std::vector<uint8_t>>::Error(
                  "Block compression failed");
            });
  }
};

template <> struct CompressionTraits<Compressor> {
  using CompressionType = CompressionBase<Compressor>;
  static constexpr const char *Name = "LZ4HC";
};

} // namespace owl::compression

#endif // VECTORFS_COMPRESSION_HPP