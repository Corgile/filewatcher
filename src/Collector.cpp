#include <thread>

#include "fw/Collector.h"

Collector::Collector(const Filter::sptr& filter, const std::chrono::milliseconds sleepDuration)
  : mFilter(filter), mSleepDuration(sleepDuration), mRunning(true) {
  mRunner = std::thread(&Collector::work, this);
}

Collector::~Collector() {
  mRunning = false;
  if (mRunner.joinable()) { mRunner.join(); }
}

/*void Collector::stop() {
  mRunning = true;
}*/

void Collector::work() {
  while (mRunning) {
    sendEvents();
    std::this_thread::sleep_for(mSleepDuration);
  }
}

void Collector::sendEvents() {
  std::vector<Event::uptr> result;
  {
    std::lock_guard lock(event_input_mutex);
    std::swap(inputVector, result);
  }

  std::map<fs::path, std::vector<Event::uptr>::reverse_iterator> values;
  for (auto itr = result.rbegin(); itr != result.rend(); ++itr) {
    auto [pos, inserted] = values.emplace((*itr)->relativePath, itr);

    if (inserted) { continue; }

    Event::uptr& event = *itr;
    const Event::uptr& conflictedEvent = *pos->second;
    conflictedEvent->type = conflictedEvent->type | event->type;

    event.reset(nullptr);
  }
  std::erase_if(result, [](const Event::uptr& value) { return !value; });
  mFilter->filterAndNotify(std::move(result));
}

void Collector::sendError(const std::string& errorMsg) const {
  mFilter->sendError(errorMsg);
}

void Collector::insert(std::vector<Event::uptr>&& events) {
  std::lock_guard lock(event_input_mutex);
  for (auto& event : events) {
    inputVector.push_back(std::move(event));
  }
}

void Collector::collect(EventType type, const fs::path& relativePath) {
  std::lock_guard lock(event_input_mutex);
  inputVector.emplace_back(std::make_unique<Event>(type, relativePath));
}
