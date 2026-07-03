#include "Zenith.h"
#include "Core/Zenith_Engine.h"

// Unified GPU-driven mesh scene builder — relocated out of Flux.cpp (god-file
// decomposition). Builds the per-frame (mesh,cull,material,VAT) bucket topology +
// GPU-scene object/draw-item records from the render snapshot: static models,
// instance-group foliage, and compute-skinned animated meshes. Pure relocation of
// Flux_RendererImpl::SyncUnifiedBucketsFromSnapshot (+ its three per-source
// extractors) and the file-static BuildStaticSubmeshDesc helper — no logic change.
#include "Flux/Flux_RendererImpl.h"
#include "Flux/Flux_GraphicsImpl.h"                       // m_xBlankMaterial
#include "Flux/Flux_GPUScene.h"                           // GPU-scene record types + build helpers
#include "Flux/Flux_MeshGeometryRegistry.h"               // mesh-geometry residency registry
#include "Flux/Flux_ModelInstance.h"                      // Flux_ModelInstance (snapshot item meshes)
#include "Flux/MeshGeometry/Flux_MeshInstance.h"          // per-submesh mesh-instance identity + bounds
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"     // skinning matrices + MAX_BONES
#include "Flux/UnifiedMesh/Flux_Skinning.h"               // bone palette, skin jobs, skinned-pose/id registries
#include "Flux/SceneGraph/Flux_RenderSceneSnapshot.h"     // snapshot + Flux_RenderSceneItem
#include "Flux/InstancedMeshes/Flux_InstancedMeshesImpl.h"// instance groups (foliage source)
#include "Flux/InstancedMeshes/Flux_InstanceGroup.h"      // Flux_InstanceGroup / bounds / anim data
#include "AssetHandling/Zenith_MaterialAsset.h"           // material blend mode + resolved params
#include "AssetHandling/Zenith_MeshAsset.h"               // mesh-asset geometry identity
#ifdef ZENITH_TOOLS
#include "Flux/RenderViews/Flux_MaterialPreviewController.h" // per-frame preview-view drive (complete type for Update())
#endif

// Build a static GPU-scene submesh descriptor from a mesh instance + material — resolving the
// shared mesh geometry (referencing it for this sync's residency refcount) and the cull mode /
// in-session material identity / local bounding sphere. Returns false to SKIP the submesh: a null
// mesh, no shareable geometry identity, a geometry build failure, or a translucent/additive
// material (those divert to the forward Translucency path). Shared by the static-model walk AND
// the static submeshes of mixed static+skinned animated models, so both stay byte-identical.
static bool BuildStaticSubmeshDesc(Flux_MeshGeometryRegistry& xRegistry, Flux_MeshInstance* pxMesh,
	Zenith_MaterialAsset* pxMat, Zenith_MaterialAsset* pxBlankMaterial, Flux_GPUSceneSourceSubmesh& xOut)
{
	if (pxMesh == nullptr)
	{
		return false;
	}
	if (pxMat == nullptr)
	{
		pxMat = pxBlankMaterial;   // stable blank-material identity (never a null key)
	}

	// Translucent / additive submeshes belong on the forward Translucency path.
	const MaterialBlendMode eBlend = pxMat->GetResolved().m_xParams.m_eBlendMode;
	if (eBlend == MATERIAL_BLEND_TRANSLUCENT || eBlend == MATERIAL_BLEND_ADDITIVE)
	{
		return false;
	}

	// Stable mesh-geometry identity -> meshGeometryId.
	Flux_MeshGeometryKey xMeshKey;
	if (Zenith_MeshAsset* pxAsset = pxMesh->GetSourceAsset())
	{
		xMeshKey.m_pvIdentity = pxAsset;
		xMeshKey.m_uKind = FLUX_MESH_GEOMETRY_SOURCE_ASSET;
	}
	else if (const Flux_MeshGeometry* pxProc = pxMesh->GetProceduralGeometry())
	{
		xMeshKey.m_pvIdentity = pxProc;
		xMeshKey.m_uKind = FLUX_MESH_GEOMETRY_SOURCE_PROCEDURAL;
	}
	else
	{
		return false;   // no shareable mesh identity
	}
	const u_int uMeshGeometryId = xRegistry.Reference(xMeshKey);
	if (uMeshGeometryId == uFLUX_INVALID_MESH_GEOMETRY_ID)
	{
		return false;   // build failed (only the real provider can fail)
	}

	const Zenith_AABB& xLocal = pxMesh->GetLocalBounds();
	xOut.m_uMeshGeometryId    = uMeshGeometryId;
	xOut.m_uCullMode          = pxMat->GetResolved().m_xParams.m_bTwoSided ? uFLUX_GPUSCENE_CULL_TWO_SIDED : uFLUX_GPUSCENE_CULL_ONE_SIDED;
	// SAFETY (pointer-as-id): the live Zenith_MaterialAsset* / Flux_AnimationTexture* is stored
	// as a u_int64 bucket-key "in-session grouping identity" and reinterpreted back CPU-side in
	// Flux_UnifiedMesh.cpp (ResolveBucketVAT / the material gather). This is sound ONLY because
	// these bucket keys are REBUILT EVERY FRAME from the live snapshot / instance groups on the
	// main thread, so a retired asset can never survive into a later frame's key — no cross-frame
	// use-after-free. (P5 will replace this with a generation-stamped handle.)
	xOut.m_ulMaterialAssetId  = reinterpret_cast<u_int64>(pxMat);   // in-session grouping identity
	xOut.m_ulVATTextureId     = 0u;   // static meshes carry no VAT
	xOut.m_xLocalBoundsSphere = Flux_LocalBoundsSphereFromAABB(xLocal.m_xMin, xLocal.m_xMax);
	xOut.m_uColorTintPacked   = uFLUX_GPUSCENE_TINT_WHITE;
	xOut.m_uFlags             = 0u;
	return true;
}

