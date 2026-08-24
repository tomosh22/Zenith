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
#include <string>    // the R1-2 scene paths, BUILT from the registry's file stems

#include "Core/Zenith_TestFramework.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Maths/Zenith_Maths.h"
#include "Zenithmon/Components/ZM_WarpTrigger.h"   // R1-3: uSERIALIZATION_VERSION + uTAG_CAPACITY -- the payload needle's own shape, never re-spelled
#include "Zenithmon/Source/UI/ZM_UI_StarterChoice.h"   // the S8 element-name constants (the needles)
#include "Zenithmon/Source/World/ZM_DawnmerePlacement.h"
#include "Zenithmon/Source/World/ZM_ProfLabPlacement.h"   // szZM_PROFLAB_ASTER_ENTITY_NAME + the SC-E lab-seam names/tag (the needles)
#include "Zenithmon/Source/World/ZM_Route1Placement.h"    // R1-2: Route 1's entity names + the gate tag RESOLVERS
#include "Zenithmon/Source/World/ZM_SceneRegistry.h"      // R1-2: the file STEMS the two new scene paths are built from
#include "Zenithmon/Source/World/ZM_ThornacrePlacement.h" // R1-2: Thornacre's entity names
#include "Zenithmon/Tests/ZM_Tests_ComponentTypeNames.h"  // the ONE inventory of serialized component TYPE names

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

	// ---- R1-2: the two scenes committed by step 2 ---------------------------
	//
	// ★ THEIR PATHS ARE BUILT, NOT SPELLED, and the three constants above are
	// deliberately left alone rather than churned into the same shape. The file
	// STEM is Source/World/ZM_SceneRegistry.h's property -- it is the ONE
	// enumerable inventory of "which scene has a .zscen, under what stem", and
	// Project_LoadInitialScene builds its registration paths from exactly this
	// expression -- so a stem edit moves the needle test with the loader instead
	// of leaving a needle pointed at a file nobody writes any more.
	// ZM_FindSceneFileStem returns a const char*, which compile-time literal
	// concatenation cannot consume, hence std::string here and a literal above.
	//
	// TOTAL: an unregistered scene yields "" and therefore a path ending in
	// "Scenes/.zscen", which ReadFile misses -- and every clause below treats an
	// unreadable TRACKED scene as a DEFECT, so that failure is loud.
	std::string ZM_CommittedScenePath(ZM_SCENE_ID eScene)
	{
		return std::string(GAME_ASSETS_DIR) + "Scenes/"
			+ ZM_FindSceneFileStem(eScene) + ZENITH_SCENE_EXT;
	}

	// ---- The serialized component TYPE names these clauses count ------------
	//
	// ★ WHY THEY ARE SPELLED HERE AND THEN CHECKED, rather than read out of the
	// shared inventory by ordinal. Tests/ZM_Tests_ComponentTypeNames.h is an
	// ARRAY with no per-row symbol, so aszZM_GAME_COMPONENT_TYPE_NAMES[6] would
	// be a magic index that silently names a different component the day a row
	// is inserted. The spelling below is instead VALIDATED against that
	// inventory by GameComponentTypeIsRegistered before it is used, which closes
	// the failure mode a zero-count needle is uniquely prone to: a typo, or a
	// registry rename that reached the inventory and not this file, would
	// otherwise count zero occurrences of a string nothing ever serializes and
	// PASS -- vacuously, and exactly on the clause whose whole job is to assert
	// an absence.
	const char* const szZM_TYPE_WARP_TRIGGER = "ZM_WarpTrigger";
	const char* const szZM_TYPE_SPAWN_POINT = "ZM_SpawnPoint";
	const char* const szZM_TYPE_PLAYER_CONTROLLER = "ZM_PlayerController";
	// R1-4 scene-attach (ZM-66/ZM-D-205): the wild-encounter producer.
	const char* const szZM_TYPE_TALL_GRASS_SYSTEM = "ZM_TallGrassSystem";

	bool GameComponentTypeIsRegistered(const char* szTypeName)
	{
		if (szTypeName == nullptr)
		{
			return false;
		}
		for (u_int u = 0u; u < uZM_GAME_COMPONENT_TYPE_COUNT; ++u)
		{
			if (std::strcmp(aszZM_GAME_COMPONENT_TYPE_NAMES[u], szTypeName) == 0)
			{
				return true;
			}
		}
		return false;
	}

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

	// ========================================================================
	// R1-3 -- THE WARP PAYLOAD NEEDLE, AND THE MUTATION IT EXISTS FOR
	//
	// ★★ A GATE SWAP IS INVISIBLE TO EVERY NAME-BASED NEEDLE IN THIS FILE. Both
	// Route 1 connections carry the SAME spawn tag -- the compiled row is
	// { ZM_SCENE_DAWNMERE, "FromRoute1" }, { ZM_SCENE_THORNACRE, "FromRoute1" } --
	// so authoring the SOUTH gate against Thornacre and the NORTH gate against
	// Dawnmere changes not one byte of any entity name, any tag string, or any
	// component type name. Every clause below that counts strings passes. What
	// ships is a route whose south end leads north: a player walking back toward
	// Dawnmere arrives in Thornacre, and the return gate there sends them to the
	// Route 1 NORTH marker, i.e. the world folds in half with no error anywhere.
	//
	// The ONE discriminating value is the target build index (2 vs 3), which
	// ZM_WarpTrigger::WriteToDataStream emits as a raw u_int immediately before
	// the zero-padded tag buffer. So the needle is the WHOLE payload:
	//
	//     [u_int uSERIALIZATION_VERSION][u_int targetBuildIndex][uTAG_CAPACITY tag]
	//
	// ★ ITS SHAPE IS READ FROM ZM_WarpTrigger, NEVER RE-SPELLED. A hand-typed 40
	// would silently stop matching the day the component gains a field, and a
	// needle that matches nothing counts zero -- which passes an absence clause
	// vacuously and fails a presence clause for a reason its message does not name.
	// ========================================================================
	constexpr uint64_t ulZM_WARP_PAYLOAD_SIZE =
		(uint64_t)sizeof(u_int) * 2u + (uint64_t)ZM_WarpTrigger::uTAG_CAPACITY;

	// FALSE (and a zeroed needle) when the arguments could not produce a payload
	// any authored gate could carry -- an unresolved build index, a null/empty tag,
	// or a tag too long for the fixed buffer. Callers assert on the return BEFORE
	// reading anything into a count, because a zeroed needle would match a run of
	// padding rather than a gate.
	bool BuildWarpPayloadNeedle(
		u_int uTargetBuildIndex,
		const char* szSpawnTag,
		unsigned char (&auNeedleOut)[ulZM_WARP_PAYLOAD_SIZE])
	{
		std::memset(auNeedleOut, 0, sizeof(auNeedleOut));
		if (szSpawnTag == nullptr || szSpawnTag[0] == '\0'
			|| uTargetBuildIndex == ZM_WarpTrigger::uINVALID_BUILD_INDEX)
		{
			return false;
		}
		const size_t ulTagLength = std::strlen(szSpawnTag);
		if (ulTagLength >= (size_t)ZM_WarpTrigger::uTAG_CAPACITY)
		{
			return false;
		}

		// ZM_WarpTrigger::Configure copies the tag into a ZERO-INITIALISED
		// uTAG_CAPACITY buffer and then writes the whole buffer, so the trailing
		// bytes are guaranteed zero -- which is what makes a fixed-width needle
		// legitimate here rather than an assumption about padding.
		const u_int uVersion = ZM_WarpTrigger::uSERIALIZATION_VERSION;
		std::memcpy(auNeedleOut, &uVersion, sizeof(uVersion));
		std::memcpy(auNeedleOut + sizeof(uVersion),
			&uTargetBuildIndex, sizeof(uTargetBuildIndex));
		std::memcpy(auNeedleOut + sizeof(uVersion) + sizeof(uTargetBuildIndex),
			szSpawnTag, ulTagLength);
		return true;
	}

	// The FIRST byte offset a needle occurs at, or ulSize when it does not occur
	// at all. Used only for the ORDERING claim below; every presence claim is a
	// count, so an absent needle lands past every real offset and fails loudly
	// rather than comparing as "earliest".
	uint64_t FindFirstOffset(const char* pData, uint64_t ulSize,
		const unsigned char* puNeedle, uint64_t ulNeedleSize)
	{
		if (pData == nullptr || ulNeedleSize == 0 || ulSize < ulNeedleSize)
		{
			return ulSize;
		}
		const unsigned char* puData = reinterpret_cast<const unsigned char*>(pData);
		for (uint64_t ul = 0; ul + ulNeedleSize <= ulSize; ++ul)
		{
			if (std::memcmp(puData + ul, puNeedle, (size_t)ulNeedleSize) == 0)
			{
				return ul;
			}
		}
		return ulSize;
	}

	uint64_t FindFirstNameOffset(const char* pData, uint64_t ulSize, const char* szName)
	{
		if (szName == nullptr || szName[0] == '\0')
		{
			return ulSize;
		}
		return FindFirstOffset(pData, ulSize,
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
// missing, the warp is ACCEPTED, the fade goes fully opaque, and the machine
// stalls in ZM_WARP_TRANSITION_WAITING_FOR_SPAWN until that barrier's frame budget
// expires (ZM-D-200). The player is frozen behind a black screen until then, and
// what he gets back is a Zenith_Error in the log and a town he never arrived in.
// Not a crash, not an assert, not a red test -- which is exactly why the check has
// to be somewhere, and why "somewhere" has to run in CI.
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
// tagged it, which is precisely the WAITING_FOR_SPAWN stall (the resolver matches on
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
		"shipped game warps into WAITING_FOR_SPAWN and sits behind an opaque fade for "
		"that barrier's whole frame budget (ZM-D-200) the moment anyone leaves the "
		"lab, then errors out into a town it never arrived in. More than one means "
		"the marker was authored twice.",
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
	// name -- an untagged marker, which resolves to nothing and stalls the warp.
	ZENITH_ASSERT_GT(uTagHits, uNameHits,
		"the committed Dawnmere.zscen contains the spawn tag '%s' %u time(s) and the "
		"entity name '%s' %u time(s) -- and the tag is a PREFIX of the name, so those "
		"counts being equal means every occurrence of the tag is part of the name and "
		"the marker was never actually TAGGED. ZM_SpawnPoint resolution matches on the "
		"TAG, not on the entity name, so an untagged marker leaves "
		"RequestWarp(<Dawnmere>, \"%s\") stalled in WAITING_FOR_SPAWN behind a fully "
		"opaque fade with the player frozen for that barrier's whole frame budget "
		"(ZM-D-200), then erroring out with nowhere to have arrived. Check that the "
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

// ============================================================================
// R1-2 / R1-3 -- THE THREE-SCENE SEAM TRIPWIRE: Dawnmere, Route 1 and Thornacre.
//
// ★★ WHY ALL THREE ARE HERE AND NOT IN THE PLACEMENT SUITES. Every pure unit in
// Tests/ZM_Tests_{Route1,Thornacre,Dawnmere}Placement.cpp reads the SAME compiled
// constants the authoring writes from, so both sides of every claim they make move
// together and not one of them can tell whether a scene was ever re-authored. The
// automated round trips that DO walk the real seam need the GITIGNORED terrain
// bakes and therefore RequestSkip on CI -- and a skip counts as a PASS. Route 1
// and Thornacre are the first committed Zenithmon scenes CI cannot load-verify at
// all (ZM-D-199), which makes these boot units, reading TRACKED files with no GPU
// and no terrain, the whole CI-visible spine of both slices.
//
// ★★ THE MUTATION THEY EXIST FOR IS THE QUIET ONE. IsWarpDestinationValid consults
// ONLY the compiled ZM_WorldSpec tag list and NEVER the destination scene, so a
// marker OR A GATE that is missing, mis-named, untagged or pointed the WRONG WAY
// still validates: the fade goes fully opaque and the machine sits in
// ZM_WARP_TRANSITION_WAITING_FOR_SPAWN until that barrier's frame budget expires
// (ZM-D-200), then errors out into a town the player never arrived in. Not a
// crash, not an assert, not a red test anywhere else.
//
// ★ R1-2 LANDED THE MARKERS AND R1-3 THE FOUR GATES, so each clause below now
// makes both halves of its scene's claim. The one thing a STRING count cannot say
// -- which gate points where -- has its own unit at the bottom of this file.
//
// ★ EVERY NEEDLE IS DERIVED, none is typed: entity names from the placement
// headers, spawn tags from the compiled world table through the SAME resolvers the
// authoring calls, component type names validated against the shared inventory in
// Tests/ZM_Tests_ComponentTypeNames.h. A hand-typed needle moves in lockstep with
// a rename and pins nothing.
//
// ★ THEY ARE ALL STRICT ABOUT AN UNREADABLE FILE, like the FrontEnd, ProfLab and
// lab-seam clauses above and unlike the Dawnmere ROTATION clause at the top of
// this file. All seven .zscen are TRACKED (ZM-D-148 / ZM-D-174 / ZM-D-199) and the
// standing position on this asset family is that absence is a DEFECT, not a
// configuration to tolerate.
//
// ★★ AND EVERY CLAUSE BELOW IS RED BY DESIGN UNTIL THE RE-AUTHOR LANDS -- which
// was true of the Dawnmere marker at R1-2 and is true of ALL THREE SCENES' gate
// clauses at R1-3. The authoring steps ship in the same commit as this file, but
// the committed bytes only move when a WINDOWED Vulkan_*_True tools boot
// re-writes them (Dawnmere additionally needs sceneAuthoring=AUTHOR_DAWNMERE) --
// a headless run may CREATE a .zscen but never CHANGE one. So THAT BOOT MUST
// CARRY --skip-unit-tests: a failing boot unit aborts the boot BEFORE scene
// authoring runs, and without the flag these units block their own fix forever.
// ============================================================================

// Dawnmere's half of the seam: the FromRoute1 arrival marker (R1-2 step 3).
ZENITH_TEST(ZM_CommittedSceneBytes, DawnmereCarriesTheRoute1ArrivalMarkerAndItsInboundTag)
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

	const char* const szMarkerName = szZM_DAWNMERE_FROM_ROUTE1_SPAWN_ENTITY_NAME;

	// ★ THE TAG COMES OFF THE ROUTE1 ROW'S DAWNMERE EDGE, which is the identical
	// table entry the authoring's ZM_ResolveInboundSpawnTag(ROUTE1, DAWNMERE)
	// walks and the identical one Route 1's south gate ASKS Dawnmere for (R1-3
	// authored that sensor). One edge, one spelling, both halves of the seam -- so
	// a table re-tag moves the marker, the sensor and this needle together, and a
	// needle that quietly disagreed with the authoring is not expressible.
	const char* const szInboundTag = ZM_GetRoute1SouthGateSpawnTag();

	const u_int uNameHits = CountNameOccurrences(pData, ulSize, szMarkerName);
	const u_int uTagHits = CountNameOccurrences(pData, ulSize, szInboundTag);
	const u_int uSpawnPointHits =
		CountNameOccurrences(pData, ulSize, szZM_TYPE_SPAWN_POINT);
	const u_int uWarpTriggerHits =
		CountNameOccurrences(pData, ulSize, szZM_TYPE_WARP_TRIGGER);
	// R1-3's addition to this scene: the outbound sensor that aims at Route 1.
	const u_int uNorthGateHits = CountNameOccurrences(
		pData, ulSize, szZM_DAWNMERE_NORTH_GATE_ENTITY_NAME);

	Zenith_Log(LOG_CATEGORY_GAMEPLAY,
		"[ZM_CommittedSceneBytes] '%s' %llu bytes; route seam: marker '%s' x%u, "
		"inbound tag '%s' x%u (name-inclusive), ZM_SpawnPoint x%u, ZM_WarpTrigger "
		"x%u, north gate '%s' x%u",
		szZM_COMMITTED_DAWNMERE_SCENE, (unsigned long long)ulSize,
		szMarkerName, uNameHits, szInboundTag, uTagHits,
		uSpawnPointHits, uWarpTriggerHits,
		szZM_DAWNMERE_NORTH_GATE_ENTITY_NAME, uNorthGateHits);

	Zenith_FileAccess::FreeFileData(pData);

	// The tag has to be a real string before any count over it means anything --
	// an empty needle counts 0 and would turn the strict inequality below into an
	// arithmetic puzzle instead of a clear failure.
	ZENITH_ASSERT_TRUE(szInboundTag != nullptr && szInboundTag[0] != '\0',
		"the compiled ZM_SCENE_ROUTE1 row carries no connection targeting "
		"ZM_SCENE_DAWNMERE, so ZM_GetRoute1SouthGateSpawnTag() resolved to the empty "
		"string. Route 1 has no way back south and there is no tag to look for in the "
		"committed scene -- fix Source/Data/ZM_WorldSpec.cpp first.");

	// The same anti-vacuity guard the Route 1 and Thornacre clauses carry. It bites
	// LESS here than it does there -- both component clauses below are `== N` for a
	// non-zero N, so a needle nothing serializes fails them rather than passing
	// them -- but it is what turns a registry rename into a message that names the
	// cause instead of a baffling `4 != 0` against a scene that is perfectly fine.
	ZENITH_ASSERT_TRUE(GameComponentTypeIsRegistered(szZM_TYPE_SPAWN_POINT)
			&& GameComponentTypeIsRegistered(szZM_TYPE_WARP_TRIGGER),
		"one of the component TYPE needles ('%s', '%s') is not a row of the shared "
		"inventory in Tests/ZM_Tests_ComponentTypeNames.h -- reconcile the spelling "
		"before reading anything into the counts below, which are taken over a string "
		"nothing serializes.",
		szZM_TYPE_SPAWN_POINT, szZM_TYPE_WARP_TRIGGER);

	// ★ THE HEADER CONSTANT AND THE MEASURED ROW'S LABEL MUST AGREE. The route-seam
	// ground row in Source/World/ZM_DawnmerePlacement.cpp carries the marker's name
	// so a test can resolve the authored entity without re-spelling it, and
	// ZM_DawnmereRouteSeamGroundTruth_Test prints that label on the `paste=` line a
	// re-measure is copied from. The name is spelled in BOTH places -- the header
	// constant the authoring and this needle read, and that .cpp row -- and nothing
	// but this clause compares them, so without it a divergence would leave the
	// oracle naming an entity the scene does not contain, silently and forever.
	const ZM_DawnmereNpcAnchor& xSeamRow =
		ZM_GetDawnmereRouteSeamSample(ZM_DAWNMERE_ROUTE_SEAM_SAMPLE_FROM_ROUTE1);
	ZENITH_ASSERT_STREQ(szMarkerName, xSeamRow.m_szEntityName,
		"the FromRoute1 marker's entity name is '%s' in "
		"Source/World/ZM_DawnmerePlacement.h but the measured route-seam row in that "
		"file's .cpp is labelled '%s'. One of the two moved without the other: the "
		"ground oracle's paste line now names an entity the committed scene does not "
		"contain, so a re-measure would be pasted against the wrong column.",
		szMarkerName, xSeamRow.m_szEntityName);

	ZENITH_ASSERT_EQ(uNameHits, 1u,
		"the committed Dawnmere.zscen must carry the Route 1 arrival marker '%s' "
		"exactly once. A ZERO is the state a source-only change leaves behind: the "
		"authoring steps are in Zenithmon.cpp but no WINDOWED *_True tools boot in "
		"sceneAuthoring=AUTHOR_DAWNMERE mode has re-authored and re-committed the "
		"scene -- a headless run may CREATE a .zscen but never CHANGE one. Until it "
		"does, R1-3's Route 1 south gate would warp into a town with nowhere to "
		"arrive. More than one means the marker was authored twice.",
		szMarkerName);

	// ★ THE PREFIX-TRAP CLAUSE, and it is the FromLab / FromLabSpawn shape exactly:
	// the tag "FromRoute1" is a strict PREFIX of the name "FromRoute1Spawn", so
	// every occurrence of the name is also an occurrence of the tag and a bare
	// `tagHits > 0` would be satisfied by the name alone -- i.e. by a marker that
	// was created and never TAGGED. Spawn resolution matches on the TAG, not the
	// name, so that is the WAITING_FOR_SPAWN stall, passing.
	ZENITH_ASSERT_GT(uTagHits, uNameHits,
		"the committed Dawnmere.zscen contains the spawn tag '%s' %u time(s) and the "
		"marker name '%s' %u time(s) -- and the tag is a PREFIX of the name, so those "
		"counts being equal means every occurrence of the tag is part of the name and "
		"the marker was never actually TAGGED. Check that the authoring still calls "
		"AddStep_Custom(&ZM_ConfigureDawnmereFromRoute1ArrivalSpawnPoint) and that the "
		"scene was re-authored afterwards.",
		szInboundTag, uTagHits, szMarkerName, uNameHits);

	// FOUR markers now: TownCenterSpawn, FromHomeSpawn, FromLabSpawn and this one.
	// The clause is about the COMPONENT, which the name clause above cannot see: an
	// entity authored under the right name but without its ZM_SpawnPoint is a bare
	// transform that resolves to nothing. Bump this literal in the same commit as
	// any slice that adds or removes a Dawnmere marker.
	ZENITH_ASSERT_EQ(uSpawnPointHits, 4u,
		"the committed Dawnmere.zscen serializes %u ZM_SpawnPoint component(s), not "
		"the 4 it authors (TownCenterSpawn, FromHomeSpawn, FromLabSpawn and '%s'). A "
		"count one short with the name clause above green means the marker entity "
		"exists but carries no ZM_SpawnPoint -- an untagged, unresolvable transform.",
		uSpawnPointHits, szMarkerName);

	// ★ THREE SENSORS SINCE R1-3. HomeDoorTrigger and LabDoorTrigger are the two
	// interior doorways; DawnmereNorthGate is the outbound seam onto Route 1, and
	// it was deliberately WITHHELD by R1-2 until every arrival marker existed (a
	// trigger authored before its destination marker validates against the
	// compiled table and then stalls in WAITING_FOR_SPAWN, ZM-D-200). Bump this
	// literal in the same commit as any slice that adds or removes a Dawnmere
	// sensor -- it is the only clause that can see one authored under a name no
	// needle here knows about.
	ZENITH_ASSERT_EQ(uWarpTriggerHits, 3u,
		"the committed Dawnmere.zscen serializes %u ZM_WarpTrigger component(s), not "
		"the 3 it authors (HomeDoorTrigger, LabDoorTrigger, '%s'). A count of 2 with "
		"the name clause below RED is the state a source-only R1-3 leaves behind: the "
		"authoring steps are in Zenithmon.cpp but no WINDOWED *_True tools boot in "
		"sceneAuthoring=AUTHOR_DAWNMERE mode has re-authored and re-committed the "
		"scene. A count of 4 means a sensor was authored twice, or one arrived under "
		"a name nothing here needles.",
		uWarpTriggerHits, szZM_DAWNMERE_NORTH_GATE_ENTITY_NAME);

	// ...and the ENTITY, which the component count above cannot distinguish from a
	// third doorway. Exactly one: a zero is the un-re-authored tree, and more than
	// one would leave FindEntityByName resolving whichever the scene stored first.
	ZENITH_ASSERT_EQ(uNorthGateHits, 1u,
		"the committed Dawnmere.zscen must carry the north seam gate '%s' exactly "
		"once. Without it Dawnmere is a dead end: Route 1's south gate can bring a "
		"player in, and nothing in town can send one out -- a ONE-WAY seam, which is "
		"exactly what landing all four triggers in one commit exists to prevent.",
		szZM_DAWNMERE_NORTH_GATE_ENTITY_NAME);
}

// Route 1's two arrival markers AND its two gate sensors -- the four entities
// that make the route a corridor rather than a cul-de-sac at both ends. Which
// gate points where is NOT this unit's claim: both gates ask for the same tag, so
// see EverySeamGatePayloadIsAuthoredExactlyOnceInItsOwnScene below.
ZENITH_TEST(ZM_CommittedSceneBytes, Route1CarriesBothArrivalMarkersAndBothGateSensors)
{
	const std::string strScenePath = ZM_CommittedScenePath(ZM_SCENE_ROUTE1);

	uint64_t ulSize = 0;
	char* pData = Zenith_FileAccess::ReadFile(strScenePath.c_str(), ulSize);

	ZENITH_ASSERT_NOT_NULL(pData,
		"the committed Route1.zscen could not be read ('%s'). It is a TRACKED asset "
		"(ZM-D-199), so this is a DEFECT and not a skip -- restore the file or "
		"re-author it with a WINDOWED *_True tools boot. Note the path is BUILT from "
		"Source/World/ZM_SceneRegistry.h's file stem, so an empty stem lands here too.",
		strScenePath.c_str());
	if (pData == nullptr)
	{
		return;   // already FAILED above; nothing further is meaningful
	}

	const u_int uSouthArrivalHits = CountNameOccurrences(
		pData, ulSize, szZM_ROUTE1_SOUTH_ARRIVAL_ENTITY_NAME);
	const u_int uNorthArrivalHits = CountNameOccurrences(
		pData, ulSize, szZM_ROUTE1_NORTH_ARRIVAL_ENTITY_NAME);
	const u_int uCameraHits = CountNameOccurrences(
		pData, ulSize, szZM_ROUTE1_CAMERA_ENTITY_NAME);
	const u_int uSouthGateHits = CountNameOccurrences(
		pData, ulSize, szZM_ROUTE1_SOUTH_GATE_ENTITY_NAME);
	const u_int uNorthGateHits = CountNameOccurrences(
		pData, ulSize, szZM_ROUTE1_NORTH_GATE_ENTITY_NAME);
	const u_int uPlayerNameHits = CountNameOccurrences(
		pData, ulSize, szZM_ROUTE1_PLAYER_ENTITY_NAME);
	const u_int uPlayerControllerHits =
		CountNameOccurrences(pData, ulSize, szZM_TYPE_PLAYER_CONTROLLER);
	const u_int uSpawnPointHits =
		CountNameOccurrences(pData, ulSize, szZM_TYPE_SPAWN_POINT);
	const u_int uWarpTriggerHits =
		CountNameOccurrences(pData, ulSize, szZM_TYPE_WARP_TRIGGER);

	// The tags Route 1 OFFERS -- "FromDawnmere" and "FromThornacre" -- are exactly
	// the tags its two arrival markers must carry, read straight off the compiled
	// row rather than resolved edge by edge. Neither is a substring of the other
	// and neither is contained in any Route 1 entity name (which
	// ZM_Route1Placement's own boot unit proves, clause 5), so these are plain
	// equalities and NOT the strictly-more form Dawnmere's marker needs.
	//
	// The 8-slot bound is a fixed local buffer, clamped rather than asserted: this
	// row offers two tags today and the clause below walks whatever it offers up to
	// eight. If Route 1 ever offers more than eight, the extras are simply not
	// counted -- widen the buffer, do not read past it.
	const ZM_WorldSpec& xRoute1Row = ZM_GetWorldSpec(ZM_SCENE_ROUTE1);
	u_int uOfferedTagHits[8] = {};
	const u_int uOfferedTagCount = xRoute1Row.m_uSpawnTagCount < 8u
		? xRoute1Row.m_uSpawnTagCount : 8u;
	for (u_int uTag = 0u; uTag < uOfferedTagCount; ++uTag)
	{
		const char* szTag = xRoute1Row.m_pszSpawnTags != nullptr
			? xRoute1Row.m_pszSpawnTags[uTag] : nullptr;
		uOfferedTagHits[uTag] = CountNameOccurrences(pData, ulSize, szTag);
	}

	// The tag Route 1's two gates ASK FOR. Both accessors walk the ROUTE1 row and
	// both answer "FromRoute1" -- an OUTBOUND tag Route 1 does not itself offer --
	// so ONE count covers both and the expected total is TWO, one per gate's
	// zero-padded 32-byte buffer. That the two answers are the same string is the
	// whole reason the payload unit below exists, and it is asserted rather than
	// assumed by the equality clause further down.
	const u_int uSouthGateTagHits =
		CountNameOccurrences(pData, ulSize, ZM_GetRoute1SouthGateSpawnTag());
	const u_int uNorthGateTagHits =
		CountNameOccurrences(pData, ulSize, ZM_GetRoute1NorthGateSpawnTag());

	Zenith_Log(LOG_CATEGORY_GAMEPLAY,
		"[ZM_CommittedSceneBytes] '%s' %llu bytes; arrivals S x%u N x%u, camera x%u, "
		"gates S x%u N x%u, player name x%u / controller x%u, ZM_SpawnPoint x%u, "
		"ZM_WarpTrigger x%u, offered tags %u, outbound tag hits S x%u N x%u",
		strScenePath.c_str(), (unsigned long long)ulSize,
		uSouthArrivalHits, uNorthArrivalHits, uCameraHits,
		uSouthGateHits, uNorthGateHits, uPlayerNameHits, uPlayerControllerHits,
		uSpawnPointHits, uWarpTriggerHits, uOfferedTagCount,
		uSouthGateTagHits, uNorthGateTagHits);

	Zenith_FileAccess::FreeFileData(pData);

	ZENITH_ASSERT_TRUE(GameComponentTypeIsRegistered(szZM_TYPE_WARP_TRIGGER)
			&& GameComponentTypeIsRegistered(szZM_TYPE_SPAWN_POINT)
			&& GameComponentTypeIsRegistered(szZM_TYPE_PLAYER_CONTROLLER),
		"one of the component TYPE needles ('%s', '%s', '%s') is not a row of the "
		"shared inventory in Tests/ZM_Tests_ComponentTypeNames.h. A needle on a "
		"string nothing serializes counts ZERO and would pass every absence clause "
		"below vacuously -- reconcile the spelling before trusting any count.",
		szZM_TYPE_WARP_TRIGGER, szZM_TYPE_SPAWN_POINT, szZM_TYPE_PLAYER_CONTROLLER);

	ZENITH_ASSERT_EQ(uSouthArrivalHits, 1u,
		"the committed Route1.zscen must carry the SOUTH arrival marker '%s' exactly "
		"once -- it is where a player walking north out of Dawnmere lands.",
		szZM_ROUTE1_SOUTH_ARRIVAL_ENTITY_NAME);
	ZENITH_ASSERT_EQ(uNorthArrivalHits, 1u,
		"the committed Route1.zscen must carry the NORTH arrival marker '%s' exactly "
		"once -- it is where a player walking south out of Thornacre lands.",
		szZM_ROUTE1_NORTH_ARRIVAL_ENTITY_NAME);
	ZENITH_ASSERT_EQ(uCameraHits, 1u,
		"the committed Route1.zscen must carry the follow camera '%s' exactly once.",
		szZM_ROUTE1_CAMERA_ENTITY_NAME);

	ZENITH_ASSERT_EQ(uSpawnPointHits, 2u,
		"the committed Route1.zscen serializes %u ZM_SpawnPoint component(s), not the "
		"2 it authors. The name clauses above cannot see this: an entity authored "
		"under the right name but without its ZM_SpawnPoint is a bare transform no "
		"warp can resolve.", uSpawnPointHits);

	// ★ EXACTLY ONE ZM_PlayerController, and that is the load-bearing property --
	// not the entity name. ZM_FollowCamera::ResolveTarget acquires the UNIQUE
	// ZM_PlayerController in the camera's own scene (Q-2026-08-15-002); zero or two
	// resolve to no target rather than a guess, and a camera with no target parks
	// ZM_GameStateManager::PollForCameraAndBeginFadeIn on its barrier.
	ZENITH_ASSERT_EQ(uPlayerControllerHits, 1u,
		"the committed Route1.zscen serializes %u ZM_PlayerController component(s), "
		"not exactly 1. The follow camera acquires the UNIQUE controller in its own "
		"scene, so zero or two leave it with no target and the arrival fade with "
		"nothing to poll for.", uPlayerControllerHits);

	// ★ THE STRICTLY-MORE CLAUSE, and it is REQUIRED here rather than tidy: the
	// entity name "Player" is a strict substring of the serialized type name
	// "ZM_PlayerController", so a `== 1` equality on the name is arithmetically
	// impossible in any scene that has a controller. The real claim is that at
	// least one occurrence of "Player" is NOT part of the type name.
	ZENITH_ASSERT_GT(uPlayerNameHits, uPlayerControllerHits,
		"the committed Route1.zscen contains '%s' %u time(s) and the type name '%s' "
		"%u time(s). Every occurrence of the type name contains the entity name, so "
		"equality means the player ENTITY was never authored -- only its component "
		"type string is present.",
		szZM_ROUTE1_PLAYER_ENTITY_NAME, uPlayerNameHits,
		szZM_TYPE_PLAYER_CONTROLLER, uPlayerControllerHits);

	ZENITH_ASSERT_GT(uOfferedTagCount, 0u,
		"the compiled ZM_SCENE_ROUTE1 row offers no spawn tags, so the tag clauses "
		"below walk nothing -- fix Source/Data/ZM_WorldSpec.cpp first.");
	for (u_int uTag = 0u; uTag < uOfferedTagCount; ++uTag)
	{
		const char* szTag = xRoute1Row.m_pszSpawnTags != nullptr
			? xRoute1Row.m_pszSpawnTags[uTag] : "";
		ZENITH_ASSERT_TRUE(szTag != nullptr && szTag[0] != '\0',
			"offered spawn tag %u of the ZM_SCENE_ROUTE1 row is null or empty, so the "
			"count taken over it is meaningless", uTag);
		ZENITH_ASSERT_EQ(uOfferedTagHits[uTag], 1u,
			"the committed Route1.zscen carries the spawn tag '%s' %u time(s), not "
			"exactly once. Route 1 OFFERS this tag in the compiled world table, so "
			"exactly one of its arrival markers must be tagged with it -- a ZERO is a "
			"warp that validates and then stalls in WAITING_FOR_SPAWN behind an opaque "
			"fade (ZM-D-200), which no other test in the CI gate can see.",
			szTag, uOfferedTagHits[uTag]);
	}

	// ★★ EXACTLY TWO TRIGGERS SINCE R1-3, and the two name clauses below are not
	// enough on their own: they needle only the DECLARED gate names, so a THIRD
	// sensor authored under any other name would be invisible to them. The
	// component-type count is what closes that, and it is what a later slice that
	// adds (say) a gym-lane gate has to bump deliberately.
	ZENITH_ASSERT_EQ(uWarpTriggerHits, 2u,
		"the committed Route1.zscen serializes %u ZM_WarpTrigger component(s), not the "
		"2 it authors ('%s', '%s'). A ZERO is the state a source-only R1-3 leaves "
		"behind -- the authoring steps are in Zenithmon.cpp but no WINDOWED *_True "
		"tools boot has re-authored and re-committed the scene, so the shipped route "
		"is a corridor with no way off it at either end.",
		uWarpTriggerHits, szZM_ROUTE1_SOUTH_GATE_ENTITY_NAME,
		szZM_ROUTE1_NORTH_GATE_ENTITY_NAME);
	ZENITH_ASSERT_EQ(uSouthGateHits, 1u,
		"the committed Route1.zscen must carry the SOUTH gate '%s' exactly once -- it "
		"is the only way back to Dawnmere.",
		szZM_ROUTE1_SOUTH_GATE_ENTITY_NAME);
	ZENITH_ASSERT_EQ(uNorthGateHits, 1u,
		"the committed Route1.zscen must carry the NORTH gate '%s' exactly once -- it "
		"is the only way on to Thornacre.",
		szZM_ROUTE1_NORTH_GATE_ENTITY_NAME);

	// ★★ THE TWO GATES ASK FOR THE SAME TAG, WHICH IS WHY THIS IS A COUNT OF TWO
	// AND NOT A PAIR OF SEPARATE CLAIMS. "FromRoute1" is an OUTBOUND tag Route 1
	// does not itself offer, so it can only get into this file inside a gate's
	// serialized tag buffer -- one occurrence per gate. THREE would mean an arrival
	// marker had been tagged with the outbound tag instead of its inbound one (the
	// exact confusion ZM_ResolveInboundSpawnTag exists to prevent, and one
	// IsWarpDestinationValid cannot see); ONE means a gate exists but was never
	// configured, i.e. a sensor that fires into nothing.
	//
	// ★ AND THIS CLAUSE SAYS NOTHING ABOUT WHICH GATE POINTS WHERE. It cannot:
	// swap the two and this count is unchanged. That claim belongs to the payload
	// unit below, which is the whole reason it exists.
	ZENITH_ASSERT_STREQ(
		ZM_GetRoute1SouthGateSpawnTag(), ZM_GetRoute1NorthGateSpawnTag(),
		"ZM_GetRoute1SouthGateSpawnTag() ('%s') and ZM_GetRoute1NorthGateSpawnTag() "
		"('%s') no longer resolve to the same string, so the shared-tag reasoning "
		"the count clause below rests on has changed -- and with it the reason the "
		"payload unit exists at all. That is not necessarily wrong (it would make the "
		"two gates distinguishable by tag), but re-read both before adjusting either.",
		ZM_GetRoute1SouthGateSpawnTag(), ZM_GetRoute1NorthGateSpawnTag());
	ZENITH_ASSERT_EQ(uSouthGateTagHits, 2u,
		"the committed Route1.zscen carries the outbound tag '%s' %u time(s), not the "
		"2 its two gate sensors must contribute (one zero-padded tag buffer each). "
		"Route 1 does not OFFER this tag, so nothing else in the file may carry it.",
		ZM_GetRoute1SouthGateSpawnTag(), uSouthGateTagHits);
}

// Thornacre's arrival marker and its ONE return gate -- and the gym door that is
// still deliberately UNBACKED.
ZENITH_TEST(ZM_CommittedSceneBytes, ThornacreCarriesItsReturnGateAndNoGymDoor)
{
	const std::string strScenePath = ZM_CommittedScenePath(ZM_SCENE_THORNACRE);

	uint64_t ulSize = 0;
	char* pData = Zenith_FileAccess::ReadFile(strScenePath.c_str(), ulSize);

	ZENITH_ASSERT_NOT_NULL(pData,
		"the committed Thornacre.zscen could not be read ('%s'). It is a TRACKED "
		"asset (ZM-D-199), so this is a DEFECT and not a skip -- restore the file or "
		"re-author it with a WINDOWED *_True tools boot.",
		strScenePath.c_str());
	if (pData == nullptr)
	{
		return;   // already FAILED above; nothing further is meaningful
	}

	// ★ THE ARRIVAL'S TAG IS ROUTE 1's THORNACRE EDGE, NOT Thornacre's own return
	// accessor. ZM_GetThornacreReturnSpawnTag() answers "FromThornacre" -- the tag
	// Thornacre's future gate ASKS Route 1 for -- and reaching for it here is the
	// natural mistake the resolver banner in Zenithmon.cpp is written about. Both
	// this and the authoring's ZM_ResolveInboundSpawnTag(ROUTE1, THORNACRE) walk the
	// SAME ROUTE1 row for the edge targeting Thornacre.
	const char* const szInboundTag = ZM_GetRoute1NorthGateSpawnTag();

	const u_int uArrivalHits = CountNameOccurrences(
		pData, ulSize, szZM_THORNACRE_SOUTH_ARRIVAL_ENTITY_NAME);
	const u_int uCameraHits = CountNameOccurrences(
		pData, ulSize, szZM_THORNACRE_CAMERA_ENTITY_NAME);
	const u_int uGateHits = CountNameOccurrences(
		pData, ulSize, szZM_THORNACRE_SOUTH_GATE_ENTITY_NAME);
	const u_int uPlayerNameHits = CountNameOccurrences(
		pData, ulSize, szZM_THORNACRE_PLAYER_ENTITY_NAME);
	const u_int uPlayerControllerHits =
		CountNameOccurrences(pData, ulSize, szZM_TYPE_PLAYER_CONTROLLER);
	const u_int uSpawnPointHits =
		CountNameOccurrences(pData, ulSize, szZM_TYPE_SPAWN_POINT);
	const u_int uWarpTriggerHits =
		CountNameOccurrences(pData, ulSize, szZM_TYPE_WARP_TRIGGER);
	const u_int uInboundTagHits =
		CountNameOccurrences(pData, ulSize, szInboundTag);

	Zenith_Log(LOG_CATEGORY_GAMEPLAY,
		"[ZM_CommittedSceneBytes] '%s' %llu bytes; arrival x%u (tag '%s' x%u), camera "
		"x%u, gate x%u, player name x%u / controller x%u, ZM_SpawnPoint x%u, "
		"ZM_WarpTrigger x%u",
		strScenePath.c_str(), (unsigned long long)ulSize,
		uArrivalHits, szInboundTag, uInboundTagHits, uCameraHits, uGateHits,
		uPlayerNameHits, uPlayerControllerHits, uSpawnPointHits, uWarpTriggerHits);

	// ★ EVERY OTHER TAG THORNACRE OFFERS MUST BE ABSENT, and today that means
	// "FromGym" -- the return marker for a gym nobody has built. ZM-D-196 rules
	// Thornacre a traversal STUB in this milestone; the compiled Thornacre->Gym1
	// edge is deliberately UNBACKED. Derived by walking the row and skipping the
	// arrival's own tag, so a third offered tag has to be BACKED or red here,
	// rather than being spelled and quietly forgotten.
	const ZM_WorldSpec& xThornacreRow = ZM_GetWorldSpec(ZM_SCENE_THORNACRE);
	u_int uUnbackedTagHits[8] = {};
	const u_int uOfferedTagCount = xThornacreRow.m_uSpawnTagCount < 8u
		? xThornacreRow.m_uSpawnTagCount : 8u;
	for (u_int uTag = 0u; uTag < uOfferedTagCount; ++uTag)
	{
		const char* szTag = xThornacreRow.m_pszSpawnTags != nullptr
			? xThornacreRow.m_pszSpawnTags[uTag] : nullptr;
		if (szTag == nullptr || szTag[0] == '\0'
			|| std::strcmp(szTag, szInboundTag) == 0)
		{
			continue;   // the arrival's own tag is asserted present below
		}
		uUnbackedTagHits[uTag] = CountNameOccurrences(pData, ulSize, szTag);
	}

	Zenith_FileAccess::FreeFileData(pData);

	ZENITH_ASSERT_TRUE(szInboundTag != nullptr && szInboundTag[0] != '\0',
		"the compiled ZM_SCENE_ROUTE1 row carries no connection targeting "
		"ZM_SCENE_THORNACRE, so ZM_GetRoute1NorthGateSpawnTag() resolved to the empty "
		"string and there is no tag to look for -- fix Source/Data/ZM_WorldSpec.cpp.");

	// Same anti-vacuity guard the Route 1 clause carries, and for the same reason:
	// this test's load-bearing claims are ABSENCES (no trigger, no gym door), and a
	// needle on a string nothing serializes counts zero and passes every one of them.
	ZENITH_ASSERT_TRUE(GameComponentTypeIsRegistered(szZM_TYPE_WARP_TRIGGER)
			&& GameComponentTypeIsRegistered(szZM_TYPE_SPAWN_POINT)
			&& GameComponentTypeIsRegistered(szZM_TYPE_PLAYER_CONTROLLER),
		"one of the component TYPE needles ('%s', '%s', '%s') is not a row of the "
		"shared inventory in Tests/ZM_Tests_ComponentTypeNames.h -- reconcile the "
		"spelling before trusting any count below.",
		szZM_TYPE_WARP_TRIGGER, szZM_TYPE_SPAWN_POINT, szZM_TYPE_PLAYER_CONTROLLER);

	ZENITH_ASSERT_EQ(uArrivalHits, 1u,
		"the committed Thornacre.zscen must carry the arrival marker '%s' exactly "
		"once -- it is the only place a player walking north off Route 1 can land.",
		szZM_THORNACRE_SOUTH_ARRIVAL_ENTITY_NAME);
	ZENITH_ASSERT_EQ(uCameraHits, 1u,
		"the committed Thornacre.zscen must carry the follow camera '%s' exactly once.",
		szZM_THORNACRE_CAMERA_ENTITY_NAME);

	// ★ A PLAIN EQUALITY, DELIBERATELY, AND THE HEADER EARNED IT. The marker is
	// named "ThornacreSouthArrival" rather than "ThornacreFromRoute1Spawn" precisely
	// so its name does not CONTAIN its own tag -- which is what lets this be
	// `== 1` instead of the strictly-more clause Dawnmere's FromRoute1Spawn needs.
	ZENITH_ASSERT_EQ(uInboundTagHits, 1u,
		"the committed Thornacre.zscen carries the inbound spawn tag '%s' %u time(s), "
		"not exactly once. A ZERO means the marker exists but was never TAGGED -- "
		"spawn resolution matches on the tag, not the entity name, so the Route 1 "
		"north gate would validate and then stall in WAITING_FOR_SPAWN (ZM-D-200). "
		"This is a plain equality because no Thornacre entity name contains this tag.",
		szInboundTag, uInboundTagHits);

	ZENITH_ASSERT_EQ(uSpawnPointHits, 1u,
		"the committed Thornacre.zscen serializes %u ZM_SpawnPoint component(s), not "
		"the 1 it authors -- a marker entity with no ZM_SpawnPoint is a bare transform "
		"no warp can resolve.", uSpawnPointHits);

	ZENITH_ASSERT_EQ(uPlayerControllerHits, 1u,
		"the committed Thornacre.zscen serializes %u ZM_PlayerController component(s), "
		"not exactly 1. The follow camera acquires the UNIQUE controller in its own "
		"scene, so zero or two leave it with no target.", uPlayerControllerHits);
	ZENITH_ASSERT_GT(uPlayerNameHits, uPlayerControllerHits,
		"the committed Thornacre.zscen contains '%s' %u time(s) and the type name "
		"'%s' %u time(s). The entity name is a strict substring of the type name, so "
		"equality means the player ENTITY was never authored.",
		szZM_THORNACRE_PLAYER_ENTITY_NAME, uPlayerNameHits,
		szZM_TYPE_PLAYER_CONTROLLER, uPlayerControllerHits);

	// ★★ EXACTLY ONE TRIGGER -- the return gate, and NOT a gym door. The name clause
	// below cannot make that second half of the claim: a gym door would be spelled
	// fresh at the authoring site (ZM_ThornacrePlacement.h names no gym entity, by
	// ruling), so no needle here would know its name. The component-type COUNT is
	// what closes it, which is why it is `== 1` and not `>= 1`.
	ZENITH_ASSERT_EQ(uWarpTriggerHits, 1u,
		"the committed Thornacre.zscen serializes %u ZM_WarpTrigger component(s), not "
		"the 1 it authors ('%s'). A ZERO leaves the town a DEAD END -- Route 1's north "
		"gate can bring a player in and nothing can send one out -- and is the state a "
		"source-only R1-3 leaves behind, before a WINDOWED *_True tools boot has "
		"re-authored and re-committed the scene. A TWO means a gym door was authored: "
		"ZM-D-196 rules this town a traversal STUB for this milestone, the compiled "
		"Thornacre->Gym1 edge is deliberately UNBACKED, and a door into a room nobody "
		"has built is a warp that validates and then stalls in WAITING_FOR_SCENE "
		"(ZM-D-200).", uWarpTriggerHits, szZM_THORNACRE_SOUTH_GATE_ENTITY_NAME);
	ZENITH_ASSERT_EQ(uGateHits, 1u,
		"the committed Thornacre.zscen must carry the return gate '%s' exactly once. "
		"It is the ONE entity that makes this stub traversable rather than a room the "
		"player cannot leave.",
		szZM_THORNACRE_SOUTH_GATE_ENTITY_NAME);

	for (u_int uTag = 0u; uTag < uOfferedTagCount; ++uTag)
	{
		const char* szTag = xThornacreRow.m_pszSpawnTags != nullptr
			? xThornacreRow.m_pszSpawnTags[uTag] : "";
		if (szTag == nullptr || szTag[0] == '\0'
			|| std::strcmp(szTag, szInboundTag) == 0)
		{
			continue;
		}
		ZENITH_ASSERT_EQ(uUnbackedTagHits[uTag], 0u,
			"the committed Thornacre.zscen carries the spawn tag '%s' %u time(s) and "
			"must carry it none. Thornacre OFFERS this tag in the compiled world "
			"table, but this milestone backs only the Route 1 arrival -- '%s' is the "
			"gym's return marker, and the gym does not exist. If a later slice backs "
			"it, move it out of this clause deliberately rather than deleting the "
			"clause.", szTag, uUnbackedTagHits[uTag], szTag);
	}
}

// ============================================================================
// R1-3 -- THE FOUR SEAM GATES, NEEDLED BY THEIR WHOLE SERIALIZED PAYLOAD.
//
// ★★ THIS IS THE ONLY CI-VISIBLE CLAIM ABOUT WHICH GATE POINTS WHERE. Every
// other clause in this file counts STRINGS, and the four gates are indexed by a
// NUMBER: Route 1's south and north sensors ask their destinations for the same
// tag ("FromRoute1", both edges of the ZM_SCENE_ROUTE1 row) and differ only in
// the target build index -- 2 for Dawnmere, 3 for Thornacre. Author them the
// wrong way round and every name count, every tag count and every component
// count in this file is bit-identical, while the shipped world folds in half:
// walking south off Route 1 lands you in Thornacre, whose return gate puts you
// back at Route 1's NORTH marker, and no error is ever raised.
//
// ★ SO THE NEEDLE IS THE WHOLE PAYLOAD, per gate, ==1 each:
//     [u_int version][u_int targetBuildIndex][32-byte zero-padded tag]
// built from ZM_WarpTrigger's own constants and from the SAME resolvers the
// authoring calls (Source/World/ZM_{Route1,Thornacre,Dawnmere}Placement.h), so a
// re-pointed edge in Source/Data/ZM_WorldSpec.cpp moves the authoring and this
// needle together and a needle that quietly disagreed is not expressible.
//
// ★★ AND A COUNT ALONE STILL CANNOT SEE THE SWAP -- WHICH IS WHY THE ORDERING
// CLAUSE IS HERE. Swap Route 1's two gates and the FILE still contains each
// payload exactly once; what moves is WHICH ENTITY RECORD each sits in. A .zscen
// entity record is `[fileIndex][name][parentFileIndex]` followed by that
// entity's component payloads, written one entity at a time in dense authoring
// order (ZM-D-148, Zenith_Entity::WriteToDataStream), so a gate's payload always
// lies between its OWN name and the NEXT entity's name. The south gate is
// authored first, so the four offsets must interleave
//     southName < southPayload < northName < northPayload
// and a swap makes that false. No container parsing, no byte offsets baked in,
// nothing that rots at the next schema bump -- only relative order.
//
// ★ THE LIVE PROOF IS ZM_SeamRoundTrip_Test, which walks all four gates and
// asserts where each one lands. That one is definitive and this one is not; but
// it needs the GITIGNORED terrain bakes and therefore RequestSkips on CI -- and
// a skip counts as a PASS. This unit reads TRACKED files with no GPU and no
// terrain, so it is what actually runs in the gate.
// ============================================================================
ZENITH_TEST(ZM_CommittedSceneBytes, EverySeamGatePayloadIsAuthoredExactlyOnceInItsOwnScene)
{
	// ---- The four needles, all DERIVED from the compiled world table --------
	unsigned char auDawnmereNorth[ulZM_WARP_PAYLOAD_SIZE] = {};
	unsigned char auRoute1South[ulZM_WARP_PAYLOAD_SIZE] = {};
	unsigned char auRoute1North[ulZM_WARP_PAYLOAD_SIZE] = {};
	unsigned char auThornacreReturn[ulZM_WARP_PAYLOAD_SIZE] = {};

	const bool bDawnmereNorthBuilt = BuildWarpPayloadNeedle(
		ZM_GetDawnmereNorthGateTargetBuildIndex(),
		ZM_GetDawnmereNorthGateSpawnTag(), auDawnmereNorth);
	const bool bRoute1SouthBuilt = BuildWarpPayloadNeedle(
		ZM_GetRoute1SouthGateTargetBuildIndex(),
		ZM_GetRoute1SouthGateSpawnTag(), auRoute1South);
	const bool bRoute1NorthBuilt = BuildWarpPayloadNeedle(
		ZM_GetRoute1NorthGateTargetBuildIndex(),
		ZM_GetRoute1NorthGateSpawnTag(), auRoute1North);
	const bool bThornacreReturnBuilt = BuildWarpPayloadNeedle(
		ZM_GetThornacreReturnTargetBuildIndex(),
		ZM_GetThornacreReturnSpawnTag(), auThornacreReturn);

	Zenith_Log(LOG_CATEGORY_GAMEPLAY,
		"[ZM_CommittedSceneBytes] seam gate payloads (%llu bytes each): "
		"Dawnmere->%u '%s' built=%d | Route1 south->%u '%s' built=%d | "
		"Route1 north->%u '%s' built=%d | Thornacre->%u '%s' built=%d",
		(unsigned long long)ulZM_WARP_PAYLOAD_SIZE,
		ZM_GetDawnmereNorthGateTargetBuildIndex(),
		ZM_GetDawnmereNorthGateSpawnTag(), (int)bDawnmereNorthBuilt,
		ZM_GetRoute1SouthGateTargetBuildIndex(),
		ZM_GetRoute1SouthGateSpawnTag(), (int)bRoute1SouthBuilt,
		ZM_GetRoute1NorthGateTargetBuildIndex(),
		ZM_GetRoute1NorthGateSpawnTag(), (int)bRoute1NorthBuilt,
		ZM_GetThornacreReturnTargetBuildIndex(),
		ZM_GetThornacreReturnSpawnTag(), (int)bThornacreReturnBuilt);

	// ★ ANTI-VACUITY FIRST, AND IT IS NOT DECORATION. A needle that could not be
	// built is all zeros, and a run of zeros DOES occur in a .zscen -- so an
	// unbuilt needle would not merely count zero, it could count many. Every
	// clause below would then fail for a reason its own message does not name.
	ZENITH_ASSERT_TRUE(bDawnmereNorthBuilt && bRoute1SouthBuilt
			&& bRoute1NorthBuilt && bThornacreReturnBuilt,
		"one of the four seam gate payloads could not be built from the compiled "
		"world table (built: Dawnmere=%d Route1South=%d Route1North=%d "
		"Thornacre=%d). That means an edge is missing from Source/Data/ZM_WorldSpec.cpp "
		"or carries an empty/over-long spawn tag -- fix the TABLE before reading "
		"anything into the counts below.",
		(int)bDawnmereNorthBuilt, (int)bRoute1SouthBuilt,
		(int)bRoute1NorthBuilt, (int)bThornacreReturnBuilt);
	if (!bDawnmereNorthBuilt || !bRoute1SouthBuilt || !bRoute1NorthBuilt
		|| !bThornacreReturnBuilt)
	{
		return;   // already FAILED above; nothing further is meaningful
	}

	// ★★ THE SECOND ANTI-VACUITY GUARD, and it is the one this unit is FOR. If
	// Route 1's two gates ever resolved to the same payload, the two `== 1`
	// clauses below would become one `== 2` claim and the ordering clause would
	// be satisfied by either assignment -- i.e. this unit would stop being able
	// to see the swap, silently, while still passing.
	ZENITH_ASSERT_TRUE(
		std::memcmp(auRoute1South, auRoute1North, sizeof(auRoute1South)) != 0,
		"Route 1's south and north gate payloads are IDENTICAL (both target build "
		"index %u with tag '%s'). The two edges of the ZM_SCENE_ROUTE1 row must "
		"point at different scenes; while they do not, nothing in this file can "
		"tell the two gates apart and the swap this unit exists to catch is "
		"undetectable.",
		ZM_GetRoute1SouthGateTargetBuildIndex(), ZM_GetRoute1SouthGateSpawnTag());

	// ---- (1) DAWNMERE: the outbound north gate ------------------------------
	{
		uint64_t ulSize = 0;
		char* pData = Zenith_FileAccess::ReadFile(
			szZM_COMMITTED_DAWNMERE_SCENE, ulSize);
		ZENITH_ASSERT_NOT_NULL(pData,
			"the committed Dawnmere.zscen could not be read ('%s'). It is a TRACKED "
			"asset (ZM-D-148), so this is a DEFECT and not a skip.",
			szZM_COMMITTED_DAWNMERE_SCENE);
		if (pData != nullptr)
		{
			const u_int uHits = CountOccurrences(
				pData, ulSize, auDawnmereNorth, ulZM_WARP_PAYLOAD_SIZE);
			Zenith_FileAccess::FreeFileData(pData);
			ZENITH_ASSERT_EQ(uHits, 1u,
				"the committed Dawnmere.zscen carries the north gate's configured "
				"payload (target build %u, tag '%s') %u time(s), not exactly once. A "
				"ZERO with '%s' PRESENT as an entity name means the sensor exists but "
				"was never configured -- ZM_WarpTrigger::Configure returned false, so "
				"the entity is a static box that warps nobody. A zero with the name "
				"absent means the scene has not been re-authored since R1-3.",
				ZM_GetDawnmereNorthGateTargetBuildIndex(),
				ZM_GetDawnmereNorthGateSpawnTag(), uHits,
				szZM_DAWNMERE_NORTH_GATE_ENTITY_NAME);
		}
	}

	// ---- (2) ROUTE 1: both gates, and which is which ------------------------
	{
		const std::string strScenePath = ZM_CommittedScenePath(ZM_SCENE_ROUTE1);
		uint64_t ulSize = 0;
		char* pData = Zenith_FileAccess::ReadFile(strScenePath.c_str(), ulSize);
		ZENITH_ASSERT_NOT_NULL(pData,
			"the committed Route1.zscen could not be read ('%s'). It is a TRACKED "
			"asset (ZM-D-199), so this is a DEFECT and not a skip.",
			strScenePath.c_str());
		if (pData != nullptr)
		{
			const u_int uSouthHits = CountOccurrences(
				pData, ulSize, auRoute1South, ulZM_WARP_PAYLOAD_SIZE);
			const u_int uNorthHits = CountOccurrences(
				pData, ulSize, auRoute1North, ulZM_WARP_PAYLOAD_SIZE);
			const uint64_t ulSouthName = FindFirstNameOffset(
				pData, ulSize, szZM_ROUTE1_SOUTH_GATE_ENTITY_NAME);
			const uint64_t ulNorthName = FindFirstNameOffset(
				pData, ulSize, szZM_ROUTE1_NORTH_GATE_ENTITY_NAME);
			const uint64_t ulSouthPayload = FindFirstOffset(
				pData, ulSize, auRoute1South, ulZM_WARP_PAYLOAD_SIZE);
			const uint64_t ulNorthPayload = FindFirstOffset(
				pData, ulSize, auRoute1North, ulZM_WARP_PAYLOAD_SIZE);
			const uint64_t ulFileSize = ulSize;
			Zenith_FileAccess::FreeFileData(pData);

			Zenith_Log(LOG_CATEGORY_GAMEPLAY,
				"[ZM_CommittedSceneBytes] '%s' %llu bytes; gate payloads S x%u N x%u; "
				"offsets southName=%llu southPayload=%llu northName=%llu "
				"northPayload=%llu",
				strScenePath.c_str(), (unsigned long long)ulFileSize,
				uSouthHits, uNorthHits,
				(unsigned long long)ulSouthName, (unsigned long long)ulSouthPayload,
				(unsigned long long)ulNorthName, (unsigned long long)ulNorthPayload);

			ZENITH_ASSERT_EQ(uSouthHits, 1u,
				"the committed Route1.zscen carries the SOUTH gate's configured payload "
				"(target build %u = Dawnmere, tag '%s') %u time(s), not exactly once.",
				ZM_GetRoute1SouthGateTargetBuildIndex(),
				ZM_GetRoute1SouthGateSpawnTag(), uSouthHits);
			ZENITH_ASSERT_EQ(uNorthHits, 1u,
				"the committed Route1.zscen carries the NORTH gate's configured payload "
				"(target build %u = Thornacre, tag '%s') %u time(s), not exactly once.",
				ZM_GetRoute1NorthGateTargetBuildIndex(),
				ZM_GetRoute1NorthGateSpawnTag(), uNorthHits);

			// ★★ THE SWAP CLAUSE. See the banner: a .zscen entity record puts a
			// component payload after its OWN entity name and before the next
			// entity's, and the south gate is authored first.
			ZENITH_ASSERT_TRUE(
				ulSouthName < ulSouthPayload && ulSouthPayload < ulNorthName
					&& ulNorthName < ulNorthPayload,
				"the two Route 1 gate payloads are not interleaved with their own "
				"entity names: southName=%llu southPayload=%llu northName=%llu "
				"northPayload=%llu (file %llu bytes), and a .zscen entity record puts "
				"a component payload between its own name and the next entity's. The "
				"overwhelmingly likely cause is that the two gates were authored "
				"AGAINST EACH OTHER'S DESTINATIONS -- '%s' pointing at Thornacre and "
				"'%s' at Dawnmere -- which no name, tag or component count in this "
				"file can see, and which ships a route whose south end leads north. "
				"Check ZM_ConfigureRoute1SouthGateTrigger / "
				"ZM_ConfigureRoute1NorthGateTrigger and the ORDER of the two authoring "
				"blocks in Zenithmon.cpp before touching this clause.",
				(unsigned long long)ulSouthName, (unsigned long long)ulSouthPayload,
				(unsigned long long)ulNorthName, (unsigned long long)ulNorthPayload,
				(unsigned long long)ulFileSize,
				szZM_ROUTE1_SOUTH_GATE_ENTITY_NAME,
				szZM_ROUTE1_NORTH_GATE_ENTITY_NAME);
		}
	}

	// ---- (3) THORNACRE: the return gate -------------------------------------
	{
		const std::string strScenePath = ZM_CommittedScenePath(ZM_SCENE_THORNACRE);
		uint64_t ulSize = 0;
		char* pData = Zenith_FileAccess::ReadFile(strScenePath.c_str(), ulSize);
		ZENITH_ASSERT_NOT_NULL(pData,
			"the committed Thornacre.zscen could not be read ('%s'). It is a TRACKED "
			"asset (ZM-D-199), so this is a DEFECT and not a skip.",
			strScenePath.c_str());
		if (pData != nullptr)
		{
			const u_int uHits = CountOccurrences(
				pData, ulSize, auThornacreReturn, ulZM_WARP_PAYLOAD_SIZE);
			Zenith_FileAccess::FreeFileData(pData);
			ZENITH_ASSERT_EQ(uHits, 1u,
				"the committed Thornacre.zscen carries the return gate's configured "
				"payload (target build %u = Route 1, tag '%s') %u time(s), not exactly "
				"once. Note the tag is Route 1's OFFERED '%s', never Thornacre's own "
				"inbound tag -- reaching for the wrong one produces a gate that "
				"validates against the compiled table and then stalls the player in "
				"WAITING_FOR_SPAWN behind an opaque fade (ZM-D-200).",
				ZM_GetThornacreReturnTargetBuildIndex(),
				ZM_GetThornacreReturnSpawnTag(), uHits,
				ZM_GetThornacreReturnSpawnTag());
		}
	}
}

// ============================================================================
// R1-4 (ZM-66/ZM-D-205) -- THE SCENE-ATTACH NEEDLE: ZM_TallGrassSystem is
// authored onto Route 1's terrain entity, and ROUTE 1 ONLY.
//
// ★★ WHY THIS IS THE PROOF THAT MATTERS. ZM-D-204 landed the retuned rate, the
// id-tagged biome table and a headless synthetic-density-map proof that the
// roll SEAM works -- but none of that makes a live boot on Route 1 actually
// roll, because ZM_TallGrassSystem reached no shipped scene's terrain entity
// at all (Status.md structural fact #2). Making that literally true is a
// scene-authoring change, and the obvious live proof (extending
// ZM_TallGrassEncounter_Test's shape to Route 1) would need the GITIGNORED
// Route1 terrain bake and therefore RequestSkip() on a fresh checkout -- and a
// skip counts as a PASS. This unit reads the TRACKED .zscen bytes directly, so
// it is what actually runs in the gate.
//
// ★★ AND IT IS EQUALLY A CLAIM ABOUT WHERE THE COMPONENT MUST NOT BE.
// ZM_QueueTerrainHostEntity (Zenithmon.cpp) authors BOTH Route 1's and
// Thornacre's terrain host entity from ONE shared definition, so the easy
// mistake is adding the AddStep_AddComponent call INSIDE that helper instead
// of at the Route 1 call site -- which would give Thornacre a wild-encounter
// surface too. ZM-D-196 rules Thornacre a TRAVERSAL STUB for this milestone
// (terrain, one arrival marker, a player, a camera, a return trigger,
// deliberately nothing else), so this clause asserts the negative on
// Thornacre exactly as deliberately as it asserts the positive on Route 1.
// Dawnmere authors its terrain host INLINE, a third independent path, and
// gets the same negative assertion.
//
// ★ THE NEEDLE IS THE SERIALIZED COMPONENT TYPE NAME, validated against the
// shared inventory (Tests/ZM_Tests_ComponentTypeNames.h) before use, same as
// every other component-type clause in this file.
//
// ★ THE PREFIX TRAP DOES NOT APPLY HERE, AND THAT WAS CHECKED RATHER THAN
// ASSUMED. "ZM_TallGrassSystem" is not a substring of, nor a superstring of,
// any OTHER row in the shared component-type inventory, and
// ZM_TallGrassSystem::WriteToDataStream serializes nothing but its version
// tag -- no path, no name, no tag field -- so the string cannot arrive in a
// .zscen via any route except the component-type-name record itself. A plain
// equality is therefore safe here, unlike the FromLab/FromLabSpawn or
// Player/ZM_PlayerController pairs elsewhere in this file, which both need
// the strictly-more form.
//
// ★★ RED BY DESIGN UNTIL THE WINDOWED RE-AUTHOR LANDS, exactly like every
// other R1-2/R1-3 needle in this file: the authoring step ships in the same
// commit as this clause, but the committed Route1.zscen bytes only move when
// a WINDOWED Vulkan_*_True tools boot re-writes them -- a headless run may
// CREATE a .zscen but never CHANGE one. That boot MUST carry
// --skip-unit-tests, or this failing unit aborts the boot before scene
// authoring runs and blocks its own fix forever.
// ============================================================================
ZENITH_TEST(ZM_CommittedSceneBytes, Route1CarriesTheTallGrassSystemAndThornacreAndDawnmereDoNot)
{
	ZENITH_ASSERT_TRUE(GameComponentTypeIsRegistered(szZM_TYPE_TALL_GRASS_SYSTEM),
		"the component TYPE needle '%s' is not a row of the shared inventory in "
		"Tests/ZM_Tests_ComponentTypeNames.h -- reconcile the spelling before "
		"trusting any count below, which is taken over a string nothing serializes.",
		szZM_TYPE_TALL_GRASS_SYSTEM);

	// ---- ROUTE 1: exactly once ----------------------------------------------
	{
		const std::string strScenePath = ZM_CommittedScenePath(ZM_SCENE_ROUTE1);
		uint64_t ulSize = 0;
		char* pData = Zenith_FileAccess::ReadFile(strScenePath.c_str(), ulSize);
		ZENITH_ASSERT_NOT_NULL(pData,
			"the committed Route1.zscen could not be read ('%s'). It is a TRACKED "
			"asset (ZM-D-199), so this is a DEFECT and not a skip -- restore the "
			"file or re-author it with a WINDOWED *_True tools boot.",
			strScenePath.c_str());
		if (pData != nullptr)
		{
			const u_int uHits =
				CountNameOccurrences(pData, ulSize, szZM_TYPE_TALL_GRASS_SYSTEM);
			Zenith_Log(LOG_CATEGORY_GAMEPLAY,
				"[ZM_CommittedSceneBytes] '%s' %llu bytes; %s x%u",
				strScenePath.c_str(), (unsigned long long)ulSize,
				szZM_TYPE_TALL_GRASS_SYSTEM, uHits);
			Zenith_FileAccess::FreeFileData(pData);
			ZENITH_ASSERT_EQ(uHits, 1u,
				"the committed Route1.zscen serializes %u '%s' component(s), not "
				"exactly 1. A ZERO is the state a source-only R1-4 scene-attach "
				"leaves behind: the AddStep_AddComponent call is in Zenithmon.cpp "
				"but no WINDOWED *_True tools boot has re-authored and "
				"re-committed the scene, so Route 1's grass still has no producer "
				"and no wild encounter can ever fire. MORE than one means the "
				"component was authored twice (check the Route 1 block was not "
				"accidentally merged with the shared ZM_QueueTerrainHostEntity "
				"helper, which would also serialize it once per every call site).",
				uHits, szZM_TYPE_TALL_GRASS_SYSTEM);
		}
	}

	// ---- THORNACRE: never ----------------------------------------------------
	{
		const std::string strScenePath = ZM_CommittedScenePath(ZM_SCENE_THORNACRE);
		uint64_t ulSize = 0;
		char* pData = Zenith_FileAccess::ReadFile(strScenePath.c_str(), ulSize);
		ZENITH_ASSERT_NOT_NULL(pData,
			"the committed Thornacre.zscen could not be read ('%s'). It is a "
			"TRACKED asset (ZM-D-199), so this is a DEFECT and not a skip.",
			strScenePath.c_str());
		if (pData != nullptr)
		{
			const u_int uHits =
				CountNameOccurrences(pData, ulSize, szZM_TYPE_TALL_GRASS_SYSTEM);
			Zenith_Log(LOG_CATEGORY_GAMEPLAY,
				"[ZM_CommittedSceneBytes] '%s' %llu bytes; %s x%u",
				strScenePath.c_str(), (unsigned long long)ulSize,
				szZM_TYPE_TALL_GRASS_SYSTEM, uHits);
			Zenith_FileAccess::FreeFileData(pData);
			ZENITH_ASSERT_EQ(uHits, 0u,
				"the committed Thornacre.zscen serializes %u '%s' component(s), "
				"not the 0 ZM-D-196 rules for this milestone. Thornacre is a "
				"TRAVERSAL STUB by ruling -- terrain, one arrival marker, a "
				"player, a camera, a return trigger, deliberately nothing else. "
				"A non-zero count almost certainly means the AddStep_AddComponent "
				"call moved INTO the shared ZM_QueueTerrainHostEntity helper, "
				"which authors both regions' terrain host from one definition.",
				uHits, szZM_TYPE_TALL_GRASS_SYSTEM);
		}
	}

	// ---- DAWNMERE: never (untouched by this slice) ---------------------------
	{
		uint64_t ulSize = 0;
		char* pData = Zenith_FileAccess::ReadFile(
			szZM_COMMITTED_DAWNMERE_SCENE, ulSize);
		ZENITH_ASSERT_NOT_NULL(pData,
			"the committed Dawnmere.zscen could not be read ('%s'). It is a "
			"TRACKED asset (ZM-D-148), so this is a DEFECT and not a skip.",
			szZM_COMMITTED_DAWNMERE_SCENE);
		if (pData != nullptr)
		{
			const u_int uHits =
				CountNameOccurrences(pData, ulSize, szZM_TYPE_TALL_GRASS_SYSTEM);
			Zenith_Log(LOG_CATEGORY_GAMEPLAY,
				"[ZM_CommittedSceneBytes] '%s' %llu bytes; %s x%u",
				szZM_COMMITTED_DAWNMERE_SCENE, (unsigned long long)ulSize,
				szZM_TYPE_TALL_GRASS_SYSTEM, uHits);
			Zenith_FileAccess::FreeFileData(pData);
			ZENITH_ASSERT_EQ(uHits, 0u,
				"the committed Dawnmere.zscen serializes %u '%s' component(s). "
				"This ticket (ZM-66/ZM-D-205) authors the component onto Route "
				"1's terrain entity ONLY and does not touch Dawnmere's authoring "
				"block -- a non-zero count means Dawnmere was re-authored "
				"unexpectedly, or the component reached it by some other path.",
				uHits, szZM_TYPE_TALL_GRASS_SYSTEM);
		}
	}
}

// ★ ZM-27 / ZM-D-202: the three ground-item props are IN the committed bytes.
//
// A companion to ZM_GroundItemProp_Test rather than a duplicate of it, and the
// difference is which artefact each reads. That test loads the scene and drives a
// pickup, so it proves the feature WORKS. This one reads the FILE, so it proves
// the props are in the bytes git carries -- which is the claim the ZM-D-179 /
// ZM-D-183 pair exists for, where a scene loaded correctly in the running process
// while the committed bytes had drifted. A `Null_` CI leg runs both; only this one
// can see a Route1.zscen that was re-authored by a boot which had the authoring
// steps removed.
ZENITH_TEST(ZM_CommittedSceneBytes, Route1CarriesTheThreeGroundItemProps)
{
	const std::string strScenePath = ZM_CommittedScenePath(ZM_SCENE_ROUTE1);

	uint64_t ulSize = 0;
	char* pData = Zenith_FileAccess::ReadFile(strScenePath.c_str(), ulSize);

	ZENITH_ASSERT_NOT_NULL(pData,
		"the committed Route1.zscen could not be read ('%s'). It is a TRACKED asset "
		"(ZM-D-199), so this is a DEFECT and not a skip",
		strScenePath.c_str());
	if (pData == nullptr)
	{
		return;
	}

	// Each prop's entity name appears EXACTLY ONCE. Not "at least once": a second
	// occurrence would mean the authoring block ran twice, which is how a scene
	// grows duplicate entities that both answer to the same id -- and taking either
	// would mark the other collected, since the collected set is keyed on the ID and
	// not on the entity.
	//
	// The Prop_ prefix is what keeps these needles from colliding with an NPC name,
	// and no prop name is a substring of another (SouthSalve / LaneCatchorb /
	// NorthSalve), so these are plain equalities rather than the strictly-more form
	// Dawnmere's arrival marker needs.
	const char* const aszPropNames[] =
	{
		szZM_ROUTE1_PROP_SOUTH_SALVE_ENTITY_NAME,
		szZM_ROUTE1_PROP_LANE_CATCHORB_ENTITY_NAME,
		szZM_ROUTE1_PROP_NORTH_SALVE_ENTITY_NAME,
	};
	constexpr u_int uPropNameCount =
		(u_int)(sizeof(aszPropNames) / sizeof(aszPropNames[0]));

	for (u_int u = 0u; u < uPropNameCount; ++u)
	{
		const u_int uHits = CountNameOccurrences(pData, ulSize, aszPropNames[u]);
		ZENITH_ASSERT_EQ(uHits, 1u,
			"the committed Route1.zscen carries the name '%s' %u times, not once. "
			"Zero means the scene has not been re-authored since the ZM-27 props "
			"landed (windowed Vulkan_*_True boot, sceneAuthoring=AUTHOR_DAWNMERE); "
			"more than one means the authoring block ran twice and two entities now "
			"share one save-stable ground-item id",
			aszPropNames[u], uHits);
	}

	// ...and the COMPONENT type name appears once per prop. An entity present with
	// its component stripped is a blockout cube nobody can interact with -- visible,
	// authored, and inert -- which no name needle above would notice.
	const u_int uPropComponentHits =
		CountNameOccurrences(pData, ulSize, "ZM_GroundItemProp");
	ZENITH_ASSERT_EQ(uPropComponentHits, uPropNameCount,
		"the committed Route1.zscen carries %u ZM_GroundItemProp component payloads "
		"for %u authored prop entities. A prop entity whose component was dropped is "
		"a cube standing beside the path that can never be picked up",
		uPropComponentHits, uPropNameCount);

	Zenith_FileAccess::FreeFileData(pData);
}
