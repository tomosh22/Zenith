//------------------------------------------------------------------------------
// Zenith_AnimatorComponent unit tests — the CONTROLLER-ASSET PATH (C2).
// Included at the bottom of Zenith_AnimatorComponent.cpp.
//
// ★ WHAT THIS FILE EXISTS TO CATCH. The path is a single std::string, and every
// way it can be lost is silent: a move operation that forgets it empties it on an
// unrelated component's removal (the pool relocates by move-construction), a
// writer that forgets it makes every reloaded entity anonymous, and a reader that
// accepts a schema-1 record reads the NEXT component's bytes as a path. None of
// those changes a single rendered pixel or trips any assert — the animation keeps
// playing from the inline scene bytes, and the only symptom is a tool that cannot
// say where a graph came from.
//
// Headless throughout: real .zanim / .zanimctrl files in the OS temp directory,
// entities in throwaway additive scenes, no device, no skeleton. Not
// requiresGraphics.
//
// ★ FIXTURE FIRST, SCENE SECOND, in every test that has both. Locals destruct in
// reverse order, so the temp scene must be the LATER declaration: unloading it
// destroys the components, which destroys the store-owned controllers, which is
// what releases the clip handles the fixture's ForceUnload then invalidates.
// Declared the other way round, ForceUnload would delete assets live controllers
// still point at.
//
// Public API only. Zenith_AnimatorComponent grants friendship to
// Zenith_UnitTests, not to free functions, and these bodies are free — the path
// is reachable through GetControllerAssetPath() and nothing here needs more.
//------------------------------------------------------------------------------

#include "UnitTests/Zenith_UnitTests.h"
#include "UnitTests/Zenith_TempScene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "Collections/Zenith_Vector.h"
#include "DataStream/Zenith_DataStream.h"
#include "EntityComponent/Components/Zenith_AnimatorComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "AssetHandling/Zenith_AnimatorControllerAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "Flux/MeshAnimation/Flux_AnimationClip.h"
#include "Flux/MeshAnimation/Flux_AnimationController.h"   // Flux_AnimationStateMachineDef / Flux_BlendTreeNode_Clip

#include <cstdio>       // snprintf — the growth loop's filler names
#include <filesystem>
#include <string>

namespace
{
	//--------------------------------------------------------------------------
	// Fixture: a temp directory holding one clip and two controllers — one that
	// builds completely, one that names a bone mask nothing can resolve.
	//--------------------------------------------------------------------------
	struct AnimCompFixture
	{
		std::filesystem::path m_xDirectory;
		std::string m_strClipPath;
		std::string m_strControllerPath;
		std::string m_strMaskedControllerPath;
		std::string m_strMissingControllerPath;
		std::string m_strDanglingMaskPath;

		explicit AnimCompFixture(const char* szLeafDirectory)
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

			m_strClipPath              = (m_xDirectory / "Idle.zanim").generic_string();
			m_strControllerPath        = (m_xDirectory / "probe.zanimctrl").generic_string();
			m_strMaskedControllerPath  = (m_xDirectory / "masked.zanimctrl").generic_string();
			// DELIBERATELY never written — the two references that must not resolve.
			m_strMissingControllerPath = (m_xDirectory / "missing.zanimctrl").generic_string();
			m_strDanglingMaskPath      = (m_xDirectory / "Upper.zanimmask").generic_string();
		}

		~AnimCompFixture()
		{
			// Unconditional: a no-op for a path that was never cached, and what keeps
			// the live registry the suite runs inside undisturbed by throwaway assets.
			Zenith_AssetRegistry::ForceUnload(m_strClipPath);
			Zenith_AssetRegistry::ForceUnload(m_strControllerPath);
			Zenith_AssetRegistry::ForceUnload(m_strMaskedControllerPath);
			std::error_code xError;
			std::filesystem::remove_all(m_xDirectory, xError);
		}

