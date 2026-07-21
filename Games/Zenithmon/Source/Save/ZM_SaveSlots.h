#pragma once

#include "Core/Zenith_Result.h"

#include <cstdint>

// ============================================================================
// ZM_SaveSlots (S7 item 2 SC2) -- the typed SLOT/DISK layer that sits ON TOP of
// the frozen ZMSV codec (Source/Core/ZM_SaveSchema.{h,cpp}, ZM-D-136).
//
// The directory boundary IS the purity boundary: Source/Core/ owns the pure
// payload codec and names no slot, no file and no runtime; Source/Save/ owns
// slots, files and the engine save layer and adds NOTHING to the payload. The
// 824-byte v1 golden pinned by Tests/ZM_Tests_SaveMigration.cpp is untouched by
// everything in this file -- the one piece of framing this layer contributes
// (the length prefix below) sits OUTSIDE the ZMSV blob, inside the ENGINE's
// payload region.
//
// This file names NO ECS type, NO scene, NO UI element and NO component. World-
// position capture, quit-to-title, autosave policy and the save screen are
// later sub-commits and must not leak down here. The single live-state reader
// (ResolveLiveSaveBlocker) is a thin wrapper declared at the bottom, and it
// only ever forwards four bools into the pure predicate above it.
// ============================================================================

struct ZM_GameState;

// Roles are fixed by Docs/SaveFormat.md:42-45: Save0-2 are the MANUAL slots and
// Auto is written ONLY by milestone triggers, never by the manual flow. That is
// a POLICY distinction owned by the caller -- to the storage layer below, AUTO
// is an ordinary slot in every respect.
// APPEND ONLY: a slot's ordinal picks its file name, so reordering renames every
// existing save on disk.
enum ZM_SAVE_SLOT : u_int
{
	ZM_SAVE_SLOT_0 = 0u,
	ZM_SAVE_SLOT_1,
	ZM_SAVE_SLOT_2,
	ZM_SAVE_SLOT_AUTO,

	ZM_SAVE_SLOT_COUNT,
	ZM_SAVE_SLOT_NONE = ZM_SAVE_SLOT_COUNT
};

// What a slot looks like from outside WITHOUT committing to loading it. Three
// states, not a bool: "no file" and "unreadable file" demand OPPOSITE UI, and
// collapsing them is how a New Game silently clobbers a recoverable save.
enum ZM_SAVE_SLOT_STATUS : u_int
{
	ZM_SAVE_SLOT_EMPTY = 0u,   // no file on disk
	ZM_SAVE_SLOT_READY,        // file present AND its ZMSV payload decoded
	ZM_SAVE_SLOT_DAMAGED,      // file present but the outer file OR the payload was rejected

	ZM_SAVE_SLOT_STATUS_COUNT
};

namespace ZM_SaveSlots
{
	// Stamped into the ENGINE header's uGameVersion field. Deliberately its own
	// constant: LoadEx never inspects uGameVersion (Zenith_SaveData.cpp:321 forwards
	// it to the callback and nothing else), and the REAL version gate lives inside
	// the ZMSV payload (ZM_SaveSchema::Read, ZM_SaveSchema.cpp:1114-1120). A second
	// independent version axis here would be a gate with no owner.
	static constexpr uint32_t uGAME_VERSION = 1u;

	// THE ONE piece of framing this layer adds, written INSIDE the engine payload:
	//     [u32 little-endian ZMSV byte length][ZMSV blob]
	// It exists because ZM_SaveSchema::Read demands an EXACT length (any slack is
	// rejected as Header.trailingBytes, ZM_SaveSchema.cpp:1154) and the engine's two
	// Load paths DISAGREE about GetCapacity(): the disk path wraps an exactly-
	// payload-sized buffer (Zenith_SaveData.cpp:317-319, capacity == payload) while
	// the staged-readback path hands the callback a default-constructed OWNING stream
	// whose capacity is the 1024-byte ALLOCATION (Zenith_SaveData.cpp:229-235). The
	// prefix makes both paths byte-identical to the codec.
	// No magic and no version here: ZMSV's own magic + schema version sit 4 bytes
	// later and duplicating either would create two sources of truth.
	// This is framing AROUND the frozen payload -- the 824-byte v1 golden is untouched.
	static constexpr u_int uLENGTH_PREFIX_BYTES = 4u;

