//------------------------------------------------------------------------------
// Pin-table coverage for the Input node TU. Included at the bottom of
// Zenith_GraphNode_Registration_Input.cpp (ZENITH_TESTING), where the
// anonymous-namespace node classes and this TU's registrar are still in scope.
//
// The shared machinery (the totality walk, the registrar swap and its RAII
// restore) is Zenith_GraphPinTotality.TestHarness.inl; this file carries only
// this TU's registrar and its representative pins.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"

#ifdef ZENITH_TESTING

#include "EntityComponent/Zenith_GraphPinTotality.TestHarness.inl"

ZENITH_TEST(GraphPinTable, InputTotality)
{
	Zenith_CheckPinTableTotality(&Zenith_RegisterEngineGraphNodes_Input, "_Input.cpp", nullptr, 0u);
}

ZENITH_TEST(GraphPinTable, InputRoleSpotCheck)
{
	// This TU is almost entirely OUTPUTS: a query node publishes its computed
	// value through a slot. The types are the ones the Execute bodies actually Set*.
	Zenith_CheckGraphPin("ReadKeyState", "Result", GRAPH_PIN_ROLE_OUTPUT, PROPERTY_TYPE_BOOL, "");
	Zenith_CheckGraphPin("ReadMovementAxis", "Result", GRAPH_PIN_ROLE_OUTPUT, PROPERTY_TYPE_VECTOR3, "");
	// ★ VECTOR2, not VECTOR3: the mouse/action 2D reads write SetVector2, and a
	// VECTOR3 here would make every correct graph a type error.
	Zenith_CheckGraphPin("ReadMouseDelta", "Result", GRAPH_PIN_ROLE_OUTPUT, PROPERTY_TYPE_VECTOR2, "");
	Zenith_CheckGraphPin("ReadActionAxis2D", "Result", GRAPH_PIN_ROLE_OUTPUT, PROPERTY_TYPE_VECTOR2, "");

	// ReadPointer publishes FOUR independent results through output slots.
	Zenith_CheckGraphPin("ReadPointer", "Down", GRAPH_PIN_ROLE_OUTPUT, PROPERTY_TYPE_BOOL, "");
	Zenith_CheckGraphPin("ReadPointer", "Count", GRAPH_PIN_ROLE_OUTPUT, PROPERTY_TYPE_INT32, "");

	// The one non-OUTPUT: an event SOURCE stashing its payload into a configured
	// destination (the collision-source pattern).
	Zenith_CheckGraphPin("OnMouseMoved", "StoreDelta", GRAPH_PIN_ROLE_SELECTOR_WRITE, PROPERTY_TYPE_VECTOR2, "m_strStoreDeltaVar");
}

//==============================================================================
// PIN RUNTIME for this TU (B-6.8) - the pins are LIVE.
//
// The eleven readers above now write their OUTPUT descriptors through
// Zenith_GraphNode::SetOutput. ★ THIS TU HAS NO INPUT PIN AT ALL: every key code,
// mode, sensitivity, pointer index and action NAME is device / action
// configuration, so nothing here can ever log a [GraphPin] FALLBACK census line
// and there is no input-default row to write. InputPinRuntime.NoPinIsAnInputPin is the
// mechanical statement of that, so the claim is falsifiable rather than a comment.
//
// ★ EVERY ROW ASSERTS THE EXECUTE STATUS FIRST. Two of the readers FAIL before
// they write anything, and a row that checked a slot without checking the status
// would pass just as happily on a node that did nothing at all.
//
// ★ ORDERING RULE (B-6.1): pin state is built ONCE, on the first accessor call,
// from the properties as they read THEN. Assign every property before the first
// Execute, and use a FRESH directly-constructed node per leg.
//
// ★ LIVE SINGLETONS, stated once. The device rows drive the process-wide
// Zenith_InputSimulator and Disable() + ResetAllInputState() on EVERY exit path;
// the pointer row drives g_xEngine.Pointers() and calls ResetTransientForTest() on
// every exit. The action rows register two actions on the LIVE action layer and
// never unregister: Zenith_InputActions has no Unregister and RegisterAction
// asserts on a re-registered id, so the registrar below is idempotent by design.
//
// ★ These fixtures never reach a counted census log: the per-game census parses
// `zenith test <G> --headless` runs, which pass --skip-unit-tests.
//==============================================================================

#include "Input/Zenith_InputSimulator.h"
#include "UnitTests/Zenith_TempScene.h"
#include "UnitTests/Zenith_UnitTests.h"

