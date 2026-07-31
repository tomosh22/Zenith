#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

// ============================================================================
// S7 item 3 SC1b -- the two automated tests for baked-navmesh persistence
// (ZM-D-147). Both are HEADLESS-CAPABLE and both RUN on the Null backend; that
// is the whole point of committing the asset.
//
// ★ NO RequestSkip -- a DELIBERATE deviation from TestPlan C6.
// C6 says a test whose baked asset may be absent should RequestSkip. That rule
// exists because most baked assets are gitignored, so absence is the EXPECTED
// state on a fresh CI checkout. `.znavmesh` is the exception: it is a TRACKED,
// COMMITTED asset (ZM-D-145 / .gitignore's `!**/*.znavmesh`), so on CI it is
// always present and its absence is a DEFECT, not a condition to tolerate.
// Skipping would also be self-defeating: a skip counts as a PASS, so the one
// gate that proves the committed navmesh loads would go quiet exactly when it
// broke. Recorded in Docs/TestPlan.md alongside the C6 rule.
//
// Per-phase driver functions, never one monolithic Step (the ZM-D-141 stack
// rule): each phase owns its own frame.
// ============================================================================

#include "AI/Navigation/Zenith_NavMesh.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "EntityComponent/Components/Zenith_NavMeshComponent.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Source/Nav/ZM_NavBake.h"
#include "Zenithmon/Source/Nav/ZM_NavEval.h"

namespace
{
	// SC1's asserted polygon band for a 16 m bake of the Dawnmere rect. Written
	// as literals, never derived from the cell size, so a change to either end
	// has to be a deliberate edit.
	constexpr u_int uZM_NAV_POLY_BAND_MIN = 3969u;
	constexpr u_int uZM_NAV_POLY_BAND_MAX = 4489u;

	// Dawnmere's TownCenter spawn in world XZ -- inside the baked rect by
	// construction, so the navmesh must cover it.
	constexpr float fZM_NAV_TOWN_CENTER_X = 512.0f;
	constexpr float fZM_NAV_TOWN_CENTER_Z = 480.0f;

	// The sampled ground height the coverage grid is built at. The generator
	// voxelises at 0.2 m cell height, so the baked surface lands NEAR this, not
	// exactly on it -- hence the coarse band. Exact agreement is phase C's job.
	constexpr float fZM_NAV_GROUND_HEIGHT = 25.99055f;
	constexpr float fZM_NAV_GROUND_TOLERANCE = 2.0f;

	// A generated polygon counts as walkable when its normal points up.
	constexpr float fZM_NAV_WALKABLE_NORMAL_Y_MIN = 0.5f;

	bool  g_bNavFailed = false;
	bool  g_bNavDone = false;

	void FailNav(const char* szWhy)
	{
		g_bNavFailed = true;
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[ZM_Navmesh] FAIL: %s", szWhy);
	}

	u_int CountNavMeshComponents()
	{
		u_int uCount = 0u;
		g_xEngine.Scenes().QueryAllScenes<Zenith_NavMeshComponent>().ForEach(
			[&uCount](Zenith_EntityID, Zenith_NavMeshComponent&) { ++uCount; });
		return uCount;
	}

	Zenith_NavMeshComponent* FindActiveSceneNavMesh()
	{
		Zenith_NavMeshComponent* pxFound = nullptr;
		g_xEngine.Scenes().QueryActiveScene<Zenith_NavMeshComponent>().ForEach(
			[&pxFound](Zenith_EntityID, Zenith_NavMeshComponent& xComponent)
			{
				if (pxFound == nullptr) { pxFound = &xComponent; }
			});
		return pxFound;
	}
}

// ============================================================================
// ZM_NavmeshAsset_Test -- the COMMITTED Dawnmere.znavmesh, adopted at runtime.
//
// A: adopt the asset on a fixture entity (the runtime-adoption recipe) and let
//    it load.
// B: assert the mesh's CONTENTS, not merely that it is non-null.
// C: structural agreement against a FRESH in-memory bake of the same geometry --
//    the drift detector that turns red if the committed bytes stop matching the
//    source recipe.
// D: unload the fixture scene and prove the component went with it.
// ============================================================================

