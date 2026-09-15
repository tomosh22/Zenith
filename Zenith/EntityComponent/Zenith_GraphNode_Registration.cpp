#include "Zenith.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_GraphComponent.h"
#include "EntityComponent/Zenith_GraphOps.h"
#include "Core/Zenith_Engine.h"
#include "ZenithECS/Zenith_SceneSystem.h"

//------------------------------------------------------------------------------
// Engine Behaviour Graph node library (Phase 1 set).
//
// This is the EntityComponent-glue-layer twin of
// Zenith_ComponentMeta_Registration.cpp: the Scripting runtime names no
// concrete component, so every node that touches one lives here.
// Zenith_Engine::Initialise installs Zenith_RegisterEngineGraphNodes via
// Zenith_GraphNodeRegistry::SetNodeRegistrar; games add their own node types
// from their project hooks (first-class from Phase 5).
//
// Movement nodes drive the TRANSFORM and are for non-physics entities;
// physics-driven movement nodes (forces/impulses through Zenith_Physics)
// arrive with the game-migration waves. Designers should not teleport
// physics bodies.
//
// ★ THE PINS IN THIS TU ARE LIVE (B-6.9). Every INPUT / INPUT_CONST descriptor is
// read through Zenith_GraphNode::GetInput or TryGetInput and every OUTPUT
// descriptor is written through SetOutput, so a wire into or out of any of them
// carries a value. An UNCONNECTED node uses its typed default; input and output
// pins have no name fallback or implicit blackboard publication.
// Each node that addresses a pin declares
// `static constexpr u_int uPIN_<Name>` immediately before its pin table (the
// INDEX is the runtime address; table order is the contract, asserted by
// GraphPinTable.RegistrationPinIndicesMatchTables). 35 node types register here;
// 24 carry a pin table (the three collision sources by INHERITANCE from the
// unregistered base) and 11 carry none.
//
// What went live, by role:
//   - INPUT plain (type-zero default): CompareBlackboardFloat.Value,
//     CompareBlackboardInt.Value, Branch.Condition, Gate.Open.
//   - INPUT plain ANY, through TryGetInput: FireCustomEvent.Payload and
//     BroadcastCustomEvent.Payload.
//   - INPUT var-or-const: Compare{Float,Int}.CompareTo, TranslateEntity
//     .UnitsPerSecond, AddBlackboardFloat.Delta, Wait.Seconds, Loop.Count.
//   - INPUT_CONST (the repo's first WIRE-ABLE constants): the five
//     SetBlackboardEntityID.Value is intentionally different: it is a normal
//     ENTITY_ID INPUT with no literal-value property, and defaults to packed 0.
//   - OUTPUT: Compare{Float,Int}.Result and StoreSelfEntityID.Variable.
//   - UNTOUCHED: GetVariable (migrated in B-3) and every SELECTOR pin -
//     SetBlackboard*.Variable and AddBlackboardFloat.Variable (READWRITE: read
//     AND written directly in one Execute), OnCustomEvent.StorePayload and the
//     collision sources' StoreEntity - plus every TARGET_ENTITY reference.
//
// ★ THE `""` DIVERGENCE, at exactly three sites: CompareBlackboardFloat.Result,
// CompareBlackboardInt.Result and StoreSelfEntityID.Variable were UNGUARDED
// named "". It no longer does - the slot still carries the value. No shipped
// observable depended on it.
//
// ★ TWO FAILURE SHAPES. Shape A - the FAILURE sits above every accessor, so a
// directly-constructed instance that takes it has built NO pin state, read no
// input and latched no slot: StoreSelfEntityID (invalid self), TranslateEntity
// (target + transform guards) and FireCustomEvent.Payload (target +
// graph-component guards). BroadcastCustomEvent.Payload reads unconditionally.
// Shape B - the FAILURE sits BELOW a read: Compare{Float,Int} on an
// out-of-range op code have already read Value and CompareTo (and, in a graph,
// already pulled their producers) before the `default:` arm returns FAILURE.
//
// ★ WHEN EACH INPUT IS READ. Branch reads Condition ONCE PER ACTIVATION (inside
// `m_iActivePin < 0`: a suspended taken branch keeps re-driving that pin without
// re-reading), and Loop reads Count once per fresh run (inside
// `m_iRemaining < 0`). Wait reads Seconds on EVERY tick including RUNNING ones -
// that is what lets a shrinking variable shorten the wait, and it means a WIRED
// Wait pulls its producer every tick. Reactive by design.
//
// ★ THE CENSUS RISES WITH THIS TU, and that is expected. Branch.m_strConditionVar
// defaults to "condition", Gate.m_strOpenVar to "open" and Compare*.m_strVar to
// "value" - all NON-EMPTY - so every PLACED Branch/Gate/Compare instance logs one
//------------------------------------------------------------------------------

namespace
{
	//==========================================================================
	// Event sources
	//==========================================================================

	// Sources are chain anchors: Execute is the per-fire gate (SUCCESS = run
	// the chain). Most sources fire unconditionally.
#define ZENITH_GRAPH_SIMPLE_SOURCE(ClassName, szName) \
	class ClassName : public Zenith_GraphNode \
	{ \
	public: \
		GraphNodeStatus Execute(Zenith_GraphContext&) override { return GRAPH_NODE_STATUS_SUCCESS; } \
		const char* GetTypeName() const override { return szName; } \
	};

	ZENITH_GRAPH_SIMPLE_SOURCE(Zenith_GraphNode_OnStart, "OnStart")
	ZENITH_GRAPH_SIMPLE_SOURCE(Zenith_GraphNode_OnUpdate, "OnUpdate")
	ZENITH_GRAPH_SIMPLE_SOURCE(Zenith_GraphNode_OnFixedUpdate, "OnFixedUpdate")
	ZENITH_GRAPH_SIMPLE_SOURCE(Zenith_GraphNode_OnEnable, "OnEnable")
	ZENITH_GRAPH_SIMPLE_SOURCE(Zenith_GraphNode_OnDisable, "OnDisable")
	ZENITH_GRAPH_SIMPLE_SOURCE(Zenith_GraphNode_OnDestroyEvent, "OnDestroy")
	ZENITH_GRAPH_SIMPLE_SOURCE(Zenith_GraphNode_OnGraphCall, "OnGraphCall")

