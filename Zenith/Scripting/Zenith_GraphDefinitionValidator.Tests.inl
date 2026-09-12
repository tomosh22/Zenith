//------------------------------------------------------------------------------
// Pin descriptor table + graph definition validator unit tests.
// Included at the bottom of Zenith_GraphDefinitionValidator.cpp.
//
// ★ EVERY SCRATCH TYPE IS REGISTERED LAZILY, inside a test body, behind a
// ls_bRegistered latch - NEVER at static init. ST_NoGameExtensionsContract
// asserts that every live registry row is engine-derived, and a static-init
// registration would put these in the registry before that contract ever runs.
// (The pattern is Zenith_Scripting.Tests.inl's EnsureTestNodesRegistered.)
//
// ★ NO PRODUCTION NODE IS ANNOTATED BY THIS UNIT. Every pin table below belongs
// to a Test_-prefixed scratch class; annotating the real node library is a
// separate unit, and until it lands every production node is OPAQUE - which is
// exactly what Validator_OpaqueNodeProducesNothing pins.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"
#include "Scripting/Zenith_GraphBuilder.h"

#ifdef ZENITH_TESTING

namespace
{
	//--------------------------------------------------------------------------
	// Scratch node types WITH pin tables. Declared in production order:
	// the property table first, the pin table second.
	//--------------------------------------------------------------------------

	// A pin table that names a NON-STRING property as its var binding. The
	// validator must report PIN_BINDING_INVALID and skip the pin - never reach
	// the tagged getter, which asserts on a type mismatch.
	class ValTestBadBindingNode : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(ValTestBadBindingNode)
	public:
		ZENITH_PROPERTY(float, m_fNotAString, 1.0f)

