//------------------------------------------------------------------------------
// Zenith_GraphEditorPanel unit tests.
// Included at the bottom of Zenith_EditorPanel_GraphEditor.cpp (inside its
// ZENITH_TOOLS block - the panel does not exist without it).
//
// ★ THIS DRIVES A REAL ImGui FRAME, HEADLESS, AND IS NOT requiresGraphics.
// ImGui is genuinely live on the Null backend: the context is real, layout and
// hit-testing behave exactly as they do windowed, and only the draw data is
// discarded. The precedent - and the fixture this clones - is
// Zenith_EditorPanel_Animation.Tests.inl.
//
// ★ EVERY RECT ASSERTION HAPPENS INSIDE THE OPEN FRAME. GetPinScreenPos funnels
// through IsOnScreen, which re-reads ImGui::GetIO().DisplaySize AT QUERY TIME,
// and that is (-1, -1) outside a backend NewFrame - so the same query that
// succeeds inside the frame refuses every rect outside it. The fixture restores
// the IO on the way out, which is exactly why the query cannot be moved after it.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"
#include "Scripting/Zenith_GraphPinTable.h"
#include "Scripting/Zenith_BehaviourGraph.h"

#ifdef ZENITH_TESTING

namespace
{
	//--------------------------------------------------------------------------
	// Scratch node types. Registered into the global registry under Test_ names
	// (the discipline Zenith_Scripting.Tests.inl already uses), so A-3 flags no
	// PRODUCTION node - which node library entries earn a failure pin is a
	// separate decision, made per node.
	//--------------------------------------------------------------------------

	// Fixed-pin, non-flow, FLAGGED: the only shape the registry honours the
	// failure-pin flag on.
	class GraphEditorTestFailurePinNode : public Zenith_GraphNode
	{
	public:
		GraphNodeStatus Execute(Zenith_GraphContext&) override { return GRAPH_NODE_STATUS_FAILURE; }
		const char* GetTypeName() const override { return "Test_EditorFailurePin"; }
	};

	// The control: identical in every respect except the flag.
	class GraphEditorTestPlainNode : public Zenith_GraphNode
	{
	public:
		GraphNodeStatus Execute(Zenith_GraphContext&) override { return GRAPH_NODE_STATUS_SUCCESS; }
		const char* GetTypeName() const override { return "Test_EditorPlain"; }
	};

	// Carries a PIN TABLE, so it is not opaque to the validator, and its one
	// INPUT pin reads a variable whose default name nothing declares - the
	// smallest graph that produces exactly one ERROR finding and no warnings.
	class GraphEditorTestReaderNode : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(GraphEditorTestReaderNode)
	public:
		ZENITH_PROPERTY(std::string, m_strValueVar, "editorPanelUndeclared")

