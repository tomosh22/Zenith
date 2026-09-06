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
//   4. the shared preview slot is last-opened-wins and reclaimable;
//   5. a clip with no rig asks for one, and the answer is remembered per clip;
//   6. a seek MOVES the event bookkeeping mark even though it emits nothing, so
//      the next forward tick does not replay the skipped span as a burst (D40).
//
// All of it is CPU-only and runs headless under the Null backend: the rig and the
// preview mesh are tiny assets written into a private temp directory, the clips
// are plain data, and nothing here touches a device or a UI. None of these is
// requiresGraphics. UpdatePreviewView() is deliberately NOT exercised — it is the
// one entry point that needs a live renderer, and it early-outs without one.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"
#include "UnitTests/Zenith_UnitTests.h"

#include <filesystem>

namespace
{
	//--------------------------------------------------------------------------
	// Fixture — the shape Zenith_AnimationDocument.Tests.inl established: a
	// private temp directory removed on the way out, plus a ForceUnload of every
	// registry path the test caused to be loaded so a throwaway asset never
	// lingers in the live registry the suite runs inside.
	//
	// ★ IT ALSO RESETS THE PREVIEW-SLOT ARBITER at BOTH ends. The arbiter is
	// process-level state (there is one preview view slot in one renderer), so a
	// unit that left it claimed would hand its claim to the next unit — and the
	// symptom would be an unrelated test reporting the wrong slot owner.
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
// (4) The shared preview slot: last-opened-wins, and reclaimable.
//------------------------------------------------------------------------------
ZENITH_TEST(AnimationPreview, PreviewSlotIsLastOpenedWinsAndReclaimable)
{
	AnimPreviewFixture xFixture("zenith_animpreview_slot");

	Zenith_AnimationPreviewSession xFirst("Walk.zanim");
	Zenith_AnimationPreviewSession xSecond("Run.zanim");
	const Flux_AnimationClip xClip = xFixture.MakeClip("Walk", 2.0f, true);

	// Nothing owns the slot before anything opens.
	ZENITH_ASSERT_FALSE(xFirst.HasPreviewSlot(), "an unopened session holds nothing");
	ZENITH_ASSERT_TRUE(xFirst.GetPreviewSlotOwnerName().empty(), "an unowned slot has no owner name");

	xFirst.Open(xClip, "game:Anims/Walk.zanim");
	ZENITH_ASSERT_TRUE(xFirst.HasPreviewSlot(), "the first session to open takes the slot");
	ZENITH_ASSERT_TRUE(xFirst.GetPreviewSlotOwnerName() == "Walk.zanim", "and is named as its owner");

	xSecond.Open(xClip, "game:Anims/Run.zanim");
	ZENITH_ASSERT_TRUE(xSecond.HasPreviewSlot(), "the LAST opened wins");
	ZENITH_ASSERT_FALSE(xFirst.HasPreviewSlot(), "the first is dispossessed");
	// ★ THE DISPOSSESSED SESSION LEARNS WHO TOOK IT. This is what its placeholder
	// says; "the preview is unavailable" with no name is the version of this UI
	// that leaves a user hunting for the panel to close.
	ZENITH_ASSERT_TRUE(xFirst.GetPreviewSlotOwnerName() == "Run.zanim", "the dispossessed session names the new owner");

	ZENITH_ASSERT_TRUE(xFirst.ReclaimPreviewSlot(), "the reclaim button takes it back");
	ZENITH_ASSERT_TRUE(xFirst.HasPreviewSlot(), "the first session holds it again");
	ZENITH_ASSERT_FALSE(xSecond.HasPreviewSlot(), "and the second is now the dispossessed one");
	ZENITH_ASSERT_TRUE(xSecond.GetPreviewSlotOwnerName() == "Walk.zanim", "which names the first");

	// Reclaiming what you already hold is a no-op that still reports success.
	ZENITH_ASSERT_TRUE(xFirst.ReclaimPreviewSlot(), "reclaiming an owned slot succeeds");
	ZENITH_ASSERT_TRUE(xFirst.HasPreviewSlot(), "and keeps it");

	// ★ A DISPOSSESSED SESSION CLOSING MUST NOT FREE THE SLOT — it does not own it,
	// and releasing here would blank the live preview of whoever does.
	xSecond.Close();
	ZENITH_ASSERT_TRUE(xFirst.HasPreviewSlot(), "a dispossessed session's close leaves the owner alone");

	xFirst.Close();
	ZENITH_ASSERT_TRUE(Flux_PreviewSlotArbiter::GetOwner() == nullptr, "the owner's close frees the slot");
	ZENITH_ASSERT_TRUE(xFirst.GetPreviewSlotOwnerName().empty(), "and clears the owner name");
}

//------------------------------------------------------------------------------
// (4b) The OTHER claimant. Both editors go through the same arbiter, which is why
// it sits in Flux/RenderViews rather than in Editor/ — Flux_MaterialPreviewController
// could not have reached it there.
//------------------------------------------------------------------------------
ZENITH_TEST(AnimationPreview, MaterialEditorAndSessionArbitrateTheSamePreviewSlot)
{
	AnimPreviewFixture xFixture("zenith_animpreview_arbitration");

	Zenith_AnimationPreviewSession xSession("Walk.zanim");
	const Flux_AnimationClip xClip = xFixture.MakeClip("Walk", 2.0f, true);
	xSession.Open(xClip, "game:Anims/Walk.zanim");
	ZENITH_ASSERT_TRUE(xSession.HasPreviewSlot(), "opening the animation preview takes the slot");

	// ★ THE REAL MATERIAL-EDITOR PATH, NOT A STAND-IN OWNER. SetActive is exactly
	// what the Material Editor panel calls every frame it is visible, and it is
	// pure CPU state plus the arbiter claim, so it runs headless. (Update() is the
	// part that needs a renderer; it is deliberately not called here.)
	Flux_MaterialPreviewController xMaterialPreview;
	xMaterialPreview.SetActive(true);

	ZENITH_ASSERT_TRUE(xMaterialPreview.HasPreviewSlot(), "the material editor, opened last, wins");
	ZENITH_ASSERT_FALSE(xSession.HasPreviewSlot(), "the animation session is dispossessed");
	ZENITH_ASSERT_TRUE(xSession.GetPreviewSlotOwnerName() == "Material Editor",
		"and the session names the material editor as the owner");

	// ★ ACTIVE AND DISPOSSESSED ARE DIFFERENT THINGS, and that pair IS the
	// placeholder state. The DP automation asserts IsActive() stays true for the
	// whole time the panel is open, so arbitration must not touch it.
	ZENITH_ASSERT_TRUE(xMaterialPreview.IsActive(), "claiming/losing the slot does not move the liveness flag");

	ZENITH_ASSERT_TRUE(xSession.ReclaimPreviewSlot(), "the session's reclaim button takes it back");
	ZENITH_ASSERT_TRUE(xSession.HasPreviewSlot(), "the session holds it again");
	ZENITH_ASSERT_FALSE(xMaterialPreview.HasPreviewSlot(), "and the material editor is now dispossessed");
	ZENITH_ASSERT_TRUE(xMaterialPreview.GetPreviewSlotOwnerName() == "Walk.zanim",
		"which names the session");

	// ★ THE PER-FRAME REFRESH MUST NOT RE-STEAL. SetActive(true) runs every frame
	// the material panel is visible; if it claimed unconditionally the material
	// editor could never be dispossessed at all and last-opened-wins would quietly
	// become last-drawn-wins.
	xMaterialPreview.SetActive(true);
	xMaterialPreview.SetActive(true);
	ZENITH_ASSERT_FALSE(xMaterialPreview.HasPreviewSlot(), "a liveness refresh is not a claim");
	ZENITH_ASSERT_TRUE(xSession.HasPreviewSlot(), "the session still holds it");

	// A dispossessed panel closing must not free someone else's slot.
	xMaterialPreview.SetActive(false);
	ZENITH_ASSERT_TRUE(xSession.HasPreviewSlot(), "a dispossessed close leaves the owner alone");
	ZENITH_ASSERT_TRUE(xSession.GetPreviewSlotOwnerName() == "Walk.zanim", "the session is still the owner");

	// ...and reopening it wins again, because that IS a rising edge.
	xMaterialPreview.SetActive(true);
	ZENITH_ASSERT_TRUE(xMaterialPreview.HasPreviewSlot(), "reopening the material editor wins the slot");
	ZENITH_ASSERT_FALSE(xSession.HasPreviewSlot(), "and dispossesses the session again");
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