	// ---- Slot identity -------------------------------------------------------
	// TOTAL name maps. "" for NONE / out of range, so a bad id can never build a
	// path (Zenith_SaveData::BuildSlotPath does ZERO sanitisation -- it snprintfs the
	// name straight between the save directory and the extension,
	// Zenith_SaveData.cpp:95-98). Every name below is a COMPILE-TIME LITERAL and
	// nothing in this layer ever derives one from player or save data.
	//
	// Every live name gains a "_Test" suffix (the DP_MetaSave::SlotName idiom,
	// DP_MetaSave.cpp:138-146) when EITHER Zenith_CommandLine::IsAutomatedTestRun()
	// is set OR a test has explicitly opted in via SetTestSlotNamesForTests below,
	// because Zenith_SaveData::Save hits REAL disk even in test builds and
	// ClearForTest does NOT delete files (Zenith_SaveData.h:119). Without this, every
	// batched save test would overwrite -- and every DeleteSlot hygiene call would
	// destroy -- the developer's real Save0.zsave.
	//
	// The command-line flag ALONE is not enough, and that is why the explicit opt-in
	// exists: the boot ZENITH_TEST suite runs under `--headless --exit-after-frames
	// 120` (Tools/run_unit_gate.ps1) and under a plain developer launch, NEITHER of
	// which sets IsAutomatedTestRun(); and the one command that does pass
	// --all-automated-tests (`zenith test <Game>`) also passes --skip-unit-tests
	// (Zenith_Engine.cpp:754), so the boot units never run there at all. Keying the
	// redirection on the flag alone therefore means the disk half of this layer has
	// either zero coverage or the player's real saves as its fixture.
	const char* SlotName(ZM_SAVE_SLOT eSlot);           // "Save0" / "Save0_Test" / ...
	const char* SlotShippingName(ZM_SAVE_SLOT eSlot);   // always the unsuffixed name; units only
	const char* SlotDisplayName(ZM_SAVE_SLOT eSlot);    // "Slot 1".."Slot 3" / "Auto"; UI copy
	bool        IsManualSlot(ZM_SAVE_SLOT eSlot);       // Save0-2 true; AUTO and NONE false

	// ---- Storage -------------------------------------------------------------

	// Encode xState with the FROZEN codec, wrap it in the length prefix, and write it
	// through Zenith_SaveData::Save -- then RE-PROBE, because Save returns true
	// UNCONDITIONALLY (Zenith_SaveData.cpp:204; WriteToFile is void and
	// Zenith_FileAccess::WriteFile only logs), so a disk-full or permission failure is
	// otherwise completely invisible. The verify probe is the ONLY evidence this layer
	// has that the save landed, and it must never be "optimised away".
	//   SUCCESS          - written AND the verify probe came back READY
	//   INVALID_ARGUMENT - bad slot id, or ZM_SaveSchema::Write rejected xState
	//   OUT_OF_MEMORY    - the codec could not stage the payload
	//   FILE_NOT_FOUND   - the write claimed success but no file landed on disk
	//   CORRUPT_DATA     - a file landed but the verify probe did not decode it
	// Nothing is written when validation fails: the payload is staged and validated
	// BEFORE Zenith_SaveData::Save is called at all, so a rejected state leaves the
	// slot exactly as it was -- not even a zero-length file appears.
	// TEST COVERAGE NOTE: SUCCESS / INVALID_ARGUMENT / CORRUPT_DATA all have units
	// (Tests/ZM_Tests_SaveSlots.cpp; the CORRUPT_DATA one poisons the verify probe via
	// Zenith_SaveData::SetReadbackForTest, which is what pins the re-probe). The
	// FILE_NOT_FOUND arm is deliberately UNCOVERED and is NOT reachable from a unit:
	// it requires Zenith_SaveData::Save to leave no file, and the only way to make
	// that happen is to make Zenith_FileAccess::WriteFile fail, which trips a
	// Zenith_Assert on the ofstream open (Zenith_Windows_FileAccess.cpp:103) and would
	// kill the whole boot-unit gate rather than fail one unit. It stays because
	// WriteToFile is void and silent, not because it is expected.
	Zenith_Status WriteState(const ZM_GameState& xState, ZM_SAVE_SLOT eSlot);

	// Read eSlot into xOutState. TRANSACTIONAL: on ANY failure xOutState is
	// byte-for-byte unchanged. The codec guarantees exactly that (it decodes into a
	// local candidate and publishes only after every module validates,
	// ZM_SaveSchema.cpp:1125/1162), and this layer must not weaken it by pre-clearing
	// the destination or by publishing a partial decode.
	//   INVALID_ARGUMENT - bad slot id (nothing is read)
	//   FILE_NOT_FOUND / BAD_MAGIC / VERSION_MISMATCH / CORRUPT_DATA / SUCCESS
	Zenith_Status ReadState(ZM_SAVE_SLOT eSlot, ZM_GameState& xOutState);

