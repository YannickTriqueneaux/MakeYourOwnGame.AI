#include "Game/BlackjackAudio.h"

#include <array>

namespace {
constexpr std::array DealVoices{Engine::ProceduralSoundVoice{
    .waveform = Engine::AudioWaveform::Triangle,
    .frequencyHz = 620.0f,
    .volume = 0.38f,
    .envelope = {.attackSeconds = 0.002f, .decaySeconds = 0.025f,
                 .sustainLevel = 0.0f, .releaseSeconds = 0.025f}}};
constexpr Engine::ProceduralSoundDefinition DealPatch{
    .durationSeconds = 0.055f, .masterVolume = 0.35f, .voices = DealVoices};
constexpr std::array WinVoices{Engine::ProceduralSoundVoice{
    .waveform = Engine::AudioWaveform::Sine,
    .frequencyHz = 784.0f,
    .volume = 0.45f,
    .envelope = {.attackSeconds = 0.008f, .decaySeconds = 0.08f,
                 .sustainLevel = 0.25f, .releaseSeconds = 0.12f}}};
constexpr Engine::ProceduralSoundDefinition WinPatch{
    .durationSeconds = 0.22f, .masterVolume = 0.45f, .voices = WinVoices};
constexpr std::array LossVoices{Engine::ProceduralSoundVoice{
    .waveform = Engine::AudioWaveform::Triangle,
    .frequencyHz = 196.0f,
    .volume = 0.4f,
    .envelope = {.attackSeconds = 0.01f, .decaySeconds = 0.12f,
                 .sustainLevel = 0.15f, .releaseSeconds = 0.14f}}};
constexpr Engine::ProceduralSoundDefinition LossPatch{
    .durationSeconds = 0.28f, .masterVolume = 0.4f, .voices = LossVoices};
}

void BlackjackAudio::Initialize() {
  static_cast<void>(deal_.Build(DealPatch));
  static_cast<void>(win_.Build(WinPatch));
  static_cast<void>(loss_.Build(LossPatch));
  static_cast<void>(deal_.Upload());
  static_cast<void>(win_.Upload());
  static_cast<void>(loss_.Upload());
}
void BlackjackAudio::Shutdown() {
  loss_.Unload();
  win_.Unload();
  deal_.Unload();
}
void BlackjackAudio::PlayDeal() const { deal_.Play(0.42f, 1.0f, 0.5f); }
void BlackjackAudio::PlayWin() const { win_.Play(0.55f, 1.0f, 0.5f); }
void BlackjackAudio::PlayLoss() const { loss_.Play(0.45f, 1.0f, 0.5f); }
