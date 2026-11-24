#pragma once
#include "Engine/Application/GameInstance.h"
#include "Engine/Application/GameInterface.h"
#include "Engine/Audio/ProceduralAudio.h"
#include "Game/GameInput.h"
#include "Game/FpsComponents.h"

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
#if defined(ENGINE_AUTOTESTS)
  void SerializeAutoTestState(Engine::Serializer &serializer) override;
#endif
private:
  void RegisterTypes();
  void EnsurePlayer();
  void SpawnBot();
  void SpawnBullet(Engine::Vector2 origin, Engine::Vector2 direction, float height, float verticalSpeed, FpsGame::Faction faction);
  void ProcessWorld();
  Engine::GameInstance gameInstance_;
  GameInput inputBindings_;
  Engine::Gameplay::World &world_;
  Engine::Gameplay::ObjectRef<Engine::Gameplay::Entity> player_;
  Engine::Gameplay::ObjectRef<FpsGame::PlayerController> controller_;
  Engine::Gameplay::ObjectRef<FpsGame::Vitality> playerVitality_;
  float botSpawnTimer_ = 2.0f;
  Engine::ProceduralSound rifleSound_;
  Engine::ProceduralSound impactSound_;
};
