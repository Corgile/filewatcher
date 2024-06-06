#ifndef PFW_EVENT_H
#define PFW_EVENT_H

#include <filesystem>
#include <map>

namespace fs = std::filesystem;

enum EventType : uint8_t {
  NONE = 0,
  CREATED = 1 << 0,
  CHANGED = 1 << 1,
  DELETED = 1 << 2,
  RENAMED = 1 << 3,
  OVERFLOW = 1 << 4,
  FAILED = 1 << 5
};

inline bool noop(const EventType eventType) { return eventType == NONE; }

inline bool created(const EventType eventType) {
  return (eventType & CREATED) == CREATED;
}

inline bool modified(const EventType eventType) {
  return (eventType & CHANGED) == CHANGED;
}

inline bool deleted(const EventType eventType) {
  return (eventType & DELETED) == DELETED;
}

inline bool renamed(const EventType eventType) {
  return (eventType & RENAMED) == RENAMED;
}

inline bool buffer_overflow(const EventType eventType) {
  return (eventType & OVERFLOW) == OVERFLOW;
}

inline bool failed(const EventType eventType) {
  return (eventType & FAILED) == FAILED;
}

inline EventType operator|(EventType lhs, EventType rhs) {
  return static_cast<EventType>(static_cast<uint8_t>(lhs) |
    static_cast<uint8_t>(rhs));
}

inline EventType operator&(EventType lhs, EventType rhs) {
  return static_cast<EventType>(static_cast<uint8_t>(lhs) &
    static_cast<uint8_t>(rhs));
}

inline EventType operator~(EventType lhs) {
  return static_cast<EventType>(~static_cast<uint8_t>(lhs));
}

const std::map<EventType, std::string> eventTypeToString = {
  {NONE, "NONE"},
  {CREATED, "创建"},
  {CHANGED, "修改"},
  {DELETED, "删除"},
  {RENAMED, "重命名"},
  {OVERFLOW, "溢出"},
  {FAILED, "失败"}
};

inline std::string translate(EventType eventType) {
  std::string result;
  if (renamed(eventType)) {
    eventType = eventType & RENAMED;
  }
  for (const auto& [event, literal] : eventTypeToString) {
    if (eventType & event) {
      if (!result.empty()) {
        result += " | ";
      }
      result += literal;
    }
  }
  return result;
}

struct Event {
  using uptr = std::unique_ptr<Event>;
  Event(const EventType type, fs::path relativePath)
    : type(type), relativePath(std::move(relativePath)) {
    timePoint = std::chrono::high_resolution_clock::now();
  }

  EventType type;
  fs::path relativePath;
  std::chrono::high_resolution_clock::time_point timePoint;
};

#endif
