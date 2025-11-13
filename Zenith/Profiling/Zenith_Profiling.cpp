#include "Zenith.h"

#include "Profiling/Zenith_Profiling.h"

#include "DebugVariables/Zenith_DebugVariables.h"
#include "Flux/Flux.h"
#include "Multithreading/Zenith_Multithreading.h"

#include <chrono>
#include <algorithm>
#include <functional>

static constexpr u_int uMAX_PROFILE_DEPTH = 16;
thread_local static u_int tl_g_uCurrentDepth;
thread_local static Zenith_ProfileIndex tl_g_aeIndices[uMAX_PROFILE_DEPTH];
thread_local static std::chrono::time_point<std::chrono::high_resolution_clock> tl_g_axStartPoints[uMAX_PROFILE_DEPTH];
thread_local static std::chrono::time_point<std::chrono::high_resolution_clock> tl_g_axEndPoints[uMAX_PROFILE_DEPTH];
static std::unordered_map<u_int, Zenith_Vector<Zenith_Profiling::Event>> g_xEvents;
static std::unordered_map<u_int, Zenith_Vector<Zenith_Profiling::Event>> g_xPreviousFrameEvents;
static std::chrono::time_point<std::chrono::high_resolution_clock> g_xFrameStart;
static std::chrono::time_point<std::chrono::high_resolution_clock> g_xFrameEnd;
static std::chrono::time_point<std::chrono::high_resolution_clock> g_xPreviousFrameStart;
static std::chrono::time_point<std::chrono::high_resolution_clock> g_xPreviousFrameEnd;

DEBUGVAR bool dbg_bPaused = false;
static bool g_bIsPaused = false;

void Zenith_Profiling::Initialise()
{
	tl_g_uCurrentDepth = 0;
	memset(tl_g_aeIndices, ZENITH_PROFILE_INDEX__TOTAL_FRAME, sizeof(tl_g_aeIndices));
	memset(tl_g_axStartPoints, 0, sizeof(tl_g_axStartPoints));
	memset(tl_g_axEndPoints, 0, sizeof(tl_g_axEndPoints));
	g_xEvents.clear();
}

void Zenith_Profiling::RegisterThread()
{
	Zenith_Assert(g_xEvents.find(Zenith_Multithreading::GetCurrentThreadID()) == g_xEvents.end(), "Thread already registered");
	g_xEvents.insert({ Zenith_Multithreading::GetCurrentThreadID(), {} });
}

void Zenith_Profiling::BeginFrame()
{
	if (g_bIsPaused) return;

	// Save previous frame's data for rendering
	g_xPreviousFrameEvents = g_xEvents;
	g_xPreviousFrameStart = g_xFrameStart;
	g_xPreviousFrameEnd = g_xFrameEnd;

	for (auto xIt = g_xEvents.begin(); xIt != g_xEvents.end(); xIt++)
	{
		xIt->second.Clear();
	}

	g_xFrameStart = std::chrono::high_resolution_clock::now();
	BeginProfile(ZENITH_PROFILE_INDEX__TOTAL_FRAME);
}

