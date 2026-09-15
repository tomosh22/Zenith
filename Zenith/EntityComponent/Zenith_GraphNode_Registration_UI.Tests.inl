//------------------------------------------------------------------------------
// Routable-FAILURE and pin-runtime coverage for the UI node TU. Included at the bottom of
// Zenith_GraphNode_Registration_UI.cpp (ZENITH_TESTING), where the
// anonymous-namespace node classes are still in scope.
//
// ONE table-driven test per owning TU. What each row proves, why the wired
// fail-probe is the positive control, and why the anchor/probes are engine
// nodes addressed by name all live ONCE, in the shared harness:
// Zenith_GraphNodeFailurePin.TestHarness.inl. This file carries only what is
// specific to this TU - the scene fixture, the rows, and the non-opted node.
//
// Every row runs against an entity that HAS a Zenith_UIComponent with a real
// element in it, so the FAILURE reached is the name lookup missing - the
// outcome a graph might reasonably handle - and not the "no UI component at
// all" guard each node opens with.
//
// CROSS-REFERENCES, deliberately not duplicated here:
//   * the runtime SEMANTICS of the pin are the FailurePin_* tests in
//     Zenith/Scripting/Zenith_Scripting.Tests.inl;
//   * the SUCCESS paths of all four setters, and SetUIFillAmount's
//     wrong-element-type gate, are GraphComponent.UINodeFamilyExecution in
//     Zenith_GraphComponent.Tests.inl (cited by NAME: the line numbers moved
//     twice, and its "UIBad" chain is the wrong-element-type half).
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"

#ifdef ZENITH_TESTING

#include "EntityComponent/Zenith_GraphNodeFailurePin.TestHarness.inl"
#include "UnitTests/Zenith_TempScene.h"

// The one element name no canvas in this fixture has. File scope so the
// captureless row lambdas below can name it without capturing.
static const char* const szFAILPIN_MISSING_ELEMENT = "__no_ui_element_has_this_name__";

ZENITH_TEST(GraphNodeFailurePin, UIOptIns)
{
	Zenith_TempScene xTempScene("FailurePinUIScene");
	Zenith_Entity xSelf = xTempScene.CreateEntity("FailurePinUI");
	Zenith_UIComponent& xUI = xSelf.AddComponent<Zenith_UIComponent>();
	xUI.CreateText("FailPinTitle", "placeholder");
	ZENITH_ASSERT_NOT_NULL(xUI.FindElement("FailPinTitle"), "the fixture canvas must hold a real element");
	ZENITH_ASSERT_NULL(xUI.FindElement(szFAILPIN_MISSING_ELEMENT), "the missing-element name must really be missing");

	const Zenith_FailurePinCase axCases[] =
	{
		// SetUIText: FAILURE on ELEMENT NOT FOUND. The target has a
		// Zenith_UIComponent, so the no-component guard is cleared, and the lookup
		// misses before BOTH the pin read and the text-bearing-type gate below it.
		{
			"SetUIText",
			[](Zenith_GraphBuilder& xBuilder, u_int uNode)
			{
				xBuilder.ParamString(uNode, "m_strElement", szFAILPIN_MISSING_ELEMENT);
			}
		},
		// SetUIColor: FAILURE on ELEMENT NOT FOUND; the no-component guard above
		// it is cleared by the fixture.
		{
			"SetUIColor",
			[](Zenith_GraphBuilder& xBuilder, u_int uNode)
			{
				xBuilder.ParamString(uNode, "m_strElement", szFAILPIN_MISSING_ELEMENT);
			}
		},
		// SetUIVisible: FAILURE on ELEMENT NOT FOUND.
		// ★ m_strElement MUST be set. Its default is "", which addresses the
		// WHOLE CANVAS and returns SUCCESS from the branch above the lookup - a
		// row that forgot this would exercise the canvas branch and never reach
		// the pin at all.
		{
			"SetUIVisible",
			[](Zenith_GraphBuilder& xBuilder, u_int uNode)
			{
				xBuilder.ParamString(uNode, "m_strElement", szFAILPIN_MISSING_ELEMENT);
			}
		},
		// SetUIFillAmount: FAILURE on ELEMENT NOT FOUND (the same return also
		// covers "found but not a Rect"); the no-component guard above it is
		// cleared by the fixture.
		{
			"SetUIFillAmount",
			[](Zenith_GraphBuilder& xBuilder, u_int uNode)
			{
				xBuilder.ParamString(uNode, "m_strElement", szFAILPIN_MISSING_ELEMENT);
			}
		},
	};

	Zenith_CheckFailurePinTable(axCases, static_cast<u_int>(sizeof(axCases) / sizeof(axCases[0])), xSelf);

	// OnUIButtonClicked is an ON_UPDATE event SOURCE - its FAILURE is the
	// per-tick "no click this tick" gate, which the opt-in rule excludes twice
	// over (registration refuses the flag on a source outright).
	Zenith_CheckNodeIsNotOptedIn("OnUIButtonClicked");
}

//------------------------------------------------------------------------------
// Pin-table coverage (A-6). The shared machinery - the totality walk, the
// registrar swap and its RAII restore - is Zenith_GraphPinTotality.TestHarness.inl.
//------------------------------------------------------------------------------

#include "EntityComponent/Zenith_GraphPinTotality.TestHarness.inl"

ZENITH_TEST(GraphPinTable, UITotality)
{
	Zenith_CheckPinTableTotality(&Zenith_RegisterEngineGraphNodes_UI, "_UI.cpp", nullptr, 0u);
}

