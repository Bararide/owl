#ifndef FILEINFO_HPP
#define FILEINFO_HPP

#include <algorithm>
#include <string>
#include <locale>
#include <memory>
#include <unistd.h>
#include <vector>

namespace vfs::fileinfo {
struct FileInfo {
  mode_t mode;
  size_t size;
  std::string content;
  uid_t uid;
  gid_t gid;
  time_t access_time;
  time_t modification_time;
  time_t create_time;
  std::vector<float> embedding;
  bool embedding_updated;

  FileInfo()
      : mode(0), size(0), uid(0), gid(0), access_time(time(nullptr)),
        modification_time(time(nullptr)), create_time(time(nullptr)),
        embedding_updated(false) {}

  FileInfo(mode_t mode, size_t size, const std::string &content, uid_t uid,
           gid_t gid, time_t access_time, time_t modification_time,
           time_t create_time)
      : mode(mode), size(size), content(content), uid(uid), gid(gid),
        access_time(access_time), modification_time(modification_time),
        create_time(create_time), embedding_updated(false) {}

  FileInfo(mode_t mode, size_t size, uid_t uid, gid_t gid, time_t access_time,
           time_t modification_time, time_t create_time)
      : mode(mode), size(size), uid(uid), gid(gid), access_time(access_time),
        modification_time(modification_time), create_time(create_time),
        embedding_updated(false) {}
};
} // namespace fileinfo

#endif // FILEINFO_HPP
