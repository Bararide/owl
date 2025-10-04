#include "application.hpp"
#include "network/capnproto.hpp"
#include <csignal>
#include <iostream>
#include <list>
#include <pipeline/pipeline.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>
#include <iox2/client.hpp>

std::atomic<bool> g_shutdown_requested{false};

void signalHandler(int signal) {
  spdlog::info("Received signal: {}, initiating shutdown...", signal);
  g_shutdown_requested.store(true);
}

void setupSignalHandlers() {
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);
  std::signal(SIGQUIT, signalHandler);

  std::signal(SIGPIPE, SIG_IGN);
}

void printUsage(const char *program_name) {
  std::cout << "Usage: " << program_name << " [OPTIONS]" << std::endl;
  std::cout << "Options:" << std::endl;
  std::cout
      << "  --server ADDRESS    Set server address (default: 0.0.0.0:12345)"
      << std::endl;
  std::cout << "  --model PATH        Path to embedder model file" << std::endl;
  std::cout << "  --quantization      Enable quantization" << std::endl;
  std::cout << "  --help              Show this help message" << std::endl;
  std::cout << std::endl;
  std::cout << "Examples:" << std::endl;
  std::cout << "  " << program_name << " --server localhost:8080" << std::endl;
  std::cout << "  " << program_name << " --server 0.0.0.0:54321 --quantization"
            << std::endl;
}

void testServerConnection(owl::app::Application<> &app) {
  spdlog::info("Testing server connection...");

  auto status_result = app.getServerStatus();
  if (status_result.is_ok()) {
    spdlog::info("Server is running on: {}", app.getServerAddress());

    app.createClient();

  } else {
    spdlog::error("Server is not running: {}", status_result.error().what());
  }
}

int main(int argc, char *argv[]) {
  setupSignalHandlers();

  std::string server_address = "127.0.0.1:5346";
  std::string model_path = owl::kEmbedderModel;
  bool use_quantization = false;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--server" && i + 1 < argc) {
      server_address = argv[++i];
    } else if (arg == "--model" && i + 1 < argc) {
      model_path = argv[++i];
    } else if (arg == "--quantization") {
      use_quantization = true;
    } else if (arg == "--help") {
      printUsage(argv[0]);
      return 0;
    } else {
      spdlog::warn("Unknown argument: {}", arg);
      printUsage(argv[0]);
      return 1;
    }
  }

  spdlog::info("Starting VectorFS Application");
  spdlog::info("Configuration:");
  spdlog::info("  Server address: {}", server_address);
  spdlog::info("  Model path: {}", model_path);
  spdlog::info("  Quantization: {}", use_quantization ? "enabled" : "disabled");

  int exit_code = 0;
  int restart_count = 0;
  const int max_restarts = 5;

  while (!g_shutdown_requested && restart_count < max_restarts) {
    try {
      auto app = owl::app::Application<>(argc, argv);

      spdlog::info("Initializing application...");
      auto init_result = app.run(model_path, use_quantization);

      if (!init_result.is_ok()) {
        spdlog::error("Failed to initialize application: {}",
                      init_result.error().what());

        if (!g_shutdown_requested && restart_count < max_restarts) {
          restart_count++;
          spdlog::warn("Restarting application (attempt {}/{})", restart_count,
                       max_restarts);
          std::this_thread::sleep_for(std::chrono::seconds(2));
          continue;
        } else {
          return 1;
        }
      }

      testServerConnection(app);
      try {
        owl::capnp::TestClient client("127.0.0.1:5346");
        client.testConnection();
      } catch (const std::exception &e) {
        std::cerr << "Failed to create client: " << e.what() << std::endl;
        return 1;
      }

      spdlog::info(
          "Application initialized successfully. Starting main loop...");

      auto run_result = app.spin(g_shutdown_requested);

      if (run_result.is_ok()) {
        spdlog::info("Application finished successfully");
        exit_code = 0;
        break;
      } else {
        spdlog::error("Application finished with error: {}",
                      run_result.error().what());

        if (!g_shutdown_requested) {
          restart_count++;
          spdlog::warn("Restarting application (attempt {}/{})", restart_count,
                       max_restarts);

          std::this_thread::sleep_for(std::chrono::seconds(2));
          continue;
        } else {
          exit_code = 1;
          break;
        }
      }

    } catch (const std::exception &e) {
      spdlog::critical("Unhandled exception in main: {}", e.what());

      if (!g_shutdown_requested && restart_count < max_restarts) {
        restart_count++;
        spdlog::warn("Restarting after exception (attempt {}/{})",
                     restart_count, max_restarts);
        std::this_thread::sleep_for(std::chrono::seconds(2));
        continue;
      } else {
        exit_code = 1;
        break;
      }
    }
  }

  if (restart_count >= max_restarts) {
    spdlog::critical("Maximum restart attempts reached. Exiting.");
    exit_code = 1;
  }

  spdlog::info("Application shutdown complete");
  return exit_code;
}

