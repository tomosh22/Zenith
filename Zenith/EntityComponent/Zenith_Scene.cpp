#include "Zenith.h"

#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "Flux/MeshAnimation/Flux_MeshAnimation.h"
#include "TaskSystem/Zenith_TaskSystem.h"

Zenith_Scene Zenith_Scene::s_xCurrentScene;

static Zenith_Task* g_pxAnimUpdateTask = nullptr;

enum class ComponentType {
	Transform,
	Model,
	Collider,
	Script,
	Terrain,
	Foliage
};

void AnimUpdateTask(void*)
{
	const float fDt = Zenith_Core::GetDt();

	Zenith_Vector<Zenith_ModelComponent*> xModels;
	Zenith_Scene::GetCurrentScene().GetAllOfComponentType<Zenith_ModelComponent>(xModels);
	for (Zenith_Vector<Zenith_ModelComponent*>::Iterator xIt(xModels); !xIt.Done(); xIt.Next())
	{
		Zenith_ModelComponent* pxModel = xIt.GetData();
		for (u_int uMesh = 0; uMesh < pxModel->GetNumMeshEntires(); uMesh++)
		{
			Flux_MeshAnimation* pxAnim = pxModel->GetMeshGeometryAtIndex(uMesh).m_pxAnimation;
			if (pxAnim)
			{
				pxAnim->Update(fDt);
			}
		}
	}
}

Zenith_Scene::Zenith_Scene()
{
	//#TO_TODO: don't like this, need some sort of global init
	static bool ls_bOnce = true;
	if (ls_bOnce)
	{
		ls_bOnce = false;
		g_pxAnimUpdateTask = new Zenith_Task(ZENITH_PROFILE_INDEX__ANIMATION, AnimUpdateTask, nullptr);
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

void Zenith_Scene::Serialize(const std::string& strFilename) {
	STUBBED
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
	Zenith_TaskSystem::SubmitTask(g_pxAnimUpdateTask);
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