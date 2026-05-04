#pragma once

#include "Collections/Zenith_HashMap.h"
#include "Collections/Zenith_HashSet.h"
#include "Collections/Zenith_Vector.h"
#include "Flux/Flux.h"
#include <unordered_set> // #TODO: Replace with engine hash set
#include <string>
#include <utility>

using Flux_RenderGraph_OnRecordFunc = void(*)(Flux_CommandList*, void*);
using Flux_RenderGraph_OnPrepareFunc = void(*)(void*);

// Sentinel constants for "all mips" / "all layers" when declaring a
// subresource usage range. Map onto VK_REMAINING_* in the backend.
constexpr u_int FLUX_RG_ALL_MIPS   = UINT32_MAX;
constexpr u_int FLUX_RG_ALL_LAYERS = UINT32_MAX;

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
    u_int m_uFirstWrite = UINT32_MAX;
    u_int m_uLastRead   = UINT32_MAX;
    // Last write. Distinct from m_uFirstWrite for transients that are
    // written multiple times (e.g. UAV ping-pong patterns). m_uLastUse
    // on TransientResource is max(m_uLastRead, m_uLastWrite) so the
    // packer / aliasing-barrier pass treats the lifetime as covering
    // every actual access, not just up to the first write.
    u_int m_uLastWrite  = UINT32_MAX;
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

struct Flux_RenderGraph_Pass
{
#ifdef ZENITH_TOOLS
    std::string m_strName;
#endif
    Zenith_Vector<Flux_RenderGraph_ResourceUsage> m_xReads;
    Zenith_Vector<Flux_RenderGraph_ResourceUsage> m_xWrites;
    Zenith_Vector<u_int> m_xExplicitDependencies;
    // Subresource transitions to emit BEFORE pfnOnRecord runs. Computed in
    // Compile()::SynthesizeBarriers; the record task prepends them to the
    // pass's command list as Flux_CommandImageTransition entries.
    Zenith_Vector<Flux_RenderGraph_Barrier> m_xPrologueBarriers;
    Flux_RenderGraph_OnRecordFunc m_pfnOnRecord = nullptr;
    Flux_RenderGraph_OnPrepareFunc m_pfnOnPrepare = nullptr;
    void* m_pUserData = nullptr;
    u_int m_uTopologicalOrder = UINT32_MAX;
    bool m_bIsCompute = false;
    bool m_bEnabled = true;
    bool m_bRequestsClear = false;
    bool m_bClearTargets = false;
    // One entry per transient handed off TO this pass from a different prior
    // occupant of the same aliasing-pool slot. Populated by
    // SynthesizeAliasingBarriers; consumed by the backend's pass-prologue
    // emitter which turns each entry into a vk::MemoryBarrier (separate
    // pipelineBarrier calls so each hand-off carries its own src/dst stage
    // mask instead of being unioned into a single over-conservative barrier).
    Zenith_Vector<Flux_RenderGraph_AliasingBarrier> m_xAliasingBarriers;
    Flux_RenderGraph_AttachmentRef m_axColourAttachments[FLUX_MAX_TARGETS];
    uint32_t m_uNumColourAttachments = 0;
    Flux_RenderGraph_AttachmentRef m_xDepthStencil;
    Flux_CommandList* m_pxCommandList = nullptr;

    // Debug-name accessor that compiles in all configurations. Tools builds return the real
    // name; shipping builds return a placeholder so Zenith_Assert(...) sites keep compiling
    // without pulling std::string weight into release binaries.
#ifdef ZENITH_TOOLS
    const char* DebugName() const { return m_strName.c_str(); }
#else
    const char* DebugName() const { return "<release>"; }
#endif

    ~Flux_RenderGraph_Pass()
    {
        delete m_pxCommandList;
        m_pxCommandList = nullptr;
    }
};

class Flux_RenderGraph;

