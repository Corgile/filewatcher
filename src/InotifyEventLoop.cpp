// ReSharper disable CppRedundantQualifier
#include "fw/InotifyEventLoop.h"

#include <sys/ioctl.h>
#include <csignal>
#include <cstring>

InotifyEventLooper::InotifyEventLooper(const int inotifyInstance,
                                       const InotifyService::ptr inotifyService)
  : mInotifyService(inotifyService)
    , mInotifyInstance(inotifyInstance)
    , mStopped(true) {
  const int result = pthread_create(&mEventLoopThread, nullptr, work, this);

  if (result != 0) {
    mInotifyService->sendError(strerror(errno));
    return;
  }

  /// main loop
  mThreadStartedSemaphore.wait();
}

bool InotifyEventLooper::isLooping() const { return !mStopped; }

void InotifyEventLooper::recordCreatedEvent(const inotify_event* event,
                                            const bool isDirectoryEvent,
                                            const bool sendInitEvents) const {
  if (event == nullptr || mStopped) {
    return;
  }

  if (isDirectoryEvent) {
    auto v = strdup(event->name);
    mInotifyService->emitEventCreateDir(event->wd, v, sendInitEvents);
    free(v);
  } else {
    auto name = strdup(event->name);
    mInotifyService->emitEventCreate(event->wd, name);
    free(name);
  }
}

void InotifyEventLooper::recordChangedEvent(const inotify_event* event) const {
  if (event == nullptr || mStopped) {
    return;
  }
  auto name = strdup(event->name);
  mInotifyService->emitEventModify(event->wd, name);
  free(name);
}

void InotifyEventLooper::recordDeletedEvent(const inotify_event* event, bool isDirectoryEvent) const {
  if (event == nullptr || mStopped) {
    return;
  }

  if (isDirectoryEvent) {
    mInotifyService->emitEventDeleteDir(event->wd);
  } else {
    auto name = strdup(event->name);
    mInotifyService->emitEventDelete(event->wd, name);
    free(name);
  }
}

void InotifyEventLooper::recordRenameOldEvent(const inotify_event* event,
                                              const bool isDirectoryEvent,
                                              InotifyRenameEvent& renameEvent) const {
  if (mStopped) { return; }
  renameEvent = InotifyRenameEvent(event, isDirectoryEvent);
}

void InotifyEventLooper::recordRenameNewEvent(const inotify_event* event,
                                              bool isDirectoryEvent,
                                              InotifyRenameEvent& renameEvent) const {
  if (mStopped) { return; }

  if (!renameEvent.isGood) {
    return recordCreatedEvent(event, isDirectoryEvent, false);
  }

  renameEvent.isGood = false;

  if (renameEvent.cookie != event->cookie) {
    if (renameEvent.isDirectory) {
      mInotifyService->emitEventDeleteDir(renameEvent.wd, renameEvent.name);
    }
    mInotifyService->emitEventDelete(renameEvent.wd, renameEvent.name);

    return recordCreatedEvent(event, isDirectoryEvent, false);
  }

  if (renameEvent.isDirectory) {
    mInotifyService->emitEventMoveDir(renameEvent.wd, renameEvent.name,
                                      event->wd, event->name);
  } else {
    mInotifyService->emitEventMove(renameEvent.wd, renameEvent.name,
                                   event->wd, event->name);
  }
}

void InotifyEventLooper::finish(void* args) {
  auto const eventLoop = static_cast<InotifyEventLooper::ptr>(args);
  eventLoop->mStopped = true;
}

