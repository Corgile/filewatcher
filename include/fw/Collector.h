//
// FileWatch / collector.hh
// Created by brian on 2024-06-07.
//

#ifndef COLLECTOR_HH
#define COLLECTOR_HH

// ReSharper disable CppRedundantQualifier
#include <thread>
#include <vector>
#include <memory>
#include <atomic>
#include <chrono>
#include <mutex>
#include <condition_variable>

#include "fw/Filter.h"

class Collector {
public:
  using sptr = std::shared_ptr<Collector>;
  using ptr = Collector*;

  Collector(const Filter::sptr& filter, std::chrono::milliseconds sleepDuration);
  ~Collector();

  void insert(std::vector<Event::uptr>&& events);
  void collect(EventType type, const fs::path& relativePath);

  void sendEvents();
  void sendError(const std::string& errorMsg) const;

private:
  // void stop();
  void work();

  Filter::sptr mFilter;
  std::chrono::milliseconds mSleepDuration;
  std::atomic<bool> mRunning;
  std::thread mRunner;
  std::mutex event_input_mutex;
  std::vector<Event::uptr> inputVector;
};

#endif //COLLECTOR_HH
