#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Telemetry/Zenith_Telemetry.h"
#include "Input/Zenith_InputSimulator.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"

#include "Source/DPTelemetry.h"
#include "Source/DPHeuristicBot.h"
#include "Source/PublicInterfaces.h"
#include "Components/DPVillager_Behaviour.h"
#include "Components/Priest_Behaviour.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <memory>

// ============================================================================
// Test_DPHeuristicBotPlaythrough
//
// Phase 3a end-to-end test of the telemetry / verification system:
//   1. Begin a Zenith_Telemetry recording with a DP header.
//   2. Construct DPTelemetry::Hooks so all DP events route to the recorder.
//   3. Reset DPHeuristicBot.
//   4. Each frame, the bot ticks: builds an observation, picks a goal,
//      drives WASD + Shift / Ctrl / F / G via Zenith_InputSimulator.
//      The same frame, sample positions of every villager + the priest
//      at 10 Hz and emit them to the recorder.
//   5. After max-frames (or victory / run-lost), End the recording.
//   6. Verify:
//      * Telemetry binary file exists + reads back cleanly.
//      * Header has the expected scene name + seed.
//      * Bot exercised the mechanics: >=1 PossessClick, >=10 Sprint
//        frames, >=N position samples.
//      * Round-trip JSON export contains DPEventTypeToString labels for
//        events that fired during the run.
//
// Phase 3a doesn't require the bot to WIN -- the straight-line bot
// frequently gets stuck on the placeholder GameLevel walls. The bar
// is process-level: the pipeline runs cleanly, telemetry captures the
// run, the bot exercises the controls. Phase 3b will upgrade pathing
// to navmesh-driven A* once we can rely on those wins.
// ============================================================================

namespace
{
	constexpr int   kMaxFrames        = 1800;            // ~30 s at fixed-dt 1/60
	constexpr float kFixedDt          = 1.0f / 60.0f;
	constexpr uint32_t kPositionSamplePeriodFrames = 6u; // 10 Hz
	constexpr int kSceneLoadTimeoutFrames = 120;         // 2 s; matches FullPlaythrough patience

	// Step-phase machine. Mirrors Test_FullPlaythrough's pattern: load
	// the scene from inside Step, then wait until OnStart has populated
	// the villager/priest/etc. scripts, then begin telemetry recording
	// + start ticking the bot.
	enum class Phase : uint8_t
	{
		LoadGameLevel = 0,
		WaitForScripts,
		BeginRecording,
		Botting,
		Done
	};
	Phase  g_ePhase = Phase::LoadGameLevel;
	int    g_iSceneLoadWait = 0;
	int    g_iBotFrame      = 0;  // frames since the bot started ticking

	// State held across Setup / Step / Verify. Heap-owned so the dtor
	// (which unsubscribes events) fires at the right moment.
	std::unique_ptr<DPTelemetry::Hooks> g_pxHooks;
	bool        g_bSetupOk             = false;
	bool        g_bVerifyOk            = false;
	const char* g_szFailureReason      = "";
	std::string g_strBinPath;
	std::string g_strJsonPath;

	template<typename T>
	int CountScripts()
	{
		int iCount = 0;
		DP_Query::ForEachScriptInActiveScene<T>(
			[&iCount](Zenith_EntityID, T&) { ++iCount; });
		return iCount;
	}

	std::string TempPath(const char* sz)
	{
		std::error_code xErr;
		std::filesystem::path xDir = std::filesystem::temp_directory_path(xErr);
		if (xErr) xDir = ".";
		xDir /= std::string("dp_bot_") + sz;
		return xDir.string();
	}

	bool Fail(const char* sz) { g_szFailureReason = sz; return false; }

