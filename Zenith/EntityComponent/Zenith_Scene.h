#pragma once

#include "Collections/Zenith_Vector.h"

class Zenith_CameraComponent;
class Zenith_Entity;

using Zenith_EntityID = u_int;

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

class Zenith_Scene
{
public:

	using TypeID = u_int;
	class TypeIDGenerator
	{
	public:
		template<typename T>
		static TypeID GetTypeID()
		{
			static TypeID ls_uRet = s_uCounter++;
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
		static Zenith_EntityID ls_uCount = 1;
		while (m_xEntityComponents.GetSize() <= ls_uCount)
		{
			m_xEntityComponents.PushBack({});
		}
		return ls_uCount++;
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

	// Query methods
	u_int GetEntityCount() const { return static_cast<u_int>(m_xEntityMap.size()); }

	static void Update(const float fDt);
	static void WaitForUpdateComplete();

	Zenith_Entity GetEntityByID(Zenith_EntityID ulGuid);

	static Zenith_Scene& GetCurrentScene() { return s_xCurrentScene; }

	void SetMainCameraEntity(Zenith_Entity& xEntity);
	Zenith_CameraComponent& GetMainCamera();
private:
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

	public:
	//#TO type id is index into vector
	Zenith_Vector<Zenith_ComponentPoolBase*> m_xComponents;

	//#TO EntityID is index into vector
	Zenith_Vector<std::unordered_map<Zenith_Scene::TypeID, u_int>> m_xEntityComponents;
};
