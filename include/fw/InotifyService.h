#ifndef PFW_INOTIFY_SERVICE_H
#define PFW_INOTIFY_SERVICE_H

#include "fw/Filter.h"
#include "fw/Collector.h"
#include "fw/InotifyEventLoop.h"
#include "fw/InotifyTree.h"

class InotifyEventLooper;
class InotifyTree;

class InotifyService {
public:
  using ptr = InotifyService*;
  InotifyService(const std::shared_ptr<Filter>& filter,
                 const fs::path& path,
                 std::chrono::milliseconds latency);

  bool isWatching() const;

  ~InotifyService();

private:
  void dispatchEvent(EventType action, int wd, const fs::path& name) const;
  void dispatchEvent(EventType actionOld, int wdOld, const fs::path& nameOld,
                     EventType actionNew, int wdNew, const fs::path& nameNew) const;

  void emitEventCreate(int wd, const fs::path& name) const;
  void emitEventCreateDir(int wd, const fs::path& name, bool sendInitEvents) const;
  void emitEventModify(int wd, const fs::path& name) const;
  void emitEventDelete(int wd, const fs::path& name) const;
  void emitEventDeleteDir(int wd) const;
  void emitEventDeleteDir(int wd, const fs::path& name) const;
  void emitEventMove(int wdOld, const fs::path& nameOld, int wdNew, const fs::path& nameNew) const;
  void emitEventMoveDir(int wdOld, const fs::path& nameOld, int wdNew, const fs::path& newName) const;

  void sendError(const std::string& errorMsg) const;

  InotifyEventLooper* mEventLoop;
  std::shared_ptr<Collector> mCollector;
  InotifyTree* mTree;
  int mInotifyInstance;

  friend class InotifyEventLooper;
};

#endif
