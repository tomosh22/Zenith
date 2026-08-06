#include "Core/Zenith_TestFramework.h"
#include "Flux/Vegetation/Flux_GrassImpl.h"
#include "Flux/Vegetation/Flux_GrassTypes.h"
#include "Flux/Vegetation/Flux_GrassTypeTable.h"
#include "Core/Zenith_Engine.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "Flux/Flux_FeatureRegistry.h"
#include "DataStream/Zenith_DataStream.h"

// ============================================================================
// Flux_Grass unit tests. Three suites, pinning three different things:
//
//   FluxGrassTypes     — Flux/Vegetation/Flux_GrassTypes.h ONLY: plain structs
//                        and free functions, no engine singleton, no device, no
//                        file IO. That includes the CPU MIRRORS of the placement
//                        compute shader and the vertex stage (blade pose, weld /
//                        fold, clump Voronoi, dithered type pick) — the .slang
//                        twin is the authority in every case and each mirror
//                        names its own.
//   FluxGrassImpl      — the live g_xEngine.Grass() feature: build validation,
//                        the CPU query surface, scene-state clearing, movers.
//   FluxGrassTypeTable — authoring, validation and serialization of the per-type
//                        parameter table.
//
// This file is textually included at the bottom of Flux_Grass.cpp (the always-
// linked feature TU), so it CAN see Flux_GrassImpl's internals — the impl suite
// deliberately drives the public surface anyway, because that is the surface the
// render-reset hook and game code actually use.
//
// HEADLESS-SAFE. Nothing here dispatches, records, or reads back GPU content:
//   * The impl tests drive the process-wide subsystem through g_xEngine.Grass()
//     rather than constructing a Flux_GrassImpl on the stack (its pipeline and
//     shader members' destructors reach the backend device).
//   * Build* / ClearSceneData / the three samplers are pure CPU work: the map
//     quantize is a CPU copy and UploadMapTextures is a no-op under the null
//     renderer.
//   * ReadbackVisibleBladeCount is WINDOWED-ONLY truth (DownloadBufferData
//     zero-fills without an allocator), so it is asserted only where zero is the
//     contract rather than an observation.
//
// Every impl test starts AND ends with ClearSceneData(), so neither a prior boot
// phase's grass nor a prior test's leaks into the next. They also build with the
// default BuildParams, which re-stamps the density scale to 1 — each restores
// the value it found, because the debug variables bind that member BY REFERENCE
// and game code is entitled to have written it.
// ============================================================================

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
			for (u_int uClass = 0; uClass < 4u; uClass++)
			{
				for (int iFold = 0; iFold < 2; iFold++)
				{
					for (int iLO = 0; iLO < 2; iLO++)
					{
						const bool bFolded = (iFold != 0);
						const bool bLOMesh = (iLO != 0);
						const u_int uFlags = Flux_GrassPackTypeFlags(auTypes[uT], bFolded, bLOMesh, uClass, auHashes[uH]);

						ZENITH_ASSERT_EQ(Flux_GrassTypeFlagsIndex(uFlags), auTypes[uT],
							"type index must round-trip (type %u, hash %u)", auTypes[uT], auHashes[uH]);
						ZENITH_ASSERT_EQ(Flux_GrassTypeFlagsIsFolded(uFlags), bFolded,
							"folded bit must round-trip (type %u)", auTypes[uT]);
						ZENITH_ASSERT_EQ(Flux_GrassTypeFlagsIsLOMesh(uFlags), bLOMesh,
							"LO-mesh bit must round-trip (type %u)", auTypes[uT]);
						// The vertex stage applies the class fade and cannot recover the
						// class from posWS (the base is jittered and clump-pulled), so the
						// class HAS to survive the record.
						ZENITH_ASSERT_EQ(Flux_GrassTypeFlagsLatticeClass(uFlags), uClass,
							"lattice class must round-trip (type %u, class %u)", auTypes[uT], uClass);
						ZENITH_ASSERT_EQ(Flux_GrassTypeFlagsFragmentHash(uFlags), auHashes[uH],
							"fragment hash must round-trip (type %u, hash %u)", auTypes[uT], auHashes[uH]);
					}
				}
			}
		}
	}

	// The class field must be independent of its neighbours in BOTH directions:
	// bits 10-11 sit between the LO-mesh bit and the reserved 12-15 gap, so a
	// shift error would either eat the LO flag or bleed into the reserved bits.
	for (u_int uClass = 0; uClass < 4u; uClass++)
	{
		const u_int uClassOnly = Flux_GrassPackTypeFlags(0u, false, false, uClass, 0u);
		ZENITH_ASSERT_EQ(uClassOnly, uClass << 10u, "the class must occupy bits 10-11 alone (class %u)", uClass);
		ZENITH_ASSERT_EQ(Flux_GrassTypeFlagsIndex(uClassOnly), 0u, "the class must not reach the type index (class %u)", uClass);
		ZENITH_ASSERT_FALSE(Flux_GrassTypeFlagsIsFolded(uClassOnly), "the class must not reach the folded bit (class %u)", uClass);
		ZENITH_ASSERT_FALSE(Flux_GrassTypeFlagsIsLOMesh(uClassOnly), "the class must not reach the LO-mesh bit (class %u)", uClass);
		ZENITH_ASSERT_EQ(Flux_GrassTypeFlagsFragmentHash(uClassOnly), 0u, "the class must not reach the fragment hash (class %u)", uClass);
	}

	// Reserved bits 12-15 must stay clear for every legal input, or a later field
	// added there would decode a value this packer never intended to write.
	ZENITH_ASSERT_EQ(Flux_GrassPackTypeFlags(0xFFu, true, true, 3u, 0xFFFFu) & 0x0000F000u, 0u,
		"bits 12-15 are reserved and must be zero even with every other field saturated");

	// Oversized inputs must truncate to their own field, never corrupt the next.
	const u_int uTruncated = Flux_GrassPackTypeFlags(0x1FFu, false, false, 0u, 0x12345u);
	ZENITH_ASSERT_EQ(Flux_GrassTypeFlagsIndex(uTruncated), 0xFFu, "a 9-bit type index must truncate to 8 bits");
	ZENITH_ASSERT_FALSE(Flux_GrassTypeFlagsIsFolded(uTruncated), "a 9-bit type index must not spill into the folded bit");
	ZENITH_ASSERT_FALSE(Flux_GrassTypeFlagsIsLOMesh(uTruncated), "a 9-bit type index must not spill into the LO-mesh bit");
	ZENITH_ASSERT_EQ(Flux_GrassTypeFlagsFragmentHash(uTruncated), 0x2345u, "a 17-bit fragment hash must truncate to 16 bits");

	const u_int uWideClass = Flux_GrassPackTypeFlags(0u, false, false, 0x7u, 0u);
	ZENITH_ASSERT_EQ(Flux_GrassTypeFlagsLatticeClass(uWideClass), 0x3u, "a 3-bit class must truncate to 2 bits");
	ZENITH_ASSERT_EQ(uWideClass & 0x0000F000u, 0u, "an oversized class must not spill into the reserved bits");

	ZENITH_ASSERT_EQ(Flux_GrassPackTypeFlags(0u, false, false, 0u, 0u), 0u,
		"the all-default record must be all zero, so a zeroed buffer decodes to type 0");

	// The four classes the packer must carry are exactly the four the lattice
	// produces — a wider class space would silently alias in two bits.
	ZENITH_ASSERT_LT(Flux_GrassLatticeClass(1, 1), 4u, "the lattice must only ever produce classes 0-3");
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

// ============================================================================
// CPU mirrors of the shader. Everything below pins Flux_GrassTypes.h against
// Zenith/Flux/Shaders/Vegetation/Flux_GrassCommon.slang and
// Zenith/Flux/Shaders/Vegetation/Flux_Grass_Placement.slang, which are the
// AUTHORITATIVE copies. Two strengths of claim live here and they are not equal:
//
//   * Anything the INTEGER HASH decides — the clump site jitter, the type
//     dither's roll — is bit-exact across the two languages, because
//     Common/Noise.slang is a hand-written twin of Zenith_Noise.h.
//   * Anything float-valued is pinned by PROPERTY (endpoint identity, convex
//     hull, monotonicity, weld coincidence), never by cross-language bit
//     equality: FMA contraction and fast-math modes differ per target.
// ============================================================================

namespace
{
	// A deliberately asymmetric blade: a pose with zero bend, zero side curve or
	// zero wind would pass a Bezier that silently dropped any of those terms.
	constexpr float fGrassMirrorHeight = 0.6f;
	constexpr float fGrassMirrorBend = 0.30f;
	constexpr float fGrassMirrorSideCurve = 0.12f;
	constexpr float fGrassMirrorTilt = 0.20f;

	Flux_GrassBladeFrame GrassMirrorTest_Frame()
	{
		return Flux_GrassMakeBladeFrame(Zenith_Maths::Vector2(1.0f, 0.0f), fGrassMirrorTilt);
	}

	Flux_GrassBladeCurve GrassMirrorTest_Curve(float fHeight)
	{
		return Flux_GrassBuildBladeCurve(Zenith_Maths::Vector3(0.0f), GrassMirrorTest_Frame(), fHeight,
			fGrassMirrorBend, fGrassMirrorSideCurve, Zenith_Maths::Vector3(0.05f, 0.0f, 0.02f));
	}

	// Componentwise convex hull of the four control points. A cubic Bezier never
	// leaves it, so this is the defining property and not a loose bound.
	bool GrassMirrorTest_InsideHull(const Flux_GrassBladeCurve& xCurve, const Zenith_Maths::Vector3& xPoint)
	{
		for (int i = 0; i < 3; i++)
		{
			const float fMin = glm::min(glm::min(xCurve.m_xP0[i], xCurve.m_xP1[i]),
				glm::min(xCurve.m_xP2[i], xCurve.m_xP3[i]));
			const float fMax = glm::max(glm::max(xCurve.m_xP0[i], xCurve.m_xP1[i]),
				glm::max(xCurve.m_xP2[i], xCurve.m_xP3[i]));
			if (xPoint[i] < fMin - 1.0e-6f || xPoint[i] > fMax + 1.0e-6f)
			{
				return false;
			}
		}
		return true;
	}
}

ZENITH_TEST(FluxGrassTypes, BladeBezierMirrorsTheShaderCurve)
{
	const Flux_GrassBladeFrame xFrame = GrassMirrorTest_Frame();
	const Flux_GrassBladeCurve xCurve = GrassMirrorTest_Curve(fGrassMirrorHeight);

	// The frame the control points are built in. A side axis that is not
	// perpendicular to the facing skews every blade in the world.
	ZENITH_ASSERT_EQ_FLOAT(glm::length(xFrame.m_xGrow), 1.0f, 1.0e-5f, "the growth axis must be unit length");
	ZENITH_ASSERT_EQ_FLOAT(glm::length(xFrame.m_xSide), 1.0f, 1.0e-5f, "the width axis must be unit length");
	ZENITH_ASSERT_EQ_FLOAT(glm::dot(xFrame.m_xSide, xFrame.m_xFacing), 0.0f, 1.0e-6f,
		"the width axis must be perpendicular to the facing");
	ZENITH_ASSERT_EQ_FLOAT(xFrame.m_xSide.y, 0.0f, 0.0f, "the width axis is horizontal — the blade widens sideways, not upward");

	// Endpoints are EXACT in both languages: t = 0 multiplies P1..P3 by zero and
	// t = 1 multiplies P0..P2 by zero, so no rounding is involved either side.
	ZENITH_ASSERT_NEAR_VEC3(Flux_GrassBezierPoint(xCurve.m_xP0, xCurve.m_xP1, xCurve.m_xP2, xCurve.m_xP3, 0.0f),
		xCurve.m_xP0, 0.0f, "t = 0 must land exactly on P0 — the blade's root is authored, not interpolated");
	ZENITH_ASSERT_NEAR_VEC3(Flux_GrassBezierPoint(xCurve.m_xP0, xCurve.m_xP1, xCurve.m_xP2, xCurve.m_xP3, 1.0f),
		xCurve.m_xP3, 0.0f, "t = 1 must land exactly on P3 — the tip is where the full wind deflection lands");

	// The control-point construction itself. P1 sits a third of the way up the
	// growth axis with no bend, curl or wind on it; everything that shapes the
	// blade enters at P2 and P3.
	ZENITH_ASSERT_NEAR_VEC3(xCurve.m_xP0, Zenith_Maths::Vector3(0.0f), 0.0f, "P0 is the blade root");
	ZENITH_ASSERT_NEAR_VEC3(xCurve.m_xP1, xFrame.m_xGrow * (fGrassMirrorHeight / 3.0f), 1.0e-6f,
		"P1 must sit one third up the growth axis, unshaped");

	// Midpoint: inside the control hull, and strictly between root and tip.
	const Zenith_Maths::Vector3 xMid = Flux_GrassBezierPoint(xCurve.m_xP0, xCurve.m_xP1, xCurve.m_xP2, xCurve.m_xP3, 0.5f);
	ZENITH_ASSERT_TRUE(GrassMirrorTest_InsideHull(xCurve, xMid), "a cubic Bezier never leaves its control hull");
	ZENITH_ASSERT_GT(xMid.y, xCurve.m_xP0.y, "the blade's midpoint must stand above its root");
	ZENITH_ASSERT_LT(xMid.y, xCurve.m_xP3.y, "... and below its tip");

	// Tangents. At t = 0 the derivative is exactly 3(P1 - P0), i.e. the growth
	// axis: a blade leaves the ground along the direction it grew.
	const Zenith_Maths::Vector3 xTangent0 =
		Flux_GrassBezierTangent(xCurve.m_xP0, xCurve.m_xP1, xCurve.m_xP2, xCurve.m_xP3, 0.0f);
	ZENITH_ASSERT_GT(glm::length(xTangent0), 0.0f, "the tangent at the root must not be degenerate");
	ZENITH_ASSERT_NEAR_VEC3(glm::normalize(xTangent0), xFrame.m_xGrow, 1.0e-5f,
		"the root tangent must BE the growth axis");

	// The vertex stage normalizes this tangent to build the blade's normal, so a
	// zero anywhere along the spine would produce a NaN normal and a black blade.
	for (int i = 0; i <= 8; i++)
	{
		const float fT = static_cast<float>(i) * 0.125f;
		const Zenith_Maths::Vector3 xTangent =
			Flux_GrassBezierTangent(xCurve.m_xP0, xCurve.m_xP1, xCurve.m_xP2, xCurve.m_xP3, fT);
		ZENITH_ASSERT_GT(glm::length(xTangent), 0.0f, "the spine tangent must never vanish at t=%f", fT);
		ZENITH_ASSERT_GT(xTangent.y, 0.0f, "the spine must climb everywhere at t=%f — it must not fold back", fT);
	}

	// Purity: the pose is rebuilt from the record every frame, so a curve that
	// evaluated differently twice would flicker under TAA.
	const Flux_GrassBladeCurve xAgain = GrassMirrorTest_Curve(fGrassMirrorHeight);
	ZENITH_ASSERT_NEAR_VEC3(xAgain.m_xP2, xCurve.m_xP2, 0.0f, "the curve build must be a pure function");

	// ... and a taller blade must actually reach higher.
	const Flux_GrassBladeCurve xTall = GrassMirrorTest_Curve(fGrassMirrorHeight * 2.0f);
	ZENITH_ASSERT_GT(xTall.m_xP3.y, xCurve.m_xP3.y, "doubling the height must raise the tip");
}

