#include "Zenith.h"

#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Flux/Flux_Graphics.h"
#include "Vulkan/Zenith_Vulkan.h"
#include "Vulkan/Zenith_Vulkan_MemoryManager.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#include "Multithreading/Zenith_Multithreading.h"

// ---------------------------------------------------------------------------
// Mutation guards
// ---------------------------------------------------------------------------
void Flux_RenderGraph::AssertMutable(const char* szFn)
{
	// It is legal to mutate the graph any time before Compile(), or after
	// MarkDirty() invalidated a prior compilation. After a successful Compile
	// the pass vector's layout is assumed stable by Execute().
	Zenith_Assert(!m_bCompiled || m_bDirty,
		"Flux_RenderGraph::%s called after Compile() without MarkDirty()", szFn);
	(void)szFn;
}

// ---------------------------------------------------------------------------
// Resource tracking
// ---------------------------------------------------------------------------
void Flux_RenderGraph::EnsureResourceTracked(void* pResource, Flux_GraphResourceKind eKind, const char* szName)
{
	auto it = m_xResources.find(pResource);
	if (it == m_xResources.end())
	{
		Flux_RenderGraph_Resource xResource;
		xResource.m_pResource = pResource;
		xResource.m_eKind = eKind;
		xResource.m_szName = szName;
		m_xResources[pResource] = xResource;
	}
}

// ---------------------------------------------------------------------------
// Pass construction
// ---------------------------------------------------------------------------
u_int Flux_RenderGraph::AddPass(const char* szName, Flux_RenderGraph_OnRecordFunc pfnOnRecord,
	void* pUserData, u_int uInitialCapacity)
{
	AssertMutable("AddPass");

	Flux_RenderGraph_Pass* pxPass = new Flux_RenderGraph_Pass();
	pxPass->m_szName = szName;
	pxPass->m_pfnOnRecord = pfnOnRecord;
	pxPass->m_pUserData = pUserData;
	// Pre-size the per-pass command list and set its name once at construction.
	// The recording phase no longer touches either field.
	pxPass->m_pxCommandList = new Flux_CommandList(szName, uInitialCapacity);
	m_xPasses.PushBack(pxPass);
	return m_xPasses.GetSize() - 1;
}

void Flux_RenderGraph::PassReads(u_int uPassIndex, Flux_RenderAttachment* pxAttachment,
	ResourceAccess eAccess, u_int uMip, u_int uMipCount)
{
	AssertMutable("PassReads");
	Zenith_Assert(uPassIndex < m_xPasses.GetSize(), "PassReads: Invalid pass index %u", uPassIndex);
	Zenith_Assert(pxAttachment != nullptr, "PassReads: Null attachment");

	EnsureResourceTracked(pxAttachment, FLUX_GRAPH_RESOURCE_KIND__IMAGE, "<image>");

	Flux_RenderGraph_ResourceUsage xUsage;
	xUsage.m_pResource = pxAttachment;
	xUsage.m_eKind = FLUX_GRAPH_RESOURCE_KIND__IMAGE;
	xUsage.m_eAccess = eAccess;
	xUsage.m_uMipLevel = uMip;
	xUsage.m_uMipCount = uMipCount;
	m_xPasses.Get(uPassIndex)->m_xReads.PushBack(xUsage);
}

void Flux_RenderGraph::PassWrites(u_int uPassIndex, Flux_RenderAttachment* pxAttachment,
	ResourceAccess eAccess, u_int uMip, u_int uMipCount)
{
	AssertMutable("PassWrites");
	Zenith_Assert(uPassIndex < m_xPasses.GetSize(), "PassWrites: Invalid pass index %u", uPassIndex);
	Zenith_Assert(pxAttachment != nullptr, "PassWrites: Null attachment");

	EnsureResourceTracked(pxAttachment, FLUX_GRAPH_RESOURCE_KIND__IMAGE, "<image>");

	Flux_RenderGraph_ResourceUsage xUsage;
	xUsage.m_pResource = pxAttachment;
	xUsage.m_eKind = FLUX_GRAPH_RESOURCE_KIND__IMAGE;
	xUsage.m_eAccess = eAccess;
	xUsage.m_uMipLevel = uMip;
	xUsage.m_uMipCount = uMipCount;
	m_xPasses.Get(uPassIndex)->m_xWrites.PushBack(xUsage);
}

