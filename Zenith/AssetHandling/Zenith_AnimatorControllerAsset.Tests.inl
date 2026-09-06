//------------------------------------------------------------------------------
// Zenith_AnimatorControllerAsset unit tests (WU-6.2) — the ACCEPTANCE case.
// Included at the bottom of Zenith_AnimatorControllerAsset.cpp.
//
// ★ WHAT THIS FILE EXISTS TO CATCH, and why the def's own tests cannot: a def
// that round-trips through a stream proves nothing about whether the RUNTIME it
// rebuilds is the one that was authored. The controller is where a layer id, a
// blend mode, an emit flag and a mask stop being fields and start being the
// thing an animation plays through — and three of those four are absent from
// Flux_AnimationLayer's own serializer, so they only survive at all because the
// .zanimctrl carries them.
//
// Headless throughout: real .zanim / .zanimmask / .zanimctrl files in the OS
// temp directory, an in-memory Zenith_SkeletonAsset, no device. Not
// requiresGraphics.
//------------------------------------------------------------------------------

#include "UnitTests/Zenith_UnitTests.h"
#include "UnitTests/Zenith_AssertCapture.h"   // the refused-file paths assert on purpose
#include "AssetHandling/Zenith_AnimationAsset.h"
#include "AssetHandling/Zenith_BoneMaskAsset.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "AssetHandling/Zenith_AssetTypeIds.h"
#include "DataStream/Zenith_StreamEnvelope.h"
#include "Flux/MeshAnimation/Flux_AnimationClip.h"
#include "Flux/MeshAnimation/Flux_AnimationController.h"

#include <cstring>      // std::memcpy — poking the envelope's schema word
#include <filesystem>

namespace
{
	//--------------------------------------------------------------------------
	// Fixture: a temp directory holding two clips, two masks and one controller.
	//--------------------------------------------------------------------------
	struct AnimCtrlFixture
	{
		std::filesystem::path m_xDirectory;
		std::string m_strControllerPath;
		std::string m_strIdleClipPath;
		std::string m_strWalkClipPath;
		std::string m_strLowerMaskPath;
		std::string m_strUpperMaskPath;

		explicit AnimCtrlFixture(const char* szLeafDirectory)
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
			m_strIdleClipPath   = (m_xDirectory / "Idle.zanim").generic_string();
			m_strWalkClipPath   = (m_xDirectory / "Walk.zanim").generic_string();
			m_strLowerMaskPath  = (m_xDirectory / "Lower.zanimmask").generic_string();
			m_strUpperMaskPath  = (m_xDirectory / "Upper.zanimmask").generic_string();
		}

		~AnimCtrlFixture()
		{
			// Unconditional: a no-op for a path that was never cached, and what keeps
			// the live registry the suite runs inside undisturbed by throwaway assets.
			Zenith_AssetRegistry::ForceUnload(m_strControllerPath);
			Zenith_AssetRegistry::ForceUnload(m_strIdleClipPath);
			Zenith_AssetRegistry::ForceUnload(m_strWalkClipPath);
			Zenith_AssetRegistry::ForceUnload(m_strLowerMaskPath);
			Zenith_AssetRegistry::ForceUnload(m_strUpperMaskPath);
			std::error_code xError;
			std::filesystem::remove_all(m_xDirectory, xError);
		}

