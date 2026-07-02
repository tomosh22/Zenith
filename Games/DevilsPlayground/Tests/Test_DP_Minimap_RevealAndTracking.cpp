#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Source/DP_Minimap.h"
#include "../Components/DPMinimap_Component.h"
#include "../Components/DPProcLevelBootstrap_Component.h"
#include "../Components/DPVillager_Component.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Core/Zenith_CommandLine.h"
#include "Flux/Flux_Screenshot.h"

#include <cmath>
#include <cstdio>

// ============================================================================
// Test_DP_Minimap_RevealAndTracking
//
// In-engine minimap contract, on the real ProcLevel scene:
//
//   Phase A (settle): the minimap component built one rect per layout room
//     and rooms containing idle villagers are already revealed (villagers
//     record memory reveals every frame).
//   Phase B (reveal): pick the room whose rect is still hidden (least
//     revealed), record a synthetic memory reveal at its centre, and assert
//     the rect becomes visible with a fresh (opaque) colour next frame.
//   Phase C (tracking): possess a villager via the system path; assert the
//     gold player icon appears at exactly WorldToPanel(villager pos) in the
//     panel's coordinate space, then clear possession and assert it hides.
// ============================================================================

namespace
{
	bool g_bMapFailed = false;
	bool g_bMapDone = false;
	char g_szMapWhy[192] = {};
	uint32_t g_uHiddenRoom = UINT32_MAX;
	Zenith_EntityID g_xMapVillager = INVALID_ENTITY_ID;

	void MapFail(const char* szWhy)
	{
		g_bMapFailed = true;
		std::snprintf(g_szMapWhy, sizeof(g_szMapWhy), "%s", szWhy);
	}

	DPMinimap_Component* FindMinimap()
	{
		DPMinimap_Component* pxFound = nullptr;
		DP_Query::ForEachComponentInActiveScene<DPMinimap_Component>(
			[&pxFound](Zenith_EntityID, DPMinimap_Component& xMap)
			{
				if (pxFound == nullptr) pxFound = &xMap;
			});
		return pxFound;
	}
}

static void Setup_MinimapReveal()
{
	g_bMapFailed = false;
	g_bMapDone = false;
	g_szMapWhy[0] = '\0';
	g_uHiddenRoom = UINT32_MAX;
	g_xMapVillager = INVALID_ENTITY_ID;
	g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE); // ProcLevel
}