		ZENITH_GRAPH_PINS_BEGIN(ValTestBadBindingNode)
		ZENITH_GRAPH_PIN_INPUT(Bad, "m_fNotAString", PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext&) override { return GRAPH_NODE_STATUS_SUCCESS; }
		const char* GetTypeName() const override { return "Test_ValBadBinding"; }
	};

	// Reads one FLOAT variable.
	class ValTestReaderNode : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(ValTestReaderNode)
	public:
		ZENITH_PROPERTY(std::string, m_strValueVar, "value")

		ZENITH_GRAPH_PINS_BEGIN(ValTestReaderNode)
		ZENITH_GRAPH_PIN_INPUT(Value, "m_strValueVar", PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext&) override { return GRAPH_NODE_STATUS_SUCCESS; }
		const char* GetTypeName() const override { return "Test_ValReader"; }
	};

	// Writes one FLOAT result.
	class ValTestWriterNode : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(ValTestWriterNode)
	public:
		ZENITH_PROPERTY(std::string, m_strResultVar, "value")

		ZENITH_GRAPH_PINS_BEGIN(ValTestWriterNode)
		ZENITH_GRAPH_PIN_OUTPUT(Result, "m_strResultVar", PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext&) override { return GRAPH_NODE_STATUS_SUCCESS; }
		const char* GetTypeName() const override { return "Test_ValWriter"; }
	};

	// Same shape, INT32 result - the type-disagreement partner.
	class ValTestIntWriterNode : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(ValTestIntWriterNode)
	public:
		ZENITH_PROPERTY(std::string, m_strResultVar, "value")

		ZENITH_GRAPH_PINS_BEGIN(ValTestIntWriterNode)
		ZENITH_GRAPH_PIN_OUTPUT(Result, "m_strResultVar", PROPERTY_TYPE_INT32)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext&) override { return GRAPH_NODE_STATUS_SUCCESS; }
		const char* GetTypeName() const override { return "Test_ValIntWriter"; }
	};

	// Writes a packed ENTITY_ID - the shape every collision source has.
	class ValTestEntityWriterNode : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(ValTestEntityWriterNode)
	public:
		ZENITH_PROPERTY(std::string, m_strResultVar, "target")

		ZENITH_GRAPH_PINS_BEGIN(ValTestEntityWriterNode)
		ZENITH_GRAPH_PIN_OUTPUT(Result, "m_strResultVar", PROPERTY_TYPE_ENTITY_ID)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext&) override { return GRAPH_NODE_STATUS_SUCCESS; }
		const char* GetTypeName() const override { return "Test_ValEntityWriter"; }
	};

	// Writes a world position.
	class ValTestVec3WriterNode : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(ValTestVec3WriterNode)
	public:
		ZENITH_PROPERTY(std::string, m_strResultVar, "target")

		ZENITH_GRAPH_PINS_BEGIN(ValTestVec3WriterNode)
		ZENITH_GRAPH_PIN_OUTPUT(Result, "m_strResultVar", PROPERTY_TYPE_VECTOR3)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext&) override { return GRAPH_NODE_STATUS_SUCCESS; }
		const char* GetTypeName() const override { return "Test_ValVec3Writer"; }
	};

	// Writes a value whose type is genuinely not knowable - the OnCustomEvent
	// payload stash shape. ANY unifies with every reader.
	class ValTestAnyWriterNode : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(ValTestAnyWriterNode)
	public:
		ZENITH_PROPERTY(std::string, m_strResultVar, "value")

		ZENITH_GRAPH_PINS_BEGIN(ValTestAnyWriterNode)
		ZENITH_GRAPH_PIN_SELECTOR_WRITE(Payload, "m_strResultVar", eGRAPH_PIN_TYPE_ANY)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext&) override { return GRAPH_NODE_STATUS_SUCCESS; }
		const char* GetTypeName() const override { return "Test_ValAnyWriter"; }
	};

	// AddBlackboardFloat-shaped: reads its variable AND writes it back in one
	// Execute, so the reference is SELECTOR_READWRITE and cannot seed itself.
	class ValTestAccumulateNode : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(ValTestAccumulateNode)
	public:
		ZENITH_PROPERTY(std::string, m_strVariable, "value")
		ZENITH_PROPERTY(float, m_fAmount, 1.0f)

		ZENITH_GRAPH_PINS_BEGIN(ValTestAccumulateNode)
		ZENITH_GRAPH_PIN_SELECTOR_READWRITE(Var, "m_strVariable", PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PIN_INPUT_CONST(Amount, "m_fAmount", PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext&) override { return GRAPH_NODE_STATUS_SUCCESS; }
		const char* GetTypeName() const override { return "Test_ValAccumulate"; }
	};

	// ResolveTargetEntity's shape: a packed ENTITY_ID and nothing else.
	class ValTestTargetEntityNode : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(ValTestTargetEntityNode)
	public:
		ZENITH_PROPERTY(std::string, m_strTargetVar, "target")

		ZENITH_GRAPH_PINS_BEGIN(ValTestTargetEntityNode)
		ZENITH_GRAPH_PIN_TARGET_ENTITY(Target, "m_strTargetVar")
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext&) override { return GRAPH_NODE_STATUS_SUCCESS; }
		const char* GetTypeName() const override { return "Test_ValTargetEntity"; }
	};

	// The polymorphic position reference: ENTITY_ID or VECTOR3.
	class ValTestTargetPositionNode : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(ValTestTargetPositionNode)
	public:
		ZENITH_PROPERTY(std::string, m_strTargetVar, "target")

		ZENITH_GRAPH_PINS_BEGIN(ValTestTargetPositionNode)
		ZENITH_GRAPH_PIN_TARGET_POSITION(Target, "m_strTargetVar")
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext&) override { return GRAPH_NODE_STATUS_SUCCESS; }
		const char* GetTypeName() const override { return "Test_ValTargetPosition"; }
	};

	// A blackboard LIST name: runtime-only, never declared.
	class ValTestListNode : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(ValTestListNode)
	public:
		ZENITH_PROPERTY(std::string, m_strListVar, "items")

		ZENITH_GRAPH_PINS_BEGIN(ValTestListNode)
		ZENITH_GRAPH_PIN_LIST(Items, "m_strListVar")
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext&) override { return GRAPH_NODE_STATUS_SUCCESS; }
		const char* GetTypeName() const override { return "Test_ValList"; }
	};

	// A const-only INPUT: a legal descriptor with NO var-name binding at all.
	class ValTestConstOnlyNode : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(ValTestConstOnlyNode)
	public:
		ZENITH_PROPERTY(float, m_fAmount, 2.0f)

		ZENITH_GRAPH_PINS_BEGIN(ValTestConstOnlyNode)
		ZENITH_GRAPH_PIN_INPUT_CONST(Amount, "m_fAmount", PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext&) override { return GRAPH_NODE_STATUS_SUCCESS; }
		const char* GetTypeName() const override { return "Test_ValConstOnly"; }
	};

	// MathBlackboardFloat's shape: write to m_strResultVar, or - when that is
	// empty - back into m_strVar (the in-place form).
	class ValTestFallbackNode : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(ValTestFallbackNode)
	public:
		ZENITH_PROPERTY(std::string, m_strVar, "value")
		ZENITH_PROPERTY(std::string, m_strResultVar, "")

		ZENITH_GRAPH_PINS_BEGIN(ValTestFallbackNode)
		ZENITH_GRAPH_PIN_OUTPUT_FALLBACK(Result, "m_strResultVar", "m_strVar", PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext&) override { return GRAPH_NODE_STATUS_SUCCESS; }
		const char* GetTypeName() const override { return "Test_ValFallback"; }
	};

	// Instance-resolved pin type: op 0 answers FLOAT, anything else DECLINES -
	// and a declined answer must produce ANY plus a warning, never a guess.
	class ValTestInstanceNode : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(ValTestInstanceNode)
	public:
		ZENITH_PROPERTY(std::string, m_strResultVar, "value")
		ZENITH_PROPERTY(int32_t, m_iOp, 0)

		ZENITH_GRAPH_PINS_BEGIN(ValTestInstanceNode)
		ZENITH_GRAPH_PIN_OUTPUT_INSTANCE(Result, "m_strResultVar")
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext&) override { return GRAPH_NODE_STATUS_SUCCESS; }
		const char* GetTypeName() const override { return "Test_ValInstance"; }

		bool GetPinType(u_int uPinIndex, Zenith_PropertyType& eOut) const override
		{
			if (uPinIndex == 0 && m_iOp == 0)
			{
				eOut = PROPERTY_TYPE_FLOAT;
				return true;
			}
			return false;
		}
	};

	// The inheritance case: the derived class declares NO block of its own and
	// must share its base's table (the collision-source family relies on this
	// for its property table).
	class ValTestPinBaseNode : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(ValTestPinBaseNode)
	public:
		ZENITH_PROPERTY(std::string, m_strStoreEntityVar, "other")

		ZENITH_GRAPH_PINS_BEGIN(ValTestPinBaseNode)
		ZENITH_GRAPH_PIN_SELECTOR_WRITE(StoreEntity, "m_strStoreEntityVar", PROPERTY_TYPE_ENTITY_ID)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext&) override { return GRAPH_NODE_STATUS_SUCCESS; }
		const char* GetTypeName() const override { return "Test_ValPinBase"; }
	};

	class ValTestPinDerivedNode : public ValTestPinBaseNode
	{
	public:
		const char* GetTypeName() const override { return "Test_ValPinDerived"; }
	};

	// Every descriptor field, once, so the macro family's round-trip is pinned.
	class ValTestDescriptorNode : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(ValTestDescriptorNode)
	public:
		ZENITH_PROPERTY(std::string, m_strValueVar, "")
		ZENITH_PROPERTY(float, m_fAmount, 0.0f)
		ZENITH_PROPERTY(std::string, m_strBothVar, "")
		ZENITH_PROPERTY(float, m_fBoth, 0.0f)
		ZENITH_PROPERTY(std::string, m_strResultVar, "")
		ZENITH_PROPERTY(std::string, m_strResultVar2, "")
		ZENITH_PROPERTY(std::string, m_strVar, "")
		ZENITH_PROPERTY(std::string, m_strInstVar, "")
		ZENITH_PROPERTY(std::string, m_strSelReadVar, "")
		ZENITH_PROPERTY(std::string, m_strSelWriteVar, "")
		ZENITH_PROPERTY(std::string, m_strSelRWVar, "")
		ZENITH_PROPERTY(std::string, m_strTargetEntityVar, "")
		ZENITH_PROPERTY(std::string, m_strTargetPosVar, "")
		ZENITH_PROPERTY(std::string, m_strListVar, "")

		ZENITH_GRAPH_PINS_BEGIN(ValTestDescriptorNode)
		ZENITH_GRAPH_PIN_INPUT(ValueIn, "m_strValueVar", PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PIN_INPUT_CONST(AmountIn, "m_fAmount", PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PIN_INPUT_VAR_OR_CONST(BothIn, "m_strBothVar", "m_fBoth", PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PIN_OUTPUT(ResultOut, "m_strResultVar", PROPERTY_TYPE_VECTOR3)
		ZENITH_GRAPH_PIN_OUTPUT_FALLBACK(FallbackOut, "m_strResultVar2", "m_strVar", PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PIN_OUTPUT_INSTANCE(InstanceOut, "m_strInstVar")
		ZENITH_GRAPH_PIN_SELECTOR_READ(SelRead, "m_strSelReadVar", PROPERTY_TYPE_STRING)
		ZENITH_GRAPH_PIN_SELECTOR_WRITE(SelWrite, "m_strSelWriteVar", PROPERTY_TYPE_ENTITY_ID)
		ZENITH_GRAPH_PIN_SELECTOR_READWRITE(SelRW, "m_strSelRWVar", PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PIN_TARGET_ENTITY(TargetEntity, "m_strTargetEntityVar")
		ZENITH_GRAPH_PIN_TARGET_POSITION(TargetPosition, "m_strTargetPosVar")
		ZENITH_GRAPH_PIN_LIST(Items, "m_strListVar")
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext&) override { return GRAPH_NODE_STATUS_SUCCESS; }
		const char* GetTypeName() const override { return "Test_ValDescriptors"; }
	};

	//--------------------------------------------------------------------------
	// Scratch node types WITHOUT pin tables (opaque / structural fixtures).
	//--------------------------------------------------------------------------

	// Has PROPERTIES but no pin table: opaque, exactly like every production
	// node until the annotation unit lands.
	class ValTestOpaqueNode : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(ValTestOpaqueNode)
	public:
		ZENITH_PROPERTY(std::string, m_strValueVar, "value")

	public:
		GraphNodeStatus Execute(Zenith_GraphContext&) override { return GRAPH_NODE_STATUS_SUCCESS; }
		const char* GetTypeName() const override { return "Test_ValOpaque"; }
	};

	class ValTestPlainNode : public Zenith_GraphNode
	{
	public:
		GraphNodeStatus Execute(Zenith_GraphContext&) override { return GRAPH_NODE_STATUS_SUCCESS; }
		const char* GetTypeName() const override { return "Test_ValPlain"; }
	};

	// Fixed-pin, non-flow, FLAGGED: the only shape the registry honours the
	// routable failure pin on, so its effective output count is 1 + 1.
	class ValTestFailurePinNode : public Zenith_GraphNode
	{
	public:
		GraphNodeStatus Execute(Zenith_GraphContext&) override { return GRAPH_NODE_STATUS_FAILURE; }
		const char* GetTypeName() const override { return "Test_ValFailurePin"; }
	};

	// Variable-pin flow node: its count only exists once params are applied.
	class ValTestDynamicPinNode : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(ValTestDynamicPinNode)
	public:
		ZENITH_PROPERTY(int32_t, m_iPinCount, 2)

		GraphNodeStatus Execute(Zenith_GraphContext&) override { return GRAPH_NODE_STATUS_SUCCESS; }
		int32_t GetDynamicExecOutputCount() const override { return m_iPinCount; }
		const char* GetTypeName() const override { return "Test_ValDynPin"; }
	};

	//--------------------------------------------------------------------------
	// Lazy registration + assertions helpers
	//--------------------------------------------------------------------------

	void EnsureValidatorTestNodesRegistered()
	{
		static bool ls_bRegistered = false;
		if (ls_bRegistered)
		{
			return;
		}
		ls_bRegistered = true;
		Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
		xRegistry.EnsureInitialized();
		xRegistry.RegisterNodeType<ValTestReaderNode>("Test_ValReader", GRAPH_EVENT_NONE, 1, false, "Test");
		xRegistry.RegisterNodeType<ValTestBadBindingNode>("Test_ValBadBinding", GRAPH_EVENT_NONE, 1, false, "Test");
		xRegistry.RegisterNodeType<ValTestWriterNode>("Test_ValWriter", GRAPH_EVENT_NONE, 1, false, "Test");
		xRegistry.RegisterNodeType<ValTestIntWriterNode>("Test_ValIntWriter", GRAPH_EVENT_NONE, 1, false, "Test");
		xRegistry.RegisterNodeType<ValTestEntityWriterNode>("Test_ValEntityWriter", GRAPH_EVENT_NONE, 1, false, "Test");
		xRegistry.RegisterNodeType<ValTestVec3WriterNode>("Test_ValVec3Writer", GRAPH_EVENT_NONE, 1, false, "Test");
		xRegistry.RegisterNodeType<ValTestAnyWriterNode>("Test_ValAnyWriter", GRAPH_EVENT_NONE, 1, false, "Test");
		xRegistry.RegisterNodeType<ValTestAccumulateNode>("Test_ValAccumulate", GRAPH_EVENT_NONE, 1, false, "Test");
		xRegistry.RegisterNodeType<ValTestTargetEntityNode>("Test_ValTargetEntity", GRAPH_EVENT_NONE, 1, false, "Test");
		xRegistry.RegisterNodeType<ValTestTargetPositionNode>("Test_ValTargetPosition", GRAPH_EVENT_NONE, 1, false, "Test");
		xRegistry.RegisterNodeType<ValTestListNode>("Test_ValList", GRAPH_EVENT_NONE, 1, false, "Test");
		xRegistry.RegisterNodeType<ValTestConstOnlyNode>("Test_ValConstOnly", GRAPH_EVENT_NONE, 1, false, "Test");
		xRegistry.RegisterNodeType<ValTestFallbackNode>("Test_ValFallback", GRAPH_EVENT_NONE, 1, false, "Test");
		xRegistry.RegisterNodeType<ValTestInstanceNode>("Test_ValInstance", GRAPH_EVENT_NONE, 1, false, "Test");
		xRegistry.RegisterNodeType<ValTestPinBaseNode>("Test_ValPinBase", GRAPH_EVENT_NONE, 1, false, "Test");
		xRegistry.RegisterNodeType<ValTestPinDerivedNode>("Test_ValPinDerived", GRAPH_EVENT_NONE, 1, false, "Test");
		xRegistry.RegisterNodeType<ValTestDescriptorNode>("Test_ValDescriptors", GRAPH_EVENT_NONE, 1, false, "Test");
		xRegistry.RegisterNodeType<ValTestOpaqueNode>("Test_ValOpaque", GRAPH_EVENT_NONE, 1, false, "Test");
		xRegistry.RegisterNodeType<ValTestPlainNode>("Test_ValPlain", GRAPH_EVENT_NONE, 1, false, "Test");
		xRegistry.RegisterNodeType<ValTestFailurePinNode>("Test_ValFailurePin", GRAPH_EVENT_NONE, 1, false, "Test", true);
		xRegistry.RegisterNodeType<ValTestDynamicPinNode>("Test_ValDynPin", GRAPH_EVENT_NONE, 1, true, "Test");
	}

	void RunValidate(const Zenith_GraphDefinition& xDefinition, bool bLatchErrors,
		Zenith_Vector<Zenith_GraphValidationFinding>& axOut)
	{
		Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
		xRegistry.EnsureInitialized();
		Zenith_GraphDefinitionValidator::Validate(xDefinition, xRegistry, "Test_Graph", bLatchErrors, axOut);
	}

	u_int CountRule(const Zenith_Vector<Zenith_GraphValidationFinding>& axFindings, Zenith_GraphValidationRule eRule)
	{
		u_int uCount = 0;
		for (u_int u = 0; u < axFindings.GetSize(); ++u)
		{
			if (axFindings.Get(u).m_eRule == eRule)
			{
				++uCount;
			}
		}
		return uCount;
	}

	const Zenith_GraphValidationFinding* FirstOfRule(const Zenith_Vector<Zenith_GraphValidationFinding>& axFindings,
		Zenith_GraphValidationRule eRule)
	{
		for (u_int u = 0; u < axFindings.GetSize(); ++u)
		{
			if (axFindings.Get(u).m_eRule == eRule)
			{
				return &axFindings.Get(u);
			}
		}
		return nullptr;
	}
}

