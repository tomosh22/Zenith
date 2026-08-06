#include "Core/Zenith_TestFramework.h"
#include "Flux/Vegetation/Flux_GrassImpl.h"
#include "Flux/Vegetation/Flux_GrassTypes.h"
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

// ============================================================================
// Flux_GrassTypes — the pure GPU-driven grass definitions.
//
// Everything below exercises Flux/Vegetation/Flux_GrassTypes.h ONLY: plain
// structs and free functions, no engine singleton, no device, no file IO. That
// is the point of the header — the placement compute shader's decisions have to
// be reproducible on the CPU or nothing about them can be pinned headlessly.
//
// Two classes of pin live here and they are not equally strong:
//
//   * The INTEGER hash vectors are exact and cross-language. Common/Noise.slang
//     is a hand-written twin of Zenith_Noise.h, and every placement decision is
//     keyed off HashCoords, so these constants are the contract that says a
//     given world position grows the same blade on the CPU, on the GPU, today
//     and after a shader rewrite. They were observed once from the real
//     functions and frozen; a change to them is a change to every blade in
//     every world, not a test failure to paper over.
//
//   * Everything float-valued is pinned by PROPERTY (determinism, ordering,
//     range, monotonicity), never by cross-language bit equality — floor,
//     FMA contraction and fast-math modes all differ per target.
// ============================================================================

namespace
{
	Flux_GrassTileSelectParams GrassTypesTest_DefaultParams()
	{
		// Camera at the centre of HI tile (0,0). No frusta (culling disabled),
		// no extents (unbounded map), no height grid (flat fallback band).
		Flux_GrassTileSelectParams xParams;
		xParams.m_xCameraPos = Zenith_Maths::Vector3(8.0f, 0.0f, 8.0f);
		return xParams;
	}

	// Index of (eLOD, iTileX, iTileZ) in the output, or m_uCount when absent.
	u_int GrassTypesTest_FindTile(const Flux_GrassTileList& xList, Flux_GrassTileLOD eLOD, int iTileX, int iTileZ)
	{
		for (u_int u = 0; u < xList.m_uCount; u++)
		{
			const Flux_GrassTile& xTile = xList.Get(u);
			if (xTile.m_eLOD == eLOD && xTile.m_iTileX == iTileX && xTile.m_iTileZ == iTileZ)
			{
				return u;
			}
		}
		return xList.m_uCount;
	}

	bool GrassTypesTest_TilesIdentical(const Flux_GrassTile& xA, const Flux_GrassTile& xB)
	{
		return xA.m_eLOD == xB.m_eLOD
			&& xA.m_iTileX == xB.m_iTileX
			&& xA.m_iTileZ == xB.m_iTileZ
			&& xA.m_fSize == xB.m_fSize
			&& xA.m_fWorldMinX == xB.m_fWorldMinX
			&& xA.m_fWorldMinZ == xB.m_fWorldMinZ
			&& xA.m_fDistanceSq == xB.m_fDistanceSq;
	}

	bool GrassTypesTest_ListsIdentical(const Flux_GrassTileList& xA, const Flux_GrassTileList& xB)
	{
		if (xA.m_uCount != xB.m_uCount || xA.m_uConsidered != xB.m_uConsidered || xA.m_bOverflowed != xB.m_bOverflowed)
		{
			return false;
		}
		for (u_int u = 0; u < xA.m_uCount; u++)
		{
			if (!GrassTypesTest_TilesIdentical(xA.Get(u), xB.Get(u)))
			{
				return false;
			}
		}
		return true;
	}
}

ZENITH_TEST(FluxGrassTypes, HashMirrorPinnedVectors)
{
	// Frozen outputs of the real functions. Common/Noise.slang must reproduce
	// every one of these bit for bit — that is the whole claim the GPU mirror
	// makes, and it is the only part of the noise family where bit equality is
	// claimed at all.
	ZENITH_ASSERT_EQ(Zenith_TerrainNoise::HashUInt(0u), 0x00000000u,
		"HashUInt(0) — the finalizer maps zero to zero; it is a real vector, not a missing one");
	ZENITH_ASSERT_EQ(Zenith_TerrainNoise::HashUInt(1u), 0x688990C0u, "HashUInt(1)");
	ZENITH_ASSERT_EQ(Zenith_TerrainNoise::HashUInt(1013u), 0x5EB12970u, "HashUInt(1013) — the FBM octave stride");
	ZENITH_ASSERT_EQ(Zenith_TerrainNoise::HashUInt(0xFFFFFFFFu), 0x6768824Au, "HashUInt(0xFFFFFFFF)");
	ZENITH_ASSERT_EQ(Zenith_TerrainNoise::HashUInt(0x9E3779B9u), 0x01FCE552u, "HashUInt(golden ratio constant)");

	ZENITH_ASSERT_EQ(Zenith_TerrainNoise::HashCoords(0, 0, 0u), 0x00000000u, "HashCoords(0,0,seed 0)");
	ZENITH_ASSERT_EQ(Zenith_TerrainNoise::HashCoords(1, 0, 1337u), 0x1A3B07EFu, "HashCoords(1,0,1337)");
	ZENITH_ASSERT_EQ(Zenith_TerrainNoise::HashCoords(3, 7, 1337u), 0x903D615Au, "HashCoords(3,7,1337)");
	ZENITH_ASSERT_EQ(Zenith_TerrainNoise::HashCoords(-1, -1, 1337u), 0x213A7FEEu,
		"negative lattice coordinates must survive the int->uint reinterpretation identically on both sides");
	ZENITH_ASSERT_EQ(Zenith_TerrainNoise::HashCoords(12345, -6789, 42u), 0x963B392Du, "HashCoords(12345,-6789,42)");

	// Zero epsilon is the assertion, not a convenience: a 24-bit integer scaled
	// by 2^-24 rounds nowhere, in either language.
	ZENITH_ASSERT_EQ_FLOAT(Zenith_TerrainNoise::HashToFloat01(0u), 0.0f, 0.0f, "HashToFloat01(0)");
	ZENITH_ASSERT_EQ_FLOAT(Zenith_TerrainNoise::HashToFloat01(0x00FFFFFFu), 16777215.0f / 16777216.0f, 0.0f,
		"HashToFloat01 must reach, but never touch, 1.0");
	ZENITH_ASSERT_EQ_FLOAT(Zenith_TerrainNoise::HashToFloat01(0x01000000u), 0.0f, 0.0f,
		"only the low 24 bits may reach the mantissa");
	ZENITH_ASSERT_EQ_FLOAT(Zenith_TerrainNoise::HashToFloat01(0x688990C0u), 9015488.0f / 16777216.0f, 0.0f,
		"HashToFloat01(HashUInt(1))");
	ZENITH_ASSERT_EQ_FLOAT(Zenith_TerrainNoise::HashToFloat01(0x903D615Au), 4022618.0f / 16777216.0f, 0.0f,
		"HashToFloat01(HashCoords(3,7,1337))");
}

