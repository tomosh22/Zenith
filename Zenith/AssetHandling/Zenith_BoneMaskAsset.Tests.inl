//------------------------------------------------------------------------------
// Zenith_BoneMaskAsset unit tests (WU-6.2).
// Included at the bottom of Zenith_BoneMaskAsset.cpp.
//
// ★ THE HEADLINE PROPERTY: a mask is authored BY BONE NAME and only becomes
// indices when it meets a specific skeleton. Everything here pins an edge of
// that — an all-zero mask is still a mask (D47), a name the rig does not carry
// is REPORTED rather than dropped, and a refused file is not a successful load.
//
// All of it is headless: an in-memory Zenith_SkeletonAsset, files written into
// the OS temp directory and removed again, no device. None of it is
// requiresGraphics.
//------------------------------------------------------------------------------

#include "UnitTests/Zenith_UnitTests.h"
#include "UnitTests/Zenith_AssertCapture.h"   // the refused-file paths assert on purpose
// WU-7.1: the all-zero-mask acceptance case drives the REAL build path, because
// "the asset says it is a mask" only means something once a layer has been built
// from a controller def that names it.
#include "Flux/MeshAnimation/Flux_AnimationController.h"
#include "Flux/MeshAnimation/Flux_AnimatorControllerDef.h"
#include "Flux/MeshAnimation/Flux_AnimationLayer.h"

#include <cstring>      // std::memcpy — poking the envelope's schema word
#include <filesystem>

namespace
{
	// A private directory under the OS temp dir, removed on the way out, plus the
	// registry entries for the files inside it. ForceUnload is unconditional: a
	// no-op when the path was never cached, and what keeps the live registry the
	// suite runs inside undisturbed by a throwaway asset.
	struct BoneMaskFixture
	{
		std::filesystem::path m_xDirectory;
		std::string m_strPath;

		explicit BoneMaskFixture(const char* szLeafDirectory)
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
			m_strPath = (m_xDirectory / "probe.zanimmask").generic_string();
		}

		~BoneMaskFixture()
		{
			Zenith_AssetRegistry::ForceUnload(m_strPath);
			std::error_code xError;
			std::filesystem::remove_all(m_xDirectory, xError);
		}

		BoneMaskFixture(const BoneMaskFixture&) = delete;
		BoneMaskFixture& operator=(const BoneMaskFixture&) = delete;
	};

	// Root -> Spine -> Arm. Three bones is the smallest rig that can show a mask
	// covering SOME of it.
	void BoneMaskBuildRig(Zenith_SkeletonAsset& xSkeleton)
	{
		const Zenith_Maths::Quat xIdentity = glm::identity<Zenith_Maths::Quat>();
		const Zenith_Maths::Vector3 xUnitScale(1.0f);
		xSkeleton.AddBone("Root", -1, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), xIdentity, xUnitScale);
		xSkeleton.AddBone("Spine", 0, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), xIdentity, xUnitScale);
		xSkeleton.AddBone("Arm", 1, Zenith_Maths::Vector3(0.5f, 0.0f, 0.0f), xIdentity, xUnitScale);
		xSkeleton.ComputeBindPoseMatrices();
	}

	// Four bytes — short enough that Zenith_ReadStreamHeader's capacity check
	// refuses the file outright, with no dependence on what the bytes are.
	void BoneMaskWriteTruncatedFile(const std::string& strPath)
	{
		Zenith_DataStream xStream;
		const u_int uGarbage = 0xDEADBEEFu;
		xStream << uGarbage;
		xStream.WriteToFile(strPath.c_str());
	}

	// A well-formed mask whose envelope claims a schema this build does not read.
	void BoneMaskWriteFutureSchemaFile(const std::string& strPath)
	{
		Zenith_BoneMaskAsset xMask;
		xMask.SetBoneWeight("Spine", 1.0f);

		Zenith_DataStream xStream;
		xMask.WriteToDataStream(xStream);

		// The schema word is the FOURTH u_int of Zenith_StreamHeader, written in
		// declaration order by Zenith_WriteStreamHeader.
		const u_int uFutureSchema = uZENITH_ANIMMASK_SCHEMA_CURRENT + 1u;
		std::memcpy(static_cast<uint8_t*>(xStream.GetData()) + (3 * sizeof(u_int)), &uFutureSchema, sizeof(u_int));

		xStream.WriteToFile(strPath.c_str());
	}
}