ZENITH_TEST(GraphPinTable, UIRoleSpotCheck)
{
	// Every node in this TU targets an entity through ResolveTargetUI, which
	// wraps xContext.ResolveTargetEntity - so every m_strTargetVar is a
	// TARGET_REF accepting a packed ENTITY_ID and nothing else.
	Zenith_CheckGraphPin("SetUIText", "Target", GRAPH_PIN_ROLE_TARGET_REF, eGRAPH_PIN_TYPE_ANY, "m_strTargetVar");
	const Zenith_GraphPinDesc* pxTarget = Zenith_FindGraphPin("SetUIText", "Target");
	ZENITH_ASSERT_NOT_NULL(pxTarget);
	if (pxTarget != nullptr)
	{
		ZENITH_ASSERT_EQ(pxTarget->m_uAcceptedTypeMask, uGRAPH_PIN_ACCEPT_TARGET_ENTITY,
			"a STRING entity name is never legal at runtime");
	}
	// OnUIButtonClicked reaches ResolveTargetEntity directly rather than through
	// the wrapper; same pin either way.
	Zenith_CheckGraphPin("OnUIButtonClicked", "Target", GRAPH_PIN_ROLE_TARGET_REF, eGRAPH_PIN_TYPE_ANY, "m_strTargetVar");

	// ★ ANY: SetUIText fetches the value with the presence-aware TryGetInput and
	// type-dispatches it into a display string, so every property type is legal
	// and no tag is ever checked.
	Zenith_CheckGraphPin("SetUIText", "Value", GRAPH_PIN_ROLE_INPUT, eGRAPH_PIN_TYPE_ANY, "");

	// The two const-or-var pairs: the var half is read only when it is named,
	// otherwise the inline constant is used.
	Zenith_CheckGraphPin("SetUIColor", "Color", GRAPH_PIN_ROLE_INPUT, PROPERTY_TYPE_VECTOR4, "");
	Zenith_CheckGraphPin("SetUIFillAmount", "Amount", GRAPH_PIN_ROLE_INPUT, PROPERTY_TYPE_FLOAT, "");
	const Zenith_GraphPinDesc* pxAmount = Zenith_FindGraphPin("SetUIFillAmount", "Amount");
	ZENITH_ASSERT_NOT_NULL(pxAmount);
	if (pxAmount != nullptr)
	{
		ZENITH_ASSERT_STREQ(pxAmount->m_szConstProperty, "m_fAmount",
			"the Amount pin lost its inline-constant half");
	}
}

//==============================================================================
// PIN RUNTIME for this TU (B-6.6) - the pins are LIVE.
//
// The three data-bearing nodes read their INPUT descriptors through the pin
// runtime: SetUIColor.Color and SetUIFillAmount.Amount through GetInput,
// SetUIText.Value through the presence-aware TryGetInput. There are NO OUTPUT
// pins in this TU, so there is no SetOutput row and no `""` divergence row -
// B-6.1's deliberate divergence has no site here at all.
//
// The proof that an UNCONNECTED node is unchanged is the three tests ABOVE plus
// GraphComponent.UINodeFamilyExecution, which drives SetUIText with
// m_strValueVar = "score" (staged INT32 42 -> "Score: 42" - the var-name
// fallback path, one census line, expected), SetUIColor / SetUIFillAmount with
// CONSTS only, the "UIBad" wrong-element-type chain and the button trampoline.
// It must stay green UNCHANGED. The rows below are the proof that a WIRE now
// carries a value.
//
// ★ EVERY ROW ASSERTS THE EXECUTE STATUS FIRST. Every node here opens with
// ResolveTargetUI, which fails before any accessor runs - a row that checked an
// element's text without checking the status would pass just as happily on a
// node that did nothing at all.
//
// ★ NODES ARE CONSTRUCTED DIRECTLY ON THE STACK with m_strTargetVar = "" and
// xContext.m_xSelf pointing at a Zenith_TempScene entity. That is legal - and
// exercises this TU's real classes - because the accessors SELF-BIND (B-6.1).
//
// ★ ORDERING RULE: pin state is built ONCE, on the first accessor call, from the
// properties as they read THEN - including the var NAME. Every multi-leg row
// below therefore uses a FRESH directly-constructed node per leg (there is no
// ClearInputForTest, and a test override is consulted before the default path,
// and the per-(instance, pin) counters persist).
//
// ★ NO LEG MAY EQUAL A DEFAULT. Zenith_UIElement::m_xColor defaults to
// (1,1,1,1), SetUIColor.m_xColor to (1,1,1,1), a button's normal fill to a dark
// grey, Zenith_UIRect::m_fFillAmount to 1.0 and SetUIFillAmount.m_fAmount to 1.0
// - a leg equal to any of those passes for a node that never ran. The colour
// legs differ in all FOUR components (alpha included) and the fill legs start
// from an explicitly pre-set 0.05. SetFillAmount also CLAMPS to [0,1], so no leg
// is ever out of range.
//
// ★ These fixtures never reach a counted census log: the per-game census parses
// `zenith test <G> --headless` runs, which pass --skip-unit-tests.
//==============================================================================

