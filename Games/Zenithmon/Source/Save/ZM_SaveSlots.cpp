#include "Zenith.h"

#include "Core/Zenith_CommandLine.h"
#include "Core/Zenith_Engine.h"
#include "Core/Multithreading/Zenith_Multithreading.h"
#include "DataStream/Zenith_DataStream.h"
#include "SaveData/Zenith_SaveData.h"

#include "Zenithmon/Source/Save/ZM_SaveSlots.h"

#include "Zenithmon/Source/Core/ZM_SaveSchema.h"
#include "Zenithmon/Source/Party/ZM_GameState.h"

// The ONE live-state reach in this file, and it is confined to
// ResolveLiveSaveBlocker at the very bottom: everything above is slots, bytes and
// files only.
#include "Zenithmon/Components/ZM_BattleTransition.h"
#include "Zenithmon/Components/ZM_GameStateManager.h"
#include "Zenithmon/Components/ZM_UI_MenuStack.h"

// ============================================================================
// ZM_SaveSlots -- see the header for the contract. This file is the ORDER of
// operations, and two orderings in it are load-bearing:
//
//   1. WriteState STAGES AND VALIDATES the payload BEFORE it calls
//      Zenith_SaveData::Save. Save creates the file the instant it is called,
//      so validating inside the write callback would leave a zero-length file
//      behind for every rejected state.
//   2. WriteState believes the RE-PROBE, never Save's return value, which is
//      the literal constant `true` (Zenith_SaveData.cpp:204).
//
// NOTHING IN THIS FILE MAY Zenith_Assert ON A SLOT ID, A STATE OR A STREAM.
// Zenith.h:138 defines ZENITH_ASSERT unconditionally, so Zenith_Assert breaks
// the process in EVERY configuration, and the whole ZENITH_TEST suite runs at
// boot before the scene loads -- one assert on an input a unit deliberately
// supplies (an out-of-range slot, a truncated prefix, an invalid state) kills
// the process and takes the entire unit gate with it, not just that one unit.
// Every function here is TOTAL: it returns its defined answer and diagnoses
// mis-authored input with a NON-FATAL Zenith_Error. The only asserts in this
// file are the main-thread guards, which no unit can trip -- boot units run on
// the main thread.
// ============================================================================

namespace
{
	// Compile-time literals ONLY. Zenith_SaveData::BuildSlotPath snprintfs the name
	// straight into a path with zero sanitisation (Zenith_SaveData.cpp:95-98), so a
	// name derived from player or save data would be a path-traversal seam. Keeping
	// the maps as literal tables makes that structurally impossible.
	const char* const s_aszShippingNames[ZM_SAVE_SLOT_COUNT] =
	{
		"Save0",
		"Save1",
		"Save2",
		"Auto",
	};

	// The automated-test aliases. Each is its shipping name plus "_Test", which is the
	// prefix relationship the name unit pins.
	const char* const s_aszTestNames[ZM_SAVE_SLOT_COUNT] =
	{
		"Save0_Test",
		"Save1_Test",
		"Save2_Test",
		"Auto_Test",
	};

	// Player-facing copy. One-based for the manual slots because "Slot 0" reads as a
	// programmer artefact; AUTO names its ROLE rather than a number.
	const char* const s_aszDisplayNames[ZM_SAVE_SLOT_COUNT] =
	{
		"Slot 1",
		"Slot 2",
		"Slot 3",
		"Auto",
	};

	static_assert(sizeof(s_aszShippingNames) / sizeof(s_aszShippingNames[0]) == (u_int)ZM_SAVE_SLOT_COUNT,
		"one shipping name per ZM_SAVE_SLOT");
	static_assert(sizeof(s_aszTestNames) / sizeof(s_aszTestNames[0]) == (u_int)ZM_SAVE_SLOT_COUNT,
		"one test name per ZM_SAVE_SLOT");
	static_assert(sizeof(s_aszDisplayNames) / sizeof(s_aszDisplayNames[0]) == (u_int)ZM_SAVE_SLOT_COUNT,
		"one display name per ZM_SAVE_SLOT");

	// Handed back for NONE and for anything past it. An empty name can never build a
	// slot path, and every entry point below refuses to call the engine with one.
	const char* const szNO_SLOT_NAME = "";

