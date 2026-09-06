//------------------------------------------------------------------------------
// Zenith_EditorPanel_Animation unit tests (WU-3.2).
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
#include "Flux/RenderViews/Flux_PreviewSlotArbiter.h"

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
		}

		~AnimPanelFixture()
		{
			Zenith_AssetRegistry::ForceUnload(m_strPath);
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

	Zenith_AnimTrackId AnimPanelHipPosition()
	{
		return Zenith_AnimTrackId::Bone("Hip", FLUX_ANIM_TRACK_POSITION);
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
