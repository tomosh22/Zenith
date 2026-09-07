//------------------------------------------------------------------------------
// Zenith_EditorPanel_AnimStateMachine unit tests (WU-6.5).
// Included at the bottom of Zenith_EditorPanel_AnimStateMachine.cpp.
//
// ★ THE FLAGSHIP IS AN END-TO-END ONE: a two-state machine authored entirely
// through the panel's Action_* verbs is SAVED, PARSED BACK OFF DISK by the real
// reader, built into a real Flux_AnimationController, and then driven — set the
// parameter so the condition holds, tick, and the current state moves. Every
// link in that chain is a place an editor can produce something that looks
// right and animates nothing.
//
// Everything here is headless. The one thing that would normally make a panel
// test requiresGraphics — an ImGui frame — is supplied by the RAII helper below,
// which is a duplicate of the dope sheet's AnimPanelImGuiFrame rather than a
// share: two panels' test fixtures should not be able to break each other, and
// the helper is fifteen lines.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"
#include "UnitTests/Zenith_UnitTests.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_AnimationAsset.h"
#include "Flux/MeshAnimation/Flux_AnimationClip.h"
#include "Flux/MeshAnimation/Flux_BlendTree.h"
#include "DataStream/Zenith_DataStream.h"

#include "imgui.h"

#include <filesystem>

namespace
{
	//--------------------------------------------------------------------------
	// One self-contained ImGui frame.
	//
	// ImGui::NewFrame asserts the PREVIOUS frame was ended, so the pair has to be
	// balanced on every path — hence RAII rather than two calls in the test body.
	// Everything it touches on the IO is restored, because the editor's own frame
	// loop starts up moments after this batch and must not inherit a display size
	// or a mouse position a test invented.
	//
	// ★ THE RESTORE IS WHY EVERY RECT QUERY MUST BE ANSWERED FROM STATE CAPTURED
	// DURING THE FRAME. io.DisplaySize is (-1, -1) until a backend NewFrame fills
	// it in, and this batch runs before the main loop's first one — so the value
	// put back here is (-1, -1), and any gate that re-read io.DisplaySize at query
	// time would refuse every rect in the assertions below.
	//--------------------------------------------------------------------------
	struct AnimSmPanelImGuiFrame
	{
		AnimSmPanelImGuiFrame(float fWidth, float fHeight)
		{
			ImGuiIO& xIO = ImGui::GetIO();
			m_xSavedDisplaySize = xIO.DisplaySize;
			m_fSavedDeltaTime = xIO.DeltaTime;
			m_xSavedMousePos = xIO.MousePos;
			m_fSavedMouseWheel = xIO.MouseWheel;

			xIO.DisplaySize = ImVec2(fWidth, fHeight);
			xIO.DeltaTime = 1.0f / 60.0f;
			// Far outside ImGui's MOUSE_INVALID_MARKER, so nothing is hovered and
			// no wheel or click can reach the panel. A test that measured a rect
			// while the developer's real cursor happened to sit over the canvas
			// would pass or fail on where the mouse was.
			xIO.MousePos = ImVec2(-1.0e30f, -1.0e30f);
			xIO.MouseWheel = 0.0f;

			ImGui::NewFrame();
		}

		~AnimSmPanelImGuiFrame()
		{
			ImGui::EndFrame();
			ImGuiIO& xIO = ImGui::GetIO();
			xIO.DisplaySize = m_xSavedDisplaySize;
			xIO.DeltaTime = m_fSavedDeltaTime;
			xIO.MousePos = m_xSavedMousePos;
			xIO.MouseWheel = m_fSavedMouseWheel;
		}

		AnimSmPanelImGuiFrame(const AnimSmPanelImGuiFrame&) = delete;
		AnimSmPanelImGuiFrame& operator=(const AnimSmPanelImGuiFrame&) = delete;

