#include "Zenith.h"

#include "Flux/Flux_RendererImpl.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_RenderTargets.h"
#include "TaskSystem/Zenith_TaskSystem.h"

//==========================================================================
// Flux_RenderGraph — execution / command-list recording / recording context
//
// Carved out of Flux_RenderGraph.cpp. The whole recording-context subsystem
// moves together: the thread-local current-pass/current-graph pointers, the
// CurrentPassScope/CurrentGraphScope RAII helpers, the per-pass recording
// task, and the Execute→RecordCommandLists→SubmitRecordedLists pipeline.
// Keeps the tls_ state co-located with everyone who reads or writes it so
// no cross-TU extern declarations are needed.
//==========================================================================

struct Flux_RenderGraph_RecordTaskData { Flux_RenderGraph* m_pxGraph; u_int m_uPassIndex; };

// ---- Current-pass / current-graph thread-local recording context -------
// Set around each pfnOnRecord callback so Flux_ShaderBinder can cross-reference
// bound VRAM handles against the current pass's declared Read/Write sets.
static thread_local const Flux_RenderGraph_Pass* tls_pxCurrentRecordingPass = nullptr;
static thread_local const Flux_RenderGraph* tls_pxCurrentRecordingGraph = nullptr;

namespace
{
	class CurrentGraphScope
	{
	public:
		CurrentGraphScope(const Flux_RenderGraph* pxGraph) : m_pxPrev(tls_pxCurrentRecordingGraph)
		{
			tls_pxCurrentRecordingGraph = pxGraph;
		}
		~CurrentGraphScope()
		{
			tls_pxCurrentRecordingGraph = m_pxPrev;
		}
	private:
		const Flux_RenderGraph* m_pxPrev;
	};
}

void Flux_RenderGraph_RecordPassTask(void* pData, u_int uInvocationIndex, u_int uWorkerIndex)
{
	auto* pxData = static_cast<Flux_RenderGraph_RecordTaskData*>(pData);
	Flux_RenderGraph* pxGraph = pxData[uInvocationIndex].m_pxGraph;
	const u_int uPassIndex = pxData[uInvocationIndex].m_uPassIndex;
	(void)uWorkerIndex;
	const Zenith_Vector<u_int>& xExecOrder = pxGraph->GetExecutionOrder();
	const Zenith_Vector<Flux_RenderGraph_Pass*>& xPasses = pxGraph->GetPasses();
	Flux_RenderGraph_Pass& xPass = *xPasses.Get(xExecOrder.Get(uPassIndex));
	xPass.m_pxCommandList->Reset();
	if (!xPass.m_bEnabled || !xPass.m_pfnOnRecord) return;

	// Set the thread-local current-pass and current-graph pointers so
	// Flux_ShaderBinder bind-time assertions can cross-reference against this
	// pass's declared Read/Write sets and skip non-tracked static assets.
	// Scopes restore prior values on exit even if the callback throws.
	Flux_RenderGraph::CurrentPassScope xPassScope(&xPass);
	CurrentGraphScope xGraphScope(pxGraph);

	// Prologue barriers from SynthesizeBarriers are NOT injected into the
	// command list — that path runs INSIDE the render pass for graphics
	// passes (vkCmdBeginRenderPass already issued by the backend before
	// IterateCommands), where vkCmdPipelineBarrier needs subpass self-deps.
	// The backend reads xPass.m_xPrologueBarriers directly and emits them
	// right before TransitionTargetsForRenderPass / Dispatch entry, outside
	// any active render pass. See Zenith_Vulkan.cpp::RecordCommandBuffersTask.
	xPass.m_pfnOnRecord(xPass.m_pxCommandList, xPass.m_pUserData);
}