// void testClientOperations() {
//   try {
//     spdlog::info("Testing client operations...");

//     auto client = owl::capnp::VectorFSClient("localhost:12345");
//     auto service = client.getService();

//     capnp::MallocMessageBuilder message;
//     auto createRequest =
//         message.initRoot<owl::capnp::VectorFSService::CreateFileRequest>();
//     auto request = createRequest.initRequest();
//     request.setPath("/test/file.txt");
//     request.setName("test_file");
//     request.setContent("Hello, World!");

//     spdlog::info("Client test completed");

//   } catch (const std::exception &e) {
//     spdlog::error("Client test failed: {}", e.what());
//   }
// }

// class DataValidator : public PipelineHandler<DataValidator, std::vector<int>>
// { public:
//   core::Result<std::vector<int>> handle(const std::vector<int> &data)
//   override {
//     std::cout << "DataValidator processing " << data.size() << " elements" <<
//     std::endl;

//     if (data.empty()) {
//       return core::Result<std::vector<int>>::Error("Empty data");
//     }

//     for (const auto &item : data) {
//       if (item < 0) {
//         return core::Result<std::vector<int>>::Error("Negative values
//         found");
//       }
//     }

//     std::cout << "DataValidator: validation passed" << std::endl;
//     return core::Result<std::vector<int>>::Ok(data);
//   }

//   void await() override {
//     std::cout << "DataValidator awaiting..." << std::endl;
//   }
// };

// class DataTransformer : public PipelineHandler<DataTransformer,
// std::vector<int>> { public:
//   core::Result<std::vector<int>> handle(const std::vector<int> &data)
//   override {
//     std::cout << "DataTransformer processing " << data.size() << " elements"
//     << std::endl;

//     std::vector<int> transformed = data;
//     for (auto &item : transformed) {
//       item += 10;
//     }

//     std::cout << "Transformed data: ";
//     for (const auto &item : transformed) {
//       std::cout << item << " ";
//     }
//     std::cout << std::endl;

//     return core::Result<std::vector<int>>::Ok(transformed);
//   }

//   void await() override {
//     std::cout << "DataTransformer awaiting..." << std::endl;
//   }
// };

// class DataLogger : public PipelineHandler<DataLogger, std::vector<int>> {
// public:
//   core::Result<std::vector<int>> handle(const std::vector<int> &data)
//   override {
//     std::cout << "DataLogger received " << data.size() << " elements" <<
//     std::endl;

//     std::cout << "Final data: ";
//     for (const auto &item : data) {
//       std::cout << item << " ";
//     }
//     std::cout << std::endl;

//     return core::Result<std::vector<int>>::Ok(data);
//   }

//   void await() override {
//     std::cout << "DataLogger awaiting..." << std::endl;
//     std::this_thread::sleep_for(std::chrono::milliseconds(2));
//   }
// };

// class StringProcessor : public PipelineHandler<StringProcessor,
// std::list<std::string>, std::list<std::string>> { public:
//   core::Result<std::list<std::string>> handle(const std::list<std::string>
//   &data) override {
//     std::cout << "StringProcessor processing " << data.size() << " strings"
//     << std::endl;

