#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "Flux/Flux.h"
#include "Flux/Flux_PerFrame.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/SceneGraph/Flux_RenderSceneSnapshot.h"   // complete type for the by-ptr snapshot (alloc/rebuild/free)
#include "Flux/MeshGeometry/Flux_MeshInstance.h"        // Stage 0e: per-submesh mesh-instance identity + local bounds
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"   // Stage 5: per-instance skinning matrices + MAX_BONES
#include "AssetHandling/Zenith_MeshAsset.h"             // Stage 5: bind-pose vertex arrays for the skinned-pose store
#include "Flux/Skybox/Flux_SkyboxImpl.h"
#include "Flux/Terrain/Flux_TerrainImpl.h"
#ifdef ZENITH_WINDOWS
#include "Flux/Slang/Flux_SlangCompiler.h"
#endif
#ifdef ZENITH_TOOLS
#include "Flux/Gizmos/Flux_GizmosImpl.h"
#include "Flux/MaterialPreview/Flux_MaterialPreviewImpl.h"
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif
#include "Flux/Primitives/Flux_PrimitivesImpl.h"
#include "Flux/DeferredShading/Flux_DeferredShadingImpl.h"
#include "Flux/SSAO/Flux_SSAOImpl.h"
#include "Flux/Fog/Flux_FogImpl.h"
#include "Flux/Fog/Flux_VolumeFogImpl.h"
#include "Flux/Zenith_GameRenderFeatures.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "Flux/SDFs/Flux_SDFsImpl.h"
#include "Flux/Shadows/Flux_ShadowsImpl.h"
#include "Flux/Flux_PersistentSetLayouts.h"   // VIEW-set binding indices (Phase 5.4)
#include "Flux/Particles/Flux_ParticlesImpl.h"
#include "Flux/Text/Flux_TextImpl.h"
#include "Flux/Quads/Flux_QuadsImpl.h"
#include "Flux/InstancedMeshes/Flux_InstancedMeshesImpl.h"
#include "Flux/InstancedMeshes/Flux_InstanceGroup.h"      // Stage 3b: fold instance-group foliage into the unified scene
#include "Flux/HDR/Flux_HDRImpl.h"
#include "Flux/IBL/Flux_IBLImpl.h"
#include "Flux/HiZ/Flux_HiZImpl.h"
#include "Flux/SSR/Flux_SSRImpl.h"
#include "Flux/SSGI/Flux_SSGIImpl.h"
#include "Flux/Vegetation/Flux_GrassImpl.h"
#include "Flux/DynamicLights/Flux_DynamicLightsImpl.h"
#include "Flux/DynamicLights/Flux_LightClusteringImpl.h"
#include "Flux/Decals/Flux_DecalsImpl.h"
#include "DebugVariables/Zenith_DebugVariables.h"

#include "Flux/Flux_RendererImpl.h"
#include "Flux/Flux_FeatureRegistry.h"
#include "Flux/Slang/Flux_ShaderCatalog.h" // ValidateFeatureParity boot check

// (The frame counter that used to be read from here moved to FrameContext —
// g_xEngine.Frame().GetFrameIndex().)

void Flux_RendererImpl::QueueRenderPass(const Flux_RenderGraph* pxGraph,
	const Flux_RenderGraph_AttachmentRef* axColourAttachments, uint32_t uNumColour,
	const Flux_RenderGraph_AttachmentRef& xDepthStencil,
	bool bClearTargets, bool bDepthIsReadOnly, const Flux_RenderGraph_Pass* pxPass)
{
	Zenith_Assert(pxGraph != nullptr, "QueueRenderPass: graph pointer is null");
	Zenith_Assert(pxPass != nullptr, "QueueRenderPass: pass pointer is null — bypass path no longer supported");
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "QueueRenderPass: must be called from the main thread (Flux_RenderGraph::Execute)");
	Flux_RenderPassEntry xEntry;
	xEntry.m_pxGraph = pxGraph;
	for (uint32_t i = 0; i < uNumColour; i++) xEntry.m_axColourAttachments[i] = axColourAttachments[i];
	xEntry.m_uNumColourAttachments = uNumColour;
	xEntry.m_xDepthStencil = xDepthStencil;
	xEntry.m_pxPass = pxPass;
	xEntry.m_bClearTargets = bClearTargets;
	xEntry.m_bDepthIsReadOnly = bDepthIsReadOnly;
	m_xPendingRenderPasses.PushBack(xEntry);
}

void Flux_RendererImpl::AddResChangeCallback(void(*pfnCallback)())
{
	m_xResChangeCallbacks.PushBack(pfnCallback);
}

