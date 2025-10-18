#include "vectorfs.hpp"

namespace owl::vectorfs {
std::string VectorFS::generate_markov_test_result() {
  std::stringstream ss;
  ss << "=== Markov Chain Test Results ===\n\n";

  ss << "Semantic Graph Analysis:\n";
  ss << "Nodes: " << semantic_graph->get_node_count() << "\n";
  ss << "Edges: " << semantic_graph->get_edge_count() << "\n";

  auto hubs = semantic_graph->get_semantic_hubs(5);
  if (!hubs.empty()) {
    ss << "Top semantic hubs:\n";
    for (const auto &hub : hubs) {
      ss << "  ⭐ " << hub << "\n";
    }
  }
  ss << "\n";

  ss << "Hidden Markov Model Analysis:\n";
  ss << "Trained states: " << hmm_model->get_state_count() << "\n";
  ss << "Observation sequences: " << hmm_model->get_sequence_count() << "\n";

  auto predictions = predict_next_files();
  if (!predictions.empty()) {
    ss << "Current predictions:\n";
    for (const auto &pred : predictions) {
      ss << "  ↗ " << pred << "\n";
    }
  }
  ss << "\n";

  ss << "Recommendation Test:\n";
  int test_count = 0;
  for (const auto &[path, _] : virtual_files) {
    if (test_count >= 3)
      break;

    auto recs = semantic_graph->get_recommendations(path, 3);
    if (!recs.empty()) {
      ss << "For " << path << ":\n";
      for (const auto &rec : recs) {
        ss << "  → " << rec << "\n";
      }
      ss << "\n";
      test_count++;
    }
  }

  ss << "Access Patterns:\n";
  ss << "Recent queries: " << recent_queries.size() << "\n";
  if (!recent_queries.empty()) {
    ss << "Last 5 queries:\n";
    for (int i = std::max(0, (int)recent_queries.size() - 5);
         i < recent_queries.size(); i++) {
      ss << "  " << recent_queries[i] << "\n";
    }
  }

  return ss.str();
}

void VectorFS::test_semantic_search() {
  if (std::is_same_v<decltype(embedder_.embedder()), FastTextEmbedder> ||
      !faiss_index) {
    spdlog::error("Embedder or index not initialized for testing");
    return;
  }

  spdlog::info("=== Enhanced Semantic Search Test with Code Examples ===");

  std::vector<std::pair<std::string, std::string>> test_files = {
      // Python функции и основы
      {"/python/functions_basic.py",
       "def greet(name):\n    return f\"Hello, {name}!\"\n\n# Функции в "
       "Python объявляются через def\n# Могут возвращать значения через "
       "return\n# Поддерживают аргументы по умолчанию\n\ndef "
       "calculate_area(radius, pi=3.14159):\n    return pi * radius * "
       "radius"},

      {"/python/functions_advanced.py",
       "# Лямбда-функции в Python\nsquare = lambda x: x * x\n\n# Функции "
       "высшего порядка\ndef apply_func(func, value):\n    return "
       "func(value)\n\n# Декораторы функций\ndef debug_decorator(func):\n    "
       "def wrapper(*args, **kwargs):\n        print(f\"Calling "
       "{func.__name__}\")\n        return func(*args, **kwargs)\n    return "
       "wrapper"},

      {"/python/oop_basics.py",
       "class Car:\n    def __init__(self, brand, model):\n        "
       "self.brand = brand\n        self.model = model\n    \n    def "
       "display_info(self):\n        return f\"{self.brand} "
       "{self.model}\"\n\n# Наследование в Python\nclass ElectricCar(Car):\n "
       "   def __init__(self, brand, model, battery_size):\n        "
       "super().__init__(brand, model)\n        self.battery_size = "
       "battery_size"},

      // C++ функции и основы
      {"/cpp/functions_basic.cpp",
       "// Функции в C++ объявляются с указанием типа возвращаемого "
       "значения\nint add(int a, int b) {\n    return a + b;\n}\n\n// "
       "Функции могут принимать параметры по ссылке\nvoid swap(int &a, int "
       "&b) {\n    int temp = a;\n    a = b;\n    b = temp;\n}"},

      {"/cpp/functions_advanced.cpp",
       "// Шаблонные функции в C++\ntemplate<typename T>\nT max(T a, T b) "
       "{\n    return (a > b) ? a : b;\n}\n\n// Рекурсивные функции\nint "
       "factorial(int n) {\n    if (n <= 1) return 1;\n    return n * "
       "factorial(n - 1);\n}\n\n// Указатели на функции\nint operate(int a, "
       "int b, int (*func)(int, int)) {\n    return func(a, b);\n}"},

      {"/cpp/oop_basics.cpp",
       "class Car {\nprivate:\n    std::string brand;\n    std::string "
       "model;\npublic:\n    Car(std::string b, std::string m) : brand(b), "
       "model(m) {}\n    \n    std::string displayInfo() {\n        return "
       "brand + \" \" + model;\n    }\n};\n\n// Наследование в C++\nclass "
       "ElectricCar : public Car {\nprivate:\n    int "
       "battery_size;\npublic:\n    ElectricCar(std::string b, std::string "
       "m, int bs) \n        : Car(b, m), battery_size(bs) {}\n};"},

      // Сравнительные примеры Python vs C++
      {"/comparison/functions_python_vs_cpp.txt",
       "Сравнение функций в Python и C++:\n\nPython: def "
       "function_name(args):\nC++: return_type function_name(parameters) "
       "{}\n\nPython: динамическая типизация\nC++: статическая "
       "типизация\n\nPython: поддержка lambda\nC++: поддержка lambda с "
       "C++11\n\nPython: декораторы функций\nC++: шаблоны и функторы"},

      {"/comparison/oop_python_vs_cpp.txt",
       "Сравнение ООП в Python и C++:\n\nНаследование:\nPython: class "
       "Child(Parent):\nC++: class Child : public "
       "Parent\n\nИнкапсуляция:\nPython: соглашения (_protected, "
       "__private)\nC++: модификаторы private, protected, "
       "public\n\nПолиморфизм:\nPython: duck typing\nC++: виртуальные "
       "функции\n\nКонструкторы:\nPython: def __init__(self):\nC++: "
       "ClassName() : initialization_list {}"},

      // Алгоритмы на обоих языках
      {"/algorithms/sort_python.py",
       "def bubble_sort(arr):\n    n = len(arr)\n    for i in range(n):\n    "
       "    for j in range(0, n-i-1):\n            if arr[j] > arr[j+1]:\n   "
       "             arr[j], arr[j+1] = arr[j+1], arr[j]\n\n# Быстрая "
       "сортировка на Python\ndef quick_sort(arr):\n    if len(arr) <= 1:\n  "
       "      return arr\n    pivot = arr[len(arr)//2]\n    left = [x for x "
       "in arr if x < pivot]\n    middle = [x for x in arr if x == pivot]\n  "
       "  right = [x for x in arr if x > pivot]\n    return quick_sort(left) "
       "+ middle + quick_sort(right)"},

      {"/algorithms/sort_cpp.cpp",
       "// Пузырьковая сортировка на C++\nvoid bubbleSort(int arr[], int n) "
       "{\n    for (int i = 0; i < n-1; i++) {\n        for (int j = 0; j < "
       "n-i-1; j++) {\n            if (arr[j] > arr[j+1]) {\n                "
       "std::swap(arr[j], arr[j+1]);\n            }\n        }\n    "
       "}\n}\n\n// Быстрая сортировка на C++\nvoid quickSort(int arr[], int "
       "low, int high) {\n    if (low < high) {\n        int pi = "
       "partition(arr, low, high);\n        quickSort(arr, low, pi - 1);\n   "
       "     quickSort(arr, pi + 1, high);\n    }\n}"},

      // Структуры данных
      {"/data_structures/linked_list_python.py",
       "class Node:\n    def __init__(self, data):\n        self.data = "
       "data\n        self.next = None\n\nclass LinkedList:\n    def "
       "__init__(self):\n        self.head = None\n    \n    def "
       "append(self, data):\n        new_node = Node(data)\n        if not "
       "self.head:\n            self.head = new_node\n            return\n   "
       "     last = self.head\n        while last.next:\n            last = "
       "last.next\n        last.next = new_node"},

      {"/data_structures/linked_list_cpp.cpp",
       "struct Node {\n    int data;\n    Node* next;\n    Node(int d) : "
       "data(d), next(nullptr) {}\n};\n\nclass LinkedList {\nprivate:\n    "
       "Node* head;\npublic:\n    LinkedList() : head(nullptr) {}\n    \n    "
       "void append(int data) {\n        Node* new_node = new Node(data);\n  "
       "      if (!head) {\n            head = new_node;\n            "
       "return;\n        }\n        Node* last = head;\n        while "
       "(last->next) {\n            last = last->next;\n        }\n        "
       "last->next = new_node;\n    }\n};"},

      // Многопоточность
      {"/concurrency/threads_python.py",
       "import threading\nimport time\n\ndef print_numbers():\n    for i in "
       "range(5):\n        time.sleep(1)\n        print(i)\n\n# Создание "
       "потоков в Python\nthread1 = "
       "threading.Thread(target=print_numbers)\nthread2 = "
       "threading.Thread(target=print_numbers)\nthread1.start()\nthread2."
       "start()\nthread1.join()\nthread2.join()"},

      {"/concurrency/threads_cpp.cpp",
       "#include <thread>\n#include <iostream>\n#include <chrono>\n\nvoid "
       "printNumbers() {\n    for (int i = 0; i < 5; ++i) {\n        "
       "std::this_thread::sleep_for(std::chrono::seconds(1));\n        "
       "std::cout << i << std::endl;\n    }\n}\n\n// Создание потоков в "
       "C++\nint main() {\n    std::thread thread1(printNumbers);\n    "
       "std::thread thread2(printNumbers);\n    thread1.join();\n    "
       "thread2.join();\n    return 0;\n}"},

      // Веб и сети
      {"/web/http_server_python.py",
       "from flask import Flask\napp = "
       "Flask(__name__)\n\n@app.route('/')\ndef hello():\n    return \"Hello "
       "World!\"\n\nif __name__ == '__main__':\n    app.run()\n\n# Простой "
       "HTTP сервер на Python Flask"},

      {"/web/http_server_cpp.cpp",
       "#include <cpprest/http_listener.h>\n#include "
       "<cpprest/json.h>\n\nusing namespace web;\nusing namespace "
       "http;\nusing namespace http::experimental::listener;\n\nvoid "
       "handle_get(http_request request) {\n    "
       "request.reply(status_codes::OK, \"Hello World!\");\n}\n\n// Простой "
       "HTTP сервер на C++ REST SDK"},

      // Машинное обучение
      {"/ml/linear_regression_python.py",
       "import numpy as np\nfrom sklearn.linear_model import "
       "LinearRegression\n\n# Линейная регрессия на Python\nX = "
       "np.array([[1], [2], [3], [4], [5]])\ny = np.array([1, 2, 3, 4, "
       "5])\n\nmodel = LinearRegression()\nmodel.fit(X, y)\npredictions = "
       "model.predict([[6], [7]])"},

      {"/ml/neural_network_python.py",
       "import tensorflow as tf\nfrom tensorflow.keras import "
       "Sequential\nfrom tensorflow.keras.layers import Dense\n\n# Нейронная "
       "сеть на Python\nmodel = Sequential([\n    Dense(64, "
       "activation='relu', input_shape=(10,)),\n    Dense(32, "
       "activation='relu'),\n    Dense(1, "
       "activation='sigmoid')\n])\nmodel.compile(optimizer='adam', "
       "loss='binary_crossentropy')"},

      // Системное программирование
      {"/system/file_io_python.py",
       "# Работа с файлами в Python\nwith open('file.txt', 'r') as file:\n   "
       " content = file.read()\n\nwith open('output.txt', 'w') as file:\n    "
       "file.write('Hello World!')\n\n# Чтение бинарных файлов\nwith "
       "open('image.jpg', 'rb') as file:\n    data = file.read()"},

      {"/system/file_io_cpp.cpp",
       "// Работа с файлами в C++\n#include <fstream>\n#include "
       "<string>\n\nstd::ifstream file(\"file.txt\");\nstd::string "
       "content;\nif (file.is_open()) {\n    "
       "content.assign((std::istreambuf_iterator<char>(file)),\n             "
       "      std::istreambuf_iterator<char>());\n    "
       "file.close();\n}\n\nstd::ofstream outfile(\"output.txt\");\noutfile "
       "<< \"Hello World!\";\noutfile.close();"},

      // Базы данных
      {"/database/sqlite_python.py",
       "import sqlite3\n\n# Работа с SQLite в Python\nconn = "
       "sqlite3.connect('example.db')\ncursor = "
       "conn.cursor()\n\ncursor.execute('''CREATE TABLE users\n              "
       " (id INTEGER PRIMARY KEY, name TEXT, age "
       "INTEGER)''')\n\ncursor.execute(\"INSERT INTO users (name, age) "
       "VALUES (?, ?)\", (\"Alice\", 25))\nconn.commit()\nconn.close()"},

      {"/database/mysql_cpp.cpp",
       "// Работа с MySQL в C++\n#include <mysql_driver.h>\n#include "
       "<mysql_connection.h>\n#include "
       "<cppconn/prepared_statement.h>\n\nsql::mysql::MySQL_Driver "
       "*driver;\nsql::Connection *con;\n\ndriver = "
       "sql::mysql::get_mysql_driver_instance();\ncon = "
       "driver->connect(\"tcp://127.0.0.1:3306\", \"user\", "
       "\"password\");\n\nsql::PreparedStatement *pstmt;\npstmt = "
       "con->prepareStatement(\"INSERT INTO users(name, age) VALUES(?, "
       "?)\");\npstmt->setString(1, \"Alice\");\npstmt->setInt(2, "
       "25);\npstmt->execute();\ndelete pstmt;\ndelete con;"}};

  for (const auto &[path, content] : test_files) {
    virtual_files[path] =
        fileinfo::FileInfo(S_IFREG | 0644, 0, content, getuid(), getgid(),
                           time(nullptr), time(nullptr), time(nullptr));
    update_embedding(path);
  }

  std::vector<std::vector<std::string>> access_patterns = {
      {"/python/functions_basic.py", "/python/oop_basics.py",
       "/algorithms/sort_python.py", "/web/http_server_python.py",
       "/ml/linear_regression_python.py"},

      {"/cpp/functions_basic.cpp", "/cpp/oop_basics.cpp",
       "/algorithms/sort_cpp.cpp", "/system/file_io_cpp.cpp",
       "/web/http_server_cpp.cpp"},

      {"/comparison/functions_python_vs_cpp.txt", "/web/http_server_python.py",
       "/web/http_server_cpp.cpp", "/database/sqlite_python.py",
       "/database/mysql_cpp.cpp"},

      {"/ml/linear_regression_python.py", "/ml/neural_network_python.py",
       "/python/functions_advanced.py", "/algorithms/sort_python.py",
       "/data_structures/linked_list_python.py"},

      {"/system/file_io_cpp.cpp", "/concurrency/threads_cpp.cpp",
       "/cpp/functions_advanced.cpp", "/algorithms/sort_cpp.cpp",
       "/data_structures/linked_list_cpp.cpp"}};

  for (const auto &pattern : access_patterns) {
    for (const auto &file : pattern) {
      record_file_access(file, "test_pattern");
    }
    hmm_model->add_sequence(pattern);
  }

  rebuild_index();
  update_models();

  std::vector<std::pair<std::string, std::string>> test_queries = {
      {"программирование", "Общие запросы по программированию"},
      {"нейронные сети", "Специфичные ML запросы"},
      {"SQL база данных", "Запросы по базам данных"},
      {"веб приложение", "Веб-разработка"},
      {"Linux система", "Системное администрирование"},
      {"обработка данных", "Межкатегорийные запросы"}};

  spdlog::info("Total indexed files: {}", index_to_path.size());
  spdlog::info("HMM states: {}", hmm_model->get_state_count());
  spdlog::info("Semantic graph edges: {}", semantic_graph->get_edge_count());
  spdlog::info("");

  for (const auto &[query, description] : test_queries) {
    spdlog::info("=== Query: '{}' ({}) ===", query, description);

    auto basic_results = semantic_search(query, 5);
    spdlog::info("Basic semantic search results:");
    if (basic_results.empty()) {
      spdlog::warn("  No results found");
    } else {
      for (const auto &[path, score] : basic_results) {
        auto category = hmm_model->classify_file_category(path, {});
        spdlog::info("  📄 {} (score: {:.3f}, category: {})", path, score,
                     category);

        auto it = virtual_files.find(path);
        if (it != virtual_files.end()) {
          spdlog::info("    Content: {}...", it->second.content.substr(0, 40));
        }
      }
    }

    auto enhanced_results = enhanced_semantic_search(query, 3);
    spdlog::info("Enhanced search with Markov chains:");
    if (enhanced_results.empty()) {
      spdlog::warn("  No enhanced results found");
    } else {
      for (const auto &[path, score] : enhanced_results) {
        auto category = hmm_model->classify_file_category(path, {});
        spdlog::info("  🚀 {} (score: {:.3f}, category: {})", path, score,
                     category);
      }
    }

    if (!basic_results.empty()) {
      auto recommendations = get_recommendations_for_query(query);
      if (!recommendations.empty()) {
        spdlog::info("Related recommendations:");
        for (const auto &rec : recommendations) {
          auto category = hmm_model->classify_file_category(rec, {});
          spdlog::info("  → {} (category: {})", rec, category);
        }
      }
    }

    spdlog::info("---");
  }

  spdlog::info("=== Additional Statistics ===");

  auto hubs = semantic_graph->get_semantic_hubs(5);
  if (!hubs.empty()) {
    spdlog::info("Top semantic hubs:");
    for (const auto &hub : hubs) {
      auto category = hmm_model->classify_file_category(hub, {});
      spdlog::info("  ⭐ {} (category: {})", hub, category);
    }
  }

  auto predictions = predict_next_files();
  if (!predictions.empty()) {
    spdlog::info("Predicted next files:");
    for (const auto &pred : predictions) {
      auto category = hmm_model->classify_file_category(pred, {});
      spdlog::info("  🔮 {} (category: {})", pred, category);
    }
  }

  spdlog::info("File categorization summary:");
  std::map<std::string, int> category_counts;
  for (const auto &[path, _] : virtual_files) {
    std::string category = hmm_model->classify_file_category(path, {});
    category_counts[category]++;
  }

  for (const auto &[category, count] : category_counts) {
    spdlog::info("  {}: {} files", category, count);
  }
}

void VectorFS::test_markov_chains() {
  spdlog::info("=== Testing Markov Chains ===");

  std::vector<std::string> test_files = {
      "/code/main.cpp", "/code/utils.h", "/docs/readme.txt",
      "/config/settings.json", "/tests/test1.py"};

  std::vector<std::string> test_contents = {
      "#include <iostream>\nusing namespace std;",
      "#pragma once\nvoid helper_function();",
      "Project documentation\nImportant information",
      "{\"debug\": true, \"port\": 8080}",
      "def test_function():\n    assert True"};

  for (size_t i = 0; i < test_files.size(); i++) {
    virtual_files[test_files[i]] = fileinfo::FileInfo(
        S_IFREG | 0644, 0, test_contents[i], getuid(), getgid(), time(nullptr),
        time(nullptr), time(nullptr));
    update_embedding(test_files[i]);
  }

  std::vector<std::vector<std::string>> test_sequences = {
      {"/code/main.cpp", "/code/utils.h", "/tests/test1.py"},
      {"/docs/readme.txt", "/code/main.cpp", "/config/settings.json"},
      {"/config/settings.json", "/code/utils.h", "/docs/readme.txt"}};

  for (const auto &seq : test_sequences) {
    hmm_model->add_sequence(seq);
    for (const auto &file : seq) {
      record_file_access(file, "test");
    }
  }

  update_models();

  spdlog::info("Markov chains test completed");
}

void VectorFS::test_container() {
  try {
    std::string container_data_path = "/home/bararide/test_container_2";
    std::filesystem::remove_all(container_data_path);
    std::filesystem::create_directories(container_data_path);
    spdlog::info("Created clean data directory: {}", container_data_path);

    auto container_builder = ossec::ContainerBuilder::create();

    auto container_result =
        container_builder.with_owner("test_user")
            .with_container_id("test_container_2")
            .with_data_path(container_data_path)
            .with_vectorfs_namespace("default")
            .with_supported_formats({"txt", "json", "yaml", "cpp", "py"})
            .with_vector_search(true)
            .with_memory_limit(512)
            .with_storage_quota(1024)
            .with_file_limit(1000)
            .with_label("environment", "development")
            .with_label("type", "knowledge_base")
            .with_commands({"search", "debug", "all"})
            .privileged(false)
            .build();

    if (container_result.is_ok()) {
      auto container = container_result.value();

      auto pid_container =
          std::make_shared<ossec::PidContainer>(std::move(container));

      auto start_result = pid_container->start();
      if (start_result.is_ok()) {
        auto adapter = std::make_shared<OssecContainerAdapter>(pid_container);

        if (container_manager_.register_container(adapter)) {
          spdlog::info("Successfully created and registered container: "
                       "test_container_1");

          std::vector<std::pair<std::string, std::string>> files_to_create = {
              {"/readme.txt",
               "Welcome to test_container_1!\n\n"
               "This container contains sample knowledge files.\n"
               "You can browse and search through the files here.\n\n"
               "Status: " +
                   adapter->get_status() +
                   "\n"
                   "Size: " +
                   std::to_string(adapter->get_size()) +
                   " bytes\n"
                   "Owner: " +
                   adapter->get_owner()},

              {"/examples/hello.cpp", "#include <iostream>\n\n"
                                      "int main() {\n"
                                      "    std::cout << \"Hello from VectorFS "
                                      "container!\" << std::endl;\n"
                                      "    return 0;\n}"},

              {"/examples/sort.cpp",
               "#include <algorithm>\n#include <vector>\n\n"
               "void bubble_sort(std::vector<int>& arr) {\n"
               "    int n = arr.size();\n"
               "    for (int i = 0; i < n-1; i++) {\n"
               "        for (int j = 0; j < n-i-1; j++) {\n"
               "            if (arr[j] > arr[j+1]) {\n"
               "                std::swap(arr[j], arr[j+1]);\n"
               "            }\n"
               "        }\n"
               "    }\n}"},

              {"/notes/programming.md", "# Programming Notes\n\n"
                                        "## C++ Tips\n"
                                        "- Use smart pointers\n"
                                        "- Prefer RAII\n"
                                        "- Learn move semantics\n\n"
                                        "## Python Tips\n"
                                        "- Use virtual environments\n"
                                        "- Write docstrings\n"
                                        "- Follow PEP8"},

              {"/config/settings.json",
               "{\n"
               "  \"container_id\": \"test_container_1\",\n"
               "  \"owner\": \"test_user\",\n"
               "  \"created\": \"2024-10-17\",\n"
               "  \"status\": \"active\"\n}"}};

          for (const auto &[file_path, content] : files_to_create) {
            bool success = adapter->add_file(file_path, content);
            if (success) {
              spdlog::info("Successfully created file: {}", file_path);
            } else {
              spdlog::error("Failed to create file: {}", file_path);
            }
          }

          auto files = adapter->list_files("/");
          spdlog::info("Container root now contains {} files:", files.size());
          for (const auto &file : files) {
            spdlog::info("  - {}", file);

            if (file.find('.') ==
                std::string::npos) {
              auto sub_files = adapter->list_files("/" + file);
              for (const auto &sub_file : sub_files) {
                spdlog::info("    - {}/{}", file, sub_file);
              }
            }
          }

        } else {
          spdlog::error("Failed to register container adapter");
        }
      } else {
        spdlog::error("Failed to start container: {}",
                      start_result.error().what());
      }
    } else {
      spdlog::error("Failed to build container: {}",
                    container_result.error().what());
    }

  } catch (const std::exception &e) {
    spdlog::error("Exception while creating container: {}", e.what());
  }
}

} // namespace owl::vectorfs