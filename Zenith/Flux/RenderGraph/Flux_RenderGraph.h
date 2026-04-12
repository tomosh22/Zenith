#pragma once

#include "Collections/Zenith_Vector.h"
#include "Flux/Flux.h" // Provides Flux_RenderAttachment, Flux_TargetSetup, Flux_SurfaceInfo, Flux_CommandList
#include <unordered_map> // #TODO: Replace with engine hash map
#include <unordered_set> // #TODO: Replace with engine hash set

// "OnRecord" runs on a worker thread inside a per-pass command list. It is the
// only callback allowed to call Flux_CommandList::AddCommand.
using Flux_RenderGraph_OnRecordFunc  = void(*)(Flux_CommandList*, void*);

// "OnPrepare" runs sequentially on the main thread before any parallel
// recording. Use it for memory uploads, scene queries, or pass enable/disable
// decisions that must be made before the recording phase begins.
using Flux_RenderGraph_OnPrepareFunc = void(*)(void*);

// Resources tracked by the graph can be either images (render attachments) or
// GPU buffers. The graph only needs identity (pointer equality) to build its
// dependency DAG, so we store the pointer type-erased via a small tagged union.
enum Flux_GraphResourceKind
{
	FLUX_GRAPH_RESOURCE_KIND__IMAGE,
	FLUX_GRAPH_RESOURCE_KIND__BUFFER,
};

struct Flux_RenderGraph_ResourceUsage
{
	void* m_pResource = nullptr;               // Flux_RenderAttachment* or Flux_Buffer*
	Flux_GraphResourceKind m_eKind = FLUX_GRAPH_RESOURCE_KIND__IMAGE;
	ResourceAccess m_eAccess = RESOURCE_ACCESS_READ_SRV;
	u_int m_uMipLevel = 0;
	u_int m_uMipCount = 1;
};

struct Flux_RenderGraph_Resource
{
	void* m_pResource = nullptr;
	Flux_GraphResourceKind m_eKind = FLUX_GRAPH_RESOURCE_KIND__IMAGE;
	const char* m_szName = nullptr;
	u_int m_uFirstWrite = UINT32_MAX;   // Execution index of first write
	u_int m_uLastRead = UINT32_MAX;     // Execution index of last read
};

// Per-pass barrier record computed during Compile() and consumed by the
// platform layer at record time. One entry per image attachment that needs a
// layout/access transition before the pass begins.
struct Flux_RenderGraph_ImageBarrier
{
	Flux_RenderAttachment* m_pxAttachment = nullptr;
	ResourceAccess m_ePrevAccess = RESOURCE_ACCESS_UNDEFINED;
	ResourceAccess m_eNewAccess = RESOURCE_ACCESS_READ_SRV;
	bool m_bDiscard = false;   // True when old contents can be discarded (first writer or explicit clear)
};

struct Flux_RenderGraph_Pass
{
	const char* m_szName = nullptr;
	Zenith_Vector<Flux_RenderGraph_ResourceUsage> m_xReads;
	Zenith_Vector<Flux_RenderGraph_ResourceUsage> m_xWrites;
	Zenith_Vector<u_int> m_xExplicitDependencies;   // Direct pass->pass edges (no resource)
	Flux_RenderGraph_OnRecordFunc m_pfnOnRecord = nullptr;
	Flux_RenderGraph_OnPrepareFunc m_pfnOnPrepare = nullptr;
	void* m_pUserData = nullptr;
	u_int m_uTopologicalOrder = UINT32_MAX;
	u_int m_uLevel = 0;           // Topological level (depth from a source). Execute() batches by level.
	bool m_bIsCompute = false;
	bool m_bEnabled = true;
	// Caller-set "I want the target cleared when my pass runs" intent.
	// Compile() resolves these to the actual per-pass m_bClearTargets flag below.
	bool m_bRequestsClear = false;
	// Resolved by Compile() (and re-resolved by Execute when the dirty flag was
	// set by SetPassEnabled). Reflects which pass actually performs the clear
	// for a shared target setup once disabled passes are factored in.
	bool m_bClearTargets = false;
	const Flux_TargetSetup* m_pxTargetSetup = nullptr; // non-owning
	Zenith_Vector<Flux_RenderGraph_ImageBarrier> m_xPrologueBarriers;
	Zenith_Vector<Flux_RenderGraph_ImageBarrier> m_xEpilogueBarriers;
	// Heap-owned because Flux_CommandList is non-copyable and non-movable
	// (explicit-only construction). Allocated in AddPass, deleted in the
	// destructor when Flux_RenderGraph::Clear() calls delete on the pass.
	Flux_CommandList* m_pxCommandList = nullptr;

	~Flux_RenderGraph_Pass()
	{
		delete m_pxCommandList;
		m_pxCommandList = nullptr;
	}
};

class Flux_RenderGraph
{
public:
	Flux_RenderGraph() = default;
	~Flux_RenderGraph() { Clear(); }