		ZENITH_GRAPH_PINS_BEGIN(GraphEditorTestReaderNode)
		ZENITH_GRAPH_PIN_INPUT(Value, "m_strValueVar", PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext&) override { return GRAPH_NODE_STATUS_SUCCESS; }
		const char* GetTypeName() const override { return "Test_EditorReader"; }
	};

	//--------------------------------------------------------------------------
	// B-4 DATA-PIN fixtures. Every var-name property defaults to "" on purpose:
	// a non-empty default is an annotated READ of a variable nothing declares,
	// which would put an UNDECLARED_READ error in every graph below and make the
	// error-set diff these tests measure impossible to reason about.
	// (GraphEditorTestReaderNode above is the deliberate exception - producing
	// exactly one error IS its job.)
	//--------------------------------------------------------------------------

	// Two typed OUTPUTS, so one node offers both a matching and a mismatching
	// source for the consumer's two inputs.
	class GraphEditorTestDataProducerNode : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(GraphEditorTestDataProducerNode)
	public:
		ZENITH_PROPERTY(std::string, m_strFloatVar, "")
		ZENITH_PROPERTY(std::string, m_strIntVar, "")

		ZENITH_GRAPH_PINS_BEGIN(GraphEditorTestDataProducerNode)
		ZENITH_GRAPH_PIN_OUTPUT(F, "m_strFloatVar", PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PIN_OUTPUT(I, "m_strIntVar", PROPERTY_TYPE_INT32)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			SetOutput<float>(xContext, 0u, 1.0f);
			SetOutput<int32_t>(xContext, 1u, 1);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "Test_EditorDataProducer"; }
	};

	// TWO typed inputs + ONE output: the "2 in, 1 out" layout shape, and the
	// destination of every wire fixture.
	class GraphEditorTestDataConsumerNode : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(GraphEditorTestDataConsumerNode)
	public:
		ZENITH_PROPERTY(std::string, m_strAVar, "")
		ZENITH_PROPERTY(std::string, m_strBVar, "")
		ZENITH_PROPERTY(std::string, m_strOutVar, "")

		ZENITH_GRAPH_PINS_BEGIN(GraphEditorTestDataConsumerNode)
		ZENITH_GRAPH_PIN_INPUT(A, "m_strAVar", PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PIN_INPUT(B, "m_strBVar", PROPERTY_TYPE_INT32)
		ZENITH_GRAPH_PIN_OUTPUT(Out, "m_strOutVar", PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			// Pin A is READ on every Execute - that read is what PULLS a wired pure
			// producer, so the highlight fixture cannot pass vacuously.
			SetOutput<float>(xContext, 2u, GetInput<float>(xContext, 0u));
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "Test_EditorDataConsumer"; }
	};

	// PURE: one INPUT, one OUTPUT, no exec pins at all. The registry refuses the
	// pure flag on a type with no OUTPUT pin, which is why the table matters.
	class GraphEditorTestPureRelayNode : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(GraphEditorTestPureRelayNode)
	public:
		ZENITH_PROPERTY(std::string, m_strInVar, "")
		ZENITH_PROPERTY(std::string, m_strOutVar, "")

		ZENITH_GRAPH_PINS_BEGIN(GraphEditorTestPureRelayNode)
		ZENITH_GRAPH_PIN_INPUT(In, "m_strInVar", PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PIN_OUTPUT(Out, "m_strOutVar", PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			SetOutput<float>(xContext, 1u, GetInput<float>(xContext, 0u) + 1.0f);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "Test_EditorPureRelay"; }
	};

	// An INSTANCE-RESOLVED output: the one pin shape whose type costs
	// ResolvePinType a temp instance, which is what makes the pin cache
	// measurable at all (a static pin never calls GetPinType).
	class GraphEditorTestInstanceOutNode : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(GraphEditorTestInstanceOutNode)
	public:
		ZENITH_PROPERTY(std::string, m_strOutVar, "")
		ZENITH_PROPERTY(int32_t, m_iTypeCode, 0)

		ZENITH_GRAPH_PINS_BEGIN(GraphEditorTestInstanceOutNode)
		ZENITH_GRAPH_PIN_OUTPUT_INSTANCE(Out, "m_strOutVar")
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext&) override { return GRAPH_NODE_STATUS_SUCCESS; }
		const char* GetTypeName() const override { return "Test_EditorInstanceOut"; }
		bool GetPinType(u_int uPinIndex, Zenith_PropertyType& eOut) const override
		{
			if (uPinIndex != 0u)
			{
				return false;
			}
			eOut = m_iTypeCode == 0 ? PROPERTY_TYPE_FLOAT : PROPERTY_TYPE_INT32;
			return true;
		}
	};

	// An ON_UPDATE source, so the highlight fixture can drive a real graph
	// without borrowing another TU's scratch types.
	class GraphEditorTestUpdateSourceNode : public Zenith_GraphNode
	{
	public:
		GraphNodeStatus Execute(Zenith_GraphContext&) override { return GRAPH_NODE_STATUS_SUCCESS; }
		const char* GetTypeName() const override { return "Test_EditorOnUpdate"; }
	};

	// Idempotent via a static rather than the registry's duplicate guard, which
	// would log an error line per re-entry.
	//
	// ★ REGISTERED HERE AND NOWHERE ELSE. A registrar-time (static-init)
	// registration would put these types in every build's node library, which
	// ST_NoGameExtensionsContract only tolerates today because `zenith test` runs
	// with --skip-unit-tests.
	void EnsureGraphEditorTestNodesRegistered()
	{
		static bool ls_bRegistered = false;
		if (ls_bRegistered)
		{
			return;
		}
		ls_bRegistered = true;
		Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
		xRegistry.EnsureInitialized();
		xRegistry.RegisterNodeType<GraphEditorTestFailurePinNode>("Test_EditorFailurePin", GRAPH_EVENT_NONE, 1, false, "Test", true);
		xRegistry.RegisterNodeType<GraphEditorTestPlainNode>("Test_EditorPlain", GRAPH_EVENT_NONE, 1, false, "Test");
		xRegistry.RegisterNodeType<GraphEditorTestReaderNode>("Test_EditorReader", GRAPH_EVENT_NONE, 1, false, "Test");
		xRegistry.RegisterNodeType<GraphEditorTestDataProducerNode>("Test_EditorDataProducer", GRAPH_EVENT_NONE, 1, false, "Test");
		xRegistry.RegisterNodeType<GraphEditorTestDataConsumerNode>("Test_EditorDataConsumer", GRAPH_EVENT_NONE, 1, false, "Test");
		xRegistry.RegisterNodeType<GraphEditorTestInstanceOutNode>("Test_EditorInstanceOut", GRAPH_EVENT_NONE, 1, false, "Test");
		xRegistry.RegisterNodeType<GraphEditorTestUpdateSourceNode>("Test_EditorOnUpdate", GRAPH_EVENT_ON_UPDATE, 1, false, "Test");
		// Pure LAST, and with the flag: registration validates and refuses it
		// observably, so a test reads the stored flag rather than trusting this
		// call site.
		xRegistry.RegisterNodeType<GraphEditorTestPureRelayNode>("Test_EditorPureRelay", GRAPH_EVENT_NONE, 1, false, "Test", false, true);
	}

	//--------------------------------------------------------------------------
	// One self-contained ImGui frame - the AnimPanelImGuiFrame contract, cloned
	// rather than shared because the two .inl files are compiled into different
	// TUs and neither owns a header. NewFrame asserts the previous frame ended,
	// so the pair is RAII; everything touched on the IO is put back, because the
	// editor's own frame loop starts moments after this batch and must not
	// inherit a display size or a mouse position a test invented.
	//--------------------------------------------------------------------------
	struct GraphPanelImGuiFrame
	{
		GraphPanelImGuiFrame(float fWidth, float fHeight)
		{
			ImGuiIO& xIO = ImGui::GetIO();
			m_xSavedDisplaySize = xIO.DisplaySize;
			m_fSavedDeltaTime = xIO.DeltaTime;
			m_xSavedMousePos = xIO.MousePos;
			m_fSavedMouseWheel = xIO.MouseWheel;

			xIO.DisplaySize = ImVec2(fWidth, fHeight);
			xIO.DeltaTime = 1.0f / 60.0f;
			// Far outside ImGui's MOUSE_INVALID_MARKER: nothing is hovered and no
			// click can reach the panel, so a measurement cannot depend on where
			// the developer's real cursor happened to be.
			xIO.MousePos = ImVec2(-1.0e30f, -1.0e30f);
			xIO.MouseWheel = 0.0f;

			ImGui::NewFrame();
		}

		~GraphPanelImGuiFrame()
		{
			ImGui::EndFrame();
			ImGuiIO& xIO = ImGui::GetIO();
			xIO.DisplaySize = m_xSavedDisplaySize;
			xIO.DeltaTime = m_fSavedDeltaTime;
			xIO.MousePos = m_xSavedMousePos;
			xIO.MouseWheel = m_fSavedMouseWheel;
		}

		GraphPanelImGuiFrame(const GraphPanelImGuiFrame&) = delete;
		GraphPanelImGuiFrame& operator=(const GraphPanelImGuiFrame&) = delete;

		ImVec2 m_xSavedDisplaySize;
		ImVec2 m_xSavedMousePos;
		float m_fSavedDeltaTime = 0.0f;
		float m_fSavedMouseWheel = 0.0f;
	};

	constexpr float fGRAPHPANEL_DISPLAY_W = 1280.0f;
	constexpr float fGRAPHPANEL_DISPLAY_H = 720.0f;

	// Does the LAST validation run the panel kept carry a finding of this rule?
	// Asserted in preference to an exact finding COUNT: a count breaks the moment
	// an unrelated rule is added, and what these tests mean is "this rule fired".
	bool GraphEditorTestHasFinding(Zenith_GraphValidationRule eRule)
	{
		for (u_int u = 0; u < Zenith_GraphEditorPanel::GetValidationFindingCount(); ++u)
		{
			const Zenith_GraphValidationFinding* pxFinding = Zenith_GraphEditorPanel::GetValidationFindingAt(u);
			if (pxFinding != nullptr && pxFinding->m_eRule == eRule)
			{
				return true;
			}
		}
		return false;
	}

	// |A.channel - B.channel| for one IM_COL32 channel shift. Hand-rolled rather
	// than std::abs so this file needs no new include for one subtraction.
	int ChannelSeparation(ImU32 uA, ImU32 uB, u_int uShift)
	{
		const int iA = static_cast<int>((uA >> uShift) & 0xFFu);
		const int iB = static_cast<int>((uB >> uShift) & 0xFFu);
		return iA > iB ? iA - iB : iB - iA;
	}

	void RenderGraphPanelFrame()
	{
		GraphPanelImGuiFrame xFrame(fGRAPHPANEL_DISPLAY_W, fGRAPHPANEL_DISPLAY_H);
		// A collapsed window records no rects at all (Render early-outs after
		// Begin returns false), and collapse state can arrive from a persisted
		// imgui.ini this batch did not write. Say it explicitly rather than
		// inherit it.
		ImGui::SetNextWindowCollapsed(false);
		Zenith_GraphEditorPanel::Render();
	}
}

