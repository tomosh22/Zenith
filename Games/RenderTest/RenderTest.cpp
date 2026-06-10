#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "Core/Zenith_CommandLine.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "DataStream/Zenith_DataStream.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_AnimatorComponent.h"
// Wave-19: AnimatorComponent.h is a Flux-include-free forwarding handle now;
// this TU uses the complete Flux_AnimationController type directly (GetController()).
#include "Flux/MeshAnimation/Flux_AnimationController.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_Query.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Particles/Flux_ParticleEmitterConfig.h"
#include "Flux/Terrain/Flux_TerrainImpl.h"
#include "Flux/Terrain/Flux_TerrainStreamingManagerImpl.h"
#include "Flux/Terrain/Flux_TerrainConfig.h"
#include "Physics/Zenith_Physics.h"
#include "Physics/Zenith_Physics.h"
#include "Prefab/Zenith_Prefab.h"
#include "UI/Zenith_UI.h"
#include "Zenith_OS_Include.h"

// Header-only behaviours: must be included into a compiled TU so their
// ZENITH_BEHAVIOUR_TYPE_NAME auto-registers the factory at startup.
#include "RenderTest/Components/RenderTest_PlayerBehaviour.h"
#include "RenderTest/Components/RenderTest_FollowCamera.h"

#ifdef ZENITH_TOOLS
#include "Editor/Zenith_EditorAutomation.h"
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_UndoSystem.h"
#include "Editor/TerrainEditor/Zenith_TerrainEditor.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#endif

// Grass system: painted-density meadows rebuilt from GrassDensity.ztxtr at
// scene load (windowed, all configs).
#include "Flux/Vegetation/Flux_GrassImpl.h"

// Defined alongside Project_LoadInitialScene below; also queued as a
// post-scene-load automation step in tools builds.
static void RenderTest_ApplyGrassDensityFromDisk();

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <vector>

// Phase 8: per-game ProjectResources struct.
namespace RenderTest
{
	struct RenderTestResources
	{
		MaterialHandle              m_axTerrainMaterials[4];
		ModelHandle                 m_xCubeModelAsset;
		std::string                 m_strCubeModelPath;
		ModelHandle                 m_xStickFigureModelAsset;
		std::string                 m_strStickFigureModelPath;
		MaterialHandle              m_xCubeMaterial;
		MaterialHandle              m_xPlayerMaterial;
		Flux_ParticleEmitterConfig* m_pxMuzzleConfig = nullptr;
	};

	static RenderTestResources g_xResources;
	RenderTestResources& Resources() { return g_xResources; }
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
	const uint32_t uVertexStride = xTerrain.GetVertexStride();
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
	xTerrains.Clear();
	g_xEngine.Scenes().QueryAllScenes<Zenith_TerrainComponent>().ForEach(
		[&xTerrains](Zenith_EntityID, Zenith_TerrainComponent& xTerrain)
		{
			xTerrains.PushBack(&xTerrain);
		});

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
		Flux_TerrainStreamingState* pxState = pxTerrain->m_pxStreamingState;

		const uint32_t uActiveCount = pxState ? static_cast<uint32_t>(pxState->m_xActiveChunkIndices.GetSize()) : 0;
		const uint32_t uHighResident = pxState ? RenderTest_CountHighResidentChunks(*pxState) : 0;
		const uint32_t uLowZero = pxState ? RenderTest_CountLowZeroChunks(*pxState) : TOTAL_CHUNKS;