ZENITH_TEST(FluxGrassTypes, HashIsAPureFunction)
{
	// Blades are regenerated every frame from nothing but their lattice
	// coordinates. If any of these were stateful, a blade would change identity
	// between frames and flicker under TAA.
	for (int iX = -3; iX <= 3; iX++)
	{
		for (int iZ = -3; iZ <= 3; iZ++)
		{
			const u_int uFirst = Zenith_TerrainNoise::HashCoords(iX, iZ, 1337u);
			const u_int uSecond = Zenith_TerrainNoise::HashCoords(iX, iZ, 1337u);
			ZENITH_ASSERT_EQ(uFirst, uSecond, "HashCoords(%d,%d) must be a pure function", iX, iZ);
			ZENITH_ASSERT_EQ_FLOAT(Zenith_TerrainNoise::HashToFloat01(uFirst),
				Zenith_TerrainNoise::HashToFloat01(uSecond), 0.0f, "HashToFloat01 must be a pure function");

			const float fSampleX = static_cast<float>(iX) * 0.37f;
			const float fSampleZ = static_cast<float>(iZ) * 0.37f;
			ZENITH_ASSERT_EQ_FLOAT(Zenith_TerrainNoise::ValueNoise(fSampleX, fSampleZ, 1337u),
				Zenith_TerrainNoise::ValueNoise(fSampleX, fSampleZ, 1337u), 0.0f,
				"ValueNoise(%f,%f) must be a pure function", fSampleX, fSampleZ);
		}
	}

	// ... and the seed must actually be an input, or "seeded" is a lie.
	ZENITH_ASSERT_NE(Zenith_TerrainNoise::HashCoords(3, 7, 1337u), Zenith_TerrainNoise::HashCoords(3, 7, 1338u),
		"adjacent seeds must produce different lattice hashes");
}

ZENITH_TEST(FluxGrassTypes, GpuRecordLayoutIsPinned)
{
	// The header's static_asserts are the real gate — they fail the BUILD. These
	// restate them so a size drift also names the record in the test log.
	ZENITH_ASSERT_EQ(sizeof(Flux_GrassBladeInstance), static_cast<size_t>(64),
		"the blade record is 16 x 32-bit slots; the shader-side twin is hand-written");
	ZENITH_ASSERT_EQ(sizeof(Flux_GrassDrawIndexedIndirectArgs), static_cast<size_t>(20),
		"indirect args must match VkDrawIndexedIndirectCommand's five words");
	ZENITH_ASSERT_EQ(sizeof(Flux_GrassTypeParamsGPU), static_cast<size_t>(144),
		"the per-type block is a pinned 144 bytes");
	ZENITH_ASSERT_EQ(sizeof(Flux_GrassTypeParamsGPU) / sizeof(float), static_cast<size_t>(36),
		"144 bytes is exactly 36 32-bit scalars — no padding is hiding in there");

	// Slot placement of the three integer fields. These are the ones the shader
	// reads through asuint(), so a silent reorder would corrupt blade identity
	// while still compiling and still measuring 64 bytes.
	const Flux_GrassBladeInstance xBlade{};
	const char* pcBase = reinterpret_cast<const char*>(&xBlade);
	const size_t uHashOffset = static_cast<size_t>(reinterpret_cast<const char*>(&xBlade.m_uHashBits) - pcBase);
	const size_t uFacingOffset = static_cast<size_t>(reinterpret_cast<const char*>(&xBlade.m_xFacingXZ) - pcBase);
	const size_t uClumpOffset = static_cast<size_t>(reinterpret_cast<const char*>(&xBlade.m_uClumpPacked) - pcBase);
	const size_t uFlagsOffset = static_cast<size_t>(reinterpret_cast<const char*>(&xBlade.m_uTypeFlags) - pcBase);

	ZENITH_ASSERT_EQ(uHashOffset, static_cast<size_t>(12), "the lattice hash must be slot 3, right after the 3-float position");
	ZENITH_ASSERT_EQ(uFacingOffset, static_cast<size_t>(16), "facingXZ must be slots 4-5 — posWS must not be padded to 16 bytes");
	ZENITH_ASSERT_EQ(uClumpOffset, static_cast<size_t>(56), "clumpPacked must be slot 14");
	ZENITH_ASSERT_EQ(uFlagsOffset, static_cast<size_t>(60), "typeFlags must be slot 15 — the last word of the record");

	// The per-type block's packed appearance slots are the fields that let 36
	// scalars carry 40-odd parameters; they must decode to what they claim.
	const Flux_GrassTypeParamsGPU xTypeParams;
	ZENITH_ASSERT_NEAR_VEC3(Flux_GrassUnpackColourRGB(xTypeParams.m_uBaseColourPacked),
		Zenith_Maths::Vector3(0.15f, 0.28f, 0.09f), 1.0f / 255.0f,
		"the packed base colour must round-trip within 8-bit precision");
	ZENITH_ASSERT_NEAR_VEC3(Flux_GrassUnpackColourRGB(xTypeParams.m_uTipColourPacked),
		Zenith_Maths::Vector3(0.42f, 0.55f, 0.18f), 1.0f / 255.0f,
		"the packed tip colour must round-trip within 8-bit precision");
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassUnpackUnorm2x16X(xTypeParams.m_uTranslucencyPacked), 0.15f, 3.0e-5f,
		"translucency base sits in the low half");
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassUnpackUnorm2x16Y(xTypeParams.m_uTranslucencyPacked), 0.65f, 3.0e-5f,
		"translucency tip sits in the high half");
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassUnpackUnorm2x16X(xTypeParams.m_uAOPacked), 0.25f, 3.0e-5f, "AO base");
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassUnpackUnorm2x16Y(xTypeParams.m_uAOPacked), 0.75f, 3.0e-5f, "AO tip release height");
}

ZENITH_TEST(FluxGrassTypes, ClumpPackedRoundTrips)
{
	// unorm16 quantization: one step is 1/65535, so the tolerance is two steps.
	const float afValues[5] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
	for (u_int uA = 0; uA < 5u; uA++)
	{
		for (u_int uB = 0; uB < 5u; uB++)
		{
			const u_int uPacked = Flux_GrassPackClump(afValues[uA], afValues[uB]);
			ZENITH_ASSERT_EQ_FLOAT(Flux_GrassUnpackClumpHash01(uPacked), afValues[uA], 3.0e-5f,
				"clump hash must survive the pack (%f, %f)", afValues[uA], afValues[uB]);
			ZENITH_ASSERT_EQ_FLOAT(Flux_GrassUnpackClumpDist01(uPacked), afValues[uB], 3.0e-5f,
				"clump distance must survive the pack (%f, %f)", afValues[uA], afValues[uB]);
		}
	}

	// The two halves must not bleed into each other.
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassUnpackClumpHash01(Flux_GrassPackClump(0.0f, 1.0f)), 0.0f, 0.0f,
		"a saturated high half must leave the low half at zero");
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassUnpackClumpDist01(Flux_GrassPackClump(1.0f, 0.0f)), 0.0f, 0.0f,
		"a saturated low half must leave the high half at zero");

	// Out-of-range inputs saturate; they must never wrap into the other half.
	ZENITH_ASSERT_EQ(Flux_GrassPackClump(2.0f, -1.0f), 0x0000FFFFu,
		"out-of-range components must clamp, not wrap");
}

