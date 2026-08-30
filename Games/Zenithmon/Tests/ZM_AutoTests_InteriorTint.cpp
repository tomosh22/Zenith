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
// test would be silent exactly where it is needed.
//
// ★★ THE SCAN IS KEYED ON THE SEVEN BLOCK ENTITY NAMES, NOT ON "every material in
// the scene called ZM_Greybox", AND THE DIFFERENCE IS A HARD CI GATE.
// ZM_GreyboxVisual serves TWO populations and gives BOTH the same material name
// (see the comment on its BuildBlockMesh: the name is the only handle a test TU
// has on a file-local class). A HUMAN whose bake is not loadable -- the cold-start
// fallback, which is what a non-tools build on a fresh clone runs -- gets a
// proportioned block wearing that same "ZM_Greybox" name in its PALETTE colour.
// A material-name scan therefore collects human bodies as though they were walls:
// both interiors author a Player, ProfLab now also authors Professor Aster, and on
// a cold tree the count runs past ZM_*_BLOCK_COUNT while the palette colours fail
// the exact-grey clause. Neither symptom names its cause, and the failure is
// ENVIRONMENT-DEPENDENT -- warm and cold trees disagree -- which is the worst
// possible shape for a required check.
//
// Walking ZM_GetPlayerHomeBlockName / ZM_GetProfLabBlockName over
// [0, BLOCK_COUNT) and resolving each through Zenith_SceneData::FindEntityByName
// (the idiom ZM_AutoTests_ProfLab.cpp's shell walk already uses) closes it BY
// CONSTRUCTION: a body that is not one of the named shell blocks cannot enter the
// population at all, whatever material it wears. The material NAME is still
// asserted per block -- it is the evidence that ZM_GreyboxVisual::OnStart really
// does build its model and material on the Null backend -- it is simply no longer
// the thing that decides WHO is measured.
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
#include "ZenithECS/Zenith_SceneData.h"        // FindEntityByName -- the shell walk's key
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"
#include "Zenithmon/Source/Gen/ZM_HumanAppearance.h"
#include "Zenithmon/Source/World/ZM_PlayerHomePlacement.h"
#include "Zenithmon/Source/World/ZM_ProfLabPlacement.h"
#include "Zenithmon/Source/Gen/ZM_InteriorGen.h"          // the room + surface names the shell materials carry
#include "Zenithmon/Source/World/ZM_InteriorDressing.h"   // the shell entity names

namespace
{
	constexpr float fIT_FIXED_DT = 1.0f / 60.0f;

	// The material name every blockout body wears. ZM_GreyboxVisual is file-local to
	// Zenithmon.cpp and unnameable from a test TU, so this is still the only handle
	// a test has on the material it builds -- but it is now a PER-BLOCK ASSERTION
	// rather than the scan's selector (see the ★★ note in the file header).
	const char* const szIT_GREYBOX_MATERIAL = "ZM_Greybox";

	// The sample cap is slack against the seven blocks each interior authors, and a
	// block table that outgrew it is counted so a truncated scan reds rather than
	// quietly reporting a clean subset.
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

	// The shell inventory each room is scanned BY, spelled once. Both accessors are
	// exactly the ones the tools authoring loop walks, so a renamed or reordered
	// block moves the authoring and this scan together instead of stranding one.
	u_int ITRoomBlockCount(u_int uRoom)
	{
		return uRoom == (u_int)IT_ROOM_PLAYERHOME
			? (u_int)ZM_PLAYERHOME_BLOCK_COUNT
			: (u_int)ZM_PROFLAB_BLOCK_COUNT;
	}

	const char* ITRoomBlockName(u_int uRoom, u_int uBlock)
	{
		return uRoom == (u_int)IT_ROOM_PLAYERHOME
			? ZM_GetPlayerHomeBlockName((ZM_PLAYERHOME_BLOCK)uBlock)
			: ZM_GetProfLabBlockName((ZM_PROFLAB_BLOCK)uBlock);
	}

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
		u_int m_uOverflow = 0u;             // block table larger than the sample cap
		u_int m_uUnresolved = 0u;           // named blocks with no entity / no model / no material
		u_int m_uWrongMaterial = 0u;        // named blocks whose material is not szIT_GREYBOX_MATERIAL
		u_int m_uOffExpected = 0u;          // blocks whose colour is not the expected one
		bool m_bSampled = false;
		float m_fWorstError = 0.0f;         // worst per-channel deviation seen
		Zenith_Maths::Vector4 m_xFirstColour = Zenith_Maths::Vector4(0.0f);
		const char* m_szFirstUnresolved = nullptr;
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

