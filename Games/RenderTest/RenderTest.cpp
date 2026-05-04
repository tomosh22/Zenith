#include "Zenith.h"

#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "DataStream/Zenith_DataStream.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_AnimatorComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Flux/Terrain/Flux_Terrain.h"
#include "Flux/Terrain/Flux_TerrainStreamingManager.h"
#include "Flux/Terrain/Flux_TerrainConfig.h"
#include "Zenith_OS_Include.h"

// Header-only behaviours: must be included into a compiled TU so their
// ZENITH_BEHAVIOUR_TYPE_NAME auto-registers the factory at startup.
#include "RenderTest/Components/RenderTest_PlayerBehaviour.h"
#include "RenderTest/Components/RenderTest_FollowCamera.h"

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
extern void ExportHeightmapFromPaths(const std::string& strHeightmapPath, const std::string& strOutputDir);
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

	// Cube primitive — exported as a .zmodel so it survives scene save/load.
	ModelHandle g_xCubeModelAsset;
	std::string g_strCubeModelPath;

	// StickFigure model + materials for the player and cube.
	ModelHandle g_xStickFigureModelAsset;
	std::string g_strStickFigureModelPath;

	MaterialHandle g_xCubeMaterial;
	MaterialHandle g_xPlayerMaterial;
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
	ZENITH_BEHAVIOUR_TYPE_NAME(RenderTest_SmokeRunner)

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

// Write a 1x1 colored .ztxtr file and return a TextureHandle pointing at it.
// Mirrors Combat::ExportColoredTexture so the procedural diffuse texture survives
// scene save/load via its disk path.
static TextureHandle ExportColoredTexture(const std::string& strPath, uint8_t uR, uint8_t uG, uint8_t uB)
{
	uint8_t aucPixelData[] = { uR, uG, uB, 255 };

	Zenith_DataStream xStream;
	xStream << (int32_t)1;
	xStream << (int32_t)1;
	xStream << (int32_t)1;
	xStream << (TextureFormat)TEXTURE_FORMAT_RGBA8_UNORM;
	xStream << (size_t)4;
	xStream.WriteData(aucPixelData, 4);
	xStream.WriteToFile(strPath.c_str());

	std::string strRelativePath = Zenith_AssetRegistry::MakeRelativePath(strPath);
	if (strRelativePath.empty())
	{
		Zenith_Error(LOG_CATEGORY_ASSET, "[RenderTest] Failed to make relative path for texture: %s", strPath.c_str());
		return TextureHandle();
	}

	return TextureHandle(strRelativePath);
}

static MaterialHandle CreateFlatColorMaterial(const std::string& strMaterialName,
	const std::string& strTexturePath,
	uint8_t uR, uint8_t uG, uint8_t uB)
{
	Zenith_MaterialAsset* pxMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	pxMaterial->SetName(strMaterialName);
	pxMaterial->SetDiffuseTexture(ExportColoredTexture(strTexturePath, uR, uG, uB));

	MaterialHandle xHandle;
	xHandle.Set(pxMaterial);
	return xHandle;
}

// Build a unit cube .zasset (mesh) + .zmodel (model bundle) on disk if they
// don't already exist, and store their paths/handles in the RenderTest namespace.
// The .zmodel is what AddStep_LoadModel references in the saved scene; the
// scene file persists the model path, so the runtime scene loader resolves the
// model through Zenith_AssetRegistry the same way Combat resolves StickFigure.
// Generation happens only in tools builds — non-tools assumes the assets were
// produced by a previous tools-build run.
static void EnsureUnitCubeModelExists()
{
	if (!RenderTest::g_strCubeModelPath.empty())
		return;

	const std::string strMeshAssetPath = std::string(GAME_ASSETS_DIR) + "Meshes/UnitCube" ZENITH_MESH_ASSET_EXT;
	const std::string strModelPath     = std::string(GAME_ASSETS_DIR) + "Meshes/UnitCube" ZENITH_MODEL_EXT;

#ifdef ZENITH_TOOLS
	std::filesystem::create_directories(std::filesystem::path(strMeshAssetPath).parent_path());

	if (!std::filesystem::exists(strMeshAssetPath))
	{
		Zenith_MeshAsset xCubeMesh;
		Zenith_MeshAsset::GenerateUnitCube(xCubeMesh);
		xCubeMesh.Export(strMeshAssetPath.c_str());
	}

	if (!std::filesystem::exists(strModelPath))
	{
		Zenith_ModelAsset* pxModel = Zenith_AssetRegistry::Create<Zenith_ModelAsset>();
		pxModel->SetName("UnitCube");
		Zenith_Vector<std::string> xEmptyMaterials;
		pxModel->AddMeshByPath(strMeshAssetPath, xEmptyMaterials);
		pxModel->Export(strModelPath.c_str());
		RenderTest::g_xCubeModelAsset.Set(pxModel);
	}
#else
	if (!std::filesystem::exists(strModelPath))
	{
		Zenith_Warning(LOG_CATEGORY_MESH,
			"[RenderTest] UnitCube model missing at %s — run a tools build once to generate it",
			strModelPath.c_str());
		return;
	}
#endif

	RenderTest::g_strCubeModelPath = strModelPath;
}