		AnimCtrlFixture(const AnimCtrlFixture&) = delete;
		AnimCtrlFixture& operator=(const AnimCtrlFixture&) = delete;
	};

	// Root -> Spine -> Arm, the same three-bone rig the mask unit uses.
	void AnimCtrlBuildRig(Zenith_SkeletonAsset& xSkeleton)
	{
		const Zenith_Maths::Quat xIdentity = glm::identity<Zenith_Maths::Quat>();
		const Zenith_Maths::Vector3 xUnitScale(1.0f);
		xSkeleton.AddBone("Root", -1, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), xIdentity, xUnitScale);
		xSkeleton.AddBone("Spine", 0, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), xIdentity, xUnitScale);
		xSkeleton.AddBone("Arm", 1, Zenith_Maths::Vector3(0.5f, 0.0f, 0.0f), xIdentity, xUnitScale);
		xSkeleton.ComputeBindPoseMatrices();
	}

	void AnimCtrlWriteClipFile(const std::string& strPath, const char* szClipName)
	{
		Flux_AnimationClip xClip;
		xClip.SetName(szClipName);
		xClip.SetDuration(1.0f);
		xClip.SetLooping(true);

		Flux_BoneChannel xChannel;
		xChannel.SetBoneName("Root");
		xChannel.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
		xClip.AddBoneChannel("Root", std::move(xChannel));

		xClip.Export(strPath);
	}

	void AnimCtrlWriteMaskFile(const std::string& strPath, const char* szBone, float fWeight)
	{
		Zenith_BoneMaskAsset xMask;
		xMask.SetBoneWeight(szBone, fWeight);
		xMask.Export(strPath);
	}

	// One state with a NAMED clip leaf — the shape a def carries (a clip POINTER is
	// resolved against a live collection and is not authored data).
	void AnimCtrlAddClipState(Flux_AnimationStateMachineDef& xDef, const char* szState, const char* szClipName)
	{
		Flux_AnimationState* pxState = xDef.AddState(szState);
		Flux_BlendTreeNode_Clip* pxLeaf = new Flux_BlendTreeNode_Clip();
		pxLeaf->SetClipName(szClipName);
		pxState->SetBlendTree(pxLeaf);
	}

	void AnimCtrlAuthorGraph(Flux_AnimationStateMachineDef& xDef, const char* szName)
	{
		xDef.SetName(szName);
		xDef.GetParameterDeclarations().AddFloat("Speed", 0.0f);
		AnimCtrlAddClipState(xDef, "Idle", "Idle");
		AnimCtrlAddClipState(xDef, "Walk", "Walk");
		xDef.SetDefaultState("Idle");

		Flux_StateTransition xToWalk;
		xToWalk.m_strTargetStateName = "Walk";
		xToWalk.m_fTransitionDuration = 0.2f;
		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "Speed";
		xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Greater;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Float;
		xCond.m_fThreshold = 0.5f;
		xToWalk.m_xConditions.PushBack(xCond);
		xDef.GetState("Idle")->AddTransition(xToWalk);
	}

	// The full authored controller: two clips, a top-level graph, and two masked
	// layers that differ in every field a layer has.
	void AnimCtrlAuthorControllerDef(Flux_AnimatorControllerDef& xDef, const AnimCtrlFixture& xFixture)
	{
		xDef.SetName("Probe");
		xDef.AddClipPath(xFixture.m_strIdleClipPath);
		xDef.AddClipPath(xFixture.m_strWalkClipPath);

		AnimCtrlAuthorGraph(xDef.GetOrCreateStateMachineDef(), "Base");

		Flux_AnimatorControllerLayerDef* pxLower = xDef.AddLayer("Lower");
		pxLower->SetWeight(0.75f);
		pxLower->SetBlendMode(LAYER_BLEND_OVERRIDE);
		pxLower->SetEmitEvents(true);
		pxLower->SetBoneMaskAssetPath(xFixture.m_strLowerMaskPath);
		AnimCtrlAuthorGraph(pxLower->GetStateMachineDef(), "LowerGraph");

		Flux_AnimatorControllerLayerDef* pxUpper = xDef.AddLayer("Upper");
		pxUpper->SetWeight(0.25f);
		pxUpper->SetBlendMode(LAYER_BLEND_ADDITIVE);
		pxUpper->SetEmitEvents(false);
		pxUpper->SetBoneMaskAssetPath(xFixture.m_strUpperMaskPath);
		AnimCtrlAuthorGraph(pxUpper->GetStateMachineDef(), "UpperGraph");
	}

	// Write every file the fixture's controller needs, then the controller itself.
	void AnimCtrlWriteEverything(const AnimCtrlFixture& xFixture)
	{
		AnimCtrlWriteClipFile(xFixture.m_strIdleClipPath, "Idle");
		AnimCtrlWriteClipFile(xFixture.m_strWalkClipPath, "Walk");
		AnimCtrlWriteMaskFile(xFixture.m_strLowerMaskPath, "Root", 1.0f);
		AnimCtrlWriteMaskFile(xFixture.m_strUpperMaskPath, "Arm", 0.5f);

		Zenith_AnimatorControllerAsset xAsset;
		AnimCtrlAuthorControllerDef(xAsset.GetDef(), xFixture);
		xAsset.Export(xFixture.m_strControllerPath);
	}
}

