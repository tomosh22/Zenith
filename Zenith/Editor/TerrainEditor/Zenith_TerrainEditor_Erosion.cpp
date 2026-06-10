#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Editor/TerrainEditor/Zenith_TerrainEditor.h"
#include "Editor/TerrainEditor/Zenith_TerrainEditor_Noise.h"
#include "Editor/Zenith_UndoSystem.h"

//=============================================================================
// Erosion simulation: hydraulic (particle/droplet model) + thermal (talus-
// angle relaxation).
//
// THREADING: erosion runs ONLY on the main thread (synchronously, or sliced
// across frames from UpdatePerFrame). The terrain stream-in hook reads the
// heightfield from PreRenderUpdate on the same thread later in the frame, so
// main-thread slicing is race-free by construction — a background-task
// erosion would race it. Determinism: xorshift droplets seeded per-index from
// an integer hash, fixed iteration order, no std random engines.
//=============================================================================

namespace
{
	// Droplet model tuning (classic particle-erosion constants; heights are
	// normalized [0,1] over 512 m, cells are 1 m).
	constexpr u_int uDROPLET_MAX_STEPS = 64;
	constexpr float fDROPLET_INERTIA = 0.05f;
	constexpr float fDROPLET_CAPACITY = 4.0f;
	constexpr float fDROPLET_MIN_SLOPE = 0.0001f;
	constexpr float fDROPLET_ERODE_SPEED = 0.3f;
	constexpr float fDROPLET_DEPOSIT_SPEED = 0.3f;
	constexpr float fDROPLET_EVAPORATE = 0.012f;
	constexpr float fDROPLET_GRAVITY = 4.0f;

	constexpr u_int uDROPLETS_PER_SLICE = 5000;
	constexpr u_int uTHERMAL_ROWS_PER_SLICE = 512;
}

void Zenith_TerrainEditor::RunErosion(const Zenith_TerrainErosionParams& xParams, bool bSynchronous)
{
	EnsureImagesAllocated();

	// Erosion edits are global-ish (droplets wander) — not undoable.
	if (g_xEngine.UndoSystem().GetUndoStackSize() > 0 || g_xEngine.UndoSystem().GetRedoStackSize() > 0)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "[TerrainEditor] Erosion clears the undo history");
		g_xEngine.UndoSystem().Clear();
	}

	m_xActiveErosion = xParams;
	m_uErosionDropletsDone = 0;
	m_uErosionThermalRowsDone = 0;

	u_int uRows = uHEIGHTFIELD_SIZE;
	if (xParams.m_bRegionOnly)
	{
		const u_int uZ0 = static_cast<u_int>(std::max(0.0f, xParams.m_fRegionCentreZ - xParams.m_fRegionRadius));
		const u_int uZ1 = static_cast<u_int>(std::min(static_cast<float>(uHEIGHTFIELD_SIZE - 1), xParams.m_fRegionCentreZ + xParams.m_fRegionRadius));
		uRows = (uZ1 >= uZ0) ? (uZ1 - uZ0 + 1) : 0;
	}
	m_uErosionThermalRowsTotal = uRows * xParams.m_uThermalIterations;

	if (bSynchronous)
	{
		m_bErosionRunning = true;
		while (m_bErosionRunning)
		{
			StepErosionSlice();
		}
		Zenith_Log(LOG_CATEGORY_EDITOR, "[TerrainEditor] Erosion complete (%u droplets, %u thermal iterations)",
			xParams.m_uHydraulicDroplets, xParams.m_uThermalIterations);
	}
	else
	{
		m_bErosionRunning = true;
		Zenith_Log(LOG_CATEGORY_EDITOR, "[TerrainEditor] Erosion started (%u droplets, %u thermal iterations, sliced)",
			xParams.m_uHydraulicDroplets, xParams.m_uThermalIterations);
	}
}

void Zenith_TerrainEditor::StepErosionSlice()
{
	if (!m_bErosionRunning)
	{
		return;
	}

	if (m_uErosionDropletsDone < m_xActiveErosion.m_uHydraulicDroplets)
	{
		const u_int uCount = std::min(uDROPLETS_PER_SLICE, m_xActiveErosion.m_uHydraulicDroplets - m_uErosionDropletsDone);
		RunHydraulicDroplets(m_uErosionDropletsDone, uCount);
		m_uErosionDropletsDone += uCount;
		return;
	}

	if (m_uErosionThermalRowsDone < m_uErosionThermalRowsTotal)
	{
		const u_int uCount = std::min(uTHERMAL_ROWS_PER_SLICE, m_uErosionThermalRowsTotal - m_uErosionThermalRowsDone);
		RunThermalRows(m_uErosionThermalRowsDone, uCount);
		m_uErosionThermalRowsDone += uCount;
		return;
	}

	m_bErosionRunning = false;
}

