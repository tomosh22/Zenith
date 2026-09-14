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
		ZENITH_GRAPH_PIN_INPUT_VARIADIC(VariadicIn, PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext&) override { return GRAPH_NODE_STATUS_SUCCESS; }
		const char* GetTypeName() const override { return "Test_ValDescriptors"; }
	};

	//--------------------------------------------------------------------------
	// B-3 WIRE fixtures. Every var-name property below defaults to "" on purpose:
	// a builder fixture that merely places one of these must not latch an
	// UNDECLARED_READ that has nothing to do with the wire under test.
	//--------------------------------------------------------------------------

	// An IMPURE producer: two typed OUTPUTs, so a wire can pick the wrong one.
	class ValTestProducerNode : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(ValTestProducerNode)
	public:
		ZENITH_PROPERTY(std::string, m_strResultVar, "")
		ZENITH_PROPERTY(std::string, m_strCountVar, "")

		ZENITH_GRAPH_PINS_BEGIN(ValTestProducerNode)
		ZENITH_GRAPH_PIN_OUTPUT(Result, "m_strResultVar", PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PIN_OUTPUT(Count, "m_strCountVar", PROPERTY_TYPE_INT32)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext&) override { return GRAPH_NODE_STATUS_SUCCESS; }
		const char* GetTypeName() const override { return "Test_ValProducer"; }
	};

	// An IMPURE consumer: a typed INPUT, a second typed INPUT of a DIFFERENT
	// type, a wildcard INPUT, and an OUTPUT (the pin a role-mismatch wire aims
	// at).
	class ValTestConsumerNode : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(ValTestConsumerNode)
	public:
		ZENITH_PROPERTY(std::string, m_strValueVar, "")
		ZENITH_PROPERTY(std::string, m_strIndexVar, "")
		ZENITH_PROPERTY(std::string, m_strAnyVar, "")
		ZENITH_PROPERTY(std::string, m_strEchoVar, "")

		ZENITH_GRAPH_PINS_BEGIN(ValTestConsumerNode)
		ZENITH_GRAPH_PIN_INPUT(Value, "m_strValueVar", PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PIN_INPUT(Index, "m_strIndexVar", PROPERTY_TYPE_INT32)
		ZENITH_GRAPH_PIN_INPUT(Any, "m_strAnyVar", eGRAPH_PIN_TYPE_ANY)
		ZENITH_GRAPH_PIN_OUTPUT(Echo, "m_strEchoVar", PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext&) override { return GRAPH_NODE_STATUS_SUCCESS; }
		const char* GetTypeName() const override { return "Test_ValConsumer"; }
	};

	// A PURE relay: one typed INPUT, one typed OUTPUT.
	class ValTestPureNode : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(ValTestPureNode)
	public:
		ZENITH_PROPERTY(std::string, m_strInVar, "")
		ZENITH_PROPERTY(std::string, m_strOutVar, "")

		ZENITH_GRAPH_PINS_BEGIN(ValTestPureNode)
		ZENITH_GRAPH_PIN_INPUT(In, "m_strInVar", PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PIN_OUTPUT(Out, "m_strOutVar", PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext&) override { return GRAPH_NODE_STATUS_SUCCESS; }
		const char* GetTypeName() const override { return "Test_ValPure"; }
	};

	// The SECOND pure type, so a cycle needs two DISTINCT nodes (AddDataEdge
	// refuses a self-loop whatever the pin names say).
	class ValTestPureBNode : public ValTestPureNode
	{
	public:
		const char* GetTypeName() const override { return "Test_ValPureB"; }
	};

	// An INSTANCE-RESOLVED OUTPUT usable as a wire SOURCE: op 0 answers FLOAT,
	// op 1 answers INT32, anything else DECLINES.
	class ValTestInstanceOutNode : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(ValTestInstanceOutNode)
	public:
		ZENITH_PROPERTY(std::string, m_strOutVar, "")
		ZENITH_PROPERTY(int32_t, m_iOp, 0)

		ZENITH_GRAPH_PINS_BEGIN(ValTestInstanceOutNode)
		ZENITH_GRAPH_PIN_OUTPUT_INSTANCE(Out, "m_strOutVar")
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext&) override { return GRAPH_NODE_STATUS_SUCCESS; }
		const char* GetTypeName() const override { return "Test_ValInstanceOut"; }

		bool GetPinType(u_int uPinIndex, Zenith_PropertyType& eOut) const override
		{
			if (uPinIndex != 0)
			{
				return false;
			}
			if (m_iOp == 0) { eOut = PROPERTY_TYPE_FLOAT; return true; }
			if (m_iOp == 1) { eOut = PROPERTY_TYPE_INT32; return true; }
			return false;
		}
	};

	// GetVariable's SHAPE, in the validator's own TU so no wire test needs the
	// engine node library: a SELECTOR_READ naming the variable, plus an OUTPUT
	// whose type FOLLOWS THAT VARIABLE'S DECLARATION.
	class ValTestFromVarNode : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(ValTestFromVarNode)
	public:
		ZENITH_PROPERTY(std::string, m_strVariable, "")

		ZENITH_GRAPH_PINS_BEGIN(ValTestFromVarNode)
		ZENITH_GRAPH_PIN_SELECTOR_READ(Variable, "m_strVariable", eGRAPH_PIN_TYPE_ANY)
		ZENITH_GRAPH_PIN_OUTPUT_FROM_VARIABLE(Value, "m_strVariable")
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext&) override { return GRAPH_NODE_STATUS_SUCCESS; }
		const char* GetTypeName() const override { return "Test_ValFromVar"; }
	};

	// A variadic INPUT family WITH a param-driven member count - the
	// GraphTestVariadic shape. ValTestDescriptorNode declares a family but
	// inherits GetDynamicDataInputCount() == -1, so it expands to zero members
	// and could never exercise an ordinal.
	class ValTestVariadicNode : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(ValTestVariadicNode)
	public:
		ZENITH_PROPERTY(int32_t, m_iInputCount, 3)
		ZENITH_PROPERTY(std::string, m_strTotalVar, "")

		ZENITH_GRAPH_PINS_BEGIN(ValTestVariadicNode)
		ZENITH_GRAPH_PIN_INPUT_VARIADIC(in, PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PIN_OUTPUT(Total, "m_strTotalVar", PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PINS_END

	public:
		int32_t GetDynamicDataInputCount() const override { return m_iInputCount; }
		GraphNodeStatus Execute(Zenith_GraphContext&) override { return GRAPH_NODE_STATUS_SUCCESS; }
		const char* GetTypeName() const override { return "Test_ValVariadic"; }
	};

	//--------------------------------------------------------------------------
	// Scratch node types WITHOUT pin tables (opaque / structural fixtures).
	//--------------------------------------------------------------------------

	// An EVENT SOURCE, so the dominance pass has a reachability root. Opaque on
	// purpose: dominance reads the exec graph and the registry's event type, and
	// never a pin table.
	class ValTestEventSourceNode : public Zenith_GraphNode
	{
	public:
		GraphNodeStatus Execute(Zenith_GraphContext&) override { return GRAPH_NODE_STATUS_SUCCESS; }
		const char* GetTypeName() const override { return "Test_ValEventSource"; }
	};

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
		// B-3 wire fixtures. Test_ValPure / Test_ValPureB carry the PURE flag (its
		// exec count is forced to 0 by the registry); the rest are ordinary.
		xRegistry.RegisterNodeType<ValTestProducerNode>("Test_ValProducer", GRAPH_EVENT_NONE, 1, false, "Test");
		xRegistry.RegisterNodeType<ValTestConsumerNode>("Test_ValConsumer", GRAPH_EVENT_NONE, 1, false, "Test");
		xRegistry.RegisterNodeType<ValTestPureNode>("Test_ValPure", GRAPH_EVENT_NONE, 1, false, "Test", false, true);
		xRegistry.RegisterNodeType<ValTestPureBNode>("Test_ValPureB", GRAPH_EVENT_NONE, 1, false, "Test", false, true);
		xRegistry.RegisterNodeType<ValTestInstanceOutNode>("Test_ValInstanceOut", GRAPH_EVENT_NONE, 1, false, "Test");
		xRegistry.RegisterNodeType<ValTestFromVarNode>("Test_ValFromVar", GRAPH_EVENT_NONE, 1, false, "Test");
		xRegistry.RegisterNodeType<ValTestVariadicNode>("Test_ValVariadic", GRAPH_EVENT_NONE, 1, false, "Test");
		xRegistry.RegisterNodeType<ValTestEventSourceNode>("Test_ValEventSource", GRAPH_EVENT_ON_UPDATE, 1, false, "Test");
	}

	void DeclareVar(Zenith_GraphBuilder& xBuilder, const char* szName, Zenith_PropertyType eType)
	{
		Zenith_PropertyValue xValue;
		switch (eType)
		{
		case PROPERTY_TYPE_INT32: xValue.SetInt32(0); break;
		case PROPERTY_TYPE_BOOL:  xValue.SetBool(false); break;
		default:                  xValue.SetFloat(0.0f); break;
		}
		xBuilder.Variable(szName, xValue);
	}

	// Declared below; used here. Unqualified lookup in an anonymous namespace
	// needs the declaration FIRST (ADL cannot find it: the argument types live
	// in the global namespace), so a definition placed after this helper is
	// C3861 in every configuration.
	u_int CountRule(const Zenith_Vector<Zenith_GraphValidationFinding>& axFindings, Zenith_GraphValidationRule eRule);

	// "Nothing but the rule under test fired." Every wire test asserts the other
	// three pass-1b rules are ZERO beside its own count, the precedent being
	// Validator_TypeMismatchWriterReaderIsError.
	void AssertOnlyWireRule(const Zenith_Vector<Zenith_GraphValidationFinding>& axFindings,
		Zenith_GraphValidationRule eExpected, u_int uExpectedCount)
	{
		const Zenith_GraphValidationRule aeRules[] =
		{
			GRAPH_VALIDATION_RULE_WIRE_PIN_UNKNOWN,
			GRAPH_VALIDATION_RULE_WIRE_ROLE_MISMATCH,
			GRAPH_VALIDATION_RULE_TYPE_MISMATCH,
			GRAPH_VALIDATION_RULE_EXEC_INTO_PURE,
		};
		for (u_int u = 0; u < 4u; ++u)
		{
			const u_int uExpect = (aeRules[u] == eExpected) ? uExpectedCount : 0u;
			ZENITH_ASSERT_EQ(CountRule(axFindings, aeRules[u]), uExpect,
				"rule %s: expected %u finding(s)", Zenith_GraphDefinitionValidator::GetRuleName(aeRules[u]), uExpect);
		}
	}

	void RunValidate(const Zenith_GraphDefinition& xDefinition,
		Zenith_Vector<Zenith_GraphValidationFinding>& axOut)
	{
		Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
		xRegistry.EnsureInitialized();
		Zenith_GraphDefinitionValidator::Validate(xDefinition, xRegistry, "Test_Graph", axOut);
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
	ZENITH_ASSERT_EQ(xTable.GetPinCount(), 13u);

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
	ZENITH_ASSERT_FALSE(pxValueIn->m_bVariadic);

	// The variadic INPUT family (B-2): an ordinal family, so it binds NEITHER a
	// var-name property NOR a const - a member is a wire or it is the type's
	// zero. Its members are named "<family><ordinal>" by a data edge.
	const Zenith_GraphPinDesc* pxVariadicIn = xTable.FindPin("VariadicIn");
	ZENITH_ASSERT_NOT_NULL(pxVariadicIn);
	if (pxVariadicIn)
	{
		ZENITH_ASSERT_TRUE(pxVariadicIn->m_bVariadic);
		ZENITH_ASSERT_TRUE(pxVariadicIn->m_eRole == GRAPH_PIN_ROLE_INPUT);
		ZENITH_ASSERT_TRUE(pxVariadicIn->m_eType == PROPERTY_TYPE_FLOAT);
		ZENITH_ASSERT_STREQ(pxVariadicIn->m_szVarNameProperty, "");
		ZENITH_ASSERT_STREQ(pxVariadicIn->m_szConstProperty, "");
	}

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
	u_int uInstance = 0;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		// Round-trip a permanent parameter, rather than the disposable INPUT
		// binding property C-1 removes from production node shapes.
		uInstance = xBuilder.Node("Test_ValInstance");
		xBuilder.ParamInt(uInstance, "m_iOp", 7);
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}
	ZENITH_ASSERT_NE(uInstance, 0u);

	const Zenith_GraphNodeTypeInfo* pxInfo = Zenith_GraphNodeRegistry::Get().Find("Test_ValInstance");
	ZENITH_ASSERT_NOT_NULL(pxInfo);
	if (pxInfo == nullptr)
	{
		return;
	}

	Zenith_GraphNode* pxNode = pxInfo->m_pfnCreate();
	ZENITH_ASSERT_TRUE(xDef.ApplyNodeParams(uInstance, pxNode, *pxInfo));

	const Zenith_ReflectedProperty* pxProperty = pxInfo->m_pfnGetPropertyTable()->FindProperty("m_iOp");
	ZENITH_ASSERT_NOT_NULL(pxProperty);
	if (pxProperty)
	{
		Zenith_PropertyValue xValue;
		pxProperty->m_pfnGet(pxNode, xValue);
		ZENITH_ASSERT_TRUE(xValue.GetType() == PROPERTY_TYPE_INT32);
		if (xValue.GetType() == PROPERTY_TYPE_INT32)
		{
			ZENITH_ASSERT_EQ(xValue.GetInt32(), 7);
		}
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

// ★ NEGATIVE FIXTURE. The graph deliberately reads a variable nothing declares,
// so Build() now returns FALSE - and the definition it leaves behind is still
// complete, which is what lets the validation below run over it at all.
ZENITH_TEST(GraphValidator, Validator_UndeclaredReadIsError)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		const u_int uReader = xBuilder.Node("Test_ValReader");
		xBuilder.ParamString(uReader, "m_strValueVar", "missing");
		ZENITH_ASSERT_FALSE(xBuilder.Build());
	}
	ZENITH_ASSERT_EQ(xDef.GetNodeCount(), 1u);	// a failed Build() rolls nothing back

	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, axFindings);

	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_UNDECLARED_READ), 1u);
	const Zenith_GraphValidationFinding* pxFinding = FirstOfRule(axFindings, GRAPH_VALIDATION_RULE_UNDECLARED_READ);
	ZENITH_ASSERT_NOT_NULL(pxFinding);
	if (pxFinding)
	{
		ZENITH_ASSERT_TRUE(pxFinding->m_eSeverity == GRAPH_VALIDATION_SEVERITY_ERROR);
		ZENITH_ASSERT_STREQ(pxFinding->m_strVar.c_str(), "missing");
		ZENITH_ASSERT_STREQ(pxFinding->m_strPin.c_str(), "Value");
		ZENITH_ASSERT_STREQ(pxFinding->m_strTypeName.c_str(), "Test_ValReader");
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
	RunValidate(xDef, axFindings);
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
	RunValidate(xDef, axFindings);
	ZENITH_ASSERT_EQ(axFindings.GetSize(), 0u);
}

