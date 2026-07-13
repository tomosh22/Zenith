#include "Zenith.h"
#include "Zenithmon/Source/World/ZM_TerrainAuthoring.h"

#include <cmath>
#include <fstream>

#ifdef ZENITH_TOOLS
#include "Core/Zenith_Engine.h"
#include "Editor/TerrainEditor/Zenith_TerrainEditor.h"
#include "Editor/Zenith_EditorAutomation.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#endif

namespace
{
	constexpr u_int uDIRT_PATH_PASSES = 5u;
	constexpr u_int uMANIFEST_MAGIC_SIZE = 4u;
	constexpr char acMANIFEST_MAGIC[uMANIFEST_MAGIC_SIZE] = { 'Z', 'M', 'T', 'R' };
	constexpr const char* szMANIFEST_NAME = "ZM_TerrainRecipe.manifest";

	const ZM_TerrainLandformSpec s_axDawnmereLandforms[] =
	{
		{ { 224.0f, 650.0f }, 180.0f, 0.55f, 42.0f },
		{ { 810.0f, 690.0f }, 190.0f, 0.55f, 46.0f },
		{ { 180.0f, 260.0f }, 160.0f, 0.45f, 34.0f },
		// One metre below the original 175 m draft keeps the east edge inside
		// Dawnmere's inclusive X=1024 authoring boundary (850 + 174 = 1024).
		{ { 850.0f, 300.0f }, 174.0f, 0.45f, 38.0f },
	};

	const ZM_TerrainPoint2 s_axDawnmereRoutePath[] =
	{
		{ 512.0f, 928.0f },
		{ 500.0f, 760.0f },
		{ 524.0f, 620.0f },
		{ 512.0f, 512.0f },
	};

	const ZM_TerrainPoint2 s_axDawnmereHomePath[] =
	{
		{ 512.0f, 512.0f },
		{ 454.0f, 486.0f },
		{ 384.0f, 456.0f },
	};

	const ZM_TerrainPoint2 s_axDawnmereLabPath[] =
	{
		{ 512.0f, 512.0f },
		{ 574.0f, 526.0f },
		{ 640.0f, 552.0f },
	};

	const ZM_TerrainPathSpec s_axDawnmerePaths[] =
	{
		{ "Route", s_axDawnmereRoutePath, 4u, 18.0f, 9.0f, 49u, 10.0f, 6.0f, 73u },
		{ "Home", s_axDawnmereHomePath, 3u, 13.0f, 7.0f, 22u, 7.0f, 5.0f, 30u },
		{ "Lab", s_axDawnmereLabPath, 3u, 13.0f, 7.0f, 22u, 7.0f, 5.0f, 29u },
	};

	const ZM_TerrainPadSpec s_axDawnmerePads[] =
	{
		{ "Plaza", { 512.0f, 512.0f }, 60.0f, 45.0f, 4u },
		{ "Home", { 384.0f, 456.0f }, 36.0f, 26.0f, 4u },
		{ "Lab", { 640.0f, 552.0f }, 48.0f, 38.0f, 4u },
		{ "RouteGate", { 512.0f, 896.0f }, 30.0f, 0.0f, 0u },
	};

	const ZM_TerrainAutoSplatSpec s_axDawnmereAutoSplat[] =
	{
		{ "Meadow", 10.0f, 50.0f, 0.0f, 22.0f, 1.25f, 0.12f },
		{ "Stone", 0.0f, 512.0f, 18.0f, 90.0f, 1.10f, 0.08f },
		{ "Dirt", 0.0f, 36.0f, 0.0f, 35.0f, 0.45f, 0.18f },
		{ "Heath", 28.0f, 90.0f, 0.0f, 25.0f, 0.55f, 0.25f },
	};

	const ZM_TerrainGrassDabSpec s_axDawnmereGrassDabs[] =
	{
		{ { 315.0f, 650.0f }, 110.0f, 0.55f },
		{ { 325.0f, 800.0f }, 95.0f, 0.70f },
		{ { 705.0f, 660.0f }, 105.0f, 0.55f },
		{ { 700.0f, 810.0f }, 90.0f, 0.70f },
		{ { 260.0f, 430.0f }, 80.0f, 0.45f },
		{ { 780.0f, 500.0f }, 80.0f, 0.45f },
	};

	const ZM_TerrainLandmarkSpec s_axDawnmereLandmarks[] =
	{
		{ "TownCenter", { 512.0f, 24.0f, 480.0f } },
		{ "Plaza", { 512.0f, 24.0f, 512.0f } },
		{ "Home", { 384.0f, 24.0f, 456.0f } },
		{ "FromHome", { 384.0f, 24.0f, 482.0f } },
		{ "Lab", { 640.0f, 24.0f, 552.0f } },
		{ "FromLab", { 640.0f, 24.0f, 520.0f } },
		{ "FromRoute1", { 512.0f, 24.0f, 864.0f } },
		{ "RouteBoundary", { 512.0f, 24.0f, 928.0f } },
		{ "ReservedRivalHome", { 360.0f, 24.0f, 560.0f } },
		{ "PlazaLandmark", { 462.0f, 24.0f, 548.0f } },
	};