void Flux_RenderGraph::PassReadsBuffer(u_int uPassIndex, Flux_Buffer* pxBuffer, ResourceAccess eAccess)
{
	AssertMutable("PassReadsBuffer");
	Zenith_Assert(uPassIndex < m_xPasses.GetSize(), "PassReadsBuffer: Invalid pass index %u", uPassIndex);
	Zenith_Assert(pxBuffer != nullptr, "PassReadsBuffer: Null buffer");

	EnsureResourceTracked(pxBuffer, FLUX_GRAPH_RESOURCE_KIND__BUFFER, "<buffer>");

	Flux_RenderGraph_ResourceUsage xUsage;
	xUsage.m_pResource = pxBuffer;
	xUsage.m_eKind = FLUX_GRAPH_RESOURCE_KIND__BUFFER;
	xUsage.m_eAccess = eAccess;
	m_xPasses.Get(uPassIndex)->m_xReads.PushBack(xUsage);
}

void Flux_RenderGraph::PassWritesBuffer(u_int uPassIndex, Flux_Buffer* pxBuffer, ResourceAccess eAccess)
{
	AssertMutable("PassWritesBuffer");
	Zenith_Assert(uPassIndex < m_xPasses.GetSize(), "PassWritesBuffer: Invalid pass index %u", uPassIndex);
	Zenith_Assert(pxBuffer != nullptr, "PassWritesBuffer: Null buffer");

	EnsureResourceTracked(pxBuffer, FLUX_GRAPH_RESOURCE_KIND__BUFFER, "<buffer>");

	Flux_RenderGraph_ResourceUsage xUsage;
	xUsage.m_pResource = pxBuffer;
	xUsage.m_eKind = FLUX_GRAPH_RESOURCE_KIND__BUFFER;
	xUsage.m_eAccess = eAccess;
	m_xPasses.Get(uPassIndex)->m_xWrites.PushBack(xUsage);
}

void Flux_RenderGraph::AddPassDependency(u_int uDependentPass, u_int uDependencyPass)
{
	AssertMutable("AddPassDependency");
	Zenith_Assert(uDependentPass < m_xPasses.GetSize() && uDependencyPass < m_xPasses.GetSize(),
		"AddPassDependency: Invalid pass index");
	Zenith_Assert(uDependentPass != uDependencyPass, "AddPassDependency: self-loop");
	m_xPasses.Get(uDependentPass)->m_xExplicitDependencies.PushBack(uDependencyPass);
}

void Flux_RenderGraph::SetPassEnabled(u_int uPassIndex, bool bEnabled)
{
	Zenith_Assert(uPassIndex < m_xPasses.GetSize(), "SetPassEnabled: Invalid pass index %u", uPassIndex);
	if (m_xPasses.Get(uPassIndex)->m_bEnabled != bEnabled)
	{
		m_xPasses.Get(uPassIndex)->m_bEnabled = bEnabled;
		// Mark the enable mask dirty so the next Execute() re-resolves
		// per-target-setup clear ownership before recording. We deliberately do
		// NOT set m_bDirty (which would force a full Compile) — toggling at
		// runtime is allowed without rebuilding the topo order.
		m_bEnabledMaskDirty = true;
	}
}

void Flux_RenderGraph::SetPassTargetSetup(u_int uPassIndex, const Flux_TargetSetup& xTargetSetup)
{
	AssertMutable("SetPassTargetSetup");
	Zenith_Assert(uPassIndex < m_xPasses.GetSize(), "SetPassTargetSetup: Invalid pass index %u", uPassIndex);
	// Store pointer, not copy. Subsystems own the target-setup storage.
	m_xPasses.Get(uPassIndex)->m_pxTargetSetup = &xTargetSetup;
}

void Flux_RenderGraph::SetPassOnPrepare(u_int uPassIndex, Flux_RenderGraph_OnPrepareFunc pfnOnPrepare)
{
	AssertMutable("SetPassOnPrepare");
	Zenith_Assert(uPassIndex < m_xPasses.GetSize(), "SetPassOnPrepare: Invalid pass index %u", uPassIndex);
	m_xPasses.Get(uPassIndex)->m_pfnOnPrepare = pfnOnPrepare;
}

void Flux_RenderGraph::SetPassClearTargets(u_int uPassIndex, bool bClearTargets)
{
	AssertMutable("SetPassClearTargets");
	Zenith_Assert(uPassIndex < m_xPasses.GetSize(), "SetPassClearTargets: Invalid pass index %u", uPassIndex);
	m_xPasses.Get(uPassIndex)->m_bRequestsClear = bClearTargets;
}

void Flux_RenderGraph::MarkDirty()
{
	m_bDirty = true;
}