		ImVec2 m_xSavedDisplaySize;
		ImVec2 m_xSavedMousePos;
		float m_fSavedDeltaTime = 0.0f;
		float m_fSavedMouseWheel = 0.0f;
	};

	constexpr float fANIMSM_DISPLAY_W = 1280.0f;
	constexpr float fANIMSM_DISPLAY_H = 720.0f;

	void AnimSmRenderFrames(Zenith_EditorPanel_AnimStateMachine& xPanel, u_int uFrames)
	{
		for (u_int u = 0; u < uFrames; ++u)
		{
			AnimSmPanelImGuiFrame xFrame(fANIMSM_DISPLAY_W, fANIMSM_DISPLAY_H);
			// dt 0: nothing here is testing the preview clock, and a running
			// preview would move the highlight between two frames a test compares.
			xPanel.Render(0.0f);
		}
	}

	//--------------------------------------------------------------------------
	// Fixture — a private temp directory removed on the way out, plus a
	// ForceUnload of every registry path the test caused to be loaded.
	//
	// ★ DECLARE THE FIXTURE BEFORE THE PANEL IN EVERY TEST. The panel's preview
	// controller holds owning animation handles; ForceUnload deletes regardless
	// of refcount, so the panel has to be destroyed first and declaration order
	// is what guarantees that.
	//--------------------------------------------------------------------------
	struct AnimSmFixture
	{
		std::filesystem::path m_xDirectory;
		std::string m_strControllerPath;
		Zenith_Vector<std::string> m_axTrackedPaths;

		explicit AnimSmFixture(const char* szLeafDirectory)
		{
			std::error_code xError;
			std::filesystem::path xRoot = std::filesystem::temp_directory_path(xError);
			if (xError)
			{
				xRoot = ".";
			}
			m_xDirectory = xRoot / szLeafDirectory;
			std::filesystem::remove_all(m_xDirectory, xError);
			std::filesystem::create_directories(m_xDirectory, xError);
			m_strControllerPath = (m_xDirectory / "probe.zanimctrl").generic_string();
			m_axTrackedPaths.PushBack(m_strControllerPath);
		}

		std::string PathFor(const char* szLeafName)
		{
			std::string strPath = (m_xDirectory / szLeafName).generic_string();
			m_axTrackedPaths.PushBack(strPath);
			return strPath;
		}

		// A one-bone, one-second .zanim called szClipName, written to szLeafName.
		std::string WriteClip(const char* szLeafName, const char* szClipName)
		{
			const std::string strPath = PathFor(szLeafName);
			Flux_AnimationClip xClip;
			xClip.SetName(szClipName);
			xClip.SetDuration(1.0f);
			Flux_BoneChannel xHip;
			xHip.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
			xHip.AddPositionKeyframe(1.0f, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
			xHip.SortKeyframes();
			xClip.AddBoneChannel("Hip", std::move(xHip));
			xClip.Export(strPath);
			return strPath;
		}

		~AnimSmFixture()
		{
			for (u_int u = 0; u < m_axTrackedPaths.GetSize(); ++u)
			{
				Zenith_AssetRegistry::ForceUnload(m_axTrackedPaths.Get(u));
			}
			std::error_code xError;
			std::filesystem::remove_all(m_xDirectory, xError);
		}

		AnimSmFixture(const AnimSmFixture&) = delete;
		AnimSmFixture& operator=(const AnimSmFixture&) = delete;
	};

