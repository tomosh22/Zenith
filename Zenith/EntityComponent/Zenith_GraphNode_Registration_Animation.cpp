#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "Core/Zenith_Tween.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "Scripting/Zenith_GraphBlackboard.h"
#include "EntityComponent/Components/Zenith_AnimatorComponent.h"
#include "EntityComponent/Components/Zenith_TweenComponent.h"
#include "EntityComponent/Components/Zenith_ParticleEmitterComponent.h"
#include "EntityComponent/Zenith_GraphNodeHelpers.h"
// Particle nodes assign named emitter configs via Zenith_ParticleEmitterComponent::
// SetConfigByName (which owns the Flux_ParticleEmitterConfig::Find call in its .cpp,
// the allow-listed EntityComponent->Flux bridge). Including the Flux config header
// directly here would add a fresh EntityComponent->Flux edge the gate rejects.

#include <cmath>

//------------------------------------------------------------------------------
// Engine Behaviour Graph node library - Animation domain (animator state
// machine, tweens, particles).
//
// Animator nodes drive the CONTROLLER-LEVEL state machine through
// Zenith_AnimatorComponent's forwarding surface. Engine semantics under the
// hood are silent no-ops (missing parameter, wrong type, unknown state), so
// these nodes gate on HasStateMachine() and FAIL the chain when there is no
// state machine at all - the designer-visible failure mode. Layered
// controllers (RenderTest-style per-layer SMs) are NOT reached by these
// nodes; layer-targeted writes stay game-side.
//
// Tween nodes add the Zenith_TweenComponent on demand (the engine ScaleTo
// pattern - safe mid-dispatch, Tween pool != Graph pool). One tween per
// property: starting a tween on a property cancels that property's running
// tween (same-property sequences do not queue).
//------------------------------------------------------------------------------

namespace
{
	Zenith_AnimatorComponent* ResolveTargetAnimator(Zenith_GraphContext& xContext, const std::string& strTargetVar)
	{
		Zenith_Entity xTarget = xContext.ResolveTargetEntity(strTargetVar);
		if (!xTarget.IsValid())
		{
			return nullptr;
		}
		Zenith_AnimatorComponent* pxAnimator = xTarget.TryGetComponent<Zenith_AnimatorComponent>();
		if (pxAnimator == nullptr || !pxAnimator->HasStateMachine())
		{
			return nullptr;
		}
		return pxAnimator;
	}

	//==========================================================================
	// Animator parameters + state
	//==========================================================================