// Mirrors Combat::TryInitializeStickFigureModel — bundle the StickFigure mesh
// asset + skeleton at ENGINE_ASSETS_DIR/Meshes/StickFigure into a .zmodel on
// disk for AddStep_LoadModel to reference. Idempotent: skips export when the
// .zmodel already exists. Non-tools builds assume the .zmodel was produced by
// a previous tools run.
static void EnsureStickFigureModelExists()
{
	if (!RenderTest::g_strStickFigureModelPath.empty())
		return;

	const std::string strMeshAssetPath = std::string(ENGINE_ASSETS_DIR) + "Meshes/StickFigure/StickFigure" ZENITH_MESH_ASSET_EXT;
	const std::string strSkeletonPath  = std::string(ENGINE_ASSETS_DIR) + "Meshes/StickFigure/StickFigure" ZENITH_SKELETON_EXT;
	const std::string strModelPath     = std::string(ENGINE_ASSETS_DIR) + "Meshes/StickFigure/StickFigure" ZENITH_MODEL_EXT;

#ifdef ZENITH_TOOLS
	if (!std::filesystem::exists(strMeshAssetPath) || !std::filesystem::exists(strSkeletonPath))
	{
		Zenith_Warning(LOG_CATEGORY_MESH,
			"[RenderTest] StickFigure source assets missing — player will fail to load model. Expected: %s, %s",
			strMeshAssetPath.c_str(), strSkeletonPath.c_str());
		return;
	}

	if (!std::filesystem::exists(strModelPath))
	{
		Zenith_ModelAsset* pxModel = Zenith_AssetRegistry::Create<Zenith_ModelAsset>();
		pxModel->SetName("StickFigure");
		pxModel->SetSkeletonPath(strSkeletonPath);

		Zenith_Vector<std::string> xEmptyMaterials;
		pxModel->AddMeshByPath(strMeshAssetPath, xEmptyMaterials);
		pxModel->Export(strModelPath.c_str());
		RenderTest::g_xStickFigureModelAsset.Set(pxModel);
	}
#else
	if (!std::filesystem::exists(strModelPath))
	{
		Zenith_Warning(LOG_CATEGORY_MESH,
			"[RenderTest] StickFigure model missing at %s — run a tools build once to generate it",
			strModelPath.c_str());
		return;
	}
#endif

	RenderTest::g_strStickFigureModelPath = strModelPath;
}

// Build a terrain material from one of the user-provided PBR texture sets at
// `Assets/Textures/Terrain/<Name>/`. Each set has diffuse / normal / ao /
// roughness / metallic / height / gloss / reflection .ztxtr files; we wire up
// the four slots the engine's MaterialAsset surfaces. The roughness texture is
// plugged into the "RoughnessMetallic" slot since the engine expects a single
// packed RM texture and the user supplied roughness/metallic separately —
// using just roughness is a reasonable visual approximation; metallic stays at
// the engine default. Emissive is left unset (default black).
static void SetupPBRTerrainMaterial(MaterialHandle& xHandle, const std::string& strDisplayName, const std::string& strRelativeDir)
{
	xHandle.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	Zenith_MaterialAsset* pxMaterial = xHandle.GetDirect();
	pxMaterial->SetName(strDisplayName);
	pxMaterial->SetDiffuseTexture          (TextureHandle(strRelativeDir + "diffuse"   ZENITH_TEXTURE_EXT));
	pxMaterial->SetNormalTexture           (TextureHandle(strRelativeDir + "normal"    ZENITH_TEXTURE_EXT));
	pxMaterial->SetRoughnessMetallicTexture(TextureHandle(strRelativeDir + "rm_packed" ZENITH_TEXTURE_EXT));
	pxMaterial->SetOcclusionTexture        (TextureHandle(strRelativeDir + "ao"        ZENITH_TEXTURE_EXT));

	// The terrain vertex UV (`a_xUV`) is heightmap pixel coordinates, scaled
	// by g_fUVScale (= 0.07) in the vertex shader, so input.uv ≈ [0, 286] across
	// the 4096-unit terrain. With the default tiling of 1.0 each PBR texture
	// then tiles ~286 times across the terrain (≈14m per tile) which reads as
	// a fine grid pattern under typical viewing angles. Scaling the per-
	// material tiling down brings the on-screen tile size up to something
	// that reads as ground texture rather than a moiré. 0.05 → texture tiles
	// ~14× across the terrain (~290m per tile), close enough to the native
	// ~40m physical scale of the supplied PBR set without going so coarse
	// that the texture detail is lost.
	pxMaterial->SetUVTiling(Zenith_Maths::Vector2(0.05f, 0.05f));
}

