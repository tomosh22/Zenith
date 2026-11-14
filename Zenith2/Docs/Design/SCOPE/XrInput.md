# SCOPE design: XR input through the OpenXR device source

This file is part of the SCOPE design family; the entry document is `Zenith2/Docs/Design/SCOPE.md`. It specifies the OpenXR device source that PLAT's XR-ready input model names as SCOPE's (`Zenith2/Docs/Design/PLAT/Input.md` → "XR-ready design"): the control-mirror action set (ruling Z2-D-136), the XR control catalogue, the suggested bindings per interaction profile, the per-frame flow into PLAT's devices, poses for simulation, haptic output, interaction-profile changes and replay.

## Scope of this file

PLAT owns the model above the device source: actions, contexts, the mapper, bindings files with their `interactionProfile` key, the POSE value and its command encoding, and `INPUT_DEVICE_KIND_XR_CONTROLLER` virtual devices named `left`, `right` and `head` (`Zenith2/Docs/Design/PLAT/Input.md` → "Zenith_InputDevices API", "Zenith_InputSystem API"; `Zenith2/Docs/Design/PLAT/SimulationInput.md` → "Input commands and quantisation"). This file owns everything below: what OpenXR is asked for, and how its answers become transitions of those three devices.

## The control-mirror action set

The game's own actions stay PLAT's. ZenithXR creates one OpenXR action set, `zenith_mirror` (localized name "Zenith2 controls", priority 0), whose actions mirror the XR control catalogue, so OpenXR reports the raw state of every control and PLAT's mapper applies the game's bindings to it, exactly as for a gamepad.

| Option considered | Why not |
|---|---|
| one OpenXR action per game action, built from the bindings files | PLAT's contexts, modifiers, composites and rebinding would have to be re-expressed as OpenXR suggestions; OpenXR action sets are attached once per session, so a rebind or a context push mid-session could not be represented |
| a vendor input API | not OpenXR (ruling SC-R3) |
| the mirror (chosen) | PLAT's model is unchanged, rebinding works as for every device, and OpenXR's per-profile suggestions map vendor controllers onto one catalogue |

One action per control kind with the subaction paths `/user/hand/left` and `/user/hand/right`; each hand's state is read with its subaction path. The action set is created and attached in startup step S3 (`Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Startup and the device-interop provider"); action spaces are created for `grip` and `aim` per hand (four spaces).

## XR control catalogue

The controls of the `left` and `right` devices (identical lists; `m_uControl` the row number, starting at 1) and of the `head` device. Path names are PLAT's `/xr/<device>/<control>` (`Zenith2/Docs/Design/PLAT/Input.md` → "Control paths").

| `m_uControl` | Control | `Zenith_InputActionType` | OpenXR action (name, type) | Range |
|---|---|---|---|---|
| 1 | `grip` | POSE | `grip`, `XR_ACTION_TYPE_POSE_INPUT` | metres and a unit quaternion in the active space after recentre |
| 2 | `aim` | POSE | `aim`, `XR_ACTION_TYPE_POSE_INPUT` | as grip |
| 3 | `trigger` | AXIS1D | `trigger_value`, FLOAT | [0, 1] |
| 4 | `triggerClick` | BUTTON | `trigger_click`, BOOLEAN | 0 or 1 |
| 5 | `triggerTouch` | BUTTON | `trigger_touch`, BOOLEAN | 0 or 1 |
| 6 | `squeeze` | AXIS1D | `squeeze_value`, FLOAT | [0, 1] |
| 7 | `squeezeClick` | BUTTON | `squeeze_click`, BOOLEAN | 0 or 1 |
| 8 | `thumbstick` | AXIS2D | `thumbstick`, VECTOR2F | [−1, 1] per axis, y up |
| 9 | `thumbstickClick` | BUTTON | `thumbstick_click`, BOOLEAN | 0 or 1 |
| 10 | `thumbstickTouch` | BUTTON | `thumbstick_touch`, BOOLEAN | 0 or 1 |
| 11 | `trackpad` | AXIS2D | `trackpad`, VECTOR2F | [−1, 1] per axis, y up |
| 12 | `trackpadClick` | BUTTON | `trackpad_click`, BOOLEAN | 0 or 1 |
| 13 | `trackpadTouch` | BUTTON | `trackpad_touch`, BOOLEAN | 0 or 1 |
| 14 | `primary` | BUTTON | `primary_click`, BOOLEAN | 0 or 1; the lower face button (X on the left Touch controller, A on the right) |
| 15 | `primaryTouch` | BUTTON | `primary_touch`, BOOLEAN | 0 or 1 |
| 16 | `secondary` | BUTTON | `secondary_click`, BOOLEAN | 0 or 1; the upper face button (Y, B) |
| 17 | `secondaryTouch` | BUTTON | `secondary_touch`, BOOLEAN | 0 or 1 |
| 18 | `menu` | BUTTON | `menu_click`, BOOLEAN | 0 or 1 |
| 19 | `thumbrestTouch` | BUTTON | `thumbrest_touch`, BOOLEAN | 0 or 1 |
| — | haptic output | — | `haptic`, `XR_ACTION_TYPE_VIBRATION_OUTPUT` | § "Haptic output" |

