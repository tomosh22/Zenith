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
// WU-7.2: the layer strip pushes the selected layer's blend mode into the dope
// sheet's bone-mask sub-panel, and one unit below reads it back out of the
// EDITOR-OWNED dope sheet — the same object the push targets.
#include "Editor/Panels/Zenith_EditorPanel_Animation.h"
#include "Editor/Zenith_BoneMaskDocument.h"

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

		// WU-7.3: a clip whose ONLY content is a CONSTANT rotation on "Root".
		//
		// ★ CONSTANT ON PURPOSE. A blend test has to be able to attribute a pose
		// difference to the BLEND WEIGHT and to nothing else — a clip whose value
		// varied with time would make the result depend on where the two leaves'
		// playheads happened to be, and they run their own clocks (D34).
		std::string WriteRotationClip(const char* szLeafName, const char* szClipName, float fRadiansAboutY)
		{
			const std::string strPath = PathFor(szLeafName);
			Flux_AnimationClip xClip;
			xClip.SetName(szClipName);
			xClip.SetDuration(1.0f);
			const Zenith_Maths::Quat xRotation =
				glm::angleAxis(fRadiansAboutY, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
			Flux_BoneChannel xRoot;
			xRoot.AddRotationKeyframe(0.0f, xRotation);
			xRoot.AddRotationKeyframe(1.0f, xRotation);
			xRoot.SortKeyframes();
			xClip.AddBoneChannel("Root", std::move(xRoot));
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
	// ★ THE REFUSAL, AS A REFUSAL RATHER THAN A SILENT FLATTEN. Assigning a clip
	// to a state holding something else would delete the sub-graph and report
	// success.
	//
	// ★ WU-7.3 MOVED THE BOUNDARY, AND THIS UNIT MOVED WITH IT. A blend SPACE was
	// COMPLEX here until this unit landed; it is now its own kind with an editor,
	// so the shape that stands in for "no editor anywhere" is a COMPOSITE — and
	// the refusal is asserted on that instead. The blend space's own half of the
	// story is the second block below.
	AnimSmFixture xFixture("zenith_animsm_complextree");

	// Author a def with a COMPOSITE on one state, straight through the engine
	// types — the panel cannot create one, which is the point.
	{
		Flux_AnimatorControllerDef xDef;
		Flux_AnimationStateMachineDef& xMachine = xDef.GetOrCreateStateMachineDef();
		Flux_AnimationState* pxPlain = xMachine.AddState("Plain");
		Flux_BlendTreeNode_Clip* pxLeaf = new Flux_BlendTreeNode_Clip();
		pxLeaf->SetClipName("IdleClip");
		pxPlain->SetBlendTree(pxLeaf);

		Flux_AnimationState* pxFancy = xMachine.AddState("Fancy");
		pxFancy->SetBlendTree(new Flux_BlendTreeNode_Blend());

		Flux_AnimationState* pxSpace = xMachine.AddState("Space");
		pxSpace->SetBlendTree(new Flux_BlendTreeNode_BlendSpace1D());
		xDef.Export(xFixture.m_strControllerPath);
	}

	Zenith_EditorPanel_AnimStateMachine xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenAsset(xFixture.m_strControllerPath), "the def opens");

	ZENITH_ASSERT_TRUE(xPanel.GetStateTreeKind("Plain") == ZENITH_ANIMCTRL_TREE_SINGLE_CLIP,
		"the plain state is a single clip leaf");
	ZENITH_ASSERT_TRUE(xPanel.GetStateTreeKind("Fancy") == ZENITH_ANIMCTRL_TREE_COMPLEX,
		"★ and the COMPOSITE is what is left in COMPLEX");
	ZENITH_ASSERT_TRUE(xPanel.GetStateTreeKind("Space") == ZENITH_ANIMCTRL_TREE_BLENDSPACE_1D,
		"★ while the blend space is its own kind — WU-7.3 edits it");

	const u_int uDepth = xPanel.Document().GetUndoStackSize();
	ZENITH_ASSERT_FALSE(xPanel.Action_SetStateClip("Fancy", "IdleClip"),
		"★ assigning a clip to the composite is REFUSED");
	ZENITH_ASSERT_FALSE(xPanel.Action_SetStateClip("Space", "IdleClip"),
		"★ and so is assigning one to the blend space — it would delete every point in it");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), uDepth, "and neither pushes an undo entry");
	ZENITH_ASSERT_TRUE(xPanel.GetStateTreeKind("Fancy") == ZENITH_ANIMCTRL_TREE_COMPLEX,
		"★ and the composite is still there");
	ZENITH_ASSERT_TRUE(xPanel.GetStateTreeKind("Space") == ZENITH_ANIMCTRL_TREE_BLENDSPACE_1D,
		"★ and so is the space");

	ZENITH_ASSERT_NOT_NULL(Zenith_EditorPanel_AnimStateMachine::BlendTreeRefusalText(),
		"the refusal has ONE wording, so the node badge and the inspector cannot disagree");
	// ★ THE PANEL'S WORDING IS THE DOCUMENT'S, not a second copy: the document
	// puts the same string into GetLastBlendTreeDiagnostic when it refuses, so
	// the badge, the inspector, the strip and a recipe's log line cannot describe
	// one refusal four ways.
	ZENITH_ASSERT_STREQ(Zenith_EditorPanel_AnimStateMachine::BlendTreeRefusalText(),
		Zenith_AnimControllerDocument::BlendTreeRefusalText(),
		"and the panel forwards the document's, rather than restating it");
	ZENITH_ASSERT_TRUE(std::string(Zenith_EditorPanel_AnimStateMachine::BlendTreeRefusalText())
		.find("Masked") != std::string::npos, "and it names the SHAPES it is refusing");

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

