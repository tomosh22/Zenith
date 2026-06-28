#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "Flux/InstancedMeshes/Flux_InstancedMeshesImpl.h"
#include "Flux/InstancedMeshes/Flux_InstanceGroup.h"
#include "Flux/InstancedMeshes/Flux_AnimationTexture.h"
#include "Flux/Flux_RendererImpl.h"  // RequestGraphRebuild on group register/unregister
#include "Flux/Flux_MaterialBinding.h"
#include "Profiling/Zenith_Profiling.h"

//=============================================================================
// Stage 4: the legacy InstancedMeshes draw machinery (the GPU cull -> indirect
// G-buffer + per-cascade shadow passes, their compute/graphics pipelines and the
// per-group GPU cull-output buffers) was retired. Instanced foliage now renders
// through the unified GPU-driven path (Flux_UnifiedMesh): the renderer's
// SyncUnifiedBucketsFromSnapshot walks the registered instance groups' CPU
// transform/anim SoA (Flux_InstanceGroup::GetTransforms / GetAnimData) and folds
// each enabled instance into the shared (mesh,cull,material,VAT) bucket topology,
// which the one unified core then culls + draws (camera + every shadow cascade).
//
// This subsystem is now a thin registration FRONT-END: it owns the live
// instance-group registry the unified sync consumes each frame, and nothing else.
// (Flux_InstanceGroup still carries its CPU SoA + the now-unused GPU-buffer members;
// those buffers are never populated because the gather that used to fill them is
// gone, so they cost nothing.)
//=============================================================================

void Flux_InstancedMeshesImpl::BuildPipelines()
{
	// No pipelines: the unified GPU-driven path owns the instanced draw/cull/shadow
	// kernels now. Kept as a no-op so the feature still satisfies FluxRenderFeature.
}

void Flux_InstancedMeshesImpl::Initialise()
{
	BuildPipelines();
	Zenith_Log(LOG_CATEGORY_MESH, "Flux_InstancedMeshes initialised (registration front-end; unified path draws)");
}

void Flux_InstancedMeshesImpl::Shutdown()
{
	ClearAllGroups();
	Zenith_Log(LOG_CATEGORY_MESH, "Flux_InstancedMeshes shutdown");
}

void Flux_InstancedMeshesImpl::Reset()
{
	Zenith_Log(LOG_CATEGORY_MESH, "Flux_InstancedMeshesImpl::Reset()");
}

//=============================================================================
// Instance Group Registration — the live registry the unified GPU-scene sync
// (Flux_RendererImpl::SyncUnifiedBucketsFromSnapshot) walks each frame.
//=============================================================================

void Flux_InstancedMeshesImpl::RegisterInstanceGroup(Flux_InstanceGroup* pxGroup)
{
	if (!pxGroup)
	{
		Zenith_Error(LOG_CATEGORY_MESH, "Flux_InstancedMeshesImpl::RegisterInstanceGroup - null group");
		return;
	}

	// Check if already registered
	for (u_int i = 0; i < m_apxInstanceGroups.GetSize(); ++i)
	{
		if (m_apxInstanceGroups.Get(i) == pxGroup)
		{
			return;  // Already registered
		}
	}

	m_apxInstanceGroups.PushBack(pxGroup);
	Zenith_Log(LOG_CATEGORY_MESH, "Flux_InstancedMeshes: Registered instance group (total: %u)", m_apxInstanceGroups.GetSize());

	// A new group adds instances to the unified GPU scene, which changes the bucket
	// topology -> the render graph must rebuild. The unified sync also requests this on
	// a bucket create, but requesting it here too is idempotent and keeps register/
	// unregister self-contained. RequestGraphRebuild defers to the next safe point.
	g_xEngine.FluxRenderer().RequestGraphRebuild();
}

void Flux_InstancedMeshesImpl::UnregisterInstanceGroup(Flux_InstanceGroup* pxGroup)
{
	for (u_int i = 0; i < m_apxInstanceGroups.GetSize(); ++i)
	{
		if (m_apxInstanceGroups.Get(i) == pxGroup)
		{
			// Swap with last and pop
			m_apxInstanceGroups.RemoveSwap(i);
			Zenith_Log(LOG_CATEGORY_MESH, "Flux_InstancedMeshes: Unregistered instance group (remaining: %u)", m_apxInstanceGroups.GetSize());

			// Group set changed -> the unified bucket topology may change (see RegisterInstanceGroup).
			g_xEngine.FluxRenderer().RequestGraphRebuild();
			return;
		}
	}
}

void Flux_InstancedMeshesImpl::ClearAllGroups()
{
	m_apxInstanceGroups.Clear();
	Zenith_Log(LOG_CATEGORY_MESH, "Flux_InstancedMeshes: Cleared all instance groups");
}

void Flux_InstancedMeshesImpl::SetupRenderGraph(Flux_RenderGraph& /*xGraph*/)
{
	// No passes: instanced foliage draws through the unified GPU-driven path
	// (Flux_UnifiedMesh's reset/cull/gbuffer + per-cascade shadow). Kept as a no-op so
	// the feature still satisfies FluxRenderFeature.
}

// Flux material-binding unit tests (pure CPU). Hosted here so the test TU lives
// in the Flux module rather than pulling Flux into an AssetHandling test TU.
#include "Flux/Flux_MaterialBinding.Tests.inl"
