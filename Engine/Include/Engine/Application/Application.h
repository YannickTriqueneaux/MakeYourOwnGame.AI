#pragma once

#include "Engine/Core/Export.h"

#include <functional>
#include <string>

namespace Engine {
class GameInterface;

struct ApplicationConfig {
  int windowWidth = 1280;
  int windowHeight = 720;
  int targetFramesPerSecond = 144;
  std::string windowTitle = "Engine Application";
  std::string focusWindowTitle;
  bool verticalSync = true;
  // Host maintenance continues while gameplay input and updates are suspended.
  std::function<void()> backgroundUpdate;
};

class ENGINE_API Application {
public:
  explicit Application(ApplicationConfig config);
  ~Application();

  Application(const Application &) = delete;
  Application &operator=(const Application &) = delete;

  void Run(GameInterface &game);

private:
  ApplicationConfig config_;
};
} // namespace Engine