//     std::list<std::string> processed;
//     for (const auto &str : data) {
//       std::string upper = str;
//       for (auto &c : upper) {
//         c = std::toupper(c);
//       }
//       processed.push_back(upper);
//       std::cout << "Processed: " << str << " -> " << upper << std::endl;
//     }

//     return core::Result<std::list<std::string>>::Ok(processed);
//   }

//   void await() override {
//     std::cout << "StringProcessor awaiting..." << std::endl;
//   }
// };

// int main() {
//   spdlog::set_level(spdlog::level::info);

//   std::cout << "=== Processing integers ===" << std::endl;
//   core::pipeline::Pipeline pipeline;

//   auto validator = std::make_shared<DataValidator>();
//   auto transformer = std::make_shared<DataTransformer>();
//   auto logger = std::make_shared<DataLogger>();

//   pipeline.add_handler(validator);
//   pipeline.add_handler(transformer);
//   pipeline.add_handler(logger);

//   std::vector<int> data = {1, 2, 3, 4, 5};

//   std::cout << "Original data: ";
//   for (const auto &item : data) {
//     std::cout << item << " ";
//   }
//   std::cout << std::endl;

//   auto result = pipeline.process(data);

//   if (result.is_ok()) {
//     std::cout << "Processing successful!" << std::endl;
//     auto processed_data = result.value();
//     std::cout << "Final result size: " << processed_data.size() << "
//     elements" << std::endl;
//   } else {
//     std::cout << "Processing failed: " << result.error().what() << std::endl;
//   }

//   std::cout << pipeline.describe() << std::endl;

//   std::cout << "\n=== Processing strings ===" << std::endl;
//   core::pipeline::Pipeline string_pipeline;
//   auto string_processor = std::make_shared<StringProcessor>();
//   string_pipeline.add_handler(string_processor);

//   std::list<std::string> strings = {"hello", "world", "test"};

//   std::cout << "Original strings: ";
//   for (const auto &str : strings) {
//     std::cout << str << " ";
//   }
//   std::cout << std::endl;

//   auto string_result = string_pipeline.process(strings);

//   if (string_result.is_ok()) {
//     std::cout << "String processing successful!" << std::endl;
//     auto processed_strings = string_result.value();
//     std::cout << "Processed strings: ";
//     for (const auto &str : processed_strings) {
//       std::cout << str << " ";
//     }
//     std::cout << std::endl;
//   } else {
//     std::cout << "String processing failed: " << string_result.error().what()
//     << std::endl;
//   }

//   return 0;
// }

// std::atomic<bool> running{true};

// void signal_handler(int signal) {
//   spdlog::info("Received signal {}, shutting down...", signal);
//   running = false;
// }

// int main(int argc, char *argv[]) {
//   try {
//     std::signal(SIGINT, signal_handler);
//     std::signal(SIGTERM, signal_handler);
//     spdlog::set_level(spdlog::level::info);
//     spdlog::info("Starting VectorFS...");

//     const std::string fasttext_model_path =
//         "/home/bararide/code/models/crawl-300d-2M-subword/"
//         "crawl-300d-2M-subword.bin";

//     core::measure::Measure::start();
//     owl::instance::VFSInstance<owl::embedded::FastTextEmbedder,
//                                owl::compression::Compressor>::
//         initialize(std::move(fasttext_model_path));
//     core::measure::Measure::end();
//     core::measure::Measure::result<std::chrono::milliseconds>(
//         "VectorFS initialized with compression in {} ms");

//     core::measure::Measure::start();
//     auto &vectorfs =
//         owl::instance::VFSInstance<owl::embedded::FastTextEmbedder,
//                                    owl::compression::Compressor>::getInstance();
//     core::measure::Measure::end();
//     core::measure::Measure::result<std::chrono::milliseconds>(
//         "VectorFS with compression loaded in {} ms");

//     spdlog::info("Embedder: {}", vectorfs.get_embedder_info());