ZENITH_TEST(FluxGrassTypes, TypeFlagsRoundTrip)
{
	const u_int auTypes[4] = { 0u, 1u, 127u, 255u };
	const u_int auHashes[4] = { 0u, 1u, 0x1234u, 0xFFFFu };
	for (u_int uT = 0; uT < 4u; uT++)
	{
		for (u_int uH = 0; uH < 4u; uH++)
		{
			for (int iFold = 0; iFold < 2; iFold++)
			{
				for (int iLO = 0; iLO < 2; iLO++)
				{
					const bool bFolded = (iFold != 0);
					const bool bLOMesh = (iLO != 0);
					const u_int uFlags = Flux_GrassPackTypeFlags(auTypes[uT], bFolded, bLOMesh, auHashes[uH]);

					ZENITH_ASSERT_EQ(Flux_GrassTypeFlagsIndex(uFlags), auTypes[uT],
						"type index must round-trip (type %u, hash %u)", auTypes[uT], auHashes[uH]);
					ZENITH_ASSERT_EQ(Flux_GrassTypeFlagsIsFolded(uFlags), bFolded,
						"folded bit must round-trip (type %u)", auTypes[uT]);
					ZENITH_ASSERT_EQ(Flux_GrassTypeFlagsIsLOMesh(uFlags), bLOMesh,
						"LO-mesh bit must round-trip (type %u)", auTypes[uT]);
					ZENITH_ASSERT_EQ(Flux_GrassTypeFlagsFragmentHash(uFlags), auHashes[uH],
						"fragment hash must round-trip (type %u, hash %u)", auTypes[uT], auHashes[uH]);
				}
			}
		}
	}

	// Oversized inputs must truncate to their own field, never corrupt the next.
	const u_int uTruncated = Flux_GrassPackTypeFlags(0x1FFu, false, false, 0x12345u);
	ZENITH_ASSERT_EQ(Flux_GrassTypeFlagsIndex(uTruncated), 0xFFu, "a 9-bit type index must truncate to 8 bits");
	ZENITH_ASSERT_FALSE(Flux_GrassTypeFlagsIsFolded(uTruncated), "a 9-bit type index must not spill into the folded bit");
	ZENITH_ASSERT_FALSE(Flux_GrassTypeFlagsIsLOMesh(uTruncated), "a 9-bit type index must not spill into the LO-mesh bit");
	ZENITH_ASSERT_EQ(Flux_GrassTypeFlagsFragmentHash(uTruncated), 0x2345u, "a 17-bit fragment hash must truncate to 16 bits");

	ZENITH_ASSERT_EQ(Flux_GrassPackTypeFlags(0u, false, false, 0u), 0u,
		"the all-default record must be all zero, so a zeroed buffer decodes to type 0");
}

ZENITH_TEST(FluxGrassTypes, LatticeClassPartitionsParitySpace)
{
	ZENITH_ASSERT_EQ(Flux_GrassLatticeClass(0, 0), 0u, "(even,even) is class 0 — the set the LO lattice reproduces");
	ZENITH_ASSERT_EQ(Flux_GrassLatticeClass(1, 0), 1u, "(odd,even) is class 1");
	ZENITH_ASSERT_EQ(Flux_GrassLatticeClass(0, 1), 2u, "(even,odd) is class 2");
	ZENITH_ASSERT_EQ(Flux_GrassLatticeClass(1, 1), 3u, "(odd,odd) is class 3");

	bool abSeen[4] = { false, false, false, false };
	for (int iX = -4; iX <= 4; iX++)
	{
		for (int iZ = -4; iZ <= 4; iZ++)
		{
			const u_int uClass = Flux_GrassLatticeClass(iX, iZ);
			ZENITH_ASSERT_LT(uClass, 4u, "lattice class must be one of four (%d,%d)", iX, iZ);
			abSeen[uClass] = true;

			// Negative coordinates are the interesting half: the parity has to
			// come out of the two's-complement bit, not out of a division.
			const bool bBothEven = ((iX % 2) == 0) && ((iZ % 2) == 0);
			ZENITH_ASSERT_EQ(uClass == 0u, bBothEven,
				"class 0 must be exactly the (even,even) nodes, negatives included (%d,%d)", iX, iZ);
		}
	}
	for (u_int u = 0; u < 4u; u++)
	{
		ZENITH_ASSERT_TRUE(abSeen[u], "lattice class %u is unreachable — the four classes must partition parity space", u);
	}

	// Class 0 is only the LO survivor set if the LO lattice really is every
	// other HI node on both axes. A different stride would make the HI->LO
	// transition a reshuffle instead of a fade.
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassLatticeStep(Flux_GrassTileLOD::LO),
		Flux_GrassLatticeStep(Flux_GrassTileLOD::HI) * static_cast<float>(Flux_GrassConfig::uLO_LATTICE_STRIDE), 0.0f,
		"the LO lattice step must be exactly the HI step times the stride");
}

ZENITH_TEST(FluxGrassTypes, LatticeFadeBandsAreStaggeredAndSpareClassZero)
{
	const float fR = Flux_GrassConfig::fHI_RADIUS;

	// Class 0 survives into the LO lattice, so fading it would punch holes that
	// nothing fills. It must be flat at every distance, including well past R.
	for (int i = 0; i <= 8; i++)
	{
		const float fDistance = static_cast<float>(i) * 0.25f * fR;
		ZENITH_ASSERT_EQ_FLOAT(Flux_GrassLatticeFade(0u, fDistance, fR), 1.0f, 0.0f,
			"class 0 must never fade (distance %f)", fDistance);
	}

	// The whole transition is contained in [0.70, 0.92] R: nothing starts early,
	// nothing survives past the band.
	for (u_int uClass = 1u; uClass <= 3u; uClass++)
	{
		ZENITH_ASSERT_EQ_FLOAT(Flux_GrassLatticeFade(uClass, 0.69f * fR, fR), 1.0f, 0.0f,
			"class %u must be at full size before the band opens", uClass);
		ZENITH_ASSERT_EQ_FLOAT(Flux_GrassLatticeFade(uClass, 0.93f * fR, fR), 0.0f, 0.0f,
			"class %u must be gone after the band closes", uClass);
	}

	// Staggered, never simultaneous: class 1 leads, class 3 trails, everywhere.
	// Three classes popping together IS the artefact the stagger exists to avoid.
	for (int i = 0; i <= 20; i++)
	{
		const float fDistance = (0.65f + static_cast<float>(i) * 0.015f) * fR;
		const float fClass1 = Flux_GrassLatticeFade(1u, fDistance, fR);
		const float fClass2 = Flux_GrassLatticeFade(2u, fDistance, fR);
		const float fClass3 = Flux_GrassLatticeFade(3u, fDistance, fR);

		ZENITH_ASSERT_LE(fClass1, fClass2, "class 1 must lead class 2 at distance %f", fDistance);
		ZENITH_ASSERT_LE(fClass2, fClass3, "class 2 must lead class 3 at distance %f", fDistance);
		ZENITH_ASSERT_GE(fClass1, 0.0f, "fade must not go negative at distance %f", fDistance);
		ZENITH_ASSERT_LE(fClass3, 1.0f, "fade must not exceed full size at distance %f", fDistance);
	}

	// Monotone in distance — a blade may shrink as it recedes, never regrow.
	for (u_int uClass = 1u; uClass <= 3u; uClass++)
	{
		float fPrevious = 1.0f;
		for (int i = 0; i <= 40; i++)
		{
			const float fValue = Flux_GrassLatticeFade(uClass, (0.60f + static_cast<float>(i) * 0.01f) * fR, fR);
			ZENITH_ASSERT_LE(fValue, fPrevious, "class %u fade must never increase with distance", uClass);
			fPrevious = fValue;
		}
	}
}

