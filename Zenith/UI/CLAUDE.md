# UI System

## Overview

Screen-space UI framework with a canvas-based element hierarchy. Renders via `Flux_QuadsImpl` (quads with UV mapping, corner radius, and gradients) and `Flux_TextQueue` (text batching system). All classes are in the `Zenith_UI` namespace.

## Coordinate System
- Origin (0,0) is **top-left** of screen
- X increases rightward, Y increases **downward**
- Units are in **pixels**

## Files

- `Zenith_UI.h` - Convenience header (includes all UI element classes)
- `Zenith_UICanvas.h/cpp` - Root container, manages hierarchy and rendering
- `Zenith_UIElement.h/cpp` - Base class for all UI elements
- `Zenith_UIRect.h/cpp` - Colored rectangle (fill, border, shadow, rounded corners, gradient)
- `Zenith_UIText.h/cpp` - Text rendering (alignment, font size)
- `Zenith_UIImage.h/cpp` - Texture/sprite rendering (UV mapping, sprite sheets)
- `Zenith_UIButton.h/cpp` - Interactive button (state machine, function pointer callback)
- `Zenith_UIToggle.h/cpp` - Toggle widget with on/off states and callbacks (`UIElementType::Toggle`)
- `Zenith_UIOverlay.h/cpp` - Modal overlay widget with dim background and fade animations (`UIElementType::Overlay`)
- `Zenith_UIScrollView.h/cpp` - Scrollable content container with clip-rect support (`UIElementType::ScrollView`)
- `Zenith_UILayoutGroup.h/cpp` - Layout container with horizontal/vertical arrangement, padding, spacing, child alignment, fit-to-content (`UIElementType::LayoutGroup`)
- `Zenith_UIGridLayoutGroup.h/cpp` - Grid layout container: fixed column count + fixed cell size, row-major placement of visible children, spacing, padding, fit-to-content auto-size (`UIElementType::GridLayoutGroup`). The grid analogue of `Zenith_UILayoutGroup` (bag/box/dex/party grids).
- `Zenith_UIVirtualStick.h/cpp` - B9 on-screen thumbstick: FIXED/FLOATING base, radius + deadzone fraction, activation rect, publishes an AXIS2D through a VIRTUAL binding source (`UIElementType::VirtualStick`)
- `Zenith_UIVirtualButton.h/cpp` - B9 on-screen action button: hit slop, publishes HELD through a VIRTUAL binding source (`UIElementType::VirtualButton`)
- `Zenith_UIVirtualControls.Tests.inl` - unit tests for both on-screen controls (hosted at the bottom of `Zenith_UIVirtualStick.cpp`)
- `Zenith_UIStyle.h` - `UIStyle` struct: fill, gradient, border, corner radius, and shadow properties used throughout the UI system
- `Zenith_UIStyleRenderer.h/cpp` - `RenderStyledRect()` static method rendering styled rects (shadow, border, fill, gradient); used by rect, button, overlay, scrollview, and toggle elements
- `Zenith_UITween.h` - `TweenEasing`/`TweenProperty` enums and `Zenith_UITween` struct for element animation

Text-bearing elements (button, text, toggle, overlay, layout group) also include `Flux/Text/Flux_TextImpl.h` for the text-render path.

## Architecture

### Canvas (`Zenith_UICanvas`)
Root container that **owns all elements** (deletes them on removal/destruction).

- Elements added via `AddElement(new Zenith_UIRect(...))` - canvas takes ownership
- `FindElement(strName)` searches entire hierarchy recursively
- `SetReferenceResolution()` enables resolution-independent scaling
- Move semantics supported for ECS component pool compatibility
- Static `GetPrimaryCanvas()` / `SetPrimaryCanvas()` for global access
- **Focus navigation:** `SetFocusedElement()` plus `NavigateUp/Down/Left/Right()` and `ActivateFocused()` drive keyboard/gamepad focus traversal across focusable elements
- **Clip-rect stack:** `PushClipRect()` / `PopClipRect()` / `HasActiveClipRect()` manage a general scissor stack (used by ScrollView, but available to any element)

**Rendering:** Canvas collects quad and text submissions from elements during `Render()`, then batches them to `Flux_QuadsImpl` and `Flux_TextQueue`.

### Element Base (`Zenith_UIElement`)
All elements have:
- **Transform:** Position, Size, Anchor (normalized 0-1), Pivot (normalized 0-1)
- **Appearance:** Color (RGBA Vector4), Visibility flag
- **Hierarchy:** Parent/children via raw pointers (canvas owns all)
- **Dirty flag:** `m_bTransformDirty` for lazy bounds recalculation
- **Group inheritance:** `SetGroupAlpha()` and `SetGroupInteractable()` set local values; descendants determine effectiveness by walking UP the parent chain via `GetEffectiveAlpha()` (multiplies ancestor group alphas) and `IsGroupInteractable()` (checks ancestor interactability), so a parent can fade or disable all descendants by setting its own group values

