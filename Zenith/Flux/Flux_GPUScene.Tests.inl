#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/Flux_GPUScene.h"
#include "Flux/Flux_ModelInstance.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "Flux/Flux_RendererImpl.h"   // Flux_ExternalSceneItem + the pure external-item classifier/router + the pull seam

// ============================================================================
// Flux GPU-scene Stage-0 unit tests (unified GPU-driven opaque-mesh pipeline).
//
// Pure CPU: the GPU-scene record packers, the snapshot->records builder, and the
// (mesh,cull,material,VAT) bucket registry's refcount-diff topology sync. No GPU,
// no renderer boot — buckets and records are built from POD source descriptors.
// ============================================================================

namespace
{
	Flux_GPUSceneSourceSubmesh GPUScene_MakeSub(u_int uMesh, u_int uCull, u_int64 ulMat, u_int64 ulVat)
	{
		Flux_GPUSceneSourceSubmesh xSub;
		xSub.m_uMeshGeometryId    = uMesh;
		xSub.m_uCullMode          = uCull;
		xSub.m_ulMaterialAssetId  = ulMat;
		xSub.m_ulVATTextureId     = ulVat;
		xSub.m_xLocalBoundsSphere = Zenith_Maths::Vector4(0.0f, 0.0f, 0.0f, 1.0f);
		return xSub;
	}

	// Append one item (caller has Reserve()d enough that this never reallocates,
	// so the returned reference stays valid while submeshes are pushed onto it).
	Flux_GPUSceneSourceItem& GPUScene_AddItem(Zenith_Vector<Flux_GPUSceneSourceItem>& xItems,
		const Zenith_Maths::Matrix4& xWorld)
	{
		xItems.EmplaceBack();
		Flux_GPUSceneSourceItem& xItem = xItems.Get(xItems.GetSize() - 1u);
		xItem.m_xWorldMatrix = xWorld;
		return xItem;
	}
}

// ---- pack helpers ----------------------------------------------------------

ZENITH_TEST(GPUScene, PackObjectSetsFieldsAndZeroesVATForStatics)
{
	Zenith_Maths::Matrix4 xModel(1.0f);
	xModel[3] = Zenith_Maths::Vector4(3.0f, 4.0f, 5.0f, 1.0f);   // translation column

	// Static object: VAT params default to 0 — byte-identical to the old zeroed pads, so the
	// VS skips VAT and the golden hash of a static scene is unchanged from Stage 0.
	Flux_GPUSceneObject xObj;
	Flux_BuildGPUSceneObject(xObj, xModel, 0x5u, 17u);

	ZENITH_ASSERT_EQ(xObj.m_uFlags, 0x5u, "object flags must round-trip");
	ZENITH_ASSERT_EQ(xObj.m_uBonePaletteRef, 17u, "bone-palette ref must round-trip");
	ZENITH_ASSERT_EQ(xObj.m_uVATAnimPacked, 0u, "static object has no VAT anim (zeroed)");
	ZENITH_ASSERT_EQ(xObj.m_uVATAnimTime, 0u, "static object has no VAT time (zeroed)");
	ZENITH_ASSERT_EQ_FLOAT(xObj.m_xModelMatrix[3].x, 3.0f, 0.0001f, "model translation X must round-trip");
	ZENITH_ASSERT_EQ_FLOAT(xObj.m_xModelMatrix[3].z, 5.0f, 0.0001f, "model translation Z must round-trip");
}

ZENITH_TEST(GPUScene, PackObjectCarriesVATAnimFields)
{
	// VAT (foliage) object: animIndex/frameCount packed into one word, time bits in another,
	// and the VAT flag set so the VS samples the animation texture.
	const u_int uPacked = Flux_PackVATAnim(/*animIndex*/ 3u, /*frameCount*/ 120u);
	ZENITH_ASSERT_EQ(uPacked & 0xFFFFu, 3u, "low 16 bits = anim index");
	ZENITH_ASSERT_EQ((uPacked >> 16) & 0xFFFFu, 120u, "high 16 bits = frame count");

	const float fTime = 0.25f;
	u_int uTimeBits = 0u;
	static_assert(sizeof(float) == sizeof(u_int), "VAT time bit-reinterpret assumes 4-byte float");
	memcpy(&uTimeBits, &fTime, sizeof(uTimeBits));

	Flux_GPUSceneObject xObj;
	Flux_BuildGPUSceneObject(xObj, Zenith_Maths::Matrix4(1.0f), uFLUX_GPUSCENE_OBJFLAG_VAT, 0u, uPacked, uTimeBits);

	ZENITH_ASSERT_TRUE((xObj.m_uFlags & uFLUX_GPUSCENE_OBJFLAG_VAT) != 0u, "VAT flag set on a foliage object");
	ZENITH_ASSERT_EQ(xObj.m_uVATAnimPacked, uPacked, "packed VAT anim round-trips");
	float fRoundTrip = 0.0f;
	memcpy(&fRoundTrip, &xObj.m_uVATAnimTime, sizeof(fRoundTrip));
	ZENITH_ASSERT_EQ_FLOAT(fRoundTrip, 0.25f, 0.0001f, "VAT time bit-reinterpret round-trips");
}

ZENITH_TEST(GPUScene, PackDrawItemCarriesAllFields)
{
	Zenith_Maths::Vector4 xSphere(1.0f, 2.0f, 3.0f, 9.0f);
	Flux_GPUSceneDrawItem xDI;
	Flux_BuildGPUSceneDrawItem(xDI, 12u, 34u, 0xAABBCCDDu, 0x2u, xSphere);

	ZENITH_ASSERT_EQ(xDI.m_uObjectIndex, 12u, "objectIndex must round-trip");
	ZENITH_ASSERT_EQ(xDI.m_uBucketIndex, 34u, "bucketIndex must round-trip");
	ZENITH_ASSERT_EQ(xDI.m_uColorTintPacked, 0xAABBCCDDu, "packed tint must round-trip");
	ZENITH_ASSERT_EQ(xDI.m_uFlags, 0x2u, "draw-item flags must round-trip");
	ZENITH_ASSERT_EQ_FLOAT(xDI.m_xLocalBoundsSphere.w, 9.0f, 0.0001f, "bounds radius must round-trip");
}

// ---- builder: object/draw-item shape ---------------------------------------

ZENITH_TEST(GPUScene, BuildEmitsObjectPerItemAndDrawItemPerSubmesh)
{
	Zenith_Vector<Flux_GPUSceneSourceItem> xItems;
	xItems.Reserve(4u);

	Flux_GPUSceneSourceItem& xItemA = GPUScene_AddItem(xItems, Zenith_Maths::Matrix4(1.0f));
	xItemA.m_xSubmeshes.PushBack(GPUScene_MakeSub(1u, 0u, 10u, 0u));   // 1 submesh

	Flux_GPUSceneSourceItem& xItemB = GPUScene_AddItem(xItems, Zenith_Maths::Matrix4(1.0f));
	xItemB.m_xSubmeshes.PushBack(GPUScene_MakeSub(2u, 0u, 11u, 0u));   // 2 submeshes
	xItemB.m_xSubmeshes.PushBack(GPUScene_MakeSub(3u, 0u, 12u, 0u));

	Flux_GPUSceneBucketRegistry xRegistry;
	Flux_GPUSceneBuildResult xResult;
	Flux_BuildGPUScene(xItems, xRegistry, xResult);

	ZENITH_ASSERT_EQ(xResult.m_xObjects.GetSize(), 2u, "one GPUSceneObject per source item");
	ZENITH_ASSERT_EQ(xResult.m_xDrawItems.GetSize(), 3u, "one GPUSceneDrawItem per submesh");

	// Draw-items 0 -> object 0; draw-items 1,2 -> object 1.
	ZENITH_ASSERT_EQ(xResult.m_xDrawItems.Get(0).m_uObjectIndex, 0u, "first submesh points at object 0");
	ZENITH_ASSERT_EQ(xResult.m_xDrawItems.Get(1).m_uObjectIndex, 1u, "second item's submeshes point at object 1");
	ZENITH_ASSERT_EQ(xResult.m_xDrawItems.Get(2).m_uObjectIndex, 1u, "second item's submeshes point at object 1");
}

// ---- bucket registry: de-dup / distinctness --------------------------------

ZENITH_TEST(GPUScene, ImportedMaterialSectionsKeepTheirOwnIndexRanges)
{
	Zenith_MeshAsset xMesh;
	xMesh.AddSubmesh(0u, 6u, 0u);
	xMesh.AddSubmesh(6u, 3u, 1u);
	xMesh.AddSubmesh(9u, 6u, 0u); // disjoint ranges sharing a material must not merge
	Zenith_Vector<Flux_GPUSceneSourceItem> xItems;
	auto& xItem = GPUScene_AddItem(xItems, Zenith_Maths::Matrix4(1.0f));
	for (u_int u = 0; u < 3u; ++u)
	{
		Flux_MeshDrawSection xSection;
		ZENITH_ASSERT_TRUE(Flux_ResolveMeshDrawSection(&xMesh, 15u, u, xSection), "valid imported section");
		auto xSub = GPUScene_MakeSub(7u, 0u, 100u + xSection.m_uMaterialSlot, 0u);
		xSub.m_uFirstIndex = xSection.m_uFirstIndex;
		xSub.m_uIndexCount = xSection.m_uIndexCount;
		xItem.m_xSubmeshes.PushBack(xSub);
	}
	Flux_GPUSceneBucketRegistry xRegistry;
	Flux_GPUSceneBuildResult xOut;
	Flux_BuildGPUScene(xItems, xRegistry, xOut);
	ZENITH_ASSERT_EQ(xRegistry.GetLiveBucketCount(), 3u, "separate ranges survive even when geometry and material match");
	for (u_int u = 0; u < 3u; ++u)
	{
		const auto* pxKey = xRegistry.TryGetBucketKey(xOut.m_xDrawItems.Get(u).m_uBucketIndex);
		ZENITH_ASSERT_TRUE(pxKey != nullptr, "draw has a live bucket");
		if (!pxKey) continue;
		const auto& xExpected = xMesh.m_xSubmeshes.Get(u);
		ZENITH_ASSERT_EQ(pxKey->m_uFirstIndex, xExpected.m_uStartIndex, "first index retained");
		ZENITH_ASSERT_EQ(pxKey->m_uIndexCount, xExpected.m_uIndexCount, "section count retained");
		ZENITH_ASSERT_EQ(pxKey->m_ulMaterialAssetId, 100ull + xExpected.m_uMaterialIndex, "material slot retained");
		u_int auCommand[5];
		Flux_PackResetIndirectCommand(auCommand, pxKey->m_uIndexCount, 0u, pxKey->m_uFirstIndex);
		ZENITH_ASSERT_EQ(auCommand[0], xExpected.m_uIndexCount, "camera/cascade draws only this section");
		ZENITH_ASSERT_EQ(auCommand[2], xExpected.m_uStartIndex, "camera/cascade starts at this section");
	}
}