`head` declares one control, `pose` (`m_uControl` 1, POSE), located from the VIEW space, not an action. The system and home buttons are never bound: OpenXR reserves them for the runtime. The catalogue is 19 controls per hand device and 1 head control, inside PLAT's limit of 32 controls per virtual device.

## Suggested bindings

`xrSuggestInteractionProfileBindings` once per profile below, in this order, at step S3; a profile the runtime refuses (`XR_ERROR_PATH_UNSUPPORTED`, `XR_ERROR_INTERACTION_PROFILE_UNSUPPORTED`) is skipped and logged, never fatal; at least one accepted profile is required (else `ZENITH_XR_ERROR_ACTION_SETUP`). Paths are relative to `/user/hand/<left|right>/`; "both" means the binding is suggested for each hand.

| Control | `khr/simple_controller` | `oculus/touch_controller` | `valve/index_controller` | `htc/vive_controller` | `microsoft/motion_controller` |
|---|---|---|---|---|---|
| grip, aim | `input/grip/pose`, `input/aim/pose` | both | both | both | both |
| trigger | `input/select/click` | `input/trigger/value` | `input/trigger/value` | `input/trigger/value` | `input/trigger/value` |
| triggerClick | `input/select/click` | `input/trigger/value` | `input/trigger/click` | `input/trigger/click` | `input/trigger/value` |
| triggerTouch | — | `input/trigger/touch` | `input/trigger/touch` | — | — |
| squeeze | — | `input/squeeze/value` | `input/squeeze/value` | `input/squeeze/click` | `input/squeeze/click` |
| squeezeClick | — | `input/squeeze/value` | `input/squeeze/value` | `input/squeeze/click` | `input/squeeze/click` |
| thumbstick, Click, Touch | — | `input/thumbstick`, `…/click`, `…/touch` | `input/thumbstick`, `…/click`, `…/touch` | — | `input/thumbstick`, `…/click`, — |
| trackpad, Click, Touch | — | — | `input/trackpad`, —, `input/trackpad/touch` | `input/trackpad`, `…/click`, `…/touch` | `input/trackpad`, `…/click`, `…/touch` |
| primary, primaryTouch | — | left `input/x/click`, `input/x/touch`; right `input/a/click`, `input/a/touch` | `input/a/click`, `input/a/touch` | — | — |
| secondary, secondaryTouch | — | left `input/y/click`, `input/y/touch`; right `input/b/click`, `input/b/touch` | `input/b/click`, `input/b/touch` | — | — |
| menu | `input/menu/click` | left `input/menu/click` | — | `input/menu/click` | `input/menu/click` |
| thumbrestTouch | — | `input/thumbrest/touch` | — | — | — |
| haptic | `output/haptic` | both | both | both | both |