void Flux_RendererImpl::RecordFrame()
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "RecordFrame: must be called from the main thread (Flux_RenderGraph::Execute)");

	// Distribute the queued passes across worker threads. No work this frame →
	// drain the (already-empty) queue and report no render work so EndFrame skips
	// the render submit.
	Flux_WorkDistribution xWorkDistribution;
	xWorkDistribution.Clear();
	if (!PrepareFrame(xWorkDistribution))
	{
		m_bHasRenderWork = false;
		ClearPendingRenderPasses();
		return;
	}

	// Upload the GPU material table for this frame BEFORE any pass records. All
	// material registration (index assignment + record build) happened during the
	// gather Prepare callbacks (CallPrepareCallbacks ran before this, on the main
	// thread); this copies the CPU records into the current frame-in-flight buffer.
	// Frame-indexed + host-coherent → no graph barrier (Flux_FrameIndexedBufferBase
	// contract). Main-thread + before worker recording, so it is race-free.
	g_xEngine.FluxGraphics().MaterialTable().Upload();

	// Phase 5.1: write the current frame's persistent GLOBAL/VIEW descriptor sets from
	// the spine constant buffers, also BEFORE any worker records (write-before-bind).
	// Main-thread + frame-indexed sets → race-free, no graph barrier. The workers then
	// bind these persistent sets per pipeline instead of allocating sets 0/1 per draw.
	g_xEngine.FluxBackend().PreparePersistentSets(
		g_xEngine.FluxGraphics().GetGlobalConstantsBufferHandle(),
		g_xEngine.FluxGraphics().MaterialTable().GetSRV().m_xBufferDescHandle,
		g_xEngine.FluxGraphics().GetViewConstantsBufferHandle());

	// Phase 5.4: write the view-frequency SRVs promoted into the persistent VIEW set.
	// The CSM array is always allocated (cleared-to-far when shadows are disabled), so
	// VIEW binding 1 is always valid; consumers sample g_xViewSet.g_xCSM and declare the
	// graph Read() (enforced by the Flux_ViewSetBinding validator). Same write-before-bind
	// window as PreparePersistentSets.
	g_xEngine.FluxBackend().WritePersistentViewImage(
		Flux_PersistentSetLayouts::kuViewBinding_CSM,
		g_xEngine.Shadows().GetCSMArraySRV(),
		g_xEngine.FluxGraphics().m_xClampSampler);

	// Phase 5.4: the all-cascade ShadowMatrices SSBO is also a VIEW-frequency resource.
	// It is a frame-indexed Flux_DynamicReadWriteBuffer (graph-invisible by contract, like
	// g_xView / g_axMaterials), so GetShadowMatricesSRV() yields THIS frame's buffer view —
	// re-written into the persistent VIEW set every frame (no staleness). Updated earlier this
	// frame by UpdateShadowMatrices (hoisted to the Zenith_Core main-thread seam), which runs
	// before record.
	g_xEngine.FluxBackend().WritePersistentViewBuffer(
		Flux_PersistentSetLayouts::kuViewBinding_ShadowMatrices,
		g_xEngine.Shadows().GetShadowMatricesSRV());

	// Phase 5.4: the clustered-lighting read buffers also live in the persistent VIEW set.
	// g_xLightBuffer is a frame-indexed dynamic buffer (current-frame SRV each frame); the
	// cluster count/index buffers are GPU-only (graph-tracked — the LightClustering compute
	// writes them via UAV, consumers read these persistent SRVs with graph-driven barriers).
	// All three are always allocated post-init, so the descriptor is always valid — VIEW
	// bindings 3-5 must never be left unwritten (consumers statically sample them).
	g_xEngine.FluxBackend().WritePersistentViewBuffer(
		Flux_PersistentSetLayouts::kuViewBinding_LightBuffer,
		g_xEngine.DynamicLights().GetLightBufferSRV());
	g_xEngine.FluxBackend().WritePersistentViewBuffer(
		Flux_PersistentSetLayouts::kuViewBinding_ClusterLightCounts,
		g_xEngine.LightClustering().GetClusterLightCountsSRV());
	g_xEngine.FluxBackend().WritePersistentViewBuffer(
		Flux_PersistentSetLayouts::kuViewBinding_ClusterLightIndices,
		g_xEngine.LightClustering().GetClusterLightIndicesSRV());

	// Phase 5.4: the IBL trio (BRDF LUT + irradiance/prefiltered cubes) are graph-tracked
	// render attachments sampled by DeferredShading/Translucency/MaterialPreview. They use
	// the SAME combined-image-sampler write as g_xCSM (a cube's cube-ness lives in the image
	// view, so no special path); m_xRepeatSampler matches the per-pass BindSRV default
	// (render-identical). Always allocated; the hook rewrites the current SRV each frame so
	// an IBL re-bake (sky change) is absorbed.
	g_xEngine.FluxBackend().WritePersistentViewImage(
		Flux_PersistentSetLayouts::kuViewBinding_BRDFLUT,
		g_xEngine.IBL().GetBRDFLUTSRV(), g_xEngine.FluxGraphics().m_xRepeatSampler);
	g_xEngine.FluxBackend().WritePersistentViewImage(
		Flux_PersistentSetLayouts::kuViewBinding_IrradianceMap,
		g_xEngine.IBL().GetIrradianceMapSRV(), g_xEngine.FluxGraphics().m_xRepeatSampler);
	g_xEngine.FluxBackend().WritePersistentViewImage(
		Flux_PersistentSetLayouts::kuViewBinding_PrefilteredMap,
		g_xEngine.IBL().GetPrefilteredMapSRV(), g_xEngine.FluxGraphics().m_xRepeatSampler);

	// Drive the backend to record every queued pass directly into its worker
	// command buffers (Vulkan: parallel worker task; D3D12 null backend: serial
	// callback loop). Runs synchronously inside the render-task safe window and
	// BEFORE the frame memory submit, so record-callback uploads land this frame
	// and ECS reads stay inside the window.
	g_xEngine.FluxBackend().RecordFrame(xWorkDistribution);
	m_bHasRenderWork = true;

	// The backend has finished recording (command buffers retain the recorded
	// commands until EndFrame submits them); the queue entries are no longer
	// needed and the next frame's passes will be queued fresh.
	ClearPendingRenderPasses();
}

void Flux_RendererImpl::ClearPendingRenderPasses()
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(),
		"ClearPendingRenderPasses: main-thread only");
	m_xPendingRenderPasses.Clear();
}

Flux_RenderGraph& Flux_RendererImpl::GetRenderGraph()    { return *m_pxRenderGraph; }
bool              Flux_RendererImpl::IsRenderGraphValid(){ return m_pxRenderGraph != nullptr; }

void Flux_RendererImpl::RequestGraphRebuild() { m_bGraphRebuildRequested = true; }

// Phase 2: rebuild the uncullled scene snapshot for this frame via the EC fill seam and
// stamp the render-mutation epoch (passed in by the caller — keeps this off g_xEngine).
// Called once per frame from Zenith_Core.cpp before SetRenderTasksActive(true), so it
// runs on the main thread and every render-pass Prepare that derives from it sees the
// freshly built list.
Flux_RendererImpl::~Flux_RendererImpl()
{
	// Backstop free for the by-ptr snapshot (Shutdown's early free nulls it first when it
	// runs; headless never calls Shutdown, so this is the only free there). delete-null safe.
	delete m_pxSceneSnapshot;
	m_pxSceneSnapshot = nullptr;
}

void Flux_RendererImpl::RebuildSceneSnapshot(uint64_t uRenderMutationEpoch, const Zenith_Maths::Matrix4& xCameraViewProj, bool bCameraValid)
{
	m_pxSceneSnapshot->Rebuild(g_pfnZenithSceneSnapshotFill, uRenderMutationEpoch);
	// Phase 3: stamp the frame's camera frustum so the geometry consumers camera-cull
	// against it without reaching FluxGraphics themselves — but ONLY when the camera
	// actually resolved this frame. Rebuild cleared the frustum-valid flag, so an invalid
	// camera frame leaves it unset and the consumers cull nothing (vs culling every off-
	// origin object against an identity/stale view-proj).
	if (bCameraValid)
	{
		m_pxSceneSnapshot->SetCameraFrustum(xCameraViewProj);
	}
}

