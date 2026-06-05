#pragma once

#include "Collections/Zenith_Vector.h"
#include "DataStream/Zenith_DataStream.h"
#include <cmath>
#include <cstdint>

// ============================================================================
// CB_TerrainHeightfield — game-owned, CPU-authoritative terrain height field.
//
// Stores normalized heights [0,1] on a regular grid covering the world, and is
// the single source of truth for "how high is the ground at (x,z)". Roads and
// buildings query GetHeightAt to sit on the terrain; the runtime deform tools
// (raise/lower/flatten/smooth) edit it directly via ApplyBrush. Pure logic — no
// engine/Flux dependency — so it unit-tests headlessly and serializes for
// save/load.
//
// v1 renders a flat ground (the field is initialised flat); wiring the field to
// a deforming GPU terrain mesh is a documented follow-up. The field itself is
// fully runtime-editable and authoritative for all gameplay height queries.
// ============================================================================

enum CB_ETerrainBrushMode : uint8_t
{
	CB_TERRAIN_BRUSH_RAISE   = 0,
	CB_TERRAIN_BRUSH_LOWER   = 1,
	CB_TERRAIN_BRUSH_FLATTEN = 2,
	CB_TERRAIN_BRUSH_SMOOTH  = 3,
	CB_TERRAIN_BRUSH_COUNT
};

struct CB_TerrainBrush
{
	CB_ETerrainBrushMode m_eMode        = CB_TERRAIN_BRUSH_FLATTEN;
	float                m_fCentreX     = 0.0f;  // world
	float                m_fCentreZ     = 0.0f;  // world
	float                m_fRadius      = 0.0f;  // world units
	float                m_fStrength    = 1.0f;  // 0..1 blend per application
	float                m_fTargetWorldY = 0.0f; // FLATTEN target height (world Y)
};

class CB_TerrainHeightfield
{
public:
	// Allocate a uSamplesX x uSamplesZ field. World position of sample (x,z) is
	// (fOriginX + x*fCellSize, fOriginZ + z*fCellSize); world height =
	// normalized * fHeightScale. Initialised flat (all zero).
	void Init(uint32_t uSamplesX, uint32_t uSamplesZ, float fCellSize,
	          float fOriginX, float fOriginZ, float fHeightScale)
	{
		m_uSamplesX   = uSamplesX;
		m_uSamplesZ   = uSamplesZ;
		m_fCellSize   = (fCellSize > 0.0f) ? fCellSize : 1.0f;
		m_fOriginX    = fOriginX;
		m_fOriginZ    = fOriginZ;
		m_fHeightScale = fHeightScale;
		m_uDirtyCount = 0;

		m_afHeights.Clear();
		const uint32_t uCount = uSamplesX * uSamplesZ;
		m_afHeights.Reserve(uCount);
		for (uint32_t u = 0; u < uCount; ++u)
		{
			m_afHeights.PushBack(0.0f);
		}
	}

	void Shutdown()
	{
		m_afHeights.Clear();
		m_uSamplesX = 0;
		m_uSamplesZ = 0;
	}

	bool     IsInitialized() const { return m_uSamplesX > 0 && m_uSamplesZ > 0; }
	uint32_t GetSamplesX()   const { return m_uSamplesX; }
	uint32_t GetSamplesZ()   const { return m_uSamplesZ; }
	float    GetHeightScale() const { return m_fHeightScale; }

