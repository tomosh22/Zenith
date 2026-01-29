#include "Zenith.h"

#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_ComponentMeta.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Flux/MeshAnimation/Flux_MeshAnimation.h"
#include "Flux/Terrain/Flux_Terrain.h"
#include "Flux/StaticMeshes/Flux_StaticMeshes.h"
#include "Flux/AnimatedMeshes/Flux_AnimatedMeshes.h"
#include "Flux/Shadows/Flux_Shadows.h"
#include "Flux/Primitives/Flux_Primitives.h"
#include "Flux/Text/Flux_Text.h"
#include "Flux/Particles/Flux_Particles.h"
#include "Flux/Skybox/Flux_Skybox.h"
#include "Flux/DeferredShading/Flux_DeferredShading.h"
#include "Flux/HiZ/Flux_HiZ.h"
#include "Flux/SSR/Flux_SSR.h"
#include "Flux/SSAO/Flux_SSAO.h"
#include "Flux/Fog/Flux_Fog.h"
#include "Flux/SDFs/Flux_SDFs.h"
#include "Flux/Quads/Flux_Quads.h"
#ifdef ZENITH_TOOLS
#include "Flux/Gizmos/Flux_Gizmos.h"
#include "Editor/Zenith_Editor.h"
#endif
#include "Physics/Zenith_Physics.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#include "DataStream/Zenith_DataStream.h"
#include "Prefab/Zenith_Prefab.h"

#include <mutex>

Zenith_Scene Zenith_Scene::s_xCurrentScene;
bool Zenith_Scene::s_bIsLoadingScene = false;
bool Zenith_Scene::s_bIsPrefabInstantiating = false;
float Zenith_Scene::s_fFixedTimeAccumulator = 0.0f;

static constexpr float FIXED_TIMESTEP = 1.0f / 60.0f;  // 60 Hz fixed update

// Force link ScriptComponent to ensure its static registration runs
// This is needed because linker may not include translation units with no referenced symbols
namespace { struct ForceScriptComponentLink { ForceScriptComponentLink() { Zenith_ScriptComponent_ForceLink(); } } s_xForceLink; }

static Zenith_TaskArray* g_pxAnimUpdateTask = nullptr;
static Zenith_Vector<Flux_MeshAnimation*> g_xAnimationsToUpdate;
static std::once_flag g_xAnimTaskInitFlag;

enum class ComponentType {
	Transform,
	Model,
	Collider,
	Script,
	Terrain,
	Foliage
};

void AnimUpdateTask(void*, u_int uInvocationIndex, u_int uNumInvocations)
{
	const float fDt = Zenith_Core::GetDt();
	const u_int uTotalAnimations = g_xAnimationsToUpdate.GetSize();

	if (uInvocationIndex >= uTotalAnimations)
	{
		return;
	}

	const u_int uAnimsPerInvocation = (uTotalAnimations + uNumInvocations - 1) / uNumInvocations;
	const u_int uStartIndex = uInvocationIndex * uAnimsPerInvocation;
	const u_int uEndIndex = (uStartIndex + uAnimsPerInvocation < uTotalAnimations) ? 
		uStartIndex + uAnimsPerInvocation : uTotalAnimations;

	for (u_int u = uStartIndex; u < uEndIndex; u++)
	{
		Flux_MeshAnimation* pxAnim = g_xAnimationsToUpdate.Get(u);
		Zenith_Assert(pxAnim != nullptr, "Null animation")
		pxAnim->Update(fDt);
	}
}

Zenith_Scene::Zenith_Scene()
{
	// Thread-safe one-time initialization using std::call_once
	// This replaces the previous ls_bOnce pattern which had a data race
	std::call_once(g_xAnimTaskInitFlag, []() {
		// Create task array with 4 invocations for parallel processing
		// Enable submitting thread joining to utilize the main thread
		g_pxAnimUpdateTask = new Zenith_TaskArray(ZENITH_PROFILE_INDEX__ANIMATION, AnimUpdateTask, nullptr, 4, true);
		atexit([]() { delete g_pxAnimUpdateTask; });
	});
}

Zenith_Scene::~Zenith_Scene() {
	Reset();
}

