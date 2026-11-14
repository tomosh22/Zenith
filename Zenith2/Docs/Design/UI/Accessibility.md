# UI design: accessibility, captions and scaling

This file is part of the UI design family; the entry document is `Zenith2/Docs/Design/UI.md`. It specifies the accessibility settings, the semantic tree UI builds for screen readers and narration, narration composition and live regions, the platform bridge UI expects from PLAT, caption cues and their presentation, reduced motion, colour-vision modes and contrast, UI and text scaling, accessibility validation, and the four end-to-end accessible flows of the inventory (Z2-I-UI-27 to Z2-I-UI-30). Every setting here is presentation configuration: none changes simulation, so replays and determinism are unaffected by any combination of settings.

## Scope of this file

| Topic | Section |
|---|---|
| settings, their keys and persistence | "Accessibility settings" |
| what assistive technology sees | "Semantic tree" |
| speech | "Narration" |
| captions | "Captions and subtitles" |
| motion, colour, size | "Reduced motion", "Colour-vision modes and contrast", "UI scaling and text scaling" |
| proving it | "Accessibility validation", "End-to-end accessible flows" |

## Accessibility settings

The settings are configuration keys of the presentation kind registered by `Zenith_RegisterUIConfigKeys`, read into `Zenith_UIAccessibilitySettings` (`Zenith2/Docs/Design/UI/ServicesAPI.md → "Zenith_UIAccessibility header"`), persisted in the user configuration layer and applied at the next frame's prologue (`Zenith_UISystem::ApplyConfig`):

| Key | Type, range, default | Field | Effect |
|---|---|---|---|
| `ui.scale` | float 0.75–2.0, 1.0 | `m_fUIScale` | canvas scale ("UI scaling and text scaling") |
| `ui.access.textScale` | float 1.0–2.0, 1.0 | `m_fTextScale` | font sizes only |
| `ui.access.reducedMotion` | bool, the OS preference | `m_bReducedMotion` | "Reduced motion" |
| `ui.access.highContrast` | bool, the OS preference | `m_bHighContrast` | selects the `highContrast` palette |
| `ui.access.colourVision` | enum `none`, `protanopia`, `deuteranopia`, `tritanopia`, `achromatopsia`; `none` | `m_eColourVision` | "Colour-vision modes and contrast" |
| `ui.access.narration` | bool, false | `m_bNarration` | built-in text-to-speech narration |
| `ui.access.screenReaderBridge` | bool, true | `m_bScreenReaderBridge` | publish the semantic tree to an active OS screen reader |
| `ui.access.narrationRate` | float 0.5–2.0, 1.0 | `m_fNarrationRate` | speech rate multiplier for built-in narration |
| `ui.access.focusRingScale` | float 1.0–3.0, 1.0 | `m_fFocusRingScale` | focus ring width multiplier |
| `ui.captions.subtitles` | bool, true | `m_bSubtitles` | dialogue subtitles |
| `ui.captions.sounds` | bool, false | `m_bSoundCaptions` | non-speech sound captions |
| `ui.captions.scale` | float 1.0–2.5, 1.0 | `m_fCaptionScale` | caption text size multiplier |
| `ui.captions.backgroundOpacity` | float 0–1, 0.75 | `m_fCaptionBackgroundOpacity` | caption box opacity |
| `ui.captions.speakerNames` | bool, true | `m_bSpeakerNames` | prefix a speaker's name |
| `ui.captions.speakerColours` | bool, true | `m_bSpeakerColours` | colour the speaker's name |

