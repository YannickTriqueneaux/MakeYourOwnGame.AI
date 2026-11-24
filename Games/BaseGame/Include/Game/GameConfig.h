#pragma once

#include "Engine/Application/GameModule.h"
#include "Engine/Graphics/Color.h"

namespace GameConfig {
inline constexpr int PlayAreaWidth = 1280;
inline constexpr int PlayAreaHeight = 720;
inline constexpr int TargetFramesPerSecond = 144;
inline constexpr const char *WindowTitle = "Blackjack";

inline Engine::GameApplicationConfig CreateApplicationConfig() {
  return {.windowWidth = PlayAreaWidth,
          .windowHeight = PlayAreaHeight,
          .targetFramesPerSecond = TargetFramesPerSecond,
          .windowTitle = WindowTitle,
          .verticalSync = true};
}

inline constexpr Engine::Color BackgroundColor{12, 58, 47, 255};
} // namespace GameConfig