//==============================================================================
// (1) THE ACCEPTANCE CASE. A def with a top-level machine and two masked layers
//     round-trips through a .zanimctrl plus two .zanimmask files and rebuilds an
//     IDENTICAL RUNTIME.
//==============================================================================
ZENITH_TEST(AnimatorControllerAsset, AControllerRoundTripsThroughFilesIntoAnIdenticalRuntime)
{
	AnimCtrlFixture xFixture("zenith_animctrl_roundtrip");
	AnimCtrlWriteEverything(xFixture);

	// Through the REGISTRY — which is what a game and WU-6.5's panel will use. A
	// direct ParseStream would leave the loader registration untested.
	Zenith_AnimatorControllerAsset* pxAsset =
		Zenith_AssetRegistry::GetView<Zenith_AnimatorControllerAsset>(xFixture.m_strControllerPath);
	ZENITH_ASSERT_NOT_NULL(pxAsset, "a well-formed .zanimctrl resolves through the registry");
	if (pxAsset == nullptr)
	{
		return;
	}

	Zenith_SkeletonAsset xSkeleton;
	AnimCtrlBuildRig(xSkeleton);

	Flux_AnimationController xController;
	ZENITH_ASSERT_TRUE(xController.BuildFromControllerDef(pxAsset->GetDef(), &xSkeleton),
		"every clip and every mask the def names resolves");

	// --- Clips reached the collection, by NAME, from their FILES -------------
	ZENITH_ASSERT_NOT_NULL(xController.GetClip("Idle"), "the Idle clip is in the controller's collection");
	ZENITH_ASSERT_NOT_NULL(xController.GetClip("Walk"), "the Walk clip is in the controller's collection");

	// --- The top-level machine ------------------------------------------------
	ZENITH_ASSERT_TRUE(xController.HasStateMachine(), "the def's top-level machine was built");
	if (xController.HasStateMachine())
	{
		const Flux_AnimationStateMachine* pxTop = xController.GetStateMachinePtr();
		ZENITH_ASSERT_TRUE(pxTop->GetName() == "Base", "the top-level machine's name");
		ZENITH_ASSERT_TRUE(pxTop->HasState("Idle") && pxTop->HasState("Walk"), "its states");
		ZENITH_ASSERT_TRUE(pxTop->GetDefaultStateName() == "Idle", "its default state");
		const Flux_AnimationState* pxIdle = pxTop->GetState("Idle");
		ZENITH_ASSERT_EQ(pxIdle->GetTransitions().GetSize(), 1u, "its transition");
		if (pxIdle->GetTransitions().GetSize() == 1u)
		{
			ZENITH_ASSERT_TRUE(pxIdle->GetTransitions().Get(0).m_strTargetStateName == "Walk", "the transition's target");
			ZENITH_ASSERT_EQ(pxIdle->GetTransitions().Get(0).m_xConditions.GetSize(), 1u, "and its condition");
		}
	}

	// --- The layers -----------------------------------------------------------
	ZENITH_ASSERT_EQ(xController.GetLayerCount(), 2u, "both layers were built");
	if (xController.GetLayerCount() != 2u)
	{
		xController.ReleaseAssetReferences();
		return;
	}

	const Flux_AnimationLayer* pxLower = xController.GetLayer(0);
	ZENITH_ASSERT_TRUE(pxLower->GetName() == "Lower", "layer 0 name");
	ZENITH_ASSERT_EQ(pxLower->GetLayerId(), 0u, "layer 0 keeps its authored id");
	ZENITH_ASSERT_EQ_FLOAT(pxLower->GetWeight(), 0.75f, 1e-5f, "layer 0 weight");
	ZENITH_ASSERT_TRUE(pxLower->GetBlendMode() == LAYER_BLEND_OVERRIDE, "layer 0 blend mode");
	ZENITH_ASSERT_TRUE(pxLower->GetEmitEvents(), "layer 0 emit flag");
	ZENITH_ASSERT_TRUE(pxLower->HasAvatarMask(), "layer 0 is masked");
	// ★ THE MASK RESOLVED BY NAME ONTO THIS RIG. Root is index 0 and Arm is index
	// 2; the two masks deliberately name different bones, so a build that mixed
	// them up (or applied one mask to both layers) fails here rather than looking
	// plausible.
	ZENITH_ASSERT_EQ_FLOAT(pxLower->GetAvatarMask().GetBoneWeight(0u), 1.0f, 1e-5f, "layer 0 mask: Root -> 1.0");
	ZENITH_ASSERT_EQ_FLOAT(pxLower->GetAvatarMask().GetBoneWeight(2u), 0.0f, 1e-5f, "layer 0 mask: Arm untouched");
	ZENITH_ASSERT_TRUE(pxLower->GetStateMachinePtr() != nullptr && pxLower->GetStateMachinePtr()->GetName() == "LowerGraph",
		"layer 0 has its OWN state machine, from its OWN embedded def");

	const Flux_AnimationLayer* pxUpper = xController.GetLayer(1);
	ZENITH_ASSERT_TRUE(pxUpper->GetName() == "Upper", "layer 1 name");
	ZENITH_ASSERT_EQ(pxUpper->GetLayerId(), 1u, "layer 1 keeps its authored id");
	ZENITH_ASSERT_EQ_FLOAT(pxUpper->GetWeight(), 0.25f, 1e-5f, "layer 1 weight");
	ZENITH_ASSERT_TRUE(pxUpper->GetBlendMode() == LAYER_BLEND_ADDITIVE, "layer 1 blend mode");
	// ★ THE FLAG Flux_AnimationLayer's OWN SERIALIZER DROPS. A controller restored
	// from a .zscen comes back with every layer emitting; only the .zanimctrl
	// carries the silence.
	ZENITH_ASSERT_FALSE(pxUpper->GetEmitEvents(), "layer 1 is SILENCED, and that survived the file");
	ZENITH_ASSERT_TRUE(pxUpper->HasAvatarMask(), "layer 1 is masked");
	ZENITH_ASSERT_EQ_FLOAT(pxUpper->GetAvatarMask().GetBoneWeight(2u), 0.5f, 1e-5f, "layer 1 mask: Arm -> 0.5");
	ZENITH_ASSERT_EQ_FLOAT(pxUpper->GetAvatarMask().GetBoneWeight(0u), 0.0f, 1e-5f, "layer 1 mask: Root untouched");
	ZENITH_ASSERT_TRUE(pxUpper->GetStateMachinePtr() != nullptr && pxUpper->GetStateMachinePtr()->GetName() == "UpperGraph",
		"layer 1 has its own state machine too");

	// --- The parameters were published (D42) ----------------------------------
	ZENITH_ASSERT_TRUE(xController.GetParameters().HasParameter("Speed"),
		"every attached machine's declarations reached the controller's live set");

	// A controller that can outlive the registry must give its references back.
	xController.ReleaseAssetReferences();
}

