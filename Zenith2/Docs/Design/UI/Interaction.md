# UI design: input, focus, navigation and the simulation boundary

This file is part of the UI design family; the entry document is `Zenith2/Docs/Design/UI.md`. It specifies where UI work runs in a frame, how UI claims input from PLAT, hit testing, the pointer and focus state machines, directional and tab navigation, controls, scrolling, text fields with IME, input arbitration with gameplay, input prompts and remapping, on-screen controls, and the two crossings between presentation and simulation: UI events into simulation as recorded publications, and requests from simulation into UI as command kinds. Input devices, actions, contexts, bindings and IME are PLAT's (`Zenith2/Docs/Design/PLAT/Input.md`); UI never reads SDL and never names a device in game content.

## Scope of this file

| Topic | Section |
|---|---|
| per-frame placement of UI work | "Frame flow" |
| PLAT's claimant interface as UI implements it | "Input claims" |
| pointer and hit testing | "Hit testing", "Pointer state machine" |
| focus | "Focus model", "Directional navigation algorithm", "Tab order" |
| controls | "Activation and value controls", "Scrolling", "Text fields and IME" |
| gameplay coexistence | "Input arbitration with gameplay" |
| prompts, remapping, touch controls | "Input prompts and remapping", "On-screen controls" |
| presentation to simulation | "UI events to simulation" |
| simulation to presentation | "Requests from simulation" |

## Frame flow

| Where | Node or step | UI work |
|---|---|---|
| frame prologue step 1, `Platform.Pump` (main thread) | the claimant's first callback of the frame | under `--replay`, the `MENU` resume check of § "Pause menus"; `Zenith_UISystem::EndFrame` for frame k−1 (dynamic page uploads), then `BeginFrame` (open requested faces, commit a pending locale, rebuild font stacks, `ProcessRequests` for glyphs, apply a changed `ui.*` configuration read at the previous frame's step 3), then the input world's surface (display metrics, safe area) |
| `Platform.Pump`, `ClaimPointers` | `Zenith_UIInputClaimant` | hit-test new pointer presses against the last layout and claim them |
| `Platform.Pump`, `ClaimActions` | `Zenith_UIInputClaimant` | navigation, submit, cancel and scroll from the presentation actions; pointer motion and releases; text input; Engine.UI context management; event publication (§ "Publication path") |
| tick boundary step 1, every world | UI's publication handler (through the publication sink's runtime-side forwarder, row UI1) | decode each UI publication into the module's per-world publication buffer |
| phase 1, every world | `UI.Events@W<tag>` | emit the `UI.*` events, record two-way binding write-backs |
| sync points after phases 2–10 | command playback | `UI.Focus`, `UI.ScrollTo`, `UI.PlayAnimation`, `UI.Announce`, `UI.ShowCaption`, `UI.ClearCaptions` append to `Zenith.UI.Requests` or `Zenith.UI.Captions` |
| phase 9 (`PHASE_EVENTS`), every world (Stage 4) | `UI.Captions@W<tag>` (a C++ event handler) | delivered caption cue events (row UI4) into `Zenith.UI.Captions` |
| phase 12 (EPILOGUE), every world | the extractor `UI.Extract@W<tag>` | screen sync, requests, bindings, `Update` (animations, styles, layout), draw lists into the render world, the semantic tree, narration requests |

The prologue's work happens before the frame graph starts and the extractor runs after the frame's last tick, so a context is touched by one thread at a time ([Z2-N-ARCH-384]); input is hit-tested against the layout extracted in frame k−1, which is the layout on screen when the player pressed.

## Input claims

`Zenith_UIInputClaimant` (`Internal/Input/Zenith_UIInputClaimant.cpp`) implements PLAT's `Zenith_InputClaimant` (`Zenith2/Docs/Design/PLAT/Input.md → "Presentation claims"`).

<!-- assumption: pending PLAT (UI6) -->
The UI module registers its claimant in start step 7 with `Zenith_InputSystem::RegisterClaimant` through `Zenith_ModuleStartContext::GetPlatform()` and keeps the token, unregistering it at shutdown, so UI is registered before the editor and the first claim wins; PLAT's contract currently says the composition root registers claimants, and row UI6 asks PLAT to admit a module registering its own.
<!-- /assumption -->

<!-- assumption: pending MOD, PLAT (UI3) -->
Only the input world's context receives input: the module reads which world receives presentation input this frame through `Zenith_WorldAttachContext::IsInputWorld()` (kept per attached world) and a change notification delivered on the main thread before `Platform.Pump`; a world that stops being the input world has its pointers cancelled (the pointer state machine's cancel row) and its text session ended.
<!-- /assumption -->

`ClaimPointers(axDowns, xClaims)`, for each pointer down in order:
1. hit-test its pixel (§ "Hit testing");
2. when the hit is an element with `pointerEvents: auto` of an interactive screen, or the hit screen blocks input, call `ClaimPointer(i)` and record the press for `ClaimActions`;
3. otherwise leave it for gameplay (the gesture recognisers and the simulation mapper see it).

