#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_CommandList.h"
#include "Flux/Flux_Buffers.h"
#include "Zenith_PlatformGraphics_Include.h"

// Concept: per-worker command recording. Instance methods on the backend
// command-buffer type (aliased as Flux_CommandBuffer). Each Flux_CommandList
// command's operator()(Flux_CommandBuffer*) calls these to translate the
// portable command DSL into native draws/dispatches/binds.
//
// All inputs are engine-typed. Render-pass / barrier methods that take vk::*
// types live in the backend-internal surface and are NOT in this concept;
// the engine-typed ImageTransition overloads are formalised in
// Flux_Concept_Sync.h.
//
// ---- Sub-concept decomposition -------------------------------------------
// The umbrella concept FluxBackendCommandRecorder is composed from seven
// per-capability sub-concepts so:
//   1. A compile error on a missing method names the specific sub-concept
//      that failed (e.g. "FluxBackendIndirectDraws requires
//      DrawIndexedIndirectCount") rather than flagging the whole mega-concept.
//   2. A future second backend can satisfy the capabilities it implements
//      (e.g. a mobile backend without indirect-count draws) and opt out of
//      the umbrella — downstream code then gates on the sub-concept it
//      actually needs. Currently the engine needs all seven, so every
//      backend must satisfy the umbrella; this split is for future
//      flexibility, not gates any current code paths.
//   3. Per-sub-concept parameter lists stay small, which keeps each
//      requires-clause readable.

template <typename T>
concept FluxBackendRecordingLifecycle = requires(
	T& xRec,
	bool bEndPass,
	bool bClearColour,
	bool bClearDepth,
	bool bClearStencil,
	bool bDepthReadOnly,
	uint32_t uNumColour,
	const Flux_RenderGraph_AttachmentRef* paxColour,
	const Flux_RenderGraph_AttachmentRef& xDepth)
{
	{ xRec.BeginRecording()                                                                    } -> std::same_as<void>;
	{ xRec.EndRecording(bEndPass)                                                              } -> std::same_as<void>;
	{ xRec.EndRenderPass()                                                                     } -> std::same_as<void>;
	{ xRec.BeginRenderPass(paxColour, uNumColour, xDepth,
	                       bClearColour, bClearDepth, bClearStencil, bDepthReadOnly)           } -> std::same_as<void>;
};

template <typename T>
concept FluxBackendPipelineBinding = requires(T& xRec, Flux_Pipeline* pxPipeline)
{
	{ xRec.SetPipeline(pxPipeline)                                                             } -> std::same_as<void>;
	{ xRec.BindComputePipeline(pxPipeline)                                                     } -> std::same_as<void>;
};

template <typename T>
concept FluxBackendVertexIndexStreams = requires(
	T& xRec,
	uint32_t uBindPoint,
	const Flux_VertexBuffer& xVB,
	const Flux_DynamicVertexBuffer& xDVB,
	const Flux_IndexBuffer& xIB)
{
	{ xRec.SetVertexBuffer(xVB, uBindPoint)                                                    } -> std::same_as<void>;
	{ xRec.SetVertexBuffer(xDVB, uBindPoint)                                                   } -> std::same_as<void>;
	{ xRec.SetIndexBuffer(xIB)                                                                 } -> std::same_as<void>;
};

template <typename T>
concept FluxBackendBasicDraws = requires(
	T& xRec,
	uint32_t uNumVerts,
	uint32_t uDrawCount,
	uint32_t uVertexOffset,
	uint32_t uIndexOffset,
	uint32_t uInstanceOffset)
{
	{ xRec.Draw(uNumVerts)                                                                     } -> std::same_as<void>;
	{ xRec.DrawIndexed(uNumVerts, uDrawCount, uVertexOffset, uIndexOffset, uInstanceOffset)    } -> std::same_as<void>;
};

template <typename T>
concept FluxBackendIndirectDraws = requires(
	T& xRec,
	uint32_t uDrawCount,
	uint32_t uIndirectBufferOffset,
	uint32_t uCountBufferOffset,
	uint32_t uStride,
	const Flux_IndirectBuffer* pxArgsB,
	const Flux_IndirectBuffer* pxCountB)
{
	{ xRec.DrawIndexedIndirect(pxArgsB, uDrawCount, uIndirectBufferOffset, uStride)                                    } -> std::same_as<void>;
	{ xRec.DrawIndexedIndirectCount(pxArgsB, pxCountB, uDrawCount, uIndirectBufferOffset, uCountBufferOffset, uStride) } -> std::same_as<void>;
};