	bool AnimSmParseControllerFile(const std::string& strPath, Flux_AnimatorControllerDef& xOut)
	{
		Zenith_DataStream xStream;
		xStream.ReadFromFile(strPath.c_str());
		if (!xStream.IsValid())
		{
			return false;
		}
		xStream.SetCursor(0);
		return xOut.ParseStream(xStream).IsOk();
	}
}

ZENITH_TEST(AnimSmPanel, NodeRectsAgreeBetweenTwoFramesAndSitWhereTheLayoutPutThem)
{
	AnimSmFixture xFixture("zenith_animsm_rects");
	Zenith_EditorPanel_AnimStateMachine xPanel;

	ZENITH_ASSERT_TRUE(xPanel.OpenAssetFresh(xFixture.m_strControllerPath), "a fresh controller opens");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddState("Idle"), "Idle");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddState("Walk"), "Walk");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddState("Run"), "Run");

	xPanel.RequestWindowPlacement(20.0f, 20.0f, 1200.0f, 640.0f);
	AnimSmRenderFrames(xPanel, 2);

	// ★ THE DISCRIMINATORS FIRST. A bare `false` from a rect accessor has four
	// causes — never drawn, no room, scrolled away, outside the display — and a
	// test that only saw the bool would report "nothing was published".
	ZENITH_ASSERT_TRUE(xPanel.WasCanvasDrawnLastFrame(), "the canvas was drawn");
	ZENITH_ASSERT_GT(xPanel.GetRenderedFrameCount(), 0u, "and the window rendered");
	ZENITH_ASSERT_EQ_FLOAT(xPanel.GetRecordedDisplayWidth(), fANIMSM_DISPLAY_W, 0.5f,
		"and the display bound was captured AT RECORD TIME, not re-read now");
	ZENITH_ASSERT_GE(xPanel.GetDrawnNodeCount(), 2u, "at least the first two nodes were painted");

	Zenith_AnimCtrlPanelRect xIdleA;
	Zenith_AnimCtrlPanelRect xWalkA;
	ZENITH_ASSERT_TRUE(xPanel.GetStateNodeRect("Idle", xIdleA), "Idle's node publishes");
	ZENITH_ASSERT_TRUE(xPanel.GetStateNodeRect("Walk", xWalkA), "and Walk's");
	ZENITH_ASSERT_GT(xIdleA.Width(), 1.0f, "with a real width");

	// ★ THE RECT IS THE LAYOUT, not a second derivation of it. GetNodePosition is
	// what the draw asked; the published rect has to be that position scaled and
	// offset by the canvas origin and nothing else, or the hit test and the paint
	// are two answers that can drift.
	//
	// The DPI scale is recovered from the node box rather than read from
	// Zenith_EditorUI, so the assertion is about the panel's own two derivations
	// agreeing rather than about what display the test ran on.
	const float fScaleFromRect = xIdleA.Width() / fANIMSM_NODE_WIDTH_1X;
	ZENITH_ASSERT_GT(fScaleFromRect, 0.0f, "the node box has a positive scale");
	Zenith_Maths::Vector2 xIdlePos(0.0f);
	Zenith_Maths::Vector2 xWalkPos(0.0f);
	ZENITH_ASSERT_TRUE(xPanel.GetNodePosition("Idle", xIdlePos), "Idle has a layout position");
	ZENITH_ASSERT_TRUE(xPanel.GetNodePosition("Walk", xWalkPos), "and so does Walk");
	ZENITH_ASSERT_EQ_FLOAT(xWalkA.m_fMinX - xIdleA.m_fMinX, (xWalkPos.x - xIdlePos.x) * fScaleFromRect, 0.5f,
		"★ the on-screen gap between two nodes IS the gap between their layout positions");

	// Two more frames with nothing changed must publish the same coordinates: a
	// rect that moved on its own is a layout being recomputed from something that
	// is not stable (hash-map iteration order is the classic).
	AnimSmRenderFrames(xPanel, 2);
	Zenith_AnimCtrlPanelRect xIdleB;
	ZENITH_ASSERT_TRUE(xPanel.GetStateNodeRect("Idle", xIdleB), "Idle publishes again");
	ZENITH_ASSERT_EQ_FLOAT(xIdleB.m_fMinX, xIdleA.m_fMinX, 0.01f, "★ at the same x");
	ZENITH_ASSERT_EQ_FLOAT(xIdleB.m_fMinY, xIdleA.m_fMinY, 0.01f, "★ and the same y");

	xPanel.CloseAsset();
}

