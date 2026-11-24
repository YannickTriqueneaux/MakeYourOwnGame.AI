#pragma once

#include "Engine/Application/GameInstance.h"
#include "Engine/Application/GameInterface.h"
#include "Engine/Graphics/Camera3D.h"

#include "Game/BlackjackAudio.h"
#include "Game/BlackjackTable.h"

class ProceduralGame final : public Engine::GameInterface {
public:
  ProceduralGame();
  void Initialize() override;
  void Update(const Engine::InputSystem &input, float deltaTime) override;
  [[nodiscard]] Engine::Color GetClearColor() const override;
  void Render(Engine::RenderContext &context) const override;
  void Shutdown() override;
  [[nodiscard]] std::vector<std::byte> SaveResumeState() const override;
  void ResumeFromState(std::span<const std::byte> state) override;

private:
  void RegisterGameplayTypes();
  void EnsureTable();
  Engine::GameInstance gameInstance_;
  Engine::Gameplay::World &world_;
  Engine::Gameplay::ObjectRef<Blackjack::Table> table_;
  Engine::Camera3D camera_;
  BlackjackAudio audio_;
};
