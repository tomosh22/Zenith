#include "Core/Zenith_TestFramework.h"
#include "Flux/Vegetation/Flux_GrassImpl.h"
#include "Core/Zenith_Engine.h"

// ============================================================================
// Flux_Grass scene-reset unit tests (S5 item 2, engine E5).
//
// Lock the FULL scene-state clear that Flux_GrassImpl::Reset() performs so grass
// never leaks between scenes. A full Reset() must drop the chunk list, the CPU
// instance array, the instances-generated / instances-uploaded flags, and the
// copied painted density map (plus the visible/active counters). The production
// single-load render-reset hook calls g_xEngine.Grass().Reset(), so these tests
// pin exactly that behaviour at the API surface.
//
// This file is textually included at the bottom of Flux_Grass.cpp (under
// ZENITH_TESTING), so it sees Flux_GrassImpl's internals directly.
//
// HEADLESS-SAFE — the tests touch CPU-side state ONLY:
//   * They drive the live, process-wide subsystem via g_xEngine.Grass() rather
//     than constructing a Flux_GrassImpl on the stack (its GPU pipeline/shader
//     members' destructors reach g_xEngine.FluxBackend().GetDevice()).
//   * Scene state is populated with a synthetic density map (SetDensityMap copies
//     on the CPU) plus hand-built CPU instance/chunk entries and the flags set
//     directly. GenerateFromTerrain / UploadInstanceData (the GPU-upload paths)
//     are NEVER called, so no device work happens.
//   * Reset() itself is CPU-only (clears the vectors + flags + density map; the
//     engine-owned instance buffer stays allocated), so it is safe to call here.
// Each test Reset()s the singleton FIRST so a prior test's — or a prior boot
// phase's — leftovers never taint setup, and again at the end to leave it clean.
// ============================================================================

namespace
{
	constexpr u_int kGrassTestBladeCount = 3u;
	constexpr u_int kGrassTestChunkCount = 1u;

	// Fill xGrass with non-empty, GPU-free scene state that a full Reset() must
	// discard. Returns the number of CPU blade instances pushed so callers can
	// assert exact per-scene counts (no accumulation across repeated setups).
	u_int GrassTest_PopulateSceneState(Flux_GrassImpl& xGrass)
	{
		// Synthetic painted density map (data is COPIED into the impl; pure CPU).
		const float afDensity[4] = { 1.0f, 0.5f, 0.25f, 0.75f };
		xGrass.SetDensityMap(afDensity, 2u, 2u, 64.0f);

		// Hand-built CPU blade instances — bypasses GenerateFromTerrain, which
		// would upload to the GPU.
		for (u_int u = 0; u < kGrassTestBladeCount; ++u)
		{
			GrassBladeInstance xBlade;
			xBlade.m_xPosition  = Zenith_Maths::Vector3(static_cast<float>(u), 0.0f, 0.0f);
			xBlade.m_fRotation  = 0.5f;
			xBlade.m_fHeight    = 0.6f;
			xBlade.m_fWidth     = 0.03f;
			xBlade.m_fBend      = 0.1f;
			xBlade.m_uColorTint = 0xFF00FF00u;
			xGrass.m_axAllInstances.PushBack(xBlade);
		}

		// One contiguous chunk covering those blades.
		GrassChunk xChunk;
		xChunk.m_xCenter         = Zenith_Maths::Vector3(0.0f);
		xChunk.m_fRadius         = 32.0f;
		xChunk.m_uInstanceOffset = 0u;
		xChunk.m_uInstanceCount  = kGrassTestBladeCount;
		xChunk.m_uLOD            = 0u;
		xChunk.m_bVisible        = true;
		xGrass.m_axChunks.PushBack(xChunk);

		// Flags + visibility counters set directly — mirrors post-generate/upload
		// state without touching the GPU.
		xGrass.m_bInstancesGenerated = true;
		xGrass.m_bInstancesUploaded  = true;
		xGrass.m_uVisibleBladeCount  = kGrassTestBladeCount;
		xGrass.m_uActiveChunkCount   = kGrassTestChunkCount;

		return kGrassTestBladeCount;
	}

