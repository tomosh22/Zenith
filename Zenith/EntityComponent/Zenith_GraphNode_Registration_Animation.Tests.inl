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
	// INPUT_VAR_OR_CONST, once per parameter width: the pin's TYPE is the
	// animator setter's, not the property's spelling.
	Zenith_CheckGraphPin("SetAnimatorFloat", "Value", GRAPH_PIN_ROLE_INPUT, PROPERTY_TYPE_FLOAT, "m_strValueVar");
	Zenith_CheckGraphPin("SetAnimatorBool", "Value", GRAPH_PIN_ROLE_INPUT, PROPERTY_TYPE_BOOL, "m_strValueVar");
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

	// OUTPUTs: ReadAnimatorState's four results, each typed by the
	// Zenith_PropertyValue::Set* that feeds it (an empty property skips that
	// write, which no role expresses).
	Zenith_CheckGraphPin("ReadAnimatorState", "StateName", GRAPH_PIN_ROLE_OUTPUT, PROPERTY_TYPE_STRING, "m_strStateNameVar");
	Zenith_CheckGraphPin("ReadAnimatorState", "NormalizedTime", GRAPH_PIN_ROLE_OUTPUT, PROPERTY_TYPE_FLOAT, "m_strNormalizedTimeVar");
	Zenith_CheckGraphPin("ReadAnimatorState", "HasLooped", GRAPH_PIN_ROLE_OUTPUT, PROPERTY_TYPE_BOOL, "m_strHasLoopedVar");
}

#endif // ZENITH_TESTING
