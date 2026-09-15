//------------------------------------------------------------------------------
// Pin-table coverage for the Animation node TU (animator + tweens + particles).
// Included at the bottom of Zenith_GraphNode_Registration_Animation.cpp
// (ZENITH_TESTING), where the anonymous-namespace node classes and this TU's
// registrar are still in scope.
//
// What the totality walk proves, why the registry is SWAPPED rather than
// filtered, and why the restore is RAII all live ONCE, in the shared harness:
// Zenith_GraphPinTotality.TestHarness.inl. This file carries only what is
// specific to this TU - its registrar and its representative pins.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"

#ifdef ZENITH_TESTING

#include "Scripting/Zenith_BehaviourGraph.h"
#include "EntityComponent/Zenith_GraphPinTotality.TestHarness.inl"

ZENITH_TEST(GraphPinTable, AnimationTotality)
{
	// No exemptions: every m_str*Var* property in this TU is expressible as a
	// pin. The three string properties this TU does NOT annotate -
	// m_strParameter, m_strState and m_strConfigName - name an animator
	// parameter, an animator state and a registered emitter config; none is a
	// blackboard variable, and none matches the harness's m_str*Var* matcher.
	Zenith_CheckPinTableTotality(&Zenith_RegisterEngineGraphNodes_Animation, "_Animation.cpp", nullptr, 0u);
}

ZENITH_TEST(GraphPinTable, AnimationRoleSpotCheck)
{
	// INPUT_CONST, once per parameter width: the pin's TYPE is the
	// animator setter's, not the property's spelling.
	Zenith_CheckGraphPin("SetAnimatorFloat", "Value", GRAPH_PIN_ROLE_INPUT, PROPERTY_TYPE_FLOAT, "");
	Zenith_CheckGraphPin("SetAnimatorBool", "Value", GRAPH_PIN_ROLE_INPUT, PROPERTY_TYPE_BOOL, "");
	const Zenith_GraphPinDesc* pxValue = Zenith_FindGraphPin("SetAnimatorFloat", "Value");
	ZENITH_ASSERT_NOT_NULL(pxValue, "SetAnimatorFloat must declare a Value pin");
	if (pxValue != nullptr)
	{
		ZENITH_ASSERT_STREQ(pxValue->m_szConstProperty, "m_fValue",
			"SetAnimatorFloat.Value lost its inline-constant half - an unnamed var would read as unwritten");
	}

	// ★ TweenRotation's constant is the EULER-DEGREES property, not the m_xTo
	// its two siblings carry: the same pin NAME binds a differently-spelled
	// const, which only an explicit assertion catches (the totality walk proves
	// the property EXISTS, not that it is the right one).
	const Zenith_GraphPinDesc* pxTo = Zenith_FindGraphPin("TweenRotation", "To");
	ZENITH_ASSERT_NOT_NULL(pxTo, "TweenRotation must declare a To pin");
	if (pxTo != nullptr)
	{
		ZENITH_ASSERT_STREQ(pxTo->m_szConstProperty, "m_xToEulerDegrees",
			"TweenRotation.To must bind the euler-degrees constant, not m_xTo");
	}

	// TARGET_ENTITY, reached through ResolveTargetAnimator / ResolveOrAddTween /
	// ResolveTargetEmitter - three wrappers, one ENTITY reference.
	Zenith_CheckGraphPin("CrossFadeAnimation", "Target", GRAPH_PIN_ROLE_TARGET_REF, eGRAPH_PIN_TYPE_ANY, "m_strTargetVar");
	Zenith_CheckGraphPin("WaitForTween", "Target", GRAPH_PIN_ROLE_TARGET_REF, eGRAPH_PIN_TYPE_ANY, "m_strTargetVar");

	// ★ The two TARGET flavours differ ONLY in the accepted-type mask, so a
	// spot check that skips the mask cannot tell "entity only" from
	// "entity or vec3".
	const Zenith_GraphPinDesc* pxEmitTarget = Zenith_FindGraphPin("SetParticleEmitPosition", "Target");
	const Zenith_GraphPinDesc* pxEmitPosition = Zenith_FindGraphPin("SetParticleEmitPosition", "Position");
	ZENITH_ASSERT_NOT_NULL(pxEmitTarget, "SetParticleEmitPosition must declare a Target pin");
	ZENITH_ASSERT_NOT_NULL(pxEmitPosition, "SetParticleEmitPosition must declare a Position pin");
	if (pxEmitTarget != nullptr && pxEmitPosition != nullptr)
	{
		ZENITH_ASSERT_EQ(pxEmitTarget->m_uAcceptedTypeMask, uGRAPH_PIN_ACCEPT_TARGET_ENTITY,
			"SetParticleEmitPosition.Target must accept a packed ENTITY_ID and nothing else");
		ZENITH_ASSERT_EQ(pxEmitPosition->m_uAcceptedTypeMask, uGRAPH_PIN_ACCEPT_TARGET_POSITION,
			"SetParticleEmitPosition.Position is a polymorphic position ref - ENTITY_ID or VECTOR3");
	}

	// OUTPUTs: ReadAnimatorState's four results, each typed by the value its
	// Execute writes. The slot is always latched on SUCCESS; only the BLACKBOARD
	// write is skipped for an empty name.
	Zenith_CheckGraphPin("ReadAnimatorState", "StateName", GRAPH_PIN_ROLE_OUTPUT, PROPERTY_TYPE_STRING, "");
	Zenith_CheckGraphPin("ReadAnimatorState", "NormalizedTime", GRAPH_PIN_ROLE_OUTPUT, PROPERTY_TYPE_FLOAT, "");
	Zenith_CheckGraphPin("ReadAnimatorState", "HasLooped", GRAPH_PIN_ROLE_OUTPUT, PROPERTY_TYPE_BOOL, "");
}

