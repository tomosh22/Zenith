#include "Zenith.h"

#include "DP_Minimap.h"

#include <algorithm>
#include <cmath>

namespace DP_Minimap
{
	MapView BuildMapView(float fBoundsMinX, float fBoundsMinZ,
		float fBoundsMaxX, float fBoundsMaxZ,
		float fPanelSizePx, float fPaddingM)
	{
		MapView xView;
		const float fExtentX = (fBoundsMaxX - fBoundsMinX) + 2.0f * fPaddingM;
		const float fExtentZ = (fBoundsMaxZ - fBoundsMinZ) + 2.0f * fPaddingM;
		// Square coverage sized to the larger axis, centred on the bounds,
		// so rooms keep their world aspect ratio on the square panel.
		const float fExtent = std::max(std::max(fExtentX, fExtentZ), 0.001f);
		xView.m_fWorldMinX   = fBoundsMinX - fPaddingM - (fExtent - fExtentX) * 0.5f;
		xView.m_fWorldMinZ   = fBoundsMinZ - fPaddingM - (fExtent - fExtentZ) * 0.5f;
		xView.m_fInvExtentM  = 1.0f / fExtent;
		xView.m_fPanelSizePx = fPanelSizePx;
		return xView;
	}

	Vec2 WorldToPanel(const MapView& xView, float fWorldX, float fWorldZ)
	{
		return Vec2(
			(fWorldX - xView.m_fWorldMinX) * xView.m_fInvExtentM * xView.m_fPanelSizePx,
			(fWorldZ - xView.m_fWorldMinZ) * xView.m_fInvExtentM * xView.m_fPanelSizePx);
	}

	void RoomPanelRect(const MapView& xView, float fCentreX, float fCentreZ,
		float fHalfExtentX, float fHalfExtentZ, float fYawRadians,
		Vec2& xTopLeftOut, Vec2& xSizeOut)
	{
		// AABB of the yawed rect: |hx cos| + |hz sin| along X, etc.
		const float fCos = std::fabs(std::cos(fYawRadians));
		const float fSin = std::fabs(std::sin(fYawRadians));
		const float fAABBHalfX = fHalfExtentX * fCos + fHalfExtentZ * fSin;
		const float fAABBHalfZ = fHalfExtentX * fSin + fHalfExtentZ * fCos;

		const Vec2 xMin = WorldToPanel(xView, fCentreX - fAABBHalfX, fCentreZ - fAABBHalfZ);
		const Vec2 xMax = WorldToPanel(xView, fCentreX + fAABBHalfX, fCentreZ + fAABBHalfZ);
		xTopLeftOut = xMin;
		xSizeOut = Vec2(xMax.x - xMin.x, xMax.y - xMin.y);
	}

	Vec4 ColourForMemoryState(DP_Fog::MemoryTileState eState, uint8_t uVisibility)
	{
		if (eState == DP_Fog::MemoryTileState::NeverSeen)
		{
			return Vec4(0.0f, 0.0f, 0.0f, 0.0f);
		}
		const float fV = static_cast<float>(uVisibility) / 255.0f;
		// Remembered rooms never fully vanish (hidden floor), and the colour
		// desaturates from a cool fresh tint toward flat grey as the memory
		// ages — mirroring the world fog's dimming.
		const float fAlpha = std::max(fHIDDEN_FLOOR_ALPHA, fV);
		const Vec3 xFresh(0.72f, 0.76f, 0.86f);
		const Vec3 xStale(0.45f, 0.45f, 0.50f);
		const Vec3 xColour = xStale + (xFresh - xStale) * fV;
		return Vec4(xColour.x, xColour.y, xColour.z, fAlpha);
	}
}