// One row of the index contract: the constant (or the literal, for a pin no
// Execute addresses) names the pin it is documented as, with the role the
// migration assumed.
inline void InputPin_CheckPin(const Zenith_GraphPinTable& xPins, u_int uIndex, const char* szName,
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

// The slot readers are TAG-CHECKED: Zenith_PropertyValue's typed getters
// Zenith_Assert on a mismatch, and a wrong slot type must read as a test FAILURE
// rather than a DebugBreak. VECTOR2 is new with this TU (five slots carry it) and
// there is no ZENITH_ASSERT macro for a vec2 - callers assert x and y with two
// ZENITH_ASSERT_EQ_FLOAT.
inline Zenith_Maths::Vector2 InputPin_SlotVec2(const Zenith_PropertyValue* pxSlot, const char* szWhat)
{
	ZENITH_ASSERT_NOT_NULL(pxSlot, "%s: the output slot is UNSET", szWhat);
	if (pxSlot == nullptr)
	{
		return Zenith_Maths::Vector2(0.0f);
	}
	ZENITH_ASSERT_EQ(static_cast<int>(pxSlot->GetType()), static_cast<int>(PROPERTY_TYPE_VECTOR2),
		"%s: the slot holds type %u, not VECTOR2", szWhat, static_cast<u_int>(pxSlot->GetType()));
	return pxSlot->GetType() == PROPERTY_TYPE_VECTOR2 ? pxSlot->GetVector2() : Zenith_Maths::Vector2(0.0f);
}

inline Zenith_Maths::Vector3 InputPin_SlotVec3(const Zenith_PropertyValue* pxSlot, const char* szWhat)
{
	ZENITH_ASSERT_NOT_NULL(pxSlot, "%s: the output slot is UNSET", szWhat);
	if (pxSlot == nullptr)
	{
		return Zenith_Maths::Vector3(0.0f);
	}
	ZENITH_ASSERT_EQ(static_cast<int>(pxSlot->GetType()), static_cast<int>(PROPERTY_TYPE_VECTOR3),
		"%s: the slot holds type %u, not VECTOR3", szWhat, static_cast<u_int>(pxSlot->GetType()));
	return pxSlot->GetType() == PROPERTY_TYPE_VECTOR3 ? pxSlot->GetVector3() : Zenith_Maths::Vector3(0.0f);
}

inline float InputPin_SlotFloat(const Zenith_PropertyValue* pxSlot, const char* szWhat)
{
	ZENITH_ASSERT_NOT_NULL(pxSlot, "%s: the output slot is UNSET", szWhat);
	if (pxSlot == nullptr)
	{
		return 0.0f;
	}
	ZENITH_ASSERT_EQ(static_cast<int>(pxSlot->GetType()), static_cast<int>(PROPERTY_TYPE_FLOAT),
		"%s: the slot holds type %u, not FLOAT", szWhat, static_cast<u_int>(pxSlot->GetType()));
	return pxSlot->GetType() == PROPERTY_TYPE_FLOAT ? pxSlot->GetFloat() : 0.0f;
}

inline bool InputPin_SlotBool(const Zenith_PropertyValue* pxSlot, const char* szWhat)
{
	ZENITH_ASSERT_NOT_NULL(pxSlot, "%s: the output slot is UNSET", szWhat);
	if (pxSlot == nullptr)
	{
		return false;
	}
	ZENITH_ASSERT_EQ(static_cast<int>(pxSlot->GetType()), static_cast<int>(PROPERTY_TYPE_BOOL),
		"%s: the slot holds type %u, not BOOL", szWhat, static_cast<u_int>(pxSlot->GetType()));
	return pxSlot->GetType() == PROPERTY_TYPE_BOOL ? pxSlot->GetBool() : false;
}

inline int32_t InputPin_SlotInt32(const Zenith_PropertyValue* pxSlot, const char* szWhat)
{
	ZENITH_ASSERT_NOT_NULL(pxSlot, "%s: the output slot is UNSET", szWhat);
	if (pxSlot == nullptr)
	{
		return 0;
	}
	ZENITH_ASSERT_EQ(static_cast<int>(pxSlot->GetType()), static_cast<int>(PROPERTY_TYPE_INT32),
		"%s: the slot holds type %u, not INT32", szWhat, static_cast<u_int>(pxSlot->GetType()));
	return pxSlot->GetType() == PROPERTY_TYPE_INT32 ? pxSlot->GetInt32() : 0;
}

// ★ THE ACTION FIXTURE IS PERMANENT BY DESIGN. Zenith_InputActions has NO
// Unregister and RegisterAction asserts on a re-registered id, so this is
// idempotent and is never undone. Two ids at the TOP of the table (uMAX_ACTIONS-4
// / -3) that no game's contiguous block from 16 can reach, two names nothing else
// spells, and four function keys (F19-F22 for the 2D pair, F23/F24 for the 1D
// pair) that no game and no other fixture binds - GraphComponent's own axis
// actions live at uMAX_ACTIONS-2 / -1 on F13-F18, deliberately disjoint.
namespace
{
	constexpr Zenith_InputActionID uINPUTPIN_ACTION_AXIS1D = Zenith_InputActions::uMAX_ACTIONS - 4;
	constexpr Zenith_InputActionID uINPUTPIN_ACTION_AXIS2D = Zenith_InputActions::uMAX_ACTIONS - 3;
	constexpr const char* szINPUTPIN_ACTION_AXIS1D = "__InputPinAxis1D";
	constexpr const char* szINPUTPIN_ACTION_AXIS2D = "__InputPinAxis2D";

	void InputPin_RegisterAxisActions()
	{
		Zenith_InputActions& xActions = g_xEngine.Actions();
		if (xActions.IsActionRegistered(uINPUTPIN_ACTION_AXIS1D) || xActions.IsActionRegistered(uINPUTPIN_ACTION_AXIS2D))
		{
			// Both or neither: RegisterAction asserts on a re-registered id, so a
			// half-registered pair must never fall through to it.
			return;
		}
		xActions.RegisterAction(uINPUTPIN_ACTION_AXIS1D, szINPUTPIN_ACTION_AXIS1D, INPUT_ACTION_AXIS1D);
		xActions.RegisterBinding(uINPUTPIN_ACTION_AXIS1D,
			Zenith_InputBinding::KeyAxis1D(ZENITH_KEY_F23, ZENITH_KEY_F24));
		xActions.RegisterAction(uINPUTPIN_ACTION_AXIS2D, szINPUTPIN_ACTION_AXIS2D, INPUT_ACTION_AXIS2D);
		xActions.RegisterBinding(uINPUTPIN_ACTION_AXIS2D,
			Zenith_InputBinding::KeyAxis2D(ZENITH_KEY_F19, ZENITH_KEY_F20, ZENITH_KEY_F21, ZENITH_KEY_F22));
	}

	// ONE leg of the empty-output-name row, invoked once per unguarded writer. A
	// function TEMPLATE rather than a table of callbacks because the codebase
	// forbids std::function and the nine classes share no base beyond
	// Zenith_GraphNode. The action readers need their action to RESOLVE (their
	// FAILURE precedes the write), which the requires-expression below supplies
	// without the device readers having to declare an m_strAction they do not own.
	template<typename TNode>
	void InputPin_CheckResultSlotLatches(u_int uPin, Zenith_PropertyType eType, const char* szClass,
		const char* szAction)
	{
		Zenith_GraphContext xCtx;

		TNode xNode;
		if constexpr (requires { xNode.m_strAction; })
		{
			xNode.m_strAction = szAction;
		}
		else
		{
			(void)szAction;
		}
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS),
			"%s did not reach its output write", szClass);

		const Zenith_PropertyValue* pxSlot = xNode.GetOutputForTest(uPin);
		ZENITH_ASSERT_NOT_NULL(pxSlot, "%s: the SLOT must still be latched", szClass);
		if (pxSlot != nullptr)
		{
			ZENITH_ASSERT_EQ(static_cast<int>(pxSlot->GetType()), static_cast<int>(eType),
				"%s: the latched slot carries the wrong tag", szClass);
		}
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}
}

