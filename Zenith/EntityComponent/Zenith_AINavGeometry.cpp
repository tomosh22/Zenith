#include "Zenith.h"
#include "EntityComponent/Zenith_AINavGeometry.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Profiling/Zenith_Profiling.h"

namespace
{
	bool CollectGeometryFromScene(Zenith_SceneData& xScene,
		Zenith_Vector<Zenith_Maths::Vector3>& axVerticesOut,
		Zenith_Vector<uint32_t>& axIndicesOut)
	{
		axVerticesOut.Clear();
		axIndicesOut.Clear();

		// Query all entities with ColliderComponent
		const Zenith_Vector<Zenith_EntityID>& axActiveEntities = xScene.GetActiveEntities();

		Zenith_Log(LOG_CATEGORY_AI, "CollectGeometryFromScene: Checking %u active entities", axActiveEntities.GetSize());
		uint32_t uEntitiesWithColliders = 0;
		uint32_t uEntitiesWithValidBodies = 0;

		for (uint32_t u = 0; u < axActiveEntities.GetSize(); ++u)
		{
			Zenith_EntityID xEntityID = axActiveEntities.Get(u);
			Zenith_Entity xEntity = xScene.TryGetEntity(xEntityID);

			if (!xEntity.IsValid())
			{
				continue;
			}

			// Check if entity has a ColliderComponent
			if (!xEntity.HasComponent<Zenith_ColliderComponent>())
			{
				continue;
			}

			Zenith_ColliderComponent& xCollider = xEntity.GetComponent<Zenith_ColliderComponent>();
			++uEntitiesWithColliders;

			// Only include static bodies (floors, walls, etc.)
			// Dynamic bodies (players, enemies) shouldn't be part of the navmesh
			if (xCollider.GetRigidBodyType() != RIGIDBODY_TYPE_STATIC)
			{
				continue;
			}

			// Opt-out: callers can mark a static collider as "not navmesh geometry"
			// when the obstacle is meant to be runtime-blockable (doors, gates,
			// lift barriers). Skipping these here lets the generator emit
			// walkable polygons across the doorway gap; the gameplay layer then
			// calls Zenith_NavMesh::SetBlockedAtPoint at runtime to mark those
			// polygons blocked when the door is closed. See
			// Zenith_ColliderComponent::SetIncludeInNavMesh for the contract.
			if (!xCollider.GetIncludeInNavMesh())
			{
				continue;
			}

			++uEntitiesWithValidBodies;

			if (!xEntity.HasComponent<Zenith_TransformComponent>())
			{
				continue;
			}

			Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
			Zenith_Maths::Vector3 xPos;
			Zenith_Maths::Vector3 xScale;
			Zenith_Maths::Quat    xRot;
			xTransform.GetPosition(xPos);
			xTransform.GetScale(xScale);
			xTransform.GetRotation(xRot);

			// 2026-05-23: build the world-space box corners properly. The
			// previous implementation used (xPos + axis-aligned half-extents)
			// which silently ignored TWO real properties of every collider in
			// the scene:
			//
			//   1. Rotation. Any entity with a non-identity yaw -- procgen-
			//      generated walls, rotated props, etc -- ended up modelled
			//      in the navmesh as an AABB at the entity's TRANSLATION,
			//      not its actual rotated OBB. So a wall sitting at 45 deg
			//      had its real OBB span replaced by a same-size axis-aligned
			//      box rotated 0 deg, leaving large gaps where the wall
			//      really was and false obstacles in the rotated-into-AABB
			//      corners.
			//
			//   2. Mesh anchoring. Zenith_ColliderComponent's BoxShape uses
			//      ComputeBoxDimensionsAndOffset to compute a local-space
			//      offset that aligns the collision shape with the visible
			//      mesh (e.g. wall meshes anchored at [0,1]^3 corner have a
			//      +0.5 local offset on each axis after scale; building-kit
			//      meshes with y-min=0 have an offset of half the height).
			//      The navmesh ignored this offset entirely, so its modelled
			//      box centre was shifted by up to one full half-extent from
			//      where the physics body actually sits.
			//
			// Together this produced a navmesh whose polygons drifted +/- a
			// full half-extent from real geometry, and on rotated walls had
			// the wrong orientation too. The bot's grid-A* worked around it
			// by using its own raycast grid (see DP Tests/CLAUDE.md "Known
			// follow-up"). The priest, which has no such workaround, simply
			// got stuck against walls the navmesh said weren't there.
			//
			// Fix: use ComputeBoxDimensionsAndOffset to get the actual world
			// half-extents + local offset, rotate the offset by the entity's
			// rotation to land the box centre where the physics body is, then
			// rotate each of the eight signed half-extent corners by the
			// rotation to form a true world-space OBB. Same logic Zenith_
			// ColliderComponent's QueueDebugDraw uses for the OBB wireframe,
			// so the rendered debug viz and the navmesh now agree on where
			// every collider is.
			Zenith_Maths::Vector3 xHalfExtents;
			Zenith_Maths::Vector3 xLocalOffset;
			xCollider.ComputeBoxDimensionsAndOffset(xScale, xHalfExtents,
				xLocalOffset, /*bWarnOnDegenerateBounds=*/false);

			const Zenith_Maths::Vector3 xRotatedOffset =
				Zenith_Maths::RotateVector(xLocalOffset, xRot);
			const Zenith_Maths::Vector3 xBoxCentre = xPos + xRotatedOffset;

			uint32_t uBaseVertex = axVerticesOut.GetSize();

			// Eight corners in the order the triangle emit code below expects:
			//   0..3 = bottom face CCW (-y), 4..7 = top face CCW (+y)
			//   per-corner XZ winding: (-,-) (+,-) (+,+) (-,+)
			static const Zenith_Maths::Vector3 s_axCornerSigns[8] = {
				Zenith_Maths::Vector3(-1.0f, -1.0f, -1.0f),
				Zenith_Maths::Vector3( 1.0f, -1.0f, -1.0f),
				Zenith_Maths::Vector3( 1.0f, -1.0f,  1.0f),
				Zenith_Maths::Vector3(-1.0f, -1.0f,  1.0f),
				Zenith_Maths::Vector3(-1.0f,  1.0f, -1.0f),
				Zenith_Maths::Vector3( 1.0f,  1.0f, -1.0f),
				Zenith_Maths::Vector3( 1.0f,  1.0f,  1.0f),
				Zenith_Maths::Vector3(-1.0f,  1.0f,  1.0f),
			};
			Zenith_Maths::Vector3 axBoxVerts[8];
			for (uint32_t uC = 0; uC < 8; ++uC)
			{
				const Zenith_Maths::Vector3 xLocalCorner(
					s_axCornerSigns[uC].x * xHalfExtents.x,
					s_axCornerSigns[uC].y * xHalfExtents.y,
					s_axCornerSigns[uC].z * xHalfExtents.z);
				axBoxVerts[uC] = xBoxCentre +
					Zenith_Maths::RotateVector(xLocalCorner, xRot);
			}

			for (int i = 0; i < 8; ++i)
			{
				axVerticesOut.PushBack(axBoxVerts[i]);
			}

			// Emit all six faces of the box. The top face's normal points up
			// and the slope check (in RasterizeTriangle) marks it walkable;
			// the four sides and the bottom face have normals that fail the
			// slope check and are marked as OBSTRUCTION spans (area type 0).
			// Obstruction spans block the clearance check above any walkable
			// span beneath them, so a 1m-tall wall sitting on the floor carves
			// a hole in the floor's walkable surface directly under its
			// footprint -- which is what the navmesh needs to route AI around
			// walls instead of through them.
			//
			// Vertex layout (matches the axBoxVerts initialiser above):
			//   0-3 = bottom face corners, 4-7 = top face corners.
			//   0=(-x,-y,-z), 1=(+x,-y,-z), 2=(+x,-y,+z), 3=(-x,-y,+z),
			//   4=(-x,+y,-z), 5=(+x,+y,-z), 6=(+x,+y,+z), 7=(-x,+y,+z).
			// Each face wound CCW when viewed from OUTSIDE so the computed
			// triangle normal points outward (away from the box centre).
			auto EmitTri = [&](uint32_t a, uint32_t b, uint32_t c)
			{
				axIndicesOut.PushBack(uBaseVertex + a);
				axIndicesOut.PushBack(uBaseVertex + b);
				axIndicesOut.PushBack(uBaseVertex + c);
			};
			// Top face (normal +Y, walkable).
			EmitTri(4, 7, 6); EmitTri(4, 6, 5);
			// Bottom face (normal -Y, obstruction).
			EmitTri(0, 1, 2); EmitTri(0, 2, 3);
			// Front face Z- (normal -Z, obstruction).
			EmitTri(0, 4, 5); EmitTri(0, 5, 1);
			// Back face Z+ (normal +Z, obstruction).
			EmitTri(3, 2, 6); EmitTri(3, 6, 7);
			// Left face X- (normal -X, obstruction).
			EmitTri(0, 3, 7); EmitTri(0, 7, 4);
			// Right face X+ (normal +X, obstruction).
			EmitTri(1, 5, 6); EmitTri(1, 6, 2);
		}

		Zenith_Log(LOG_CATEGORY_AI, "CollectGeometryFromScene: %u entities with colliders, %u with valid bodies, generated %u vertices",
			uEntitiesWithColliders, uEntitiesWithValidBodies, axVerticesOut.GetSize());

		// Debug: Log all collected geometry heights
		for (uint32_t u = 0; u < axVerticesOut.GetSize(); u += 8)
		{
			// Log each box's top face height (vertices 4-7 are top face)
			if (u + 7 < axVerticesOut.GetSize())
			{
				float fTopY = axVerticesOut.Get(u + 4).y;
				float fBottomY = axVerticesOut.Get(u).y;
				Zenith_Log(LOG_CATEGORY_AI, "  Box %u: bottom Y=%.2f, top Y=%.2f",
					u / 8, fBottomY, fTopY);
			}
		}

		return axVerticesOut.GetSize() > 0;
	}
}

Zenith_NavMesh* Zenith_AINavGeometry::GenerateFromScene(Zenith_SceneData& xScene, const NavMeshGenerationConfig& xConfig)
{
	Zenith_Log(LOG_CATEGORY_AI, "Starting NavMesh generation from scene...");

	// Collect geometry from static colliders
	Zenith_Vector<Zenith_Maths::Vector3> axVertices;
	Zenith_Vector<uint32_t> axIndices;

	{
		Zenith_Profiling::ScopeZone xScope(ZENITH_PROFILE_ZONE("AI NavMesh Generate / Collect Geometry"));
		if (!CollectGeometryFromScene(xScene, axVertices, axIndices))
		{
			Zenith_Log(LOG_CATEGORY_AI, "Failed to collect geometry from scene");
			return nullptr;
		}
	}

	Zenith_Log(LOG_CATEGORY_AI, "Collected %u vertices, %u triangles",
		axVertices.GetSize(), axIndices.GetSize() / 3);

	return Zenith_NavMeshGenerator::GenerateFromGeometry(axVertices, axIndices, xConfig);
}
