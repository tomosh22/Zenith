//------------------------------------------------------------------------------
// Zenith_EditorPanel_Animation unit tests (WU-3.2 rendering + WU-3.3 operations
// + WU-5B events).
// Included at the bottom of Zenith_EditorPanel_Animation.cpp.
//
// ★ THESE DRIVE A REAL ImGui FRAME, HEADLESS, AND NONE OF THEM IS
// requiresGraphics. ImGui is genuinely live on the Null backend — the context is
// real, the font atlas is rasterised, layout and hit-testing behave exactly as
// they do windowed, and only the draw data is discarded (Zenith/Null/CLAUDE.md).
// The boot-time unit batch runs inside Zenith_Init AFTER Flux has created that
// context and BEFORE the main loop's first frame, so a NewFrame/EndFrame pair
// here is a legal, self-contained frame. AnimPanelImGuiFrame below is the whole
// mechanism: pin DisplaySize, pin a dt, park the mouse somewhere invalid so no
// hover or click can vary between runs, and put every field back afterwards.
//
// ★ THE HEADLINE PROPERTY, and the reason the panel exposes rects at all: a
// recorded rect AGREES WITH WU-3.1's PURE MAPPING, and an off-screen one comes
// back FALSE rather than as a coordinate. The graph editor learned the second
// half the expensive way — it used to hand out the virtual, scrolled-away rect,
// so a test clicked screen y=1768 on a 720-tall display and reported only "the
// nodes were not created" with the click, the bridge and the panel all healthy.
//
// The clips are plain data in a private temp directory, removed on the way out.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"
#include "UnitTests/Zenith_UnitTests.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "Flux/RenderViews/Flux_PreviewSlotArbiter.h"
// WU-4.3's manipulator units read the joint the rings are centred on straight
// from WU-4.2's conversions, and compare the panel's answer against it — two
// derivations through different code, which is what makes the round trip a real
// assertion rather than a value checked against a re-computation of itself.
#include "Editor/Animation/Zenith_BoneSpace.h"
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"

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
	// time would refuse every rect in the assertions below. The panel captures its
	// display bound at record time for exactly this reason
	// (GetRecordedDisplayWidth); do not "fix" a future failure of this shape by
	// leaving a display size behind, which would only hide it until something
	// queried between frames.
	//--------------------------------------------------------------------------
	struct AnimPanelImGuiFrame
	{
		AnimPanelImGuiFrame(float fWidth, float fHeight)
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

		~AnimPanelImGuiFrame()
		{
			ImGui::EndFrame();
			ImGuiIO& xIO = ImGui::GetIO();
			xIO.DisplaySize = m_xSavedDisplaySize;
			xIO.DeltaTime = m_fSavedDeltaTime;
			xIO.MousePos = m_xSavedMousePos;
			xIO.MouseWheel = m_fSavedMouseWheel;
		}

		AnimPanelImGuiFrame(const AnimPanelImGuiFrame&) = delete;
		AnimPanelImGuiFrame& operator=(const AnimPanelImGuiFrame&) = delete;

		ImVec2 m_xSavedDisplaySize;
		ImVec2 m_xSavedMousePos;
		float m_fSavedDeltaTime = 0.0f;
		float m_fSavedMouseWheel = 0.0f;
	};

	constexpr float fANIMPANEL_DISPLAY_W = 1280.0f;
	constexpr float fANIMPANEL_DISPLAY_H = 720.0f;

	void AnimPanelRenderFrames(Zenith_EditorPanel_Animation& xPanel, u_int uFrames)
	{
		for (u_int u = 0; u < uFrames; ++u)
		{
			AnimPanelImGuiFrame xFrame(fANIMPANEL_DISPLAY_W, fANIMPANEL_DISPLAY_H);
			// dt 0: nothing here is testing playback, and a moving play head would
			// move the playhead rect between two frames a test is comparing.
			xPanel.Render(0.0f);
		}
	}

	//--------------------------------------------------------------------------
	// Fixture — the shape Zenith_AnimationDocument.Tests.inl established: a
	// private temp directory removed on the way out, plus a ForceUnload of the
	// one registry path these tests cause to be loaded, so a throwaway clip never
	// lingers in the live registry the suite runs inside.
	//
	// ★ IT ALSO RESETS THE PREVIEW-SLOT ARBITER AT BOTH ENDS, exactly as
	// Zenith_AnimationPreviewSession.Tests.inl does: opening a clip opens a
	// preview session, which CLAIMS the process-level slot, and a unit that left
	// it claimed would hand its claim to the next one.
	//
	// ★ DECLARE THE FIXTURE BEFORE THE PANEL IN EVERY TEST. The panel owns a
	// document and a session, both of which hold owning asset handles;
	// ForceUnload deletes regardless of refcount, so the panel has to be
	// destroyed first and declaration order is what guarantees that.
	//--------------------------------------------------------------------------
	struct AnimPanelFixture
	{
		std::filesystem::path m_xDirectory;
		std::string m_strPath;
		// Only written by AnimPanelWriteRiggedProbe. ForceUnload on a path that was
		// never loaded is a no-op, so they are torn down unconditionally.
		std::string m_strSkeletonPath;
		std::string m_strMeshPath;
		// WU-7.1's mask sub-panel. Same rule: written only by the tests that want
		// one, torn down unconditionally.
		std::string m_strMaskPath;

		explicit AnimPanelFixture(const char* szLeafDirectory)
		{
			Flux_PreviewSlotArbiter::ResetForTesting();

			std::error_code xError;
			std::filesystem::path xRoot = std::filesystem::temp_directory_path(xError);
			if (xError)
			{
				xRoot = ".";
			}
			m_xDirectory = xRoot / szLeafDirectory;
			std::filesystem::remove_all(m_xDirectory, xError);
			std::filesystem::create_directories(m_xDirectory, xError);
			m_strPath = (m_xDirectory / "sheet.zanim").generic_string();
			m_strSkeletonPath = (m_xDirectory / "sheet.zskel").generic_string();
			m_strMeshPath = (m_xDirectory / "sheet.zasset").generic_string();
			m_strMaskPath = (m_xDirectory / "sheet.zanimmask").generic_string();
		}

		~AnimPanelFixture()
		{
			Zenith_AssetRegistry::ForceUnload(m_strPath);
			Zenith_AssetRegistry::ForceUnload(m_strSkeletonPath);
			Zenith_AssetRegistry::ForceUnload(m_strMeshPath);
			Zenith_AssetRegistry::ForceUnload(m_strMaskPath);
			std::error_code xError;
			std::filesystem::remove_all(m_xDirectory, xError);
			Flux_PreviewSlotArbiter::ResetForTesting();
		}

		AnimPanelFixture(const AnimPanelFixture&) = delete;
		AnimPanelFixture& operator=(const AnimPanelFixture&) = delete;
	};

	// One bone, THREE position keys at 0 / 1 / 2 s, duration 2 s. Three keys at
	// distinct times is the minimum that can tell "the near one is visible and
	// the far one is not" apart from "nothing was recorded".
	void AnimPanelWriteProbe(const std::string& strPath, bool bGenerated)
	{
		Flux_AnimationClip xClip;
		xClip.SetName("SheetProbe");
		xClip.SetDuration(2.0f);
		xClip.GetMetadata().m_bGenerated = bGenerated;
		xClip.GetMetadata().m_uAuthoredFrameRate = 30u;

		Flux_BoneChannel xHip;
		xHip.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
		xHip.AddPositionKeyframe(1.0f, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
		xHip.AddPositionKeyframe(2.0f, Zenith_Maths::Vector3(0.0f, 2.0f, 0.0f));
		xHip.SortKeyframes();
		xClip.AddBoneChannel("Hip", std::move(xHip));

		xClip.Export(strPath);
	}

	// Enough bones that the row list is TALLER THAN ANY WINDOW — which is the
	// state the off-screen gate exists for, and the same state the graph
	// editor's palette is permanently in.
	void AnimPanelWriteTallProbe(const std::string& strPath, u_int uBoneCount)
	{
		Flux_AnimationClip xClip;
		xClip.SetName("TallProbe");
		xClip.SetDuration(2.0f);
		xClip.GetMetadata().m_uAuthoredFrameRate = 30u;

		for (u_int u = 0; u < uBoneCount; ++u)
		{
			char acName[32];
			snprintf(acName, sizeof(acName), "Bone%02u", u);
			Flux_BoneChannel xChannel;
			xChannel.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0.0f));
			xChannel.AddPositionKeyframe(2.0f, Zenith_Maths::Vector3(1.0f));
			xChannel.SortKeyframes();
			xClip.AddBoneChannel(acName, std::move(xChannel));
		}

		xClip.Export(strPath);
	}

	// Hip (position keys at 0 / 1 / 2, y = 0 / 1 / 2) plus Spine, which has ONE
	// position key and no rotation or scale keys at all. What a cross-bone paste
	// needs: a source with something to copy and a target that is animated but
	// not on every track.
	void AnimPanelWriteTwoBoneProbe(const std::string& strPath)
	{
		Flux_AnimationClip xClip;
		xClip.SetName("TwoBoneProbe");
		xClip.SetDuration(2.0f);
		xClip.GetMetadata().m_bGenerated = false;
		xClip.GetMetadata().m_uAuthoredFrameRate = 30u;

		Flux_BoneChannel xHip;
		xHip.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
		xHip.AddPositionKeyframe(1.0f, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
		xHip.AddPositionKeyframe(2.0f, Zenith_Maths::Vector3(0.0f, 2.0f, 0.0f));
		xHip.SortKeyframes();
		xClip.AddBoneChannel("Hip", std::move(xHip));

		Flux_BoneChannel xSpine;
		xSpine.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0.0f, 5.0f, 0.0f));
		xSpine.SortKeyframes();
		xClip.AddBoneChannel("Spine", std::move(xSpine));

		xClip.Export(strPath);
	}

	// A probe whose metadata records a REAL rig, so the preview session resolves
	// and Seek actually does something. Both assets are tiny, hand-built and
	// device-free — the mesh exists only to be a resolvable preview PATH, which is
	// why it is not GenerateUnitCube (that helper ends in EnsureGPUBuffers).
	void AnimPanelWriteRiggedProbe(const AnimPanelFixture& xFixture)
	{
		{
			Zenith_SkeletonAsset xSkeleton;
			const Zenith_Maths::Quat xIdentity(1.0f, 0.0f, 0.0f, 0.0f);
			const Zenith_Maths::Vector3 xUnitScale(1.0f);
			xSkeleton.AddBone("Hip", -1, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), xIdentity, xUnitScale);
			xSkeleton.AddBone("Spine", 0, Zenith_Maths::Vector3(0.0f, 0.5f, 0.0f), xIdentity, xUnitScale);
			xSkeleton.ComputeBindPoseMatrices();
			xSkeleton.Export(xFixture.m_strSkeletonPath.c_str());
		}
		{
			Zenith_MeshAsset xMesh;
			xMesh.Reserve(3, 3);
			xMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f), Zenith_Maths::Vector2(0.0f, 0.0f));
			xMesh.AddVertex(Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f), Zenith_Maths::Vector2(1.0f, 0.0f));
			xMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f), Zenith_Maths::Vector2(0.0f, 1.0f));
			xMesh.AddTriangle(0u, 1u, 2u);
			xMesh.AddSubmesh(0u, 3u, 0u);
			xMesh.ComputeBounds();
			xMesh.Export(xFixture.m_strMeshPath.c_str());
		}

		Flux_AnimationClip xClip;
		xClip.SetName("RiggedProbe");
		xClip.SetDuration(2.0f);
		// ★ NOT LOOPING, and that is load-bearing for the scrub test rather than a
		// preference. Flux_AnimationController::WrapClipTime WRAPS for a looping
		// clip and CLAMPS for one that does not loop, so a seek to exactly the
		// duration would fold back to 0 on a looping clip — and "the clamp works"
		// and "the clamp wrapped all the way round" would be indistinguishable.
		xClip.SetLooping(false);
		xClip.GetMetadata().m_uAuthoredFrameRate = 30u;
		xClip.GetMetadata().m_strSkeletonPath = xFixture.m_strSkeletonPath;
		xClip.GetMetadata().m_strPreviewModelPath = xFixture.m_strMeshPath;

		Flux_BoneChannel xHip;
		xHip.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
		xHip.AddPositionKeyframe(2.0f, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
		xHip.SortKeyframes();
		xClip.AddBoneChannel("Hip", std::move(xHip));

		xClip.Export(xFixture.m_strPath);
	}

	Zenith_AnimTrackId AnimPanelHipPosition()
	{
		return Zenith_AnimTrackId::Bone("Hip", FLUX_ANIM_TRACK_POSITION);
	}

	Zenith_AnimTrackId AnimPanelSpinePosition()
	{
		return Zenith_AnimTrackId::Bone("Spine", FLUX_ANIM_TRACK_POSITION);
	}

	// Every key time on a track, in index order — what a bit-exact restore is
	// compared against.
	void AnimPanelSnapshotTimes(const Zenith_AnimationDocument& xDoc, const Zenith_AnimTrackId& xTrack,
		Zenith_Vector<float>& afOut)
	{
		afOut.Clear();
		const u_int uCount = xDoc.GetKeyCount(xTrack);
		for (u_int u = 0; u < uCount; ++u)
		{
			float fTime = 0.0f;
			xDoc.GetKeyTime(xTrack, xDoc.GetKeyIdAtIndex(xTrack, u), fTime);
			afOut.PushBack(fTime);
		}
	}

	// The id of the Hip position key sitting at fTimeSeconds, or 0.
	u_int AnimPanelKeyIdAtTime(const Zenith_AnimationDocument& xDoc, const Zenith_AnimTrackId& xTrack, float fTimeSeconds)
	{
		const u_int uCount = xDoc.GetKeyCount(xTrack);
		for (u_int u = 0; u < uCount; ++u)
		{
			const u_int uId = xDoc.GetKeyIdAtIndex(xTrack, u);
			float fTime = 0.0f;
			if (xDoc.GetKeyTime(xTrack, uId, fTime) && std::fabs(fTime - fTimeSeconds) < 1.0e-4f)
			{
				return uId;
			}
		}
		return uINVALID_ANIM_KEY_ID;
	}
}

//==============================================================================
// (1) The panel opens an authored clip and draws a frame HEADLESS.
//
// This is the load-bearing one for everything below it: if a panel cannot be
// rendered without a graphics driver, every rect test would have to be
// requiresGraphics — and a requiresGraphics test is SKIPPED-AS-PASSED headless,
// which is how coverage rots without anything going red.
//==============================================================================
ZENITH_TEST(AnimPanel, OpensAnAuthoredClipAndRendersHeadless)
{
	AnimPanelFixture xFixture("zenith_animpanel_open");
	AnimPanelWriteProbe(xFixture.m_strPath, /*bGenerated*/ false);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "an authored .zanim opens into the panel");
	ZENITH_ASSERT_TRUE(xPanel.IsOpen(), "and the panel reports itself open");
	ZENITH_ASSERT_TRUE(xPanel.GetLastOpenResult() == ZENITH_ANIMDOC_OPEN_OK, "with no refusal recorded");
	ZENITH_ASSERT_TRUE(xPanel.IsShown(), "opening a clip shows the window");
	ZENITH_ASSERT_EQ(xPanel.GetRenderedFrameCount(), 0u, "nothing has been drawn yet");

	xPanel.RequestWindowPlacement(40.0f, 40.0f, 900.0f, 600.0f);
	AnimPanelRenderFrames(xPanel, 2u);

	ZENITH_ASSERT_EQ(xPanel.GetRenderedFrameCount(), 2u, "both frames drew the window");
	ZENITH_ASSERT_TRUE(xPanel.IsOpen(), "and the document survived them");

	// ★ THE DIAGNOSTICS COME BEFORE THE RECT ASSERTIONS, DELIBERATELY. Every
	// accessor below answers a flat `false` for four different causes — never
	// drawn, no room, scrolled away, outside the display bound — and the first
	// time these units failed, all anyone could see was "nothing was published"
	// while the panel was in fact drawing correctly. These three separate the
	// four, so the next failure of this shape says which one it is on the line
	// that fails rather than needing a rebuild to find out.
	ZENITH_ASSERT_TRUE(xPanel.WasSheetDrawnLastFrame(), "the sheet pass ran (else: the window never opened its body)");
	ZENITH_ASSERT_GT(xPanel.GetLastTrackWidth(), 0.0f, "the key lane has width (else: every rect was culled before recording)");
	ZENITH_ASSERT_GT(xPanel.GetRecordedDisplayWidth(), 0.0f,
		"a display bound was captured at record time (else: every publish refuses, whatever was drawn)");

	Zenith_AnimPanelRect xRuler;
	ZENITH_ASSERT_TRUE(xPanel.GetRulerRect(xRuler), "the ruler was drawn and is on screen");
	ZENITH_ASSERT_GT(xRuler.Width(), 0.0f, "the ruler spans the track area");

	Zenith_AnimPanelRect xPlayhead;
	ZENITH_ASSERT_TRUE(xPanel.GetPlayheadRect(xPlayhead), "the play head is drawn at t=0, which is in view");

	// The preview session opens over the SAME clip; this probe records no rig, so
	// the panel is in its rig-prompt state rather than showing an image. That is
	// what keeps this test off a graphics driver.
	ZENITH_ASSERT_TRUE(xPanel.Session().IsOpen(), "the preview session opened alongside the document");
	ZENITH_ASSERT_TRUE(xPanel.Session().NeedsRigSelection(), "a clip with no recorded rig asks for one");
}

//==============================================================================
// (2) A key's recorded rect IS WU-3.1's mapping, and does not move between two
// frames of an unchanged panel.
//
// The equality is the whole reason the rects can be trusted without a
// screenshot: if the panel ever grew its own seconds->pixels arithmetic, this is
// the assertion that would catch the two copies drifting.
//==============================================================================
ZENITH_TEST(AnimPanel, KeyRectsAreStableAndAgreeWithTheTimelineMapping)
{
	AnimPanelFixture xFixture("zenith_animpanel_rects");
	AnimPanelWriteProbe(xFixture.m_strPath, /*bGenerated*/ false);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the probe clip opens");

	const Zenith_AnimTrackId xTrack = AnimPanelHipPosition();
	const u_int uKeyAtOne = AnimPanelKeyIdAtTime(xPanel.Document(), xTrack, 1.0f);
	ZENITH_ASSERT_NE(uKeyAtOne, uINVALID_ANIM_KEY_ID, "the probe's midpoint key resolves to an id");

	xPanel.RequestWindowPlacement(40.0f, 40.0f, 900.0f, 600.0f);
	AnimPanelRenderFrames(xPanel, 2u);

	Zenith_AnimPanelRect xFirst;
	ZENITH_ASSERT_TRUE(xPanel.GetKeyRect(xTrack, uKeyAtOne, xFirst), "the key at t=1 is on screen and recorded");

	const float fExpectedX = Zenith_AnimTimelineTimeToPixel(xPanel.View(), 1.0f);
	ZENITH_ASSERT_EQ_FLOAT(xFirst.Centre().x, fExpectedX, 1.0f,
		"the key's centre x IS Zenith_AnimTimelineTimeToPixel(view, t) — the panel derives no mapping of its own");

	Zenith_AnimPanelRect xRow;
	ZENITH_ASSERT_TRUE(xPanel.GetRowTrackRect(xTrack, xRow), "so is its row's key lane");
	ZENITH_ASSERT_GE(xFirst.Centre().y, xRow.m_fMinY, "and the key sits inside its own row");
	ZENITH_ASSERT_LE(xFirst.Centre().y, xRow.m_fMaxY, "and the key sits inside its own row");

	// Nothing changed between these two frames, so nothing may move.
	AnimPanelRenderFrames(xPanel, 1u);
	Zenith_AnimPanelRect xSecond;
	ZENITH_ASSERT_TRUE(xPanel.GetKeyRect(xTrack, uKeyAtOne, xSecond), "and it is still recorded a frame later");
	ZENITH_ASSERT_EQ_FLOAT(xSecond.m_fMinX, xFirst.m_fMinX, 0.001f, "a rect is stable across two unchanged frames");
	ZENITH_ASSERT_EQ_FLOAT(xSecond.m_fMinY, xFirst.m_fMinY, 0.001f, "a rect is stable across two unchanged frames");
	ZENITH_ASSERT_EQ_FLOAT(xSecond.m_fMaxX, xFirst.m_fMaxX, 0.001f, "a rect is stable across two unchanged frames");
	ZENITH_ASSERT_EQ_FLOAT(xSecond.m_fMaxY, xFirst.m_fMaxY, 0.001f, "a rect is stable across two unchanged frames");
}

//==============================================================================
// (3a) A key scrolled OUT OF THE TRACK is refused, not reported.
//==============================================================================
ZENITH_TEST(AnimPanel, AnOffScreenKeyRectIsRefusedRatherThanReported)
{
	AnimPanelFixture xFixture("zenith_animpanel_offscreen_key");
	AnimPanelWriteProbe(xFixture.m_strPath, /*bGenerated*/ false);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the probe clip opens");

	const Zenith_AnimTrackId xTrack = AnimPanelHipPosition();
	const u_int uKeyAtZero = AnimPanelKeyIdAtTime(xPanel.Document(), xTrack, 0.0f);
	const u_int uKeyAtOne = AnimPanelKeyIdAtTime(xPanel.Document(), xTrack, 1.0f);
	ZENITH_ASSERT_NE(uKeyAtZero, uINVALID_ANIM_KEY_ID, "the probe's first key resolves");
	ZENITH_ASSERT_NE(uKeyAtOne, uINVALID_ANIM_KEY_ID, "the probe's midpoint key resolves");

	xPanel.RequestWindowPlacement(40.0f, 40.0f, 900.0f, 600.0f);
	AnimPanelRenderFrames(xPanel, 2u);

	Zenith_AnimPanelRect xRect;
	ZENITH_ASSERT_TRUE(xPanel.GetKeyRect(xTrack, uKeyAtZero, xRect), "zoomed out, every key is in view");

	// Zoom right in and scroll to the midpoint. Both outer keys are then a long
	// way outside the track and only the middle one is reachable.
	Zenith_AnimTimelineView xView = xPanel.View();
	xView.m_fPixelsPerSecond = fANIM_TIMELINE_MAX_PPS;
	xPanel.SetView(xView);
	// ★ THE REQUIRED PRECURSOR. Like ScrollPaletteEntryIntoView, this is applied
	// by the NEXT Render — reading a rect without giving it a frame is exactly
	// the mistake the contract exists to make impossible to make silently.
	xPanel.ScrollTimeIntoView(1.0f);
	AnimPanelRenderFrames(xPanel, 2u);

	ZENITH_ASSERT_TRUE(xPanel.GetKeyRect(xTrack, uKeyAtOne, xRect), "the scrolled-to key is in view");
	ZENITH_ASSERT_FALSE(xPanel.GetKeyRect(xTrack, uKeyAtZero, xRect),
		"a key scrolled past the left edge is REFUSED, not handed out as a coordinate no click can reach");

	// The row itself is still perfectly visible — only its key is not. Refusing
	// the whole row here would be just as wrong as reporting the key.
	ZENITH_ASSERT_TRUE(xPanel.GetRowTrackRect(xTrack, xRect), "the row is still on screen");
}

//==============================================================================
// (3b) A row scrolled BELOW the sheet is refused too, and ScrollRowIntoView is
// how a caller gets it back — the vertical half of the same contract.
//==============================================================================
ZENITH_TEST(AnimPanel, AnOffScreenRowRectIsRefusedUntilScrolledIntoView)
{
	AnimPanelFixture xFixture("zenith_animpanel_offscreen_row");
	AnimPanelWriteTallProbe(xFixture.m_strPath, 30u);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the tall probe clip opens");
	// 30 bones x (1 header + 3 tracks) + a root-motion header + its 2 tracks + events.
	ZENITH_ASSERT_EQ(xPanel.GetRowCount(), 30u * 4u + 3u + 1u, "every bone contributes a header and three track rows");

	const Zenith_AnimTrackId xFirstTrack = Zenith_AnimTrackId::Bone("Bone00", FLUX_ANIM_TRACK_POSITION);
	const Zenith_AnimTrackId xLastTrack = Zenith_AnimTrackId::Bone("Bone29", FLUX_ANIM_TRACK_POSITION);

	xPanel.RequestWindowPlacement(40.0f, 40.0f, 900.0f, 600.0f);
	AnimPanelRenderFrames(xPanel, 2u);

	Zenith_AnimPanelRect xRect;
	ZENITH_ASSERT_TRUE(xPanel.GetRowRect(xFirstTrack, xRect), "the first bone's row is drawn");
	ZENITH_ASSERT_FALSE(xPanel.GetRowRect(xLastTrack, xRect),
		"the last bone's row is far below the sheet and is REFUSED rather than given a virtual coordinate");

	ZENITH_ASSERT_TRUE(xPanel.ScrollRowIntoView(xLastTrack), "the row exists, so the scroll request is accepted");
	AnimPanelRenderFrames(xPanel, 2u);
	ZENITH_ASSERT_TRUE(xPanel.GetRowRect(xLastTrack, xRect), "and after a frame it resolves");
	ZENITH_ASSERT_GE(xRect.Centre().y, 0.0f, "at a coordinate inside the display");
	ZENITH_ASSERT_LE(xRect.Centre().y, fANIMPANEL_DISPLAY_H, "at a coordinate inside the display");
}

