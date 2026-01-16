#pragma once

#include "Flux/Flux.h"

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

	// Dispatch GPU culling compute shader (call before RenderToGBuffer)
	static void DispatchCulling(void*);

	// Render all instance groups to GBuffer
	static void RenderToGBuffer(void*);

	// Render to shadow map
	static void RenderToShadowMap(Flux_CommandList& xCmdBuf, const Flux_DynamicConstantBuffer& xShadowMatrixBuffer);

	//-------------------------------------------------------------------------
	// Task System
	//-------------------------------------------------------------------------
	static void SubmitCullingTask();
	static void WaitForCullingTask();
	static void SubmitRenderTask();
	static void WaitForRenderTask();

	//-------------------------------------------------------------------------
	// Accessors
	//-------------------------------------------------------------------------
	static uint32_t GetTotalInstanceCount();
	static uint32_t GetVisibleInstanceCount();
	static uint32_t GetGroupCount();
};