	// ★ THE POPULATION IS THE NAMED SHELL BLOCKS, AND ONLY THEM. Walking the block
	// enum and resolving each name is what makes it impossible for a human body --
	// the Player, or ProfLab's Professor Aster -- to be measured as though it were a
	// wall, whatever material it happens to be wearing on a cold tree. An
	// unresolvable name is COUNTED, never skipped: a missing block would otherwise
	// shrink the population and leave the colour clauses judging a clean subset.
	//
	// bExact selects the comparison: ProfLab's half is an EQUALITY claim about
	// bytes that must not have moved, so it is compared exactly; PlayerHome's is a
	// claim about a compiled colour reaching a runtime material, so it carries the
	// float-noise tolerance.
	void ITScanActiveScene(
		ITRoom eRoom, const Zenith_Maths::Vector4& xExpected, bool bExact)
	{
		ITRoomSample& xRoom = g_axITRooms[eRoom];
		xRoom = ITRoomSample{};
		xRoom.m_bSampled = true;

		Zenith_Maths::Vector4 axColours[uIT_MAX_SAMPLED] = {};
		u_int uCollected = 0u;

		// ★★ THIS WALKS THE ROOM SHELL, NOT THE SEVEN BLOCKOUTS, and the change is
		// a re-derivation rather than a relaxation.
		//
		// Until the interior overhaul, both rooms were seven grey blockout boxes
		// distinguished by a per-scene VERTEX TINT, and this scan sampled that tint
		// off each block's ZM_GreyboxVisual material. The blockouts are
		// COLLIDER-ONLY now -- they carry no model and no material at all, by
		// design, because Zenith_ColliderComponent sizes an AABB from mesh bounds
		// the moment a model appears. The picture is one visual-only shell entity
		// per room carrying the generated four-surface room model.
		//
		// So the old scan would report all fourteen blocks UNRESOLVED, which its own
		// guard correctly calls "an entity whose ZM_GreyboxVisual built no
		// model/material". The premise died on purpose; the CLAIM did not.
		//
		// ★ WHAT IS MEASURED NOW, AND WHY IT IS NOT A COLOUR. The rooms' colours
		// come from TEXTURES, not from a material base-colour factor, so there is no
		// longer a runtime float to compare -- sampling GetBaseColor() would read
		// the same default from both rooms and pass vacuously forever. What CAN go
		// wrong at runtime, and what this therefore checks, is IDENTITY: that each
		// scene resolved a shell, that it carries one submesh per surface class, and
		// that every one of those materials is THIS room's own. A PlayerHome scene
		// that loaded ProfLab's room model would be a complete, correct-looking
		// interior belonging to the wrong building, and nothing else would see it.
		//
		// The "the two rooms read apart" half of ZM-D-176 is a property of compiled
		// constants, so it moved to a unit that can state it exactly:
		// ZM_Gen/InteriorRoomsAreVisuallySeparated.
		Zenith_SceneData* pxData = g_xEngine.Scenes().GetActiveSceneData();
		const ZM_INTERIOR_ROOM eGenRoom = (eRoom == IT_ROOM_PROFLAB)
			? ZM_INTERIOR_ROOM_PROF_LAB : ZM_INTERIOR_ROOM_PLAYER_HOME;
		const char* szShellName = (eRoom == IT_ROOM_PROFLAB)
			? szZM_PROFLAB_SHELL_ENTITY_NAME : szZM_PLAYERHOME_SHELL_ENTITY_NAME;

		Zenith_Entity xShell = pxData != nullptr
			? pxData->FindEntityByName(szShellName) : Zenith_Entity();
		const Zenith_ModelComponent* pxShellModel = xShell.IsValid()
			? xShell.TryGetComponent<Zenith_ModelComponent>() : nullptr;
		if (pxShellModel == nullptr || pxShellModel->GetNumMeshes() == 0u)
		{
			++xRoom.m_uUnresolved;
			xRoom.m_szFirstUnresolved = szShellName;
		}
		else
		{
			const u_int uMeshes = pxShellModel->GetNumMeshes();
			for (u_int uSurface = 0u; uSurface < uMeshes; ++uSurface)
			{
				if (uCollected >= uIT_MAX_SAMPLED)
				{
					++xRoom.m_uOverflow;   // Verify reds rather than judging a subset
					continue;
				}
				const Zenith_MaterialAsset* pxMaterial = pxShellModel->GetMaterial(uSurface);
				if (pxMaterial == nullptr)
				{
					++xRoom.m_uUnresolved;
					if (xRoom.m_szFirstUnresolved == nullptr)
					{
						xRoom.m_szFirstUnresolved = szShellName;
					}
					continue;
				}

				// The material name is "<Room>_<surface>" -- the evidence that THIS
				// room's bundle reached THIS scene, and the one thing here that a
				// cross-wired scene would fail.
				char acExpected[128];
				snprintf(acExpected, sizeof(acExpected), "%s_%s",
					ZM_InteriorRoomName(eGenRoom),
					ZM_InteriorSurfaceName((ZM_INTERIOR_SURFACE)
						(uSurface < (u_int)ZM_INTERIOR_SURFACE_COUNT
							? uSurface : 0u)));
				if (pxMaterial->GetName() != acExpected)
				{
					++xRoom.m_uWrongMaterial;
				}

				axColours[uCollected] = pxMaterial->GetBaseColor();
				++uCollected;
			}
		}

		xRoom.m_uCollected = uCollected;
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

	// One material per SURFACE CLASS -- the shell model has exactly that many
	// submeshes. It used to be one per BLOCKOUT; the blockouts carry no material
	// now, by design (see ITScanActiveScene).
	const u_int auExpectedBlocks[IT_ROOM_COUNT] =
	{
		(u_int)ZM_INTERIOR_SURFACE_COUNT,
		(u_int)ZM_INTERIOR_SURFACE_COUNT,
	};

	// ---- THE VACUITY GUARD, FIRST AND SEPARATE ------------------------------
	// It names a MISSING OBSERVATION, while the clauses under it name a wiring
	// violation. Without it a run whose scan resolved no blockout bodies at all
	// would satisfy "offExpected == 0" having measured nothing -- and an empty scan
	// is precisely what a headless regression in ZM_GreyboxVisual::OnStart would
	// produce. A zero or truncated scan is a FAILURE here, never a quiet pass.
	//
	// ★ THE COUNT IS NOW EXACT BY CONSTRUCTION, and that is deliberate rather than
	// redundant: the scan walks exactly ZM_*_BLOCK_COUNT names, so a shortfall can
	// only be an unresolved block and can never again be a human body inflating the
	// total. The equality is kept BECAUSE it is now a real question about the scene
	// (were all seven authored, and did each build a material?) instead of a
	// question about which population the material name happened to select.
	bool bScansUsable = true;
	for (u_int u = 0u; u < IT_ROOM_COUNT; ++u)
	{
		const ITRoomSample& xRoom = g_axITRooms[u];
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_InteriorTint] %s: sampled=%s blocks=%u (expected %u) overflow=%u "
			"unresolved=%u wrongMaterial=%u offExpected=%u worstChannelError=%.6f "
			"firstColour=(%.4f, %.4f, %.4f, %.4f)",
			g_aszITRoomNames[u], xRoom.m_bSampled ? "true" : "false",
			xRoom.m_uCollected, auExpectedBlocks[u], xRoom.m_uOverflow,
			xRoom.m_uUnresolved, xRoom.m_uWrongMaterial,
			xRoom.m_uOffExpected, (double)xRoom.m_fWorstError,
			(double)xRoom.m_xFirstColour.x, (double)xRoom.m_xFirstColour.y,
			(double)xRoom.m_xFirstColour.z, (double)xRoom.m_xFirstColour.w);