ZENITH_TEST(AnimSmPanel, AnOffScreenNodeRefusesRatherThanHandingOutACoordinate)
{
	// The half of the graph editor's hard-won contract that is worth copying: it
	// used to hand out the VIRTUAL, scrolled-away rect, so a test clicked screen
	// y=1768 on a 720-tall display and reported only "the nodes were not
	// created" — with the click, the bridge and the panel all healthy.
	AnimSmFixture xFixture("zenith_animsm_offscreen");
	Zenith_EditorPanel_AnimStateMachine xPanel;

	ZENITH_ASSERT_TRUE(xPanel.OpenAssetFresh(xFixture.m_strControllerPath), "a fresh controller opens");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddState("Idle"), "one state");

	xPanel.RequestWindowPlacement(20.0f, 20.0f, 1000.0f, 620.0f);
	AnimSmRenderFrames(xPanel, 2);

	Zenith_AnimCtrlPanelRect xRect;
	ZENITH_ASSERT_TRUE(xPanel.GetStateNodeRect("Idle", xRect), "on screen it publishes");

	xPanel.SetCanvasScroll(100000.0f, 0.0f);
	AnimSmRenderFrames(xPanel, 2);

	// The discriminators say the panel is still healthy, so the refusal below is
	// the gate doing its job rather than the window having stopped drawing.
	ZENITH_ASSERT_TRUE(xPanel.WasCanvasDrawnLastFrame(), "the canvas is still being drawn");
	ZENITH_ASSERT_GT(xPanel.GetRecordedDisplayWidth(), 0.0f, "and still recording a display bound");
	ZENITH_ASSERT_EQ(xPanel.GetDrawnNodeCount(), 0u, "★ but nothing was painted inside it");
	ZENITH_ASSERT_FALSE(xPanel.GetStateNodeRect("Idle", xRect),
		"★ and the accessor REFUSES instead of handing out a coordinate no click can reach");

	// And a name that never existed refuses the same way, so a typo does not
	// resolve to a neighbour.
	xPanel.SetCanvasScroll(0.0f, 0.0f);
	AnimSmRenderFrames(xPanel, 2);
	ZENITH_ASSERT_TRUE(xPanel.GetStateNodeRect("Idle", xRect), "scrolling back publishes again");
	ZENITH_ASSERT_FALSE(xPanel.GetStateNodeRect("NoSuchState", xRect), "an unknown state refuses");

	xPanel.CloseAsset();
}

