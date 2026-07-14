#include "Zenith.h"

// ============================================================================
// ZM_Tests_GenCommon -- S4 box ZM_GenCommon unit tests (suite ZM_Gen). Covers
// the deterministic generation RNG (byte-identical same-seed streams, the
// golden-PCG32 forward, the fixed integer->float helpers), seed derivation
// (golden fold recipe + per-domain stream disjointness + the FNV-1a identity
// against the terrain seed hash), integer-hash noise (determinism + bounds),
// the loft/mesh toolkit (byte-identical rebuild, outward winding, non-degenerate
// bounds, the <=2-bone ring-bind weight contract, in-range bone indices, the -Z
// seam duplicate, subdivision-preserves-authored-rings), the pure skeleton
// topology (bone caps, parent-before-child order, clip-channel binding), and the
// validator self-check on a hand-built bad mesh.
//
// PURE / HEADLESS: every case exercises only the ZM_GenCommon pure API -- no
// disk, no GPU, no ZENITH_TOOLS reach. These run at boot before the scene loads.
//
// DETERMINISM STRATEGY (mirrors ZM_Tests_BattleTower's local-oracle method):
// * The seed-derivation golden is checked against a LOCAL reconstruction
//   (OracleHashCombine / OracleDeriveSeed) that INDEPENDENTLY replays the frozen
//   fold recipe from the S4 impl notes (left-reduce combine over
//   (family, species, evo, domain), then a 32->64 splat via a golden-ratio
//   second combine). If the production fold ever drifts from that recipe the
//   golden fails -- which is exactly the version-bump signal the contract wants.
// * The FNV-1a identity is pinned two ways: ZM_GenHashName(s) == ZM_Fnv1a32(s)
//   (algorithm identity vs the shipped terrain-seed hash) AND a hand-computed
//   literal for the single byte "A" that locks the FNV constants 2166136261 /
//   16777619.
// * Same-seed byte-identity is a raw memcmp over the ZM_GenMesh SoA buffers on
//   two independently-built meshes (the computation is identical, so the bits
//   match exactly).
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Gen/ZM_GenCommon.h"
#include "Zenithmon/Source/Data/ZM_BattleRNG.h"        // RNG golden-forward comparison
#include "Zenithmon/Source/World/ZM_TerrainAuthoring.h" // ZM_Fnv1a32 (terrain seed anchor)
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"

#include <cstring>   // memcmp
#include <cmath>     // fabsf

namespace
{
	constexpr float fWEIGHT_TOL = 1.0e-4f;   // matches ZM_ValidateGenMesh default
	constexpr float fPOS_TOL    = 1.0e-3f;   // authored-ring reproduce tolerance
	constexpr float fWEIGHT_EPS = 1.0e-6f;   // "non-zero influence" threshold

	// ---- FNV-1a / seed-derivation oracle (frozen S4 recipe) ----------------
	// FNV-1a 32-bit over a NUL-terminated string -- byte-identical to the
	// production ZM_GenHashName / ZM_Fnv1a32 (constants 2166136261 / 16777619).
	u_int OracleFnv1a32(const char* szText)
	{
		u_int uHash = 2166136261u;
		if (szText != nullptr)
		{
			for (const char* p = szText; *p != '\0'; ++p)
			{
				uHash ^= (u_int)(u_int8)(*p);
				uHash *= 16777619u;
			}
		}
		return uHash;   // wraps mod 2^32
	}

	// Order-sensitive 32-bit mix (impl-note recipe): combine(a,b) != combine(b,a).
	u_int OracleHashCombine(u_int uSeedA, u_int uSeedB)
	{
		u_int uHash = uSeedA ^ 2166136261u;
		uHash = (uHash * 16777619u) ^ uSeedB;
		uHash *= 16777619u;
		return uHash;
	}

	// Left-reduce the 4-tuple (family, species, evo, domain) into a 32-bit
	// accumulator, then splat to 64 bits by combining a decorrelated second word.
	u_int64 OracleDeriveSeed(u_int uFamilySeed, u_int uSpeciesId, u_int uEvoStage,
		ZM_GEN_DOMAIN eDomain)
	{
		u_int uLo = OracleHashCombine(uFamilySeed, uSpeciesId);
		uLo = OracleHashCombine(uLo, uEvoStage);
		uLo = OracleHashCombine(uLo, (u_int)eDomain);
		const u_int uHi = OracleHashCombine(uLo, 0x9E3779B9u);
		return ((u_int64)uHi << 32) | (u_int64)uLo;
	}

	// ---- loft fixtures ------------------------------------------------------
	constexpr u_int uTEST_SEGS = 8u;

	// A fixed 4-ring elliptical part table skinned across three chained bones.
	// The bone indices reference bones 0/1/2 (added by AddThreeChainBones).
	void FillFixedRings(ZM_LoftRing (&axRings)[4])
	{
		axRings[0] = ZM_LoftRing{}; axRings[0].m_fY = 0.0f; axRings[0].m_fRx = 0.50f; axRings[0].m_fRz = 0.40f; axRings[0].m_uBoneA = 0u; axRings[0].m_uBoneB = 0u; axRings[0].m_fBlendB = 0.0f;
		axRings[1] = ZM_LoftRing{}; axRings[1].m_fY = 1.0f; axRings[1].m_fRx = 0.60f; axRings[1].m_fRz = 0.50f; axRings[1].m_uBoneA = 0u; axRings[1].m_uBoneB = 1u; axRings[1].m_fBlendB = 0.30f;
		axRings[2] = ZM_LoftRing{}; axRings[2].m_fY = 2.0f; axRings[2].m_fRx = 0.50f; axRings[2].m_fRz = 0.45f; axRings[2].m_uBoneA = 1u; axRings[2].m_uBoneB = 2u; axRings[2].m_fBlendB = 0.50f;
		axRings[3] = ZM_LoftRing{}; axRings[3].m_fY = 3.0f; axRings[3].m_fRx = 0.30f; axRings[3].m_fRz = 0.25f; axRings[3].m_uBoneA = 2u; axRings[3].m_uBoneB = 2u; axRings[3].m_fBlendB = 0.0f;
	}

