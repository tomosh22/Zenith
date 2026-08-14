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
#include "Zenithmon/Source/UI/ZM_UI_StarterChoice.h"   // the S8 element-name constants (the needles)
#include "Zenithmon/Source/World/ZM_DawnmerePlacement.h"
#include "Zenithmon/Source/World/ZM_ProfLabPlacement.h"   // szZM_PROFLAB_ASTER_ENTITY_NAME + the SC-E lab-seam names/tag (the needles)

namespace
{
	// The committed scene this file is about. GAME_ASSETS_DIR is the compile-time
	// absolute assets root; a packaged run (--assets-root) may not have it, which
	// the test treats as "not applicable" rather than as a failure -- see the
	// absent-file clause.
	const char* const szZM_COMMITTED_DAWNMERE_SCENE =
		GAME_ASSETS_DIR "Scenes/Dawnmere" ZENITH_SCENE_EXT;

	// ...and the committed FrontEnd, which carries every persistent-root UI element
	// (the S8 starter screen among them).
	const char* const szZM_COMMITTED_FRONTEND_SCENE =
		GAME_ASSETS_DIR "Scenes/FrontEnd" ZENITH_SCENE_EXT;

	// ...and ProfLab, the interior that now has somebody in it.
	const char* const szZM_COMMITTED_PROFLAB_SCENE =
		GAME_ASSETS_DIR "Scenes/ProfLab" ZENITH_SCENE_EXT;

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

