#ifndef PFW_INOTIFY_EVENT_LOOP_H
#define PFW_INOTIFY_EVENT_LOOP_H

#include <sys/inotify.h>
#include <filesystem>

#include "fw/InotifyService.h"

class InotifyService;
namespace fs = std::filesystem;

class InotifyEventLooper {
  struct InotifyRenameEvent {
    InotifyRenameEvent()
      : cookie(0), isDirectory(false), isGood(false), wd(0) {};

    InotifyRenameEvent(const inotify_event* event, bool isDirectoryEvent)
      : cookie(event->cookie)
        , isDirectory(isDirectoryEvent)
        , isGood(true)
        , name(event->name)
        , wd(event->wd) {};

    uint32_t cookie;
    bool isDirectory;
    bool isGood;
    fs::path name;
    int wd;
  };

public:
  using ptr = InotifyEventLooper*;
  InotifyEventLooper(int inotifyInstance, InotifyService* inotifyService);

  bool isLooping() const;

  void work();

  ~InotifyEventLooper();

private:
  /// 记录事件

  void recordChangedEvent(const inotify_event* event) const;
  void recordDeletedEvent(const inotify_event* event, bool isDir) const;
  void recordCreatedEvent(const inotify_event* event, bool isDirectoryEvent, bool sendInitEvents = true) const;

  void recordRenameOldEvent(const inotify_event* event, bool isDirectoryEvent, InotifyRenameEvent& renameEvent) const;
  void recordRenameNewEvent(const inotify_event* event, bool isDirectoryEvent, InotifyRenameEvent& renameEvent) const;

  void handleEvent(const inotify_event* event, InotifyRenameEvent& renameEvent) const;
  InotifyService* mInotifyService;
  const int mInotifyInstance;
  std::atomic<bool> mRunning;

  std::thread mEventLoopThread;
  std::binary_semaphore mThreadStartedSemaphore;
};

#define HANDLE_ERROR_CODE(condition, msg,statement) \
do {                                                \
  if((condition)) {                                 \
    mInotifyService->sendError(msg);                \
    statement;                                      \
  }                                                 \
} while(false)

#define CONTINUE_LOOP_ON_CONDITION(condition)       \
  if((condition)) { continue; }

#endif
