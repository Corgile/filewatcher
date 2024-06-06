#include "fw/Filter.h"

#pragma unmanaged

Filter::Filter(const CallBackSignatur& callBack) {
  mCallbackHandle = registerCallback(callBack);
}

Filter::~Filter() { deRegisterCallback(mCallbackHandle); }

void Filter::sendError(const std::string& errorMsg) {
  std::vector<Event::uptr> events;
  events.emplace_back(std::make_unique<Event>(FAILED, errorMsg));
  notify(std::move(events));
}

void Filter::filterAndNotify(std::vector<Event::uptr>&& events) {
  if (events.empty()) {
    return;
  }
  notify(std::move(events));
}