// Build the unified GPU-driven mesh scene — the (mesh,cull,material,VAT) bucket topology +
// the GPU-scene object/draw-item record arrays — from the just-rebuilt snapshot. Single
// main-thread writer (the WS7 keystone shape), called from Zenith_Core right after
// RebuildSceneSnapshot, before the render-task window opens. Consumed by the Flux_UnifiedMesh
// feature's GatherUnifiedPacket (uploads + reset/cull/draw). The mesh-geometry registry's
// real provider (wired in LateInitialise) builds one shared VB/IB per unique mesh identity.
// Stage 5: build (or fetch the cache of) a distinct skinned mesh's bind-pose data — the 104-byte
// interleaved vertices as raw words (the compute-skinning input) + a Flux_MeshInstance for the
// shared index buffer / counts the skinned draw binds. Replicates the interleave done by
// Flux_MeshInstance::CreateSkinnedFromAsset so the words byte-match the 104B skinned vertex layout
// (Flux_SkinInputVertex). Cached per Zenith_MeshAsset*; entries freed in Shutdown.
Flux_RendererImpl::Flux_SkinnedPoseEntry* Flux_RendererImpl::GetOrBuildSkinnedPose(Zenith_MeshAsset* pxAsset)
{
	if (pxAsset == nullptr)
	{
		return nullptr;
	}
	if (const u_int* puIdx = m_xUnifiedSkinnedPoseByAsset.TryGet(pxAsset))
	{
		return m_axUnifiedSkinnedPoseStore.Get(*puIdx);
	}

	const u_int uNumVerts = pxAsset->GetNumVerts();
	if (uNumVerts == 0u || !pxAsset->HasSkinning())
	{
		return nullptr;
	}

	Flux_MeshInstance* pxMesh = Flux_MeshInstance::CreateSkinnedFromAsset(pxAsset);
	if (pxMesh == nullptr)
	{
		return nullptr;
	}

	Flux_SkinnedPoseEntry* pxEntry = new Flux_SkinnedPoseEntry();
	pxEntry->m_pxMesh    = pxMesh;
	pxEntry->m_uNumVerts = uNumVerts;

	// Interleave the 104B skinned vertex as 26 raw words/vertex (mirror Flux_MeshInstance.cpp's
	// CreateSkinnedFromAsset packing: pos uv normal tangent bitangent color boneIDs boneWeights).
	const bool bHasPos = pxAsset->m_xPositions.GetSize()  >= uNumVerts;
	const bool bHasUV  = pxAsset->m_xUVs.GetSize()        >= uNumVerts;
	const bool bHasNrm = pxAsset->m_xNormals.GetSize()    >= uNumVerts;
	const bool bHasTan = pxAsset->m_xTangents.GetSize()   >= uNumVerts;
	const bool bHasBit = pxAsset->m_xBitangents.GetSize() >= uNumVerts;
	const bool bHasCol = pxAsset->m_xColors.GetSize()     >= uNumVerts;
	const bool bHasBI  = pxAsset->m_xBoneIndices.GetSize()>= uNumVerts;
	const bool bHasBW  = pxAsset->m_xBoneWeights.GetSize()>= uNumVerts;

	pxEntry->m_auBindPoseWords.Reserve((size_t)uNumVerts * uFLUX_SKIN_INPUT_WORDS);
	auto PushF = [&](float f) { pxEntry->m_auBindPoseWords.PushBack(Flux_SkinDetail::FloatToWord(f)); };
	auto PushU = [&](u_int u) { pxEntry->m_auBindPoseWords.PushBack(u); };
	for (u_int v = 0; v < uNumVerts; ++v)
	{
		const Zenith_Maths::Vector3 xPos = bHasPos ? pxAsset->m_xPositions.Get(v)  : Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f);
		const Zenith_Maths::Vector2 xUV  = bHasUV  ? pxAsset->m_xUVs.Get(v)        : Zenith_Maths::Vector2(0.0f, 0.0f);
		const Zenith_Maths::Vector3 xNrm = bHasNrm ? pxAsset->m_xNormals.Get(v)    : Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
		const Zenith_Maths::Vector3 xTan = bHasTan ? pxAsset->m_xTangents.Get(v)   : Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);
		const Zenith_Maths::Vector3 xBit = bHasBit ? pxAsset->m_xBitangents.Get(v) : Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
		const Zenith_Maths::Vector4 xCol = bHasCol ? pxAsset->m_xColors.Get(v)     : Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
		const glm::uvec4 xBI = bHasBI ? pxAsset->m_xBoneIndices.Get(v) : glm::uvec4(0u, 0u, 0u, 0u);
		const glm::vec4  xBW = bHasBW ? pxAsset->m_xBoneWeights.Get(v) : glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
		PushF(xPos.x); PushF(xPos.y); PushF(xPos.z);
		PushF(xUV.x);  PushF(xUV.y);
		PushF(xNrm.x); PushF(xNrm.y); PushF(xNrm.z);
		PushF(xTan.x); PushF(xTan.y); PushF(xTan.z);
		PushF(xBit.x); PushF(xBit.y); PushF(xBit.z);
		PushF(xCol.x); PushF(xCol.y); PushF(xCol.z); PushF(xCol.w);
		PushU(xBI.x);  PushU(xBI.y);  PushU(xBI.z);  PushU(xBI.w);
		PushF(xBW.x);  PushF(xBW.y);  PushF(xBW.z);  PushF(xBW.w);
	}

	const u_int uIdx = m_axUnifiedSkinnedPoseStore.GetSize();
	m_axUnifiedSkinnedPoseStore.PushBack(pxEntry);
	m_xUnifiedSkinnedPoseByAsset.Insert(pxAsset, uIdx);
	return pxEntry;
}

