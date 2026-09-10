// Included after the automation animation probe writers.
#include "Editor/Panels/Zenith_EditorPanel_AnimStateMachine.h"
#include "UnitTests/Zenith_AssertCapture.h"

namespace
{
	struct AutomationAnimationScratch
	{
		std::filesystem::path m_xDirectory;
		AutomationAnimationScratch(const char* szName)
		{
			m_xDirectory = std::filesystem::temp_directory_path() / szName;
			std::filesystem::create_directories(m_xDirectory);
		}
		std::string Path(const char* szName) const { return (m_xDirectory / szName).generic_string(); }
		~AutomationAnimationScratch()
		{
			auto& xPanel = Zenith_EditorPanel_Animation::Instance();
			xPanel.CloseClip();
			xPanel.Document().SetAuthoredRootOverride("");
			xPanel.Action_SetPoseAngleSnap(false);
			xPanel.ShowFlag() = false;
			auto& xSm = Zenith_EditorPanel_AnimStateMachine::Instance();
			xSm.CloseAsset();
			xSm.ShowFlag() = false;
			Zenith_AssetRegistry::UnloadUnused();
			std::error_code xError;
			std::filesystem::remove_all(m_xDirectory, xError);
		}
	};

	void AutomationDrain(Zenith_EditorAutomation& xAuto)
	{
		Zenith_AssertCaptureScope xCapture;
		xAuto.Begin();
		while (!xAuto.IsComplete())
		{
			xAuto.ExecuteNextStep();
		}
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 0u, "successful recipe must not trip a checked wrapper");
		xAuto.Reset();
	}

	void AutomationExpectRejected(Zenith_EditorAutomation& xAuto)
	{
		Zenith_AssertCaptureScope xCapture;
		xAuto.Begin();
		xAuto.ExecuteNextStep();
		ZENITH_ASSERT_TRUE(xCapture.DidAssertFire(), "invalid recipe step must report refusal");
		xAuto.Reset();
	}
}

ZENITH_TEST(Automation, AnimSaveAndSaveAsPersistEditedKeys)
{
	AutomationAnimationScratch xScratch("zenith_automation_clip_persistence");
	const auto strSource = xScratch.Path("source.zanim");
	const auto strCopy = xScratch.Path("copy.zanim");
	AutomationWriteAnimProbe(strSource);
	Zenith_EditorAutomation xAuto;
	xAuto.AddStep_AnimOpenClip(strSource.c_str());
	xAuto.AddStep_AnimSaveAs(strCopy.c_str());
	xAuto.AddStep_AnimSelectKey("Hip", FLUX_ANIM_TRACK_POSITION, 1, ZENITH_ANIMSELECT_REPLACE);
	xAuto.AddStep_AnimMoveSelection(0.25f, false);
	xAuto.AddStep_AnimSave();
	xAuto.AddStep_AnimCloseClip();
	AutomationDrain(xAuto);
	// Evict cached bytes before reopening: this must read the saved file.
	Zenith_AssetRegistry::UnloadUnused();
	xAuto.AddStep_AnimOpenClip(strCopy.c_str());
	AutomationDrain(xAuto);
	auto& xPanel = Zenith_EditorPanel_Animation::Instance();
	const auto xTrack = Zenith_AnimTrackId::Bone("Hip", FLUX_ANIM_TRACK_POSITION);
	float fTime = 0.0f;
	ZENITH_ASSERT_EQ(xPanel.Document().GetAssetPath(), strCopy, "SaveAs adopts the new path");
	ZENITH_ASSERT_TRUE(xPanel.Document().GetKeyTime(xTrack, xPanel.Document().GetKeyIdAtIndex(xTrack, 1), fTime), "saved key exists");
	ZENITH_ASSERT_EQ_FLOAT(fTime, 1.25f, 0.0f, "Save persisted the retime");
	ZENITH_ASSERT_FALSE(xPanel.Document().IsDirty(), "reopened copy is clean");
	xAuto.AddStep_AnimCloseClip();
	xAuto.AddStep_AnimOpenClip(strSource.c_str());
	AutomationDrain(xAuto);
	ZENITH_ASSERT_TRUE(xPanel.Document().GetKeyTime(xTrack, xPanel.Document().GetKeyIdAtIndex(xTrack, 1), fTime), "source key exists");
	ZENITH_ASSERT_EQ_FLOAT(fTime, 1.0f, 0.0f, "SaveAs left the source unchanged");
}