		AnimCompFixture(const AnimCompFixture&) = delete;
		AnimCompFixture& operator=(const AnimCompFixture&) = delete;
	};

	// A one-channel clip on disk. The pose is irrelevant here; what matters is that
	// the controller def names a clip that genuinely LOADS, so a failed build can
	// only be the mask.
	void AnimCompWriteClipFile(const std::string& strPath)
	{
		Flux_AnimationClip xClip;
		xClip.SetName("Idle");
		xClip.SetDuration(1.0f);
		xClip.SetLooping(true);

		Flux_BoneChannel xChannel;
		xChannel.SetBoneName("Root");
		xChannel.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
		xClip.AddBoneChannel("Root", std::move(xChannel));

		xClip.Export(strPath);
	}

	// One state whose tree is a named clip leaf — the shape a def carries (a clip
	// POINTER is resolved against a live collection and is not authored data).
	void AnimCompAuthorGraph(Flux_AnimationStateMachineDef& xDef, const char* szName)
	{
		xDef.SetName(szName);
		Flux_AnimationState* pxState = xDef.AddState("Idle");
		Flux_BlendTreeNode_Clip* pxLeaf = new Flux_BlendTreeNode_Clip();
		pxLeaf->SetClipName("Idle");
		pxState->SetBlendTree(pxLeaf);
		xDef.SetDefaultState("Idle");
	}

	// ★ NO LAYERS, AND THAT IS LOAD-BEARING. A layer with a bone mask resolves
	// against the rig the entity's ModelComponent animates, and these entities have
	// no ModelComponent — so a masked def could never build completely here, and
	// the round-trip test would be asserting on the failure path by accident.
	void AnimCompWriteControllerFile(const AnimCompFixture& xFixture)
	{
		AnimCompWriteClipFile(xFixture.m_strClipPath);

		Zenith_AnimatorControllerAsset xAsset;
		xAsset.GetDef().SetName("Probe");
		xAsset.GetDef().AddClipPath(xFixture.m_strClipPath);
		AnimCompAuthorGraph(xAsset.GetDef().GetOrCreateStateMachineDef(), "Base");
		xAsset.Export(xFixture.m_strControllerPath);
	}

	// The same controller plus one layer naming a .zanimmask that is not on disk.
	// TWO independent reasons this build comes back incomplete — the file is absent
	// AND there is no skeleton to resolve it against — and the test below cares only
	// that the verdict is false while the path is still recorded.
	void AnimCompWriteMaskedControllerFile(const AnimCompFixture& xFixture)
	{
		AnimCompWriteClipFile(xFixture.m_strClipPath);

		Zenith_AnimatorControllerAsset xAsset;
		xAsset.GetDef().SetName("MaskedProbe");
		xAsset.GetDef().AddClipPath(xFixture.m_strClipPath);
		AnimCompAuthorGraph(xAsset.GetDef().GetOrCreateStateMachineDef(), "Base");

		Flux_AnimatorControllerLayerDef* pxLayer = xAsset.GetDef().AddLayer("Upper");
		pxLayer->SetBoneMaskAssetPath(xFixture.m_strDanglingMaskPath);
		AnimCompAuthorGraph(pxLayer->GetStateMachineDef(), "UpperGraph");

		xAsset.Export(xFixture.m_strMaskedControllerPath);
	}

	//--------------------------------------------------------------------------
	// Scene-stream framing, for the schema-refusal test.
	//
	// ★ DUPLICATED, NOT CALLED. The equivalents in Core/Zenith_UnitTests.Tests.inl
	// live in another translation unit inside an anonymous namespace, so they have
	// internal linkage and are unreachable from here. Copying them is the only
	// option that does not push a test-only helper into a shared header.
	//--------------------------------------------------------------------------

	// One [typeName][schemaVersion][size][payload] component record, framed exactly
	// as Zenith_ComponentMetaRegistry::SerializeEntityComponents writes it (scene v6+).
	void AnimCompWriteComponentRecord(Zenith_DataStream& xStream, const std::string& strTypeName,
		u_int uStampedSchemaVersion, const void* pPayload, u_int uPayloadSize)
	{
		xStream << strTypeName;
		xStream << uStampedSchemaVersion;
		xStream << uPayloadSize;
		if (uPayloadSize > 0u)
		{
			xStream.WriteData(pPayload, uPayloadSize);
		}
	}

	// A real Transform payload, captured from a LIVE component so the fixture cannot
	// drift from the Transform's field layout.
	u_int AnimCompCaptureTransformPayload(Zenith_SceneData* pxSceneData,
		const Zenith_Maths::Vector3& xPos, Zenith_DataStream& xOut)
	{
		Zenith_Entity xScratch = g_xEngine.Scenes().CreateEntity(pxSceneData, "AnimCompPayloadScratch");
		xScratch.GetComponent<Zenith_TransformComponent>().SetPosition(xPos);
		xScratch.GetComponent<Zenith_TransformComponent>().WriteToDataStream(xOut);
		return static_cast<u_int>(xOut.GetCursor());
	}

	// Frames a scene v7 stream: entity "A" carrying ONE Animator component stamped
	// with uStampedSchema over an arbitrary payload, then entity "B" carrying a
	// Transform. B comes AFTER A on purpose — it is the evidence that the load
	// continued past the refusal. Rewinds the cursor to 0.
	//
	// The version is the LITERAL 7, not uSCENE_VERSION_CURRENT: this hand-framed
	// stream writes the v7 entity record ([fileIndex][name][parentFileIndex]), and a
	// future version bump that changes that layout must not silently relabel these
	// bytes as something they are not.
	void AnimCompBuildAThenB(Zenith_DataStream& xStream, u_int uStampedSchema,
		const void* pTransformPayload, u_int uTransformPayloadSize)
	{
		xStream << (u_int)Zenith_SceneData::uSCENE_MAGIC;
		xStream << (u_int)7u;
		xStream << (u_int)2u;

		// -- Entity A: the refusing animator --
		xStream << (uint32_t)0u;                                      // file index
		xStream << std::string("A");                                  // name
		xStream << (uint32_t)Zenith_EntityID::INVALID_INDEX;          // no parent (v7 record)
		xStream << (u_int)1u;                                         // component count
		const uint32_t uArbitraryPayload = 0xD00Du;
		AnimCompWriteComponentRecord(xStream, "Animator", uStampedSchema,
			&uArbitraryPayload, (u_int)sizeof(uArbitraryPayload));

		// -- Entity B: a plain Transform, AFTER the refusal --
		xStream << (uint32_t)1u;
		xStream << std::string("B");
		xStream << (uint32_t)Zenith_EntityID::INVALID_INDEX;
		xStream << (u_int)1u;
		AnimCompWriteComponentRecord(xStream, "Transform",
			Zenith_TransformComponent::uSchemaVersion, pTransformPayload, uTransformPayloadSize);

		// Trailer: main camera file index (none).
		xStream << (uint32_t)Zenith_EntityID::INVALID_INDEX;

		xStream.SetCursor(0);
	}
}

