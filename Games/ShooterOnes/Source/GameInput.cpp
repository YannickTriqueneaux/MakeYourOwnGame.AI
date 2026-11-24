#include "Game/GameInput.h"

#include "Engine/Input/InputSystem.h"

#include <cmath>

PlayerCommand
GameInput::BuildPlayerCommand(const Engine::InputSystem &input) const {
  input.SetMouseCaptured(true);
  PlayerCommand command;
  command.movement = {
      float(input.IsDown(Engine::Key::A) || input.IsDown(Engine::Key::Q)) -
          float(input.IsDown(Engine::Key::D) || input.IsDown(Engine::Key::E)),
      float(input.IsDown(Engine::Key::W)) -
          float(input.IsDown(Engine::Key::S))};
  const float length =
      std::sqrt(command.movement.x * command.movement.x +
                command.movement.y * command.movement.y);
  if (length > 1) {
    command.movement.x /= length;
    command.movement.y /= length;
  }
  command.firing = input.IsMouseDown(Engine::MouseButton::Left);
  const auto mouseDelta = input.GetMouseDelta();
  command.yawDelta = -mouseDelta.x * 0.0022f;
  command.pitchDelta = -mouseDelta.y * 0.0022f;
  return command;
}