// Stage 4.3 (TAA): record one object's previous-frame world matrix — index-locked to
// m_xUnifiedGPUScene.m_xObjects (called immediately after EACH append) — and remember this
// frame's matrix as next frame's prev. A miss / id-0 (foliage, external) yields prev == current,
// i.e. camera-only velocity (sub-texel, invisible), which is the correct degrade.
void Flux_RendererImpl::RecordUnifiedPrevTransform(u_int64 ulEntityId, const Zenith_Maths::Matrix4& xCurrentWorld)
{
	Zenith_Maths::Matrix4 xPrev;
	if (!m_xUnifiedPrevTransformCache.TryGetPrev(ulEntityId, xPrev))
	{
		xPrev = xCurrentWorld;
	}
	m_axUnifiedPrevTransforms.PushBack(xPrev);
	m_xUnifiedPrevTransformCache.RecordCurrent(ulEntityId, xCurrentWorld);
}

void Flux_RendererImpl::SyncUnifiedBucketsFromSnapshot()
{
	const Flux_RenderSceneSnapshot& xSnapshot = *m_pxSceneSnapshot;
	Zenith_MaterialAsset* pxBlankMaterial = g_xEngine.FluxGraphics().m_xBlankMaterial.GetDirect();

	// Drive the material-preview controller (panel liveness -> view activation +
	// camera/sun staging + preview-mesh external-item submission) BEFORE the sync
	// consumes external items.
#ifdef ZENITH_TOOLS
	g_xEngine.MaterialPreview().Update();
#endif

	// ONE sync covers both sources: snapshot statics (Zenith_ModelComponent meshes) AND
	// instance-group foliage (trees). The mesh-geometry residency refcount + the GPU-scene
	// record build + the bucket-topology refcount diff all run over this single main-thread
	// walk (the proven WS7 single-writer shape).
	//
	// NOTE — this deliberately does NOT call RequestGraphRebuild() on a bucket-topology change.
	// The unified path's graph structure is bucket-count-INDEPENDENT: a fixed set of passes
	// (skinning -> reset -> cull -> GBuffer, plus the 4 shadow cascades) drives N per-bucket draws
	// inside ONE pass each, over two persistent cull-output buffers that grow in place (reusing the
	// same Flux_Buffer object, so the graph's barrier keys stay valid). So adding/retiring buckets
	// changes only per-frame dispatch/draw counts, never the pass graph. The bucket registry's
	// topology-change flag / high-water count are still computed (and unit-tested) as an available
	// signal for a future per-pass design, but the renderer has nothing to rebuild.
	m_xUnifiedMeshGeometryRegistry.BeginSync();
	Flux_BeginGPUSceneBuild(m_xUnifiedGPUScene, m_xUnifiedBucketRegistry);

	// Stage 4.3 (TAA): open the per-object previous-transform frame BEFORE the extractors.
	// BeginFrame ping-pongs prev<-cur (last frame's records) then clears cur, so it MUST run once
	// before any RecordUnifiedPrevTransform this frame. The parallel array is rebuilt in lockstep
	// with m_xObjects (one push per append). Run every frame so prev data exists the instant the
	// velocity latch turns on; GatherUnifiedPacket uploads it only when velocity is active.
	m_xUnifiedPrevTransformCache.BeginFrame();
	m_axUnifiedPrevTransforms.Clear();

	// Three independent data sources fill the one unified GPU scene; each is extracted
	// into its own helper below for readability (pure factoring — no behaviour change).
	// The refcount-diff bracket (mesh-geometry BeginSync/EndSync + the GPU-scene build
	// Begin/End) stays here, around all three.
	ExtractSnapshotStaticBuckets(xSnapshot, pxBlankMaterial);   // Zenith_ModelComponent static meshes
	ExtractInstanceGroupBuckets(pxBlankMaterial);               // instance-group foliage (trees)
	ExtractSkinnedBuckets(xSnapshot, pxBlankMaterial);          // animated skinned models (compute-skinned to the arena)
	ExtractExternalSceneItems(pxBlankMaterial);                 // renderer-level submissions (the material-preview mesh)

	Flux_EndGPUSceneBuild(m_xUnifiedGPUScene, m_xUnifiedBucketRegistry);
	m_xUnifiedMeshGeometryRegistry.EndSync();

	// Stage 4.3: the prev-transform array MUST stay index-locked to the GPU-scene objects (one
	// push per append). A mismatch means an append site was missed — the velocity VS would then
	// read the wrong / out-of-range prev matrix. This tripwire makes any desync loud + immediate.
	Zenith_Assert(m_axUnifiedPrevTransforms.GetSize() == m_xUnifiedGPUScene.m_xObjects.GetSize(),
		"Flux_RendererImpl: prev-transform array (%u) desynced from GPU-scene objects (%u) — a Flux_AppendGPUScene* site is missing its RecordUnifiedPrevTransform",
		m_axUnifiedPrevTransforms.GetSize(), m_xUnifiedGPUScene.m_xObjects.GetSize());

#ifdef ZENITH_DEBUG
	// One-shot proof the build ran on a real (non-empty) scene. Silent on empty snapshots
	// (Sokoban/Marble/Survival render no model-component static meshes), fires once on the
	// first populated scene. Verified on RenderTest: identical-mesh items de-dupe to one
	// shared geometry + per-(cull,material) buckets — mesh sharing + indirect-draw batching.
	static bool ls_bLoggedUnifiedScene = false;
	if (!ls_bLoggedUnifiedScene && m_xUnifiedGPUScene.m_xObjects.GetSize() > 0u)
	{
		ls_bLoggedUnifiedScene = true;
		Zenith_Log(LOG_CATEGORY_RENDERER,
			"[UnifiedMesh] scene built: %u objects, %u draw-items, %u buckets, %u meshes",
			m_xUnifiedGPUScene.m_xObjects.GetSize(), m_xUnifiedGPUScene.m_xDrawItems.GetSize(),
			m_xUnifiedBucketRegistry.GetLiveBucketCount(), m_xUnifiedMeshGeometryRegistry.GetLiveCount());
	}
#endif
}

