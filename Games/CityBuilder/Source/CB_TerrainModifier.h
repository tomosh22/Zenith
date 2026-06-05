#pragma once

#include "CityBuilder/Components/CB_CityManager_Behaviour.h"
#include "CityBuilder/Source/CB_TerrainHeightfield.h"

// ============================================================================
// CB_TerrainModifier — thin game-side bridge to the active terrain heightfield.
//
// Roads and buildings call these to sit on / flatten the ground without knowing
// where the heightfield lives. Forwards to the CityManager's published active
// field; safe no-ops when no CityManager is live (e.g. unit tests with their
// own local field).
// ============================================================================
namespace CB_TerrainModifier
{
	inline CB_TerrainHeightfield* GetActive()
	{
		return CB_CityManager_Behaviour::GetActiveHeightfield();
	}

	inline float GetHeightAt(float fWorldX, float fWorldZ)
	{
		const CB_TerrainHeightfield* pxField = GetActive();
		return pxField ? pxField->GetHeightAt(fWorldX, fWorldZ) : 0.0f;
	}

	inline void ApplyBrush(const CB_TerrainBrush& xBrush)
	{
		if (CB_TerrainHeightfield* pxField = GetActive())
		{
			pxField->ApplyBrush(xBrush);
		}
	}

	// Flatten a building footprint to its current centre height (feathered skirt
	// via the 1.5x radius). fHalfExtent is the footprint half-size in world units.
	inline void FlattenForBuilding(float fCentreX, float fCentreZ, float fHalfExtent)
	{
		CB_TerrainHeightfield* pxField = GetActive();
		if (!pxField)
		{
			return;
		}
		CB_TerrainBrush xBrush;
		xBrush.m_eMode = CB_TERRAIN_BRUSH_FLATTEN;
		xBrush.m_fCentreX = fCentreX;
		xBrush.m_fCentreZ = fCentreZ;
		xBrush.m_fRadius = fHalfExtent * 1.5f;
		xBrush.m_fStrength = 1.0f;
		xBrush.m_fTargetWorldY = pxField->GetHeightAt(fCentreX, fCentreZ);
		pxField->ApplyBrush(xBrush);
	}

	// Flatten a single road cell to its current centre height.
	inline void FlattenForRoad(float fCentreX, float fCentreZ, float fHalfCell)
	{
		CB_TerrainHeightfield* pxField = GetActive();
		if (!pxField)
		{
			return;
		}
		CB_TerrainBrush xBrush;
		xBrush.m_eMode = CB_TERRAIN_BRUSH_FLATTEN;
		xBrush.m_fCentreX = fCentreX;
		xBrush.m_fCentreZ = fCentreZ;
		xBrush.m_fRadius = fHalfCell * 1.5f;
		xBrush.m_fStrength = 1.0f;
		xBrush.m_fTargetWorldY = pxField->GetHeightAt(fCentreX, fCentreZ);
		pxField->ApplyBrush(xBrush);
	}
}