namespace
{
	Zenith_Scene g_xNavFixtureScene;
	Zenith_Scene g_xNavPreviousScene;
	Zenith_EntityID g_xNavFixtureEntity;
	u_int g_uNavBaselineComponents = 0u;

	bool g_bNavPhaseAAdopted = false;
	bool g_bNavPhaseBContent = false;
	bool g_bNavPhaseCAgreement = false;
	bool g_bNavPhaseDTornDown = false;

	// ---- phase A -----------------------------------------------------------
	bool NavPhaseA_AdoptAsset()
	{
		Zenith_NavMeshComponent* pxComponent = FindActiveSceneNavMesh();
		if (pxComponent == nullptr)
		{
			FailNav("the fixture entity lost its Zenith_NavMeshComponent");
			return false;
		}

		if (!pxComponent->IsLoaded())
		{
			FailNav("the committed Dawnmere.znavmesh did not load");
			return false;
		}

		if (pxComponent->GetLoadState() != NAVMESH_LOAD_STATE_LOADED)
		{
			FailNav("load state is not LOADED");
			return false;
		}

		g_bNavPhaseAAdopted = true;
		return true;
	}

	// ---- phase B -----------------------------------------------------------
	bool NavPhaseB_CheckContents()
	{
		Zenith_NavMeshComponent* pxComponent = FindActiveSceneNavMesh();
		const Zenith_NavMesh* pxMesh = pxComponent != nullptr ? pxComponent->GetNavMesh() : nullptr;
		if (pxMesh == nullptr)
		{
			FailNav("no mesh to inspect in phase B");
			return false;
		}

		const u_int uPolys = pxMesh->GetPolygonCount();
		if (uPolys < uZM_NAV_POLY_BAND_MIN || uPolys > uZM_NAV_POLY_BAND_MAX)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[ZM_Navmesh] polygon count %u outside [%u, %u]",
				uPolys, uZM_NAV_POLY_BAND_MIN, uZM_NAV_POLY_BAND_MAX);
			FailNav("committed navmesh polygon count is outside the asserted band");
			return false;
		}

		// Every polygon must face up: a winding regression in the bake would
		// leave a mesh that loads fine and is useless to path on.
		for (u_int u = 0; u < uPolys; ++u)
		{
			if (pxMesh->GetPolygon(u).m_xNormal.y <= fZM_NAV_WALKABLE_NORMAL_Y_MIN)
			{
				FailNav("a committed polygon does not face up");
				return false;
			}
		}

		const Zenith_Maths::Vector3 xMin = pxMesh->GetBoundsMin();
		const Zenith_Maths::Vector3 xMax = pxMesh->GetBoundsMax();

		if (fZM_NAV_TOWN_CENTER_X < xMin.x || fZM_NAV_TOWN_CENTER_X > xMax.x ||
			fZM_NAV_TOWN_CENTER_Z < xMin.z || fZM_NAV_TOWN_CENTER_Z > xMax.z)
		{
			FailNav("the navmesh bounds do not cover the TownCenter spawn in XZ");
			return false;
		}

		// The coverage grid is FLAT, so the mesh has no vertical extent at all --
		// asserting 3D containment of y ~ 26 would be asserting the wrong thing.
		if (fabsf(xMax.y - xMin.y) > 0.01f)
		{
			FailNav("a flat coverage bake must have zero vertical extent");
			return false;
		}
		if (fabsf(xMin.y - fZM_NAV_GROUND_HEIGHT) > fZM_NAV_GROUND_TOLERANCE)
		{
			FailNav("the baked surface is not at Dawnmere's sampled ground height");
			return false;
		}

		uint32_t uPoly = 0;
		Zenith_Maths::Vector3 xNearest(0.0f);
		const Zenith_Maths::Vector3 xTownCenter(fZM_NAV_TOWN_CENTER_X, xMin.y, fZM_NAV_TOWN_CENTER_Z);
		if (!pxMesh->FindNearestPolygon(xTownCenter, uPoly, xNearest))
		{
			FailNav("FindNearestPolygon failed at the TownCenter spawn");
			return false;
		}

		g_bNavPhaseBContent = true;
		return true;
	}

	// ---- phase C -----------------------------------------------------------
	bool NavPhaseC_AgreeWithFreshBake()
	{
		Zenith_NavMeshComponent* pxComponent = FindActiveSceneNavMesh();
		const Zenith_NavMesh* pxMesh = pxComponent != nullptr ? pxComponent->GetNavMesh() : nullptr;
		if (pxMesh == nullptr)
		{
			FailNav("no mesh to compare in phase C");
			return false;
		}

		// A fresh bake of the SAME pure inputs (const recipe table, fixed plane,
		// no terrain and no disk), so this comparison is valid on a GPU-less CI
		// runner with no baked terrain present.
		const ZM_NavEvalResult xFresh =
			ZM_EvaluateDawnmereNavGeneration(fZM_DAWNMERE_NAVMESH_CELL_SIZE, /*bUpwardNormals*/true);

		if (!xFresh.m_bSuccess)
		{
			FailNav("the in-memory reference bake did not generate");
			return false;
		}

		if (xFresh.m_uPolygonCount != pxMesh->GetPolygonCount() ||
			xFresh.m_uVertexCount != pxMesh->GetVertexCount())
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_Navmesh] committed %u polys / %u verts vs fresh %u / %u",
				pxMesh->GetPolygonCount(), pxMesh->GetVertexCount(),
				xFresh.m_uPolygonCount, xFresh.m_uVertexCount);
			FailNav("the committed navmesh has drifted from a fresh bake of its source geometry");
			return false;
		}

		const Zenith_Maths::Vector3 xMinDelta = xFresh.m_xBoundsMin - pxMesh->GetBoundsMin();
		const Zenith_Maths::Vector3 xMaxDelta = xFresh.m_xBoundsMax - pxMesh->GetBoundsMax();
		constexpr float fBOUNDS_TOLERANCE = 0.01f;
		if (fabsf(xMinDelta.x) > fBOUNDS_TOLERANCE || fabsf(xMinDelta.y) > fBOUNDS_TOLERANCE ||
			fabsf(xMinDelta.z) > fBOUNDS_TOLERANCE || fabsf(xMaxDelta.x) > fBOUNDS_TOLERANCE ||
			fabsf(xMaxDelta.y) > fBOUNDS_TOLERANCE || fabsf(xMaxDelta.z) > fBOUNDS_TOLERANCE)
		{
			FailNav("the committed navmesh bounds have drifted from a fresh bake");
			return false;
		}

		g_bNavPhaseCAgreement = true;
		return true;
	}

	// ---- phase D -----------------------------------------------------------
	bool NavPhaseD_TearDownFixture()
	{
		const u_int uWithFixture = CountNavMeshComponents();
		if (uWithFixture != g_uNavBaselineComponents + 1u)
		{
			FailNav("the fixture component was not counted before teardown");
			return false;
		}

		if (g_xNavPreviousScene.IsValid())
		{
			g_xEngine.Scenes().SetActiveScene(g_xNavPreviousScene);
		}
		g_xEngine.Scenes().UnloadSceneForced(g_xNavFixtureScene);
		g_xNavFixtureScene = Zenith_Scene();

		// Lifetime rides the SCENE: unloading it must destroy the COMPONENT.
		// (This counts components, not meshes -- an undeleted Zenith_NavMesh is
		// invisible to an ECS query. The mesh's single-owner invariant is pinned
		// by the engine's move-relocation unit, not here.)
		const u_int uAfter = CountNavMeshComponents();
		if (uAfter != g_uNavBaselineComponents)
		{
			FailNav("the navmesh component outlived its scene");
			return false;
		}

		g_bNavPhaseDTornDown = true;
		return true;
	}
}

