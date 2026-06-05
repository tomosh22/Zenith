#include "Zenith.h"

#include "CityBuilder/Source/CB_CityGrid.h"
#include <cmath>

void CB_CityGrid::Initialize(uint32_t uWidth, uint32_t uHeight, float fCellSize, float fOriginX, float fOriginZ)
{
	m_uWidth    = uWidth;
	m_uHeight   = uHeight;
	m_fCellSize = (fCellSize > 0.0f) ? fCellSize : 1.0f;
	m_fOriginX  = fOriginX;
	m_fOriginZ  = fOriginZ;

	m_axCells.Clear();
	const uint32_t uCount = uWidth * uHeight;
	m_axCells.Reserve(uCount);
	const CB_Cell xDefault;
	for (uint32_t u = 0; u < uCount; ++u)
	{
		m_axCells.PushBack(xDefault);
	}
}

void CB_CityGrid::Shutdown()
{
	m_axCells.Clear();
	m_uWidth = 0;
	m_uHeight = 0;
}

CB_Cell& CB_CityGrid::GetCell(uint32_t uX, uint32_t uZ)
{
	Zenith_Assert(IsInBounds(uX, uZ), "CB_CityGrid::GetCell out of bounds");
	return m_axCells.Get(uZ * m_uWidth + uX);
}

const CB_Cell& CB_CityGrid::GetCell(uint32_t uX, uint32_t uZ) const
{
	Zenith_Assert(IsInBounds(uX, uZ), "CB_CityGrid::GetCell out of bounds");
	return m_axCells.Get(uZ * m_uWidth + uX);
}

bool CB_CityGrid::WorldToGrid(float fWorldX, float fWorldZ, uint32_t& uOutX, uint32_t& uOutZ) const
{
	if (!IsInitialized())
	{
		return false;
	}
	const float fX = (fWorldX - m_fOriginX) / m_fCellSize;
	const float fZ = (fWorldZ - m_fOriginZ) / m_fCellSize;
	if (fX < 0.0f || fZ < 0.0f)
	{
		return false;
	}
	const uint32_t uX = static_cast<uint32_t>(std::floor(fX));
	const uint32_t uZ = static_cast<uint32_t>(std::floor(fZ));
	if (uX >= m_uWidth || uZ >= m_uHeight)
	{
		return false;
	}
	uOutX = uX;
	uOutZ = uZ;
	return true;
}

void CB_CityGrid::GridToWorld(uint32_t uX, uint32_t uZ, float& fOutX, float& fOutZ) const
{
	fOutX = m_fOriginX + (static_cast<float>(uX) + 0.5f) * m_fCellSize;
	fOutZ = m_fOriginZ + (static_cast<float>(uZ) + 0.5f) * m_fCellSize;
}

void CB_CityGrid::PaintZone(uint32_t uCentreX, uint32_t uCentreZ, uint32_t uRadius, CB_EZoneType eZone, uint8_t uDensity)
{
	if (!IsInitialized())
	{
		return;
	}
	const uint32_t uMinX = (uCentreX > uRadius) ? (uCentreX - uRadius) : 0;
	const uint32_t uMinZ = (uCentreZ > uRadius) ? (uCentreZ - uRadius) : 0;
	const uint32_t uMaxX = (uCentreX + uRadius < m_uWidth)  ? (uCentreX + uRadius) : (m_uWidth - 1);
	const uint32_t uMaxZ = (uCentreZ + uRadius < m_uHeight) ? (uCentreZ + uRadius) : (m_uHeight - 1);

	for (uint32_t uZ = uMinZ; uZ <= uMaxZ; ++uZ)
	{
		for (uint32_t uX = uMinX; uX <= uMaxX; ++uX)
		{
			CB_Cell& xCell = m_axCells.Get(uZ * m_uWidth + uX);
			if (xCell.HasRoad())
			{
				continue;  // roads aren't zoned
			}
			xCell.SetZone(eZone);
			xCell.SetDensity(uDensity);
		}
	}
}

void CB_CityGrid::ClearZone(uint32_t uCentreX, uint32_t uCentreZ, uint32_t uRadius)
{
	if (!IsInitialized())
	{
		return;
	}
	const uint32_t uMinX = (uCentreX > uRadius) ? (uCentreX - uRadius) : 0;
	const uint32_t uMinZ = (uCentreZ > uRadius) ? (uCentreZ - uRadius) : 0;
	const uint32_t uMaxX = (uCentreX + uRadius < m_uWidth)  ? (uCentreX + uRadius) : (m_uWidth - 1);
	const uint32_t uMaxZ = (uCentreZ + uRadius < m_uHeight) ? (uCentreZ + uRadius) : (m_uHeight - 1);

	for (uint32_t uZ = uMinZ; uZ <= uMaxZ; ++uZ)
	{
		for (uint32_t uX = uMinX; uX <= uMaxX; ++uX)
		{
			CB_Cell& xCell = m_axCells.Get(uZ * m_uWidth + uX);
			xCell.SetZone(CB_ZONE_NONE);
			xCell.SetDensity(0);
		}
	}
}

void CB_CityGrid::RecalculateRoadAccess()
{
	if (!IsInitialized())
	{
		return;
	}
	for (uint32_t uZ = 0; uZ < m_uHeight; ++uZ)
	{
		for (uint32_t uX = 0; uX < m_uWidth; ++uX)
		{
			CB_Cell& xCell = m_axCells.Get(uZ * m_uWidth + uX);
			bool bAccess = xCell.HasRoad();
			if (!bAccess && uX > 0)            { bAccess = m_axCells.Get(uZ * m_uWidth + (uX - 1)).HasRoad(); }
			if (!bAccess && uX + 1 < m_uWidth) { bAccess = m_axCells.Get(uZ * m_uWidth + (uX + 1)).HasRoad(); }
			if (!bAccess && uZ > 0)            { bAccess = m_axCells.Get((uZ - 1) * m_uWidth + uX).HasRoad(); }
			if (!bAccess && uZ + 1 < m_uHeight){ bAccess = m_axCells.Get((uZ + 1) * m_uWidth + uX).HasRoad(); }
			xCell.SetRoadAccess(bAccess);
		}
	}
}

void CB_CityGrid::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_uWidth;
	xStream << m_uHeight;
	xStream << m_fCellSize;
	xStream << m_fOriginX;
	xStream << m_fOriginZ;
	const uint32_t uCount = m_uWidth * m_uHeight;
	for (uint32_t u = 0; u < uCount; ++u)
	{
		xStream << m_axCells.Get(u);   // CB_Cell is trivially copyable
	}
}

void CB_CityGrid::ReadFromDataStream(Zenith_DataStream& xStream)
{
	uint32_t uW, uH;
	float fCell, fOX, fOZ;
	xStream >> uW;
	xStream >> uH;
	xStream >> fCell;
	xStream >> fOX;
	xStream >> fOZ;
	Initialize(uW, uH, fCell, fOX, fOZ);
	const uint32_t uCount = uW * uH;
	for (uint32_t u = 0; u < uCount; ++u)
	{
		xStream >> m_axCells.Get(u);
	}
}