ZENITH_TEST(AnimSmPanel, AuthoredThroughTheActionsItSavesAndDrivesARuntimeController)
{
	// ★ THE END-TO-END ONE. Nothing below asserts about the panel's own state
	// after the save: the def is re-read off disk by the real reader, built by
	// the real Flux_AnimationController, and driven by the real state machine.
	AnimSmFixture xFixture("zenith_animsm_endtoend");
	const std::string strIdleClip = xFixture.WriteClip("idle.zanim", "IdleClip");
	const std::string strWalkClip = xFixture.WriteClip("walk.zanim", "WalkClip");

	Zenith_EditorPanel_AnimStateMachine xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenAssetFresh(xFixture.m_strControllerPath), "a fresh controller opens");

	ZENITH_ASSERT_TRUE(xPanel.Action_AddClipPath(strIdleClip), "the idle clip is in the def's clip list");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddClipPath(strWalkClip), "and the walk clip");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddParameter("Speed", Flux_AnimationParameters::ParamType::Float, 0.0f),
		"Speed is declared");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddState("Idle"), "Idle");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddState("Walk"), "Walk");
	ZENITH_ASSERT_TRUE(xPanel.Action_SetStateClip("Idle", "IdleClip"), "Idle plays IdleClip");
	ZENITH_ASSERT_TRUE(xPanel.Action_SetStateClip("Walk", "WalkClip"), "Walk plays WalkClip");
	ZENITH_ASSERT_TRUE(xPanel.Action_SetDefaultState("Idle"), "Idle is the entry point");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddTransition("Idle", "Walk"), "Idle -> Walk");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddCondition("Idle", 0, "Speed",
		Flux_TransitionCondition::CompareOp::Greater, 0.1f), "when Speed > 0.1");
	ZENITH_ASSERT_TRUE(xPanel.Action_Save(), "and it saves");

	// ---- the file, through the real reader ---------------------------------
	Flux_AnimatorControllerDef xDef;
	ZENITH_ASSERT_TRUE(AnimSmParseControllerFile(xFixture.m_strControllerPath, xDef),
		"★ the saved bytes parse as a .zanimctrl — envelope, schema and all");
	ZENITH_ASSERT_EQ(xDef.GetClipPaths().GetSize(), 2u, "with both clip paths");
	ZENITH_ASSERT_NOT_NULL(xDef.GetStateMachineDef(), "and a top-level machine");
	ZENITH_ASSERT_EQ(xDef.GetStateMachineDef()->GetStates().GetSize(), 2u, "holding both states");
	ZENITH_ASSERT_EQ(xDef.GetStateMachineDef()->GetDefaultStateName(), std::string("Idle"), "entering at Idle");

	// ---- and it DRIVES ------------------------------------------------------
	// Heap, not stack: a Flux_AnimationController carries two FLUX_MAX_BONES
	// poses and the test already has a panel holding a third.
	Flux_AnimationController* pxController = new Flux_AnimationController();
	ZENITH_ASSERT_TRUE(pxController->BuildFromControllerDef(xDef, nullptr),
		"★ every clip the def names resolves — a false here is a dangling reference, not a style point");
	pxController->PublishSharedParameters();
	ZENITH_ASSERT_TRUE(pxController->HasStateMachine(), "the controller has the machine");

	// ★ THE MACHINE IS TICKED DIRECTLY AGAINST A ZERO-BONE RIG.
	// Flux_AnimationController::Update returns on its first line without a
	// Flux_SkeletonInstance, and this test has no rig — but transition
	// evaluation, exit times and the blend-tree playheads all live on the
	// MACHINE and need only a Zenith_SkeletonAsset& to satisfy the signature.
	Zenith_SkeletonAsset xStubSkeleton;
	Flux_SkeletonPose* pxPose = new Flux_SkeletonPose();
	Flux_AnimationStateMachine& xMachine = pxController->GetStateMachine();

	pxController->SetFloat("Speed", 0.0f);
	xMachine.Update(1.0f / 60.0f, *pxPose, xStubSkeleton);
	ZENITH_ASSERT_EQ(xMachine.GetCurrentStateName(), std::string("Idle"),
		"it enters at the default state and the condition does not hold");

	pxController->SetFloat("Speed", 1.0f);
	xMachine.Update(1.0f / 60.0f, *pxPose, xStubSkeleton);   // starts the transition
	xMachine.Update(0.5f, *pxPose, xStubSkeleton);           // and finishes it
	ZENITH_ASSERT_EQ(xMachine.GetCurrentStateName(), std::string("Walk"),
		"★ setting the parameter so the condition holds moves the machine — the whole chain works");

	// The controller pins one animation asset per clip; hand them back while the
	// registry is still alive.
	pxController->ReleaseAssetReferences();
	delete pxController;
	delete pxPose;

	xPanel.CloseAsset();
}