// One row of the index contract: the constant (or the literal, for a pin no
// Execute addresses) names the pin it is documented as, with the role the
// migration assumed.
inline void UIPin_Check(const Zenith_GraphPinTable& xPins, u_int uIndex, const char* szName,
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

inline void UIPin_SeedInt(Zenith_GraphBlackboard& xBB, const char* szName, int32_t iValue)
{
	Zenith_PropertyValue xValue;
	xValue.SetInt32(iValue);
	xBB.SetValue(szName, xValue);
}

inline void UIPin_SeedFloat(Zenith_GraphBlackboard& xBB, const char* szName, float fValue)
{
	Zenith_PropertyValue xValue;
	xValue.SetFloat(fValue);
	xBB.SetValue(szName, xValue);
}

inline void UIPin_SeedVec4(Zenith_GraphBlackboard& xBB, const char* szName, const Zenith_Maths::Vector4& xVec)
{
	Zenith_PropertyValue xValue;
	xValue.SetVector4(xVec);
	xBB.SetValue(szName, xValue);
}

inline Zenith_PropertyValue UIPin_WireInt(int32_t iValue)
{
	Zenith_PropertyValue xValue;
	xValue.SetInt32(iValue);
	return xValue;
}

inline Zenith_PropertyValue UIPin_WireFloat(float fValue)
{
	Zenith_PropertyValue xValue;
	xValue.SetFloat(fValue);
	return xValue;
}

inline Zenith_PropertyValue UIPin_WireString(const char* szValue)
{
	Zenith_PropertyValue xValue;
	xValue.SetString(szValue);
	return xValue;
}

inline Zenith_PropertyValue UIPin_WireVec3(const Zenith_Maths::Vector3& xVec)
{
	Zenith_PropertyValue xValue;
	xValue.SetVector3(xVec);
	return xValue;
}

inline Zenith_PropertyValue UIPin_WireVec4(const Zenith_Maths::Vector4& xVec)
{
	Zenith_PropertyValue xValue;
	xValue.SetVector4(xVec);
	return xValue;
}

class Zenith_GraphNode_UITestCountingStringProducer : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_UITestCountingStringProducer)
public:
	static constexpr u_int uPIN_Value = 0u;
	ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_UITestCountingStringProducer)
	ZENITH_GRAPH_PIN_OUTPUT(Value, PROPERTY_TYPE_STRING)
	ZENITH_GRAPH_PINS_END

public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		++s_uPullCount;
		SetOutput<std::string>(xContext, uPIN_Value, "wired");
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "Test_UICountingStringProducer"; }
	inline static u_int s_uPullCount = 0u;
};

class Zenith_GraphNode_UITestCountingFloatProducer : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_UITestCountingFloatProducer)
public:
	static constexpr u_int uPIN_Value = 0u;
	ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_UITestCountingFloatProducer)
	ZENITH_GRAPH_PIN_OUTPUT(Value, PROPERTY_TYPE_FLOAT)
	ZENITH_GRAPH_PINS_END

public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override { ++s_uPullCount; SetOutput<float>(xContext, uPIN_Value, s_fValue); return GRAPH_NODE_STATUS_SUCCESS; }
	const char* GetTypeName() const override { return "Test_UICountingFloatProducer"; }
	inline static u_int s_uPullCount = 0u;
	inline static float s_fValue = 0.0f;
};

class Zenith_GraphNode_UITestCountingColorProducer : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_UITestCountingColorProducer)
public:
	static constexpr u_int uPIN_Value = 0u;
	ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_UITestCountingColorProducer)
	ZENITH_GRAPH_PIN_OUTPUT(Value, PROPERTY_TYPE_VECTOR4)
	ZENITH_GRAPH_PINS_END

public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override { ++s_uPullCount; SetOutput<Zenith_Maths::Vector4>(xContext, uPIN_Value, s_xValue); return GRAPH_NODE_STATUS_SUCCESS; }
	const char* GetTypeName() const override { return "Test_UICountingColorProducer"; }
	inline static u_int s_uPullCount = 0u;
	inline static Zenith_Maths::Vector4 s_xValue = Zenith_Maths::Vector4(0.0f);
};

static void EnsureUICountingStringProducerRegistered()
{
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
	xRegistry.EnsureInitialized();
	if (xRegistry.Find("Test_UICountingStringProducer") == nullptr)
	{
		xRegistry.RegisterNodeType<Zenith_GraphNode_UITestCountingStringProducer>(
			"Test_UICountingStringProducer", GRAPH_EVENT_NONE, 1, false, "Test", false, true);
	}
}

static void EnsureUICountingGuardProducersRegistered()
{
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
	xRegistry.EnsureInitialized();
	if (xRegistry.Find("Test_UICountingFloatProducer") == nullptr)
		xRegistry.RegisterNodeType<Zenith_GraphNode_UITestCountingFloatProducer>("Test_UICountingFloatProducer", GRAPH_EVENT_NONE, 1, false, "Test", false, true);
	if (xRegistry.Find("Test_UICountingColorProducer") == nullptr)
		xRegistry.RegisterNodeType<Zenith_GraphNode_UITestCountingColorProducer>("Test_UICountingColorProducer", GRAPH_EVENT_NONE, 1, false, "Test", false, true);
}

// All four components, because no VEC4 assertion macro exists and a
// ZENITH_ASSERT_NEAR_VEC3 would silently drop alpha - which is exactly the
// component a "did the wire win" question needs.
inline void UIPin_CheckColor(const Zenith_Maths::Vector4& xActual, const Zenith_Maths::Vector4& xExpected,
	const char* szWhat)
{
	ZENITH_ASSERT_EQ_FLOAT(xActual.x, xExpected.x, 0.001f, "%s: red", szWhat);
	ZENITH_ASSERT_EQ_FLOAT(xActual.y, xExpected.y, 0.001f, "%s: green", szWhat);
	ZENITH_ASSERT_EQ_FLOAT(xActual.z, xExpected.z, 0.001f, "%s: blue", szWhat);
	ZENITH_ASSERT_EQ_FLOAT(xActual.w, xExpected.w, 0.001f, "%s: ALPHA - a VEC3 comparison would not have seen this",
		szWhat);
}