// Fluent builder returned by Flux_RenderGraph::AddPass so subsystems can
// declare reads / writes / dependencies / user data on a single expression
// without repeating the pass index. Implicitly converts to Flux_PassHandle so
// existing call sites that stored the handle continue to compile unchanged:
//
//     Flux_PassHandle xPass = xGraph.AddPass(...);               // still works
//     xGraph.AddPass("HiZ Mip", ExecuteHiZMip)
//           .UserData(uMip)                                      // typed, no void* cast
//           .Reads (GetHiZBuffer(), RESOURCE_ACCESS_READ_SRV, uMip-1, 1)
//           .Writes(GetHiZBuffer(), RESOURCE_ACCESS_WRITE_UAV, uMip, 1);
//
// The builder is lightweight (just a graph pointer + the handle) and meant
// to be used immediately in the AddPass expression; do not store one.
class Flux_PassBuilder
{
public:
    Flux_PassBuilder(Flux_RenderGraph* pxGraph, Flux_PassHandle xPass)
        : m_pxGraph(pxGraph), m_xPass(xPass) {}
    ~Flux_PassBuilder() { m_uCanary = CANARY_DEAD; }

    // Non-copyable / non-movable: the builder is a transient fluent-API handle
    // meant to be used immediately in the AddPass expression.
    Flux_PassBuilder(const Flux_PassBuilder&) = delete;
    Flux_PassBuilder& operator=(const Flux_PassBuilder&) = delete;
    Flux_PassBuilder(Flux_PassBuilder&&) = delete;
    Flux_PassBuilder& operator=(Flux_PassBuilder&&) = delete;

    operator Flux_PassHandle() const { return m_xPass; }
    Flux_PassHandle Handle() const { return m_xPass; }

    // Resource declarations — mirror the graph's Read/Write but return *this as
    // an rvalue-ref so calls can chain. All chain methods are &&-qualified: the
    // builder only exists as a prvalue/xvalue returned from AddPass + chain, and
    // any attempt to capture it via `auto& x = xGraph.AddPass(...).Foo();` is a
    // compile error because `auto&` can't bind to an xvalue. Store the
    // Flux_PassHandle via the implicit conversion instead, and use the graph's
    // public Read/Write/... helpers for conditional/loop declarations.
    Flux_PassBuilder&& Reads (Flux_RenderAttachment& xImage, ResourceAccess eAccess = RESOURCE_ACCESS_READ_SRV, u_int uMip = 0, u_int uMipCount = FLUX_RG_ALL_MIPS) &&;
    Flux_PassBuilder&& Writes(Flux_RenderAttachment& xImage, ResourceAccess eAccess = RESOURCE_ACCESS_WRITE_RTV, u_int uMip = 0, u_int uMipCount = 1) &&;
    Flux_PassBuilder&& Reads (Flux_RenderAttachmentCube& xImage, ResourceAccess eAccess = RESOURCE_ACCESS_READ_SRV,
                              u_int uMip = 0, u_int uMipCount = FLUX_RG_ALL_MIPS,
                              u_int uLayer = 0, u_int uLayerCount = FLUX_RG_ALL_LAYERS) &&;
    Flux_PassBuilder&& Writes(Flux_RenderAttachmentCube& xImage, ResourceAccess eAccess = RESOURCE_ACCESS_WRITE_RTV,
                              u_int uMip = 0, u_int uMipCount = 1,
                              u_int uLayer = 0, u_int uLayerCount = 1) &&;
    Flux_PassBuilder&& ReadsBuffer (Flux_Buffer& xBuffer, ResourceAccess eAccess) &&;
    Flux_PassBuilder&& WritesBuffer(Flux_Buffer& xBuffer, ResourceAccess eAccess) &&;
    Flux_PassBuilder&& ReadsTransient (Flux_TransientHandle xHandle, ResourceAccess eAccess = RESOURCE_ACCESS_READ_SRV) &&;
    Flux_PassBuilder&& WritesTransient(Flux_TransientHandle xHandle, ResourceAccess eAccess = RESOURCE_ACCESS_WRITE_RTV) &&;
    Flux_PassBuilder&& ReadsTransient (Flux_TransientHandle xHandle, ResourceAccess eAccess, u_int uMip, u_int uMipCount) &&;
    Flux_PassBuilder&& WritesTransient(Flux_TransientHandle xHandle, ResourceAccess eAccess, u_int uMip, u_int uMipCount) &&;

    // Pass config
    Flux_PassBuilder&& DependsOn (Flux_PassHandle xDependency) &&;
    Flux_PassBuilder&& ClearTargets(bool bClear = true) &&;
    Flux_PassBuilder&& Prepare   (Flux_RenderGraph_OnPrepareFunc pfnPrepare) &&;

