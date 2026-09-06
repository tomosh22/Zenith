#include "Zenith.h"

// ============================================================================
// ZM_Tests_CreatureAnimGen -- S4 unit gate for ZM_CreatureAnimGen (suite ZM_Gen).
// Born in SC1 (QUADRUPED); as of SC5 the anim dispatch is TOTAL, so this generic
// harness now covers all 8 archetypes.
//
// This is the GENERIC, contract-driven creature-ANIMATION harness: it authors
// against the frozen seam (Games/Zenithmon/Source/Gen/ZM_CreatureAnimGen.h),
// NEVER against a specific archetype builder .cpp. It loops every ZM_SPECIES_ID
// whose ZM_GetArchetypeAnimBuilder is non-null; as of SC5 the dispatch is TOTAL --
// all 8 archetypes are wired, so the harness exercises the COMPLETE wired set (the
// non-null guard below is now defensive future-proofing, not a real skip), and
// proves the load-bearing S4 clip invariants:
//   (1) every channel binds to a real skeleton bone (no dead channels) + every
//       authored quat is finite and ~unit-length      -- ChannelsMatchSkeleton
//   (2) the whole ZM_ValidateCreatureClip contract holds -- ValidationPasses
//   (3) golden clip metadata (names / durations / looping / ticks-per-second)
//  (3b) D7/D8 rig identity: a species' clip names that species' OWN .zskel /
//       .zmodel and is flagged m_bGenerated -- SpeciesClipsNameTheirOwnRig; and the
//       identity differs across two species of one archetype while the MOTION does
//       not                                    -- RigIdentityDiffersWhileMotionDoesNot
//   (4) clips are PURE f(archetype, clip): byte-identical across two species of
//       one archetype                                  -- SameArchetypeByteIdentical
//   (5) same-inputs determinism (repeat build byte-identical)
//   (6) meaningfully-different clips are actually distinct (no motion collision)
//   (7) looping clips (Idle / Walk) wrap cleanly; one-shots do not loop
//   (8) Faint clamps past the end (KO pose holds, no extrapolation)
//  (11) D3: every key time is SECONDS on the clip's own clock -- no key past the
//       duration, and a looping clip's last key ON it -- KeyTimesAreSeconds
//
// PURE / HEADLESS: no disk, no GPU, no ZENITH_TOOLS reach (the .zanim bake is
// compiled out). Clips draw from NO RNG; a clip is a closed-form function of
// (archetype, clip-id) only. These run at boot before the scene loads.
//
// The seam already provides the byte-equality / content-hash / validation
// helpers -- this harness USES them; it never re-derives curve maths.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Gen/ZM_CreatureAnimGen.h"
#include "Zenithmon/Source/Gen/ZM_CreatureGen.h"     // ZM_BuildCreatureMesh, ZM_ResolveCreatureRecipe, ZM_GetArchetypeBuilder
#include "Zenithmon/Source/Gen/ZM_GenCommon.h"        // ZM_GenMesh, ZM_GenMeshFindBone
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"
#include "Flux/MeshAnimation/Flux_AnimationClip.h"
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_HashMap.h"
#include "Collections/Zenith_Vector.h"

#include <cmath>     // std::isfinite, fabsf
#include <string>
#include <utility>   // std::pair

namespace
{
	// Quaternion closeness epsilons.
	constexpr float fUNIT_TOL   = 1.0e-3f;   // |len^2 - 1| tolerance for "unit-length"
	constexpr float fDOT_CLOSE  = 0.999f;    // |dot| >= this => same orientation (double-cover aware)
	constexpr float fDOT_DIFFER = 0.99f;     // |dot| <= this => clearly different orientation

	// Keyframe-TIME tolerance, in SECONDS (D3). This used to be 0.05 TICKS, on the
	// reasoning that authored ticks are near-integers; the same instant is now a
	// second, so the tolerance is scaled by the 24-per-second grid to keep it the
	// same physical slack rather than silently becoming 24x looser.
	constexpr float fTIME_TOL   = 0.05f / static_cast<float>(uZM_CREATURE_ANIM_TICKS_PER_SECOND);

	// Golden per-clip metadata (the version-bump contract; mirrors the header's
	// LOCKED table). Any change here is a deliberate .zanim re-bake.
	struct ZM_ClipGolden
	{
		ZM_ANIM_CLIP m_eClip;
		const char*  m_szName;
		float        m_fDurationSeconds;
		bool         m_bLooping;
	};
	const ZM_ClipGolden g_axGolden[ZM_ANIM_CLIP_COUNT] =
	{
		{ ZM_ANIM_CLIP_IDLE,    "Idle",    2.0f, true  },
		{ ZM_ANIM_CLIP_WALK,    "Walk",    1.0f, true  },
		{ ZM_ANIM_CLIP_ATTACK,  "Attack",  0.7f, false },
		{ ZM_ANIM_CLIP_SPECIAL, "Special", 0.9f, false },
		{ ZM_ANIM_CLIP_HIT,     "Hit",     0.4f, false },
		{ ZM_ANIM_CLIP_FAINT,   "Faint",   1.2f, false },
	};

	// True when the species' body plan has a wired ANIM builder. As of SC5 every
	// archetype is wired, so this is true for all species (the harness's nullptr
	// guard is kept as defensive future-proofing, not an active skip).
	bool HasAnimBuilder(ZM_SPECIES_ID eId)
	{
		return ZM_GetArchetypeAnimBuilder(ZM_GetSpeciesData(eId).m_eArchetype) != nullptr;
	}

	// True when the species' body plan has a wired MESH builder (needed to build
	// the per-species skeleton the clip channels bind against).
	bool HasMeshBuilder(ZM_SPECIES_ID eId)
	{
		return ZM_GetArchetypeBuilder(ZM_GetSpeciesData(eId).m_eArchetype) != nullptr;
	}

	bool QuatFinite(const Zenith_Maths::Quat& xQ)
	{
		return std::isfinite(xQ.w) && std::isfinite(xQ.x) && std::isfinite(xQ.y) && std::isfinite(xQ.z);
	}

