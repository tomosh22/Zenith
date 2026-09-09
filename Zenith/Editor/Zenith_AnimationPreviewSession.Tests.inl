//------------------------------------------------------------------------------
// Zenith_AnimationPreviewSession unit tests (WU-2.4).
// Included at the bottom of Zenith_AnimationPreviewSession.cpp.
//
// ★ THE HEADLINE PROPERTIES, one test each:
//   1. two previews are two clocks — ticking one never advances the other, and
//      one controller can only be driven ONCE per frame however many callers ask;
//   2. a preview still plays when SOME OTHER controller has layers (the layer
//      short-circuit in UpdateWithSkeletonInstance disables direct play, and the
//      session sidesteps it by owning a layer-free controller) — and the new seek
//      works even ON a layered controller, because it bypasses that dispatcher;
//   3. SCRUBBING TO t AND PLAYING TO t PRODUCE THE SAME POSE — the one property
//      that makes a play head trustworthy, and the reason the tick and the seek
//      share SampleDirectPlayPoseAtCurrentTime rather than each sampling;
//   4. the session raises and lowers ITS OWN render view (kuFluxViewSlotPreviewAnim)
//      and never touches the material editor's — three tests, one per edge;
//   5. a clip with no rig asks for one, and the answer is remembered per clip;
//   6. a seek MOVES the event bookkeeping mark even though it emits nothing, so
//      the next forward tick does not replay the skipped span as a burst (D40).
//
// ★ UpdatePreviewView() IS EXERCISED HERE, and the note that used to say it could
// not be was wrong on its premise. It needs a Flux_GraphicsImpl, not a GPU: the
// view registry is a pure CPU fixed-slot array, the Null backend builds a graphics
// object exactly like the Vulkan one, and Zenith_Engine::Initialise creates it long
// before the boot batch runs these units. Everything else here is CPU-only too —
// the rig and the preview mesh are tiny assets written into a private temp
// directory, the clips are plain data, and nothing touches a device or a UI. None
// of these is requiresGraphics.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"
#include "UnitTests/Zenith_UnitTests.h"
// The session no longer includes the arbiter — it is not one of its claimants any
// more — but the one test below that still covers the arbiter does.
#include "Flux/RenderViews/Flux_PreviewSlotArbiter.h"

#include <filesystem>

namespace
{
	//--------------------------------------------------------------------------
	// The live view registry, through the SAME accessor the session stages with
	// (TryGetPreviewViewRegistry, in this TU's anonymous namespace above). Asking
	// for it the same way is deliberate: a test that reached the registry by some
	// other route could pass while the session was staging into a different one.
	//
	// Null only in a run with no Flux_GraphicsImpl at all. The boot unit batch is
	// not that run — see the file header — so every test below asserts it is here
	// rather than skipping, which is how a graphics-gated test rots.
	//--------------------------------------------------------------------------
	Flux_RenderViewRegistry* AnimPreview_Views()
	{
		return TryGetPreviewViewRegistry();
	}

	// Fixture teardown/setup half — see the fixture note below.
	void LowerTheAnimationPreviewView()
	{
		Flux_RenderViewRegistry* pxViews = AnimPreview_Views();
		if (pxViews != nullptr)
		{
			pxViews->SetViewActive(kuFluxViewSlotPreviewAnim, false);
		}
	}

	//--------------------------------------------------------------------------
	// Fixture — the shape Zenith_AnimationDocument.Tests.inl established: a
	// private temp directory removed on the way out, plus a ForceUnload of every
	// registry path the test caused to be loaded so a throwaway asset never
	// lingers in the live registry the suite runs inside.
	//
	// ★ IT ALSO RESTORES TWO PIECES OF PROCESS-LEVEL STATE at BOTH ends, and both
	// are process-level because the resources are: ONE render-view registry and
	// ONE arbiter inside ONE renderer.
	//
	//   - The ANIMATION PREVIEW VIEW (slot 6). These units raise a real view in the
	//     live registry; a unit that left it raised would hand a rendering preview
	//     to every test after it, and to the automation run that follows the batch.
	//     Nothing else lowers this slot — the material preview's per-frame janitor
	//     owns slot 5 and only slot 5.
	//   - The PREVIEW-SLOT ARBITER. The session is no longer a claimant, but the
	//     arbiter test below is, and a unit that left it claimed would have an
	//     unrelated test reporting the wrong owner.
	//
	// Slot 5 is deliberately NOT touched here: one of the tests asserts that the
	// session leaves it exactly as it found it, which a fixture that reset it would
	// make unfalsifiable.
	//
	// ★ DECLARE THE FIXTURE BEFORE ANY SESSION IN EVERY TEST. A session pins the
	// rig assets with owning handles; ForceUnload deletes them regardless of
	// refcount, so the sessions have to be destroyed first. Declaration order in
	// the test body is what guarantees that.
	//--------------------------------------------------------------------------
	struct AnimPreviewFixture
	{
		std::filesystem::path m_xDirectory;
		std::string m_strSkeletonPath;
		std::string m_strMeshPath;

		explicit AnimPreviewFixture(const char* szLeafDirectory)
		{
			Flux_PreviewSlotArbiter::ResetForTesting();
			LowerTheAnimationPreviewView();

			std::error_code xError;
			std::filesystem::path xRoot = std::filesystem::temp_directory_path(xError);
			if (xError)
			{
				xRoot = ".";
			}
			m_xDirectory = xRoot / szLeafDirectory;
			std::filesystem::remove_all(m_xDirectory, xError);
			std::filesystem::create_directories(m_xDirectory, xError);

			m_strSkeletonPath = (m_xDirectory / "rig.zskel").generic_string();
			m_strMeshPath = (m_xDirectory / "rig.zasset").generic_string();

			// A three-bone rig: Root -> Spine -> Head. Small on purpose — the pose
			// comparisons below walk every bone.
			{
				Zenith_SkeletonAsset xSkeleton;
				const Zenith_Maths::Quat xIdentity = glm::identity<Zenith_Maths::Quat>();
				const Zenith_Maths::Vector3 xUnitScale(1.0f);
				xSkeleton.AddBone("Root", -1, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), xIdentity, xUnitScale);
				xSkeleton.AddBone("Spine", 0, Zenith_Maths::Vector3(0.0f, 0.5f, 0.0f), xIdentity, xUnitScale);
				xSkeleton.AddBone("Head", 1, Zenith_Maths::Vector3(0.0f, 0.4f, 0.0f), xIdentity, xUnitScale);
				xSkeleton.ComputeBindPoseMatrices();
				xSkeleton.Export(m_strSkeletonPath.c_str());
			}

			// ★ A BARE .zasset MESH, NOT A .zmodel — that is the shape the generated
			// tree and bush sway clips record, and the session has to accept it.
			//
			// ★ HAND-BUILT, NOT GenerateUnitCube. That helper ends with
			// EnsureGPUBuffers(), and this fixture has to stay device-free: the
			// mesh here exists only to be a resolvable preview PATH. One triangle
			// is enough for that and for the load to round-trip.
			{
				Zenith_MeshAsset xMesh;
				xMesh.Reserve(3, 3);
				xMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f), Zenith_Maths::Vector2(0.0f, 0.0f));
				xMesh.AddVertex(Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f), Zenith_Maths::Vector2(1.0f, 0.0f));
				xMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f), Zenith_Maths::Vector2(0.0f, 1.0f));
				xMesh.AddTriangle(0u, 1u, 2u);
				xMesh.AddSubmesh(0u, 3u, 0u);
				xMesh.ComputeBounds();
				xMesh.Export(m_strMeshPath.c_str());
			}
		}

		~AnimPreviewFixture()
		{
			Zenith_AssetRegistry::ForceUnload(m_strSkeletonPath);
			Zenith_AssetRegistry::ForceUnload(m_strMeshPath);
			std::error_code xError;
			std::filesystem::remove_all(m_xDirectory, xError);
			Flux_PreviewSlotArbiter::ResetForTesting();
			LowerTheAnimationPreviewView();
		}

		AnimPreviewFixture(const AnimPreviewFixture&) = delete;
		AnimPreviewFixture& operator=(const AnimPreviewFixture&) = delete;

		// A clip that moves Spine from y=0 to y=1 across its whole duration, so any
		// change of playhead is visible in the skeleton instance's LOCAL pose.
		Flux_AnimationClip MakeClip(const char* szName, float fDuration, bool bRecordRig) const
		{
			Flux_AnimationClip xClip;
			xClip.SetName(szName);
			xClip.SetDuration(fDuration);
			xClip.SetLooping(true);
			if (bRecordRig)
			{
				xClip.GetMetadata().m_strSkeletonPath = m_strSkeletonPath;
				xClip.GetMetadata().m_strPreviewModelPath = m_strMeshPath;
			}

			Flux_BoneChannel xChannel;
			xChannel.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
			xChannel.AddPositionKeyframe(fDuration, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
			xChannel.SortKeyframes();
			xClip.AddBoneChannel("Spine", std::move(xChannel));
			return xClip;
		}
	};

	// The Spine bone's local position in a session's own skeleton instance — the
	// thing the clip above animates.
	Zenith_Maths::Vector3 AnimPreview_SpineLocal(const Zenith_AnimationPreviewSession& xSession)
	{
		const Flux_SkeletonInstance* pxInstance = xSession.GetSkeletonInstance();
		Zenith_Assert(pxInstance != nullptr, "AnimPreview_SpineLocal: the session has no rig");
		return pxInstance->GetBoneLocalPosition(1u);
	}

	// Event-callback sink: captureless fn-ptr + user data, per engine convention.
	struct AnimPreviewEventSink
	{
		u_int m_uCount = 0;
		std::string m_strLast;
	};

	void AnimPreview_OnEvent(void* pUserData, const std::string& strEventName, const Zenith_Maths::Vector4&)
	{
		AnimPreviewEventSink* pxSink = static_cast<AnimPreviewEventSink*>(pUserData);
		pxSink->m_uCount++;
		pxSink->m_strLast = strEventName;
	}
}

