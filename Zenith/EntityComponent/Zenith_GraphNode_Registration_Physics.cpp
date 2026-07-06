#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "Scripting/Zenith_GraphBlackboard.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Zenith_GraphNodeHelpers.h"
#include "EntityComponent/Zenith_PhysicsQuery.h"
#include "Physics/Zenith_Physics.h"

//------------------------------------------------------------------------------
// Engine Behaviour Graph node library - Physics domain.
//
// Every node targets self or the entity in m_strTargetVar (the standard
// entity-targeting convention) and resolves the body through the canonical
// chain: ResolveTargetEntity -> TryGetComponent<Zenith_ColliderComponent> ->
// HasValidBody -> g_xEngine.Physics().Method(GetBodyID(), ...). A missing /
// bodyless / dead target is FAILURE - chains fail closed.
//
// Body-state caveats a graph author must know (from the Physics impl):
//   - AddImpulse is a mass-independent velocity DELTA (m/s), not N*s.
//   - AddForce (Newtons) is consumed by the next physics step - continuous
//     thrust means firing the node every frame (OnUpdate chains).
//   - LockRotation is ONE-WAY: passing false does not restore inertia.
//   - RebuildCollider (any SetScale) silently resets sensor/gravity/lock
//     state - re-apply after scale changes.
//------------------------------------------------------------------------------

namespace
{
	// The canonical entity->live-body resolution. Null = FAILURE the chain.
	Zenith_ColliderComponent* ResolveTargetBody(Zenith_GraphContext& xContext, const std::string& strTargetVar)
	{
		Zenith_Entity xTarget = xContext.ResolveTargetEntity(strTargetVar);
		if (!xTarget.IsValid())
		{
			return nullptr;
		}
		Zenith_ColliderComponent* pxCollider = xTarget.TryGetComponent<Zenith_ColliderComponent>();
		if (pxCollider == nullptr || !pxCollider->HasValidBody())
		{
			return nullptr;
		}
		if (!g_xEngine.Physics().HasActiveSimulation())
		{
			return nullptr;
		}
		return pxCollider;
	}