//==============================================================================
// (4) A GENERATED clip is refused, and the refusal is SURFACED as its own value
// so the panel can offer the promotion (D21) rather than a bare failure.
//==============================================================================
ZENITH_TEST(AnimPanel, OpeningAGeneratedClipIsRefusedAndSurfaced)
{
	AnimPanelFixture xFixture("zenith_animpanel_generated");
	AnimPanelWriteProbe(xFixture.m_strPath, /*bGenerated*/ true);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_FALSE(xPanel.OpenClip(xFixture.m_strPath), "a generated clip does not open for in-place editing");
	ZENITH_ASSERT_TRUE(xPanel.GetLastOpenResult() == ZENITH_ANIMDOC_OPEN_REFUSED_GENERATED,
		"and the panel records WHICH refusal it was, which is what selects the promotion offer");
	ZENITH_ASSERT_FALSE(xPanel.IsOpen(), "nothing was opened");
	ZENITH_ASSERT_STREQ(xPanel.GetLastOpenAttemptPath().c_str(), xFixture.m_strPath.c_str(),
		"the attempted path is kept so the offer can name it");

	// The banner path draws with no document open — a refusal must not leave the
	// panel unable to render.
	xPanel.RequestWindowPlacement(40.0f, 40.0f, 900.0f, 320.0f);
	AnimPanelRenderFrames(xPanel, 1u);
	ZENITH_ASSERT_EQ(xPanel.GetRenderedFrameCount(), 1u, "the refusal banner renders");

	Zenith_AnimPanelRect xRect;
	ZENITH_ASSERT_FALSE(xPanel.GetKeyRect(AnimPanelHipPosition(), 1u, xRect), "and there is nothing to hit-test");
}

//==============================================================================
// (5) D13 — a key past the duration is counted and flagged.
//
// Shrinking a duration does not move a key, so the clip keeps holding keys
// nothing will ever sample. Silence is the failure mode: they round-trip
// through every save and simply stop having an effect.
//==============================================================================
ZENITH_TEST(AnimPanel, KeysPastTheDurationAreFlagged)
{
	AnimPanelFixture xFixture("zenith_animpanel_pastduration");
	AnimPanelWriteProbe(xFixture.m_strPath, /*bGenerated*/ false);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the probe clip opens");
	ZENITH_ASSERT_EQ(xPanel.GetKeysPastDurationCount(), 0u, "a 2 s clip with keys at 0/1/2 has none past its end");

	const Zenith_AnimTrackId xTrack = AnimPanelHipPosition();
	const u_int uKeyAtZero = AnimPanelKeyIdAtTime(xPanel.Document(), xTrack, 0.0f);
	const u_int uKeyAtTwo = AnimPanelKeyIdAtTime(xPanel.Document(), xTrack, 2.0f);
	ZENITH_ASSERT_NE(uKeyAtTwo, uINVALID_ANIM_KEY_ID, "the probe's last key resolves");

	ZENITH_ASSERT_TRUE(xPanel.Document().SetDuration(1.0f), "the duration shrinks to 1 s");

	xPanel.RequestWindowPlacement(40.0f, 40.0f, 900.0f, 600.0f);
	AnimPanelRenderFrames(xPanel, 1u);

	ZENITH_ASSERT_EQ(xPanel.GetKeysPastDurationCount(), 1u, "the key at t=2 is now past the end");
	ZENITH_ASSERT_TRUE(xPanel.IsKeyPastDuration(xTrack, uKeyAtTwo), "and is flagged by id");
	ZENITH_ASSERT_FALSE(xPanel.IsKeyPastDuration(xTrack, uKeyAtZero), "while the key at t=0 is not");

	// A key sitting exactly ON the duration is SAMPLED, so it must not be
	// flagged — the comparison uses the clip's own fANIM_TIME_EPSILON.
	ZENITH_ASSERT_TRUE(xPanel.Document().SetDuration(2.0f), "and the duration goes back");
	AnimPanelRenderFrames(xPanel, 1u);
	ZENITH_ASSERT_EQ(xPanel.GetKeysPastDurationCount(), 0u, "a key exactly on the duration is not past it");
}

//==============================================================================
// (6) The row model: three sub-rows per bone, TWO for root motion, events last.
//
// The root-motion count is the one that would rot silently. D16 says there is no
// scale delta, so a third row there would offer a lane every document verb
// refuses — and the only symptom would be a click that does nothing.
//==============================================================================
ZENITH_TEST(AnimPanel, RowModelCarriesEveryAddressableTrackAndNoOthers)
{
	AnimPanelFixture xFixture("zenith_animpanel_rows");
	AnimPanelWriteProbe(xFixture.m_strPath, /*bGenerated*/ false);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the probe clip opens");

	u_int uRow = 0u;
	ZENITH_ASSERT_TRUE(xPanel.FindRowIndexForTrack(Zenith_AnimTrackId::Bone("Hip", FLUX_ANIM_TRACK_POSITION), uRow),
		"the bone has a translation row");
	ZENITH_ASSERT_TRUE(xPanel.FindRowIndexForTrack(Zenith_AnimTrackId::Bone("Hip", FLUX_ANIM_TRACK_ROTATION), uRow),
		"a rotation row, even though the probe authored no rotation keys — a channel is deleted when its last key goes (D14), and a vanishing row would leave nowhere to put one back");
	ZENITH_ASSERT_TRUE(xPanel.FindRowIndexForTrack(Zenith_AnimTrackId::Bone("Hip", FLUX_ANIM_TRACK_SCALE), uRow),
		"and a scale row");

	ZENITH_ASSERT_TRUE(xPanel.FindRowIndexForTrack(Zenith_AnimTrackId::RootMotion(FLUX_ANIM_TRACK_POSITION), uRow),
		"root motion has a position delta row");
	ZENITH_ASSERT_TRUE(xPanel.FindRowIndexForTrack(Zenith_AnimTrackId::RootMotion(FLUX_ANIM_TRACK_ROTATION), uRow),
		"and a rotation delta row");
	ZENITH_ASSERT_FALSE(xPanel.FindRowIndexForTrack(Zenith_AnimTrackId::RootMotion(FLUX_ANIM_TRACK_SCALE), uRow),
		"and NO scale row: there is no scale delta (D16)");

	ZENITH_ASSERT_EQ(xPanel.GetEventsRowIndex(), xPanel.GetRowCount() - 1u, "the events row is last");

	// 1 bone header + 3 tracks + root header + 2 tracks + events.
	ZENITH_ASSERT_EQ(xPanel.GetRowCount(), 8u, "one bone produces exactly eight rows");

	// Collapsing a bone hides its tracks and nothing else.
	xPanel.SetGroupCollapsed("Hip", true);
	ZENITH_ASSERT_FALSE(xPanel.FindRowIndexForTrack(Zenith_AnimTrackId::Bone("Hip", FLUX_ANIM_TRACK_POSITION), uRow),
		"a collapsed bone's track rows are gone");
	ZENITH_ASSERT_EQ(xPanel.GetRowCount(), 5u, "leaving the header, root motion and events");
	ZENITH_ASSERT_TRUE(xPanel.FindRowIndexForTrack(Zenith_AnimTrackId::RootMotion(FLUX_ANIM_TRACK_POSITION), uRow),
		"root motion is a separate group and is unaffected");
}

//==============================================================================
// (7) A panel that did not DRAW reports no rects at all.
//
// The other half of the off-screen contract, and the one a hidden panel or an
// unselected dock tab exercises every frame: last frame's coordinates are just
// as unclickable as a scrolled-away one.
//==============================================================================
ZENITH_TEST(AnimPanel, AnUndrawnPanelReportsNoRects)
{
	AnimPanelFixture xFixture("zenith_animpanel_undrawn");
	AnimPanelWriteProbe(xFixture.m_strPath, /*bGenerated*/ false);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the probe clip opens");

	const Zenith_AnimTrackId xTrack = AnimPanelHipPosition();
	const u_int uKeyAtZero = AnimPanelKeyIdAtTime(xPanel.Document(), xTrack, 0.0f);

	xPanel.RequestWindowPlacement(40.0f, 40.0f, 900.0f, 600.0f);
	AnimPanelRenderFrames(xPanel, 2u);

	Zenith_AnimPanelRect xRect;
	ZENITH_ASSERT_TRUE(xPanel.GetKeyRect(xTrack, uKeyAtZero, xRect), "the key is recorded while the panel draws");
	ZENITH_ASSERT_TRUE(xPanel.GetRulerRect(xRect), "so is the ruler");

	xPanel.ShowFlag() = false;
	AnimPanelRenderFrames(xPanel, 1u);

	ZENITH_ASSERT_FALSE(xPanel.GetKeyRect(xTrack, uKeyAtZero, xRect), "a hidden panel reports no key rect");
	ZENITH_ASSERT_FALSE(xPanel.GetRulerRect(xRect), "nor a ruler");
	ZENITH_ASSERT_FALSE(xPanel.GetPlayheadRect(xRect), "nor a play head");
	ZENITH_ASSERT_TRUE(xPanel.IsOpen(), "but the document is untouched — hiding a window is not closing a clip");
}

//==============================================================================
//                        WU-3.3 — OPERATIONS AND UNDO
//
// ★ EVERY ONE OF THESE DRIVES AN Action_* DIRECTLY, NOT A SYNTHESISED CLICK.
// That is the point of the actions existing: a failure here names the OPERATION,
// where a failure through simulated input could equally be the click, the input
// bridge, the hit-rect or the operation, and reports only "the key did not
// move". The two tests that DO need screen coordinates (box select) go through
// the published rect accessors, which is the same path a click takes.
//
// ★ THE RECURRING ASSERTION IS "THE SELECTION SURVIVES". Stable ids exist so
// that an edit — and its undo — hand the user back the keys they had picked;
// every mutating test below re-checks its ids after the undo, because a
// selection quietly emptied by an edit is invisible until the NEXT operation
// does less than it was asked to.
//==============================================================================

//==============================================================================
// (8) The three select modes, and the refusal of an id the document never
// issued.
//==============================================================================
ZENITH_TEST(AnimPanel, SelectModesReplaceAddAndToggleComposeAsClicksDo)
{
	AnimPanelFixture xFixture("zenith_animpanel_selectmodes");
	AnimPanelWriteProbe(xFixture.m_strPath, /*bGenerated*/ false);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the probe clip opens");

	const Zenith_AnimTrackId xTrack = AnimPanelHipPosition();
	const u_int uKey0 = AnimPanelKeyIdAtTime(xPanel.Document(), xTrack, 0.0f);
	const u_int uKey1 = AnimPanelKeyIdAtTime(xPanel.Document(), xTrack, 1.0f);
	const u_int uKey2 = AnimPanelKeyIdAtTime(xPanel.Document(), xTrack, 2.0f);
	ZENITH_ASSERT_NE(uKey0, uINVALID_ANIM_KEY_ID, "the probe's three keys resolve");
	ZENITH_ASSERT_NE(uKey1, uINVALID_ANIM_KEY_ID, "the probe's three keys resolve");
	ZENITH_ASSERT_NE(uKey2, uINVALID_ANIM_KEY_ID, "the probe's three keys resolve");

	ZENITH_ASSERT_EQ(xPanel.GetSelectedKeyCount(), 0u, "nothing is selected to begin with");

	ZENITH_ASSERT_TRUE(xPanel.Action_SelectKey(xTrack, uKey0, ZENITH_ANIMSELECT_REPLACE), "a plain click selects");
	ZENITH_ASSERT_EQ(xPanel.GetSelectedKeyCount(), 1u, "exactly one key");
	ZENITH_ASSERT_TRUE(xPanel.IsKeySelected(xTrack, uKey0), "and it is the one clicked");

	ZENITH_ASSERT_TRUE(xPanel.Action_SelectKey(xTrack, uKey1, ZENITH_ANIMSELECT_ADD), "shift-click adds");
	ZENITH_ASSERT_EQ(xPanel.GetSelectedKeyCount(), 2u, "without dropping the first");
	ZENITH_ASSERT_TRUE(xPanel.Action_SelectKey(xTrack, uKey1, ZENITH_ANIMSELECT_ADD), "adding twice is idempotent");
	ZENITH_ASSERT_EQ(xPanel.GetSelectedKeyCount(), 2u, "and does not duplicate the entry");

	ZENITH_ASSERT_TRUE(xPanel.Action_SelectKey(xTrack, uKey0, ZENITH_ANIMSELECT_TOGGLE), "ctrl-click toggles");
	ZENITH_ASSERT_EQ(xPanel.GetSelectedKeyCount(), 1u, "one back out");
	ZENITH_ASSERT_FALSE(xPanel.IsKeySelected(xTrack, uKey0), "the toggled one");
	ZENITH_ASSERT_TRUE(xPanel.IsKeySelected(xTrack, uKey1), "and only that one");

	ZENITH_ASSERT_TRUE(xPanel.Action_SelectKey(xTrack, uKey2, ZENITH_ANIMSELECT_REPLACE), "a plain click again");
	ZENITH_ASSERT_EQ(xPanel.GetSelectedKeyCount(), 1u, "replaces everything");
	ZENITH_ASSERT_TRUE(xPanel.IsKeySelected(xTrack, uKey2), "with the clicked key");

	// ★ AN ID THE DOCUMENT NEVER ISSUED IS REFUSED. Stable ids only rescue a
	// selection for ids that exist; an invented one would sit in the list forever
	// and silently shrink every operation's effective selection by one.
	ZENITH_ASSERT_FALSE(xPanel.Action_SelectKey(xTrack, 999999u, ZENITH_ANIMSELECT_ADD),
		"selecting a key that does not resolve is refused");
	ZENITH_ASSERT_EQ(xPanel.GetSelectedKeyCount(), 1u, "and changes nothing");

	ZENITH_ASSERT_TRUE(xPanel.Action_ClearSelection(), "clearing a non-empty selection reports that it did something");
	ZENITH_ASSERT_EQ(xPanel.GetSelectedKeyCount(), 0u, "and empties it");
	ZENITH_ASSERT_FALSE(xPanel.Action_ClearSelection(), "clearing an empty one reports that there was nothing to do");
}

//==============================================================================
// (9) Box select works through the RENDERED rects, and picks exactly what the
// band covers.
//
// It is deliberately driven from GetKeyRect rather than from computed
// coordinates: the band has to agree with what a click would hit, and both go
// through the same off-screen gate. A rubber band that selected keys the gate
// refuses to publish would be the graph editor's virtual-palette defect turned
// sideways — the count would look right and the keys would be somewhere nobody
// is looking.
//==============================================================================
ZENITH_TEST(AnimPanel, BoxSelectThroughRenderedRectsPicksExactlyTheKeysInside)
{
	AnimPanelFixture xFixture("zenith_animpanel_boxselect");
	AnimPanelWriteProbe(xFixture.m_strPath, /*bGenerated*/ false);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the probe clip opens");

	const Zenith_AnimTrackId xTrack = AnimPanelHipPosition();
	const u_int uKey0 = AnimPanelKeyIdAtTime(xPanel.Document(), xTrack, 0.0f);
	const u_int uKey1 = AnimPanelKeyIdAtTime(xPanel.Document(), xTrack, 1.0f);
	const u_int uKey2 = AnimPanelKeyIdAtTime(xPanel.Document(), xTrack, 2.0f);

	xPanel.RequestWindowPlacement(40.0f, 40.0f, 900.0f, 600.0f);
	AnimPanelRenderFrames(xPanel, 2u);

	ZENITH_ASSERT_TRUE(xPanel.WasSheetDrawnLastFrame(), "the sheet pass ran");
	ZENITH_ASSERT_GT(xPanel.GetLastTrackWidth(), 0.0f, "with a key lane to draw into");

	Zenith_AnimPanelRect xRect0;
	Zenith_AnimPanelRect xRect1;
	Zenith_AnimPanelRect xRect2;
	Zenith_AnimPanelRect xRow;
	ZENITH_ASSERT_TRUE(xPanel.GetKeyRect(xTrack, uKey0, xRect0), "the key at t=0 is on screen");
	ZENITH_ASSERT_TRUE(xPanel.GetKeyRect(xTrack, uKey1, xRect1), "the key at t=1 is on screen");
	ZENITH_ASSERT_TRUE(xPanel.GetKeyRect(xTrack, uKey2, xRect2), "the key at t=2 is on screen");
	ZENITH_ASSERT_TRUE(xPanel.GetRowTrackRect(xTrack, xRow), "and so is their row's key lane");
	ZENITH_ASSERT_GT(xRect2.m_fMinX, xRect1.m_fMaxX + 1.0f,
		"the third key is clear of the band below (else this test proves nothing)");

	// A band across the first two keys only.
	ZENITH_ASSERT_TRUE(xPanel.Action_BoxSelect(xRect0.m_fMinX - 1.0f, xRow.m_fMinY,
		xRect1.m_fMaxX + 1.0f, xRow.m_fMaxY), "the band caught something");
	ZENITH_ASSERT_EQ(xPanel.GetSelectedKeyCount(), 2u, "exactly the two keys inside it");
	ZENITH_ASSERT_TRUE(xPanel.IsKeySelected(xTrack, uKey0), "the first");
	ZENITH_ASSERT_TRUE(xPanel.IsKeySelected(xTrack, uKey1), "the second");
	ZENITH_ASSERT_FALSE(xPanel.IsKeySelected(xTrack, uKey2), "and NOT the one outside it");

	// Dragged the other way round — a band is a rectangle, not an ordered pair.
	ZENITH_ASSERT_TRUE(xPanel.Action_BoxSelect(xRect2.m_fMaxX + 1.0f, xRow.m_fMaxY,
		xRect2.m_fMinX - 1.0f, xRow.m_fMinY), "an up-left band is normalised, not empty");
	ZENITH_ASSERT_EQ(xPanel.GetSelectedKeyCount(), 1u, "and REPLACE dropped the previous two");
	ZENITH_ASSERT_TRUE(xPanel.IsKeySelected(xTrack, uKey2), "leaving the one it covered");

	// A band over nothing, in ADD mode, leaves the selection alone.
	ZENITH_ASSERT_FALSE(xPanel.Action_BoxSelect(xRow.m_fMinX, xRow.m_fMinY - 400.0f,
		xRow.m_fMaxX, xRow.m_fMinY - 380.0f, ZENITH_ANIMSELECT_ADD), "a band over nothing catches nothing");
	ZENITH_ASSERT_EQ(xPanel.GetSelectedKeyCount(), 1u, "and does not disturb what was picked");
}

//==============================================================================
// (10) A multi-key move snaps ONCE, lands both keys, and is ONE undo step.
//
// The stack-size assertion is the load-bearing one. Without the compound this
// would push a command per key, and the only symptom would be a Ctrl+Z that put
// half a drag back — a state the user was only passing through.
//==============================================================================
ZENITH_TEST(AnimPanel, MoveSelectionSnapsOnceAndUndoesAsExactlyOneStep)
{
	AnimPanelFixture xFixture("zenith_animpanel_move");
	AnimPanelWriteProbe(xFixture.m_strPath, /*bGenerated*/ false);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the probe clip opens");
	ZENITH_ASSERT_EQ(xPanel.GetFrameRate(), 30u, "the probe is authored at 30 fps, so a frame is 1/30 s");

	const Zenith_AnimTrackId xTrack = AnimPanelHipPosition();
	const u_int uKey0 = AnimPanelKeyIdAtTime(xPanel.Document(), xTrack, 0.0f);
	const u_int uKey1 = AnimPanelKeyIdAtTime(xPanel.Document(), xTrack, 1.0f);

	ZENITH_ASSERT_TRUE(xPanel.Action_SelectKey(xTrack, uKey0, ZENITH_ANIMSELECT_REPLACE), "pick the first key");
	ZENITH_ASSERT_TRUE(xPanel.Action_SelectKey(xTrack, uKey1, ZENITH_ANIMSELECT_ADD), "and add the second");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 0u, "selecting is not an edit");

	// 0.02 s at 30 fps is 0.6 of a frame, so the snap rounds the PRIMARY (the most
	// recently selected key, t = 1) up to frame 31 and both keys move by that same
	// 1/30 - the spacing between them is untouched.
	ZENITH_ASSERT_TRUE(xPanel.Action_MoveSelection(0.02f, /*bSnap*/ true), "the move lands");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 1u,
		"TWO keys moved in ONE undo step — the whole reason the compound exists");

	const float fOneFrame = Zenith_AnimTimelineFrameToTime(1u, 30u);
	float fTime = 0.0f;
	ZENITH_ASSERT_TRUE(xPanel.Document().GetKeyTime(xTrack, uKey1, fTime), "the primary resolves");
	ZENITH_ASSERT_EQ_FLOAT(fTime, 1.0f + fOneFrame, 1.0e-4f, "and sits on the frame grid");
	ZENITH_ASSERT_TRUE(xPanel.Document().GetKeyTime(xTrack, uKey0, fTime), "the other resolves");
	ZENITH_ASSERT_EQ_FLOAT(fTime, fOneFrame, 1.0e-4f, "and moved by the SAME delta, not to its own nearest frame");

	ZENITH_ASSERT_TRUE(xPanel.IsKeySelected(xTrack, uKey0), "the selection survived the edit");
	ZENITH_ASSERT_TRUE(xPanel.IsKeySelected(xTrack, uKey1), "the selection survived the edit");

	// ---- one press puts both back -------------------------------------------
	ZENITH_ASSERT_TRUE(xPanel.Action_Undo(), "one undo");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 0u, "drains the stack");

	ZENITH_ASSERT_TRUE(xPanel.Document().GetKeyTime(xTrack, uKey0, fTime), "the first key still resolves by id");
	ZENITH_ASSERT_EQ_FLOAT(fTime, 0.0f, 0.0f, "back EXACTLY where it was");
	ZENITH_ASSERT_TRUE(xPanel.Document().GetKeyTime(xTrack, uKey1, fTime), "so does the second");
	ZENITH_ASSERT_EQ_FLOAT(fTime, 1.0f, 0.0f, "back EXACTLY where it was");
	ZENITH_ASSERT_TRUE(xPanel.IsKeySelected(xTrack, uKey0), "and both are STILL selected afterwards");
	ZENITH_ASSERT_TRUE(xPanel.IsKeySelected(xTrack, uKey1), "and both are STILL selected afterwards");

	ZENITH_ASSERT_TRUE(xPanel.Action_Redo(), "and the redo re-applies the whole group");
	ZENITH_ASSERT_TRUE(xPanel.Document().GetKeyTime(xTrack, uKey1, fTime), "the primary resolves again");
	ZENITH_ASSERT_EQ_FLOAT(fTime, 1.0f + fOneFrame, 1.0e-4f, "at the moved time");
}