// ★ A read-modify-write cannot seed itself: the value it writes is a function
// of the value it read, so its own write is not a writer for its own read.
ZENITH_TEST(GraphValidator, Validator_OwnReadWriteDoesNotSatisfyItself)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		xBuilder.Node("Test_ValAccumulate");	// READWRITE FLOAT on "value"
		ZENITH_ASSERT_FALSE(xBuilder.Build());	// negative fixture: SELF_READWRITE is an ERROR
	}

	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, axFindings);

	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_SELF_READWRITE), 1u);
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_UNDECLARED_READ), 0u);
	const Zenith_GraphValidationFinding* pxFinding = FirstOfRule(axFindings, GRAPH_VALIDATION_RULE_SELF_READWRITE);
	if (pxFinding)
	{
		ZENITH_ASSERT_TRUE(pxFinding->m_eSeverity == GRAPH_VALIDATION_SEVERITY_ERROR);
		ZENITH_ASSERT_STREQ(pxFinding->m_strVar.c_str(), "value");
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
			ZENITH_ASSERT_FALSE(xBuilder.Build());	// the flagged half is a negative fixture
		}
		Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
		RunValidate(xDef, axFindings);
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
		RunValidate(xDef, axFindings);
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
		ZENITH_ASSERT_FALSE(xBuilder.Build());	// negative fixture: TYPE_MISMATCH is an ERROR
	}

	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, axFindings);

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
	RunValidate(xDef, axFindings);
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
	RunValidate(xDef, axFindings);
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
		RunValidate(xDef, axFindings);
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
		RunValidate(xDef, axFindings);
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
		ZENITH_ASSERT_FALSE(xBuilder.Build());	// negative fixture: the mask rejects VECTOR3
	}

	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, axFindings);

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
		RunValidate(xDef, axFindings);
		// ★ B-3: the binding is what this test pins, and it still works - but the
		// in-place shape it expresses is now ONE warning of its own
		// (IN_PLACE_ALIASING), so the total is 1 rather than 0.
		ZENITH_ASSERT_EQ(axFindings.GetSize(), 1u);
		ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_IN_PLACE_ALIASING), 1u);
	}

	{
		Zenith_GraphDefinition xDef;
		{
			Zenith_GraphBuilder xBuilder(xDef);
			const u_int uFallback = xBuilder.Node("Test_ValFallback");
			xBuilder.ParamString(uFallback, "m_strVar", "elsewhere");
			xBuilder.Node("Test_ValReader");	// still reads "value" - now unwritten
			ZENITH_ASSERT_FALSE(xBuilder.Build());	// negative half: the read is unsatisfied
		}
		Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
		RunValidate(xDef, axFindings);
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
		RunValidate(xDef, axFindings);
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
		RunValidate(xDef, axFindings);
		ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_INSTANCE_TYPE_UNRESOLVED), 1u);
		// ANY, so no fabricated disagreement with the FLOAT reader...
		ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_TYPE_MISMATCH), 0u);
		// ...and the write still counts as a write.
		ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_UNDECLARED_READ), 0u);
		const Zenith_GraphValidationFinding* pxFinding =
			FirstOfRule(axFindings, GRAPH_VALIDATION_RULE_INSTANCE_TYPE_UNRESOLVED);
		if (pxFinding)
		{
			// A node declining to answer is not the graph author's defect.
			ZENITH_ASSERT_TRUE(pxFinding->m_eSeverity == GRAPH_VALIDATION_SEVERITY_WARNING);
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
	RunValidate(xDef, axFindings);

	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_DECLARED_UNUSED), 1u);
	const Zenith_GraphValidationFinding* pxFinding = FirstOfRule(axFindings, GRAPH_VALIDATION_RULE_DECLARED_UNUSED);
	if (pxFinding)
	{
		// Never an error: an unused declaration breaks nothing, so a graph that
		// carries one still BUILDS.
		ZENITH_ASSERT_TRUE(pxFinding->m_eSeverity == GRAPH_VALIDATION_SEVERITY_WARNING);
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
	RunValidate(xDef, axFindings);
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
	RunValidate(xControl, axControl);
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
	RunValidate(xDef, axFindings);
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
		// SwitchOnString's Value pin reads m_strVar, whose default is "state" -
		// a positive fixture must DECLARE what it reads or Build() latches.
		Zenith_PropertyValue xState;
		xState.SetString("");
		xBuilder.Variable("state", xState);
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
	RunValidate(xDef, axFindings);

	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_LIST_NAME), 1u);
	// A list name is NOT an undeclared read: lists are a separate store that is
	// created on first use and never declared.
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_UNDECLARED_READ), 0u);
	const Zenith_GraphValidationFinding* pxFinding = FirstOfRule(axFindings, GRAPH_VALIDATION_RULE_LIST_NAME);
	if (pxFinding)
	{
		ZENITH_ASSERT_TRUE(pxFinding->m_eSeverity == GRAPH_VALIDATION_SEVERITY_WARNING);
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
	RunValidate(xDef, axFindings);
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
	RunValidate(xDef, axFindings);
	ZENITH_ASSERT_EQ(axFindings.GetSize(), 0u);
}