void Flux_RendererImpl::SyncUnifiedBucketsFromSnapshot()
{
	const Flux_RenderSceneSnapshot& xSnapshot = *m_pxSceneSnapshot;
	Zenith_MaterialAsset* pxBlankMaterial = g_xEngine.FluxGraphics().m_xBlankMaterial.GetDirect();

	// ONE sync covers both sources: snapshot statics (Zenith_ModelComponent meshes) AND
	// instance-group foliage (trees). The mesh-geometry residency refcount + the GPU-scene
	// record build + the bucket-topology refcount diff all run over this single main-thread
	// walk (the proven WS7 single-writer shape).
	m_xUnifiedMeshGeometryRegistry.BeginSync();
	Flux_BeginGPUSceneBuild(m_xUnifiedGPUScene, m_xUnifiedBucketRegistry);

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
			Flux_MeshInstance* pxMesh = pxModel->GetMeshInstance(uMesh);
			if (pxMesh == nullptr)
			{
				continue;
			}

			Zenith_MaterialAsset* pxMat = pxModel->GetMaterial(uMesh);
			if (pxMat == nullptr)
			{
				pxMat = pxBlankMaterial;   // stable blank-material identity (never a null key)
			}

			// Translucent / additive submeshes belong on the forward Translucency path.
			const MaterialBlendMode eBlend = pxMat->GetResolved().m_xParams.m_eBlendMode;
			if (eBlend == MATERIAL_BLEND_TRANSLUCENT || eBlend == MATERIAL_BLEND_ADDITIVE)
			{
				continue;
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
				continue;   // no shareable mesh identity
			}
			const u_int uMeshGeometryId = m_xUnifiedMeshGeometryRegistry.Reference(xMeshKey);
			if (uMeshGeometryId == uFLUX_INVALID_MESH_GEOMETRY_ID)
			{
				continue;   // build failed (only the real provider can fail)
			}

			const bool bTwoSided = pxMat->GetResolved().m_xParams.m_bTwoSided;
			const Zenith_AABB& xLocal = pxMesh->GetLocalBounds();

			Flux_GPUSceneSourceSubmesh xSub;
			xSub.m_uMeshGeometryId    = uMeshGeometryId;
			xSub.m_uCullMode          = bTwoSided ? uFLUX_GPUSCENE_CULL_TWO_SIDED : uFLUX_GPUSCENE_CULL_ONE_SIDED;
			xSub.m_ulMaterialAssetId  = reinterpret_cast<u_int64>(pxMat);   // in-session grouping identity
			xSub.m_ulVATTextureId     = 0u;   // static meshes carry no VAT
			xSub.m_xLocalBoundsSphere = Flux_LocalBoundsSphereFromAABB(xLocal.m_xMin, xLocal.m_xMax);
			xSub.m_uColorTintPacked   = uFLUX_GPUSCENE_TINT_WHITE;
			xSub.m_uFlags             = 0u;
			xItem.m_xSubmeshes.PushBack(xSub);
		}

		if (xItem.m_xSubmeshes.GetSize() > 0u)
		{
			Flux_AppendGPUSceneItem(xItem, m_xUnifiedBucketRegistry, m_xUnifiedGPUScene);
		}
	}

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
		}
	}

	// ---- animated skinned models (Stage 5) ----
	// Reset the per-frame skin builders ALWAYS (so GatherUnifiedPacket sees 0 jobs when the
	// toggle is off / there is no animated content). When enabled, walk the snapshot's
	// animated-skinned items: each skinned submesh-instance is compute-skinned to the arena and
	// drawn through the unified kernels via its own per-instance skinned bucket.
	m_xUnifiedBonePalette.Begin(Flux_SkeletonInstance::MAX_BONES);
	m_auUnifiedBindPosePoolWords.Clear();
	m_xUnifiedBindPosePoolBaseByMesh.Clear();
	m_axUnifiedSkinJobs.Clear();
	m_axUnifiedSkinnedDraws.Clear();
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
				for (u_int b = 0; b < uNumBones && b < Flux_SkeletonInstance::MAX_BONES; ++b)
				{
					m_xUnifiedBonePalette.Matrices().Get(uBonePaletteBase + b) = pxMats[b];
				}
			}

			const uint32_t uNumMeshes = pxModel->GetNumMeshes();
			for (uint32_t uMesh = 0; uMesh < uNumMeshes; ++uMesh)
			{
				if (pxModel->GetSkinnedMeshInstance(uMesh) == nullptr)
				{
					// A non-skinned submesh of an animated-skinned model: NOT drawn by the unified
					// path. The static walk above skips the whole m_bAnimatedSkinned item, and this
					// walk only skins+draws the skinned submeshes — so a mixed model's static
					// submeshes are currently dropped. This is a known gap for hypothetical mixed
					// models (TODO: route such submeshes to the static GPU-scene with the model's
					// world matrix if a real case appears).
					continue;
				}
				Flux_MeshInstance* pxMeshInst = pxModel->GetMeshInstance(uMesh);
				if (pxMeshInst == nullptr)
				{
					continue;
				}
				Flux_SkinnedPoseEntry* pxPose = GetOrBuildSkinnedPose(pxMeshInst->GetSourceAsset());
				if (pxPose == nullptr || pxPose->m_uNumVerts == 0u)
				{
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

				// Bind-pose pool: append this mesh's words once per frame; reuse the base afterwards.
				u_int uBindPoseBase;
				if (const u_int* puBase = m_xUnifiedBindPosePoolBaseByMesh.TryGet(pxMeshInst->GetSourceAsset()))
				{
					uBindPoseBase = *puBase;
				}
				else
				{
					uBindPoseBase = m_auUnifiedBindPosePoolWords.GetSize() / uFLUX_SKIN_INPUT_WORDS;
					for (u_int w = 0; w < pxPose->m_auBindPoseWords.GetSize(); ++w)
					{
						m_auUnifiedBindPosePoolWords.PushBack(pxPose->m_auBindPoseWords.Get(w));
					}
					m_xUnifiedBindPosePoolBaseByMesh.Insert(pxMeshInst->GetSourceAsset(), uBindPoseBase);
				}

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

				const u_int uSkinIdx = m_axUnifiedSkinnedDraws.GetSize();
				Flux_UnifiedSkinnedDraw xSD;
				xSD.m_pxMesh        = pxPose->m_pxMesh;
				xSD.m_uVertexOffset = uOutVertBase;
				m_axUnifiedSkinnedDraws.PushBack(xSD);

				Flux_GPUSceneBucketKey xKey;
				xKey.m_uMeshGeometryId   = uFLUX_GPUSCENE_SKINNED_MESH_BIT | uSkinIdx;
				xKey.m_uCullMode         = pxMat->GetResolved().m_xParams.m_bTwoSided ? uFLUX_GPUSCENE_CULL_TWO_SIDED : uFLUX_GPUSCENE_CULL_ONE_SIDED;
				xKey.m_ulMaterialAssetId = reinterpret_cast<u_int64>(pxMat);
				xKey.m_ulVATTextureId    = 0u;   // skinned != VAT (keeps ResolveBucketVAT safe)

				const Zenith_AABB& xLocal = pxMeshInst->GetLocalBounds();
				const Zenith_Maths::Vector4 xSphere = Flux_InflateBoundsSphere(
					Flux_LocalBoundsSphereFromAABB(xLocal.m_xMin, xLocal.m_xMax), fFLUX_SKIN_BOUNDS_INFLATION);

				Flux_AppendGPUSceneSkinnedInstance(m_xUnifiedBucketRegistry, m_xUnifiedGPUScene,
					xSrc.m_xWorldMatrix, uBonePaletteBase, xKey, xSphere, uFLUX_GPUSCENE_TINT_WHITE);
			}
		}
		m_uUnifiedSkinTotalOutVerts = uArenaCursor;
	}

	Flux_EndGPUSceneBuild(m_xUnifiedGPUScene, m_xUnifiedBucketRegistry);
	m_xUnifiedMeshGeometryRegistry.EndSync();

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