//==============================================================================
// The Layers strip (WU-7.2)
//==============================================================================

ZENITH_TEST(AnimSmPanel, TheLayerStripDrawsNothingWhenClosedAndNeverTakesCanvasHeight)
{
	// ★ THE HEIGHT GUARD, AND IT MEASURES THE CANVAS RECT RATHER THAN "is a row
	// visible". Editor/CLAUDE.md's rule was learned on the dope sheet, where an
	// always-present collapsed header pushed the events row off the bottom and
	// the failure arrived as a flat `false` from a rect accessor a long way from
	// its cause. This panel's canvas is a child sized out of the MAIN window's
	// remaining region, so anything emitted into the main window before it comes
	// straight out of the canvas — which is exactly why the layer strip lives
	// inside the fixed-size side child, and exactly what this measures.
	AnimSmFixture xFixture("zenith_animsm_layerheight");
	Zenith_EditorPanel_AnimStateMachine xPanel;

	ZENITH_ASSERT_TRUE(xPanel.OpenAssetFresh(xFixture.m_strControllerPath), "a fresh controller opens");
	xPanel.RequestWindowPlacement(20.0f, 20.0f, 1200.0f, 640.0f);
	AnimSmRenderFrames(xPanel, 2);

	// The discriminators first: a bare `false` from GetCanvasRect has four causes
	// and the height below would be meaningless against any of them.
	ZENITH_ASSERT_TRUE(xPanel.WasCanvasDrawnLastFrame(), "the canvas was drawn");
	ZENITH_ASSERT_EQ_FLOAT(xPanel.GetRecordedDisplayWidth(), fANIMSM_DISPLAY_W, 0.5f,
		"and the display bound was captured AT RECORD TIME");
	ZENITH_ASSERT_TRUE(xPanel.WasLayerStripDrawnLastFrame(), "the layer strip drew (a document is open)");
	ZENITH_ASSERT_EQ(xPanel.GetDrawnLayerRowCount(), 0u,
		"with no LAYER rows — the 'Top-level machine' row is a machine, not a layer");

	Zenith_AnimCtrlPanelRect xCanvasEmpty;
	ZENITH_ASSERT_TRUE(xPanel.GetCanvasRect(xCanvasEmpty), "the canvas publishes");
	const float fHeightNoLayers = xCanvasEmpty.Height();
	ZENITH_ASSERT_GT(fHeightNoLayers, 1.0f, "with a real height");

	// ---- six layers, and the canvas must not move ----------------------------
	for (u_int u = 0; u < 6u; ++u)
	{
		char acName[32];
		snprintf(acName, sizeof(acName), "Layer%u", u);
		ZENITH_ASSERT_TRUE(xPanel.Action_AddLayer(std::string(acName)), "a layer");
	}
	AnimSmRenderFrames(xPanel, 2);

	ZENITH_ASSERT_EQ(xPanel.GetDrawnLayerRowCount(), 6u, "all six rows were emitted");
	Zenith_AnimCtrlPanelRect xCanvasFull;
	ZENITH_ASSERT_TRUE(xPanel.GetCanvasRect(xCanvasFull), "the canvas still publishes");
	ZENITH_ASSERT_EQ_FLOAT(xCanvasFull.Height(), fHeightNoLayers, 0.5f,
		"★ six layers cost the canvas NOTHING — the strip is inside the side child, so a long list "
		"cannot push the graph off the bottom the way an always-drawn section did on the dope sheet");

	// ★ AND THE EQUALITY ABOVE IS NOT AN EQUALITY WITH A CONSTANT. A height that
	// never moved would satisfy it just as well, so this proves the measurement
	// is live before the guard is believed. Deliberately in the GROWING
	// direction: shrinking the window far enough to be convincing can drive the
	// canvas child below RenderCanvas's 8 px floor on a high-DPI machine, and
	// then the sensitivity check would fail for a reason that has nothing to do
	// with what it is testing.
	xPanel.RequestWindowPlacement(20.0f, 20.0f, 1200.0f, 780.0f);
	AnimSmRenderFrames(xPanel, 2);
	Zenith_AnimCtrlPanelRect xCanvasTall;
	ZENITH_ASSERT_TRUE(xPanel.GetCanvasRect(xCanvasTall), "the canvas publishes in the taller window");
	ZENITH_ASSERT_GT(xCanvasTall.Height(), fHeightNoLayers + 60.0f,
		"★ and a 140 px taller window really does grow it — the guard measures something");

	// ---- closed: NOTHING -----------------------------------------------------
	xPanel.CloseAsset();
	xPanel.RequestWindowPlacement(20.0f, 20.0f, 1200.0f, 640.0f);
	AnimSmRenderFrames(xPanel, 2);
	ZENITH_ASSERT_TRUE(xPanel.WasCanvasDrawnLastFrame(), "the panel is still drawing");
	ZENITH_ASSERT_FALSE(xPanel.WasLayerStripDrawnLastFrame(),
		"★ but the strip emitted NOT ONE item — not a header, not a disabled row");
	ZENITH_ASSERT_EQ(xPanel.GetDrawnLayerRowCount(), 0u, "and no rows");
}