	void AddThreeChainBones(ZM_GenMesh& xMesh)
	{
		const Zenith_Maths::Quat xIdent = glm::identity<Zenith_Maths::Quat>();
		const Zenith_Maths::Vector3 xOne(1.0f);
		ZM_GenAddBone(xMesh, "Root",  -1, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), xIdent, xOne);
		ZM_GenAddBone(xMesh, "Mid",    0, Zenith_Maths::Vector3(0.0f, 1.5f, 0.0f), xIdent, xOne);
		ZM_GenAddBone(xMesh, "Tip",    1, Zenith_Maths::Vector3(0.0f, 1.5f, 0.0f), xIdent, xOne);
	}

	// Deterministic swept part (no RNG): three bones + the fixed ring table, both
	// ends capped, default Catmull-Rom subdivision.
	void BuildFixedMesh(ZM_GenMesh& xMesh)
	{
		xMesh.Reset();
		AddThreeChainBones(xMesh);
		ZM_LoftRing axRings[4];
		FillFixedRings(axRings);

		ZM_MeshLoft::Part xPart;
		xPart.m_pxRings   = axRings;
		xPart.m_uNumRings = 4u;
		xPart.m_uSegs     = uTEST_SEGS;
		xPart.m_bCapStart = true;
		xPart.m_bCapEnd   = true;
		xPart.m_uSubdiv   = uZM_GEN_RING_SUBDIV;
		ZM_MeshLoft::AppendPart(xMesh, xPart);

		ZM_GenNormalizeSkinWeights(xMesh);
	}

	// Seed-driven swept part: the ring radii are jittered from an explicit
	// ZM_GenRNG so "same seed => byte-identical mesh" is a real exercise of the
	// determinism claim (identical draw order + identical loft math).
	void BuildSeededMesh(ZM_GenRNG& xRng, ZM_GenMesh& xMesh)
	{
		xMesh.Reset();
		AddThreeChainBones(xMesh);
		ZM_LoftRing axRings[4];
		FillFixedRings(axRings);
		for (u_int i = 0; i < 4u; ++i)
		{
			axRings[i].m_fRx += xRng.NextFloatRange(-0.05f, 0.05f);
			axRings[i].m_fRz += xRng.NextFloatRange(-0.05f, 0.05f);
			axRings[i].m_fCx += xRng.NextSignedUnit() * 0.02f;
			axRings[i].m_fCz += xRng.NextSignedUnit() * 0.02f;
		}

		ZM_MeshLoft::Part xPart;
		xPart.m_pxRings   = axRings;
		xPart.m_uNumRings = 4u;
		xPart.m_uSegs     = uTEST_SEGS;
		xPart.m_bCapStart = true;
		xPart.m_bCapEnd   = true;
		xPart.m_uSubdiv   = uZM_GEN_RING_SUBDIV;
		ZM_MeshLoft::AppendPart(xMesh, xPart);

		// Run the two shipped finalizers so the normals AND tangents byte-compares
		// in Loft_SameSeedMeshByteIdentical exercise real (non-empty) buffers rather
		// than a vacuous size-0 vs size-0 match. Both are pure functions of the mesh,
		// so same-seed input stays byte-identical.
		ZM_GenGenerateNormals(xMesh);
		ZM_GenGenerateTangents(xMesh);

		ZM_GenNormalizeSkinWeights(xMesh);
	}

	// memcmp one SoA buffer pair (sizes must match first).
	template<typename T>
	bool BufferBytesEqual(const Zenith_Vector<T>& xA, const Zenith_Vector<T>& xB)
	{
		if (xA.GetSize() != xB.GetSize()) { return false; }
		if (xA.GetSize() == 0u)           { return true;  }
		return memcmp(xA.GetDataPointer(), xB.GetDataPointer(),
			(size_t)xA.GetSize() * sizeof(T)) == 0;
	}
}

// ############################################################################
// A. Generation RNG (ZM_Gen)
// ############################################################################

// Two RNGs on the same seed emit an identical first 4096 integer sequence AND an
// identical NextFloat01 sequence (bit-for-bit). The headline RNG-determinism gate.
ZENITH_TEST(ZM_Gen, RNG_SameSeedByteIdentical)
{
	ZM_GenRNG xA(0xABCDEF01ull);
	ZM_GenRNG xB(0xABCDEF01ull);

	u_int uFirstIntDivergence = 0xFFFFFFFFu;
	for (u_int i = 0; i < 4096u; ++i)
	{
		if (xA.Next() != xB.Next()) { uFirstIntDivergence = i; break; }
	}
	ZENITH_ASSERT_EQ(uFirstIntDivergence, 0xFFFFFFFFu,
		"same-seed integer streams diverged at draw %u", uFirstIntDivergence);

	// Separate same-seed pair for the float stream, bit-compared.
	ZM_GenRNG xFA(0xABCDEF01ull);
	ZM_GenRNG xFB(0xABCDEF01ull);
	float afA[1024];
	float afB[1024];
	for (u_int i = 0; i < 1024u; ++i) { afA[i] = xFA.NextFloat01(); }
	for (u_int i = 0; i < 1024u; ++i) { afB[i] = xFB.NextFloat01(); }
	ZENITH_ASSERT_EQ(memcmp(afA, afB, sizeof(afA)), 0,
		"same-seed NextFloat01 streams are not bit-identical");
}