//==============================================================================
// The declaration mechanism
//==============================================================================

// Every descriptor field survives the macro family verbatim - including the two
// that have no other test (the const-only binding, whose var-name property is
// deliberately "", and the TARGET masks, which are the corrected pairing rule).
ZENITH_TEST(GraphPinTable, PinTable_MacroDeclaresTotalTable)
{
	EnsureValidatorTestNodesRegistered();

	const Zenith_GraphPinTable& xTable = ValTestDescriptorNode::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xTable.GetPinCount(), 12u);

	const Zenith_GraphPinDesc* pxValueIn = xTable.FindPin("ValueIn");
	ZENITH_ASSERT_NOT_NULL(pxValueIn);
	if (pxValueIn == nullptr)
	{
		return;
	}
	ZENITH_ASSERT_TRUE(pxValueIn->m_eRole == GRAPH_PIN_ROLE_INPUT);
	ZENITH_ASSERT_TRUE(pxValueIn->m_eType == PROPERTY_TYPE_FLOAT);
	ZENITH_ASSERT_STREQ(pxValueIn->m_szVarNameProperty, "m_strValueVar");
	ZENITH_ASSERT_STREQ(pxValueIn->m_szConstProperty, "");
	ZENITH_ASSERT_STREQ(pxValueIn->m_szFallbackVarNameProperty, "");
	ZENITH_ASSERT_FALSE(pxValueIn->m_bInstanceResolved);

	// A const-only INPUT is a VALID descriptor whose var-name binding is "" -
	// the variable checks skip it entirely.
	const Zenith_GraphPinDesc* pxAmountIn = xTable.FindPin("AmountIn");
	ZENITH_ASSERT_NOT_NULL(pxAmountIn);
	if (pxAmountIn)
	{
		ZENITH_ASSERT_STREQ(pxAmountIn->m_szVarNameProperty, "");
		ZENITH_ASSERT_STREQ(pxAmountIn->m_szConstProperty, "m_fAmount");
	}

	const Zenith_GraphPinDesc* pxBothIn = xTable.FindPin("BothIn");
	ZENITH_ASSERT_NOT_NULL(pxBothIn);
	if (pxBothIn)
	{
		ZENITH_ASSERT_STREQ(pxBothIn->m_szVarNameProperty, "m_strBothVar");
		ZENITH_ASSERT_STREQ(pxBothIn->m_szConstProperty, "m_fBoth");
	}

	const Zenith_GraphPinDesc* pxResultOut = xTable.FindPin("ResultOut");
	ZENITH_ASSERT_NOT_NULL(pxResultOut);
	if (pxResultOut)
	{
		ZENITH_ASSERT_TRUE(pxResultOut->m_eRole == GRAPH_PIN_ROLE_OUTPUT);
		ZENITH_ASSERT_TRUE(pxResultOut->m_eType == PROPERTY_TYPE_VECTOR3);
	}

	const Zenith_GraphPinDesc* pxFallbackOut = xTable.FindPin("FallbackOut");
	ZENITH_ASSERT_NOT_NULL(pxFallbackOut);
	if (pxFallbackOut)
	{
		ZENITH_ASSERT_STREQ(pxFallbackOut->m_szVarNameProperty, "m_strResultVar2");
		ZENITH_ASSERT_STREQ(pxFallbackOut->m_szFallbackVarNameProperty, "m_strVar");
	}

	const Zenith_GraphPinDesc* pxInstanceOut = xTable.FindPin("InstanceOut");
	ZENITH_ASSERT_NOT_NULL(pxInstanceOut);
	if (pxInstanceOut)
	{
		ZENITH_ASSERT_TRUE(pxInstanceOut->m_bInstanceResolved);
		ZENITH_ASSERT_TRUE(pxInstanceOut->m_eType == eGRAPH_PIN_TYPE_ANY);
	}

	const Zenith_GraphPinDesc* pxSelRead = xTable.FindPin("SelRead");
	const Zenith_GraphPinDesc* pxSelWrite = xTable.FindPin("SelWrite");
	const Zenith_GraphPinDesc* pxSelRW = xTable.FindPin("SelRW");
	const Zenith_GraphPinDesc* pxItems = xTable.FindPin("Items");
	ZENITH_ASSERT_NOT_NULL(pxSelRead);
	ZENITH_ASSERT_NOT_NULL(pxSelWrite);
	ZENITH_ASSERT_NOT_NULL(pxSelRW);
	ZENITH_ASSERT_NOT_NULL(pxItems);
	if (pxSelRead && pxSelWrite && pxSelRW && pxItems)
	{
		ZENITH_ASSERT_TRUE(pxSelRead->m_eRole == GRAPH_PIN_ROLE_SELECTOR_READ);
		ZENITH_ASSERT_TRUE(pxSelRead->m_eType == PROPERTY_TYPE_STRING);
		ZENITH_ASSERT_TRUE(pxSelWrite->m_eRole == GRAPH_PIN_ROLE_SELECTOR_WRITE);
		ZENITH_ASSERT_TRUE(pxSelWrite->m_eType == PROPERTY_TYPE_ENTITY_ID);
		ZENITH_ASSERT_TRUE(pxSelRW->m_eRole == GRAPH_PIN_ROLE_SELECTOR_READWRITE);
		ZENITH_ASSERT_TRUE(pxItems->m_eRole == GRAPH_PIN_ROLE_LIST);
		ZENITH_ASSERT_STREQ(pxItems->m_szVarNameProperty, "m_strListVar");
	}

	// ★ The corrected pairing rule: ResolveTargetEntity accepts ENTITY_ID ONLY
	// (a string entity name is never legal at runtime), while a POSITION
	// reference resolves ENTITY_ID or VECTOR3.
	const Zenith_GraphPinDesc* pxTargetEntity = xTable.FindPin("TargetEntity");
	const Zenith_GraphPinDesc* pxTargetPosition = xTable.FindPin("TargetPosition");
	ZENITH_ASSERT_NOT_NULL(pxTargetEntity);
	ZENITH_ASSERT_NOT_NULL(pxTargetPosition);
	if (pxTargetEntity && pxTargetPosition)
	{
		ZENITH_ASSERT_TRUE(pxTargetEntity->m_eRole == GRAPH_PIN_ROLE_TARGET_REF);
		ZENITH_ASSERT_EQ(pxTargetEntity->m_uAcceptedTypeMask, uGRAPH_PIN_ACCEPT_TARGET_ENTITY);
		ZENITH_ASSERT_EQ(pxTargetEntity->m_uAcceptedTypeMask & Zenith_GraphPinTypeMaskBit(PROPERTY_TYPE_STRING), 0u);
		ZENITH_ASSERT_NE(pxTargetPosition->m_uAcceptedTypeMask & Zenith_GraphPinTypeMaskBit(PROPERTY_TYPE_VECTOR3), 0u);
		ZENITH_ASSERT_NE(pxTargetPosition->m_uAcceptedTypeMask & Zenith_GraphPinTypeMaskBit(PROPERTY_TYPE_ENTITY_ID), 0u);
	}

	// An unknown pin name is a null, not an assert.
	ZENITH_ASSERT_NULL(xTable.FindPin("NoSuchPin"));
}