//==============================================================================
// (1) THE ACCEPTANCE CASE. The path a load recorded survives a write/read round
//     trip into a DIFFERENT component.
//==============================================================================
ZENITH_TEST(AnimatorComponent, ControllerPathRoundTripsThroughTheStream)
{
	AnimCompFixture xFixture("zenith_animcomp_roundtrip");
	AnimCompWriteControllerFile(xFixture);
	Zenith_TempScene xScene("AnimCompRoundTrip");

	const std::string strExpected = Zenith_AssetRegistry::NormalizeAssetPath(xFixture.m_strControllerPath);
	ZENITH_ASSERT_FALSE(strExpected.empty(),
		"the fixture path normalizes to nothing — every comparison below would be vacuously true");

	Zenith_Entity xSource = xScene.CreateEntity("Source");
	Zenith_AnimatorComponent& xAnim = xSource.AddComponent<Zenith_AnimatorComponent>();
	ZENITH_ASSERT_TRUE(xAnim.GetControllerAssetPath().empty(),
		"a freshly added animator must name no controller asset");

	ZENITH_ASSERT_TRUE(xAnim.LoadControllerAsset(xFixture.m_strControllerPath),
		"an unmasked def whose only clip is on disk must build completely, with no rig");
	ZENITH_ASSERT_TRUE(xAnim.GetControllerAssetPath() == strExpected,
		"a successful load must record the NORMALIZED asset path");

	Zenith_DataStream xStream;
	xAnim.WriteToDataStream(xStream);
	xStream.SetCursor(0);

	// ★ A SECOND COMPONENT OF THE SAME TYPE INVALIDATES xAnim. Both live in the one
	// per-scene animator pool, so nothing above this line may be re-read below it.
	Zenith_Entity xTarget = xScene.CreateEntity("Target");
	Zenith_AnimatorComponent& xRestored = xTarget.AddComponent<Zenith_AnimatorComponent>();
	ZENITH_ASSERT_TRUE(xRestored.GetControllerAssetPath().empty(),
		"the restore target must start with no path, or the assert below proves nothing");

	ZENITH_ASSERT_TRUE(xRestored.ReadFromDataStream(xStream, Zenith_AnimatorComponent::uSchemaVersion),
		"a payload stamped with the schema this build writes must be accepted");
	ZENITH_ASSERT_TRUE(xRestored.GetControllerAssetPath() == strExpected,
		"the controller-asset path did not survive WriteToDataStream -> ReadFromDataStream");
}