	bool QuatUnit(const Zenith_Maths::Quat& xQ)
	{
		const float fLen2 = xQ.w * xQ.w + xQ.x * xQ.x + xQ.y * xQ.y + xQ.z * xQ.z;
		return fabsf(fLen2 - 1.0f) <= fUNIT_TOL;
	}

	// Double-cover-aware orientation closeness: |dot| in [0,1], 1 == identical.
	float QuatAbsDot(const Zenith_Maths::Quat& xA, const Zenith_Maths::Quat& xB)
	{
		return fabsf(xA.w * xB.w + xA.x * xB.x + xA.y * xB.y + xA.z * xB.z);
	}

	// Build the QUADRUPED reference clip for a clip-id into a fresh clip (used by
	// the metadata-golden test, whose stamp check is archetype-independent).
	void BuildQuadClip(ZM_ANIM_CLIP eClip, Flux_AnimationClip& xOut)
	{
		ZM_BuildCreatureClip(ZM_ARCHETYPE_QUADRUPED, eClip, xOut);
	}

	// Enumerate the archetypes that have a wired anim builder into aeOut (capacity
	// uCap), returning the count. Iterates [0, ZM_ARCHETYPE_COUNT); as of SC5 all 8
	// archetypes are wired, so this returns the COMPLETE archetype set. The generic
	// byte-identity / determinism / distinctness / loop-wrap / faint-clamp /
	// end-neutral gates below all iterate this set, so every archetype is exercised.
	u_int WiredAnimArchetypes(ZM_ARCHETYPE* aeOut, u_int uCap)
	{
		u_int uCount = 0u;
		for (u_int a = 0; a < (u_int)ZM_ARCHETYPE_COUNT; ++a)
		{
			const ZM_ARCHETYPE eArch = (ZM_ARCHETYPE)a;
			if (ZM_GetArchetypeAnimBuilder(eArch) != nullptr)
			{
				if (uCount < uCap) { aeOut[uCount] = eArch; }
				++uCount;
			}
		}
		return uCount;
	}

	// Find up to two DISTINCT species whose body plan is eArch (both are anim-
	// buildable, since eArch is a wired archetype). Writes them into eOutA / eOutB
	// and returns how many were found (0, 1, or 2). Species order follows the
	// ZM_SPECIES_ID enum, so the pick is deterministic.
	u_int FindTwoSpeciesOfArchetype(ZM_ARCHETYPE eArch, ZM_SPECIES_ID& eOutA, ZM_SPECIES_ID& eOutB)
	{
		u_int uFound = 0u;
		for (u_int id = 0; id < (u_int)ZM_SPECIES_COUNT; ++id)
		{
			const ZM_SPECIES_ID eId = (ZM_SPECIES_ID)id;
			if (ZM_GetSpeciesData(eId).m_eArchetype != eArch) { continue; }
			if (!HasAnimBuilder(eId)) { continue; }   // defensive; always true for a wired archetype
			if (uFound == 0u) { eOutA = eId; uFound = 1u; }
			else              { eOutB = eId; uFound = 2u; break; }
		}
		return uFound;
	}
}

// ############################################################################
// (1) The core playability proof: every channel binds to the skeleton, every
//     authored quat is finite + ~unit-length, no dead channels / skeleton drift.
// ############################################################################

ZENITH_TEST(ZM_Gen, CreatureAnimGen_ChannelsMatchSkeleton)
{
	u_int uTested = 0u;
	for (u_int id = 0; id < (u_int)ZM_SPECIES_COUNT; ++id)
	{
		const ZM_SPECIES_ID eId = (ZM_SPECIES_ID)id;
		if (!HasAnimBuilder(eId) || !HasMeshBuilder(eId)) { continue; }   // defensive guard (all 8 archetypes wired -- never skips)
		++uTested;

		// Build the species' skeleton mesh ONCE, reuse across all 6 clips.
		ZM_GenMesh xMesh;
		ZM_BuildCreatureMesh(ZM_ResolveCreatureRecipe(eId), xMesh);
		const ZM_ARCHETYPE eArch = ZM_GetSpeciesData(eId).m_eArchetype;

		for (u_int c = 0; c < (u_int)ZM_ANIM_CLIP_COUNT; ++c)
		{
			const ZM_ANIM_CLIP eClip = (ZM_ANIM_CLIP)c;
			Flux_AnimationClip xClip;
			ZM_BuildCreatureClip(eArch, eClip, xClip);

			const Zenith_HashMap<std::string, Flux_BoneChannel>& xChannels = xClip.GetBoneChannels();
			ZENITH_ASSERT_GT(xChannels.GetSize(), 0u,
				"species %u clip %u has no bone channels", id, c);

			Zenith_HashMap<std::string, Flux_BoneChannel>::Iterator xIt(xChannels);
			for (; !xIt.Done(); xIt.Next())
			{
				const Flux_BoneChannel& xChannel = xIt.GetValue();
				const std::string& strBone = xChannel.GetBoneName();

				// Rotation-only clips: every channel must carry rotation keyframes.
				ZENITH_ASSERT_TRUE(xChannel.HasRotationKeyframes(),
					"species %u clip %u channel '%s' has no rotation keyframes",
					id, c, strBone.c_str());

				// Every authored quat finite AND ~unit-length.
				const Zenith_Vector<std::pair<Zenith_Maths::Quat, float>>& xKeys =
					xChannel.GetRotationKeyframes();
				for (u_int k = 0; k < xKeys.GetSize(); ++k)
				{
					const Zenith_Maths::Quat& xRot = xKeys.Get(k).first;
					ZENITH_ASSERT_TRUE(QuatFinite(xRot),
						"species %u clip %u channel '%s' key %u rotation not finite",
						id, c, strBone.c_str(), k);
					ZENITH_ASSERT_TRUE(QuatUnit(xRot),
						"species %u clip %u channel '%s' key %u rotation not unit-length",
						id, c, strBone.c_str(), k);
				}

				// No dead channels: the channel's bone name must resolve in the skeleton.
				ZENITH_ASSERT_GE(ZM_GenMeshFindBone(xMesh, strBone.c_str()), 0,
					"species %u clip %u channel '%s' binds to no skeleton bone (drift)",
					id, c, strBone.c_str());
			}
		}
	}
	ZENITH_ASSERT_GT(uTested, 0u, "no anim-buildable species exercised the channel/skeleton gate");
}