	// Collision sources stash the other entity (packed EntityID payload) into a
	// blackboard variable so downstream nodes can use it.
	class Zenith_GraphNode_CollisionSourceBase : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_CollisionSourceBase)
	public:
		ZENITH_PROPERTY(std::string, m_strStoreEntityVar, "other")

		// Annotated on the BASE only: OnCollisionEnter/Stay/Exit declare no table
		// of their own and therefore SHARE this one (pin tables inherit exactly
		// like property tables). A PINS_BEGIN in a derived class would SHADOW it.
		// The payload is always the other entity's packed EntityID, so the pin is
		// ENTITY_ID rather than ANY.
		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_CollisionSourceBase)
		ZENITH_GRAPH_PIN_SELECTOR_WRITE(StoreEntity, "m_strStoreEntityVar", PROPERTY_TYPE_ENTITY_ID)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			if (xContext.m_pxEventPayload && !m_strStoreEntityVar.empty())
			{
				xContext.m_pxBlackboard->SetValue(m_strStoreEntityVar, *xContext.m_pxEventPayload);
			}
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "OnCollision"; }
	};

	class Zenith_GraphNode_OnCollisionEnter : public Zenith_GraphNode_CollisionSourceBase
	{
	public:
		const char* GetTypeName() const override { return "OnCollisionEnter"; }
	};
	class Zenith_GraphNode_OnCollisionStay : public Zenith_GraphNode_CollisionSourceBase
	{
	public:
		const char* GetTypeName() const override { return "OnCollisionStay"; }
	};
	class Zenith_GraphNode_OnCollisionExit : public Zenith_GraphNode_CollisionSourceBase
	{
	public:
		const char* GetTypeName() const override { return "OnCollisionExit"; }
	};

	// Fires its chain every m_fInterval seconds (ticked by the ON_UPDATE dispatch).
	class Zenith_GraphNode_Timer : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_Timer)
	public:
		ZENITH_PROPERTY_RANGED(float, m_fInterval, 1.0f, 0.01f, 3600.0f)

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			m_fAccumulated += xContext.m_fDt;
			if (m_fAccumulated < m_fInterval)
			{
				return GRAPH_NODE_STATUS_FAILURE;	// gate closed - no fire this tick
			}
			m_fAccumulated -= m_fInterval;
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "Timer"; }

	private:
		float m_fAccumulated = 0.0f;
	};

	class Zenith_GraphNode_OnCustomEvent : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_OnCustomEvent)
	public:
		ZENITH_PROPERTY(std::string, m_strEventName, "event")
		// When the firer supplied a payload, stash it into this blackboard
		// variable (same pattern as the collision sources' packed EntityID).
		ZENITH_PROPERTY(std::string, m_strStorePayloadVar, "payload")

		// The one stash var that is genuinely ANY: the firer chooses the payload
		// type. (m_strEventName is not a blackboard name and so is not a pin. The
		// FireCustomEventWithArgs args stashed below are named by the FIRER at
		// runtime - no property carries them, so no pin can either.)
		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_OnCustomEvent)
		ZENITH_GRAPH_PIN_SELECTOR_WRITE(StorePayload, "m_strStorePayloadVar", eGRAPH_PIN_TYPE_ANY)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			if (xContext.m_pxEventPayload && !m_strStorePayloadVar.empty())
			{
				xContext.m_pxBlackboard->SetValue(m_strStorePayloadVar, *xContext.m_pxEventPayload);
			}
			// Multi-field payloads (FireCustomEventWithArgs): every named arg is
			// stashed verbatim under its own name - the firer owns the naming.
			for (u_int u = 0; u < xContext.m_uEventArgCount; ++u)
			{
				const Zenith_GraphEventArg& xArg = xContext.m_pxEventArgs[u];
				if (!xArg.m_strName.empty())
				{
					xContext.m_pxBlackboard->SetValue(xArg.m_strName, xArg.m_xValue);
				}
			}
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "OnCustomEvent"; }
		bool MatchesCustomEvent(const char* szName) const override { return m_strEventName == szName; }
	};

	// Compares a blackboard float against a constant (or another variable when
	// m_strCompareVar is set) and writes the boolean result to a blackboard
	// variable - Branch consumes it. m_iOp is a Zenith_GraphCompareFloatOp.
	class Zenith_GraphNode_CompareBlackboardFloat : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_CompareBlackboardFloat)
	public:
		ZENITH_PROPERTY(float, m_fCompareTo, 0.0f)
		ZENITH_PROPERTY(int32_t, m_iOp, 0)
		// ★ m_strVar DEFAULTS TO "value", which is NOT empty - so every placed
		static constexpr u_int uPIN_Value = 0u;
		static constexpr u_int uPIN_CompareTo = 1u;
		static constexpr u_int uPIN_Result = 2u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_CompareBlackboardFloat)
		ZENITH_GRAPH_PIN_INPUT(Value, PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PIN_INPUT_CONST(CompareTo, "m_fCompareTo", PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PIN_OUTPUT(Result, PROPERTY_TYPE_BOOL)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			// SHAPE B: both reads sit ABOVE the op switch, exactly where the two
			// blackboard reads sat, so an out-of-range op code FAILS having
			// already read both pins (and, in a graph, already pulled both
			// producers). Moving them below the switch would be the "exactly
			// where it is today" violation.
			const float fValue = GetInput<float>(xContext, uPIN_Value);
			const float fCompareTo = GetInput<float>(xContext, uPIN_CompareTo);
			bool bResult = false;
			switch (static_cast<Zenith_GraphCompareFloatOp>(m_iOp))
			{
			case GRAPH_COMPARE_FLOAT_OP_LESS:          bResult = fValue <  fCompareTo; break;
			case GRAPH_COMPARE_FLOAT_OP_LESS_EQUAL:    bResult = fValue <= fCompareTo; break;
			case GRAPH_COMPARE_FLOAT_OP_GREATER:       bResult = fValue >  fCompareTo; break;
			case GRAPH_COMPARE_FLOAT_OP_GREATER_EQUAL: bResult = fValue >= fCompareTo; break;
			case GRAPH_COMPARE_FLOAT_OP_EQUAL:         bResult = fValue == fCompareTo; break;
			default: return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_PropertyValue xResult;
			xResult.SetBool(bResult);
			// UNGUARDED writer -> one of the TU's three `""` divergence sites: an
			// empty m_strResultVar latches the slot and creates no blackboard entry
			// (today's unconditional SetValue created one named "").
			SetOutput(xContext, uPIN_Result, xResult);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "CompareBlackboardFloat"; }
	};

	// Loads a registered scene by build index. Mode: 0 = single (replaces the
	// current scene set - the default, and the same call the front-end menu's
	// Play handler makes), 1 = additive, 2 = additive without loading. The
	// scene system's mid-update deferral rules apply identically. Authoring
	// note: place SINGLE-mode loads at the END of a chain - the dispatching
	// entity does not survive the load.
	class Zenith_GraphNode_LoadSceneByIndex : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_LoadSceneByIndex)
	public:
		ZENITH_PROPERTY(int32_t, m_iSceneIndex, 0)
		ZENITH_PROPERTY_RANGED(int32_t, m_iLoadMode, 0, 0, 2)

		GraphNodeStatus Execute(Zenith_GraphContext&) override
		{
			g_xEngine.Scenes().LoadSceneByIndex(m_iSceneIndex, static_cast<Zenith_SceneLoadMode>(m_iLoadMode));
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "LoadSceneByIndex"; }
	};

	//==========================================================================
	// Actions
	//==========================================================================

	class Zenith_GraphNode_DebugLog : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_DebugLog)
	public:
		ZENITH_PROPERTY(std::string, m_strMessage, "graph log")

		GraphNodeStatus Execute(Zenith_GraphContext&) override
		{
			Zenith_Log(LOG_CATEGORY_CORE, "[BehaviourGraph] %s", m_strMessage.c_str());
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "DebugLog"; }
	};

	// Yaw rotation at a designer-tuned rate. Transform-driven (non-physics
	// entities). Targets self, or the entity in m_strTargetVar when set.
	class Zenith_GraphNode_RotateEntity : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_RotateEntity)
	public:
		ZENITH_PROPERTY_RANGED(float, m_fDegreesPerSecond, 90.0f, -1080.0f, 1080.0f)
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		// m_fDegreesPerSecond has no var partner, so it is not a pin. Target is an
		// entity REFERENCE resolved directly through xContext.ResolveTargetEntity
		// and is never a wire, so this class addresses no pin and declares no
		// uPIN_ constant.
		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_RotateEntity)
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
			Zenith_Maths::Quat xRotation;
			pxTransform->GetRotation(xRotation);
			const float fRadians = glm::radians(m_fDegreesPerSecond) * xContext.m_fDt;
			const Zenith_Maths::Quat xDelta = glm::angleAxis(fRadians, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
			pxTransform->SetRotation(glm::normalize(xDelta * xRotation));
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "RotateEntity"; }
	};

	// Transform-driven linear motion (units/second, world space). The constant
	// direction can be overridden by a vec3 blackboard var (m_strUnitsVar) -
	// the blackboard-direction movement primitive.
	class Zenith_GraphNode_TranslateEntity : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_TranslateEntity)
	public:
		ZENITH_PROPERTY(Zenith_Maths::Vector3, m_xUnitsPerSecond, Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f))
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		// UnitsPerSecond is the only pin this Execute addresses; Target is a
		// reference, resolved directly.
		static constexpr u_int uPIN_UnitsPerSecond = 0u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_TranslateEntity)
		ZENITH_GRAPH_PIN_INPUT_CONST(UnitsPerSecond, "m_xUnitsPerSecond", PROPERTY_TYPE_VECTOR3)
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
			// SHAPE A: the read stays BELOW both guards, exactly where it sat, so a
			// targetless or transformless execution reads no pin at all.
			const Zenith_Maths::Vector3 xUnitsPerSecond = GetInput<Zenith_Maths::Vector3>(xContext, uPIN_UnitsPerSecond);
			Zenith_Maths::Vector3 xPosition;
			pxTransform->GetPosition(xPosition);
			pxTransform->SetPosition(xPosition + xUnitsPerSecond * xContext.m_fDt);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "TranslateEntity"; }
	};

	class Zenith_GraphNode_DestroyEntity : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_DestroyEntity)
	public:
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		// Target only: an entity reference, resolved directly. No pin is addressed,
		// so no uPIN_ constant exists.
		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_DestroyEntity)
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
			xTarget.Destroy();	// deferred to end of frame - safe mid-dispatch
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "DestroyEntity"; }
	};

	class Zenith_GraphNode_SetBlackboardBool : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_SetBlackboardBool)
	public:
		ZENITH_PROPERTY(std::string, m_strVariable, "flag")
		ZENITH_PROPERTY(bool, m_bValue, true)

		// The SetBlackboard* shape: the destination is a configured NAME
		// (SELECTOR_WRITE, never a wire, written directly below) and the value is
		// the const half, which IS a wire now.
		//
		// ★ AN INPUT_CONST PIN CARRIES NO VAR-NAME PROPERTY, so its binding's var
		// line. Unconnected, GetInput answers MakePinDefault - the const property's
		// CURRENT value - which is byte-for-byte the old direct member read. These
		// five are the library's first wire-able constants.
		static constexpr u_int uPIN_Value = 1u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_SetBlackboardBool)
		ZENITH_GRAPH_PIN_SELECTOR_WRITE(Variable, "m_strVariable", PROPERTY_TYPE_BOOL)
		ZENITH_GRAPH_PIN_INPUT_CONST(Value, "m_bValue", PROPERTY_TYPE_BOOL)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_PropertyValue xValue;
			xValue.SetBool(GetInput<bool>(xContext, uPIN_Value));
			xContext.m_pxBlackboard->SetValue(m_strVariable, xValue);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "SetBlackboardBool"; }
	};

	class Zenith_GraphNode_SetBlackboardFloat : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_SetBlackboardFloat)
	public:
		ZENITH_PROPERTY(std::string, m_strVariable, "value")
		ZENITH_PROPERTY(float, m_fValue, 0.0f)

		static constexpr u_int uPIN_Value = 1u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_SetBlackboardFloat)
		ZENITH_GRAPH_PIN_SELECTOR_WRITE(Variable, "m_strVariable", PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PIN_INPUT_CONST(Value, "m_fValue", PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_PropertyValue xValue;
			xValue.SetFloat(GetInput<float>(xContext, uPIN_Value));
			xContext.m_pxBlackboard->SetValue(m_strVariable, xValue);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "SetBlackboardFloat"; }
	};

	// variable += delta. Delta from const or var (m_strDeltaVar); m_bScaleByDt
	// multiplies by the dispatching event's dt - together the timer-decrement /
	// integrate primitive (e.g. deltaVar="speed", scaleByDt on an OnUpdate).
	class Zenith_GraphNode_AddBlackboardFloat : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_AddBlackboardFloat)
	public:
		ZENITH_PROPERTY(std::string, m_strVariable, "value")
		ZENITH_PROPERTY(float, m_fDelta, 1.0f)
		ZENITH_PROPERTY(bool, m_bScaleByDt, false)

		// READWRITE: m_strVariable is READ (:GetFloat below) and WRITTEN
		// (:SetValue below) in the same Execute - unlike Branch.m_strConditionVar,
		// which shares the shape of a condition name but is read-only.
		// m_bScaleByDt has no var partner and so is not a pin.
		//
		// ★ THE READWRITE HALF STAYS DIRECT. m_strVariable is a SELECTOR - a
		// configured destination NAME, never a wire - so both its read and its
		// write-back below are plain blackboard calls, exactly as in _Math's Add*
		// family. Only Delta went live.
		static constexpr u_int uPIN_Delta = 1u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_AddBlackboardFloat)
		ZENITH_GRAPH_PIN_SELECTOR_READWRITE(Variable, "m_strVariable", PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PIN_INPUT_CONST(Delta, "m_fDelta", PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			// Read FIRST, exactly where the ternary sat: the dt scale applies to
			// whatever the pin delivered, wire or const.
			float fDelta = GetInput<float>(xContext, uPIN_Delta);
			if (m_bScaleByDt)
			{
				fDelta *= xContext.m_fDt;
			}
			Zenith_PropertyValue xValue;
			xValue.SetFloat(xContext.m_pxBlackboard->GetFloat(m_strVariable, 0.0f) + fDelta);
			xContext.m_pxBlackboard->SetValue(m_strVariable, xValue);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "AddBlackboardFloat"; }
	};

	class Zenith_GraphNode_SetBlackboardInt : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_SetBlackboardInt)
	public:
		ZENITH_PROPERTY(std::string, m_strVariable, "value")
		ZENITH_PROPERTY(int32_t, m_iValue, 0)

		static constexpr u_int uPIN_Value = 1u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_SetBlackboardInt)
		ZENITH_GRAPH_PIN_SELECTOR_WRITE(Variable, "m_strVariable", PROPERTY_TYPE_INT32)
		ZENITH_GRAPH_PIN_INPUT_CONST(Value, "m_iValue", PROPERTY_TYPE_INT32)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_PropertyValue xValue;
			xValue.SetInt32(GetInput<int32_t>(xContext, uPIN_Value));
			xContext.m_pxBlackboard->SetValue(m_strVariable, xValue);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "SetBlackboardInt"; }
	};

	class Zenith_GraphNode_SetBlackboardVector3 : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_SetBlackboardVector3)
	public:
		ZENITH_PROPERTY(std::string, m_strVariable, "vec")
		ZENITH_PROPERTY(Zenith_Maths::Vector3, m_xValue, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f))

		static constexpr u_int uPIN_Value = 1u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_SetBlackboardVector3)
		ZENITH_GRAPH_PIN_SELECTOR_WRITE(Variable, "m_strVariable", PROPERTY_TYPE_VECTOR3)
		ZENITH_GRAPH_PIN_INPUT_CONST(Value, "m_xValue", PROPERTY_TYPE_VECTOR3)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_PropertyValue xValue;
			xValue.SetVector3(GetInput<Zenith_Maths::Vector3>(xContext, uPIN_Value));
			xContext.m_pxBlackboard->SetValue(m_strVariable, xValue);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "SetBlackboardVector3"; }
	};

	// Entity IDs deliberately have no literal-value property. A graph obtains
	// one from an ENTITY_ID producer and routes it here; the destination remains
	// a configured blackboard selector, just like the other typed setters.
	class Zenith_GraphNode_SetBlackboardEntityID : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_SetBlackboardEntityID)
	public:
		ZENITH_PROPERTY(std::string, m_strVariable, "value")

		// Value is a normal INPUT rather than INPUT_CONST: ENTITY_ID has no
		// authorable literal form here. An unconnected input stamps typed packed
		// zero through MakePinDefault.
		static constexpr u_int uPIN_Value = 1u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_SetBlackboardEntityID)
		ZENITH_GRAPH_PIN_SELECTOR_WRITE(Variable, "m_strVariable", PROPERTY_TYPE_ENTITY_ID)
		ZENITH_GRAPH_PIN_INPUT(Value, PROPERTY_TYPE_ENTITY_ID)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_PropertyValue xValue;
			xValue.SetPackedEntityID(GetInputPackedEntityID(xContext, uPIN_Value));
			xContext.m_pxBlackboard->SetValue(m_strVariable, xValue);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "SetBlackboardEntityID"; }
	};

	class Zenith_GraphNode_SetBlackboardString : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_SetBlackboardString)
	public:
		ZENITH_PROPERTY(std::string, m_strVariable, "text")
		ZENITH_PROPERTY(std::string, m_strValue, "")

		// m_strValue is the literal STRING written, not a variable name (it does
		// not match the m_str*Var* matcher either) - hence INPUT_CONST, which
		static constexpr u_int uPIN_Value = 1u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_SetBlackboardString)
		ZENITH_GRAPH_PIN_SELECTOR_WRITE(Variable, "m_strVariable", PROPERTY_TYPE_STRING)
		ZENITH_GRAPH_PIN_INPUT_CONST(Value, "m_strValue", PROPERTY_TYPE_STRING)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_PropertyValue xValue;
			xValue.SetString(GetInput<std::string>(xContext, uPIN_Value));
			xContext.m_pxBlackboard->SetValue(m_strVariable, xValue);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "SetBlackboardString"; }
	};

	//--------------------------------------------------------------------------
	// GetVariable - the READ half of the blackboard, and the one node through
	// which a declared variable becomes a WIRE.
	//
	// ★ ITS OUTPUT TYPE COMES FROM THE GRAPH. ZENITH_GRAPH_PIN_OUTPUT_FROM_VARIABLE
	// types the Value pin as the DECLARED type of the variable m_strVariable
	// names, so a wire out of it is checked at author time against the
	// declaration - there is no "GetVariableFloat" family and no per-type node.
	// The SELECTOR_READ beside it is the blackboard read itself, and it is what
	// declare-or-error reports when the variable is undeclared; the from-variable
	// descriptor binds nothing and writes nothing, so reading a variable can
	// never register this node as a WRITER of it.
	//
	// PURE: it has no exec pins and evaluates on demand inside its consumer's
	// gather, memoised for that one gather (Zenith_BehaviourGraph::PullSlot).
	//--------------------------------------------------------------------------
	class Zenith_GraphNode_GetVariable : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_GetVariable)
	public:
		ZENITH_PROPERTY(std::string, m_strVariable, "value")

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_GetVariable)
		ZENITH_GRAPH_PIN_SELECTOR_READ(Variable, "m_strVariable", eGRAPH_PIN_TYPE_ANY)
		ZENITH_GRAPH_PIN_OUTPUT_FROM_VARIABLE(Value, "m_strVariable")
		ZENITH_GRAPH_PINS_END

	public:
		// The Value pin's index in the table above.
		static constexpr u_int uVALUE_PIN = 1u;

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			if (xContext.m_pxBlackboard == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			const Zenith_PropertyValue* pxValue = xContext.m_pxBlackboard->TryGetValue(m_strVariable);
			if (pxValue == nullptr)
			{
				// A MISSING variable is FAILURE and nothing else: the pull yields
				// the consumer's own pin default plus one [GraphPin] STATUS line.
				// Writing a fabricated zero here would be indistinguishable, to the
				// consumer, from a variable that really held zero.
				return GRAPH_NODE_STATUS_FAILURE;
			}
			// ★ THE TAG IS COMPARED HERE, not left to SetOutput's refusal. SetOutput
			// would leave the slot at its stamped zero - still SET - and this node
			// would then return SUCCESS while the consumer silently read that zero.
			// The same FAILURE shape as the missing-variable case is the honest
			// answer: the live value is not the declared type.
			const Zenith_PropertyType eSlot = GetOutputPinType(uVALUE_PIN);
			if (eSlot != eGRAPH_PIN_TYPE_ANY && pxValue->GetType() != eSlot)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			SetOutput(xContext, uVALUE_PIN, *pxValue);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "GetVariable"; }
	};

	// Integer sibling of CompareBlackboardFloat. Compares a blackboard int32
	// against a constant, or against another variable when m_strCompareVar is
	// set. m_iOp is a Zenith_GraphCompareIntOp (adds NOT_EQUAL over the float node).
	class Zenith_GraphNode_CompareBlackboardInt : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_CompareBlackboardInt)
	public:
		ZENITH_PROPERTY(int32_t, m_iCompareTo, 0)
		ZENITH_PROPERTY(int32_t, m_iOp, 4)
		// Same index map as the float sibling, and the same non-empty "value"
		static constexpr u_int uPIN_Value = 0u;
		static constexpr u_int uPIN_CompareTo = 1u;
		static constexpr u_int uPIN_Result = 2u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_CompareBlackboardInt)
		ZENITH_GRAPH_PIN_INPUT(Value, PROPERTY_TYPE_INT32)
		ZENITH_GRAPH_PIN_INPUT_CONST(CompareTo, "m_iCompareTo", PROPERTY_TYPE_INT32)
		ZENITH_GRAPH_PIN_OUTPUT(Result, PROPERTY_TYPE_BOOL)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			// SHAPE B, as the float sibling: both reads precede the op switch.
			const int32_t iValue = GetInput<int32_t>(xContext, uPIN_Value);
			const int32_t iCompareTo = GetInput<int32_t>(xContext, uPIN_CompareTo);
			bool bResult = false;
			switch (static_cast<Zenith_GraphCompareIntOp>(m_iOp))
			{
			case GRAPH_COMPARE_INT_OP_LESS:          bResult = iValue <  iCompareTo; break;
			case GRAPH_COMPARE_INT_OP_LESS_EQUAL:    bResult = iValue <= iCompareTo; break;
			case GRAPH_COMPARE_INT_OP_GREATER:       bResult = iValue >  iCompareTo; break;
			case GRAPH_COMPARE_INT_OP_GREATER_EQUAL: bResult = iValue >= iCompareTo; break;
			case GRAPH_COMPARE_INT_OP_EQUAL:         bResult = iValue == iCompareTo; break;
			case GRAPH_COMPARE_INT_OP_NOT_EQUAL:     bResult = iValue != iCompareTo; break;
			default: return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_PropertyValue xResult;
			xResult.SetBool(bResult);
			// UNGUARDED writer -> the second of the TU's three `""` divergence sites.
			SetOutput(xContext, uPIN_Result, xResult);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "CompareBlackboardInt"; }
	};

	// Writes self's packed EntityID to a blackboard variable - the wiring
	// primitive for entity-targeting and cross-entity event chains.
	class Zenith_GraphNode_StoreSelfEntityID : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_StoreSelfEntityID)
	public:
		// OUTPUT, not SELECTOR_WRITE: the value is this node's own COMPUTED
		// result (self's packed EntityID), not a value routed from a const.
		// ENTITY_ID has no Zenith_PropertyTraits specialisation (Core stays
		// ECS-agnostic), so the write goes through the NON-template SetOutput with
		// SetPackedEntityID - the _Entity / _Physics precedent.
		static constexpr u_int uPIN_Variable = 0u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_StoreSelfEntityID)
		ZENITH_GRAPH_PIN_OUTPUT(Variable, PROPERTY_TYPE_ENTITY_ID)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			if (!xContext.m_xSelf.IsValid())
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			// SHAPE A: the invalid-self FAILURE is above every accessor, so a failed
			// instance builds no pin state and latches no slot.
			Zenith_PropertyValue xValue;
			xValue.SetPackedEntityID(xContext.m_xSelf.GetEntityID().GetPacked());
			// UNGUARDED writer -> the third and last `""` divergence site in this TU.
			SetOutput(xContext, uPIN_Variable, xValue);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "StoreSelfEntityID"; }
	};

	// Fires a named custom event on an entity's GraphComponent (all slots).
	// Targets self by default, or the entity in m_strTargetVar - the graph-side
	// cross-entity messaging primitive. m_strPayloadVar (optional) sends a
	// blackboard value as the payload.
	class Zenith_GraphNode_FireCustomEvent : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_FireCustomEvent)
	public:
		ZENITH_PROPERTY(std::string, m_strEventName, "event")
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")
		// The payload is READ (TryGetInput below) and passed on verbatim, so its
		// type is whatever the sender put there: ANY. m_strEventName is an event
		// name, not a blackboard name, so it is not a pin.
		static constexpr u_int uPIN_Payload = 1u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_FireCustomEvent)
		ZENITH_GRAPH_PIN_TARGET_ENTITY(Target, "m_strTargetVar")
		ZENITH_GRAPH_PIN_INPUT(Payload, eGRAPH_PIN_TYPE_ANY)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_Entity xTarget = xContext.ResolveTargetEntity(m_strTargetVar);
			if (!xTarget.IsValid())
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_GraphComponent* pxGraph = xTarget.TryGetComponent<Zenith_GraphComponent>();
			if (pxGraph == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			// ★ TryGetInput COLLAPSES THE OLD TERNARY EXACTLY, so this is NOT a
			// second hoist exception (the _UI one is the only one): pxOut is nulled
			// on entry and every return is `pxOut != nullptr`, so an empty var with
			// nothing wired returns false with NO log and a null payload; a bound
			// PRESENT var yields the value. A WIRE supplies a payload even when the
			// sits BELOW both guards (Shape A).
			//
			// ★ POINTER LIFETIME. The payload is passed through UNCHANGED - never
			// copied - and may point at two different places: a producer's output
			// path); an ANY plain INPUT has no const, so the const-scratch case cannot
			// arise here. The dispatch below
			// is SYNCHRONOUS and the pointer is not retained past it. The hazard it
			// does carry - a self-targeted dispatch whose handler re-pulls the same
			// pure producer, or rewrites the payload variable, while the payload is
			// still live - is the SAME hazard class as today's blackboard-pointer
			// path, so it is not a regression. Copying instead would DIVERGE on the
			// unconnected path, where a receiver observes blackboard mutation today.
			const Zenith_PropertyValue* pxPayload = nullptr;
			TryGetInput(xContext, uPIN_Payload, pxPayload);
			pxGraph->FireCustomEvent(m_strEventName.c_str(), pxPayload);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "FireCustomEvent"; }
	};

	// Fires a named custom event on EVERY GraphComponent in loaded scenes.
	class Zenith_GraphNode_BroadcastCustomEvent : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_BroadcastCustomEvent)
	public:
		ZENITH_PROPERTY(std::string, m_strEventName, "event")
		// The Payload pin is index 0 here - there is no Target - so a copy of
		// FireCustomEvent's `uPIN_Payload = 1u` would address nothing.
		static constexpr u_int uPIN_Payload = 0u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_BroadcastCustomEvent)
		ZENITH_GRAPH_PIN_INPUT(Payload, eGRAPH_PIN_TYPE_ANY)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			// Same collapse and the same pointer-lifetime ruling as
			// FireCustomEvent above, with no guards to sit below: the pointer is
			// passed through unchanged into a SYNCHRONOUS dispatch that reaches
			// every GraphComponent in every loaded scene.
			const Zenith_PropertyValue* pxPayload = nullptr;
			TryGetInput(xContext, uPIN_Payload, pxPayload);
			Zenith_GraphComponent::BroadcastCustomEvent(m_strEventName.c_str(), pxPayload);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "BroadcastCustomEvent"; }
	};

	//==========================================================================
	// Flow
	//==========================================================================

	// Suspends the chain for m_fSeconds of dispatched time, then continues.
	// m_strSecondsVar overrides the constant (re-read every tick, so a var
	// shrinking mid-wait shortens the wait).
	class Zenith_GraphNode_Wait : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_Wait)
	public:
		ZENITH_PROPERTY_RANGED(float, m_fSeconds, 1.0f, 0.0f, 3600.0f)
		static constexpr u_int uPIN_Seconds = 0u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_Wait)
		ZENITH_GRAPH_PIN_INPUT_CONST(Seconds, "m_fSeconds", PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			// ★ READ ON EVERY TICK, INCLUDING RUNNING ONES - the first statement,
			// above the accumulation and both returns, exactly where it sat. That
			// is what lets a shrinking variable shorten a wait in flight, and it
			// means a WIRED Wait pulls its producer every tick (a gather token is
			// minted per non-pure Execute, so the memo rule stays consistent). The
			const float fSeconds = GetInput<float>(xContext, uPIN_Seconds);
			m_fElapsed += xContext.m_fDt;
			if (m_fElapsed < fSeconds)
			{
				return GRAPH_NODE_STATUS_RUNNING;
			}
			m_fElapsed = 0.0f;
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		void OnAbort(Zenith_GraphContext&) override { m_fElapsed = 0.0f; }
		const char* GetTypeName() const override { return "Wait"; }

	private:
		float m_fElapsed = 0.0f;
	};

	// Flow node: runs pin 0 when the blackboard bool is true, pin 1 otherwise.
	class Zenith_GraphNode_Branch : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_Branch)
	public:
		// ★ READ-ONLY (GetBool below, no write anywhere in Execute) - the same
		// property NAME as WaitForCondition's, whose node DOES write it back
		// under m_bResetOnPass and is therefore READWRITE. The role follows the
		// Execute body, never the property name.
		//
		// ★ m_strConditionVar DEFAULTS TO "condition", which is NOT empty, so every
		static constexpr u_int uPIN_Condition = 0u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_Branch)
		ZENITH_GRAPH_PIN_INPUT(Condition, PROPERTY_TYPE_BOOL)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			// While a taken branch is suspended, keep re-driving THAT pin. ★ THE
			// READ STAYS INSIDE THIS BRANCH: Condition is read ONCE PER ACTIVATION,
			// so a suspended taken branch neither re-reads the variable nor (in a
			// graph) re-pulls its producer. Hoisting it would change both.
			if (m_iActivePin < 0)
			{
				m_iActivePin = GetInput<bool>(xContext, uPIN_Condition) ? 0 : 1;
			}
			const GraphNodeStatus eStatus = xContext.m_pxGraph->RunChainFromPin(GetNodeID(), static_cast<u_int>(m_iActivePin), xContext);
			if (eStatus != GRAPH_NODE_STATUS_RUNNING)
			{
				m_iActivePin = -1;
			}
			return eStatus;
		}
		void OnAbort(Zenith_GraphContext& xContext) override
		{
			if (m_iActivePin >= 0)
			{
				xContext.m_pxGraph->AbortChain(GetNodeID(), static_cast<u_int>(m_iActivePin), xContext);
				m_iActivePin = -1;
			}
		}
		const char* GetTypeName() const override { return "Branch"; }

	private:
		int32_t m_iActivePin = -1;
	};

	// Gate: SUCCESS (chain continues) while the blackboard bool is true,
	// FAILURE (chain aborts) otherwise.
	class Zenith_GraphNode_Gate : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_Gate)
	public:
		// placed instance.
		static constexpr u_int uPIN_Open = 0u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_Gate)
		ZENITH_GRAPH_PIN_INPUT(Open, PROPERTY_TYPE_BOOL)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			return GetInput<bool>(xContext, uPIN_Open)
				? GRAPH_NODE_STATUS_SUCCESS : GRAPH_NODE_STATUS_FAILURE;
		}
		const char* GetTypeName() const override { return "Gate"; }
	};

	// Lets the chain through exactly once per graph instance lifetime.
	class Zenith_GraphNode_Once : public Zenith_GraphNode
	{
	public:
		GraphNodeStatus Execute(Zenith_GraphContext&) override
		{
			if (m_bFired)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			m_bFired = true;
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "Once"; }

	private:
		bool m_bFired = false;
	};

	// Flow node: runs its body (pin 0) m_iCount times, then its done chain
	// (pin 1). m_strCountVar overrides the constant, read once at loop entry
	// (a fresh run); values < 1 clamp to 1 (the constant's floor).
	// Per-instance state: -1 = fresh, > 0 = iterations left, 0 = in the done
	// chain (a suspended done chain resumes WITHOUT re-running the body).
	class Zenith_GraphNode_Loop : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_Loop)
	public:
		ZENITH_PROPERTY_RANGED(int32_t, m_iCount, 1, 1, 10000)
		static constexpr u_int uPIN_Count = 0u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_Loop)
		ZENITH_GRAPH_PIN_INPUT_CONST(Count, "m_iCount", PROPERTY_TYPE_INT32)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			// ★ THE READ STAYS INSIDE THIS BRANCH: Count is read ONCE at loop entry
			// (a fresh run), never on a resumed RUNNING tick - so a wired Count
			// pulls its producer once per run, not once per iteration. The < 1
			// clamp is unchanged.
			if (m_iRemaining < 0)
			{
				const int32_t iCount = GetInput<int32_t>(xContext, uPIN_Count);
				m_iRemaining = iCount < 1 ? 1 : iCount;
			}
			while (m_iRemaining > 0)
			{
				const GraphNodeStatus eStatus = xContext.m_pxGraph->RunChainFromPin(GetNodeID(), 0, xContext);
				if (eStatus == GRAPH_NODE_STATUS_RUNNING)
				{
					return GRAPH_NODE_STATUS_RUNNING;
				}
				if (eStatus == GRAPH_NODE_STATUS_FAILURE)
				{
					m_iRemaining = -1;
					return GRAPH_NODE_STATUS_FAILURE;
				}
				--m_iRemaining;
			}
			// m_iRemaining == 0 = "in done chain": stays 0 across RUNNING
			// ticks so a resume skips the body; only a finished done chain
			// re-arms the loop.
			const GraphNodeStatus eDone = xContext.m_pxGraph->RunChainFromPin(GetNodeID(), 1, xContext);
			if (eDone != GRAPH_NODE_STATUS_RUNNING)
			{
				m_iRemaining = -1;
			}
			return eDone;
		}
		void OnAbort(Zenith_GraphContext& xContext) override
		{
			xContext.m_pxGraph->AbortChain(GetNodeID(), 0, xContext);
			xContext.m_pxGraph->AbortChain(GetNodeID(), 1, xContext);
			m_iRemaining = -1;
		}
		const char* GetTypeName() const override { return "Loop"; }

	private:
		int32_t m_iRemaining = -1;
	};
}

