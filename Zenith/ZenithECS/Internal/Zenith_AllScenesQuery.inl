// Zenith_SceneSystem::AllScenesQuery<Ts...> — out-of-line member-template bodies.
//
// Included at the very bottom of Zenith_SceneSystem.h, AFTER Zenith_SceneData.h
// and Zenith_Query.h are complete: the bodies below name pxData->Query<Ts...>(),
// whose returned Zenith_Query<Ts...> must be a COMPLETE type at instantiation.
// Kept out of the public header body so the scene API reads as a method list, not
// a query-iteration implementation.
//
// AllScenesQuery mirrors the per-scene Zenith_Query<Ts...> surface (ForEach /
// Count / First / Any) but spans EVERY loaded scene: it iterates loaded scenes in
// slot order (GetLoadedSceneDataAtSlot) and forwards to each scene's Query<Ts...>.
// To collect pointers, callers do:
//   g_xEngine.Scenes().QueryAllScenes<T>().ForEach(
//       [&](Zenith_EntityID, T& xComp) { xOut.PushBack(&xComp); });
// The callback is passed as an lvalue to each scene's ForEach so it is reused
// across scenes (never moved-from between iterations).
#pragma once

template<typename... Ts>
class Zenith_SceneSystem::AllScenesQuery
{
public:
	explicit AllScenesQuery(Zenith_SceneSystem& xSystem) : m_pxSystem(&xSystem) {}

	template<typename Func>
	void ForEach(Func&& fn)
	{
		const uint32_t uSlotCount = m_pxSystem->GetSceneSlotCount();
		for (uint32_t uIndex = 0; uIndex < uSlotCount; ++uIndex)
		{
			Zenith_SceneData* pxData = m_pxSystem->GetLoadedSceneDataAtSlot(uIndex);
			if (pxData)
			{
				pxData->Query<Ts...>().ForEach(fn);
			}
		}
	}

	u_int Count()
	{
		u_int uCount = 0;
		const uint32_t uSlotCount = m_pxSystem->GetSceneSlotCount();
		for (uint32_t uIndex = 0; uIndex < uSlotCount; ++uIndex)
		{
			Zenith_SceneData* pxData = m_pxSystem->GetLoadedSceneDataAtSlot(uIndex);
			if (pxData)
			{
				uCount += pxData->Query<Ts...>().Count();
			}
		}
		return uCount;
	}

	// First match across all loaded scenes (slot order), or INVALID_ENTITY_ID.
	Zenith_EntityID First()
	{
		const uint32_t uSlotCount = m_pxSystem->GetSceneSlotCount();
		for (uint32_t uIndex = 0; uIndex < uSlotCount; ++uIndex)
		{
			Zenith_SceneData* pxData = m_pxSystem->GetLoadedSceneDataAtSlot(uIndex);
			if (pxData)
			{
				const Zenith_EntityID xID = pxData->Query<Ts...>().First();
				if (xID.IsValid()) return xID;
			}
		}
		return INVALID_ENTITY_ID;
	}

	bool Any() { return First().IsValid(); }

private:
	Zenith_SceneSystem* m_pxSystem;
};

template<typename... Ts>
inline Zenith_SceneSystem::AllScenesQuery<Ts...> Zenith_SceneSystem::QueryAllScenes()
{
	return AllScenesQuery<Ts...>(*this);
}