	// Classify without publishing. Re-reads disk EVERY call and is deliberately
	// UNCACHED: four ~830-byte reads when a slot screen opens is free, and a cache is
	// a stale-state generator the between-tests hook would then have to know about.
	// NEVER writes, NEVER deletes: a DAMAGED slot is reported and left EXACTLY as it
	// is (Docs/SaveFormat.md:318-321). An out-of-range id reports EMPTY -- there is no
	// file for a slot that does not exist, and WriteState rejects it anyway.
	ZM_SAVE_SLOT_STATUS ProbeSlot(ZM_SAVE_SLOT eSlot);

	// "Is there anything on disk at all" -- the FrontEnd Continue VISIBILITY gate.
	// A DAMAGED slot COUNTS AS OCCUPIED: a damaged save must never make Continue
	// vanish and let New Game silently clobber a recoverable file. Loadability is a
	// separate, stricter question answered per-row by the SC5 screen.
	bool AnySlotOccupied();
	// Stricter: at least one slot probes READY.
	bool AnySlotReady();

	// Explicit destruction. Only from a player-confirmed erase or a test teardown.
	// False when the slot id is bad or no file was there to delete, so a UI can never
	// claim it erased something that was not present.
	bool DeleteSlotFile(ZM_SAVE_SLOT eSlot);

	// ---- Save-permission policy (PURE; every live condition is passed in) ----
	// ONE predicate shared by the manual menu path AND the milestone autosave path, so
	// "may the player save right now" can never have two different answers.
	// Fixed precedence, top to bottom as enumerated -- the ZM_ShouldInteract idiom.
	// APPEND ONLY.
	enum ZM_SAVE_BLOCKER : u_int
	{
		ZM_SAVE_BLOCKER_NONE = 0u,
		ZM_SAVE_BLOCKER_NOT_OVERWORLD,      // FrontEnd / the additive battle scene
		ZM_SAVE_BLOCKER_BATTLE,             // a battle transition owns the screen
		ZM_SAVE_BLOCKER_WARP,               // a warp owns the screen
		ZM_SAVE_BLOCKER_PENDING_WHITEOUT,   // the loss latch has not resolved yet

		ZM_SAVE_BLOCKER_COUNT
	};
	// Precedence, first match wins:
	//   1. !bOverworld              -> NOT_OVERWORLD
	//   2. bBattleTransitionActive  -> BATTLE
	//   3. bWarpInProgress          -> WARP
	//   4. bPendingWhiteout         -> PENDING_WHITEOUT
	//   5. otherwise                -> NONE
	ZM_SAVE_BLOCKER ResolveSaveBlocker(bool bOverworld, bool bBattleTransitionActive,
		bool bWarpInProgress, bool bPendingWhiteout);
	// TOTAL: never returns nullptr; anything outside the enumerated range is "UNKNOWN".
	const char*     SaveBlockerName(ZM_SAVE_BLOCKER eBlocker);

	// Live wrapper (thin, deliberately not unit-tested -- the pure form above is):
	// reads ZM_UI_MenuStack::IsActiveSceneOverworld, ZM_BattleTransition::
	// IsTransitionActive, ZM_GameStateManager::IsWarpInProgress and the live
	// m_bPendingWhiteout, then defers to ResolveSaveBlocker. A context with no live
	// game state reports "no pending whiteout" rather than inventing a blocker.
	ZM_SAVE_BLOCKER ResolveLiveSaveBlocker();

#ifdef ZENITH_INPUT_SIMULATOR
	// Delete every slot FILE this build would use. Zenith_SaveData::ClearForTest does
	// NOT touch disk (Zenith_SaveData.h:119), so without this a test's real .zsave
	// leaks into the next test AND into the next process.
	void DeleteAllSlotsForTests();

	// EXPLICIT, test-driven redirection of SlotName() onto the "_Test" aliases, for
	// the window in which a unit touches disk. Owned by the RAII ZM_SlotDiskScope in
	// Tests/ZM_Tests_SaveSlots.cpp: set on entry, cleared on exit, so a disk unit can
	// never address a live save file and a unit that forgets the scope gets the
	// SHIPPING name and is caught by its own hard assertion rather than eating a save.
	//
	// This is a deliberate file-scope mutable static in the .cpp. It DEFAULTS OFF and
	// nothing outside a test ever calls this, so shipping behaviour is byte-for-byte
	// unchanged: SlotName() keeps answering exactly what IsAutomatedTestRun() dictates.
	// NOTE: ZENITH_INPUT_SIMULATOR is defined UNCONDITIONALLY (Zenith.h:255), so this
	// #ifdef documents intent -- it does NOT compile the symbol out of any build we
	// currently produce. The default-off static is what makes shipping safe, not the
	// guard.
	void SetTestSlotNamesForTests(bool bEnable);
#endif
}
