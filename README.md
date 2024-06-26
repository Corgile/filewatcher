# file watcher

```cpp

#include <bitset>
#include <filesystem>
#include <iostream>
#include <memory>
#include "fw/Filter.h"
#include <fw/InotifyService.h>
using namespace std::chrono_literals;

int main(int argc, char* argv[]) {
  const auto path = fs::path(argv[1]);
  std::cout << "监听： '" << path.string() << "'" << std::endl;
  CallBackSignatur _call_back = [](const std::vector<Event::uptr>& events) {
    for (const auto& event : events) {
      std::cout << std::bitset<16>(event->type)
        << "; " << translate(event->type)
        << ": " << event->relativePath.string() << "\n";
    }
  };
  const auto _filter = std::make_shared<Filter>(_call_back);
  auto listenerInstance = InotifyService(_filter, path, 1ms);
  std::cout << "任意键退出" << std::endl;
  std::cin.ignore();

  return 0;
}

```

![image](https://github.com/Corgile/filewatcher/assets/48396880/d990c7d5-a301-4e02-8902-85a54016de7d)