ZENITH_TEST(GPUScene, MeshDrawSectionsRejectInvalidRangesAndKeepLegacyMeshes)
{
	Flux_MeshDrawSection xSection;
	ZENITH_ASSERT_TRUE(Flux_ResolveMeshDrawSection(nullptr, 12u, 0u, xSection), "legacy whole mesh");
	ZENITH_ASSERT_EQ(xSection.m_uIndexCount, 12u, "legacy count unchanged");
	ZENITH_ASSERT_FALSE(Flux_ResolveMeshDrawSection(nullptr, 12u, 1u, xSection), "legacy has one section");
	Zenith_MeshAsset xMesh;
	xMesh.AddSubmesh(9u, 6u, 1u);
	xMesh.AddSubmesh(~0u - 2u, 9u, 0u);
	xMesh.AddSubmesh(0u, 0u, 0u);
	ZENITH_ASSERT_FALSE(Flux_ResolveMeshDrawSection(&xMesh, 12u, 0u, xSection), "past index buffer");
	ZENITH_ASSERT_FALSE(Flux_ResolveMeshDrawSection(&xMesh, 12u, 1u, xSection), "overflow cannot wrap to valid");
	ZENITH_ASSERT_FALSE(Flux_ResolveMeshDrawSection(&xMesh, 12u, 2u, xSection), "empty section skipped");
}

ZENITH_TEST(GPUScene, ModelMaterialSlotsAreLocalToEachMeshBinding)
{
	// Named, cached assets exercise the path-based model construction path. The
	// Null memory manager supports this tiny mesh without a graphics device.
	auto xMesh = Zenith_AssetRegistry::Create<Zenith_MeshAsset>("game:__material_section_test_mesh.zmesh");
	auto* pxMesh = xMesh.GetDirect();
	pxMesh->AddVertex({0.f, 0.f, 0.f}, {0.f, 0.f, 1.f}, {0.f, 0.f});
	pxMesh->AddVertex({1.f, 0.f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 0.f});
	pxMesh->AddVertex({0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}, {0.f, 1.f});
	pxMesh->AddTriangle(0u, 1u, 2u);
	pxMesh->AddTriangle(2u, 1u, 0u);
	pxMesh->AddSubmesh(0u, 3u, 0u);
	pxMesh->AddSubmesh(3u, 3u, 1u);
	pxMesh->ComputeBounds();
	auto xModel = Zenith_AssetRegistry::Create<Zenith_ModelAsset>();
	Zenith_Vector<MaterialHandle> xFirst, xSecond, xEmpty;
	xFirst.PushBack(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>("game:__material_section_test_0.zmtrl"));
	xFirst.PushBack(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>("game:__material_section_test_1.zmtrl"));
	xSecond.PushBack(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>("game:__material_section_test_2.zmtrl"));
	xSecond.PushBack(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>("game:__material_section_test_3.zmtrl"));
	xModel.GetDirect()->AddMesh(xMesh, xFirst);
	xModel.GetDirect()->AddMesh(xMesh, xSecond);
	xModel.GetDirect()->AddMesh(xMesh, xEmpty);
	Flux_ModelInstance* pxModel = Flux_ModelInstance::CreateFromAsset(xModel.GetDirect());
	ZENITH_ASSERT_NOT_NULL(pxModel, "real model instance created");
	if (!pxModel) return;
	ZENITH_ASSERT_EQ(pxModel->GetNumMeshes(), 3u, "all bindings loaded");
	ZENITH_ASSERT_EQ(pxModel->GetNumMaterials(), 5u, "two pairs plus blank fallback");
	ZENITH_ASSERT_EQ(pxModel->GetMeshMaterial(0u, 1u), xFirst.Get(1u).GetDirect(), "first binding slot 1");
	ZENITH_ASSERT_EQ(pxModel->GetMeshMaterial(1u, 0u), xSecond.Get(0u).GetDirect(), "second binding starts after both first materials");
	ZENITH_ASSERT_EQ(pxModel->GetMeshMaterial(1u, 1u), xSecond.Get(1u).GetDirect(), "second binding slot 1");
	ZENITH_ASSERT_NOT_NULL(pxModel->GetMeshMaterial(2u, 0u), "empty binding receives blank material");
	ZENITH_ASSERT_NULL(pxModel->GetMeshMaterial(0u, 2u), "invalid local slot cannot leak into next binding");
	ZENITH_ASSERT_NULL(pxModel->GetMeshMaterial(3u, 0u), "invalid binding rejected");
	pxModel->SetMaterial(2u, xFirst.Get(0u).GetDirect());
	ZENITH_ASSERT_EQ(pxModel->GetMeshMaterial(1u, 0u), xFirst.Get(0u).GetDirect(), "flat serialized overrides still affect correct binding");
	delete pxModel;
}

ZENITH_TEST(GPUScene, SameKeySharesOneBucketWithRefcount)
{
	Zenith_Vector<Flux_GPUSceneSourceItem> xItems;
	xItems.Reserve(4u);

	// Two items, same (mesh,cull,material,VAT) submesh -> one bucket, refcount 2.
	GPUScene_AddItem(xItems, Zenith_Maths::Matrix4(1.0f)).m_xSubmeshes.PushBack(GPUScene_MakeSub(7u, 0u, 100u, 0u));
	GPUScene_AddItem(xItems, Zenith_Maths::Matrix4(1.0f)).m_xSubmeshes.PushBack(GPUScene_MakeSub(7u, 0u, 100u, 0u));

	Flux_GPUSceneBucketRegistry xRegistry;
	Flux_GPUSceneBuildResult xResult;
	Flux_BuildGPUScene(xItems, xRegistry, xResult);

	ZENITH_ASSERT_EQ(xRegistry.GetLiveBucketCount(), 1u, "identical submeshes share one bucket");

	Flux_GPUSceneBucketKey xKey;
	xKey.m_uMeshGeometryId = 7u; xKey.m_ulMaterialAssetId = 100u;
	ZENITH_ASSERT_EQ(xRegistry.GetBucketRefcount(xKey), 2u, "bucket refcount counts referencing draw-items");
	ZENITH_ASSERT_EQ(xResult.m_xDrawItems.Get(0).m_uBucketIndex, xResult.m_xDrawItems.Get(1).m_uBucketIndex,
		"both draw-items carry the same bucket index");
}

ZENITH_TEST(GPUScene, DistinctKeyFieldsGetDistinctBuckets)
{
	Zenith_Vector<Flux_GPUSceneSourceItem> xItems;
	xItems.Reserve(2u);

	Flux_GPUSceneSourceItem& xItem = GPUScene_AddItem(xItems, Zenith_Maths::Matrix4(1.0f));
	xItem.m_xSubmeshes.PushBack(GPUScene_MakeSub(1u, 0u, 10u, 0u));    // base
	xItem.m_xSubmeshes.PushBack(GPUScene_MakeSub(2u, 0u, 10u, 0u));    // differs: mesh
	xItem.m_xSubmeshes.PushBack(GPUScene_MakeSub(1u, 1u, 10u, 0u));    // differs: cull
	xItem.m_xSubmeshes.PushBack(GPUScene_MakeSub(1u, 0u, 11u, 0u));    // differs: material
	xItem.m_xSubmeshes.PushBack(GPUScene_MakeSub(1u, 0u, 10u, 99u));   // differs: VAT

	Flux_GPUSceneBucketRegistry xRegistry;
	Flux_GPUSceneBuildResult xResult;
	Flux_BuildGPUScene(xItems, xRegistry, xResult);

	ZENITH_ASSERT_EQ(xRegistry.GetLiveBucketCount(), 5u,
		"a difference in any one key field (mesh/cull/material/VAT) yields a distinct bucket");
}

ZENITH_TEST(GPUScene, NullMaterialBlankIdGroupsAndStaysDistinctFromRealMaterial)
{
	// The adapter maps a null submesh material to the resolved blank-material id;
	// two null-material submeshes must share one bucket, distinct from a real one.
	const u_int64 ulBlankId = 1u;
	const u_int64 ulRealId  = 500u;

	Zenith_Vector<Flux_GPUSceneSourceItem> xItems;
	xItems.Reserve(2u);
	Flux_GPUSceneSourceItem& xItem = GPUScene_AddItem(xItems, Zenith_Maths::Matrix4(1.0f));
	xItem.m_xSubmeshes.PushBack(GPUScene_MakeSub(3u, 0u, ulBlankId, 0u));
	xItem.m_xSubmeshes.PushBack(GPUScene_MakeSub(3u, 0u, ulBlankId, 0u));
	xItem.m_xSubmeshes.PushBack(GPUScene_MakeSub(3u, 0u, ulRealId,  0u));

	Flux_GPUSceneBucketRegistry xRegistry;
	Flux_GPUSceneBuildResult xResult;
	Flux_BuildGPUScene(xItems, xRegistry, xResult);

	ZENITH_ASSERT_EQ(xRegistry.GetLiveBucketCount(), 2u, "two blank + one real material -> two buckets");
	ZENITH_ASSERT_EQ(xResult.m_xDrawItems.Get(0).m_uBucketIndex, xResult.m_xDrawItems.Get(1).m_uBucketIndex,
		"blank-material submeshes share a bucket");
	ZENITH_ASSERT_NE(xResult.m_xDrawItems.Get(0).m_uBucketIndex, xResult.m_xDrawItems.Get(2).m_uBucketIndex,
		"blank-material bucket is distinct from the real-material bucket");
}

