#include "Game/BlackjackTable.h"

#include <algorithm>
#include <stdexcept>

namespace Blackjack {
namespace {
void RequireVersion(std::uint32_t version) {
  if (version != 1)
    throw std::runtime_error("Unsupported Blackjack table state version.");
}
} // namespace

Table::Table(ObjectRef<Engine::Gameplay::Entity> owner)
    : Component(owner) {
  Shuffle();
  StartRound();
}

std::uint32_t Table::Random() {
  randomState_ ^= randomState_ << 13u;
  randomState_ ^= randomState_ >> 17u;
  randomState_ ^= randomState_ << 5u;
  return randomState_;
}

void Table::Shuffle() {
  int index = 0;
  for (std::uint8_t suit = 0; suit < 4; ++suit)
    for (std::uint8_t rank = 1; rank <= 13; ++rank)
      deck_[static_cast<std::size_t>(index++)] = {rank, suit};
  for (int i = 51; i > 0; --i) {
    const int other = static_cast<int>(Random() % static_cast<std::uint32_t>(i + 1));
    std::swap(deck_[static_cast<std::size_t>(i)], deck_[static_cast<std::size_t>(other)]);
  }
  deckIndex_ = 0;
}

Card Table::Draw() {
  if (deckIndex_ >= static_cast<int>(deck_.size()))
    Shuffle();
  return deck_[static_cast<std::size_t>(deckIndex_++)];
}

int Table::PlayerTotal() const {
  int total = 0;
  int aces = 0;
  for (int i = 0; i < playerCount_; ++i) {
    const int rank = player_[static_cast<std::size_t>(i)].rank;
    total += rank == 1 ? 11 : std::min(rank, 10);
    aces += rank == 1;
  }
  while (total > 21 && aces-- > 0)
    total -= 10;
  return total;
}

int Table::DealerTotal() const {
  int total = 0;
  int aces = 0;
  for (int i = 0; i < dealerCount_; ++i) {
    const int rank = dealer_[static_cast<std::size_t>(i)].rank;
    total += rank == 1 ? 11 : std::min(rank, 10);
    aces += rank == 1;
  }
  while (total > 21 && aces-- > 0)
    total -= 10;
  return total;
}

void Table::StartRound() {
  if (state_ == RoundState::PlayerTurn || bankroll_ < bet_)
    return;
  if (deckIndex_ > 40)
    Shuffle();
  playerCount_ = dealerCount_ = 0;
  result_ = Result::None;
  bankroll_ -= bet_;
  player_[static_cast<std::size_t>(playerCount_++)] = Draw();
  dealer_[static_cast<std::size_t>(dealerCount_++)] = Draw();
  player_[static_cast<std::size_t>(playerCount_++)] = Draw();
  dealer_[static_cast<std::size_t>(dealerCount_++)] = Draw();
  state_ = RoundState::PlayerTurn;
  if (PlayerTotal() == 21) {
    if (DealerTotal() == 21)
      Finish(Result::Push, bet_);
    else
      Finish(Result::Blackjack, bet_ + bet_ * 3 / 2);
  }
}

void Table::Hit() {
  if (state_ != RoundState::PlayerTurn || playerCount_ >= static_cast<int>(player_.size()))
    return;
  player_[static_cast<std::size_t>(playerCount_++)] = Draw();
  if (PlayerTotal() > 21)
    Finish(Result::Bust, 0);
}

void Table::Stand() {
  if (state_ != RoundState::PlayerTurn)
    return;
  state_ = RoundState::DealerTurn;
  while (DealerTotal() < 17 && dealerCount_ < static_cast<int>(dealer_.size()))
    dealer_[static_cast<std::size_t>(dealerCount_++)] = Draw();
  const int playerTotal = PlayerTotal();
  const int dealerTotal = DealerTotal();
  if (dealerTotal > 21)
    Finish(Result::PlayerWin, bet_ * 2);
  else if (playerTotal > dealerTotal)
    Finish(Result::PlayerWin, bet_ * 2);
  else if (playerTotal == dealerTotal)
    Finish(Result::Push, bet_);
  else
    Finish(Result::DealerWin, 0);
}

void Table::Finish(Result result, int payout) {
  result_ = result;
  state_ = RoundState::RoundOver;
  bankroll_ += payout;
  ++roundsPlayed_;
}

void Table::ResetBankroll() {
  bankroll_ = 100;
  roundsPlayed_ = 0;
  state_ = RoundState::RoundOver;
  result_ = Result::None;
  StartRound();
}

void Table::SaveState(StateWriter &writer) const {
  writer.Value(deck_);
  writer.Value(player_);
  writer.Value(dealer_);
  writer.Value(randomState_);
  writer.Value(deckIndex_);
  writer.Value(playerCount_);
  writer.Value(dealerCount_);
  writer.Value(bankroll_);
  writer.Value(bet_);
  writer.Value(roundsPlayed_);
  writer.Value(state_);
  writer.Value(result_);
}

void Table::LoadState(StateReader &reader, std::uint32_t version) {
  RequireVersion(version);
  deck_ = reader.Value<std::array<Card, 52>>();
  player_ = reader.Value<std::array<Card, 12>>();
  dealer_ = reader.Value<std::array<Card, 12>>();
  randomState_ = reader.Value<std::uint32_t>();
  deckIndex_ = reader.Value<int>();
  playerCount_ = reader.Value<int>();
  dealerCount_ = reader.Value<int>();
  bankroll_ = reader.Value<int>();
  bet_ = reader.Value<int>();
  roundsPlayed_ = reader.Value<int>();
  state_ = reader.Value<RoundState>();
  result_ = reader.Value<Result>();
  if (deckIndex_ < 0 || deckIndex_ > 52 || playerCount_ < 0 || playerCount_ > 12 ||
      dealerCount_ < 0 || dealerCount_ > 12 || bankroll_ < 0 || bet_ <= 0)
    throw std::runtime_error("Invalid Blackjack table state.");
}
} // namespace Blackjack