	// The explicit test-name opt-in behind ZM_SaveSlots::SetTestSlotNamesForTests.
	//
	// A file-scope mutable static is normally a smell, and this one is a deliberate
	// exception: the boot ZENITH_TEST suite runs under `--headless
	// --exit-after-frames 120` and under a plain developer launch, neither of which
	// sets Zenith_CommandLine::IsAutomatedTestRun(), so keying the "_Test" aliases on
	// that flag alone leaves every disk unit either skipped (zero coverage) or aimed
	// at the player's real Save0/Save1/Save2/Auto files. There is no per-call seam to
	// thread the choice through -- SlotName is a free function consulted from inside
	// ProbeSlot/WriteState/ReadState/DeleteSlotFile -- so the redirection has to be
	// ambient.
	//
	// It DEFAULTS FALSE and only Tests/ZM_Tests_SaveSlots.cpp's RAII ZM_SlotDiskScope
	// ever writes it (true on entry, false on exit). Shipping behaviour is therefore
	// unchanged: with this false, SlotName is exactly the IsAutomatedTestRun()
	// expression it was before.
	bool s_bForceTestSlotNames = false;

	bool ZM_IsRealSlot(ZM_SAVE_SLOT eSlot)
	{
		// ZM_SAVE_SLOT_NONE aliases ZM_SAVE_SLOT_COUNT, so this single comparison
		// rejects the sentinel and every garbage value together.
		return (u_int)eSlot < (u_int)ZM_SAVE_SLOT_COUNT;
	}

	// ---- Engine-callback plumbing -------------------------------------------
	// Both Zenith_SaveData callbacks return void (Zenith_SaveData.h:41,45), so the
	// codec's Zenith_Status has nowhere to go but the userdata block.

	struct WriteUserData
	{
		// The ALREADY-ENCODED, already-validated ZMSV blob. The callback frames it; it
		// never encodes, so it can never be the thing that discovers a bad state.
		const uint8_t* m_pPayloadBytes  = nullptr;
		uint64_t       m_ulPayloadBytes = 0u;
		Zenith_Status  m_xStatus        = Zenith_ErrorCode::CORRUPT_DATA;
	};

	struct ReadUserData
	{
		ZM_GameState* m_pxOut   = nullptr;
		Zenith_Status m_xStatus = Zenith_ErrorCode::CORRUPT_DATA;
	};

	void WritePayloadCallback(Zenith_DataStream& xStream, void* pxUserData)
	{
		WriteUserData* pxUser = static_cast<WriteUserData*>(pxUserData);
		if (pxUser == nullptr) { return; }

		if (pxUser->m_pPayloadBytes == nullptr || pxUser->m_ulPayloadBytes == 0u
			|| pxUser->m_ulPayloadBytes > 0xFFFFFFFFull)
		{
			pxUser->m_xStatus = Zenith_ErrorCode::INVALID_ARGUMENT;
			return;
		}

		const uint64_t ulEntryCursor = xStream.GetCursor();
		const uint32_t uLength = (uint32_t)pxUser->m_ulPayloadBytes;

		// EXPLICIT little-endian, byte by byte -- never a memcpy of a uint32_t and
		// never a struct write. The prefix is a wire field and must not inherit the
		// host's byte order or a compiler's struct padding.
		const uint8_t auPrefix[ZM_SaveSlots::uLENGTH_PREFIX_BYTES] =
		{
			(uint8_t)(uLength & 0xFFu),
			(uint8_t)((uLength >> 8) & 0xFFu),
			(uint8_t)((uLength >> 16) & 0xFFu),
			(uint8_t)((uLength >> 24) & 0xFFu),
		};
		// Prefix FIRST, then the blob: a reader has to know the length before it can
		// bound the payload, and the unit that pins bytes [4..7] == 'Z','M','S','V'
		// exists to keep this order from being flipped.
		xStream.WriteData(auPrefix, ZM_SaveSlots::uLENGTH_PREFIX_BYTES);
		xStream.WriteData(pxUser->m_pPayloadBytes, pxUser->m_ulPayloadBytes);

		const uint64_t ulExpected = ulEntryCursor + (uint64_t)ZM_SaveSlots::uLENGTH_PREFIX_BYTES
			+ pxUser->m_ulPayloadBytes;
		if (xStream.GetCursor() != ulExpected)
		{
			// Zenith_DataStream::WriteData logs and returns on a failed grow rather than
			// signalling, so the cursor is the only evidence a write went short.
			Zenith_Error(LOG_CATEGORY_GAMEPLAY,
				"[ZM SaveSlots] write callback staged %llu of %llu bytes",
				xStream.GetCursor() - ulEntryCursor, ulExpected - ulEntryCursor);
			pxUser->m_xStatus = Zenith_ErrorCode::OUT_OF_MEMORY;
			return;
		}
		pxUser->m_xStatus = true;
	}