// The extra pin is LAID OUT and KEYED like any other output pin, and an
// unflagged type has nothing at that index - plus the ImGui-free half of the
// same contract: Action_Connect (the drag-drop completion handler) accepts the
// failure index on a flagged type and refuses it on an unflagged one.
ZENITH_TEST(GraphEditorPanel, FailurePin_EditorLaysOutAndKeysExtraPin)
{
	EnsureGraphEditorTestNodesRegistered();

	const Zenith_GraphNodeTypeInfo* pxFlagged = Zenith_GraphNodeRegistry::Get().Find("Test_EditorFailurePin");
	const Zenith_GraphNodeTypeInfo* pxPlain = Zenith_GraphNodeRegistry::Get().Find("Test_EditorPlain");
	ZENITH_ASSERT_NOT_NULL(pxFlagged);
	ZENITH_ASSERT_NOT_NULL(pxPlain);
	if (pxFlagged == nullptr || pxPlain == nullptr)
	{
		return;
	}
	ZENITH_ASSERT_TRUE(pxFlagged->m_bHasFailurePin);
	ZENITH_ASSERT_FALSE(pxPlain->m_bHasFailurePin);
	const u_int uFailurePin = pxFlagged->m_uExecOutputCount;

	// A path that does not exist on disk: OpenAsset's documented "new asset"
	// branch edits an OWNED empty definition, nothing is cached in the asset
	// registry, and Close() below frees it. No file is ever written (Save is
	// never called), so this leaves no artefact behind.
	Zenith_GraphEditorPanel::OpenAsset("game:Graphs/zz_unit_failurepin.bgraph");
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::IsOpen());

	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorFailurePin"));
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorPlain"));
	const u_int uFlaggedID = Zenith_GraphEditorPanel::FindNodeIDByType("Test_EditorFailurePin");
	const u_int uPlainID = Zenith_GraphEditorPanel::FindNodeIDByType("Test_EditorPlain");
	ZENITH_ASSERT_NE(uFlaggedID, 0u);
	ZENITH_ASSERT_NE(uPlainID, 0u);

	// Frame 1 positions the window; frame 2 is the one measured, so nothing is
	// read off a layout that is still settling.
	RenderGraphPanelFrame();
	{
		GraphPanelImGuiFrame xFrame(fGRAPHPANEL_DISPLAY_W, fGRAPHPANEL_DISPLAY_H);
		ImGui::SetNextWindowCollapsed(false);
		Zenith_GraphEditorPanel::Render();

		// Control: pin 0 of the flagged node, the pin that existed before A-3.
		Zenith_Maths::Vector2 xNormalPin;
		ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::GetPinScreenPos(uFlaggedID, 0, false, xNormalPin));

		// The failure pin is a real, keyed, on-screen output pin one row below
		// the last normal one, in the same column.
		Zenith_Maths::Vector2 xFailPin;
		ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::GetPinScreenPos(uFlaggedID, uFailurePin, false, xFailPin));
		ZENITH_ASSERT_EQ_FLOAT(xFailPin.x, xNormalPin.x, 0.001f);
		ZENITH_ASSERT_GT(xFailPin.y, xNormalPin.y);

		// The unflagged control node has the SAME static pin count and renders
		// nothing at that index - so the extra pin follows the flag, not the
		// index.
		Zenith_Maths::Vector2 xPlainPin;
		ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::GetPinScreenPos(uPlainID, 0, false, xPlainPin));
		Zenith_Maths::Vector2 xAbsentPin;
		ZENITH_ASSERT_FALSE(Zenith_GraphEditorPanel::GetPinScreenPos(uPlainID, pxPlain->m_uExecOutputCount, false, xAbsentPin));
	}

	// The ImGui-free seam: connect validation reads the same funnel the drawing
	// does, so authoring a failure wire is possible exactly where a failure pin
	// is drawn.
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_Connect("Test_EditorFailurePin", 0, uFailurePin, "Test_EditorPlain", 0));
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetEdgeCount(), 1u);
	ZENITH_ASSERT_FALSE(Zenith_GraphEditorPanel::Action_Connect("Test_EditorPlain", 0, pxPlain->m_uExecOutputCount, "Test_EditorFailurePin", 0));
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetEdgeCount(), 1u);

	Zenith_GraphEditorPanel::Close();
	ZENITH_ASSERT_FALSE(Zenith_GraphEditorPanel::IsOpen());
}

// ★ A REFUSED CONNECTION NOW SAYS SO. The canvas drop handler used to call
// AddEdge inline with no else branch at all: a rejected drag changed nothing and
// printed nothing, which is indistinguishable from a missed drop. TryConnect is
// the ImGui-free body that drop handler now runs, so driving it here is the only
// way a headless unit can say anything about the human gesture.
ZENITH_TEST(GraphEditorPanel, GraphEditor_DropRefusalIsVisible)
{
	EnsureGraphEditorTestNodesRegistered();

	Zenith_GraphEditorPanel::OpenAsset("game:Graphs/zz_unit_refusal.bgraph");
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::IsOpen());
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorPlain"));
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorFailurePin"));
	const u_int uPlainID = Zenith_GraphEditorPanel::FindNodeIDByType("Test_EditorPlain");
	const u_int uFlaggedID = Zenith_GraphEditorPanel::FindNodeIDByType("Test_EditorFailurePin");
	ZENITH_ASSERT_NE(uPlainID, 0u);
	ZENITH_ASSERT_NE(uFlaggedID, 0u);

	// A freshly opened asset has nothing to refuse.
	ZENITH_ASSERT_STREQ(Zenith_GraphEditorPanel::GetConnectRefusalText(), "");

	// A drop whose source pin the node does not have.
	ZENITH_ASSERT_FALSE(TryConnect(uPlainID, 3, uFlaggedID));
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetEdgeCount(), 0u);
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::GetConnectRefusalText()[0] != '\0');

	// A drop naming a node that is not in this graph - and it must be THAT
	// refusal, not AddEdge's generic one (which would also fire).
	ZENITH_ASSERT_FALSE(TryConnect(uPlainID, 0, 4242u));
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetEdgeCount(), 0u);
	ZENITH_ASSERT_NOT_NULL(strstr(Zenith_GraphEditorPanel::GetConnectRefusalText(), "not in this graph"));

	Zenith_GraphEditorPanel::Close();
	// Closing the asset drops the refusal with it.
	ZENITH_ASSERT_STREQ(Zenith_GraphEditorPanel::GetConnectRefusalText(), "");
}

// The canvas drop and the atomic Action_Connect used to be DIVERGENT COPIES -
// only one of them checked the pin range. They are one body now, and it is the
// same exec-output funnel the canvas draws with.
ZENITH_TEST(GraphEditorPanel, GraphEditor_TryConnectIsTheOneFunnel)
{
	EnsureGraphEditorTestNodesRegistered();

	const Zenith_GraphNodeTypeInfo* pxFlagged = Zenith_GraphNodeRegistry::Get().Find("Test_EditorFailurePin");
	ZENITH_ASSERT_NOT_NULL(pxFlagged);
	if (pxFlagged == nullptr)
	{
		return;
	}
	const u_int uFailurePin = pxFlagged->m_uExecOutputCount;

	Zenith_GraphEditorPanel::OpenAsset("game:Graphs/zz_unit_funnel.bgraph");
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorFailurePin"));
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorPlain"));
	const u_int uFlaggedID = Zenith_GraphEditorPanel::FindNodeIDByType("Test_EditorFailurePin");
	const u_int uPlainID = Zenith_GraphEditorPanel::FindNodeIDByType("Test_EditorPlain");

	// The same INDEX: absent on the unflagged type, present on the flagged one.
	ZENITH_ASSERT_FALSE(TryConnect(uPlainID, uFailurePin, uFlaggedID));
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::GetConnectRefusalText()[0] != '\0');

	ZENITH_ASSERT_TRUE(TryConnect(uFlaggedID, uFailurePin, uPlainID));
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetEdgeCount(), 1u);
	// A connect that LANDS clears the refusal - the text is about the last
	// attempt, not a latch nobody can reset.
	ZENITH_ASSERT_STREQ(Zenith_GraphEditorPanel::GetConnectRefusalText(), "");

	// The one-outgoing-edge-per-(node, pin) rule, refused through the same body.
	ZENITH_ASSERT_FALSE(TryConnect(uFlaggedID, uFailurePin, uPlainID));
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetEdgeCount(), 1u);
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::GetConnectRefusalText()[0] != '\0');

	// Action_Connect resolves (type, occurrence) and then runs the SAME body.
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_Connect("Test_EditorFailurePin", 0, 0, "Test_EditorPlain", 0));
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetEdgeCount(), 2u);
	ZENITH_ASSERT_STREQ(Zenith_GraphEditorPanel::GetConnectRefusalText(), "");

	Zenith_GraphEditorPanel::Close();
}

