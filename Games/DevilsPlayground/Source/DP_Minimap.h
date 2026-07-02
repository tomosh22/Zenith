#pragma once

#include "DPCommonTypes.h"
#include "DP_Fog.h"

#include <cstdint>

// ============================================================================
// DP_Minimap — pure minimap maths (2026-07-01). The DPMinimap_Component owns
// the UI wiring; everything here is stateless + unit-testable. There is no
// cross-run state bucket in this namespace by design (all minimap state lives
// on the component and dies with its scene), so no ResetForNewRun hook is
// needed in the between-tests reset.
//
// The minimap is a 2D/UI-space vector map, NOT a render-to-texture camera:
// DPProcLevel's LevelLayout provides exact top-down room geometry, and the
// fog memory table already answers per-position visibility — so the panel is
// just Zenith_UIRect draws. Convention: world +X maps to panel +X (right),
// world +Z maps to panel +Y (down), i.e. the map is a straight top-down
// plan of the level, not rotated to match the (player-rotatable) camera.
// ============================================================================
namespace DP_Minimap
{
	// Panel geometry defaults (pixels; UI origin is top-left, Y down).
	constexpr float fPANEL_SIZE_PX   = 220.0f;
	constexpr float fPANEL_MARGIN_PX = 16.0f;
	constexpr float fWORLD_PADDING_M = 4.0f;

	// Alpha floor for rooms whose memory has fully faded (VisitedHidden):
	// once seen, a room never completely vanishes from the map — that is
	// the distinction between VisitedHidden and NeverSeen (alpha 0).
	constexpr float fHIDDEN_FLOOR_ALPHA = 0.18f;

	// World -> panel mapping. Square panel covering the level bounds (plus
	// padding) uniformly — the larger world axis sets the scale so rooms
	// keep their aspect ratio.
	struct MapView
	{
		float m_fWorldMinX  = 0.0f;
		float m_fWorldMinZ  = 0.0f;
		float m_fInvExtentM = 0.0f; // 1 / covered world extent (square)
		float m_fPanelSizePx = 0.0f;
	};

	MapView BuildMapView(float fBoundsMinX, float fBoundsMinZ,
		float fBoundsMaxX, float fBoundsMaxZ,
		float fPanelSizePx = fPANEL_SIZE_PX,
		float fPaddingM = fWORLD_PADDING_M);

	// World (x, z) -> panel-local pixels, (0,0) = panel top-left.
	Vec2 WorldToPanel(const MapView& xView, float fWorldX, float fWorldZ);

	// Panel-space axis-aligned footprint of a (possibly yawed) room
	// rectangle: the AABB of the rotated rect, mapped into panel pixels.
	// (Zenith_UIRect has no rotation — drawing the yawed rect's AABB is the
	// documented v1 approximation; true rotated rects are a follow-up.)
	void RoomPanelRect(const MapView& xView, float fCentreX, float fCentreZ,
		float fHalfExtentX, float fHalfExtentZ, float fYawRadians,
		Vec2& xTopLeftOut, Vec2& xSizeOut);

	// RGBA for a room given its fog-memory state and continuous visibility
	// (DP_Fog::MemoryVisibilityForAge output, 0..255 — the SAME curve the
	// fog texture rasterizes, so minimap and world fog agree). NeverSeen ->
	// fully transparent; remembered rooms desaturate + fade toward the
	// hidden floor as the memory ages.
	Vec4 ColourForMemoryState(DP_Fog::MemoryTileState eState, uint8_t uVisibility);
}