//------------------------------------------------------------------------------
// (1) Two previews are two clocks, and one controller has one driver per frame.
//------------------------------------------------------------------------------
ZENITH_TEST(AnimationPreview, SessionsTickIndependentlyAndDriveGuardRefusesASecondCaller)
{
	AnimPreviewFixture xFixture("zenith_animpreview_independent");

	Zenith_AnimationPreviewSession xSessionA("A");
	Zenith_AnimationPreviewSession xSessionB("B");

	const Flux_AnimationClip xClip = xFixture.MakeClip("Walk", 2.0f, true);
	ZENITH_ASSERT_TRUE(xSessionA.Open(xClip, "game:Anims/Walk.zanim") == ZENITH_ANIMPREVIEW_OPEN_OK,
		"session A should open with the clip's own recorded rig");
	ZENITH_ASSERT_TRUE(xSessionB.Open(xClip, "game:Anims/Walk.zanim") == ZENITH_ANIMPREVIEW_OPEN_OK,
		"session B should open too");

	// ★ SEPARATE CONTROLLERS AND SEPARATE SKELETON INSTANCES (D30). If the session
	// borrowed one, everything below would pass for the wrong reason.
	ZENITH_ASSERT_TRUE(&xSessionA.Controller() != &xSessionB.Controller(), "each session owns its controller");
	ZENITH_ASSERT_TRUE(xSessionA.GetSkeletonInstance() != xSessionB.GetSkeletonInstance(),
		"each session owns its skeleton instance");

	xSessionA.Tick(0.5f);
	ZENITH_ASSERT_EQ_FLOAT(xSessionA.GetTime(), 0.5f, 1e-4f, "A advanced by its own tick");
	ZENITH_ASSERT_EQ_FLOAT(xSessionB.GetTime(), 0.0f, 1e-4f, "B must NOT have advanced");

	xSessionB.Tick(0.25f);
	ZENITH_ASSERT_EQ_FLOAT(xSessionB.GetTime(), 0.25f, 1e-4f, "B advanced by its own tick");
	ZENITH_ASSERT_EQ_FLOAT(xSessionA.GetTime(), 0.5f, 1e-4f, "A must NOT have moved again");

	// A paused session ignores its own tick — the scrub is the only way to move it.
	xSessionA.Pause();
	xSessionA.Tick(1.0f);
	ZENITH_ASSERT_EQ_FLOAT(xSessionA.GetTime(), 0.5f, 1e-4f, "a paused session does not advance");

	//--------------------------------------------------------------------------
	// The drive guard, simulating the case it exists for: TWO panels aimed at ONE
	// controller inside ONE frame.
	//--------------------------------------------------------------------------
	Flux_AnimationController& xShared = xSessionA.Controller();
	const int iPanelOne = 0;
	const int iPanelTwo = 0;

	ZENITH_ASSERT_TRUE(xShared.TryBeginFrameDrive(&iPanelOne, 7ull), "the first caller in a frame drives");
	ZENITH_ASSERT_TRUE(xShared.GetFrameDriveOwner() == &iPanelOne, "the owner is recorded");
	ZENITH_ASSERT_FALSE(xShared.TryBeginFrameDrive(&iPanelTwo, 7ull), "a second caller in the SAME frame is refused");
	// ★ INCLUDING A REPEAT BY THE SAME DRIVER. A second tick is a second tick
	// whoever asks for it, so re-entry is refused as well.
	ZENITH_ASSERT_FALSE(xShared.TryBeginFrameDrive(&iPanelOne, 7ull), "even the same caller cannot drive twice");
	ZENITH_ASSERT_TRUE(xShared.GetFrameDriveOwner() == &iPanelOne, "a refused claim does not steal ownership");

	// A new frame token frees it again.
	ZENITH_ASSERT_TRUE(xShared.TryBeginFrameDrive(&iPanelTwo, 8ull), "the next frame is claimable");
	ZENITH_ASSERT_TRUE(xShared.GetFrameDriveOwner() == &iPanelTwo, "the new owner is recorded");

	// A release by someone who does NOT hold it changes nothing.
	xShared.ClearFrameDrive(&iPanelOne);
	ZENITH_ASSERT_TRUE(xShared.GetFrameDriveOwner() == &iPanelTwo, "only the owner may release");
	xShared.ClearFrameDrive(&iPanelTwo);
	ZENITH_ASSERT_TRUE(xShared.GetFrameDriveOwner() == nullptr, "the owner released it");
	ZENITH_ASSERT_TRUE(xShared.TryBeginFrameDrive(&iPanelOne, 8ull), "an early release re-opens the same frame");
}

//------------------------------------------------------------------------------
// (2) The layer short-circuit, and what the session and the new seek do about it.
//------------------------------------------------------------------------------
ZENITH_TEST(AnimationPreview, PreviewPlaysWhileAnotherControllerHasLayers)
{
	AnimPreviewFixture xFixture("zenith_animpreview_layers");

	Zenith_AnimationPreviewSession xSession("Preview");
	Flux_AnimationClip xClip = xFixture.MakeClip("Walk", 2.0f, true);
	ZENITH_ASSERT_TRUE(xSession.Open(xClip, "game:Anims/Walk.zanim") == ZENITH_ANIMPREVIEW_OPEN_OK, "session opens");
	ZENITH_ASSERT_EQ(xSession.Controller().GetLayerCount(), 0u, "the session's controller has NO layers");

	// A second, LAYERED controller on its own instance of the same rig — the shape
	// Flux_AnimationController::UpdateWithSkeletonInstance short-circuits on.
	Zenith_SkeletonAsset* pxSkeleton = Zenith_AssetRegistry::GetView<Zenith_SkeletonAsset>(xFixture.m_strSkeletonPath);
	ZENITH_ASSERT_NOT_NULL(pxSkeleton, "the fixture rig should load");
	Flux_SkeletonInstance* pxOtherInstance = Flux_SkeletonInstance::CreateFromAsset(pxSkeleton);
	ZENITH_ASSERT_NOT_NULL(pxOtherInstance, "a second skeleton instance should be creatable");

	{
		Flux_AnimationController xLayered;
		xLayered.Initialize(pxOtherInstance);
		xLayered.GetClipCollection().AddClipReference(&xClip);
		xLayered.PlayClip("Walk", 0.0f);
		xLayered.AddLayer("Base");

		// ★ THE SHORT-CIRCUIT, MEASURED: a controller with any layer never reaches
		// the direct-play path, so its playhead does not move on a tick.
		xLayered.Update(0.5f);
		ZENITH_ASSERT_EQ_FLOAT(xLayered.GetDirectPlayTime(), 0.0f, 1e-4f,
			"a layered controller's tick does NOT advance direct play");

		// ...and the session, which has no layers, plays normally.
		const Zenith_Maths::Vector3 xBefore = AnimPreview_SpineLocal(xSession);
		xSession.Tick(0.5f);
		const Zenith_Maths::Vector3 xAfter = AnimPreview_SpineLocal(xSession);
		ZENITH_ASSERT_EQ_FLOAT(xSession.GetTime(), 0.5f, 1e-4f, "the session's playhead advanced");
		ZENITH_ASSERT_TRUE(glm::length(xAfter - xBefore) > 0.01f,
			"the session's skeleton instance pose changes with time");

		// ★ AND THE SEEK WORKS ANYWAY. SeekDirectPlay drives the direct-play node
		// itself rather than going through the dispatcher, so a scrub is available
		// on a layered controller too — which is the whole reason it is a separate
		// entry point rather than "Update with a negative dt".
		ZENITH_ASSERT_TRUE(xLayered.SeekDirectPlay(0.75f), "a layered controller can still be scrubbed");
		ZENITH_ASSERT_EQ_FLOAT(xLayered.GetDirectPlayTime(), 0.75f, 1e-4f, "the scrub landed");

		xLayered.Stop();
		xLayered.GetClipCollection().Clear();
	}

	pxOtherInstance->Destroy();
	delete pxOtherInstance;
}