static void InitializeRenderTestResources()
{
	if (s_bResourcesInitialized)
		return;

	// User's two PBR material sets live under Assets/Textures/Terrain/<Name>/.
	// The terrain expects 4 material slots; the user's splatmap only blends 2,
	// so slots 2 and 3 are filled with copies — they get weight 0 from the
	// converted splatmap (B and A channels are zero) and never actually
	// contribute, but having them populated is cheaper and safer than letting
	// the terrain shader sample null materials.
	const std::string strGrassDir = std::string(GAME_ASSETS_DIR) + "Textures/Terrain/Grass/";
	const std::string strRockDir  = std::string(GAME_ASSETS_DIR) + "Textures/Terrain/Rock/";

	SetupPBRTerrainMaterial(RenderTest::g_axTerrainMaterials[0], "RenderTestTerrainGrass",  strGrassDir);
	SetupPBRTerrainMaterial(RenderTest::g_axTerrainMaterials[1], "RenderTestTerrainRock",   strRockDir);
	SetupPBRTerrainMaterial(RenderTest::g_axTerrainMaterials[2], "RenderTestTerrainGrass2", strGrassDir);
	SetupPBRTerrainMaterial(RenderTest::g_axTerrainMaterials[3], "RenderTestTerrainRock2",  strRockDir);

	const std::string strProceduralTexDir = std::string(GAME_ASSETS_DIR) + "Textures/";
	std::filesystem::create_directories(strProceduralTexDir);

	RenderTest::g_xCubeMaterial   = CreateFlatColorMaterial("RenderTestCubeMaterial",
		strProceduralTexDir + "CubeDiffuse" ZENITH_TEXTURE_EXT, 200, 200, 220);
	RenderTest::g_xPlayerMaterial = CreateFlatColorMaterial("RenderTestPlayerMaterial",
		strProceduralTexDir + "PlayerDiffuse" ZENITH_TEXTURE_EXT, 0, 200, 220);

	EnsureUnitCubeModelExists();
	EnsureStickFigureModelExists();

	s_bResourcesInitialized = true;
}

#ifdef ZENITH_TOOLS
// Drive terrain chunk generation from the user-supplied heightmap at
// Assets/Textures/Terrain/Height.ztxtr. ExportHeightmapFromPaths reads the
// .ztxtr (or .tif) and writes Render_*/Render_LOW_*/Physics_*.zmesh into
// Assets/Terrain/. Idempotent: skips regeneration when the chunks are at
// least as new as the source heightmap. The procedural fallback (in
// GenerateAndExportTerrain) is still wired up for the case where no user
// heightmap is provided — same shape, just slightly different inputs.
static void RenderTest_EnsureTerrainAssetsForAutomation()
{
	const std::string strUserHeightmap = std::string(GAME_ASSETS_DIR) + "Textures/Terrain/Height" ZENITH_TEXTURE_EXT;
	const std::string strChunksDir     = std::string(GAME_ASSETS_DIR) + "Terrain/";
	const std::string strFirstChunk    = strChunksDir + "Render_LOW_0_0" ZENITH_MESH_EXT;

	if (!std::filesystem::exists(strUserHeightmap))
	{
		// No user heightmap — fall back to the procedural path (which itself
		// no-ops when chunks are already on disk).
		if (!GenerateAndExportTerrain())
		{
			Zenith_Error(LOG_CATEGORY_TERRAIN,
				"[RenderTest] Terrain asset generation failed during automation — saved scene will be missing terrain meshes");
		}
		return;
	}

	const bool bForceRegen = RenderTest_HasCommandLineFlag("--rendertest-force-regenerate");
	bool bRegen = bForceRegen || !std::filesystem::exists(strFirstChunk);
	if (!bRegen)
	{
		const auto chunkTime  = std::filesystem::last_write_time(strFirstChunk);
		const auto sourceTime = std::filesystem::last_write_time(strUserHeightmap);
		bRegen = sourceTime > chunkTime;
	}

	if (!bRegen)
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "[RenderTest] Terrain chunks up to date with %s; skipping regeneration",
			strUserHeightmap.c_str());
		return;
	}

	std::filesystem::create_directories(strChunksDir);
	Zenith_Log(LOG_CATEGORY_TERRAIN, "[RenderTest] Regenerating terrain chunks from user heightmap %s",
		strUserHeightmap.c_str());
	ExportHeightmapFromPaths(strUserHeightmap, strChunksDir);
}

