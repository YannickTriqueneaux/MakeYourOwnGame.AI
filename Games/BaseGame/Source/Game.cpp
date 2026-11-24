#include "Game/Game.h"

#include "Game/GameConfig.h"

#include "Engine/Core/Profile.h"
#include "Engine/Gameplay/World.h"
#include "Engine/Graphics/RenderContext.h"
#include "Engine/Graphics/Renderer2D.h"
#include "Engine/Graphics/Renderer3D.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Input/Key.h"

#include <algorithm>
#include <array>
#include <string>

namespace {
using Engine::Color;
using Engine::Vector2;
using Engine::Vector3;
using Blackjack::Card;

constexpr Color Gold{238, 195, 101, 255};
constexpr Color Cream{244, 239, 221, 255};
constexpr Color Muted{185, 207, 190, 255};
constexpr Color Red{190, 58, 55, 255};
constexpr Color Ink{27, 35, 33, 255};
constexpr Color Felt{18, 91, 68, 255};
constexpr Color CardBack{36, 75, 57, 255};
constexpr float CardSpacing = 1.0f;
constexpr float CardZ = 1.45f;
constexpr float DealerZ = -1.10f;
constexpr float PlayerZ = 1.10f;

Vector2 ProjectToScreen(Vector3 point, int width, int height) {
  constexpr float halfSqrtTwo = 0.70710678f;
  constexpr float isoHorizontal = 0.40824829f;
  constexpr float isoVertical = 0.81649658f;
  const float pixelsPerUnit = static_cast<float>(height) / 12.5f;
  const float horizontal = (point.x - point.z) * halfSqrtTwo;
  const float vertical = -isoHorizontal * point.x + isoVertical * point.y -
                         isoHorizontal * point.z;
  return {static_cast<float>(width) * 0.5f + horizontal * pixelsPerUnit,
          static_cast<float>(height) * 0.5f - vertical * pixelsPerUnit};
}

std::string CardRank(std::uint8_t rank) {
  if (rank == 1) return "A";
  if (rank == 11) return "J";
  if (rank == 12) return "Q";
  if (rank == 13) return "K";
  return std::to_string(rank);
}

char CardSuit(std::uint8_t suit) {
  constexpr std::array<char, 4> suits{'C', 'D', 'H', 'S'};
  return suits[std::min<std::size_t>(suit, suits.size() - 1)];
}

std::string OutcomeText(Blackjack::Result result) {
  switch (result) {
  case Blackjack::Result::PlayerWin: return "YOU WIN";
  case Blackjack::Result::DealerWin: return "DEALER WINS";
  case Blackjack::Result::Push: return "PUSH - BET RETURNED";
  case Blackjack::Result::Blackjack: return "BLACKJACK!  3:2 PAYOUT";
  case Blackjack::Result::Bust: return "BUST - DEALER WINS";
  case Blackjack::Result::None: return {};
  }
  return {};
}

void DrawCardOverlay(Engine::Renderer2D &renderer, float centerX, float centerY,
                     Card card, bool hidden) {
  if (hidden) {
    renderer.DrawText("?", {centerX - 10.0f, centerY - 16.0f}, 30, Gold);
    return;
  }
  const Color ink = (card.suit == 1 || card.suit == 2) ? Red : Ink;
  renderer.DrawText(CardRank(card.rank), {centerX - 24.0f, centerY - 15.0f}, 22, ink);
  renderer.DrawText(std::string(1, CardSuit(card.suit)),
                    {centerX + 2.0f, centerY - 15.0f}, 22, ink);
}
} // namespace

ProceduralGame::ProceduralGame() : world_(*gameInstance_.GetWorld()) {
  camera_.position = {12.0f, 12.0f, 12.0f};
  camera_.target = {0.0f, 0.0f, 0.0f};
  camera_.up = {0.0f, 1.0f, 0.0f};
  camera_.fieldOfView = 12.5f;
  camera_.projection = Engine::CameraProjection::Orthographic;
  RegisterGameplayTypes();
  EnsureTable();
}

void ProceduralGame::RegisterGameplayTypes() {
  world_.RegisterComponent(
      Engine::Gameplay::MakeComponentType<Blackjack::Table>("BlackjackTable"));
  world_.RegisterEntity(
      {Blackjack::TableEntityType, "BlackjackTable", {Blackjack::Table::Type}});
}