ZENITH_TEST(AnimSmPanel, ApplyReloadsThePreviewAndKeepsItsCurrentState)
{
	// ★ D45. BuildFromControllerDef is a DEMOLITION — it resets every machine's
	// runtime and puts every playhead back at zero — so an Apply built on it
	// would snap the character to the default state at frame 0, which is exactly
	// what makes editing a controller with the preview running useless.
	AnimSmFixture xFixture("zenith_animsm_apply");
	const std::string strIdleClip = xFixture.WriteClip("idle.zanim", "IdleClip");
	const std::string strWalkClip = xFixture.WriteClip("walk.zanim", "WalkClip");

	Zenith_EditorPanel_AnimStateMachine xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenAssetFresh(xFixture.m_strControllerPath), "a fresh controller opens");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddClipPath(strIdleClip), "idle clip");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddClipPath(strWalkClip), "walk clip");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddParameter("Speed", Flux_AnimationParameters::ParamType::Float, 0.0f), "Speed");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddState("Idle"), "Idle");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddState("Walk"), "Walk");
	ZENITH_ASSERT_TRUE(xPanel.Action_SetStateClip("Idle", "IdleClip"), "Idle's clip");
	ZENITH_ASSERT_TRUE(xPanel.Action_SetStateClip("Walk", "WalkClip"), "Walk's clip");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddTransition("Idle", "Walk"), "Idle -> Walk");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddCondition("Idle", 0, "Speed",
		Flux_TransitionCondition::CompareOp::Greater, 0.1f), "on Speed");

	ZENITH_ASSERT_TRUE(xPanel.Action_SetPreviewEnabled(true), "the preview builds cleanly");
	ZENITH_ASSERT_TRUE(xPanel.IsPreviewEnabled(), "and is on");
	ZENITH_ASSERT_TRUE(xPanel.IsPreviewComplete(), "with every clip resolved");

	ZENITH_ASSERT_TRUE(xPanel.Action_SetPreviewFloat("Speed", 1.0f), "drive Speed past the threshold");
	ZENITH_ASSERT_TRUE(xPanel.Action_TickPreview(1.0f / 60.0f), "tick");
	ZENITH_ASSERT_TRUE(xPanel.Action_TickPreview(0.5f), "and tick past the blend");
	ZENITH_ASSERT_EQ(xPanel.GetHighlightedStateName(), std::string("Walk"),
		"the preview is now in Walk, and the canvas rings it");

	// An edit that does NOT delete Walk.
	ZENITH_ASSERT_TRUE(xPanel.Action_SetTransitionDuration("Idle", 0, 0.4f), "retime the transition");
	ZENITH_ASSERT_TRUE(xPanel.Action_Apply(), "and apply");

	ZENITH_ASSERT_EQ(xPanel.GetHighlightedStateName(), std::string("Walk"),
		"★ the preview is STILL in Walk — the state survived on its NAME (D45), which a rebuild "
		"would have thrown away in favour of the default state at frame 0");
	ZENITH_ASSERT_EQ_FLOAT(xPanel.PreviewController().GetParameters().GetFloat("Speed"), 1.0f, 1e-6f,
		"★ and the live parameter value survived too, on name AND type");

	xPanel.CloseAsset();
}

