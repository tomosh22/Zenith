#include "Zenith.h"
#include "Flux/Flux_GPUScene.h"

// ============================================================================
// Flux GPU-scene builder + bucket registry. Pure CPU — no GPU access, no renderer
// boot — so the whole surface is headless-unit-testable (the tests live in
// Flux_GPUScene.Tests.inl, hosted in an already-linked TU; see the NOTE at the
// bottom of this file).
// ============================================================================

// Flux_GPUSceneBucketRegistry's BeginSync / Reference / EndSync / lookups now come
// from Flux_RefcountDiffRegistry<Flux_GPUSceneBucketKey> (see Flux_RefcountDiffRegistry.h);
// only the GPU-scene BUILD helpers below remain in this TU.

void Flux_BeginGPUSceneBuild(Flux_GPUSceneBuildResult& xOut, Flux_GPUSceneBucketRegistry& xRegistry)
{
	xOut.m_xObjects.Clear();
	xOut.m_xDrawItems.Clear();
	xRegistry.BeginSync();
}

void Flux_AppendGPUSceneItem(const Flux_GPUSceneSourceItem& xItem,
	Flux_GPUSceneBucketRegistry& xRegistry, Flux_GPUSceneBuildResult& xOut)
{
	const u_int uObjectIndex = xOut.m_xObjects.GetSize();
	Flux_GPUSceneObject xObj;
	Flux_BuildGPUSceneObject(xObj, xItem.m_xWorldMatrix, xItem.m_uFlags, xItem.m_uBonePaletteRef,
		xItem.m_uVATAnimPacked, xItem.m_uVATAnimTimeBits);
	xOut.m_xObjects.PushBack(xObj);

	for (u_int uSub = 0; uSub < xItem.m_xSubmeshes.GetSize(); ++uSub)
	{
		const Flux_GPUSceneSourceSubmesh& xSub = xItem.m_xSubmeshes.Get(uSub);

		Flux_GPUSceneBucketKey xKey;
		xKey.m_uMeshGeometryId   = xSub.m_uMeshGeometryId;
		xKey.m_uCullMode         = xSub.m_uCullMode;
		xKey.m_ulMaterialAssetId = xSub.m_ulMaterialAssetId;
		xKey.m_ulVATTextureId    = xSub.m_ulVATTextureId;

		const u_int uBucket = xRegistry.Reference(xKey);

		Flux_GPUSceneDrawItem xDrawItem;
		Flux_BuildGPUSceneDrawItem(xDrawItem, uObjectIndex, uBucket,
			xSub.m_uColorTintPacked, Flux_PackDrawItemViewMask(xSub.m_uFlags, xItem.m_uViewMask),
			xSub.m_xLocalBoundsSphere);
		xOut.m_xDrawItems.PushBack(xDrawItem);
	}
}

void Flux_AppendGPUSceneInstance(Flux_GPUSceneBucketRegistry& xRegistry, Flux_GPUSceneBuildResult& xOut,
	const Zenith_Maths::Matrix4& xModel, u_int uObjFlags, u_int uVATAnimPacked, u_int uVATAnimTimeBits,
	const Flux_GPUSceneBucketKey& xKey, const Zenith_Maths::Vector4& xLocalBoundsSphere, u_int uColorTintPacked,
	u_int uViewMask)
{
	const u_int uObjectIndex = xOut.m_xObjects.GetSize();
	Flux_GPUSceneObject xObj;
	Flux_BuildGPUSceneObject(xObj, xModel, uObjFlags, 0u, uVATAnimPacked, uVATAnimTimeBits);
	xOut.m_xObjects.PushBack(xObj);

	const u_int uBucket = xRegistry.Reference(xKey);

	Flux_GPUSceneDrawItem xDrawItem;
	Flux_BuildGPUSceneDrawItem(xDrawItem, uObjectIndex, uBucket, uColorTintPacked,
		Flux_PackDrawItemViewMask(0u, uViewMask), xLocalBoundsSphere);
	xOut.m_xDrawItems.PushBack(xDrawItem);
}

void Flux_AppendGPUSceneSkinnedInstance(Flux_GPUSceneBucketRegistry& xRegistry, Flux_GPUSceneBuildResult& xOut,
	const Zenith_Maths::Matrix4& xModel, u_int uBonePaletteBase,
	const Flux_GPUSceneBucketKey& xKey, const Zenith_Maths::Vector4& xLocalBoundsSphere, u_int uColorTintPacked,
	u_int uViewMask)
{
	const u_int uObjectIndex = xOut.m_xObjects.GetSize();
	Flux_GPUSceneObject xObj;
	// SKINNED object: carries the bone-palette base (consumed by the skinning compute via the
	// skin-job, mirrored here for completeness) + the SKINNED flag; no VAT.
	Flux_BuildGPUSceneObject(xObj, xModel, uFLUX_GPUSCENE_OBJFLAG_SKINNED, uBonePaletteBase, 0u, 0u);
	xOut.m_xObjects.PushBack(xObj);

	const u_int uBucket = xRegistry.Reference(xKey);

	Flux_GPUSceneDrawItem xDrawItem;
	Flux_BuildGPUSceneDrawItem(xDrawItem, uObjectIndex, uBucket, uColorTintPacked,
		Flux_PackDrawItemViewMask(0u, uViewMask), xLocalBoundsSphere);
	xOut.m_xDrawItems.PushBack(xDrawItem);
}

