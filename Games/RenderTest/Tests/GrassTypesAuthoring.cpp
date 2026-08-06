#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR
#ifdef ZENITH_TOOLS

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"

#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_GrassTypeTableAsset.h"
#include "Editor/Zenith_EditorAutomation.h"
#include "Editor/TerrainEditor/Zenith_TerrainEditor.h"
#include "Flux/Vegetation/Flux_GrassImpl.h"

#include "Collections/Zenith_Vector.h"

#include <cmath>
#include <filesystem>
#include <fstream>

// ============================================================================
// GrassTypesAuthoring -- windowed proof that the GRASS_TYPES_* automation family
// authors the grass type table end to end, exactly as the terrain editor panel's
// buttons do: both drive the SAME Zenith_TerrainEditor working copy and the same
// four verbs, so this test covers the panel's behaviour without simulating clicks.
//
// Windowed rather than headless for one reason: Apply/Save re-place the grass, and
// a Null build deliberately skips the grass rebuild (the maps are GPU textures --
// see Zenith/Null/CLAUDE.md). Headless would therefore assert a path that never ran.
//
// The three observable stages are asserted separately so a break localises:
//   * the working table       -- the steps reached Zenith_TerrainEditor,
//   * g_xEngine.Grass()       -- Save applied it to the running renderer,
//   * the .zdata on disk      -- Save wrote a file the registry can read back.
//
// RESIDUE IS THE HAZARD HERE. game:Vegetation/GrassTypes.zdata is exactly the file
// Flux_GrassImpl::LoadAuthoredTypeTable boot-loads, so a leaked one would silently
// re-skin every later RenderTest boot with this test's bright red type. Teardown
// therefore drops the registry's cache entry, deletes the file (restoring any
// pre-existing bytes) and puts the engine's table back verbatim -- and Teardown
// runs even when Step times out or Verify fails.
// ============================================================================

namespace
{
	// Distinctive on purpose: nothing in the built-in set (Meadow / Tall / Dry /
	// Flowers) is red, two metres tall, or named this.
	constexpr int   iTEST_TYPE_INDEX  = 4;
	constexpr int   iTEST_TYPE_COUNT  = 5;
	const char* const szTEST_TYPE_NAME = "TestReeds";
	constexpr float fTEST_HEIGHT_MAX  = 2.0f;
	constexpr float fTEST_BASE_R      = 0.95f;
	constexpr float fTEST_BASE_G      = 0.05f;
	constexpr float fTEST_BASE_B      = 0.05f;

	// The gather stages the type block and the placement CS re-runs; a couple of
	// frames after Apply is enough for the new table to be the one on screen.
	constexpr int iSETTLE_FRAMES = 4;

	// Captured in Setup, restored in Teardown. The engine table is global state no
	// scene owns, so restoring the DEFAULTS would be wrong for a game that ships an
	// authored file -- the captured value is right in both cases.
	Flux_GrassTypeTable g_xEngineTableBefore;
	// A game that DOES ship an authored table must come out of this test byte
	// identical, so the pre-existing file is captured rather than merely noted.
	Zenith_Vector<u_int8> g_xFileBytesBefore;
	bool  g_bFileExistedBefore = false;
	bool  g_bSaveReported      = false;
	bool  g_bFileExistsAfter   = false;
	bool  g_bReloadedOk        = false;
	u_int g_uEngineCount       = 0u;
	u_int g_uWorkingCount      = 0u;
	float g_fEngineHeightMax   = 0.0f;
	float g_fEngineBaseRed     = 0.0f;
	float g_fReloadedHeightMax = 0.0f;
	std::string g_strEngineName;
	std::string g_strReloadedName;

	std::filesystem::path TypeTablePath()
	{
		return std::filesystem::path(Zenith_AssetRegistry::ResolvePath(szZENITH_GRASS_TYPE_TABLE_ASSET_PATH));
	}

	bool TypeTableFileExists()
	{
		std::error_code xEC;
		return std::filesystem::exists(TypeTablePath(), xEC) && !xEC;
	}

	void Setup_GrassTypesAuthoring()
	{
		g_xEngineTableBefore  = g_xEngine.Grass().GetTypeTable();
		g_bFileExistedBefore  = TypeTableFileExists();
		g_xFileBytesBefore.Clear();
		if (g_bFileExistedBefore)
		{
			std::error_code xEC;
			const uintmax_t ulSize = std::filesystem::file_size(TypeTablePath(), xEC);
			if (!xEC && ulSize > 0)
			{
				g_xFileBytesBefore.Resize(static_cast<u_int>(ulSize), 0u);
				std::ifstream xIn(TypeTablePath(), std::ios::binary);
				xIn.read(reinterpret_cast<char*>(g_xFileBytesBefore.GetDataPointer()),
					static_cast<std::streamsize>(ulSize));
			}
		}
		g_bSaveReported       = false;
		g_bFileExistsAfter    = false;
		g_bReloadedOk         = false;
		g_uEngineCount        = 0u;
		g_uWorkingCount       = 0u;
		g_fEngineHeightMax    = 0.0f;
		g_fEngineBaseRed      = 0.0f;
		g_fReloadedHeightMax  = 0.0f;
		g_strEngineName.clear();
		g_strReloadedName.clear();
	}

