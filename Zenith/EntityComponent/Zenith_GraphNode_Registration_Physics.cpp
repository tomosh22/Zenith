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
//
// ★ THE PINS IN THIS TU ARE LIVE (B-6.3). Every INPUT descriptor is read through
// Zenith_GraphNode::GetInput and every OUTPUT descriptor is written through
// SetOutput, so a wire into or out of any of these 10 nodes carries a value. An
// UNCONNECTED node behaves byte-for-byte as it did (with ONE divergence: an OUTPUT
// whose var name reads EMPTY - ReadVelocity - no longer creates a blackboard
// variable named ""): the var-name fallback IS the
// old `var.empty() ? const : bb->GetVector3(var, const)` read, and SetOutput's
// dual-write IS the old SetValue. Each node that addresses a pin declares
// `static constexpr u_int uPIN_<Name>` immediately before its pin table (the
// INDEX is the runtime address; table order is the contract, asserted by
// GraphPinTable.PhysicsPinIndicesMatchTables).
//
// Three things specific to THIS TU:
//   - EVERY input read sits AFTER the node's body guard, exactly where its
//     blackboard read sat, so a bodyless target FAILURE reads no pin at all
//     (fallback count 0). SetVelocity's read stays ABOVE the per-axis-preserve
//     branch - the preserve only overwrites components, it does not decide
//     whether the value is fetched.
//   - RAYCAST IS THE EXCEPTION and deliberately so: Direction is read after the
//     origin resolve but BEFORE the zero-direction and NO-HIT exits, which is
//     where its blackboard read has always been. A zero-direction or MISSING
//     execution therefore HAS already read the pin (and, in a graph, pulled
//     Direction's producer).
//   - A MISS returns FAILURE without touching a slot: the four hit slots keep
//     whatever they last held (their stamped zeros on a fresh instance). A hit
//     latches all four unconditionally - the four `!m_strHitXVar.empty()` guards
//     are gone, and the dual-write's own non-empty rule reproduces exactly which
//     of them reach the blackboard. ReadVelocity has the same shape: a bodyless
//     target FAILURES before the write. A wire off either node's output must be
//     gated on SUCCESS (Raycast's FAILURE exec pin is how a graph expresses it).
//
// ★ An ENTITY_ID slot's stamped zero is packed 0 = {index 0, generation 0},
// which is a LEGAL entity id and NOT INVALID_ENTITY_ID (index 0xFFFFFFFF). A
// consumer that reads HitEntity without gating on SUCCESS therefore sees a
// plausible entity, not an obviously-invalid one.
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

		// Target reaches xContext.ResolveTargetEntity through ResolveTargetBody
		// (this TU's resolver, top of file), so it is an ENTITY reference, not a
		// value input. Every node in this TU targets the same way - a target
		// reference is never a wire, so it declares no uPIN_ constant and its
		// resolution stays direct.
		static constexpr u_int uPIN_Impulse = 0u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_ApplyImpulse)
		ZENITH_GRAPH_PIN_INPUT_VAR_OR_CONST(Impulse, "m_strImpulseVar", "m_xImpulse", PROPERTY_TYPE_VECTOR3)
		ZENITH_GRAPH_PIN_TARGET_ENTITY(Target, "m_strTargetVar")
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_ColliderComponent* pxCollider = ResolveTargetBody(xContext, m_strTargetVar);
			if (pxCollider == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			// After the body guard, exactly where the blackboard read sat.
			const Zenith_Maths::Vector3 xImpulse = GetInput<Zenith_Maths::Vector3>(xContext, uPIN_Impulse);
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

		static constexpr u_int uPIN_Force = 0u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_ApplyForce)
		ZENITH_GRAPH_PIN_INPUT_VAR_OR_CONST(Force, "m_strForceVar", "m_xForce", PROPERTY_TYPE_VECTOR3)
		ZENITH_GRAPH_PIN_TARGET_ENTITY(Target, "m_strTargetVar")
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_ColliderComponent* pxCollider = ResolveTargetBody(xContext, m_strTargetVar);
			if (pxCollider == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			const Zenith_Maths::Vector3 xForce = GetInput<Zenith_Maths::Vector3>(xContext, uPIN_Force);
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

		// The three per-axis SET flags are pin-less consts with no var partner:
		// they select which components of Velocity land, not a blackboard value.
		static constexpr u_int uPIN_Velocity = 0u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_SetVelocity)
		ZENITH_GRAPH_PIN_INPUT_VAR_OR_CONST(Velocity, "m_strVelocityVar", "m_xVelocity", PROPERTY_TYPE_VECTOR3)
		ZENITH_GRAPH_PIN_TARGET_ENTITY(Target, "m_strTargetVar")
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_ColliderComponent* pxCollider = ResolveTargetBody(xContext, m_strTargetVar);
			if (pxCollider == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			// UNCONDITIONAL, and ABOVE the per-axis branch: the preserve flags
			// overwrite components of the value, they do not decide whether it is
			// fetched. Folding the read into the branch would stop a wired
			// producer from being pulled on an all-axes-preserve instance.
			Zenith_Maths::Vector3 xVelocity = GetInput<Zenith_Maths::Vector3>(xContext, uPIN_Velocity);
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

		// Result is the node's own COMPUTED value (SetVector3 + SetOutput in the
		// Execute below), so it registers a writer - OUTPUT, not SELECTOR_WRITE.
		//
		// ★ INDEX 1, NOT 0. Target is declared first, so a copy of the sibling
		// TUs' `uPIN_Result = 0u` would address the TARGET_REF pin - and a
		// wrong-role SetOutput is a silent no-op, not an error.
		static constexpr u_int uPIN_Result = 1u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_ReadVelocity)
		ZENITH_GRAPH_PIN_TARGET_ENTITY(Target, "m_strTargetVar")
		ZENITH_GRAPH_PIN_OUTPUT(Result, "m_strResultVar", PROPERTY_TYPE_VECTOR3)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_ColliderComponent* pxCollider = ResolveTargetBody(xContext, m_strTargetVar);
			if (pxCollider == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_PropertyValue xValue;
			xValue.SetVector3(g_xEngine.Physics().GetLinearVelocity(pxCollider->GetBodyID()));
			SetOutput(xContext, uPIN_Result, xValue);
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

		// The const half is m_xAngularVelocity while the var half is
		// m_strVelocityVar - the pin's NAME follows neither, so the constant is
		// named for the pin (Velocity) and not for either property.
		static constexpr u_int uPIN_Velocity = 0u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_SetAngularVelocity)
		ZENITH_GRAPH_PIN_INPUT_VAR_OR_CONST(Velocity, "m_strVelocityVar", "m_xAngularVelocity", PROPERTY_TYPE_VECTOR3)
		ZENITH_GRAPH_PIN_TARGET_ENTITY(Target, "m_strTargetVar")
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_ColliderComponent* pxCollider = ResolveTargetBody(xContext, m_strTargetVar);
			if (pxCollider == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			const Zenith_Maths::Vector3 xVelocity = GetInput<Zenith_Maths::Vector3>(xContext, uPIN_Velocity);
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

		// The three lock flags are consts with no var partner - not pins. The one
		// pin is the target reference, resolved directly, so this Execute
		// addresses no pin at all and declares no uPIN_ constant.
		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_LockRotation)
		ZENITH_GRAPH_PIN_TARGET_ENTITY(Target, "m_strTargetVar")
		ZENITH_GRAPH_PINS_END

	public:
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

		// Target only: no pin is addressed, so no uPIN_ constant exists.
		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_SetGravityEnabled)
		ZENITH_GRAPH_PIN_TARGET_ENTITY(Target, "m_strTargetVar")
		ZENITH_GRAPH_PINS_END

	public:
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

		// Target only: no pin is addressed, so no uPIN_ constant exists.
		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_SetSensor)
		ZENITH_GRAPH_PIN_TARGET_ENTITY(Target, "m_strTargetVar")
		ZENITH_GRAPH_PINS_END

	public:
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
	// + constant offset; direction = a wire, a vec3 var or the const (normalized
	// by the engine). Self is excluded by default. Hit -> SUCCESS + all four hit
	// outputs latched (and dual-written where named); no hit -> FAILURE touching
	// nothing (the chain-gate pattern, like Gate).
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

		// Origin is a POSITION ref (Zenith_GraphNode_ResolvePositionRef in the
		// Execute below takes an EntityID var or a vec3 var; "" = self). The four
		// hit vars are the node's own computed results, each typed by the
		// Zenith_PropertyValue::Set* the Execute actually calls.
		// m_xOriginOffset, m_fMaxDistance and m_bIgnoreSelf are consts with no
		// var partner - not pins. Origin stays direct (a position REFERENCE is
		// never a wire), so it declares no constant.
		static constexpr u_int uPIN_Direction = 1u;
		static constexpr u_int uPIN_HitEntity = 2u;
		static constexpr u_int uPIN_HitPoint = 3u;
		static constexpr u_int uPIN_HitNormal = 4u;
		static constexpr u_int uPIN_HitDistance = 5u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_Raycast)
		ZENITH_GRAPH_PIN_TARGET_POSITION(Origin, "m_strOriginVar")
		ZENITH_GRAPH_PIN_INPUT_VAR_OR_CONST(Direction, "m_strDirectionVar", "m_xDirection", PROPERTY_TYPE_VECTOR3)
		ZENITH_GRAPH_PIN_OUTPUT(HitEntity, "m_strHitEntityVar", PROPERTY_TYPE_ENTITY_ID)
		ZENITH_GRAPH_PIN_OUTPUT(HitPoint, "m_strHitPointVar", PROPERTY_TYPE_VECTOR3)
		ZENITH_GRAPH_PIN_OUTPUT(HitNormal, "m_strHitNormalVar", PROPERTY_TYPE_VECTOR3)
		ZENITH_GRAPH_PIN_OUTPUT(HitDistance, "m_strHitDistanceVar", PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_Maths::Vector3 xOrigin;
			if (!Zenith_GraphNode_ResolvePositionRef(xContext, m_strOriginVar, xOrigin))
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			xOrigin += m_xOriginOffset;
			// ★ EXACTLY WHERE THE BLACKBOARD READ WAS, which for this node is
			// BEFORE the zero-direction and NO-HIT exits rather than after every
			// guard. Moving it down would change which executions read the pin.
			const Zenith_Maths::Vector3 xDirection = GetInput<Zenith_Maths::Vector3>(xContext, uPIN_Direction);
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
				// A MISS TOUCHES NO SLOT - deliberately. Latching zeros here would
				// hand a consumer a hit at the origin with distance 0; leaving the
				// slots alone means a fresh instance still reads its stamped zeros
				// and a re-cast that misses keeps the PREVIOUS hit, which is what
				// the FAILURE exec pin is there to gate on.
				return GRAPH_NODE_STATUS_FAILURE;
			}

			// A HIT latches all four, unconditionally. The four
			// `!m_strHitXVar.empty()` guards are GONE: SetOutput's dual-write
			// applies the same non-empty rule, so which of the four reach the
			// blackboard is unchanged (hitEntity / hitPoint by default, the other
			// two only when the author names them) while the SLOTS now always
			// carry the hit - which is what a wire reads. HitEntity is written
			// even when the hit body carries no entity (INVALID_ENTITY_ID packed),
			// exactly as today; an IsValid() guard added here would leave the slot
			// holding the PREVIOUS cast's entity while the other three describe
			// this one.
			Zenith_PropertyValue xValue;
			xValue.SetPackedEntityID(xResult.m_xHitEntity.GetPacked());
			SetOutput(xContext, uPIN_HitEntity, xValue);
			xValue.SetVector3(xResult.m_xHitPoint);
			SetOutput(xContext, uPIN_HitPoint, xValue);
			xValue.SetVector3(xResult.m_xHitNormal);
			SetOutput(xContext, uPIN_HitNormal, xValue);
			xValue.SetFloat(xResult.m_fDistance);
			SetOutput(xContext, uPIN_HitDistance, xValue);
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

		// Position is a POSITION ref; Target is resolved directly by
		// xContext.ResolveTargetEntity rather than through ResolveTargetBody
		// (this node also drives a bodyless transform), which is the same ENTITY
		// reference either way. m_xOffset and m_bTeleport are consts with no var
		// partner - not pins. Both pins are references, so this Execute addresses
		// no pin and declares no uPIN_ constant.
		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_SetEntityPosition)
		ZENITH_GRAPH_PIN_TARGET_POSITION(Position, "m_strPositionVar")
		ZENITH_GRAPH_PIN_TARGET_ENTITY(Target, "m_strTargetVar")
		ZENITH_GRAPH_PINS_END

	public:
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
				Zenith_Physics& xPhysics = g_xEngine.Physics();
				if (pxCollider != nullptr && pxCollider->HasValidBody() && xPhysics.HasActiveSimulation())
				{
					const Zenith_PhysicsBodyID xBodyID = pxCollider->GetBodyID();
					xPhysics.SetLinearVelocity(xBodyID, Zenith_Maths::Vector3(0.0f));
					xPhysics.SetAngularVelocity(xBodyID, Zenith_Maths::Vector3(0.0f));
					// Fires the pose-changed hook, committing the transform this frame.
					xPhysics.TeleportBody(xBodyID, xPosition);
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
	// On Failure = NO HIT + the two misconfiguration guards (an unresolvable
	// origin reference, a zero-length direction).
	xRegistry.RegisterNodeType<Zenith_GraphNode_Raycast>("Raycast", GRAPH_EVENT_NONE, 1, false, "Physics", true);
	xRegistry.RegisterNodeType<Zenith_GraphNode_SetEntityPosition>("SetEntityPosition", GRAPH_EVENT_NONE, 1, false, "Physics");
}

#include "EntityComponent/Zenith_GraphNode_Registration_Physics.Tests.inl"
