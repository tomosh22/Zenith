#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Telemetry/Zenith_Telemetry.h"
#include "EntityComponent/Zenith_EventSystem.h"

#include "Source/DPTelemetry.h"
#include "Source/PublicInterfaces.h"
#include "Source/DevilsPlayground_Tags.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>

// ============================================================================
// Test_DPTelemetryHooks
//
// Phase 2 of the verification system (2026-05-16).
//
// Pins the wiring contract for DPTelemetry::Hooks: when constructed, it
// subscribes to every DP-side event listed in DPEventType; when an event
// is dispatched, a matching Zenith_Telemetry::Event lands in the
// recorder with the documented payload conventions; when Hooks
// destructs, all subscriptions unsubscribe (asserted by dispatching
// after Hooks has been torn down -- the recorder should NOT receive
// further events).
//
// This is a pure unit-style test:
//   - No scene authoring, no behaviours, no entities created in the world.
//   - Hand-built EntityIDs (any uIndex/uGeneration values; the test
//     never dereferences them, only round-trips the bytes).
//   - DP_Items::GetItemTag falls back to DP_ItemTag::None for unknown
//     entity IDs (matches the namespace's "missing -> None" contract)
//     so the ItemPickup ints[0] will be 0 here -- which is what we
//     assert. The real game stores the tag via DPItemBase OnAwake;
//     coverage for that is in Test_ItemPickup.
// ============================================================================

namespace
{
	bool g_bPassed = false;
	const char* g_szFailureReason = "";

	bool Fail(const char* sz) { g_szFailureReason = sz; return false; }

	std::string TempPath(const char* sz)
	{
		std::error_code xErr;
		std::filesystem::path xDir = std::filesystem::temp_directory_path(xErr);
		if (xErr) xDir = ".";
		xDir /= std::string("dp_telemetry_hooks_") + sz;
		return xDir.string();
	}
}

// Test-data constants shared between the dispatch phase + the
// post-recording assertions. Pulling them up here keeps the assertion
// values in lock-step with the dispatch values (the original
// inline-literal form re-typed `5u` and `3` and `11.5f` in two places,
// any drift between them would have produced spurious failures).
namespace
{
	constexpr Zenith_EntityID kEntVillager       = {3u, 1u};
	constexpr Zenith_EntityID kEntTarget         = {7u, 2u};
	constexpr Zenith_EntityID kEntItem           = {9u, 3u};
	constexpr Zenith_EntityID kEntPriorVillager  = {5u, 1u};

	constexpr float kBellPosX = 11.5f;
	constexpr float kBellPosY =  0.0f;
	constexpr float kBellPosZ = 22.5f;

	// Bit index that DP_OnObjectivePlaced carries in its payload --
	// arbitrary but specific so the round-trip check has something to
	// compare against. 3 -> Objective4 (per DP_ObjectiveTagToBit).
	constexpr int   kObjectiveBitIndex = 3;
}