namespace
{
	// GraphComponent.UINodeFamilyExecution's canvas recipe, verbatim: a Text, a
	// Rect and a Button on one entity, plus a SECOND entity with NO
	// Zenith_UIComponent at all - the no-canvas FAILURE leg's target. (UIOptIns'
	// own canvas holds only a Text, which is why the rows below build their own.)
	struct Zenith_UIPinFixture
	{
		explicit Zenith_UIPinFixture(const char* szSceneName)
			: m_xScene(szSceneName)
		{
			m_xEntity = m_xScene.CreateEntity("UIPinCanvas");
			Zenith_UIComponent& xUI = m_xEntity.AddComponent<Zenith_UIComponent>();
			m_pxTitle = xUI.CreateText("Title", "placeholder");
			m_pxBar = xUI.CreateRect("Bar");
			m_pxPlay = xUI.CreateButton("Play", "Play");
			ZENITH_ASSERT_NOT_NULL(m_pxTitle, "no Text element - every row below would be vacuous");
			ZENITH_ASSERT_NOT_NULL(m_pxBar, "no Rect element - the fill rows would be vacuous");
			ZENITH_ASSERT_NOT_NULL(m_pxPlay, "no Button element - the button colour leg would be vacuous");
			m_bReady = m_pxTitle != nullptr && m_pxBar != nullptr && m_pxPlay != nullptr;

			m_xNoCanvas = m_xScene.CreateEntity("UIPinNoCanvas");
		}

		Zenith_TempScene m_xScene;
		Zenith_Entity m_xEntity;
		Zenith_Entity m_xNoCanvas;
		Zenith_UI::Zenith_UIText* m_pxTitle = nullptr;
		Zenith_UI::Zenith_UIRect* m_pxBar = nullptr;
		Zenith_UI::Zenith_UIButton* m_pxPlay = nullptr;
		bool m_bReady = false;
	};
}

// ★ TABLE ORDER IS THE CONTRACT. A pin INDEX is what every accessor addresses, so
// a reorder - or an inserted pin - silently re-points every uPIN_ constant in this
// TU at a different descriptor. All FIVE classes are asserted, including the two
// whose Execute addresses no pin: asserting their count and index 0 is what would
// notice a value pin being ADDED to one without its Execute being migrated. A
// static_assert is impossible: the tables are filled at static init.
ZENITH_TEST(GraphPinTable, UIPinIndicesMatchTables)
{
	const Zenith_GraphPinTable& xText = Zenith_GraphNode_SetUIText::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xText.GetPinCount(), 2u, "SetUIText gained or lost a pin");
	UIPin_Check(xText, Zenith_GraphNode_SetUIText::uPIN_Value, "Value", GRAPH_PIN_ROLE_INPUT, "SetUIText");
	UIPin_Check(xText, 1u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "SetUIText");

	const Zenith_GraphPinTable& xColor = Zenith_GraphNode_SetUIColor::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xColor.GetPinCount(), 2u, "SetUIColor gained or lost a pin");
	UIPin_Check(xColor, Zenith_GraphNode_SetUIColor::uPIN_Color, "Color", GRAPH_PIN_ROLE_INPUT, "SetUIColor");
	UIPin_Check(xColor, 1u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "SetUIColor");

	const Zenith_GraphPinTable& xFill = Zenith_GraphNode_SetUIFillAmount::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xFill.GetPinCount(), 2u, "SetUIFillAmount gained or lost a pin");
	UIPin_Check(xFill, Zenith_GraphNode_SetUIFillAmount::uPIN_Amount, "Amount", GRAPH_PIN_ROLE_INPUT,
		"SetUIFillAmount");
	UIPin_Check(xFill, 1u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "SetUIFillAmount");

	// The two classes whose Execute addresses NO pin declare no uPIN_ constant at
	// all - their only pin is a target reference, resolved directly.
	const Zenith_GraphPinTable& xVisible = Zenith_GraphNode_SetUIVisible::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xVisible.GetPinCount(), 1u, "SetUIVisible gained a pin; nothing in its Execute addresses one");
	UIPin_Check(xVisible, 0u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "SetUIVisible");

	const Zenith_GraphPinTable& xClicked = Zenith_GraphNode_OnUIButtonClicked::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xClicked.GetPinCount(), 1u, "OnUIButtonClicked gained a pin");
	UIPin_Check(xClicked, 0u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "OnUIButtonClicked");
}

// Three DISTINCT colours, all four components apart: const (0.9,0.1,0.2,1), var
// (0.7,0.3,0.4,1), wire (0.5,0.6,0.7,0.8). Only a live wire produces the third.
// Two legs, fresh node each - a Text element (Zenith_UIElement::SetColor) and a
// Button (SetNormalColor, the branch that exists because buttons render per-state
// styles and ignore the base colour).
ZENITH_TEST(UIPinRuntime, Wired_SetUIColorFromWire)
{
	Zenith_UIPinFixture xFixture("TestUIPinColorScene");
	if (!xFixture.m_bReady)
	{
		return;
	}
	const u_int uColor = Zenith_GraphNode_SetUIColor::uPIN_Color;
	const Zenith_Maths::Vector4 xConst(0.9f, 0.1f, 0.2f, 1.0f);
	const Zenith_Maths::Vector4 xVar(0.7f, 0.3f, 0.4f, 1.0f);
	const Zenith_Maths::Vector4 xWire(0.5f, 0.6f, 0.7f, 0.8f);

	Zenith_GraphBlackboard xBB;
	UIPin_SeedVec4(xBB, "col", xVar);

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	xCtx.m_xSelf = xFixture.m_xEntity;

	// (a) the TEXT element: the base colour.
	{
		Zenith_GraphNode_SetUIColor xNode;
		xNode.m_strElement = "Title";
		xNode.m_xColor = xConst;
		xNode.SetInputForTest(uColor, UIPin_WireVec4(xWire));
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		UIPin_CheckColor(xFixture.m_pxTitle->GetColor(), xWire, "Title colour");
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}

	// (b) the BUTTON: SetNormalColor, read back off the normal STYLE specifically.
	{
		Zenith_GraphNode_SetUIColor xNode;
		xNode.m_strElement = "Play";
		xNode.m_xColor = xConst;
		xNode.SetInputForTest(uColor, UIPin_WireVec4(xWire));
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		UIPin_CheckColor(xFixture.m_pxPlay->GetNormalColor(), xWire, "Play normal colour");
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}
}

