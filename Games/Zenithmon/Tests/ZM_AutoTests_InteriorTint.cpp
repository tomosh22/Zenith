#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

// ============================================================================
// ZM_AutoTests_InteriorTint (ZM-D-176) -- THE ONLY CI-VISIBLE PROOF THAT THE
// WARM PLAYERHOME TINT REACHED ANYTHING.
//
// The ruling was: the player's bedroom must stop reading as the same greybox
// room as Aster's lab, so PlayerHome gets a slight warm tint and ProfLab is left
// exactly as it is. The boot units in Tests/ZM_Tests_PlayerHomeInterior.cpp pin
// the tint CONSTANT (warm, slight, far from the grey) and the seven-block name
// inventory it is keyed on -- but a constant no material ever reads is a number
// that LOOKS like a check. This test is what closes that gap.
//
// TWO ARMS, AND THEY MAKE EACH OTHER NON-VACUOUS:
//   * ARM 1 loads PlayerHome and requires every one of its seven blockout
//     materials to carry the tint;
//   * ARM 2 loads ProfLab and requires every one of ITS seven to be EXACTLY the
//     shipped blockout grey, byte for byte.
// Delete the tint and arms 1+3 red while arm 2 stays green. Paint everything and
// arms 2+3 red while arm 1 stays green. Neither mistake can pass.
//   * ARM 3 then measures the separation BETWEEN THE TWO SAMPLES -- not between
//     the two constants -- which is the clause that literally answers the user's
//     motivation, end to end, through the real material path.
//
// ★ NOT GRAPHICS-REQUIRED, DELIBERATELY, AND THAT IS THE WHOLE POINT. A skip
// counts as a PASS in the headless zm-tests gate, so a graphics-required tint
// test would be silent exactly where it is needed. The seam is the shipped one:
// ZM_GreyboxVisual is file-local to Zenithmon.cpp and cannot be named from a test
// TU, so the scan keys on the MATERIAL NAME -- the identical idiom
// ZM_RivalVesperAuthored_Test uses, which is itself registered
// m_bRequiresGraphics = false and passes headlessly today. That is the evidence
// that ZM_GreyboxVisual::OnStart really does build its model and material on the
// Null backend.
//
// ★ WHAT THIS TEST CANNOT DO. It reads a Zenith_MaterialAsset's base colour, not
// a pixel. A tint set on a material that is never the material drawn, or one the
// tonemapper crushes, would leave this green. That last mile belongs to
// ZM_InteriorTintPixels_Test -- which IS graphics-required and therefore SKIPS
// (i.e. passes) headlessly, and must be run windowed.
//
// ★ AND NOTHING HERE RE-SPELLS A BUILD INDEX OR A COLOUR. The two indices are
// read from the compiled world table and the two expected colours from the
// shipped accessors.
// ============================================================================

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

#include "AssetHandling/Zenith_MaterialAsset.h"                 // GetName / GetBaseColor
#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "Input/Zenith_InputSimulator.h"
#include "Maths/Zenith_Maths.h"
#include "Physics/Zenith_Physics.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"
#include "Zenithmon/Source/Gen/ZM_HumanAppearance.h"
#include "Zenithmon/Source/World/ZM_PlayerHomePlacement.h"
#include "Zenithmon/Source/World/ZM_ProfLabPlacement.h"

namespace
{
	constexpr float fIT_FIXED_DT = 1.0f / 60.0f;

	// Keyed on the MATERIAL NAME, because ZM_GreyboxVisual is file-local to
	// Zenithmon.cpp and unnameable from a test TU. Every blockout body in the game
	// wears a material called this; what differs is the base colour it was given.
	const char* const szIT_GREYBOX_MATERIAL = "ZM_Greybox";

	// The sample cap is slack against the seven blocks each interior authors, and
	// anything DROPPED is counted so a truncated scan reds rather than quietly
	// reporting a clean subset.
	constexpr u_int uIT_MAX_SAMPLED = 32u;

	// The tint is a compiled pure function, so a sampled material colour must
	// match it to float noise, not "approximately".
	constexpr float fIT_COLOUR_TOLERANCE = 1.0e-4f;

	// Per-room budgets. Their sum sits inside the registered cap so a named phase
	// deadline, not the harness backstop, diagnoses an ordinary stall.
	constexpr int iIT_SETTLE_MIN_FRAMES = 8;
	constexpr int iIT_SETTLE_DEADLINE = 240;