void Zenith_Scene::Reset()
{
	// Set flag BEFORE destroying components - this tells component destructors
	// to skip hierarchy cleanup (which would try to acquire mutexes and access
	// scene data that may be in an inconsistent state or destroyed).
	m_bIsBeingDestroyed = true;

	for (Zenith_Vector<Zenith_ComponentPoolBase*>::Iterator xIt(m_xComponents); !xIt.Done(); xIt.Next())
	{
		Zenith_ComponentPoolBase* pxPool = xIt.GetData();
		if (pxPool)
		{
			delete pxPool;
		}
	}
	m_xComponents.Clear();
	m_xEntityComponents.Clear();
	m_xEntitySlots.Clear();
	m_xFreeIndices.Clear();
	m_xActiveEntities.Clear();
	m_xEntitiesStarted.clear();  // Clear started tracking
	m_xEntitiesAwoken.clear();   // Clear awoken tracking
	m_xPendingDestruction.Clear();  // Clear pending destructions
	m_xPendingDestructionSet.clear();
	m_xCreatedDuringUpdate.clear();  // Clear deferred creation tracking
	m_bIsUpdating = false;
	m_xMainCameraEntity = INVALID_ENTITY_ID;

	// Reset flag in case scene is reused (e.g., loading a new scene)
	m_bIsBeingDestroyed = false;
}

void Zenith_Scene::Destroy(Zenith_Entity& xEntity)
{
	Destroy(xEntity.GetEntityID());
}

void Zenith_Scene::Destroy(Zenith_EntityID xEntityID)
{
	if (!xEntityID.IsValid()) return;
	if (!s_xCurrentScene.EntityExists(xEntityID)) return;

	// Already marked for destruction?
	if (s_xCurrentScene.m_xPendingDestructionSet.count(xEntityID) > 0) return;

	// Mark for deferred destruction (both in slot and pending set)
	s_xCurrentScene.m_xEntitySlots.Get(xEntityID.m_uIndex).m_bMarkedForDestruction = true;
	s_xCurrentScene.m_xPendingDestruction.PushBack(xEntityID);
	s_xCurrentScene.m_xPendingDestructionSet.insert(xEntityID);

	// Also mark all children recursively (using EntityIDs - safe against pool relocations)
	Zenith_Entity xEntity = s_xCurrentScene.GetEntity(xEntityID);
	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	const Zenith_Vector<Zenith_EntityID>& xChildIDs = xTransform.GetChildEntityIDs();
	for (u_int u = 0; u < xChildIDs.GetSize(); ++u)
	{
		Destroy(xChildIDs.Get(u));
	}
}

void Zenith_Scene::DestroyImmediate(Zenith_Entity& xEntity)
{
	DestroyImmediate(xEntity.GetEntityID());
}

void Zenith_Scene::DestroyImmediate(Zenith_EntityID xEntityID)
{
	// Remove from pending if present
	s_xCurrentScene.m_xPendingDestructionSet.erase(xEntityID);

	// Immediate destruction
	s_xCurrentScene.RemoveEntity(xEntityID);
}

bool Zenith_Scene::IsMarkedForDestruction(Zenith_EntityID xEntityID) const
{
	if (!xEntityID.IsValid()) return false;
	if (xEntityID.m_uIndex >= m_xEntitySlots.GetSize()) return false;
	return m_xEntitySlots.Get(xEntityID.m_uIndex).m_bMarkedForDestruction;
}

void Zenith_Scene::ProcessPendingDestructions()
{
	// Process in reverse order (children before parents since children added after parents)
	for (int i = static_cast<int>(m_xPendingDestruction.GetSize()) - 1; i >= 0; --i)
	{
		Zenith_EntityID xEntityID = m_xPendingDestruction.Get(i);
		if (EntityExists(xEntityID))
		{
			// Detach from parent before destruction
			Zenith_Entity xEntity = GetEntity(xEntityID);
			xEntity.GetComponent<Zenith_TransformComponent>().DetachFromParent();

			RemoveEntity(xEntityID);
		}
	}

	m_xPendingDestruction.Clear();
	m_xPendingDestructionSet.clear();
}

//------------------------------------------------------------------------------
// Entity Management
//------------------------------------------------------------------------------