// 0.9 / 0.7 / 0.5 - three values inside the clamp, none of them the 1.0 that
// BOTH m_fFillAmount and m_fAmount default to. The bar is pre-set to 0.05 so
// "the node never ran" is distinguishable from every leg.
ZENITH_TEST(UIPinRuntime, Wired_SetUIFillAmountFromWire)
{
	Zenith_UIPinFixture xFixture("TestUIPinFillScene");
	if (!xFixture.m_bReady)
	{
		return;
	}
	const u_int uAmount = Zenith_GraphNode_SetUIFillAmount::uPIN_Amount;
	xFixture.m_pxBar->SetFillAmount(0.05f);

	Zenith_GraphBlackboard xBB;
	UIPin_SeedFloat(xBB, "amt", 0.7f);

	Zenith_GraphNode_SetUIFillAmount xNode;
	xNode.m_strElement = "Bar";
	xNode.m_fAmount = 0.9f;
	xNode.SetInputForTest(uAmount, UIPin_WireFloat(0.5f));

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	xCtx.m_xSelf = xFixture.m_xEntity;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	ZENITH_ASSERT_EQ_FLOAT(xFixture.m_pxBar->GetFillAmount(), 0.5f, 0.001f,
		"0.9 = the const, 0.7 = the blackboard, 0.05 = never ran; only the WIRE gives 0.5");
	ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
}

// SetUIText.Value is a PLAIN ANY input - no const half at all - so the discipline
// is var-vs-wire rather than 9/7/5. An ANY pin never checks tags:
// PropertyValueToDisplayString switches on the value's own type, which is why the
// STRING and VECTOR3 legs must leave the mismatch counter at zero.
ZENITH_TEST(UIPinRuntime, Wired_SetUITextValueFromWire)
{
	Zenith_UIPinFixture xFixture("TestUIPinTextWireScene");
	if (!xFixture.m_bReady)
	{
		return;
	}
	const u_int uValue = Zenith_GraphNode_SetUIText::uPIN_Value;

	Zenith_GraphBlackboard xBB;
	UIPin_SeedInt(xBB, "sc", 7);	// the var leg, which the wire must beat

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	xCtx.m_xSelf = xFixture.m_xEntity;

	// (a) INT32: var holds 7, wire carries 5.
	{
		Zenith_GraphNode_SetUIText xNode;
		xNode.m_strElement = "Title";
		xNode.m_strText = "Score: {}";
		xNode.SetInputForTest(uValue, UIPin_WireInt(5));
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_STREQ(xFixture.m_pxTitle->GetText().c_str(), "Score: 5");
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}

	// (b) STRING through the same ANY pin.
	{
		Zenith_GraphNode_SetUIText xNode;
		xNode.m_strElement = "Title";
		xNode.m_strText = "Score: {}";
		xNode.SetInputForTest(uValue, UIPin_WireString("hello"));
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_STREQ(xFixture.m_pxTitle->GetText().c_str(), "Score: hello");
		ZENITH_ASSERT_EQ(xNode.GetMismatchWarningCountForTest(uValue), 0u,
			"an ANY pin has no expected tag, so a STRING on a pin whose var holds an INT32 is not a mismatch");
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}

	// (c) VECTOR3, formatted by the type-dispatch and nothing else.
	{
		Zenith_GraphNode_SetUIText xNode;
		xNode.m_strElement = "Title";
		xNode.m_strText = "Score: {}";
		xNode.SetInputForTest(uValue, UIPin_WireVec3(Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f)));
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_STREQ(xFixture.m_pxTitle->GetText().c_str(), "Score: (1, 2, 3)");
		ZENITH_ASSERT_EQ(xNode.GetMismatchWarningCountForTest(uValue), 0u);
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}

	// (d) FLOAT with m_iDecimals: the decimal count is a property of the NODE, so
	//     it applies to a wired value exactly as it applied to a blackboard one.
	{
		Zenith_GraphNode_SetUIText xNode;
		xNode.m_strElement = "Title";
		xNode.m_strText = "Score: {}";
		xNode.m_iDecimals = 2;
		xNode.SetInputForTest(uValue, UIPin_WireFloat(5.0f));
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_STREQ(xFixture.m_pxTitle->GetText().c_str(), "Score: 5.00");
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}

	// (e) THE BUTTON BRANCH: a Button is text-bearing too, and its SetText is the
	//     second of the two SUCCESS returns.
	{
		Zenith_GraphNode_SetUIText xNode;
		xNode.m_strElement = "Play";
		xNode.m_strText = "Score: {}";
		xNode.SetInputForTest(uValue, UIPin_WireInt(5));
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_STREQ(xFixture.m_pxPlay->GetText().c_str(), "Score: 5");
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}
}

