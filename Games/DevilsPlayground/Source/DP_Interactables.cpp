#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "DP_Interactables.h"
#include "DP_Query.h"

#include "ZenithECS/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "Maths/Zenith_Maths.h"

#include "../Components/DPDoor_Component.h"
#include "../Components/DPForge_Component.h"
#include "../Components/DPGraphInteractable_Component.h"
#include "EntityComponent/Components/Zenith_GraphComponent.h"

namespace
{
	// Resolve an entity handle to its world position. Mirrors the HUD's
	// former static TryGetEntityPos helper.
	bool TryGetEntityPos(Zenith_EntityID xId, Zenith_Maths::Vector3& xOut)
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return false;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid()) return false;
		if (!xEnt.HasComponent<Zenith_TransformComponent>()) return false;
		xEnt.GetComponent<Zenith_TransformComponent>().GetPosition(xOut);
		return true;
	}

	// Scan every interactable of type TInteract in the active scene; if the
	// position xMyPos is within an instance's own interact radius AND it's the
	// closest in-range interactable so far, record szTypeLabel + its squared
	// distance. Moved verbatim from DPHUDController's ScanInteractables.
	template<typename TInteract>
	void ScanInteractables(const Zenith_Maths::Vector3& xMyPos,
	                       const char* szTypeLabel,
	                       const char*& szResult,
	                       float& fClosestSq)
	{
		DP_Query::ForEachComponentInActiveScene<TInteract>(
			[&xMyPos, &szResult, &fClosestSq, szTypeLabel]
			(Zenith_EntityID xId, TInteract& xInteract)
			{
				Zenith_Maths::Vector3 xIPos(0.0f);
				if (!TryGetEntityPos(xId, xIPos)) return;
				const float fDx = xIPos.x - xMyPos.x;
				const float fDz = xIPos.z - xMyPos.z;
				const float fSq = fDx * fDx + fDz * fDz;
				const float fR = xInteract.GetInteractRadius();
				if (fSq <= fR * fR && fSq < fClosestSq)
				{
					fClosestSq = fSq;
					szResult = szTypeLabel;
				}
			});
	}

	// Graph-driven interactables share one shim component; the human-readable
	// type label is derived from the attached graph asset.
	const char* LabelForGraphEntity(Zenith_EntityID xId)
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return nullptr;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid() || !xEnt.HasComponent<Zenith_GraphComponent>()) return nullptr;
		Zenith_GraphComponent& xGraphs = xEnt.GetComponent<Zenith_GraphComponent>();
		for (u_int u = 0; u < xGraphs.GetGraphCount(); ++u)
		{
			const char* szPath = xGraphs.GetGraphAssetPathAt(u);
			if (std::strcmp(szPath, "game:Graphs/DP_Chest.bgraph") == 0)        return "chest";
			if (std::strcmp(szPath, "game:Graphs/DP_Pentagram.bgraph") == 0)    return "pentagram";
			if (std::strcmp(szPath, "game:Graphs/DP_NoiseMachine.bgraph") == 0) return "noise machine";
			if (std::strcmp(szPath, "game:Graphs/DP_DoubleDoor.bgraph") == 0)   return "door";
		}
		return nullptr;
	}

	void ScanGraphInteractables(const Zenith_Maths::Vector3& xMyPos,
	                            const char*& szResult,
	                            float& fClosestSq)
	{
		DP_Query::ForEachComponentInActiveScene<DPGraphInteractable_Component>(
			[&xMyPos, &szResult, &fClosestSq]
			(Zenith_EntityID xId, DPGraphInteractable_Component& xShim)
			{
				Zenith_Maths::Vector3 xIPos(0.0f);
				if (!TryGetEntityPos(xId, xIPos)) return;
				const float fDx = xIPos.x - xMyPos.x;
				const float fDz = xIPos.z - xMyPos.z;
				const float fSq = fDx * fDx + fDz * fDz;
				const float fR = xShim.GetInteractRadius();
				if (fSq <= fR * fR && fSq < fClosestSq)
				{
					if (const char* szLabel = LabelForGraphEntity(xId))
					{
						fClosestSq = fSq;
						szResult = szLabel;
					}
				}
			});
	}
}

namespace DP_Interactables
{
	void MarkAsInteractable(Zenith_EntityID /*xId*/, Kind /*eKind*/, void* /*pUserData*/)
	{
		// W0 stub. B3 wires this into DPInteractable_Base during component
		// attach; this entry point is reserved for non-behaviour entities (props
		// the editor can flag as interactable).
	}

	// Walk every interactable subclass in the active scene; if the villager
	// is within range of any of them, return a human-readable type name.
	// Returns nullptr if none in range. Moved here from
	// DPHUDController's FindNearestInteractableType so the HUD header
	// no longer includes the interactable component headers.
	const char* FindNearestInteractableType(Zenith_EntityID xVillager)
	{
		Zenith_Maths::Vector3 xMyPos(0.0f);
		if (!TryGetEntityPos(xVillager, xMyPos)) return nullptr;
		const char* szResult = nullptr;
		float fClosestSq = 1e30f;
		ScanInteractables<DPDoor_Component> (xMyPos, "door",  szResult, fClosestSq);
		ScanInteractables<DPForge_Component>(xMyPos, "forge", szResult, fClosestSq);
		// Chest / pentagram / noise machine / double door are graph-driven.
		ScanGraphInteractables(xMyPos, szResult, fClosestSq);
		return szResult;
	}
}