void ProceduralGame::EnsureTable() {
  for (const auto id : world_.Entities()) {
    Engine::Gameplay::ObjectRef<Engine::Gameplay::Entity> entity(id);
    auto *resolved = entity.Resolve();
    if (resolved && resolved->GetTypeID() == Blackjack::TableEntityType) {
      table_ = resolved->GetComponent<Blackjack::Table>();
      return;
    }
  }
  const auto entity = world_.Spawn(Blackjack::TableEntityType, "BlackjackTable");
  world_.FlushSpawns();
  if (auto *spawned = entity.Resolve())
    table_ = spawned->GetComponent<Blackjack::Table>();
}

void ProceduralGame::Initialize() {
  audio_.Initialize();
  audio_.PlayDeal();
}

void ProceduralGame::Update(const Engine::InputSystem &input, float deltaTime) {
  ENGINE_PROFILE_SCOPE("Blackjack Update");
  world_.Update(deltaTime);
  auto *table = table_.Resolve();
  if (!table)
    return;
  const auto oldState = table->State();
  const auto oldResult = table->Outcome();
  if (input.WasPressed(Engine::Key::Q)) {
    table->ResetBankroll();
    audio_.PlayDeal();
  } else if (input.WasPressed(Engine::Key::R)) {
    table->StartRound();
    if (table->State() == Blackjack::RoundState::PlayerTurn)
      audio_.PlayDeal();
  } else if (input.WasPressed(Engine::Key::W) &&
             table->State() == Blackjack::RoundState::PlayerTurn) {
    table->Hit();
    if (table->State() == Blackjack::RoundState::RoundOver)
      audio_.PlayLoss();
    else
      audio_.PlayDeal();
  } else if (input.WasPressed(Engine::Key::S)) {
    table->Stand();
  }
  if (oldState == Blackjack::RoundState::PlayerTurn &&
      table->State() == Blackjack::RoundState::RoundOver &&
      table->Outcome() != Blackjack::Result::Bust &&
      table->Outcome() != Blackjack::Result::DealerWin &&
      table->Outcome() != oldResult)
    audio_.PlayWin();
  else if (oldState == Blackjack::RoundState::PlayerTurn &&
           table->State() == Blackjack::RoundState::RoundOver &&
           table->Outcome() == Blackjack::Result::DealerWin)
    audio_.PlayLoss();
}

Engine::Color ProceduralGame::GetClearColor() const {
  return GameConfig::BackgroundColor;
}