	void ReadPayloadCallback(Zenith_DataStream& xStream, uint32_t /*uGameVersion*/,
		void* pxUserData)
	{
		// uGameVersion is deliberately unused: it is engine metadata, and the REAL
		// version gate is the ZMSV schema version the codec checks four bytes into the
		// blob. Two version gates would mean two places to forget to bump.
		ReadUserData* pxUser = static_cast<ReadUserData*>(pxUserData);
		if (pxUser == nullptr) { return; }
		if (pxUser->m_pxOut == nullptr)
		{
			pxUser->m_xStatus = Zenith_ErrorCode::INVALID_ARGUMENT;
			return;
		}

		// This guard is ONLY reachable on the DISK path: LoadEx wraps a stream whose
		// capacity IS the header's declared payload size (Zenith_SaveData.cpp:317-319),
		// so a hand-edited file declaring a 0-3 byte payload with a matching CRC arrives
		// here with fewer than 4 readable bytes. The staged-readback path can never
		// reach it -- it always hands over a default-constructed OWNING stream whose
		// capacity is the 1024-byte allocation (Zenith_SaveData.cpp:229-235). Without
		// this branch, the ReadData below would ask a 2-byte stream for 4 bytes and trip
		// Zenith_DataStream's FATAL bounds assert (Zenith_DataStream.h:154).
		// Covered by Tests/ZM_Tests_SaveSlots.cpp Slot_ReadRejectsADiskPayloadTooSmallForAPrefix.
		const uint64_t ulCursor   = xStream.GetCursor();
		const uint64_t ulCapacity = xStream.GetCapacity();
		if (xStream.GetData() == nullptr || ulCursor > ulCapacity
			|| (ulCapacity - ulCursor) < (uint64_t)ZM_SaveSlots::uLENGTH_PREFIX_BYTES)
		{
			Zenith_Error(LOG_CATEGORY_GAMEPLAY,
				"[ZM SaveSlots] payload is too small to hold the %u-byte length prefix",
				ZM_SaveSlots::uLENGTH_PREFIX_BYTES);
			pxUser->m_xStatus = Zenith_ErrorCode::CORRUPT_DATA;
			return;
		}

		uint8_t auPrefix[ZM_SaveSlots::uLENGTH_PREFIX_BYTES] = {};
		xStream.ReadData(auPrefix, ZM_SaveSlots::uLENGTH_PREFIX_BYTES);
		const uint64_t ulLength = (uint64_t)auPrefix[0]
			| ((uint64_t)auPrefix[1] << 8)
			| ((uint64_t)auPrefix[2] << 16)
			| ((uint64_t)auPrefix[3] << 24);

		// The prefix is UNTRUSTED data from a file an editor may have touched. Bound it
		// against what the stream can actually serve BEFORE handing it to the codec.
		const uint64_t ulRemaining = xStream.GetCapacity() - xStream.GetCursor();
		if (ulLength == 0u || ulLength > ulRemaining)
		{
			Zenith_Error(LOG_CATEGORY_GAMEPLAY,
				"[ZM SaveSlots] length prefix %llu is unusable (%llu bytes remain)",
				ulLength, ulRemaining);
			pxUser->m_xStatus = Zenith_ErrorCode::CORRUPT_DATA;
			return;
		}

		// The prefix goes to the codec VERBATIM. GetCapacity() must NEVER be used as
		// the length here: on the staged-readback path it is the 1024-byte allocation,
		// not the payload (Zenith_SaveData.cpp:229-235), and the codec rejects any
		// slack as Header.trailingBytes. Nor may this branch on OwnsData() -- that
		// couples the game to which engine load path happens to be running.
		// The destination is handed over directly because ZM_SaveSchema::Read is itself
		// transactional: it decodes into a local candidate and assigns only after every
		// module validates (ZM_SaveSchema.cpp:1125/1162).
		pxUser->m_xStatus = ZM_SaveSchema::Read(xStream, ulLength, *pxUser->m_pxOut);
	}
}

