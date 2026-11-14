# PLAT design: simulation input

This file is part of the PLAT design family. The entry document is `Zenith2/Docs/Design/PLAT.md`. It specifies how input enters simulation: the phase-1 Input system and its world resources, the tick assignment of events, input commands and their quantisation, replay and scripted input, latency attribution, and the Luau input surface. The devices, actions, bindings, contexts and the action mapper it runs are specified in `Zenith2/Docs/Design/PLAT/Input.md`; its tests are that file's "Input tests". The SPEC baselines are `Zenith2/Docs/Architecture/FramePhases.md` → "Phase-specific rules" and `Zenith2/Docs/Architecture.md` → "Multiplayer foundations".

## Scope of this file

| Topic | Heading here |
|---|---|
| the phase-1 system, its world resources, tick assignment, commands, replay, scripted input, input-script recording, latency | "Simulation input" |
| the `Input` Luau namespace and the `Platform.*` script events | "Script-facing input API" |

The requirement each heading specifies is listed in `Zenith2/Docs/Design/PLAT.md` → "Requirement index".

## Simulation input

### The Input phase system

The runtime bridge (`Zenith2/Engine/ZenithRuntime/Internal/PlatformBridge/Zenith_InputPhaseSystem.cpp`, entry document → "Runtime bridge files in ZenithRuntime") registers one C++ system per world:

```cpp
Zenith_SystemBuilder& xBuilder = xContext.GetSystems().AddSystem(Zenith_SystemDesc{
	.m_szName = "Input.Actions",
	.m_ePhase = PHASE_INPUT,
	.m_pfnRun = &Zenith_InputPhaseSystem_Run,
	.m_pUserData = pxWorldInput,
	.m_eAffinity = JOB_AFFINITY_ANY,
	.m_uCostHint = 4 });
xBuilder.ReadResource(Zenith_FrameInputResource::xTYPE_ID)
	.WriteResource(Zenith_InputActionStateResource::xTYPE_ID)
	.WriteResource(Zenith_SimulationInputContexts::xTYPE_ID)
	.WriteResource(Zenith_InputTickSummary::xTYPE_ID);
```

The five world resources are declared by the bridge header `Zenith2/Engine/ZenithRuntime/Internal/PlatformBridge/Zenith_InputResources.h` and registered as reflected resources by `Zenith_InputResources.cpp`:

```cpp
// Zenith2/Engine/ZenithRuntime/Internal/PlatformBridge/Zenith_InputResources.h
struct Zenith_FrameInputResource            // refreshed by the runtime between frames; read-only in every phase
{
	Zenith_Span<const Zenith_InputTransition> m_axCarried;     // the carry region, consumed first
	Zenith_Span<const Zenith_InputTransition> m_axFrame;       // this frame's transitions, flags final
	Zenith_Span<const float> m_afConfigScales;                 // the configuration-scale snapshot, by slot
	const Zenith_InputBindingTable* m_pxBindings = nullptr;
	Zenith_ControlScheme m_eActiveScheme = CONTROL_SCHEME_KEYBOARD_MOUSE;
	uint64 m_ulFirstTickIndex = 0;                           // the first tick instance of this frame
	bool m_bInputWorld = false;
};

struct Zenith_InputActionStateResource      // rebuilt from the tick's commands
{
	Zenith_InputActionStateTable m_xStates;                    // what simulation reads
	Zenith_InputActionStateTable m_xMapperStates;              // the simulation mapper's private output
	Zenith_ActionMapper m_xMapper;
	Zenith_InputCommandEncoder m_xEncoder;                     // last emitted quantised value per action
	uint32 m_uNextSequence = 0;                               // per player slot 0
};

struct Zenith_SimulationInputContexts       // serialized with the world
{
	Zenith_InputContextStack m_xStack;
};

struct Zenith_InputTickSummary
{
	uint64 m_ulNewestEventSequence = 0;
	uint64 m_ulNewestEventTimestampNs = 0;
	uint64 m_ulLatencyProbeSequence = 0;                      // 0 when no probe was consumed this tick
	uint32 m_uCommandCount = 0;
	uint32 m_uCommandsDropped = 0;
	uint32 m_uCommandsRejected = 0;
};

struct Zenith_HapticRequestQueue           // appended by command playback, drained by the epilogue stage
{
	Zenith_HapticEffect m_axRequests[64];                      // m_xDevice unused: targets are ACTIVE_DEVICE or ALL_GAMEPADS
	uint32 m_uCount = 0;
	bool m_abStopChannel[HAPTIC_CHANNEL_COUNT] = {};         // Input.StopHaptic requests, applied before the plays
};
```