//------------------------------------------------------------------------------
// (3) Scrubbing to t and playing to t leave the SAME pose.
//------------------------------------------------------------------------------
ZENITH_TEST(AnimationPreview, SeekProducesTheSamePoseAsPlayingToThatTime)
{
	AnimPreviewFixture xFixture("zenith_animpreview_seek");

	Zenith_AnimationPreviewSession xPlayed("Played");
	Zenith_AnimationPreviewSession xScrubbed("Scrubbed");

	const Flux_AnimationClip xClip = xFixture.MakeClip("Walk", 2.0f, true);
	ZENITH_ASSERT_TRUE(xPlayed.Open(xClip, "game:Anims/Walk.zanim") == ZENITH_ANIMPREVIEW_OPEN_OK, "played opens");
	ZENITH_ASSERT_TRUE(xScrubbed.Open(xClip, "game:Anims/Walk.zanim") == ZENITH_ANIMPREVIEW_OPEN_OK, "scrubbed opens");

	// One tick to an awkward time, and a scrub straight to it.
	xPlayed.Tick(0.37f);
	ZENITH_ASSERT_TRUE(xScrubbed.Seek(0.37f), "the scrub should succeed");
	ZENITH_ASSERT_EQ_FLOAT(xPlayed.GetTime(), xScrubbed.GetTime(), 1e-5f, "both playheads sit at the same time");

	const Flux_SkeletonInstance* pxPlayed = xPlayed.GetSkeletonInstance();
	const Flux_SkeletonInstance* pxScrubbed = xScrubbed.GetSkeletonInstance();
	ZENITH_ASSERT_EQ(pxPlayed->GetNumBones(), pxScrubbed->GetNumBones(), "same rig, same bone count");
	for (uint32_t u = 0; u < pxPlayed->GetNumBones(); ++u)
	{
		ZENITH_ASSERT_NEAR_VEC3(pxPlayed->GetBoneLocalPosition(u), pxScrubbed->GetBoneLocalPosition(u), 1e-4f,
			"a scrubbed bone position must match the played one");
		ZENITH_ASSERT_NEAR_VEC3(pxPlayed->GetBoneLocalScale(u), pxScrubbed->GetBoneLocalScale(u), 1e-4f,
			"a scrubbed bone scale must match the played one");
		const Zenith_Maths::Quat xA = pxPlayed->GetBoneLocalRotation(u);
		const Zenith_Maths::Quat xB = pxScrubbed->GetBoneLocalRotation(u);
		// Compare on the shorter arc: q and -q are the same rotation.
		const float fDot = glm::abs(glm::dot(xA, xB));
		ZENITH_ASSERT_EQ_FLOAT(fDot, 1.0f, 1e-4f, "a scrubbed bone rotation must match the played one");
	}

	// Several small ticks to a time, against one scrub to it — the accumulation
	// path a real preview actually takes.
	xPlayed.Tick(0.1f);
	xPlayed.Tick(0.1f);
	xPlayed.Tick(0.1f);
	ZENITH_ASSERT_TRUE(xScrubbed.Seek(xPlayed.GetTime()), "the scrub should succeed");
	for (uint32_t u = 0; u < pxPlayed->GetNumBones(); ++u)
	{
		ZENITH_ASSERT_NEAR_VEC3(pxPlayed->GetBoneLocalPosition(u), pxScrubbed->GetBoneLocalPosition(u), 1e-4f,
			"an accumulated playhead and a scrub to it must agree");
	}

	// A scrub past the end of a LOOPING clip wraps rather than clamping, matching
	// what the tick does — a slider must not be able to park the playhead outside
	// the clip.
	ZENITH_ASSERT_TRUE(xScrubbed.Seek(2.5f), "an out-of-range scrub is folded, not refused");
	ZENITH_ASSERT_EQ_FLOAT(xScrubbed.GetTime(), 0.5f, 1e-4f, "a looping clip wraps");
	ZENITH_ASSERT_TRUE(xScrubbed.Seek(-0.5f), "a negative scrub is folded too");
	ZENITH_ASSERT_EQ_FLOAT(xScrubbed.GetTime(), 1.5f, 1e-4f, "a negative time wraps to the tail");

	// ...and clamps for a clip that does not loop.
	xScrubbed.SetLooping(false);
	ZENITH_ASSERT_TRUE(xScrubbed.Seek(9.0f), "a non-looping scrub still succeeds");
	ZENITH_ASSERT_EQ_FLOAT(xScrubbed.GetTime(), 2.0f, 1e-4f, "a non-looping clip clamps at its duration");
}

//------------------------------------------------------------------------------
// (4a) A staged frame RAISES the session's own view, and slot 5 is not its
// business.
//
// ★ THE SLOT-5 HALF IS THE POINT OF THE UNIT. Before D3 this session drove the
// material editor's view, and every symptom of that was invisible to a test that
// only looked at "is a preview view up": the animation editor opening blanked the
// material editor's preview, and the two took turns filling one payload. Recording
// slot 5 before and after is what makes "it moved onto its own slot" a checkable
// fact rather than a claim in a comment.
//------------------------------------------------------------------------------
ZENITH_TEST(AnimationPreview, AStagedFrameRaisesSlotSixAndNeverTouchesSlotFive)
{
	AnimPreviewFixture xFixture("zenith_animpreview_slotsix");

	Flux_RenderViewRegistry* pxViews = AnimPreview_Views();
	ZENITH_ASSERT_NOT_NULL(pxViews, "the unit batch runs with a live Flux_GraphicsImpl — the registry is CPU-only");
	if (pxViews == nullptr)
	{
		return;
	}

	// ★ A GUARD ON THIS TEST'S OWN PREMISE. Everything below assumes slot 6 is a
	// full-pipeline PREVIEW view the registry constructed for this purpose; if a
	// later slot-layout change moved that, the assertions would still pass while
	// measuring the wrong view.
	ZENITH_ASSERT_TRUE(pxViews->View(kuFluxViewSlotPreviewAnim).m_bFullPipeline,
		"slot 6 is a full-pipeline view");
	ZENITH_ASSERT_TRUE(pxViews->View(kuFluxViewSlotPreviewAnim).m_eType == FLUX_RENDER_VIEW_PREVIEW,
		"and a PREVIEW-typed one");

	const bool bMaterialBefore = pxViews->IsViewActive(kuFluxViewSlotPreviewMaterial);
	ZENITH_ASSERT_FALSE(pxViews->IsViewActive(kuFluxViewSlotPreviewAnim),
		"the fixture starts every test with the animation view down");

	Zenith_AnimationPreviewSession xSession("Walk.zanim");
	const Flux_AnimationClip xClip = xFixture.MakeClip("Walk", 2.0f, true);
	ZENITH_ASSERT_TRUE(xSession.Open(xClip, "game:Anims/Walk.zanim") == ZENITH_ANIMPREVIEW_OPEN_OK, "session opens");

	// ★ OPENING IS NOT A FRAME. The old session CLAIMED on open, which is why this
	// used to be the moment the other editor's preview went dark.
	ZENITH_ASSERT_FALSE(pxViews->IsViewActive(kuFluxViewSlotPreviewAnim), "Open() alone raises nothing");

	xSession.UpdatePreviewView();
	ZENITH_ASSERT_TRUE(pxViews->IsViewActive(kuFluxViewSlotPreviewAnim), "the first staged frame raises slot 6");
	ZENITH_ASSERT_TRUE(pxViews->IsViewActive(kuFluxViewSlotPreviewMaterial) == bMaterialBefore,
		"and slot 5's active flag is exactly where the session found it");

	// The payload names its OWN slot: every per-view consumer reads m_uViewSlot
	// back out to find its resources.
	const Flux_RenderView& xAnimView = pxViews->View(kuFluxViewSlotPreviewAnim);
	ZENITH_ASSERT_EQ(xAnimView.m_xConstants.m_uViewSlot, kuFluxViewSlotPreviewAnim,
		"the staged constants carry the ANIMATION slot");
	ZENITH_ASSERT_EQ(xAnimView.m_xTargetDims.x, kuFLUX_PREVIEW_VIEW_SIZE, "staged at the preview view size");
	ZENITH_ASSERT_EQ(xAnimView.m_xTargetDims.y, kuFLUX_PREVIEW_VIEW_SIZE, "square");

	// A second staged frame is not a second edge — it just refills the payload.
	xSession.UpdatePreviewView();
	ZENITH_ASSERT_FALSE(pxViews->SetViewActive(kuFluxViewSlotPreviewAnim, true),
		"the view is already up, so raising it again changes no active set");
}