// Source 1/3 — snapshot statics (Zenith_ModelComponent meshes). Skips animated-skinned
// items (ExtractSkinnedBuckets owns those); one static GPU-scene item per model.
void Flux_RendererImpl::ExtractSnapshotStaticBuckets(const Flux_RenderSceneSnapshot& xSnapshot, Zenith_MaterialAsset* pxBlankMaterial)
{
	// ---- snapshot statics ----
	Flux_GPUSceneSourceItem xItem;   // reused per model (cleared each iteration)
	const Zenith_Vector<Flux_RenderSceneItem>& xItems = xSnapshot.Items();
	for (u_int uItem = 0; uItem < xItems.GetSize(); ++uItem)
	{
		const Flux_RenderSceneItem& xSrc = xItems.Get(uItem);
		Flux_ModelInstance* pxModel = xSrc.m_pxModelInstance;
		// Animated-skinned models draw via the unified compute-skinning path (appended as
		// per-skinned buckets below); skip them in this static-mesh walk.
		if (pxModel == nullptr || xSrc.m_bAnimatedSkinned)
		{
			continue;
		}

		xItem.m_xSubmeshes.Clear();
		xItem.m_xWorldMatrix     = xSrc.m_xWorldMatrix;
		xItem.m_uFlags           = 0u;   // statics carry no VAT
		xItem.m_uVATAnimPacked   = 0u;
		xItem.m_uVATAnimTimeBits = 0u;

		const uint32_t uNumMeshes = pxModel->GetNumMeshes();
		for (uint32_t uMesh = 0; uMesh < uNumMeshes; ++uMesh)
		{
			Flux_GPUSceneSourceSubmesh xSub;
			if (BuildStaticSubmeshDesc(m_xUnifiedMeshGeometryRegistry, pxModel->GetMeshInstance(uMesh),
				pxModel->GetMaterial(uMesh), pxBlankMaterial, xSub))
			{
				xItem.m_xSubmeshes.PushBack(xSub);
			}
		}

		if (xItem.m_xSubmeshes.GetSize() > 0u)
		{
			Flux_AppendGPUSceneItem(xItem, m_xUnifiedBucketRegistry, m_xUnifiedGPUScene);
			RecordUnifiedPrevTransform(xSrc.m_ulEntityIDPacked, xSrc.m_xWorldMatrix);   // Stage 4.3 (index-locked to the append above)
		}
	}
}