| World resource | Owning phase | Contents |
|---|---|---|
| `Zenith_FrameInputResource` | Input (read-only in every phase; refreshed by the runtime between frames) | a view of the frame's transitions and carry, the input-world flag, the active scheme, the frame's first tick index, the configuration-scale snapshot |
| `Zenith_InputActionStateResource` | Input | the world's `Zenith_InputActionStateTable`, rebuilt from the tick's commands |
| `Zenith_SimulationInputContexts` | Input | the world's simulation `Zenith_InputContextStack`; changed by `Input.PushContext` and `Input.PopContext` playback |
| `Zenith_InputTickSummary` | Input | the tick's newest consumed event sequence and timestamp, command count, latency-probe sequence |
| `Zenith_HapticRequestQueue` | the sync points after phases 2–10 (playback) and the epilogue (drain) | the tick's haptic requests (`Zenith2/Docs/Design/PLAT/Input.md` → "Haptics from simulation") |

`Zenith_InputPhaseSystem_Run` performs, per tick of the world:

1. Replay (`--replay`): read the tick's input commands from the replay reader (`Zenith2/Docs/Design/SAVE.md` (pending; its "Public API" section)); go to step 4.
2. Choose the slice: when this world is the input world and this is the frame's first tick instance, the carried transitions followed by the frame's transitions; otherwise an empty slice.
3. Run the world's simulation `Zenith_ActionMapper` over the slice with the world's context stack, `m_bSimulation = true` and `m_ulSliceDurationUs` = the tick duration, writing its private `m_xMapperStates`; encode its changes as commands with the resource's `m_xEncoder` (§ "Input commands and quantisation"); append the input script's action events for this tick (§ "The input-script format").
4. Rebuild `m_xStates` with `Zenith_ApplyInputCommands` from the previous tick's table and this tick's commands; simulation reads only `m_xStates`, never `m_xMapperStates`.
5. Record (`--record`): append the commands to the replay writer. While an input-script recording is active, also hand the tick's commands to the recorder, with the run's input tick (§ "Input-script recording").
6. Write `Zenith_InputTickSummary`.

The system's cost is the row Z2-B-PLAT-001 "Input phase CPU" (entry document → "Budget rows"). Its node runs in every configuration including headless, where the slice is empty unless an input script or replay supplies input.

- Exactly one world is the input world at a time: the runtime marks the startup world, moves the mark at a world-transition swap, and in the editor marks the play world while one runs (`Zenith2/Docs/Design/MOD.md` → "Public API"); every other world maps an empty slice and emits no command.
- The resource types are registered by the runtime bridge as `TYPE_KIND_RESOURCE` with owning phase `PHASE_INPUT` (ZenithPlatform is below ZenithReflect, so the reflected wrappers live in the bridge); `Zenith_SimulationInputContexts` is flagged serialized, so saves and the resume state keep the stack.

### Tick assignment of events

1. Every transition of a frame belongs to the first tick instance of that frame for the input world.
2. Ticks 2 to T of a catch-up frame receive no new transitions; held actions stay held (their state carries), and edges happened in tick 1.
3. When the input world runs no tick in a frame (T = 0) while not paused, the frame's transitions are carried: after `RunFrame`, the runtime calls `Zenith_InputDevices::CarryFrameTransitions()`, which appends them to the carry region (capacity `input.carryCapacity`, 1,024; overflow drops the oldest motion transitions first, then the oldest others, counted in `input.carry.dropped`); the next tick that runs consumes the carry first, and after a `RunFrame` in which the input world ran at least one tick the runtime calls `ClearCarriedTransitions()`.
4. While the input world is paused (`sim.paused`) nothing is carried; resynchronisation (step 6 of "Mapper algorithm") repairs held state when simulation resumes.
5. Presentation evaluation is unaffected by ticks: the presentation mapper maps every frame's transitions in that frame.

