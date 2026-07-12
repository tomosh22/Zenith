#pragma once

#include <cstdint>

// Inclusive rectangle of terrain chunks selected for an authoring export.
// Construction is transactional: invalid bounds leave the caller's existing
// rectangle unchanged. Every valid export includes the anchor chunk (0, 0),
// which remains the runtime's hard-required terrain chunk.
struct Flux_TerrainExportRect
{
public:
	static constexpr int32_t iCHUNK_GRID_SIZE = 64;

	static bool TryCreate(int32_t iMinX, int32_t iMinY,
		int32_t iMaxX, int32_t iMaxY, Flux_TerrainExportRect& xOut)
	{
		if (iMinX < 0 || iMinY < 0 ||
			iMaxX < iMinX || iMaxY < iMinY ||
			iMaxX >= iCHUNK_GRID_SIZE || iMaxY >= iCHUNK_GRID_SIZE ||
			!(iMinX <= 0 && iMaxX >= 0 && iMinY <= 0 && iMaxY >= 0))
		{
			return false;
		}

		Flux_TerrainExportRect xCandidate;
		xCandidate.m_iMinX = iMinX;
		xCandidate.m_iMinY = iMinY;
		xCandidate.m_iMaxX = iMaxX;
		xCandidate.m_iMaxY = iMaxY;
		xCandidate.m_bValid = true;
		xOut = xCandidate;
		return true;
	}

	bool IsValid() const { return m_bValid; }
	int32_t GetMinX() const { return m_iMinX; }
	int32_t GetMinY() const { return m_iMinY; }
	int32_t GetMaxX() const { return m_iMaxX; }
	int32_t GetMaxY() const { return m_iMaxY; }

	uint32_t Width() const
	{
		return m_bValid ? static_cast<uint32_t>(m_iMaxX - m_iMinX + 1) : 0;
	}

	uint32_t Height() const
	{
		return m_bValid ? static_cast<uint32_t>(m_iMaxY - m_iMinY + 1) : 0;
	}

	uint32_t ChunkCount() const
	{
		return Width() * Height();
	}

	bool TryGetChunkCoords(uint32_t uOrdinal, uint32_t& uChunkXOut,
		uint32_t& uChunkYOut) const
	{
		if (!m_bValid || uOrdinal >= ChunkCount())
		{
			return false;
		}

		const uint32_t uWidth = Width();
		const uint32_t uCandidateX = static_cast<uint32_t>(m_iMinX) + uOrdinal % uWidth;
		const uint32_t uCandidateY = static_cast<uint32_t>(m_iMinY) + uOrdinal / uWidth;
		uChunkXOut = uCandidateX;
		uChunkYOut = uCandidateY;
		return true;
	}

private:
	int32_t m_iMinX = -1;
	int32_t m_iMinY = -1;
	int32_t m_iMaxX = -1;
	int32_t m_iMaxY = -1;
	bool m_bValid = false;
};