// Source 2/3 — instance-group foliage (trees). Each enabled instance becomes one
// GPUSceneObject (world transform + VAT anim) + one draw-item in the group's
// (mesh,cull,material,VAT) bucket; identical meshes de-dupe to one shared VB/IB + one
// indirect draw (instanceCount = survivors after the GPU cull).
void Flux_RendererImpl::ExtractInstanceGroupBuckets(Zenith_MaterialAsset* pxBlankMaterial)
{
	// ---- instance-group foliage (Stage 3b) ----
	// Each enabled instance becomes one GPUSceneObject (its world transform + VAT anim) and one
	// draw-item in the group's (mesh, cull, material, VAT) bucket. The mesh-geometry registry
	// de-dupes geometry across groups + instances, so N identical trees collapse to ONE shared
	// VB/IB + ONE indirect draw (instanceCount = surviving instances after the GPU cull).
	Flux_InstancedMeshesImpl& xInstanced = g_xEngine.InstancedMeshes();
	for (u_int uGroup = 0; uGroup < xInstanced.m_apxInstanceGroups.GetSize(); ++uGroup)
	{
		Flux_InstanceGroup* pxGroup = xInstanced.m_apxInstanceGroups.Get(uGroup);
		if (pxGroup == nullptr || pxGroup->GetMesh() == nullptr || pxGroup->GetInstanceCount() == 0u)
		{
			continue;
		}

		Zenith_MaterialAsset* pxMat = pxGroup->GetMaterial();
		if (pxMat == nullptr)
		{
			pxMat = pxBlankMaterial;
		}
		const MaterialBlendMode eBlend = pxMat->GetResolved().m_xParams.m_eBlendMode;
		if (eBlend == MATERIAL_BLEND_TRANSLUCENT || eBlend == MATERIAL_BLEND_ADDITIVE)
		{
			continue;   // forward path (mirror the static filter; instanced foliage is opaque/masked)
		}

		Flux_MeshInstance* pxMesh = pxGroup->GetMesh();
		Flux_MeshGeometryKey xMeshKey;
		if (Zenith_MeshAsset* pxAsset = pxMesh->GetSourceAsset())
		{
			xMeshKey.m_pvIdentity = pxAsset;
			xMeshKey.m_uKind = FLUX_MESH_GEOMETRY_SOURCE_ASSET;
		}
		else if (const Flux_MeshGeometry* pxProc = pxMesh->GetProceduralGeometry())
		{
			xMeshKey.m_pvIdentity = pxProc;
			xMeshKey.m_uKind = FLUX_MESH_GEOMETRY_SOURCE_PROCEDURAL;
		}
		else
		{
			continue;
		}
		const u_int uMeshGeometryId = m_xUnifiedMeshGeometryRegistry.Reference(xMeshKey);
		if (uMeshGeometryId == uFLUX_INVALID_MESH_GEOMETRY_ID)
		{
			continue;
		}

		Flux_AnimationTexture* pxVAT = pxGroup->GetAnimationTexture();
		const bool bGroupHasVAT = (pxVAT != nullptr);

		Flux_GPUSceneBucketKey xKey;
		xKey.m_uMeshGeometryId   = uMeshGeometryId;
		xKey.m_uCullMode         = pxMat->GetResolved().m_xParams.m_bTwoSided ? uFLUX_GPUSCENE_CULL_TWO_SIDED : uFLUX_GPUSCENE_CULL_ONE_SIDED;
		// Pointer-as-id: same per-frame-rebuilt lifetime contract as BuildStaticSubmeshDesc's
		// SAFETY note above — the material + VAT pointers are stored as in-session grouping ids.
		xKey.m_ulMaterialAssetId = reinterpret_cast<u_int64>(pxMat);
		xKey.m_ulVATTextureId    = bGroupHasVAT ? reinterpret_cast<u_int64>(pxVAT) : 0u;

		const Flux_InstanceBounds& xBounds = pxGroup->GetBounds();
		const Zenith_Maths::Vector4 xSphere(xBounds.m_xCenter.x, xBounds.m_xCenter.y, xBounds.m_xCenter.z, xBounds.m_fRadius);

		const Zenith_Vector<Zenith_Maths::Matrix4>& axTransforms = pxGroup->GetTransforms();
		const Zenith_Vector<Flux_InstanceAnimData>& axAnim       = pxGroup->GetAnimData();
		const u_int uCount = pxGroup->GetInstanceCount();
		for (u_int uInst = 0; uInst < uCount; ++uInst)
		{
			const Flux_InstanceAnimData& xAnim = axAnim.Get(uInst);
			if (xAnim.m_uFlags == 0u)
			{
				continue;   // disabled slot (matches Flux_InstanceGroup::ComputeVisibleIndices)
			}

			// VAT is per-instance: bit 1 of the instance flags + the group having a VAT texture.
			const bool  bVAT       = bGroupHasVAT && ((xAnim.m_uFlags & 2u) != 0u);
			const u_int uObjFlags  = bVAT ? uFLUX_GPUSCENE_OBJFLAG_VAT : 0u;
			const u_int uVATPacked = Flux_PackVATAnim(xAnim.m_uAnimationIndex, xAnim.m_uFrameCount);
			u_int uVATTimeBits = 0u;
			memcpy(&uVATTimeBits, &xAnim.m_fAnimTime, sizeof(uVATTimeBits));

			Flux_AppendGPUSceneInstance(m_xUnifiedBucketRegistry, m_xUnifiedGPUScene,
				axTransforms.Get(uInst), uObjFlags, uVATPacked, uVATTimeBits, xKey, xSphere, xAnim.m_uColorTint);
			RecordUnifiedPrevTransform(0u, axTransforms.Get(uInst));   // Stage 4.3: foliage has no stable id -> camera-only velocity (wind sway absorbed by the resolve clamp)
		}
	}
}