// ---------------------------------------------------------------------------
// Clear / reset
// ---------------------------------------------------------------------------
void Flux_RenderGraph::Clear()
{
	for (u_int i = 0; i < m_xPasses.GetSize(); i++)
	{
		delete m_xPasses.Get(i);
	}
	m_xPasses.Clear();
	m_xResources.clear();
	m_xTraffic.clear();
	m_xBarrierState.clear();
	m_xSetupNeedsClear.clear();
	m_xSetupClearAssigned.clear();
	m_xEdgeSet.clear();
	m_xAdjacency.Clear();
	m_xInDegree.Clear();
	m_xQueue.Clear();
	m_xExecutionOrder.Clear();
	m_xLevelStarts.Clear();
	m_bCompiled = false;
	m_bDirty = true;
	m_bEnabledMaskDirty = false;
}

// ---------------------------------------------------------------------------
// Resource traffic enumeration
// ---------------------------------------------------------------------------
void Flux_RenderGraph::BuildResourceTraffic()
{
	m_xTraffic.clear();
	const u_int uNumPasses = m_xPasses.GetSize();
	for (u_int uPass = 0; uPass < uNumPasses; uPass++)
	{
		if (!m_xPasses.Get(uPass)->m_bEnabled)
			continue;

		const Flux_RenderGraph_Pass& xPass = *m_xPasses.Get(uPass);

		for (u_int i = 0; i < xPass.m_xWrites.GetSize(); i++)
		{
			void* pResource = xPass.m_xWrites.Get(i).m_pResource;
			m_xTraffic[pResource].m_xWriters.PushBack(uPass);
		}

		for (u_int i = 0; i < xPass.m_xReads.GetSize(); i++)
		{
			void* pResource = xPass.m_xReads.Get(i).m_pResource;
			m_xTraffic[pResource].m_xReaders.PushBack(uPass);
		}
	}
}

// ---------------------------------------------------------------------------
// Validation — strict: any consistency violation aborts via assert.
// ---------------------------------------------------------------------------
void Flux_RenderGraph::Validate()
{
	for (auto& xPair : m_xTraffic)
	{
		const ResourceTraffic& xTraffic = xPair.second;
		if (xTraffic.m_xReaders.GetSize() > 0 && xTraffic.m_xWriters.GetSize() == 0)
		{
			auto itRes = m_xResources.find(xPair.first);
			const char* szName = (itRes != m_xResources.end()) ? itRes->second.m_szName : "<unknown>";
			Zenith_Assert(false,
				"RenderGraph: Resource '%s' is read but never written within the graph. "
				"Either declare a writer or remove the read.", szName);
		}
	}

	// Every graphics pass must declare at least one write.
	for (u_int uPass = 0; uPass < m_xPasses.GetSize(); uPass++)
	{
		const Flux_RenderGraph_Pass& xPass = *m_xPasses.Get(uPass);
		if (!xPass.m_bEnabled) continue;

		const bool bIsGraphicsPass = (xPass.m_pxTargetSetup != nullptr)
			&& (*xPass.m_pxTargetSetup != Flux_Graphics::s_xNullTargetSetup);
		if (bIsGraphicsPass)
		{
			Zenith_Assert(xPass.m_xWrites.GetSize() > 0,
				"RenderGraph: graphics pass '%s' declares no PassWrites — clear detection and "
				"barrier inference require at least one write declaration", xPass.m_szName);
		}
	}
}

