#include "Zenith.h"
#include "Core/Zenith_TerrainDimensions.h"

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/World/ZM_TerrainAuthoring.h"

#include <filesystem>
#include <fstream>
#include <set>
#include <string>

namespace
{
	constexpr float fEPSILON = 0.00001f;
	constexpr float fTERRAIN_CHUNK_SIZE = 64.0f;

	struct ZM_ExpectedTerrainRecipe
	{
		ZM_SCENE_ID m_eSceneId;
		ZM_SCENE_KIND m_eKind;
		u_int m_uBuildIndex;
		const char* m_szTerrainSet;
		u_int m_uSeed;
		int m_iChunkWidth;
		int m_iChunkHeight;
		u_int m_uRequiredOutputCount;
		u_int m_uFamilyFileCount;
	};

	// Chunk counts and the file counts that follow from them. Each map used to be
	// a 16-wide slice of a fixed 4096 m grid; they carry their own grids now and
	// are sized to their content, so these numbers moved with the shrink:
	//   Dawnmere   4 x  4 =  16 chunks -> 16*3 + 4 =  52 required outputs
	//   Thornacre 13 x 15 = 195 chunks -> 195*3 + 4 = 589
	//   Route 1   11 x 24 = 264 chunks -> 264*3 + 4 = 796
	// (+4 = Height, Splatmap_RGBA, GrassDensity, TerrainDims.zdata; the family
	// count adds the recipe manifest the required list does not enumerate.)
	const ZM_ExpectedTerrainRecipe s_axExpectedRecipes[] =
	{
		{ ZM_SCENE_DAWNMERE, ZM_SCENE_KIND_TOWN, 2u, "Dawnmere",
			0x7BF32CA4u, 4, 4, 52u, 53u },
		{ ZM_SCENE_THORNACRE, ZM_SCENE_KIND_TOWN, 3u, "Thornacre",
			0x9D41BD83u, 13, 15, 589u, 590u },
		{ ZM_SCENE_ROUTE1, ZM_SCENE_KIND_ROUTE, 20u, "Route1",
			0x552E711Du, 11, 24, 796u, 797u },
	};
	static_assert(sizeof(s_axExpectedRecipes) / sizeof(s_axExpectedRecipes[0]) ==
		uZM_TERRAIN_RECIPE_COUNT);

	bool HasPathNamed(const ZM_TerrainAuthoringRecipe& xRecipe, const char* szName)
	{
		for (u_int i = 0; i < xRecipe.m_uPathCount; ++i)
		{
			if (strcmp(xRecipe.m_pxPaths[i].m_szName, szName) == 0)
			{
				return true;
			}
		}
		return false;
	}

	bool HasMaterialNamed(const ZM_TerrainAuthoringRecipe& xRecipe, const char* szName)
	{
		for (u_int i = 0; i < xRecipe.m_uMaterialCount; ++i)
		{
			if (strcmp(xRecipe.m_pxMaterials[i].m_szName, szName) == 0)
			{
				return true;
			}
		}
		return false;
	}

	bool HasAutoSplatNamed(const ZM_TerrainAuthoringRecipe& xRecipe, const char* szName)
	{
		for (u_int i = 0; i < xRecipe.m_uAutoSplatCount; ++i)
		{
			if (strcmp(xRecipe.m_pxAutoSplat[i].m_szName, szName) == 0)
			{
				return true;
			}
		}
		return false;
	}

	bool HasLandmarkNamed(const ZM_TerrainAuthoringRecipe& xRecipe, const char* szName)
	{
		for (u_int i = 0; i < xRecipe.m_uLandmarkCount; ++i)
		{
			if (strcmp(xRecipe.m_pxLandmarks[i].m_szName, szName) == 0)
			{
				return true;
			}
		}
		return false;
	}

	bool SameShapePlan(const ZM_TerrainAuthoringRecipe& xA,
		const ZM_TerrainAuthoringRecipe& xB)
	{
		if (xA.m_fTargetHeight != xB.m_fTargetHeight ||
			xA.m_xProcedural.m_fBaseHeight != xB.m_xProcedural.m_fBaseHeight ||
			xA.m_xProcedural.m_fAmplitude != xB.m_xProcedural.m_fAmplitude ||
			xA.m_xProcedural.m_fFrequency != xB.m_xProcedural.m_fFrequency ||
			xA.m_xProcedural.m_uOctaves != xB.m_xProcedural.m_uOctaves ||
			xA.m_xProcedural.m_fLacunarity != xB.m_xProcedural.m_fLacunarity ||
			xA.m_xProcedural.m_fGain != xB.m_xProcedural.m_fGain ||
			xA.m_xProcedural.m_fRidgedBlend != xB.m_xProcedural.m_fRidgedBlend ||
			xA.m_uLandformCount != xB.m_uLandformCount)
		{
			return false;
		}
		for (u_int i = 0; i < xA.m_uLandformCount; ++i)
		{
			const ZM_TerrainLandformSpec& xLandformA = xA.m_pxLandforms[i];
			const ZM_TerrainLandformSpec& xLandformB = xB.m_pxLandforms[i];
			if (xLandformA.m_xCentre.m_fX != xLandformB.m_xCentre.m_fX ||
				xLandformA.m_xCentre.m_fZ != xLandformB.m_xCentre.m_fZ ||
				xLandformA.m_fRadius != xLandformB.m_fRadius ||
				xLandformA.m_fStrength != xLandformB.m_fStrength ||
				xLandformA.m_fHeight != xLandformB.m_fHeight)
			{
				return false;
			}
		}
		return true;
	}

	bool SamePathPlan(const ZM_TerrainAuthoringRecipe& xA,
		const ZM_TerrainAuthoringRecipe& xB)
	{
		if (xA.m_uPathCount != xB.m_uPathCount)
		{
			return false;
		}
		for (u_int i = 0; i < xA.m_uPathCount; ++i)
		{
			const ZM_TerrainPathSpec& xPathA = xA.m_pxPaths[i];
			const ZM_TerrainPathSpec& xPathB = xB.m_pxPaths[i];
			if (strcmp(xPathA.m_szName, xPathB.m_szName) != 0 ||
				xPathA.m_uPointCount != xPathB.m_uPointCount ||
				xPathA.m_fFlattenRadius != xPathB.m_fFlattenRadius ||
				xPathA.m_fFlattenSpacing != xPathB.m_fFlattenSpacing ||
				xPathA.m_uFlattenSampleCount != xPathB.m_uFlattenSampleCount ||
				xPathA.m_fDirtRadius != xPathB.m_fDirtRadius ||
				xPathA.m_fDirtSpacing != xPathB.m_fDirtSpacing ||
				xPathA.m_uDirtSampleCount != xPathB.m_uDirtSampleCount)
			{
				return false;
			}
			for (u_int uPoint = 0; uPoint < xPathA.m_uPointCount; ++uPoint)
			{
				if (xPathA.m_pxPoints[uPoint].m_fX != xPathB.m_pxPoints[uPoint].m_fX ||
					xPathA.m_pxPoints[uPoint].m_fZ != xPathB.m_pxPoints[uPoint].m_fZ)
				{
					return false;
				}
			}
		}
		return true;
	}

	bool SameMaterialPlan(const ZM_TerrainAuthoringRecipe& xA,
		const ZM_TerrainAuthoringRecipe& xB)
	{
		if (xA.m_uMaterialCount != xB.m_uMaterialCount)
		{
			return false;
		}
		for (u_int i = 0; i < xA.m_uMaterialCount; ++i)
		{
			const ZM_TerrainMaterialSpec& xMaterialA = xA.m_pxMaterials[i];
			const ZM_TerrainMaterialSpec& xMaterialB = xB.m_pxMaterials[i];
			if (strcmp(xMaterialA.m_szName, xMaterialB.m_szName) != 0 ||
				xMaterialA.m_fRoughness != xMaterialB.m_fRoughness ||
				xMaterialA.m_fMetallic != xMaterialB.m_fMetallic ||
				xMaterialA.m_fUVTiling != xMaterialB.m_fUVTiling)
			{
				return false;
			}
			// A textured slot's set directory is part of the plan: swapping the
			// ground maps changes what the terrain looks like just as much as
			// swapping its base colour does.
			const bool bTexturedA = xMaterialA.m_szTextureSetDir != nullptr;
			const bool bTexturedB = xMaterialB.m_szTextureSetDir != nullptr;
			if (bTexturedA != bTexturedB)
			{
				return false;
			}
			if (bTexturedA && strcmp(xMaterialA.m_szTextureSetDir,
				xMaterialB.m_szTextureSetDir) != 0)
			{
				return false;
			}
			for (u_int uChannel = 0; uChannel < 4u; ++uChannel)
			{
				if (xMaterialA.m_afBaseColour[uChannel] !=
					xMaterialB.m_afBaseColour[uChannel])
				{
					return false;
				}
			}
		}
		return true;
	}

	bool SameLandmarkPlan(const ZM_TerrainAuthoringRecipe& xA,
		const ZM_TerrainAuthoringRecipe& xB)
	{
		if (xA.m_uLandmarkCount != xB.m_uLandmarkCount)
		{
			return false;
		}
		for (u_int i = 0; i < xA.m_uLandmarkCount; ++i)
		{
			const ZM_TerrainLandmarkSpec& xLandmarkA = xA.m_pxLandmarks[i];
			const ZM_TerrainLandmarkSpec& xLandmarkB = xB.m_pxLandmarks[i];
			if (strcmp(xLandmarkA.m_szName, xLandmarkB.m_szName) != 0 ||
				xLandmarkA.m_xPosition.m_fX != xLandmarkB.m_xPosition.m_fX ||
				xLandmarkA.m_xPosition.m_fY != xLandmarkB.m_xPosition.m_fY ||
				xLandmarkA.m_xPosition.m_fZ != xLandmarkB.m_xPosition.m_fZ)
			{
				return false;
			}
		}
		return true;
	}

	void AssertPlanOpEqual(const ZM_TerrainPlanOp& xA,
		const ZM_TerrainPlanOp& xB, u_int uIndex)
	{
		ZENITH_ASSERT_EQ((u_int)xA.m_eType, (u_int)xB.m_eType,
			"plan type differs at %u", uIndex);
		ZENITH_ASSERT_EQ((u_int)xA.m_eDabKind, (u_int)xB.m_eDabKind,
			"plan dab kind differs at %u", uIndex);
		ZENITH_ASSERT_EQ((u_int)xA.m_ePhase, (u_int)xB.m_ePhase,
			"plan phase differs at %u", uIndex);
		ZENITH_ASSERT_EQ(xA.m_uIndex, xB.m_uIndex,
			"plan index differs at %u", uIndex);
		ZENITH_ASSERT_EQ_FLOAT(xA.m_fWorldX, xB.m_fWorldX, 0.0f,
			"plan X differs at %u", uIndex);
		ZENITH_ASSERT_EQ_FLOAT(xA.m_fWorldZ, xB.m_fWorldZ, 0.0f,
			"plan Z differs at %u", uIndex);
		ZENITH_ASSERT_EQ_FLOAT(xA.m_fRadius, xB.m_fRadius, 0.0f,
			"plan radius differs at %u", uIndex);
		ZENITH_ASSERT_EQ_FLOAT(xA.m_fStrength, xB.m_fStrength, 0.0f,
			"plan strength differs at %u", uIndex);
		ZENITH_ASSERT_EQ_FLOAT(xA.m_fValue, xB.m_fValue, 0.0f,
			"plan value differs at %u", uIndex);
	}