bool Flux_RendererImpl::ConsumeGraphRebuildRequest()
{
	bool b = m_bGraphRebuildRequested;
	m_bGraphRebuildRequested = false;
	return b;
}

Zenith_Vector<Flux_RenderPassEntry>& Flux_RendererImpl::GetPendingRenderPasses()
{
	return m_xPendingRenderPasses;
}

// Debug-variable backing store for the transient-aliasing runtime toggle.
// Synced into the render graph at each SetupRenderGraph via SetAliasingEnabled;
// changes trigger MarkDirty so the next Compile rebuilds the pool layout.
DEBUGVAR bool dbg_bTransientAliasing = true;

// (Flux_FinalLayoutTransitionNoOp moved to Flux_FeatureRegistry.cpp, where the
// final-RT layout-transition setup step that uses it now lives.)

void Flux_PipelineHelper::BuildFullscreenPipeline(
	Flux_Shader& xShader,
	Flux_Pipeline& xPipeline,
	const Flux_ShaderDecl& xDecl,
	TextureFormat eColourFormat,
	TextureFormat eDepthStencilFormat)
{
	Flux_PipelineSpecification xSpec = CreateFullscreenSpec(xShader, xDecl, eColourFormat, eDepthStencilFormat);
	Flux_PipelineBuilder::FromSpecification(xPipeline, xSpec);
}

Flux_PipelineSpecification Flux_PipelineHelper::CreateFullscreenSpec(
	Flux_Shader& xShader,
	const Flux_ShaderDecl& xDecl,
	TextureFormat eColourFormat,
	TextureFormat eDepthStencilFormat)
{
	// Single-RTV is the N==1 case. Taking the address of the by-value param
	// yields a valid 1-element array for the duration of this call.
	return CreateFullscreenSpecMRT(xShader, xDecl, &eColourFormat, 1u, eDepthStencilFormat);
}

Flux_PipelineSpecification Flux_PipelineHelper::CreateFullscreenSpecMRT(
	Flux_Shader& xShader,
	const Flux_ShaderDecl& xDecl,
	const TextureFormat* aeColourFormats,
	u_int uNumColourAttachments,
	TextureFormat eDepthStencilFormat)
{
	Zenith_Assert(uNumColourAttachments >= 1 && uNumColourAttachments <= FLUX_MAX_TARGETS,
		"CreateFullscreenSpecMRT: uNumColourAttachments %u out of range [1, %u]", uNumColourAttachments, FLUX_MAX_TARGETS);

	xShader.Initialise(xDecl);

	Flux_PipelineSpecification xSpec;
	for (u_int u = 0; u < uNumColourAttachments; u++)
	{
		xSpec.m_aeColourAttachmentFormats[u] = aeColourFormats[u];
	}
	xSpec.m_uNumColourAttachments = uNumColourAttachments;
	xSpec.m_eDepthStencilFormat = eDepthStencilFormat;
	xSpec.m_pxShader = &xShader;
	xSpec.m_xVertexInputDesc.m_eTopology = MESH_TOPOLOGY_NONE;
	xSpec.m_bDepthTestEnabled = false;
	xSpec.m_bDepthWriteEnabled = false;

	// Fullscreen passes are conceptually overwrite operations — the fragment
	// shader is expected to write every pixel. Default Flux_BlendState has
	// alpha blending enabled, which for passes like SSR RayMarch would blend
	// the new output into stale last-frame contents (alpha < 1 preserves old
	// pixels, producing ghosting). Callers that actually want a blend mode
	// (e.g. forward translucency) override this explicitly.
	for (Flux_BlendState& xBlendState : xSpec.m_axBlendStates)
	{
		xBlendState.m_bBlendEnabled = false;
		xBlendState.m_eSrcBlendFactor = BLEND_FACTOR_ONE;
		xBlendState.m_eDstBlendFactor = BLEND_FACTOR_ZERO;
	}

	xShader.GetReflection().PopulateLayout(xSpec.m_xPipelineLayout);

	return xSpec;
}

void Flux_RendererImpl::EarlyInitialise()
{
	// Flux_PerFrame must initialise BEFORE backend Initialise so backends can
	// register their begin/end-frame callbacks during their own setup. The
	// counter starts at 0 and the ring index is correctly defined from the
	// first call.
	PerFrameInitialise();

	g_xEngine.FluxBackend().Initialise();
	g_xEngine.FluxMemory().Initialise();
	g_xEngine.FluxBackend().InitialisePerFrameResources(); // Must be after memory manager init
	g_xEngine.FluxGraphics().InitialiseSamplers(); // Must be before any CreateShaderResourceView calls (bindless registration)
}