ZENITH_TEST(Automation, AnimPromotionPersistsAnAuthoredCopyInTheSandbox)
{
	AutomationAnimationScratch xScratch("zenith_automation_clip_promotion");
	const auto strSource = xScratch.Path("generated.zanim");
	Flux_AnimationClip xClip;
	xClip.SetName("Generated");
	xClip.SetDuration(2.0f);
	xClip.GetMetadata().m_bGenerated = true;
	xClip.Export(strSource);
	auto& xPanel = Zenith_EditorPanel_Animation::Instance();
	xPanel.Document().SetAuthoredRootOverride(xScratch.Path("Authored"));
	Zenith_EditorAutomation xAuto;
	xAuto.AddStep_AnimPromoteToAuthoredOverride(strSource.c_str());
	xAuto.AddStep_AnimEventAdd(0.25f, "PromotedEvent");
	xAuto.AddStep_AnimSave();
	AutomationDrain(xAuto);
	const auto strPromoted = xPanel.Document().GetAssetPath();
	ZENITH_ASSERT_EQ(strPromoted, xScratch.Path("Authored/generated.zanim"), "promotion stays in scratch root");
	ZENITH_ASSERT_FALSE(xPanel.Document().GetClip().GetMetadata().m_bGenerated, "copy is authored");
	xAuto.AddStep_AnimCloseClip();
	AutomationDrain(xAuto);
	Zenith_AssetRegistry::UnloadUnused();
	xAuto.AddStep_AnimOpenClip(strPromoted.c_str());
	AutomationDrain(xAuto);
	ZENITH_ASSERT_EQ(xPanel.Document().GetEventCount(), 1u, "promoted edit persisted");
	Zenith_DataStream xBytes;
	xBytes.ReadFromFile(strSource.c_str());
	Flux_AnimationClip xOriginal;
	xOriginal.ReadFromDataStream(xBytes);
	ZENITH_ASSERT_TRUE(xOriginal.GetMetadata().m_bGenerated, "source remains generated");
	ZENITH_ASSERT_EQ(xOriginal.GetEvents().GetSize(), 0u, "source was not edited");
}

ZENITH_TEST(Automation, AnimEventStepsResolveIndicesAfterReorderingAndPersist)
{
	AutomationAnimationScratch xScratch("zenith_automation_events");
	const auto strPath = xScratch.Path("events.zanim");
	AutomationWriteAnimProbe(strPath);
	Zenith_EditorAutomation xAuto;
	xAuto.AddStep_AnimOpenClip(strPath.c_str());
	xAuto.AddStep_AnimEventAdd(0.25f, "First");
	xAuto.AddStep_AnimEventAdd(0.5f, "Second");
	AutomationDrain(xAuto);
	auto& xPanel = Zenith_EditorPanel_Animation::Instance();
	const u_int uFirst = xPanel.Document().GetEventIdAtIndex(0);
	xAuto.AddStep_AnimEventSelect(0, ZENITH_ANIMSELECT_REPLACE);
	AutomationDrain(xAuto);
	ZENITH_ASSERT_TRUE(xPanel.IsEventSelected(uFirst), "selection resolved the event ID");
	xAuto.AddStep_AnimEventMoveSelected(0.5f, false);
	xAuto.AddStep_AnimEventRename(1, "Moved");
	xAuto.AddStep_AnimEventSetPayload(1, 1.25f, -2.5f, 3.75f, -4.0f);
	xAuto.AddStep_AnimEventSetEmitEventsOnScrub(true);
	xAuto.AddStep_AnimEventSetEmitEventsOnScrub(true);
	AutomationDrain(xAuto);
	Flux_AnimationEvent xEvent;
	ZENITH_ASSERT_TRUE(xPanel.Document().GetEvent(uFirst, xEvent), "stable event survived reorder");
	ZENITH_ASSERT_EQ_FLOAT(xEvent.m_fNormalizedTime, 0.75f, 0.0f, "normalized delta applied");
	ZENITH_ASSERT_EQ(xEvent.m_strEventName, std::string("Moved"), "rename resolved the new index");
	ZENITH_ASSERT_EQ_FLOAT(xEvent.m_xData.x, 1.25f, 0.0f, "payload x");
	ZENITH_ASSERT_EQ_FLOAT(xEvent.m_xData.y, -2.5f, 0.0f, "payload y");
	ZENITH_ASSERT_EQ_FLOAT(xEvent.m_xData.z, 3.75f, 0.0f, "payload z");
	ZENITH_ASSERT_EQ_FLOAT(xEvent.m_xData.w, -4.0f, 0.0f, "payload w");
	ZENITH_ASSERT_TRUE(xPanel.GetEmitEventsOnScrub(), "repeated toggle remains enabled");
	xAuto.AddStep_AnimEventSetEmitEventsOnScrub(false);
	xAuto.AddStep_AnimSave();
	xAuto.AddStep_AnimCloseClip();
	AutomationDrain(xAuto);
	Zenith_AssetRegistry::UnloadUnused();
	xAuto.AddStep_AnimOpenClip(strPath.c_str());
	AutomationDrain(xAuto);
	ZENITH_ASSERT_EQ(xPanel.Document().GetEventCount(), 2u, "both events saved");
	ZENITH_ASSERT_TRUE(xPanel.Document().GetEvent(xPanel.Document().GetEventIdAtIndex(1), xEvent), "disk event resolves");
	ZENITH_ASSERT_EQ(xEvent.m_strEventName, std::string("Moved"), "event rename persisted");
	ZENITH_ASSERT_EQ_FLOAT(xEvent.m_xData.w, -4.0f, 0.0f, "event payload persisted");
}