	enum ITRoom : u_int
	{
		IT_ROOM_PLAYERHOME,
		IT_ROOM_PROFLAB,
		IT_ROOM_COUNT
	};
	const char* g_aszITRoomNames[IT_ROOM_COUNT] = { "PlayerHome", "ProfLab" };

	enum class ITPhase
	{
		SettlePlayerHome,
		LoadProfLab,
		SettleProfLab,
		Done,
	};

	// ---- Per-room observation ------------------------------------------------
	struct ITRoomSample
	{
		u_int m_uCollected = 0u;
		u_int m_uOverflow = 0u;
		u_int m_uOffExpected = 0u;          // blocks whose colour is not the expected one
		bool m_bSampled = false;
		float m_fWorstError = 0.0f;         // worst per-channel deviation seen
		Zenith_Maths::Vector4 m_xFirstColour = Zenith_Maths::Vector4(0.0f);
	};

	ITPhase g_eITPhase = ITPhase::Done;
	int g_iITPhaseFrames = 0;
	bool g_bITActive = false;
	bool g_bITSkipped = false;
	bool g_bITFailed = false;
	const char* g_szITFailure = "test did not reach verification";
	ITRoomSample g_axITRooms[IT_ROOM_COUNT];

	void ITFail(const char* szReason)
	{
		if (!g_bITFailed)
		{
			g_szITFailure = szReason;
		}
		g_bITFailed = true;
		g_eITPhase = ITPhase::Done;
	}

	u_int ITBuildIndex(ZM_SCENE_ID eScene)
	{
		return ZM_GetWorldSpec(eScene).m_uBuildIndex;
	}

	bool ITSceneFilePresent(const char* szPath)
	{
		std::error_code xError;
		return std::filesystem::is_regular_file(szPath, xError) && !xError
			&& std::filesystem::file_size(szPath, xError) > 0u && !xError;
	}

	// The largest per-channel gap between two colours, alpha included. Reported
	// (not just compared) so a failure says HOW far off the material landed.
	float ITWorstChannelError(
		const Zenith_Maths::Vector4& xA, const Zenith_Maths::Vector4& xB)
	{
		float fWorst = 0.0f;
		const float afA[4] = { xA.x, xA.y, xA.z, xA.w };
		const float afB[4] = { xB.x, xB.y, xB.z, xB.w };
		for (u_int u = 0u; u < 4u; ++u)
		{
			const float fDelta = afA[u] > afB[u] ? afA[u] - afB[u] : afB[u] - afA[u];
			if (!std::isfinite(fDelta))
			{
				return 1.0e9f;   // FAIL CLOSED: garbage can never look like a match
			}
			if (fDelta > fWorst) { fWorst = fDelta; }
		}
		return fWorst;
	}

	// COLLECT INSIDE THE QUERY, JUDGE OUTSIDE IT -- the shipped idiom. Nothing but
	// the material read happens under the iterator.
	//
	// bExact selects the comparison: ProfLab's half is an EQUALITY claim about
	// bytes that must not have moved, so it is compared exactly; PlayerHome's is a
	// claim about a compiled colour reaching a runtime material, so it carries the
	// float-noise tolerance.
	void ITScanActiveScene(
		ITRoom eRoom, const Zenith_Maths::Vector4& xExpected, bool bExact)
	{
		Zenith_Maths::Vector4 axColours[uIT_MAX_SAMPLED] = {};
		u_int uCollected = 0u;
		u_int uOverflow = 0u;

		g_xEngine.Scenes().QueryActiveScene<Zenith_ModelComponent>().ForEach(
			[&](Zenith_EntityID, Zenith_ModelComponent& xModel)
			{
				if (xModel.GetNumMeshes() == 0u)
				{
					return;
				}
				const Zenith_MaterialAsset* pxMaterial = xModel.GetMaterial(0u);
				if (pxMaterial == nullptr
					|| pxMaterial->GetName() != szIT_GREYBOX_MATERIAL)
				{
					return;   // not a blockout body
				}
				if (uCollected >= uIT_MAX_SAMPLED)
				{
					++uOverflow;   // Verify reds on this rather than judging a subset
					return;
				}
				axColours[uCollected] = pxMaterial->GetBaseColor();
				++uCollected;
			});

		ITRoomSample& xRoom = g_axITRooms[eRoom];
		xRoom.m_uCollected = uCollected;
		xRoom.m_uOverflow = uOverflow;
		xRoom.m_uOffExpected = 0u;
		xRoom.m_fWorstError = 0.0f;
		xRoom.m_bSampled = true;
		if (uCollected > 0u)
		{
			xRoom.m_xFirstColour = axColours[0];
		}

		for (u_int u = 0u; u < uCollected; ++u)
		{
			const float fError = ITWorstChannelError(axColours[u], xExpected);
			if (fError > xRoom.m_fWorstError)
			{
				xRoom.m_fWorstError = fError;
			}
			const bool bMatches = bExact
				? (axColours[u].x == xExpected.x && axColours[u].y == xExpected.y
					&& axColours[u].z == xExpected.z && axColours[u].w == xExpected.w)
				: (fError <= fIT_COLOUR_TOLERANCE);
			if (!bMatches)
			{
				++xRoom.m_uOffExpected;
			}
		}
	}