void Zenith_Scene::RemoveEntity(Zenith_EntityID xID)
{
	// Check if entity exists (with generation validation)
	if (!EntityExists(xID))
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "Attempted to remove non-existent entity (idx=%u, gen=%u)", xID.m_uIndex, xID.m_uGeneration);
		return;
	}

	Zenith_EntitySlot& xSlot = m_xEntitySlots.Get(xID.m_uIndex);

	// Clear the main camera reference if this is the camera entity
	if (m_xMainCameraEntity.IsValid() && m_xMainCameraEntity == xID)
	{
		m_xMainCameraEntity = INVALID_ENTITY_ID;
	}

	// Remove all components from the entity (calls OnDestroy and removes from pools)
	Zenith_Entity xEntity(this, xID);
	Zenith_ComponentMetaRegistry::Get().RemoveAllComponents(xEntity);

	// Clear component mapping for this slot
	m_xEntityComponents.Get(xID.m_uIndex).clear();

	// Remove from started tracking
	m_xEntitiesStarted.erase(xID);

	// Mark slot as free and add to free list
	xSlot.m_bOccupied = false;
	xSlot.m_bMarkedForDestruction = false;
	m_xFreeIndices.PushBack(xID.m_uIndex);

	// Remove from active entities list
	m_xActiveEntities.EraseValue(xID);

	Zenith_Log(LOG_CATEGORY_SCENE, "Entity (idx=%u, gen=%u) removed from scene", xID.m_uIndex, xID.m_uGeneration);
}

void Zenith_Scene::SaveToFile(const std::string& strFilename, bool bIncludeTransient)
{
	Zenith_DataStream xStream;

	// Write file header and version
	// Version 5: Generation counter entity IDs (stores index only, generation is runtime)
	static constexpr u_int SCENE_MAGIC = 0x5A53434E; // "ZSCN" in hex
	static constexpr u_int SCENE_VERSION = 5;
	xStream << SCENE_MAGIC;
	xStream << SCENE_VERSION;

	// Count entities to save
	// When bIncludeTransient is false: only save non-transient entities (normal scene save)
	// When bIncludeTransient is true: save ALL entities (editor backup for Play/Stop)
	u_int uNumEntities = 0;
	for (u_int u = 0; u < m_xActiveEntities.GetSize(); ++u)
	{
		Zenith_EntityID xID = m_xActiveEntities.Get(u);
		const Zenith_EntitySlot& xSlot = m_xEntitySlots.Get(xID.m_uIndex);
		if (bIncludeTransient || !xSlot.m_bTransient)
		{
			uNumEntities++;
		}
	}
	xStream << uNumEntities;

	// Write entities
	for (u_int u = 0; u < m_xActiveEntities.GetSize(); ++u)
	{
		Zenith_EntityID xID = m_xActiveEntities.Get(u);
		const Zenith_EntitySlot& xSlot = m_xEntitySlots.Get(xID.m_uIndex);
		// Skip transient entities unless bIncludeTransient is true
		if (!bIncludeTransient && xSlot.m_bTransient)
		{
			continue;
		}
		Zenith_Entity xEntity(this, xID);
		xEntity.WriteToDataStream(xStream);
	}

	// Write main camera entity index (generation is runtime only)
	uint32_t uMainCameraIndex = m_xMainCameraEntity.IsValid() ? m_xMainCameraEntity.m_uIndex : Zenith_EntityID::INVALID_INDEX;
	xStream << uMainCameraIndex;

	// Write to file
	xStream.WriteToFile(strFilename.c_str());
}

