#ifndef PFW_FILTER_H
#define PFW_FILTER_H

#include <memory>
#include <string>
#include <vector>

#include "fw/Event.h"
#include "fw/Listener.h"

using CallBackSignatur = std::function<void(std::vector<Event::uptr>&&)>;

class Filter : public Listener<CallBackSignatur> {
public:
  using sptr = std::shared_ptr<Filter>;
  Filter(const CallBackSignatur& callBack);
  ~Filter();

  void sendError(const std::string& errorMsg);
  void filterAndNotify(std::vector<Event::uptr>&& events);

private:
  CallbackHandle mCallbackHandle;
};

#endif
