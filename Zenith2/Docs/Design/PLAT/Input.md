# PLAT design: input

This file is part of the PLAT design family. The entry document is `Zenith2/Docs/Design/PLAT.md`. It specifies input devices, controllers and hotplug, touch, pen and gestures, text input and IME, haptics, actions, bindings, input contexts, the action mapper, the `*.bindings.json` format, rebinding, and how input enters simulation as replayable commands in the Input phase. The SPEC baselines are `Zenith2/Docs/Architecture/PlatformLayer.md` → "SDL3 responsibilities", `Zenith2/Docs/Architecture/FramePhases.md` → "Phase-specific rules" (phase 1) and `Zenith2/Docs/Architecture.md` → "Multiplayer foundations" (the replay log's input commands).

## Scope of this file

| Topic | Heading here |
|---|---|
| layers, XR readiness, frame and tick flow | "Input model overview" |
| devices, control paths, the transition log | "Devices and controls" |
| keyboard and mouse specifics | "Keyboard and mouse" |
| gamepads, mapping database, hotplug, reconnection | "Controllers" |
| touch and pen | "Touch and pen" |
| gesture recognisers and their state tables | "Gestures" |
| text input and IME | "Text input and IME" |
| rumble, trigger rumble, device vibration | "Haptics" |
| the input system object | "Zenith_InputSystem API" |
| action types, IDs and state | "Actions" |
| composites, modifiers, processors, interactions, schemes | "Bindings" |
| context stacks, consumption, claims | "Input contexts" |
| the deterministic mapping algorithm | "The action mapper" |
| the bindings file | "The bindings file format" |
| interactive rebinding and persistence | "Rebinding" |
| phase 1, commands, replay, scripted input, latency | `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Simulation input" |
| the Luau surface | `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Script-facing input API" |

The requirement each heading specifies is listed in `Zenith2/Docs/Design/PLAT.md` → "Requirement index".

## Input model overview

### Layers

| Layer | Type | Answers | Consumers |
|---|---|---|---|
| devices | `Zenith_InputDevices` | "which physical controls changed, in what order, when" | the mapper, the gesture recognisers, UI text editing, the editor, the latency harness |
| gestures | `Zenith_GestureRecogniserSet` | "which touch gestures were recognised" (as controls of the synthetic gesture device) | the mapper |
| actions | `Zenith_InputSystem` and `Zenith_ActionMapper` | "is Gameplay.Jump pressed this tick", "what is Gameplay.Move" | simulation (through phase 1), UI, the editor |
| commands | `Zenith_InputCommand` | "what did the player do in tick T", replayable | simulation, the replay log, the network loopback |

A game never reads a device. Its scripts read actions (`Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Script-facing input API"); its bindings files, which are data, are the one place a device control is named (§ "The bindings file format").

<!-- informative -->
Zenith's input stack had the same three layers (device, pointers, actions) and an ordered transition log drained after the swapchain gate, with UI claims resolved before gameplay actions were finalised. Zenith2 keeps those properties and adds what Zenith lacked: stable action IDs, contexts pushed by simulation commands, a bindings file instead of a per-game C++ header, the presentation/simulation split that makes input replayable, and control paths shaped after OpenXR's so that XR input is a new device kind rather than a new model.
<!-- /informative -->

### XR-ready design

Input is kept XR-ready now and XR input lands in Stage 7 through OpenXR (Z2-D-072; the SCOPE area owns the Stage 7 tickets, `Zenith2/Docs/Design/SCOPE.md` (pending; its "Public API" section)). What that means for the Stage 3 design:

| XR concept (OpenXR) | Zenith2 equivalent, present from Stage 3 |
|---|---|
| action (`XrAction`) with a type (boolean, float, vector2f, pose) | `Zenith_InputActionID` with `Zenith_InputActionType` (`BUTTON`, `AXIS1D`, `AXIS2D`, `POSE` reserved) |
| action set, attached and activated | input context, pushed on a context stack |
| interaction profile with suggested bindings | a control scheme with the bindings of a `*.bindings.json` |
| input path (`/user/hand/left/input/trigger/value`) | control path (`/xr/left/trigger`), in the reserved `/xr/` namespace |
| subaction path (left or right hand) | the device index segment of a control path |
| haptic output action | `Zenith_HapticEffect` played on a device |

- Gameplay code, UI and scripts refer to actions and contexts alone and never to a device kind, so adding the `INPUT_DEVICE_KIND_XR_CONTROLLER` kind and its `/xr/` paths in Stage 7 changes no gameplay code, no action and no context.
- `INPUT_ACTION_TYPE_POSE` is declared now and rejected by the Stage 3 bindings loader with `INPUT_ERROR_UNSUPPORTED_ACTION_TYPE`; its command encoding is reserved (`Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Input commands and quantisation"), so enabling it adds no replay-format change.
- The XR runtime (OpenXR loader) will be a device source of `Zenith_InputDevices` like SDL; the mapper, contexts and commands are unchanged.
- Two additive changes, neither a redesign, land with it in Stage 7: `Zenith_InputValue` (three floats today) widens to hold a POSE state (position and orientation quaternion), which its contract allows because action state is never serialized and "the layout may change in any build"; and the platform-layer bindings file gains an optional `interactionProfile` key per binding set (the OpenXR interaction-profile path, such as `/interaction_profiles/khr/simple_controller`), finer than the `schemes` mask. Both are catalogued as Stage 7 requirements.

### Frame and tick flow

```text
prologue step 1, Platform.Pump (main thread)
   SDL drain ──► transition log (+ pointer-down claims) ──► gestures ──► coalesce ──► presentation mapper ──► action claims
																						│
																	Zenith_InputFrame (UI, editor, dev tools; never logged)
TICK subgraph, per world, per tick
   phase 1  Input.Actions@W<tag>: simulation mapper over the frame's unclaimed transitions (tick 1 of the frame)
			──► input commands (replay-logged) ──► action state rebuilt from the commands ──► world resources
EPILOGUE, per world
   Input.Haptics@W<tag>: haptic requests played back at sync points ──► the haptic player's submission queue
```

- The presentation mapper and the simulation mapper are the same code (`Zenith_ActionMapper`, § "The action mapper") with different context stacks.
- Simulation observes input only as input commands (`Zenith2/Docs/Architecture.md` → "Multiplayer foundations"), and the simulation's action state is rebuilt from those commands in live runs and in replays alike, so a replay reproduces a live run exactly (Z2-D-019).

## Devices and controls

### Device kinds and IDs

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_InputTypes.h

enum Zenith_InputDeviceKind : uint8
{
	INPUT_DEVICE_KIND_NONE = 0,
	INPUT_DEVICE_KIND_KEYBOARD = 1,        // every physical keyboard is merged into one logical keyboard
	INPUT_DEVICE_KIND_MOUSE,               // every physical mouse is merged into one logical mouse
	INPUT_DEVICE_KIND_GAMEPAD,             // one logical device per connected gamepad, at most 8
	INPUT_DEVICE_KIND_TOUCHSCREEN,         // one per touch surface SDL reports
	INPUT_DEVICE_KIND_PEN,                 // one per pen SDL reports
	INPUT_DEVICE_KIND_VIRTUAL,             // registered by UI (on-screen controls), tests and the input script
	INPUT_DEVICE_KIND_GESTURE,             // the synthetic device whose controls are recognised gestures
	INPUT_DEVICE_KIND_XR_CONTROLLER,       // reserved for Stage 7 (OpenXR); never created before then
	INPUT_DEVICE_KIND_COUNT,
};

/// @contract value Zenith_InputDeviceID
/// Ownership: Plain value naming one logical input device of one Zenith_InputDevices.
/// Thread affinity: IMMUTABLE.
///   Members non-const: a handle is copy-assigned and stored in containers and in other types' members.
/// Invalidation: Stale when the device is removed and its reconnection window has expired (see "Reconnection");
///   a device that reconnects inside the window keeps its ID.
/// Units: None.
/// Errors: None; lookups of a stale ID return INPUT_ERROR_DEVICE_NOT_FOUND.
/// Version: since Z2 Stage 3. Never serialized; saved bindings name device kinds and paths, never IDs.
/// @endcontract
struct Zenith_InputDeviceID
{
	uint32 m_uSlot = 0;
	uint32 m_uGeneration = 0;
	constexpr bool IsNull() const { return m_uGeneration == 0; }
	friend constexpr bool operator==(Zenith_InputDeviceID, Zenith_InputDeviceID) = default;
};

/// @contract value Zenith_InputControl
/// Ownership: Plain value naming one control of a device kind.
/// Thread affinity: IMMUTABLE. Members non-const: a key copy-assigned into binding records and transitions.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3. m_uControl values of a kind keep their meaning once shipped (they are SDL3's numbering
///   where the table "Control catalogue" says so).
/// @endcontract
struct Zenith_InputControl
{
	Zenith_InputDeviceKind m_eDeviceKind = INPUT_DEVICE_KIND_NONE;
	uint8 m_uIndex = 0;                   // touch index 0-9, 0xFF = primary touch; gamepad or virtual device ordinal; else 0
	uint16 m_uControl = 0;                // per-kind control value (table "Control catalogue")
	friend constexpr bool operator==(Zenith_InputControl, Zenith_InputControl) = default;
};
static_assert(sizeof(Zenith_InputControl) == 4);

inline constexpr uint32 uINPUT_MAX_GAMEPADS = 8;
inline constexpr uint32 uINPUT_MAX_TOUCHES = 10;
inline constexpr uint32 uINPUT_MAX_VIRTUAL_DEVICES = 16;
inline constexpr uint32 uINPUT_MAX_DEVICES = 64;       // the device slot table's hard cap
inline constexpr uint8 uINPUT_TOUCH_INDEX_PRIMARY = 0xFF;
```

Devices live in a `Zenith_SlotTable<Zenith_InputDeviceRecord>` with `m_uMaxSlots = uINPUT_MAX_DEVICES` (`Zenith2/Docs/Design/CORE/Memory.md` → "Zenith_SlotTable"). The logical keyboard, the logical mouse and the gesture device exist from creation in windowed mode; gamepads, touchscreens and pens come and go with hotplug; virtual devices are created by their owners.

### Control paths

Control paths are the text form of a control in bindings files, logs, the debug overlay and rebinding results.

```text
path       := "/" device-seg [ "/" index-seg ] "/" control-seg [ "/" component-seg ]
device-seg := "keyboard" | "mouse" | "gamepad" | "touch" | "pen" | "gesture" | "virtual" | "xr"
index-seg  := touch: "primary" | "0" … "9"      gamepad: omitted (any gamepad)      virtual: <device name>
			  xr (Stage 7): "left" | "right" | "head"
control-seg:= the control's path name in the table "Control catalogue" (lower camel case)
component  := "x" | "y" | "z"                    one axis of an AXIS2D or AXIS3D control, as AXIS1D
			| "up" | "down" | "left" | "right"   one half-axis of an AXIS2D control, as a button (value ≥ 0.5 is pressed)
```

- A path is ASCII, at most 96 bytes, case-sensitive, and parsed by `Zenith_ParseControlPath(Zenith_StringView) → Zenith_Result<Zenith_ControlPath, Zenith_InputError>`, which returns `INPUT_ERROR_UNKNOWN_CONTROL_PATH` for any segment not in the grammar or the catalogue; `Zenith_FormatControlPath` is its exact inverse (round-trip tested).
- A gamepad path without an index binds every connected gamepad: local multiplayer is excluded (Z2-D-076), so every device belongs to player slot 0.
- `/xr/…` paths parse (so Stage 3 files with them are rejected with a precise message) and resolve to no control until Stage 7: a binding naming one fails validation with `INPUT_ERROR_UNSUPPORTED_DEVICE_KIND`.

### Control catalogue

<!-- elaboration -->
| Device kind | `m_uControl` values | Path names | Value type | Range and units |
|---|---|---|---|---|
| keyboard | `Zenith_InputKey`: the SDL3 `SDL_Scancode` values 0–511 of the pinned SDL3 release, enumerator names `INPUT_KEY_<X>` for `SDL_SCANCODE_<X>` | `/keyboard/<name>`: the SDL scancode name in lower camel case (`a`, `space`, `leftShift`, `f1`, `keypad5`, `acBack`) | button | 0 or 1; physical position, layout independent |
| mouse | `MOUSE_CONTROL_LEFT` 1, `_RIGHT` 2, `_MIDDLE` 3, `_X1` 4, `_X2` 5, `_POSITION` 16, `_DELTA` 17, `_WHEEL` 18 | `left`, `right`, `middle`, `x1`, `x2`, `position`, `delta`, `wheel` | buttons; `position` AXIS2D; `delta` DELTA2D; `wheel` DELTA2D | position in window points, y down; delta in points (raw, unaccelerated in relative mode), y up; wheel in notches, y up = away from the user |
| gamepad buttons | `Zenith_GamepadButton` = `SDL_GamepadButton` + 1 (`GAMEPAD_BUTTON_SOUTH` 1 … `GAMEPAD_BUTTON_MISC6` 26) | `south`, `east`, `west`, `north`, `back`, `guide`, `start`, `leftStickPress`, `rightStickPress`, `leftShoulder`, `rightShoulder`, `dpadUp`, `dpadDown`, `dpadLeft`, `dpadRight`, `misc1`, `rightPaddle1`, `leftPaddle1`, `rightPaddle2`, `leftPaddle2`, `touchpadPress`, `misc2` … `misc6` | button | 0 or 1; positional (south is the bottom face button on every controller) |
| gamepad axes | `GAMEPAD_CONTROL_LEFT_STICK` 64, `_RIGHT_STICK` 65, `_LEFT_TRIGGER` 66, `_RIGHT_TRIGGER` 67, `_DPAD` 68, `_GYRO` 69, `_ACCELEROMETER` 70, `_TOUCHPAD` 71 | `leftStick`, `rightStick`, `leftTrigger`, `rightTrigger`, `dpad`, `gyro`, `accelerometer`, `touchpad` | sticks and dpad AXIS2D; triggers AXIS1D; gyro and accelerometer AXIS3D; touchpad AXIS2D (first finger) | sticks [-1, 1] per axis, y up (SDL's y is negated), raw (no deadzone: deadzones are processors); triggers [0, 1]; gyro rad/s; accelerometer m/s²; touchpad [0, 1] |
| touch | `TOUCH_CONTROL_PRESS` 1, `_POSITION` 2, `_DELTA` 3, `_PRESSURE` 4 (index segment 0–9 or `primary`) | `press`, `position`, `delta`, `pressure` | press button; position AXIS2D; delta DELTA2D; pressure AXIS1D | position and delta in window pixels, y down; pressure [0, 1] |
| pen | `PEN_CONTROL_TIP` 1, `_ERASER` 2, `_BARREL` 3, `_BUTTON2` 4, `_POSITION` 5, `_PRESSURE` 6, `_TILT` 7 | `tip`, `eraser`, `barrel`, `button2`, `position`, `pressure`, `tilt` | buttons; position AXIS2D; pressure AXIS1D; tilt AXIS2D | position in window pixels; pressure [0, 1]; tilt in degrees [-90, 90] |
| gesture | `GESTURE_CONTROL_TAP` 1, `_DOUBLE_TAP` 2, `_LONG_PRESS` 3, `_TWO_FINGER_TAP` 4, `_SWIPE_LEFT` 5, `_SWIPE_RIGHT` 6, `_SWIPE_UP` 7, `_SWIPE_DOWN` 8, `_PAN` 16, `_PINCH` 17, `_ROTATE` 18, `_POSITION` 19 | `tap`, `doubleTap`, `longPress`, `twoFingerTap`, `swipeLeft`, `swipeRight`, `swipeUp`, `swipeDown`, `pan`, `pinch`, `rotate`, `position` | taps and swipes pulse buttons (pressed and released at the same timestamp); long press a held button; pan DELTA2D; pinch and rotate AXIS1D deltas; position AXIS2D | pan in dp; pinch as a scale ratio minus 1 per transition; rotate in radians, counter-clockwise positive; position in pixels |
| virtual | declared by the virtual device's registration (`Zenith_VirtualDeviceDesc`), at most 32 controls | `/virtual/<device>/<control>` | as declared | as declared |
| xr | reserved for Stage 7 | `/xr/<left\|right\|head>/<control>` | — | — |

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_InputTypes.h
enum Zenith_InputKey : uint16            // one enumerator per SDL_Scancode of the pinned SDL3 release, same value
{
	INPUT_KEY_UNKNOWN = 0,
	INPUT_KEY_A = 4, INPUT_KEY_B = 5,          // through INPUT_KEY_Z = 29, then INPUT_KEY_1 = 30 … INPUT_KEY_0 = 39
	INPUT_KEY_RETURN = 40, INPUT_KEY_ESCAPE = 41, INPUT_KEY_BACKSPACE = 42, INPUT_KEY_TAB = 43, INPUT_KEY_SPACE = 44,
	// every other SDL_SCANCODE_<X> as INPUT_KEY_<X> with SDL's value (INPUT_KEY_F1 = 58, INPUT_KEY_LEFT_SHIFT = 225,
	// INPUT_KEY_AC_BACK = 282, …); names follow SDL's with words separated by underscores
	INPUT_KEY_COUNT = 512,
};

enum Zenith_GamepadButton : uint8        // SDL_GamepadButton + 1, so 0 can mean "none"
{
	GAMEPAD_BUTTON_NONE = 0,
	GAMEPAD_BUTTON_SOUTH, GAMEPAD_BUTTON_EAST, GAMEPAD_BUTTON_WEST, GAMEPAD_BUTTON_NORTH,
	GAMEPAD_BUTTON_BACK, GAMEPAD_BUTTON_GUIDE, GAMEPAD_BUTTON_START,
	GAMEPAD_BUTTON_LEFT_STICK, GAMEPAD_BUTTON_RIGHT_STICK, GAMEPAD_BUTTON_LEFT_SHOULDER, GAMEPAD_BUTTON_RIGHT_SHOULDER,
	GAMEPAD_BUTTON_DPAD_UP, GAMEPAD_BUTTON_DPAD_DOWN, GAMEPAD_BUTTON_DPAD_LEFT, GAMEPAD_BUTTON_DPAD_RIGHT,
	GAMEPAD_BUTTON_MISC1, GAMEPAD_BUTTON_RIGHT_PADDLE1, GAMEPAD_BUTTON_LEFT_PADDLE1, GAMEPAD_BUTTON_RIGHT_PADDLE2,
	GAMEPAD_BUTTON_LEFT_PADDLE2, GAMEPAD_BUTTON_TOUCHPAD, GAMEPAD_BUTTON_MISC2, GAMEPAD_BUTTON_MISC3,
	GAMEPAD_BUTTON_MISC4, GAMEPAD_BUTTON_MISC5, GAMEPAD_BUTTON_MISC6,    // = 26
};
```

- The SDL-numbered enumerations are checked by `static_assert`s in `Internal/Zenith_InputSdlMapping.cpp` (one per enumerator, comparing against the SDL constant), so a pinned SDL3 upgrade that renumbers fails to compile rather than remapping silently.

### Zenith_InputDevices API

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_InputDevices.h

enum Zenith_GamepadType : uint8
{
	GAMEPAD_TYPE_UNKNOWN = 0, GAMEPAD_TYPE_STANDARD, GAMEPAD_TYPE_XBOX360, GAMEPAD_TYPE_XBOXONE,
	GAMEPAD_TYPE_PS3, GAMEPAD_TYPE_PS4, GAMEPAD_TYPE_PS5, GAMEPAD_TYPE_SWITCH_PRO,
	GAMEPAD_TYPE_SWITCH_JOYCON_LEFT, GAMEPAD_TYPE_SWITCH_JOYCON_RIGHT, GAMEPAD_TYPE_SWITCH_JOYCON_PAIR,
};

struct Zenith_InputDeviceInfo
{
	Zenith_InputDeviceID m_xID;
	Zenith_InputDeviceKind m_eKind = INPUT_DEVICE_KIND_NONE;
	Zenith_GamepadType m_eGamepadType = GAMEPAD_TYPE_UNKNOWN;   // gamepads only; selects the glyph family
	bool m_bHasRumble = false;
	bool m_bHasTriggerRumble = false;
	bool m_bHasGyro = false;
	bool m_bHasAccelerometer = false;
	bool m_bVirtual = false;
	uint8 m_uOrdinal = 0;                  // gamepads: connection order 0-7; virtual devices: registration order
	uint8 m_auGuid[16] = {};               // SDL_GUID of gamepads; zero otherwise
	Zenith_StringView m_strName;            // SDL_GetGamepadName or the virtual device's name; valid until the next Pump
};

enum Zenith_InputTransitionFlag : uint8
{
	INPUT_TRANSITION_FLAG_SYNTHETIC_RELEASE = 1u << 0,   // generated by focus loss, disconnect or claim release
	INPUT_TRANSITION_FLAG_REPEAT = 1u << 1,              // an OS key repeat: exposed to UI text editing, ignored by the mapper
	INPUT_TRANSITION_FLAG_INJECTED = 1u << 2,            // from a virtual device, the input script or a test
	INPUT_TRANSITION_FLAG_LATENCY_PROBE = 1u << 3,       // from the MP-LATENCY harness
	INPUT_TRANSITION_FLAG_CLAIMED = 1u << 4,             // its pointer was claimed (see "Presentation claims")
	INPUT_TRANSITION_FLAG_COALESCED = 1u << 5,           // merged motion (see "Transition log")
	INPUT_TRANSITION_FLAG_CONSUMED = 1u << 6,            // consumed by the presentation stack or a claim (see "Consumption rules")
};

/// @contract value Zenith_InputTransition
/// Ownership: Plain value in the transition log owned by Zenith_InputDevices.
/// Thread affinity: IMMUTABLE after the pump that wrote it; the log span is read by phase-1 nodes of every world
///   during the frame (read-only, published by the pump before RunFrame).
///   Members non-const: an element of the transition log, overwritten every pump.
/// Invalidation: Invalid after the second Pump following the one that wrote it (the log keeps one frame of carry).
/// Units: m_ulTimestampNs in monotonic nanoseconds; values per the table "Control catalogue".
/// Errors: None.
/// Version: since Z2 Stage 3. Never serialized.
/// @endcontract
struct Zenith_InputTransition
{
	Zenith_InputDeviceID m_xDevice;         // 8
	Zenith_InputControl m_xControl;         // 4
	uint8 m_uFlags = 0;                    // Zenith_InputTransitionFlag: SYNTHETIC_RELEASE, REPEAT, INJECTED, LATENCY_PROBE, CLAIMED, COALESCED, CONSUMED
	uint8 m_auReserved[3] = {};
	float m_fX = 0.0f;                      // button: 0 or 1; AXIS1D: the value; AXIS2D and deltas: x
	float m_fY = 0.0f;
	float m_fZ = 0.0f;
	Zenith_WindowID m_xWindow;              // the window the transition belongs to (focus or pointer window)
	uint64 m_ulTimestampNs = 0;
	uint64 m_ulSequence = 0;               // process-wide, strictly increasing, shared with Zenith_PlatformEvent
};
static_assert(sizeof(Zenith_InputTransition) == 56);

struct Zenith_VirtualControlDesc { const char* m_szName; Zenith_InputActionType m_eType; };
struct Zenith_VirtualDeviceDesc
{
	const char* m_szName = nullptr;         // ^[a-z][A-Za-z0-9]{0,31}$, unique among virtual devices
	Zenith_Span<const Zenith_VirtualControlDesc> m_axControls;   // at most 32
	Zenith_InputDeviceKind m_eSchemeKind = INPUT_DEVICE_KIND_TOUCHSCREEN;   // the scheme its actuations count toward
};

/// @contract type Zenith_InputDevices
/// Ownership: Owned by Zenith_Platform; created by Zenith_Platform::Create.
/// Thread affinity: MAIN_THREAD, except GetFrameTransitions (ANY_THREAD during the frame, read-only) and
///   InjectSyntheticTransition (ANY_THREAD; queued for the next Pump).
/// Invalidation: Spans returned by GetFrameTransitions and EnumerateDevices are valid until the next Pump.
/// Units: As the table "Control catalogue".
/// Errors: Zenith_Result<…, Zenith_InputError>: INPUT_ERROR_DEVICE_NOT_FOUND, INPUT_ERROR_TOO_MANY_DEVICES,
///   INPUT_ERROR_BAD_VIRTUAL_DEVICE; injected values out of their range are clamped and counted, never asserted.
/// Version: since Z2 Stage 3. Source API.
/// @endcontract
class Zenith_InputDevices
{
public:
	Zenith_Span<const Zenith_InputTransition> GetFrameTransitions() const;     // this frame, in order
	Zenith_Span<const Zenith_InputTransition> GetCarriedTransitions() const;   // see "Tick assignment of events"
	// MAIN_THREAD, after RunFrame when the input world ran no tick: appends this frame's transitions (flags final) to the
	// carry region; overflow drops the oldest motion transitions first, then the oldest others (input.carry.dropped).
	void CarryFrameTransitions();
	void ClearCarriedTransitions();   // MAIN_THREAD, after a RunFrame in which the input world ran at least one tick
	Zenith_Span<const Zenith_InputDeviceInfo> EnumerateDevices() const;
	Zenith_Result<Zenith_InputDeviceInfo, Zenith_InputError> GetDevice(Zenith_InputDeviceID xDevice) const;
	bool IsControlHeld(Zenith_InputDeviceID xDevice, Zenith_InputControl xControl) const;
	Zenith_InputValue GetControlValue(Zenith_InputDeviceID xDevice, Zenith_InputControl xControl) const;
	Zenith_InputDeviceID GetLogicalKeyboard() const;
	Zenith_InputDeviceID GetLogicalMouse() const;
	Zenith_InputDeviceID GetMostRecentGamepad() const;       // null when none connected

	Zenith_Result<Zenith_InputDeviceID, Zenith_InputError> CreateVirtualDevice(const Zenith_VirtualDeviceDesc& xDesc);
	void DestroyVirtualDevice(Zenith_InputDeviceID xDevice);                // synthesises releases first
	Zenith_Result<void, Zenith_InputError> SetVirtualControl(Zenith_InputDeviceID xDevice, uint16 uControl,
		Zenith_InputValue xValue);                                          // applied at the next Pump, in call order

	/// @deviation Thread affinity: ANY_THREAD. For the MP-LATENCY harness and tests; the transition carries
	///   INPUT_TRANSITION_FLAG_LATENCY_PROBE or INPUT_TRANSITION_FLAG_INJECTED and the given timestamp.
	Zenith_Result<void, Zenith_InputError> InjectSyntheticTransition(const Zenith_InputTransition& xTransition);

	Zenith_StringView GetKeyDisplayName(Zenith_InputKey eKey) const;   // current layout; valid until the next Pump
};
```

### Transition log

The transition log is the ordered record of every control change of a frame.

1. Each SDL input event produces one or more transitions in SDL queue order: a key or button event one; a stick event one AXIS2D transition carrying both axes (SDL reports one axis per event; the pump merges the pair into the stick's current value); a finger event one per touch control that changed (`press`, `position`, `delta`, `pressure`).
2. Every transition gets the next process-wide sequence number and the event's timestamp converted to the monotonic clock.
3. Capacity is `input.transitionCapacity` (8,192) per frame, of which the last 1,024 slots are reserved for non-motion transitions (buttons, presses, connect and disconnect releases).
4. Motion coalescing happens when a frame's motion transitions (mouse `position` and `delta`, touch `position`, `delta` and `pressure`, pen motion, stick and trigger values, gyro and accelerometer) would use the unreserved region beyond 50%: consecutive motion transitions of the same device and control with no button transition of that device between them merge into one (positions, sticks and triggers: last value wins; deltas: summed), flagged `INPUT_TRANSITION_FLAG_COALESCED`, keeping the latest timestamp and sequence.
5. When the unreserved region is full, further motion transitions are dropped and counted in `input.transitions.dropped_motion`; the device's current value is still updated, so the next frame's first motion transition carries the correct value.
6. When the reserved region is also full, a further non-motion transition is dropped and counted in `input.transitions.dropped_buttons`, and the device's held state is still updated, so the mapper resynchronises from device state at its next run (`Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Tick assignment of events") and no action stays stuck.

The log keeps the previous frame's unconsumed tail as its carry region (`Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Tick assignment of events"); older transitions are discarded.

## Keyboard and mouse

- Bindings name physical keys (scancodes), so a binding survives a keyboard layout change; `GetKeyDisplayName` returns the key's label in the current layout (`SDL_GetKeyFromScancode` then `SDL_GetKeyName`) for prompts and rebinding UI.
- OS key repeats are recorded with `INPUT_TRANSITION_FLAG_REPEAT`; the mapper ignores them, UI text editing uses them.
- Focus loss of the window that holds keyboard focus synthesises, at the focus-loss timestamp, a release transition (`INPUT_TRANSITION_FLAG_SYNTHETIC_RELEASE`) for every held key and mouse button, so no action stays pressed while the game is not focused.
- Relative mouse mode (`Zenith_Window::SetRelativeMouseMode`, `SDL_SetWindowRelativeMouseMode`) hides and captures the cursor; `delta` then carries raw unaccelerated motion. Relative mode is released on focus loss and restored on focus gain when it was on.
- Mouse `position` is in the points of the window under the pointer; while a button is held the pointer stays captured by the window that received the press (SDL's automatic capture), so a drag across editor windows keeps one window's coordinates.
- The Android back key arrives as `/keyboard/acBack` (`SDL_HINT_ANDROID_TRAP_BACK_BUTTON=1`) and is bound by the engine bindings to `System.Back` (§ "Engine actions and contexts").

## Controllers

### Gamepad API

Gamepads are SDL3 gamepads (`SDL_OpenGamepad` on `SDL_EVENT_GAMEPAD_ADDED`). Joysticks that SDL cannot map to the gamepad layout are ignored and counted in `input.devices.unmapped_joysticks`.

| Query | SDL3 source |
|---|---|
| type and glyph family | `SDL_GetGamepadType` mapped to `Zenith_GamepadType` |
| face-button label for prompts | `SDL_GetGamepadButtonLabel` (`A`/`B`/`X`/`Y`, `Cross`/`Circle`/`Square`/`Triangle`) through `Zenith_InputSystem::GetBindingDisplay` |
| rumble, trigger rumble | `SDL_GetGamepadProperties` booleans `SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN`, `SDL_PROP_GAMEPAD_CAP_TRIGGER_RUMBLE_BOOLEAN` |
| gyro, accelerometer | `SDL_GamepadHasSensor`; enabled with `SDL_SetGamepadSensorEnabled` only while `input.gamepadSensors` is true and a binding in an active context names `gyro` or `accelerometer` |
| identity for reconnection | `SDL_GetGamepadGUIDForID`, `SDL_GetGamepadSerial`, `SDL_GetGamepadPath` |

Stick values are delivered raw; the default engine and game bindings apply a radial deadzone processor (§ "Processors"). `SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS=0` stops gamepad input reaching an unfocused Windows game.

### Gamepad types and glyph families

| `Zenith_GamepadType` | Glyph family returned by `GetBindingDisplay` |
|---|---|
| `XBOX360`, `XBOXONE`, `STANDARD`, `UNKNOWN` | `GLYPH_FAMILY_XBOX` |
| `PS3`, `PS4`, `PS5` | `GLYPH_FAMILY_PLAYSTATION` |
| `SWITCH_PRO`, `SWITCH_JOYCON_*` | `GLYPH_FAMILY_NINTENDO` |

The glyph images are UI content (`Zenith2/Docs/Design/UI.md` (pending; its "Public API" section)); PLAT supplies the family, the control path and the label.

### Mapping database

- SDL3's built-in mapping database is used as shipped with the pinned SDL3.
- A game may ship `content:/Input/gamecontrollerdb.txt` (at most 1 MiB); `Zenith_InputSystem::LoadBindings` reads it through the virtual filesystem and passes it to `SDL_AddGamepadMappingsFromIO` over an `SDL_IOFromConstMem` stream; lines SDL rejects are counted in `input.mappings.rejected` and logged with their line number, and the file never fails startup.
- `SDL_EVENT_GAMEPAD_REMAPPED` re-reads the device's capabilities and emits `PLATFORM_EVENT_INPUT_DEVICE_REMAPPED`; held controls whose mapping changed are released synthetically first.

### Hotplug

| Event | Behaviour |
|---|---|
| `SDL_EVENT_GAMEPAD_ADDED` | open the gamepad; if a disconnected gamepad with the same GUID and serial (or, with no serial, the same GUID and path) is inside its reconnection window, reuse its `Zenith_InputDeviceID` (§ "Reconnection"); otherwise allocate a slot; at `uINPUT_MAX_GAMEPADS` connected gamepads the new one is not opened and `input.devices.gamepads_rejected` counts it; emit `PLATFORM_EVENT_INPUT_DEVICE_CONNECTED` (flag `uINPUT_DEVICE_FLAG_RECONNECTED` when reused) |
| `SDL_EVENT_GAMEPAD_REMOVED` | synthesise a release (or a return to rest: sticks and triggers to 0) for every non-rest control of the device at the removal timestamp; stop its haptics; close it; keep its slot reserved for the reconnection window; emit `PLATFORM_EVENT_INPUT_DEVICE_DISCONNECTED` with `uINPUT_DEVICE_FLAG_LAST_OF_KIND` when no other gamepad remains connected |
| keyboard, mouse, touchscreen, pen added or removed | keyboards and mice stay one logical device each (the logical device never disconnects; removal of a physical keyboard synthesises releases of the keys it held); touchscreens and pens are created and destroyed like gamepads without a reconnection window |

- Both events are world-visible (`Zenith2/Docs/Design/PLAT/Platform.md` → "Zenith_PlatformEvent"): the game decides whether a disconnect pauses play (the engine pauses nothing itself), and its scripts observe `Platform.DeviceDisconnected` with the last-of-kind flag at the next tick boundary.
- `Zenith_PlatformDevicePayload::m_uFlags` carries `uINPUT_DEVICE_FLAG_RECONNECTED` (bit 0) and `uINPUT_DEVICE_FLAG_LAST_OF_KIND` (bit 1).

### Reconnection

- A removed gamepad's slot stays reserved for `input.reconnectWindowSeconds` (30) with its generation unchanged; a matching reconnection inside the window gets the same `Zenith_InputDeviceID`, so every consumer holding the ID (the haptic player, the UI's active-device choice) resumes without a lookup.
- When the window expires the slot is freed (its generation increments) and the ID is stale.
- User binding overrides never name a device ID (§ "Persisting user bindings"), so reconnection has no binding consequence.

## Touch and pen

### Touch points

- Each SDL finger is assigned the lowest free touch index 0–9 on `SDL_EVENT_FINGER_DOWN` and keeps it until `SDL_EVENT_FINGER_UP` or `SDL_EVENT_FINGER_CANCELED`; an eleventh concurrent finger is ignored and counted in `input.touch.over_limit`.
- The primary touch (`index-seg` `primary`, `uINPUT_TOUCH_INDEX_PRIMARY`) is the earliest-down finger still down; when it lifts, the next earliest becomes primary and its press is not re-reported (the primary `press` control goes to 0 then to 1 at the same timestamp only when another finger is still down, flagged synthetic), so a binding to the primary touch sees one continuous press per finger.
- Positions are SDL's normalised `x`, `y` multiplied by the window's pixel size; `SDL_EVENT_FINGER_CANCELED` (a system gesture took the touch) produces a release with `INPUT_TRANSITION_FLAG_SYNTHETIC_RELEASE`.

### Mouse and touch emulation

- SDL's synthesis is off in both directions (`SDL_HINT_TOUCH_MOUSE_EVENTS=0`, `SDL_HINT_MOUSE_TOUCH_EVENTS=0`, `SDL_HINT_PEN_MOUSE_EVENTS=0`, `SDL_HINT_PEN_TOUCH_EVENTS=0`), so a touch is never also a mouse click and every scheme is explicit.
- `input.emulateTouchWithMouse` (non-Shipping builds, default false) makes the left mouse button and position drive touch index 0 of a virtual touchscreen, for testing touch bindings and gestures in the editor on Windows.
- Pens report `tip`, buttons, `pressure` and `tilt` as their own device; binding a pen to touch actions is a bindings-file choice.

## Gestures

### Gesture recogniser API

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_Gestures.h

enum Zenith_GestureKind : uint8
{
	GESTURE_KIND_TAP = 1, GESTURE_KIND_DOUBLE_TAP, GESTURE_KIND_LONG_PRESS, GESTURE_KIND_TWO_FINGER_TAP,
	GESTURE_KIND_PAN, GESTURE_KIND_SWIPE, GESTURE_KIND_PINCH, GESTURE_KIND_ROTATE, GESTURE_KIND_COUNT,
};

enum Zenith_GestureState : uint8
{
	GESTURE_STATE_IDLE = 0,          // no candidate touch
	GESTURE_STATE_TRACKING,          // candidate touches down; not yet decided
	GESTURE_STATE_PENDING,           // recognised, waiting for a dependency to fail (tap waiting for double tap)
	GESTURE_STATE_BEGAN,             // continuous gesture started this transition
	GESTURE_STATE_CHANGED,           // continuous gesture updated
	GESTURE_STATE_ENDED,             // continuous gesture finished normally
	GESTURE_STATE_RECOGNISED,        // discrete gesture fired
	GESTURE_STATE_CANCELLED,         // continuous gesture stopped by a competitor or a cancelled touch
	GESTURE_STATE_FAILED,            // cannot be this gesture until every touch lifts
};

struct Zenith_GestureConfig          // every threshold from a configuration key (table "Gesture thresholds")
{
	float m_fTapSlopDp = 10.0f;
	uint32 m_uTapMaxMs = 250;
	uint32 m_uDoubleTapIntervalMs = 300;
	float m_fDoubleTapSlopDp = 40.0f;
	uint32 m_uLongPressMs = 500;
	float m_fPanStartDp = 10.0f;
	uint32 m_uSwipeMaxMs = 300;
	float m_fSwipeMinDistanceDp = 50.0f;
	float m_fSwipeMinVelocityDpPerS = 1000.0f;
	float m_fPinchStartDp = 8.0f;
	float m_fRotateStartDegrees = 5.0f;
	uint32 m_uTwoFingerTapMaxMs = 250;
	bool m_bTapWaitsForDoubleTap = true;
	uint32 m_uEnabledMask = 0xFFu;  // bit (1 << (kind − 1)) per enabled recogniser
};

enum Zenith_SwipeDirection : uint8 { SWIPE_DIRECTION_NONE = 0, SWIPE_DIRECTION_LEFT, SWIPE_DIRECTION_RIGHT, SWIPE_DIRECTION_UP, SWIPE_DIRECTION_DOWN };

struct Zenith_GestureEvent
{
	Zenith_GestureKind m_eKind;
	Zenith_GestureState m_eState;        // BEGAN, CHANGED, ENDED, RECOGNISED or CANCELLED
	Zenith_SwipeDirection m_eDirection;  // swipes: SWIPE_DIRECTION_LEFT, _RIGHT, _UP, _DOWN
	float m_fPositionX, m_fPositionY;    // centroid in window pixels
	float m_fDeltaX, m_fDeltaY;          // pan: dp since the previous event
	float m_fScaleDelta;                 // pinch: current distance / previous distance − 1
	float m_fRotationDelta;              // rotate: radians since the previous event, counter-clockwise positive
	uint64 m_ulTimestampNs;
};

/// @contract type Zenith_GestureRecogniserSet
/// Ownership: Owned by Zenith_InputSystem, one per touchscreen device.
/// Thread affinity: MAIN_THREAD (runs inside Platform.Pump).
/// Invalidation: None.
/// Units: dp, milliseconds, radians, as the fields state.
/// Errors: None: invalid touch sequences (an up with no down) are ignored and counted in input.gestures.bad_sequence.
/// Version: since Z2 Stage 3. Source API.
/// @endcontract
class Zenith_GestureRecogniserSet
{
public:
	explicit Zenith_GestureRecogniserSet(const Zenith_GestureConfig& xConfig);
	void SetConfig(const Zenith_GestureConfig& xConfig, float fPixelsPerDp);   // at a pump, between frames
	void ProcessTouch(const Zenith_InputTransition& xTouch, Zenith_Span<Zenith_GestureEvent>& xOut);
	void AdvanceTime(uint64 ulNowNs, Zenith_Span<Zenith_GestureEvent>& xOut);  // timers: long press, tap timeouts
	Zenith_GestureState GetState(Zenith_GestureKind eKind) const;
	void CancelAll(uint64 ulNowNs, Zenith_Span<Zenith_GestureEvent>& xOut);    // focus loss, background
};
```

Recognised gestures become transitions of the gesture device (`INPUT_DEVICE_KIND_GESTURE`), inserted into the transition log at the timestamp of the touch transition or timer that produced them, so bindings refer to them like any control (`/gesture/doubleTap`). Touches claimed by a pointer claimant (§ "Presentation claims") are never given to the recognisers.

### Gesture thresholds

| Key | Default | Meaning |
|---|---|---|
| `input.gesture.tapSlopDp` | 10 | maximum movement of a tap or long press |
| `input.gesture.tapMaxMs` | 250 | maximum press duration of a tap |
| `input.gesture.doubleTapIntervalMs` | 300 | maximum time from the first tap's release to the second tap's press |
| `input.gesture.doubleTapSlopDp` | 40 | maximum distance between the two taps |
| `input.gesture.longPressMs` | 500 | press duration that begins a long press |
| `input.gesture.panStartDp` | 10 | movement that begins a pan |
| `input.gesture.swipeMaxMs` | 300 | maximum duration of a swipe |
| `input.gesture.swipeMinDistanceDp` | 50 | minimum distance of a swipe |
| `input.gesture.swipeMinVelocityDpPerS` | 1000 | minimum average velocity of a swipe |
| `input.gesture.pinchStartDp` | 8 | change of finger distance that begins a pinch |
| `input.gesture.rotateStartDegrees` | 5 | change of finger angle that begins a rotation |
| `input.gesture.twoFingerTapMaxMs` | 250 | maximum duration of a two-finger tap |
| `input.gesture.tapWaitsForDoubleTap` | true | delay a tap until a double tap has failed |

Distances in dp are converted to pixels with the window's `m_fDisplayScale` (`Zenith2/Docs/Design/PLAT/Platform.md` → "Units points pixels and dp") at every display-scale change.

### Tap state table

One-finger events: `DOWN` (first finger down), `EXTRA_DOWN` (another finger down while one is tracked), `MOVE` (tracked finger moved; `d` = distance from its down position in dp), `UP` (tracked finger up; `t` = time since down), `CANCEL` (touch cancelled), `TIMER` (time advanced; `t` = time since down, `r` = time since the first tap's release).

| State | DOWN | EXTRA_DOWN | MOVE | UP | CANCEL | TIMER |
|---|---|---|---|---|---|---|
| IDLE | → TRACKING: record down time and position | — | — | ignored | ignored | — |
| TRACKING | — | → FAILED | d > tapSlopDp → FAILED | t ≤ tapMaxMs: → RECOGNISED if tap does not wait for double tap, else → PENDING; t > tapMaxMs → FAILED | → FAILED | t > tapMaxMs → FAILED |
| PENDING | double-tap candidate: stays PENDING while the double-tap recogniser accepts this DOWN as its second tap; when it rejects the DOWN (farther than `doubleTapSlopDp`), → RECOGNISED at once (emit at this DOWN's timestamp), then the DOWN is processed again from IDLE, so the new finger starts a fresh TRACKING | → FAILED | — | — | → FAILED | r > doubleTapIntervalMs → RECOGNISED (emit at the timer's timestamp) |
| RECOGNISED | emit the `tap` pulse at the UP (or TIMER) timestamp, then → IDLE | | | | | |
| FAILED | ignored | ignored | ignored | last finger up → IDLE | last finger up → IDLE | — |

A double tap's recognition moves a PENDING tap to FAILED without emitting it.

### Double tap state table

| State | DOWN | EXTRA_DOWN | MOVE | UP | CANCEL | TIMER |
|---|---|---|---|---|---|---|
| IDLE | → FIRST_DOWN | — | — | — | — | — |
| FIRST_DOWN | — | → FAILED | d > tapSlopDp → FAILED | t ≤ tapMaxMs → WAIT_SECOND; else → FAILED | → FAILED | t > tapMaxMs → FAILED |
| WAIT_SECOND | distance to the first tap ≤ doubleTapSlopDp → SECOND_DOWN; else the pending tap is emitted (tap row PENDING) and this DOWN restarts the recogniser at FIRST_DOWN | — | — | — | → FAILED | r > doubleTapIntervalMs → FAILED |
| SECOND_DOWN | — | → FAILED | d > tapSlopDp → FAILED | t ≤ tapMaxMs → RECOGNISED; else → FAILED | → FAILED | t > tapMaxMs → FAILED |
| RECOGNISED | emit `doubleTap` at the UP timestamp, fail the pending tap, → IDLE | | | | | |
| FAILED | ignored | ignored | ignored | last finger up → IDLE (a pending tap is released to RECOGNISED) | last finger up → IDLE | — |

FIRST_DOWN, WAIT_SECOND and SECOND_DOWN are sub-states of `GESTURE_STATE_TRACKING` in `GetState`.

### Long press state table

| State | DOWN | EXTRA_DOWN | MOVE | UP | CANCEL | TIMER |
|---|---|---|---|---|---|---|
| IDLE | → TRACKING | — | — | — | — | — |
| TRACKING | — | → FAILED | d > tapSlopDp → FAILED | → FAILED | → FAILED | t ≥ longPressMs → BEGAN: `longPress` press transition, fail tap, double tap, pan and swipe |
| BEGAN, CHANGED | — | → CANCELLED | → CHANGED: `position` update | → ENDED: `longPress` release transition, → IDLE | → CANCELLED: release with the synthetic flag, → IDLE | — |
| FAILED | ignored | ignored | ignored | last finger up → IDLE | last finger up → IDLE | — |

### Pan state table

| State | DOWN | EXTRA_DOWN | MOVE | UP | CANCEL | TIMER |
|---|---|---|---|---|---|---|
| IDLE | → TRACKING | — | — | — | — | — |
| TRACKING | — | → FAILED (two-finger recognisers take over) | d ≥ panStartDp → BEGAN: `pan` delta of the whole movement since down; fail tap, double tap, long press | → FAILED | → FAILED | — |
| BEGAN, CHANGED | — | → CANCELLED: a zero `pan` delta flagged synthetic | → CHANGED: `pan` delta since the previous move | → ENDED: evaluate "Swipe classification", → IDLE | → CANCELLED, → IDLE | — |
| FAILED | ignored | ignored | ignored | last finger up → IDLE | last finger up → IDLE | — |

### Swipe classification

On a pan's ENDED: let `D` be the displacement from the down position in dp, `T` the duration from down to up. A swipe is recognised when `T ≤ swipeMaxMs`, `|D| ≥ swipeMinDistanceDp` and `|D| / T ≥ swipeMinVelocityDpPerS`. Its direction is the dominant axis of `D` (`|Dx| ≥ |Dy|`: left or right by the sign of `Dx`; otherwise up or down by the sign of `Dy`, with up meaning towards the top of the screen); ties go to the horizontal axis. It emits one pulse on `swipeLeft`, `swipeRight`, `swipeUp` or `swipeDown` at the up timestamp, after the pan's ENDED transition.

### Pinch and rotate state table

Two-finger events: `SECOND_DOWN` (a second finger down; `d0`, `a0` = initial distance and angle), `TWO_MOVE` (either finger moved; `d`, `a` = current distance in dp and angle in degrees), `ONE_UP` (either finger up), `THIRD_DOWN`.

| State | SECOND_DOWN | TWO_MOVE | ONE_UP | THIRD_DOWN | CANCEL |
|---|---|---|---|---|---|
| IDLE | → TRACKING (pinch and rotate each), cancel every one-finger recogniser in TRACKING and every BEGAN pan | — | — | — | — |
| TRACKING | — | pinch: \|d − d0\| ≥ pinchStartDp → BEGAN; rotate: \|a − a0\| ≥ rotateStartDegrees → BEGAN; the two are evaluated independently and may both begin | two-finger tap: both fingers up within twoFingerTapMaxMs of SECOND_DOWN with neither pinch nor rotate begun → RECOGNISED (`twoFingerTap` pulse); otherwise → IDLE | ignored (the first two fingers stay tracked) | → FAILED |
| BEGAN, CHANGED | — | → CHANGED: `pinch` value `d / d_previous − 1`, `rotate` value `a − a_previous` in radians | → ENDED, → IDLE | ignored | → CANCELLED, → IDLE |
| FAILED | ignored | ignored | last finger up → IDLE | ignored | last finger up → IDLE |

### Gesture arbitration

- One-finger recognisers (tap, double tap, long press, pan, swipe) are exclusive: the first to reach BEGAN or RECOGNISED fails the others, except that tap waits in PENDING for double tap when `tapWaitsForDoubleTap` is true, and swipe is evaluated only from a pan's end.
- Two-finger recognisers (pinch, rotate, two-finger tap) take over from every one-finger recogniser on SECOND_DOWN; pinch and rotate may be active together.
- A cancelled touch, focus loss and entering the background cancel every active recogniser (`CancelAll`), which emits the releases and CANCELLED transitions so no gesture control stays active.
- Every recogniser is a pure function of the touch transitions and the timestamps given to it, so a recorded touch sequence reproduces its gestures exactly (the gesture tests replay recorded sequences).

## Text input and IME

### Text input API

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_InputDevices.h (text input and IME; Architecture's header table places them here)

enum Zenith_TextInputPurpose : uint8
{
	TEXT_INPUT_PURPOSE_TEXT = 1, TEXT_INPUT_PURPOSE_NAME, TEXT_INPUT_PURPOSE_EMAIL, TEXT_INPUT_PURPOSE_USERNAME,
	TEXT_INPUT_PURPOSE_PASSWORD, TEXT_INPUT_PURPOSE_NUMBER, TEXT_INPUT_PURPOSE_NUMBER_PASSWORD,
};

enum Zenith_TextInputEventKind : uint8
{
	TEXT_INPUT_EVENT_COMMIT = 1,          // final text to insert at the caret
	TEXT_INPUT_EVENT_COMPOSITION,         // the current composition (pre-edit) string, replacing the previous one
	TEXT_INPUT_EVENT_COMPOSITION_END,     // the composition was cleared without a commit
};

struct Zenith_TextInputRect { sint32 m_iX = 0; sint32 m_iY = 0; sint32 m_iWidth = 0; sint32 m_iHeight = 0; sint32 m_iCaretOffset = 0; };  // window points

struct Zenith_TextInputDesc
{
	Zenith_WindowID m_xWindow;
	Zenith_TextInputPurpose m_ePurpose = TEXT_INPUT_PURPOSE_TEXT;
	Zenith_TextInputRect m_xArea;            // where the candidate window should appear
	bool m_bMultiline = false;
	bool m_bAutoCorrect = true;
	bool m_bCapitaliseSentences = true;
	uint32 m_uMaxBytes = 4096;              // commits beyond the session's remaining capacity are truncated at a code point boundary
};

struct Zenith_TextInputSessionID
{
	uint32 m_uValue = 0;                    // increments at every Begin; 0 is null
	constexpr bool IsNull() const { return m_uValue == 0; }
	friend constexpr bool operator==(Zenith_TextInputSessionID, Zenith_TextInputSessionID) = default;
};

struct Zenith_TextInputEvent
{
	Zenith_TextInputEventKind m_eKind;
	Zenith_StringView m_strText;             // valid UTF-8, in the frame's text arena
	sint32 m_iCursor = 0;                   // composition: cursor in code points from the composition start
	sint32 m_iSelectionLength = 0;          // composition: the highlighted clause in code points
	uint64 m_ulTimestampNs = 0;
};

/// @contract type Zenith_TextInput
/// Ownership: Owned by Zenith_Platform.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: Event spans are valid until the next Pump; a session ID is stale after End or after another Begin.
/// Units: Rectangles in window points; lengths in code points where named, bytes elsewhere.
/// Errors: Zenith_Result<…, Zenith_InputError>: INPUT_ERROR_TEXT_INPUT_ACTIVE (Begin while a session is active),
///   INPUT_ERROR_UNKNOWN_WINDOW; OS text that is not valid UTF-8 is replaced per code unit with U+FFFD and counted.
/// Version: since Z2 Stage 3. Source API.
/// @endcontract
class Zenith_TextInput
{
public:
	Zenith_Result<Zenith_TextInputSessionID, Zenith_InputError> Begin(const Zenith_TextInputDesc& xDesc);
	void UpdateArea(Zenith_TextInputSessionID xSession, const Zenith_TextInputRect& xArea);
	void End(Zenith_TextInputSessionID xSession);
	bool IsActive() const;
	Zenith_Span<const Zenith_TextInputEvent> GetFrameEvents() const;
	bool IsSoftKeyboardShown() const;       // Android: SDL_ScreenKeyboardShown
};
```

`Begin` calls `SDL_StartTextInputWithProperties` with `SDL_PROP_TEXTINPUT_TYPE_NUMBER` from the purpose (`TEXT` → `SDL_TEXTINPUT_TYPE_TEXT`, `NAME` → `_TEXT_NAME`, `EMAIL` → `_TEXT_EMAIL`, `USERNAME` → `_TEXT_USERNAME`, `PASSWORD` → `_TEXT_PASSWORD_HIDDEN`, `NUMBER` → `_NUMBER`, `NUMBER_PASSWORD` → `_NUMBER_PASSWORD_HIDDEN`), `SDL_PROP_TEXTINPUT_CAPITALIZATION_NUMBER`, `SDL_PROP_TEXTINPUT_AUTOCORRECT_BOOLEAN` and `SDL_PROP_TEXTINPUT_MULTILINE_BOOLEAN`, then `SDL_SetTextInputArea`; `End` calls `SDL_StopTextInput`. One session is active at a time.

### Composition events

- `SDL_HINT_IME_IMPLEMENTED_UI=composition`: the engine's text field draws the composition string inline (UI area) and the OS draws the candidate list at the text-input area.
- `SDL_EVENT_TEXT_EDITING` becomes `TEXT_INPUT_EVENT_COMPOSITION` (its text, `start` as the cursor, `length` as the selection); an empty editing string after a non-empty one becomes `TEXT_INPUT_EVENT_COMPOSITION_END`.
- `SDL_EVENT_TEXT_INPUT` becomes `TEXT_INPUT_EVENT_COMMIT`; a commit implicitly ends the composition.
- Text is validated as UTF-8; invalid sequences are replaced by U+FFFD per invalid code unit and counted in `input.text.invalid_utf8`; NUL is removed.
- Ending a session with a composition in progress emits `TEXT_INPUT_EVENT_COMPOSITION_END`, never a commit.

### Soft keyboard on Android

- `Begin` shows the soft keyboard (`SDL_StartTextInputWithProperties` does) and the keyboard's inset is reported through `PLATFORM_EVENT_WINDOW_SAFE_AREA_CHANGED`, so UI keeps the field visible.
- `IsSoftKeyboardShown` lets UI hide the keyboard-shaped prompts; the soft keyboard's own "done" key commits and ends the session (SDL delivers `SDL_EVENT_TEXT_INPUT` then stops text input, which `Zenith_TextInput` reports by `IsActive()` becoming false).

### Text entry consumes keyboard controls

While a session is active `Zenith_TextInput` pushes the engine context `Engine.TextEntry` (§ "Engine actions and contexts") on the presentation stack, pinned at the top. The context has no bindings; its bindings-file entry declares `"consumesDevices": ["Keyboard"]`, and step 4.3 of "Mapper algorithm" consumes every control of a listed device kind at that context's level, so every keyboard transition is flagged `INPUT_TRANSITION_FLAG_CONSUMED` for lower presentation contexts and for the simulation mapper, and typing a name never moves the character. `End` removes the context. The raw keyboard transitions stay readable by UI through `GetFrameTransitions` for caret movement and deletion. Gamepad and touch controls are not consumed.

## Haptics

### Haptic API

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_InputDevices.h (haptics; Architecture's header table places them here)

enum Zenith_HapticEffectKind : uint8
{
	HAPTIC_EFFECT_KIND_RUMBLE = 1,          // two motors: low and high frequency
	HAPTIC_EFFECT_KIND_TRIGGER_RUMBLE,      // impulse triggers where supported
	HAPTIC_EFFECT_KIND_DEVICE_VIBRATION,    // the phone's vibrator (Android), for touch play
};

enum Zenith_HapticChannel : uint8
{
	HAPTIC_CHANNEL_GAMEPLAY = 0, HAPTIC_CHANNEL_UI, HAPTIC_CHANNEL_CINEMATIC, HAPTIC_CHANNEL_SYSTEM, HAPTIC_CHANNEL_COUNT,
};

enum Zenith_HapticTarget : uint8
{
	HAPTIC_TARGET_ACTIVE_DEVICE = 1,        // the most recently actuated gamepad, or the phone when the active scheme is touch
	HAPTIC_TARGET_ALL_GAMEPADS,
	HAPTIC_TARGET_DEVICE,                   // m_xDevice
};

struct Zenith_HapticPlayback             // a generational handle to one playing effect; null when Play was refused
{
	uint32 m_uSlot = 0;                     // device slot × HAPTIC_CHANNEL_COUNT + channel, plus 1
	uint32 m_uGeneration = 0;               // incremented whenever the slot's effect ends, is replaced or is stopped
	constexpr bool IsNull() const { return m_uSlot == 0; }
};

struct Zenith_HapticEffect
{
	Zenith_HapticEffectKind m_eKind = HAPTIC_EFFECT_KIND_RUMBLE;
	Zenith_HapticChannel m_eChannel = HAPTIC_CHANNEL_GAMEPLAY;
	Zenith_HapticTarget m_eTarget = HAPTIC_TARGET_ACTIVE_DEVICE;
	Zenith_InputDeviceID m_xDevice;          // HAPTIC_TARGET_DEVICE only
	uint8 m_uPriority = 128;                // higher replaces lower on the same channel
	float m_fLowFrequency = 0.0f;            // [0, 1]; also the trigger-rumble left value and the vibration strength
	float m_fHighFrequency = 0.0f;           // [0, 1]; also the trigger-rumble right value
	uint32 m_uDurationMs = 0;               // 1 to 10,000
};

/// @contract type Zenith_HapticPlayer
/// Ownership: Owned by Zenith_Platform.
/// Thread affinity: MAIN_THREAD, except Submit (ANY_THREAD: the epilogue stage of each world submits from a worker).
/// Invalidation: A Zenith_HapticPlayback is stale once its effect ends, is replaced or is stopped.
/// Units: Amplitudes in [0, 1]; durations in milliseconds.
/// Errors: Out-of-range amplitudes and durations are clamped and counted (input.haptics.clamped); an effect on a
///   device without the capability is ignored and counted (input.haptics.unsupported); nothing asserts.
/// Version: since Z2 Stage 3. Source API.
/// @endcontract
class Zenith_HapticPlayer
{
public:
	Zenith_HapticPlayback Play(const Zenith_HapticEffect& xEffect);      // main thread, immediate
	void Stop(Zenith_HapticPlayback xPlayback);
	void StopChannel(Zenith_HapticChannel eChannel);
	void StopAll();
	/// @deviation Thread affinity: ANY_THREAD. Bounded MPSC queue of 256 with QUEUE_FULL_POLICY_DROP_NEWEST (counted in
	///   input.haptics.submissions.full, .dropped, .high_water); submissions play at the next Pump in submission order.
	bool Submit(const Zenith_HapticEffect& xEffect);
	void SetChannelIntensity(Zenith_HapticChannel eChannel, float fIntensity);
	void Update(uint64 ulNowNs);                                                      // inside Platform.Pump
};
```

### Haptic mixing

1. Each device has one slot per channel; `Play` on a channel whose slot holds an effect of higher priority is refused (the returned playback is null and `input.haptics.refused` counts it); equal or lower priority is replaced.
2. The device's motor values are the per-motor maximum over its channels' active effects, each multiplied by its channel intensity and by `input.hapticsIntensity` (default 1.0; 0.0 disables every haptic, an accessibility setting surfaced by UI).
3. `Update` expires effects whose duration elapsed; whenever a device's mixed values change it calls `SDL_RumbleGamepad(low, high, remainingMs)` (values scaled to 0–65535) or `SDL_RumbleGamepadTriggers`; the remaining time passed is the minimum remaining duration of the active effects, so SDL stops the motors even if the process stalls.
4. `HAPTIC_EFFECT_KIND_DEVICE_VIBRATION` uses SDL3's Android haptic device for the system vibrator (`SDL_OpenHaptic` of the first `SDL_GetHaptics` entry, `SDL_InitHapticRumble`, `SDL_PlayHapticRumble(strength, ms)`); when no vibrator exists the effect is ignored and counted.
5. `StopAll` runs on entering `APP_STATE_SUSPENDING`, on focus loss when `input.hapticsStopOnFocusLoss` is true (default true), when the world is paused through the `Input.StopHaptic` command of a pause menu, and in the platform's destructor; a disconnected device's effects are dropped.

### Haptics from simulation

Gameplay requests haptics as commands, never by calling the player, so haptics are part of the transaction (a rolled-back invocation plays nothing) and replay reproduces them:

1. Scripts call `Input.PlayHaptic` or `Input.StopHaptic` (`Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Script-facing input API"); C++ systems record the command kinds `Input.PlayHaptic` and `Input.StopHaptic` through their command buffer. Both kinds are registered by the runtime bridge with `Zenith_ModuleStartContext::RegisterCommandKind` (`Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces"), payload `Zenith_HapticEffect` minus the device ID (targets are `ACTIVE_DEVICE` or `ALL_GAMEPADS` only, because device IDs are not simulation data), allowed in phases 2–10.
2. Their playback function appends to the world resource `Zenith_HapticRequestQueue` (capacity 64 per tick, `QUEUE_FULL_POLICY_DROP_NEWEST`, counted in `input.haptics.requests_dropped`), in merge order.
3. The epilogue stage `Input.Haptics@W<tag>`, registered through `Zenith_WorldAttachContext::AddEpilogueStage`, reads the queue and calls `Zenith_HapticPlayer::Submit` for each request in order (in a headless run it submits nothing), then clears the queue.
4. A replay plays the haptics of the recorded run, because the commands are reproduced; haptics never feed back into simulation.

## Zenith_InputSystem API

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_InputSystem.h

struct Zenith_InputFrame
{
	const Zenith_InputActionStateTable* m_pxActions = nullptr;      // the presentation action states of this frame
	Zenith_Span<const Zenith_InputTransition> m_axTransitions;      // the frame's transitions, flags final
	Zenith_Span<const Zenith_TextInputEvent> m_axTextEvents;
	Zenith_ControlScheme m_eActiveScheme = CONTROL_SCHEME_KEYBOARD_MOUSE;
	Zenith_Span<const Zenith_InputContextEntry> m_axPresentationStack;
	uint64 m_ulFrameIndex = 0;
};

enum Zenith_RebindResetScope : uint8 { REBIND_RESET_SCOPE_BINDING = 1, REBIND_RESET_SCOPE_ACTION, REBIND_RESET_SCOPE_CONTEXT, REBIND_RESET_SCOPE_ALL };

/// @contract type Zenith_InputSystem
/// Ownership: Owned by Zenith_Platform (created empty in Zenith_Platform::Create); its binding table, action set and
///   presentation mapper live in the general heap and are replaced whole by LoadBindings.
/// Thread affinity: MAIN_THREAD, except GetBindingTable and GetActionSet, which phase-1 nodes read during the frame
///   (ANY_THREAD, read-only; the table is replaced only in the frame prologue).
/// Invalidation: The Zenith_InputFrame and every span it holds are valid until the next Pump; the binding table and the
///   action set pointers are valid until the next LoadBindings or rebind apply, which happen only in the prologue.
/// Units: As the headers of the types it returns.
/// Errors: Zenith_Result<…, Zenith_InputError>; LoadBindings reports every diagnostic of every file in the log and
///   returns INPUT_ERROR_BINDINGS_INVALID when an engine, game or platform file fails.
/// Reentrancy: ALLOWED(PushPresentationContext, RemovePresentationContext) from inside claimant callbacks; the change
///   takes effect at the next Pump. Every other member follows NONE.
/// Version: since Z2 Stage 3. Source API.
/// @endcontract
class Zenith_InputSystem
{
public:
	Zenith_Result<void, Zenith_InputError> LoadBindings(Zenith_VirtualFileSystem& xFiles, Zenith_Config& xConfig,
		Zenith_AsyncIO& xIO);
	void EvaluatePresentationFrame(Zenith_Arena& xFrameArena);          // called by Zenith_Platform::Pump
	const Zenith_InputFrame& GetFrame() const;
	const Zenith_InputBindingTable& GetBindingTable() const;
	const Zenith_InputActionSet& GetActionSet() const;
	Zenith_Result<Zenith_InputActionID, Zenith_InputError> FindAction(Zenith_StringView strName) const;
	Zenith_Result<Zenith_InputContextID, Zenith_InputError> FindContext(Zenith_StringView strName) const;

	Zenith_Result<void, Zenith_InputError> PushPresentationContext(Zenith_InputContextEntry xEntry);
	void RemovePresentationContext(Zenith_InputContextID xContext);
	Zenith_Result<Zenith_CallbackToken, Zenith_InputError> RegisterClaimant(Zenith_InputClaimant& xClaimant);

	Zenith_ControlScheme GetActiveScheme() const;
	Zenith_Span<const Zenith_BindingDisplay> GetBindingDisplay(Zenith_InputActionID xAction, Zenith_InputContextID xContext,
		Zenith_ControlScheme eScheme, Zenith_Arena& xArena) const;
	Zenith_Span<const Zenith_BindingProvenanceEntry> GetBindingProvenance(Zenith_InputContextID xContext,
		Zenith_InputActionID xAction, uint32 uBindingID, Zenith_Arena& xArena) const;

	Zenith_Result<void, Zenith_InputError> BeginRebind(const Zenith_RebindDesc& xDesc);
	Zenith_RebindResult GetRebindResult() const;
	void CancelRebind();
	Zenith_Result<void, Zenith_InputError> ResetUserBindings(Zenith_RebindResetScope eScope,
		Zenith_InputBindingRef xTarget);

	Zenith_Result<Zenith_ActionMapper, Zenith_InputError> CreateSimulationMapper(Zenith_Heap& xHeap) const;
	// --input-script: the host file named on the command line, opened only through its grant
	// (Zenith2/Docs/Design/PLAT/Filesystem.md "Command-line host files"); tests and gametests pass a content path.
	Zenith_Result<void, Zenith_InputError> LoadInputScript(const Zenith_CommandLineFileGrant& xGrant);
	Zenith_Result<void, Zenith_InputError> LoadInputScript(Zenith_VirtualFileSystem& xFiles,
		const Zenith_VirtualPath& xPath);
	// The run's input tick count: input-world ticks completed since Create, summed across world swaps; the input script's
	// "tick" and the recorder's (Zenith2/Docs/Design/PLAT/SimulationInput.md "The input-script format").
	uint64 GetInputTickCount() const;
	// Input-script recording (Zenith2/Docs/Design/PLAT/SimulationInput.md "Input-script recording"); MAIN_THREAD, outside RunFrame.
	Zenith_Result<void, Zenith_InputError> BeginScriptRecording();
	Zenith_Result<Zenith_ByteSpan, Zenith_InputError> EndScriptRecording(Zenith_Arena& xArena);
	bool IsScriptRecording() const;
	// Background entry: submits the pending user-bindings write, if any, and returns at once.
	void SubmitUserBindings();
	// Shutdown (Contracts step 8, before Zenith_AsyncIO is destroyed): submits and waits for the write on the main thread.
	Zenith_Result<void, Zenith_InputError> FlushUserBindings();
	void WriteDebugSnapshot(Zenith_InputDebugSnapshot& xOut, Zenith_Arena& xArena) const;   // the input debug overlay
};
```

`Zenith_BindingProvenanceEntry` is `{ Zenith_BindingLayer m_eLayer; Zenith_StringView m_strSource; uint32 m_uLine; bool m_bOverride; bool m_bUnbound; }`. `Zenith_InputDebugSnapshot` holds the data of the overlay rows listed in the entry document's "Input debug overlay":

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_InputSystem.h
struct Zenith_InputDebugSnapshot
{
	Zenith_Span<const Zenith_InputDeviceInfo> m_axDevices;
	Zenith_ControlScheme m_eActiveScheme = CONTROL_SCHEME_KEYBOARD_MOUSE;
	Zenith_InputDeviceID m_xSchemeDevice;                         // the device that set the scheme
	Zenith_Span<const Zenith_InputContextEntry> m_axPresentationStack;   // top first
	Zenith_Span<const Zenith_InputContextEntry> m_axSimulationStack;     // the input world's, top first
	Zenith_Span<const Zenith_InputActionState> m_axPresentationActions;
	Zenith_Span<const Zenith_InputActionState> m_axSimulationActions;    // the input world's last tick
	Zenith_Span<const Zenith_InputTransition> m_axRecentTransitions;     // the last 32
	Zenith_Span<const uint32> m_auClaimedPointerDowns;                  // indices into this frame's pointer downs
	Zenith_Span<const Zenith_InputActionID> m_axClaimedActions;
	Zenith_GestureState m_aeGestureStates[8] = {};                       // per Zenith_GestureKind − 1
	uint32 m_uTickCommandCount = 0, m_uTickCommandsDropped = 0, m_uTickCommandsRejected = 0;
};
```

Every span lives in the arena passed to `WriteDebugSnapshot`.

## Actions

### Action types

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_InputAction.h

enum Zenith_InputActionType : uint8
{
	INPUT_ACTION_TYPE_BUTTON = 1,    // held or not; pressed and released edges; interactions
	INPUT_ACTION_TYPE_AXIS1D,        // a signed or unsigned scalar (triggers, throttle)
	INPUT_ACTION_TYPE_AXIS2D,        // an absolute 2D value (move stick, dpad)
	INPUT_ACTION_TYPE_AXIS3D,        // an absolute 3D value (gyro rate, accelerometer)
	INPUT_ACTION_TYPE_DELTA2D,       // a relative 2D value summed over the tick and reset every tick (mouse look)
	INPUT_ACTION_TYPE_POSE,          // reserved for Stage 7 XR: position and orientation
};

struct Zenith_InputValue { float m_fX = 0.0f; float m_fY = 0.0f; float m_fZ = 0.0f; };

enum Zenith_InputPhaseFlag : uint8
{
	INPUT_PHASE_FLAG_STARTED = 1u << 0,      // the interaction began (a press started a hold)
	INPUT_PHASE_FLAG_PERFORMED = 1u << 1,    // the interaction completed (the hold reached its duration)
	INPUT_PHASE_FLAG_ENDED = 1u << 2,        // a performed interaction ended normally (release after performing)
	INPUT_PHASE_FLAG_CANCELLED = 1u << 3,    // the interaction stopped without performing (a hold released early)
};
```

| Control type → action type | BUTTON | AXIS1D | AXIS2D | AXIS3D | DELTA2D |
|---|---|---|---|---|---|
| button | value 0/1 | 0 or 1 | — | — | — |
| AXIS1D control or an axis component | pressed at `m_fPressPoint` (default 0.5) | the value | — | — | — |
| half-axis component (`/up`, `/down`, …) | pressed at `m_fPressPoint` | the half-axis magnitude | — | — | — |
| AXIS2D control | — | — | the value | — | integrated: value × slice duration in seconds |
| AXIS3D control | — | — | — | the value | — |
| DELTA2D control | — | — | — | — | summed |
| composite | per composite kind (§ "Composites") | | | | |

Every cell marked — is rejected at load with `INPUT_ERROR_TYPE_MISMATCH`. The slice duration of the AXIS2D-to-DELTA2D integration is the frame time for the presentation mapper and the fixed tick duration (`Zenith_SimulationClock::GetTickDurationUs`) for the simulation mapper, so a stick bound to mouse look turns at a rate independent of the frame rate.

### Action IDs

```cpp
/// @contract value Zenith_InputActionID
/// Ownership: Plain value; the stable identity of an action declared in a bindings file.
/// Thread affinity: IMMUTABLE.
///   Members non-const: a handle is copy-assigned and stored in containers and in other types' members.
/// Invalidation: None: an ID is never reused for a different action, including after the action is retired.
/// Units: None.
/// Errors: None; unknown IDs resolve to INPUT_ERROR_UNKNOWN_ACTION.
/// Version: since Z2 Stage 3. Serialized in replay logs (as the command's action ID) and in user binding overrides.
/// @endcontract
struct Zenith_InputActionID
{
	uint32 m_uValue = 0;
	constexpr bool IsNull() const { return m_uValue == 0; }
	friend constexpr bool operator==(Zenith_InputActionID, Zenith_InputActionID) = default;
};

inline constexpr uint32 uINPUT_ACTION_ID_GAME_MIN = 0x00000001;
inline constexpr uint32 uINPUT_ACTION_ID_GAME_MAX = 0x00EFFFFF;
inline constexpr uint32 uINPUT_ACTION_ID_ENGINE_MIN = 0x00F00000;
inline constexpr uint32 uINPUT_ACTION_ID_ENGINE_MAX = 0x00FFFFFF;
inline constexpr uint32 uINPUT_ACTION_ID_COMPONENT_SHIFT = 24;   // bits 24-31 of a command's action ID: the value component
inline constexpr uint32 uINPUT_MAX_ACTIONS = 4096;
```

- A game declares IDs in its own range; the engine bindings file declares the engine range; a game file using an engine-range ID, or two files declaring one ID, fail with `INPUT_ERROR_ID_OUT_OF_RANGE` or `INPUT_ERROR_DUPLICATE_ACTION_ID`.
- An action removed from a bindings file lists its ID in `retiredActionIds`; any subsequent version of the file that declares a retired ID fails with `INPUT_ERROR_REUSED_RETIRED_ID`.
- Actions also have a stable name (`Gameplay.Jump`, matching `^[A-Z][A-Za-z0-9]{0,31}(\.[A-Z][A-Za-z0-9]{0,31}){1,3}$`) that scripts use; the name is looked up to the ID when a script module loads (`Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Script-facing input API"), and a rename is a bindings-file change that keeps the ID.

### Action state

```cpp
/// @contract value Zenith_InputActionState
/// Ownership: Plain value in an action-state table (the presentation frame's, or a world's Zenith_InputActionStateTable).
/// Thread affinity: IMMUTABLE for readers of a published table. Members non-const: rewritten for every slice.
/// Invalidation: A presentation table is replaced at every Pump; a world's table at every phase 1 of that world.
/// Units: m_uHeldMs in milliseconds of platform time (presentation) or of simulated time (simulation).
/// Errors: None.
/// Version: since Z2 Stage 3. Never serialized (commands are); the layout may change in any build.
/// @endcontract
struct Zenith_InputActionState
{
	Zenith_InputActionID m_xAction;
	Zenith_InputActionType m_eType = INPUT_ACTION_TYPE_BUTTON;
	uint8 m_uPhaseFlags = 0;           // accumulated over the slice (frame or tick)
	bool m_bHeld = false;               // BUTTON and AXIS1D: at or above the press point
	bool m_bPressed = false;            // became held during the slice (also true for a press and release inside one slice)
	bool m_bReleased = false;           // stopped being held during the slice
	uint16 m_uTapCount = 0;            // MULTI_TAP progress
	uint32 m_uHeldMs = 0;              // since the press; saturates at 0xFFFFFFFF
	Zenith_InputValue m_xValue;         // BUTTON: m_fX 0 or 1; AXIS1D: m_fX; AXIS2D and DELTA2D: x, y; AXIS3D: x, y, z
	Zenith_InputDeviceID m_xLastDevice; // presentation only; null in simulation tables
};

/// @contract container Zenith_InputActionStateTable
/// Ownership: Owned by its user (Zenith_InputSystem for presentation; the world resource for simulation); storage from
///   the general heap, sized once at bindings load to the action count.
/// Thread affinity: OWNER_THREAD for writers; readers read a published table.
/// Invalidation: Pointers from Find are valid until the next ResetSliceFields or rebuild.
/// Units: None.
/// Errors: Find of an unknown ID returns nullptr.
/// Version: since Z2 Stage 3. Source API.
/// Alignment: element alignment is alignof(Zenith_InputActionState).
/// Overflow: at most uINPUT_MAX_ACTIONS entries.
/// Allocation failure: Create returns INPUT_ERROR_OUT_OF_MEMORY; nothing allocates after Create.
/// Complexity: Find O(log n) by binary search over the ascending action IDs; ResetSliceFields O(n).
/// @endcontract
class Zenith_InputActionStateTable
{
public:
	static Zenith_Result<Zenith_InputActionStateTable, Zenith_InputError> Create(
		const Zenith_InputActionSet& xActions, Zenith_Heap& xHeap);
	const Zenith_InputActionState* Find(Zenith_InputActionID xAction) const;
	Zenith_Span<const Zenith_InputActionState> GetAll() const;     // ascending action ID
	void ResetSliceFields();     // clears pressed, released, phase flags and DELTA2D values
};
```

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_InputAction.h
struct Zenith_InputActionDesc
{
	Zenith_InputActionID m_xAction;
	Zenith_StringView m_strName;             // "Gameplay.Jump"
	Zenith_StringView m_strDisplayName;      // a @loc: key or literal text
	Zenith_InputActionType m_eType = INPUT_ACTION_TYPE_BUTTON;
	bool m_bRebindable = true;
	bool m_bEngine = false;                 // declared in the engine ID range
};

/// @contract type Zenith_InputActionSet
/// Ownership: Built by Zenith_InputSystem::LoadBindings in the general heap; owned by the input system; shared read-only
///   by every action-state table and mapper through const references.
/// Thread affinity: IMMUTABLE between loads; replaced only in the frame prologue.
///   Members non-const: its loader builds the tables in place.
/// Invalidation: Every view it returns is invalid after the next LoadBindings.
/// Units: None.
/// Errors: Find returns INPUT_ERROR_UNKNOWN_ACTION; GetDesc of an unknown ID returns nullptr.
/// Version: since Z2 Stage 3. Source API.
/// @endcontract
class Zenith_InputActionSet
{
public:
	Zenith_Result<Zenith_InputActionID, Zenith_InputError> Find(Zenith_StringView strName) const;   // O(log n), name-sorted index
	const Zenith_InputActionDesc* GetDesc(Zenith_InputActionID xAction) const;                     // O(log n)
	Zenith_Span<const Zenith_InputActionDesc> GetAll() const;                                     // ascending ID
	uint32 GetCount() const;
};
```

## Bindings

### Binding model

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_InputBindings.h

enum Zenith_BindingLayer : uint8
{
	BINDING_LAYER_ENGINE = 1,        // content:/Engine/Input/Engine.bindings.json
	BINDING_LAYER_GAME,              // content:/Input/*.bindings.json
	BINDING_LAYER_PLATFORM,          // content:/Input/Platform/<windows|android>/*.bindings.json
	BINDING_LAYER_USER,              // user:/config/input.bindings.json (overrides only)
};

enum Zenith_ControlScheme : uint8
{
	CONTROL_SCHEME_KEYBOARD_MOUSE = 1u << 0,
	CONTROL_SCHEME_GAMEPAD = 1u << 1,
	CONTROL_SCHEME_TOUCH = 1u << 2,        // touchscreen, pen, gesture and touch-scheme virtual devices
	CONTROL_SCHEME_XR = 1u << 3,           // reserved for Stage 7
};

enum Zenith_CompositeKind : uint8 { COMPOSITE_KIND_NONE = 0, COMPOSITE_KIND_DPAD, COMPOSITE_KIND_AXIS, COMPOSITE_KIND_VECTOR3 };

enum Zenith_ProcessorKind : uint8
{
	PROCESSOR_KIND_AXIAL_DEADZONE = 1, PROCESSOR_KIND_RADIAL_DEADZONE, PROCESSOR_KIND_SCALE, PROCESSOR_KIND_INVERT,
	PROCESSOR_KIND_NORMALISE, PROCESSOR_KIND_CLAMP, PROCESSOR_KIND_CURVE, PROCESSOR_KIND_CONFIG_SCALE,
};

enum Zenith_InteractionKind : uint8
{
	INTERACTION_KIND_DEFAULT = 0, INTERACTION_KIND_PRESS, INTERACTION_KIND_HOLD, INTERACTION_KIND_TAP,
	INTERACTION_KIND_SLOW_TAP, INTERACTION_KIND_MULTI_TAP,
};

enum Zenith_PressBehaviour : uint8 { PRESS_BEHAVIOUR_PRESS_ONLY = 1, PRESS_BEHAVIOUR_RELEASE_ONLY, PRESS_BEHAVIOUR_PRESS_AND_RELEASE };

struct Zenith_ControlPath
{
	Zenith_InputControl m_xControl;
	uint8 m_uComponent = 0;         // 0 none; 1 x, 2 y, 3 z; 4 up, 5 down, 6 left, 7 right
	uint8 m_uVirtualDevice = 0;     // /virtual/<device>: index into the virtual-device name table; 0 otherwise
	uint16 m_uReserved = 0;
};

struct Zenith_InputProcessor
{
	Zenith_ProcessorKind m_eKind = PROCESSOR_KIND_SCALE;
	float m_afParams[4] = {};        // per kind: table "Processors"
	uint32 m_uConfigScaleSlot = 0;  // PROCESSOR_KIND_CONFIG_SCALE: index into the binding table's configuration-scale key list
};

struct Zenith_InputInteraction
{
	Zenith_InteractionKind m_eKind = INTERACTION_KIND_DEFAULT;
	Zenith_PressBehaviour m_eBehaviour = PRESS_BEHAVIOUR_PRESS_ONLY;
	uint8 m_uTapCount = 2;          // MULTI_TAP
	float m_fPressPoint = 0.5f;
	uint32 m_uDurationMs = 0;       // HOLD, TAP (maximum), SLOW_TAP (minimum), MULTI_TAP (maximum per tap)
	uint32 m_uSpacingMs = 300;      // MULTI_TAP: maximum from a release to the next press
};

struct Zenith_InputBinding            // the resolved, runtime form; one per binding after layering
{
	uint32 m_uBindingID = 0;         // stable within its action (file "id")
	Zenith_InputActionID m_xAction;
	Zenith_InputContextID m_xContext;
	Zenith_CompositeKind m_eComposite = COMPOSITE_KIND_NONE;
	uint8 m_uControlCount = 0;       // alternatives (no composite) or composite parts, at most 6
	uint8 m_uModifierCount = 0;      // at most 3
	uint8 m_uProcessorCount = 0;     // at most 8
	Zenith_ControlPath m_axControls[6];
	Zenith_ControlPath m_axModifiers[3];
	Zenith_InputProcessor m_axProcessors[8];
	Zenith_InputInteraction m_xInteraction;
	uint8 m_uSchemeMask = 0;         // derived from the controls' device kinds unless the file overrides it
	bool m_bConsumes = true;
	bool m_bRebindable = true;
	bool m_bUnbound = false;          // a user override removed it
	bool m_bModifiersOrdered = true;
	bool m_bNormaliseDpad = true;
	Zenith_BindingLayer m_eLayer = BINDING_LAYER_GAME;   // the layer that last defined it
	uint16 m_uSourceIndex = 0;       // provenance: index into the loaded-file table
	uint32 m_uSourceLine = 0;
};
```

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_InputBindings.h
struct Zenith_InputContextDesc
{
	Zenith_InputContextID m_xContext;
	Zenith_StringView m_strName;
	Zenith_InputContextScope m_eScope = INPUT_CONTEXT_SCOPE_SIMULATION;
	bool m_bConsumes = true;
	bool m_bPinnedTop = false;
	uint16 m_uConsumedDeviceKindMask = 0;  // bit (1 << Zenith_InputDeviceKind) per kind of the file's consumesDevices
	Zenith_StringView m_strConflictGroup;
};

struct Zenith_BindingSourceFile { Zenith_StringView m_strPath; Zenith_BindingLayer m_eLayer; };

/// @contract type Zenith_InputBindingTable
/// Ownership: Built by Zenith_InputSystem::LoadBindings (and rebuilt by a rebind apply) in the general heap; owned by the
///   input system; shared read-only by the presentation mapper and every world's simulation mapper.
/// Thread affinity: IMMUTABLE between rebuilds; phase-1 nodes read it during the frame; rebuilt only in the prologue.
///   Members non-const: its rebuild fills the tables in place.
/// Invalidation: Every span it returns is invalid after the next rebuild, which resets every mapper.
/// Units: None.
/// Errors: Lookups of unknown IDs return empty spans or nullptr; nothing asserts.
/// Version: since Z2 Stage 3. Source API.
/// Complexity: GetBindingsForControl O(log C) over the control-to-binding index; the others O(1) or O(log n).
/// @endcontract
class Zenith_InputBindingTable
{
public:
	Zenith_Span<const Zenith_InputBinding> GetBindings() const;          // sorted by (context, action, binding ID)
	Zenith_Span<const uint32> GetBindingsForControl(Zenith_InputControl xControl) const;   // indices, context then binding order
	const Zenith_InputBinding* FindBinding(Zenith_InputContextID xContext, Zenith_InputActionID xAction,
		uint32 uBindingID) const;
	Zenith_Span<const Zenith_InputContextDesc> GetContexts() const;      // ascending ID
	const Zenith_InputContextDesc* FindContext(Zenith_InputContextID xContext) const;
	Zenith_Span<const Zenith_InputContextID> GetDefaultSimulationContexts() const;   // bottom first
	Zenith_Span<const Zenith_ControlPath> GetReservedControls() const;
	const Zenith_BindingSourceFile& GetSourceFile(uint16 uSourceIndex) const;   // provenance
	Zenith_Span<const Zenith_ConfigKey> GetConfigScaleKeys() const;     // indexed by m_uConfigScaleSlot
	const Zenith_InputActionSet& GetActions() const;
};
```

### Composites

| Kind | Parts (file keys, in part order) | Action types | Value |
|---|---|---|---|
| `COMPOSITE_KIND_DPAD` | `up`, `down`, `left`, `right` | AXIS2D, DELTA2D (integrated) | `(right − left, up − down)`, each part's value in [0, 1]; normalised to unit length when both axes are non-zero and `normalise` is true (default) |
| `COMPOSITE_KIND_AXIS` | `negative`, `positive` | AXIS1D | `positive − negative`; with `whichSideWins`: `neither` (default, 0 when both held), `negative`, `positive` |
| `COMPOSITE_KIND_VECTOR3` | `left`, `right`, `down`, `up`, `backward`, `forward` | AXIS3D | `(right − left, up − down, forward − backward)` |

A part may be a button or a half-axis component; its value is its button value or half-axis magnitude.

### Modifiers

- A binding with modifiers actuates only while every modifier control is held (value ≥ 0.5).
- With `modifiersOrdered` true (the default), each modifier's most recent press must precede the main control's actuation in sequence order, so pressing `S` then `Ctrl` never actuates `Ctrl+S`.
- Specificity: among the bindings of one context whose main control is the same control and which actuate on the same transition, only those with the largest modifier count actuate, so `Ctrl+S` suppresses a plain `S` binding in the same context while Ctrl is held.

### Processors

Processors run in declared order after the composite, component and type conversion.

| Kind | File name | Parameters (`m_afParams`) | Function |
|---|---|---|---|
| `PROCESSOR_KIND_AXIAL_DEADZONE` | `AxialDeadzone` | `min` (0.125), `max` (0.925) | per component: `|v| < min` → 0; `|v| > max` → sign(v); else `sign(v) × (|v| − min) / (max − min)` |
| `PROCESSOR_KIND_RADIAL_DEADZONE` | `RadialDeadzone` | `min` (0.125), `max` (0.925) | on the vector's magnitude m: `m < min` → zero vector; else the vector scaled to magnitude `min(1, (m − min) / (max − min))` |
| `PROCESSOR_KIND_SCALE` | `Scale` | `x`, `y`, `z` (1, 1, 1) | component-wise multiply |
| `PROCESSOR_KIND_INVERT` | `Invert` | `x`, `y`, `z` as 0 or 1 | negate the flagged components |
| `PROCESSOR_KIND_NORMALISE` | `Normalise` | `min`, `max`, `zero` | per component `(v − zero) / (max − zero)` for `v ≥ zero`, `(v − zero) / (zero − min)` below, clamped to [−1, 1] |
| `PROCESSOR_KIND_CLAMP` | `Clamp` | `min`, `max` | per component clamp |
| `PROCESSOR_KIND_CURVE` | `Curve` | `exponent` (1) | per component `sign(v) × |v|^exponent` |
| `PROCESSOR_KIND_CONFIG_SCALE` | `ConfigScale` | `key` (a FLOAT configuration key, e.g. `input.lookSensitivity`) | component-wise multiply by the key's value; the input system copies every referenced key's value into the binding table's scale list on the main thread after prologue step 3 (`Zenith_Config` is main-thread only), so a user-layer change applies from the next prologue and a phase-1 job reads only the snapshot |

### Interactions

Interactions apply to BUTTON and AXIS1D actions; value actions use `INTERACTION_KIND_DEFAULT` alone (a file giving another kind to a value action fails with `INPUT_ERROR_INTERACTION_TYPE`). Events: `ACTUATE` (the binding's magnitude rose to `m_fPressPoint` or above), `RELEASE` (it fell below `0.75 × m_fPressPoint`, the release point), `TIMER` (time advanced to the slice end or to a pending deadline), `DISABLE` (the context left the stack, or the binding became consumed). `t` is the time since the last ACTUATE; `r` the time since the last RELEASE.

| Interaction | State | ACTUATE | RELEASE | TIMER | DISABLE |
|---|---|---|---|---|---|
| `DEFAULT` | WAITING | → ACTIVE: STARTED \| PERFORMED | — | — | — |
| | ACTIVE | — | → WAITING: ENDED | — | → WAITING: CANCELLED |
| `PRESS` (press only) | WAITING | → ACTIVE: PERFORMED | — | — | — |
| | ACTIVE | — | → WAITING: ENDED | — | → WAITING: CANCELLED |
| `PRESS` (release only) | WAITING | → ACTIVE: STARTED | — | — | — |
| | ACTIVE | — | → WAITING: PERFORMED \| ENDED | — | → WAITING: CANCELLED |
| `PRESS` (press and release) | WAITING | → ACTIVE: PERFORMED | — | — | — |
| | ACTIVE | — | → WAITING: PERFORMED \| ENDED | — | → WAITING: CANCELLED |
| `HOLD` (duration d) | WAITING | → STARTED: STARTED | — | — | — |
| | STARTED | — | → WAITING: CANCELLED | t ≥ d → PERFORMING: PERFORMED | → WAITING: CANCELLED |
| | PERFORMING | — | → WAITING: ENDED | — | → WAITING: CANCELLED |
| `TAP` (maximum d) | WAITING | → STARTED: STARTED | — | — | — |
| | STARTED | — | t ≤ d → WAITING: PERFORMED \| ENDED | t > d → EXPIRED: CANCELLED | → WAITING: CANCELLED |
| | EXPIRED | — | → WAITING | — | → WAITING |
| `SLOW_TAP` (minimum d) | WAITING | → STARTED: STARTED | — | — | — |
| | STARTED | — | t ≥ d → WAITING: PERFORMED \| ENDED; t < d → WAITING: CANCELLED | — | → WAITING: CANCELLED |
| `MULTI_TAP` (count n, maximum d, spacing s) | WAITING | → TAPPING, count 0: STARTED | — | — | — |
| | TAPPING (held) | — | t ≤ d: count + 1; count = n → WAITING: PERFORMED \| ENDED; else → BETWEEN | t > d → EXPIRED: CANCELLED | → WAITING: CANCELLED |
| | BETWEEN (released) | → TAPPING | — | r > s → WAITING: CANCELLED | → WAITING: CANCELLED |
| | EXPIRED | — | → WAITING | — | → WAITING |

The flags each transition sets are OR-ed into the action's `m_uPhaseFlags` for the slice; the interaction's state carries across slices. The action is `m_bHeld` while its driving binding's magnitude is at or above the press point, independent of the interaction.

### Control schemes

- Every binding has a scheme mask derived from the device kinds of its controls (keyboard and mouse → `KEYBOARD_MOUSE`; gamepad → `GAMEPAD`; touchscreen, pen, gesture → `TOUCH`; a virtual device → the scheme of its `m_eSchemeKind`), or given by the file's `schemes` key.
- Every scheme is live at once: a player may mix keyboard and gamepad; the scheme decides prompts and haptic targets only.
- The active scheme is the scheme of the device whose transition most recently reached `input.schemeSwitchThreshold` (0.5 for axes; any press for buttons; mouse motion counts only when its delta exceeds 4 points in one frame). A change emits `PLATFORM_EVENT_INPUT_SCHEME_CHANGED` (world-visible, so game UI scripts swap prompts) and is exposed by `Zenith_InputSystem::GetActiveScheme`.

### Binding display

```cpp
enum Zenith_GlyphFamily : uint8 { GLYPH_FAMILY_NONE = 0, GLYPH_FAMILY_KEYBOARD, GLYPH_FAMILY_MOUSE, GLYPH_FAMILY_XBOX,
	GLYPH_FAMILY_PLAYSTATION, GLYPH_FAMILY_NINTENDO, GLYPH_FAMILY_TOUCH };

struct Zenith_BindingDisplay
{
	uint32 m_uBindingID;
	uint8 m_uPart;                  // composite part index, 0xFF when not a composite part
	Zenith_ControlPath m_xPath;
	Zenith_GlyphFamily m_eGlyphFamily;
	Zenith_StringView m_strLabel;    // key name in the current layout, gamepad button label, or the path; arena storage
	Zenith_BindingLayer m_eLayer;
	bool m_bRebindable;
};
```

`Zenith_InputSystem::GetBindingDisplay(xAction, xContext, eScheme, xArena)` returns the action's active bindings of that context and scheme, one entry per control or composite part, in binding order, so UI prompts and the remapping screen (Z2-I-UI-12, `Zenith2/Docs/Design/UI.md` (pending; its "Public API" section)) never parse paths themselves.

## Input contexts

### Context stacks

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_InputContext.h

struct Zenith_InputContextID
{
	uint32 m_uValue = 0;            // stable, from the bindings file "id"; engine contexts use 0x00F00000 and above
	constexpr bool IsNull() const { return m_uValue == 0; }
	friend constexpr bool operator==(Zenith_InputContextID, Zenith_InputContextID) = default;
};

enum Zenith_InputContextScope : uint8 { INPUT_CONTEXT_SCOPE_SIMULATION = 1, INPUT_CONTEXT_SCOPE_PRESENTATION };

struct Zenith_InputContextEntry
{
	Zenith_InputContextID m_xContext;
	Zenith_WindowID m_xWindow;       // presentation only: null = every window; otherwise active only while that window has focus
};

/// @contract container Zenith_InputContextStack
/// Ownership: Owned by Zenith_InputSystem (the presentation stack) or by a world's Zenith_SimulationInputContexts resource.
/// Thread affinity: OWNER_THREAD. The simulation stack is written only by command playback at sync points and read by phase 1.
/// Invalidation: GetEntries spans are invalid after the next Push or Remove.
/// Units: None.
/// Errors: Push returns INPUT_ERROR_CONTEXT_STACK_FULL at 16 entries and INPUT_ERROR_UNKNOWN_CONTEXT or
///   INPUT_ERROR_CONTEXT_SCOPE for a context of the other scope; Remove of an absent context is a counted no-op.
/// Version: since Z2 Stage 3. The simulation stack is saved with the world (a reflected resource).
/// Alignment: element alignment of Zenith_InputContextEntry.
/// Overflow: at most uINPUT_MAX_CONTEXT_DEPTH (16) entries.
/// Allocation failure: none; fixed inline storage.
/// Complexity: Push, Remove and Contains O(16).
/// @endcontract
class Zenith_InputContextStack
{
public:
	Zenith_Result<void, Zenith_InputError> Push(Zenith_InputContextEntry xEntry);   // an entry already present moves to the top
	void Remove(Zenith_InputContextID xContext);
	bool Contains(Zenith_InputContextID xContext) const;
	Zenith_Span<const Zenith_InputContextEntry> GetEntries() const;                // bottom first
	void Clear();
};
inline constexpr uint32 uINPUT_MAX_CONTEXT_DEPTH = 16;
```

| Stack | Owner | Changed by | Default content |
|---|---|---|---|
| presentation | `Zenith_InputSystem` | UI (menus push `Engine.UI` or a game presentation context), the editor, the development overlay, `Zenith_TextInput` (`Engine.TextEntry`) — directly, on the main thread, effective at the next Pump | `Engine.Dev` in non-Shipping builds, pinned at the top |
| simulation | the world resource `Zenith_SimulationInputContexts` (one per world) | the commands `Input.PushContext` and `Input.PopContext`, applied at the sync point after the issuing phase and effective from the next phase 1 | the bindings files' `defaultSimulationContexts`, above `Engine.Simulation` at the bottom |

Contexts flagged `pinnedTop` in their file stay above every non-pinned context of their stack regardless of push order; among pinned contexts push order applies. A simulation push of a full stack or of an unknown context is dropped at playback with `input.contexts.push_refused` incremented and a log entry naming the command's issuer, never an assert.

### Consumption rules

1. Evaluation order is: claims (§ "Presentation claims"); then the presentation stack from top to bottom (where `Engine.TextEntry`, pinned at the top while a text session is active, consumes the keyboard through its `consumesDevices` list); then the world's simulation stack from top to bottom.
2. When a binding of a context with `consumes: true` actuates on a transition (the binding's magnitude reaches `input.actuationThreshold`, 0.1, or it is a button press), the controls that drove it are consumed for every context below it until each returns to rest.
3. A consumed transition is seen by the contexts below as that control at rest; a lower binding that held the control therefore releases, so no lower action stays held when a menu opens over it.
4. A release of a control is delivered to every context whose bindings saw its press, even when the control was consumed afterwards.
5. Controls not bound by any higher context pass through unchanged.
6. The presentation mapper marks every transition consumed by the presentation stack or a claim with `INPUT_TRANSITION_FLAG_CONSUMED` (bit 6 of `m_uFlags`); the simulation mapper applies rule 3 to flagged transitions.

### Presentation claims

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_InputSystem.h

struct Zenith_PointerDown             // a new press of a pointer-like control this frame
{
	uint32 m_uTransitionIndex;       // into GetFrameTransitions
	Zenith_InputDeviceID m_xDevice;
	Zenith_InputControl m_xControl;   // mouse button, touch press (with index), pen tip
	Zenith_WindowID m_xWindow;
	float m_fPixelX, m_fPixelY;
};

/// @contract type Zenith_InputClaimSet
/// Ownership: Owned by Zenith_InputSystem; passed by reference to claimants for the duration of one call.
/// Thread affinity: MAIN_THREAD, inside Platform.Pump.
/// Invalidation: Valid for the claimant call alone.
/// Units: None.
/// Errors: An index outside the frame's pointer downs, a pointer already claimed or an unknown action is ignored and
///   counted (input.claims.ignored); nothing asserts.
/// Version: since Z2 Stage 3. Source API.
/// @endcontract
class Zenith_InputClaimSet
{
public:
	void ClaimPointer(uint32 uPointerDownIndex);                    // claimed until the pointer's release
	void ClaimAction(Zenith_InputActionID xAction);                  // this frame: its driving controls are consumed
};

/// @contract interface Zenith_InputClaimant
/// Ownership: Implemented by UI (Zenith2/Docs/Design/UI.md "Public API") and the editor; registered with
///   Zenith_InputSystem::RegisterClaimant by the composition root; unregistered before the implementer is destroyed
///   (the returned Zenith_CallbackToken does it).
/// Thread affinity: MAIN_THREAD. Called inside Platform.Pump.
/// Invalidation: The spans and the frame passed are valid for the call alone.
/// Units: Pixels of the pointer's window.
/// Errors: None.
/// Reentrancy: NONE. A claimant may not call Pump, push presentation contexts effective this frame, or register claimants.
/// Version: since Z2 Stage 3. Source API.
/// @endcontract
class Zenith_InputClaimant
{
public:
	virtual ~Zenith_InputClaimant() = default;
	// Before gestures and mapping: hit-test new pointer presses against last frame's layout.
	virtual void ClaimPointers(Zenith_Span<const Zenith_PointerDown> axDowns, Zenith_InputClaimSet& xClaims) = 0;
	// After the presentation mapper: claim actions the claimant handled (UI.Submit consumed by a focused button).
	virtual void ClaimActions(const Zenith_InputFrame& xFrame, Zenith_InputClaimSet& xClaims) = 0;
};
```

- Claimants run in registration order (UI, then the editor); the first claim of a pointer wins and every subsequent claimant sees it as claimed.
- A claimed pointer's transitions (from its press to its release, inclusive) are flagged `INPUT_TRANSITION_FLAG_CLAIMED` and `INPUT_TRANSITION_FLAG_CONSUMED`, never reach the gesture recognisers, and are seen at rest by the simulation mapper; so a thumb on an on-screen button never also pans the camera.
- An action claim consumes the controls that drove the action this frame, as rule 2 of "Consumption rules" does.
- `Zenith_InputFrame` is the presentation result: the presentation action-state table, the frame's transitions, the active scheme, the presentation stack and the text-input events; UI, the editor and development tools read it after `Platform.Pump` returns.

### Engine actions and contexts

The engine bindings file `content:/Engine/Input/Engine.bindings.json` (source `Zenith2/Engine/ZenithPlatform/Internal/Content/Input/Engine.bindings.json`, cooked as engine content, entry document → "Authoring and cooking") declares:

| Action | ID | Type | Default bindings (context) |
|---|---|---|---|
| `UI.Navigate` | 0x00F00001 | AXIS2D | arrow keys (dpad composite), `/gamepad/dpad`, `/gamepad/leftStick` with a radial deadzone (`Engine.UI`) |
| `UI.Submit` | 0x00F00002 | BUTTON | `/keyboard/return`, `/keyboard/space`, `/gamepad/south` (`Engine.UI`) |
| `UI.Cancel` | 0x00F00003 | BUTTON | `/keyboard/escape`, `/gamepad/east` (`Engine.UI`) |
| `UI.TabNext`, `UI.TabPrevious` | 0x00F00004, 0x00F00005 | BUTTON | `/gamepad/rightShoulder`, `/gamepad/leftShoulder`; `Tab`, `Shift+Tab` (`Engine.UI`) |
| `UI.Scroll` | 0x00F00006 | AXIS2D | `/mouse/wheel`, `/gamepad/rightStick` (`Engine.UI`) |
| `System.Back` | 0x00F00010 | BUTTON | `/keyboard/acBack`, `/keyboard/escape`, `/gamepad/start` (`Engine.Simulation`, so a game with no menu open receives it) |
| `Dev.ToggleProfiler` | 0x00F00100 | BUTTON | `F3`; `/gamepad/back` with the modifier `/gamepad/leftStickPress` (`Engine.Dev`) |
| `Dev.ToggleConsole` | 0x00F00101 | BUTTON | the grave key (`Engine.Dev`) |
| `Dev.ToggleInputOverlay` | 0x00F00102 | BUTTON | `Ctrl+F3` (`Engine.Dev`) |
| `Dev.LatencyProbe` | 0x00F00103 | BUTTON | `/virtual/latencyProbe/pulse` (`Engine.Simulation`, non-Shipping) |

| Context | ID | Scope | Consumes | Pinned |
|---|---|---|---|---|
| `Engine.UI` | 0x00F00001 | presentation | yes | no |
| `Engine.TextEntry` | 0x00F00002 | presentation | every keyboard control through `"consumesDevices": ["Keyboard"]` (no bindings) | yes |
| `Engine.Dev` | 0x00F00003 | presentation, non-Shipping only | yes | yes |
| `Engine.Simulation` | 0x00F00004 | simulation | no (it adds `System.Back` and the latency probe) | no; always the bottom entry |

The development bindings and `Engine.Dev` are absent from Shipping builds: the loader skips every engine action and context whose file entry carries `"developmentOnly": true` when `Zenith_IsShippingBuild()`.

## The action mapper

### Mapper algorithm

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_ActionMapper.h

struct Zenith_ActionMapperInputs
{
	const Zenith_InputBindingTable* m_pxBindings = nullptr;          // the resolved layers
	Zenith_Span<const Zenith_InputContextEntry> m_axStack;          // bottom first; evaluated top first
	Zenith_Span<const Zenith_InputTransition> m_axSlice;            // in sequence order
	Zenith_WindowID m_xFocusWindow;                                  // for window-scoped contexts
	uint64 m_ulSliceStartNs = 0;
	uint64 m_ulSliceEndNs = 0;                                      // TIMER events are evaluated at this time
	uint64 m_ulSliceDurationUs = 0;                                 // the frame time, or the fixed tick duration
	bool m_bSimulation = false;                                      // apply rule 3 of "Consumption rules" to flagged transitions
	const Zenith_InputDevices* m_pxDevices = nullptr;                // for the end-of-slice resynchronisation
	Zenith_Span<const float> m_afConfigScales;                       // PROCESSOR_KIND_CONFIG_SCALE values by slot, copied on the main thread
};

struct Zenith_ActionChange            // one entry per state change, in the order they happened
{
	Zenith_InputActionID m_xAction;
	uint64 m_ulSequence;             // of the transition that caused it; slice-end changes use UINT64_MAX
	Zenith_InputActionState m_xState; // the action's state after the change
};

/// @contract type Zenith_ActionMapper
/// Ownership: A value owned by its user: Zenith_InputSystem (presentation) and each world's input resource (simulation).
/// Thread affinity: OWNER_THREAD. Map is a pure function of its inputs and the mapper's own state.
/// Invalidation: The change span returned by Map is valid until the next Map or until the arena is reset.
/// Units: Times in nanoseconds; durations in microseconds.
/// Errors: None; malformed transitions (a control unknown to the table) are skipped and counted.
/// Version: since Z2 Stage 3. Source API.
/// @endcontract
class Zenith_ActionMapper
{
public:
	static Zenith_Result<Zenith_ActionMapper, Zenith_InputError> Create(const Zenith_InputBindingTable& xBindings,
		Zenith_Heap& xHeap);
	Zenith_Span<const Zenith_ActionChange> Map(const Zenith_ActionMapperInputs& xInputs,
		Zenith_InputActionStateTable& xStates, Zenith_Arena& xArena);
	void Reset();            // every binding to WAITING, every control to rest (world transition, bindings reload)
};
```

`Map` runs these steps:

1. `xStates.ResetSliceFields()`.
2. Build the active context list: the stack's entries from top to bottom, pinned entries first, skipping window-scoped entries whose window is not `m_xFocusWindow`.
3. For each context that left the active list since the previous `Map`: deliver `DISABLE` to its bindings' interactions (their actions get CANCELLED or ENDED per the interaction table) and release the actions they held.
4. For each transition of the slice, in order:
   1. Skip it when `INPUT_TRANSITION_FLAG_REPEAT` is set.
   2. Update the mapper's view of the control: its value, or rest when the transition is flagged consumed and `m_bSimulation` is true (rule 3 of "Consumption rules").
   3. For each active context, top first, whose `consumesDevices` mask (`Zenith_InputContextDesc::m_uConsumedDeviceKindMask`) includes the transition's device kind: mark the control consumed for every context below it (and, in the presentation mapper, flag the transition `INPUT_TRANSITION_FLAG_CONSUMED`), exactly as a consuming binding's actuation does in step 6. Then, for each binding that references the control (the table's control-to-binding index, in context order then binding order), whose context is active and the control is not consumed at that context's level: recompute the binding's value (composite or alternatives, component and type conversion, modifier check and specificity, processors) and its magnitude.
   4. Resolve each affected action: its value is the value of its highest-magnitude actuated binding among active contexts (ties: the higher context, then the earlier binding); DELTA2D actions instead sum the deltas of every actuated binding; AXIS2D-to-DELTA2D bindings contribute `value × m_ulSliceDurationUs / 1e6` once, at step 5.
   5. Feed ACTUATE and RELEASE events to the driving binding's interaction at the transition's timestamp; OR the resulting flags into the action; set `m_bPressed` and `m_bReleased` on held-state changes; append one `Zenith_ActionChange` per changed action with the transition's sequence.
   6. Apply consumption: mark the transition's control consumed for the contexts below any consuming context whose binding actuated.
5. At the slice end: add the integrated AXIS2D-to-DELTA2D contributions; feed `TIMER` at `m_ulSliceEndNs` to every interaction with a deadline; update `m_uHeldMs`; append a change with sequence `UINT64_MAX` for every action whose value changed only through this step.
6. Resynchronise: for every control the mapper holds non-rest whose device value (from `m_pxDevices`) is at rest, or the reverse, apply a synthetic transition at `m_ulSliceEndNs` through step 4 (this repairs any transition dropped by log or carry overflow, and a device that disconnected mid-slice).
7. Return the change list.

Complexity: O(T × k) for T transitions and k bindings per control (k ≤ 16 in practice), plus O(A) at the slice end for A actions. The mapper allocates only from `xArena`.

### Determinism of the mapper

- `Map` reads nothing but its inputs and its own state: no clock, no global, no configuration value other than the `m_afConfigScales` snapshot passed with the call.
- Floating-point operations are performed in the fixed order above, per binding and per action, on the thread's fixed floating-point environment (`Zenith2/Docs/Contracts.md` → "Threading and concurrency contract", "Floating-point environment").
- Determinism of the mapper is a test property (`Test_ActionMapper_SameInputsSameOutputs`); simulation does not depend on it, because simulation consumes the commands the mapper produced, logged and replayed (`Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Input commands and quantisation").

## The bindings file format

### Bindings file schema

A bindings file is UTF-8 JSON under the parser limits of `Zenith2/Docs/Architecture.md` → "Security architecture", at most 1 MiB. Keys are exactly those shown; unknown keys fail validation except keys prefixed `x-`, which every tool that rewrites the file preserves.

```json
{
	"schema": "zenith2.input-bindings",
	"schemaVersion": 1,
	"actions": [
		{ "id": 1, "name": "Gameplay.Move", "type": "Axis2D", "displayName": "@loc:input.move" },
		{ "id": 2, "name": "Gameplay.Jump", "type": "Button" },
		{ "id": 3, "name": "Gameplay.Look", "type": "Delta2D" },
		{ "id": 4, "name": "Gameplay.Interact", "type": "Button", "rebindable": true },
		{ "id": 5, "name": "Gameplay.Pause", "type": "Button", "rebindable": false }
	],
	"retiredActionIds": [],
	"contexts": [
		{
			"id": 1, "name": "Gameplay", "scope": "Simulation", "consumes": true,
			"bindings": [
				{ "id": 1, "action": "Gameplay.Move",
				  "composite": { "kind": "Dpad", "up": "/keyboard/w", "down": "/keyboard/s", "left": "/keyboard/a", "right": "/keyboard/d" } },
				{ "id": 2, "action": "Gameplay.Move", "controls": ["/gamepad/leftStick"],
				  "processors": [ { "kind": "RadialDeadzone", "min": 0.15, "max": 0.95 } ] },
				{ "id": 3, "action": "Gameplay.Jump", "controls": ["/keyboard/space", "/gamepad/south"] },
				{ "id": 4, "action": "Gameplay.Look", "controls": ["/mouse/delta"],
				  "processors": [ { "kind": "Scale", "x": 0.1, "y": 0.1 }, { "kind": "ConfigScale", "key": "input.lookSensitivity" } ] },
				{ "id": 5, "action": "Gameplay.Look", "controls": ["/gamepad/rightStick"],
				  "processors": [ { "kind": "RadialDeadzone" }, { "kind": "Curve", "exponent": 2.0 }, { "kind": "Scale", "x": 3.0, "y": 3.0 } ] },
				{ "id": 6, "action": "Gameplay.Interact", "controls": ["/keyboard/e", "/gamepad/west"],
				  "interaction": { "kind": "Hold", "durationMs": 400 } },
				{ "id": 7, "action": "Gameplay.Pause", "controls": ["/keyboard/escape", "/gamepad/start"] },
				{ "id": 8, "action": "Gameplay.Jump", "controls": ["/gesture/tap"], "schemes": ["Touch"] }
			]
		},
		{ "id": 2, "name": "Vehicle", "scope": "Simulation", "consumes": true, "bindings": [] }
	],
	"retiredContextIds": [],
	"defaultSimulationContexts": ["Gameplay"],
	"reservedControls": ["/gamepad/guide"]
}
```

| Key | Type | Presence | Rule |
|---|---|---|---|
| `schema` | string | always | exactly `zenith2.input-bindings` (`zenith2.input-bindings-platform` in the platform layer) |
| `schemaVersion` | integer | always | `1`; a newer value fails with `INPUT_ERROR_BINDINGS_VERSION` |
| `actions[].id` | integer | always | in the file's ID range; unique across every file of every layer |
| `actions[].name` | string | always | the name grammar of "Action IDs"; unique |
| `actions[].type` | string | always | `Button`, `Axis1D`, `Axis2D`, `Axis3D`, `Delta2D` (`Pose` is rejected in Stage 3) |
| `actions[].displayName` | string | optional | a localisation key `@loc:<key>` or literal text; UI's |
| `actions[].rebindable` | bool | optional (true) | false: no binding of the action can be rebound |
| `actions[].developmentOnly` | bool | optional (false), engine file only | skipped in Shipping |
| `retiredActionIds`, `retiredContextIds` | integer arrays | optional | never reused |
| `contexts[].id`, `.name` | integer, string | always | unique; names match `^[A-Z][A-Za-z0-9.]{0,63}$` |
| `contexts[].scope` | string | always | `Simulation` or `Presentation` |
| `contexts[].consumes`, `.pinnedTop`, `.developmentOnly` | bool | optional (true, false, false) | |
| `contexts[].consumesDevices` | array of strings | optional (empty) | device kinds (`Keyboard`, `Mouse`, `Gamepad`, `Touchscreen`, `Pen`) every control of which the context consumes for the contexts below it, bound or not (mapper step 4.3); presentation contexts only, else `INPUT_ERROR_BINDINGS_SCHEMA` |
| `contexts[].conflictGroup` | string | optional (the context's name) | § "Conflict detection" |
| `contexts[].bindings[].id` | integer | always | 1 to 65,535, unique within its action across every context and layer |
| `.action` | string | always | a declared action name |
| `.controls` | array of paths | exactly one of `controls` and `composite` | 1 to 4 alternatives |
| `.composite` | object | exactly one of `controls` and `composite` | `kind` plus the kind's part keys (table "Composites") and, for `Dpad`, optional `normalise`; for `Axis`, optional `whichSideWins` |
| `.modifiers` | array of paths | optional | at most 3 button or half-axis paths |
| `.modifiersOrdered` | bool | optional (true) | |
| `.processors` | array of objects | optional | at most 8; `kind` plus the parameters of the table "Processors" |
| `.interaction` | object | optional | `kind` (`Default`, `Press`, `Hold`, `Tap`, `SlowTap`, `MultiTap`), `pressPoint`, `durationMs`, `spacingMs`, `tapCount`, `behaviour` (`PressOnly`, `ReleaseOnly`, `PressAndRelease`) |
| `.schemes` | array of strings | optional | `KeyboardMouse`, `Gamepad`, `Touch`; overrides the derived mask |
| `.consumes`, `.rebindable` | bool | optional | override the context's and action's defaults for this binding |
| `defaultSimulationContexts` | array of names | optional (game file only) | pushed, first at the bottom, on every new world's simulation stack |
| `reservedControls` | array of paths | optional | controls no rebinding may assign |

The platform-layer file (`zenith2.input-bindings-platform`) holds `bindings` additions (each naming its `context`) and `overrides` (the user file's record format, § "Persisting user bindings") and may not declare actions or contexts.

### Validation rules

The loader validates every file completely and reports every error of the file in one pass, each as `input bindings: <file>:<line>: <message>` with the line of the offending value, and returns `INPUT_ERROR_BINDINGS_INVALID` when any exists.

| Rule | Error enumerator in the diagnostic |
|---|---|
| JSON parse failure, parser limit exceeded | `INPUT_ERROR_BINDINGS_PARSE` |
| wrong `schema`, newer `schemaVersion` | `INPUT_ERROR_BINDINGS_SCHEMA`, `INPUT_ERROR_BINDINGS_VERSION` |
| a missing required key, a wrong type, an unknown key without `x-` | `INPUT_ERROR_BINDINGS_SCHEMA` |
| duplicate action ID or name, context ID or name, binding ID within an action | `INPUT_ERROR_DUPLICATE_ACTION_ID`, `_ACTION_NAME`, `_CONTEXT_ID`, `_CONTEXT_NAME`, `_BINDING_ID` |
| an ID outside its range; a retired ID declared | `INPUT_ERROR_ID_OUT_OF_RANGE`, `INPUT_ERROR_REUSED_RETIRED_ID` |
| an unknown action or context name in a binding or a list | `INPUT_ERROR_UNKNOWN_ACTION`, `INPUT_ERROR_UNKNOWN_CONTEXT` |
| a path that does not parse; an `/xr/` path | `INPUT_ERROR_UNKNOWN_CONTROL_PATH`, `INPUT_ERROR_UNSUPPORTED_DEVICE_KIND` |
| a control, composite or processor incompatible with the action type | `INPUT_ERROR_TYPE_MISMATCH` |
| an interaction on a value action | `INPUT_ERROR_INTERACTION_TYPE` |
| more than the limits (actions 4,096; contexts 256; bindings 16,384 across every layer; alternatives 4; parts 6; modifiers 3; processors 8) | `INPUT_ERROR_TOO_MANY_ACTIONS`, `_TOO_MANY_CONTEXTS`, `_TOO_MANY_BINDINGS`; `INPUT_ERROR_BINDINGS_SCHEMA` for the per-binding limits |
| `Pose` action type | `INPUT_ERROR_UNSUPPORTED_ACTION_TYPE` |
| a `ConfigScale` key that is not a registered FLOAT key | `INPUT_ERROR_BAD_CONFIG_KEY` |

- A game or engine file that fails validation stops startup with exit code 3 (a startup validation failure, `Zenith2/Docs/Architecture/CompositionRoot.md` → "Exit codes") and the full diagnostic list in the log: shipped bindings are content, and the cook runs the same validator (entry document → "Authoring and cooking"), so a shipped build never meets this.
- A platform-layer file that fails is a content defect handled the same way.
- The user file never stops startup (§ "Persisting user bindings").

### Binding layers and provenance

1. After the configuration is built (startup step 6), the runtime calls `Zenith_InputSystem::LoadBindings(xFiles, xConfig, xIO)`, which reads, in order: the engine file; every `content:/Input/*.bindings.json` in byte order of their paths; every `content:/Input/Platform/<windows|android>/*.bindings.json` in byte order; `user:/config/input.bindings.json`.
2. Actions and contexts come from the engine and game layers; the platform layer adds bindings and overrides; the user layer overrides only.
3. An override replaces a binding's `controls` or composite parts (and optionally `modifiers`), or marks it unbound, keyed by (context ID, action ID, binding ID); it never adds a binding.
4. Each resolved binding records the layer and the file and line that last defined it; `Zenith_InputSystem::GetBindingProvenance(xContext, xAction, uBindingID, xArena)` returns the whole chain (every layer that touched it, with file and line), and the input debug overlay and `--config-dump` show it (entry document → "Frame-dump and config-dump entries").
5. The result is one `Zenith_InputBindingTable`, immutable until the next `LoadBindings` or rebind apply; the presentation mapper and every world's simulation mapper share it, and a table change resets every mapper (`Zenith_ActionMapper::Reset`) at the next prologue.

## Rebinding

### Rebind operation

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_InputSystem.h

enum Zenith_RebindState : uint8
{
	REBIND_STATE_WAITING_FOR_REST = 1,  // every eligible control must return to rest before listening starts
	REBIND_STATE_LISTENING,
	REBIND_STATE_CANDIDATE,             // a control actuated; waiting for its release (m_bWaitForRelease)
	REBIND_STATE_COMPLETED,
	REBIND_STATE_CANCELLED,             // a cancel control was pressed, or CancelRebind was called
	REBIND_STATE_TIMED_OUT,
	REBIND_STATE_CONFLICT,              // completed with a conflict under REBIND_CONFLICT_POLICY_REJECT
};

enum Zenith_RebindConflictPolicy : uint8
{
	REBIND_CONFLICT_POLICY_REJECT = 1,  // the result reports the conflicting bindings and nothing changes
	REBIND_CONFLICT_POLICY_SWAP,        // the conflicting binding receives the rebound binding's previous control
	REBIND_CONFLICT_POLICY_UNBIND_OTHER,
	REBIND_CONFLICT_POLICY_ALLOW,
};

struct Zenith_RebindDesc
{
	Zenith_InputContextID m_xContext;
	Zenith_InputActionID m_xAction;
	uint32 m_uBindingID = 0;
	uint8 m_uPart = 0xFF;                      // a composite part index; 0xFF for the binding's first control
	uint8 m_uSchemeMask = 0;                   // eligible controls: devices of these schemes only
	Zenith_Span<const Zenith_ControlPath> m_axCancelControls;   // empty: /keyboard/escape and /gamepad/start
	uint32 m_uTimeoutMs = 10000;
	float m_fMinActuation = 0.5f;
	bool m_bWaitForRelease = true;
	Zenith_RebindConflictPolicy m_eConflictPolicy = REBIND_CONFLICT_POLICY_REJECT;
};

struct Zenith_InputBindingRef { Zenith_InputContextID m_xContext; Zenith_InputActionID m_xAction; uint32 m_uBindingID; uint8 m_uPart; };

struct Zenith_RebindResult
{
	Zenith_RebindState m_eState;
	Zenith_ControlPath m_xChosen;
	Zenith_Span<const Zenith_InputBindingRef> m_axConflicts;   // valid until the next Pump
};
```

`Zenith_InputSystem::BeginRebind(const Zenith_RebindDesc&) → Zenith_Result<void, Zenith_InputError>` starts the one rebind operation (a second one fails `INPUT_ERROR_REBIND_IN_PROGRESS`; an unknown binding `INPUT_ERROR_UNKNOWN_BINDING`; a non-rebindable one `INPUT_ERROR_NOT_REBINDABLE`); `GetRebindResult()` reports its state each frame; `CancelRebind()` ends it. While the operation runs, the rebind listener sits above every context and consumes every eligible control, so the capture never also triggers an action.

| State | Event | Next state and action |
|---|---|---|
| WAITING_FOR_REST | every eligible control at rest | → LISTENING; start the timeout |
| LISTENING | a cancel control pressed | → CANCELLED |
| LISTENING | an eligible control reaches `m_fMinActuation` (buttons: pressed; axes: the dominant half-axis; mouse position and deltas, touch position and pressure, pen motion: ignored) | a reserved control (`reservedControls`, and `/keyboard/acBack`) is ignored; otherwise → CANDIDATE when `m_bWaitForRelease`, else → evaluate |
| CANDIDATE | the candidate released | → evaluate |
| CANDIDATE, LISTENING | the timeout elapsed | → TIMED_OUT |
| evaluate | conflicts found (below) under `REJECT` | → CONFLICT, with the conflicts |
| evaluate | otherwise | apply per the policy; → COMPLETED; schedule the user-layer write (§ "Persisting user bindings") |

### Conflict detection

A conflict is another binding, not unbound, whose control set contains the chosen control with the same modifier set, in a context that can be active at the same time as the rebound binding's context: the same context, or any context of the same scope sharing its `conflictGroup`. Conflicts across scopes (a presentation and a simulation context) are reported only when both name a common `conflictGroup`. Composite parts of one binding conflict with each other (binding `up` and `down` of one dpad to one key is a conflict).

### Persisting user bindings

```json
{
	"schema": "zenith2.input-bindings-user",
	"schemaVersion": 1,
	"overrides": [
		{ "context": 1, "action": 2, "binding": 3, "controls": ["/keyboard/f", "/gamepad/south"] },
		{ "context": 1, "action": 1, "binding": 1, "part": "up", "control": "/keyboard/z" },
		{ "context": 1, "action": 4, "binding": 6, "unbound": true }
	]
}
```

- The file lives at `user:/config/input.bindings.json` and is written only through `Zenith_AsyncIO::WriteAtomic` (`Zenith2/Docs/Design/PLAT/Filesystem.md` → "Atomic writes"), always at `IO_PRIORITY_LOW` so its writes land in submission order, at most once per `input.bindingsSaveDebounceMs` (1,000) after the last change, on entering the background (`SubmitUserBindings`, never waiting) and at shutdown (`FlushUserBindings`, waiting).
- It is untrusted input: parsed under the JSON limits and a 256 KiB cap; an entry that fails (unknown IDs, a path that does not parse, a type mismatch) is kept in memory as an orphan and re-written unchanged, with one warning per entry per load (`input.bindings.orphans`), never dropped (`Zenith2/Docs/Contracts.md` → "Compatibility policy per kind", "Unknown-field preservation"); unknown keys are preserved.
- A file that fails to parse is renamed to `input.bindings.json.corrupt-<UTC yyyymmddThhmmssZ>` (the newest three kept), the defaults apply, and `PLATFORM_EVENT_CONFIG_RESET` with `m_eLayer = CONFIG_LAYER_USER` and reason `CONFIG_RESET_REASON_INPUT_BINDINGS_CORRUPT` is raised (`Zenith2/Docs/Design/PLAT/Configuration.md` → "Corrupt configuration handling").
- A file with a newer `schemaVersion` is ignored, never rewritten, and backed up to `input.bindings.json.v<N>.bak` before the first rebind would replace it.
- Overrides name context, action and binding IDs and control paths, never device IDs, so they apply to any connected device of the path's kind.

### Reset to defaults

`Zenith_InputSystem::ResetUserBindings(Zenith_RebindResetScope eScope, Zenith_InputBindingRef xTarget)` removes the user-layer overrides of one binding, one action, one context or every context (`REBIND_RESET_SCOPE_BINDING`, `_ACTION`, `_CONTEXT`, `_ALL`), rebuilds the table, and schedules the user-file write; the removed overrides are not kept as orphans.

## Input error enumerations

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_InputTypes.h
enum Zenith_InputError : uint8
{
	INPUT_ERROR_OUT_OF_MEMORY = 1,          // an allocation of the input system failed
	INPUT_ERROR_DEVICE_NOT_FOUND,           // a stale or unknown Zenith_InputDeviceID
	INPUT_ERROR_TOO_MANY_DEVICES,           // the device table or the virtual-device limit is full
	INPUT_ERROR_BAD_VIRTUAL_DEVICE,         // a virtual-device desc with a bad name or too many controls
	INPUT_ERROR_UNKNOWN_CONTROL_PATH,       // a path outside the grammar or the catalogue
	INPUT_ERROR_UNSUPPORTED_DEVICE_KIND,    // an /xr/ path before Stage 7
	INPUT_ERROR_UNSUPPORTED_ACTION_TYPE,    // a Pose action before Stage 7
	INPUT_ERROR_BINDINGS_PARSE,             // a bindings file is not valid JSON or exceeds a parser limit
	INPUT_ERROR_BINDINGS_SCHEMA,            // a wrong schema string, key, type or per-binding limit
	INPUT_ERROR_BINDINGS_VERSION,           // a schemaVersion newer than this build reads
	INPUT_ERROR_BINDINGS_INVALID,           // the summary error of a file with one or more diagnostics
	INPUT_ERROR_DUPLICATE_ACTION_ID,
	INPUT_ERROR_DUPLICATE_ACTION_NAME,
	INPUT_ERROR_DUPLICATE_CONTEXT_ID,
	INPUT_ERROR_DUPLICATE_CONTEXT_NAME,
	INPUT_ERROR_DUPLICATE_BINDING_ID,
	INPUT_ERROR_ID_OUT_OF_RANGE,            // an action or context ID outside the file's range
	INPUT_ERROR_REUSED_RETIRED_ID,
	INPUT_ERROR_UNKNOWN_ACTION,
	INPUT_ERROR_UNKNOWN_CONTEXT,
	INPUT_ERROR_UNKNOWN_BINDING,
	INPUT_ERROR_TYPE_MISMATCH,              // a control, composite or processor incompatible with the action type
	INPUT_ERROR_INTERACTION_TYPE,           // an interaction other than Default on a value action
	INPUT_ERROR_TOO_MANY_ACTIONS,
	INPUT_ERROR_TOO_MANY_CONTEXTS,
	INPUT_ERROR_TOO_MANY_BINDINGS,
	INPUT_ERROR_BAD_CONFIG_KEY,             // a ConfigScale processor names no registered FLOAT key
	INPUT_ERROR_CONTEXT_STACK_FULL,
	INPUT_ERROR_CONTEXT_SCOPE,              // a presentation context pushed on a simulation stack or the reverse
	INPUT_ERROR_REBIND_IN_PROGRESS,
	INPUT_ERROR_NOT_REBINDABLE,
	INPUT_ERROR_TEXT_INPUT_ACTIVE,
	INPUT_ERROR_UNKNOWN_WINDOW,
	INPUT_ERROR_BAD_COMMAND,                // a malformed command value in a replay log or network message
	INPUT_ERROR_INPUT_SCRIPT_INVALID,
	INPUT_ERROR_IO_UNAVAILABLE,             // a user-bindings flush after Zenith_AsyncIO began shutting down (IO_ERROR_SHUTTING_DOWN)
	INPUT_ERROR_RECORDING_STATE,            // BeginScriptRecording while recording, or EndScriptRecording while not
	INPUT_ERROR_NOT_IN_SHIPPING,            // input-script recording in a Shipping build
};
constexpr const char* Zenith_ToString(Zenith_InputError eError);
```

## Input tests

Tests live in `Zenith2/Engine/ZenithPlatform/Tests/` (or the integration suite `Zenith2/Tests/Input/` where the row says so) and run in the `Test` step on `win-debug`, `win-release` and `linux-tsan`; every test below drives devices through `Zenith_PlatformBackendHooks`, virtual devices or injected transitions, so none needs a physical device. The device rows are `needs-human` runs (entry document → "Device tests").

| Test | File | Asserts |
|---|---|---|
| `Test_ControlPath_RoundTripEveryCatalogueEntry` | `Zenith_ControlPath_Tests.cpp` | every path of the catalogue parses and formats back byte-identical; 40 malformed paths return `INPUT_ERROR_UNKNOWN_CONTROL_PATH` |
| `Test_ControlPath_XrPathsReserved` | `Zenith_ControlPath_Tests.cpp` | `/xr/left/trigger` parses, and a binding naming it fails with `INPUT_ERROR_UNSUPPORTED_DEVICE_KIND` |
| `Test_InputDevices_TransitionLogOrderAndSequence` | `Zenith_InputDevices_Tests.cpp` | scripted SDL events produce transitions in queue order with strictly increasing sequences |
| `Test_InputDevices_MotionCoalescingKeepsButtons` | `Zenith_InputDevices_Tests.cpp` | 20,000 motion events and 500 button events in one frame: every button transition survives, motion coalesces (deltas summed exactly), the overflow counters match |
| `Test_InputDevices_FocusLossReleasesHeld` | `Zenith_InputDevices_Tests.cpp` | held keys and buttons receive synthetic releases at the focus-loss timestamp |
| `Test_InputDevices_StickYAxisUp` | `Zenith_InputDevices_Tests.cpp` | an SDL stick y of −32768 reads +1.0 |
| `Test_Mouse_RelativeModeReleasedOnFocusLoss` | `Zenith_InputDevices_Tests.cpp` | relative mode is released on focus loss and restored on focus gain; deltas in relative mode are the raw values |
| `Test_VirtualDevice_CreateSetDestroy` | `Zenith_InputDevices_Tests.cpp` | a virtual device's controls appear as transitions at the next pump in call order; destroying it synthesises releases; a bad desc returns `INPUT_ERROR_BAD_VIRTUAL_DEVICE` |
| `Test_Gamepad_HotplugAddRemove` | `Zenith_Gamepad_Tests.cpp` | add yields connected; remove releases every held control first, then disconnected with the last-of-kind flag |
| `Test_Gamepad_ReconnectWithinWindowKeepsID` | `Zenith_Gamepad_Tests.cpp` | same GUID and serial within 30 s: same device ID and the reconnected flag; after 31 s: a new ID and the old one stale |
| `Test_Gamepad_NinthRejected` | `Zenith_Gamepad_Tests.cpp` | the ninth concurrent gamepad is not opened and is counted |
| `Test_Gamepad_MappingFileBadLinesCounted` | `Zenith_Gamepad_Tests.cpp` | a mapping file with three bad lines loads the good ones and counts three |
| `Test_Gamepad_SensorsEnabledOnlyWhenBound` | `Zenith_Gamepad_Tests.cpp` | the gyro is enabled only while `input.gamepadSensors` is true and an active context binds it |
| `Test_Touch_IndexAssignmentAndPrimary` | `Zenith_Touch_Tests.cpp` | fingers take the lowest free index; the eleventh is ignored; primary passes to the earliest remaining finger without a new press |
| `Test_Touch_CancelledFingerReleasesSynthetic` | `Zenith_Touch_Tests.cpp` | `SDL_EVENT_FINGER_CANCELED` yields a synthetic release |
| `Test_Touch_NoSyntheticMouseEvents` | `Zenith_Touch_Tests.cpp` | a touch produces no mouse transition and a mouse click no touch transition; `input.emulateTouchWithMouse` maps the left button to touch index 0 in a non-Shipping build |
| `Test_Pen_TransitionsAndPressure` | `Zenith_Touch_Tests.cpp` | pen tip, buttons, pressure and tilt produce pen-device transitions with the catalogue's ranges and no touch or mouse transition |
| `Test_Gesture_TapStateTable` | `Zenith_Gestures_Tests.cpp` | every cell of "Tap state table" through recorded touch sequences |
| `Test_Gesture_DoubleTapStateTable` | `Zenith_Gestures_Tests.cpp` | every cell of "Double tap state table"; a pending tap is released when double tap fails and dropped when it succeeds; a second finger down farther than `doubleTapSlopDp` emits the first tap at that DOWN and is itself recognised as a tap on its release |
| `Test_Gesture_LongPressStateTable` | `Zenith_Gestures_Tests.cpp` | every cell of "Long press state table" |
| `Test_Gesture_PanAndSwipe` | `Zenith_Gestures_Tests.cpp` | the pan table; swipe thresholds at the limit and limit ± 1 for distance, duration and velocity; each direction and the horizontal tie |
| `Test_Gesture_PinchRotateTwoFingerTap` | `Zenith_Gestures_Tests.cpp` | the two-finger table; pinch and rotate together; a second finger cancels a began pan |
| `Test_Gesture_ThresholdsInDpFollowDisplayScale` | `Zenith_Gestures_Tests.cpp` | the same pixel movement recognises at scale 1 and not at scale 3.5, where it is below the slop in dp |
| `Test_Gesture_DeterministicReplay` | `Zenith_Gestures_Tests.cpp` | a recorded 10,000-transition touch session yields byte-identical gesture transitions on two runs |
| `Test_Gesture_ClaimedTouchIgnored` | `Zenith_Gestures_Tests.cpp` | a pointer claimed in `ClaimPointers` produces no gesture |
| `Test_TextInput_CompositionThenCommit` | `Zenith_TextInput_Tests.cpp` | editing events produce composition events with cursor and selection, then one commit; ending mid-composition produces COMPOSITION_END and no commit |
| `Test_TextInput_InvalidUtf8Replaced` | `Zenith_TextInput_Tests.cpp` | invalid sequences become U+FFFD and are counted; NUL removed |
| `Test_TextInput_ConsumesKeyboardForSimulation` | `Zenith_TextInput_Tests.cpp` | with a session active, `Engine.TextEntry` is pinned at the top, every keyboard transition is flagged `INPUT_TRANSITION_FLAG_CONSUMED` through its `consumesDevices` list, a key bound in a simulation context emits no command, a gamepad press still does, and the raw transition stays readable; `End` removes the context |
| `Test_Bindings_ConsumesDevicesRules` | `Zenith_BindingsFile_Tests.cpp` | `consumesDevices` on a presentation context consumes every control of the kind for lower contexts, bound or not; on a simulation context it fails with `INPUT_ERROR_BINDINGS_SCHEMA` |
| `Test_TextInput_SecondBeginFails` | `Zenith_TextInput_Tests.cpp` | `INPUT_ERROR_TEXT_INPUT_ACTIVE` |
| `Test_Haptics_MixMaxAcrossChannels` | `Zenith_Haptics_Tests.cpp` | two channels mix to the per-motor maximum times the intensities; SDL rumble is called with the minimum remaining duration |
| `Test_Haptics_PriorityRefusesLower` | `Zenith_Haptics_Tests.cpp` | a lower-priority effect on an occupied channel is refused and counted |
| `Test_Haptics_GlobalIntensityZeroDisables` | `Zenith_Haptics_Tests.cpp` | `input.hapticsIntensity = 0` issues zero-amplitude rumble only |
| `Test_Haptics_StopAllOnSuspendAndFocusLoss` | `Zenith_Haptics_Tests.cpp` | entering SUSPENDING and focus loss stop every effect |
| `Test_Haptics_CommandsRolledBackPlayNothing` | `Zenith2/Tests/Input/Zenith_InputIntegration_Tests.cpp` | a script that calls `Input.PlayHaptic` and then errors submits nothing; a successful one submits once, in merge order |
| `Test_Action_TypeConversionTable` | `Zenith_InputBindings_Tests.cpp` | every allowed cell converts as stated; every — cell fails with `INPUT_ERROR_TYPE_MISMATCH` |
| `Test_Binding_CompositesDpadAxisVector3` | `Zenith_InputBindings_Tests.cpp` | dpad normalisation on and off, axis `whichSideWins` for each value, vector3 part order |
| `Test_Binding_ModifiersOrderedAndSpecificity` | `Zenith_InputBindings_Tests.cpp` | Ctrl then S actuates Ctrl+S and not S; S then Ctrl actuates S alone; an unordered modifier binding accepts both |
| `Test_Binding_ProcessorsEachKind` | `Zenith_InputBindings_Tests.cpp` | each processor against analytic values, including the deadzone limits and a `ConfigScale` change applied at the next prologue |
| `Test_Interaction_EveryStateTableCell` | `Zenith_Interactions_Tests.cpp` | every cell of the interaction table for each kind, at durations of limit − 1, limit and limit + 1 ms |
| `Test_Scheme_ActiveSchemeSwitchesOnThreshold` | `Zenith_InputBindings_Tests.cpp` | a stick below 0.5 does not switch, above does; small mouse motion does not; the event is world-visible |
| `Test_BindingDisplay_LabelsAndGlyphs` | `Zenith_InputBindings_Tests.cpp` | a PS5 pad yields the PlayStation family and "Cross" for south; a key yields its layout label |
| `Test_Context_ConsumptionReleasesLowerHeld` | `Zenith_InputContext_Tests.cpp` | a presentation context binding the stick, pushed while gameplay moves, releases the gameplay move (rule 3) and restores it on pop with no stuck value |
| `Test_Context_ReleaseReachesContextsThatSawPress` | `Zenith_InputContext_Tests.cpp` | rule 4 of "Consumption rules" |
| `Test_Context_PinnedTopAndWindowScoped` | `Zenith_InputContext_Tests.cpp` | a pinned context stays on top; a window-scoped context is inactive while another window has focus |
| `Test_Context_StackFullRefused` | `Zenith_InputContext_Tests.cpp` | the seventeenth push returns `INPUT_ERROR_CONTEXT_STACK_FULL`; a push of a present context moves it to the top |
| `Test_Claims_PointerClaimRemovesFromSimulation` | `Zenith_InputSystem_Tests.cpp` | a claimed touch press, motion and release are flagged and produce no simulation command |
| `Test_Claims_ActionClaimConsumesControls` | `Zenith_InputSystem_Tests.cpp` | `ClaimAction(UI.Submit)` consumes the south button for the simulation mapper that frame |
| `Test_ActionMapper_SameInputsSameOutputs` | `Zenith_ActionMapper_Tests.cpp` | two mappers over the same 100,000-transition recorded session yield byte-identical change lists |
| `Test_ActionMapper_ResyncRepairsDroppedRelease` | `Zenith_ActionMapper_Tests.cpp` | a dropped release is repaired at the slice end from device state |
| `Test_ActionMapper_ContextRemovalCancelsInteraction` | `Zenith_ActionMapper_Tests.cpp` | removing a context mid-hold yields CANCELLED and a release |
| `Test_BindingsFile_EachValidationRule` | `Zenith_BindingsFile_Tests.cpp` | one fixture per row of "Validation rules" yields its enumerator and the file and line in the diagnostic; a file with three errors reports all three |
| `Test_BindingsFile_LayerOrderAndProvenance` | `Zenith_BindingsFile_Tests.cpp` | engine, game, platform and user layers apply in order; the provenance chain names every layer with file and line |
| `Test_BindingsFile_ShippingSkipsDevelopmentOnly` | `Zenith_BindingsFile_Tests.cpp` (Shipping test build) | `Engine.Dev` and the development actions are absent |
| `Test_Rebind_StateTableEveryCell` | `Zenith_Rebind_Tests.cpp` | every row of the rebind table: waiting for rest, cancel, timeout, reserved control ignored, motion ignored, candidate release |
| `Test_Rebind_ConflictPoliciesEach` | `Zenith_Rebind_Tests.cpp` | reject reports the conflict and changes nothing; swap exchanges; unbind-other unbinds; allow duplicates; composite parts conflict with each other |
| `Test_Rebind_CaptureTriggersNoAction` | `Zenith_Rebind_Tests.cpp` | the key pressed to rebind produces no action change in any context |
| `Test_UserBindings_PersistAndReload` | `Zenith_UserBindings_Tests.cpp` | a completed rebind writes the file once after the debounce through `WriteAtomic`; a reload reproduces the table |
| `Test_UserBindings_OrphansPreserved` | `Zenith_UserBindings_Tests.cpp` | an override for an unknown binding survives a rewrite byte for byte and warns once per load; unknown keys survive |
| `Test_UserBindings_CorruptFileQuarantined` | `Zenith_UserBindings_Tests.cpp` | an unparsable file is renamed with the timestamp, three are kept, defaults apply and `PLATFORM_EVENT_CONFIG_RESET` is raised |
| `Test_UserBindings_NewerVersionNotOverwritten` | `Zenith_UserBindings_Tests.cpp` | a version-2 file is ignored and backed up before the first rebind replaces it |
| `Test_UserBindings_ResetScopes` | `Zenith_UserBindings_Tests.cpp` | reset of a binding, an action, a context and everything removes exactly those overrides |
| `Test_InputCommand_EncodeDecodeEveryType` | `Zenith_InputCommand_Tests.cpp` | round trip of every type at the range limits, rounding of halves, clamping; malformed values return `INPUT_ERROR_BAD_COMMAND` |
| `Test_InputCommand_EncoderTracksLastEmitted` | `Zenith_InputCommand_Tests.cpp` | a stick held at one value across 10 ticks emits one AXIS2D command; a change below the quantisation step emits none; an AXIS command dropped at the cap is re-emitted at the next tick and then reaches `m_xStates`; after `Reset`, an action that was non-rest reads rest in `m_xStates` and a held stick is emitted again; the encoder's last value equals `m_xStates` after every tick |
| `Test_InputCommand_EmissionOrderAndCap` | `Zenith_InputCommand_Tests.cpp` | a press and release inside one tick emit two commands in order; 5,000 changes emit 4,096 with buttons kept |
| `Test_InputCommand_LiveEqualsReplay` | `Zenith2/Tests/Input/Zenith_InputIntegration_Tests.cpp` | a 600-tick recorded run and its replay produce bit-identical action-state tables every tick |
| `Test_InputPhase_FirstTickTakesFrameInput` | `Zenith2/Tests/Input/Zenith_InputIntegration_Tests.cpp` | with three catch-up ticks, tick 1 gets the edges and ticks 2–3 none; held state carries |
| `Test_InputPhase_ZeroTickFramesCarry` | `Zenith2/Tests/Input/Zenith_InputIntegration_Tests.cpp` | a tap during a T = 0 frame reaches the next tick; while paused nothing is carried and resynchronisation fixes held state |
| `Test_InputPhase_OnlyInputWorldReceives` | `Zenith2/Tests/Input/Zenith_InputIntegration_Tests.cpp` | an edit world and a play world: only the play world emits commands |
| `Test_InputPhase_ContextCommandsApplyNextTick` | `Zenith2/Tests/Input/Zenith_InputIntegration_Tests.cpp` | `Input.PushContext` from phase 2 changes mapping from the next tick's phase 1, identically with 1, 2, 8 and all workers |
| `Test_InputScript_ActionAndControlEvents` | `Zenith_InputScript_Tests.cpp` | action events become commands at their tick; control events flow through the presentation mapper and claims; exclusive mode ignores physical devices |
| `Test_InputScript_LoadsFromGrantOrContentPath` | `Zenith_InputScript_Tests.cpp` | `LoadInputScript` reads through a command-line grant or a content path and has no raw host-path form (a compile-time check of the overload set) |
| `Test_UserBindings_FlushBeforeIOShutdown` | `Zenith_UserBindings_Tests.cpp` | a pending user-bindings write flushed by `FlushUserBindings` before `Zenith_AsyncIO` is destroyed lands on disk; a flush after the I/O system began shutting down returns `INPUT_ERROR_IO_UNAVAILABLE`, logs, and leaves the old file intact |
| `Test_InputValue_PoseRoundTrip` (Stage 7) | `Zenith_InputCommand_Tests.cpp` | a POSE state (position and orientation) survives encode, decode and `Zenith_ApplyInputCommands` within the encoding's precision, and every other type's encoding is byte-identical to Stage 3's |
| `Test_Bindings_InteractionProfileKey` (Stage 7) | `Zenith_BindingsFile_Tests.cpp` | a platform-layer file with `interactionProfile` loads its bindings for that profile only; an unknown profile is a diagnostic; a file without the key loads unchanged |
| `Test_InputDevices_CarryFrameTransitions` | `Zenith_InputDevices_Tests.cpp` | `CarryFrameTransitions` appends the frame's transitions with final flags; overflow drops the oldest motion first and counts; `ClearCarriedTransitions` after a `RunFrame` in which the input world ran a tick empties the region |
| `Test_InputScript_LimitsAndBadFile` | `Zenith_InputScript_Tests.cpp` | non-decreasing tick order enforced; limits at limit and limit + 1; a bad file returns `INPUT_ERROR_INPUT_SCRIPT_INVALID` |
| `Test_InputScript_Version2Bits` | `Zenith_InputScript_Tests.cpp` | a version 2 action event with `bits` yields a command with exactly those 8 bytes (a BUTTON with tap count 2 and held 350 ms, an AXIS1D with phase flags); `bits` of 15 or 17 digits, non-hex digits, bytes `Zenith_DecodeInputValue` refuses, or `bits` in a version 1 file each fail with `INPUT_ERROR_INPUT_SCRIPT_INVALID`; a version 1 file loads as before |
| `Test_InputScript_RecorderLimitsAndStates` | `Zenith_InputScript_Tests.cpp` | `EndScriptRecording` before `BeginScriptRecording` and a second `BeginScriptRecording` return `INPUT_ERROR_RECORDING_STATE`; 262,145 recorded commands keep 262,144 and count 1 in `input.script.record_dropped`; a recording whose document would exceed 16 MiB is cut at a whole tick, still loads with `LoadInputScript`, and counts the cut; a POSE-component command is skipped; the Shipping test build returns `INPUT_ERROR_NOT_IN_SHIPPING` |
| `Test_PlatformDevToolsArea_RecordCommands` | `Zenith2/Engine/ZenithRuntime/Tests/Zenith_PlatformDevToolsArea_Tests.cpp` (`ZenithRuntime_Tests`) | over a console with `Zenith_TestDevFileWriter`: `input.recordScript Acceptance/Playthrough.replay` starts a recording, a second one replies `input.recordScript: already recording`, `input.stopRecordScript` writes one file under that name and replies with the event count, a stop with nothing recording replies `input.stopRecordScript: not recording`, `input.recordScript ../x` replies `input.recordScript: bad name` and writes nothing; with null services no command is registered |
| `Test_InputScript_RecordReplayRoundTrip` | `Zenith2/Tests/Input/Zenith_InputIntegration_Tests.cpp` | a scripted 600-tick session (keyboard and gamepad transitions through backend hooks, a BUTTON with a hold interaction, an AXIS2D and a DELTA2D action) recorded with `input.recordScript` and stopped with `input.stopRecordScript` writes a version 2 file through the test file writer; replaying it from startup with `--input-script` and `exclusive: true` gives per-tick command lists (action ID, value bytes) identical to the recorded run's; one world-transition swap at tick 300 in both runs keeps the ticks continuous (an event recorded at input tick 450 applies at input tick 450 after the swap) |
| `Test_Latency_ProbeSequenceReachesSummary` | `Zenith2/Tests/Input/Zenith_InputIntegration_Tests.cpp` | an injected probe produces a `Dev.LatencyProbe` command and its sequence in `Zenith_InputTickSummary` |
| `Test_InputScriptApi_UnknownActionRaisesAtLoad` | `Zenith2/Tests/Input/Zenith_InputIntegration_Tests.cpp` | a module naming an undeclared action fails to load with the unknown-name misuse error |
| `Test_InputScriptApi_ExposesActionsOnly` | `Zenith2/Tests/Input/Zenith_InputIntegration_Tests.cpp` | the sealed binding table's `Input` namespace holds exactly the functions of `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Script-facing input API": no function takes or returns a device, a control path or a raw transition |
| `Test_InputSystem_DebugSnapshotRows` | `Zenith_InputSystem_Tests.cpp` | `WriteDebugSnapshot` fills every row of the input debug overlay (devices, scheme, both stacks, action values and phases, the last 32 transitions, claims, gesture states, command and drop counters) |
| `Zenith_Fuzz_BindingsJson` | `Tests/Fuzz/BindingsJson.cpp` | libFuzzer over the bindings loader (all three schemas): no crash, no hang, every rejection a `Zenith_InputError` |
| `Zenith_Fuzz_InputScript` | `Tests/Fuzz/InputScript.cpp` | libFuzzer over the input-script loader |
| `Zenith_Fuzz_InputCommandDecode` | `Tests/Fuzz/InputCommandDecode.cpp` | libFuzzer over `Zenith_DecodeInputValue` and `Zenith_ApplyInputCommands` with arbitrary bytes |
| S24 device run: touch and gestures | device test (entry document → "Device tests") | every gesture recognised by hand on the S24 display with the default thresholds; claimed on-screen controls never pan |
| S24 and desktop device run: gamepad hotplug and haptics | device test | a Bluetooth and a USB controller connect, disconnect and reconnect mid-play on the S24 and on Windows; rumble and trigger rumble are felt; the vibrator responds in touch play |
| Windows and S24 device run: IME and soft keyboard | device test | Japanese and Chinese IME composition inline in a text field with candidates at the caret on Windows; each Android soft-keyboard purpose shows its layout on the S24 |