    // Typed user data — packs T bitwise into the pass's void* m_pUserData slot.
    // Retrieve via Flux_UnpackUserData<T>(pUserData) in the record callback.
    // Restricted to types that fit in a pointer and are trivially copyable —
    // anything larger belongs in a statically allocated user-data struct
    // addressed by pointer.
    template<typename T>
    Flux_PassBuilder&& UserData(T xData) &&;

private:
    // Primary defence against `auto& x = xGraph.AddPass(...).Foo()` is the
    // &&-qualifier on every chain method — `auto&` (and `auto&&` bound to a
    // named local that's subsequently used like an lvalue) can't satisfy the
    // ref-qualifier, so the misuse is a compile error. The canary below is a
    // secondary, runtime check: the destructor overwrites it and every chain
    // method asserts it still matches. Belt-and-braces for any residual route
    // the compiler might leave behind (e.g. a future overload set that accepts
    // both ref-qualifications).
    static constexpr u_int32 CANARY_LIVE = 0xBADC0FFEu;
    static constexpr u_int32 CANARY_DEAD = 0xDEADBEEFu;
    void AssertAlive(const char* szCaller) const;

    // Assign the packed bits to the pass's m_pUserData slot.
    void SetUserData(void* pData);

    Flux_RenderGraph* m_pxGraph;
    Flux_PassHandle m_xPass;
    u_int32 m_uCanary = CANARY_LIVE;
};

// Unpack a T previously stored via Flux_PassBuilder::UserData<T>. Intended
// for use at the top of a record callback to recover the typed payload.
template<typename T>
T Flux_UnpackUserData(void* pUserData)
{
    static_assert(sizeof(T) <= sizeof(void*), "Flux_UnpackUserData: T must fit in void* (use a pointer to static storage for larger payloads)");
    static_assert(std::is_trivially_copyable_v<T>, "Flux_UnpackUserData: T must be trivially copyable");
    T xData;
    memcpy(&xData, &pUserData, sizeof(T));
    return xData;
}

template<typename T>
Flux_PassBuilder&& Flux_PassBuilder::UserData(T xData) &&
{
    static_assert(sizeof(T) <= sizeof(void*), "Flux_PassBuilder::UserData: T must fit in void* (use a pointer to static storage for larger payloads)");
    static_assert(std::is_trivially_copyable_v<T>, "Flux_PassBuilder::UserData: T must be trivially copyable");
    // AssertAlive explicitly guards the template-inlined SetUserData below;
    // the non-template chain methods all call AssertAlive but this template
    // bypasses the wrapper, so the canary check has to be written out here.
    AssertAlive("UserData");
    void* pStorage = nullptr;
    memcpy(&pStorage, &xData, sizeof(T));
    SetUserData(pStorage);
    return std::move(*this);
}

class Flux_RenderGraph
{
    // Unit tests poke private state (m_xExecutionOrder, SynthesizeBarriers)
    // to exercise barrier synthesis without spinning up a Vulkan device.
    friend class Zenith_UnitTests;

public:
    Flux_RenderGraph() = default;
    ~Flux_RenderGraph() { Clear(); }

    // Pass registration. Returns a Flux_PassBuilder for fluent chaining
    // (Reads / Writes / DependsOn / ClearTargets / Prepare / UserData<T>).
    // Chain inline and let the expression implicitly convert to Flux_PassHandle
    // if you need to store the handle for later SetEnabled toggling. Chain
    // methods are &&-qualified, so `auto& x = xGraph.AddPass(...).Foo();` is a
    // compile error — use the graph's public Read/Write helpers below for
    // conditional or loop-driven extra declarations.
    [[nodiscard]] Flux_PassBuilder AddPass(const char* szName, Flux_RenderGraph_OnRecordFunc pfnOnRecord, void* pUserData = nullptr, u_int uInitialCapacity = 4096);

    // Graph-owned resources. Subsystems declare each resource by descriptor in
    // SetupRenderGraph; the graph allocates the backing Flux_RenderAttachment
    // at Compile() time. Handles are invalidated on Clear() / recompile — the
    // graph's generation counter increments and AssertTransientHandleValid
    // trips on stale handles.
    Flux_TransientHandle CreateTransient(const Flux_TransientTextureDesc& xDesc);
    Flux_RenderAttachment& GetTransientAttachment(Flux_TransientHandle xHandle);
    const Flux_RenderAttachment& GetTransientAttachment(Flux_TransientHandle xHandle) const;