void Zenith_Profiling::EndFrame()
{
	if (dbg_bPaused != g_bIsPaused)
	{
		EndProfile(ZENITH_PROFILE_INDEX__TOTAL_FRAME);
		g_xFrameEnd = std::chrono::high_resolution_clock::now();
		g_bIsPaused = dbg_bPaused;
		return;
	}
	g_bIsPaused = dbg_bPaused;
	if (g_bIsPaused) return;

	EndProfile(ZENITH_PROFILE_INDEX__TOTAL_FRAME);
	g_xFrameEnd = std::chrono::high_resolution_clock::now();
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

	static int ls_iMinDepthToRender = 0;
	static int ls_iMaxDepthToRender = 10;
	static int ls_iMaxDepthToRenderSeparately = 3;
	static float ls_fTimelineZoom = 1.0f;
	static float ls_fTimelineScroll = 0.0f;
	static float ls_fVerticalScale = 1.0f;
	static bool ls_bShowStats = true;
	static u_int ls_uSelectedThreadID = 0;

	// Frame statistics
	const float fFrameDurationMs = std::chrono::duration_cast<std::chrono::microseconds>(g_xPreviousFrameEnd - g_xPreviousFrameStart).count() / 1000.0f;
	const float fFPS = (fFrameDurationMs > 0.0f) ? (1000.0f / fFrameDurationMs) : 0.0f;

	if (ls_bShowStats)
	{
		ImGui::Text("Frame Time: %.3f ms (%.1f FPS)", fFrameDurationMs, fFPS);
		ImGui::Text("Threads: %zu", g_xPreviousFrameEvents.size());
		ImGui::Separator();
	}

	// Global controls available in all tabs
	ImGui::Checkbox("Paused", &dbg_bPaused);
	ImGui::Separator();

	if (ImGui::BeginTabBar("ProfilingTabs"))
	{
		if (ImGui::BeginTabItem("Timeline"))
		{
			RenderTimelineView(ls_iMinDepthToRender, ls_iMaxDepthToRender, ls_iMaxDepthToRenderSeparately, 
			                   ls_fTimelineZoom, ls_fTimelineScroll, ls_fVerticalScale, fFrameDurationMs);
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

void Zenith_Profiling::RenderTimelineView(int& iMinDepthToRender, int& iMaxDepthToRender, int& iMaxDepthToRenderSeparately,
                                          float& fTimelineZoom, float& fTimelineScroll, float& fVerticalScale, float fFrameDurationMs)
{
	if (ImGui::CollapsingHeader("Controls", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::SliderInt("Min Depth to Render", &iMinDepthToRender, 0, 10);
		ImGui::SliderInt("Max Depth to Render", &iMaxDepthToRender, 0, 20);
		ImGui::SliderInt("Max Depth to Render Separately", &iMaxDepthToRenderSeparately, 0, 20);
		ImGui::SliderFloat("Vertical Scale", &fVerticalScale, 0.5f, 4.0f, "%.1fx");
	}

	iMaxDepthToRender = std::max(iMaxDepthToRender, iMinDepthToRender);
	iMaxDepthToRenderSeparately = std::clamp(iMaxDepthToRenderSeparately, iMinDepthToRender, iMaxDepthToRender);

	constexpr float fBASE_ROW_HEIGHT = 20.0f;
	constexpr float fBASE_ROW_SPACING = 5.0f;
	constexpr float fTHREAD_SPACING = 30.0f;

	const float fRowHeight = fBASE_ROW_HEIGHT * fVerticalScale;
	const float fRowSpacing = fBASE_ROW_SPACING * fVerticalScale;

	const u_int uSeparateRowCount = iMaxDepthToRenderSeparately - iMinDepthToRender + 1;
	const u_int uRowsPerThread = uSeparateRowCount;
	const float fThreadHeight = uRowsPerThread * (fRowHeight + fRowSpacing) + fTHREAD_SPACING;

	const float fCanvasWidth = ImGui::GetContentRegionAvail().x;
	const float fTotalHeight = static_cast<float>(g_xPreviousFrameEvents.size()) * fThreadHeight;

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
			const float fOldZoom = fTimelineZoom;
			fTimelineZoom *= (1.0f + ImGui::GetIO().MouseWheel * 0.1f);
			fTimelineZoom = std::clamp(fTimelineZoom, 0.1f, 100.0f);
			
			const float fMouseX = ImGui::GetMousePos().x - xCanvasPos.x;
			const float fZoomRatio = fTimelineZoom / fOldZoom;
			fTimelineScroll = (fTimelineScroll + fMouseX) * fZoomRatio - fMouseX;
			fTimelineScroll = std::max(0.0f, fTimelineScroll);
		}

		if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
		{
			fTimelineScroll -= ImGui::GetIO().MouseDelta.x;
			fTimelineScroll = std::max(0.0f, fTimelineScroll);
		}
	}
	
	static ImU32 ls_axCachedColors[ZENITH_PROFILE_INDEX__COUNT] = {0};
	static float ls_afCachedTextWidths[ZENITH_PROFILE_INDEX__COUNT] = { 0.f };

	//#TO_TODO: this should be in Initialise but ImGui hasn't been inited a that point
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

	const float fFrameDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(g_xPreviousFrameEnd - g_xPreviousFrameStart).count();
	const float fCanvasTimeScale = (fCanvasWidth * fTimelineZoom) / fFrameDuration;

	const ImVec2 xMousePos = ImGui::GetMousePos();
	const Event* pHoveredEvent = nullptr;
	float fHoveredEventDuration = 0.0f;

	for (const auto& [uThreadID, xEvents] : g_xPreviousFrameEvents)
	{
		const float fThreadBaseY = xCanvasPos.y + uThreadID * fThreadHeight;

		char acLabel[64];
		snprintf(acLabel, sizeof(acLabel), "Thread %u", uThreadID);
		pxDrawList->AddText(ImVec2(xCanvasPos.x, fThreadBaseY), IM_COL32_WHITE, acLabel);

		const u_int uEventCount = xEvents.GetSize();
		for (u_int u = 0; u < uEventCount; ++u)
		{
			const Event& xEvent = xEvents.Get(uEventCount - u - 1);

			if (xEvent.m_uDepth < iMinDepthToRender || xEvent.m_uDepth > iMaxDepthToRender)
				continue;

			const u_int uRowIndex = (xEvent.m_uDepth <= iMaxDepthToRenderSeparately)
				? (xEvent.m_uDepth - iMinDepthToRender)
				: (iMaxDepthToRenderSeparately - iMinDepthToRender);

			const float fEventStartNs = std::chrono::duration_cast<std::chrono::nanoseconds>(xEvent.m_xBegin - g_xPreviousFrameStart).count();
			const float fEventEndNs = std::chrono::duration_cast<std::chrono::nanoseconds>(xEvent.m_xEnd - g_xPreviousFrameStart).count();
			const float fEventDurationNs = fEventEndNs - fEventStartNs;
			
			const float fStartPx = (fEventStartNs * fCanvasTimeScale) - fTimelineScroll;
			const float fEndPx = (fEventEndNs * fCanvasTimeScale) - fTimelineScroll;
			
			if (fEndPx < 0.0f || fStartPx > fCanvasWidth)
				continue;

			const float fRowY = fThreadBaseY + uRowIndex * (fRowHeight + fRowSpacing);
			const ImVec2 xRectMin = ImVec2(xCanvasPos.x + fStartPx, fRowY);
			const ImVec2 xRectMax = ImVec2(xCanvasPos.x + fEndPx, fRowY + fRowHeight);

			const ImVec2 xClampedMin = ImVec2(std::max(xRectMin.x, xCanvasPos.x), xRectMin.y);
			const ImVec2 xClampedMax = ImVec2(std::min(xRectMax.x, xCanvasMax.x), xRectMax.y);

			const bool bIsEventHovered = bIsHovered &&
				xMousePos.x >= xClampedMin.x && xMousePos.x <= xClampedMax.x &&
				xMousePos.y >= xClampedMin.y && xMousePos.y <= xClampedMax.y;

			const ImU32 uColor = bIsEventHovered 
				? IM_COL32(255, 255, 255, 255)
				: ls_axCachedColors[xEvent.m_eIndex];
			
			pxDrawList->AddRectFilled(xClampedMin, xClampedMax, uColor, 3.0f);

			const float fRectWidth = xRectMax.x - xRectMin.x;
			if (ls_afCachedTextWidths[xEvent.m_eIndex] <= fRectWidth)
			{
				const ImVec2 xTextPos = ImVec2(std::max(xRectMin.x, xCanvasPos.x), xRectMin.y);
				const ImU32 uTextColor = bIsEventHovered ? IM_COL32(0, 0, 0, 255) : IM_COL32_WHITE;
				pxDrawList->AddText(xTextPos, uTextColor, g_aszProfileNames[xEvent.m_eIndex]);
			}

			if (bIsEventHovered)
			{
				pHoveredEvent = &xEvent;
				fHoveredEventDuration = fEventDurationNs;
			}
		}
	}

	if (pHoveredEvent != nullptr)
	{
		ImGui::BeginTooltip();
		ImGui::Text("%s", g_aszProfileNames[pHoveredEvent->m_eIndex]);
		ImGui::Separator();
		
		const float fDurationUs = fHoveredEventDuration / 1000.0f;
		const float fDurationMs = fDurationUs / 1000.0f;
		
		if (fDurationMs >= 1.0f)
		{
			ImGui::Text("Duration: %.3f ms", fDurationMs);
		}
		else
		{
			ImGui::Text("Duration: %.3f us", fDurationUs);
		}
		
		ImGui::Text("Depth: %u", pHoveredEvent->m_uDepth);
		
		const float fPercentOfFrame = (fHoveredEventDuration / fFrameDuration) * 100.0f;
		ImGui::Text("Frame %%: %.2f%%", fPercentOfFrame);
		
		ImGui::EndTooltip();
	}

	ImGui::EndChild();
}

void Zenith_Profiling::RenderThreadBreakdown(float fFrameDurationMs, u_int& uThreadID)
{
	// Thread selector
	ImGui::Text("Select Thread:");
	
	// Build list of available threads
	Zenith_Vector<u_int> xAvailableThreads;
	for (const auto& [uID, xEvents] : g_xPreviousFrameEvents)
	{
		xAvailableThreads.PushBack(uID);
	}
	
	// Sort thread IDs
	std::sort(xAvailableThreads.GetDataPointer(), xAvailableThreads.GetDataPointer() + xAvailableThreads.GetSize());
	
	// Create combo box for thread selection
	char acCurrentThreadLabel[64];
	snprintf(acCurrentThreadLabel, sizeof(acCurrentThreadLabel), "Thread %u", uThreadID);
	
	if (ImGui::BeginCombo("Thread", acCurrentThreadLabel))
	{
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
	
	ImGui::Separator();

	// Find selected thread
	auto xThreadIt = g_xPreviousFrameEvents.find(uThreadID);
	
	if (xThreadIt == g_xPreviousFrameEvents.end())
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

	// Build hierarchical structure
	struct ProfileNode
	{
		Zenith_ProfileIndex eIndex;
		float fTotalTimeMs;
		float fSelfTimeMs;
		u_int uCallCount;
		u_int uDepth;
		const Event* pEvent; // Store pointer to original event for time comparisons
		Zenith_Vector<ProfileNode> xChildren;
		
		ProfileNode() : eIndex(ZENITH_PROFILE_INDEX__COUNT), fTotalTimeMs(0.0f), fSelfTimeMs(0.0f), uCallCount(0), uDepth(0), pEvent(nullptr) {}
	};

	// First, create a sorted copy of events by start time
	Zenith_Vector<const Event*> xSortedEvents;
	const u_int uEventCount = xThreadEvents.GetSize();
	for (u_int u = 0; u < uEventCount; ++u)
	{
		xSortedEvents.PushBack(&xThreadEvents.Get(u));
	}
	
	// Sort by start time
	std::sort(xSortedEvents.GetDataPointer(), xSortedEvents.GetDataPointer() + xSortedEvents.GetSize(),
		[](const Event* a, const Event* b) { return a->m_xBegin < b->m_xBegin; });

	// Build hierarchy using a stack of currently active events
	Zenith_Vector<ProfileNode> xRootNodes;
	Zenith_Vector<ProfileNode*> xActiveStack; // Stack of nodes that are currently active (haven't ended yet)
	
	for (u_int u = 0; u < xSortedEvents.GetSize(); ++u)
	{
		const Event* pEvent = xSortedEvents.Get(u);
		const float fDurationNs = std::chrono::duration_cast<std::chrono::nanoseconds>(pEvent->m_xEnd - pEvent->m_xBegin).count();
		const float fDurationMs = fDurationNs / 1000000.0f;
		
		// Pop from stack any events that have ended before this event starts
		while (xActiveStack.GetSize() > 0)
		{
			ProfileNode* pStackNode = xActiveStack.Get(xActiveStack.GetSize() - 1);
			// Check if the stacked event has ended before this new event starts
			if (pStackNode->pEvent->m_xEnd <= pEvent->m_xBegin)
			{
				xActiveStack.Remove(xActiveStack.GetSize() - 1);
			}
			else
			{
				break;
			}
		}
		
		// Create new node
		ProfileNode xNode;
		xNode.eIndex = pEvent->m_eIndex;
		xNode.fTotalTimeMs = fDurationMs;
		xNode.fSelfTimeMs = fDurationMs;
		xNode.uCallCount = 1;
		xNode.uDepth = pEvent->m_uDepth;
		xNode.pEvent = pEvent;
		
		// Add to parent or root
		if (xActiveStack.GetSize() == 0)
		{
			// This is a root node
			xRootNodes.PushBack(xNode);
			xActiveStack.PushBack(&xRootNodes.Get(xRootNodes.GetSize() - 1));
		}
		else
		{
			// This is a child of the top stack node (the most recent still-active event)
			ProfileNode* pParent = xActiveStack.Get(xActiveStack.GetSize() - 1);
			pParent->xChildren.PushBack(xNode);
			pParent->fSelfTimeMs -= fDurationMs; // Subtract child time from parent's self time
			xActiveStack.PushBack(&pParent->xChildren.Get(pParent->xChildren.GetSize() - 1));
		}
	}

	// Cache colors
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

	// Recursive function to render nodes
	u_int uNodeIDCounter = 0;
	std::function<void(const ProfileNode&, u_int)> RenderNode = [&](const ProfileNode& xNode, u_int uIndentLevel)
	{
		const u_int uCurrentNodeID = uNodeIDCounter++;
		
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
			ls_axCachedColors[xNode.eIndex],
			2.0f
		);
		ImGui::Dummy(ImVec2(fSwatchSize + fIndent, fSwatchSize));

		// Profile name with indentation and tree node
		ImGui::TableSetColumnIndex(1);
		
		// Set cursor position for proper indentation
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + uIndentLevel * 20.0f);
		
		bool bHasChildren = xNode.xChildren.GetSize() > 0;
		bool bNodeOpen = false;
		
		// Create a stable ID using thread ID and node counter
		char acNodeID[128];
		snprintf(acNodeID, sizeof(acNodeID), "###node_%u_%u", uThreadID, uCurrentNodeID);
		
		if (bHasChildren)
		{
			// Use tree node for items with children
			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth;
			bNodeOpen = ImGui::TreeNodeEx(acNodeID, flags, "%s", g_aszProfileNames[xNode.eIndex]);
		}
		else
		{
			// Use tree node with Leaf flag for items without children
			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth;
			ImGui::TreeNodeEx(acNodeID, flags, "%s", g_aszProfileNames[xNode.eIndex]);
		}

		// Total time
		ImGui::TableSetColumnIndex(2);
		if (xNode.fTotalTimeMs >= 1.0f)
		{
			ImGui::Text("%.3f ms", xNode.fTotalTimeMs);
		}
		else
		{
			ImGui::Text("%.3f us", xNode.fTotalTimeMs * 1000.0f);
		}

		// Self time
		ImGui::TableSetColumnIndex(3);
		if (xNode.fSelfTimeMs >= 1.0f)
		{
			ImGui::Text("%.3f ms", xNode.fSelfTimeMs);
		}
		else if (xNode.fSelfTimeMs >= 0.0f)
		{
			ImGui::Text("%.3f us", xNode.fSelfTimeMs * 1000.0f);
		}
		else
		{
			ImGui::Text("0.000 us");
		}

		// Percentage
		ImGui::TableSetColumnIndex(4);
		const float fPercentOfFrame = (fFrameDurationMs > 0.0f) ? (xNode.fTotalTimeMs / fFrameDurationMs) * 100.0f : 0.0f;
		ImGui::Text("%.2f%%", fPercentOfFrame);

		// Call count
		ImGui::TableSetColumnIndex(5);
		ImGui::Text("%u", xNode.uCallCount);

		// Render children only if node is open
		if (bNodeOpen && bHasChildren)
		{
			for (u_int i = 0; i < xNode.xChildren.GetSize(); ++i)
			{
				RenderNode(xNode.xChildren.Get(i), uIndentLevel + 1);
			}
			ImGui::TreePop();
		}
	};

	// Table display
	ImGui::Text("Thread %u - Hierarchical Breakdown", uThreadID);
	ImGui::Separator();
	
	if (ImGui::BeginTable("ProfileBreakdown", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY))
	{
		ImGui::TableSetupColumn("Color", ImGuiTableColumnFlags_WidthFixed, 20.0f);
		ImGui::TableSetupColumn("Profile Name", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Total Time", ImGuiTableColumnFlags_WidthFixed, 120.0f);
		ImGui::TableSetupColumn("Self Time", ImGuiTableColumnFlags_WidthFixed, 120.0f);
		ImGui::TableSetupColumn("% of Frame", ImGuiTableColumnFlags_WidthFixed, 100.0f);
		ImGui::TableSetupColumn("Call Count", ImGuiTableColumnFlags_WidthFixed, 100.0f);
		ImGui::TableHeadersRow();

		for (u_int i = 0; i < xRootNodes.GetSize(); ++i)
		{
			RenderNode(xRootNodes.Get(i), 0);
		}

		ImGui::EndTable();
	}
}
#endif

void Zenith_Profiling::BeginProfile(const Zenith_ProfileIndex eIndex)
{
	if (g_bIsPaused) return;

	Zenith_Assert(tl_g_uCurrentDepth < uMAX_PROFILE_DEPTH, "Profiling has nested too far");

	tl_g_aeIndices[tl_g_uCurrentDepth] = eIndex;
	tl_g_axStartPoints[tl_g_uCurrentDepth] = std::chrono::high_resolution_clock::now();

	tl_g_uCurrentDepth++;
}

void Zenith_Profiling::EndProfile(const Zenith_ProfileIndex eIndex)
{
	if (g_bIsPaused) return;

	Zenith_Assert(tl_g_uCurrentDepth > 0, "Ending profiling but it never started");
	Zenith_Assert(tl_g_aeIndices[tl_g_uCurrentDepth - 1] == eIndex, "Expecting to end profile index %u but %u was found", eIndex, tl_g_aeIndices[tl_g_uCurrentDepth]);

	tl_g_uCurrentDepth--;

	tl_g_axEndPoints[tl_g_uCurrentDepth] = std::chrono::high_resolution_clock::now();
	const Event& xEvent = {tl_g_axStartPoints[tl_g_uCurrentDepth], tl_g_axEndPoints[tl_g_uCurrentDepth], tl_g_aeIndices[tl_g_uCurrentDepth], tl_g_uCurrentDepth};
	g_xEvents.at(Zenith_Multithreading::GetCurrentThreadID()).PushBack(xEvent);

	tl_g_aeIndices[tl_g_uCurrentDepth] = ZENITH_PROFILE_INDEX__TOTAL_FRAME;
}

const Zenith_ProfileIndex Zenith_Profiling::GetCurrentIndex()
{
	Zenith_Assert(tl_g_uCurrentDepth > 0, "Trying to get profiling index but nothing is being profiled");
	return tl_g_aeIndices[tl_g_uCurrentDepth - 1];
}

const std::unordered_map<u_int, Zenith_Vector<Zenith_Profiling::Event>>& Zenith_Profiling::GetEvents()
{
	return g_xEvents;
}