ZENITH_TEST(FluxGrassTypes, BladeVertexTableIsTwoWeldedPieces)
{
	// Rows come in (left, right) PAIRS at one height, up to the lone tip vertex.
	for (u_int u = 0; u + 1u < Flux_GrassConfig::uBLADE_VERTEX_COUNT; u += 2u)
	{
		const Flux_GrassBladeVertex& xLeft = Flux_GrassConfig::axBLADE_VERTEX_TABLE[u];
		const Flux_GrassBladeVertex& xRight = Flux_GrassConfig::axBLADE_VERTEX_TABLE[u + 1u];
		ZENITH_ASSERT_EQ_FLOAT(xLeft.m_fU, xRight.m_fU, 0.0f, "row %u's two edges must sit at one height", u / 2u);
		ZENITH_ASSERT_EQ_FLOAT(xLeft.m_fSide, -1.0f, 0.0f, "row %u's first vertex is the left edge", u / 2u);
		ZENITH_ASSERT_EQ_FLOAT(xRight.m_fSide, 1.0f, 0.0f, "row %u's second vertex is the right edge", u / 2u);
		ZENITH_ASSERT_EQ(static_cast<u_int>(xLeft.m_uPiece), static_cast<u_int>(xRight.m_uPiece),
			"a row cannot straddle the two pieces (row %u)", u / 2u);
	}

	const Flux_GrassBladeVertex& xTip = Flux_GrassConfig::axBLADE_VERTEX_TABLE[Flux_GrassConfig::uBLADE_VERTEX_COUNT - 1u];
	ZENITH_ASSERT_EQ_FLOAT(xTip.m_fU, 1.0f, 0.0f, "the last vertex is the top of the blade");
	ZENITH_ASSERT_EQ_FLOAT(xTip.m_fSide, 0.0f, 0.0f, "the tip sits ON the spine — it is the one vertex with no side");

	u_int uPieceACount = 0u;
	u_int uPieceBCount = 0u;
	for (u_int u = 0; u < Flux_GrassConfig::uBLADE_VERTEX_COUNT; u++)
	{
		const u_int uPiece = static_cast<u_int>(Flux_GrassConfig::axBLADE_VERTEX_TABLE[u].m_uPiece);
		ZENITH_ASSERT_LT(uPiece, 2u, "vertex %u belongs to a piece that does not exist", u);
		(uPiece == 0u ? uPieceACount : uPieceBCount)++;
	}
	ZENITH_ASSERT_EQ(uPieceACount, 8u, "piece A is 4 rows x 2 columns");
	ZENITH_ASSERT_EQ(uPieceBCount, 7u, "piece B is 3 rows x 2 columns plus the tip");

	// Row heights climb with the vertex id INSIDE a piece. The one step that does
	// not is the A -> B seam, which is the whole point of the table.
	for (u_int u = 2; u < Flux_GrassConfig::uBLADE_VERTEX_COUNT; u++)
	{
		if (Flux_GrassConfig::axBLADE_VERTEX_TABLE[u].m_uPiece != Flux_GrassConfig::axBLADE_VERTEX_TABLE[u - 2u].m_uPiece)
		{
			continue;
		}
		ZENITH_ASSERT_GT(Flux_GrassConfig::axBLADE_VERTEX_TABLE[u].m_fU,
			Flux_GrassConfig::axBLADE_VERTEX_TABLE[u - 2u].m_fU,
			"row heights must climb inside a piece (vertex %u)", u);
	}

	// The per-type distribution exponent remaps u -> t. It has to be an
	// order-preserving map of [0,1] onto itself, or two rows would swap.
	float fPrevious = -1.0f;
	for (u_int u = 0; u < Flux_GrassConfig::uBLADE_VERTEX_COUNT; u++)
	{
		const float fT = Flux_GrassRemapHeightT(Flux_GrassConfig::axBLADE_VERTEX_TABLE[u].m_fU, 1.6f);
		ZENITH_ASSERT_GE(fT, 0.0f, "the remapped height must stay in [0,1] (vertex %u)", u);
		ZENITH_ASSERT_LE(fT, 1.0f, "the remapped height must stay in [0,1] (vertex %u)", u);
		ZENITH_ASSERT_GE(fT, fPrevious, "the remap must preserve row order (vertex %u)", u);
		fPrevious = fT;
	}
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassRemapHeightT(0.0f, 1.6f), 0.0f, 0.0f, "the base row stays at the base");
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassRemapHeightT(1.0f, 1.6f), 1.0f, 0.0f, "the tip row stays at the tip");
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassRemapHeightT(0.5f, 1.0f), 0.5f, 1.0e-6f, "an exponent of one must be the identity");
	ZENITH_ASSERT_GT(Flux_GrassRemapHeightT(0.5f, 1.6f), 0.5f,
		"an exponent above one must pack the rows toward the tip — that is what tessellates the curvature");
	// The remap divides by the exponent, so zero must be clamped rather than
	// producing an infinity that poses every row at the tip.
	ZENITH_ASSERT_LE(Flux_GrassRemapHeightT(0.5f, 0.0f), 1.0f, "a zero exponent must clamp, not produce an infinity");
	ZENITH_ASSERT_GE(Flux_GrassRemapHeightT(0.5f, 0.0f), 0.0f, "a zero exponent must clamp, not produce a NaN");

	// WELD IDENTITY. Unfolded, piece A's top edge (6, 7) and piece B's seam row
	// (8, 9) are the same two points — same height, same side — and nothing moves
	// either, so the two disconnected pieces read as one continuous surface.
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassConfig::axBLADE_VERTEX_TABLE[6].m_fU,
		Flux_GrassConfig::axBLADE_VERTEX_TABLE[8].m_fU, 0.0f, "the seam's left edges must share a height");
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassConfig::axBLADE_VERTEX_TABLE[7].m_fU,
		Flux_GrassConfig::axBLADE_VERTEX_TABLE[9].m_fU, 0.0f, "the seam's right edges must share a height");
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassConfig::axBLADE_VERTEX_TABLE[6].m_fSide,
		Flux_GrassConfig::axBLADE_VERTEX_TABLE[8].m_fSide, 0.0f, "the seam's left edges must share a side");
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassConfig::axBLADE_VERTEX_TABLE[7].m_fSide,
		Flux_GrassConfig::axBLADE_VERTEX_TABLE[9].m_fSide, 0.0f, "the seam's right edges must share a side");
	for (u_int u = 0; u < Flux_GrassConfig::uBLADE_VERTEX_COUNT; u++)
	{
		const Flux_GrassFoldOffset xUnfolded = Flux_GrassWeldOffset(u, false, 0.35f);
		ZENITH_ASSERT_EQ_FLOAT(xUnfolded.m_fAlongFacing, 0.0f, 0.0f, "an unfolded blade must not move vertex %u", u);
		ZENITH_ASSERT_EQ_FLOAT(xUnfolded.m_fAlongUp, 0.0f, 0.0f, "an unfolded blade must not move vertex %u", u);
	}

	// FOLDED: piece B moves RIGIDLY and piece A does not move at all. Both seam
	// edges take the identical offset, so the row translates instead of tearing.
	const float fPush = 0.35f;
	for (u_int u = 0; u < 8u; u++)
	{
		const Flux_GrassFoldOffset xPieceA = Flux_GrassWeldOffset(u, true, fPush);
		ZENITH_ASSERT_EQ_FLOAT(xPieceA.m_fAlongFacing, 0.0f, 0.0f, "piece A vertex %u must never receive the fold", u);
		ZENITH_ASSERT_EQ_FLOAT(xPieceA.m_fAlongUp, 0.0f, 0.0f, "piece A vertex %u must never receive the fold", u);
	}
	const Flux_GrassFoldOffset xSeamLeft = Flux_GrassWeldOffset(8u, true, fPush);
	const Flux_GrassFoldOffset xSeamRight = Flux_GrassWeldOffset(9u, true, fPush);
	ZENITH_ASSERT_EQ_FLOAT(xSeamLeft.m_fAlongFacing, fPush, 0.0f, "the fold pushes piece B along the blade's facing");
	ZENITH_ASSERT_EQ_FLOAT(xSeamLeft.m_fAlongUp, -fPush * 0.5f, 0.0f, "... and drops it by half as much");
	ZENITH_ASSERT_EQ_FLOAT(xSeamRight.m_fAlongFacing, xSeamLeft.m_fAlongFacing, 0.0f,
		"both seam edges must move identically — an asymmetric offset would shear the row apart");
	ZENITH_ASSERT_EQ_FLOAT(xSeamRight.m_fAlongUp, xSeamLeft.m_fAlongUp, 0.0f, "both seam edges must move identically");
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassWeldOffset(14u, true, fPush).m_fAlongFacing, fPush, 0.0f,
		"the tip belongs to piece B and folds with the rest of it");

	// The push anneals to zero as a blade converges on the LO silhouette, so it
	// has to be linear in the value the caller passes — including at zero.
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassWeldOffset(8u, true, fPush * 0.5f).m_fAlongFacing, fPush * 0.5f, 0.0f,
		"the fold must be linear in the push");
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassWeldOffset(8u, true, 0.0f).m_fAlongFacing, 0.0f, 0.0f,
		"a fully annealed fold must be exactly zero, not merely small");

	// The two HI index ranges must address ONE piece each, or "permanently
	// disconnected pieces" is false and a folded blade tears mid-quad.
	for (u_int u = 0; u < 18u; u++)
	{
		const u_int uVertex = Flux_GrassConfig::auBLADE_INDEX_TABLE[u];
		ZENITH_ASSERT_EQ(static_cast<u_int>(Flux_GrassConfig::axBLADE_VERTEX_TABLE[uVertex].m_uPiece), 0u,
			"HI index %u must address piece A", u);
	}
	for (u_int u = 18u; u < Flux_GrassConfig::uBLADE_HI_INDEX_COUNT; u++)
	{
		const u_int uVertex = Flux_GrassConfig::auBLADE_INDEX_TABLE[u];
		ZENITH_ASSERT_EQ(static_cast<u_int>(Flux_GrassConfig::axBLADE_VERTEX_TABLE[uVertex].m_uPiece), 1u,
			"HI index %u must address piece B", u);
	}

	// The LO strip deliberately reaches into piece B: its upper row is the seam
	// row, so a folded blade offsets the whole upper half of the strip together
	// and the strip skews instead of tearing.
	bool bLoReachesPieceB = false;
	for (u_int u = Flux_GrassConfig::uBLADE_LO_FIRST_INDEX; u < Flux_GrassConfig::uBLADE_INDEX_COUNT; u++)
	{
		const u_int uVertex = Flux_GrassConfig::auBLADE_INDEX_TABLE[u];
		bLoReachesPieceB = bLoReachesPieceB || (Flux_GrassConfig::axBLADE_VERTEX_TABLE[uVertex].m_uPiece != 0u);
	}
	ZENITH_ASSERT_TRUE(bLoReachesPieceB, "the LO strip must reach into piece B — its top row is the seam row and the tip");
}