// The registry detects a pin table the same way it detects a property table -
// including through INHERITANCE, which the collision-source family needs.
ZENITH_TEST(GraphPinTable, Registry_PinTableConceptDetected)
{
	EnsureValidatorTestNodesRegistered();
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();

	// Positive: a class with BOTH tables, declared in production order.
	const Zenith_GraphNodeTypeInfo* pxReader = xRegistry.Find("Test_ValReader");
	ZENITH_ASSERT_NOT_NULL(pxReader);
	if (pxReader == nullptr)
	{
		return;
	}
	ZENITH_ASSERT_NOT_NULL(pxReader->m_pfnGetPropertyTable);
	ZENITH_ASSERT_NOT_NULL(pxReader->m_pfnGetPinTable);
	ZENITH_ASSERT_EQ(pxReader->m_pfnGetPinTable()->GetPinCount(), 1u);
	ZENITH_ASSERT_NOT_NULL(pxReader->m_pfnGetPinTable()->FindPin("Value"));

	// Negative: no pin table at all -> null fn -> the node is OPAQUE.
	const Zenith_GraphNodeTypeInfo* pxOpaque = xRegistry.Find("Test_ValOpaque");
	ZENITH_ASSERT_NOT_NULL(pxOpaque);
	if (pxOpaque)
	{
		ZENITH_ASSERT_NOT_NULL(pxOpaque->m_pfnGetPropertyTable);	// it DOES have params
		ZENITH_ASSERT_NULL(pxOpaque->m_pfnGetPinTable);
	}

	// Inheritance: the derived class declares no block and must resolve to -
	// and SHARE - the base's table, not an empty one of its own.
	const Zenith_GraphNodeTypeInfo* pxBase = xRegistry.Find("Test_ValPinBase");
	const Zenith_GraphNodeTypeInfo* pxDerived = xRegistry.Find("Test_ValPinDerived");
	ZENITH_ASSERT_NOT_NULL(pxBase);
	ZENITH_ASSERT_NOT_NULL(pxDerived);
	if (pxBase && pxDerived)
	{
		ZENITH_ASSERT_NOT_NULL(pxDerived->m_pfnGetPinTable);
		ZENITH_ASSERT_EQ(pxDerived->m_pfnGetPinTable(), pxBase->m_pfnGetPinTable());
		ZENITH_ASSERT_EQ(pxDerived->m_pfnGetPinTable()->GetPinCount(), 1u);
	}
}

//==============================================================================
// The shared plumbing the validator and the editor both use
//==============================================================================