// ############################################################################
// (2) The whole ZM_ValidateCreateClip contract holds against a real skeleton.
// ############################################################################

ZENITH_TEST(ZM_Gen, CreatureAnimGen_ValidationPasses)
{
	u_int uTested = 0u;
	for (u_int id = 0; id < (u_int)ZM_SPECIES_COUNT; ++id)
	{
		const ZM_SPECIES_ID eId = (ZM_SPECIES_ID)id;
		if (!HasAnimBuilder(eId) || !HasMeshBuilder(eId)) { continue; }
		++uTested;

		ZM_GenMesh xMesh;
		ZM_BuildCreatureMesh(ZM_ResolveCreatureRecipe(eId), xMesh);

		for (u_int c = 0; c < (u_int)ZM_ANIM_CLIP_COUNT; ++c)
		{
			const ZM_ANIM_CLIP eClip = (ZM_ANIM_CLIP)c;
			Flux_AnimationClip xClip;
			// ...ForSpecies, because the validation contract now includes D7's
			// "the clip names a rig" and only the species knows which .zskel that is.
			// This is also what the disk bake calls, so the gate validates the shape
			// that actually reaches a file rather than an intermediate one.
			ZM_BuildCreatureClipForSpecies(eId, eClip, xClip);

			const ZM_CreatureClipValidation xVal =
				ZM_ValidateCreatureClip(xClip, xMesh, ZM_CreatureClipLooping(eClip));
			ZENITH_ASSERT_TRUE(xVal.m_bAllValid,
				"species %u clip %u failed ZM_ValidateCreatureClip (first bad bone '%s')",
				id, c, xVal.m_szFirstBadBone);
		}
	}
	ZENITH_ASSERT_GT(uTested, 0u, "no anim-buildable species exercised the clip-validation gate");
}

// ############################################################################
// (3) Golden clip metadata -- literal pins (any change is a version-bump).
// ############################################################################

ZENITH_TEST(ZM_Gen, CreatureAnimGen_ClipMetadataGolden)
{
	// The ticks-per-second constant is a golden literal.
	ZENITH_ASSERT_EQ(uZM_CREATURE_ANIM_TICKS_PER_SECOND, 24u,
		"creature-anim ticks-per-second golden-pinned to 24");

	for (u_int c = 0; c < (u_int)ZM_ANIM_CLIP_COUNT; ++c)
	{
		const ZM_ClipGolden& xG = g_axGolden[c];
		const ZM_ANIM_CLIP eClip = xG.m_eClip;

		// Golden accessors (literal-pinned; exact float compare -- these are exact literals).
		ZENITH_ASSERT_EQ(ZM_CreatureClipDurationSeconds(eClip), xG.m_fDurationSeconds,
			"clip %u duration drifted from its golden literal", c);
		ZENITH_ASSERT_EQ(ZM_CreatureClipLooping(eClip), xG.m_bLooping,
			"clip %u looping flag drifted from its golden literal", c);
		ZENITH_ASSERT_STREQ(ZM_CreatureClipName(eClip), xG.m_szName,
			"clip %u name drifted from its golden literal", c);

		// Build one clip per id and confirm the metadata is stamped onto the Flux clip.
		Flux_AnimationClip xClip;
		BuildQuadClip(eClip, xClip);
		ZENITH_ASSERT_EQ(xClip.GetTicksPerSecond(), 24u,
			"clip %u built-clip ticks-per-second != 24", c);
		ZENITH_ASSERT_STREQ(xClip.GetName().c_str(), xG.m_szName,
			"clip %u built-clip name != golden", c);
		ZENITH_ASSERT_EQ(xClip.GetDuration(), xG.m_fDurationSeconds,
			"clip %u built-clip duration != golden", c);
		ZENITH_ASSERT_EQ(xClip.IsLooping(), xG.m_bLooping,
			"clip %u built-clip looping != golden", c);
	}
}

// ############################################################################
// (3b) D7 / D8: a SPECIES' clip names that species' OWN rig, and says the bake
//      owns it.
//
// ★ "NON-EMPTY" WOULD BE THE WRONG ASSERTION HERE, and it is the one
// ZM_ValidateCreatureClip is stuck with (it is handed a clip and a skeleton MESH,
// never a species). The failure this catches is not an empty ref -- it is EVERY
// species carrying the SAME ref, which is precisely what folding the stamp into
// the archetype-pure builder would produce. Every channel would still bind by
// name, every clip would still validate, and 150 creatures would preview against
// one arbitrary skeleton. So this asserts the exact per-species string from
// ZM_CreatureAssetPath and, below, that two species of ONE archetype disagree.
//
// The CONTRAST also protects the purity gate in (4) from the opposite mistake: the
// motion must still be identical across those two species while the identity
// differs.
// ############################################################################

