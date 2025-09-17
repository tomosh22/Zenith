#pragma once
#include "EntityComponent/Zenith_Scene.h"

class Zenith_Entity
{
public:
	Zenith_Entity() = default;
	Zenith_Entity(Zenith_Scene* pxScene, const std::string& strName);
	Zenith_Entity(Zenith_Scene* pxScene, Zenith_EntityID xGUID, Zenith_EntityID uParentID, const std::string& strName);
	void Initialise(Zenith_Scene* pxScene, const std::string& strName);
	void Initialise(Zenith_Scene* pxScene, Zenith_EntityID xGUID, Zenith_EntityID uParentID, const std::string& strName);

	template<typename T, typename... Args>
	T& AddComponent(Args&&... args)
	{
		Zenith_Assert(!HasComponent<T>(), "Already has this component");
		return m_pxParentScene->CreateComponent<T>(m_uEntityID, std::forward<Args>(args)..., *this);
	}

	template<typename T, typename... Args>
	T& AddOrReplaceComponent(Args&&... args)
	{
		if (HasComponent<T>())
		{
			RemoveComponent<T>();
		}
		return AddComponent<T>(args);
	}

	template<typename T>
	bool HasComponent() const
	{
		return m_pxParentScene->EntityHasComponent<T>(m_uEntityID);
	}

	template<typename T>
	T& GetComponent() const
	{
		Zenith_Assert(HasComponent<T>(), "Doesn't have this component");
		return m_pxParentScene->GetComponentFromEntity<T>(m_uEntityID);
	}

	template<typename T>
	void RemoveComponent()
	{
		Zenith_Assert(HasComponent<T>(), "Doesn't have this component");
		m_pxParentScene->RemoveComponentFromEntity<T>(m_uEntityID);
	}

	Zenith_EntityID GetEntityID() { return m_uEntityID; }
	class Zenith_Scene* m_pxParentScene;

	void Serialize(std::ofstream& xOut);

	Zenith_EntityID m_uParentEntityID = -1;
	std::string m_strName;
private:
	Zenith_EntityID m_uEntityID;

	bool m_bInitialised = false;
};
