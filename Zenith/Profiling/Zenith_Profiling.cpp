#include "Zenith.h"

#include "Profiling/Zenith_Profiling.h"

#include "Core/Zenith_Engine.h"
#include "Core/Multithreading/Zenith_Multithreading.h"
#include "DebugVariables/Zenith_DebugVariables.h"

// Phase 3b: the 8 non-TLS module statics moved onto
// Zenith_ProfilingImpl held by Zenith_Engine. Read/written here as
// g_xEngine.Profiling().m_xFoo (formerly g_xFoo). The 5 TLS variables
// below stay at file scope -- per-OS-thread, not per-engine.
static constexpr float fPROFILING_MAX_EVENT_TIME_SECONDS = 0.5f;
static constexpr u_int uMAX_PROFILE_DEPTH = 16;
thread_local static u_int tl_g_uCurrentDepth;
thread_local static Zenith_ProfileIndex tl_g_aeIndices[uMAX_PROFILE_DEPTH];
thread_local static const char* tl_g_aszLabels[uMAX_PROFILE_DEPTH];
thread_local static std::chrono::time_point<std::chrono::high_resolution_clock> tl_g_axStartPoints[uMAX_PROFILE_DEPTH];
thread_local static std::chrono::time_point<std::chrono::high_resolution_clock> tl_g_axEndPoints[uMAX_PROFILE_DEPTH];

// Pause has two flags because the ImGui checkbox can be toggled mid-frame, but
// pause must take effect at frame boundary for the recorded events to be
// internally consistent.
//
//   dbg_bPauseRequested                       - ImGui checkbox / debug var. Live.
//   g_xEngine.Profiling().m_bPauseEffective   - latched at EndFrame. Read by
//                                               Begin/EndFrame and the
//                                               BeginProfile/EndProfile fast-path.
//
// EndFrame compares the two: on a request->effective transition it still closes
// the in-flight TOTAL_FRAME scope so the displayed final frame is well-formed.
DEBUGVAR bool dbg_bPauseRequested = false;

void Zenith_Profiling::Initialise(Zenith_Multithreading& xThreading)
{
	m_pxThreading = &xThreading;

	tl_g_uCurrentDepth = 0;
	memset(tl_g_aeIndices, ZENITH_PROFILE_INDEX__TOTAL_FRAME, sizeof(tl_g_aeIndices));
	memset(tl_g_aszLabels, 0, sizeof(tl_g_aszLabels));
	memset(tl_g_axStartPoints, 0, sizeof(tl_g_axStartPoints));
	memset(tl_g_axEndPoints, 0, sizeof(tl_g_axEndPoints));
	Zenith_ScopedMutexLock_T xLock(m_xEventsMutex);
	m_xEvents.clear();
}

void Zenith_Profiling::RegisterThread()
{
	// NOTE: this runs during engine bootstrap BEFORE Initialise() stores
	// m_pxThreading (Zenith_Engine::Initialise calls Threading().RegisterThread()
	// -> here, then Profiling().Initialise()). m_pxThreading is therefore not
	// guaranteed valid yet, so the thread-id query stays on g_xEngine.Threading()
	// rather than the injected member. The injected m_pxThreading covers the
	// frame-loop sites (EndProfile + the GetOrCreateThreadEvents helper).
	const u_int uThreadID = g_xEngine.Threading().GetCurrentThreadID();
	Zenith_ScopedMutexLock_T xLock(m_xEventsMutex);
	Zenith_Assert(m_xEvents.find(uThreadID) == m_xEvents.end(), "Thread already registered");
	m_xEvents.insert({ uThreadID, {} });
}

void Zenith_Profiling::BeginFrame()
{
	if (m_bPauseEffective) return;

	{
		Zenith_ScopedMutexLock_T xLock(m_xEventsMutex);
		// Save previous frame's data for rendering
		m_xPreviousFrameEvents = m_xEvents;
		m_xPreviousFrameStart = m_xFrameStart;
		m_xPreviousFrameEnd = m_xFrameEnd;

		for (auto xIt = m_xEvents.begin(); xIt != m_xEvents.end(); xIt++)
		{
			xIt->second.Clear();
		}
	}

	m_xFrameStart = std::chrono::high_resolution_clock::now();
	BeginProfile(ZENITH_PROFILE_INDEX__TOTAL_FRAME);
}

void Zenith_Profiling::EndFrame()
{
	if (dbg_bPauseRequested != m_bPauseEffective)
	{
		EndProfile(ZENITH_PROFILE_INDEX__TOTAL_FRAME);
		m_xFrameEnd = std::chrono::high_resolution_clock::now();
		m_bPauseEffective = dbg_bPauseRequested;
		return;
	}
	m_bPauseEffective = dbg_bPauseRequested;
	if (m_bPauseEffective) return;

	EndProfile(ZENITH_PROFILE_INDEX__TOTAL_FRAME);
	m_xFrameEnd = std::chrono::high_resolution_clock::now();
}

