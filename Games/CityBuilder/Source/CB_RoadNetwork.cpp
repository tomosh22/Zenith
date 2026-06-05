#include "Zenith.h"

#include "CityBuilder/Source/CB_RoadNetwork.h"
#include <unordered_map>

void CB_RoadNetwork::Initialize(CB_CityGrid* pxGrid)
{
	m_pxGrid = pxGrid;
	m_uRoadCellCount = 0;
	m_axRoadCells.Clear();
}

void CB_RoadNetwork::Shutdown()
{
	m_pxGrid = nullptr;
	m_uRoadCellCount = 0;
}

CB_ERoadPiece CB_RoadNetwork::DeterminePieceFromMask(uint8_t uMask)
{
	const uint8_t N = CB_ROAD_DIR_NORTH;
	const uint8_t E = CB_ROAD_DIR_EAST;
	const uint8_t S = CB_ROAD_DIR_SOUTH;
	const uint8_t W = CB_ROAD_DIR_WEST;
	switch (uMask & 0x0F)
	{
	case (N | S):         return CB_ROAD_PIECE_STRAIGHT_NS;
	case (E | W):         return CB_ROAD_PIECE_STRAIGHT_EW;
	case (N | E):         return CB_ROAD_PIECE_CORNER_NE;
	case (S | E):         return CB_ROAD_PIECE_CORNER_SE;
	case (S | W):         return CB_ROAD_PIECE_CORNER_SW;
	case (N | W):         return CB_ROAD_PIECE_CORNER_NW;
	case (N | E | S):     return CB_ROAD_PIECE_T_E;  // bar N-S, open stem East
	case (E | S | W):     return CB_ROAD_PIECE_T_S;  // bar E-W, open stem South
	case (N | S | W):     return CB_ROAD_PIECE_T_W;  // bar N-S, open stem West
	case (N | E | W):     return CB_ROAD_PIECE_T_N;  // bar E-W, open stem North
	case (N | E | S | W): return CB_ROAD_PIECE_INTERSECTION;
	default:
		// Isolated cell or a single connection (dead end): align a straight piece
		// with the connection axis (default to NS for the isolated case).
		return (uMask & (N | S)) ? CB_ROAD_PIECE_STRAIGHT_NS : ((uMask & (E | W)) ? CB_ROAD_PIECE_STRAIGHT_EW : CB_ROAD_PIECE_STRAIGHT_NS);
	}
}

uint8_t CB_RoadNetwork::ComputeNeighbourMask(uint32_t uX, uint32_t uZ) const
{
	if (!m_pxGrid)
	{
		return 0;
	}
	uint8_t uMask = 0;
	if (uZ > 0 && m_pxGrid->GetCell(uX, uZ - 1).HasRoad())                       { uMask |= CB_ROAD_DIR_NORTH; }
	if (uX + 1 < m_pxGrid->GetWidth() && m_pxGrid->GetCell(uX + 1, uZ).HasRoad()) { uMask |= CB_ROAD_DIR_EAST; }
	if (uZ + 1 < m_pxGrid->GetHeight() && m_pxGrid->GetCell(uX, uZ + 1).HasRoad()){ uMask |= CB_ROAD_DIR_SOUTH; }
	if (uX > 0 && m_pxGrid->GetCell(uX - 1, uZ).HasRoad())                       { uMask |= CB_ROAD_DIR_WEST; }
	return uMask;
}

CB_ERoadPiece CB_RoadNetwork::GetPieceAt(uint32_t uX, uint32_t uZ) const
{
	if (!m_pxGrid || !m_pxGrid->IsInBounds(uX, uZ))
	{
		return CB_ROAD_PIECE_STRAIGHT_NS;
	}
	return DeterminePieceFromMask(m_pxGrid->GetCell(uX, uZ).GetRoadMask());
}

void CB_RoadNetwork::RefreshMasks(uint32_t uX, uint32_t uZ)
{
	auto RefreshOne = [this](uint32_t cx, uint32_t cz)
	{
		if (m_pxGrid->IsInBounds(cx, cz))
		{
			CB_Cell& xCell = m_pxGrid->GetCell(cx, cz);
			if (xCell.HasRoad())
			{
				xCell.SetRoadMask(ComputeNeighbourMask(cx, cz));
			}
			else
			{
				xCell.SetRoadMask(0);
			}
		}
	};
	RefreshOne(uX, uZ);
	if (uZ > 0)                          { RefreshOne(uX, uZ - 1); }
	if (uX + 1 < m_pxGrid->GetWidth())   { RefreshOne(uX + 1, uZ); }
	if (uZ + 1 < m_pxGrid->GetHeight())  { RefreshOne(uX, uZ + 1); }
	if (uX > 0)                          { RefreshOne(uX - 1, uZ); }
}

void CB_RoadNetwork::UpdateLocalRoadAccess(uint32_t uX, uint32_t uZ)
{
	auto HasRoadNeighbour = [this](uint32_t cx, uint32_t cz) -> bool
	{
		if (cz > 0 && m_pxGrid->GetCell(cx, cz - 1).HasRoad())                       { return true; }
		if (cx + 1 < m_pxGrid->GetWidth() && m_pxGrid->GetCell(cx + 1, cz).HasRoad()) { return true; }
		if (cz + 1 < m_pxGrid->GetHeight() && m_pxGrid->GetCell(cx, cz + 1).HasRoad()){ return true; }
		if (cx > 0 && m_pxGrid->GetCell(cx - 1, cz).HasRoad())                       { return true; }
		return false;
	};
	auto UpdateOne = [this, &HasRoadNeighbour](uint32_t cx, uint32_t cz)
	{
		if (!m_pxGrid->IsInBounds(cx, cz))
		{
			return;
		}
		CB_Cell& xCell = m_pxGrid->GetCell(cx, cz);
		xCell.SetRoadAccess(xCell.HasRoad() || HasRoadNeighbour(cx, cz));
	};
	UpdateOne(uX, uZ);
	if (uZ > 0)                          { UpdateOne(uX, uZ - 1); }
	if (uX + 1 < m_pxGrid->GetWidth())   { UpdateOne(uX + 1, uZ); }
	if (uZ + 1 < m_pxGrid->GetHeight())  { UpdateOne(uX, uZ + 1); }
	if (uX > 0)                          { UpdateOne(uX - 1, uZ); }
}