void Zenith_Scene::LoadFromFile(const std::string& strFilename)
{
	// CRITICAL: RAII guard sets loading flag BEFORE Reset() to prevent asset deletion
	// During Reset(), component destructors check this flag to avoid deleting
	// assets that will be needed when deserializing the scene.
	// The RAII guard ensures the flag is cleared even on early returns or assertions.
	SceneLoadingGuard xLoadGuard;

	// CRITICAL: Reset Flux render systems BEFORE clearing the scene
	// Command lists must be cleared before we destroy components/descriptors
	// Otherwise command lists have dangling pointers to destroyed descriptors
	// which causes access violations in UpdateDescriptorSets during IterateCommands
	Flux_Terrain::Reset();
	Flux_StaticMeshes::Reset();
	Flux_AnimatedMeshes::Reset();
	Flux_Shadows::Reset();  // Shadow cascades reference scene geometry
	Flux_Primitives::Reset();
	Flux_Text::Reset();
	Flux_Particles::Reset();
	Flux_Skybox::Reset();
	Flux_HiZ::Reset();
	Flux_SSR::Reset();
	Flux_DeferredShading::Reset();
	Flux_SSAO::Reset();
	Flux_Fog::Reset();
	Flux_SDFs::Reset();
	Flux_Quads::Reset();
#ifdef ZENITH_TOOLS
	Flux_Gizmos::Reset();   // Gizmos reference selected entity
#endif

	// Clear the current scene (destroys components and their descriptors)
	// Safe now because command lists no longer reference them
	// Note: This destroys collider components which remove their bodies from physics
	Reset();

	// Reset physics world to clear any stale bodies that weren't properly cleaned up
	// Must happen AFTER scene Reset() so collider destructors can remove their bodies first
	Zenith_Physics::Reset();

	// Read file into data stream
	Zenith_DataStream xStream;
	xStream.ReadFromFile(strFilename.c_str());

	// Read and validate file header
	u_int uMagicNumber;
	u_int uVersion;
	xStream >> uMagicNumber;
	xStream >> uVersion;

	static constexpr u_int SCENE_MAGIC = 0x5A53434E; // "ZSCN"
	static constexpr u_int SCENE_VERSION_CURRENT = 5;
	static constexpr u_int SCENE_VERSION_MIN_SUPPORTED = 3;

	if (uMagicNumber != SCENE_MAGIC)
	{
		Zenith_Assert(false, "Invalid scene file format");
		return;
	}

	if (uVersion > SCENE_VERSION_CURRENT)
	{
		Zenith_Assert(false, "Unsupported scene file version (newer than current)");
		return;
	}

	if (uVersion < SCENE_VERSION_MIN_SUPPORTED)
	{
		Zenith_Error(LOG_CATEGORY_SCENE, "[Scene] Unsupported legacy scene format version %u. Please re-export the scene.", uVersion);
		return;
	}

	// Read the number of entities
	u_int uNumEntities;
	xStream >> uNumEntities;

	// Map from file index to new EntityID (for hierarchy reconstruction)
	std::unordered_map<uint32_t, Zenith_EntityID> xFileIndexToNewID;

	// Read and reconstruct each entity
	for (u_int u = 0; u < uNumEntities; u++)
	{
		uint32_t uFileIndex;
		std::string strName;
		uint32_t uFileParentIndex = Zenith_EntityID::INVALID_INDEX;

		if (uVersion == 3)
		{
			// Old format (v3): Entity stores parent ID and child IDs as raw u_int
			xStream >> uFileIndex;
			xStream >> uFileParentIndex;
			xStream >> strName;

			// Read and discard child IDs (will rebuild from parent refs)
			uint32_t uChildCount = 0;
			xStream >> uChildCount;
			for (uint32_t i = 0; i < uChildCount; ++i)
			{
				uint32_t uChildIndex;
				xStream >> uChildIndex;
			}
		}
		else if (uVersion == 4)
		{
			// v4 format: Entity has ID and name, parent in Transform
			xStream >> uFileIndex;
			xStream >> strName;
		}
		else if (uVersion == 5)
		{
			// v5 format: Same as v4 (parent in Transform)
			xStream >> uFileIndex;
			xStream >> strName;
		}
		else
		{
			// This should be unreachable - the version check at the top of LoadFromFile()
			// ensures uVersion is in the valid range. If we get here, it means a new
			// version was added without updating this entity parsing code.
			Zenith_Assert(false, "LoadFromFile: Unhandled scene version %u - update entity parsing code", uVersion);
		}

		// Create entity with fresh generation
		Zenith_EntityID xNewID = CreateEntity();
		xFileIndexToNewID[uFileIndex] = xNewID;

		// Set entity state directly in the slot
		Zenith_EntitySlot& xSlot = m_xEntitySlots.Get(xNewID.m_uIndex);
		xSlot.m_strName = strName;
		xSlot.m_bEnabled = true;
		xSlot.m_bTransient = false;  // Loaded entities are persistent

		// Create entity handle and add TransformComponent
		Zenith_Entity xEntity(this, xNewID);
		xEntity.AddComponent<Zenith_TransformComponent>();

		// Deserialize components
		Zenith_ComponentMetaRegistry::Get().DeserializeEntityComponents(xEntity, xStream);

		// For v3 files: parent ID was in entity, store pending parent index
		if (uVersion == 3 && uFileParentIndex != Zenith_EntityID::INVALID_INDEX)
		{
			Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
			// Store file index temporarily - will be resolved after all entities loaded
			xTransform.SetPendingParentFileIndex(uFileParentIndex);
		}
	}

	// Rebuild hierarchy using the file index to new ID mapping
	for (u_int u = 0; u < m_xActiveEntities.GetSize(); ++u)
	{
		Zenith_EntityID xID = m_xActiveEntities.Get(u);
		Zenith_Entity xEntity = GetEntity(xID);
		Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();

		// Get the pending parent file index (set during deserialization)
		uint32_t uParentFileIndex = xTransform.GetPendingParentFileIndex();
		xTransform.ClearPendingParentFileIndex();

		if (uParentFileIndex != Zenith_EntityID::INVALID_INDEX)
		{
			auto it = xFileIndexToNewID.find(uParentFileIndex);
			if (it != xFileIndexToNewID.end())
			{
				Zenith_EntityID xParentID = it->second;
				if (EntityExists(xParentID))
				{
					// Set parent using new ID
					xTransform.SetParentByID(xParentID);
				}
				else
				{
					Zenith_Warning(LOG_CATEGORY_SCENE, "Entity has invalid parent - clearing");
				}
			}
			else
			{
				Zenith_Warning(LOG_CATEGORY_SCENE, "Entity has unmapped parent file index %u - clearing", uParentFileIndex);
			}
		}
	}

	// Read main camera entity
	if (uVersion >= 5)
	{
		// v5: Stores index only
		uint32_t uMainCameraFileIndex;
		xStream >> uMainCameraFileIndex;

		if (uMainCameraFileIndex != Zenith_EntityID::INVALID_INDEX)
		{
			auto it = xFileIndexToNewID.find(uMainCameraFileIndex);
			if (it != xFileIndexToNewID.end() && EntityExists(it->second))
			{
				m_xMainCameraEntity = it->second;
			}
		}
	}
	else
	{
		// v3/v4: Stored as raw u_int EntityID
		uint32_t uMainCameraFileIndex;
		xStream >> uMainCameraFileIndex;

		if (uMainCameraFileIndex != 0xFFFFFFFF)
		{
			auto it = xFileIndexToNewID.find(uMainCameraFileIndex);
			if (it != xFileIndexToNewID.end() && EntityExists(it->second))
			{
				m_xMainCameraEntity = it->second;
			}
		}
	}

	// Unity-style lifecycle: Only dispatch OnAwake/OnEnable at runtime or when entering Play mode.
	// In editor Stopped mode, scene loads but scripts remain "dormant" - lifecycle hooks run when Play is clicked.
#ifdef ZENITH_TOOLS
	bool bShouldDispatchLifecycle = (Zenith_Editor::GetEditorMode() != EditorMode::Stopped);
#else
	bool bShouldDispatchLifecycle = true;  // Always dispatch at runtime (no editor)
#endif

	if (bShouldDispatchLifecycle)
	{
		// Dispatch OnAwake/OnEnable for all loaded entities (Unity-style: after ALL entities instantiated)
		// This ensures entities can find each other in OnAwake since all are loaded
		Zenith_ComponentMetaRegistry& xRegistry = Zenith_ComponentMetaRegistry::Get();
		for (u_int u = 0; u < m_xActiveEntities.GetSize(); ++u)
		{
			Zenith_EntityID xEntityID = m_xActiveEntities.Get(u);
			if (EntityExists(xEntityID))
			{
				Zenith_Entity xEntity = GetEntity(xEntityID);
				xRegistry.DispatchOnAwake(xEntity);
			}
		}

		// OnEnable - only for enabled entities, after all OnAwake calls complete
		for (u_int u = 0; u < m_xActiveEntities.GetSize(); ++u)
		{
			Zenith_EntityID xEntityID = m_xActiveEntities.Get(u);
			if (EntityExists(xEntityID))
			{
				Zenith_Entity xEntity = GetEntity(xEntityID);
				if (xEntity.IsEnabled())
				{
					xRegistry.DispatchOnEnable(xEntity);
				}
				// Mark as awoken so Update() doesn't dispatch again
				m_xEntitiesAwoken.insert(xEntityID);
			}
		}
	}

	// Note: Loading flag is automatically cleared by SceneLoadingGuard destructor
}