ZENITH_TEST(Automation, AnimPoseControlStepsReachTheRig)
{
	AutomationAnimationScratch xScratch("zenith_automation_pose_controls");
	const auto strClip = xScratch.Path("pose.zanim");
	AutomationWriteRiggedAnimProbe(strClip, xScratch.Path("pose.zskel"), xScratch.Path("pose.zasset"));
	Zenith_EditorAutomation xAuto;
	xAuto.AddStep_AnimOpenClip(strClip.c_str());
	xAuto.AddStep_AnimSetPoseAngleSnap(true);
	xAuto.AddStep_AnimSetPoseAngleSnap(true);
	xAuto.AddStep_AnimSelectBone(0);
	xAuto.AddStep_AnimSetKeyTranslationForRoot();
	AutomationDrain(xAuto);
	auto& xPanel = Zenith_EditorPanel_Animation::Instance();
	ZENITH_ASSERT_TRUE(xPanel.Action_GetPoseAngleSnap(), "snap enabled idempotently");
	ZENITH_ASSERT_EQ(xPanel.Document().GetKeyCount(Zenith_AnimTrackId::Bone("Hip", FLUX_ANIM_TRACK_POSITION)), 2u, "root translation replaces the time-zero key");
	ZENITH_ASSERT_EQ(xPanel.Document().GetKeyCount(Zenith_AnimTrackId::Bone("Hip", FLUX_ANIM_TRACK_ROTATION)), 1u, "root rotation keyed with translation");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 1u, "root keying is one undoable edit");
	xAuto.AddStep_AnimClearBoneSelection();
	xAuto.AddStep_AnimClearBoneSelection();
	xAuto.AddStep_AnimSetPoseAngleSnap(false);
	AutomationDrain(xAuto);
	ZENITH_ASSERT_FALSE(xPanel.Session().HasBoneSelection(), "clear is idempotent");
	ZENITH_ASSERT_FALSE(xPanel.Action_GetPoseAngleSnap(), "snap disabled");
}

