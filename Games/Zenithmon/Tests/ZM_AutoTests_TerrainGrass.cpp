#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Flux/Vegetation/Flux_GrassImpl.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_TerrainGrassComponent.h"

#include <array>
#include <cmath>
#include <filesystem>
#include <string>

namespace
{
	enum class GrassTestPhase
	{
		FirstLoad,
		Reload,
		FrontEnd,
		Done,
	};

	GrassTestPhase g_eGrassPhase = GrassTestPhase::Done;
	int g_iGrassPhaseFrames = 0;
	bool g_bGrassPrerequisitesPresent = false;
	bool g_bGrassPassed = false;
	u_int g_uFirstBladeCount = 0;
	const char* g_szGrassFailure = "";

	ZM_TerrainGrass* FindTerrainGrass()
	{
		ZM_TerrainGrass* pxFound = nullptr;
		g_xEngine.Scenes().QueryAllScenes<ZM_TerrainGrass>().ForEach(
			[&pxFound](Zenith_EntityID, ZM_TerrainGrass& xGrass)
			{
				if (pxFound == nullptr)
				{
					pxFound = &xGrass;
				}
			});
		return pxFound;
	}

	bool RequiredGrassAssetsPresent()
	{
		const std::string strRoot = std::string(GAME_ASSETS_DIR);
		const std::array<std::string, 7> astrRequired = {
			strRoot + "Scenes/Dawnmere" + ZENITH_SCENE_EXT,
			strRoot + "Terrain/Dawnmere/Height" + ZENITH_TEXTURE_EXT,
			strRoot + "Terrain/Dawnmere/Splatmap_RGBA" + ZENITH_TEXTURE_EXT,
			strRoot + "Terrain/Dawnmere/GrassDensity" + ZENITH_TEXTURE_EXT,
			strRoot + "Terrain/Dawnmere/Physics_0_0" + ZENITH_MESH_EXT,
			strRoot + "Terrain/Dawnmere/Render_LOW_0_0" + ZENITH_MESH_EXT,
			strRoot + "Terrain/Dawnmere/Render_0_0" + ZENITH_MESH_EXT,
		};
		for (const std::string& strPath : astrRequired)
		{
			std::error_code xError;
			if (!std::filesystem::is_regular_file(strPath, xError) || xError)
			{
				return false;
			}
			const std::uintmax_t ulFileSize = std::filesystem::file_size(strPath, xError);
			if (xError || ulFileSize == 0)
			{
				return false;
			}
		}
		return true;
	}

	void FailGrassTest(const char* szReason)
	{
		g_szGrassFailure = szReason;
		g_eGrassPhase = GrassTestPhase::Done;
		g_bGrassPassed = false;
	}

	bool ValidateAppliedGrass(ZM_TerrainGrass& xComponent, u_int& uBladeCountOut)
	{
		Flux_GrassImpl& xGrass = g_xEngine.Grass();
		if (!xComponent.HasCPUMap())
		{
			FailGrassTest("Dawnmere grass component did not retain its CPU density map");
			return false;
		}
		if (xComponent.GetDensityMap().GetWidth() != ZM_GrassDensityMap::uEXPECTED_WIDTH
			|| xComponent.GetDensityMap().GetHeight() != ZM_GrassDensityMap::uEXPECTED_HEIGHT
			|| xComponent.GetDensityMap().GetPixelCount()
				!= static_cast<size_t>(ZM_GrassDensityMap::uEXPECTED_WIDTH)
					* ZM_GrassDensityMap::uEXPECTED_HEIGHT)
		{
			FailGrassTest("Dawnmere CPU grass map does not match the exact 1024-square contract");
			return false;
		}
		if (!xGrass.HasDensityMap())
		{
			FailGrassTest("Dawnmere grass component did not install the Flux density map");
			return false;
		}
		if (xGrass.m_uDensityMapWidth != ZM_GrassDensityMap::uEXPECTED_WIDTH)
		{
			FailGrassTest("Flux grass density-map width is not 1024");
			return false;
		}
		if (xGrass.m_uDensityMapHeight != ZM_GrassDensityMap::uEXPECTED_HEIGHT)
		{
			FailGrassTest("Flux grass density-map height is not 1024");
			return false;
		}
		if (xGrass.m_xDensityMap.GetSize()
			!= ZM_GrassDensityMap::uEXPECTED_WIDTH * ZM_GrassDensityMap::uEXPECTED_HEIGHT)
		{
			FailGrassTest("Flux grass density-map copied pixel count is not 1,048,576");
			return false;
		}
		if (std::fabs(xGrass.m_fDensityMapWorldSize - ZM_GrassDensityMap::fWORLD_SIZE)
			> 0.0001f)
		{
			FailGrassTest("Flux grass density-map world size is not 4096");
			return false;
		}
		if (std::fabs(xComponent.GetAppliedDensityScale() - 0.15f) > 0.0001f)
		{
			FailGrassTest("Dawnmere generation did not apply density scale 0.15");
			return false;
		}
		uBladeCountOut = xComponent.GetGeneratedBladeCount();
		if (uBladeCountOut == 0)
		{
			FailGrassTest("Dawnmere grass regeneration produced zero blades");
			return false;
		}
		if (xGrass.GetGeneratedInstanceCount() != uBladeCountOut)
		{
			FailGrassTest("captured grass blade count differs from the generated instance array");
			return false;
		}
		if (!xGrass.HasGeneratedInstances() || !xGrass.HasUploadedInstances())
		{
			FailGrassTest("Dawnmere grass instances were not generated and uploaded");
			return false;
		}
		return true;
	}
}