	bool Step_GrassTypesAuthoring(int iFrame)
	{
		if (iFrame == 0)
		{
			// The ACTION path, drained directly. A local queue is a complete
			// authoring session -- the same object games enqueue into at boot --
			// so this exercises AddStep packing, the router's range check and
			// ExecuteGrassTypeAction, not a shortcut past them.
			Zenith_EditorAutomation xAuto;
			xAuto.AddStep_GrassTypesCreate();
			xAuto.AddStep_GrassTypesSetCount(iTEST_TYPE_COUNT);
			xAuto.AddStep_GrassTypesSetName(iTEST_TYPE_INDEX, szTEST_TYPE_NAME);
			xAuto.AddStep_GrassTypesSetParamFloat(iTEST_TYPE_INDEX, "HeightMax", fTEST_HEIGHT_MAX);
			xAuto.AddStep_GrassTypesSetParamColor(iTEST_TYPE_INDEX, "BaseColour",
				fTEST_BASE_R, fTEST_BASE_G, fTEST_BASE_B);
			xAuto.AddStep_GrassTypesSave();

			xAuto.Begin();
			while (!xAuto.IsComplete())
			{
				xAuto.ExecuteNextStep();
			}

			// The working copy is what the panel would be showing at this point.
			g_uWorkingCount = g_xEngine.TerrainEditor().GrassTypes().GetCount();
			g_bSaveReported = true;
			return true;
		}

		if (iFrame < iSETTLE_FRAMES)
		{
			return true;
		}

		// Sampled ONCE, after the gather has had the new table for a frame.
		const Flux_GrassTypeTable& xEngine = g_xEngine.Grass().GetTypeTable();
		g_uEngineCount     = xEngine.GetCount();
		g_strEngineName    = xEngine.GetName(static_cast<u_int>(iTEST_TYPE_INDEX));
		g_fEngineHeightMax = xEngine.Get(static_cast<u_int>(iTEST_TYPE_INDEX)).m_fHeightMax;
		g_fEngineBaseRed   = xEngine.Get(static_cast<u_int>(iTEST_TYPE_INDEX)).m_xBaseColour.x;
		g_bFileExistsAfter = TypeTableFileExists();

		// Reading it back through the REAL asset path is what proves the file is
		// loadable rather than merely present: a wrong envelope or a stale table
		// version would still leave bytes on disk.
		const Zenith_GrassTypeTableAsset* pxLoaded =
			Zenith_AssetRegistry::GetView<Zenith_GrassTypeTableAsset>(szZENITH_GRASS_TYPE_TABLE_ASSET_PATH);
		if (pxLoaded != nullptr && pxLoaded->LoadedOk())
		{
			g_bReloadedOk        = true;
			g_strReloadedName    = pxLoaded->GetTable().GetName(static_cast<u_int>(iTEST_TYPE_INDEX));
			g_fReloadedHeightMax = pxLoaded->GetTable().Get(static_cast<u_int>(iTEST_TYPE_INDEX)).m_fHeightMax;
		}
		return false;
	}