// Distinct seeds diverge inside the first 8 integer draws (guards a stuck or
// aliased stream that ignores its seed).
ZENITH_TEST(ZM_Gen, RNG_DistinctSeedsDiffer)
{
	ZM_GenRNG xA(0x1111111111111111ull);
	ZM_GenRNG xB(0x2222222222222222ull);

	bool bDiverged = false;
	for (u_int i = 0; i < 8u; ++i)
	{
		if (xA.Next() != xB.Next()) { bDiverged = true; break; }
	}
	ZENITH_ASSERT_TRUE(bDiverged, "distinct seeds must diverge within 8 draws");
}

// The fixed integer->float helpers stay in their declared ranges over a large
// sample: NextFloat01 in [0,1), NextFloatRange(a,b) in [a,b), NextSignedUnit in
// [-1,1).
ZENITH_TEST(ZM_Gen, RNG_FloatUnitIntervalAndRange)
{
	ZM_GenRNG xRng(0xFEEDFACEull);

	const float fMin = -3.0f;
	const float fMax = 7.0f;
	for (u_int i = 0; i < 100000u; ++i)
	{
		const float f01 = xRng.NextFloat01();
		ZENITH_ASSERT_GE(f01, 0.0f, "NextFloat01 below 0 at draw %u", i);
		ZENITH_ASSERT_LT(f01, 1.0f, "NextFloat01 reached 1 at draw %u", i);

		const float fR = xRng.NextFloatRange(fMin, fMax);
		ZENITH_ASSERT_GE(fR, fMin, "NextFloatRange below min at draw %u", i);
		ZENITH_ASSERT_LT(fR, fMax, "NextFloatRange reached max at draw %u", i);

		const float fS = xRng.NextSignedUnit();
		ZENITH_ASSERT_GE(fS, -1.0f, "NextSignedUnit below -1 at draw %u", i);
		ZENITH_ASSERT_LT(fS, 1.0f, "NextSignedUnit reached 1 at draw %u", i);
	}
}

// ZM_GenRNG is a faithful, non-drifting forward of the golden-pinned PCG32: for a
// fixed (seed, sequence) its Next() equals a raw ZM_BattleRNG's Next() over 64
// draws (proves the generation stream shares the battle stream's exact bytes).
ZENITH_TEST(ZM_Gen, RNG_WrapsBattleGoldenStream)
{
	const u_int64 ulSeed = 0x1234567890ABCDEFull;
	const u_int64 ulSeq  = 54ull;

	ZM_GenRNG   xGen(ulSeed, ulSeq);
	ZM_BattleRNG xBattle(ulSeed, ulSeq);

	u_int uFirstDivergence = 0xFFFFFFFFu;
	for (u_int i = 0; i < 64u; ++i)
	{
		if (xGen.Next() != xBattle.Next()) { uFirstDivergence = i; break; }
	}
	ZENITH_ASSERT_EQ(uFirstDivergence, 0xFFFFFFFFu,
		"ZM_GenRNG diverged from the golden ZM_BattleRNG stream at draw %u", uFirstDivergence);
}

// ############################################################################
// B. Seed derivation + name hashing (ZM_Gen)
// ############################################################################

// ZM_GenDeriveSeed reproduces the frozen fold recipe (matched against the local
// oracle -- a change to the fold fails here and forces a version bump), is stable
// across calls, and a different domain yields a different 64-bit seed.
ZENITH_TEST(ZM_Gen, Seed_DeriveIsStableGolden)
{
	const u_int uFamily  = 0x7BF32CA4u;   // the Dawnmere terrain seed, as a stand-in family seed
	const u_int uSpecies = 42u;
	const u_int uEvo     = 1u;

	const u_int64 ulMesh   = ZM_GenDeriveSeed(uFamily, uSpecies, uEvo, ZM_GEN_DOMAIN_MESH);
	const u_int64 ulAlbedo = ZM_GenDeriveSeed(uFamily, uSpecies, uEvo, ZM_GEN_DOMAIN_ALBEDO);

	// Pinned against the independent recipe reconstruction.
	ZENITH_ASSERT_EQ(ulMesh, OracleDeriveSeed(uFamily, uSpecies, uEvo, ZM_GEN_DOMAIN_MESH),
		"MESH seed drifted from the frozen fold recipe");
	ZENITH_ASSERT_EQ(ulAlbedo, OracleDeriveSeed(uFamily, uSpecies, uEvo, ZM_GEN_DOMAIN_ALBEDO),
		"ALBEDO seed drifted from the frozen fold recipe");

	// Stable across repeated calls (pure function).
	ZENITH_ASSERT_EQ(ulMesh, ZM_GenDeriveSeed(uFamily, uSpecies, uEvo, ZM_GEN_DOMAIN_MESH),
		"derive must be a pure function of its inputs");

	// A different output domain gives a different seed.
	ZENITH_ASSERT_NE(ulMesh, ulAlbedo, "MESH and ALBEDO domains must not collide");
}

