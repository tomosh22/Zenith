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
