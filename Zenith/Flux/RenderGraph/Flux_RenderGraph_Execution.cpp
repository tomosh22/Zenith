#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "Flux/Flux_RendererImpl.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_BackendTypes.h"  // full Flux_CommandBuffer for RecordPassInto's debug-marker + callback dispatch
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

void Flux_RenderGraph::RecordPassInto(const Flux_RenderGraph_Pass* pxPass, const Flux_RenderGraph* pxGraph, Flux_CommandBuffer* pxCmdBuf, u_int uExecutionIndex)
{
	// Barrier-only no-op passes (and any pass with no record callback) record
	// nothing here — the backend has already emitted their prologue barriers
	// before calling this. Guard so they're a clean no-op.
	if (!pxPass || !pxPass->m_pfnOnRecord) return;

	// Set the thread-local current-pass and current-graph pointers so
	// Flux_ShaderBinder bind-time assertions can cross-reference against this
	// pass's declared Read/Write sets and skip non-tracked static assets. The
	// graph is passed explicitly (not pulled from g_xEngine) so local / test /
	// future-multiple graphs validate against their own resource set. Scopes
	// restore prior values on exit even if the callback throws.
	CurrentPassScope xPassScope(pxPass);
	CurrentGraphScope xGraphScope(pxGraph);

	// Prologue barriers from SynthesizeBarriers are emitted by the backend
	// (outside any render pass) immediately before this call — never inside the
	// pass's recording. The backend has the render pass / compute scope open at
	// this point, so the callback's draws/dispatches land correctly.
#ifdef ZENITH_FLUX_PROFILING
	// One labelled GPU debug group per logical pass (RenderDoc / Nsight / PIX),
	// emitted inside whatever render-pass / compute scope the backend has open.
	pxCmdBuf->BeginDebugMarker(pxPass->DebugName());
	// GPU per-pass timing: bracket the pass with two timestamp writes into the
	// frame's query pool. DebugName() is a static-lifetime literal, so the GPU
	// readback can store the pointer and resolve the pass name a few frames later.
	// uExecutionIndex (the pass's place in the topological order) is stored with the
	// timer so the readback can present passes in execution order, not record-race
	// order.
	const u_int uGPUTimer = pxCmdBuf->BeginGPUTimer(pxPass->DebugName(), uExecutionIndex);
#endif
	// Per-pass record-cost scope. Runs on a worker thread; the profiler keys
	// events by thread id, so the pass's DebugName() label disambiguates which
	// pass the cost belongs to in the timeline. DebugName() is a static-lifetime
	// string literal (RenderGraph stores only the pointer), so it is safe as the
	// event label. The "Flux Record Pass" zone id is cached once via the macro's
	// static-local -- no per-frame zone interning.
	g_xEngine.Profiling().BeginProfileZone(ZENITH_PROFILE_ZONE("Flux Record Pass"), pxPass->DebugName());
	pxPass->m_pfnOnRecord(pxCmdBuf, pxPass->m_pUserData);
	g_xEngine.Profiling().EndProfileZone(ZENITH_PROFILE_ZONE("Flux Record Pass"));
#ifdef ZENITH_FLUX_PROFILING
	pxCmdBuf->EndGPUTimer(uGPUTimer);
	pxCmdBuf->EndDebugMarker();
#endif
}