// ---------------------------------------------------------------------------
// Topological sort (Kahn) — uses shared traffic map and removes silent
// writer fallback. Also carries explicit pass-dependency edges.
// ---------------------------------------------------------------------------
bool Flux_RenderGraph::TopologicalSort()
{
	const u_int uNumPasses = m_xPasses.GetSize();

	m_xAdjacency.Clear();
	m_xAdjacency.Reserve(uNumPasses);
	for (u_int i = 0; i < uNumPasses; i++)
		m_xAdjacency.EmplaceBack();

	m_xInDegree.Clear();
	m_xInDegree.Reserve(uNumPasses);
	for (u_int i = 0; i < uNumPasses; i++)
		m_xInDegree.PushBack(0);

	m_xEdgeSet.clear();

	auto TryAddEdge = [&](u_int uFrom, u_int uTo) -> bool
	{
		if (uFrom == uTo) return false;
		u_int64 ulKey = (static_cast<u_int64>(uFrom) << 32) | static_cast<u_int64>(uTo);
		if (m_xEdgeSet.find(ulKey) != m_xEdgeSet.end()) return false;
		m_xEdgeSet.insert(ulKey);
		m_xAdjacency.Get(uFrom).PushBack(uTo);
		m_xInDegree.Get(uTo)++;
		return true;
	};

	// Resource-derived edges. The writer list is small (almost always 1–3
	// entries) so the linear "find latest writer before this reader" search
	// is fine in practice; we mark the data as sorted by execution order to
	// keep that invariant explicit.
	for (auto& xPair : m_xTraffic)
	{
		const ResourceTraffic& xTraffic = xPair.second;
		const Zenith_Vector<u_int>& xWriters = xTraffic.m_xWriters;
		const Zenith_Vector<u_int>& xReaders = xTraffic.m_xReaders;

		// Pure readers: readers that do not also write this resource
		for (u_int r = 0; r < xReaders.GetSize(); r++)
		{
			u_int uReader = xReaders.Get(r);
			bool bAlsoWriter = false;
			for (u_int w = 0; w < xWriters.GetSize(); w++)
			{
				if (xWriters.Get(w) == uReader) { bAlsoWriter = true; break; }
			}
			if (bAlsoWriter) continue;

			// xWriters is populated in increasing pass-index order by
			// BuildResourceTraffic, so we can scan forward and stop at the
			// first writer that exceeds the reader index. The writer just
			// before that is the latest.
			u_int uBestWriter = UINT32_MAX;
			for (u_int w = 0; w < xWriters.GetSize(); w++)
			{
				u_int uWriter = xWriters.Get(w);
				if (uWriter >= uReader) break;
				uBestWriter = uWriter;
			}
			if (uBestWriter == UINT32_MAX)
				continue;

			TryAddEdge(uBestWriter, uReader);
		}

		// Sequential writer chain
		if (xWriters.GetSize() > 1)
		{
			for (u_int i = 0; i < xWriters.GetSize() - 1; i++)
			{
				TryAddEdge(xWriters.Get(i), xWriters.Get(i + 1));
			}
		}
	}

	// Explicit pass dependencies
	for (u_int uPass = 0; uPass < uNumPasses; uPass++)
	{
		const Flux_RenderGraph_Pass& xPass = *m_xPasses.Get(uPass);
		if (!xPass.m_bEnabled) continue;
		for (u_int i = 0; i < xPass.m_xExplicitDependencies.GetSize(); i++)
		{
			u_int uDep = xPass.m_xExplicitDependencies.Get(i);
			if (!m_xPasses.Get(uDep)->m_bEnabled) continue;
			TryAddEdge(uDep, uPass);
		}
	}

	m_xExecutionOrder.Clear();
	m_xQueue.Clear();
	for (u_int i = 0; i < uNumPasses; i++)
	{
		if (!m_xPasses.Get(i)->m_bEnabled) continue;
		if (m_xInDegree.Get(i) == 0) m_xQueue.PushBack(i);
	}

	u_int uProcessed = 0;
	u_int uQueueFront = 0;
	while (uQueueFront < m_xQueue.GetSize())
	{
		u_int uCurrent = m_xQueue.Get(uQueueFront++);
		uProcessed++;
		m_xExecutionOrder.PushBack(uCurrent);
		m_xPasses.Get(uCurrent)->m_uTopologicalOrder = m_xExecutionOrder.GetSize() - 1;

		for (u_int i = 0; i < m_xAdjacency.Get(uCurrent).GetSize(); i++)
		{
			u_int uNeighbor = m_xAdjacency.Get(uCurrent).Get(i);
			m_xInDegree.Get(uNeighbor)--;
			if (m_xInDegree.Get(uNeighbor) == 0)
				m_xQueue.PushBack(uNeighbor);
		}
	}

	u_int uEnabledCount = 0;
	for (u_int i = 0; i < uNumPasses; i++)
		if (m_xPasses.Get(i)->m_bEnabled) uEnabledCount++;

	if (uProcessed != uEnabledCount)
	{
		for (u_int i = 0; i < uNumPasses; i++)
		{
			if (!m_xPasses.Get(i)->m_bEnabled) continue;
			if (m_xInDegree.Get(i) > 0)
			{
				Zenith_Log(LOG_CATEGORY_RENDERER, "  STUCK pass %u '%s' (in-degree %u)",
					i, m_xPasses.Get(i)->m_szName, m_xInDegree.Get(i));
				for (u_int j = 0; j < uNumPasses; j++)
				{
					if (!m_xPasses.Get(j)->m_bEnabled) continue;
					for (u_int k = 0; k < m_xAdjacency.Get(j).GetSize(); k++)
					{
						if (m_xAdjacency.Get(j).Get(k) == i && m_xInDegree.Get(j) > 0)
						{
							Zenith_Log(LOG_CATEGORY_RENDERER, "    <- depends on STUCK pass %u '%s'",
								j, m_xPasses.Get(j)->m_szName);
						}
					}
				}
			}
		}
		Zenith_Assert(false, "RenderGraph: Cycle detected! Processed %u of %u enabled passes", uProcessed, uEnabledCount);
		return false;
	}

	// Compute per-pass "level": longest path length from any source.
	for (u_int i = 0; i < uNumPasses; i++)
		m_xPasses.Get(i)->m_uLevel = 0;

	for (u_int e = 0; e < m_xExecutionOrder.GetSize(); e++)
	{
		u_int uPassIdx = m_xExecutionOrder.Get(e);
		u_int uLevel = m_xPasses.Get(uPassIdx)->m_uLevel;
		for (u_int i = 0; i < m_xAdjacency.Get(uPassIdx).GetSize(); i++)
		{
			u_int uNeighbor = m_xAdjacency.Get(uPassIdx).Get(i);
			if (m_xPasses.Get(uNeighbor)->m_uLevel < uLevel + 1)
				m_xPasses.Get(uNeighbor)->m_uLevel = uLevel + 1;
		}
	}

	// Stable insertion-sort m_xExecutionOrder by level (ties preserve topo
	// order). Insertion sort is fine: pass count is small (<100) and the
	// inner-loop comparison is a single integer compare.
	for (u_int i = 1; i < m_xExecutionOrder.GetSize(); i++)
	{
		u_int uKey = m_xExecutionOrder.Get(i);
		u_int uKeyLevel = m_xPasses.Get(uKey)->m_uLevel;
		int j = static_cast<int>(i) - 1;
		while (j >= 0 && m_xPasses.Get(m_xExecutionOrder.Get(j))->m_uLevel > uKeyLevel)
		{
			m_xExecutionOrder.Get(j + 1) = m_xExecutionOrder.Get(j);
			j--;
		}
		m_xExecutionOrder.Get(j + 1) = uKey;
	}

	// Build level-start indices.
	m_xLevelStarts.Clear();
	if (m_xExecutionOrder.GetSize() > 0)
	{
		m_xLevelStarts.PushBack(0);
		u_int uPrevLevel = m_xPasses.Get(m_xExecutionOrder.Get(0))->m_uLevel;
		for (u_int i = 1; i < m_xExecutionOrder.GetSize(); i++)
		{
			u_int uLevel = m_xPasses.Get(m_xExecutionOrder.Get(i))->m_uLevel;
			if (uLevel != uPrevLevel)
			{
				m_xLevelStarts.PushBack(i);
				uPrevLevel = uLevel;
			}
		}
	}

	// Refresh per-pass topological order after the level-based sort.
	for (u_int e = 0; e < m_xExecutionOrder.GetSize(); e++)
	{
		m_xPasses.Get(m_xExecutionOrder.Get(e))->m_uTopologicalOrder = e;
	}

	return true;
}