	// The ASCII occurrences of an element NAME in a scene blob. Element names are
	// written as contiguous bytes by the UI serializer, so the same format-agnostic
	// search the rotation clause uses works verbatim -- no container parsing, no byte
	// offsets, nothing that rots at the next schema bump.
	u_int CountNameOccurrences(const char* pData, uint64_t ulSize, const char* szName)
	{
		if (szName == nullptr || szName[0] == '\0')
		{
			return 0u;
		}
		return CountOccurrences(pData, ulSize,
			reinterpret_cast<const unsigned char*>(szName),
			(uint64_t)std::strlen(szName));
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

// ============================================================================
// S8 SC-E -- THE LAB SEAM'S BOOT-LEVEL TRIPWIRE. The cheapest possible check on
// the one mutation that makes this game unplayable rather than merely wrong.
//
// ★★ WHAT IT IS FOR. ProfLab now ships an exit configured against Dawnmere's
// "FromLab" tag. ZM_GameStateManager::IsWarpDestinationValid consults ONLY the
// compiled ZM_WorldSpec tag list and NEVER the destination scene, and "FromLab"
// has been a compiled Dawnmere tag since S1 -- so that exit validates whether or
// not any entity in Dawnmere.zscen actually carries the tag. If the marker is
// missing, the warp is ACCEPTED, the fade goes fully opaque, and the machine parks
// in ZM_WARP_TRANSITION_WAITING_FOR_SPAWN, WHICH HAS NO TIMEOUT. The player is
// frozen behind a black screen, forever. Not a crash, not an assert, not a red
// test -- which is exactly why the check has to be somewhere, and why "somewhere"
// has to run in CI.
//
// ★ AND WHY IT IS HERE RATHER THAN IN A PURE UNIT. Every pure unit about this seam
// reads the compiled constants the AUTHORING also reads, so both sides move
// together and none of them can tell whether the scene was ever re-authored. The
// automated round trip (ZM_LabRoundTrip_Test) DOES walk the real door, but it
// needs the GITIGNORED Dawnmere terrain bake and therefore RequestSkips on CI --
// and a skip counts as a PASS. This unit is a BOOT unit reading a TRACKED file, so
// it runs on every Null CI boot with no terrain and no GPU.
//
// ★ THE TWO NEEDLES, AND THE PREFIX TRAP BETWEEN THEM. "FromLab" is a strict
// PREFIX of "FromLabSpawn", so every occurrence of the entity NAME is also an
// occurrence of the TAG and a bare `tagHits > 0` would be satisfied by the name
// alone -- i.e. it would pass on a scene that authored the marker entity and never
// tagged it, which is precisely the WAITING_FOR_SPAWN hang (the resolver matches on
// the TAG, not on the name). The claim is therefore STRICTLY MORE tag occurrences
// than name occurrences: at least one occurrence of "FromLab" that is not part of
// "FromLabSpawn", which is the serialized ZM_SpawnPoint tag.
//
// Both needles are DERIVED -- the name from the shared placement header, the tag
// from the compiled world table via ZM_GetProfLabExitSpawnTag() -- so a rename on
// either side that missed the authoring reds here instead of moving in lockstep.
//
// ★ IT IS STRICT ABOUT AN UNREADABLE FILE, like the FrontEnd and ProfLab clauses
// and UNLIKE the Dawnmere rotation clause at the top of this file. That one
// tolerates a packaged/relocated-assets run; this one does not, because the whole
// ZM-D-147/148 position on this asset family is that a TRACKED scene's absence is a
// DEFECT -- and a quiet return here would silence the check exactly when it broke.
// (The existing clause's behaviour is deliberately left alone.)
// ============================================================================
ZENITH_TEST(ZM_CommittedSceneBytes, DawnmereCarriesTheLabSeamMarkerAndTag)
{
	uint64_t ulSize = 0;
	char* pData = Zenith_FileAccess::ReadFile(
		szZM_COMMITTED_DAWNMERE_SCENE, ulSize);

	ZENITH_ASSERT_NOT_NULL(pData,
		"the committed Dawnmere.zscen could not be read ('%s'). It is a TRACKED asset "
		"(ZM-D-148), so this is a DEFECT and not a skip -- restore the file or "
		"re-author it with a WINDOWED *_True tools boot in sceneAuthoring="
		"AUTHOR_DAWNMERE mode.",
		szZM_COMMITTED_DAWNMERE_SCENE);
	if (pData == nullptr)
	{
		return;   // already FAILED above; nothing further is meaningful
	}

	const char* const szSpawnEntityName =
		szZM_DAWNMERE_FROM_LAB_SPAWN_ENTITY_NAME;
	const char* const szSpawnTag = ZM_GetProfLabExitSpawnTag();

	const u_int uNameHits =
		CountNameOccurrences(pData, ulSize, szSpawnEntityName);
	const u_int uTagHits = CountNameOccurrences(pData, ulSize, szSpawnTag);
	const u_int uDoorTriggerHits = CountNameOccurrences(
		pData, ulSize, szZM_DAWNMERE_LAB_DOOR_TRIGGER_ENTITY_NAME);
	const u_int uShellHits = CountNameOccurrences(
		pData, ulSize, szZM_DAWNMERE_LAB_SHELL_ENTITY_NAME);

	Zenith_Log(LOG_CATEGORY_GAMEPLAY,
		"[ZM_CommittedSceneBytes] '%s' %llu bytes; lab seam: marker '%s' x%u, tag "
		"'%s' x%u (name-inclusive), sensor '%s' x%u, shell '%s' x%u",
		szZM_COMMITTED_DAWNMERE_SCENE, (unsigned long long)ulSize,
		szSpawnEntityName, uNameHits, szSpawnTag, uTagHits,
		szZM_DAWNMERE_LAB_DOOR_TRIGGER_ENTITY_NAME, uDoorTriggerHits,
		szZM_DAWNMERE_LAB_SHELL_ENTITY_NAME, uShellHits);

	Zenith_FileAccess::FreeFileData(pData);

	// The tag has to be a real string before any count over it means anything: an
	// empty needle would make CountNameOccurrences return 0 and turn the strict
	// inequality below into a confusing arithmetic failure rather than a clear one.
	ZENITH_ASSERT_TRUE(szSpawnTag != nullptr && szSpawnTag[0] != '\0',
		"the compiled ZM_SCENE_PROFLAB row carries no connection targeting "
		"ZM_SCENE_DAWNMERE, so ZM_GetProfLabExitSpawnTag() resolved to the empty "
		"string. The lab has no way out and there is no tag to look for in the "
		"committed scene -- fix Source/Data/ZM_WorldSpec.cpp first.");

	ZENITH_ASSERT_EQ(uNameHits, 1u,
		"the committed Dawnmere.zscen must carry the lab arrival marker '%s' exactly "
		"once. A ZERO is the state a source-only change leaves behind: the authoring "
		"steps are in Zenithmon.cpp but no WINDOWED *_True tools boot has re-authored "
		"and re-committed the scene -- and ProfLab's exit is ALREADY shipped, so the "
		"shipped game warps into WAITING_FOR_SPAWN (no timeout) and hangs behind an "
		"opaque fade the moment anyone leaves the lab. More than one means the marker "
		"was authored twice.",
		szSpawnEntityName);

	ZENITH_ASSERT_EQ(uDoorTriggerHits, 1u,
		"the committed Dawnmere.zscen must carry the lab doorway sensor '%s' exactly "
		"once -- without it the lab is authored but has no entrance, so the exit "
		"leads out of a building the player can never get into.",
		szZM_DAWNMERE_LAB_DOOR_TRIGGER_ENTITY_NAME);

	ZENITH_ASSERT_EQ(uShellHits, 1u,
		"the committed Dawnmere.zscen must carry the lab shell '%s' exactly once. "
		"This name is ALSO the key SC-D's ground-truth oracle "
		"(ZM_DawnmereLabGroundTruth_Test) looks up to ignore the shell during its "
		"post-SC-E re-measure, so a miss here means that oracle would silently "
		"measure the building's roof instead of the terrain.",
		szZM_DAWNMERE_LAB_SHELL_ENTITY_NAME);

	// ★ THE CLAUSE THE PREFIX TRAP IS ABOUT. Every "FromLabSpawn" contains a
	// "FromLab", so equality here means the tag exists ONLY as part of the entity
	// name -- an untagged marker, which resolves to nothing and hangs the warp.
	ZENITH_ASSERT_GT(uTagHits, uNameHits,
		"the committed Dawnmere.zscen contains the spawn tag '%s' %u time(s) and the "
		"entity name '%s' %u time(s) -- and the tag is a PREFIX of the name, so those "
		"counts being equal means every occurrence of the tag is part of the name and "
		"the marker was never actually TAGGED. ZM_SpawnPoint resolution matches on the "
		"TAG, not on the entity name, so an untagged marker leaves "
		"RequestWarp(<Dawnmere>, \"%s\") parked in WAITING_FOR_SPAWN -- which has NO "
		"timeout -- behind a fully opaque fade with the player frozen. Check that the "
		"FromLabSpawn authoring in Zenithmon.cpp still calls "
		"AddStep_Custom(&ZM_ConfigureFromLabSpawnPoint) and that the scene was "
		"re-authored afterwards.",
		szSpawnTag, uTagHits, szSpawnEntityName, uNameHits, szSpawnTag);
}

// ============================================================================
// The S8 starter screen's AUTHORED-BYTES gate.
//
// ★ WHY IT EXISTS. Tests/ZM_Tests_StarterScreen.cpp pins the starter picker's whole
// decision surface -- the name maps, the choice maps, the labels, the grant
// composition -- and NOT ONE of those units can see the AUTHORED screen, because all
// of them are pure. Author only two of the three cells, mis-type one cell's name in
// its AddStep_CreateUIButton call so FindElement returns nullptr every frame, or
// author the cells and forget the panel, and every one of them stays green while
// FrontEnd.zscen ships a screen the player cannot use. This unit is the only thing
// in the gate that looks at what was actually written.
//
// ★ EVERY NEEDLE IS DERIVED FROM ZM_UI_StarterChoice's OWN CONSTANTS, never re-spelled
// as a literal. A hand-typed needle would move in lockstep with a renamed element and
// pin nothing; derived, a rename that misses the authoring site is red here.
//
// ★ AND IT IS STRICT ABOUT AN UNREADABLE FILE, UNLIKE THE DAWNMERE CLAUSE ABOVE. That
// one returns early on a missing file because a packaged run may legitimately not have
// it. FrontEnd.zscen is TRACKED (ZM-D-148) and is the scene this game BOOTS into, so
// its absence is a DEFECT, not an inapplicable configuration -- this unit FAILS rather
// than silently passing. (The existing clause's behaviour is deliberately left alone.)
// ============================================================================
ZENITH_TEST(ZM_CommittedSceneBytes, FrontEndCarriesEveryStarterScreenElementName)
{
	uint64_t ulSize = 0;
	char* pData = Zenith_FileAccess::ReadFile(szZM_COMMITTED_FRONTEND_SCENE, ulSize);

	ZENITH_ASSERT_NOT_NULL(pData,
		"the committed FrontEnd.zscen could not be read ('%s'). It is a TRACKED asset and "
		"the scene this game boots into, so this is a DEFECT, not a skip -- restore the "
		"file or re-author it with a *_True tools boot.",
		szZM_COMMITTED_FRONTEND_SCENE);
	if (pData == nullptr)
	{
		return;   // already FAILED above; nothing further is meaningful
	}

	// The panel and the header first: cells authored onto a screen with no backing panel
	// would draw over whatever is behind them, and the assertion set below would not
	// notice on its own.
	const u_int uPanelHits =
		CountNameOccurrences(pData, ulSize, ZM_UI_StarterChoice::szPANEL_NAME);
	const u_int uHeaderHits =
		CountNameOccurrences(pData, ulSize, ZM_UI_StarterChoice::szHEADER_NAME);

	u_int auCellHits[ZM_UI_StarterChoice::uCELL_COUNT] = {};
	for (u_int u = 0u; u < ZM_UI_StarterChoice::uCELL_COUNT; ++u)
	{
		auCellHits[u] =
			CountNameOccurrences(pData, ulSize, ZM_UI_StarterChoice::CellElementName(u));
	}

	Zenith_Log(LOG_CATEGORY_GAMEPLAY,
		"[ZM_CommittedSceneBytes] '%s' %llu bytes; starter screen panel=%u header=%u cells=%u",
		szZM_COMMITTED_FRONTEND_SCENE, (unsigned long long)ulSize,
		uPanelHits, uHeaderHits, ZM_UI_StarterChoice::uCELL_COUNT);

	Zenith_FileAccess::FreeFileData(pData);

	// EXACTLY ONE occurrence each: zero means the element was never authored (or its name
	// drifted from the constant), and more than one means it was authored twice, which
	// would leave FindElement resolving whichever the canvas stored first.
	ZENITH_ASSERT_EQ(uPanelHits, 1u,
		"the committed FrontEnd.zscen must carry the starter PANEL element '%s' exactly "
		"once -- add its AddStep_CreateUIRect to the ZM_MenuRoot block in Zenithmon.cpp "
		"and re-author the scene with a *_True tools boot",
		ZM_UI_StarterChoice::szPANEL_NAME);
	ZENITH_ASSERT_EQ(uHeaderHits, 1u,
		"the committed FrontEnd.zscen must carry the starter HEADER element '%s' exactly once",
		ZM_UI_StarterChoice::szHEADER_NAME);

	for (u_int u = 0u; u < ZM_UI_StarterChoice::uCELL_COUNT; ++u)
	{
		ZENITH_ASSERT_EQ(auCellHits[u], 1u,
			"the committed FrontEnd.zscen must carry starter CELL %u ('%s') exactly once. A "
			"ZERO here is the mutation the pure units cannot see: the cell is missing from "
			"the authoring loop, or its authored name no longer matches CellElementName, so "
			"FindElement returns nullptr every frame and the row is dead.",
			u, ZM_UI_StarterChoice::CellElementName(u));
	}
}

// ============================================================================
// The S8 professor's BOOT-LEVEL TRIPWIRE -- and it is deliberately no more than
// that.
//
// ★ WHAT THIS IS NOT. A name occurring in a file proves a STRING is in the file.
// It cannot say where Professor Aster stands, how big he is, what shape his body
// wears, or whether he is talkable -- and none of the pure units in
// Tests/ZM_Tests_ProfLabPlacement.cpp can see the bytes at all, because they read
// the same compiled constants the authoring writes from, so both sides of every
// one of their claims move together. THE PROOF is clause I3 of
// ZM_AutoTests_ProfLab.cpp, which resolves him in the LOADED scene and compares
// his live transform, collider and interactable. This unit is the cheap thing
// that runs at boot and says "he was never authored at all" in one line, seconds
// before that test would have said it in a hundred frames.
//
// ★ IT IS STRICT ABOUT AN UNREADABLE FILE, like the FrontEnd clause above and
// UNLIKE the Dawnmere one at the top. ProfLab.zscen is TRACKED (ZM-D-148,
// ZM-D-174) and the whole ZM-D-147 deviation on this asset family is that its
// absence is a DEFECT, not a configuration to tolerate -- a skip counts as a PASS
// and would silence this exactly when it broke. FAIL, never return quietly.
// ============================================================================
ZENITH_TEST(ZM_CommittedSceneBytes, ProfLabCarriesTheAuthoredProfessorEntityName)
{
	uint64_t ulSize = 0;
	char* pData = Zenith_FileAccess::ReadFile(szZM_COMMITTED_PROFLAB_SCENE, ulSize);

	ZENITH_ASSERT_NOT_NULL(pData,
		"the committed ProfLab.zscen could not be read ('%s'). It is a TRACKED asset "
		"(ZM-D-148 / ZM-D-174), so this is a DEFECT and not a skip -- restore the file "
		"or re-author it with a *_True tools boot.",
		szZM_COMMITTED_PROFLAB_SCENE);
	if (pData == nullptr)
	{
		return;   // already FAILED above; nothing further is meaningful
	}

	// DERIVED from the placement header, never typed: a hand-written needle would
	// move in lockstep with a renamed entity and pin nothing.
	const u_int uAsterHits =
		CountNameOccurrences(pData, ulSize, szZM_PROFLAB_ASTER_ENTITY_NAME);

	Zenith_Log(LOG_CATEGORY_GAMEPLAY,
		"[ZM_CommittedSceneBytes] '%s' %llu bytes; professor entity '%s' occurs %u time(s)",
		szZM_COMMITTED_PROFLAB_SCENE, (unsigned long long)ulSize,
		szZM_PROFLAB_ASTER_ENTITY_NAME, uAsterHits);

	Zenith_FileAccess::FreeFileData(pData);

	// EXACTLY ONE: zero means the scene was never re-authored after the professor's
	// authoring step landed (or his name drifted from the header constant), and more
	// than one means he was authored twice, which would leave FindEntityByName
	// resolving whichever entity the scene stored first.
	ZENITH_ASSERT_EQ(uAsterHits, 1u,
		"the committed ProfLab.zscen must carry the professor's entity name '%s' exactly "
		"once. A ZERO is the state a source-only change leaves behind: the authoring step "
		"is in Zenithmon.cpp but no *_True tools boot has re-authored and re-committed the "
		"scene, so the shipped lab is still empty. Do NOT weaken this to a >= 0 check -- "
		"re-author the scene.",
		szZM_PROFLAB_ASTER_ENTITY_NAME);
}