- Initial values: `ui.access.reducedMotion`, `ui.access.highContrast` and `ui.access.textScale` take their OS default from PLAT's keys `platform.accessibility.reducedMotion`, `platform.accessibility.highContrast` and `platform.accessibility.textScale`, which PLAT fills from the OS into the configuration store's platform layer at startup and re-applies at prologue step 3 when the OS changes (`Zenith2/Docs/Design/PLAT/Accessibility.md → "Preference keys and the platform layer"`). PLAT cannot write UI's keys, and UI may read PLAT's because ZenithUI declares ZenithPlatform ([Z2-N-ARCH-930]): `Zenith_ReadUIConfig` takes each of the three values from the corresponding `platform.accessibility.*` key while its own `ui.access.*` key's winning layer is the engine default (`Zenith_Config::GetProvenance(key).m_eLayer == CONFIG_LAYER_ENGINE_DEFAULT`), and from its own key otherwise, so any value a game, a platform file, the user or the command line set wins and an untouched setting follows the OS (the rule of PLAT's cross-area row P21). An OS change dispatches the change registry for PLAT's key, which UI's `ApplyConfig` sees at the next prologue; UI calls no OS API.
- The engine ships an accessibility settings screen `content:/Engine/UI/AccessibilitySettings.ui.json`, built only from bound controls (`config` sources, Interaction.md "Activation and value controls"), which a game instances from its options menu; its strings are the engine namespace `engine.access.*`. A game may author its own instead; validator rule 46 still forbids binding a simulation-affecting key.
- Presentation systems outside UI (camera shake, screen flashes, VFX intensity) resolve reduced motion with the same two-key rule (`ui.access.reducedMotion` unless its winning layer is the engine default, else `platform.accessibility.reducedMotion`); scripts cannot read it, because scripts read no presentation state.

## Semantic tree