	// Float parameter (const or var). Parameters must be DECLARED by the
	// game's animator setup - a missing/mistyped name is a silent no-op in
	// the animation system.
	class Zenith_GraphNode_SetAnimatorFloat : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_SetAnimatorFloat)
	public:
		ZENITH_PROPERTY(std::string, m_strParameter, "Speed")
		ZENITH_PROPERTY(float, m_fValue, 0.0f)
		ZENITH_PROPERTY(std::string, m_strValueVar, "")
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_AnimatorComponent* pxAnimator = ResolveTargetAnimator(xContext, m_strTargetVar);
			if (pxAnimator == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			pxAnimator->SetFloat(m_strParameter, m_strValueVar.empty()
				? m_fValue : xContext.m_pxBlackboard->GetFloat(m_strValueVar, m_fValue));
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "SetAnimatorFloat"; }
	};

	class Zenith_GraphNode_SetAnimatorInt : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_SetAnimatorInt)
	public:
		ZENITH_PROPERTY(std::string, m_strParameter, "State")
		ZENITH_PROPERTY(int32_t, m_iValue, 0)
		ZENITH_PROPERTY(std::string, m_strValueVar, "")
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_AnimatorComponent* pxAnimator = ResolveTargetAnimator(xContext, m_strTargetVar);
			if (pxAnimator == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			pxAnimator->SetInt(m_strParameter, m_strValueVar.empty()
				? m_iValue : xContext.m_pxBlackboard->GetInt32(m_strValueVar, m_iValue));
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "SetAnimatorInt"; }
	};

	class Zenith_GraphNode_SetAnimatorBool : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_SetAnimatorBool)
	public:
		ZENITH_PROPERTY(std::string, m_strParameter, "Grounded")
		ZENITH_PROPERTY(bool, m_bValue, true)
		ZENITH_PROPERTY(std::string, m_strValueVar, "")
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_AnimatorComponent* pxAnimator = ResolveTargetAnimator(xContext, m_strTargetVar);
			if (pxAnimator == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			pxAnimator->SetBool(m_strParameter, m_strValueVar.empty()
				? m_bValue : xContext.m_pxBlackboard->GetBool(m_strValueVar, m_bValue));
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "SetAnimatorBool"; }
	};

	// Latches until a transition consumes it (Unity trigger semantics).
	class Zenith_GraphNode_SetAnimatorTrigger : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_SetAnimatorTrigger)
	public:
		ZENITH_PROPERTY(std::string, m_strParameter, "Attack")
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_AnimatorComponent* pxAnimator = ResolveTargetAnimator(xContext, m_strTargetVar);
			if (pxAnimator == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			pxAnimator->SetTrigger(m_strParameter);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "SetAnimatorTrigger"; }
	};

	// Condition-less interruptible blend to a named STATE over m_fDuration
	// seconds (Unity Animator.CrossFade semantics; unknown state = engine
	// no-op). Already-in-state is a no-op, not an error.
	class Zenith_GraphNode_CrossFadeAnimation : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_CrossFadeAnimation)
	public:
		ZENITH_PROPERTY(std::string, m_strState, "Idle")
		ZENITH_PROPERTY_RANGED(float, m_fDuration, 0.15f, 0.0f, 10.0f)
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_AnimatorComponent* pxAnimator = ResolveTargetAnimator(xContext, m_strTargetVar);
			if (pxAnimator == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			pxAnimator->CrossFade(m_strState, m_fDuration);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "CrossFadeAnimation"; }
	};

	// Current animator state -> blackboard: state name (string), normalized
	// time (fractional part, 0-1), transitioning (bool), hasLooped (bool -
	// "wrapped past the end at least once", not "clip is a looping clip").
	// Empty result-var properties skip that output.
	class Zenith_GraphNode_ReadAnimatorState : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_ReadAnimatorState)
	public:
		ZENITH_PROPERTY(std::string, m_strStateNameVar, "animState")
		ZENITH_PROPERTY(std::string, m_strNormalizedTimeVar, "animTime")
		ZENITH_PROPERTY(std::string, m_strTransitioningVar, "")
		ZENITH_PROPERTY(std::string, m_strHasLoopedVar, "")
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_AnimatorComponent* pxAnimator = ResolveTargetAnimator(xContext, m_strTargetVar);
			if (pxAnimator == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			const Zenith_AnimatorStateInfo xInfo = pxAnimator->GetCurrentAnimatorStateInfo();
			Zenith_PropertyValue xValue;
			if (!m_strStateNameVar.empty())
			{
				xValue.SetString(xInfo.m_strStateName);
				xContext.m_pxBlackboard->SetValue(m_strStateNameVar, xValue);
			}
			if (!m_strNormalizedTimeVar.empty())
			{
				// Unity packing: integer part = loop count; expose progress 0-1.
				xValue.SetFloat(xInfo.m_fNormalizedTime - std::floor(xInfo.m_fNormalizedTime));
				xContext.m_pxBlackboard->SetValue(m_strNormalizedTimeVar, xValue);
			}
			if (!m_strTransitioningVar.empty())
			{
				xValue.SetBool(xInfo.m_bIsTransitioning);
				xContext.m_pxBlackboard->SetValue(m_strTransitioningVar, xValue);
			}
			if (!m_strHasLoopedVar.empty())
			{
				xValue.SetBool(xInfo.m_bHasLooped);
				xContext.m_pxBlackboard->SetValue(m_strHasLoopedVar, xValue);
			}
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "ReadAnimatorState"; }
	};

	//==========================================================================
	// Tweens
	//==========================================================================

	// Shared body for the three tween starters. Adds the component on demand;
	// returns null only when the target entity is unresolvable.
	Zenith_TweenComponent* ResolveOrAddTween(Zenith_GraphContext& xContext, const std::string& strTargetVar)
	{
		Zenith_Entity xTarget = xContext.ResolveTargetEntity(strTargetVar);
		if (!xTarget.IsValid())
		{
			return nullptr;
		}
		Zenith_TweenComponent* pxTween = xTarget.TryGetComponent<Zenith_TweenComponent>();
		if (pxTween == nullptr)
		{
			pxTween = &xTarget.AddComponent<Zenith_TweenComponent>();
		}
		return pxTween;
	}

	bool ValidEasing(int32_t iEasing)
	{
		return iEasing >= 0 && iEasing < static_cast<int32_t>(EASING_COUNT);
	}

	// World-space position tween over m_fDuration seconds. Starting one
	// cancels any running position tween on the target (engine per-property
	// rule). Compose with WaitForTween for move-then-continue chains.
	class Zenith_GraphNode_TweenPosition : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_TweenPosition)
	public:
		ZENITH_PROPERTY(Zenith_Maths::Vector3, m_xTo, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f))
		ZENITH_PROPERTY(std::string, m_strToVar, "")
		ZENITH_PROPERTY_RANGED(float, m_fDuration, 1.0f, 0.0f, 3600.0f)
		ZENITH_PROPERTY(int32_t, m_iEasing, EASING_QUAD_OUT)
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_TweenComponent* pxTween = ResolveOrAddTween(xContext, m_strTargetVar);
			if (pxTween == nullptr || !ValidEasing(m_iEasing))
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			const Zenith_Maths::Vector3 xTo = m_strToVar.empty()
				? m_xTo : xContext.m_pxBlackboard->GetVector3(m_strToVar, m_xTo);
			pxTween->TweenPosition(xTo, m_fDuration, static_cast<Zenith_EasingType>(m_iEasing));
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "TweenPosition"; }
	};

	class Zenith_GraphNode_TweenScale : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_TweenScale)
	public:
		ZENITH_PROPERTY(Zenith_Maths::Vector3, m_xTo, Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f))
		ZENITH_PROPERTY(std::string, m_strToVar, "")
		ZENITH_PROPERTY_RANGED(float, m_fDuration, 1.0f, 0.0f, 3600.0f)
		ZENITH_PROPERTY(int32_t, m_iEasing, EASING_QUAD_OUT)
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_TweenComponent* pxTween = ResolveOrAddTween(xContext, m_strTargetVar);
			if (pxTween == nullptr || !ValidEasing(m_iEasing))
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			const Zenith_Maths::Vector3 xTo = m_strToVar.empty()
				? m_xTo : xContext.m_pxBlackboard->GetVector3(m_strToVar, m_xTo);
			pxTween->TweenScale(xTo, m_fDuration, static_cast<Zenith_EasingType>(m_iEasing));
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "TweenScale"; }
	};

	// Rotation tween: target is Euler DEGREES, slerped shortest-path from the
	// current rotation.
	class Zenith_GraphNode_TweenRotation : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_TweenRotation)
	public:
		ZENITH_PROPERTY(Zenith_Maths::Vector3, m_xToEulerDegrees, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f))
		ZENITH_PROPERTY(std::string, m_strToVar, "")
		ZENITH_PROPERTY_RANGED(float, m_fDuration, 1.0f, 0.0f, 3600.0f)
		ZENITH_PROPERTY(int32_t, m_iEasing, EASING_QUAD_OUT)
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_TweenComponent* pxTween = ResolveOrAddTween(xContext, m_strTargetVar);
			if (pxTween == nullptr || !ValidEasing(m_iEasing))
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			const Zenith_Maths::Vector3 xTo = m_strToVar.empty()
				? m_xToEulerDegrees : xContext.m_pxBlackboard->GetVector3(m_strToVar, m_xToEulerDegrees);
			pxTween->TweenRotation(xTo, m_fDuration, static_cast<Zenith_EasingType>(m_iEasing));
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "TweenRotation"; }
	};

	// RUNNING while the target has active tweens; SUCCESS once none remain
	// (or the component/entity has none to begin with). Stateless - OnAbort
	// needs no reset. Note Tween (order 12) ticks before Graph (order 60), so
	// a tween started this frame first advances next frame.
	class Zenith_GraphNode_WaitForTween : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_WaitForTween)
	public:
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_Entity xTarget = xContext.ResolveTargetEntity(m_strTargetVar);
			if (!xTarget.IsValid())
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_TweenComponent* pxTween = xTarget.TryGetComponent<Zenith_TweenComponent>();
			if (pxTween != nullptr && pxTween->HasActiveTweens())
			{
				return GRAPH_NODE_STATUS_RUNNING;
			}
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "WaitForTween"; }
	};

	// Cancels tweens on the target. Property: -1 = all, 0 = position,
	// 1 = rotation, 2 = scale. No completion callbacks fire; the transform
	// stays wherever the last tick put it. Nothing to stop = SUCCESS.
	class Zenith_GraphNode_StopTweens : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_StopTweens)
	public:
		ZENITH_PROPERTY(int32_t, m_iProperty, -1)
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_Entity xTarget = xContext.ResolveTargetEntity(m_strTargetVar);
			if (!xTarget.IsValid())
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_TweenComponent* pxTween = xTarget.TryGetComponent<Zenith_TweenComponent>();
			if (pxTween != nullptr)
			{
				if (m_iProperty >= 0 && m_iProperty <= 2)
				{
					pxTween->CancelByProperty(static_cast<Zenith_TweenProperty>(m_iProperty));
				}
				else
				{
					pxTween->CancelAll();
				}
			}
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "StopTweens"; }
	};

	//==========================================================================
	// Particles
	//==========================================================================

	Zenith_ParticleEmitterComponent* ResolveTargetEmitter(Zenith_GraphContext& xContext, const std::string& strTargetVar)
	{
		Zenith_Entity xTarget = xContext.ResolveTargetEntity(strTargetVar);
		if (!xTarget.IsValid())
		{
			return nullptr;
		}
		return xTarget.TryGetComponent<Zenith_ParticleEmitterComponent>();
	}

	// Synchronous burst of N particles. m_strConfigName assigns a registered
	// Flux_ParticleEmitterConfig when the emitter has none yet ("" = require
	// one already set). No config resolvable = FAILURE (the engine Emit would
	// silently no-op).
	class Zenith_GraphNode_EmitParticles : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_EmitParticles)
	public:
		ZENITH_PROPERTY_RANGED(int32_t, m_iCount, 10, 1, 100000)
		ZENITH_PROPERTY(std::string, m_strConfigName, "")
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_ParticleEmitterComponent* pxEmitter = ResolveTargetEmitter(xContext, m_strTargetVar);
			if (pxEmitter == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			if (pxEmitter->GetConfig() == nullptr && !m_strConfigName.empty())
			{
				pxEmitter->SetConfigByName(m_strConfigName);
			}
			if (pxEmitter->GetConfig() == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			pxEmitter->Emit(static_cast<uint32_t>(m_iCount));
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "EmitParticles"; }
	};

	// Continuous-emission toggle (needs a config with spawn rate > 0 to
	// actually produce particles; the flag itself always sets).
	class Zenith_GraphNode_SetParticleEmitting : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_SetParticleEmitting)
	public:
		ZENITH_PROPERTY(bool, m_bEmitting, true)
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_ParticleEmitterComponent* pxEmitter = ResolveTargetEmitter(xContext, m_strTargetVar);
			if (pxEmitter == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			pxEmitter->SetEmitting(m_bEmitting);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "SetParticleEmitting"; }
	};

	// World-space emit override. m_bClear restores transform-following.
	// Position comes from a position ref ("" = self) + offset; the direction
	// is set TOGETHER with the position when m_bSetDirection is on (the
	// engine's override flag couples them - setting direction alone would
	// teleport emission to the origin).
	class Zenith_GraphNode_SetParticleEmitPosition : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_SetParticleEmitPosition)
	public:
		ZENITH_PROPERTY(std::string, m_strPositionVar, "")
		ZENITH_PROPERTY(Zenith_Maths::Vector3, m_xOffset, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f))
		ZENITH_PROPERTY(bool, m_bSetDirection, false)
		ZENITH_PROPERTY(Zenith_Maths::Vector3, m_xDirection, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f))
		ZENITH_PROPERTY(bool, m_bClear, false)
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_ParticleEmitterComponent* pxEmitter = ResolveTargetEmitter(xContext, m_strTargetVar);
			if (pxEmitter == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			if (m_bClear)
			{
				pxEmitter->ClearPositionOverride();
				return GRAPH_NODE_STATUS_SUCCESS;
			}
			Zenith_Maths::Vector3 xPosition;
			if (!Zenith_GraphNode_ResolvePositionRef(xContext, m_strPositionVar, xPosition))
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			pxEmitter->SetEmitPosition(xPosition + m_xOffset);
			if (m_bSetDirection && glm::dot(m_xDirection, m_xDirection) > 0.0001f)
			{
				pxEmitter->SetEmitDirection(m_xDirection);
			}
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "SetParticleEmitPosition"; }
	};
}