	bool IsTreeFreeTerrainOp(ZM_TERRAIN_PLAN_OP_TYPE eType)
	{
		switch (eType)
		{
		case ZM_TERRAIN_PLAN_SET_DIMENSIONS:
		case ZM_TERRAIN_PLAN_SET_ASSET_SET:
		case ZM_TERRAIN_PLAN_RESET:
		case ZM_TERRAIN_PLAN_GENERATE_PROCEDURAL:
		case ZM_TERRAIN_PLAN_BRUSH_DAB:
		case ZM_TERRAIN_PLAN_EROSION:
		case ZM_TERRAIN_PLAN_AUTO_SPLAT_RULE:
		case ZM_TERRAIN_PLAN_RUN_AUTO_SPLAT:
		case ZM_TERRAIN_PLAN_TERMINAL_BAKE:
			return true;
		default:
			return false;
		}
	}

	bool IsTreeFreeTerrainDab(ZM_TERRAIN_DAB_KIND eKind)
	{
		switch (eKind)
		{
		case ZM_TERRAIN_DAB_SET_HEIGHT:
		case ZM_TERRAIN_DAB_FLATTEN:
		case ZM_TERRAIN_DAB_SPLAT:
		case ZM_TERRAIN_DAB_GRASS_DENSITY:
			return true;
		default:
			return false;
		}
	}

	std::filesystem::path RecipeManifestTestRoot()
	{
		std::filesystem::path xCursor =
			std::filesystem::absolute(std::filesystem::path(GAME_ASSETS_DIR)).lexically_normal();
		while (!xCursor.empty())
		{
			if (std::filesystem::exists(xCursor / "CLAUDE.md") &&
				std::filesystem::is_directory(xCursor / "Build") &&
				std::filesystem::is_directory(xCursor / "Games" / "Zenithmon"))
			{
				return xCursor / "Build" / "artifacts" /
					"zm_terrain_recipe_set_manifest_test";
			}
			const std::filesystem::path xParent = xCursor.parent_path();
			if (xParent == xCursor)
			{
				break;
			}
			xCursor = xParent;
		}
		return {};
	}

	struct RecipeManifestTestRootGuard
	{
		explicit RecipeManifestTestRootGuard(const std::filesystem::path& xRoot)
			: m_xRoot(xRoot)
		{
			std::error_code xError;
			std::filesystem::remove_all(m_xRoot, xError);
		}

		~RecipeManifestTestRootGuard()
		{
			std::error_code xError;
			std::filesystem::remove_all(m_xRoot, xError);
		}

		std::filesystem::path m_xRoot;
	};

	bool WriteBytes(const std::filesystem::path& xPath,
		const u_int8* pBytes, size_t uSize)
	{
		std::ofstream xOutput(xPath, std::ios::binary | std::ios::trunc);
		if (uSize > 0u)
		{
			xOutput.write(reinterpret_cast<const char*>(pBytes),
				static_cast<std::streamsize>(uSize));
		}
		return static_cast<bool>(xOutput);
	}

	bool WriteNonEmptyFile(const std::filesystem::path& xPath)
	{
		const u_int8 uValue = 0x5au;
		return WriteBytes(xPath, &uValue, 1u);
	}

	u_int ReadU32LE(const u_int8* pBytes)
	{
		return static_cast<u_int>(pBytes[0]) |
			(static_cast<u_int>(pBytes[1]) << 8u) |
			(static_cast<u_int>(pBytes[2]) << 16u) |
			(static_cast<u_int>(pBytes[3]) << 24u);
	}

	void WriteU32LE(u_int8* pBytes, u_int uValue)
	{
		pBytes[0] = static_cast<u_int8>(uValue & 0xffu);
		pBytes[1] = static_cast<u_int8>((uValue >> 8u) & 0xffu);
		pBytes[2] = static_cast<u_int8>((uValue >> 16u) & 0xffu);
		pBytes[3] = static_cast<u_int8>((uValue >> 24u) & 0xffu);
	}

	bool WriteTestManifest(const std::filesystem::path& xPath, u_int uCount)
	{
		u_int8 auBytes[uZM_TERRAIN_MANIFEST_SIZE] = { 'Z', 'M', 'T', 'R' };
		WriteU32LE(auBytes + 4u, uZM_TERRAIN_MANIFEST_VERSION);
		WriteU32LE(auBytes + 8u, uCount);
		return WriteBytes(xPath, auBytes, sizeof(auBytes));
	}

	void AssertSelectionFailure(const char* const* pszArguments,
		int iArgumentCount, ZM_TERRAIN_BAKE_SELECTION_PARSE_RESULT eExpectedResult,
		int iExpectedErrorArgument)
	{
		ZM_TerrainBakeSelection xSelection;
		xSelection.m_eMode = ZM_TERRAIN_BAKE_SELECTION_FORCE_ALL;
		xSelection.m_uSelectedRecipeMask = 0xffffffffu;
		xSelection.m_iErrorArgument = 12345;
		ZENITH_ASSERT_FALSE(ZM_ParseTerrainBakeSelection(
			iArgumentCount, pszArguments, xSelection));
		ZENITH_ASSERT_EQ((u_int)xSelection.m_eParseResult,
			(u_int)eExpectedResult);
		ZENITH_ASSERT_EQ(xSelection.m_iErrorArgument, iExpectedErrorArgument,
			"parser did not report the first offending argv index");
	}
}

ZENITH_TEST(ZM_TerrainRecipeSet, RegistryHasExactlyThreeWorldSpecRecipesInFixedOrder)
{
	ZENITH_ASSERT_EQ(ZM_GetTerrainAuthoringRecipeCount(), 3u);
	ZENITH_ASSERT_EQ(ZM_GetTerrainAuthoringRecipeCount(), uZM_TERRAIN_RECIPE_COUNT);
	// chunks x 3 mesh files + Height/Splatmap_RGBA/GrassDensity + TerrainDims.zdata,
	// on grids sized to each map's own content: 9x10, 13x15 and 11x24 where all
	// three used to be a 16-wide slice of one fixed 4096 m terrain.
	ZENITH_ASSERT_EQ(uZM_DAWNMERE_REQUIRED_OUTPUT_COUNT, 52u);
	ZENITH_ASSERT_EQ(uZM_THORNACRE_REQUIRED_OUTPUT_COUNT, 589u);
	ZENITH_ASSERT_EQ(uZM_ROUTE1_REQUIRED_OUTPUT_COUNT, 796u);

	for (u_int i = 0; i < uZM_TERRAIN_RECIPE_COUNT; ++i)
	{
		const ZM_ExpectedTerrainRecipe& xExpected = s_axExpectedRecipes[i];
		const ZM_TerrainAuthoringRecipe& xRecipe = ZM_GetTerrainAuthoringRecipe(i);
		const ZM_WorldSpec& xWorld = ZM_GetWorldSpec(xExpected.m_eSceneId);
		ZENITH_ASSERT_TRUE(xRecipe.m_pxWorldSpec == &xWorld,
			"recipe %u must bind its canonical WorldSpec row", i);
		ZENITH_ASSERT_EQ((u_int)xWorld.m_eId, (u_int)xExpected.m_eSceneId);
		ZENITH_ASSERT_EQ((u_int)xWorld.m_eKind, (u_int)xExpected.m_eKind);
		ZENITH_ASSERT_EQ(xWorld.m_uBuildIndex, xExpected.m_uBuildIndex);
		ZENITH_ASSERT_STREQ(xWorld.m_szTerrainSet, xExpected.m_szTerrainSet);
		ZENITH_ASSERT_EQ(ZM_Fnv1a32(xWorld.m_szTerrainSet), xExpected.m_uSeed);
		ZENITH_ASSERT_EQ(xRecipe.m_uSeed, xExpected.m_uSeed);
		ZENITH_ASSERT_TRUE(ZM_FindTerrainAuthoringRecipe(xExpected.m_eSceneId) == &xRecipe);
		ZENITH_ASSERT_TRUE(&ZM_GetTerrainAuthoringRecipe(i) == &xRecipe,
			"registry references must be stable");

		ZENITH_ASSERT_EQ_FLOAT(xRecipe.WorldMinX(), 0.0f, fEPSILON);
		ZENITH_ASSERT_EQ_FLOAT(xRecipe.WorldMinZ(), 0.0f, fEPSILON);
		ZENITH_ASSERT_EQ_FLOAT(xRecipe.WorldMaxX(),
			static_cast<float>(xExpected.m_iChunkWidth) * fTERRAIN_CHUNK_SIZE,
			fEPSILON);
		ZENITH_ASSERT_EQ_FLOAT(xRecipe.WorldMaxZ(),
			static_cast<float>(xExpected.m_iChunkHeight) * fTERRAIN_CHUNK_SIZE,
			fEPSILON);
		ZENITH_ASSERT_EQ(xRecipe.ExportRect().m_iMinX, 0);
		ZENITH_ASSERT_EQ(xRecipe.ExportRect().m_iMinY, 0);
		ZENITH_ASSERT_EQ(xRecipe.ExportRect().m_iMaxX,
			xExpected.m_iChunkWidth - 1);
		ZENITH_ASSERT_EQ(xRecipe.ExportRect().m_iMaxY,
			xExpected.m_iChunkHeight - 1);
		ZENITH_ASSERT_EQ(ZM_GetTerrainRequiredOutputCount(xRecipe),
			xExpected.m_uRequiredOutputCount);
		ZENITH_ASSERT_EQ(ZM_GetTerrainRequiredOutputCount(xRecipe) + 1u,
			xExpected.m_uFamilyFileCount);
	}

	ZENITH_ASSERT_TRUE(&ZM_GetDawnmereTerrainRecipe() ==
		&ZM_GetTerrainAuthoringRecipe(0u));
	ZENITH_ASSERT_TRUE(&ZM_GetThornacreTerrainRecipe() ==
		&ZM_GetTerrainAuthoringRecipe(1u));
	ZENITH_ASSERT_TRUE(&ZM_GetRoute1TerrainRecipe() ==
		&ZM_GetTerrainAuthoringRecipe(2u));

	ZENITH_ASSERT_TRUE(ZM_FindTerrainAuthoringRecipe(ZM_SCENE_FRONTEND) == nullptr);
	ZENITH_ASSERT_TRUE(ZM_FindTerrainAuthoringRecipe(ZM_SCENE_BATTLE) == nullptr);
	ZENITH_ASSERT_TRUE(ZM_FindTerrainAuthoringRecipe(ZM_SCENE_PLAYERHOME) == nullptr);
	ZENITH_ASSERT_TRUE(ZM_FindTerrainAuthoringRecipe(ZM_SCENE_PROFLAB) == nullptr);
	ZENITH_ASSERT_TRUE(ZM_FindTerrainAuthoringRecipe(ZM_SCENE_GYM1) == nullptr);
	ZENITH_ASSERT_TRUE(ZM_FindTerrainAuthoringRecipe(ZM_SCENE_NONE) == nullptr);
}

