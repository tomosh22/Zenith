#pragma once
#include "entt/entt.hpp"

class ColliderComponent;
class Entity;

using EntityRegistry = entt::registry;
using EntityID = entt::entity;

class Scene
{
public:
	Scene(const std::string& strFilename);
	~Scene();
	void Reset();

	void LoadSceneFromFile(const std::string& strFilename);

	template<typename T>
	T& GetComponentFromEntity(EntityID xID) {
		VCE_Assert(EntityHasComponent<T>(xID), "Doesn't have this component");
		return m_xRegistry.get<T>(xID);
	}

	template<typename T>
	bool EntityHasComponent(EntityID xID) const {
		return m_xRegistry.all_of<T>(xID);
	}

	template<typename T>
	std::vector<T*> GetAllOfComponentType() {
		std::vector<T*> xRet;
		auto view = m_xRegistry.view<T>();
		for (auto [xEntity, xComponent] : view.each())
			xRet.push_back(&xComponent);
		return xRet;
	}

	void Serialize(const std::string& strFilename);
	
	std::vector<ColliderComponent*> GetAllColliderComponents();

	Entity GetEntityByGUID(Zenith_GUID ulGuid);
private:
	friend class Entity;
	EntityRegistry m_xRegistry;
	std::unordered_map<GUIDType, Entity> m_xEntityMap;
};