void Zenith_RegisterEngineGraphNodes_Animation()
{
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();

	// Animator
	xRegistry.RegisterNodeType<Zenith_GraphNode_SetAnimatorFloat>("SetAnimatorFloat", GRAPH_EVENT_NONE, 1, false, "Animation");
	xRegistry.RegisterNodeType<Zenith_GraphNode_SetAnimatorInt>("SetAnimatorInt", GRAPH_EVENT_NONE, 1, false, "Animation");
	xRegistry.RegisterNodeType<Zenith_GraphNode_SetAnimatorBool>("SetAnimatorBool", GRAPH_EVENT_NONE, 1, false, "Animation");
	xRegistry.RegisterNodeType<Zenith_GraphNode_SetAnimatorTrigger>("SetAnimatorTrigger", GRAPH_EVENT_NONE, 1, false, "Animation");
	xRegistry.RegisterNodeType<Zenith_GraphNode_CrossFadeAnimation>("CrossFadeAnimation", GRAPH_EVENT_NONE, 1, false, "Animation");
	xRegistry.RegisterNodeType<Zenith_GraphNode_ReadAnimatorState>("ReadAnimatorState", GRAPH_EVENT_NONE, 1, false, "Animation");

	// Tweens
	xRegistry.RegisterNodeType<Zenith_GraphNode_TweenPosition>("TweenPosition", GRAPH_EVENT_NONE, 1, false, "Tween");
	xRegistry.RegisterNodeType<Zenith_GraphNode_TweenScale>("TweenScale", GRAPH_EVENT_NONE, 1, false, "Tween");
	xRegistry.RegisterNodeType<Zenith_GraphNode_TweenRotation>("TweenRotation", GRAPH_EVENT_NONE, 1, false, "Tween");
	xRegistry.RegisterNodeType<Zenith_GraphNode_WaitForTween>("WaitForTween", GRAPH_EVENT_NONE, 1, false, "Tween");
	xRegistry.RegisterNodeType<Zenith_GraphNode_StopTweens>("StopTweens", GRAPH_EVENT_NONE, 1, false, "Tween");

	// Particles
	xRegistry.RegisterNodeType<Zenith_GraphNode_EmitParticles>("EmitParticles", GRAPH_EVENT_NONE, 1, false, "Particles");
	xRegistry.RegisterNodeType<Zenith_GraphNode_SetParticleEmitting>("SetParticleEmitting", GRAPH_EVENT_NONE, 1, false, "Particles");
	xRegistry.RegisterNodeType<Zenith_GraphNode_SetParticleEmitPosition>("SetParticleEmitPosition", GRAPH_EVENT_NONE, 1, false, "Particles");
}