const char* ZM_SaveSlots::SlotName(ZM_SAVE_SLOT eSlot)
{
	if (!ZM_IsRealSlot(eSlot)) { return szNO_SLOT_NAME; }
	// EITHER trigger redirects: the command-line flag covers `zenith test <Game>` and
	// the automated harness, the explicit opt-in covers the boot ZENITH_TEST suite,
	// which runs with no flags at all. s_bForceTestSlotNames is false in every
	// non-test process, so this is the original expression outside a test.
	return (Zenith_CommandLine::IsAutomatedTestRun() || s_bForceTestSlotNames)
		? s_aszTestNames[(u_int)eSlot]
		: s_aszShippingNames[(u_int)eSlot];
}

const char* ZM_SaveSlots::SlotShippingName(ZM_SAVE_SLOT eSlot)
{
	if (!ZM_IsRealSlot(eSlot)) { return szNO_SLOT_NAME; }
	return s_aszShippingNames[(u_int)eSlot];
}

const char* ZM_SaveSlots::SlotDisplayName(ZM_SAVE_SLOT eSlot)
{
	if (!ZM_IsRealSlot(eSlot)) { return szNO_SLOT_NAME; }
	return s_aszDisplayNames[(u_int)eSlot];
}

bool ZM_SaveSlots::IsManualSlot(ZM_SAVE_SLOT eSlot)
{
	// Save0-2 only. AUTO is a real slot to the storage layer but is NOT a manual one,
	// and the sentinel is neither.
	return (u_int)eSlot < (u_int)ZM_SAVE_SLOT_AUTO;
}

Zenith_Status ZM_SaveSlots::WriteState(const ZM_GameState& xState, ZM_SAVE_SLOT eSlot)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(),
		"ZM_SaveSlots::WriteState must be called from the main thread");

	const char* szSlot = SlotName(eSlot);
	if (szSlot[0] == '\0')
	{
		Zenith_Error(LOG_CATEGORY_GAMEPLAY,
			"[ZM SaveSlots] WriteState: %u is not a save slot", (u_int)eSlot);
		return Zenith_ErrorCode::INVALID_ARGUMENT;
	}

	// Stage and validate BEFORE anything reaches the disk. ZM_SaveSchema::Write
	// validates the whole state before producing a single byte
	// (ZM_SaveSchema.cpp:1059-1062), so encoding here rather than inside the engine's
	// write callback is exactly what makes "a rejected state never creates a file"
	// true -- Zenith_SaveData::Save writes the file unconditionally once called, even
	// for an empty payload.
	Zenith_DataStream xPayload;
	const Zenith_Status xEncoded = ZM_SaveSchema::Write(xState, xPayload);
	if (!xEncoded.IsOk()) { return xEncoded.Error(); }

	const uint64_t ulPayloadBytes = xPayload.GetCursor();
	if (xPayload.GetData() == nullptr || ulPayloadBytes == 0u
		|| ulPayloadBytes > 0xFFFFFFFFull)
	{
		Zenith_Error(LOG_CATEGORY_GAMEPLAY,
			"[ZM SaveSlots] WriteState: staged payload of %llu bytes cannot be framed",
			ulPayloadBytes);
		return Zenith_ErrorCode::OUT_OF_MEMORY;
	}

	WriteUserData xUser;
	xUser.m_pPayloadBytes  = static_cast<const uint8_t*>(xPayload.GetData());
	xUser.m_ulPayloadBytes = ulPayloadBytes;

	// The return value is DELIBERATELY discarded: Zenith_SaveData::Save returns the
	// literal `true` on every path (Zenith_SaveData.cpp:204) because
	// Zenith_DataStream::WriteToFile is void and Zenith_FileAccess::WriteFile only
	// logs. Believing it would make a disk-full failure report SUCCESS.
	Zenith_SaveData::Save(szSlot, uGAME_VERSION, &WritePayloadCallback, &xUser);
	if (!xUser.m_xStatus.IsOk()) { return xUser.m_xStatus.Error(); }

	// THE verification. This re-read is the only evidence this layer has that the
	// save landed and is loadable; it is load-bearing and must not be removed as a
	// redundant round trip.
	const ZM_SAVE_SLOT_STATUS eStatus = ProbeSlot(eSlot);
	if (eStatus == ZM_SAVE_SLOT_READY) { return true; }
	if (eStatus == ZM_SAVE_SLOT_DAMAGED)
	{
		Zenith_Error(LOG_CATEGORY_GAMEPLAY,
			"[ZM SaveSlots] '%s' was written but the verify read rejected it", szSlot);
		return Zenith_ErrorCode::CORRUPT_DATA;
	}
	Zenith_Error(LOG_CATEGORY_GAMEPLAY,
		"[ZM SaveSlots] '%s' reported a successful write but no file is present", szSlot);
	return Zenith_ErrorCode::FILE_NOT_FOUND;
}