// ★ TABLE ORDER IS THE CONTRACT. A pin INDEX is what every accessor addresses, so
// a reorder - or an inserted pin - silently re-points every uPIN_ constant in this
// TU at a different descriptor. A static_assert is impossible: the tables are
// filled at static init.
//
// NINETEEN classes are registered here. TWELVE carry a pin table: the eleven
// OUTPUT-bearing readers plus OnMouseMoved, whose one pin is a SELECTOR_WRITE and
// therefore has no uPIN_ constant (nothing addresses it - the stash stays a direct
// SetValue and always will). The remaining SEVEN carry none at all, and the only
// thing that can be asserted about them is what a base-class pointer sees: a null
// GetPinTableVirtual(), the precedent being
// BehaviourGraph.PinRuntime_OpaqueNodeStillBadAccess. They are checked on stack
// instances rather than through the live registry on purpose - the registry answers
// for whatever a game registered last, the class answers for itself.
ZENITH_TEST(GraphPinTable, InputPinIndicesMatchTables)
{
	const Zenith_GraphPinTable& xKeyState = Zenith_GraphNode_ReadKeyState::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xKeyState.GetPinCount(), 1u, "ReadKeyState gained or lost a pin");
	InputPin_CheckPin(xKeyState, Zenith_GraphNode_ReadKeyState::uPIN_Result, "Result", GRAPH_PIN_ROLE_OUTPUT,
		"ReadKeyState");

	const Zenith_GraphPinTable& xMove = Zenith_GraphNode_ReadMovementAxis::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xMove.GetPinCount(), 1u, "ReadMovementAxis gained or lost a pin");
	InputPin_CheckPin(xMove, Zenith_GraphNode_ReadMovementAxis::uPIN_Result, "Result", GRAPH_PIN_ROLE_OUTPUT,
		"ReadMovementAxis");

	const Zenith_GraphPinTable& xAxis = Zenith_GraphNode_ReadInputAxis::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xAxis.GetPinCount(), 1u, "ReadInputAxis gained or lost a pin");
	InputPin_CheckPin(xAxis, Zenith_GraphNode_ReadInputAxis::uPIN_Result, "Result", GRAPH_PIN_ROLE_OUTPUT,
		"ReadInputAxis");

	const Zenith_GraphPinTable& xMousePos = Zenith_GraphNode_ReadMousePosition::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xMousePos.GetPinCount(), 1u, "ReadMousePosition gained or lost a pin");
	InputPin_CheckPin(xMousePos, Zenith_GraphNode_ReadMousePosition::uPIN_Result, "Result", GRAPH_PIN_ROLE_OUTPUT,
		"ReadMousePosition");

	const Zenith_GraphPinTable& xMouseDelta = Zenith_GraphNode_ReadMouseDelta::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xMouseDelta.GetPinCount(), 1u, "ReadMouseDelta gained or lost a pin");
	InputPin_CheckPin(xMouseDelta, Zenith_GraphNode_ReadMouseDelta::uPIN_Result, "Result", GRAPH_PIN_ROLE_OUTPUT,
		"ReadMouseDelta");

	const Zenith_GraphPinTable& xMouseHeld = Zenith_GraphNode_ReadMouseButtonHeld::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xMouseHeld.GetPinCount(), 1u, "ReadMouseButtonHeld gained or lost a pin");
	InputPin_CheckPin(xMouseHeld, Zenith_GraphNode_ReadMouseButtonHeld::uPIN_Result, "Result", GRAPH_PIN_ROLE_OUTPUT,
		"ReadMouseButtonHeld");

	const Zenith_GraphPinTable& xWheel = Zenith_GraphNode_ReadMouseWheel::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xWheel.GetPinCount(), 1u, "ReadMouseWheel gained or lost a pin");
	InputPin_CheckPin(xWheel, Zenith_GraphNode_ReadMouseWheel::uPIN_Result, "Result", GRAPH_PIN_ROLE_OUTPUT,
		"ReadMouseWheel");

	// FOUR independent outputs in declaration order - the one class in this TU
	// where a copied uPIN_Result = 0 would have addressed three wrong pins.
	const Zenith_GraphPinTable& xPointer = Zenith_GraphNode_ReadPointer::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xPointer.GetPinCount(), 4u, "ReadPointer gained or lost a pin");
	InputPin_CheckPin(xPointer, Zenith_GraphNode_ReadPointer::uPIN_Down, "Down", GRAPH_PIN_ROLE_OUTPUT, "ReadPointer");
	InputPin_CheckPin(xPointer, Zenith_GraphNode_ReadPointer::uPIN_Position, "Position", GRAPH_PIN_ROLE_OUTPUT,
		"ReadPointer");
	InputPin_CheckPin(xPointer, Zenith_GraphNode_ReadPointer::uPIN_Tap, "Tap", GRAPH_PIN_ROLE_OUTPUT, "ReadPointer");
	InputPin_CheckPin(xPointer, Zenith_GraphNode_ReadPointer::uPIN_Count, "Count", GRAPH_PIN_ROLE_OUTPUT, "ReadPointer");

	const Zenith_GraphPinTable& xRay = Zenith_GraphNode_ReadMousePickRay::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xRay.GetPinCount(), 2u, "ReadMousePickRay gained or lost a pin");
	InputPin_CheckPin(xRay, Zenith_GraphNode_ReadMousePickRay::uPIN_Origin, "Origin", GRAPH_PIN_ROLE_OUTPUT,
		"ReadMousePickRay");
	InputPin_CheckPin(xRay, Zenith_GraphNode_ReadMousePickRay::uPIN_Direction, "Direction", GRAPH_PIN_ROLE_OUTPUT,
		"ReadMousePickRay");

	const Zenith_GraphPinTable& xAction1D = Zenith_GraphNode_ReadActionAxis1D::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xAction1D.GetPinCount(), 1u, "ReadActionAxis1D gained or lost a pin");
	InputPin_CheckPin(xAction1D, Zenith_GraphNode_ReadActionAxis1D::uPIN_Result, "Result", GRAPH_PIN_ROLE_OUTPUT,
		"ReadActionAxis1D");

	const Zenith_GraphPinTable& xAction2D = Zenith_GraphNode_ReadActionAxis2D::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xAction2D.GetPinCount(), 1u, "ReadActionAxis2D gained or lost a pin");
	InputPin_CheckPin(xAction2D, Zenith_GraphNode_ReadActionAxis2D::uPIN_Result, "Result", GRAPH_PIN_ROLE_OUTPUT,
		"ReadActionAxis2D");

	// The twelfth table, and the only SELECTOR_WRITE in the TU. It declares no
	// uPIN_ constant because nothing addresses it: an event source's stash
	// destination is a configured name, never a wire.
	const Zenith_GraphPinTable& xMoved = Zenith_GraphNode_OnMouseMoved::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xMoved.GetPinCount(), 1u, "OnMouseMoved gained or lost a pin");
	InputPin_CheckPin(xMoved, 0u, "StoreDelta", GRAPH_PIN_ROLE_SELECTOR_WRITE, "OnMouseMoved");

	// The SEVEN table-less classes. A null table is what makes them OPAQUE, and it
	// is also what stops them self-binding, so this is the assertion that would
	// notice a pin block being added to one without its Execute being migrated.
	Zenith_GraphNode_OnKeyPressed xKeyPressed;
	ZENITH_ASSERT_NULL(xKeyPressed.GetPinTableVirtual(), "OnKeyPressed gained a pin table");
	Zenith_GraphNode_OnKeyReleased xKeyReleased;
	ZENITH_ASSERT_NULL(xKeyReleased.GetPinTableVirtual(), "OnKeyReleased gained a pin table");
	Zenith_GraphNode_OnKeyHeld xKeyHeld;
	ZENITH_ASSERT_NULL(xKeyHeld.GetPinTableVirtual(), "OnKeyHeld gained a pin table");
	Zenith_GraphNode_OnMouseButton xMouseButton;
	ZENITH_ASSERT_NULL(xMouseButton.GetPinTableVirtual(), "OnMouseButton gained a pin table");
	Zenith_GraphNode_OnActionPressed xActionPressed;
	ZENITH_ASSERT_NULL(xActionPressed.GetPinTableVirtual(), "OnActionPressed gained a pin table");
	Zenith_GraphNode_OnActionReleased xActionReleased;
	ZENITH_ASSERT_NULL(xActionReleased.GetPinTableVirtual(), "OnActionReleased gained a pin table");
	Zenith_GraphNode_OnActionHeld xActionHeld;
	ZENITH_ASSERT_NULL(xActionHeld.GetPinTableVirtual(), "OnActionHeld gained a pin table");
}