void Flux_RendererImpl::LateInitialise()
{
	// Subsystem dependency graph (A -> B means A must init before B):
	//
	// MemoryManager -> SlangCompiler -> Swapchain -> Graphics
	// Graphics -> HDR -> DeferredShading
	// Graphics -> Shadows, Skybox, UnifiedMesh, InstancedMeshes, Primitives
	// Skybox -> IBL (environment probes use skybox cubemap)
	// Terrain -> Grass (vegetation placed on terrain)
	// HiZ -> SSR, SSGI (screen-space effects use Hi-Z pyramid)
	// SSR, SSGI -> DeferredShading (composites SSR/SSGI results)
	// DeferredShading -> DynamicLights (lights applied after deferred setup)
	// [Tools] PlatformAPI -> ImGui -> Gizmos, ShaderHotReload
	//
	// Independent (no ordering constraint): SSAO, Fog, SDFs, Particles, Quads, Text

#if defined(ZENITH_WINDOWS) && defined(ZENITH_VULKAN)
	// Initialize Slang compiler before any shader loading. Slang is the Vulkan
	// SPIR-V toolchain; the D3D12 null backend loads pre-baked reflection only.
	Flux_SlangCompiler::Initialise();
	// Tell the modern session API where to resolve `loadModule` paths from.
	// Required for FluxShaderProgram-based runtime compilation; the legacy
	// per-file paths embed their own search root via spAddSearchPath.
	Flux_SlangCompiler::AddSearchPath(SHADER_SOURCE_ROOT);
#endif

	g_xEngine.FluxSwapchain().Initialise();

#ifdef ZENITH_TOOLS
	// Bring up the hot-reload watcher BEFORE any subsystem Initialise() so
	// the RegisterSubsystem calls inside those Initialise bodies see a live
	// watcher. (The static registration list is also safe to append to
	// before Initialise, but starting the file watcher early means the
	// `firing N rebuild callback(s)` log line picks up edits made even
	// during engine boot.)
	Flux_ShaderHotReload::Initialise();
#endif

#ifdef ZENITH_TOOLS
	// ImGui depends only on the Vulkan device + swapchain format (see
	// Zenith_Vulkan::InitialiseImGuiRenderPass), not on any registry feature
	// (FluxGraphics included), so bringing it up before the feature walk is
	// dependency-safe. Gizmos (which DOES depend on ImGui) is registered as a
	// feature and initialised by the walk.
	g_xEngine.FluxBackend().InitialiseImGui();
#endif

	// The per-subsystem Initialise() ladder walks the Flux_FeatureRegistry in
	// registration order. FluxGraphics is registered FIRST, so the walk brings it
	// up before every dependent feature — it used to be a separate inline init,
	// but it is now an ordinary RegisterFeature<> entry like everything else. The
	// dependency rationale documented at the top of this function is encoded as
	// the registration order in Flux_FeatureRegistry.cpp::RegisterDefaultFeatures().
	Flux_FeatureRegistry::RegisterDefaultFeatures();

	// Catalog<->feature parity. Backend-independent; runs in ALL configs right
	// after registration so a forgotten catalog include or RegisterFeature line
	// fails loudly at boot instead of as a silent stale/missing-shader bug.
	{
		std::string strParityErr;
		const bool bParity = Flux_ShaderCatalog::ValidateFeatureParity(Flux_FeatureRegistry::Get(), strParityErr);
		Zenith_Assert(bParity, "Flux shader catalog/feature parity failed: %s", strParityErr.c_str());
	}

	const Flux_FeatureRegistry& xRegistry = Flux_FeatureRegistry::Get();
	for (u_int uFeature = 0; uFeature < xRegistry.GetNumFeatures(); uFeature++)
	{
		const Flux_FeatureDesc& xDesc = xRegistry.GetFeatures()[uFeature];
		if (xDesc.m_pfnInitialise != nullptr)
			xDesc.m_pfnInitialise();
	}

#ifdef ZENITH_TOOLS
	// Hot-reload: derive every engine shader program's rebuild callback from the
	// feature registry — no per-subsystem RegisterSubsystem needed. Runs after the
	// feature table is populated (BuildPipelines trampolines exist) and after the
	// watcher came up above. Game-side / non-feature owners register afterwards.
	Flux_ShaderHotReload::AutoRegisterFeatures();
#endif

	// Drain the GPU uploads staged by swapchain init + the feature walk above.
	g_xEngine.FluxMemory().Flush();

	// Create and compile the render graph
	m_pxRenderGraph = new Flux_RenderGraph();

	// Give the mesh-geometry registry its REAL provider so the first reference to a unique
	// mesh builds ONE shared VB/IB (N identical meshes -> 1 bucket -> 1 indirect draw). This
	// is a FUNCTIONAL requirement of the unified path (not debug tooling): it MUST be set in
	// every config — without it the registry stays in id-only mode (m_pvBuilt == null), so
	// GetUnifiedMeshGeometry returns null and the unified path builds no geometry and draws
	// nothing. (Previously this lived inside the ZENITH_DEBUG_VARIABLES block below next to
	// the toggle, so non-debug-variable builds silently got an inert unified path.)
	m_xUnifiedMeshGeometryRegistry.SetProvider(Flux_MakeRealMeshGeometryProvider());

#ifdef ZENITH_DEBUG_VARIABLES
	// Debug-variable tree-path convention: most renderer variables live under
	// "Render/...", but a handful of subsystems (HDR and HiZ) established a
	// "Flux/<Subsystem>/..." convention before the "Render/" top-level
	// consolidated. New HDR / HiZ variables follow their subsystem's existing
	// convention; generic renderer-level variables go under "Render/". A
	// future cleanup pass may migrate HDR / HiZ under "Render/HDR" and
	// "Render/HiZ" but is deliberately NOT done here (would require an editor
	// config migration for users who saved expanded-tree state).

	// Debug toggle for transient memory aliasing. The graph reads this value
	// at each SetupRenderGraph invocation and calls SetAliasingEnabled; a
	// change triggers MarkDirty so the next Compile rebuilds the pool layout.
	g_xEngine.DebugVariables().AddBoolean({ "Render", "RenderGraph", "Transient Aliasing" }, dbg_bTransientAliasing);

	// Click-to-log button: prints the compiled render-graph pass order. Useful
	// for newcomers asking "what runs when?" — the answer is the topological
	// sort of the pass DAG, not the order of AddPass() calls in SetupRenderGraph.
	g_xEngine.DebugVariables().AddButton({ "Render", "RenderGraph", "Print Pass Order" }, []() {
		if (g_xEngine.FluxRenderer().m_pxRenderGraph != nullptr)
		{
			const std::string strOrder = g_xEngine.FluxRenderer().m_pxRenderGraph->GetPassOrderDescription();
			Zenith_Log(LOG_CATEGORY_RENDERER, "Render-graph pass order: %s", strOrder.c_str());
		}
		else
		{
			Zenith_Warning(LOG_CATEGORY_RENDERER, "Render graph not initialised yet — try again after the first frame.");
		}
	});

	// MRT / depth / HDR / bloom transient previews. Registered once here (not in
	// SetupTransients, which re-runs on resize) and resolve the current SRV
	// via callback every ImGui draw — rebuilds that invalidate the underlying
	// TransientResource don't leave a dangling pointer in the tree.
	g_xEngine.DebugVariables().AddTextureCallback({ "Render", "Debug", "MRT Diffuse" },       [](){ return g_xEngine.FluxGraphics().GetDebugSRV_MRTDiffuse(); });
	g_xEngine.DebugVariables().AddTextureCallback({ "Render", "Debug", "MRT NormalsAO" },     [](){ return g_xEngine.FluxGraphics().GetDebugSRV_MRTNormalsAO(); });
	g_xEngine.DebugVariables().AddTextureCallback({ "Render", "Debug", "MRT Material" },      [](){ return g_xEngine.FluxGraphics().GetDebugSRV_MRTMaterial(); });
	g_xEngine.DebugVariables().AddTextureCallback({ "Render", "Debug", "Depth" },             [](){ return g_xEngine.FluxGraphics().GetDebugSRV_Depth(); });
	// HDR textures follow Flux_HDR.cpp's established "Flux/HDR/..." convention.
	g_xEngine.DebugVariables().AddTextureCallback({ "Flux",   "HDR",   "Textures", "HDRScene"  }, [](){ return g_xEngine.FluxGraphics().GetDebugSRV_HDRScene(); });
	g_xEngine.DebugVariables().AddTextureCallback({ "Flux",   "HDR",   "Textures", "BloomMip0" }, [](){ return g_xEngine.HDR().GetDebugSRV_Bloom0(); });
	g_xEngine.DebugVariables().AddTextureCallback({ "Flux",   "HDR",   "Textures", "BloomMip1" }, [](){ return g_xEngine.HDR().GetDebugSRV_Bloom1(); });
	g_xEngine.DebugVariables().AddTextureCallback({ "Flux",   "HDR",   "Textures", "BloomMip2" }, [](){ return g_xEngine.HDR().GetDebugSRV_Bloom2(); });
#endif

	// Initialise any game render features registered BEFORE Flux came up (their
	// Initialise was deferred since the graph wasn't valid yet). Runs after the
	// graph exists and before the first SetupRenderGraph walk so their setup is
	// picked up immediately. No-op for the common late-registration case (games
	// that register during Project init, after this point — those initialise on
	// Register and request a rebuild).
	Zenith_GameRenderFeatures::InitialiseAllPending();

	SetupRenderGraph();
	// Free-function wrapper for AddResChangeCallback — pfnCallback is a
	// `void(*)()` callable, which a member function pointer can't satisfy
	// directly without an instance bound. The instance is the singleton on
	// g_xEngine, so a trivial wrapper routes through.
	AddResChangeCallback(+[](){ g_xEngine.FluxRenderer().SetupRenderGraph(); });
}

