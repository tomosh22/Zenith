#pragma once
/**
 * DPResources - per-archetype Zenith_Prefab templates, created once at startup
 * and instantiated by the runtime spawn paths (DPProcLevelBootstrap, the item
 * manager, the forge, the villager held-item visual).
 *
 * Lives in a header (not just DevilsPlayground.cpp) because several spawn sites
 * are header-only behaviours -- they need to see Resources() + the PrefabHandle
 * fields. Mirrors the resources-in-a-header pattern used by other games
 * (e.g. Runner_Behaviour.h's RunnerResources).
 *
 * Each template bakes Transform + Model(cube) + its collider; the per-instance
 * transform is supplied at Instantiate() time, and scripts / per-instance config
 * (tags, navmesh flags, tints) are applied after instantiation. Forge/NoiseMachine
 * have no collider (matching the pre-prefab behaviour); HeldVisual is model-only.
 */

#include "AssetHandling/Zenith_AssetHandle.h"   // PrefabHandle = Zenith_AssetHandle<Zenith_Prefab>

class Zenith_Prefab;

namespace DevilsPlayground
{
	struct DPResources
	{
		PrefabHandle m_xWallPrefab;          // OBB / STATIC
		PrefabHandle m_xVillagerPrefab;      // CAPSULE / DYNAMIC
		PrefabHandle m_xPriestPrefab;        // CAPSULE / DYNAMIC
		PrefabHandle m_xItemPrefab;          // SPHERE / STATIC (also forge output)
		PrefabHandle m_xDoorPrefab;          // OBB / STATIC
		PrefabHandle m_xChestPrefab;         // AABB / STATIC
		PrefabHandle m_xPentagramPrefab;     // AABB / STATIC
		PrefabHandle m_xForgePrefab;         // no collider
		PrefabHandle m_xNoiseMachinePrefab;  // no collider
		PrefabHandle m_xHeldVisualPrefab;    // model only (held-item marker)
	};

	// The single process-wide resources instance. Defined in DevilsPlayground.cpp.
	DPResources& Resources();
}