//==============================================================================
// PIN RUNTIME for this TU (B-6.4) - the pins are LIVE.
//
// Every node above reads its INPUT descriptors through Zenith_GraphNode::GetInput
// and writes its OUTPUT descriptors through SetOutput. The two tests ABOVE, plus
// GraphComponent.AnimatorTweenParticleNodesExecution (whose SetAnimator* and
// Tween* instances bind NO var and therefore take the CONST path of
// MakePinDefault, and whose `animState` / `animTrans` are observed through
// output slots), are the proof that an UNCONNECTED node is unchanged. The rows
// below are the proof that a WIRE now carries a value.
//
// ★ EVERY ROW ASSERTS THE EXECUTE STATUS FIRST. ResolveTargetAnimator /
// ResolveOrAddTween fail before any accessor runs, so a row that checked an
// animator parameter without checking the status would pass just as happily on a
// node that did nothing at all.
//
// ★ THE FIXTURE IS REAL, and it is the state-machine recipe
// GraphComponent.AnimatorTweenParticleNodesExecution uses - with SetState in
// place of that test's pose/skeleton Update(0), which is all that is needed to
// ENTER the default state. Without entering it, HasStateMachine() is true while
// GetCurrentStateInfo() answers an EMPTY name, and every Output_ row would be
// asserting against "" - which is also the STRING slot's stamped zero.
//
// ★ WHAT THIS TU CANNOT COVER, said out loud: Flux_AnimationStateMachine::
// GetCurrentStateInfo fills m_fNormalizedTime and m_bHasLooped only inside
// `if (pxBlendTree)`, and AddState leaves a state with NO blend tree. A
// synthetic, clip-less state therefore ALWAYS reports NormalizedTime 0 and
// HasLooped false, so those two pins are covered by the APPEARANCE of their
// output slots, never by a distinguishing value. A true HasLooped and a non-zero
// NormalizedTime need a clip-backed blend tree and are NOT covered here.
//
// ★ ORDERING RULE (B-6.1): pin state is built ONCE, on the first accessor call,
// from the properties as they read THEN. Assign every property before the first
// Execute; use a FRESH node when a leg needs a different var name.
//
// ★ INPUT values are slot-only. The rows below use typed wires or current
// constants and never depend on an input-name property.
//
// ★ These fixtures never reach a counted census log: the per-game census parses
// `zenith test <G> --headless` runs, which pass --skip-unit-tests.
//==============================================================================

#include "UnitTests/Zenith_TempScene.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Flux/MeshAnimation/Flux_AnimationStateMachine.h"

// One row of the index contract: the constant (or the literal, for a pin no
// Execute addresses) names the pin it is documented as, with the role the
// migration assumed.
inline void AnimPin_Check(const Zenith_GraphPinTable& xPins, u_int uIndex, const char* szName,
	Zenith_GraphPinRole eRole, const char* szClass)
{
	ZENITH_ASSERT_LT(uIndex, xPins.GetPinCount(), "%s: pin index %u is past the end of the table", szClass, uIndex);
	if (uIndex >= xPins.GetPinCount())
	{
		return;
	}
	const Zenith_GraphPinDesc& xDesc = xPins.GetPinAt(uIndex);
	ZENITH_ASSERT_STREQ(xDesc.m_szName, szName,
		"%s: pin %u is '%s', not '%s' - the table was REORDERED, so every uPIN_ constant now addresses the wrong pin",
		szClass, uIndex, xDesc.m_szName != nullptr ? xDesc.m_szName : "(null)", szName);
	ZENITH_ASSERT_EQ(static_cast<int>(xDesc.m_eRole), static_cast<int>(eRole),
		"%s: pin '%s' changed ROLE - its runtime accessor would bad-access", szClass, szName);
}

inline void AnimPin_SeedFloat(Zenith_GraphBlackboard& xBB, const char* szName, float fValue)
{
	Zenith_PropertyValue xValue;
	xValue.SetFloat(fValue);
	xBB.SetValue(szName, xValue);
}

inline void AnimPin_SeedInt(Zenith_GraphBlackboard& xBB, const char* szName, int32_t iValue)
{
	Zenith_PropertyValue xValue;
	xValue.SetInt32(iValue);
	xBB.SetValue(szName, xValue);
}

inline void AnimPin_SeedBool(Zenith_GraphBlackboard& xBB, const char* szName, bool bValue)
{
	Zenith_PropertyValue xValue;
	xValue.SetBool(bValue);
	xBB.SetValue(szName, xValue);
}

inline void AnimPin_SeedVec3(Zenith_GraphBlackboard& xBB, const char* szName, const Zenith_Maths::Vector3& xVec)
{
	Zenith_PropertyValue xValue;
	xValue.SetVector3(xVec);
	xBB.SetValue(szName, xValue);
}

inline Zenith_PropertyValue AnimPin_WireFloat(float fValue)
{
	Zenith_PropertyValue xValue;
	xValue.SetFloat(fValue);
	return xValue;
}

inline Zenith_PropertyValue AnimPin_WireInt(int32_t iValue)
{
	Zenith_PropertyValue xValue;
	xValue.SetInt32(iValue);
	return xValue;
}

inline Zenith_PropertyValue AnimPin_WireBool(bool bValue)
{
	Zenith_PropertyValue xValue;
	xValue.SetBool(bValue);
	return xValue;
}

inline Zenith_PropertyValue AnimPin_WireVec3(const Zenith_Maths::Vector3& xVec)
{
	Zenith_PropertyValue xValue;
	xValue.SetVector3(xVec);
	return xValue;
}

class Zenith_GraphNode_AnimTestCountingFloatProducer : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_AnimTestCountingFloatProducer)
public:
	static constexpr u_int uPIN_Value = 0u;
	ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_AnimTestCountingFloatProducer)
	ZENITH_GRAPH_PIN_OUTPUT(Value, PROPERTY_TYPE_FLOAT)
	ZENITH_GRAPH_PINS_END

public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override { ++s_uPullCount; SetOutput<float>(xContext, uPIN_Value, s_fValue); return GRAPH_NODE_STATUS_SUCCESS; }
	const char* GetTypeName() const override { return "Test_AnimCountingFloatProducer"; }
	inline static u_int s_uPullCount = 0u;
	inline static float s_fValue = 0.0f;
};

class Zenith_GraphNode_AnimTestCountingVec3Producer : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_AnimTestCountingVec3Producer)
public:
	static constexpr u_int uPIN_Value = 0u;
	ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_AnimTestCountingVec3Producer)
	ZENITH_GRAPH_PIN_OUTPUT(Value, PROPERTY_TYPE_VECTOR3)
	ZENITH_GRAPH_PINS_END

public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override { ++s_uPullCount; SetOutput<Zenith_Maths::Vector3>(xContext, uPIN_Value, s_xValue); return GRAPH_NODE_STATUS_SUCCESS; }
	const char* GetTypeName() const override { return "Test_AnimCountingVec3Producer"; }
	inline static u_int s_uPullCount = 0u;
	inline static Zenith_Maths::Vector3 s_xValue = Zenith_Maths::Vector3(0.0f);
};

static void EnsureAnimCountingGuardProducersRegistered()
{
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
	xRegistry.EnsureInitialized();
	if (xRegistry.Find("Test_AnimCountingFloatProducer") == nullptr)
		xRegistry.RegisterNodeType<Zenith_GraphNode_AnimTestCountingFloatProducer>("Test_AnimCountingFloatProducer", GRAPH_EVENT_NONE, 1, false, "Test", false, true);
	if (xRegistry.Find("Test_AnimCountingVec3Producer") == nullptr)
		xRegistry.RegisterNodeType<Zenith_GraphNode_AnimTestCountingVec3Producer>("Test_AnimCountingVec3Producer", GRAPH_EVENT_NONE, 1, false, "Test", false, true);
}