//==============================================================================
// (11) D11 — a drop onto an OCCUPIED time fails VISIBLY and destroys nothing.
//
// Three things have to be true at once and each one is a different failure if it
// is not: the action refuses, the refusal is visible on the sheet (a return
// value is not a UI), and nothing whatsoever moved — including the undo stack,
// because an entry that reverses nothing is worse than no entry.
//==============================================================================
ZENITH_TEST(AnimPanel, MovingAKeyOntoAnOccupiedTimeFailsVisiblyAndDestroysNothing)
{
	AnimPanelFixture xFixture("zenith_animpanel_collision");
	AnimPanelWriteProbe(xFixture.m_strPath, /*bGenerated*/ false);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the probe clip opens");

	const Zenith_AnimTrackId xTrack = AnimPanelHipPosition();
	const u_int uKey0 = AnimPanelKeyIdAtTime(xPanel.Document(), xTrack, 0.0f);
	const u_int uKey1 = AnimPanelKeyIdAtTime(xPanel.Document(), xTrack, 1.0f);

	Zenith_Vector<float> afBefore;
	AnimPanelSnapshotTimes(xPanel.Document(), xTrack, afBefore);
	ZENITH_ASSERT_EQ(afBefore.GetSize(), 3u, "three key times recorded before the attempt");

	ZENITH_ASSERT_TRUE(xPanel.Action_SelectKey(xTrack, uKey0, ZENITH_ANIMSELECT_REPLACE), "pick the key at t=0");
	ZENITH_ASSERT_EQ(xPanel.GetCollisionFlashFramesRemaining(), 0u, "nothing is flashing yet");

	// Straight onto the key at t = 1.
	ZENITH_ASSERT_FALSE(xPanel.Action_MoveSelection(1.0f, /*bSnap*/ false), "the drop is REFUSED");

	ZENITH_ASSERT_GT(xPanel.GetCollisionFlashFramesRemaining(), 0u,
		"and it is VISIBLE — a bare false is not something a user can see");
	Zenith_AnimTrackId xFlashTrack;
	u_int uFlashKeyId = uINVALID_ANIM_KEY_ID;
	ZENITH_ASSERT_TRUE(xPanel.GetCollisionFlashKey(xFlashTrack, uFlashKeyId), "the flash names a key");
	ZENITH_ASSERT_EQ(uFlashKeyId, uKey1, "the one that was in the way, not the one being dragged");
	ZENITH_ASSERT_TRUE(xFlashTrack == xTrack, "on its own track");

	// ---- and NOTHING moved ---------------------------------------------------
	Zenith_Vector<float> afAfter;
	AnimPanelSnapshotTimes(xPanel.Document(), xTrack, afAfter);
	ZENITH_ASSERT_EQ(afAfter.GetSize(), afBefore.GetSize(), "the key count is unchanged");
	for (u_int u = 0; u < afBefore.GetSize(); ++u)
	{
		ZENITH_ASSERT_EQ_FLOAT(afAfter.Get(u), afBefore.Get(u), 0.0f, "every key time is bit-for-bit unchanged");
	}
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 0u,
		"and the undo stack did NOT grow — a refusal is not an edit");
	ZENITH_ASSERT_TRUE(xPanel.IsKeySelected(xTrack, uKey0), "the selection is left exactly as it was");

	// The flash is a FRAME counter, so a rendered frame burns one.
	const u_int uLit = xPanel.GetCollisionFlashFramesRemaining();
	xPanel.RequestWindowPlacement(40.0f, 40.0f, 900.0f, 600.0f);
	AnimPanelRenderFrames(xPanel, 1u);
	ZENITH_ASSERT_EQ(xPanel.GetCollisionFlashFramesRemaining(), uLit - 1u, "one frame, one tick of the flash");
}

//==============================================================================
// (12) Delete is one step, and the UNDO gives the selection back with the keys.
//
// ★ THE SELECTION IS NOT PRUNED BY THE DELETE, deliberately. If it were, the
// undo would restore the keys and the user would be left with nothing picked —
// and the stable ids the document goes to such lengths to preserve would have
// bought precisely nothing.
//==============================================================================
ZENITH_TEST(AnimPanel, DeleteSelectionIsOneStepAndItsUndoRestoresTheSelection)
{
	AnimPanelFixture xFixture("zenith_animpanel_delete");
	AnimPanelWriteProbe(xFixture.m_strPath, /*bGenerated*/ false);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the probe clip opens");

	const Zenith_AnimTrackId xTrack = AnimPanelHipPosition();
	const u_int uKey1 = AnimPanelKeyIdAtTime(xPanel.Document(), xTrack, 1.0f);
	const u_int uKey2 = AnimPanelKeyIdAtTime(xPanel.Document(), xTrack, 2.0f);

	ZENITH_ASSERT_FALSE(xPanel.Action_DeleteSelection(), "deleting nothing is refused rather than logged as an edit");

	ZENITH_ASSERT_TRUE(xPanel.Action_SelectKey(xTrack, uKey1, ZENITH_ANIMSELECT_REPLACE), "pick two keys");
	ZENITH_ASSERT_TRUE(xPanel.Action_SelectKey(xTrack, uKey2, ZENITH_ANIMSELECT_ADD), "pick two keys");

	ZENITH_ASSERT_TRUE(xPanel.Action_DeleteSelection(), "the delete lands");
	ZENITH_ASSERT_EQ(xPanel.Document().GetKeyCount(xTrack), 1u, "leaving one key on the track");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 1u, "two removals, ONE undo step");
	ZENITH_ASSERT_EQ(xPanel.Document().GetKeyIndexForId(xTrack, uKey1), uINVALID_ANIM_KEY_INDEX,
		"and the deleted ids stop resolving");

	// The ids are STILL in the selection, naming keys that do not currently exist.
	ZENITH_ASSERT_EQ(xPanel.GetSelectedKeyCount(), 2u, "the selection still names them");

	ZENITH_ASSERT_TRUE(xPanel.Action_Undo(), "one undo brings both back");
	ZENITH_ASSERT_EQ(xPanel.Document().GetKeyCount(xTrack), 3u, "all three keys are on the track again");

	float fTime = 0.0f;
	ZENITH_ASSERT_TRUE(xPanel.Document().GetKeyTime(xTrack, uKey1, fTime), "the first resolves under its ORIGINAL id");
	ZENITH_ASSERT_EQ_FLOAT(fTime, 1.0f, 0.0f, "at exactly its original time");
	ZENITH_ASSERT_TRUE(xPanel.Document().GetKeyTime(xTrack, uKey2, fTime), "so does the second");
	ZENITH_ASSERT_EQ_FLOAT(fTime, 2.0f, 0.0f, "at exactly its original time");

	ZENITH_ASSERT_TRUE(xPanel.IsKeySelected(xTrack, uKey1), "and the selection came back WITH them");
	ZENITH_ASSERT_TRUE(xPanel.IsKeySelected(xTrack, uKey2), "and the selection came back WITH them");
}

//==============================================================================
// (13) Duplicate produces NEW ids carrying the SAME values, and selects them.
//==============================================================================
ZENITH_TEST(AnimPanel, DuplicateSelectionProducesNewIdsCarryingTheSameValues)
{
	AnimPanelFixture xFixture("zenith_animpanel_duplicate");
	AnimPanelWriteProbe(xFixture.m_strPath, /*bGenerated*/ false);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the probe clip opens");

	const Zenith_AnimTrackId xTrack = AnimPanelHipPosition();
	const u_int uKey1 = AnimPanelKeyIdAtTime(xPanel.Document(), xTrack, 1.0f);
	ZENITH_ASSERT_TRUE(xPanel.Action_SelectKey(xTrack, uKey1, ZENITH_ANIMSELECT_REPLACE), "pick the key at t=1");

	ZENITH_ASSERT_TRUE(xPanel.Action_DuplicateSelection(), "the duplicate lands");
	ZENITH_ASSERT_EQ(xPanel.Document().GetKeyCount(xTrack), 4u, "a fourth key exists");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 1u, "as one undo step");

	// ★ THE COPY IS SELECTED, NOT THE ORIGINAL. The gesture after a duplicate is
	// almost always "now drag it", and with the two sitting one frame apart,
	// dragging the wrong one is the version of that mistake nobody notices.
	ZENITH_ASSERT_EQ(xPanel.GetSelectedKeyCount(), 1u, "exactly the copy is selected");
	ZENITH_ASSERT_FALSE(xPanel.IsKeySelected(xTrack, uKey1), "and the original is not");

	Zenith_AnimTrackId xCopyTrack;
	u_int uCopyId = uINVALID_ANIM_KEY_ID;
	ZENITH_ASSERT_TRUE(xPanel.GetSelectedKeyAt(0u, xCopyTrack, uCopyId), "the copy is reachable through the selection");
	ZENITH_ASSERT_NE(uCopyId, uKey1, "and carries a FRESH id — a duplicate is a new key, not an alias");
	ZENITH_ASSERT_TRUE(xCopyTrack == xTrack, "on the same track");

	const float fOneFrame = Zenith_AnimTimelineFrameToTime(1u, 30u);
	float fTime = 0.0f;
	ZENITH_ASSERT_TRUE(xPanel.Document().GetKeyTime(xTrack, uCopyId, fTime), "the copy resolves");
	ZENITH_ASSERT_EQ_FLOAT(fTime, 1.0f + fOneFrame, 1.0e-4f, "one frame past the key it was copied from");

	Zenith_AnimKeyValue xOriginal;
	Zenith_AnimKeyValue xCopy;
	ZENITH_ASSERT_TRUE(xPanel.Document().GetKeyValue(xTrack, uKey1, xOriginal), "the original's value reads back");
	ZENITH_ASSERT_TRUE(xPanel.Document().GetKeyValue(xTrack, uCopyId, xCopy), "and so does the copy's");
	ZENITH_ASSERT_EQ_FLOAT(xCopy.m_xVector.y, xOriginal.m_xVector.y, 0.0f, "with the value carried across exactly");

	ZENITH_ASSERT_TRUE(xPanel.Action_Undo(), "and the undo removes it again");
	ZENITH_ASSERT_EQ(xPanel.Document().GetKeyCount(xTrack), 3u, "back to three keys");
}

//==============================================================================
// (14) Cross-bone paste: kinds are matched, and a bone with no channel gets one.
//
// The second half is the one worth having. A channel is DELETED when its last
// key goes (D14) and a bone the clip never animated has none at all, so "paste
// onto that bone" has to go through the document's insert verb — which creates
// it — rather than looking for a track that is not there and quietly doing
// nothing.
//==============================================================================
ZENITH_TEST(AnimPanel, CrossBonePasteMatchesKindsAndCreatesAMissingChannel)
{
	AnimPanelFixture xFixture("zenith_animpanel_paste");
	AnimPanelWriteTwoBoneProbe(xFixture.m_strPath);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the two-bone probe opens");

	const Zenith_AnimTrackId xHip = AnimPanelHipPosition();
	const Zenith_AnimTrackId xSpine = AnimPanelSpinePosition();
	ZENITH_ASSERT_EQ(xPanel.Document().GetKeyCount(xSpine), 1u, "Spine starts with one position key");

	const u_int uHip0 = AnimPanelKeyIdAtTime(xPanel.Document(), xHip, 0.0f);
	const u_int uHip1 = AnimPanelKeyIdAtTime(xPanel.Document(), xHip, 1.0f);

	ZENITH_ASSERT_FALSE(xPanel.Action_CopySelection(), "copying an empty selection is refused");
	ZENITH_ASSERT_TRUE(xPanel.Action_SelectKey(xHip, uHip0, ZENITH_ANIMSELECT_REPLACE), "pick two of Hip's keys");
	ZENITH_ASSERT_TRUE(xPanel.Action_SelectKey(xHip, uHip1, ZENITH_ANIMSELECT_ADD), "pick two of Hip's keys");
	ZENITH_ASSERT_TRUE(xPanel.Action_CopySelection(), "the copy lands");
	ZENITH_ASSERT_EQ(xPanel.GetClipboardKeyCount(), 2u, "two keys on the clipboard");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 0u, "and a copy is not an edit");

	// ---- onto a bone that HAS the channel -----------------------------------
	ZENITH_ASSERT_TRUE(xPanel.Action_PasteToBone("Spine", 0.5f), "the paste lands");
	ZENITH_ASSERT_EQ(xPanel.Document().GetKeyCount(xSpine), 3u, "Spine gained both keys");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 1u, "as ONE undo step");
	ZENITH_ASSERT_EQ(xPanel.Document().GetKeyCount(xHip), 3u, "and the source is untouched");

	// The relative spacing survived the trip: 0 and 1 became 0.5 and 1.5.
	const u_int uPasted0 = AnimPanelKeyIdAtTime(xPanel.Document(), xSpine, 0.5f);
	const u_int uPasted1 = AnimPanelKeyIdAtTime(xPanel.Document(), xSpine, 1.5f);
	ZENITH_ASSERT_NE(uPasted0, uINVALID_ANIM_KEY_ID, "the first copy landed at the offset");
	ZENITH_ASSERT_NE(uPasted1, uINVALID_ANIM_KEY_ID, "and the second kept its spacing from it");
	ZENITH_ASSERT_NE(uPasted0, uHip0, "the pasted keys carry FRESH ids");
	ZENITH_ASSERT_NE(uPasted1, uHip1, "the pasted keys carry FRESH ids");

	Zenith_AnimKeyValue xValue;
	ZENITH_ASSERT_TRUE(xPanel.Document().GetKeyValue(xSpine, uPasted1, xValue), "the second copy's value reads back");
	ZENITH_ASSERT_EQ_FLOAT(xValue.m_xVector.y, 1.0f, 0.0f, "carrying Hip's value, not Spine's");

	ZENITH_ASSERT_EQ(xPanel.GetSelectedKeyCount(), 2u, "the pasted keys become the selection");
	ZENITH_ASSERT_TRUE(xPanel.IsKeySelected(xSpine, uPasted0), "on the bone they were pasted onto");

	// ---- onto a bone with NO channel at all ---------------------------------
	const Zenith_AnimTrackId xTail = Zenith_AnimTrackId::Bone("Tail", FLUX_ANIM_TRACK_POSITION);
	ZENITH_ASSERT_FALSE(xPanel.Document().TrackExists(xTail), "the clip has no Tail channel to begin with");

	ZENITH_ASSERT_TRUE(xPanel.Action_PasteToBone("Tail", 0.0f), "pasting onto it still lands");
	ZENITH_ASSERT_TRUE(xPanel.Document().TrackExists(xTail), "and the document's own verb CREATED the channel");
	ZENITH_ASSERT_EQ(xPanel.Document().GetKeyCount(xTail), 2u, "with both keys on it");

	ZENITH_ASSERT_TRUE(xPanel.Action_Undo(), "the undo reverses the whole paste");
	ZENITH_ASSERT_EQ(xPanel.Document().GetKeyCount(xTail), 0u, "leaving nothing on Tail");
}

//==============================================================================
// (15) A ripple retime undone restores EVERY key time exactly, and events do
// not move with it (D4).
//==============================================================================
ZENITH_TEST(AnimPanel, RippleRetimeUndoneRestoresEveryKeyTimeExactly)
{
	AnimPanelFixture xFixture("zenith_animpanel_ripple");
	AnimPanelWriteProbe(xFixture.m_strPath, /*bGenerated*/ false);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the probe clip opens");

	const Zenith_AnimTrackId xTrack = AnimPanelHipPosition();
	const u_int uEventId = xPanel.Document().AddEvent("Beat", 0.25f, Zenith_Maths::Vector4(0.0f, 0.0f, 0.0f, 0.0f));
	ZENITH_ASSERT_NE(uEventId, uINVALID_ANIM_KEY_ID, "an event to watch stay put");

	Zenith_Vector<float> afBefore;
	AnimPanelSnapshotTimes(xPanel.Document(), xTrack, afBefore);
	ZENITH_ASSERT_EQ(afBefore.GetSize(), 3u, "three key times recorded");

	const u_int uStackBefore = xPanel.Document().GetUndoStackSize();

	// Everything at or after t = 1 slides half a second later; the key at t = 0
	// does not move, which is what makes this a ripple rather than a shift.
	ZENITH_ASSERT_TRUE(xPanel.Action_RippleRetime(1.0f, 0.5f), "the ripple lands");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), uStackBefore + 1u, "as ONE undo step");

	float fTime = 0.0f;
	ZENITH_ASSERT_TRUE(xPanel.Document().GetKeyTime(xTrack, xPanel.Document().GetKeyIdAtIndex(xTrack, 0u), fTime),
		"the first key resolves");
	ZENITH_ASSERT_EQ_FLOAT(fTime, 0.0f, 0.0f, "and did NOT move — it is before the ripple point");
	ZENITH_ASSERT_TRUE(xPanel.Document().GetKeyTime(xTrack, xPanel.Document().GetKeyIdAtIndex(xTrack, 2u), fTime),
		"the last key resolves");
	ZENITH_ASSERT_EQ_FLOAT(fTime, 2.5f, 1.0e-5f, "and slid by the delta");

	// ★ D4. An event's time is a [0,1] FRACTION of the clip, not a point on the
	// seconds clock the keys are on, so it is already relative to the duration —
	// sliding it "proportionally" alongside the keys would move it twice.
	Flux_AnimationEvent xEvent;
	ZENITH_ASSERT_TRUE(xPanel.Document().GetEvent(uEventId, xEvent), "the event resolves");
	ZENITH_ASSERT_EQ_FLOAT(xEvent.m_fNormalizedTime, 0.25f, 0.0f, "and a ripple did not touch it");

	// ---- exact restore -------------------------------------------------------
	ZENITH_ASSERT_TRUE(xPanel.Action_Undo(), "one undo reverses the whole ripple");
	Zenith_Vector<float> afAfter;
	AnimPanelSnapshotTimes(xPanel.Document(), xTrack, afAfter);
	ZENITH_ASSERT_EQ(afAfter.GetSize(), afBefore.GetSize(), "with the same number of keys");
	for (u_int u = 0; u < afBefore.GetSize(); ++u)
	{
		ZENITH_ASSERT_EQ_FLOAT(afAfter.Get(u), afBefore.Get(u), 0.0f,
			"and every key time restored BIT-FOR-BIT, not merely close");
	}
}

//==============================================================================
// (16) A ripple that would collide refuses whole, and a ripple with nothing to
// move is not an edit.
//==============================================================================
ZENITH_TEST(AnimPanel, ARippleThatWouldCollideRefusesAndChangesNothing)
{
	AnimPanelFixture xFixture("zenith_animpanel_ripple_collide");
	AnimPanelWriteProbe(xFixture.m_strPath, /*bGenerated*/ false);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the probe clip opens");

	const Zenith_AnimTrackId xTrack = AnimPanelHipPosition();
	Zenith_Vector<float> afBefore;
	AnimPanelSnapshotTimes(xPanel.Document(), xTrack, afBefore);

	// Slide everything at or after t = 1 BACK by one second: the key at t = 1
	// would land on the key at t = 0, which is not part of the move.
	ZENITH_ASSERT_FALSE(xPanel.Action_RippleRetime(1.0f, -1.0f), "the ripple is REFUSED");
	ZENITH_ASSERT_GT(xPanel.GetCollisionFlashFramesRemaining(), 0u, "and says so on the sheet");

	Zenith_Vector<float> afAfter;
	AnimPanelSnapshotTimes(xPanel.Document(), xTrack, afAfter);
	ZENITH_ASSERT_EQ(afAfter.GetSize(), afBefore.GetSize(), "nothing was removed");
	for (u_int u = 0; u < afBefore.GetSize(); ++u)
	{
		ZENITH_ASSERT_EQ_FLOAT(afAfter.Get(u), afBefore.Get(u), 0.0f, "and nothing moved");
	}
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 0u, "the undo stack is untouched");

	// Nothing at or after t = 9 s, so there is no edit to make and no empty undo
	// entry to leave behind.
	ZENITH_ASSERT_FALSE(xPanel.Action_RippleRetime(9.0f, 0.5f), "a ripple past the last key is not an edit");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 0u, "and pushes nothing");
}

//==============================================================================
// (17) The duration handle's action, and undo / redo through the panel.
//==============================================================================
ZENITH_TEST(AnimPanel, SetDurationIsUndoableAndRefusesNonEdits)
{
	AnimPanelFixture xFixture("zenith_animpanel_duration");
	AnimPanelWriteProbe(xFixture.m_strPath, /*bGenerated*/ false);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the probe clip opens");
	ZENITH_ASSERT_EQ_FLOAT(xPanel.Document().GetDuration(), 2.0f, 0.0f, "at its authored duration");

	ZENITH_ASSERT_FALSE(xPanel.Action_Undo(), "there is nothing to undo yet");
	ZENITH_ASSERT_FALSE(xPanel.Action_Redo(), "nor to redo");

	ZENITH_ASSERT_TRUE(xPanel.Action_SetDuration(3.0f), "the handle's drop lands");
	ZENITH_ASSERT_EQ_FLOAT(xPanel.Document().GetDuration(), 3.0f, 0.0f, "moving the duration");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 1u, "as one step");

	// ★ A NO-OP IS REFUSED. A drag that ends where it began would otherwise leave
	// an undo entry whose Ctrl+Z visibly does nothing, which reads as broken.
	ZENITH_ASSERT_FALSE(xPanel.Action_SetDuration(3.0f), "setting it to what it already is is not an edit");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 1u, "and pushes nothing");

	ZENITH_ASSERT_FALSE(xPanel.Action_SetDuration(-1.0f), "a negative duration is refused");
	ZENITH_ASSERT_EQ_FLOAT(xPanel.Document().GetDuration(), 3.0f, 0.0f, "leaving the duration alone");

	ZENITH_ASSERT_TRUE(xPanel.Action_Undo(), "the undo runs");
	ZENITH_ASSERT_EQ_FLOAT(xPanel.Document().GetDuration(), 2.0f, 0.0f, "restoring the authored duration exactly");
	ZENITH_ASSERT_TRUE(xPanel.Action_Redo(), "and the redo runs");
	ZENITH_ASSERT_EQ_FLOAT(xPanel.Document().GetDuration(), 3.0f, 0.0f, "re-applying it exactly");
}

//==============================================================================
// (18) Scrubbing moves the session's clock, clamps to the clip, and emits no
// events.
//
// This is the one operation that needs a REAL rig, because Seek is inert until
// one resolves — so the fixture writes a two-bone skeleton and a one-triangle
// mesh. Both are device-free: the mesh exists only to be a resolvable preview
// PATH, which is why it is hand-built rather than GenerateUnitCube (that helper
// ends in EnsureGPUBuffers). Nothing here renders a frame.
//==============================================================================
ZENITH_TEST(AnimPanel, ScrubMovesTheSessionClockWithoutEmittingEvents)
{
	AnimPanelFixture xFixture("zenith_animpanel_scrub");
	AnimPanelWriteRiggedProbe(xFixture);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the rigged probe opens");
	ZENITH_ASSERT_TRUE(xPanel.Session().IsOpen(), "with a preview session");
	ZENITH_ASSERT_FALSE(xPanel.Session().NeedsRigSelection(), "whose rig resolved (else the scrub is inert)");
	ZENITH_ASSERT_EQ_FLOAT(xPanel.Session().GetTime(), 0.0f, 1.0e-5f, "a freshly opened clip sits at t=0");

	ZENITH_ASSERT_TRUE(xPanel.Action_Scrub(0.75f), "the scrub lands");
	ZENITH_ASSERT_EQ_FLOAT(xPanel.Session().GetTime(), 0.75f, 1.0e-3f, "and the session's clock moved");

	// ★ A SCRUB EMITS NOTHING. Flux_AnimationController advances the event
	// bookkeeping mark on a seek WITHOUT firing what the playhead skipped (D40) —
	// dragging across a clip must not replay every footstep in it. The flag below
	// is the hook that would change that, and nothing sets it.
	ZENITH_ASSERT_FALSE(xPanel.Session().Controller().GetEmitEventsOnSeek(),
		"the seek path is the non-emitting one");

	// Clamped into the clip rather than refused: a drag runs off the end of the
	// ruler constantly, and refusing there would make the playhead stick.
	ZENITH_ASSERT_TRUE(xPanel.Action_Scrub(99.0f), "a scrub past the end still lands");
	ZENITH_ASSERT_EQ_FLOAT(xPanel.Session().GetTime(), 2.0f, 1.0e-3f, "clamped to the clip's duration");
	ZENITH_ASSERT_TRUE(xPanel.Action_Scrub(-5.0f), "and so does one before the start");
	ZENITH_ASSERT_EQ_FLOAT(xPanel.Session().GetTime(), 0.0f, 1.0e-3f, "clamped to zero");

	// The document is a scrub away from nothing: seeking is a VIEW change.
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 0u, "and a scrub is not an edit");
	ZENITH_ASSERT_FALSE(xPanel.Document().IsDirty(), "nor does it dirty the clip");
}

//==============================================================================
//                            WU-5B — THE EVENTS ROW
//
// ★ EVERY TIME BELOW IS A [0,1] FRACTION (D4), AND THAT IS THE ONE THING THESE
// UNITS EXIST TO PIN. An event is not on the seconds clock the keys are on, so
// the sheet maps it through fNormalized * duration at DRAW time and the stored
// value never moves. The failure that shape invites is silent in both
// directions: rescale the value on a duration change and every event drifts a
// second time; forget the multiply and the flag lands at the wrong pixel while
// the file is perfectly correct. (24) renders frames either side of a duration
// change and checks BOTH halves at once.
//
// ★ EVENTS MAY COINCIDE. Keys may not (D11 — a second key on one time would
// overwrite the first's value), and copying that refusal across would make a
// double footstep unauthorable. (21) is that difference, stated.
//==============================================================================