ZENITH_TEST(Automation, AnimSmEditStepsDriveSelectionLayoutAndClipRemoval)
{
	AutomationAnimationScratch xScratch("zenith_automation_sm_edits");
	Zenith_EditorAutomation xAuto;
	xAuto.AddStep_AnimSmOpenFresh(xScratch.Path("controller.zanimctrl").c_str());
	xAuto.AddStep_AnimSmAddState("Idle");
	xAuto.AddStep_AnimSmAddState("Walk");
	xAuto.AddStep_AnimSmAddTransition("Idle", "Walk");
	xAuto.AddStep_AnimSmSelectState("Idle");
	AutomationDrain(xAuto);
	auto& xPanel = Zenith_EditorPanel_AnimStateMachine::Instance();
	ZENITH_ASSERT_EQ(xPanel.GetSelectedStateName(), std::string("Idle"), "state selected");
	xAuto.AddStep_AnimSmSelectTransition("Idle", 0);
	AutomationDrain(xAuto);
	std::string strFrom;
	u_int uIndex = 42;
	ZENITH_ASSERT_TRUE(xPanel.GetSelectedTransition(strFrom, uIndex), "transition selected");
	ZENITH_ASSERT_EQ(strFrom, std::string("Idle"), "transition source");
	ZENITH_ASSERT_EQ(uIndex, 0u, "transition index");
	xAuto.AddStep_AnimSmSelectAnyState();
	AutomationDrain(xAuto);
	ZENITH_ASSERT_TRUE(xPanel.IsAnyStateSelected(), "pseudo-node selected");
	ZENITH_ASSERT_FALSE(xPanel.GetSelectedTransition(strFrom, uIndex), "transition selection cleared");
	xAuto.AddStep_AnimSmClearSelection();
	xAuto.AddStep_AnimSmClearSelection();
	xAuto.AddStep_AnimSmSetStatePosition("Idle", -125.0f, 275.0f);
	xAuto.AddStep_AnimSmAddClipPath(xScratch.Path("unused.zanim").c_str());
	xAuto.AddStep_AnimSmRemoveClipPath(xScratch.Path("unused.zanim").c_str());
	AutomationDrain(xAuto);
	ZENITH_ASSERT_FALSE(xPanel.IsAnyStateSelected(), "clear removed pseudo-node selection");
	ZENITH_ASSERT_TRUE(xPanel.GetSelectedStateName().empty(), "no state selected");
	Zenith_Maths::Vector2 xPosition;
	ZENITH_ASSERT_TRUE(xPanel.Document().GetStateEditorPosition("Idle", xPosition), "position exists");
	ZENITH_ASSERT_EQ_FLOAT(xPosition.x, -125.0f, 0.0f, "position x");
	ZENITH_ASSERT_EQ_FLOAT(xPosition.y, 275.0f, 0.0f, "position y");
	ZENITH_ASSERT_EQ(xPanel.Document().GetClipPathCount(), 0u, "clip path removed");
}

