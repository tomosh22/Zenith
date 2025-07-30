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

	std::vector<Zenith_ModelComponent*> xModels;
	Zenith_Scene::GetCurrentScene().GetAllOfComponentType<Zenith_ModelComponent>(xModels);
	for (Zenith_ModelComponent* pxModel : xModels)
	{
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

void Zenith_Scene::Reset() {
	m_xRegistry.clear();
	m_uMainCameraEntity = (EntityID)0;
	m_xEntityMap.clear();
}

void Zenith_Scene::Serialize(const std::string& strFilename) {
	STUBBED
}

void Zenith_Scene::Update(const float fDt)
{
	std::vector<Zenith_ScriptComponent*> xScripts;
	s_xCurrentScene.GetAllOfComponentType<Zenith_ScriptComponent>(xScripts);
	for (Zenith_ScriptComponent* pxScript : xScripts)
	{
		pxScript->OnUpdate(fDt);
	}

	Zenith_TaskSystem::SubmitTask(g_pxAnimUpdateTask);

}

void Zenith_Scene::WaitForUpdateComplete()
{
	g_pxAnimUpdateTask->WaitUntilComplete();
}

Zenith_Entity Zenith_Scene::GetEntityByGUID(Zenith_GUID ulGuid) {
	return m_xEntityMap.at(ulGuid);
}

void Zenith_Scene::SetMainCameraEntity(Zenith_Entity& xEntity)
{
	Zenith_Assert(m_uMainCameraEntity == (EntityID)0, "Scene already has a main camera");
	m_uMainCameraEntity = xEntity.GetEntityID();
}

Zenith_CameraComponent& Zenith_Scene::GetMainCamera()
{
	return GetComponentFromEntity<Zenith_CameraComponent>(m_uMainCameraEntity);
}