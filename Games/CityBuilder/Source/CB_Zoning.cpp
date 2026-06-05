#include "Zenith.h"

#include "CityBuilder/Source/CB_Zoning.h"
#include "CityBuilder/Source/CB_Serialize.h"
#include "Flux/Primitives/Flux_PrimitivesImpl.h"
#include <algorithm>

namespace
{
	Zenith_Maths::Vector3 ZoneColor(CB_EZoneType eZone)
	{
		switch (eZone)
		{
		case CB_ZONE_RESIDENTIAL: return Zenith_Maths::Vector3(0.20f, 0.65f, 0.25f);  // green
		case CB_ZONE_COMMERCIAL:  return Zenith_Maths::Vector3(0.20f, 0.40f, 0.80f);  // blue
		case CB_ZONE_INDUSTRIAL:  return Zenith_Maths::Vector3(0.80f, 0.70f, 0.20f);  // yellow
		case CB_ZONE_PARK:        return Zenith_Maths::Vector3(0.15f, 0.45f, 0.15f);  // dark green
		default:                  return Zenith_Maths::Vector3(0.5f, 0.5f, 0.5f);
		}
	}
}

void CB_Zoning::Reset()
{
	m_axLots.Clear();
	m_abSegHasLots.Clear();
	m_uActiveLots = 0;
}

void CB_Zoning::AddSegmentLots(uint32_t uSeg, const CB_RoadSegment& xSeg, const CB_TerrainHeightfield& xField)
{
	const float fLen = xSeg.m_xSpline.Length();
	if (fLen < LOT_SPACING * 0.6f)
	{
		return;  // too short for a lot
	}
	const float fOffset = xSeg.m_fWidth * 0.5f + LOT_DEPTH * 0.5f + 1.0f;  // plot centre, back from the road edge
	const uint32_t uSteps = std::max<uint32_t>(8u, static_cast<uint32_t>(fLen));  // ~1m sub-steps

	float fAccum   = 0.0f;
	float fNextLot = LOT_SPACING * 0.5f;
	Zenith_Maths::Vector2 xPrev = xSeg.m_xSpline.Evaluate(0.0f);

	for (uint32_t i = 1; i <= uSteps; ++i)
	{
		const float fT = static_cast<float>(i) / static_cast<float>(uSteps);
		const Zenith_Maths::Vector2 xCur = xSeg.m_xSpline.Evaluate(fT);
		fAccum += CB_Spline::Distance(xPrev, xCur);
		xPrev = xCur;

		while (fAccum >= fNextLot && fNextLot < fLen - LOT_SPACING * 0.4f)
		{
			const Zenith_Maths::Vector2 xC   = xCur;
			const Zenith_Maths::Vector2 xTan = xSeg.m_xSpline.UnitTangent(fT);
			const Zenith_Maths::Vector2 xPerp(-xTan.y, xTan.x);

			// Left + right plots; face the road (toward the centreline).
			for (int s = 0; s < 2; ++s)
			{
				const float fSide = (s == 0) ? 1.0f : -1.0f;
				CB_Lot xLot;
				xLot.m_xPos     = xC + xPerp * (fOffset * fSide);
				xLot.m_xFaceDir = xPerp * (-fSide);   // points back toward the road
				// Fine rendered surface (not the coarse field) so the zone tile + any building
				// that grows here sit on the actual GPU mesh — see CB_TerrainHeightfield::GetRenderSurfaceY.
				xLot.m_fWorldY  = xField.GetRenderSurfaceY(xLot.m_xPos.x, xLot.m_xPos.y);
				xLot.m_uSegment = uSeg;
				xLot.m_bActive  = true;
				m_axLots.PushBack(xLot);
				++m_uActiveLots;
			}
			fNextLot += LOT_SPACING;
		}
	}
}