// Dawnmere's meadow, stone and dirt slots are the TEXTURED terrain materials:
// they sample the ENGINE's shared grass, rock and clay ground sets
// (Zenith/Assets/Textures/Terrain/{Grass,Rock,Clay}); the first two are the maps
// RenderTest's terrain uses on the same two slots. Two things can silently undo
// that -- someone re-tinting a slot (base colour multiplies the sampled diffuse,
// so a green, grey or brown tint puts the flat look back), and someone moving a
// set out from under its "engine:" ref. Both are pinned here.
ZENITH_TEST(ZM_TerrainRecipeSet, DawnmereGroundSlotsSampleTheSharedEngineSets)
{
	const ZM_TerrainAuthoringRecipe& xDawnmere = ZM_GetDawnmereTerrainRecipe();

	const char* aszSlotNames[] = { "Meadow", "Stone", "Dirt" };
	const char* aszSetRefs[] =
		{ "engine:Textures/Terrain/Grass/", "engine:Textures/Terrain/Rock/",
		  "engine:Textures/Terrain/Clay/" };
	const char* aszSetDirs[] = { "Grass", "Rock", "Clay" };
	constexpr u_int uTEXTURED_SLOT_COUNT = 3u;

	for (u_int uSlot = 0; uSlot < uTEXTURED_SLOT_COUNT; ++uSlot)
	{
		const ZM_TerrainMaterialSpec& xGround = xDawnmere.m_pxMaterials[uSlot];

		ZENITH_ASSERT_STREQ(xGround.m_szName, aszSlotNames[uSlot]);
		ZENITH_ASSERT_TRUE(xGround.m_szTextureSetDir != nullptr,
			"Dawnmere's %s slot lost its texture set and is a flat colour again",
			aszSlotNames[uSlot]);
		ZENITH_ASSERT_STREQ(xGround.m_szTextureSetDir, aszSetRefs[uSlot]);
		ZENITH_ASSERT_GT(xGround.m_fUVTiling, 0.0f);
		for (u_int uChannel = 0; uChannel < 4u; ++uChannel)
		{
			ZENITH_ASSERT_EQ_FLOAT(xGround.m_afBaseColour[uChannel], 1.0f, fEPSILON);
		}

		// The maps themselves are gitignored workspace assets, so their absence is
		// a cold clone rather than a defect -- but a set that IS present must be
		// complete, or the slot silently falls back to default textures.
		const std::filesystem::path xSetDir =
			std::filesystem::path(ENGINE_ASSETS_DIR) / "Textures" / "Terrain" /
			aszSetDirs[uSlot];
		if (std::filesystem::exists(xSetDir))
		{
			const char* aszMaps[] = { "diffuse", "normal", "rm_packed", "ao" };
			for (const char* szMap : aszMaps)
			{
				const std::filesystem::path xMap =
					xSetDir / (std::string(szMap) + ZENITH_TEXTURE_EXT);
				ZENITH_ASSERT_TRUE(std::filesystem::exists(xMap),
					"shared %s set is missing %s", aszSetDirs[uSlot],
					xMap.generic_string().c_str());
			}
		}
	}

	// Heath, the one remaining slot, stays flat-colour by design.
	for (u_int uSlot = uTEXTURED_SLOT_COUNT; uSlot < xDawnmere.m_uMaterialCount; ++uSlot)
	{
		ZENITH_ASSERT_TRUE(xDawnmere.m_pxMaterials[uSlot].m_szTextureSetDir == nullptr);
	}
}

ZENITH_TEST(ZM_TerrainRecipeSet, RecipesCarryDistinctDocumentedOutdoorPlans)
{
	const ZM_TerrainAuthoringRecipe& xDawnmere = ZM_GetDawnmereTerrainRecipe();
	const ZM_TerrainAuthoringRecipe& xThornacre = ZM_GetThornacreTerrainRecipe();
	const ZM_TerrainAuthoringRecipe& xRoute1 = ZM_GetRoute1TerrainRecipe();

	const char* aszDawnmerePaths[] = { "Route", "Home", "Lab" };
	const char* aszDawnmereMaterials[] = { "Meadow", "Dirt" };
	const char* aszThornacrePaths[] = { "MainLane", "GymLane", "BerryRow" };
	const char* aszThornacreMaterials[] =
		{ "Drystone", "Dirt", "Hedgerow" };
	const char* aszRoute1Paths[] = { "DirtLane", "RivalSpur" };
	const char* aszRoute1Materials[] = { "CoastalMeadow", "Dirt" };

	for (u_int i = 0; i < sizeof(aszDawnmerePaths) / sizeof(aszDawnmerePaths[0]); ++i)
	{
		ZENITH_ASSERT_TRUE(HasPathNamed(xDawnmere, aszDawnmerePaths[i]));
	}
	for (u_int i = 0; i < sizeof(aszDawnmereMaterials) / sizeof(aszDawnmereMaterials[0]); ++i)
	{
		ZENITH_ASSERT_TRUE(HasMaterialNamed(xDawnmere, aszDawnmereMaterials[i]));
		ZENITH_ASSERT_TRUE(HasAutoSplatNamed(xDawnmere, aszDawnmereMaterials[i]));
	}
	for (u_int i = 0; i < sizeof(aszThornacrePaths) / sizeof(aszThornacrePaths[0]); ++i)
	{
		ZENITH_ASSERT_TRUE(HasPathNamed(xThornacre, aszThornacrePaths[i]));
	}
	for (u_int i = 0; i < sizeof(aszThornacreMaterials) / sizeof(aszThornacreMaterials[0]); ++i)
	{
		ZENITH_ASSERT_TRUE(HasMaterialNamed(xThornacre, aszThornacreMaterials[i]));
		ZENITH_ASSERT_TRUE(HasAutoSplatNamed(xThornacre, aszThornacreMaterials[i]));
	}
	for (u_int i = 0; i < sizeof(aszRoute1Paths) / sizeof(aszRoute1Paths[0]); ++i)
	{
		ZENITH_ASSERT_TRUE(HasPathNamed(xRoute1, aszRoute1Paths[i]));
	}
	for (u_int i = 0; i < sizeof(aszRoute1Materials) / sizeof(aszRoute1Materials[0]); ++i)
	{
		ZENITH_ASSERT_TRUE(HasMaterialNamed(xRoute1, aszRoute1Materials[i]));
		ZENITH_ASSERT_TRUE(HasAutoSplatNamed(xRoute1, aszRoute1Materials[i]));
	}

	for (u_int i = 0; i < uZM_TERRAIN_RECIPE_COUNT; ++i)
	{
		const ZM_TerrainAuthoringRecipe& xRecipe = ZM_GetTerrainAuthoringRecipe(i);
		ZENITH_ASSERT_TRUE(xRecipe.m_pxLandforms != nullptr && xRecipe.m_uLandformCount > 0u);
		ZENITH_ASSERT_TRUE(xRecipe.m_pxPaths != nullptr && xRecipe.m_uPathCount > 0u);
		ZENITH_ASSERT_TRUE(xRecipe.m_pxPads != nullptr && xRecipe.m_uPadCount > 0u);
		ZENITH_ASSERT_TRUE(xRecipe.m_pxAutoSplat != nullptr && xRecipe.m_uAutoSplatCount == 4u);
		ZENITH_ASSERT_TRUE(xRecipe.m_pxGrassDabs != nullptr && xRecipe.m_uGrassDabCount > 0u);
		ZENITH_ASSERT_TRUE(xRecipe.m_pxLandmarks != nullptr && xRecipe.m_uLandmarkCount > 0u);
		ZENITH_ASSERT_TRUE(xRecipe.m_pxMaterials != nullptr && xRecipe.m_uMaterialCount == 4u);
		ZENITH_ASSERT_GT(xRecipe.m_xProcedural.m_fAmplitude, 0.0f);
		ZENITH_ASSERT_GT(xRecipe.m_xProcedural.m_fFrequency, 0.0f);
		ZENITH_ASSERT_GT(xRecipe.m_xProcedural.m_uOctaves, 0u);
		ZENITH_ASSERT_GT(xRecipe.m_xProcedural.m_fLacunarity, 0.0f);
		ZENITH_ASSERT_GT(xRecipe.m_xProcedural.m_fGain, 0.0f);
		ZENITH_ASSERT_GE(xRecipe.m_xProcedural.m_fRidgedBlend, 0.0f);
		ZENITH_ASSERT_LE(xRecipe.m_xProcedural.m_fRidgedBlend, 1.0f);

		ZENITH_ASSERT_TRUE(xRecipe.m_xErosion.m_bRegionOnly,
			"measurement recipes must use regional rather than full-sheet erosion");
		ZENITH_ASSERT_GT(xRecipe.m_xErosion.m_fRadius, 0.0f);
		ZENITH_ASSERT_GE(xRecipe.m_xErosion.m_xCentre.m_fX, xRecipe.WorldMinX());
		ZENITH_ASSERT_LE(xRecipe.m_xErosion.m_xCentre.m_fX, xRecipe.WorldMaxX());
		ZENITH_ASSERT_GE(xRecipe.m_xErosion.m_xCentre.m_fZ, xRecipe.WorldMinZ());
		ZENITH_ASSERT_LE(xRecipe.m_xErosion.m_xCentre.m_fZ, xRecipe.WorldMaxZ());

		for (u_int uPath = 0; uPath < xRecipe.m_uPathCount; ++uPath)
		{
			const ZM_TerrainPathSpec& xPath = xRecipe.m_pxPaths[uPath];
			ZENITH_ASSERT_TRUE(xPath.m_szName != nullptr && xPath.m_szName[0] != '\0');
			ZENITH_ASSERT_TRUE(xPath.m_pxPoints != nullptr && xPath.m_uPointCount >= 2u);
			ZENITH_ASSERT_GT(xPath.m_fFlattenRadius, 0.0f);
			ZENITH_ASSERT_GT(xPath.m_fFlattenSpacing, 0.0f);
			ZENITH_ASSERT_GT(xPath.m_uFlattenSampleCount, 0u);
			ZENITH_ASSERT_GT(xPath.m_fDirtRadius, 0.0f);
			ZENITH_ASSERT_GT(xPath.m_fDirtSpacing, 0.0f);
			ZENITH_ASSERT_GT(xPath.m_uDirtSampleCount, 0u);
		}
		for (u_int uGrass = 0; uGrass < xRecipe.m_uGrassDabCount; ++uGrass)
		{
			const ZM_TerrainGrassDabSpec& xGrass = xRecipe.m_pxGrassDabs[uGrass];
			ZENITH_ASSERT_GT(xGrass.m_fRadius, 0.0f);
			ZENITH_ASSERT_GT(xGrass.m_fTargetDensity, 0.0f);
			ZENITH_ASSERT_LE(xGrass.m_fTargetDensity, 1.0f);
		}
		for (u_int uMaterial = 0; uMaterial < xRecipe.m_uMaterialCount; ++uMaterial)
		{
			const ZM_TerrainMaterialSpec& xMaterial =
				xRecipe.m_pxMaterials[uMaterial];
			ZENITH_ASSERT_TRUE(HasAutoSplatNamed(
				xRecipe, xMaterial.m_szName),
				"%s material '%s' has no matching auto-splat rule",
				xRecipe.m_pxWorldSpec->m_szTerrainSet,
				xMaterial.m_szName);
			for (u_int uChannel = 0; uChannel < 4u; ++uChannel)
			{
				ZENITH_ASSERT_GE(xMaterial.m_afBaseColour[uChannel], 0.0f);
				ZENITH_ASSERT_LE(xMaterial.m_afBaseColour[uChannel], 1.0f);
			}
			ZENITH_ASSERT_EQ_FLOAT(xMaterial.m_afBaseColour[3], 1.0f, fEPSILON);
			ZENITH_ASSERT_GE(xMaterial.m_fRoughness, 0.0f);
			ZENITH_ASSERT_LE(xMaterial.m_fRoughness, 1.0f);
			ZENITH_ASSERT_EQ_FLOAT(xMaterial.m_fMetallic, 0.0f, fEPSILON);
			if (xMaterial.m_szTextureSetDir)
			{
				// A textured slot needs a usable tiling and a directory ref the
				// asset registry can resolve -- prefixed and slash-terminated,
				// because the loader concatenates the map stems onto it.
				const std::string strSetDir = xMaterial.m_szTextureSetDir;
				ZENITH_ASSERT_GT(xMaterial.m_fUVTiling, 0.0f);
				ZENITH_ASSERT_TRUE(!strSetDir.empty() && strSetDir.back() == '/',
					"%s material '%s' texture set '%s' must end in '/'",
					xRecipe.m_pxWorldSpec->m_szTerrainSet,
					xMaterial.m_szName, strSetDir.c_str());
				ZENITH_ASSERT_TRUE(
					strSetDir.rfind("engine:", 0) == 0 ||
					strSetDir.rfind("game:", 0) == 0,
					"%s material '%s' texture set '%s' must carry an engine:/game: prefix",
					xRecipe.m_pxWorldSpec->m_szTerrainSet,
					xMaterial.m_szName, strSetDir.c_str());
			}
			else
			{
				ZENITH_ASSERT_EQ_FLOAT(xMaterial.m_fUVTiling, 0.0f, fEPSILON);
			}
		}
		for (u_int uRule = 0; uRule < xRecipe.m_uAutoSplatCount; ++uRule)
		{
			const ZM_TerrainAutoSplatSpec& xRule = xRecipe.m_pxAutoSplat[uRule];
			ZENITH_ASSERT_TRUE(HasMaterialNamed(xRecipe, xRule.m_szName));
			ZENITH_ASSERT_LE(xRule.m_fHeightMin, xRule.m_fHeightMax);
			ZENITH_ASSERT_LE(xRule.m_fSlopeMin, xRule.m_fSlopeMax);
			ZENITH_ASSERT_GT(xRule.m_fWeight, 0.0f);
			ZENITH_ASSERT_GE(xRule.m_fJitter, 0.0f);
			ZENITH_ASSERT_LE(xRule.m_fJitter, 1.0f);
		}
		ZENITH_ASSERT_GE(xRecipe.m_xPreviewCamera.m_xPosition.m_fX,
			xRecipe.WorldMinX());
		ZENITH_ASSERT_LE(xRecipe.m_xPreviewCamera.m_xPosition.m_fX,
			xRecipe.WorldMaxX());
		ZENITH_ASSERT_GE(xRecipe.m_xPreviewCamera.m_xPosition.m_fZ,
			xRecipe.WorldMinZ());
		ZENITH_ASSERT_LE(xRecipe.m_xPreviewCamera.m_xPosition.m_fZ,
			xRecipe.WorldMaxZ());
		ZENITH_ASSERT_GT(xRecipe.m_xPreviewCamera.m_fFovDegrees, 0.0f);
		ZENITH_ASSERT_LT(xRecipe.m_xPreviewCamera.m_fFovDegrees, 180.0f);
		ZENITH_ASSERT_GT(xRecipe.m_xPreviewCamera.m_fNearPlane, 0.0f);
		ZENITH_ASSERT_GT(xRecipe.m_xPreviewCamera.m_fFarPlane,
			xRecipe.m_xPreviewCamera.m_fNearPlane);

		const ZM_WorldSpec& xWorld = *xRecipe.m_pxWorldSpec;
		for (u_int uTag = 0; uTag < xWorld.m_uSpawnTagCount; ++uTag)
		{
			ZENITH_ASSERT_TRUE(HasLandmarkNamed(xRecipe, xWorld.m_pszSpawnTags[uTag]),
				"%s recipe is missing required spawn landmark '%s'",
				xWorld.m_szTerrainSet, xWorld.m_pszSpawnTags[uTag]);
		}
		for (u_int uLandmark = 0; uLandmark < xRecipe.m_uLandmarkCount; ++uLandmark)
		{
			const ZM_TerrainPoint3& xPoint =
				xRecipe.m_pxLandmarks[uLandmark].m_xPosition;
			ZENITH_ASSERT_GE(xPoint.m_fX, xRecipe.WorldMinX());
			ZENITH_ASSERT_LE(xPoint.m_fX, xRecipe.WorldMaxX());
			ZENITH_ASSERT_GE(xPoint.m_fZ, xRecipe.WorldMinZ());
			ZENITH_ASSERT_LE(xPoint.m_fZ, xRecipe.WorldMaxZ());
		}
	}

	const ZM_TerrainAuthoringRecipe* apxRecipes[] =
		{ &xDawnmere, &xThornacre, &xRoute1 };
	for (u_int i = 0; i < uZM_TERRAIN_RECIPE_COUNT; ++i)
	{
		for (u_int j = i + 1u; j < uZM_TERRAIN_RECIPE_COUNT; ++j)
		{
			ZENITH_ASSERT_FALSE(SameShapePlan(*apxRecipes[i], *apxRecipes[j]),
				"outdoor recipes %u and %u reused one shape plan", i, j);
			ZENITH_ASSERT_FALSE(SamePathPlan(*apxRecipes[i], *apxRecipes[j]),
				"outdoor recipes %u and %u reused one path plan", i, j);
			ZENITH_ASSERT_FALSE(SameMaterialPlan(*apxRecipes[i], *apxRecipes[j]),
				"outdoor recipes %u and %u reused one material plan", i, j);
			ZENITH_ASSERT_FALSE(SameLandmarkPlan(*apxRecipes[i], *apxRecipes[j]),
				"outdoor recipes %u and %u reused one landmark plan", i, j);
		}
	}
}