void Flux_RenderGraph::Execute()
{
	Zenith_Assert(m_bCompiled, "Flux_RenderGraph::Execute: must call Compile() first");
	Zenith_Assert(!m_bDirty, "Flux_RenderGraph::Execute: graph is dirty — AddPass/Read/Write was called after Compile(). Call Compile() again.");
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "Flux_RenderGraph::Execute: must be called from main thread");
	if (m_xExecutionOrder.GetSize() == 0) return;

	// Every enabled pass in the execution order must have a valid command list —
	// a null list means the pass was destroyed without being removed from the
	// exec order, which corrupts downstream parallel recording.
	for (Zenith_Vector<u_int>::Iterator it(m_xExecutionOrder); !it.Done(); it.Next())
	{
		const Flux_RenderGraph_Pass* pxP = m_xPasses.Get(it.GetData());
		Zenith_Assert(pxP != nullptr,
			"Flux_RenderGraph::Execute: null pass at execution order index %u", it.GetData());
		if (!pxP->m_bEnabled) continue;
		Zenith_Assert(pxP->m_pxCommandList != nullptr,
			"Flux_RenderGraph::Execute: enabled pass '%s' has null command list", pxP->DebugName());
		Zenith_Assert(pxP->m_pfnOnRecord != nullptr,
			"Flux_RenderGraph::Execute: enabled pass '%s' has null record callback", pxP->DebugName());
	}

	if (m_bEnabledMaskDirty)
	{
		// SetEnabled flipped at least one pass's enable bit since last Execute.
		// The cheap path (no full recompile) re-resolves clear ownership AND
		// resynthesises barriers — both depend on which passes are actually
		// running this frame. Without the barrier re-run, a pass that was
		// enabled last compile but disabled now leaves the graph's compile-
		// time state tracker out of sync with the actual GPU layout, and
		// downstream consumers' barriers transition from a layout the resource
		// is no longer in (sync-validator layout-mismatch error).
		//
		// Recompute resource/transient lifetimes too — SynthesizeAliasingBarriers
		// reads pxPrior->m_uLastUse to look up the prior pool occupant's last
		// access (line ~1199). If a transient's last access was at a now-
		// disabled pass, the compile-time lifetime points there and the barrier's
		// src access would reflect what the disabled pass would have done, not
		// the actual GPU state. Pool assignments stay from compile (lifetimes
		// can only shrink with disabled passes, never extend, so prior alias-
		// non-overlap decisions remain valid).
		ComputeResourceLifetimes();
		ComputeTransientLifetimes();
		ResolveClearFlags();
		SynthesizeBarriers();
		SynthesizeAliasingBarriers();
		m_bEnabledMaskDirty = false;
	}
	CallPrepareCallbacks();
	// Prepare callbacks may have called MarkBufferHostWritten (for buffers
	// host-uploaded this frame). If so, re-run SynthesizeBarriers so the next
	// reader of each marked buffer gets a TransferWrite→ShaderRead barrier.
	// The host-written list is consumed inside SynthesizeBarriers — it stays
	// empty across frames where no host upload happens, so this is a no-op
	// in the common case.
	if (m_xHostWrittenBuffers.GetSize() > 0)
	{
		SynthesizeBarriers();
	}
	RecordCommandLists();
	SubmitRecordedLists();
}

void Flux_RenderGraph::CallPrepareCallbacks()
{
	for (Zenith_Vector<u_int>::Iterator it(m_xExecutionOrder); !it.Done(); it.Next())
	{
		Flux_RenderGraph_Pass* pxPass = m_xPasses.Get(it.GetData());
		if (pxPass->m_bEnabled && pxPass->m_pfnOnPrepare) pxPass->m_pfnOnPrepare(pxPass->m_pUserData);
	}
}

void Flux_RenderGraph::RecordCommandLists()
{
	const u_int uNumPasses = m_xExecutionOrder.GetSize();
	if (uNumPasses == 0) return;

	auto* pxTaskData = static_cast<Flux_RenderGraph_RecordTaskData*>(Zenith_MemoryManagement::Allocate(sizeof(Flux_RenderGraph_RecordTaskData) * uNumPasses));
	for (u_int i = 0; i < uNumPasses; i++)
	{
		pxTaskData[i].m_pxGraph = this;
		pxTaskData[i].m_uPassIndex = i;
	}

	Zenith_TaskArray xTasks(ZENITH_PROFILE_INDEX__FLUX_RECORD_COMMAND_BUFFERS, Flux_RenderGraph_RecordPassTask, pxTaskData, uNumPasses, true);
	g_xEngine.Tasks().SubmitTaskArray(&xTasks);
	xTasks.WaitUntilComplete();
	Zenith_MemoryManagement::Deallocate(pxTaskData);
}

void Flux_RenderGraph::SubmitRecordedLists()
{
	for (Zenith_Vector<u_int>::Iterator it(m_xExecutionOrder); !it.Done(); it.Next())
	{
		Flux_RenderGraph_Pass* pxPass = m_xPasses.Get(it.GetData());
		if (!pxPass->m_bEnabled || !pxPass->m_pfnOnRecord) continue;
		// A pass with zero recorded commands is normally pruned, but if it has
		// graph-synthesised prologue barriers (e.g. the "Final RT Layout
		// Transition" no-op pass that exists purely to flip the swapchain
		// source target into SHADER_READ_ONLY) we MUST submit it so the
		// backend gets a chance to emit those barriers. Same goes for clear-only
		// passes that need their target zeroed.
		const bool bHasBarriers = pxPass->m_xPrologueBarriers.GetSize() > 0;
		if (pxPass->m_pxCommandList->GetCommandCount() == 0 && !pxPass->m_bClearTargets && !bHasBarriers) continue;
		bool bDepthReadOnly = false;
		if (pxPass->m_xDepthStencil.IsValid())
		{
			void* pDepthRes = pxPass->m_xDepthStencil.m_xResource.GetVoidPtr();
			for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator itR(pxPass->m_xReads); !itR.Done(); itR.Next())
			{
				if (itR.GetData().m_xResource.GetVoidPtr() == pDepthRes) { bDepthReadOnly = true; break; }
			}
		}
		g_xEngine.FluxRenderer().SubmitCommandList(pxPass->m_pxCommandList,
			pxPass->m_axColourAttachments, pxPass->m_uNumColourAttachments,
			pxPass->m_xDepthStencil,
			pxPass->m_bClearTargets, bDepthReadOnly, pxPass);
	}
}