// ★ THE PANEL IS ADVISORY, AND STAYS ADVISORY NOW THAT ERRORS ARE ERRORS.
// A-8 latched the validator so a boot-authoring Build() FAILS on an undeclared
// read - but the editor is where an author FIXES one, so refusing to open, edit
// or save a graph with errors in it would trap them inside the mistake. This
// pins both halves: the count says ERROR, and every gesture still works.
ZENITH_TEST(GraphEditorPanel, GraphEditor_ValidationErrorsAreAdvisory)
{
	EnsureGraphEditorTestNodesRegistered();

	Zenith_GraphEditorPanel::OpenAsset("game:Graphs/zz_unit_validation_error.bgraph");
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::IsOpen());

	// A fresh, empty definition validates clean - so the count below is the
	// node's doing and not a leftover from another test's asset.
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetValidationFindingCount(), 0u);
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetValidationErrorCount(), 0u);

	// One annotated node reading a variable nothing declares. The pass re-runs
	// on a COMMITTED PARAMETER EDIT (not on Action_AddNode), so the var-name
	// edit below is what refreshes the report - which is also the edit that
	// breaks a binding in real use.
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorReader"));
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_SelectNode("Test_EditorReader", 0));
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_SetSelectedNodeParamString("m_strValueVar", "stillUndeclared"));

	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetValidationFindingCount(), 1u);
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetValidationErrorCount(), 1u);

	const Zenith_GraphValidationFinding* pxFinding = Zenith_GraphEditorPanel::GetValidationFindingAt(0);
	ZENITH_ASSERT_NOT_NULL(pxFinding);
	if (pxFinding != nullptr)
	{
		ZENITH_ASSERT_TRUE(pxFinding->m_eSeverity == GRAPH_VALIDATION_SEVERITY_ERROR);
		ZENITH_ASSERT_TRUE(pxFinding->m_eRule == GRAPH_VALIDATION_RULE_UNDECLARED_READ);
		ZENITH_ASSERT_STREQ(pxFinding->m_strVar.c_str(), "stillUndeclared");
	}
	// An out-of-range index is a null, not an assert.
	ZENITH_ASSERT_NULL(Zenith_GraphEditorPanel::GetValidationFindingAt(1));

	// ADVISORY: the panel is still open and every Action_* still lands with an
	// ERROR standing.
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::IsOpen());
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddVariable("stillUndeclared", "float", 0.0f));
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorPlain"));
	// A landed connect is the other re-validation trigger.
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_Connect("Test_EditorReader", 0, 0, "Test_EditorPlain", 0));

	// ...and the declaration CLEARED the error, which is the proof the pass
	// re-runs rather than latching its first answer. (Test_EditorPlain carries
	// no pin table, so it is OPAQUE and the declared-but-unreferenced warning
	// is suppressed for this graph - hence zero findings, not just zero
	// errors.)
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetValidationErrorCount(), 0u);
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetValidationFindingCount(), 0u);

	Zenith_GraphEditorPanel::Close();
	ZENITH_ASSERT_FALSE(Zenith_GraphEditorPanel::IsOpen());
}

//==============================================================================
// B-4 DATA PINS
//
// ★ WHICH ASSERTIONS MUST BE INSIDE THE OPEN FRAME, AND WHY. Anything reading a
// RECT (GetPinScreenPos, GetDataPinScreenPos, GetNodeScreenRect) or a
// last-frame counter goes inside, because those funnel through IsOnScreen, which
// re-reads GetIO().DisplaySize AT QUERY TIME - and the fixture restores the IO on
// the way out, so the identical query refuses every rect once the frame has
// ended. Frame 1 settles the window; frame 2 is the one measured. The funnel and
// refusal tests need no frame at all: TryConnectData and TryDisconnectData are
// ImGui-free on purpose, which is the only reason a headless unit can say
// anything about a human's drag.
//==============================================================================

// Data pins are laid out on their own rows, keyed distinctly from exec pins, and
// the node box grows to hold them.
ZENITH_TEST(GraphEditorPanel, GraphEditor_DataPinsLaidOutAndKeyed)
{
	EnsureGraphEditorTestNodesRegistered();

	// The KIND BIT, asserted on the key function itself: without it a data pin at
	// ordinal 0 and the exec input would collide in m_xPinRects and one would
	// silently overwrite the other.
	ZENITH_ASSERT_NE(MakePinKey(7u, 0u, true, false), MakePinKey(7u, 0u, true, true));
	ZENITH_ASSERT_NE(MakePinKey(7u, 0u, false, false), MakePinKey(7u, 0u, false, true));
	// ...and the node id still separates two nodes now that the shift moved.
	ZENITH_ASSERT_NE(MakePinKey(7u, 0u, true, true), MakePinKey(8u, 0u, true, true));

	Zenith_GraphEditorPanel::OpenAsset("game:Graphs/zz_unit_datapins.bgraph");
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorDataConsumer"));
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorPlain"));
	const u_int uConsumer = Zenith_GraphEditorPanel::FindNodeIDByType("Test_EditorDataConsumer");
	const u_int uPlain = Zenith_GraphEditorPanel::FindNodeIDByType("Test_EditorPlain");
	ZENITH_ASSERT_NE(uConsumer, 0u);
	ZENITH_ASSERT_NE(uPlain, 0u);

	RenderGraphPanelFrame();
	{
		GraphPanelImGuiFrame xFrame(fGRAPHPANEL_DISPLAY_W, fGRAPHPANEL_DISPLAY_H);
		ImGui::SetNextWindowCollapsed(false);
		Zenith_GraphEditorPanel::Render();

		// Every declared INPUT/OUTPUT pin has a rect, found BY NAME.
		Zenith_Maths::Vector2 xA, xB, xOut, xExecIn;
		ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::GetDataPinScreenPos(uConsumer, "A", true, xA));
		ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::GetDataPinScreenPos(uConsumer, "B", true, xB));
		ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::GetDataPinScreenPos(uConsumer, "Out", false, xOut));
		ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::GetPinScreenPos(uConsumer, 0, true, xExecIn));

		// TWO data inputs, TWO distinct rects - not one item hit-testing for both
		// (which is what a shared ImGui id would produce).
		ZENITH_ASSERT_GT(xB.y, xA.y);
		// Inputs left, outputs right, data inputs BELOW the exec input (which keeps
		// row 0 so every existing exec test still reads the same place).
		ZENITH_ASSERT_GT(xOut.x, xA.x);
		ZENITH_ASSERT_EQ_FLOAT(xA.x, xExecIn.x, 0.001f);
		ZENITH_ASSERT_GT(xA.y, xExecIn.y);

		// A pin name the type does not declare, and a role that is never drawn,
		// both answer false rather than a coordinate.
		Zenith_Maths::Vector2 xAbsent;
		ZENITH_ASSERT_FALSE(Zenith_GraphEditorPanel::GetDataPinScreenPos(uConsumer, "NoSuchPin", true, xAbsent));
		// "Out" is an OUTPUT: it is not an input pin, whatever the name says.
		ZENITH_ASSERT_FALSE(Zenith_GraphEditorPanel::GetDataPinScreenPos(uConsumer, "Out", true, xAbsent));

		// THE BOX GREW. Compared against a node with no pin table rather than
		// against a recomputed formula - a test that re-derives the arithmetic
		// asserts the formula against itself.
		Zenith_Maths::Vector2 xConsumerMin, xConsumerMax, xPlainMin, xPlainMax;
		ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::GetNodeScreenRect(uConsumer, xConsumerMin, xConsumerMax));
		ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::GetNodeScreenRect(uPlain, xPlainMin, xPlainMax));
		ZENITH_ASSERT_GT(xConsumerMax.y - xConsumerMin.y, xPlainMax.y - xPlainMin.y);
		// ...and the lowest pin is still INSIDE the box.
		ZENITH_ASSERT_GT(xConsumerMax.y, xB.y);

		// A node with NO pin table draws exactly what it did before B-4.
		Zenith_Maths::Vector2 xPlainPin;
		ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::GetPinScreenPos(uPlain, 0, true, xPlainPin));
		ZENITH_ASSERT_FALSE(Zenith_GraphEditorPanel::GetDataPinScreenPos(uPlain, "A", true, xAbsent));
	}

	Zenith_GraphEditorPanel::Close();
}

