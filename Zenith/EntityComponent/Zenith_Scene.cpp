#include "Zenith.h"

#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TextComponent.h"
#include "Flux/MeshAnimation/Flux_MeshAnimation.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#include "DataStream/Zenith_DataStream.h"

Zenith_Scene Zenith_Scene::s_xCurrentScene;

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
	m_pxMainCameraEntity = nullptr;
}

void Zenith_Scene::SaveToFile(const std::string& strFilename)
{
	Zenith_DataStream xStream;

	// Write file header and version
	const u_int uMagicNumber = 0x5A53434E; // "ZSCN" in hex
	const u_int uVersion = 1;
	xStream << uMagicNumber;
	xStream << uVersion;

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
	Zenith_EntityID uMainCameraID = (m_pxMainCameraEntity != nullptr) ? m_pxMainCameraEntity->GetEntityID() : static_cast<Zenith_EntityID>(-1);
	xStream << uMainCameraID;

	// Write to file
	xStream.WriteToFile(strFilename.c_str());
}

void Zenith_Scene::LoadFromFile(const std::string& strFilename)
{
	// Clear the current scene
	Reset();

	// Read file into data stream
	Zenith_DataStream xStream;
	xStream.ReadFromFile(strFilename.c_str());

	// Read and validate file header
	u_int uMagicNumber;
	u_int uVersion;
	xStream >> uMagicNumber;
	xStream >> uVersion;

	if (uMagicNumber != 0x5A53434E) // "ZSCN"
	{
		Zenith_Assert(false, "Invalid scene file format");
		return;
	}

	if (uVersion != 1)
	{
		Zenith_Assert(false, "Unsupported scene file version");
		return;
	}

	// Read the number of entities
	u_int uNumEntities;
	xStream >> uNumEntities;

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

		// Create entity with the exact same ID from the saved scene
		// We need to manually ensure the CreateEntity counter doesn't conflict
		Zenith_Entity xEntity(this, uEntityID, uParentID, strName);

		// Now read the components
		u_int uNumComponents;
		xStream >> uNumComponents;

		for (u_int c = 0; c < uNumComponents; c++)
		{
			std::string strComponentType;
			xStream >> strComponentType;

			// Deserialize component based on type
			// (This is the same logic as in Entity::ReadFromDataStream)
			if (strComponentType == "TransformComponent")
			{
				if (xEntity.HasComponent<Zenith_TransformComponent>())
				{
					xEntity.GetComponent<Zenith_TransformComponent>().ReadFromDataStream(xStream);
				}
			}
			else if (strComponentType == "ModelComponent")
			{
				if (!xEntity.HasComponent<Zenith_ModelComponent>())
				{
					Zenith_ModelComponent& xComponent = xEntity.AddComponent<Zenith_ModelComponent>();
					xComponent.ReadFromDataStream(xStream);
				}
			}
			else if (strComponentType == "CameraComponent")
			{
				if (!xEntity.HasComponent<Zenith_CameraComponent>())
				{
					Zenith_CameraComponent& xComponent = xEntity.AddComponent<Zenith_CameraComponent>();
					xComponent.ReadFromDataStream(xStream);
				}
			}
			else if (strComponentType == "ColliderComponent")
			{
				if (!xEntity.HasComponent<Zenith_ColliderComponent>())
				{
					Zenith_ColliderComponent& xComponent = xEntity.AddComponent<Zenith_ColliderComponent>();
					xComponent.ReadFromDataStream(xStream);
				}
			}
			else if (strComponentType == "TextComponent")
			{
				if (!xEntity.HasComponent<Zenith_TextComponent>())
				{
					Zenith_TextComponent& xComponent = xEntity.AddComponent<Zenith_TextComponent>();
					xComponent.ReadFromDataStream(xStream);
				}
			}
		}
	}

	// Read main camera entity ID
	Zenith_EntityID uMainCameraID;
	xStream >> uMainCameraID;

	if (uMainCameraID != static_cast<Zenith_EntityID>(-1))
	{
		// Find and set the main camera entity
		auto it = m_xEntityMap.find(uMainCameraID);
		if (it != m_xEntityMap.end())
		{
			m_pxMainCameraEntity = &it->second;
		}
	}
}

void Zenith_Scene::Update(const float fDt)
{
	s_xCurrentScene.AcquireMutex();
	Zenith_Vector<Zenith_ScriptComponent*> xScripts;
	s_xCurrentScene.GetAllOfComponentType<Zenith_ScriptComponent>(xScripts);
	for (Zenith_Vector<Zenith_ScriptComponent*>::Iterator xIt(xScripts); !xIt.Done(); xIt.Next())
	{
		xIt.GetData()->OnUpdate(fDt);
	}
	s_xCurrentScene.ReleaseMutex();

	//#TO used to have this before script update but scripts can add new model components
	//causing a vector resize which causes the animation update to read deallocate model memory
	
	g_xAnimationsToUpdate.Clear();
	Zenith_Vector<Zenith_ModelComponent*> xModels;
	s_xCurrentScene.GetAllOfComponentType<Zenith_ModelComponent>(xModels);
	for (Zenith_Vector<Zenith_ModelComponent*>::Iterator xIt(xModels); !xIt.Done(); xIt.Next())
	{
		Zenith_ModelComponent* pxModel = xIt.GetData();
		for (u_int uMesh = 0; uMesh < pxModel->GetNumMeshEntires(); uMesh++)
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

void Zenith_Scene::SetMainCameraEntity(Zenith_Entity& xEntity)
{
	m_pxMainCameraEntity = &xEntity;
}

Zenith_CameraComponent& Zenith_Scene::GetMainCamera()
{
	return m_pxMainCameraEntity->GetComponent<Zenith_CameraComponent>();
}