void Flux_RenderGraph::ComputeResourceLifetimes()
{
	for (u_int uExecIdx = 0; uExecIdx < m_xExecutionOrder.GetSize(); uExecIdx++)
	{
		u_int uPassIdx = m_xExecutionOrder.Get(uExecIdx);
		const Flux_RenderGraph_Pass& xPass = *m_xPasses.Get(uPassIdx);

		for (u_int i = 0; i < xPass.m_xWrites.GetSize(); i++)
		{
			void* pResource = xPass.m_xWrites.Get(i).m_pResource;
			auto it = m_xResources.find(pResource);
			if (it != m_xResources.end())
			{
				if (uExecIdx < it->second.m_uFirstWrite)
					it->second.m_uFirstWrite = uExecIdx;
			}
		}

		for (u_int i = 0; i < xPass.m_xReads.GetSize(); i++)
		{
			void* pResource = xPass.m_xReads.Get(i).m_pResource;
			auto it = m_xResources.find(pResource);
			if (it != m_xResources.end())
			{
				if (it->second.m_uLastRead == UINT32_MAX || uExecIdx > it->second.m_uLastRead)
					it->second.m_uLastRead = uExecIdx;
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Barrier generation
//
// Walks the execution order and computes, for each pass, the set of image
// attachments that must be transitioned into a new layout before the pass
// begins. The platform layer consumes m_xPrologueBarriers and applies them via
// its existing barrier helpers instead of re-deriving them from the target
// setup each frame.
// ---------------------------------------------------------------------------
void Flux_RenderGraph::GenerateBarriers()
{
	// Tracks the "current" access state of each image attachment as the graph
	// progresses. Missing key = first touch (UNDEFINED layout, contents discardable).
	m_xBarrierState.clear();

	for (u_int uExecIdx = 0; uExecIdx < m_xExecutionOrder.GetSize(); uExecIdx++)
	{
		u_int uPassIdx = m_xExecutionOrder.Get(uExecIdx);
		Flux_RenderGraph_Pass& xPass = *m_xPasses.Get(uPassIdx);
		xPass.m_xPrologueBarriers.Clear();
		xPass.m_xEpilogueBarriers.Clear();

		// Reads → need shader-read layout on entry
		for (u_int i = 0; i < xPass.m_xReads.GetSize(); i++)
		{
			const Flux_RenderGraph_ResourceUsage& xUsage = xPass.m_xReads.Get(i);
			if (xUsage.m_eKind != FLUX_GRAPH_RESOURCE_KIND__IMAGE) continue;
			Flux_RenderAttachment* pxImg = static_cast<Flux_RenderAttachment*>(xUsage.m_pResource);

			auto it = m_xBarrierState.find(pxImg);
			const bool bFirstTouch = (it == m_xBarrierState.end());
			const ResourceAccess ePrev = bFirstTouch ? RESOURCE_ACCESS_UNDEFINED : it->second;

			if (bFirstTouch || ePrev != xUsage.m_eAccess)
			{
				Flux_RenderGraph_ImageBarrier xBarrier;
				xBarrier.m_pxAttachment = pxImg;
				xBarrier.m_ePrevAccess = ePrev;
				xBarrier.m_eNewAccess = xUsage.m_eAccess;
				xBarrier.m_bDiscard = bFirstTouch;
				xPass.m_xPrologueBarriers.PushBack(xBarrier);
				m_xBarrierState[pxImg] = xUsage.m_eAccess;
			}
		}

		// Writes → need the appropriate attachment layout on entry
		for (u_int i = 0; i < xPass.m_xWrites.GetSize(); i++)
		{
			const Flux_RenderGraph_ResourceUsage& xUsage = xPass.m_xWrites.Get(i);
			if (xUsage.m_eKind != FLUX_GRAPH_RESOURCE_KIND__IMAGE) continue;
			Flux_RenderAttachment* pxImg = static_cast<Flux_RenderAttachment*>(xUsage.m_pResource);

			auto it = m_xBarrierState.find(pxImg);
			const bool bFirstTouch = (it == m_xBarrierState.end());
			const ResourceAccess ePrev = bFirstTouch ? RESOURCE_ACCESS_UNDEFINED : it->second;

			if (bFirstTouch || ePrev != xUsage.m_eAccess)
			{
				Flux_RenderGraph_ImageBarrier xBarrier;
				xBarrier.m_pxAttachment = pxImg;
				xBarrier.m_ePrevAccess = ePrev;
				xBarrier.m_eNewAccess = xUsage.m_eAccess;
				// Always discard the previous contents on the very first write
				// to a resource within this graph — the prior contents are
				// either uninitialised or stale and the pass is overwriting them.
				xBarrier.m_bDiscard = bFirstTouch || xPass.m_bClearTargets;
				xPass.m_xPrologueBarriers.PushBack(xBarrier);
			}
			m_xBarrierState[pxImg] = xUsage.m_eAccess;
		}
	}
}

// ---------------------------------------------------------------------------
// Clear-flag resolution. Pulled out of Compile() so it can be re-run from
// Execute() when SetPassEnabled toggled an enable since the last compile.
// ---------------------------------------------------------------------------
void Flux_RenderGraph::ResolveClearFlags()
{
	for (u_int i = 0; i < m_xPasses.GetSize(); i++)
	{
		m_xPasses.Get(i)->m_bClearTargets = false;
	}

	m_xSetupNeedsClear.clear();
	for (u_int e = 0; e < m_xExecutionOrder.GetSize(); e++)
	{
		Flux_RenderGraph_Pass& xPass = *m_xPasses.Get(m_xExecutionOrder.Get(e));
		if (xPass.m_bIsCompute || !xPass.m_bEnabled) continue;
		const Flux_TargetSetup* pxSetup = xPass.m_pxTargetSetup;
		if (pxSetup == nullptr) continue;

		auto it = m_xSetupNeedsClear.find(pxSetup);
		if (it == m_xSetupNeedsClear.end())
			m_xSetupNeedsClear[pxSetup] = xPass.m_bRequestsClear;
		else
			it->second = it->second || xPass.m_bRequestsClear;
	}

	m_xSetupClearAssigned.clear();
	for (u_int e = 0; e < m_xExecutionOrder.GetSize(); e++)
	{
		Flux_RenderGraph_Pass& xPass = *m_xPasses.Get(m_xExecutionOrder.Get(e));
		if (xPass.m_bIsCompute || !xPass.m_bEnabled) continue;
		const Flux_TargetSetup* pxSetup = xPass.m_pxTargetSetup;
		if (pxSetup == nullptr) continue;

		auto itNeeds = m_xSetupNeedsClear.find(pxSetup);
		if (itNeeds == m_xSetupNeedsClear.end() || !itNeeds->second) continue;

		if (m_xSetupClearAssigned.find(pxSetup) == m_xSetupClearAssigned.end())
		{
			xPass.m_bClearTargets = true;
			m_xSetupClearAssigned.insert(pxSetup);
		}
	}
}

// ---------------------------------------------------------------------------
// Compile
// ---------------------------------------------------------------------------
bool Flux_RenderGraph::Compile()
{
	if (!m_bDirty)
		return m_bCompiled;

	if (m_xPasses.GetSize() == 0)
	{
		m_bCompiled = true;
		m_bDirty = false;
		m_bEnabledMaskDirty = false;
		return true;
	}

	BuildResourceTraffic();

	Validate();

	if (!TopologicalSort())
		return false;

	ComputeResourceLifetimes();

	// Determine compute/graphics from target-setup pointer.
	// Passes whose target setup is null or equals s_xNullTargetSetup are compute.
	for (u_int i = 0; i < m_xPasses.GetSize(); i++)
	{
		Flux_RenderGraph_Pass& xPass = *m_xPasses.Get(i);
		xPass.m_bIsCompute = (xPass.m_pxTargetSetup == nullptr)
			|| (*xPass.m_pxTargetSetup == Flux_Graphics::s_xNullTargetSetup);
	}

	// Resolve explicit clear requests into per-pass clear flags. Each subsystem
	// calls SetPassClearTargets(true) on whichever pass wants the target cleared
	// before its work runs. Several passes may share the same target setup
	// (Skybox + StaticMeshes + …, both bloom down- and up-sample into the same
	// mip setup, etc.), and any of them may be runtime-disabled. The correct
	// behaviour is:
	//
	//   For each target setup, if ANY enabled pass sharing it requested a
	//   clear, the FIRST such pass in execution order performs the clear.
	//   All other passes sharing the setup load its contents.
	//
	// If the originally-chosen pass is later runtime-disabled via
	// SetPassEnabled, Execute() will re-run this resolver before the next
	// frame so the clear responsibility shifts to the next enabled pass.
	ResolveClearFlags();

	GenerateBarriers();

	m_bCompiled = true;
	m_bDirty = false;
	m_bEnabledMaskDirty = false;

	// Verbose by intent — Compile() runs on every res change.
#ifdef ZENITH_DEBUG
	Zenith_Log(LOG_CATEGORY_RENDERER, "RenderGraph: Compiled %u passes", m_xExecutionOrder.GetSize());
#endif

	return true;
}

// ---------------------------------------------------------------------------
// Execute
// ---------------------------------------------------------------------------
struct Flux_RenderGraph_RecordTaskData
{
	Flux_RenderGraph* m_pxGraph;
	u_int m_uLevelBatchStart;
	u_int m_uLevelBatchEnd;
};

void Flux_RenderGraph_RecordLevelTask(void* pData, u_int uInvocationIndex, u_int)
{
	Flux_RenderGraph_RecordTaskData* pxData = static_cast<Flux_RenderGraph_RecordTaskData*>(pData);
	Flux_RenderGraph* pxGraph = pxData[uInvocationIndex].m_pxGraph;
	const u_int uStart = pxData[uInvocationIndex].m_uLevelBatchStart;
	const u_int uEnd = pxData[uInvocationIndex].m_uLevelBatchEnd;

	const Zenith_Vector<u_int>& xExecutionOrder = pxGraph->GetExecutionOrder();
	const Zenith_Vector<Flux_RenderGraph_Pass*>& xPasses = pxGraph->GetPasses();

	for (u_int i = uStart; i < uEnd; i++)
	{
		u_int uPassIdx = xExecutionOrder.Get(i);
		Flux_RenderGraph_Pass& xPass = *xPasses.Get(uPassIdx);

		// Always reset the command list, even for disabled/skipped passes,
		// so that Phase 2's GetCommandCount() == 0 guard correctly drops
		// them instead of submitting stale commands from a previous frame.
		xPass.m_pxCommandList->Reset();

		if (!xPass.m_bEnabled) continue;
		if (xPass.m_pfnOnRecord == nullptr) continue;

		xPass.m_pfnOnRecord(xPass.m_pxCommandList, xPass.m_pUserData);
	}
}

void Flux_RenderGraph::Execute()
{
	Zenith_Assert(m_bCompiled, "RenderGraph: Must compile before executing");

	if (m_xExecutionOrder.GetSize() == 0)
		return;

	// If any SetPassEnabled call between the last Compile() and now changed an
	// enable state, re-resolve which pass owns the clear for each target
	// setup. This is cheap and avoids forcing a full topological recompile.
	if (m_bEnabledMaskDirty)
	{
		ResolveClearFlags();
		m_bEnabledMaskDirty = false;
	}

	// Phase 0: Sequential prepare on main thread.
	//
	// NOTE: ImGui rendering is intentionally issued by Zenith_Core BEFORE the
	// graph executes, so editor UI callbacks (scene load/save/unload) can
	// safely call SceneManager on the main thread. Any OnPrepare callback
	// that interacts with the scene must follow the same rule.
	for (u_int uExecIdx = 0; uExecIdx < m_xExecutionOrder.GetSize(); uExecIdx++)
	{
		u_int uPassIdx = m_xExecutionOrder.Get(uExecIdx);
		Flux_RenderGraph_Pass& xPass = *m_xPasses.Get(uPassIdx);
		if (!xPass.m_bEnabled || xPass.m_pfnOnPrepare == nullptr) continue;
		xPass.m_pfnOnPrepare(xPass.m_pUserData);
	}

	// Phase 1: Parallel command-list recording, batched by topological level.
	// One task invocation per level; each invocation records every pass at that level.
	const u_int uNumLevels = m_xLevelStarts.GetSize();
	if (uNumLevels > 0)
	{
		// Heap-allocate the per-level task data: stack arrays bounded by an
		// arbitrary cap are a footgun once the DAG depth grows.
		Flux_RenderGraph_RecordTaskData* pxTaskData =
			static_cast<Flux_RenderGraph_RecordTaskData*>(
				Zenith_MemoryManagement::Allocate(sizeof(Flux_RenderGraph_RecordTaskData) * uNumLevels));

		for (u_int uLevel = 0; uLevel < uNumLevels; uLevel++)
		{
			pxTaskData[uLevel].m_pxGraph = this;
			pxTaskData[uLevel].m_uLevelBatchStart = m_xLevelStarts.Get(uLevel);
			pxTaskData[uLevel].m_uLevelBatchEnd = (uLevel + 1 < uNumLevels)
				? m_xLevelStarts.Get(uLevel + 1)
				: m_xExecutionOrder.GetSize();
		}

		Zenith_TaskArray xRecordingTasks(
			ZENITH_PROFILE_INDEX__FLUX_RECORD_COMMAND_BUFFERS,
			Flux_RenderGraph_RecordLevelTask,
			pxTaskData,
			uNumLevels,
			true);

		Zenith_TaskSystem::SubmitTaskArray(&xRecordingTasks);
		xRecordingTasks.WaitUntilComplete();

		Zenith_MemoryManagement::Deallocate(pxTaskData);
	}

	// Phase 2: Submit command lists in topological order. Sequential on main thread.
	for (u_int uExecIdx = 0; uExecIdx < m_xExecutionOrder.GetSize(); uExecIdx++)
	{
		u_int uPassIdx = m_xExecutionOrder.Get(uExecIdx);
		Flux_RenderGraph_Pass& xPass = *m_xPasses.Get(uPassIdx);
		if (!xPass.m_bEnabled || xPass.m_pfnOnRecord == nullptr) continue;

		// Skip empty command lists ONLY when they have no work and no clear.
		// A graphics pass with bClearTargets=true but zero commands still needs
		// to be submitted so the Vulkan layer Begins the render pass and
		// performs the clear — this is how subsystems with conditionally-empty
		// execute callbacks (e.g. Skybox aerial perspective when atmosphere is
		// disabled) still guarantee their target is in a known state.
		if (xPass.m_pxCommandList->GetCommandCount() == 0 && !xPass.m_bClearTargets)
			continue;

		const Flux_TargetSetup& xTargetSetup = (xPass.m_pxTargetSetup != nullptr)
			? *xPass.m_pxTargetSetup
			: Flux_Graphics::s_xNullTargetSetup;

		// If this pass's target-setup depth attachment is declared as a READ
		// in the pass's reads list (i.e. sampled as SRV or bound as read-only
		// depth), the render pass initial/finalLayout must be
		// DEPTH_STENCIL_READ_ONLY_OPTIMAL to match the layout the previous
		// producer transitioned it to. Compute this inline from the pass's
		// declarations — the staged render graph doesn't store it in the Pass
		// struct. Fog/SSAO/SSR/etc. all sample depth this way.
		bool bDepthIsReadOnly = false;
		if (xTargetSetup.m_pxDepthStencil != nullptr)
		{
			void* pxDepthResource = static_cast<void*>(xTargetSetup.m_pxDepthStencil);
			for (u_int r = 0; r < xPass.m_xReads.GetSize(); r++)
			{
				if (xPass.m_xReads.Get(r).m_pResource == pxDepthResource)
				{
					bDepthIsReadOnly = true;
					break;
				}
			}
		}

		Flux::SubmitCommandList(xPass.m_pxCommandList, xTargetSetup, xPass.m_bClearTargets, bDepthIsReadOnly, &xPass);
	}
}
