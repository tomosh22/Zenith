#pragma once
#include "Core/Zenith_Engine.h"
/**
 * DPForge_Component - gym-map crafting station. Consumes the held item if
 * it matches m_eRecipeInputTag and spawns an output item next to the forge,
 * then auto-equips it on the villager that just interacted (skeleton-grade
 * crafting parity — UE source's recipe table system would replace this).
 *
 * Output spawn flow (mirrors DPItemManager_Component::SpawnItemEntity):
 *   1. Destroy the held input entity (RemoveHeldItem keeps the tag side-table
 *      consistent because the source entity is destroyed too).
 *   2. Instantiate the shared item prefab (cube mesh + sphere collider, baked)
 *      at the forge's world position.
 *   3. Attach DPItemBase_Component with m_eRecipeOutputTag stamped
 *      (post-instantiation).
 *   4. Snap the new item into the villager's hand via DP_Player::SetHeldItem.
 */

#include "Components/DPInteractable_Base.h"
// Contract exception (creation/lifecycle, not a cross-state read): the forge
// spawns an item entity and attaches DPItemBase_Component to it. See
// Components/CLAUDE.md.
#include "Components/DPItemBase_Component.h"
#include "EntityComponent/Components/Zenith_GraphComponent.h"
#include "Scripting/Zenith_BehaviourGraph.h"
#include "Scripting/Zenith_GraphBlackboard.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "Maths/Zenith_Maths.h"
#include "Core/Zenith_AudioBus.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "Prefab/Zenith_Prefab.h"
#include "Source/DPResources.h"

#include "Source/PublicInterfaces.h"
#include "Source/DevilsPlayground_Tags.h"
#include "Source/DP_Tuning.h"

#include <cstdio>
#include <cstring>

class DPForge_Component ZENITH_FINAL : public DPInteractable_Base
{
public:
	// W3 conversion: the craft DECISION sequence (recipe gate, consume ->
	// spawn -> auto-equip -> count -> audio -> event, verbatim order) lives
	// on this graph (DPForgeCraft node); the component is the interact shim
	// (proximity + F-press plumbing from DPInteractable_Base, "Interact"
	// custom-event firing - the DPGraphInteractable pattern) + the prefab
	// spawn systems body. Recipe tags + craft count live on the blackboard.
	static constexpr const char* kszGraphAsset = "game:Graphs/DP_Forge.bgraph";

	DPForge_Component() = delete;
	DPForge_Component(Zenith_Entity& xParentEntity)
		: DPInteractable_Base(xParentEntity)
	{}

	void OnAwake()
	{
		DPInteractable_Base::OnAwake();
		EnsureGraphAttached();
	}

	// Idempotent self-attach. Called from OnAwake AND on-demand from
	// SetRecipe/HandleInteract: tests AddComponent + SetRecipe/CraftForTest
	// in the same frame, BEFORE the awake wave runs - the pre-W3 component
	// worked immediately, so the graph must too (the door's attach-before-
	// seed rule, self-served).
	void EnsureGraphAttached()
	{
		Zenith_GraphComponent* pxGraphs = m_xParentEntity.TryGetComponent<Zenith_GraphComponent>();
		if (pxGraphs == nullptr)
		{
			pxGraphs = &m_xParentEntity.AddComponent<Zenith_GraphComponent>();
		}
		for (u_int u = 0; u < pxGraphs->GetGraphCount(); ++u)
		{
			if (std::strcmp(pxGraphs->GetGraphAssetPathAt(u), kszGraphAsset) == 0)
			{
				return;
			}
		}
		pxGraphs->AddGraphByAssetPath(kszGraphAsset);
	}

	// Test + HUD accessors (blackboard-backed since W3; defaults mirror the
	// pre-W3 member defaults so pre-attach reads keep the old answers).
	DP_ItemTag GetRecipeInputTag() const
	{
		return (DP_ItemTag)ReadBBInt("recipeInput", (int32_t)DP_ItemTag::Iron);
	}
	DP_ItemTag GetRecipeOutputTag() const
	{
		return (DP_ItemTag)ReadBBInt("recipeOutput", (int32_t)DP_ItemTag::Key);
	}
	uint32_t GetCraftCount() const { return (uint32_t)ReadBBInt("craftCount", 0); }

	// MVP-2.3: configure the forge's recipe. Each forge instance holds
	// ONE recipe (input -> output); a level with multiple recipes
	// places multiple forge entities, each with its own recipe.
	// Default is Iron -> Key (the graph's declared variable defaults).
	// Authoring / test code calls SetRecipe after attach to override.
	void SetRecipe(DP_ItemTag eInput, DP_ItemTag eOutput)
	{
		EnsureGraphAttached();
		WriteBBInt("recipeInput", (int32_t)eInput);
		WriteBBInt("recipeOutput", (int32_t)eOutput);
	}