// Every distinct domain of one (family, species, evo) tuple gets its own seed,
// and the per-domain RNG first draws are uncorrelated (do not all coincide) --
// so adding a draw in one domain can never perturb another domain's bytes.
ZENITH_TEST(ZM_Gen, Seed_DomainStreamsDisjoint)
{
	const u_int uFamily  = 0xCAFEBABEu;
	const u_int uSpecies = 7u;
	const u_int uEvo     = 2u;

	u_int64 aulSeeds[ZM_GEN_DOMAIN_COUNT];
	u_int32 auFirst[ZM_GEN_DOMAIN_COUNT];
	for (u_int d = 0; d < (u_int)ZM_GEN_DOMAIN_COUNT; ++d)
	{
		aulSeeds[d] = ZM_GenDeriveSeed(uFamily, uSpecies, uEvo, (ZM_GEN_DOMAIN)d);
		ZM_GenRNG xRng(aulSeeds[d]);
		auFirst[d] = xRng.Next();
	}

	// Pairwise-distinct seeds.
	for (u_int a = 0; a < (u_int)ZM_GEN_DOMAIN_COUNT; ++a)
	{
		for (u_int b = a + 1u; b < (u_int)ZM_GEN_DOMAIN_COUNT; ++b)
		{
			ZENITH_ASSERT_NE(aulSeeds[a], aulSeeds[b],
				"domains %u and %u share a derived seed", a, b);
		}
	}

	// The MESH vs ALBEDO first draws must differ (guards a coincidental alias).
	ZENITH_ASSERT_NE(auFirst[ZM_GEN_DOMAIN_MESH], auFirst[ZM_GEN_DOMAIN_ALBEDO],
		"MESH and ALBEDO streams produce the same first draw");

	// And not every domain's first draw is identical (the streams are decorrelated).
	bool bAllEqual = true;
	for (u_int d = 1; d < (u_int)ZM_GEN_DOMAIN_COUNT; ++d)
	{
		if (auFirst[d] != auFirst[0]) { bAllEqual = false; break; }
	}
	ZENITH_ASSERT_FALSE(bAllEqual, "all domain streams coincide on their first draw");
}

// ZM_GenHashName is FNV-1a, byte-identical to the shipped terrain-seed hash
// ZM_Fnv1a32, and matches a hand-computed literal that pins the FNV constants.
ZENITH_TEST(ZM_Gen, Hash_Fnv1aMatchesTerrainAnchor)
{
	const char* aszProbe[] = { "", "A", "Dawnmere", "Nibbin", "ZM_GEN_DOMAIN_MESH", "species_0042" };
	for (u_int i = 0; i < (u_int)(sizeof(aszProbe) / sizeof(aszProbe[0])); ++i)
	{
		ZENITH_ASSERT_EQ(ZM_GenHashName(aszProbe[i]), ZM_Fnv1a32(aszProbe[i]),
			"ZM_GenHashName vs ZM_Fnv1a32 mismatch on probe %u", i);
		ZENITH_ASSERT_EQ(ZM_GenHashName(aszProbe[i]), OracleFnv1a32(aszProbe[i]),
			"ZM_GenHashName vs local FNV oracle mismatch on probe %u", i);
	}

	// The empty string is the FNV-1a offset basis; "A" (0x41) is one full round.
	ZENITH_ASSERT_EQ(ZM_GenHashName(""),  2166136261u, "FNV offset basis pinned");
	ZENITH_ASSERT_EQ(ZM_GenHashName("A"), 0xC40BF6CCu, "FNV constants (basis/prime) pinned via \"A\"");
}

// ############################################################################
// C. Integer-hash noise (ZM_Gen)
// ############################################################################

// ValueNoise2D / FBM2D / RidgedFBM2D reproduce bit-identically over a fixed
// (x, y, seed) sample grid across two calls (integer-hash, no float-order drift).
ZENITH_TEST(ZM_Gen, Noise_SameSeedByteIdentical)
{
	const u_int uSeed = 0x51EED00Du;
	constexpr u_int uGrid = 16u;
	float afValueA[uGrid * uGrid],  afValueB[uGrid * uGrid];
	float afFbmA[uGrid * uGrid],    afFbmB[uGrid * uGrid];
	float afRidgeA[uGrid * uGrid],  afRidgeB[uGrid * uGrid];

	for (u_int pass = 0; pass < 2u; ++pass)
	{
		float* pfValue = (pass == 0u) ? afValueA : afValueB;
		float* pfFbm   = (pass == 0u) ? afFbmA   : afFbmB;
		float* pfRidge = (pass == 0u) ? afRidgeA : afRidgeB;
		for (u_int y = 0; y < uGrid; ++y)
		{
			for (u_int x = 0; x < uGrid; ++x)
			{
				const float fX = (float)x * 0.37f;
				const float fY = (float)y * 0.37f;
				const u_int i = y * uGrid + x;
				pfValue[i] = ZM_GenNoise::ValueNoise2D(fX, fY, uSeed);
				pfFbm[i]   = ZM_GenNoise::FBM2D(fX, fY, uSeed, 5u, 2.0f, 0.5f);
				pfRidge[i] = ZM_GenNoise::RidgedFBM2D(fX, fY, uSeed, 5u, 2.0f, 0.5f);
			}
		}
	}

	ZENITH_ASSERT_EQ(memcmp(afValueA, afValueB, sizeof(afValueA)), 0, "ValueNoise2D not reproducible");
	ZENITH_ASSERT_EQ(memcmp(afFbmA,   afFbmB,   sizeof(afFbmA)),   0, "FBM2D not reproducible");
	ZENITH_ASSERT_EQ(memcmp(afRidgeA, afRidgeB, sizeof(afRidgeA)), 0, "RidgedFBM2D not reproducible");
}