// ★ THE HOISTED-PRESENCE ROW, and the most explicit one in the file: the node has
// NO var name, NO const half and the blackboard is EMPTY, so the only thing that
// can format this label is the wire. Under the old shape - the read living inside
// `if (!m_strValueVar.empty())` - this would have printed "Score: {}" and the pin
// would never have been read at all. That is why the presence check is hoisted
// above the branch; after C-1 deletes the var-name half the condition is bPresent
// alone and this row is the whole contract.
ZENITH_TEST(UIPinRuntime, Wired_SetUITextWireFormatsWithAnEmptyVarName)
{
	Zenith_UIPinFixture xFixture("TestUIPinTextHoistScene");
	if (!xFixture.m_bReady)
	{
		return;
	}
	const u_int uValue = Zenith_GraphNode_SetUIText::uPIN_Value;

	Zenith_GraphBlackboard xBB;		// deliberately EMPTY - no entry of any name

	Zenith_GraphNode_SetUIText xNode;
	xNode.m_strElement = "Title";
	xNode.m_strText = "Score: {}";
	xNode.SetInputForTest(uValue, UIPin_WireInt(5));

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	xCtx.m_xSelf = xFixture.m_xEntity;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	ZENITH_ASSERT_STREQ(xFixture.m_pxTitle->GetText().c_str(), "Score: 5");
	ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	ZENITH_ASSERT_EQ(xBB.GetCount(), 0u, "nothing here may touch the blackboard");
}

// ★ THE PERMANENT PRESENCE TABLE. A FRESH node per leg keeps the unbound,
// explicitly-present-empty, and explicitly-present-value cases separate. The
// empty-string leg is a PRESENT STRING, never a stand-in for an unset value.
ZENITH_TEST(UIPinRuntime, SetUITextThreeOutcomes)
{
	Zenith_UIPinFixture xFixture("TestUIPinTextParityScene");
	if (!xFixture.m_bReady)
	{
		return;
	}
	const u_int uValue = Zenith_GraphNode_SetUIText::uPIN_Value;

	Zenith_GraphBlackboard xBB;

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	xCtx.m_xSelf = xFixture.m_xEntity;

	// (a) UNSET: the placeholder is left LITERAL, and the
	//     unbound/unconnected path returns false WITHOUT logging - this pin has no
	//     const half, so there is no bad access either.
	{
		Zenith_GraphNode_SetUIText xNode;
		xNode.m_strElement = "Title";
		xNode.m_strText = "Score: {}";
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_STREQ(xFixture.m_pxTitle->GetText().c_str(), "Score: {}");
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u,
			"an unbound, unconnected ANY pin must not log a bad access - it falls through to 'no value'");
	}

	// (b) PRESENT EMPTY STRING: presence consumes the placeholder while retaining
	//     the exact empty display text. This is deliberately distinct from (a).
	{
		Zenith_GraphNode_SetUIText xNode;
		xNode.m_strElement = "Title";
		xNode.m_strText = "Score: {}";
		xNode.SetInputForTest(uValue, UIPin_WireString(""));
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_STREQ(xFixture.m_pxTitle->GetText().c_str(), "Score: ");
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}

	// (c) PRESENT INT32: ANY preserves the source tag and formats the value.
	{
		Zenith_GraphNode_SetUIText xNode;
		xNode.m_strElement = "Title";
		xNode.m_strText = "Score: {}";
		xNode.SetInputForTest(uValue, UIPin_WireInt(7));
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_STREQ(xFixture.m_pxTitle->GetText().c_str(), "Score: 7");
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}

	// (d) THE APPEND FORM: no "{}" in the template, so the value is appended - the
	//     "Score: " label pattern, unchanged by the migration.
	{
		Zenith_GraphNode_SetUIText xNode;
		xNode.m_strElement = "Title";
		xNode.m_strText = "Total: ";
		xNode.SetInputForTest(uValue, UIPin_WireInt(7));
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_STREQ(xFixture.m_pxTitle->GetText().c_str(), "Total: 7");
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}
}

// ★ SHAPE B, the TU's one read-before-a-FAILURE. SetUIText's "not a text-bearing
// element" return runs BELOW the hoisted read, so an instance pointed at the Rect
// has ALREADY read its Value pin (and, in a graph, already pulled that pin's
// producer) before failing. The read stays exactly where it is - moving it above
// the two guards or below this one would both be the "exactly where it is today"
// violation. Every OTHER FAILURE in this TU is Shape A: above every accessor.
ZENITH_TEST(UIPinRuntime, SetUITextReadsValueBeforeTheTextBearingGate)
{
	Zenith_UIPinFixture xFixture("TestUIPinTextGateScene");
	if (!xFixture.m_bReady)
	{
		return;
	}
	EnsureUICountingStringProducerRegistered();
	const auto RunWired = [&](const char* szElement)
	{
		Zenith_GraphDefinition xDef;
		const u_int uSource = xDef.AddNode("OnUpdate");
		const u_int uText = xDef.AddNode("SetUIText");
		const u_int uProducer = xDef.AddNode("Test_UICountingStringProducer");
		ZENITH_ASSERT_NE(uText, 0u);
		ZENITH_ASSERT_NE(uProducer, 0u);
		if (uText == 0u || uProducer == 0u) return;
		Zenith_GraphNode_SetUIText xParams;
		xParams.m_strElement = szElement;
		xParams.m_strText = "Score: {}";
		xDef.SetNodeParamsFromInstance(uText, &xParams);
		xDef.AddEdge(uSource, 0u, uText);
		ZENITH_ASSERT_TRUE(xDef.AddDataEdge(uProducer, "Value", uText, "Value"));
		Zenith_BehaviourGraph xGraph;
		ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
		ZENITH_ASSERT_EQ(xGraph.GetResolutionSkipCountForTest(), 0u);
		Zenith_GraphContext xGraphCtx;
		xGraphCtx.m_xSelf = xFixture.m_xEntity;
		xGraphCtx.m_pxGraph = &xGraph; xGraphCtx.m_pxBlackboard = &xGraph.GetBlackboard(); xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xGraphCtx);
	};
	Zenith_GraphNode_UITestCountingStringProducer::s_uPullCount = 0u;
	RunWired(szFAILPIN_MISSING_ELEMENT);
	ZENITH_ASSERT_EQ(Zenith_GraphNode_UITestCountingStringProducer::s_uPullCount, 0u,
		"the missing-element guard must run before Value is pulled");
	RunWired("Bar");
	ZENITH_ASSERT_EQ(Zenith_GraphNode_UITestCountingStringProducer::s_uPullCount, 1u,
		"the non-text gate must run after Value is pulled");
	const u_int uValue = Zenith_GraphNode_SetUIText::uPIN_Value;

	Zenith_GraphBlackboard xBB;

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	xCtx.m_xSelf = xFixture.m_xEntity;

	// The Rect is found, but bears no text: FAILURE - after the read.
	{
		Zenith_GraphNode_SetUIText xNode;
		xNode.m_strElement = "Bar";
		xNode.m_strText = "Score: {}";
		xNode.SetInputForTest(uValue, UIPin_WireInt(7));
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}

	// POSITIVE CONTROL: the identical configuration on the Text element.
	{
		Zenith_GraphNode_SetUIText xNode;
		xNode.m_strElement = "Title";
		xNode.m_strText = "Score: {}";
		xNode.SetInputForTest(uValue, UIPin_WireInt(7));
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_STREQ(xFixture.m_pxTitle->GetText().c_str(), "Score: 7");
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}
}

