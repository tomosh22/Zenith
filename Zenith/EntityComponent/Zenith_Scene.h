#pragma once

#include "Collections/Zenith_Vector.h"
#include <cstdint>
#include <functional>  // Required for std::hash<Zenith_Scene> specialization (NOT for std::function)
#include <string>

// Forward declarations
class Zenith_SceneData;
class Zenith_Entity;
struct Zenith_EntityID;

/**
 * Zenith_Scene - Lightweight scene handle
 *
 * This is a VALUE TYPE that can be freely copied. It only holds an integer handle
 * that references scene data managed by Zenith_SceneManager.
 *
 * All scene state (entities, components, metadata) is stored in Zenith_SceneData
 * and accessed through Zenith_SceneManager.
 *
 * Usage:
 *   Zenith_Scene xScene = Zenith_SceneManager::GetActiveScene();
 *   Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);
 *   Zenith_Entity xEntity(pxData, "MyEntity");  // Creates entity with transform
 */
struct Zenith_Scene
{
	int m_iHandle = -1;
	uint32_t m_uGeneration = 0;  // Generation counter for stale handle detection

	//==========================================================================
	// Validity Check
	//==========================================================================

	/**
	 * Check if this scene handle is valid and not stale.
	 * Uses generation counter to detect handles to unloaded/reused scenes.
	 */
	bool IsValid() const;

	//==========================================================================
	// Properties (delegate to SceneData via SceneManager)
	//==========================================================================

	/**
	 * Get the scene's build index (-1 if not in build settings)
	 */
	int GetBuildIndex() const;

	/**
	 * Get the scene handle
	 */
	int GetHandle() const { return m_iHandle; }

#ifdef ZENITH_TOOLS
	/**
	 * Check if scene has unsaved changes
	 */
	bool HasUnsavedChanges() const;
#endif

	/**
	 * Check if scene is fully loaded
	 */
	bool IsLoaded() const;

	/**
	 * Check if this scene was loaded with additive mode
	 *
	 * Returns true if the scene was loaded with SCENE_LOAD_ADDITIVE mode.
	 * Returns false if loaded with SCENE_LOAD_SINGLE mode.
	 *
	 * Note: This differs from Unity's isSubScene property (which relates to
	 * scene streaming). WasLoadedAdditively simply indicates whether
	 * the scene was loaded additively alongside other scenes.
	 */
	bool WasLoadedAdditively() const;

	/**
	 * Get the scene name
	 */
	const std::string& GetName() const;

	/**
	 * Get the scene file path
	 */
	const std::string& GetPath() const;

	/**
	 * Get the number of root entities in the scene
	 */
	uint32_t GetRootEntityCount() const;

	/**
	 * Get all root entities in the scene
	 */
	void GetRootEntities(Zenith_Vector<Zenith_Entity>& axOut) const;

	//==========================================================================
	// Comparison Operators
	//==========================================================================

	/**
	 * Compares both handle AND generation.
	 * Stale handles (from unloaded/reused scenes) are never equal to new scenes.
	 */
	bool operator==(const Zenith_Scene& xOther) const
	{
		return m_iHandle == xOther.m_iHandle && m_uGeneration == xOther.m_uGeneration;
	}
	bool operator!=(const Zenith_Scene& xOther) const { return !(*this == xOther); }

	// Sentinel value for invalid scene references (defined below after struct is complete)
	static const Zenith_Scene INVALID_SCENE;
};

// Definition of INVALID_SCENE (must be after struct is complete)
inline constexpr Zenith_Scene Zenith_Scene::INVALID_SCENE = { -1, 0 };

namespace std
{
	template<>
	struct hash<Zenith_Scene>
	{
		size_t operator()(const Zenith_Scene& xScene) const
		{
			// Pack handle + generation into uint64_t (same pattern as Zenith_EntityID)
			uint64_t ulPacked = (static_cast<uint64_t>(static_cast<uint32_t>(xScene.m_iHandle)) << 32)
				| static_cast<uint64_t>(xScene.m_uGeneration);
			return std::hash<uint64_t>{}(ulPacked);
		}
	};
}

//==============================================================================
// Include SceneData for template implementations
// This must come AFTER Zenith_Scene is defined
//==============================================================================
#include "EntityComponent/Zenith_SceneData.h"

//==============================================================================
// Zenith_Entity template implementations
// These are placed here after both Zenith_Scene and Zenith_SceneData are defined
//==============================================================================
#include "EntityComponent/Zenith_Entity.h"

template<typename T, typename... Args>
T& Zenith_Entity::AddComponent(Args&&... args)
{
	Zenith_SceneData* pxSceneData = GetSceneData();
	Zenith_Assert(pxSceneData != nullptr, "AddComponent: Entity has no scene");
	Zenith_Assert(pxSceneData->m_bIsLoaded, "AddComponent: Entity's scene is not loaded");
	Zenith_Assert(!pxSceneData->m_bIsUnloading, "AddComponent: Cannot add component during scene unload");
	Zenith_Assert(pxSceneData->EntityExists(m_xEntityID), "AddComponent: Entity (idx=%u, gen=%u) is stale", m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);

	const Zenith_SceneData::TypeID uTypeID = Zenith_SceneData::TypeIDGenerator::GetTypeID<T>();
	const std::unordered_map<Zenith_SceneData::TypeID, u_int>& xComponentsForThisEntity =
		Zenith_SceneData::s_axEntityComponents.Get(m_xEntityID.m_uIndex);
	Zenith_Assert(!xComponentsForThisEntity.contains(uTypeID), "AddComponent: Entity already has this component type");

	return pxSceneData->CreateComponent<T>(m_xEntityID, std::forward<Args>(args)..., *this);
}

template<typename T, typename... Args>
T& Zenith_Entity::AddOrReplaceComponent(Args&&... args)
{
	if (HasComponent<T>())
	{
		RemoveComponent<T>();
	}
	return AddComponent<T>(std::forward<Args>(args)...);
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
	Zenith_Assert(pxSceneData->m_bIsLoaded, "GetComponent: Entity's scene is not loaded");

	return pxSceneData->GetComponentFromEntity<T>(m_xEntityID);
}

template<typename T>
T* Zenith_Entity::TryGetComponent() const
{
	Zenith_SceneData* pxSceneData = GetSceneData();
	if (pxSceneData == nullptr) return nullptr;
	if (!pxSceneData->m_bIsLoaded) return nullptr;
	if (!pxSceneData->EntityExists(m_xEntityID)) return nullptr;
	if (!pxSceneData->EntityHasComponent<T>(m_xEntityID)) return nullptr;

	return &pxSceneData->GetComponentFromEntity<T>(m_xEntityID);
}

template<typename T>
void Zenith_Entity::RemoveComponent()
{
	Zenith_SceneData* pxSceneData = GetSceneData();
	Zenith_Assert(pxSceneData != nullptr, "RemoveComponent: Entity has no scene");
	Zenith_Assert(pxSceneData->m_bIsLoaded || pxSceneData->m_bIsUnloading, "RemoveComponent: Entity's scene is not loaded");
	// Note: Removing components during unload IS allowed (it's part of cleanup)
	// Only adding components during unload is forbidden
	Zenith_Assert(pxSceneData->EntityExists(m_xEntityID), "RemoveComponent: Entity (idx=%u, gen=%u) is stale", m_xEntityID.m_uIndex, m_xEntityID.m_uGeneration);
	Zenith_Assert(pxSceneData->EntityHasComponent<T>(m_xEntityID), "RemoveComponent: Entity does not have this component type");

	pxSceneData->RemoveComponentFromEntity<T>(m_xEntityID);
}
