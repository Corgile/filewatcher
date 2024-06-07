// ReSharper disable CppRedundantQualifier
#include "fw/InotifyEventLoop.h"

#include <sys/ioctl.h>
#include <csignal>
#include <cstring>

InotifyEventLooper::InotifyEventLooper(const int inotifyInstance,
                                       const InotifyService::ptr inotifyService)
  : mInotifyService(inotifyService)
    , mInotifyInstance(inotifyInstance)
    , mRunning(true), mThreadStartedSemaphore(0) {
  mEventLoopThread = std::thread([this] { work(); });
  /// main loop
  mThreadStartedSemaphore.acquire();
}

bool InotifyEventLooper::isLooping() const { return mRunning; }

void InotifyEventLooper::recordCreatedEvent(const inotify_event* event,
                                            const bool isDirectoryEvent,
                                            const bool sendInitEvents) const {
  if (event == nullptr) { return; }

  if (isDirectoryEvent) {
    mInotifyService->emitEventCreateDir(event->wd, event->name, sendInitEvents);
  } else {
    mInotifyService->emitEventCreate(event->wd, event->name);
  }
}

void InotifyEventLooper::recordChangedEvent(const inotify_event* event) const {
  if (event == nullptr) { return; }
  mInotifyService->emitEventModify(event->wd, event->name);
}

void InotifyEventLooper::recordDeletedEvent(const inotify_event* event, const bool isDir) const {
  if (event == nullptr) { return; }

  if (isDir) {
    mInotifyService->emitEventDeleteDir(event->wd);
  } else {
    mInotifyService->emitEventDelete(event->wd, event->name);
  }
}

void InotifyEventLooper::recordRenameOldEvent(const inotify_event* event,
                                              const bool isDirectoryEvent,
                                              InotifyRenameEvent& renameEvent) const {
  renameEvent = InotifyRenameEvent(event, isDirectoryEvent);
}

void InotifyEventLooper::recordRenameNewEvent(const inotify_event* event,
                                              bool isDirectoryEvent,
                                              InotifyRenameEvent& renameEvent) const {
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

void InotifyEventLooper::work() {
  mThreadStartedSemaphore.release();
  while (mRunning) {
    constexpr int BUFFER_SIZE = 16384;
    InotifyRenameEvent renameEvent;
    char buffer[BUFFER_SIZE];
    const auto bytesRead = read(mInotifyInstance, &buffer, BUFFER_SIZE);
    HANDLE_ERROR_CODE(bytesRead == 0, "没有读取到事件， InotifyEventLooper 线程结束.", break);
    HANDLE_ERROR_CODE(bytesRead == -1, strerror(errno), break);
    ssize_t position = 0;
    while (position < bytesRead) {
      const auto* event = reinterpret_cast<inotify_event*>(buffer + position);
      handleEvent(event, renameEvent);
      position += sizeof(inotify_event) + event->len;
    }
    ssize_t bytesAvailable = 0;
    const auto erc = ioctl(mInotifyInstance, FIONREAD, &bytesAvailable);
    CONTINUE_LOOP_ON_CONDITION(erc < 0);
    CONTINUE_LOOP_ON_CONDITION(bytesAvailable != 0);
    /// bytesAvailable为0的情况说明了inotify事件队列中当前没有任何事件等待处理。
    /// 如果此时有挂起的重命名事件，需要进行相应的清理操作以避免信息丢失
    CONTINUE_LOOP_ON_CONDITION(not renameEvent.isGood);
    if (renameEvent.isDirectory) {
      mInotifyService->emitEventDeleteDir(renameEvent.wd, renameEvent.name);
    }
    mInotifyService->emitEventDelete(renameEvent.wd, renameEvent.name);
  }
}

InotifyEventLooper::~InotifyEventLooper() {
  if (not mRunning) { return; }
  mRunning = false;
  /// 将信号 32 设置为忽略，可以确保在取消线程时，线程不会因为信号处理而被中断。
  const auto previousHandler = std::signal(32, SIG_IGN);
  const auto errorCode = pthread_cancel(mEventLoopThread.native_handle());
  HANDLE_ERROR_CODE(errorCode != 0, "cancel失败: " + std::to_string(errorCode), return);
  if (mEventLoopThread.joinable()) mEventLoopThread.join();
  /// 在成功取消和终止线程后，恢复信号 32 的原始处理程序。
  /// 这是为了确保在 InotifyEventLooper 对象的生命周期结束后，
  /// 信号处理恢复到之前的状态，以避免影响程序的其他部分。
  if (previousHandler != SIG_ERR) {
    std::signal(32, previousHandler);
  }
}

void InotifyEventLooper::handleEvent(const inotify_event* event, InotifyRenameEvent& renameEvent) const {
  const bool isDirectoryRemoval = event->mask & (IN_IGNORED | IN_DELETE_SELF);
  const bool isDirectoryEvent = event->mask & IN_ISDIR;

  switch (event->mask & InotifyNode::ATTRIBUTES) {
  case IN_ATTRIB:
  case IN_MODIFY:
    recordChangedEvent(event);
    break;
  case IN_CREATE:
    recordCreatedEvent(event, isDirectoryEvent);
    break;
  case IN_DELETE:
  case IN_DELETE_SELF:
    recordDeletedEvent(event, isDirectoryRemoval);
    break;
  case IN_MOVED_TO:
    if (event->cookie == 0) {
      recordCreatedEvent(event, isDirectoryEvent);
    } else recordRenameNewEvent(event, isDirectoryEvent, renameEvent);
    break;
  case IN_MOVED_FROM:
    if (event->cookie == 0) {
      recordDeletedEvent(event, isDirectoryRemoval);
    } else recordRenameOldEvent(event, isDirectoryEvent, renameEvent);
    break;
  case IN_MOVE_SELF:
    mInotifyService->emitEventDelete(event->wd, event->name);
    mInotifyService->emitEventDeleteDir(event->wd);
    break;
  default:
    if (renameEvent.isGood && event->cookie != renameEvent.cookie) {
      recordRenameNewEvent(event, isDirectoryEvent, renameEvent);
    }
    break;
  }
}
