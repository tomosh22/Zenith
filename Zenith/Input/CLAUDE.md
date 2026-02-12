# Input System

## Overview

Frame-based input handling wrapping GLFW (Windows) and touch events (Android). Static class `Zenith_Input` provides keyboard, mouse, and gamepad state queries.

## Files

- `Zenith_Input.h` - Static input class with all query functions
- `Zenith_KeyCodes.h` - GLFW-compatible key code constants

## Public API

### Keyboard
- `IsKeyDown(Zenith_KeyCode)` / `IsKeyHeld(Zenith_KeyCode)` - Current key state (held)
- `WasKeyPressedThisFrame(Zenith_KeyCode)` - Edge detection (pressed this frame only)

### Mouse
- `GetMousePosition(Vector2_64&)` - Current position (output parameter)
- `GetMouseDelta(Vector2_64&)` - Movement since last frame
- `IsMouseButtonHeld(Zenith_KeyCode)` - Button state

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

## Key Constants

### Keyboard (GLFW-compatible values)
| Category | Constants | Range |
|----------|-----------|-------|
| Letters | `ZENITH_KEY_A` - `ZENITH_KEY_Z` | 65-90 |
| Numbers | `ZENITH_KEY_0` - `ZENITH_KEY_9` | 48-57 |
| Function | `ZENITH_KEY_F1` - `ZENITH_KEY_25` | 290-314 |
| Arrows | `ZENITH_KEY_UP/DOWN/LEFT/RIGHT` | 265/264/263/262 |
| Special | `ZENITH_KEY_ESCAPE(256)`, `ENTER(257)`, `TAB(258)`, `SPACE(32)` | - |
| Modifiers | `ZENITH_KEY_LEFT_SHIFT(340)`, `LEFT_CONTROL(341)`, `LEFT_ALT(342)` | - |

### Mouse Buttons
- `ZENITH_MOUSE_BUTTON_LEFT(0)`, `RIGHT(1)`, `MIDDLE(2)`, up to `BUTTON_8(7)`

### Gamepad Buttons
- `ZENITH_GAMEPAD_BUTTON_A(0)` through `DPAD_RIGHT(14)`
- PlayStation aliases: `CROSS=A`, `CIRCLE=B`, `SQUARE=X`, `TRIANGLE=Y`

### Gamepad Axes
- `ZENITH_GAMEPAD_AXIS_LEFT_X(0)` through `RIGHT_TRIGGER(5)`

## Constants
- `GAMEPAD_DEADZONE = 0.15f` - Analog stick deadzone threshold

## Key Patterns

- Frame-based: call `BeginFrame()` each frame before querying
- Edge detection (`WasKeyPressedThisFrame`) vs state (`IsKeyDown`) distinction
- Platform layer calls callbacks; game code only reads state
- Vector results via output parameters (not return values)
- Android touch emulates `ZENITH_MOUSE_BUTTON_1` for compatibility