ZENITH_TEST(AnimSmPanel, SelectingALayerSelectsItsMachineAndPushesItsBlendModeToTheMaskSubPanel)
{
	// ★ THE SECOND HALF OF "select a layer" IS THE ONE THAT IS EASY TO FORGET,
	// and the one whose absence is invisible. WU-7.1's mask sub-panel offers an
	// assignment control only while its TARGET layer accepts a mask; an additive
	// layer ignores its mask entirely, so a strip that changed the canvas's
	// machine without telling the dope sheet would go on offering the control for
	// whichever layer was last looked at — and the result is a mask authored,
	// saved, assigned and never consulted, with every gate green.
	AnimSmFixture xFixture("zenith_animsm_layerselect");
	Zenith_EditorPanel_AnimStateMachine xPanel;

	ZENITH_ASSERT_TRUE(xPanel.OpenAssetFresh(xFixture.m_strControllerPath), "a fresh controller opens");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddLayer("Base"), "a base layer");
	u_int uBase = uFLUX_INVALID_LAYER_ID;
	ZENITH_ASSERT_TRUE(xPanel.Document().GetLayerIdAt(0u, uBase), "which has an id");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddLayer("Aim"), "and an overlay");
	u_int uAim = uFLUX_INVALID_LAYER_ID;
	ZENITH_ASSERT_TRUE(xPanel.Document().GetLayerIdAt(1u, uAim), "which has its own");

	// Adding a layer selects it, so the AnimSm* verbs author ITS machine.
	ZENITH_ASSERT_EQ(xPanel.GetSelectedLayerId(), uAim, "the new layer is selected");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddState("Aiming"), "a state authored now lands in it");
	ZENITH_ASSERT_EQ(xPanel.Document().GetStateCount(), 1u, "one state here");
	ZENITH_ASSERT_TRUE(xPanel.Action_SelectLayer(uBase), "select the base layer");
	ZENITH_ASSERT_EQ(xPanel.Document().GetStateCount(), 0u,
		"★ whose machine is empty — selecting a layer selected its MACHINE, not just a row");

	// The top-level machine is not a layer, and the list's own verb says so.
	ZENITH_ASSERT_FALSE(xPanel.Action_SelectLayer(uANIMCTRL_TOP_LEVEL_MACHINE),
		"the top-level machine is not a layer — Action_SelectLayerMachine is its route");
	ZENITH_ASSERT_FALSE(xPanel.Action_SelectLayer(uFLUX_INVALID_LAYER_ID), "and an unminted id resolves to nothing");

	// ---- the push into the dope sheet ---------------------------------------
	// ★ THE EDITOR-OWNED dope sheet, because that is the object the push targets.
	// Guarded rather than assumed: this panel is a STACK object here, and
	// Zenith_EditorPanel_Animation::Instance() asserts when the editor has not
	// allocated its panels.
	Zenith_EditorPanel_Animation* pxDopeSheet =
		g_xEngine.HasEditor() ? g_xEngine.Editor().TryGetAnimationPanel() : nullptr;
	ZENITH_ASSERT_NOT_NULL(pxDopeSheet,
		"the editor's dope sheet is reachable from the unit batch (it is allocated in "
		"Zenith_Editor::Initialise, which runs long before RunAllTests)");
	if (pxDopeSheet != nullptr)
	{
		const Flux_LayerBlendMode eRestore = pxDopeSheet->GetMaskTargetLayerBlendMode();

		ZENITH_ASSERT_TRUE(xPanel.Action_SetLayerBlendMode(uAim, LAYER_BLEND_ADDITIVE), "make the overlay additive");
		ZENITH_ASSERT_TRUE(xPanel.Action_SelectLayer(uAim), "and select it");
		ZENITH_ASSERT_TRUE(pxDopeSheet->GetMaskTargetLayerBlendMode() == LAYER_BLEND_ADDITIVE,
			"★ the mask sub-panel now targets an ADDITIVE layer, so it will refuse to offer an assignment");
		ZENITH_ASSERT_FALSE(Zenith_BoneMaskDocument::LayerAcceptsMask(pxDopeSheet->GetMaskTargetLayerBlendMode()),
			"which is the ONE statement of that rule agreeing with the push");

		ZENITH_ASSERT_TRUE(xPanel.Action_SelectLayer(uBase), "select the override layer");
		ZENITH_ASSERT_TRUE(pxDopeSheet->GetMaskTargetLayerBlendMode() == LAYER_BLEND_OVERRIDE,
			"★ and the target follows the selection rather than latching");

		// ★ CHANGING THE SELECTED LAYER'S MODE PUSHES TOO, not only a selection
		// change: without it the sub-panel would go on offering an assignment for
		// a layer that had just stopped accepting one.
		ZENITH_ASSERT_TRUE(xPanel.Action_SetLayerBlendMode(uBase, LAYER_BLEND_ADDITIVE), "make IT additive");
		ZENITH_ASSERT_TRUE(pxDopeSheet->GetMaskTargetLayerBlendMode() == LAYER_BLEND_ADDITIVE,
			"★ and the sub-panel hears about it without a re-selection");

		// The top-level machine has no blend mode, so it pushes OVERRIDE — the
		// sub-panel's own default and the mode a mask means something to.
		ZENITH_ASSERT_TRUE(xPanel.Action_SelectLayerMachine(uANIMCTRL_TOP_LEVEL_MACHINE), "back to the top level");
		ZENITH_ASSERT_TRUE(pxDopeSheet->GetMaskTargetLayerBlendMode() == LAYER_BLEND_OVERRIDE,
			"★ which is not a layer and pushes OVERRIDE rather than whatever was there");

		// This unit writes to an object the whole editor shares; put it back.
		pxDopeSheet->SetMaskTargetLayerBlendMode(eRestore);
	}

	xPanel.CloseAsset();
}