	// A room is ready to sample once physics is simulating (which only happens
	// after the scene's bodies are live) and a few frames have passed so pending
	// OnStarts -- ZM_GreyboxVisual's included -- have been drained.
	bool ITRoomReady(u_int uExpectedBuildIndex)
	{
		return g_xEngine.Scenes().GetSceneInfo(
				g_xEngine.Scenes().GetActiveScene()).m_iBuildIndex
				== static_cast<int>(uExpectedBuildIndex)
			&& g_xEngine.Physics().HasActiveSimulation()
			&& g_iITPhaseFrames >= iIT_SETTLE_MIN_FRAMES;
	}
}

static void Setup_ZMInteriorTint()
{
	g_eITPhase = ITPhase::Done;
	g_iITPhaseFrames = 0;
	g_bITActive = false;
	g_bITSkipped = false;
	g_bITFailed = false;
	g_szITFailure = "test did not reach verification";
	for (u_int u = 0u; u < IT_ROOM_COUNT; ++u)
	{
		g_axITRooms[u] = ITRoomSample{};
	}

	// Guard FIRST -- RequestSkip bypasses Verify, so no fixed-dt or scene state is
	// installed until both interiors are known present. Both .zscen files are
	// TRACKED (ZM-D-148) so this should never fire; it is here so a checkout that
	// somehow lacks them reports a named prerequisite instead of an empty scan.
	const std::string strPlayerHome =
		std::string(GAME_ASSETS_DIR) + "Scenes/PlayerHome" + ZENITH_SCENE_EXT;
	const std::string strProfLab =
		std::string(GAME_ASSETS_DIR) + "Scenes/ProfLab" + ZENITH_SCENE_EXT;
	if (!ITSceneFilePresent(strPlayerHome.c_str())
		|| !ITSceneFilePresent(strProfLab.c_str()))
	{
		g_bITSkipped = true;
		Zenith_AutomatedTestRunner::RequestSkip(
			"[ZM_InteriorTint] PlayerHome/ProfLab scene files are absent");
		return;
	}

	Zenith_InputSimulator::ResetAllInputState();
	Zenith_InputSimulator::SetFixedDt(fIT_FIXED_DT);

	// Idempotent same-path re-registers (the game boot already registers both).
	g_xEngine.Scenes().RegisterSceneBuildIndex(
		static_cast<int>(ITBuildIndex(ZM_SCENE_PLAYERHOME)),
		GAME_ASSETS_DIR "Scenes/PlayerHome" ZENITH_SCENE_EXT);
	g_xEngine.Scenes().RegisterSceneBuildIndex(
		static_cast<int>(ITBuildIndex(ZM_SCENE_PROFLAB)),
		GAME_ASSETS_DIR "Scenes/ProfLab" ZENITH_SCENE_EXT);

	g_xEngine.Scenes().LoadSceneByIndex(
		static_cast<int>(ITBuildIndex(ZM_SCENE_PLAYERHOME)), SCENE_LOAD_SINGLE);
	g_eITPhase = ITPhase::SettlePlayerHome;
	g_bITActive = true;
}