ZENITH_TEST(ZM_Gen, CreatureAnimGen_SpeciesClipsNameTheirOwnRig)
{
	u_int uTested = 0u;
	for (u_int id = 0; id < (u_int)ZM_SPECIES_COUNT; ++id)
	{
		const ZM_SPECIES_ID eId = (ZM_SPECIES_ID)id;
		if (!HasAnimBuilder(eId)) { continue; }
		++uTested;

		// ★ THE REFS ARE BUILT OUTSIDE THE ASSERT. ZENITH_ASSERT_TRUE compiles to
		// ((void)0) when ZENITH_TESTING is off, so a call placed inside it stops
		// happening -- harmless here, but the habit is how a "test" quietly becomes a
		// no-op that still reads as coverage.
		char acSkeletonRef[512] = {};
		char acModelRef[512]    = {};
		const bool bSkeletonRefFits = ZM_CreatureAssetPath(eId, ZM_CREATURE_ASSET_SKELETON, acSkeletonRef, sizeof(acSkeletonRef));
		const bool bModelRefFits    = ZM_CreatureAssetPath(eId, ZM_CREATURE_ASSET_MODEL, acModelRef, sizeof(acModelRef));
		ZENITH_ASSERT_TRUE(bSkeletonRefFits, "species %u skeleton ref overflowed its buffer", id);
		ZENITH_ASSERT_TRUE(bModelRefFits, "species %u model ref overflowed its buffer", id);

		for (u_int c = 0; c < (u_int)ZM_ANIM_CLIP_COUNT; ++c)
		{
			const ZM_ANIM_CLIP eClip = (ZM_ANIM_CLIP)c;
			Flux_AnimationClip xClip;
			ZM_BuildCreatureClipForSpecies(eId, eClip, xClip);
			const Flux_AnimationClipMetadata& xMeta = xClip.GetMetadata();

			ZENITH_ASSERT_STREQ(xMeta.m_strSkeletonPath.c_str(), acSkeletonRef,
				"species %u clip %u does not name that species' own .zskel", id, c);
			ZENITH_ASSERT_STREQ(xMeta.m_strPreviewModelPath.c_str(), acModelRef,
				"species %u clip %u does not name that species' own .zmodel", id, c);
			ZENITH_ASSERT_TRUE(xMeta.m_bGenerated,
				"species %u clip %u is rewritten in full by every bake but does not say so (D8)", id, c);
			ZENITH_ASSERT_EQ(xMeta.m_uAuthoredFrameRate, uZM_CREATURE_ANIM_TICKS_PER_SECOND,
				"species %u clip %u authored frame rate is not the generator's own grid", id, c);
		}
	}
	ZENITH_ASSERT_GT(uTested, 0u, "no anim-buildable species exercised the rig-identity gate");
}

ZENITH_TEST(ZM_Gen, CreatureAnimGen_RigIdentityDiffersWhileMotionDoesNot)
{
	// The pair of properties that only make sense together: two species of ONE body
	// plan get the SAME curves and DIFFERENT rig references. Assert both on the same
	// two clips, so neither can be satisfied by collapsing into the other.
	ZM_ARCHETYPE aeArch[ZM_ARCHETYPE_COUNT];
	const u_int uArch = WiredAnimArchetypes(aeArch, (u_int)ZM_ARCHETYPE_COUNT);
	ZENITH_ASSERT_GT(uArch, 0u, "no wired anim archetypes (harness would be vacuous)");

	u_int uTestedArch = 0u;
	for (u_int ia = 0; ia < uArch; ++ia)
	{
		ZM_SPECIES_ID eA = (ZM_SPECIES_ID)0;   // placeholders; only read when uFound == 2
		ZM_SPECIES_ID eB = (ZM_SPECIES_ID)0;
		if (FindTwoSpeciesOfArchetype(aeArch[ia], eA, eB) < 2u) { continue; }
		++uTestedArch;

		Flux_AnimationClip xSpeciesClipA;
		Flux_AnimationClip xSpeciesClipB;
		ZM_BuildCreatureClipForSpecies(eA, ZM_ANIM_CLIP_IDLE, xSpeciesClipA);
		ZM_BuildCreatureClipForSpecies(eB, ZM_ANIM_CLIP_IDLE, xSpeciesClipB);

		ZENITH_ASSERT_FALSE(xSpeciesClipA.GetMetadata().m_strSkeletonPath ==
		                    xSpeciesClipB.GetMetadata().m_strSkeletonPath,
			"two species of archetype %u share a skeleton ref -- the stamp is not per-species",
			(u_int)aeArch[ia]);

		// ...and the MOTION is still identical. Compare the curves directly rather
		// than the serialized bytes, which now legitimately differ by the two strings.
		const Zenith_HashMap<std::string, Flux_BoneChannel>& xChannelsA = xSpeciesClipA.GetBoneChannels();
		const Zenith_HashMap<std::string, Flux_BoneChannel>& xChannelsB = xSpeciesClipB.GetBoneChannels();
		ZENITH_ASSERT_EQ(xChannelsA.GetSize(), xChannelsB.GetSize(),
			"archetype %u: two species disagree on channel count", (u_int)aeArch[ia]);

		Zenith_HashMap<std::string, Flux_BoneChannel>::Iterator xIt(xChannelsA);
		for (; !xIt.Done(); xIt.Next())
		{
			const Flux_BoneChannel& xChannelA = xIt.GetValue();
			const Flux_BoneChannel* pxChannelB = xSpeciesClipB.GetBoneChannel(xChannelA.GetBoneName());
			ZENITH_ASSERT_TRUE(pxChannelB != nullptr,
				"archetype %u: channel '%s' is missing from the second species",
				(u_int)aeArch[ia], xChannelA.GetBoneName().c_str());
			if (pxChannelB == nullptr) { continue; }

			const Zenith_Vector<std::pair<Zenith_Maths::Quat, float>>& xKeysA = xChannelA.GetRotationKeyframes();
			const Zenith_Vector<std::pair<Zenith_Maths::Quat, float>>& xKeysB = pxChannelB->GetRotationKeyframes();
			ZENITH_ASSERT_EQ(xKeysA.GetSize(), xKeysB.GetSize(),
				"archetype %u channel '%s': key counts differ across species",
				(u_int)aeArch[ia], xChannelA.GetBoneName().c_str());
			for (u_int k = 0; k < xKeysA.GetSize() && k < xKeysB.GetSize(); ++k)
			{
				ZENITH_ASSERT_LE(fabsf(xKeysA.Get(k).second - xKeysB.Get(k).second), fTIME_TOL,
					"archetype %u channel '%s' key %u: time differs across species",
					(u_int)aeArch[ia], xChannelA.GetBoneName().c_str(), k);
				ZENITH_ASSERT_GE(QuatAbsDot(xKeysA.Get(k).first, xKeysB.Get(k).first), fDOT_CLOSE,
					"archetype %u channel '%s' key %u: rotation differs across species -- the "
					"motion is no longer pure f(archetype, clip)",
					(u_int)aeArch[ia], xChannelA.GetBoneName().c_str(), k);
			}
		}
	}
	ZENITH_ASSERT_GT(uTestedArch, 0u,
		"no wired archetype had two distinct species to compare (harness would be vacuous)");
}