void Flux_EndGPUSceneBuild(Flux_GPUSceneBuildResult& xOut, Flux_GPUSceneBucketRegistry& xRegistry)
{
	xRegistry.EndSync();
	xOut.m_bTopologyChanged = xRegistry.WasTopologyChangedThisSync();
}

void Flux_BuildGPUScene(const Zenith_Vector<Flux_GPUSceneSourceItem>& xItems,
	Flux_GPUSceneBucketRegistry& xRegistry, Flux_GPUSceneBuildResult& xOut)
{
	Flux_BeginGPUSceneBuild(xOut, xRegistry);
	for (u_int uItem = 0; uItem < xItems.GetSize(); ++uItem)
	{
		Flux_AppendGPUSceneItem(xItems.Get(uItem), xRegistry, xOut);
	}
	Flux_EndGPUSceneBuild(xOut, xRegistry);
}

void Flux_BuildBucketOffsets(const Zenith_Vector<u_int>& auCounts, Zenith_Vector<u_int>& auOffsetsOut)
{
	auOffsetsOut.Clear();
	u_int uRunning = 0u;
	for (u_int u = 0; u < auCounts.GetSize(); ++u)
	{
		auOffsetsOut.PushBack(uRunning);
		uRunning += auCounts.Get(u);
	}
}

float Flux_MaxScaleFromMatrix(const Zenith_Maths::Matrix4& xModel)
{
	const float fScaleX = glm::length(Zenith_Maths::Vector3(xModel[0]));
	const float fScaleY = glm::length(Zenith_Maths::Vector3(xModel[1]));
	const float fScaleZ = glm::length(Zenith_Maths::Vector3(xModel[2]));
	return glm::max(fScaleX, glm::max(fScaleY, fScaleZ));
}

bool Flux_CullDrawItemAgainstFrustum(const Zenith_Maths::Matrix4& xModel,
	const Zenith_Maths::Vector4& xLocalBoundsSphere, const Zenith_Maths::Vector4 axPlanes[6])
{
	const Zenith_Maths::Vector4 xWorldCenter4 = xModel * Zenith_Maths::Vector4(
		xLocalBoundsSphere.x, xLocalBoundsSphere.y, xLocalBoundsSphere.z, 1.0f);
	const Zenith_Maths::Vector3 xWorldCenter(xWorldCenter4.x, xWorldCenter4.y, xWorldCenter4.z);
	const float fWorldRadius = xLocalBoundsSphere.w * Flux_MaxScaleFromMatrix(xModel);

	for (int i = 0; i < 6; ++i)
	{
		const Zenith_Maths::Vector3 xNormal(axPlanes[i].x, axPlanes[i].y, axPlanes[i].z);
		if (glm::dot(xNormal, xWorldCenter) + axPlanes[i].w < -fWorldRadius)
		{
			return false;   // fully outside this plane → culled
		}
	}
	return true;
}

u_int64 Flux_HashGPUSceneForTest(const Flux_GPUSceneBuildResult& xResult)
{
	u_int64 uHash = 0xcbf29ce484222325ull;
	auto Bytes = [&uHash](const void* p, size_t n)
	{
		const u_int8* pb = static_cast<const u_int8*>(p);
		for (size_t i = 0; i < n; ++i) { uHash ^= pb[i]; uHash *= 0x100000001b3ull; }
	};

	// Full-struct byte feed is deterministic here: every pad field is explicitly
	// zeroed by the pack helpers and neither record has trailing compiler padding.
	const u_int uObjCount = xResult.m_xObjects.GetSize();
	Bytes(&uObjCount, sizeof(uObjCount));
	for (u_int u = 0; u < uObjCount; ++u)
	{
		const Flux_GPUSceneObject& xObj = xResult.m_xObjects.Get(u);
		Bytes(&xObj, sizeof(xObj));
	}

	const u_int uDrawCount = xResult.m_xDrawItems.GetSize();
	Bytes(&uDrawCount, sizeof(uDrawCount));
	for (u_int u = 0; u < uDrawCount; ++u)
	{
		const Flux_GPUSceneDrawItem& xDrawItem = xResult.m_xDrawItems.Get(u);
		Bytes(&xDrawItem, sizeof(xDrawItem));
	}

	return uHash;
}

// NOTE: the GPU-scene unit tests (Flux_GPUScene.Tests.inl) are intentionally
// hosted in an already-linked TU (Flux_MaterialTable.cpp), NOT here. Although the
// renderer now references Flux_BuildGPUScene (so this obj is pulled into the link),
// the test bodies are static-init registrations that /OPT:REF can still dead-strip
// from an obj whose only referenced symbols are elsewhere. Hosting the .Tests.inl
// in a TU whose surrounding code is referenced keeps the registrations alive in
// every config (the same idiom as Zenith_Physics.Tests.inl → Zenith_ColliderComponent.cpp).