// ★ This TU declares no INPUT pin. The walk makes that falsifiable: add an INPUT
// pin here tomorrow and this test fails.
ZENITH_TEST(InputPinRuntime, NoPinIsAnInputPin)
{
	const Zenith_GraphPinTable* apxTables[] =
	{
		&Zenith_GraphNode_ReadKeyState::GetPinTableStatic(),
		&Zenith_GraphNode_ReadMovementAxis::GetPinTableStatic(),
		&Zenith_GraphNode_ReadInputAxis::GetPinTableStatic(),
		&Zenith_GraphNode_ReadMousePosition::GetPinTableStatic(),
		&Zenith_GraphNode_ReadMouseDelta::GetPinTableStatic(),
		&Zenith_GraphNode_ReadMouseButtonHeld::GetPinTableStatic(),
		&Zenith_GraphNode_ReadMouseWheel::GetPinTableStatic(),
		&Zenith_GraphNode_ReadPointer::GetPinTableStatic(),
		&Zenith_GraphNode_ReadMousePickRay::GetPinTableStatic(),
		&Zenith_GraphNode_ReadActionAxis1D::GetPinTableStatic(),
		&Zenith_GraphNode_ReadActionAxis2D::GetPinTableStatic(),
		&Zenith_GraphNode_OnMouseMoved::GetPinTableStatic(),
	};
	ZENITH_ASSERT_EQ(static_cast<u_int>(sizeof(apxTables) / sizeof(apxTables[0])), 12u,
		"twelve of the nineteen registered Input classes carry a pin table");

	for (const Zenith_GraphPinTable* pxTable : apxTables)
	{
		for (u_int uPin = 0; uPin < pxTable->GetPinCount(); ++uPin)
		{
			const Zenith_GraphPinDesc& xDesc = pxTable->GetPinAt(uPin);
			ZENITH_ASSERT_NE(static_cast<int>(xDesc.m_eRole), static_cast<int>(GRAPH_PIN_ROLE_INPUT),
				"pin '%s' is an INPUT - this TU must not declare value inputs",
				xDesc.m_szName != nullptr ? xDesc.m_szName : "(null)");
		}
	}

	// And a migrated node executed once logs nothing at all.
	Zenith_GraphBlackboard xBB;
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	Zenith_GraphNode_ReadInputAxis xNode;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
}

// Every unguarded writer latches its typed output slot. The test seam observes
// the value directly.
ZENITH_TEST(InputPinRuntime, Output_TypedSlotsLatch)
{
	// The two action readers FAIL above their write unless the action resolves, so
	// their legs need the fixture registered; the seven device readers ignore it.
	InputPin_RegisterAxisActions();

	InputPin_CheckResultSlotLatches<Zenith_GraphNode_ReadKeyState>(
		Zenith_GraphNode_ReadKeyState::uPIN_Result, PROPERTY_TYPE_BOOL, "ReadKeyState", "");
	InputPin_CheckResultSlotLatches<Zenith_GraphNode_ReadMovementAxis>(
		Zenith_GraphNode_ReadMovementAxis::uPIN_Result, PROPERTY_TYPE_VECTOR3, "ReadMovementAxis", "");
	InputPin_CheckResultSlotLatches<Zenith_GraphNode_ReadInputAxis>(
		Zenith_GraphNode_ReadInputAxis::uPIN_Result, PROPERTY_TYPE_FLOAT, "ReadInputAxis", "");
	InputPin_CheckResultSlotLatches<Zenith_GraphNode_ReadMousePosition>(
		Zenith_GraphNode_ReadMousePosition::uPIN_Result, PROPERTY_TYPE_VECTOR2, "ReadMousePosition", "");
	InputPin_CheckResultSlotLatches<Zenith_GraphNode_ReadMouseDelta>(
		Zenith_GraphNode_ReadMouseDelta::uPIN_Result, PROPERTY_TYPE_VECTOR2, "ReadMouseDelta", "");
	InputPin_CheckResultSlotLatches<Zenith_GraphNode_ReadMouseButtonHeld>(
		Zenith_GraphNode_ReadMouseButtonHeld::uPIN_Result, PROPERTY_TYPE_BOOL, "ReadMouseButtonHeld", "");
	InputPin_CheckResultSlotLatches<Zenith_GraphNode_ReadMouseWheel>(
		Zenith_GraphNode_ReadMouseWheel::uPIN_Result, PROPERTY_TYPE_FLOAT, "ReadMouseWheel", "");
	InputPin_CheckResultSlotLatches<Zenith_GraphNode_ReadActionAxis1D>(
		Zenith_GraphNode_ReadActionAxis1D::uPIN_Result, PROPERTY_TYPE_FLOAT, "ReadActionAxis1D",
		szINPUTPIN_ACTION_AXIS1D);
	InputPin_CheckResultSlotLatches<Zenith_GraphNode_ReadActionAxis2D>(
		Zenith_GraphNode_ReadActionAxis2D::uPIN_Result, PROPERTY_TYPE_VECTOR2, "ReadActionAxis2D",
		szINPUTPIN_ACTION_AXIS2D);
}