void Zenith_Scene::Update(const float fDt)
{
	s_xCurrentScene.AcquireMutex();

	// Mark that we're updating - entities created during this will be tracked
	s_xCurrentScene.m_bIsUpdating = true;

	Zenith_ComponentMetaRegistry& xRegistry = Zenith_ComponentMetaRegistry::Get();

	// Snapshot entity IDs before iteration to prevent iterator invalidation
	// if entities are created during callbacks
	Zenith_Vector<Zenith_EntityID> xEntityIDs;
	xEntityIDs.Reserve(s_xCurrentScene.m_xActiveEntities.GetSize());
	for (u_int u = 0; u < s_xCurrentScene.m_xActiveEntities.GetSize(); ++u)
	{
		xEntityIDs.PushBack(s_xCurrentScene.m_xActiveEntities.Get(u));
	}

	// 1. OnAwake/OnEnable for runtime-created entities (not yet awoken)
	// Scene-loaded and prefab entities are already awoken during creation
	// Skip entities created during this Update frame
	for (u_int u = 0; u < xEntityIDs.GetSize(); ++u)
	{
		Zenith_EntityID uID = xEntityIDs.Get(u);
		// Skip if destroyed or created this frame
		if (!s_xCurrentScene.EntityExists(uID)) continue;
		if (s_xCurrentScene.WasCreatedDuringUpdate(uID)) continue;

		// Skip if already awoken (scene-loaded or prefab entities)
		if (s_xCurrentScene.m_xEntitiesAwoken.find(uID) != s_xCurrentScene.m_xEntitiesAwoken.end())
			continue;

		Zenith_Entity xEntity = s_xCurrentScene.GetEntity(uID);
		xRegistry.DispatchOnAwake(xEntity);
		if (xEntity.IsEnabled())
		{
			xRegistry.DispatchOnEnable(xEntity);
		}
		s_xCurrentScene.m_xEntitiesAwoken.insert(uID);
	}

	// 2. OnStart for new entities (entities not yet started, only if enabled)
	// Skip entities created during this Update frame
	for (u_int u = 0; u < xEntityIDs.GetSize(); ++u)
	{
		Zenith_EntityID uID = xEntityIDs.Get(u);
		// Skip if destroyed or created this frame
		if (!s_xCurrentScene.EntityExists(uID)) continue;
		if (s_xCurrentScene.WasCreatedDuringUpdate(uID)) continue;

		if (s_xCurrentScene.m_xEntitiesStarted.find(uID) == s_xCurrentScene.m_xEntitiesStarted.end())
		{
			Zenith_Entity xEntity = s_xCurrentScene.GetEntity(uID);
			// Only dispatch OnStart if entity is enabled (Unity behavior)
			if (xEntity.IsEnabled())
			{
				xRegistry.DispatchOnStart(xEntity);
				s_xCurrentScene.m_xEntitiesStarted.insert(uID);
			}
		}
	}

	// 3. OnFixedUpdate (physics timestep - 60Hz)
	s_fFixedTimeAccumulator += fDt;
	while (s_fFixedTimeAccumulator >= FIXED_TIMESTEP)
	{
		for (u_int u = 0; u < xEntityIDs.GetSize(); ++u)
		{
			Zenith_EntityID uID = xEntityIDs.Get(u);
			if (!s_xCurrentScene.EntityExists(uID)) continue;
			if (s_xCurrentScene.WasCreatedDuringUpdate(uID)) continue;

			Zenith_Entity xEntity = s_xCurrentScene.GetEntity(uID);
			if (!xEntity.IsEnabled()) continue;
			xRegistry.DispatchOnFixedUpdate(xEntity, FIXED_TIMESTEP);
		}
		s_fFixedTimeAccumulator -= FIXED_TIMESTEP;
	}

	// 4. OnUpdate (every frame)
	for (u_int u = 0; u < xEntityIDs.GetSize(); ++u)
	{
		Zenith_EntityID uID = xEntityIDs.Get(u);
		if (!s_xCurrentScene.EntityExists(uID)) continue;
		if (s_xCurrentScene.WasCreatedDuringUpdate(uID)) continue;

		Zenith_Entity xEntity = s_xCurrentScene.GetEntity(uID);
		if (!xEntity.IsEnabled()) continue;
		xRegistry.DispatchOnUpdate(xEntity, fDt);
	}

	// 5. OnLateUpdate (after all OnUpdate calls)
	for (u_int u = 0; u < xEntityIDs.GetSize(); ++u)
	{
		Zenith_EntityID uID = xEntityIDs.Get(u);
		if (!s_xCurrentScene.EntityExists(uID)) continue;
		if (s_xCurrentScene.WasCreatedDuringUpdate(uID)) continue;

		Zenith_Entity xEntity = s_xCurrentScene.GetEntity(uID);
		if (!xEntity.IsEnabled()) continue;
		xRegistry.DispatchOnLateUpdate(xEntity, fDt);
	}

	// 5. Process deferred destructions (Unity-style end-of-frame cleanup)
	s_xCurrentScene.ProcessPendingDestructions();

	// End update cycle - clear creation tracking for next frame
	s_xCurrentScene.m_bIsUpdating = false;
	s_xCurrentScene.m_xCreatedDuringUpdate.clear();

	s_xCurrentScene.ReleaseMutex();

	// ===========================================================================
	// Animation Update (parallel task system)
	// ===========================================================================
	// Note: This runs after script updates to avoid vector resize issues
	// when scripts add new model components

	g_xAnimationsToUpdate.Clear();
	Zenith_Vector<Zenith_ModelComponent*> xModels;
	s_xCurrentScene.GetAllOfComponentType<Zenith_ModelComponent>(xModels);
	for (Zenith_Vector<Zenith_ModelComponent*>::Iterator xIt(xModels); !xIt.Done(); xIt.Next())
	{
		Zenith_ModelComponent* pxModel = xIt.GetData();

		// New model instance system: Update the animation controller and skeleton
		if (pxModel->IsUsingModelInstance())
		{
			pxModel->Update(fDt);
		}

		// Legacy system: Collect animations for parallel update task
		for (u_int uMesh = 0; uMesh < pxModel->GetNumMeshEntries(); uMesh++)
		{
			if(Flux_MeshAnimation* pxAnim = pxModel->GetMeshGeometryAtIndex(uMesh).m_pxAnimation)
			{
				g_xAnimationsToUpdate.PushBack(pxAnim);
			}
		}
	}

	Zenith_TaskSystem::SubmitTaskArray(g_pxAnimUpdateTask);
}

