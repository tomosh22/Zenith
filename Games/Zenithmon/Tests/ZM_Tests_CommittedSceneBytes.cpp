#include "Zenith.h"

// ============================================================================
// ZM_Tests_CommittedSceneBytes (ZM-D-183) -- the boot unit that checks the
// COMMITTED Dawnmere.zscen BYTES, not the compiled constants they came from.
//
// ★ WHY THIS FILE EXISTS, AND WHY NOTHING ALREADY HERE COULD DO ITS JOB.
// Zenithmon commits its five .zscen files (ZM-D-148) and asserts that a tools
// boot leaves them unmodified in git status. That invariant has now broken TWICE,
// and each time every existing guard stayed green:
//
//   * a6c66b68 (ZM-D-179) -- Zenith_TransformComponent serialized the LIVE JOLT
//     BODY's pose, so the committed rival rotation was a few ULP off the authored
//     one. Fixed engine-side in c225450f.
//   * ZM-D-183 -- the AUTHORED value itself was build-configuration dependent
//     (std::atan2 + glm::angleAxis differ by 1-2 ULP between MSVC Debug and
//     Release codegen), so a Release tools boot and a Debug tools boot wrote
//     DIFFERENT bytes and the file ping-ponged in git forever.
//
// The two guards that existed could not see either one:
//   * ZM_VerifyAuthoredRivalFacingStep IS bit-exact, but it compares the
//     serialized bytes against ZM_DawnmereVesperFacing() evaluated IN THE SAME
//     BINARY -- both sides moved together in the ZM-D-183 case -- and it only runs
//     on the WINDOWED AUTHOR_DAWNMERE boot, which CI never performs (zm-tests.yml
//     builds Vulkan_..._True but RUNS Null_..._True).
//   * ZM_RivalVesperAuthored_Test runs headless but compares |dot| against 0.999,
//     and both drifts land at 1 - |dot| ~ 1e-14 -- six orders below the threshold.
//
// So the gap was structural: NOTHING checked the bytes that are actually in git
// against a value that does not move with the build configuration. That is the
// one and only job of this file.
//
// ★ IT READS DISK, WHICH IS WHY IT IS NOT IN ZM_Tests_DawnmerePlacement.cpp.
// That file opens by declaring itself pure -- no scene, no assets -- and the
// frozen-constant ORACLE lives there (Vesper_FrozenFacingStillEncodesTheDerivedBearing).
// The division of labour is deliberate and neither half substitutes for the other:
//   * there: is the CONSTANT still the bearing the anchors imply? (tolerance --
//     bit-exact would be permanently red in Release, which is the whole finding);
//   * here:  do the COMMITTED BYTES still equal that constant? (bit-exact -- a
//     tolerance here would reproduce precisely the blind spot described above).
//
// ★ FORMAT-AGNOSTIC BY CONSTRUCTION. This does NOT parse the .zscen container: a
// byte offset would rot at the next schema bump and a parser would duplicate the
// deserializer. It searches the file for the 16-byte little-endian quaternion in
// SERIALIZED component order (x, y, z, w) -- the exact bytes
// Zenith_TransformComponent::WriteToDataStream emits. A hit proves the authored
// rotation survived into the file; the drifted variants simply are not present.
// ============================================================================

#include <bit>
#include <cstring>   // memcpy -- building the needle from the frozen quaternion

#include "Core/Zenith_TestFramework.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Maths/Zenith_Maths.h"
#include "Zenithmon/Source/World/ZM_DawnmerePlacement.h"

namespace
{
	// The committed scene this file is about. GAME_ASSETS_DIR is the compile-time
	// absolute assets root; a packaged run (--assets-root) may not have it, which
	// the test treats as "not applicable" rather than as a failure -- see the
	// absent-file clause.
	const char* const szZM_COMMITTED_DAWNMERE_SCENE =
		GAME_ASSETS_DIR "Scenes/Dawnmere" ZENITH_SCENE_EXT;