//==============================================================================
// (19) Add: one undo step, the new event becomes the selection, and the
// refusals that keep a gesture from producing something invisible.
//==============================================================================
ZENITH_TEST(AnimPanel, AddEventSelectsTheNewEventAndUndoesAsOneStep)
{
	AnimPanelFixture xFixture("zenith_animpanel_addevent");
	AnimPanelWriteProbe(xFixture.m_strPath, /*bGenerated*/ false);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the probe clip opens");
	ZENITH_ASSERT_EQ(xPanel.Document().GetEventCount(), 0u, "the probe authored no events");

	// ---- the refusals --------------------------------------------------------
	ZENITH_ASSERT_FALSE(xPanel.Action_AddEvent(-0.1f, "Early"), "a negative normalized time is refused");
	// ★ AN EMPTY NAME IS REFUSED. The flag would draw unlabelled and the runtime
	// dispatcher would hand every listener an empty string, so the event exists
	// and matches nothing — a silent no-op carrying an undo entry.
	ZENITH_ASSERT_FALSE(xPanel.Action_AddEvent(0.25f, ""), "an unnamed event is refused");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 0u, "and neither refusal pushed anything");

	// ---- the real one --------------------------------------------------------
	ZENITH_ASSERT_TRUE(xPanel.Action_AddEvent(0.25f, Zenith_EditorPanel_Animation::DefaultEventName()),
		"the add lands");
	ZENITH_ASSERT_EQ(xPanel.Document().GetEventCount(), 1u, "one event on the clip");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 1u, "as ONE undo step");

	// ★ THE NEW EVENT IS THE SELECTION, which is what points the inspector strip
	// at it: the gesture straight after "add" is "name it", and leaving the
	// previous selection in place would put the user's typing into another event.
	ZENITH_ASSERT_EQ(xPanel.GetSelectedEventCount(), 1u, "and it is selected");
	const u_int uEventId = xPanel.GetSelectedEventIdAt(0u);
	ZENITH_ASSERT_NE(uEventId, uINVALID_ANIM_KEY_ID, "with an id the selection can name");
	ZENITH_ASSERT_EQ(xPanel.GetInspectorEventId(), uEventId, "and the inspector strip targets exactly it");

	Flux_AnimationEvent xEvent;
	ZENITH_ASSERT_TRUE(xPanel.Document().GetEvent(uEventId, xEvent), "the event resolves by id");
	ZENITH_ASSERT_EQ_FLOAT(xEvent.m_fNormalizedTime, 0.25f, 0.0f, "at the NORMALIZED time asked for, not a second");
	ZENITH_ASSERT_STREQ(xEvent.m_strEventName.c_str(), Zenith_EditorPanel_Animation::DefaultEventName(),
		"under the default name a gesture gives it");

	// ---- undo / redo ---------------------------------------------------------
	ZENITH_ASSERT_TRUE(xPanel.Action_Undo(), "one undo removes it");
	ZENITH_ASSERT_EQ(xPanel.Document().GetEventCount(), 0u, "leaving no events");
	ZENITH_ASSERT_FALSE(xPanel.Document().GetEvent(uEventId, xEvent), "and the id stops resolving");
	ZENITH_ASSERT_EQ(xPanel.GetSelectedEventCount(), 1u,
		"while the SELECTION still names it — the same rule the key delete follows, so the redo hands it back");

	ZENITH_ASSERT_TRUE(xPanel.Action_Redo(), "the redo puts it back");
	ZENITH_ASSERT_TRUE(xPanel.Document().GetEvent(uEventId, xEvent), "under its ORIGINAL id");
	ZENITH_ASSERT_TRUE(xPanel.IsEventSelected(uEventId), "so the selection resolves again");
	ZENITH_ASSERT_EQ(xPanel.GetInspectorEventId(), uEventId, "and the inspector is pointing at it once more");

	// ---- past the end is ALLOWED, and counted (D13) --------------------------
	// Refusing it would silently discard what the author asked for; the panel says
	// so instead, exactly as it does for a key past the duration.
	ZENITH_ASSERT_TRUE(xPanel.Action_AddEvent(1.5f, "Late"), "an event past the clip's end is allowed");
	xPanel.RequestWindowPlacement(40.0f, 40.0f, 900.0f, 600.0f);
	AnimPanelRenderFrames(xPanel, 1u);
	ZENITH_ASSERT_EQ(xPanel.GetEventsPastDurationCount(), 1u,
		"and is COUNTED as past the end — >1.0, not >duration: an event is a fraction, not a second");
}

//==============================================================================
// (20) A move snaps to the frame grid AT THE CURRENT DURATION, and its undo
// restores the stored fraction bit-for-bit.
//
// ★ THE SNAP HAS TO HAPPEN IN SECONDS. The frame grid is the clip's authored
// frame rate; there is no "nearest frame" in [0,1] without a duration to divide
// by. So the action converts out, snaps, and converts back — and this is the
// assertion that catches anyone snapping the fraction directly, which would
// quantise to 1/30 of the WHOLE CLIP and be wrong by a factor of the duration.
//==============================================================================
ZENITH_TEST(AnimPanel, MovingAnEventSnapsToTheFrameGridAndUndoesExactly)
{
	AnimPanelFixture xFixture("zenith_animpanel_moveevent");
	AnimPanelWriteProbe(xFixture.m_strPath, /*bGenerated*/ false);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the probe clip opens");
	ZENITH_ASSERT_EQ(xPanel.GetFrameRate(), 30u, "authored at 30 fps");
	ZENITH_ASSERT_EQ_FLOAT(xPanel.Document().GetDuration(), 2.0f, 0.0f, "over 2 s");

	ZENITH_ASSERT_TRUE(xPanel.Action_AddEvent(0.25f, "Beat"), "an event at a quarter of the clip");
	const u_int uEventId = xPanel.GetSelectedEventIdAt(0u);

	ZENITH_ASSERT_FALSE(xPanel.Action_MoveSelectedEvents(0.0f, /*bSnap*/ false),
		"a zero move is refused rather than pushing an undo entry that reverses nothing");

	// 0.25 normalized is 0.5 s. +0.02 normalized is +0.04 s, landing on 0.54 s —
	// 16.2 frames — which snaps DOWN to frame 16 and comes back as 16/30 s
	// expressed as a fraction of 2 s.
	ZENITH_ASSERT_TRUE(xPanel.Action_MoveSelectedEvents(0.02f, /*bSnap*/ true), "the move lands");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 2u, "the add and the move: one step each");

	Flux_AnimationEvent xEvent;
	ZENITH_ASSERT_TRUE(xPanel.Document().GetEvent(uEventId, xEvent), "the event still resolves by id");
	const float fFrame16 = Zenith_AnimTimelineFrameToTime(16u, 30u);
	ZENITH_ASSERT_EQ_FLOAT(xEvent.m_fNormalizedTime * 2.0f, fFrame16, 1.0e-5f,
		"its time IN SECONDS sits on the frame grid — the snap went through the duration, not around it");
	ZENITH_ASSERT_TRUE(xPanel.IsEventSelected(uEventId), "and the selection survived the edit");

	// ---- exact restore -------------------------------------------------------
	ZENITH_ASSERT_TRUE(xPanel.Action_Undo(), "one undo reverses the whole move");
	ZENITH_ASSERT_TRUE(xPanel.Document().GetEvent(uEventId, xEvent), "the event resolves under its original id");
	ZENITH_ASSERT_EQ_FLOAT(xEvent.m_fNormalizedTime, 0.25f, 0.0f, "back EXACTLY where it was, not merely close");
	ZENITH_ASSERT_TRUE(xPanel.IsEventSelected(uEventId), "with the selection intact");

	// A drag that would take an event before the start of the clip is refused
	// whole rather than clamped: clamping a multi-event drag piles the leaders
	// onto zero and silently rewrites their spacing.
	ZENITH_ASSERT_FALSE(xPanel.Action_MoveSelectedEvents(-1.0f, /*bSnap*/ false),
		"a move below zero is refused");
	ZENITH_ASSERT_TRUE(xPanel.Document().GetEvent(uEventId, xEvent), "and the event is untouched");
	ZENITH_ASSERT_EQ_FLOAT(xEvent.m_fNormalizedTime, 0.25f, 0.0f, "at its original fraction");
}

//==============================================================================
// (21) TWO EVENTS MAY SHARE ONE NORMALIZED TIME, and a drag onto an occupied
// one is allowed.
//
// This is the deliberate difference from keys. D11 refuses a key drop onto an
// occupied time because Flux_BoneChannel would REPLACE the value in place (D25)
// and the operation would report success having produced fewer keys than it was
// asked for. An event list appends and sorts; two footsteps on one frame are two
// events and Flux_AnimationController::EmitSpanEvents fires both. Copying the
// key refusal across would make that unauthorable, so no event action carries a
// collision pre-check — and nothing here may flash.
//==============================================================================
ZENITH_TEST(AnimPanel, TwoEventsMayShareOneNormalizedTime)
{
	AnimPanelFixture xFixture("zenith_animpanel_coincident");
	AnimPanelWriteProbe(xFixture.m_strPath, /*bGenerated*/ false);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the probe clip opens");

	ZENITH_ASSERT_TRUE(xPanel.Action_AddEvent(0.25f, "Left"), "one event at 0.25");
	const u_int uLeftId = xPanel.GetSelectedEventIdAt(0u);
	ZENITH_ASSERT_TRUE(xPanel.Action_AddEvent(0.5f, "Right"), "another at 0.5");
	const u_int uRightId = xPanel.GetSelectedEventIdAt(0u);
	ZENITH_ASSERT_NE(uLeftId, uRightId, "two distinct ids");

	// Drag the first straight onto the second. Unsnapped, so the target is exactly
	// 0.5 and there is no rounding to hide behind.
	ZENITH_ASSERT_TRUE(xPanel.Action_SelectEvent(uLeftId, ZENITH_ANIMSELECT_REPLACE), "pick the first");
	ZENITH_ASSERT_TRUE(xPanel.Action_MoveSelectedEvents(0.25f, /*bSnap*/ false),
		"and dropping it on top of the other is ALLOWED — events are not keys");

	ZENITH_ASSERT_EQ(xPanel.Document().GetEventCount(), 2u, "both events still exist");
	ZENITH_ASSERT_EQ(xPanel.GetCollisionFlashFramesRemaining(), 0u,
		"and nothing flashed — there was no refusal to make visible");

	Flux_AnimationEvent xLeft;
	Flux_AnimationEvent xRight;
	ZENITH_ASSERT_TRUE(xPanel.Document().GetEvent(uLeftId, xLeft), "the moved one still resolves under its id");
	ZENITH_ASSERT_TRUE(xPanel.Document().GetEvent(uRightId, xRight), "and so does the one it landed on");
	ZENITH_ASSERT_EQ_FLOAT(xLeft.m_fNormalizedTime, 0.5f, 1.0e-6f, "both sit at the same normalized time");
	ZENITH_ASSERT_EQ_FLOAT(xRight.m_fNormalizedTime, 0.5f, 1.0e-6f, "both sit at the same normalized time");
	ZENITH_ASSERT_STREQ(xLeft.m_strEventName.c_str(), "Left", "and neither absorbed the other");
	ZENITH_ASSERT_STREQ(xRight.m_strEventName.c_str(), "Right", "and neither absorbed the other");
}

//==============================================================================
// (22) Delete covers EVENTS as well as keys, in ONE step, and the undo hands
// the selection back with them.
//==============================================================================
ZENITH_TEST(AnimPanel, DeleteSelectionRemovesEventsAndItsUndoRestoresTheSelection)
{
	AnimPanelFixture xFixture("zenith_animpanel_deleteevent");
	AnimPanelWriteProbe(xFixture.m_strPath, /*bGenerated*/ false);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the probe clip opens");

	ZENITH_ASSERT_TRUE(xPanel.Action_AddEvent(0.25f, "Left"), "two events");
	const u_int uLeftId = xPanel.GetSelectedEventIdAt(0u);
	ZENITH_ASSERT_TRUE(xPanel.Action_AddEvent(0.75f, "Right"), "two events");
	const u_int uRightId = xPanel.GetSelectedEventIdAt(0u);
	ZENITH_ASSERT_TRUE(xPanel.Action_SelectEvent(uLeftId, ZENITH_ANIMSELECT_ADD), "both picked");
	ZENITH_ASSERT_EQ(xPanel.GetSelectedEventCount(), 2u, "both picked");

	// A key on the same sheet, selected alongside, so this also pins that ONE
	// Delete covers both lists rather than whichever one it looks at first.
	const Zenith_AnimTrackId xTrack = AnimPanelHipPosition();
	const u_int uKey2 = AnimPanelKeyIdAtTime(xPanel.Document(), xTrack, 2.0f);
	ZENITH_ASSERT_TRUE(xPanel.Action_SelectKey(xTrack, uKey2, ZENITH_ANIMSELECT_ADD), "and a keyframe too");

	const u_int uStackBefore = xPanel.Document().GetUndoStackSize();
	ZENITH_ASSERT_TRUE(xPanel.Action_DeleteSelection(), "the delete lands");
	ZENITH_ASSERT_EQ(xPanel.Document().GetEventCount(), 0u, "both events are gone");
	ZENITH_ASSERT_EQ(xPanel.Document().GetKeyCount(xTrack), 2u, "and so is the key");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), uStackBefore + 1u,
		"two events and a key removed in ONE undo step");

	// ★ THE SELECTION IS NOT PRUNED, exactly as for keys: the undo restores each
	// event under its ORIGINAL id, so the ids still listed here resolve again.
	ZENITH_ASSERT_EQ(xPanel.GetSelectedEventCount(), 2u, "the selection still names both events");

	ZENITH_ASSERT_TRUE(xPanel.Action_Undo(), "one undo brings everything back");
	ZENITH_ASSERT_EQ(xPanel.Document().GetEventCount(), 2u, "both events again");
	ZENITH_ASSERT_EQ(xPanel.Document().GetKeyCount(xTrack), 3u, "and the key");

	Flux_AnimationEvent xEvent;
	ZENITH_ASSERT_TRUE(xPanel.Document().GetEvent(uLeftId, xEvent), "the first resolves under its ORIGINAL id");
	ZENITH_ASSERT_EQ_FLOAT(xEvent.m_fNormalizedTime, 0.25f, 0.0f, "at exactly its original fraction");
	ZENITH_ASSERT_TRUE(xPanel.Document().GetEvent(uRightId, xEvent), "so does the second");
	ZENITH_ASSERT_EQ_FLOAT(xEvent.m_fNormalizedTime, 0.75f, 0.0f, "at exactly its original fraction");

	ZENITH_ASSERT_TRUE(xPanel.IsEventSelected(uLeftId), "and the selection came back WITH them");
	ZENITH_ASSERT_TRUE(xPanel.IsEventSelected(uRightId), "and the selection came back WITH them");
	ZENITH_ASSERT_TRUE(xPanel.IsKeySelected(xTrack, uKey2), "the key's selection too");
}

//==============================================================================
// (23) Rename and payload are ONE EventEdit command EACH, and a field left
// alone commits nothing.
//
// The "nothing" half is the load-bearing one. The strip commits on
// edit-complete, so a click into the name box and straight back out fires the
// same code path an edit does — and without the equality refusal that would
// push an undo entry whose Ctrl+Z visibly does nothing, once per click.
//==============================================================================
ZENITH_TEST(AnimPanel, RenamingAnEventAndEditingItsPayloadAreOneUndoStepEach)
{
	AnimPanelFixture xFixture("zenith_animpanel_eventedit");
	AnimPanelWriteProbe(xFixture.m_strPath, /*bGenerated*/ false);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the probe clip opens");

	ZENITH_ASSERT_TRUE(xPanel.Action_AddEvent(0.25f, Zenith_EditorPanel_Animation::DefaultEventName()),
		"an event to edit");
	const u_int uEventId = xPanel.GetSelectedEventIdAt(0u);

	// ---- the refusals --------------------------------------------------------
	ZENITH_ASSERT_FALSE(xPanel.Action_RenameEvent(999999u, "Nope"), "an id the document never issued is refused");
	ZENITH_ASSERT_FALSE(xPanel.Action_RenameEvent(uEventId, ""), "and so is an empty name");
	ZENITH_ASSERT_FALSE(xPanel.Action_RenameEvent(uEventId, Zenith_EditorPanel_Animation::DefaultEventName()),
		"renaming to the name it already has is not an edit");
	ZENITH_ASSERT_FALSE(xPanel.Action_SetEventPayload(999999u, Zenith_Maths::Vector4(1.0f, 0.0f, 0.0f, 0.0f)),
		"the payload refuses an unknown id too");
	ZENITH_ASSERT_FALSE(xPanel.Action_SetEventPayload(uEventId, Zenith_Maths::Vector4(0.0f, 0.0f, 0.0f, 0.0f)),
		"and a payload the event already carries");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 1u, "so only the ADD is on the stack");

	// ---- the real edits ------------------------------------------------------
	ZENITH_ASSERT_TRUE(xPanel.Action_RenameEvent(uEventId, "FootstepLeft"), "the rename lands");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 2u,
		"as ONE command for the completed edit, not one per keystroke");
	ZENITH_ASSERT_TRUE(xPanel.Action_SetEventPayload(uEventId, Zenith_Maths::Vector4(1.0f, 2.0f, 3.0f, 4.0f)),
		"the payload edit lands");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 3u, "as one more");

	Flux_AnimationEvent xEvent;
	ZENITH_ASSERT_TRUE(xPanel.Document().GetEvent(uEventId, xEvent), "the event resolves");
	ZENITH_ASSERT_STREQ(xEvent.m_strEventName.c_str(), "FootstepLeft", "with the new name");
	ZENITH_ASSERT_EQ_FLOAT(xEvent.m_xData.x, 1.0f, 0.0f, "and all four payload components");
	ZENITH_ASSERT_EQ_FLOAT(xEvent.m_xData.y, 2.0f, 0.0f, "and all four payload components");
	ZENITH_ASSERT_EQ_FLOAT(xEvent.m_xData.z, 3.0f, 0.0f, "and all four payload components");
	ZENITH_ASSERT_EQ_FLOAT(xEvent.m_xData.w, 4.0f, 0.0f, "and all four payload components");
	ZENITH_ASSERT_EQ_FLOAT(xEvent.m_fNormalizedTime, 0.25f, 0.0f, "neither edit moved it");

	// ---- and each undoes on its own -----------------------------------------
	ZENITH_ASSERT_TRUE(xPanel.Action_Undo(), "one undo reverses the payload");
	ZENITH_ASSERT_TRUE(xPanel.Document().GetEvent(uEventId, xEvent), "the event still resolves");
	ZENITH_ASSERT_EQ_FLOAT(xEvent.m_xData.x, 0.0f, 0.0f, "the payload is back");
	ZENITH_ASSERT_EQ_FLOAT(xEvent.m_xData.w, 0.0f, 0.0f, "the payload is back");
	ZENITH_ASSERT_STREQ(xEvent.m_strEventName.c_str(), "FootstepLeft", "and the name is untouched by it");

	ZENITH_ASSERT_TRUE(xPanel.Action_Undo(), "the next undo reverses the rename");
	ZENITH_ASSERT_TRUE(xPanel.Document().GetEvent(uEventId, xEvent), "the event still resolves");
	ZENITH_ASSERT_STREQ(xEvent.m_strEventName.c_str(), Zenith_EditorPanel_Animation::DefaultEventName(),
		"back to the name it was added with");
	ZENITH_ASSERT_TRUE(xPanel.IsEventSelected(uEventId), "and the selection survived both");
	ZENITH_ASSERT_EQ(xPanel.GetInspectorEventId(), uEventId, "so the strip is still pointing at it");
}

//==============================================================================
// (24) D4, BOTH HALVES AT ONCE: a duration change MOVES THE ROW POSITION and
// LEAVES THE STORED VALUE ALONE.
//
// ★ THE RECT IS RENDERED EITHER SIDE, and the expectation is recomputed from
// the pure mapping rather than from the first measurement, so the two failure
// modes this guards against are separated on the line that fails: an
// implementation that "adjusts" the fraction when the duration moves fails the
// stored-value assertion, and one that forgot the * duration in the row's draw
// fails the pixel one.
//==============================================================================
ZENITH_TEST(AnimPanel, ChangingTheDurationMovesTheEventRowAndNotTheStoredValue)
{
	AnimPanelFixture xFixture("zenith_animpanel_eventduration");
	AnimPanelWriteProbe(xFixture.m_strPath, /*bGenerated*/ false);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the probe clip opens");
	ZENITH_ASSERT_EQ_FLOAT(xPanel.Document().GetDuration(), 2.0f, 0.0f, "at 2 s");

	ZENITH_ASSERT_TRUE(xPanel.Action_AddEvent(0.25f, "Beat"), "an event a quarter of the way in");
	const u_int uEventId = xPanel.GetSelectedEventIdAt(0u);

	xPanel.RequestWindowPlacement(40.0f, 40.0f, 900.0f, 600.0f);
	AnimPanelRenderFrames(xPanel, 2u);

	ZENITH_ASSERT_TRUE(xPanel.WasSheetDrawnLastFrame(), "the sheet pass ran");
	ZENITH_ASSERT_GT(xPanel.GetLastTrackWidth(), 0.0f, "with a key lane to draw into");

	Zenith_AnimPanelRect xBefore;
	ZENITH_ASSERT_TRUE(xPanel.GetEventRect(uEventId, xBefore), "the event flag is on screen and recorded");
	ZENITH_ASSERT_EQ_FLOAT(xBefore.Centre().x, Zenith_AnimTimelineTimeToPixel(xPanel.View(), 0.25f * 2.0f), 1.0f,
		"its centre x IS TimeToPixel(view, fNormalized * duration) — the panel derives no mapping of its own");

	// ---- double the clip -----------------------------------------------------
	ZENITH_ASSERT_TRUE(xPanel.Action_SetDuration(4.0f), "the duration doubles");
	AnimPanelRenderFrames(xPanel, 2u);
	ZENITH_ASSERT_EQ_FLOAT(xPanel.Document().GetDuration(), 4.0f, 0.0f, "the clip is 4 s now");

	Flux_AnimationEvent xEvent;
	ZENITH_ASSERT_TRUE(xPanel.Document().GetEvent(uEventId, xEvent), "the event resolves under the same id");
	// ★ THE STORED VALUE DID NOT MOVE. It is already expressed relative to
	// whatever the duration becomes, so rescaling it here would move the event a
	// second time — the exact reason Action_RippleRetime leaves events out too.
	ZENITH_ASSERT_EQ_FLOAT(xEvent.m_fNormalizedTime, 0.25f, 0.0f,
		"and its stored fraction is BIT-FOR-BIT what it was — a duration change is not an event edit");

	Zenith_AnimPanelRect xAfter;
	ZENITH_ASSERT_TRUE(xPanel.GetEventRect(uEventId, xAfter), "the flag is still on screen");
	ZENITH_ASSERT_EQ_FLOAT(xAfter.Centre().x, Zenith_AnimTimelineTimeToPixel(xPanel.View(), 0.25f * 4.0f), 1.0f,
		"and now sits where the SAME fraction of the NEW duration maps to");
	ZENITH_ASSERT_GT(xAfter.Centre().x, xBefore.Centre().x + 1.0f,
		"which is somewhere else entirely (else this test would pass on a panel that ignored the duration)");
}