//==============================================================================
// (2) Build -> Export -> Build is a fixed point.
//==============================================================================
ZENITH_TEST(AnimatorControllerAsset, ExportControllerDefRecoversWhatBuildConsumed)
{
	AnimCtrlFixture xFixture("zenith_animctrl_export");
	AnimCtrlWriteEverything(xFixture);

	Zenith_AnimatorControllerAsset* pxAsset =
		Zenith_AssetRegistry::GetView<Zenith_AnimatorControllerAsset>(xFixture.m_strControllerPath);
	ZENITH_ASSERT_NOT_NULL(pxAsset, "the probe controller loads");
	if (pxAsset == nullptr)
	{
		return;
	}

	Zenith_SkeletonAsset xSkeleton;
	AnimCtrlBuildRig(xSkeleton);

	Flux_AnimationController xController;
	ZENITH_ASSERT_TRUE(xController.BuildFromControllerDef(pxAsset->GetDef(), &xSkeleton), "the build succeeds");

	Flux_AnimatorControllerDef xExported;
	ZENITH_ASSERT_TRUE(xController.ExportControllerDef(xExported), "the export is complete");

	ZENITH_ASSERT_EQ(xExported.GetClipPaths().GetSize(), 2u, "both clip paths came back");
	ZENITH_ASSERT_EQ(xExported.GetLayerCount(), 2u, "both layers came back");
	ZENITH_ASSERT_TRUE(xExported.HasStateMachineDef(), "and the top-level machine");
	if (xExported.GetLayerCount() != 2u)
	{
		xController.ReleaseAssetReferences();
		return;
	}

	// ★ THE MASK PATH IS THE FRAGILE ONE, AND IT IS WHY Flux_AnimationLayer CARRIES
	// IT. Flux_BoneMask is resolved and index-based, so nothing in the runtime layer
	// could reproduce the file name; without the path stored beside it, this save
	// would silently unmask every layer.
	ZENITH_ASSERT_TRUE(xExported.GetLayer(0)->GetBoneMaskAssetPath() == xFixture.m_strLowerMaskPath,
		"layer 0's mask reference survived the round trip");
	ZENITH_ASSERT_TRUE(xExported.GetLayer(1)->GetBoneMaskAssetPath() == xFixture.m_strUpperMaskPath,
		"layer 1's mask reference survived the round trip");

	ZENITH_ASSERT_EQ(xExported.GetLayer(0)->GetLayerId(), 0u, "layer ids are re-stated, not renumbered");
	ZENITH_ASSERT_EQ(xExported.GetLayer(1)->GetLayerId(), 1u, "layer ids are re-stated, not renumbered");
	ZENITH_ASSERT_FALSE(xExported.GetLayer(1)->GetEmitEvents(), "and the silenced layer is still silent");
	ZENITH_ASSERT_TRUE(xExported.GetLayer(1)->GetBlendMode() == LAYER_BLEND_ADDITIVE, "and additive is still additive");
	ZENITH_ASSERT_TRUE(xExported.GetLayer(0)->GetStateMachineDef().HasState("Walk"),
		"and each layer's embedded graph came with it");

	xController.ReleaseAssetReferences();
}