//------------------------------------------------------------------------------
// The engine registrar (installed by Zenith_Engine::Initialise). Core nodes
// register here; domain node families live in sibling sub-registrar TUs
// (Zenith_GraphNode_Registration_<Domain>.cpp) so each TU names exactly the
// engine systems its nodes wrap.
//------------------------------------------------------------------------------
void Zenith_RegisterEngineGraphNodes_Input();
void Zenith_RegisterEngineGraphNodes_Physics();
void Zenith_RegisterEngineGraphNodes_Animation();
void Zenith_RegisterEngineGraphNodes_UI();
void Zenith_RegisterEngineGraphNodes_Scene();
void Zenith_RegisterEngineGraphNodes_Entity();
void Zenith_RegisterEngineGraphNodes_Math();
void Zenith_RegisterEngineGraphNodes_Flow();
void Zenith_RegisterEngineGraphNodes_AI();

void Zenith_RegisterEngineGraphNodes()
{
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();

	// Event sources
	xRegistry.RegisterNodeType<Zenith_GraphNode_OnStart>("OnStart", GRAPH_EVENT_ON_START, 1, false, "Events");
	xRegistry.RegisterNodeType<Zenith_GraphNode_OnUpdate>("OnUpdate", GRAPH_EVENT_ON_UPDATE, 1, false, "Events");
	xRegistry.RegisterNodeType<Zenith_GraphNode_OnFixedUpdate>("OnFixedUpdate", GRAPH_EVENT_ON_FIXED_UPDATE, 1, false, "Events");
	xRegistry.RegisterNodeType<Zenith_GraphNode_OnEnable>("OnEnable", GRAPH_EVENT_ON_ENABLE, 1, false, "Events");
	xRegistry.RegisterNodeType<Zenith_GraphNode_OnDisable>("OnDisable", GRAPH_EVENT_ON_DISABLE, 1, false, "Events");
	xRegistry.RegisterNodeType<Zenith_GraphNode_OnDestroyEvent>("OnDestroy", GRAPH_EVENT_ON_DESTROY, 1, false, "Events");
	xRegistry.RegisterNodeType<Zenith_GraphNode_OnCollisionEnter>("OnCollisionEnter", GRAPH_EVENT_ON_COLLISION_ENTER, 1, false, "Events");
	xRegistry.RegisterNodeType<Zenith_GraphNode_OnCollisionStay>("OnCollisionStay", GRAPH_EVENT_ON_COLLISION_STAY, 1, false, "Events");
	xRegistry.RegisterNodeType<Zenith_GraphNode_OnCollisionExit>("OnCollisionExit", GRAPH_EVENT_ON_COLLISION_EXIT, 1, false, "Events");
	xRegistry.RegisterNodeType<Zenith_GraphNode_Timer>("Timer", GRAPH_EVENT_TIMER, 1, false, "Events");
	xRegistry.RegisterNodeType<Zenith_GraphNode_OnCustomEvent>("OnCustomEvent", GRAPH_EVENT_CUSTOM, 1, false, "Events");
	xRegistry.RegisterNodeType<Zenith_GraphNode_OnGraphCall>("OnGraphCall", GRAPH_EVENT_ON_GRAPH_CALL, 1, false, "Events");

	// Actions
	xRegistry.RegisterNodeType<Zenith_GraphNode_DebugLog>("DebugLog", GRAPH_EVENT_NONE, 1, false, "Debug");
	xRegistry.RegisterNodeType<Zenith_GraphNode_RotateEntity>("RotateEntity", GRAPH_EVENT_NONE, 1, false, "Transform");
	xRegistry.RegisterNodeType<Zenith_GraphNode_TranslateEntity>("TranslateEntity", GRAPH_EVENT_NONE, 1, false, "Transform");
	xRegistry.RegisterNodeType<Zenith_GraphNode_DestroyEntity>("DestroyEntity", GRAPH_EVENT_NONE, 1, false, "Entity");
	xRegistry.RegisterNodeType<Zenith_GraphNode_SetBlackboardBool>("SetBlackboardBool", GRAPH_EVENT_NONE, 1, false, "Blackboard");
	xRegistry.RegisterNodeType<Zenith_GraphNode_SetBlackboardFloat>("SetBlackboardFloat", GRAPH_EVENT_NONE, 1, false, "Blackboard");
	xRegistry.RegisterNodeType<Zenith_GraphNode_AddBlackboardFloat>("AddBlackboardFloat", GRAPH_EVENT_NONE, 1, false, "Blackboard");
	xRegistry.RegisterNodeType<Zenith_GraphNode_SetBlackboardInt>("SetBlackboardInt", GRAPH_EVENT_NONE, 1, false, "Blackboard");
	xRegistry.RegisterNodeType<Zenith_GraphNode_SetBlackboardVector3>("SetBlackboardVector3", GRAPH_EVENT_NONE, 1, false, "Blackboard");
	xRegistry.RegisterNodeType<Zenith_GraphNode_SetBlackboardEntityID>("SetBlackboardEntityID", GRAPH_EVENT_NONE, 1, false, "Blackboard");
	xRegistry.RegisterNodeType<Zenith_GraphNode_SetBlackboardString>("SetBlackboardString", GRAPH_EVENT_NONE, 1, false, "Blackboard");
	xRegistry.RegisterNodeType<Zenith_GraphNode_CompareBlackboardInt>("CompareBlackboardInt", GRAPH_EVENT_NONE, 1, false, "Blackboard");
	xRegistry.RegisterNodeType<Zenith_GraphNode_StoreSelfEntityID>("StoreSelfEntityID", GRAPH_EVENT_NONE, 1, false, "Entity");
	xRegistry.RegisterNodeType<Zenith_GraphNode_FireCustomEvent>("FireCustomEvent", GRAPH_EVENT_NONE, 1, false, "Events");
	xRegistry.RegisterNodeType<Zenith_GraphNode_BroadcastCustomEvent>("BroadcastCustomEvent", GRAPH_EVENT_NONE, 1, false, "Events");
	xRegistry.RegisterNodeType<Zenith_GraphNode_LoadSceneByIndex>("LoadSceneByIndex", GRAPH_EVENT_NONE, 1, false, "Scene");
	xRegistry.RegisterNodeType<Zenith_GraphNode_CompareBlackboardFloat>("CompareBlackboardFloat", GRAPH_EVENT_NONE, 1, false, "Blackboard");
	// PURE (the last argument): no exec pins, evaluated on demand by whichever
	// consumer gathers its Value wire.
	xRegistry.RegisterNodeType<Zenith_GraphNode_GetVariable>("GetVariable", GRAPH_EVENT_NONE, 0, false, "Blackboard", false, true);

	// Flow
	xRegistry.RegisterNodeType<Zenith_GraphNode_Wait>("Wait", GRAPH_EVENT_NONE, 1, false, "Flow");
	xRegistry.RegisterNodeType<Zenith_GraphNode_Branch>("Branch", GRAPH_EVENT_NONE, 2, true, "Flow");
	xRegistry.RegisterNodeType<Zenith_GraphNode_Gate>("Gate", GRAPH_EVENT_NONE, 1, false, "Flow");
	xRegistry.RegisterNodeType<Zenith_GraphNode_Once>("Once", GRAPH_EVENT_NONE, 1, false, "Flow");
	xRegistry.RegisterNodeType<Zenith_GraphNode_Loop>("Loop", GRAPH_EVENT_NONE, 2, true, "Flow");

	// Domain node families (sibling TUs).
	Zenith_RegisterEngineGraphNodes_Input();
	Zenith_RegisterEngineGraphNodes_Physics();
	Zenith_RegisterEngineGraphNodes_Animation();
	Zenith_RegisterEngineGraphNodes_UI();
	Zenith_RegisterEngineGraphNodes_Scene();
	Zenith_RegisterEngineGraphNodes_Entity();
	Zenith_RegisterEngineGraphNodes_Math();
	Zenith_RegisterEngineGraphNodes_Flow();
	Zenith_RegisterEngineGraphNodes_AI();
}

#include "EntityComponent/Zenith_GraphNode_Registration.Tests.inl"