#ifdef ZENITH_TOOLS
static Zenith_Maths::Vector3 HSV2RGB(const Zenith_Maths::Vector3 xHSV)
{
	const float c = xHSV[2] * xHSV[1];
	const float x = c * (1.0f - glm::abs(fmodf(xHSV[0] / 60.f, 2.f) - 1.f));
	const float m = xHSV[2] - c;

	Zenith_Maths::Vector3 xRet;
	if (xHSV[0] < 60.f)
	{
		xRet = {c,x,0.f};
	}
	else if (xHSV[0] < 120.f)
	{
		xRet = {x,c,0.f};
	}
	else if (xHSV[0] < 180)
	{
		xRet = {0.f,c,x};
	}
	else if (xHSV[0] < 240)
	{
		xRet = { 0.f,x,c };
	}
	else if (xHSV[0] < 300)
	{
		xRet = { x,0.f,c };
	}
	else
	{
		xRet = {c,0.f,x};
	}
	return xRet + Zenith_Maths::Vector3(m,m,m);
}

static Zenith_Maths::Vector3 IntToColour(u_int u)
{
	const float fHue = fmodf((u * 0.61803398875f) * 360.f, 360.f);
	return HSV2RGB({fHue,0.8f,0.5f});
}

void Zenith_Profiling::RenderToImGui()
{
	ImGui::Begin("Profiling");

	static TimelineViewState ls_xTimelineState;
	static bool ls_bShowStats = true;
	static u_int ls_uSelectedThreadID = 0;

	// Frame statistics
	const float fFrameDurationMs = std::chrono::duration_cast<std::chrono::microseconds>(m_xPreviousFrameEnd - m_xPreviousFrameStart).count() / 1000.0f;
	const float fFPS = (fFrameDurationMs > 0.0f) ? (1000.0f / fFrameDurationMs) : 0.0f;

	if (ls_bShowStats)
	{
		ImGui::Text("Frame Time: %.3f ms (%.1f FPS)", fFrameDurationMs, fFPS);
		ImGui::Text("Threads: %zu", m_xPreviousFrameEvents.size());
		ImGui::Separator();
	}

	// Global controls available in all tabs
	ImGui::Checkbox("Paused", &dbg_bPauseRequested);
	ImGui::Separator();

	if (ImGui::BeginTabBar("ProfilingTabs"))
	{
		if (ImGui::BeginTabItem("Timeline"))
		{
			RenderTimelineView(ls_xTimelineState);
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Thread Breakdown"))
		{
			RenderThreadBreakdown(fFrameDurationMs, ls_uSelectedThreadID);
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	ImGui::End();
}

// Per-frame canvas + interaction context for the timeline event renderer. Holds
// derived layout info (sizes, scaling) plus the cached colour/text-width arrays
// so the inner loop doesn't need 15 individual parameters.
struct TimelineRenderContext
{
	ImDrawList* pxDrawList;
	ImVec2 xCanvasPos;
	ImVec2 xCanvasMax;
	ImVec2 xMousePos;
	bool bIsHovered;
	float fCanvasWidth;
	float fCanvasTimeScale;
	float fTimelineScroll;
	float fThreadHeight;
	float fRowHeight;
	float fRowSpacing;
	int iMinDepthToRender;
	int iMaxDepthToRender;
	int iMaxDepthToRenderSeparately;
	const ImU32* axCachedColors;
	const float* afCachedTextWidths;
};

struct TimelineHoveredEvent
{
	const Zenith_Profiling::Event* pEvent = nullptr;
	float fDurationNs = 0.0f;
};

// Walk every thread × event in the previous frame, draw each visible event as a
// coloured rect with optional label, and report which event the mouse hovers
// (so the caller can render a tooltip outside the loop).
static TimelineHoveredEvent RenderTimelineEvents(const TimelineRenderContext& xCtx)
{
	// Static free function: cannot use 'this'. Recover the engine-owned
	// instance once and route every member reach through xSelf.
	auto& xSelf = g_xEngine.Profiling();
	TimelineHoveredEvent xHovered;
	for (const auto& [uThreadID, xEvents] : xSelf.m_xPreviousFrameEvents)
	{
		const float fThreadBaseY = xCtx.xCanvasPos.y + uThreadID * xCtx.fThreadHeight;

		char acLabel[64];
		snprintf(acLabel, sizeof(acLabel), "Thread %u", uThreadID);
		xCtx.pxDrawList->AddText(ImVec2(xCtx.xCanvasPos.x, fThreadBaseY), IM_COL32_WHITE, acLabel);

		const u_int uEventCount = xEvents.GetSize();
		for (u_int u = 0; u < uEventCount; ++u)
		{
			const Zenith_Profiling::Event& xEvent = xEvents.Get(uEventCount - u - 1);

			if (xEvent.m_uDepth < static_cast<u_int>(xCtx.iMinDepthToRender) || xEvent.m_uDepth > static_cast<u_int>(xCtx.iMaxDepthToRender))
				continue;

			const u_int uRowIndex = (xEvent.m_uDepth <= static_cast<u_int>(xCtx.iMaxDepthToRenderSeparately))
				? (xEvent.m_uDepth - static_cast<u_int>(xCtx.iMinDepthToRender))
				: (static_cast<u_int>(xCtx.iMaxDepthToRenderSeparately) - static_cast<u_int>(xCtx.iMinDepthToRender));

			const float fEventStartNs = static_cast<float>(std::chrono::duration_cast<std::chrono::nanoseconds>(xEvent.m_xBegin - xSelf.m_xPreviousFrameStart).count());
			const float fEventEndNs = static_cast<float>(std::chrono::duration_cast<std::chrono::nanoseconds>(xEvent.m_xEnd - xSelf.m_xPreviousFrameStart).count());
			const float fEventDurationNs = fEventEndNs - fEventStartNs;

			const float fStartPx = (fEventStartNs * xCtx.fCanvasTimeScale) - xCtx.fTimelineScroll;
			const float fEndPx = (fEventEndNs * xCtx.fCanvasTimeScale) - xCtx.fTimelineScroll;

			if (fEndPx < 0.0f || fStartPx > xCtx.fCanvasWidth)
				continue;

			const float fRowY = fThreadBaseY + uRowIndex * (xCtx.fRowHeight + xCtx.fRowSpacing);
			const ImVec2 xRectMin = ImVec2(xCtx.xCanvasPos.x + fStartPx, fRowY);
			const ImVec2 xRectMax = ImVec2(xCtx.xCanvasPos.x + fEndPx, fRowY + xCtx.fRowHeight);

			const ImVec2 xClampedMin = ImVec2(std::max(xRectMin.x, xCtx.xCanvasPos.x), xRectMin.y);
			const ImVec2 xClampedMax = ImVec2(std::min(xRectMax.x, xCtx.xCanvasMax.x), xRectMax.y);

			const bool bIsEventHovered = xCtx.bIsHovered &&
				xCtx.xMousePos.x >= xClampedMin.x && xCtx.xMousePos.x <= xClampedMax.x &&
				xCtx.xMousePos.y >= xClampedMin.y && xCtx.xMousePos.y <= xClampedMax.y;

			const ImU32 uColor = bIsEventHovered
				? IM_COL32(255, 255, 255, 255)
				: xCtx.axCachedColors[xEvent.m_eIndex];

			xCtx.pxDrawList->AddRectFilled(xClampedMin, xClampedMax, uColor, 3.0f);

			const char* szDisplayName = xEvent.m_szLabel ? xEvent.m_szLabel : g_aszProfileNames[xEvent.m_eIndex];
			const float fDisplayTextWidth = xEvent.m_szLabel
				? ImGui::CalcTextSize(szDisplayName).x
				: xCtx.afCachedTextWidths[xEvent.m_eIndex];
			const float fRectWidth = xRectMax.x - xRectMin.x;
			if (fDisplayTextWidth <= fRectWidth)
			{
				const ImVec2 xTextPos = ImVec2(std::max(xRectMin.x, xCtx.xCanvasPos.x), xRectMin.y);
				const ImU32 uTextColor = bIsEventHovered ? IM_COL32(0, 0, 0, 255) : IM_COL32_WHITE;
				xCtx.pxDrawList->AddText(xTextPos, uTextColor, szDisplayName);
			}

			if (bIsEventHovered)
			{
				xHovered.pEvent = &xEvent;
				xHovered.fDurationNs = fEventDurationNs;
			}
		}
	}
	return xHovered;
}

static void RenderTimelineHoverTooltip(const TimelineHoveredEvent& xHovered, float fFrameDurationNs)
{
	if (xHovered.pEvent == nullptr) return;

	ImGui::BeginTooltip();
	const char* szHoveredName = xHovered.pEvent->m_szLabel ? xHovered.pEvent->m_szLabel : g_aszProfileNames[xHovered.pEvent->m_eIndex];
	ImGui::Text("%s", szHoveredName);
	ImGui::Separator();

	const float fDurationUs = xHovered.fDurationNs / 1000.0f;
	const float fDurationMs = fDurationUs / 1000.0f;

	if (fDurationMs >= 1.0f)
	{
		ImGui::Text("Duration: %.3f ms", fDurationMs);
	}
	else
	{
		ImGui::Text("Duration: %.3f us", fDurationUs);
	}

	ImGui::Text("Depth: %u", xHovered.pEvent->m_uDepth);

	const float fPercentOfFrame = (xHovered.fDurationNs / fFrameDurationNs) * 100.0f;
	ImGui::Text("Frame %%: %.2f%%", fPercentOfFrame);

	ImGui::EndTooltip();
}

void Zenith_Profiling::RenderTimelineView(TimelineViewState& xState)
{
	if (ImGui::CollapsingHeader("Controls", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::SliderInt("Min Depth to Render", &xState.m_iMinDepthToRender, 0, 10);
		ImGui::SliderInt("Max Depth to Render", &xState.m_iMaxDepthToRender, 0, 20);
		ImGui::SliderInt("Max Depth to Render Separately", &xState.m_iMaxDepthToRenderSeparately, 0, 20);
		ImGui::SliderFloat("Vertical Scale", &xState.m_fVerticalScale, 0.5f, 4.0f, "%.1fx");
	}

	xState.m_iMaxDepthToRender = std::max(xState.m_iMaxDepthToRender, xState.m_iMinDepthToRender);
	xState.m_iMaxDepthToRenderSeparately = std::clamp(xState.m_iMaxDepthToRenderSeparately, xState.m_iMinDepthToRender, xState.m_iMaxDepthToRender);

	constexpr float fBASE_ROW_HEIGHT = 20.0f;
	constexpr float fBASE_ROW_SPACING = 5.0f;
	constexpr float fTHREAD_SPACING = 30.0f;

	const float fRowHeight = fBASE_ROW_HEIGHT * xState.m_fVerticalScale;
	const float fRowSpacing = fBASE_ROW_SPACING * xState.m_fVerticalScale;

	const u_int uSeparateRowCount = xState.m_iMaxDepthToRenderSeparately - xState.m_iMinDepthToRender + 1;
	const u_int uRowsPerThread = uSeparateRowCount;
	const float fThreadHeight = uRowsPerThread * (fRowHeight + fRowSpacing) + fTHREAD_SPACING;

	const float fCanvasWidth = ImGui::GetContentRegionAvail().x;
	const float fTotalHeight = static_cast<float>(m_xPreviousFrameEvents.size()) * fThreadHeight;

	ImGui::BeginChild("Timeline", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	ImGui::Dummy(ImVec2(fCanvasWidth, fTotalHeight));
	ImDrawList* const pxDrawList = ImGui::GetWindowDrawList();
	const ImVec2 xCanvasPos = ImGui::GetItemRectMin();
	const ImVec2 xCanvasMax = ImGui::GetItemRectMax();
	const bool bIsHovered = ImGui::IsItemHovered();

	if (bIsHovered)
	{
		if (ImGui::GetIO().MouseWheel != 0.0f)
		{
			const float fOldZoom = xState.m_fTimelineZoom;
			xState.m_fTimelineZoom *= (1.0f + ImGui::GetIO().MouseWheel * 0.1f);
			xState.m_fTimelineZoom = std::clamp(xState.m_fTimelineZoom, 0.1f, 100.0f);

			const float fMouseX = ImGui::GetMousePos().x - xCanvasPos.x;
			const float fZoomRatio = xState.m_fTimelineZoom / fOldZoom;
			xState.m_fTimelineScroll = (xState.m_fTimelineScroll + fMouseX) * fZoomRatio - fMouseX;
			xState.m_fTimelineScroll = std::max(0.0f, xState.m_fTimelineScroll);
		}

		if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
		{
			xState.m_fTimelineScroll -= ImGui::GetIO().MouseDelta.x;
			xState.m_fTimelineScroll = std::max(0.0f, xState.m_fTimelineScroll);
		}
	}

	static ImU32 ls_axCachedColors[ZENITH_PROFILE_INDEX__COUNT] = {0};
	static float ls_afCachedTextWidths[ZENITH_PROFILE_INDEX__COUNT] = { 0.f };

	// Cached on first render rather than in Initialise() because ImGui isn't yet
	// initialised when Zenith_Profiling::Initialise runs.
	static bool ls_bOnce = true;
	if (ls_bOnce)
	{
		for (u_int i = 0; i < ZENITH_PROFILE_INDEX__COUNT; ++i)
		{
			const Zenith_Maths::Vector3 xColour = IntToColour(i) * 255.f;
			ls_axCachedColors[i] = IM_COL32((int)xColour.r, (int)xColour.g, (int)xColour.b, 255);
			ls_afCachedTextWidths[i] = ImGui::CalcTextSize(g_aszProfileNames[i]).x;
		}
		ls_bOnce = false;
	}

	const float fFrameDuration = static_cast<float>(std::chrono::duration_cast<std::chrono::nanoseconds>(m_xPreviousFrameEnd - m_xPreviousFrameStart).count());
	const float fCanvasTimeScale = (fCanvasWidth * xState.m_fTimelineZoom) / fFrameDuration;

	TimelineRenderContext xCtx;
	xCtx.pxDrawList = pxDrawList;
	xCtx.xCanvasPos = xCanvasPos;
	xCtx.xCanvasMax = xCanvasMax;
	xCtx.xMousePos = ImGui::GetMousePos();
	xCtx.bIsHovered = bIsHovered;
	xCtx.fCanvasWidth = fCanvasWidth;
	xCtx.fCanvasTimeScale = fCanvasTimeScale;
	xCtx.fTimelineScroll = xState.m_fTimelineScroll;
	xCtx.fThreadHeight = fThreadHeight;
	xCtx.fRowHeight = fRowHeight;
	xCtx.fRowSpacing = fRowSpacing;
	xCtx.iMinDepthToRender = xState.m_iMinDepthToRender;
	xCtx.iMaxDepthToRender = xState.m_iMaxDepthToRender;
	xCtx.iMaxDepthToRenderSeparately = xState.m_iMaxDepthToRenderSeparately;
	xCtx.axCachedColors = ls_axCachedColors;
	xCtx.afCachedTextWidths = ls_afCachedTextWidths;

	const TimelineHoveredEvent xHovered = RenderTimelineEvents(xCtx);
	RenderTimelineHoverTooltip(xHovered, fFrameDuration);

	ImGui::EndChild();
}

// Hierarchy node built from a thread's profile events. Lives at file scope so the
// data-build pass (BuildProfileHierarchy) and the render lambda inside
// RenderThreadBreakdown share the same type without crossing translation units.
struct ProfileNode
{
	Zenith_ProfileIndex eIndex;
	float fTotalTimeMs;
	float fSelfTimeMs;
	u_int uCallCount;
	u_int uDepth;
	const Zenith_Profiling::Event* pEvent; // Original event for time comparisons.
	Zenith_Vector<ProfileNode> xChildren;

	ProfileNode() : eIndex(ZENITH_PROFILE_INDEX__COUNT), fTotalTimeMs(0.0f), fSelfTimeMs(0.0f), uCallCount(0), uDepth(0), pEvent(nullptr) {}
};

// Sort a thread's events by start-timestamp. Stable order is required because the
// hierarchy reconstruction below assumes that, within a depth, events appear in
// the order they were entered.
static Zenith_Vector<const Zenith_Profiling::Event*> SortEventsByStart(const Zenith_Vector<Zenith_Profiling::Event>& xThreadEvents)
{
	Zenith_Vector<const Zenith_Profiling::Event*> xSorted;
	const u_int uEventCount = xThreadEvents.GetSize();
	for (u_int u = 0; u < uEventCount; ++u)
	{
		xSorted.PushBack(&xThreadEvents.Get(u));
	}
	std::sort(xSorted.GetDataPointer(), xSorted.GetDataPointer() + xSorted.GetSize(),
		[](const Zenith_Profiling::Event* a, const Zenith_Profiling::Event* b) { return a->m_xBegin < b->m_xBegin; });
	return xSorted;
}

// Build a leaf ProfileNode from a single event. Self-time starts equal to total
// time; it is reduced as children are added in the main loop.
static ProfileNode MakeNodeFromEvent(const Zenith_Profiling::Event* pEvent)
{
	const float fDurationNs = static_cast<float>(std::chrono::duration_cast<std::chrono::nanoseconds>(pEvent->m_xEnd - pEvent->m_xBegin).count());
	const float fDurationMs = fDurationNs / 1000000.0f;

	ProfileNode xNode;
	xNode.eIndex = pEvent->m_eIndex;
	xNode.fTotalTimeMs = fDurationMs;
	xNode.fSelfTimeMs = fDurationMs;
	xNode.uCallCount = 1;
	xNode.uDepth = pEvent->m_uDepth;
	xNode.pEvent = pEvent;
	return xNode;
}

// Pop entries from the depth stack whose scope ended before the new event begins.
// These are completed siblings/parents and must not collect further children.
static void PopExpiredScopes(Zenith_Vector<ProfileNode*>& xDepthStack, const Zenith_Profiling::Event* pNewEvent)
{
	while (xDepthStack.GetSize() > 0)
	{
		const ProfileNode* pStackTop = xDepthStack.Get(xDepthStack.GetSize() - 1);
		if (pStackTop->pEvent->m_xEnd <= pNewEvent->m_xBegin)
		{
			xDepthStack.Remove(xDepthStack.GetSize() - 1);
		}
		else
		{
			break;
		}
	}
}

// Trim the stack so its size matches the requested depth. Anything beyond that
// depth is a sibling at or below the new event's level and must be removed
// before the new event becomes the active scope at its depth.
static void TrimStackToDepth(Zenith_Vector<ProfileNode*>& xDepthStack, u_int uDepth)
{
	while (xDepthStack.GetSize() > uDepth)
	{
		xDepthStack.Remove(xDepthStack.GetSize() - 1);
	}
}

// Sort a thread's events by start time and assemble them into a parent/child tree
// using the per-event depth field. Self-time is subtracted from the parent as each
// child is added, so no second pass is needed.
//
// Algorithm:
//   For each event in start-time order:
//     1) Pop any scope on the depth stack that has already ended (PopExpiredScopes).
//     2) Trim the stack to the new event's depth so we have the correct parent.
//     3) Either push as a root (depth 0) or attach to the parent at depth-1,
//        subtracting our duration from the parent's self-time.
//     4) Push the new node as the active scope at its depth.
static Zenith_Vector<ProfileNode> BuildProfileHierarchy(const Zenith_Vector<Zenith_Profiling::Event>& xThreadEvents)
{
	const Zenith_Vector<const Zenith_Profiling::Event*> xSortedEvents = SortEventsByStart(xThreadEvents);

	Zenith_Vector<ProfileNode> xRootNodes;
	Zenith_Vector<ProfileNode*> xDepthStack; // xDepthStack[i] = current parent node at depth i
	xDepthStack.Reserve(16);

	for (u_int u = 0; u < xSortedEvents.GetSize(); ++u)
	{
		const Zenith_Profiling::Event* pEvent = xSortedEvents.Get(u);
		ProfileNode xNode = MakeNodeFromEvent(pEvent);

		PopExpiredScopes(xDepthStack, pEvent);
		TrimStackToDepth(xDepthStack, pEvent->m_uDepth);

		if (pEvent->m_uDepth == 0)
		{
			xRootNodes.PushBack(xNode);
			TrimStackToDepth(xDepthStack, 0);
			xDepthStack.PushBack(&xRootNodes.Get(xRootNodes.GetSize() - 1));
		}
		else if (xDepthStack.GetSize() >= pEvent->m_uDepth)
		{
			ProfileNode* pParent = xDepthStack.Get(pEvent->m_uDepth - 1);
			pParent->xChildren.PushBack(xNode);
			pParent->fSelfTimeMs -= xNode.fTotalTimeMs;
			TrimStackToDepth(xDepthStack, pEvent->m_uDepth);
			xDepthStack.PushBack(&pParent->xChildren.Get(pParent->xChildren.GetSize() - 1));
		}
	}
	return xRootNodes;
}

// Thread combo box for RenderThreadBreakdown — reads the current selection from
// uThreadID and writes back any new selection.
static void RenderThreadSelector(u_int& uThreadID)
{
	ImGui::Text("Select Thread:");

	// Static free function: recover the engine-owned instance once.
	auto& xSelf = g_xEngine.Profiling();
	Zenith_Vector<u_int> xAvailableThreads;
	for (const auto& [uID, xEvents] : xSelf.m_xPreviousFrameEvents)
	{
		xAvailableThreads.PushBack(uID);
	}
	std::sort(xAvailableThreads.GetDataPointer(), xAvailableThreads.GetDataPointer() + xAvailableThreads.GetSize());

	char acCurrentThreadLabel[64];
	snprintf(acCurrentThreadLabel, sizeof(acCurrentThreadLabel), "Thread %u", uThreadID);

	if (!ImGui::BeginCombo("Thread", acCurrentThreadLabel))
		return;

	for (u_int u = 0; u < xAvailableThreads.GetSize(); ++u)
	{
		const u_int uID = xAvailableThreads.Get(u);
		char acThreadLabel[64];
		snprintf(acThreadLabel, sizeof(acThreadLabel), "Thread %u", uID);

		const bool bIsSelected = (uThreadID == uID);
		if (ImGui::Selectable(acThreadLabel, bIsSelected))
		{
			uThreadID = uID;
		}
		if (bIsSelected)
		{
			ImGui::SetItemDefaultFocus();
		}
	}
	ImGui::EndCombo();
}

struct ProfileRowContext
{
	float fFrameDurationMs;
	u_int uThreadID;
	const ImU32* pxCachedColors;
	u_int uNodeIDCounter;
};

// Render a single ProfileNode row (6 columns). Recurses into children when open.
// `uNodeIDCounter` is carried in-context so every node gets a unique ImGui ID.
static void RenderProfileNodeRow(const ProfileNode& xNode, u_int uIndentLevel, ProfileRowContext& xCtx)
{
	const u_int uCurrentNodeID = xCtx.uNodeIDCounter++;

	ImGui::TableNextRow();

	// Color swatch
	ImGui::TableSetColumnIndex(0);
	ImDrawList* pxDrawList = ImGui::GetWindowDrawList();
	const ImVec2 xCursorPos = ImGui::GetCursorScreenPos();
	const float fSwatchSize = 16.0f;
	const float fIndent = uIndentLevel * 20.0f;
	pxDrawList->AddRectFilled(
		ImVec2(xCursorPos.x + 2 + fIndent, xCursorPos.y + 2),
		ImVec2(xCursorPos.x + fSwatchSize + fIndent, xCursorPos.y + fSwatchSize),
		xCtx.pxCachedColors[xNode.eIndex],
		2.0f
	);
	ImGui::Dummy(ImVec2(fSwatchSize + fIndent, fSwatchSize));

	// Profile name (indented tree node)
	ImGui::TableSetColumnIndex(1);
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + uIndentLevel * 20.0f);

	const bool bHasChildren = xNode.xChildren.GetSize() > 0;
	char acNodeID[128];
	snprintf(acNodeID, sizeof(acNodeID), "###node_%u_%u", xCtx.uThreadID, uCurrentNodeID);

	bool bNodeOpen = false;
	if (bHasChildren)
	{
		bNodeOpen = ImGui::TreeNodeEx(acNodeID, ImGuiTreeNodeFlags_SpanFullWidth, "%s", g_aszProfileNames[xNode.eIndex]);
	}
	else
	{
		const ImGuiTreeNodeFlags eFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth;
		ImGui::TreeNodeEx(acNodeID, eFlags, "%s", g_aszProfileNames[xNode.eIndex]);
	}

	// Total time
	ImGui::TableSetColumnIndex(2);
	if (xNode.fTotalTimeMs >= 1.0f)
		ImGui::Text("%.3f ms", xNode.fTotalTimeMs);
	else
		ImGui::Text("%.3f us", xNode.fTotalTimeMs * 1000.0f);

	// Self time
	ImGui::TableSetColumnIndex(3);
	if (xNode.fSelfTimeMs >= 1.0f)
		ImGui::Text("%.3f ms", xNode.fSelfTimeMs);
	else if (xNode.fSelfTimeMs >= 0.0f)
		ImGui::Text("%.3f us", xNode.fSelfTimeMs * 1000.0f);
	else
		ImGui::Text("0.000 us");

	// Percentage
	ImGui::TableSetColumnIndex(4);
	const float fPercentOfFrame = (xCtx.fFrameDurationMs > 0.0f) ? (xNode.fTotalTimeMs / xCtx.fFrameDurationMs) * 100.0f : 0.0f;
	ImGui::Text("%.2f%%", fPercentOfFrame);

	// Call count
	ImGui::TableSetColumnIndex(5);
	ImGui::Text("%u", xNode.uCallCount);

	if (bNodeOpen && bHasChildren)
	{
		for (u_int i = 0; i < xNode.xChildren.GetSize(); ++i)
		{
			RenderProfileNodeRow(xNode.xChildren.Get(i), uIndentLevel + 1, xCtx);
		}
		ImGui::TreePop();
	}
}

void Zenith_Profiling::RenderThreadBreakdown(float fFrameDurationMs, u_int& uThreadID)
{
	RenderThreadSelector(uThreadID);
	ImGui::Separator();

	auto xThreadIt = m_xPreviousFrameEvents.find(uThreadID);
	if (xThreadIt == m_xPreviousFrameEvents.end())
	{
		ImGui::Text("Thread %u not found in profiling data", uThreadID);
		return;
	}

	const Zenith_Vector<Event>& xThreadEvents = xThreadIt->second;
	if (xThreadEvents.GetSize() == 0)
	{
		ImGui::Text("No events recorded for Thread %u", uThreadID);
		return;
	}

	Zenith_Vector<ProfileNode> xRootNodes = BuildProfileHierarchy(xThreadEvents);

	// Cache colors — populated once on first call.
	static ImU32 ls_axCachedColors[ZENITH_PROFILE_INDEX__COUNT] = {0};
	static bool ls_bColorsInitialized = false;
	if (!ls_bColorsInitialized)
	{
		for (u_int i = 0; i < ZENITH_PROFILE_INDEX__COUNT; ++i)
		{
			const Zenith_Maths::Vector3 xColour = IntToColour(i) * 255.f;
			ls_axCachedColors[i] = IM_COL32((int)xColour.r, (int)xColour.g, (int)xColour.b, 255);
		}
		ls_bColorsInitialized = true;
	}

	ImGui::Text("Thread %u - Hierarchical Breakdown", uThreadID);
	ImGui::Separator();

	if (!ImGui::BeginTable("ProfileBreakdown", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY))
		return;

	ImGui::TableSetupColumn("Color", ImGuiTableColumnFlags_WidthFixed, 20.0f);
	ImGui::TableSetupColumn("Profile Name", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupColumn("Total Time", ImGuiTableColumnFlags_WidthFixed, 120.0f);
	ImGui::TableSetupColumn("Self Time", ImGuiTableColumnFlags_WidthFixed, 120.0f);
	ImGui::TableSetupColumn("% of Frame", ImGuiTableColumnFlags_WidthFixed, 100.0f);
	ImGui::TableSetupColumn("Call Count", ImGuiTableColumnFlags_WidthFixed, 100.0f);
	ImGui::TableHeadersRow();

	ProfileRowContext xCtx{ fFrameDurationMs, uThreadID, ls_axCachedColors, 0u };
	for (u_int i = 0; i < xRootNodes.GetSize(); ++i)
	{
		RenderProfileNodeRow(xRootNodes.Get(i), 0, xCtx);
	}

	ImGui::EndTable();
}
#endif

static Zenith_Vector<Zenith_Profiling::Event>& GetOrCreateThreadEvents()
{
	// Static free function: recover the engine-owned instance once and route
	// both the thread-id query (via the injected m_pxThreading) and the event
	// map through xSelf.
	auto& xSelf = g_xEngine.Profiling();
	u_int uThreadID = xSelf.m_pxThreading->GetCurrentThreadID();
	Zenith_ScopedMutexLock_T xLock(xSelf.m_xEventsMutex);
	auto xIt = xSelf.m_xEvents.find(uThreadID);
	if (xIt == xSelf.m_xEvents.end())
	{
		xIt = xSelf.m_xEvents.insert({ uThreadID, {} }).first;
	}
	return xIt->second;
}

void Zenith_Profiling::BeginProfile(const Zenith_ProfileIndex eIndex, const char* szLabel)
{
	if (m_bPauseEffective) return;

	Zenith_Assert(tl_g_uCurrentDepth < uMAX_PROFILE_DEPTH, "Profiling has nested too far");

	tl_g_aeIndices[tl_g_uCurrentDepth] = eIndex;
	tl_g_aszLabels[tl_g_uCurrentDepth] = szLabel;
	tl_g_axStartPoints[tl_g_uCurrentDepth] = std::chrono::high_resolution_clock::now();

	tl_g_uCurrentDepth++;
}

void Zenith_Profiling::EndProfile(const Zenith_ProfileIndex eIndex)
{
	if (m_bPauseEffective) return;

	Zenith_Assert(tl_g_uCurrentDepth > 0, "Ending profiling but it never started");
	Zenith_Assert(tl_g_aeIndices[tl_g_uCurrentDepth - 1] == eIndex, "Expecting to end profile index %u but %u was found", eIndex, tl_g_aeIndices[tl_g_uCurrentDepth]);

	tl_g_uCurrentDepth--;

	tl_g_axEndPoints[tl_g_uCurrentDepth] = std::chrono::high_resolution_clock::now();
	const Event xEvent = {
		tl_g_axStartPoints[tl_g_uCurrentDepth],
		tl_g_axEndPoints[tl_g_uCurrentDepth],
		tl_g_aeIndices[tl_g_uCurrentDepth],
		tl_g_uCurrentDepth,
		tl_g_aszLabels[tl_g_uCurrentDepth]
	};

	const float fDurationSeconds = std::chrono::duration<float>(tl_g_axEndPoints[tl_g_uCurrentDepth] - tl_g_axStartPoints[tl_g_uCurrentDepth]).count();
	if (fDurationSeconds > fPROFILING_MAX_EVENT_TIME_SECONDS)
	{
		const char* szEventName = tl_g_aszLabels[tl_g_uCurrentDepth] ? tl_g_aszLabels[tl_g_uCurrentDepth] : g_aszProfileNames[tl_g_aeIndices[tl_g_uCurrentDepth]];
		Zenith_Warning(LOG_CATEGORY_CORE, "Profiling: Event '%s' took %.3fms (threshold: %.3fms) on thread %u",
			szEventName,
			fDurationSeconds * 1000.0f,
			fPROFILING_MAX_EVENT_TIME_SECONDS * 1000.0f,
			m_pxThreading->GetCurrentThreadID());
	}

	GetOrCreateThreadEvents().PushBack(xEvent);

	tl_g_aeIndices[tl_g_uCurrentDepth] = ZENITH_PROFILE_INDEX__TOTAL_FRAME;
	tl_g_aszLabels[tl_g_uCurrentDepth] = nullptr;
}

const Zenith_ProfileIndex Zenith_Profiling::GetCurrentIndex()
{
	Zenith_Assert(tl_g_uCurrentDepth > 0, "Trying to get profiling index but nothing is being profiled");
	return tl_g_aeIndices[tl_g_uCurrentDepth - 1];
}

const std::unordered_map<u_int, Zenith_Vector<Zenith_Profiling::Event>>& Zenith_Profiling::GetEvents()
{
	return m_xEvents;
}

void Zenith_Profiling::ClearEvents()
{
	Zenith_ScopedMutexLock_T xLock(m_xEventsMutex);
	for (auto xIt = m_xEvents.begin(); xIt != m_xEvents.end(); xIt++)
	{
		xIt->second.Clear();
	}
}

void Zenith_Profiling::WriteTextReport(FILE* pFile)
{
	Zenith_ScopedMutexLock_T xLock(m_xEventsMutex);

	struct IndexStats
	{
		double fTotalMs = 0.0;
		double fMinMs = 1e30;
		double fMaxMs = 0.0;
		uint32_t uCallCount = 0;
		Zenith_ProfileIndex eIndex = ZENITH_PROFILE_INDEX__COUNT;
	};

	IndexStats axStats[ZENITH_PROFILE_INDEX__COUNT] = {};
	for (u_int i = 0; i < ZENITH_PROFILE_INDEX__COUNT; ++i)
		axStats[i].eIndex = static_cast<Zenith_ProfileIndex>(i);

	u_int uTotalEvents = 0;
	u_int uThreadCount = 0;

	for (const auto& [uThreadID, xEvents] : m_xEvents)
	{
		u_int uEventCount = xEvents.GetSize();
		if (uEventCount > 0)
			uThreadCount++;
		uTotalEvents += uEventCount;

		for (u_int u = 0; u < uEventCount; ++u)
		{
			const Event& xEvent = xEvents.Get(u);
			double fDurationMs = std::chrono::duration<double, std::milli>(xEvent.m_xEnd - xEvent.m_xBegin).count();

			IndexStats& xStat = axStats[xEvent.m_eIndex];
			xStat.uCallCount++;
			xStat.fTotalMs += fDurationMs;
			if (fDurationMs < xStat.fMinMs)
				xStat.fMinMs = fDurationMs;
			if (fDurationMs > xStat.fMaxMs)
				xStat.fMaxMs = fDurationMs;
		}
	}

	// Sort indices by total time descending
	Zenith_ProfileIndex aeSorted[ZENITH_PROFILE_INDEX__COUNT];
	u_int uSortedCount = 0;
	for (u_int i = 0; i < ZENITH_PROFILE_INDEX__COUNT; ++i)
	{
		if (axStats[i].uCallCount > 0)
			aeSorted[uSortedCount++] = static_cast<Zenith_ProfileIndex>(i);
	}

	for (u_int i = 0; i < uSortedCount; ++i)
	{
		for (u_int j = i + 1; j < uSortedCount; ++j)
		{
			if (axStats[aeSorted[j]].fTotalMs > axStats[aeSorted[i]].fTotalMs)
			{
				Zenith_ProfileIndex eTmp = aeSorted[i];
				aeSorted[i] = aeSorted[j];
				aeSorted[j] = eTmp;
			}
		}
	}

	fprintf(pFile, "\n=== Profiling Report ===\n");
	fprintf(pFile, "Threads with events: %u | Total events: %u\n\n", uThreadCount, uTotalEvents);
	fprintf(pFile, "%-40s %12s %10s %12s %12s %12s\n",
		"Profile Zone", "Total (ms)", "Calls", "Avg (ms)", "Min (ms)", "Max (ms)");
	fprintf(pFile, "---------------------------------------- ------------ ---------- ------------ ------------ ------------\n");

	for (u_int i = 0; i < uSortedCount; ++i)
	{
		const IndexStats& xStat = axStats[aeSorted[i]];
		double fAvgMs = xStat.fTotalMs / xStat.uCallCount;
		fprintf(pFile, "%-40s %12.1f %10u %12.3f %12.3f %12.3f\n",
			g_aszProfileNames[aeSorted[i]],
			xStat.fTotalMs,
			xStat.uCallCount,
			fAvgMs,
			xStat.fMinMs,
			xStat.fMaxMs);
	}

	fprintf(pFile, "\n");
}

// ===== Bridge forwarders (documented header-include-cycle break) =====
// These let header-inline code (Zenith_Profiling::Scope ctor/dtor and the
// ZENITH_PROFILING_FUNCTION_WRAPPER macro body) call into the engine-owned
// instance without dragging Zenith_Engine.h into Zenith_Profiling.h.
// See Zenith_Profiling.h for the matching declarations.
void Zenith_Profiling_Detail::BeginProfile(Zenith_ProfileIndex eIndex, const char* szLabel)
{
	g_xEngine.Profiling().BeginProfile(eIndex, szLabel);
}

void Zenith_Profiling_Detail::EndProfile(Zenith_ProfileIndex eIndex)
{
	g_xEngine.Profiling().EndProfile(eIndex);
}