// The blob -> instance idiom has ONE home now; this is it working, and
// refusing, at both ends.
ZENITH_TEST(GraphPinTable, Definition_ApplyNodeParamsRoundTrip)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	u_int uReader = 0;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		uReader = xBuilder.Node("Test_ValReader");
		xBuilder.ParamString(uReader, "m_strValueVar", "roundtrip");
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}
	ZENITH_ASSERT_NE(uReader, 0u);

	const Zenith_GraphNodeTypeInfo* pxInfo = Zenith_GraphNodeRegistry::Get().Find("Test_ValReader");
	ZENITH_ASSERT_NOT_NULL(pxInfo);
	if (pxInfo == nullptr)
	{
		return;
	}

	Zenith_GraphNode* pxNode = pxInfo->m_pfnCreate();
	ZENITH_ASSERT_TRUE(xDef.ApplyNodeParams(uReader, pxNode, *pxInfo));

	const Zenith_ReflectedProperty* pxProperty = pxInfo->m_pfnGetPropertyTable()->FindProperty("m_strValueVar");
	ZENITH_ASSERT_NOT_NULL(pxProperty);
	if (pxProperty)
	{
		Zenith_PropertyValue xValue;
		pxProperty->m_pfnGet(pxNode, xValue);
		ZENITH_ASSERT_TRUE(xValue.GetType() == PROPERTY_TYPE_STRING);
		ZENITH_ASSERT_STREQ(xValue.GetString().c_str(), "roundtrip");
	}

	// A node ID this definition does not contain changes nothing and says so.
	ZENITH_ASSERT_FALSE(xDef.ApplyNodeParams(9999u, pxNode, *pxInfo));
	delete pxNode;
}

// The effective exec-output count, in all three of its shapes, out of the one
// home the editor and the validator now share.
ZENITH_TEST(GraphPinTable, Registry_ExecOutputCountFunnel)
{
	EnsureValidatorTestNodesRegistered();
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();

	Zenith_GraphDefinition xDef;
	// An UNREGISTERED type: preserved as an unresolved node, and the funnel
	// answers 1 rather than guessing.
	const u_int uUnknown = xDef.AddNode("Test_ValNeverRegistered");

	u_int uPlain = 0;
	u_int uFlagged = 0;
	u_int uDynamic = 0;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		uPlain = xBuilder.Node("Test_ValPlain");
		uFlagged = xBuilder.Node("Test_ValFailurePin");
		uDynamic = xBuilder.Node("Test_ValDynPin");
		xBuilder.ParamInt(uDynamic, "m_iPinCount", 4);
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}

	ZENITH_ASSERT_NE(uUnknown, 0u);
	ZENITH_ASSERT_EQ(xRegistry.GetExecOutputCount(xDef, uUnknown), 1u);
	// A node ID that is not in the definition at all.
	ZENITH_ASSERT_EQ(xRegistry.GetExecOutputCount(xDef, 0u), 1u);
	// Static, unflagged.
	ZENITH_ASSERT_EQ(xRegistry.GetExecOutputCount(xDef, uPlain), 1u);
	// Static + the routable failure pin.
	ZENITH_ASSERT_EQ(xRegistry.GetExecOutputCount(xDef, uFlagged), 2u);
	// Dynamic: the CONFIGURED count, which only exists once params are applied
	// (the registered static count is 1).
	ZENITH_ASSERT_EQ(xRegistry.GetExecOutputCount(xDef, uDynamic), 4u);
}

//==============================================================================
// Declare-or-error
//==============================================================================

ZENITH_TEST(GraphValidator, Validator_UndeclaredReadIsErrorWhenLatched)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		const u_int uReader = xBuilder.Node("Test_ValReader");
		xBuilder.ParamString(uReader, "m_strValueVar", "missing");
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}

	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, true, axFindings);

	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_UNDECLARED_READ), 1u);
	const Zenith_GraphValidationFinding* pxFinding = FirstOfRule(axFindings, GRAPH_VALIDATION_RULE_UNDECLARED_READ);
	ZENITH_ASSERT_NOT_NULL(pxFinding);
	if (pxFinding)
	{
		ZENITH_ASSERT_TRUE(pxFinding->m_eSeverity == GRAPH_VALIDATION_SEVERITY_ERROR);
		ZENITH_ASSERT_TRUE(pxFinding->m_bWouldBeError);
		ZENITH_ASSERT_STREQ(pxFinding->m_strVar.c_str(), "missing");
		ZENITH_ASSERT_STREQ(pxFinding->m_strPin.c_str(), "Value");
		ZENITH_ASSERT_STREQ(pxFinding->m_strTypeName.c_str(), "Test_ValReader");
	}
}

// The SAME graph under the report-only setting every A-5 caller uses: the
// finding is still made, at WARNING, carrying the flag that says what it would
// have been. That flag is the whole latch mechanism.
ZENITH_TEST(GraphValidator, Validator_UndeclaredReadIsWarningWhenNotLatched)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		const u_int uReader = xBuilder.Node("Test_ValReader");
		xBuilder.ParamString(uReader, "m_strValueVar", "missing");
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}

	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, false, axFindings);

	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_UNDECLARED_READ), 1u);
	const Zenith_GraphValidationFinding* pxFinding = FirstOfRule(axFindings, GRAPH_VALIDATION_RULE_UNDECLARED_READ);
	if (pxFinding)
	{
		ZENITH_ASSERT_TRUE(pxFinding->m_eSeverity == GRAPH_VALIDATION_SEVERITY_WARNING);
		ZENITH_ASSERT_TRUE(pxFinding->m_bWouldBeError);
	}
}

ZENITH_TEST(GraphValidator, Validator_DeclaredReadPasses)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		Zenith_PropertyValue xDefault;
		xDefault.SetFloat(0.0f);
		xBuilder.Variable("value", xDefault);
		xBuilder.Node("Test_ValReader");	// default m_strValueVar == "value"
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}

	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, true, axFindings);
	ZENITH_ASSERT_EQ(axFindings.GetSize(), 0u);
}

ZENITH_TEST(GraphValidator, Validator_OtherWriterSatisfiesRead)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		xBuilder.Node("Test_ValWriter");	// OUTPUT FLOAT -> "value"
		xBuilder.Node("Test_ValReader");	// INPUT FLOAT  <- "value"
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}

	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, true, axFindings);
	ZENITH_ASSERT_EQ(axFindings.GetSize(), 0u);
}

// ★ A read-modify-write cannot seed itself: the value it writes is a function
// of the value it read, so its own write is not a writer for its own read.
ZENITH_TEST(GraphValidator, Validator_OwnReadWriteDoesNotSatisfyItselfWhenLatched)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		xBuilder.Node("Test_ValAccumulate");	// READWRITE FLOAT on "value"
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}

	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, true, axFindings);

	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_SELF_READWRITE), 1u);
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_UNDECLARED_READ), 0u);
	const Zenith_GraphValidationFinding* pxFinding = FirstOfRule(axFindings, GRAPH_VALIDATION_RULE_SELF_READWRITE);
	if (pxFinding)
	{
		ZENITH_ASSERT_TRUE(pxFinding->m_eSeverity == GRAPH_VALIDATION_SEVERITY_ERROR);
		ZENITH_ASSERT_STREQ(pxFinding->m_strVar.c_str(), "value");
	}
}

ZENITH_TEST(GraphValidator, Validator_OwnReadWriteDoesNotSatisfyItselfWhenNotLatched)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		xBuilder.Node("Test_ValAccumulate");
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}

	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, false, axFindings);

	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_SELF_READWRITE), 1u);
	const Zenith_GraphValidationFinding* pxFinding = FirstOfRule(axFindings, GRAPH_VALIDATION_RULE_SELF_READWRITE);
	if (pxFinding)
	{
		ZENITH_ASSERT_TRUE(pxFinding->m_eSeverity == GRAPH_VALIDATION_SEVERITY_WARNING);
		ZENITH_ASSERT_TRUE(pxFinding->m_bWouldBeError);
	}
}

