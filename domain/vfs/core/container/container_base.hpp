#ifndef OWL_VFS_CORE_CONTAINER_CONTAINER_BASE
#define OWL_VFS_CORE_CONTAINER_CONTAINER_BASE

#include <infrastructure/result.hpp>
#include <map>

namespace owl {

// ---------- Метаданные и базовая идентичность ----------

template <typename Derived> class IdentifiableContainer {
public:
  std::string getId() const { return derived().getId(); }

  std::string getOwner() const { return derived().getOwner(); }

  std::string getNamespace() const { return derived().getNamespace(); }

  std::string getDataPath() const { return derived().getDataPath(); }

  std::vector<std::string> getCommands() const {
    return derived().getCommands();
  }

  std::map<std::string, std::string> getLabels() const {
    return derived().getLabels();
  }

private:
  const Derived &derived() const { return static_cast<const Derived &>(*this); }
  Derived &derived() { return static_cast<Derived &>(*this); }
};

// ---------- ФС‑аспекты контейнера ----------

template <typename Derived> class FileSystemContainer {
public:
  core::Result<std::vector<std::string>>
  listFiles(const std::string &virtual_path = "/") const {
    return derived().listFiles(virtual_path);
  }

  core::Result<bool> fileExists(const std::string &virtual_path) const {
    return derived().fileExists(virtual_path);
  }

  core::Result<bool> isDirectory(const std::string &virtual_path) const {
    return derived().isDirectory(virtual_path);
  }

  core::Result<std::string>
  getFileContent(const std::string &virtual_path) const {
    return derived().getFileContent(virtual_path);
  }

  core::Result<void> addFile(const std::string &virtual_path,
                             const std::string &content) {
    return derived().addFile(virtual_path, content);
  }

  core::Result<void> removeFile(const std::string &virtual_path) {
    return derived().removeFile(virtual_path);
  }

  core::Result<std::vector<std::string>>
  searchFiles(const std::string &pattern) const {
    return derived().searchFiles(pattern);
  }

  core::Result<size_t> getSize() const { return derived().getSize(); }

private:
  const Derived &derived() const { return static_cast<const Derived &>(*this); }
  Derived &derived() { return static_cast<Derived &>(*this); }
};

// ---------- Ресурс‑менеджмент ----------

template <typename Derived> class ResourceManagedContainer {
public:
  core::Result<void> setResourceLimit(const std::string &resource_name,
                                      const std::string &value) {
    return derived().setResourceLimit(resource_name, value);
  }

  core::Result<std::string> getCurrentResources() const {
    return derived().getCurrentResources();
  }

private:
  const Derived &derived() const { return static_cast<const Derived &>(*this); }
  Derived &derived() { return static_cast<Derived &>(*this); }
};

// ---------- Search / Embedding / Recs ----------

template <typename Derived> class SearchableContainer {
public:
  core::Result<std::vector<std::pair<std::string, float>>>
  semanticSearch(const std::string &query, int limit = 10) {
    return derived().semanticSearch(query, limit);
  }

  core::Result<std::vector<std::pair<std::string, float>>>
  enhancedSemanticSearch(const std::string &query, int limit = 10) {
    return derived().enhancedSemanticSearch(query, limit);
  }

  core::Result<std::vector<std::string>>
  getRecommendations(const std::string &current_file, int limit = 5) {
    return derived().getRecommendations(current_file, limit);
  }

  core::Result<std::vector<std::string>> predictNextFiles(int limit = 3) {
    return derived().predictNextFiles(limit);
  }

  core::Result<std::vector<std::string>> getSemanticHubs(int count = 5) {
    return derived().getSemanticHubs(count);
  }

  core::Result<std::string> classifyFile(const std::string &file_path) {
    return derived().classifyFile(file_path);
  }

  core::Result<void> updateAllEmbeddings() {
    return derived().updateAllEmbeddings();
  }

  core::Result<std::string> getSearchInfo() const {
    return derived().getSearchInfo();
  }

  core::Result<void> recordSearchQuery(const std::string &query) {
    return derived().recordSearchQuery(query);
  }

private:
  const Derived &derived() const { return static_cast<const Derived &>(*this); }
  Derived &derived() { return static_cast<Derived &>(*this); }
};

// ---------- Статус контейнера ----------

template <typename Derived> class StatefulContainer {
public:
  std::string getStatus() const { return derived().getStatus(); }

  bool isAvailable() const { return derived().isAvailable(); }

private:
  const Derived &derived() const { return static_cast<const Derived &>(*this); }
  Derived &derived() { return static_cast<Derived &>(*this); }
};

// ---------- Объединённый CRTP‑интерфейс контейнера ----------

template <typename Derived>
class ContainerBase : public IdentifiableContainer<Derived>,
                      public FileSystemContainer<Derived>,
                      public ResourceManagedContainer<Derived>,
                      public SearchableContainer<Derived>,
                      public StatefulContainer<Derived> {
public:
  using Self = Derived;
};

} // namespace owl

#endif // OWL_VFS_CORE_CONTAINER_CONTAINER_BASE