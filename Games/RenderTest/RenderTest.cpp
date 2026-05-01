#include "Zenith.h"

#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "DataStream/Zenith_DataStream.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "Flux/Terrain/Flux_Terrain.h"
#include "Flux/Terrain/Flux_TerrainStreamingManager.h"
#include "Flux/Terrain/Flux_TerrainConfig.h"
#include "Zenith_OS_Include.h"

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#pragma warning(push, 0)
#include <opencv2/opencv.hpp>
#pragma warning(pop)
#include "Memory/Zenith_MemoryManagement_Enabled.h"

#include "Editor/Zenith_EditorAutomation.h"
#include "Editor/Zenith_Editor.h"
#include "TaskSystem/Zenith_TaskSystem.h"

extern void ExportHeightmapFromMat(const cv::Mat& xHeightmap, const std::string& strOutputDir);
#endif

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <vector>

namespace RenderTest
{
	MaterialHandle g_axTerrainMaterials[4];
}

static bool s_bResourcesInitialized = false;
static uint32_t s_uRenderTestSmokeFrameLimit = 240;

static bool RenderTest_HasCommandLineFlag(const char* szFlag)
{
#ifdef ZENITH_WINDOWS
	for (int i = 1; i < __argc; i++)
	{
		if (std::strcmp(__argv[i], szFlag) == 0)
			return true;
	}
#else
	(void)szFlag;
#endif
	return false;
}

static uint32_t RenderTest_GetCommandLineUInt(const char* szPrefix, uint32_t uDefault)
{
#ifdef ZENITH_WINDOWS
	const size_t ulPrefixLen = std::strlen(szPrefix);
	for (int i = 1; i < __argc; i++)
	{
		if (std::strncmp(__argv[i], szPrefix, ulPrefixLen) == 0)
		{
			const uint32_t uValue = static_cast<uint32_t>(std::strtoul(__argv[i] + ulPrefixLen, nullptr, 10));
			return (uValue > 0) ? uValue : uDefault;
		}
	}
#else
	(void)szPrefix;
#endif
	return uDefault;
}

static bool RenderTest_IsSmokeMode()
{
	return RenderTest_HasCommandLineFlag("--rendertest-smoke");
}

static void RenderTest_RequestClose()
{
#ifdef ZENITH_WINDOWS
	Zenith_Window::GetInstance()->RequestClose();
#endif
}

static uint32_t RenderTest_CountHighResidentChunks(const Flux_TerrainStreamingState& xState)
{
	uint32_t uCount = 0;
	for (uint32_t u = 0; u < TOTAL_CHUNKS; u++)
	{
		if (xState.m_axChunkResidency[u].m_aeStates[LOD_HIGH] == Flux_TerrainLODResidencyState::RESIDENT)
			uCount++;
	}
	return uCount;
}

static uint32_t RenderTest_CountLowZeroChunks(const Flux_TerrainStreamingState& xState)
{
	uint32_t uCount = 0;
	for (uint32_t u = 0; u < TOTAL_CHUNKS; u++)
	{
		const Flux_TerrainLODAllocation& xAlloc = xState.m_axChunkResidency[u].m_axAllocations[LOD_LOW];
		if (xAlloc.m_uIndexCount == 0)
			uCount++;
	}
	return uCount;
}

