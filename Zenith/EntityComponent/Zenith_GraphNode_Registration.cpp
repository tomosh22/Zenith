#include "Zenith.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_GraphComponent.h"
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
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "OnCustomEvent"; }
		bool MatchesCustomEvent(const char* szName) const override { return m_strEventName == szName; }
	};

	// Compares a blackboard float against a constant (or another variable) and
	// writes the boolean result to a blackboard variable - Branch consumes it.
	// Op: 0 = less, 1 = lessEqual, 2 = greater, 3 = greaterEqual, 4 = equal.
	class Zenith_GraphNode_CompareBlackboardFloat : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_CompareBlackboardFloat)
	public:
		ZENITH_PROPERTY(std::string, m_strVar, "value")
		ZENITH_PROPERTY(float, m_fCompareTo, 0.0f)
		ZENITH_PROPERTY(int32_t, m_iOp, 0)
		ZENITH_PROPERTY(std::string, m_strResultVar, "result")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			const float fValue = xContext.m_pxBlackboard->GetFloat(m_strVar);
			bool bResult = false;
			switch (m_iOp)
			{
			case 0: bResult = fValue <  m_fCompareTo; break;
			case 1: bResult = fValue <= m_fCompareTo; break;
			case 2: bResult = fValue >  m_fCompareTo; break;
			case 3: bResult = fValue >= m_fCompareTo; break;
			case 4: bResult = fValue == m_fCompareTo; break;
			default: return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_PropertyValue xResult;
			xResult.SetBool(bResult);
			xContext.m_pxBlackboard->SetValue(m_strResultVar, xResult);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "CompareBlackboardFloat"; }
	};

	// Loads a registered scene by build index (SINGLE: replaces the current
	// scene set). The same call the front-end menu's Play handler makes; the
	// scene system's mid-update deferral rules apply identically. Authoring
	// note: place this at the END of a chain - the dispatching entity does not
	// survive the load.
	class Zenith_GraphNode_LoadSceneByIndex : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_LoadSceneByIndex)
	public:
		ZENITH_PROPERTY(int32_t, m_iSceneIndex, 0)

		GraphNodeStatus Execute(Zenith_GraphContext&) override
		{
			g_xEngine.Scenes().LoadSceneByIndex(m_iSceneIndex, SCENE_LOAD_SINGLE);
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
	// entities).
	class Zenith_GraphNode_RotateEntity : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_RotateEntity)
	public:
		ZENITH_PROPERTY_RANGED(float, m_fDegreesPerSecond, 90.0f, -1080.0f, 1080.0f)

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			if (!xContext.m_xSelf.IsValid() || !xContext.m_xSelf.HasComponent<Zenith_TransformComponent>())
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_TransformComponent& xTransform = xContext.m_xSelf.GetComponent<Zenith_TransformComponent>();
			Zenith_Maths::Quat xRotation;
			xTransform.GetRotation(xRotation);
			const float fRadians = glm::radians(m_fDegreesPerSecond) * xContext.m_fDt;
			const Zenith_Maths::Quat xDelta = glm::angleAxis(fRadians, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
			xTransform.SetRotation(glm::normalize(xDelta * xRotation));
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "RotateEntity"; }
	};

	// Transform-driven linear motion (units/second, world space).
	class Zenith_GraphNode_TranslateEntity : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_TranslateEntity)
	public:
		ZENITH_PROPERTY(Zenith_Maths::Vector3, m_xUnitsPerSecond, Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f))

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			if (!xContext.m_xSelf.IsValid() || !xContext.m_xSelf.HasComponent<Zenith_TransformComponent>())
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_TransformComponent& xTransform = xContext.m_xSelf.GetComponent<Zenith_TransformComponent>();
			Zenith_Maths::Vector3 xPosition;
			xTransform.GetPosition(xPosition);
			xTransform.SetPosition(xPosition + m_xUnitsPerSecond * xContext.m_fDt);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "TranslateEntity"; }
	};

	class Zenith_GraphNode_DestroyEntity : public Zenith_GraphNode
	{
	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			if (!xContext.m_xSelf.IsValid())
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			xContext.m_xSelf.Destroy();	// deferred to end of frame - safe mid-dispatch
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

	class Zenith_GraphNode_AddBlackboardFloat : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_AddBlackboardFloat)
	public:
		ZENITH_PROPERTY(std::string, m_strVariable, "value")
		ZENITH_PROPERTY(float, m_fDelta, 1.0f)

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_PropertyValue xValue;
			xValue.SetFloat(xContext.m_pxBlackboard->GetFloat(m_strVariable, 0.0f) + m_fDelta);
			xContext.m_pxBlackboard->SetValue(m_strVariable, xValue);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "AddBlackboardFloat"; }
	};

	// Fires a named custom event on THIS entity's GraphComponent (all slots).
	class Zenith_GraphNode_FireCustomEvent : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_FireCustomEvent)
	public:
		ZENITH_PROPERTY(std::string, m_strEventName, "event")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			if (!xContext.m_xSelf.IsValid() || !xContext.m_xSelf.HasComponent<Zenith_GraphComponent>())
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			xContext.m_xSelf.GetComponent<Zenith_GraphComponent>().FireCustomEvent(m_strEventName.c_str());
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "FireCustomEvent"; }
	};

	//==========================================================================
	// Flow
	//==========================================================================

	// Suspends the chain for m_fSeconds of dispatched time, then continues.
	class Zenith_GraphNode_Wait : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_Wait)
	public:
		ZENITH_PROPERTY_RANGED(float, m_fSeconds, 1.0f, 0.0f, 3600.0f)

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			m_fElapsed += xContext.m_fDt;
			if (m_fElapsed < m_fSeconds)
			{
				return GRAPH_NODE_STATUS_RUNNING;
			}
			m_fElapsed = 0.0f;
			return GRAPH_NODE_STATUS_SUCCESS;
		}
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

	// Flow node: runs its body (pin 0) m_iCount times, then its done chain (pin 1).
	class Zenith_GraphNode_Loop : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_Loop)
	public:
		ZENITH_PROPERTY_RANGED(int32_t, m_iCount, 1, 1, 10000)

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			if (m_iRemaining < 0)
			{
				m_iRemaining = m_iCount;
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
			m_iRemaining = -1;
			return xContext.m_pxGraph->RunChainFromPin(GetNodeID(), 1, xContext);
		}
		const char* GetTypeName() const override { return "Loop"; }

	private:
		int32_t m_iRemaining = -1;
	};
}