    // Runtime pass-enable toggle. Intentionally public (not builder-only)
    // because it's called post-Compile by subsystems like Fog and IBL that
    // flip which technique's passes run each frame based on debug variables.
    // AddPass returns a Flux_PassBuilder; subsystems that need this capture
    // the handle via the builder's implicit conversion to Flux_PassHandle.
    void SetEnabled(Flux_PassHandle xPass, bool bEnabled);

    // Transient-aliasing runtime toggle. When enabled (default) AND the
    // backend reports SupportsTransientAliasing() == true, the render graph
    // packs non-overlapping transient lifetimes into shared VMA blocks so
    // peak transient VRAM drops ~20-40% on a typical deferred frame. When
    // disabled, every transient gets its own standalone allocation (the
    // pre-aliasing behaviour). The visual output is identical in both
    // modes — the toggle IS the primary verification tool for aliasing
    // correctness (flip on/off, confirm bit-identical frames).
    //
    // Toggling calls MarkDirty() so the next Compile rebuilds the pool
    // layout. Bound to a debug variable (Render/RenderGraph/Transient Aliasing)
    // in Flux_RenderGraph's setup so the editor can flip it at runtime.
    void SetAliasingEnabled(bool bEnabled);
    bool IsAliasingEnabled() const { return m_bAliasingEnabled; }

    // Produce a u_int64 key that hashes every transient property that affects
    // memory-requirement compatibility. Public so unit tests can exercise it
    // directly without friending the whole class; pure function, no state
    // touched. Two transients with the same signature CAN alias (compatible
    // heap + alignment class); two with different signatures MUST NOT.
    static u_int64 MakeTransientMemorySignature(const Flux_TransientTextureDesc& xDesc);

    bool Compile();
    void Execute();
    void MarkDirty();
    bool IsDirty() const { return m_bDirty; }
    void Clear();

    const Zenith_Vector<Flux_RenderGraph_Pass*>& GetPasses() const { return m_xPasses; }
    const Zenith_Vector<u_int>& GetExecutionOrder() const { return m_xExecutionOrder; }
    const Zenith_HashMap<void*, Flux_RenderGraph_Resource>& GetResources() const { return m_xResources; }

    // Human-readable list of passes in their compiled execution order. Format is
    // "PassA -> PassB -> PassC ...". Disabled passes are suffixed " (disabled)".
    // Returns the empty string before the first Compile(). Useful for answering
    // "what runs when?" without manually tracing the topological sort —
    // surfaced via the "Flux/PrintPassOrder" debug button.
    std::string GetPassOrderDescription() const;

    // --- Current-pass thread-local context --------------------------------
    // Set around each pfnOnRecord callback by Flux_RenderGraph_RecordPassTask.
    // Enables bind-time assertions ("is the resource I'm about to bind declared
    // as a Read/Write on this pass?") without threading the pass through every
    // Flux_ShaderBinder call. Null outside a pass's recording window.
    static const Flux_RenderGraph_Pass* GetCurrentRecordingPass();
    // Scoped guard — sets TLS to the pass on construction, restores prior value
    // (usually nullptr) on destruction. Used by the record task.
    class CurrentPassScope
    {
    public:
        CurrentPassScope(const Flux_RenderGraph_Pass* pxPass);
        ~CurrentPassScope();
    private:
        const Flux_RenderGraph_Pass* m_pxPrev;
    };

    // Assert that the bound resource (identified by VRAM handle) appears in the
    // current recording pass's reads or writes with an access compatible with
    // the binding kind. Called from Flux_ShaderBinder. When no pass is recording
    // (e.g. an Initialise-time bind), returns silently — Flux_ShaderBinder has
    // callers outside the render graph recording window.
    // bIsWrite: true for UAV bindings that express a write (or read-modify-write),
    //           false for SRV / read-only binds.
    static void AssertBoundResourceDeclared(Flux_VRAMHandle xVRAMHandle, bool bIsWrite, const char* szBindCall);