`ClaimActions(xFrame, xClaims)`:
1. translate the presentation actions of `Engine.UI` (`UI.Navigate`, `UI.Submit`, `UI.Cancel`, `UI.TabNext`, `UI.TabPrevious`, `UI.Scroll`; `Zenith2/Docs/Design/PLAT/Input.md → "Engine actions and contexts"`) into `Zenith_UINavInput`: `UI.Navigate` takes the dominant axis of its AXIS2D value when its magnitude passes 0.5, with hold repeat after `ui.input.repeatDelayMs` (400) every `ui.input.repeatIntervalMs` (80); every input `FeedNavigation` consumes is followed by `ClaimAction` of its action;
2. feed the claimed pointers' transitions (moves, releases, cancels) and the unclaimed mouse position (hover) from `xFrame.m_axTransitions` as `Zenith_UIPointerInput`, and wheel input over a screen;
3. feed `xFrame.m_axTextEvents` and, while a text field edits, the raw keyboard transitions as `Zenith_UITextInput` (§ "Text fields and IME");
4. push `Engine.UI` on the presentation stack with `PushPresentationContext` when `HasFocusScope()` became true, remove it when it became false (effective at the next Pump); begin, update or end the `Zenith_TextInput` session as `IsTextEntryActive()` changes;
5. drain the context's events and publish them (§ "Publication path"); a drained `Zenith_UIEventRecord` whose `m_uPresentationAction` is 1 (`resumeWorld`: an `Activated` event of a `button` whose TYPEDATA `presentationAction` is `resumeWorld`, or a `Cancelled` event of a screen whose header byte 23, `cancelAction`, is `resumeWorld`; the context sets the field when it produces the event) runs the steps of § "Pause menus" before its ordinary event is published, and a record carrying 0 is published directly.

## Hit testing

`Zenith_UIContext::HitTest(pixel)`:
1. Screen-space screens in descending (layer, open order) are tested first, because screen-space UI is composited over the world; world-space panels are tested after them, nearest first along the main camera ray ("World-space panels" of Layout.md).
2. Within a screen, elements are tested in reverse paint order (the last child first): an element is skipped with its subtree when the point lies outside its clip rectangle; a visible element whose `pointerEvents` is `auto` is hit when the point, transformed by the inverse of its visual transform, lies inside its border box, corners excluded outside the corner radius circles; `passThrough` elements are not hit but their children are; `none` excludes the subtree.
3. The result is the deepest hit element; its activation target is the nearest inclusive ancestor of an interactive type that is enabled.
4. A screen with `m_bBlocksInput` is hit everywhere on its canvas (`m_bBlocking`), so nothing below receives the pointer.
5. Cost: O(elements of the screens tested) with subtree pruning by clip; at most a few hits per frame.

## Pointer state machine

One machine per pointer (mouse 0, touch 1–10, pen 11); `e` is the activation target, `c` the captured control.

| State | Event | Next | Action |
|---|---|---|---|
| IDLE | move over element e (mouse, pen) | HOVER(e) | set `hover` on e and its ancestors up to the screen |
| HOVER(e) | move over e′ | HOVER(e′) | move the hover flags |
| HOVER(e), IDLE | press on e | PRESSED(e) | capture; set `pressed`; focus e when focusable (focus not visible); a text field places its caret |
| PRESSED(e) | move within `ui.input.dragThresholdDp` (8 dp) | PRESSED(e) | — |
| PRESSED(e) | move beyond the threshold, e is a slider | DRAG(e) | the value tracks the pointer along the slider axis |
| PRESSED(e) | move beyond the threshold along a scroll axis of a scroll ancestor s | SCROLL(s) | clear `pressed` on e (no activation); scroll follows the pointer |
| PRESSED(e) | move beyond the threshold, e is a text field | SELECT(e) | extend the selection to the hit caret |
| PRESSED(e) | release inside e | HOVER(e) or IDLE (touch) | activate e (§ "Activation and value controls") |
| PRESSED(e) | release outside e | HOVER or IDLE | no activation |
| DRAG(e) | release | HOVER or IDLE | commit the value (`UI.ValueChanged`) |
| SCROLL(s) | release | IDLE | inertia from the velocity of the last 100 ms of samples, decaying by 0.95 per 16.67 ms until below 1 dp per frame; none under reduced motion |
| SELECT(e) | release | HOVER(e) | — |
| any | cancel (a synthetic release, a lost claim, a disconnected device) | IDLE | clear flags; no activation, no commit |
| any | the captured element becomes hidden, disabled or recycled | IDLE | as cancel |
| any | wheel | unchanged | scroll the deepest scroll container under the pointer that can move in the wheel direction by notches × `ui.scroll.wheelStepDp` (48 dp) |

Two presses on a text field within 400 ms and 8 dp select a word, three select the line.

## Focus model

### Focus scopes

- A screen takes keyboard and gamepad focus only when it is visible, interactive, its document's root member `focusScope` is `auto` (the default; `never` makes a pointer-only screen, the HUD and on-screen controls) and it has at least one visible, enabled, focusable element.
- The context's active scope is the highest-layer screen that qualifies; a blocking screen above every other screen makes the screens below inactive even when they qualify.
- `HasFocusScope()` is true while an active scope exists; that is the condition under which `Engine.UI` is on the presentation stack and navigation is consumed.

### Focus state machine