	// World-space ground height at (fWorldX, fWorldZ), bilinearly filtered.
	float GetHeightAt(float fWorldX, float fWorldZ) const
	{
		if (!IsInitialized())
		{
			return 0.0f;
		}
		float fX = (fWorldX - m_fOriginX) / m_fCellSize;
		float fZ = (fWorldZ - m_fOriginZ) / m_fCellSize;
		fX = Clampf(fX, 0.0f, static_cast<float>(m_uSamplesX - 1));
		fZ = Clampf(fZ, 0.0f, static_cast<float>(m_uSamplesZ - 1));

		const uint32_t uX0 = static_cast<uint32_t>(std::floor(fX));
		const uint32_t uZ0 = static_cast<uint32_t>(std::floor(fZ));
		const uint32_t uX1 = (uX0 + 1 < m_uSamplesX) ? uX0 + 1 : uX0;
		const uint32_t uZ1 = (uZ0 + 1 < m_uSamplesZ) ? uZ0 + 1 : uZ0;
		const float fTX = fX - static_cast<float>(uX0);
		const float fTZ = fZ - static_cast<float>(uZ0);

		const float fH00 = Normalized(uX0, uZ0);
		const float fH10 = Normalized(uX1, uZ0);
		const float fH01 = Normalized(uX0, uZ1);
		const float fH11 = Normalized(uX1, uZ1);
		const float fTop = fH00 + (fH10 - fH00) * fTX;
		const float fBot = fH01 + (fH11 - fH01) * fTX;
		const float fN   = fTop + (fBot - fTop) * fTZ;
		return fN * m_fHeightScale;
	}

	// Raw normalized sample accessor (tests / serialization / future GPU sync).
	float Normalized(uint32_t uX, uint32_t uZ) const
	{
		if (uX >= m_uSamplesX || uZ >= m_uSamplesZ)
		{
			return 0.0f;
		}
		return m_afHeights.Get(uZ * m_uSamplesX + uX);
	}

	// Author a raw normalized sample (used to shape the initial terrain to match
	// the baked GPU mesh; world height = value * heightScale).
	void SetNormalized(uint32_t uX, uint32_t uZ, float fValue)
	{
		if (uX < m_uSamplesX && uZ < m_uSamplesZ)
		{
			m_afHeights.Get(uZ * m_uSamplesX + uX) = fValue;
		}
	}

	// Apply a runtime deform brush. Edits the field in place and accumulates a
	// dirty-sample count (a later GPU-sync pass / tests can read + clear it).
	void ApplyBrush(const CB_TerrainBrush& xBrush)
	{
		if (!IsInitialized() || xBrush.m_fRadius <= 0.0f)
		{
			return;
		}

		const float fCentreFX = (xBrush.m_fCentreX - m_fOriginX) / m_fCellSize;
		const float fCentreFZ = (xBrush.m_fCentreZ - m_fOriginZ) / m_fCellSize;
		const float fRadiusSamples = xBrush.m_fRadius / m_fCellSize;
		if (fRadiusSamples <= 0.0f)
		{
			return;
		}

		const int iMinX = MaxI(0, static_cast<int>(std::floor(fCentreFX - fRadiusSamples)));
		const int iMaxX = MinI(static_cast<int>(m_uSamplesX) - 1, static_cast<int>(std::ceil(fCentreFX + fRadiusSamples)));
		const int iMinZ = MaxI(0, static_cast<int>(std::floor(fCentreFZ - fRadiusSamples)));
		const int iMaxZ = MinI(static_cast<int>(m_uSamplesZ) - 1, static_cast<int>(std::ceil(fCentreFZ + fRadiusSamples)));

		const float fTargetNorm = Clampf(xBrush.m_fTargetWorldY / m_fHeightScale, 0.0f, 1.0f);
		const float fRaiseStep  = 0.1f;  // normalized height change at full weight

		for (int iZ = iMinZ; iZ <= iMaxZ; ++iZ)
		{
			for (int iX = iMinX; iX <= iMaxX; ++iX)
			{
				const float fDX = static_cast<float>(iX) - fCentreFX;
				const float fDZ = static_cast<float>(iZ) - fCentreFZ;
				const float fDist = std::sqrt(fDX * fDX + fDZ * fDZ);
				if (fDist > fRadiusSamples)
				{
					continue;
				}
				const float fFalloff = 1.0f - (fDist / fRadiusSamples);
				const float fWeight = Clampf(xBrush.m_fStrength * fFalloff, 0.0f, 1.0f);
				if (fWeight <= 0.0f)
				{
					continue;
				}

				const uint32_t uIdx = static_cast<uint32_t>(iZ) * m_uSamplesX + static_cast<uint32_t>(iX);
				const float fOld = m_afHeights.Get(uIdx);
				float fNew = fOld;
				switch (xBrush.m_eMode)
				{
				case CB_TERRAIN_BRUSH_RAISE:   fNew = fOld + fWeight * fRaiseStep; break;
				case CB_TERRAIN_BRUSH_LOWER:   fNew = fOld - fWeight * fRaiseStep; break;
				case CB_TERRAIN_BRUSH_FLATTEN: fNew = fOld + (fTargetNorm - fOld) * fWeight; break;
				case CB_TERRAIN_BRUSH_SMOOTH:  fNew = fOld + (NeighbourAverage(iX, iZ) - fOld) * fWeight; break;
				default: break;
				}
				fNew = Clampf(fNew, 0.0f, 1.0f);
				if (fNew != fOld)
				{
					m_afHeights.Get(uIdx) = fNew;
					++m_uDirtyCount;
				}
			}
		}
	}