// ValueNoise2D, FBM2D and RidgedFBM2D all stay within [0,1] over a sampled grid.
ZENITH_TEST(ZM_Gen, Noise_OutputsBounded)
{
	const u_int uSeed = 0xB0A710C5u;
	for (u_int y = 0; y < 32u; ++y)
	{
		for (u_int x = 0; x < 32u; ++x)
		{
			const float fX = (float)x * 0.53f - 7.0f;   // include negative coords
			const float fY = (float)y * 0.53f - 7.0f;

			const float fV = ZM_GenNoise::ValueNoise2D(fX, fY, uSeed);
			ZENITH_ASSERT_GE(fV, 0.0f, "ValueNoise2D < 0 at (%u,%u)", x, y);
			ZENITH_ASSERT_LE(fV, 1.0f, "ValueNoise2D > 1 at (%u,%u)", x, y);

			const float fF = ZM_GenNoise::FBM2D(fX, fY, uSeed, 5u, 2.0f, 0.5f);
			ZENITH_ASSERT_GE(fF, 0.0f, "FBM2D < 0 at (%u,%u)", x, y);
			ZENITH_ASSERT_LE(fF, 1.0f, "FBM2D > 1 at (%u,%u)", x, y);

			const float fR = ZM_GenNoise::RidgedFBM2D(fX, fY, uSeed, 5u, 2.0f, 0.5f);
			ZENITH_ASSERT_GE(fR, 0.0f, "RidgedFBM2D < 0 at (%u,%u)", x, y);
			ZENITH_ASSERT_LE(fR, 1.0f, "RidgedFBM2D > 1 at (%u,%u)", x, y);
		}
	}
}

// ############################################################################
// D. Loft / mesh toolkit (ZM_Gen)
// ############################################################################

// Building the same seeded ring-table loft twice yields memcmp-identical SoA
// buffers (positions / normals / uvs / tangents / indices / bone indices / bone
// weights). The headline mesh-determinism gate.
ZENITH_TEST(ZM_Gen, Loft_SameSeedMeshByteIdentical)
{
	ZM_GenMesh xA;
	ZM_GenMesh xB;
	ZM_GenRNG xRngA(0x9E3779B97F4A7C15ull);
	ZM_GenRNG xRngB(0x9E3779B97F4A7C15ull);
	BuildSeededMesh(xRngA, xA);
	BuildSeededMesh(xRngB, xB);

	ZENITH_ASSERT_GT(xA.GetNumVerts(), 0u, "mesh must be non-empty");
	ZENITH_ASSERT_GT(xA.m_xTangents.GetSize(), 0u, "tangents must be generated (guards a vacuous determinism compare)");
	ZENITH_ASSERT_EQ(xA.GetNumVerts(), xB.GetNumVerts(), "vertex counts differ");
	ZENITH_ASSERT_EQ(xA.GetNumTris(),  xB.GetNumTris(),  "triangle counts differ");

	ZENITH_ASSERT_TRUE(BufferBytesEqual(xA.m_xPositions,   xB.m_xPositions),   "positions differ");
	ZENITH_ASSERT_TRUE(BufferBytesEqual(xA.m_xNormals,     xB.m_xNormals),     "normals differ");
	ZENITH_ASSERT_TRUE(BufferBytesEqual(xA.m_xUVs,         xB.m_xUVs),         "uvs differ");
	ZENITH_ASSERT_TRUE(BufferBytesEqual(xA.m_xTangents,    xB.m_xTangents),    "tangents differ");
	ZENITH_ASSERT_TRUE(BufferBytesEqual(xA.m_xIndices,     xB.m_xIndices),     "indices differ");
	ZENITH_ASSERT_TRUE(BufferBytesEqual(xA.m_xBoneIndices, xB.m_xBoneIndices), "bone indices differ");
	ZENITH_ASSERT_TRUE(BufferBytesEqual(xA.m_xBoneWeights, xB.m_xBoneWeights), "bone weights differ");
}

// Every triangle winds outward: ZM_ValidateGenMesh reports m_bWindingOutward
// (each tri cross(C-A,B-A) . avg(vertex normal) > 0) across ring/stitch/cap output.
ZENITH_TEST(ZM_Gen, Loft_WindingOutward)
{
	ZM_GenMesh xMesh;
	BuildFixedMesh(xMesh);
	const ZM_GenMeshValidation xVal = ZM_ValidateGenMesh(xMesh, uZM_GEN_CREATURE_BONE_CAP);
	ZENITH_ASSERT_TRUE(xVal.m_bWindingOutward,
		"loft winding not outward (first bad tri %u)", xVal.m_uFirstBadTriangle);
}

// The bounds are non-degenerate: extent exceeds epsilon on all three axes and
// min < max on each.
ZENITH_TEST(ZM_Gen, Loft_BoundsNonDegenerate)
{
	ZM_GenMesh xMesh;
	BuildFixedMesh(xMesh);
	const Zenith_Maths::Vector3 xMin = ZM_GenMeshBoundsMin(xMesh);
	const Zenith_Maths::Vector3 xMax = ZM_GenMeshBoundsMax(xMesh);

	ZENITH_ASSERT_GT(xMax.x - xMin.x, fWEIGHT_TOL, "X extent degenerate");
	ZENITH_ASSERT_GT(xMax.y - xMin.y, fWEIGHT_TOL, "Y extent degenerate");
	ZENITH_ASSERT_GT(xMax.z - xMin.z, fWEIGHT_TOL, "Z extent degenerate");
	ZENITH_ASSERT_LT(xMin.x, xMax.x, "X min !< max");
	ZENITH_ASSERT_LT(xMin.y, xMax.y, "Y min !< max");
	ZENITH_ASSERT_LT(xMin.z, xMax.z, "Z min !< max");

	// The validator agrees.
	const ZM_GenMeshValidation xVal = ZM_ValidateGenMesh(xMesh, uZM_GEN_CREATURE_BONE_CAP);
	ZENITH_ASSERT_TRUE(xVal.m_bBoundsNonDegen, "validator flagged degenerate bounds");
}