// ############################################################################
// (4) THE leverage proof: a clip is pure f(archetype, clip) -- byte-identical
//     across two DISTINCT species of the same archetype (author once, transfer
//     to every species of the body plan).
// ############################################################################

ZENITH_TEST(ZM_Gen, CreatureAnimGen_SameArchetypeByteIdentical)
{
	ZM_ARCHETYPE aeArch[ZM_ARCHETYPE_COUNT];
	const u_int uArch = WiredAnimArchetypes(aeArch, (u_int)ZM_ARCHETYPE_COUNT);
	ZENITH_ASSERT_GT(uArch, 0u, "no wired anim archetypes (harness would be vacuous)");

	// For EVERY wired archetype: two DISTINCT species must produce byte-identical
	// clips (a clip is pure f(archetype, clip)). An archetype with only a single
	// species is skipped, but at least one archetype must have been compared.
	u_int uTestedArch = 0u;
	for (u_int ia = 0; ia < uArch; ++ia)
	{
		const ZM_ARCHETYPE eArch = aeArch[ia];

		ZM_SPECIES_ID eA = (ZM_SPECIES_ID)0;   // placeholders; only read when uFound == 2
		ZM_SPECIES_ID eB = (ZM_SPECIES_ID)0;
		if (FindTwoSpeciesOfArchetype(eArch, eA, eB) < 2u) { continue; }
		++uTestedArch;

		const ZM_ARCHETYPE eArchA = ZM_GetSpeciesData(eA).m_eArchetype;
		const ZM_ARCHETYPE eArchB = ZM_GetSpeciesData(eB).m_eArchetype;

		for (u_int c = 0; c < (u_int)ZM_ANIM_CLIP_COUNT; ++c)
		{
			const ZM_ANIM_CLIP eClip = (ZM_ANIM_CLIP)c;
			Flux_AnimationClip xClipA;
			Flux_AnimationClip xClipB;
			ZM_BuildCreatureClip(eArchA, eClip, xClipA);
			ZM_BuildCreatureClip(eArchB, eClip, xClipB);

			ZENITH_ASSERT_TRUE(ZM_CreatureClipBytesEqual(xClipA, xClipB),
				"clip %u differs byte-wise across two species of archetype %u (clip is not pure f(archetype,clip))", c, (u_int)eArch);
			ZENITH_ASSERT_EQ(ZM_CreatureClipContentHash(xClipA), ZM_CreatureClipContentHash(xClipB),
				"clip %u content hash differs across two species of archetype %u", c, (u_int)eArch);
		}
	}
	ZENITH_ASSERT_GT(uTestedArch, 0u,
		"no wired archetype had two distinct species to compare (harness would be vacuous)");
}

// ############################################################################
// (5) Same-inputs determinism: the SAME (archetype, clip) built twice is
//     byte-identical (no RNG, no clock, no address-dependent data).
// ############################################################################

ZENITH_TEST(ZM_Gen, CreatureAnimGen_SameInputsDeterminism)
{
	ZM_ARCHETYPE aeArch[ZM_ARCHETYPE_COUNT];
	const u_int uArch = WiredAnimArchetypes(aeArch, (u_int)ZM_ARCHETYPE_COUNT);
	ZENITH_ASSERT_GT(uArch, 0u, "no wired anim archetypes (harness would be vacuous)");

	for (u_int ia = 0; ia < uArch; ++ia)
	{
		const ZM_ARCHETYPE eArch = aeArch[ia];
		for (u_int c = 0; c < (u_int)ZM_ANIM_CLIP_COUNT; ++c)
		{
			const ZM_ANIM_CLIP eClip = (ZM_ANIM_CLIP)c;
			Flux_AnimationClip xClip1;
			Flux_AnimationClip xClip2;
			ZM_BuildCreatureClip(eArch, eClip, xClip1);
			ZM_BuildCreatureClip(eArch, eClip, xClip2);

			ZENITH_ASSERT_TRUE(ZM_CreatureClipBytesEqual(xClip1, xClip2),
				"archetype %u clip %u not byte-identical on a repeat build (non-determinism)", (u_int)eArch, c);
			ZENITH_ASSERT_EQ(ZM_CreatureClipContentHash(xClip1), ZM_CreatureClipContentHash(xClip2),
				"archetype %u clip %u content hash diverged on a repeat build", (u_int)eArch, c);
		}
	}
}

// ############################################################################
// (6) Meaningfully-different clips are actually DISTINCT (guards two clips from
//     accidentally sharing the same motion).
// ############################################################################

ZENITH_TEST(ZM_Gen, CreatureAnimGen_ClipsDistinct)
{
	ZM_ARCHETYPE aeArch[ZM_ARCHETYPE_COUNT];
	const u_int uArch = WiredAnimArchetypes(aeArch, (u_int)ZM_ARCHETYPE_COUNT);
	ZENITH_ASSERT_GT(uArch, 0u, "no wired anim archetypes (harness would be vacuous)");

	for (u_int ia = 0; ia < uArch; ++ia)
	{
		const ZM_ARCHETYPE eArch = aeArch[ia];

		Flux_AnimationClip xIdle;
		Flux_AnimationClip xWalk;
		Flux_AnimationClip xAttack;
		Flux_AnimationClip xSpecial;
		Flux_AnimationClip xHit;
		Flux_AnimationClip xFaint;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_IDLE,    xIdle);
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_WALK,    xWalk);
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_ATTACK,  xAttack);
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_SPECIAL, xSpecial);
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_HIT,     xHit);
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_FAINT,   xFaint);

		const u_int uIdle    = ZM_CreatureClipContentHash(xIdle);
		const u_int uWalk    = ZM_CreatureClipContentHash(xWalk);
		const u_int uAttack  = ZM_CreatureClipContentHash(xAttack);
		const u_int uSpecial = ZM_CreatureClipContentHash(xSpecial);
		const u_int uHit     = ZM_CreatureClipContentHash(xHit);
		const u_int uFaint   = ZM_CreatureClipContentHash(xFaint);

		ZENITH_ASSERT_NE(uIdle,   uWalk,    "archetype %u: Idle and Walk clips share a content hash (motion collision)", (u_int)eArch);
		ZENITH_ASSERT_NE(uAttack, uSpecial, "archetype %u: Attack and Special clips share a content hash", (u_int)eArch);
		ZENITH_ASSERT_NE(uAttack, uHit,     "archetype %u: Attack and Hit clips share a content hash", (u_int)eArch);
		ZENITH_ASSERT_NE(uHit,    uFaint,   "archetype %u: Hit and Faint clips share a content hash", (u_int)eArch);
	}
}

