//------------------------------------------------------------------------------
// Zenith_AnimationAsset unit tests (WU-2.1).
// Included at the bottom of Zenith_AnimationAsset.cpp.
//
// ★ THE HEADLINE PROPERTY OF THIS UNIT: a controller's BORROWED clip pointer stays
// valid across a reload AND observes the reloaded content. Everything else here
// exists to pin the edges of that — a failed reload must not disturb the live clip,
// a rename must be refused, and a refused .zanim must no longer look like a
// successful load.
//
// All of it runs headless under the Null backend: clips are pure CPU data, the
// files are written into the OS temp directory and removed again, and nothing
// touches a device. None of these is requiresGraphics.
//------------------------------------------------------------------------------

#include "UnitTests/Zenith_UnitTests.h"
#include "UnitTests/Zenith_AssertCapture.h"   // the refused-file / rename paths assert on purpose
#include "AssetHandling/Zenith_AssetTypeIds.h"
#include "DataStream/Zenith_StreamEnvelope.h"

#include <cstring>      // std::memcpy — poking the envelope's schema word
#include <filesystem>

namespace
{
	//--------------------------------------------------------------------------
	// Fixture
	//--------------------------------------------------------------------------

	// A private directory under the OS temp dir, removed on the way out, plus the
	// registry entry for the one file inside it. ForceUnload is unconditional: it is
	// a no-op when the path was never cached, and it is what keeps the live registry
	// the suite runs inside undisturbed by a test that loaded a throwaway asset.
	struct AnimReloadFixture
	{
		std::filesystem::path m_xDirectory;
		std::string m_strPath;

		explicit AnimReloadFixture(const char* szLeafDirectory)
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
			m_strPath = (m_xDirectory / "probe.zanim").generic_string();
		}

		~AnimReloadFixture()
		{
			Zenith_AssetRegistry::ForceUnload(m_strPath);
			std::error_code xError;
			std::filesystem::remove_all(m_xDirectory, xError);
		}

		AnimReloadFixture(const AnimReloadFixture&) = delete;
		AnimReloadFixture& operator=(const AnimReloadFixture&) = delete;
	};

	//--------------------------------------------------------------------------
	// Probe clip helpers
	//--------------------------------------------------------------------------

	// A one-bone clip whose Hip sits at a constant height. The height is the value
	// every test below SAMPLES — comparing key COUNTS would pass on a clip whose keys
	// had not moved at all, which is exactly the failure a reload can produce.
	void AnimBuildProbeClip(Flux_AnimationClip& xClip, const char* szName, float fHipHeight)
	{
		xClip.SetName(szName);
		xClip.SetDuration(2.0f);

		Flux_BoneChannel xHip;
		xHip.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0.0f, fHipHeight, 0.0f));
		xHip.AddPositionKeyframe(2.0f, Zenith_Maths::Vector3(0.0f, fHipHeight, 0.0f));
		xClip.AddBoneChannel("Hip", std::move(xHip));
	}

	void AnimWriteProbeClipFile(const std::string& strPath, const char* szName, float fHipHeight)
	{
		Flux_AnimationClip xClip;
		AnimBuildProbeClip(xClip, szName, fHipHeight);
		xClip.Export(strPath);
	}

	float AnimSampleHipHeight(const Flux_AnimationClip* pxClip)
	{
		if (pxClip == nullptr)
		{
			return -1.0f;
		}
		const Flux_BoneChannel* pxHip = pxClip->GetBoneChannel("Hip");
		if (pxHip == nullptr)
		{
			return -1.0f;
		}
		return pxHip->SamplePosition(1.0f).y;
	}

	// A well-formed .zanim whose envelope claims a schema this build does not read —
	// the "a newer tool wrote this" case, which must be refused rather than parsed
	// with the current field order.
	void AnimWriteFutureSchemaFile(const std::string& strPath)
	{
		Flux_AnimationClip xClip;
		AnimBuildProbeClip(xClip, "ReloadProbe", 1.0f);

		Zenith_DataStream xStream;
		xClip.WriteToDataStream(xStream);

		// The schema word is the FOURTH u_int of Zenith_StreamHeader, written in
		// declaration order by Zenith_WriteStreamHeader.
		const u_int uFutureSchema = uZENITH_ANIMATION_SCHEMA_CURRENT + 1u;
		std::memcpy(static_cast<uint8_t*>(xStream.GetData()) + (3 * sizeof(u_int)), &uFutureSchema, sizeof(u_int));

		xStream.WriteToFile(strPath.c_str());
	}

	// Four bytes. Short enough that Zenith_ReadStreamHeader's capacity check refuses
	// it outright, so this is the truncated-file case with no dependence on what the
	// bytes happen to be.
	void AnimWriteTruncatedFile(const std::string& strPath)
	{
		Zenith_DataStream xStream;
		const u_int uGarbage = 0xDEADBEEFu;
		xStream << uGarbage;
		xStream.WriteToFile(strPath.c_str());
	}
}