//------------------------------------------------------------------------------
// The engine registrar (installed by Zenith_Engine::Initialise).
//------------------------------------------------------------------------------
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

	// Actions
	xRegistry.RegisterNodeType<Zenith_GraphNode_DebugLog>("DebugLog", GRAPH_EVENT_NONE, 1, false, "Debug");
	xRegistry.RegisterNodeType<Zenith_GraphNode_RotateEntity>("RotateEntity", GRAPH_EVENT_NONE, 1, false, "Transform");
	xRegistry.RegisterNodeType<Zenith_GraphNode_TranslateEntity>("TranslateEntity", GRAPH_EVENT_NONE, 1, false, "Transform");
	xRegistry.RegisterNodeType<Zenith_GraphNode_DestroyEntity>("DestroyEntity", GRAPH_EVENT_NONE, 1, false, "Entity");
	xRegistry.RegisterNodeType<Zenith_GraphNode_SetBlackboardBool>("SetBlackboardBool", GRAPH_EVENT_NONE, 1, false, "Blackboard");
	xRegistry.RegisterNodeType<Zenith_GraphNode_SetBlackboardFloat>("SetBlackboardFloat", GRAPH_EVENT_NONE, 1, false, "Blackboard");
	xRegistry.RegisterNodeType<Zenith_GraphNode_AddBlackboardFloat>("AddBlackboardFloat", GRAPH_EVENT_NONE, 1, false, "Blackboard");
	xRegistry.RegisterNodeType<Zenith_GraphNode_FireCustomEvent>("FireCustomEvent", GRAPH_EVENT_NONE, 1, false, "Events");
	xRegistry.RegisterNodeType<Zenith_GraphNode_LoadSceneByIndex>("LoadSceneByIndex", GRAPH_EVENT_NONE, 1, false, "Scene");
	xRegistry.RegisterNodeType<Zenith_GraphNode_CompareBlackboardFloat>("CompareBlackboardFloat", GRAPH_EVENT_NONE, 1, false, "Blackboard");

	// Flow
	xRegistry.RegisterNodeType<Zenith_GraphNode_Wait>("Wait", GRAPH_EVENT_NONE, 1, false, "Flow");
	xRegistry.RegisterNodeType<Zenith_GraphNode_Branch>("Branch", GRAPH_EVENT_NONE, 2, true, "Flow");
	xRegistry.RegisterNodeType<Zenith_GraphNode_Gate>("Gate", GRAPH_EVENT_NONE, 1, false, "Flow");
	xRegistry.RegisterNodeType<Zenith_GraphNode_Once>("Once", GRAPH_EVENT_NONE, 1, false, "Flow");
	xRegistry.RegisterNodeType<Zenith_GraphNode_Loop>("Loop", GRAPH_EVENT_NONE, 2, true, "Flow");
}