	// Add a pass. The name pointer must outlive the graph (string literal).
	// uInitialCapacity controls the per-pass command list initial allocation —
	// heavy passes (e.g. static meshes) should pass a larger value to avoid
	// the cold-start grow spike.
	u_int AddPass(const char* szName, Flux_RenderGraph_OnRecordFunc pfnOnRecord,
		void* pUserData = nullptr, u_int uInitialCapacity = 4096);

	// Image reads/writes (identity by Flux_RenderAttachment*)
	void PassReads(u_int uPassIndex, Flux_RenderAttachment* pxAttachment,
		ResourceAccess eAccess = RESOURCE_ACCESS_READ_SRV, u_int uMip = 0, u_int uMipCount = 1);
	void PassWrites(u_int uPassIndex, Flux_RenderAttachment* pxAttachment,
		ResourceAccess eAccess = RESOURCE_ACCESS_WRITE_RTV, u_int uMip = 0, u_int uMipCount = 1);

	// Buffer reads/writes (identity by Flux_Buffer*)
	void PassReadsBuffer(u_int uPassIndex, Flux_Buffer* pxBuffer,
		ResourceAccess eAccess = RESOURCE_ACCESS_READ_SRV);
	void PassWritesBuffer(u_int uPassIndex, Flux_Buffer* pxBuffer,
		ResourceAccess eAccess = RESOURCE_ACCESS_WRITE_UAV);

	// Direct pass->pass dependency not tied to a resource. Use for CPU-side
	// sequencing or when the two passes touch a resource the graph doesn't track.
	void AddPassDependency(u_int uDependentPass, u_int uDependencyPass);

	void SetPassEnabled(u_int uPassIndex, bool bEnabled);
	void SetPassTargetSetup(u_int uPassIndex, const Flux_TargetSetup& xTargetSetup);
	void SetPassOnPrepare(u_int uPassIndex, Flux_RenderGraph_OnPrepareFunc pfnOnPrepare);
	// Each subsystem owns the decision of whether its pass wants a cleared or
	// loaded target on entry. Default is false (load).
	void SetPassClearTargets(u_int uPassIndex, bool bClearTargets);

	bool Compile();
	void Execute();
	void MarkDirty();
	bool IsDirty() const { return m_bDirty; }

	void Clear();

	const Zenith_Vector<Flux_RenderGraph_Pass*>& GetPasses() const { return m_xPasses; }
	const Zenith_Vector<u_int>& GetExecutionOrder() const { return m_xExecutionOrder; }
	const Zenith_Vector<u_int>& GetLevelStarts() const { return m_xLevelStarts; }

private:
	// Pointer-stable storage: passes are heap-allocated so worker threads can
	// hold references into command lists across the recording phase without
	// any risk of vector growth invalidating them. Pass count per graph is
	// small (~50) so the per-pass new/delete is negligible compared to the
	// stability guarantee it provides.
	Zenith_Vector<Flux_RenderGraph_Pass*> m_xPasses;
	std::unordered_map<void*, Flux_RenderGraph_Resource> m_xResources; // #TODO: Replace with engine hash map
	Zenith_Vector<u_int> m_xExecutionOrder;
	Zenith_Vector<u_int> m_xLevelStarts; // Indices into m_xExecutionOrder at which each topo level begins
	bool m_bCompiled = false;
	bool m_bDirty = true;
	// Set when SetPassEnabled toggled an enable state since the last Compile —
	// triggers cheap clear-flag re-resolution at the start of Execute().
	bool m_bEnabledMaskDirty = false;

	// Persistent scratch state — kept as members so Compile() does not reallocate
	// hash tables every res change.
	struct ResourceTraffic
	{
		Zenith_Vector<u_int> m_xWriters;
		Zenith_Vector<u_int> m_xReaders;
	};
	std::unordered_map<void*, ResourceTraffic> m_xTraffic;
	std::unordered_map<Flux_RenderAttachment*, ResourceAccess> m_xBarrierState;
	std::unordered_map<const Flux_TargetSetup*, bool> m_xSetupNeedsClear;
	std::unordered_set<const Flux_TargetSetup*> m_xSetupClearAssigned;
	std::unordered_set<u_int64> m_xEdgeSet;
	Zenith_Vector<Zenith_Vector<u_int>> m_xAdjacency;
	Zenith_Vector<u_int> m_xInDegree;
	Zenith_Vector<u_int> m_xQueue;

	bool TopologicalSort();
	void ComputeResourceLifetimes();
	void GenerateBarriers();
	void Validate();
	void BuildResourceTraffic();
	void ResolveClearFlags();

	void EnsureResourceTracked(void* pResource, Flux_GraphResourceKind eKind, const char* szName);
	void AssertMutable(const char* szFn);

	// Worker thread access to mutable pass storage during command-list recording.
	friend void Flux_RenderGraph_RecordLevelTask(void* pData, u_int uInvocationIndex, u_int uWorkerIndex);
};