void Zenith_Scene::WaitForUpdateComplete()
{
	g_pxAnimUpdateTask->WaitUntilComplete();
}

void Zenith_Scene::DispatchFullLifecycleInit()
{
	// This function is now only used by the editor when restoring backup in stopped mode.
	// OnAwake and OnEnable are already dispatched per-entity during LoadFromFile.
	// We just need to dispatch OnStart for enabled entities that haven't had it yet.

	Zenith_ComponentMetaRegistry& xRegistry = Zenith_ComponentMetaRegistry::Get();

	// Copy entity IDs to prevent iterator invalidation if callbacks create entities
	Zenith_Vector<Zenith_EntityID> xEntityIDs;
	xEntityIDs.Reserve(s_xCurrentScene.m_xActiveEntities.GetSize());
	for (u_int u = 0; u < s_xCurrentScene.m_xActiveEntities.GetSize(); ++u)
	{
		xEntityIDs.PushBack(s_xCurrentScene.m_xActiveEntities.Get(u));
	}

	// OnStart - Only for enabled entities that haven't had it yet
	for (u_int u = 0; u < xEntityIDs.GetSize(); ++u)
	{
		Zenith_EntityID xEntityID = xEntityIDs.Get(u);
		if (s_xCurrentScene.EntityExists(xEntityID))
		{
			// Skip if already started
			if (s_xCurrentScene.m_xEntitiesStarted.find(xEntityID) != s_xCurrentScene.m_xEntitiesStarted.end())
				continue;

			Zenith_Entity xEntity = s_xCurrentScene.GetEntity(xEntityID);
			// Only dispatch OnStart if entity is enabled (Unity behavior)
			if (xEntity.IsEnabled())
			{
				xRegistry.DispatchOnStart(xEntity);
				s_xCurrentScene.m_xEntitiesStarted.insert(xEntityID);
			}
		}
	}
}

