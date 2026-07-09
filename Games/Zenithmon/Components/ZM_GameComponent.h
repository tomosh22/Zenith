#pragma once
#include "Core/Zenith_Engine.h"
#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "DataStream/Zenith_DataStream.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_MeshGeometryAsset.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Maths/Zenith_Maths.h"
#include <cmath>

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

// ============================================================================
// ZM_GameComponent -- Zenithmon's S0 starter component.
//
// Spawns one lit cube in OnStart and bobs it up and down in OnUpdate -- the
// boot-proof placeholder until the real game systems land (S1+). Lifecycle
// hooks (OnStart / OnUpdate / the serialization pair) are CONCEPT-DETECTED by
// the component-meta registry -- there is no base class to inherit.
// ============================================================================
class ZM_GameComponent
{
public:
	ZM_GameComponent() = delete;
	ZM_GameComponent(Zenith_Entity& xParentEntity)
		: m_xParentEntity(xParentEntity)
	{
	}

	// Component pools relocate their elements (move-construct + destruct the
	// source), so moves must exist; copies are deleted. Every member is a movable
	// handle / POD, so defaulted moves are correct (no owned raw pointers).
	ZM_GameComponent(const ZM_GameComponent&) = delete;
	ZM_GameComponent& operator=(const ZM_GameComponent&) = delete;
	ZM_GameComponent(ZM_GameComponent&&) noexcept = default;
	ZM_GameComponent& operator=(ZM_GameComponent&&) noexcept = default;

	// OnStart -- called before the first update, after all OnAwake. Fires for both
	// the live-authored (_True) and the disk-loaded (_False) scene, so the cube is
	// built here (OnAwake is skipped on scene deserialization).
	void OnStart()
	{
		Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
		if (!xScene.IsValid())
		{
			return;
		}
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
		if (pxSceneData == nullptr)
		{
			return;
		}

		// Unit cube geometry (owned by the asset handle) + a simple lit material.
		m_xCubeAsset = Zenith_MeshGeometryAsset::CreateUnitCube();
		Flux_MeshGeometry* pxGeometry = m_xCubeAsset.GetDirect()->GetGeometry();

		m_xMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
		m_xMaterial.GetDirect()->SetName("ZM_Cube");
		m_xMaterial.GetDirect()->SetBaseColor({ 0.30f, 0.60f, 0.90f, 1.0f });

		m_xCubeEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "ZM_Cube");
		Zenith_TransformComponent& xTransform = m_xCubeEntity.GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition({ 0.0f, 1.0f, 0.0f });

		Zenith_ModelComponent& xModel = m_xCubeEntity.AddComponent<Zenith_ModelComponent>();
		xModel.AddMeshEntry(*pxGeometry, *m_xMaterial.GetDirect());
	}

	// OnUpdate -- bob the cube.
	void OnUpdate(const float fDt)
	{
		m_fElapsed += fDt;
		if (!m_xCubeEntity.IsValid())
		{
			return;
		}
		Zenith_TransformComponent& xTransform = m_xCubeEntity.GetComponent<Zenith_TransformComponent>();
		const float fY = 1.0f + std::sin(m_fElapsed * 2.0f) * 0.4f;
		xTransform.SetPosition({ 0.0f, fY, 0.0f });
	}

	// Serialization -- a component-contract leading version is required by the
	// meta registry even when there is no payload.
	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		const u_int uComponentVersion = 1;
		xStream << uComponentVersion;
	}

	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		u_int uComponentVersion = 0;
		xStream >> uComponentVersion;
	}

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel()
	{
		ImGui::Text("Zenithmon S0 game component");
		ImGui::TextWrapped("Boot-proof placeholder: spawns and bobs one lit cube. "
			"Replaced by the real game systems from S1 onward.");
	}
#endif

private:
	Zenith_Entity      m_xParentEntity;
	Zenith_Entity      m_xCubeEntity;
	MeshGeometryHandle m_xCubeAsset;
	MaterialHandle     m_xMaterial;
	float              m_fElapsed = 0.0f;
};
