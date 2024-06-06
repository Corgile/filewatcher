#ifndef PFW_COLLECTOR_H
#define PFW_COLLECTOR_H

#include <atomic>
#include <chrono>
#include <mutex>
#include <vector>

#include "fw/Filter.h"
#include "fw/Semaphore.h"

class Collector {
public:
  using sptr = std::shared_ptr<Collector>;
  using ptr = Collector*;
  Collector(const Filter::sptr& filter, std::chrono::milliseconds sleepDuration);
  ~Collector();

  static void finish(void* args);
  static void* work(void* args);

  void sendError(const std::string& errorMsg) const;
  void insert(std::vector<Event::uptr>&& events);
  void collect(EventType type, const fs::path& relativePath);

private:
  void sendEvents();

  Filter::sptr mFilter;
  std::chrono::milliseconds mSleepDuration;
  pthread_t mRunner;
  std::atomic<bool> mStopped;
  std::vector<Event::uptr> inputVector;
  std::mutex event_input_mutex;
};

#endif