**Anchor/Pivot system:** Anchor defines which point on the parent the element positions relative to. Pivot defines which point on the element is placed at the anchor. `AnchorPreset` enum provides common positions (TopLeft, Center, BottomRight, etc).

### Element Types

| Type | Key Features |
|------|-------------|
| `Zenith_UIRect` | Fill amount (0-1) with 4 fill directions (`FillDirection`: `LeftToRight`/`RightToLeft`/`BottomToTop`/`TopToBottom`), border (color + thickness), shadow |
| `Zenith_UIText` | Font size, horizontal alignment (Left/Center/Right), vertical alignment (Top/Middle/Bottom) |
| `Zenith_UIImage` | Texture loading via path, UV mapping, sprite sheet support (`SetSpriteSheetFrame`) |
| `Zenith_UIButton` | `ButtonState` machine (`NORMAL`/`HOVERED`/`PRESSED`), per-state colors, function pointer callback (`UIButtonCallback`), keyboard focus, optional icon (`SetIconTexturePath`/`SetIconSize`/`SetIconPlacement` with `IconPlacement` enum/`SetIconPadding`) |
| `Zenith_UIToggle` | On/off boolean state with per-state styling and `UIToggleCallback`, centered text label |
| `Zenith_UIOverlay` | Modal full-screen dim background + centered content container, tween-driven fade in/out, blocks input behind it |
| `Zenith_UIScrollView` | Viewport that clips children, scrollable via `ScrollDirection` (`VERTICAL`/`HORIZONTAL`/`BOTH`) using the canvas clip-rect stack |
| `Zenith_UILayoutGroup` | `LayoutDirection` (`Horizontal`/`Vertical`) child arrangement with padding, spacing, `ChildAlignment` (9 values, `UpperLeft`..`LowerRight`), fit-to-content |
| `Zenith_UIGridLayoutGroup` | Fixed-column, fixed-cell-size grid: row-major placement of visible children, horizontal/vertical spacing, padding, fit-to-content auto-size |
| `Zenith_UIVirtualStick` | On-screen thumbstick: `StickMode` (`FIXED`/`FLOATING`), radius + deadzone fraction, activation slop, publishes AXIS2D (+y FORWARD) |
| `Zenith_UIVirtualButton` | On-screen action button: hit slop, publishes HELD; no callback and no focus (that is `Zenith_UIButton`'s job) |

### The UI INPUT phase (frame contract step 10)

Input and rendering are **two separate passes over the canvas**, and the split is
load-bearing: `Update()` / `Render()` are the VISUAL path and a frame is free to
skip them (a scene transition submits no render work), so **nothing there may
change input state**. Everything interactive happens in the input phase, which
`Zenith_UISystem::UpdateInput(Zenith_InputActions&, Zenith_Pointers&, float)`
drives from `Zenith_Core::Zenith_MainLoop` BEFORE game logic — so a widget
consumes a press the same frame gameplay would otherwise have seen it. Both
layers are passed IN by reference; no widget reaches `g_xEngine`.

| Sub-step | Who | What |
|---|---|---|
| **10a** | `Canvas::UpdateSize()` | Layout refresh FIRST. Bounds recompute lazily off the dirty flag, so a resize or rotation this frame is reflected before the first hit test — never hit-tested against last frame's bounds. Run at the head of BOTH walks below. |
| **10b** | `Actions().FinalizeReservedUI()` then `Canvas::UpdateFocusNavigation()` | The engine-reserved UI actions (ids 0-15) close FIRST, then every visible canvas consumes them. Before the capture walk, so a widget claim can never suppress UI navigation. |
| **10c** | `Canvas::UpdatePointerInput()` | The standard-control capture walk. Claims are taken here. |
| **10d** | `Canvas::UpdateVirtualControls()` | The B9 virtual producers claim and publish. |
| **10e** | `Actions().FinalizeGameplay()` | Everything else closes, with claim suppression applied against the final claim state. |

The whole phase is wrapped in a `Zenith_SceneUpdateDeferralGuard`: a click
callback that queues a `LoadScene` must not tear the scene down underneath the
walk. It brackets BOTH walks, because a focus-navigation confirm fires exactly
the same callbacks a click does.

#### Focus navigation (10b) — reserved actions only

`UpdateFocusNavigation(const Zenith_InputActions&)` reads **only** the five
reserved actions: `UI_NAV_UP/DOWN/LEFT/RIGHT` → `NavigateUp/Down/Left/Right()`,
`UI_CONFIRM` → `ActivateFocused()`. A canvas no longer reads a raw key or pad
button anywhere, and the four directions are an `else if` chain because each
reserved action ALREADY merges its arrow key with its d-pad direction — the two
parallel chains this replaced could both fire in one frame and navigate twice.

The canvas is the **SOLE** owner of keyboard/gamepad activation; the button's own
Enter/Space path is gone, so there is exactly one route from a confirm to a
widget and it goes through the focused element. `NotifyPointerActivate` sets
focus BEFORE raising the activate edge, so a consumer dispatching by focused-element
name sees a consistent pair within the frame.

Because the input reads are non-consuming, every visible canvas sees the same
confirm — mutual exclusion between canvases is the game's business, not the
engine's.

#### The capture walk (10c) and the claim rules

* **Order is authored pre-order** — roots in order, parent before children,
  visible-only — the exact order the update pass walks. That is what makes
  "first claim wins" a deterministic answer rather than a race.
* **Claims live in `Zenith_Pointers`** (`ClaimPointer` / `ReleaseClaim`, owner
  token = the widget's own address). FIRST claim wins; a claim survives the
  terminal UP/CANCEL frame so its owner can see the edge.
* **A claim is what suppresses gameplay.** At 10e the action layer drops any
  pointer-sourced transition (`TOUCH_PRIMARY`, and `MOUSE_BUTTON` fed by a
  pointer — the B3 projection and a real Windows mouse alike) whose pointer is
  claimed, press and matching release together. Keyboard and gamepad rows are
  never claim-filtered.
* **Drag-off keeps the claim** and clears `m_bPointerInside`: the button
  un-presses visually and a release outside does not click, but the gesture
  stays consumed so nothing behind it can pick it up mid-drag.
* **Mutation during the walk is deferred.** The canvas is in an input pass
  (`IsInputPassActive()`): element creation is queued and deletion deferred, so
  a click callback that builds or destroys UI cannot invalidate the snapshot the
  walk is iterating. An element already marked `IsPendingDestroy()` is SKIPPED —
  logically gone means it must not still take clicks.
* Tools-only: a button ignores clicks while the editor is Stopped and no
  simulator is driving (`IsInputSuppressedByEditor`) — the canvas is being
  AUTHORED, not played.

### On-screen controls (B9) — the virtual producers

`Zenith_UIVirtualStick` / `Zenith_UIVirtualButton` are GAMEPLAY controls, not menu
widgets. They publish into the **action layer** rather than firing callbacks, so a
game reads `"Move"` / `"Jump"` and never learns which device answered.

- **Input lives in frame step 10d only.** `UpdateVirtualInput(Zenith_InputActions&,
  Zenith_Pointers&, float)` runs from `Zenith_UICanvas::UpdateVirtualControls`,
  after the 10c capture walk (so a standard widget wins a contested pointer) and
  before `FinalizeGameplay` (so the virtual transitions replay in the same frame).
  `Render()` only draws what 10d decided — a skipped render frame cannot change
  input state.
- **The 10d walk is visibility-BLIND** (10c is not). A control hidden or masked out
  MID-GESTURE still owes its action a release transition and its axis a zero, and an
  element the walk stopped visiting could not publish either.
- **A control targets an ACTION NAME**; the named action's own `INPUT_BINDING_VIRTUAL`
  row supplies the source id it publishes to, so a rebind is a binding-table edit.
- **`SetAction()` mid-gesture is the retarget case:** the OLD action gets a release,
  the pointer claim is **KEPT** (the gesture stays consumed), and the control is
  DISARMED for the new action until a fresh DOWN. Hiding and losing TOUCH from the
  active profile mask do the same.
- **Visible iff the ACTIVE profile mask carries TOUCH.** The visual pass has no action
  layer, so 10d latches the answer for `Render()`.
- **All geometry is density-scaled** through `Zenith_Pointers::GetDisplayScale()`, via
  `Zenith_UIElement::ResolveTouchTargetRect` (slop + the 57-logical-px minimum touch
  target, grown about the rect's centre). Authored values are LOGICAL pixels.
- Authoring: `Zenith_UIComponent::CreateVirtualStick/CreateVirtualButton`, the UI
  component panel's `+ V.Stick` / `+ V.Button` buttons, and the
  `AddStep_CreateUIVirtual*` / `AddStep_SetUIVirtual*` editor-automation verbs.

### Styling & Animation
- **`UIStyle`** (`Zenith_UIStyle.h`) bundles fill color, optional bottom gradient color, border (color + thickness), corner radius, and shadow (color/offset/spread). `UIStyle::Lerp` blends two styles for tween targets.
- **`Zenith_UIStyleRenderer::RenderStyledRect()`** is the shared draw path that emits shadow, border, fill, and gradient quads; rect, button, overlay, scrollview, and toggle elements render their styled content through it.
- **`Zenith_UITween`** (`Zenith_UITween.h`) animates element properties via `TweenProperty` with a `TweenEasing` curve (drives overlay fades, etc).

### Type System
- `UIElementType` enum for serialization dispatch
- `CreateFromType()` factory method creates elements by type
- Full DataStream serialization with type preservation

## Key Patterns

- **Ownership:** Canvas owns ALL elements - always allocate with `new`, never delete manually
- **Non-copyable:** Elements and canvas are non-copyable (hierarchy uses raw pointers)
- **Button callbacks:** Uses function pointer `UIButtonCallback`, NOT `std::function`
- **Tools integration:** `RenderPropertiesPanel()` wrapped in `#ifdef ZENITH_TOOLS`
- **Serialization:** `WriteToDataStream`/`ReadFromDataStream` on all elements, factory-based deserialization