ZENITH_TEST(Automation, AnimSmPreviewStepsDriveParametersAndTransition)
{
	AutomationAnimationScratch xScratch("zenith_automation_sm_preview");
	const auto strClip = xScratch.Path("preview.zanim");
	AutomationWriteAnimProbe(strClip);
	Zenith_EditorAutomation xAuto;
	xAuto.AddStep_AnimSmOpenFresh(xScratch.Path("controller.zanimctrl").c_str());
	xAuto.AddStep_AnimSmAddClipPath(strClip.c_str());
	xAuto.AddStep_AnimSmAddState("Idle");
	xAuto.AddStep_AnimSmAddState("Walk");
	xAuto.AddStep_AnimSmSetStateClip("Idle", "AutomationAnimProbe");
	xAuto.AddStep_AnimSmSetStateClip("Walk", "AutomationAnimProbe");
	xAuto.AddStep_AnimSmAddParameter("Speed", static_cast<int>(Flux_AnimationParameters::ParamType::Float), 0.0f);
	xAuto.AddStep_AnimSmAddParameter("Count", static_cast<int>(Flux_AnimationParameters::ParamType::Int), 0.0f);
	xAuto.AddStep_AnimSmAddParameter("Enabled", static_cast<int>(Flux_AnimationParameters::ParamType::Bool), 0.0f);
	xAuto.AddStep_AnimSmAddParameter("Fire", static_cast<int>(Flux_AnimationParameters::ParamType::Trigger), 0.0f);
	xAuto.AddStep_AnimSmAddTransition("Idle", "Walk");
	xAuto.AddStep_AnimSmAddCondition("Idle", 0, "Speed", static_cast<int>(Flux_TransitionCondition::CompareOp::Greater), 0.1f);
	xAuto.AddStep_AnimSmSetPreviewEnabled(true);
	AutomationDrain(xAuto);
	auto& xPanel = Zenith_EditorPanel_AnimStateMachine::Instance();
	ZENITH_ASSERT_TRUE(xPanel.IsPreviewEnabled(), "preview enabled");
	ZENITH_ASSERT_TRUE(xPanel.IsPreviewComplete(), "all clips resolved");
	xAuto.AddStep_AnimSmSetPreviewFloat("Speed", 1.25f);
	xAuto.AddStep_AnimSmSetPreviewInt("Count", -7);
	xAuto.AddStep_AnimSmSetPreviewBool("Enabled", true);
	xAuto.AddStep_AnimSmSetPreviewTrigger("Fire");
	AutomationDrain(xAuto);
	const auto& xParams = xPanel.PreviewController().GetParameters();
	ZENITH_ASSERT_EQ_FLOAT(xParams.GetFloat("Speed"), 1.25f, 0.0f, "float parameter");
	ZENITH_ASSERT_EQ(xParams.GetInt("Count"), -7, "signed int parameter");
	ZENITH_ASSERT_TRUE(xParams.GetBool("Enabled"), "bool parameter");
	ZENITH_ASSERT_TRUE(xParams.PeekTrigger("Fire"), "trigger parameter");
	xAuto.AddStep_AnimSmSetPreviewFloat("Typo", 99.0f);
	AutomationExpectRejected(xAuto);
	xAuto.AddStep_AnimSmSetPreviewInt("Speed", 99);
	AutomationExpectRejected(xAuto);
	xAuto.AddStep_AnimSmSetPreviewBool("Count", false);
	AutomationExpectRejected(xAuto);
	xAuto.AddStep_AnimSmSetPreviewTrigger("Enabled");
	AutomationExpectRejected(xAuto);
	ZENITH_ASSERT_EQ_FLOAT(xParams.GetFloat("Speed"), 1.25f, 0.0f, "wrong-type write left live value intact");
	xAuto.AddStep_AnimSmTickPreview(1.0f / 60.0f);
	xAuto.AddStep_AnimSmTickPreview(0.5f);
	xAuto.AddStep_AnimSmExpectPreviewState("Walk");
	AutomationDrain(xAuto);
	ZENITH_ASSERT_EQ(xPanel.GetHighlightedStateName(), std::string("Walk"), "ticks transitioned the live preview");
	xAuto.AddStep_AnimSmSetPreviewBool("Enabled", false);
	xAuto.AddStep_AnimSmSetPreviewEnabled(false);
	AutomationDrain(xAuto);
	ZENITH_ASSERT_FALSE(xPanel.IsPreviewEnabled(), "preview disabled");
}

ZENITH_TEST(Automation, AnimPanelSaveRefusesMissingDocumentsAndExternalChanges)
{
	AutomationAnimationScratch xScratch("zenith_automation_save_conflict");
	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_FALSE(xPanel.Action_Save(), "no document to save");
	ZENITH_ASSERT_FALSE(xPanel.Action_SaveAs(xScratch.Path("none.zanim")), "no document to copy");
	const auto strPath = xScratch.Path("clip.zanim");
	AutomationWriteAnimProbe(strPath);
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(strPath), "open authored file");
	ZENITH_ASSERT_TRUE(xPanel.Action_AddEvent(0.25f, "Working"), "dirty edit");
	Flux_AnimationClip xExternal;
	xExternal.SetName("ExternalWriter");
	xExternal.SetDuration(3.0f);
	xExternal.Export(strPath);
	ZENITH_ASSERT_FALSE(xPanel.Action_Save(), "external change is not overwritten");
	ZENITH_ASSERT_TRUE(xPanel.HasExternalConflict(), "conflict is visible immediately");
	ZENITH_ASSERT_TRUE(xPanel.Document().IsDirty(), "failed save retains working edits");
	ZENITH_ASSERT_TRUE(xPanel.Action_SaveAs(xScratch.Path("resolved.zanim")), "SaveAs preserves edits separately");
	ZENITH_ASSERT_FALSE(xPanel.HasExternalConflict(), "successful SaveAs clears conflict");
	xPanel.CloseClip();
}