// ★ A PURE NODE HAS NO EXEC PINS, so the panel draws none - and the refusal is
// paired with a POSITIVE on the same node in the same frame, because "no rect"
// is also what an off-screen node, a collapsed window and a typo produce.
ZENITH_TEST(GraphEditorPanel, GraphEditor_PureNodeDrawsNoExecPins)
{
	EnsureGraphEditorTestNodesRegistered();

	const Zenith_GraphNodeTypeInfo* pxPure = Zenith_GraphNodeRegistry::Get().Find("Test_EditorPureRelay");
	const Zenith_GraphNodeTypeInfo* pxPlain = Zenith_GraphNodeRegistry::Get().Find("Test_EditorPlain");
	ZENITH_ASSERT_NOT_NULL(pxPure);
	ZENITH_ASSERT_NOT_NULL(pxPlain);
	if (pxPure == nullptr || pxPlain == nullptr)
	{
		return;
	}
	// The registry VALIDATES the pure flag and refuses it observably, so this
	// reads the stored flag rather than trusting the registration call.
	ZENITH_ASSERT_TRUE(pxPure->m_bPureNode);
	ZENITH_ASSERT_EQ(pxPure->m_uExecOutputCount, 0u);

	// A pure node LOOKS different because it BEHAVES differently.
	ZENITH_ASSERT_NE(NodeHeaderColour(pxPure), NodeHeaderColour(pxPlain));
	ZENITH_ASSERT_NE(NodeHeaderColour(pxPure), NodeHeaderColour(nullptr));
	ZENITH_ASSERT_EQ(NodeHeaderColour(pxPure), IM_COL32(130, 90, 190, 255));

	Zenith_GraphEditorPanel::OpenAsset("game:Graphs/zz_unit_purepins.bgraph");
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorPureRelay"));
	const u_int uPureID = Zenith_GraphEditorPanel::FindNodeIDByType("Test_EditorPureRelay");
	ZENITH_ASSERT_NE(uPureID, 0u);

	RenderGraphPanelFrame();
	{
		GraphPanelImGuiFrame xFrame(fGRAPHPANEL_DISPLAY_W, fGRAPHPANEL_DISPLAY_H);
		ImGui::SetNextWindowCollapsed(false);
		Zenith_GraphEditorPanel::Render();

		// The POSITIVE first: this node really was drawn, on screen, this frame.
		Zenith_Maths::Vector2 xIn, xOut;
		ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::GetDataPinScreenPos(uPureID, "In", true, xIn));
		ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::GetDataPinScreenPos(uPureID, "Out", false, xOut));

		// ...and it has no exec pin on either side.
		Zenith_Maths::Vector2 xExec;
		ZENITH_ASSERT_FALSE(Zenith_GraphEditorPanel::GetPinScreenPos(uPureID, 0, true, xExec));
		ZENITH_ASSERT_FALSE(Zenith_GraphEditorPanel::GetPinScreenPos(uPureID, 0, false, xExec));

		// Its first DATA input takes row 0, the row an exec input would have had.
		Zenith_Maths::Vector2 xMin, xMax;
		ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::GetNodeScreenRect(uPureID, xMin, xMax));
		ZENITH_ASSERT_EQ_FLOAT(xIn.y, xOut.y, 0.001f);
	}

	Zenith_GraphEditorPanel::Close();
}

// A data wire lands through ONE funnel, and Action_ConnectData runs that funnel.
ZENITH_TEST(GraphEditorPanel, GraphEditor_DataWireConnectsThroughFunnel)
{
	EnsureGraphEditorTestNodesRegistered();

	Zenith_GraphEditorPanel::OpenAsset("game:Graphs/zz_unit_datawire.bgraph");
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorDataProducer"));
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorDataConsumer"));
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetDataEdgeCount(), 0u);

	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_ConnectData(
		"Test_EditorDataProducer", 0, "F", "Test_EditorDataConsumer", 0, "A"));
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetDataEdgeCount(), 1u);
	// A wire that LANDS clears the refusal and dirties the asset.
	ZENITH_ASSERT_STREQ(Zenith_GraphEditorPanel::GetConnectRefusalText(), "");
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::IsDirty());
	// The report it kept describes the graph WITH the wire: no errors.
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetValidationErrorCount(), 0u);

	// The EXEC edge count is untouched: the two graphs are separate.
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetEdgeCount(), 0u);

	Zenith_GraphEditorPanel::Close();
}

// ★ A TYPE-MISMATCHED WIRE IS REFUSED AND SAYS WHY. FLOAT -> INT32 is a wire
// whose consumer could only ever read its pin default, and nothing at runtime
// would say so.
ZENITH_TEST(GraphEditorPanel, GraphEditor_DataWireTypeMismatchRefusedVisibly)
{
	EnsureGraphEditorTestNodesRegistered();

	Zenith_GraphEditorPanel::OpenAsset("game:Graphs/zz_unit_datamismatch.bgraph");
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorDataProducer"));
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorDataConsumer"));

	ZENITH_ASSERT_FALSE(Zenith_GraphEditorPanel::Action_ConnectData(
		"Test_EditorDataProducer", 0, "F", "Test_EditorDataConsumer", 0, "B"));
	// REVERTED, not merely reported: the definition must not keep a wire the
	// author was told they could not have.
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetDataEdgeCount(), 0u);
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::GetConnectRefusalText()[0] != '\0');
	// The text NAMES THE RULE - "refused" alone leaves the author guessing.
	ZENITH_ASSERT_NOT_NULL(strstr(Zenith_GraphEditorPanel::GetConnectRefusalText(), "TYPE_MISMATCH"));
	// ...and the displayed report describes the definition WITHOUT the reverted
	// wire, so no error is left standing for a wire that is not there.
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetValidationErrorCount(), 0u);

	// The matching pin on the same producer still works, which is what makes the
	// refusal above about the TYPE rather than about the nodes.
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_ConnectData(
		"Test_EditorDataProducer", 0, "F", "Test_EditorDataConsumer", 0, "A"));
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetDataEdgeCount(), 1u);

	Zenith_GraphEditorPanel::Close();
}