void* InotifyEventLooper::work(void* args) {
  pthread_cleanup_push(InotifyEventLooper::finish, args);

    static constexpr int BUFFER_SIZE = 16384;
    auto const eventLoop = static_cast<InotifyEventLooper::ptr>(args);

    eventLoop->mStopped = false;
    eventLoop->mThreadStartedSemaphore.signal();

    InotifyRenameEvent renameEvent;

    while (!eventLoop->mStopped) {
      char buffer[BUFFER_SIZE];
      auto bytesRead = read(eventLoop->mInotifyInstance, &buffer, BUFFER_SIZE);
      if (eventLoop->mStopped) { break; }
      if (bytesRead == 0) {
        eventLoop->mInotifyService->sendError("InotifyEventLooper 线程 mStopped,因为读取返回 0.");
        break;
      }
      if (bytesRead == -1) {
        eventLoop->mInotifyService->sendError("无法读取inotify: " + std::string(strerror(errno)));
        break;
      }

      ssize_t position = 0;
      inotify_event* event;
      do {
        if (eventLoop->mStopped) {
          break;
        }
        event = reinterpret_cast<inotify_event*>(buffer + position);

        const bool isDirectoryRemoval = event->mask & (IN_IGNORED | IN_DELETE_SELF);
        const bool isDirectoryEvent = event->mask & IN_ISDIR;

        if (renameEvent.isGood && event->cookie != renameEvent.cookie) {
          eventLoop->recordRenameNewEvent(event, isDirectoryEvent, renameEvent);
        } else if (event->mask & (IN_ATTRIB | IN_MODIFY)) {
          eventLoop->recordChangedEvent(event);
        } else if (event->mask & IN_CREATE) {
          eventLoop->recordCreatedEvent(event, isDirectoryEvent);
        } else if (event->mask & (IN_DELETE | IN_DELETE_SELF)) {
          eventLoop->recordDeletedEvent(event, isDirectoryRemoval);
        } else if (event->mask & IN_MOVED_TO) {
          if (event->cookie == 0) {
            eventLoop->recordCreatedEvent(event, isDirectoryEvent);
            continue;
          }
          eventLoop->recordRenameNewEvent(event, isDirectoryEvent, renameEvent);
        } else if (event->mask & IN_MOVED_FROM) {
          if (event->cookie == 0) {
            eventLoop->recordDeletedEvent(event, isDirectoryRemoval);
            continue;
          }
          eventLoop->recordRenameOldEvent(event, isDirectoryEvent, renameEvent);
        } else if (event->mask & IN_MOVE_SELF) {
          auto name = strdup(event->name);
          eventLoop->mInotifyService->emitEventDelete(event->wd, name);
          eventLoop->mInotifyService->emitEventDeleteDir(event->wd);
          free(name);
        }
      } while ((position += sizeof(inotify_event) + event->len) < bytesRead);

      if (eventLoop->mStopped) { break; }

      ssize_t bytesAvailable = 0;
      if (ioctl(eventLoop->mInotifyInstance, FIONREAD, &bytesAvailable) < 0) {
        continue;
      }
      if (bytesAvailable == 0) {
        /// bytesAvailable为0的情况说明了inotify事件队列中当前没有任何事件等待处理。
        /// 如果此时有挂起的重命名事件，需要进行相应的清理操作以避免信息丢失
        if (renameEvent.isGood) {
          if (renameEvent.isDirectory) {
            eventLoop->mInotifyService->emitEventDeleteDir(renameEvent.wd, renameEvent.name);
          }
          eventLoop->mInotifyService->emitEventDelete(renameEvent.wd, renameEvent.name);
        }
      }
    }

  pthread_cleanup_pop(1);
  return nullptr;
}

InotifyEventLooper::~InotifyEventLooper() {
  if (mStopped) { return; }
  mStopped = true;

  /// 将信号 32 设置为忽略，可以确保在取消线程时，线程不会因为信号处理而被中断。
  auto previousHandler = std::signal(32, SIG_IGN);

  auto errorCode = pthread_cancel(mEventLoopThread);
  if (errorCode != 0) {
    mInotifyService->sendError("cancel失败: " + std::to_string(errorCode));
    return;
  }

  errorCode = pthread_join(mEventLoopThread, NULL);
  if (errorCode != 0) {
    mInotifyService->sendError("join失败: " + std::to_string(errorCode));
  }

  /// 在成功取消和终止线程后，恢复信号 32 的原始处理程序。
  /// 这是为了确保在 InotifyEventLooper 对象的生命周期结束后，
  /// 信号处理恢复到之前的状态，以避免影响程序的其他部分。
  if (previousHandler != SIG_ERR) {
    std::signal(32, previousHandler);
  }
}