	// Assert xGrass is in the fully-zeroed post-Reset state (via the real public
	// accessors). szWhen pinpoints which test / phase failed.
	void GrassTest_AssertFullyCleared(Flux_GrassImpl& xGrass, const char* szWhen)
	{
		ZENITH_ASSERT_EQ(xGrass.GetGeneratedInstanceCount(), 0u,
			"%s: Reset() must drop every generated CPU blade instance", szWhen);
		ZENITH_ASSERT_FALSE(xGrass.HasGeneratedInstances(),
			"%s: Reset() must clear the instances-generated flag", szWhen);
		ZENITH_ASSERT_FALSE(xGrass.HasUploadedInstances(),
			"%s: Reset() must clear the instances-uploaded flag", szWhen);
		ZENITH_ASSERT_FALSE(xGrass.HasDensityMap(),
			"%s: Reset() must discard the copied density map", szWhen);
		ZENITH_ASSERT_EQ(xGrass.GetChunkCount(), 0u,
			"%s: Reset() must drop every grass chunk", szWhen);
		ZENITH_ASSERT_EQ(xGrass.GetVisibleBladeCount(), 0u,
			"%s: Reset() must zero the visible-blade counter", szWhen);
		ZENITH_ASSERT_EQ(xGrass.GetActiveChunkCount(), 0u,
			"%s: Reset() must zero the active-chunk counter", szWhen);
	}
}

ZENITH_TEST(Flux_Grass, Reset_ClearsAllSceneData)
{
	Flux_GrassImpl& xGrass = g_xEngine.Grass();
	xGrass.Reset();   // clean baseline (defensive against any prior state)

	const u_int uBlades = GrassTest_PopulateSceneState(xGrass);

	// Sanity: the setup really did leave non-empty scene state, so the clear
	// assertions below are not vacuously satisfied.
	ZENITH_ASSERT_EQ(xGrass.GetGeneratedInstanceCount(), uBlades,
		"setup must leave the CPU instance array populated");
	ZENITH_ASSERT_TRUE(xGrass.HasGeneratedInstances(),
		"setup must mark instances generated");
	ZENITH_ASSERT_TRUE(xGrass.HasUploadedInstances(),
		"setup must mark instances uploaded");
	ZENITH_ASSERT_TRUE(xGrass.HasDensityMap(),
		"setup must install a density map");
	ZENITH_ASSERT_EQ(xGrass.GetChunkCount(), kGrassTestChunkCount,
		"setup must leave a grass chunk");

	xGrass.Reset();

	GrassTest_AssertFullyCleared(xGrass, "Reset_ClearsAllSceneData");
}

ZENITH_TEST(Flux_Grass, Reset_IsIdempotent)
{
	Flux_GrassImpl& xGrass = g_xEngine.Grass();
	xGrass.Reset();

	GrassTest_PopulateSceneState(xGrass);

	// Two Resets back-to-back must be safe (no crash, no counter underflow) and
	// leave the same fully-zeroed state as a single Reset.
	xGrass.Reset();
	xGrass.Reset();

	GrassTest_AssertFullyCleared(xGrass, "Reset_IsIdempotent (second Reset)");
}

ZENITH_TEST(Flux_Grass, Reset_NoAccumulationAcrossSetup)
{
	Flux_GrassImpl& xGrass = g_xEngine.Grass();
	xGrass.Reset();

	// First "scene".
	const u_int uFirst = GrassTest_PopulateSceneState(xGrass);
	ZENITH_ASSERT_EQ(xGrass.GetGeneratedInstanceCount(), uFirst,
		"first scene's blades must all be present before reset");
	xGrass.Reset();
	GrassTest_AssertFullyCleared(xGrass, "Reset_NoAccumulationAcrossSetup (after scene 1)");

	// Second "scene": because Reset() fully drained scene 1, the identical setup
	// must yield the identical count -- a new scene's grass state does NOT pile on
	// top of the prior scene's (mirrors the per-scene instance-count assertion).
	const u_int uSecond = GrassTest_PopulateSceneState(xGrass);
	ZENITH_ASSERT_EQ(uSecond, uFirst,
		"a second scene's setup must not accumulate on top of the first");
	ZENITH_ASSERT_EQ(xGrass.GetGeneratedInstanceCount(), uFirst,
		"instance count after the second setup must equal a single scene's, not 2x");
	ZENITH_ASSERT_EQ(xGrass.GetChunkCount(), kGrassTestChunkCount,
		"chunk count after the second setup must equal a single scene's, not 2x");

	xGrass.Reset();   // leave the shared singleton clean for subsequent tests
	GrassTest_AssertFullyCleared(xGrass, "Reset_NoAccumulationAcrossSetup (after scene 2)");
}