//==============================================================================
// Structural
//==============================================================================

// An edge to a node that is not in the graph. AddEdge refuses to CREATE one, so
// the only way in is a loaded asset - ReadFromDataStream takes edges verbatim,
// by design, so an unresolved/hand-edited graph round-trips. This builds that
// payload directly.
//
// ★ AN ORPHAN IS DELIBERATELY NOT LOAD_SAFETY. It is inert at runtime
// (FindSuccessor returns 0), so the READ must SUCCEED and the FULL tier reports
// it - which is exactly what this test asserts, in the v2 byte layout.
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
	xStream << 1u;									// exec edges
	xStream << 1u;									//   src node 1
	xStream << 0u;									//   src pin 0
	xStream << 99u;									//   dst node 99 - NOT IN THE GRAPH
	xStream << 0u;									// data edges
	xStream << 4u;									// layout section bytes (just the count)
	xStream << 0u;									// layout entry count
	xStream.SetCursor(0);

	Zenith_GraphDefinition xDef;
	ZENITH_ASSERT_TRUE(xDef.ReadFromDataStream(xStream));
	ZENITH_ASSERT_EQ(xDef.GetNodeCount(), 1u);
	ZENITH_ASSERT_EQ(xDef.GetEdgeCount(), 1u);

	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, axFindings);

	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_ORPHAN_EDGE), 1u);
	const Zenith_GraphValidationFinding* pxFinding = FirstOfRule(axFindings, GRAPH_VALIDATION_RULE_ORPHAN_EDGE);
	if (pxFinding)
	{
		ZENITH_ASSERT_TRUE(pxFinding->m_eSeverity == GRAPH_VALIDATION_SEVERITY_ERROR);
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
		ZENITH_ASSERT_FALSE(xBuilder.Build());				// negative fixture: PIN_OUT_OF_RANGE
	}

	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, axFindings);
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
		ZENITH_ASSERT_FALSE(xBuilder.Build());	// the second edge is a PIN_OUT_OF_RANGE error
	}

	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, axFindings);

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
		ZENITH_ASSERT_FALSE(xBuilder.Build());	// the last edge is a PIN_OUT_OF_RANGE error
	}

	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, axFindings);
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_PIN_OUT_OF_RANGE), 1u);
}

//==============================================================================
// B-3 WIRES - pass 1b
//
// ★ Every test here asserts ZERO of the other three pass-1b rules beside its
// own count (AssertOnlyWireRule): a rule that fires on the wrong shape is
// exactly as wrong as one that does not fire at all.
//==============================================================================

ZENITH_TEST(GraphValidator, Validator_WireUnknownPinIsError)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		const u_int uSrc = xBuilder.Node("Test_ValProducer");
		const u_int uDst = xBuilder.Node("Test_ValConsumer");
		xBuilder.DataEdge(uSrc, "Result", uDst, "no_such_in");
		ZENITH_ASSERT_FALSE(xBuilder.Build());	// negative fixture: WIRE_PIN_UNKNOWN is an ERROR
	}

	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, axFindings);
	AssertOnlyWireRule(axFindings, GRAPH_VALIDATION_RULE_WIRE_PIN_UNKNOWN, 1u);
	const Zenith_GraphValidationFinding* pxFinding = FirstOfRule(axFindings, GRAPH_VALIDATION_RULE_WIRE_PIN_UNKNOWN);
	if (pxFinding)
	{
		ZENITH_ASSERT_TRUE(pxFinding->m_eSeverity == GRAPH_VALIDATION_SEVERITY_ERROR);
		ZENITH_ASSERT_STREQ(pxFinding->m_strPin.c_str(), "no_such_in");
	}
}

// ★ An OPAQUE endpoint on a DATA edge is an ERROR, deliberately, and that does
// NOT weaken the opaque doctrine: ResolveDataEdges skips exactly this wire, so
// it can never carry a value - reporting it is not a false finding.
ZENITH_TEST(GraphValidator, Validator_WireOpaqueEndpointIsError)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		const u_int uSrc = xBuilder.Node("Test_ValPlain");	// registered, NO pin table
		const u_int uDst = xBuilder.Node("Test_ValConsumer");
		xBuilder.DataEdge(uSrc, "out", uDst, "Value");
		ZENITH_ASSERT_FALSE(xBuilder.Build());
	}
	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, axFindings);
	AssertOnlyWireRule(axFindings, GRAPH_VALIDATION_RULE_WIRE_PIN_UNKNOWN, 1u);

	// Control: the SAME wire between two ANNOTATED nodes is clean, so the error
	// above is the OPACITY and not the wire's existence.
	Zenith_GraphDefinition xControl;
	{
		Zenith_GraphBuilder xBuilder(xControl);
		const u_int uSrc = xBuilder.Node("Test_ValProducer");
		const u_int uDst = xBuilder.Node("Test_ValConsumer");
		xBuilder.DataEdge(uSrc, "Result", uDst, "Value");
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}
	Zenith_Vector<Zenith_GraphValidationFinding> axControl;
	RunValidate(xControl, axControl);
	ZENITH_ASSERT_EQ(axControl.GetSize(), 0u);
}

ZENITH_TEST(GraphValidator, Validator_WireRoleMismatchForOutputAsDestination)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		const u_int uSrc = xBuilder.Node("Test_ValProducer");
		const u_int uDst = xBuilder.Node("Test_ValConsumer");
		xBuilder.DataEdge(uSrc, "Result", uDst, "Echo");	// a real pin, but an OUTPUT
		ZENITH_ASSERT_FALSE(xBuilder.Build());
	}
	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, axFindings);
	AssertOnlyWireRule(axFindings, GRAPH_VALIDATION_RULE_WIRE_ROLE_MISMATCH, 1u);
}

ZENITH_TEST(GraphValidator, Validator_WireRoleMismatchForInputAsSource)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		const u_int uSrc = xBuilder.Node("Test_ValConsumer");
		const u_int uDst = xBuilder.Node("Test_ValConsumer");
		xBuilder.DataEdge(uSrc, "Value", uDst, "Value");	// a real pin, but an INPUT
		ZENITH_ASSERT_FALSE(xBuilder.Build());
	}
	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, axFindings);
	AssertOnlyWireRule(axFindings, GRAPH_VALIDATION_RULE_WIRE_ROLE_MISMATCH, 1u);
}

// Two STATIC types that disagree. The finding comes from the LOAD-SAFETY body -
// one home for the static case - which is why the count is exactly one.
ZENITH_TEST(GraphValidator, Validator_WireTypeMismatchIsError)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		const u_int uSrc = xBuilder.Node("Test_ValProducer");
		const u_int uDst = xBuilder.Node("Test_ValConsumer");
		xBuilder.DataEdge(uSrc, "Result", uDst, "Index");	// FLOAT -> INT32
		ZENITH_ASSERT_FALSE(xBuilder.Build());
	}
	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, axFindings);
	AssertOnlyWireRule(axFindings, GRAPH_VALIDATION_RULE_TYPE_MISMATCH, 1u);
}

// "ANY unifies" is EXACT: there is deliberately no warning for an ANY endpoint
// meeting a typed one.
ZENITH_TEST(GraphValidator, Validator_WireAnyUnifies)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		const u_int uSrc = xBuilder.Node("Test_ValProducer");
		const u_int uDst = xBuilder.Node("Test_ValConsumer");
		xBuilder.DataEdge(uSrc, "Count", uDst, "Any");	// INT32 -> ANY
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}
	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, axFindings);
	ZENITH_ASSERT_EQ(axFindings.GetSize(), 0u);
}