ZENITH_TEST(AnimSmPanel, TheLayerActionsReorderTheListAndRemovingTheSelectedOneFallsBack)
{
	AnimSmFixture xFixture("zenith_animsm_layeractions");
	Zenith_EditorPanel_AnimStateMachine xPanel;

	ZENITH_ASSERT_TRUE(xPanel.OpenAssetFresh(xFixture.m_strControllerPath), "a fresh controller opens");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddLayer("Base"), "Base");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddLayer("Aim"), "Aim");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddLayer("Face"), "Face");
	ZENITH_ASSERT_EQ(xPanel.Document().GetLayerCount(), 3u, "three layers");

	u_int uBase = uFLUX_INVALID_LAYER_ID;
	u_int uFace = uFLUX_INVALID_LAYER_ID;
	ZENITH_ASSERT_TRUE(xPanel.Document().GetLayerIdAt(0u, uBase), "Base's id");
	ZENITH_ASSERT_TRUE(xPanel.Document().GetLayerIdAt(2u, uFace), "Face's id");

	const u_int uDepth = xPanel.Document().GetUndoStackSize();
	ZENITH_ASSERT_TRUE(xPanel.Action_MoveLayer(uFace, 0u), "move the top layer to the base");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), uDepth + 1u, "as ONE undo step");
	u_int uIdAt = uFLUX_INVALID_LAYER_ID;
	ZENITH_ASSERT_TRUE(xPanel.Document().GetLayerIdAt(0u, uIdAt) && uIdAt == uFace, "★ and it is index 0 now");

	// ★ THE SELECTION IS AN ID AND THE REORDER DID NOT TOUCH IT, which is the
	// property WU-6.3 exists for: every index moved and no identity did.
	ZENITH_ASSERT_EQ(xPanel.GetSelectedLayerId(), uFace,
		"the layer that was selected is still selected, wherever the move put it");

	ZENITH_ASSERT_FALSE(xPanel.Action_MoveLayer(uFace, 9u), "a destination past the end is refused");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), uDepth + 1u, "and pushes nothing");

	// ---- removing the SELECTED layer ----------------------------------------
	ZENITH_ASSERT_TRUE(xPanel.Action_SelectLayer(uBase), "select Base");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddState("Idle"), "author a state in it");
	ZENITH_ASSERT_TRUE(xPanel.Action_SelectState("Idle"), "and select that state");
	ZENITH_ASSERT_TRUE(xPanel.Action_RemoveLayer(uBase), "now delete the layer out from under it");

	ZENITH_ASSERT_EQ(xPanel.GetSelectedLayerId(), uANIMCTRL_TOP_LEVEL_MACHINE,
		"★ the machine selection falls back to the top level");
	ZENITH_ASSERT_TRUE(xPanel.GetSelectedStateName().empty(),
		"★ and the STATE selection goes with it — a state name means nothing in a different machine");
	ZENITH_ASSERT_EQ(xPanel.Document().GetLayerCount(), 2u, "two layers left");

	// ---- the additive refusal, through the panel -----------------------------
	u_int uOverlay = uFLUX_INVALID_LAYER_ID;
	ZENITH_ASSERT_TRUE(xPanel.Document().GetLayerIdAt(1u, uOverlay), "the second surviving layer");
	ZENITH_ASSERT_TRUE(xPanel.Action_SetLayerBlendMode(uOverlay, LAYER_BLEND_ADDITIVE), "make it additive");
	ZENITH_ASSERT_FALSE(xPanel.Action_SetLayerMaskAssetPath(uOverlay, "game:Anim/UpperBody.zanimmask"),
		"★ a mask assignment on it is refused");
	ZENITH_ASSERT_EQ(xPanel.GetLayerNotice(), std::string(Zenith_BoneMaskDocument::AdditiveLayerMaskNotice()),
		"★ and the panel forwards the ONE wording of the reason rather than inventing a second");

	xPanel.CloseAsset();
}

//==============================================================================
// The blend-tree sub-graph (WU-7.3)
//==============================================================================