ZENITH_TEST(FluxGrassTypes, TileSelectionCoversTheCameraNeighbourhood)
{
	Flux_GrassTileList xList;
	Flux_GrassSelectTiles(GrassTypesTest_DefaultParams(), xList);

	ZENITH_ASSERT_GT(xList.m_uCount, 0u, "the scheduler must select something from a default camera");

	// The camera stands inside HI tile (0,0) — 16 m tiles, camera at (8, 8).
	ZENITH_ASSERT_EQ(GrassTypesTest_FindTile(xList, Flux_GrassTileLOD::HI, 0, 0), 0u,
		"the camera's own HI tile is at distance zero, so it must be selected AND sort first");
	ZENITH_ASSERT_EQ_FLOAT(xList.Get(0).m_fDistanceSq, 0.0f, 0.0f,
		"a tile containing the camera is at distance zero");

	// The four HI neighbours are well inside the 64 m radius.
	ZENITH_ASSERT_LT(GrassTypesTest_FindTile(xList, Flux_GrassTileLOD::HI, 1, 0), xList.m_uCount, "HI (+1, 0) must be selected");
	ZENITH_ASSERT_LT(GrassTypesTest_FindTile(xList, Flux_GrassTileLOD::HI, -1, 0), xList.m_uCount, "HI (-1, 0) must be selected");
	ZENITH_ASSERT_LT(GrassTypesTest_FindTile(xList, Flux_GrassTileLOD::HI, 0, 1), xList.m_uCount, "HI (0, +1) must be selected");
	ZENITH_ASSERT_LT(GrassTypesTest_FindTile(xList, Flux_GrassTileLOD::HI, 0, -1), xList.m_uCount, "HI (0, -1) must be selected");

	// ... and HI stops at its radius: tile (20,20) starts 320 m out.
	ZENITH_ASSERT_EQ(GrassTypesTest_FindTile(xList, Flux_GrassTileLOD::HI, 20, 20), xList.m_uCount,
		"HI must not reach past its 64 m radius");

	for (u_int u = 0; u < xList.m_uCount; u++)
	{
		const Flux_GrassTile& xTile = xList.Get(u);
		const float fExpectedSize = Flux_GrassTileSize(xTile.m_eLOD);
		ZENITH_ASSERT_EQ_FLOAT(xTile.m_fSize, fExpectedSize, 0.0f, "tile size must follow its LOD (index %u)", u);
		ZENITH_ASSERT_EQ_FLOAT(xTile.m_fWorldMinX, static_cast<float>(xTile.m_iTileX) * fExpectedSize, 0.0f,
			"tile world origin must follow its grid coordinate (index %u)", u);
		ZENITH_ASSERT_EQ_FLOAT(xTile.m_fWorldMinZ, static_cast<float>(xTile.m_iTileZ) * fExpectedSize, 0.0f,
			"tile world origin must follow its grid coordinate (index %u)", u);
	}

	// The seam rule the placement CS applies per lattice node so a partially
	// covered LO tile does not re-emit blades a HI tile already placed. It must
	// be the same 64 m radius the HI tiles were selected against, or the seam
	// either doubles up or gaps.
	const Zenith_Maths::Vector3 xCamera = GrassTypesTest_DefaultParams().m_xCameraPos;
	ZENITH_ASSERT_TRUE(Flux_GrassIsInsideHiRegion(xCamera, xCamera.x, xCamera.z),
		"the camera's own position is inside the HI region");
	ZENITH_ASSERT_TRUE(Flux_GrassIsInsideHiRegion(xCamera, xCamera.x + 63.0f, xCamera.z),
		"a node just inside the HI radius belongs to the HI pass");
	ZENITH_ASSERT_FALSE(Flux_GrassIsInsideHiRegion(xCamera, xCamera.x + 65.0f, xCamera.z),
		"a node just outside the HI radius belongs to the LO pass");
}

ZENITH_TEST(FluxGrassTypes, TileSelectionClipsToMapExtents)
{
	Flux_GrassTileList xUnbounded;
	Flux_GrassSelectTiles(GrassTypesTest_DefaultParams(), xUnbounded);

	// A 128 m square map around the camera. Tiles that do not overlap it hold no
	// terrain to grow on, so scheduling them would burn a dispatch on nothing.
	Flux_GrassTileSelectParams xParams = GrassTypesTest_DefaultParams();
	xParams.m_xExtents.m_fMinX = 0.0f;
	xParams.m_xExtents.m_fMinZ = 0.0f;
	xParams.m_xExtents.m_fMaxX = 128.0f;
	xParams.m_xExtents.m_fMaxZ = 128.0f;

	Flux_GrassTileList xClipped;
	Flux_GrassSelectTiles(xParams, xClipped);

	ZENITH_ASSERT_GT(xClipped.m_uCount, 0u, "a map the camera stands on must still yield tiles");
	ZENITH_ASSERT_LT(xClipped.m_uCount, xUnbounded.m_uCount, "clipping to a 128 m map must drop the tiles outside it");
	for (u_int u = 0; u < xClipped.m_uCount; u++)
	{
		const Flux_GrassTile& xTile = xClipped.Get(u);
		ZENITH_ASSERT_GT(xTile.m_fWorldMinX + xTile.m_fSize, 0.0f, "kept tile %u must overlap the map in X", u);
		ZENITH_ASSERT_LT(xTile.m_fWorldMinX, 128.0f, "kept tile %u must overlap the map in X", u);
		ZENITH_ASSERT_GT(xTile.m_fWorldMinZ + xTile.m_fSize, 0.0f, "kept tile %u must overlap the map in Z", u);
		ZENITH_ASSERT_LT(xTile.m_fWorldMinZ, 128.0f, "kept tile %u must overlap the map in Z", u);
	}

	// Degenerate extents mean "unbounded", not "empty": a caller that has not
	// loaded its map yet must not silently lose all its grass.
	Flux_GrassTileSelectParams xDegenerate = GrassTypesTest_DefaultParams();
	xDegenerate.m_xExtents.m_fMinX = 10.0f;
	xDegenerate.m_xExtents.m_fMaxX = 10.0f;
	Flux_GrassTileList xDegenerateList;
	Flux_GrassSelectTiles(xDegenerate, xDegenerateList);
	ZENITH_ASSERT_TRUE(GrassTypesTest_ListsIdentical(xDegenerateList, xUnbounded),
		"degenerate extents must disable clipping entirely");
}

