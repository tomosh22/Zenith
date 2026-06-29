#pragma once
#include "Zenith.h"            // u_int / u_int64 / Zenith_Assert (defined before Flux.h in the PCH)
#include "Flux/Flux_Types.h"   // handles, enums, view structs, Flux_BindingSlot/SubresourceRange
#include "Flux/Flux_Fwd.h"     // the Flux_* aliases + forward decls of the other D3D12 classes
#include "Flux/Flux_RecordValidation.h"  // shared constructor-era validation (seam-safe: Flux_Types.h only)

// ============================================================================
// NO-OP D3D12 null backend: per-worker command recorder.
//
// Mirrors the neutral public surface of Zenith_Vulkan_CommandBuffer (aliased
// as Flux_CommandBuffer) and satisfies the umbrella FluxBackendCommandRecorder
// concept (FluxBackendRecordingLifecycle, FluxBackendPipelineBinding,
// FluxBackendVertexIndexStreams, FluxBackendBasicDraws, FluxBackendIndirectDraws,
// FluxBackendCompute, FluxBackendResourceBinding, FluxBackendDynamicState,
// FluxBackendDebugMarkers) plus the FluxBackendSync concept (ResourceBarrier).
//
// Performs ZERO real recording: every method is an inline no-op stub. Exists
// only to prove the Flux command-recorder + sync concepts are backend-neutral.
//
// Forward-declared (not Flux_Fwd.h) backend-neutral types used in signatures:
class Flux_GraphResource;
struct Flux_RenderingBeginInfo;

// vk::-typed methods on the Vulkan class are intentionally OMITTED: their
// signatures mention backend-internal Vulkan types, so the engine never calls
// them on the neutral Flux_CommandBuffer alias:
//   - InitialiseWithCustomPool(const vk::CommandPool&, ...)
//   - GetCurrentCmdBuffer()  -> vk::CommandBuffer&
//   - ImageTransitionBarrier / ImageTransitionBarrierRange (vk:: params)
//   - SetCurrentRenderPass(vk::RenderPass)
//   - the public vk:: data members (m_xCurrentCmdBuffer, m_xCurrentRenderPass)
// ============================================================================
// Every method below is a no-op stub, so every parameter is intentionally
// unused. Disable C4100 (unreferenced formal parameter) for the whole class
// rather than /*comment*/-ing the ~40 parameters of the recorder surface.
#pragma warning(push)
#pragma warning(disable : 4100)
class Zenith_D3D12_CommandBuffer
{
public:
	Zenith_D3D12_CommandBuffer() {}
	~Zenith_D3D12_CommandBuffer() {}

	// ---- Initialisation (neutral overload only) ----------------------------
	void Initialise(CommandType /*eType*/ = COMMANDTYPE_GRAPHICS) { }

	// ---- FluxBackendRecordingLifecycle -------------------------------------
	void BeginRecording() { }
	void EndRendering() { }
	void EndRecording(bool bEndPass = true) { }
	void EndAndCpuWait(bool bEndPass) { }
	void BeginRendering(const Flux_RenderingBeginInfo& xInfo) { }

	// ---- FluxBackendVertexIndexStreams -------------------------------------
	void SetVertexBuffer(const Flux_VertexBuffer& xVertexBuffer, uint32_t uBindPoint = 0) { }
	void SetVertexBuffer(const Flux_DynamicVertexBuffer& xVertexBuffer, uint32_t uBindPoint = 0) { }
	void SetVertexBuffer(const Flux_ReadWriteBuffer& xVertexBuffer, uint32_t uBindPoint = 0, size_t uByteOffset = 0) { }
	void SetIndexBuffer(const Flux_IndexBuffer& xIndexBuffer) { }

	// ---- FluxBackendBasicDraws ---------------------------------------------
	void Draw(uint32_t uNumVerts) { }
	void DrawIndexed(uint32_t uNumIndices, uint32_t uNumInstances = 1, uint32_t uVertexOffset = 0, uint32_t uIndexOffset = 0, uint32_t uInstanceOffset = 0) { }

	// ---- FluxBackendIndirectDraws ------------------------------------------
	void DrawIndexedIndirect(const Flux_IndirectBuffer* pxIndirectBuffer, uint32_t uDrawCount, uint32_t uOffset = 0, uint32_t uStride = 20) { }
	void DrawIndexedIndirectCount(const Flux_IndirectBuffer* pxIndirectBuffer, const Flux_IndirectBuffer* pxCountBuffer, uint32_t uMaxDrawCount, uint32_t uIndirectOffset = 0, uint32_t uCountOffset = 0, uint32_t uStride = 20) { }