// After ZM_GenNormalizeSkinWeights every vertex's four weights sum to 1 within
// tolerance and at most two are non-zero (the ring-bind contract).
ZENITH_TEST(ZM_Gen, Loft_SkinWeightsSumToOne)
{
	ZM_GenMesh xMesh;
	BuildFixedMesh(xMesh);   // BuildFixedMesh already normalises

	ZENITH_ASSERT_GT(xMesh.m_xBoneWeights.GetSize(), 0u, "no bone weights emitted");
	ZENITH_ASSERT_EQ(xMesh.m_xBoneWeights.GetSize(), xMesh.GetNumVerts(), "one weight-4 per vertex");

	u_int uFirstBad = 0xFFFFFFFFu;
	u_int uFirstOverInfluenced = 0xFFFFFFFFu;
	for (u_int v = 0; v < xMesh.m_xBoneWeights.GetSize(); ++v)
	{
		const glm::vec4& xW = xMesh.m_xBoneWeights.Get(v);
		const float fSum = xW.x + xW.y + xW.z + xW.w;
		if (fabsf(fSum - 1.0f) > fWEIGHT_TOL && uFirstBad == 0xFFFFFFFFu) { uFirstBad = v; }

		u_int uNonZero = 0u;
		if (fabsf(xW.x) > fWEIGHT_EPS) { ++uNonZero; }
		if (fabsf(xW.y) > fWEIGHT_EPS) { ++uNonZero; }
		if (fabsf(xW.z) > fWEIGHT_EPS) { ++uNonZero; }
		if (fabsf(xW.w) > fWEIGHT_EPS) { ++uNonZero; }
		if (uNonZero > 2u && uFirstOverInfluenced == 0xFFFFFFFFu) { uFirstOverInfluenced = v; }
	}
	ZENITH_ASSERT_EQ(uFirstBad, 0xFFFFFFFFu, "vertex %u weights do not sum to 1", uFirstBad);
	ZENITH_ASSERT_EQ(uFirstOverInfluenced, 0xFFFFFFFFu,
		"vertex %u has more than 2 non-zero influences", uFirstOverInfluenced);

	const ZM_GenMeshValidation xVal = ZM_ValidateGenMesh(xMesh, uZM_GEN_CREATURE_BONE_CAP);
	ZENITH_ASSERT_TRUE(xVal.m_bWeightsSumToOne,  "validator flagged weight-sum");
	ZENITH_ASSERT_TRUE(xVal.m_bWeightsAtMostTwo, "validator flagged >2 influences");
}

// No vertex references a bone index outside [0, GetNumBones()) (no dangling
// influence).
ZENITH_TEST(ZM_Gen, Loft_BoneIndicesInRange)
{
	ZM_GenMesh xMesh;
	BuildFixedMesh(xMesh);
	const u_int uNumBones = xMesh.GetNumBones();
	ZENITH_ASSERT_GT(uNumBones, 0u, "mesh must have bones");
	ZENITH_ASSERT_EQ(xMesh.m_xBoneIndices.GetSize(), xMesh.GetNumVerts(), "one index-4 per vertex");

	u_int uFirstBad = 0xFFFFFFFFu;
	for (u_int v = 0; v < xMesh.m_xBoneIndices.GetSize(); ++v)
	{
		const glm::uvec4& xI = xMesh.m_xBoneIndices.Get(v);
		if ((xI.x >= uNumBones || xI.y >= uNumBones || xI.z >= uNumBones || xI.w >= uNumBones)
			&& uFirstBad == 0xFFFFFFFFu)
		{
			uFirstBad = v;
		}
	}
	ZENITH_ASSERT_EQ(uFirstBad, 0xFFFFFFFFu, "vertex %u references an out-of-range bone", uFirstBad);
}

// EmitRing emits uSegs+1 vertices; the first and last share a position but carry
// a distinct UV.u -- the -Z seam-wrap invariant.
ZENITH_TEST(ZM_Gen, Loft_SeamColumnDuplicated)
{
	ZM_GenMesh xMesh;
	AddThreeChainBones(xMesh);

	ZM_LoftRing xRing;
	xRing.m_fY = 0.5f; xRing.m_fRx = 0.7f; xRing.m_fRz = 0.5f;
	xRing.m_uBoneA = 0u; xRing.m_uBoneB = 1u; xRing.m_fBlendB = 0.25f;

	const ZM_GenUVIsland xIsland;   // default full [0,1] island
	const u_int uFirst = ZM_MeshLoft::EmitRing(xMesh, xRing, uTEST_SEGS, xIsland, 0.5f);

	ZENITH_ASSERT_EQ(xMesh.GetNumVerts(), uTEST_SEGS + 1u, "ring must emit uSegs+1 verts");

	const Zenith_Maths::Vector3& xP0   = xMesh.m_xPositions.Get(uFirst);
	const Zenith_Maths::Vector3& xPEnd = xMesh.m_xPositions.Get(uFirst + uTEST_SEGS);
	ZENITH_ASSERT_LT(fabsf(xP0.x - xPEnd.x), fPOS_TOL, "seam X mismatch");
	ZENITH_ASSERT_LT(fabsf(xP0.y - xPEnd.y), fPOS_TOL, "seam Y mismatch");
	ZENITH_ASSERT_LT(fabsf(xP0.z - xPEnd.z), fPOS_TOL, "seam Z mismatch");

	const Zenith_Maths::Vector2& xUV0   = xMesh.m_xUVs.Get(uFirst);
	const Zenith_Maths::Vector2& xUVEnd = xMesh.m_xUVs.Get(uFirst + uTEST_SEGS);
	ZENITH_ASSERT_GT(fabsf(xUV0.x - xUVEnd.x), fPOS_TOL, "seam columns must carry distinct U");
}