// An INSTANCE-RESOLVED endpoint: the wire is checked against the CONFIGURED
// instance's answer, not against the descriptor's ANY.
ZENITH_TEST(GraphValidator, Validator_WireInstanceResolvedTypeChecked)
{
	EnsureValidatorTestNodesRegistered();

	{	// op 1 -> INT32 into a FLOAT input
		Zenith_GraphDefinition xDef;
		{
			Zenith_GraphBuilder xBuilder(xDef);
			const u_int uSrc = xBuilder.Node("Test_ValInstanceOut");
			xBuilder.ParamInt(uSrc, "m_iOp", 1);
			const u_int uDst = xBuilder.Node("Test_ValConsumer");
			xBuilder.DataEdge(uSrc, "Out", uDst, "Value");
			ZENITH_ASSERT_FALSE(xBuilder.Build());
		}
		Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
		RunValidate(xDef, axFindings);
		AssertOnlyWireRule(axFindings, GRAPH_VALIDATION_RULE_TYPE_MISMATCH, 1u);
	}

	{	// op 0 -> FLOAT into the same FLOAT input
		Zenith_GraphDefinition xDef;
		{
			Zenith_GraphBuilder xBuilder(xDef);
			const u_int uSrc = xBuilder.Node("Test_ValInstanceOut");
			xBuilder.ParamInt(uSrc, "m_iOp", 0);
			const u_int uDst = xBuilder.Node("Test_ValConsumer");
			xBuilder.DataEdge(uSrc, "Out", uDst, "Value");
			ZENITH_ASSERT_TRUE(xBuilder.Build());
		}
		Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
		RunValidate(xDef, axFindings);
		ZENITH_ASSERT_EQ(axFindings.GetSize(), 0u);
	}
}

// ★ THE FROM-VARIABLE FORM: the output pin's type is the DECLARATION's.
ZENITH_TEST(GraphValidator, Validator_WireFromVariableTypeFollowsDeclaration)
{
	EnsureValidatorTestNodesRegistered();

	{	// declared FLOAT, wired into an INT32 input
		Zenith_GraphDefinition xDef;
		{
			Zenith_GraphBuilder xBuilder(xDef);
			DeclareVar(xBuilder, "hp", PROPERTY_TYPE_FLOAT);
			const u_int uSrc = xBuilder.Node("Test_ValFromVar");
			xBuilder.ParamString(uSrc, "m_strVariable", "hp");
			const u_int uDst = xBuilder.Node("Test_ValConsumer");
			xBuilder.DataEdge(uSrc, "Value", uDst, "Index");
			ZENITH_ASSERT_FALSE(xBuilder.Build());
		}
		Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
		RunValidate(xDef, axFindings);
		AssertOnlyWireRule(axFindings, GRAPH_VALIDATION_RULE_TYPE_MISMATCH, 1u);
		ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_UNDECLARED_READ), 0u);
	}

	{	// the SAME declaration into the FLOAT input: clean
		Zenith_GraphDefinition xDef;
		{
			Zenith_GraphBuilder xBuilder(xDef);
			DeclareVar(xBuilder, "hp", PROPERTY_TYPE_FLOAT);
			const u_int uSrc = xBuilder.Node("Test_ValFromVar");
			xBuilder.ParamString(uSrc, "m_strVariable", "hp");
			const u_int uDst = xBuilder.Node("Test_ValConsumer");
			xBuilder.DataEdge(uSrc, "Value", uDst, "Value");
			ZENITH_ASSERT_TRUE(xBuilder.Build());
		}
		Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
		RunValidate(xDef, axFindings);
		ZENITH_ASSERT_EQ(axFindings.GetSize(), 0u);
	}
}

// An UNDECLARED variable is ANY - one finding for one mistake, and it is the
// declare-or-error one on the SELECTOR, never a fabricated type disagreement.
ZENITH_TEST(GraphValidator, Validator_WireFromVariableUndeclaredIsAnyAndReported)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		const u_int uSrc = xBuilder.Node("Test_ValFromVar");
		xBuilder.ParamString(uSrc, "m_strVariable", "ghost");
		const u_int uDst = xBuilder.Node("Test_ValConsumer");
		xBuilder.DataEdge(uSrc, "Value", uDst, "Index");
		ZENITH_ASSERT_FALSE(xBuilder.Build());
	}
	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, axFindings);
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_UNDECLARED_READ), 1u);
	AssertOnlyWireRule(axFindings, GRAPH_VALIDATION_RULE_TYPE_MISMATCH, 0u);
}

// ★ A TYPE SOURCE IS NOT A WRITER. Reusing the var-name binding for the
// from-variable form would make this node an annotated WRITER of the variable it
// READS - which would silently satisfy every other reader's declare-or-error.
ZENITH_TEST(GraphValidator, Validator_FromVariableOutputIsNotAWriter)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		const u_int uFromVar = xBuilder.Node("Test_ValFromVar");
		xBuilder.ParamString(uFromVar, "m_strVariable", "ghost");
		const u_int uReader = xBuilder.Node("Test_ValReader");
		xBuilder.ParamString(uReader, "m_strValueVar", "ghost");
		ZENITH_ASSERT_FALSE(xBuilder.Build());
	}
	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, axFindings);

	// TWO undeclared reads: the from-variable node's own selector, and the second
	// node - which would be SILENT if the first had registered as a writer.
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_UNDECLARED_READ), 2u);
	bool bReaderReported = false;
	for (u_int u = 0; u < axFindings.GetSize(); ++u)
	{
		const Zenith_GraphValidationFinding& xFinding = axFindings.Get(u);
		if (xFinding.m_eRule == GRAPH_VALIDATION_RULE_UNDECLARED_READ && xFinding.m_strTypeName == "Test_ValReader")
		{
			bReaderReported = true;
		}
	}
	ZENITH_ASSERT_TRUE(bReaderReported);
}

// A declaration that disagrees with an ANNOTATED WRITER is pass 3's finding, and
// pass 1b must not report the same disagreement a second time.
ZENITH_TEST(GraphValidator, Validator_FromVariableWriterDisagreementReportedOnce)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		DeclareVar(xBuilder, "hp", PROPERTY_TYPE_FLOAT);
		const u_int uWriter = xBuilder.Node("Test_ValIntWriter");	// writes "hp" as INT32
		xBuilder.ParamString(uWriter, "m_strResultVar", "hp");
		const u_int uReader = xBuilder.Node("Test_ValReader");		// reads "hp" as FLOAT
		xBuilder.ParamString(uReader, "m_strValueVar", "hp");
		const u_int uFromVar = xBuilder.Node("Test_ValFromVar");		// types its output FLOAT (the DECLARATION)
		xBuilder.ParamString(uFromVar, "m_strVariable", "hp");
		const u_int uDst = xBuilder.Node("Test_ValConsumer");
		xBuilder.DataEdge(uFromVar, "Value", uDst, "Value");			// FLOAT -> FLOAT: clean
		ZENITH_ASSERT_FALSE(xBuilder.Build());
	}
	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, axFindings);
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_TYPE_MISMATCH), 1u);
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_WIRE_PIN_UNKNOWN), 0u);
}

// ★ A WIRE SUPERSEDES THE FALLBACK the C-1 sweep deletes: a CONNECTED input
// never consults its var name at runtime, so the name is not a read.
ZENITH_TEST(GraphValidator, Validator_WiredInputSkipsVarNameCheck)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		const u_int uSrc = xBuilder.Node("Test_ValProducer");
		const u_int uDst = xBuilder.Node("Test_ValConsumer");
		xBuilder.ParamString(uDst, "m_strValueVar", "nobody_declares_me");
		xBuilder.DataEdge(uSrc, "Result", uDst, "Value");
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}
	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, axFindings);
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_UNDECLARED_READ), 0u);

	// Control: the SAME var name with NO wire IS an undeclared read, so the zero
	// above is the wire and not a rule that stopped firing.
	Zenith_GraphDefinition xControl;
	{
		Zenith_GraphBuilder xBuilder(xControl);
		const u_int uDst = xBuilder.Node("Test_ValConsumer");
		xBuilder.ParamString(uDst, "m_strValueVar", "nobody_declares_me");
		ZENITH_ASSERT_FALSE(xBuilder.Build());
	}
	Zenith_Vector<Zenith_GraphValidationFinding> axControl;
	RunValidate(xControl, axControl);
	ZENITH_ASSERT_EQ(CountRule(axControl, GRAPH_VALIDATION_RULE_UNDECLARED_READ), 1u);
}

ZENITH_TEST(GraphValidator, Validator_WireVariadicOrdinalResolved)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		const u_int uSrc = xBuilder.Node("Test_ValProducer");
		const u_int uDst = xBuilder.Node("Test_ValVariadic");
		xBuilder.ParamInt(uDst, "m_iInputCount", 3);
		xBuilder.DataEdge(uSrc, "Result", uDst, "in2");	// the last configured member
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}
	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, axFindings);
	ZENITH_ASSERT_EQ(axFindings.GetSize(), 0u);
}