static bool Step_MinimapReveal(int iFrame)
{
	if (g_bMapFailed || g_bMapDone) return false;

	// ---- Phase A (frame 30): built + counts match; make the target room
	// deterministic by forgetting everything (villagers only re-reveal
	// their OWN cells afterwards, which may or may not hit room 0's
	// probes — Phase B's synthetic reveal is what's asserted, so the test
	// can't flake on villager/probe-cell coincidence across seeds). ----
	if (iFrame == 30)
	{
		DPMinimap_Component* pxMap = FindMinimap();
		DPProcLevelBootstrap_Component* pxBoot = DPProcLevelBootstrap_Component::Instance();
		if (pxMap == nullptr || !pxMap->IsBuilt()) { MapFail("A: minimap not built on ProcLevel"); return false; }
		if (pxBoot == nullptr) { MapFail("A: no bootstrap"); return false; }
		if (pxMap->GetRoomRectCount() != pxBoot->GetLayout().axRooms.GetSize())
		{
			MapFail("A: room rect count != layout room count");
			return false;
		}
		DP_Fog::ClearAllMemoryReveals();
		g_uHiddenRoom = 0;
	}

	// ---- Phase B (frame 32): synthetic reveal flips the target room ----
	if (iFrame == 32)
	{
		DPProcLevelBootstrap_Component* pxBoot = DPProcLevelBootstrap_Component::Instance();
		if (pxBoot == nullptr) { MapFail("B: bootstrap vanished"); return false; }
		const auto& xRoom = pxBoot->GetLayout().axRooms.Get(g_uHiddenRoom);
		DP_Fog::RecordMemoryReveal(Zenith_Maths::Vector3(xRoom.fCentreX, 0.0f, xRoom.fCentreZ));
	}
	if (iFrame == 34)
	{
		DPMinimap_Component* pxMap = FindMinimap();
		if (pxMap == nullptr) { MapFail("B: minimap vanished"); return false; }
		const Zenith_UI::Zenith_UIRect* pxRect = pxMap->GetRoomRect(g_uHiddenRoom);
		if (!pxRect->IsVisible() || pxRect->GetColor().w < 0.9f)
		{
			MapFail("B: revealed room rect did not become visible/opaque");
			return false;
		}
	}

	// ---- Phase C (frame 36): player icon tracks the possessed villager ----
	if (iFrame == 36)
	{
		DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
			[](Zenith_EntityID xId, DPVillager_Component&)
			{
				if (!g_xMapVillager.IsValid()) g_xMapVillager = xId;
			});
		if (!g_xMapVillager.IsValid()) { MapFail("C: no villager"); return false; }
		DP_Player::SetPossessedVillager(g_xMapVillager);
	}
	if (iFrame == 40)
	{
		DPMinimap_Component* pxMap = FindMinimap();
		if (pxMap == nullptr) { MapFail("C: minimap vanished"); return false; }
		const Zenith_UI::Zenith_UIRect* pxIcon = pxMap->GetPlayerIcon();
		if (pxIcon == nullptr || !pxIcon->IsVisible())
		{
			MapFail("C: player icon not visible while possessed");
			return false;
		}
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(g_xMapVillager);
		Zenith_TransformComponent* pxT = xEnt.IsValid() ? xEnt.TryGetComponent<Zenith_TransformComponent>() : nullptr;
		if (pxT == nullptr) { MapFail("C: villager transform missing"); return false; }
		Zenith_Maths::Vector3 xPos;
		pxT->GetPosition(xPos);
		const Vec2 xExpected = DP_Minimap::WorldToPanel(pxMap->GetMapView(), xPos.x, xPos.z);
		const Zenith_Maths::Vector2 xOrigin = DPMinimap_Component::PanelOrigin();
		const Zenith_Maths::Vector2 xActual = pxIcon->GetPosition();
		if (std::fabs(xActual.x - (xOrigin.x + xExpected.x)) > 1.0f ||
		    std::fabs(xActual.y - (xOrigin.y + xExpected.y)) > 1.0f)
		{
			MapFail("C: player icon not at WorldToPanel(villager)");
			return false;
		}
		DP_Player::SetPossessedVillager(INVALID_ENTITY_ID);
	}
	// Windowed runs: dump a visual artifact of the revealed minimap
	// (skipped headless; the assertions above are the actual gate).
	if (iFrame == 42 && !Zenith_CommandLine::IsHeadless())
	{
		Flux_Screenshot::RequestDump("C:/tmp/dp_minimap.tga");
	}

	if (iFrame == 44)
	{
		DPMinimap_Component* pxMap = FindMinimap();
		if (pxMap == nullptr) { MapFail("C2: minimap vanished"); return false; }
		if (pxMap->GetPlayerIcon()->IsVisible())
		{
			MapFail("C2: player icon must hide when possession clears");
			return false;
		}
		g_bMapDone = true;
		return false;
	}

	return true;
}

static bool Verify_MinimapReveal()
{
	if (!g_bMapDone || g_bMapFailed)
	{
		Zenith_Log(LOG_CATEGORY_AI, "DPMinimap failed: %s",
			g_szMapWhy[0] != '\0' ? g_szMapWhy : "did not complete");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xDPMinimapTest = {
	"Test_DP_Minimap_RevealAndTracking",
	&Setup_MinimapReveal,
	&Step_MinimapReveal,
	&Verify_MinimapReveal,
	/*maxFrames*/ 120
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xDPMinimapTest);

#endif // ZENITH_INPUT_SIMULATOR