ZENITH_TEST(FluxGrassTypes, TileAABBTakesItsHeightBandFromTheGrid)
{
	// 2x2 cells of 64 m, covering [0,128] on both axes. Row-major in Z.
	const float afMinY[4] = { 0.0f, 10.0f, 20.0f, 30.0f };
	const float afMaxY[4] = { 1.0f, 11.0f, 21.0f, 31.0f };

	Flux_GrassTileSelectParams xParams = GrassTypesTest_DefaultParams();
	xParams.m_fBladeHeadroom = 2.0f;
	xParams.m_xHeights.m_pfMinY = afMinY;
	xParams.m_xHeights.m_pfMaxY = afMaxY;
	xParams.m_xHeights.m_uCellsX = 2u;
	xParams.m_xHeights.m_uCellsZ = 2u;
	xParams.m_xHeights.m_fCellSize = 64.0f;

	Flux_GrassTile xTile;
	xTile.m_eLOD = Flux_GrassTileLOD::HI;
	xTile.m_fSize = Flux_GrassConfig::fHI_TILE_SIZE;
	xTile.m_fWorldMinX = 0.0f;
	xTile.m_fWorldMinZ = 0.0f;

	const Zenith_AABB xInside = Flux_GrassTileAABB(xParams, xTile);
	ZENITH_ASSERT_EQ_FLOAT(xInside.m_xMin.x, 0.0f, 0.0f, "the AABB's XZ span is the tile rect");
	ZENITH_ASSERT_EQ_FLOAT(xInside.m_xMax.x, Flux_GrassConfig::fHI_TILE_SIZE, 0.0f, "the AABB's XZ span is the tile rect");
	ZENITH_ASSERT_EQ_FLOAT(xInside.m_xMin.y, 0.0f, 0.0f, "a tile inside one cell takes that cell's floor");
	ZENITH_ASSERT_EQ_FLOAT(xInside.m_xMax.y, 1.0f + 2.0f, 0.0f,
		"... and its ceiling plus the blade headroom — blades stand ON the ground, so terrain height alone would clip them");

	// A tile straddling two cells must take the union, not one of them.
	Flux_GrassTile xStraddle = xTile;
	xStraddle.m_fWorldMinX = 56.0f;
	const Zenith_AABB xStraddleAABB = Flux_GrassTileAABB(xParams, xStraddle);
	ZENITH_ASSERT_EQ_FLOAT(xStraddleAABB.m_xMin.y, 0.0f, 0.0f, "a straddling tile takes the lowest floor it covers");
	ZENITH_ASSERT_EQ_FLOAT(xStraddleAABB.m_xMax.y, 11.0f + 2.0f, 0.0f, "a straddling tile takes the highest ceiling it covers");

	// Off the grid entirely: clamp to the edge cell rather than index past it.
	Flux_GrassTile xFar = xTile;
	xFar.m_fWorldMinX = 1000.0f;
	xFar.m_fWorldMinZ = 1000.0f;
	const Zenith_AABB xFarAABB = Flux_GrassTileAABB(xParams, xFar);
	ZENITH_ASSERT_EQ_FLOAT(xFarAABB.m_xMin.y, 30.0f, 0.0f, "off-grid must clamp to the edge cell, not read past the grid");
	ZENITH_ASSERT_EQ_FLOAT(xFarAABB.m_xMax.y, 31.0f + 2.0f, 0.0f, "off-grid must clamp to the edge cell, not read past the grid");

	// No grid at all: the fallback band, not garbage and not a zero-height slab
	// that would cull every tile.
	Flux_GrassTileSelectParams xNoGrid = GrassTypesTest_DefaultParams();
	xNoGrid.m_fBladeHeadroom = 1.0f;
	xNoGrid.m_xHeights.m_fFallbackMinY = -7.0f;
	xNoGrid.m_xHeights.m_fFallbackMaxY = 3.0f;
	const Zenith_AABB xFallback = Flux_GrassTileAABB(xNoGrid, xTile);
	ZENITH_ASSERT_EQ_FLOAT(xFallback.m_xMin.y, -7.0f, 0.0f, "an absent height grid must fall back, not produce an empty band");
	ZENITH_ASSERT_EQ_FLOAT(xFallback.m_xMax.y, 3.0f + 1.0f, 0.0f, "the fallback ceiling still takes the blade headroom");
	ZENITH_ASSERT_TRUE(xFallback.IsValid(), "a fallback AABB must still be a valid box");
}

ZENITH_TEST(FluxGrassTypes, TileSelectionIsDistanceSorted)
{
	Flux_GrassTileList xList;
	Flux_GrassSelectTiles(GrassTypesTest_DefaultParams(), xList);
	ZENITH_ASSERT_GT(xList.m_uCount, 1u, "need at least two tiles for the ordering to mean anything");

	for (u_int u = 1; u < xList.m_uCount; u++)
	{
		ZENITH_ASSERT_LE(xList.Get(u - 1u).m_fDistanceSq, xList.Get(u).m_fDistanceSq,
			"output must be nearest-first at index %u", u);
		// Strictness matters: the total order is what makes the cap's kept set
		// independent of visit order, and a duplicate tile would break it.
		ZENITH_ASSERT_TRUE(Flux_GrassTileOrderLess(xList.Get(u - 1u), xList.Get(u)),
			"the tile order must be strict at index %u — a duplicate tile was emitted", u);
	}
}