// The slot readers are TAG-CHECKED: Zenith_PropertyValue's typed getters
// Zenith_Assert on a mismatch, and a wrong slot type must read as a test FAILURE
// rather than a DebugBreak. They take a bare Zenith_PropertyValue pointer, so the
// same helper reads an output SLOT and a BLACKBOARD entry.
inline std::string AnimPin_SlotString(const Zenith_PropertyValue* pxSlot, const char* szWhat)
{
	ZENITH_ASSERT_NOT_NULL(pxSlot, "%s: the value is UNSET", szWhat);
	if (pxSlot == nullptr)
	{
		return std::string();
	}
	ZENITH_ASSERT_EQ(static_cast<int>(pxSlot->GetType()), static_cast<int>(PROPERTY_TYPE_STRING),
		"%s: the value holds type %u, not STRING", szWhat, static_cast<u_int>(pxSlot->GetType()));
	return pxSlot->GetType() == PROPERTY_TYPE_STRING ? pxSlot->GetString() : std::string();
}

inline float AnimPin_SlotFloat(const Zenith_PropertyValue* pxSlot, const char* szWhat)
{
	ZENITH_ASSERT_NOT_NULL(pxSlot, "%s: the value is UNSET", szWhat);
	if (pxSlot == nullptr)
	{
		return 0.0f;
	}
	ZENITH_ASSERT_EQ(static_cast<int>(pxSlot->GetType()), static_cast<int>(PROPERTY_TYPE_FLOAT),
		"%s: the value holds type %u, not FLOAT", szWhat, static_cast<u_int>(pxSlot->GetType()));
	return pxSlot->GetType() == PROPERTY_TYPE_FLOAT ? pxSlot->GetFloat() : 0.0f;
}

inline bool AnimPin_SlotBool(const Zenith_PropertyValue* pxSlot, const char* szWhat)
{
	ZENITH_ASSERT_NOT_NULL(pxSlot, "%s: the value is UNSET", szWhat);
	if (pxSlot == nullptr)
	{
		return false;
	}
	ZENITH_ASSERT_EQ(static_cast<int>(pxSlot->GetType()), static_cast<int>(PROPERTY_TYPE_BOOL),
		"%s: the value holds type %u, not BOOL", szWhat, static_cast<u_int>(pxSlot->GetType()));
	return pxSlot->GetType() == PROPERTY_TYPE_BOOL ? pxSlot->GetBool() : false;
}

namespace
{
	// A TempScene holding the two entities the rows below need: one carrying a
	// live animator whose state machine has ENTERED "Idle", and one with a
	// transform and nothing else - the guarded-FAILURE leg's animator target, and
	// a perfectly good tween target (a tween needs only a transform).
	struct Zenith_AnimationPinFixture
	{
		explicit Zenith_AnimationPinFixture(const char* szSceneName)
			: m_xScene(szSceneName)
		{
			m_xAnimated = m_xScene.CreateEntity("PinAnimated");
			Zenith_AnimatorComponent& xAnimator = m_xAnimated.AddComponent<Zenith_AnimatorComponent>();
			m_pxStateMachine = xAnimator.CreateStateMachine();
			ZENITH_ASSERT_NOT_NULL(m_pxStateMachine,
				"no state machine - ResolveTargetAnimator would FAILURE every row below");
			if (m_pxStateMachine != nullptr)
			{
				m_pxStateMachine->AddState("Idle");
				m_pxStateMachine->AddState("Run");
				m_pxStateMachine->SetDefaultState("Idle");
				m_pxStateMachine->GetParameters().AddFloat("Speed");
				m_pxStateMachine->GetParameters().AddInt("Phase");
				m_pxStateMachine->GetParameters().AddBool("Grounded");
				// ENTER the default state. HasStateMachine() is true without this,
				// but the state info would carry an EMPTY name.
				m_pxStateMachine->SetState("Idle");
				ZENITH_ASSERT_STREQ(m_pxStateMachine->GetCurrentStateName().c_str(), "Idle",
					"the fixture never entered its default state - every Output_ row would assert against \"\"");
				m_bReady = xAnimator.HasStateMachine()
					&& m_pxStateMachine->GetCurrentStateName() == "Idle";
			}
			ZENITH_ASSERT_TRUE(m_bReady, "the animator fixture is not usable - the rows below would be vacuous");

			m_xNoAnimator = m_xScene.CreateEntity("PinNoAnimator");
		}

		Flux_AnimationParameters& Parameters() const { return m_pxStateMachine->GetParameters(); }

		Zenith_TempScene m_xScene;
		Zenith_Entity m_xAnimated;
		Zenith_Entity m_xNoAnimator;
		Flux_AnimationStateMachine* m_pxStateMachine = nullptr;
		bool m_bReady = false;
	};

	// A transform-only entity for the three tween rows, which need no animator.
	struct Zenith_TweenPinFixture
	{
		explicit Zenith_TweenPinFixture(const char* szSceneName)
			: m_xScene(szSceneName)
		{
			m_xEntity = m_xScene.CreateEntity("PinTweenTarget");
			ZENITH_ASSERT_TRUE(m_xEntity.IsValid());
		}

		Zenith_TempScene m_xScene;
		Zenith_Entity m_xEntity;
	};
}