ZENITH_TEST(GraphValidator, Validator_WireVariadicOrdinalBeyondCountIsError)
{
	EnsureValidatorTestNodesRegistered();

	{	// one past the configured member count
		Zenith_GraphDefinition xDef;
		{
			Zenith_GraphBuilder xBuilder(xDef);
			const u_int uSrc = xBuilder.Node("Test_ValProducer");
			const u_int uDst = xBuilder.Node("Test_ValVariadic");
			xBuilder.ParamInt(uDst, "m_iInputCount", 3);
			xBuilder.DataEdge(uSrc, "Result", uDst, "in3");
			ZENITH_ASSERT_FALSE(xBuilder.Build());
		}
		Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
		RunValidate(xDef, axFindings);
		AssertOnlyWireRule(axFindings, GRAPH_VALIDATION_RULE_WIRE_PIN_UNKNOWN, 1u);
	}

	{	// ★ the BARE FAMILY name: a family has no non-ordinal member, so a wire
		// naming it could never be addressed by any accessor.
		Zenith_GraphDefinition xDef;
		{
			Zenith_GraphBuilder xBuilder(xDef);
			const u_int uSrc = xBuilder.Node("Test_ValProducer");
			const u_int uDst = xBuilder.Node("Test_ValVariadic");
			xBuilder.DataEdge(uSrc, "Result", uDst, "in");
			ZENITH_ASSERT_FALSE(xBuilder.Build());
		}
		Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
		RunValidate(xDef, axFindings);
		AssertOnlyWireRule(axFindings, GRAPH_VALIDATION_RULE_WIRE_PIN_UNKNOWN, 1u);
	}
}

//==============================================================================
// B-3 PURE nodes, cycles and exec edges
//==============================================================================

ZENITH_TEST(GraphValidator, Validator_ExecIntoPureIsError)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		const u_int uSrc = xBuilder.Node("Test_ValPlain");
		const u_int uPure = xBuilder.Node("Test_ValPure");
		const u_int uDst = xBuilder.Node("Test_ValConsumer");
		xBuilder.Edge(uSrc, 0, uPure);					// exec INTO a pure node
		xBuilder.DataEdge(uPure, "Out", uDst, "Value");	// consumed, so PURE_UNCONSUMED cannot fire
		ZENITH_ASSERT_FALSE(xBuilder.Build());
	}
	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, axFindings);
	AssertOnlyWireRule(axFindings, GRAPH_VALIDATION_RULE_EXEC_INTO_PURE, 1u);
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_PURE_UNCONSUMED), 0u);
}

// ★ There is deliberately NO rule for an exec edge OUT of a pure node: a
// surviving PURE flag forces the type's exec-output count to 0, so the existing
// range check already reports it.
ZENITH_TEST(GraphValidator, Validator_ExecOutOfPureIsPinOutOfRange)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		const u_int uPure = xBuilder.Node("Test_ValPure");
		const u_int uDst = xBuilder.Node("Test_ValPlain");
		xBuilder.Edge(uPure, 0, uDst);
		ZENITH_ASSERT_FALSE(xBuilder.Build());
	}
	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, axFindings);
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_PIN_OUT_OF_RANGE), 1u);
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_EXEC_INTO_PURE), 0u);
}

ZENITH_TEST(GraphValidator, Validator_PureUnconsumedIsWarning)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		xBuilder.Node("Test_ValPure");	// no outgoing wire: nothing can ever evaluate it
		ZENITH_ASSERT_TRUE(xBuilder.Build());	// a WARNING, so Build() still succeeds
	}
	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, axFindings);
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_PURE_UNCONSUMED), 1u);
	const Zenith_GraphValidationFinding* pxFinding = FirstOfRule(axFindings, GRAPH_VALIDATION_RULE_PURE_UNCONSUMED);
	if (pxFinding)
	{
		ZENITH_ASSERT_TRUE(pxFinding->m_eSeverity == GRAPH_VALIDATION_SEVERITY_WARNING);
	}

	// Control: the SAME node with a consumer is silent.
	Zenith_GraphDefinition xControl;
	{
		Zenith_GraphBuilder xBuilder(xControl);
		const u_int uPure = xBuilder.Node("Test_ValPure");
		const u_int uDst = xBuilder.Node("Test_ValConsumer");
		xBuilder.DataEdge(uPure, "Out", uDst, "Value");
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}
	Zenith_Vector<Zenith_GraphValidationFinding> axControl;
	RunValidate(xControl, axControl);
	ZENITH_ASSERT_EQ(CountRule(axControl, GRAPH_VALIDATION_RULE_PURE_UNCONSUMED), 0u);
}

ZENITH_TEST(GraphValidator, Validator_DataCycleIsError)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		const u_int uA = xBuilder.Node("Test_ValPure");
		const u_int uB = xBuilder.Node("Test_ValPureB");
		xBuilder.DataEdge(uA, "Out", uB, "In");
		xBuilder.DataEdge(uB, "Out", uA, "In");
		ZENITH_ASSERT_FALSE(xBuilder.Build());	// negative fixture: DATA_CYCLE is an ERROR
	}
	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, axFindings);
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_DATA_CYCLE), 1u);
	AssertOnlyWireRule(axFindings, GRAPH_VALIDATION_RULE_WIRE_PIN_UNKNOWN, 0u);	// nothing else on those edges
	const Zenith_GraphValidationFinding* pxFinding = FirstOfRule(axFindings, GRAPH_VALIDATION_RULE_DATA_CYCLE);
	if (pxFinding)
	{
		ZENITH_ASSERT_TRUE(pxFinding->m_eSeverity == GRAPH_VALIDATION_SEVERITY_ERROR);
	}
}

// ★ AN IMPURE PRODUCER BREAKS A CYCLE BY DESIGN: its slot is LATCHED by its own
// Execute, so a consumer pulling it reads the previous run's value rather than
// re-entering it. Only PURE-sourced edges are followed.
ZENITH_TEST(GraphValidator, Validator_ImpureProducerBreaksCycle)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		const u_int uPure = xBuilder.Node("Test_ValPure");
		const u_int uImpure = xBuilder.Node("Test_ValConsumer");	// INPUT Value + OUTPUT Echo
		xBuilder.DataEdge(uPure, "Out", uImpure, "Value");			// pure -> impure
		xBuilder.DataEdge(uImpure, "Echo", uPure, "In");			// impure -> pure: NOT followed
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}
	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, axFindings);
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_DATA_CYCLE), 0u);
	AssertOnlyWireRule(axFindings, GRAPH_VALIDATION_RULE_WIRE_PIN_UNKNOWN, 0u);	// clean, not merely un-cycled
}

//==============================================================================
// B-3 DOMINANCE - a warning, never an error
//==============================================================================

ZENITH_TEST(GraphValidator, Validator_DominanceWarnsWhenConsumerReachableWithoutProducer)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		const u_int uSource = xBuilder.Node("Test_ValEventSource");
		const u_int uProducer = xBuilder.Node("Test_ValProducer");	// NOTHING runs it
		const u_int uConsumer = xBuilder.Node("Test_ValConsumer");
		xBuilder.Edge(uSource, 0, uConsumer);
		xBuilder.DataEdge(uProducer, "Result", uConsumer, "Value");
		ZENITH_ASSERT_TRUE(xBuilder.Build());	// a WARNING: the graph still builds
	}
	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, axFindings);
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_DOMINANCE), 1u);
}

ZENITH_TEST(GraphValidator, Validator_DominanceSilentWhenProducerDominates)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		const u_int uSource = xBuilder.Node("Test_ValEventSource");
		const u_int uProducer = xBuilder.Node("Test_ValProducer");
		const u_int uConsumer = xBuilder.Node("Test_ValConsumer");
		xBuilder.Edge(uSource, 0, uProducer);		// every path to the consumer
		xBuilder.Edge(uProducer, 0, uConsumer);		// goes through the producer
		xBuilder.DataEdge(uProducer, "Result", uConsumer, "Value");
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}
	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, axFindings);
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_DOMINANCE), 0u);
}

// The chain is followed THROUGH a pure relay: a pure node is not the consumer,
// it is the wire.
ZENITH_TEST(GraphValidator, Validator_DominanceFollowsThroughPureNode)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		const u_int uSource = xBuilder.Node("Test_ValEventSource");
		const u_int uProducer = xBuilder.Node("Test_ValProducer");
		const u_int uPure = xBuilder.Node("Test_ValPure");
		const u_int uConsumer = xBuilder.Node("Test_ValConsumer");
		xBuilder.Edge(uSource, 0, uConsumer);
		xBuilder.DataEdge(uProducer, "Result", uPure, "In");
		xBuilder.DataEdge(uPure, "Out", uConsumer, "Value");
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}
	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, axFindings);
	// The PRODUCER is blamed, not the relay: a pure node cannot run late.
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_DOMINANCE), 1u);
	const Zenith_GraphValidationFinding* pxFinding = FirstOfRule(axFindings, GRAPH_VALIDATION_RULE_DOMINANCE);
	if (pxFinding)
	{
		ZENITH_ASSERT_STREQ(pxFinding->m_strTypeName.c_str(), "Test_ValProducer");
	}
}

