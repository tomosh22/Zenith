//------------------------------------------------------------------------------
// Routable-FAILURE coverage for the UI node TU. Included at the bottom of
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
//     wrong-element-type gate, are Zenith_GraphComponent.Tests.inl:2749-2766.
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
		// SetUIText: FAILURE at Registration_UI.cpp:117 (ELEMENT NOT FOUND). The
		// target has a Zenith_UIComponent, so the :112 guard is cleared, and the
		// lookup misses before the text-bearing-type gate at :146.
		{
			"SetUIText",
			[](Zenith_GraphBuilder& xBuilder, u_int uNode)
			{
				xBuilder.ParamString(uNode, "m_strElement", szFAILPIN_MISSING_ELEMENT);
			}
		},
		// SetUIColor: FAILURE at Registration_UI.cpp:175 (ELEMENT NOT FOUND);
		// the :170 no-component guard is cleared by the fixture.
		{
			"SetUIColor",
			[](Zenith_GraphBuilder& xBuilder, u_int uNode)
			{
				xBuilder.ParamString(uNode, "m_strElement", szFAILPIN_MISSING_ELEMENT);
			}
		},
		// SetUIVisible: FAILURE at Registration_UI.cpp:218 (ELEMENT NOT FOUND).
		// ★ m_strElement MUST be set. Its default is "", which addresses the
		// WHOLE CANVAS and returns SUCCESS at :213 - a row that forgot this
		// would exercise the canvas branch and never reach the pin at all.
		{
			"SetUIVisible",
			[](Zenith_GraphBuilder& xBuilder, u_int uNode)
			{
				xBuilder.ParamString(uNode, "m_strElement", szFAILPIN_MISSING_ELEMENT);
			}
		},
		// SetUIFillAmount: FAILURE at Registration_UI.cpp:248 (ELEMENT NOT FOUND
		// - the same return also covers "found but not a Rect"); the :243
		// no-component guard is cleared by the fixture.
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

#endif // ZENITH_TESTING