	// Instant velocity change (world-space, m/s - the engine's AddImpulse is a
	// mass-independent velocity delta, not N*s). Wakes sleeping bodies.
	class Zenith_GraphNode_ApplyImpulse : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_ApplyImpulse)
	public:
		ZENITH_PROPERTY(Zenith_Maths::Vector3, m_xImpulse, Zenith_Maths::Vector3(0.0f, 5.0f, 0.0f))
		ZENITH_PROPERTY(std::string, m_strImpulseVar, "")
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_ColliderComponent* pxCollider = ResolveTargetBody(xContext, m_strTargetVar);
			if (pxCollider == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			const Zenith_Maths::Vector3 xImpulse = m_strImpulseVar.empty()
				? m_xImpulse : xContext.m_pxBlackboard->GetVector3(m_strImpulseVar, m_xImpulse);
			g_xEngine.Physics().AddImpulse(pxCollider->GetBodyID(), xImpulse);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "ApplyImpulse"; }
	};

	// Continuous force (Newtons). Jolt consumes the accumulated force on the
	// next physics step - thrust chains must fire this every frame.
	class Zenith_GraphNode_ApplyForce : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_ApplyForce)
	public:
		ZENITH_PROPERTY(Zenith_Maths::Vector3, m_xForce, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f))
		ZENITH_PROPERTY(std::string, m_strForceVar, "")
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_ColliderComponent* pxCollider = ResolveTargetBody(xContext, m_strTargetVar);
			if (pxCollider == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			const Zenith_Maths::Vector3 xForce = m_strForceVar.empty()
				? m_xForce : xContext.m_pxBlackboard->GetVector3(m_strForceVar, m_xForce);
			g_xEngine.Physics().AddForce(pxCollider->GetBodyID(), xForce);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "ApplyForce"; }
	};

	// Sets linear velocity (m/s) with per-axis preserve: an axis whose flag is
	// false keeps the body's current velocity on that axis - the canonical
	// "drive XZ, let gravity own Y" locomotion primitive.
	class Zenith_GraphNode_SetVelocity : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_SetVelocity)
	public:
		ZENITH_PROPERTY(Zenith_Maths::Vector3, m_xVelocity, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f))
		ZENITH_PROPERTY(std::string, m_strVelocityVar, "")
		ZENITH_PROPERTY(bool, m_bSetX, true)
		ZENITH_PROPERTY(bool, m_bSetY, true)
		ZENITH_PROPERTY(bool, m_bSetZ, true)
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_ColliderComponent* pxCollider = ResolveTargetBody(xContext, m_strTargetVar);
			if (pxCollider == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_Maths::Vector3 xVelocity = m_strVelocityVar.empty()
				? m_xVelocity : xContext.m_pxBlackboard->GetVector3(m_strVelocityVar, m_xVelocity);
			if (!m_bSetX || !m_bSetY || !m_bSetZ)
			{
				const Zenith_Maths::Vector3 xCurrent = g_xEngine.Physics().GetLinearVelocity(pxCollider->GetBodyID());
				if (!m_bSetX) { xVelocity.x = xCurrent.x; }
				if (!m_bSetY) { xVelocity.y = xCurrent.y; }
				if (!m_bSetZ) { xVelocity.z = xCurrent.z; }
			}
			g_xEngine.Physics().SetLinearVelocity(pxCollider->GetBodyID(), xVelocity);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "SetVelocity"; }
	};

	// Linear velocity (m/s) -> vec3 var. Compose with MathBlackboardVector3
	// (length op) for speed gates.
	class Zenith_GraphNode_ReadVelocity : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_ReadVelocity)
	public:
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")
		ZENITH_PROPERTY(std::string, m_strResultVar, "velocity")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_ColliderComponent* pxCollider = ResolveTargetBody(xContext, m_strTargetVar);
			if (pxCollider == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_PropertyValue xValue;
			xValue.SetVector3(g_xEngine.Physics().GetLinearVelocity(pxCollider->GetBodyID()));
			xContext.m_pxBlackboard->SetValue(m_strResultVar, xValue);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "ReadVelocity"; }
	};

	// Sets angular velocity (rad/s). Note: the engine call does not wake a
	// sleeping body - pair with an impulse/force if the target may sleep.
	class Zenith_GraphNode_SetAngularVelocity : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_SetAngularVelocity)
	public:
		ZENITH_PROPERTY(Zenith_Maths::Vector3, m_xAngularVelocity, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f))
		ZENITH_PROPERTY(std::string, m_strVelocityVar, "")
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_ColliderComponent* pxCollider = ResolveTargetBody(xContext, m_strTargetVar);
			if (pxCollider == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			const Zenith_Maths::Vector3 xVelocity = m_strVelocityVar.empty()
				? m_xAngularVelocity : xContext.m_pxBlackboard->GetVector3(m_strVelocityVar, m_xAngularVelocity);
			g_xEngine.Physics().SetAngularVelocity(pxCollider->GetBodyID(), xVelocity);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "SetAngularVelocity"; }
	};

	// Locks rotation on the flagged axes (dynamic bodies only). ONE-WAY: the
	// engine never restores inertia, so false only means "leave this axis as
	// it is today" - the upright-character primitive (lock X+Z).
	class Zenith_GraphNode_LockRotation : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_LockRotation)
	public:
		ZENITH_PROPERTY(bool, m_bLockX, true)
		ZENITH_PROPERTY(bool, m_bLockY, false)
		ZENITH_PROPERTY(bool, m_bLockZ, true)
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_ColliderComponent* pxCollider = ResolveTargetBody(xContext, m_strTargetVar);
			if (pxCollider == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			g_xEngine.Physics().LockRotation(pxCollider->GetBodyID(), m_bLockX, m_bLockY, m_bLockZ);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "LockRotation"; }
	};

	// Per-body gravity toggle (factor 1/0). Enabling wakes the body.
	class Zenith_GraphNode_SetGravityEnabled : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_SetGravityEnabled)
	public:
		ZENITH_PROPERTY(bool, m_bEnabled, true)
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_ColliderComponent* pxCollider = ResolveTargetBody(xContext, m_strTargetVar);
			if (pxCollider == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			g_xEngine.Physics().SetGravityEnabled(pxCollider->GetBodyID(), m_bEnabled);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "SetGravityEnabled"; }
	};

	// Runtime sensor (trigger) toggle: overlap events still fire, no pushback.
	class Zenith_GraphNode_SetSensor : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_SetSensor)
	public:
		ZENITH_PROPERTY(bool, m_bSensor, true)
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_ColliderComponent* pxCollider = ResolveTargetBody(xContext, m_strTargetVar);
			if (pxCollider == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			pxCollider->SetIsSensor(m_bSensor);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "SetSensor"; }
	};

	// World raycast. Origin = position ref ("" = self, vec3 or EntityID var)
	// + constant offset; direction = const or vec3 var (normalized by the
	// engine). Self is excluded by default. Hit -> SUCCESS + hit vars stashed;
	// no hit -> FAILURE (the chain-gate pattern, like Gate).
	class Zenith_GraphNode_Raycast : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_Raycast)
	public:
		ZENITH_PROPERTY(std::string, m_strOriginVar, "")
		ZENITH_PROPERTY(Zenith_Maths::Vector3, m_xOriginOffset, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f))
		ZENITH_PROPERTY(Zenith_Maths::Vector3, m_xDirection, Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f))
		ZENITH_PROPERTY(std::string, m_strDirectionVar, "")
		ZENITH_PROPERTY_RANGED(float, m_fMaxDistance, 100.0f, 0.01f, 100000.0f)
		ZENITH_PROPERTY(bool, m_bIgnoreSelf, true)
		ZENITH_PROPERTY(std::string, m_strHitEntityVar, "hitEntity")
		ZENITH_PROPERTY(std::string, m_strHitPointVar, "hitPoint")
		ZENITH_PROPERTY(std::string, m_strHitNormalVar, "")
		ZENITH_PROPERTY(std::string, m_strHitDistanceVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_Maths::Vector3 xOrigin;
			if (!Zenith_GraphNode_ResolvePositionRef(xContext, m_strOriginVar, xOrigin))
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			xOrigin += m_xOriginOffset;
			const Zenith_Maths::Vector3 xDirection = m_strDirectionVar.empty()
				? m_xDirection : xContext.m_pxBlackboard->GetVector3(m_strDirectionVar, m_xDirection);
			if (glm::dot(xDirection, xDirection) < 0.0001f)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}

			const Zenith_EntityID xIgnoreID = (m_bIgnoreSelf && xContext.m_xSelf.IsValid())
				? xContext.m_xSelf.GetEntityID() : INVALID_ENTITY_ID;
			const Zenith_Physics::RaycastResult xResult =
				Zenith_PhysicsQuery::RaycastIgnoring(xOrigin, xDirection, m_fMaxDistance, xIgnoreID);
			if (!xResult.m_bHit)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}

			Zenith_PropertyValue xValue;
			if (!m_strHitEntityVar.empty())
			{
				xValue.SetPackedEntityID(xResult.m_xHitEntity.GetPacked());
				xContext.m_pxBlackboard->SetValue(m_strHitEntityVar, xValue);
			}
			if (!m_strHitPointVar.empty())
			{
				xValue.SetVector3(xResult.m_xHitPoint);
				xContext.m_pxBlackboard->SetValue(m_strHitPointVar, xValue);
			}
			if (!m_strHitNormalVar.empty())
			{
				xValue.SetVector3(xResult.m_xHitNormal);
				xContext.m_pxBlackboard->SetValue(m_strHitNormalVar, xValue);
			}
			if (!m_strHitDistanceVar.empty())
			{
				xValue.SetFloat(xResult.m_fDistance);
				xContext.m_pxBlackboard->SetValue(m_strHitDistanceVar, xValue);
			}
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "Raycast"; }
	};

	// Places the target at a position ref + offset. Default path is
	// TransformComponent::SetPosition (already body-aware: mirrors a live body
	// keeping velocity + rotation). m_bTeleport additionally zeroes both
	// velocities and routes through Zenith_Physics::TeleportBody (respawn /
	// reset semantics - NOTE: body rotation resets to identity). Doctrine:
	// teleporting is for respawn/reset, never for gameplay movement.
	class Zenith_GraphNode_SetEntityPosition : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_SetEntityPosition)
	public:
		ZENITH_PROPERTY(std::string, m_strPositionVar, "pos")
		ZENITH_PROPERTY(Zenith_Maths::Vector3, m_xOffset, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f))
		ZENITH_PROPERTY(bool, m_bTeleport, false)
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_Entity xTarget = xContext.ResolveTargetEntity(m_strTargetVar);
			if (!xTarget.IsValid())
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_TransformComponent* pxTransform = xTarget.TryGetComponent<Zenith_TransformComponent>();
			if (pxTransform == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_Maths::Vector3 xPosition;
			if (!Zenith_GraphNode_ResolvePositionRef(xContext, m_strPositionVar, xPosition))
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			xPosition += m_xOffset;

			if (m_bTeleport)
			{
				Zenith_ColliderComponent* pxCollider = xTarget.TryGetComponent<Zenith_ColliderComponent>();
				if (pxCollider != nullptr && pxCollider->HasValidBody() && g_xEngine.Physics().HasActiveSimulation())
				{
					const Zenith_PhysicsBodyID xBodyID = pxCollider->GetBodyID();
					g_xEngine.Physics().SetLinearVelocity(xBodyID, Zenith_Maths::Vector3(0.0f));
					g_xEngine.Physics().SetAngularVelocity(xBodyID, Zenith_Maths::Vector3(0.0f));
					// Fires the pose-changed hook, committing the transform this frame.
					g_xEngine.Physics().TeleportBody(xBodyID, xPosition);
					return GRAPH_NODE_STATUS_SUCCESS;
				}
			}
			pxTransform->SetPosition(xPosition);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "SetEntityPosition"; }
	};
}