// Decode a single BC1 (DXT1) 4x4 block to 16 RGB888 pixels in row-major order.
// Layout: 2 bytes color0 (RGB565), 2 bytes color1 (RGB565), 4 bytes of 2-bit
// indices into a 4-entry palette derived from the two endpoints. This is just
// enough to extract the red channel of the user's grayscale roughness/metallic
// BC1 textures so they can be packed into an uncompressed RM texture below.
static void RenderTest_DecodeBC1Block(const uint8_t* pxBlock, uint8_t auOut[16][3])
{
	const uint16_t uC0 = static_cast<uint16_t>(pxBlock[0]) | static_cast<uint16_t>(pxBlock[1] << 8);
	const uint16_t uC1 = static_cast<uint16_t>(pxBlock[2]) | static_cast<uint16_t>(pxBlock[3] << 8);

	auto Expand565 = [](uint16_t uColor, uint8_t& uR, uint8_t& uG, uint8_t& uB)
	{
		const uint8_t u5R = static_cast<uint8_t>((uColor >> 11) & 0x1F);
		const uint8_t u6G = static_cast<uint8_t>((uColor >> 5)  & 0x3F);
		const uint8_t u5B = static_cast<uint8_t>(uColor         & 0x1F);
		// Bit-replicate to fill 8-bit space (matches D3D / glTF BC1 reference).
		uR = static_cast<uint8_t>((u5R << 3) | (u5R >> 2));
		uG = static_cast<uint8_t>((u6G << 2) | (u6G >> 4));
		uB = static_cast<uint8_t>((u5B << 3) | (u5B >> 2));
	};

	uint8_t auPal[4][3] = {};
	Expand565(uC0, auPal[0][0], auPal[0][1], auPal[0][2]);
	Expand565(uC1, auPal[1][0], auPal[1][1], auPal[1][2]);

	if (uC0 > uC1)
	{
		for (int c = 0; c < 3; ++c)
		{
			auPal[2][c] = static_cast<uint8_t>((2 * auPal[0][c] + auPal[1][c]) / 3);
			auPal[3][c] = static_cast<uint8_t>((auPal[0][c] + 2 * auPal[1][c]) / 3);
		}
	}
	else
	{
		for (int c = 0; c < 3; ++c)
		{
			auPal[2][c] = static_cast<uint8_t>((auPal[0][c] + auPal[1][c]) / 2);
			auPal[3][c] = 0;
		}
	}

	const uint32_t uIndices = static_cast<uint32_t>(pxBlock[4])
		| (static_cast<uint32_t>(pxBlock[5]) << 8)
		| (static_cast<uint32_t>(pxBlock[6]) << 16)
		| (static_cast<uint32_t>(pxBlock[7]) << 24);

	for (int i = 0; i < 16; ++i)
	{
		const int iEntry = (uIndices >> (i * 2)) & 0x3;
		auOut[i][0] = auPal[iEntry][0];
		auOut[i][1] = auPal[iEntry][1];
		auOut[i][2] = auPal[iEntry][2];
	}
}

// Decode a full BC1_RGB image into an RGB888 byte array. Width/height must be
// multiples of 4 (BC1 block size). The user's 2048x2048 textures satisfy that.
static std::vector<uint8_t> RenderTest_DecodeBC1Image(const uint8_t* pxBC1, int32_t iWidth, int32_t iHeight)
{
	std::vector<uint8_t> xResult(static_cast<size_t>(iWidth) * iHeight * 3);
	const int32_t iBlocksX = iWidth / 4;
	const int32_t iBlocksY = iHeight / 4;
	for (int32_t by = 0; by < iBlocksY; ++by)
	{
		for (int32_t bx = 0; bx < iBlocksX; ++bx)
		{
			uint8_t auPixels[16][3];
			RenderTest_DecodeBC1Block(pxBC1 + (static_cast<size_t>(by) * iBlocksX + bx) * 8, auPixels);
			for (int py = 0; py < 4; ++py)
			{
				for (int px = 0; px < 4; ++px)
				{
					const int32_t iX = bx * 4 + px;
					const int32_t iY = by * 4 + py;
					const size_t uSrc = static_cast<size_t>(py) * 4 + px;
					const size_t uDst = (static_cast<size_t>(iY) * iWidth + iX) * 3;
					xResult[uDst + 0] = auPixels[uSrc][0];
					xResult[uDst + 1] = auPixels[uSrc][1];
					xResult[uDst + 2] = auPixels[uSrc][2];
				}
			}
		}
	}
	return xResult;
}

