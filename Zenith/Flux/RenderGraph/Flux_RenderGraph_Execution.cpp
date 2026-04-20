#include "Zenith.h"

#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_RenderTargets.h"
#include "Zenith_PlatformGraphics_Include.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#include "Multithreading/Zenith_Multithreading.h"

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
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "Flux_RenderGraph::Execute: must be called from main thread");
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
		ResolveClearFlags();
		SynthesizeBarriers();
		SynthesizeAliasingBarriers();
		m_bEnabledMaskDirty = false;
	}
	CallPrepareCallbacks();
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
	Zenith_TaskSystem::SubmitTaskArray(&xTasks);
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
		Flux::SubmitCommandList(pxPass->m_pxCommandList,
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
	for (auto& xPair : pxGraph->GetResources())
	{
		const Flux_RenderGraph_Resource& rxRes = xPair.second;
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

void Flux_RenderGraph::AssertBoundResourceDeclared(Flux_VRAMHandle xVRAMHandle, bool bIsWrite, const char* szBindCall)
{
	const Flux_RenderGraph_Pass* pxPass = tls_pxCurrentRecordingPass;
	if (pxPass == nullptr)
	{
		// Outside a render-graph recording window — e.g. Initialise-time binding
		// path or unit test. Legal.
		return;
	}
	if (!xVRAMHandle.IsValid())
	{
		// Null/invalid VRAM handle cannot be cross-referenced — likely the caller
		// already asserted on a higher level (or bound a placeholder for a
		// disabled feature). Skip silently; the Vulkan validator will catch
		// actual unbound-descriptor usage.
		return;
	}

	// Scan the pass's declared reads and writes for a resource whose image-like
	// VRAM handle matches the bound view's VRAM handle. Buffers compare by the
	// buffer pointer's VRAM handle too (Flux_Buffer::m_xVRAMHandle).
	auto ScanUsages = [&](const Zenith_Vector<Flux_RenderGraph_ResourceUsage>& rxUsages, bool bExpectReads) -> bool
	{
		for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(rxUsages); !it.Done(); it.Next())
		{
			const Flux_RenderGraph_ResourceUsage& rxUsage = it.GetData();
			Flux_VRAMHandle xDeclHandle;
			if (rxUsage.m_xResource.IsImageLike())
				xDeclHandle = rxUsage.m_xResource.GetVRAMHandle();
			else if (rxUsage.m_xResource.GetKind() == Flux_GraphResourceKind::Buffer)
				xDeclHandle = rxUsage.m_xResource.AsBuffer()->m_xVRAMHandle;
			else
				continue;

			if (!(xDeclHandle == xVRAMHandle)) continue;
			// Match — enforce that the access direction is compatible with the bind call.
			if (bIsWrite)
			{
				if (bExpectReads)
				{
					// Read-list entry; only READWRITE_UAV on the read side is meaningful,
					// because the graph tracks it as a reader too.
					if (rxUsage.m_eAccess == RESOURCE_ACCESS_READWRITE_UAV) return true;
					continue;
				}
				return true;
			}
			else
			{
				// Read binding (SRV). Must be declared as a Read with SRV-compatible access
				// OR as READWRITE_UAV (which implies both read and write).
				if (!bExpectReads)
				{
					// Write-list entry — only READWRITE_UAV qualifies (read-modify-write).
					if (rxUsage.m_eAccess == RESOURCE_ACCESS_READWRITE_UAV) return true;
					continue;
				}
				if (rxUsage.m_eAccess == RESOURCE_ACCESS_READ_SRV
				 || rxUsage.m_eAccess == RESOURCE_ACCESS_READ_DEPTH
				 || rxUsage.m_eAccess == RESOURCE_ACCESS_READWRITE_UAV)
					return true;
				continue;
			}
		}
		return false;
	};

	// Resource is legal to bind if either: it's declared in this pass's reads
	// or writes, OR it's an external static asset not tracked by the graph
	// (e.g. disk-loaded skybox, BRDF LUT, frame constants buffer). Only the
	// "tracked but undeclared" case is the missed-dependency bug we catch.
	const bool bDeclared = ScanUsages(pxPass->m_xReads, true) || ScanUsages(pxPass->m_xWrites, false);
	const bool bUntracked = !IsGraphTrackedVRAMHandle(tls_pxCurrentRecordingGraph, xVRAMHandle);

	Zenith_Assert(bDeclared || bUntracked,
		"Flux_ShaderBinder::%s: pass '%s' binding graph-tracked VRAM handle %u without declaring it as a %s. "
		"Add the missing Read()/Write() in SetupRenderGraph or the graph cannot emit correct barriers.",
		szBindCall, pxPass->DebugName(), xVRAMHandle.AsUInt(), bIsWrite ? "Write" : "Read");
}