//     core::measure::Measure::start();
//     vectorfs.test_semantic_search();
//     core::measure::Measure::end();
//     core::measure::Measure::result<std::chrono::milliseconds>(
//         "Semantic search test completed in {} ms");

//     core::measure::Measure::start();
//     vectorfs.test_markov_model();
//     core::measure::Measure::end();
//     core::measure::Measure::result<std::chrono::milliseconds>(
//         "Markov search test completed in {} ms");

//     try {
//       core::measure::Measure::start();
//       auto embedding = vectorfs.get_embedding("test sentence");
//       core::measure::Measure::end();
//       auto embed_duration =
//           core::measure::Measure::duration<std::chrono::microseconds>();
//       spdlog::info(
//           "Embedding generated successfully, dimension: {}, time: {} μs",
//           embedding.size(), embed_duration.count());
//     } catch (const std::exception &e) {
//       spdlog::warn("Embedding test failed: {}", e.what());
//       core::measure::Measure::cancel();
//     }

//     auto &shm_manager = owl::shared::SharedMemoryManager::getInstance();
//     if (!shm_manager.initialize()) {
//       spdlog::warn("Failed to initialize shared memory in main process");
//     }

//     core::measure::Measure::start();

//     pid_t http_pid = fork();
//     if (http_pid == 0) {
//       core::measure::Measure::reset();

//       spdlog::info("Starting HTTP server in child process (PID: {})...",
//                    getpid());
//       try {
//         auto http_start = std::chrono::high_resolution_clock::now();
//         owl::network::VectorFSApi<owl::embedded::FastTextEmbedder>::init();
//         owl::network::VectorFSApi<owl::embedded::FastTextEmbedder>::run();
//         auto http_end = std::chrono::high_resolution_clock::now();
//         auto http_duration =
//         std::chrono::duration_cast<std::chrono::seconds>(
//             http_end - http_start);
//         spdlog::info("HTTP server ran for {} seconds",
//         http_duration.count());
//       } catch (const std::exception &e) {
//         spdlog::error("HTTP server error: {}", e.what());
//         exit(EXIT_FAILURE);
//       }
//       exit(0);
//     } else if (http_pid > 0) {
//       spdlog::info("Starting FUSE in parent process (PID: {})...", getpid());
//       std::this_thread::sleep_for(std::chrono::seconds(2));

//       auto fuse_start = std::chrono::high_resolution_clock::now();
//       int result = vectorfs.initialize_fuse(argc, argv);
//       auto fuse_end = std::chrono::high_resolution_clock::now();
//       auto fuse_duration = std::chrono::duration_cast<std::chrono::seconds>(
//           fuse_end - fuse_start);

//       core::measure::Measure::end();
//       auto total_duration =
//           core::measure::Measure::duration<std::chrono::seconds>();

//       spdlog::info("FUSE exited with code: {}, ran for {} seconds", result,
//                    fuse_duration.count());
//       spdlog::info("Total application runtime: {} seconds",
//                    total_duration.count());

//       spdlog::info("Stopping HTTP server...");
//       if (kill(http_pid, SIGTERM) == 0) {
//         int status;
//         waitpid(http_pid, &status, 0);
//         spdlog::info("HTTP server terminated with status: {}", status);
//       } else {
//         spdlog::warn("Failed to terminate HTTP server gracefully");
//       }

//       owl::instance::VFSInstance<owl::embedded::FastTextEmbedder>::shutdown();
//       spdlog::info("VectorFS shutdown complete");
//       return result;
//     } else {
//       spdlog::error("Failed to fork process for HTTP server");
//       return EXIT_FAILURE;
//     }
//   } catch (const std::exception &e) {
//     spdlog::error("Fatal error: {}", e.what());
//     core::measure::Measure::cancel();
//     try {
//       owl::instance::VFSInstance<owl::embedded::FastTextEmbedder>::shutdown();
//     } catch (...) {
//     }
//     return EXIT_FAILURE;
//   }
// }