ZENITH_TEST(FluxGrassTypes, TileSelectionRejectsTilesOutsideEveryFrustum)
{
	Flux_GrassTileList xUnculled;
	Flux_GrassSelectTiles(GrassTypesTest_DefaultParams(), xUnculled);
	ZENITH_ASSERT_LT(GrassTypesTest_FindTile(xUnculled, Flux_GrassTileLOD::HI, 0, -1), xUnculled.m_uCount,
		"sanity: the tile the frustum is about to reject is present without culling");

	// A single half-space: keep only geometry reaching z >= 8 (the camera's own
	// z), which is exactly the "behind the camera" half. The other five planes
	// stay at their default (0,1,0)/0, which every tile AABB passes, so this
	// isolates one axis and the union rule below is unambiguous.
	Zenith_Frustum xFrustum;
	xFrustum.m_axPlanes[4] = Zenith_Plane(Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f), -8.0f);

	Flux_GrassTileSelectParams xParams = GrassTypesTest_DefaultParams();
	xParams.m_xFrusta.m_pxFrusta = &xFrustum;
	xParams.m_xFrusta.m_uCount = 1u;

	Flux_GrassTileList xCulled;
	Flux_GrassSelectTiles(xParams, xCulled);

	ZENITH_ASSERT_GT(xCulled.m_uCount, 0u, "culling must narrow the list, not empty it");
	ZENITH_ASSERT_LT(xCulled.m_uConsidered, xUnculled.m_uConsidered, "the half-space must actually reject tiles");
	ZENITH_ASSERT_EQ(GrassTypesTest_FindTile(xCulled, Flux_GrassTileLOD::HI, 0, -1), xCulled.m_uCount,
		"a tile lying entirely behind the plane must be rejected");

	for (u_int u = 0; u < xCulled.m_uCount; u++)
	{
		const Flux_GrassTile& xTile = xCulled.Get(u);
		ZENITH_ASSERT_GE(xTile.m_fWorldMinZ + xTile.m_fSize, 8.0f,
			"every surviving tile must reach the plane (index %u)", u);
	}

	// An empty plane set means "no culling", not "cull everything" — a caller
	// that has not built its cascades yet must still get grass.
	Flux_GrassTileSelectParams xNoFrusta = GrassTypesTest_DefaultParams();
	xNoFrusta.m_xFrusta.m_pxFrusta = &xFrustum;
	xNoFrusta.m_xFrusta.m_uCount = 0u;
	Flux_GrassTileList xNoFrustaList;
	Flux_GrassSelectTiles(xNoFrusta, xNoFrustaList);
	ZENITH_ASSERT_TRUE(GrassTypesTest_ListsIdentical(xNoFrustaList, xUnculled),
		"a zero-count plane set must disable culling entirely");
}

ZENITH_TEST(FluxGrassTypes, TileSelectionCapsAtTheTileBudgetDeterministically)
{
	Flux_GrassTileList xFirst;
	Flux_GrassSelectTiles(GrassTypesTest_DefaultParams(), xFirst);

	ZENITH_ASSERT_EQ(xFirst.m_uCount, Flux_GrassConfig::uMAX_TILES,
		"the default draw distance overruns the tile budget — if it stopped doing so the overflow path would go untested");
	ZENITH_ASSERT_TRUE(xFirst.m_bOverflowed, "overflow must be reported so the caller can log it");
	ZENITH_ASSERT_GT(xFirst.m_uConsidered, xFirst.m_uCount, "more tiles must have survived culling than were kept");

	// Determinism is the point of the total order: a second identical call must
	// keep the identical set, in the identical order, including which tiles the
	// cap dropped.
	Flux_GrassTileList xSecond;
	Flux_GrassSelectTiles(GrassTypesTest_DefaultParams(), xSecond);
	ZENITH_ASSERT_TRUE(GrassTypesTest_ListsIdentical(xFirst, xSecond),
		"two identical calls must produce an identical kept set");

	// Overflow keeps the NEAREST, so nothing kept may sit past the last entry.
	const float fFarthestKept = xFirst.Get(xFirst.m_uCount - 1u).m_fDistanceSq;
	for (u_int u = 0; u < xFirst.m_uCount; u++)
	{
		ZENITH_ASSERT_LE(xFirst.Get(u).m_fDistanceSq, fFarthestKept,
			"the kept set must be the nearest tiles, not the first visited (index %u)", u);
	}

	// The cap is a budget, not a wall: a tighter draw distance fits inside it.
	Flux_GrassTileSelectParams xNear = GrassTypesTest_DefaultParams();
	xNear.m_fMaxDistance = Flux_GrassConfig::fMIN_MAX_DISTANCE;
	Flux_GrassTileList xNearList;
	Flux_GrassSelectTiles(xNear, xNearList);
	ZENITH_ASSERT_LT(xNearList.m_uCount, Flux_GrassConfig::uMAX_TILES, "the minimum draw distance must fit in the budget");
	ZENITH_ASSERT_FALSE(xNearList.m_bOverflowed, "a list that fits must not report overflow");
	ZENITH_ASSERT_EQ(xNearList.m_uConsidered, xNearList.m_uCount, "nothing was dropped, so considered == kept");
}

ZENITH_TEST(FluxGrassTypes, MaxDistanceIsClampedToItsBand)
{
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassClampMaxDistance(1.0f), Flux_GrassConfig::fMIN_MAX_DISTANCE, 0.0f,
		"below the band clamps up");
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassClampMaxDistance(-100.0f), Flux_GrassConfig::fMIN_MAX_DISTANCE, 0.0f,
		"a negative draw distance clamps up, it does not invert");
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassClampMaxDistance(10000.0f), Flux_GrassConfig::fMAX_MAX_DISTANCE, 0.0f,
		"above the band clamps down");
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassClampMaxDistance(Flux_GrassConfig::fDEFAULT_MAX_DISTANCE),
		Flux_GrassConfig::fDEFAULT_MAX_DISTANCE, 0.0f, "the default must sit inside its own band");

	// The scheduler must APPLY the clamp, not merely offer it.
	Flux_GrassTileSelectParams xTiny = GrassTypesTest_DefaultParams();
	xTiny.m_fMaxDistance = 1.0f;
	Flux_GrassTileSelectParams xMin = GrassTypesTest_DefaultParams();
	xMin.m_fMaxDistance = Flux_GrassConfig::fMIN_MAX_DISTANCE;
	Flux_GrassTileList xTinyList;
	Flux_GrassTileList xMinList;
	Flux_GrassSelectTiles(xTiny, xTinyList);
	Flux_GrassSelectTiles(xMin, xMinList);
	ZENITH_ASSERT_TRUE(GrassTypesTest_ListsIdentical(xTinyList, xMinList),
		"a 1 m draw distance must behave exactly like the 50 m floor");

	Flux_GrassTileSelectParams xHuge = GrassTypesTest_DefaultParams();
	xHuge.m_fMaxDistance = 10000.0f;
	Flux_GrassTileSelectParams xMax = GrassTypesTest_DefaultParams();
	xMax.m_fMaxDistance = Flux_GrassConfig::fMAX_MAX_DISTANCE;
	Flux_GrassTileList xHugeList;
	Flux_GrassTileList xMaxList;
	Flux_GrassSelectTiles(xHuge, xHugeList);
	Flux_GrassSelectTiles(xMax, xMaxList);
	ZENITH_ASSERT_TRUE(GrassTypesTest_ListsIdentical(xHugeList, xMaxList),
		"a 10 km draw distance must behave exactly like the 400 m ceiling");
}

