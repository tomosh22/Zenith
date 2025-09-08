#pragma once
#include "EntityComponent/Zenith_Scene.h"

class Zenith_Entity
{
public:
	Zenith_Entity() = default;
	Zenith_Entity(Zenith_Scene* pxScene, const std::string& strName);
	Zenith_Entity(Zenith_Scene* pxScene, Zenith_GUID xGUID, Zenith_GUID xParentGUID, const std::string& strName);
	void Initialise(Zenith_Scene* pxScene, const std::string& strName);
	void Initialise(Zenith_Scene* pxScene, Zenith_GUID xGUID, Zenith_GUID xParentGUID, const std::string& strName);

	template<typename T, typename... Args>
	T& AddComponent(Args&&... args)
	{
		Zenith_Assert(!HasComponent<T>(), "Already has this component");
		return m_pxParentScene->m_xRegistry.emplace<T>(m_xEntity, std::forward<Args>(args)..., *this);
	}

	template<typename T, typename... Args>
	T& AddOrReplaceComponent(Args&&... args)
	{
		if (HasComponent<T>())
			RemoveComponent<T>();
		return m_pxParentScene->m_xRegistry.emplace<T>(m_xEntity, std::forward<Args>(args)..., *this);
	}

	template<typename T>
	bool HasComponent() const
	{
		return m_pxParentScene->m_xRegistry.all_of<T>(m_xEntity);
	}

	template<typename T>
	T& GetComponent() const
	{
		Zenith_Assert(HasComponent<T>(), "Doesn't have this component");
		return m_pxParentScene->m_xRegistry.get<T>(m_xEntity);
	}

	template<typename T>
	void RemoveComponent()
	{
		Zenith_Assert(HasComponent<T>(), "Doesn't have this component");
		m_pxParentScene->m_xRegistry.remove<T>(m_xEntity);
	}

	EntityID GetEntityID() { return m_xEntity; }

	Zenith_GUID GetGUID() { return m_ulGUID; }
	const Zenith_GUID GetGUID() const { return m_ulGUID; }
	class Zenith_Scene* m_pxParentScene;

	void Serialize(std::ofstream& xOut);

	Zenith_GUID m_xParentEntityGUID = Zenith_GUID::Invalid;
	std::string m_strName;
private:
	EntityID m_xEntity;

	Zenith_GUID m_ulGUID;

	bool m_bInitialised = false;
};
