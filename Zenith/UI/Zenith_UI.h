#pragma once

/**
 * Zenith_UI.h - Convenience header for the UI system
 *
 * Include this file to get access to all UI classes:
 *   - Zenith_UICanvas: Root container for UI elements
 *   - Zenith_UIElement: Base class for all UI elements
 *   - Zenith_UIText: Text rendering widget
 *   - Zenith_UIRect: Colored rectangle widget (health bars, progress bars)
 *   - Zenith_UIImage: Textured image widget (icons, sprites)
 *
 * Usage Example:
 *
 *   #include "UI/Zenith_UI.h"
 *   using namespace Zenith_UI;
 *
 *   // Create elements via UIComponent
 *   auto& ui = entity.GetComponent<Zenith_UIComponent>();
 *
 *   // Health bar
 *   auto* pxHealthBar = ui.CreateRect("HealthBar");
 *   pxHealthBar->SetAnchorAndPivot(AnchorPreset::BottomLeft);
 *   pxHealthBar->SetPosition(20, -50);
 *   pxHealthBar->SetSize(200, 20);
 *   pxHealthBar->SetColor({1, 0, 0, 1});
 *   pxHealthBar->SetFillAmount(0.75f);
 *
 *   // Score text
 *   auto* pxScore = ui.CreateText("Score", "Score: 0");
 *   pxScore->SetAnchorAndPivot(AnchorPreset::TopRight);
 *   pxScore->SetPosition(-20, 20);
 *   pxScore->SetFontSize(32);
 *
 *   // Inventory icon
 *   auto* pxIcon = ui.CreateImage("Slot1");
 *   pxIcon->SetTexturePath("C:/path/to/icon.ztx");
 *   pxIcon->SetSize(64, 64);
 *
 * Coordinate System:
 *   - Origin (0,0) at top-left of screen
 *   - X increases rightward
 *   - Y increases downward
 *   - Units are pixels
 *
 * Anchoring:
 *   - Anchor: Point on parent (0-1 normalized)
 *   - Pivot: Point on this element (0-1 normalized)
 *   - Position: Offset from anchor in pixels
 *
 * NOTE: Zenith_TextComponent is deprecated. Use Zenith_UIComponent with
 *       Zenith_UIText elements instead for in-game UI text.
 */

#include "UI/Zenith_UICanvas.h"
#include "UI/Zenith_UIElement.h"
#include "UI/Zenith_UIText.h"
#include "UI/Zenith_UIRect.h"
#include "UI/Zenith_UIImage.h"