// The action readers' FAILURE shape: the unresolvable-action exit is ABOVE every
// accessor, so an inert node never even self-binds - the slot reads as ABSENT
// rather than as a stamped zero. SUCCESS-then-FAILURE on ONE instance is the
// primary leg (the earlier value survives, nothing new is written); the
// fresh-instance null is the secondary, labelled one.
ZENITH_TEST(InputPinRuntime, Output_ReadActionAxisFailureBuildsNoSlots)
{
	InputPin_RegisterAxisActions();

	Zenith_GraphBlackboard xBB;
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;

	Zenith_GraphNode_ReadActionAxis1D xNode;
	xNode.m_strAction = szINPUTPIN_ACTION_AXIS1D;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	const Zenith_PropertyValue* pxSlot = xNode.GetOutputForTest(Zenith_GraphNode_ReadActionAxis1D::uPIN_Result);
	ZENITH_ASSERT_NOT_NULL(pxSlot, "a resolvable action must latch its Result slot");
	const float fFirst = InputPin_SlotFloat(pxSlot, "ReadActionAxis1D.Result (resolved)");

	// The SAME instance now names an action nothing registers. The resolve cache
	// re-resolves because the property string changed.
	xNode.m_strAction = "__NoSuchInputPinAction";
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
	ZENITH_ASSERT_EQ_FLOAT(InputPin_SlotFloat(xNode.GetOutputForTest(Zenith_GraphNode_ReadActionAxis1D::uPIN_Result),
		"ReadActionAxis1D.Result (inert)"), fFirst, 0.0001f, "the inert execute overwrote the slot");

	// SECONDARY LEG, labelled as such: a FRESH instance that only ever fails has
	// built no pin state at all, so there is no slot to read.
	{
		Zenith_GraphBlackboard xFreshBB;
		Zenith_GraphContext xFreshCtx;
		xFreshCtx.m_pxBlackboard = &xFreshBB;
		Zenith_GraphNode_ReadActionAxis2D xFresh;
		xFresh.m_strAction = "__NoSuchInputPinAction2D";
		ZENITH_ASSERT_EQ(static_cast<int>(xFresh.Execute(xFreshCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
		ZENITH_ASSERT_NULL(xFresh.GetOutputForTest(Zenith_GraphNode_ReadActionAxis2D::uPIN_Result),
			"the FAILURE is above every accessor, so no pin state was ever built");
	}
}

// ★ ORIGIN IS THE NEAR PLANE, NOT THE CAMERA POSITION. BuildRayFromMouse
// unprojects the cursor at clip z = 0, which with GLM_FORCE_DEPTH_ZERO_TO_ONE IS
// the near plane - 0.1 m in front of the camera - so a row asserting
// "Origin == camera position" would be asserting something the node has never
// done. With pitch and yaw both zero the view axis is +/-Z and every lateral
// offset the cursor contributes lands in x and y only, so |Origin.z - camera.z| is
// the near distance EXACTLY, whatever the window size and wherever the cursor sits.
// That is the one claim here that does not depend on either.
ZENITH_TEST(InputPinRuntime, Output_ReadMousePickRayCarriesValues)
{
	// Zenith.h defines ZENITH_INPUT_SIMULATOR in every configuration. Scope this
	// live singleton so every early return restores it too.
	struct InputSimulatorScope
	{
		InputSimulatorScope()
		{
			Zenith_InputSimulator::Enable();
			Zenith_InputSimulator::ResetAllInputState();
		}
		~InputSimulatorScope()
		{
			Zenith_InputSimulator::Disable();
			Zenith_InputSimulator::ResetAllInputState();
		}
	} xInputSimulator;

	Zenith_Window* pxWindow = Zenith_Window::GetInstance();
	ZENITH_ASSERT_NOT_NULL(pxWindow, "the PickRay fixture requires a window");
	if (pxWindow == nullptr)
	{
		return;
	}
	int32_t iWindowWidth = 0;
	int32_t iWindowHeight = 0;
	pxWindow->GetSize(iWindowWidth, iWindowHeight);
	ZENITH_ASSERT_GT(iWindowWidth, 0, "the PickRay fixture requires a non-zero window width");
	ZENITH_ASSERT_GT(iWindowHeight, 0, "the PickRay fixture requires a non-zero window height");
	if (iWindowWidth <= 0 || iWindowHeight <= 0)
	{
		return;
	}
	Zenith_InputSimulator::SimulateMousePosition(iWindowWidth * 0.5, iWindowHeight * 0.5);

	Zenith_TempScene xScene("TestInputPinPickRayScene");
	Zenith_Entity xCameraEntity = xScene.CreateEntity("PinPickRayCamera");
	Zenith_CameraComponent& xCamera = xCameraEntity.AddComponent<Zenith_CameraComponent>();
	Zenith_UnitTests::SetMainCameraForTest(xScene.Data(), xCameraEntity.GetEntityID());

	// Another suite may leave a camera resolvable, so the row asserts it is OURS.
	ZENITH_ASSERT_EQ(Zenith_GetMainCameraAcrossScenes(), &xCamera,
		"the resolved main camera is not this row's - every assertion below would be about someone else's view");
	if (Zenith_GetMainCameraAcrossScenes() != &xCamera)
	{
		return;
	}

	// OFF THE ORIGIN IN ALL THREE AXES: (0,0,0) is the VECTOR3 slot's own stamped
	// zero, so a camera at the origin could not tell a latched ray from an unlatched
	// slot.
	const Zenith_Maths::Vector3 xCameraPosition(12.0f, 7.0f, -5.0f);
	xCamera.SetPosition(xCameraPosition);
	xCamera.SetPitch(0.0);
	xCamera.SetYaw(0.0);

	Zenith_GraphBlackboard xBB;
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;

	Zenith_GraphNode_ReadMousePickRay xNode;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	const Zenith_Maths::Vector3 xOrigin = InputPin_SlotVec3(
		xNode.GetOutputForTest(Zenith_GraphNode_ReadMousePickRay::uPIN_Origin), "ReadMousePickRay.Origin");
	const Zenith_Maths::Vector3 xDirection = InputPin_SlotVec3(
		xNode.GetOutputForTest(Zenith_GraphNode_ReadMousePickRay::uPIN_Direction), "ReadMousePickRay.Direction");

	// A zero-size window makes ScreenSpaceToWorldSpace return (0,0,0) for BOTH
	// unprojections, which would leave Origin at the stamped zero and Direction NaN.
	// This is where that shows up, named.
	ZENITH_ASSERT_TRUE(glm::length(xOrigin) > 5.0f,
		"Origin is at (or near) the world origin - either the slot never latched or the window reports zero size");
	ZENITH_ASSERT_EQ_FLOAT(fabsf(xOrigin.z - xCameraPosition.z), xCamera.GetNearPlane(), 0.02f,
		"Origin is not on the near plane directly ahead of the camera");
	ZENITH_ASSERT_EQ_FLOAT(glm::length(xDirection), 1.0f, 0.01f, "the pick direction is not a unit vector");

	ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
}

// The no-camera FAILURE, DETERMINISTICALLY: the cross-scene finder returns the
// ACTIVE scene's main-camera entity first and does not itself check for a
// Zenith_CameraComponent, so pointing the test seam at an entity that carries none
// makes Zenith_GetMainCameraAcrossScenes() null no matter what other scenes hold.
// Zenith_TempScene makes itself the active scene, which is what closes that.
ZENITH_TEST(InputPinRuntime, Output_ReadMousePickRayFailureBuildsNoSlots)
{
	struct InputSimulatorScope
	{
		InputSimulatorScope()
		{
			Zenith_InputSimulator::Enable();
			Zenith_InputSimulator::ResetAllInputState();
		}
		~InputSimulatorScope()
		{
			Zenith_InputSimulator::Disable();
			Zenith_InputSimulator::ResetAllInputState();
		}
	} xInputSimulator;

	Zenith_Window* pxWindow = Zenith_Window::GetInstance();
	ZENITH_ASSERT_NOT_NULL(pxWindow, "the PickRay fixture requires a window");
	if (pxWindow == nullptr)
	{
		return;
	}
	int32_t iWindowWidth = 0;
	int32_t iWindowHeight = 0;
	pxWindow->GetSize(iWindowWidth, iWindowHeight);
	ZENITH_ASSERT_GT(iWindowWidth, 0, "the PickRay fixture requires a non-zero window width");
	ZENITH_ASSERT_GT(iWindowHeight, 0, "the PickRay fixture requires a non-zero window height");
	if (iWindowWidth <= 0 || iWindowHeight <= 0)
	{
		return;
	}
	Zenith_InputSimulator::SimulateMousePosition(iWindowWidth * 0.5, iWindowHeight * 0.5);

	Zenith_TempScene xScene("TestInputPinPickRayFailScene");
	Zenith_Entity xCameraEntity = xScene.CreateEntity("PinPickRayFailCamera");
	Zenith_CameraComponent& xCamera = xCameraEntity.AddComponent<Zenith_CameraComponent>();
	xCamera.SetPosition(Zenith_Maths::Vector3(12.0f, 7.0f, -5.0f));
	xCamera.SetPitch(0.0);
	xCamera.SetYaw(0.0);
	Zenith_UnitTests::SetMainCameraForTest(xScene.Data(), xCameraEntity.GetEntityID());
	ZENITH_ASSERT_EQ(Zenith_GetMainCameraAcrossScenes(), &xCamera);
	if (Zenith_GetMainCameraAcrossScenes() != &xCamera)
	{
		return;
	}

	Zenith_GraphBlackboard xBB;
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;

	Zenith_GraphNode_ReadMousePickRay xNode;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	const Zenith_Maths::Vector3 xOrigin = InputPin_SlotVec3(
		xNode.GetOutputForTest(Zenith_GraphNode_ReadMousePickRay::uPIN_Origin), "ReadMousePickRay.Origin (resolved)");
	const Zenith_Maths::Vector3 xDirection = InputPin_SlotVec3(
		xNode.GetOutputForTest(Zenith_GraphNode_ReadMousePickRay::uPIN_Direction), "ReadMousePickRay.Direction (resolved)");

	// Take the camera away by pointing the seam at an entity with no camera
	// component. The SAME instance now fails.
	Zenith_Entity xNotACamera = xScene.CreateEntity("PinPickRayNotACamera");
	Zenith_UnitTests::SetMainCameraForTest(xScene.Data(), xNotACamera.GetEntityID());
	ZENITH_ASSERT_NULL(Zenith_GetMainCameraAcrossScenes(),
		"a main-camera entity carrying no Zenith_CameraComponent must resolve to null");

	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
	ZENITH_ASSERT_NEAR_VEC3(InputPin_SlotVec3(
		xNode.GetOutputForTest(Zenith_GraphNode_ReadMousePickRay::uPIN_Origin), "ReadMousePickRay.Origin (failed)"),
		xOrigin, 0.0001f, "the FAILURE overwrote a latched slot");
	ZENITH_ASSERT_NEAR_VEC3(InputPin_SlotVec3(
		xNode.GetOutputForTest(Zenith_GraphNode_ReadMousePickRay::uPIN_Direction), "ReadMousePickRay.Direction (failed)"),
		xDirection, 0.0001f, "the FAILURE overwrote a latched slot");

	// SECONDARY LEG, labelled as such: a FRESH instance that only ever fails built
	// no pin state, so there is no slot at all.
	{
		Zenith_GraphBlackboard xFreshBB;
		Zenith_GraphContext xFreshCtx;
		xFreshCtx.m_pxBlackboard = &xFreshBB;
		Zenith_GraphNode_ReadMousePickRay xFresh;
		ZENITH_ASSERT_EQ(static_cast<int>(xFresh.Execute(xFreshCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
		ZENITH_ASSERT_NULL(xFresh.GetOutputForTest(Zenith_GraphNode_ReadMousePickRay::uPIN_Origin));
		ZENITH_ASSERT_NULL(xFresh.GetOutputForTest(Zenith_GraphNode_ReadMousePickRay::uPIN_Direction));
	}
}

#ifdef ZENITH_INPUT_SIMULATOR

//==============================================================================
// The value rows. Every one of these drives a REAL device path
// (Zenith_InputSimulator, the engine's own pointer table, the engine's own action
// layer) rather than stubbing a reader, because that is the only way the assertion
// is about the node and not about the fixture.
//
// The pin FIGURE this unit reports is the SIMULATOR-ENABLED count from a
// Null_vs2022_Debug_Win64_True boot: ZENITH_INPUT_SIMULATOR is defined
// unconditionally in Zenith.h, so every row below is compiled and counted there.
//==============================================================================

namespace
{
	// ★ THE ORDER IS THE CONTRACT (GraphComponent.ActionNodeFamilyExecution's own
	// discipline, re-created locally rather than shared because it lives in that
	// file's anonymous namespace): BeginFrame DISCARDS pending injections, because
	// the real loop queues them from a test Step AFTER BeginFrame. Begin opens the
	// next frame, keys go in between, Close drains and finalises.
	void InputPin_BeginActionFrame() { g_xEngine.Input().BeginFrame(); }

	void InputPin_CloseActionFrame()
	{
		g_xEngine.Input().ApplySimulatorInjection();	// step 7
		g_xEngine.Actions().UpdateProfile();			// step 8
		g_xEngine.Actions().FinalizeReservedUI();		// step 10b
		g_xEngine.Actions().FinalizeGameplay();			// step 10e
	}
}

ZENITH_TEST(InputPinRuntime, Output_ReadKeyStateResultByValue)
{
	Zenith_InputSimulator::Enable();
	Zenith_InputSimulator::ResetAllInputState();

	Zenith_GraphBlackboard xBB;
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;

	// Mode 0 (held). LEFT_SHIFT is the node's own default key code.
	{
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_LEFT_SHIFT, true);
		Zenith_GraphNode_ReadKeyState xNode;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_TRUE(InputPin_SlotBool(xNode.GetOutputForTest(Zenith_GraphNode_ReadKeyState::uPIN_Result),
			"ReadKeyState.Result (held)"), "the slot did not carry the held key");
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_LEFT_SHIFT, false);
	}

	// Mode 1 (pressed this frame), a FRESH node - pin state latches once.
	{
		Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_SPACE);
		Zenith_GraphNode_ReadKeyState xNode;
		xNode.m_iKeyCode = ZENITH_KEY_SPACE;
		xNode.m_iMode = 1;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_TRUE(InputPin_SlotBool(xNode.GetOutputForTest(Zenith_GraphNode_ReadKeyState::uPIN_Result),
			"ReadKeyState.Result (pressed)"), "the slot did not carry the press edge");
	}

	Zenith_InputSimulator::ResetAllInputState();
	Zenith_InputSimulator::Disable();
}

ZENITH_TEST(InputPinRuntime, Output_ReadMovementAxisResultByValue)
{
	Zenith_InputSimulator::Enable();
	Zenith_InputSimulator::ResetAllInputState();

	// W + A: forward and left, normalised - a value with TWO non-zero components
	// that the VECTOR3 stamped zero cannot imitate.
	Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, true);
	Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_A, true);

	Zenith_GraphBlackboard xBB;
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;

	Zenith_GraphNode_ReadMovementAxis xNode;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	const Zenith_Maths::Vector3 xSlot = InputPin_SlotVec3(
		xNode.GetOutputForTest(Zenith_GraphNode_ReadMovementAxis::uPIN_Result), "ReadMovementAxis.Result");
	ZENITH_ASSERT_NEAR_VEC3(xSlot, Zenith_Maths::Vector3(-0.70710678f, 0.0f, 0.70710678f), 0.001f);

	Zenith_InputSimulator::ResetAllInputState();
	Zenith_InputSimulator::Disable();
}

ZENITH_TEST(InputPinRuntime, Output_ReadInputAxisResultByValue)
{
	Zenith_InputSimulator::Enable();
	Zenith_InputSimulator::ResetAllInputState();

	Zenith_GraphBlackboard xBB;
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;

	// +1 from the positive key alone, then -1 from the negative key alone: two
	// values, neither of them the FLOAT stamped zero.
	{
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_D, true);
		Zenith_GraphNode_ReadInputAxis xNode;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ_FLOAT(InputPin_SlotFloat(xNode.GetOutputForTest(Zenith_GraphNode_ReadInputAxis::uPIN_Result),
			"ReadInputAxis.Result (+)"), 1.0f, 0.0001f);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_D, false);
	}
	{
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_A, true);
		Zenith_GraphNode_ReadInputAxis xNode;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ_FLOAT(InputPin_SlotFloat(xNode.GetOutputForTest(Zenith_GraphNode_ReadInputAxis::uPIN_Result),
			"ReadInputAxis.Result (-)"), -1.0f, 0.0001f);
	}

	Zenith_InputSimulator::ResetAllInputState();
	Zenith_InputSimulator::Disable();
}