// Read a .ztxtr header into out-params and return a pointer to the start of
// the pixel data. The DataStream is left positioned just after the header.
static bool RenderTest_ReadZtxtrHeader(Zenith_DataStream& xStream,
	int32_t& iWidthOut, int32_t& iHeightOut, int32_t& iDepthOut,
	TextureFormat& eFormatOut, size_t& ulDataSizeOut)
{
	xStream >> iWidthOut;
	xStream >> iHeightOut;
	xStream >> iDepthOut;
	xStream.ReadData(&eFormatOut, sizeof(eFormatOut));
	xStream >> ulDataSizeOut;
	return iWidthOut > 0 && iHeightOut > 0;
}

// Pack a roughness texture and a metallic texture (both BC1_RGB grayscale) into
// a single uncompressed RGBA8 texture where G = roughness and B = metallic —
// the channels the terrain shader samples (`xRM.gb` in Flux_Terrain_ToGBuffer).
// Output written to <strSourceDir>/rm_packed.ztxtr. Idempotent on file mtime.
static void RenderTest_PackRoughnessMetallic(const std::string& strSourceDir)
{
	const std::string strRoughnessPath = strSourceDir + "roughness" ZENITH_TEXTURE_EXT;
	const std::string strMetallicPath  = strSourceDir + "metallic"  ZENITH_TEXTURE_EXT;
	const std::string strOutputPath    = strSourceDir + "rm_packed" ZENITH_TEXTURE_EXT;

	if (!std::filesystem::exists(strRoughnessPath) || !std::filesystem::exists(strMetallicPath))
	{
		Zenith_Warning(LOG_CATEGORY_TERRAIN,
			"[RenderTest] Missing roughness/metallic in %s — skipping RM pack", strSourceDir.c_str());
		return;
	}

	// The engine touches source .ztxtr files when loading them through the
	// asset registry, which makes a naive mtime comparison fire every launch.
	// Skip on file presence instead — pass --rendertest-force-regenerate (or
	// delete rm_packed.ztxtr) to opt back in to a fresh pack when sources
	// genuinely change.
	if (std::filesystem::exists(strOutputPath) &&
		!RenderTest_HasCommandLineFlag("--rendertest-force-regenerate"))
	{
		return;
	}

	auto LoadAndDecodeBC1 = [](const std::string& strPath, int32_t& iWidthOut, int32_t& iHeightOut)
		-> std::vector<uint8_t>
	{
		Zenith_DataStream xStream;
		xStream.ReadFromFile(strPath.c_str());
		int32_t iDepth = 0;
		TextureFormat eFormat = TEXTURE_FORMAT_NONE;
		size_t ulDataSize = 0;
		if (!RenderTest_ReadZtxtrHeader(xStream, iWidthOut, iHeightOut, iDepth, eFormat, ulDataSize)
			|| eFormat != TEXTURE_FORMAT_BC1_RGB_UNORM)
		{
			Zenith_Error(LOG_CATEGORY_TERRAIN,
				"[RenderTest] %s is not BC1_RGB (got fmt=%d) — RM packer expects user PBR maps to be BC1.",
				strPath.c_str(), static_cast<int>(eFormat));
			return {};
		}
		std::vector<uint8_t> xBC1(ulDataSize);
		xStream.ReadData(xBC1.data(), ulDataSize);
		return RenderTest_DecodeBC1Image(xBC1.data(), iWidthOut, iHeightOut);
	};

	int32_t iRWidth = 0, iRHeight = 0;
	int32_t iMWidth = 0, iMHeight = 0;
	std::vector<uint8_t> xRoughRGB    = LoadAndDecodeBC1(strRoughnessPath, iRWidth, iRHeight);
	std::vector<uint8_t> xMetallicRGB = LoadAndDecodeBC1(strMetallicPath,  iMWidth, iMHeight);
	if (xRoughRGB.empty() || xMetallicRGB.empty())
		return;

	if (iRWidth != iMWidth || iRHeight != iMHeight)
	{
		Zenith_Error(LOG_CATEGORY_TERRAIN,
			"[RenderTest] Roughness/metallic resolution mismatch in %s (%dx%d vs %dx%d) — RM pack skipped",
			strSourceDir.c_str(), iRWidth, iRHeight, iMWidth, iMHeight);
		return;
	}

	// Both source textures are grayscale stored in all three RGB channels by
	// the BC1 encoder. Sample the red channel as the value.
	const size_t ulPixelCount = static_cast<size_t>(iRWidth) * iRHeight;
	std::vector<uint8_t> xPacked(ulPixelCount * 4, 0);
	for (size_t i = 0; i < ulPixelCount; ++i)
	{
		xPacked[i * 4 + 0] = 0;
		xPacked[i * 4 + 1] = xRoughRGB[i * 3];     // G = roughness
		xPacked[i * 4 + 2] = xMetallicRGB[i * 3];  // B = metallic
		xPacked[i * 4 + 3] = 255;
	}

	Zenith_DataStream xOut;
	xOut << iRWidth;
	xOut << iRHeight;
	xOut << static_cast<int32_t>(1);
	xOut << static_cast<TextureFormat>(TEXTURE_FORMAT_RGBA8_UNORM);
	xOut << static_cast<size_t>(xPacked.size());
	xOut.WriteData(xPacked.data(), xPacked.size());
	xOut.WriteToFile(strOutputPath.c_str());

	Zenith_Log(LOG_CATEGORY_TERRAIN,
		"[RenderTest] Packed roughness/metallic into %s (%dx%d, RGBA8, G=roughness, B=metallic)",
		strOutputPath.c_str(), iRWidth, iRHeight);
}