// Source 3/3 — animated skinned models (Stage 5). Resets the per-frame skin builders,
// compute-skins each animated submesh-instance to the shared arena via its own per-instance
// skinned bucket, and appends any non-skinned submeshes of an animated model as static geometry.
void Flux_RendererImpl::ExtractSkinnedBuckets(const Flux_RenderSceneSnapshot& xSnapshot, Zenith_MaterialAsset* pxBlankMaterial)
{
	const Zenith_Vector<Flux_RenderSceneItem>& xItems = xSnapshot.Items();

	// ---- animated skinned models (Stage 5) ----
	// Reset the per-frame skin builders ALWAYS (so GatherUnifiedPacket sees 0 jobs when the
	// toggle is off / there is no animated content). When enabled, walk the snapshot's
	// animated-skinned items: each skinned submesh-instance is compute-skinned to the arena and
	// drawn through the unified kernels via its own per-instance skinned bucket.
	// Evict skinned-pose entries not referenced by the PREVIOUS sync (frees their GPU IB/VB +
	// re-packs the bind-pose pool) BEFORE this sync's walk, so this frame's skin-jobs see the
	// re-packed pool bases consistently, then open this sync's references (refcount-diff).
	m_xUnifiedSkinnedPoseRegistry.BeginFrameEvictingPrevious();

	m_xUnifiedBonePalette.Begin(Flux_SkeletonInstance::MAX_BONES);
	// Stage 4.3b: assemble this frame's PREVIOUS palette (same bases as the current one) for
	// skeletal motion vectors. Runs every frame in lockstep with m_xUnifiedBonePalette so prev
	// poses are ready the instant the velocity latch turns on. EndFrame() closes it below.
	m_xUnifiedBonePaletteHistory.BeginFrame(Flux_SkeletonInstance::MAX_BONES);
	// NOTE: the bind-pose pool is PERSISTENT (grow-only between evictions) — owned by the pose registry.
	m_axUnifiedSkinJobs.Clear();
	m_xUnifiedSkinnedDrawById.Clear();
	m_xUnifiedSkinnedIdRegistry.BeginSync();   // stable skinned bucket ids: mark all unreferenced (EndSync after the walk recycles dropped instances)
	m_uUnifiedSkinMaxVerts      = 0u;
	m_uUnifiedSkinTotalOutVerts = 0u;
	// Animated skinned meshes route through the unified compute-skinning path: each skinned
	// submesh-instance is skinned to object space here, then drawn by the unified GPU-driven path.
	{
		u_int uArenaCursor = 0u;
		for (u_int uItem = 0; uItem < xItems.GetSize(); ++uItem)
		{
			const Flux_RenderSceneItem& xSrc = xItems.Get(uItem);
			Flux_ModelInstance* pxModel = xSrc.m_pxModelInstance;
			if (pxModel == nullptr || !xSrc.m_bAnimatedSkinned)
			{
				continue;
			}
			Flux_SkeletonInstance* pxSkeleton = pxModel->GetSkeletonInstance();
			if (pxSkeleton == nullptr)
			{
				continue;
			}

			// Dedup this instance's skeleton into the shared palette; fill its block on first sight.
			bool bNewSkel = false;
			const u_int uBonePaletteBase = m_xUnifiedBonePalette.GetOrAddSkeleton(
				reinterpret_cast<u_int64>(pxSkeleton), bNewSkel);
			if (bNewSkel)
			{
				const u_int uNumBones = pxSkeleton->GetNumBones();
				const Zenith_Maths::Matrix4* pxMats = pxSkeleton->GetSkinningMatrices();
				// GetOrAddSkeleton appended a MAX_BONES-sized block at uBonePaletteBase, so the fill
				// (bounded b < MAX_BONES) can never spill into the next skeleton's block.
				Zenith_Assert(uBonePaletteBase + Flux_SkeletonInstance::MAX_BONES <= m_xUnifiedBonePalette.Matrices().GetSize(),
					"Bone-palette block overruns the concatenated palette buffer");
				for (u_int b = 0; b < uNumBones && b < Flux_SkeletonInstance::MAX_BONES; ++b)
				{
					m_xUnifiedBonePalette.Matrices().Get(uBonePaletteBase + b) = pxMats[b];
				}
				// Stage 4.3b: emit this skeleton (at the SAME block base) into the prev palette —
				// last frame's matrices for these bones, or these matrices on first sight (prev ==
				// current => zero skeletal velocity). Once per distinct skeleton (this bNewSkel branch).
				m_xUnifiedBonePaletteHistory.SubmitSkeleton(
					reinterpret_cast<u_int64>(pxSkeleton), uBonePaletteBase, pxMats,
					(uNumBones < Flux_SkeletonInstance::MAX_BONES) ? uNumBones : Flux_SkeletonInstance::MAX_BONES);
			}

			// Mixed static+skinned model: the static walk above skipped the whole m_bAnimatedSkinned
			// item, so this walk owns BOTH the skinned submeshes (compute-skinned below) AND any
			// non-skinned submeshes — the latter accumulate into one static GPU-scene item (the
			// model's world matrix, no VAT/skin) appended after the submesh loop.
			Flux_GPUSceneSourceItem xStaticOfAnimated;
			xStaticOfAnimated.m_xWorldMatrix     = xSrc.m_xWorldMatrix;
			xStaticOfAnimated.m_uFlags           = 0u;
			xStaticOfAnimated.m_uVATAnimPacked   = 0u;
			xStaticOfAnimated.m_uVATAnimTimeBits = 0u;

			const uint32_t uNumMeshes = pxModel->GetNumMeshes();
			for (uint32_t uMesh = 0; uMesh < uNumMeshes; ++uMesh)
			{
				if (pxModel->GetSkinnedMeshInstance(uMesh) == nullptr)
				{
					// Non-skinned submesh of an animated model -> draw it as STATIC geometry (the
					// model's world matrix). Skinned submeshes (below) are compute-skinned instead.
					Flux_GPUSceneSourceSubmesh xStaticSub;
					if (BuildStaticSubmeshDesc(m_xUnifiedMeshGeometryRegistry, pxModel->GetMeshInstance(uMesh),
						pxModel->GetMaterial(uMesh), pxBlankMaterial, xStaticSub))
					{
						xStaticOfAnimated.m_xSubmeshes.PushBack(xStaticSub);
					}
					continue;
				}
				Flux_MeshInstance* pxMeshInst = pxModel->GetMeshInstance(uMesh);
				if (pxMeshInst == nullptr)
				{
					continue;
				}
				Flux_SkinnedPoseEntry* pxPose = m_xUnifiedSkinnedPoseRegistry.Reference(pxMeshInst->GetSourceAsset());
				if (pxPose == nullptr || pxPose->m_uNumVerts == 0u)
				{
#ifdef ZENITH_DEBUG
					// A skinned submesh whose bind-pose build failed renders NOTHING — make that
					// detectable rather than a silent drop (the gather is the single writer, so this
					// is the one place a missing skinned pose surfaces).
					if (pxPose == nullptr)
					{
						Zenith_Log(LOG_CATEGORY_RENDERER,
							"[UnifiedMesh] skinned submesh %u of an animated model has no bind-pose (skipped)", uMesh);
					}
#endif
					continue;
				}

				Zenith_MaterialAsset* pxMat = pxModel->GetMaterial(uMesh);
				if (pxMat == nullptr)
				{
					pxMat = pxBlankMaterial;   // blank fallback (unifies the null-material policy)
				}
				const MaterialBlendMode eBlend = pxMat->GetResolved().m_xParams.m_eBlendMode;
				if (eBlend == MATERIAL_BLEND_TRANSLUCENT || eBlend == MATERIAL_BLEND_ADDITIVE)
				{
					continue;   // forward path
				}

				// Stable base in the persistent bind-pose pool (the words were appended once when the
				// pose entry was first built — see Flux_SkinnedPoseRegistry::Reference).
				const u_int uBindPoseBase = pxPose->m_uPoolVertBase;

				const u_int uOutVertBase = uArenaCursor;
				uArenaCursor += pxPose->m_uNumVerts;
				if (pxPose->m_uNumVerts > m_uUnifiedSkinMaxVerts)
				{
					m_uUnifiedSkinMaxVerts = pxPose->m_uNumVerts;
				}

				Flux_GPUSkinJob xJob;
				xJob.m_uBindPoseVertBase = uBindPoseBase;
				xJob.m_uOutVertBase      = uOutVertBase;
				xJob.m_uVertCount        = pxPose->m_uNumVerts;
				xJob.m_uBonePaletteBase  = uBonePaletteBase;
				m_axUnifiedSkinJobs.PushBack(xJob);

				// Stable per-instance id (recycled across frames) -> the skinned bucket key. Identity =
				// (this entity's skeleton instance, the submesh source asset, the submesh slot).
				Flux_SkinnedInstanceKey xInstKey;
				xInstKey.m_pvSkeleton   = pxSkeleton;
				xInstKey.m_pvMeshAsset  = pxMeshInst->GetSourceAsset();
				xInstKey.m_uSubmeshSlot = uMesh;
				const u_int uStableId = m_xUnifiedSkinnedIdRegistry.Reference(xInstKey);

				Flux_UnifiedSkinnedDraw xSD;
				xSD.m_pxMesh        = pxPose->m_pxMesh;
				xSD.m_uVertexOffset = uOutVertBase;
				m_xUnifiedSkinnedDrawById.Insert(uStableId, xSD);

				Flux_GPUSceneBucketKey xKey;
				xKey.m_uMeshGeometryId   = uFLUX_GPUSCENE_SKINNED_MESH_BIT | uStableId;
				xKey.m_uCullMode         = pxMat->GetResolved().m_xParams.m_bTwoSided ? uFLUX_GPUSCENE_CULL_TWO_SIDED : uFLUX_GPUSCENE_CULL_ONE_SIDED;
				// Pointer-as-id: same per-frame-rebuilt lifetime contract as BuildStaticSubmeshDesc's SAFETY note.
				xKey.m_ulMaterialAssetId = reinterpret_cast<u_int64>(pxMat);
				xKey.m_ulVATTextureId    = 0u;   // skinned != VAT (keeps ResolveBucketVAT safe)

				const Zenith_AABB& xLocal = pxMeshInst->GetLocalBounds();
				const Zenith_Maths::Vector4 xSphere = Flux_InflateBoundsSphere(
					Flux_LocalBoundsSphereFromAABB(xLocal.m_xMin, xLocal.m_xMax), fFLUX_SKIN_BOUNDS_INFLATION);

				Flux_AppendGPUSceneSkinnedInstance(m_xUnifiedBucketRegistry, m_xUnifiedGPUScene,
					xSrc.m_xWorldMatrix, uBonePaletteBase, xKey, xSphere, uFLUX_GPUSCENE_TINT_WHITE);
				RecordUnifiedPrevTransform(xSrc.m_ulEntityIDPacked, xSrc.m_xWorldMatrix);   // Stage 4.3: one per skinned submesh append (same entity => same prev)
			}

			// Append the model's non-skinned submeshes (if any) as one static item — drawn via the
			// model's world matrix, sharing the static buckets with regular static models.
			if (xStaticOfAnimated.m_xSubmeshes.GetSize() > 0u)
			{
				Flux_AppendGPUSceneItem(xStaticOfAnimated, m_xUnifiedBucketRegistry, m_xUnifiedGPUScene);
				RecordUnifiedPrevTransform(xSrc.m_ulEntityIDPacked, xSrc.m_xWorldMatrix);   // Stage 4.3: animated model's non-skinned submeshes (index-locked)
			}
		}
		m_uUnifiedSkinTotalOutVerts = uArenaCursor;
		m_xUnifiedSkinnedIdRegistry.EndSync();   // recycle stable ids for skinned instances no longer drawn
	}

	// Stage 4.3b: this frame's palette becomes next frame's history (skeletons not seen this
	// frame drop out and restart at prev == current). Must follow every SubmitSkeleton above.
	m_xUnifiedBonePaletteHistory.EndFrame();
}

