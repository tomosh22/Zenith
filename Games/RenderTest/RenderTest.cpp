#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_MaterialParamTable.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_MeshGeometryAsset.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Maths/Zenith_Maths.h"
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
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "ZenithECS/Zenith_ComponentMeta.h"
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

// Header-only game components: included into this TU so the
// ZENITH_REGISTER_COMPONENT thunks below can name the complete types.
#include "RenderTest/Components/RenderTest_PlayerComponent.h"
#include "RenderTest/Components/RenderTest_FollowCameraComponent.h"

// Tennis-court testbed (third platform): court/net/ball/NPCs/match, spawned
// procedurally post-load (see RenderTest_Tennis.cpp). Component headers are
// included here so the ZENITH_REGISTER_COMPONENT thunks below name the types.
#include "RenderTest/RenderTest_Tennis.h"
#include "RenderTest/Components/RenderTest_TennisMatchComponent.h"
#include "RenderTest/Components/RenderTest_TennisPlayerComponent.h"
#include "RenderTest/Components/RenderTest_TennisAgentComponent.h"
#include "RenderTest/Components/RenderTest_GraphNodes.h"
#include "Scripting/Zenith_GraphBuilder.h"

// FPS gun pickup/drop testbed (guns lying on the spawn platform; player picks
// them up, shoots, drops; arm IK keeps the hands on the gun). Header included so
// the ZENITH_REGISTER_COMPONENT thunk below names the type; the spawn lives in
// RenderTest_Guns.cpp.
#include "RenderTest/RenderTest_Guns.h"
#include "RenderTest/Components/RenderTest_GunComponent.h"

// Jetpack testbed (backpack worn on the player's back; Space fires it to lift
// the player + steer mid-air; particle jet trail). Header included so the
// ZENITH_REGISTER_COMPONENT thunk below names the type; the spawn lives in
// RenderTest_Jetpack.cpp.
#include "RenderTest/RenderTest_Jetpack.h"
#include "RenderTest/Components/RenderTest_JetpackComponent.h"

// Material-showcase testbed (platform + grid of every shape × a wide material matrix,
// each shape a saved entity with a static per-primitive collider). Assets baked to
// disk by RenderTest_ExportMaterialShowcaseAssets; entities authored by
// RenderTest_AuthorMaterialShowcase.
#include "RenderTest/RenderTest_MaterialShowcase.h"

// Per-launch bootstrap (CLI/tuning state + post-terrain grass apply) now that the
// testbeds are baked into the saved scene. Header-only component; also declares
// RenderTest_TryApplyGrassDensityFromDisk (defined in this TU).
#include "RenderTest/Components/RenderTest_BootstrapComponent.h"

#ifdef ZENITH_TOOLS
#include "EntityComponent/Zenith_ComponentEditorRegistry.h"
#include "Editor/Zenith_EditorAutomation.h"
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_UndoSystem.h"
#include "Editor/TerrainEditor/Zenith_TerrainEditor.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#endif

// Grass system: painted-density meadows rebuilt from GrassDensity.ztxtr at
// scene load (windowed, all configs).
#include "Flux/Vegetation/Flux_GrassImpl.h"

// Owner for the shared vertex-colour testbed material created during the tools asset
// export. File-scope (not in the ZENITH_TOOLS block) so Project_Shutdown can release
// it before the asset registry tears down; in non-tools it stays an empty handle.
static MaterialHandle s_xTestbedVtxColorMaterial;

// Campus origin. RenderTest's entire "campus" — the player, the three co-planar
// platforms (IK deck, tennis court, material showcase), the ring of hills/grass/trees,
// and every demo/capture vantage — sits at the CENTRE of the 4096 m terrain
// (Flux_TerrainConfig::TERRAIN_SIZE * 0.5), not the (256,256) corner it was historically
// authored around. fCAMPUS_C{X,Z} is that centre; fCAMPUS_SHIFT (= centre - 256, the old
// anchor) translates the legacy 256-anchored local offsets (IK cubes, grass meadows,
// scattered feature hills) onto it so the whole layout moves rigidly with no relative
// drift. Bumping the terrain marker re-bakes the heightfield with the features at the
// new centre, leaving the old corner as plain procedural ground.
static constexpr float fCAMPUS_CX    = 2048.0f;
static constexpr float fCAMPUS_CZ    = 2048.0f;
static constexpr float fCAMPUS_SHIFT = 1792.0f;   // = fCAMPUS_CX - 256.0f (legacy anchor)

// Defined alongside Project_LoadInitialScene below; also queued as a
// post-scene-load automation step in tools builds (ZENITH_TOOLS-only — the
// runtime/Playing grass apply is driven by RenderTest_BootstrapComponent).
#ifdef ZENITH_TOOLS
static void RenderTest_ApplyGrassDensityFromDisk();
#endif

// Material-showcase testbed: the platform + grid of every shape × material matrix are
// now baked to disk + authored into the saved scene (with static per-primitive
// colliders) like the other testbeds — see RenderTest_MaterialShowcase.{h,cpp}.

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
		Flux_ParticleEmitterConfig* m_pxMuzzleConfig = nullptr;
		Flux_ParticleEmitterConfig* m_pxJetTrailConfig = nullptr;
	};

	static RenderTestResources g_xResources;
	RenderTestResources& Resources() { return g_xResources; }
}

static bool s_bResourcesInitialized = false;
static uint32_t s_uRenderTestSmokeFrameLimit = 240;

#ifdef ZENITH_TOOLS
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
#endif