    // Per-pass resource declarations. Prefer the fluent Flux_PassBuilder API
    // returned from AddPass for inline chains; these handle-taking overloads
    // exist for conditional / loop-driven declarations that can't fit a chain.
    // Flux_PassBuilder itself forwards each chained call into one of these.
    void Read(Flux_PassHandle xPass, Flux_RenderAttachment& xImage, ResourceAccess eAccess = RESOURCE_ACCESS_READ_SRV, u_int uMip = 0, u_int uMipCount = FLUX_RG_ALL_MIPS);
    void Write(Flux_PassHandle xPass, Flux_RenderAttachment& xImage, ResourceAccess eAccess = RESOURCE_ACCESS_WRITE_RTV, u_int uMip = 0, u_int uMipCount = 1);
    void Read(Flux_PassHandle xPass, Flux_RenderAttachmentCube& xImage, ResourceAccess eAccess = RESOURCE_ACCESS_READ_SRV,
              u_int uMip = 0, u_int uMipCount = FLUX_RG_ALL_MIPS,
              u_int uLayer = 0, u_int uLayerCount = FLUX_RG_ALL_LAYERS);
    void Write(Flux_PassHandle xPass, Flux_RenderAttachmentCube& xImage, ResourceAccess eAccess = RESOURCE_ACCESS_WRITE_RTV,
               u_int uMip = 0, u_int uMipCount = 1,
               u_int uLayer = 0, u_int uLayerCount = 1);
    void ReadBuffer(Flux_PassHandle xPass, Flux_Buffer& xBuffer, ResourceAccess eAccess);
    void WriteBuffer(Flux_PassHandle xPass, Flux_Buffer& xBuffer, ResourceAccess eAccess);

    // Per-frame: tell the graph that a buffer was just host-written (typically
    // via Flux_MemoryManager::UploadBufferDataAtOffset during a Prepare
    // callback). The next Synthesize cycle seeds the buffer's barrier-tracker
    // state with RESOURCE_ACCESS_HOST_TRANSFER_WRITE, so the first pass that
    // declares it as a read gets a TransferWrite@eTransfer → ShaderRead@…
    // buffer-memory barrier emitted in its prologue. Without this, the
    // synthesizer assumes the buffer's prior state is whatever the previous
    // frame's last pass left behind (or UNDEFINED on the first frame), and
    // the barrier emitted misses TRANSFER_WRITE on the src access mask —
    // memory availability for the host transfer write is never published.
    //
    // Also adds the buffer to a persistent "externally-written" set so the
    // orphaned-read validator (which runs at Compile time, before the first
    // Prepare callback) doesn't trip on Read declarations whose writers live
    // outside the graph. Idempotent on both lists.
    void MarkBufferHostWritten(const Flux_Buffer& xBuffer);

    // Drop a buffer from the externally-written set. Call from teardown
    // paths (e.g. terrain DestroyCullingResources) so the validator
    // doesn't end up exempting an address that gets reused by a different,
    // legitimately-orphaned buffer.
    void UnmarkBufferHostWritten(const Flux_Buffer& xBuffer);
    void ReadTransient(Flux_PassHandle xPass, Flux_TransientHandle xHandle, ResourceAccess eAccess = RESOURCE_ACCESS_READ_SRV);
    void WriteTransient(Flux_PassHandle xPass, Flux_TransientHandle xHandle, ResourceAccess eAccess = RESOURCE_ACCESS_WRITE_RTV);
    void ReadTransient(Flux_PassHandle xPass, Flux_TransientHandle xHandle, ResourceAccess eAccess, u_int uMip, u_int uMipCount);
    void WriteTransient(Flux_PassHandle xPass, Flux_TransientHandle xHandle, ResourceAccess eAccess, u_int uMip, u_int uMipCount);
    void DependsOn(Flux_PassHandle xDependentPass, Flux_PassHandle xDependencyPass);
    void SetPrepare(Flux_PassHandle xPass, Flux_RenderGraph_OnPrepareFunc pfnOnPrepare);
    void SetClear(Flux_PassHandle xPass, bool bClearTargets);

private:

    // Transient resources owned by the graph (heap-allocated for pointer stability —
    // Flux_GraphResource stores raw pointers to the m_xAttachment, so the address
    // must not move when new transients are added to the vector).
    struct TransientResource
    {
        Flux_TransientTextureDesc m_xDesc;
        Flux_RenderAttachment m_xAttachment;
        bool m_bAllocated = false;
        // Lifetime data populated by ComputeTransientLifetimes after Compile's
        // resource-lifetime pass. Both fields are UINT32_MAX iff the transient
        // was never referenced by any enabled pass (the single sentinel meaning).
        // When referenced, m_uFirstWrite is the topological position of the
        // first writing pass; m_uLastUse is the topological position of the
        // last access, taking the max across last-read AND last-write so
        // multi-write transients (UAV ping-pong patterns) extend their
        // lifetime through every actual write — pre-fix the field collapsed
        // to last-read only and the packer would silently alias other
        // transients into the period between the first read and the final
        // write. For a write-only single-writer transient m_uLastUse ==
        // m_uFirstWrite, so the aliasing packer can compare lifetimes without
        // special-casing the "never read" case.
        u_int m_uFirstWrite = UINT32_MAX;
        u_int m_uLastUse    = UINT32_MAX;
        // Aliasing packer output (populated by AssignAliasingGroups). UINT32_MAX
        // when aliasing is disabled or the transient hasn't been packed yet.
        // When set, the transient shares its backing memory with other transients
        // assigned to the same pool index + binning group (see MakeTransientMemorySignature).
        u_int    m_uAliasPoolIndex = UINT32_MAX;
        u_int64  m_ulAliasOffset   = 0;
    };
    Zenith_Vector<TransientResource*> m_axTransients;

    // Aliasing pool state (populated by AssignAliasingGroups, consumed by
    // AllocateTransients when m_bAliasingEnabled is active). One entry per
    // pool; each pool hosts >=1 transient with matching signature and
    // non-overlapping lifetimes. The VRAM handle references a VMA allocation
    // that the per-transient VkImages get bound to at their assigned offsets.
    struct AliasPool
    {
        u_int64             m_ulSignature = 0;     // matches TransientResource signature
        u_int64             m_ulTotalSize = 0;     // max(offset + image_size) across occupants
        u_int64             m_ulMaxAlignment = 0;  // max image alignment across occupants (0 → probe failed; pool falls back to 64 KB)
        u_int               m_uMemoryFlags = 0;    // mirrors the signature's memory flags for diagnostics
        Flux_VRAMHandle     m_xPoolVRAM;           // backing VMA allocation
    };
    Zenith_Vector<AliasPool> m_axAliasPools;

    void AllocateTransients();
    void DestroyTransients();
    // Release the backing VRAM of every currently-allocated transient + every
    // aliasing pool, but keep the Flux_TransientHandle / m_axTransients vector
    // intact so subsystem-held handles stay valid. Called by AssignAliasingGroups
    // at the start of each re-Compile so a toggle of aliasing state (or any
    // other change that flips pool layout) doesn't leak the previous pool set.
    void ReleaseTransientAllocations();

    // Aliasing-pool packer phases — split out of AssignAliasingGroups so each
    // step is independently readable and testable. Run in order:
    //   sort → pack → size.
    // SortTransientsByLifetime: build sort-order over referenced transients,
    //   ascending by m_uFirstWrite (matches classic interval-coloring order).
    // PackTransientsIntoPools: greedy first-fit per memory signature; updates
    //   each transient's m_uAliasPoolIndex and pushes into m_axAliasPools.
    // ComputePoolSizes: per-pool size = max(occupant size); alignment = max
    //   per-image alignment. All occupants bind at offset 0 (lifetimes proven
    //   non-overlapping by the packer).
    void SortTransientsByLifetime(Zenith_Vector<u_int>& axSortedIndices) const;
    void PackTransientsIntoPools(const Zenith_Vector<u_int>& axSortedIndices);
    void ComputePoolSizes();