	// The 16 bytes a Transform blob carries for this rotation, in the order
	// Zenith_DataStream writes them. Built from the frozen constants rather than
	// typed, so the needle can never disagree with what the authoring writes.
	void BuildFrozenRotationNeedle(unsigned char (&auNeedleOut)[16])
	{
		const Zenith_Maths::Quat xFacing = ZM_DawnmereVesperFacing();
		const float afComponents[4] =
			{ xFacing.x, xFacing.y, xFacing.z, xFacing.w };
		std::memcpy(auNeedleOut, afComponents, sizeof(afComponents));
	}

	u_int CountOccurrences(const char* pData, uint64_t ulSize,
		const unsigned char* puNeedle, uint64_t ulNeedleSize)
	{
		if (ulSize < ulNeedleSize)
		{
			return 0u;
		}
		u_int uCount = 0u;
		const unsigned char* puData = reinterpret_cast<const unsigned char*>(pData);
		for (uint64_t ul = 0; ul + ulNeedleSize <= ulSize; ++ul)
		{
			if (std::memcmp(puData + ul, puNeedle, (size_t)ulNeedleSize) == 0)
			{
				++uCount;
			}
		}
		return uCount;
	}
}

// The regression test for BOTH historical breaks. It fails on the a6c66b68 bytes
// and it fails on the bytes a pre-ZM-D-183 Release tools boot wrote, which is
// exactly the property neither existing guard had.
ZENITH_TEST(ZM_CommittedSceneBytes, DawnmereCarriesTheFrozenRivalFacingBitExactly)
{
	uint64_t ulSize = 0;
	char* pData = Zenith_FileAccess::ReadFile(
		szZM_COMMITTED_DAWNMERE_SCENE, ulSize);

	// NOT APPLICABLE rather than FAILED when the committed asset is not reachable
	// (a packaged/relocated-assets run). Logged at warning so a CI run that somehow
	// lost the scene is visible rather than silently vacuous -- on a normal clone
	// the file is tracked and this branch never runs.
	if (pData == nullptr)
	{
		Zenith_Warning(LOG_CATEGORY_GAMEPLAY,
			"[ZM_CommittedSceneBytes] '%s' unreadable -- the committed-bytes clause "
			"is NOT APPLICABLE this run (packaged assets?), not passing.",
			szZM_COMMITTED_DAWNMERE_SCENE);
		return;
	}

	unsigned char auNeedle[16] = {};
	BuildFrozenRotationNeedle(auNeedle);
	const u_int uHits = CountOccurrences(pData, ulSize, auNeedle, sizeof(auNeedle));

	// Log the frozen bits unconditionally: when this reds, the FIRST thing the
	// reader needs is what was expected, in the same %08X form the authoring step
	// and the [ZM Authoring] boot line print.
	Zenith_Log(LOG_CATEGORY_GAMEPLAY,
		"[ZM_CommittedSceneBytes] '%s' %llu bytes; frozen rival facing "
		"(x %08X y %08X z %08X w %08X) occurs %u time(s)",
		szZM_COMMITTED_DAWNMERE_SCENE, (unsigned long long)ulSize,
		uZM_DAWNMERE_VESPER_FACING_X_BITS, uZM_DAWNMERE_VESPER_FACING_Y_BITS,
		uZM_DAWNMERE_VESPER_FACING_Z_BITS, uZM_DAWNMERE_VESPER_FACING_W_BITS,
		uHits);

	Zenith_FileAccess::FreeFileData(pData);

	ZENITH_ASSERT_GT(uHits, 0u,
		"the committed Dawnmere.zscen does NOT contain Npc_RivalVesper's frozen "
		"rotation bit-exactly. Either the scene was re-authored by a build whose "
		"rotation math differs (the ZM-D-183 defect -- check that the rival is "
		"authored via AddStep_SetTransformRotationQuat and NOT AddStep_SetTransformYaw), "
		"or the frozen constants in Source/World/ZM_DawnmerePlacement.h were changed "
		"without re-authoring the scene in the same commit. Do NOT 'fix' this by "
		"committing whatever bytes are on disk -- read the ZM-D-183 block first.");
}