void Zenith_Scene::DispatchLifecycleForNewScene()
{
	// Called after programmatic scene creation (not file loading).
	// Dispatches OnAwake/OnEnable for entities that haven't been awoken yet.
	// Entities that were awoken via SetBehaviour will be skipped.

	Zenith_ComponentMetaRegistry& xRegistry = Zenith_ComponentMetaRegistry::Get();

	// Copy entity IDs to prevent iterator invalidation if callbacks create entities
	Zenith_Vector<Zenith_EntityID> xEntityIDs;
	xEntityIDs.Reserve(m_xActiveEntities.GetSize());
	for (u_int u = 0; u < m_xActiveEntities.GetSize(); ++u)
	{
		xEntityIDs.PushBack(m_xActiveEntities.Get(u));
	}

	// OnAwake - for entities not yet awoken
	for (u_int u = 0; u < xEntityIDs.GetSize(); ++u)
	{
		Zenith_EntityID xEntityID = xEntityIDs.Get(u);
		if (!EntityExists(xEntityID)) continue;
		if (m_xEntitiesAwoken.find(xEntityID) != m_xEntitiesAwoken.end()) continue;

		Zenith_Entity xEntity = GetEntity(xEntityID);
		xRegistry.DispatchOnAwake(xEntity);
	}

	// OnEnable + mark awoken - for entities not yet awoken
	for (u_int u = 0; u < xEntityIDs.GetSize(); ++u)
	{
		Zenith_EntityID xEntityID = xEntityIDs.Get(u);
		if (!EntityExists(xEntityID)) continue;
		if (m_xEntitiesAwoken.find(xEntityID) != m_xEntitiesAwoken.end()) continue;

		Zenith_Entity xEntity = GetEntity(xEntityID);
		if (xEntity.IsEnabled())
		{
			xRegistry.DispatchOnEnable(xEntity);
		}
		m_xEntitiesAwoken.insert(xEntityID);
	}
}

