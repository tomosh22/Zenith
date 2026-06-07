#include "Zenith.h"
#include "EntityComponent/Zenith_PhysicsDebugDraw.h"
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