static void Setup_ZMGrassRegeneration()
{
	g_eGrassPhase = GrassTestPhase::Done;
	g_iGrassPhaseFrames = 0;
	g_bGrassPrerequisitesPresent = RequiredGrassAssetsPresent();
	g_bGrassPassed = false;
	g_uFirstBladeCount = 0;
	g_szGrassFailure = "";

	if (!g_bGrassPrerequisitesPresent)
	{
		Zenith_AutomatedTestRunner::RequestSkip(
			"Dawnmere scene/terrain bake is absent or incomplete");
		return;
	}

	g_eGrassPhase = GrassTestPhase::FirstLoad;
	g_xEngine.Scenes().LoadSceneByIndex(2, SCENE_LOAD_SINGLE);
}

static bool Step_ZMGrassRegeneration(int)
{
	if (!g_bGrassPrerequisitesPresent || g_eGrassPhase == GrassTestPhase::Done)
	{
		return false;
	}

	++g_iGrassPhaseFrames;
	if ((g_eGrassPhase == GrassTestPhase::FirstLoad
			|| g_eGrassPhase == GrassTestPhase::Reload)
		&& g_iGrassPhaseFrames > 360)
	{
		FailGrassTest("Dawnmere grass load did not complete within its bounded retry window");
		return false;
	}
	if (g_eGrassPhase == GrassTestPhase::FrontEnd && g_iGrassPhaseFrames > 120)
	{
		FailGrassTest("FrontEnd teardown did not complete within 120 frames");
		return false;
	}
	switch (g_eGrassPhase)
	{
	case GrassTestPhase::FirstLoad:
	{
		ZM_TerrainGrass* pxComponent = FindTerrainGrass();
		if (pxComponent == nullptr)
		{
			return true;
		}
		if (pxComponent->HasTerminalFailure())
		{
			FailGrassTest("first Dawnmere load reported a terminal grass failure");
			return false;
		}
		if (!pxComponent->IsGrassApplied())
		{
			return true;
		}
		if (!ValidateAppliedGrass(*pxComponent, g_uFirstBladeCount))
		{
			return false;
		}

		g_eGrassPhase = GrassTestPhase::Reload;
		g_iGrassPhaseFrames = 0;
		g_xEngine.Scenes().LoadSceneByIndex(2, SCENE_LOAD_SINGLE);
		return true;
	}

	case GrassTestPhase::Reload:
	{
		// SINGLE loads are deferred; do not accidentally inspect the outgoing
		// instance before its OnDestroy/new-scene replacement has completed.
		if (g_iGrassPhaseFrames < 5)
		{
			return true;
		}
		ZM_TerrainGrass* pxComponent = FindTerrainGrass();
		if (pxComponent == nullptr)
		{
			return true;
		}
		if (pxComponent->HasTerminalFailure())
		{
			FailGrassTest("reloaded Dawnmere reported a terminal grass failure");
			return false;
		}
		if (!pxComponent->IsGrassApplied())
		{
			return true;
		}

		u_int uReloadedBladeCount = 0;
		if (!ValidateAppliedGrass(*pxComponent, uReloadedBladeCount))
		{
			return false;
		}
		if (uReloadedBladeCount != g_uFirstBladeCount)
		{
			FailGrassTest("Dawnmere reload changed or accumulated the generated blade count");
			return false;
		}

		g_eGrassPhase = GrassTestPhase::FrontEnd;
		g_iGrassPhaseFrames = 0;
		g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
		return true;
	}

	case GrassTestPhase::FrontEnd:
		if (g_iGrassPhaseFrames < 5)
		{
			return true;
		}
		if (FindTerrainGrass() != nullptr)
		{
			return true;
		}
		if (g_xEngine.Grass().HasDensityMap())
		{
			FailGrassTest("FrontEnd retained Dawnmere's Flux grass density map");
			return false;
		}
		if (g_xEngine.Grass().GetActiveChunkCount() != 0
			|| g_xEngine.Grass().GetVisibleBladeCount() != 0)
		{
			FailGrassTest("FrontEnd retained active or visible Dawnmere grass chunks");
			return false;
		}
		if (g_xEngine.Grass().GetChunkCount() != 0)
		{
			FailGrassTest("FrontEnd retained Dawnmere grass chunk state");
			return false;
		}
		if (g_xEngine.Grass().GetGeneratedInstanceCount() != 0
			|| g_xEngine.Grass().HasGeneratedInstances()
			|| g_xEngine.Grass().HasUploadedInstances())
		{
			FailGrassTest("FrontEnd retained generated or uploaded Dawnmere grass instances");
			return false;
		}
		g_bGrassPassed = true;
		g_eGrassPhase = GrassTestPhase::Done;
		return false;

	case GrassTestPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_ZMGrassRegeneration()
{
	if (!g_bGrassPassed && g_bGrassPrerequisitesPresent)
	{
		Zenith_Error(LOG_CATEGORY_UNITTEST, "[ZM_GrassRegeneration] %s", g_szGrassFailure);
	}
	return g_bGrassPassed || !g_bGrassPrerequisitesPresent;
}

static const Zenith_AutomatedTest g_xZMGrassRegenerationTest = {
	"ZM_GrassRegeneration_Test",
	&Setup_ZMGrassRegeneration,
	&Step_ZMGrassRegeneration,
	&Verify_ZMGrassRegeneration,
	/* maxFrames */ 900,
	true /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMGrassRegenerationTest);

#endif // ZENITH_INPUT_SIMULATOR
