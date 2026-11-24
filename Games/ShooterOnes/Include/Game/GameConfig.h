#pragma once
#include "Engine/Application/GameModule.h"
#include "Engine/Graphics/Color.h"
namespace GameConfig {
inline constexpr int PlayAreaWidth=1280, PlayAreaHeight=720, TargetFramesPerSecond=144;
inline constexpr const char *WindowTitle="BALLISTIC//BREACH";
inline Engine::GameApplicationConfig CreateApplicationConfig(){return {.windowWidth=PlayAreaWidth,.windowHeight=PlayAreaHeight,.targetFramesPerSecond=TargetFramesPerSecond,.windowTitle=WindowTitle,.verticalSync=true};}
inline constexpr Engine::Color BackgroundColor{8,12,20,255};
}
