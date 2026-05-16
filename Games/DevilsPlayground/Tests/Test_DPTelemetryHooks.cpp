#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Telemetry/Zenith_Telemetry.h"
#include "EntityComponent/Zenith_EventSystem.h"

#include "Source/DPTelemetry.h"
#include "Source/PublicInterfaces.h"
#include "Source/DevilsPlayground_Tags.h"

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

	const Zenith_EntityID xV = {3u, 1u};   // pretend villager
	const Zenith_EntityID xT = {7u, 2u};   // pretend interactable target
	const Zenith_EntityID xI = {9u, 3u};   // pretend item

	// 2) Hooks alive: dispatch every event and assert it landed.
	{
		DPTelemetry::Hooks xHooks;

		auto& xDisp = Zenith_EventDispatcher::Get();
		xDisp.Dispatch(DP_OnItemPickedUp{xV, xI});
		xDisp.Dispatch(DP_OnInteract{xV, xT});
		xDisp.Dispatch(DP_OnInteractionBegin{xV, xT});
		xDisp.Dispatch(DP_OnInteractionEnd{xV, xT});
		xDisp.Dispatch(DP_OnInteractionCancelled{xV, xT});
		xDisp.Dispatch(DP_OnVillagerDied{xV});
		xDisp.Dispatch(DP_OnVictory{});
		xDisp.Dispatch(DP_OnRunLost{DP_RunLostCause::Dawn});
		Zenith_Maths::Vector3 xBellPos(11.5f, 0.0f, 22.5f);
		xDisp.Dispatch(DP_OnBellRing{xV, xI, xBellPos});

		// 3) Hooks goes out of scope here -> unsubscribe.
	}

	// 4) Post-tear-down: dispatching MORE events must NOT increase the
	// recorder's event count. Otherwise Hooks leaked a captured lambda
	// and the recorder would crash if invoked after dispatcher unsub.
	const uint32_t uNBeforePostDispatch = xRec.GetEvents().GetSize();
	{
		auto& xDisp = Zenith_EventDispatcher::Get();
		xDisp.Dispatch(DP_OnItemPickedUp{xV, xI});
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

	// 6) Read back and assert: 9 events in dispatch order.
	Zenith_Telemetry::Reader xReader;
	if (!xReader.LoadFromFile(strBin.c_str())) { Fail("hooks: reader load failed"); return; }

	const auto& axE = xReader.GetEvents();
	if (axE.GetSize() != 9u) { Fail("hooks: expected 9 events"); return; }

	using DPE = DPTelemetry::DPEventType;

	auto Check = [&](uint32_t i, DPE eExpected, const char* szWhich) -> bool
	{
		if (axE.Get(i).uEventType != static_cast<uint16_t>(eExpected))
		{
			Fail(szWhich);
			return false;
		}
		return true;
	};

	if (!Check(0, DPE::ItemPickup,        "hooks: event 0 != ItemPickup")) return;
	if (!Check(1, DPE::Interact,          "hooks: event 1 != Interact")) return;
	if (!Check(2, DPE::InteractionBegin,  "hooks: event 2 != InteractionBegin")) return;
	if (!Check(3, DPE::InteractionEnd,    "hooks: event 3 != InteractionEnd")) return;
	if (!Check(4, DPE::InteractionCancel, "hooks: event 4 != InteractionCancel")) return;
	if (!Check(5, DPE::VillagerDied,      "hooks: event 5 != VillagerDied")) return;
	if (!Check(6, DPE::Victory,           "hooks: event 6 != Victory")) return;
	if (!Check(7, DPE::RunLost,           "hooks: event 7 != RunLost")) return;
	if (!Check(8, DPE::BellRing,          "hooks: event 8 != BellRing")) return;

	// Payload spot-checks.
	const auto& xPickup = axE.Get(0);
	if (xPickup.xPayload.xEntityA.m_uIndex != xV.m_uIndex) { Fail("pickup: villager entityA mismatch"); return; }
	if (xPickup.xPayload.xEntityB.m_uIndex != xI.m_uIndex) { Fail("pickup: item entityB mismatch"); return; }

	const auto& xRunLost = axE.Get(7);
	if (xRunLost.xPayload.aiInts[0] != static_cast<int32_t>(DP_RunLostCause::Dawn))
	{
		Fail("runlost: ints[0] != Dawn cause");
		return;
	}

	const auto& xBell = axE.Get(8);
	if (xBell.xPayload.afFloats[0] < 11.4f || xBell.xPayload.afFloats[0] > 11.6f)
	{
		Fail("bell: floats[0] (pos.x) didn't round-trip");
		return;
	}
	if (xBell.xPayload.afFloats[2] < 22.4f || xBell.xPayload.afFloats[2] > 22.6f)
	{
		Fail("bell: floats[2] (pos.z) didn't round-trip");
		return;
	}

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