ZENITH_TEST(AnimSmPanel, AStateWhoseTreeIsNotASingleClipLeafIsRefusedAndSaysWhoOwnsIt)
{
	// ★ THE WU-7.3 BOUNDARY, AS A REFUSAL RATHER THAN A SILENT FLATTEN. Assigning
	// a clip to a state holding a blend space would delete the space and every
	// clip in it, and report success.
	AnimSmFixture xFixture("zenith_animsm_complextree");

	// Author a def with a blend SPACE on one state, straight through the engine
	// types — the panel cannot create one, which is the point.
	{
		Flux_AnimatorControllerDef xDef;
		Flux_AnimationStateMachineDef& xMachine = xDef.GetOrCreateStateMachineDef();
		Flux_AnimationState* pxPlain = xMachine.AddState("Plain");
		Flux_BlendTreeNode_Clip* pxLeaf = new Flux_BlendTreeNode_Clip();
		pxLeaf->SetClipName("IdleClip");
		pxPlain->SetBlendTree(pxLeaf);

		Flux_AnimationState* pxFancy = xMachine.AddState("Fancy");
		pxFancy->SetBlendTree(new Flux_BlendTreeNode_BlendSpace1D());
		xDef.Export(xFixture.m_strControllerPath);
	}

	Zenith_EditorPanel_AnimStateMachine xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenAsset(xFixture.m_strControllerPath), "the def opens");

	ZENITH_ASSERT_TRUE(xPanel.GetStateTreeKind("Plain") == ZENITH_ANIMCTRL_TREE_SINGLE_CLIP,
		"the plain state is a single clip leaf");
	ZENITH_ASSERT_TRUE(xPanel.GetStateTreeKind("Fancy") == ZENITH_ANIMCTRL_TREE_COMPLEX,
		"★ and the blend space is COMPLEX");

	const u_int uDepth = xPanel.Document().GetUndoStackSize();
	ZENITH_ASSERT_FALSE(xPanel.Action_SetStateClip("Fancy", "IdleClip"),
		"★ assigning a clip to it is REFUSED");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), uDepth, "and pushes no undo entry");
	ZENITH_ASSERT_TRUE(xPanel.GetStateTreeKind("Fancy") == ZENITH_ANIMCTRL_TREE_COMPLEX,
		"★ and the blend space is still there");

	ZENITH_ASSERT_NOT_NULL(Zenith_EditorPanel_AnimStateMachine::BlendTreeRefusalText(),
		"the refusal has ONE wording, so the node badge and the inspector cannot disagree");
	ZENITH_ASSERT_TRUE(std::string(Zenith_EditorPanel_AnimStateMachine::BlendTreeRefusalText())
		.find("WU-7.3") != std::string::npos, "and it names the editor that owns the case");

	// The plain state is still editable, so the refusal is scoped to the state
	// and not to the machine.
	ZENITH_ASSERT_TRUE(xPanel.Action_SetStateClip("Plain", "WalkClip"), "the plain state still takes a clip");

	xPanel.CloseAsset();
}

ZENITH_TEST(AnimSmPanel, EveryPanelActionIsOneUndoStepAndTheSelectionFollowsIt)
{
	AnimSmFixture xFixture("zenith_animsm_undo");
	Zenith_EditorPanel_AnimStateMachine xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenAssetFresh(xFixture.m_strControllerPath), "a fresh controller opens");

	ZENITH_ASSERT_TRUE(xPanel.Action_AddState("Idle"), "Idle");
	ZENITH_ASSERT_EQ(xPanel.GetSelectedStateName(), std::string("Idle"),
		"a new state becomes the selection, so the obvious next gesture needs no second click");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddState("Walk"), "Walk");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddTransition("Idle", "Walk"), "and an edge");

	std::string strFrom;
	u_int uIndex = uINVALID_ANIMSM_TRANSITION;
	ZENITH_ASSERT_TRUE(xPanel.GetSelectedTransition(strFrom, uIndex), "the new edge becomes the selection");
	ZENITH_ASSERT_EQ(strFrom, std::string("Idle"), "from Idle");
	ZENITH_ASSERT_EQ(uIndex, 0u, "at index 0");
	ZENITH_ASSERT_TRUE(xPanel.GetSelectedStateName().empty(),
		"and a transition selection clears the state one — the inspector shows exactly one thing");

	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 3u, "three edits, three undo steps");

	ZENITH_ASSERT_TRUE(xPanel.Action_Undo(), "undo the edge");
	ZENITH_ASSERT_FALSE(xPanel.GetSelectedTransition(strFrom, uIndex),
		"★ and the selection goes with it — an inspector pointed at a transition that no longer "
		"resolves would edit whatever now sits at that index");

	ZENITH_ASSERT_TRUE(xPanel.Action_Undo(), "undo Walk");
	ZENITH_ASSERT_TRUE(xPanel.Action_Undo(), "undo Idle");
	ZENITH_ASSERT_EQ(xPanel.Document().GetStateCount(), 0u, "the machine is empty");
	ZENITH_ASSERT_FALSE(xPanel.Action_Undo(), "and there is nothing left to undo");

	ZENITH_ASSERT_TRUE(xPanel.Action_Redo(), "redo Idle");
	ZENITH_ASSERT_TRUE(xPanel.Action_Redo(), "redo Walk");
	ZENITH_ASSERT_TRUE(xPanel.Action_Redo(), "redo the edge");
	ZENITH_ASSERT_EQ(xPanel.Document().GetStateCount(), 2u, "both states are back");
	ZENITH_ASSERT_EQ(xPanel.Document().GetTransitionCount("Idle"), 1u, "and so is the edge");

	xPanel.CloseAsset();
}