<!-- informative -->
Assigning a frame's input to its first tick keeps edges in the tick that follows the input and costs nothing; distributing transitions across catch-up ticks by timestamp would model sub-frame timing that fixed-tick gameplay cannot use, and would make the tick a transition lands in depend on wall-clock jitter.
<!-- /informative -->

### Input commands and quantisation

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_InputCommand.h

/// @contract value Zenith_InputCommand
/// Ownership: Plain value; produced by phase 1, logged by the replay writer, read back by replay and the loopback server.
/// Thread affinity: IMMUTABLE. Members non-const: an element of the command log and rings, written in place.
/// Invalidation: None.
/// Units: The value bytes per the table "Command value encoding".
/// Errors: Zenith_DecodeInputValue returns INPUT_ERROR_BAD_COMMAND for a malformed value, Zenith_ApplyInputCommands
///   skips unknown actions; decoding never asserts, because replay logs and network messages are untrusted.
/// Version: since Z2 Stage 3. Serialized: the field order and the value encoding are part of the ZRPL format version.
/// @endcontract
struct Zenith_InputCommand
{
	uint16 m_uPlayerSlot = 0;       // 0 for the local player; other slots belong to NET (Stage 7)
	uint16 m_uReserved = 0;         // not serialized
	uint32 m_uActionID = 0;         // Zenith_InputActionID value | (component << uINPUT_ACTION_ID_COMPONENT_SHIFT)
	uint32 m_uSequence = 0;         // per player slot, strictly increasing across the run
	uint8 m_auValue[8] = {};
};
static_assert(sizeof(Zenith_InputCommand) == 20);

void Zenith_EncodeInputValue(Zenith_InputActionType eType, const Zenith_InputActionState& xState, uint8 (&auOut)[8]);
Zenith_Result<void, Zenith_InputError> Zenith_DecodeInputValue(Zenith_InputActionType eType, const uint8 (&auIn)[8],
	Zenith_InputActionState& xOut);
/// @contract type Zenith_InputCommandEncoder
/// Ownership: A value owned by the world's Zenith_InputActionStateResource; storage from the general heap, sized at
///   bindings load to the action count.
/// Thread affinity: OWNER_THREAD (the world's phase-1 node).
/// Invalidation: Reset by a bindings reload or a world transition, together with the mapper.
/// Units: None.
/// Errors: Create returns INPUT_ERROR_OUT_OF_MEMORY; EncodeTick never fails; commands beyond the 4,096 cap are dropped,
///   counted and marked (see emission rule 5).
/// Version: since Z2 Stage 3. Source API.
/// @endcontract
class Zenith_InputCommandEncoder
{
public:
	static Zenith_Result<Zenith_InputCommandEncoder, Zenith_InputError> Create(const Zenith_InputActionSet& xActions,
		Zenith_Heap& xHeap);
	// Emits this tick's commands from the mapper's changes and the mapper's end-of-slice states, comparing AXIS2D and
	// AXIS3D values with the last value this encoder emitted per action; returns the command count.
	uint32 EncodeTick(Zenith_Span<const Zenith_ActionChange> axChanges, const Zenith_InputActionStateTable& xMapperStates,
		uint16 uPlayerSlot, uint32& uInOutSequence, Zenith_Span<Zenith_InputCommand> axOut);
	void Reset();            // every last-emitted value to rest and every dropped marker cleared; the owner resets m_xStates to rest with it
};
Zenith_Result<void, Zenith_InputError> Zenith_ApplyInputCommands(const Zenith_InputActionSet& xActions,
	Zenith_Span<const Zenith_InputCommand> axCommands, Zenith_InputActionStateTable& xStates);
