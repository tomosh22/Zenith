#pragma once

#include "Collections/Zenith_Vector.h"
#include "Core/Multithreading/Zenith_Multithreading.h"
#include <atomic>
#include <mutex>

class Zenith_CameraComponent;
class Zenith_Entity;

using Zenith_EntityID = u_int;
constexpr Zenith_EntityID INVALID_ENTITY_ID = static_cast<Zenith_EntityID>(-1);

class Zenith_ComponentPoolBase
{
public:
	virtual ~Zenith_ComponentPoolBase() = default;
};

template<typename T>
class Zenith_ComponentPool : public Zenith_ComponentPoolBase
{
public:
	Zenith_Vector<T> m_xData;
};

// Forward declare Zenith_Scene so Zenith_Entity can reference it
class Zenith_Scene;

// Include Zenith_Entity to get complete type definition for m_xEntityMap
#include "EntityComponent/Zenith_Entity.h"

// Define the component concept after Zenith_Entity is fully defined
template<typename T>
concept Zenith_Component =
// Component must be constructible from an entity reference
// This matches the existing pattern where components store their parent entity
std::is_constructible_v<T, Zenith_Entity&>&&
// Component must be destructible
std::is_destructible_v<T>
	#ifdef ZENITH_TOOLS
	&&
// Component must have a RenderPropertiesPanel method for editor UI
// This method is responsible for rendering the component's properties in ImGui
	requires(T& t) { { t.RenderPropertiesPanel() } -> std::same_as<void>; }&&
// Component must have a static RegisterWithEditor function for self-registration
	requires() { { T::RegisterWithEditor() } -> std::same_as<void>; }
	#endif
	;

class Zenith_Scene
{
public:

	using TypeID = u_int;
	class TypeIDGenerator
	{
	public:
		template<Zenith_Component T>
		static TypeID GetTypeID()
		{
			static TypeID ls_uRet = s_uCounter++;

			#ifdef ZENITH_TOOLS
			static bool ls_bRegistered = false;
			static bool ls_bInRegistration = false;
			
			if (!ls_bRegistered && !ls_bInRegistration)
			{
				Zenith_Assert(Zenith_Multithreading::IsMainThread(), "TypeIDs must be registered on main thread");
				ls_bInRegistration = true;
				T::RegisterWithEditor();
				ls_bRegistered = true;
				ls_bInRegistration = false;
			}
			#endif

			return ls_uRet;
		}
	private:
		inline static TypeID s_uCounter = 0;
	};

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

	Zenith_EntityID CreateEntity()
	{
		// Entity ID counter per-scene (starts at 1 because 0 is reserved as invalid)
		while (m_xEntityComponents.GetSize() <= m_uNextEntityID)
		{
			m_xEntityComponents.PushBack({});
		}
		return m_uNextEntityID++;
	}

	template<typename T, typename... Args>
	T& CreateComponent(Zenith_EntityID uID, Args&&... args)
	{
		
		Zenith_ComponentPool<T>* const pxPool = GetComponentPool<T>();
		u_int uIndex = pxPool->m_xData.GetSize();
		pxPool->m_xData.EmplaceBack(std::forward<Args>(args)...);

		const Zenith_Scene::TypeID uTypeID = Zenith_Scene::TypeIDGenerator::GetTypeID<T>();
		std::unordered_map<Zenith_Scene::TypeID, u_int>& xComponentsForThisEntity = m_xEntityComponents.Get(uID);
		Zenith_Assert(xComponentsForThisEntity.find(uTypeID) == xComponentsForThisEntity.end(), "This component already has this entity");

		xComponentsForThisEntity[uTypeID] = uIndex;

		T& xRet = GetComponentFromPool<T>(uIndex);
		return xRet;
	}

	template<typename T>
	bool EntityHasComponent(Zenith_EntityID uID)
	{
		const Zenith_Scene::TypeID uTypeID = Zenith_Scene::TypeIDGenerator::GetTypeID<T>();
		std::unordered_map<Zenith_Scene::TypeID, u_int>& xComponentsForThisEntity = m_xEntityComponents.Get(uID);
		return xComponentsForThisEntity.contains(uTypeID);
	}

	template<typename T>
	T& GetComponentFromEntity(Zenith_EntityID uID)
	{
		const Zenith_Scene::TypeID uTypeID = Zenith_Scene::TypeIDGenerator::GetTypeID<T>();
		std::unordered_map<Zenith_Scene::TypeID, u_int>& xComponentsForThisEntity = m_xEntityComponents.Get(uID);
		const u_int uIndex = xComponentsForThisEntity.at(uTypeID);
		return GetComponentPool<T>()->m_xData.Get(uIndex);
	}