//==============================================================================
// (1) The headline: a borrowed clip pointer survives a reload and sees the new file.
//==============================================================================
ZENITH_TEST(AnimationAssetReload, LiveClipPointerSurvivesAndObservesTheNewFile)
{
	AnimReloadFixture xFixture("zenith_anim_reload_pointer");
	AnimWriteProbeClipFile(xFixture.m_strPath, "ReloadProbe", 1.0f);

	Zenith_AnimationAsset* pxAsset = Zenith_AssetRegistry::GetView<Zenith_AnimationAsset>(xFixture.m_strPath);
	ZENITH_ASSERT_NOT_NULL(pxAsset, "a well-formed .zanim must load");
	if (pxAsset == nullptr)
	{
		return;
	}

	Flux_AnimationClip* pxLiveClip = pxAsset->GetClip();
	ZENITH_ASSERT_NOT_NULL(pxLiveClip, "a loaded animation asset holds a clip");
	if (pxLiveClip == nullptr)
	{
		return;
	}
	ZENITH_ASSERT_EQ_FLOAT(AnimSampleHipHeight(pxLiveClip), 1.0f, 1e-5f, "the first load samples the first file");

	// ★ THIS is what a controller does: Flux_AnimationController::AddClipFromFile
	// borrows the asset's clip POINTER into its collection, and the state machine
	// resolves its clip references through that same name-keyed collection. Both are
	// invalidated by anything that moves the clip — which is all ForceUnload + a
	// fresh acquire can do, and is why an in-place reload exists (D26).
	Flux_AnimationClipCollection xBorrower;
	xBorrower.AddClipReference(pxLiveClip);
	ZENITH_ASSERT_TRUE(xBorrower.GetClip("ReloadProbe") == pxLiveClip, "the borrower resolves the clip by name");

	// The file changes underneath: same clip NAME, different key value.
	AnimWriteProbeClipFile(xFixture.m_strPath, "ReloadProbe", 5.0f);

	ZENITH_ASSERT_TRUE(pxAsset->ReloadFromDisk(), "the reload succeeds");

	ZENITH_ASSERT_TRUE(pxAsset->GetClip() == pxLiveClip, "the asset still owns the SAME clip object");
	ZENITH_ASSERT_TRUE(xBorrower.GetClip("ReloadProbe") == pxLiveClip, "the borrowed pointer is still the live clip");
	ZENITH_ASSERT_EQ_FLOAT(AnimSampleHipHeight(pxLiveClip), 5.0f, 1e-5f,
		"and that SAME pointer now samples the reloaded content");

	// The explicit-path overload is the same primitive, reached a different way.
	AnimWriteProbeClipFile(xFixture.m_strPath, "ReloadProbe", 9.0f);
	ZENITH_ASSERT_TRUE(pxAsset->ReloadFromDisk(xFixture.m_strPath), "the explicit-path overload reloads too");
	ZENITH_ASSERT_TRUE(pxAsset->GetClip() == pxLiveClip, "and still does not move the clip");
	ZENITH_ASSERT_EQ_FLOAT(AnimSampleHipHeight(pxLiveClip), 9.0f, 1e-5f, "and picks up the newest file");

	xBorrower.Clear();
}

