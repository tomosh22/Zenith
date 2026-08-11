# Input System

## Overview

Three layers, bottom to top, plus one persisted setting:

| Layer | Class | Held on | What it answers |
|---|---|---|---|
| **Device** | `Zenith_Input` | `g_xEngine.Input()` | "is SPACE down", "what did the wheel do", "where is the mouse" |
| **Pointers** | `Zenith_Pointers` | `g_xEngine.Pointers()` | "which fingers are on the glass, where, and who claimed them" |
| **Actions** | `Zenith_InputActions` | `g_xEngine.Actions()` | "is JUMP held" — a named verb, resolved through the binding table by the active profile's scheme mask |
| **Settings** | `Zenith_UserSettings` | `g_xEngine.UserSettings()` | "which profile did the player pin last time" (`Zenith/Core/`) |

A game asks the ACTION layer. Only a game's own `<Game>_Bindings.h` — the one
production file per game allowed to spell a raw key, mouse button or pad code —
names devices, and even that is a table, not a poll.

Every one of these is a PLAIN INSTANCE CLASS. The engine owns exactly one of
each and injects dependencies at `Initialise`; the classes never name
`g_xEngine` themselves, so a unit test constructs LOCAL instances and can never
collide with a game's registrations on the engine's.

## Files

| File | What |
|---|---|
| `Zenith_Input.h/cpp` | The device layer: the platform FIFO, the drain, the ordered transition log, gamepads |
| `Zenith_KeyCodes.h` | GLFW-compatible key / mouse / pad constants |
| `Zenith_Pointers.h/cpp` | The pointer table (8 slots, generation handles, claims, taps) |
| `Zenith_InputActions.h/cpp` | The action layer (actions, bindings, profiles, activity, virtual sources) |
| `Zenith_InputSimulator.h/cpp` | Test-only injection + frame stepping |
| `Zenith_Input.Tests.inl` | Device-layer units (FIFO pressure, reconcile, lifecycle, B3, gamepads) |
| `Zenith_Pointers.Tests.inl` | Pointer-table units (LOCAL instances; never the engine's) |
| `Zenith_InputActions.Tests.inl` | Action-layer units (LOCAL instances + injected local device/pointer layers) |

The persisted profile override lives outside this directory, in
`Core/Zenith_UserSettings.{h,cpp}` — see [Persisted settings](#persisted-settings-zenith_usersettings).

## THE FRAME CONTRACT (steps 1-13)

This ordering is the single source of truth for the whole input stack. The call
sites are `Zenith_Core::Zenith_MainLoop`, `Zenith_UISystem::UpdateInput` and
`Zenith_UICanvas`; nothing else may close, consume or reorder input.

| # | Step | Who | Notes |
|---|---|---|---|
| 1 | Platform pump | `Zenith_Core::BeginFrame_Platform` | renderer begin + timers + `glfwPollEvents` (Windows) / the `ALooper` loop (Android) + the gamepad **poll-diff**. Platform callbacks **only enqueue into the FIFO** — they change no state. |
| 2 | Acquire swapchain | `AcquireSwapchainOrSkip` | On a skip the frame **RETURNS**. The FIFO and the retained pad snapshot persist, so nothing is lost across skipped frames — and no game logic ever misses input it was about to be shown. |
| 3 | **The DRAIN** | `Input().BeginFrame()` | Per-frame edges reset, then the FIFO is consumed **IN ARRIVAL ORDER** into the held tables, the press/release edges, the wheel, the staged touch stream and the **ordered transition log**. |
| 4 | Pointer platform pass | `Pointers().ApplyPlatform()` | Consumes the staged touch stream + the B3 mouse projection. |
| 5 | Editor / tuning | `UpdateEditorAndTuning` | Tools-only; derives the render-submit and game-logic gates. |
| 6 | Automated-test pump | `PumpAutomatedTest` | Runs this frame's test `Step`, which may inject input. |
| 7 | Injection | `Input().ApplySimulatorInjection()` + `Pointers().ApplyInjection()` | Everything a Step injected becomes ordered transitions, so simulated and real input reach the action layer through **exactly one path**. Input PULLS from the simulator (the harness never reaches the engine singleton). |
| 8 | **`Actions().UpdateProfile()`** | action layer | Activity detection over this frame's transitions → auto profile switch / forced override. On ANY mask change the MASK-CHANGE SYNTHESIS runs here, BEFORE any replay. Also **opens the action frame**: per-action edges cleared, source shadow snapshotted. |
| 9 | Per-context binding sync | game code | The gap between a settled profile and the first close. Games retarget on-screen controls / swap HUD contexts here (Zenithmon's `ZM_TouchLayoutController`). |
| 10a | Canvas layout refresh | `Zenith_UICanvas` | FIRST — a rotation or resize this frame must never be hit-tested against last frame's bounds. |
| 10b | **`Actions().FinalizeReservedUI()`** + focus nav | `Zenith_UISystem` | Closes ONLY the engine-reserved UI actions (ids 0-15), then every visible canvas consumes them. Runs BEFORE the capture walk, so a widget claim can never suppress UI navigation. |
| 10c | Standard-control capture walk | `Canvas::UpdatePointerInput` | Widgets take pointer CLAIMS here, in authored pre-order; first claim wins. |
| 10d | Virtual-producer walk | `Canvas::UpdateVirtualControls` | B9 on-screen stick/buttons claim and publish. AFTER 10c so a real widget wins a contested pointer; BEFORE 10e so the transitions replay in the frame they happened. Deliberately **visibility-BLIND**. |
| 10e | **`Actions().FinalizeGameplay()`** | action layer | Closes everything else, with claim suppression applied. |
| 11 | Game logic | `UpdateGameLogic` | All action state is closed; claims and suppression are final. |
| 12 | Render submit | `SubmitRenderWork` | May be skipped by the editor; input state is already final, so a skipped render frame can never change it. |
| 13 | End frame / present | `EndFrameSubmitAndPresent` | |

Two properties this ordering buys, both load-bearing:

* **The drain sits AFTER the acquire gate.** A frame the swapchain skips must
  not consume input no game logic will ever see.
* **The UI input phase is independent of the UI VISUAL pass.** A scene
  transition can skip rendering; it must still answer an arrow key.

## Device layer (`Zenith_Input`)

### The FIFO (B2)

`Zenith_InputEventQueue`, capacity 256. Deliberately NOT `Zenith_CircularQueue`:
it has to evict from the MIDDLE.

* **Coalescing.** A redundant touch MOVE merges into the pending MOVE for the
  same pointer — but only when no DOWN/UP/CANCEL for that pointer sits between
  it and the tail, or the merge would reorder a gesture. The merged entry keeps
  the chain's ANCHOR and the running MAX excursion, so a finger that wanders and
  comes back still reports that it moved.
* **Under pressure**, in order: evict the oldest MOVE (lossless) → evict the
  oldest PRESS → drop the oldest entry. The last two corrupt the held tables, so
  both raise a **reconcile request** the drain acts on.
* **Reconcile policy** is per platform, because the two device families differ
  in whether an oracle exists: Windows is `INPUT_RECONCILE_RESYNC_POLLABLE` (ask
  the live device and synthesize the lost releases); Android is
  `INPUT_RECONCILE_CANCEL_EVENT_FED` (there is nothing to ask — cancel).

### The drain (step 3)

Resets the per-frame edges, then consumes the FIFO in arrival order into:
the held tables (`m_abKeyHeld` — **event-fed logical state, not a device poll**,
because a poll cannot be cancelled by a lifecycle barrier or corrected after an
overflow), the press/release edges, the wheel accumulator, the **staged touch
stream** and the **ordered transition log** (`GetTransitionCount` /
`GetTransition`, max 64) that the action layer replays.

Mouse buttons occupy key slots 0-7, so `MOUSE_*` and `KEY_*` events differ only
in what they tell a consumer about the originating device; both land in the same
held table, and `IsMouseButtonHeld` is literally `IsKeyDown`.

### Lifecycle barriers

`INPUT_EVENT_LIFECYCLE_RESET` (focus loss / pause / window teardown) releases
every held key, button and pad button — emitting the release edges and
transitions — cancels every live pointer, zeroes the pad axes, and **DISARMS**
the layer: ordinary events are discarded until `INPUT_EVENT_LIFECYCLE_ARM`. The
logical pad state and the physical polled baseline deliberately diverge while
disarmed, so a button held across a pause does not fire a phantom press on
resume — it stays released until a fresh transition arrives.

### Gamepads

Polled once per frame inside the platform pump (step 1) and **diffed** into
events, so a pad tap on a frame the swapchain later skips still reaches the
FIFO. Sticks are canonical `[-1,1]` with a **radial** deadzone applied at the
getter (a per-axis test carves a square hole and leaks diagonal drift);
triggers are normalised to `[0,1]` with REST = 0 **once**, at the device layer —
the old per-getter `(v+1)*0.5` made a DISCONNECTED pad's zeroed axis read 0.5.

## ★ B3: the primary pointer IS the mouse view — PERMANENT

**This projection is a design decision, not a migration scaffold. No future
cleanup may delete it.**

* On a pointing-device platform the **mouse feeds pointer 0**.
* On Android the **FIRST touch feeds the mouse view** — position plus
  `ZENITH_MOUSE_BUTTON_LEFT` held / press / release — maintained at drain, and
  reported through `GetProjectedPointerPosition` / `GetPrimaryPointerId`.
  A SECOND finger never steals it; secondary fingers exist only in the pointer
  table. Any touch event at all in a frame suppresses the mouse projection in
  the other direction, so the two can never both open a pointer for one finger.

It is what keeps **every `INPUT_BINDING_MOUSE_BUTTON` row and every unmigrated
`IsMouseButtonHeld` consumer working on a touch device** — which is most of the
engine's editor and gameplay code, and all of Combat and RenderTest, neither of
which registers a TOUCH profile at all. Deleting it would not "simplify" the
stack; it would silently make every mouse-bound action dead on Android while
every test on the desktop stayed green.

The projection is claim-aware where it matters: a MOUSE_BUTTON transition fed by
a pointer is claim-filtered at 10e exactly like a real Windows mouse button, so
a tap a UI widget consumed can never also reach gameplay.

Pinned by `Zenith_Input.Tests.inl` (`B3` cases) and
`Zenith_Pointers.Tests.inl:240`, both of which say PERMANENT in the test itself.

## Pointers (`Zenith_Pointers`)

Replaced the old single-touch `Zenith_TouchInput` gesture subsystem outright
(swipe detection went with it — it had no consumers).

* **8 slots.** A pointer is a finger, or — per B3 — the mouse.
* **Handles are `(slot, generation)`.** A slot recycles the frame AFTER its
  pointer ended, generation bumped, so a stale handle is detectable and a
  stale-generation claim release is rejected.
* **Coordinates are RAW SURFACE PIXELS.** Canvas / editor-viewport remapping
  belongs to the UI layer (`Zenith_UIElement::TransformSurfacePosition`).
* `ApplyPlatform()` (step 4) consumes the drain's staged touch stream + the
  mouse projection; `ApplyInjection()` (step 7) consumes only the tail an
  automated-test Step added after step 4. The window-free cores (`BeginFrame` /
  `ApplyEvent` / `ApplyMouseState` / `ConsumeStagedTail`) are public so units can
  drive a frame with no window.
* **Claims:** `ClaimPointer` / `ReleaseClaim` / `IsClaimed` /
  `ReleaseAllClaimsForOwner`. FIRST claim wins. A claim survives the terminal
  UP/CANCEL frame so its owner can see the edge. Owner tokens are `u_int64`; UI
  widgets use their own address.
* `WasTapThisFrame()` = an UNCLAIMED pointer, down ≤ 0.3 s, max excursion ≤ 15 ×
  `GetDisplayScale()` px. A claimed pointer is a consumed one and never taps.
* A `LIFECYCLE_RESET` barrier cancels every live pointer (raising CANCEL edges)
  and disarms the table until the device layer re-arms.

## Actions (`Zenith_InputActions`)

`Initialise(Zenith_Input&, Zenith_Pointers&)` injects both sources, so this TU
never names the engine singleton.

### Actions and bindings

* **Actions** are `(id, name, kind)`, kind ∈ BUTTON / AXIS1D / AXIS2D. Ids
  **0-15 are engine-reserved** (`UI_NAV_UP/DOWN/LEFT/RIGHT`, `UI_CONFIRM`);
  game ids start at `uINPUT_ACTION_FIRST_GAME_ID` (16). `FindActionByName` is
  how a Behaviour Graph node names one.
* **Bindings** are rows of the table, built with constexpr factories:
  `KeySet` / `KeyAxis1D` / `KeyAxis2D` / `MouseButton` / `MouseWheel` /
  `MouseDelta` / `TouchPrimary` / `SystemBack` / `GamepadButton` /
  `GamepadStick` / `GamepadAxis` / `GamepadAxisAsButton` (press/release
  hysteresis) / `GamepadTriggerPair` (RT−LT) / `GamepadDpadAxis2D` / `Virtual`.
  Max 4 rows per action. `RegisterBinding` asserts the row can produce the
  action's kind, and REJECTS a pointer-sourced or virtual row on a reserved
  action (those close at 10b, before any claim exists, so a claim rule could not
  apply to them).

### Schemes, profiles and masks

`Zenith_EInputScheme { KEYBOARD, MOUSE, TOUCH, GAMEPAD }` are the binding-table
COLUMNS. A **profile** is a named mask over them. Resolution consults ONLY the
active profile's mask.

* **A scheme may appear in at most ONE registered profile** (asserted): the
  auto-switch resolves a scheme to a profile, and two answers is not a
  resolution.
* The engine registers a default profile at boot — Windows
  `EngineDesktop {KB|MOUSE|PAD}`, Android `EngineTouch {TOUCH}` — and **the
  FIRST game `RegisterProfile` call clears the defaults wholesale**. A game
  therefore registers ALL its profiles or none; one that registered a single
  profile would be left with exactly that one and every scheme outside it would
  go dead.
* A profile's **NAME is its stable identity** — `FindProfileByName` /
  `GetProfileName`. Ids are a game's private numbering and may be reordered
  freely; the persisted setting keys off the name for exactly that reason.

### AUTO switching (activity)

Follows ACTIVITY, never held state: press edges, the wheel, a 3 px
mouse-movement CROSSING (Windows), a pointer DOWN, and a pad stick/trigger
deadzone CROSSING. Every threshold has a re-arm latch, so an axis that stays
deflected notifies ONCE. Same-frame priority is **TOUCH > GAMEPAD > MOUSE >
KEYBOARD**. `bSystemNav` events never count.

`SetProfileOverride` forces a profile and SUSPENDS the auto-switch;
`ClearOverride` returns to AUTO. Both run the mask-change synthesis.
The **last-used device** is tracked separately and is FINER than the profile: a
pad press inside a profile that already owns the pad changes the glyph set
without changing the profile.

### SYSTEM_BACK

`INPUT_BINDING_SYSTEM_BACK` is **MASK-EXEMPT** — it answers
`INPUT_SCHEME_NONE`, so no profile can mask it out. It is system NAVIGATION, not
a key: Android's `AKEYCODE_BACK` becomes a dedicated `INPUT_EVENT_SYSTEM_BACK`
that never lands in the key domain. The gesture has no held phase, so the row
PULSES (rise then fall) inside a single replay step.

Whether the platform's Back is **consumed** is a question, never a constant — and
this layer PUBLISHES the answer DOWNWARDS rather than being asked. `RegisterBinding`
raises a sticky flag on the injected `Zenith_Input` the first time a SYSTEM_BACK row
is registered (`Zenith_Input::HasSystemBackBinding()`, not cleared by
`ResetTransientForTest` — a registration is not per-frame state), and
`Zenith_Android_Main` reads it through the window funnel. It has to be that
direction: the platform entry point is layer 0 and Input is layer 1, so it cannot
include an `Input/` header at all, let alone walk this class's registered actions.
A game that binds it (Zenithmon's `Cancel`) gets Back closing its menu; a game that
does not keeps the platform's own behaviour, and an un-bound Back still backgrounds
the app. Pinned by `SystemBackBindingFlagsTheDeviceLayer` — the platform half is not
compiled on win64, so the unit is the only thing that can catch it going stale.

### Virtual sources (the on-screen controls)

`PublishVirtualButton` / `PublishVirtualAxis` are what the B9 widgets
(`Zenith_UIVirtualStick` / `Zenith_UIVirtualButton`) drive at step 10d. A button
publish becomes an **ORDERED virtual transition** replayed at 10e after this
frame's device transitions; an axis publish is level state. A control targets an
ACTION NAME and the named action's own `INPUT_BINDING_VIRTUAL` row supplies the
source id — so a rebind is a binding-table edit, not a widget edit. Full
semantics (retarget mid-gesture, visibility-blind walk, the TOUCH-mask
visibility latch): **`Zenith/UI/CLAUDE.md`**.

### Finalization

Both close stages replay the frame's ORDERED transitions through each action's
aggregate: rise ⇒ pressed, fall ⇒ released. A same-frame tap therefore fires
BOTH edges, and a second binding pulsing while another stays held fires NEITHER.
Held state is per BINDING; the action's aggregate is the OR over the rows the
active mask enables. Axis values are last-state, not replayed.

Any **mask change** first REBASES every action from its current source states: a
held action losing its rows synthesizes a RELEASE (held over the frame boundary
until a close stage has actually published it), and a newly enabled already-held
source becomes held with NO press edge.

At **10e only**, pointer-sourced rows (`TOUCH_PRIMARY`, and `MOUSE_BUTTON`
transitions fed by a pointer) are SUPPRESSED when their pointer is CLAIMED; a
suppressed press suppresses its matching release. Keyboard and gamepad
transitions are NEVER claim-filtered.

Queries (`IsHeld` / `WasPressedThisFrame` / `WasReleasedThisFrame` /
`GetAxis1D` / `GetAxis2D`) are **NON-CONSUMING** reads of closed state: two
readers in the same frame both see an edge, and mutual exclusion stays the
caller's job.

`ResolveMoveComposite` is public because it is a contract, not an
implementation detail: **+y is FORWARD, diagonals are UNNORMALISED, opposite
keys CANCEL.**

## Persisted settings (`Zenith_UserSettings`)

`Zenith/Core/Zenith_UserSettings.{h,cpp}` — the engine-owned player-preference
store, one SaveData slot (`"user_settings"`) per project. Today it holds exactly
one field: the input **profile override**.

**The boot order is engine-owned** (`Zenith_Engine::InitialiseProject`), and the
four steps are one contract:

1. `Zenith_SaveData::Initialise(Project_GetName())` — the **only** call in the
   process. Games must never call it: a second call re-enters the cross-process
   residue wipe and would delete a live automated-test batch's slots mid-run.
   (Pinned by the engine unit `BootInitDoesNotWipeAutomatedTestSandbox`, which
   asserts `Zenith_SaveData::GetInitialiseCallCount() == 1`.)
2. `UserSettings().Load()` — read the blob. No profile exists yet, so nothing is
   validated here.
3. `Project_RegisterGameComponents()` — the game registers its profiles.
4. `UserSettings().ApplyProfileOverride(Actions())` — resolve the persisted NAME
   against them and `SetProfileOverride`, or stay on AUTO.

**Profiles persist BY STABLE NAME, never by numeric id.** Reordering a game's
`RegisterProfile` calls must not reinterpret a saved setting as a different
profile; an unknown name degrades to AUTO and the saved name is deliberately
NOT rewritten, so a build in which the profile is temporarily absent does not
discard the player's choice.

**v1 payload (frozen):** 32 bytes — `char acProfileOverrideName[16]`,
NUL-terminated and ZERO-PADDED (all-zero = AUTO), then 16 reserved bytes written
as zero and IGNORED on read. The version rides in the SaveData header's
game-version field. Absent file, short blob, corrupt blob, unterminated or
non-printable name, dirty padding, or an unknown version all read as AUTO — a
settings file is the one file a player can corrupt by accident, and it must
never be able to stop the game booting.

**The override applies at BOOT ONLY.** Nothing re-applies it per frame, and the
automated-test between-tests reset returns the action layer to AUTO. Test runs
are isolated by construction: SaveData redirects the whole save root into the
run's sandbox, so a test process never reads or writes the player's real
settings.

## Input simulation (`Zenith_InputSimulator`)

Test-only static harness (compiled under `ZENITH_INPUT_SIMULATOR`) for injecting
synthetic input and driving frames deterministically. Everything it injects
enters at step 7 and travels the same path real input does.

- `Enable()` / `Disable()`
- `StepFrame()` / `StepFrames(uCount)` / `StepFramesWithFixedDt(uCount, fDt)` / `StepUntil(pfnCondition, uMaxFrames)`
- `SimulateKeyDown/Up/Press(eKey)`, `SimulateKeySequence(...)`
- `SimulateMousePosition/ButtonDown/ButtonUp/Click/Drag(...)`, `SimulateMouseWheel(fDelta)`
- `SimulateTouchDown/Move/Up/Cancel(iPointerId, x, y)` — raw pointer-stream injection
- `SetKeyHeld/ClearHeldKeys/ResetAllInputState()`, `SetFixedDt/ClearFixedDt()`

## Key constants

### Keyboard (GLFW-compatible values)
| Category | Constants | Range |
|----------|-----------|-------|
| Letters | `ZENITH_KEY_A` - `ZENITH_KEY_Z` | 65-90 |
| Numbers | `ZENITH_KEY_0` - `ZENITH_KEY_9` | 48-57 |
| Function | `ZENITH_KEY_F1` - `ZENITH_KEY_F25` | 290-314 |
| Arrows | `ZENITH_KEY_UP/DOWN/LEFT/RIGHT` | 265/264/263/262 |
| Special | `ZENITH_KEY_ESCAPE(256)`, `ZENITH_KEY_ENTER(257)`, `ZENITH_KEY_TAB(258)`, `ZENITH_KEY_SPACE(32)` | - |
| Modifiers | `ZENITH_KEY_LEFT_SHIFT(340)`, `ZENITH_KEY_LEFT_CONTROL(341)`, `ZENITH_KEY_LEFT_ALT(342)` | - |

### Mouse buttons
`ZENITH_MOUSE_BUTTON_LEFT(0)`, `ZENITH_MOUSE_BUTTON_RIGHT(1)`,
`ZENITH_MOUSE_BUTTON_MIDDLE(2)`, up to `ZENITH_MOUSE_BUTTON_8(7)`.

### Gamepad
`ZENITH_GAMEPAD_BUTTON_A(0)` through `ZENITH_GAMEPAD_BUTTON_DPAD_LEFT(14)`
(`== ZENITH_GAMEPAD_BUTTON_LAST`); PlayStation aliases `CROSS`/`CIRCLE`/
`SQUARE`/`TRIANGLE` map to A/B/X/Y. Axes `ZENITH_GAMEPAD_AXIS_LEFT_X(0)`
through `ZENITH_GAMEPAD_AXIS_RIGHT_TRIGGER(5)`.
`Zenith_Input::GAMEPAD_DEADZONE = 0.15f` (radial).

### Engine-reserved UI actions (ids 0-15)

Registered by the engine at boot; the UI canvas is their sole consumer. Every
game gets them without registering anything:

| Action | Keyboard | Gamepad |
|---|---|---|
| `INPUT_ACTION_UI_NAV_UP` | `ZENITH_KEY_UP` | d-pad Up |
| `INPUT_ACTION_UI_NAV_DOWN` | `ZENITH_KEY_DOWN` | d-pad Down |
| `INPUT_ACTION_UI_NAV_LEFT` | `ZENITH_KEY_LEFT` | d-pad Left |
| `INPUT_ACTION_UI_NAV_RIGHT` | `ZENITH_KEY_RIGHT` | d-pad Right |
| `INPUT_ACTION_UI_CONFIRM` | `ZENITH_KEY_ENTER`, `ZENITH_KEY_SPACE` | pad A |

There is deliberately no W/S row: menu navigation is arrows or the d-pad, and a
game that wants WASD focus movement rebinds these ids itself.

## Key patterns

- Ask the ACTION layer. Reach for `Zenith_Input` only for a POSITION (which no
  binding row carries) or inside a `<Game>_Bindings.h`.
- Edge (`WasPressedThisFrame`) vs state (`IsHeld`) is a real distinction; both
  are non-consuming.
- Platform callbacks ENQUEUE. They never mutate state — that is step 3's job.
- Vector results come back through output parameters, not return values.
- Unit tests build LOCAL instances and inject local dependencies (B13). Nothing
  in this directory may reach `g_xEngine`.