//------------------------------------------------------------------------------
// (4b) Closing LOWERS it, and so does losing the rig.
//
// ★ NOTHING ELSE IN THE ENGINE CAN. The material preview's janitor
// (Flux_MaterialPreviewController::Update, run every frame from the GPU-scene
// sync) lowers slot 5 and only slot 5, and the panel stops calling the session the
// moment the clip closes — so if this path is wrong the symptom is a preview view
// rendering for the rest of the process with nothing looking at it, which no
// existing assertion anywhere would notice.
//------------------------------------------------------------------------------
ZENITH_TEST(AnimationPreview, CloseAndAnUnresolvedRigBothLowerSlotSix)
{
	AnimPreviewFixture xFixture("zenith_animpreview_slotsixdown");

	Flux_RenderViewRegistry* pxViews = AnimPreview_Views();
	ZENITH_ASSERT_NOT_NULL(pxViews, "the unit batch runs with a live Flux_GraphicsImpl");
	if (pxViews == nullptr)
	{
		return;
	}

	Zenith_AnimationPreviewSession xSession("Walk.zanim");
	const Flux_AnimationClip xClip = xFixture.MakeClip("Walk", 2.0f, true);

	ZENITH_ASSERT_TRUE(xSession.Open(xClip, "game:Anims/Walk.zanim") == ZENITH_ANIMPREVIEW_OPEN_OK, "session opens");
	xSession.UpdatePreviewView();
	ZENITH_ASSERT_TRUE(pxViews->IsViewActive(kuFluxViewSlotPreviewAnim), "raised");

	xSession.Close();
	ZENITH_ASSERT_FALSE(pxViews->IsViewActive(kuFluxViewSlotPreviewAnim), "Close lowers it");

	// Re-opening is a fresh rising edge — the state is per-frame, not per-object.
	ZENITH_ASSERT_TRUE(xSession.Open(xClip, "game:Anims/Walk.zanim") == ZENITH_ANIMPREVIEW_OPEN_OK, "reopen");
	xSession.UpdatePreviewView();
	ZENITH_ASSERT_TRUE(pxViews->IsViewActive(kuFluxViewSlotPreviewAnim), "raised again");

	// ★ AND A RIG THAT STOPS RESOLVING LOWERS IT TOO, which is the path the PANEL
	// cannot cover: RenderPreviewPane returns at its rig prompt without ever
	// reaching UpdatePreviewView, so a session left rigless with the view up would
	// never get another chance to lower it.
	ZENITH_ASSERT_FALSE(xSession.SetRigOverride("", ""), "an empty rig does not resolve");
	ZENITH_ASSERT_TRUE(xSession.NeedsRigSelection(), "the session is asking for a rig");
	ZENITH_ASSERT_FALSE(pxViews->IsViewActive(kuFluxViewSlotPreviewAnim),
		"and the view went down with the rig — there is no pose left to draw");

	// A staged frame on a rigless session must not put it back up.
	xSession.UpdatePreviewView();
	ZENITH_ASSERT_FALSE(pxViews->IsViewActive(kuFluxViewSlotPreviewAnim),
		"staging a session with no rig lowers rather than raises");
}

//------------------------------------------------------------------------------
// (4c) The panel's hidden-frame call: idempotent in both directions.
//
// The panel drives this from ABOVE its early returns (Zenith_EditorPanel_Animation
// ::Render, both the !m_bShow return and the collapsed/unselected-tab one), where
// it has no idea whether it was hidden last frame too — so "lower it" has to be
// safe to say on every frame, and "show it again" has to work afterwards.
//
// ★ NOT DRIVEN THROUGH THE PANEL FIXTURE. That fixture lives in
// Zenith_EditorPanel_Animation.Tests.inl, which is included into a DIFFERENT TU;
// nothing here can reach it. The session-level call is the whole of what the panel
// contributes at those two sites, so this is the same fact one layer down.
//------------------------------------------------------------------------------
ZENITH_TEST(AnimationPreview, DeactivateIsIdempotentAndAStagedFrameRecovers)
{
	AnimPreviewFixture xFixture("zenith_animpreview_slotsixhidden");

	Flux_RenderViewRegistry* pxViews = AnimPreview_Views();
	ZENITH_ASSERT_NOT_NULL(pxViews, "the unit batch runs with a live Flux_GraphicsImpl");
	if (pxViews == nullptr)
	{
		return;
	}

	Zenith_AnimationPreviewSession xSession("Walk.zanim");
	const Flux_AnimationClip xClip = xFixture.MakeClip("Walk", 2.0f, true);
	ZENITH_ASSERT_TRUE(xSession.Open(xClip, "game:Anims/Walk.zanim") == ZENITH_ANIMPREVIEW_OPEN_OK, "session opens");

	// A deactivate before anything was ever raised is legal and changes nothing —
	// the panel's first hidden frame may well precede its first visible one.
	xSession.DeactivatePreviewView();
	ZENITH_ASSERT_FALSE(pxViews->IsViewActive(kuFluxViewSlotPreviewAnim), "still down");

	xSession.UpdatePreviewView();
	ZENITH_ASSERT_TRUE(pxViews->IsViewActive(kuFluxViewSlotPreviewAnim), "a visible frame raises it");

	xSession.DeactivatePreviewView();
	ZENITH_ASSERT_FALSE(pxViews->IsViewActive(kuFluxViewSlotPreviewAnim), "the panel hid: down on the falling edge");
	xSession.DeactivatePreviewView();
	xSession.DeactivatePreviewView();
	ZENITH_ASSERT_FALSE(pxViews->SetViewActive(kuFluxViewSlotPreviewAnim, false),
		"and every hidden frame after it is a no-op — there is no edge left to spend");

	// The session is still open all along, so becoming visible again just works.
	ZENITH_ASSERT_TRUE(xSession.IsOpen(), "hiding a panel does not close its clip");
	xSession.UpdatePreviewView();
	ZENITH_ASSERT_TRUE(pxViews->IsViewActive(kuFluxViewSlotPreviewAnim), "and the next visible frame puts it back");
}

//------------------------------------------------------------------------------
// (4c') ONLY THE RAISER LOWERS. A session that never raised slot 6 must leave it
// exactly as it found it: the panel calls DeactivatePreviewView() on every hidden
// frame, and an animation panel with nothing open is hidden in every editor frame
// of every game -- the render-graph oracle's sample C raises slot 6 itself and
// was lowered by that call once.
//------------------------------------------------------------------------------
ZENITH_TEST(AnimationPreview, ASessionThatNeverRaisedTheViewNeverLowersIt)
{
	Flux_RenderViewRegistry* pxViews = AnimPreview_Views();
	ZENITH_ASSERT_NOT_NULL(pxViews, "the unit batch runs with a live Flux_GraphicsImpl");
	if (pxViews == nullptr)
	{
		return;
	}
	const bool bWasActive = pxViews->IsViewActive(kuFluxViewSlotPreviewAnim);
	pxViews->SetViewActive(kuFluxViewSlotPreviewAnim, true);

	{
		Zenith_AnimationPreviewSession xSession("Nothing.zanim");
		xSession.DeactivatePreviewView();
		ZENITH_ASSERT_TRUE(pxViews->IsViewActive(kuFluxViewSlotPreviewAnim),
			"a session that never raised the view leaves somebody else's activation alone");
	}
	ZENITH_ASSERT_TRUE(pxViews->IsViewActive(kuFluxViewSlotPreviewAnim),
		"and destroying it (Close on a never-opened session) leaves it alone too");

	pxViews->SetViewActive(kuFluxViewSlotPreviewAnim, bWasActive);
}