void Flux_RenderGraph::InferPassAttachments()
{
	for (u_int i = 0; i < m_xPasses.GetSize(); i++)
	{
		Flux_RenderGraph_Pass* pxPass = m_xPasses.Get(i);
		pxPass->m_uNumColourAttachments = 0;
		pxPass->m_xDepthStencil = Flux_RenderGraph_AttachmentRef();
		for (uint32_t t = 0; t < FLUX_MAX_TARGETS; t++)
			pxPass->m_axColourAttachments[t] = Flux_RenderGraph_AttachmentRef();
		for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(pxPass->m_xWrites); !it.Done(); it.Next())
		{
			const Flux_RenderGraph_ResourceUsage& rxUsage = it.GetData();
			if (!rxUsage.m_xResource.IsImageLike()) continue;
			if (rxUsage.m_eAccess == RESOURCE_ACCESS_WRITE_RTV)
			{
				if (pxPass->m_uNumColourAttachments < FLUX_MAX_TARGETS)
				{
					pxPass->m_axColourAttachments[pxPass->m_uNumColourAttachments++] =
						Flux_RenderGraph_AttachmentRef(rxUsage.m_xResource, rxUsage.m_uMipLevel, rxUsage.m_uLayer);
				}
			}
			else if (rxUsage.m_eAccess == RESOURCE_ACCESS_WRITE_DSV)
			{
				// Depth attachments are never cubemaps in this engine — they always
				// bind as a 2D depth/stencil view. Assert to catch accidental misuse.
				Zenith_Assert(rxUsage.m_xResource.GetKind() == Flux_GraphResourceKind::Image,
					"Flux_RenderGraph: depth/stencil writes require a 2D Flux_RenderAttachment (pass '%s')", pxPass->DebugName());
				pxPass->m_xDepthStencil =
					Flux_RenderGraph_AttachmentRef(rxUsage.m_xResource, rxUsage.m_uMipLevel, rxUsage.m_uLayer);
			}
		}
	}
}

const Flux_RenderGraph_Pass* Flux_RenderGraph::GetCurrentRecordingPass()
{
	return tls_pxCurrentRecordingPass;
}

Flux_RenderGraph::CurrentPassScope::CurrentPassScope(const Flux_RenderGraph_Pass* pxPass)
	: m_pxPrev(tls_pxCurrentRecordingPass)
{
	tls_pxCurrentRecordingPass = pxPass;
}

Flux_RenderGraph::CurrentPassScope::~CurrentPassScope()
{
	tls_pxCurrentRecordingPass = m_pxPrev;
}

// Walk every resource the graph knows about (both declared attachments/buffers
// via m_xResources, and transient attachments) and check whether xVRAMHandle
// matches one. Externally-managed static assets (e.g. skybox cubemap loaded
// from disk, BRDF LUT preserved across frames) are NOT tracked — binding one
// of those without a Read/Write is legal because its layout is fixed.
static bool IsGraphTrackedVRAMHandle(const Flux_RenderGraph* pxGraph, Flux_VRAMHandle xVRAMHandle)
{
	if (pxGraph == nullptr) return false;
	const Zenith_HashMap<void*, Flux_RenderGraph_Resource>& xResources = pxGraph->GetResources();
	for (Zenith_HashMap<void*, Flux_RenderGraph_Resource>::Iterator it(xResources); !it.Done(); it.Next())
	{
		const Flux_RenderGraph_Resource& rxRes = it.GetValue();
		if (rxRes.m_xResource.IsImageLike())
		{
			if (rxRes.m_xResource.GetVRAMHandle() == xVRAMHandle) return true;
		}
		else if (rxRes.m_xResource.GetKind() == Flux_GraphResourceKind::Buffer)
		{
			if (rxRes.m_xResource.AsBuffer()->m_xVRAMHandle == xVRAMHandle) return true;
		}
	}
	return false;
}