// ############################################################################
// (7) Looping clips (Idle / Walk) wrap cleanly -- a key at t=0 and at
//     t=durationTicks per channel with matching orientation (no loop pop). The
//     one-shot action clips do NOT loop.
// ############################################################################

ZENITH_TEST(ZM_Gen, CreatureAnimGen_LoopingClipsWrapCleanly)
{
	ZM_ARCHETYPE aeArch[ZM_ARCHETYPE_COUNT];
	const u_int uArch = WiredAnimArchetypes(aeArch, (u_int)ZM_ARCHETYPE_COUNT);
	ZENITH_ASSERT_GT(uArch, 0u, "no wired anim archetypes (harness would be vacuous)");

	for (u_int ia = 0; ia < uArch; ++ia)
	{
		const ZM_ARCHETYPE eArch = aeArch[ia];

		const ZM_ANIM_CLIP aeLooping[2] = { ZM_ANIM_CLIP_IDLE, ZM_ANIM_CLIP_WALK };
		for (u_int i = 0; i < 2u; ++i)
		{
			const ZM_ANIM_CLIP eClip = aeLooping[i];
			Flux_AnimationClip xClip;
			ZM_BuildCreatureClip(eArch, eClip, xClip);

			ZENITH_ASSERT_TRUE(xClip.IsLooping(), "archetype %u clip %u must be looping", (u_int)eArch, (u_int)eClip);
			// SECONDS (D3): the clip's own duration IS the last key's time now, with no
			// ticks-per-second multiply between them.
			const float fDurSeconds = xClip.GetDuration();

			const Zenith_HashMap<std::string, Flux_BoneChannel>& xChannels = xClip.GetBoneChannels();
			ZENITH_ASSERT_GT(xChannels.GetSize(), 0u, "archetype %u looping clip %u has no channels", (u_int)eArch, (u_int)eClip);

			Zenith_HashMap<std::string, Flux_BoneChannel>::Iterator xIt(xChannels);
			for (; !xIt.Done(); xIt.Next())
			{
				const Flux_BoneChannel& xChannel = xIt.GetValue();
				const char* szBone = xChannel.GetBoneName().c_str();
				const Zenith_Vector<std::pair<Zenith_Maths::Quat, float>>& xKeys = xChannel.GetRotationKeyframes();
				ZENITH_ASSERT_GE(xKeys.GetSize(), 2u,
					"archetype %u looping clip %u channel '%s' needs >= 2 keys to close", (u_int)eArch, (u_int)eClip, szBone);

				// SortKeyframes puts the earliest time first and the latest last.
				const float fFirstSeconds = xKeys.GetFront().second;
				const float fLastSeconds  = xKeys.GetBack().second;
				ZENITH_ASSERT_LE(fabsf(fFirstSeconds - 0.0f), fTIME_TOL,
					"archetype %u looping clip %u channel '%s' has no key at t=0", (u_int)eArch, (u_int)eClip, szBone);
				ZENITH_ASSERT_LE(fabsf(fLastSeconds - fDurSeconds), fTIME_TOL,
					"archetype %u looping clip %u channel '%s' has no key at t=duration seconds", (u_int)eArch, (u_int)eClip, szBone);

				// Loop closes: rot(t=0) ~= rot(t=duration) per channel (|dot| ~ 1).
				const Zenith_Maths::Quat& xR0 = xKeys.GetFront().first;
				const Zenith_Maths::Quat& xRN = xKeys.GetBack().first;
				ZENITH_ASSERT_GE(QuatAbsDot(xR0, xRN), fDOT_CLOSE,
					"archetype %u looping clip %u channel '%s' does not close (t=0 rot != t=end rot)", (u_int)eArch, (u_int)eClip, szBone);
			}
		}

		// The one-shot action clips must NOT loop (their clip-end clamp holds neutral/KO).
		const ZM_ANIM_CLIP aeOneShot[4] =
			{ ZM_ANIM_CLIP_ATTACK, ZM_ANIM_CLIP_SPECIAL, ZM_ANIM_CLIP_HIT, ZM_ANIM_CLIP_FAINT };
		for (u_int i = 0; i < 4u; ++i)
		{
			Flux_AnimationClip xClip;
			ZM_BuildCreatureClip(eArch, aeOneShot[i], xClip);
			ZENITH_ASSERT_FALSE(xClip.IsLooping(), "archetype %u one-shot clip %u must not loop", (u_int)eArch, (u_int)aeOneShot[i]);
		}
	}
}

// ############################################################################
// (8) Faint settles and CLAMPS: the KO pose holds past the end (clamp, not
//     extrapolate) AND the final pose genuinely differs from the t=0 pose.
// ############################################################################

