#pragma once

// Render-graph INTERNAL data types — the structs the graph uses to track resource
// usage, lifetimes, and synthesise barriers. Split out of Flux_RenderGraph.h so a
// newcomer reading the public surface (Flux_RenderGraph, Flux_PassBuilder, AddPass)
// isn't wading through barrier-synthesis plumbing first.
//
// Included near the TOP of Flux_RenderGraph.h, BEFORE Flux_RenderGraph_Pass and the
// Flux_RenderGraph class — two hard ordering reasons:
//   1. Flux_RenderGraph_Pass embeds Flux_RenderGraph_ResourceUsage / _Barrier /
//      _AliasingBarrier by value (in Zenith_Vector members), so those must be complete.
//   2. std::hash<Flux_BarrierKey> (below) MUST be visible before Flux_RenderGraph
//      declares its std::unordered_map<Flux_BarrierKey, ...> members, or the member
//      declaration implicitly instantiates std::hash and the specialization conflicts.
#include "Flux/Flux_GraphResource.h"   // Flux_GraphResource (+ ResourceAccess via Flux_Types → Flux_Enums)
#include <functional>                   // std::hash

struct Flux_RenderGraph_ResourceUsage
{
    Flux_GraphResource m_xResource;
    ResourceAccess m_eAccess = RESOURCE_ACCESS_READ_SRV;
    u_int m_uMipLevel = 0;
    u_int m_uMipCount = 1;
    u_int m_uLayer = 0;
    u_int m_uLayerCount = 1;
};

struct Flux_RenderGraph_Resource
{
    Flux_GraphResource m_xResource;
    // All three values are TOPOLOGICAL ORDER indices (positions in
    // m_xExecutionOrder), populated by ComputeResourceLifetimes. The aliasing
    // packer compares them as execution-time-ordered intervals; pass-
    // declaration indices would mis-rank passes from different subsystems
    // that interleave during topological sort.
    u_int m_uFirstWriteTopoIdx = UINT32_MAX;
    u_int m_uLastReadTopoIdx   = UINT32_MAX;
    // Last write. Distinct from m_uFirstWriteTopoIdx for transients that are
    // written multiple times (e.g. UAV ping-pong patterns). m_uLastUseTopoIdx
    // on TransientResource is max(m_uLastReadTopoIdx, m_uLastWriteTopoIdx) so the
    // packer / aliasing-barrier pass treats the lifetime as covering
    // every actual access, not just up to the first write.
    u_int m_uLastWriteTopoIdx  = UINT32_MAX;
};

// Per-(resource, subresource) state transition emitted by the graph as a
// pass prologue. Synthesised in Compile()::SynthesizeBarriers from the
// declared Read/Write set; the Vulkan backend
// (Zenith_Vulkan.cpp::RecordCommandBuffersTask) dispatches each entry to
// Zenith_Vulkan_CommandBuffer::ImageTransition (image kinds) or BufferBarrier
// (buffer kind) right before each pass executes (outside any active render
// pass scope). The graph is the SOLE barrier authority — graphics programmers
// declare access via Read/Write only; no inline transitions, no backend-side
// ad-hoc transitions.
//
// Resource is held as a Flux_GraphResource so the same struct covers 2D
// images, cube images, and buffers. For buffer entries the four mip/layer
// fields are unused: the SynthesizeBarriers buffer path leaves them at
// (0, 1, 0, 1) and the backend buffer-barrier emitter ignores them. Buffer
// barriers are pure memory + execution barriers (no layout transitions); the
// only fields the backend reads for them are m_xResource, m_eSrcAccess, and
// m_eDstAccess.
struct Flux_RenderGraph_Barrier
{
    Flux_GraphResource m_xResource;
    u_int m_uBaseMip = 0;     // images only — buffer entries leave at 0
    u_int m_uMipCount = 1;    // images only — buffer entries leave at 1
    u_int m_uBaseLayer = 0;   // images only — buffer entries leave at 0
    u_int m_uLayerCount = 1;  // images only — buffer entries leave at 1
    ResourceAccess m_eSrcAccess = RESOURCE_ACCESS_UNDEFINED;
    ResourceAccess m_eDstAccess = RESOURCE_ACCESS_UNDEFINED;
};

// One aliasing hand-off emitted before the pass — translated by the backend
// to a vk::MemoryBarrier. Multiple transients that share an aliasing pool
// and are first-used in the same pass each produce their own entry; the
// backend iterates and emits one pipelineBarrier per entry so the stage /
// access masks stay tight. bSrcIsDepth / bDstIsDepth pre-compute the
// colour-vs-depth disambiguation the access-to-vulkan translator needs.
struct Flux_RenderGraph_AliasingBarrier
{
    ResourceAccess m_eSrcAccess = RESOURCE_ACCESS_UNDEFINED;
    ResourceAccess m_eDstAccess = RESOURCE_ACCESS_UNDEFINED;
    bool m_bSrcIsDepth = false;
    bool m_bDstIsDepth = false;
};

// Subresource key used by the render graph's clear-tracking and barrier-state
// maps. Holds the full attachment pointer (no 48-bit truncation like the old
// u_int64-packed key) plus mip and layer indices packed into a u_int32. The
// static_asserts at the construction site pin the 16-bit packing so bumping
// FLUX_MAX_MIPS above the limit fails at compile time.
struct Flux_BarrierKey
{
    void*   m_pRes             = nullptr;
    u_int32 m_uMipLayerPacked  = 0; // low 16 bits = mip, high 16 bits = layer

    bool operator==(const Flux_BarrierKey& rhs) const
    {
        return m_pRes == rhs.m_pRes && m_uMipLayerPacked == rhs.m_uMipLayerPacked;
    }
};

// Hash specialisation must be visible BEFORE Flux_RenderGraph declares
// std::unordered_map<Flux_BarrierKey, ...> as a member — the member
// declaration triggers instantiation of std::hash<Flux_BarrierKey>, and once
// that happens the specialization below would conflict with the implicit
// instantiation. Keep it here, tightly coupled to the struct.
namespace std
{
    template<> struct hash<Flux_BarrierKey>
    {
        size_t operator()(const Flux_BarrierKey& xKey) const noexcept
        {
            // Raw pointer hash; pointers may cluster at heap/pool boundaries.
            // Clustering is absorbed by the golden-ratio combine below and by
            // std::unordered_map's bucket-chain traversal (which compares keys
            // via Flux_BarrierKey::operator== exactly, so any collision is a
            // perf event — never a correctness bug).
            const size_t h1 = hash<void*>{}(xKey.m_pRes);
            const size_t h2 = hash<u_int32>{}(xKey.m_uMipLayerPacked);
            // Boost-style hash_combine; 0x9e3779b97f4a7c15 is the Fibonacci
            // golden-ratio constant (fractional part of 2^64 / phi).
            return h1 ^ (h2 + 0x9e3779b97f4a7c15ull + (h1 << 6) + (h1 >> 2));
        }
    };
}