//==============================================================================
// (3) A dangling mask reference FAILS LOUDLY.
//==============================================================================
ZENITH_TEST(AnimatorControllerAsset, ADanglingMaskReferenceFailsLoudly)
{
	// ★ WHY THIS MUST NOT BE A SILENT SKIP. An OVERRIDE layer with no mask replaces
	// the WHOLE skeleton, so a lost mask does not make the layer do less — it makes
	// it do everything, and the symptom is "the legs stopped animating" with no
	// error anywhere.
	AnimCtrlFixture xFixture("zenith_animctrl_dangling_mask");
	AnimCtrlWriteClipFile(xFixture.m_strIdleClipPath, "Idle");
	AnimCtrlWriteClipFile(xFixture.m_strWalkClipPath, "Walk");
	AnimCtrlWriteMaskFile(xFixture.m_strLowerMaskPath, "Root", 1.0f);
	// ...and DELIBERATELY no Upper.zanimmask on disk.

	Zenith_AnimatorControllerAsset xAsset;
	AnimCtrlAuthorControllerDef(xAsset.GetDef(), xFixture);

	Zenith_SkeletonAsset xSkeleton;
	AnimCtrlBuildRig(xSkeleton);

	Flux_AnimationController xController;
	// A missing FILE is reported, not asserted (it is a data condition, not a
	// programming error), so no capture scope is needed here — the bool IS the
	// signal, which is the point.
	ZENITH_ASSERT_FALSE(xController.BuildFromControllerDef(xAsset.GetDef(), &xSkeleton),
		"a mask reference that does not resolve makes the whole build incomplete");

	// The rest of the build still happened, so the failure is reportable rather
	// than destructive — and the layer that COULD be masked still is.
	ZENITH_ASSERT_EQ(xController.GetLayerCount(), 2u, "both layers still exist");
	if (xController.GetLayerCount() == 2u)
	{
		ZENITH_ASSERT_TRUE(xController.GetLayer(0)->HasAvatarMask(), "the resolvable mask was applied");
		ZENITH_ASSERT_FALSE(xController.GetLayer(1)->HasAvatarMask(), "the dangling one left the layer unmasked");
	}

	// The same def with NO skeleton to resolve against is the second way a mask can
	// go missing, and it reports the same way.
	Flux_AnimationController xNoRig;
	ZENITH_ASSERT_FALSE(xNoRig.BuildFromControllerDef(xAsset.GetDef(), nullptr),
		"a masked def built with no skeleton is incomplete too");

	xController.ReleaseAssetReferences();
	xNoRig.ReleaseAssetReferences();
}