ZENITH_TEST(ZM_Gen, CreatureAnimGen_FaintSettlesAndClamps)
{
	ZM_ARCHETYPE aeArch[ZM_ARCHETYPE_COUNT];
	const u_int uArch = WiredAnimArchetypes(aeArch, (u_int)ZM_ARCHETYPE_COUNT);
	ZENITH_ASSERT_GT(uArch, 0u, "no wired anim archetypes (harness would be vacuous)");

	for (u_int ia = 0; ia < uArch; ++ia)
	{
		const ZM_ARCHETYPE eArch = aeArch[ia];

		Flux_AnimationClip xClip;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_FAINT, xClip);
		ZENITH_ASSERT_FALSE(xClip.IsLooping(), "archetype %u Faint must be a one-shot (non-looping)", (u_int)eArch);

		const float fDurSeconds = xClip.GetDuration();   // SECONDS (D3)
		const Zenith_HashMap<std::string, Flux_BoneChannel>& xChannels = xClip.GetBoneChannels();
		ZENITH_ASSERT_GT(xChannels.GetSize(), 0u, "archetype %u Faint has no channels", (u_int)eArch);

		// Archetype-agnostic (no hardcoded bone name): EVERY channel must clamp past
		// the end (KO pose holds, not extrapolate), and AT LEAST ONE channel's final
		// pose must genuinely differ from t=0 (a real collapse, not a no-op).
		u_int uExamined  = 0u;
		u_int uCollapsed = 0u;
		Zenith_HashMap<std::string, Flux_BoneChannel>::Iterator xIt(xChannels);
		for (; !xIt.Done(); xIt.Next())
		{
			const Flux_BoneChannel& xChannel = xIt.GetValue();
			const char* szBone = xChannel.GetBoneName().c_str();

			const Zenith_Maths::Quat xAt0   = xChannel.SampleRotation(0.0f);
			const Zenith_Maths::Quat xAtMid = xChannel.SampleRotation(fDurSeconds * 0.5f);
			const Zenith_Maths::Quat xAtEnd = xChannel.SampleRotation(fDurSeconds);
			const Zenith_Maths::Quat xPast  = xChannel.SampleRotation(fDurSeconds * 2.0f);

			ZENITH_ASSERT_TRUE(QuatFinite(xAt0) && QuatFinite(xAtMid) && QuatFinite(xAtEnd) && QuatFinite(xPast),
				"archetype %u Faint channel '%s' samples must all be finite", (u_int)eArch, szBone);

			// Clamp-not-extrapolate: sampling past the end holds the settled KO pose.
			ZENITH_ASSERT_GE(QuatAbsDot(xPast, xAtEnd), fDOT_CLOSE,
				"archetype %u Faint channel '%s' does not clamp past the end (KO pose should hold, not extrapolate)", (u_int)eArch, szBone);

			// Count channels whose final pose genuinely differs from bind (t=0).
			if (QuatAbsDot(xAtEnd, xAt0) <= fDOT_DIFFER) { ++uCollapsed; }
			++uExamined;
		}
		ZENITH_ASSERT_GT(uExamined, 0u, "archetype %u Faint examined no channels", (u_int)eArch);
		ZENITH_ASSERT_GT(uCollapsed, 0u,
			"archetype %u Faint final pose ~= t=0 pose on every channel (the creature did not visibly collapse)", (u_int)eArch);
	}
}

// ############################################################################
// (9) One-shot ACTION clips (Attack / Special / Hit) resolve to ~neutral at their
//     END so the clip-end clamp holds a clean bind pose. Faint deliberately does
//     NOT (it holds a collapsed KO pose), which the CONTRAST below proves -- so the
//     end-neutral property is specific to the action clips, not trivially true of
//     every one-shot.
// ############################################################################

ZENITH_TEST(ZM_Gen, CreatureAnimGen_OneShotClipsEndNeutral)
{
	const Zenith_Maths::Quat xIdentity = glm::identity<Zenith_Maths::Quat>();

	ZM_ARCHETYPE aeArch[ZM_ARCHETYPE_COUNT];
	const u_int uArch = WiredAnimArchetypes(aeArch, (u_int)ZM_ARCHETYPE_COUNT);
	ZENITH_ASSERT_GT(uArch, 0u, "no wired anim archetypes (harness would be vacuous)");

	for (u_int ia = 0; ia < uArch; ++ia)
	{
		const ZM_ARCHETYPE eArch = aeArch[ia];

		// Attack / Special / Hit: EVERY channel returns to ~identity at the clip end.
		const ZM_ANIM_CLIP aeAction[3] =
			{ ZM_ANIM_CLIP_ATTACK, ZM_ANIM_CLIP_SPECIAL, ZM_ANIM_CLIP_HIT };
		for (u_int i = 0; i < 3u; ++i)
		{
			const ZM_ANIM_CLIP eClip = aeAction[i];
			Flux_AnimationClip xClip;
			ZM_BuildCreatureClip(eArch, eClip, xClip);

			const float fDurSeconds = xClip.GetDuration();   // SECONDS (D3)
			const Zenith_HashMap<std::string, Flux_BoneChannel>& xChannels = xClip.GetBoneChannels();
			ZENITH_ASSERT_GT(xChannels.GetSize(), 0u, "archetype %u action clip %u has no channels", (u_int)eArch, (u_int)eClip);

			u_int uExamined = 0u;
			Zenith_HashMap<std::string, Flux_BoneChannel>::Iterator xIt(xChannels);
			for (; !xIt.Done(); xIt.Next())
			{
				const Flux_BoneChannel& xChannel = xIt.GetValue();
				const char* szBone = xChannel.GetBoneName().c_str();
				const Zenith_Maths::Quat xEnd = xChannel.SampleRotation(fDurSeconds);
				ZENITH_ASSERT_TRUE(QuatFinite(xEnd),
					"archetype %u action clip %u channel '%s' end sample not finite", (u_int)eArch, (u_int)eClip, szBone);
				ZENITH_ASSERT_GE(QuatAbsDot(xEnd, xIdentity), fDOT_CLOSE,
					"archetype %u action clip %u channel '%s' does NOT end at ~identity (clip-end clamp would not hold neutral)",
					(u_int)eArch, (u_int)eClip, szBone);
				++uExamined;
			}
			ZENITH_ASSERT_GT(uExamined, 0u,
				"archetype %u action clip %u examined no channels (end-neutral gate would be vacuous)", (u_int)eArch, (u_int)eClip);
		}

		// CONTRAST: Faint holds a collapsed pose -- at least one channel is NOT
		// ~identity at the end. This proves the end-neutral property above is a real
		// property of the action clips, not a trivial truth of every one-shot clip.
		{
			Flux_AnimationClip xFaint;
			ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_FAINT, xFaint);

			const float fDurSeconds = xFaint.GetDuration();   // SECONDS (D3)
			const Zenith_HashMap<std::string, Flux_BoneChannel>& xChannels = xFaint.GetBoneChannels();
			ZENITH_ASSERT_GT(xChannels.GetSize(), 0u, "archetype %u Faint has no channels", (u_int)eArch);

			u_int uExamined  = 0u;
			u_int uCollapsed = 0u;
			Zenith_HashMap<std::string, Flux_BoneChannel>::Iterator xIt(xChannels);
			for (; !xIt.Done(); xIt.Next())
			{
				const Flux_BoneChannel& xChannel = xIt.GetValue();
				const Zenith_Maths::Quat xEnd = xChannel.SampleRotation(fDurSeconds);
				if (QuatAbsDot(xEnd, xIdentity) <= fDOT_DIFFER) { ++uCollapsed; }
				++uExamined;
			}
			ZENITH_ASSERT_GT(uExamined, 0u, "archetype %u Faint examined no channels (contrast would be vacuous)", (u_int)eArch);
			ZENITH_ASSERT_GT(uCollapsed, 0u,
				"archetype %u Faint ends at ~identity on every channel (end-neutral would be trivially true, not specific to action clips)", (u_int)eArch);
		}
	}
}

