#pragma once

#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_SceneManager.h"

//------------------------------------------------------------------------------
// Zenith_Query - Multi-component entity query system
//------------------------------------------------------------------------------
//
// Enables querying entities that have multiple component types with a fluent API.
//
// Usage:
//   pxSceneData->Query<TransformComponent, ColliderComponent>()
//       .ForEach([](Zenith_EntityID uID, TransformComponent& xT, ColliderComponent& xC) {
//           // Process entities with both components
//       });
//
// The query iterates only over entities that have ALL specified component types.
//------------------------------------------------------------------------------

template<typename... Ts>
class Zenith_Query
{
public:
	explicit Zenith_Query(Zenith_SceneData& xSceneData)
		: m_pxSceneData(&xSceneData)
	{
	}

	// ForEach - iterate over all entities that have all queried component types
	// Callback signature: void(Zenith_EntityID, T1&, T2&, ...)
	// NOTE: Safe against entity creation/destruction during iteration via snapshot
	template<typename Func>
	void ForEach(Func&& fn)
	{
		Zenith_Assert(Zenith_Multithreading::IsMainThread() || Zenith_SceneManager::AreRenderTasksActive(), "Query::ForEach must be called from main thread or during render task execution");
		// Snapshot entity IDs before iteration to prevent invalidation
		// if callback creates/destroys entities
		Zenith_Vector<Zenith_EntityID> xSnapshot;
		xSnapshot.Reserve(m_pxSceneData->m_xActiveEntities.GetSize());
		for (u_int u = 0; u < m_pxSceneData->m_xActiveEntities.GetSize(); ++u)
		{
			xSnapshot.PushBack(m_pxSceneData->m_xActiveEntities.Get(u));
		}

		// Iterate snapshot
		for (u_int u = 0; u < xSnapshot.GetSize(); ++u)
		{
			Zenith_EntityID xEntityID = xSnapshot.Get(u);

			// Skip entities that were destroyed during iteration
			if (!m_pxSceneData->EntityExists(xEntityID)) continue;

			// Skip entities pending destruction (Unity-style)
			if (m_pxSceneData->IsMarkedForDestruction(xEntityID)) continue;

			if (HasAllComponents<Ts...>(xEntityID))
			{
				fn(xEntityID, m_pxSceneData->GetComponentFromEntity<Ts>(xEntityID)...);
			}
		}
	}

	// ForEachUnsafe - iterate without snapshot (no allocation)
	// Caller guarantees no structural changes (entity creation/destruction) during iteration.
	// Use from inside Update() where mutations are already deferred.
	template<typename Func>
	void ForEachUnsafe(Func&& fn)
	{
		Zenith_Assert(Zenith_Multithreading::IsMainThread() || Zenith_SceneManager::AreRenderTasksActive(), "Query::ForEachUnsafe must be called from main thread or during render task execution");
		for (u_int u = 0; u < m_pxSceneData->m_xActiveEntities.GetSize(); ++u)
		{
			Zenith_EntityID xEntityID = m_pxSceneData->m_xActiveEntities.Get(u);

			if (m_pxSceneData->IsMarkedForDestruction(xEntityID)) continue;

			if (HasAllComponents<Ts...>(xEntityID))
			{
				fn(xEntityID, m_pxSceneData->GetComponentFromEntity<Ts>(xEntityID)...);
			}
		}
	}

	// Count - returns the number of entities matching the query
	u_int Count()
	{
		Zenith_Assert(Zenith_Multithreading::IsMainThread() || Zenith_SceneManager::AreRenderTasksActive(), "Query::Count must be called from main thread or during render task execution");
		u_int uCount = 0;
		ForEach([&uCount](Zenith_EntityID, Ts&...) { ++uCount; });
		return uCount;
	}

	// CountUnsafe - count without snapshot (no allocation)
	// Same safety requirements as ForEachUnsafe.
	u_int CountUnsafe()
	{
		Zenith_Assert(Zenith_Multithreading::IsMainThread() || Zenith_SceneManager::AreRenderTasksActive(), "Query::CountUnsafe must be called from main thread or during render task execution");
		u_int uCount = 0;
		ForEachUnsafe([&uCount](Zenith_EntityID, Ts&...) { ++uCount; });
		return uCount;
	}

	// First - returns the first matching entity ID, or INVALID_ENTITY_ID if none
	Zenith_EntityID First()
	{
		Zenith_Assert(Zenith_Multithreading::IsMainThread() || Zenith_SceneManager::AreRenderTasksActive(), "Query::First must be called from main thread or during render task execution");
		for (u_int u = 0; u < m_pxSceneData->m_xActiveEntities.GetSize(); ++u)
		{
			Zenith_EntityID xEntityID = m_pxSceneData->m_xActiveEntities.Get(u);

			// Skip entities pending destruction
			if (m_pxSceneData->IsMarkedForDestruction(xEntityID)) continue;

			if (HasAllComponents<Ts...>(xEntityID))
			{
				return xEntityID;
			}
		}
		return INVALID_ENTITY_ID;
	}

	// Any - returns true if at least one entity matches the query
	bool Any()
	{
		Zenith_Assert(Zenith_Multithreading::IsMainThread() || Zenith_SceneManager::AreRenderTasksActive(), "Query::Any must be called from main thread or during render task execution");
		return First().IsValid();
	}

private:
	// Helper to check if entity has all component types using fold expression
	template<typename... Us>
	bool HasAllComponents(Zenith_EntityID xEntityID)
	{
		return (m_pxSceneData->EntityHasComponent<Us>(xEntityID) && ...);
	}

	Zenith_SceneData* m_pxSceneData;
};

//------------------------------------------------------------------------------
// Zenith_SceneData::Query implementation (must be after Zenith_Query definition)
//------------------------------------------------------------------------------

template<typename... Ts>
Zenith_Query<Ts...> Zenith_SceneData::Query()
{
	return Zenith_Query<Ts...>(*this);
}