//------------------------------------------------------------------------------
// (4d) TWO PREVIEWS, TWO PAYLOADS. The property the whole slot split exists for.
//
// Slot 5's ViewConstants are hand-staged here with a camera nothing else would
// produce, and must come back byte-identical after the session has run a frame.
//
// ★ Flux_MaterialPreviewController::Update() IS DELIBERATELY NOT RUN. It needs
// procedural mesh assets, a material table and the external-item submission seam —
// device work this batch does not do — so the material editor's half of the
// property is represented by the payload it would have staged, not by the call
// that stages it. What is being measured is that the SESSION does not write there,
// and a hand-staged payload measures that exactly.
//------------------------------------------------------------------------------
ZENITH_TEST(AnimationPreview, EachPreviewStagesItsOwnConstants)
{
	AnimPreviewFixture xFixture("zenith_animpreview_ownconstants");

	Flux_RenderViewRegistry* pxViews = AnimPreview_Views();
	ZENITH_ASSERT_NOT_NULL(pxViews, "the unit batch runs with a live Flux_GraphicsImpl");
	if (pxViews == nullptr)
	{
		return;
	}

	// Slot 5 belongs to the material editor, so whatever it holds is put back on
	// the way out — this unit is a guest there.
	Flux_RenderView& xMaterialView = pxViews->View(kuFluxViewSlotPreviewMaterial);
	const Flux_ViewConstants xMaterialConstantsOnEntry = xMaterialView.m_xConstants;
	const Zenith_Maths::UVector2 xMaterialDimsOnEntry = xMaterialView.m_xTargetDims;

	// A camera the session's own defaults could never produce, so "unchanged" and
	// "overwritten with something similar" cannot be confused.
	Flux_PreviewBuildViewConstants(-2.9f, -1.1f, 5.75f, xMaterialView.m_xConstants);
	xMaterialView.m_xConstants.m_uViewSlot = kuFluxViewSlotPreviewMaterial;
	xMaterialView.m_xTargetDims = Zenith_Maths::UVector2(64u, 32u);

	// ★ THE SNAPSHOT IS TAKEN WITH memcpy, NOT WITH A COPY-CONSTRUCTION. An implicit
	// copy is member-wise and says nothing about the struct's padding bytes, so a
	// byte comparison against a copied object could report a difference nothing
	// wrote. memcpy takes the object representation, which is what "byte-unchanged"
	// has to mean.
	unsigned char aucMaterialBytes[sizeof(Flux_ViewConstants)];
	std::memcpy(aucMaterialBytes, &xMaterialView.m_xConstants, sizeof(aucMaterialBytes));

	{
		Zenith_AnimationPreviewSession xSession("Walk.zanim");
		const Flux_AnimationClip xClip = xFixture.MakeClip("Walk", 2.0f, true);
		ZENITH_ASSERT_TRUE(xSession.Open(xClip, "game:Anims/Walk.zanim") == ZENITH_ANIMPREVIEW_OPEN_OK, "session opens");

		// Orbit somewhere of its own, so the two payloads genuinely differ.
		xSession.OrbitCamera(1.3f, 0.2f);
		xSession.ZoomCamera(-4.0f);
		xSession.UpdatePreviewView();

		// ★ SLOT 5 IS BYTE-UNCHANGED. A field-by-field comparison would only cover
		// the fields somebody remembered to name; the payload is a standard-layout
		// block of spine constants, so the whole thing is comparable at once.
		ZENITH_ASSERT_EQ(std::memcmp(&xMaterialView.m_xConstants, aucMaterialBytes, sizeof(aucMaterialBytes)), 0,
			"the animation session wrote nothing into the material preview's constants");
		ZENITH_ASSERT_EQ(xMaterialView.m_xTargetDims.x, 64u, "nor into its target dims");
		ZENITH_ASSERT_EQ(xMaterialView.m_xTargetDims.y, 32u, "either half");

		const Flux_RenderView& xAnimView = pxViews->View(kuFluxViewSlotPreviewAnim);
		ZENITH_ASSERT_EQ(xAnimView.m_xConstants.m_uViewSlot, kuFluxViewSlotPreviewAnim,
			"and slot 6 carries its own slot number");
		ZENITH_ASSERT_EQ(xAnimView.m_xTargetDims.x, kuFLUX_PREVIEW_VIEW_SIZE, "at the preview view size");
		ZENITH_ASSERT_TRUE(
			std::memcmp(&xAnimView.m_xConstants, aucMaterialBytes, sizeof(aucMaterialBytes)) != 0,
			"the two payloads are genuinely different, so the comparison above means something");

		// The session's camera is the one the pure builder makes from ITS orbit.
		float fYaw = 0.0f;
		float fPitch = 0.0f;
		float fDistance = 0.0f;
		xSession.GetCameraOrbit(fYaw, fPitch, fDistance);
		Flux_ViewConstants xExpectedAnim;
		Flux_PreviewBuildViewConstants(fYaw, fPitch, fDistance, xExpectedAnim);
		ZENITH_ASSERT_NEAR_VEC3(Zenith_Maths::Vector3(xAnimView.m_xConstants.m_xCamPos_Pad),
			Zenith_Maths::Vector3(xExpectedAnim.m_xCamPos_Pad), 1e-4f,
			"slot 6 holds the SESSION's orbit camera");
	}

	xMaterialView.m_xConstants = xMaterialConstantsOnEntry;
	xMaterialView.m_xTargetDims = xMaterialDimsOnEntry;
}

//------------------------------------------------------------------------------
// (4e) The arbiter, which the session is NO LONGER A CLAIMANT OF.
//
// ★ THIS TEST IS IN THE WRONG FILE AND IS HERE ON PURPOSE. Its subject is
// Flux_PreviewSlotArbiter plus Flux_MaterialPreviewController::SetActive, and it
// belongs beside the orbit maths in Flux_MaterialPreviewController.Tests.inl.
// D3 does not own that file, and the arbiter's ONLY coverage was the two session
// tests D3 deletes — so it moves here rather than evaporating. Relocating it is a
// one-file follow-up with no behaviour in it.
//
// The rising-edge rule is what the coverage is FOR: SetActive(true) runs every
// frame the material panel is visible, and a claim per frame would make the
// material editor impossible to dispossess — last-opened-wins would quietly become
// last-drawn-wins. With one real claimant left, the second one here is a bare
// address: the arbiter is owner-agnostic by construction and never dereferences
// the identity it stores.
//------------------------------------------------------------------------------
ZENITH_TEST(AnimationPreview, TheArbiterClaimsOnTheRisingEdgeOnlyAndTheSessionIsNotAClaimant)
{
	AnimPreviewFixture xFixture("zenith_animpreview_arbiter");

	ZENITH_ASSERT_TRUE(Flux_PreviewSlotArbiter::GetOwner() == nullptr, "the fixture reset leaves nobody owning it");
	ZENITH_ASSERT_TRUE(Flux_PreviewSlotArbiter::GetOwnerName().empty(), "and no owner name");

	// ★ OPENING AND STAGING A SESSION IS NOT A CLAIM ANY MORE. This is the whole of
	// R5 in one assertion: the animation editor left the arbitration, so a session
	// running a frame cannot dispossess the material editor.
	{
		Zenith_AnimationPreviewSession xSession("Walk.zanim");
		const Flux_AnimationClip xClip = xFixture.MakeClip("Walk", 2.0f, true);
		ZENITH_ASSERT_TRUE(xSession.Open(xClip, "game:Anims/Walk.zanim") == ZENITH_ANIMPREVIEW_OPEN_OK, "session opens");
		xSession.UpdatePreviewView();
		ZENITH_ASSERT_TRUE(Flux_PreviewSlotArbiter::GetOwner() == nullptr,
			"a session that opened AND staged a frame still owns nothing");
	}

	Flux_MaterialPreviewController xMaterialPreview;
	xMaterialPreview.SetActive(true);
	ZENITH_ASSERT_TRUE(xMaterialPreview.HasPreviewSlot(), "the material editor opening claims the slot");
	ZENITH_ASSERT_TRUE(Flux_PreviewSlotArbiter::GetOwnerName() == "Material Editor", "under its own name");

	// The second claimant: a bare address standing in for whatever claims the
	// material slot next (the --preview-test-view diagnostic claims exactly like
	// this, under its own name). The arbiter stores an IDENTITY it never
	// dereferences, so a local is a complete claimant.
	const int iOtherClaimant = 0;

	// ★ ACTIVE AND DISPOSSESSED ARE STILL DIFFERENT THINGS. The DP automation
	// asserts IsActive() stays true for the whole time the panel is open, so
	// arbitration must not touch the liveness flag.
	ZENITH_ASSERT_TRUE(Flux_PreviewSlotArbiter::Claim(&iOtherClaimant, "Preview Test View"), "the owner changed");
	ZENITH_ASSERT_FALSE(xMaterialPreview.HasPreviewSlot(), "the material editor is dispossessed");
	ZENITH_ASSERT_TRUE(xMaterialPreview.IsActive(), "but its panel is still open");
	ZENITH_ASSERT_TRUE(xMaterialPreview.GetPreviewSlotOwnerName() == "Preview Test View", "and it is told who took it");

	// THE RISING EDGE IS THE ONLY EDGE. These are the per-frame liveness refreshes.
	xMaterialPreview.SetActive(true);
	xMaterialPreview.SetActive(true);
	ZENITH_ASSERT_FALSE(xMaterialPreview.HasPreviewSlot(), "a liveness refresh is not a claim");

	// A dispossessed claimant closing must not free somebody else's slot.
	xMaterialPreview.SetActive(false);
	ZENITH_ASSERT_TRUE(Flux_PreviewSlotArbiter::GetOwner() == &iOtherClaimant, "a dispossessed close frees nothing");

	// ...and re-opening IS a rising edge, so it wins.
	xMaterialPreview.SetActive(true);
	ZENITH_ASSERT_TRUE(xMaterialPreview.HasPreviewSlot(), "reopening claims again");

	// The falling edge of the OWNER releases.
	xMaterialPreview.SetActive(false);
	ZENITH_ASSERT_TRUE(Flux_PreviewSlotArbiter::GetOwner() == nullptr, "the owner's close frees the slot");
	ZENITH_ASSERT_TRUE(Flux_PreviewSlotArbiter::GetOwnerName().empty(), "and clears the owner name");

	// The reset is what every fixture in the suite leans on.
	Flux_PreviewSlotArbiter::Claim(&iOtherClaimant, "Leftover");
	Flux_PreviewSlotArbiter::ResetForTesting();
	ZENITH_ASSERT_TRUE(Flux_PreviewSlotArbiter::GetOwner() == nullptr, "ResetForTesting clears the owner");
	ZENITH_ASSERT_TRUE(Flux_PreviewSlotArbiter::GetOwnerName().empty(), "and the name with it");
}