// ★ TABLE ORDER IS THE CONTRACT. A pin INDEX is what every accessor addresses, so a
// reorder - or an inserted pin - silently re-points every uPIN_ constant in this TU
// at a different descriptor. SEVEN of the fourteen classes declare no uPIN_ constant
// because nothing in their Execute addresses a pin; asserting their COUNT and their
// index-0 TARGET here is what would notice a value pin being ADDED to one of them
// without its Execute being migrated. A static_assert is impossible: the tables are
// filled at static init.
ZENITH_TEST(GraphPinTable, AnimationPinIndicesMatchTables)
{
	const Zenith_GraphPinTable& xFloat = Zenith_GraphNode_SetAnimatorFloat::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xFloat.GetPinCount(), 2u, "SetAnimatorFloat gained or lost a pin");
	AnimPin_Check(xFloat, Zenith_GraphNode_SetAnimatorFloat::uPIN_Value, "Value", GRAPH_PIN_ROLE_INPUT,
		"SetAnimatorFloat");
	AnimPin_Check(xFloat, 1u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "SetAnimatorFloat");

	const Zenith_GraphPinTable& xInt = Zenith_GraphNode_SetAnimatorInt::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xInt.GetPinCount(), 2u, "SetAnimatorInt gained or lost a pin");
	AnimPin_Check(xInt, Zenith_GraphNode_SetAnimatorInt::uPIN_Value, "Value", GRAPH_PIN_ROLE_INPUT, "SetAnimatorInt");
	AnimPin_Check(xInt, 1u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "SetAnimatorInt");

	const Zenith_GraphPinTable& xBool = Zenith_GraphNode_SetAnimatorBool::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xBool.GetPinCount(), 2u, "SetAnimatorBool gained or lost a pin");
	AnimPin_Check(xBool, Zenith_GraphNode_SetAnimatorBool::uPIN_Value, "Value", GRAPH_PIN_ROLE_INPUT,
		"SetAnimatorBool");
	AnimPin_Check(xBool, 1u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "SetAnimatorBool");

	// ★ FOUR OUTPUTS AND A TARGET, in that order: the Target is LAST here, which is
	// the opposite of ReadVelocity in the Physics TU - a copied constant would
	// address the wrong pin, and a wrong-role SetOutput is a SILENT no-op.
	const Zenith_GraphPinTable& xRead = Zenith_GraphNode_ReadAnimatorState::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xRead.GetPinCount(), 5u, "ReadAnimatorState gained or lost a pin");
	AnimPin_Check(xRead, Zenith_GraphNode_ReadAnimatorState::uPIN_StateName, "StateName", GRAPH_PIN_ROLE_OUTPUT,
		"ReadAnimatorState");
	AnimPin_Check(xRead, Zenith_GraphNode_ReadAnimatorState::uPIN_NormalizedTime, "NormalizedTime",
		GRAPH_PIN_ROLE_OUTPUT, "ReadAnimatorState");
	AnimPin_Check(xRead, Zenith_GraphNode_ReadAnimatorState::uPIN_Transitioning, "Transitioning",
		GRAPH_PIN_ROLE_OUTPUT, "ReadAnimatorState");
	AnimPin_Check(xRead, Zenith_GraphNode_ReadAnimatorState::uPIN_HasLooped, "HasLooped", GRAPH_PIN_ROLE_OUTPUT,
		"ReadAnimatorState");
	AnimPin_Check(xRead, 4u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "ReadAnimatorState");

	const Zenith_GraphPinTable& xPos = Zenith_GraphNode_TweenPosition::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xPos.GetPinCount(), 2u, "TweenPosition gained or lost a pin");
	AnimPin_Check(xPos, Zenith_GraphNode_TweenPosition::uPIN_To, "To", GRAPH_PIN_ROLE_INPUT, "TweenPosition");
	AnimPin_Check(xPos, 1u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "TweenPosition");

	const Zenith_GraphPinTable& xScale = Zenith_GraphNode_TweenScale::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xScale.GetPinCount(), 2u, "TweenScale gained or lost a pin");
	AnimPin_Check(xScale, Zenith_GraphNode_TweenScale::uPIN_To, "To", GRAPH_PIN_ROLE_INPUT, "TweenScale");
	AnimPin_Check(xScale, 1u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "TweenScale");

	const Zenith_GraphPinTable& xRot = Zenith_GraphNode_TweenRotation::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xRot.GetPinCount(), 2u, "TweenRotation gained or lost a pin");
	AnimPin_Check(xRot, Zenith_GraphNode_TweenRotation::uPIN_To, "To", GRAPH_PIN_ROLE_INPUT, "TweenRotation");
	AnimPin_Check(xRot, 1u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "TweenRotation");

	// The SEVEN classes whose Execute addresses no pin at all.
	const Zenith_GraphPinTable& xTrigger = Zenith_GraphNode_SetAnimatorTrigger::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xTrigger.GetPinCount(), 1u, "SetAnimatorTrigger gained a pin; nothing in its Execute addresses one");
	AnimPin_Check(xTrigger, 0u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "SetAnimatorTrigger");

	const Zenith_GraphPinTable& xFade = Zenith_GraphNode_CrossFadeAnimation::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xFade.GetPinCount(), 1u, "CrossFadeAnimation gained a pin");
	AnimPin_Check(xFade, 0u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "CrossFadeAnimation");

	const Zenith_GraphPinTable& xWait = Zenith_GraphNode_WaitForTween::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xWait.GetPinCount(), 1u, "WaitForTween gained a pin");
	AnimPin_Check(xWait, 0u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "WaitForTween");

	const Zenith_GraphPinTable& xStop = Zenith_GraphNode_StopTweens::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xStop.GetPinCount(), 1u, "StopTweens gained a pin");
	AnimPin_Check(xStop, 0u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "StopTweens");

	const Zenith_GraphPinTable& xEmit = Zenith_GraphNode_EmitParticles::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xEmit.GetPinCount(), 1u, "EmitParticles gained a pin");
	AnimPin_Check(xEmit, 0u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "EmitParticles");

	const Zenith_GraphPinTable& xEmitting = Zenith_GraphNode_SetParticleEmitting::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xEmitting.GetPinCount(), 1u, "SetParticleEmitting gained a pin");
	AnimPin_Check(xEmitting, 0u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "SetParticleEmitting");

	const Zenith_GraphPinTable& xEmitPos = Zenith_GraphNode_SetParticleEmitPosition::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xEmitPos.GetPinCount(), 2u, "SetParticleEmitPosition gained or lost a pin");
	AnimPin_Check(xEmitPos, 0u, "Position", GRAPH_PIN_ROLE_TARGET_REF, "SetParticleEmitPosition");
	AnimPin_Check(xEmitPos, 1u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "SetParticleEmitPosition");
}

//------------------------------------------------------------------------------
// WIRED INPUTS
//------------------------------------------------------------------------------

// The 9/7/5 discipline: the current constant (9), an unrelated blackboard value
// (7), and the WIRE (5). Only the live wire can produce 5.
ZENITH_TEST(AnimationPinRuntime, Wired_SetAnimatorFloatValue)
{
	Zenith_AnimationPinFixture xFixture("TestAnimPinFloatScene");
	if (!xFixture.m_bReady)
	{
		return;
	}
	const u_int uValue = Zenith_GraphNode_SetAnimatorFloat::uPIN_Value;

	Zenith_GraphBlackboard xBB;
	AnimPin_SeedFloat(xBB, "sv", 7.0f);

	Zenith_GraphNode_SetAnimatorFloat xNode;
	xNode.m_strParameter = "Speed";
	xNode.m_fValue = 9.0f;
	xNode.m_strTargetVar = "";								// "" = self
	xNode.SetInputForTest(uValue, AnimPin_WireFloat(5.0f));

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	xCtx.m_xSelf = xFixture.m_xAnimated;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	ZENITH_ASSERT_EQ_FLOAT(xFixture.Parameters().GetFloat("Speed"), 5.0f, 0.0001f,
		"the animator parameter did not come from the WIRE");
	ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
}

