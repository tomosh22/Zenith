#include "Zenith.h"

#include "Zenithmon/Source/Nav/ZM_NavBake.h"

#include "AI/Navigation/Zenith_NavMesh.h"
#include "AI/Navigation/Zenith_NavMeshBaker.h"
#include "AI/Navigation/Zenith_NavMeshGenerator.h"
#include "Zenithmon/Source/Nav/ZM_NavEval.h"

const char* ZM_GetDawnmereNavmeshBakePath()
{
	return GAME_ASSETS_DIR "Navmesh/Dawnmere" ZENITH_NAVMESH_EXT;
}

#ifdef ZENITH_TOOLS
void ZM_BakeDawnmereNavmeshStep()
{
	if (Zenith_IsNullRenderer())
	{
		// A Null (headless/CI) boot LOADS the committed bytes; it must never
		// re-author them. Baking here would churn a tracked asset from a build
		// that authors no render content (D8).
		Zenith_Log(LOG_CATEGORY_AI,
			"[ZM Nav] Dawnmere navmesh bake DEFERRED (null renderer); the committed asset is load-only here");
		return;
	}

	// The same pure, terrain-free coverage grid SC1 evaluated: a flat quad grid
	// over the recipe's 1024 m export sub-rect at Dawnmere's sampled ground
	// height. No terrain component, no GPU, no disk read -- so the bake is valid
	// on any tools boot, warm terrain or not.
	const ZM_NavEvalRect xRect = ZM_GetDawnmereNavRect();

	Zenith_Vector<Zenith_Maths::Vector3> axVertices;
	Zenith_Vector<uint32_t> auIndices;
	const ZM_NavEvalGrid xGrid = ZM_BuildCoverageGrid(
		xRect, fZM_DAWNMERE_NAVMESH_CELL_SIZE, /*bUpwardNormals*/true, axVertices, auIndices);

	Zenith_Assert(xGrid.m_bBuilt,
		"Dawnmere navmesh bake: the coverage grid failed closed at cell size %.2f",
		fZM_DAWNMERE_NAVMESH_CELL_SIZE);
	if (!xGrid.m_bBuilt)
	{
		Zenith_Error(LOG_CATEGORY_AI,
			"[ZM Nav] Dawnmere navmesh bake ABORTED: coverage grid failed closed at cell size %.2f",
			fZM_DAWNMERE_NAVMESH_CELL_SIZE);
		return;
	}

	NavMeshGenerationConfig xConfig;
	xConfig.m_fCellSize = fZM_DAWNMERE_NAVMESH_CELL_SIZE;

	const Zenith_NavMeshBakeResult xResult = Zenith_NavMeshBaker::BakeToFile(
		axVertices, auIndices, xConfig, ZM_GetDawnmereNavmeshBakePath());

	// LOUD on failure: the committed asset is what every CI run then tests
	// against, so a silently-missing bake would surface as a confusing runtime
	// load failure hours later instead of here.
	Zenith_Assert(xResult.m_bSuccess,
		"Dawnmere navmesh bake FAILED (%s)", Zenith_NavMeshBakeErrorToString(xResult.m_eError));
	if (!xResult.m_bSuccess)
	{
		Zenith_Error(LOG_CATEGORY_AI, "[ZM Nav] Dawnmere navmesh bake FAILED: %s",
			Zenith_NavMeshBakeErrorToString(xResult.m_eError));
		return;
	}

	Zenith_Log(LOG_CATEGORY_AI,
		"[ZM Nav] Dawnmere navmesh baked: %u polygons, %u vertices, %llu bytes -> %s",
		xResult.m_uPolygonCount, xResult.m_uVertexCount, xResult.m_ulBytesWritten,
		ZM_GetDawnmereNavmeshBakePath());
}
#endif
