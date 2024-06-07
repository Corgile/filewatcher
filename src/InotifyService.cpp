#include "fw/InotifyService.h"

InotifyService::InotifyService(const std::shared_ptr<Filter>& filter,
                               const fs::path& path,
                               const std::chrono::milliseconds latency)
  : mEventLoop(nullptr)
    , mCollector(std::make_shared<Collector>(filter, latency))
    , mTree(nullptr) {
  mInotifyInstance = inotify_init();

  if (mInotifyInstance == -1) {
    mCollector->sendError("inotify_init 失败");
    return;
  }

  mTree = new InotifyTree(mInotifyInstance, path, mCollector);
  if (mTree->isRootAlive()) {
    /// 实例化即启动 .wait()
    mEventLoop = new InotifyEventLooper(mInotifyInstance, this);
  } else {
    delete mTree;
    mTree = nullptr;
    mEventLoop = nullptr;
  }
}

InotifyService::~InotifyService() {
  delete mEventLoop;
  delete mTree;
  close(mInotifyInstance);
}

void InotifyService::emitEventCreate(const int wd, const fs::path& name) const {
  dispatchEvent(CREATED, wd, name);
}

void InotifyService::sendError(const std::string& errorMsg) const {
  mCollector->sendError(errorMsg);
}

void InotifyService::dispatchEvent(EventType actionOld,
                                   const int wdOld,
                                   const fs::path& nameOld,
                                   EventType actionNew,
                                   const int wdNew,
                                   const fs::path& nameNew) const {
  std::vector<Event::uptr> result;
  fs::path pathOld;
  if (!mTree->getRelPath(pathOld, wdOld)) {
    return;
  }
  result.emplace_back(std::make_unique<Event>(actionOld, pathOld / nameOld));

  fs::path pathNew;
  if (!mTree->getRelPath(pathNew, wdNew)) {
    return;
  }
  result.emplace_back(std::make_unique<Event>(actionNew, pathNew / nameNew));

  mCollector->insert(std::move(result));
}

void InotifyService::dispatchEvent(const EventType action,
                                   const int wd,
                                   const fs::path& name) const {
  fs::path path;
  if (!mTree->getRelPath(path, wd)) {
    return;
  }

  mCollector->collect(action, std::move(path / name));
}

bool InotifyService::isWatching() const {
  if (mTree == nullptr || mEventLoop == nullptr) {
    return false;
  }

  return mTree->isRootAlive() && mEventLoop->isLooping();
}

void InotifyService::emitEventModify(const int wd, const fs::path& name) const {
  dispatchEvent(CHANGED, wd, name);
}

void InotifyService::emitEventCreateDir(const int wd,
                                        const fs::path& name,
                                        const bool sendInitEvents) const {
  if (!mTree->nodeExists(wd)) { return; }
  mTree->addDirNode(wd, name, sendInitEvents);
  dispatchEvent(CREATED, wd, name);
}

void InotifyService::emitEventDelete(const int wd, const fs::path& name) const {
  dispatchEvent(DELETED, wd, name);
}
void InotifyService::emitEventDeleteDir(const int wd) const {
  mTree->removeDirNode(wd);
}
void InotifyService::emitEventDeleteDir(const int wd, const fs::path& name) const {
  mTree->removeDirNode(wd, name);
}

void InotifyService::emitEventMove(const int wdOld,
                                   const fs::path& nameOld,
                                   const int wdNew,
                                   const fs::path& nameNew) const {
  /// TODO 只保留RENAMED就够了
  dispatchEvent(DELETED | RENAMED, wdOld, nameOld, CREATED | RENAMED, wdNew, nameNew);
}

void InotifyService::emitEventMoveDir(const int wdOld,
                                      const fs::path& nameOld,
                                      const int wdNew,
                                      const fs::path& newName) const {
  emitEventMove(wdOld, nameOld, wdNew, newName);
  mTree->moveDirNode(wdOld, nameOld, wdNew, newName);
}
