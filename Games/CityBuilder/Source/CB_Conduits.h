#pragma once

#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"
#include "DataStream/Zenith_DataStream.h"
#include <cstdint>

// ============================================================================
// CB_Conduits — Cities: Skylines-style utility networks: the player lays conduits
// (a utility easement carrying both power and water) to carry service from a source
// to buildings beyond the source's own radius. A conduit is "powered"/"watered" if
// it sits near a power plant / water tower OR near an already-energized conduit, so
// energy/water FLOODS along the connected chain. A building is served if a source OR
// an energized conduit reaches it — so a chain of conduits extends utility coverage.
//
// Pure data/logic (header-only) so it unit-tests headlessly + serializes; the sim
// (CB_BuildingPlacement) calls Energize(sources) each pass then IsPowered/IsWatered.
// ============================================================================

struct CB_Conduit
{
	float m_fX        = 0.0f;
	float m_fZ        = 0.0f;
	bool  m_bPowered  = false;   // recomputed each Energize
	bool  m_bWatered  = false;
};

class CB_Conduits
{
public:
	static constexpr float LINK_DIST   = 130.0f;   // two conduits connect within this
	static constexpr float SOURCE_DIST = 200.0f;   // a conduit energizes within this of a source
	static constexpr float SERVE_DIST  = 150.0f;   // a building is served within this of an energized conduit

	void Reset() { m_axConduits.Clear(); }

	void AddConduit(float fX, float fZ)
	{
		CB_Conduit xC; xC.m_fX = fX; xC.m_fZ = fZ;
		m_axConduits.PushBack(xC);
	}

	uint32_t GetCount() const { return m_axConduits.GetSize(); }
	const CB_Conduit& Get(uint32_t i) const { return m_axConduits.Get(i); }

	// Recompute which conduits carry power / water: seed from the sources, then flood
	// along links until stable. O(conduits² · passes) — fine for a city's worth.
	void Energize(const Zenith_Vector<Zenith_Maths::Vector2>& xPowerSrcs,
	              const Zenith_Vector<Zenith_Maths::Vector2>& xWaterSrcs)
	{
		const uint32_t uN = m_axConduits.GetSize();
		const float fSrc2  = SOURCE_DIST * SOURCE_DIST;
		const float fLink2 = LINK_DIST * LINK_DIST;
		for (uint32_t i = 0; i < uN; ++i)
		{
			CB_Conduit& xC = m_axConduits.Get(i);
			xC.m_bPowered = NearAny(xC.m_fX, xC.m_fZ, xPowerSrcs, fSrc2);
			xC.m_bWatered = NearAny(xC.m_fX, xC.m_fZ, xWaterSrcs, fSrc2);
		}
		// Flood along links (bounded by uN passes — a chain energizes one hop per pass).
		for (uint32_t pass = 0; pass < uN; ++pass)
		{
			bool bChanged = false;
			for (uint32_t i = 0; i < uN; ++i)
			{
				CB_Conduit& xC = m_axConduits.Get(i);
				if (xC.m_bPowered && xC.m_bWatered) { continue; }
				for (uint32_t j = 0; j < uN; ++j)
				{
					if (j == i) { continue; }
					const CB_Conduit& xO = m_axConduits.Get(j);
					const float dx = xC.m_fX - xO.m_fX, dz = xC.m_fZ - xO.m_fZ;
					if (dx * dx + dz * dz > fLink2) { continue; }
					if (xO.m_bPowered && !xC.m_bPowered) { xC.m_bPowered = true; bChanged = true; }
					if (xO.m_bWatered && !xC.m_bWatered) { xC.m_bWatered = true; bChanged = true; }
				}
			}
			if (!bChanged) { break; }
		}
	}

	bool IsPowered(float fX, float fZ) const { return NearEnergized(fX, fZ, true); }
	bool IsWatered(float fX, float fZ) const { return NearEnergized(fX, fZ, false); }

	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		const uint32_t uN = m_axConduits.GetSize();
		xStream << uN;
		for (uint32_t i = 0; i < uN; ++i)
		{
			const CB_Conduit& xC = m_axConduits.Get(i);
			xStream << xC.m_fX; xStream << xC.m_fZ;
		}
	}
	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		Reset();
		uint32_t uN = 0u;
		xStream >> uN;
		for (uint32_t i = 0; i < uN; ++i)
		{
			float fX = 0.0f, fZ = 0.0f;
			xStream >> fX; xStream >> fZ;
			AddConduit(fX, fZ);
		}
	}

private:
	static bool NearAny(float fX, float fZ, const Zenith_Vector<Zenith_Maths::Vector2>& xPts, float fR2)
	{
		for (uint32_t i = 0; i < xPts.GetSize(); ++i)
		{
			const Zenith_Maths::Vector2& xP = xPts.Get(i);
			const float dx = fX - xP.x, dz = fZ - xP.y;
			if (dx * dx + dz * dz <= fR2) { return true; }
		}
		return false;
	}

	bool NearEnergized(float fX, float fZ, bool bPower) const
	{
		const float fR2 = SERVE_DIST * SERVE_DIST;
		for (uint32_t i = 0; i < m_axConduits.GetSize(); ++i)
		{
			const CB_Conduit& xC = m_axConduits.Get(i);
			if (bPower ? !xC.m_bPowered : !xC.m_bWatered) { continue; }
			const float dx = fX - xC.m_fX, dz = fZ - xC.m_fZ;
			if (dx * dx + dz * dz <= fR2) { return true; }
		}
		return false;
	}

	Zenith_Vector<CB_Conduit> m_axConduits;
};
