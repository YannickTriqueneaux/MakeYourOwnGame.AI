#pragma once

#include "Engine/Core/Export.h"
#include "Engine/Input/Key.h"
#include "Engine/Input/MouseButton.h"
#include "Engine/Math/Vector2.h"

#if defined(ENGINE_AUTOTESTS)
#include <array>
#endif

namespace Engine {
class ENGINE_API InputSystem {
public:
  void Update();
  void SetGameplayInputEnabled(bool enabled);

  [[nodiscard]] bool IsDown(Key key) const;
  [[nodiscard]] bool WasPressed(Key key) const;
  [[nodiscard]] bool IsMouseDown(MouseButton button) const;
  [[nodiscard]] Vector2 GetMouseDelta() const;
  // A request: Application can suspend capture when TAB releases the mouse.
  void SetMouseCaptured(bool captured) const;
#if defined(ENGINE_AUTOTESTS)
  void EnableAutoTestInput();
  void SetAutoTestKeyDown(Key key, bool down);
  void SetAutoTestKeyPressed(Key key, bool pressed);
  void ClearAutoTestPressedKeys();
#endif

private:
  bool gameplayInputEnabled_ = true;
  mutable bool mouseCaptureRequested_ = false;
  mutable bool discardMouseDelta_ = true;
#if defined(ENGINE_AUTOTESTS)
  static constexpr std::size_t KeyCount = 12;
  bool autoTestInput_ = false;
  std::array<bool, KeyCount> autoTestDown_{};
  std::array<bool, KeyCount> autoTestPressed_{};
#endif
};
} // namespace Engine