// Probe B (CPU-side): walk every chunk's resident allocations and range-check
// the offsets+counts against the terrain's unified buffer bounds. This catches
// the same class of bug as a GPU readback of the indirect-draw buffer (stale
// residency pointing past the buffer end → garbage geometry on screen / device
// lost on bounds-checking GPUs) at its source — the CPU-side residency table
// the chunk-data buffer is built from.
static bool RenderTest_ValidateResidencyAllocationRanges(const Zenith_TerrainComponent& xTerrain,
														 const Flux_TerrainStreamingState& xState,
														 u_int uTerrainIndex,
														 uint32_t uFrame)
{
	const uint64_t ulVertexBufferSize = xTerrain.GetUnifiedVertexBuffer().GetBuffer().m_ulSize;
	const uint64_t ulIndexBufferSize  = xTerrain.GetUnifiedIndexBuffer().GetBuffer().m_ulSize;
	const uint32_t uVertexStride = xTerrain.m_uVertexStride;
	if (uVertexStride == 0) return true;  // pre-init terrain — nothing to validate

	const uint32_t uMaxVertices = static_cast<uint32_t>(ulVertexBufferSize / uVertexStride);
	const uint32_t uMaxIndices  = static_cast<uint32_t>(ulIndexBufferSize  / sizeof(uint32_t));

	bool bPass = true;
	for (uint32_t uChunkIdx = 0; uChunkIdx < TOTAL_CHUNKS; uChunkIdx++)
	{
		const Flux_TerrainChunkResidency& xResidency = xState.m_axChunkResidency[uChunkIdx];
		for (uint32_t uLOD = 0; uLOD < LOD_COUNT; uLOD++)
		{
			if (xResidency.m_aeStates[uLOD] != Flux_TerrainLODResidencyState::RESIDENT) continue;
			const Flux_TerrainLODAllocation& xAlloc = xResidency.m_axAllocations[uLOD];
			if (xAlloc.m_uIndexCount == 0) continue;  // zero-count is legal (empty chunk)

			const uint32_t uVertexEnd = xAlloc.m_uVertexOffset + xAlloc.m_uVertexCount;
			const uint32_t uIndexEnd  = xAlloc.m_uIndexOffset  + xAlloc.m_uIndexCount;
			if (xAlloc.m_uVertexOffset > uMaxVertices || uVertexEnd > uMaxVertices)
			{
				Zenith_Error(LOG_CATEGORY_TERRAIN,
					"RENDERTEST_SMOKE_FAIL: terrain[%u] frame %u chunk %u LOD %u vertex range [%u,%u) exceeds unified vertex count %u",
					uTerrainIndex, uFrame, uChunkIdx, uLOD,
					xAlloc.m_uVertexOffset, uVertexEnd, uMaxVertices);
				bPass = false;
			}
			if (xAlloc.m_uIndexOffset > uMaxIndices || uIndexEnd > uMaxIndices)
			{
				Zenith_Error(LOG_CATEGORY_TERRAIN,
					"RENDERTEST_SMOKE_FAIL: terrain[%u] frame %u chunk %u LOD %u index range [%u,%u) exceeds unified index count %u",
					uTerrainIndex, uFrame, uChunkIdx, uLOD,
					xAlloc.m_uIndexOffset, uIndexEnd, uMaxIndices);
				bPass = false;
			}
		}
	}
	return bPass;
}

// Probe C (CPU-side): track residency-state snapshots across multiple frames
// and assert that no chunk's residency flips in an alternating pattern
// (T0 == T2 != T1) — the streaming-side equivalent of the GPU LOD red/green
// flicker. With proper hysteresis, each chunk's residency should be either
// stable across the snapshots or monotonically transitioning (never oscillate).
struct RenderTest_ResidencySnapshot
{
	uint8_t m_aeStates[TOTAL_CHUNKS][LOD_COUNT] = {};
	bool    m_bCaptured = false;
};

static void RenderTest_CaptureResidencySnapshot(const Flux_TerrainStreamingState& xState,
												RenderTest_ResidencySnapshot& xSnapshot)
{
	for (uint32_t uChunkIdx = 0; uChunkIdx < TOTAL_CHUNKS; uChunkIdx++)
	{
		for (uint32_t uLOD = 0; uLOD < LOD_COUNT; uLOD++)
		{
			xSnapshot.m_aeStates[uChunkIdx][uLOD] =
				static_cast<uint8_t>(xState.m_axChunkResidency[uChunkIdx].m_aeStates[uLOD]);
		}
	}
	xSnapshot.m_bCaptured = true;
}

static bool RenderTest_AssertNoResidencyAlternation(const RenderTest_ResidencySnapshot& xT0,
													 const RenderTest_ResidencySnapshot& xT1,
													 const RenderTest_ResidencySnapshot& xT2,
													 u_int uTerrainIndex)
{
	if (!xT0.m_bCaptured || !xT1.m_bCaptured || !xT2.m_bCaptured) return true;

	uint32_t uAlternations = 0;
	for (uint32_t uChunkIdx = 0; uChunkIdx < TOTAL_CHUNKS; uChunkIdx++)
	{
		for (uint32_t uLOD = 0; uLOD < LOD_COUNT; uLOD++)
		{
			const uint8_t e0 = xT0.m_aeStates[uChunkIdx][uLOD];
			const uint8_t e1 = xT1.m_aeStates[uChunkIdx][uLOD];
			const uint8_t e2 = xT2.m_aeStates[uChunkIdx][uLOD];
			if (e0 == e2 && e0 != e1)
			{
				if (uAlternations < 4)
				{
					Zenith_Error(LOG_CATEGORY_TERRAIN,
						"RENDERTEST_SMOKE_FAIL: terrain[%u] chunk %u LOD %u residency alternation T0=%u T1=%u T2=%u",
						uTerrainIndex, uChunkIdx, uLOD, e0, e1, e2);
				}
				uAlternations++;
			}
		}
	}
	if (uAlternations >= 4)
	{
		Zenith_Error(LOG_CATEGORY_TERRAIN,
			"RENDERTEST_SMOKE_FAIL: terrain[%u] has %u total residency alternations (4 logged above)",
			uTerrainIndex, uAlternations);
	}
	return uAlternations == 0;
}