ZENITH_TEST(FluxGrassTypes, WindStrengthIsDeterministicAndBounded)
{
	const Flux_WindConstants xWind;
	for (int i = 0; i < 8; i++)
	{
		const float fWorldX = static_cast<float>(i) * 17.0f;
		const float fWorldZ = static_cast<float>(i) * -23.0f;
		const float fFirst = Flux_SampleWindStrength(xWind, fWorldX, fWorldZ);
		const float fSecond = Flux_SampleWindStrength(xWind, fWorldX, fWorldZ);

		ZENITH_ASSERT_EQ_FLOAT(fFirst, fSecond, 0.0f,
			"the same point must give the same gust, bit for bit (%f, %f)", fWorldX, fWorldZ);
		ZENITH_ASSERT_GE(fFirst, 0.0f, "gust strength must not go negative at (%f, %f)", fWorldX, fWorldZ);
		ZENITH_ASSERT_LE(fFirst, xWind.m_fStrength, "gust strength must not exceed the configured strength at (%f, %f)",
			fWorldX, fWorldZ);
	}

	// Zero strength is exactly calm — the gust term must not leak through.
	Flux_WindConstants xCalm = xWind;
	xCalm.m_fStrength = 0.0f;
	ZENITH_ASSERT_EQ_FLOAT(Flux_SampleWindStrength(xCalm, 10.0f, 20.0f), 0.0f, 0.0f, "zero strength must be exactly calm");

	// Sharpening can only suppress: the field is in [0,1], so a larger exponent
	// cannot raise it. This is the property that makes gusts rare rather than
	// making everything vibrate.
	Flux_WindConstants xSoft = xWind;
	xSoft.m_fGustSharpness = 2.0f;
	Flux_WindConstants xHard = xWind;
	xHard.m_fGustSharpness = 4.0f;
	ZENITH_ASSERT_LE(Flux_SampleWindStrength(xHard, 10.0f, 20.0f), Flux_SampleWindStrength(xSoft, 10.0f, 20.0f),
		"a harder gust exponent must never raise the field");
}

ZENITH_TEST(FluxGrassTypes, WindFieldScrollsWithTimeAndVariesInSpace)
{
	const Flux_WindConstants xWind;

	Flux_WindConstants xLater = xWind;
	xLater.m_fTime = 5.0f;
	ZENITH_ASSERT_NE(Flux_SampleWindStrength(xWind, 10.0f, 20.0f), Flux_SampleWindStrength(xLater, 10.0f, 20.0f),
		"a scrolling gust field must move: same point, different time");

	// It is a FIELD, not a global multiplier — distant points differ at one instant.
	ZENITH_ASSERT_NE(Flux_SampleWindStrength(xWind, 10.0f, 20.0f), Flux_SampleWindStrength(xWind, 1000.0f, 1000.0f),
		"two distant points must not share one gust value");

	// Time may only enter through the scroll. Freezing the scroll must freeze
	// the field completely — there is no second clock hidden in the sampler.
	Flux_WindConstants xFrozen = xWind;
	xFrozen.m_fScrollSpeed = 0.0f;
	Flux_WindConstants xFrozenLater = xFrozen;
	xFrozenLater.m_fTime = 5.0f;
	ZENITH_ASSERT_EQ_FLOAT(Flux_SampleWindStrength(xFrozen, 10.0f, 20.0f),
		Flux_SampleWindStrength(xFrozenLater, 10.0f, 20.0f), 0.0f,
		"with no scroll speed, time must not reach the field at all");
}

ZENITH_TEST(FluxGrassTypes, MapSamplingIsBilinearInsideAndClampedAtTheEdge)
{
	// 2x2 map over a 2 m square: texel (x,z) sits exactly at world (x,z), and
	// every value below is exactly representable, so zero epsilon is honest.
	const float afMap[4] = { 1.0f, 2.0f, 3.0f, 4.0f };   // row z=0, then row z=1
	Flux_GrassMap xMap;
	xMap.m_pData = afMap;
	xMap.m_uWidth = 2u;
	xMap.m_uHeight = 2u;
	xMap.m_fWorldSize = 2.0f;
	xMap.m_eFormat = Flux_GrassMapFormat::F32;

	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassSampleMapBilinear(xMap, 0.0f, 0.0f), 1.0f, 0.0f, "texel positions must read back exactly");
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassSampleMapBilinear(xMap, 1.0f, 0.0f), 2.0f, 0.0f, "+X texel");
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassSampleMapBilinear(xMap, 0.0f, 1.0f), 3.0f, 0.0f, "+Z texel — the map is row-major in Z");
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassSampleMapBilinear(xMap, 0.5f, 0.0f), 1.5f, 0.0f, "half way along X is the mean of two texels");
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassSampleMapBilinear(xMap, 0.0f, 0.5f), 2.0f, 0.0f, "half way along Z is the mean of two texels");
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassSampleMapBilinear(xMap, 0.5f, 0.5f), 2.5f, 0.0f, "the centre is the mean of all four");

	// Edge clamp, both directions on both axes.
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassSampleMapBilinear(xMap, -50.0f, -50.0f), 1.0f, 0.0f, "off the near corner clamps to it");
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassSampleMapBilinear(xMap, 50.0f, 50.0f), 4.0f, 0.0f, "off the far corner clamps to it");
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassSampleMapBilinear(xMap, 50.0f, 0.0f), 2.0f, 0.0f, "clamping one axis must not disturb the other");

	// The scale rides on top of the interpolation, not under it.
	Flux_GrassMap xScaled = xMap;
	xScaled.m_fScale = 0.5f;
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassSampleMapBilinear(xScaled, 0.5f, 0.0f), 0.75f, 0.0f,
		"m_fScale must apply to the interpolated value");

	// U8 normalization: 255 is exactly 1.0, so the midpoint is exactly 0.5.
	const u_int8 aucMap[2] = { 0u, 255u };
	Flux_GrassMap xByteMap;
	xByteMap.m_pData = aucMap;
	xByteMap.m_uWidth = 2u;
	xByteMap.m_uHeight = 1u;
	xByteMap.m_fWorldSize = 2.0f;
	xByteMap.m_eFormat = Flux_GrassMapFormat::U8;
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassSampleMapBilinear(xByteMap, 1.0f, 0.0f), 1.0f, 0.0f, "a U8 255 must normalize to exactly 1");
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassSampleMapBilinear(xByteMap, 0.5f, 0.0f), 0.5f, 0.0f, "U8 bilinear midpoint");
}

