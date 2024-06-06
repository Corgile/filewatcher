#ifndef PFW_SINGLESHOT_SEMAPHORE_H
#define PFW_SINGLESHOT_SEMAPHORE_H

#include <condition_variable>
#include <mutex>

class Semaphore {
public:
  Semaphore() : _state(false) {}

  void wait() {
    std::unique_lock lk(_mutex);
    while (!_state) { _cond.wait(lk); }
  }

  bool waitFor(const std::chrono::milliseconds ms) {
    std::unique_lock lk(_mutex);
    if (_state) { return true; }
    _cond.wait_for(lk, ms);
    return _state;
  }

  void signal() {
    std::unique_lock lk(_mutex);
    _state = true;
    _cond.notify_all();
  }

private:
  std::mutex _mutex;
  std::condition_variable _cond;
  bool _state;
};

#endif