Zenith_Status ZM_SaveSlots::ReadState(ZM_SAVE_SLOT eSlot, ZM_GameState& xOutState)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(),
		"ZM_SaveSlots::ReadState must be called from the main thread");

	const char* szSlot = SlotName(eSlot);
	if (szSlot[0] == '\0')
	{
		Zenith_Error(LOG_CATEGORY_GAMEPLAY,
			"[ZM SaveSlots] ReadState: %u is not a save slot", (u_int)eSlot);
		return Zenith_ErrorCode::INVALID_ARGUMENT;
	}

	// No SlotExists pre-check: LoadEx serves a staged test readback for a slot that
	// has no file at all (Zenith_SaveData.cpp:219-238), and a pre-check would make
	// that path unreachable. LoadEx reports FILE_NOT_FOUND itself.
	ReadUserData xUser;
	xUser.m_pxOut = &xOutState;
	const Zenith_Status xLoad = Zenith_SaveData::LoadEx(szSlot, &ReadPayloadCallback, &xUser);
	if (!xLoad.IsOk()) { return xLoad.Error(); }
	return xUser.m_xStatus;
}

ZM_SAVE_SLOT_STATUS ZM_SaveSlots::ProbeSlot(ZM_SAVE_SLOT eSlot)
{
	// The same main-thread invariant WriteState/ReadState/DeleteSlotFile carry: this
	// is the same engine file I/O, and Zenith_SaveData has no internal locking.
	// AnySlotOccupied/AnySlotReady inherit the guard rather than repeating it --
	// every byte of I/O they perform goes through this function.
	// Safe to assert (this one is FATAL in every configuration, like all
	// Zenith_Asserts): the boot ZENITH_TEST suite runs from
	// Zenith_Engine::InitialiseProject (Zenith_Engine.cpp:765), which is reached from
	// Zenith_Engine::Initialise (:819) long after InitialiseRuntimeServices registered
	// the main thread (:472), and the automated-test between-tests hook
	// (Zenithmon.cpp:1262) is likewise main-thread. No unit can trip it.
	Zenith_Assert(g_xEngine.Threading().IsMainThread(),
		"ZM_SaveSlots::ProbeSlot must be called from the main thread");

	const char* szSlot = SlotName(eSlot);
	if (szSlot[0] == '\0') { return ZM_SAVE_SLOT_EMPTY; }

	if (!Zenith_SaveData::SlotExists(szSlot)) { return ZM_SAVE_SLOT_EMPTY; }

	// Decoded into a SCRATCH state that is thrown away: classifying a slot must never
	// publish anything, and it must never write, delete or "repair" the file it is
	// looking at (Docs/SaveFormat.md:318-321).
	ZM_GameState xScratch;
	ReadUserData xUser;
	xUser.m_pxOut = &xScratch;
	const Zenith_Status xLoad = Zenith_SaveData::LoadEx(szSlot, &ReadPayloadCallback, &xUser);
	if (!xLoad.IsOk()) { return ZM_SAVE_SLOT_DAMAGED; }
	return xUser.m_xStatus.IsOk() ? ZM_SAVE_SLOT_READY : ZM_SAVE_SLOT_DAMAGED;
}

bool ZM_SaveSlots::AnySlotOccupied()
{
	for (u_int uSlot = 0u; uSlot < (u_int)ZM_SAVE_SLOT_COUNT; ++uSlot)
	{
		// DAMAGED counts. A damaged save must never make Continue vanish and let a New
		// Game silently clobber a file the player may still want recovered.
		if (ProbeSlot((ZM_SAVE_SLOT)uSlot) != ZM_SAVE_SLOT_EMPTY) { return true; }
	}
	return false;
}