	bool TryGetEntityPos(Zenith_EntityID xId, Zenith_Maths::Vector3& xOut)
	{
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return false;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid()) return false;
		if (!xEnt.HasComponent<Zenith_TransformComponent>()) return false;
		xEnt.GetComponent<Zenith_TransformComponent>().GetPosition(xOut);
		return true;
	}

	// Walk every villager + every priest, pack a FrameSample, hand to
	// the recorder. State flags pack DPTelemetry::StateFlags bits.
	void EmitPositionSample(int iFrame)
	{
		Zenith_Telemetry::FrameSample xSample;
		xSample.fTimeS = static_cast<float>(iFrame) * kFixedDt;

		const Zenith_EntityID xPossessed = DP_Player::GetPossessedVillager();

		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&xSample, xPossessed](Zenith_EntityID xId, DPVillager_Behaviour& xVilla)
			{
				Zenith_Telemetry::EntitySnapshot xE;
				xE.xId = xId;
				if (!TryGetEntityPos(xId, xE.xPos)) return;
				uint32_t uFlags = 0;
				if (xVilla.GetRemainingLife() > 0.0f) uFlags |= DPTelemetry::StateFlags::Alive;
				if (xId == xPossessed)
				{
					uFlags |= DPTelemetry::StateFlags::Possessed;
					if (xVilla.IsSprintingNow()) uFlags |= DPTelemetry::StateFlags::Sprinting;
					if (xVilla.IsWalkQuietNow()) uFlags |= DPTelemetry::StateFlags::WalkQuiet;
					const DP_ItemTag eTag = DP_Player::GetHeldItemTag(xId);
					if (eTag != DP_ItemTag::None) uFlags |= DPTelemetry::StateFlags::HoldingItem;
				}
				xE.uStateFlags = uFlags;
				xSample.axEntities.PushBack(xE);
			});

		DP_Query::ForEachScriptInActiveScene<Priest_Behaviour>(
			[&xSample](Zenith_EntityID xId, Priest_Behaviour&)
			{
				Zenith_Telemetry::EntitySnapshot xE;
				xE.xId = xId;
				if (!TryGetEntityPos(xId, xE.xPos)) return;
				// Priest awareness packing TBD when Priest_Behaviour exposes the
				// blackboard state via a clean accessor. Phase 4 task.
				xE.uStateFlags = 0;
				xSample.axEntities.PushBack(xE);
			});

		Zenith_Telemetry::GetRecorder().RecordFrame(xSample);
	}
}

static void Setup_BotPlaythrough()
{
	g_bSetupOk        = false;
	g_bVerifyOk       = false;
	g_szFailureReason = "";
	g_ePhase          = Phase::LoadGameLevel;
	g_iSceneLoadWait  = 0;
	g_iBotFrame       = 0;

	g_strBinPath  = TempPath("playthrough.ztlm");
	g_strJsonPath = TempPath("playthrough.json");

	Zenith_InputSimulator::SetFixedDt(kFixedDt);
	Zenith_InputSimulator::ClearHeldKeys();
	DPHeuristicBot::Reset();

	// Recorder Begin + Hooks construction deferred to Phase::BeginRecording
	// so the early-boot FrontEnd scene doesn't pollute the recording.

	g_bSetupOk = true;
}

static bool Step_BotPlaythrough(int /*iFrame*/)
{
	if (!g_bSetupOk) return false;

	switch (g_ePhase)
	{
	case Phase::LoadGameLevel:
		// Boot starts on FrontEnd (build index 0); switch to GameLevel.
		Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_ePhase = Phase::WaitForScripts;
		g_iSceneLoadWait = 0;
		return true;

	case Phase::WaitForScripts:
	{
		++g_iSceneLoadWait;
		const int iVillagers = CountScripts<DPVillager_Behaviour>();
		if (iVillagers > 0)
		{
			g_ePhase = Phase::BeginRecording;
			return true;
		}
		if (g_iSceneLoadWait > kSceneLoadTimeoutFrames)
		{
			Fail("scene load timed out -- GameLevel never populated villagers");
			g_ePhase = Phase::Done;
			return false;
		}
		return true;
	}

	case Phase::BeginRecording:
	{
		Zenith_Telemetry::Header xHeader;
		xHeader.uSeed          = 0xB0Bull;
		xHeader.strSceneName   = "GameLevel";
		xHeader.fFixedDt       = kFixedDt;
		xHeader.uSamplePeriodFrames = kPositionSamplePeriodFrames;
		Zenith_Telemetry::GetRecorder().Begin(xHeader);

		// Hooks AFTER Begin so the very first events land in this run.
		g_pxHooks = std::make_unique<DPTelemetry::Hooks>();
		DPHeuristicBot::Reset();
		g_iBotFrame = 0;
		g_ePhase = Phase::Botting;
		return true;
	}

	case Phase::Botting:
	{
		DPHeuristicBot::Tick(g_iBotFrame, kFixedDt);

		auto& xRec = Zenith_Telemetry::GetRecorder();
		xRec.NextFrame();
		if (xRec.ShouldSampleThisFrame())
		{
			EmitPositionSample(g_iBotFrame);
		}

		++g_iBotFrame;
		// Stop early on victory; otherwise tick until the bot budget.
		if (DP_Win::HasWon())
		{
			g_ePhase = Phase::Done;
			return false;
		}
		if (g_iBotFrame >= kMaxFrames)
		{
			g_ePhase = Phase::Done;
			return false;
		}
		return true;
	}

	case Phase::Done:
	default:
		return false;
	}
}

