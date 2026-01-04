#include "Zenith.h"

#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_ComponentMeta.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
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
#include "Flux/SSAO/Flux_SSAO.h"
#include "Flux/Fog/Flux_Fog.h"
#include "Flux/SDFs/Flux_SDFs.h"
#include "Flux/Quads/Flux_Quads.h"
#ifdef ZENITH_TOOLS
#include "Flux/Gizmos/Flux_Gizmos.h"
#endif
#include "TaskSystem/Zenith_TaskSystem.h"
#include "DataStream/Zenith_DataStream.h"

Zenith_Scene Zenith_Scene::s_xCurrentScene;
bool Zenith_Scene::s_bIsLoadingScene = false;

// Force link ScriptComponent to ensure its static registration runs
// This is needed because linker may not include translation units with no referenced symbols
namespace { struct ForceScriptComponentLink { ForceScriptComponentLink() { Zenith_ScriptComponent_ForceLink(); } } s_xForceLink; }

static Zenith_TaskArray* g_pxAnimUpdateTask = nullptr;
static Zenith_Vector<Flux_MeshAnimation*> g_xAnimationsToUpdate;

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
	//#TO_TODO: don't like this, need some sort of global init
	static bool ls_bOnce = true;
	if (ls_bOnce)
	{
		ls_bOnce = false;
		// Create task array with 4 invocations for parallel processing
		// Enable submitting thread joining to utilize the main thread
		g_pxAnimUpdateTask = new Zenith_TaskArray(ZENITH_PROFILE_INDEX__ANIMATION, AnimUpdateTask, nullptr, 4, true);
		atexit([]() {delete g_pxAnimUpdateTask; });
	}
}

Zenith_Scene::~Zenith_Scene() {
	Reset();
}

void Zenith_Scene::Reset()
{
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
	m_xEntityMap.clear();
	m_uMainCameraEntity = INVALID_ENTITY_ID;
	m_uNextEntityID = 1;  // Reset entity ID counter (0 is reserved as invalid)
}

void Zenith_Scene::RemoveEntity(Zenith_EntityID uID)
{
	// Check if entity exists
	auto it = m_xEntityMap.find(uID);
	if (it == m_xEntityMap.end())
	{
		Zenith_Log("Warning: Attempted to remove non-existent entity %u", uID);
		return;
	}

	// Clear the main camera reference if this is the camera entity
	if (m_uMainCameraEntity != INVALID_ENTITY_ID && m_uMainCameraEntity == uID)
	{
		m_uMainCameraEntity = INVALID_ENTITY_ID;
	}

	// Remove all components from the entity (calls OnDestroy and removes from pools)
	Zenith_Entity& xEntity = it->second;
	Zenith_ComponentMetaRegistry::Get().RemoveAllComponents(xEntity);

	// Remove entity name
	m_xEntityNames.erase(uID);

	// Remove from entity map
	m_xEntityMap.erase(it);

	Zenith_Log("Entity %u removed from scene", uID);
}

void Zenith_Scene::SaveToFile(const std::string& strFilename)
{
	Zenith_DataStream xStream;

	// Write file header and version
	// Version 3: Entity hierarchy with child IDs
	static constexpr u_int SCENE_MAGIC = 0x5A53434E; // "ZSCN" in hex
	static constexpr u_int SCENE_VERSION = 3;
	xStream << SCENE_MAGIC;
	xStream << SCENE_VERSION;

	// Write the number of entities
	u_int uNumEntities = static_cast<u_int>(m_xEntityMap.size());
	xStream << uNumEntities;

	// Write each entity
	for (auto& pair : m_xEntityMap)
	{
		Zenith_Entity& xEntity = pair.second;
		xEntity.WriteToDataStream(xStream);
	}

	// Write main camera entity ID (if exists)
	Zenith_EntityID uMainCameraID = (m_uMainCameraEntity != INVALID_ENTITY_ID) ? m_uMainCameraEntity : INVALID_ENTITY_ID;
	xStream << uMainCameraID;

	// Write to file
	xStream.WriteToFile(strFilename.c_str());
}