	bool Verify_GrassTypesAuthoring()
	{
		bool bPass = true;

		Zenith_Log(LOG_CATEGORY_TERRAIN,
			"[GrassTypesAuthoring] working=%u engine=%u name='%s' heightMax=%.3f baseR=%.3f "
			"file=%d reloaded=%d reloadedName='%s' reloadedHeightMax=%.3f",
			g_uWorkingCount, g_uEngineCount, g_strEngineName.c_str(), g_fEngineHeightMax, g_fEngineBaseRed,
			g_bFileExistsAfter ? 1 : 0, g_bReloadedOk ? 1 : 0,
			g_strReloadedName.c_str(), g_fReloadedHeightMax);

		if (!g_bSaveReported)
		{
			Zenith_Error(LOG_CATEGORY_TERRAIN, "[GrassTypesAuthoring] the authoring queue never drained");
			return false;
		}
		if (g_uWorkingCount != static_cast<u_int>(iTEST_TYPE_COUNT))
		{
			Zenith_Error(LOG_CATEGORY_TERRAIN,
				"[GrassTypesAuthoring] the steps did not reach the terrain editor working table (count %u, want %d)",
				g_uWorkingCount, iTEST_TYPE_COUNT);
			bPass = false;
		}
		if (g_uEngineCount != static_cast<u_int>(iTEST_TYPE_COUNT))
		{
			Zenith_Error(LOG_CATEGORY_TERRAIN,
				"[GrassTypesAuthoring] Save did not apply to the running renderer (count %u, want %d)",
				g_uEngineCount, iTEST_TYPE_COUNT);
			bPass = false;
		}
		if (g_strEngineName != szTEST_TYPE_NAME)
		{
			Zenith_Error(LOG_CATEGORY_TERRAIN,
				"[GrassTypesAuthoring] the renamed type did not reach the renderer ('%s', want '%s')",
				g_strEngineName.c_str(), szTEST_TYPE_NAME);
			bPass = false;
		}
		if (fabsf(g_fEngineHeightMax - fTEST_HEIGHT_MAX) > 0.001f)
		{
			Zenith_Error(LOG_CATEGORY_TERRAIN,
				"[GrassTypesAuthoring] HeightMax did not reach the renderer (%.3f, want %.3f)",
				g_fEngineHeightMax, fTEST_HEIGHT_MAX);
			bPass = false;
		}
		if (fabsf(g_fEngineBaseRed - fTEST_BASE_R) > 0.001f)
		{
			Zenith_Error(LOG_CATEGORY_TERRAIN,
				"[GrassTypesAuthoring] BaseColour did not reach the renderer (red %.3f, want %.3f)",
				g_fEngineBaseRed, fTEST_BASE_R);
			bPass = false;
		}
		if (!g_bFileExistsAfter)
		{
			Zenith_Error(LOG_CATEGORY_TERRAIN,
				"[GrassTypesAuthoring] GrassTypesSave wrote no file at '%s'",
				TypeTablePath().string().c_str());
			bPass = false;
		}
		if (!g_bReloadedOk)
		{
			Zenith_Error(LOG_CATEGORY_TERRAIN,
				"[GrassTypesAuthoring] the written .zdata did not load back through the asset registry");
			bPass = false;
		}
		else if (g_strReloadedName != szTEST_TYPE_NAME ||
			fabsf(g_fReloadedHeightMax - fTEST_HEIGHT_MAX) > 0.001f)
		{
			Zenith_Error(LOG_CATEGORY_TERRAIN,
				"[GrassTypesAuthoring] the round trip lost content (name '%s' heightMax %.3f)",
				g_strReloadedName.c_str(), g_fReloadedHeightMax);
			bPass = false;
		}
		return bPass;
	}

	// Runs even on a timeout or a failed Verify, which is the whole point: the
	// authored file is the one Flux_GrassImpl boot-loads, so leaving it behind
	// would re-skin every later boot of this game.
	void Teardown_GrassTypesAuthoring()
	{
		// Drop the cache entry first -- Step's GetView registered one keyed on the
		// path whose file is about to disappear.
		Zenith_AssetRegistry::ForceUnload(szZENITH_GRASS_TYPE_TABLE_ASSET_PATH);

		std::error_code xEC;
		if (g_bFileExistedBefore && g_xFileBytesBefore.GetSize() > 0)
		{
			std::ofstream xOut(TypeTablePath(), std::ios::binary);
			xOut.write(reinterpret_cast<const char*>(g_xFileBytesBefore.GetDataPointer()),
				static_cast<std::streamsize>(g_xFileBytesBefore.GetSize()));
		}
		else
		{
			std::filesystem::remove(TypeTablePath(), xEC);
		}

		// Panel-equivalent restore, through the SAME verbs the panel buttons call.
		// The captured table goes back rather than Reset's built-ins: identical for
		// a game that ships no file, and correct for one that does. It must go
		// through Apply, not a bare SetTypeTable — Save re-placed the grass, so the
		// blades standing in the world still carry this test's bright red type
		// until something re-places them again.
		Zenith_TerrainEditor& xTerrainEditor = g_xEngine.TerrainEditor();
		xTerrainEditor.GrassTypes() = g_xEngineTableBefore;
		xTerrainEditor.GrassTypes_Apply();
	}

	const Zenith_AutomatedTest g_xGrassTypesAuthoring = {
		"GrassTypesAuthoring",
		&Setup_GrassTypesAuthoring,
		&Step_GrassTypesAuthoring,
		&Verify_GrassTypesAuthoring,
		/* maxFrames */ 60,
		true /* m_bRequiresGraphics */,
		false /* m_bManualOnly */,
		&Teardown_GrassTypesAuthoring,
	};
	ZENITH_AUTOMATED_TEST_REGISTER(g_xGrassTypesAuthoring);
}

#endif // ZENITH_TOOLS
#endif // ZENITH_INPUT_SIMULATOR