ZENITH_TEST(FluxGrassTypes, ClumpPickFindsTheNearestOfNineSites)
{
	const float fScale = 3.0f;
	const u_int uSeed = 1337u;

	for (int i = 0; i < 12; i++)
	{
		const float fWorldX = static_cast<float>(i) * 1.7f - 5.0f;
		const float fWorldZ = static_cast<float>(i) * -2.3f + 4.0f;
		const Zenith_Maths::Vector2 xWorld(fWorldX, fWorldZ);
		const Flux_GrassClump xClump = Flux_GrassClumpPick(fWorldX, fWorldZ, fScale, uSeed);

		// Brute-force the SAME nine cells through the same site function: the pick
		// has to be the nearest, not merely a plausible one. A 2x2 search passes a
		// centred sample and fails exactly here, on the corner cases — which is
		// what produces visible clump seams along the cell grid.
		const int iCellX = static_cast<int>(floorf(fWorldX / fScale));
		const int iCellZ = static_cast<int>(floorf(fWorldZ / fScale));
		float fBestDistSq = 1.0e30f;
		Zenith_Maths::Vector2 xBest(0.0f, 0.0f);
		for (int iDZ = -1; iDZ <= 1; iDZ++)
		{
			for (int iDX = -1; iDX <= 1; iDX++)
			{
				const Flux_GrassClumpCell xCell = Flux_GrassClumpSite(iCellX + iDX, iCellZ + iDZ, fScale, uSeed);
				const Zenith_Maths::Vector2 xDelta = xWorld - xCell.m_xSite;
				const float fDistSq = glm::dot(xDelta, xDelta);
				if (fDistSq < fBestDistSq)
				{
					fBestDistSq = fDistSq;
					xBest = xCell.m_xSite;
				}
			}
		}
		ZENITH_ASSERT_EQ_FLOAT(xClump.m_xCentre.x, xBest.x, 0.0f, "clump centre X at world (%f, %f)", fWorldX, fWorldZ);
		ZENITH_ASSERT_EQ_FLOAT(xClump.m_xCentre.y, xBest.y, 0.0f, "clump centre Z at world (%f, %f)", fWorldX, fWorldZ);

		ZENITH_ASSERT_GE(xClump.m_fDist01, 0.0f, "the normalized clump distance must not go negative");
		ZENITH_ASSERT_LE(xClump.m_fDist01, 1.0f, "the normalized clump distance must saturate at the cell size");
		ZENITH_ASSERT_EQ_FLOAT(glm::length(xClump.m_xNormalXZ), 1.0f, 1.0e-5f,
			"the outward direction must be unit length — the blade record carries it verbatim");
		ZENITH_ASSERT_GE(xClump.m_fHash01, 0.0f, "the clump hash is a [0,1) roll");
		ZENITH_ASSERT_LT(xClump.m_fHash01, 1.0f, "the clump hash is a [0,1) roll");

		// Rebuild stability. A clump that moved between frames would drag every
		// blade in it, which reads as the whole field crawling.
		const Flux_GrassClump xAgain = Flux_GrassClumpPick(fWorldX, fWorldZ, fScale, uSeed);
		ZENITH_ASSERT_EQ_FLOAT(xAgain.m_xCentre.x, xClump.m_xCentre.x, 0.0f, "the clump pick must be a pure function");
		ZENITH_ASSERT_EQ_FLOAT(xAgain.m_xCentre.y, xClump.m_xCentre.y, 0.0f, "the clump pick must be a pure function");
		ZENITH_ASSERT_EQ_FLOAT(xAgain.m_fHash01, xClump.m_fHash01, 0.0f, "the clump hash must be a pure function");
		ZENITH_ASSERT_EQ_FLOAT(xAgain.m_fDist01, xClump.m_fDist01, 0.0f, "the clump distance must be a pure function");
	}

	// The seed must actually be an input, or "seeded" is a lie. Checked over a
	// sweep rather than one point: two hashes agreeing at a single position is a
	// coincidence, not a contract.
	bool bSeedMatters = false;
	for (int i = 0; i < 8 && !bSeedMatters; i++)
	{
		const float fWorld = static_cast<float>(i) * 1.3f;
		bSeedMatters = Flux_GrassClumpPick(fWorld, fWorld, fScale, uSeed).m_fHash01
			!= Flux_GrassClumpPick(fWorld, fWorld, fScale, uSeed + 1u).m_fHash01;
	}
	ZENITH_ASSERT_TRUE(bSeedMatters, "two different seeds must not lay down the identical clump lattice");

	// The chosen centre must be REACHABLE. A blade's own cell always holds a site
	// somewhere inside it, so the nearest of the nine can never be further than the
	// cell diagonal — a pick beyond that would mean the search left its
	// neighbourhood, which is the failure a 2x2 search actually produces.
	for (int i = 0; i < 8; i++)
	{
		const float fWorld = static_cast<float>(i) * 4.9f - 11.0f;
		const Flux_GrassClump xClump = Flux_GrassClumpPick(fWorld, fWorld, fScale, uSeed);
		const float fDistance = glm::length(Zenith_Maths::Vector2(fWorld, fWorld) - xClump.m_xCentre);
		ZENITH_ASSERT_LE(fDistance, fScale * 1.4143f,
			"the picked clump centre must lie within one cell diagonal of the blade (world %f)", fWorld);
	}

	// A degenerate cell size divides the world position, so it must clamp rather
	// than place every blade in one clump at infinity.
	const Flux_GrassClump xDegenerate = Flux_GrassClumpPick(1.0f, 1.0f, 0.0f, uSeed);
	ZENITH_ASSERT_GE(xDegenerate.m_fDist01, 0.0f, "a zero cell size must clamp, not produce a NaN");
	ZENITH_ASSERT_LE(xDegenerate.m_fDist01, 1.0f, "a zero cell size must clamp, not produce a NaN");
	ZENITH_ASSERT_EQ_FLOAT(glm::length(xDegenerate.m_xNormalXZ), 1.0f, 1.0e-5f,
		"a zero cell size must still yield a usable outward direction");
}

ZENITH_TEST(FluxGrassTypes, TypeGatherPicksAWholeTexelFromTheFootprint)
{
	// The four gather weights are used as a PROBABILITY DISTRIBUTION, so they have
	// to be one: a set that did not sum to 1 would bias the last bucket.
	const float afFracs[4] = { 0.0f, 0.25f, 0.5f, 1.0f };
	for (u_int uX = 0; uX < 4u; uX++)
	{
		for (u_int uZ = 0; uZ < 4u; uZ++)
		{
			float afWeight[4];
			Flux_GrassTypeGatherWeights(afFracs[uX], afFracs[uZ], afWeight);
			float fSum = 0.0f;
			for (u_int u = 0; u < 4u; u++)
			{
				ZENITH_ASSERT_GE(afWeight[u], 0.0f, "a gather weight must not go negative");
				ZENITH_ASSERT_LE(afWeight[u], 1.0f, "a gather weight must not exceed one");
				fSum += afWeight[u];
			}
			ZENITH_ASSERT_EQ_FLOAT(fSum, 1.0f, 1.0e-6f, "the four gather weights must be a distribution");
		}
	}

	// A 2x2 index map over 2 m with the SAME two types in both rows, so the pick
	// depends on X alone. Types 0 and 5 are adjacent on purpose: any interpolation
	// would invent 1 to 4, types the author never placed anywhere.
	const u_int8 aucTypes[4] = { 0u, 5u, 0u, 5u };
	Flux_GrassMap xMap;
	xMap.m_pData = aucTypes;
	xMap.m_uWidth = 2u;
	xMap.m_uHeight = 2u;
	xMap.m_fWorldSize = 2.0f;
	xMap.m_eFormat = Flux_GrassMapFormat::U8;

	// Sitting exactly ON texel 0, the footprint has no weight anywhere else.
	for (u_int uHash = 0; uHash < 64u; uHash++)
	{
		ZENITH_ASSERT_EQ(Flux_GrassSampleTypeDithered(xMap, 0.5f, 0.5f, uHash, 8u), 0u,
			"a sample centred on texel 0 must not dither off it (hash %u)", uHash);
	}

	// On the boundary the footprint straddles both texels: every pick must be one
	// of them, and over enough rolls it must be BOTH — that stochastic mix is what
	// dissolves the boundary instead of drawing a hard line across the terrain.
	bool bSawNear = false;
	bool bSawFar = false;
	for (u_int uHash = 0; uHash < 64u; uHash++)
	{
		const u_int uType = Flux_GrassSampleTypeDithered(xMap, 1.0f, 0.5f, uHash, 8u);
		ZENITH_ASSERT_TRUE(uType == 0u || uType == 5u,
			"the boundary pick produced %u, a type outside the gathered footprint (hash %u)", uType, uHash);
		ZENITH_ASSERT_EQ(Flux_GrassSampleTypeDithered(xMap, 1.0f, 0.5f, uHash, 8u), uType,
			"the pick must be a pure function of position and hash (hash %u)", uHash);
		bSawNear = bSawNear || (uType == 0u);
		bSawFar = bSawFar || (uType == 5u);
	}
	ZENITH_ASSERT_TRUE(bSawNear, "the boundary must sometimes take the near texel");
	ZENITH_ASSERT_TRUE(bSawFar, "the boundary must sometimes take the far texel");

	// The LIVE type count clamps the byte: a map painted with a type the table no
	// longer carries must fall back to the last live type, never index off the end.
	ZENITH_ASSERT_EQ(Flux_GrassSampleTypeDithered(xMap, 1.5f, 0.5f, 7u, 4u), 3u,
		"a byte above the live count must clamp to the last type");
	ZENITH_ASSERT_EQ(Flux_GrassSampleTypeDithered(xMap, 1.5f, 0.5f, 7u, 1u), 0u,
		"a single-type table can only ever select type 0");
	ZENITH_ASSERT_EQ(Flux_GrassSampleTypeDithered(xMap, 1.5f, 0.5f, 7u, 0u), 0u,
		"a zero type count must behave as one type, not underflow the clamp");

	// An unpainted map is meadow everywhere, whatever the roll.
	const u_int8 aucZeros[4] = { 0u, 0u, 0u, 0u };
	Flux_GrassMap xZeroMap = xMap;
	xZeroMap.m_pData = aucZeros;
	for (u_int uHash = 0; uHash < 32u; uHash++)
	{
		ZENITH_ASSERT_EQ(Flux_GrassSampleTypeDithered(xZeroMap, 1.0f, 1.0f, uHash, 8u), 0u,
			"an all-zero type map must select type 0 everywhere (hash %u)", uHash);
	}

	// An absent or non-U8 map is not a type map: the raw byte semantics do not
	// survive a float or u16 payload, so the pick refuses rather than reinterpreting.
	const Flux_GrassMap xNull;
	ZENITH_ASSERT_EQ(Flux_GrassSampleTypeDithered(xNull, 0.0f, 0.0f, 3u, 8u), 0u, "an unset type map must pick type 0");
	Flux_GrassMap xWrongFormat = xMap;
	xWrongFormat.m_eFormat = Flux_GrassMapFormat::F32;
	ZENITH_ASSERT_EQ(Flux_GrassSampleTypeDithered(xWrongFormat, 1.0f, 0.5f, 3u, 8u), 0u,
		"a non-U8 map must not be reinterpreted as type indices");
}

// ============================================================================
// Displacement anchor + decay. The first three tests pin the ONE property the
// whole trail field rests on: the map's origin moves in WHOLE TEXELS.
//
// It is not an optimisation. The displacement CS re-anchors by reading the
// previous map at (texel + shift); a fractional shift would make that a
// resample, so every frame would run the field through a filter and a trail
// would blur itself away while the camera stood still. Integrality is therefore
// a correctness property, and it is pinned from three directions: the anchor does
// not move for a sub-texel camera move, it moves by exactly one across a
// boundary, and the shift the constant buffer carries is always integral.
//
// The fourth pins the other half of the field's behaviour over time — that the
// decay is derived from dt, so a trail has the same LIFETIME whatever the frame
// rate, and that degenerate inputs freeze the field rather than erasing it.
// ============================================================================