// The AddBlackboardFloat shape end to end: flagged while its target is
// undeclared, SILENT the moment the graph declares it. Without the second half
// the first is just an assertion that something warns.
ZENITH_TEST(GraphValidator, Validator_UndeclaredReadWriteTargetIsError)
{
	EnsureValidatorTestNodesRegistered();

	{
		Zenith_GraphDefinition xDef;
		{
			Zenith_GraphBuilder xBuilder(xDef);
			const u_int uAdd = xBuilder.Node("Test_ValAccumulate");
			xBuilder.ParamString(uAdd, "m_strVariable", "score");
			ZENITH_ASSERT_TRUE(xBuilder.Build());
		}
		Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
		RunValidate(xDef, true, axFindings);
		ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_SELF_READWRITE), 1u);
	}

	{
		Zenith_GraphDefinition xDef;
		{
			Zenith_GraphBuilder xBuilder(xDef);
			Zenith_PropertyValue xDefault;
			xDefault.SetFloat(0.0f);
			xBuilder.Variable("score", xDefault);
			const u_int uAdd = xBuilder.Node("Test_ValAccumulate");
			xBuilder.ParamString(uAdd, "m_strVariable", "score");
			ZENITH_ASSERT_TRUE(xBuilder.Build());
		}
		Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
		RunValidate(xDef, true, axFindings);
		ZENITH_ASSERT_EQ(axFindings.GetSize(), 0u);
	}
}

//==============================================================================
// Types
//==============================================================================

ZENITH_TEST(GraphValidator, Validator_TypeMismatchWriterReaderIsError)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		xBuilder.Node("Test_ValIntWriter");	// OUTPUT INT32 -> "value"
		xBuilder.Node("Test_ValReader");	// INPUT  FLOAT <- "value"
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}

	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, true, axFindings);

	// The write SATISFIES the read - the problem is what it writes.
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_UNDECLARED_READ), 0u);
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_TYPE_MISMATCH), 1u);
	const Zenith_GraphValidationFinding* pxFinding = FirstOfRule(axFindings, GRAPH_VALIDATION_RULE_TYPE_MISMATCH);
	if (pxFinding)
	{
		ZENITH_ASSERT_TRUE(pxFinding->m_eSeverity == GRAPH_VALIDATION_SEVERITY_ERROR);
		ZENITH_ASSERT_STREQ(pxFinding->m_strVar.c_str(), "value");
	}
}

ZENITH_TEST(GraphValidator, Validator_AnyUnifies)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		xBuilder.Node("Test_ValAnyWriter");	// SELECTOR_WRITE ANY -> "value"
		xBuilder.Node("Test_ValReader");	// INPUT FLOAT        <- "value"
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}

	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, true, axFindings);
	ZENITH_ASSERT_EQ(axFindings.GetSize(), 0u);
}

ZENITH_TEST(GraphValidator, Validator_TargetRefPairsWithEntityIdWriter)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		xBuilder.Node("Test_ValEntityWriter");	// OUTPUT ENTITY_ID -> "target"
		xBuilder.Node("Test_ValTargetEntity");	// TARGET_REF       <- "target"
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}

	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, true, axFindings);
	ZENITH_ASSERT_EQ(axFindings.GetSize(), 0u);
}

ZENITH_TEST(GraphValidator, Validator_TargetPositionAcceptsVector3AndEntityId)
{
	EnsureValidatorTestNodesRegistered();

	{
		Zenith_GraphDefinition xDef;
		{
			Zenith_GraphBuilder xBuilder(xDef);
			xBuilder.Node("Test_ValVec3Writer");
			xBuilder.Node("Test_ValTargetPosition");
			ZENITH_ASSERT_TRUE(xBuilder.Build());
		}
		Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
		RunValidate(xDef, true, axFindings);
		ZENITH_ASSERT_EQ(axFindings.GetSize(), 0u);
	}

	{
		Zenith_GraphDefinition xDef;
		{
			Zenith_GraphBuilder xBuilder(xDef);
			xBuilder.Node("Test_ValEntityWriter");
			xBuilder.Node("Test_ValTargetPosition");
			ZENITH_ASSERT_TRUE(xBuilder.Build());
		}
		Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
		RunValidate(xDef, true, axFindings);
		ZENITH_ASSERT_EQ(axFindings.GetSize(), 0u);
	}
}

// ★ The mask is not decoration: an ENTITY target fed a VECTOR3 resolves to an
// invalid entity at runtime, silently, forever.
ZENITH_TEST(GraphValidator, Validator_TargetEntityRefusesVector3)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		xBuilder.Node("Test_ValVec3Writer");	// OUTPUT VECTOR3 -> "target"
		xBuilder.Node("Test_ValTargetEntity");	// entity-only TARGET_REF <- "target"
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}

	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, true, axFindings);

	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_UNDECLARED_READ), 0u);
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_TYPE_MISMATCH), 1u);
}

// The empty-result-var form binds to the FALLBACK property, and the control
// half proves that is what did the binding.
ZENITH_TEST(GraphValidator, Validator_FallbackVarNameBinds)
{
	EnsureValidatorTestNodesRegistered();

	{
		Zenith_GraphDefinition xDef;
		{
			Zenith_GraphBuilder xBuilder(xDef);
			xBuilder.Node("Test_ValFallback");	// result var "" -> binds to m_strVar == "value"
			xBuilder.Node("Test_ValReader");	// reads "value"
			ZENITH_ASSERT_TRUE(xBuilder.Build());
		}
		Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
		RunValidate(xDef, true, axFindings);
		ZENITH_ASSERT_EQ(axFindings.GetSize(), 0u);
	}

	{
		Zenith_GraphDefinition xDef;
		{
			Zenith_GraphBuilder xBuilder(xDef);
			const u_int uFallback = xBuilder.Node("Test_ValFallback");
			xBuilder.ParamString(uFallback, "m_strVar", "elsewhere");
			xBuilder.Node("Test_ValReader");	// still reads "value" - now unwritten
			ZENITH_ASSERT_TRUE(xBuilder.Build());
		}
		Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
		RunValidate(xDef, true, axFindings);
		ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_UNDECLARED_READ), 1u);
	}
}

// An instance-resolved pin: answered, then DECLINED. A declined answer is ANY
// plus a warning naming the type - never a fabricated type.
ZENITH_TEST(GraphValidator, Validator_InstanceResolvedPinTypeAndUnresolvedWarning)
{
	EnsureValidatorTestNodesRegistered();

	{
		Zenith_GraphDefinition xDef;
		{
			Zenith_GraphBuilder xBuilder(xDef);
			const u_int uInstance = xBuilder.Node("Test_ValInstance");
			xBuilder.ParamInt(uInstance, "m_iOp", 0);	// answers FLOAT
			xBuilder.Node("Test_ValReader");			// INPUT FLOAT <- "value"
			ZENITH_ASSERT_TRUE(xBuilder.Build());
		}
		Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
		RunValidate(xDef, true, axFindings);
		ZENITH_ASSERT_EQ(axFindings.GetSize(), 0u);
	}

	{
		Zenith_GraphDefinition xDef;
		{
			Zenith_GraphBuilder xBuilder(xDef);
			const u_int uInstance = xBuilder.Node("Test_ValInstance");
			xBuilder.ParamInt(uInstance, "m_iOp", 1);	// declines
			xBuilder.Node("Test_ValReader");
			ZENITH_ASSERT_TRUE(xBuilder.Build());
		}
		Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
		RunValidate(xDef, true, axFindings);
		ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_INSTANCE_TYPE_UNRESOLVED), 1u);
		// ANY, so no fabricated disagreement with the FLOAT reader...
		ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_TYPE_MISMATCH), 0u);
		// ...and the write still counts as a write.
		ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_UNDECLARED_READ), 0u);
		const Zenith_GraphValidationFinding* pxFinding =
			FirstOfRule(axFindings, GRAPH_VALIDATION_RULE_INSTANCE_TYPE_UNRESOLVED);
		if (pxFinding)
		{
			ZENITH_ASSERT_FALSE(pxFinding->m_bWouldBeError);
		}
	}
}