void Flux_RendererImpl::ApplySubsystemGraphSelections(Flux_RenderGraph& xGraph)
{
	// Order matters here. Both subsystems below run BEFORE Compile() so any
	// SetPassEnabled / MarkDirty mutations they perform take effect on the
	// same frame. Neither can live as a pass OnPrepare callback because
	// Phase 0 only fires OnPrepare for *enabled* passes — once a system has
	// disabled its previously-active pass, an OnPrepare-based switcher would
	// never run again.
	//
	// Fog must run BEFORE IBL so that if the user changes fog technique this
	// frame, ApplyTechniqueSelectionToGraph calls MarkDirty() *before* IBL's
	// UpdateGraphPassEnables checks IsDirty() — which lets IBL force-enable
	// all 49 of its passes for the upcoming full Compile() so the validator
	// sees a writer for every IBL texture that DeferredShading reads.
	g_xEngine.Fog().ApplyTechniqueSelectionToGraph(xGraph);
	// SSR / SSGI runtime output toggles: when blur or denoise flip, these
	// enable/disable their post-pass and MarkDirty so the deferred-lighting
	// pass re-reads the correct handle (see g_xEngine.SSR().GetReflectionHandle).
	// Must run BEFORE IBL's UpdateGraphPassEnables for the same MarkDirty
	// propagation reason described above.
	g_xEngine.SSR().ApplyBlurSelectionToGraph(xGraph);
	g_xEngine.SSGI().ApplyDenoiseSelectionToGraph(xGraph);
	// Skybox transmittance/sky-view LUT enables. Must run BEFORE IBL's (and after
	// any graph-dirtying system above) so a dirty compile force-enables the LUT
	// writers the "Skybox" pass reads — same MarkDirty-propagation reason as IBL.
	g_xEngine.Skybox().UpdateGraphPassEnables(xGraph);
	g_xEngine.IBL().UpdateGraphPassEnables(xGraph);
}

void Flux_RendererImpl::SyncRenderGraphDebugToggles()
{
	if (m_pxRenderGraph == nullptr)
		return;
#ifdef ZENITH_DEBUG_VARIABLES
	// Transient aliasing toggle — SetAliasingEnabled is a no-op when the
	// value is unchanged and MarkDirty's on change, so calling this every
	// frame is cheap and propagates editor flips on the next Compile.
	// Guard uses ZENITH_DEBUG_VARIABLES (not ZENITH_TOOLS) to match the
	// declaration guard on dbg_bTransientAliasing and the AddBoolean /
	// AddTextureCallback registrations above — the macros imply one another
	// (enforced in Zenith.h) so the choice is purely for reader clarity:
	// anything touching a debug variable guards on ZENITH_DEBUG_VARIABLES.
	m_pxRenderGraph->SetAliasingEnabled(dbg_bTransientAliasing);
#endif
}

void Flux_RendererImpl::SetupRenderGraph()
{
	Zenith_Assert(g_xEngine.Threading().IsMainThread(),
		"SetupRenderGraph: must run on the main thread; the pending render-pass queue is accessed without locking here.");

	// Clear the queued render passes first — they hold pointers to the graph's
	// passes which will be destroyed by Clear(). Caller must have already drained the GPU.
	ClearPendingRenderPasses();
	Flux_RendererImpl& xRenderer = g_xEngine.FluxRenderer();
	xRenderer.m_pxRenderGraph->Clear();

	// Sync the transient-aliasing debug toggle into the graph. SetAliasingEnabled
	// is a no-op if the value is unchanged; on change it calls MarkDirty so the
	// next Compile rebuilds the pool layout.
	SyncRenderGraphDebugToggles();

	// Single ordered setup walk — there are NO discrete render phases. The render
	// graph computes pass execution order by topologically sorting each pass's
	// declared Reads/Writes; the walk's declaration order seeds that sort where it
	// matters — producers must precede consumers (a reader links only to an
	// earlier-declared writer of the same resource) and same-resource writers run
	// in declaration order (see the ORDERING note in Flux_FeatureRegistry.h). The
	// walk (built in Flux_FeatureRegistry::RegisterDefaultFeatures) folds the former
	// inline irregulars — FluxGraphics/HDR transient creation, the post-fog game
	// hook, and the final-RT layout-transition
	// pass — in as ordinary ordered steps at their exact prior positions, so the
	// compiled order is unchanged.
	Flux_FeatureRegistry::Get().RunSetup(*xRenderer.m_pxRenderGraph);

	// Clear() already left the graph dirty — no explicit MarkDirty() needed.
}

void Flux_RendererImpl::ReleaseAssetReferences()
{
	// Drop refs to Flux-side assets so Zenith_AssetRegistry::Shutdown can delete them
	// cleanly. Each subsystem releases the handles it owns.
	g_xEngine.FluxGraphics().ReleaseAssetReferences();
	g_xEngine.Text().ReleaseAssetReferences();
	g_xEngine.Particles().ReleaseAssetReferences();
	g_xEngine.Terrain().ReleaseAssetReferences();
	g_xEngine.Skybox().ReleaseAssetReferences();
	g_xEngine.VolumeFog().ReleaseAssetReferences();

	// Material defaults live in AssetHandling but are part of the same pre-registry
	// release window — this is the natural place to drop them.
	Zenith_MaterialAsset::ReleaseDefaults();
}