//==============================================================================
// (3) A failed reload leaves the live clip on its previous contents, untouched.
//==============================================================================
ZENITH_TEST(AnimationAssetReload, AFailedReloadLeavesTheLiveClipUntouched)
{
	AnimReloadFixture xFixture("zenith_anim_reload_failure");
	AnimWriteProbeClipFile(xFixture.m_strPath, "ReloadProbe", 3.0f);

	Zenith_AnimationAsset* pxAsset = Zenith_AssetRegistry::GetView<Zenith_AnimationAsset>(xFixture.m_strPath);
	ZENITH_ASSERT_NOT_NULL(pxAsset, "the good file loads first");
	if (pxAsset == nullptr)
	{
		return;
	}
	Flux_AnimationClip* pxLiveClip = pxAsset->GetClip();
	ZENITH_ASSERT_EQ_FLOAT(AnimSampleHipHeight(pxLiveClip), 3.0f, 1e-5f, "the sampled value BEFORE any failure");

	// A truncated file. D27: the parse happens into a temporary, so nothing reaches
	// the live clip — the alternative (reset then parse) would empty a clip that is
	// mid-playback because a save went wrong.
	AnimWriteTruncatedFile(xFixture.m_strPath);
	{
		Zenith_AssertCaptureScope xCapture;
		ZENITH_ASSERT_FALSE(pxAsset->ReloadFromDisk(), "a truncated file is a FAILED reload");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "and asserts exactly once");
	}
	ZENITH_ASSERT_TRUE(pxAsset->GetClip() == pxLiveClip, "the clip object did not move");
	ZENITH_ASSERT_EQ_FLOAT(AnimSampleHipHeight(pxLiveClip), 3.0f, 1e-5f, "and still samples its PREVIOUS contents");
	ZENITH_ASSERT_TRUE(pxLiveClip->GetName() == "ReloadProbe", "and still has its name");

	// A well-formed file from a newer tool. Same contract.
	AnimWriteFutureSchemaFile(xFixture.m_strPath);
	{
		Zenith_AssertCaptureScope xCapture;
		ZENITH_ASSERT_FALSE(pxAsset->ReloadFromDisk(), "a future-schema file is a FAILED reload");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "and asserts exactly once");
	}
	ZENITH_ASSERT_EQ_FLOAT(AnimSampleHipHeight(pxLiveClip), 3.0f, 1e-5f, "still on its previous contents");

	// A missing file. No assert here — a deleted file is a reportable condition, not
	// a programming error.
	std::error_code xError;
	std::filesystem::remove(xFixture.m_strPath, xError);
	ZENITH_ASSERT_FALSE(pxAsset->ReloadFromDisk(), "a missing file is a FAILED reload");
	ZENITH_ASSERT_EQ_FLOAT(AnimSampleHipHeight(pxLiveClip), 3.0f, 1e-5f, "still on its previous contents");

	// A path that is not a .zanim at all is refused BEFORE anything is read, so it
	// produces one clear message rather than a missing-envelope assert.
	{
		Zenith_AssertCaptureScope xCapture;
		ZENITH_ASSERT_FALSE(pxAsset->ReloadFromDisk((xFixture.m_xDirectory / "probe.glb").generic_string()),
			"re-importing a source file is an import, not a reload");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "and asserts exactly once");
	}
	ZENITH_ASSERT_EQ_FLOAT(AnimSampleHipHeight(pxLiveClip), 3.0f, 1e-5f, "still on its previous contents");
}

//==============================================================================
// (4) Reloading repeatedly neither moves the clip nor accumulates content.
//==============================================================================
ZENITH_TEST(AnimationAssetReload, RepeatedReloadsNeitherMoveNorAccumulate)
{
	AnimReloadFixture xFixture("zenith_anim_reload_repeat");
	AnimWriteProbeClipFile(xFixture.m_strPath, "ReloadProbe", 2.0f);

	Zenith_AnimationAsset* pxAsset = Zenith_AssetRegistry::GetView<Zenith_AnimationAsset>(xFixture.m_strPath);
	ZENITH_ASSERT_NOT_NULL(pxAsset, "the probe file loads");
	if (pxAsset == nullptr)
	{
		return;
	}

	Flux_AnimationClip* const pxLiveClip = pxAsset->GetClip();
	ZENITH_ASSERT_NOT_NULL(pxLiveClip, "a loaded animation asset holds a clip");
	if (pxLiveClip == nullptr || pxLiveClip->GetBoneChannel("Hip") == nullptr)
	{
		ZENITH_FAIL("the probe clip must round-trip its Hip channel before the repeat can mean anything");
		return;
	}
	const u_int uChannelsAfterFirstLoad = pxLiveClip->GetBoneChannels().GetSize();
	const u_int uHipKeysAfterFirstLoad = pxLiveClip->GetBoneChannel("Hip")->GetKeyframeCount(FLUX_ANIM_TRACK_POSITION);

	for (u_int u = 0; u < 16u; ++u)
	{
		ZENITH_ASSERT_TRUE(pxAsset->ReloadFromDisk(), "every reload succeeds");
		ZENITH_ASSERT_TRUE(pxAsset->GetClip() == pxLiveClip, "and none of them moves the clip");
	}

	// ★ NO ALLOCATION-COUNT ASSERT HERE, DELIBERATELY. The only hook the engine
	// exposes (Zenith_MemoryTracker::GetAllocationCount) is PROCESS-WIDE and compiled
	// in at FULL tracking only, so background threads make it a flaky oracle for a
	// single call's behaviour. The leak this replaces is closed BY CONSTRUCTION
	// instead: Zenith_AnimationAsset::LoadZanimIntoLiveClip parses into a STACK-LOCAL
	// staging clip and replaces the live clip's contents in place, so the reload path
	// contains no `new Flux_AnimationClip` at all after the first load — which is
	// exactly what the two assertions below and the pointer identity above measure.
	ZENITH_ASSERT_EQ(pxLiveClip->GetBoneChannels().GetSize(), uChannelsAfterFirstLoad,
		"16 reloads leave the channel map the same SIZE, not 17 copies of it");
	const Flux_BoneChannel* pxHipAfter = pxLiveClip->GetBoneChannel("Hip");
	ZENITH_ASSERT_NOT_NULL(pxHipAfter, "the Hip channel is still there after 16 reloads");
	if (pxHipAfter != nullptr)
	{
		ZENITH_ASSERT_EQ(pxHipAfter->GetKeyframeCount(FLUX_ANIM_TRACK_POSITION), uHipKeysAfterFirstLoad,
			"and the same key count — a replace that appended would show up here");
	}
	ZENITH_ASSERT_EQ_FLOAT(AnimSampleHipHeight(pxLiveClip), 2.0f, 1e-5f, "and the clip still samples correctly");
}

