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
		ZENITH_PROPERTY(std::string, m_strVar, "value")
		ZENITH_PROPERTY(float, m_fCompareTo, 0.0f)
		ZENITH_PROPERTY(std::string, m_strCompareVar, "")
		ZENITH_PROPERTY(int32_t, m_iOp, 0)
		ZENITH_PROPERTY(std::string, m_strResultVar, "result")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			const float fValue = xContext.m_pxBlackboard->GetFloat(m_strVar);
			const float fCompareTo = m_strCompareVar.empty()
				? m_fCompareTo : xContext.m_pxBlackboard->GetFloat(m_strCompareVar, m_fCompareTo);
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
			xContext.m_pxBlackboard->SetValue(m_strResultVar, xResult);
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
		ZENITH_PROPERTY(std::string, m_strUnitsVar, "")
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
			const Zenith_Maths::Vector3 xUnitsPerSecond = m_strUnitsVar.empty()
				? m_xUnitsPerSecond : xContext.m_pxBlackboard->GetVector3(m_strUnitsVar, m_xUnitsPerSecond);
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

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_PropertyValue xValue;
			xValue.SetBool(m_bValue);
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

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_PropertyValue xValue;
			xValue.SetFloat(m_fValue);
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
		ZENITH_PROPERTY(std::string, m_strDeltaVar, "")
		ZENITH_PROPERTY(bool, m_bScaleByDt, false)

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			float fDelta = m_strDeltaVar.empty()
				? m_fDelta : xContext.m_pxBlackboard->GetFloat(m_strDeltaVar, m_fDelta);
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

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_PropertyValue xValue;
			xValue.SetInt32(m_iValue);
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

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_PropertyValue xValue;
			xValue.SetVector3(m_xValue);
			xContext.m_pxBlackboard->SetValue(m_strVariable, xValue);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "SetBlackboardVector3"; }
	};

	class Zenith_GraphNode_SetBlackboardString : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_SetBlackboardString)
	public:
		ZENITH_PROPERTY(std::string, m_strVariable, "text")
		ZENITH_PROPERTY(std::string, m_strValue, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_PropertyValue xValue;
			xValue.SetString(m_strValue);
			xContext.m_pxBlackboard->SetValue(m_strVariable, xValue);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "SetBlackboardString"; }
	};

	// Integer sibling of CompareBlackboardFloat. Compares a blackboard int32
	// against a constant, or against another variable when m_strCompareVar is
	// set. m_iOp is a Zenith_GraphCompareIntOp (adds NOT_EQUAL over the float node).
	class Zenith_GraphNode_CompareBlackboardInt : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_CompareBlackboardInt)
	public:
		ZENITH_PROPERTY(std::string, m_strVar, "value")
		ZENITH_PROPERTY(int32_t, m_iCompareTo, 0)
		ZENITH_PROPERTY(std::string, m_strCompareVar, "")
		ZENITH_PROPERTY(int32_t, m_iOp, 4)
		ZENITH_PROPERTY(std::string, m_strResultVar, "result")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			const int32_t iValue = xContext.m_pxBlackboard->GetInt32(m_strVar);
			const int32_t iCompareTo = m_strCompareVar.empty()
				? m_iCompareTo : xContext.m_pxBlackboard->GetInt32(m_strCompareVar, m_iCompareTo);
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
			xContext.m_pxBlackboard->SetValue(m_strResultVar, xResult);
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
		ZENITH_PROPERTY(std::string, m_strVariable, "self")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			if (!xContext.m_xSelf.IsValid())
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_PropertyValue xValue;
			xValue.SetPackedEntityID(xContext.m_xSelf.GetEntityID().GetPacked());
			xContext.m_pxBlackboard->SetValue(m_strVariable, xValue);
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
		ZENITH_PROPERTY(std::string, m_strPayloadVar, "")

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
			const Zenith_PropertyValue* pxPayload = m_strPayloadVar.empty()
				? nullptr : xContext.m_pxBlackboard->TryGetValue(m_strPayloadVar);
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
		ZENITH_PROPERTY(std::string, m_strPayloadVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			const Zenith_PropertyValue* pxPayload = m_strPayloadVar.empty()
				? nullptr : xContext.m_pxBlackboard->TryGetValue(m_strPayloadVar);
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
		ZENITH_PROPERTY(std::string, m_strSecondsVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			const float fSeconds = m_strSecondsVar.empty()
				? m_fSeconds : xContext.m_pxBlackboard->GetFloat(m_strSecondsVar, m_fSeconds);
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
		ZENITH_PROPERTY(std::string, m_strConditionVar, "condition")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			// While a taken branch is suspended, keep re-driving THAT pin.
			if (m_iActivePin < 0)
			{
				m_iActivePin = xContext.m_pxBlackboard->GetBool(m_strConditionVar, false) ? 0 : 1;
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
		ZENITH_PROPERTY(std::string, m_strOpenVar, "open")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			return xContext.m_pxBlackboard->GetBool(m_strOpenVar, false)
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
		ZENITH_PROPERTY(std::string, m_strCountVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			if (m_iRemaining < 0)
			{
				const int32_t iCount = m_strCountVar.empty()
					? m_iCount : xContext.m_pxBlackboard->GetInt32(m_strCountVar, m_iCount);
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
	xRegistry.RegisterNodeType<Zenith_GraphNode_SetBlackboardString>("SetBlackboardString", GRAPH_EVENT_NONE, 1, false, "Blackboard");
	xRegistry.RegisterNodeType<Zenith_GraphNode_CompareBlackboardInt>("CompareBlackboardInt", GRAPH_EVENT_NONE, 1, false, "Blackboard");
	xRegistry.RegisterNodeType<Zenith_GraphNode_StoreSelfEntityID>("StoreSelfEntityID", GRAPH_EVENT_NONE, 1, false, "Entity");
	xRegistry.RegisterNodeType<Zenith_GraphNode_FireCustomEvent>("FireCustomEvent", GRAPH_EVENT_NONE, 1, false, "Events");
	xRegistry.RegisterNodeType<Zenith_GraphNode_BroadcastCustomEvent>("BroadcastCustomEvent", GRAPH_EVENT_NONE, 1, false, "Events");
	xRegistry.RegisterNodeType<Zenith_GraphNode_LoadSceneByIndex>("LoadSceneByIndex", GRAPH_EVENT_NONE, 1, false, "Scene");
	xRegistry.RegisterNodeType<Zenith_GraphNode_CompareBlackboardFloat>("CompareBlackboardFloat", GRAPH_EVENT_NONE, 1, false, "Blackboard");

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