ZENITH_TEST(FluxGrassTypes, DisplacementAnchorIgnoresSubTexelCameraMotion)
{
	constexpr float fTexel = Flux_GrassConfig::fDISPLACEMENT_TEXEL_SIZE;
	ZENITH_ASSERT_EQ_FLOAT(fTexel, 0.25f, 1.0e-6f, "the map is 64 m over 256 texels");

	// A texel-aligned camera, then nudges that all stay inside the SAME texel. The
	// last one is a hair under a full texel: the anchor must not have moved yet.
	const float fBaseX = 100.0f;
	const float fBaseZ = -37.5f;
	const Flux_GrassDisplacementAnchor xBase = Flux_GrassSnapDisplacementAnchor(fBaseX, fBaseZ);

	const float afNudge[] = { 0.0f, 0.01f, fTexel * 0.5f, fTexel * 0.99f };
	for (float fNudge : afNudge)
	{
		const Flux_GrassDisplacementAnchor xMoved = Flux_GrassSnapDisplacementAnchor(fBaseX + fNudge, fBaseZ + fNudge);
		ZENITH_ASSERT_TRUE(xMoved == xBase,
			"a camera move inside one texel must leave the anchor exactly where it was — a moving anchor "
			"resamples the whole field");
		ZENITH_ASSERT_TRUE(Flux_GrassDisplacementTexelShift(xBase, xMoved) == Zenith_Maths::Vector2(0.0f, 0.0f),
			"an unmoved anchor must produce a zero scroll, not a rounding-error scroll");
	}

	// The world origin the anchor projects to is an exact multiple of the texel size,
	// which is what the CS's world -> texel mapping assumes.
	const Zenith_Maths::Vector2 xOrigin = Flux_GrassDisplacementOriginWS(xBase);
	ZENITH_ASSERT_EQ_FLOAT(xOrigin.x, static_cast<float>(xBase.m_iTexelX) * fTexel, 1.0e-6f,
		"the map origin must be a whole number of texels from the world origin");
	ZENITH_ASSERT_EQ_FLOAT(xOrigin.y, static_cast<float>(xBase.m_iTexelZ) * fTexel, 1.0e-6f,
		"the map origin must be a whole number of texels from the world origin");

	// ...and it centres the map on the camera to within one texel, which is the other
	// half of the contract: the snap must not drift the footprint off the viewer.
	const float fHalf = Flux_GrassConfig::fDISPLACEMENT_WORLD_SIZE * 0.5f;
	ZENITH_ASSERT_LE(fabsf((xOrigin.x + fHalf) - fBaseX), fTexel, "the map must stay centred on the camera");
	ZENITH_ASSERT_LE(fabsf((xOrigin.y + fHalf) - fBaseZ), fTexel, "the map must stay centred on the camera");
}

ZENITH_TEST(FluxGrassTypes, DisplacementAnchorStepsOneTexelPerBoundary)
{
	constexpr float fTexel = Flux_GrassConfig::fDISPLACEMENT_TEXEL_SIZE;

	// Start exactly on a texel boundary so the crossing point is unambiguous: the
	// snap is a floor of (camera - half) / texel, so an anchor-aligned camera sits at
	// (anchor texel) * texel + half.
	const Flux_GrassDisplacementAnchor xStart = Flux_GrassSnapDisplacementAnchor(0.0f, 0.0f);
	const Zenith_Maths::Vector2 xStartOrigin = Flux_GrassDisplacementOriginWS(xStart);
	const float fHalf = Flux_GrassConfig::fDISPLACEMENT_WORLD_SIZE * 0.5f;
	const float fAlignedX = xStartOrigin.x + fHalf;
	const float fAlignedZ = xStartOrigin.y + fHalf;

	// One whole texel of camera travel is exactly one texel of anchor travel — never
	// two, which is what floor buys over round-to-nearest.
	for (int iStep = 1; iStep <= 8; iStep++)
	{
		const float fMove = fTexel * static_cast<float>(iStep);
		const Flux_GrassDisplacementAnchor xMoved = Flux_GrassSnapDisplacementAnchor(fAlignedX + fMove, fAlignedZ - fMove);
		ZENITH_ASSERT_EQ(xMoved.m_iTexelX - xStart.m_iTexelX, iStep, "N texels of camera travel must move the anchor N texels");
		ZENITH_ASSERT_EQ(xMoved.m_iTexelZ - xStart.m_iTexelZ, -iStep, "the snap must be symmetric under a negative move");
	}

	// Just short of the first boundary: still the starting anchor. Just past it:
	// exactly one texel, so there is no step the anchor can skip.
	const Flux_GrassDisplacementAnchor xJustShort =
		Flux_GrassSnapDisplacementAnchor(fAlignedX + fTexel * 0.999f, fAlignedZ);
	ZENITH_ASSERT_EQ(xJustShort.m_iTexelX, xStart.m_iTexelX, "the anchor must not step before the boundary");

	const Flux_GrassDisplacementAnchor xJustPast =
		Flux_GrassSnapDisplacementAnchor(fAlignedX + fTexel * 1.001f, fAlignedZ);
	ZENITH_ASSERT_EQ(xJustPast.m_iTexelX - xStart.m_iTexelX, 1, "crossing one boundary must step exactly one texel");
}

ZENITH_TEST(FluxGrassTypes, DisplacementTexelShiftIsAlwaysIntegral)
{
	constexpr float fTexel = Flux_GrassConfig::fDISPLACEMENT_TEXEL_SIZE;

	// Walk a camera along an irrational-ish path so consecutive positions land at
	// arbitrary sub-texel offsets, and check the scroll the constant buffer carries
	// is integral at EVERY step. A fractional scroll is the failure mode the whole
	// snap exists to prevent, and it would never announce itself — the field would
	// just quietly soften.
	Flux_GrassDisplacementAnchor xPrev = Flux_GrassSnapDisplacementAnchor(2048.0f, 2048.0f);
	for (int iStep = 1; iStep <= 64; iStep++)
	{
		const float fT = static_cast<float>(iStep);
		const float fX = 2048.0f + fT * 0.137f + sinf(fT) * 1.7f;
		const float fZ = 2048.0f - fT * 0.311f + cosf(fT * 0.5f) * 2.3f;

		const Flux_GrassDisplacementAnchor xNext = Flux_GrassSnapDisplacementAnchor(fX, fZ);
		const Zenith_Maths::Vector2 xShift = Flux_GrassDisplacementTexelShift(xPrev, xNext);

		ZENITH_ASSERT_EQ_FLOAT(xShift.x, floorf(xShift.x), 0.0f, "the scroll the CB carries must be a whole texel count");
		ZENITH_ASSERT_EQ_FLOAT(xShift.y, floorf(xShift.y), 0.0f, "the scroll the CB carries must be a whole texel count");
		ZENITH_ASSERT_EQ(static_cast<int>(xShift.x), xNext.m_iTexelX - xPrev.m_iTexelX,
			"the float scroll must be the integer anchor difference widened, never a metre division");
		ZENITH_ASSERT_EQ(static_cast<int>(xShift.y), xNext.m_iTexelZ - xPrev.m_iTexelZ,
			"the float scroll must be the integer anchor difference widened, never a metre division");

		// ...and the two metre origins agree with that scroll to the last bit, which
		// is what lets the CS use either representation without them disagreeing.
		const Zenith_Maths::Vector2 xPrevOrigin = Flux_GrassDisplacementOriginWS(xPrev);
		const Zenith_Maths::Vector2 xNextOrigin = Flux_GrassDisplacementOriginWS(xNext);
		ZENITH_ASSERT_EQ_FLOAT(xNextOrigin.x - xPrevOrigin.x, xShift.x * fTexel, 1.0e-3f,
			"the metre origins and the texel scroll must describe the same slide");
		ZENITH_ASSERT_EQ_FLOAT(xNextOrigin.y - xPrevOrigin.y, xShift.y * fTexel, 1.0e-3f,
			"the metre origins and the texel scroll must describe the same slide");

		xPrev = xNext;
	}
}

ZENITH_TEST(FluxGrassTypes, DisplacementDecayIsFrameRateIndependent)
{
	// The retention factor is derived from dt so a trail has the same LIFETIME on a
	// fast and a slow machine. Two 1/120 s steps must therefore retain as much as one
	// 1/60 s step, or the field would fade at whatever rate the frame happened to run.
	const float fOneStep = Flux_GrassDisplacementDecay(1.0f / 60.0f, 0.5f);
	const float fHalfStep = Flux_GrassDisplacementDecay(1.0f / 120.0f, 0.5f);
	ZENITH_ASSERT_EQ_FLOAT(fHalfStep * fHalfStep, fOneStep, 1.0e-6f,
		"two half-length steps must retain exactly as much as one whole step");

	ZENITH_ASSERT_LT(fOneStep, 1.0f, "a frame of decay must actually remove something");
	ZENITH_ASSERT_GT(fOneStep, 0.9f, "a half-second e-fold must not erase a 60 Hz frame's trail outright");

	// One e-folding time retains 1/e, which is the definition the tuning constant is
	// authored against.
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassDisplacementDecay(0.5f, 0.5f), 0.36787944f, 1.0e-5f,
		"one e-folding time must retain exactly 1/e");

	// Degenerate inputs retain EVERYTHING rather than wiping the field: a paused frame
	// (dt 0) must freeze the trail, not delete it.
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassDisplacementDecay(0.0f, 0.5f), 1.0f, 0.0f, "a zero-length frame must decay nothing");
	ZENITH_ASSERT_EQ_FLOAT(Flux_GrassDisplacementDecay(1.0f / 60.0f, 0.0f), 1.0f, 0.0f,
		"a zero e-fold must degrade to 'no decay', never to a divide by zero");
}

// ============================================================================
// Flux_GrassImpl — the live feature.
//
// BuildFromMaps REJECTS any dimension but the fixed texture extents: the map
// textures are created ONCE at those extents and updated in place, so a resample
// would misalign the CPU query surface against the GPU texels. There is therefore
// no "small" legal map set — the smallest legal one is a 4096^2 float
// heightfield, a 1024^2 float coverage map and a 1024^2 type-byte map, roughly
// 69 MB.
//
// It is owned by a LOCAL in each test that needs it, not by a shared static: a
// function-local static would still hold those 69 MB when
// Zenith_MemoryManagement's shutdown leak checkpoint runs, and would be reported
// as a leak forever after. BuildFromMaps copies (and quantizes) everything it is
// handed and retains no pointer, so a source that dies with the test is fine.
// ============================================================================

namespace
{
	constexpr u_int uGrassTestHeightSize = 4096u;
	constexpr u_int uGrassTestCoverageSize = 1024u;
	constexpr u_int uGrassTestTypeSize = 1024u;
	constexpr float fGrassTestWorldSize = 4096.0f;    // exactly 1 m per height texel
	constexpr float fGrassTestCoverage = 0.5f;
	constexpr float fGrassTestHeightRange = 100.0f;   // metres, across the full +Z span

	// The authored metres at a world Z, before quantization. Texel k sits at world
	// k (1 m per texel), so this is the same ramp GrassTestMaps writes.
	float GrassTest_ExpectedHeight(float fWorldZ)
	{
		return fWorldZ * (fGrassTestHeightRange / static_cast<float>(uGrassTestHeightSize - 1u));
	}

	// Height ramps linearly along +Z so the bilinear query has something to
	// interpolate — a flat map would pass a sampler that ignored its fraction.
	// Coverage is uniform, and the type map is all zeros (type 0, Meadow).
	struct GrassTestMaps
	{
		GrassTestMaps()
		{
			m_afHeight.Resize(uGrassTestHeightSize * uGrassTestHeightSize, 0.0f);
			float* pfHeight = m_afHeight.GetDataPointer();
			for (u_int uZ = 0; uZ < uGrassTestHeightSize; uZ++)
			{
				const float fRow = GrassTest_ExpectedHeight(static_cast<float>(uZ));
				float* pfRow = pfHeight + static_cast<size_t>(uZ) * uGrassTestHeightSize;
				for (u_int uX = 0; uX < uGrassTestHeightSize; uX++)
				{
					pfRow[uX] = fRow;
				}
			}
			m_afCoverage.Resize(uGrassTestCoverageSize * uGrassTestCoverageSize, fGrassTestCoverage);
			m_aucType.Resize(uGrassTestTypeSize * uGrassTestTypeSize, static_cast<u_int8>(0));
		}

		// An ABSENT type map is normal content (a terrain set baked before the map
		// existed simply has no file), so both shapes have to be constructible.
		Flux_GrassImpl::MapSet Get(bool bWithTypeMap) const
		{
			Flux_GrassImpl::MapSet xMaps;
			xMaps.pHeight = m_afHeight.GetDataPointer();
			xMaps.uHeightSize = uGrassTestHeightSize;
			xMaps.pCoverage = m_afCoverage.GetDataPointer();
			xMaps.uCoverageSize = uGrassTestCoverageSize;
			xMaps.pType = bWithTypeMap ? m_aucType.GetDataPointer() : nullptr;
			xMaps.uTypeSize = bWithTypeMap ? uGrassTestTypeSize : 0u;
			xMaps.fWorldSize = fGrassTestWorldSize;
			return xMaps;
		}

		Zenith_Vector<float>  m_afHeight;
		Zenith_Vector<float>  m_afCoverage;
		Zenith_Vector<u_int8> m_aucType;
	};
}