//==============================================================================
// (2) A LOOKUP THAT FOUND NOTHING LEARNED NOTHING. The previous path stands.
//==============================================================================
ZENITH_TEST(AnimatorComponent, FailedLookupPreservesThePreviousPath)
{
	AnimCompFixture xFixture("zenith_animcomp_failed_lookup");
	AnimCompWriteControllerFile(xFixture);
	Zenith_TempScene xScene("AnimCompFailedLookup");

	const std::string strExpected = Zenith_AssetRegistry::NormalizeAssetPath(xFixture.m_strControllerPath);

	Zenith_Entity xEntity = xScene.CreateEntity("Loader");
	Zenith_AnimatorComponent& xAnim = xEntity.AddComponent<Zenith_AnimatorComponent>();
	ZENITH_ASSERT_TRUE(xAnim.LoadControllerAsset(xFixture.m_strControllerPath), "the probe controller loads");
	ZENITH_ASSERT_TRUE(xAnim.GetControllerAssetPath() == strExpected, "...and is recorded");

	// A missing FILE is reported, not asserted (Zenith_AnimatorControllerAsset::
	// LoadFromFile returns FILE_NOT_FOUND without an assert, and the registry deletes
	// the half-made asset and answers null), so no capture scope is needed here.
	ZENITH_ASSERT_FALSE(xAnim.LoadControllerAsset(xFixture.m_strMissingControllerPath),
		"a .zanimctrl that is not on disk must fail the load");

	// ★ THE CLAUSE THE FEATURE TURNS ON. Nothing was rebuilt, so the live controller
	// is still the probe's — recording the path that did not resolve would make this
	// getter name an asset the controller was never built from, and clearing it would
	// throw away the true answer.
	ZENITH_ASSERT_TRUE(xAnim.GetControllerAssetPath() == strExpected,
		"a failed asset lookup overwrote the previously recorded controller path");
}