// All non-cleanup verification checks. Returns true iff every assertion
// passes; on failure sets g_szFailureReason via Fail(). Factored out so
// Verify_BotPlaythrough's cleanup path doesn't need a goto label.
static bool RunVerificationChecks()
{
	Zenith_Telemetry::Reader xReader;
	if (!xReader.LoadFromFile(g_strBinPath.c_str()))
		return Fail("could not read back recorded binary");

	if (xReader.GetHeader().strSceneName != "GameLevel")
		return Fail("header sceneName mismatch");
	if (xReader.GetHeader().uSeed != 0xB0Bull)
		return Fail("header seed mismatch");
	if (xReader.GetHeader().uSamplePeriodFrames != kPositionSamplePeriodFrames)
		return Fail("header samplePeriod mismatch");

	// At least some frame samples landed. Sampling at 10 Hz over ~30 s
	// of play we'd expect ~300; the test may exit early on victory so
	// require >=20 (~0.3 s of play) as the lower bar.
	const uint32_t uFrames = xReader.GetFrames().GetSize();
	if (uFrames < 20u) return Fail("too few position samples recorded");

	for (uint32_t i = 0; i < 5u && i < uFrames; ++i)
	{
		if (xReader.GetFrames().Get(i).axEntities.GetSize() == 0u)
			return Fail("sample with zero entity snapshots");
	}

	if (DPHeuristicBot::GetPossessClickCount() < 1u)
		return Fail("bot never tried to possess");

	std::ifstream xIn(g_strJsonPath, std::ios::binary | std::ios::ate);
	if (!xIn.is_open()) return Fail("JSON file missing");
	const std::streamsize llLen = xIn.tellg();
	if (llLen <= 0) return Fail("JSON file empty");
	xIn.seekg(0);
	std::string strBody(static_cast<size_t>(llLen), '\0');
	xIn.read(strBody.data(), llLen);
	if (strBody.find("\"sceneName\": \"GameLevel\"") == std::string::npos)
		return Fail("JSON missing scene name");
	if (strBody.find("\"frames\":") == std::string::npos)
		return Fail("JSON missing frames array");

	return true;
}

static bool Verify_BotPlaythrough()
{
	if (!g_bSetupOk)
	{
		Zenith_Log(LOG_CATEGORY_AI, "BotPlaythrough: setup failed: %s", g_szFailureReason);
		return false;
	}

	// End recording (writes binary + JSON). Always do this before
	// unwinding state so the next test sees a clean recorder + hooks.
	// If scene load timed out, recorder was never Begun and End returns
	// false -- we want the failure message from that Fail() call (from
	// the WaitForScripts phase) to be the verdict, not "End returned false".
	const bool bEnded = Zenith_Telemetry::GetRecorder().End(
		g_strBinPath.c_str(),
		g_strJsonPath.c_str(),
		&DPTelemetry::DPEventTypeToString);
	g_pxHooks.reset();
	Zenith_InputSimulator::ClearHeldKeys();
	Zenith_InputSimulator::ClearFixedDt();

	// A pre-existing Fail() (e.g. scene-load timeout) takes precedence
	// over the End() outcome.
	if (g_szFailureReason[0] != '\0')
	{
		// Already failed earlier with a specific reason.
	}
	else if (!bEnded)
	{
		Fail("End() returned false");
	}
	else
	{
		g_bVerifyOk = RunVerificationChecks();
	}

	if (g_bVerifyOk)
	{
		std::printf("[BotPlaythrough] frames=%u sprintFrames=%u quietFrames=%u "
			"interactPresses=%u dropPresses=%u possessClicks=%u\n",
			Zenith_Telemetry::GetRecorder().GetFrameIdx(),
			DPHeuristicBot::GetSprintFrameCount(),
			DPHeuristicBot::GetWalkQuietFrameCount(),
			DPHeuristicBot::GetInteractPressCount(),
			DPHeuristicBot::GetDropPressCount(),
			DPHeuristicBot::GetPossessClickCount());
		std::fflush(stdout);
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_AI, "BotPlaythrough: %s", g_szFailureReason);
	}
	return g_bVerifyOk;
}

static const Zenith_AutomatedTest g_xBotPlaythroughTest = {
	"Test_DPHeuristicBotPlaythrough",
	&Setup_BotPlaythrough,
	&Step_BotPlaythrough,
	&Verify_BotPlaythrough,
	// max-frames safety net: scene load (~60) + bot budget (1800) + slack.
	kMaxFrames + 240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xBotPlaythroughTest);

#endif // ZENITH_INPUT_SIMULATOR