// ★ SHAPE A: every guard on SetUIColor and SetUIFillAmount precedes every
// accessor, so a failed instance reads no pin and, having called no accessor,
// never even self-binds. The positive legs cover unconnected current-constant
// inputs and checked mismatch paths without relying on name metadata.
ZENITH_TEST(UIPinRuntime, GuardedFailureDoesNotReadInputs)
{
	Zenith_UIPinFixture xFixture("TestUIPinGuardScene");
	if (!xFixture.m_bReady)
	{
		return;
	}
	const u_int uAmount = Zenith_GraphNode_SetUIFillAmount::uPIN_Amount;
	EnsureUICountingGuardProducersRegistered();
	const auto RunFill = [&](const char* szElement, Zenith_Entity xSelf, bool bExpectSuccess)
	{
		Zenith_GraphDefinition xDef;
		const u_int uSource = xDef.AddNode("OnUpdate");
		const u_int uFill = xDef.AddNode("SetUIFillAmount");
		const u_int uProducer = xDef.AddNode("Test_UICountingFloatProducer");
		const u_int uSentinel = xDef.AddNode("SetBlackboardBool");
		Zenith_GraphNode_SetUIFillAmount xParams;
		xParams.m_strElement = szElement;
		xDef.SetNodeParamsFromInstance(uFill, &xParams);
		xDef.AddEdge(uSource, 0u, uFill);
		xDef.AddEdge(uFill, 0u, uSentinel);
		ZENITH_ASSERT_TRUE(xDef.AddDataEdge(uProducer, "Value", uFill, "Amount"));
		Zenith_BehaviourGraph xGraph;
		ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
		ZENITH_ASSERT_EQ(xGraph.GetResolutionSkipCountForTest(), 0u);
		Zenith_GraphContext xGraphCtx;
		xGraphCtx.m_xSelf = xSelf;
		xGraphCtx.m_pxGraph = &xGraph; xGraphCtx.m_pxBlackboard = &xGraph.GetBlackboard(); xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xGraphCtx);
		ZENITH_ASSERT_EQ(xGraph.GetBlackboard().HasValue("flag"), bExpectSuccess);
	};
	const auto RunColor = [&](const char* szElement, bool bExpectSuccess)
	{
		Zenith_GraphDefinition xDef;
		const u_int uSource = xDef.AddNode("OnUpdate");
		const u_int uColorNode = xDef.AddNode("SetUIColor");
		const u_int uProducer = xDef.AddNode("Test_UICountingColorProducer");
		const u_int uSentinel = xDef.AddNode("SetBlackboardBool");
		Zenith_GraphNode_SetUIColor xParams;
		xParams.m_strElement = szElement;
		xDef.SetNodeParamsFromInstance(uColorNode, &xParams);
		xDef.AddEdge(uSource, 0u, uColorNode);
		xDef.AddEdge(uColorNode, 0u, uSentinel);
		ZENITH_ASSERT_TRUE(xDef.AddDataEdge(uProducer, "Value", uColorNode, "Color"));
		Zenith_BehaviourGraph xGraph;
		ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
		ZENITH_ASSERT_EQ(xGraph.GetResolutionSkipCountForTest(), 0u);
		Zenith_GraphContext xGraphCtx;
		xGraphCtx.m_xSelf = xFixture.m_xEntity;
		xGraphCtx.m_pxGraph = &xGraph; xGraphCtx.m_pxBlackboard = &xGraph.GetBlackboard(); xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xGraphCtx);
		ZENITH_ASSERT_EQ(xGraph.GetBlackboard().HasValue("flag"), bExpectSuccess);
	};
	Zenith_GraphNode_UITestCountingFloatProducer::s_fValue = 0.7f;
	Zenith_GraphNode_UITestCountingFloatProducer::s_uPullCount = 0u;
	RunFill("Title", xFixture.m_xEntity, false);
	ZENITH_ASSERT_EQ(Zenith_GraphNode_UITestCountingFloatProducer::s_uPullCount, 0u);
	RunFill("Bar", xFixture.m_xNoCanvas, false);
	ZENITH_ASSERT_EQ(Zenith_GraphNode_UITestCountingFloatProducer::s_uPullCount, 0u, "a missing UI component must fail before Amount is pulled");
	RunFill("Bar", xFixture.m_xEntity, true);
	ZENITH_ASSERT_EQ(Zenith_GraphNode_UITestCountingFloatProducer::s_uPullCount, 1u);
	ZENITH_ASSERT_EQ_FLOAT(xFixture.m_pxBar->GetFillAmount(), 0.7f, 0.001f);
	Zenith_GraphNode_UITestCountingColorProducer::s_xValue = Zenith_Maths::Vector4(0.2f, 0.3f, 0.4f, 1.0f);
	Zenith_GraphNode_UITestCountingColorProducer::s_uPullCount = 0u;
	RunColor(szFAILPIN_MISSING_ELEMENT, false);
	ZENITH_ASSERT_EQ(Zenith_GraphNode_UITestCountingColorProducer::s_uPullCount, 0u);
	RunColor("Title", true);
	ZENITH_ASSERT_EQ(Zenith_GraphNode_UITestCountingColorProducer::s_uPullCount, 1u);
	UIPin_CheckColor(xFixture.m_pxTitle->GetColor(), Zenith_Maths::Vector4(0.2f, 0.3f, 0.4f, 1.0f), "Title wired guard colour");

	Zenith_GraphBlackboard xBB;
	UIPin_SeedFloat(xBB, "amt", 0.7f);
	UIPin_SeedVec4(xBB, "col", Zenith_Maths::Vector4(0.7f, 0.3f, 0.4f, 1.0f));

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;

	// LEG A: the element is found but is a TEXT, not a Rect -> FAILURE above the
	// Amount read.
	{
		xCtx.m_xSelf = xFixture.m_xEntity;
		Zenith_GraphNode_SetUIFillAmount xNode;
		xNode.m_strElement = "Title";
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}

	// LEG B, THE POSITIVE CONTROL: the same configuration on the Rect reads the
	// pin and consumes the typed wire value.
	{
		xFixture.m_pxBar->SetFillAmount(0.05f);
		xCtx.m_xSelf = xFixture.m_xEntity;
		Zenith_GraphNode_SetUIFillAmount xNode;
		xNode.m_strElement = "Bar";
		xNode.SetInputForTest(uAmount, UIPin_WireFloat(0.7f));
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ_FLOAT(xFixture.m_pxBar->GetFillAmount(), 0.7f, 0.001f,
			"the typed wire must reach the fill input");
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}

	// LEG C: SetUIColor with an element name no canvas has -> FAILURE above the
	// Color read.
	{
		xCtx.m_xSelf = xFixture.m_xEntity;
		Zenith_GraphNode_SetUIColor xNode;
		xNode.m_strElement = szFAILPIN_MISSING_ELEMENT;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}

	// LEG C2: the HOISTED read stays BELOW the element-found guard - a SetUIText
	// whose element no canvas has FAILS before TryGetInput runs (Shape A). The
	// resolved counting-producer graph in the sibling row proves that it is not
	// pulled. (The Rect leg there is the Shape-B twin.)
	{
		xCtx.m_xSelf = xFixture.m_xEntity;
		Zenith_GraphNode_SetUIText xNode;
		xNode.m_strElement = szFAILPIN_MISSING_ELEMENT;
		xNode.m_strText = "Score: {}";
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}

	// LEG D: an entity with NO Zenith_UIComponent at all - the guard every node in
	// this TU opens with.
	{
		xCtx.m_xSelf = xFixture.m_xNoCanvas;
		Zenith_GraphNode_SetUIFillAmount xNode;
		xNode.m_strElement = "Bar";
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}
}