//==============================================================================
// (3) AN INCOMPLETE BUILD STILL RECORDS THE PATH. The bool is the completeness
//     verdict; the path is "what was asked for", and a diagnostic wants both.
//==============================================================================
ZENITH_TEST(AnimatorComponent, IncompleteBuildStillRecordsThePath)
{
	AnimCompFixture xFixture("zenith_animcomp_incomplete");
	AnimCompWriteMaskedControllerFile(xFixture);
	Zenith_TempScene xScene("AnimCompIncomplete");

	const std::string strExpected = Zenith_AssetRegistry::NormalizeAssetPath(xFixture.m_strMaskedControllerPath);

	Zenith_Entity xEntity = xScene.CreateEntity("Masked");
	Zenith_AnimatorComponent& xAnim = xEntity.AddComponent<Zenith_AnimatorComponent>();

	ZENITH_ASSERT_FALSE(xAnim.LoadControllerAsset(xFixture.m_strMaskedControllerPath),
		"a layer naming a bone mask that cannot resolve must make the build incomplete");
	ZENITH_ASSERT_TRUE(xAnim.GetControllerAssetPath() == strExpected,
		"an incomplete build must STILL record the asset it was built from — recording it "
		"only on success would leave a half-built entity naming the previous asset, which is "
		"the one answer that is actively wrong");
}

//==============================================================================
// (4a) MOVE CONSTRUCTION — the operation the component pool actually uses.
//==============================================================================
ZENITH_TEST(AnimatorComponent, MoveConstructionTransfersThePathAndClearsTheSource)
{
	AnimCompFixture xFixture("zenith_animcomp_move_ctor");
	AnimCompWriteControllerFile(xFixture);
	Zenith_TempScene xScene("AnimCompMoveCtor");

	const std::string strExpected = Zenith_AssetRegistry::NormalizeAssetPath(xFixture.m_strControllerPath);

	// Stack components, so the move is forced rather than hoped for. The entity has
	// no animator in the pool; the ctor creates the store entry and the dtors of the
	// pair net to exactly one Destroy (moved-from is neutralised).
	Zenith_Entity xEntity = xScene.CreateEntity("MoveCtorEntity");
	{
		Zenith_AnimatorComponent xSource(xEntity);
		ZENITH_ASSERT_TRUE(xSource.LoadControllerAsset(xFixture.m_strControllerPath), "the probe controller loads");

		Zenith_AnimatorComponent xMoved(std::move(xSource));

		ZENITH_ASSERT_TRUE(xMoved.GetControllerAssetPath() == strExpected,
			"move construction dropped the controller-asset path — a pool relocation would "
			"silently empty it on an unrelated component's removal");
		ZENITH_ASSERT_TRUE(xSource.GetControllerAssetPath().empty(),
			"the moved-from component must be left naming nothing, not sharing the path");
	}
}

//==============================================================================
// (4b) MOVE ASSIGNMENT — not a pool path, but hand-written, so it is pinned too.
//==============================================================================
ZENITH_TEST(AnimatorComponent, MoveAssignmentTransfersThePathAndClearsTheSource)
{
	AnimCompFixture xFixture("zenith_animcomp_move_assign");
	AnimCompWriteControllerFile(xFixture);
	Zenith_TempScene xScene("AnimCompMoveAssign");

	const std::string strExpected = Zenith_AssetRegistry::NormalizeAssetPath(xFixture.m_strControllerPath);

	Zenith_Entity xEntityA = xScene.CreateEntity("MoveAssignSource");
	Zenith_Entity xEntityB = xScene.CreateEntity("MoveAssignDest");
	{
		Zenith_AnimatorComponent xSource(xEntityA);
		Zenith_AnimatorComponent xDest(xEntityB);

		ZENITH_ASSERT_TRUE(xSource.LoadControllerAsset(xFixture.m_strControllerPath), "the probe controller loads");
		ZENITH_ASSERT_TRUE(xDest.GetControllerAssetPath().empty(), "the destination starts with no path");

		xDest = std::move(xSource);

		ZENITH_ASSERT_TRUE(xDest.GetControllerAssetPath() == strExpected,
			"move assignment dropped the controller-asset path");
		ZENITH_ASSERT_TRUE(xSource.GetControllerAssetPath().empty(),
			"the move-assigned-from component must be left naming nothing");
	}
}