		Zenith_Log(LOG_CATEGORY_TERRAIN,
			"[RenderTestSmoke] terrain[%u]=%p usable=%u culling=%u state=%p active=%u LOWZero=%u HIGHResident=%u streamsThisFrame=%u evictionsThisFrame=%u",
			u,
			static_cast<void*>(pxTerrain),
			pxTerrain->IsRenderGeometryUsable() ? 1u : 0u,
			(pxState && pxState->m_bCullingResourcesInitialized) ? 1u : 0u,
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
		if (!pxState || !pxState->m_bCullingResourcesInitialized)
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
			if (pxState->m_bCullingResourcesInitialized)
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

// Helper: find the player entity in the active scene by name.
static Zenith_Entity RenderTest_FindPlayerEntity()
{
	Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
	if (!xScene.IsValid()) return Zenith_Entity();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
	if (!pxSceneData) return Zenith_Entity();
	return pxSceneData->FindEntityByName("Player");
}

// IK smoke validation. Verifies that:
//   - The player's animator has 2 IK chains registered (LeftLeg, RightLeg)
//   - Each foot's world-space position is within a sane vertical band
//   - No NaN bone transforms have leaked into the skeleton
// Called after warm-up (>= frame 60) so OnStart has run and the chain registration
// in SetupLayeredAnimator has fired.
static bool RenderTest_LogIKSmokeState(uint32_t uFrame)
{
	bool bPass = true;
	Zenith_Entity xPlayer = RenderTest_FindPlayerEntity();
	if (!xPlayer.IsValid())
	{
		Zenith_Error(LOG_CATEGORY_ANIMATION, "RENDERTEST_SMOKE_FAIL: player entity not found at frame %u", uFrame);
		return false;
	}

	if (!xPlayer.HasComponent<Zenith_AnimatorComponent>())
	{
		Zenith_Error(LOG_CATEGORY_ANIMATION, "RENDERTEST_SMOKE_FAIL: player has no AnimatorComponent at frame %u", uFrame);
		return false;
	}

	Zenith_AnimatorComponent& xAnimator = xPlayer.GetComponent<Zenith_AnimatorComponent>();
	Flux_AnimationController& xController = xAnimator.GetController();
	const Flux_IKSolver* pxIK = xController.GetIKSolverPtr();
	if (!pxIK)
	{
		Zenith_Error(LOG_CATEGORY_ANIMATION, "RENDERTEST_SMOKE_FAIL: animator has no IK solver at frame %u", uFrame);
		return false;
	}

	if (!pxIK->HasChain("LeftLeg"))
	{
		Zenith_Error(LOG_CATEGORY_ANIMATION, "RENDERTEST_SMOKE_FAIL: IK solver missing LeftLeg chain at frame %u", uFrame);
		bPass = false;
	}
	if (!pxIK->HasChain("RightLeg"))
	{
		Zenith_Error(LOG_CATEGORY_ANIMATION, "RENDERTEST_SMOKE_FAIL: IK solver missing RightLeg chain at frame %u", uFrame);
		bPass = false;
	}

	if (!xPlayer.HasComponent<Zenith_ModelComponent>()) return bPass;
	Zenith_ModelComponent& xModel = xPlayer.GetComponent<Zenith_ModelComponent>();
	if (!xModel.HasSkeleton()) return bPass;
	Flux_SkeletonInstance* pxSkel = xModel.GetSkeletonInstance();
	if (!pxSkel) return bPass;
	Zenith_SkeletonAsset* pxSkelAsset = pxSkel->GetSourceSkeleton();
	if (!pxSkelAsset) return bPass;

	Zenith_TransformComponent& xT = xPlayer.GetComponent<Zenith_TransformComponent>();
	Zenith_Maths::Matrix4 xWorld; xT.BuildModelMatrix(xWorld);

	const char* aszFootBones[2] = { "LeftFoot", "RightFoot" };
	for (const char* szFootBone : aszFootBones)
	{
		const int32_t iFoot = pxSkelAsset->GetBoneIndex(szFootBone);
		if (iFoot < 0) continue;

		const Zenith_Maths::Matrix4& xFootModel = pxSkel->GetBoneModelTransform(static_cast<uint32_t>(iFoot));

		// NaN guard
		for (int c = 0; c < 4; ++c)
		{
			for (int r = 0; r < 4; ++r)
			{
				if (std::isnan(xFootModel[c][r]))
				{
					Zenith_Error(LOG_CATEGORY_ANIMATION,
						"RENDERTEST_SMOKE_FAIL: NaN bone transform on bone \"%s\" at frame %u", szFootBone, uFrame);
					return false;
				}
			}
		}
	}

	return bPass;
}

// IK validation against a known surface Y. Used during the teleport-to-cube
// portion of the smoke run to confirm the foot lands at the expected height.
static bool RenderTest_LogIKOnSurfaceState(uint32_t uFrame, float fExpectedSurfaceY, float fTolerance)
{
	Zenith_Entity xPlayer = RenderTest_FindPlayerEntity();
	if (!xPlayer.IsValid() || !xPlayer.HasComponent<Zenith_ModelComponent>())
	{
		return RenderTest_LogIKSmokeState(uFrame);
	}
	Zenith_ModelComponent& xModel = xPlayer.GetComponent<Zenith_ModelComponent>();
	if (!xModel.HasSkeleton()) return RenderTest_LogIKSmokeState(uFrame);
	Flux_SkeletonInstance* pxSkel = xModel.GetSkeletonInstance();
	if (!pxSkel) return RenderTest_LogIKSmokeState(uFrame);
	Zenith_SkeletonAsset* pxSkelAsset = pxSkel->GetSourceSkeleton();
	if (!pxSkelAsset) return RenderTest_LogIKSmokeState(uFrame);

	Zenith_TransformComponent& xT = xPlayer.GetComponent<Zenith_TransformComponent>();
	Zenith_Maths::Matrix4 xWorld; xT.BuildModelMatrix(xWorld);

	bool bPass = true;
	bool bAnyFootInRange = false;
	const char* aszFootBones[2] = { "LeftFoot", "RightFoot" };
	for (const char* szFootBone : aszFootBones)
	{
		const int32_t iFoot = pxSkelAsset->GetBoneIndex(szFootBone);
		if (iFoot < 0) continue;
		const Zenith_Maths::Matrix4& xFootModel = pxSkel->GetBoneModelTransform(static_cast<uint32_t>(iFoot));
		Zenith_Maths::Vector4 xFootW = xWorld * xFootModel * Zenith_Maths::Vector4(0, 0, 0, 1);
		const float fFootY = xFootW.y;
		if (fFootY > fExpectedSurfaceY - fTolerance && fFootY < fExpectedSurfaceY + fTolerance)
		{
			bAnyFootInRange = true;
		}
		Zenith_Log(LOG_CATEGORY_ANIMATION,
			"[RenderTestSmoke] frame=%u %s worldY=%.3f (expected ~%.3f +/- %.3f)",
			uFrame, szFootBone, fFootY, fExpectedSurfaceY, fTolerance);
	}

	// Require at least one foot to be planted on the expected surface. The
	// other may be lifted by the walk/idle animation cycle.
	if (!bAnyFootInRange)
	{
		Zenith_Error(LOG_CATEGORY_ANIMATION,
			"RENDERTEST_SMOKE_FAIL: neither foot world-Y in expected range [%.3f, %.3f] at frame %u",
			fExpectedSurfaceY - fTolerance, fExpectedSurfaceY + fTolerance, uFrame);
		bPass = false;
	}

	return bPass && RenderTest_LogIKSmokeState(uFrame);
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

			// IK chain registration / no-NaN check. Warm-up gate: chain
			// registration runs in the player's OnStart, which fires after the
			// scripts are dispatched — give it 60 frames to settle before
			// validating.
			if (m_uFrame >= 60)
			{
				m_bPassed = RenderTest_LogIKSmokeState(m_uFrame) && m_bPassed;
			}
		}

		// IK foot-placement validation via teleport.
		// Frame 60: warp the player above IKPlatform_NW (top Y=49.05) so it
		// settles on the cube. Frame 90+: validate foot Y is on the cube top.
		// Frame 150: warp back over the platform (top Y=48.75) and validate.
		if (m_uFrame == 60)
		{
			TeleportPlayerTo(Zenith_Maths::Vector3(244.0f, 49.5f, 268.0f));
		}
		if (m_uFrame == 120)
		{
			// On the NW cube. Add ankle height (0.05m) to expected surface Y.
			m_bPassed = RenderTest_LogIKOnSurfaceState(m_uFrame, 49.05f + 0.05f, 0.20f) && m_bPassed;
		}
		if (m_uFrame == 150)
		{
			TeleportPlayerTo(Zenith_Maths::Vector3(256.0f, 49.5f, 256.0f));
		}
		if (m_uFrame == 200)
		{
			// On the central platform. Top Y=48.75 plus ankle height.
			m_bPassed = RenderTest_LogIKOnSurfaceState(m_uFrame, 48.75f + 0.05f, 0.20f) && m_bPassed;
		}

		// Decal smoke: fire several shots straight down at the platform between
		// frames 210-220. Each shot exercises the full chain (raycast → decal
		// spawn → CPU pool → GPU upload → render-graph passes), and the
		// per-stage diagnostic logs in Flux_Decals + PlayerBehaviour::Shoot
		// pinpoint where the chain breaks if decals don't appear in the frame.
		if (m_uFrame == 210 || m_uFrame == 215 || m_uFrame == 220)
		{
			RenderTest_PlayerBehaviour* pxPlayer = RenderTest_PlayerBehaviour::GetActiveInstance();
			if (pxPlayer)
			{
				Zenith_Log(LOG_CATEGORY_GAMEPLAY,
					"[DECAL_SMOKE] frame %u: triggering downward shot", m_uFrame);
				pxPlayer->TriggerShootDownwardForTest();
			}
			else
			{
				Zenith_Log(LOG_CATEGORY_GAMEPLAY,
					"[DECAL_SMOKE] frame %u: PlayerBehaviour instance not registered yet", m_uFrame);
			}
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
	// Teleport the player's physics body to a target world position. Used by the
	// IK foot-placement validation to drop the player onto a known cube without
	// the 200+ frames of walking required at m_fMoveSpeed=5.0f.
	void TeleportPlayerTo(const Zenith_Maths::Vector3& xTarget)
	{
		Zenith_Entity xPlayer = RenderTest_FindPlayerEntity();
		if (!xPlayer.IsValid() || !xPlayer.HasComponent<Zenith_ColliderComponent>()) return;
		g_xEngine.Physics().TeleportBody(xPlayer.GetComponent<Zenith_ColliderComponent>().GetBodyID(), xTarget);
	}

	void CaptureResidencySnapshots(Zenith_Vector<RenderTest_ResidencySnapshot>& axSnapshots)
	{
		Zenith_Vector<Zenith_TerrainComponent*> xTerrains;
		xTerrains.Clear();
		g_xEngine.Scenes().QueryAllScenes<Zenith_TerrainComponent>().ForEach(
			[&xTerrains](Zenith_EntityID, Zenith_TerrainComponent& xTerrain)
			{
				xTerrains.PushBack(&xTerrain);
			});
		axSnapshots.Clear();
		for (u_int u = 0; u < xTerrains.GetSize(); u++)
		{
			Flux_TerrainStreamingState* pxState =
				xTerrains.Get(u)->m_pxStreamingState;
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
	if (!RenderTest::Resources().m_strCubeModelPath.empty())
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
		RenderTest::Resources().m_xCubeModelAsset.Set(pxModel);
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

	RenderTest::Resources().m_strCubeModelPath = strModelPath;
}

// Mirrors Combat::TryInitializeStickFigureModel — bundle the StickFigure mesh
// asset + skeleton at ENGINE_ASSETS_DIR/Meshes/StickFigure into a .zmodel on
// disk for AddStep_LoadModel to reference. Idempotent: skips export when the
// .zmodel already exists. Non-tools builds assume the .zmodel was produced by
// a previous tools run.
static void EnsureStickFigureModelExists()
{
	if (!RenderTest::Resources().m_strStickFigureModelPath.empty())
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
		RenderTest::Resources().m_xStickFigureModelAsset.Set(pxModel);
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

	RenderTest::Resources().m_strStickFigureModelPath = strModelPath;
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

	SetupPBRTerrainMaterial(RenderTest::Resources().m_axTerrainMaterials[0], "RenderTestTerrainGrass",  strGrassDir);
	SetupPBRTerrainMaterial(RenderTest::Resources().m_axTerrainMaterials[1], "RenderTestTerrainRock",   strRockDir);
	SetupPBRTerrainMaterial(RenderTest::Resources().m_axTerrainMaterials[2], "RenderTestTerrainGrass2", strGrassDir);
	SetupPBRTerrainMaterial(RenderTest::Resources().m_axTerrainMaterials[3], "RenderTestTerrainRock2",  strRockDir);

	const std::string strProceduralTexDir = std::string(GAME_ASSETS_DIR) + "Textures/";
	std::filesystem::create_directories(strProceduralTexDir);

	RenderTest::Resources().m_xCubeMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	RenderTest::Resources().m_xCubeMaterial.GetDirect()->SetName("RenderTestCubeMaterial");
	RenderTest::Resources().m_xCubeMaterial.GetDirect()->SetDiffuseTexture(g_xEngine.FluxGraphics().m_xGridTexture);
	RenderTest::Resources().m_xPlayerMaterial = CreateFlatColorMaterial("RenderTestPlayerMaterial",
		strProceduralTexDir + "PlayerDiffuse" ZENITH_TEXTURE_EXT, 0, 200, 220);

	EnsureUnitCubeModelExists();
	EnsureStickFigureModelExists();

	// --- Muzzle flash particle config (Sokoban pattern) ---
	if (!RenderTest::Resources().m_pxMuzzleConfig)
	{
		RenderTest::Resources().m_pxMuzzleConfig = new Flux_ParticleEmitterConfig();
		RenderTest::Resources().m_pxMuzzleConfig->m_fSpawnRate            = 0.0f;       // burst-only
		RenderTest::Resources().m_pxMuzzleConfig->m_uBurstCount           = 8;
		RenderTest::Resources().m_pxMuzzleConfig->m_uMaxParticles         = 64;
		RenderTest::Resources().m_pxMuzzleConfig->m_fLifetimeMin          = 0.04f;
		RenderTest::Resources().m_pxMuzzleConfig->m_fLifetimeMax          = 0.10f;
		RenderTest::Resources().m_pxMuzzleConfig->m_fSpreadAngleDegrees   = 25.0f;
		RenderTest::Resources().m_pxMuzzleConfig->m_fSpeedMin             = 4.0f;
		RenderTest::Resources().m_pxMuzzleConfig->m_fSpeedMax             = 8.0f;
		RenderTest::Resources().m_pxMuzzleConfig->m_xGravity              = Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f);
		RenderTest::Resources().m_pxMuzzleConfig->m_fDrag                 = 4.0f;
		RenderTest::Resources().m_pxMuzzleConfig->m_xColorStart           = { 1.0f, 0.95f, 0.4f, 1.0f };
		RenderTest::Resources().m_pxMuzzleConfig->m_xColorEnd             = { 1.0f, 0.40f, 0.0f, 0.0f };
		RenderTest::Resources().m_pxMuzzleConfig->m_fSizeStart            = 0.25f;
		RenderTest::Resources().m_pxMuzzleConfig->m_fSizeEnd              = 0.05f;
		RenderTest::Resources().m_pxMuzzleConfig->m_bAdditiveBlending     = true;
		RenderTest::Resources().m_pxMuzzleConfig->m_bUseGPUCompute        = false;
		Flux_ParticleEmitterConfig::Register("RenderTest_MuzzleFlash", RenderTest::Resources().m_pxMuzzleConfig);
	}

	s_bResourcesInitialized = true;
}

#ifdef ZENITH_TOOLS
// RenderTest's terrain is GENERATED by the engine terrain editor
// (Zenith_TerrainEditor) through deterministic editor-automation steps —
// seeded procedural noise + scripted brush strokes + erosion + auto-splat —
// which save Height/Splatmap_RGBA/GrassDensity .ztxtr to the game assets dir
// and bake the chunk meshes from them. No pre-made source textures on disk.
// The marker gates a one-time generation; bump the version (or pass
// --rendertest-force-regenerate) to regenerate.
// v2: spawn meadow painted as a ring around the CenterPlatform footprint
// (was one dab over it — blades poked through the platform deck).
static const char* sk_szTerrainProcMarkerRel = "Terrain/terrain_proc_v2.marker";

static bool RenderTest_TerrainAssetsNeedRegeneration()
{
	if (RenderTest_HasCommandLineFlag("--rendertest-force-regenerate"))
	{
		return true;
	}
	const std::string strChunksDir = std::string(GAME_ASSETS_DIR) + "Terrain/";
	return !std::filesystem::exists(std::string(GAME_ASSETS_DIR) + sk_szTerrainProcMarkerRel)
		|| !std::filesystem::exists(strChunksDir + "Render_LOW_0_0" ZENITH_MESH_EXT)
		|| !std::filesystem::exists(std::string(GAME_ASSETS_DIR) + "Textures/Terrain/Splatmap_RGBA" ZENITH_TEXTURE_EXT);
}

static void RenderTest_WriteTerrainProcMarker()
{
	const std::string strMarkerPath = std::string(GAME_ASSETS_DIR) + sk_szTerrainProcMarkerRel;
	Zenith_DataStream xStream;
	xStream << static_cast<int32_t>(1);
	xStream.WriteToFile(strMarkerPath.c_str());
	Zenith_Log(LOG_CATEGORY_TERRAIN, "[RenderTest] Terrain generation marker written: %s", strMarkerPath.c_str());
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

#endif

const char* Project_GetName()
{
	return "RenderTest";
}

const char* Project_GetGameAssetsDirectory()
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
	// Particle config registry borrows the pointer (Sokoban precedent at
	// Sokoban.cpp:204). Unregister + delete + null so a subsequent Initialise
	// rebuilds cleanly if the process were to keep running.
	if (RenderTest::Resources().m_pxMuzzleConfig)
	{
		Flux_ParticleEmitterConfig::Unregister("RenderTest_MuzzleFlash");
		delete RenderTest::Resources().m_pxMuzzleConfig;
		RenderTest::Resources().m_pxMuzzleConfig = nullptr;
	}

	// Release file-scope asset handles BEFORE AssetRegistry::Shutdown
	// destroys the underlying assets. Without this, the static
	// destructors of these globals fire AFTER the engine has shut down
	// and try to Release() pointers into freed memory -- manifesting
	// as either "Release called on asset with 0 ref count" or an
	// outright access violation, depending on whether the freed slot
	// has been reused. Reset-via-assignment runs each handle's dtor
	// while the registry is still alive.
	for (MaterialHandle& xMat : RenderTest::Resources().m_axTerrainMaterials)
	{
		xMat = MaterialHandle{};
	}
	RenderTest::Resources().m_xCubeModelAsset        = ModelHandle{};
	RenderTest::Resources().m_xStickFigureModelAsset = ModelHandle{};
	RenderTest::Resources().m_xCubeMaterial          = MaterialHandle{};
	RenderTest::Resources().m_xPlayerMaterial        = MaterialHandle{};
}

void Project_LoadInitialScene();

#ifdef ZENITH_TOOLS
void Project_InitializeResources()
{
	InitializeRenderTestResources();
}

// Final smoke automation step: flips the editor into Play mode (and applies
// the terrain debug CLI flags) AFTER the deferred initial-scene load has
// completed, so the smoke runner's first OnUpdate probes the real scene.
static void RenderTest_EnterSmokePlayMode()
{
	if (RenderTest_HasCommandLineFlag("--rendertest-lod-debug"))
		g_xEngine.Terrain().GetDebugMode() = 1;
	if (RenderTest_HasCommandLineFlag("--rendertest-wireframe"))
		g_xEngine.Terrain().GetWireframeMode() = true;

	g_xEngine.Editor().SetEditorMode(EditorMode::Playing);
}

void Project_RegisterEditorAutomationSteps()
{
	using namespace Flux_TerrainConfig;

	// Resources (cube model + stick figure model + materials) are initialized
	// from Project_RegisterScriptBehaviours, which runs before automation steps.

	// Terrain authoring: the engine terrain editor GENERATES the terrain
	// deterministically (seed 1337 + integer-hash noise => byte-identical
	// outputs per run) and bakes Height/Splatmap_RGBA/GrassDensity .ztxtr +
	// every chunk mesh to the game assets dir. One-time, marker-gated; these
	// steps run before the terrain entity exists (standalone editor session).
	if (RenderTest_TerrainAssetsNeedRegeneration())
	{
		Zenith_EditorAutomation& xAuto = g_xEngine.EditorAutomation();
		const int iSetHeight = static_cast<int>(Zenith_TerrainBrushTool::SetHeight);
		const int iTerrace   = static_cast<int>(Zenith_TerrainBrushTool::Terrace);
		const int iNoise     = static_cast<int>(Zenith_TerrainBrushTool::Noise);
		const int iStamp     = static_cast<int>(Zenith_TerrainBrushTool::Stamp);
		const int iGrass     = static_cast<int>(Zenith_TerrainBrushTool::GrassDensity);

		// Start from defaults — the session seeds from any previous bake's
		// textures on disk, and the splat/grass strokes below BLEND (a re-run
		// without the reset would diverge byte-wise).
		xAuto.AddStep_TerrainResetSession();

		// Rolling FBM base around 46m with a ridged accent.
		xAuto.AddStep_TerrainGenerateProcedural(1337, 0.09f, 0.05f, 0.0008f, 6, 2.0f, 0.5f, 0.25f);

		// Feature hills: a smooth dome, a terraced mesa, a roughened noise
		// field, and the dome cloned east via the copy/stamp brush.
		xAuto.AddStep_TerrainBrushStroke(iSetHeight, 760.0f, 420.0f, 220.0f, 0.9f, 120.0f);
		xAuto.AddStep_TerrainBrushStroke(iSetHeight, 1500.0f, 900.0f, 260.0f, 0.85f, 95.0f);
		xAuto.AddStep_TerrainBrushStroke(iTerrace, 1500.0f, 900.0f, 300.0f, 0.8f, 10.0f);
		xAuto.AddStep_TerrainBrushStroke(iNoise, 1100.0f, 1500.0f, 280.0f, 0.7f, 14.0f);
		xAuto.AddStep_TerrainSampleStamp(760.0f, 420.0f, 240.0f);
		xAuto.AddStep_TerrainBrushStroke(iStamp, 2100.0f, 600.0f, 240.0f, 0.9f, 0.0f);

		// Erode everything (hydraulic droplets + thermal slumping), THEN
		// re-flatten the gameplay plateau the IK platforms / player spawn /
		// smoke expectations assume (fully flat within ~45m of 256,256).
		xAuto.AddStep_TerrainErode(150000, 2, 1337);
		xAuto.AddStep_TerrainBrushStroke(iSetHeight, 256.0f, 256.0f, 90.0f, 1.0f, 48.25f);

		// Auto-splat: grass on flats, rock on steeps, lowland + altitude accents.
		xAuto.AddStep_TerrainAutoSplatRule(0, 0.0f, 512.0f, 0.0f, 22.0f, 1.0f, 0.15f);
		xAuto.AddStep_TerrainAutoSplatRule(1, 0.0f, 512.0f, 16.0f, 90.0f, 1.2f, 0.10f);
		xAuto.AddStep_TerrainAutoSplatRule(2, 0.0f, 60.0f, 0.0f, 30.0f, 0.5f, 0.30f);
		xAuto.AddStep_TerrainAutoSplatRule(3, 110.0f, 512.0f, 0.0f, 90.0f, 0.8f, 0.10f);
		xAuto.AddStep_TerrainRunAutoSplat();

		// Grass meadows painted in around the gameplay plateau + the dome foot.
		// The spawn meadow is a RING of four dabs around the CenterPlatform
		// footprint (X/Z 241-271) rather than one dab over it — blades root in
		// the terrain under the platform and would poke up through its deck.
		xAuto.AddStep_TerrainBrushStroke(iGrass, 256.0f, 330.0f, 60.0f, 1.0f, 0.6f);
		xAuto.AddStep_TerrainBrushStroke(iGrass, 256.0f, 182.0f, 60.0f, 1.0f, 0.6f);
		xAuto.AddStep_TerrainBrushStroke(iGrass, 330.0f, 256.0f, 60.0f, 1.0f, 0.6f);
		xAuto.AddStep_TerrainBrushStroke(iGrass, 182.0f, 256.0f, 60.0f, 1.0f, 0.6f);
		xAuto.AddStep_TerrainBrushStroke(iGrass, 420.0f, 360.0f, 80.0f, 1.0f, 0.5f);

		// Persist the textures + bake every chunk mesh, then write the marker.
		xAuto.AddStep_TerrainSaveTextures();
		xAuto.AddStep_TerrainExportChunks();
		xAuto.AddStep_Custom(&RenderTest_WriteTerrainProcMarker);
	}

	// Pack each material set's separately-supplied roughness + metallic BC1
	// textures into a single RGBA8 (G=roughness, B=metallic) — matches the
	// `xRM.gb` swizzle in Flux_Terrain_ToGBuffer.slang.
	g_xEngine.EditorAutomation().AddStep_Custom(&RenderTest_PackTerrainRoughnessMetallic);

	g_xEngine.EditorAutomation().AddStep_CreateScene("RenderTest");

	// GameManager — main camera with follow-camera script.
	// Initial position is approximate; the FollowCamera overwrites it on the
	// first OnLateUpdate to track the player at the over-the-shoulder offset.
	// Pitch starts at the shooter angle so the editor preview matches the
	// in-play view rather than the old top-down -0.7rad framing.
	// 50.5 puts the CenterPlatform (centre = this - 2, half-height 0.25) with
	// its BASE exactly on the generated spawn plateau at 48.25m — the whole
	// platform assembly sits proud of the terrain instead of buried in it.
	const float fInitialPlayerY = 50.5f;
	const float fCamOffsetY = 2.0f;
	const float fCamOffsetZ = -4.0f;
	g_xEngine.EditorAutomation().AddStep_CreateEntity("GameManager");
	g_xEngine.EditorAutomation().AddStep_AddCamera();
	g_xEngine.EditorAutomation().AddStep_SetCameraPosition(512 * 0.5f, fInitialPlayerY + fCamOffsetY, 512 * 0.5f + fCamOffsetZ);
	g_xEngine.EditorAutomation().AddStep_SetCameraPitch(-0.15f);
	g_xEngine.EditorAutomation().AddStep_SetCameraYaw(0.0f);
	g_xEngine.EditorAutomation().AddStep_SetCameraFOV(glm::radians(70.0f));
	g_xEngine.EditorAutomation().AddStep_SetCameraNear(0.1f);
	g_xEngine.EditorAutomation().AddStep_SetCameraFar(10000.0f);
	g_xEngine.EditorAutomation().AddStep_SetAsMainCamera();
	g_xEngine.EditorAutomation().AddStep_AttachScript("RenderTest_FollowCamera");

	// Terrain — fully expressible via the new automation steps.
	g_xEngine.EditorAutomation().AddStep_CreateEntity("RenderTestTerrain");
	g_xEngine.EditorAutomation().AddStep_SetEntityTransient(false);
	g_xEngine.EditorAutomation().AddStep_AddComponent("Terrain");
	g_xEngine.EditorAutomation().AddStep_SetTerrainMaterial(0, RenderTest::Resources().m_axTerrainMaterials[0].GetDirect());
	g_xEngine.EditorAutomation().AddStep_SetTerrainMaterial(1, RenderTest::Resources().m_axTerrainMaterials[1].GetDirect());
	g_xEngine.EditorAutomation().AddStep_SetTerrainMaterial(2, RenderTest::Resources().m_axTerrainMaterials[2].GetDirect());
	g_xEngine.EditorAutomation().AddStep_SetTerrainMaterial(3, RenderTest::Resources().m_axTerrainMaterials[3].GetDirect());
	g_xEngine.EditorAutomation().AddStep_SetTerrainSplatmapPath("game:Textures/Terrain/Splatmap_RGBA" ZENITH_TEXTURE_EXT);
	g_xEngine.EditorAutomation().AddStep_AddCollider();
	g_xEngine.EditorAutomation().AddStep_AddColliderShape(COLLISION_VOLUME_TYPE_TERRAIN, RIGIDBODY_TYPE_STATIC);

	// Cube platform — uses exported .zmodel so it survives SaveScene/LoadScene.
	g_xEngine.EditorAutomation().AddStep_CreateEntity("CenterPlatform");
	g_xEngine.EditorAutomation().AddStep_SetEntityTransient(false);
	g_xEngine.EditorAutomation().AddStep_SetTransformPosition(512 * 0.5f, fInitialPlayerY - 2.f, 512 * 0.5f);
	g_xEngine.EditorAutomation().AddStep_SetTransformScale(30.0f, 0.5f, 30.0f);
	g_xEngine.EditorAutomation().AddStep_AddModel();
	g_xEngine.EditorAutomation().AddStep_LoadModel(RenderTest::Resources().m_strCubeModelPath.c_str());
	g_xEngine.EditorAutomation().AddStep_SetModelMaterial(0, RenderTest::Resources().m_xCubeMaterial.GetDirect());
	g_xEngine.EditorAutomation().AddStep_AddCollider();
	g_xEngine.EditorAutomation().AddStep_AddColliderShape(COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_STATIC);

	// IK foot-placement demo: small cube platforms at varying heights around the
	// center platform. As the player walks across them with feet on different
	// surfaces, the asymmetric knee bend demonstrates real IK foot placement.
	// Top-Y values are designed so the dynamic capsule can step up without jumping
	// (max +0.30m above the platform top at Y=48.75). Bases sit on the main
	// platform top, which itself sits base-down on the 48.25m terrain plateau.
	auto AddIKPlatform = [](const char* szName, float fX, float fY, float fZ,
		float fSX, float fSY, float fSZ)
	{
		g_xEngine.EditorAutomation().AddStep_CreateEntity(szName);
		g_xEngine.EditorAutomation().AddStep_SetEntityTransient(false);
		g_xEngine.EditorAutomation().AddStep_SetTransformPosition(fX, fY, fZ);
		g_xEngine.EditorAutomation().AddStep_SetTransformScale(fSX, fSY, fSZ);
		g_xEngine.EditorAutomation().AddStep_AddModel();
		g_xEngine.EditorAutomation().AddStep_LoadModel(RenderTest::Resources().m_strCubeModelPath.c_str());
		g_xEngine.EditorAutomation().AddStep_SetModelMaterial(0, RenderTest::Resources().m_xCubeMaterial.GetDirect());
		g_xEngine.EditorAutomation().AddStep_AddCollider();
		g_xEngine.EditorAutomation().AddStep_AddColliderShape(COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_STATIC);
	};

	AddIKPlatform("IKPlatform_N",  256.0f, 48.80f, 269.0f, 1.0f, 0.10f, 1.0f);  // top Y=48.85
	AddIKPlatform("IKPlatform_E",  269.0f, 48.85f, 256.0f, 1.0f, 0.20f, 1.0f);  // top Y=48.95
	AddIKPlatform("IKPlatform_S",  256.0f, 48.80f, 243.0f, 1.0f, 0.10f, 1.0f);  // top Y=48.85
	AddIKPlatform("IKPlatform_W",  243.0f, 48.90f, 256.0f, 1.0f, 0.30f, 1.0f);  // top Y=49.05
	AddIKPlatform("IKPlatform_NE", 268.0f, 48.80f, 268.0f, 1.2f, 0.10f, 1.2f);  // top Y=48.85
	AddIKPlatform("IKPlatform_SE", 268.0f, 48.90f, 244.0f, 1.2f, 0.30f, 1.2f);  // top Y=49.05
	AddIKPlatform("IKPlatform_SW", 244.0f, 48.85f, 244.0f, 1.2f, 0.20f, 1.2f);  // top Y=48.95
	AddIKPlatform("IKPlatform_NW", 244.0f, 48.90f, 268.0f, 1.5f, 0.30f, 1.5f);  // top Y=49.05 — the showpiece step

	// Spawn step: a 30cm-tall block placed under the player's LEFT foot at
	// spawn, dimensioned so it sits ENTIRELY OUTSIDE the player's capsule
	// X-range (capsule radius 0.10 → X-range [-0.10, +0.10] from player center).
	// The cube sits at X-range [255.70, 255.895] — its right edge at offset
	// -0.105 from the player center (256), 5mm clear of the capsule's left
	// edge at offset -0.10. This margin is critical: if the cube edge touches
	// or penetrates the capsule, the capsule rests on the cube top instead of
	// the main platform, and BOTH feet end up at bind level (49.10) with the
	// right foot's main-platform target unreachable — making both legs straight
	// and the IK demo invisible. With the cube outside the capsule, the capsule
	// rests cleanly on the main platform (playerY=49.80), and only the left
	// foot's downward raycast hits the cube top.
	//
	// Result at spawn:
	//   Left foot at X=255.85 (over step) → IK lifts to 49.10 (knee bends 36cm forward)
	//   Right foot at X=256.15 (over main) → IK at 48.80 (leg nearly straight)
	AddIKPlatform("IKStep_Spawn", 255.7975f, 48.90f, 256.0f, 0.195f, 0.30f, 0.40f);  // top Y=49.05, X-range [255.70, 255.895]

	// Player — .zmodel with skeleton; AnimatorComponent discovers skeleton on OnStart.
	g_xEngine.EditorAutomation().AddStep_CreateEntity("Player");
	g_xEngine.EditorAutomation().AddStep_SetEntityTransient(false);
	g_xEngine.EditorAutomation().AddStep_SetTransformPosition(512 * 0.5f, fInitialPlayerY, 512 * 0.5f);
	g_xEngine.EditorAutomation().AddStep_AddModel();
	g_xEngine.EditorAutomation().AddStep_LoadModel(RenderTest::Resources().m_strStickFigureModelPath.c_str());
	g_xEngine.EditorAutomation().AddStep_SetModelMaterial(0, RenderTest::Resources().m_xPlayerMaterial.GetDirect());
	g_xEngine.EditorAutomation().AddStep_AddAnimator();
	// Muzzle flash emitter lives on the Player entity; the player behaviour
	// overrides its emit position+direction per shot so we don't need a
	// separate gun-barrel child entity.
	g_xEngine.EditorAutomation().AddStep_AddParticleEmitter();
	g_xEngine.EditorAutomation().AddStep_SetParticleConfigByName("RenderTest_MuzzleFlash");
	g_xEngine.EditorAutomation().AddStep_SetParticleEmitting(false);
	g_xEngine.EditorAutomation().AddStep_AttachScript("RenderTest_PlayerBehaviour");

	// HUD canvas — crosshair (5 small UIRects) + ammo counter text.
	g_xEngine.EditorAutomation().AddStep_CreateEntity("HUD");
	g_xEngine.EditorAutomation().AddStep_SetEntityTransient(false);
	g_xEngine.EditorAutomation().AddStep_AddUI();

	// Crosshair: center pixel + 4 short bars at +/-10px on each axis.
	g_xEngine.EditorAutomation().AddStep_CreateUIRect("Crosshair_Center");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("Crosshair_Center", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("Crosshair_Center", 0.0f, 0.0f);
	g_xEngine.EditorAutomation().AddStep_SetUISize("Crosshair_Center", 4.0f, 4.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("Crosshair_Center", 1.0f, 1.0f, 1.0f, 0.85f);

	g_xEngine.EditorAutomation().AddStep_CreateUIRect("Crosshair_Top");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("Crosshair_Top", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("Crosshair_Top", 0.0f, -10.0f);
	g_xEngine.EditorAutomation().AddStep_SetUISize("Crosshair_Top", 2.0f, 8.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("Crosshair_Top", 1.0f, 1.0f, 1.0f, 0.7f);

	g_xEngine.EditorAutomation().AddStep_CreateUIRect("Crosshair_Bottom");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("Crosshair_Bottom", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("Crosshair_Bottom", 0.0f, 10.0f);
	g_xEngine.EditorAutomation().AddStep_SetUISize("Crosshair_Bottom", 2.0f, 8.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("Crosshair_Bottom", 1.0f, 1.0f, 1.0f, 0.7f);

	g_xEngine.EditorAutomation().AddStep_CreateUIRect("Crosshair_Left");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("Crosshair_Left", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("Crosshair_Left", -10.0f, 0.0f);
	g_xEngine.EditorAutomation().AddStep_SetUISize("Crosshair_Left", 8.0f, 2.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("Crosshair_Left", 1.0f, 1.0f, 1.0f, 0.7f);

	g_xEngine.EditorAutomation().AddStep_CreateUIRect("Crosshair_Right");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("Crosshair_Right", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("Crosshair_Right", 10.0f, 0.0f);
	g_xEngine.EditorAutomation().AddStep_SetUISize("Crosshair_Right", 8.0f, 2.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("Crosshair_Right", 1.0f, 1.0f, 1.0f, 0.7f);

	g_xEngine.EditorAutomation().AddStep_CreateUIText("AmmoText", "30 / 90");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("AmmoText", static_cast<int>(Zenith_UI::AnchorPreset::BottomRight));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("AmmoText", -100.0f, -40.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("AmmoText", 32.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("AmmoText", 1.0f, 1.0f, 1.0f, 1.0f);

	// Smoke runner — attached BEFORE save so it ends up in the saved scene.
	if (RenderTest_IsSmokeMode())
	{
		g_xEngine.EditorAutomation().AddStep_CreateEntity("RenderTestSmokeRunner");
		g_xEngine.EditorAutomation().AddStep_SetEntityTransient(false);
		g_xEngine.EditorAutomation().AddStep_AttachScript("RenderTest_SmokeRunner");
	}

	g_xEngine.EditorAutomation().AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/RenderTest" ZENITH_SCENE_EXT);
	g_xEngine.EditorAutomation().AddStep_UnloadScene();

	g_xEngine.EditorAutomation().AddStep_LoadInitialScene(&Project_LoadInitialScene);

	// Grass placement needs the loaded terrain's physics mesh; the editor
	// defers the scene load to the next Update, so apply the painted density
	// map one automation step (== one frame) later.
	g_xEngine.EditorAutomation().AddStep_Custom(&RenderTest_ApplyGrassDensityFromDisk);

	// Smoke play-mode entry LAST — by this step the deferred load has
	// completed, so the smoke runner's first tick probes the fully loaded
	// terrain (see the note in Project_LoadInitialScene).
	if (RenderTest_IsSmokeMode())
	{
		g_xEngine.EditorAutomation().AddStep_Custom(&RenderTest_EnterSmokePlayMode);
	}
}
#endif

// Painted grass: load the terrain editor's saved GrassDensity.ztxtr (R32,
// 1024^2) into the grass system and rebuild blade placement from the terrain's
// physics mesh. Windowed only — the grass GPU buffers don't exist headless.
static void RenderTest_ApplyGrassDensityFromDisk()
{
	if (Zenith_CommandLine::IsHeadless())
	{
		return;
	}
	const std::string strPath = std::string(GAME_ASSETS_DIR) + "Textures/Terrain/GrassDensity" ZENITH_TEXTURE_EXT;
	if (!std::filesystem::exists(strPath))
	{
		return;
	}

	Zenith_DataStream xIn;
	xIn.ReadFromFile(strPath.c_str());
	int32_t iWidth = 0, iHeight = 0, iDepth = 0;
	TextureFormat eFormat = TEXTURE_FORMAT_NONE;
	size_t ulDataSize = 0;
	xIn >> iWidth;
	xIn >> iHeight;
	xIn >> iDepth;
	xIn.ReadData(&eFormat, sizeof(eFormat));
	xIn >> ulDataSize;
	if (eFormat != TEXTURE_FORMAT_R32_SFLOAT || iWidth <= 0 || iHeight <= 0 ||
		ulDataSize != static_cast<size_t>(iWidth) * iHeight * sizeof(float))
	{
		Zenith_Warning(LOG_CATEGORY_TERRAIN, "[RenderTest] GrassDensity%s has unexpected layout - skipping grass", ZENITH_TEXTURE_EXT);
		return;
	}
	std::vector<float> xDensity(static_cast<size_t>(iWidth) * iHeight);
	xIn.ReadData(xDensity.data(), ulDataSize);

	Zenith_TerrainComponent* pxTerrain = nullptr;
	g_xEngine.Scenes().QueryAllScenes<Zenith_TerrainComponent>().ForEach(
		[&pxTerrain](Zenith_EntityID, Zenith_TerrainComponent& xTerrain)
		{
			if (pxTerrain == nullptr)
			{
				pxTerrain = &xTerrain;
			}
		});
	if (pxTerrain == nullptr || !pxTerrain->HasPhysicsGeometry())
	{
		Zenith_Warning(LOG_CATEGORY_TERRAIN, "[RenderTest] Grass density present but %s - grass skipped",
			pxTerrain == nullptr ? "no terrain component found" : "terrain has no physics geometry");
		return;
	}

	g_xEngine.Grass().SetDensityMap(xDensity.data(), static_cast<u_int>(iWidth), static_cast<u_int>(iHeight), 4096.0f);
	g_xEngine.Grass().GenerateFromTerrain(pxTerrain->GetPhysicsMeshGeometry());
}

void Project_LoadInitialScene()
{
	g_xEngine.Scenes().RegisterSceneBuildIndex(0, GAME_ASSETS_DIR "Scenes/RenderTest" ZENITH_SCENE_EXT);
	g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);

#ifndef ZENITH_TOOLS
	// Painted-density grass meadows (terrain-editor authored). Runtime builds
	// load the scene synchronously, so the terrain's physics mesh exists here.
	// Tools builds defer the load to the next editor Update — there the grass
	// is applied by the automation step queued after LOAD_INITIAL_SCENE.
	RenderTest_ApplyGrassDensityFromDisk();
#endif

	// Tools builds: smoke play-mode entry is queued as the FINAL automation
	// step (RenderTest_EnterSmokePlayMode) instead of being set here. This
	// function runs on the frame the scene load is QUEUED (the load itself is
	// deferred to the next editor Update) — setting Playing here let the
	// smoke runner attached to the BUILD scene tick once against the
	// half-initialized authoring terrain (culling uninitialized, LOW LOD
	// unstreamed) and emit spurious RENDERTEST_SMOKE_FAIL lines before the
	// real scene arrived.
}

// Input-simulator tests for RenderTest_FollowCamera + RenderTest_PlayerBehaviour.
// Included here (rather than from a Zenith engine TU) so the auto-registered
// ZENITH_TEST cases land in the RenderTest binary's test runner.
#include "RenderTest/Components/RenderTest_PlayerBehaviour.Tests.inl"