//==============================================================================
// (1) A mask round-trips through a file, under an envelope that identifies it,
//     and comes back through the registry.
//==============================================================================
ZENITH_TEST(BoneMaskAsset, MaskRoundTripsThroughItsEnvelopeAndTheRegistry)
{
	BoneMaskFixture xFixture("zenith_bonemask_roundtrip");

	{
		Zenith_BoneMaskAsset xAuthored;
		xAuthored.SetBoneWeight("Spine", 1.0f);
		xAuthored.SetBoneWeight("Arm", 0.5f);
		// Replace, do not duplicate.
		xAuthored.SetBoneWeight("Arm", 0.25f);
		ZENITH_ASSERT_EQ(xAuthored.GetEntryCount(), 2u, "SetBoneWeight on a named bone REPLACES its weight");
		ZENITH_ASSERT_EQ_FLOAT(xAuthored.GetBoneWeight("Arm"), 0.25f, 1e-5f, "and it is the second value that sticks");

		// The envelope FIRST: a payload that round-trips inside the wrong header is
		// a file another loader will accept.
		Zenith_DataStream xStream;
		xAuthored.WriteToDataStream(xStream);
		xStream.SetCursor(0);
		Zenith_Result<Zenith_StreamHeader> xHeader = Zenith_ReadStreamHeader(xStream, uZENITH_ANIMMASK_ASSET_TYPE_ID);
		ZENITH_ASSERT_TRUE(xHeader.IsOk(), "a bone mask must lead with the shared stream envelope");
		if (xHeader.IsOk())
		{
			ZENITH_ASSERT_EQ(xHeader.Value().m_uAssetTypeId, uZENITH_ANIMMASK_ASSET_TYPE_ID, ".zanimmask envelope type id");
			ZENITH_ASSERT_EQ(xHeader.Value().m_uSchemaVersion, uZENITH_ANIMMASK_SCHEMA_CURRENT, ".zanimmask envelope schema");
		}

		ZENITH_ASSERT_TRUE(xAuthored.Export(xFixture.m_strPath), "Export writes the file");
	}

	// Through the REGISTRY, which is the thing WU-6.3/6.4/6.5 will use — a direct
	// ParseStream would not prove the loader is wired up.
	Zenith_BoneMaskAsset* pxLoaded = Zenith_AssetRegistry::GetView<Zenith_BoneMaskAsset>(xFixture.m_strPath);
	ZENITH_ASSERT_NOT_NULL(pxLoaded, "a well-formed .zanimmask resolves through the registry");
	if (pxLoaded == nullptr)
	{
		return;
	}

	ZENITH_ASSERT_EQ(pxLoaded->GetEntryCount(), 2u, "both entries round-trip");
	ZENITH_ASSERT_EQ_FLOAT(pxLoaded->GetBoneWeight("Spine"), 1.0f, 1e-5f, "Spine's weight round-trips");
	ZENITH_ASSERT_EQ_FLOAT(pxLoaded->GetBoneWeight("Arm"), 0.25f, 1e-5f, "Arm's weight round-trips");
	ZENITH_ASSERT_EQ_FLOAT(pxLoaded->GetBoneWeight("Root"), 0.0f, 1e-5f, "a bone the mask does not name weighs 0");
	ZENITH_ASSERT_TRUE(pxLoaded->HasAvatarMask(), "and it is a mask");

	// And it resolves onto a rig by NAME.
	Zenith_SkeletonAsset xSkeleton;
	BoneMaskBuildRig(xSkeleton);
	Flux_BoneMask xResolved;
	ZENITH_ASSERT_TRUE(pxLoaded->ResolveTo(xSkeleton, xResolved), "every name in the mask is on this rig");
	ZENITH_ASSERT_EQ_FLOAT(xResolved.GetBoneWeight(0u), 0.0f, 1e-5f, "Root (index 0) is unmasked");
	ZENITH_ASSERT_EQ_FLOAT(xResolved.GetBoneWeight(1u), 1.0f, 1e-5f, "Spine resolved to index 1");
	ZENITH_ASSERT_EQ_FLOAT(xResolved.GetBoneWeight(2u), 0.25f, 1e-5f, "Arm resolved to index 2");
}

