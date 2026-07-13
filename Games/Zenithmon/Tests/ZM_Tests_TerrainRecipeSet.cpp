#include "Zenith.h"

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

	const ZM_ExpectedTerrainRecipe s_axExpectedRecipes[] =
	{
		{ ZM_SCENE_DAWNMERE, ZM_SCENE_KIND_TOWN, 2u, "Dawnmere",
			0x7BF32CA4u, 16, 16, 771u, 772u },
		{ ZM_SCENE_THORNACRE, ZM_SCENE_KIND_TOWN, 3u, "Thornacre",
			0x9D41BD83u, 16, 16, 771u, 772u },
		{ ZM_SCENE_ROUTE1, ZM_SCENE_KIND_ROUTE, 20u, "Route1",
			0x552E711Du, 16, 24, 1155u, 1156u },
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
				xMaterialA.m_fMetallic != xMaterialB.m_fMetallic)
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
			if (std::filesystem::exists(xCursor / "AGENTS.md") &&
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
	ZENITH_ASSERT_EQ(uZM_DAWNMERE_REQUIRED_OUTPUT_COUNT, 771u);
	ZENITH_ASSERT_EQ(uZM_THORNACRE_REQUIRED_OUTPUT_COUNT, 771u);
	ZENITH_ASSERT_EQ(uZM_ROUTE1_REQUIRED_OUTPUT_COUNT, 1155u);

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

		ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_fWorldMinX, 0.0f, fEPSILON);
		ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_fWorldMinZ, 0.0f, fEPSILON);
		ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_fWorldMaxX,
			static_cast<float>(xExpected.m_iChunkWidth) * fTERRAIN_CHUNK_SIZE,
			fEPSILON);
		ZENITH_ASSERT_EQ_FLOAT(xRecipe.m_fWorldMaxZ,
			static_cast<float>(xExpected.m_iChunkHeight) * fTERRAIN_CHUNK_SIZE,
			fEPSILON);
		ZENITH_ASSERT_EQ(xRecipe.m_xExportRect.m_iMinX, 0);
		ZENITH_ASSERT_EQ(xRecipe.m_xExportRect.m_iMinY, 0);
		ZENITH_ASSERT_EQ(xRecipe.m_xExportRect.m_iMaxX,
			xExpected.m_iChunkWidth - 1);
		ZENITH_ASSERT_EQ(xRecipe.m_xExportRect.m_iMaxY,
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
		ZENITH_ASSERT_GE(xRecipe.m_xErosion.m_xCentre.m_fX, xRecipe.m_fWorldMinX);
		ZENITH_ASSERT_LE(xRecipe.m_xErosion.m_xCentre.m_fX, xRecipe.m_fWorldMaxX);
		ZENITH_ASSERT_GE(xRecipe.m_xErosion.m_xCentre.m_fZ, xRecipe.m_fWorldMinZ);
		ZENITH_ASSERT_LE(xRecipe.m_xErosion.m_xCentre.m_fZ, xRecipe.m_fWorldMaxZ);

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
			xRecipe.m_fWorldMinX);
		ZENITH_ASSERT_LE(xRecipe.m_xPreviewCamera.m_xPosition.m_fX,
			xRecipe.m_fWorldMaxX);
		ZENITH_ASSERT_GE(xRecipe.m_xPreviewCamera.m_xPosition.m_fZ,
			xRecipe.m_fWorldMinZ);
		ZENITH_ASSERT_LE(xRecipe.m_xPreviewCamera.m_xPosition.m_fZ,
			xRecipe.m_fWorldMaxZ);
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
			ZENITH_ASSERT_GE(xPoint.m_fX, xRecipe.m_fWorldMinX);
			ZENITH_ASSERT_LE(xPoint.m_fX, xRecipe.m_fWorldMaxX);
			ZENITH_ASSERT_GE(xPoint.m_fZ, xRecipe.m_fWorldMinZ);
			ZENITH_ASSERT_LE(xPoint.m_fZ, xRecipe.m_fWorldMaxZ);
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
		ZENITH_ASSERT_EQ((u_int)xPlanA.Get(0u).m_eType,
			(u_int)ZM_TERRAIN_PLAN_RESET);
		ZENITH_ASSERT_EQ((u_int)xPlanA.Get(1u).m_eType,
			(u_int)ZM_TERRAIN_PLAN_SET_ASSET_SET);
		ZENITH_ASSERT_EQ((u_int)xPlanA.Get(2u).m_eType,
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
				xRecipe.m_fWorldMinX,
				"recipe %u dab %u escaped minimum X", uRecipe, i);
			ZENITH_ASSERT_LE(xOp.m_fWorldX + xOp.m_fRadius,
				xRecipe.m_fWorldMaxX,
				"recipe %u dab %u escaped maximum X", uRecipe, i);
			ZENITH_ASSERT_GE(xOp.m_fWorldZ - xOp.m_fRadius,
				xRecipe.m_fWorldMinZ,
				"recipe %u dab %u escaped minimum Z", uRecipe, i);
			ZENITH_ASSERT_LE(xOp.m_fWorldZ + xOp.m_fRadius,
				xRecipe.m_fWorldMaxZ,
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
		ZENITH_ASSERT_EQ(uTextureCount, 3u);

		const char* aszMeshPrefixes[] = { "Render", "Render_LOW", "Physics" };
		u_int uExpectedOutputIndex = 0u;
		for (u_int uPrefix = 0; uPrefix < 3u; ++uPrefix)
		{
			for (int iY = xRecipe.m_xExportRect.m_iMinY;
				iY <= xRecipe.m_xExportRect.m_iMaxY; ++iY)
			{
				for (int iX = xRecipe.m_xExportRect.m_iMinX;
					iX <= xRecipe.m_xExportRect.m_iMaxX; ++iX)
				{
					const std::string strExpected = strPrefix + aszMeshPrefixes[uPrefix] +
						"_" + std::to_string(iX) + "_" + std::to_string(iY) + ".zmesh";
					ZENITH_ASSERT_STREQ(xOutputsA.Get(uExpectedOutputIndex).c_str(),
						strExpected.c_str(),
						"recipe %u output order drifted at %u",
						uRecipe, uExpectedOutputIndex);
					++uExpectedOutputIndex;
				}
			}
		}
		ZENITH_ASSERT_EQ(uExpectedOutputIndex, uArea * 3u);
		ZENITH_ASSERT_STREQ(xOutputsA.Get(xOutputsA.GetSize() - 3u).c_str(),
			(strPrefix + "Height.ztxtr").c_str());
		ZENITH_ASSERT_STREQ(xOutputsA.Get(xOutputsA.GetSize() - 2u).c_str(),
			(strPrefix + "Splatmap_RGBA.ztxtr").c_str());
		ZENITH_ASSERT_STREQ(xOutputsA.GetBack().c_str(),
			(strPrefix + "GrassDensity.ztxtr").c_str());
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
		{ "zenithmon.exe", "--headless", "--skip-unit-tests" };
	ZM_TerrainBakeSelection xAutoSelection;
	ZENITH_ASSERT_TRUE(ZM_ParseTerrainBakeSelection(
		3, aszAutoArguments, xAutoSelection));
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
		"--headless",
		"--zm-force-terrain-bake=Route1",
		"--zm-force-terrain-bake=Unknown",
		"--zm-force-terrain-bake=",
	};
	AssertSelectionFailure(aszFirstFailureWins, 5,
		ZM_TERRAIN_BAKE_SELECTION_PARSE_UNKNOWN_SET, 3);

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