//------------------------------------------------------------------------------
// (5) A clip with no rig asks for one, and the answer is remembered per clip.
//------------------------------------------------------------------------------
ZENITH_TEST(AnimationPreview, MissingRigNeedsSelectionAndTheChoiceIsRemembered)
{
	AnimPreviewFixture xFixture("zenith_animpreview_rig");
	Zenith_EditorPrefs xPrefs;

	const std::string strClipPath = "game:Anims/Unrigged.zanim";
	const Flux_AnimationClip xClip = xFixture.MakeClip("Unrigged", 2.0f, /*bRecordRig*/ false);

	{
		Zenith_AnimationPreviewSession xSession("Unrigged");
		xSession.SetPreferenceStore(&xPrefs);

		ZENITH_ASSERT_TRUE(xSession.Open(xClip, strClipPath) == ZENITH_ANIMPREVIEW_OPEN_NEEDS_RIG,
			"a clip with no recorded skeleton opens but needs a rig");
		ZENITH_ASSERT_TRUE(xSession.NeedsRigSelection(), "NeedsRigSelection is what the panel asks");
		ZENITH_ASSERT_TRUE(xSession.GetRigStatus() == ZENITH_ANIMPREVIEW_RIG_NO_SKELETON_PATH,
			"and it says WHICH half is missing");
		// It is still OPEN: the track list and the duration are usable without a rig.
		ZENITH_ASSERT_TRUE(xSession.IsOpen(), "no rig is not a failed open");
		ZENITH_ASSERT_EQ_FLOAT(xSession.GetDuration(), 2.0f, 1e-5f, "the clip is loaded regardless");
		ZENITH_ASSERT_FALSE(xSession.Seek(1.0f), "playback is inert until a rig resolves");

		// A skeleton that does not resolve is a DIFFERENT answer from no skeleton.
		ZENITH_ASSERT_FALSE(xSession.SetRigOverride("game:NoSuchRig.zskel", xFixture.m_strMeshPath),
			"an unresolvable skeleton is refused");
		ZENITH_ASSERT_TRUE(xSession.GetRigStatus() == ZENITH_ANIMPREVIEW_RIG_SKELETON_UNRESOLVED,
			"and says so specifically");
		ZENITH_ASSERT_EQ(xPrefs.GetAnimRigChoiceCount(), 0u, "a refused choice is NOT remembered");

		// A skeleton with no preview mesh is the third answer.
		ZENITH_ASSERT_FALSE(xSession.SetRigOverride(xFixture.m_strSkeletonPath, ""),
			"a rig with no preview mesh is not yet complete");
		ZENITH_ASSERT_TRUE(xSession.GetRigStatus() == ZENITH_ANIMPREVIEW_RIG_NO_MODEL_PATH, "named precisely");

		// ★ THE .zasset CASE. The fixture's preview path is a bare skinned mesh, not
		// a .zmodel bundle — exactly what the generated tree/bush sway clips record.
		ZENITH_ASSERT_TRUE(xSession.SetRigOverride(xFixture.m_strSkeletonPath, xFixture.m_strMeshPath),
			"a .zasset mesh is an acceptable preview subject");
		ZENITH_ASSERT_FALSE(xSession.NeedsRigSelection(), "the session is fully open now");
		ZENITH_ASSERT_TRUE(xSession.IsPreviewMeshBareMeshAsset(), "and knows it is a bare mesh, not a model");
		ZENITH_ASSERT_NOT_NULL(xSession.GetSkeletonInstance(), "a rig means a skeleton instance");
		ZENITH_ASSERT_TRUE(xSession.Seek(1.0f), "and playback works");

		ZENITH_ASSERT_EQ(xPrefs.GetAnimRigChoiceCount(), 1u, "the accepted choice is remembered");
	}

	// ★ REMEMBERED PER CLIP, AND IT SURVIVES THE PREFS FILE. Tested through
	// Serialize/Parse directly: Load() returns false and Save() no-ops in a
	// headless run, so a round trip through the file would prove nothing.
	Zenith_EditorPrefs xReloaded;
	xReloaded.Parse(xPrefs.Serialize());
	Zenith_EditorPrefs_AnimRigChoice xChoice;
	// The session keys on the NORMALIZED clip path, so the lookup has to normalize too.
	const std::string strKey = Zenith_AssetRegistry::NormalizeAssetPath(strClipPath);
	ZENITH_ASSERT_TRUE(xReloaded.TryGetAnimRigChoice(strKey, xChoice), "the choice survives the round trip");
	ZENITH_ASSERT_TRUE(xChoice.m_strSkeletonPath == xFixture.m_strSkeletonPath, "skeleton half survived");
	ZENITH_ASSERT_TRUE(xChoice.m_strPreviewModelPath == xFixture.m_strMeshPath, "model half survived");

	// A NEW session on the same clip opens straight away from the reloaded prefs —
	// the point of remembering it at all.
	{
		Zenith_AnimationPreviewSession xReopened("Unrigged");
		xReopened.SetPreferenceStore(&xReloaded);
		ZENITH_ASSERT_TRUE(xReopened.Open(xClip, strClipPath) == ZENITH_ANIMPREVIEW_OPEN_OK,
			"the remembered rig is applied on open, with no second prompt");
		ZENITH_ASSERT_FALSE(xReopened.NeedsRigSelection(), "no prompt needed");
	}

	// A session with NO preference store still works — it just remembers nothing.
	{
		Zenith_AnimationPreviewSession xUnstored("Unrigged");
		ZENITH_ASSERT_TRUE(xUnstored.Open(xClip, strClipPath) == ZENITH_ANIMPREVIEW_OPEN_NEEDS_RIG,
			"without a store there is nothing to remember");
		ZENITH_ASSERT_TRUE(xUnstored.SetRigOverride(xFixture.m_strSkeletonPath, xFixture.m_strMeshPath),
			"and the override still applies");
	}
}

//------------------------------------------------------------------------------
// (6) A seek moves the event bookkeeping mark (D40).
//------------------------------------------------------------------------------
ZENITH_TEST(AnimationPreview, SeekAdvancesTheEventBookkeepingMark)
{
	AnimPreviewFixture xFixture("zenith_animpreview_events");

	Flux_AnimationClip xClip = xFixture.MakeClip("Walk", 2.0f, true);
	{
		Flux_AnimationEvent xEarly;
		xEarly.m_fNormalizedTime = 0.1f;
		xEarly.m_strEventName = "Early";
		xClip.AddEvent(xEarly);

		Flux_AnimationEvent xLate;
		xLate.m_fNormalizedTime = 0.4f;
		xLate.m_strEventName = "Late";
		xClip.AddEvent(xLate);
	}

	Zenith_AnimationPreviewSession xSession("Events");
	ZENITH_ASSERT_TRUE(xSession.Open(xClip, "game:Anims/Walk.zanim") == ZENITH_ANIMPREVIEW_OPEN_OK, "session opens");

	AnimPreviewEventSink xSink;
	xSession.Controller().SetEventCallback(&AnimPreview_OnEvent, &xSink);

	ZENITH_ASSERT_FALSE(xSession.Controller().GetEmitEventsOnSeek(),
		"a scrub emits nothing by default — WU-5A owns the policy, this is only the hook");
	ZENITH_ASSERT_EQ_FLOAT(xSession.Controller().GetLastEventCheckTime(), 0.0f, 1e-5f, "the mark starts at zero");

	// Scrub straight past the Early event to halfway.
	ZENITH_ASSERT_TRUE(xSession.Seek(1.0f), "scrub to the middle of the clip");
	ZENITH_ASSERT_EQ(xSink.m_uCount, 0u, "the scrub emitted nothing");
	// ★ THE MARK MOVED ANYWAY. 1.0s of a 2s clip is normalized 0.5.
	ZENITH_ASSERT_EQ_FLOAT(xSession.Controller().GetLastEventCheckTime(), 0.5f, 1e-4f,
		"the seek advanced the bookkeeping mark");

	// ★ AND THAT IS WHY. A forward tick from here scans only 0.5 -> 0.55; with the
	// mark left at zero it would have scanned 0 -> 0.55 and fired BOTH skipped
	// events at once, which on a real clip is a burst of footsteps at the moment a
	// user releases the scrubber.
	xSession.Tick(0.1f);
	ZENITH_ASSERT_EQ(xSink.m_uCount, 0u, "no skipped event is replayed after the scrub");

	// Scrub BACK behind both events, then play forward over the Early one only:
	// the mark has to work in that direction too.
	ZENITH_ASSERT_TRUE(xSession.Seek(0.0f), "scrub back to the start");
	ZENITH_ASSERT_EQ_FLOAT(xSession.Controller().GetLastEventCheckTime(), 0.0f, 1e-4f, "the mark followed it back");
	xSession.Tick(0.5f);   // 0.0 -> 0.5s == normalized 0 -> 0.25: crosses Early (0.1), not Late (0.4)
	ZENITH_ASSERT_EQ(xSink.m_uCount, 1u, "exactly the event the playhead crossed");
	ZENITH_ASSERT_TRUE(xSink.m_strLast == "Early", "and it was the right one");

	xSession.Controller().ClearEventCallback();
}

