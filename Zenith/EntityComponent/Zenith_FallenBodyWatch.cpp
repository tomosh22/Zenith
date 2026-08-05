#include "Zenith.h"
#include "EntityComponent/Zenith_FallenBodyWatch.h"

#include "Collections/Zenith_Vector.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Physics/Zenith_Physics.h"
#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"

namespace
{
	// The "this body is no longer in the world" line. Deliberately FAR below any
	// authored floor rather than just below it: unmistakable, not early. Zenithmon's
	// Dawnmere terrain sits around y=25, so a body crossing this has fallen ~75 m.
	constexpr float fFALLEN_BODY_Y_THRESHOLD = -50.0f;

	// ONSET detection. A body that is merely SETTLING drops a few millimetres and
	// stops (Vesper's authored pose settles 13 mm); a body that has lost its support
	// keeps going. Requiring half a metre of CONSECUTIVE descent separates the two
	// without needing to know where the floor is, and fires ~0.3 s into a fall --
	// early enough that the body state still resembles the moment support was lost.
	constexpr float fONSET_ACCUMULATED_DROP = 0.5f;
	// Per-frame noise floor: below this a frame does not count as "descending", so
	// float jitter on a resting body cannot accumulate into a false onset.
	constexpr float fONSET_PER_FRAME_EPSILON = 1.0e-4f;
	// ★ AND IT MUST ACTUALLY BE FALLING. Sustained descent alone is NOT enough: a
	// character WALKING DOWN A SLOPE descends continuously for metres, and an early
	// version of this watch duly reported the player and the wanderer on a perfectly
	// healthy run (observed vertical speeds -1.0 to -3.2 m/s). Free fall is separable
	// by SPEED, because gravity reaches values a walk cannot: the real Vesper fall
	// was measured at -36 m/s. 8 m/s is ~2.5x the fastest observed walking descent
	// and is reached ~0.8 s into a genuine fall, so this stays early without lying.
	constexpr float fONSET_MIN_FALL_SPEED = 8.0f;

	struct WatchedBody
	{
		u_int                 m_uEntityIndex = 0u;
		float                 m_fLastY = 0.0f;
		float                 m_fAccumulatedDrop = 0.0f;
		Zenith_PhysicsBodyID  m_xBodyID;
		bool                  m_bSeeded = false;
		bool                  m_bOnsetReported = false;
		bool                  m_bLeftWorldReported = false;
	};

	// Falls are rare and dynamic bodies are few, so a linear scan beats a hash map
	// and allocates nothing on the happy path.
	Zenith_Vector<WatchedBody> g_xWatched;

	u_int g_uFramesSinceLoad = 0u;
	float g_fSecondsSinceLoad = 0.0f;

	WatchedBody& FindOrAdd(u_int uIndex)
	{
		for (u_int u = 0; u < g_xWatched.GetSize(); ++u)
		{
			if (g_xWatched.Get(u).m_uEntityIndex == uIndex)
			{
				return g_xWatched.Get(u);
			}
		}
		WatchedBody xNew;
		xNew.m_uEntityIndex = uIndex;
		g_xWatched.PushBack(xNew);
		return g_xWatched.Get(g_xWatched.GetSize() - 1u);
	}
}

void Zenith_ResetFallenBodyWatch()
{
	g_xWatched.Clear();
	g_uFramesSinceLoad = 0u;
	g_fSecondsSinceLoad = 0.0f;
}