//==============================================================================
// (2) D47 — an ALL-ZERO mask round-trips as a mask.
//==============================================================================
ZENITH_TEST(BoneMaskAsset, AnAllZeroMaskStillRoundTripsAsAMask)
{
	// ★ THIS IS THE GAP THE EXPLICIT FLAG CLOSES.
	// Flux_AnimationLayer::ReadFromDataStream decides "this layer has an avatar
	// mask" by scanning for `any weight > 0`. An all-zero mask is a perfectly
	// meaningful one — "this layer overrides nothing yet" — and under that rule it
	// comes back as NO MASK, at which point the layer overrides the WHOLE skeleton.
	// Losing a mask does not make a layer do less; it makes it do everything.
	BoneMaskFixture xFixture("zenith_bonemask_allzero");

	{
		Zenith_BoneMaskAsset xAuthored;
		xAuthored.SetBoneWeight("Spine", 0.0f);
		xAuthored.SetBoneWeight("Arm", 0.0f);
		ZENITH_ASSERT_TRUE(xAuthored.HasAvatarMask(), "a .zanimmask is a mask by default, whatever its weights sum to");
		ZENITH_ASSERT_TRUE(xAuthored.Export(xFixture.m_strPath), "Export writes the file");
	}

	Zenith_BoneMaskAsset* pxLoaded = Zenith_AssetRegistry::GetView<Zenith_BoneMaskAsset>(xFixture.m_strPath);
	ZENITH_ASSERT_NOT_NULL(pxLoaded, "an all-zero mask is still a loadable file");
	if (pxLoaded == nullptr)
	{
		return;
	}
	ZENITH_ASSERT_TRUE(pxLoaded->HasAvatarMask(), "and it comes back as a mask, not as 'no mask'");
	ZENITH_ASSERT_EQ(pxLoaded->GetEntryCount(), 2u, "with its entries intact");
	ZENITH_ASSERT_EQ_FLOAT(pxLoaded->GetBoneWeight("Spine"), 0.0f, 1e-5f, "and its zero weights");

	// The flag is DATA, not a derivation: an author can turn it off and that also
	// survives, which is what makes the round trip lossless rather than merely
	// biased towards "yes".
	pxLoaded->SetHasAvatarMask(false);
	ZENITH_ASSERT_TRUE(pxLoaded->Export(xFixture.m_strPath), "re-export the flag");
	Zenith_BoneMaskAsset xReread;
	Zenith_DataStream xStream;
	xStream.ReadFromFile(xFixture.m_strPath.c_str());
	ZENITH_ASSERT_TRUE(xReread.ParseStream(xStream).IsOk(), "the re-exported file parses");
	ZENITH_ASSERT_FALSE(xReread.HasAvatarMask(), "and the flag survives in BOTH states");
}

//==============================================================================
// (3) A name the rig does not carry is REPORTED, not dropped.
//==============================================================================
ZENITH_TEST(BoneMaskAsset, AnUnresolvableBoneNameIsReportedNotDropped)
{
	Zenith_SkeletonAsset xSkeleton;
	BoneMaskBuildRig(xSkeleton);

	Zenith_BoneMaskAsset xMask;
	xMask.SetBoneWeight("Spine", 1.0f);
	xMask.SetBoneWeight("Elbow", 0.75f);   // not on this rig — a typo, or the wrong rig

	Flux_BoneMask xResolved;
	// ★ FALSE, NOT A SILENT PARTIAL. A mask quietly losing a bone is a layer that
	// quietly starts or stops overriding it, and the only symptom is an animation
	// that looks wrong. ResolveTo names the bone in a Zenith_Error as well.
	ZENITH_ASSERT_FALSE(xMask.ResolveTo(xSkeleton, xResolved), "an unresolvable bone name fails the resolve");

	// Everything resolvable is still applied, so a caller may carry on with a
	// partial mask — it just cannot do so unknowingly.
	ZENITH_ASSERT_EQ_FLOAT(xResolved.GetBoneWeight(1u), 1.0f, 1e-5f, "Spine still resolved");
	ZENITH_ASSERT_EQ_FLOAT(xResolved.GetBoneWeight(0u), 0.0f, 1e-5f, "and nothing was written for the missing bone");

	// The same mask against a rig that DOES carry the bone resolves cleanly, so the
	// failure above is about the RIG and not about the mask being malformed.
	Zenith_SkeletonAsset xFullSkeleton;
	BoneMaskBuildRig(xFullSkeleton);
	xFullSkeleton.AddBone("Elbow", 2, Zenith_Maths::Vector3(0.5f, 0.0f, 0.0f),
		glm::identity<Zenith_Maths::Quat>(), Zenith_Maths::Vector3(1.0f));
	xFullSkeleton.ComputeBindPoseMatrices();

	Flux_BoneMask xResolvedFull;
	ZENITH_ASSERT_TRUE(xMask.ResolveTo(xFullSkeleton, xResolvedFull), "the same mask resolves on the rig that has the bone");
	ZENITH_ASSERT_EQ_FLOAT(xResolvedFull.GetBoneWeight(3u), 0.75f, 1e-5f, "Elbow resolved to index 3");
}