ZENITH_TEST(AnimSmPanel, DraggingABlendPointChangesTheSampledPose)
{
	// ★ THE FLAGSHIP, AND IT IS DELIBERATELY NOT AN ASSERTION ABOUT THE DOCUMENT.
	// Every link between "the author moved a marker" and "the character poses
	// differently" is a place an editor can look right and animate nothing: the
	// verb, the undo snapshot, the re-sort, the Apply, D48's named binding, and
	// the blend itself. This drives all of them and reads the POSE at the end.
	//
	// ★ THE POSE IS SAMPLED AGAINST A REAL ONE-BONE RIG THE TEST OWNS, not
	// against the panel's zero-bone stub. The stub exists so the panel can tick a
	// machine without a skeleton (WU-6.5); it produces no pose by design, so a
	// test that used it would be asserting on an empty one.
	AnimSmFixture xFixture("zenith_animsm_blenddrag");
	// A at identity, B at 90° about Y — so "closer to A" is a quaternion dot
	// against identity, which is monotonic in the blend factor over this range.
	const std::string strClipA = xFixture.WriteRotationClip("a.zanim", "ClipA", 0.0f);
	const std::string strClipB = xFixture.WriteRotationClip("b.zanim", "ClipB", 1.5707963f);

	Zenith_EditorPanel_AnimStateMachine xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenAssetFresh(xFixture.m_strControllerPath), "a fresh controller opens");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddClipPath(strClipA), "clip A is in the def's clip list");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddClipPath(strClipB), "and clip B");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddParameter("Speed", Flux_AnimationParameters::ParamType::Float, 0.0f),
		"Speed is declared");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddState("Locomotion"), "one state");
	ZENITH_ASSERT_TRUE(xPanel.Action_SetStateTreeKind("Locomotion", ZENITH_ANIMCTRL_TREE_BLENDSPACE_1D),
		"whose tree is a 1D blend space");
	ZENITH_ASSERT_TRUE(xPanel.Action_SetBlendSpaceParameter("Locomotion", ZENITH_ANIMCTRL_BLEND_AXIS_X, "Speed"),
		"★ bound to Speed — D48's repair is what makes any of this move");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddBlendPoint("Locomotion", "ClipA", 0.0f, 0.0f), "A at 0");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddBlendPoint("Locomotion", "ClipB", 1.0f, 0.0f), "B at 1");

	ZENITH_ASSERT_TRUE(xPanel.Action_SetPreviewEnabled(true),
		"★ the preview builds and every clip the def names resolves — a false here is a dangling reference");
	ZENITH_ASSERT_TRUE(xPanel.Action_SetPreviewFloat("Speed", 0.5f), "Speed sits halfway between the two points");

	// A real rig: one bone called "Root", which is the bone both clips animate.
	Zenith_SkeletonAsset xSkeleton;
	xSkeleton.AddBone("Root", -1, Zenith_Maths::Vector3(0.0f), glm::identity<Zenith_Maths::Quat>(),
		Zenith_Maths::Vector3(1.0f));
	xSkeleton.ComputeBindPoseMatrices();

	// Heap: a Flux_SkeletonPose carries FLUX_MAX_BONES of transforms and the test
	// already has a panel holding two.
	Flux_SkeletonPose* pxPose = new Flux_SkeletonPose();
	const Zenith_Maths::Quat xIdentity = glm::identity<Zenith_Maths::Quat>();

	Flux_AnimationStateMachine* pxMachine = xPanel.GetPreviewMachine();
	ZENITH_ASSERT_NOT_NULL(pxMachine, "the preview's machine resolves");
	// dt 0: both leaves stay at time 0, where each clip's rotation is constant —
	// so the only thing that can move the pose is the BLEND WEIGHT.
	pxMachine->Update(0.0f, *pxPose, xSkeleton);
	const Zenith_Maths::Quat xHalfWay = pxPose->GetLocalPose(0).m_xRotation;
	const float fDotHalfWay = std::fabs(glm::dot(xHalfWay, xIdentity));
	ZENITH_ASSERT_LT(fDotHalfWay, 0.999f,
		"★ at the halfway parameter the pose is genuinely between the two clips, not sitting on A");

	// ---- move B further out --------------------------------------------------
	// Speed stays at 0.5; with B at 4 instead of 1 the blend factor falls from
	// 0.5 to 0.125, so the pose must move TOWARDS A.
	ZENITH_ASSERT_TRUE(xPanel.Action_SetBlendPointPosition("Locomotion", 1, 4.0f, 0.0f), "drag B out to 4");
	ZENITH_ASSERT_TRUE(xPanel.Action_Apply(),
		"Apply is a RELOAD (D45), so the preview keeps its state and its live Speed across the edit");

	// ★ RE-FETCHED, NEVER CACHED. ReloadFromControllerDef deletes every layer and
	// rebuilds every machine, so the pointer taken before the Apply is freed
	// memory — WU-6.3's D44 rule, one call site over.
	pxMachine = xPanel.GetPreviewMachine();
	ZENITH_ASSERT_NOT_NULL(pxMachine, "the reloaded preview's machine resolves");
	ZENITH_ASSERT_EQ_FLOAT(xPanel.PreviewController().GetParameters().GetFloat("Speed"), 0.5f, 1e-6f,
		"★ and Speed survived the reload — a rebuild would have reset it to its declared default");

	pxMachine->Update(0.0f, *pxPose, xSkeleton);
	const Zenith_Maths::Quat xAfter = pxPose->GetLocalPose(0).m_xRotation;
	const float fDotAfter = std::fabs(glm::dot(xAfter, xIdentity));
	ZENITH_ASSERT_GT(fDotAfter, fDotHalfWay + 0.01f,
		"★ THE SAMPLED POSE MOVED TOWARDS CLIP A — dragging a blend point changed what the character does, "
		"which is the whole claim of this unit");

	// And the undo takes the pose back, so the edit is reversible all the way
	// through to the sampled result rather than only in the document.
	ZENITH_ASSERT_TRUE(xPanel.Action_Undo(), "undo the drag");
	ZENITH_ASSERT_TRUE(xPanel.Action_Apply(), "and apply it");
	pxMachine = xPanel.GetPreviewMachine();
	ZENITH_ASSERT_NOT_NULL(pxMachine, "the machine resolves again");
	pxMachine->Update(0.0f, *pxPose, xSkeleton);
	ZENITH_ASSERT_EQ_FLOAT(std::fabs(glm::dot(pxPose->GetLocalPose(0).m_xRotation, xIdentity)), fDotHalfWay, 1e-3f,
		"★ and the pose is back where it was");

	delete pxPose;
	xPanel.CloseAsset();
}