ZENITH_TEST(InputPinRuntime, Output_ReadMousePositionResultByValue)
{
	Zenith_InputSimulator::Enable();
	Zenith_InputSimulator::ResetAllInputState();
	Zenith_InputSimulator::SimulateMousePosition(100.0, 200.0);

	Zenith_GraphBlackboard xBB;
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;

	Zenith_GraphNode_ReadMousePosition xNode;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	const Zenith_Maths::Vector2 xSlot = InputPin_SlotVec2(
		xNode.GetOutputForTest(Zenith_GraphNode_ReadMousePosition::uPIN_Result), "ReadMousePosition.Result");
	ZENITH_ASSERT_EQ_FLOAT(xSlot.x, 100.0f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(xSlot.y, 200.0f, 0.0001f);

	Zenith_InputSimulator::ResetAllInputState();
	Zenith_InputSimulator::Disable();
}

// ★ THE DELTA IS RECOMPUTED ONLY INSIDE Zenith_Input::BeginFrame's simulator
// branch, and the FIRST simulated frame forces (0,0) because the previous position
// came from another input domain. So it takes TWO BeginFrames: one to latch the
// start position, one to difference against it. BeginTestFrame never touches the
// delta and BeginFrame discards pending injections, which is why this row stages
// positions rather than key edges.
ZENITH_TEST(InputPinRuntime, Output_ReadMouseDeltaResultByValue)
{
	Zenith_InputSimulator::Enable();
	Zenith_InputSimulator::ResetAllInputState();

	Zenith_InputSimulator::SimulateMousePosition(10.0, 20.0);
	g_xEngine.Input().BeginFrame();
	Zenith_InputSimulator::SimulateMousePosition(40.0, 60.0);
	g_xEngine.Input().BeginFrame();

	Zenith_GraphBlackboard xBB;
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;

	Zenith_GraphNode_ReadMouseDelta xNode;
	xNode.m_fSensitivity = 2.0f;		// so the row also proves the scale is applied
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	const Zenith_Maths::Vector2 xSlot = InputPin_SlotVec2(
		xNode.GetOutputForTest(Zenith_GraphNode_ReadMouseDelta::uPIN_Result), "ReadMouseDelta.Result");
	ZENITH_ASSERT_EQ_FLOAT(xSlot.x, 60.0f, 0.001f, "(40-10) * 2.0");
	ZENITH_ASSERT_EQ_FLOAT(xSlot.y, 80.0f, 0.001f, "(60-20) * 2.0");

	Zenith_InputSimulator::ResetAllInputState();
	Zenith_InputSimulator::Disable();
}

ZENITH_TEST(InputPinRuntime, Output_ReadMouseButtonHeldResultByValue)
{
	Zenith_InputSimulator::Enable();
	Zenith_InputSimulator::ResetAllInputState();

	Zenith_GraphBlackboard xBB;
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;

	// The negative control first: false is the BOOL stamped zero, so the true leg
	// below is the one that carries the value.
	{
		Zenith_GraphNode_ReadMouseButtonHeld xNode;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_FALSE(InputPin_SlotBool(xNode.GetOutputForTest(
			Zenith_GraphNode_ReadMouseButtonHeld::uPIN_Result), "ReadMouseButtonHeld.Result (up)"));
	}
	{
		Zenith_InputSimulator::SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_LEFT);
		Zenith_GraphNode_ReadMouseButtonHeld xNode;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_TRUE(InputPin_SlotBool(xNode.GetOutputForTest(
			Zenith_GraphNode_ReadMouseButtonHeld::uPIN_Result), "ReadMouseButtonHeld.Result (down)"));
	}

	Zenith_InputSimulator::ResetAllInputState();
	Zenith_InputSimulator::Disable();
}