static bool RenderTest_LogTerrainSmokeState(uint32_t uFrame)
{
	Zenith_Vector<Zenith_TerrainComponent*> xTerrains;
	Zenith_SceneManager::GetAllOfComponentTypeFromAllScenes<Zenith_TerrainComponent>(xTerrains);

	bool bPass = true;
	const bool bStreamingWarm = uFrame >= 60;
	// Probe A threshold: by frame 120 the streaming loop should have settled —
	// no streams/evictions per frame on a stationary camera. Tighter than the
	// "warm" threshold for the existing checks because the steady-state is the
	// actual signal we're asserting (not just "anything HIGH-resident yet").
	const bool bSteadyStateRequired = uFrame >= 120;
	Zenith_Log(LOG_CATEGORY_TERRAIN, "[RenderTestSmoke] frame=%u terrainCount=%u", uFrame, xTerrains.GetSize());
	if (xTerrains.GetSize() == 0)
	{
		Zenith_Error(LOG_CATEGORY_TERRAIN, "RENDERTEST_SMOKE_FAIL: no terrain components found");
		return false;
	}

	for (u_int u = 0; u < xTerrains.GetSize(); u++)
	{
		Zenith_TerrainComponent* pxTerrain = xTerrains.Get(u);
		Flux_TerrainStreamingState* pxState = Flux_TerrainStreamingManager::GetStateFor(pxTerrain);

		const uint32_t uActiveCount = pxState ? static_cast<uint32_t>(pxState->m_xActiveChunkIndices.size()) : 0;
		const uint32_t uHighResident = pxState ? RenderTest_CountHighResidentChunks(*pxState) : 0;
		const uint32_t uLowZero = pxState ? RenderTest_CountLowZeroChunks(*pxState) : TOTAL_CHUNKS;

		Zenith_Log(LOG_CATEGORY_TERRAIN,
			"[RenderTestSmoke] terrain[%u]=%p usable=%u culling=%u state=%p active=%u LOWZero=%u HIGHResident=%u streamsThisFrame=%u evictionsThisFrame=%u",
			u,
			static_cast<void*>(pxTerrain),
			pxTerrain->IsRenderGeometryUsable() ? 1u : 0u,
			pxTerrain->m_bCullingResourcesInitialized ? 1u : 0u,
			static_cast<void*>(pxState),
			uActiveCount,
			uLowZero,
			uHighResident,
			pxState ? pxState->m_xStats.m_uStreamsThisFrame : 0u,
			pxState ? pxState->m_xStats.m_uEvictionsThisFrame : 0u);

		if (!pxTerrain->IsRenderGeometryUsable())
		{
			Zenith_Error(LOG_CATEGORY_TERRAIN, "RENDERTEST_SMOKE_FAIL: terrain[%u] render geometry unusable", u);
			bPass = false;
		}
		if (!pxTerrain->m_bCullingResourcesInitialized)
		{
			Zenith_Error(LOG_CATEGORY_TERRAIN, "RENDERTEST_SMOKE_FAIL: terrain[%u] culling resources not initialized", u);
			bPass = false;
		}
		if (!pxState)
		{
			Zenith_Error(LOG_CATEGORY_TERRAIN, "RENDERTEST_SMOKE_FAIL: terrain[%u] has no registered streaming state", u);
			bPass = false;
		}
		else if (uLowZero > 0)
		{
			Zenith_Error(LOG_CATEGORY_TERRAIN, "RENDERTEST_SMOKE_FAIL: terrain[%u] has %u LOW zero-count chunks", u, uLowZero);
			bPass = false;
		}
		else if (bStreamingWarm && uActiveCount == 0)
		{
			Zenith_Error(LOG_CATEGORY_TERRAIN, "RENDERTEST_SMOKE_FAIL: terrain[%u] active streaming set is still empty at warm frame %u", u, uFrame);
			bPass = false;
		}
		else if (bStreamingWarm && uHighResident == 0)
		{
			Zenith_Error(LOG_CATEGORY_TERRAIN, "RENDERTEST_SMOKE_FAIL: terrain[%u] has no HIGH resident chunks at warm frame %u", u, uFrame);
			bPass = false;
		}

		if (pxState != nullptr)
		{
			// Probe A: steady-state streaming check. After frame 120 the camera
			// has been stationary long enough for the streaming loop to settle —
			// any residual stream-in / eviction per frame is a streaming-loop
			// instability that would surface upstream as LOD red/green flicker.
			if (bSteadyStateRequired)
			{
				if (pxState->m_xStats.m_uStreamsThisFrame != 0 ||
					pxState->m_xStats.m_uEvictionsThisFrame != 0)
				{
					Zenith_Error(LOG_CATEGORY_TERRAIN,
						"RENDERTEST_SMOKE_FAIL: terrain[%u] frame %u not at steady state (streams=%u, evictions=%u)",
						u, uFrame, pxState->m_xStats.m_uStreamsThisFrame, pxState->m_xStats.m_uEvictionsThisFrame);
					bPass = false;
				}
			}

			// Probe B: residency allocation range validation.
			if (pxTerrain->m_bCullingResourcesInitialized)
			{
				if (!RenderTest_ValidateResidencyAllocationRanges(*pxTerrain, *pxState, u, uFrame))
				{
					bPass = false;
				}
			}
		}
	}

	return bPass;
}