//==============================================================================
// (25) D40 — a scrub emits nothing until the toggle says otherwise, and what
// the strip shows is what the RUNTIME dispatcher fired.
//
// ★ THE COUNT COMES THROUGH THE Flux_AnimationEventCallback, not from the panel
// re-deriving which events the playhead crossed. A second opinion would agree
// with D35-D40 exactly until one of them changed, and the strip would then show
// events no listener received — which is the failure WU-5A was written to end.
//
// Needs a REAL rig: Seek is inert until one resolves, so the fixture writes a
// two-bone skeleton and a one-triangle mesh. Both are device-free and nothing
// here renders a frame.
//==============================================================================
ZENITH_TEST(AnimPanel, ScrubEmitsEventsOnlyWhenTheToggleIsOn)
{
	AnimPanelFixture xFixture("zenith_animpanel_scrubemit");
	AnimPanelWriteRiggedProbe(xFixture);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the rigged probe opens");
	ZENITH_ASSERT_FALSE(xPanel.Session().NeedsRigSelection(), "whose rig resolved (else every scrub below is inert)");
	ZENITH_ASSERT_FALSE(xPanel.GetEmitEventsOnScrub(), "and the toggle is OFF by default (D40)");
	ZENITH_ASSERT_EQ(xPanel.GetTotalEmittedEventCount(), 0u, "with nothing emitted yet");

	// The event lands in the DOCUMENT; the session holds its own deep copy (D30),
	// so it has to be pushed across before the preview can fire it. The panel does
	// this from Render via NotifyDocumentEdited; here it is done directly, which
	// keeps this unit off the preview pane entirely.
	ZENITH_ASSERT_TRUE(xPanel.Action_AddEvent(0.5f, "Beat"), "an event at the middle of the clip");
	ZENITH_ASSERT_TRUE(xPanel.Session().RefreshClipFrom(xPanel.Document().GetClip()),
		"and the session re-copies the clip");
	ZENITH_ASSERT_EQ(xPanel.Session().GetClip().GetEvents().GetSize(), 1u, "so the preview's copy carries it");

	// ---- toggle OFF: a scrub across the event fires nothing -------------------
	ZENITH_ASSERT_TRUE(xPanel.Action_Scrub(1.5f), "a scrub straight past the event lands");
	ZENITH_ASSERT_EQ_FLOAT(xPanel.Session().GetTime(), 1.5f, 1.0e-3f, "and the clock moved");
	ZENITH_ASSERT_EQ(xPanel.GetTotalEmittedEventCount(), 0u,
		"but NOTHING fired — dragging across a clip must not replay every footstep in it");
	ZENITH_ASSERT_EQ(xPanel.GetEmittedEventCount(), 0u, "so the strip is empty");

	// The mark still MOVED (D40), which is what stops the next forward step from
	// replaying the whole skipped span as one burst; scrub back so the span below
	// starts before the event.
	ZENITH_ASSERT_TRUE(xPanel.Action_Scrub(0.0f), "back to the start");
	ZENITH_ASSERT_EQ(xPanel.GetTotalEmittedEventCount(), 0u, "a backward scrub emits nothing either");

	// ---- toggle ON ------------------------------------------------------------
	ZENITH_ASSERT_TRUE(xPanel.Action_SetEmitEventsOnScrub(true), "the toggle flips");
	ZENITH_ASSERT_TRUE(xPanel.GetEmitEventsOnScrub(), "and reads back on");
	ZENITH_ASSERT_TRUE(xPanel.Session().Controller().GetEmitEventsOnSeek(),
		"on the SESSION'S OWN controller, which is the one the preview seeks");
	ZENITH_ASSERT_FALSE(xPanel.Action_SetEmitEventsOnScrub(true), "setting it to what it already is does nothing");

	ZENITH_ASSERT_TRUE(xPanel.Action_Scrub(1.5f), "the same scrub again");
	ZENITH_ASSERT_EQ(xPanel.GetTotalEmittedEventCount(), 1u, "fires the event in the scrubbed span, exactly once");
	ZENITH_ASSERT_EQ(xPanel.GetEmittedEventCount(), 1u, "and the strip has one entry");

	std::string strMostRecent;
	ZENITH_ASSERT_TRUE(xPanel.GetEmittedEventNameAt(0u, strMostRecent), "index 0 is the most recent");
	ZENITH_ASSERT_STREQ(strMostRecent.c_str(), "Beat", "and it names the event that fired");
	ZENITH_ASSERT_FALSE(xPanel.GetEmittedEventNameAt(1u, strMostRecent), "there is no second entry");

	// ★ EMITTING IS NOT EDITING. The toggle writes nothing to the document and the
	// scrub writes nothing either; putting either on the clip's undo stack would
	// make Ctrl+Z change what the play head does.
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 1u, "only the ADD is on the undo stack");
}

//==============================================================================
// POSE AUTHORING (WU-4.1). Still headless, still not requiresGraphics: the
// preview pane occupies the same rectangle whether or not the backend can hand
// it a texture, and everything below is CPU maths over that rectangle.
//==============================================================================

//==============================================================================
// (A) Action_SelectBone / Action_ClearBoneSelection round-trip.
//
// No frame is needed: neither action reads ImGui state, which is the whole
// contract the Action_* surface exists to keep.
//==============================================================================
ZENITH_TEST(AnimPanel, BoneSelectionActionsRoundTrip)
{
	AnimPanelFixture xFixture("zenith_animpanel_boneselect");
	AnimPanelWriteRiggedProbe(xFixture);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the rigged probe opens");
	ZENITH_ASSERT_FALSE(xPanel.Session().NeedsRigSelection(),
		"whose rig resolved (else there is no bone to select and every assertion below is vacuous)");
	ZENITH_ASSERT_EQ(xPanel.Session().GetBoneCount(), 2u, "Hip -> Spine");

	ZENITH_ASSERT_FALSE(xPanel.Action_ClearBoneSelection(), "clearing nothing reports that it did nothing");

	ZENITH_ASSERT_TRUE(xPanel.Action_SelectBone(1u), "selecting a bone that exists takes");
	ZENITH_ASSERT_TRUE(xPanel.Session().HasBoneSelection(), "and the session holds it");
	ZENITH_ASSERT_EQ(xPanel.Session().GetSelectedBoneIndex(), 1u, "at the index asked for");

	ZENITH_ASSERT_FALSE(xPanel.Action_SelectBone(7u), "an index the rig does not have is refused");
	ZENITH_ASSERT_FALSE(xPanel.Session().HasBoneSelection(),
		"and clears rather than leaving an index nothing can resolve");

	ZENITH_ASSERT_TRUE(xPanel.Action_SelectBone(0u), "the root selects too");
	ZENITH_ASSERT_TRUE(xPanel.Action_ClearBoneSelection(), "clearing a live selection reports the change");
	ZENITH_ASSERT_FALSE(xPanel.Action_ClearBoneSelection(), "and is idempotent afterwards");

	// ★ THE WHOLE PHASE-4 SURFACE IS DECLARED, AND WHAT IS STILL A STUB SAYS SO.
	// A stub that returned true would let a caller wired up early report success
	// having written no key at all. WU-4.3 has since filled Set Key, auto-key,
	// the angle snap, the rotate primitive and the pointer drag — those have
	// their own units below, and what this pins here is the SELECTION-less
	// refusal each of them makes, because the selection was just cleared.
	Zenith_Vector<u_int> axNoBones;
	ZENITH_ASSERT_FALSE(xPanel.Action_SetKeyForBones(axNoBones, true, false),
		"Set Key with no bones writes nothing rather than pushing an empty undo step");
	ZENITH_ASSERT_FALSE(xPanel.Action_SetKeyForSelectedBone(), "and its twin needs a SELECTED bone");
	ZENITH_ASSERT_FALSE(xPanel.Action_SetKeyTranslationForRoot(), "as does the root-translation verb");
	ZENITH_ASSERT_FALSE(xPanel.Action_RotateSelectedBoneWorld(Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f)),
		"and so does the rotate primitive");
	ZENITH_ASSERT_FALSE(xPanel.Action_BakeIKForSelectedChain(Zenith_Maths::Vector3(0.0f)),
		"the IK bake is WU-4.4's to fill and still refuses");

	// The two toggles report whether the value CHANGED, matching
	// Action_SetEmitEventsOnScrub — a toggle set to what it already is pushes
	// nothing and reports nothing.
	ZENITH_ASSERT_FALSE(xPanel.Action_GetAutoKey(), "auto-key is OFF by default");
	ZENITH_ASSERT_TRUE(xPanel.Action_SetAutoKey(true), "turning it on is a change");
	ZENITH_ASSERT_TRUE(xPanel.Action_GetAutoKey(), "and it reads back on");
	ZENITH_ASSERT_FALSE(xPanel.Action_SetAutoKey(true), "setting it to what it already is reports nothing");
	ZENITH_ASSERT_TRUE(xPanel.Action_SetAutoKey(false), "and off again is a change");
	ZENITH_ASSERT_FALSE(xPanel.Action_GetPoseAngleSnap(), "the angle snap is OFF by default too");
	ZENITH_ASSERT_TRUE(xPanel.Action_SetPoseAngleSnap(true), "and behaves the same way");
	ZENITH_ASSERT_FALSE(xPanel.Action_SetPoseAngleSnap(true), "setting it to what it already is reports nothing");

	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 0u,
		"★ and not one of the refusals above touched the clip");
}

//==============================================================================
// (B) A click on the preview image selects the bone under it.
//
// ★ THE TARGET PIXEL IS COMPUTED, NOT GUESSED: the test asks the pick set where
// the bone IS in world space, projects that through the preview camera, and
// clicks the answer. Projection and un-projection are separate code paths — one
// multiplies the view-projection forward, the other inverts it through
// Zenith_Gizmo::ScreenToWorldRay — so the round trip is a real assertion rather
// than a value compared against a re-computation of itself.
//==============================================================================
ZENITH_TEST(AnimPanel, ClickingThePreviewImageSelectsTheBoneUnderIt)
{
	AnimPanelFixture xFixture("zenith_animpanel_bonepick");
	AnimPanelWriteRiggedProbe(xFixture);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the rigged probe opens");
	ZENITH_ASSERT_FALSE(xPanel.Session().NeedsRigSelection(), "and its rig resolved");

	xPanel.RequestWindowPlacement(40.0f, 40.0f, 900.0f, 600.0f);
	AnimPanelRenderFrames(xPanel, 2u);

	// The diagnostics first, as everywhere else on this panel: a bare false from
	// the pick has four causes and only one of them is "the ray missed".
	ZENITH_ASSERT_TRUE(xPanel.WasSheetDrawnLastFrame(), "the window drew its body");
	Zenith_AnimPanelRect xImage;
	ZENITH_ASSERT_TRUE(xPanel.GetPreviewImageRect(xImage),
		"the preview pane occupied a rectangle (it does so on EVERY backend — see the header)");
	ZENITH_ASSERT_GT(xImage.Width(), 0.0f, "with real width to express a pixel against");

	Zenith_Maths::Matrix4 xView(1.0f);
	Zenith_Maths::Matrix4 xProj(1.0f);
	ZENITH_ASSERT_TRUE(xPanel.GetPreviewViewProj(xView, xProj), "and the preview camera is available");

	// Where is the Hip bone, really? Ask the same geometry a click will hit.
	const Zenith_BonePickSet& xSet = xPanel.Session().GetBonePickSet();
	ZENITH_ASSERT_GT(xSet.m_xShapes.GetSize(), 0u, "the rig produced pick geometry");

	const Zenith_BonePickShape* pxHip = nullptr;
	const Zenith_BonePickShape* pxSpineJoint = nullptr;
	for (u_int u = 0; u < xSet.m_xShapes.GetSize(); ++u)
	{
		const Zenith_BonePickShape& xShape = xSet.m_xShapes.Get(u);
		if (!xShape.m_bIsJointOnly && xShape.m_uBoneIndex == 0u) { pxHip = &xShape; }
		if (xShape.m_bIsJointOnly && xShape.m_uBoneIndex == 1u) { pxSpineJoint = &xShape; }
	}
	ZENITH_ASSERT_NOT_NULL(pxHip, "the ROOT owns the capsule running up to Spine");
	ZENITH_ASSERT_NOT_NULL(pxSpineJoint, "and the leaf owns a joint sphere");
	if (pxHip == nullptr || pxSpineJoint == nullptr)
	{
		return;
	}

	// ---- the capsule's midpoint selects its OWNER ---------------------------
	const Zenith_Maths::Vector3 xMidpoint = (pxHip->m_xA + pxHip->m_xB) * 0.5f;
	float fPixelX = 0.0f;
	float fPixelY = 0.0f;
	ZENITH_ASSERT_TRUE(xPanel.ProjectPreviewWorldPoint(xMidpoint, fPixelX, fPixelY),
		"the middle of that capsule is in front of the preview camera");
	ZENITH_ASSERT_GE(fPixelX, 0.0f, "and projects inside the image");
	ZENITH_ASSERT_LE(fPixelX, xImage.Width(), "and projects inside the image");
	ZENITH_ASSERT_GE(fPixelY, 0.0f, "and projects inside the image");
	ZENITH_ASSERT_LE(fPixelY, xImage.Height(), "and projects inside the image");

	ZENITH_ASSERT_TRUE(xPanel.Action_PickBoneAtPreviewPixel(fPixelX, fPixelY),
		"clicking that pixel hits a bone");
	ZENITH_ASSERT_EQ(xPanel.Session().GetSelectedBoneIndex(), 0u,
		"and selects the bone whose ROTATION swings that segment — the parent, not the child");

	// ---- the leaf's joint selects the leaf ----------------------------------
	ZENITH_ASSERT_TRUE(xPanel.ProjectPreviewWorldPoint(pxSpineJoint->m_xB, fPixelX, fPixelY),
		"the leaf joint projects too");
	ZENITH_ASSERT_TRUE(xPanel.Action_PickBoneAtPreviewPixel(fPixelX, fPixelY), "and is clickable");
	ZENITH_ASSERT_EQ(xPanel.Session().GetSelectedBoneIndex(), 1u,
		"★ resolving to the LEAF, not to the parent capsule whose end cap it sits inside");

	// ---- a click on empty space misses, and changes nothing -----------------
	ZENITH_ASSERT_FALSE(xPanel.Action_PickBoneAtPreviewPixel(2.0f, 2.0f),
		"a click in the far corner of the pane hits nothing");
	ZENITH_ASSERT_EQ(xPanel.Session().GetSelectedBoneIndex(), 1u,
		"and a miss is NOT a deselect — that is Action_ClearBoneSelection's job");

	// A pick before anything has been rendered has no image size to work from,
	// and says so rather than inventing one.
	Zenith_EditorPanel_Animation xUnrendered;
	ZENITH_ASSERT_FALSE(xUnrendered.Action_PickBoneAtPreviewPixel(10.0f, 10.0f),
		"a panel that has never drawn cannot resolve a preview pixel");
}

//==============================================================================
// POSE AUTHORING (WU-4.3) — the manipulator, the drag transaction, Set Key and
// auto-key. Still headless and still not requiresGraphics, for the reason the
// WU-4.1 block above is: the preview pane occupies the same rectangle whether or
// not the backend can hand it a texture, and the manipulator is CPU maths over
// that rectangle.
//==============================================================================

namespace
{
	// Spine's rotation track: EMPTY in the rigged probe (only Hip's POSITION is
	// animated), which is what makes it the first-key case §5.1 is about.
	Zenith_AnimTrackId AnimPanelSpineRotation()
	{
		return Zenith_AnimTrackId::Bone("Spine", FLUX_ANIM_TRACK_ROTATION);
	}

	// A synthetic ring: a circle sampled exactly the way GetPoseRingSet samples
	// one, so the pure hit test can be exercised with no camera, no rig and no
	// frame — which is the whole point of the geometry being free functions.
	void AnimPanelMakeCircleRing(Zenith_AnimPoseRing& xRing, float fCentreX, float fCentreY, float fRadius)
	{
		for (u_int u = 0; u <= uANIM_POSE_RING_SEGMENTS; ++u)
		{
			const u_int uStep = u % uANIM_POSE_RING_SEGMENTS;
			const float fTheta = 6.28318530717958f * static_cast<float>(uStep)
				/ static_cast<float>(uANIM_POSE_RING_SEGMENTS);
			xRing.m_axPoints[u] = Zenith_Maths::Vector2(
				fCentreX + fRadius * std::cos(fTheta), fCentreY + fRadius * std::sin(fTheta));
		}
		xRing.m_bValid = true;
	}

	// |dot| of two quaternions: 1 when they are the same rotation (q and -q
	// included), which is the only comparison that means anything here.
	float AnimPanelQuatAlignment(const Zenith_Maths::Quat& xA, const Zenith_Maths::Quat& xB)
	{
		return std::fabs(xA.x * xB.x + xA.y * xB.y + xA.z * xB.z + xA.w * xB.w);
	}

	// Press on ring uAxis at parameter point uFrom and walk the cursor to uTo,
	// ONE Action_UpdateBoneDragToPixel per step. That is the thing being pinned:
	// however many steps a gesture spans, the release produces one undo entry.
	//
	// Leaves the drag OPEN — the caller decides whether it ends or is cancelled.
	//
	// ★ THE RING THE PRESS ACTUALLY GRABS IS NOT ASSERTED ANYWHERE. Three
	// projected rings genuinely cross, and which one is nearest a shared pixel is
	// a property of the camera rather than of the code under test; the point
	// walked is exactly ON ring uAxis, so the grab always succeeds and the traced
	// path always sweeps a real angle whichever ring answered.
	bool AnimPanelRingDrag(Zenith_EditorPanel_Animation& xPanel, u_int uAxis, u_int uFrom, u_int uTo)
	{
		Zenith_AnimPoseRingSet xRings;
		if (!xPanel.GetPoseRingSet(xRings) || !xRings.m_axRings[uAxis].m_bValid)
		{
			return false;
		}
		// Captured ONCE and reused: the selected bone's own joint is the FIXED
		// POINT of the rotation being authored, so the rings do not move under
		// the cursor while it is dragging them.
		const Zenith_AnimPoseRing xRing = xRings.m_axRings[uAxis];
		if (!xPanel.Action_BeginBoneDragAtPixel(xRing.m_axPoints[uFrom].x, xRing.m_axPoints[uFrom].y))
		{
			return false;
		}
		for (u_int u = uFrom + 1u; u <= uTo; ++u)
		{
			if (!xPanel.Action_UpdateBoneDragToPixel(xRing.m_axPoints[u].x, xRing.m_axPoints[u].y))
			{
				return false;
			}
		}
		return true;
	}

	// The press half only — a gesture that grabs a ring and lets go without ever
	// moving.
	bool AnimPanelRingPressOnly(Zenith_EditorPanel_Animation& xPanel, u_int uAxis, u_int uPoint)
	{
		Zenith_AnimPoseRingSet xRings;
		if (!xPanel.GetPoseRingSet(xRings) || !xRings.m_axRings[uAxis].m_bValid)
		{
			return false;
		}
		const Zenith_Maths::Vector2 xPoint = xRings.m_axRings[uAxis].m_axPoints[uPoint];
		return xPanel.Action_BeginBoneDragAtPixel(xPoint.x, xPoint.y);
	}
}

//==============================================================================
// (C) The ring geometry, with no panel at all.
//
// ★ THE THREE THINGS BELOW ARE EXACTLY THE THREE THAT ARE EASY TO GET SILENTLY
// WRONG: which way round a basis is wound (a sign error turns every drag
// backwards), what a pivot-relative pixel delta means as an angle, and which of
// several overlapping rings a click grabbed. None of them needs a camera, a rig
// or a frame, which is why the manipulator's maths is free functions.
//==============================================================================
ZENITH_TEST(AnimPanel, PoseRingGeometryIsPure)
{
	// ---- cross(u, v) == the axis, for all three ------------------------------
	// This is the contract that makes the ring PARAMETER the right-handed
	// rotation angle about the axis, which is in turn what lets a drag recover
	// its screen sign by measuring one step of it.
	for (u_int uAxis = 0; uAxis < 3u; ++uAxis)
	{
		Zenith_Maths::Vector3 xU(0.0f);
		Zenith_Maths::Vector3 xV(0.0f);
		Zenith_AnimPoseRingBasis(uAxis, xU, xV);
		const Zenith_Maths::Vector3 xCross = Zenith_Maths::Cross(xU, xV);
		ZENITH_ASSERT_NEAR_VEC3(xCross, Zenith_AnimPoseRingAxis(uAxis), 1.0e-6f,
			"cross(u, v) must BE the ring's axis, or the parameter runs backwards");
		ZENITH_ASSERT_EQ_FLOAT(Zenith_Maths::Dot(xU, xV), 0.0f, 1.0e-6f, "and the basis is orthogonal");
	}

	// ---- a pivot-relative pixel delta IS the angle ---------------------------
	const Zenith_Maths::Vector2 xPivot(100.0f, 100.0f);
	const float fHALF_PI = 1.57079632679f;
	ZENITH_ASSERT_EQ_FLOAT(Zenith_AnimPoseSignedScreenAngle(xPivot,
		Zenith_Maths::Vector2(140.0f, 100.0f), Zenith_Maths::Vector2(100.0f, 140.0f)), fHALF_PI, 1.0e-5f,
		"+x to +y is a quarter turn one way");
	ZENITH_ASSERT_EQ_FLOAT(Zenith_AnimPoseSignedScreenAngle(xPivot,
		Zenith_Maths::Vector2(100.0f, 140.0f), Zenith_Maths::Vector2(140.0f, 100.0f)), -fHALF_PI, 1.0e-5f,
		"and a quarter turn the other way when the arms are swapped");
	ZENITH_ASSERT_EQ_FLOAT(Zenith_AnimPoseSignedScreenAngle(xPivot,
		Zenith_Maths::Vector2(140.0f, 100.0f), Zenith_Maths::Vector2(140.0f, 100.0f)), 0.0f, 1.0e-6f,
		"a cursor that did not move sweeps nothing");
	ZENITH_ASSERT_EQ_FLOAT(std::fabs(Zenith_AnimPoseSignedScreenAngle(xPivot,
		Zenith_Maths::Vector2(140.0f, 100.0f), Zenith_Maths::Vector2(60.0f, 100.0f))), 3.14159265f, 1.0e-5f,
		"and a half turn is pi, whichever branch atan2 lands on");
	// The distance from the pivot must NOT matter — a drag that reached further
	// out is the same rotation, not a bigger one.
	ZENITH_ASSERT_EQ_FLOAT(Zenith_AnimPoseSignedScreenAngle(xPivot,
		Zenith_Maths::Vector2(101.0f, 100.0f), Zenith_Maths::Vector2(100.0f, 900.0f)), fHALF_PI, 1.0e-4f,
		"the angle is scale-free in the radius");
	// ★ A CURSOR ON THE PIVOT HAS NO DIRECTION, and normalising it would produce
	// a NaN that then reaches a bone rotation and a saved .zanim.
	ZENITH_ASSERT_EQ_FLOAT(Zenith_AnimPoseSignedScreenAngle(xPivot, xPivot,
		Zenith_Maths::Vector2(140.0f, 100.0f)), 0.0f, 1.0e-6f, "a degenerate arm sweeps nothing, not NaN");

	// ---- the NEAREST ring wins ----------------------------------------------
	// Three concentric circles, so "nearest" is a number the test controls
	// exactly rather than an accident of a projection.
	Zenith_AnimPoseRingSet xRings;
	xRings.m_xPivotPixel = xPivot;
	AnimPanelMakeCircleRing(xRings.m_axRings[0], 100.0f, 100.0f, 50.0f);
	AnimPanelMakeCircleRing(xRings.m_axRings[1], 100.0f, 100.0f, 80.0f);
	AnimPanelMakeCircleRing(xRings.m_axRings[2], 100.0f, 100.0f, 120.0f);

	u_int uAxis = 0xABCDu;
	ZENITH_ASSERT_TRUE(Zenith_AnimPosePickRing(xRings, 100.0f, 152.0f, 8.0f, uAxis), "2 px off the inner ring hits");
	ZENITH_ASSERT_EQ(uAxis, 0u, "and picks the INNER one, 2 px away, not the middle one 28 px away");
	ZENITH_ASSERT_TRUE(Zenith_AnimPosePickRing(xRings, 100.0f, 182.0f, 8.0f, uAxis), "2 px off the middle ring hits");
	ZENITH_ASSERT_EQ(uAxis, 1u, "and picks the middle one");

	uAxis = 0xABCDu;
	ZENITH_ASSERT_FALSE(Zenith_AnimPosePickRing(xRings, 100.0f, 100.0f, 8.0f, uAxis),
		"the centre is 50 px from the nearest ring, which is a miss");
	ZENITH_ASSERT_EQ(uAxis, 0xABCDu,
		"★ and a miss leaves the output UNTOUCHED — a caller that ignored the bool would "
		"otherwise read a plausible 0 meaning 'the X ring'");

	// An INVALID ring is never picked: half of it is behind the camera and the
	// missing arc is exactly where a mirrored coordinate would land.
	xRings.m_axRings[0].m_bValid = false;
	ZENITH_ASSERT_FALSE(Zenith_AnimPosePickRing(xRings, 100.0f, 152.0f, 8.0f, uAxis),
		"a ring that failed to project is not hit-testable, however near the cursor is");
	ZENITH_ASSERT_GT(Zenith_AnimPoseDistanceToRing(xRings.m_axRings[0], 100.0f, 152.0f), 1.0e6f,
		"and reports an unreachable distance rather than a small one");
}