ZENITH_TEST(FluxGrassImpl, Build_RejectsInvalidInput_StateIntact)
{
	Flux_GrassImpl& xGrass = g_xEngine.Grass();
	const float fDensityScaleBefore = xGrass.GetDensityScale();
	xGrass.ClearSceneData();
	ZENITH_ASSERT_FALSE(xGrass.IsBuilt(), "the baseline for this test is an unbuilt world");

	const Flux_GrassImpl::BuildParams xParams;
	const GrassTestMaps xSource;
	const Flux_GrassImpl::MapSet xEmpty;

	Flux_GrassImpl::MapSet xNoCoverage = xSource.Get(false);
	xNoCoverage.pCoverage = nullptr;
	Flux_GrassImpl::MapSet xWrongHeightSize = xSource.Get(false);
	xWrongHeightSize.uHeightSize = uGrassTestHeightSize / 2u;
	Flux_GrassImpl::MapSet xZeroWorld = xSource.Get(false);
	xZeroWorld.fWorldSize = 0.0f;
	Flux_GrassImpl::MapSet xWrongTypeSize = xSource.Get(true);
	xWrongTypeSize.uTypeSize = 64u;

	// From UNBUILT. Every one of these is rejected during validation, before a
	// single texel is written.
	ZENITH_ASSERT_FALSE(xGrass.BuildFromMaps(xEmpty, xParams), "a default (all-null) map set must be rejected");
	ZENITH_ASSERT_FALSE(xGrass.BuildFromMaps(xNoCoverage, xParams), "a null coverage map must be rejected");
	ZENITH_ASSERT_FALSE(xGrass.BuildFromMaps(xWrongHeightSize, xParams),
		"the map textures are created once at fixed extents, so a differently-sized heightfield must be rejected rather than resampled");
	ZENITH_ASSERT_FALSE(xGrass.BuildFromMaps(xZeroWorld, xParams), "a map covering no world must be rejected");
	ZENITH_ASSERT_FALSE(xGrass.BuildFromMaps(xWrongTypeSize, xParams), "a mis-sized type map must be rejected");
	ZENITH_ASSERT_FALSE(xGrass.IsBuilt(), "no rejection may have marked the world built");
	ZENITH_ASSERT_FALSE(xGrass.HasCoverageMap(), "no rejection may have installed a coverage map");

	// Now build for real and repeat the rejections: the PRIOR world has to survive
	// them untouched, which is the whole point of validating before writing.
	ZENITH_ASSERT_TRUE(xGrass.BuildFromMaps(xSource.Get(true), xParams), "the reference map set must build");
	ZENITH_ASSERT_TRUE(xGrass.IsBuilt(), "a valid build must mark the world built");
	const float fCoverageBefore = xGrass.SampleGrassCoverage(100.0f, 100.0f);
	const float fHeightBefore = xGrass.SampleGrassHeight(100.0f, 1000.0f);

	ZENITH_ASSERT_FALSE(xGrass.BuildFromMaps(xEmpty, xParams), "an all-null rebuild must still be rejected");
	ZENITH_ASSERT_FALSE(xGrass.BuildFromMaps(xNoCoverage, xParams), "a null-coverage rebuild must still be rejected");
	ZENITH_ASSERT_FALSE(xGrass.BuildFromMaps(xWrongHeightSize, xParams), "a mis-sized rebuild must still be rejected");
	ZENITH_ASSERT_FALSE(xGrass.BuildFromMaps(xZeroWorld, xParams), "a zero-world rebuild must still be rejected");
	ZENITH_ASSERT_FALSE(xGrass.BuildFromMaps(xWrongTypeSize, xParams), "a mis-sized-type rebuild must still be rejected");

	ZENITH_ASSERT_TRUE(xGrass.IsBuilt(), "a rejected rebuild must leave the previous world built");
	ZENITH_ASSERT_EQ(xGrass.GetCoverageMapSize(), uGrassTestCoverageSize, "... with its coverage map still installed");
	ZENITH_ASSERT_EQ_FLOAT(xGrass.SampleGrassCoverage(100.0f, 100.0f), fCoverageBefore, 0.0f,
		"a rejected rebuild must not disturb a single texel of the previous world");
	ZENITH_ASSERT_EQ_FLOAT(xGrass.SampleGrassHeight(100.0f, 1000.0f), fHeightBefore, 0.0f,
		"a rejected rebuild must not disturb the previous heightfield");

	xGrass.SetDensityScale(fDensityScaleBefore);
	xGrass.ClearSceneData();
}

ZENITH_TEST(FluxGrassImpl, Build_AllZeroTypeMapIsMeadowEverywhere)
{
	Flux_GrassImpl& xGrass = g_xEngine.Grass();
	const float fDensityScaleBefore = xGrass.GetDensityScale();
	xGrass.ClearSceneData();
	const Flux_GrassImpl::BuildParams xParams;
	const GrassTestMaps xSource;
	const float afProbe[5] = { 0.0f, 1.0f, 1234.5f, 4095.0f, 9999.0f };

	// An ABSENT type map is normal content, not an error: a terrain set baked
	// before the map existed simply has no file, and type 0 is what it must read as.
	ZENITH_ASSERT_TRUE(xGrass.BuildFromMaps(xSource.Get(false), xParams), "a build with no type map must succeed");
	ZENITH_ASSERT_TRUE(xGrass.IsBuilt(), "a build with no type map must still mark the world built");
	for (u_int u = 0; u < 5u; u++)
	{
		ZENITH_ASSERT_EQ(static_cast<u_int>(xGrass.SampleGrassType(afProbe[u], afProbe[4u - u])), 0u,
			"an absent type map must read as type 0 at x=%f", afProbe[u]);
	}

	// ... and an explicitly all-zero map must be indistinguishable from an absent one.
	ZENITH_ASSERT_TRUE(xGrass.BuildFromMaps(xSource.Get(true), xParams), "a build with an all-zero type map must succeed");
	for (u_int u = 0; u < 5u; u++)
	{
		ZENITH_ASSERT_EQ(static_cast<u_int>(xGrass.SampleGrassType(afProbe[u], afProbe[4u - u])), 0u,
			"an all-zero type map must read as type 0 at x=%f", afProbe[u]);
	}

	// Coverage reads back what was painted, to the 8-bit texture's precision. The
	// CPU copy IS the bytes the texture holds, so this is the GPU's view too.
	ZENITH_ASSERT_TRUE(xGrass.HasCoverageMap(), "a built world must report its coverage map");
	ZENITH_ASSERT_EQ(xGrass.GetCoverageMapSize(), uGrassTestCoverageSize, "the coverage map keeps its authored size");
	ZENITH_ASSERT_EQ_FLOAT(xGrass.GetCoverageWorldSize(), fGrassTestWorldSize, 0.0f, "... and its authored world footprint");
	ZENITH_ASSERT_EQ_FLOAT(xGrass.SampleGrassCoverage(512.0f, 512.0f), fGrassTestCoverage, 1.0f / 255.0f,
		"coverage must read back what was painted");
	ZENITH_ASSERT_EQ_FLOAT(xGrass.SampleGrassCoverage(-100.0f, -100.0f), fGrassTestCoverage, 1.0f / 255.0f,
		"off the map, coverage clamps to the edge texel rather than falling to zero");

	// Height is BILINEAR and in metres. The map ramps along +Z, so a whole-texel
	// probe pins the ramp and a half-texel probe pins the interpolation itself.
	ZENITH_ASSERT_EQ_FLOAT(xGrass.SampleGrassHeight(37.0f, 1000.0f), GrassTest_ExpectedHeight(1000.0f), 0.01f,
		"a whole-texel height probe must return the authored metres");
	ZENITH_ASSERT_EQ_FLOAT(xGrass.SampleGrassHeight(37.0f, 1000.5f), GrassTest_ExpectedHeight(1000.5f), 0.01f,
		"a half-texel probe must land half way between two rows — the height query is bilinear");
	ZENITH_ASSERT_EQ_FLOAT(xGrass.SampleGrassHeight(37.0f, 0.0f), 0.0f, 0.01f, "the ramp starts at zero");
	ZENITH_ASSERT_GT(xGrass.SampleGrassHeight(37.0f, 2000.0f), xGrass.SampleGrassHeight(37.0f, 1000.0f),
		"the heightfield must climb along +Z, not along the row-major index");

	xGrass.SetDensityScale(fDensityScaleBefore);
	xGrass.ClearSceneData();
}

ZENITH_TEST(FluxGrassImpl, SamplesReturnZeroUnbuilt)
{
	Flux_GrassImpl& xGrass = g_xEngine.Grass();
	xGrass.ClearSceneData();

	// ALL THREE samplers return 0 with no map. The retired SampleDensityMap
	// returned a neutral 1.0 for "no map", which silently made an unbuilt world
	// read as fully covered; a caller with no data has to decide what that means,
	// so this flip is deliberate and is pinned here.
	const float afProbe[4] = { -1000.0f, 0.0f, 128.5f, 1.0e6f };
	for (u_int u = 0; u < 4u; u++)
	{
		ZENITH_ASSERT_EQ_FLOAT(xGrass.SampleGrassCoverage(afProbe[u], afProbe[u]), 0.0f, 0.0f,
			"unbuilt coverage must be zero, never a neutral one (probe %f)", afProbe[u]);
		ZENITH_ASSERT_EQ(static_cast<u_int>(xGrass.SampleGrassType(afProbe[u], afProbe[u])), 0u,
			"unbuilt type must be zero (probe %f)", afProbe[u]);
		ZENITH_ASSERT_EQ_FLOAT(xGrass.SampleGrassHeight(afProbe[u], afProbe[u]), 0.0f, 0.0f,
			"unbuilt height must be zero, not the stale height bias (probe %f)", afProbe[u]);
	}

	ZENITH_ASSERT_FALSE(xGrass.IsBuilt(), "a cleared world is not built");
	ZENITH_ASSERT_FALSE(xGrass.HasCoverageMap(), "a cleared world holds no coverage map");
	ZENITH_ASSERT_EQ(xGrass.GetCoverageMapSize(), 0u, "a cleared world's coverage map has no extent");
	ZENITH_ASSERT_EQ_FLOAT(xGrass.GetCoverageWorldSize(), 0.0f, 0.0f, "a cleared world covers no world");

	ZENITH_ASSERT_EQ(xGrass.GetScheduledInstanceCount(), 0u, "a cleared world schedules no lattice cells");
	ZENITH_ASSERT_EQ(xGrass.GetVisibleTileCount(), 0u, "a cleared world dispatches no tiles");
	ZENITH_ASSERT_EQ(xGrass.GetTileCount(), 0u, "a cleared world culls no tiles");
	ZENITH_ASSERT_EQ(xGrass.GetSubmittedDrawCount(), 0u, "a cleared world submits no draws");
	ZENITH_ASSERT_EQ(xGrass.GetMoverCount(), 0u, "a cleared world holds no movers");
	ZENITH_ASSERT_EQ(xGrass.GetMoverOverflowCount(), 0u, "a cleared world has dropped no movers");
}

ZENITH_TEST(FluxGrassImpl, ResetIsIdempotentAndPreservesTypeTable)
{
	Flux_GrassImpl& xGrass = g_xEngine.Grass();
	const float fDensityScaleBefore = xGrass.GetDensityScale();
	// Snapshotted, not assumed to be the defaults: LoadAuthoredTypeTable may have
	// replaced them at Initialise with a table the game ships, and this test must
	// hand back exactly what it found.
	const Flux_GrassTypeTable xTableBefore = xGrass.GetTypeTable();
	xGrass.ClearSceneData();

	// The type table is AUTHORED content, not scene state: it is seeded once at
	// Initialise and has to outlive every scene load.
	Flux_GrassTypeTable xAuthored = Flux_GrassTypeTable::Defaults();
	xAuthored.SetCount(7u);
	xAuthored.Get(0u).m_fHeightMin = 0.11f;
	xAuthored.Get(1u).m_fClumpScale = 9.25f;
	xAuthored.SetName(0u, "UnitTestMeadow");
	xGrass.SetTypeTable(xAuthored);

	const Flux_GrassImpl::BuildParams xParams;
	const GrassTestMaps xSource;
	ZENITH_ASSERT_TRUE(xGrass.BuildFromMaps(xSource.Get(true), xParams), "the reference map set must build");
	Flux_GrassImpl::Mover xMover;
	xMover.m_xPos = Zenith_Maths::Vector3(1.0f, 0.0f, 2.0f);
	xMover.m_fRadius = 1.5f;
	xMover.m_fStrength = 1.0f;
	xGrass.SubmitMover(xMover);
	ZENITH_ASSERT_EQ(xGrass.GetMoverCount(), 1u, "the setup must leave scene state worth clearing");

	// Reset DOUBLE-FIRES at boot, so idempotency is a contract and not a nicety.
	xGrass.Reset();
	xGrass.Reset();

	ZENITH_ASSERT_FALSE(xGrass.IsBuilt(), "Reset must drop the built world");
	ZENITH_ASSERT_FALSE(xGrass.HasCoverageMap(), "Reset must drop the coverage map");
	ZENITH_ASSERT_EQ(xGrass.GetCoverageMapSize(), 0u, "Reset must drop the coverage map's extent");
	ZENITH_ASSERT_EQ(xGrass.GetScheduledInstanceCount(), 0u, "Reset must zero the scheduled cell count");
	ZENITH_ASSERT_EQ(xGrass.GetVisibleTileCount(), 0u, "Reset must zero the dispatched tile count");
	ZENITH_ASSERT_EQ(xGrass.GetTileCount(), 0u, "Reset must zero the culled tile count");
	ZENITH_ASSERT_EQ(xGrass.GetSubmittedDrawCount(), 0u, "Reset must zero the submitted draw count");
	ZENITH_ASSERT_EQ(xGrass.GetMoverCount(), 0u, "Reset must drop every mover");
	ZENITH_ASSERT_EQ(xGrass.GetMoverOverflowCount(), 0u, "Reset must zero the mover overflow counter");

	ZENITH_ASSERT_EQ(xGrass.GetTypeTable().GetCount(), 7u, "Reset must NOT touch the authored type table");
	ZENITH_ASSERT_EQ_FLOAT(xGrass.GetTypeTable().Get(0u).m_fHeightMin, 0.11f, 0.0f,
		"authored type parameters must survive a scene load");
	ZENITH_ASSERT_EQ_FLOAT(xGrass.GetTypeTable().Get(1u).m_fClumpScale, 9.25f, 0.0f,
		"authored type parameters must survive a scene load");
	ZENITH_ASSERT_STREQ(xGrass.GetTypeTable().GetName(0u).c_str(), "UnitTestMeadow",
		"authored type names must survive a scene load");

	// Every GPU allocation survives Reset too, so the next scene builds straight
	// back on top of them without re-creating a single resource.
	ZENITH_ASSERT_TRUE(xGrass.BuildFromMaps(xSource.Get(true), xParams), "a world must rebuild after a Reset");
	ZENITH_ASSERT_TRUE(xGrass.IsBuilt(), "the rebuilt world must be built");
	ZENITH_ASSERT_EQ(xGrass.GetCoverageMapSize(), uGrassTestCoverageSize, "the rebuilt world must reinstall its coverage map");

	// Hand the singleton back the table it had, not this test's.
	xGrass.SetTypeTable(xTableBefore);
	xGrass.SetDensityScale(fDensityScaleBefore);
	xGrass.ClearSceneData();
}