//==============================================================================
// Informational rules + the opaque rule
//==============================================================================

ZENITH_TEST(GraphValidator, Validator_DeclaredUnreferencedIsWarning)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		Zenith_PropertyValue xFloat;
		xFloat.SetFloat(0.0f);
		xBuilder.Variable("value", xFloat);
		xBuilder.Variable("unused", xFloat);
		xBuilder.Node("Test_ValReader");	// references "value" only
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}

	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, true, axFindings);

	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_DECLARED_UNUSED), 1u);
	const Zenith_GraphValidationFinding* pxFinding = FirstOfRule(axFindings, GRAPH_VALIDATION_RULE_DECLARED_UNUSED);
	if (pxFinding)
	{
		// Never an error, even latched: an unused declaration breaks nothing.
		ZENITH_ASSERT_TRUE(pxFinding->m_eSeverity == GRAPH_VALIDATION_SEVERITY_WARNING);
		ZENITH_ASSERT_FALSE(pxFinding->m_bWouldBeError);
		ZENITH_ASSERT_STREQ(pxFinding->m_strVar.c_str(), "unused");
	}
}

// ★ One opaque node suppresses the whole graph's unreferenced warning: an
// opaque node's reads are invisible, so "nothing references this" is a claim
// the validator cannot make. Until the node library is annotated, that is EVERY
// shipped graph.
ZENITH_TEST(GraphValidator, Validator_UnreferencedWarningSuppressedByOpaqueNode)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		Zenith_PropertyValue xFloat;
		xFloat.SetFloat(0.0f);
		xBuilder.Variable("value", xFloat);
		xBuilder.Variable("unused", xFloat);
		xBuilder.Node("Test_ValReader");
		xBuilder.Node("Test_ValOpaque");	// the one un-annotated node
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}

	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, true, axFindings);
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_DECLARED_UNUSED), 0u);

	// Control: the SAME declarations with no opaque node DO warn, so the
	// zero above is the suppression and not a rule that never fires.
	Zenith_GraphDefinition xControl;
	{
		Zenith_GraphBuilder xBuilder(xControl);
		Zenith_PropertyValue xFloat;
		xFloat.SetFloat(0.0f);
		xBuilder.Variable("value", xFloat);
		xBuilder.Variable("unused", xFloat);
		xBuilder.Node("Test_ValReader");
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}
	Zenith_Vector<Zenith_GraphValidationFinding> axControl;
	RunValidate(xControl, true, axControl);
	ZENITH_ASSERT_EQ(CountRule(axControl, GRAPH_VALIDATION_RULE_DECLARED_UNUSED), 1u);
}

ZENITH_TEST(GraphValidator, Validator_PinBindingToNonStringPropertyIsReportedNotAsserted)
{
	EnsureValidatorTestNodesRegistered();

	// The pin names a FLOAT property as its var binding. The tagged string
	// getter would ASSERT on that; the validator must report it and skip the
	// pin instead (no undeclared-read finding can come from a skipped pin).
	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		xBuilder.Node("Test_ValBadBinding");
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}
	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, true, axFindings);
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_PIN_BINDING_INVALID), 1u);
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_UNDECLARED_READ), 0u);
}

ZENITH_TEST(GraphPinTable, Registry_ExecOutputCountAppliesParamsBeforeCounting)
{
	// SwitchOnString derives its pin count from m_strCases and CACHES the parse
	// on the first GetDynamicExecOutputCount call. The funnel must apply the
	// definition's params before that first call, or a configured three-case
	// switch counts as its one-pin default (three PIN_OUT_OF_RANGE findings on
	// every ST_Dispenser build, and one drawn pin instead of four).
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
	xRegistry.EnsureInitialized();
	if (xRegistry.Find("SwitchOnString") == nullptr)
	{
		return;	// an exe without the engine flow library has nothing to measure
	}
	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		const u_int uSwitch = xBuilder.Node("SwitchOnString");
		xBuilder.ParamString(uSwitch, "m_strCases", "a,b,c");
		ZENITH_ASSERT_TRUE(xBuilder.Build());
		ZENITH_ASSERT_EQ(xRegistry.GetExecOutputCount(xDef, uSwitch), 4u);	// 3 cases + default
	}
}

ZENITH_TEST(GraphValidator, Validator_EveryRuleAndSeverityHasAName)
{
	// The [GraphValidator] log grammar is the whole capture story: a rule added
	// without a name arm degrades to UNKNOWN and disappears from A-7's group-by.
	for (u_int u = 0; u < static_cast<u_int>(GRAPH_VALIDATION_RULE_COUNT); ++u)
	{
		const char* szName = Zenith_GraphDefinitionValidator::GetRuleName(static_cast<Zenith_GraphValidationRule>(u));
		ZENITH_ASSERT_NOT_NULL(szName);
		ZENITH_ASSERT_TRUE(szName != nullptr && szName[0] != '\0' && strcmp(szName, "UNKNOWN") != 0);
	}
	ZENITH_ASSERT_STREQ(Zenith_GraphDefinitionValidator::GetSeverityName(GRAPH_VALIDATION_SEVERITY_ERROR), "ERROR");
	ZENITH_ASSERT_STREQ(Zenith_GraphDefinitionValidator::GetSeverityName(GRAPH_VALIDATION_SEVERITY_WARNING), "WARN");
}

ZENITH_TEST(GraphValidator, Validator_ListNameIsWarning)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		xBuilder.Node("Test_ValList");	// LIST "items"
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}

	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, true, axFindings);

	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_LIST_NAME), 1u);
	// A list name is NOT an undeclared read: lists are a separate store that is
	// created on first use and never declared.
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_UNDECLARED_READ), 0u);
	const Zenith_GraphValidationFinding* pxFinding = FirstOfRule(axFindings, GRAPH_VALIDATION_RULE_LIST_NAME);
	if (pxFinding)
	{
		ZENITH_ASSERT_FALSE(pxFinding->m_bWouldBeError);
		ZENITH_ASSERT_STREQ(pxFinding->m_strVar.c_str(), "items");
	}
}

// An opaque node contributes NOTHING: no writer, no read, no unused warning.
// This is the shape every production node has today, which is why the A-5
// report over the games is expected to be near-empty.
ZENITH_TEST(GraphValidator, Validator_OpaqueNodeProducesNothing)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		Zenith_PropertyValue xFloat;
		xFloat.SetFloat(0.0f);
		xBuilder.Variable("declared", xFloat);
		const u_int uOpaque = xBuilder.Node("Test_ValOpaque");
		// Its param names a variable nothing declares - and that is invisible,
		// because nothing told the validator the property IS a variable name.
		xBuilder.ParamString(uOpaque, "m_strValueVar", "invisible");
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}

	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, true, axFindings);
	ZENITH_ASSERT_EQ(axFindings.GetSize(), 0u);
}

// "" is NOT BOUND, not "a variable called nothing" - for a const-only pin whose
// descriptor never names a var property, and for a bound pin whose var name was
// edited to empty.
ZENITH_TEST(GraphValidator, Validator_EmptyBindingIsSkipped)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		xBuilder.Node("Test_ValConstOnly");
		const u_int uReader = xBuilder.Node("Test_ValReader");
		xBuilder.ParamString(uReader, "m_strValueVar", "");
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}

	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, true, axFindings);
	ZENITH_ASSERT_EQ(axFindings.GetSize(), 0u);
}