//==============================================================================
// (D) The rings are projected around the selected bone's OWN joint — the FIXED
// POINT of the rotation they author.
//
// ★ THAT IS THE HALF OF §3 A READER IS MOST LIKELY TO GET BACKWARDS. With
// L_i = T(p) * R(q) * S, the translation of M_i is M_parent(i) * p, which does
// not contain q_i at all: rotating the bone moves its CHILDREN's joints and
// leaves its own exactly where it was. So the assertion is not "the pivot is
// somewhere sensible" but "the pivot does not move when the bone turns", which
// is checkable to the pixel.
//==============================================================================
ZENITH_TEST(AnimPanel, PoseRingsAreCentredOnTheJointTheBoneTurnsAbout)
{
	AnimPanelFixture xFixture("zenith_animpanel_posering");
	AnimPanelWriteRiggedProbe(xFixture);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the rigged probe opens");
	ZENITH_ASSERT_FALSE(xPanel.Session().NeedsRigSelection(), "and its rig resolved");

	Zenith_AnimPoseRingSet xRings;
	ZENITH_ASSERT_TRUE(xPanel.Action_SelectBone(1u), "select the leaf bone");
	ZENITH_ASSERT_FALSE(xPanel.GetPoseRingSet(xRings),
		"★ but there are no rings before a frame has been DRAWN: the projection needs the "
		"preview image's SIZE, and a panel that invented one would hand out a pixel nothing "
		"can be clicked at — the same failing-closed contract every rect accessor keeps");

	xPanel.RequestWindowPlacement(40.0f, 40.0f, 900.0f, 600.0f);
	AnimPanelRenderFrames(xPanel, 2u);
	ZENITH_ASSERT_TRUE(xPanel.WasSheetDrawnLastFrame(), "the window drew its body");
	ZENITH_ASSERT_TRUE(xPanel.GetPoseRingSet(xRings), "and now there are rings");

	ZENITH_ASSERT_TRUE(xPanel.Action_ClearBoneSelection(), "drop the selection");
	Zenith_AnimPoseRingSet xUnselected;
	ZENITH_ASSERT_FALSE(xPanel.GetPoseRingSet(xUnselected), "and they go: there is nothing to manipulate");
	ZENITH_ASSERT_TRUE(xPanel.Action_SelectBone(1u), "select it again");
	ZENITH_ASSERT_TRUE(xPanel.GetPoseRingSet(xRings), "and they come back");

	const Flux_SkeletonInstance* pxInstance = xPanel.Session().GetSkeletonInstance();
	ZENITH_ASSERT_NOT_NULL(pxInstance, "the session owns a skeleton instance");
	if (pxInstance == nullptr)
	{
		return;
	}

	const Zenith_Maths::Vector3 xJoint = Zenith_BoneSpace::BoneWorldPosition(
		xPanel.Session().GetSessionModelMatrix(), *pxInstance, 1u);
	float fJointX = 0.0f;
	float fJointY = 0.0f;
	ZENITH_ASSERT_TRUE(xPanel.ProjectPreviewWorldPoint(xJoint, fJointX, fJointY), "which projects");
	ZENITH_ASSERT_EQ_FLOAT(xRings.m_xPivotPixel.x, fJointX, 1.0e-3f, "the ring centre IS that joint");
	ZENITH_ASSERT_EQ_FLOAT(xRings.m_xPivotPixel.y, fJointY, 1.0e-3f, "the ring centre IS that joint");

	for (u_int uAxis = 0; uAxis < 3u; ++uAxis)
	{
		ZENITH_ASSERT_TRUE(xRings.m_axRings[uAxis].m_bValid,
			"every ring projected — none of them straddles the near plane at this orbit");
		// Closed by construction, so the hit test walks every segment without a
		// wrap-around case and cannot fall through a hairline gap.
		ZENITH_ASSERT_EQ_FLOAT(xRings.m_axRings[uAxis].m_axPoints[uANIM_POSE_RING_SEGMENTS].x,
			xRings.m_axRings[uAxis].m_axPoints[0].x, 0.0f, "the polyline closes EXACTLY");
		ZENITH_ASSERT_EQ_FLOAT(xRings.m_axRings[uAxis].m_axPoints[uANIM_POSE_RING_SEGMENTS].y,
			xRings.m_axRings[uAxis].m_axPoints[0].y, 0.0f, "the polyline closes EXACTLY");
		// A ring with no radius on screen would make every pick ambiguous and
		// every drag a divide by nothing.
		const float fDx = xRings.m_axRings[uAxis].m_axPoints[0].x - xRings.m_xPivotPixel.x;
		const float fDy = xRings.m_axRings[uAxis].m_axPoints[0].y - xRings.m_xPivotPixel.y;
		ZENITH_ASSERT_GT(std::sqrt(fDx * fDx + fDy * fDy), 4.0f,
			"and is big enough on screen to grab (the radius is camera-relative, not rig-relative)");
	}

	// ★ THE ASSERTION THIS TEST EXISTS FOR.
	ZENITH_ASSERT_TRUE(xPanel.Action_RotateSelectedBoneWorld(
		Zenith_Maths::AngleAxis(0.5f, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f))), "turn the bone");
	Zenith_AnimPoseRingSet xAfter;
	ZENITH_ASSERT_TRUE(xPanel.GetPoseRingSet(xAfter), "the rings are still there");
	ZENITH_ASSERT_EQ_FLOAT(xAfter.m_xPivotPixel.x, fJointX, 1.0e-3f,
		"and the pivot has not moved: the bone turns ABOUT its own joint, it does not move it");
	ZENITH_ASSERT_EQ_FLOAT(xAfter.m_xPivotPixel.y, fJointY, 1.0e-3f,
		"and the pivot has not moved: the bone turns ABOUT its own joint, it does not move it");
}

//==============================================================================
// (E) Set Key — insert, replace, and what each one's undo does.
//==============================================================================
ZENITH_TEST(AnimPanel, SetKeyInsertsThenReplacesAndUndoRestoresEach)
{
	AnimPanelFixture xFixture("zenith_animpanel_setkey");
	AnimPanelWriteRiggedProbe(xFixture);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the rigged probe opens");
	ZENITH_ASSERT_FALSE(xPanel.Session().NeedsRigSelection(), "and its rig resolved");
	ZENITH_ASSERT_EQ(xPanel.GetFrameRate(), 30u, "on a 30 fps grid");

	const Zenith_AnimTrackId xTrack = AnimPanelSpineRotation();
	ZENITH_ASSERT_EQ(xPanel.Document().GetKeyCount(xTrack), 0u,
		"Spine's rotation channel starts EMPTY — only Hip's POSITION is animated");
	ZENITH_ASSERT_TRUE(xPanel.Action_SelectBone(1u), "select Spine");

	// ★ THE PLAY HEAD IS PARKED OFF THE GRID ON PURPOSE. 0.5111 s at 30 fps is
	// frame 15.333, and the key has to land on frame 15 (0.5 s exactly) — an
	// unsnapped key would make the sheet's frame columns lie and would make
	// D11's occupancy test fire or not on float noise.
	ZENITH_ASSERT_TRUE(xPanel.Action_Scrub(0.5111f), "park the play head between two frames");

	ZENITH_ASSERT_TRUE(xPanel.Action_SetKeyForSelectedBone(), "Set Key on an empty channel takes");
	ZENITH_ASSERT_EQ(xPanel.Document().GetKeyCount(xTrack), 1u, "and creates the channel with one key in it");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 1u, "as ONE undo step");

	const u_int uKeyId = xPanel.Document().GetKeyIdAtIndex(xTrack, 0u);
	ZENITH_ASSERT_NE(uKeyId, uINVALID_ANIM_KEY_ID, "with a stable id");
	float fKeyTime = -1.0f;
	ZENITH_ASSERT_TRUE(xPanel.Document().GetKeyTime(xTrack, uKeyId, fKeyTime), "the key resolves");
	ZENITH_ASSERT_EQ_FLOAT(fKeyTime, 0.5f, 1.0e-5f, "★ snapped to frame 15, not left at 0.5111");
	ZENITH_ASSERT_EQ_FLOAT(fKeyTime, Zenith_AnimTimelineFrameToTime(15u, 30u), 0.0f,
		"and BIT-IDENTICAL to the frame time it names, which is what the shared snap guarantees");

	Zenith_AnimKeyValue xFirst;
	ZENITH_ASSERT_TRUE(xPanel.Document().GetKeyValue(xTrack, uKeyId, xFirst), "and carries a value");
	ZENITH_ASSERT_TRUE(xFirst.m_bIsRotation, "tagged ROTATION, which is what the track holds");
	ZENITH_ASSERT_EQ_FLOAT(xFirst.m_xQuat.w, 1.0f, 1.0e-4f, "the bind identity — nothing has been posed yet");

	// ---- an OCCUPIED time is a value edit that KEEPS the id (D11 / D25) -------
	ZENITH_ASSERT_TRUE(xPanel.Action_RotateSelectedBoneWorld(
		Zenith_Maths::AngleAxis(0.6f, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f))), "pose the bone");
	ZENITH_ASSERT_TRUE(xPanel.Action_SetKeyForSelectedBone(), "Set Key again, on the same frame");
	ZENITH_ASSERT_EQ(xPanel.Document().GetKeyCount(xTrack), 1u,
		"★ still ONE key — an occupied slot is a value edit, not a second key");
	ZENITH_ASSERT_EQ(xPanel.Document().GetKeyIdAtIndex(xTrack, 0u), uKeyId,
		"★ under the SAME id, which is what the dope sheet's selection is holding");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 2u, "and one more undo step");

	Zenith_AnimKeyValue xSecond;
	ZENITH_ASSERT_TRUE(xPanel.Document().GetKeyValue(xTrack, uKeyId, xSecond), "the replaced value reads back");
	ZENITH_ASSERT_GT(std::fabs(xSecond.m_xQuat.y), 0.05f, "and is the posed rotation, not the identity");

	// ---- undo restores the VALUE, then removes the key ----------------------
	ZENITH_ASSERT_TRUE(xPanel.Action_Undo(), "undo the replace");
	ZENITH_ASSERT_EQ(xPanel.Document().GetKeyCount(xTrack), 1u,
		"★ which restores a VALUE rather than deleting a key the user never created");
	ZENITH_ASSERT_EQ(xPanel.Document().GetKeyIdAtIndex(xTrack, 0u), uKeyId, "still the same id");
	Zenith_AnimKeyValue xRestored;
	ZENITH_ASSERT_TRUE(xPanel.Document().GetKeyValue(xTrack, uKeyId, xRestored), "and reads back");
	ZENITH_ASSERT_EQ_FLOAT(xRestored.m_xQuat.y, xFirst.m_xQuat.y, 1.0e-6f, "as the value it had before");
	ZENITH_ASSERT_EQ_FLOAT(xRestored.m_xQuat.w, xFirst.m_xQuat.w, 1.0e-6f, "as the value it had before");

	ZENITH_ASSERT_TRUE(xPanel.Action_Undo(), "undo the insert");
	ZENITH_ASSERT_EQ(xPanel.Document().GetKeyCount(xTrack), 0u, "which DOES remove it");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 0u, "and the stack is back where it started");

	// ---- what a translation key is, and is not ------------------------------
	// Spine has a parent, so the root-translation verb refuses it outright rather
	// than silently downgrading to a rotation-only key.
	ZENITH_ASSERT_FALSE(xPanel.Action_SetKeyTranslationForRoot(),
		"a translation key on a LIMB is refused — root motion is the only place one belongs");
	ZENITH_ASSERT_TRUE(xPanel.Action_SelectBone(0u), "select the ROOT");
	ZENITH_ASSERT_TRUE(xPanel.Action_SetKeyTranslationForRoot(), "where it is allowed");
	ZENITH_ASSERT_EQ(xPanel.Document().GetKeyCount(Zenith_AnimTrackId::Bone("Hip", FLUX_ANIM_TRACK_ROTATION)), 1u,
		"writing rotation");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 1u,
		"★ and BOTH tracks are ONE undo step, not two");
	ZENITH_ASSERT_EQ(xPanel.Document().GetKeyCount(Zenith_AnimTrackId::Bone("Hip", FLUX_ANIM_TRACK_SCALE)), 0u,
		"★ and SCALE is never written: the first key on a channel changes that bone across the WHOLE clip");
}

//==============================================================================
// (F) Auto-key decides whether a released drag writes anything at all.
//==============================================================================
ZENITH_TEST(AnimPanel, AutoKeyDecidesWhetherAReleasedDragWritesAKey)
{
	AnimPanelFixture xFixture("zenith_animpanel_autokey");
	AnimPanelWriteRiggedProbe(xFixture);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the rigged probe opens");
	ZENITH_ASSERT_FALSE(xPanel.Session().NeedsRigSelection(), "and its rig resolved");
	xPanel.RequestWindowPlacement(40.0f, 40.0f, 900.0f, 600.0f);
	AnimPanelRenderFrames(xPanel, 2u);
	ZENITH_ASSERT_TRUE(xPanel.WasSheetDrawnLastFrame(), "the window drew its body");
	ZENITH_ASSERT_TRUE(xPanel.Action_SelectBone(1u), "select Spine");

	const Zenith_AnimTrackId xTrack = AnimPanelSpineRotation();

	// ---- OFF (the default) ---------------------------------------------------
	ZENITH_ASSERT_FALSE(xPanel.Action_GetAutoKey(), "auto-key is off by default");
	ZENITH_ASSERT_TRUE(AnimPanelRingDrag(xPanel, 1u, 6u, 18u), "a quarter turn round the Y ring");
	ZENITH_ASSERT_TRUE(xPanel.Action_EndBoneDrag(), "released");

	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 0u, "auto-key OFF pushes NO undo entry");
	ZENITH_ASSERT_EQ(xPanel.Document().GetKeyCount(xTrack), 0u, "and writes NO key");
	ZENITH_ASSERT_TRUE(xPanel.Session().HasUnkeyedPose(),
		"★ the pose is live-but-unkeyed, and the pane must SAY so — it is the one thing a user can silently lose");

	// A SEEK is the explicit discard: the controller re-evaluates every bone from
	// the clip, so whatever the drag left behind is gone and the badge goes too.
	ZENITH_ASSERT_TRUE(xPanel.Action_Scrub(0.25f), "scrub away");
	ZENITH_ASSERT_FALSE(xPanel.Session().HasUnkeyedPose(), "which discards it, visibly");

	// ---- ON ------------------------------------------------------------------
	ZENITH_ASSERT_TRUE(xPanel.Action_SetAutoKey(true), "turn auto-key on");
	ZENITH_ASSERT_TRUE(AnimPanelRingDrag(xPanel, 1u, 6u, 18u), "the same quarter turn");
	ZENITH_ASSERT_TRUE(xPanel.Action_EndBoneDrag(), "released");

	ZENITH_ASSERT_EQ(xPanel.Document().GetKeyCount(xTrack), 1u, "writes exactly one key");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 1u, "as exactly one undo step");
	ZENITH_ASSERT_FALSE(xPanel.Session().HasUnkeyedPose(), "and the badge goes: the pose IS in the clip now");
}

//==============================================================================
// (G) One drag is one undo step, however many frames it spanned — and a drag
// that never moved records nothing.
//==============================================================================
ZENITH_TEST(AnimPanel, OneBoneDragIsOneUndoStepAndAnEmptyOneRecordsNothing)
{
	AnimPanelFixture xFixture("zenith_animpanel_posedrag");
	AnimPanelWriteRiggedProbe(xFixture);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the rigged probe opens");
	ZENITH_ASSERT_FALSE(xPanel.Session().NeedsRigSelection(), "and its rig resolved");
	xPanel.RequestWindowPlacement(40.0f, 40.0f, 900.0f, 600.0f);
	AnimPanelRenderFrames(xPanel, 2u);
	ZENITH_ASSERT_TRUE(xPanel.WasSheetDrawnLastFrame(), "the window drew its body");

	ZENITH_ASSERT_FALSE(xPanel.Action_BeginBoneDragAtPixel(10.0f, 10.0f),
		"nothing to grab with no bone selected");
	ZENITH_ASSERT_TRUE(xPanel.Action_SelectBone(1u), "select Spine");
	ZENITH_ASSERT_TRUE(xPanel.Action_SetAutoKey(true), "with auto-key on, so the release has something to record");

	const Zenith_AnimTrackId xTrack = AnimPanelSpineRotation();

	// ★ TWELVE Update CALLS — twelve FRAMES of a real drag. The thing being
	// pinned is that the count does not reach the undo stack.
	ZENITH_ASSERT_TRUE(AnimPanelRingDrag(xPanel, 1u, 6u, 18u), "the ring was grabbed and walked");
	ZENITH_ASSERT_TRUE(xPanel.IsBonePoseDragActive(), "the drag is in flight");
	ZENITH_ASSERT_NE(xPanel.GetPoseDragAxis(), uINVALID_ANIM_POSE_RING, "on a ring it can name");
	ZENITH_ASSERT_GT(std::fabs(xPanel.GetPoseDragAngleRadians()), 0.05f, "having swept a real angle");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 0u,
		"★ and NOTHING has reached the document yet — the live pose is not an undo entry");

	ZENITH_ASSERT_TRUE(xPanel.Action_EndBoneDrag(), "release");
	ZENITH_ASSERT_FALSE(xPanel.IsBonePoseDragActive(), "which ends it");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 1u,
		"★ ONE undo entry for the whole gesture, not one per frame of it");
	ZENITH_ASSERT_EQ(xPanel.Document().GetKeyCount(xTrack), 1u, "and one key");

	ZENITH_ASSERT_TRUE(xPanel.Action_Undo(), "one Ctrl+Z takes the whole drag back");
	ZENITH_ASSERT_EQ(xPanel.Document().GetKeyCount(xTrack), 0u, "leaving the channel as it was");
	const u_int uDepth = xPanel.Document().GetUndoStackSize();

	// ---- a press and release that never moved -------------------------------
	ZENITH_ASSERT_TRUE(AnimPanelRingPressOnly(xPanel, 1u, 6u), "grab a ring");
	ZENITH_ASSERT_TRUE(xPanel.IsBonePoseDragActive(), "the drag starts");
	ZENITH_ASSERT_TRUE(xPanel.Action_EndBoneDrag(), "and is released without moving");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), uDepth,
		"★ a click that never moved records NOTHING, auto-key or no auto-key");
	ZENITH_ASSERT_EQ(xPanel.Document().GetKeyCount(xTrack), 0u, "and writes no key");
	ZENITH_ASSERT_FALSE(xPanel.Session().HasUnkeyedPose(),
		"and leaves no unkeyed-pose badge behind either, because it changed no pose");

	ZENITH_ASSERT_FALSE(xPanel.Action_EndBoneDrag(), "ending a drag that is not in flight reports nothing");
	ZENITH_ASSERT_FALSE(xPanel.Action_UpdateBoneDragToPixel(1.0f, 1.0f), "and neither does updating one");
}

//==============================================================================
// (H) Escape cancels a drag and puts the bone back where it found it.
//==============================================================================
ZENITH_TEST(AnimPanel, CancellingABoneDragRestoresTheRotationItStartedFrom)
{
	AnimPanelFixture xFixture("zenith_animpanel_posecancel");
	AnimPanelWriteRiggedProbe(xFixture);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the rigged probe opens");
	ZENITH_ASSERT_FALSE(xPanel.Session().NeedsRigSelection(), "and its rig resolved");
	xPanel.RequestWindowPlacement(40.0f, 40.0f, 900.0f, 600.0f);
	AnimPanelRenderFrames(xPanel, 2u);
	ZENITH_ASSERT_TRUE(xPanel.WasSheetDrawnLastFrame(), "the window drew its body");
	ZENITH_ASSERT_TRUE(xPanel.Action_SelectBone(1u), "select Spine");
	ZENITH_ASSERT_TRUE(xPanel.Action_SetAutoKey(true), "auto-key ON, so a COMMIT would be visible");

	const Zenith_Maths::Quat xBefore = xPanel.Session().GetBoneLocalRotation(1u);

	ZENITH_ASSERT_TRUE(AnimPanelRingDrag(xPanel, 1u, 6u, 18u), "drag the ring");
	const Zenith_Maths::Quat xDuring = xPanel.Session().GetBoneLocalRotation(1u);
	ZENITH_ASSERT_LT(AnimPanelQuatAlignment(xBefore, xDuring), 0.999f,
		"the drag genuinely moved the bone (else the restore below asserts nothing)");
	ZENITH_ASSERT_TRUE(xPanel.Session().HasUnkeyedPose(), "and left a live pose");

	ZENITH_ASSERT_TRUE(xPanel.Action_CancelBoneDrag(), "Escape");
	ZENITH_ASSERT_FALSE(xPanel.IsBonePoseDragActive(), "ends the drag");

	const Zenith_Maths::Quat xAfter = xPanel.Session().GetBoneLocalRotation(1u);
	ZENITH_ASSERT_EQ_FLOAT(AnimPanelQuatAlignment(xBefore, xAfter), 1.0f, 1.0e-5f,
		"★ and puts the bone back at the rotation BeginBoneDrag latched");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 0u,
		"★ writing NOTHING even with auto-key on: a cancel is not a release");
	ZENITH_ASSERT_FALSE(xPanel.Session().HasUnkeyedPose(),
		"and the badge goes with it — the pose is back to what the clip evaluates to");

	ZENITH_ASSERT_FALSE(xPanel.Action_CancelBoneDrag(), "and it is idempotent afterwards");
}

//==============================================================================
// BONE MASKS (WU-7.1) — the "Bone Masks" sub-panel.
//==============================================================================