ZENITH_TEST(FluxGrassImpl, MoverCapAndClear)
{
	Flux_GrassImpl& xGrass = g_xEngine.Grass();
	xGrass.ClearSceneData();
	ZENITH_ASSERT_EQ(xGrass.GetMoverCount(), 0u, "the baseline for this test is an empty mover list");

	// One submission past the cap. Over-cap movers are DROPPED and counted, never
	// grown into: the list is consumed by every frame's gather, so an unbounded one
	// would let a runaway submitter grow without limit inside a single frame.
	for (u_int u = 0; u < uFLUX_GRASS_MAX_MOVERS + 1u; u++)
	{
		Flux_GrassImpl::Mover xMover;
		xMover.m_xPos = Zenith_Maths::Vector3(static_cast<float>(u), 0.0f, 0.0f);
		xMover.m_fRadius = 1.0f;
		xMover.m_fStrength = 1.0f;
		xGrass.SubmitMover(xMover);
	}

	ZENITH_ASSERT_EQ(xGrass.GetMoverCount(), uFLUX_GRASS_MAX_MOVERS, "the mover list must stop at its cap");
	ZENITH_ASSERT_EQ(xGrass.GetMoverOverflowCount(), 1u,
		"the one dropped submission must be counted — a silent drop is indistinguishable from a mover that did nothing");

	xGrass.ClearSceneData();
	ZENITH_ASSERT_EQ(xGrass.GetMoverCount(), 0u, "ClearSceneData must drop every mover");
	ZENITH_ASSERT_EQ(xGrass.GetMoverOverflowCount(), 0u, "ClearSceneData must zero the overflow counter too");

	// The debug orbiter is the ONE mover the engine submits for itself, and it goes in
	// through the same public SubmitMover a game body uses — so it answers to the cap
	// above rather than getting a private slot. The setter is the ImGui-free route a
	// capture sweep drives it by, and it has to survive ClearSceneData: it is a render
	// debug state, not scene content.
	const bool bOrbiterBefore = xGrass.IsDebugOrbitDisplacerEnabled();
	xGrass.SetDebugOrbitDisplacer(true);
	ZENITH_ASSERT_TRUE(xGrass.IsDebugOrbitDisplacerEnabled(), "the orbiter override must read back what it was set to");
	xGrass.ClearSceneData();
	ZENITH_ASSERT_TRUE(xGrass.IsDebugOrbitDisplacerEnabled(), "a scene clear must not silently cancel a debug override");
	xGrass.SetDebugOrbitDisplacer(bOrbiterBefore);
	ZENITH_ASSERT_EQ(xGrass.IsDebugOrbitDisplacerEnabled(), bOrbiterBefore, "the test must hand back the value it found");
}

ZENITH_TEST(FluxGrassImpl, ReadbackZeroHeadless)
{
	Flux_GrassImpl& xGrass = g_xEngine.Grass();
	const float fDensityScaleBefore = xGrass.GetDensityScale();
	xGrass.ClearSceneData();
	const GrassTestMaps xSource;
	ZENITH_ASSERT_TRUE(xGrass.BuildFromMaps(xSource.Get(true), Flux_GrassImpl::BuildParams()),
		"the reference map set must build before a readback means anything");

	// The surviving blade count is a GPU quantity. DownloadBufferData zero-fills
	// without an allocator, so under the null renderer ZERO is the CONTRACT rather
	// than an observation — which is exactly why no headless suite may assert a
	// non-zero blade count. On a windowed build the same call returns real,
	// frame-dependent truth and is not assertable from a unit test at all, so the
	// windowed leg deliberately checks nothing.
	if (Zenith_IsNullRenderer())
	{
		ZENITH_ASSERT_EQ(xGrass.ReadbackVisibleBladeCount(), 0u,
			"a headless readback must be exactly zero — a non-zero one would mean the download invented data");
	}

	xGrass.SetDensityScale(fDensityScaleBefore);
	xGrass.ClearSceneData();
}

ZENITH_TEST(FluxGrassImpl, ShadowCastingAnswersToAllThreeInputs)
{
	Flux_GrassImpl& xGrass = g_xEngine.Grass();
	Zenith_GraphicsOptions& xOpts = Zenith_GraphicsOptions::Get();

	// Process-wide state: every leg below restores exactly what it found, because
	// m_bGrassShadowsEnabled is set once at boot from Project_SetGraphicsOptions and
	// a later test (or the next frame) is entitled to that value.
	const bool bShadowsBefore      = xOpts.m_bShadowsEnabled;
	const bool bGrassShadowsBefore = xOpts.m_bGrassShadowsEnabled;
	const bool bDisableBefore      = xGrass.IsShadowCastingDisabled();

	// The three inputs are ANDed, so each one ALONE must be able to switch casting
	// off. Grass casters are the one caster class cheap enough to keep and expensive
	// enough to want dropped on their own, which is why they answer to a per-feature
	// option and a debug escape hatch on top of the engine-wide shadow switch.
	xOpts.m_bShadowsEnabled = true;
	xOpts.m_bGrassShadowsEnabled = true;
	xGrass.SetDisableShadowCasting(false);
	ZENITH_ASSERT_TRUE(xGrass.IsShadowCastingEnabled(), "all three inputs permitting must enable grass shadow casting");

	xOpts.m_bShadowsEnabled = false;
	ZENITH_ASSERT_FALSE(xGrass.IsShadowCastingEnabled(), "the engine-wide shadow switch alone must disable grass casting");
	xOpts.m_bShadowsEnabled = true;

	xOpts.m_bGrassShadowsEnabled = false;
	ZENITH_ASSERT_FALSE(xGrass.IsShadowCastingEnabled(), "the per-feature grass-shadow option alone must disable grass casting");
	xOpts.m_bGrassShadowsEnabled = true;

	xGrass.SetDisableShadowCasting(true);
	ZENITH_ASSERT_TRUE(xGrass.IsShadowCastingDisabled(), "the debug override must read back what was written");
	ZENITH_ASSERT_FALSE(xGrass.IsShadowCastingEnabled(), "the DisableShadowCasting override alone must disable grass casting");

	// Clearing it must RESTORE casting, not merely stop forcing it off — the override
	// is an A/B switch, so both directions are the contract.
	xGrass.SetDisableShadowCasting(false);
	ZENITH_ASSERT_TRUE(xGrass.IsShadowCastingEnabled(), "clearing the debug override must restore grass casting");

	xOpts.m_bShadowsEnabled = bShadowsBefore;
	xOpts.m_bGrassShadowsEnabled = bGrassShadowsBefore;
	xGrass.SetDisableShadowCasting(bDisableBefore);
}

ZENITH_TEST(FluxGrassImpl, ActiveSlotMaskExcludesCascadesWithoutShadowCasting)
{
	Flux_GrassImpl& xGrass = g_xEngine.Grass();
	Zenith_GraphicsOptions& xOpts = Zenith_GraphicsOptions::Get();
	const bool bShadowsBefore      = xOpts.m_bShadowsEnabled;
	const bool bGrassShadowsBefore = xOpts.m_bGrassShadowsEnabled;
	const bool bDisableBefore      = xGrass.IsShadowCastingDisabled();

	const u_int uCameraSlots  = (1u << uFLUX_GRASS_SLOT_CAMERA_HI) | (1u << uFLUX_GRASS_SLOT_CAMERA_LO);
	const u_int uCascadeSlots = (1u << uFLUX_GRASS_SLOT_CASCADE_0) | (1u << uFLUX_GRASS_SLOT_CASCADE_1);

	// The mask is what the placement CS reads, so casting off must stop GENERATION
	// into the cascade partitions and not merely the two draws — a partition still
	// being filled would cost every shadow blade's append for a cascade nothing reads.
	xGrass.SetDisableShadowCasting(true);
	ZENITH_ASSERT_EQ(xGrass.GetActiveSlotMask() & uCascadeSlots, 0u,
		"the debug override must drop BOTH cascade partitions out of the active-slot mask");
	ZENITH_ASSERT_EQ(xGrass.GetActiveSlotMask() & uCameraSlots, uCameraSlots,
		"the camera partitions are unconditional — casting off must not touch them");

	xOpts.m_bShadowsEnabled = false;
	xOpts.m_bGrassShadowsEnabled = false;
	xGrass.SetDisableShadowCasting(false);
	ZENITH_ASSERT_EQ(xGrass.GetActiveSlotMask() & uCascadeSlots, 0u,
		"the graphics options must gate the cascade partitions exactly as the debug override does");
	ZENITH_ASSERT_EQ(xGrass.GetActiveSlotMask() & uCameraSlots, uCameraSlots,
		"the camera partitions are unconditional under the options too");

	// Permitting casting is NOT enough on its own: a slot also needs a REAL cascade
	// frustum staged by a gather, because culling a partition against a duplicated
	// CAMERA frustum would fill it with the wrong blades. No frame is staged here, so
	// the enabled case is bounded rather than asserted set.
	xOpts.m_bShadowsEnabled = true;
	xOpts.m_bGrassShadowsEnabled = true;
	const u_int uMask = xGrass.GetActiveSlotMask();
	ZENITH_ASSERT_EQ(uMask & uCameraSlots, uCameraSlots, "the camera partitions must be live in every configuration");
	ZENITH_ASSERT_EQ(uMask & ~(uCameraSlots | uCascadeSlots), 0u,
		"only the four LIVE partitions may ever appear — the other 12 indirect slots are reserved and must stay zero");

	xOpts.m_bShadowsEnabled = bShadowsBefore;
	xOpts.m_bGrassShadowsEnabled = bGrassShadowsBefore;
	xGrass.SetDisableShadowCasting(bDisableBefore);
}

ZENITH_TEST(FluxGrassImpl, GrassIsDeclaredBeforeShadows)
{
	// The live feature table this build ships. The setup walk IS the render-graph
	// declaration order, so comparing the two indices asks exactly the question the
	// cascades' grass reads depend on.
	const Flux_FeatureRegistry& xReg = Flux_FeatureRegistry::Get();
	const u_int uGrass   = xReg.FindSetupStepIndex("Grass");
	const u_int uShadows = xReg.FindSetupStepIndex("Shadows");

	// Guarded against vacuity: a rename would otherwise make the comparison pass on
	// two UINT32_MAXes.
	ZENITH_ASSERT_TRUE(uGrass != UINT32_MAX, "setup step 'Grass' must exist, or this ordering assertion is vacuous");
	ZENITH_ASSERT_TRUE(uShadows != UINT32_MAX, "setup step 'Shadows' must exist, or this ordering assertion is vacuous");

	ZENITH_ASSERT_TRUE(uGrass < uShadows,
		"'Grass' must be declared BEFORE 'Shadows': each of cascades 0-1 READS the grass blade pool / visible-index / "
		"indirect-args buffers, and a reader only links to an EARLIER-declared writer. The other way round no edge forms "
		"at all and the cascades are free to draw from indirect args the reset has not filled yet (grass %u, shadows %u)",
		uGrass, uShadows);
}

// ============================================================================
// Flux_GrassTypeTable — the AUTHORING side of the per-type parameters.
//
// Flux_GrassTypeParams is 43 packed 4-byte fields with no padding, which is both
// what lets the serializer budget its payload by a FIELD COUNT and what makes a
// memcmp a legitimate field-by-field compare in this file. DefaultsValidateClean
// asserts that size first, so a struct that grew padding fails there rather than
// silently comparing it.
// ============================================================================

