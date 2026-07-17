#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Flux/Vegetation/Flux_GrassImpl.h"
#include "Input/Zenith_InputSimulator.h"
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
		SpawnVisible,
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
		if (std::fabs(xComponent.GetAppliedDensityScale() - 0.70f) > 0.0001f)
		{
			FailGrassTest("Dawnmere generation did not apply density scale 0.70");
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

		// Proven generated + uploaded; prove grass is DRAWN from the authored
		// spawn camera before the reload path (closes the generated-but-invisible
		// gap where a blade-count check passed while zero blades reached screen).
		g_eGrassPhase = GrassTestPhase::SpawnVisible;
		g_iGrassPhaseFrames = 0;
		return true;
	}

	case GrassTestPhase::SpawnVisible:
	{
		// ExecuteRender recomputes the visible blade count during Render, one
		// frame behind this Update-phase Step, so allow a bounded settle window
		// for the follow camera to frame the surrounding lawn.
		if (g_xEngine.Grass().GetVisibleBladeCount() > 0)
		{
			g_eGrassPhase = GrassTestPhase::Reload;
			g_iGrassPhaseFrames = 0;
			g_xEngine.Scenes().LoadSceneByIndex(2, SCENE_LOAD_SINGLE);
			return true;
		}
		if (g_iGrassPhaseFrames > 60)
		{
			FailGrassTest(
				"Dawnmere grass generated + uploaded but zero blades were visible from the spawn camera");
			return false;
		}
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

// ============================================================================
// ZM_TerrainGrassResumeRegen_Test -- the S5 item-3 SC2 gate for the explicit
// grass-restore seam ZM_TerrainGrass::RegenerateForSceneResume().
//
// NO battle is involved. The additive-battle round trip clears the engine-owned
// grass singleton on battle entry, and neither existing path restores it: the
// engine E5 render-reset hook fires only on SINGLE loads, and ZM_TerrainGrass's
// OnUpdate cannot self-heal while its scene is paused (its apply path is latched
// behind m_bGrassApplied, which a bare singleton clear does not touch). This test
// isolates exactly that contract -- generate, clear the singleton directly, drive
// the restore seam, and prove the SAME deterministic blade count comes back --
// so a failure localises to the seam rather than to the battle transition.
//
// GATING: m_bRequiresGraphics = true, so the headless CI batch skips it and the
// unit baseline is unchanged. Setup RequestSkip()s when the git-ignored Dawnmere
// bake is absent, so a fresh CI checkout skips rather than fails.
// ============================================================================

namespace
{
	constexpr float fFIXED_DT = 1.0f / 60.0f;

	enum class ResumePhase
	{
		AwaitGrass,
		ConfirmCleared,
		Capture,
		Done,
	};

	// Per-phase deadlines sum to 422, inside the 600-frame cap below.
	constexpr int iRESUME_GRASS_DEADLINE = 420;   // Dawnmere first-load generation window

	ResumePhase g_eResumePhase             = ResumePhase::Done;
	int         g_iResumePhaseFrames       = 0;
	bool        g_bResumePrereqsPresent    = false;
	bool        g_bResumeActive            = false;
	bool        g_bResumeFailed            = false;
	const char* g_szResumeFailure          = "test did not reach verification";

	u_int       g_uResumeBaselineBlades    = 0u;   // generated on Dawnmere (> 0 expected)
	u_int       g_uResumeAfterClearBlades  = 0u;   // after the direct singleton clear (0 expected)
	bool        g_bResumeRegenReturned     = false;
	u_int       g_uResumeAfterRegenBlades  = 0u;   // after the restore seam (== baseline expected)

	void FailResume(const char* szReason)
	{
		g_szResumeFailure = szReason;
		g_bResumeFailed = true;
		g_eResumePhase = ResumePhase::Done;
	}
}

static void Setup_ZMTerrainGrassResumeRegen()
{
	g_eResumePhase            = ResumePhase::Done;
	g_iResumePhaseFrames      = 0;
	g_bResumeActive           = false;
	g_bResumeFailed           = false;
	g_szResumeFailure         = "test did not reach verification";
	g_uResumeBaselineBlades   = 0u;
	g_uResumeAfterClearBlades = 0u;
	g_bResumeRegenReturned    = false;
	g_uResumeAfterRegenBlades = 0u;

	g_bResumePrereqsPresent = RequiredGrassAssetsPresent();

	// Guard FIRST -- RequestSkip bypasses Verify, so no fixed-dt / scene state is
	// installed until the bake is known present.
	if (!g_bResumePrereqsPresent)
	{
		Zenith_AutomatedTestRunner::RequestSkip(
			"Dawnmere scene/terrain bake is absent or incomplete");
		return;
	}

	Zenith_InputSimulator::ResetAllInputState();
	Zenith_InputSimulator::SetFixedDt(fFIXED_DT);

	g_xEngine.Scenes().LoadSceneByIndex(2, SCENE_LOAD_SINGLE);   // Dawnmere
	g_eResumePhase = ResumePhase::AwaitGrass;
	g_bResumeActive = true;
}

static bool Step_ZMTerrainGrassResumeRegen(int)
{
	if (!g_bResumeActive || g_bResumeFailed || g_eResumePhase == ResumePhase::Done)
	{
		return false;
	}

	++g_iResumePhaseFrames;
	switch (g_eResumePhase)
	{
	case ResumePhase::AwaitGrass:
		if (g_xEngine.Grass().GetGeneratedInstanceCount() > 0u)
		{
			g_uResumeBaselineBlades = g_xEngine.Grass().GetGeneratedInstanceCount();

			// Stand in for battle entry: clear the engine-owned singleton directly.
			// The component's m_bGrassApplied latch survives this, which is exactly
			// why its OnUpdate will NOT put the blades back on its own.
			g_xEngine.Grass().ClearSceneData();

			g_eResumePhase = ResumePhase::ConfirmCleared;
			g_iResumePhaseFrames = 0;
			return true;
		}
		if (g_iResumePhaseFrames > iRESUME_GRASS_DEADLINE)
		{
			FailResume("Dawnmere never generated any grass instances within the deadline");
			return false;
		}
		return true;

	case ResumePhase::ConfirmCleared:
		// Capture the cleared count BEFORE driving the restore, so a restore that
		// silently never ran cannot be mistaken for a clear that never landed.
		g_uResumeAfterClearBlades = g_xEngine.Grass().GetGeneratedInstanceCount();

		// Drive the seam across every LOADED scene (not just the active one): the
		// battle path this stands in for leaves the overworld loaded-but-paused
		// behind an additive Battle scene, so the resume must reach it there.
		g_xEngine.Scenes().QueryAllScenes<ZM_TerrainGrass>().ForEach(
			[](Zenith_EntityID, ZM_TerrainGrass& xGrass)
			{
				// RegenerateForSceneResume() FIRST -- an accumulating || must never
				// short-circuit away a later instance's restore.
				g_bResumeRegenReturned =
					xGrass.RegenerateForSceneResume() || g_bResumeRegenReturned;
			});

		g_eResumePhase = ResumePhase::Capture;
		g_iResumePhaseFrames = 0;
		return true;

	case ResumePhase::Capture:
		g_uResumeAfterRegenBlades = g_xEngine.Grass().GetGeneratedInstanceCount();
		g_eResumePhase = ResumePhase::Done;
		return false;

	case ResumePhase::Done:
		return false;
	}
	return false;
}

static bool Verify_ZMTerrainGrassResumeRegen()
{
	bool bPassed = true;

	if (g_bResumeActive)
	{
		// Log EVERY captured value on one line so a failure is fully localisable
		// from the log alone.
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_TerrainGrassResumeRegen] captured: baselineBlades=%u (want > 0) "
			"afterClearBlades=%u (want 0) regenReturned=%s (want true) "
			"afterRegenBlades=%u (want %u)",
			g_uResumeBaselineBlades,
			g_uResumeAfterClearBlades,
			g_bResumeRegenReturned ? "true" : "false",
			g_uResumeAfterRegenBlades,
			g_uResumeBaselineBlades);

		if (g_bResumeFailed)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_TerrainGrassResumeRegen] %s", g_szResumeFailure);
			bPassed = false;
		}
		if (g_uResumeBaselineBlades == 0u)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_TerrainGrassResumeRegen] Dawnmere produced zero grass instances "
				"(cannot prove the restore)");
			bPassed = false;
		}
		if (g_uResumeAfterClearBlades != 0u)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_TerrainGrassResumeRegen] ClearSceneData must drop every blade, but "
				"%u remained", g_uResumeAfterClearBlades);
			bPassed = false;
		}
		if (!g_bResumeRegenReturned)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_TerrainGrassResumeRegen] no ZM_TerrainGrass instance reported a "
				"successful RegenerateForSceneResume()");
			bPassed = false;
		}
		if (g_uResumeAfterRegenBlades != g_uResumeBaselineBlades)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_TerrainGrassResumeRegen] resume must restore the same deterministic "
				"blade count: got %u, expected %u",
				g_uResumeAfterRegenBlades, g_uResumeBaselineBlades);
			bPassed = false;
		}
	}

	// Restore FrontEnd + clear the fixed timestep on EVERY path (all guarded).
	Zenith_InputSimulator::ClearFixedDt();
	if (g_bResumeActive)
	{
		g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);   // FrontEnd
	}
	Zenith_InputSimulator::ResetAllInputState();
	g_bResumeActive = false;

	return bPassed || !g_bResumePrereqsPresent;
}

static const Zenith_AutomatedTest g_xZMTerrainGrassResumeRegenTest = {
	"ZM_TerrainGrassResumeRegen_Test",
	&Setup_ZMTerrainGrassResumeRegen,
	&Step_ZMTerrainGrassResumeRegen,
	&Verify_ZMTerrainGrassResumeRegen,
	/* maxFrames */ 600,
	true /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMTerrainGrassResumeRegenTest);

#endif // ZENITH_INPUT_SIMULATOR