//==============================================================================
// (M1) THE ACCEPTANCE CASE — the sub-panel does not offer a mask control on an
//      ADDITIVE layer, and SAYS SO.
//
// ★ WHY THIS NEEDS A RENDERED FRAME AND NOT JUST THE PURE PREDICATE. The rule
// itself is one line and is pinned in Zenith_BoneMaskDocument.Tests.inl; what
// can silently be wrong is the panel FORGETTING TO ASK IT — the control would be
// drawn, the user would author and assign a mask, and Flux_AnimationController
// would go straight to AdditiveBlend with every gate green. So this drives the
// real ImGui frame and reads the draw diagnostic.
//==============================================================================
ZENITH_TEST(AnimPanel, TheMaskSectionOffersNoMaskControlOnAnAdditiveLayer)
{
	AnimPanelFixture xFixture("zenith_animpanel_maskadditive");
	AnimPanelWriteRiggedProbe(xFixture);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the rigged probe opens");
	ZENITH_ASSERT_FALSE(xPanel.Session().NeedsRigSelection(),
		"and its rig resolved (else the section would show its no-rig prompt and this would prove nothing)");

	xPanel.RequestWindowPlacement(40.0f, 40.0f, 900.0f, 600.0f);

	//--------------------------------------------------------------------------
	// ★★ OFF BY DEFAULT MEANS ZERO ITEMS AND ZERO HEIGHT, AND THAT IS A
	// REGRESSION GUARD, NOT A WARM-UP.
	//
	// This section shipped as a COLLAPSED CollapsingHeader — one row, always
	// present, ~24 px. RenderSheet is sized from GetContentRegionAvail(), so that
	// row came straight out of the sheet's height, and the EVENTS ROW IS THE LAST
	// ROW OF THE SHEET: it dropped below the canvas bottom and
	// `ChangingTheDurationMovesTheEventRowAndNotTheStoredValue` went red on all
	// three Null_ exes with GetEventRect false on BOTH sides of the edit. That
	// reads as "the events row is broken" and is nowhere near its cause, which is
	// exactly what the off-screen gate does to a height regression.
	//
	// ★ THE ORACLE IS THE TRACK AREA'S HEIGHT, NOT WHETHER A PARTICULAR ROW FITS,
	// and that distinction cost this guard a red run of its own. The first version
	// asserted GetEventsRowRect() here — and it was false, for a reason that has
	// nothing to do with the mask section: THIS test uses the RIGGED probe, whose
	// session resolves a rig, so RenderPreviewPane takes its live-image branch and
	// occupies fSHEET_PREVIEW_SIZE_1X (192 px) plus WU-4.3's pose-toolbar line,
	// where the rig-LESS probe the duration test uses takes the rig-PROMPT branch
	// (a wrapped line, two InputTexts and a button). Same window, same eight rows,
	// ~100 px less canvas — so the events row is off the bottom before the mask
	// section is even considered.
	//
	// GetTrackAreaRect() spans m_fCanvasTop..m_fCanvasBottom: it is the height the
	// sheet was GIVEN, which is precisely the invariant ("the sheet did not lose
	// height") and is indifferent to how many rows happen to fit in it.
	//--------------------------------------------------------------------------
	ZENITH_ASSERT_FALSE(xPanel.IsMaskSectionShown(), "the section is OFF by default");
	AnimPanelRenderFrames(xPanel, 2u);
	ZENITH_ASSERT_TRUE(xPanel.WasSheetDrawnLastFrame(),
		"the sheet pass ran (else every assertion below is about a frame that never happened)");
	ZENITH_ASSERT_FALSE(xPanel.WasMaskSectionDrawnLastFrame(),
		"★ and the mask section drew NOTHING — not a collapsed header, not a label");
	ZENITH_ASSERT_EQ(xPanel.GetMaskBoneRowCount(), 0u, "no bone rows either");

	Zenith_AnimPanelRect xTrackAreaHidden;
	ZENITH_ASSERT_TRUE(xPanel.GetTrackAreaRect(xTrackAreaHidden),
		"the sheet's canvas was recorded (else the height below is not a measurement)");
	const float fTrackHeightHidden = xTrackAreaHidden.Height();
	ZENITH_ASSERT_GT(fTrackHeightHidden, 0.0f, "and it has height to lose");

	// ---- now show it: OVERRIDE, so the control IS drawn ----------------------
	ZENITH_ASSERT_TRUE(xPanel.Action_MaskOpenFresh(xFixture.m_strMaskPath), "a fresh mask opens into the section");
	ZENITH_ASSERT_TRUE(xPanel.IsMaskSectionShown(), "which shows the section");

	xPanel.SetMaskTargetLayerBlendMode(LAYER_BLEND_OVERRIDE);
	AnimPanelRenderFrames(xPanel, 2u);

	// The diagnostics first, as everywhere on this panel: "not drawn" has several
	// causes and the bool alone names none of them.
	ZENITH_ASSERT_TRUE(xPanel.WasMaskSectionDrawnLastFrame(),
		"the section body drew (else: the section is toggled off and everything below is vacuous)");
	ZENITH_ASSERT_TRUE(xPanel.WasMaskAssignmentDrawnLastFrame(),
		"an override layer gets the assignment control");
	ZENITH_ASSERT_GT(xPanel.GetMaskBoneRowCount(), 0u, "and one weight row per bone of the session's rig");

	// ★ THE ORACLE IS NOT BLIND. A before/after equality that never moves in
	// between proves nothing — it would pass just as well if GetTrackAreaRect
	// returned a constant. The section really does cost the sheet height while it
	// is shown, and that is what makes the restore below an assertion.
	//
	// ★ AN UNRECORDED TRACK RECT IN THE SHOWN STATE IS HEIGHT ZERO, NOT A
	// FAILURE, and that is the third time this guard has had to learn the same
	// lesson: on the RIGGED probe the sheet is already competing with a 192 px
	// preview and the pose toolbar, so adding the section's rows can leave it no
	// canvas at all — at which point the accessor answers false, either because
	// nothing was recorded or because the off-screen gate refused what was. Both
	// mean "the sheet got nothing", which is the strongest possible form of the
	// property being measured; requiring the rect to exist would fail the test on
	// the very outcome it is asserting. That the panel still RENDERED is already
	// proven above by WasMaskSectionDrawnLastFrame(), which is set inside the
	// window body.
	Zenith_AnimPanelRect xTrackAreaShown;
	const float fTrackHeightShown = xPanel.GetTrackAreaRect(xTrackAreaShown) ? xTrackAreaShown.Height() : 0.0f;
	ZENITH_ASSERT_LT(fTrackHeightShown, fTrackHeightHidden,
		"★ a SHOWN section really does take height out of the sheet — which is why an always-drawn "
		"one was a defect and why the restore below means something");

	// ---- ADDITIVE: it is NOT --------------------------------------------------
	xPanel.SetMaskTargetLayerBlendMode(LAYER_BLEND_ADDITIVE);
	AnimPanelRenderFrames(xPanel, 1u);

	ZENITH_ASSERT_TRUE(xPanel.WasMaskSectionDrawnLastFrame(), "the section is still drawn");
	ZENITH_ASSERT_FALSE(xPanel.WasMaskAssignmentDrawnLastFrame(),
		"★ but the assignment control is NOT — an additive layer ignores its mask entirely");
	// ★ AND THE DIAGNOSTIC SAYS WHY. "Unavailable" would send a reader looking for
	// a missing rig or an unopened file; the blend mode is the answer.
	ZENITH_ASSERT_STREQ(xPanel.GetMaskNotice(), Zenith_BoneMaskDocument::AdditiveLayerMaskNotice(),
		"and the panel's diagnostic names the reason");
	// The rest of the section keeps working — the mask is still editable, it just
	// cannot be assigned to THIS layer.
	ZENITH_ASSERT_GT(xPanel.GetMaskBoneRowCount(), 0u, "the weight rows are still listed");

	// Back to override, to show the refusal is the blend mode and not a latch.
	xPanel.SetMaskTargetLayerBlendMode(LAYER_BLEND_OVERRIDE);
	AnimPanelRenderFrames(xPanel, 1u);
	ZENITH_ASSERT_TRUE(xPanel.WasMaskAssignmentDrawnLastFrame(), "and it comes back");

	// ★ AND CLOSING THE MASK GIVES THE HEIGHT BACK — EXACTLY. The section goes
	// with the document, so the toolbar checkbox and what is on screen cannot
	// disagree, and the sheet returns to the full window rather than keeping a row
	// for an empty section. This is the assertion the whole guard is for: not "a
	// particular row is visible" (which depends on the probe's rig and its preview
	// pane) but "the sheet got its canvas back".
	ZENITH_ASSERT_TRUE(xPanel.Action_MaskClose(), "close the mask");
	ZENITH_ASSERT_FALSE(xPanel.IsMaskSectionShown(), "the section goes with it");
	AnimPanelRenderFrames(xPanel, 1u);
	ZENITH_ASSERT_FALSE(xPanel.WasMaskSectionDrawnLastFrame(), "and draws nothing again");

	Zenith_AnimPanelRect xTrackAreaRestored;
	ZENITH_ASSERT_TRUE(xPanel.GetTrackAreaRect(xTrackAreaRestored), "the canvas was recorded again");
	ZENITH_ASSERT_EQ_FLOAT(xTrackAreaRestored.Height(), fTrackHeightHidden, 0.5f,
		"★ and the sheet is back to the height it had before the section was ever shown — "
		"a hidden section costs it NOTHING");

	xPanel.Shutdown();
}

//==============================================================================
// (M2) Mask edits are one undo step each; a SUBTREE paint is one compound.
//
// ★ THROUGH THE PANEL'S Action_* TWINS, not the document's verbs directly —
// which is the layer WU-7.2 and the ANIM_MASK_* automation steps both call, and
// the layer where "the panel forgot to pass the session's rig to the subtree
// walk" would show up.
//==============================================================================
ZENITH_TEST(AnimPanel, MaskWeightEditsAreOneStepEachAndASubtreePaintIsOneCompound)
{
	AnimPanelFixture xFixture("zenith_animpanel_maskundo");
	AnimPanelWriteRiggedProbe(xFixture);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the rigged probe opens");
	ZENITH_ASSERT_FALSE(xPanel.Session().NeedsRigSelection(), "and its rig resolved");
	ZENITH_ASSERT_EQ(xPanel.Session().GetBoneCount(), 2u, "Hip -> Spine");
	ZENITH_ASSERT_TRUE(xPanel.Action_MaskOpenFresh(xFixture.m_strMaskPath), "a fresh mask opens");

	ZENITH_ASSERT_EQ(xPanel.MaskDocument().GetUndoStackSize(), 0u, "with an empty stack");

	ZENITH_ASSERT_TRUE(xPanel.Action_MaskSetWeight("Spine", 1.0f), "one weight edit");
	ZENITH_ASSERT_EQ(xPanel.MaskDocument().GetUndoStackSize(), 1u, "is one step");
	ZENITH_ASSERT_TRUE(xPanel.Action_MaskSetWeight("Spine", 1.0f), "re-stating it is SATISFIED, not refused");
	ZENITH_ASSERT_EQ(xPanel.MaskDocument().GetUndoStackSize(), 1u, "and pushes nothing");

	// ★ THE MASK'S UNDO STACK IS NOT THE CLIP'S. A Ctrl+Z in the sheet must not
	// take back a slider drag in the mask list — they are two documents and two
	// files, and folding them would make one gesture undo the other's work.
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 0u, "the CLIP document is untouched by a mask edit");

	// ---- the subtree paint ----------------------------------------------------
	ZENITH_ASSERT_TRUE(xPanel.Action_MaskSetSubtree("Hip", 0.5f), "paint Hip and everything under it");
	ZENITH_ASSERT_EQ(xPanel.MaskDocument().GetUndoStackSize(), 2u,
		"★ TWO BONES, ONE MORE STEP — the whole paint is one compound");
	ZENITH_ASSERT_EQ_FLOAT(xPanel.MaskDocument().GetBoneWeight("Hip"), 0.5f, 1e-5f, "the root of the subtree");
	ZENITH_ASSERT_EQ_FLOAT(xPanel.MaskDocument().GetBoneWeight("Spine"), 0.5f, 1e-5f, "and its child");

	ZENITH_ASSERT_TRUE(xPanel.Action_MaskUndo(), "one undo");
	ZENITH_ASSERT_FALSE(xPanel.MaskDocument().HasBone("Hip"), "takes the whole paint back");
	ZENITH_ASSERT_EQ_FLOAT(xPanel.MaskDocument().GetBoneWeight("Spine"), 1.0f, 1e-5f,
		"leaving the earlier, separate edit exactly where it was");

	ZENITH_ASSERT_TRUE(xPanel.Action_MaskRedo(), "and redo puts it back");
	ZENITH_ASSERT_EQ_FLOAT(xPanel.MaskDocument().GetBoneWeight("Hip"), 0.5f, 1e-5f, "in one gesture");

	// D47's flag, through the panel.
	ZENITH_ASSERT_TRUE(xPanel.MaskDocument().HasAvatarMask(), "a fresh mask IS a mask");
	ZENITH_ASSERT_TRUE(xPanel.Action_MaskSetHasAvatar(false), "and the flag is editable");
	ZENITH_ASSERT_FALSE(xPanel.MaskDocument().HasAvatarMask(), "and took");

	// Save, close, reopen — the round trip through the panel's own verbs.
	ZENITH_ASSERT_TRUE(xPanel.Action_MaskSave(), "the mask saves");
	ZENITH_ASSERT_FALSE(xPanel.MaskDocument().IsDirty(), "and the document is clean");
	ZENITH_ASSERT_TRUE(xPanel.Action_MaskClose(), "and closes");
	ZENITH_ASSERT_FALSE(xPanel.MaskDocument().IsOpen(), "leaving nothing open");
	ZENITH_ASSERT_FALSE(xPanel.Action_MaskClose(), "and the close is idempotent");

	Zenith_AssetRegistry::ForceUnload(xFixture.m_strMaskPath);
	ZENITH_ASSERT_TRUE(xPanel.Action_MaskOpen(xFixture.m_strMaskPath), "the saved mask reopens");
	ZENITH_ASSERT_EQ_FLOAT(xPanel.MaskDocument().GetBoneWeight("Hip"), 0.5f, 1e-5f, "with its weights");
	ZENITH_ASSERT_FALSE(xPanel.MaskDocument().HasAvatarMask(), "and its flag");

	xPanel.Shutdown();
}

//==============================================================================
// (M3) The section LISTS THE SESSION'S RIG, and PROMPTS when there is not one.
//
// ★ THE PROMPT IS NOT AN EMPTY LIST. A mask names bones; with no rig there is
// nothing to name, and a blank list would read as "this rig has no bones". The
// same distinction the subtree verb makes by refusing rather than painting one
// bone.
//==============================================================================
ZENITH_TEST(AnimPanel, TheMaskSectionListsTheSessionRigAndPromptsWithoutOne)
{
	// ---- the UNRIGGED probe: a clip whose metadata names no skeleton ----------
	{
		AnimPanelFixture xFixture("zenith_animpanel_masknorig");
		AnimPanelWriteProbe(xFixture.m_strPath, /*bGenerated*/ false);

		Zenith_EditorPanel_Animation xPanel;
		ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the unrigged probe opens");
		ZENITH_ASSERT_TRUE(xPanel.Session().NeedsRigSelection(), "and asks for a rig");
		ZENITH_ASSERT_TRUE(xPanel.Action_MaskOpenFresh(xFixture.m_strMaskPath), "a fresh mask opens anyway");

		Zenith_Vector<std::string> axNames;
		xPanel.GetMaskRigBoneNames(axNames);
		ZENITH_ASSERT_EQ(axNames.GetSize(), 0u, "there is no rig to list");

		xPanel.RequestWindowPlacement(40.0f, 40.0f, 900.0f, 600.0f);
		AnimPanelRenderFrames(xPanel, 2u);

		ZENITH_ASSERT_TRUE(xPanel.WasMaskSectionDrawnLastFrame(), "the section drew");
		ZENITH_ASSERT_EQ(xPanel.GetMaskBoneRowCount(), 0u, "with no bone rows");
		ZENITH_ASSERT_GT(static_cast<u_int>(std::string(xPanel.GetMaskNotice()).size()), 0u,
			"and a PROMPT rather than silence");

		// ★ AND THE SUBTREE VERB REFUSES rather than falling back to one bone,
		// which would look like the hierarchy was flat.
		ZENITH_ASSERT_FALSE(xPanel.Action_MaskSetSubtree("Hip", 1.0f), "no rig, no subtree");
		ZENITH_ASSERT_EQ(xPanel.MaskDocument().GetUndoStackSize(), 0u, "and nothing was pushed");

		xPanel.Shutdown();
	}

	// ---- the RIGGED probe: the list is the skeleton, in skeleton order --------
	{
		AnimPanelFixture xFixture("zenith_animpanel_maskrig");
		AnimPanelWriteRiggedProbe(xFixture);

		Zenith_EditorPanel_Animation xPanel;
		ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the rigged probe opens");
		ZENITH_ASSERT_FALSE(xPanel.Session().NeedsRigSelection(), "and its rig resolved");

		Zenith_Vector<std::string> axNames;
		xPanel.GetMaskRigBoneNames(axNames);
		ZENITH_ASSERT_EQ(axNames.GetSize(), 2u, "both bones are listed");
		if (axNames.GetSize() == 2u)
		{
			// SKELETON ORDER — parents before children, so a row's "Subtree" button
			// acts on the rows below it rather than on an arbitrary scatter.
			ZENITH_ASSERT_STREQ(axNames.Get(0).c_str(), "Hip", "the root first");
			ZENITH_ASSERT_STREQ(axNames.Get(1).c_str(), "Spine", "then its child");
		}

		ZENITH_ASSERT_TRUE(xPanel.Action_MaskOpenFresh(xFixture.m_strMaskPath), "a fresh mask opens");
		xPanel.RequestWindowPlacement(40.0f, 40.0f, 900.0f, 600.0f);
		AnimPanelRenderFrames(xPanel, 2u);
		ZENITH_ASSERT_EQ(xPanel.GetMaskBoneRowCount(), 2u, "and the section draws one row per bone");

		// ★ A FRAME THE PANEL DID NOT DRAW REPORTS NOT-DRAWN, not last frame's
		// answer — the same contract every rect accessor here carries, and the one
		// that turns "the control is missing" into a fact about a specific frame.
		xPanel.ShowFlag() = false;
		AnimPanelRenderFrames(xPanel, 1u);
		ZENITH_ASSERT_FALSE(xPanel.WasMaskSectionDrawnLastFrame(), "a hidden panel drew no section");
		ZENITH_ASSERT_EQ(xPanel.GetMaskBoneRowCount(), 0u, "and reports no rows");
		ZENITH_ASSERT_STREQ(xPanel.GetMaskNotice(), "", "and no stale explanation");

		xPanel.Shutdown();
	}
}

//==============================================================================
// THE CURVE VIEW (WU-8.2)
//==============================================================================

namespace
{
	// A clip whose Hip carries TWO ROTATION keys — identity at t=0, a quarter turn
	// about Y at t=2. Two keys is the minimum that has a SEGMENT, which is the
	// thing a tangent changes: with one key there is nothing between to move, and
	// with three the middle key's own tangents confound the measurement.
	void AnimPanelWriteRotationProbe(const std::string& strPath)
	{
		Flux_AnimationClip xClip;
		xClip.SetName("RotationProbe");
		xClip.SetDuration(2.0f);
		xClip.GetMetadata().m_uAuthoredFrameRate = 30u;

		Flux_BoneChannel xHip;
		xHip.AddRotationKeyframe(0.0f, Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f));
		xHip.AddRotationKeyframe(2.0f, glm::angleAxis(1.5707963f, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)));
		xHip.SortKeyframes();
		xClip.AddBoneChannel("Hip", std::move(xHip));

		xClip.Export(strPath);
	}

	Zenith_AnimTrackId AnimPanelHipRotation()
	{
		return Zenith_AnimTrackId::Bone("Hip", FLUX_ANIM_TRACK_ROTATION);
	}
}

//==============================================================================
// (C1) The curve view REPLACES the rows: nothing of it is drawn while it is off,
// it costs the sheet NO height while it is on, and the dope-sheet row rects are
// not recorded while it is up.
//
// ★ THE HEIGHT ORACLE IS GetTrackAreaRect().Height(), NOT "is row X visible",
// and here it is asserted EQUAL on purpose. This panel's standing rule is that
// anything drawn above RenderSheet comes straight out of the sheet's height and
// the events row is the sheet's last row — which is exactly why the curve view is
// painted INSIDE the canvas, in the rectangle the rows would have used, and why
// its five controls sit on a toolbar row that already exists. So the number that
// proves it did the right thing is a number that does NOT move.
//
// ★ AND THE EQUALITY IS PAIRED WITH TWO THINGS THAT MAKE IT NON-VACUOUS, because
// an equality that never moves would hold just as well against a constant: the
// rect population CHANGES OVER (curve rects appear, row rects vanish), and the
// window is grown at the end to show the oracle is live.
//==============================================================================
ZENITH_TEST(AnimPanel, TheCurveViewReplacesTheRowsAndCostsTheSheetNoHeight)
{
	AnimPanelFixture xFixture("zenith_animpanel_curveheight");
	AnimPanelWriteProbe(xFixture.m_strPath, /*bGenerated*/ false);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the probe clip opens");
	ZENITH_ASSERT_FALSE(xPanel.IsCurveViewShown(), "the curve view is OFF by default");

	xPanel.RequestWindowPlacement(40.0f, 40.0f, 900.0f, 600.0f);
	AnimPanelRenderFrames(xPanel, 2u);

	// The diagnostics first, as everywhere on this panel: a flat `false` below has
	// four causes and the bool names none of them.
	ZENITH_ASSERT_TRUE(xPanel.WasSheetDrawnLastFrame(),
		"the sheet pass ran (else every assertion below is about a frame that never happened)");
	ZENITH_ASSERT_GT(xPanel.GetRecordedDisplayWidth(), 0.0f,
		"a display bound was captured at record time (else every publish refuses, whatever was drawn)");

	const Zenith_AnimTrackId xTrack = AnimPanelHipPosition();

	Zenith_AnimPanelRect xTrackAreaOff;
	ZENITH_ASSERT_TRUE(xPanel.GetTrackAreaRect(xTrackAreaOff), "the sheet's canvas was recorded");
	const float fHeightOff = xTrackAreaOff.Height();
	ZENITH_ASSERT_GT(fHeightOff, 0.0f, "and it has height to lose");

	Zenith_AnimPanelRect xUnused;
	ZENITH_ASSERT_FALSE(xPanel.GetCurveViewRect(xUnused),
		"★ with the view OFF nothing of it is drawn and no curve rect is recorded");
	ZENITH_ASSERT_EQ(xPanel.GetDrawnCurveTrackCount(), 0u, "no curves either");
	ZENITH_ASSERT_EQ(xPanel.GetRecordedCurvePointCount(), 0u, "and no curve points");
	ZENITH_ASSERT_TRUE(xPanel.GetRowTrackRect(xTrack, xUnused), "while the dope-sheet ROW rect is recorded");

	// ---- switch to curves ----------------------------------------------------
	ZENITH_ASSERT_TRUE(xPanel.Action_SetCurveView(true), "the view switches on");
	ZENITH_ASSERT_TRUE(xPanel.IsCurveViewShown(), "and says so");
	AnimPanelRenderFrames(xPanel, 2u);

	Zenith_AnimPanelRect xCurveView;
	ZENITH_ASSERT_TRUE(xPanel.GetCurveViewRect(xCurveView), "the curve area was recorded");
	ZENITH_ASSERT_GT(xCurveView.Width(), 0.0f, "and spans the key lane");
	ZENITH_ASSERT_GT(xPanel.GetDrawnCurveTrackCount(), 0u, "with at least one track drawn");
	ZENITH_ASSERT_GT(xPanel.GetRecordedCurvePointCount(), 0u, "and its keys as curve points");

	// ★ THE ROWS WERE NOT PAINTED, SO THEIR RECTS ARE NOT HANDED OUT. Answering
	// with last frame's coordinate would be a click into a row nobody can see —
	// the same failure the off-screen gate exists for, one view over.
	ZENITH_ASSERT_FALSE(xPanel.GetRowTrackRect(xTrack, xUnused),
		"★ the dope-sheet row rects are NOT recorded while the curve view is up");
	const u_int uKeyAtOne = AnimPanelKeyIdAtTime(xPanel.Document(), xTrack, 1.0f);
	ZENITH_ASSERT_NE(uKeyAtOne, uINVALID_ANIM_KEY_ID, "the midpoint key resolves to an id");
	ZENITH_ASSERT_FALSE(xPanel.GetKeyRect(xTrack, uKeyAtOne, xUnused), "and neither are the key diamonds");
	ZENITH_ASSERT_TRUE(xPanel.GetCurveKeyRect(xTrack, uKeyAtOne, 1u, xUnused),
		"but that key's y-component CURVE POINT is");

	Zenith_AnimPanelRect xTrackAreaOn;
	ZENITH_ASSERT_TRUE(xPanel.GetTrackAreaRect(xTrackAreaOn), "the canvas is still recorded");
	ZENITH_ASSERT_EQ_FLOAT(xTrackAreaOn.Height(), fHeightOff, 0.5f,
		"★ and the sheet lost NO height — the curve view occupies the rows' own rectangle and its "
		"controls sit on a toolbar row that already existed");

	// ---- and back ------------------------------------------------------------
	ZENITH_ASSERT_TRUE(xPanel.Action_SetCurveView(false), "the view switches off");
	AnimPanelRenderFrames(xPanel, 1u);
	ZENITH_ASSERT_FALSE(xPanel.GetCurveViewRect(xUnused), "and draws nothing again");
	ZENITH_ASSERT_EQ(xPanel.GetRecordedCurvePointCount(), 0u, "recording no curve points");
	ZENITH_ASSERT_TRUE(xPanel.GetRowTrackRect(xTrack, xUnused), "with the rows back");

	Zenith_AnimPanelRect xTrackAreaRestored;
	ZENITH_ASSERT_TRUE(xPanel.GetTrackAreaRect(xTrackAreaRestored), "the canvas was recorded again");
	ZENITH_ASSERT_EQ_FLOAT(xTrackAreaRestored.Height(), fHeightOff, 0.5f, "at the height it always had");

	// ★ THE SENSITIVITY CHECK. Without it the three equalities above would be
	// satisfied just as well by an oracle that returned a constant. The window is
	// GROWN rather than shrunk: shrinking far enough to be convincing can drive the
	// canvas below RenderSheet's floor on a high-DPI machine, which would fail for
	// a reason that has nothing to do with what is being tested.
	xPanel.RequestWindowPlacement(40.0f, 40.0f, 900.0f, 650.0f);
	AnimPanelRenderFrames(xPanel, 2u);
	Zenith_AnimPanelRect xTrackAreaGrown;
	ZENITH_ASSERT_TRUE(xPanel.GetTrackAreaRect(xTrackAreaGrown), "the canvas was recorded in the bigger window");
	ZENITH_ASSERT_GT(xTrackAreaGrown.Height(), fHeightOff,
		"★ the height oracle genuinely MOVES when the window does — so the equalities above are "
		"assertions and not a constant agreeing with itself");

	xPanel.Shutdown();
}