// ★ NEVER AN ERROR. Dominance models neither Sequence's branch ORDER nor
// reactive preemption, so a false positive must never fail a Build().
ZENITH_TEST(GraphValidator, Validator_DominanceNeverError)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	Zenith_GraphBuilder xBuilder(xDef);
	const u_int uSource = xBuilder.Node("Test_ValEventSource");
	const u_int uProducer = xBuilder.Node("Test_ValProducer");
	const u_int uConsumer = xBuilder.Node("Test_ValConsumer");
	xBuilder.Edge(uSource, 0, uConsumer);
	xBuilder.DataEdge(uProducer, "Result", uConsumer, "Value");

	ZENITH_ASSERT_TRUE(xBuilder.Build());
	ZENITH_ASSERT_FALSE(xBuilder.HasErrors());
	ZENITH_ASSERT_EQ(xBuilder.GetValidationFindingCount(), 1u);
	ZENITH_ASSERT_TRUE(xBuilder.GetValidationFindingAt(0).m_eRule == GRAPH_VALIDATION_RULE_DOMINANCE);
	ZENITH_ASSERT_TRUE(xBuilder.GetValidationFindingAt(0).m_eSeverity == GRAPH_VALIDATION_SEVERITY_WARNING);
}

//==============================================================================
// B-3 IN-PLACE ALIASING + the exported pin-type resolver
//==============================================================================

// The two Math nodes' in-place form: an empty result var means the OUTPUT writes
// back over the variable it was computed from. B-6.1 records the count; C-1
// requires zero.
ZENITH_TEST(GraphValidator, Validator_InPlaceAliasingIsWarning)
{
	EnsureValidatorTestNodesRegistered();

	{
		Zenith_GraphDefinition xDef;
		{
			Zenith_GraphBuilder xBuilder(xDef);
			xBuilder.Node("Test_ValFallback");	// result var "" -> binds m_strVar == "value"
			xBuilder.Node("Test_ValReader");	// reads "value", so the write satisfies it
			ZENITH_ASSERT_TRUE(xBuilder.Build());
		}
		Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
		RunValidate(xDef, axFindings);
		ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_IN_PLACE_ALIASING), 1u);
		const Zenith_GraphValidationFinding* pxFinding = FirstOfRule(axFindings, GRAPH_VALIDATION_RULE_IN_PLACE_ALIASING);
		if (pxFinding)
		{
			ZENITH_ASSERT_TRUE(pxFinding->m_eSeverity == GRAPH_VALIDATION_SEVERITY_WARNING);
			ZENITH_ASSERT_STREQ(pxFinding->m_strVar.c_str(), "value");
		}
	}

	{	// the result var SET: the output has its own destination, no aliasing
		Zenith_GraphDefinition xDef;
		{
			Zenith_GraphBuilder xBuilder(xDef);
			const u_int uFallback = xBuilder.Node("Test_ValFallback");
			xBuilder.ParamString(uFallback, "m_strResultVar", "result");
			ZENITH_ASSERT_TRUE(xBuilder.Build());
		}
		Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
		RunValidate(xDef, axFindings);
		ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_IN_PLACE_ALIASING), 0u);
	}
}

// ★ THE RESOLVER AND THE RUNTIME MUST AGREE. Two answers to "what type is this
// pin" is how a slot gets stamped one way and validated another.
ZENITH_TEST(GraphValidator, Validator_ResolvePinTypeAgreesWithRuntimeSlot)
{
	EnsureValidatorTestNodesRegistered();
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
	xRegistry.EnsureInitialized();

	Zenith_GraphDefinition xDef;
	u_int uStatic = 0;
	u_int uInstance = 0;
	u_int uFromVar = 0;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		DeclareVar(xBuilder, "hp", PROPERTY_TYPE_INT32);
		uStatic = xBuilder.Node("Test_ValProducer");		// pin 0 Result: STATIC FLOAT
		uInstance = xBuilder.Node("Test_ValInstanceOut");	// pin 0 Out: INSTANCE-RESOLVED
		xBuilder.ParamInt(uInstance, "m_iOp", 1);			// ...answering INT32
		uFromVar = xBuilder.Node("Test_ValFromVar");		// pin 1 Value: FROM THE DECLARATION
		xBuilder.ParamString(uFromVar, "m_strVariable", "hp");
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}

	Zenith_PropertyType eStatic = eGRAPH_PIN_TYPE_ANY;
	Zenith_PropertyType eInstance = eGRAPH_PIN_TYPE_ANY;
	Zenith_PropertyType eFromVar = eGRAPH_PIN_TYPE_ANY;
	ZENITH_ASSERT_TRUE(Zenith_GraphDefinitionValidator::ResolvePinType(xDef, xRegistry, uStatic, 0u, eStatic));
	ZENITH_ASSERT_TRUE(Zenith_GraphDefinitionValidator::ResolvePinType(xDef, xRegistry, uInstance, 0u, eInstance));
	ZENITH_ASSERT_TRUE(Zenith_GraphDefinitionValidator::ResolvePinType(xDef, xRegistry, uFromVar, 1u, eFromVar));
	ZENITH_ASSERT_TRUE(eStatic == PROPERTY_TYPE_FLOAT);
	ZENITH_ASSERT_TRUE(eInstance == PROPERTY_TYPE_INT32);
	ZENITH_ASSERT_TRUE(eFromVar == PROPERTY_TYPE_INT32);

	// A node this definition does not contain, and a pin past the table.
	Zenith_PropertyType eMissing = eGRAPH_PIN_TYPE_ANY;
	ZENITH_ASSERT_FALSE(Zenith_GraphDefinitionValidator::ResolvePinType(xDef, xRegistry, 9999u, 0u, eMissing));
	ZENITH_ASSERT_FALSE(Zenith_GraphDefinitionValidator::ResolvePinType(xDef, xRegistry, uStatic, 99u, eMissing));

	Zenith_BehaviourGraph xGraph;
	ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
	const Zenith_GraphNode* pxStatic = xGraph.FindNode(uStatic);
	const Zenith_GraphNode* pxInstance = xGraph.FindNode(uInstance);
	Zenith_GraphNode* pxFromVar = xGraph.FindNode(uFromVar);
	ZENITH_ASSERT_NOT_NULL(pxStatic);
	ZENITH_ASSERT_NOT_NULL(pxInstance);
	ZENITH_ASSERT_NOT_NULL(pxFromVar);
	if (pxStatic == nullptr || pxInstance == nullptr || pxFromVar == nullptr)
	{
		return;
	}
	ZENITH_ASSERT_TRUE(pxStatic->GetOutputPinType(0u) == eStatic);
	ZENITH_ASSERT_TRUE(pxInstance->GetOutputPinType(0u) == eInstance);
	ZENITH_ASSERT_TRUE(pxFromVar->GetOutputPinType(1u) == eFromVar);

	// ★ THE DECLARATION, NOT THE LIVE VALUE. A blackboard override of a different
	// type must not retype the slot under a consumer the validator already
	// checked against INT32.
	Zenith_PropertyValue xOverride;
	xOverride.SetFloat(1.5f);
	xGraph.GetBlackboard().SetValue("hp", xOverride);
	ZENITH_ASSERT_TRUE(pxFromVar->GetOutputPinType(1u) == PROPERTY_TYPE_INT32);
}

//==============================================================================
// The builder seam
//==============================================================================

// ★ THE LATCH. A graph with an undeclared read FAILS its Build() - "caught at
// Build()" is literal - and the definition it leaves behind is still complete,
// with the findings readable off the builder.
ZENITH_TEST(GraphValidator, GraphBuilder_BuildFailsOnValidationError)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	Zenith_GraphBuilder xBuilder(xDef);
	const u_int uReader = xBuilder.Node("Test_ValReader");
	xBuilder.ParamString(uReader, "m_strValueVar", "nobody_declares_me");

	ZENITH_ASSERT_FALSE(xBuilder.Build());
	ZENITH_ASSERT_TRUE(xBuilder.HasErrors());

	// Exactly ONE finding, and it is the ERROR - not a warning that happens to
	// sit beside one.
	ZENITH_ASSERT_EQ(xBuilder.GetValidationFindingCount(), 1u);
	u_int uErrors = 0;
	bool bFoundUndeclaredRead = false;
	for (u_int u = 0; u < xBuilder.GetValidationFindingCount(); ++u)
	{
		const Zenith_GraphValidationFinding& xFinding = xBuilder.GetValidationFindingAt(u);
		if (xFinding.m_eSeverity == GRAPH_VALIDATION_SEVERITY_ERROR)
		{
			++uErrors;
		}
		if (xFinding.m_eRule == GRAPH_VALIDATION_RULE_UNDECLARED_READ)
		{
			bFoundUndeclaredRead = true;
			ZENITH_ASSERT_TRUE(xFinding.m_eSeverity == GRAPH_VALIDATION_SEVERITY_ERROR);
			ZENITH_ASSERT_STREQ(xFinding.m_strVar.c_str(), "nobody_declares_me");
		}
	}
	ZENITH_ASSERT_EQ(uErrors, 1u);
	ZENITH_ASSERT_TRUE(bFoundUndeclaredRead);

	// ★ THE DEFINITION IS STILL COMPLETE. Validation is the LAST thing Build()
	// does and nothing is rolled back, so a false return is a report, not a
	// half-built graph - the negative fixtures above all depend on this.
	ZENITH_ASSERT_EQ(xDef.GetNodeCount(), 1u);
	ZENITH_ASSERT_NOT_NULL(xDef.FindNodeDef(uReader));
}