ZENITH_TEST(ZM_TerrainRecipeSet, PlansAreDeterministicContainedAndEndWithGrassErase)
{
	for (u_int uRecipe = 0; uRecipe < uZM_TERRAIN_RECIPE_COUNT; ++uRecipe)
	{
		const ZM_TerrainAuthoringRecipe& xRecipe =
			ZM_GetTerrainAuthoringRecipe(uRecipe);
		Zenith_Vector<ZM_TerrainPlanOp> xPlanA;
		Zenith_Vector<ZM_TerrainPlanOp> xPlanB;
		ZM_BuildTerrainAuthoringPlan(xRecipe, xPlanA);
		ZM_BuildTerrainAuthoringPlan(xRecipe, xPlanB);

		ZENITH_ASSERT_EQ(xPlanA.GetSize(), xPlanB.GetSize());
		ZENITH_ASSERT_GT(xPlanA.GetSize(), 3u);
		// Every recipe stages its SHAPE first: the bake runs on a standalone
		// editor session, so nothing downstream can infer the grid from a
		// component, and every coordinate after this point is world-space
		// against it.
		ZENITH_ASSERT_EQ((u_int)xPlanA.Get(0u).m_eType,
			(u_int)ZM_TERRAIN_PLAN_SET_DIMENSIONS);
		ZENITH_ASSERT_EQ((u_int)xPlanA.Get(1u).m_eType,
			(u_int)ZM_TERRAIN_PLAN_RESET);
		ZENITH_ASSERT_EQ((u_int)xPlanA.Get(2u).m_eType,
			(u_int)ZM_TERRAIN_PLAN_SET_ASSET_SET);
		ZENITH_ASSERT_EQ((u_int)xPlanA.Get(3u).m_eType,
			(u_int)ZM_TERRAIN_PLAN_GENERATE_PROCEDURAL);
		ZENITH_ASSERT_EQ((u_int)xPlanA.GetBack().m_eType,
			(u_int)ZM_TERRAIN_PLAN_TERMINAL_BAKE);

		u_int uResetCount = 0u;
		u_int uSetCount = 0u;
		u_int uProceduralCount = 0u;
		u_int uErosionCount = 0u;
		u_int uRunAutoSplatCount = 0u;
		u_int uTerminalCount = 0u;
		u_int uGrassFillCount = 0u;
		u_int uGrassEraseCount = 0u;
		u_int uLastDensityIndex = UINT_MAX;
		bool bEraseSeen = false;
		for (u_int i = 0; i < xPlanA.GetSize(); ++i)
		{
			const ZM_TerrainPlanOp& xOp = xPlanA.Get(i);
			AssertPlanOpEqual(xOp, xPlanB.Get(i), i);
			ZENITH_ASSERT_TRUE(IsTreeFreeTerrainOp(xOp.m_eType),
				"recipe %u introduced a non-terrain/tree operation at %u", uRecipe, i);

			switch (xOp.m_eType)
			{
			case ZM_TERRAIN_PLAN_RESET: ++uResetCount; break;
			case ZM_TERRAIN_PLAN_SET_ASSET_SET: ++uSetCount; break;
			case ZM_TERRAIN_PLAN_GENERATE_PROCEDURAL: ++uProceduralCount; break;
			case ZM_TERRAIN_PLAN_EROSION: ++uErosionCount; break;
			case ZM_TERRAIN_PLAN_RUN_AUTO_SPLAT: ++uRunAutoSplatCount; break;
			case ZM_TERRAIN_PLAN_TERMINAL_BAKE: ++uTerminalCount; break;
			default: break;
			}

			if (xOp.m_eType != ZM_TERRAIN_PLAN_BRUSH_DAB)
			{
				continue;
			}
			ZENITH_ASSERT_TRUE(IsTreeFreeTerrainDab(xOp.m_eDabKind),
				"recipe %u introduced an unsupported/tree dab at %u", uRecipe, i);
			ZENITH_ASSERT_GT(xOp.m_fRadius, 0.0f);
			ZENITH_ASSERT_GE(xOp.m_fWorldX - xOp.m_fRadius,
				xRecipe.WorldMinX(),
				"recipe %u dab %u escaped minimum X", uRecipe, i);
			ZENITH_ASSERT_LE(xOp.m_fWorldX + xOp.m_fRadius,
				xRecipe.WorldMaxX(),
				"recipe %u dab %u escaped maximum X", uRecipe, i);
			ZENITH_ASSERT_GE(xOp.m_fWorldZ - xOp.m_fRadius,
				xRecipe.WorldMinZ(),
				"recipe %u dab %u escaped minimum Z", uRecipe, i);
			ZENITH_ASSERT_LE(xOp.m_fWorldZ + xOp.m_fRadius,
				xRecipe.WorldMaxZ(),
				"recipe %u dab %u escaped maximum Z", uRecipe, i);

			if (xOp.m_eDabKind != ZM_TERRAIN_DAB_GRASS_DENSITY)
			{
				continue;
			}
			uLastDensityIndex = i;
			if (xOp.m_ePhase == ZM_TERRAIN_PHASE_GRASS_ERASE)
			{
				bEraseSeen = true;
				++uGrassEraseCount;
				ZENITH_ASSERT_EQ_FLOAT(xOp.m_fValue, 0.0f, 0.0f);
			}
			else
			{
				ZENITH_ASSERT_EQ((u_int)xOp.m_ePhase,
					(u_int)ZM_TERRAIN_PHASE_GRASS_FILL);
				ZENITH_ASSERT_FALSE(bEraseSeen,
					"recipe %u repopulated grass after erase at op %u", uRecipe, i);
				++uGrassFillCount;
			}
		}

		ZENITH_ASSERT_EQ(uResetCount, 1u);
		ZENITH_ASSERT_EQ(uSetCount, 1u);
		ZENITH_ASSERT_EQ(uProceduralCount, 1u);
		ZENITH_ASSERT_EQ(uErosionCount, 1u);
		ZENITH_ASSERT_EQ(uRunAutoSplatCount, 1u);
		ZENITH_ASSERT_EQ(uTerminalCount, 1u);
		ZENITH_ASSERT_EQ(uGrassFillCount, xRecipe.m_uGrassDabCount);
		ZENITH_ASSERT_GT(uGrassEraseCount, 0u);
		ZENITH_ASSERT_NE(uLastDensityIndex, UINT_MAX);
		ZENITH_ASSERT_EQ(uLastDensityIndex + 1u, xPlanA.GetSize() - 1u,
			"grass erase must be the final recipe phase before terminal bake");
	}
}

