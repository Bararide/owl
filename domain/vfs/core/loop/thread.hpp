#ifndef OWL_VFS_CORE_THREAD
#define OWL_VFS_CORE_THREAD

#include <thread>
#include <sched.h>
#include <spdlog/spdlog.h>

namespace owl {

inline bool setThreadAffinity(std::thread* thread, const cpu_set_t& cpu_set) {
    return pthread_setaffinity_np(thread->native_handle(), sizeof(cpu_set_t), &cpu_set) == 0;
}

inline bool setThreadAffinity(std::thread* thread, uint64_t cpu_id) {
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    CPU_SET(cpu_id, &cpu_set);
    return setThreadAffinity(thread, cpu_set);
}

inline bool setThreadName(std::thread* thread, std::string_view name) {
    return pthread_setname_np(thread->native_handle(), name.data()) == 0;
}

inline void setThreadNameAndAffinity(
  std::thread* thread,
  std::string_view new_name,
  std::optional<uint64_t> cpu_id = std::nullopt) {
    if (!setThreadName(thread, new_name)) {
        spdlog::warn("Failed to set thread name");
    }
    if (cpu_id.has_value()) {
        if (!setThreadAffinity(thread, cpu_id.value())) {
            spdlog::warn("Failed to set thread affinity");
        }
    }
}

}  // namespace owl

#endif  // OWL_VFS_CORE_THREAD