void ProceduralGame::Render(Engine::RenderContext &context) const {
  ENGINE_PROFILE_SCOPE("Blackjack Render");
  const auto *table = table_.Resolve();
  if (!table)
    return;

  auto &scene = context.Draw3D();
  scene.Begin(camera_);
  scene.DrawCube({0.0f, -0.20f, 0.0f}, {17.0f, 0.38f, 13.5f}, Felt);
  scene.DrawCubeOutline({0.0f, 0.005f, 0.0f}, {16.5f, 0.08f, 13.0f}, Gold);
  scene.DrawLine({-7.7f, 0.055f, 0.0f}, {7.7f, 0.055f, 0.0f},
                 {100, 154, 119, 255});

  const float dealerStart = -0.5f * static_cast<float>(table->DealerCount() - 1) * CardSpacing;
  for (int i = 0; i < table->DealerCount(); ++i) {
    const float offset = dealerStart + static_cast<float>(i) * CardSpacing;
    const float x = DealerZ + offset;
    const float z = DealerZ - offset;
    const bool hidden = table->DealerCardHidden() && i == 1;
    scene.DrawCube({x, 0.06f, z}, {0.98f, 0.10f, CardZ},
                   hidden ? CardBack : Cream);
    scene.DrawCubeOutline({x, 0.12f, z}, {1.00f, 0.025f, CardZ + 0.02f},
                          hidden ? Gold : Color{220, 215, 195, 255});
    if (hidden) {
      for (float stripe = -0.55f; stripe <= 0.55f; stripe += 0.28f)
        scene.DrawLine({x - 0.42f, 0.13f, z + stripe},
                       {x + 0.42f, 0.13f, z + stripe + 0.22f}, Gold);
    }
  }

  const float playerStart = -0.5f * static_cast<float>(table->PlayerCount() - 1) * CardSpacing;
  for (int i = 0; i < table->PlayerCount(); ++i) {
    const float offset = playerStart + static_cast<float>(i) * CardSpacing;
    const float x = PlayerZ + offset;
    const float z = PlayerZ - offset;
    scene.DrawCube({x, 0.06f, z}, {0.98f, 0.10f, CardZ}, Cream);
    scene.DrawCubeOutline({x, 0.12f, z}, {1.00f, 0.025f, CardZ + 0.02f},
                          {220, 215, 195, 255});
  }
  scene.End();

  auto &ui = context.Draw2D();
  const float screenCenterX = static_cast<float>(context.GetWidth()) * 0.5f;
  const float screenCenterY = static_cast<float>(context.GetHeight()) * 0.5f;
  for (int i = 0; i < table->DealerCount(); ++i) {
    const float offset = dealerStart + static_cast<float>(i) * CardSpacing;
    const Vector2 screen = ProjectToScreen(
        {DealerZ + offset, 0.14f, DealerZ - offset},
        context.GetWidth(), context.GetHeight());
    DrawCardOverlay(ui, screen.x, screen.y,
                    table->DealerCards()[static_cast<std::size_t>(i)],
                    table->DealerCardHidden() && i == 1);
  }
  for (int i = 0; i < table->PlayerCount(); ++i) {
    const float offset = playerStart + static_cast<float>(i) * CardSpacing;
    const Vector2 screen = ProjectToScreen(
        {PlayerZ + offset, 0.14f, PlayerZ - offset},
        context.GetWidth(), context.GetHeight());
    DrawCardOverlay(ui, screen.x, screen.y,
                    table->PlayerCards()[static_cast<std::size_t>(i)], false);
  }

  ui.DrawText("BLACKJACK", {36.0f, 24.0f}, 30, Gold);
  ui.DrawText("3D TABLE  /  DEALER STANDS ON 17", {38.0f, 58.0f}, 16, Muted);
  ui.DrawText("DEALER", {38.0f, 215.0f}, 18, Cream);
  if (!table->DealerCardHidden())
    ui.DrawText("TOTAL " + std::to_string(table->DealerTotal()),
                {38.0f, 240.0f}, 16, Cream);
  else
    ui.DrawText("SHOWING " + CardRank(table->DealerCards()[0].rank),
                {38.0f, 240.0f}, 16, Cream);

  const std::string result = OutcomeText(table->Outcome());
  if (!result.empty())
    ui.DrawText(result, {screenCenterX - 125.0f, screenCenterY - 11.0f}, 20,
                table->Outcome() == Blackjack::Result::DealerWin ||
                        table->Outcome() == Blackjack::Result::Bust
                    ? Color{242, 137, 122, 255}
                    : Gold);

  ui.DrawText("PLAYER  /  TOTAL " + std::to_string(table->PlayerTotal()),
              {38.0f, static_cast<float>(context.GetHeight()) - 232.0f}, 18, Cream);
  ui.DrawText("CHIPS  $" + std::to_string(table->Bankroll()) +
                  "    BET  $" + std::to_string(table->Bet()) +
                  "    HANDS  " + std::to_string(table->RoundsPlayed()),
              {38.0f, static_cast<float>(context.GetHeight()) - 74.0f}, 18, Gold);
  if (table->State() == Blackjack::RoundState::PlayerTurn) {
    ui.DrawText("W HIT    S STAND", {38.0f, static_cast<float>(context.GetHeight()) - 43.0f}, 17, Cream);
    ui.DrawText("R NEXT HAND    Q RESET", {250.0f, static_cast<float>(context.GetHeight()) - 43.0f}, 15, Muted);
  } else if (table->Bankroll() < table->Bet()) {
    ui.DrawText("OUT OF CHIPS - PRESS Q TO START OVER",
                {38.0f, static_cast<float>(context.GetHeight()) - 43.0f}, 17, Cream);
  } else {
    ui.DrawText("PRESS R TO DEAL AGAIN    Q RESET BANKROLL",
                {38.0f, static_cast<float>(context.GetHeight()) - 43.0f}, 17, Cream);
  }
}

void ProceduralGame::Shutdown() { audio_.Shutdown(); }

std::vector<std::byte> ProceduralGame::SaveResumeState() const {
  return world_.Save();
}

void ProceduralGame::ResumeFromState(std::span<const std::byte> state) {
  world_.Resume(state);
  table_ = {};
  EnsureTable();
}
