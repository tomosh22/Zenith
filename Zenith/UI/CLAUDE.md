# UI System

## Overview

Screen-space UI framework with a canvas-based element hierarchy. Renders via `Flux_Quads` (images/rects) and `Flux_Text` (text). All classes are in the `Zenith_UI` namespace.

## Coordinate System
- Origin (0,0) is **top-left** of screen
- X increases rightward, Y increases **downward**
- Units are in **pixels**

## Files

- `Zenith_UI.h` - Convenience header with usage examples
- `Zenith_UICanvas.h/cpp` - Root container, manages hierarchy and rendering
- `Zenith_UIElement.h/cpp` - Base class for all UI elements
- `Zenith_UIRect.h/cpp` - Colored rectangle (fill, border, glow)
- `Zenith_UIText.h/cpp` - Text rendering (alignment, font size)
- `Zenith_UIImage.h/cpp` - Texture/sprite rendering (UV mapping, sprite sheets)
- `Zenith_UIButton.h/cpp` - Interactive button (state machine, function pointer callback)

## Architecture

### Canvas (`Zenith_UICanvas`)
Root container that **owns all elements** (deletes them on removal/destruction).

- Elements added via `AddElement(new Zenith_UIRect(...))` - canvas takes ownership
- `FindElement(strName)` searches entire hierarchy recursively
- `SetReferenceResolution()` enables resolution-independent scaling
- Move semantics supported for ECS component pool compatibility
- Static `GetPrimaryCanvas()` / `SetPrimaryCanvas()` for global access

**Rendering:** Canvas collects quad and text submissions from elements during `Render()`, then batches them to `Flux_Quads` and `Flux_Text`.

### Element Base (`Zenith_UIElement`)
All elements have:
- **Transform:** Position, Size, Anchor (normalized 0-1), Pivot (normalized 0-1)
- **Appearance:** Color (RGBA Vector4), Visibility flag
- **Hierarchy:** Parent/children via raw pointers (canvas owns all)
- **Dirty flag:** `m_bTransformDirty` for lazy bounds recalculation

**Anchor/Pivot system:** Anchor defines which point on the parent the element positions relative to. Pivot defines which point on the element is placed at the anchor. `AnchorPreset` enum provides common positions (TopLeft, Center, BottomRight, etc).

### Element Types

| Type | Key Features |
|------|-------------|
| `Zenith_UIRect` | Fill amount (0-1) with 4 fill directions, border (color + thickness), glow |
| `Zenith_UIText` | Font size, horizontal alignment (Left/Center/Right), vertical alignment (Top/Middle/Bottom) |
| `Zenith_UIImage` | Texture loading via path, UV mapping, sprite sheet support (`SetSpriteSheetFrame`) |
| `Zenith_UIButton` | Normal/Hovered/Pressed state machine, per-state colors, function pointer callback (`UIButtonCallback`), keyboard focus |

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