static void RenderTest_PackTerrainRoughnessMetallic()
{
	RenderTest_PackRoughnessMetallic(std::string(GAME_ASSETS_DIR) + "Textures/Terrain/Grass/");
	RenderTest_PackRoughnessMetallic(std::string(GAME_ASSETS_DIR) + "Textures/Terrain/Rock/");
}

// User-supplied splatmap at Assets/Textures/Terrain/Splatmap.ztxtr is a
// single-channel R32_SFLOAT image whose values blend between just two
// materials (0 = grass, 1 = rock). The terrain shader expects an RGBA8
// splatmap where each channel holds the weight of one of the four material
// slots. Pack the user's value into R = (1 - x) and G = x with B = A = 0 and
// write the result alongside the source so AddStep_SetTerrainSplatmapPath can
// reference a known prefixed path. Idempotent on file modification time.
static const char* sk_szUserSplatmapSourceRel = "Textures/Terrain/Splatmap" ZENITH_TEXTURE_EXT;
static const char* sk_szUserSplatmapRGBARel   = "Textures/Terrain/Splatmap_RGBA" ZENITH_TEXTURE_EXT;

static void RenderTest_ConvertUserSplatmapToRGBA8()
{
	const std::string strSourcePath = std::string(GAME_ASSETS_DIR) + sk_szUserSplatmapSourceRel;
	const std::string strOutputPath = std::string(GAME_ASSETS_DIR) + sk_szUserSplatmapRGBARel;

	if (!std::filesystem::exists(strSourcePath))
	{
		Zenith_Warning(LOG_CATEGORY_TERRAIN,
			"[RenderTest] User splatmap source not found at %s — terrain will sample uninitialised splat data",
			strSourcePath.c_str());
		return;
	}

	// Skip the conversion if the output is at least as new as the source.
	const bool bForce = RenderTest_HasCommandLineFlag("--rendertest-force-regenerate");
	if (!bForce && std::filesystem::exists(strOutputPath) &&
		std::filesystem::last_write_time(strOutputPath) >= std::filesystem::last_write_time(strSourcePath))
	{
		return;
	}

	Zenith_DataStream xIn;
	xIn.ReadFromFile(strSourcePath.c_str());

	int32_t iWidth = 0, iHeight = 0, iDepth = 0;
	TextureFormat eFormat = TEXTURE_FORMAT_NONE;
	size_t ulDataSize = 0;
	xIn >> iWidth;
	xIn >> iHeight;
	xIn >> iDepth;
	xIn.ReadData(&eFormat, sizeof(eFormat));
	xIn >> ulDataSize;

	const size_t ulExpectedFloats = static_cast<size_t>(iWidth) * static_cast<size_t>(iHeight);
	if (eFormat != TEXTURE_FORMAT_R32_SFLOAT || ulDataSize != ulExpectedFloats * sizeof(float))
	{
		Zenith_Error(LOG_CATEGORY_TERRAIN,
			"[RenderTest] Splatmap source has unexpected format/size (got fmt=%d dataSize=%zu, expected R32_SFLOAT %dx%d=%zu bytes)",
			static_cast<int>(eFormat), ulDataSize, iWidth, iHeight, ulExpectedFloats * sizeof(float));
		return;
	}

	std::vector<float> xFloats(ulExpectedFloats);
	xIn.ReadData(xFloats.data(), ulDataSize);

	std::vector<uint8_t> xOutBytes(ulExpectedFloats * 4);
	for (size_t i = 0; i < ulExpectedFloats; ++i)
	{
		const float fX = std::clamp(xFloats[i], 0.0f, 1.0f);
		const size_t uIdx = i * 4;
		xOutBytes[uIdx + 0] = static_cast<uint8_t>((1.0f - fX) * 255.0f + 0.5f);
		xOutBytes[uIdx + 1] = static_cast<uint8_t>(fX * 255.0f + 0.5f);
		xOutBytes[uIdx + 2] = 0;
		xOutBytes[uIdx + 3] = 0;
	}

	Zenith_DataStream xOut;
	xOut << iWidth;
	xOut << iHeight;
	xOut << iDepth;
	xOut << static_cast<TextureFormat>(TEXTURE_FORMAT_RGBA8_UNORM);
	xOut << static_cast<size_t>(xOutBytes.size());
	xOut.WriteData(xOutBytes.data(), xOutBytes.size());
	xOut.WriteToFile(strOutputPath.c_str());

	Zenith_Log(LOG_CATEGORY_TERRAIN,
		"[RenderTest] Converted single-channel splatmap to RGBA8 (%dx%d) at %s",
		iWidth, iHeight, strOutputPath.c_str());
}
#endif

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
	s_uRenderTestSmokeFrameLimit = RenderTest_GetCommandLineUInt("--rendertest-smoke-frames=", 240);
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
	using namespace Flux_TerrainConfig;

	// Resources (cube model + stick figure model + materials) are initialized
	// from Project_RegisterScriptBehaviours, which runs before automation steps.

	// Generate terrain heightmap + chunks + splatmap on disk before the saved
	// scene's terrain entity tries to read them at SaveScene / first load.
	Zenith_EditorAutomation::AddStep_Custom(&RenderTest_EnsureTerrainAssetsForAutomation);

	// Convert the user-supplied single-channel R32_SFLOAT splatmap into the
	// RGBA8 4-material weight format the terrain shader samples.
	Zenith_EditorAutomation::AddStep_Custom(&RenderTest_ConvertUserSplatmapToRGBA8);

	// Pack each material set's separately-supplied roughness + metallic BC1
	// textures into a single RGBA8 (G=roughness, B=metallic) — matches the
	// `xRM.gb` swizzle in Flux_Terrain_ToGBuffer.slang.
	Zenith_EditorAutomation::AddStep_Custom(&RenderTest_PackTerrainRoughnessMetallic);

	Zenith_EditorAutomation::AddStep_CreateScene("RenderTest");

	// GameManager — main camera with follow-camera script.
	const float fInitialPlayerY = 82.0f;
	const float fCamOffsetY = 12.0f;
	const float fCamOffsetZ = -15.0f;
	Zenith_EditorAutomation::AddStep_CreateEntity("GameManager");
	Zenith_EditorAutomation::AddStep_AddCamera();
	Zenith_EditorAutomation::AddStep_SetCameraPosition(TERRAIN_SIZE * 0.5f, fInitialPlayerY + fCamOffsetY, TERRAIN_SIZE * 0.5f + fCamOffsetZ);
	Zenith_EditorAutomation::AddStep_SetCameraPitch(-0.7f);
	Zenith_EditorAutomation::AddStep_SetCameraYaw(0.0f);
	Zenith_EditorAutomation::AddStep_SetCameraFOV(glm::radians(70.0f));
	Zenith_EditorAutomation::AddStep_SetCameraNear(0.1f);
	Zenith_EditorAutomation::AddStep_SetCameraFar(10000.0f);
	Zenith_EditorAutomation::AddStep_SetAsMainCamera();
	Zenith_EditorAutomation::AddStep_AttachScript("RenderTest_FollowCamera");

	// Terrain — fully expressible via the new automation steps.
	Zenith_EditorAutomation::AddStep_CreateEntity("RenderTestTerrain");
	Zenith_EditorAutomation::AddStep_SetEntityTransient(false);
	Zenith_EditorAutomation::AddStep_AddComponent("Terrain");
	Zenith_EditorAutomation::AddStep_SetTerrainMaterial(0, RenderTest::g_axTerrainMaterials[0].GetDirect());
	Zenith_EditorAutomation::AddStep_SetTerrainMaterial(1, RenderTest::g_axTerrainMaterials[1].GetDirect());
	Zenith_EditorAutomation::AddStep_SetTerrainMaterial(2, RenderTest::g_axTerrainMaterials[2].GetDirect());
	Zenith_EditorAutomation::AddStep_SetTerrainMaterial(3, RenderTest::g_axTerrainMaterials[3].GetDirect());
	Zenith_EditorAutomation::AddStep_SetTerrainSplatmapPath("game:Textures/Terrain/Splatmap_RGBA" ZENITH_TEXTURE_EXT);
	Zenith_EditorAutomation::AddStep_AddCollider();
	Zenith_EditorAutomation::AddStep_AddColliderShape(COLLISION_VOLUME_TYPE_TERRAIN, RIGIDBODY_TYPE_STATIC);

	// Cube platform — uses exported .zmodel so it survives SaveScene/LoadScene.
	Zenith_EditorAutomation::AddStep_CreateEntity("CenterPlatform");
	Zenith_EditorAutomation::AddStep_SetEntityTransient(false);
	Zenith_EditorAutomation::AddStep_SetTransformPosition(TERRAIN_SIZE * 0.5f, 80.0f, TERRAIN_SIZE * 0.5f);
	Zenith_EditorAutomation::AddStep_SetTransformScale(30.0f, 0.5f, 30.0f);
	Zenith_EditorAutomation::AddStep_AddModel();
	Zenith_EditorAutomation::AddStep_LoadModel(RenderTest::g_strCubeModelPath.c_str());
	Zenith_EditorAutomation::AddStep_SetModelMaterial(0, RenderTest::g_xCubeMaterial.GetDirect());
	Zenith_EditorAutomation::AddStep_AddCollider();
	Zenith_EditorAutomation::AddStep_AddColliderShape(COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_STATIC);

	// Player — .zmodel with skeleton; AnimatorComponent discovers skeleton on OnStart.
	Zenith_EditorAutomation::AddStep_CreateEntity("Player");
	Zenith_EditorAutomation::AddStep_SetEntityTransient(false);
	Zenith_EditorAutomation::AddStep_SetTransformPosition(TERRAIN_SIZE * 0.5f, fInitialPlayerY, TERRAIN_SIZE * 0.5f);
	Zenith_EditorAutomation::AddStep_AddModel();
	Zenith_EditorAutomation::AddStep_LoadModel(RenderTest::g_strStickFigureModelPath.c_str());
	Zenith_EditorAutomation::AddStep_SetModelMaterial(0, RenderTest::g_xPlayerMaterial.GetDirect());
	Zenith_EditorAutomation::AddStep_AddAnimator();
	Zenith_EditorAutomation::AddStep_AttachScript("RenderTest_PlayerBehaviour");

	// Smoke runner — attached BEFORE save so it ends up in the saved scene.
	if (RenderTest_IsSmokeMode())
	{
		Zenith_EditorAutomation::AddStep_CreateEntity("RenderTestSmokeRunner");
		Zenith_EditorAutomation::AddStep_SetEntityTransient(false);
		Zenith_EditorAutomation::AddStep_AttachScript("RenderTest_SmokeRunner");
	}

	Zenith_EditorAutomation::AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/RenderTest" ZENITH_SCENE_EXT);
	Zenith_EditorAutomation::AddStep_UnloadScene();

	Zenith_EditorAutomation::AddStep_LoadInitialScene(&Project_LoadInitialScene);
}
#endif

void Project_LoadInitialScene()
{
	Zenith_SceneManager::RegisterSceneBuildIndex(0, GAME_ASSETS_DIR "Scenes/RenderTest" ZENITH_SCENE_EXT);
	Zenith_SceneManager::LoadSceneByIndexBlockingForBootstrap(0, SCENE_LOAD_SINGLE);

	// Global Flux_Terrain debug flags + editor mode are runtime state that
	// affects the loaded scene's rendering. Apply them here so they take effect
	// in both tools and non-tools builds, AFTER the scene is fully loaded.
#ifdef ZENITH_TOOLS
	if (RenderTest_IsSmokeMode())
	{
		if (RenderTest_HasCommandLineFlag("--rendertest-lod-debug"))
			Flux_Terrain::GetDebugMode() = 1;
		if (RenderTest_HasCommandLineFlag("--rendertest-wireframe"))
			Flux_Terrain::GetWireframeMode() = true;

		Zenith_Editor::SetEditorMode(EditorMode::Playing);
	}
#endif
}