// One incoming wire per INPUT - the data-graph invariant, refused through the
// same funnel and said out loud.
ZENITH_TEST(GraphEditorPanel, GraphEditor_SecondWireIntoInputRefused)
{
	EnsureGraphEditorTestNodesRegistered();

	Zenith_GraphEditorPanel::OpenAsset("game:Graphs/zz_unit_datasecond.bgraph");
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorDataProducer"));
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorDataProducer"));
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorDataConsumer"));

	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_ConnectData(
		"Test_EditorDataProducer", 0, "F", "Test_EditorDataConsumer", 0, "A"));
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetDataEdgeCount(), 1u);

	// A SECOND producer into the same input.
	ZENITH_ASSERT_FALSE(Zenith_GraphEditorPanel::Action_ConnectData(
		"Test_EditorDataProducer", 1, "F", "Test_EditorDataConsumer", 0, "A"));
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetDataEdgeCount(), 1u);
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::GetConnectRefusalText()[0] != '\0');

	// By contrast a DIFFERENT source pin into a different input lands (the refusal
	// is per INPUT, not per node). Unbounded fan-out from ONE output pin is pinned
	// at the definition level by Definition_DataEdgeFanOutFromOneOutputIsUnbounded
	// (Zenith_Scripting.Tests.inl); the panel adds no rule of its own.
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_ConnectData(
		"Test_EditorDataProducer", 0, "I", "Test_EditorDataConsumer", 0, "B"));
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetDataEdgeCount(), 2u);

	Zenith_GraphEditorPanel::Close();
}

// ★ THE REFUSAL IS AN ERROR-SET DIFFERENCE, NOT AN ATTRIBUTION MATCH, and this
// is the case that proves it: the finding names the SOURCE node and the SOURCE
// pin, so a check for "an error on the destination pin I just dropped on" would
// let this wire through.
ZENITH_TEST(GraphEditorPanel, GraphEditor_DataWireRefusedOnSourceSideError)
{
	EnsureGraphEditorTestNodesRegistered();

	Zenith_GraphEditorPanel::OpenAsset("game:Graphs/zz_unit_datasrcerror.bgraph");
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorDataProducer"));
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorDataConsumer"));

	ZENITH_ASSERT_FALSE(Zenith_GraphEditorPanel::Action_ConnectData(
		"Test_EditorDataProducer", 0, "NoSuchOutput", "Test_EditorDataConsumer", 0, "A"));
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetDataEdgeCount(), 0u);
	ZENITH_ASSERT_NOT_NULL(strstr(Zenith_GraphEditorPanel::GetConnectRefusalText(), "WIRE_PIN_UNKNOWN"));

	// An empty pin name never reaches the definition at all.
	ZENITH_ASSERT_FALSE(Zenith_GraphEditorPanel::Action_ConnectData(
		"Test_EditorDataProducer", 0, "F", "Test_EditorDataConsumer", 0, ""));
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetDataEdgeCount(), 0u);
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::GetConnectRefusalText()[0] != '\0');

	// A wire whose SOURCE pin is a real pin of the wrong ROLE - an INPUT - is
	// refused too, and that finding also names the SOURCE. (Two DIFFERENT consumer
	// nodes: one node feeding its own input is a self-loop, which the definition
	// refuses before any pin is looked at.)
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorDataConsumer"));
	ZENITH_ASSERT_FALSE(Zenith_GraphEditorPanel::Action_ConnectData(
		"Test_EditorDataConsumer", 0, "A", "Test_EditorDataConsumer", 1, "A"));
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetDataEdgeCount(), 0u);
	ZENITH_ASSERT_NOT_NULL(strstr(Zenith_GraphEditorPanel::GetConnectRefusalText(), "WIRE_ROLE_MISMATCH"));

	Zenith_GraphEditorPanel::Close();
}

// ...and the other half of the same rule: a PRE-EXISTING error on the
// destination node must not revert a good wire. An attribution match would have
// reverted this one, which would make an error anywhere on a node permanently
// un-wireable - exactly when an author most needs to wire it.
ZENITH_TEST(GraphEditorPanel, GraphEditor_DataWireLandsDespitePreexistingErrorOnDestination)
{
	EnsureGraphEditorTestNodesRegistered();

	Zenith_GraphEditorPanel::OpenAsset("game:Graphs/zz_unit_datapreexisting.bgraph");
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorDataProducer"));
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorDataConsumer"));

	// Pin B of the consumer now reads a variable nothing declares: one ERROR,
	// standing, on the node this test then wires INTO.
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_SelectNode("Test_EditorDataConsumer", 0));
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_SetSelectedNodeParamString("m_strBVar", "editorUndeclaredB"));
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetValidationErrorCount(), 1u);

	// A GOOD wire into pin A of that same node.
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_ConnectData(
		"Test_EditorDataProducer", 0, "F", "Test_EditorDataConsumer", 0, "A"));
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetDataEdgeCount(), 1u);
	ZENITH_ASSERT_STREQ(Zenith_GraphEditorPanel::GetConnectRefusalText(), "");
	// The pre-existing error is still reported - ADVISORY, as ever. It was never
	// this wire's fault and it is not this wire's to clear.
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetValidationErrorCount(), 1u);

	Zenith_GraphEditorPanel::Close();
}

// ★ A WIRE THAT CLOSES A DATA CYCLE IS REFUSED even though the finding names
// neither endpoint reliably: DATA_CYCLE is attributed to the DFS entry node. It
// is a NEW error, which is the only thing the funnel asks.
ZENITH_TEST(GraphEditorPanel, GraphEditor_DataWireClosingCycleRefused)
{
	EnsureGraphEditorTestNodesRegistered();

	Zenith_GraphEditorPanel::OpenAsset("game:Graphs/zz_unit_datacycle.bgraph");
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorPureRelay"));
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorPureRelay"));

	// relay0 -> relay1 lands (a pure chain is perfectly legal).
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_ConnectData(
		"Test_EditorPureRelay", 0, "Out", "Test_EditorPureRelay", 1, "In"));
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetDataEdgeCount(), 1u);

	// relay1 -> relay0 closes the ring: unbounded recursion on paper, a defaulted
	// read plus a warning at runtime - which is exactly why it is caught here.
	ZENITH_ASSERT_FALSE(Zenith_GraphEditorPanel::Action_ConnectData(
		"Test_EditorPureRelay", 1, "Out", "Test_EditorPureRelay", 0, "In"));
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetDataEdgeCount(), 1u);
	ZENITH_ASSERT_NOT_NULL(strstr(Zenith_GraphEditorPanel::GetConnectRefusalText(), "DATA_CYCLE"));

	Zenith_GraphEditorPanel::Close();
}

// ONE disconnect funnel, keyed by the DESTINATION - and it re-validates, which
// is observable because a pure node with no outgoing wire warns.
ZENITH_TEST(GraphEditorPanel, GraphEditor_DataWireDisconnectByDestination)
{
	EnsureGraphEditorTestNodesRegistered();

	Zenith_GraphEditorPanel::OpenAsset("game:Graphs/zz_unit_datadisconnect.bgraph");
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorPureRelay"));
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorDataConsumer"));

	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_ConnectData(
		"Test_EditorPureRelay", 0, "Out", "Test_EditorDataConsumer", 0, "A"));
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetDataEdgeCount(), 1u);
	// CONSUMED: the pure node has an outgoing wire, so it can be evaluated - and
	// the report the connect left behind says so.
	ZENITH_ASSERT_FALSE(GraphEditorTestHasFinding(GRAPH_VALIDATION_RULE_PURE_UNCONSUMED));

	// A destination pin that carries no wire changes nothing and says so.
	ZENITH_ASSERT_FALSE(Zenith_GraphEditorPanel::Action_DisconnectData("Test_EditorDataConsumer", 0, "B"));
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetDataEdgeCount(), 1u);

	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_DisconnectData("Test_EditorDataConsumer", 0, "A"));
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetDataEdgeCount(), 0u);
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::IsDirty());
	// ★ THE ACTION PATH RE-VALIDATED. The pure node is unconsumed again, and only
	// a fresh run over the definition WITHOUT the wire could say so.
	ZENITH_ASSERT_TRUE(GraphEditorTestHasFinding(GRAPH_VALIDATION_RULE_PURE_UNCONSUMED));

	Zenith_GraphEditorPanel::Close();
}