ZENITH_TEST(ZM_TerrainRecipeSet, OutputsAreUniqueSetContainedAndQueuePolicyIsPure)
{
	for (u_int uRecipe = 0; uRecipe < uZM_TERRAIN_RECIPE_COUNT; ++uRecipe)
	{
		const ZM_TerrainAuthoringRecipe& xRecipe =
			ZM_GetTerrainAuthoringRecipe(uRecipe);
		const ZM_ExpectedTerrainRecipe& xExpected = s_axExpectedRecipes[uRecipe];
		Zenith_Vector<std::string> xOutputsA;
		Zenith_Vector<std::string> xOutputsB;
		ZM_EnumerateRequiredTerrainOutputs(xRecipe, xOutputsA);
		ZM_EnumerateRequiredTerrainOutputs(xRecipe, xOutputsB);
		ZENITH_ASSERT_EQ(xOutputsA.GetSize(), xExpected.m_uRequiredOutputCount);
		ZENITH_ASSERT_EQ(xOutputsA.GetSize(), xOutputsB.GetSize());

		const u_int uArea = static_cast<u_int>(
			xExpected.m_iChunkWidth * xExpected.m_iChunkHeight);
		const std::string strDirectory =
			std::string("Terrain/") + xExpected.m_szTerrainSet;
		const std::string strPrefix = strDirectory + "/";
		std::set<std::string> xUnique;
		u_int uRenderCount = 0u;
		u_int uRenderLowCount = 0u;
		u_int uPhysicsCount = 0u;
		u_int uTextureCount = 0u;
		for (u_int i = 0; i < xOutputsA.GetSize(); ++i)
		{
			const std::string& strOutput = xOutputsA.Get(i);
			ZENITH_ASSERT_STREQ(strOutput.c_str(), xOutputsB.Get(i).c_str());
			ZENITH_ASSERT_TRUE(strOutput.rfind(strPrefix, 0u) == 0u,
				"recipe %u output escaped set: %s", uRecipe, strOutput.c_str());
			ZENITH_ASSERT_TRUE(std::filesystem::path(strOutput).parent_path().generic_string() ==
				strDirectory,
				"recipe %u output was not a direct set child: %s",
				uRecipe, strOutput.c_str());
			xUnique.insert(strOutput);

			const std::string strName =
				std::filesystem::path(strOutput).filename().string();
			if (strName.rfind("Render_LOW_", 0u) == 0u) ++uRenderLowCount;
			else if (strName.rfind("Render_", 0u) == 0u) ++uRenderCount;
			else if (strName.rfind("Physics_", 0u) == 0u) ++uPhysicsCount;
			else ++uTextureCount;
		}

		ZENITH_ASSERT_EQ(xUnique.size(),
			static_cast<size_t>(xExpected.m_uRequiredOutputCount));
		ZENITH_ASSERT_EQ(uRenderCount, uArea);
		ZENITH_ASSERT_EQ(uRenderLowCount, uArea);
		ZENITH_ASSERT_EQ(uPhysicsCount, uArea);
		// Three .ztxtr maps plus TerrainDims.zdata -- the manifest lands in this
		// bucket because it is not a Render_/Render_LOW_/Physics_ chunk.
		ZENITH_ASSERT_EQ(uTextureCount, 4u);

		const char* aszMeshPrefixes[] = { "Render", "Render_LOW", "Physics" };
		u_int uExpectedOutputIndex = 0u;
		for (u_int uPrefix = 0; uPrefix < 3u; ++uPrefix)
		{
			for (int iY = xRecipe.ExportRect().m_iMinY;
				iY <= xRecipe.ExportRect().m_iMaxY; ++iY)
			{
				for (int iX = xRecipe.ExportRect().m_iMinX;
					iX <= xRecipe.ExportRect().m_iMaxX; ++iX)
				{
					const std::string strExpected = strPrefix + aszMeshPrefixes[uPrefix] +
						"_" + std::to_string(iX) + "_" + std::to_string(iY) + ".zgeom";
					ZENITH_ASSERT_STREQ(xOutputsA.Get(uExpectedOutputIndex).c_str(),
						strExpected.c_str(),
						"recipe %u output order drifted at %u",
						uRecipe, uExpectedOutputIndex);
					++uExpectedOutputIndex;
				}
			}
		}
		ZENITH_ASSERT_EQ(uExpectedOutputIndex, uArea * 3u);
		// The four non-chunk outputs, in emission order. TerrainDims.zdata is
		// LAST and is a REQUIRED output: a set missing it is a stale bake the
		// runtime loader refuses outright.
		ZENITH_ASSERT_STREQ(xOutputsA.Get(xOutputsA.GetSize() - 4u).c_str(),
			(strPrefix + "Height.ztxtr").c_str());
		ZENITH_ASSERT_STREQ(xOutputsA.Get(xOutputsA.GetSize() - 3u).c_str(),
			(strPrefix + "Splatmap_RGBA.ztxtr").c_str());
		ZENITH_ASSERT_STREQ(xOutputsA.Get(xOutputsA.GetSize() - 2u).c_str(),
			(strPrefix + "GrassDensity.ztxtr").c_str());
		ZENITH_ASSERT_STREQ(xOutputsA.GetBack().c_str(),
			(strPrefix + Zenith_TerrainDimsManifestFormat::szFILENAME).c_str());
		ZENITH_ASSERT_STREQ(ZM_GetTerrainManifestRelativePath(xRecipe).c_str(),
			(strPrefix + "ZM_TerrainRecipe.manifest").c_str());
		ZENITH_ASSERT_STREQ(ZM_GetTerrainGrassAssetPath(xRecipe).c_str(),
			(std::string("game:") + strPrefix + "GrassDensity.ztxtr").c_str());
	}

	ZENITH_ASSERT_STREQ(szZM_FORCE_TERRAIN_BAKE_FLAG, "--zm-force-terrain-bake");
	ZENITH_ASSERT_EQ((u_int)ZM_DetermineTerrainBakeQueueResult(true, false, false, false),
		(u_int)ZM_TERRAIN_BAKE_HEADLESS);
	ZENITH_ASSERT_EQ((u_int)ZM_DetermineTerrainBakeQueueResult(true, true, true, true),
		(u_int)ZM_TERRAIN_BAKE_HEADLESS,
		"headless must win over force, warm state, and preparation");
	ZENITH_ASSERT_EQ((u_int)ZM_DetermineTerrainBakeQueueResult(false, false, true, false),
		(u_int)ZM_TERRAIN_BAKE_WARM);
	ZENITH_ASSERT_EQ((u_int)ZM_DetermineTerrainBakeQueueResult(false, false, false, true),
		(u_int)ZM_TERRAIN_BAKE_QUEUED);
	ZENITH_ASSERT_EQ((u_int)ZM_DetermineTerrainBakeQueueResult(false, true, true, true),
		(u_int)ZM_TERRAIN_BAKE_QUEUED,
		"force must queue a warm recipe again");
	ZENITH_ASSERT_EQ((u_int)ZM_DetermineTerrainBakeQueueResult(false, false, false, false),
		(u_int)ZM_TERRAIN_BAKE_PREPARE_FAILED);
	ZENITH_ASSERT_EQ((u_int)ZM_DetermineTerrainBakeQueueResult(false, true, true, false),
		(u_int)ZM_TERRAIN_BAKE_PREPARE_FAILED);

	// No selector means "bake missing". Unrelated command-line flags are
	// deliberately ignored by this focused parser.
	const char* const aszAutoArguments[] =
		{ "zenithmon.exe", "--skip-unit-tests" };
	ZM_TerrainBakeSelection xAutoSelection;
	ZENITH_ASSERT_TRUE(ZM_ParseTerrainBakeSelection(
		2, aszAutoArguments, xAutoSelection));
	ZENITH_ASSERT_EQ((u_int)xAutoSelection.m_eMode,
		(u_int)ZM_TERRAIN_BAKE_SELECTION_AUTO_MISSING);
	ZENITH_ASSERT_EQ(xAutoSelection.m_uSelectedRecipeMask, 0u);
	ZENITH_ASSERT_EQ(xAutoSelection.m_iErrorArgument, -1);
	ZENITH_ASSERT_EQ((u_int)xAutoSelection.m_eParseResult,
		(u_int)ZM_TERRAIN_BAKE_SELECTION_PARSE_OK);

	const char* const aszBareArguments[] =
		{ "zenithmon.exe", "--zm-force-terrain-bake" };
	ZM_TerrainBakeSelection xForceAllSelection;
	ZENITH_ASSERT_TRUE(ZM_ParseTerrainBakeSelection(
		2, aszBareArguments, xForceAllSelection));
	ZENITH_ASSERT_EQ((u_int)xForceAllSelection.m_eMode,
		(u_int)ZM_TERRAIN_BAKE_SELECTION_FORCE_ALL);
	ZENITH_ASSERT_EQ(xForceAllSelection.m_uSelectedRecipeMask, 0u);

	// Selector argv order never becomes bake order: the parser maps each set to
	// its immutable registry bit, and callers walk those bits in registry order.
	const char* const aszReversedSelectedArguments[] =
	{
		"zenithmon.exe",
		"--zm-force-terrain-bake=Route1",
		"--zm-force-terrain-bake=Dawnmere",
	};
	ZM_TerrainBakeSelection xSelectedSelection;
	ZENITH_ASSERT_TRUE(ZM_ParseTerrainBakeSelection(
		3, aszReversedSelectedArguments, xSelectedSelection));
	ZENITH_ASSERT_EQ((u_int)xSelectedSelection.m_eMode,
		(u_int)ZM_TERRAIN_BAKE_SELECTION_FORCE_SELECTED);
	ZENITH_ASSERT_EQ(xSelectedSelection.m_uSelectedRecipeMask,
		(1u << 0u) | (1u << 2u));
	u_int uSelectedOrdinal = 0u;
	const u_int auExpectedSelectedRecipeIndices[] = { 0u, 2u };
	for (u_int i = 0; i < uZM_TERRAIN_RECIPE_COUNT; ++i)
	{
		if ((xSelectedSelection.m_uSelectedRecipeMask & (1u << i)) != 0u)
		{
			ZENITH_ASSERT_TRUE(uSelectedOrdinal < 2u);
			ZENITH_ASSERT_EQ(i, auExpectedSelectedRecipeIndices[uSelectedOrdinal]);
			++uSelectedOrdinal;
		}
	}
	ZENITH_ASSERT_EQ(uSelectedOrdinal, 2u);

	const char* const aszEmptyValue[] =
		{ "zenithmon.exe", "--zm-force-terrain-bake=" };
	AssertSelectionFailure(aszEmptyValue, 2,
		ZM_TERRAIN_BAKE_SELECTION_PARSE_MALFORMED, 1);
	const char* const aszUnknownSet[] =
		{ "zenithmon.exe", "--zm-force-terrain-bake=SunkenVale" };
	AssertSelectionFailure(aszUnknownSet, 2,
		ZM_TERRAIN_BAKE_SELECTION_PARSE_UNKNOWN_SET, 1);
	const char* const aszWrongCase[] =
		{ "zenithmon.exe", "--zm-force-terrain-bake=dawnmere" };
	AssertSelectionFailure(aszWrongCase, 2,
		ZM_TERRAIN_BAKE_SELECTION_PARSE_UNKNOWN_SET, 1);
	const char* const aszMalformedSuffix[] =
		{ "zenithmon.exe", "--zm-force-terrain-bake:Dawnmere" };
	AssertSelectionFailure(aszMalformedSuffix, 2,
		ZM_TERRAIN_BAKE_SELECTION_PARSE_MALFORMED, 1);
	const char* const aszDuplicateSet[] =
	{
		"zenithmon.exe",
		"--zm-force-terrain-bake=Dawnmere",
		"--zm-force-terrain-bake=Dawnmere",
	};
	AssertSelectionFailure(aszDuplicateSet, 3,
		ZM_TERRAIN_BAKE_SELECTION_PARSE_DUPLICATE, 2);
	const char* const aszRepeatedBare[] =
	{
		"zenithmon.exe",
		"--zm-force-terrain-bake",
		"--zm-force-terrain-bake",
	};
	AssertSelectionFailure(aszRepeatedBare, 3,
		ZM_TERRAIN_BAKE_SELECTION_PARSE_DUPLICATE, 2);
	const char* const aszBareThenSelected[] =
	{
		"zenithmon.exe",
		"--zm-force-terrain-bake",
		"--zm-force-terrain-bake=Route1",
	};
	AssertSelectionFailure(aszBareThenSelected, 3,
		ZM_TERRAIN_BAKE_SELECTION_PARSE_CONFLICT, 2);
	const char* const aszSelectedThenBare[] =
	{
		"zenithmon.exe",
		"--zm-force-terrain-bake=Route1",
		"--zm-force-terrain-bake",
	};
	AssertSelectionFailure(aszSelectedThenBare, 3,
		ZM_TERRAIN_BAKE_SELECTION_PARSE_CONFLICT, 2);
	const char* const aszFirstFailureWins[] =
	{
		"zenithmon.exe",
		"--zm-force-terrain-bake=Route1",
		"--zm-force-terrain-bake=Unknown",
		"--zm-force-terrain-bake=",
	};
	AssertSelectionFailure(aszFirstFailureWins, 4,
		ZM_TERRAIN_BAKE_SELECTION_PARSE_UNKNOWN_SET, 2);

	constexpr u_int uAllRecipeMask = (1u << uZM_TERRAIN_RECIPE_COUNT) - 1u;
	ZM_TerrainBakeBatchPlan xBatch = ZM_BuildTerrainBakeBatchPlan(
		xAutoSelection, false, 1u << 0u);
	ZENITH_ASSERT_EQ(xBatch.m_uWarmRecipeMask, 1u << 0u);
	ZENITH_ASSERT_EQ(xBatch.m_uQueueRecipeMask, (1u << 1u) | (1u << 2u));
	ZENITH_ASSERT_FALSE(xBatch.m_bAllWarm);
	ZENITH_ASSERT_FALSE(xBatch.m_bAuthorDawnmereScene);

	xBatch = ZM_BuildTerrainBakeBatchPlan(
		xAutoSelection, false, uAllRecipeMask | (1u << 12u));
	ZENITH_ASSERT_EQ(xBatch.m_uWarmRecipeMask, uAllRecipeMask,
		"warm masks must be limited to the fixed recipe registry");
	ZENITH_ASSERT_EQ(xBatch.m_uQueueRecipeMask, 0u);
	ZENITH_ASSERT_TRUE(xBatch.m_bAllWarm);
	ZENITH_ASSERT_TRUE(xBatch.m_bAuthorDawnmereScene);

	xBatch = ZM_BuildTerrainBakeBatchPlan(
		xForceAllSelection, false, uAllRecipeMask);
	ZENITH_ASSERT_EQ(xBatch.m_uQueueRecipeMask, uAllRecipeMask);
	ZENITH_ASSERT_TRUE(xBatch.m_bAllWarm);
	ZENITH_ASSERT_FALSE(xBatch.m_bAuthorDawnmereScene,
		"a forced bake must finish before warm scene authoring");

	xBatch = ZM_BuildTerrainBakeBatchPlan(
		xSelectedSelection, false, uAllRecipeMask);
	ZENITH_ASSERT_EQ(xBatch.m_uQueueRecipeMask, (1u << 0u) | (1u << 2u));
	ZENITH_ASSERT_TRUE(xBatch.m_bAllWarm);
	ZENITH_ASSERT_FALSE(xBatch.m_bAuthorDawnmereScene,
		"a targeted forced bake must finish before warm scene authoring");

	xBatch = ZM_BuildTerrainBakeBatchPlan(
		xAutoSelection, true, uAllRecipeMask);
	ZENITH_ASSERT_EQ(xBatch.m_uWarmRecipeMask, 0u);
	ZENITH_ASSERT_EQ(xBatch.m_uQueueRecipeMask, 0u);
	ZENITH_ASSERT_FALSE(xBatch.m_bAllWarm);
	ZENITH_ASSERT_FALSE(xBatch.m_bAuthorDawnmereScene,
		"headless selection must not scan warm state or queue authoring");

	ZM_TerrainBakeSelection xInvalidSelection;
	xInvalidSelection.m_eParseResult = ZM_TERRAIN_BAKE_SELECTION_PARSE_CONFLICT;
	xBatch = ZM_BuildTerrainBakeBatchPlan(
		xInvalidSelection, false, uAllRecipeMask);
	ZENITH_ASSERT_EQ(xBatch.m_uWarmRecipeMask, 0u);
	ZENITH_ASSERT_EQ(xBatch.m_uQueueRecipeMask, 0u);
	ZENITH_ASSERT_FALSE(xBatch.m_bAllWarm);
	ZENITH_ASSERT_FALSE(xBatch.m_bAuthorDawnmereScene);

	ZENITH_ASSERT_STREQ(ZM_TerrainBakeSelectionModeToString(
		ZM_TERRAIN_BAKE_SELECTION_AUTO_MISSING), "AUTO_MISSING");
	ZENITH_ASSERT_STREQ(ZM_TerrainBakeSelectionModeToString(
		ZM_TERRAIN_BAKE_SELECTION_FORCE_ALL), "FORCE_ALL");
	ZENITH_ASSERT_STREQ(ZM_TerrainBakeSelectionModeToString(
		ZM_TERRAIN_BAKE_SELECTION_FORCE_SELECTED), "FORCE_SELECTED");
	ZENITH_ASSERT_STREQ(ZM_TerrainBakeSelectionParseResultToString(
		ZM_TERRAIN_BAKE_SELECTION_PARSE_MALFORMED), "MALFORMED");
	ZENITH_ASSERT_STREQ(ZM_TerrainBakeSelectionParseResultToString(
		ZM_TERRAIN_BAKE_SELECTION_PARSE_UNKNOWN_SET), "UNKNOWN_SET");
	ZENITH_ASSERT_STREQ(ZM_TerrainBakeSelectionParseResultToString(
		ZM_TERRAIN_BAKE_SELECTION_PARSE_DUPLICATE), "DUPLICATE");
	ZENITH_ASSERT_STREQ(ZM_TerrainBakeSelectionParseResultToString(
		ZM_TERRAIN_BAKE_SELECTION_PARSE_CONFLICT), "CONFLICT");
}

