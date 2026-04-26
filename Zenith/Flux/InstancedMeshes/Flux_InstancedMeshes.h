#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

class Flux_DynamicConstantBuffer;
class Flux_InstanceGroup;

//=============================================================================
// Flux_InstancedMeshes
// Renders large numbers of mesh instances using GPU instancing.
// Supports GPU frustum culling and indirect drawing for 100k+ instances.
//=============================================================================
class Flux_InstancedMeshes
{
public:
	static void Initialise();
	static void BuildPipelines();
	static void Shutdown();
	static void Reset();  // Clear state when scene resets

	//-------------------------------------------------------------------------
	// Instance Group Registration
	//-------------------------------------------------------------------------
	static void RegisterInstanceGroup(Flux_InstanceGroup* pxGroup);
	static void UnregisterInstanceGroup(Flux_InstanceGroup* pxGroup);
	static void ClearAllGroups();

	//-------------------------------------------------------------------------
	// Per-Frame Rendering
	//-------------------------------------------------------------------------

	// Render to shadow map
	static void RenderToShadowMap(Flux_CommandList& xCmdBuf, const Flux_DynamicConstantBuffer& xShadowMatrixBuffer);

	//-------------------------------------------------------------------------
	// Render Graph
	//-------------------------------------------------------------------------
	static void SetupRenderGraph(Flux_RenderGraph& xGraph);

	//-------------------------------------------------------------------------
	// Accessors
	//-------------------------------------------------------------------------
	static uint32_t GetTotalInstanceCount();
	static uint32_t GetVisibleInstanceCount();
	static uint32_t GetGroupCount();

private:
	static void ExecuteCulling(Flux_CommandList* pxCmdList, void* pUserData);
	static void ExecuteGBuffer(Flux_CommandList* pxCmdList, void* pUserData);
};
