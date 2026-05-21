#pragma once

//==============================================================================
// Zenith_Entity template implementations
//
// This .inl is included from Zenith_Scene.h after both Zenith_SceneData and
// Zenith_Entity are fully defined. It is NOT included from Zenith_Entity.h
// directly because the template bodies need Zenith_SceneData visible.
//
// If you are reading Zenith_Entity.h and looking for the AddComponent /
// GetComponent / TryGetComponent / RemoveComponent / HasComponent /
// AddOrReplaceComponent template bodies, this is where they live.
//==============================================================================

template<typename T, typename... Args>
T& Zenith_Entity::AddComponent(Args&&... args)
{
	Zenith_SceneData* pxSceneData = GetSceneData();
	Zenith_Assert(pxSceneData != nullptr, "AddComponent: Entity has no scene");
	Zenith_Assert(pxSceneData->m_bIsLoaded, "AddComponent: Entity's scene is not loaded");
	Zenith_Assert(!pxSceneData->m_bIsUnloading, "AddComponent: Cannot add component during scene unload");
	Zenith_Assert(pxSceneData->EntityExists(m_xEntityID), "AddComponent: Entity (idx=%u, gen=%u) is stale", m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);

	const Zenith_SceneData::TypeID uTypeID = Zenith_SceneData::TypeIDGenerator::GetTypeID<T>();
	const std::unordered_map<Zenith_SceneData::TypeID, u_int>& xComponentsForThisEntity =
		g_xEngine.EntityStore().m_axEntityComponents.Get(m_xEntityID.m_uIndex);
	Zenith_Assert(!xComponentsForThisEntity.contains(uTypeID), "AddComponent: Entity already has this component type");

	return pxSceneData->CreateComponent<T>(m_xEntityID, std::forward<Args>(args)..., *this);
}

template<typename T, typename... Args>
T& Zenith_Entity::AddOrReplaceComponent(Args&&... args)
{
	if (HasComponent<T>())
	{
		RemoveComponent<T>();
	}
	return AddComponent<T>(std::forward<Args>(args)...);
}

template<typename T>
bool Zenith_Entity::HasComponent() const
{
	Zenith_SceneData* pxSceneData = GetSceneData();
	Zenith_Assert(pxSceneData != nullptr, "HasComponent: Entity has no scene");
	return pxSceneData->EntityHasComponent<T>(m_xEntityID);
}

template<typename T>
T& Zenith_Entity::GetComponent() const
{
	Zenith_SceneData* pxSceneData = GetSceneData();
	Zenith_Assert(pxSceneData != nullptr, "GetComponent: Entity has no scene");
	Zenith_Assert(pxSceneData->m_bIsLoaded, "GetComponent: Entity's scene is not loaded");

	return pxSceneData->GetComponentFromEntity<T>(m_xEntityID);
}

template<typename T>
T* Zenith_Entity::TryGetComponent() const
{
	Zenith_SceneData* pxSceneData = GetSceneData();
	if (pxSceneData == nullptr) return nullptr;
	if (!pxSceneData->m_bIsLoaded) return nullptr;
	if (!pxSceneData->EntityExists(m_xEntityID)) return nullptr;
	if (!pxSceneData->EntityHasComponent<T>(m_xEntityID)) return nullptr;

	return &pxSceneData->GetComponentFromEntity<T>(m_xEntityID);
}

template<typename T>
void Zenith_Entity::RemoveComponent()
{
	Zenith_SceneData* pxSceneData = GetSceneData();
	Zenith_Assert(pxSceneData != nullptr, "RemoveComponent: Entity has no scene");
	Zenith_Assert(pxSceneData->m_bIsLoaded || pxSceneData->m_bIsUnloading, "RemoveComponent: Entity's scene is not loaded");
	// Note: Removing components during unload IS allowed (it's part of cleanup)
	// Only adding components during unload is forbidden
	Zenith_Assert(pxSceneData->EntityExists(m_xEntityID), "RemoveComponent: Entity (idx=%u, gen=%u) is stale", m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);
	Zenith_Assert(pxSceneData->EntityHasComponent<T>(m_xEntityID), "RemoveComponent: Entity does not have this component type");

	pxSceneData->RemoveComponentFromEntity<T>(m_xEntityID);
}