ZENITH_TEST(ZM_TerrainRecipeSet, ManifestsEncodePerRecipeCountsAndInvalidateMissingOrEmptyOutputs)
{
#ifdef ZENITH_TOOLS
	const std::filesystem::path xTestRoot = RecipeManifestTestRoot();
	ZENITH_ASSERT_FALSE(xTestRoot.empty(),
		"could not resolve repository Build/artifacts root");
	if (xTestRoot.empty())
	{
		return;
	}
	RecipeManifestTestRootGuard xGuard(xTestRoot);

	for (u_int uRecipe = 0; uRecipe < uZM_TERRAIN_RECIPE_COUNT; ++uRecipe)
	{
		const ZM_TerrainAuthoringRecipe& xRecipe =
			ZM_GetTerrainAuthoringRecipe(uRecipe);
		const u_int uRequiredCount = ZM_GetTerrainRequiredOutputCount(xRecipe);
		Zenith_Vector<std::string> xOutputs;
		ZM_EnumerateRequiredTerrainOutputs(xRecipe, xOutputs);
		const std::filesystem::path xSetDirectory = xGuard.m_xRoot /
			"Terrain" / xRecipe.m_pxWorldSpec->m_szTerrainSet;
		std::error_code xError;
		std::filesystem::create_directories(xSetDirectory, xError);
		ZENITH_ASSERT_FALSE(static_cast<bool>(xError));

		for (u_int i = 0; i < xOutputs.GetSize(); ++i)
		{
			ZENITH_ASSERT_TRUE(WriteNonEmptyFile(xGuard.m_xRoot / xOutputs.Get(i)),
				"could not seed recipe %u output %u", uRecipe, i);
		}
		ZENITH_ASSERT_FALSE(ZM_IsTerrainBakeWarm(xRecipe, xGuard.m_xRoot),
			"outputs without a marker must remain cold");
		ZENITH_ASSERT_TRUE(ZM_FinalizeTerrainBake(xRecipe, xGuard.m_xRoot));
		ZENITH_ASSERT_TRUE(ZM_IsTerrainBakeWarm(xRecipe, xGuard.m_xRoot));

		const std::filesystem::path xMarker =
			xGuard.m_xRoot / ZM_GetTerrainManifestRelativePath(xRecipe);
		xError.clear();
		const uintmax_t ulMarkerSize = std::filesystem::file_size(xMarker, xError);
		ZENITH_ASSERT_FALSE(static_cast<bool>(xError));
		ZENITH_ASSERT_EQ(ulMarkerSize,
			static_cast<uintmax_t>(uZM_TERRAIN_MANIFEST_SIZE));
		std::ifstream xInput(xMarker, std::ios::binary);
		u_int8 auMarker[uZM_TERRAIN_MANIFEST_SIZE] = {};
		xInput.read(reinterpret_cast<char*>(auMarker), sizeof(auMarker));
		ZENITH_ASSERT_TRUE(static_cast<bool>(xInput));
		xInput.close();
		ZENITH_ASSERT_TRUE(memcmp(auMarker, "ZMTR", 4u) == 0);
		ZENITH_ASSERT_EQ(ReadU32LE(auMarker + 4u), uZM_TERRAIN_MANIFEST_VERSION);
		ZENITH_ASSERT_EQ(ReadU32LE(auMarker + 8u), uRequiredCount);

		const std::filesystem::path xMissingOutput =
			xGuard.m_xRoot / xOutputs.Get(xOutputs.GetSize() / 2u);
		xError.clear();
		std::filesystem::remove(xMissingOutput, xError);
		ZENITH_ASSERT_FALSE(static_cast<bool>(xError));
		ZENITH_ASSERT_FALSE(ZM_IsTerrainBakeWarm(xRecipe, xGuard.m_xRoot),
			"recipe %u ignored a missing required output", uRecipe);
		ZENITH_ASSERT_TRUE(WriteNonEmptyFile(xMissingOutput));
		ZENITH_ASSERT_TRUE(ZM_IsTerrainBakeWarm(xRecipe, xGuard.m_xRoot));

		const std::filesystem::path xEmptyOutput =
			xGuard.m_xRoot / xOutputs.GetBack();
		ZENITH_ASSERT_TRUE(WriteBytes(xEmptyOutput, nullptr, 0u));
		ZENITH_ASSERT_FALSE(ZM_IsTerrainBakeWarm(xRecipe, xGuard.m_xRoot),
			"recipe %u ignored an empty required output", uRecipe);
		ZENITH_ASSERT_TRUE(WriteNonEmptyFile(xEmptyOutput));
		ZENITH_ASSERT_TRUE(ZM_IsTerrainBakeWarm(xRecipe, xGuard.m_xRoot));

		ZENITH_ASSERT_TRUE(WriteTestManifest(xMarker, uRequiredCount - 1u));
		ZENITH_ASSERT_FALSE(ZM_IsTerrainBakeWarm(xRecipe, xGuard.m_xRoot),
			"recipe %u accepted the wrong marker count", uRecipe);
		ZENITH_ASSERT_TRUE(WriteTestManifest(xMarker, uRequiredCount));
		ZENITH_ASSERT_TRUE(ZM_IsTerrainBakeWarm(xRecipe, xGuard.m_xRoot));
	}
#endif
}