//==============================================================================
// (4) A hand-masked layer cannot be exported, and says so.
//==============================================================================
ZENITH_TEST(AnimatorControllerAsset, AMaskedLayerWithNoAssetPathExportsUnmaskedAndReportsIt)
{
	Zenith_SkeletonAsset xSkeleton;
	AnimCtrlBuildRig(xSkeleton);

	Flux_AnimationController xController;
	Flux_AnimationLayer* pxLayer = xController.AddLayer("HandMasked");
	Flux_BoneMask xMask;
	xMask.SetBoneWeight(1u, 1.0f);
	pxLayer->SetAvatarMask(xMask);   // ...and no SetBoneMaskAssetPath

	Flux_AnimatorControllerDef xExported;
	// ★ FALSE, BECAUSE THERE IS NOTHING TO WRITE. Flux_BoneMask holds resolved
	// indices and no provenance, so an export cannot invent the .zanimmask this
	// mask would have come from. Reporting it is the difference between a save that
	// is known to be lossy and one that quietly unmasks a character.
	ZENITH_ASSERT_FALSE(xController.ExportControllerDef(xExported),
		"a mask with no asset path makes the export incomplete");
	ZENITH_ASSERT_EQ(xExported.GetLayerCount(), 1u, "the layer is still exported");
	ZENITH_ASSERT_TRUE(xExported.GetLayer(0)->GetBoneMaskAssetPath().empty(), "...but unmasked");

	xController.ReleaseAssetReferences();
}

//==============================================================================
// (5) A refused .zanimctrl is not a successful load.
//==============================================================================
ZENITH_TEST(AnimatorControllerAsset, ARefusedZanimctrlIsNotASuccessfulLoad)
{
	AnimCtrlFixture xFixture("zenith_animctrl_refusal");

	// Four bytes: too short for an envelope at all.
	{
		Zenith_DataStream xStream;
		const u_int uGarbage = 0xDEADBEEFu;
		xStream << uGarbage;
		xStream.WriteToFile(xFixture.m_strControllerPath.c_str());

		Zenith_AssertCaptureScope xCapture;
		ZENITH_ASSERT_NULL(Zenith_AssetRegistry::GetView<Zenith_AnimatorControllerAsset>(xFixture.m_strControllerPath),
			"a truncated .zanimctrl must NOT resolve to an asset");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "the refusal asserts exactly once");
	}

	// A well-formed file from a newer tool.
	{
		Zenith_AnimatorControllerAsset xAsset;
		AnimCtrlAuthorControllerDef(xAsset.GetDef(), xFixture);
		Zenith_DataStream xStream;
		xAsset.GetDef().WriteToDataStream(xStream);

		// The schema word is the FOURTH u_int of Zenith_StreamHeader.
		const u_int uFutureSchema = uZENITH_ANIMCTRL_SCHEMA_CURRENT + 1u;
		std::memcpy(static_cast<uint8_t*>(xStream.GetData()) + (3 * sizeof(u_int)), &uFutureSchema, sizeof(u_int));
		xStream.WriteToFile(xFixture.m_strControllerPath.c_str());

		Zenith_AssertCaptureScope xCapture;
		ZENITH_ASSERT_NULL(Zenith_AssetRegistry::GetView<Zenith_AnimatorControllerAsset>(xFixture.m_strControllerPath),
			"a future-schema .zanimctrl must NOT resolve to an asset");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "the refusal asserts exactly once");
	}

	// And a good file at the same path DOES load, so the refusals are about the
	// BYTES and not about the fixture.
	AnimCtrlWriteEverything(xFixture);
	Zenith_AnimatorControllerAsset* pxAsset =
		Zenith_AssetRegistry::GetView<Zenith_AnimatorControllerAsset>(xFixture.m_strControllerPath);
	ZENITH_ASSERT_NOT_NULL(pxAsset, "a well-formed .zanimctrl at the same path loads");
	if (pxAsset != nullptr)
	{
		ZENITH_ASSERT_EQ(pxAsset->GetDef().GetLayerCount(), 2u, "with its layers");
		ZENITH_ASSERT_TRUE(pxAsset->GetDef().HasStateMachineDef(), "and its top-level machine");
	}
}