// ---- bucket registry: topology diff across frames --------------------------

ZENITH_TEST(GPUScene, IdenticalResyncReportsNoTopologyChange)
{
	Zenith_Vector<Flux_GPUSceneSourceItem> xItems;
	xItems.Reserve(2u);
	Flux_GPUSceneSourceItem& xItem = GPUScene_AddItem(xItems, Zenith_Maths::Matrix4(1.0f));
	xItem.m_xSubmeshes.PushBack(GPUScene_MakeSub(1u, 0u, 10u, 0u));
	xItem.m_xSubmeshes.PushBack(GPUScene_MakeSub(2u, 0u, 11u, 0u));

	Flux_GPUSceneBucketRegistry xRegistry;
	Flux_GPUSceneBuildResult xFirst;
	Flux_BuildGPUScene(xItems, xRegistry, xFirst);
	ZENITH_ASSERT_TRUE(xFirst.m_bTopologyChanged, "first build creates buckets -> topology changed");

	Flux_GPUSceneBuildResult xSecond;
	Flux_BuildGPUScene(xItems, xRegistry, xSecond);
	ZENITH_ASSERT_FALSE(xSecond.m_bTopologyChanged, "re-syncing the same buckets must not change topology");

	// Bucket indices are stable across the identical re-sync.
	ZENITH_ASSERT_EQ(xFirst.m_xDrawItems.Get(0).m_uBucketIndex, xSecond.m_xDrawItems.Get(0).m_uBucketIndex,
		"bucket indices are stable across an identical re-sync");
	ZENITH_ASSERT_EQ(xFirst.m_xDrawItems.Get(1).m_uBucketIndex, xSecond.m_xDrawItems.Get(1).m_uBucketIndex,
		"bucket indices are stable across an identical re-sync");
}

ZENITH_TEST(GPUScene, DroppingAKeyRetiresItsBucket)
{
	Flux_GPUSceneBucketKey xKeyB;
	xKeyB.m_uMeshGeometryId = 2u; xKeyB.m_ulMaterialAssetId = 11u;

	Flux_GPUSceneBucketRegistry xRegistry;

	// Frame 1: keys A and B present.
	{
		Zenith_Vector<Flux_GPUSceneSourceItem> xItems;
		xItems.Reserve(2u);
		Flux_GPUSceneSourceItem& xItem = GPUScene_AddItem(xItems, Zenith_Maths::Matrix4(1.0f));
		xItem.m_xSubmeshes.PushBack(GPUScene_MakeSub(1u, 0u, 10u, 0u));   // A
		xItem.m_xSubmeshes.PushBack(GPUScene_MakeSub(2u, 0u, 11u, 0u));   // B
		Flux_GPUSceneBuildResult xResult;
		Flux_BuildGPUScene(xItems, xRegistry, xResult);
	}
	ZENITH_ASSERT_EQ(xRegistry.GetLiveBucketCount(), 2u, "frame 1 has two buckets");
	ZENITH_ASSERT_TRUE(xRegistry.HasBucket(xKeyB), "bucket B exists in frame 1");

	// Frame 2: only key A present -> B retires.
	{
		Zenith_Vector<Flux_GPUSceneSourceItem> xItems;
		xItems.Reserve(1u);
		GPUScene_AddItem(xItems, Zenith_Maths::Matrix4(1.0f)).m_xSubmeshes.PushBack(GPUScene_MakeSub(1u, 0u, 10u, 0u));
		Flux_GPUSceneBuildResult xResult;
		Flux_BuildGPUScene(xItems, xRegistry, xResult);
		ZENITH_ASSERT_TRUE(xResult.m_bTopologyChanged, "retiring a bucket changes topology");
	}
	ZENITH_ASSERT_EQ(xRegistry.GetLiveBucketCount(), 1u, "frame 2 retired bucket B");
	ZENITH_ASSERT_FALSE(xRegistry.HasBucket(xKeyB), "bucket B is gone after its last reference");
}

ZENITH_TEST(GPUScene, RetiredBucketSlotIsRecycled)
{
	Flux_GPUSceneBucketKey xKeyB;
	xKeyB.m_uMeshGeometryId = 2u; xKeyB.m_ulMaterialAssetId = 11u;

	Flux_GPUSceneBucketRegistry xRegistry;
	u_int uBIndex = 0u;

	// Frame 1: A and B (high-water reaches 2).
	{
		Zenith_Vector<Flux_GPUSceneSourceItem> xItems;
		xItems.Reserve(2u);
		Flux_GPUSceneSourceItem& xItem = GPUScene_AddItem(xItems, Zenith_Maths::Matrix4(1.0f));
		xItem.m_xSubmeshes.PushBack(GPUScene_MakeSub(1u, 0u, 10u, 0u));   // A
		xItem.m_xSubmeshes.PushBack(GPUScene_MakeSub(2u, 0u, 11u, 0u));   // B
		Flux_GPUSceneBuildResult xResult;
		Flux_BuildGPUScene(xItems, xRegistry, xResult);
		ZENITH_ASSERT_TRUE(xRegistry.TryGetBucketIndex(xKeyB, uBIndex), "B has a bucket index in frame 1");
	}
	ZENITH_ASSERT_EQ(xRegistry.GetHighWaterBucketSlots(), 2u, "two buckets -> high-water 2");

	// Frame 2: drop B (slot freed).
	{
		Zenith_Vector<Flux_GPUSceneSourceItem> xItems;
		xItems.Reserve(1u);
		GPUScene_AddItem(xItems, Zenith_Maths::Matrix4(1.0f)).m_xSubmeshes.PushBack(GPUScene_MakeSub(1u, 0u, 10u, 0u));
		Flux_GPUSceneBuildResult xResult;
		Flux_BuildGPUScene(xItems, xRegistry, xResult);
	}

	// Frame 3: add a brand-new key C -> must recycle B's freed slot (no growth).
	Flux_GPUSceneBucketKey xKeyC;
	xKeyC.m_uMeshGeometryId = 9u; xKeyC.m_ulMaterialAssetId = 77u;
	{
		Zenith_Vector<Flux_GPUSceneSourceItem> xItems;
		xItems.Reserve(2u);
		Flux_GPUSceneSourceItem& xItem = GPUScene_AddItem(xItems, Zenith_Maths::Matrix4(1.0f));
		xItem.m_xSubmeshes.PushBack(GPUScene_MakeSub(1u, 0u, 10u, 0u));   // A
		xItem.m_xSubmeshes.PushBack(GPUScene_MakeSub(9u, 0u, 77u, 0u));   // C
		Flux_GPUSceneBuildResult xResult;
		Flux_BuildGPUScene(xItems, xRegistry, xResult);
	}

	u_int uCIndex = 0u;
	ZENITH_ASSERT_TRUE(xRegistry.TryGetBucketIndex(xKeyC, uCIndex), "C has a bucket index in frame 3");
	ZENITH_ASSERT_EQ(xRegistry.GetHighWaterBucketSlots(), 2u, "recycled slot -> high-water does not grow");
	ZENITH_ASSERT_EQ(uCIndex, uBIndex, "new bucket C recycled bucket B's freed slot");
}

ZENITH_TEST(GPUScene, DistinctMaterialRecyclingASlotStillFlagsTopologyChange)
{
	// Anti-aliasing guard: when a bucket retires and a DISTINCT-material key recycles
	// its freed slot, the sync must still report a topology change (so Stage 1 issues a
	// RequestGraphRebuild) — proving a recycled slot never silently aliases a different
	// material/mesh into a stale bucket. This is the executable form of the
	// "pointer churn is benign because retire+create both flag topology" reasoning.
	Flux_GPUSceneBucketRegistry xRegistry;

	// Frame 1: (mesh 1, material 10).
	{
		Zenith_Vector<Flux_GPUSceneSourceItem> xItems;
		xItems.Reserve(1u);
		GPUScene_AddItem(xItems, Zenith_Maths::Matrix4(1.0f)).m_xSubmeshes.PushBack(GPUScene_MakeSub(1u, 0u, 10u, 0u));
		Flux_GPUSceneBuildResult xResult;
		Flux_BuildGPUScene(xItems, xRegistry, xResult);
	}

	// Frame 2: empty scene -> the bucket retires (its slot is freed).
	{
		Zenith_Vector<Flux_GPUSceneSourceItem> xItems;
		Flux_GPUSceneBuildResult xResult;
		Flux_BuildGPUScene(xItems, xRegistry, xResult);
		ZENITH_ASSERT_TRUE(xResult.m_bTopologyChanged, "retiring the only bucket changes topology");
		ZENITH_ASSERT_EQ(xRegistry.GetLiveBucketCount(), 0u, "no live buckets after the scene empties");
	}

	// Frame 3: (mesh 1, material 99) — same mesh, DIFFERENT material. It recycles the
	// freed slot but is a brand-new key, so topology MUST change (not a silent alias).
	{
		Zenith_Vector<Flux_GPUSceneSourceItem> xItems;
		xItems.Reserve(1u);
		GPUScene_AddItem(xItems, Zenith_Maths::Matrix4(1.0f)).m_xSubmeshes.PushBack(GPUScene_MakeSub(1u, 0u, 99u, 0u));
		Flux_GPUSceneBuildResult xResult;
		Flux_BuildGPUScene(xItems, xRegistry, xResult);
		ZENITH_ASSERT_TRUE(xResult.m_bTopologyChanged,
			"a distinct material recycling a freed slot must flag a topology change (no silent alias)");
		ZENITH_ASSERT_EQ(xRegistry.GetLiveBucketCount(), 1u, "the new material's bucket is live");
	}

	// And the old material's key is genuinely gone (not aliased onto the recycled slot).
	Flux_GPUSceneBucketKey xOldKey;
	xOldKey.m_uMeshGeometryId = 1u; xOldKey.m_ulMaterialAssetId = 10u;
	ZENITH_ASSERT_FALSE(xRegistry.HasBucket(xOldKey), "the retired material's bucket did not silently survive");
}

// ---- golden FNV-1a hash: determinism + sensitivity -------------------------