// ---------------------------------------------------------------------------
// S8 item 2 / R1-1 helpers. The anonymous namespace at the top of this file
// belongs to the units that were already here; these serve the ground-slot unit
// below.
// ---------------------------------------------------------------------------
namespace
{
	// The THREE shared ENGINE ground sets every outdoor recipe samples: grass on
	// the flats (slot 0), rock on the steeps (slot 1) and clay on the lanes and
	// pads (slot 2). Grass and rock are the same two sets RenderTest's terrain
	// uses, on the same two slots. Frozen here as the CLAIM; the nine recipe rows
	// are the subject.
	constexpr const char* szSHARED_GRASS_SET_DIR = "engine:Textures/Terrain/Grass/";
	constexpr const char* szSHARED_ROCK_SET_DIR = "engine:Textures/Terrain/Rock/";
	constexpr const char* szSHARED_CLAY_SET_DIR = "engine:Textures/Terrain/Clay/";

	// The renderer's terrain UV scale (Flux_Terrain.cpp's m_fUVScale): uv ~=
	// worldUnits * 0.07, so a slot tiled at t repeats every 1 / (0.07 * t) world
	// units. Spelled here because it is a RENDERER constant with no exported
	// symbol, and it is what turns an opaque tiling number into metres.
	constexpr float fTERRAIN_SHADER_UV_SCALE = 0.07f;

	// ★ TWO WINDOWS, NOT ONE, AND THE DIFFERENCE IS A PROPERTY OF THE IMAGES.
	// Grass and rock are non-repeating photographic ground: one tile should read as
	// roughly a 16 m patch, and at that size the repeat hides. Clay is a REGULAR
	// 6x6 GRID OF PAVING SLABS, so its repeat is an architectural dimension the eye
	// measures directly -- a 16 m repeat would mean 2.6 m slabs and the lanes would
	// read as a chessboard. Its ~4 m repeat gives a 0.66 m slab. Both windows are
	// deliberately wide: these are order-of-magnitude guards on the DERIVED tile
	// size, not second spellings of the tiling numbers.
	constexpr float fMIN_GROUND_TILE_METRES = 14.0f;
	constexpr float fMAX_GROUND_TILE_METRES = 18.0f;
	constexpr float fMIN_PAVING_TILE_METRES = 3.5f;
	constexpr float fMAX_PAVING_TILE_METRES = 4.5f;

	// Does this row sample the NAMED shared ENGINE set at a usable tiling? The set
	// directory is a PARAMETER rather than baked in, so one predicate answers for
	// all three sets alike -- and, crucially, can still say NO, which is what the
	// anti-vacuity arm below leans on.
	bool SamplesSharedEngineSet(const ZM_TerrainMaterialSpec& xMaterial,
		const char* szSetDir)
	{
		return xMaterial.m_szTextureSetDir != nullptr &&
			strcmp(xMaterial.m_szTextureSetDir, szSetDir) == 0 &&
			xMaterial.m_fUVTiling > 0.0f;
	}

	// Resolve a PREFIXED terrain texture-set directory the way the asset registry
	// does. An unprefixed or absent ref resolves to nothing, which is itself the
	// answer the caller wants.
	std::filesystem::path ResolveTextureSetDirectory(const char* szSetDir)
	{
		if (szSetDir == nullptr)
		{
			return {};
		}
		const std::string strSetDir = szSetDir;
		if (strSetDir.rfind("engine:", 0u) == 0u)
		{
			return std::filesystem::path(ENGINE_ASSETS_DIR) /
				strSetDir.substr(strlen("engine:"));
		}
		if (strSetDir.rfind("game:", 0u) == 0u)
		{
			return std::filesystem::path(GAME_ASSETS_DIR) /
				strSetDir.substr(strlen("game:"));
		}
		return {};
	}

	u_int CountTexturedMaterialSlots(const ZM_TerrainAuthoringRecipe& xRecipe,
		u_int& uFirstTexturedSlotOut)
	{
		u_int uCount = 0u;
		uFirstTexturedSlotOut = UINT_MAX;
		for (u_int uSlot = 0; uSlot < xRecipe.m_uMaterialCount; ++uSlot)
		{
			if (xRecipe.m_pxMaterials[uSlot].m_szTextureSetDir != nullptr)
			{
				if (uCount == 0u)
				{
					uFirstTexturedSlotOut = uSlot;
				}
				++uCount;
			}
		}
		return uCount;
	}
}