		if (!xRoom.m_bSampled || xRoom.m_uOverflow != 0u
			|| xRoom.m_uUnresolved != 0u
			|| xRoom.m_uCollected != auExpectedBlocks[u])
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_InteriorTint] the %s scan did not observe its shell (sampled=%s "
				"blocks=%u expected=%u overflow=%u unresolved=%u, first unresolved "
				"'%s') -- every colour clause about this room would be vacuous, or "
				"would be judging a truncated subset. An UNRESOLVED block is either "
				"an entity the scene never authored under that name, or one whose "
				"ZM_GreyboxVisual built no model/material; if EVERY block is "
				"unresolved on the Null backend, the greybox visual is not building "
				"its model headlessly, and that must be booked as a coverage "
				"boundary with an explicit skip, NEVER left passing on an empty scan",
				g_aszITRoomNames[u], xRoom.m_bSampled ? "true" : "false",
				xRoom.m_uCollected, auExpectedBlocks[u], xRoom.m_uOverflow,
				xRoom.m_uUnresolved,
				xRoom.m_szFirstUnresolved != nullptr
					? xRoom.m_szFirstUnresolved : "(none)");
			bScansUsable = false;
			bPassed = false;
		}

		// The material-name claim, kept as its OWN clause now that it no longer
		// selects the population. A shell block wearing something other than
		// ZM_GreyboxVisual's material means the blockout branch stopped running for
		// it -- which the colour clauses below would report as a wrong colour and
		// misdiagnose.
		if (xRoom.m_uWrongMaterial != 0u)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_InteriorTint] %u of %s's %u shell blocks carry a material that "
				"is not '%s' -- ZM_GreyboxVisual's BLOCKOUT branch did not run for "
				"them, so whatever colour they are wearing is not the one this test "
				"is about",
				xRoom.m_uWrongMaterial, g_aszITRoomNames[u], xRoom.m_uCollected,
				szIT_GREYBOX_MATERIAL);
			bScansUsable = false;
			bPassed = false;
		}
	}

	if (bScansUsable)
	{
		// ---- THE ONE LIVE CLAIM: EACH SCENE LOADED ITS OWN ROOM -------------
		//
		// ★ THE THREE COLOUR ARMS THAT STOOD HERE ARE GONE, AND NOT BECAUSE THEY
		// WERE INCONVENIENT. They asserted that PlayerHome's blockouts wore the
		// ZM-D-176 warm tint EXACTLY, that ProfLab's wore the shipped grey exactly,
		// and that the two sampled colours were far enough apart. All three read
		// Zenith_MaterialAsset::GetBaseColor() off a per-block greybox material.
		//
		// Both premises are now deliberately false. The blockouts are collider-only
		// -- there is no per-block material to sample -- and the rooms' colour comes
		// from TEXTURES on a shell model, not from a base-colour factor. Left in
		// place, all three would read the same untouched default from both rooms:
		// arms 1 and 2 would red forever, and arm 3 would pass VACUOUSLY at a
		// separation of exactly zero-versus-zero. A clause that cannot fail is worse
		// than a deleted one, because it still looks like coverage.
		//
		// So the claim splits along what each layer can actually see:
		//
		//   HERE, live: identity. Each scene resolved a shell, it carries one
		//   material per surface class, and every one of them is THIS room's own.
		//   A PlayerHome scene that had loaded ProfLab's model would be a complete,
		//   correct-looking interior belonging to the wrong building -- the one
		//   thing that can go wrong at runtime, and invisible to everything else.
		//
		//   ZM_Gen/InteriorRoomsAreVisuallySeparated, pure: the ZM-D-176 magnitude.
		//   The two rooms' palettes are compiled constants, so a unit can state the
		//   separation exactly, over EVERY surface rather than one sampled block,
		//   and can assert the direction too (the bedroom warm, the lab cool) --
		//   which is what the pixel test downstream actually measures.
		for (u_int u = 0u; u < (u_int)IT_ROOM_COUNT; ++u)
		{
			if (g_axITRooms[u].m_uWrongMaterial != 0u)
			{
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ZM_InteriorTint] %u of %u materials on the %s shell do not belong "
					"to that room. The scene has loaded another room's model: it will "
					"render as a complete and entirely convincing interior, of the wrong "
					"building",
					g_axITRooms[u].m_uWrongMaterial, g_axITRooms[u].m_uCollected,
					g_aszITRoomNames[u]);
				bPassed = false;
			}
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