// AppendPart with subdivision > 0 places the authored-ring vertices exactly (at
// Catmull-Rom t==0) -- the authored-ring-preserved property. The first emitted
// ring must match a standalone EmitRing of the same authored ring, position-wise.
ZENITH_TEST(ZM_Gen, Loft_SubdivT0ReproducesRings)
{
	ZM_LoftRing axRings[4];
	FillFixedRings(axRings);

	// Part WITHOUT caps so the first uSegs+1 output verts are exactly authored ring 0.
	ZM_GenMesh xPartMesh;
	AddThreeChainBones(xPartMesh);
	ZM_MeshLoft::Part xPart;
	xPart.m_pxRings   = axRings;
	xPart.m_uNumRings = 4u;
	xPart.m_uSegs     = uTEST_SEGS;
	xPart.m_bCapStart = false;
	xPart.m_bCapEnd   = false;
	xPart.m_uSubdiv   = uZM_GEN_RING_SUBDIV;   // > 0: subdivision active
	const u_int uPartFirst = ZM_MeshLoft::AppendPart(xPartMesh, xPart);

	// Standalone authored ring 0 as the reference.
	ZM_GenMesh xRefMesh;
	AddThreeChainBones(xRefMesh);
	const ZM_GenUVIsland xIsland;
	const u_int uRefFirst = ZM_MeshLoft::EmitRing(xRefMesh, axRings[0], uTEST_SEGS, xIsland, 0.0f);

	ZENITH_ASSERT_GE(xPartMesh.GetNumVerts(), uTEST_SEGS + 1u, "part is missing the first ring");
	for (u_int i = 0; i <= uTEST_SEGS; ++i)
	{
		const Zenith_Maths::Vector3& xPart3 = xPartMesh.m_xPositions.Get(uPartFirst + i);
		const Zenith_Maths::Vector3& xRef3  = xRefMesh.m_xPositions.Get(uRefFirst + i);
		ZENITH_ASSERT_LT(fabsf(xPart3.x - xRef3.x), fPOS_TOL, "ring0 vert %u X drift at t==0", i);
		ZENITH_ASSERT_LT(fabsf(xPart3.y - xRef3.y), fPOS_TOL, "ring0 vert %u Y drift at t==0", i);
		ZENITH_ASSERT_LT(fabsf(xPart3.z - xRef3.z), fPOS_TOL, "ring0 vert %u Z drift at t==0", i);
	}
}

// ############################################################################
// E. Skeleton topology (ZM_Gen)
// ############################################################################

namespace
{
	// A representative creature skeleton (root spine + head + four limbs + tail),
	// well under the creature bone cap, built strictly parent-before-child.
	void BuildCreatureSkeleton(ZM_GenMesh& xMesh)
	{
		xMesh.Reset();
		const Zenith_Maths::Quat xIdent = glm::identity<Zenith_Maths::Quat>();
		const Zenith_Maths::Vector3 xOne(1.0f);
		const Zenith_Maths::Vector3 xUp(0.0f, 0.5f, 0.0f);

		const u_int uRoot  = ZM_GenAddBone(xMesh, "Root",  -1,          xUp, xIdent, xOne);
		const u_int uSpine = ZM_GenAddBone(xMesh, "Spine", (int)uRoot,  xUp, xIdent, xOne);
		const u_int uChest = ZM_GenAddBone(xMesh, "Chest", (int)uSpine, xUp, xIdent, xOne);
		const u_int uHead  = ZM_GenAddBone(xMesh, "Head",  (int)uChest, xUp, xIdent, xOne);
		ZM_GenAddBone(xMesh, "LegFL", (int)uChest, xUp, xIdent, xOne);
		ZM_GenAddBone(xMesh, "LegFR", (int)uChest, xUp, xIdent, xOne);
		ZM_GenAddBone(xMesh, "LegBL", (int)uRoot,  xUp, xIdent, xOne);
		ZM_GenAddBone(xMesh, "LegBR", (int)uRoot,  xUp, xIdent, xOne);
		ZM_GenAddBone(xMesh, "Tail",  (int)uRoot,  xUp, xIdent, xOne);
		ZM_GenAddBone(xMesh, "Jaw",   (int)uHead,  xUp, xIdent, xOne);
	}
}

// A creature-topology skeleton stays within the creature bone cap (30).
ZENITH_TEST(ZM_Gen, Skeleton_BoneCapCreature)
{
	ZM_GenMesh xMesh;
	BuildCreatureSkeleton(xMesh);
	ZENITH_ASSERT_GT(xMesh.GetNumBones(), 0u, "creature skeleton must have bones");
	ZENITH_ASSERT_LE(xMesh.GetNumBones(), uZM_GEN_CREATURE_BONE_CAP,
		"creature skeleton exceeds the creature bone cap");

	// The validator's bone-cap flag agrees when the cap is the creature cap; the
	// mesh has no geometry, so only the cap flag is meaningful here.
	const ZM_GenMeshValidation xVal = ZM_ValidateGenMesh(xMesh, uZM_GEN_CREATURE_BONE_CAP);
	ZENITH_ASSERT_TRUE(xVal.m_bBonesWithinCap, "validator flagged the creature bone cap");
}

// A skeleton filled exactly to the engine ceiling reports GetNumBones() == 100
// (Zenith_SkeletonAsset::MAX_BONES) and is flagged within a cap of 100. (Adding a
// 101st bone would trip ZM_GenAddBone's hard assert; that path is intentionally
// not exercised from a unit test -- see the report notes.)
ZENITH_TEST(ZM_Gen, Skeleton_BoneCapEngineMax)
{
	constexpr u_int uENGINE_MAX_BONES = 100u;   // == Zenith_SkeletonAsset::MAX_BONES
	ZM_GenMesh xMesh;
	const Zenith_Maths::Quat xIdent = glm::identity<Zenith_Maths::Quat>();
	const Zenith_Maths::Vector3 xOne(1.0f);
	const Zenith_Maths::Vector3 xStep(0.0f, 0.1f, 0.0f);

	ZM_GenAddBone(xMesh, "B000", -1, xStep, xIdent, xOne);
	for (u_int i = 1; i < uENGINE_MAX_BONES; ++i)
	{
		char acName[uZM_GEN_BONE_NAME_MAX];
		snprintf(acName, sizeof(acName), "B%03u", i);
		ZM_GenAddBone(xMesh, acName, (int)(i - 1u), xStep, xIdent, xOne);
	}

	ZENITH_ASSERT_EQ(xMesh.GetNumBones(), uENGINE_MAX_BONES, "must fill exactly to the engine ceiling");
	ZENITH_ASSERT_LE(xMesh.GetNumBones(), uENGINE_MAX_BONES, "must not exceed the engine ceiling");

	const ZM_GenMeshValidation xVal = ZM_ValidateGenMesh(xMesh, uENGINE_MAX_BONES);
	ZENITH_ASSERT_TRUE(xVal.m_bBonesWithinCap, "validator flagged the engine bone cap at exactly MAX_BONES");
}