static void Setup_ZMNavmeshAsset()
{
	g_bNavFailed = false;
	g_bNavDone = false;
	g_bNavPhaseAAdopted = false;
	g_bNavPhaseBContent = false;
	g_bNavPhaseCAgreement = false;
	g_bNavPhaseDTornDown = false;
	g_xNavFixtureScene = Zenith_Scene();
	g_xNavPreviousScene = Zenith_Scene();
	g_uNavBaselineComponents = CountNavMeshComponents();

	// The committed asset is what this test exists to prove; say so plainly if
	// the checkout is missing it rather than failing later on a null mesh.
	const std::string strPath = Zenith_AssetRegistry::ResolvePath(szZM_DAWNMERE_NAVMESH_ASSET_REF);
	if (!Zenith_FileAccess::FileExists(strPath.c_str()))
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[ZM_Navmesh] committed asset missing: %s", strPath.c_str());
		FailNav("the committed Dawnmere.znavmesh is absent (it is TRACKED -- absence is a defect)");
		return;
	}

	g_xNavPreviousScene = g_xEngine.Scenes().GetActiveScene();
	g_xNavFixtureScene = g_xEngine.Scenes().LoadScene(
		"ZM_NavmeshAssetFixture", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	if (!g_xNavFixtureScene.IsValid() || !g_xEngine.Scenes().SetActiveScene(g_xNavFixtureScene))
	{
		FailNav("could not stage the fixture scene");
		return;
	}

	Zenith_SceneData* pxData = g_xEngine.Scenes().GetSceneData(g_xNavFixtureScene);
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxData, "ZM_NavmeshFixtureHolder");
	g_xNavFixtureEntity = xEntity.GetEntityID();

	// The RUNTIME adoption recipe: AddComponent + SetAssetRef. (The authored
	// recipe -- a component serialized into a scene -- is what
	// ZM_DawnmereHeadless_Test covers.)
	Zenith_NavMeshComponent& xComponent = xEntity.AddComponent<Zenith_NavMeshComponent>();
	xComponent.SetAssetRef(szZM_DAWNMERE_NAVMESH_ASSET_REF);
}