// Resolve a usage entry to its VRAM handle. Returns false if the usage kind
// doesn't participate in cross-reference (neither image-like nor buffer).
static bool TryGetDeclaredVRAMHandle(const Flux_RenderGraph_ResourceUsage& rxUsage, Flux_VRAMHandle& xOut)
{
	if (rxUsage.m_xResource.IsImageLike())
	{
		xOut = rxUsage.m_xResource.GetVRAMHandle();
		return true;
	}
	if (rxUsage.m_xResource.GetKind() == Flux_GraphResourceKind::Buffer)
	{
		xOut = rxUsage.m_xResource.AsBuffer()->m_xVRAMHandle;
		return true;
	}
	return false;
}

// Is this usage declaration compatible with a bind of the given direction?
// bIsWrite   : true when binding as UAV-write (or read-modify-write).
// bExpectReads: true when scanning the pass's read-list, false for write-list.
static bool IsAccessCompatibleWithBind(const Flux_RenderGraph_ResourceUsage& rxUsage, bool bIsWrite, bool bExpectReads)
{
	if (bIsWrite)
	{
		if (bExpectReads)
		{
			// Read-list entry only qualifies when it's READWRITE_UAV (graph tracks it as a reader too).
			return rxUsage.m_eAccess == RESOURCE_ACCESS_READWRITE_UAV;
		}
		return true;
	}
	if (!bExpectReads)
	{
		// Write-list entry only qualifies for a read-bind if it's a read-modify-write UAV.
		return rxUsage.m_eAccess == RESOURCE_ACCESS_READWRITE_UAV;
	}
	// READ_BUFFER_SRV is a shader-bind read mode (StructuredBuffer<T>); accept it.
	// READ_INDIRECT_ARG is GPU-command-processor only — never satisfies a shader
	// bind, so it is intentionally absent from this list.
	return rxUsage.m_eAccess == RESOURCE_ACCESS_READ_SRV
		|| rxUsage.m_eAccess == RESOURCE_ACCESS_READ_DEPTH
		|| rxUsage.m_eAccess == RESOURCE_ACCESS_READWRITE_UAV
		|| rxUsage.m_eAccess == RESOURCE_ACCESS_READ_BUFFER_SRV;
}

// Scan one usage list (reads or writes) for an entry matching xVRAMHandle whose
// declared access direction is compatible with the bind call.
static bool ScanUsagesForMatch(const Zenith_Vector<Flux_RenderGraph_ResourceUsage>& rxUsages, Flux_VRAMHandle xVRAMHandle, bool bIsWrite, bool bExpectReads)
{
	for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(rxUsages); !it.Done(); it.Next())
	{
		const Flux_RenderGraph_ResourceUsage& rxUsage = it.GetData();
		Flux_VRAMHandle xDeclHandle;
		if (!TryGetDeclaredVRAMHandle(rxUsage, xDeclHandle)) continue;
		if (!(xDeclHandle == xVRAMHandle)) continue;
		if (IsAccessCompatibleWithBind(rxUsage, bIsWrite, bExpectReads)) return true;
	}
	return false;
}

void Flux_RenderGraph::AssertBoundResourceDeclared(Flux_VRAMHandle xVRAMHandle, bool bIsWrite, const char* szBindCall)
{
	const Flux_RenderGraph_Pass* pxPass = tls_pxCurrentRecordingPass;
	// Outside a recording window (e.g. Initialise-time bind / unit test) — legal.
	if (pxPass == nullptr) return;
	// Null/invalid handle cannot be cross-referenced; Vulkan validator catches real misuse.
	if (!xVRAMHandle.IsValid()) return;

	// Resource is legal to bind if either: it's declared in this pass's reads
	// or writes, OR it's an external static asset not tracked by the graph
	// (e.g. disk-loaded skybox, BRDF LUT, frame constants buffer). Only the
	// "tracked but undeclared" case is the missed-dependency bug we catch.
	const bool bDeclared = ScanUsagesForMatch(pxPass->m_xReads, xVRAMHandle, bIsWrite, true)
	                    || ScanUsagesForMatch(pxPass->m_xWrites, xVRAMHandle, bIsWrite, false);
	const bool bUntracked = !IsGraphTrackedVRAMHandle(tls_pxCurrentRecordingGraph, xVRAMHandle);

	Zenith_Assert(bDeclared || bUntracked,
		"Flux_ShaderBinder::%s: pass '%s' binding graph-tracked VRAM handle %u without declaring it as a %s. "
		"Add the missing Read()/Write() in SetupRenderGraph or the graph cannot emit correct barriers.",
		szBindCall, pxPass->DebugName(), xVRAMHandle.AsUInt(), bIsWrite ? "Write" : "Read");
}