	// Test-only: bypass DPInteractable's proximity / rising-edge dance.
	// Used by Forge_Test to drive the recipe-consume + output-spawn path
	// directly from a fixed frame budget.
	void CraftForTest(Zenith_EntityID xVillager) { HandleInteract(xVillager); }

	// SYSTEMS body invoked synchronously by the DPForgeCraft node: prefab
	// instantiate + DPItemBase attach (the explicit-OnAwake gap pattern).
	// Recipe/count are passed in from the blackboard so the naming quirk
	// stays byte-identical ("ForgeOut_<preIncrementCount>_<outputTag>").
	Zenith_EntityID SpawnOutputItem(DP_ItemTag eOutputTag, uint32_t uCraftCountForName)
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(
			m_xParentEntity.GetEntityID());
		if (pxScene == nullptr) return INVALID_ENTITY_ID;

		Zenith_Maths::Vector3 xForgePos(0.0f);
		if (Zenith_TransformComponent* pxTransform = m_xParentEntity.TryGetComponent<Zenith_TransformComponent>())
		{
			pxTransform->GetPosition(xForgePos);
		}
		// Slight offset so the new item doesn't perfectly overlap the forge mesh.
		xForgePos.x += 0.5f;

		Zenith_Prefab* pxItemPrefab = DevilsPlayground::Resources().m_xItemPrefab.GetDirect();
		if (pxItemPrefab == nullptr) return INVALID_ENTITY_ID;

		char szName[64];
		std::snprintf(szName, sizeof(szName), "ForgeOut_%u_%s",
			uCraftCountForName, DP_ItemTagToString(eOutputTag));

		// Reuse the shared item prefab (cube mesh + sphere collider, baked);
		// Instantiate places it at the forge offset and finalizes the collider.
		Zenith_Entity xEntity = pxItemPrefab->Instantiate(pxScene, std::string(szName), xForgePos);
		if (!xEntity.IsValid()) return INVALID_ENTITY_ID;

		// Component + tag stay post-instantiation. Prefab::Instantiate already
		// dispatched the entity's OnAwake wave, so a component added after it
		// never receives the hook from the lifecycle scheduler -- invoke it
		// explicitly (mirrors the old script system's immediate-OnAwake-on-
		// attach semantics: registers tag=None, then SetTag re-registers).
		DPItemBase_Component& xItemBase = xEntity.AddComponent<DPItemBase_Component>();
		xItemBase.OnAwake();
		xItemBase.SetTag(eOutputTag);

		return xEntity.GetEntityID();
	}

	// Blackboard plumbing (the DPDoor shim pattern). Public so the craft
	// node can bump craftCount through the same seam.
	Zenith_BehaviourGraph* FindForgeGraph() const
	{
		Zenith_GraphComponent* pxGraphs = m_xParentEntity.TryGetComponent<Zenith_GraphComponent>();
		if (pxGraphs == nullptr) return nullptr;
		for (u_int u = 0; u < pxGraphs->GetGraphCount(); ++u)
		{
			if (std::strcmp(pxGraphs->GetGraphAssetPathAt(u), kszGraphAsset) == 0)
			{
				return pxGraphs->GetGraphAt(u);
			}
		}
		return nullptr;
	}
	int32_t ReadBBInt(const char* szVar, int32_t iDefault) const
	{
		Zenith_BehaviourGraph* pxGraph = FindForgeGraph();
		return pxGraph ? pxGraph->GetBlackboard().GetInt32(szVar, iDefault) : iDefault;
	}
	void WriteBBInt(const char* szVar, int32_t iValue)
	{
		if (Zenith_BehaviourGraph* pxGraph = FindForgeGraph())
		{
			Zenith_PropertyValue xValue;
			xValue.SetInt32(iValue);
			pxGraph->GetBlackboard().SetValue(szVar, xValue);
		}
	}

protected:
	void HandleInteract(Zenith_EntityID xVillager) override
	{
		// The DPGraphInteractable pattern: fire "Interact" with the packed
		// villager payload into this entity's graph; DPForgeCraft owns the
		// decisions from here (synchronous - no 1-frame race). Attach on
		// demand: CraftForTest may run before the awake wave.
		EnsureGraphAttached();
		Zenith_GraphComponent* pxGraphs = m_xParentEntity.TryGetComponent<Zenith_GraphComponent>();
		if (pxGraphs == nullptr) return;
		Zenith_PropertyValue xPayload;
		xPayload.SetPackedEntityID(xVillager.GetPacked());
		pxGraphs->FireCustomEvent("Interact", &xPayload);
	}
};