static bool Step_ZMNavmeshAsset(int iFrame)
{
	if (g_bNavFailed || g_bNavDone)
	{
		return false;
	}

	// One frame of settle before phase A: OnStart is deferred to the entity's
	// first Update, so an immediate read would be testing SetAssetRef alone.
	if (iFrame < 1)
	{
		return true;
	}

	if (!g_bNavPhaseAAdopted)
	{
		return NavPhaseA_AdoptAsset();
	}
	if (!g_bNavPhaseBContent)
	{
		return NavPhaseB_CheckContents();
	}
	if (!g_bNavPhaseCAgreement)
	{
		return NavPhaseC_AgreeWithFreshBake();
	}
	if (!g_bNavPhaseDTornDown)
	{
		if (!NavPhaseD_TearDownFixture())
		{
			return false;
		}
		g_bNavDone = true;
		return false;
	}

	return false;
}

static bool Verify_ZMNavmeshAsset()
{
	// Clean up a fixture scene left behind by an early failure, so the next test
	// starts from the same world this one did.
	if (g_xNavFixtureScene.IsValid())
	{
		if (g_xNavPreviousScene.IsValid())
		{
			g_xEngine.Scenes().SetActiveScene(g_xNavPreviousScene);
		}
		g_xEngine.Scenes().UnloadSceneForced(g_xNavFixtureScene);
		g_xNavFixtureScene = Zenith_Scene();
	}

	const bool bPassed = !g_bNavFailed && g_bNavPhaseAAdopted && g_bNavPhaseBContent &&
		g_bNavPhaseCAgreement && g_bNavPhaseDTornDown;
	Zenith_Log(LOG_CATEGORY_UNITTEST,
		"[ZM_Navmesh] adopt=%d contents=%d agreement=%d teardown=%d",
		g_bNavPhaseAAdopted ? 1 : 0, g_bNavPhaseBContent ? 1 : 0,
		g_bNavPhaseCAgreement ? 1 : 0, g_bNavPhaseDTornDown ? 1 : 0);
	return bPassed;
}

static const Zenith_AutomatedTest g_xZMNavmeshAssetTest = {
	"ZM_NavmeshAsset_Test",
	&Setup_ZMNavmeshAsset,
	&Step_ZMNavmeshAsset,
	&Verify_ZMNavmeshAsset,
	/*maxFrames*/ 600,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMNavmeshAssetTest);

// ============================================================================
// ZM_DawnmereHeadless_Test -- the AUTHORED recipe, end to end, GPU-less.
//
// Directly loads the COMMITTED Dawnmere.zscen (build index 2) and proves the
// authored Zenith_NavMeshComponent came back with a live, in-band mesh. This is
// the gate that would catch the editor-registry mirror being missed (the scene
// would save with no component at all).
//
// A DIRECT load records no warp arrival, so it has no autosave side effects. No
// gameplay, height or terrain claims are made -- on CI the terrain chunks are
// absent by design and the terrain component is inert.
// ============================================================================