ZENITH_TEST(AnimationPinRuntime, Wired_SetAnimatorIntValue)
{
	Zenith_AnimationPinFixture xFixture("TestAnimPinIntScene");
	if (!xFixture.m_bReady)
	{
		return;
	}
	const u_int uValue = Zenith_GraphNode_SetAnimatorInt::uPIN_Value;

	Zenith_GraphBlackboard xBB;
	AnimPin_SeedInt(xBB, "iv", 7);

	Zenith_GraphNode_SetAnimatorInt xNode;
	xNode.m_strParameter = "Phase";
	xNode.m_iValue = 9;
	xNode.m_strTargetVar = "";
	xNode.SetInputForTest(uValue, AnimPin_WireInt(5));

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	xCtx.m_xSelf = xFixture.m_xAnimated;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	ZENITH_ASSERT_EQ(xFixture.Parameters().GetInt("Phase"), 5);
	ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
}

// ★ A BOOL HAS ONLY TWO STATES, so no single leg can make the wire differ from
// BOTH the const and the variable. Two legs, opposite ways round, do: in each one
// the const and the variable AGREE and the wire disagrees with them, so a node
// that read either of the losing legs lands on the value this row rejects.
ZENITH_TEST(AnimationPinRuntime, Wired_SetAnimatorBoolValue)
{
	Zenith_AnimationPinFixture xFixture("TestAnimPinBoolScene");
	if (!xFixture.m_bReady)
	{
		return;
	}
	const u_int uValue = Zenith_GraphNode_SetAnimatorBool::uPIN_Value;

	Zenith_GraphBlackboard xBB;
	AnimPin_SeedBool(xBB, "bFalse", false);
	AnimPin_SeedBool(xBB, "bTrue", true);

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	xCtx.m_xSelf = xFixture.m_xAnimated;

	// (a) const false, var false, WIRE true.
	{
		Zenith_GraphNode_SetAnimatorBool xNode;
		xNode.m_strParameter = "Grounded";
		xNode.m_bValue = false;
		xNode.m_strTargetVar = "";
		xNode.SetInputForTest(uValue, AnimPin_WireBool(true));
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_TRUE(xFixture.Parameters().GetBool("Grounded"),
			"the wired TRUE lost to the const/variable FALSE");
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}

	// (b) the mirror image, on a FRESH node: const true, var true, WIRE false.
	{
		Zenith_GraphNode_SetAnimatorBool xNode;
		xNode.m_strParameter = "Grounded";
		xNode.m_bValue = true;
		xNode.m_strTargetVar = "";
		xNode.SetInputForTest(uValue, AnimPin_WireBool(false));
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_FALSE(xFixture.Parameters().GetBool("Grounded"),
			"the wired FALSE lost to the const/variable TRUE");
	}
}

// The tween rows all use m_fDuration = 0, which completes on the first hand tick
// (the fixture pattern GraphComponent.AnimatorTweenParticleNodesExecution uses for
// its rotation tween), so the transform IS the resolved target value.
ZENITH_TEST(AnimationPinRuntime, Wired_TweenPositionTo)
{
	Zenith_TweenPinFixture xFixture("TestAnimPinTweenPosScene");
	const u_int uTo = Zenith_GraphNode_TweenPosition::uPIN_To;

	Zenith_GraphBlackboard xBB;
	AnimPin_SeedVec3(xBB, "tv", Zenith_Maths::Vector3(0.0f, 7.0f, 0.0f));

	Zenith_GraphNode_TweenPosition xNode;
	xNode.m_xTo = Zenith_Maths::Vector3(9.0f, 0.0f, 0.0f);
	xNode.m_fDuration = 0.0f;
	xNode.m_iEasing = EASING_LINEAR;
	xNode.m_strTargetVar = "";
	// THREE DISTINCT DIRECTIONS: +Z is the wire, +X the const, +Y the variable.
	xNode.SetInputForTest(uTo, AnimPin_WireVec3(Zenith_Maths::Vector3(0.0f, 0.0f, 5.0f)));

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	xCtx.m_xSelf = xFixture.m_xEntity;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	Zenith_TweenComponent* pxTween = xFixture.m_xEntity.TryGetComponent<Zenith_TweenComponent>();
	ZENITH_ASSERT_NOT_NULL(pxTween, "ResolveOrAddTween did not add the tween component");
	if (pxTween == nullptr)
	{
		return;
	}
	pxTween->OnUpdate(0.016f);

	Zenith_Maths::Vector3 xPosition;
	xFixture.m_xEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xPosition);
	ZENITH_ASSERT_NEAR_VEC3(xPosition, Zenith_Maths::Vector3(0.0f, 0.0f, 5.0f), 0.001f);
	ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
}

ZENITH_TEST(AnimationPinRuntime, Wired_TweenScaleTo)
{
	Zenith_TweenPinFixture xFixture("TestAnimPinTweenScaleScene");
	const u_int uTo = Zenith_GraphNode_TweenScale::uPIN_To;

	Zenith_GraphBlackboard xBB;
	AnimPin_SeedVec3(xBB, "tv", Zenith_Maths::Vector3(7.0f, 7.0f, 7.0f));

	Zenith_GraphNode_TweenScale xNode;
	xNode.m_xTo = Zenith_Maths::Vector3(9.0f, 9.0f, 9.0f);
	xNode.m_fDuration = 0.0f;
	xNode.m_iEasing = EASING_LINEAR;
	xNode.m_strTargetVar = "";
	// THREE DISTINCT COMPONENTS on the wire, so a transposed or partially-read
	// vector fails rather than agreeing by symmetry.
	xNode.SetInputForTest(uTo, AnimPin_WireVec3(Zenith_Maths::Vector3(5.0f, 6.0f, 7.0f)));

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	xCtx.m_xSelf = xFixture.m_xEntity;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	Zenith_TweenComponent* pxTween = xFixture.m_xEntity.TryGetComponent<Zenith_TweenComponent>();
	ZENITH_ASSERT_NOT_NULL(pxTween);
	if (pxTween == nullptr)
	{
		return;
	}
	pxTween->OnUpdate(0.016f);

	Zenith_Maths::Vector3 xScale;
	xFixture.m_xEntity.GetComponent<Zenith_TransformComponent>().GetScale(xScale);
	ZENITH_ASSERT_NEAR_VEC3(xScale, Zenith_Maths::Vector3(5.0f, 6.0f, 7.0f), 0.001f);
}