ZENITH_TEST(GPUScene, RecordHashIsDeterministic)
{
	Zenith_Vector<Flux_GPUSceneSourceItem> xItems;
	xItems.Reserve(2u);
	Flux_GPUSceneSourceItem& xItem = GPUScene_AddItem(xItems, Zenith_Maths::Matrix4(1.0f));
	xItem.m_xSubmeshes.PushBack(GPUScene_MakeSub(1u, 0u, 10u, 0u));
	xItem.m_xSubmeshes.PushBack(GPUScene_MakeSub(2u, 1u, 11u, 5u));

	Flux_GPUSceneBucketRegistry xRegA, xRegB;
	Flux_GPUSceneBuildResult xResA, xResB;
	Flux_BuildGPUScene(xItems, xRegA, xResA);
	Flux_BuildGPUScene(xItems, xRegB, xResB);

	ZENITH_ASSERT_EQ(Flux_HashGPUSceneForTest(xResA), Flux_HashGPUSceneForTest(xResB),
		"identical input must produce identical record hashes");
}

ZENITH_TEST(GPUScene, RecordHashIsSensitiveToContent)
{
	auto BuildHash = [](u_int uColorTint, float fTz) -> u_int64
	{
		Zenith_Vector<Flux_GPUSceneSourceItem> xItems;
		xItems.Reserve(1u);
		Zenith_Maths::Matrix4 xWorld(1.0f);
		xWorld[3] = Zenith_Maths::Vector4(0.0f, 0.0f, fTz, 1.0f);
		Flux_GPUSceneSourceItem& xItem = GPUScene_AddItem(xItems, xWorld);
		Flux_GPUSceneSourceSubmesh xSub = GPUScene_MakeSub(1u, 0u, 10u, 0u);
		xSub.m_uColorTintPacked = uColorTint;
		xItem.m_xSubmeshes.PushBack(xSub);

		Flux_GPUSceneBucketRegistry xReg;
		Flux_GPUSceneBuildResult xRes;
		Flux_BuildGPUScene(xItems, xReg, xRes);
		return Flux_HashGPUSceneForTest(xRes);
	};

	const u_int64 ulBase   = BuildHash(uFLUX_GPUSCENE_TINT_WHITE, 0.0f);
	const u_int64 ulTint   = BuildHash(0x00FF00FFu,               0.0f);
	const u_int64 ulMoved  = BuildHash(uFLUX_GPUSCENE_TINT_WHITE, 9.0f);

	ZENITH_ASSERT_NE(ulBase, ulTint,  "a different packed tint must change the record hash");
	ZENITH_ASSERT_NE(ulBase, ulMoved, "a different model transform must change the record hash");
}

// ---- per-submesh bounding sphere (Stage 0d) --------------------------------

ZENITH_TEST(GPUScene, LocalBoundsSphereEnclosesAABB)
{
	// Symmetric unit cube: centre at origin, radius = half-diagonal = sqrt(3).
	Zenith_Maths::Vector4 xCube = Flux_LocalBoundsSphereFromAABB(
		Zenith_Maths::Vector3(-1.0f, -1.0f, -1.0f), Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f));
	ZENITH_ASSERT_EQ_FLOAT(xCube.x, 0.0f, 0.0001f, "unit cube sphere centre X is the origin");
	ZENITH_ASSERT_EQ_FLOAT(xCube.y, 0.0f, 0.0001f, "unit cube sphere centre Y is the origin");
	ZENITH_ASSERT_EQ_FLOAT(xCube.z, 0.0f, 0.0001f, "unit cube sphere centre Z is the origin");
	ZENITH_ASSERT_EQ_FLOAT(xCube.w, 1.7320508f, 0.0005f, "radius encloses the box (half-diagonal sqrt(3))");

	// Offset, non-cubic box: centre = midpoint, radius = half-diagonal.
	Zenith_Maths::Vector4 xBox = Flux_LocalBoundsSphereFromAABB(
		Zenith_Maths::Vector3(2.0f, 4.0f, 6.0f), Zenith_Maths::Vector3(4.0f, 8.0f, 10.0f));
	ZENITH_ASSERT_EQ_FLOAT(xBox.x, 3.0f, 0.0001f, "offset box sphere centre X is the midpoint");
	ZENITH_ASSERT_EQ_FLOAT(xBox.y, 6.0f, 0.0001f, "offset box sphere centre Y is the midpoint");
	ZENITH_ASSERT_EQ_FLOAT(xBox.z, 8.0f, 0.0001f, "offset box sphere centre Z is the midpoint");
	ZENITH_ASSERT_EQ_FLOAT(xBox.w, 3.0f, 0.0005f, "radius = half-diagonal sqrt(1+4+4) = 3");
}

// ---- per-bucket prefix-sum offsets (Stage 1) -------------------------------

ZENITH_TEST(GPUScene, BuildBucketOffsetsIsExclusivePrefixSum)
{
	Zenith_Vector<u_int> auCounts;
	auCounts.PushBack(3u);   // bucket 0
	auCounts.PushBack(0u);   // bucket 1 (empty/retired contributes 0 width)
	auCounts.PushBack(5u);   // bucket 2
	auCounts.PushBack(1u);   // bucket 3

	Zenith_Vector<u_int> auOffsets;
	Flux_BuildBucketOffsets(auCounts, auOffsets);

	ZENITH_ASSERT_EQ(auOffsets.GetSize(), 4u, "one offset per bucket");
	ZENITH_ASSERT_EQ(auOffsets.Get(0), 0u, "bucket 0 starts at 0");
	ZENITH_ASSERT_EQ(auOffsets.Get(1), 3u, "bucket 1 starts after bucket 0's 3 items");
	ZENITH_ASSERT_EQ(auOffsets.Get(2), 3u, "an empty bucket adds no width (offset unchanged)");
	ZENITH_ASSERT_EQ(auOffsets.Get(3), 8u, "bucket 3 starts after 3+0+5 items");
	// The next offset (the running total) would be 9 == sum of all counts; the slices
	// [off,off+count) thus exactly tile the visible-index buffer with no overlap.
}

ZENITH_TEST(GPUScene, BuildBucketOffsetsEmptyInputProducesEmptyOutput)
{
	Zenith_Vector<u_int> auCounts;     // no buckets this frame
	Zenith_Vector<u_int> auOffsets;
	auOffsets.PushBack(123u);          // stale content must be cleared
	Flux_BuildBucketOffsets(auCounts, auOffsets);
	ZENITH_ASSERT_EQ(auOffsets.GetSize(), 0u, "no buckets -> no offsets (output is cleared)");
}

// ---- max-scale + frustum cull (CPU mirror of Flux_UnifiedMesh_Culling.slang) --

namespace
{
	// Inward-pointing planes of the axis-aligned box [-10,10]^3 (xyz = normal, w = dist):
	// a point p is inside plane i iff dot(n_i,p)+w_i >= 0; the cull test rejects when
	// dot(n,worldCenter)+w < -worldRadius (i.e. the sphere is fully outside that plane).
	void GPUScene_MakeBoxFrustum(Zenith_Maths::Vector4 axPlanes[6])
	{
		axPlanes[0] = Zenith_Maths::Vector4( 1.0f,  0.0f,  0.0f, 10.0f);  // x >= -10
		axPlanes[1] = Zenith_Maths::Vector4(-1.0f,  0.0f,  0.0f, 10.0f);  // x <= +10
		axPlanes[2] = Zenith_Maths::Vector4( 0.0f,  1.0f,  0.0f, 10.0f);  // y >= -10
		axPlanes[3] = Zenith_Maths::Vector4( 0.0f, -1.0f,  0.0f, 10.0f);  // y <= +10
		axPlanes[4] = Zenith_Maths::Vector4( 0.0f,  0.0f,  1.0f, 10.0f);  // z >= -10
		axPlanes[5] = Zenith_Maths::Vector4( 0.0f,  0.0f, -1.0f, 10.0f);  // z <= +10
	}
}

