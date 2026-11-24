#pragma once

#include "Engine/Audio/ProceduralAudio.h"

class BlackjackAudio {
public:
  void Initialize();
  void Shutdown();
  void PlayDeal() const;
  void PlayWin() const;
  void PlayLoss() const;

private:
  Engine::ProceduralSound deal_;
  Engine::ProceduralSound win_;
  Engine::ProceduralSound loss_;
};