ZENITH_TEST(Automation, AnimEventStepsRejectInvalidTargetsAndNames)
{
	AutomationAnimationScratch xScratch("zenith_automation_event_refusals");
	const auto strPath = xScratch.Path("clip.zanim");
	AutomationWriteAnimProbe(strPath);
	Zenith_EditorAutomation xAuto;
	xAuto.AddStep_AnimOpenClip(strPath.c_str());
	AutomationDrain(xAuto);
	xAuto.AddStep_AnimEventAdd(0.25f, "");
	AutomationExpectRejected(xAuto);
	xAuto.AddStep_AnimEventSelect(-1, ZENITH_ANIMSELECT_REPLACE);
	AutomationExpectRejected(xAuto);
	xAuto.AddStep_AnimEventRename(99, "Missing");
	AutomationExpectRejected(xAuto);
	xAuto.AddStep_AnimEventSetPayload(99, 1, 2, 3, 4);
	AutomationExpectRejected(xAuto);
	xAuto.AddStep_AnimEventMoveSelected(0.25f, false);
	AutomationExpectRejected(xAuto);
	ZENITH_ASSERT_EQ(Zenith_EditorPanel_Animation::Instance().Document().GetEventCount(), 0u, "refusals leave document intact");
	xAuto.AddStep_AnimCloseClip();
	AutomationDrain(xAuto);
	xAuto.AddStep_AnimEventSetEmitEventsOnScrub(false);
	AutomationExpectRejected(xAuto);
}

ZENITH_TEST(Automation, AnimSmStepsRejectMissingTargetsAndInactivePreview)
{
	AutomationAnimationScratch xScratch("zenith_automation_sm_refusals");
	Zenith_EditorAutomation xAuto;
	xAuto.AddStep_AnimSmOpenFresh(xScratch.Path("controller.zanimctrl").c_str());
	AutomationDrain(xAuto);
	xAuto.AddStep_AnimSmSelectState("Missing");
	AutomationExpectRejected(xAuto);
	xAuto.AddStep_AnimSmSelectTransition("", 99);
	AutomationExpectRejected(xAuto);
	xAuto.AddStep_AnimSmSetStatePosition("Missing", 10, 20);
	AutomationExpectRejected(xAuto);
	xAuto.AddStep_AnimSmRemoveClipPath("missing.zanim");
	AutomationExpectRejected(xAuto);
	xAuto.AddStep_AnimSmTickPreview(0.1f);
	AutomationExpectRejected(xAuto);
	xAuto.AddStep_AnimSmSetPreviewFloat("Missing", 1.0f);
	AutomationExpectRejected(xAuto);
	xAuto.AddStep_AnimSmSetPreviewInt("Missing", 1);
	AutomationExpectRejected(xAuto);
	xAuto.AddStep_AnimSmSetPreviewBool("Missing", true);
	AutomationExpectRejected(xAuto);
	xAuto.AddStep_AnimSmSetPreviewTrigger("Missing");
	AutomationExpectRejected(xAuto);
	xAuto.AddStep_AnimSmExpectPreviewState("Missing");
	AutomationExpectRejected(xAuto);
	ZENITH_ASSERT_EQ(Zenith_EditorPanel_AnimStateMachine::Instance().Document().GetStateCount(), 0u, "refusals did not create states");
}

ZENITH_TEST(Automation, AnimClipAndRootKeyStepsRejectMissingDocuments)
{
	AutomationAnimationScratch xScratch("zenith_automation_clip_refusals");
	Zenith_EditorAutomation xAuto;
	xAuto.AddStep_AnimSave();
	AutomationExpectRejected(xAuto);
	xAuto.AddStep_AnimSaveAs(xScratch.Path("missing.zanim").c_str());
	AutomationExpectRejected(xAuto);
	xAuto.AddStep_AnimPromoteToAuthoredOverride(xScratch.Path("missing.zanim").c_str());
	AutomationExpectRejected(xAuto);
	xAuto.AddStep_AnimSetKeyTranslationForRoot();
	AutomationExpectRejected(xAuto);
	ZENITH_ASSERT_FALSE(std::filesystem::exists(xScratch.Path("missing.zanim")), "no failed operation wrote a file");
}