// The other half: a WARNING-only graph still builds. Without it, the test above
// would pass on a Build() that latched on any finding at all.
ZENITH_TEST(GraphValidator, GraphBuilder_BuildPassesWithWarningsOnly)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	Zenith_GraphBuilder xBuilder(xDef);
	Zenith_PropertyValue xFloat;
	xFloat.SetFloat(0.0f);
	xBuilder.Variable("value", xFloat);		// the reader's default var - satisfied
	xBuilder.Variable("unused", xFloat);	// DECLARED_UNUSED: a warning, and only that
	xBuilder.Node("Test_ValReader");

	ZENITH_ASSERT_TRUE(xBuilder.Build());
	ZENITH_ASSERT_FALSE(xBuilder.HasErrors());
	ZENITH_ASSERT_EQ(xBuilder.GetValidationFindingCount(), 1u);
	ZENITH_ASSERT_TRUE(xBuilder.GetValidationFindingAt(0).m_eSeverity == GRAPH_VALIDATION_SEVERITY_WARNING);
	ZENITH_ASSERT_TRUE(xBuilder.GetValidationFindingAt(0).m_eRule == GRAPH_VALIDATION_RULE_DECLARED_UNUSED);
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

//==============================================================================
// The LOAD_SAFETY tier (B-1)
//
// ★ EVERY DEFECT HERE IS UNREACHABLE THROUGH THE AUTHORING API - AddEdge and
// AddDataEdge refuse all three - so the fixtures are hand-built v2 STREAMS read
// through ReadFromDataStream's findings out-param, and each asserts the RULE
// rather than the bool. Every one is paired with a defect-free control from the
// same emitter, so a refusal cannot be a malformed fixture.
//==============================================================================

namespace
{
	struct VSNode
	{
		u_int m_uNodeID = 0;
		const char* m_szTypeName = "Test_ValOpaque";
	};

	struct VSEdge
	{
		u_int m_uSrc = 0;
		u_int m_uSrcPin = 0;
		u_int m_uDst = 0;
	};

	struct VSDataEdge
	{
		u_int m_uSrc = 0;
		const char* m_szSrcPin = "";
		u_int m_uDst = 0;
		const char* m_szDstPin = "";
	};

	void EmitValidatorGraphStream(Zenith_DataStream& xStream,
		const VSNode* pxNodes, u_int uNodeCount,
		const VSEdge* pxEdges, u_int uEdgeCount,
		const VSDataEdge* pxDataEdges, u_int uDataEdgeCount)
	{
		xStream << Zenith_GraphDefinition::uGRAPH_MAGIC;
		xStream << Zenith_GraphDefinition::uGRAPH_VERSION;
		xStream << 0u;								// variables
		xStream << uNodeCount;
		for (u_int u = 0; u < uNodeCount; ++u)
		{
			xStream << pxNodes[u].m_uNodeID;
			xStream << std::string(pxNodes[u].m_szTypeName);
			xStream << 1u;							//   type version
			xStream << 0u;							//   param blob bytes
		}
		xStream << uEdgeCount;
		for (u_int u = 0; u < uEdgeCount; ++u)
		{
			xStream << pxEdges[u].m_uSrc;
			xStream << pxEdges[u].m_uSrcPin;
			xStream << pxEdges[u].m_uDst;
		}
		xStream << uDataEdgeCount;
		for (u_int u = 0; u < uDataEdgeCount; ++u)
		{
			xStream << pxDataEdges[u].m_uSrc;
			xStream << std::string(pxDataEdges[u].m_szSrcPin);
			xStream << pxDataEdges[u].m_uDst;
			xStream << std::string(pxDataEdges[u].m_szDstPin);
		}
		xStream << 4u;								// layout block bytes (the count field alone)
		xStream << 0u;								// layout entry count
		xStream.SetCursor(0);
	}
}

ZENITH_TEST(GraphValidator, Validator_LoadSafetyDuplicateExecSourceIsError)
{
	EnsureValidatorTestNodesRegistered();

	const VSNode axNodes[] = { { 1u }, { 2u }, { 3u } };

	{	// control: two edges off two DIFFERENT pins
		const VSEdge axEdges[] = { { 1u, 0u, 2u }, { 1u, 1u, 3u } };
		Zenith_DataStream xStream;
		EmitValidatorGraphStream(xStream, axNodes, 3u, axEdges, 2u, nullptr, 0u);
		Zenith_GraphDefinition xDef;
		Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
		ZENITH_ASSERT_TRUE(xDef.ReadFromDataStream(xStream, &axFindings));
		ZENITH_ASSERT_EQ(axFindings.GetSize(), 0u);
	}

	const VSEdge axEdges[] = { { 1u, 0u, 2u }, { 1u, 0u, 3u } };
	Zenith_DataStream xStream;
	EmitValidatorGraphStream(xStream, axNodes, 3u, axEdges, 2u, nullptr, 0u);
	Zenith_GraphDefinition xDef;
	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	ZENITH_ASSERT_FALSE(xDef.ReadFromDataStream(xStream, &axFindings));
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_DUPLICATE_EXEC_SOURCE), 1u);
	const Zenith_GraphValidationFinding* pxFinding = FirstOfRule(axFindings, GRAPH_VALIDATION_RULE_DUPLICATE_EXEC_SOURCE);
	if (pxFinding)
	{
		ZENITH_ASSERT_TRUE(pxFinding->m_eSeverity == GRAPH_VALIDATION_SEVERITY_ERROR);
		ZENITH_ASSERT_EQ(pxFinding->m_uNodeID, 1u);
	}
}

ZENITH_TEST(GraphValidator, Validator_LoadSafetyDuplicateDataInputIsError)
{
	EnsureValidatorTestNodesRegistered();

	const VSNode axNodes[] = { { 1u }, { 2u }, { 3u } };

	{	// control: two wires into two DIFFERENT inputs
		const VSDataEdge axData[] = { { 1u, "out", 3u, "in" }, { 2u, "out", 3u, "other" } };
		Zenith_DataStream xStream;
		EmitValidatorGraphStream(xStream, axNodes, 3u, nullptr, 0u, axData, 2u);
		Zenith_GraphDefinition xDef;
		Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
		ZENITH_ASSERT_TRUE(xDef.ReadFromDataStream(xStream, &axFindings));
		ZENITH_ASSERT_EQ(axFindings.GetSize(), 0u);
	}

	const VSDataEdge axData[] = { { 1u, "out", 3u, "in" }, { 2u, "out", 3u, "in" } };
	Zenith_DataStream xStream;
	EmitValidatorGraphStream(xStream, axNodes, 3u, nullptr, 0u, axData, 2u);
	Zenith_GraphDefinition xDef;
	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	ZENITH_ASSERT_FALSE(xDef.ReadFromDataStream(xStream, &axFindings));
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_DUPLICATE_DATA_INPUT), 1u);
	const Zenith_GraphValidationFinding* pxFinding = FirstOfRule(axFindings, GRAPH_VALIDATION_RULE_DUPLICATE_DATA_INPUT);
	if (pxFinding)
	{
		ZENITH_ASSERT_TRUE(pxFinding->m_eSeverity == GRAPH_VALIDATION_SEVERITY_ERROR);
		ZENITH_ASSERT_STREQ(pxFinding->m_strPin.c_str(), "in");
	}
}

// Node id 0 is never a node, so a wire naming it can never resolve.
ZENITH_TEST(GraphValidator, Validator_LoadSafetyMalformedDataEdgeIsError)
{
	EnsureValidatorTestNodesRegistered();

	const VSNode axNodes[] = { { 1u }, { 2u } };

	{	// control: the same wire with a real source node
		const VSDataEdge axData[] = { { 1u, "out", 2u, "in" } };
		Zenith_DataStream xStream;
		EmitValidatorGraphStream(xStream, axNodes, 2u, nullptr, 0u, axData, 1u);
		Zenith_GraphDefinition xDef;
		Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
		ZENITH_ASSERT_TRUE(xDef.ReadFromDataStream(xStream, &axFindings));
		ZENITH_ASSERT_EQ(axFindings.GetSize(), 0u);
	}

	const VSDataEdge axData[] = { { 0u, "out", 2u, "in" } };
	Zenith_DataStream xStream;
	EmitValidatorGraphStream(xStream, axNodes, 2u, nullptr, 0u, axData, 1u);
	Zenith_GraphDefinition xDef;
	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	ZENITH_ASSERT_FALSE(xDef.ReadFromDataStream(xStream, &axFindings));
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_DATA_EDGE_MALFORMED), 1u);
	const Zenith_GraphValidationFinding* pxFinding = FirstOfRule(axFindings, GRAPH_VALIDATION_RULE_DATA_EDGE_MALFORMED);
	if (pxFinding)
	{
		ZENITH_ASSERT_TRUE(pxFinding->m_eSeverity == GRAPH_VALIDATION_SEVERITY_ERROR);
	}
}

// The tier CLEARS its output, like the FULL entry point - a pre-seeded finding
// must not survive a clean run.
//
// ★ RE-FIXTURED BY B-3. This used to wire two Test_ValPlain (OPAQUE) nodes with
// made-up pin names, which is now an ERROR by design - see the opaque-on-a-wire
// note in the validator header. The shape it is pinning is "a CLEAN definition",
// so it uses two ANNOTATED nodes and their real pin names.
ZENITH_TEST(GraphValidator, Validator_LoadSafetyCleanDefinitionIsEmpty)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		const u_int uSrc = xBuilder.Node("Test_ValProducer");
		const u_int uDst = xBuilder.Node("Test_ValConsumer");
		xBuilder.Edge(uSrc, 0, uDst);
		xBuilder.DataEdge(uSrc, "Result", uDst, "Value");
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}

	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
	xRegistry.EnsureInitialized();
	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	axFindings.PushBack(Zenith_GraphValidationFinding());	// pins the clear
	Zenith_GraphDefinitionValidator::ValidateLoadSafety(xDef, xRegistry, axFindings);
	ZENITH_ASSERT_EQ(axFindings.GetSize(), 0u);
}

