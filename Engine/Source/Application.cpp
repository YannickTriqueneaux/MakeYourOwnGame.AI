#include "Engine/Application/Application.h"

#include "Engine/Application/GameInterface.h"
#include "Engine/Core/Profile.h"
#include "Engine/Graphics/RenderContext.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Platform/WindowFocus.h"
#include "Engine/UI/UiSystem.h"

#include "raylib.h"

#include <utility>

namespace {
void ReleaseGameCursor() {
  // EnableCursor recenters the mouse; do not call it every background frame.
  if (IsCursorHidden())
    EnableCursor();
  Engine::WindowFocus::ReleaseMouseCapture();
}
} // namespace

namespace Engine {
Application::Application(ApplicationConfig config)
    : config_(std::move(config)) {
#if defined(NDEBUG)
  if (config_.verticalSync) {
    SetConfigFlags(FLAG_VSYNC_HINT);
  }
#endif

  InitWindow(config_.windowWidth, config_.windowHeight,
             config_.windowTitle.c_str());
  InitAudioDevice();

#if defined(NDEBUG)
  SetTargetFPS(config_.targetFramesPerSecond);
#endif
}

Application::~Application() {
  if (IsAudioDeviceReady())
    CloseAudioDevice();
  CloseWindow();
}

void Application::Run(GameInterface &game) {
  InputSystem input;
  RenderContext renderContext;
  UiSystem ui;

  game.Initialize();

  bool cursorReleased = false;
  bool restoreCursorCapture = false;
  bool observedFocusLoss = false;

  while (!WindowShouldClose()) {
    if (!cursorReleased && !IsWindowFocused()) {
      restoreCursorCapture = IsCursorHidden();
      cursorReleased = true;
      observedFocusLoss = true;
    }
    if (cursorReleased) {
      if (!IsWindowFocused())
        observedFocusLoss = true;
      else if (observedFocusLoss) {
        cursorReleased = false;
        if (restoreCursorCapture)
          DisableCursor();
      }
    }

    if (IsKeyPressed(KEY_TAB) && !config_.focusWindowTitle.empty()) {
      if (cursorReleased && IsWindowFocused()) {
        // If the partner window is unavailable, TAB can return control to
        // the game without requiring a focus change.
        cursorReleased = false;
        if (restoreCursorCapture)
          DisableCursor();
      } else {
        restoreCursorCapture = IsCursorHidden();
        cursorReleased = true;
        observedFocusLoss = false;
        ReleaseGameCursor();
        static_cast<void>(
            WindowFocus::FocusWindowByTitle(config_.focusWindowTitle));
      }
    }

    // A background game must not retain the desktop mouse, including after
    // callbacks in a Game DLL request capture again.
    const bool keepCursorFree = cursorReleased || !IsWindowFocused();
    if (keepCursorFree)
      ReleaseGameCursor();

    input.Update();
    input.SetGameplayInputEnabled(!keepCursorFree);
    {
      ENGINE_PROFILE_SCOPE("Game Update");
      if (!keepCursorFree)
        game.Update(input, GetFrameTime());
      else if (config_.backgroundUpdate)
        config_.backgroundUpdate();
    }
    if (keepCursorFree)
      ReleaseGameCursor();

    {
      ENGINE_PROFILE_SCOPE("Game Render");
      renderContext.BeginFrame(game.GetClearColor());
      game.Render(renderContext);

      ui.BeginFrame();
      if (!keepCursorFree)
        game.RenderUi(ui);
      ui.EndFrame();

      if (keepCursorFree)
        ReleaseGameCursor();
      renderContext.EndFrame();
    }
    ENGINE_PROFILE_FRAME();
  }

  game.Shutdown();
}
} // namespace Engine