void Flux_RenderGraph::Execute()
{
	Zenith_Assert(m_bCompiled, "Flux_RenderGraph::Execute: must call Compile() first");
	Zenith_Assert(!m_bDirty, "Flux_RenderGraph::Execute: graph is dirty — AddPass/Read/Write was called after Compile(). Call Compile() again.");
	Zenith_Assert(g_xEngine.Threading().IsMainThread(), "Flux_RenderGraph::Execute: must be called from main thread");
	if (m_xExecutionOrder.GetSize() == 0) return;

	// Every enabled pass in the execution order must have a record callback — a
	// null callback means the pass was destroyed without being removed from the
	// exec order, which corrupts downstream recording. (Barrier-only no-op passes
	// carry an explicit no-op callback, so this holds for them too.)
	for (Zenith_Vector<u_int>::Iterator it(m_xExecutionOrder); !it.Done(); it.Next())
	{
		const Flux_RenderGraph_Pass* pxP = m_xPasses.Get(it.GetData());
		Zenith_Assert(pxP != nullptr,
			"Flux_RenderGraph::Execute: null pass at execution order index %u", it.GetData());
		if (!IsPassEffectivelyEnabled(pxP)) continue;
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
		// reads pxPrior->m_uLastUseTopoIdx to look up the prior pool occupant's last
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
	// Queue every enabled pass (topological order), then drive the single direct-
	// recording stage: the backend records each pass's callback into its worker
	// command buffers (RecordFrame). Both happen here, synchronously, inside the
	// render-task safe window and before the frame memory submit — so record-
	// callback uploads land this frame and ECS reads stay inside the window.
	SubmitRecordedLists();
	g_xEngine.FluxRenderer().RecordFrame();
}

void Flux_RenderGraph::CallPrepareCallbacks()
{
	for (Zenith_Vector<u_int>::Iterator it(m_xExecutionOrder); !it.Done(); it.Next())
	{
		Flux_RenderGraph_Pass* pxPass = m_xPasses.Get(it.GetData());
		if (IsPassEffectivelyEnabled(pxPass) && pxPass->m_pfnOnPrepare) pxPass->m_pfnOnPrepare(pxPass->m_pUserData);
	}
}

void Flux_RenderGraph::SubmitRecordedLists()
{
	for (Zenith_Vector<u_int>::Iterator it(m_xExecutionOrder); !it.Done(); it.Next())
	{
		Flux_RenderGraph_Pass* pxPass = m_xPasses.Get(it.GetData());
		// Queue every effectively-enabled pass with a record callback. Whether the
		// callback records anything isn't knowable until it runs (no command-list
		// to count any more), so passes are no longer pruned on command count — a
		// callback that records nothing costs an empty BeginRendering/EndRendering,
		// which the backend's render-pass continuation coalesces. Barrier-only
		// no-op passes (e.g. the "Final RT Layout Transition" pass that flips the
		// swapchain source into SHADER_READ_ONLY) carry an explicit no-op callback,
		// so they're queued here and the backend emits their prologue barriers.
		if (!IsPassEffectivelyEnabled(pxPass) || !pxPass->m_pfnOnRecord) continue;
		bool bDepthReadOnly = false;
		if (pxPass->m_xDepthStencil.IsValid())
		{
			void* pDepthRes = pxPass->m_xDepthStencil.m_xResource.GetVoidPtr();
			for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator itR(pxPass->m_xReads); !itR.Done(); itR.Next())
			{
				if (itR.GetData().m_xResource.GetVoidPtr() == pDepthRes) { bDepthReadOnly = true; break; }
			}
		}
		g_xEngine.FluxRenderer().QueueRenderPass(this,
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

		// READ_DEPTH binds the resource as a READ-ONLY depth attachment (the
		// access's documented meaning — depth-tested passes that never write
		// depth, e.g. forward vegetation over the lit scene). Only WRITE_DSV
		// was inferred above historically, which left READ_DEPTH passes with
		// no depth attachment at all — an attachment-count mismatch with any
		// depth-tested pipeline (latent until Grass, the access's only user,
		// first rendered). SubmitRecordedLists sees the resource in m_xReads
		// and flags the render pass bDepthReadOnly.
		if (!pxPass->m_xDepthStencil.IsValid())
		{
			for (Zenith_Vector<Flux_RenderGraph_ResourceUsage>::Iterator it(pxPass->m_xReads); !it.Done(); it.Next())
			{
				const Flux_RenderGraph_ResourceUsage& rxUsage = it.GetData();
				if (rxUsage.m_eAccess != RESOURCE_ACCESS_READ_DEPTH || !rxUsage.m_xResource.IsImageLike())
				{
					continue;
				}
				Zenith_Assert(rxUsage.m_xResource.GetKind() == Flux_GraphResourceKind::Image,
					"Flux_RenderGraph: depth attachment reads require a 2D Flux_RenderAttachment (pass '%s')", pxPass->DebugName());
				pxPass->m_xDepthStencil =
					Flux_RenderGraph_AttachmentRef(rxUsage.m_xResource, rxUsage.m_uMipLevel, rxUsage.m_uLayer);
				break;
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

bool Flux_RenderGraph::IsHandleDeclaredRead(const Flux_RenderGraph_Pass* pxPass, Flux_VRAMHandle xVRAMHandle)
{
	if (pxPass == nullptr) return false;
	// Mirror AssertBoundResourceDeclared's SRV-bind acceptance EXACTLY: a sampled
	// resource counts as declared if it is in the reads list, OR appears as a
	// READWRITE_UAV on the writes list — the graph permits a read-modify-write
	// resource to be declared exactly once on EITHER list (see
	// CheckSubresourceConflicts), and SynthesizeBarriers handles either form. Only
	// scanning reads would false-positive on a legitimate Writes(.., READWRITE_UAV).
	return ScanUsagesForMatch(pxPass->m_xReads,  xVRAMHandle, /*bIsWrite*/false, /*bExpectReads*/true)
	    || ScanUsagesForMatch(pxPass->m_xWrites, xVRAMHandle, /*bIsWrite*/false, /*bExpectReads*/false);
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
