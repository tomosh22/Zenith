#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "EntityComponent/Zenith_PhysicsDebugDraw.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "Physics/Zenith_PhysicsMeshGenerator.h"   // g_xPhysicsMeshConfig
#include "ZenithECS/Zenith_Query.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Primitives/Flux_PrimitivesImpl.h"

void Zenith_PhysicsDebugDraw::DrawMesh(
	const Flux_MeshGeometry* pxPhysicsMesh,
	const Zenith_Maths::Matrix4& xTransform,
	const Zenith_Maths::Vector3& xColor)
{
	if (!pxPhysicsMesh || !pxPhysicsMesh->m_pxPositions || pxPhysicsMesh->GetNumIndices() < 3)
	{
		return;
	}

	// Draw wireframe triangles using lines
	const uint32_t uNumTris = pxPhysicsMesh->GetNumIndices() / 3;

	for (uint32_t t = 0; t < uNumTris; t++)
	{
		uint32_t uIdx0 = pxPhysicsMesh->m_puIndices[t * 3 + 0];
		uint32_t uIdx1 = pxPhysicsMesh->m_puIndices[t * 3 + 1];
		uint32_t uIdx2 = pxPhysicsMesh->m_puIndices[t * 3 + 2];

		// Get positions and transform to world space
		Zenith_Maths::Vector4 xPos0 = xTransform * Zenith_Maths::Vector4(pxPhysicsMesh->m_pxPositions[uIdx0], 1.0f);
		Zenith_Maths::Vector4 xPos1 = xTransform * Zenith_Maths::Vector4(pxPhysicsMesh->m_pxPositions[uIdx1], 1.0f);
		Zenith_Maths::Vector4 xPos2 = xTransform * Zenith_Maths::Vector4(pxPhysicsMesh->m_pxPositions[uIdx2], 1.0f);

		Zenith_Maths::Vector3 xV0(xPos0.x, xPos0.y, xPos0.z);
		Zenith_Maths::Vector3 xV1(xPos1.x, xPos1.y, xPos1.z);
		Zenith_Maths::Vector3 xV2(xPos2.x, xPos2.y, xPos2.z);

		// Draw the three edges of the triangle
		g_xEngine.Primitives().AddLine(xV0, xV1, xColor, 0.05f);
		g_xEngine.Primitives().AddLine(xV1, xV2, xColor, 0.05f);
		g_xEngine.Primitives().AddLine(xV2, xV0, xColor, 0.05f);
	}
}

void Zenith_PhysicsDebugDraw::QueueAll()
{
	// Iterate all loaded scenes (not just the active one) so props in additively-
	// loaded scenes and characters in the persistent scene also surface their
	// physics-mesh wireframes. Forwards to each component's debug-draw hook; the
	// actual Flux line rendering happens in DrawMesh above.
	Zenith_Vector<Zenith_ModelComponent*> xModels;
	g_xEngine.Scenes().QueryAllScenes<Zenith_ModelComponent>().ForEach(
		[&xModels](Zenith_EntityID, Zenith_ModelComponent& xComp) { xModels.PushBack(&xComp); });

	for (uint32_t i = 0; i < xModels.GetSize(); i++)
	{
		Zenith_ModelComponent* pxModel = xModels.Get(i);
		if (!pxModel || !pxModel->GetDebugDrawPhysicsMesh())
		{
			continue;
		}
		pxModel->QueueDebugDrawPhysicsMesh(g_xPhysicsMeshConfig.m_xDebugColor);
	}

	Zenith_Vector<Zenith_ColliderComponent*> xColliders;
	g_xEngine.Scenes().QueryAllScenes<Zenith_ColliderComponent>().ForEach(
		[&xColliders](Zenith_EntityID, Zenith_ColliderComponent& xComp) { xColliders.PushBack(&xComp); });

	for (uint32_t i = 0; i < xColliders.GetSize(); i++)
	{
		Zenith_ColliderComponent* pxCollider = xColliders.Get(i);
		if (!pxCollider || !pxCollider->GetDebugDrawPhysicsMesh())
		{
			continue;
		}
		pxCollider->QueueDebugDraw(g_xPhysicsMeshConfig.m_xDebugColor);
	}
}
