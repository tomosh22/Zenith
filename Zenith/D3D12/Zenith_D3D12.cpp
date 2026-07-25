#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "D3D12/Zenith_D3D12.h"
#include "D3D12/Zenith_D3D12_CommandBuffer.h"
#include "Flux/Flux_RendererImpl.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Flux/Flux_WorkDistribution.h"

#ifdef ZENITH_TOOLS
#include "Core/Zenith_ImGuiBridgeHook.h"
#include "Windows/Zenith_Windows_Window.h"
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include <atomic>
#include <cstdlib>
#endif

// Out-of-line so the body can pull the full render-graph / pass / entry types,
// which the seam-reachable Zenith_D3D12.h header is kept clear of. See the
// declaration's comment in Zenith_D3D12.h.
void Zenith_D3D12::RecordFrame(const Flux_WorkDistribution& xWorkDistribution)
{
	// The null backend records nothing real, but it MUST still run every queued
	// pass's record callback: callback side effects (buffer uploads, ECS reads,
	// CPU draw-list construction) are what the engine + games rely on each frame,
	// and they used to run via the (now removed) Flux_CommandList recording stage.
	// The CB_HumanSession-on-null-backend proof exercises exactly this path.
	//
	// A single no-op command buffer suffices — its recorder methods are no-ops, so
	// there is no GPU cost and no need for the worker distribution / parallelism
	// the real backend uses. Passes are recorded in topological order (the queue's
	// order), matching the Vulkan backend's per-worker contiguous slices.
	(void)xWorkDistribution;
	Zenith_D3D12_CommandBuffer xNoOpCmdBuf;
	Zenith_Vector<Flux_RenderPassEntry>& xPending = g_xEngine.FluxRenderer().GetPendingRenderPasses();
	for (u_int i = 0; i < xPending.GetSize(); i++)
	{
		const Flux_RenderPassEntry& xEntry = xPending.Get(i);
		Flux_RenderGraph::RecordPassInto(xEntry.m_pxPass, xEntry.m_pxGraph, &xNoOpCmdBuf, i);
	}
}

#ifdef ZENITH_TOOLS

// ============================================================================
// ImGui integration — the one part of the null backend that does REAL work.
//
// The editor composes a full ImGui frame every tools boot regardless of
// backend, and ImGui asserts hard ("No current context") the moment a widget
// call runs without one. So the null backend owns a genuine ImGui context and
// the RENDERER-AGNOSTIC GLFW platform backend; only the renderer backend
// (ImGui_ImplVulkan_*) is missing, so the composed draw data is built and then
// simply discarded. That is precisely what makes editor-driven automated tests
// runnable on a headless Null build.
//
// The allocator wrappers mirror the Vulkan backend's byte-for-byte so the
// editor's ImGui memory panel reports the same numbers on both backends.
// ============================================================================

namespace
{
	std::atomic<u_int64> s_ulImGuiMemoryAllocated = 0;
	std::atomic<u_int64> s_ulImGuiAllocationCount = 0;

	void* ImGuiAllocWrapper(size_t sz, void* /*user_data*/)
	{
		if (sz == 0)
		{
			return nullptr;
		}

		// Allocate with a header so the free path can subtract the right size.
		size_t* pBlock = static_cast<size_t*>(std::malloc(sizeof(size_t) + sz));
		if (!pBlock)
		{
			return nullptr;
		}

		*pBlock = sz;
		s_ulImGuiMemoryAllocated += sz;
		s_ulImGuiAllocationCount++;

		return pBlock + 1;
	}

	void ImGuiFreeWrapper(void* ptr, void* /*user_data*/)
	{
		if (!ptr)
		{
			return;
		}

		size_t* pBlock = static_cast<size_t*>(ptr) - 1;
		const size_t sz = *pBlock;
		s_ulImGuiMemoryAllocated -= sz;
		s_ulImGuiAllocationCount--;
		std::free(pBlock);
	}
}

void Zenith_D3D12::InitialiseImGui()
{
	// Hook the allocator BEFORE creating the context (same order as Vulkan).
	ImGui::SetAllocatorFunctions(ImGuiAllocWrapper, ImGuiFreeWrapper, nullptr);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGui::StyleColorsDark();

	ImGuiIO& xIO = ImGui::GetIO();
	xIO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	xIO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	// InitForOther, not InitForVulkan: the platform backend is renderer-agnostic
	// and this build has no renderer backend to name. The window is hidden (see
	// Zenith_Windows_Window.cpp), which the GLFW backend handles fine -- it still
	// reports a real framebuffer size, so ImGui's display size is valid and
	// layout/hit-testing behave exactly as they do windowed.
	ImGui_ImplGlfw_InitForOther(Zenith_Window::GetInstance()->GetNativeWindow(), true);

	// Build the font atlas HERE. Normally a renderer backend does this (its
	// NewFrame honours ImGui's texture requests, or the legacy path calls
	// GetTexDataAsRGBA32); with no renderer backend at all, nothing would, and
	// ImGui asserts on the first ImDrawList text call ("font atlas is not
	// built!"). Rasterising it costs one small CPU allocation and makes every
	// text-measuring path -- widget sizing, hit-testing, layout -- behave
	// exactly as it does on the real backend, which is what the editor-driven
	// automated tests depend on. The pixels are then simply never uploaded.
	unsigned char* pucPixels = nullptr;
	int iAtlasWidth = 0;
	int iAtlasHeight = 0;
	xIO.Fonts->GetTexDataAsRGBA32(&pucPixels, &iAtlasWidth, &iAtlasHeight);
	// A non-zero id keeps ImDrawCmd's texture handle looking valid to any code
	// that inspects it; nothing ever dereferences it on this backend.
	xIO.Fonts->SetTexID(static_cast<ImTextureID>(1));

	Zenith_Log(LOG_CATEGORY_EDITOR, "ImGui initialised on the Null backend (no renderer backend; draw data is discarded)");
}

void Zenith_D3D12::ImGuiBeginFrame()
{
	ImGui_ImplGlfw_NewFrame();
#if defined(ZENITH_INPUT_SIMULATOR)
	// Queued AFTER the GLFW backend's NewFrame and BEFORE ImGui::NewFrame
	// consumes the queue -- last event wins, so simulated input deterministically
	// overrides the hardware mouse. Same ordering contract as the Vulkan backend;
	// see Core/Zenith_ImGuiBridgeHook.h.
	if (g_pfnZenithImGuiSimulatedInput)
	{
		g_pfnZenithImGuiSimulatedInput();
	}
#endif
	ImGui::NewFrame();
}

void Zenith_D3D12::ShutdownImGui()
{
	// No GPU work is ever in flight, so there is nothing to wait on first.
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	Zenith_Log(LOG_CATEGORY_EDITOR, "ImGui shut down");
}

u_int64 Zenith_D3D12::GetImGuiMemoryAllocated()
{
	return s_ulImGuiMemoryAllocated.load();
}

u_int64 Zenith_D3D12::GetImGuiAllocationCount()
{
	return s_ulImGuiAllocationCount.load();
}

#endif // ZENITH_TOOLS
