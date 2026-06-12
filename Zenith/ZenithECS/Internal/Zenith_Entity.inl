#pragma once

//==============================================================================
// Zenith_Entity template implementations
//
// This .inl is included from Zenith_Scene.h after both Zenith_SceneData and
// Zenith_Entity are fully defined. It is NOT included from Zenith_Entity.h
// directly because the template bodies need Zenith_SceneData visible.
//
// If you are reading Zenith_Entity.h and looking for the AddComponent /
// GetComponent / TryGetComponent / RemoveComponent / HasComponent
// template bodies, this is where they live.
//
//------------------------------------------------------------------------------
// Component accessor precondition matrix (Phase 9b)
//
//   Accessor              | requires loaded? | tolerates unloading? | entity exists?
//   ----------------------|------------------|----------------------|---------------
//   AddComponent          | yes              | NO (would race with  | asserts
//                         |                  |  unload cleanup)     |
//   GetComponent          | yes              | NO                   | implicit
//                         |                  |                      | (slot validation)
//   RemoveComponent       | yes              | yes (unload cleanup  | asserts
//                         |                  |  legitimately calls) |
//   HasComponent          | NO (read-only,   | n/a                  | implicit
//                         |  always safe)    |                      |
//   TryGetComponent       | soft (nullptr on | soft                 | soft
//                         |  fail)           |                      |
//
// Why the divergence: AddComponent / GetComponent want a fully usable scene
// to operate against; RemoveComponent has to keep working during the unload
// pass so component destructors can fire; HasComponent + TryGetComponent are
// query APIs that tolerate any state. If you add a new accessor, place it in
// the matrix and pick the strictness level that matches its safety profile.
//
// Post-dispatch cleanup paths (e.g., FlushPendingRemovalsViaEntity in
// hot dispatch paths) should use TryGetComponent because their
// parent entity may legitimately have been moved or destroyed by the
// callback they were dispatching.
//==============================================================================

template<typename T, typename... Args>
T& Zenith_Entity::AddComponent(Args&&... args)
{
	Zenith_SceneData* pxSceneData = GetSceneData();
	Zenith_Assert(pxSceneData != nullptr, "AddComponent: Entity has no scene");
	Zenith_Assert(pxSceneData->IsLoaded(), "AddComponent: Entity's scene is not loaded");
	Zenith_Assert(pxSceneData->EntityExists(m_xEntityID), "AddComponent: Entity (idx=%u, gen=%u) is stale", m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);

	const Zenith_SceneData::TypeID uTypeID = Zenith_SceneData::TypeIDGenerator::GetTypeID<T>();
	const Zenith_HashMap<Zenith_SceneData::TypeID, u_int>& xComponentsForThisEntity =
		Zenith_ECS_EntityStore().m_axEntityComponents.Get(m_xEntityID.m_uIndex);
	Zenith_Assert(!xComponentsForThisEntity.Contains(uTypeID), "AddComponent: Entity already has this component type");

	return pxSceneData->CreateComponent<T>(m_xEntityID, std::forward<Args>(args)..., *this);
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
	Zenith_Assert(pxSceneData->IsLoaded(), "GetComponent: Entity's scene is not loaded");

	return pxSceneData->GetComponentFromEntity<T>(m_xEntityID);
}

template<typename T>
T* Zenith_Entity::TryGetComponent() const
{
	Zenith_SceneData* pxSceneData = GetSceneData();
	if (pxSceneData == nullptr) return nullptr;
	if (!pxSceneData->IsLoaded()) return nullptr;
	if (!pxSceneData->EntityExists(m_xEntityID)) return nullptr;
	if (!pxSceneData->EntityHasComponent<T>(m_xEntityID)) return nullptr;

	return &pxSceneData->GetComponentFromEntity<T>(m_xEntityID);
}

template<typename T>
void Zenith_Entity::RemoveComponent()
{
	Zenith_SceneData* pxSceneData = GetSceneData();
	Zenith_Assert(pxSceneData != nullptr, "RemoveComponent: Entity has no scene");
	Zenith_Assert(pxSceneData->IsLoaded(), "RemoveComponent: Entity's scene is not loaded");
	Zenith_Assert(pxSceneData->EntityExists(m_xEntityID), "RemoveComponent: Entity (idx=%u, gen=%u) is stale", m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);
	Zenith_Assert(pxSceneData->EntityHasComponent<T>(m_xEntityID), "RemoveComponent: Entity does not have this component type");

	pxSceneData->RemoveComponentFromEntity<T>(m_xEntityID);
}
