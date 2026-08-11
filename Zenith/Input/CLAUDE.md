# Input System

## Overview

Frame-based input handling wrapping GLFW on Windows. Instance class `Zenith_Input` (held on `g_xEngine`, accessed via `g_xEngine.Input()`) provides keyboard, mouse, and gamepad state queries (gamepad support is `#ifdef ZENITH_WINDOWS` only). Multi-touch lives in the separate pointer table `Zenith_Pointers` (`g_xEngine.Pointers()`).

## Files

- `Zenith_Input.h` - Instance input class (held on `g_xEngine`, accessed via `g_xEngine.Input()`) with all query functions
- `Zenith_KeyCodes.h` - GLFW-compatible key code constants
- `Zenith_Pointers.h` - The pointer table (8 slots, generation handles, claims, taps), held on `g_xEngine`, accessed via `g_xEngine.Pointers()`
- `Zenith_InputSimulator.h` - Static test harness for injecting synthetic input and stepping frames
- `Zenith_Input.Tests.inl` - Unit test cases for the input subsystem
- `Zenith_Pointers.Tests.inl` - Unit test cases for the pointer table (LOCAL instances; never the engine's)

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

## Pointers (`Zenith_Pointers`)

The pointer table. Instance class held on `g_xEngine`, accessed via `g_xEngine.Pointers()`;
a plain class, so unit tests construct LOCAL instances rather than poking the engine's.
Replaced the old single-touch `Zenith_TouchInput` gesture subsystem outright (swipe
detection is gone — it had no consumers).

- 8 slots. A pointer is a finger, or — on a pointing-device platform — the mouse:
  **the mouse feeds pointer 0, and on Android the FIRST touch feeds the mouse view.**
  That projection is permanent; it is what keeps every `IsMouseButtonHeld` consumer
  working on a touch device. Secondary fingers exist only in this table.
- Handles are `(slot, generation)`. A slot recycles the frame AFTER its pointer ended,
  with the generation bumped, so a stale handle is detectable — and a stale-generation
  claim release is rejected.
- **Coordinates are RAW SURFACE PIXELS.** Canvas / editor-viewport remapping belongs to
  the UI layer (`Zenith_UIElement::TransformSurfacePosition`).
- `ApplyPlatform()` (frame step 4) consumes the drain's staged touch stream + the mouse
  projection; `ApplyInjection()` (step 7) consumes what the automated-test Steps added
  after step 4. Window-free cores (`BeginFrame` / `ApplyEvent` / `ApplyMouseState` /
  `ConsumeStagedTail`) are public so units can drive a frame without a window.
- Claims: `ClaimPointer` / `ReleaseClaim` / `IsClaimed` / `ReleaseAllClaimsForOwner`.
  FIRST claim wins. A claim survives the terminal UP/CANCEL frame so its owner can see
  the edge. Owner tokens are `u_int64`; UI widgets use their own address.
- `WasTapThisFrame()` = an UNCLAIMED pointer, down ≤ 0.3 s, max excursion ≤ 15 ×
  `GetDisplayScale()` px. A claimed pointer is a consumed one and never taps.
- A `LIFECYCLE_RESET` barrier cancels every live pointer (raising CANCEL edges) and
  disarms the table until the device layer re-arms.

## Input Simulation (`Zenith_InputSimulator`)

Static test-only harness for injecting synthetic input and driving frames deterministically.
- `Enable()` / `Disable()` - Route queries through the simulator
- `StepFrame()` / `StepFrames(uCount)` / `StepFramesWithFixedDt(uCount, fDt)` / `StepUntil(pfnCondition, uMaxFrames)` - Frame stepping
- `SimulateKeyDown/Up/Press(eKey)`, `SimulateKeySequence(...)` - Keyboard injection
- `SimulateMousePosition/ButtonDown/ButtonUp/Click/Drag(...)`, `SimulateMouseWheel(fDelta)` - Mouse injection
- `SimulateTouchDown/Move/Up/Cancel(iPointerId, x, y)` - Raw pointer-stream injection (applied at frame step 7)
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
