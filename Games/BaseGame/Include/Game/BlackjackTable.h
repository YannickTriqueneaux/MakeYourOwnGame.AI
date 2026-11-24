#pragma once

#include "Engine/Gameplay/Object.h"

#include <array>
#include <cstdint>

namespace Blackjack {
using Engine::Gameplay::ObjectRef;
using Engine::Gameplay::StateReader;
using Engine::Gameplay::StateWriter;
using Engine::Gameplay::TypeID;

inline constexpr TypeID TableEntityType =
    Engine::Gameplay::StableTypeID("Blackjack.TableEntity.v1");

struct Card {
  std::uint8_t rank = 1; // Ace through King: 1..13
  std::uint8_t suit = 0; // clubs, diamonds, hearts, spades
};

enum class RoundState : std::uint8_t { PlayerTurn, DealerTurn, RoundOver };
enum class Result : std::uint8_t { None, PlayerWin, DealerWin, Push, Blackjack, Bust };

class Table final : public Engine::Gameplay::Component {
public:
  static constexpr TypeID Type = Engine::Gameplay::StableTypeID("Blackjack.Table.v1");
  explicit Table(ObjectRef<Engine::Gameplay::Entity> owner);
  [[nodiscard]] TypeID GetTypeID() const override { return Type; }
  [[nodiscard]] std::uint32_t CurrentStateVersion() const override { return 1; }
  [[nodiscard]] std::uint32_t MinimumStateVersion() const override { return 1; }

  void StartRound();
  void Hit();
  void Stand();
  void ResetBankroll();
  [[nodiscard]] const std::array<Card, 12> &PlayerCards() const { return player_; }
  [[nodiscard]] const std::array<Card, 12> &DealerCards() const { return dealer_; }
  [[nodiscard]] int PlayerCount() const { return playerCount_; }
  [[nodiscard]] int DealerCount() const { return dealerCount_; }
  [[nodiscard]] int PlayerTotal() const;
  [[nodiscard]] int DealerTotal() const;
  [[nodiscard]] int Bankroll() const { return bankroll_; }
  [[nodiscard]] int Bet() const { return bet_; }
  [[nodiscard]] int RoundsPlayed() const { return roundsPlayed_; }
  [[nodiscard]] RoundState State() const { return state_; }
  [[nodiscard]] Result Outcome() const { return result_; }
  [[nodiscard]] bool DealerCardHidden() const { return state_ == RoundState::PlayerTurn; }
  void SaveState(StateWriter &writer) const override;
  void LoadState(StateReader &reader, std::uint32_t version) override;

private:
  [[nodiscard]] std::uint32_t Random();
  void Shuffle();
  [[nodiscard]] Card Draw();
  void Finish(Result result, int payout);
  std::array<Card, 52> deck_{};
  std::array<Card, 12> player_{};
  std::array<Card, 12> dealer_{};
  std::uint32_t randomState_ = 0xB1AC4ACu;
  int deckIndex_ = 0;
  int playerCount_ = 0;
  int dealerCount_ = 0;
  int bankroll_ = 100;
  int bet_ = 10;
  int roundsPlayed_ = 0;
  RoundState state_ = RoundState::RoundOver;
  Result result_ = Result::None;
};
} // namespace Blackjack
