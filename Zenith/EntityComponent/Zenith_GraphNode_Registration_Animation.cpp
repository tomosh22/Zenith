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
//
// ★ THE PINS IN THIS TU ARE LIVE (B-6.4). Every INPUT descriptor is read
// through Zenith_GraphNode::GetInput and every OUTPUT descriptor is written
// through SetOutput, so a wire into or out of any of these 14 nodes carries a
// value. An UNCONNECTED node behaves byte-for-byte as it did: the var-name
// fallback IS the old `var.empty() ? const : bb->GetX(var, const)` read, and
// SetOutput's dual-write IS the old SetValue. Each node that addresses a pin
// declares `static constexpr u_int uPIN_<Name>` immediately before its pin
// table (the INDEX is the runtime address; table order is the contract,
// asserted by GraphPinTable.AnimationPinIndicesMatchTables).
//
// Five things specific to THIS TU:
//   - SEVEN of the fourteen classes declare NO uPIN_ constant at all:
//     SetAnimatorTrigger, CrossFadeAnimation, WaitForTween, StopTweens,
//     EmitParticles, SetParticleEmitting and SetParticleEmitPosition address no
//     pin from their Execute (their only pins are references, resolved
//     directly). The index test still asserts the name, role and pin COUNT of
//     all fourteen, which is what would notice a value pin being ADDED to one
//     of them without its Execute being migrated.
//   - EVERY input read sits AFTER the node's resolver guard - and, for the
//     three tween starters, after the ValidEasing guard as well - exactly where
//     its blackboard read sat. A resolver FAILURE therefore reads no pin at all
//     (fallback count 0 for a var-bound pin).
//   - THIS TU TAKES NO `""` DIVERGENCE. All four ReadAnimatorState writes were
//     already guarded on a non-empty name, so deleting those guards is PARITY
//     on the blackboard (SetOutput's dual-write applies the same non-empty
//     rule); what is new is that the four SLOTS are now latched on every
//     SUCCESS, which is the only reason a wire can come off a Transitioning or
//     a HasLooped whose author never named a variable.
//   - ReadAnimatorState's FAILURE is ABOVE every read and every write, and its
//     only accessor is SetOutput - so a failed execution builds NO pin state,
//     latches no slot and writes no variable. A DIRECTLY-CONSTRUCTED instance
//     that has only ever failed answers GetOutputForTest == nullptr (no pin
//     state was ever built); a GRAPH instance has its four slots stamped at
//     instantiation, so a failed run leaves them at their zeros. It
//     has no failure exec pin, so a consumer wire off any of its four outputs
//     must be gated on SUCCESS.
//   - ReadAnimatorState.StateName is the library's first STRING output, written
//     through the template SetOutput<std::string> (Zenith_PropertyTraits has a
//     std::string specialisation, unlike the ENTITY_ID outputs in _Entity /
//     _Physics).
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

		// m_strParameter names an ANIMATOR parameter declared by the game's
		// animator setup - not a blackboard variable, so it is not a pin. Target
		// reaches xContext.ResolveTargetEntity through ResolveTargetAnimator
		// (this TU's resolver, top of file), so it is an ENTITY reference rather
		// than a value input: it declares no uPIN_ constant and stays direct.
		// uPIN_Value addresses the Value pin (index 0).
		static constexpr u_int uPIN_Value = 0u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_SetAnimatorFloat)
		ZENITH_GRAPH_PIN_INPUT_VAR_OR_CONST(Value, "m_strValueVar", "m_fValue", PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PIN_TARGET_ENTITY(Target, "m_strTargetVar")
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_AnimatorComponent* pxAnimator = ResolveTargetAnimator(xContext, m_strTargetVar);
			if (pxAnimator == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			// After the animator guard, exactly where the blackboard read sat.
			const float fValue = GetInput<float>(xContext, uPIN_Value);
			pxAnimator->SetFloat(m_strParameter, fValue);
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

		static constexpr u_int uPIN_Value = 0u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_SetAnimatorInt)
		ZENITH_GRAPH_PIN_INPUT_VAR_OR_CONST(Value, "m_strValueVar", "m_iValue", PROPERTY_TYPE_INT32)
		ZENITH_GRAPH_PIN_TARGET_ENTITY(Target, "m_strTargetVar")
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_AnimatorComponent* pxAnimator = ResolveTargetAnimator(xContext, m_strTargetVar);
			if (pxAnimator == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			const int32_t iValue = GetInput<int32_t>(xContext, uPIN_Value);
			pxAnimator->SetInt(m_strParameter, iValue);
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

		static constexpr u_int uPIN_Value = 0u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_SetAnimatorBool)
		ZENITH_GRAPH_PIN_INPUT_VAR_OR_CONST(Value, "m_strValueVar", "m_bValue", PROPERTY_TYPE_BOOL)
		ZENITH_GRAPH_PIN_TARGET_ENTITY(Target, "m_strTargetVar")
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_AnimatorComponent* pxAnimator = ResolveTargetAnimator(xContext, m_strTargetVar);
			if (pxAnimator == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			const bool bValue = GetInput<bool>(xContext, uPIN_Value);
			pxAnimator->SetBool(m_strParameter, bValue);
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

		// m_strParameter is an animator TRIGGER name, not a blackboard variable.
		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_SetAnimatorTrigger)
		ZENITH_GRAPH_PIN_TARGET_ENTITY(Target, "m_strTargetVar")
		ZENITH_GRAPH_PINS_END

	public:
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

		// m_strState names an animator STATE and m_fDuration is a const with no
		// var partner - neither is a pin.
		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_CrossFadeAnimation)
		ZENITH_GRAPH_PIN_TARGET_ENTITY(Target, "m_strTargetVar")
		ZENITH_GRAPH_PINS_END

	public:
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

	// Current animator state -> four OUTPUT pins: state name (string),
	// normalized time (fractional part, 0-1), transitioning (bool), hasLooped
	// (bool - "wrapped past the end at least once", not "clip is a looping
	// clip"). Every slot is latched on SUCCESS; only the BLACKBOARD write is
	// skipped for an empty name. A FAILURE (no animator, no state machine)
	// returns above all four, so it writes nothing and latches nothing.
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

		// All four are the node's own READ RESULTS (SetOutput in the Execute
		// below), each typed by the value the Execute actually writes. An empty
		// name no longer skips anything the pin can see: the SLOT is always
		// latched on SUCCESS and only the dual-write to the blackboard obeys the
		// non-empty rule - which is exactly what the four deleted
		// `!m_strXVar.empty()` guards used to express.
		static constexpr u_int uPIN_StateName = 0u;
		static constexpr u_int uPIN_NormalizedTime = 1u;
		static constexpr u_int uPIN_Transitioning = 2u;
		static constexpr u_int uPIN_HasLooped = 3u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_ReadAnimatorState)
		ZENITH_GRAPH_PIN_OUTPUT(StateName, "m_strStateNameVar", PROPERTY_TYPE_STRING)
		ZENITH_GRAPH_PIN_OUTPUT(NormalizedTime, "m_strNormalizedTimeVar", PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PIN_OUTPUT(Transitioning, "m_strTransitioningVar", PROPERTY_TYPE_BOOL)
		ZENITH_GRAPH_PIN_OUTPUT(HasLooped, "m_strHasLoopedVar", PROPERTY_TYPE_BOOL)
		ZENITH_GRAPH_PIN_TARGET_ENTITY(Target, "m_strTargetVar")
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_AnimatorComponent* pxAnimator = ResolveTargetAnimator(xContext, m_strTargetVar);
			if (pxAnimator == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			// ALL FOUR, UNCONDITIONALLY. The four `!m_strXVar.empty()` guards are
			// gone and so is the Zenith_PropertyValue scratch they shared; the
			// normalized-time computation came out of its guard with its write.
			// SetOutput's own dual-write rule reproduces exactly which of them
			// reach the blackboard (animState / animTime by default, the other
			// two only when the author names them).
			const Zenith_AnimatorStateInfo xInfo = pxAnimator->GetCurrentAnimatorStateInfo();
			SetOutput<std::string>(xContext, uPIN_StateName, xInfo.m_strStateName);
			// Unity packing: integer part = loop count; expose progress 0-1.
			SetOutput<float>(xContext, uPIN_NormalizedTime,
				xInfo.m_fNormalizedTime - std::floor(xInfo.m_fNormalizedTime));
			SetOutput<bool>(xContext, uPIN_Transitioning, xInfo.m_bIsTransitioning);
			SetOutput<bool>(xContext, uPIN_HasLooped, xInfo.m_bHasLooped);
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

		// Target reaches xContext.ResolveTargetEntity through ResolveOrAddTween.
		// m_fDuration and m_iEasing are consts with no var partner - not pins.
		static constexpr u_int uPIN_To = 0u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_TweenPosition)
		ZENITH_GRAPH_PIN_INPUT_VAR_OR_CONST(To, "m_strToVar", "m_xTo", PROPERTY_TYPE_VECTOR3)
		ZENITH_GRAPH_PIN_TARGET_ENTITY(Target, "m_strTargetVar")
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_TweenComponent* pxTween = ResolveOrAddTween(xContext, m_strTargetVar);
			if (pxTween == nullptr || !ValidEasing(m_iEasing))
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			// After BOTH guards, exactly where the blackboard read sat. Note the
			// pre-existing order: ResolveOrAddTween has already ADDED the tween
			// component by the time an invalid easing fails the node.
			const Zenith_Maths::Vector3 xTo = GetInput<Zenith_Maths::Vector3>(xContext, uPIN_To);
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

		static constexpr u_int uPIN_To = 0u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_TweenScale)
		ZENITH_GRAPH_PIN_INPUT_VAR_OR_CONST(To, "m_strToVar", "m_xTo", PROPERTY_TYPE_VECTOR3)
		ZENITH_GRAPH_PIN_TARGET_ENTITY(Target, "m_strTargetVar")
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_TweenComponent* pxTween = ResolveOrAddTween(xContext, m_strTargetVar);
			if (pxTween == nullptr || !ValidEasing(m_iEasing))
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			const Zenith_Maths::Vector3 xTo = GetInput<Zenith_Maths::Vector3>(xContext, uPIN_To);
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

		// The const half is the EULER-DEGREES property, not an m_xTo.
		static constexpr u_int uPIN_To = 0u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_TweenRotation)
		ZENITH_GRAPH_PIN_INPUT_VAR_OR_CONST(To, "m_strToVar", "m_xToEulerDegrees", PROPERTY_TYPE_VECTOR3)
		ZENITH_GRAPH_PIN_TARGET_ENTITY(Target, "m_strTargetVar")
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_TweenComponent* pxTween = ResolveOrAddTween(xContext, m_strTargetVar);
			if (pxTween == nullptr || !ValidEasing(m_iEasing))
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			// The pin's const half is m_xToEulerDegrees; GetInput reads it through
			// the descriptor, so the differently-spelled constant is honoured
			// without this Execute naming it.
			const Zenith_Maths::Vector3 xTo = GetInput<Zenith_Maths::Vector3>(xContext, uPIN_To);
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

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_WaitForTween)
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

		// m_iProperty is a const selector (-1 = all) with no var partner.
		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_StopTweens)
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

		// m_strConfigName names a registered emitter CONFIG asset, not a
		// blackboard variable; m_iCount is a const with no var partner.
		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_EmitParticles)
		ZENITH_GRAPH_PIN_TARGET_ENTITY(Target, "m_strTargetVar")
		ZENITH_GRAPH_PINS_END

	public:
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

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_SetParticleEmitting)
		ZENITH_GRAPH_PIN_TARGET_ENTITY(Target, "m_strTargetVar")
		ZENITH_GRAPH_PINS_END

	public:
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

		// Position is a POSITION ref (Zenith_GraphNode_ResolvePositionRef in the
		// Execute below; "" = self). m_xOffset, m_xDirection, m_bSetDirection and
		// m_bClear are consts with no var partner - none is a pin.
		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_SetParticleEmitPosition)
		ZENITH_GRAPH_PIN_TARGET_POSITION(Position, "m_strPositionVar")
		ZENITH_GRAPH_PIN_TARGET_ENTITY(Target, "m_strTargetVar")
		ZENITH_GRAPH_PINS_END

	public:
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

#include "EntityComponent/Zenith_GraphNode_Registration_Animation.Tests.inl"