// All three outdoor recipes carry THREE textured slots -- the shared ENGINE
// grass set on the flats (slot 0), the shared rock set on the steeps (slot 1)
// and the shared clay set on the lanes and pads (slot 2). Grass and rock are the
// two sets RenderTest's terrain samples on the same two slots. Every one of
// those nine rows was flat colour at some point, and a flat slot reads as
// painted cardboard beside a photographic one.
//
// This is a CROSS-RECIPE claim on purpose. Spelling the set directories nine
// times would only restate the rows; what is pinned here is that all three
// outdoor regions resolve to the SAME three sets at the SAME per-set scale, that
// each recipe textures exactly THREE slots and that they are slots 0, 1 and 2,
// and that each tiling number still turns into a sensible tile once the shader's
// UV scale is applied. So a revert of any row to nullptr, a re-pointed set, a
// set hung on the wrong slot, a re-tint, and a lockstep re-tiling of all three
// all red here -- and so does moving DAWNMERE's rows out from under the other
// two.
ZENITH_TEST(ZM_TerrainRecipeSet, OutdoorGroundSlotsSampleTheSharedEngineGroundSets)
{
	const ZM_TerrainAuthoringRecipe& xDawnmere = ZM_GetDawnmereTerrainRecipe();
	const ZM_TerrainAuthoringRecipe& xThornacre = ZM_GetThornacreTerrainRecipe();
	const ZM_TerrainAuthoringRecipe& xRoute1 = ZM_GetRoute1TerrainRecipe();

	ZENITH_ASSERT_EQ(xDawnmere.m_uMaterialCount, 4u);
	ZENITH_ASSERT_EQ(xThornacre.m_uMaterialCount, 4u);
	ZENITH_ASSERT_EQ(xRoute1.m_uMaterialCount, 4u);

	const ZM_TerrainMaterialSpec& xMeadow = xDawnmere.m_pxMaterials[0];
	const ZM_TerrainMaterialSpec& xPasture = xThornacre.m_pxMaterials[0];
	const ZM_TerrainMaterialSpec& xCoastalMeadow = xRoute1.m_pxMaterials[0];

	const ZM_TerrainMaterialSpec& xStone = xDawnmere.m_pxMaterials[1];
	const ZM_TerrainMaterialSpec& xDrystone = xThornacre.m_pxMaterials[1];
	const ZM_TerrainMaterialSpec& xChalk = xRoute1.m_pxMaterials[1];

	const ZM_TerrainMaterialSpec& xDawnmereDirt = xDawnmere.m_pxMaterials[2];
	const ZM_TerrainMaterialSpec& xThornacreDirt = xThornacre.m_pxMaterials[2];
	const ZM_TerrainMaterialSpec& xRoute1Dirt = xRoute1.m_pxMaterials[2];

	// The anchor: these are the rows this unit is talking about. A renamed slot
	// would also break its auto-splat rule pairing, which is a different unit's
	// business -- here it just proves we are reading the intended row.
	ZENITH_ASSERT_STREQ(xMeadow.m_szName, "Meadow");
	ZENITH_ASSERT_STREQ(xPasture.m_szName, "Pasture");
	ZENITH_ASSERT_STREQ(xCoastalMeadow.m_szName, "CoastalMeadow");
	ZENITH_ASSERT_STREQ(xStone.m_szName, "Stone");
	ZENITH_ASSERT_STREQ(xDrystone.m_szName, "Drystone");
	ZENITH_ASSERT_STREQ(xChalk.m_szName, "Chalk");
	ZENITH_ASSERT_STREQ(xDawnmereDirt.m_szName, "Dirt");
	ZENITH_ASSERT_STREQ(xThornacreDirt.m_szName, "Dirt");
	ZENITH_ASSERT_STREQ(xRoute1Dirt.m_szName, "Dirt");

	// One walk over all NINE textured rows. The expected set AND the expected tile
	// window travel with the row, so the grass, rock and clay thirds are policed
	// by the same clauses rather than by three divergent copies of them.
	struct GroundSlotUnderTest
	{
		const ZM_TerrainMaterialSpec* m_pxSlot;
		const char* m_szExpectedSetDir;
		float m_fMinTileMetres;
		float m_fMaxTileMetres;
		const char* m_szOwner;
	};
	const GroundSlotUnderTest axGroundSlots[] =
	{
		{ &xMeadow, szSHARED_GRASS_SET_DIR, fMIN_GROUND_TILE_METRES, fMAX_GROUND_TILE_METRES, "Dawnmere flats" },
		{ &xPasture, szSHARED_GRASS_SET_DIR, fMIN_GROUND_TILE_METRES, fMAX_GROUND_TILE_METRES, "Thornacre flats" },
		{ &xCoastalMeadow, szSHARED_GRASS_SET_DIR, fMIN_GROUND_TILE_METRES, fMAX_GROUND_TILE_METRES, "Route1 flats" },
		{ &xStone, szSHARED_ROCK_SET_DIR, fMIN_GROUND_TILE_METRES, fMAX_GROUND_TILE_METRES, "Dawnmere steeps" },
		{ &xDrystone, szSHARED_ROCK_SET_DIR, fMIN_GROUND_TILE_METRES, fMAX_GROUND_TILE_METRES, "Thornacre steeps" },
		{ &xChalk, szSHARED_ROCK_SET_DIR, fMIN_GROUND_TILE_METRES, fMAX_GROUND_TILE_METRES, "Route1 steeps" },
		{ &xDawnmereDirt, szSHARED_CLAY_SET_DIR, fMIN_PAVING_TILE_METRES, fMAX_PAVING_TILE_METRES, "Dawnmere paving" },
		{ &xThornacreDirt, szSHARED_CLAY_SET_DIR, fMIN_PAVING_TILE_METRES, fMAX_PAVING_TILE_METRES, "Thornacre paving" },
		{ &xRoute1Dirt, szSHARED_CLAY_SET_DIR, fMIN_PAVING_TILE_METRES, fMAX_PAVING_TILE_METRES, "Route1 paving" },
	};
	constexpr u_int uGROUND_SLOT_COUNT = 9u;
	static_assert(sizeof(axGroundSlots) / sizeof(axGroundSlots[0]) ==
		uGROUND_SLOT_COUNT);

	for (u_int i = 0; i < uGROUND_SLOT_COUNT; ++i)
	{
		const ZM_TerrainMaterialSpec& xGround = *axGroundSlots[i].m_pxSlot;
		const bool bSamplesSharedSet =
			SamplesSharedEngineSet(xGround, axGroundSlots[i].m_szExpectedSetDir);
		ZENITH_ASSERT_TRUE(bSamplesSharedSet,
			"%s slot '%s' no longer samples %s -- a flat-colour revert or a "
			"re-pointed set puts the painted-cardboard ground straight back",
			axGroundSlots[i].m_szOwner, xGround.m_szName,
			axGroundSlots[i].m_szExpectedSetDir);
		if (!bSamplesSharedSet)
		{
			continue;
		}

		// A textured slot's base colour MULTIPLIES the sampled diffuse
		// (Flux_Terrain_ToGBuffer -> SampleDiffuseWithBaseColor), so the row must
		// author WHITE and let the maps carry the hue. Any tint is the exact
		// regression that made this check necessary for Dawnmere.
		for (u_int uChannel = 0; uChannel < 4u; ++uChannel)
		{
			ZENITH_ASSERT_EQ_FLOAT(xGround.m_afBaseColour[uChannel], 1.0f, fEPSILON,
				"%s slot '%s' tints channel %u -- a textured slot authors white",
				axGroundSlots[i].m_szOwner, xGround.m_szName, uChannel);
		}

		// The tiling number only means something once the shader's UV scale is
		// applied. Deriving metres catches an order-of-magnitude slip that the
		// cross-recipe equality below cannot -- because that one still passes if
		// somebody re-tiles all nine rows together.
		const float fTileMetres =
			1.0f / (fTERRAIN_SHADER_UV_SCALE * xGround.m_fUVTiling);
		ZENITH_ASSERT_GE(fTileMetres, axGroundSlots[i].m_fMinTileMetres,
			"%s slot tiles every %.2f m -- finer than the %.1f m floor this slot's "
			"imagery is authored for", axGroundSlots[i].m_szOwner, fTileMetres,
			axGroundSlots[i].m_fMinTileMetres);
		ZENITH_ASSERT_LE(fTileMetres, axGroundSlots[i].m_fMaxTileMetres,
			"%s slot tiles every %.2f m -- coarser than the %.1f m ceiling this "
			"slot's imagery is authored for", axGroundSlots[i].m_szOwner,
			fTileMetres, axGroundSlots[i].m_fMaxTileMetres);
	}

	// ★ THE PAVING WINDOW IS A SEPARATE CLAIM FROM THE GROUND WINDOW, AND IT MUST
	// STAY ONE. Clay is a regular 6x6 grid of slabs, so its repeat is a size the
	// player reads directly; the other two sets are non-repeating photographs
	// whose repeat is meant to hide. Proving the two windows do not overlap is
	// what stops a later "tidy them to one number" from passing this unit while
	// turning every lane into 2.6 m flagstones.
	static_assert(fMAX_PAVING_TILE_METRES < fMIN_GROUND_TILE_METRES,
		"the paving and ground tile windows overlap, so a clay row tiled like "
		"grass would satisfy both");

	// ★ ANTI-VACUITY, through the IDENTICAL predicate. The flat-colour control has
	// walked out along the slots as each one gained a texture -- it was slot 1,
	// then slot 2, and slot 3 (Heath / Hedgerow / Wildflower) is the last one
	// left. A predicate that accepted everything would pass every clause above
	// while proving nothing -- and the control must also say no to the WRONG set,
	// or hanging the grass maps on the paving would read as green.
	const ZM_TerrainAuthoringRecipe* apxOutdoorRecipes[] =
		{ &xDawnmere, &xThornacre, &xRoute1 };
	const char* aszRecipeOwners[] = { "Dawnmere", "Thornacre", "Route1" };
	for (u_int i = 0; i < uZM_TERRAIN_RECIPE_COUNT; ++i)
	{
		const ZM_TerrainMaterialSpec& xFlat = apxOutdoorRecipes[i]->m_pxMaterials[3];
		ZENITH_ASSERT_FALSE(
			SamplesSharedEngineSet(xFlat, szSHARED_GRASS_SET_DIR) ||
			SamplesSharedEngineSet(xFlat, szSHARED_ROCK_SET_DIR) ||
			SamplesSharedEngineSet(xFlat, szSHARED_CLAY_SET_DIR),
			"%s's slot 3 reads as textured, so the shared-set predicate cannot "
			"distinguish a textured slot from a flat one", aszRecipeOwners[i]);
	}
	ZENITH_ASSERT_FALSE(SamplesSharedEngineSet(xStone, szSHARED_GRASS_SET_DIR),
		"the shared-set predicate cannot tell the rock set from the grass one, so "
		"the nine rows above prove nothing about WHICH maps they sample");
	ZENITH_ASSERT_FALSE(SamplesSharedEngineSet(xDawnmereDirt, szSHARED_ROCK_SET_DIR),
		"the shared-set predicate cannot tell the clay set from the rock one, so "
		"the nine rows above prove nothing about WHICH maps they sample");

	// Three sets, one scale PER SET, across all three outdoor regions. Dawnmere is
	// the already-shipped reference the other two rows are tied to, so re-pointing
	// or re-tiling ANY single recipe breaks the tie -- including Dawnmere's own
	// rows. Note the clay rows are tied to each OTHER, not to the grass tiling: a
	// paving slab is the same size in all three towns, which is a different claim
	// from "the ground reads at one scale".
	ZENITH_ASSERT_STREQ(xPasture.m_szTextureSetDir, xMeadow.m_szTextureSetDir,
		"Thornacre's flats drifted off the set Dawnmere samples");
	ZENITH_ASSERT_STREQ(xCoastalMeadow.m_szTextureSetDir, xMeadow.m_szTextureSetDir,
		"Route1's flats drifted off the set Dawnmere samples");
	ZENITH_ASSERT_STREQ(xDrystone.m_szTextureSetDir, xStone.m_szTextureSetDir,
		"Thornacre's steeps drifted off the set Dawnmere samples");
	ZENITH_ASSERT_STREQ(xChalk.m_szTextureSetDir, xStone.m_szTextureSetDir,
		"Route1's steeps drifted off the set Dawnmere samples");
	ZENITH_ASSERT_STREQ(xThornacreDirt.m_szTextureSetDir, xDawnmereDirt.m_szTextureSetDir,
		"Thornacre's paving drifted off the set Dawnmere samples");
	ZENITH_ASSERT_STREQ(xRoute1Dirt.m_szTextureSetDir, xDawnmereDirt.m_szTextureSetDir,
		"Route1's paving drifted off the set Dawnmere samples");

	ZENITH_ASSERT_EQ_FLOAT(xPasture.m_fUVTiling, xMeadow.m_fUVTiling, fEPSILON,
		"Thornacre's flats read at a different physical scale to Dawnmere's");
	ZENITH_ASSERT_EQ_FLOAT(xCoastalMeadow.m_fUVTiling, xMeadow.m_fUVTiling, fEPSILON,
		"Route1's flats read at a different physical scale to Dawnmere's");
	ZENITH_ASSERT_EQ_FLOAT(xStone.m_fUVTiling, xMeadow.m_fUVTiling, fEPSILON,
		"Dawnmere's steeps read at a different physical scale to its flats");
	ZENITH_ASSERT_EQ_FLOAT(xDrystone.m_fUVTiling, xStone.m_fUVTiling, fEPSILON,
		"Thornacre's steeps read at a different physical scale to Dawnmere's");
	ZENITH_ASSERT_EQ_FLOAT(xChalk.m_fUVTiling, xStone.m_fUVTiling, fEPSILON,
		"Route1's steeps read at a different physical scale to Dawnmere's");
	ZENITH_ASSERT_EQ_FLOAT(xThornacreDirt.m_fUVTiling, xDawnmereDirt.m_fUVTiling, fEPSILON,
		"a Thornacre paving slab is a different size to a Dawnmere one");
	ZENITH_ASSERT_EQ_FLOAT(xRoute1Dirt.m_fUVTiling, xDawnmereDirt.m_fUVTiling, fEPSILON,
		"a Route1 paving slab is a different size to a Dawnmere one");

	// Exactly THREE textured slots per outdoor recipe, and they are slots 0, 1 and
	// 2. This is what catches a set hung on the WRONG slot: texturing Heath leaves
	// every clause above green while a lane stays flat. The running total is the
	// loop's anti-vacuity arm -- if the registry ever went empty, or every recipe
	// lost its textures, the total stops matching 3x the recipe count.
	u_int uTexturedSlotTotal = 0u;
	for (u_int uRecipe = 0; uRecipe < uZM_TERRAIN_RECIPE_COUNT; ++uRecipe)
	{
		const ZM_TerrainAuthoringRecipe& xRecipe =
			ZM_GetTerrainAuthoringRecipe(uRecipe);
		u_int uFirstTexturedSlot = UINT_MAX;
		const u_int uTexturedSlots =
			CountTexturedMaterialSlots(xRecipe, uFirstTexturedSlot);
		ZENITH_ASSERT_EQ(uTexturedSlots, 3u,
			"recipe %u ('%s') carries %u textured slots -- outdoor recipes texture "
			"their ground slot 0, their steeps slot 1 and their lane slot 2, and "
			"nothing else", uRecipe, xRecipe.m_pxWorldSpec->m_szTerrainSet,
			uTexturedSlots);
		ZENITH_ASSERT_EQ(uFirstTexturedSlot, 0u,
			"recipe %u ('%s') textured slot %u rather than its ground slot 0",
			uRecipe, xRecipe.m_pxWorldSpec->m_szTerrainSet, uFirstTexturedSlot);
		ZENITH_ASSERT_TRUE(
			SamplesSharedEngineSet(xRecipe.m_pxMaterials[1], szSHARED_ROCK_SET_DIR),
			"recipe %u ('%s') textured three slots, but slot 1 is not the shared "
			"rock set", uRecipe, xRecipe.m_pxWorldSpec->m_szTerrainSet);
		ZENITH_ASSERT_TRUE(
			SamplesSharedEngineSet(xRecipe.m_pxMaterials[2], szSHARED_CLAY_SET_DIR),
			"recipe %u ('%s') textured three slots, but slot 2 is not the shared "
			"clay set", uRecipe, xRecipe.m_pxWorldSpec->m_szTerrainSet);
		uTexturedSlotTotal += uTexturedSlots;
	}
	ZENITH_ASSERT_EQ(uTexturedSlotTotal, uZM_TERRAIN_RECIPE_COUNT * 3u,
		"the textured-slot walk found nothing to police");

	// ★ ACCEPTED I/O EXCEPTION -- a boot unit is otherwise pure, but the only way
	// to prove the "engine:" refs RESOLVE is to look, and this file's Dawnmere
	// unit already sets that precedent. The paths are DERIVED from the rows under
	// test rather than hard-coded, so they follow a future move of any set. The
	// maps are gitignored workspace assets, so an absent directory is a cold
	// clone and is skipped; a directory that IS present must be complete, or the
	// ground silently falls back to default textures.
	//
	// ★★ `rm_packed` IS THE ONE THAT GOES MISSING. Stage 1 of the asset pipeline
	// (`ExportAllTextures`) turns every jpg under the engine tree into a .ztxtr on
	// any tools boot, but the packed roughness+metallic map the terrain shader
	// samples (`xRM.gb`) is written only by RenderTest's
	// `RenderTest_PackTerrainRoughnessMetallic`, from a hand-maintained list of
	// set directories. A set added there and forgotten here has three of its four
	// maps and falls back silently on the fourth.
	const ZM_TerrainMaterialSpec* apxSetOwners[] =
		{ &xCoastalMeadow, &xChalk, &xRoute1Dirt };
	for (u_int i = 0; i < 3u; ++i)
	{
		const std::filesystem::path xSetDir =
			ResolveTextureSetDirectory(apxSetOwners[i]->m_szTextureSetDir);
		ZENITH_ASSERT_FALSE(xSetDir.empty(),
			"the '%s' ground set ref carries no engine:/game: prefix, so the asset "
			"registry cannot resolve it at all", apxSetOwners[i]->m_szName);
		if (xSetDir.empty() || !std::filesystem::exists(xSetDir))
		{
			continue;
		}
		const char* aszSharedGroundMaps[] = { "diffuse", "normal", "rm_packed", "ao" };
		for (const char* szMap : aszSharedGroundMaps)
		{
			const std::filesystem::path xMap =
				xSetDir / (std::string(szMap) + ZENITH_TEXTURE_EXT);
			ZENITH_ASSERT_TRUE(std::filesystem::exists(xMap),
				"the shared ground set slot '%s' samples is missing %s",
				apxSetOwners[i]->m_szName, xMap.generic_string().c_str());
		}
	}
}