| State | Event | Next | Action |
|---|---|---|---|
| NO_SCOPE | a scope becomes active | FOCUSED(i) | i = the screen's remembered element, else `Zenith.UI.Screen.InitialFocus`, else the `nav.initial` element, else the first focusable in tab order |
| FOCUSED(s, e) | navigate d | FOCUSED(s, e′) | e′ by § "Directional navigation algorithm"; unchanged when none |
| FOCUSED | next, previous | FOCUSED | § "Tab order" |
| FOCUSED(s, e) | e hidden, disabled, removed or recycled | FOCUSED(s, e″) or SCOPE_EMPTY | e″ = the next focusable in tab order after e, else the previous; a recycled list item moves focus to its list, keeping the item index (Layout.md "Item recycling") |
| any | pointer press on a focusable e of the active scope | FOCUSED(s, e) | focus not visible |
| FOCUSED(s, e) | the scope becomes inactive (closed, hidden, covered) | the new active scope's initial | s remembers e |
| any | `UI.Focus` request for (screen, element) | FOCUSED or unchanged | applied when the element is focusable and its screen is or becomes the active scope; otherwise remembered on the screen and applied when it becomes active; an empty element clears the screen's remembered focus |
| any | `m_uStateSerial` of the screen changed | FOCUSED(s, initial) | reapply the initial focus |
| SCOPE_EMPTY | a focusable element appears in the scope | FOCUSED | the first in tab order |

Every transition that changes the focused element raises a narration request (Accessibility.md "Narration composition") and sets `STYLE_DIRTY` on the old and new elements.

### Focus visibility

The `focusVisible` state (the focus ring) is set on the first navigation, tab or submit input and cleared on a pointer press, as CSS `:focus-visible` behaves; a text field shows its caret whenever focused.

## Directional navigation algorithm

<!-- elaboration: the navigation score -->
For the focused element F (border box in the screen canvas, scroll offsets applied) and a direction d:

1. An explicit `nav.<d>` target that is visible, enabled and focusable wins.
2. Candidates are the visible, enabled, focusable elements of F's innermost focus group (`nav.group`), excluding F; inside a virtualized list, the item before or after the focused item counts as a candidate at its computed position even when not materialised.
3. A candidate C qualifies for d = right when `C.centre.x > F.centre.x` and `C.right > F.right` (left, up and down by symmetry).
4. Score = `primary + 4 × orthogonalGap + 0.1 × |C.centre⊥ − F.centre⊥|`, where `primary` is the distance from F's edge in d to C's opposite edge (0 when they overlap) and `orthogonalGap` is the gap between their projections on the perpendicular axis (0 when they overlap); the lowest score wins, ties by document order.
5. No candidate: with `nav.wrap`, the group's qualifying element farthest in −d with the smallest orthogonal gap; without wrap, the search repeats at the next enclosing group up to the screen; still none: focus stays.
6. After a move, the nearest scroll ancestor scrolls the focused element fully into view with an 8 dp margin; a list scrolls by one item and materialises it before focus moves onto it.

Cost O(focusable elements of the scope).

## Tab order

Next and previous follow the ascending (`nav.order`, document order) sequence of the active screen's focusable elements, wrapping at the ends; an element with `nav.trap` keeps the sequence inside its subtree while it is visible.

## Activation and value controls

| Type | Activation (submit, or press and release) | Directional input when focused | Event |
|---|---|---|---|
| `button` | activate | navigation | `UI.Activated` |
| `toggle` | flip `checked`; in a `group`, check this one and clear the others | navigation | `UI.Toggled` for each toggle whose state changed |
| `slider` | — | along its axis: ± `step`, or 1/20 of the range when continuous, with repeat; across its axis: navigation | `UI.ValueChanged` per committed step |
| `choice` | next option | left and right cycle; up and down navigate | `UI.SelectionChanged` |
| `list` (selectable) | activate the focused item | navigation within and out of the list | `UI.Activated` with `Item`; `UI.SelectionChanged` |
| `textField` | begin editing | caret movement while editing | `UI.TextCommitted` on commit |
| `rebind` | begin a rebind | navigation | none (the change is PLAT's binding data) |
| `scroll` | — | `UI.Scroll` scrolls | none |

- Cancel: while editing, reverts the field to its text at edit start and ends editing (claimed); otherwise, when the active screen's document declares `cancelEvent`, emits `UI.Cancelled` and claims the action; without `cancelEvent` the action is not claimed and reaches lower contexts (a HUD-only game's pause binding).
- A disabled element receives no activation, no focus and no value change.
- A committed value is shown until the bound simulation value equals it or 30 frames pass, after which the binding drives the control again (Layout.md "Data binding evaluation"), so the one-tick round trip never flickers.
- Controls bound to a `config` source write the value with `Zenith_Config::SetUserValue` in the same callback; such writes never produce a publication, because configuration of the presentation kind is not simulation state.

## Scrolling

- Offsets clamp to the scroll extent after every layout; `UI.Scroll` scrolls the scroll container nearest to the focused element at 1,200 dp per second × axis value; focus moves scroll elements into view (§ "Directional navigation algorithm" step 6).
- `UI.ScrollTo` requests scroll a named element or list item to the top of its viewport, clamped.
- Smooth scrolling (a 150 ms ease-out to the target) and inertia are presentation motion: reduced motion makes every scroll immediate.
- A scrollbar (`scrollbar: auto`) is drawn while the extent is non-zero and is draggable as a slider over the offset.