ZENITH_TEST(FluxGrassTypes, AbsentMapsSampleZero)
{
	// An unset map samples 0, never a neutral 1: a caller with no data must
	// decide what "no data" means rather than silently inheriting "full".
	const Flux_GrassMap xNull;
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassSampleMapBilinear(xNull, 0.0f, 0.0f), 0.0f, 0.0f, "an unset map must sample zero");
	ZENITH_ASSERT_EQ(Flux_GrassSampleTypeIndex(xNull, 0.0f, 0.0f), 0u, "an unset type map must pick type 0");

	const float afMap[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

	Flux_GrassMap xZeroDims;
	xZeroDims.m_pData = afMap;
	xZeroDims.m_uWidth = 0u;
	xZeroDims.m_uHeight = 0u;
	xZeroDims.m_fWorldSize = 4.0f;
	xZeroDims.m_eFormat = Flux_GrassMapFormat::F32;
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassSampleMapBilinear(xZeroDims, 1.0f, 1.0f), 0.0f, 0.0f,
		"a zero-dimension map must not be indexed");

	Flux_GrassMap xZeroWorld;
	xZeroWorld.m_pData = afMap;
	xZeroWorld.m_uWidth = 2u;
	xZeroWorld.m_uHeight = 2u;
	xZeroWorld.m_fWorldSize = 0.0f;
	xZeroWorld.m_eFormat = Flux_GrassMapFormat::F32;
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassSampleMapBilinear(xZeroWorld, 1.0f, 1.0f), 0.0f, 0.0f,
		"a map covering no world must not be divided by");
}

ZENITH_TEST(FluxGrassTypes, TypeMapPicksNearestTexelNeverABlend)
{
	// Adjacent types 0 and 7: any interpolation would invent types 1 to 6, which
	// the author never placed anywhere on the map.
	const u_int8 aucTypes[4] = { 0u, 7u, 7u, 3u };
	Flux_GrassMap xMap;
	xMap.m_pData = aucTypes;
	xMap.m_uWidth = 4u;
	xMap.m_uHeight = 1u;
	xMap.m_fWorldSize = 4.0f;
	xMap.m_eFormat = Flux_GrassMapFormat::U8;

	ZENITH_ASSERT_EQ(Flux_GrassSampleTypeIndex(xMap, 0.0f, 0.0f), 0u, "texel 0");
	ZENITH_ASSERT_EQ(Flux_GrassSampleTypeIndex(xMap, 0.49f, 0.0f), 0u, "below the halfway point the nearer texel wins");
	ZENITH_ASSERT_EQ(Flux_GrassSampleTypeIndex(xMap, 0.51f, 0.0f), 7u, "past the halfway point the other texel wins outright");
	ZENITH_ASSERT_EQ(Flux_GrassSampleTypeIndex(xMap, 3.0f, 0.0f), 3u, "texel 3");
	ZENITH_ASSERT_EQ(Flux_GrassSampleTypeIndex(xMap, 99.0f, 0.0f), 3u, "off the far edge clamps to the last texel");

	// The whole sweep must only ever return a type that is IN the map.
	for (int i = 0; i <= 40; i++)
	{
		const float fWorldX = static_cast<float>(i) * 0.1f;
		const u_int uType = Flux_GrassSampleTypeIndex(xMap, fWorldX, 0.0f);
		ZENITH_ASSERT_TRUE(uType == 0u || uType == 3u || uType == 7u,
			"type pick at x=%f produced %u, a type that is not on the map — the index was interpolated", fWorldX, uType);
	}

	// A non-U8 map is not a type map: the raw byte semantics do not survive a
	// float or u16 payload, so the pick refuses rather than reinterpreting.
	Flux_GrassMap xWrongFormat = xMap;
	xWrongFormat.m_eFormat = Flux_GrassMapFormat::F32;
	ZENITH_ASSERT_EQ(Flux_GrassSampleTypeIndex(xWrongFormat, 0.51f, 0.0f), 0u,
		"a non-U8 map must not be reinterpreted as type indices");
}

ZENITH_TEST(FluxGrassTypes, PipelineVariantSelectionIsATotalFunction)
{
	ZENITH_ASSERT_TRUE(Flux_GrassSelectPipelineVariant(false, false) == Flux_GrassPipelineVariant::GBUFFER,
		"no velocity, no shadow: the base 4-MRT G-buffer pipeline");
	ZENITH_ASSERT_TRUE(Flux_GrassSelectPipelineVariant(true, false) == Flux_GrassPipelineVariant::GBUFFER_VELOCITY,
		"the TAA velocity latch selects the 5-MRT pipeline");
	ZENITH_ASSERT_TRUE(Flux_GrassSelectPipelineVariant(false, true) == Flux_GrassPipelineVariant::SHADOW_DEPTH,
		"a shadow cascade selects the depth-only pipeline");
	ZENITH_ASSERT_TRUE(Flux_GrassSelectPipelineVariant(true, true) == Flux_GrassPipelineVariant::SHADOW_DEPTH,
		"a cascade target has no colour attachment at all, so the velocity latch must not reach it");

	// Surjectivity: every variant is reachable, so none is dead pipeline state.
	bool abSeen[3] = { false, false, false };
	for (int iVelocity = 0; iVelocity < 2; iVelocity++)
	{
		for (int iShadow = 0; iShadow < 2; iShadow++)
		{
			const Flux_GrassPipelineVariant eVariant = Flux_GrassSelectPipelineVariant(iVelocity != 0, iShadow != 0);
			const u_int uIndex = static_cast<u_int>(eVariant);
			ZENITH_ASSERT_LT(uIndex, static_cast<u_int>(Flux_GrassPipelineVariant::COUNT),
				"selection must never return COUNT (velocity=%d, shadow=%d)", iVelocity, iShadow);
			abSeen[uIndex] = true;
		}
	}
	for (u_int u = 0; u < 3u; u++)
	{
		ZENITH_ASSERT_TRUE(abSeen[u], "pipeline variant %u is unreachable — no input selects it", u);
	}

	// The framebuffer contract each variant's pipeline declares.
	ZENITH_ASSERT_EQ(Flux_GrassPipelineColourAttachmentCount(Flux_GrassPipelineVariant::GBUFFER), 4u,
		"the base variant writes the 4 core MRTs");
	ZENITH_ASSERT_EQ(Flux_GrassPipelineColourAttachmentCount(Flux_GrassPipelineVariant::GBUFFER_VELOCITY), 5u,
		"the velocity variant adds the motion-vector target");
	ZENITH_ASSERT_EQ(Flux_GrassPipelineColourAttachmentCount(Flux_GrassPipelineVariant::SHADOW_DEPTH), 0u,
		"the shadow variant writes depth only");

	ZENITH_ASSERT_TRUE(Flux_GrassPipelineVariantIsShadow(Flux_GrassPipelineVariant::SHADOW_DEPTH),
		"the shadow variant must classify as a shadow variant");
	ZENITH_ASSERT_FALSE(Flux_GrassPipelineVariantIsVelocity(Flux_GrassPipelineVariant::SHADOW_DEPTH),
		"the shadow variant must never classify as a velocity variant");
	ZENITH_ASSERT_TRUE(Flux_GrassPipelineVariantIsVelocity(Flux_GrassPipelineVariant::GBUFFER_VELOCITY),
		"the velocity variant must classify as a velocity variant");
	ZENITH_ASSERT_FALSE(Flux_GrassPipelineVariantIsShadow(Flux_GrassPipelineVariant::GBUFFER),
		"the base variant must not classify as a shadow variant");
}