namespace
{
	constexpr size_t uGrassTypeParamsBytes = 43u * 4u;

	bool GrassTableTest_ParamsIdentical(const Flux_GrassTypeParams& xA, const Flux_GrassTypeParams& xB)
	{
		return memcmp(&xA, &xB, sizeof(Flux_GrassTypeParams)) == 0;
	}

	// Only the LIVE entries: ReadFromDataStream resets every slot and fills exactly
	// GetCount() of them, so the tail is defined but not authored.
	bool GrassTableTest_LiveEntriesIdentical(const Flux_GrassTypeTable& xA, const Flux_GrassTypeTable& xB)
	{
		if (xA.GetCount() != xB.GetCount())
		{
			return false;
		}
		for (u_int u = 0; u < xA.GetCount(); u++)
		{
			if (!GrassTableTest_ParamsIdentical(xA.Get(u), xB.Get(u)) || xA.GetName(u) != xB.GetName(u))
			{
				return false;
			}
		}
		return true;
	}

	// A table that is plainly NOT the defaults, so "untouched" cannot pass by
	// accident against a freshly defaulted one.
	Flux_GrassTypeTable GrassTableTest_MakeAuthored()
	{
		Flux_GrassTypeTable xTable = Flux_GrassTypeTable::Defaults();
		xTable.SetCount(3u);
		xTable.SetName(0u, "AuthoredZero");
		xTable.SetName(2u, "AuthoredTwo");
		xTable.Get(0u).m_fHeightMax = 1.23f;
		xTable.Get(1u).m_fSlopeMax = 0.42f;
		xTable.Get(2u).m_xTipColour = Zenith_Maths::Vector3(0.71f, 0.19f, 0.33f);
		xTable.Get(2u).m_uVeinTextureIndex = 12u;
		xTable.Validate();
		return xTable;
	}
}

ZENITH_TEST(FluxGrassTypeTable, DefaultsValidateClean)
{
	ZENITH_ASSERT_EQ(sizeof(Flux_GrassTypeParams), uGrassTypeParamsBytes,
		"the authored record must stay 43 packed 4-byte fields — the serializer budgets its payload from that count, and this file compares entries with memcmp");

	const Flux_GrassTypeTable xTable = Flux_GrassTypeTable::Defaults();
	ZENITH_ASSERT_EQ(xTable.GetCount(), 4u, "the shipped set is Meadow / Tall / Dry / Flowers");
	ZENITH_ASSERT_STREQ(xTable.GetName(0u).c_str(), "Meadow",
		"entry 0 is what an unpainted (all-zero) type map selects, so it must be the ordinary lawn");

	// A default-constructed table IS the authored default set, so the named factory
	// and the constructor must not have drifted into two different sets.
	const Flux_GrassTypeTable xConstructed;
	ZENITH_ASSERT_TRUE(GrassTableTest_LiveEntriesIdentical(xTable, xConstructed),
		"Defaults() must be exactly a default construction, not a second copy of the set");

	// The shipped defaults must already sit inside every range Validate enforces —
	// otherwise the engine ships grass it immediately clamps to something else.
	Flux_GrassTypeTable xValidated = Flux_GrassTypeTable::Defaults();
	xValidated.Validate();
	ZENITH_ASSERT_TRUE(GrassTableTest_LiveEntriesIdentical(xTable, xValidated),
		"validating the defaults must change nothing");

	// ... and Validate is idempotent, so a repeated editor edit cannot walk a value.
	xValidated.Validate();
	xValidated.Validate();
	ZENITH_ASSERT_TRUE(GrassTableTest_LiveEntriesIdentical(xTable, xValidated), "Validate must be idempotent");
}

ZENITH_TEST(FluxGrassTypeTable, ValidateClampsOutOfRange)
{
	// The poison values are built and CLASSIFIED by bit pattern throughout. Nothing
	// here may ask a float whether it is a NaN: this build's float optimizations let
	// the compiler assume NaN and infinity never occur, so `f != f` evaluates EQUAL
	// for a genuine NaN and std::isnan folds to false. That is not a test-harness
	// quirk — it is the exact reason Validate() had to stop using a float test, and
	// a float test here would have silently agreed with the broken Validate.
	const float fNaN = std::bit_cast<float>(0x7FC00000u);
	const float fPosInf = std::bit_cast<float>(0x7F800000u);
	const float fNegInf = std::bit_cast<float>(0xFF800000u);

	// IEEE-754 binary32: exponent all-ones + non-zero mantissa is a NaN; exponent
	// all-ones + zero mantissa is an infinity. Assert the patterns SURVIVED into the
	// float variables, so a compiler that canonicalized them says so here rather
	// than by quietly making the rest of this test vacuous.
	ZENITH_ASSERT_EQ(std::bit_cast<u_int>(fNaN) & 0x7F800000u, 0x7F800000u,
		"the injected NaN must have an all-ones exponent");
	ZENITH_ASSERT_NE(std::bit_cast<u_int>(fNaN) & 0x007FFFFFu, 0u,
		"... and a non-zero mantissa — that is what makes it a NaN and not an infinity");
	ZENITH_ASSERT_EQ(std::bit_cast<u_int>(fPosInf) & 0x7F800000u, 0x7F800000u, "+inf has an all-ones exponent");
	ZENITH_ASSERT_EQ(std::bit_cast<u_int>(fPosInf) & 0x807FFFFFu, 0u, "... a zero mantissa and a clear sign bit");
	ZENITH_ASSERT_EQ(std::bit_cast<u_int>(fNegInf) & 0x807FFFFFu, 0x80000000u, "-inf is +inf with the sign bit set");

	// The shared classifier Validate() now depends on. It must reject all three and
	// accept ordinary values, including the extremes of the normal range.
	ZENITH_ASSERT_FALSE(Flux_GrassIsFiniteFloat(fNaN), "a NaN is not finite");
	ZENITH_ASSERT_FALSE(Flux_GrassIsFiniteFloat(fPosInf), "+inf is not finite");
	ZENITH_ASSERT_FALSE(Flux_GrassIsFiniteFloat(fNegInf), "-inf is not finite — the sign bit must not reach the test");
	ZENITH_ASSERT_TRUE(Flux_GrassIsFiniteFloat(0.0f), "zero is finite");
	ZENITH_ASSERT_TRUE(Flux_GrassIsFiniteFloat(-0.0f), "negative zero is finite");
	ZENITH_ASSERT_TRUE(Flux_GrassIsFiniteFloat(0.55f), "an ordinary authored value is finite");
	ZENITH_ASSERT_TRUE(Flux_GrassIsFiniteFloat(-1.0e9f), "a large negative value is finite, merely out of range");
	ZENITH_ASSERT_TRUE(Flux_GrassIsFiniteFloat(std::bit_cast<float>(0x7F7FFFFFu)), "FLT_MAX is finite");
	ZENITH_ASSERT_TRUE(Flux_GrassIsFiniteFloat(std::bit_cast<float>(0x00000001u)), "the smallest denormal is finite");

	Flux_GrassTypeTable xTable = Flux_GrassTypeTable::Defaults();
	Flux_GrassTypeParams& xType = xTable.Get(0u);
	xType.m_fHeightMin = -5.0f;
	xType.m_fHeightMax = 1.0e9f;
	xType.m_fWidthMin = -1.0f;
	xType.m_fTiltMaxRad = 99.0f;
	xType.m_fVertexDistributionPow = 0.0f;
	xType.m_fClumpScale = 0.0f;
	xType.m_fDensity = 7.0f;
	xType.m_fAOTipRelease = 0.0f;
	xType.m_fMaxDrawDistance = 1.0e9f;
	xType.m_xBaseColour = Zenith_Maths::Vector3(5.0f, -1.0f, 0.5f);
	// The three non-finite injections, one per kind, in three different fields —
	// the sanitize step is shared, so this pins that it is applied UNIFORMLY and not
	// just to whichever field someone once noticed.
	xType.m_fStiffness = fNaN;
	xType.m_fRoughnessBase = fPosInf;
	xType.m_fSpecular = fNegInf;

	xTable.Validate();

	// These ranges are load-bearing, not cosmetic: a zero clump scale divides by
	// zero in the Voronoi search, a zero distribution exponent divides by zero in
	// the height remap, and a negative height inverts the blade.
	ZENITH_ASSERT_GE(xType.m_fHeightMin, 0.01f, "a negative height must clamp up, not invert the blade");
	ZENITH_ASSERT_LE(xType.m_fHeightMax, 8.0f, "an absurd height must clamp down");
	ZENITH_ASSERT_LE(xType.m_fHeightMin, xType.m_fHeightMax, "the height pair must come out ordered");
	ZENITH_ASSERT_GE(xType.m_fWidthMin, 0.001f, "a negative width must clamp up");
	ZENITH_ASSERT_LE(xType.m_fWidthMin, xType.m_fWidthMax, "the width pair must come out ordered");
	ZENITH_ASSERT_LE(xType.m_fTiltMaxRad, 1.5f, "tilt is bounded short of a right angle");
	ZENITH_ASSERT_GE(xType.m_fVertexDistributionPow, 0.25f, "the distribution exponent divides — it can never reach zero");
	ZENITH_ASSERT_GE(xType.m_fClumpScale, 0.05f, "the clump cell size divides — it can never reach zero");
	ZENITH_ASSERT_LE(xType.m_fDensity, 1.0f, "density is an acceptance PROBABILITY, so it caps at one");
	ZENITH_ASSERT_GE(xType.m_fAOTipRelease, 0.01f, "the AO release height divides — it can never reach zero");
	ZENITH_ASSERT_LE(xType.m_fMaxDrawDistance, Flux_GrassConfig::fMAX_MAX_DISTANCE,
		"a per-type draw distance must land inside the global band");
	ZENITH_ASSERT_GE(xType.m_fMaxDrawDistance, Flux_GrassConfig::fMIN_MAX_DISTANCE,
		"a per-type draw distance must land inside the global band");
	ZENITH_ASSERT_EQ_FLOAT(xType.m_xBaseColour.x, 1.0f, 0.0f, "an over-bright colour channel must clamp to one");
	ZENITH_ASSERT_EQ_FLOAT(xType.m_xBaseColour.y, 0.0f, 0.0f, "a negative colour channel must clamp to zero");
	ZENITH_ASSERT_EQ_FLOAT(xType.m_xBaseColour.z, 0.5f, 0.0f, "an in-range channel must be left exactly alone");

	// The three non-finite fields must be REPLACED with their authored defaults, not
	// clamped. Clamping is what a plain range test does to them and it is wrong in
	// both directions: a NaN passes straight through (`x < lo` and `x > hi` are both
	// false for it) and reaches the GPU as a blade with no size, while an infinity
	// lands on a range END that the author never asked for. Each expected value is
	// the field's default, and each differs from what a clamp would have produced —
	// roughness would be 1.0 (its ceiling) and specular 0.0 (its floor).
	ZENITH_ASSERT_TRUE(Flux_GrassIsFiniteFloat(xType.m_fStiffness), "no non-finite value may survive Validate");
	ZENITH_ASSERT_TRUE(Flux_GrassIsFiniteFloat(xType.m_fRoughnessBase), "no non-finite value may survive Validate");
	ZENITH_ASSERT_TRUE(Flux_GrassIsFiniteFloat(xType.m_fSpecular), "no non-finite value may survive Validate");
	ZENITH_ASSERT_EQ_FLOAT(xType.m_fStiffness, 0.50f, 0.0f, "a NaN must be replaced with the authored default");
	ZENITH_ASSERT_EQ_FLOAT(xType.m_fRoughnessBase, 0.55f, 0.0f,
		"+inf must be replaced with the authored default, not clamped to the range ceiling");
	ZENITH_ASSERT_EQ_FLOAT(xType.m_fSpecular, 0.35f, 0.0f,
		"-inf must be replaced with the authored default, not clamped to the range floor");

	// A zero live count would make the placement CS clamp the map's type index
	// to -1; an overflowing one would index past the table.
	xTable.SetCount(0u);
	xTable.Validate();
	ZENITH_ASSERT_GE(xTable.GetCount(), 1u, "the live type count must never reach zero");
	xTable.SetCount(uFLUX_GRASS_MAX_TYPES + 5u);
	xTable.Validate();
	ZENITH_ASSERT_LE(xTable.GetCount(), uFLUX_GRASS_MAX_TYPES, "the live type count must never exceed the table");
}