//------------------------------------------------------------------------------
// Pose authoring (WU-4.1) — bone selection, pick geometry, the live drag.
//
// Still CPU-only and still headless: the pick set is built from the skeleton
// instance's model-space cache and raycast on the CPU, and the drag writes
// through Flux_SkeletonInstance, which owns no device resources.
//------------------------------------------------------------------------------

namespace
{
	// The capsule bone uOwner owns, whichever child it runs to. Null when the
	// bone owns none (a leaf, or a bone whose children sit on top of it).
	const Zenith_BonePickShape* AnimPreview_FindCapsule(const Zenith_BonePickSet& xSet, u_int uOwner)
	{
		for (u_int u = 0; u < xSet.m_xShapes.GetSize(); ++u)
		{
			const Zenith_BonePickShape& xShape = xSet.m_xShapes.Get(u);
			if (!xShape.m_bIsJointOnly && xShape.m_uBoneIndex == uOwner)
			{
				return &xShape;
			}
		}
		return nullptr;
	}
}

//------------------------------------------------------------------------------
// (7) A bone selection outlives a scrub and a clip refresh, and dies with the
// rig.
//
// ★ THE TWO HALVES ARE DIFFERENT FACTS, NOT ONE. A selection names a BONE, and a
// scrub or a clip edit changes WHEN and WHAT, never the rig — so it must survive
// both. A rig change replaces the very thing the index indexes, so the same
// number would silently mean a different bone, and it must not.
//------------------------------------------------------------------------------
ZENITH_TEST(AnimationPreview, BoneSelectionSurvivesASeekAndAClipRefreshButNotARigChange)
{
	AnimPreviewFixture xFixture("zenith_animpreview_boneselect");

	Zenith_AnimationPreviewSession xSession("Select");
	const Flux_AnimationClip xClip = xFixture.MakeClip("Walk", 2.0f, true);
	ZENITH_ASSERT_TRUE(xSession.Open(xClip, "game:Anims/Walk.zanim") == ZENITH_ANIMPREVIEW_OPEN_OK,
		"the session opens with its recorded rig");
	xSession.Pause();

	ZENITH_ASSERT_EQ(xSession.GetBoneCount(), 3u, "Root -> Spine -> Head");
	ZENITH_ASSERT_FALSE(xSession.HasBoneSelection(), "a freshly opened session has nothing selected");
	ZENITH_ASSERT_EQ(xSession.GetSelectedBoneIndex(), kuINVALID_BONE_SELECTION, "and reports the sentinel");

	xSession.SelectBone(1u);
	ZENITH_ASSERT_TRUE(xSession.HasBoneSelection(), "selecting by index takes");
	ZENITH_ASSERT_EQ(xSession.GetSelectedBoneIndex(), 1u, "and names the bone asked for");

	// ★ OUT OF RANGE CLEARS RATHER THAN STORING. An index that will never resolve
	// would give HasBoneSelection() a true that every consumer has to re-validate,
	// and the first one that forgot would index a bone that is not there.
	xSession.SelectBone(99u);
	ZENITH_ASSERT_FALSE(xSession.HasBoneSelection(), "an out-of-range index CLEARS the selection");

	xSession.SelectBone(2u);
	ZENITH_ASSERT_TRUE(xSession.Seek(1.0f), "scrub to the middle");
	ZENITH_ASSERT_EQ(xSession.GetSelectedBoneIndex(), 2u, "a scrub does not deselect — it changes WHEN, not WHAT");

	ZENITH_ASSERT_TRUE(xSession.RefreshClipFrom(xFixture.MakeClip("Walk", 2.0f, true)),
		"the document pushes an edited clip across");
	ZENITH_ASSERT_EQ(xSession.GetSelectedBoneIndex(), 2u, "and a clip refresh does not deselect either");

	// Hover is independent of selection and unfiltered — it is a paint hint the
	// frame owner resets.
	xSession.SetHoveredBoneIndex(1u);
	ZENITH_ASSERT_TRUE(xSession.HasBoneHover(), "hover is set");
	ZENITH_ASSERT_EQ(xSession.GetHoveredBoneIndex(), 1u, "at the bone asked for");
	ZENITH_ASSERT_EQ(xSession.GetSelectedBoneIndex(), 2u, "without disturbing the selection");
	xSession.SetHoveredBoneIndex(kuINVALID_BONE_SELECTION);
	ZENITH_ASSERT_FALSE(xSession.HasBoneHover(), "and cleared with the sentinel");

	xSession.Close();
	ZENITH_ASSERT_FALSE(xSession.HasBoneSelection(), "Close drops the selection with the rig");
	ZENITH_ASSERT_EQ(xSession.GetBoneCount(), 0u, "because there is no rig left to index");

	// ★ AND SO DOES A RIG CHANGE. An empty skeleton path is a rig change that
	// needs no second asset on disk: the resolve refuses before it ever reaches
	// the registry, the bone count goes 3 -> 0, and index 1 now means nothing.
	ZENITH_ASSERT_TRUE(xSession.Open(xClip, "game:Anims/Walk.zanim") == ZENITH_ANIMPREVIEW_OPEN_OK,
		"reopen over the same clip");
	xSession.Pause();
	xSession.SelectBone(1u);
	ZENITH_ASSERT_TRUE(xSession.HasBoneSelection(), "with a bone selected again");

	ZENITH_ASSERT_FALSE(xSession.SetRigOverride("", ""), "an empty rig does not resolve");
	ZENITH_ASSERT_FALSE(xSession.HasBoneSelection(),
		"and the selection goes with the rig it indexed, rather than pointing into a skeleton that is gone");
}

//------------------------------------------------------------------------------
// (8) Selecting a bone BY INDEX and BY PICK RAY resolve to the same bone.
//
// The ray is aimed at the midpoint of the capsule the pick set says bone 1 owns,
// so the test does not hard-code a joint position the clip is free to animate.
//------------------------------------------------------------------------------
ZENITH_TEST(AnimationPreview, SelectingByIndexAndByPickRayResolveToTheSameBone)
{
	AnimPreviewFixture xFixture("zenith_animpreview_bonepick");

	Zenith_AnimationPreviewSession xSession("Pick");
	const Flux_AnimationClip xClip = xFixture.MakeClip("Walk", 2.0f, true);
	ZENITH_ASSERT_TRUE(xSession.Open(xClip, "game:Anims/Walk.zanim") == ZENITH_ANIMPREVIEW_OPEN_OK, "session opens");
	xSession.Pause();

	// Seek somewhere the clip has actually separated the joints, then bring model
	// space current before anything reads a model matrix (§3.2).
	ZENITH_ASSERT_TRUE(xSession.Seek(1.0f), "scrub to the middle of the clip");
	xSession.RefreshDerivedPose();

	const Zenith_BonePickSet& xSet = xSession.GetBonePickSet();
	ZENITH_ASSERT_GT(xSet.m_xShapes.GetSize(), 0u, "the rig produced pick geometry");
	ZENITH_ASSERT_GT(xSet.m_fSkeletonExtent, 0.0f, "with a non-degenerate extent to scale radii from");

	const Zenith_BonePickShape* pxSpine = AnimPreview_FindCapsule(xSet, 1u);
	ZENITH_ASSERT_NOT_NULL(pxSpine, "Spine owns the capsule running to Head");
	if (pxSpine == nullptr)
	{
		return;
	}

	const Zenith_Maths::Vector3 xMidpoint = (pxSpine->m_xA + pxSpine->m_xB) * 0.5f;
	const Zenith_Maths::Vector3 xRayDir(-1.0f, 0.0f, 0.0f);
	const Zenith_Maths::Vector3 xRayOrigin = xMidpoint - xRayDir * 5.0f;

	u_int uPicked = kuINVALID_BONE_SELECTION;
	ZENITH_ASSERT_TRUE(xSession.PickBone(xRayOrigin, xRayDir, uPicked),
		"a ray through the middle of that capsule hits it");
	ZENITH_ASSERT_EQ(uPicked, 1u, "and reports the bone the capsule belongs to");

	xSession.SelectBone(uPicked);
	const u_int uByRay = xSession.GetSelectedBoneIndex();
	xSession.ClearBoneSelection();
	xSession.SelectBone(1u);
	ZENITH_ASSERT_EQ(uByRay, xSession.GetSelectedBoneIndex(),
		"picking by ray and selecting by index land on the same bone");

	// A miss changes neither the output nor the selection.
	u_int uUntouched = 0x1234u;
	ZENITH_ASSERT_FALSE(xSession.PickBone(
		xMidpoint + Zenith_Maths::Vector3(5.0f, 50.0f, 0.0f), xRayDir, uUntouched),
		"a ray far above the rig misses");
	ZENITH_ASSERT_EQ(uUntouched, 0x1234u, "and leaves the caller's index alone");
}