    Zenith_Vector<Flux_RenderGraph_Pass*> m_xPasses;
    Zenith_HashMap<void*, Flux_RenderGraph_Resource> m_xResources;
    Zenith_Vector<u_int> m_xExecutionOrder;
    bool m_bCompiled = false;
    bool m_bDirty = true;
    bool m_bEnabledMaskDirty = false;
    // Transient-aliasing toggle (see SetAliasingEnabled). Defaults to true;
    // the effective decision is (m_bAliasingEnabled && backend supports it)
    // evaluated inside Compile. The backend capability may be cached on the
    // first Compile to avoid querying every frame.
    bool m_bAliasingEnabled = true;
    // Per-frame list of buffers that were just host-written. Populated by
    // MarkBufferHostWritten (typically from a Prepare callback after an
    // UploadBufferDataAtOffset). Consumed by Execute, which re-runs
    // SynthesizeBarriers with these buffers' barrier-tracker state seeded
    // to RESOURCE_ACCESS_HOST_TRANSFER_WRITE so the next reader gets a
    // TransferWrite→ShaderRead barrier emitted in its prologue. Cleared
    // after each re-synth.
    Zenith_Vector<Flux_Buffer*> m_xHostWrittenBuffers;
    // Persistent set of buffers known to be externally-written (host
    // uploads, etc.). Populated alongside m_xHostWrittenBuffers by
    // MarkBufferHostWritten and never cleared until graph Clear(). The
    // orphaned-read validator skips these because their writes
    // legitimately happen outside the render graph — Read declarations on
    // them aren't a "missing producer" wiring bug.
    Zenith_Vector<Flux_Buffer*> m_xExternallyWrittenBuffers;
    // Incremented on every Clear(). Flux_PassHandle and Flux_TransientHandle
    // both carry this value; APIs that take a handle assert the handle's
    // generation matches the current graph generation so stale handles
    // (typically cached across a resize rebuild) trip immediately instead of
    // silently addressing a different pass / transient slot.
    u_int m_uGeneration = 1;
    // Per-graph monotonic ID, embedded into every issued handle. Distinguishes
    // handles from different graph instances (primarily a concern in unit
    // tests that instantiate multiple graphs, but also protects any future
    // path that holds two graphs live concurrently). 0 is the "no graph"
    // sentinel; s_uNextGraphInstanceID starts at 1 so the post-increment in
    // the in-class initialiser below always yields >= 1.
    static u_int s_uNextGraphInstanceID;
    u_int m_uGraphInstanceID = s_uNextGraphInstanceID++;
    // Build a pass handle for the pass most recently pushed to m_xPasses,
    // stamped with the current graph generation. Used by AddPass.
    Flux_PassHandle MakePassHandle(u_int uIndex) const;
    // Assert handle's index is in range AND its generation matches the current
    // graph generation. szCaller is the API method name for the error message.
    void AssertPassHandleValid(Flux_PassHandle xPass, const char* szCaller) const;
    void AssertTransientHandleValid(Flux_TransientHandle xHandle, const char* szCaller) const;

    struct ResourceTraffic { Zenith_Vector<u_int> m_xWriters; Zenith_Vector<u_int> m_xReaders; };
    Zenith_HashMap<void*, ResourceTraffic> m_xTraffic;
    // Clear tracking is keyed on Flux_BarrierKey so that a cubemap whose 42
    // (mip, face) subresources are each written by a different pass correctly
    // grants every pass "first writer" status for *its* subresource. A ptr-only
    // key would grant first-writer to only the first of the 42 passes and leave
    // the other 41 with bClearTargets=false, driving the backend to transition
    // SHADER_READ_ONLY_OPTIMAL → COLOR_ATTACHMENT_OPTIMAL on subresources that
    // were never written and are actually still in UNDEFINED layout — which
    // the Vulkan validator (correctly) rejects.
    Zenith_HashMap<Flux_BarrierKey, bool> m_xAttachmentNeedsClear;
    Zenith_HashSet<Flux_BarrierKey> m_xAttachmentClearAssigned;
    // Edge-set key is (fromPassIdx << 32) | toPassIdx — pass indices only, no
    // pointer bits, so u_int64 is safe and doesn't need the Flux_BarrierKey
    // treatment.
    Zenith_HashSet<u_int64> m_xEdgeSet;
    Zenith_Vector<Zenith_Vector<u_int>> m_xAdjacency;
    Zenith_Vector<u_int> m_xInDegree;
    Zenith_Vector<u_int> m_xQueue;