	template<typename T>
	bool RemoveComponentFromEntity(Zenith_EntityID uID)
	{
		const Zenith_Scene::TypeID uTypeID = Zenith_Scene::TypeIDGenerator::GetTypeID<T>();
		std::unordered_map<Zenith_Scene::TypeID, u_int>& xComponentsForThisEntity = m_xEntityComponents.Get(uID);
		const u_int uIndex = xComponentsForThisEntity.at(uTypeID);
		xComponentsForThisEntity.erase(uTypeID);
		GetComponentPool<T>()->m_xData.Remove(uIndex);
	}

	template<typename T>
	void GetAllOfComponentType(Zenith_Vector<T*>& xOut)
	{
		for (typename Zenith_Vector<T>::Iterator xIt(GetComponentPool<T>()->m_xData); !xIt.Done(); xIt.Next())
		{
			xOut.PushBack(&xIt.GetData());
		}
	}

	// Serialization methods
	void SaveToFile(const std::string& strFilename);
	void LoadFromFile(const std::string& strFilename);

	// Entity management
	void RemoveEntity(Zenith_EntityID uID);

	// Query methods
	u_int GetEntityCount() const { return static_cast<u_int>(m_xEntityMap.size()); }
	bool EntityExists(Zenith_EntityID uID) const { return m_xEntityMap.find(uID) != m_xEntityMap.end(); }
	Zenith_Entity GetEntityFromID(Zenith_EntityID uID);

	static void Update(const float fDt);
	static void WaitForUpdateComplete();

	Zenith_Entity GetEntityByID(Zenith_EntityID ulGuid);

	static Zenith_Scene& GetCurrentScene() { return s_xCurrentScene; }

	void SetMainCameraEntity(Zenith_Entity& xEntity);
	Zenith_CameraComponent& GetMainCamera();

	// Scene loading state (prevents asset deletion during Reset())
	static bool IsLoadingScene() { return s_bIsLoadingScene; }
private:
	static bool s_bIsLoadingScene;
	friend class Zenith_Entity;
#ifdef ZENITH_TOOLS
	friend class Zenith_Editor;
	friend class Zenith_SelectionSystem;
#endif

	template<typename T>
	T& GetComponentFromPool(u_int uIndex)
	{
		const TypeID uTypeID = TypeIDGenerator::GetTypeID<T>();
		Zenith_ComponentPool<T>* pxPool = static_cast<Zenith_ComponentPool<T>*>(m_xComponents.Get(uTypeID));
		return pxPool->m_xData.Get(uIndex);
	}

	template<typename T>
	Zenith_ComponentPool<T>* GetComponentPool()
	{
		const Zenith_Scene::TypeID uTypeID = Zenith_Scene::TypeIDGenerator::GetTypeID<T>();
		while (m_xComponents.GetSize() <= uTypeID)
		{
			m_xComponents.PushBack(nullptr);
		}
		Zenith_ComponentPoolBase*& pxPoolBase = static_cast<Zenith_ComponentPoolBase*&>(m_xComponents.Get(uTypeID));
		if (pxPoolBase == nullptr)
		{
			pxPoolBase = new Zenith_ComponentPool<T>;
		}
		return static_cast<Zenith_ComponentPool<T>*>(pxPoolBase);
	}

	std::unordered_map<Zenith_EntityID, Zenith_Entity> m_xEntityMap;
	static Zenith_Scene s_xCurrentScene;
	Zenith_Entity* m_pxMainCameraEntity;
	Zenith_Mutex m_xMutex;
	Zenith_EntityID m_uNextEntityID = 1;  // Starts at 1 (0 is reserved as invalid)

	public:
	//#TO type id is index into vector
	Zenith_Vector<Zenith_ComponentPoolBase*> m_xComponents;

	//#TO EntityID is index into vector
	Zenith_Vector<std::unordered_map<Zenith_Scene::TypeID, u_int>> m_xEntityComponents;
};

// Zenith_Entity template implementations (placed here after Zenith_Scene is fully defined)
template<typename T, typename... Args>
T& Zenith_Entity::AddComponent(Args&&... args)
{
	Zenith_Assert(!HasComponent<T>(), "Already has this component");
	return m_pxParentScene->CreateComponent<T>(m_uEntityID, std::forward<Args>(args)..., *this);
}

template<typename T, typename... Args>
T& Zenith_Entity::AddOrReplaceComponent(Args&&... args)
{
	if (HasComponent<T>())
	{
		RemoveComponent<T>();
	}
	return AddComponent<T>(args);
}

template<typename T>
bool Zenith_Entity::HasComponent() const
{
	return m_pxParentScene->EntityHasComponent<T>(m_uEntityID);
}

template<typename T>
T& Zenith_Entity::GetComponent() const
{
	Zenith_Assert(HasComponent<T>(), "Doesn't have this component");
	return m_pxParentScene->GetComponentFromEntity<T>(m_uEntityID);
}

template<typename T>
void Zenith_Entity::RemoveComponent()
{
	Zenith_Assert(HasComponent<T>(), "Doesn't have this component");
	m_pxParentScene->RemoveComponentFromEntity<T>(m_uEntityID);
}