ZENITH_TEST(AnimSmPanel, TheLiveParameterDotTracksTheBoundParameterOnThePreview)
{
	// ★ THE DOT IS ONLY MEANINGFUL BECAUSE WU-6.1 REPAIRED THE BINDING (D48).
	// Before that, nothing passed Flux_AnimationParameters into a blend tree at
	// all — the position was frozen at its deserialized literal — so a dot drawn
	// from it would have sat still forever and looked like a UI bug.
	AnimSmFixture xFixture("zenith_animsm_livedot");
	const std::string strClip = xFixture.WriteClip("walk.zanim", "WalkClip");

	Zenith_EditorPanel_AnimStateMachine xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenAssetFresh(xFixture.m_strControllerPath), "a fresh controller opens");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddClipPath(strClip), "one clip");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddParameter("Speed", Flux_AnimationParameters::ParamType::Float, 0.0f),
		"Speed is declared");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddState("Locomotion"), "one state");
	ZENITH_ASSERT_TRUE(xPanel.Action_SetStateTreeKind("Locomotion", ZENITH_ANIMCTRL_TREE_BLENDSPACE_1D), "as a 1D space");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddBlendPoint("Locomotion", "WalkClip", 0.0f, 0.0f), "with a point");

	float fDotX = 99.0f;
	float fDotY = 99.0f;
	// The four things that are NOT a dot, each refused rather than answered with
	// a zero — a marker at the origin is indistinguishable from a parameter that
	// happens to be zero.
	ZENITH_ASSERT_FALSE(xPanel.GetLiveParameterDot(fDotX, fDotY), "no preview, no dot");
	ZENITH_ASSERT_TRUE(xPanel.Action_SetPreviewEnabled(true), "the preview builds");
	ZENITH_ASSERT_FALSE(xPanel.GetLiveParameterDot(fDotX, fDotY),
		"★ an UNBOUND space has no dot — it reads no parameter at all");

	ZENITH_ASSERT_TRUE(xPanel.Action_SetBlendSpaceParameter("Locomotion", ZENITH_ANIMCTRL_BLEND_AXIS_X, "Speed"),
		"bind the axis");
	ZENITH_ASSERT_TRUE(xPanel.Action_SetPreviewFloat("Speed", 1.25f), "drive the preview's live set");
	ZENITH_ASSERT_TRUE(xPanel.GetLiveParameterDot(fDotX, fDotY), "★ now there is a dot");
	ZENITH_ASSERT_EQ_FLOAT(fDotX, 1.25f, 1e-6f, "★ reading the LIVE value of the bound parameter");
	ZENITH_ASSERT_EQ_FLOAT(fDotY, 0.0f, 1e-6f, "and y is 0 on a 1D space");

	// ★ AND IT TRACKS. A dot that reported the DECLARED default would pass every
	// assertion above and never move.
	ZENITH_ASSERT_TRUE(xPanel.Action_SetPreviewFloat("Speed", -0.5f), "move the parameter");
	ZENITH_ASSERT_TRUE(xPanel.GetLiveParameterDot(fDotX, fDotY), "the dot still resolves");
	ZENITH_ASSERT_EQ_FLOAT(fDotX, -0.5f, 1e-6f, "★ and it MOVED with the parameter");

	// A state that is not a blend space has no dot, whatever the preview is doing.
	ZENITH_ASSERT_TRUE(xPanel.Action_AddState("Plain"), "a plain state");
	ZENITH_ASSERT_TRUE(xPanel.Action_SelectState("Plain"), "selected");
	ZENITH_ASSERT_FALSE(xPanel.GetLiveParameterDot(fDotX, fDotY), "which has no blend space and therefore no dot");

	xPanel.CloseAsset();
}