// True if any CLI arg STARTS WITH szPrefix (so "--rendertest-gun-showcase=rifle"
// matches "--rendertest-gun-showcase"). RenderTest_HasCommandLineFlag is exact-match.
// [[maybe_unused]]: only referenced from the ZENITH_TOOLS automation path.
[[maybe_unused]] static bool RenderTest_HasCommandLineFlagPrefix(const char* szPrefix)
{
#ifdef ZENITH_WINDOWS
	const size_t ulLen = std::strlen(szPrefix);
	for (int i = 1; i < __argc; i++)
	{
		if (std::strncmp(__argv[i], szPrefix, ulLen) == 0)
			return true;
	}
#else
	(void)szPrefix;
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

#ifdef ZENITH_TOOLS
static bool RenderTest_IsSmokeMode()
{
	return RenderTest_HasCommandLineFlag("--rendertest-smoke");
}
#endif

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

	Zenith_AnimatorComponent* pxAnimator = xPlayer.TryGetComponent<Zenith_AnimatorComponent>();
	if (pxAnimator == nullptr)
	{
		Zenith_Error(LOG_CATEGORY_ANIMATION, "RENDERTEST_SMOKE_FAIL: player has no AnimatorComponent at frame %u", uFrame);
		return false;
	}

	Zenith_AnimatorComponent& xAnimator = *pxAnimator;
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

	Zenith_ModelComponent* pxModel = xPlayer.TryGetComponent<Zenith_ModelComponent>();
	if (pxModel == nullptr) return bPass;
	Zenith_ModelComponent& xModel = *pxModel;
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
	Zenith_ModelComponent* pxModel = xPlayer.TryGetComponent<Zenith_ModelComponent>();
	if (pxModel == nullptr)
	{
		return RenderTest_LogIKSmokeState(uFrame);
	}
	Zenith_ModelComponent& xModel = *pxModel;
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

class RenderTest_SmokeRunnerComponent
{
public:
	RenderTest_SmokeRunnerComponent() = delete;
	RenderTest_SmokeRunnerComponent(Zenith_Entity& xParentEntity)
		: m_xParentEntity(xParentEntity)
	{
	}

	void OnStart()
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN,
			"[RenderTestSmoke] starting bounded terrain smoke run frameLimit=%u",
			s_uRenderTestSmokeFrameLimit);
	}

	void OnUpdate(float)
	{
		m_uFrame++;

		if (m_uFrame == 1 || (m_uFrame % 60) == 0 || m_uFrame >= s_uRenderTestSmokeFrameLimit)
		{
			m_bPassed = RenderTest_LogTerrainSmokeState(m_uFrame) && m_bPassed;

			// IK chain registration / no-NaN check. Warm-up gate: chain
			// registration runs in the player's OnStart, which fires after the
			// pending-start components are dispatched — give it 60 frames to
			// settle before validating.
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
			TeleportPlayerTo(Zenith_Maths::Vector3(244.0f + fCAMPUS_SHIFT, 49.5f, 268.0f + fCAMPUS_SHIFT));
		}
		if (m_uFrame == 120)
		{
			// On the NW cube. Add ankle height (0.05m) to expected surface Y.
			m_bPassed = RenderTest_LogIKOnSurfaceState(m_uFrame, 49.05f + 0.05f, 0.20f) && m_bPassed;
		}
		if (m_uFrame == 150)
		{
			TeleportPlayerTo(Zenith_Maths::Vector3(fCAMPUS_CX, 49.5f, fCAMPUS_CZ));
		}
		if (m_uFrame == 200)
		{
			// On the central platform. Top Y=48.75 plus ankle height.
			m_bPassed = RenderTest_LogIKOnSurfaceState(m_uFrame, 48.75f + 0.05f, 0.20f) && m_bPassed;
		}

		// Decal smoke: fire several shots straight down at the platform between
		// frames 210-220. Each shot exercises the full chain (raycast → decal
		// spawn → CPU pool → GPU upload → render-graph passes), and the
		// per-stage diagnostic logs in Flux_Decals + RenderTest_PlayerComponent::Shoot
		// pinpoint where the chain breaks if decals don't appear in the frame.
		if (m_uFrame == 210 || m_uFrame == 215 || m_uFrame == 220)
		{
			RenderTest_PlayerComponent* pxPlayer = RenderTest_PlayerComponent::GetActiveInstance();
			if (pxPlayer)
			{
				Zenith_Log(LOG_CATEGORY_GAMEPLAY,
					"[DECAL_SMOKE] frame %u: triggering downward shot", m_uFrame);
				pxPlayer->TriggerShootDownwardForTest();
			}
			else
			{
				Zenith_Log(LOG_CATEGORY_GAMEPLAY,
					"[DECAL_SMOKE] frame %u: PlayerComponent instance not registered yet", m_uFrame);
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

	// Component contract. The smoke runner is pure runtime probe state; only
	// the version tag persists.
	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		const u_int uVersion = 1;
		xStream << uVersion;
	}
	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		u_int uVersion = 0;
		xStream >> uVersion;
	}
#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel()
	{
		ImGui::Text("Frame: %u", m_uFrame);
		ImGui::Text("Passed so far: %s", m_bPassed ? "true" : "false");
	}
#endif

private:
	// Teleport the player's physics body to a target world position. Used by the
	// IK foot-placement validation to drop the player onto a known cube without
	// the 200+ frames of walking required at m_fMoveSpeed=5.0f.
	void TeleportPlayerTo(const Zenith_Maths::Vector3& xTarget)
	{
		Zenith_Entity xPlayer = RenderTest_FindPlayerEntity();
		Zenith_ColliderComponent* pxCollider = xPlayer.TryGetComponent<Zenith_ColliderComponent>();
		if (pxCollider == nullptr) return;
		g_xEngine.Physics().TeleportBody(pxCollider->GetBodyID(), xTarget);
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

	Zenith_Entity m_xParentEntity;

	uint32_t m_uFrame = 0;
	bool m_bPassed = true;
	Zenith_Vector<RenderTest_ResidencySnapshot> m_axSnapshotT0;
	Zenith_Vector<RenderTest_ResidencySnapshot> m_axSnapshotT1;
	Zenith_Vector<RenderTest_ResidencySnapshot> m_axSnapshotT2;
};

// Component-meta registration (serialization + lifecycle dispatch). These MUST
// go through the ZENITH_REGISTER_COMPONENT static-init thunks, NOT direct
// RegisterComponent calls from Project_RegisterGameComponents: the engine
// seals the meta registry (EnsureInitialized -> Finalize, which builds the
// sorted dispatch list) in Zenith_Engine::InitialiseECS, BEFORE
// Project_RegisterGameComponents runs in InitialiseProject. A post-seal
// RegisterComponent call lands in the name map but never in the sorted list,
// so the component would silently never serialize nor receive OnUpdate. The
// thunks enqueue at static-init and are drained by the boot-time
// EnsureInitialized, landing the types in the sealed sorted list. Dead-strip
// safety: this TU defines Project_GetName etc., so its .obj is always linked.
// Orders 100+ keep game components after every engine built-in (Transform=0
// ... ParticleEmitter=85, AIAgent=90) — in particular the player's OnUpdate
// runs after the Animator's component update, matching the old script phase.
ZENITH_REGISTER_COMPONENT(RenderTest_FollowCameraComponent, "RenderTestFollowCamera", 100u)
ZENITH_REGISTER_COMPONENT(RenderTest_PlayerComponent, "RenderTestPlayer", 101u)
ZENITH_REGISTER_COMPONENT(RenderTest_SmokeRunnerComponent, "RenderTestSmokeRunner", 102u)
ZENITH_REGISTER_COMPONENT(RenderTest_GunComponent, "RenderTestGun", 110u)
ZENITH_REGISTER_COMPONENT(RenderTest_TennisPlayerComponent, "RenderTestTennisPlayer", 120u)
ZENITH_REGISTER_COMPONENT(RenderTest_TennisMatchComponent, "RenderTestTennisMatch", 130u)
ZENITH_REGISTER_COMPONENT(RenderTest_TennisAgentComponent, "RenderTestTennisAgent", 135u)
ZENITH_REGISTER_COMPONENT(RenderTest_JetpackComponent, "RenderTestJetpack", 140u)
ZENITH_REGISTER_COMPONENT(RenderTest_BootstrapComponent, "RenderTestBootstrap", 150u)

// (ExportColoredTexture/CreateFlatColorMaterial used to live here for the
// flat-teal player material — the StickFigure .zmodel now bundles its own
// painted-atlas body material, so the helpers are gone.)

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
		auto xhModel = Zenith_AssetRegistry::Create<Zenith_ModelAsset>();
		Zenith_ModelAsset* pxModel = xhModel.GetDirect();
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

	// GenerateStickFigureAssets exports the canonical .zmodel (mesh + skeleton
	// + painted-atlas body material) every boot, so this fallback only fires
	// when the generator was skipped (--skip-tool-exports).
	if (!std::filesystem::exists(strModelPath))
	{
		auto xhModel = Zenith_AssetRegistry::Create<Zenith_ModelAsset>();
		Zenith_ModelAsset* pxModel = xhModel.GetDirect();
		pxModel->SetName("StickFigure");
		pxModel->SetSkeletonPath(strSkeletonPath);

		Zenith_Vector<std::string> xMaterials;
		xMaterials.PushBack("engine:Meshes/StickFigure/StickFigure_Body.zmtrl");
		pxModel->AddMeshByPath(strMeshAssetPath, xMaterials);
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
	xHandle = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
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

	RenderTest::Resources().m_xCubeMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	RenderTest::Resources().m_xCubeMaterial.GetDirect()->SetName("RenderTestCubeMaterial");
	RenderTest::Resources().m_xCubeMaterial.GetDirect()->SetDiffuseTexture(g_xEngine.FluxGraphics().m_xGridTexture);
	// (The old flat-teal player material is gone — the StickFigure .zmodel now
	// bundles its own painted-atlas body material.)

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

	// --- Jetpack jet-trail particle config ---
	// Continuous, additive, hot-flame-to-smoke exhaust. The jetpack emitter's
	// position/direction are overridden per-frame by the player while thrusting;
	// here we only set the look (the player toggles SetEmitting).
	if (!RenderTest::Resources().m_pxJetTrailConfig)
	{
		RenderTest::Resources().m_pxJetTrailConfig = new Flux_ParticleEmitterConfig();
		RenderTest::Resources().m_pxJetTrailConfig->m_fSpawnRate          = 90.0f;     // dense continuous stream
		RenderTest::Resources().m_pxJetTrailConfig->m_uBurstCount         = 0;
		RenderTest::Resources().m_pxJetTrailConfig->m_uMaxParticles       = 256;
		RenderTest::Resources().m_pxJetTrailConfig->m_fLifetimeMin        = 0.22f;
		RenderTest::Resources().m_pxJetTrailConfig->m_fLifetimeMax        = 0.50f;
		RenderTest::Resources().m_pxJetTrailConfig->m_fSpawnRadius        = 0.03f;
		RenderTest::Resources().m_pxJetTrailConfig->m_xEmitDirection      = Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f);
		RenderTest::Resources().m_pxJetTrailConfig->m_fSpreadAngleDegrees = 12.0f;
		RenderTest::Resources().m_pxJetTrailConfig->m_fSpeedMin           = 6.0f;
		RenderTest::Resources().m_pxJetTrailConfig->m_fSpeedMax           = 11.0f;
		RenderTest::Resources().m_pxJetTrailConfig->m_xGravity            = Zenith_Maths::Vector3(0.0f, -2.0f, 0.0f);
		RenderTest::Resources().m_pxJetTrailConfig->m_fDrag              = 2.5f;
		RenderTest::Resources().m_pxJetTrailConfig->m_xColorStart         = { 1.0f, 0.85f, 0.35f, 1.0f };  // hot yellow-orange
		RenderTest::Resources().m_pxJetTrailConfig->m_xColorEnd           = { 0.5f, 0.12f, 0.02f, 0.0f };  // dark red, fades out
		RenderTest::Resources().m_pxJetTrailConfig->m_fSizeStart          = 0.18f;
		RenderTest::Resources().m_pxJetTrailConfig->m_fSizeEnd            = 0.45f;   // expands as it cools (smoke)
		RenderTest::Resources().m_pxJetTrailConfig->m_fTurbulence         = 1.5f;
		RenderTest::Resources().m_pxJetTrailConfig->m_bAdditiveBlending   = true;
		RenderTest::Resources().m_pxJetTrailConfig->m_bUseGPUCompute      = false;
		Flux_ParticleEmitterConfig::Register("RenderTest_JetTrail", RenderTest::Resources().m_pxJetTrailConfig);
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
// v3: ring of 8 hills around the campus + dense grass on them + plateau
// flatten widened 90->100m (covers the lowered tennis court + showcase).
// v4: hills now ENTIRELY surround the spawn — a continuous 24-dab inner ring
// (r=170) + a taller 18-dab outer ring (r=255), both grassed + forested.
// v5: hills made LOWER + GENTLER (peaks 74/92 over wider brush 80/95 → ~27/36deg
// flanks, was 104/134 over 62/78 → ~53deg) at pushed-out radii 185/265.
// v6: whole campus (+ its hills/grass/trees + scattered feature hills) recentred from
// the (256,256) corner to the terrain centre (2048,2048) — every world XZ shifted +1792.
static const char* sk_szTerrainProcMarkerRel = "Terrain/terrain_proc_v6.marker";

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
		// Use the single .ztxtr parser (legacy/v1/v2 aware) rather than hand-
		// parsing the header — the source roughness/metallic maps are BC1 and now
		// ship as v2 mip chains. mip 0 is the first bytes of the returned buffer,
		// which is all RenderTest_DecodeBC1Image reads.
		Flux_SurfaceInfo xInfo;
		Zenith_Vector<uint8_t> xBytes;
		Zenith_Status xStatus = Zenith_TextureAsset::LoadCPUData(strPath, xInfo, xBytes);
		if (!xStatus.IsOk() || xInfo.m_eFormat != TEXTURE_FORMAT_BC1_RGB_UNORM)
		{
			Zenith_Error(LOG_CATEGORY_TERRAIN,
				"[RenderTest] %s is not BC1_RGB (got fmt=%d) — RM packer expects user PBR maps to be BC1.",
				strPath.c_str(), static_cast<int>(xInfo.m_eFormat));
			return {};
		}
		iWidthOut = static_cast<int32_t>(xInfo.m_uWidth);
		iHeightOut = static_cast<int32_t>(xInfo.m_uHeight);
		return RenderTest_DecodeBC1Image(xBytes.GetDataPointer(), iWidthOut, iHeightOut);
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

void Project_RegisterGameComponents()
{
	// Component-meta registration happens via the ZENITH_REGISTER_COMPONENT
	// thunks next to the class definitions (see the comment there for why it
	// cannot live here). This hook only mirrors the components into the editor
	// "Add Component" registry, which is append-any-time (no seal) and is what
	// AddStep_AddComponent resolves display names against.
#ifdef ZENITH_TOOLS
	Zenith_ComponentEditorRegistry& xEditorRegistry = Zenith_ComponentEditorRegistry::Get();
	xEditorRegistry.RegisterComponent<RenderTest_FollowCameraComponent>("RenderTestFollowCamera");
	xEditorRegistry.RegisterComponent<RenderTest_PlayerComponent>("RenderTestPlayer");
	xEditorRegistry.RegisterComponent<RenderTest_SmokeRunnerComponent>("RenderTestSmokeRunner");
	xEditorRegistry.RegisterComponent<RenderTest_GunComponent>("RenderTestGun");
	xEditorRegistry.RegisterComponent<RenderTest_TennisPlayerComponent>("RenderTestTennisPlayer");
	xEditorRegistry.RegisterComponent<RenderTest_TennisMatchComponent>("RenderTestTennisMatch");
	xEditorRegistry.RegisterComponent<RenderTest_TennisAgentComponent>("RenderTestTennisAgent");
	xEditorRegistry.RegisterComponent<RenderTest_JetpackComponent>("RenderTestJetpack");
	xEditorRegistry.RegisterComponent<RenderTest_BootstrapComponent>("RenderTestBootstrap");
#endif

	// Behaviour-graph node library (tennis brain verbs + player action verbs).
	// The graph-node registry is append-any-time (not sealed like the
	// component meta registry), so a plain call here works in every config.
	RenderTest_RegisterGraphNodes();

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
	if (RenderTest::Resources().m_pxJetTrailConfig)
	{
		Flux_ParticleEmitterConfig::Unregister("RenderTest_JetTrail");
		delete RenderTest::Resources().m_pxJetTrailConfig;
		RenderTest::Resources().m_pxJetTrailConfig = nullptr;
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

	// Shared testbed material created during the tools asset export (empty in
	// non-tools — clearing is a safe no-op there).
	s_xTestbedVtxColorMaterial = MaterialHandle{};

	// Release the tennis-testbed material/texture handles before the registry
	// tears down (otherwise their static destructors assert on a freed registry).
	RenderTest_TennisShutdown();

	// Likewise the gun-testbed material handle.
	RenderTest_GunsShutdown();

	// Likewise the jetpack-testbed material handle.
	RenderTest_JetpackShutdown();

	// Likewise the material-showcase material/model handles.
	RenderTest_MaterialShowcaseShutdown();
}

void Project_LoadInitialScene();

#ifdef ZENITH_TOOLS
// Stable on-disk path of the shared vertex-colour testbed material (white base; the
// jetpack / guns / racket / net-tape meshes show their per-vertex colour through it).
static const std::string& RenderTest_TestbedVtxColorMaterialPath()
{
	static const std::string s = std::string(GAME_ASSETS_DIR) + "Materials/RenderTest/TestbedVtxColor" ZENITH_MATERIAL_EXT;
	return s;
}

// Bake every testbed mesh / material / texture to disk so the authored scene can
// LoadModel them in ANY build. CPU-only; skipped for headless and --skip-tool-exports
// runs (no asset mutation there). Overwrites every tools run so geometry/texture
// edits always propagate (the GenerateStickFigureAssets policy).
static void GenerateRenderTestTestbedAssets()
{
	if (Zenith_CommandLine::IsHeadless() || RenderTest_HasCommandLineFlag("--skip-tool-exports"))
	{
		return;
	}

	const std::string& strMatPath = RenderTest_TestbedVtxColorMaterialPath();
	std::filesystem::create_directories(std::filesystem::path(strMatPath).parent_path());
	auto xhMat = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	Zenith_MaterialAsset* pxMat = xhMat.GetDirect();
	pxMat->SetName("RenderTest_TestbedVtxColor");
	pxMat->SetBaseColor(Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f));
	pxMat->SetRoughness(0.45f);
	pxMat->SetMetallic(0.55f);
	pxMat->SaveToFile(strMatPath);
	s_xTestbedVtxColorMaterial.Set(pxMat);

	RenderTest_ExportJetpackAssets(strMatPath.c_str());
	RenderTest_ExportGunAssets(strMatPath.c_str());
	RenderTest_ExportTennisAssets(strMatPath.c_str());

	// Material showcase builds its own per-cell materials/textures (no shared vtx-colour
	// material), so it takes no path argument.
	RenderTest_ExportMaterialShowcaseAssets();
}

void Project_InitializeResources()
{
	InitializeRenderTestResources();
	// Export the testbed assets BEFORE automation runs, so AddStep_LoadModel resolves
	// them when the authored scene is (re)built and saved.
	GenerateRenderTestTestbedAssets();
}

// --rendertest-open-terrain-editor: queued as a final automation step (no
// smoke mode, editor stays Stopped) so an external capture harness can park
// the OS cursor over the viewport and screenshot the interactive brush
// indicator decal following it across the terrain.
static void RenderTest_OpenTerrainEditorForCursor()
{
	Zenith_EntityID uTerrain = INVALID_ENTITY_ID;
	g_xEngine.Scenes().QueryAllScenes<Zenith_TerrainComponent>().ForEach(
		[&uTerrain](Zenith_EntityID uEntity, Zenith_TerrainComponent&)
		{
			if (uTerrain == INVALID_ENTITY_ID)
			{
				uTerrain = uEntity;
			}
		});
	if (uTerrain == INVALID_ENTITY_ID)
	{
		Zenith_Error(LOG_CATEGORY_TERRAIN, "[RenderTest] open-terrain-editor: no terrain entity found");
		return;
	}
	g_xEngine.Editor().OpenTerrainEditor(uTerrain);
	Zenith_TerrainEditor& xTE = g_xEngine.TerrainEditor();
	xTE.m_xBrush.m_eTool = Zenith_TerrainBrushTool::TreePaint;
	xTE.m_xBrush.m_fRadius = 15.0f;

	// Plant a demo grove around the plateau with the tree brush so the
	// capture harness can photograph instanced wind-swayed trees without
	// scripted mouse drags. Coords are the legacy 256-anchored layout shifted
	// onto the recentred campus (fCAMPUS_SHIFT).
	xTE.m_xBrush.m_uTreesPerDab = 6;
	xTE.ApplyBrushDab(Zenith_TerrainBrushTool::TreePaint, 215.0f + fCAMPUS_SHIFT, 300.0f + fCAMPUS_SHIFT, 26.0f, 1.0f, 0.0f);
	xTE.ApplyBrushDab(Zenith_TerrainBrushTool::TreePaint, 300.0f + fCAMPUS_SHIFT, 305.0f + fCAMPUS_SHIFT, 26.0f, 1.0f, 0.0f);
	xTE.ApplyBrushDab(Zenith_TerrainBrushTool::TreePaint, 256.0f + fCAMPUS_SHIFT, 340.0f + fCAMPUS_SHIFT, 30.0f, 1.0f, 0.0f);
	xTE.ApplyBrushDab(Zenith_TerrainBrushTool::TreePaint, 190.0f + fCAMPUS_SHIFT, 255.0f + fCAMPUS_SHIFT, 22.0f, 1.0f, 0.0f);
	xTE.ApplyBrushDab(Zenith_TerrainBrushTool::TreePaint, 325.0f + fCAMPUS_SHIFT, 255.0f + fCAMPUS_SHIFT, 22.0f, 1.0f, 0.0f);

	// Elevate the EDITOR camera (authoritative for the Stopped-mode view)
	// so cursor rays spread across the plateau instead of clustering a few
	// metres ahead of a near-ground vantage. Mark it initialized so the
	// post-automation game-camera sync doesn't snap it back.
	Zenith_EditorCameraState& xCamera = g_xEngine.Editor().m_xEditorState.m_xCamera;
	if (RenderTest_HasCommandLineFlag("--rendertest-tree-closeup"))
	{
		// Eye-level vantage inside the grove for sway/foliage close-ups.
		xCamera.m_xPosition = { 213.0f + fCAMPUS_SHIFT, 53.5f, 286.0f + fCAMPUS_SHIFT };
		xCamera.m_fPitch = -0.06;
		xCamera.m_fYaw = 0.0;
	}
	else
	{
		xCamera.m_xPosition = { 256.0f + fCAMPUS_SHIFT, 95.0f, 215.0f + fCAMPUS_SHIFT };
		xCamera.m_fPitch = -0.55;
		xCamera.m_fYaw = 0.0;
	}
	xCamera.m_bInitialized = true;

	Zenith_Log(LOG_CATEGORY_TERRAIN, "[RenderTest] TERRAIN_EDITOR_CURSOR_READY (grove planted: %u trunks)",
		g_xEngine.TerrainEditor().m_xBrush.m_uTreesPerDab);
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

// Stable per-type gun entity name ("Gun_<TypeName>"). Function-local-static storage
// so the c_str() is safe to hand to AddStep_CreateEntity (which stores the pointer).
// Shared by the authoring + the per-entity config pass so the names always match.
static const char* RenderTest_GunEntityName(RenderTest_Guns::GunType eType)
{
	static std::string s_aNames[static_cast<int>(RenderTest_Guns::GunType::COUNT)];
	static bool s_bInit = false;
	if (!s_bInit)
	{
		for (int i = 0; i < static_cast<int>(RenderTest_Guns::GunType::COUNT); ++i)
		{
			s_aNames[i] = std::string("Gun_") + RenderTest_Guns::GetSpec(static_cast<RenderTest_Guns::GunType>(i)).m_szName;
		}
		s_bInit = true;
	}
	return s_aNames[static_cast<int>(eType)].c_str();
}

// AddStep_Custom callback (queued after the testbed CreateEntity steps, before
// SaveScene): stamp each authored entity's per-instance config — gun type/ammo and
// tennis NPC side — through the existing Init() setters, so SaveScene serializes it
// (the components persist it via their v2 streams). Runs in the authoring scene.
static void RenderTest_ApplyTestbedEntityConfig()
{
	Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
	if (!pxSceneData)
	{
		return;
	}

	// Guns: rebuild the full spec from the per-type entity.
	for (int i = 0; i < static_cast<int>(RenderTest_Guns::GunType::COUNT); ++i)
	{
		const RenderTest_Guns::GunType eType = static_cast<RenderTest_Guns::GunType>(i);
		Zenith_Entity xGun = pxSceneData->FindEntityByName(RenderTest_GunEntityName(eType));
		if (RenderTest_GunComponent* pxGun = xGun.TryGetComponent<RenderTest_GunComponent>())
		{
			pxGun->Init(RenderTest_Guns::GetSpec(eType));
		}
	}

	// Tennis NPCs: set their baseline side (derives facing) + the AI tick rate
	// (serialized on the AIAgent, so the rate persists into the saved scene).
	auto ConfigNpc = [pxSceneData](const char* szName, bool bNear)
	{
		Zenith_Entity xNpc = pxSceneData->FindEntityByName(szName);
		if (!xNpc.IsValid())
			return;
		if (RenderTest_TennisPlayerComponent* pxTennisPlayer = xNpc.TryGetComponent<RenderTest_TennisPlayerComponent>())
			pxTennisPlayer->Init(bNear);
		if (Zenith_AIAgentComponent* pxAgent = xNpc.TryGetComponent<Zenith_AIAgentComponent>())
			pxAgent->SetUpdateInterval(0.08f);
	};
	ConfigNpc("Tennis_NPC_Near", true);
	ConfigNpc("Tennis_NPC_Far", false);
}

// ============================================================================
// Behaviour-graph builders (W3 conversion; regenerated every tools boot via
// AddStep_GraphBuild before the scene authoring references them).
// ============================================================================

// The tennis NPC brain: the retired BT (root Selector over serve / rally /
// recover, NO RUNNING leaves) as a fully reactive graph. Driven by the
// engine ON_UPDATE dispatch at component order 60 through an authored tick
// accumulator that reproduces the retired AIAgent 0.08 s interval semantics
// EXACTLY (RNG-cadence contract, pinned by RT_TennisDeterminismDigest):
//   - RTTennisTickGate mirrors `if (!m_bEnabled) return;` - no accumulation
//     while the referee has the agent parked (freeze, not reset);
//   - AddBlackboardFloat(dt) then CompareBlackboardFloat(>= 0.08) then
//     SetBlackboardFloat(0) is accumulate -> fire -> RESET-TO-ZERO (the
//     remainder is discarded, exactly like the AIAgent; the engine Timer
//     node's subtractive carry would drift the cadence);
//   - one ON_UPDATE dispatch per frame = at most one tick per frame.
// The BT tick ran at order 90 BEFORE the same AIAgent update's nav step, so
// a tick at order 60 sees identical inputs (ball physics pre-stepped, BB +
// awareness from last frame's referee OnLateUpdate) and its SetDestination
// is consumed by the same frame's nav update at 90 - frame-for-frame parity.
static void BuildGraph_RenderTestTennisBrain(Zenith_GraphBuilder& xBuilder)
{
	using namespace RenderTest_TennisBB;
	Zenith_PropertyValue xF0;    xF0.SetFloat(0.0f);
	Zenith_PropertyValue xI0;    xI0.SetInt32(0);
	Zenith_PropertyValue xBF;    xBF.SetBool(false);
	Zenith_PropertyValue xBT;    xBT.SetBool(true);
	Zenith_PropertyValue xV0;    xV0.SetVector3(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xBuilder.Variable("tickAccum", xF0);
	xBuilder.Variable("tickDue", xBF);
	xBuilder.Variable("phaseIsServing", xBF);
	xBuilder.Variable("phaseIsLive", xBF);
	xBuilder.Variable(k_szPhase, xI0);           // POINT_PHASE_WARMUP
	xBuilder.Variable(k_szBallEpoch, xI0);
	xBuilder.Variable(k_szMySide, xI0);
	xBuilder.Variable(k_szMyPoints, xI0);
	xBuilder.Variable(k_szOppPoints, xI0);
	xBuilder.Variable(k_szIsServer, xBF);
	xBuilder.Variable(k_szServeFromDeuce, xBT);
	xBuilder.Variable(k_szIsSecondServe, xBF);
	xBuilder.Variable(k_szIsMyBall, xBF);
	xBuilder.Variable(k_szServeBallParked, xBF);
	xBuilder.Variable(k_szBallSpin, xV0);
	// BallEntity/OppEntity are runtime handles seeded by the brain shim's
	// OnStart (SeedGraphBlackboard) - deliberately NOT declared here.

	// Tick spine.
	const u_int uUpdate = xBuilder.Node("OnUpdate");
	const u_int uGateEnabled = xBuilder.Node("RTTennisTickGate");
	const u_int uAccum = xBuilder.Node("AddBlackboardFloat");
	xBuilder.ParamString(uAccum, "m_strVariable", "tickAccum");
	xBuilder.ParamBool(uAccum, "m_bScaleByDt", true);
	const u_int uDue = xBuilder.Node("CompareBlackboardFloat");
	xBuilder.ParamString(uDue, "m_strVar", "tickAccum");
	xBuilder.ParamFloat(uDue, "m_fCompareTo", 0.08f);
	xBuilder.ParamInt(uDue, "m_iOp", 3);   // greaterEqual
	xBuilder.ParamString(uDue, "m_strResultVar", "tickDue");
	const u_int uGateDue = xBuilder.Node("Gate");
	xBuilder.ParamString(uGateDue, "m_strOpenVar", "tickDue");
	const u_int uReset = xBuilder.Node("SetBlackboardFloat");
	xBuilder.ParamString(uReset, "m_strVariable", "tickAccum");
	const u_int uSelector = xBuilder.Node("Selector");
	xBuilder.ParamInt(uSelector, "m_iBranchCount", 3);
	// Defaults kept (reactive, abort-preempted): no chain in this graph ever
	// suspends (the tree had NO RUNNING leaves), so neither flag can engage.
	xBuilder.Chain(uUpdate, uGateEnabled).Chain(uGateEnabled, uAccum)
		.Chain(uAccum, uDue).Chain(uDue, uGateDue).Chain(uGateDue, uReset)
		.Chain(uReset, uSelector);

	// Pin 0 - serve: phase==SERVING && IsServer && ServeBallParked (the
	// retired IsMyServeAndBallParked condition, decomposed to engine gates)
	// -> decide -> position -> arm.
	const u_int uPhaseServe = xBuilder.Node("CompareBlackboardInt");
	xBuilder.ParamString(uPhaseServe, "m_strVar", k_szPhase);
	xBuilder.ParamInt(uPhaseServe, "m_iCompareTo", static_cast<int32_t>(RenderTest_Tennis::POINT_PHASE_SERVING));
	xBuilder.ParamString(uPhaseServe, "m_strResultVar", "phaseIsServing");
	const u_int uGateServing = xBuilder.Node("Gate");
	xBuilder.ParamString(uGateServing, "m_strOpenVar", "phaseIsServing");
	const u_int uGateServer = xBuilder.Node("Gate");
	xBuilder.ParamString(uGateServer, "m_strOpenVar", k_szIsServer);
	const u_int uGateParked = xBuilder.Node("Gate");
	xBuilder.ParamString(uGateParked, "m_strOpenVar", k_szServeBallParked);
	const u_int uDecideServe = xBuilder.Node("RTTennisDecideServe");
	const u_int uPositionServe = xBuilder.Node("RTTennisPositionForServe");
	const u_int uArmServe = xBuilder.Node("RTTennisArmServe");
	xBuilder.Edge(uSelector, 0, uPhaseServe);
	xBuilder.Chain(uPhaseServe, uGateServing).Chain(uGateServing, uGateServer)
		.Chain(uGateServer, uGateParked).Chain(uGateParked, uDecideServe)
		.Chain(uDecideServe, uPositionServe).Chain(uPositionServe, uArmServe);

	// Pin 1 - rally: phase==LIVE && IsMyBall (the retired BallIsMine's pure
	// gates) -> aware+reachable (its systems half) -> move -> decide -> arm.
	const u_int uPhaseLive = xBuilder.Node("CompareBlackboardInt");
	xBuilder.ParamString(uPhaseLive, "m_strVar", k_szPhase);
	xBuilder.ParamInt(uPhaseLive, "m_iCompareTo", static_cast<int32_t>(RenderTest_Tennis::POINT_PHASE_LIVE));
	xBuilder.ParamString(uPhaseLive, "m_strResultVar", "phaseIsLive");
	const u_int uGateLive = xBuilder.Node("Gate");
	xBuilder.ParamString(uGateLive, "m_strOpenVar", "phaseIsLive");
	const u_int uGateMyBall = xBuilder.Node("Gate");
	xBuilder.ParamString(uGateMyBall, "m_strOpenVar", k_szIsMyBall);
	const u_int uReachable = xBuilder.Node("RTTennisBallReachable");
	const u_int uMove = xBuilder.Node("RTTennisMoveToIntercept");
	const u_int uDecideShot = xBuilder.Node("RTTennisDecideShot");
	const u_int uArmSwing = xBuilder.Node("RTTennisArmSwing");
	xBuilder.Edge(uSelector, 1, uPhaseLive);
	xBuilder.Chain(uPhaseLive, uGateLive).Chain(uGateLive, uGateMyBall)
		.Chain(uGateMyBall, uReachable).Chain(uReachable, uMove)
		.Chain(uMove, uDecideShot).Chain(uDecideShot, uArmSwing);

	// Pin 2 - recover fallback (always succeeds).
	const u_int uRecover = xBuilder.Node("RTTennisRecoverToReady");
	xBuilder.Edge(uSelector, 2, uRecover);
}

// The slim player-actions graph: the discrete key/button PRESS decisions.
// Continuous holds (WASD / Shift sprint / Space jump+jetpack / RMB ADS) and
// all systems stay C++ in RenderTest_PlayerComponent / FollowCamera.
static void BuildGraph_RenderTestPlayerActions(Zenith_GraphBuilder& xBuilder)
{
	const u_int uKeyE = xBuilder.Node("OnKeyPressed");
	xBuilder.ParamInt(uKeyE, "m_iKeyCode", ZENITH_KEY_E);
	const u_int uInteract = xBuilder.Node("RTPlayerInteractGun");
	xBuilder.Chain(uKeyE, uInteract);

	const u_int uKeyR = xBuilder.Node("OnKeyPressed");
	xBuilder.ParamInt(uKeyR, "m_iKeyCode", ZENITH_KEY_R);
	const u_int uReload = xBuilder.Node("RTPlayerTryReload");
	xBuilder.Chain(uKeyR, uReload);

	const u_int uFireBtn = xBuilder.Node("OnMouseButton");
	// Defaults: LEFT button, mode 0 = pressed edge - exactly the retired
	// WasKeyPressedThisFrame(LMB) poll.
	const u_int uFire = xBuilder.Node("RTPlayerTryFire");
	xBuilder.Chain(uFireBtn, uFire);

	const u_int uKeyT = xBuilder.Node("OnKeyPressed");
	xBuilder.ParamInt(uKeyT, "m_iKeyCode", ZENITH_KEY_T);
	const u_int uCycleCam = xBuilder.Node("RTPlayerCycleTennisCam");
	xBuilder.Chain(uKeyT, uCycleCam);
}

void Project_RegisterEditorAutomationSteps()
{
	using namespace Flux_TerrainConfig;

	// Behaviour-graph assets first: authored via the builder so the attach
	// steps (player + tennis NPCs, below) resolve them, and SaveScene
	// serializes the GraphComponent slots into RenderTest.zscen.
	g_xEngine.EditorAutomation().AddStep_GraphBuild(
		RenderTest_TennisAgentComponent::kszGraphAsset, &BuildGraph_RenderTestTennisBrain);
	g_xEngine.EditorAutomation().AddStep_GraphBuild(
		"game:Graphs/RenderTest_PlayerActions.bgraph", &BuildGraph_RenderTestPlayerActions);

	// Resources (cube model + stick figure model + materials) are initialized
	// from Project_RegisterGameComponents, which runs before automation steps.

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
		// (all +fCAMPUS_SHIFT so they cluster NE of the recentred campus, not the corner)
		xAuto.AddStep_TerrainBrushStroke(iSetHeight, 760.0f + fCAMPUS_SHIFT, 420.0f + fCAMPUS_SHIFT, 220.0f, 0.9f, 120.0f);
		xAuto.AddStep_TerrainBrushStroke(iSetHeight, 1500.0f + fCAMPUS_SHIFT, 900.0f + fCAMPUS_SHIFT, 260.0f, 0.85f, 95.0f);
		xAuto.AddStep_TerrainBrushStroke(iTerrace, 1500.0f + fCAMPUS_SHIFT, 900.0f + fCAMPUS_SHIFT, 300.0f, 0.8f, 10.0f);
		xAuto.AddStep_TerrainBrushStroke(iNoise, 1100.0f + fCAMPUS_SHIFT, 1500.0f + fCAMPUS_SHIFT, 280.0f, 0.7f, 14.0f);
		xAuto.AddStep_TerrainSampleStamp(760.0f + fCAMPUS_SHIFT, 420.0f + fCAMPUS_SHIFT, 240.0f);
		xAuto.AddStep_TerrainBrushStroke(iStamp, 2100.0f + fCAMPUS_SHIFT, 600.0f + fCAMPUS_SHIFT, 240.0f, 0.9f, 0.0f);

		// Hills ENTIRELY surrounding the spawn campus (player + the 3 co-planar
		// platforms, all at deck-top 48.75): a CONTINUOUS inner ring the player
		// can't see through, plus a taller, half-step-offset outer ring for
		// layered depth. Both use deterministic per-dab peak variation (sinf of
		// the index → byte-stable) for a natural undulating skyline. Placed
		// before Erode so they weather naturally; the post-erode r=100 plateau
		// re-flatten restores the flat campus the dabs ring.
		//
		// Inner ring: 24 dabs at r=170, brush 62 → chord spacing ~44m < brush, so
		// the dabs overlap into a gapless ridge. Inner edge (170-62=108) clears
		// the widened (100m) plateau flatten by ~8m → surrounds without burying.
		const int   iINNER_HILLS = 24;
		const float fInnerR = 185.0f, fInnerBrush = 80.0f;
		for (int iHill = 0; iHill < iINNER_HILLS; ++iHill)
		{
			const float fA = glm::radians(360.0f / iINNER_HILLS * static_cast<float>(iHill));
			const float fPeak = 74.0f + 8.0f * sinf(static_cast<float>(iHill) * 2.1f);
			xAuto.AddStep_TerrainBrushStroke(iSetHeight, fCAMPUS_CX + fInnerR * cosf(fA),
				fCAMPUS_CZ + fInnerR * sinf(fA), fInnerBrush, 0.9f, fPeak);
		}
		// Outer ring: 18 taller dabs at r=255, brush 78, offset +10deg so its
		// peaks interleave the inner ring → a layered hill belt receding into the
		// distance. Outer edge (255+78=333) stays well inside the dome (760,420).
		const int   iOUTER_HILLS = 18;
		const float fOuterR = 265.0f, fOuterBrush = 95.0f;
		for (int iHill = 0; iHill < iOUTER_HILLS; ++iHill)
		{
			const float fA = glm::radians(360.0f / iOUTER_HILLS * static_cast<float>(iHill) + 10.0f);
			const float fPeak = 92.0f + 12.0f * sinf(static_cast<float>(iHill) * 1.3f + 0.6f);
			xAuto.AddStep_TerrainBrushStroke(iSetHeight, fCAMPUS_CX + fOuterR * cosf(fA),
				fCAMPUS_CZ + fOuterR * sinf(fA), fOuterBrush, 0.88f, fPeak);
		}

		// Erode everything (hydraulic droplets + thermal slumping), THEN
		// re-flatten the gameplay plateau the IK platforms / player spawn /
		// smoke expectations assume (fully flat within ~100m of 256,256 — wide
		// enough to cover the lowered tennis court + material showcase cluster).
		xAuto.AddStep_TerrainErode(150000, 2, 1337);
		xAuto.AddStep_TerrainBrushStroke(iSetHeight, fCAMPUS_CX, fCAMPUS_CZ, 100.0f, 1.0f, 48.25f);

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
		xAuto.AddStep_TerrainBrushStroke(iGrass, fCAMPUS_CX, fCAMPUS_CZ + 74.0f, 60.0f, 1.0f, 0.6f);
		xAuto.AddStep_TerrainBrushStroke(iGrass, fCAMPUS_CX, fCAMPUS_CZ - 74.0f, 60.0f, 1.0f, 0.6f);
		xAuto.AddStep_TerrainBrushStroke(iGrass, fCAMPUS_CX + 74.0f, fCAMPUS_CZ, 60.0f, 1.0f, 0.6f);
		xAuto.AddStep_TerrainBrushStroke(iGrass, fCAMPUS_CX - 74.0f, fCAMPUS_CZ, 60.0f, 1.0f, 0.6f);
		xAuto.AddStep_TerrainBrushStroke(iGrass, 420.0f + fCAMPUS_SHIFT, 360.0f + fCAMPUS_SHIFT, 80.0f, 1.0f, 0.5f);

		// Dense grass blanketing BOTH hill rings (matches the hill footprints
		// above). Reaches non-tools via the saved GrassDensity.ztxtr + the
		// bootstrap component's per-boot apply.
		for (int iHill = 0; iHill < 24; ++iHill)
		{
			const float fA = glm::radians(360.0f / 24.0f * static_cast<float>(iHill));
			xAuto.AddStep_TerrainBrushStroke(iGrass, fCAMPUS_CX + 185.0f * cosf(fA),
				fCAMPUS_CZ + 185.0f * sinf(fA), 80.0f, 1.0f, 0.9f);
		}
		for (int iHill = 0; iHill < 18; ++iHill)
		{
			const float fA = glm::radians(360.0f / 18.0f * static_cast<float>(iHill) + 10.0f);
			xAuto.AddStep_TerrainBrushStroke(iGrass, fCAMPUS_CX + 265.0f * cosf(fA),
				fCAMPUS_CZ + 265.0f * sinf(fA), 95.0f, 1.0f, 0.85f);
		}

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

	// Per-launch bootstrap, authored FIRST so its OnAwake (CLI/tuning parse) precedes
	// every other entity's OnAwake/OnStart. Saved (non-transient) by default. Owns the
	// CLI tuning state, the jetpack-mount calibration override, and the post-terrain
	// grass apply now that the runtime spawns are gone.
	g_xEngine.EditorAutomation().AddStep_CreateEntity("RenderTestBootstrap");
	g_xEngine.EditorAutomation().AddStep_AddComponent("RenderTestBootstrap");

	// GameManager — main camera with follow-camera component.
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
	g_xEngine.EditorAutomation().AddStep_SetCameraPosition(fCAMPUS_CX, fInitialPlayerY + fCamOffsetY, fCAMPUS_CZ + fCamOffsetZ);
	g_xEngine.EditorAutomation().AddStep_SetCameraPitch(-0.15f);
	g_xEngine.EditorAutomation().AddStep_SetCameraYaw(0.0f);
	g_xEngine.EditorAutomation().AddStep_SetCameraFOV(glm::radians(70.0f));
	g_xEngine.EditorAutomation().AddStep_SetCameraNear(0.1f);
	g_xEngine.EditorAutomation().AddStep_SetCameraFar(10000.0f);
	g_xEngine.EditorAutomation().AddStep_SetAsMainCamera();
	g_xEngine.EditorAutomation().AddStep_AddComponent("RenderTestFollowCamera");

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
	g_xEngine.EditorAutomation().AddStep_SetTransformPosition(fCAMPUS_CX, fInitialPlayerY - 2.f, fCAMPUS_CZ);
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
	// fX/fZ below are the legacy 256-anchored campus-local coords; the lambda
	// translates them onto the recentred campus (fCAMPUS_SHIFT). The absolute
	// X-range notes in the callers are pre-shift but the relative geometry (cube
	// edge vs capsule, step heights) is preserved exactly by the rigid translation.
	auto AddIKPlatform = [](const char* szName, float fX, float fY, float fZ,
		float fSX, float fSY, float fSZ)
	{
		g_xEngine.EditorAutomation().AddStep_CreateEntity(szName);
		g_xEngine.EditorAutomation().AddStep_SetEntityTransient(false);
		g_xEngine.EditorAutomation().AddStep_SetTransformPosition(fX + fCAMPUS_SHIFT, fY, fZ + fCAMPUS_SHIFT);
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
	g_xEngine.EditorAutomation().AddStep_SetTransformPosition(fCAMPUS_CX, fInitialPlayerY, fCAMPUS_CZ);
	g_xEngine.EditorAutomation().AddStep_AddModel();
	// The .zmodel bundles the painted-atlas body material — no override step.
	g_xEngine.EditorAutomation().AddStep_LoadModel(RenderTest::Resources().m_strStickFigureModelPath.c_str());
	g_xEngine.EditorAutomation().AddStep_AddAnimator();
	// Muzzle flash emitter lives on the Player entity; the player component
	// overrides its emit position+direction per shot so we don't need a
	// separate gun-barrel child entity.
	g_xEngine.EditorAutomation().AddStep_AddParticleEmitter();
	g_xEngine.EditorAutomation().AddStep_SetParticleConfigByName("RenderTest_MuzzleFlash");
	g_xEngine.EditorAutomation().AddStep_SetParticleEmitting(false);
	g_xEngine.EditorAutomation().AddStep_AddComponent("RenderTestPlayer");
	// W3: the discrete-press action decisions (E/R/LMB/T) live in the
	// PlayerActions graph; the component keeps the holds + systems.
	g_xEngine.EditorAutomation().AddStep_AttachGraph("game:Graphs/RenderTest_PlayerActions.bgraph");

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

	// Gun pickup/drop prompt — bottom-centre, empty until the player nears or
	// holds a gun (the player fills it via the "GunPrompt" element).
	g_xEngine.EditorAutomation().AddStep_CreateUIText("GunPrompt", "");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("GunPrompt", static_cast<int>(Zenith_UI::AnchorPreset::BottomCenter));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("GunPrompt", 0.0f, -80.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("GunPrompt", 26.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("GunPrompt", 1.0f, 0.95f, 0.6f, 1.0f);

	// =====================================================================
	// Testbeds baked into the scene (jetpack / guns / tennis). Meshes/materials/
	// textures are exported to disk by GenerateRenderTestTestbedAssets (run in
	// Project_InitializeResources, before automation); here we author the ENTITIES,
	// which SaveScene serializes. The Player (jetpack/attach target) is authored
	// above; the NPCs (racket targets) are authored here BEFORE their attach steps,
	// so every AddStep_AttachToBone target resolves by name at author time.
	// =====================================================================
	{
		Zenith_EditorAutomation& xAuto = g_xEngine.EditorAutomation();

		// --- Jetpack: worn on the Player's Spine bone (default mount). ---
		xAuto.AddStep_CreateEntity("Jetpack");
		// Seat at the player spawn so frame 0 (before the attachment's OnLateUpdate
		// resolves the Spine bone) isn't at the world origin.
		xAuto.AddStep_SetTransformPosition(fCAMPUS_CX, fInitialPlayerY, fCAMPUS_CZ);
		xAuto.AddStep_AddModel();
		xAuto.AddStep_LoadModel(RenderTest_JetpackModelPath());
		xAuto.AddStep_AddParticleEmitter();
		xAuto.AddStep_SetParticleConfigByName("RenderTest_JetTrail");
		xAuto.AddStep_SetParticleEmitting(false);
		xAuto.AddStep_AddComponent("RenderTestJetpack");
		// Default mount T(0, 0.18, -0.14) — matches RenderTest_BuildJetpackMount's
		// default; the --jetpack-mount-* override is re-applied at runtime by the
		// bootstrap.
		xAuto.AddStep_AttachToBone("Player", "Spine", 0.0f, 0.18f, -0.14f, 0.0f, 0.0f, 0.0f);

		// --- Guns: one of each type laid flat on the spawn-platform deck. ---
		// CenterPlatform deck top Y = 48.75; lay them in a row in front of the player.
		{
			constexpr float fDeckTopY = 48.75f;
			constexpr float fRowZ = fCAMPUS_CZ + 5.0f;   // 5 m in front of the campus centre
			constexpr float fSpacing = 2.0f;
			constexpr float fRestLift = 0.05f;
			const int iCount = static_cast<int>(RenderTest_Guns::GunType::COUNT);
			for (int i = 0; i < iCount; ++i)
			{
				const RenderTest_Guns::GunType eType = static_cast<RenderTest_Guns::GunType>(i);
				const float fX = fCAMPUS_CX + (static_cast<float>(i) - (iCount - 1) * 0.5f) * fSpacing;
				xAuto.AddStep_CreateEntity(RenderTest_GunEntityName(eType));
				xAuto.AddStep_SetTransformPosition(fX, fDeckTopY + fRestLift, fRowZ);
				// Rest flat on a side face (rotZ +90deg).
				xAuto.AddStep_SetTransformRotationEuler(0.0f, 0.0f, 90.0f);
				xAuto.AddStep_AddModel();
				xAuto.AddStep_LoadModel(RenderTest_GunModelPath(eType));
				xAuto.AddStep_AddComponent("RenderTestGun");
				xAuto.AddStep_AddComponent("Attachment");   // idle until picked up; no collider
			}
		}

		// --- Tennis court / net / tape / ball + 2 NPCs + rackets + match. ---
		{
			using namespace RenderTest_Tennis;

			// Court slab (textured), static OBB collider for the bounce.
			xAuto.AddStep_CreateEntity("Tennis_Court");
			xAuto.AddStep_SetTransformPosition(fCOURT_CX, fSURFACE_Y - fSLAB_THICKNESS * 0.5f, fCOURT_CZ);
			xAuto.AddStep_SetTransformScale(2.0f * fSLAB_HALF_WIDTH, fSLAB_THICKNESS, 2.0f * fSLAB_HALF_LENGTH);
			xAuto.AddStep_AddModel();
			xAuto.AddStep_LoadModel(RenderTest_TennisCourtModelPath());
			xAuto.AddStep_AddCollider();
			xAuto.AddStep_AddColliderShape(COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_STATIC);

			// Net (masked, two-sided), thin static OBB collider.
			xAuto.AddStep_CreateEntity("Tennis_Net");
			xAuto.AddStep_SetTransformPosition(fCOURT_CX, fSURFACE_Y + fNET_HEIGHT * 0.5f, fCOURT_CZ);
			xAuto.AddStep_SetTransformScale(2.0f * fNET_HALF_WIDTH, fNET_HEIGHT, 0.06f);
			xAuto.AddStep_AddModel();
			xAuto.AddStep_LoadModel(RenderTest_TennisNetModelPath());
			xAuto.AddStep_AddCollider();
			xAuto.AddStep_AddColliderShape(COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_STATIC);

			// White net tape (real-size mesh, visual only) along the top edge.
			xAuto.AddStep_CreateEntity("Tennis_NetTape");
			xAuto.AddStep_SetTransformPosition(fCOURT_CX, fSURFACE_Y + fNET_HEIGHT, fCOURT_CZ);
			xAuto.AddStep_AddModel();
			xAuto.AddStep_LoadModel(RenderTest_TennisTapeModelPath());

			// Ball (yellow sphere), dynamic sphere collider. Scale BEFORE the shape
			// step — the sphere radius is derived from the transform scale.
			xAuto.AddStep_CreateEntity("Tennis_Ball");
			xAuto.AddStep_SetTransformPosition(fCOURT_CX, fSURFACE_Y + 3.0f, fBASELINE_NEAR_Z + 1.0f);
			xAuto.AddStep_SetTransformScale(2.0f * fBALL_RADIUS, 2.0f * fBALL_RADIUS, 2.0f * fBALL_RADIUS);
			xAuto.AddStep_AddModel();
			xAuto.AddStep_LoadModel(RenderTest_TennisBallModelPath());
			xAuto.AddStep_AddCollider();
			xAuto.AddStep_AddColliderShape(COLLISION_VOLUME_TYPE_SPHERE, RIGIDBODY_TYPE_DYNAMIC);

			// NPC players (StickFigure model + animator + tennis component) and their
			// hand-attached rackets. Per-side config is stamped by the config step
			// below; the NPC's dynamic capsule collider is added in OnStart.
			struct NpcAuthoring { const char* m_szNpc; const char* m_szRacket; float m_fZ; };
			const NpcAuthoring axNpcs[2] = {
				{ "Tennis_NPC_Near", "Tennis_Racket_Near", fBASELINE_NEAR_Z },
				{ "Tennis_NPC_Far",  "Tennis_Racket_Far",  fBASELINE_FAR_Z  },
			};
			for (const NpcAuthoring& xNpc : axNpcs)
			{
				const float fFeetY = fSURFACE_Y + 1.05f;
				xAuto.AddStep_CreateEntity(xNpc.m_szNpc);
				xAuto.AddStep_SetTransformPosition(fCOURT_CX, fFeetY, xNpc.m_fZ);
				xAuto.AddStep_AddModel();
				xAuto.AddStep_LoadModel(RenderTest::Resources().m_strStickFigureModelPath.c_str());
				xAuto.AddStep_AddAnimator();
				xAuto.AddStep_AddComponent("RenderTestTennisPlayer");
				// Autonomous AI (W3): the engine AIAgent survives as perception
				// registrar + nav-agent host; the DECISION body lives in the
				// TennisBrain graph attached below (its own tick chain reproduces
				// the retired 0.08 s AIAgent interval). The brain shim keeps the
				// RNG / decided-shot / player-state systems. All authored +
				// serialized so the scene plays the same in tools and _False.
				xAuto.AddStep_AddComponent("AIAgent");
				xAuto.AddStep_AddComponent("RenderTestTennisAgent");
				xAuto.AddStep_AttachGraph(RenderTest_TennisAgentComponent::kszGraphAsset);

				// Racket, seated at the NPC for frame 0, attached to the right hand
				// (Rx 180deg so the head extends along the hand).
				xAuto.AddStep_CreateEntity(xNpc.m_szRacket);
				xAuto.AddStep_SetTransformPosition(fCOURT_CX, fFeetY, xNpc.m_fZ);
				xAuto.AddStep_AddModel();
				xAuto.AddStep_LoadModel(RenderTest_TennisRacketModelPath());
				xAuto.AddStep_AttachToBone(xNpc.m_szNpc, "RightHand", 0.0f, 0.0f, 0.0f, 180.0f, 0.0f, 0.0f);
			}

			// Match manager (owns the ball + players, scoring).
			xAuto.AddStep_CreateEntity("Tennis_Match");
			xAuto.AddStep_AddComponent("RenderTestTennisMatch");
		}

		// --- Instanced trees ringing the hills. -----------------------------
		// Painted AFTER CreateScene (TreePaint needs the active scene to host the
		// TerrainTrees_Trunk/_Leaves instanced entities) and BEFORE SaveScene, so
		// the instances serialize into RenderTest.zscen via Zenith_InstancedMesh-
		// Component and load in non-tools (which run no automation). The terrain
		// editor auto-opens a standalone session here, seeded from the baked
		// Height.ztxtr, so SampleHeightWorld reads the ring-hill heights for tree
		// Y + slope rejection. Re-painted into the FRESH scene every tools boot
		// (CreateScene makes a new scene each boot => no accumulation); the fixed
		// seed + deterministic heightfield keep the save byte-stable. Windowed-
		// only (EnsureTreeEntities no-ops headless — harmless; authoring is windowed).
		{
			const int iTreeTool = static_cast<int>(Zenith_TerrainBrushTool::TreePaint);
			// Dense brush: many attempts/dab, tight spacing, allow steeper flanks
			// (the default 38deg skips most of a ~56m hill). Seed 4242 for a
			// reproducible scatter. fToolValue 0 = paint (>0.5 would erase).
			xAuto.AddStep_TerrainSetTreeBrush(60, 1.0f, 1.8f, 2.5f, 50.0f, 4242);
			for (int iHill = 0; iHill < 24; ++iHill)
			{
				const float fA = glm::radians(360.0f / 24.0f * static_cast<float>(iHill));
				xAuto.AddStep_TerrainBrushStroke(iTreeTool, fCAMPUS_CX + 185.0f * cosf(fA),
					fCAMPUS_CZ + 185.0f * sinf(fA), 75.0f, 1.0f, 0.0f);
			}
			for (int iHill = 0; iHill < 18; ++iHill)
			{
				const float fA = glm::radians(360.0f / 18.0f * static_cast<float>(iHill) + 10.0f);
				xAuto.AddStep_TerrainBrushStroke(iTreeTool, fCAMPUS_CX + 265.0f * cosf(fA),
					fCAMPUS_CZ + 265.0f * sinf(fA), 90.0f, 1.0f, 0.0f);
			}
		}

		// Stamp per-instance config (gun type/ammo, NPC side) after all the testbed
		// entities exist, before SaveScene serializes them.
		xAuto.AddStep_Custom(&RenderTest_ApplyTestbedEntityConfig);
	}

	// Material showcase — platform + a grid of every shape × the material matrix,
	// authored as saved entities (each with a static per-primitive collider) from the
	// disk assets baked by RenderTest_ExportMaterialShowcaseAssets. Authored BEFORE
	// SaveScene so it ends up in RenderTest.zscen (was a procedural post-load spawn).
	RenderTest_AuthorMaterialShowcase(g_xEngine.EditorAutomation());

	// Smoke runner — attached BEFORE save so it ends up in the saved scene.
	if (RenderTest_IsSmokeMode())
	{
		g_xEngine.EditorAutomation().AddStep_CreateEntity("RenderTestSmokeRunner");
		g_xEngine.EditorAutomation().AddStep_SetEntityTransient(false);
		g_xEngine.EditorAutomation().AddStep_AddComponent("RenderTestSmokeRunner");
	}

	g_xEngine.EditorAutomation().AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/RenderTest" ZENITH_SCENE_EXT);
	g_xEngine.EditorAutomation().AddStep_UnloadScene();

	g_xEngine.EditorAutomation().AddStep_LoadInitialScene(&Project_LoadInitialScene);

	// Grass placement needs the loaded terrain's physics mesh; the editor
	// defers the scene load to the next Update, so apply the painted density
	// map one automation step (== one frame) later.
	g_xEngine.EditorAutomation().AddStep_Custom(&RenderTest_ApplyGrassDensityFromDisk);

	// (The material showcase + jetpack / guns / tennis testbeds are no longer spawned
	// post-load — they are authored into the saved scene above, before AddStep_SaveScene,
	// and their meshes/materials/textures are baked to disk by
	// GenerateRenderTestTestbedAssets.)

	// Smoke play-mode entry LAST — by this step the deferred load has
	// completed, so the smoke runner's first tick probes the fully loaded
	// terrain (see the note in Project_LoadInitialScene).
	if (RenderTest_IsSmokeMode())
	{
		g_xEngine.EditorAutomation().AddStep_Custom(&RenderTest_EnterSmokePlayMode);
	}

	// Gun/jetpack-showcase capture (no smoke): enter Play so the player ticks
	// (auto-equips the chosen gun + holds the IK pose, or forces jetpack thrust +
	// frames the rising player for a screenshot). Skipped when --rendertest-smoke
	// is set (that path already enters Play, and its player-teleporting smoke
	// runner would fight the showcase pose).
	else if (RenderTest_HasCommandLineFlagPrefix("--rendertest-gun-showcase")
		|| RenderTest_HasCommandLineFlagPrefix("--rendertest-jetpack-showcase"))
	{
		g_xEngine.EditorAutomation().AddStep_Custom(&RenderTest_EnterSmokePlayMode);
	}

	// Interactive brush-indicator capture aid: open a terrain-editor session
	// once the scene is loaded (editor stays Stopped — no smoke mode).
	if (RenderTest_HasCommandLineFlag("--rendertest-open-terrain-editor"))
	{
		g_xEngine.EditorAutomation().AddStep_Custom(&RenderTest_OpenTerrainEditorForCursor);
	}
}
#endif

// Painted grass: load the terrain editor's saved GrassDensity.ztxtr (R32,
// 1024^2) into the grass system and rebuild blade placement from the terrain's
// physics mesh. Idempotent + retryable — the result tells the caller (the
// bootstrap component) whether to retry (terrain not streamed yet) or give up
// (missing file / headless). Declared in RenderTest_BootstrapComponent.h.
RenderTest_GrassApplyResult RenderTest_TryApplyGrassDensityFromDisk()
{
	if (Zenith_CommandLine::IsHeadless())
	{
		return RenderTest_GrassApplyResult::SkippedHeadless;   // grass GPU buffers don't exist headless
	}

	// Probe terrain readiness FIRST — it's the gate that flips frame-to-frame while
	// terrain streams in, and the bootstrap retries this every frame until it does.
	// Reading + decoding the ~4MB density .ztxtr before this check would re-parse the
	// whole file on every not-ready frame (up to the retry cap); doing it after the
	// cheap probe restores the old single-read behaviour.
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
		return RenderTest_GrassApplyResult::TerrainNotReady;   // retry next frame (no disk read yet)
	}

	const std::string strPath = std::string(GAME_ASSETS_DIR) + "Textures/Terrain/GrassDensity" ZENITH_TEXTURE_EXT;
	if (!std::filesystem::exists(strPath))
	{
		return RenderTest_GrassApplyResult::FileMissing;
	}

	// Read through the single .ztxtr parser (no GPU upload) — never hand-parse.
	Flux_SurfaceInfo xInfo;
	Zenith_Vector<uint8_t> xBytes;
	if (!Zenith_TextureAsset::LoadCPUData(strPath, xInfo, xBytes).IsOk()
		|| xInfo.m_eFormat != TEXTURE_FORMAT_R32_SFLOAT
		|| static_cast<size_t>(xBytes.GetSize()) != static_cast<size_t>(xInfo.m_uWidth) * xInfo.m_uHeight * sizeof(float))
	{
		return RenderTest_GrassApplyResult::FileMissing;   // invalid layout — treat as missing (caller warns once)
	}
	const int32_t iWidth = static_cast<int32_t>(xInfo.m_uWidth);
	const int32_t iHeight = static_cast<int32_t>(xInfo.m_uHeight);
	std::vector<float> xDensity(static_cast<size_t>(iWidth) * iHeight);
	memcpy(xDensity.data(), xBytes.GetDataPointer(), xBytes.GetSize());

	g_xEngine.Grass().SetDensityMap(xDensity.data(), static_cast<u_int>(iWidth), static_cast<u_int>(iHeight), 4096.0f);
	g_xEngine.Grass().GenerateFromTerrain(pxTerrain->GetPhysicsMeshGeometry());
	return RenderTest_GrassApplyResult::Applied;
}

// Void wrapper for the tools post-load AddStep_Custom (the editor Stopped-view
// apply — the bootstrap's OnUpdate only ticks while Playing in tools). Shares the
// one idempotent helper and, like master's inline version, logs a diagnostic on the
// failure paths (the bootstrap warns on the runtime/Playing path instead).
#ifdef ZENITH_TOOLS
static void RenderTest_ApplyGrassDensityFromDisk()
{
	switch (RenderTest_TryApplyGrassDensityFromDisk())
	{
	case RenderTest_GrassApplyResult::FileMissing:
		Zenith_Warning(LOG_CATEGORY_TERRAIN, "[RenderTest] grass density map missing/invalid — grass not applied (tools post-load)");
		break;
	case RenderTest_GrassApplyResult::TerrainNotReady:
		Zenith_Warning(LOG_CATEGORY_TERRAIN, "[RenderTest] terrain not ready at tools post-load grass step — grass not applied");
		break;
	case RenderTest_GrassApplyResult::Applied:
	case RenderTest_GrassApplyResult::SkippedHeadless:
		break;
	}
}
#endif

void Project_LoadInitialScene()
{
	g_xEngine.Scenes().RegisterSceneBuildIndex(0, GAME_ASSETS_DIR "Scenes/RenderTest" ZENITH_SCENE_EXT);
	g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);

	// The jetpack / guns / tennis testbeds are now AUTHORED into RenderTest.zscen
	// (entities + on-disk assets) and the bone bindings are re-resolved on load, so
	// nothing is spawned procedurally here any more. Per-launch state (CLI tuning,
	// jetpack-mount calibration override, post-terrain grass apply) is owned by the
	// authored RenderTest_BootstrapComponent (OnAwake/OnStart/OnUpdate), which covers
	// both the non-tools runtime and tools Playing paths. (The tools Stopped-view
	// grass apply is the AddStep_Custom queued after LOAD_INITIAL_SCENE.)

	// Tools builds: smoke play-mode entry is queued as the FINAL automation
	// step (RenderTest_EnterSmokePlayMode) instead of being set here. This
	// function runs on the frame the scene load is QUEUED (the load itself is
	// deferred to the next editor Update) — setting Playing here let the
	// smoke runner attached to the BUILD scene tick once against the
	// half-initialized authoring terrain (culling uninitialized, LOW LOD
	// unstreamed) and emit spurious RENDERTEST_SMOKE_FAIL lines before the
	// real scene arrived.
}

// Input-simulator tests for RenderTest_FollowCameraComponent +
// RenderTest_PlayerComponent. Included here (rather than from a Zenith engine
// TU) so the auto-registered ZENITH_TEST cases land in the RenderTest binary's
// test runner.
#include "RenderTest/Components/RenderTest_PlayerComponent.Tests.inl"
#include "RenderTest/Components/RenderTest_Testbed.Tests.inl"
#include "RenderTest/Components/RenderTest_Tennis.Tests.inl"