// A drag started on an EXEC output, dropped on a DATA input: refused with text
// rather than silently connected as the wrong kind of wire.
ZENITH_TEST(GraphEditorPanel, GraphEditor_ExecLinkOnDataPinRefused)
{
	EnsureGraphEditorTestNodesRegistered();

	Zenith_GraphEditorPanel::OpenAsset("game:Graphs/zz_unit_execondata.bgraph");
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorPlain"));
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorDataConsumer"));
	const u_int uPlain = Zenith_GraphEditorPanel::FindNodeIDByType("Test_EditorPlain");
	const u_int uConsumer = Zenith_GraphEditorPanel::FindNodeIDByType("Test_EditorDataConsumer");

	// The drag-start funnel the exec-output handler runs: no data pin named.
	BeginPendingLink(uPlain, 0, nullptr);
	ZENITH_ASSERT_FALSE(CompletePendingLinkOnDataInput(uConsumer, "A"));
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetDataEdgeCount(), 0u);
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetEdgeCount(), 0u);
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::GetConnectRefusalText()[0] != '\0');
	// The drag ENDED: a refused drop that left the rubber band attached would read
	// as a hung UI.
	ZENITH_ASSERT_FALSE(g_xGraphEditor.m_bLinking);

	Zenith_GraphEditorPanel::Close();
}

// ...and the mirror image: a DATA drag dropped on an exec input.
ZENITH_TEST(GraphEditorPanel, GraphEditor_DataLinkOnExecPinRefused)
{
	EnsureGraphEditorTestNodesRegistered();

	Zenith_GraphEditorPanel::OpenAsset("game:Graphs/zz_unit_dataonexec.bgraph");
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorDataProducer"));
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorPlain"));
	const u_int uProducer = Zenith_GraphEditorPanel::FindNodeIDByType("Test_EditorDataProducer");
	const u_int uPlain = Zenith_GraphEditorPanel::FindNodeIDByType("Test_EditorPlain");

	BeginPendingLink(uProducer, 0, "F");
	ZENITH_ASSERT_FALSE(CompletePendingLinkOnExecInput(uPlain));
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetEdgeCount(), 0u);
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetDataEdgeCount(), 0u);
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::GetConnectRefusalText()[0] != '\0');
	ZENITH_ASSERT_FALSE(g_xGraphEditor.m_bLinking);

	// The SAME drag onto a real data input lands - which is what makes the refusal
	// above about the PIN KIND and not about the wire.
	BeginPendingLink(uProducer, 0, "F");
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorDataConsumer"));
	const u_int uConsumer = Zenith_GraphEditorPanel::FindNodeIDByType("Test_EditorDataConsumer");
	ZENITH_ASSERT_TRUE(CompletePendingLinkOnDataInput(uConsumer, "A"));
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetDataEdgeCount(), 1u);

	Zenith_GraphEditorPanel::Close();
}

// ★ EXEC_INTO_PURE MUST NOT BE AUTHORABLE. A pure node draws no exec input, so a
// mouse cannot reach it - but Action_Connect can, and B-3 makes such an edge an
// ERROR that the runtime silently DROPS. The check lives in the funnel for
// exactly that reason.
ZENITH_TEST(GraphEditorPanel, GraphEditor_ExecIntoPureRefused)
{
	EnsureGraphEditorTestNodesRegistered();

	Zenith_GraphEditorPanel::OpenAsset("game:Graphs/zz_unit_execintopure.bgraph");
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorPlain"));
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorPureRelay"));

	ZENITH_ASSERT_FALSE(Zenith_GraphEditorPanel::Action_Connect("Test_EditorPlain", 0, 0, "Test_EditorPureRelay", 0));
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetEdgeCount(), 0u);
	ZENITH_ASSERT_NOT_NULL(strstr(Zenith_GraphEditorPanel::GetConnectRefusalText(), "PURE"));

	// The same source into a non-pure destination still connects.
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorFailurePin"));
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_Connect("Test_EditorPlain", 0, 0, "Test_EditorFailurePin", 0));
	ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetEdgeCount(), 1u);

	Zenith_GraphEditorPanel::Close();
}

// ★ A WIRE WHOSE ENDPOINT PIN CANNOT BE LOCATED IS STILL DRAWN. A loaded asset
// can carry one (the panel refuses to author it), and an invisible wire means the
// canvas disagrees with the file while the findings panel names a pin the author
// cannot see. The fallback is COUNTED so this is an observable rather than a
// claim about pixels.
ZENITH_TEST(GraphEditorPanel, GraphEditor_UnresolvableDataEdgeStillDrawn)
{
	EnsureGraphEditorTestNodesRegistered();

	Zenith_GraphEditorPanel::OpenAsset("game:Graphs/zz_unit_databroken.bgraph");
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorDataProducer"));
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorDataConsumer"));
	const u_int uProducer = Zenith_GraphEditorPanel::FindNodeIDByType("Test_EditorDataProducer");
	const u_int uConsumer = Zenith_GraphEditorPanel::FindNodeIDByType("Test_EditorDataConsumer");

	// Hand-built, straight on the definition: this is the shape a LOADED asset
	// carries, and AddDataEdge makes no judgement about pin names by design.
	Zenith_GraphDefinition* pxDef = GetOpenDefinition();
	ZENITH_ASSERT_NOT_NULL(pxDef);
	if (pxDef == nullptr)
	{
		return;
	}
	ZENITH_ASSERT_TRUE(pxDef->AddDataEdge(uProducer, "F", uConsumer, "PinThatMovedAway"));
	ValidateOpenGraph();

	// The findings panel names it...
	ZENITH_ASSERT_TRUE(GraphEditorTestHasFinding(GRAPH_VALIDATION_RULE_WIRE_PIN_UNKNOWN));

	RenderGraphPanelFrame();
	{
		GraphPanelImGuiFrame xFrame(fGRAPHPANEL_DISPLAY_W, fGRAPHPANEL_DISPLAY_H);
		ImGui::SetNextWindowCollapsed(false);
		Zenith_GraphEditorPanel::Render();

		// ...and the canvas DREW it, exactly once, as the fallback.
		ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetUnresolvableEdgeDrawCountForTest(), 1u);
		// The nodes themselves rendered normally alongside it.
		Zenith_Maths::Vector2 xPin;
		ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::GetDataPinScreenPos(uProducer, "F", false, xPin));
	}

	// A RESOLVABLE wire uses no fallback, which is what stops the count above
	// from being true of every frame.
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_DisconnectData("Test_EditorDataConsumer", 0, "PinThatMovedAway"));
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_ConnectData(
		"Test_EditorDataProducer", 0, "F", "Test_EditorDataConsumer", 0, "A"));
	RenderGraphPanelFrame();
	{
		GraphPanelImGuiFrame xFrame(fGRAPHPANEL_DISPLAY_W, fGRAPHPANEL_DISPLAY_H);
		ImGui::SetNextWindowCollapsed(false);
		Zenith_GraphEditorPanel::Render();
		ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetUnresolvableEdgeDrawCountForTest(), 0u);
	}

	Zenith_GraphEditorPanel::Close();
}