ZENITH_TEST(AnimSmPanel, TheBlendStripDrawsNothingForASingleClipStateAndNeverTakesCanvasHeight)
{
	// ★ THE HEIGHT GUARD, MEASURED ON THE CANVAS RECT rather than on "is a marker
	// visible" — Editor/CLAUDE.md's rule, learned on the dope sheet where an
	// always-present collapsed header pushed the events row off the bottom and the
	// failure arrived as a flat `false` a long way from its cause. The strip lives
	// inside the INSPECTOR child, which is a fixed height, so a blend space with
	// points must cost the graph exactly nothing.
	AnimSmFixture xFixture("zenith_animsm_blendheight");
	const std::string strClip = xFixture.WriteClip("walk.zanim", "WalkClip");

	Zenith_EditorPanel_AnimStateMachine xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenAssetFresh(xFixture.m_strControllerPath), "a fresh controller opens");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddClipPath(strClip), "a clip in the list");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddState("Locomotion"), "a state, which becomes the selection");
	ZENITH_ASSERT_TRUE(xPanel.Action_SetStateClip("Locomotion", "WalkClip"), "playing a single clip");

	xPanel.RequestWindowPlacement(20.0f, 20.0f, 1200.0f, 640.0f);
	AnimSmRenderFrames(xPanel, 2);

	// The discriminators first: a bare `false` has four causes and the height
	// below would be meaningless against any of them.
	ZENITH_ASSERT_TRUE(xPanel.WasCanvasDrawnLastFrame(), "the canvas was drawn");
	ZENITH_ASSERT_EQ_FLOAT(xPanel.GetRecordedDisplayWidth(), fANIMSM_DISPLAY_W, 0.5f,
		"and the display bound was captured AT RECORD TIME");
	ZENITH_ASSERT_FALSE(xPanel.WasBlendStripDrawnLastFrame(),
		"★ the strip emitted NOT ONE item for a single-clip state — not a header, not a disabled row");
	ZENITH_ASSERT_EQ(xPanel.GetDrawnBlendPointCount(), 0u, "and no markers");
	Zenith_AnimCtrlPanelRect xUnused;
	ZENITH_ASSERT_FALSE(xPanel.GetBlendStripRect(xUnused), "so the strip rect refuses");

	Zenith_AnimCtrlPanelRect xCanvasPlain;
	ZENITH_ASSERT_TRUE(xPanel.GetCanvasRect(xCanvasPlain), "the canvas publishes");
	const float fHeightPlain = xCanvasPlain.Height();
	ZENITH_ASSERT_GT(fHeightPlain, 1.0f, "with a real height");

	// ---- a blend space with four more points, and the canvas must not move ---
	// ★ A 1D SPACE ON PURPOSE. The strip lives inside the inspector child, which
	// SCROLLS, and a clipped ImGui item is not interactable — so the marker rects
	// are recorded only while the surface is visible. A 1D strip is one axis row
	// tall and fits; a 2D square is deliberately not what a height guard should
	// hinge on, because then a failure would be about the fixture's layout rather
	// than about the canvas.
	ZENITH_ASSERT_TRUE(xPanel.Action_SetStateTreeKind("Locomotion", ZENITH_ANIMCTRL_TREE_BLENDSPACE_1D),
		"convert to a 1D space");
	for (u_int u = 0; u < 4u; ++u)
	{
		ZENITH_ASSERT_TRUE(xPanel.Action_AddBlendPoint("Locomotion", "WalkClip",
			static_cast<float>(u) + 1.0f, 0.0f), "a point");
	}
	AnimSmRenderFrames(xPanel, 2);

	ZENITH_ASSERT_TRUE(xPanel.WasBlendStripDrawnLastFrame(), "★ now the strip draws");
	ZENITH_ASSERT_EQ(xPanel.GetDrawnBlendPointCount(), 5u,
		"with all five markers painted inside it (the seeded one plus the four added)");
	Zenith_AnimCtrlPanelRect xStrip;
	ZENITH_ASSERT_TRUE(xPanel.GetBlendStripRect(xStrip), "and the strip rect publishes");
	ZENITH_ASSERT_GT(xStrip.Width(), 1.0f, "with a real width");

	Zenith_AnimCtrlPanelRect xCanvasSpace;
	ZENITH_ASSERT_TRUE(xPanel.GetCanvasRect(xCanvasSpace), "the canvas still publishes");
	ZENITH_ASSERT_EQ_FLOAT(xCanvasSpace.Height(), fHeightPlain, 0.5f,
		"★ a blend space with five markers costs the canvas NOTHING — the strip is inside the fixed-height "
		"inspector child, so a sub-graph cannot push the graph off the bottom the way an always-drawn "
		"section did on the dope sheet");

	// ★ AND THE EQUALITY ABOVE IS NOT AN EQUALITY WITH A CONSTANT. A height that
	// never moved would satisfy it just as well. Deliberately in the GROWING
	// direction, for the reason the layer strip's guard grows too: shrinking far
	// enough to be convincing can drive the canvas below RenderCanvas's 8 px floor
	// on a high-DPI machine.
	xPanel.RequestWindowPlacement(20.0f, 20.0f, 1200.0f, 780.0f);
	AnimSmRenderFrames(xPanel, 2);
	Zenith_AnimCtrlPanelRect xCanvasTall;
	ZENITH_ASSERT_TRUE(xPanel.GetCanvasRect(xCanvasTall), "the canvas publishes in the taller window");
	ZENITH_ASSERT_GT(xCanvasTall.Height(), fHeightPlain + 60.0f,
		"★ and a 140 px taller window really does grow it — the guard measures something");

	// Deselecting takes the strip away again, so "drawn" tracks the SELECTION and
	// not merely "a blend space exists somewhere in this machine".
	ZENITH_ASSERT_TRUE(xPanel.Action_ClearSelection(), "clear the selection");
	AnimSmRenderFrames(xPanel, 2);
	ZENITH_ASSERT_FALSE(xPanel.WasBlendStripDrawnLastFrame(),
		"★ and with nothing selected the strip emits nothing again");

	xPanel.CloseAsset();
}