//==============================================================================
// (5) SWAP-AND-POP. Removing one entity's animator move-constructs the LAST
//     component into the freed slot; the survivor's path must come with it.
//==============================================================================
ZENITH_TEST(AnimatorComponent, SwapAndPopRelocationKeepsThePath)
{
	AnimCompFixture xFixture("zenith_animcomp_swap_pop");
	AnimCompWriteControllerFile(xFixture);
	Zenith_TempScene xScene("AnimCompSwapPop");

	const std::string strExpected = Zenith_AssetRegistry::NormalizeAssetPath(xFixture.m_strControllerPath);

	Zenith_Entity xFirst = xScene.CreateEntity("Reloc0");
	Zenith_Entity xSecond = xScene.CreateEntity("Reloc1");

	xFirst.AddComponent<Zenith_AnimatorComponent>();
	Zenith_AnimatorComponent& xSecondAnim = xSecond.AddComponent<Zenith_AnimatorComponent>();
	ZENITH_ASSERT_TRUE(xSecondAnim.LoadControllerAsset(xFixture.m_strControllerPath), "the probe controller loads");

	const void* pSecondBefore = xSecond.TryGetComponent<Zenith_AnimatorComponent>();
	ZENITH_ASSERT_NOT_NULL(pSecondBefore, "the second animator was not created");

	// The real relocation: the last component (entity1's) is move-constructed into
	// entity0's freed slot.
	xFirst.RemoveComponent<Zenith_AnimatorComponent>();

	Zenith_AnimatorComponent* pxSurvivor = xSecond.TryGetComponent<Zenith_AnimatorComponent>();
	ZENITH_ASSERT_NOT_NULL(pxSurvivor, "the surviving animator vanished with the removal");
	if (pxSurvivor == nullptr)
	{
		return;
	}

	// ★ ASSERTED, NOT ASSUMED. If the component never moved, the clause below would
	// pass while exercising nothing.
	ZENITH_ASSERT_NE(static_cast<const void*>(pxSurvivor), pSecondBefore,
		"the survivor did not change address — no swap-and-pop happened, so this test "
		"proved NOTHING about move construction");
	ZENITH_ASSERT_TRUE(pxSurvivor->GetControllerAssetPath() == strExpected,
		"a swap-and-pop relocation lost the controller-asset path");
}

//==============================================================================
// (6) POOL GROWTH. Every component in the pool is move-constructed into new
//     storage; every path must arrive intact.
//==============================================================================
ZENITH_TEST(AnimatorComponent, GrowRelocationKeepsEveryPath)
{
	AnimCompFixture xFixture("zenith_animcomp_grow");
	AnimCompWriteControllerFile(xFixture);
	Zenith_TempScene xScene("AnimCompGrow");

	const std::string strExpected = Zenith_AssetRegistry::NormalizeAssetPath(xFixture.m_strControllerPath);

	Zenith_Vector<Zenith_Entity> axEntities;

	Zenith_Entity xFirst = xScene.CreateEntity("GrowFirst");
	xFirst.AddComponent<Zenith_AnimatorComponent>().LoadControllerAsset(xFixture.m_strControllerPath);
	axEntities.PushBack(xFirst);

	// Re-resolved through the ENTITY every time, never cached: the pool is about to
	// move the component out from under any pointer we kept.
	const void* pFirstAddress = xFirst.TryGetComponent<Zenith_AnimatorComponent>();
	ZENITH_ASSERT_NOT_NULL(pFirstAddress, "the first animator was not created");

	// ★ NEVER A HARD-CODED COUNT. The pool's initial capacity and growth factor are
	// its business; what this test needs is the OBSERVED moment the first component
	// changes address, whenever that is.
	bool bRelocated = false;
	for (u_int u = 0u; u < 256u && !bRelocated; ++u)
	{
		char acName[32];
		snprintf(acName, sizeof(acName), "GrowFiller%u", u);
		Zenith_Entity xFiller = xScene.CreateEntity(acName);
		xFiller.AddComponent<Zenith_AnimatorComponent>().LoadControllerAsset(xFixture.m_strControllerPath);
		axEntities.PushBack(xFiller);

		const void* pNow = xFirst.TryGetComponent<Zenith_AnimatorComponent>();
		ZENITH_ASSERT_NOT_NULL(pNow, "the first animator vanished during growth");
		bRelocated = (pNow != pFirstAddress);
	}

	ZENITH_ASSERT_TRUE(bRelocated,
		"the animator pool never relocated in 256 additions — this test proved NOTHING "
		"about the move constructor. Raise the count or check the pool's growth policy "
		"rather than deleting the clause");

	// One assert for the sweep, so a regression names the count rather than drowning
	// the report in one failure per component.
	u_int uMismatches = 0u;
	for (u_int u = 0u; u < axEntities.GetSize(); ++u)
	{
		const Zenith_AnimatorComponent* pxAnim = axEntities.Get(u).TryGetComponent<Zenith_AnimatorComponent>();
		if (pxAnim == nullptr || pxAnim->GetControllerAssetPath() != strExpected)
		{
			++uMismatches;
		}
	}
	ZENITH_ASSERT_EQ(uMismatches, 0u,
		"pool growth lost the controller-asset path on at least one relocated animator");
}

