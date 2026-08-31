#include "Zenith.h"
#include "EntityComponent/Zenith_TerrainPhysicsValidate.h"

#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"

namespace
{
	// Counted only when something is already wrong, so the happy path stays a
	// terrain-only walk. A count makes the error concrete -- "N dynamic bodies are
	// about to fall" is a reproduction step; "no terrain body" is a puzzle.
	u_int CountDynamicBodiesAcrossLoadedScenes()
	{
		u_int uDynamic = 0u;
		Zenith_SceneSystem::Get().QueryAllScenes<Zenith_ColliderComponent>()
			.ForEach([&uDynamic](Zenith_EntityID, Zenith_ColliderComponent& xCollider)
		{
			if (xCollider.GetRigidBodyType() == RIGIDBODY_TYPE_DYNAMIC
				&& xCollider.HasValidBody())
			{
				++uDynamic;
			}
		});
		return uDynamic;
	}
}

void Zenith_ValidateTerrainPhysicsBodies(const char* szContext)
{
	const char* const szSafeContext = szContext != nullptr ? szContext : "<unknown>";

	// Engine-free scene access (Zenith_SceneSystem::Get(), not g_xEngine) so this
	// TU stays off the singleton ratchet -- the same convention
	// Zenith_PhysicsTransformSync follows.
	Zenith_SceneSystem::Get().QueryAllScenes<Zenith_TerrainComponent>()
		.ForEach([szSafeContext](Zenith_EntityID, Zenith_TerrainComponent& xTerrain)
	{
		Zenith_Entity xEntity = xTerrain.GetParentEntity();
		const std::string strName = xEntity.GetName();
		const bool bHasGeometry = xTerrain.HasPhysicsGeometry();

		// TryGetComponent, not Has+Get: it is the single-lookup form AND it
		// short-circuits on an unloaded/stale scene, which matters here because
		// this runs at load-completion time.
		const Zenith_ColliderComponent* pxCollider =
			xEntity.TryGetComponent<Zenith_ColliderComponent>();
		const bool bHasCollider = pxCollider != nullptr;
		const bool bIsTerrainCollider = bHasCollider
			&& pxCollider->GetCollisionVolumeType() == COLLISION_VOLUME_TYPE_TERRAIN;
		const bool bHasBody = bHasCollider && pxCollider->HasValidBody();

		// UNCONDITIONAL, and that is the point: a fall-through report should never
		// require a repro to answer "did the terrain have collision?".
		Zenith_Log(LOG_CATEGORY_TERRAIN,
			"[TerrainPhysics] context='%s' terrain='%s' physicsGeometry=%s "
			"collider=%s terrainVolume=%s body=%s",
			szSafeContext, strName.c_str(),
			bHasGeometry ? "yes" : "NO",
			bHasCollider ? "yes" : "NO",
			bIsTerrainCollider ? "yes" : "NO",
			bHasBody ? "yes" : "NO");

		// A terrain with no collider at all is an AUTHORING choice (decorative
		// terrain), not the fall-through mode -- say nothing about it.
		if (!bIsTerrainCollider || bHasBody)
		{
			return;
		}

		const u_int uDynamic = CountDynamicBodiesAcrossLoadedScenes();

		if (!bHasGeometry)
		{
			// THE COMMON, ASSET-CAUSED MODE (ZM-D-182). Non-fatal on purpose: a cold
			// or deliberately-unbaked tree is a legitimate developer state, and
			// Zenith_Assert breaks in every configuration.
			Zenith_Error(LOG_CATEGORY_TERRAIN,
				"[TerrainPhysics] context='%s' terrain='%s' HAS NO PHYSICS BODY because it "
				"has no physics geometry -- %u dynamic body(ies) in loaded scenes will FALL "
				"THROUGH THE WORLD. This is an ASSET problem, not a gameplay one: the baked "
				"Physics_*.zgeom chunks are missing, or STALE and rejected by "
				"Zenith_TerrainChunkLayout's vertex/index validation. A bake stamp that is "
				"(version, file COUNT) does NOT notice a content-only change, so a warm-looking "
				"tree can sit in this state forever -- re-bake this terrain and bump its stamp. "
				"See Zenith/Flux/Terrain/CLAUDE.md and Games/Zenithmon/Docs/DecisionLog.md ZM-D-182.",
				szSafeContext, strName.c_str(), uDynamic);
			return;
		}

		// THE IMPOSSIBLE MODE. The geometry loaded, so no asset can explain this --
		// shape construction or body registration failed. That is an engine defect
		// and is worth a break.
		Zenith_Error(LOG_CATEGORY_TERRAIN,
			"[TerrainPhysics] context='%s' terrain='%s' has physics geometry but NO BODY -- "
			"%u dynamic body(ies) will fall through. Shape creation or body registration "
			"failed (see the CreateTerrainShape / AddCollider warnings above).",
			szSafeContext, strName.c_str(), uDynamic);
		Zenith_Assert(false,
			"terrain '%s' loaded physics geometry but has no physics body (context '%s') -- "
			"this cannot be caused by an asset; it is an engine defect in "
			"Zenith_ColliderComponent::CreateTerrainShape or AddCollider",
			strName.c_str(), szSafeContext);
	});
}