//==============================================================================
// (C2) ONE SELECTION, TWO VIEWS. Switching between the dope sheet and the curve
// view preserves the selected key ids exactly.
//
// ★ A SEPARATE CURVE SELECTION WOULD BE THE OBVIOUS DESIGN AND IS THE WRONG ONE:
// an "Auto" applied in the curve view would then act on a set the dope sheet was
// not showing, and a Delete in the sheet would leave the curve view drawing
// handles for keys that are gone. There is one (track, key id) set and the view
// only decides which rects it is hit-tested against.
//==============================================================================
ZENITH_TEST(AnimPanel, TheKeySelectionSurvivesSwitchingBetweenTheDopeSheetAndTheCurveView)
{
	AnimPanelFixture xFixture("zenith_animpanel_curveselection");
	AnimPanelWriteProbe(xFixture.m_strPath, /*bGenerated*/ false);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the probe clip opens");

	const Zenith_AnimTrackId xTrack = AnimPanelHipPosition();
	const u_int uKeyA = AnimPanelKeyIdAtTime(xPanel.Document(), xTrack, 0.0f);
	const u_int uKeyB = AnimPanelKeyIdAtTime(xPanel.Document(), xTrack, 2.0f);
	ZENITH_ASSERT_NE(uKeyA, uINVALID_ANIM_KEY_ID, "the first key resolves");
	ZENITH_ASSERT_NE(uKeyB, uINVALID_ANIM_KEY_ID, "and the last");

	ZENITH_ASSERT_TRUE(xPanel.Action_SelectKey(xTrack, uKeyA, ZENITH_ANIMSELECT_REPLACE), "select one");
	ZENITH_ASSERT_TRUE(xPanel.Action_SelectKey(xTrack, uKeyB, ZENITH_ANIMSELECT_ADD), "and add another");
	ZENITH_ASSERT_EQ(xPanel.GetSelectedKeyCount(), 2u, "two keys are selected in the dope sheet");

	xPanel.RequestWindowPlacement(40.0f, 40.0f, 900.0f, 600.0f);
	AnimPanelRenderFrames(xPanel, 2u);

	ZENITH_ASSERT_TRUE(xPanel.Action_SetCurveView(true), "switch to the curve view");
	AnimPanelRenderFrames(xPanel, 2u);
	ZENITH_ASSERT_EQ(xPanel.GetSelectedKeyCount(), 2u, "the selection COUNT survived the switch");
	ZENITH_ASSERT_TRUE(xPanel.IsKeySelected(xTrack, uKeyA), "and the first id still resolves as selected");
	ZENITH_ASSERT_TRUE(xPanel.IsKeySelected(xTrack, uKeyB), "and the second");

	ZENITH_ASSERT_TRUE(xPanel.Action_SetCurveView(false), "switch back");
	AnimPanelRenderFrames(xPanel, 2u);
	ZENITH_ASSERT_EQ(xPanel.GetSelectedKeyCount(), 2u, "and it survived the way back too");
	ZENITH_ASSERT_TRUE(xPanel.IsKeySelected(xTrack, uKeyA), "with the same ids");
	ZENITH_ASSERT_TRUE(xPanel.IsKeySelected(xTrack, uKeyB), "with the same ids");

	xPanel.Shutdown();
}

//==============================================================================
// (C3) THE HANDLE MAPPING ROUND-TRIPS, AND A DRAG THROUGH IT PRODUCES EXACTLY
// THE TANGENT THE DIRECT ACTION PRODUCES.
//
// ★ THE PURE HALF NEEDS NO FRAME AT ALL, which is the point of the mapping being
// free functions: "which way up the value axis runs" and "what a handle pixel
// means as a derivative" are the two things a curve editor gets silently wrong,
// and both are answerable here with no clip, no window and no rig.
//==============================================================================
ZENITH_TEST(AnimPanel, TheTangentHandleMappingRoundTripsAndADragReproducesIt)
{
	// ---- pure: draw a handle from a tangent, read the tangent back -----------
	Zenith_AnimTimelineView xTimeView;
	xTimeView.m_fPixelsPerSecond = 200.0f;
	xTimeView.m_fScrollSeconds = 0.0f;
	xTimeView.m_fTrackLeftPixel = 100.0f;
	xTimeView.m_fTrackWidthPixels = 800.0f;

	Zenith_AnimCurveValueView xValueView;
	xValueView.m_fValueAtTop = 3.0f;
	xValueView.m_fPixelsPerUnit = 50.0f;
	xValueView.m_fTopPixel = 200.0f;
	xValueView.m_fHeightPixels = 300.0f;

	// The value axis runs UPWARDS: a bigger value is a SMALLER y.
	const float fPixelAtOne = Zenith_AnimCurveValueToPixel(xValueView, 1.0f);
	const float fPixelAtTwo = Zenith_AnimCurveValueToPixel(xValueView, 2.0f);
	ZENITH_ASSERT_LT(fPixelAtTwo, fPixelAtOne, "★ a LARGER value is a SMALLER y — the axis runs upwards");
	ZENITH_ASSERT_EQ_FLOAT(Zenith_AnimCurvePixelToValue(xValueView, fPixelAtOne), 1.0f, 1e-4f,
		"and the inverse is the inverse");

	const float afTangents[4] = { 2.5f, -2.5f, 0.0f, 137.0f };
	for (u_int u = 0; u < 4u; ++u)
	{
		for (u_int uEnd = 0; uEnd < 2u; ++uEnd)
		{
			const bool bIn = (uEnd == 0u);
			float fHandleX = 0.0f;
			float fHandleY = 0.0f;
			Zenith_AnimCurveHandlePixel(xTimeView, xValueView, 1.0f, 1.0f, afTangents[u], bIn,
				fANIM_CURVE_HANDLE_SECONDS, fHandleX, fHandleY);
			const float fBack = Zenith_AnimCurveTangentFromPixel(xTimeView, xValueView, 1.0f, 1.0f,
				bIn, fHandleX, fHandleY);
			ZENITH_ASSERT_EQ_FLOAT(fBack, afTangents[u], 1e-2f,
				"★ a handle drawn from a tangent reads back AS that tangent, at both ends and through "
				"zero — which is what makes 'drag it back to where it was drawn' a no-op rather than a "
				"slow drift");
		}
	}

	// ---- and the panel's drag agrees with the pure function -------------------
	AnimPanelFixture xFixture("zenith_animpanel_curvedrag");
	AnimPanelWriteProbe(xFixture.m_strPath, /*bGenerated*/ false);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the probe clip opens");
	ZENITH_ASSERT_TRUE(xPanel.Action_SetCurveView(true), "with the curve view up");

	xPanel.RequestWindowPlacement(40.0f, 40.0f, 900.0f, 600.0f);
	AnimPanelRenderFrames(xPanel, 2u);

	const Zenith_AnimTrackId xTrack = AnimPanelHipPosition();
	const u_int uKeyAtOne = AnimPanelKeyIdAtTime(xPanel.Document(), xTrack, 1.0f);
	ZENITH_ASSERT_NE(uKeyAtOne, uINVALID_ANIM_KEY_ID, "the midpoint key resolves");

	Zenith_AnimPanelRect xHandle;
	ZENITH_ASSERT_TRUE(xPanel.GetCurveHandleRect(xTrack, uKeyAtOne, 1u /* y */, /*bIn*/ false, xHandle),
		"its y-component OUT handle was drawn and is on screen");

	// Drop the handle 20 px ABOVE where it was drawn — a steeper positive slope.
	const float fDropX = xHandle.Centre().x;
	const float fDropY = xHandle.Centre().y - 20.0f;

	float fKeyValue = 0.0f;
	float fKeyTime = 0.0f;
	ZENITH_ASSERT_TRUE(xPanel.Document().GetKeyTime(xTrack, uKeyAtOne, fKeyTime), "the key's time reads back");
	Zenith_AnimKeyValue xKeyValue;
	ZENITH_ASSERT_TRUE(xPanel.Document().GetKeyValue(xTrack, uKeyAtOne, xKeyValue), "and its value");
	fKeyValue = xKeyValue.m_xVector.y;

	// ★ THE ORACLE IS THE PURE FUNCTION, EVALUATED AGAINST THE PANEL'S OWN LIVE
	// VIEWS. That is the whole claim: the drag verb does no arithmetic of its own.
	const float fExpected = Zenith_AnimCurveTangentFromPixel(xPanel.View(), xPanel.CurveValueView(),
		fKeyTime, fKeyValue, /*bIn*/ false, fDropX, fDropY);
	ZENITH_ASSERT_GT(std::fabs(fExpected), 0.0f, "the drop is far enough to mean something");

	const u_int uStackBefore = xPanel.Document().GetUndoStackSize();
	ZENITH_ASSERT_TRUE(xPanel.Action_DragTangentHandleToPixel(xTrack, uKeyAtOne, 1u, /*bIn*/ false, fDropX, fDropY),
		"the drag lands");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), uStackBefore + 1u, "as exactly ONE undo step");

	Flux_KeyTangents xTangents;
	ZENITH_ASSERT_TRUE(xPanel.Document().GetKeyTangents(xTrack, uKeyAtOne, xTangents), "and the pair reads back");
	ZENITH_ASSERT_EQ_FLOAT(xTangents.m_xOutTangent.y, fExpected, 1e-3f,
		"★ the drag produced exactly the tangent the pure mapping says that pixel means");
	// Unified is the default, so the other half went with it — one key, one slope.
	ZENITH_ASSERT_TRUE(xPanel.AreTangentsUnified(), "unified is the default");
	ZENITH_ASSERT_EQ_FLOAT(xTangents.m_xInTangent.y, fExpected, 1e-3f, "so the IN half moved with it");
	// The two components nobody dragged are untouched, which is what makes a
	// component a component.
	ZENITH_ASSERT_EQ_FLOAT(xTangents.m_xOutTangent.x, 0.0f, 0.0f, "the x component was not touched");
	ZENITH_ASSERT_EQ_FLOAT(xTangents.m_xOutTangent.z, 0.0f, 0.0f, "nor the z");

	xPanel.Shutdown();
}

//==============================================================================
// (C4) Auto on the SELECTION is the slope, Linear zeroes it, each is ONE undo
// step, and the undo leaves the selection still resolving.
//
// ★ "THE SELECTION SURVIVES THE UNDO" IS THE HALF THAT NEEDS SAYING. A tangent
// edit does not move a key, so nothing here reorders — but the same stable ids
// that make a retime survivable are what let the toolbar's Auto, a Ctrl+Z and a
// second Auto all name the same three keys.
//==============================================================================
ZENITH_TEST(AnimPanel, SelectionAutoIsTheSlopeLinearZeroesAndEachIsOneUndoStep)
{
	AnimPanelFixture xFixture("zenith_animpanel_curveauto");
	// y = 0 / 1 / 2 at t = 0 / 1 / 2 — three COLLINEAR keys, so the centred slope
	// at the middle and the one-sided slopes at both ends are all exactly 1.
	AnimPanelWriteProbe(xFixture.m_strPath, /*bGenerated*/ false);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the probe clip opens");

	const Zenith_AnimTrackId xTrack = AnimPanelHipPosition();
	for (u_int u = 0; u < 3u; ++u)
	{
		ZENITH_ASSERT_TRUE(xPanel.Action_SelectKey(xTrack, xPanel.Document().GetKeyIdAtIndex(xTrack, u),
			u == 0u ? ZENITH_ANIMSELECT_REPLACE : ZENITH_ANIMSELECT_ADD), "select all three keys");
	}
	ZENITH_ASSERT_EQ(xPanel.GetSelectedKeyCount(), 3u, "three keys selected");

	ZENITH_ASSERT_TRUE(xPanel.Action_SetSelectionTangentsAuto(), "Auto on the selection");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 1u,
		"★ is ONE compound — three keys, one Ctrl+Z");

	for (u_int u = 0; u < 3u; ++u)
	{
		Flux_KeyTangents xTangents;
		ZENITH_ASSERT_TRUE(xPanel.Document().GetKeyTangents(xTrack,
			xPanel.Document().GetKeyIdAtIndex(xTrack, u), xTangents), "each key's pair reads back");
		ZENITH_ASSERT_EQ_FLOAT(xTangents.m_xOutTangent.y, 1.0f, 1e-5f,
			"and Catmull-Rom through three collinear keys IS the line's slope, at both ends too");
	}

	// The displayed mode follows the numbers, which is all the wire can carry.
	Zenith_AnimCurveTangentMode eMode = ZENITH_ANIMCURVE_TANGENT_LINEAR;
	ZENITH_ASSERT_TRUE(xPanel.GetKeyTangentMode(xTrack, xPanel.Document().GetKeyIdAtIndex(xTrack, 1), eMode),
		"the mode reads back");
	ZENITH_ASSERT_TRUE(eMode == ZENITH_ANIMCURVE_TANGENT_CUSTOM,
		"★ a key Auto just wrote displays as CUSTOM, not as 'Auto' — Auto is an OPERATION and nothing "
		"re-applies it when a neighbour moves");

	ZENITH_ASSERT_TRUE(xPanel.Action_SetSelectionTangentsLinear(), "Linear on the same selection");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 2u, "is one more compound");
	for (u_int u = 0; u < 3u; ++u)
	{
		Flux_KeyTangents xTangents;
		ZENITH_ASSERT_TRUE(xPanel.Document().GetKeyTangents(xTrack,
			xPanel.Document().GetKeyIdAtIndex(xTrack, u), xTangents), "reads back");
		ZENITH_ASSERT_TRUE(Flux_TangentIsUnset(xTangents.m_xInTangent),
			"★ Linear writes exact ZEROES — the sampler's linear branch. It is NOT a flat handle, which "
			"this format cannot represent, which is why the control may never be labelled 'Flat'");
		ZENITH_ASSERT_TRUE(Flux_TangentIsUnset(xTangents.m_xOutTangent), "on both ends");
	}

	// ---- undo, with the selection intact -------------------------------------
	ZENITH_ASSERT_TRUE(xPanel.Action_Undo(), "one Ctrl+Z");
	ZENITH_ASSERT_EQ(xPanel.GetSelectedKeyCount(), 3u, "leaves the selection alone");
	for (u_int u = 0; u < 3u; ++u)
	{
		const u_int uKeyId = xPanel.Document().GetKeyIdAtIndex(xTrack, u);
		ZENITH_ASSERT_TRUE(xPanel.IsKeySelected(xTrack, uKeyId), "with every id still resolving");
		Flux_KeyTangents xTangents;
		ZENITH_ASSERT_TRUE(xPanel.Document().GetKeyTangents(xTrack, uKeyId, xTangents), "and reading back");
		ZENITH_ASSERT_EQ_FLOAT(xTangents.m_xOutTangent.y, 1.0f, 1e-5f, "the auto slope it had before Linear");
	}

	ZENITH_ASSERT_TRUE(xPanel.Action_Undo(), "a second Ctrl+Z");
	for (u_int u = 0; u < 3u; ++u)
	{
		const u_int uKeyId = xPanel.Document().GetKeyIdAtIndex(xTrack, u);
		ZENITH_ASSERT_TRUE(xPanel.IsKeySelected(xTrack, uKeyId), "the selection still resolves");
		Flux_KeyTangents xTangents;
		ZENITH_ASSERT_TRUE(xPanel.Document().GetKeyTangents(xTrack, uKeyId, xTangents), "and the pair");
		ZENITH_ASSERT_TRUE(Flux_TangentIsUnset(xTangents.m_xOutTangent),
			"★ is back to the EXACT unset pair the file carried");
	}

	xPanel.Shutdown();
}

//==============================================================================
// (C5) The DISPLAYED mode is "Linear" for an unset pair and "Custom" for
// anything else — and it is the ONLY mode a reader can derive.
//
// ★ THE LABELS ARE THE POINT OF THIS TEST, NOT THE ENUM. A per-key tangent MODE
// is not on the wire (Flux/MeshAnimation/CLAUDE.md → *Tangent sampling*), zero
// means LINEAR rather than flat, and a genuinely flat handle is unrepresentable —
// so a UI that showed "Flat" would promise an ease this format cannot store and
// silently deliver a straight line.
//==============================================================================
ZENITH_TEST(AnimPanel, TheDisplayedTangentModeIsLinearForUnsetAndCustomOtherwise)
{
	// Pure first — no clip needed to pin what the two words mean.
	Flux_KeyTangents xUnset;
	ZENITH_ASSERT_TRUE(Zenith_AnimCurveTangentModeOf(xUnset) == ZENITH_ANIMCURVE_TANGENT_LINEAR,
		"an all-zero pair displays as LINEAR");
	ZENITH_ASSERT_STREQ(Zenith_AnimCurveTangentModeLabel(ZENITH_ANIMCURVE_TANGENT_LINEAR), "Linear",
		"★ and the word is 'Linear' — never 'Flat', which this format cannot represent");
	ZENITH_ASSERT_STREQ(Zenith_AnimCurveTangentModeLabel(ZENITH_ANIMCURVE_TANGENT_CUSTOM), "Custom",
		"anything authored displays as Custom");

	Flux_KeyTangents xOneHalf;
	xOneHalf.m_xOutTangent = Zenith_Maths::Vector3(0.0f, 1.0e-6f, 0.0f);
	ZENITH_ASSERT_TRUE(Zenith_AnimCurveTangentModeOf(xOneHalf) == ZENITH_ANIMCURVE_TANGENT_CUSTOM,
		"★ ONE non-zero half is enough, and the compare is EXACT — a tolerance would swallow a "
		"deliberately tiny authored tangent and report it as untouched");

	// ---- and through the panel, against a real clip ---------------------------
	AnimPanelFixture xFixture("zenith_animpanel_curvemode");
	AnimPanelWriteProbe(xFixture.m_strPath, /*bGenerated*/ false);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the probe clip opens");

	const Zenith_AnimTrackId xTrack = AnimPanelHipPosition();
	const u_int uKeyId = xPanel.Document().GetKeyIdAtIndex(xTrack, 1);

	Zenith_AnimCurveTangentMode eMode = ZENITH_ANIMCURVE_TANGENT_CUSTOM;
	ZENITH_ASSERT_TRUE(xPanel.GetKeyTangentMode(xTrack, uKeyId, eMode), "the mode reads back");
	ZENITH_ASSERT_TRUE(eMode == ZENITH_ANIMCURVE_TANGENT_LINEAR,
		"a key of a clip nobody authored a tangent on is LINEAR — which is every clip in the tree");

	ZENITH_ASSERT_TRUE(xPanel.Action_SetKeyTangents(xTrack, uKeyId,
		Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)),
		"one tangent edit");
	ZENITH_ASSERT_TRUE(xPanel.GetKeyTangentMode(xTrack, uKeyId, eMode), "the mode reads back");
	ZENITH_ASSERT_TRUE(eMode == ZENITH_ANIMCURVE_TANGENT_CUSTOM, "and it displays as Custom");

	ZENITH_ASSERT_TRUE(xPanel.Action_Undo(), "undo it");
	ZENITH_ASSERT_TRUE(xPanel.GetKeyTangentMode(xTrack, uKeyId, eMode), "the mode reads back");
	ZENITH_ASSERT_TRUE(eMode == ZENITH_ANIMCURVE_TANGENT_LINEAR,
		"★ and it is Linear again — which is only true because the undo restored EXACT zeroes");

	// A root-motion track has no tangents at all, so it has no mode either.
	const Zenith_AnimTrackId xRoot = Zenith_AnimTrackId::RootMotion(FLUX_ANIM_TRACK_POSITION);
	const u_int uRootKeyId = xPanel.Document().InsertKey(xRoot, 0.0f, Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));
	ZENITH_ASSERT_NE(uRootKeyId, uINVALID_ANIM_KEY_ID, "a root-motion key exists");
	ZENITH_ASSERT_FALSE(xPanel.GetKeyTangentMode(xRoot, uRootKeyId, eMode),
		"★ but it has no tangents (D17), so it has no MODE — refused rather than reported as Linear");

	xPanel.Shutdown();
}

//==============================================================================
// (C6) A ROTATION handle writes an ANGULAR VELOCITY, and the sampled quaternion
// really moves mid-segment.
//
// ★ THIS IS THE ONE THAT PROVES THE CURVE VIEW EDITS THE THING THAT PLAYS. A
// rotation tangent is a body-frame angular velocity (axis * rad/s), not a
// quaternion control point, and the sampler's cumulative-Bezier form reads it in
// the key's OWN frame — so the only honest check is: sample the pose in the
// middle of the segment before and after, and require it to have moved while the
// two ENDPOINTS stay exactly where they were authored.
//==============================================================================
ZENITH_TEST(AnimPanel, ARotationHandleWritesAnAngularVelocityAndMovesTheSampledPose)
{
	AnimPanelFixture xFixture("zenith_animpanel_curverotation");
	AnimPanelWriteRotationProbe(xFixture.m_strPath);

	Zenith_EditorPanel_Animation xPanel;
	ZENITH_ASSERT_TRUE(xPanel.OpenClip(xFixture.m_strPath), "the rotation probe opens");
	ZENITH_ASSERT_TRUE(xPanel.Action_SetCurveView(true), "with the curve view up");

	const Zenith_AnimTrackId xTrack = AnimPanelHipRotation();
	const u_int uFirstKeyId = xPanel.Document().GetKeyIdAtIndex(xTrack, 0);
	const u_int uLastKeyId = xPanel.Document().GetKeyIdAtIndex(xTrack, 1);
	ZENITH_ASSERT_NE(uFirstKeyId, uINVALID_ANIM_KEY_ID, "the first rotation key resolves");
	ZENITH_ASSERT_NE(uLastKeyId, uINVALID_ANIM_KEY_ID, "and the last");

	const Flux_BoneChannel* pxChannel = xPanel.Document().GetClip().GetBoneChannel("Hip");
	ZENITH_ASSERT_NOT_NULL(pxChannel, "the channel is there to sample");
	if (pxChannel == nullptr)
	{
		return;
	}

	const Zenith_Maths::Quat xMidBefore = pxChannel->SampleRotation(1.0f);
	const Zenith_Maths::Quat xStartBefore = pxChannel->SampleRotation(0.0f);
	const Zenith_Maths::Quat xEndBefore = pxChannel->SampleRotation(2.0f);

	// ★ THROUGH THE SELECTION VERB, which is a HANDLE EDIT expressed as numbers:
	// Auto on the two keys of a single segment writes each one's angular velocity
	// in its own body frame, exactly as a drag on its handle would.
	ZENITH_ASSERT_TRUE(xPanel.Action_SelectKey(xTrack, uFirstKeyId, ZENITH_ANIMSELECT_REPLACE), "select the first");
	ZENITH_ASSERT_TRUE(xPanel.Action_SetKeyTangents(xTrack, uFirstKeyId,
		Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(0.0f, 2.0f, 0.0f)),
		"give its OUT end an angular velocity of 2 rad/s about Y");

	Flux_KeyTangents xTangents;
	ZENITH_ASSERT_TRUE(xPanel.Document().GetKeyTangents(xTrack, uFirstKeyId, xTangents), "which reads back");
	ZENITH_ASSERT_EQ_FLOAT(xTangents.m_xOutTangent.y, 2.0f, 1e-6f, "as rad/s about Y, on the OUT end");
	ZENITH_ASSERT_TRUE(Flux_TangentIsUnset(xTangents.m_xInTangent),
		"with the IN end left UNSET — which the sampler still reads as slerp's own velocity");

	// Re-fetched rather than reusing the pointer above: nothing in a tangent edit
	// rehashes the channel map, but a test should not rest on that.
	pxChannel = xPanel.Document().GetClip().GetBoneChannel("Hip");
	ZENITH_ASSERT_NOT_NULL(pxChannel, "the channel is still there");
	const Zenith_Maths::Quat xMidAfter = pxChannel->SampleRotation(1.0f);
	const Zenith_Maths::Quat xStartAfter = pxChannel->SampleRotation(0.0f);
	const Zenith_Maths::Quat xEndAfter = pxChannel->SampleRotation(2.0f);

	const float fMidDot = std::fabs(glm::dot(xMidBefore, xMidAfter));
	ZENITH_ASSERT_LT(fMidDot, 0.9999f,
		"★ the pose MID-SEGMENT really moved — a tangent that reached the file but not the sampler "
		"would leave this identical, and nothing else in this test would notice");

	// ★ AND THE ENDPOINTS DID NOT. A Hermite that moved its own endpoints would be
	// a curve through different keys, which is the loudest possible way for the
	// segment arithmetic to be wrong and the easiest to miss by only measuring the
	// middle.
	ZENITH_ASSERT_EQ_FLOAT(std::fabs(glm::dot(xStartBefore, xStartAfter)), 1.0f, 1e-5f,
		"the key at t=0 is exactly where it was authored");
	ZENITH_ASSERT_EQ_FLOAT(std::fabs(glm::dot(xEndBefore, xEndAfter)), 1.0f, 1e-5f,
		"and so is the key at t=2");

	// The undo puts the segment back on the sampler's bit-identical linear branch.
	ZENITH_ASSERT_TRUE(xPanel.Action_Undo(), "one Ctrl+Z");
	ZENITH_ASSERT_TRUE(xPanel.Document().GetKeyTangents(xTrack, uFirstKeyId, xTangents), "the pair reads back");
	ZENITH_ASSERT_TRUE(Flux_TangentIsUnset(xTangents.m_xOutTangent), "unset again");
	ZENITH_ASSERT_TRUE(xPanel.IsKeySelected(xTrack, uFirstKeyId), "with the selection intact");
	pxChannel = xPanel.Document().GetClip().GetBoneChannel("Hip");
	ZENITH_ASSERT_NOT_NULL(pxChannel, "the channel is still there");
	if (pxChannel == nullptr)
	{
		return;
	}
	const Zenith_Maths::Quat xMidUndone = pxChannel->SampleRotation(1.0f);
	ZENITH_ASSERT_EQ_FLOAT(std::fabs(glm::dot(xMidBefore, xMidUndone)), 1.0f, 1e-5f,
		"★ and the mid-segment pose is back to the slerp it was — bit-identical, not merely close");

	xPanel.Shutdown();
}