// ############################################################################
// (10) TOTALITY GATE: the anim-builder dispatch is TOTAL -- EVERY archetype in
//      [0, ZM_ARCHETYPE_COUNT) has a wired (non-null) anim builder. As of SC5 the
//      last archetype (FLOATER_PLANTOID) is wired, so ZM_GetArchetypeAnimBuilder
//      never returns nullptr for a real archetype. This is the twin of
//      ZM_CreatureGen's ArchetypeDispatch/AllSpeciesBuildable totality gate: it
//      forbids a future archetype being added to the enum without also wiring its
//      anim builder (which would silently skip that body plan in every
//      HasAnimBuilder-gated generic test above). Fn-ptr nullness is checked with
//      `!= nullptr` (never ZENITH_ASSERT_NULL -- MSVC rejects the fnptr->void* cast).
// ############################################################################

ZENITH_TEST(ZM_Gen, CreatureAnimGen_AllArchetypesHaveAnimBuilder)
{
	for (u_int a = 0; a < (u_int)ZM_ARCHETYPE_COUNT; ++a)
	{
		const ZM_ARCHETYPE eArch = (ZM_ARCHETYPE)a;
		const ZM_ArchetypeAnimFn pxFn = ZM_GetArchetypeAnimBuilder(eArch);
		ZENITH_ASSERT_TRUE(pxFn != nullptr,
			"archetype %u has no wired anim builder (dispatch is not total)", a);
	}
}

// ############################################################################
// (11) D3 UNIT GATE: keyframe times are SECONDS on the clip's own clock.
//
// ★ THIS IS THE ONE CHECK A TICK-GRID RELAPSE CANNOT PASS. Every other gate in
// this file is unit-blind: a builder that went back to authoring
// t01 * duration * 24 would still produce finite unit quaternions, still bind
// every channel to a real bone, still be byte-identical across species, still be
// deterministic, still close its loop (first key == last key by VALUE) and still
// clamp past the end. The clip would simply be 24x too long, and nothing here
// would say so -- which is exactly the state the tree was in before D3, with a
// ticks-per-second multiply in Flux_SkeletonPose::SampleFromClip papering over it.
//
// Two assertions, because either alone is weak:
//   * NO key past the duration -- catches the 24x relapse on any clip;
//   * a LOOPING clip's last key lands ON the duration -- catches the opposite
//     slip (a conversion applied twice, leaving every key at 1/24 of its time),
//     which "no key past the end" would happily accept.
// ############################################################################

ZENITH_TEST(ZM_Gen, CreatureAnimGen_KeyTimesAreSeconds)
{
	ZM_ARCHETYPE aeArch[ZM_ARCHETYPE_COUNT];
	const u_int uArch = WiredAnimArchetypes(aeArch, (u_int)ZM_ARCHETYPE_COUNT);
	ZENITH_ASSERT_GT(uArch, 0u, "no wired anim archetypes (harness would be vacuous)");

	u_int uChecked = 0u;
	for (u_int ia = 0; ia < uArch; ++ia)
	{
		const ZM_ARCHETYPE eArch = aeArch[ia];
		for (u_int c = 0; c < (u_int)ZM_ANIM_CLIP_COUNT; ++c)
		{
			const ZM_ANIM_CLIP eClip = (ZM_ANIM_CLIP)c;
			Flux_AnimationClip xClip;
			ZM_BuildCreatureClip(eArch, eClip, xClip);
			++uChecked;

			const float fDurSeconds = xClip.GetDuration();
			ZENITH_ASSERT_GT(fDurSeconds, 0.0f,
				"archetype %u clip %u has a non-positive duration", (u_int)eArch, c);

			// No key past the end. Reported with the offending time so a 24x relapse
			// names itself rather than showing up as a bare false.
			const float fLastSeconds = Flux_ClipLastKeyTimeSeconds(xClip);
			ZENITH_ASSERT_TRUE(Flux_ClipKeyTimesFitDuration(xClip, fTIME_TOL),
				"archetype %u clip %u: last key at %.4f s but duration is %.4f s -- key times are not seconds (a tick grid would put it at %.4f)",
				(u_int)eArch, c, fLastSeconds, fDurSeconds,
				fDurSeconds * static_cast<float>(uZM_CREATURE_ANIM_TICKS_PER_SECOND));

			// A looping clip authors key0 AND keyN inclusively, so its last key IS the
			// duration. That is the half a range check cannot see.
			if (ZM_CreatureClipLooping(eClip))
			{
				ZENITH_ASSERT_LE(fabsf(fLastSeconds - fDurSeconds), fTIME_TOL,
					"archetype %u looping clip %u: last key at %.4f s, duration %.4f s -- the key spread does not reach the clip end",
					(u_int)eArch, c, fLastSeconds, fDurSeconds);
			}
		}
	}
	ZENITH_ASSERT_GT(uChecked, 0u, "no clips exercised the key-time unit gate");
}
