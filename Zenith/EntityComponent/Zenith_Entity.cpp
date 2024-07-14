#include "Zenith.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Zenith_Scene.h"

Entity::Entity(Scene* pxScene, const std::string& strName) : m_pxParentScene(pxScene){
	m_xEntity = m_pxParentScene->m_xRegistry.create();
	AddComponent<Zenith_TransformComponent>(strName);
	pxScene->m_xEntityMap.insert({ m_ulGUID, *this });
}

Entity::Entity(Scene* pxScene, Zenith_GUID xGUID, Zenith_GUID xParentGUID, const std::string& strName) : m_pxParentScene(pxScene), m_ulGUID(xGUID), m_xParentEntityGUID(xParentGUID), m_strName(strName) {
	m_xEntity = m_pxParentScene->m_xRegistry.create();
	AddComponent<Zenith_TransformComponent>(strName);
	pxScene->m_xEntityMap.insert({ m_ulGUID, *this });
}

void Entity::Serialize(std::ofstream& xOut) {
	STUBBED
#if 0
	xOut << m_xGuid.m_uGuid << '\n';
	xOut << GetComponent<TransformComponent>().m_strName << '\n';
	if (HasComponent<TransformComponent>())
		GetComponent<TransformComponent>().Serialize(xOut);
	if (HasComponent<ColliderComponent>())
		GetComponent<ColliderComponent>().Serialize(xOut);
	if (HasComponent<ModelComponent>())
		GetComponent<ModelComponent>().Serialize(xOut);
	if (HasComponent<ScriptComponent>())
		GetComponent<ScriptComponent>().Serialize(xOut);
#endif
}
