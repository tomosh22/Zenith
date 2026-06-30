#pragma once
/**
 * DP_TestGraphHelpers - test-side helpers for graph-driven entities.
 *
 * The graph conversions moved interactable logic off C++ components onto
 * Behaviour Graphs; tests that used to locate entities by component type and
 * read members (IsOpen() etc.) now locate them by .bgraph slot path and read
 * the graph blackboard.
 */

#include "EntityComponent/Components/Zenith_GraphComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Scripting/Zenith_BehaviourGraph.h"
#include "Scripting/Zenith_GraphBlackboard.h"
#include "Source/DP_Query.h"

#include <cstring>

// First entity in the active scene whose GraphComponent carries the asset.
inline Zenith_EntityID DP_FindFirstEntityWithGraph(const char* szAssetPath)
{
	Zenith_EntityID xResult;
	DP_Query::ForEachComponentInActiveScene<Zenith_GraphComponent>(
		[&xResult, szAssetPath](Zenith_EntityID xId, Zenith_GraphComponent& xGraphs)
		{
			if (xResult.IsValid()) return;
			for (u_int u = 0; u < xGraphs.GetGraphCount(); ++u)
			{
				if (std::strcmp(xGraphs.GetGraphAssetPathAt(u), szAssetPath) == 0)
				{
					xResult = xId;
					return;
				}
			}
		});
	return xResult;
}

// Count of active-scene entities carrying the asset.
inline int DP_CountEntitiesWithGraph(const char* szAssetPath)
{
	int iCount = 0;
	DP_Query::ForEachComponentInActiveScene<Zenith_GraphComponent>(
		[&iCount, szAssetPath](Zenith_EntityID, Zenith_GraphComponent& xGraphs)
		{
			for (u_int u = 0; u < xGraphs.GetGraphCount(); ++u)
			{
				if (std::strcmp(xGraphs.GetGraphAssetPathAt(u), szAssetPath) == 0)
				{
					++iCount;
					return;
				}
			}
		});
	return iCount;
}

// Closest active-scene entity carrying the asset (squared-XZ distance).
inline Zenith_EntityID DP_FindClosestEntityWithGraph(const char* szAssetPath, const Zenith_Maths::Vector3& xTo)
{
	Zenith_EntityID xResult;
	float fBestSq = 1e30f;
	DP_Query::ForEachComponentInActiveScene<Zenith_GraphComponent>(
		[&xResult, &fBestSq, szAssetPath, &xTo](Zenith_EntityID xId, Zenith_GraphComponent& xGraphs)
		{
			bool bMatch = false;
			for (u_int u = 0; u < xGraphs.GetGraphCount(); ++u)
			{
				if (std::strcmp(xGraphs.GetGraphAssetPathAt(u), szAssetPath) == 0) { bMatch = true; break; }
			}
			if (!bMatch) return;
			Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xId);
			if (!xEnt.IsValid()) return;
			Zenith_TransformComponent* pxTransform = xEnt.TryGetComponent<Zenith_TransformComponent>();
			if (pxTransform == nullptr) return;
			Zenith_Maths::Vector3 xPos;
			pxTransform->GetPosition(xPos);
			const float fDx = xPos.x - xTo.x;
			const float fDz = xPos.z - xTo.z;
			const float fSq = fDx * fDx + fDz * fDz;
			if (fSq < fBestSq) { fBestSq = fSq; xResult = xId; }
		});
	return xResult;
}

// The live graph instance for the asset on the given entity (nullptr if absent
// or unresolved).
inline Zenith_BehaviourGraph* DP_GetGraphOn(Zenith_EntityID xId, const char* szAssetPath)
{
	Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xId);
	if (!xEnt.IsValid()) return nullptr;
	Zenith_GraphComponent* pxGraphs = xEnt.TryGetComponent<Zenith_GraphComponent>();
	if (pxGraphs == nullptr) return nullptr;
	Zenith_GraphComponent& xGraphs = *pxGraphs;
	for (u_int u = 0; u < xGraphs.GetGraphCount(); ++u)
	{
		if (std::strcmp(xGraphs.GetGraphAssetPathAt(u), szAssetPath) == 0)
		{
			return xGraphs.GetGraphAt(u);
		}
	}
	return nullptr;
}

// Blackboard reads - the graph-era equivalent of the C++ accessors.
inline bool DP_GetGraphBool(Zenith_EntityID xId, const char* szAssetPath, const char* szVar, bool bDefault = false)
{
	Zenith_BehaviourGraph* pxGraph = DP_GetGraphOn(xId, szAssetPath);
	return pxGraph ? pxGraph->GetBlackboard().GetBool(szVar, bDefault) : bDefault;
}

inline float DP_GetGraphFloat(Zenith_EntityID xId, const char* szAssetPath, const char* szVar, float fDefault = 0.0f)
{
	Zenith_BehaviourGraph* pxGraph = DP_GetGraphOn(xId, szAssetPath);
	return pxGraph ? pxGraph->GetBlackboard().GetFloat(szVar, fDefault) : fDefault;
}