## Text fields and IME

1. Editing begins on activation (gamepad or keyboard) or a press: `Zenith_TextInput::Begin` with the window, the field's `purpose` mapped to `Zenith_TextInputPurpose`, `m_xArea` the field's rectangle in window points (pixels ÷ the window's pixel density), `m_bMultiline`, autocorrect and sentence capitalisation on for `text` purpose only, `m_uMaxBytes` the remaining capacity (`Zenith2/Docs/Design/PLAT/Input.md → "Text input API"`); a refused `Begin` (another session is active) leaves the field unfocused-for-editing and counts `ui.text_field.begin_refused`.
2. While editing, PLAT pushes `Engine.TextEntry`, which consumes the keyboard for every lower context (`Zenith2/Docs/Design/PLAT/Input.md → "Text entry consumes keyboard controls"`); UI reads raw key transitions for Backspace, Delete, the arrows, Home, End, Return, Ctrl+A, C, X, V and their repeats.
3. Composition events update the buffer's pre-edit text; commits insert; the caret's area is reported with `UpdateArea` whenever the caret moves.
4. On Android the soft keyboard's inset changes the safe area; the field's nearest scroll ancestor scrolls it into the remaining area, and `IsSoftKeyboardShown` hides keyboard prompts.
5. Editing ends on commit (Return when `commitOnEnter`, or the soft keyboard's done key), cancel, focus loss or screen close; commit publishes `UI.TextCommitted` with the field's text (at most 256 bytes published; a longer field's text is truncated at a grapheme boundary for the event and counted `ui.events.text_truncated`).

## Input arbitration with gameplay

| Situation | What UI consumes | What gameplay sees |
|---|---|---|
| a pointer press on a hit-testable element of an interactive screen | that pointer from press to release (claimed: `INPUT_TRANSITION_FLAG_CLAIMED`, `_CONSUMED`) | the pointer at rest; no gesture |
| a pointer press elsewhere | nothing | everything |
| a blocking screen visible | every pointer; `Engine.UI` consumes the navigation controls | nothing of those controls; held actions release (PLAT consumption rule 3) |
| an active focus scope (a menu) | the `Engine.UI` actions UI claims, from (arrows, Return, Escape, Tab, D-pad, south and east buttons, shoulders, right stick, wheel) | the other controls |
| a text field editing | the keyboard (`Engine.TextEntry`) | gamepad and touch |
| HUD screens (`focusScope: never`, not blocking) | only pointers over their hit-testable elements | navigation and keys |
| on-screen controls | the touches on them; the controls' values reach simulation as a virtual device (§ "On-screen controls") | the virtual device's actions through the game's bindings |

Every consumption above is presentation: what simulation receives is decided by PLAT's simulation mapper over unclaimed, unconsumed transitions, recorded as input commands, so replays reproduce gameplay input exactly.

## Input prompts and remapping

- `inputPrompt` elements and inline `<prompt action="…"/>` objects show an action's active bindings for the active scheme. `Zenith_InputSystem::GetBindingDisplay` is main-thread, so the claimant callback queries every (action, context) pair the input world's open documents use whenever the active scheme, the binding table or the open screen set changed, copying the results (glyph family, control path, label, part) into the context's prompt cache; the extractor reads the cache (`Zenith2/Docs/Design/PLAT/Input.md → "Binding display"`, "Gamepad types and glyph families").
- A prompt draws the theme sprite named `prompt:<family>:<control path>` (`prompt:xbox:/gamepad/south`, families `keyboard`, `mouse`, `xbox`, `playstation`, `nintendo`, `touch`) when the theme defines it, otherwise a key cap: a rounded rectangle in the theme class `prompt.keycap` holding the binding's label text; engine content ships no prompt images.
- `rebind` elements: activation calls `Zenith_InputSystem::BeginRebind` with the element's action, context, scheme and binding index (`Zenith2/Docs/Design/PLAT/Input.md → "Rebind operation"`); while the operation runs, the element shows the engine string `engine.rebind.press` and UI feeds no navigation (PLAT's rebind capture owns the controls); the claimant polls `GetRebindResult` each Pump; on success or conflict the element shows the new binding or the engine string `engine.rebind.conflict` with the conflicting action's name, and the conflict policy is PLAT's (`Zenith2/Docs/Design/PLAT/Input.md → "Conflict detection"`); a `rebind` whose `reset` member is `action` calls `ResetUserBindings` for its action (`REBIND_RESET_SCOPE_ACTION`) on activation instead of rebinding, and one whose `reset` is `all` resets every binding (`REBIND_RESET_SCOPE_ALL`). Saving user bindings is PLAT's (`SubmitUserBindings`).

## On-screen controls

- At start step 7 the UI module creates one virtual device `ui` (`Zenith_InputDevices::CreateVirtualDevice`, scheme kind touchscreen) whose controls are the union of the `virtualControl` names of every `onScreenControl` element the game's cooked documents declare (listed in the binding manifest; at most 32: `BUTTON` for buttons, `AXIS2D` for sticks); a game binds `/virtual/ui/<name>` in its bindings files like any control.
- A claimed touch on a control drives it: a button sets 1 while pressed and 0 at release; a stick sets the normalised offset from its centre (fixed mode) or from the touch-down point (floating mode), radial deadzone `deadzone`, clamped to the unit disc; values are set with `SetVirtualControl` every frame the value changes and take effect at the next Pump, so they flow through the simulation mapper into replayed input commands (`Zenith2/Docs/Design/PLAT/Input.md → "Zenith_InputDevices API"`).
- Controls hide when the active scheme is not touch unless the screen keeps them visible; the device is destroyed at module shutdown, which synthesises releases.

<!-- informative -->
Zenith's on-screen stick and button published through a "virtual binding source" consumed by its own input layer. Zenith2 keeps the idea but routes the values through PLAT's virtual devices, so on-screen controls are ordinary bound controls, rebindable and recorded like a gamepad.
<!-- /informative -->

## UI events to simulation

### Event kinds and payloads

<!-- elaboration: the payload layout -->
A UI event that simulation can observe is published once, as a payload of `uUI_PUBLICATION_MAX_BYTES` (448) bytes at most, little-endian, field by field:

| Offset | Size | Field |
|---|---|---|
| 0 | 2 | kind (`Zenith_UIEventKind`) |
| 2 | 1 | element name bytes E (0–64) |
| 3 | 1 | event name bytes V (0–64) |
| 4 | 2 | text bytes T (0–256; TEXT_COMMITTED only) |
| 6 | 2 | reserved (0) |
| 8 | 4 | item index (sint32, −1 outside lists) |
| 12 | 4 | write-back property ID (0 for none) |
| 16 | 8 | the screen entity's stable simulation ID |
| 24 | 8 | value bits (VALUE_CHANGED: the f64; TOGGLED: 0 or 1; SELECTION_CHANGED: the sint32 sign-extended; otherwise 0, including `UI_EVENT_KIND_WORLD_RESUMED`, whose E, V and T are 0) |
| 32 | 8 | write-back component type ID (0 for none) |
| 40 | E | element name (UTF-8) |
| 40 + E | V | event name |
| 40 + E + V | T | text |

The payload names nothing platform-scoped and no world handle ([Z2-N-CONTRACT-064]); `Zenith_EncodeUIPublication` and `Zenith_DecodeUIPublication` are the only writer and reader. The text is validated at encode: it is UTF-8 produced by the text-edit buffer, and a text containing an unpaired surrogate encoding or any ill-formed sequence (possible only from a platform text event that bypassed the buffer) is rejected at encode, the event dropped and counted `ui.events.text_rejected` (`Test_UIPublication_RejectsIllFormedText`); a text longer than 256 bytes is truncated at a grapheme boundary before encode (§ "Text fields and IME").

### Publication path

<!-- elaboration: UI events as recorded GENERIC publications, decoded at tick-boundary step 1 into module state, emitted in phase 1 -->
1. `ClaimActions` drains the input world's context events in production order.
2. For each event, the replay check comes first. Under `--replay` (the world's sink reports `IsReplaying()`), every live event is dropped here, before anything is allocated, and counted `ui.replay.live_events_ignored`: UI never calls `AllocateCompletionID` for live input in a replay. [Z2-N-SCRIPT-341] gives each world one "monotonic completion-ID counter" owned by `Zenith_AsyncResultTable` and shared by every source kind of the world, so one allocation by a viewer's click would shift the IDs of every subsequent allocation (SCENE, ANIM, and the recomputed results [Z2-N-SCRIPT-341] holds ticks for) and break the bit-for-bit replay of [Z2-N-ARCH-783]; platform events follow the same rule, since a replay "ignores live platform events" ([Z2-N-ARCH-791]). Live input never allocates; that the counter reproduces the recording for the IDs UI allocated live in the recorded run is the replaying table's obligation, not UI's (the assumption below). Outside a replay, the module allocates a completion ID from the world's module publication sink (`AllocateCompletionID` on the main thread in the frame prologue, which [Z2-N-SCRIPT-341] admits) and parks it at once (`Park`) with publication kind `PUBLICATION_KIND_GENERIC_RECORDED` and the encoded payload, under the source kind `ASYNC_SOURCE_UI` (`Zenith2/Docs/Scripting.md → "Asynchronous source kind registry"`, [Z2-N-SCRIPT-925]; UI writes no numeric value of its own). Two limits apply per world, and each keeps one slot for a resume record (§ "Pause menus"): at most `ui.events.maxPerFrame − 1` ordinary events (default 63) are published per frame, and at most `ui.events.maxInFlight − 1` ordinary completions (default 255) are outstanding, that is parked but not yet published because no tick of the world has run since (a paused world accumulates them). UI counts its ordinary outstanding completions as `GetOutstandingCount() − (m_bResumeOutstanding ? 1 : 0)`, where `Zenith_UIWorldRecord::m_bResumeOutstanding` is set when `Park` of a resume record returns true and cleared when UI's handler decodes that record at the tick boundary (§ "Pause menus"). The reserved slot of each limit admits only a `UI_EVENT_KIND_WORLD_RESUMED` record, and at most one resume record is outstanding per world (§ "Pause menus"), so UI's own count never exceeds the `m_uMaxInFlight` = `ui.events.maxInFlight` UI declared for its source and the sink never refuses a resume record for capacity. An ordinary event beyond either limit is dropped and counted `ui.events.dropped` (the player's action is lost and the control returns to its bound value; `Test_UIEvents_ChannelFullDropsAndCounts`).

The replaying table reproduces the counter, as core text states since cross-doc item X278 ([Z2-N-ARCH-794], [Z2-N-SCRIPT-341], the `Zenith_AsyncResultTable` contract of `Zenith2/Docs/Scripting/PublicTypes.md → "ZenithScript asynchronous results and host interfaces"`). The ZRPL `TICK` record (`REPLAY_RECORD_TICK`, `Zenith2/Docs/Architecture.md → "Fixed ticks and replayable commands"`) carries the world's next completion ID at the tick boundary: the value of the world's counter after every frame-prologue allocation of that frame and before the tick's first merge-chain allocation. Under `--replay` the world's `Zenith_AsyncResultTable` adopts the recorded value at that point, so the IDs UI's live publications consumed in the recorded run are skipped exactly and every subsequent allocation of the world (a SCENE additive load, an ANIM binding, an AI or PHYS park, a script request) receives its recorded ID; a live counter above the recorded value is a replay divergence, on which the world halts with a named diagnostic. An allocation outside a tick (the frame prologue) is made only for a RECORDED publication kind, which `ASYNC_SOURCE_UI` is, and under `--replay` a prologue producer of a recorded kind allocates nothing, which this step does. The replay mode of `Zenith2/Docs/Design/SCRIPT/Runtime.md → "Asynchronous result table"` is SCRIPT's design half of the same rule (orchestrator ruling Q89 (a)), and the core text above governs it; UI's publication ticket (slice 11) and its replay test rest on this rule (cross-area row UI1 (f)).
3. At the world's next tick boundary that runs (a paused world's tick boundary runs none of its steps, `Zenith2/Docs/Design/GAME/WorldHost.md → "Pause arbitration"`), the outstanding results publish in completion-ID order ([Z2-N-SCRIPT-335]); UI's handler, called at tick-boundary step 1 through the sink's runtime-side forwarder, decodes each payload (a malformed payload, possible only from a corrupt replay, is dropped and counted `ui.events.decode_failed`), clears `Zenith_UIWorldRecord::m_bResumeOutstanding` when the decoded record is a `UI_EVENT_KIND_WORLD_RESUMED` record, and appends the decoded record to the module's per-world publication buffer `Zenith_UIWorldRecord::m_xPublished` (`Internal/Zenith_UIPublication.cpp`; a bounded array of `ui.events.maxInFlight` records). The handler writes no world state at all, so it uses none of the property access of `Zenith_TickBoundaryAccess` ([Z2-N-SCRIPT-343]). The replay recorder records every publication through `Zenith_ScriptPublicationObserver` ([Z2-N-SCRIPT-341]).
4. Phase 1, `UI.Events@W<tag>` (C++ system, `PHASE_INPUT`, reads `Zenith.UI.Screen`, emits the six event types, records commands): it takes the records of `m_xPublished` in order and empties it. The buffer is module state of that world alone, written only at the world's tick-boundary step 1 and read only by the world's phase-1 node, which the tick orders after the boundary ([Z2-N-ARCH-383]: each world's tick boundary node is the first node of its tick), so it needs no lock and no declared world access; it is empty at every sync point, so nothing of it is saved. For each record: find the screen entity by its stable simulation ID among the query's rows (a linear scan of at most `ui.screens.maxPerWorld` rows; absent: the screen was destroyed since the click; dropped, counted `ui.events.stale_screen`), intern the element, event and text names (`Zenith_StringTable::Intern`; a full intern table drops the event and counts `ui.events.text_intern_failed`), and emit the event through `Events(screen simulation ID)`; delivery is phase 9 of the same tick in merge order.
5. A record with a write-back records `Commands(simulation ID).SetProperty(screen, type, property, value)`, applied at the sync point after phase 1 (§ "Two-way bindings").

### Pause menus

A paused world runs no tick and so publishes nothing until it resumes; an event made meanwhile is delivered at the first tick after resumption, as long as fewer than `ui.events.maxInFlight − 1` ordinary completions of the world are outstanding when it is made (step 2). Because a paused world runs no script, a pause menu is resumed by UI itself (GAME's row GA13, adopted): a script opens the menu screen and requests the `MENU` pause reason (`Game.RequestPause(ctx, "Menu")`); a `button` whose `presentationAction` is `resumeWorld`, or Cancel on a screen whose document's `cancelAction` is `resumeWorld` (a drained event record carrying `m_uPresentationAction` 1, § "Input claims" `ClaimActions` step 5), runs these steps in the claimant callback on the main thread, outside a replay (under `--replay`, rule 1 below), in this order:

1. When the input world's control has no requested `MENU` reason (it was already cleared this frame, or never set), the action skips to step 4.
2. **Publish first.** The action allocates a completion ID and parks a `UI_EVENT_KIND_WORLD_RESUMED` record (the screen's stable simulation ID, no names, no value) through the reserved slot of both limits of § "Publication path" step 2. The slot is free unless a resume record of this world is still outstanding, that is unless `Zenith_UIWorldRecord::m_bResumeOutstanding` is set: `Park` returning true sets it, and UI's handler clears it when it decodes the resume record at the world's tick boundary, which publishes it before any script can request `MENU` again; a second `resumeWorld` in the same frame finds `MENU` already cleared and stops at step 1.
3. **Then resume, only on success.** When `Park` returned true, the action calls `Zenith_WorldControl::RemovePauseReasons(WORLD_PAUSE_REASON_MENU)` on the input world's control (effective at that frame's prologue step 7). When the record could not be parked (the reserved slot is still in use, or the sink refused it), the action does not touch the control: the world stays paused, the menu stays open, `ui.events.resume_refused` counts it, and the player's next activation retries. A live run therefore never clears `MENU` without a resume record in the log, and the log never holds a resume record whose clear did not happen, so no replay can stall on a resume the recording made.
4. The action publishes its ordinary event (`UI.Activated` or `UI.Cancelled`) under the ordinary limits of step 2; dropping it never undoes the resume.

The resume record therefore publishes at the first resumed tick, ahead of the ordinary event, and is recorded in the replay log with that tick. UI never sets a pause reason itself, and `UI.Events` emits no ECS event for a resume record: the ordinary event that follows is what the menu's script handles, by closing the screen.

<!-- elaboration: the replay rule for MENU, coordinated with GAME through row UI22 -->
Replay rule: GAME's pause arbitration stands (a paused tick does not exist, so the ticks and their publications are the same whatever the pause pattern), except that clearing `MENU` is a presentation input, so a replay takes it from the log, as a recorded publication, never from the viewer. Under `--replay`:

1. No live input reaches the sink: § "Publication path" step 2 drops every live event before allocation and counts `ui.replay.live_events_ignored`, so live input never allocates; that the counter reproduces the recording for the IDs UI allocated live is the replaying table's obligation ([Z2-N-ARCH-794]; § "Publication path"); the publication source supplies every `ASYNC_SOURCE_UI` result ([Z2-N-ARCH-783]), and the sink refusing allocation and completion for this recorded kind under `--replay` is only a backstop (row UI1). A live `resumeWorld` action allocates nothing, does not touch the world's control and counts `ui.replay.live_resume_ignored`, so a viewer pressing a replay's pause menu cannot change its course.
2. Premise: this rule needs the replay's frame boundaries to fall at the recording's ticks, and they do, because "replays drive the clock in external-tick mode from the log, so dropped real time never changes the tick sequence" (`Zenith2/Docs/Design/JOBS.md → "Determinism obligations"`): the frame whose tick T set `MENU` ends at the same tick P in the replay as live, so the next prologue's peek at P + 1 sees the tick where the recorded resume record published. In the claimant's first callback of every frame (prologue step 1, which precedes `Zenith_WorldHost_ApplyPauseMasks` in prologue step 7), for each attached world whose control's requested reasons contain `MENU`, UI asks the sink for the recorded publications of the world's next tick (`PeekRecorded(GetNextTick())`); when one of them is a `UI_EVENT_KIND_WORLD_RESUMED` record, UI calls `RemovePauseReasons(WORLD_PAUSE_REASON_MENU)`. Prologue step 7 then resumes the world, its next tick runs, and that tick's recorded UI publications (the resume record first, then the menu's ordinary event) reach the handler at tick-boundary step 1 exactly as in the recorded run.
3. A world paused by `MENU` whose log holds no resume record after the pause stays paused, which is how the recorded session ended.

`Test_UIPauseMenu_ResumeClearsMenuReason` records a session that pauses with `MENU`, resumes through a `resumeWorld` button after 30 frames and runs 60 more ticks, then replays it with no input and asserts that the replay passes the paused span, the resume record publishes at the recorded tick, and every tick's state hash matches; a second case fills the paused world's channel with 255 outstanding ordinary events, presses the resume button, and asserts that the resume record still publishes (ahead of the ordinary event, which is dropped and counted), `MENU` clears, and the replay of that session resumes at the same tick; a third case holds a resume record outstanding (the world also paused for `GAMEPLAY`), requests `MENU` again by calling `Zenith_WorldControl::AddPauseReasons(WORLD_PAUSE_REASON_MENU)` from the test on the main thread (which GAME's arbitration leaves to scripts in a tick, so only a test reaches this state), presses resume, and asserts that the control is untouched and `ui.events.resume_refused` counts. `Test_UIEvents_PausedWorldDefersPublication` asserts that an ordinary event clicked during a `GAMEPLAY` pause is delivered at the first resumed tick in the run and at the same tick in its replay.

<!-- assumption: pending MOD, SCRIPT (UI1: the single module publication sink) -->
UI's needs against the single module publication sink (one MOD post-signoff patch with the SCRIPT runtime-side forwarder), in the shape AI, PHYS, AUDIO, CINE and GAME already assume: per attached world, `Zenith_WorldAttachContext::GetPublicationSink()` returns the world's `Zenith_ModulePublicationSink&` (held by the world host record, `Zenith2/Docs/Design/GAME/WorldHost.md → "World host state"`, and valid until `DetachWorld` returns), and `Park(ulCompletionID, uSourceKind, uPublicationKind, xPayload)` parks one result, here `Park(ulID, ASYNC_SOURCE_UI, PUBLICATION_KIND_GENERIC_RECORDED, xPayload)`, returning `bool`: false under `--replay` for a RECORDED publication kind or when the source's `m_uMaxInFlight` results are outstanding. UI's extras beyond the peers' shape: (a) the source declaration at attach step 6, `DeclareSource(ASYNC_SOURCE_UI, m_uMaxInFlight = ui.events.maxInFlight, m_uMaxResultBytes = uUI_PUBLICATION_MAX_BYTES)` on the sink, with a module-side handler interface declared in ZenithRuntime (so ZenithUI includes no ZenithScript header) that SCRIPT's runtime-side forwarder calls at tick-boundary step 1 with each published payload of the source in completion-ID order, live and in replay alike; (b) `AllocateCompletionID(ASYNC_SOURCE_UI)` on the main thread in the frame prologue (phase-node allocation, which ANIM needs, is the sink's other site; UI does not use it), refused under `--replay` for a RECORDED kind (it returns an error and allocates no ID, so a module that forgot the replay check still cannot move the world's counter; UI checks `IsReplaying()` first and never relies on the refusal); (c) `GetOutstandingCount(ASYNC_SOURCE_UI)`, `IsReplaying()` and `GetNextTick()`; (d) under `--replay`, on the main thread in the prologue, `PeekRecorded(ASYNC_SOURCE_UI, ulTick)` returning the source's recorded payloads for that tick (the shape of `Zenith_ScriptPublicationSource::GetRecorded`, `Zenith2/Docs/Design/SCRIPT/PublicAPI.md → "Zenith_ScriptHost header"`); (e) `DetachWorld` discards the source's outstanding results; (f) the replaying table's adoption of the recorded next completion ID, core text since X278 ([Z2-N-ARCH-794]; § "Publication path").
<!-- /assumption -->

<!-- assumption: pending MOD (UI19; GAME's row GA5 (a)) -->
The control pointer is the one `Zenith_WorldAttachContext::GetWorldControl()` gives the module at attach (`Zenith2/Docs/Design/GAME/WorldHost.md → "Pause arbitration"` declares `Zenith_WorldControl`; MOD adds the accessor), valid until the world's `DetachWorld`.
<!-- /assumption -->

### Two-way bindings

A slider, toggle, choice or text field bound to a `self` component property with `writeBack: true` publishes the write-back type and property with its event; `UI.Events` records the property command with the event's value converted to the property's type (f64 → the numeric type with `Zenith_TryNarrowCast` and clamping; bool; sint32 selection; text interned as a string ID); a conversion failure drops the command and counts `ui.events.write_back_failed`. The property's authority is untouched: the change arrives as a command, which [Z2-N-ARCH-483] permits from any phase.

## Requests from simulation

### Command kinds

<!-- elaboration: the six request command kinds -->
Registered by the module at start step 8 through `Zenith_ModuleStartContext::RegisterCommandKind` (payload structs of `Zenith2/Docs/Design/UI/PublicAPI.md → "Zenith_UIComponents header"`; every kind recordable in phases 2–10):

| Command kind | Payload | Playback |
|---|---|---|
| `UI.Focus` | `Zenith.UI.FocusRequest` | appends `UI_REQUEST_KIND_FOCUS` to `Zenith.UI.Requests` |
| `UI.ScrollTo` | `Zenith.UI.ScrollRequest` | appends `UI_REQUEST_KIND_SCROLL` |
| `UI.PlayAnimation` | `Zenith.UI.AnimationRequest` | appends `UI_REQUEST_KIND_ANIMATION` |
| `UI.Announce` | `Zenith.UI.AnnounceRequest` | appends `UI_REQUEST_KIND_ANNOUNCE` |
| `UI.ShowCaption` | `Zenith.UI.CaptionCue` | inserts the cue into `Zenith.UI.Captions` with `m_ulStartTick` = the playback tick |
| `UI.ClearCaptions` | `uint8` kind mask (struct `Zenith.UI.ClearCaptionsRequest`) | removes matching cues |

`Zenith.UI.Requests` is a bounded queue of 256 requests per world (a full queue drops the newest and counts `ui.requests.dropped`), drained by the extractor as its sole consumer each frame; a request naming a destroyed screen or an unknown element is counted and ignored. Requests carry presentation intent only: their effects (focus, scroll, animation, speech) never feed back into simulation except as subsequent player events.

### Script bindings

The Luau surface is the table of `Zenith2/Docs/Design/UI/ServicesAPI.md → "Zenith_UIScriptApi header"`; each buffered function appends its command kind through the invocation's side-effect buffer with `uSIDE_EFFECT_MASK_FIELD_COMMAND` (`Zenith2/Docs/Scripting/Bindings.md → "Binding registration and the thunk ABI"`), so a failed invocation's UI requests are discarded with its other side effects. Screens themselves are opened, closed, layered and shown by ordinary component writes and spawns of entities carrying `Zenith.UI.Screen` (from prefabs or scenes), which are transactional and recorded like every other simulation state.
