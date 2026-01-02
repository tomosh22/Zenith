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
		// Iterate through entity components map to find matching entities
		for (u_int uEntityIdx = 0; uEntityIdx < m_pxScene->m_xEntityComponents.GetSize(); ++uEntityIdx)
		{
			const auto& xComponentMap = m_pxScene->m_xEntityComponents.Get(uEntityIdx);
			if (xComponentMap.empty())
			{
				continue;
			}

			// Check if this entity has all required component types
			// Note: We need to find the actual entity ID from the component pool
			// Since m_xEntityComponents is indexed by entity ID, uEntityIdx IS the entity ID
			Zenith_EntityID uEntityID = static_cast<Zenith_EntityID>(uEntityIdx);

			if (!m_pxScene->EntityExists(uEntityID))
			{
				continue;
			}

			if (HasAllComponents<Ts...>(uEntityID))
			{
				fn(uEntityID, m_pxScene->GetComponentFromEntity<Ts>(uEntityID)...);
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
		Zenith_EntityID uResult = INVALID_ENTITY_ID;
		for (u_int uEntityIdx = 0; uEntityIdx < m_pxScene->m_xEntityComponents.GetSize(); ++uEntityIdx)
		{
			Zenith_EntityID uEntityID = static_cast<Zenith_EntityID>(uEntityIdx);
			if (m_pxScene->EntityExists(uEntityID) && HasAllComponents<Ts...>(uEntityID))
			{
				uResult = uEntityID;
				break;
			}
		}
		return uResult;
	}

	// Any - returns true if at least one entity matches the query
	bool Any()
	{
		return First() != INVALID_ENTITY_ID;
	}

private:
	// Helper to check if entity has all component types using fold expression
	template<typename... Us>
	bool HasAllComponents(Zenith_EntityID uEntityID)
	{
		return (m_pxScene->EntityHasComponent<Us>(uEntityID) && ...);
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
