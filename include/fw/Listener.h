#ifndef PFW_LISTENER_H
#define PFW_LISTENER_H

#include <functional>

template <typename CallbackType>
concept CallbackConcept = requires(CallbackType callback)
{
  { callback(std::vector<std::unique_ptr<Event>>{}) } -> std::convertible_to<void>;
};

template <CallbackConcept CallbackType>
class Listener {
public:
  using CallbackHandle = int;

private:
  std::map<CallbackHandle, CallbackType> mListeners;
  std::mutex mListenersMutex;
  int mHandleCount{0};

public:
  CallbackHandle registerCallback(const CallbackType callback) {
    std::lock_guard lock(mListenersMutex);
    mListeners[++mHandleCount] = callback;
    return mHandleCount;
  }

  void deRegisterCallback(const CallbackHandle& id) {
    std::lock_guard lock(mListenersMutex);
    auto it = mListeners.find(id);
    if (it != mListeners.end()) {
      mListeners.erase(it);
    }
  }

protected:
  template <typename ...Args>
  void notify(Args&& ...args) {
    std::lock_guard lock(mListenersMutex);
    for (const auto& func : mListeners) {
      func.second(std::move(std::forward<Args>(args)) ...);
    }
  }
};

#endif