class RenderTest_SmokeRunner : public Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME_INTERNAL(RenderTest_SmokeRunner)

	RenderTest_SmokeRunner() = delete;
	RenderTest_SmokeRunner(Zenith_Entity& xParentEntity)
	{
		m_xParentEntity = xParentEntity;
	}

	void OnStart() override
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN,
			"[RenderTestSmoke] starting bounded terrain smoke run frameLimit=%u",
			s_uRenderTestSmokeFrameLimit);
	}

	void OnUpdate(float) override
	{
		m_uFrame++;

		if (m_uFrame == 1 || (m_uFrame % 60) == 0 || m_uFrame >= s_uRenderTestSmokeFrameLimit)
		{
			m_bPassed = RenderTest_LogTerrainSmokeState(m_uFrame) && m_bPassed;
		}

		// Probe C: residency-snapshot alternation detection.
		// Capture residency at three steady-state checkpoints. If any chunk's
		// residency follows the alternation pattern (T0 == T2 != T1) the
		// streaming loop is oscillating in a way that would manifest as the
		// user-visible LOD red/green flicker. Snapshots are scheduled at
		// frames 130 / 150 / 200 — well past the steady-state warm-up at 120.
		if (m_uFrame == 130) CaptureResidencySnapshots(m_axSnapshotT0);
		if (m_uFrame == 150) CaptureResidencySnapshots(m_axSnapshotT1);
		if (m_uFrame == 200)
		{
			CaptureResidencySnapshots(m_axSnapshotT2);
			AssertNoAlternationAcrossSnapshots();
		}

		if (m_uFrame >= s_uRenderTestSmokeFrameLimit)
		{
			if (m_bPassed)
			{
				Zenith_Log(LOG_CATEGORY_TERRAIN, "RENDERTEST_SMOKE_PASS: completed %u frames", m_uFrame);
			}
			else
			{
				Zenith_Error(LOG_CATEGORY_TERRAIN, "RENDERTEST_SMOKE_FAIL: completed %u frames with terrain diagnostics failing", m_uFrame);
			}
			RenderTest_RequestClose();
		}
	}

private:
	void CaptureResidencySnapshots(Zenith_Vector<RenderTest_ResidencySnapshot>& axSnapshots)
	{
		Zenith_Vector<Zenith_TerrainComponent*> xTerrains;
		Zenith_SceneManager::GetAllOfComponentTypeFromAllScenes<Zenith_TerrainComponent>(xTerrains);
		axSnapshots.Clear();
		for (u_int u = 0; u < xTerrains.GetSize(); u++)
		{
			Flux_TerrainStreamingState* pxState =
				Flux_TerrainStreamingManager::GetStateFor(xTerrains.Get(u));
			RenderTest_ResidencySnapshot xSnapshot;
			if (pxState != nullptr) RenderTest_CaptureResidencySnapshot(*pxState, xSnapshot);
			axSnapshots.PushBack(xSnapshot);
		}
	}

	void AssertNoAlternationAcrossSnapshots()
	{
		// Compare per-terrain across all three snapshots. Mismatched sizes
		// would mean a terrain came/went between captures — treat as failure
		// because Probe C assumes a stable terrain set.
		const u_int uCount = m_axSnapshotT0.GetSize();
		if (m_axSnapshotT1.GetSize() != uCount || m_axSnapshotT2.GetSize() != uCount)
		{
			Zenith_Error(LOG_CATEGORY_TERRAIN,
				"RENDERTEST_SMOKE_FAIL: residency snapshot terrain count mismatch (T0=%u T1=%u T2=%u)",
				uCount, m_axSnapshotT1.GetSize(), m_axSnapshotT2.GetSize());
			m_bPassed = false;
			return;
		}
		for (u_int u = 0; u < uCount; u++)
		{
			if (!RenderTest_AssertNoResidencyAlternation(m_axSnapshotT0.Get(u),
														 m_axSnapshotT1.Get(u),
														 m_axSnapshotT2.Get(u),
														 u))
			{
				m_bPassed = false;
			}
		}
	}

	uint32_t m_uFrame = 0;
	bool m_bPassed = true;
	Zenith_Vector<RenderTest_ResidencySnapshot> m_axSnapshotT0;
	Zenith_Vector<RenderTest_ResidencySnapshot> m_axSnapshotT1;
	Zenith_Vector<RenderTest_ResidencySnapshot> m_axSnapshotT2;
};

