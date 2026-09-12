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

	// Idempotent via a static rather than the registry's duplicate guard, which
	// would log an error line per re-entry.
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

#endif // ZENITH_TESTING