//==============================================================================
// (7) THE REFUSAL. A schema-1 Animator record is refused without a byte read,
//     the load reports false — and the entity AFTER it still loads.
//==============================================================================
ZENITH_TEST(AnimatorComponent, SchemaOneRecordIsRefusedAndTheNextEntityStillLoads)
{
	Zenith_TempScene xScratch("AnimCompRefusalScratch");

	const Zenith_Maths::Vector3 xKnownPos(2.25f, -4.5f, 8.0f);
	Zenith_DataStream xTransformPayload;
	const u_int uTransformPayloadSize =
		AnimCompCaptureTransformPayload(xScratch.Data(), xKnownPos, xTransformPayload);
	ZENITH_ASSERT_GT(uTransformPayloadSize, 0u, "the captured Transform payload is empty");

	// Stamp schema 1 — the animator only reads 2, so its reader refuses. There is no
	// migration branch by design: this repo keeps no legacy read path.
	Zenith_DataStream xStream;
	AnimCompBuildAThenB(xStream, 1u, xTransformPayload.GetData(), uTransformPayloadSize);

	Zenith_TempScene xLoad("AnimCompRefusalLoad");
	ZENITH_ASSERT_FALSE(xLoad.Data()->LoadFromDataStream(xStream),
		"a refused Animator payload must make LoadFromDataStream report false");

	// RECORD AND CONTINUE, part 1: the SECOND entity still loaded, intact. A false
	// alone would also be produced by an early return, which is precisely the design
	// the refusal channel rejects.
	Zenith_Entity xB = xLoad.Data()->FindEntityByName("B");
	ZENITH_ASSERT_TRUE(xB.IsValid(), "the entity AFTER the refusing one must still load");
	ZENITH_ASSERT_TRUE(xB.HasComponent<Zenith_TransformComponent>(), "...and must still have its Transform");
	Zenith_Maths::Vector3 xLoadedPos;
	xB.GetComponent<Zenith_TransformComponent>().GetPosition(xLoadedPos);
	ZENITH_ASSERT_EQ(xLoadedPos, xKnownPos, "...whose payload must be untouched by the refusal");

	// RECORD AND CONTINUE, part 2: entity A exists, its animator was added by the
	// deserialize wrapper, and it names NOTHING — the reader refused in front of the
	// payload rather than consuming an arbitrary blob as a path.
	Zenith_Entity xA = xLoad.Data()->FindEntityByName("A");
	ZENITH_ASSERT_TRUE(xA.IsValid(), "the refusing entity itself must still be created");
	if (xA.IsValid() && xA.HasComponent<Zenith_AnimatorComponent>())
	{
		ZENITH_ASSERT_TRUE(xA.GetComponent<Zenith_AnimatorComponent>().GetControllerAssetPath().empty(),
			"the refused record left a path behind — the reader consumed payload bytes it "
			"had already decided it could not read");
	}
}
