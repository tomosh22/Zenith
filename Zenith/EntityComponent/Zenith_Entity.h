#pragma once
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"

class Entity
{
public:
	Entity() = delete;
	Entity(Scene* pxScene, const std::string& strName);
	Entity(Scene* pxScene, Zenith_GUID xGUID, Zenith_GUID xParentGUID, const std::string& strName);

	template<typename T, typename... Args>
	T& AddComponent(Args&&... args) {
		VCE_Assert(!HasComponent<T>(), "Already has this component");
		return m_pxParentScene->m_xRegistry.emplace<T>(m_xEntity, std::forward<Args>(args)..., GetComponent<Zenith_TransformComponent>(), this);
	}

	template<typename T, typename... Args>
	T& AddOrReplaceComponent(Args&&... args) {
		if (HasComponent<T>())
			RemoveComponent<T>();
		return m_pxParentScene->m_xRegistry.emplace<T>(m_xEntity, std::forward<Args>(args)..., GetComponent<Zenith_TransformComponent>(), this);
	}

	template<>
	Zenith_TransformComponent& AddComponent(const std::string& strName) {
		return m_pxParentScene->m_xRegistry.emplace<Zenith_TransformComponent>(m_xEntity, strName);
	}

	template<typename T>
	bool HasComponent() {
		return m_pxParentScene->m_xRegistry.all_of<T>(m_xEntity);
	}

	template<typename T>
	T& GetComponent() {
		VCE_Assert(HasComponent<T>(), "Doesn't have this component");
		return m_pxParentScene->m_xRegistry.get<T>(m_xEntity);
	}

	template<typename T>
	void RemoveComponent() {
		VCE_Assert(HasComponent<T>(), "Doesn't have this component");
		m_pxParentScene->m_xRegistry.remove<T>(m_xEntity);
	}

	EntityID GetEntityID() { return m_xEntity; }
	
	const Zenith_GUID GetGuid() const { return m_ulGUID; }
	class Scene* m_pxParentScene;

	void Serialize(std::ofstream& xOut);

	Zenith_GUID m_xParentEntityGUID = Zenith_GUID::Invalid;
	std::string m_strName;
private:
	EntityID m_xEntity;
	
	Zenith_GUID m_ulGUID;
	
};