//------------------------------------------------------------------------------
// (9) ★ THE PICK SET IS REBUILT WHEN THE POSE MOVES, AND NOT OTHERWISE.
//
// This is the checkable form of the design note's §3.2 precondition. The set is
// derived from a CACHE that only ComputeSkinningMatrices fills, so the session
// has to know when that cache moved — and a build count is the only way a unit
// can tell "the pick used fresh geometry" from "the pick used last frame's and
// happened to give the same answer".
//------------------------------------------------------------------------------
ZENITH_TEST(AnimationPreview, ThePickSetIsRebuiltOnlyWhenThePoseMoves)
{
	AnimPreviewFixture xFixture("zenith_animpreview_pickcache");

	Zenith_AnimationPreviewSession xSession("Cache");
	const Flux_AnimationClip xClip = xFixture.MakeClip("Walk", 2.0f, true);
	ZENITH_ASSERT_TRUE(xSession.Open(xClip, "game:Anims/Walk.zanim") == ZENITH_ANIMPREVIEW_OPEN_OK, "session opens");
	xSession.Pause();

	xSession.GetBonePickSet();
	const u_int uAfterFirst = xSession.GetPickSetBuildCount();
	ZENITH_ASSERT_GT(uAfterFirst, 0u, "the first ask builds it");

	xSession.GetBonePickSet();
	xSession.GetBonePickSet();
	ZENITH_ASSERT_EQ(xSession.GetPickSetBuildCount(), uAfterFirst,
		"asking again with nothing changed costs nothing — the rebuild is lazy, not per call");

	ZENITH_ASSERT_TRUE(xSession.Seek(0.5f), "scrub");
	u_int uBone = kuINVALID_BONE_SELECTION;
	xSession.PickBone(Zenith_Maths::Vector3(5.0f, 0.25f, 0.0f), Zenith_Maths::Vector3(-1.0f, 0.0f, 0.0f), uBone);
	ZENITH_ASSERT_EQ(xSession.GetPickSetBuildCount(), uAfterFirst + 1u,
		"★ a pick after a seek rebuilds — otherwise it would be aiming at where the bones USED to be");

	const u_int uAfterSeek = xSession.GetPickSetBuildCount();
	xSession.RefreshDerivedPose();
	xSession.GetBonePickSet();
	ZENITH_ASSERT_EQ(xSession.GetPickSetBuildCount(), uAfterSeek + 1u,
		"and an explicit RefreshDerivedPose invalidates it too");

	// The session model matrix is baked into every shape, so moving it is as much
	// a change as moving a bone.
	const u_int uAfterRefresh = xSession.GetPickSetBuildCount();
	xSession.SetSessionModelMatrix(Zenith_Maths::Matrix4(1.0f));
	xSession.GetBonePickSet();
	ZENITH_ASSERT_EQ(xSession.GetPickSetBuildCount(), uAfterRefresh + 1u,
		"the session model matrix is folded into the shapes, so setting it invalidates them");
}

//------------------------------------------------------------------------------
// (10) A bone drag latches, suspends evaluation, and leaves the pose UNKEYED.
//
// ★ THE SUSPENSION IS THE LOAD-BEARING HALF. The clip and the drag are two
// writers of the same bone rotations; without it, a Tick between two mouse moves
// re-evaluates from the clip and the bone twitches back — which reads as jitter
// rather than as two drivers.
//------------------------------------------------------------------------------
ZENITH_TEST(AnimationPreview, ABoneDragSuspendsEvaluationAndLeavesThePoseUnkeyed)
{
	AnimPreviewFixture xFixture("zenith_animpreview_bonedrag");

	Zenith_AnimationPreviewSession xSession("Drag");
	const Flux_AnimationClip xClip = xFixture.MakeClip("Walk", 2.0f, true);
	ZENITH_ASSERT_TRUE(xSession.Open(xClip, "game:Anims/Walk.zanim") == ZENITH_ANIMPREVIEW_OPEN_OK, "session opens");
	ZENITH_ASSERT_TRUE(xSession.Seek(0.0f), "start at t = 0");

	ZENITH_ASSERT_FALSE(xSession.IsBoneDragActive(), "nothing is being dragged yet");
	ZENITH_ASSERT_FALSE(xSession.HasUnkeyedPose(), "and the live pose is whatever the clip says");
	ZENITH_ASSERT_FALSE(xSession.GetAutoKey(), "auto-key is OFF by default — a drag that silently wrote keys "
		"would be an edit nobody asked for");

	ZENITH_ASSERT_FALSE(xSession.BeginBoneDrag(99u), "an out-of-range bone cannot be dragged");
	ZENITH_ASSERT_TRUE(xSession.BeginBoneDrag(1u), "Spine can");
	ZENITH_ASSERT_FALSE(xSession.BeginBoneDrag(2u),
		"and a second drag is REFUSED rather than retargeted — one mouse-up cannot end two transactions");
	ZENITH_ASSERT_EQ(xSession.GetDragBoneIndex(), 1u, "the drag is on the bone it started on");

	const Zenith_Maths::Quat xInitial = xSession.GetDragInitialRotation();
	const Zenith_Maths::Quat xTarget = glm::angleAxis(glm::radians(30.0f), Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));
	ZENITH_ASSERT_TRUE(xSession.UpdateBoneDrag(xTarget), "the drag writes the live pose");
	ZENITH_ASSERT_TRUE(xSession.HasUnkeyedPose(), "★ and says so — this is the pose a later seek will destroy");

	const Zenith_Maths::Quat xLive = xSession.GetBoneLocalRotation(1u);
	ZENITH_ASSERT_GT(fabsf(glm::dot(xLive, xTarget)), 0.9999f, "the instance is holding the dragged rotation");
	ZENITH_ASSERT_LT(fabsf(glm::dot(xInitial, xTarget)), 0.9999f,
		"and the latched initial rotation is genuinely a different one, so the comparison above means something");

	// ★ EVALUATION IS SUSPENDED.
	const float fTimeAtDragStart = xSession.GetTime();
	xSession.Play();
	xSession.Tick(0.5f);
	ZENITH_ASSERT_EQ_FLOAT(xSession.GetTime(), fTimeAtDragStart, 1.0e-4f,
		"a tick during a drag advances nothing — the clip would otherwise stomp the drag");
	const Zenith_Maths::Quat xStillLive = xSession.GetBoneLocalRotation(1u);
	ZENITH_ASSERT_GT(fabsf(glm::dot(xStillLive, xTarget)), 0.9999f, "and the dragged rotation is still there");

	xSession.EndBoneDrag();
	ZENITH_ASSERT_FALSE(xSession.IsBoneDragActive(), "the drag is over");
	ZENITH_ASSERT_TRUE(xSession.HasUnkeyedPose(),
		"the pose SURVIVES the release, unkeyed — with auto-key off nothing wrote it to the document");

	xSession.Tick(0.25f);
	ZENITH_ASSERT_GT(xSession.GetTime(), fTimeAtDragStart, "and evaluation resumes");

	// ★ SEEKING IS THE EXPLICIT DISCARD (§4.4).
	ZENITH_ASSERT_TRUE(xSession.Seek(0.0f), "scrub");
	ZENITH_ASSERT_FALSE(xSession.HasUnkeyedPose(),
		"which re-evaluates every bone from the clip, so the unkeyed pose is gone and the flag with it");

	xSession.SetAutoKey(true);
	ZENITH_ASSERT_TRUE(xSession.GetAutoKey(), "auto-key is session state, ready for WU-4.3 to act on");
}