    Flux_RenderGraph_Pass* GetPass(u_int uIdx) const;
    void TrackResource(const Flux_GraphResource& xResource);
    void AddResourceUsage(u_int uPassIndex, const Flux_GraphResource& xResource, ResourceAccess eAccess,
                          u_int uMip, u_int uMipCount, u_int uLayer, u_int uLayerCount, bool bWrite);
    void BuildResourceTraffic();
    void Validate() const;
    void ValidatePassMemoryFlagCompatibility(const Flux_RenderGraph_Pass* pxP) const;
    // Validate phases — split out so the dispatcher reads as
    // orphaned-reads → unused-transients → per-pass loop. Each helper asserts
    // and logs on failure (no return value); they all run unconditionally.
    void ValidateOrphanedReads() const;
    void ValidateUnusedTransients() const;
    void ValidatePassBasics(const Flux_RenderGraph_Pass* pxP) const;
    void ValidatePassAttachmentCounts(const Flux_RenderGraph_Pass* pxP) const;
    // Walk m_xExecutionOrder, track per-(attachment, mip, layer) access state
    // for images and per-buffer access state for buffers, and synthesise
    // prologue Flux_RenderGraph_Barrier entries on each pass whose declared
    // accesses don't match the current state. Both image and buffer entries
    // share the same m_xPrologueBarriers list — the backend dispatches on
    // m_xResource kind (see Flux_RenderGraph_Barrier doc).
    void SynthesizeBarriers();
    bool TopologicalSort();
    void ComputeResourceLifetimes();
    // Cache per-transient [uFirstWrite, uLastRead] onto the TransientResource
    // struct so the aliasing packer can iterate transients directly. Must run
    // after ComputeResourceLifetimes has populated m_xResources.
    void ComputeTransientLifetimes();
    // Assign each transient to an "alias pool" — a group of transients with
    // compatible memory requirements (same signature) whose lifetimes do not
    // overlap pairwise. All transients in a pool share one VMA allocation;
    // the packer outputs (pool index, offset) cached on TransientResource.
    // No-op when m_bAliasingEnabled is false. Must run after
    // ComputeTransientLifetimes. Populates m_axAliasPools.
    void AssignAliasingGroups();

    // Inject a full-pipeline memory barrier on each transient's first-use
    // pass when that transient shares a pool slot with a prior-in-time
    // transient. The UNDEFINED → required-layout transition that
    // SynthesizeBarriers already emits handles per-image layout correctly
    // (separate VkImages are tracked independently by the driver), but does
    // NOT guarantee the previous aliased image's writes have flushed before
    // the new image's first access. The aliasing barrier closes that gap by
    // waiting on ALL prior GPU stages at the new image's first-use pass.
    // Conservative but cheap — only runs on aliased-slot transitions, which
    // naturally coincide with inter-pass boundaries that already have
    // synchronisation.
    void SynthesizeAliasingBarriers();
    void ResolveClearFlags();
    // MakeTransientMemorySignature is the public signature accessor (declared above).
    void AssertMutable(const char* szFn);
    void MarkPassesAsComputeOrGraphics();
    void CallPrepareCallbacks();
    void RecordCommandLists();
    void SubmitRecordedLists();
    void InitAdjacencyData();
    void BuildAdjacencyFromTraffic();
    void AddReaderWriterEdges(const Zenith_Vector<u_int>& axReaders, const Zenith_Vector<u_int>& axWriters);
    bool IsAlsoWriter(u_int uReader, const Zenith_Vector<u_int>& axWriters);
    u_int FindBestWriter(u_int uReader, const Zenith_Vector<u_int>& axWriters);
    void AddWriterChainEdges(const Zenith_Vector<u_int>& axWriters);
    void AddEdgeIfNew(u_int uFrom, u_int uTo);
    void AddExplicitDependencies();
    void InferPassAttachments();
    void CollectClearRequirements();
    void AssignClearFlags();

    friend void Flux_RenderGraph_RecordPassTask(void* pData, u_int uInvocationIndex, u_int uWorkerIndex);
    // Flux_PassBuilder reaches into m_xPasses + the private
    // AssertPassHandleValid helper in its SetUserData path. Scoped friendship
    // keeps the surface narrow.
    friend class Flux_PassBuilder;
};

namespace std { template<> struct hash<Flux_GraphResource> { size_t operator()(const Flux_GraphResource& xRes) const { return static_cast<size_t>(xRes.GetHash()); } }; }