// The two tiers agree because they share ONE body: a clean definition's FULL
// report carries none of the load-safety rules.
ZENITH_TEST(GraphValidator, Validator_LoadSafetyRulesAbsentFromCleanFullReport)
{
	EnsureValidatorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	{
		Zenith_GraphBuilder xBuilder(xDef);
		const u_int uSrc = xBuilder.Node("Test_ValProducer");
		const u_int uDst = xBuilder.Node("Test_ValConsumer");
		xBuilder.Edge(uSrc, 0, uDst);
		xBuilder.DataEdge(uSrc, "Result", uDst, "Value");
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}

	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, axFindings);
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_DUPLICATE_EXEC_SOURCE), 0u);
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_DUPLICATE_DATA_INPUT), 0u);
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_DATA_EDGE_MALFORMED), 0u);
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_DATA_CYCLE), 0u);
	AssertOnlyWireRule(axFindings, GRAPH_VALIDATION_RULE_WIRE_PIN_UNKNOWN, 0u);	// clean, not merely un-cycled
}

// ★ (b) THE SILENT SKIP. An UNREGISTERED endpoint - a per-game node library this
// exe does not carry - must not red every graph that uses it.
ZENITH_TEST(GraphValidator, Validator_UnresolvedEndpointSkippedSilently)
{
	EnsureValidatorTestNodesRegistered();

	const VSNode axNodes[] = { { 1u, "Test_ValNeverRegisteredAtAll" }, { 2u, "Test_ValConsumer" } };
	const VSDataEdge axData[] = { { 1u, "Result", 2u, "Value" } };
	Zenith_DataStream xStream;
	EmitValidatorGraphStream(xStream, axNodes, 2u, nullptr, 0u, axData, 1u);

	Zenith_GraphDefinition xDef;
	ZENITH_ASSERT_TRUE(xDef.ReadFromDataStream(xStream));

	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, axFindings);
	AssertOnlyWireRule(axFindings, GRAPH_VALIDATION_RULE_WIRE_PIN_UNKNOWN, 0u);
}

// The other half of the wired-input skip's precondition: the wire was NOT
// resolved (its source type is unregistered here), so the runtime WILL fall back
// to the var name - and the var-name check must therefore stay ON.
ZENITH_TEST(GraphValidator, Validator_WiredInputFromUnregisteredSourceStillChecksVarName)
{
	EnsureValidatorTestNodesRegistered();

	const VSNode axNodes[] = { { 1u, "Test_ValNeverRegisteredAtAll" }, { 2u, "Test_ValReader" } };
	const VSDataEdge axData[] = { { 1u, "Result", 2u, "Value" } };
	Zenith_DataStream xStream;
	EmitValidatorGraphStream(xStream, axNodes, 2u, nullptr, 0u, axData, 1u);

	Zenith_GraphDefinition xDef;
	ZENITH_ASSERT_TRUE(xDef.ReadFromDataStream(xStream));

	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	RunValidate(xDef, axFindings);
	// Test_ValReader's m_strValueVar defaults to "value", which nothing declares.
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_UNDECLARED_READ), 1u);
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_WIRE_PIN_UNKNOWN), 0u);
}

// ★ A STATIC-vs-STATIC wire mismatch is CRASH-CLASS enough to refuse the LOAD:
// the consumer can only ever take its pin default, and nothing at runtime says
// why. A refused load is LoadedOk() == false - "no graph" for every consumer.
ZENITH_TEST(GraphValidator, Validator_LoadSafetyStaticMismatchRefusedAtRead)
{
	EnsureValidatorTestNodesRegistered();

	const VSNode axNodes[] = { { 1u, "Test_ValProducer" }, { 2u, "Test_ValConsumer" } };

	{	// control: the SAME wire into the matching input reads clean
		const VSDataEdge axData[] = { { 1u, "Result", 2u, "Value" } };
		Zenith_DataStream xStream;
		EmitValidatorGraphStream(xStream, axNodes, 2u, nullptr, 0u, axData, 1u);
		Zenith_GraphDefinition xDef;
		Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
		ZENITH_ASSERT_TRUE(xDef.ReadFromDataStream(xStream, &axFindings));
		ZENITH_ASSERT_EQ(axFindings.GetSize(), 0u);
	}

	const VSDataEdge axData[] = { { 1u, "Result", 2u, "Index" } };	// FLOAT -> INT32
	Zenith_DataStream xStream;
	EmitValidatorGraphStream(xStream, axNodes, 2u, nullptr, 0u, axData, 1u);
	Zenith_GraphDefinition xDef;
	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	ZENITH_ASSERT_FALSE(xDef.ReadFromDataStream(xStream, &axFindings));
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_TYPE_MISMATCH), 1u);
	const Zenith_GraphValidationFinding* pxFinding = FirstOfRule(axFindings, GRAPH_VALIDATION_RULE_TYPE_MISMATCH);
	if (pxFinding)
	{
		ZENITH_ASSERT_TRUE(pxFinding->m_eSeverity == GRAPH_VALIDATION_SEVERITY_ERROR);
		ZENITH_ASSERT_STREQ(pxFinding->m_strPin.c_str(), "Index");
	}
}

// ★ THE TIER'S ANSWER DEPENDS ON THE REGISTERED NODE SET, and that is now
// deterministic rather than boot-phase dependent: an initialised-but-EMPTY
// registry (a build with no registrar - the Sentinel link proofs) resolves
// nothing, so the two registry-dependent checks are no-ops and the tier is
// exactly B-1's.
ZENITH_TEST(GraphValidator, Validator_LoadSafetyNoRegistryIsB1Behaviour)
{
	EnsureValidatorTestNodesRegistered();

	const VSNode axNodes[] = { { 1u, "Test_ValProducer" }, { 2u, "Test_ValConsumer" } };
	const VSDataEdge axData[] = { { 1u, "Result", 2u, "Index" } };	// FLOAT -> INT32
	Zenith_DataStream xStream;
	EmitValidatorGraphStream(xStream, axNodes, 2u, nullptr, 0u, axData, 1u);

	// Read with the LIVE registry: refused, exactly as the test above.
	Zenith_GraphDefinition xDef;
	ZENITH_ASSERT_FALSE(xDef.ReadFromDataStream(xStream));

	// The same definition, hand-built, run through an EMPTY registry: B-1's three
	// rules and nothing else.
	Zenith_GraphDefinition xBuilt;
	{
		Zenith_GraphBuilder xBuilder(xBuilt);
		const u_int uSrc = xBuilder.Node("Test_ValProducer");
		const u_int uDst = xBuilder.Node("Test_ValConsumer");
		xBuilder.DataEdge(uSrc, "Result", uDst, "Index");
		ZENITH_ASSERT_FALSE(xBuilder.Build());	// the LIVE registry still refuses it
	}
	Zenith_GraphNodeRegistry xEmpty;	// a LOCAL registry: the process-wide one is untouched
	xEmpty.EnsureInitialized();			// initialised, with no registrar and so no types
	ZENITH_ASSERT_EQ(xEmpty.GetTypeCount(), 0u);
	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	Zenith_GraphDefinitionValidator::ValidateLoadSafety(xBuilt, xEmpty, axFindings);
	ZENITH_ASSERT_EQ(axFindings.GetSize(), 0u);
}

ZENITH_TEST(GraphValidator, Validator_LoadSafetyPureCycleRefusedAtRead)
{
	EnsureValidatorTestNodesRegistered();

	const VSNode axNodes[] = { { 1u, "Test_ValPure" }, { 2u, "Test_ValPureB" } };

	{	// control: one wire, no ring
		const VSDataEdge axData[] = { { 1u, "Out", 2u, "In" } };
		Zenith_DataStream xStream;
		EmitValidatorGraphStream(xStream, axNodes, 2u, nullptr, 0u, axData, 1u);
		Zenith_GraphDefinition xDef;
		Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
		ZENITH_ASSERT_TRUE(xDef.ReadFromDataStream(xStream, &axFindings));
		ZENITH_ASSERT_EQ(axFindings.GetSize(), 0u);
	}

	const VSDataEdge axData[] = { { 1u, "Out", 2u, "In" }, { 2u, "Out", 1u, "In" } };
	Zenith_DataStream xStream;
	EmitValidatorGraphStream(xStream, axNodes, 2u, nullptr, 0u, axData, 2u);
	Zenith_GraphDefinition xDef;
	Zenith_Vector<Zenith_GraphValidationFinding> axFindings;
	ZENITH_ASSERT_FALSE(xDef.ReadFromDataStream(xStream, &axFindings));
	ZENITH_ASSERT_EQ(CountRule(axFindings, GRAPH_VALIDATION_RULE_DATA_CYCLE), 1u);
	AssertOnlyWireRule(axFindings, GRAPH_VALIDATION_RULE_WIRE_PIN_UNKNOWN, 0u);	// nothing else on those edges
}

#endif // ZENITH_TESTING