ZENITH_TEST(AnimSmPanel, ANodeDragCommitsOneUndoableEditAndTheLayoutFallbackIsNotWrittenToTheDef)
{
	// ★ AN UNPLACED STATE IS LAID OUT BY THE PANEL AND NOT WRITTEN TO THE DEF.
	// Flux_AnimationState::m_xEditorPosition IS serialized, so silently writing
	// the automatic grid into it on OPEN would dirty a document nobody edited and
	// rewrite a tracked .zanimctrl for a cosmetic reason. Only a DRAG commits.
	AnimSmFixture xFixture("zenith_animsm_layout");
	Zenith_EditorPanel_AnimStateMachine xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenAssetFresh(xFixture.m_strControllerPath), "a fresh controller opens");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddState("Idle"), "Idle");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddState("Walk"), "Walk");

	Zenith_Maths::Vector2 xLaidOut(0.0f);
	ZENITH_ASSERT_TRUE(xPanel.GetNodePosition("Walk", xLaidOut), "Walk has a layout position");
	ZENITH_ASSERT_TRUE(xLaidOut.x != 0.0f || xLaidOut.y != 0.0f, "which is not the origin");

	Zenith_Maths::Vector2 xStored(0.0f);
	ZENITH_ASSERT_TRUE(xPanel.Document().GetStateEditorPosition("Walk", xStored), "the def has a stored one");
	ZENITH_ASSERT_EQ_FLOAT(xStored.x, 0.0f, 1e-6f, "★ which is still the origin — the grid is a FALLBACK");
	ZENITH_ASSERT_EQ_FLOAT(xStored.y, 0.0f, 1e-6f, "on both axes");

	const u_int uDepth = xPanel.Document().GetUndoStackSize();
	ZENITH_ASSERT_TRUE(xPanel.Action_SetStatePosition("Walk", 500.0f, 300.0f), "a drag commits");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), uDepth + 1u, "as ONE undo step");
	ZENITH_ASSERT_TRUE(xPanel.GetNodePosition("Walk", xLaidOut), "and the node moved");
	ZENITH_ASSERT_EQ_FLOAT(xLaidOut.x, 500.0f, 1e-4f, "to the dropped x");

	// ★ THE ORIGIN IS RESERVED, so a drop AT it is nudged rather than being read
	// back as "never placed" and jumping to the grid on the next frame.
	ZENITH_ASSERT_TRUE(xPanel.Action_SetStatePosition("Walk", 0.0f, 0.0f), "a drop at the origin is accepted");
	ZENITH_ASSERT_TRUE(xPanel.Document().GetStateEditorPosition("Walk", xStored), "and stored");
	ZENITH_ASSERT_TRUE(xStored.x != 0.0f || xStored.y != 0.0f,
		"★ as something the fallback will not mistake for an unplaced state");

	xPanel.Action_Undo();
	ZENITH_ASSERT_TRUE(xPanel.Document().GetStateEditorPosition("Walk", xStored), "the undo restores");
	ZENITH_ASSERT_EQ_FLOAT(xStored.x, 500.0f, 1e-4f, "the previous position");

	xPanel.CloseAsset();
}