template <typename T>
concept FluxBackendCompute = requires(T& xRec, uint32_t uGroupsX, uint32_t uGroupsY, uint32_t uGroupsZ)
{
	{ xRec.Dispatch(uGroupsX, uGroupsY, uGroupsZ)                                              } -> std::same_as<void>;
};

template <typename T>
concept FluxBackendResourceBinding = requires(
	T& xRec,
	uint32_t uDescSet,
	uint32_t uBinding,
	void* pData,
	size_t sz,
	const Flux_ShaderResourceView* pxSRV,
	const Flux_UnorderedAccessView_Texture* pxUAVTex,
	const Flux_UnorderedAccessView_Buffer* pxUAVBuf,
	const Flux_ConstantBufferView* pxCBV,
	Flux_Sampler* pxSampler)
{
	{ xRec.BeginBind(uDescSet)                                                                 } -> std::same_as<void>;
	{ xRec.BindSRV(pxSRV, uBinding, pxSampler)                                                 } -> std::same_as<void>;
	{ xRec.BindCBV(pxCBV, uBinding)                                                            } -> std::same_as<void>;
	{ xRec.BindUAV_Texture(pxUAVTex, uBinding)                                                 } -> std::same_as<void>;
	{ xRec.BindUAV_Buffer(pxUAVBuf, uBinding)                                                  } -> std::same_as<void>;
	{ xRec.BindDrawConstants(pData, sz, uBinding)                                              } -> std::same_as<void>;
	{ xRec.UseBindlessTextures(uDescSet)                                                       } -> std::same_as<void>;
};

template <typename T>
concept FluxBackendDynamicState = requires(
	T& xRec,
	CullMode eCull,
	float fConstantBias,
	float fSlopeBias,
	float fClampBias)
{
	{ xRec.SetCullMode(eCull)                                                                  } -> std::same_as<void>;
	{ xRec.SetDepthBias(fConstantBias, fSlopeBias, fClampBias)                                 } -> std::same_as<void>;
};

// GPU debug-marker pair, emitted once per Flux_CommandList from
// IterateCommands() so RenderDoc / Nsight / PIX show one labelled group per
// logical pass. Gated on ZENITH_FLUX_PROFILING — the methods, calls, concept
// and conformance assert all evaporate together in profiling-off builds (same
// pattern as FluxBackendImGuiTools / ZENITH_TOOLS).
//
// Cross-backend contract: each Flux_CommandList corresponds to one logical
// pass (one render-graph pass for graphics, one standalone compute list, etc.)
// and IterateCommands() runs inside whatever pass / encoder scope the backend
// has open at that point — graphics passes are bracketed by BeginRenderPass /
// EndRenderPass on the same Flux_CommandBuffer, compute passes have no render
// pass but are still recorded into the active backend command buffer.
//
// Vulkan satisfies the contract via vkCmdBegin/EndDebugUtilsLabelEXT (allowed
// anywhere inside an open command buffer, both in and out of render passes).
//
// A future Metal backend will need to route Begin/EndDebugMarker to the
// currently-active MTL*CommandEncoder (pushDebugGroup: / popDebugGroup),
// because Metal markers are encoder-scoped. The "encoder is active when
// IterateCommands() runs" property holds today, so the Metal mapping is
// mechanical.
#ifdef ZENITH_FLUX_PROFILING
template <typename T>
concept FluxBackendDebugMarkers = requires(T& xRec, const char* szName)
{
	{ xRec.BeginDebugMarker(szName)                                                            } -> std::same_as<void>;
	{ xRec.EndDebugMarker()                                                                    } -> std::same_as<void>;
};
#endif

// Umbrella concept. Composes the sub-concepts. A backend that claims
// to be a full command recorder (i.e. participates in the conformance
// static_assert in Flux_BackendConformance.cpp) must satisfy every slice;
// a future backend can pick individual sub-concepts to implement partial
// support. FluxBackendDebugMarkers participates only in ZENITH_FLUX_PROFILING
// builds, mirroring the methods + conformance assert.
template <typename T>
concept FluxBackendCommandRecorder =
	FluxBackendRecordingLifecycle <T> &&
	FluxBackendPipelineBinding    <T> &&
	FluxBackendVertexIndexStreams <T> &&
	FluxBackendBasicDraws         <T> &&
	FluxBackendIndirectDraws      <T> &&
	FluxBackendCompute            <T> &&
	FluxBackendResourceBinding    <T> &&
#ifdef ZENITH_FLUX_PROFILING
	FluxBackendDebugMarkers       <T> &&
#endif
	FluxBackendDynamicState       <T>;