bool CB_RoadNetwork::PlaceRoad(uint32_t uX, uint32_t uZ, CB_ERoadType eType)
{
	if (!m_pxGrid || !m_pxGrid->IsInBounds(uX, uZ) || eType == CB_ROAD_NONE)
	{
		return false;
	}
	CB_Cell& xCell = m_pxGrid->GetCell(uX, uZ);
	if (xCell.HasBuilding())
	{
		return false;
	}
	if (xCell.HasRoad() && xCell.GetRoadType() == eType)
	{
		return false;  // no change
	}
	const bool bWasRoad = xCell.HasRoad();
	xCell.SetZone(CB_ZONE_NONE);
	xCell.SetDensity(0);
	xCell.SetRoadType(eType);
	if (!bWasRoad)
	{
		++m_uRoadCellCount;
		m_axRoadCells.PushBack(uZ * m_pxGrid->GetWidth() + uX);
	}
	RefreshMasks(uX, uZ);
	UpdateLocalRoadAccess(uX, uZ);
	return true;
}

bool CB_RoadNetwork::RemoveRoad(uint32_t uX, uint32_t uZ)
{
	if (!m_pxGrid || !m_pxGrid->IsInBounds(uX, uZ))
	{
		return false;
	}
	CB_Cell& xCell = m_pxGrid->GetCell(uX, uZ);
	if (!xCell.HasRoad())
	{
		return false;
	}
	xCell.SetRoadType(CB_ROAD_NONE);
	xCell.SetRoadMask(0);
	if (m_uRoadCellCount > 0)
	{
		--m_uRoadCellCount;
	}
	m_axRoadCells.EraseValueSwap(uZ * m_pxGrid->GetWidth() + uX);
	RefreshMasks(uX, uZ);
	UpdateLocalRoadAccess(uX, uZ);
	return true;
}

bool CB_RoadNetwork::FindPath(uint32_t uStartX, uint32_t uStartZ, uint32_t uEndX, uint32_t uEndZ,
                              Zenith_Vector<uint32_t>& xOutPath) const
{
	xOutPath.Clear();
	if (!m_pxGrid || !m_pxGrid->IsInBounds(uStartX, uStartZ) || !m_pxGrid->IsInBounds(uEndX, uEndZ))
	{
		return false;
	}
	if (!m_pxGrid->GetCell(uStartX, uStartZ).HasRoad() || !m_pxGrid->GetCell(uEndX, uEndZ).HasRoad())
	{
		return false;
	}

	const uint32_t uW = m_pxGrid->GetWidth();
	const uint32_t uH = m_pxGrid->GetHeight();
	const uint32_t uStart = uStartZ * uW + uStartX;
	const uint32_t uEnd   = uEndZ * uW + uEndX;
	if (uStart == uEnd)
	{
		xOutPath.PushBack(uStart);
		return true;
	}

	// BFS over road cells. Uniform step cost => first arrival is shortest.
	Zenith_Vector<uint32_t> xQueue;
	std::unordered_map<uint32_t, uint32_t> xCameFrom;  // cell -> predecessor
	xQueue.PushBack(uStart);
	xCameFrom[uStart] = uStart;  // sentinel: start points to itself

	const uint32_t uMaxNodes = 500000;
	uint32_t uHead = 0;
	bool bFound = false;
	const int aiDX[4] = { 0, 1, 0, -1 };
	const int aiDZ[4] = { -1, 0, 1, 0 };
	while (uHead < xQueue.GetSize())
	{
		const uint32_t uCur = xQueue.Get(uHead++);
		if (uCur == uEnd)
		{
			bFound = true;
			break;
		}
		if (xCameFrom.size() > uMaxNodes)
		{
			return false;
		}
		const uint32_t uCX = uCur % uW;
		const uint32_t uCZ = uCur / uW;
		for (int i = 0; i < 4; ++i)
		{
			const int iNX = static_cast<int>(uCX) + aiDX[i];
			const int iNZ = static_cast<int>(uCZ) + aiDZ[i];
			if (iNX < 0 || iNZ < 0 || iNX >= static_cast<int>(uW) || iNZ >= static_cast<int>(uH))
			{
				continue;
			}
			const uint32_t uNIdx = static_cast<uint32_t>(iNZ) * uW + static_cast<uint32_t>(iNX);
			if (xCameFrom.find(uNIdx) != xCameFrom.end())
			{
				continue;  // already visited
			}
			if (!m_pxGrid->GetCell(static_cast<uint32_t>(iNX), static_cast<uint32_t>(iNZ)).HasRoad())
			{
				continue;
			}
			xCameFrom[uNIdx] = uCur;
			xQueue.PushBack(uNIdx);
		}
	}

	if (!bFound)
	{
		return false;
	}

	// Reconstruct end -> start, then reverse to start -> end.
	uint32_t uNode = uEnd;
	while (uNode != uStart)
	{
		xOutPath.PushBack(uNode);
		uNode = xCameFrom[uNode];
	}
	xOutPath.PushBack(uStart);
	xOutPath.Reverse();
	return true;
}