//==============================================================================
// (2, asset level) A file that renames the clip is refused; D28.
//==============================================================================
ZENITH_TEST(AnimationAssetReload, AFileThatRenamesTheClipIsRefused)
{
	AnimReloadFixture xFixture("zenith_anim_reload_rename");
	AnimWriteProbeClipFile(xFixture.m_strPath, "ReloadProbe", 1.0f);

	Zenith_AnimationAsset* pxAsset = Zenith_AssetRegistry::GetView<Zenith_AnimationAsset>(xFixture.m_strPath);
	ZENITH_ASSERT_NOT_NULL(pxAsset, "the probe file loads");
	if (pxAsset == nullptr)
	{
		return;
	}
	Flux_AnimationClip* pxLiveClip = pxAsset->GetClip();

	// The borrower keys on the CURRENT name. If the reload were allowed to rename the
	// clip underneath it, this lookup would silently start returning null while the
	// pointer it handed out stayed alive — two broken lookups, no error.
	Flux_AnimationClipCollection xBorrower;
	xBorrower.AddClipReference(pxLiveClip);

	AnimWriteProbeClipFile(xFixture.m_strPath, "SomethingElse", 8.0f);
	{
		Zenith_AssertCaptureScope xCapture;
		ZENITH_ASSERT_FALSE(pxAsset->ReloadFromDisk(), "a file naming a different clip is refused");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "and asserts exactly once");
	}

	ZENITH_ASSERT_TRUE(pxLiveClip->GetName() == "ReloadProbe", "the live clip keeps its name");
	ZENITH_ASSERT_EQ_FLOAT(AnimSampleHipHeight(pxLiveClip), 1.0f, 1e-5f, "and its contents");
	ZENITH_ASSERT_TRUE(xBorrower.GetClip("ReloadProbe") == pxLiveClip, "and the borrower still resolves it");

	xBorrower.Clear();
}

//==============================================================================
// (5) A refused .zanim is not a successful load.
//==============================================================================
ZENITH_TEST(AnimationAssetLoad, ARefusedZanimIsNotASuccessfulLoad)
{
	// ★ THIS IS THE DEFECT THE STATUS-RETURNING PARSE CLOSES. LoadFromFile's .zanim
	// branch used to `return true` unconditionally — Flux_AnimationClip::
	// ReadFromDataStream was void — so a corrupt or stale file produced an assert, an
	// EMPTY clip, and an asset the registry happily cached and handed out.
	AnimReloadFixture xFixture("zenith_anim_load_refusal");

	AnimWriteTruncatedFile(xFixture.m_strPath);
	{
		Zenith_AssertCaptureScope xCapture;
		ZENITH_ASSERT_NULL(Zenith_AssetRegistry::GetView<Zenith_AnimationAsset>(xFixture.m_strPath),
			"a truncated .zanim must NOT resolve to an asset");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "the refusal asserts exactly once");
	}

	AnimWriteFutureSchemaFile(xFixture.m_strPath);
	{
		Zenith_AssertCaptureScope xCapture;
		ZENITH_ASSERT_NULL(Zenith_AssetRegistry::GetView<Zenith_AnimationAsset>(xFixture.m_strPath),
			"a future-schema .zanim must NOT resolve to an asset");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "the refusal asserts exactly once");
	}

	// And the same path, with a good file, does load — so the refusals above are
	// about the BYTES and not about the fixture.
	AnimWriteProbeClipFile(xFixture.m_strPath, "ReloadProbe", 1.0f);
	Zenith_AnimationAsset* pxAsset = Zenith_AssetRegistry::GetView<Zenith_AnimationAsset>(xFixture.m_strPath);
	ZENITH_ASSERT_NOT_NULL(pxAsset, "a well-formed .zanim at the same path loads");
	if (pxAsset != nullptr)
	{
		ZENITH_ASSERT_TRUE(pxAsset->IsValid(), "and reports itself valid");
		ZENITH_ASSERT_EQ_FLOAT(AnimSampleHipHeight(pxAsset->GetClip()), 1.0f, 1e-5f, "with the file's content");
	}
}