// ★ YAW 0 MUST NOT BE THE WIRE LEG: an identity rotation is indistinguishable from
// "the node never ran". Const 0, variable 180, WIRE 90 - and 90 degrees of yaw puts
// forward on +X (the mapping GraphComponent.AnimatorTweenParticleNodesExecution
// pins), which neither losing leg produces.
ZENITH_TEST(AnimationPinRuntime, Wired_TweenRotationTo)
{
	Zenith_TweenPinFixture xFixture("TestAnimPinTweenRotScene");
	const u_int uTo = Zenith_GraphNode_TweenRotation::uPIN_To;

	Zenith_GraphBlackboard xBB;
	AnimPin_SeedVec3(xBB, "tv", Zenith_Maths::Vector3(0.0f, 180.0f, 0.0f));

	Zenith_GraphNode_TweenRotation xNode;
	xNode.m_xToEulerDegrees = Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f);
	xNode.m_fDuration = 0.0f;
	xNode.m_iEasing = EASING_LINEAR;
	xNode.m_strTargetVar = "";
	xNode.SetInputForTest(uTo, AnimPin_WireVec3(Zenith_Maths::Vector3(0.0f, 90.0f, 0.0f)));

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	xCtx.m_xSelf = xFixture.m_xEntity;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	Zenith_TweenComponent* pxTween = xFixture.m_xEntity.TryGetComponent<Zenith_TweenComponent>();
	ZENITH_ASSERT_NOT_NULL(pxTween);
	if (pxTween == nullptr)
	{
		return;
	}
	pxTween->OnUpdate(0.016f);

	Zenith_Maths::Quat xRotation;
	xFixture.m_xEntity.GetComponent<Zenith_TransformComponent>().GetRotation(xRotation);
	const Zenith_Maths::Vector3 xForward = xRotation * Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
	ZENITH_ASSERT_NEAR_VEC3(xForward, Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f), 0.01f);
	ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
}

//------------------------------------------------------------------------------
// OUTPUTS
//------------------------------------------------------------------------------