//==============================================================================
// (4) A refused .zanimmask is not a successful load.
//==============================================================================
ZENITH_TEST(BoneMaskAsset, ARefusedFileIsNotASuccessfulLoad)
{
	BoneMaskFixture xFixture("zenith_bonemask_refusal");

	BoneMaskWriteTruncatedFile(xFixture.m_strPath);
	{
		Zenith_AssertCaptureScope xCapture;
		ZENITH_ASSERT_NULL(Zenith_AssetRegistry::GetView<Zenith_BoneMaskAsset>(xFixture.m_strPath),
			"a truncated .zanimmask must NOT resolve to an asset");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "the refusal asserts exactly once");
	}

	BoneMaskWriteFutureSchemaFile(xFixture.m_strPath);
	{
		Zenith_AssertCaptureScope xCapture;
		ZENITH_ASSERT_NULL(Zenith_AssetRegistry::GetView<Zenith_BoneMaskAsset>(xFixture.m_strPath),
			"a future-schema .zanimmask must NOT resolve to an asset");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "the refusal asserts exactly once");
	}

	// A .zanim's envelope in a .zanimmask's clothing. One extension, one format:
	// the type id is what makes that mechanical rather than a naming convention.
	{
		Zenith_DataStream xStream;
		Zenith_WriteStreamHeader(xStream, uZENITH_ANIMATION_ASSET_TYPE_ID, uZENITH_ANIMATION_SCHEMA_CURRENT);
		xStream.WriteToFile(xFixture.m_strPath.c_str());

		Zenith_AssertCaptureScope xCapture;
		ZENITH_ASSERT_NULL(Zenith_AssetRegistry::GetView<Zenith_BoneMaskAsset>(xFixture.m_strPath),
			"another asset type's envelope must NOT resolve as a bone mask");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "the refusal asserts exactly once");
	}

	// And the same path with a good file DOES load — so the refusals above are
	// about the BYTES and not about the fixture.
	{
		Zenith_BoneMaskAsset xGood;
		xGood.SetBoneWeight("Spine", 1.0f);
		ZENITH_ASSERT_TRUE(xGood.Export(xFixture.m_strPath), "write a good file at the same path");
	}
	Zenith_BoneMaskAsset* pxLoaded = Zenith_AssetRegistry::GetView<Zenith_BoneMaskAsset>(xFixture.m_strPath);
	ZENITH_ASSERT_NOT_NULL(pxLoaded, "a well-formed .zanimmask at the same path loads");
	if (pxLoaded != nullptr)
	{
		ZENITH_ASSERT_EQ_FLOAT(pxLoaded->GetBoneWeight("Spine"), 1.0f, 1e-5f, "with its content");
	}
}

