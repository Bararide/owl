#ifndef OWL_VFS_CORE_LOOP_LOOP
#define OWL_VFS_CORE_LOOP_LOOP

namespace owl {

template <typename Derived> class Loop {
public:
  void setIsActive(bool is_active) { is_active_ = is_active; }

  [[nodiscard]] bool getIsActive() const noexcept { return is_active_; }

  void start() { static_cast<Derived *>(this)->start(); }

  void update() { static_cast<Derived *>(this)->update(); }

  void stop() { static_cast<Derived *>(this)->stop(); }

  ~ILoop() = default;

private:
  friend Derived;

  bool is_active_{false};
};

} // namespace owl

#endif // !OWL_VFS_CORE_LOOP_LOOP
