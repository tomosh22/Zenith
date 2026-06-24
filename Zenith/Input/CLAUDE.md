# Input System

## Overview

Frame-based input handling wrapping GLFW on Windows. Instance class `Zenith_Input` (held on `g_xEngine`, accessed via `g_xEngine.Input()`) provides keyboard, mouse, and gamepad state queries (gamepad support is `#ifdef ZENITH_WINDOWS` only). Touch gestures (tap/swipe) are detected through mouse button events.

## Files

- `Zenith_Input.h` - Instance input class (held on `g_xEngine`, accessed via `g_xEngine.Input()`) with all query functions
- `Zenith_KeyCodes.h` - GLFW-compatible key code constants
- `Zenith_TouchInput.h` - Touch gesture subsystem (tap/swipe), held on `g_xEngine`, accessed via `g_xEngine.Touch()`
- `Zenith_InputSimulator.h` - Static test harness for injecting synthetic input and stepping frames
- `Zenith_Input.Tests.inl` - Unit test cases for the input subsystem

## Public API

### Keyboard
- `IsKeyDown(Zenith_KeyCode)` - Current key state (held)
- `WasKeyPressedThisFrame(Zenith_KeyCode)` - Edge detection (pressed this frame only)

### Mouse
- `GetMousePosition(Vector2_64&)` - Current position (output parameter)
- `GetMouseDelta(Vector2_64&)` - Movement since last frame
- `GetMouseWheelDelta()` - Wheel scroll delta this frame (sim-aware)
- `IsMouseButtonHeld(Zenith_KeyCode)` - Button state
- `UpdateMouseDeltaFromPosition(const Vector2_64&, bool bJustLeftSimMode)` - Pure (window-free) core of `BeginFrame`'s mouse-delta update; used by tests
- `NotifyMouseDiscontinuity()` - Flag a cursor-position jump (e.g. capture/release) so the next `BeginFrame` zeroes that frame's delta + resyncs the baseline (one-shot)

### Gamepad
- `IsGamepadConnected(int iGamepad = 0)` - Connection status
- `IsGamepadButtonDown(int iButton, int iGamepad = 0)` - Button state
- `WasGamepadButtonPressedThisFrame(int iButton, int iGamepad = 0)` - Edge detection
- `GetGamepadLeftStick/RightStick(float&, float&, int)` - Analog stick values
- `GetGamepadLeftTrigger/RightTrigger(int)` - Trigger values [0.0, 1.0]
- `GetGamepadAxis(int iAxis, int iGamepad = 0)` - Raw axis value

### Frame Management
- `BeginFrame()` - Must be called each frame to poll and update state

### Platform Callbacks
- `KeyPressedCallback(Zenith_KeyCode)` - Called by platform on key press
- `MouseButtonPressedCallback(Zenith_KeyCode)` - Called by platform on mouse button
- `MouseWheelCallback(double fXOffset, double fYOffset)` - Called by platform on mouse wheel scroll

## Touch Input (`Zenith_TouchInput`)

Instance subsystem held on `g_xEngine`, accessed via `g_xEngine.Touch()`. `Update()` runs each frame to detect gestures.
- `WasTapThisFrame()` / `GetTapPosition()` - Tap edge detection + position
- `WasSwipeThisFrame()` / `GetSwipeDirection()` - Swipe edge detection + direction (`ZENITH_SWIPE_UP/DOWN/LEFT/RIGHT`)
- `GetSwipeStartPosition()` / `GetSwipeDistance()` - Swipe start point + travel distance
- `IsTouchDown()` / `GetTouchPosition()` - Raw touch held state + position
- `SetSwipeThreshold/SetTapMaxMovement/SetTapMaxDuration(float)` - Gesture tuning

## Input Simulation (`Zenith_InputSimulator`)

Static test-only harness for injecting synthetic input and driving frames deterministically.
- `Enable()` / `Disable()` - Route queries through the simulator
- `StepFrame()` / `StepFrames(uCount)` / `StepFramesWithFixedDt(uCount, fDt)` / `StepUntil(pfnCondition, uMaxFrames)` - Frame stepping
- `SimulateKeyDown/Up/Press(eKey)`, `SimulateKeySequence(...)` - Keyboard injection
- `SimulateMousePosition/ButtonDown/ButtonUp/Click/Drag(...)`, `SimulateMouseWheel(fDelta)` - Mouse injection
- `SimulateTap(...)` / `SimulateSwipe(...)` - Touch gesture injection
- `SetKeyHeld/ClearHeldKeys/ResetAllInputState()`, `SetFixedDt/ClearFixedDt()` - State control

## Key Constants

### Keyboard (GLFW-compatible values)
| Category | Constants | Range |
|----------|-----------|-------|
| Letters | `ZENITH_KEY_A` - `ZENITH_KEY_Z` | 65-90 |
| Numbers | `ZENITH_KEY_0` - `ZENITH_KEY_9` | 48-57 |
| Function | `ZENITH_KEY_F1` - `ZENITH_KEY_F25` | 290-314 |
| Arrows | `ZENITH_KEY_UP/DOWN/LEFT/RIGHT` | 265/264/263/262 |
| Special | `ZENITH_KEY_ESCAPE(256)`, `ZENITH_KEY_ENTER(257)`, `ZENITH_KEY_TAB(258)`, `ZENITH_KEY_SPACE(32)` | - |
| Modifiers | `ZENITH_KEY_LEFT_SHIFT(340)`, `ZENITH_KEY_LEFT_CONTROL(341)`, `ZENITH_KEY_LEFT_ALT(342)` | - |

### Mouse Buttons
- `ZENITH_MOUSE_BUTTON_LEFT(0)`, `ZENITH_MOUSE_BUTTON_RIGHT(1)`, `ZENITH_MOUSE_BUTTON_MIDDLE(2)`, up to `ZENITH_MOUSE_BUTTON_8(7)`

### Gamepad Buttons
- `ZENITH_GAMEPAD_BUTTON_A(0)` through `ZENITH_GAMEPAD_BUTTON_DPAD_LEFT(14)` (`ZENITH_GAMEPAD_BUTTON_DPAD_LEFT` == `ZENITH_GAMEPAD_BUTTON_LAST`)
- PlayStation aliases: `ZENITH_GAMEPAD_BUTTON_CROSS=ZENITH_GAMEPAD_BUTTON_A`, `ZENITH_GAMEPAD_BUTTON_CIRCLE=ZENITH_GAMEPAD_BUTTON_B`, `ZENITH_GAMEPAD_BUTTON_SQUARE=ZENITH_GAMEPAD_BUTTON_X`, `ZENITH_GAMEPAD_BUTTON_TRIANGLE=ZENITH_GAMEPAD_BUTTON_Y`

### Gamepad Axes
- `ZENITH_GAMEPAD_AXIS_LEFT_X(0)` through `ZENITH_GAMEPAD_AXIS_RIGHT_TRIGGER(5)`

## Constants
- `GAMEPAD_DEADZONE = 0.15f` - Analog stick deadzone threshold

## Key Patterns

- Frame-based: call `BeginFrame()` each frame before querying
- Edge detection (`WasKeyPressedThisFrame`) vs state (`IsKeyDown`) distinction
- Platform layer calls callbacks; game code only reads state
- Vector results via output parameters (not return values)
- Sim-aware queries: when `Zenith_InputSimulator` is enabled (`ZENITH_INPUT_SIMULATOR`), `BeginFrame` and the query methods automatically route through the simulator instead of the live GLFW state