<!-- elaboration: tree construction, stable node IDs and the revision rule -->
`Zenith_UIContext::BuildA11yTree` runs in the input world's extractor after layout whenever a consumer is active (PLAT's service reporting a screen reader or an OS client that requests the tree, `GetState().m_bScreenReaderActive` or `m_bTreeRequested`; built-in narration on; or a test requesting it); otherwise it is skipped and costs nothing. It walks visible screens in descending layer, so the frontmost screen's nodes come first, and emits one `Zenith_UIA11yNode` per element that has a role other than `NONE` or that has a live region; elements with role `NONE` are transparent (their children attach to the nearest emitted ancestor). Hidden subtrees (an element whose member `visible` is false or that is inert, a screen below a blocking screen, `accessibility.hidden: true`) are omitted. Node IDs are stable across frames: `Hash64(context tag, screen owner stable ID, element uid)`, with the item index mixed in for list items. The tree's revision (`Zenith_UIA11yTree::m_ulRevision`, the context's own) increments only when some node's role, state, name, value, bounds (by more than 1 pixel) or structure changed, so the bridge publishes only on change; what the bridge passes to PLAT is its own module-wide revision ("Platform accessibility bridge"), never a context's.

### Roles and states

| Element type | Role | States and actions |
|---|---|---|
| `panel` | `GROUP` when it has `accessibility.label`, else `NONE` | — |
| `text` | `TEXT`; `HEADING` when `accessibility.heading` (level 1–6) | `LIVE_*` from `accessibility.live` |
| `image` | `IMAGE` when it has `accessibility.label`; else `NONE` (decorative) | — |
| `button` | `BUTTON` | `FOCUSABLE`, `DISABLED`; activate |
| `toggle` | `CHECKBOX`; in a group, `TAB` when the group has `accessibility.role: "tabs"` | `CHECKABLE`, `CHECKED`; activate |
| `slider` | `SLIDER` | range min, max, value; increment, decrement |
| `choice` | `COMBO_BOX` | value text; increment, decrement |
| `textField` | `TEXT_FIELD` | `READ_ONLY`, `PASSWORD` (value masked as bullets) |
| `list` | `LIST`, set size = item count including virtualized items | scroll into view |
| list item | `LIST_ITEM` | position in set, `SELECTED` |
| `scroll` | `SCROLL_AREA` | scroll into view |
| `progress` | `PROGRESS` | range, value text as percent |
| `rebind` | `BUTTON`, value = the current binding's label | activate |
| `inputPrompt` | `TEXT`, name = the binding display label | — |
| `onScreenControl` | `NONE` (the OS reader is not offered virtual sticks) | — |
| screen root with `accessibility.role: "dialog"` | `DIALOG` | — |
| any with `accessibility.role` `alert` or `status` | `ALERT`, `STATUS` (implicitly live assertive, polite) | — |

`FOCUSED` is set on the focused element; actions requested by the OS (activate, increment, decrement, set focus, scroll into view) arrive through the bridge as input on the next Pump and take the same paths as navigation input, so an assistive-technology activation publishes the same event a press would.

### Names and descriptions

The accessible name is the first non-empty of:
1. `accessibility.label` (a text value: a localization key with its arguments);
2. `accessibility.labelledBy`: the formatted text of the named element;
3. the element's own text content (for `button`, `toggle`, `choice`: the concatenated text of its `text` descendants in document order);
4. for `inputPrompt`, the binding's display label.

The description is `accessibility.hint` (a localization key). The value is role-specific: a slider's formatted value through its `valueFormat` message (default `{value, number}`), a choice's current option text, a progress bar's percent, a text field's content. An interactive element with no name after derivation is validator rule 62's warning and is announced by its role alone.

## Narration

### Narration composition

When focus moves, the utterance for the new element is composed from localized engine strings, so it reads naturally in every language:

`engine.accessibility.utterance` = `{name}{, role}{, state}{, value}{, position}` followed after a 1-second pause by `{hint}`, where:
- `role` is `engine.accessibility.role.<role>` ("button", "checkbox", "slider"…) and is omitted for `TEXT`;
- `state` is `engine.accessibility.state.checked` / `unchecked` / `disabled` / `selected`;
- `position` is `engine.accessibility.position` = "{index} of {count}" for list items and grouped toggles;
- when focus enters a new `DIALOG` or `GROUP` with a label, the group's name is spoken first.

A focus utterance interrupts the previous focus utterance; announcements queue behind it. Narration requests are produced by the extractor (`Zenith_UINarrationRequest`) and delivered on the main thread at the next Pump to the platform service, which owns speech. Because the extractor's frame arena is gone by the next Pump, the extractor copies each request, with its composed text, into the context's narration ring (`uUI_NARRATION_RING_CAPACITY` = 32 requests, each text at most 512 bytes, cut at a grapheme boundary; context-owned memory allocated at context creation); Pump drains the ring in order. A full ring drops the oldest announcement (never a focus utterance, which replaces the previous focus utterance in place) and counts `ui.a11y.narration_dropped` (`Test_UINarration_RingSurvivesArenaReset`).

### Live regions

- A `text` element with `accessibility.live: "polite"` speaks its new formatted text when it changes, debounced 500 ms (a ticking counter speaks its settled value), queued behind current speech; `"assertive"` interrupts.
- `UI.Announce(key, priority)` requests from simulation (Interaction.md "Requests from simulation") speak their formatted message with the given priority, without changing focus; they are the script-visible way to narrate game events ("Quest updated").
- Live regions and announcements are suppressed while a screen reader's own speech of a focus change is pending (PLAT's service reports it through `IsFocusSpeechPending()`), so the two never talk over each other.

### Platform accessibility bridge

<!-- elaboration: the division of the bridge between UI and PLAT, in the layering-correct form (UI implements a PLAT-declared provider) -->
UI consumes, and does not implement, the platform accessibility service. The OS integration is PLAT's (`Zenith2/Docs/Design/PLAT/Accessibility.md → "Ownership split with UI"`): `Zenith_AccessibilityService` is a PLAT-declared class reached through `Zenith_Platform::GetAccessibility()` (null when the backend resolves to none), its value types (`Zenith_AccessibilityNode`, `Zenith_AccessibilityTreeView`, `Zenith_AccessibilityAction`, `Zenith_AccessibilityState`) are PLAT's (`Zenith2/Docs/Design/PLAT/Accessibility.md → "Accessibility value types"`, "Zenith_AccessibilityService"), and UI implements the PLAT-declared interface `Zenith_AccessibilityTreeProvider` (`Zenith2/Docs/Design/PLAT/Accessibility.md → "Zenith_AccessibilityTreeProvider"`), so the dependency points down (UI → PLAT) and PLAT never sees a ZenithUI type. What the platform implementations do:

