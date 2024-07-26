#pragma once
#include "entt/entt.hpp"

class Zenith_CameraComponent;
class Zenith_Entity;

using EntityRegistry = entt::registry;
using EntityID = entt::entity;

class Zenith_Scene
{
public:
	Zenith_Scene() = default;
	Zenith_Scene(const std::string& strFilename);
	~Zenith_Scene();
	void Reset();

	void LoadSceneFromFile(const std::string& strFilename);

	template<typename T>
	T& GetComponentFromEntity(EntityID xID)
	{
		Zenith_Assert(EntityHasComponent<T>(xID), "Doesn't have this component");
		return m_xRegistry.get<T>(xID);
	}

	template<typename T>
	bool EntityHasComponent(EntityID xID) const
	{
		return m_xRegistry.all_of<T>(xID);
	}

	template<typename T>
	void GetAllOfComponentType(std::vector<T*>& xOut)
	{
		auto view = m_xRegistry.view<T>();
		for (auto [xEntity, xComponent] : view.each())
		{
			xOut.push_back(&xComponent);
		}
	}

	void Serialize(const std::string& strFilename);

	void Update(const float fDt);

	Zenith_Entity GetEntityByGUID(Zenith_GUID ulGuid);

	static Zenith_Scene& GetCurrentScene() { return s_xCurrentScene; }

	void SetMainCameraEntity(Zenith_Entity& xEntity);
	Zenith_CameraComponent& GetMainCamera();
private:
	friend class Zenith_Entity;
	EntityRegistry m_xRegistry;
	std::unordered_map<GUIDType, Zenith_Entity> m_xEntityMap;
	static Zenith_Scene s_xCurrentScene;
	EntityID m_uMainCameraEntity = (EntityID)0;
};
