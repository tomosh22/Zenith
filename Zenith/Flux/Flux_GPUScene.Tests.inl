#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/Flux_GPUScene.h"

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
	ZENITH_ASSERT_EQ(auCmd[3], 0u, "word 3 = vertexOffset 0");
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