#ifdef ZENITH_TOOLS
static void ExportSolidTexture(
	const std::string& strPath,
	uint8_t uR, uint8_t uG, uint8_t uB, uint8_t uA,
	uint32_t uWidth, uint32_t uHeight)
{
	std::vector<uint8_t> xPixelData(uWidth * uHeight * 4);
	for (uint32_t u = 0; u < uWidth * uHeight; u++)
	{
		xPixelData[u * 4 + 0] = uR;
		xPixelData[u * 4 + 1] = uG;
		xPixelData[u * 4 + 2] = uB;
		xPixelData[u * 4 + 3] = uA;
	}

	Zenith_DataStream xStream;
	xStream << static_cast<int32_t>(uWidth);
	xStream << static_cast<int32_t>(uHeight);
	xStream << static_cast<int32_t>(1);
	xStream << static_cast<TextureFormat>(TEXTURE_FORMAT_RGBA8_UNORM);
	xStream << static_cast<size_t>(xPixelData.size());
	xStream.WriteData(xPixelData.data(), xPixelData.size());
	xStream.WriteToFile(strPath.c_str());
}

static void ExportMaterialTextures(
	const std::string& strDir,
	const std::string& strName,
	uint8_t uDiffR, uint8_t uDiffG, uint8_t uDiffB,
	uint8_t uRoughness, uint8_t uMetallic)
{
	ExportSolidTexture(strDir + strName + "_Diffuse" ZENITH_TEXTURE_EXT, uDiffR, uDiffG, uDiffB, 255, 4, 4);
	ExportSolidTexture(strDir + strName + "_Normal" ZENITH_TEXTURE_EXT, 128, 128, 255, 255, 4, 4);
	ExportSolidTexture(strDir + strName + "_RM" ZENITH_TEXTURE_EXT, 0, uRoughness, uMetallic, 255, 4, 4);
	ExportSolidTexture(strDir + strName + "_Occlusion" ZENITH_TEXTURE_EXT, 255, 255, 255, 255, 4, 4);
	ExportSolidTexture(strDir + strName + "_Emissive" ZENITH_TEXTURE_EXT, 0, 0, 0, 255, 4, 4);
}

struct RenderTestHeightmapGenData
{
	cv::Mat* pxHeightmap = nullptr;
	uint32_t uSize = 0;
	float fTerrainWorldSize = 0.0f;
};

static void GenerateHeightmapRowsTask(void* pData, u_int uInvocationIndex, u_int uNumInvocations)
{
	RenderTestHeightmapGenData* pxData = static_cast<RenderTestHeightmapGenData*>(pData);
	const uint32_t uSize = pxData->uSize;
	const float fTerrainWorldSize = pxData->fTerrainWorldSize;
	const float fSizeMinusOne = static_cast<float>(uSize - 1);
	const float fMaxProceduralHeight = 100.0f;

	const u_int uRowsPerInvocation = (uSize + uNumInvocations - 1) / uNumInvocations;
	const u_int uStartRow = uInvocationIndex * uRowsPerInvocation;
	const u_int uEndRow = std::min(uStartRow + uRowsPerInvocation, static_cast<u_int>(uSize));

	for (uint32_t y = uStartRow; y < uEndRow; y++)
	{
		float* pfRow = pxData->pxHeightmap->ptr<float>(y);
		const float fWorldZ = (static_cast<float>(y) / fSizeMinusOne) * fTerrainWorldSize - fTerrainWorldSize * 0.5f;

		for (uint32_t x = 0; x < uSize; x++)
		{
			const float fWorldX = (static_cast<float>(x) / fSizeMinusOne) * fTerrainWorldSize - fTerrainWorldSize * 0.5f;

			float fHeight = 0.0f;
			fHeight += std::sin(fWorldX * 0.0013f) * std::cos(fWorldZ * 0.0011f) * 42.0f;
			fHeight += std::sin(fWorldX * 0.0047f + 1.1f) * std::cos(fWorldZ * 0.0041f + 0.8f) * 24.0f;
			fHeight += std::sin((fWorldX + fWorldZ) * 0.014f) * 7.0f;
			fHeight += 32.0f;

			pfRow[x] = std::clamp(fHeight / fMaxProceduralHeight, 0.0f, 1.0f);
		}
	}
}