ZENITH_TEST(AnimSmPanel, ThePixelToPositionMappingRoundTripsAndTheDragGoesThroughIt)
{
	// ★ ONE MAPPING, ASSERTED AS A PURE FUNCTION FIRST. The dope sheet's rule
	// (Zenith_AnimTimelineMath) applied to a blend axis: the draw places a marker
	// with BlendPositionToPixel and the drag reads a position back with
	// BlendPixelToPosition, so if the two are not inverses a marker sits where a
	// click does not land — and no amount of clicking tells you which half is
	// wrong.
	for (u_int u = 0; u <= 10u; ++u)
	{
		const float fPosition = -3.0f + static_cast<float>(u) * 0.9f;
		const float fPixel = Zenith_EditorPanel_AnimStateMachine::BlendPositionToPixel(
			fPosition, 100.0f, 420.0f, -3.0f, 6.0f);
		const float fBack = Zenith_EditorPanel_AnimStateMachine::BlendPixelToPosition(
			fPixel, 100.0f, 420.0f, -3.0f, 6.0f);
		ZENITH_ASSERT_EQ_FLOAT(fBack, fPosition, 1e-3f, "★ position -> pixel -> position is the identity");
	}
	// The two ends land on the two edges, so the range the strip advertises is
	// the range it actually draws.
	ZENITH_ASSERT_EQ_FLOAT(Zenith_EditorPanel_AnimStateMachine::BlendPositionToPixel(-3.0f, 100.0f, 420.0f, -3.0f, 6.0f),
		100.0f, 1e-3f, "the low end is the left edge");
	ZENITH_ASSERT_EQ_FLOAT(Zenith_EditorPanel_AnimStateMachine::BlendPositionToPixel(6.0f, 100.0f, 420.0f, -3.0f, 6.0f),
		420.0f, 1e-3f, "and the high end is the right edge");
	// A DEGENERATE range cannot be divided through, and answering the low edge
	// would stack every marker on the frame.
	ZENITH_ASSERT_EQ_FLOAT(Zenith_EditorPanel_AnimStateMachine::BlendPositionToPixel(5.0f, 100.0f, 420.0f, 2.0f, 2.0f),
		260.0f, 1e-3f, "a zero-span range maps everything to the middle");

	// The range helper: padded, and floored so a one-point space still has an
	// axis to drag along.
	float fRangeMin = 0.0f;
	float fRangeMax = 0.0f;
	Zenith_EditorPanel_AnimStateMachine::ComputeBlendAxisRange(0.0f, 10.0f, fRangeMin, fRangeMax);
	ZENITH_ASSERT_LT(fRangeMin, 0.0f, "the range is padded below the lowest point");
	ZENITH_ASSERT_GT(fRangeMax, 10.0f, "and above the highest");
	Zenith_EditorPanel_AnimStateMachine::ComputeBlendAxisRange(2.0f, 2.0f, fRangeMin, fRangeMax);
	ZENITH_ASSERT_GE(fRangeMax - fRangeMin, fANIMSM_BLEND_MIN_SPAN - 1e-4f,
		"★ and a degenerate span is widened — otherwise every pixel of the strip means one position");

	// ---- and the DRAG goes through exactly that mapping ----------------------
	AnimSmFixture xFixture("zenith_animsm_blenddragpixel");
	const std::string strClip = xFixture.WriteClip("walk.zanim", "WalkClip");

	Zenith_EditorPanel_AnimStateMachine xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenAssetFresh(xFixture.m_strControllerPath), "a fresh controller opens");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddClipPath(strClip), "a clip");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddState("Locomotion"), "a state");
	ZENITH_ASSERT_TRUE(xPanel.Action_SetStateTreeKind("Locomotion", ZENITH_ANIMCTRL_TREE_BLENDSPACE_1D), "as a 1D space");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddBlendPoint("Locomotion", "WalkClip", 0.0f, 0.0f), "one point at 0");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddBlendPoint("Locomotion", "WalkClip", 4.0f, 0.0f), "and one at 4");

	// ★ BEFORE ANY FRAME, THE DRAG REFUSES. The mapping is (strip rect, axis
	// range) and BOTH are recorded by the draw — inventing either would drop the
	// point at a position the strip never showed.
	ZENITH_ASSERT_TRUE(xPanel.Action_SelectBlendPoint(1u), "select the far point");
	ZENITH_ASSERT_FALSE(xPanel.Action_DragBlendPointToPixel(1u, 500.0f, 400.0f),
		"★ a drag with no drawn strip is refused rather than guessed at");

	xPanel.RequestWindowPlacement(20.0f, 20.0f, 1200.0f, 640.0f);
	AnimSmRenderFrames(xPanel, 2);
	ZENITH_ASSERT_TRUE(xPanel.WasBlendStripDrawnLastFrame(), "the strip drew");

	Zenith_AnimCtrlPanelRect xStrip;
	ZENITH_ASSERT_TRUE(xPanel.GetBlendStripRect(xStrip), "and published its frame");
	float fAxisMin = 0.0f;
	float fAxisMax = 0.0f;
	ZENITH_ASSERT_TRUE(xPanel.GetBlendAxisRange(ZENITH_ANIMCTRL_BLEND_AXIS_X, fAxisMin, fAxisMax),
		"and the axis range it drew with");

	// Drop the point a quarter of the way along the strip and assert the document
	// holds exactly what the pure mapping says it should.
	const float fTargetPixel = xStrip.m_fMinX + xStrip.Width() * 0.25f;
	const float fExpected = Zenith_EditorPanel_AnimStateMachine::BlendPixelToPosition(
		fTargetPixel, xStrip.m_fMinX, xStrip.m_fMaxX, fAxisMin, fAxisMax);
	ZENITH_ASSERT_TRUE(xPanel.Action_DragBlendPointToPixel(1u, fTargetPixel, xStrip.Centre().y),
		"the drag commits");

	// ★ THE POINT MAY HAVE RENUMBERED — the 1D list is kept sorted — so the
	// SELECTION is what says where it went, and the selection is what a drag
	// leaves correct.
	const u_int uNow = xPanel.GetSelectedBlendPoint();
	ZENITH_ASSERT_TRUE(uNow != uINVALID_ANIMSM_BLEND_POINT, "and the selection followed the point");
	std::string strFound;
	Zenith_Maths::Vector2 xFound(0.0f);
	ZENITH_ASSERT_TRUE(xPanel.Document().GetBlendPoint("Locomotion", uNow, strFound, xFound), "which reads back");
	ZENITH_ASSERT_EQ_FLOAT(xFound.x, fExpected, 1e-3f,
		"★ at exactly the position the strip's own mapping puts that pixel at");

	xPanel.CloseAsset();
}