namespace
{
	Zenith_Scene g_xDawnmereScene;
	Zenith_Scene g_xDawnmerePreviousScene;
	bool g_bDawnmereFound = false;
	bool g_bDawnmereUnloaded = false;
	bool g_bDawnmereFailed = false;

	void FailDawnmere(const char* szWhy)
	{
		g_bDawnmereFailed = true;
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[ZM_DawnmereHeadless] FAIL: %s", szWhy);
	}

	bool DawnmerePhase_FindAuthoredNavMesh()
	{
		Zenith_NavMeshComponent* pxComponent = FindActiveSceneNavMesh();
		if (pxComponent == nullptr)
		{
			return false;  // keep waiting -- OnStart is deferred to the first Update
		}

		if (!pxComponent->IsLoaded())
		{
			return false;
		}

		const Zenith_NavMesh* pxMesh = pxComponent->GetNavMesh();
		const u_int uPolys = pxMesh->GetPolygonCount();
		if (uPolys < uZM_NAV_POLY_BAND_MIN || uPolys > uZM_NAV_POLY_BAND_MAX)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[ZM_DawnmereHeadless] polygon count %u outside [%u, %u]",
				uPolys, uZM_NAV_POLY_BAND_MIN, uZM_NAV_POLY_BAND_MAX);
			FailDawnmere("the authored navmesh is outside the asserted polygon band");
			return false;
		}

		g_bDawnmereFound = true;
		return true;
	}

	void CleanupDawnmereScene()
	{
		if (g_xDawnmereScene.IsValid())
		{
			if (g_xDawnmerePreviousScene.IsValid())
			{
				g_xEngine.Scenes().SetActiveScene(g_xDawnmerePreviousScene);
			}
			g_xEngine.Scenes().UnloadSceneForced(g_xDawnmereScene);
			g_xDawnmereScene = Zenith_Scene();
			g_bDawnmereUnloaded = true;
		}
	}
}

static void Setup_ZMDawnmereHeadless()
{
	g_bDawnmereFound = false;
	g_bDawnmereUnloaded = false;
	g_bDawnmereFailed = false;
	g_xDawnmereScene = Zenith_Scene();
	g_xDawnmerePreviousScene = Zenith_Scene();

	// Idempotent re-assign of the identical path the game boot already
	// registered (a same-path no-op, not the mismatch assert).
	g_xEngine.Scenes().RegisterSceneBuildIndex(
		2, GAME_ASSETS_DIR "Scenes/Dawnmere" ZENITH_SCENE_EXT);

	g_xDawnmerePreviousScene = g_xEngine.Scenes().GetActiveScene();
	g_xDawnmereScene = g_xEngine.Scenes().LoadSceneByIndex(2, SCENE_LOAD_ADDITIVE);
	if (!g_xDawnmereScene.IsValid() || !g_xEngine.Scenes().SetActiveScene(g_xDawnmereScene))
	{
		FailDawnmere("could not additively load + activate the committed Dawnmere scene");
	}
}

static bool Step_ZMDawnmereHeadless(int iFrame)
{
	if (g_bDawnmereFailed)
	{
		return false;
	}

	if (!g_bDawnmereFound)
	{
		if (DawnmerePhase_FindAuthoredNavMesh())
		{
			return true;  // one more frame, then tear down
		}
		if (iFrame > 300)
		{
			FailDawnmere("no loaded Zenith_NavMeshComponent appeared in the committed Dawnmere scene");
			return false;
		}
		return true;
	}

	CleanupDawnmereScene();
	return false;
}

static bool Verify_ZMDawnmereHeadless()
{
	CleanupDawnmereScene();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "[ZM_DawnmereHeadless] found=%d unloaded=%d",
		g_bDawnmereFound ? 1 : 0, g_bDawnmereUnloaded ? 1 : 0);
	return !g_bDawnmereFailed && g_bDawnmereFound && g_bDawnmereUnloaded;
}

static const Zenith_AutomatedTest g_xZMDawnmereHeadlessTest = {
	"ZM_DawnmereHeadless_Test",
	&Setup_ZMDawnmereHeadless,
	&Step_ZMDawnmereHeadless,
	&Verify_ZMDawnmereHeadless,
	/*maxFrames*/ 600,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMDawnmereHeadlessTest);

#endif // ZENITH_INPUT_SIMULATOR
