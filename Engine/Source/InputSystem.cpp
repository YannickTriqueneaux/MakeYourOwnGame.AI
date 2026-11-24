#include "Engine/Input/InputSystem.h"
#include "Engine/Platform/WindowFocus.h"

#include "raylib.h"

namespace {
int ToRaylibKey(Engine::Key key) {
  switch (key) {
  case Engine::Key::Up:
    return KEY_UP;
  case Engine::Key::Down:
    return KEY_DOWN;
  case Engine::Key::Left:
    return KEY_LEFT;
  case Engine::Key::Right:
    return KEY_RIGHT;
  case Engine::Key::Q:
    return KEY_Q;
  case Engine::Key::W:
    return KEY_W;
  case Engine::Key::E:
    return KEY_E;
  case Engine::Key::R:
    return KEY_R;
  case Engine::Key::Escape:
    return KEY_ESCAPE;
  case Engine::Key::A:
    return KEY_A;
  case Engine::Key::S:
    return KEY_S;
  case Engine::Key::D:
    return KEY_D;
  }

  return KEY_NULL;
}
} // namespace

namespace Engine {
void InputSystem::Update() {
  // raylib updates its input state while polling window events.
  discardMouseDelta_ = false;
}

void InputSystem::SetGameplayInputEnabled(bool enabled) {
  if (gameplayInputEnabled_ != enabled)
    discardMouseDelta_ = true;
  gameplayInputEnabled_ = enabled;
  SetMouseCaptured(mouseCaptureRequested_);
  if (!enabled) {
#if defined(ENGINE_AUTOTESTS)
    if (autoTestInput_)
      return;
#endif
    WindowFocus::ReleaseMouseCapture();
  }
}

void InputSystem::SetMouseCaptured(bool captured) const {
  mouseCaptureRequested_ = captured;
#if defined(ENGINE_AUTOTESTS)
  if (autoTestInput_)
    return;
#endif
  if (!IsWindowReady())
    return;
  if (captured && gameplayInputEnabled_ && IsWindowFocused()) {
    if (!IsCursorHidden()) {
      DisableCursor();
      discardMouseDelta_ = true;
    }
  } else if (IsCursorHidden()) {
    EnableCursor();
    discardMouseDelta_ = true;
  }
}

bool InputSystem::IsMouseDown(MouseButton button) const {
  if (!gameplayInputEnabled_)
    return false;
#if defined(ENGINE_AUTOTESTS)
  if (autoTestInput_)
    return false;
#endif
  if (!IsWindowFocused())
    return false;
  switch (button) {
  case MouseButton::Left:
    return IsMouseButtonDown(MOUSE_BUTTON_LEFT);
  case MouseButton::Right:
    return IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
  case MouseButton::Middle:
    return IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);
  }
  return false;
}

Vector2 InputSystem::GetMouseDelta() const {
  if (!gameplayInputEnabled_ || discardMouseDelta_)
    return {};
#if defined(ENGINE_AUTOTESTS)
  if (autoTestInput_)
    return {};
#endif
  if (!IsWindowFocused())
    return {};
  const auto delta = ::GetMouseDelta();
  return {delta.x, delta.y};
}

bool InputSystem::IsDown(Key key) const {
  if (!gameplayInputEnabled_)
    return false;
#if defined(ENGINE_AUTOTESTS)
  if (autoTestInput_) {
    return autoTestDown_[static_cast<std::size_t>(key)];
  }
#endif
  return IsWindowFocused() && IsKeyDown(ToRaylibKey(key));
}

bool InputSystem::WasPressed(Key key) const {
  if (!gameplayInputEnabled_)
    return false;
#if defined(ENGINE_AUTOTESTS)
  if (autoTestInput_) {
    return autoTestPressed_[static_cast<std::size_t>(key)];
  }
#endif
  return IsWindowFocused() && IsKeyPressed(ToRaylibKey(key));
}

#if defined(ENGINE_AUTOTESTS)
void InputSystem::EnableAutoTestInput() { autoTestInput_ = true; }

void InputSystem::SetAutoTestKeyDown(Key key, bool down) {
  autoTestInput_ = true;
  autoTestDown_[static_cast<std::size_t>(key)] = down;
}

void InputSystem::SetAutoTestKeyPressed(Key key, bool pressed) {
  autoTestInput_ = true;
  autoTestPressed_[static_cast<std::size_t>(key)] = pressed;
}

void InputSystem::ClearAutoTestPressedKeys() { autoTestPressed_.fill(false); }
#endif
} // namespace Engine