// SimulateMouseWheel stays asserted for a full frame and GetMouseWheelDelta reads
// THROUGH the simulator on every query, so no frame stepping is needed - and
// EndTestFrame / ResetAllInputState clear it, which is why neither is called before
// the Execute.
ZENITH_TEST(InputPinRuntime, Output_ReadMouseWheelResultByValue)
{
	Zenith_InputSimulator::Enable();
	Zenith_InputSimulator::ResetAllInputState();
	Zenith_InputSimulator::SimulateMouseWheel(1.5f);

	Zenith_GraphBlackboard xBB;
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;

	Zenith_GraphNode_ReadMouseWheel xNode;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	ZENITH_ASSERT_EQ_FLOAT(InputPin_SlotFloat(xNode.GetOutputForTest(Zenith_GraphNode_ReadMouseWheel::uPIN_Result),
		"ReadMouseWheel.Result"), 1.5f, 0.0001f);

	Zenith_InputSimulator::ResetAllInputState();
	Zenith_InputSimulator::Disable();
}

ZENITH_TEST(InputPinRuntime, Output_ReadActionAxis1DResultByValue)
{
	InputPin_RegisterAxisActions();
	Zenith_InputSimulator::Enable();
	Zenith_InputSimulator::ResetAllInputState();

	// The keyboard rows only resolve while the ACTIVE profile owns the KEYBOARD
	// scheme. Windows (every gate this runs in) boots into exactly that profile; an
	// Android boot of a game with a TOUCH profile does not, and there the value
	// assertion would be about the profile system rather than about this node.
	const bool bKeyboardLive =
		(g_xEngine.Actions().GetActiveSchemeMask() & uINPUT_SCHEME_MASK_KEYBOARD) != 0;

	InputPin_BeginActionFrame();
	Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_F24);		// the POSITIVE half
	InputPin_CloseActionFrame();

	Zenith_GraphBlackboard xBB;
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;

	Zenith_GraphNode_ReadActionAxis1D xNode;
	xNode.m_strAction = szINPUTPIN_ACTION_AXIS1D;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	const float fSlot = InputPin_SlotFloat(xNode.GetOutputForTest(Zenith_GraphNode_ReadActionAxis1D::uPIN_Result),
		"ReadActionAxis1D.Result");
	if (bKeyboardLive)
	{
		ZENITH_ASSERT_EQ_FLOAT(fSlot, 1.0f, 0.0001f, "the wired-up key did not reach the axis");
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Output_ReadActionAxis1DResultByValue: the active profile does not own the KEYBOARD scheme, so the "
			"VALUE leg is not asserted; the typed output slot remains observed");
	}

	InputPin_BeginActionFrame();
	Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_F24);
	InputPin_CloseActionFrame();
	Zenith_InputSimulator::ResetAllInputState();
	Zenith_InputSimulator::Disable();
}

