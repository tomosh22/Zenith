#pragma once

#include <cstdint>
#include <functional>  // Required for std::hash<Zenith_Scene> specialization (NOT for std::function)

// Forward declarations
class Zenith_SceneData;
class Zenith_Entity;
struct Zenith_EntityID;

// std::hash<Zenith_Scene> (defined at the bottom) is befriended below so it can
// pack the sealed handle + generation into the hash key exactly as before. The
// primary std::hash template is already visible via <functional> included above.

/**
 * Zenith_Scene - Opaque, lightweight scene handle
 *
 * This is a VALUE TYPE that can be freely copied. It holds only an integer
 * handle (+ a generation counter for stale-handle detection) that references
 * scene data managed by Zenith_SceneSystem.
 *
 * The handle is OPAQUE: it exposes only identity + validity. All scene state
 * (entities, components, metadata such as name / path / build index / loaded
 * flags / root-entity count) lives in Zenith_SceneData and is reached through
 * Zenith_SceneSystem (the engine exposes it as Scenes()):
 *   - Metadata snapshot: Zenith_SceneSystem::GetSceneInfo(xScene) -> Zenith_SceneInfo
 *   - Everything else:   the rest of the Zenith_SceneSystem surface.
 *
 * Usage (via the engine's scene-system accessor, Scenes()):
 *   Zenith_Scene xScene = Scenes().GetActiveScene();
 *   Zenith_Entity xEntity = Scenes().CreateEntity(xScene, "MyEntity");
 */
struct Zenith_Scene
{
public:
	// Value type: default-constructible (invalid handle), freely copyable.
	constexpr Zenith_Scene() = default;

	//==========================================================================
	// Validity Check
	//==========================================================================

	/**
	 * Check if this scene handle is valid and not stale.
	 * Uses generation counter to detect handles to unloaded/reused scenes.
	 */
	bool IsValid() const;

	/**
	 * Get the raw scene handle. The handle storage is otherwise sealed — only the
	 * ECS scene system (Zenith_SceneSystem) reaches m_iHandle directly.
	 */
	int GetHandle() const { return m_iHandle; }

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

private:
	// Sealed handle storage. The ECS scene system allocates / frees handles and
	// fills these in; Zenith_SceneData reconstructs a handle from its own slot
	// during deserialization. External code uses GetHandle() / operator== /
	// IsValid(). std::hash<Zenith_Scene> is a friend so the existing
	// handle+generation packing is unchanged.
	friend class Zenith_SceneSystem;
	friend class Zenith_SceneData;
	friend struct std::hash<Zenith_Scene>;

	// Private constructor used only for the INVALID_SCENE constant + by friends.
	constexpr Zenith_Scene(int iHandle, uint32_t uGeneration)
		: m_iHandle(iHandle), m_uGeneration(uGeneration) {}

	int m_iHandle = -1;
	uint32_t m_uGeneration = 0;  // Generation counter for stale handle detection
};

// Definition of INVALID_SCENE (must be after struct is complete). Uses the
// private (handle, generation) constructor — accessible here because this is a
// definition of Zenith_Scene's own static member.
inline constexpr Zenith_Scene Zenith_Scene::INVALID_SCENE{ -1, 0 };

namespace std
{
	template<>
	struct hash<Zenith_Scene>
	{
		size_t operator()(const Zenith_Scene& xScene) const
		{
			// Pack handle + generation into uint64_t (same pattern as Zenith_EntityID).
			uint64_t ulPacked = (static_cast<uint64_t>(static_cast<uint32_t>(xScene.m_iHandle)) << 32)
				| static_cast<uint64_t>(xScene.m_uGeneration);
			return std::hash<uint64_t>{}(ulPacked);
		}
	};
}