//==============================================================================
// Structural
//==============================================================================

// An edge to a node that is not in the graph. AddEdge refuses to CREATE one, so
// the only way in is a loaded asset - ReadFromDataStream takes edges verbatim,
// by design, so an unresolved/hand-edited graph round-trips. This builds that
// payload directly.
ZENITH_TEST(GraphValidator, Validator_OrphanEdgeIsError)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_DataStream xStream;
	xStream << Zenith_GraphDefinition::uGRAPH_MAGIC;
	xStream << Zenith_GraphDefinition::uGRAPH_VERSION;
	xStream << 0u;									// variables
	xStream << 1u;									// nodes
	xStream << 1u;									//   node id 1
	xStream << std::string("Test_ValOpaque");		//   type (opaque: no pin findings to mix in)
	xStream << 1u;									//   type version
	xStream << 0u;									//   param blob bytes
	xStream << 1u;									// edges
	xStream << 1u;									//   src node 1
	xStream << 0u;									//   src pin 0
	xStream << 99u;									//   dst node 99 - NOT IN THE GRAPH
	xStream << 0u;									//   dst pin
	xStream << 4u;									// layout section bytes (just the count)
	xStream << 0u;									// layout entry count
	xStream.SetCursor(0);

	Zenith_GraphDefinition xDef;
	ZENITH_ASSERT_TRUE(xDef.ReadFromDataStream(xStream));
	ZENITH_ASSERT_EQ(xDef.GetNodeCount(), 1u);
	ZENITH_ASSERT_EQ(xDef.GetEdgeCount(), 1u);

	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, true, axFindings);

	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_ORPHAN_EDGE), 1u);
	const Zenith_GraphValidationFinding* pxFinding = FirstOfRule(axFindings, GRAPH_VALIDATION_RULE_ORPHAN_EDGE);
	if (pxFinding)
	{
		ZENITH_ASSERT_TRUE(pxFinding->m_eSeverity == GRAPH_VALIDATION_SEVERITY_ERROR);
		ZENITH_ASSERT_TRUE(pxFinding->m_bWouldBeError);
	}
}

// AddEdge never checks the pin index (it only enforces one edge per (node,pin)),
// so an edge past the last pin is authorable and inert - the wire is drawn
// nowhere and fires never.
ZENITH_TEST(GraphValidator, Validator_PinBeyondOutputCountIsErrorOnUnflaggedType)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		const u_int uSrc = xBuilder.Node("Test_ValPlain");	// 1 output pin, unflagged
		const u_int uDst = xBuilder.Node("Test_ValPlain");
		xBuilder.Edge(uSrc, 1, uDst);						// pin 1 does not exist
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}

	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, true, axFindings);
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_PIN_OUT_OF_RANGE), 1u);
}

// The same index, on a type that CARRIES the routable failure pin, is a legal
// wire - and one past it is not.
ZENITH_TEST(GraphValidator, Validator_FailurePinIndexAllowedOnFlaggedType)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		const u_int uFlaggedA = xBuilder.Node("Test_ValFailurePin");
		const u_int uFlaggedB = xBuilder.Node("Test_ValFailurePin");
		const u_int uHandler = xBuilder.Node("Test_ValPlain");
		xBuilder.Edge(uFlaggedA, 1, uHandler);	// the failure pin - legal
		xBuilder.Edge(uFlaggedB, 2, uHandler);	// one past it - not
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}

	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, true, axFindings);

	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_PIN_OUT_OF_RANGE), 1u);
	const Zenith_GraphValidationFinding* pxFinding = FirstOfRule(axFindings, GRAPH_VALIDATION_RULE_PIN_OUT_OF_RANGE);
	if (pxFinding)
	{
		ZENITH_ASSERT_STREQ(pxFinding->m_strTypeName.c_str(), "Test_ValFailurePin");
	}
}

// A variable-pin type's range comes from the CONFIGURED instance, not from its
// registered static count (which is 1).
ZENITH_TEST(GraphValidator, Validator_PinBeyondOutputCountUsesDynamicInstanceCount)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		const u_int uDynamic = xBuilder.Node("Test_ValDynPin");
		xBuilder.ParamInt(uDynamic, "m_iPinCount", 3);
		const u_int uDst = xBuilder.Node("Test_ValPlain");
		xBuilder.Edge(uDynamic, 0, uDst);
		xBuilder.Edge(uDynamic, 2, uDst);	// the last configured pin - legal
		xBuilder.Edge(uDynamic, 3, uDst);	// one past it - not
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}

	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, true, axFindings);
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_PIN_OUT_OF_RANGE), 1u);
}

//==============================================================================
// The builder seam
//==============================================================================

// ★ REPORT-ONLY. A graph with an undeclared read still BUILDS, still reports no
// errors, and still produces findings - which is the whole of A-5's posture.
ZENITH_TEST(GraphValidator, GraphBuilder_BuildRunsValidatorReportOnly)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	Zenith_GraphBuilder xBuilder(xDef);
	const u_int uReader = xBuilder.Node("Test_ValReader");
	xBuilder.ParamString(uReader, "m_strValueVar", "nobody_declares_me");

	ZENITH_ASSERT_TRUE(xBuilder.Build());
	ZENITH_ASSERT_FALSE(xBuilder.HasErrors());
	ZENITH_ASSERT_GT(xBuilder.GetValidationFindingCount(), 0u);

	bool bFoundUndeclaredRead = false;
	for (u_int u = 0; u < xBuilder.GetValidationFindingCount(); ++u)
	{
		const Zenith_GraphValidationFinding& xFinding = xBuilder.GetValidationFindingAt(u);
		// Report-only: NOTHING is reported at ERROR severity from Build().
		ZENITH_ASSERT_TRUE(xFinding.m_eSeverity == GRAPH_VALIDATION_SEVERITY_WARNING);
		if (xFinding.m_eRule == GRAPH_VALIDATION_RULE_UNDECLARED_READ)
		{
			bFoundUndeclaredRead = true;
			ZENITH_ASSERT_TRUE(xFinding.m_bWouldBeError);
			ZENITH_ASSERT_STREQ(xFinding.m_strVar.c_str(), "nobody_declares_me");
		}
	}
	ZENITH_ASSERT_TRUE(bFoundUndeclaredRead);
}

// A definition carries no name, so the report's graph= field comes from the
// builder. The GRAPH_BUILD automation step passes the asset path; a bare
// builder reports <unnamed>.
ZENITH_TEST(GraphValidator, GraphBuilder_GraphNameReachesReport)
{
	EnsureValidatorTestNodesRegistered();

	{
		Zenith_GraphDefinition xDef;
		Zenith_GraphBuilder xBuilder(xDef);
		ZENITH_ASSERT_STREQ(xBuilder.GetGraphName(), "<unnamed>");
		xBuilder.SetGraphName("game:Graphs/zz_unit_named.bgraph");
		ZENITH_ASSERT_STREQ(xBuilder.GetGraphName(), "game:Graphs/zz_unit_named.bgraph");
		xBuilder.Node("Test_ValPlain");
		ZENITH_ASSERT_TRUE(xBuilder.Build());
		// The name survives the build (the report is logged from inside it).
		ZENITH_ASSERT_STREQ(xBuilder.GetGraphName(), "game:Graphs/zz_unit_named.bgraph");
	}

	{
		Zenith_GraphDefinition xDef;
		Zenith_GraphBuilder xBuilder(xDef);
		xBuilder.SetGraphName(nullptr);
		ZENITH_ASSERT_STREQ(xBuilder.GetGraphName(), "<unnamed>");
	}
}

#endif // ZENITH_TESTING