void Zenith_TickFallenBodyWatch(float fDeltaSeconds)
{
	++g_uFramesSinceLoad;
	if (fDeltaSeconds > 0.0f)
	{
		g_fSecondsSinceLoad += fDeltaSeconds;
	}

	Zenith_Physics& xPhysics = Zenith_Physics::Get();
	if (!xPhysics.HasActiveSimulation())
	{
		return;
	}

	Zenith_SceneSystem::Get().QueryAllScenes<Zenith_TransformComponent, Zenith_ColliderComponent>()
		.ForEach([&xPhysics](Zenith_EntityID xID, Zenith_TransformComponent& xTransform,
			Zenith_ColliderComponent& xCollider)
	{
		if (xCollider.GetRigidBodyType() != RIGIDBODY_TYPE_DYNAMIC
			|| !xCollider.HasValidBody())
		{
			return;
		}

		Zenith_Maths::Vector3 xPosition;
		xTransform.GetPosition(xPosition);
		WatchedBody& xWatch = FindOrAdd(xID.m_uIndex);

		// ---- Collider rebuild detection -------------------------------------
		// A rebuilt collider is a DESTROY + RECREATE: the shape is rebuilt from the
		// component's current state, and for a scale-derived capsule that means the
		// TRANSFORM SCALE decides its dimensions. A uniform model scale collapses
		// such a capsule (the hazard ZM_HumanBody.h exists to avoid), so "did this
		// body get rebuilt just before it started falling?" is the single highest-
		// value question here -- and the body id is the only way to see it happen.
		const Zenith_PhysicsBodyID xCurrentBody = xCollider.GetBodyID();
		if (xWatch.m_bSeeded && xCurrentBody != xWatch.m_xBodyID)
		{
			Zenith_Maths::Vector3 xScale;
			xTransform.GetScale(xScale);
			Zenith_Warning(LOG_CATEGORY_PHYSICS,
				"[FallenBody] '%s' (entity %u) COLLIDER REBUILT: body %u -> %u at "
				"framesSinceLoad=%u (%.2fs), pos=(%.2f, %.2f, %.2f) scale=(%.3f, %.3f, %.3f) "
				"volumeType=%d. ★ A REBUILD AT framesSinceLoad~2 IS EXPECTED for a "
				"Zenithmon human (the explicit capsule from ZM_HumanBody.h is installed "
				"over the authored one) and happens to the player and every dynamic NPC "
				"alike -- observed on healthy runs, so it is NOT by itself a fault. It "
				"only becomes a suspect if a fall-onset line for the SAME entity follows "
				"within a few frames, since a scale-derived capsule collapses under a "
				"uniform model scale. (This sentence deliberately avoids the onset "
				"line's own wording so that grepping for it does not match here.)",
				xCollider.GetParentEntity().GetName().c_str(), xID.m_uIndex,
				xWatch.m_xBodyID.m_uID, xCurrentBody.m_uID,
				g_uFramesSinceLoad, g_fSecondsSinceLoad,
				xPosition.x, xPosition.y, xPosition.z,
				xScale.x, xScale.y, xScale.z,
				(int)xCollider.GetCollisionVolumeType());
		}
		xWatch.m_xBodyID = xCurrentBody;

		// ---- Descent onset ---------------------------------------------------
		if (xWatch.m_bSeeded)
		{
			const float fDrop = xWatch.m_fLastY - xPosition.y;
			if (fDrop > fONSET_PER_FRAME_EPSILON)
			{
				xWatch.m_fAccumulatedDrop += fDrop;
			}
			else
			{
				// Any non-descending frame ends the run: a body that stopped is
				// settling or standing, not falling.
				xWatch.m_fAccumulatedDrop = 0.0f;
				xWatch.m_bOnsetReported = false;
			}
		}
		xWatch.m_fLastY = xPosition.y;
		xWatch.m_bSeeded = true;

		const Zenith_Maths::Vector3 xOnsetVelocity = xPhysics.GetLinearVelocity(xCurrentBody);
		if (xWatch.m_fAccumulatedDrop > fONSET_ACCUMULATED_DROP
			&& xOnsetVelocity.y < -fONSET_MIN_FALL_SPEED
			&& !xWatch.m_bOnsetReported)
		{
			xWatch.m_bOnsetReported = true;
			Zenith_Maths::Vector3 xScale;
			xTransform.GetScale(xScale);
			const Zenith_Maths::Vector3 xVelocity = xOnsetVelocity;
			Zenith_Error(LOG_CATEGORY_PHYSICS,
				"[FallenBody] '%s' (entity %u) DESCENT ONSET after %.2f m of continuous "
				"descent: pos=(%.2f, %.2f, %.2f) vel=(%.2f, %.2f, %.2f) "
				"scale=(%.3f, %.3f, %.3f) body=%u volumeType=%d "
				"framesSinceLoad=%u timeSinceLoad=%.2fs. This is the moment support was "
				"lost -- compare it against any COLLIDER REBUILT line above for the same "
				"entity.",
				xCollider.GetParentEntity().GetName().c_str(), xID.m_uIndex,
				xWatch.m_fAccumulatedDrop,
				xPosition.x, xPosition.y, xPosition.z,
				xVelocity.x, xVelocity.y, xVelocity.z,
				xScale.x, xScale.y, xScale.z,
				xCurrentBody.m_uID, (int)xCollider.GetCollisionVolumeType(),
				g_uFramesSinceLoad, g_fSecondsSinceLoad);
		}

		// ---- Left the world --------------------------------------------------
		if (xPosition.y >= fFALLEN_BODY_Y_THRESHOLD)
		{
			xWatch.m_bLeftWorldReported = false;
			return;
		}
		if (xWatch.m_bLeftWorldReported)
		{
			return;
		}
		xWatch.m_bLeftWorldReported = true;

		const Zenith_Maths::Vector3 xVelocity = xPhysics.GetLinearVelocity(xCurrentBody);
		Zenith_Error(LOG_CATEGORY_PHYSICS,
			"[FallenBody] '%s' (entity %u) has left the world: y=%.2f below the %.1f "
			"threshold, pos=(%.2f, %.2f, %.2f) vel=(%.2f, %.2f, %.2f) "
			"framesSinceLoad=%u timeSinceLoad=%.2fs. A SINGLE body falling through a "
			"floor other bodies stand on is NOT the terrain-collision failure "
			"(Zenith_TerrainPhysicsValidate reports that separately, and would have "
			"logged body=NO) -- look at this entity's own collider, and at the DESCENT "
			"ONSET line above, which names the moment support was actually lost.",
			xCollider.GetParentEntity().GetName().c_str(), xID.m_uIndex,
			xPosition.y, fFALLEN_BODY_Y_THRESHOLD,
			xPosition.x, xPosition.y, xPosition.z,
			xVelocity.x, xVelocity.y, xVelocity.z,
			g_uFramesSinceLoad, g_fSecondsSinceLoad);
	});
}