// Source 4/4 - renderer-level external submissions (the material-preview mesh).
// Same descriptor build as the static walk; the item view mask (preview-only for
// the preview mesh) rides into the draw-item so the GPU cull scopes it to its
// target view slots. Translucent/additive-material items divert to the preserved
// translucent list instead of an opaque submesh: the owner keeps mesh + material
// alive for the frame, and the Translucency per-view gather (a pass Prepare,
// which runs AFTER this sync) consumes it. Consumes then clears this frame's
// submissions.
void Flux_RendererImpl::ExtractExternalSceneItems(Zenith_MaterialAsset* pxBlankMaterial)
{
	m_axExternalTranslucentItems.Clear();

	for (u_int u = 0; u < m_axExternalSceneItems.GetSize(); ++u)
	{
		const Flux_ExternalSceneItem& xExt = m_axExternalSceneItems.Get(u);

		// Route translucent/additive items to the forward Translucency path (the
		// opaque build below would skip them anyway — this preserves them for the
		// per-view gather, with the material pre-resolved so consumers never see null).
		if (xExt.m_pxMeshInstance != nullptr)
		{
			Zenith_MaterialAsset* pxMat = xExt.m_pxMaterial != nullptr ? xExt.m_pxMaterial : pxBlankMaterial;
			const MaterialBlendMode eBlend = pxMat->GetResolved().m_xParams.m_eBlendMode;
			if (eBlend == MATERIAL_BLEND_TRANSLUCENT || eBlend == MATERIAL_BLEND_ADDITIVE)
			{
				Flux_ExternalSceneItem xTranslucent = xExt;
				xTranslucent.m_pxMaterial = pxMat;
				m_axExternalTranslucentItems.PushBack(xTranslucent);
				continue;
			}
		}

		Flux_GPUSceneSourceItem xSrc;
		xSrc.m_xWorldMatrix = xExt.m_xWorldMatrix;
		xSrc.m_uViewMask    = xExt.m_uViewMask;

		Flux_GPUSceneSourceSubmesh xSub;
		if (!BuildStaticSubmeshDesc(m_xUnifiedMeshGeometryRegistry, xExt.m_pxMeshInstance,
			xExt.m_pxMaterial, pxBlankMaterial, xSub))
		{
			continue;   // invalid -> not an opaque unified draw
		}
		xSrc.m_xSubmeshes.PushBack(xSub);
		Flux_AppendGPUSceneItem(xSrc, m_xUnifiedBucketRegistry, m_xUnifiedGPUScene);
		RecordUnifiedPrevTransform(0u, xSrc.m_xWorldMatrix);   // Stage 4.3: external/preview item has no stable id -> camera-only velocity
	}
	m_axExternalSceneItems.Clear();
}