static cv::Mat GenerateProceduralHeightmap(uint32_t uSize, float fTerrainWorldSize)
{
	cv::Mat xHeightmap(uSize, uSize, CV_32FC1);

	RenderTestHeightmapGenData xData;
	xData.pxHeightmap = &xHeightmap;
	xData.uSize = uSize;
	xData.fTerrainWorldSize = fTerrainWorldSize;

	const u_int uNumInvocations = std::min(static_cast<u_int>(64), uSize);
	Zenith_TaskArray xTask(ZENITH_PROFILE_INDEX__FLUX_TERRAIN, GenerateHeightmapRowsTask, &xData, uNumInvocations, true);
	Zenith_TaskSystem::SubmitTaskArray(&xTask);
	xTask.WaitUntilComplete();

	cv::flip(xHeightmap, xHeightmap, 0);
	return xHeightmap;
}

static void GenerateProceduralSplatmap(const cv::Mat& xHeightmap, const std::string& strOutputPath)
{
	const int iWidth = xHeightmap.cols;
	const int iHeight = xHeightmap.rows;
	std::vector<uint8_t> xPixelData(static_cast<size_t>(iWidth) * static_cast<size_t>(iHeight) * 4);

	double dMin = 0.0;
	double dMax = 1.0;
	cv::minMaxLoc(xHeightmap, &dMin, &dMax);
	const double dRange = (dMax - dMin > 0.0001) ? (dMax - dMin) : 1.0;

	for (int y = 0; y < iHeight; y++)
	{
		const float* pfRow = xHeightmap.ptr<float>(y);
		for (int x = 0; x < iWidth; x++)
		{
			const float fHeight = static_cast<float>((pfRow[x] - dMin) / dRange);
			const float fBand = static_cast<float>(x) / static_cast<float>(std::max(1, iWidth - 1));

			float fGrass = std::clamp(1.0f - std::abs(fHeight - 0.35f) * 2.4f, 0.08f, 1.0f);
			float fRock = std::clamp((fHeight - 0.52f) * 2.8f, 0.0f, 1.0f);
			float fDirt = std::clamp(1.0f - std::abs(fHeight - 0.18f) * 4.0f, 0.0f, 1.0f);
			float fSand = std::clamp((0.28f - fHeight) * 4.0f, 0.0f, 1.0f);

			// Add a broad lateral material sweep so the four terrain slots are easy to inspect.
			fRock += std::clamp((fBand - 0.65f) * 1.5f, 0.0f, 0.35f);
			fDirt += std::clamp((0.35f - fBand) * 1.5f, 0.0f, 0.35f);

			const float fTotal = std::max(0.0001f, fGrass + fRock + fDirt + fSand);
			const uint32_t uIdx = static_cast<uint32_t>((y * iWidth + x) * 4);
			xPixelData[uIdx + 0] = static_cast<uint8_t>((fGrass / fTotal) * 255.0f + 0.5f);
			xPixelData[uIdx + 1] = static_cast<uint8_t>((fRock / fTotal) * 255.0f + 0.5f);
			xPixelData[uIdx + 2] = static_cast<uint8_t>((fDirt / fTotal) * 255.0f + 0.5f);
			xPixelData[uIdx + 3] = static_cast<uint8_t>((fSand / fTotal) * 255.0f + 0.5f);
		}
	}

	Zenith_DataStream xStream;
	xStream << static_cast<int32_t>(iWidth);
	xStream << static_cast<int32_t>(iHeight);
	xStream << static_cast<int32_t>(1);
	xStream << static_cast<TextureFormat>(TEXTURE_FORMAT_RGBA8_UNORM);
	xStream << static_cast<size_t>(xPixelData.size());
	xStream.WriteData(xPixelData.data(), xPixelData.size());
	xStream.WriteToFile(strOutputPath.c_str());
}