//==============================================================================
// (5) WU-7.1 — D47 CARRIED ALL THE WAY TO A LIVE LAYER.
//
// ★ TEST (2) ABOVE PROVES THE FILE ROUND-TRIPS THE FLAG, WHICH IS NOT THE SAME
// CLAIM. What matters is whether the layer a controller builds from a def that
// NAMES this mask ends up masked — and an OVERRIDE layer that comes back
// unmasked replaces the WHOLE skeleton, so getting this wrong makes a layer do
// more rather than less. Flux_AnimationController::BuildFromControllerDef gates
// SetAvatarMask on the ASSET'S flag rather than on the resolved weights; this
// is what pins that, in both directions.
//==============================================================================
ZENITH_TEST(BoneMaskAsset, AnAllZeroMaskFromAnAssetStillMasksTheLayerBuiltFromIt)
{
	BoneMaskFixture xFixture("zenith_bonemask_layerflag");

	Zenith_SkeletonAsset xSkeleton;
	BoneMaskBuildRig(xSkeleton);

	// An explicitly all-zero mask: "this layer overrides nothing yet", which is a
	// real authoring state and not an empty file.
	{
		Zenith_BoneMaskAsset xAuthored;
		xAuthored.SetBoneWeight("Spine", 0.0f);
		xAuthored.SetBoneWeight("Arm", 0.0f);
		ZENITH_ASSERT_TRUE(xAuthored.Export(xFixture.m_strPath), "write the all-zero mask");
	}

	// The smallest def that can carry a mask reference: one layer, no clips, no
	// top-level machine. BuildFromControllerDef acquires the mask through the
	// registry, so this is the real path and not a hand-call to ResolveTo.
	Flux_AnimatorControllerDef xDef;
	xDef.SetName("MaskFlagProbe");
	Flux_AnimatorControllerLayerDef* pxLayerDef = xDef.AddLayer("Overlay");
	pxLayerDef->SetBlendMode(LAYER_BLEND_OVERRIDE);
	pxLayerDef->SetBoneMaskAssetPath(xFixture.m_strPath);

	{
		Flux_AnimationController xController;
		ZENITH_ASSERT_TRUE(xController.BuildFromControllerDef(xDef, &xSkeleton),
			"a mask that resolves cleanly makes the build complete");
		ZENITH_ASSERT_EQ(xController.GetLayerCount(), 1u, "the layer was built");
		if (xController.GetLayerCount() == 1u)
		{
			const Flux_AnimationLayer* pxLayer = xController.GetLayer(0);
			// ★ THE ASSERTION THAT MATTERS. Derived from the weights this would be
			// FALSE — every weight is zero.
			ZENITH_ASSERT_TRUE(pxLayer->HasAvatarMask(),
				"the layer is masked because the ASSET says it is, not because a weight is non-zero");
			ZENITH_ASSERT_FALSE(pxLayer->GetAvatarMask().HasAnyNonZeroWeight(),
				"and the resolved mask really is all-zero — the flag is not standing in for content");
			ZENITH_ASSERT_EQ_FLOAT(pxLayer->GetAvatarMask().GetBoneWeight(1u), 0.0f, 1e-5f,
				"Spine resolved, at weight zero");
		}
		xController.ReleaseAssetReferences();
	}

	// ...and the flag OFF leaves the layer unmasked, so it is the flag being read
	// rather than the mask always being applied.
	Zenith_AssetRegistry::ForceUnload(xFixture.m_strPath);
	{
		Zenith_BoneMaskAsset xAuthored;
		xAuthored.SetBoneWeight("Spine", 1.0f);
		xAuthored.SetHasAvatarMask(false);
		ZENITH_ASSERT_TRUE(xAuthored.Export(xFixture.m_strPath), "rewrite it with the flag off");
	}

	{
		Flux_AnimationController xController;
		ZENITH_ASSERT_TRUE(xController.BuildFromControllerDef(xDef, &xSkeleton),
			"the build is still complete — 'not a mask' is not a failure");
		if (xController.GetLayerCount() == 1u)
		{
			ZENITH_ASSERT_FALSE(xController.GetLayer(0)->HasAvatarMask(),
				"an asset whose flag is off leaves the layer unmasked, non-zero weights and all");
			// The PATH is still recorded, so an ExportControllerDef does not turn the
			// flag-off state into a mask DELETION.
			ZENITH_ASSERT_TRUE(xController.GetLayer(0)->GetBoneMaskAssetPath() == xFixture.m_strPath,
				"and the reference survives, so a save does not drop it");
		}
		xController.ReleaseAssetReferences();
	}
}

//==============================================================================
// (6) WU-7.1 — CopyFrom deep-copies the CONTENT and not the asset identity.
//
// This is what Zenith_BoneMaskDocument's working copy is made of, so "the
// document edited the live asset by accident" is the failure it prevents.
//==============================================================================
ZENITH_TEST(BoneMaskAsset, CopyFromTakesTheContentAndNotTheRegistryIdentity)
{
	Zenith_BoneMaskAsset xSource;
	xSource.SetBoneWeight("Spine", 1.0f);
	xSource.SetBoneWeight("Arm", 0.5f);
	xSource.SetHasAvatarMask(false);

	Zenith_BoneMaskAsset xCopy;
	xCopy.SetBoneWeight("Stale", 1.0f);   // something to be overwritten
	xCopy.CopyFrom(xSource);

	ZENITH_ASSERT_EQ(xCopy.GetEntryCount(), 2u, "the copy holds exactly the source's entries");
	ZENITH_ASSERT_FALSE(xCopy.HasBone("Stale"), "and none of its own");
	ZENITH_ASSERT_EQ_FLOAT(xCopy.GetBoneWeight("Arm"), 0.5f, 1e-5f, "with their weights");
	ZENITH_ASSERT_FALSE(xCopy.HasAvatarMask(), "and the flag");

	// ★ DEEP, not aliased: editing the copy must not reach the source, which is
	// the entire point of a working copy.
	xCopy.SetBoneWeight("Arm", 0.125f);
	xCopy.SetHasAvatarMask(true);
	ZENITH_ASSERT_EQ_FLOAT(xSource.GetBoneWeight("Arm"), 0.5f, 1e-5f, "the source is untouched");
	ZENITH_ASSERT_FALSE(xSource.HasAvatarMask(), "flag included");
}
