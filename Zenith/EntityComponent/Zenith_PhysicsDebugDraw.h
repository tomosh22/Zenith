#pragma once
#include "Maths/Zenith_Maths.h"

// Forward declaration — the wireframe drawer reads positions/indices out of the
// renderer mesh. This lives engine-side (EntityComponent) so the Physics layer
// names no Flux type (Wave-1 Physics->Flux sever); EntityComponent is allowed to
// name Flux, and the wireframe rendering legitimately needs Flux_Primitives.
class Flux_MeshGeometry;

// Engine-side physics-mesh debug visualisation, relocated out of
// Zenith_PhysicsMeshGenerator (Physics/) so Physics stays renderer-neutral.
class Zenith_PhysicsDebugDraw
{
public:
	/**
	 * Render a wireframe overlay of a physics mesh using Flux_Primitives.
	 * @param pxMesh     Physics mesh to visualize (CPU positions + indices)
	 * @param xTransform World transform matrix to apply
	 * @param xColor     Debug color for the wireframe
	 */
	static void DrawMesh(
		const Flux_MeshGeometry* pxMesh,
		const Zenith_Maths::Matrix4& xTransform,
		const Zenith_Maths::Vector3& xColor);
};