// ★ THE RESOLVER IS NOT QUERIED PER FRAME. ResolvePinType ALLOCATES a temp
// instance per instance-resolved pin (its own header says so), and
// GetDynamicDataInputCount needs one too - so a naive panel would allocate per
// pin per frame. The observable is the CACHE FILL count, in DELTAS: the absolute
// number is shared with every other test in this batch.
ZENITH_TEST(GraphEditorPanel, GraphEditor_PinTypeCacheFilledOncePerInvalidation)
{
	EnsureGraphEditorTestNodesRegistered();

	Zenith_GraphEditorPanel::OpenAsset("game:Graphs/zz_unit_pincache.bgraph");
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_AddNode("Test_EditorInstanceOut"));

	RenderGraphPanelFrame();	// frame 1: settles the window AND fills the cache

	u_int uAfterSecondFrame = 0;
	{
		GraphPanelImGuiFrame xFrame(fGRAPHPANEL_DISPLAY_W, fGRAPHPANEL_DISPLAY_H);
		ImGui::SetNextWindowCollapsed(false);
		Zenith_GraphEditorPanel::Render();
		uAfterSecondFrame = Zenith_GraphEditorPanel::GetPinTypeCacheFillCountForTest();
		// The node really is being drawn, so "no refill" is not "nothing happened".
		Zenith_Maths::Vector2 xPin;
		ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::GetDataPinScreenPos(
			Zenith_GraphEditorPanel::FindNodeIDByType("Test_EditorInstanceOut"), "Out", false, xPin));
	}
	{
		GraphPanelImGuiFrame xFrame(fGRAPHPANEL_DISPLAY_W, fGRAPHPANEL_DISPLAY_H);
		ImGui::SetNextWindowCollapsed(false);
		Zenith_GraphEditorPanel::Render();
		// A THIRD frame adds ZERO fills.
		ZENITH_ASSERT_EQ(Zenith_GraphEditorPanel::GetPinTypeCacheFillCountForTest(), uAfterSecondFrame);
	}

	// A COMMITTED PARAM EDIT invalidates it: m_iTypeCode is what the instance
	// answers GetPinType with, so the cached type is now wrong.
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_SelectNode("Test_EditorInstanceOut", 0));
	ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_SetSelectedNodeParamInt("m_iTypeCode", 1));
	{
		GraphPanelImGuiFrame xFrame(fGRAPHPANEL_DISPLAY_W, fGRAPHPANEL_DISPLAY_H);
		ImGui::SetNextWindowCollapsed(false);
		Zenith_GraphEditorPanel::Render();
		ZENITH_ASSERT_GT(Zenith_GraphEditorPanel::GetPinTypeCacheFillCountForTest(), uAfterSecondFrame);
	}

	Zenith_GraphEditorPanel::Close();
}

// ★ A PULLED PURE NODE LIGHTS UP (B-4). The panel's live highlight reads
// Zenith_BehaviourGraph::GetRecentlyExecuted, and PullSlot now pushes the pure
// source it evaluated - so this drives a REAL graph and asks the panel's own
// reader.
//
// ★ DEVIATION, RECORDED. The brief's fixture wanted the FULL editor path
// (SelectEntity + EditorMode::Playing + a Zenith_GraphComponent slot). That path
// needs a .bgraph ON DISK - Zenith_GraphComponent::AddGraphByAssetPath loads
// through the asset registry - which would make this unit write a tracked asset,
// and the Playing transition backs up and restores the ACTIVE SCENE (the editor's
// own tests document that it clears the selection). Both are real cross-test
// hazards for one boolean. So this drives the reader over a real live graph
// instead, and asserts the public accessor's negative through the same funnel.
// The windowed end-to-end is B-5's.
ZENITH_TEST(GraphEditorPanel, GraphEditor_PulledPureNodeIsHighlighted)
{
	EnsureGraphEditorTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	const u_int uSource = xDef.AddNode("Test_EditorOnUpdate");
	const u_int uConsumer = xDef.AddNode("Test_EditorDataConsumer");
	const u_int uPure = xDef.AddNode("Test_EditorPureRelay");
	const u_int uUnrun = xDef.AddNode("Test_EditorPlain");
	ZENITH_ASSERT_NE(uSource, 0u);
	ZENITH_ASSERT_TRUE(xDef.AddEdge(uSource, 0, uConsumer));
	ZENITH_ASSERT_TRUE(xDef.AddDataEdge(uPure, "Out", uConsumer, "A"));

	Zenith_BehaviourGraph xGraph;
	ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
	Zenith_GraphContext xContext;
	xContext.m_fDt = 1.0f / 60.0f;
	xContext.m_pxGraph = &xGraph;
	xContext.m_pxBlackboard = &xGraph.GetBlackboard();

	// Before the first dispatch nothing has run, so the highlight cannot be a
	// constant true.
	ZENITH_ASSERT_FALSE(IsNodeRecentlyExecuted(&xGraph, uPure));

	xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);

	// The consumer ran, and the PURE node it pulled is highlighted with it...
	ZENITH_ASSERT_TRUE(IsNodeRecentlyExecuted(&xGraph, uConsumer));
	ZENITH_ASSERT_TRUE(IsNodeRecentlyExecuted(&xGraph, uPure));
	// ...while a node nothing reached is not.
	ZENITH_ASSERT_FALSE(IsNodeRecentlyExecuted(&xGraph, uUnrun));
	// A null live graph (the panel's own "not playing" answer) never highlights.
	ZENITH_ASSERT_FALSE(IsNodeRecentlyExecuted(nullptr, uPure));

	// The exported accessor, through the real live-graph lookup: no entity is
	// selected and the editor is not Playing, so it answers false rather than
	// reading some other entity's graph.
	ZENITH_ASSERT_FALSE(Zenith_GraphEditorPanel::IsNodeHighlightedForTest(uPure));
}

// Every pin type has its OWN colour, and no two are within 24 on all three
// channels - a palette whose members are merely "not equal" is one a human
// cannot read apart, which is the entire point of colouring pins by type.
ZENITH_TEST(GraphEditorPanel, GraphEditor_PinTypeColourTableTotal)
{
	// PROPERTY_TYPE_COUNT is eGRAPH_PIN_TYPE_ANY, so the loop covers every
	// property type PLUS the wildcard - the whole domain of the function.
	constexpr u_int uTYPE_COUNT = static_cast<u_int>(PROPERTY_TYPE_COUNT) + 1u;
	ImU32 auColours[uTYPE_COUNT] = {};
	for (u_int u = 0; u < uTYPE_COUNT; ++u)
	{
		auColours[u] = PinTypeColour(static_cast<Zenith_PropertyType>(u));
		// Opaque: a pin drawn at alpha 0 is a pin that is not there.
		ZENITH_ASSERT_EQ(static_cast<u_int>((auColours[u] >> IM_COL32_A_SHIFT) & 0xFFu), 255u);
	}

	// The wildcard is explicitly GREY, and an unresolvable pin maps onto exactly
	// this entry rather than onto a fabricated type.
	ZENITH_ASSERT_EQ(PinTypeColour(eGRAPH_PIN_TYPE_ANY), IM_COL32(150, 150, 150, 255));

	for (u_int uA = 0; uA < uTYPE_COUNT; ++uA)
	{
		for (u_int uB = uA + 1u; uB < uTYPE_COUNT; ++uB)
		{
			ZENITH_ASSERT_NE(auColours[uA], auColours[uB]);
			const int iRed = ChannelSeparation(auColours[uA], auColours[uB], IM_COL32_R_SHIFT);
			const int iGreen = ChannelSeparation(auColours[uA], auColours[uB], IM_COL32_G_SHIFT);
			const int iBlue = ChannelSeparation(auColours[uA], auColours[uB], IM_COL32_B_SHIFT);
			int iWidest = iRed > iGreen ? iRed : iGreen;
			iWidest = iWidest > iBlue ? iWidest : iBlue;
			ZENITH_ASSERT_GT(iWidest, 23);
		}
	}
}

#endif // ZENITH_TESTING
