// ReSharper disable CppRedundantQualifier
#include <map>
#include <csignal>
#include <cstring>
#include <thread>

#include "fw/Collector.h"

Collector::Collector(const Filter::sptr& filter,
                     const std::chrono::milliseconds sleepDuration)
  : mFilter(filter)
    , mSleepDuration(sleepDuration)
    , mStopped(true) {
  const auto result = pthread_create(&mRunner, nullptr, work, this);

  if (result != 0) {
    filter->sendError("pthread_create失败: " + std::string(strerror(errno)));
    return;
  }
}

Collector::~Collector() {
  if (mStopped) { return; }

  mStopped = true;

  const auto previousHandler = std::signal(32, SIG_IGN);

  auto errorCode = pthread_cancel(mRunner);
  if (errorCode != 0) {
    mFilter->sendError("cancel失败: " + std::to_string(errorCode));
    return;
  }

  errorCode = pthread_join(mRunner, nullptr);
  if (errorCode != 0) {
    mFilter->sendError("join失败: " + std::to_string(errorCode));
  }

  if (previousHandler != SIG_ERR) {
    std::signal(32, previousHandler);
  }
}

void Collector::finish(void* args) {
  auto const collector = static_cast<Collector::ptr>(args);
  collector->mStopped = true;
}

void* Collector::work(void* args) {
  pthread_cleanup_push(Collector::finish, args);

    const Collector::ptr collector = static_cast<Collector::ptr>(args);

    collector->mStopped = false;

    while (!collector->mStopped) {
      collector->sendEvents();
      std::this_thread::sleep_for(collector->mSleepDuration);
    }

  pthread_cleanup_pop(1);
  return nullptr;
}

void Collector::sendEvents() {
  std::vector<Event::uptr> result = std::vector<Event::uptr>();

  {
    std::lock_guard lockIn(event_input_mutex);
    std::swap(inputVector, result);
  }

  std::map<fs::path, std::vector<Event::uptr>::reverse_iterator> values;
  for (auto itr = result.rbegin(); itr != result.rend(); ++itr) {
    auto _result = values.emplace((*itr)->relativePath, itr);

    if (_result.second) { continue; }

    Event::uptr& event = *itr;
    const Event::uptr& conflictedEvent = *_result.first->second;
    conflictedEvent->type = conflictedEvent->type | event->type;

    event.reset(nullptr);
  }

  std::erase_if(result, [&](const Event::uptr& value) { return !value; });

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