ZENITH_TEST(FluxGrassTypeTable, SerializeRoundTripsExactly)
{
	const Flux_GrassTypeTable xSource = GrassTableTest_MakeAuthored();

	Zenith_DataStream xStream;
	xSource.WriteToDataStream(xStream);
	xStream.SetCursor(0);

	Flux_GrassTypeTable xLoaded = Flux_GrassTypeTable::Defaults();
	ZENITH_ASSERT_TRUE(xLoaded.ReadFromDataStream(xStream), "a table this engine wrote must read back");

	ZENITH_ASSERT_EQ(xLoaded.GetCount(), xSource.GetCount(), "the live type count must round-trip");
	ZENITH_ASSERT_TRUE(GrassTableTest_LiveEntriesIdentical(xSource, xLoaded),
		"every authored field must round-trip byte for byte — the packed GPU form is a one-way projection, so the file has to carry the unpacked truth");
	ZENITH_ASSERT_STREQ(xLoaded.GetName(0u).c_str(), "AuthoredZero", "type names must round-trip");
	ZENITH_ASSERT_STREQ(xLoaded.GetName(1u).c_str(), "Tall", "an unedited name must round-trip too");
	ZENITH_ASSERT_STREQ(xLoaded.GetName(2u).c_str(), "AuthoredTwo", "type names must round-trip");
	ZENITH_ASSERT_EQ(xLoaded.Get(2u).m_uVeinTextureIndex, 12u,
		"a bindless slot is an integer and must not come back float-quantized");

	// The slots past the count are RESET, not left showing the previous (longer)
	// table's tail: a shorter file must not leave stale types visible behind it.
	Flux_GrassTypeParams xDefault;
	xDefault.Validate();
	for (u_int u = xLoaded.GetCount(); u < uFLUX_GRASS_MAX_TYPES; u++)
	{
		ZENITH_ASSERT_TRUE(GrassTableTest_ParamsIdentical(xLoaded.Get(u), xDefault),
			"slot %u past the live count must be reset, not inherited", u);
		ZENITH_ASSERT_TRUE(xLoaded.GetName(u).empty(), "slot %u past the live count must carry no name", u);
	}
}

ZENITH_TEST(FluxGrassTypeTable, ReadRejectsGarbageAndLeavesTableUntouched)
{
	const Flux_GrassTypeTable xReference = GrassTableTest_MakeAuthored();
	Flux_GrassTypeTable xTable = GrassTableTest_MakeAuthored();

	// A good stream first, so the version word comes from the WRITER rather than a
	// constant this file would have to keep in step with it by hand.
	Zenith_DataStream xGood;
	xTable.WriteToDataStream(xGood);
	const uint64_t ulWritten = xGood.GetCursor();
	xGood.SetCursor(0);
	u_int uVersion = 0u;
	xGood >> uVersion;
	ZENITH_ASSERT_GT(ulWritten, static_cast<uint64_t>(16), "the reference payload must be long enough to truncate meaningfully");

	// TRUNCATED. The reader measures the whole payload before it clears a single
	// slot, because the stream's own bounds checks merely log and return: a
	// field-by-field read of a short file would leave a table that Validate() then
	// makes look plausible instead of one that plainly reverted.
	Zenith_DataStream xTruncated(xGood.GetData(), ulWritten - 8ull);
	ZENITH_ASSERT_FALSE(xTable.ReadFromDataStream(xTruncated), "a stream that ends part-way through an entry must be rejected");
	ZENITH_ASSERT_TRUE(GrassTableTest_LiveEntriesIdentical(xTable, xReference),
		"a rejected read must leave the table untouched, never half-written");

	// ZERO COUNT. SetCount clamps to [1, MAX] on the authoring side, so neither end
	// can come from a file this engine wrote.
	Zenith_DataStream xZeroCount;
	xZeroCount << uVersion;
	xZeroCount << 0u;
	xZeroCount.SetCursor(0);
	ZENITH_ASSERT_FALSE(xTable.ReadFromDataStream(xZeroCount), "a zero type count must be rejected");
	ZENITH_ASSERT_TRUE(GrassTableTest_LiveEntriesIdentical(xTable, xReference), "a rejected read must leave the table untouched");

	Zenith_DataStream xHugeCount;
	xHugeCount << uVersion;
	xHugeCount << (uFLUX_GRASS_MAX_TYPES + 1u);
	xHugeCount.SetCursor(0);
	ZENITH_ASSERT_FALSE(xTable.ReadFromDataStream(xHugeCount), "a type count past the table capacity must be rejected");
	ZENITH_ASSERT_TRUE(GrassTableTest_LiveEntriesIdentical(xTable, xReference), "a rejected read must leave the table untouched");

	// WRONG VERSION. There is no migration path by design: a partially-read table
	// produces plausible grass, which is far harder to spot than one that reverted.
	Zenith_DataStream xWrongVersion;
	xWrongVersion << (uVersion + 1u);
	xWrongVersion << 4u;
	xWrongVersion.SetCursor(0);
	ZENITH_ASSERT_FALSE(xTable.ReadFromDataStream(xWrongVersion), "a table written by another version must be rejected");
	ZENITH_ASSERT_TRUE(GrassTableTest_LiveEntriesIdentical(xTable, xReference), "a rejected read must leave the table untouched");

	// TOO SHORT FOR EVEN THE HEADER.
	u_int auStub[1] = { 0u };
	Zenith_DataStream xStub(auStub, sizeof(auStub));
	ZENITH_ASSERT_FALSE(xTable.ReadFromDataStream(xStub), "a stream with no room for a header must be rejected");
	ZENITH_ASSERT_TRUE(GrassTableTest_LiveEntriesIdentical(xTable, xReference), "a rejected read must leave the table untouched");

	// ... and the reader still accepts its OWN stream, so everything above rejected
	// the garbage rather than the reader having simply stopped working.
	xGood.SetCursor(0);
	ZENITH_ASSERT_TRUE(xTable.ReadFromDataStream(xGood), "the reader must still accept a well-formed stream");
	ZENITH_ASSERT_TRUE(GrassTableTest_LiveEntriesIdentical(xTable, xReference), "a good read must restore the same table");
}

ZENITH_TEST(FluxGrassTypeTable, NameAddressedParamsHitTheRightFields)
{
	// EXHAUSTIVE, not a sample: every float name is written a value derived from
	// its index and then read back, so a table row pointing at the WRONG member
	// (the copy-paste failure this mapping invites) collides with another row's
	// value and fails here rather than at authoring time.
	const u_int uFloatCount = Flux_GrassTypeParams::GetFloatParamCount();
	ZENITH_ASSERT_GT(uFloatCount, 0u, "the float mapping must not be empty");

	Flux_GrassTypeParams xParams;
	for (u_int u = 0; u < uFloatCount; u++)
	{
		const char* szName = Flux_GrassTypeParams::GetFloatParamName(u);
		ZENITH_ASSERT_TRUE(szName != nullptr, "every in-range float index must name a param");
		// Distinct per index and outside no field's range, so the write is
		// unambiguous. Validate() is deliberately NOT run here — the setter must
		// store verbatim and leave clamping to the caller's explicit step.
		ZENITH_ASSERT_TRUE(xParams.SetFloatParamByName(szName, 1000.0f + static_cast<float>(u)),
			"a name from the enumeration must be settable");
	}
	for (u_int u = 0; u < uFloatCount; u++)
	{
		float fRead = 0.0f;
		ZENITH_ASSERT_TRUE(xParams.GetFloatParamByName(Flux_GrassTypeParams::GetFloatParamName(u), fRead),
			"a name from the enumeration must be readable");
		ZENITH_ASSERT_EQ_FLOAT(fRead, 1000.0f + static_cast<float>(u), 0.0001f,
			"each name must address its OWN field — a duplicate row would read back another name's value");
	}

	// Spot-check that the names mean what they say, so an exhaustive-but-shuffled
	// table (every row distinct, all pointing one field off) still fails.
	Flux_GrassTypeParams xNamed;
	ZENITH_ASSERT_TRUE(xNamed.SetFloatParamByName("HeightMax", 2.5f), "HeightMax must be addressable");
	ZENITH_ASSERT_EQ_FLOAT(xNamed.m_fHeightMax, 2.5f, 0.0001f, "\"HeightMax\" must write m_fHeightMax");
	ZENITH_ASSERT_TRUE(xNamed.SetFloatParamByName("Density", 0.25f), "Density must be addressable");
	ZENITH_ASSERT_EQ_FLOAT(xNamed.m_fDensity, 0.25f, 0.0001f, "\"Density\" must write m_fDensity");
	ZENITH_ASSERT_TRUE(xNamed.SetFloatParamByName("WindResponse", 3.5f), "WindResponse must be addressable");
	ZENITH_ASSERT_EQ_FLOAT(xNamed.m_fWindResponse, 3.5f, 0.0001f, "\"WindResponse\" must write m_fWindResponse");
	ZENITH_ASSERT_TRUE(xNamed.SetFloatParamByName("ClumpScale", 7.0f), "ClumpScale must be addressable");
	ZENITH_ASSERT_EQ_FLOAT(xNamed.m_fClumpScale, 7.0f, 0.0001f, "\"ClumpScale\" must write m_fClumpScale");

	// Colours are a separate table with a separate setter: a colour name must NOT
	// resolve through the float map, and vice versa, or an authoring script could
	// write a Vector3's first component and think it set the colour.
	ZENITH_ASSERT_EQ(Flux_GrassTypeParams::GetColourParamCount(), 2u, "base + tip are the two authored colours");
	Flux_GrassTypeParams xColoured;
	ZENITH_ASSERT_TRUE(xColoured.SetColourParamByName("BaseColour", Zenith_Maths::Vector3(0.1f, 0.2f, 0.3f)),
		"BaseColour must be addressable");
	ZENITH_ASSERT_TRUE(xColoured.SetColourParamByName("TipColour", Zenith_Maths::Vector3(0.4f, 0.5f, 0.6f)),
		"TipColour must be addressable");
	ZENITH_ASSERT_EQ_FLOAT(xColoured.m_xBaseColour.x, 0.1f, 0.0001f, "\"BaseColour\" must write m_xBaseColour");
	ZENITH_ASSERT_EQ_FLOAT(xColoured.m_xBaseColour.z, 0.3f, 0.0001f, "\"BaseColour\" must write all three components");
	ZENITH_ASSERT_EQ_FLOAT(xColoured.m_xTipColour.y, 0.5f, 0.0001f, "\"TipColour\" must write m_xTipColour");
	Zenith_Maths::Vector3 xReadBack(0.0f, 0.0f, 0.0f);
	ZENITH_ASSERT_TRUE(xColoured.GetColourParamByName("TipColour", xReadBack), "a colour name must be readable");
	ZENITH_ASSERT_EQ_FLOAT(xReadBack.z, 0.6f, 0.0001f, "the colour round trip must be exact");
}

ZENITH_TEST(FluxGrassTypeTable, UnknownParamNamesAreRejectedAndWriteNothing)
{
	// The whole point of returning false: the Checked wrapper in editor
	// automation asserts on it, so a typo surfaces at boot instead of shipping
	// grass that is merely slightly wrong.
	Flux_GrassTypeParams xParams;
	const Flux_GrassTypeParams xBefore = xParams;

	float fUnused = -1.0f;
	ZENITH_ASSERT_FALSE(xParams.SetFloatParamByName("NotAParam", 5.0f), "an unknown float name must be rejected");
	ZENITH_ASSERT_FALSE(xParams.GetFloatParamByName("NotAParam", fUnused), "an unknown float name must not read");
	ZENITH_ASSERT_EQ_FLOAT(fUnused, -1.0f, 0.0001f, "a rejected read must not write the out param");
	ZENITH_ASSERT_FALSE(xParams.SetFloatParamByName(nullptr, 5.0f), "a null name must be rejected, not dereferenced");
	ZENITH_ASSERT_FALSE(xParams.SetFloatParamByName("", 5.0f), "an empty name must be rejected");

	// Case matters and the C++ member spelling is NOT the authoring vocabulary —
	// both are the near-misses an author actually types.
	ZENITH_ASSERT_FALSE(xParams.SetFloatParamByName("heightmax", 5.0f), "the mapping is case-sensitive");
	ZENITH_ASSERT_FALSE(xParams.SetFloatParamByName("m_fHeightMax", 5.0f), "the C++ field name is not an authoring name");

	// Cross-table misses: a colour is not a float and a float is not a colour.
	ZENITH_ASSERT_FALSE(xParams.SetFloatParamByName("BaseColour", 5.0f), "a colour must not resolve through the float map");
	ZENITH_ASSERT_FALSE(xParams.SetColourParamByName("HeightMax", Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f)),
		"a float must not resolve through the colour map");
	Zenith_Maths::Vector3 xUnused(-1.0f, -1.0f, -1.0f);
	ZENITH_ASSERT_FALSE(xParams.GetColourParamByName("NotAColour", xUnused), "an unknown colour name must not read");
	ZENITH_ASSERT_EQ_FLOAT(xUnused.x, -1.0f, 0.0001f, "a rejected colour read must not write the out param");

	ZENITH_ASSERT_TRUE(GrassTableTest_ParamsIdentical(xParams, xBefore),
		"every rejected call must leave the record byte-for-byte untouched");

	// Out-of-range enumeration returns nullptr rather than walking off the table.
	ZENITH_ASSERT_TRUE(Flux_GrassTypeParams::GetFloatParamName(Flux_GrassTypeParams::GetFloatParamCount()) == nullptr,
		"an out-of-range float index must return nullptr");
	ZENITH_ASSERT_TRUE(Flux_GrassTypeParams::GetColourParamName(Flux_GrassTypeParams::GetColourParamCount()) == nullptr,
		"an out-of-range colour index must return nullptr");
}