static bool GenerateAndExportTerrain()
{
	using namespace Flux_TerrainConfig;

	const std::string strTerrainDir = std::string(GAME_ASSETS_DIR) + "Terrain/";
	const std::string strTexturesDir = strTerrainDir + "Textures/";
	const std::string strFirstLowChunk = strTerrainDir + "Render_LOW_0_0" ZENITH_MESH_EXT;

	if (!RenderTest_HasCommandLineFlag("--rendertest-force-regenerate") && std::filesystem::exists(strFirstLowChunk))
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "[RenderTest] Existing terrain chunks found; skipping generation");
		return true;
	}

	std::filesystem::create_directories(strTerrainDir);
	std::filesystem::create_directories(strTexturesDir);

	Zenith_Log(LOG_CATEGORY_TERRAIN, "[RenderTest] Generating procedural terrain assets...");

	cv::Mat xHeightmap = GenerateProceduralHeightmap(4096, TERRAIN_SIZE);
	const std::string strHeightmapPath = strTerrainDir + "RenderTestHeightmap.tif";
	if (!cv::imwrite(strHeightmapPath, xHeightmap))
	{
		Zenith_Error(LOG_CATEGORY_TERRAIN, "[RenderTest] Failed to save generated heightmap: %s", strHeightmapPath.c_str());
		return false;
	}

	ExportHeightmapFromMat(xHeightmap, strTerrainDir);

	ExportMaterialTextures(strTexturesDir, "Grass", 72, 132, 48, 225, 0);
	ExportMaterialTextures(strTexturesDir, "Rock", 118, 112, 106, 190, 0);
	ExportMaterialTextures(strTexturesDir, "Dirt", 122, 88, 58, 235, 0);
	ExportMaterialTextures(strTexturesDir, "Sand", 196, 184, 136, 240, 0);
	GenerateProceduralSplatmap(xHeightmap, strTerrainDir + "Splatmap" ZENITH_TEXTURE_EXT);

	Zenith_Log(LOG_CATEGORY_TERRAIN, "[RenderTest] Terrain asset generation complete");
	return true;
}
#endif

static void SetMaterialTexturePaths(Zenith_MaterialAsset* pxMaterial, const std::string& strDir, const std::string& strName)
{
	pxMaterial->SetDiffuseTexturePath(strDir + strName + "_Diffuse" ZENITH_TEXTURE_EXT);
	pxMaterial->SetNormalTexturePath(strDir + strName + "_Normal" ZENITH_TEXTURE_EXT);
	pxMaterial->SetRoughnessMetallicTexturePath(strDir + strName + "_RM" ZENITH_TEXTURE_EXT);
	pxMaterial->SetOcclusionTexturePath(strDir + strName + "_Occlusion" ZENITH_TEXTURE_EXT);
	pxMaterial->SetEmissiveTexturePath(strDir + strName + "_Emissive" ZENITH_TEXTURE_EXT);
}

static void InitializeRenderTestResources()
{
	if (s_bResourcesInitialized)
		return;

	const std::string strTexturesDir = std::string(GAME_ASSETS_DIR) + "Terrain/Textures/";
	static const char* aszTextureNames[] = { "Grass", "Rock", "Dirt", "Sand" };
	static const char* aszMaterialNames[] = {
		"RenderTestTerrainGrass",
		"RenderTestTerrainRock",
		"RenderTestTerrainDirt",
		"RenderTestTerrainSand"
	};

	Zenith_AssetRegistry& xRegistry = Zenith_AssetRegistry::Get();
	for (u_int u = 0; u < 4; u++)
	{
		RenderTest::g_axTerrainMaterials[u].Set(xRegistry.Create<Zenith_MaterialAsset>());
		RenderTest::g_axTerrainMaterials[u].GetDirect()->SetName(aszMaterialNames[u]);
		SetMaterialTexturePaths(RenderTest::g_axTerrainMaterials[u].GetDirect(), strTexturesDir, aszTextureNames[u]);
	}

	s_bResourcesInitialized = true;
}

static bool EnsureTerrainAssets()
{
#ifdef ZENITH_TOOLS
	return GenerateAndExportTerrain();
#else
	const std::string strFirstLowChunk = std::string(GAME_ASSETS_DIR) + "Terrain/Render_LOW_0_0" ZENITH_MESH_EXT;
	if (std::filesystem::exists(strFirstLowChunk))
		return true;

	Zenith_Warning(LOG_CATEGORY_TERRAIN,
		"[RenderTest] No terrain chunks found at %s. Run a tools build once to generate RenderTest terrain assets.",
		strFirstLowChunk.c_str());
	return false;
#endif
}