void Flux_RendererImpl::Shutdown()
{
	Flux_RendererImpl& xRenderer = g_xEngine.FluxRenderer();
	delete xRenderer.m_pxRenderGraph;
	xRenderer.m_pxRenderGraph = nullptr;

	// Phase 2: free the snapshot (drops its non-owning Flux_ModelInstance* entries) so no
	// stale pointer survives into a fresh renderer on engine reinit.
	delete xRenderer.m_pxSceneSnapshot;
	xRenderer.m_pxSceneSnapshot = nullptr;

	// Stage 5: free the skinned-pose store (each entry owns a Flux_MeshInstance — destroy its
	// GPU buffers while the Vulkan device is still up, then delete the entry).
	for (u_int u = 0; u < xRenderer.m_axUnifiedSkinnedPoseStore.GetSize(); ++u)
	{
		Flux_SkinnedPoseEntry* pxEntry = xRenderer.m_axUnifiedSkinnedPoseStore.Get(u);
		if (pxEntry != nullptr)
		{
			if (pxEntry->m_pxMesh != nullptr)
			{
				pxEntry->m_pxMesh->Destroy();
				delete pxEntry->m_pxMesh;
			}
			delete pxEntry;
		}
	}
	xRenderer.m_axUnifiedSkinnedPoseStore.Clear();
	xRenderer.m_xUnifiedSkinnedPoseByAsset.Clear();

	// Shut down any still-registered game render features (reverse registration
	// order) AFTER the graph is gone, so a feature's Shutdown can't touch a live
	// graph — but while the Vulkan device / FluxBackend are still up, so
	// device-touching resource deletes in a feature Shutdown are safe (matches
	// where the engine features shut down below). Games that explicitly Unregister
	// in their own teardown already ran their Shutdown; this is the backstop.
	Zenith_GameRenderFeatures::ShutdownAll();

	// Clear res-change callbacks so OnResChange has nothing to invoke — the
	// callbacks would otherwise deref the now-null m_pxRenderGraph and crash.
	xRenderer.m_xResChangeCallbacks.Clear();

	// Shut down every Flux feature in REVERSE registration order. RunShutdown now
	// covers ALL features — including FluxGraphics, HDR, Gizmos and MaterialPreview,
	// which used to be torn down inline here — because no feature's Shutdown reads
	// another feature (only foundation + own state, verified), so reverse-of-init is
	// a correct teardown order with no hand-tuning. Features with no Shutdown (Fog —
	// RAII / stateless) are skipped. Only the NON-feature teardown (Slang /
	// HotReload / ImGui / swapchain / memory) stays inline below; it runs after the
	// feature walk while the Vulkan device + memory manager are still up.
	Flux_FeatureRegistry::Get().RunShutdown();

#if defined(ZENITH_WINDOWS) && defined(ZENITH_VULKAN)
	Flux_SlangCompiler::Shutdown();
#endif

#ifdef ZENITH_TOOLS
	Flux_ShaderHotReload::Shutdown();
	g_xEngine.FluxBackend().ShutdownImGui();
#endif

	// Shutdown swapchain-owned file-static shader/pipeline state before the
	// Vulkan device and memory-manager registries go away.
	g_xEngine.FluxSwapchain().Shutdown();

	// Shutdown memory manager (VMA allocator, handle registries)
	g_xEngine.FluxMemory().Shutdown();

	// Clear PerFrame callback arrays so a subsequent Flux_RendererImpl::Initialise starts
	// from a known empty state (matters for unit tests that re-init Flux
	// within one process). Frame counter is intentionally left alone.
	PerFrameShutdown();

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux shut down");
}

void Flux_RendererImpl::OnResChange()
{
	Flux_RendererImpl& xRenderer = g_xEngine.FluxRenderer();
	for (u_int i = 0; i < xRenderer.m_xResChangeCallbacks.GetSize(); i++)
	{
		xRenderer.m_xResChangeCallbacks.Get(i)();
	}
}

bool Flux_RendererImpl::PrepareFrame(Flux_WorkDistribution& xOutDistribution)
{
	static_assert(FLUX_NUM_WORKER_THREADS > 0, "FLUX_NUM_WORKER_THREADS must be positive");

	xOutDistribution.Clear();

	// The render graph queues passes in topological order — no sort needed here.
	Flux_RendererImpl& xRenderer = g_xEngine.FluxRenderer();

	const u_int uTotalPasses = xRenderer.m_xPendingRenderPasses.GetSize();
	xOutDistribution.uTotalPasses = uTotalPasses;
	if (uTotalPasses == 0)
	{
		return false;
	}

	// Distribute the passes across worker threads as contiguous, roughly-equal
	// index ranges. Topological order is preserved within and across workers
	// (worker i records a lower index range than worker i+1, and the worker
	// command buffers are submitted in order) — that ordering + the graph's
	// inline barriers is what enforces dependencies. Per-pass GPU cost is not
	// known here, so pass-count slicing is the cheap, balanced default.
	const u_int uTargetPassesPerThread = (uTotalPasses + FLUX_NUM_WORKER_THREADS - 1) / FLUX_NUM_WORKER_THREADS;
	u_int uCurrentThreadIndex = 0;

	xOutDistribution.auStartIndex[0] = 0;
	for (u_int uIndex = 0; uIndex < uTotalPasses; uIndex++)
	{
		const u_int uCountThisThread = uIndex - xOutDistribution.auStartIndex[uCurrentThreadIndex];
		if (uCountThisThread >= uTargetPassesPerThread &&
			uCurrentThreadIndex < FLUX_NUM_WORKER_THREADS - 1)
		{
			xOutDistribution.auEndIndex[uCurrentThreadIndex] = uIndex;
			uCurrentThreadIndex++;
			xOutDistribution.auStartIndex[uCurrentThreadIndex] = uIndex;
		}
	}

	Zenith_Assert(uCurrentThreadIndex < FLUX_NUM_WORKER_THREADS,
		"PrepareFrame: Thread index %u out of bounds (max %u)", uCurrentThreadIndex, FLUX_NUM_WORKER_THREADS);
	xOutDistribution.auEndIndex[uCurrentThreadIndex] = uTotalPasses;

	// Explicitly write empty (N, N) ranges for any worker that did not receive a
	// slice. The invariant is relied on by the consumer; writing it here
	// defensively protects against xOutDistribution.Clear() behaviour changing.
	for (u_int u = uCurrentThreadIndex + 1; u < FLUX_NUM_WORKER_THREADS; u++)
	{
		xOutDistribution.auStartIndex[u] = uTotalPasses;
		xOutDistribution.auEndIndex[u] = uTotalPasses;
	}

	return true;
}