void Zenith_RegisterEngineGraphNodes_Physics()
{
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();

	xRegistry.RegisterNodeType<Zenith_GraphNode_ApplyImpulse>("ApplyImpulse", GRAPH_EVENT_NONE, 1, false, "Physics");
	xRegistry.RegisterNodeType<Zenith_GraphNode_ApplyForce>("ApplyForce", GRAPH_EVENT_NONE, 1, false, "Physics");
	xRegistry.RegisterNodeType<Zenith_GraphNode_SetVelocity>("SetVelocity", GRAPH_EVENT_NONE, 1, false, "Physics");
	xRegistry.RegisterNodeType<Zenith_GraphNode_ReadVelocity>("ReadVelocity", GRAPH_EVENT_NONE, 1, false, "Physics");
	xRegistry.RegisterNodeType<Zenith_GraphNode_SetAngularVelocity>("SetAngularVelocity", GRAPH_EVENT_NONE, 1, false, "Physics");
	xRegistry.RegisterNodeType<Zenith_GraphNode_LockRotation>("LockRotation", GRAPH_EVENT_NONE, 1, false, "Physics");
	xRegistry.RegisterNodeType<Zenith_GraphNode_SetGravityEnabled>("SetGravityEnabled", GRAPH_EVENT_NONE, 1, false, "Physics");
	xRegistry.RegisterNodeType<Zenith_GraphNode_SetSensor>("SetSensor", GRAPH_EVENT_NONE, 1, false, "Physics");
	xRegistry.RegisterNodeType<Zenith_GraphNode_Raycast>("Raycast", GRAPH_EVENT_NONE, 1, false, "Physics");
	xRegistry.RegisterNodeType<Zenith_GraphNode_SetEntityPosition>("SetEntityPosition", GRAPH_EVENT_NONE, 1, false, "Physics");
}