static void Setup_TelemetryHooks()
{
	g_bPassed = false;
	g_szFailureReason = "";

	const std::string strBin = TempPath("run.ztlm");
	const std::string strJson = TempPath("run.json");

	// 1) Begin a fresh recording.
	auto& xRec = Zenith_Telemetry::GetRecorder();
	Zenith_Telemetry::Header xHeader;
	xHeader.strSceneName = "TelemetryHooksTest";
	xHeader.uSeed = 0xABCDu;
	xRec.Begin(xHeader);

	// 2) Hooks alive: dispatch every event and assert it landed.
	{
		DPTelemetry::Hooks xHooks;

		auto& xDisp = Zenith_EventDispatcher::Get();
		xDisp.Dispatch(DP_OnItemPickedUp{kEntVillager, kEntItem});
		xDisp.Dispatch(DP_OnInteract{kEntVillager, kEntTarget});
		xDisp.Dispatch(DP_OnInteractionBegin{kEntVillager, kEntTarget});
		xDisp.Dispatch(DP_OnInteractionEnd{kEntVillager, kEntTarget});
		xDisp.Dispatch(DP_OnInteractionCancelled{kEntVillager, kEntTarget});
		xDisp.Dispatch(DP_OnVillagerDied{kEntVillager});
		xDisp.Dispatch(DP_OnVictory{});
		xDisp.Dispatch(DP_OnRunLost{DP_RunLostCause::Dawn});
		Zenith_Maths::Vector3 xBellPos(kBellPosX, kBellPosY, kBellPosZ);
		xDisp.Dispatch(DP_OnBellRing{kEntVillager, kEntItem, xBellPos});

		// Phase-5-audit (2026-05-16) granular events. Each fires its
		// own DPEventType. The PossessionChanged path emits exactly
		// one PossessionChanged event per dispatch (old + new
		// villager IDs in the payload distinguish possess vs
		// un-possess vs voluntary-switch downstream).
		xDisp.Dispatch(DP_OnPossessionChanged{kEntPriorVillager, kEntVillager});  // start possess
		xDisp.Dispatch(DP_OnPossessionChanged{kEntVillager,      Zenith_EntityID{}});  // un-possess (death)
		xDisp.Dispatch(DP_OnDoorOpened{kEntVillager, kEntTarget});
		xDisp.Dispatch(DP_OnChestOpened{kEntVillager, kEntTarget});
		xDisp.Dispatch(DP_OnForgeCrafted{kEntVillager, kEntTarget, kEntItem});
		xDisp.Dispatch(DP_OnObjectivePlaced{kEntVillager, kEntTarget, kObjectiveBitIndex});

		// 3) Hooks goes out of scope here -> unsubscribe.
	}

	// 4) Post-tear-down: dispatching MORE events must NOT increase the
	// recorder's event count. Otherwise Hooks leaked a captured lambda
	// and the recorder would crash if invoked after dispatcher unsub.
	const uint32_t uNBeforePostDispatch = xRec.GetEvents().GetSize();
	{
		auto& xDisp = Zenith_EventDispatcher::Get();
		xDisp.Dispatch(DP_OnItemPickedUp{kEntVillager, kEntItem});
		xDisp.Dispatch(DP_OnVictory{});
	}
	const uint32_t uNAfterPostDispatch = xRec.GetEvents().GetSize();
	if (uNAfterPostDispatch != uNBeforePostDispatch)
	{
		Fail("hooks: post-tear-down event leaked into recorder -- subscription not removed");
		xRec.End(strBin.c_str(), nullptr, nullptr);
		return;
	}

	// 5) Flush + JSON export so the name resolver path is exercised.
	if (!xRec.End(strBin.c_str(), strJson.c_str(), &DPTelemetry::DPEventTypeToString))
	{
		Fail("hooks: End() returned false");
		return;
	}

	// 6) Read back and assert: dispatch order.
	// Each entry in kExpectedDispatch is the event type the hook
	// system should have produced from the corresponding dispatch
	// call above, in order. Comparing against a table (rather than
	// hand-numbered Check(0, ...), Check(1, ...) calls) means
	// inserting a new event type only needs one new row here -- no
	// renumbering, no stale spot-check indices.
	using DPE = DPTelemetry::DPEventType;
	static constexpr DPE kExpectedDispatch[] = {
		DPE::ItemPickup,
		DPE::Interact,
		DPE::InteractionBegin,
		DPE::InteractionEnd,
		DPE::InteractionCancel,
		DPE::VillagerDied,
		DPE::Victory,
		DPE::RunLost,
		DPE::BellRing,
		DPE::PossessionChanged,   // start possess
		DPE::PossessionChanged,   // un-possess (death)
		DPE::DoorOpened,
		DPE::ChestOpened,
		DPE::ForgeCrafted,
		DPE::ObjectivePlaced,
	};
	constexpr uint32_t kExpectedDispatchCount =
		static_cast<uint32_t>(sizeof(kExpectedDispatch) / sizeof(kExpectedDispatch[0]));

	Zenith_Telemetry::Reader xReader;
	if (!xReader.LoadFromFile(strBin.c_str())) { Fail("hooks: reader load failed"); return; }

	const auto& axE = xReader.GetEvents();
	if (axE.GetSize() != kExpectedDispatchCount)
	{
		static char sBuf[96];
		std::snprintf(sBuf, sizeof(sBuf), "hooks: expected %u events, got %u",
			static_cast<unsigned>(kExpectedDispatchCount),
			static_cast<unsigned>(axE.GetSize()));
		Fail(sBuf);
		return;
	}

	for (uint32_t i = 0; i < kExpectedDispatchCount; ++i)
	{
		if (axE.Get(i).uEventType != static_cast<uint16_t>(kExpectedDispatch[i]))
		{
			static char sBuf[128];
			std::snprintf(sBuf, sizeof(sBuf),
				"hooks: event %u expected type %u, got %u",
				static_cast<unsigned>(i),
				static_cast<unsigned>(kExpectedDispatch[i]),
				static_cast<unsigned>(axE.Get(i).uEventType));
			Fail(sBuf);
			return;
		}
	}

	// Payload spot-checks resolve events by TYPE (find-first-of-type)
	// so the assertions stay correct even if the dispatch order shifts.
	// nullptr return = type not in recording (should never happen if
	// the per-index loop above passed; defensive guard for future
	// refactors).
	auto FindFirstOfType = [&axE](DPE eType) -> const Zenith_Telemetry::Event*
	{
		for (uint32_t i = 0; i < axE.GetSize(); ++i)
		{
			if (axE.Get(i).uEventType == static_cast<uint16_t>(eType))
			{
				return &axE.Get(i);
			}
		}
		return nullptr;
	};

	const Zenith_Telemetry::Event* pxPickup = FindFirstOfType(DPE::ItemPickup);
	if (pxPickup == nullptr) { Fail("pickup: ItemPickup not found"); return; }
	if (pxPickup->xPayload.xEntityA.m_uIndex != kEntVillager.m_uIndex) { Fail("pickup: villager entityA mismatch"); return; }
	if (pxPickup->xPayload.xEntityB.m_uIndex != kEntItem.m_uIndex)     { Fail("pickup: item entityB mismatch"); return; }

	const Zenith_Telemetry::Event* pxRunLost = FindFirstOfType(DPE::RunLost);
	if (pxRunLost == nullptr) { Fail("runlost: RunLost not found"); return; }
	if (pxRunLost->xPayload.aiInts[0] != static_cast<int32_t>(DP_RunLostCause::Dawn))
	{
		Fail("runlost: ints[0] != Dawn cause");
		return;
	}

	// Float round-trip uses an explicit epsilon (1 cm) -- the binary
	// format stores IEEE-754 float as-is so the tolerance is really
	// just a defence against future format changes that go through
	// any lossy intermediate (e.g. snapshot quantisation).
	constexpr float kBellPosEpsilon = 0.1f;
	const Zenith_Telemetry::Event* pxBell = FindFirstOfType(DPE::BellRing);
	if (pxBell == nullptr) { Fail("bell: BellRing not found"); return; }
	if (std::fabs(pxBell->xPayload.afFloats[0] - kBellPosX) > kBellPosEpsilon)
	{
		Fail("bell: floats[0] (pos.x) didn't round-trip");
		return;
	}
	if (std::fabs(pxBell->xPayload.afFloats[2] - kBellPosZ) > kBellPosEpsilon)
	{
		Fail("bell: floats[2] (pos.z) didn't round-trip");
		return;
	}

	// Phase-5-audit payload spot-checks.
	const Zenith_Telemetry::Event* pxPossChange = FindFirstOfType(DPE::PossessionChanged);
	if (pxPossChange == nullptr) { Fail("possChange: PossessionChanged not found"); return; }
	if (pxPossChange->xPayload.xEntityA.m_uIndex != kEntPriorVillager.m_uIndex) { Fail("possChange: entityA (old) mismatch"); return; }
	if (pxPossChange->xPayload.xEntityB.m_uIndex != kEntVillager.m_uIndex)      { Fail("possChange: entityB (new) mismatch"); return; }

	const Zenith_Telemetry::Event* pxObjPlaced = FindFirstOfType(DPE::ObjectivePlaced);
	if (pxObjPlaced == nullptr) { Fail("objplaced: ObjectivePlaced not found"); return; }
	if (pxObjPlaced->xPayload.aiInts[0] != kObjectiveBitIndex)             { Fail("objplaced: bit index not round-tripped"); return; }
	if (pxObjPlaced->xPayload.xEntityA.m_uIndex != kEntVillager.m_uIndex)  { Fail("objplaced: villager mismatch"); return; }
	if (pxObjPlaced->xPayload.xEntityB.m_uIndex != kEntTarget.m_uIndex)    { Fail("objplaced: pentagram mismatch"); return; }

	// 7) JSON sanity: contains the resolved type names.
	std::ifstream xIn(strJson, std::ios::binary | std::ios::ate);
	if (!xIn.is_open()) { Fail("hooks: json file did not open"); return; }
	const std::streamsize llLen = xIn.tellg();
	if (llLen <= 0) { Fail("hooks: json file empty"); return; }
	xIn.seekg(0);
	std::string strBody(static_cast<size_t>(llLen), '\0');
	xIn.read(strBody.data(), llLen);
	if (strBody.find("\"name\":\"ItemPickup\"") == std::string::npos)
		{ Fail("hooks: json missing ItemPickup name"); return; }
	if (strBody.find("\"name\":\"Victory\"") == std::string::npos)
		{ Fail("hooks: json missing Victory name"); return; }
	if (strBody.find("\"name\":\"BellRing\"") == std::string::npos)
		{ Fail("hooks: json missing BellRing name"); return; }
	if (strBody.find("\"name\":\"PossessionChanged\"") == std::string::npos)
		{ Fail("hooks: json missing PossessionChanged name"); return; }
	if (strBody.find("\"name\":\"DoorOpened\"") == std::string::npos)
		{ Fail("hooks: json missing DoorOpened name"); return; }
	if (strBody.find("\"name\":\"ChestOpened\"") == std::string::npos)
		{ Fail("hooks: json missing ChestOpened name"); return; }
	if (strBody.find("\"name\":\"ForgeCrafted\"") == std::string::npos)
		{ Fail("hooks: json missing ForgeCrafted name"); return; }
	if (strBody.find("\"name\":\"ObjectivePlaced\"") == std::string::npos)
		{ Fail("hooks: json missing ObjectivePlaced name"); return; }

	// 8) Spot-check the name resolver directly.
	if (DPTelemetry::DPEventTypeToString(0) == nullptr ||
	    std::strcmp(DPTelemetry::DPEventTypeToString(0), "None") != 0)
		{ Fail("name-resolver: None case wrong"); return; }
	if (DPTelemetry::DPEventTypeToString(static_cast<uint16_t>(DPE::PriestStateChange)) == nullptr ||
	    std::strcmp(DPTelemetry::DPEventTypeToString(
	        static_cast<uint16_t>(DPE::PriestStateChange)), "PriestStateChange") != 0)
		{ Fail("name-resolver: PriestStateChange case wrong"); return; }
	if (DPTelemetry::DPEventTypeToString(0xFFFFu) != nullptr)
		{ Fail("name-resolver: unknown id should return nullptr"); return; }

	g_bPassed = true;
	std::printf("[DPTelemetryHooks] all 9 event types + post-tear-down + name resolver OK\n");
	std::fflush(stdout);
}

static bool Step_TelemetryHooks(int /*iFrame*/)
{
	return false;
}

static bool Verify_TelemetryHooks()
{
	if (!g_bPassed)
	{
		Zenith_Log(LOG_CATEGORY_AI, "DPTelemetryHooks: %s", g_szFailureReason);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xDPTelemetryHooksTest = {
	"Test_DPTelemetryHooks",
	&Setup_TelemetryHooks,
	&Step_TelemetryHooks,
	&Verify_TelemetryHooks,
	30
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xDPTelemetryHooksTest);

#endif // ZENITH_INPUT_SIMULATOR
