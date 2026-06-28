#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Collections/Zenith_Vector.h"

class Flux_InstanceGroup;

// Stage 4: the InstancedMeshes subsystem is now a thin registration FRONT-END.
// The legacy GPU cull -> indirect G-buffer + per-cascade shadow draw machinery
// (compute/graphics pipelines, shaders, per-group cull-output buffers) was retired;
// instanced foliage renders through the unified GPU-driven path (Flux_UnifiedMesh),
// whose SyncUnifiedBucketsFromSnapshot reads each registered group's CPU transform/
// anim SoA. This class owns only the live instance-group registry the sync consumes.
class Flux_InstancedMeshesImpl
{
public:
	Flux_InstancedMeshesImpl() = default;
	~Flux_InstancedMeshesImpl() = default;

	Flux_InstancedMeshesImpl(const Flux_InstancedMeshesImpl&) = delete;
	Flux_InstancedMeshesImpl& operator=(const Flux_InstancedMeshesImpl&) = delete;

	// FluxRenderFeature lifecycle. Initialise/BuildPipelines/Shutdown/SetupRenderGraph
	// are no-ops now (the unified path owns all instanced GPU work); the methods stay so
	// the type still satisfies the FluxRenderFeature concept used by RegisterFeature.
	void Initialise();
	void BuildPipelines();
	void Shutdown();
	void Reset();
	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	// The live instance-group registry the unified GPU-scene sync walks each frame.
	void RegisterInstanceGroup(Flux_InstanceGroup* pxGroup);
	void UnregisterInstanceGroup(Flux_InstanceGroup* pxGroup);
	void ClearAllGroups();

	uint32_t GetGroupCount() const { return static_cast<uint32_t>(m_apxInstanceGroups.GetSize()); }

	Zenith_Vector<Flux_InstanceGroup*> m_apxInstanceGroups;
};