void CB_Zoning::RemoveSegmentLots(uint32_t uSeg)
{
	for (uint32_t i = 0; i < m_axLots.GetSize(); ++i)
	{
		CB_Lot& xLot = m_axLots.Get(i);
		if (xLot.m_bActive && xLot.m_uSegment == uSeg)
		{
			xLot.m_bActive = false;
			if (m_uActiveLots > 0) --m_uActiveLots;
			// m_uBuildingId is left set; CB_BuildingPlacement despawns occupants
			// of now-inactive lots on its next tick.
		}
	}
}

void CB_Zoning::SyncToGraph(const CB_RoadGraph& xGraph, const CB_TerrainHeightfield& xField)
{
	const uint32_t uSegCount = xGraph.GetSegmentSlotCount();
	while (m_abSegHasLots.GetSize() < uSegCount)
	{
		m_abSegHasLots.PushBack(false);
	}
	for (uint32_t s = 0; s < uSegCount; ++s)
	{
		const bool bActive = xGraph.GetSegment(s).m_bActive;
		const bool bHas    = m_abSegHasLots.Get(s);
		if (bActive && !bHas)
		{
			AddSegmentLots(s, xGraph.GetSegment(s), xField);
			m_abSegHasLots.Get(s) = true;
		}
		else if (!bActive && bHas)
		{
			RemoveSegmentLots(s);
			m_abSegHasLots.Get(s) = false;
		}
	}
}

uint32_t CB_Zoning::PaintZone(float fWorldX, float fWorldZ, float fRadius, CB_EZoneType eZone, uint8_t uDensity)
{
	const float fR2 = fRadius * fRadius;
	uint32_t uCount = 0;
	for (uint32_t i = 0; i < m_axLots.GetSize(); ++i)
	{
		CB_Lot& xLot = m_axLots.Get(i);
		if (!xLot.m_bActive) continue;
		const float dx = xLot.m_xPos.x - fWorldX;
		const float dz = xLot.m_xPos.y - fWorldZ;
		if (dx * dx + dz * dz <= fR2)
		{
			xLot.m_eZone    = eZone;
			xLot.m_uDensity = uDensity;
			++uCount;
		}
	}
	return uCount;
}

uint32_t CB_Zoning::CountZonedLots(CB_EZoneType eZone) const
{
	uint32_t uCount = 0;
	for (uint32_t i = 0; i < m_axLots.GetSize(); ++i)
	{
		const CB_Lot& xLot = m_axLots.Get(i);
		if (xLot.m_bActive && xLot.m_eZone == eZone) ++uCount;
	}
	return uCount;
}

void CB_Zoning::RenderOverlay() const
{
	for (uint32_t i = 0; i < m_axLots.GetSize(); ++i)
	{
		const CB_Lot& xLot = m_axLots.Get(i);
		if (!xLot.m_bActive || xLot.m_eZone == CB_ZONE_NONE || xLot.m_uBuildingId != INVALID)
		{
			continue;   // empty/unzoned, or built-on (the building shows instead)
		}
		// m_fWorldY is the fine rendered surface; lift the tile 0.30m clear so the mesh can't
		// poke through it (the lot lands in the road's flattened bed, so the ground under the
		// tile is level — no slope to clip the flat slab).
		g_xEngine.Primitives().AddCube(
			Zenith_Maths::Vector3(xLot.m_xPos.x, xLot.m_fWorldY + 0.30f, xLot.m_xPos.y),
			Zenith_Maths::Vector3(LOT_DEPTH * 0.4f, 0.05f, LOT_DEPTH * 0.4f),
			ZoneColor(xLot.m_eZone));
	}
}

void CB_Zoning::WriteToDataStream(Zenith_DataStream& xStream) const
{
	CB_Serialize::WriteVec(xStream, m_axLots);
	CB_Serialize::WriteVec(xStream, m_abSegHasLots);
	xStream << m_uActiveLots;
}

void CB_Zoning::ReadFromDataStream(Zenith_DataStream& xStream)
{
	CB_Serialize::ReadVec(xStream, m_axLots);
	CB_Serialize::ReadVec(xStream, m_abSegHasLots);
	xStream >> m_uActiveLots;
}