void Zenith_TerrainEditor::RunHydraulicDroplets(u_int uFirstDroplet, u_int uCount)
{
	const int iSize = static_cast<int>(uHEIGHTFIELD_SIZE);
	const float fMaxCoord = static_cast<float>(iSize - 2);

	// Dirty-bbox of everything this slice touched.
	float fMinX = 1.0e9f, fMaxX = -1.0e9f, fMinZ = 1.0e9f, fMaxZ = -1.0e9f;
	auto Touch = [&](float fX, float fZ)
	{
		fMinX = std::min(fMinX, fX); fMaxX = std::max(fMaxX, fX);
		fMinZ = std::min(fMinZ, fZ); fMaxZ = std::max(fMaxZ, fZ);
	};

	// Bilinear height + gradient at a continuous position (cell corners).
	auto SampleHeightAndGradient = [this](float fX, float fZ, float& fGradXOut, float& fGradZOut) -> float
	{
		const int iX = static_cast<int>(fX);
		const int iZ = static_cast<int>(fZ);
		const float fTX = fX - static_cast<float>(iX);
		const float fTZ = fZ - static_cast<float>(iZ);
		const float fH00 = m_xHeightfield.At(static_cast<u_int>(iZ), static_cast<u_int>(iX));
		const float fH10 = m_xHeightfield.At(static_cast<u_int>(iZ), static_cast<u_int>(iX + 1));
		const float fH01 = m_xHeightfield.At(static_cast<u_int>(iZ + 1), static_cast<u_int>(iX));
		const float fH11 = m_xHeightfield.At(static_cast<u_int>(iZ + 1), static_cast<u_int>(iX + 1));
		fGradXOut = (fH10 - fH00) * (1.0f - fTZ) + (fH11 - fH01) * fTZ;
		fGradZOut = (fH01 - fH00) * (1.0f - fTX) + (fH11 - fH10) * fTX;
		return fH00 * (1.0f - fTX) * (1.0f - fTZ) + fH10 * fTX * (1.0f - fTZ)
			+ fH01 * (1.0f - fTX) * fTZ + fH11 * fTX * fTZ;
	};

	for (u_int uDroplet = 0; uDroplet < uCount; uDroplet++)
	{
		const u_int uIndex = uFirstDroplet + uDroplet;
		Zenith_TerrainNoise::XorShift32 xRng(
			Zenith_TerrainNoise::HashCoords(static_cast<int>(uIndex), 0x51ED, m_xActiveErosion.m_uSeed));

		float fPosX, fPosZ;
		if (m_xActiveErosion.m_bRegionOnly)
		{
			// Uniform disc sample around the region centre.
			const float fAngle = xRng.NextFloat01() * 6.28318530f;
			const float fDist = sqrtf(xRng.NextFloat01()) * m_xActiveErosion.m_fRegionRadius;
			fPosX = m_xActiveErosion.m_fRegionCentreX + cosf(fAngle) * fDist;
			fPosZ = m_xActiveErosion.m_fRegionCentreZ + sinf(fAngle) * fDist;
			fPosX = std::max(1.0f, std::min(fPosX, fMaxCoord));
			fPosZ = std::max(1.0f, std::min(fPosZ, fMaxCoord));
		}
		else
		{
			fPosX = 1.0f + xRng.NextFloat01() * (fMaxCoord - 1.0f);
			fPosZ = 1.0f + xRng.NextFloat01() * (fMaxCoord - 1.0f);
		}

		float fDirX = 0.0f, fDirZ = 0.0f;
		float fSpeed = 1.0f;
		float fWater = 1.0f;
		float fSediment = 0.0f;

		for (u_int uStep = 0; uStep < uDROPLET_MAX_STEPS; uStep++)
		{
			const int iCellX = static_cast<int>(fPosX);
			const int iCellZ = static_cast<int>(fPosZ);
			const float fCellTX = fPosX - static_cast<float>(iCellX);
			const float fCellTZ = fPosZ - static_cast<float>(iCellZ);

			float fGradX, fGradZ;
			const float fOldHeight = SampleHeightAndGradient(fPosX, fPosZ, fGradX, fGradZ);

			fDirX = fDirX * fDROPLET_INERTIA - fGradX * (1.0f - fDROPLET_INERTIA);
			fDirZ = fDirZ * fDROPLET_INERTIA - fGradZ * (1.0f - fDROPLET_INERTIA);
			const float fLen = sqrtf(fDirX * fDirX + fDirZ * fDirZ);
			if (fLen < 1.0e-8f)
			{
				break;  // flat — nowhere to go
			}
			fDirX /= fLen;
			fDirZ /= fLen;

			fPosX += fDirX;
			fPosZ += fDirZ;
			if (fPosX < 1.0f || fPosX >= fMaxCoord || fPosZ < 1.0f || fPosZ >= fMaxCoord)
			{
				break;
			}

			float fNewGradX, fNewGradZ;
			const float fNewHeight = SampleHeightAndGradient(fPosX, fPosZ, fNewGradX, fNewGradZ);
			const float fDeltaHeight = fNewHeight - fOldHeight;

			const float fCapacity = std::max(-fDeltaHeight, fDROPLET_MIN_SLOPE) * fSpeed * fWater * fDROPLET_CAPACITY;

			if (fSediment > fCapacity || fDeltaHeight > 0.0f)
			{
				// Deposit (fill the pit when moving uphill, else surplus).
				const float fDeposit = (fDeltaHeight > 0.0f)
					? std::min(fDeltaHeight, fSediment)
					: (fSediment - fCapacity) * fDROPLET_DEPOSIT_SPEED;
				fSediment -= fDeposit;

				m_xHeightfield.At(static_cast<u_int>(iCellZ), static_cast<u_int>(iCellX)) += fDeposit * (1.0f - fCellTX) * (1.0f - fCellTZ);
				m_xHeightfield.At(static_cast<u_int>(iCellZ), static_cast<u_int>(iCellX + 1)) += fDeposit * fCellTX * (1.0f - fCellTZ);
				m_xHeightfield.At(static_cast<u_int>(iCellZ + 1), static_cast<u_int>(iCellX)) += fDeposit * (1.0f - fCellTX) * fCellTZ;
				m_xHeightfield.At(static_cast<u_int>(iCellZ + 1), static_cast<u_int>(iCellX + 1)) += fDeposit * fCellTX * fCellTZ;
				Touch(static_cast<float>(iCellX), static_cast<float>(iCellZ));
				Touch(static_cast<float>(iCellX + 1), static_cast<float>(iCellZ + 1));
			}
			else
			{
				// Erode — never more than the height delta (prevents spikes).
				const float fErode = std::min((fCapacity - fSediment) * fDROPLET_ERODE_SPEED, -fDeltaHeight);
				fSediment += fErode;

				m_xHeightfield.At(static_cast<u_int>(iCellZ), static_cast<u_int>(iCellX)) -= fErode * (1.0f - fCellTX) * (1.0f - fCellTZ);
				m_xHeightfield.At(static_cast<u_int>(iCellZ), static_cast<u_int>(iCellX + 1)) -= fErode * fCellTX * (1.0f - fCellTZ);
				m_xHeightfield.At(static_cast<u_int>(iCellZ + 1), static_cast<u_int>(iCellX)) -= fErode * (1.0f - fCellTX) * fCellTZ;
				m_xHeightfield.At(static_cast<u_int>(iCellZ + 1), static_cast<u_int>(iCellX + 1)) -= fErode * fCellTX * fCellTZ;
				Touch(static_cast<float>(iCellX), static_cast<float>(iCellZ));
				Touch(static_cast<float>(iCellX + 1), static_cast<float>(iCellZ + 1));
			}

			const float fSpeedSq = fSpeed * fSpeed - fDeltaHeight * fDROPLET_GRAVITY;
			fSpeed = sqrtf(std::max(0.0f, fSpeedSq));
			fWater *= (1.0f - fDROPLET_EVAPORATE);
			if (fWater < 0.01f)
			{
				break;
			}
		}
	}

	if (fMaxX >= fMinX)
	{
		MarkHeightRegionDirty(fMinX, fMinZ, fMaxX, fMaxZ);
	}
}