A float action bound to a `…/click` path reports 0 or 1, and a boolean action bound to a `…/value` path reports the runtime's threshold; both conversions are OpenXR's. The table above is the readable form; the binding of record is the machine-readable block below, one line per suggested binding, `<profile>|<control>|<hand>|<path>` with `<hand>` `left`, `right` or `both` and `<path>` relative to `/user/hand/<hand>/`. `Internal/Zenith_XrBindingTable.cpp` is constant data with one entry per line, and `Test_XrInput_BindingTableMatchesDesign` reads this block from the source tree (the test target's `ZENITH_SOURCE_DIR`), between the fence lines that follow the marker paragraph, and compares it with the constant table entry for entry.

```text
khr/simple_controller|grip|both|input/grip/pose
khr/simple_controller|aim|both|input/aim/pose
khr/simple_controller|trigger|both|input/select/click
khr/simple_controller|triggerClick|both|input/select/click
khr/simple_controller|menu|both|input/menu/click
khr/simple_controller|haptic|both|output/haptic
oculus/touch_controller|grip|both|input/grip/pose
oculus/touch_controller|aim|both|input/aim/pose
oculus/touch_controller|trigger|both|input/trigger/value
oculus/touch_controller|triggerClick|both|input/trigger/value
oculus/touch_controller|triggerTouch|both|input/trigger/touch
oculus/touch_controller|squeeze|both|input/squeeze/value
oculus/touch_controller|squeezeClick|both|input/squeeze/value
oculus/touch_controller|thumbstick|both|input/thumbstick
oculus/touch_controller|thumbstickClick|both|input/thumbstick/click
oculus/touch_controller|thumbstickTouch|both|input/thumbstick/touch
oculus/touch_controller|primary|left|input/x/click
oculus/touch_controller|primaryTouch|left|input/x/touch
oculus/touch_controller|secondary|left|input/y/click
oculus/touch_controller|secondaryTouch|left|input/y/touch
oculus/touch_controller|primary|right|input/a/click
oculus/touch_controller|primaryTouch|right|input/a/touch
oculus/touch_controller|secondary|right|input/b/click
oculus/touch_controller|secondaryTouch|right|input/b/touch
oculus/touch_controller|menu|left|input/menu/click
oculus/touch_controller|thumbrestTouch|both|input/thumbrest/touch
oculus/touch_controller|haptic|both|output/haptic
valve/index_controller|grip|both|input/grip/pose
valve/index_controller|aim|both|input/aim/pose
valve/index_controller|trigger|both|input/trigger/value
valve/index_controller|triggerClick|both|input/trigger/click
valve/index_controller|triggerTouch|both|input/trigger/touch
valve/index_controller|squeeze|both|input/squeeze/value
valve/index_controller|squeezeClick|both|input/squeeze/value
valve/index_controller|thumbstick|both|input/thumbstick
valve/index_controller|thumbstickClick|both|input/thumbstick/click
valve/index_controller|thumbstickTouch|both|input/thumbstick/touch
valve/index_controller|trackpad|both|input/trackpad
valve/index_controller|trackpadTouch|both|input/trackpad/touch
valve/index_controller|primary|both|input/a/click
valve/index_controller|primaryTouch|both|input/a/touch
valve/index_controller|secondary|both|input/b/click
valve/index_controller|secondaryTouch|both|input/b/touch
valve/index_controller|haptic|both|output/haptic
htc/vive_controller|grip|both|input/grip/pose
htc/vive_controller|aim|both|input/aim/pose
htc/vive_controller|trigger|both|input/trigger/value
htc/vive_controller|triggerClick|both|input/trigger/click
htc/vive_controller|squeeze|both|input/squeeze/click
htc/vive_controller|squeezeClick|both|input/squeeze/click
htc/vive_controller|trackpad|both|input/trackpad
htc/vive_controller|trackpadClick|both|input/trackpad/click
htc/vive_controller|trackpadTouch|both|input/trackpad/touch
htc/vive_controller|menu|both|input/menu/click
htc/vive_controller|haptic|both|output/haptic
microsoft/motion_controller|grip|both|input/grip/pose
microsoft/motion_controller|aim|both|input/aim/pose
microsoft/motion_controller|trigger|both|input/trigger/value
microsoft/motion_controller|triggerClick|both|input/trigger/value
microsoft/motion_controller|squeeze|both|input/squeeze/click
microsoft/motion_controller|squeezeClick|both|input/squeeze/click
microsoft/motion_controller|thumbstick|both|input/thumbstick
microsoft/motion_controller|thumbstickClick|both|input/thumbstick/click
microsoft/motion_controller|trackpad|both|input/trackpad
microsoft/motion_controller|trackpadClick|both|input/trackpad/click
microsoft/motion_controller|trackpadTouch|both|input/trackpad/touch
microsoft/motion_controller|menu|both|input/menu/click
microsoft/motion_controller|haptic|both|output/haptic
```

Every profile path above is `/interaction_profiles/` followed by the first field.

## Per-frame input flow

1. **Prologue step 1, `OnBeforePlatformPump`** (`Zenith_XrSystem::PumpBeforePlatform`), while the session is `FOCUSED` or `VISIBLE`: `xrSyncActions` with the mirror set and no subaction filter; on `XR_SESSION_NOT_FOCUSED` (VISIBLE) every action reads inactive.
2. For each hand device that exists and each catalogue control: read the state with the hand's subaction path; an inactive action reads its rest value (0, or (0, 0)); when the value differs from the last value pushed for that device and control, call `Zenith_InputDevices::SetVirtualControl(device, control, value)`. Booleans push 0 or 1 in `m_fX`, floats in `m_fX`, vectors in `m_fX`, `m_fY`.
3. Poses: § "Poses for simulation".
4. `Platform.Pump` then applies the queued values in call order, so this frame's ticks see them in phase 1 (no extra frame of latency; `SetVirtualControl` applies at the next pump, which is this frame's).
5. **`OnPlatformEvents`** (`PumpAfterPlatform`): events (§ "Interaction profile changes", and the session events of `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Event handling").

Devices: the `head` device is created when the session enters `READY` and destroyed when it leaves the running states; a hand device is created when its hand's current interaction profile becomes non-null and destroyed when it becomes null or the session stops, through `CreateVirtualDevice` with `m_eDeviceKind = INPUT_DEVICE_KIND_XR_CONTROLLER` and names `left`, `right`, `head`; PLAT synthesises releases at destruction (readiness check XRR-I4).

## Poses for simulation

Indexing: frame k's simulation runs in iteration k and its picture is rendered in iteration k + 1 (Z2-D-050); W(k) is issued at the `Flux.Present` of render frame k − 1, which runs inside iteration k, so the prologue of iteration k runs after W(k − 1) and before W(k). Frame k's input sample time is therefore `t = T(k − 1) + P`, with `T(k − 1)` the predicted display time W(k − 1) returned and `P` its predicted period, which is the runtime's prediction of T(k). Before the first W (the frames between `xrBeginSession` and the first `Flux.Present` after it) no pose is sampled and the POSE controls keep their last values (none: the device was created at READY with identity poses). For each existing hand device: `xrLocateSpace(grip or aim space, active space, t)`; when both `XR_SPACE_LOCATION_POSITION_VALID_BIT` and `ORIENTATION_VALID_BIT` are set, multiply by the recentre transform and push the POSE value (position in metres, orientation normalised to w ≥ 0); otherwise push nothing and count `xr.input.pose_invalid` (the last pose holds). The head: `xrLocateSpace(view space, active space, t)` into `/xr/head/pose`. The eye poses the renderer uses are located separately, at render time (late latching), and never reach simulation.

## Haptic output

<!-- assumption: pending PLAT (row SP4) -->
PLAT's haptic player routes an effect on an `INPUT_DEVICE_KIND_XR_CONTROLLER` device to a sink the device's creator registers: `Zenith_VirtualHapticSink` (a PLAT interface, `virtual void Play(Zenith_InputDeviceID, float fLowAmplitude, float fHighAmplitude, uint32 uDurationMs)`, `virtual void Stop(Zenith_InputDeviceID)`), set through a `Zenith_VirtualDeviceDesc::m_pxHapticSink` member, called on the main thread where the player submits.
<!-- /assumption -->

ZenithXR's sink maps a play to `xrApplyHapticFeedback(haptic action, subaction path of the hand, XrHapticVibration { duration = uDurationMs × 1,000,000 ns, frequency = XR_FREQUENCY_UNSPECIFIED, amplitude = max(low, high) clamped to [0, 1] })` and an amplitude of 0 or a stop to `xrStopHapticFeedback`; outside `FOCUSED` the call is skipped and counted `xr.haptics.skipped`. The head device has no haptics. Which effects reach an XR controller: an effect aimed at `HAPTIC_TARGET_DEVICE` with an XR controller's device ID always does; an effect aimed at `HAPTIC_TARGET_ACTIVE_DEVICE` (PLAT's "the most recently actuated gamepad", `Zenith2/Docs/Design/PLAT/Input.md` → "Haptic API") reaches the most recently actuated XR controller instead when the active control scheme is the XR scheme, which PLAT's Stage 7 input ticket adds with the haptic sink (row SP4); otherwise it never reaches an XR controller.

## Interaction profile changes

On `XrEventDataInteractionProfileChanged`: `xrGetCurrentInteractionProfile` for `/user/hand/left` and `/user/hand/right`; create or destroy the hand devices as § "Per-frame input flow" says; call `Zenith_InputSystem::SetActiveInteractionProfiles` with the non-null profile paths, deduplicated, left first (PLAT activates the binding sets whose `interactionProfile` matches); log `xr.input: interaction profile <hand> = <path or none>`; count `xr.input.profile_changes`.

## Replay and determinism

- Simulation sees XR input only as PLAT's input commands, which PLAT's mapper produces from the devices' transitions in phase 1 and which are replay-logged; ZenithXR adds no command kind and no simulation path (Z2-D-019).
- Under `--replay` the provider is created disabled, so no device source runs and the logged commands replay bit-identically (PLAT's `Test_Xr_LiveEqualsReplayWithPose` proves the encoding; SCOPE's `Test_XrInput_ReplayWithoutRuntimeMatches` proves a scripted-runtime run replays without a runtime).
- The `XR` notifications are recorded publications (`ASYNC_SOURCE_XR`, `PUBLICATION_KIND_GENERIC_RECORDED`): a replay reproduces them from the log without a runtime.
- The recentre transform, the active reference space and the eye poses are presentation state.
