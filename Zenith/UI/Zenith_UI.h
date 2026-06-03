#pragma once

/**
 * Zenith_UI.h - Convenience header for the UI system (Zenith_UI namespace).
 *
 * Pulls in all UI element classes:
 *   - Zenith_UICanvas: root container that owns all elements
 *   - Zenith_UIElement: base class for all elements
 *   - Zenith_UIText / Zenith_UIRect / Zenith_UIImage / Zenith_UIButton: widgets
 *
 * Coordinate system: origin (0,0) at top-left, +X right, +Y down, units = pixels.
 * Anchor/pivot are normalized 0-1. See UI/CLAUDE.md for a worked example.
 */

#include "UI/Zenith_UICanvas.h"
#include "UI/Zenith_UIText.h"
#include "UI/Zenith_UIRect.h"
#include "UI/Zenith_UIImage.h"
#include "UI/Zenith_UIButton.h"
#include "UI/Zenith_UILayoutGroup.h"
#include "UI/Zenith_UIToggle.h"
#include "UI/Zenith_UIOverlay.h"
#include "UI/Zenith_UIScrollView.h"