// StateName is the one output of this node whose value a clip-less fixture can
// distinguish from its stamped zero ("Idle" vs ""), so it carries the VALUE claim.
// NormalizedTime and HasLooped use their output-slot presence instead: the
// blackboard entry does not exist before the Execute and is present, typed, after
// it. See the banner above for why no clip-less state can report either of them
// non-zero / true.
ZENITH_TEST(AnimationPinRuntime, Output_ReadAnimatorStateCarriesValuesAndDualWritesNamedOutputs)
{
	Zenith_AnimationPinFixture xFixture("TestAnimPinReadStateScene");
	if (!xFixture.m_bReady)
	{
		return;
	}

	Zenith_GraphBlackboard xBB;
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	xCtx.m_xSelf = xFixture.m_xAnimated;


	Zenith_GraphNode_ReadAnimatorState xNode;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	// SLOTS: all four latched, the STRING one by value.
	ZENITH_ASSERT_STREQ(AnimPin_SlotString(xNode.GetOutputForTest(Zenith_GraphNode_ReadAnimatorState::uPIN_StateName),
		"ReadAnimatorState.StateName slot").c_str(), "Idle");
	const float fSlotTime = AnimPin_SlotFloat(
		xNode.GetOutputForTest(Zenith_GraphNode_ReadAnimatorState::uPIN_NormalizedTime),
		"ReadAnimatorState.NormalizedTime slot");
	// A clip-less synthetic state reports 0.0 - stated out loud, not merely agreed with.
	ZENITH_ASSERT_EQ_FLOAT(fSlotTime, 0.0f, 0.0001f, "a clip-less state must report NormalizedTime 0");
	ZENITH_ASSERT_FALSE(AnimPin_SlotBool(xNode.GetOutputForTest(Zenith_GraphNode_ReadAnimatorState::uPIN_Transitioning),
		"ReadAnimatorState.Transitioning slot"));
	ZENITH_ASSERT_FALSE(AnimPin_SlotBool(xNode.GetOutputForTest(Zenith_GraphNode_ReadAnimatorState::uPIN_HasLooped),
		"ReadAnimatorState.HasLooped slot"));

	ZENITH_ASSERT_EQ(xBB.GetCount(), 0u);
	ZENITH_ASSERT_NULL(xBB.TryGetValue(""));
	ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);

	// A fresh instance carries the same false HasLooped value in its typed slot.
	{
		Zenith_GraphBlackboard xNamedBB;
		Zenith_GraphContext xNamedCtx;
		xNamedCtx.m_pxBlackboard = &xNamedBB;
		xNamedCtx.m_xSelf = xFixture.m_xAnimated;

		Zenith_GraphNode_ReadAnimatorState xNamed;
		ZENITH_ASSERT_EQ(static_cast<int>(xNamed.Execute(xNamedCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_FALSE(AnimPin_SlotBool(xNamed.GetOutputForTest(Zenith_GraphNode_ReadAnimatorState::uPIN_HasLooped), "looped slot"),
			"a clip-less state cannot have looped");
		ZENITH_ASSERT_EQ(xNamedBB.GetCount(), 0u);
	}
}

// Transitioning is the one BOOL in this node a clip-less fixture CAN move: CrossFade
// allocates the transition immediately and m_bIsTransitioning is "a transition
// exists". No Update is driven afterwards, because ANY dt >= the fade duration
// completes it. ★ StateName is still "Idle" mid-fade - the current state advances
// only at CompleteTransition - so this row must not assert "Run".
ZENITH_TEST(AnimationPinRuntime, Output_ReadAnimatorStateTransitioningIsTrueMidCrossFade)
{
	Zenith_AnimationPinFixture xFixture("TestAnimPinTransitionScene");
	if (!xFixture.m_bReady)
	{
		return;
	}

	Zenith_GraphBlackboard xBB;
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	xCtx.m_xSelf = xFixture.m_xAnimated;

	// NEGATIVE CONTROL first: settled in "Idle", the node reports false.
	{
		Zenith_GraphNode_ReadAnimatorState xNode;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_FALSE(AnimPin_SlotBool(
			xNode.GetOutputForTest(Zenith_GraphNode_ReadAnimatorState::uPIN_Transitioning), "settled Transitioning"));
		ZENITH_ASSERT_EQ(xBB.GetCount(), 0u);
	}

	// A LONG fade, not ticked: the transition is in flight.
	xFixture.m_pxStateMachine->CrossFade("Run", 10.0f);
	ZENITH_ASSERT_TRUE(xFixture.m_pxStateMachine->IsTransitioning(),
		"the fixture is not mid-crossfade - the row below would assert nothing");

	Zenith_GraphNode_ReadAnimatorState xNode;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	ZENITH_ASSERT_TRUE(AnimPin_SlotBool(
		xNode.GetOutputForTest(Zenith_GraphNode_ReadAnimatorState::uPIN_Transitioning), "mid-fade Transitioning"),
		"the Transitioning slot did not carry the in-flight transition");
	ZENITH_ASSERT_EQ(xBB.GetCount(), 0u);
	ZENITH_ASSERT_STREQ(AnimPin_SlotString(
		xNode.GetOutputForTest(Zenith_GraphNode_ReadAnimatorState::uPIN_StateName), "mid-fade StateName").c_str(),
		"Idle", "the current state advances only at CompleteTransition");
}

// ★ A PARITY ROW, NOT A DIVERGENCE. All four writes were already guarded on a
// non-empty name, so "an empty name creates no blackboard variable" was true before
// this migration and is true after it. What is NEW is that all four slots carry the
// values anyway - the only reason a wire can come off an output whose author never
// named a variable. This TU therefore takes NO `""` divergence at all.
ZENITH_TEST(AnimationPinRuntime, Output_ReadAnimatorStateEmptyNamesParity)
{
	Zenith_AnimationPinFixture xFixture("TestAnimPinEmptyNamesScene");
	if (!xFixture.m_bReady)
	{
		return;
	}

	Zenith_GraphBlackboard xBB;
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	xCtx.m_xSelf = xFixture.m_xAnimated;

	Zenith_GraphNode_ReadAnimatorState xNode;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	ZENITH_ASSERT_STREQ(AnimPin_SlotString(xNode.GetOutputForTest(Zenith_GraphNode_ReadAnimatorState::uPIN_StateName),
		"unnamed StateName slot").c_str(), "Idle", "the slot is latched without a blackboard output write");
	ZENITH_ASSERT_EQ_FLOAT(AnimPin_SlotFloat(
		xNode.GetOutputForTest(Zenith_GraphNode_ReadAnimatorState::uPIN_NormalizedTime),
		"unnamed NormalizedTime slot"), 0.0f, 0.0001f);
	ZENITH_ASSERT_FALSE(AnimPin_SlotBool(
		xNode.GetOutputForTest(Zenith_GraphNode_ReadAnimatorState::uPIN_Transitioning), "unnamed Transitioning slot"));
	ZENITH_ASSERT_FALSE(AnimPin_SlotBool(
		xNode.GetOutputForTest(Zenith_GraphNode_ReadAnimatorState::uPIN_HasLooped), "unnamed HasLooped slot"));

	ZENITH_ASSERT_NULL(xBB.TryGetValue(""));
	ZENITH_ASSERT_EQ(xBB.GetCount(), 0u, "an unnamed OUTPUT created a blackboard variable");
}

// ★ THE FAILURE CONTRACT, pinned once for the whole node. ReadAnimatorState's only
// accessor is SetOutput and its resolver guard is ABOVE every write, so a FAILED
// execution writes no variable, latches no slot - and, on an instance that has never
// succeeded, has not even built pin state (EnsurePinState runs only from the three
// Execute-time accessors, so GetOutputForTest answers nullptr rather than a stamped
// zero). The node has no failure EXEC pin, so a consumer wire off any of its four
// outputs must be gated on SUCCESS: a re-execution that fails leaves the PREVIOUS
// values in place, which is what the first half of this row proves.
ZENITH_TEST(AnimationPinRuntime, Output_ReadAnimatorStateFailureBuildsNoSlotsAndWritesNothing)
{
	Zenith_AnimationPinFixture xFixture("TestAnimPinReadFailScene");
	if (!xFixture.m_bReady)
	{
		return;
	}

	Zenith_GraphBlackboard xBB;
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	xCtx.m_xSelf = xFixture.m_xAnimated;

	// SUCCEED first, on the animator entity.
	Zenith_GraphNode_ReadAnimatorState xNode;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	ZENITH_ASSERT_STREQ(AnimPin_SlotString(xNode.GetOutputForTest(Zenith_GraphNode_ReadAnimatorState::uPIN_StateName),
		"succeeded StateName").c_str(), "Idle");
	const u_int uBlackboardCount = xBB.GetCount();
	ZENITH_ASSERT_EQ(uBlackboardCount, 0u);

	// The SAME instance now cannot resolve a target: m_strTargetVar names a variable
	// the blackboard does not carry, so ResolveTargetEntity yields an invalid entity.
	xNode.m_strTargetVar = "__nobody__";
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
	ZENITH_ASSERT_STREQ(AnimPin_SlotString(xNode.GetOutputForTest(Zenith_GraphNode_ReadAnimatorState::uPIN_StateName),
		"failed StateName").c_str(), "Idle", "the FAILURE overwrote a previously latched slot");
	ZENITH_ASSERT_EQ(xBB.GetCount(), uBlackboardCount, "the FAILURE wrote a blackboard variable");

	// SECONDARY LEG, labelled as such: an instance that has ONLY ever failed has no
	// pin state at all - not a stamped zero, no slot.
	{
		Zenith_GraphBlackboard xFreshBB;
		Zenith_GraphContext xFreshCtx;
		xFreshCtx.m_pxBlackboard = &xFreshBB;
		xFreshCtx.m_xSelf = xFixture.m_xNoAnimator;		// a transform, no animator

		Zenith_GraphNode_ReadAnimatorState xFresh;
		ZENITH_ASSERT_EQ(static_cast<int>(xFresh.Execute(xFreshCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
		ZENITH_ASSERT_NULL(xFresh.GetOutputForTest(Zenith_GraphNode_ReadAnimatorState::uPIN_StateName),
			"the FAILURE is above every accessor, so no pin state was ever built - there is no slot to read");
		ZENITH_ASSERT_EQ(xFreshBB.GetCount(), 0u);
	}
}

//------------------------------------------------------------------------------
// THE CENSUS OBSERVABLE
//------------------------------------------------------------------------------

// ★ EVERY INPUT READ IN THIS TU SITS AFTER ITS RESOLVER GUARD - and, for a tween,
// after the easing guard too - so a FAILURE-before-read execution must log NO census
// producer. Each leg has its positive control: the identical configuration on a
// target that resolves.
ZENITH_TEST(AnimationPinRuntime, GuardedFailureDoesNotReadInputs)
{
	Zenith_AnimationPinFixture xFixture("TestAnimPinGuardScene");
	if (!xFixture.m_bReady)
	{
		return;
	}
	EnsureAnimCountingGuardProducersRegistered();
	const auto RunFloat = [](Zenith_Entity xSelf, bool bExpectSuccess)
	{
		Zenith_GraphDefinition xDef;
		const u_int uSource = xDef.AddNode("OnUpdate");
		const u_int uSetter = xDef.AddNode("SetAnimatorFloat");
		const u_int uProducer = xDef.AddNode("Test_AnimCountingFloatProducer");
		const u_int uSentinel = xDef.AddNode("SetBlackboardBool");
		Zenith_GraphNode_SetAnimatorFloat xParams;
		xParams.m_strParameter = "Speed";
		xDef.SetNodeParamsFromInstance(uSetter, &xParams);
		xDef.AddEdge(uSource, 0u, uSetter);
		xDef.AddEdge(uSetter, 0u, uSentinel);
		ZENITH_ASSERT_TRUE(xDef.AddDataEdge(uProducer, "Value", uSetter, "Value"));
		Zenith_BehaviourGraph xGraph;
		ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
		ZENITH_ASSERT_EQ(xGraph.GetResolutionSkipCountForTest(), 0u);
		Zenith_GraphContext xCtx;
		xCtx.m_xSelf = xSelf;
		xCtx.m_pxGraph = &xGraph; xCtx.m_pxBlackboard = &xGraph.GetBlackboard(); xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xCtx);
		ZENITH_ASSERT_EQ(xGraph.GetBlackboard().HasValue("flag"), bExpectSuccess);
	};
	Zenith_GraphNode_AnimTestCountingFloatProducer::s_fValue = 7.0f;
	Zenith_GraphNode_AnimTestCountingFloatProducer::s_uPullCount = 0u;
	RunFloat(xFixture.m_xNoAnimator, false);
	ZENITH_ASSERT_EQ(Zenith_GraphNode_AnimTestCountingFloatProducer::s_uPullCount, 0u,
		"the animator guard must run before Value is pulled");
	RunFloat(xFixture.m_xAnimated, true);
	ZENITH_ASSERT_EQ(Zenith_GraphNode_AnimTestCountingFloatProducer::s_uPullCount, 1u);
	ZENITH_ASSERT_EQ_FLOAT(xFixture.Parameters().GetFloat("Speed"), 7.0f, 0.0001f);

	const auto RunTween = [](Zenith_Entity xSelf, int32_t iEasing, bool bExpectSuccess)
	{
		Zenith_GraphDefinition xDef;
		const u_int uSource = xDef.AddNode("OnUpdate");
		const u_int uTween = xDef.AddNode("TweenScale");
		const u_int uProducer = xDef.AddNode("Test_AnimCountingVec3Producer");
		const u_int uSentinel = xDef.AddNode("SetBlackboardBool");
		Zenith_GraphNode_TweenScale xParams;
		xParams.m_iEasing = iEasing;
		xParams.m_fDuration = 0.0f;
		xDef.SetNodeParamsFromInstance(uTween, &xParams);
		xDef.AddEdge(uSource, 0u, uTween);
		xDef.AddEdge(uTween, 0u, uSentinel);
		ZENITH_ASSERT_TRUE(xDef.AddDataEdge(uProducer, "Value", uTween, "To"));
		Zenith_BehaviourGraph xGraph;
		ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
		ZENITH_ASSERT_EQ(xGraph.GetResolutionSkipCountForTest(), 0u);
		Zenith_GraphContext xCtx;
		xCtx.m_xSelf = xSelf;
		xCtx.m_pxGraph = &xGraph; xCtx.m_pxBlackboard = &xGraph.GetBlackboard(); xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xCtx);
		ZENITH_ASSERT_EQ(xGraph.GetBlackboard().HasValue("flag"), bExpectSuccess);
	};
	Zenith_GraphNode_AnimTestCountingVec3Producer::s_xValue = Zenith_Maths::Vector3(0.0f, 0.0f, 5.0f);
	Zenith_GraphNode_AnimTestCountingVec3Producer::s_uPullCount = 0u;
	RunTween(xFixture.m_xNoAnimator, 999, false);
	ZENITH_ASSERT_EQ(Zenith_GraphNode_AnimTestCountingVec3Producer::s_uPullCount, 0u,
		"the easing guard must run before To is pulled");
	ZENITH_ASSERT_NOT_NULL(xFixture.m_xNoAnimator.TryGetComponent<Zenith_TweenComponent>());
	RunTween(xFixture.m_xNoAnimator, EASING_LINEAR, true);
	ZENITH_ASSERT_EQ(Zenith_GraphNode_AnimTestCountingVec3Producer::s_uPullCount, 1u);
	Zenith_TweenComponent* pxTween = xFixture.m_xNoAnimator.TryGetComponent<Zenith_TweenComponent>();
	ZENITH_ASSERT_NOT_NULL(pxTween);
	if (pxTween != nullptr) pxTween->OnUpdate(0.016f);
	Zenith_Maths::Vector3 xScale;
	xFixture.m_xNoAnimator.GetComponent<Zenith_TransformComponent>().GetScale(xScale);
	ZENITH_ASSERT_NEAR_VEC3(xScale, Zenith_Maths::Vector3(0.0f, 0.0f, 5.0f), 0.001f);
}

// An unconnected pin takes its const; an actual wire with the wrong type also takes
// that const and reports the pin mismatch. A fresh node is used for each leg because
// input state is latched after its first accessor call.
ZENITH_TEST(AnimationPinRuntime, UnconnectedOrMismatchedInputUsesConst)
{
	Zenith_AnimationPinFixture xFixture("TestAnimPinAbsentVarScene");
	if (!xFixture.m_bReady)
	{
		return;
	}
	const u_int uValue = Zenith_GraphNode_SetAnimatorFloat::uPIN_Value;

	Zenith_GraphBlackboard xBB;

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	xCtx.m_xSelf = xFixture.m_xAnimated;

	// (a) the variable does not exist -> the const.
	{
		Zenith_GraphNode_SetAnimatorFloat xNode;
		xNode.m_strParameter = "Speed";
		xNode.m_fValue = 3.0f;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ_FLOAT(xFixture.Parameters().GetFloat("Speed"), 3.0f, 0.0001f);
	}

	// (b) an actual INT32 wired into Value must reject the wire, take the const,
	//     and report one mismatch. The parameter is moved off 3 first.
	{
		xFixture.Parameters().SetFloat("Speed", -1.0f);
		Zenith_GraphNode_SetAnimatorFloat xNode;
		xNode.m_strParameter = "Speed";
		xNode.m_fValue = 3.0f;
		xNode.SetInputForTest(uValue, AnimPin_WireInt(42));
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ_FLOAT(xFixture.Parameters().GetFloat("Speed"), 3.0f, 0.0001f,
			"an INT32 wire must not be loaded into a FLOAT pin");
		ZENITH_ASSERT_EQ(xNode.GetMismatchWarningCountForTest(uValue), 1u);
	}
}

#endif // ZENITH_TESTING