void Zenith_TerrainEditor::RunThermalRows(u_int uFirstRow, u_int uRowCount)
{
	// Talus threshold as a normalized per-metre height delta.
	const float fTalusNorm = tanf(m_xActiveErosion.m_fTalusAngleDeg * 0.0174533f) / fTERRAIN_MAX_HEIGHT;

	u_int uRowBase = 0;
	u_int uRowSpan = uHEIGHTFIELD_SIZE;
	u_int uColMin = 1;
	u_int uColMax = uHEIGHTFIELD_SIZE - 2;
	if (m_xActiveErosion.m_bRegionOnly)
	{
		uRowBase = static_cast<u_int>(std::max(0.0f, m_xActiveErosion.m_fRegionCentreZ - m_xActiveErosion.m_fRegionRadius));
		const u_int uRowEnd = static_cast<u_int>(std::min(static_cast<float>(uHEIGHTFIELD_SIZE - 1), m_xActiveErosion.m_fRegionCentreZ + m_xActiveErosion.m_fRegionRadius));
		uRowSpan = (uRowEnd >= uRowBase) ? (uRowEnd - uRowBase + 1) : 0;
		uColMin = static_cast<u_int>(std::max(1.0f, m_xActiveErosion.m_fRegionCentreX - m_xActiveErosion.m_fRegionRadius));
		uColMax = static_cast<u_int>(std::min(static_cast<float>(uHEIGHTFIELD_SIZE - 2), m_xActiveErosion.m_fRegionCentreX + m_xActiveErosion.m_fRegionRadius));
	}
	if (uRowSpan == 0 || uColMax < uColMin)
	{
		return;
	}

	float fMinX = 1.0e9f, fMaxX = -1.0e9f, fMinZ = 1.0e9f, fMaxZ = -1.0e9f;

	for (u_int uRowIt = 0; uRowIt < uRowCount; uRowIt++)
	{
		// Row index within the (possibly repeated-iteration) row sequence.
		const u_int uZ = uRowBase + ((uFirstRow + uRowIt) % uRowSpan);
		if (uZ == 0 || uZ >= uHEIGHTFIELD_SIZE - 1)
		{
			continue;
		}

		float* pfRow = m_xHeightfield.Row(uZ);
		float* pfRowUp = m_xHeightfield.Row(uZ - 1);
		float* pfRowDown = m_xHeightfield.Row(uZ + 1);
		for (u_int uX = uColMin; uX <= uColMax; uX++)
		{
			const float fH = pfRow[uX];

			// Steepest descent among the 4-neighbourhood.
			float* pfLowest = nullptr;
			float fLowestH = fH;
			float* apfNeighbours[4] = { &pfRow[uX - 1], &pfRow[uX + 1], &pfRowUp[uX], &pfRowDown[uX] };
			for (u_int u = 0; u < 4; u++)
			{
				if (*apfNeighbours[u] < fLowestH)
				{
					fLowestH = *apfNeighbours[u];
					pfLowest = apfNeighbours[u];
				}
			}
			if (pfLowest == nullptr)
			{
				continue;
			}

			const float fDelta = fH - fLowestH;
			if (fDelta <= fTalusNorm)
			{
				continue;
			}

			float fMove = (fDelta - fTalusNorm) * 0.25f;
			if (m_xActiveErosion.m_bRegionOnly)
			{
				// Falloff-mask the slump at the region edge.
				const float fDX = static_cast<float>(uX) - m_xActiveErosion.m_fRegionCentreX;
				const float fDZ = static_cast<float>(uZ) - m_xActiveErosion.m_fRegionCentreZ;
				const float fDist = sqrtf(fDX * fDX + fDZ * fDZ) / std::max(1.0f, m_xActiveErosion.m_fRegionRadius);
				if (fDist > 1.0f)
				{
					continue;
				}
				fMove *= 1.0f - fDist * fDist;
			}

			pfRow[uX] = fH - fMove;
			*pfLowest += fMove;

			fMinX = std::min(fMinX, static_cast<float>(uX) - 1.0f);
			fMaxX = std::max(fMaxX, static_cast<float>(uX) + 1.0f);
			fMinZ = std::min(fMinZ, static_cast<float>(uZ) - 1.0f);
			fMaxZ = std::max(fMaxZ, static_cast<float>(uZ) + 1.0f);
		}
	}

	if (fMaxX >= fMinX)
	{
		MarkHeightRegionDirty(fMinX, fMinZ, fMaxX, fMaxZ);
	}
}

#endif // ZENITH_TOOLS