void Zenith_Scene::LoadFromFile(const std::string& strFilename)
{
	// CRITICAL: Set loading flag BEFORE Reset() to prevent asset deletion
	// During Reset(), component destructors check this flag to avoid deleting
	// assets that will be needed when deserializing the scene.
	s_bIsLoadingScene = true;

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
	Reset();

	// Read file into data stream
	Zenith_DataStream xStream;
	xStream.ReadFromFile(strFilename.c_str());

	// Read and validate file header
	u_int uMagicNumber;
	u_int uVersion;
	xStream >> uMagicNumber;
	xStream >> uVersion;

	static constexpr u_int SCENE_MAGIC = 0x5A53434E; // "ZSCN"
	static constexpr u_int SCENE_VERSION_CURRENT = 3;
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
		Zenith_Log("[Scene] ERROR: Unsupported legacy scene format version %u. Please re-export the scene.", uVersion);
		return;
	}

	// Current format (v3): Entity hierarchy with child IDs and GUID asset refs

	// Read the number of entities
	u_int uNumEntities;
	xStream >> uNumEntities;

	// Track the maximum entity ID to properly set m_uNextEntityID after loading
	Zenith_EntityID uMaxEntityID = 0;

	// Read and reconstruct each entity
	for (u_int u = 0; u < uNumEntities; u++)
	{
		// Read entity ID, parent ID, and name first
		Zenith_EntityID uEntityID;
		Zenith_EntityID uParentID;
		std::string strName;

		xStream >> uEntityID;
		xStream >> uParentID;
		xStream >> strName;

		// Read child entity IDs
		uint32_t uChildCount = 0;
		xStream >> uChildCount;
		// Read and discard child IDs - we'll rebuild from parent IDs after loading
		for (uint32_t i = 0; i < uChildCount; ++i)
		{
			Zenith_EntityID uChildID;
			xStream >> uChildID;
		}

		// Track maximum entity ID
		if (uEntityID > uMaxEntityID)
		{
			uMaxEntityID = uEntityID;
		}

		// Ensure m_xEntityComponents has space for this entity ID
		while (m_xEntityComponents.GetSize() <= uEntityID)
		{
			m_xEntityComponents.PushBack({});
		}

		// Create entity with the exact same ID from the saved scene
		// IMPORTANT: Entity constructor inserts a COPY into m_xEntityMap
		// We need to get a reference to that copy, not work with the local variable
		Zenith_Entity xEntity(this, uEntityID, uParentID, strName);

		// Get reference to the entity that's actually in the map
		// (The constructor made a copy, so we need to work with that copy)
		Zenith_Entity& xEntityInMap = m_xEntityMap.at(uEntityID);

		// Deserialize all components using the ComponentMeta registry
		Zenith_ComponentMetaRegistry::Get().DeserializeEntityComponents(xEntityInMap, xStream);
	}

	// Rebuild child lists from parent IDs (ensures consistency)
	for (auto& pair : m_xEntityMap)
	{
		Zenith_Entity& xEntity = pair.second;
		if (xEntity.HasParent())
		{
			auto xParentIt = m_xEntityMap.find(xEntity.GetParentEntityID());
			if (xParentIt != m_xEntityMap.end())
			{
				xParentIt->second.AddChild(xEntity.GetEntityID());
			}
		}
	}

	// Update m_uNextEntityID to be one more than the highest loaded entity ID
	// This ensures newly created entities get unique IDs that don't collide with loaded entities
	m_uNextEntityID = uMaxEntityID + 1;

	// Read main camera entity ID
	Zenith_EntityID uMainCameraID;
	xStream >> uMainCameraID;

	if (uMainCameraID != static_cast<Zenith_EntityID>(-1))
	{
		// Find and set the main camera entity
		auto it = m_xEntityMap.find(uMainCameraID);
		if (it != m_xEntityMap.end())
		{
			m_uMainCameraEntity = it->second.GetEntityID();
		}
	}

	// Clear loading flag - scene deserialization complete
	s_bIsLoadingScene = false;
}

void Zenith_Scene::Update(const float fDt)
{
	s_xCurrentScene.AcquireMutex();
	Zenith_Vector<Zenith_ScriptComponent*> xScripts;
	s_xCurrentScene.GetAllOfComponentType<Zenith_ScriptComponent>(xScripts);
	for (Zenith_Vector<Zenith_ScriptComponent*>::Iterator xIt(xScripts); !xIt.Done(); xIt.Next())
	{
		Zenith_ScriptComponent* const pxScript = xIt.GetData();
		pxScript->OnUpdate(fDt);
	}
	s_xCurrentScene.ReleaseMutex();

	// Update UI components after scripts (scripts can modify UI during their update)
	Zenith_Vector<Zenith_UIComponent*> xUIComponents;
	s_xCurrentScene.GetAllOfComponentType<Zenith_UIComponent>(xUIComponents);
	for (Zenith_Vector<Zenith_UIComponent*>::Iterator xIt(xUIComponents); !xIt.Done(); xIt.Next())
	{
		Zenith_UIComponent* const pxUI = xIt.GetData();
		pxUI->Update(fDt);
	}

	//#TO used to have this before script update but scripts can add new model components
	//causing a vector resize which causes the animation update to read deallocate model memory

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

Zenith_Entity Zenith_Scene::GetEntityByID(Zenith_EntityID uID) {
	return m_xEntityMap.at(uID);
}

Zenith_Entity Zenith_Scene::GetEntityFromID(Zenith_EntityID uID) {
	return m_xEntityMap.at(uID);
}

const std::string& Zenith_Scene::GetEntityName(Zenith_EntityID uID) const
{
	static const std::string s_strEmpty;
	auto xIt = m_xEntityNames.find(uID);
	if (xIt != m_xEntityNames.end())
	{
		return xIt->second;
	}
	return s_strEmpty;
}

void Zenith_Scene::SetEntityName(Zenith_EntityID uID, const std::string& strName)
{
	m_xEntityNames[uID] = strName;
}

Zenith_Entity* Zenith_Scene::FindEntityByName(const std::string& strName)
{
	for (auto& xPair : m_xEntityMap)
	{
		if (GetEntityName(xPair.first) == strName)
		{
			return &xPair.second;
		}
	}
	return nullptr;
}

void Zenith_Scene::SetMainCameraEntity(Zenith_EntityID uEntity)
{
	m_uMainCameraEntity = uEntity;
}

Zenith_EntityID Zenith_Scene::GetMainCameraEntity()
{
	return m_uMainCameraEntity;
}

Zenith_CameraComponent& Zenith_Scene::GetMainCamera()
{
	return GetEntityFromID(m_uMainCameraEntity).GetComponent<Zenith_CameraComponent>();
}