| Need | Windows | Android |
|---|---|---|
| screen reader active | UI Automation client presence (`UiaClientsAreListening`) | `AccessibilityManager.isTouchExplorationEnabled` |
| expose the semantic tree | a UIA fragment root provider on the game window, built from PLAT's snapshot of the provider's tree | `AccessibilityNodeProvider` on the game view (the Java side in MOD's Gradle template) |
| announce text | `UiaRaiseNotificationEvent` | `View.announceForAccessibility` |
| built-in text to speech | SAPI `ISpVoice` (asynchronous, purge on interrupt) | `TextToSpeech` |
| OS preferences | `SPI_GETCLIENTAREAANIMATION`, high-contrast scheme, text scale | `ANIMATOR_DURATION_SCALE` = 0, contrast, font scale |
| OS-requested actions | UIA `Invoke`, `RangeValue`, `SetFocus`, `ScrollIntoView` | `performAction` (click, scroll forward and backward, accessibility focus) |

What UI does:

- `Zenith_UIAccessibilityBridge` (`Internal/Accessibility/Zenith_UIA11yBridge.cpp`) implements `Zenith_AccessibilityTreeProvider`; at module start step 10, when `GetPlatform()->GetAccessibility()` is non-null, the module calls `SetTreeProvider(&bridge)`, and before the module instance is destroyed it calls `SetTreeProvider(nullptr)`, as the interface's contract requires. A refused registration (`ACCESSIBILITY_ERROR_PROVIDER_ALREADY_SET`) fails start step 10, which unwinds.
- The extractor converts the context's semantic tree into PLAT's node type and UTF-8 pool in a context-owned double buffer (`Zenith_UIA11yPublishBuffer`: two node arrays and two string pools, created at 256 nodes and 16 KiB and grown on the main thread only, doubling, up to PLAT's caps of 4,096 nodes and 256 KiB), filling the back buffer. When the converted tree needs more nodes or pool bytes than the back buffer holds, the extractor stages nothing that frame (the back buffer is marked not staged, with the capacity it needs), counts `ui.a11y.publish_deferred`, and the next Pump grows the back buffer before the next extraction fills it; a frame that staged nothing publishes nothing, and the previously published tree stays with PLAT.
- The bridge owns one module-wide revision, `uint64 m_ulPublishedRevision`, starting at 0, because PLAT's revision rules run per provider registration, not per context (`Zenith2/Docs/Design/PLAT/Accessibility.md → "Publication algorithm"`: a revision equal to the published one does nothing and a lower one is `ACCESSIBILITY_ERROR_STALE_REVISION`), and the input world, whose context's tree is the one published, changes on a world transition, on editor play and on detach, when a new context's own revision starts low. At each Pump the claimant callback swaps the input world's front and back buffers and, when a consumer is active and either that context's tree revision changed since the bridge last published it or the published context changed (another input world, or none), increments `m_ulPublishedRevision` and calls `NotifyTreeChanged(m_ulPublishedRevision)`. Inside that call, on the main thread, PLAT calls the bridge's `GetTree()`, which returns the input world's front buffer as a view whose `m_ulRevision` is `m_ulPublishedRevision` (never the context's), and PLAT copies it into its own snapshot before returning; OS queries, on UI Automation's threads or the Android UI thread, read PLAT's snapshot under PLAT's lock and never UI's memory (`Zenith2/Docs/Design/PLAT/Accessibility.md → "The published snapshot"`). The front buffer therefore has to stay valid only for that call, and `GetTree()` calls no service member. When the input world changes, the next Pump publishes the new input world's tree at the next revision; when it detaches with no input world left, the next Pump publishes an empty view at the next revision. Before any world attaches, `GetTree()` returns an empty view with revision 0, which is what start step 10's `SetTreeProvider` publishes (PLAT publishes an empty tree for revision 0 and resets its published revision at each registration, so the bridge's first changed tree, at revision 1, is accepted). `Test_UIA11yBridge_RepublishesOnInputWorldChange` checks a world swap and a detach.
- The conversion meets PLAT's tree validation rules V1 to V9 (`Zenith2/Docs/Design/PLAT/Accessibility.md → "Tree validation rules"`) by construction: roles, state bits and the five action bits map one to one (PLAT lists its enumerators in the order of `Zenith_UIRole` and `Zenith_UIA11yStateFlags`); the cap keeps the first `uACCESSIBILITY_MAX_NODES` (4,096) nodes of the tree's pre-order sequence, frontmost screen first, so when a context has more, the nodes of the screens furthest back are omitted and counted `ui.a11y.nodes_capped`, and because the kept nodes are a pre-order prefix every kept node's parent is kept at a lower index (V1, V3); every name, value and description has each U+0000 replaced by U+FFFD during conversion (`Zenith_UIRepairUtf8` keeps U+0000, which V5 rejects) and is then at most `uACCESSIBILITY_MAX_TEXT_BYTES` (512) bytes, cut at a grapheme boundary, and the pool at most 256 KiB, a text that would overflow it being published empty and counted the same way (V5); only the focused node carries the focused state and it is the view's `m_uFocused`, and when the focused element's node is omitted (beyond the cap, or hidden) `m_uFocused` is `uACCESSIBILITY_NO_NODE` and no node carries the focused state (V8); bounds are rounded to whole main-window pixels (`Zenith_AccessibilityRect`, V6); range values finite and clamped into their range (V7); a `HEADING` node's `Zenith_UIA11yNode::m_uHeadingLevel` (1 to 6, from `accessibility.heading`) is copied to PLAT's `m_uHeadingLevel`, 0 for every other role, and positions never exceed set sizes (V9). A rejection that `NotifyTreeChanged` returns anyway is counted `ui.a11y.publish_rejected`, logged once per session under `ui.a11y`, and the next changed revision is published normally.
- OS-requested actions drained from the service in the claimant callback (`DrainActions`) become `Zenith_UINavInput` and pointer-free activations (activate, increment, decrement, focus, scroll into view) and take the paths of navigation input; an action naming a node ID the current tree lacks is ignored. UI never advertises `ACCESSIBILITY_ACTION_SET_VALUE` (action bit 5; UI sets only bits 0 to 4, its own `m_uActions`), which PLAT offers only when a node's action bits do, so no SET_VALUE action reaches UI.
- When an OS screen reader is active (`IsScreenReaderActive()`) and `ui.access.screenReaderBridge` is on, UI publishes the tree and sends announcements with `Announce`; built-in narration is then not spoken, so the player never hears two voices.
- `SetTreeProvider`, `NotifyTreeChanged`, `Announce` and `Speak` return `Zenith_Result<void, Zenith_AccessibilityError>`: a failed `Announce` or `Speak` (including `ACCESSIBILITY_ERROR_SPEECH_UNAVAILABLE` when no voice exists) drops that utterance, counts `ui.a11y.speech_failed` and logs the error once per session under `ui.a11y`; nothing retries it, because a late utterance would describe a focus the player has left.
- When no screen reader is active and `ui.access.narration` is on, UI speaks focus utterances and live regions through the service's built-in text to speech (`Speak` at `ui.access.narrationRate`) while `GetState()` reports speech available.
- Live regions and announcements wait while `IsFocusSpeechPending()` reports the screen reader's own focus speech (`Zenith2/Docs/Design/PLAT/Accessibility.md → "Focus speech hold"`).
- With no service (`GetAccessibility()` returns null), no tree is published and built-in narration is unavailable; the settings remain and the semantic tree is still built for tests (`Test_UIA11y_NoServiceKeepsSettings`).

Tests use the service's recording implementation, a real implementation of PLAT's service selected on every test host with `--set platform.accessibility.backend=recording` (`Zenith2/Docs/Design/PLAT/Accessibility.md → "Backend table"`) and read through PLAT's declared test-access header (`Zenith2/Docs/Design/PLAT/Accessibility.md → "Zenith_AccessibilityTestAccess API"`), not a test double of a UI or PLAT type: it records every tree publication, announcement and utterance, which a test reads with `Zenith_AccessibilityTestAccess::GetRecord` (`ACCESSIBILITY_RECORD_TREE_PUBLISHED`, `_ANNOUNCEMENT`, `_UTTERANCE` and the rest), and lets a test inject OS-requested actions with `InjectAction` (`Zenith2/Docs/Design/PLAT/Accessibility.md → "The recording implementation"`). `Test_UIA11y_NoServiceKeepsSettings` uses a platform whose accessibility backend is `none`, so `GetAccessibility()` returns null. `Test_UIA11yBridge_PublishesWithinPlatformLimits` publishes a context of 5,000 focusable elements through it and asserts that the recorded tree has 4,096 nodes with the frontmost screen's complete, that no publication was rejected, and that `ui.a11y.nodes_capped` counted the rest.

## Captions and subtitles

### Caption cues

A cue (`Zenith.UI.CaptionCue`, `Zenith2/Docs/Design/UI/PublicAPI.md → "Zenith_UIComponents header"`) carries its kind (subtitle or sound), the caption text's localization key, an optional speaker key, a speaker colour index into the theme's speaker palette, a duration in ticks, a priority, and the start tick set at playback. Cues enter `Zenith.UI.Captions` from:
1. `UI.ShowCaption` commands (scripts, dialogue systems);
2. the caption cue events of row UI4, which a C++ source without a script (cinematic playback, CINE pending) emits, read by the C++ event handler `UI.Captions@W<tag>` in `PHASE_EVENTS` (9), which receives them in the delivery of [Z2-N-ARCH-478] (events emitted in phases 2–8 are delivered in phase 9 of the same tick, those of phases 9 and 10 in the next tick's phase 9).

AUDIO draws no captions and emits no cue event: the game's script receives AUDIO's `Zenith.Audio.WhenCaption` notifications (line, event, duration, sound) and calls `UI.ShowCaption` with the line's subtitle key (AUDIO's row AU16, adopted), so dialogue captions take path 1.

The queue holds `uUI_CAPTION_QUEUE_CAPACITY` (8) cues; a new cue evicts the lowest-priority oldest cue when full; a cue expires at `m_ulStartTick + duration`; `UI.ClearCaptions` removes cues by kind mask. The queue is simulation state (it replays), so captions are identical in a replay; whether they are shown is presentation.

### Caption presentation

- Captions render in the topmost visible `captionArea` element of the context's open screens; when no open screen has one, the engine caption screen (`content:/Engine/UI/Captions.ui.json`, layer 900, `focusScope: never`, not blocking, a context-internal screen with no entity) draws them. Either way the active cues of the area's `kinds` show at the bottom centre inside the safe area, newest last, wrapped at a width of 60 em, with at most `maxLines` (3) lines in total (older cues give way first); sound captions in brackets (`engine.captions.soundFormat` = "[{text}]").
- Settings: `ui.captions.subtitles` gates subtitle cues, `ui.captions.sounds` gates sound cues; text size = the theme caption size × `ui.captions.scale` × `ui.access.textScale`; the background box opacity is `ui.captions.backgroundOpacity`; the speaker's name prefix ("NAME:") and its colour from the cue's speaker palette index follow the two speaker settings.
<!-- elaboration: the caption reading-time floor -->
- Timing: a cue shows from its start tick for its duration; the reading-time floor extends display to at least 0.3 s per word plus 1 s when the duration is shorter and the next cue does not displace it; the extension is presentation only and never delays the simulation queue.
- Captions draw above the HUD and below menus (layer 900 < menu layers of 1000 and above, a theme convention of the engine default theme).

## Reduced motion

When `ui.access.reducedMotion` is on:

| Motion | Behaviour |
|---|---|
| every style transition | duration 0: the new value applies at once |
| non-essential theme animations | not played: the element shows the final key's style |
| animations marked `"essential": true` (a loading spinner, a progress indicator) | played as an opacity cross-fade alone over the same duration (Documents.md "Animations and transitions") |
| smooth scrolling, inertia | immediate |
| caret blink, focus ring pulse | steady |
| `UI.PlayAnimation` requests | applied as their end state |
| world-space panels' billboard easing | immediate |

The validator does not require authors to mark motion: every animation is non-essential unless marked. `Test_UIAnimation_ReducedMotionSnapsTransforms` and the flow of "End-to-end accessible flows" prove it.

## Colour-vision modes and contrast

- A theme may declare palettes `protanopia`, `deuteranopia`, `tritanopia`, `achromatopsia` and `highContrast`; each colour token resolves at style resolution through the active colour-vision palette, then `highContrast` when that setting is on, then `default`, the first palette defining the token winning (Documents.md "Palettes and tokens").
<!-- elaboration: the Machado (2009) correction without a palette -->
- A colour-vision mode whose palette the theme does not declare applies an automatic correction to every resolved theme colour on the CPU: the Machado, Oliveira and Fernandes (2009) simulation matrix at severity 1.0 for the mode gives the colour as perceived, and the error between original and perceived, rotated into the channels the viewer distinguishes (the daltonization shift matrix), is added back; achromatopsia without a palette leaves colours unchanged and relies on the theme's luminance contrast. The correction never touches the game's rendered image, images, or icons; games that need world-colour help supply it through their own content (a GAME concern).
- Shape and text redundancy is an authoring rule: the engine default theme and its controls never convey state by colour alone (a checked toggle shows a check mark; an error field shows an icon and text).

Contrast: WCAG 2.2 relative luminance, ratio `(L1 + 0.05) / (L2 + 0.05)`; text below 24 px (or below 18.66 px bold) needs 4.5:1, larger text and focus rings 3:1 against the element's effective background (the nearest ancestor background colour, opaque-composited). Rule 63 evaluates every text colour in every palette, including each colour-vision palette or its automatic correction.

## UI scaling and text scaling

- `ui.scale` multiplies the canvas dp scale (Layout.md "Canvas units and scaling"); `reference` scale mode documents multiply their computed scale by it as well; the safe area is unaffected.
- `ui.access.textScale` multiplies every resolved font size and every `em` length in text-bearing elements; layout reflows (text leaves are re-measured, containers grow, lists with variable extent re-measure items); elements with `textOverflow: ellipsis` truncate instead of overflowing.
- Minimum touch target: interactive elements on a touch scheme should be at least 44 × 44 dp; validator rule 64 warns below it.
- Focus ring width scales with `ui.access.focusRingScale`.
- The layout conformance suite runs its text cases at text scale 1.0 and 2.0 and asserts no text leaf overflows its container unless the container scrolls or the text truncates (Layout.md "Layout conformance cases"; Tests.md "Layout golden suite").

## Accessibility validation

| Check | Where | Severity |
|---|---|---|
| interactive element without an accessible name (rule 62) | cook | warning |
| text contrast below threshold in any palette (rule 63) | cook | warning; error when the localization project sets `checks.contrast` to `error` |
| interactive element below 44 dp on a touch-capable target (rule 64) | cook | warning |
| every focusable element reachable by directional navigation from the initial focus | test `Test_UIA11y_AllFocusablesReachable` (one `SUBCASE` per audited document) | test failure |
| the semantic tree of every audited document has no unnamed interactive node and every node's bounds lie inside the surface | test `Test_UIA11y_TreeAudit` | test failure |
| no layout overflow at text scale 2.0 | layout suite (`Test_UILayoutGolden_TextScaleTwoNoOverflow`) | test failure |

The audited documents are every engine document under `Zenith2/Engine/ZenithUI/Internal/Content/` and every `.ui.json` under `Zenith2/Games/*/`, enumerated by the test at run time; each document is one doctest `SUBCASE` of the single test case, so a failure names the document while the test name stays one registered CTest.

## End-to-end accessible flows

The inventory's four flows (Z2-I-UI-27 to Z2-I-UI-30) are run against the Game2D and OpenWorldSlice flagship slices (Z2-D-021) once their menus, settings and first ten minutes exist. Each flow has an automated half that runs headless and a human half carried by a `needs-human` `Zenith2Acceptance` ticket at the Stage 4 acceptance (the human halves of the accessible flows; UI drafts none of those tickets, which are Roadmap's):

<!-- elaboration: the flow tests, their route bound in the gate and their full runs -->
| Flow | Automated half (`Zenith2/Tests/UIAccessibility/`, in the target `Zenith_Integration_UI`) | Human half (`needs-human`, Stage 4 acceptance) |
|---|---|---|
| screen reader (27) | `Test_UIFlow_ScreenReader_Game2D`, `Test_UIFlow_ScreenReader_OpenWorldSlice`: input playback of the slice's route through main menu, settings, new game and the first ten minutes using navigation and submit only; asserts every focus change produced an utterance with a non-empty name in the recording service's log (read with `Zenith_AccessibilityTestAccess::GetRecord`), no unnamed interactive node was focused, and the route reached its end-state event | Narrator on Windows and TalkBack on the Galaxy S24 Ultra: the same route completed by a tester using speech alone |
| remapped controls (28) | `Test_UIFlow_RemappedControls_Game2D`, `Test_UIFlow_RemappedControls_OpenWorldSlice`: rebind every action of the route through the settings screen's `rebind` controls to alternate keys, then play the route back with the remapped controls; asserts the route completes and every input prompt on the route displays the remapped binding | a tester remaps through the menu with a gamepad and completes the route |
| reduced motion (29) | `Test_UIFlow_ReducedMotion_Game2D`, `Test_UIFlow_ReducedMotion_OpenWorldSlice`: the route with `ui.access.reducedMotion` on; asserts no UI element's transform animated (every sampled transform equals its end value), only essential animations ran, as opacity fades, and the route completes | a tester confirms no motion beyond opacity on the route |
| colour-blind mode (30) | `Test_UIFlow_ColourVision_Game2D`, `Test_UIFlow_ColourVision_OpenWorldSlice`: the route once per mode (`protanopia`, `deuteranopia`, `tritanopia`, `achromatopsia`, `highContrast`), each mode one `SUBCASE`; asserts rule 63's contrast for every text drawn on the route (measured from the draw lists, not pixels) and the route completes | a tester confirms the route's information is distinguishable under each mode with the simulation preview |
| every flow over engine content | `Test_UIFlow_EngineSettingsRoute`: the four flows' assertions over a fixed route through the engine's own accessibility settings screen (`Zenith2/Tests/UIAccessibility/Routes/EngineSettings.route.json`, written by UI's flow ticket) | — |

A route is a text file `Zenith2/Tests/UIAccessibility/Routes/<Slice>.route.json`: `{ "gateTicks": 1800, "steps": [ … ] }`, steps `{ "action": "UI.Navigate", "value": [1, 0] }`, `{ "action": "UI.Submit" }`, `{ "waitForEvent": "UI.Activated", "element": "NewGame", "timeoutTicks": 600 }`, `{ "waitTicks": 60 }`. The slice routes are written by GAME's slice tickets, which own the slices' menus and first ten minutes (row UI24); UI's flow ticket writes only the engine-settings route. The driver `Zenith_UIRouteDriver` (in the suite, header-only under `Zenith2/Tests/UIAccessibility/`) feeds each step through a PLAT virtual device whose controls a test bindings layer maps to the route's actions, so the input takes the production path through Pump, claims and the mapper.

Run lengths: in the `Test` step a slice flow runs its route only up to `gateTicks` (at most 1,800 ticks, 30 simulated seconds, headless, so each slice test stays within seconds of wall time and the gate budget of Z2-D-028 holds) and asserts the flow's checks over that prefix; the full route runs only when the test executable is started with `--ui-flow-full`, which the Stage 4 acceptance run uses. A slice's flow tests land in UI's flow ticket for that slice, which is blocked by GAME's ticket that writes the slice's route, so no flow test exists before its route does and none can pass on a missing file (a missing or unparsable route fails the test); `Test_UIFlow_EngineSettingsRoute` runs in every gate from the Stage 4 accessibility ticket on.