	uint32_t GetDirtyCount() const { return m_uDirtyCount; }
	void     ClearDirty()          { m_uDirtyCount = 0; }

	// Serialization (Phase 8 save/load).
	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		xStream << m_uSamplesX;
		xStream << m_uSamplesZ;
		xStream << m_fCellSize;
		xStream << m_fOriginX;
		xStream << m_fOriginZ;
		xStream << m_fHeightScale;
		const uint32_t uCount = m_uSamplesX * m_uSamplesZ;
		for (uint32_t u = 0; u < uCount; ++u)
		{
			xStream << m_afHeights.Get(u);
		}
	}
	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		uint32_t uSX, uSZ;
		float fCell, fOX, fOZ, fScale;
		xStream >> uSX;
		xStream >> uSZ;
		xStream >> fCell;
		xStream >> fOX;
		xStream >> fOZ;
		xStream >> fScale;
		Init(uSX, uSZ, fCell, fOX, fOZ, fScale);
		const uint32_t uCount = uSX * uSZ;
		for (uint32_t u = 0; u < uCount; ++u)
		{
			float f;
			xStream >> f;
			m_afHeights.Get(u) = f;
		}
	}

private:
	static float Clampf(float f, float lo, float hi) { return (f < lo) ? lo : ((f > hi) ? hi : f); }
	static int   MinI(int a, int b) { return (a < b) ? a : b; }
	static int   MaxI(int a, int b) { return (a > b) ? a : b; }

	float NeighbourAverage(int iX, int iZ) const
	{
		float fSum = 0.0f;
		int iCount = 0;
		for (int dz = -1; dz <= 1; ++dz)
		{
			for (int dx = -1; dx <= 1; ++dx)
			{
				const int nx = iX + dx;
				const int nz = iZ + dz;
				if (nx < 0 || nz < 0 || nx >= static_cast<int>(m_uSamplesX) || nz >= static_cast<int>(m_uSamplesZ))
				{
					continue;
				}
				fSum += m_afHeights.Get(static_cast<uint32_t>(nz) * m_uSamplesX + static_cast<uint32_t>(nx));
				++iCount;
			}
		}
		return (iCount > 0) ? (fSum / static_cast<float>(iCount)) : 0.0f;
	}

	Zenith_Vector<float> m_afHeights;       // normalized [0,1], row-major [z*sx + x]
	uint32_t             m_uSamplesX = 0;
	uint32_t             m_uSamplesZ = 0;
	float                m_fCellSize    = 1.0f;
	float                m_fOriginX     = 0.0f;
	float                m_fOriginZ     = 0.0f;
	float                m_fHeightScale = 1.0f;
	uint32_t             m_uDirtyCount  = 0;
};
