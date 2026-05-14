#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Source/DevilsPlayground_Tags.h"
#include "Components/DPVillager_Behaviour.h"
#include "Components/DPItemBase_Behaviour.h"

#include <cstdio>

// ============================================================================
// Test_P2Archetype_ChildCannotCarryTools (MVP-2.1.4)
//
// Pins the GDD's Child archetype mechanic: a Child-archetype villager
// cannot pick up "tool" items (Iron, Key). They CAN pick up objective
// items so the Child can still complete the win condition.
//
// The mechanic forces the player to route tools through Farmhand /
// Devout / Beggar villagers, then switch to the Child only for
// pentagram deliveries -- which interacts with the 25 s half-life
// (Test_P2Archetype_TimersMatchSpec) to make Child a high-risk,
// high-mobility role.
//
// Two-phase procedure on ONE Child villager:
//
//   PHASE A: Child + tool item.
//     1. Possess villager, apply "Child" archetype.
//     2. Find a tool item (Iron or Key) in GameLevel.
//     3. Teleport Child onto tool.
//     4. Tick ~5 frames so DPItemBase::OnUpdate runs proximity check.
//     5. Assert Child holds NOTHING (pickup refused).
//
//   PHASE B: Same Child + objective item.
//     6. Find an objective item.
//     7. Teleport Child onto objective.
//     8. Tick ~5 frames.
//     9. Assert Child holds the objective (proves the gate is
//        TOOL-specific, not "Child can't pick up anything").
//
// What this catches:
//   * MVP-2.1.4 implementation regression: gate dropped from
//     DPItemBase::OnUpdate.
//   * DP_IsToolTag classifies objectives as tools (gate fires for
//     objectives -> Child can't complete the run).
//   * DP_IsToolTag returns false for actual tools (Child can carry
//     everything -> mechanic does nothing).
//   * GetArchetypeId() returns stale "Farmhand" after
//     ApplyArchetype("Child") so the gate's archetype-id read fails.
// ============================================================================

namespace
{
	enum Phase : int { kCC_Start, kCC_WaitScene, kCC_FindItems,
	                   kCC_ApplyArchetypeAndPossess,
	                   kCC_TeleportOntoTool, kCC_TickTool,
	                   kCC_SnapshotTool, kCC_TeleportOntoObjective,
	                   kCC_TickObjective, kCC_SnapshotObjective,
	                   kCC_Verify, kCC_Done };

	int                     g_iPhase = kCC_Start;
	Zenith_EntityID         g_xChild;
	Zenith_EntityID         g_xToolItem;
	Zenith_EntityID         g_xObjectiveItem;
	DP_ItemTag              g_eToolTag = DP_ItemTag::None;
	DP_ItemTag              g_eObjectiveTag = DP_ItemTag::None;
	Zenith_EntityID         g_xHeldAfterTool;
	Zenith_EntityID         g_xHeldAfterObjective;
	int                     g_iTickCounter = 0;
	std::string             g_strArchetypeFinal;

	constexpr int kPICKUP_TICKS = 5;

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

	void TeleportTo(Zenith_EntityID xId, const Zenith_Maths::Vector3& xPos)
	{
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid()) return;
		if (!xEnt.HasComponent<Zenith_TransformComponent>()) return;
		xEnt.GetComponent<Zenith_TransformComponent>().SetPosition(xPos);
	}

	DPVillager_Behaviour* GetVillagerBehaviour(Zenith_EntityID xId)
	{
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return nullptr;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid()) return nullptr;
		if (!xEnt.HasComponent<Zenith_ScriptComponent>()) return nullptr;
		return xEnt.GetComponent<Zenith_ScriptComponent>().GetScript<DPVillager_Behaviour>();
	}
}

static void Setup_P2ChildNoTools()
{
	g_iPhase = kCC_Start;
	g_xChild = INVALID_ENTITY_ID;
	g_xToolItem = INVALID_ENTITY_ID;
	g_xObjectiveItem = INVALID_ENTITY_ID;
	g_eToolTag = DP_ItemTag::None;
	g_eObjectiveTag = DP_ItemTag::None;
	g_xHeldAfterTool = INVALID_ENTITY_ID;
	g_xHeldAfterObjective = INVALID_ENTITY_ID;
	g_iTickCounter = 0;
	g_strArchetypeFinal.clear();
}