```

The ZRPL tick block serialises each command as `{ uint16 player slot; uint32 action ID; uint32 sequence; 8 bytes value }`, little-endian (`Zenith2/Docs/Architecture.md` → "Multiplayer foundations"); the writer and reader are SAVE's, the struct and the value codec are PLAT's.

Command value encoding (all integers little-endian; fixed-point values rounded to nearest with halves away from zero, then clamped to the field's range):

| Action type | Bytes 0–3 | Bytes 4–7 |
|---|---|---|
| BUTTON | [0] held (0 or 1), [1] phase flags, [2–3] tap count (`uint16`) | held milliseconds (`uint32`, saturating) |
| AXIS1D | the value as s15.16 fixed point (`int32`) | [4] phase flags, [5] held (0 or 1), [6–7] zero |
| AXIS2D, DELTA2D | x as s15.16 | y as s15.16 |
| AXIS3D | [0–1] x as s7.8 (`int16`), [2–3] y as s7.8 | [4–5] z as s7.8, [6] phase flags, [7] zero |
| POSE (Stage 7, reserved) | component 0: position x, y, z as `int16` in 1/1024 m (±32 m) and [6] phase flags; component 1: orientation as the smallest three `int16` quaternion components and [6] the dropped index | as for bytes 0–3 |

Emission rules for one tick (`Zenith_InputCommandEncoder::EncodeTick`; the encoder keeps, per action, the last quantised value it emitted, which is exactly the value `Zenith_ApplyInputCommands` last applied to `m_xStates`):

1. BUTTON and AXIS1D actions: one command per change of held state or phase flags, in the order of the transitions that caused them; one more at the slice end when an AXIS1D value changed without an edge.
2. AXIS2D and AXIS3D actions: one command at the slice end when the quantised value differs from the last command emitted for the action.
3. DELTA2D actions: one command at the slice end when the tick's summed delta is non-zero after quantisation.
4. Within a tick, commands are ordered by the sequence of their causing transition, slice-end commands last in ascending action ID; each takes the next per-player sequence number.
5. At most 4,096 commands per tick (the replay-log limit of `Zenith2/Docs/Architecture.md` → "Security architecture"); beyond it, slice-end AXIS and DELTA commands are dropped first and BUTTON commands last, each drop counted in `input.commands.dropped`. A dropped command does not update the encoder's last-emitted value; the encoder records a `dropped` marker for the action instead, and the action's next `EncodeTick` emits its current value (for BUTTON, its held state and flags) whether or not it changed, then clears the marker. So the invariant holds after every tick: for every action, the last-emitted value equals the value in `m_xStates`, and a dropped change reaches `m_xStates` one tick late, identically live and in replay.
6. `Reset` (a bindings reload or a world transition, together with the mapper's `Reset`) returns the encoder's last-emitted values to rest and, in the same step, the owning resource resets `m_xStates` to rest, so the invariant holds across it; the first `EncodeTick` after it emits every action that is not at rest.

`Zenith_ApplyInputCommands` clears the slice fields, then applies each command in order: BUTTON: held from byte 0 (`m_bPressed` on a 0→1 change, `m_bReleased` on 1→0, both possible in one tick), flags OR-ed, tap count and held milliseconds taken; AXIS1D: value and flags, held from byte 5; AXIS2D, AXIS3D: value; DELTA2D: the delta added to the tick's value. A command for an unknown action or with a malformed value is skipped with `input.commands.rejected` incremented and a log entry, never an assert.

- Simulation reads only dequantised values: the live run rebuilds its action state from the commands it just encoded, exactly as a replay does, so live and replayed simulation see identical bits (Z2-D-019).

### Replay and scripted input

| Mode | Phase 1 reads | Physical devices | Presentation |
|---|---|---|---|
| live | the simulation mapper's commands | used | used |
| `--record <file.zrpl>` | as live, and logs the commands | used | used |
| `--replay <file.zrpl>` | the log's commands | ignored by phase 1 | used (UI stays usable) |
| `--input-script <file.json>` | the mapper's commands plus the script's action events | ignored by both mappers when the script's `exclusive` is true | the script's control events |

### The input-script format

```json
{
	"schema": "zenith2.input-script",
	"schemaVersion": 1,
	"exclusive": true,
	"events": [
		{ "tick": 10, "action": "Gameplay.Jump", "value": 1 },
		{ "tick": 12, "action": "Gameplay.Jump", "value": 0 },
		{ "tick": 20, "action": "Gameplay.Move", "value": [0.0, 1.0] },
		{ "tick": 30, "control": "/keyboard/escape", "value": 1 },
		{ "tick": 31, "control": "/keyboard/escape", "value": 0 },
		{ "tick": 40, "control": "/touch/0/press", "value": 1 },
		{ "tick": 40, "control": "/touch/0/position", "value": [540, 1200] }
	]
}
```

- `tick` is the run's input tick at which the event applies: `Zenith_InputSystem::GetInputTickCount()`, the number of input-world ticks completed before that tick, counted from 0 and summed across every world that held the input mark. A world-transition swap does not reset it (as `--exit-after-ticks` counts, `Zenith2/Docs/Architecture/CompositionRoot.md` → "Command-line options"). While no swap has happened it equals the startup world's tick index. It is non-decreasing through the file, and events of one tick keep file order.
- The phase-1 system increments the count once per input-world tick, after step 6 of "The Input phase system". Ticks of worlds without the input mark never count. A paused input world (`sim.paused`) runs no tick, so it does not count either.
- `schemaVersion` 2 adds one optional key to action events: `bits`, the 8 command value bytes as 16 lower-case hexadecimal digits (byte 0 first). When present, the event becomes a command with exactly those bytes, after `Zenith_DecodeInputValue` accepts them for the action's type (a refusal is `INPUT_ERROR_INPUT_SCRIPT_INVALID`). `value` is then still required for readability, with the shape of the action's type, but it is not used. Version 1 files load unchanged, and a `bits` key in a version 1 file is a schema error. The recorder of "Input-script recording" writes version 2, so a recorded script replays phase flags, tap counts and held times bit-exactly, which a `value` alone cannot carry.
- An `action` event becomes a command of that tick, appended after the mapper's commands, with the value converted by the action's type (a number for BUTTON and AXIS1D, an array of 2 or 3 numbers otherwise).
- A `control` event is applied to the virtual device `script` (created on load with every control the file names) through `SetVirtualControl` at the pump preceding the frame whose next tick index is at least `tick`, so it flows through claims, gestures and both mappers like physical input.
- The file is untrusted: at most 16 MiB and 1,048,576 events; a violation or any schema error fails loading with `INPUT_ERROR_INPUT_SCRIPT_INVALID` and stops startup with exit code 2.

### Input-script recording

A development session can record the input world's phase-1 action commands as a `zenith2.input-script` version 2 file. The file is the `--input-script` artefact that the playthrough recordings commit (`Zenith2/Docs/Design/GAME/OpenWorldSlice.md` → "Acceptance package"), and a text file, never a binary. PLAT's existing input layer covers the playback (`--input-script`) and the binary replay log (`--record`, SAVE's ZRPL); it did not cover writing an input script, so this section adds it.

The recorder lives in `Zenith_InputSystem` (`Internal/Zenith_InputScriptRecorder.cpp`), which persists across world swaps:

1. `BeginScriptRecording()` returns `INPUT_ERROR_NOT_IN_SHIPPING` in a Shipping build, and `INPUT_ERROR_RECORDING_STATE` while a recording is active. Otherwise it reserves the event buffer from the input system's heap: 262,144 events of 24 bytes (`uint64` input tick, `uint32` action ID, 8 value bytes, `uint32` reserved), which is 6 MiB. A failed reservation is `INPUT_ERROR_OUT_OF_MEMORY`.
2. In each input-world tick, phase-1 step 5 appends one event per command of the tick, in command order. Each event takes the run's input tick (`GetInputTickCount()` of the tick being run, an absolute tick, so the file replays from startup at the same ticks), and the command's action ID and value bytes. A command whose action component is not 0 (a POSE component, Stage 7) is skipped and counted.
   - Past 262,144 events, recording stops accepting events. Every further command is dropped and counted in `input.script.record_dropped`, and one warning is logged: `input script recording full at tick <n>`.
   - The encoder's emission rules already produce one command per changed value, so one event per command is "one event per changed action value".
3. `EndScriptRecording(xArena)` returns `INPUT_ERROR_RECORDING_STATE` when no recording is active. Otherwise it writes the document into `xArena` and frees the buffer:
   - the header is `{"schema":"zenith2.input-script","schemaVersion":2,"exclusive":true,"events":[`;
   - each event is written as `{"tick":<n>,"action":"<name>","value":<v>,"bits":"<16 hex>"}`, one per line, where `<name>` is the action's stable name from the loaded action set and `<v>` the value `Zenith_DecodeInputValue` yields: BUTTON `0` or `1`; AXIS1D a number; AXIS2D and DELTA2D `[x, y]`; AXIS3D `[x, y, z]`, each the shortest decimal that round-trips the `float`;
   - the file is LF-terminated;
   - when the document would exceed the loader's 16 MiB cap, events are cut at the last whole tick that fits, the cut is counted in `input.script.record_dropped`, and a warning is logged, so the result always loads.
4. Both calls are MAIN_THREAD. The console runs its handlers at frame-loop step 3, outside `RunFrame`, and the phase-1 node is the only writer while a frame runs, so the recorder takes no lock.

The console commands are registered by ZenithPlatform's development-tools area (`Zenith_RegisterPlatformDevToolsArea`, `Internal/DevTools/Zenith_PlatformDevToolsArea.cpp`, over `Zenith_DevToolsAreaServices`; one line in ZenithRuntime's `Internal/DevTools/Zenith_DevToolsAreaList.cpp`, `Zenith2/Docs/Design/DEVT.md` → "Area registration"). They are absent in Shipping, where `m_pxConsole` is null:

| Command | Behaviour |
|---|---|
| `input.recordScript <name>` | starts a recording (`BeginScriptRecording`) and keeps `<name>`. A missing argument, or an argument that the file writer would refuse, replies `input.recordScript: bad name`. A recording already active replies `input.recordScript: already recording` |
| `input.stopRecordScript` | `EndScriptRecording` into the command's arena, then `Zenith_DevFileWriter::Write(<name>, bytes)`. It replies `input.stopRecordScript: <events> events, <ticks> ticks, <dropped> dropped -> <name>`, or the writer's error. With no recording active it replies `input.stopRecordScript: not recording` |

- The file lands where the development file writer writes (`Zenith2/Docs/Design/DEVT/Console.md` → "Zenith_DevFileWriter header"). A person copies it into the game's `Acceptance/` directory and commits it.
- A recording still active at shutdown is discarded with one warning. Nothing is written after shutdown has begun.
- The recorder never changes a command, so recording does not change simulation. A recorded script replayed from startup with `--input-script` and `exclusive: true` reproduces the recorded action commands tick for tick: with `exclusive` the mappers see no device input, so each tick's commands are exactly the script's events, and their `bits` restore each command's bytes (`Test_InputScript_RecordReplayRoundTrip`).
- Only the input world's action commands are recorded. Presentation input (UI navigation, the console itself) and control events are not, and nothing before `input.recordScript` is recorded, so a recording session starts recording before it gives the game its first input.

### Latency attribution

- `Zenith_InputTickSummary::m_ulNewestEventSequence` and `m_ulNewestEventTimestampNs` are the sequence and platform timestamp of the newest transition the tick's mapper consumed; the render-extraction node copies them into `Flux_RenderWorld` (owned by `Zenith2/Docs/Rendering.md` → "Purpose and scope"), so that each present can be attributed to its input (`Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Pipelining and latency").
- The MP-LATENCY harness injects a `/virtual/latencyProbe/pulse` transition every 2 s with `InjectSyntheticTransition` (`INPUT_TRANSITION_FLAG_LATENCY_PROBE`); it is bound to `Dev.LatencyProbe` in `Engine.Simulation`, so its command proves that simulation consumed it, and the summary's `m_ulLatencyProbeSequence` carries the probe's sequence to the present (`Zenith2/Docs/Budgets.md` → "Measured in every workload run", row Z2-B-RPROD-027).

## Script-facing input API

The bindings are registered by the composition root in `Zenith2/Engine/ZenithRuntime/Internal/ScriptBindings/Zenith_InputScriptBindings.cpp`, because ZenithPlatform is below ZenithReflect (`Zenith2/Docs/Scripting.md` → "Modules, bindings and generated types").

| Luau function | Contexts | Effect | Returns or does |
|---|---|---|---|
| `Input.IsHeld(ctx, action)` | system, behaviour | read world (`Zenith_InputActionStateResource`) | boolean |
| `Input.WasPressed(ctx, action)`, `Input.WasReleased(ctx, action)` | system, behaviour | read world | boolean, this tick |
| `Input.Performed(ctx, action)`, `Input.Cancelled(ctx, action)`, `Input.Started(ctx, action)` | system, behaviour | read world | boolean, this tick's phase flags |
| `Input.Value(ctx, action)` | system, behaviour | read world | number (AXIS1D), vector (AXIS2D, AXIS3D, DELTA2D; z = 0 for 2D) |
| `Input.TapCount(ctx, action)`, `Input.HeldSeconds(ctx, action)` | system, behaviour | read world | number |
| `Input.PushContext(ctx, context)`, `Input.PopContext(ctx, context)` | behaviour, system | buffered (`uSIDE_EFFECT_MASK_FIELD_COMMAND`, module command kinds `Input.PushContext` and `Input.PopContext`) | nothing; effective from the next tick's phase 1 |
| `Input.PlayHaptic(ctx, effect)`, `Input.StopHaptic(ctx, channel)` | behaviour, system | buffered (module command kinds `Input.PlayHaptic` and `Input.StopHaptic`) | nothing; played in the frame's epilogue |
| `Input.ActiveScheme(ctx)` | behaviour | read world (`Zenith_FrameInputResource`) | `"KeyboardMouse"`, `"Gamepad"` or `"Touch"` |

- `action` and `context` arguments are names; they resolve to IDs when the module loads (the resolved table is part of the module's load context), and a name the loaded bindings do not declare raises the catalogued unknown-name misuse error of `Zenith2/Docs/Scripting.md` → "Views epochs and misuse errors" at load, never an assert.
- Every read binding names its world resource, so a module that calls it lists the resource in its `Uses` key and the schedule compiler sees the access (`Zenith2/Docs/Scripting/Bindings.md` → "Binding registration and the thunk ABI").
- The `effect` table of `Input.PlayHaptic` has the keys `kind` (`"Rumble"`, `"TriggerRumble"`, `"DeviceVibration"`), `channel` (`"Gameplay"`, `"UI"`, `"Cinematic"`), `target` (`"ActiveDevice"` or `"AllGamepads"`), `priority` (0–255, default 128), `low` and `high` (0–1), `durationMs` (1–10,000); a missing required key (`kind`, `durationMs`) or an unknown key is the catalogued bad-argument misuse error at the call; out-of-range numbers are clamped at playback. `Input.StopHaptic(ctx, channel)` takes a channel name.
- Platform events reach scripts as the event types `Platform.DeviceConnected`, `Platform.DeviceDisconnected`, `Platform.SchemeChanged`, `Platform.LowDisk`, `Platform.LowDiskCleared`, `Platform.AccountChanged`, `Platform.ConfigReset` and `Platform.LocaleChanged`, registered by the runtime bridge as `TYPE_KIND_EVENT` types and appended to the world's event queue by the tick-boundary publication of `Zenith2/Docs/Design/PLAT.md` → "Frame-graph placement", delivered in phase 9 of that tick. Device IDs, account identifiers and window IDs are never event fields; the fields are:

| Event type | Fields (from the recorded payload) |
|---|---|
| `Platform.DeviceConnected` | `DeviceKind` (`"Keyboard"`, `"Mouse"`, `"Gamepad"`, `"Touchscreen"`, `"Pen"`, `"Virtual"`), `GamepadType` (the `Zenith_GamepadType` name in upper camel case, e.g. `"Ps5"`, `"XboxOne"`, or `"Unknown"`), `Reconnected` (boolean) |
| `Platform.DeviceDisconnected` | `DeviceKind`, `GamepadType`, `LastOfKind` (boolean) |
| `Platform.SchemeChanged` | `Scheme` (`"KeyboardMouse"`, `"Gamepad"`, `"Touch"`), derived from the device payload of the device that switched it |
| `Platform.LowDisk` | `FreeBytes`, `MarginBytes` (numbers), `Reason` (`"WriteRefused"`, `"DiskFull"`, `"Poll"`) |
| `Platform.LowDiskCleared` | `FreeBytes` |
| `Platform.AccountChanged` | `Kind` (`"SignedIn"`, `"SignedOut"`, `"Switched"`, `"SessionLocked"`, `"SessionUnlocked"`), `Source` (`"Os"`, `"OnlineBackend"`) |
| `Platform.ConfigReset` | `Layer` (`"User"`), `Reason` (`"UserFileCorrupt"`, `"InputBindingsCorrupt"`) |
| `Platform.LocaleChanged` | `Locale` (the preferred locale tag, `language-COUNTRY`) |