ZENITH_TEST(InputPinRuntime, Output_ReadActionAxis2DResultByValue)
{
	InputPin_RegisterAxisActions();
	Zenith_InputSimulator::Enable();
	Zenith_InputSimulator::ResetAllInputState();

	const bool bKeyboardLive =
		(g_xEngine.Actions().GetActiveSchemeMask() & uINPUT_SCHEME_MASK_KEYBOARD) != 0;

	InputPin_BeginActionFrame();
	Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_F19);		// FORWARD
	Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_F22);		// RIGHT
	InputPin_CloseActionFrame();

	Zenith_GraphBlackboard xBB;
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;

	Zenith_GraphNode_ReadActionAxis2D xNode;
	xNode.m_strAction = szINPUTPIN_ACTION_AXIS2D;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	const Zenith_Maths::Vector2 xSlot = InputPin_SlotVec2(
		xNode.GetOutputForTest(Zenith_GraphNode_ReadActionAxis2D::uPIN_Result), "ReadActionAxis2D.Result");
	if (bKeyboardLive)
	{
		// UNNORMALISED, +y forward: ResolveMoveComposite's contract, not a unit
		// vector - so (1,1), not (0.707, 0.707).
		ZENITH_ASSERT_EQ_FLOAT(xSlot.x, 1.0f, 0.0001f);
		ZENITH_ASSERT_EQ_FLOAT(xSlot.y, 1.0f, 0.0001f);
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Output_ReadActionAxis2DResultByValue: the active profile does not own the KEYBOARD scheme, so the "
			"VALUE leg is not asserted; the typed output slot remains observed");
	}

	InputPin_BeginActionFrame();
	Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_F19);
	Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_F22);
	InputPin_CloseActionFrame();
	Zenith_InputSimulator::ResetAllInputState();
	Zenith_InputSimulator::Disable();
}

// ★ THE POINTER TABLE HAS PUBLIC WINDOW-FREE CORES, so this needs no window, no
// touchscreen and no frame stepping: BeginFrame + ApplyEvent ARE what the engine's
// own step 4 calls, and ResetTransientForTest is the teardown the shared instance
// needs on EVERY exit.
//
// Each pointer result latches through its output slot.
ZENITH_TEST(InputPinRuntime, Output_ReadPointerCarriesValues)
{
	Zenith_Pointers& xPointers = g_xEngine.Pointers();
	xPointers.ResetTransientForTest();
	xPointers.BeginFrame(1.0f);

	// A finger DOWN at a position with two distinct non-zero components, so the
	// VECTOR2 stamped zero cannot imitate it.
	Zenith_InputEvent xDown;
	xDown.m_eType = INPUT_EVENT_TOUCH_DOWN;
	xDown.m_iCode = 1;					// platform pointer id; the slot it lands in is 0
	xDown.m_fX = 321.0f;
	xDown.m_fY = 247.0f;
	xDown.m_fTimestamp = 100.0;
	xPointers.ApplyEvent(xDown);

	Zenith_GraphBlackboard xBB;
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;

	// LEG A: the pointer is down.
	{
		Zenith_GraphNode_ReadPointer xNode;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_TRUE(InputPin_SlotBool(xNode.GetOutputForTest(Zenith_GraphNode_ReadPointer::uPIN_Down),
			"ReadPointer.Down (down)"), "the pointer slot did not read as down");
		const Zenith_Maths::Vector2 xPosition = InputPin_SlotVec2(
			xNode.GetOutputForTest(Zenith_GraphNode_ReadPointer::uPIN_Position), "ReadPointer.Position");
		ZENITH_ASSERT_EQ_FLOAT(xPosition.x, 321.0f, 0.0001f);
		ZENITH_ASSERT_EQ_FLOAT(xPosition.y, 247.0f, 0.0001f);
		ZENITH_ASSERT_FALSE(InputPin_SlotBool(xNode.GetOutputForTest(Zenith_GraphNode_ReadPointer::uPIN_Tap),
			"ReadPointer.Tap (down)"), "a finger still on the glass has not tapped");
		ZENITH_ASSERT_EQ(InputPin_SlotInt32(xNode.GetOutputForTest(Zenith_GraphNode_ReadPointer::uPIN_Count),
			"ReadPointer.Count (down)"), 1);

	}

	// LEG B: UP inside the tap window, same frame, same position -> a TAP. The
	// pointer is no longer down and no longer counts as active.
	{
		Zenith_InputEvent xUp = xDown;
		xUp.m_eType = INPUT_EVENT_TOUCH_UP;
		xUp.m_fTimestamp = 100.05;
		xPointers.ApplyEvent(xUp);

		Zenith_GraphBlackboard xTapBB;
		Zenith_GraphContext xTapCtx;
		xTapCtx.m_pxBlackboard = &xTapBB;

		Zenith_GraphNode_ReadPointer xNode;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xTapCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_TRUE(InputPin_SlotBool(xNode.GetOutputForTest(Zenith_GraphNode_ReadPointer::uPIN_Tap),
			"ReadPointer.Tap (up)"), "a short, still gesture must report a tap");
		ZENITH_ASSERT_FALSE(InputPin_SlotBool(xNode.GetOutputForTest(Zenith_GraphNode_ReadPointer::uPIN_Down),
			"ReadPointer.Down (up)"));
		ZENITH_ASSERT_EQ(InputPin_SlotInt32(xNode.GetOutputForTest(Zenith_GraphNode_ReadPointer::uPIN_Count),
			"ReadPointer.Count (up)"), 0, "a pointer awaiting retirement is not active");
	}

	// LEG C: a slot that does not exist reads as an ABSENT pointer and still
	// SUCCEEDS - a graph polling slot 1 must not stall the frames where only one
	// finger is down.
	{
		Zenith_GraphBlackboard xBadBB;
		Zenith_GraphContext xBadCtx;
		xBadCtx.m_pxBlackboard = &xBadBB;

		Zenith_GraphNode_ReadPointer xNode;
		xNode.m_iPointerIndex = 99;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xBadCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_FALSE(InputPin_SlotBool(xNode.GetOutputForTest(Zenith_GraphNode_ReadPointer::uPIN_Down),
			"ReadPointer.Down (invalid slot)"));
		const Zenith_Maths::Vector2 xPosition = InputPin_SlotVec2(
			xNode.GetOutputForTest(Zenith_GraphNode_ReadPointer::uPIN_Position), "ReadPointer.Position (invalid slot)");
		ZENITH_ASSERT_EQ_FLOAT(xPosition.x, 0.0f, 0.0001f);
		ZENITH_ASSERT_EQ_FLOAT(xPosition.y, 0.0f, 0.0001f);
		// Count is a table-wide figure, not a slot one, so it is unaffected by the
		// index being nonsense.
		ZENITH_ASSERT_EQ(InputPin_SlotInt32(xNode.GetOutputForTest(Zenith_GraphNode_ReadPointer::uPIN_Count),
			"ReadPointer.Count (invalid slot)"), 0);
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}

	xPointers.ResetTransientForTest();
}

#endif // ZENITH_INPUT_SIMULATOR

#endif // ZENITH_TESTING
