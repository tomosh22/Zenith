#pragma once
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "entt/entt.hpp"
#include "Memory/Zenith_MemoryManagement_Enabled.h"

#include "Collections/Zenith_Vector.h"

class Zenith_CameraComponent;
class Zenith_Entity;

using EntityRegistry = entt::registry;
using EntityID = entt::entity;

class Zenith_Scene
{
public:
	Zenith_Scene();
	~Zenith_Scene();
	void Reset();

	void AcquireMutex()
	{
		m_xMutex.Lock();
	}
	void ReleaseMutex()
	{
		m_xMutex.Unlock();
	}

	template<typename T>
	void GetAllOfComponentType(Zenith_Vector<T*>& xOut)
	{
		//#TO_TODO: assert that we have acquired the mutex
		auto view = m_xRegistry.view<T>();
		for (auto [xEntity, xComponent] : view.each())
		{
			xOut.PushBack(&xComponent);
		}
	}

	void Serialize(const std::string& strFilename);

	static void Update(const float fDt);
	static void WaitForUpdateComplete();

	Zenith_Entity GetEntityByGUID(Zenith_GUID ulGuid);

	static Zenith_Scene& GetCurrentScene() { return s_xCurrentScene; }

	void SetMainCameraEntity(Zenith_Entity& xEntity);
	Zenith_CameraComponent& GetMainCamera();
private:
	friend class Zenith_Entity;
	EntityRegistry m_xRegistry;
	std::unordered_map<GUIDType, Zenith_Entity> m_xEntityMap;
	static Zenith_Scene s_xCurrentScene;
	Zenith_Entity* m_pxMainCameraEntity;
	Zenith_Mutex m_xMutex;
};