// The CENSUS observable. Only a MIGRATED node can reach the transitional var-name
// fallback, and it logs ONE line per (instance, pin) however hot the chain is -
// The permanent typed-input cases: an UNSET input and a checked wrong-type
// override both select the pin default, while a correctly typed override wins.
// A fresh node per leg keeps each resolved input state independent.
ZENITH_TEST(UIPinRuntime, UnconnectedOrMismatchedInputUsesConst)
{
	Zenith_UIPinFixture xFixture("TestUIPinVarAbsentScene");
	if (!xFixture.m_bReady)
	{
		return;
	}
	const u_int uAmount = Zenith_GraphNode_SetUIFillAmount::uPIN_Amount;

	Zenith_GraphBlackboard xBB;

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	xCtx.m_xSelf = xFixture.m_xEntity;

	// (a) UNSET: no override or wire is an actual absent input, not a typed zero.
	{
		xFixture.m_pxBar->SetFillAmount(0.05f);
		Zenith_GraphNode_SetUIFillAmount xNode;
		xNode.m_strElement = "Bar";
		xNode.m_fAmount = 0.4f;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ_FLOAT(xFixture.m_pxBar->GetFillAmount(), 0.4f, 0.001f);
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}

	// (b) WRONG TYPE: checked extraction rejects INT32 on FLOAT and retains the
	//     exact inline default.
	{
		xFixture.m_pxBar->SetFillAmount(0.05f);
		Zenith_GraphNode_SetUIFillAmount xNode;
		xNode.m_strElement = "Bar";
		xNode.m_fAmount = 0.4f;
		xNode.SetInputForTest(uAmount, UIPin_WireInt(7));
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ_FLOAT(xFixture.m_pxBar->GetFillAmount(), 0.4f, 0.001f);
		ZENITH_ASSERT_EQ(xNode.GetMismatchWarningCountForTest(uAmount), 1u);
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}

	// (c) CORRECT TYPE: FLOAT takes precedence over the inline default.
	{
		xFixture.m_pxBar->SetFillAmount(0.05f);
		Zenith_GraphNode_SetUIFillAmount xNode;
		xNode.m_strElement = "Bar";
		xNode.m_fAmount = 0.4f;
		xNode.SetInputForTest(uAmount, UIPin_WireFloat(0.7f));
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ_FLOAT(xFixture.m_pxBar->GetFillAmount(), 0.7f, 0.001f);
		ZENITH_ASSERT_EQ(xNode.GetMismatchWarningCountForTest(uAmount), 0u);
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}
}

#endif // ZENITH_TESTING