static void CreateRenderTestScene()
{
	using namespace Flux_TerrainConfig;

	static bool s_bSceneContentCreated = false;
	s_uRenderTestSmokeFrameLimit = RenderTest_GetCommandLineUInt("--rendertest-smoke-frames=", 240);

	Zenith_Scene xScene = Zenith_SceneManager::GetSceneByName("RenderTest");
	if (!xScene.IsValid())
	{
		xScene = Zenith_SceneManager::CreateScene("RenderTest");
	}

	if (!xScene.IsValid())
	{
		Zenith_Error(LOG_CATEGORY_SCENE, "[RenderTest] Failed to create RenderTest scene");
		return;
	}

	Zenith_SceneManager::SetActiveScene(xScene);
	if (s_bSceneContentCreated)
		return;

	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xScene);
	if (!pxSceneData)
	{
		Zenith_Error(LOG_CATEGORY_SCENE, "[RenderTest] RenderTest scene has no scene data");
		return;
	}

	Zenith_Entity xCamera(pxSceneData, "RenderTestCamera");
	xCamera.SetTransient(false);

	Zenith_CameraComponent& xCameraComponent = xCamera.AddComponent<Zenith_CameraComponent>();
	Zenith_CameraComponent::PerspectiveParams xCameraParams;
	xCameraParams.m_xPosition = Zenith_Maths::Vector3(TERRAIN_SIZE * 0.5f, 1200.0f, TERRAIN_SIZE * 0.5f);
	xCameraParams.m_fPitch = -0.65f;
	xCameraParams.m_fYaw = 0.0f;
	xCameraParams.m_fFOV = glm::radians(70.0f);
	xCameraParams.m_fNear = 0.1f;
	xCameraParams.m_fFar = 10000.0f;
	xCameraComponent.InitialisePerspective(xCameraParams);
	pxSceneData->SetMainCameraEntity(xCamera.GetEntityID());

	if (!EnsureTerrainAssets())
	{
		Zenith_Warning(LOG_CATEGORY_TERRAIN, "[RenderTest] Terrain assets unavailable; scene contains only the camera");
		return;
	}

	InitializeRenderTestResources();

	Zenith_Entity xTerrainEntity(pxSceneData, "RenderTestTerrain");
	xTerrainEntity.SetTransient(false);

	Zenith_TerrainComponent& xTerrain = xTerrainEntity.AddComponent<Zenith_TerrainComponent>(
		*RenderTest::g_axTerrainMaterials[0].GetDirect(),
		*RenderTest::g_axTerrainMaterials[1].GetDirect());
	xTerrain.GetMaterialHandle(2).Set(RenderTest::g_axTerrainMaterials[2].GetDirect());
	xTerrain.GetMaterialHandle(3).Set(RenderTest::g_axTerrainMaterials[3].GetDirect());
	xTerrain.GetSplatmapHandle().SetPath("game:Terrain/Splatmap" ZENITH_TEXTURE_EXT);

	if (RenderTest_IsSmokeMode())
	{
		if (RenderTest_HasCommandLineFlag("--rendertest-lod-debug"))
			Flux_Terrain::GetDebugMode() = 1;
		if (RenderTest_HasCommandLineFlag("--rendertest-wireframe"))
			Flux_Terrain::GetWireframeMode() = true;

		Zenith_Entity xSmokeEntity(pxSceneData, "RenderTestSmokeRunner");
		xSmokeEntity.SetTransient(false);
		Zenith_ScriptComponent& xScript = xSmokeEntity.AddComponent<Zenith_ScriptComponent>();
		xScript.AddScript<RenderTest_SmokeRunner>();

#ifdef ZENITH_TOOLS
		Zenith_Editor::SetEditorMode(EditorMode::Playing);
#endif
	}

	Zenith_Log(LOG_CATEGORY_TERRAIN, "[RenderTest] Procedural terrain scene created");
	s_bSceneContentCreated = true;
}

const char* Project_GetName()
{
	return "RenderTest";
}

const char* Project_GetGameAssetsDirectory()
{
	return GAME_ASSETS_DIR;
}

const char* Project_GetGameAssetsDir()
{
	return GAME_ASSETS_DIR;
}

void Project_SetGraphicsOptions(Zenith_GraphicsOptions&)
{
}

void Project_RegisterScriptBehaviours()
{
	InitializeRenderTestResources();
}

void Project_Shutdown()
{
}

void Project_LoadInitialScene();

#ifdef ZENITH_TOOLS
void Project_InitializeResources()
{
	InitializeRenderTestResources();
}

void Project_RegisterEditorAutomationSteps()
{
	Zenith_EditorAutomation::AddStep_LoadInitialScene(&Project_LoadInitialScene);
}
#endif

void Project_LoadInitialScene()
{
	CreateRenderTestScene();
}