	// ---- FluxBackendPipelineBinding ----------------------------------------
	// No-op recording, but the shared validators run so the null backend keeps the
	// constructor-era diagnostics the Flux_CommandList DSL used to fire on it.
	void SetPipeline(Flux_Pipeline* pxPipeline) { FluxAssertPipeline(pxPipeline, "SetPipeline"); }
	void BindComputePipeline(Flux_Pipeline* pxPipeline) { FluxAssertPipeline(pxPipeline, "BindComputePipeline"); }

	// ---- FluxBackendResourceBinding ----------------------------------------
	void BindSRV(const Flux_ShaderResourceView* pxSRV, const Flux_BindingSlot& xSlot, Flux_Sampler* pxSampler = nullptr) { FluxAssertValidSRV(pxSRV); }
	void BindSRV_Buffer(const Flux_ShaderResourceView_Buffer& xSRV, const Flux_BindingSlot& xSlot) { FluxAssertValidSRVBuffer(xSRV); }
	void BindUAV_Texture(const Flux_UnorderedAccessView_Texture* pxUAV, const Flux_BindingSlot& xSlot) { FluxAssertValidUAVTexture(pxUAV); }
	void BindUAV_Buffer(const Flux_UnorderedAccessView_Buffer* pxUAV, const Flux_BindingSlot& xSlot) { FluxAssertValidUAVBuffer(pxUAV); }
	void BindCBV(const Flux_ConstantBufferView* pxCBV, const Flux_BindingSlot& xSlot) { FluxAssertValidCBV(pxCBV); }
	void BindDrawConstants(const void* pData, size_t uSize, const Flux_BindingSlot& xSlot) { FluxAssertDrawConstantsSize(uSize, 2048); }
	void UseBindlessTextures(const uint32_t uSet) { }

	// Neutral binding extra (not in the concept, but on the public surface):
	void BindAccelerationStruct(void* pxStruct, uint32_t uBindPoint) { }

	// ---- FluxBackendDynamicState -------------------------------------------
	void SetDepthBias(float fConstant, float fSlope, float fClamp) { }
	void SetShoudClear(const bool bClear) { }

	// ---- FluxBackendCompute ------------------------------------------------
	void Dispatch(uint32_t uGroupCountX, uint32_t uGroupCountY, uint32_t uGroupCountZ) { }

	// ---- FluxBackendSync + neutral barrier helpers -------------------------
	void ResourceBarrier(const Flux_GraphResource& xResource,
		const Flux_SubresourceRange& xRange,
		ResourceAccess eSrcAccess, ResourceAccess eDstAccess) { }

	void ImageBarrier(Flux_Texture* pxTexture, uint32_t uOldLayout, uint32_t uNewLayout) { }

	void ImageTransition(Flux_RenderAttachment* pxAttachment,
		uint32_t uBaseMip, uint32_t uMipCount,
		uint32_t uBaseLayer, uint32_t uLayerCount,
		ResourceAccess eSrcAccess, ResourceAccess eDstAccess) { }
	void ImageTransition(const Flux_GraphResource& xResource,
		uint32_t uBaseMip, uint32_t uMipCount,
		uint32_t uBaseLayer, uint32_t uLayerCount,
		ResourceAccess eSrcAccess, ResourceAccess eDstAccess) { }

	void BufferBarrier(Flux_Buffer* pxBuffer,
		ResourceAccess eSrcAccess, ResourceAccess eDstAccess) { }

	// ---- Misc neutral surface ----------------------------------------------
	void RenderImGui() { }

	void* Platform_GetCurrentCmdBuffer() const { return nullptr; }

	u_int GetWorkerIndex() const { return 0; }

	// ---- FluxBackendDebugMarkers (profiling builds only) -------------------
#ifdef ZENITH_FLUX_PROFILING
	void BeginDebugMarker(const char* szName) { }
	void EndDebugMarker() { }
	// GPU per-pass timestamps: no GPU on the null backend, so no timing.
	u_int BeginGPUTimer(const char* szName, u_int uExecutionIndex) { return ~0u; }
	void  EndGPUTimer(u_int uTimerIdx) { }
#endif

private:
	static inline u_int ms_uDummyHandle = 1;
};
#pragma warning(pop)