static bool Step_P2ChildNoTools(int iFrame)
{
	switch (g_iPhase)
	{
	case kCC_Start:
		Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kCC_WaitScene;
		return true;

	case kCC_WaitScene:
	{
		Zenith_EntityID xFound;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&xFound](Zenith_EntityID xId, DPVillager_Behaviour&)
			{
				if (!xFound.IsValid()) xFound = xId;
			});
		// Wait for at least one item to spawn (DPItemManager OnStart).
		int iItemCount = 0;
		DP_Query::ForEachScriptInActiveScene<DPItemBase_Behaviour>(
			[&iItemCount](Zenith_EntityID, DPItemBase_Behaviour&) { ++iItemCount; });
		if (xFound.IsValid() && iItemCount > 0)
		{
			g_xChild = xFound;
			g_iPhase = kCC_FindItems;
		}
		else if (iFrame > 90)
		{
			g_iPhase = kCC_Done;
		}
		return true;
	}

	case kCC_FindItems:
		// Scan items: pick the first tool and the first objective.
		DP_Query::ForEachScriptInActiveScene<DPItemBase_Behaviour>(
			[](Zenith_EntityID xId, DPItemBase_Behaviour& xB)
			{
				const DP_ItemTag eTag = xB.GetTag();
				if (DP_IsToolTag(eTag) && !g_xToolItem.IsValid())
				{
					g_xToolItem = xId;
					g_eToolTag = eTag;
				}
				if (DP_IsObjectiveTag(eTag) && !g_xObjectiveItem.IsValid())
				{
					g_xObjectiveItem = xId;
					g_eObjectiveTag = eTag;
				}
			});
		if (!g_xToolItem.IsValid() || !g_xObjectiveItem.IsValid())
		{
			Zenith_Log(LOG_CATEGORY_AI,
				"P2ChildNoTools: GameLevel doesn't expose both a tool item and an objective item -- can't run two-phase test (tool.valid=%d objective.valid=%d)",
				(int)g_xToolItem.IsValid(), (int)g_xObjectiveItem.IsValid());
			g_iPhase = kCC_Done;
			return false;
		}
		g_iPhase = kCC_ApplyArchetypeAndPossess;
		return true;

	case kCC_ApplyArchetypeAndPossess:
	{
		DPVillager_Behaviour* pxV = GetVillagerBehaviour(g_xChild);
		if (pxV != nullptr) pxV->ApplyArchetype("Child");
		DP_Player::SetPossessedVillager(g_xChild);
		g_iPhase = kCC_TeleportOntoTool;
		return true;
	}

	case kCC_TeleportOntoTool:
	{
		Zenith_Maths::Vector3 xToolPos;
		if (TryGetEntityPos(g_xToolItem, xToolPos))
		{
			TeleportTo(g_xChild, xToolPos);
		}
		g_iTickCounter = 0;
		g_iPhase = kCC_TickTool;
		return true;
	}

	case kCC_TickTool:
		++g_iTickCounter;
		if (g_iTickCounter >= kPICKUP_TICKS) g_iPhase = kCC_SnapshotTool;
		return true;

	case kCC_SnapshotTool:
		g_xHeldAfterTool = DP_Player::GetHeldItemEntity(g_xChild);
		g_iPhase = kCC_TeleportOntoObjective;
		return true;

	case kCC_TeleportOntoObjective:
	{
		Zenith_Maths::Vector3 xObjPos;
		if (TryGetEntityPos(g_xObjectiveItem, xObjPos))
		{
			TeleportTo(g_xChild, xObjPos);
		}
		g_iTickCounter = 0;
		g_iPhase = kCC_TickObjective;
		return true;
	}

	case kCC_TickObjective:
		++g_iTickCounter;
		if (g_iTickCounter >= kPICKUP_TICKS) g_iPhase = kCC_SnapshotObjective;
		return true;

	case kCC_SnapshotObjective:
		g_xHeldAfterObjective = DP_Player::GetHeldItemEntity(g_xChild);
		g_iPhase = kCC_Verify;
		return true;

	case kCC_Verify:
	{
		DPVillager_Behaviour* pxV = GetVillagerBehaviour(g_xChild);
		if (pxV != nullptr) g_strArchetypeFinal = pxV->GetArchetypeId();
		std::printf("[P2ChildNoTools] archetype=%s toolTag=%s heldAfterTool=(%u/%u) objTag=%s heldAfterObj=(%u/%u) expectedObj=(%u/%u)\n",
			g_strArchetypeFinal.c_str(),
			DP_ItemTagToString(g_eToolTag),
			g_xHeldAfterTool.m_uIndex, g_xHeldAfterTool.m_uGeneration,
			DP_ItemTagToString(g_eObjectiveTag),
			g_xHeldAfterObjective.m_uIndex, g_xHeldAfterObjective.m_uGeneration,
			g_xObjectiveItem.m_uIndex, g_xObjectiveItem.m_uGeneration);
		std::fflush(stdout);
		g_iPhase = kCC_Done;
		return false;
	}

	case kCC_Done:
	default:
		return false;
	}
}

static bool Verify_P2ChildNoTools()
{
	if (!g_xChild.IsValid() || !g_xToolItem.IsValid() || !g_xObjectiveItem.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P2ChildNoTools: precondition failed (child/tool/objective)");
		return false;
	}
	if (g_strArchetypeFinal != "Child")
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2ChildNoTools: ApplyArchetype(\"Child\") didn't stick -- GetArchetypeId returned \"%s\"",
			g_strArchetypeFinal.c_str());
		return false;
	}
	// PHASE A: tool must NOT be picked up.
	if (g_xHeldAfterTool.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2ChildNoTools: Child picked up tool (%s, handle %u/%u) but should have been refused. DP_IsToolTag may be missing the tag, or the archetype-id gate isn't running",
			DP_ItemTagToString(g_eToolTag),
			g_xHeldAfterTool.m_uIndex, g_xHeldAfterTool.m_uGeneration);
		return false;
	}
	// PHASE B: objective MUST be picked up.
	if (!g_xHeldAfterObjective.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2ChildNoTools: Child did NOT pick up objective (%s) -- the archetype filter is too broad, blocking objectives too. Child can't complete the run.",
			DP_ItemTagToString(g_eObjectiveTag));
		return false;
	}
	if (g_xHeldAfterObjective.m_uIndex != g_xObjectiveItem.m_uIndex
		|| g_xHeldAfterObjective.m_uGeneration != g_xObjectiveItem.m_uGeneration)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2ChildNoTools: Child picked up something but not the expected objective (got %u/%u expected %u/%u)",
			g_xHeldAfterObjective.m_uIndex, g_xHeldAfterObjective.m_uGeneration,
			g_xObjectiveItem.m_uIndex, g_xObjectiveItem.m_uGeneration);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP2ChildNoToolsTest = {
	"Test_P2Archetype_ChildCannotCarryTools",
	&Setup_P2ChildNoTools,
	&Step_P2ChildNoTools,
	&Verify_P2ChildNoTools,
	300
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP2ChildNoToolsTest);

#endif // ZENITH_INPUT_SIMULATOR