static bool Step_ZMInteriorTint(int)
{
	if (!g_bITActive || g_bITFailed || g_eITPhase == ITPhase::Done)
	{
		return false;
	}
	++g_iITPhaseFrames;

	switch (g_eITPhase)
	{
	case ITPhase::SettlePlayerHome:
		if (ITRoomReady(ITBuildIndex(ZM_SCENE_PLAYERHOME)))
		{
			ITScanActiveScene(IT_ROOM_PLAYERHOME,
				ZM_GetPlayerHomeInteriorTintColour(), false /* bExact */);
			g_eITPhase = ITPhase::LoadProfLab;
			g_iITPhaseFrames = 0;
			return true;
		}
		if (g_iITPhaseFrames > iIT_SETTLE_DEADLINE)
		{
			ITFail("PlayerHome never became the settled active scene");
			return false;
		}
		return true;

	case ITPhase::LoadProfLab:
		g_xEngine.Scenes().LoadSceneByIndex(
			static_cast<int>(ITBuildIndex(ZM_SCENE_PROFLAB)), SCENE_LOAD_SINGLE);
		g_eITPhase = ITPhase::SettleProfLab;
		g_iITPhaseFrames = 0;
		return true;

	case ITPhase::SettleProfLab:
		if (ITRoomReady(ITBuildIndex(ZM_SCENE_PROFLAB)))
		{
			ITScanActiveScene(IT_ROOM_PROFLAB,
				ZM_GetHumanPaletteFallbackColour(), true /* bExact */);
			g_eITPhase = ITPhase::Done;
			return false;
		}
		if (g_iITPhaseFrames > iIT_SETTLE_DEADLINE)
		{
			ITFail("ProfLab never became the settled active scene");
			return false;
		}
		return true;

	case ITPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_ZMInteriorTint()
{
	if (g_bITSkipped)
	{
		return true;
	}

	bool bPassed = !g_bITFailed;
	if (g_bITFailed)
	{
		Zenith_Error(LOG_CATEGORY_UNITTEST, "[ZM_InteriorTint] %s", g_szITFailure);
	}

	const u_int auExpectedBlocks[IT_ROOM_COUNT] =
	{
		(u_int)ZM_PLAYERHOME_BLOCK_COUNT,
		(u_int)ZM_PROFLAB_BLOCK_COUNT,
	};

	// ---- THE VACUITY GUARD, FIRST AND SEPARATE ------------------------------
	// It names a MISSING OBSERVATION, while the clauses under it name a wiring
	// violation. Without it a run whose scan found no blockout bodies at all would
	// satisfy "offExpected == 0" having measured nothing -- and an empty scan is
	// precisely what a headless regression in ZM_GreyboxVisual::OnStart would
	// produce. A zero or truncated scan is a FAILURE here, never a quiet pass.
	bool bScansUsable = true;
	for (u_int u = 0u; u < IT_ROOM_COUNT; ++u)
	{
		const ITRoomSample& xRoom = g_axITRooms[u];
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_InteriorTint] %s: sampled=%s blocks=%u (expected %u) overflow=%u "
			"offExpected=%u worstChannelError=%.6f firstColour=(%.4f, %.4f, %.4f, %.4f)",
			g_aszITRoomNames[u], xRoom.m_bSampled ? "true" : "false",
			xRoom.m_uCollected, auExpectedBlocks[u], xRoom.m_uOverflow,
			xRoom.m_uOffExpected, (double)xRoom.m_fWorstError,
			(double)xRoom.m_xFirstColour.x, (double)xRoom.m_xFirstColour.y,
			(double)xRoom.m_xFirstColour.z, (double)xRoom.m_xFirstColour.w);

		if (!xRoom.m_bSampled || xRoom.m_uOverflow != 0u
			|| xRoom.m_uCollected != auExpectedBlocks[u])
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_InteriorTint] the %s scan did not observe its shell (sampled=%s "
				"blocks=%u expected=%u overflow=%u) -- every colour clause about "
				"this room would be vacuous, or would be judging a truncated "
				"subset. If the count is ZERO on the Null backend, the greybox "
				"visual is not building its model headlessly; that must be booked "
				"as a coverage boundary with an explicit skip, NEVER left passing "
				"on an empty scan",
				g_aszITRoomNames[u], xRoom.m_bSampled ? "true" : "false",
				xRoom.m_uCollected, auExpectedBlocks[u], xRoom.m_uOverflow);
			bScansUsable = false;
			bPassed = false;
		}
	}

	if (bScansUsable)
	{
		// ---- ARM 1: the bedroom wears the tint ------------------------------
		if (g_axITRooms[IT_ROOM_PLAYERHOME].m_uOffExpected != 0u)
		{
			const Zenith_Maths::Vector4 xTint = ZM_GetPlayerHomeInteriorTintColour();
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_InteriorTint] %u of %u PlayerHome blockout bodies are not "
				"wearing the ZM-D-176 interior tint (%.4f, %.4f, %.4f); worst "
				"channel error %.6f, first sampled (%.4f, %.4f, %.4f). The tint "
				"never reached the material, so the bedroom still reads as the "
				"same greybox room as the lab",
				g_axITRooms[IT_ROOM_PLAYERHOME].m_uOffExpected,
				g_axITRooms[IT_ROOM_PLAYERHOME].m_uCollected,
				(double)xTint.x, (double)xTint.y, (double)xTint.z,
				(double)g_axITRooms[IT_ROOM_PLAYERHOME].m_fWorstError,
				(double)g_axITRooms[IT_ROOM_PLAYERHOME].m_xFirstColour.x,
				(double)g_axITRooms[IT_ROOM_PLAYERHOME].m_xFirstColour.y,
				(double)g_axITRooms[IT_ROOM_PLAYERHOME].m_xFirstColour.z);
			bPassed = false;
		}

		// ---- ARM 2: the lab did not move ------------------------------------
		// EXACT equality, deliberately: the ruling was that ProfLab is left
		// exactly as it is, and that is a claim about bytes that did not move at
		// all -- not about bytes that moved a little.
		if (g_axITRooms[IT_ROOM_PROFLAB].m_uOffExpected != 0u)
		{
			const Zenith_Maths::Vector4 xGrey = ZM_GetHumanPaletteFallbackColour();
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_InteriorTint] %u of %u ProfLab blockout bodies are no longer "
				"EXACTLY the shipped blockout grey (%.4f, %.4f, %.4f); worst "
				"channel error %.6f, first sampled (%.4f, %.4f, %.4f). ZM-D-176 "
				"tints PLAYERHOME ONLY -- this is what a tint predicate that "
				"widened past exact name matching looks like",
				g_axITRooms[IT_ROOM_PROFLAB].m_uOffExpected,
				g_axITRooms[IT_ROOM_PROFLAB].m_uCollected,
				(double)xGrey.x, (double)xGrey.y, (double)xGrey.z,
				(double)g_axITRooms[IT_ROOM_PROFLAB].m_fWorstError,
				(double)g_axITRooms[IT_ROOM_PROFLAB].m_xFirstColour.x,
				(double)g_axITRooms[IT_ROOM_PROFLAB].m_xFirstColour.y,
				(double)g_axITRooms[IT_ROOM_PROFLAB].m_xFirstColour.z);
			bPassed = false;
		}

		// ---- ARM 3: THE TWO ROOMS DO NOT READ THE SAME ----------------------
		// ★ COMPUTED FROM THE TWO SAMPLES, NOT FROM THE TWO CONSTANTS. A clause
		// that measured the constants would be the boot unit again, one layer
		// further from the truth; this one measures what the two scenes actually
		// PUT ON THEIR WALLS, which is the user's motivation stated as a number.
		const float fSampledSeparation = ZM_HumanPaletteSeparation(
			g_axITRooms[IT_ROOM_PLAYERHOME].m_xFirstColour,
			g_axITRooms[IT_ROOM_PROFLAB].m_xFirstColour);
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_InteriorTint] OBSERVED sampled separation between the two "
			"interiors: %.5f against a %.5f floor",
			(double)fSampledSeparation, (double)fZM_HUMAN_PALETTE_MIN_SEPARATION);
		if (fSampledSeparation < fZM_HUMAN_PALETTE_MIN_SEPARATION)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_InteriorTint] the two interiors' sampled wall colours sit only "
				"%.5f apart, under the %.5f margin -- PlayerHome and ProfLab still "
				"read as the same room, which is the complaint ZM-D-176 exists to "
				"answer", (double)fSampledSeparation,
				(double)fZM_HUMAN_PALETTE_MIN_SEPARATION);
			bPassed = false;
		}
	}

	return bPassed;
}

static void Teardown_ZMInteriorTint()
{
	if (g_bITActive)
	{
		g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);   // FrontEnd
	}
	Zenith_InputSimulator::ClearFixedDt();
	Zenith_InputSimulator::ResetAllInputState();
	g_bITActive = false;
}

static const Zenith_AutomatedTest g_xZMInteriorTintTest = {
	"ZM_InteriorTint_Test",
	&Setup_ZMInteriorTint,
	&Step_ZMInteriorTint,
	&Verify_ZMInteriorTint,
	// Two 240-frame room settles plus the one-frame load hop, with headroom, so a
	// named phase deadline rather than this backstop diagnoses an ordinary stall.
	/* maxFrames */ 600,
	false /* m_bRequiresGraphics */,
	false /* m_bManualOnly */,
	&Teardown_ZMInteriorTint,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMInteriorTintTest);

#endif // ZENITH_INPUT_SIMULATOR
