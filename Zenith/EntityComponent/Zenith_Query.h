#pragma once

#include "EntityComponent/Zenith_Scene.h"

//------------------------------------------------------------------------------
// Zenith_Query - Multi-component entity query system
//------------------------------------------------------------------------------
//
// Enables querying entities that have multiple component types with a fluent API.
//
// Usage:
//   scene.Query<TransformComponent, ColliderComponent>()
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
	explicit Zenith_Query(Zenith_Scene& xScene)
		: m_pxScene(&xScene)
	{
	}

	// ForEach - iterate over all entities that have all queried component types
	// Callback signature: void(Zenith_EntityID, T1&, T2&, ...)
	template<typename Func>
	void ForEach(Func&& fn)
	{
		// Iterate m_xActiveEntities for O(num_entities)
		for (u_int u = 0; u < m_pxScene->m_xActiveEntities.GetSize(); ++u)
		{
			Zenith_EntityID xEntityID = m_pxScene->m_xActiveEntities.Get(u);

			// Skip entities pending destruction (Unity-style)
			if (m_pxScene->IsMarkedForDestruction(xEntityID)) continue;

			if (HasAllComponents<Ts...>(xEntityID))
			{
				fn(xEntityID, m_pxScene->GetComponentFromEntity<Ts>(xEntityID)...);
			}
		}
	}

	// Count - returns the number of entities matching the query
	u_int Count()
	{
		u_int uCount = 0;
		ForEach([&uCount](Zenith_EntityID, Ts&...) { ++uCount; });
		return uCount;
	}

	// First - returns the first matching entity ID, or INVALID_ENTITY_ID if none
	Zenith_EntityID First()
	{
		for (u_int u = 0; u < m_pxScene->m_xActiveEntities.GetSize(); ++u)
		{
			Zenith_EntityID xEntityID = m_pxScene->m_xActiveEntities.Get(u);

			// Skip entities pending destruction
			if (m_pxScene->IsMarkedForDestruction(xEntityID)) continue;

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
		return First().IsValid();
	}

private:
	// Helper to check if entity has all component types using fold expression
	template<typename... Us>
	bool HasAllComponents(Zenith_EntityID xEntityID)
	{
		return (m_pxScene->EntityHasComponent<Us>(xEntityID) && ...);
	}

	Zenith_Scene* m_pxScene;
};

//------------------------------------------------------------------------------
// Zenith_Scene::Query implementation (must be after Zenith_Query definition)
//------------------------------------------------------------------------------

template<typename... Ts>
Zenith_Query<Ts...> Zenith_Scene::Query()
{
	return Zenith_Query<Ts...>(*this);
}