// Every bone's parent index is < its own index (or -1 for a root): the AddBone
// parent-before-child precondition holds by construction.
ZENITH_TEST(ZM_Gen, Skeleton_ParentsBeforeChildren)
{
	ZM_GenMesh xMesh;
	BuildCreatureSkeleton(xMesh);

	u_int uRootCount = 0u;
	u_int uFirstBad = 0xFFFFFFFFu;
	for (u_int u = 0; u < xMesh.GetNumBones(); ++u)
	{
		const int iParent = xMesh.m_xBones.Get(u).m_iParent;
		if (iParent == -1) { ++uRootCount; continue; }
		if (!(iParent < (int)u) && uFirstBad == 0xFFFFFFFFu) { uFirstBad = u; }
	}
	ZENITH_ASSERT_EQ(uFirstBad, 0xFFFFFFFFu, "bone %u parent is not before it", uFirstBad);
	ZENITH_ASSERT_GE(uRootCount, 1u, "skeleton must have at least one root");
}

// The archetype's clip-template channel names all resolve to real bones via
// ZM_GenMeshFindBone (the anim-gen binding contract); an absent name returns -1.
ZENITH_TEST(ZM_Gen, Skeleton_TopologyMatchesClipChannels)
{
	ZM_GenMesh xMesh;
	BuildCreatureSkeleton(xMesh);

	// A fixture archetype's 6-channel clip template.
	const char* aszChannels[6] = { "Root", "Spine", "Head", "LegFL", "LegBR", "Tail" };
	for (u_int i = 0; i < 6u; ++i)
	{
		const int iBone = ZM_GenMeshFindBone(xMesh, aszChannels[i]);
		ZENITH_ASSERT_GE(iBone, 0, "clip channel '%s' has no matching bone", aszChannels[i]);
		ZENITH_ASSERT_LT(iBone, (int)xMesh.GetNumBones(), "channel '%s' resolved out of range", aszChannels[i]);
	}

	// A channel with no bone must report absence (not a false positive match).
	ZENITH_ASSERT_EQ(ZM_GenMeshFindBone(xMesh, "NoSuchBone"), -1, "absent bone must resolve to -1");
}

// ############################################################################
// F. Validator self-check (ZM_Gen)
// ############################################################################

// ZM_ValidateGenMesh on a hand-built bad mesh (zero-extent positions + a vertex
// whose weights sum to 0.5) flags m_bBoundsNonDegen=false and
// m_bWeightsSumToOne=false, and reports a first-bad vertex + first-bad triangle --
// proving the validator itself detects faults (not just passes clean input).
ZENITH_TEST(ZM_Gen, Validate_FlagsDegenerateAndBadWeights)
{
	ZM_GenMesh xMesh;

	// One bone so bone indices are in range and the cap holds.
	ZM_GenAddBone(xMesh, "Root", -1, Zenith_Maths::Vector3(0.0f), glm::identity<Zenith_Maths::Quat>(),
		Zenith_Maths::Vector3(1.0f));

	// Three coincident vertices -> zero-extent bounds AND a degenerate (zero-area)
	// triangle whose face normal cannot agree with any vertex normal.
	for (u_int i = 0; i < 3u; ++i)
	{
		xMesh.m_xPositions.PushBack(Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f));
		xMesh.m_xNormals.PushBack(Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
		xMesh.m_xUVs.PushBack(Zenith_Maths::Vector2(0.0f, 0.0f));
		xMesh.m_xTangents.PushBack(Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
		xMesh.m_xColors.PushBack(Zenith_Maths::Vector4(1.0f));
		xMesh.m_xBoneIndices.PushBack(glm::uvec4(0u, 0u, 0u, 0u));
		xMesh.m_xBoneWeights.PushBack(glm::vec4(0.5f, 0.0f, 0.0f, 0.0f));   // sums to 0.5, not 1
	}
	xMesh.m_xIndices.PushBack(0u);
	xMesh.m_xIndices.PushBack(1u);
	xMesh.m_xIndices.PushBack(2u);

	const ZM_GenMeshValidation xVal = ZM_ValidateGenMesh(xMesh, uZM_GEN_CREATURE_BONE_CAP);

	ZENITH_ASSERT_FALSE(xVal.m_bBoundsNonDegen,  "zero-extent bounds must be flagged");
	ZENITH_ASSERT_FALSE(xVal.m_bWeightsSumToOne, "weights summing to 0.5 must be flagged");
	ZENITH_ASSERT_NE(xVal.m_uFirstBadVertex,   0xFFFFFFFFu, "a first bad vertex must be reported");
	ZENITH_ASSERT_NE(xVal.m_uFirstBadTriangle, 0xFFFFFFFFu, "a first bad triangle must be reported");
	// The single bone is within the cap even in a broken mesh.
	ZENITH_ASSERT_TRUE(xVal.m_bBonesWithinCap, "one bone must be within the cap");
}