ZENITH_TEST(GPUScene, MaxScaleFromMatrixTakesLargestBasisLength)
{
	ZENITH_ASSERT_EQ_FLOAT(Flux_MaxScaleFromMatrix(Zenith_Maths::Matrix4(1.0f)), 1.0f, 0.0001f,
		"identity -> unit scale");

	// Non-uniform scale (2,5,3): the conservative radius must use the LARGEST axis (5).
	Zenith_Maths::Matrix4 xScale(1.0f);
	xScale[0] = Zenith_Maths::Vector4(2.0f, 0.0f, 0.0f, 0.0f);
	xScale[1] = Zenith_Maths::Vector4(0.0f, 5.0f, 0.0f, 0.0f);
	xScale[2] = Zenith_Maths::Vector4(0.0f, 0.0f, 3.0f, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(Flux_MaxScaleFromMatrix(xScale), 5.0f, 0.0001f,
		"non-uniform scale -> max basis length");
}

ZENITH_TEST(GPUScene, CullDrawItemInsideOutsideAndStraddle)
{
	Zenith_Maths::Vector4 axPlanes[6];
	GPUScene_MakeBoxFrustum(axPlanes);
	const Zenith_Maths::Matrix4 xIdentity(1.0f);

	// Unit sphere at the origin -> fully inside.
	ZENITH_ASSERT_TRUE(Flux_CullDrawItemAgainstFrustum(xIdentity,
		Zenith_Maths::Vector4(0.0f, 0.0f, 0.0f, 1.0f), axPlanes), "origin sphere is visible");

	// Sphere centre well past +x (12) radius 1 -> fully outside -> culled.
	Zenith_Maths::Matrix4 xFar(1.0f); xFar[3] = Zenith_Maths::Vector4(12.0f, 0.0f, 0.0f, 1.0f);
	ZENITH_ASSERT_FALSE(Flux_CullDrawItemAgainstFrustum(xFar,
		Zenith_Maths::Vector4(0.0f, 0.0f, 0.0f, 1.0f), axPlanes), "sphere fully past +x is culled");

	// Sphere centre at 10.5 radius 1 straddles the +x plane -> partially inside -> visible.
	Zenith_Maths::Matrix4 xEdge(1.0f); xEdge[3] = Zenith_Maths::Vector4(10.5f, 0.0f, 0.0f, 1.0f);
	ZENITH_ASSERT_TRUE(Flux_CullDrawItemAgainstFrustum(xEdge,
		Zenith_Maths::Vector4(0.0f, 0.0f, 0.0f, 1.0f), axPlanes), "sphere straddling +x stays visible");
}

ZENITH_TEST(GPUScene, CullDrawItemScaleInflatesTheWorldSphere)
{
	Zenith_Maths::Vector4 axPlanes[6];
	GPUScene_MakeBoxFrustum(axPlanes);

	// Centre at (12,0,0). With unit scale (worldRadius 1) it is fully outside -> culled.
	Zenith_Maths::Matrix4 xUnit(1.0f); xUnit[3] = Zenith_Maths::Vector4(12.0f, 0.0f, 0.0f, 1.0f);
	ZENITH_ASSERT_FALSE(Flux_CullDrawItemAgainstFrustum(xUnit,
		Zenith_Maths::Vector4(0.0f, 0.0f, 0.0f, 1.0f), axPlanes), "unit-scale sphere at 12 is culled");

	// Same centre, uniform scale 3 -> worldRadius 3 reaches back into the box -> visible.
	// Proves the model transform inflates the cull sphere (object-transformed bounds).
	Zenith_Maths::Matrix4 xScaled(1.0f);
	xScaled[0] = Zenith_Maths::Vector4(3.0f, 0.0f, 0.0f, 0.0f);
	xScaled[1] = Zenith_Maths::Vector4(0.0f, 3.0f, 0.0f, 0.0f);
	xScaled[2] = Zenith_Maths::Vector4(0.0f, 0.0f, 3.0f, 0.0f);
	xScaled[3] = Zenith_Maths::Vector4(12.0f, 0.0f, 0.0f, 1.0f);
	ZENITH_ASSERT_TRUE(Flux_CullDrawItemAgainstFrustum(xScaled,
		Zenith_Maths::Vector4(0.0f, 0.0f, 0.0f, 1.0f), axPlanes), "scale-3 sphere at 12 reaches the box");
}

// ---- reset-indirect packer (CPU mirror of Flux_UnifiedMesh_Reset.slang) -----

ZENITH_TEST(GPUScene, PackResetIndirectCommandLayout)
{
	u_int auCmd[uFLUX_GPUSCENE_INDIRECT_WORDS] = { 9u, 9u, 9u, 9u, 9u };   // poison
	Flux_PackResetIndirectCommand(auCmd, 1234u);
	ZENITH_ASSERT_EQ(auCmd[0], 1234u, "word 0 = indexCount from the bucket's mesh");
	ZENITH_ASSERT_EQ(auCmd[1], 0u, "word 1 = instanceCount starts at 0 (cull increments it)");
	ZENITH_ASSERT_EQ(auCmd[2], 0u, "word 2 = firstIndex 0");
	ZENITH_ASSERT_EQ(auCmd[3], 0u, "word 3 = vertexOffset 0 (static/foliage default)");
	ZENITH_ASSERT_EQ(auCmd[4], 0u, "word 4 = firstInstance 0");
}

ZENITH_TEST(GPUScene, PackResetIndirectCommandCarriesVertexOffset)
{
	// Stage 5: a skinned bucket packs its skinned-arena slice base into word 3 (vertexOffset)
	// so the fixed-function draw reaches that instance's slice; only word 3 differs from the
	// static layout (the MDI-ready foundation for Stage 6's per-command vertexOffsets).
	u_int auCmd[uFLUX_GPUSCENE_INDIRECT_WORDS] = { 0u, 0u, 0u, 0u, 0u };
	Flux_PackResetIndirectCommand(auCmd, 360u, /*uVertexOffset*/ 4096u);
	ZENITH_ASSERT_EQ(auCmd[0], 360u, "word 0 = indexCount");
	ZENITH_ASSERT_EQ(auCmd[1], 0u, "word 1 = instanceCount 0");
	ZENITH_ASSERT_EQ(auCmd[2], 0u, "word 2 = firstIndex 0");
	ZENITH_ASSERT_EQ(auCmd[3], 4096u, "word 3 = the skinned-arena slice base (vertexOffset)");
	ZENITH_ASSERT_EQ(auCmd[4], 0u, "word 4 = firstInstance 0");
}

// ---- multi-view cull-output index math (Stage 2 — shaders + draw code mirror these) --

ZENITH_TEST(GPUScene, UnifiedIndirectCommandWordIsViewMajor)
{
	const u_int uNumBuckets = 3u;
	// View 0 (camera) is byte-identical to the Stage-1 single-view layout: command b at word b*5.
	ZENITH_ASSERT_EQ(Flux_UnifiedIndirectCommandWord(0u, 0u, uNumBuckets), 0u, "camera bucket 0 -> word 0");
	ZENITH_ASSERT_EQ(Flux_UnifiedIndirectCommandWord(0u, 2u, uNumBuckets), 10u, "camera bucket 2 -> word 2*5");
	// Each view starts a fresh numBuckets-wide command block (view-major layout).
	ZENITH_ASSERT_EQ(Flux_UnifiedIndirectCommandWord(1u, 0u, uNumBuckets), 15u, "view 1 bucket 0 -> (1*3+0)*5");
	ZENITH_ASSERT_EQ(Flux_UnifiedIndirectCommandWord(4u, 2u, uNumBuckets), (4u * 3u + 2u) * 5u, "view 4 bucket 2");
}

ZENITH_TEST(GPUScene, UnifiedVisibleWriteIndexIsViewMajor)
{
	const u_int uTotalDrawItems = 10u;
	// View 0 (camera) base is the within-view bucket offset (Stage-1 layout unchanged).
	ZENITH_ASSERT_EQ(Flux_UnifiedVisibleWriteIndex(0u, uTotalDrawItems, 4u, 0u), 4u, "camera base = bucketOffset");
	ZENITH_ASSERT_EQ(Flux_UnifiedVisibleWriteIndex(0u, uTotalDrawItems, 4u, 2u), 6u, "camera slot 2 -> off+2");
	// View v adds a v*totalDrawItems per-view stride on top of the within-view offset.
	ZENITH_ASSERT_EQ(Flux_UnifiedVisibleWriteIndex(1u, uTotalDrawItems, 4u, 0u), 14u, "view 1 base = total+off");
	ZENITH_ASSERT_EQ(Flux_UnifiedVisibleWriteIndex(3u, uTotalDrawItems, 4u, 2u), 36u, "view 3 slot 2 -> 3*10+4+2");
}

ZENITH_TEST(GPUScene, UnifiedViewPartitionsDoNotOverlap)
{
	// Each view's visible slice must end strictly below the next view's base, so the per-view
	// partitions tile the buffer with no aliasing (worst case: every draw-item survives every view).
	const u_int uTotalDrawItems = 10u;
	for (u_int v = 0; v < 4u; ++v)
	{
		const u_int uLastInView = Flux_UnifiedVisibleWriteIndex(v, uTotalDrawItems, uTotalDrawItems - 1u, 0u);
		const u_int uNextBase   = Flux_UnifiedVisibleWriteIndex(v + 1u, uTotalDrawItems, 0u, 0u);
		ZENITH_ASSERT_TRUE(uLastInView < uNextBase, "view v's slice ends before view v+1's base");
	}
}

ZENITH_TEST(GPUScene, UnifiedLastViewSurvivorFitsAllocation)
{
	// The visible-index buffer is allocated kuUNIFIED_NUM_VIEWS * totalDrawItems wide. The highest
	// possible write is the LAST view's last survivor: base bucketOffset = totalDrawItems-1, slot 0.
	// It must land at exactly the final word (capacity-1), proving no per-view stride overruns the
	// allocation even when every draw-item survives every view. Pins the Initialise() buffer sizing.
	const u_int uTotalDrawItems = 8u;
	const u_int uNumViews  = 5u;                          // 1 camera + 4 CSM cascades (kuUNIFIED_NUM_VIEWS)
	const u_int uCapacity  = uNumViews * uTotalDrawItems;
	const u_int uMaxIndex  = Flux_UnifiedVisibleWriteIndex(uNumViews - 1u, uTotalDrawItems, uTotalDrawItems - 1u, 0u);
	ZENITH_ASSERT_TRUE(uMaxIndex < uCapacity, "last view's max survivor stays within the visible-index allocation");
	ZENITH_ASSERT_EQ(uMaxIndex, uCapacity - 1u, "last view's last survivor is exactly the final buffer word");
}

ZENITH_TEST(GPUScene, UnifiedLastIndirectCommandFitsAllocation)
{
	// The indirect buffer is kuUNIFIED_NUM_VIEWS * numBuckets * 5 words. The last command (last
	// view, last bucket) must occupy the final 5-word slot exactly — no view/bucket stride can
	// address past the allocation. Pins the Initialise() indirect-buffer sizing against the addressing.
	const u_int uNumViews   = 5u;
	const u_int uNumBuckets  = 7u;
	const u_int uCapacityWords = uNumViews * uNumBuckets * uFLUX_GPUSCENE_INDIRECT_WORDS;
	const u_int uLastCmdWord   = Flux_UnifiedIndirectCommandWord(uNumViews - 1u, uNumBuckets - 1u, uNumBuckets);
	ZENITH_ASSERT_EQ(uLastCmdWord, uCapacityWords - uFLUX_GPUSCENE_INDIRECT_WORDS,
		"last command sits at the final 5-word slot of the indirect allocation");
	ZENITH_ASSERT_EQ(uLastCmdWord + uFLUX_GPUSCENE_INDIRECT_WORDS, uCapacityWords,
		"the last command's five words fit exactly within the allocation");
}

// ---- cascade caster-extend retention (Stage 2 cull primitive vs an ortho box) --

ZENITH_TEST(GPUScene, CascadeCasterExtendRetainsNearOccluders)
{
	// Model a cascade's light-space ortho box [-10,10]x[-10,10]x[0,100]. UpdateShadowMatrices
	// pushes the light origin back past the camera-frustum slice (caster-extend) so off-frustum
	// occluders BETWEEN the light and the slice still rasterise into the cascade. The cull must
	// KEEP a caster in that extended near region and DROP ones outside the box.
	Zenith_Maths::Vector4 axPlanes[6];
	axPlanes[0] = Zenith_Maths::Vector4( 1.0f,  0.0f,  0.0f,  10.0f);  // x >= -10
	axPlanes[1] = Zenith_Maths::Vector4(-1.0f,  0.0f,  0.0f,  10.0f);  // x <= +10
	axPlanes[2] = Zenith_Maths::Vector4( 0.0f,  1.0f,  0.0f,  10.0f);  // y >= -10
	axPlanes[3] = Zenith_Maths::Vector4( 0.0f, -1.0f,  0.0f,  10.0f);  // y <= +10
	axPlanes[4] = Zenith_Maths::Vector4( 0.0f,  0.0f,  1.0f,   0.0f);  // z >= 0   (extended near)
	axPlanes[5] = Zenith_Maths::Vector4( 0.0f,  0.0f, -1.0f, 100.0f);  // z <= 100 (far + extend)

	const Zenith_Maths::Vector4 xUnitSphere(0.0f, 0.0f, 0.0f, 1.0f);

	// Occluder near the light eye (z=3) — inside the extended near region -> still casts.
	Zenith_Maths::Matrix4 xNear(1.0f); xNear[3] = Zenith_Maths::Vector4(0.0f, 0.0f, 3.0f, 1.0f);
	ZENITH_ASSERT_TRUE(Flux_CullDrawItemAgainstFrustum(xNear, xUnitSphere, axPlanes),
		"near occluder in the caster-extend region still casts");

	// Occluder behind the near plane (z=-3, radius 1) -> fully outside z>=0 -> culled.
	Zenith_Maths::Matrix4 xBehind(1.0f); xBehind[3] = Zenith_Maths::Vector4(0.0f, 0.0f, -3.0f, 1.0f);
	ZENITH_ASSERT_FALSE(Flux_CullDrawItemAgainstFrustum(xBehind, xUnitSphere, axPlanes),
		"occluder fully behind the cascade near plane is culled");

	// Occluder past the far plane (z=103) -> culled.
	Zenith_Maths::Matrix4 xFar(1.0f); xFar[3] = Zenith_Maths::Vector4(0.0f, 0.0f, 103.0f, 1.0f);
	ZENITH_ASSERT_FALSE(Flux_CullDrawItemAgainstFrustum(xFar, xUnitSphere, axPlanes),
		"occluder past the cascade far plane is culled");
}

// ---- incremental build: snapshot statics + foliage instances in one scene (Stage 3b) --

ZENITH_TEST(GPUScene, IncrementalBuildMixesItemsAndInstances)
{
	Flux_GPUSceneBucketRegistry xReg;
	Flux_GPUSceneBuildResult xOut;

	Flux_BeginGPUSceneBuild(xOut, xReg);

	// One static model item with two submeshes -> two distinct buckets.
	Flux_GPUSceneSourceItem xItem;
	xItem.m_xWorldMatrix = Zenith_Maths::Matrix4(1.0f);
	xItem.m_xSubmeshes.PushBack(GPUScene_MakeSub(1u, uFLUX_GPUSCENE_CULL_ONE_SIDED, 100u, 0u));
	xItem.m_xSubmeshes.PushBack(GPUScene_MakeSub(2u, uFLUX_GPUSCENE_CULL_TWO_SIDED, 200u, 0u));
	Flux_AppendGPUSceneItem(xItem, xReg, xOut);

	// Two foliage instances sharing ONE (mesh,cull,material,VAT) bucket (the N-trees collapse).
	Flux_GPUSceneBucketKey xTreeKey;
	xTreeKey.m_uMeshGeometryId   = 9u;
	xTreeKey.m_uCullMode         = uFLUX_GPUSCENE_CULL_TWO_SIDED;
	xTreeKey.m_ulMaterialAssetId = 777u;
	xTreeKey.m_ulVATTextureId    = 555u;
	const Zenith_Maths::Vector4 xSphere(0.0f, 0.0f, 0.0f, 3.0f);
	const u_int uPacked = Flux_PackVATAnim(0u, 120u);
	Flux_AppendGPUSceneInstance(xReg, xOut, Zenith_Maths::Matrix4(1.0f),
		uFLUX_GPUSCENE_OBJFLAG_VAT, uPacked, 0u, xTreeKey, xSphere, 0xAABBCCDDu);
	Flux_AppendGPUSceneInstance(xReg, xOut, Zenith_Maths::Matrix4(2.0f),
		uFLUX_GPUSCENE_OBJFLAG_VAT, uPacked, 0u, xTreeKey, xSphere, 0x11223344u);

	Flux_EndGPUSceneBuild(xOut, xReg);

	ZENITH_ASSERT_EQ(xOut.m_xObjects.GetSize(), 3u, "1 model + 2 instances -> 3 objects");
	ZENITH_ASSERT_EQ(xOut.m_xDrawItems.GetSize(), 4u, "2 submeshes + 2 instances -> 4 draw-items");
	ZENITH_ASSERT_EQ(xReg.GetLiveBucketCount(), 3u, "2 static buckets + 1 shared tree bucket");
	ZENITH_ASSERT_EQ(xReg.GetBucketRefcount(xTreeKey), 2u, "both instances reference the one tree bucket");

	// Instance objects (indices 1,2) carry the VAT flag + packed anim; the static object (0) does not.
	ZENITH_ASSERT_TRUE((xOut.m_xObjects.Get(0).m_uFlags & uFLUX_GPUSCENE_OBJFLAG_VAT) == 0u, "static object: no VAT flag");
	ZENITH_ASSERT_TRUE((xOut.m_xObjects.Get(1).m_uFlags & uFLUX_GPUSCENE_OBJFLAG_VAT) != 0u, "instance object: VAT flag set");
	ZENITH_ASSERT_EQ((xOut.m_xObjects.Get(1).m_uVATAnimPacked >> 16) & 0xFFFFu, 120u, "instance frame count packed");

	// The two instance draw-items point at distinct objects but share the one tree bucket.
	const Flux_GPUSceneDrawItem& xA = xOut.m_xDrawItems.Get(2);
	const Flux_GPUSceneDrawItem& xB = xOut.m_xDrawItems.Get(3);
	ZENITH_ASSERT_EQ(xA.m_uBucketIndex, xB.m_uBucketIndex, "both instances in one bucket");
	ZENITH_ASSERT_TRUE(xA.m_uObjectIndex != xB.m_uObjectIndex, "distinct instance objects");
	ZENITH_ASSERT_EQ(xA.m_uColorTintPacked, 0xAABBCCDDu, "instance A tint preserved");
}

// ---- per-draw-item render-view visibility mask (S4 multi-view) ----------------

ZENITH_TEST(GPUScene, DrawItemViewMaskPackAndTest)
{
	// Low 16 bits stay item flags; high 16 carry the per-view-slot mask.
	const u_int uPacked = Flux_PackDrawItemViewMask(0x2u, Flux_ViewMaskForSlot(kuFluxViewSlotPreviewMaterial));
	ZENITH_ASSERT_EQ(uPacked & 0xFFFFu, 0x2u, "low flag bits preserved");
	ZENITH_ASSERT_TRUE(Flux_DrawItemVisibleInView(uPacked, kuFluxViewSlotPreviewMaterial), "preview-only item visible in the material-preview slot");
	ZENITH_ASSERT_FALSE(Flux_DrawItemVisibleInView(uPacked, kuFluxViewSlotMain), "preview-only item invisible to the camera");
	ZENITH_ASSERT_FALSE(Flux_DrawItemVisibleInView(uPacked, kuFluxViewSlotShadowFirst), "preview-only item invisible to cascades");
	// The mask names ONE preview slot, so the OTHER preview view must not see it —
	// the clause the singular helper could not state, and the one that would catch
	// a per-slot mask that had degraded back to "any preview view".
	ZENITH_ASSERT_FALSE(Flux_DrawItemVisibleInView(uPacked, kuFluxViewSlotPreviewAnim), "a material-preview item is invisible to the animation preview");

	const u_int uScene = Flux_PackDrawItemViewMask(0u, Flux_ViewMaskAllSceneViews(true));
	ZENITH_ASSERT_TRUE(Flux_DrawItemVisibleInView(uScene, kuFluxViewSlotMain), "scene item visible to the camera");
	for (u_int u = 0; u < kuFluxViewNumShadowSlots; u++)
	{
		ZENITH_ASSERT_TRUE(Flux_DrawItemVisibleInView(uScene, kuFluxViewSlotShadowFirst + u), "scene item visible to every cascade");
	}
	for (u_int u = 0; u < kuFluxViewNumPreviewSlots; u++)
	{
		ZENITH_ASSERT_FALSE(Flux_DrawItemVisibleInView(uScene, kuFluxViewSlotPreviewFirst + u),
			"scene item never leaks into preview slot %u", kuFluxViewSlotPreviewFirst + u);
	}
}

ZENITH_TEST(GPUScene, BuildersDefaultToAllSceneViews)
{
	Flux_GPUSceneBucketRegistry xReg;
	Flux_GPUSceneBuildResult xOut;
	Flux_BeginGPUSceneBuild(xOut, xReg);

	// Snapshot item path (default source-item mask) + instance path (default param).
	Flux_GPUSceneSourceItem xItem;
	xItem.m_xSubmeshes.PushBack(GPUScene_MakeSub(1u, uFLUX_GPUSCENE_CULL_ONE_SIDED, 100u, 0u));
	Flux_AppendGPUSceneItem(xItem, xReg, xOut);

	Flux_GPUSceneBucketKey xKey;
	xKey.m_uMeshGeometryId = 2u;
	Flux_AppendGPUSceneInstance(xReg, xOut, Zenith_Maths::Matrix4(1.0f), 0u, 0u, 0u,
		xKey, Zenith_Maths::Vector4(0.0f, 0.0f, 0.0f, 1.0f), uFLUX_GPUSCENE_TINT_WHITE);

	// Preview-exclusive override rides the source item.
	Flux_GPUSceneSourceItem xPreviewItem;
	xPreviewItem.m_uViewMask = Flux_ViewMaskForSlot(kuFluxViewSlotPreviewMaterial);
	xPreviewItem.m_xSubmeshes.PushBack(GPUScene_MakeSub(3u, uFLUX_GPUSCENE_CULL_ONE_SIDED, 300u, 0u));
	Flux_AppendGPUSceneItem(xPreviewItem, xReg, xOut);

	Flux_EndGPUSceneBuild(xOut, xReg);

	ZENITH_ASSERT_TRUE(Flux_DrawItemVisibleInView(xOut.m_xDrawItems.Get(0).m_uFlags, kuFluxViewSlotMain), "snapshot item defaults into the camera view");
	ZENITH_ASSERT_TRUE(Flux_DrawItemVisibleInView(xOut.m_xDrawItems.Get(0).m_uFlags, kuFluxViewSlotShadowFirst), "snapshot item defaults into the cascades");
	ZENITH_ASSERT_FALSE(Flux_DrawItemVisibleInView(xOut.m_xDrawItems.Get(0).m_uFlags, kuFluxViewSlotPreviewMaterial), "snapshot item stays out of the material preview");
	ZENITH_ASSERT_FALSE(Flux_DrawItemVisibleInView(xOut.m_xDrawItems.Get(0).m_uFlags, kuFluxViewSlotPreviewAnim), "snapshot item stays out of the animation preview");
	ZENITH_ASSERT_TRUE(Flux_DrawItemVisibleInView(xOut.m_xDrawItems.Get(1).m_uFlags, kuFluxViewSlotMain), "instance defaults into the camera view");
	ZENITH_ASSERT_FALSE(Flux_DrawItemVisibleInView(xOut.m_xDrawItems.Get(2).m_uFlags, kuFluxViewSlotMain), "preview-only item stays out of the camera view");
	ZENITH_ASSERT_TRUE(Flux_DrawItemVisibleInView(xOut.m_xDrawItems.Get(2).m_uFlags, kuFluxViewSlotPreviewMaterial), "preview-only item lands in the material-preview slot");
}

// ---- external scene items: classification, routing, and the pull seam (D4) ---

namespace
{
	// The classifier returns an enum; the value formatter prints "<value>" for one, so
	// the tests compare NUMBERS and a failure names the class that came back.
	u_int GPUScene_ExternalClass(bool bHasMesh, u_int uNumVerts, u_int uNumIndices,
		bool bHasSkeleton, bool bHasSkinning, MaterialBlendMode eBlend)
	{
		return static_cast<u_int>(Flux_ClassifyExternalSceneItem(
			bHasMesh, uNumVerts, uNumIndices, bHasSkeleton, bHasSkinning, eBlend));
	}

	u_int GPUScene_ExternalWalk(Flux_ExternalItemClass eClass)
	{
		return static_cast<u_int>(Flux_RouteExternalItem(eClass));
	}

	// A pull source + its context, in the exact shape a preview session registers.
	struct GPUScene_ExternalSourceCtx
	{
		u_int m_uPollCount = 0u;
	};

	void GPUScene_GatherOneItem(void* pCtx, Zenith_Vector<Flux_RendererImpl::Flux_ExternalSceneItem>& xOut)
	{
		GPUScene_ExternalSourceCtx& xSource = *static_cast<GPUScene_ExternalSourceCtx*>(pCtx);
		++xSource.m_uPollCount;
		Flux_RendererImpl::Flux_ExternalSceneItem xItem;
		xItem.m_uViewMask = Flux_ViewMaskForSlot(kuFluxViewSlotPreviewAnim);
		xOut.PushBack(xItem);
	}
}

ZENITH_TEST(GPUScene, ExternalItemClassifierCascade)
{
	// Rule 1 — nothing to draw. FIRST, so it wins over every later test.
	ZENITH_ASSERT_EQ(GPUScene_ExternalClass(false, 100u, 300u, true, true, MATERIAL_BLEND_OPAQUE),
		static_cast<u_int>(EXTERNAL_ITEM_SKIP), "no mesh instance -> SKIP, whatever else the item carries");
	ZENITH_ASSERT_EQ(GPUScene_ExternalClass(true, 0u, 300u, false, false, MATERIAL_BLEND_OPAQUE),
		static_cast<u_int>(EXTERNAL_ITEM_SKIP), "zero vertices -> SKIP");
	// The OVERLAP row: verts but no indices. No Flux_MeshInstance factory can build this
	// (they all refuse degenerate counts), so a value-level classifier is the only place
	// it is reachable at all — and it is exactly the shape an indexed draw reads as empty.
	ZENITH_ASSERT_EQ(GPUScene_ExternalClass(true, 100u, 0u, false, false, MATERIAL_BLEND_OPAQUE),
		static_cast<u_int>(EXTERNAL_ITEM_SKIP), "vertices but no indices -> SKIP");
	// ...and rule 1 beats the blend test: a DEGENERATE translucent item is dropped, not
	// forwarded to the translucent list (the pre-D4 order would have preserved it).
	ZENITH_ASSERT_EQ(GPUScene_ExternalClass(true, 100u, 0u, false, false, MATERIAL_BLEND_TRANSLUCENT),
		static_cast<u_int>(EXTERNAL_ITEM_SKIP), "degenerate beats translucent — rule 1 runs first");

	// Rule 3 — no skeleton at all is the material preview's own shape.
	ZENITH_ASSERT_EQ(GPUScene_ExternalClass(true, 100u, 300u, false, false, MATERIAL_BLEND_OPAQUE),
		static_cast<u_int>(EXTERNAL_ITEM_STATIC), "no skeleton -> STATIC");
	// A skeleton whose MESH is not skinned still draws — as static geometry. Routing it to
	// the skinned walk would ask for a bind pose the asset has not got, and it would vanish.
	ZENITH_ASSERT_EQ(GPUScene_ExternalClass(true, 100u, 300u, true, false, MATERIAL_BLEND_OPAQUE),
		static_cast<u_int>(EXTERNAL_ITEM_STATIC), "skeleton without skinning -> STATIC, not SKINNED");
	// A skinned mesh with no skeleton to pose it is likewise static (bind pose), not skinned.
	ZENITH_ASSERT_EQ(GPUScene_ExternalClass(true, 100u, 300u, false, true, MATERIAL_BLEND_OPAQUE),
		static_cast<u_int>(EXTERNAL_ITEM_STATIC), "skinning without a skeleton -> STATIC");
	// A translucent STATIC item is still STATIC: the external walk owns the divert to the
	// forward translucent list, so the classifier must NOT swallow it.
	ZENITH_ASSERT_EQ(GPUScene_ExternalClass(true, 100u, 300u, false, false, MATERIAL_BLEND_TRANSLUCENT),
		static_cast<u_int>(EXTERNAL_ITEM_STATIC), "translucent static item stays STATIC (the walk diverts it)");

	// Rule 2 — the animation preview's own shape.
	ZENITH_ASSERT_EQ(GPUScene_ExternalClass(true, 100u, 300u, true, true, MATERIAL_BLEND_OPAQUE),
		static_cast<u_int>(EXTERNAL_ITEM_SKINNED), "skeleton + skinning -> SKINNED");
	ZENITH_ASSERT_EQ(GPUScene_ExternalClass(true, 100u, 300u, true, true, MATERIAL_BLEND_MASKED),
		static_cast<u_int>(EXTERNAL_ITEM_SKINNED), "masked is opaque-path -> still SKINNED");
	// ...but compute-skinned translucency exists on NEITHER path, so it is a drop.
	ZENITH_ASSERT_EQ(GPUScene_ExternalClass(true, 100u, 300u, true, true, MATERIAL_BLEND_TRANSLUCENT),
		static_cast<u_int>(EXTERNAL_ITEM_SKIP), "skinned + translucent -> SKIP");
	ZENITH_ASSERT_EQ(GPUScene_ExternalClass(true, 100u, 300u, true, true, MATERIAL_BLEND_ADDITIVE),
		static_cast<u_int>(EXTERNAL_ITEM_SKIP), "skinned + additive -> SKIP");
}

ZENITH_TEST(GPUScene, EveryDrawableExternalItemAppearsExactlyOnce)
{
	// The two walks split the ONE external list, and "exactly once" is a property of the
	// pair. The live consumption needs a booted renderer (covered by the Phase G
	// end-to-end run, not by a unit), so what is pinned here is the ROUTING: every class
	// maps to exactly one walk, and a hand-made list is consumed with nothing drawn twice
	// and nothing silently dropped except what the cascade said to drop.
	ZENITH_ASSERT_EQ(GPUScene_ExternalWalk(EXTERNAL_ITEM_STATIC),
		static_cast<u_int>(EXTERNAL_ITEM_WALK_EXTERNAL), "STATIC -> the external walk");
	ZENITH_ASSERT_EQ(GPUScene_ExternalWalk(EXTERNAL_ITEM_SKINNED),
		static_cast<u_int>(EXTERNAL_ITEM_WALK_SKINNED), "SKINNED -> the skinned walk");
	ZENITH_ASSERT_EQ(GPUScene_ExternalWalk(EXTERNAL_ITEM_SKIP),
		static_cast<u_int>(EXTERNAL_ITEM_WALK_NONE), "SKIP -> neither walk");

	const Flux_ExternalItemClass aeSubmissions[] =
	{
		EXTERNAL_ITEM_STATIC,    // the material preview's mesh
		EXTERNAL_ITEM_SKINNED,   // the animation preview's rig
		EXTERNAL_ITEM_SKIP,      // an unresolved / degenerate submission
		EXTERNAL_ITEM_SKINNED,
		EXTERNAL_ITEM_STATIC,
		EXTERNAL_ITEM_STATIC,
	};
	const u_int uCount = static_cast<u_int>(sizeof(aeSubmissions) / sizeof(aeSubmissions[0]));

	u_int uExternalWalk = 0u;
	u_int uSkinnedWalk  = 0u;
	u_int uDropped      = 0u;
	for (u_int u = 0; u < uCount; ++u)
	{
		switch (Flux_RouteExternalItem(aeSubmissions[u]))
		{
		case EXTERNAL_ITEM_WALK_EXTERNAL: ++uExternalWalk; break;
		case EXTERNAL_ITEM_WALK_SKINNED:  ++uSkinnedWalk;  break;
		case EXTERNAL_ITEM_WALK_NONE:     ++uDropped;      break;
		}
	}

	ZENITH_ASSERT_EQ(uExternalWalk, 3u, "three STATIC items land on the external walk");
	ZENITH_ASSERT_EQ(uSkinnedWalk, 2u, "two SKINNED items land on the skinned walk");
	ZENITH_ASSERT_EQ(uDropped, 1u, "the one SKIP item is drawn by nobody");
	ZENITH_ASSERT_EQ(uExternalWalk + uSkinnedWalk + uDropped, uCount,
		"every submission is accounted for EXACTLY once — no double-draw, no silent loss");
}

ZENITH_TEST(GPUScene, PreviewMaskedSkinnedInstanceIsVisibleOnlyInItsView)
{
	Flux_GPUSceneBucketRegistry xReg;
	Flux_GPUSceneBuildResult xOut;
	Flux_BeginGPUSceneBuild(xOut, xReg);

	const Zenith_Maths::Vector4 xSphere(0.0f, 0.0f, 0.0f, 1.0f);

	// A scene character: the append's DEFAULT mask (camera + cascades).
	Flux_GPUSceneBucketKey xSceneKey;
	xSceneKey.m_uMeshGeometryId   = uFLUX_GPUSCENE_SKINNED_MESH_BIT | 1u;
	xSceneKey.m_ulMaterialAssetId = 10u;
	Flux_AppendGPUSceneSkinnedInstance(xReg, xOut, Zenith_Maths::Matrix4(1.0f), 0u,
		xSceneKey, xSphere, uFLUX_GPUSCENE_TINT_WHITE);

	// The animation preview's rig: its OWN slot, passed EXPLICITLY as the 8th argument.
	// Taking the default here is the whole failure mode this test exists for — a preview
	// rig would then render into the main camera and all four shadow cascades.
	Flux_GPUSceneBucketKey xPreviewKey;
	xPreviewKey.m_uMeshGeometryId   = uFLUX_GPUSCENE_SKINNED_MESH_BIT | 2u;
	xPreviewKey.m_ulMaterialAssetId = 20u;
	Flux_AppendGPUSceneSkinnedInstance(xReg, xOut, Zenith_Maths::Matrix4(1.0f), 0u,
		xPreviewKey, xSphere, uFLUX_GPUSCENE_TINT_WHITE, Flux_ViewMaskForSlot(kuFluxViewSlotPreviewAnim));

	Flux_EndGPUSceneBuild(xOut, xReg);

	ZENITH_ASSERT_EQ(xOut.m_xDrawItems.GetSize(), 2u, "one draw-item per skinned append");
	const u_int uPreviewFlags = xOut.m_xDrawItems.Get(1).m_uFlags;
	const u_int uSceneFlags   = xOut.m_xDrawItems.Get(0).m_uFlags;

	ZENITH_ASSERT_TRUE(Flux_DrawItemVisibleInView(uPreviewFlags, kuFluxViewSlotPreviewAnim),
		"the preview rig is visible in the animation-preview slot");
	ZENITH_ASSERT_FALSE(Flux_DrawItemVisibleInView(uPreviewFlags, kuFluxViewSlotMain),
		"the preview rig never reaches the main camera");
	for (u_int u = 0; u < kuFluxViewNumShadowSlots; u++)
	{
		ZENITH_ASSERT_FALSE(Flux_DrawItemVisibleInView(uPreviewFlags, kuFluxViewSlotShadowFirst + u),
			"the preview rig casts into no cascade (slot %u)", kuFluxViewSlotShadowFirst + u);
	}
	// The OTHER preview view must not see it either — the clause a single "preview" mask
	// could not state, and the one that catches a per-slot mask degraded back to "any preview".
	ZENITH_ASSERT_FALSE(Flux_DrawItemVisibleInView(uPreviewFlags, kuFluxViewSlotPreviewMaterial),
		"the animation rig does not appear in the MATERIAL preview");

	ZENITH_ASSERT_TRUE(Flux_DrawItemVisibleInView(uSceneFlags, kuFluxViewSlotMain),
		"the scene character keeps the default camera visibility");
	ZENITH_ASSERT_FALSE(Flux_DrawItemVisibleInView(uSceneFlags, kuFluxViewSlotPreviewAnim),
		"scene content never leaks into the animation preview");

	// Both objects are still SKINNED objects carrying their palette base.
	ZENITH_ASSERT_TRUE((xOut.m_xObjects.Get(1).m_uFlags & uFLUX_GPUSCENE_OBJFLAG_SKINNED) != 0u,
		"a view-masked skinned append is still a skinned OBJECT");
}

ZENITH_TEST(GPUScene, SkinnedAppendRecordsExactlyOnePrevTransform)
{
	// The renderer's prev-transform array is index-locked to the GPU-scene objects: one
	// push immediately after EACH append. That is only correct because a skinned append
	// adds EXACTLY ONE object — never zero, never two — which is what this pins, driving
	// the real Flux_PrevTransformCache the way the external walk does (entity id 0).
	Flux_GPUSceneBucketRegistry xReg;
	Flux_GPUSceneBuildResult xOut;
	Flux_PrevTransformCache xCache;
	Zenith_Vector<Zenith_Maths::Matrix4> axPrevTransforms;

	Flux_BeginGPUSceneBuild(xOut, xReg);
	xCache.BeginFrame();

	const Zenith_Maths::Vector4 xSphere(0.0f, 0.0f, 0.0f, 1.0f);
	for (u_int u = 0; u < 3u; ++u)
	{
		Flux_GPUSceneBucketKey xKey;
		xKey.m_uMeshGeometryId   = uFLUX_GPUSCENE_SKINNED_MESH_BIT | u;
		xKey.m_ulMaterialAssetId = 100u;

		Zenith_Maths::Matrix4 xWorld(1.0f);
		xWorld[3] = Zenith_Maths::Vector4(static_cast<float>(u), 0.0f, 0.0f, 1.0f);

		const u_int uObjectsBefore = xOut.m_xObjects.GetSize();
		Flux_AppendGPUSceneSkinnedInstance(xReg, xOut, xWorld, 0u, xKey, xSphere,
			uFLUX_GPUSCENE_TINT_WHITE, Flux_ViewMaskForSlot(kuFluxViewSlotPreviewAnim));
		ZENITH_ASSERT_EQ(xOut.m_xObjects.GetSize(), uObjectsBefore + 1u,
			"one skinned append appends exactly one object");

		// RecordUnifiedPrevTransform's shape for an external item: no stable entity id.
		Zenith_Maths::Matrix4 xPrev;
		if (!xCache.TryGetPrev(0u, xPrev)) { xPrev = xWorld; }
		axPrevTransforms.PushBack(xPrev);
		xCache.RecordCurrent(0u, xWorld);
	}

	Flux_EndGPUSceneBuild(xOut, xReg);

	ZENITH_ASSERT_EQ(axPrevTransforms.GetSize(), xOut.m_xObjects.GetSize(),
		"the prev-transform array stays index-locked to the GPU-scene objects");
	ZENITH_ASSERT_EQ(xOut.m_xDrawItems.GetSize(), 3u, "one draw-item per skinned append");
	ZENITH_ASSERT_EQ_FLOAT(axPrevTransforms.Get(2)[3].x, 2.0f, 0.0001f,
		"an id-0 external item's prev IS its current -> camera-only velocity");
}

ZENITH_TEST(GPUScene, RegisteredSourceIsPolledAndUnregistered)
{
	// The PULL seam, with no device and no renderer boot: registration is pure
	// bookkeeping, and Gather...ForTesting polls without submitting anything.
	Flux_RendererImpl xRenderer;
	GPUScene_ExternalSourceCtx xCtx;
	Zenith_Vector<Flux_RendererImpl::Flux_ExternalSceneItem> xGathered;

	xRenderer.GatherExternalSceneItemsForTesting(xGathered);
	ZENITH_ASSERT_EQ(xGathered.GetSize(), 0u, "nothing registered -> nothing gathered");

	xRenderer.RegisterExternalSceneItemSource(&GPUScene_GatherOneItem, &xCtx);
	xGathered.Clear();
	xRenderer.GatherExternalSceneItemsForTesting(xGathered);
	ZENITH_ASSERT_EQ(xGathered.GetSize(), 1u, "a registered source is polled");
	ZENITH_ASSERT_EQ(xCtx.m_uPollCount, 1u, "...exactly once per gather");
	ZENITH_ASSERT_EQ(xGathered.Get(0).m_uViewMask, Flux_ViewMaskForSlot(kuFluxViewSlotPreviewAnim),
		"the source's own view mask arrives intact");

	// Re-registering the SAME context replaces its row. A session that re-resolves its
	// rig would otherwise be polled twice and draw its mesh twice into one view.
	xRenderer.RegisterExternalSceneItemSource(&GPUScene_GatherOneItem, &xCtx);
	xGathered.Clear();
	xRenderer.GatherExternalSceneItemsForTesting(xGathered);
	ZENITH_ASSERT_EQ(xGathered.GetSize(), 1u, "re-registering one context must not double it");

	xRenderer.UnregisterExternalSceneItemSource(&xCtx);
	xGathered.Clear();
	xRenderer.GatherExternalSceneItemsForTesting(xGathered);
	ZENITH_ASSERT_EQ(xGathered.GetSize(), 0u, "an unregistered source is never polled again");
}

ZENITH_TEST(GPUScene, UnifiedAddressingAtMaxViewSlots)
{
	// The visible/indirect buffers are sized to the FIXED slot capacity (8). The
	// highest slot's addressing must land exactly at the end of both allocations.
	const u_int uMaxViews  = FLUX_MAX_RENDER_VIEWS;
	const u_int uItems     = 100u;
	const u_int uBuckets   = 7u;
	ZENITH_ASSERT_EQ(Flux_UnifiedVisibleWriteIndex(uMaxViews - 1u, uItems, uItems - 1u, 0u),
		uMaxViews * uItems - 1u, "last view slot's final visible index is the last allocated word");
	ZENITH_ASSERT_EQ(Flux_UnifiedIndirectCommandWord(uMaxViews - 1u, uBuckets - 1u, uBuckets) + uFLUX_GPUSCENE_INDIRECT_WORDS,
		uMaxViews * uBuckets * uFLUX_GPUSCENE_INDIRECT_WORDS, "last view slot's final indirect command fits the allocation exactly");
}
