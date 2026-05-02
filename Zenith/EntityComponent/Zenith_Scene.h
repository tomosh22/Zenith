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
// Zenith_Entity template implementations live in Zenith_Entity.inl.
// They are included here (rather than from Zenith_Entity.h) because the bodies
// need Zenith_SceneData fully defined, which is only true at this point in the
// translation unit.
//==============================================================================
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Zenith_Entity.inl"