	const ZM_TerrainMaterialSpec s_axDawnmereMaterials[] =
	{
		{ "Meadow", { 0.26f, 0.46f, 0.16f, 1.0f }, 0.92f, 0.0f },
		{ "Stone", { 0.34f, 0.36f, 0.33f, 1.0f }, 0.88f, 0.0f },
		{ "Dirt", { 0.38f, 0.25f, 0.14f, 1.0f }, 0.96f, 0.0f },
		{ "Heath", { 0.48f, 0.55f, 0.20f, 1.0f }, 0.90f, 0.0f },
	};

	template<typename T, size_t N>
	constexpr u_int CountOf(const T (&)[N])
	{
		return static_cast<u_int>(N);
	}

	bool IsSafeTerrainSetName(const char* szSet)
	{
		if (szSet == nullptr || szSet[0] == '\0')
		{
			return false;
		}
		for (u_int i = 0; szSet[i] != '\0'; ++i)
		{
			const char c = szSet[i];
			const bool bAlphaNumeric = (c >= 'a' && c <= 'z') ||
				(c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
			if ((!bAlphaNumeric && c != '_' && c != '-') || i >= 64u)
			{
				return false;
			}
		}
		return true;
	}

	std::filesystem::path RelativeTerrainDirectory(const ZM_TerrainAuthoringRecipe& xRecipe)
	{
		Zenith_Assert(xRecipe.m_pxWorldSpec != nullptr, "Terrain recipe requires a WorldSpec row");
		Zenith_Assert(IsSafeTerrainSetName(xRecipe.m_pxWorldSpec->m_szTerrainSet),
			"Terrain recipe has unsafe WorldSpec terrain set");
		return std::filesystem::path("Terrain") / xRecipe.m_pxWorldSpec->m_szTerrainSet;
	}

	std::filesystem::path ManifestPath(const ZM_TerrainAuthoringRecipe& xRecipe,
		const std::filesystem::path& xGameAssetsRoot)
	{
		return (xGameAssetsRoot / RelativeTerrainDirectory(xRecipe) / szMANIFEST_NAME).lexically_normal();
	}

	void WriteU32LE(u_int8* pBytes, u_int uValue)
	{
		pBytes[0] = static_cast<u_int8>(uValue & 0xffu);
		pBytes[1] = static_cast<u_int8>((uValue >> 8u) & 0xffu);
		pBytes[2] = static_cast<u_int8>((uValue >> 16u) & 0xffu);
		pBytes[3] = static_cast<u_int8>((uValue >> 24u) & 0xffu);
	}

	u_int ReadU32LE(const u_int8* pBytes)
	{
		return static_cast<u_int>(pBytes[0]) |
			(static_cast<u_int>(pBytes[1]) << 8u) |
			(static_cast<u_int>(pBytes[2]) << 16u) |
			(static_cast<u_int>(pBytes[3]) << 24u);
	}

	bool HasValidManifest(const std::filesystem::path& xPath)
	{
		std::error_code xError;
		if (!std::filesystem::is_regular_file(xPath, xError) || xError ||
			std::filesystem::file_size(xPath, xError) != uZM_TERRAIN_MANIFEST_SIZE || xError)
		{
			return false;
		}

		u_int8 auBytes[uZM_TERRAIN_MANIFEST_SIZE] = {};
		std::ifstream xInput(xPath, std::ios::binary);
		xInput.read(reinterpret_cast<char*>(auBytes), sizeof(auBytes));
		if (!xInput || memcmp(auBytes, acMANIFEST_MAGIC, uMANIFEST_MAGIC_SIZE) != 0)
		{
			return false;
		}
		return ReadU32LE(auBytes + 4u) == uZM_TERRAIN_MANIFEST_VERSION &&
			ReadU32LE(auBytes + 8u) == uZM_DAWNMERE_REQUIRED_OUTPUT_COUNT;
	}

	bool IsNonEmptyFile(const std::filesystem::path& xPath)
	{
		std::error_code xError;
		return std::filesystem::is_regular_file(xPath, xError) && !xError &&
			std::filesystem::file_size(xPath, xError) > 0u && !xError;
	}

#ifdef ZENITH_TOOLS
	std::filesystem::path PreparedChildPath(const std::string& strPreparedDirectory,
		const char* szName)
	{
		return std::filesystem::path(strPreparedDirectory) / szName;
	}

	bool RemovePreparedManifestFiles(const std::string& strPreparedDirectory)
	{
		const std::filesystem::path xMarker =
			PreparedChildPath(strPreparedDirectory, szMANIFEST_NAME);
		std::filesystem::path xTemp = xMarker;
		xTemp += ".tmp";
		std::error_code xError;
		std::filesystem::remove(xMarker, xError);
		if (xError)
		{
			return false;
		}
		std::filesystem::remove(xTemp, xError);
		return !xError;
	}

	bool AreRequiredOutputsCompleteInPreparedDirectory(
		const ZM_TerrainAuthoringRecipe& xRecipe,
		const std::string& strPreparedDirectory)
	{
		Zenith_Vector<std::string> xOutputs;
		ZM_EnumerateRequiredTerrainOutputs(xRecipe, xOutputs);
		for (u_int i = 0; i < xOutputs.GetSize(); ++i)
		{
			const std::filesystem::path xFilename =
				std::filesystem::path(xOutputs.Get(i)).filename();
			if (xFilename.empty() || !IsNonEmptyFile(
				std::filesystem::path(strPreparedDirectory) / xFilename))
			{
				return false;
			}
		}
		return true;
	}

	bool FinalizePreparedTerrainBake(const ZM_TerrainAuthoringRecipe& xRecipe,
		const std::string& strPreparedDirectory, const std::string& strTerrainRoot)
	{
		if (!AreRequiredOutputsCompleteInPreparedDirectory(xRecipe, strPreparedDirectory))
		{
			return false;
		}

		const std::filesystem::path xMarker =
			PreparedChildPath(strPreparedDirectory, szMANIFEST_NAME);
		std::filesystem::path xTemp = xMarker;
		xTemp += ".tmp";
		std::error_code xError;
		std::filesystem::remove(xTemp, xError);
		if (xError)
		{
			return false;
		}

		u_int8 auBytes[uZM_TERRAIN_MANIFEST_SIZE] = {};
		memcpy(auBytes, acMANIFEST_MAGIC, uMANIFEST_MAGIC_SIZE);
		WriteU32LE(auBytes + 4u, uZM_TERRAIN_MANIFEST_VERSION);
		WriteU32LE(auBytes + 8u, uZM_DAWNMERE_REQUIRED_OUTPUT_COUNT);
		{
			std::ofstream xOutput(xTemp, std::ios::binary | std::ios::trunc);
			xOutput.write(reinterpret_cast<const char*>(auBytes), sizeof(auBytes));
			xOutput.flush();
			if (!xOutput)
			{
				return false;
			}
		}

		// Preparation removed the old marker. Remove defensively for the
		// standalone test wrapper, then make same-directory rename the final write.
		std::filesystem::remove(xMarker, xError);
		if (xError)
		{
			std::filesystem::remove(xTemp, xError);
			return false;
		}
		if (!Zenith_TerrainComponent::RenamePreparedTerrainAssetFileAtomically(
			xRecipe.m_pxWorldSpec->m_szTerrainSet, strTerrainRoot,
			xTemp.filename().string(), xMarker.filename().string()))
		{
			std::filesystem::remove(xTemp, xError);
			return false;
		}
		return HasValidManifest(xMarker);
	}
#endif

	ZM_TerrainPlanOp MakeSimpleOp(ZM_TERRAIN_PLAN_OP_TYPE eType, u_int uIndex = 0u)
	{
		ZM_TerrainPlanOp xOp;
		xOp.m_eType = eType;
		xOp.m_uIndex = uIndex;
		return xOp;
	}

	void AppendDab(Zenith_Vector<ZM_TerrainPlanOp>& xPlan,
		ZM_TERRAIN_DAB_KIND eKind, ZM_TERRAIN_PLAN_PHASE ePhase,
		float fX, float fZ, float fRadius, float fStrength, float fValue)
	{
		ZM_TerrainPlanOp xOp;
		xOp.m_eType = ZM_TERRAIN_PLAN_BRUSH_DAB;
		xOp.m_eDabKind = eKind;
		xOp.m_ePhase = ePhase;
		xOp.m_fWorldX = fX;
		xOp.m_fWorldZ = fZ;
		xOp.m_fRadius = fRadius;
		xOp.m_fStrength = fStrength;
		xOp.m_fValue = fValue;
		xPlan.PushBack(xOp);
	}

	void AppendSampledPath(Zenith_Vector<ZM_TerrainPlanOp>& xPlan,
		const ZM_TerrainPathSpec& xPath, float fSpacing, float fRadius,
		ZM_TERRAIN_DAB_KIND eKind, ZM_TERRAIN_PLAN_PHASE ePhase, float fValue)
	{
		for (u_int uSegment = 0; uSegment + 1u < xPath.m_uPointCount; ++uSegment)
		{
			const ZM_TerrainPoint2& xA = xPath.m_pxPoints[uSegment];
			const ZM_TerrainPoint2& xB = xPath.m_pxPoints[uSegment + 1u];
			const float fDX = xB.m_fX - xA.m_fX;
			const float fDZ = xB.m_fZ - xA.m_fZ;
			const float fLength = sqrtf(fDX * fDX + fDZ * fDZ);
			const u_int uIntervals = static_cast<u_int>(ceilf(fLength / fSpacing));
			const u_int uFirstSample = uSegment == 0u ? 0u : 1u;
			for (u_int uSample = uFirstSample; uSample <= uIntervals; ++uSample)
			{
				const float fT = static_cast<float>(uSample) / static_cast<float>(uIntervals);
				AppendDab(xPlan, eKind, ePhase,
					xA.m_fX + fDX * fT, xA.m_fZ + fDZ * fT,
					fRadius, 1.0f, fValue);
			}
		}
	}

	void AppendFlattenPass(const ZM_TerrainAuthoringRecipe& xRecipe,
		Zenith_Vector<ZM_TerrainPlanOp>& xPlan, ZM_TERRAIN_PLAN_PHASE ePhase)
	{
		for (u_int i = 0; i < xRecipe.m_uPathCount; ++i)
		{
			const ZM_TerrainPathSpec& xPath = xRecipe.m_pxPaths[i];
			const u_int uStart = xPlan.GetSize();
			AppendSampledPath(xPlan, xPath, xPath.m_fFlattenSpacing,
				xPath.m_fFlattenRadius, ZM_TERRAIN_DAB_FLATTEN, ePhase,
				xRecipe.m_fTargetHeight);
			Zenith_Assert(xPlan.GetSize() - uStart == xPath.m_uFlattenSampleCount,
				"Terrain flatten sample-count drift for %s", xPath.m_szName);
		}
		for (u_int i = 0; i < xRecipe.m_uPadCount; ++i)
		{
			const ZM_TerrainPadSpec& xPad = xRecipe.m_pxPads[i];
			AppendDab(xPlan, ZM_TERRAIN_DAB_FLATTEN, ePhase,
				xPad.m_xCentre.m_fX, xPad.m_xCentre.m_fZ,
				xPad.m_fFlattenRadius, 1.0f, xRecipe.m_fTargetHeight);
		}
	}

	void AppendDirtPaint(const ZM_TerrainAuthoringRecipe& xRecipe,
		Zenith_Vector<ZM_TerrainPlanOp>& xPlan)
	{
		for (u_int i = 0; i < xRecipe.m_uPathCount; ++i)
		{
			const ZM_TerrainPathSpec& xPath = xRecipe.m_pxPaths[i];
			for (u_int uPass = 0; uPass < uDIRT_PATH_PASSES; ++uPass)
			{
				const u_int uStart = xPlan.GetSize();
				AppendSampledPath(xPlan, xPath, xPath.m_fDirtSpacing,
					xPath.m_fDirtRadius, ZM_TERRAIN_DAB_SPLAT,
					ZM_TERRAIN_PHASE_DIRT, 2.0f);
				Zenith_Assert(xPlan.GetSize() - uStart == xPath.m_uDirtSampleCount,
					"Terrain dirt sample-count drift for %s", xPath.m_szName);
			}
		}
		for (u_int i = 0; i < xRecipe.m_uPadCount; ++i)
		{
			const ZM_TerrainPadSpec& xPad = xRecipe.m_pxPads[i];
			for (u_int uPass = 0; uPass < xPad.m_uDirtPassCount; ++uPass)
			{
				AppendDab(xPlan, ZM_TERRAIN_DAB_SPLAT, ZM_TERRAIN_PHASE_DIRT,
					xPad.m_xCentre.m_fX, xPad.m_xCentre.m_fZ,
					xPad.m_fDirtRadius, 1.0f, 2.0f);
			}
		}
	}

	void AppendGrass(const ZM_TerrainAuthoringRecipe& xRecipe,
		Zenith_Vector<ZM_TerrainPlanOp>& xPlan)
	{
		for (u_int i = 0; i < xRecipe.m_uGrassDabCount; ++i)
		{
			const ZM_TerrainGrassDabSpec& xDab = xRecipe.m_pxGrassDabs[i];
			AppendDab(xPlan, ZM_TERRAIN_DAB_GRASS_DENSITY, ZM_TERRAIN_PHASE_GRASS_FILL,
				xDab.m_xCentre.m_fX, xDab.m_xCentre.m_fZ,
				xDab.m_fRadius, 1.0f, xDab.m_fTargetDensity);
		}

		// Erasing the densely sampled paths and every flattened pad is the final
		// density phase. This prevents any later fill from repopulating walkways.
		for (u_int i = 0; i < xRecipe.m_uPathCount; ++i)
		{
			const ZM_TerrainPathSpec& xPath = xRecipe.m_pxPaths[i];
			AppendSampledPath(xPlan, xPath, xPath.m_fDirtSpacing,
				xPath.m_fFlattenRadius, ZM_TERRAIN_DAB_GRASS_DENSITY,
				ZM_TERRAIN_PHASE_GRASS_ERASE, 0.0f);
		}
		for (u_int i = 0; i < xRecipe.m_uPadCount; ++i)
		{
			const ZM_TerrainPadSpec& xPad = xRecipe.m_pxPads[i];
			AppendDab(xPlan, ZM_TERRAIN_DAB_GRASS_DENSITY,
				ZM_TERRAIN_PHASE_GRASS_ERASE,
				xPad.m_xCentre.m_fX, xPad.m_xCentre.m_fZ,
				xPad.m_fFlattenRadius, 1.0f, 0.0f);
		}
	}

#ifdef ZENITH_TOOLS
	void RunDawnmereRegionErosion()
	{
		const ZM_TerrainAuthoringRecipe& xRecipe = ZM_GetDawnmereTerrainRecipe();
		Zenith_TerrainErosionParams xParams;
		xParams.m_uSeed = xRecipe.m_uSeed;
		xParams.m_uHydraulicDroplets = xRecipe.m_xErosion.m_uHydraulicDroplets;
		xParams.m_uThermalIterations = xRecipe.m_xErosion.m_uThermalIterations;
		xParams.m_bRegionOnly = xRecipe.m_xErosion.m_bRegionOnly;
		xParams.m_fRegionCentreX = xRecipe.m_xErosion.m_xCentre.m_fX;
		xParams.m_fRegionCentreZ = xRecipe.m_xErosion.m_xCentre.m_fZ;
		xParams.m_fRegionRadius = xRecipe.m_xErosion.m_fRadius;
		g_xEngine.TerrainEditor().RunErosion(xParams, true);
	}

	void RunDawnmereTerminalBake()
	{
		const ZM_TerrainAuthoringRecipe& xRecipe = ZM_GetDawnmereTerrainRecipe();
		Zenith_TerrainEditor& xEditor = g_xEngine.TerrainEditor();
		const char* szExpectedSet = xRecipe.m_pxWorldSpec->m_szTerrainSet;
		const std::string strTerrainRoot =
			(std::filesystem::path(GAME_ASSETS_DIR) / "Terrain").string();
		bool bLeaseEntered = false;
		const char* szFailureStage = "asset-directory lease";
		const bool bSucceeded = Zenith_TerrainComponent::WithPreparedTerrainAssetDirectory(
			szExpectedSet, strTerrainRoot,
			[&](const std::string& strPreparedDirectory)
			{
				bLeaseEntered = true;
				Zenith_Log(LOG_CATEGORY_TERRAIN,
					"[ZM Terrain] Terminal bake begin: stagedSet='%s', expectedSet='%s', output='%s', status='%s'",
					xEditor.GetAssetSet().c_str(), szExpectedSet,
					strPreparedDirectory.c_str(), xEditor.m_strStatus.c_str());

				bool bStepSucceeded = xEditor.GetAssetSet() == szExpectedSet;
				if (!bStepSucceeded)
				{
					szFailureStage = "staged asset-set validation";
					Zenith_Error(LOG_CATEGORY_TERRAIN,
						"[ZM Terrain] Staged-set mismatch before persistence: staged='%s', expected='%s', output='%s'",
						xEditor.GetAssetSet().c_str(), szExpectedSet,
						strPreparedDirectory.c_str());
				}
				if (bStepSucceeded)
				{
					bStepSucceeded = xEditor.SaveTextures();
					if (!bStepSucceeded)
					{
						szFailureStage = "SaveTextures";
						Zenith_Error(LOG_CATEGORY_TERRAIN,
							"[ZM Terrain] SaveTextures failed: stagedSet='%s', output='%s', status='%s'",
							xEditor.GetAssetSet().c_str(), strPreparedDirectory.c_str(),
							xEditor.m_strStatus.c_str());
					}
				}

				Flux_TerrainExportRect xRect;
				if (bStepSucceeded)
				{
					bStepSucceeded = Flux_TerrainExportRect::TryCreate(
						xRecipe.m_xExportRect.m_iMinX, xRecipe.m_xExportRect.m_iMinY,
						xRecipe.m_xExportRect.m_iMaxX, xRecipe.m_xExportRect.m_iMaxY,
						xRect);
					if (!bStepSucceeded)
					{
						szFailureStage = "export-rectangle validation";
					}
				}
				if (bStepSucceeded)
				{
					bStepSucceeded = xEditor.BakeMeshesRect(xRect);
					if (!bStepSucceeded)
					{
						szFailureStage = "BakeMeshesRect";
						Zenith_Error(LOG_CATEGORY_TERRAIN,
							"[ZM Terrain] BakeMeshesRect failed: stagedSet='%s', output='%s', status='%s'",
							xEditor.GetAssetSet().c_str(), strPreparedDirectory.c_str(),
							xEditor.m_strStatus.c_str());
					}
				}
				if (bStepSucceeded)
				{
					bStepSucceeded = FinalizePreparedTerrainBake(
						xRecipe, strPreparedDirectory, strTerrainRoot);
					if (!bStepSucceeded)
					{
						szFailureStage = "771-output validation/manifest finalization";
						Zenith_Error(LOG_CATEGORY_TERRAIN,
							"[ZM Terrain] Output validation/finalization failed: stagedSet='%s', output='%s'",
							xEditor.GetAssetSet().c_str(), strPreparedDirectory.c_str());
					}
				}

				if (!bStepSucceeded && !RemovePreparedManifestFiles(strPreparedDirectory))
				{
					Zenith_Error(LOG_CATEGORY_TERRAIN,
						"[ZM Terrain] Failed to remove terminal manifest residue while target lease was held");
				}
				return bStepSucceeded;
			});

		if (!bSucceeded)
		{
			if (!bLeaseEntered)
			{
				Zenith_Error(LOG_CATEGORY_TERRAIN,
					"[ZM Terrain] Terminal asset-directory lease rejected Dawnmere; target may have been replaced after preparation");
			}
			Zenith_Error(LOG_CATEGORY_TERRAIN,
				"Dawnmere terminal terrain bake failed at %s; completion marker remains absent",
				szFailureStage);
			Zenith_Assert(false,
				"Dawnmere terminal terrain bake failed before atomic manifest finalization");
			return;
		}
		Zenith_Log(LOG_CATEGORY_TERRAIN,
			"[ZM Terrain] Terminal bake complete: 771 required outputs and manifest finalized");
	}

	int ResolveTerrainTool(ZM_TERRAIN_DAB_KIND eKind)
	{
		switch (eKind)
		{
		case ZM_TERRAIN_DAB_SET_HEIGHT: return static_cast<int>(Zenith_TerrainBrushTool::SetHeight);
		case ZM_TERRAIN_DAB_FLATTEN: return static_cast<int>(Zenith_TerrainBrushTool::Flatten);
		case ZM_TERRAIN_DAB_SPLAT: return static_cast<int>(Zenith_TerrainBrushTool::SplatPaint);
		case ZM_TERRAIN_DAB_GRASS_DENSITY: return static_cast<int>(Zenith_TerrainBrushTool::GrassDensity);
		default:
			Zenith_Assert(false, "Unknown Zenithmon terrain dab kind");
			return static_cast<int>(Zenith_TerrainBrushTool::Raise);
		}
	}
#endif
}

u_int ZM_Fnv1a32(const char* szText)
{
	u_int uHash = 2166136261u;
	if (szText == nullptr)
	{
		return uHash;
	}
	for (const u_int8* pByte = reinterpret_cast<const u_int8*>(szText);
		*pByte != 0u; ++pByte)
	{
		uHash ^= *pByte;
		uHash *= 16777619u;
	}
	return uHash;
}

const ZM_TerrainAuthoringRecipe& ZM_GetDawnmereTerrainRecipe()
{
	const ZM_WorldSpec& xWorldSpec = ZM_GetWorldSpec(ZM_SCENE_DAWNMERE);
	static const ZM_TerrainAuthoringRecipe s_xRecipe =
	{
		&xWorldSpec,
		ZM_Fnv1a32(xWorldSpec.m_szTerrainSet),
		0.0f,
		1024.0f,
		{ 0, 0, 15, 15 },
		24.0f,
		{ 0.046875f, 0.018f, 0.00125f, 5u, 2.0f, 0.5f, 0.10f },
		s_axDawnmereLandforms,
		CountOf(s_axDawnmereLandforms),
		s_axDawnmerePaths,
		CountOf(s_axDawnmerePaths),
		s_axDawnmerePads,
		CountOf(s_axDawnmerePads),
		{ 60000u, 1u, true, { 512.0f, 512.0f }, 725.0f },
		s_axDawnmereAutoSplat,
		CountOf(s_axDawnmereAutoSplat),
		s_axDawnmereGrassDabs,
		CountOf(s_axDawnmereGrassDabs),
		s_axDawnmereLandmarks,
		CountOf(s_axDawnmereLandmarks),
		s_axDawnmereMaterials,
		CountOf(s_axDawnmereMaterials),
		{ { 512.0f, 52.0f, 420.0f }, 0.0f, -0.22f, 65.0f, 0.1f, 2000.0f },
	};
	return s_xRecipe;
}

void ZM_BuildTerrainAuthoringPlan(const ZM_TerrainAuthoringRecipe& xRecipe,
	Zenith_Vector<ZM_TerrainPlanOp>& xPlanOut)
{
	xPlanOut.Clear();
	xPlanOut.PushBack(MakeSimpleOp(ZM_TERRAIN_PLAN_RESET));
	// RESET is deliberately first. The automation executor opens a standalone
	// session on the first terrain action; staging before that open relied on
	// session internals retaining the candidate. Staging immediately after the
	// clean reset makes Dawnmere the unambiguous target for every later action.
	xPlanOut.PushBack(MakeSimpleOp(ZM_TERRAIN_PLAN_SET_ASSET_SET));
	xPlanOut.PushBack(MakeSimpleOp(ZM_TERRAIN_PLAN_GENERATE_PROCEDURAL));

	for (u_int i = 0; i < xRecipe.m_uLandformCount; ++i)
	{
		const ZM_TerrainLandformSpec& xLandform = xRecipe.m_pxLandforms[i];
		AppendDab(xPlanOut, ZM_TERRAIN_DAB_SET_HEIGHT, ZM_TERRAIN_PHASE_LANDFORM,
			xLandform.m_xCentre.m_fX, xLandform.m_xCentre.m_fZ,
			xLandform.m_fRadius, xLandform.m_fStrength, xLandform.m_fHeight);
	}

	AppendFlattenPass(xRecipe, xPlanOut, ZM_TERRAIN_PHASE_FLATTEN_PRE_EROSION);
	xPlanOut.PushBack(MakeSimpleOp(ZM_TERRAIN_PLAN_EROSION));
	AppendFlattenPass(xRecipe, xPlanOut, ZM_TERRAIN_PHASE_FLATTEN_POST_EROSION);

	for (u_int i = 0; i < xRecipe.m_uAutoSplatCount; ++i)
	{
		xPlanOut.PushBack(MakeSimpleOp(ZM_TERRAIN_PLAN_AUTO_SPLAT_RULE, i));
	}
	xPlanOut.PushBack(MakeSimpleOp(ZM_TERRAIN_PLAN_RUN_AUTO_SPLAT));
	AppendDirtPaint(xRecipe, xPlanOut);
	AppendGrass(xRecipe, xPlanOut);
	xPlanOut.PushBack(MakeSimpleOp(ZM_TERRAIN_PLAN_TERMINAL_BAKE));
}

void ZM_EnumerateRequiredTerrainOutputs(const ZM_TerrainAuthoringRecipe& xRecipe,
	Zenith_Vector<std::string>& xOutputsOut)
{
	xOutputsOut.Clear();
	const std::filesystem::path xDirectory = RelativeTerrainDirectory(xRecipe);
	const char* aszPrefixes[] = { "Render", "Render_LOW", "Physics" };
	for (u_int uPrefix = 0; uPrefix < CountOf(aszPrefixes); ++uPrefix)
	{
		for (int iY = xRecipe.m_xExportRect.m_iMinY;
			iY <= xRecipe.m_xExportRect.m_iMaxY; ++iY)
		{
			for (int iX = xRecipe.m_xExportRect.m_iMinX;
				iX <= xRecipe.m_xExportRect.m_iMaxX; ++iX)
			{
				const std::string strName = std::string(aszPrefixes[uPrefix]) + "_" +
					std::to_string(iX) + "_" + std::to_string(iY) + ZENITH_MESH_EXT;
				xOutputsOut.PushBack((xDirectory / strName).generic_string());
			}
		}
	}
	xOutputsOut.PushBack((xDirectory / (std::string("Height") + ZENITH_TEXTURE_EXT)).generic_string());
	xOutputsOut.PushBack((xDirectory / (std::string("Splatmap_RGBA") + ZENITH_TEXTURE_EXT)).generic_string());
	xOutputsOut.PushBack((xDirectory / (std::string("GrassDensity") + ZENITH_TEXTURE_EXT)).generic_string());
	Zenith_Assert(xOutputsOut.GetSize() == uZM_DAWNMERE_REQUIRED_OUTPUT_COUNT,
		"Terrain required-output count drifted from manifest contract");
}

std::string ZM_GetTerrainManifestRelativePath(const ZM_TerrainAuthoringRecipe& xRecipe)
{
	return (RelativeTerrainDirectory(xRecipe) / szMANIFEST_NAME).generic_string();
}

std::string ZM_GetTerrainGrassAssetPath(const ZM_TerrainAuthoringRecipe& xRecipe)
{
	return std::string("game:") + (RelativeTerrainDirectory(xRecipe) /
		(std::string("GrassDensity") + ZENITH_TEXTURE_EXT)).generic_string();
}

ZM_TERRAIN_BAKE_QUEUE_RESULT ZM_DetermineTerrainBakeQueueResult(bool bHeadless,
	bool bForce, bool bWarm, bool bPrepareSucceeded)
{
	if (bHeadless)
	{
		return ZM_TERRAIN_BAKE_HEADLESS;
	}
	if (!bForce && bWarm)
	{
		return ZM_TERRAIN_BAKE_WARM;
	}
	return bPrepareSucceeded ? ZM_TERRAIN_BAKE_QUEUED :
		ZM_TERRAIN_BAKE_PREPARE_FAILED;
}

bool ZM_IsTerrainBakeWarm(const ZM_TerrainAuthoringRecipe& xRecipe,
	const std::filesystem::path& xGameAssetsRoot)
{
	if (xGameAssetsRoot.empty() || !HasValidManifest(ManifestPath(xRecipe, xGameAssetsRoot)))
	{
		return false;
	}

	Zenith_Vector<std::string> xOutputs;
	ZM_EnumerateRequiredTerrainOutputs(xRecipe, xOutputs);
	for (u_int i = 0; i < xOutputs.GetSize(); ++i)
	{
		if (!IsNonEmptyFile(xGameAssetsRoot / xOutputs.Get(i)))
		{
			return false;
		}
	}
	return true;
}

#ifdef ZENITH_TOOLS
bool ZM_PrepareTerrainBake(const ZM_TerrainAuthoringRecipe& xRecipe,
	const std::filesystem::path& xGameAssetsRoot)
{
	if (xGameAssetsRoot.empty())
	{
		return false;
	}
	const std::string strTerrainRoot = (xGameAssetsRoot / "Terrain").string();
	return Zenith_TerrainComponent::WithPreparedTerrainAssetDirectory(
		xRecipe.m_pxWorldSpec->m_szTerrainSet, strTerrainRoot,
		[&](const std::string& strPreparedDirectory)
		{
			const std::filesystem::path xMarker =
				PreparedChildPath(strPreparedDirectory, szMANIFEST_NAME);
			std::filesystem::path xTemp = xMarker;
			xTemp += ".tmp";
			const std::filesystem::path axStalePaths[] =
			{
				xMarker,
				xTemp,
				PreparedChildPath(strPreparedDirectory, "Height" ZENITH_TEXTURE_EXT),
				PreparedChildPath(strPreparedDirectory, "Splatmap_RGBA" ZENITH_TEXTURE_EXT),
				PreparedChildPath(strPreparedDirectory, "GrassDensity" ZENITH_TEXTURE_EXT),
			};
			for (const std::filesystem::path& xPath : axStalePaths)
			{
				std::error_code xError;
				std::filesystem::remove(xPath, xError);
				if (xError)
				{
					// No automation has been queued yet. Marker-first ordering means a
					// later removal failure cannot leave an old bake looking complete.
					return false;
				}
			}
			return true;
		});
}

bool ZM_FinalizeTerrainBake(const ZM_TerrainAuthoringRecipe& xRecipe,
	const std::filesystem::path& xGameAssetsRoot)
{
	if (xGameAssetsRoot.empty())
	{
		return false;
	}

	const std::string strTerrainRoot = (xGameAssetsRoot / "Terrain").string();
	return Zenith_TerrainComponent::WithPreparedTerrainAssetDirectory(
		xRecipe.m_pxWorldSpec->m_szTerrainSet, strTerrainRoot,
		[&](const std::string& strPreparedDirectory)
		{
			return FinalizePreparedTerrainBake(
				xRecipe, strPreparedDirectory, strTerrainRoot);
		});
}

ZM_TERRAIN_BAKE_QUEUE_RESULT ZM_QueueDawnmereTerrainBake(
	Zenith_EditorAutomation& xAutomation,
	bool bHeadless, bool bForce)
{
	const ZM_TerrainAuthoringRecipe& xRecipe = ZM_GetDawnmereTerrainRecipe();
	const std::filesystem::path xAssetsRoot(GAME_ASSETS_DIR);
	const bool bWarm = ZM_IsTerrainBakeWarm(xRecipe, xAssetsRoot);
	const ZM_TERRAIN_BAKE_QUEUE_RESULT eInitial =
		ZM_DetermineTerrainBakeQueueResult(bHeadless, bForce, bWarm, true);
	if (eInitial != ZM_TERRAIN_BAKE_QUEUED)
	{
		return eInitial;
	}
	if (!ZM_PrepareTerrainBake(xRecipe, xAssetsRoot))
	{
		Zenith_Error(LOG_CATEGORY_TERRAIN,
			"Could not invalidate the prior Dawnmere terrain bake; queue remains empty");
		return ZM_TERRAIN_BAKE_PREPARE_FAILED;
	}

	Zenith_Vector<ZM_TerrainPlanOp> xPlan;
	ZM_BuildTerrainAuthoringPlan(xRecipe, xPlan);
	for (u_int i = 0; i < xPlan.GetSize(); ++i)
	{
		const ZM_TerrainPlanOp& xOp = xPlan.Get(i);
		switch (xOp.m_eType)
		{
		case ZM_TERRAIN_PLAN_SET_ASSET_SET:
			xAutomation.AddStep_TerrainSetAssetSet(xRecipe.m_pxWorldSpec->m_szTerrainSet);
			break;
		case ZM_TERRAIN_PLAN_RESET:
			xAutomation.AddStep_TerrainResetSession();
			break;
		case ZM_TERRAIN_PLAN_GENERATE_PROCEDURAL:
			xAutomation.AddStep_TerrainGenerateProcedural(
				static_cast<int>(xRecipe.m_uSeed),
				xRecipe.m_xProcedural.m_fBaseHeight,
				xRecipe.m_xProcedural.m_fAmplitude,
				xRecipe.m_xProcedural.m_fFrequency,
				static_cast<int>(xRecipe.m_xProcedural.m_uOctaves),
				xRecipe.m_xProcedural.m_fLacunarity,
				xRecipe.m_xProcedural.m_fGain,
				xRecipe.m_xProcedural.m_fRidgedBlend);
			break;
		case ZM_TERRAIN_PLAN_BRUSH_DAB:
			xAutomation.AddStep_TerrainBrushStroke(ResolveTerrainTool(xOp.m_eDabKind),
				xOp.m_fWorldX, xOp.m_fWorldZ, xOp.m_fRadius,
				xOp.m_fStrength, xOp.m_fValue);
			break;
		case ZM_TERRAIN_PLAN_EROSION:
			xAutomation.AddStep_Custom(&RunDawnmereRegionErosion);
			break;
		case ZM_TERRAIN_PLAN_AUTO_SPLAT_RULE:
		{
			const ZM_TerrainAutoSplatSpec& xRule = xRecipe.m_pxAutoSplat[xOp.m_uIndex];
			xAutomation.AddStep_TerrainAutoSplatRule(static_cast<int>(xOp.m_uIndex),
				xRule.m_fHeightMin, xRule.m_fHeightMax,
				xRule.m_fSlopeMin, xRule.m_fSlopeMax,
				xRule.m_fWeight, xRule.m_fJitter);
			break;
		}
		case ZM_TERRAIN_PLAN_RUN_AUTO_SPLAT:
			xAutomation.AddStep_TerrainRunAutoSplat();
			break;
		case ZM_TERRAIN_PLAN_TERMINAL_BAKE:
			xAutomation.AddStep_Custom(&RunDawnmereTerminalBake);
			break;
		default:
			Zenith_Assert(false, "Unknown Zenithmon terrain plan operation");
			break;
		}
	}
	return ZM_TERRAIN_BAKE_QUEUED;
}
#endif