//------------------------------------------------------------------------------
// Entity Handle Creation
//------------------------------------------------------------------------------

Zenith_Entity Zenith_Scene::GetEntity(Zenith_EntityID xID)
{
	Zenith_Assert(xID.IsValid(), "GetEntity: Invalid entity ID");
	Zenith_Assert(EntityExists(xID), "GetEntity: Entity (idx=%u, gen=%u) does not exist", xID.m_uIndex, xID.m_uGeneration);
	return Zenith_Entity(this, xID);
}

Zenith_Entity Zenith_Scene::FindEntityByName(const std::string& strName)
{
	for (u_int u = 0; u < m_xActiveEntities.GetSize(); ++u)
	{
		Zenith_EntityID xID = m_xActiveEntities.Get(u);
		const Zenith_EntitySlot& xSlot = m_xEntitySlots.Get(xID.m_uIndex);
		if (xSlot.m_strName == strName)
		{
			return Zenith_Entity(this, xID);
		}
	}
	return Zenith_Entity();  // Return invalid handle
}

void Zenith_Scene::SetMainCameraEntity(Zenith_EntityID xEntityID)
{
	if (xEntityID.IsValid())
	{
		Zenith_Assert(EntityExists(xEntityID), "SetMainCameraEntity: Entity (idx=%u, gen=%u) does not exist", xEntityID.m_uIndex, xEntityID.m_uGeneration);
		Zenith_Entity xEntity = GetEntity(xEntityID);
		Zenith_Assert(xEntity.HasComponent<Zenith_CameraComponent>(), "SetMainCameraEntity: Entity does not have CameraComponent");
	}
	m_xMainCameraEntity = xEntityID;
}

Zenith_EntityID Zenith_Scene::GetMainCameraEntity()
{
	return m_xMainCameraEntity;
}

Zenith_CameraComponent* Zenith_Scene::TryGetMainCamera()
{
	if (!m_xMainCameraEntity.IsValid())
	{
		return nullptr;
	}

	if (!EntityExists(m_xMainCameraEntity))
	{
		// Entity was destroyed or stale - clear reference
		m_xMainCameraEntity = INVALID_ENTITY_ID;
		return nullptr;
	}

	Zenith_Entity xEntity = GetEntity(m_xMainCameraEntity);
	if (!xEntity.HasComponent<Zenith_CameraComponent>())
	{
		Zenith_Warning(LOG_CATEGORY_SCENE, "Main camera entity does not have CameraComponent");
		return nullptr;
	}

	return &xEntity.GetComponent<Zenith_CameraComponent>();
}

Zenith_CameraComponent& Zenith_Scene::GetMainCamera()
{
	Zenith_Assert(m_xMainCameraEntity.IsValid(), "GetMainCamera: No main camera set");
	Zenith_Assert(EntityExists(m_xMainCameraEntity), "GetMainCamera: Main camera entity no longer exists");
	Zenith_Entity xEntity = GetEntity(m_xMainCameraEntity);
	return xEntity.GetComponent<Zenith_CameraComponent>();
}