bool ZM_SaveSlots::AnySlotReady()
{
	for (u_int uSlot = 0u; uSlot < (u_int)ZM_SAVE_SLOT_COUNT; ++uSlot)
	{
		if (ProbeSlot((ZM_SAVE_SLOT)uSlot) == ZM_SAVE_SLOT_READY) { return true; }
	}
	return false;
}

bool ZM_SaveSlots::DeleteSlotFile(ZM_SAVE_SLOT eSlot)
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(),
		"ZM_SaveSlots::DeleteSlotFile must be called from the main thread");

	const char* szSlot = SlotName(eSlot);
	if (szSlot[0] == '\0') { return false; }
	// Zenith_SaveData::DeleteSlot already reports false for an absent file, which is
	// the answer a confirmation UI needs: nothing was erased.
	return Zenith_SaveData::DeleteSlot(szSlot);
}

ZM_SaveSlots::ZM_SAVE_BLOCKER ZM_SaveSlots::ResolveSaveBlocker(bool bOverworld,
	bool bBattleTransitionActive, bool bWarpInProgress, bool bPendingWhiteout)
{
	// The sequence IS the specification: the first blocker that holds is the one
	// reported, so each early-out below must stay exactly where it is.
	if (!bOverworld)             { return ZM_SAVE_BLOCKER_NOT_OVERWORLD; }
	if (bBattleTransitionActive) { return ZM_SAVE_BLOCKER_BATTLE; }
	if (bWarpInProgress)         { return ZM_SAVE_BLOCKER_WARP; }
	if (bPendingWhiteout)        { return ZM_SAVE_BLOCKER_PENDING_WHITEOUT; }
	return ZM_SAVE_BLOCKER_NONE;
}

const char* ZM_SaveSlots::SaveBlockerName(ZM_SAVE_BLOCKER eBlocker)
{
	switch (eBlocker)
	{
	case ZM_SAVE_BLOCKER_NONE:              return "NONE";
	case ZM_SAVE_BLOCKER_NOT_OVERWORLD:     return "NOT_OVERWORLD";
	case ZM_SAVE_BLOCKER_BATTLE:            return "BATTLE";
	case ZM_SAVE_BLOCKER_WARP:              return "WARP";
	case ZM_SAVE_BLOCKER_PENDING_WHITEOUT:  return "PENDING_WHITEOUT";
	// A switch, not a table lookup, precisely so COUNT and anything past it land here
	// instead of reading off the end of an array.
	default:                                return "UNKNOWN";
	}
}

ZM_SaveSlots::ZM_SAVE_BLOCKER ZM_SaveSlots::ResolveLiveSaveBlocker()
{
	// A context with no live manager (headless dispatch, pre-boot) has no whiteout
	// latch to consult, so it reports "not pending" rather than inventing a blocker --
	// the scene gate above it is what refuses to save in such a context anyway.
	ZM_GameState* pxState = nullptr;
	const bool bPendingWhiteout = ZM_GameStateManager::TryGetGameState(pxState)
		&& pxState != nullptr && pxState->m_bPendingWhiteout;

	return ResolveSaveBlocker(
		ZM_UI_MenuStack::IsActiveSceneOverworld(),
		ZM_BattleTransition::IsTransitionActive(),
		ZM_GameStateManager::IsWarpInProgress(),
		bPendingWhiteout);
}

#ifdef ZENITH_INPUT_SIMULATOR
void ZM_SaveSlots::DeleteAllSlotsForTests()
{
	for (u_int uSlot = 0u; uSlot < (u_int)ZM_SAVE_SLOT_COUNT; ++uSlot)
	{
		// The false return (nothing there) is the ordinary case and is not a failure.
		DeleteSlotFile((ZM_SAVE_SLOT)uSlot);
	}
}

void ZM_SaveSlots::SetTestSlotNamesForTests(bool bEnable)
{
	// Deliberately unconditional and un-asserted: a test's RAII scope must be able to
	// clear this on the way out even from a unit that has already recorded failures.
	s_bForceTestSlotNames = bEnable;
}
#endif
