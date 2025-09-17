#include "Zenith.h"

#include "Profiling/Zenith_Profiling.h"

#include "DebugVariables/Zenith_DebugVariables.h"
#include "Flux/Flux.h"
#include "Multithreading/Zenith_Multithreading.h"

#include <chrono>

static constexpr u_int uMAX_PROFILE_DEPTH = 16;
thread_local static u_int tl_g_uCurrentDepth;
thread_local static Zenith_ProfileIndex tl_g_aeIndices[uMAX_PROFILE_DEPTH];
thread_local static std::chrono::time_point<std::chrono::high_resolution_clock> tl_g_axStartPoints[uMAX_PROFILE_DEPTH];
thread_local static std::chrono::time_point<std::chrono::high_resolution_clock> tl_g_axEndPoints[uMAX_PROFILE_DEPTH];
static std::unordered_map<u_int, Zenith_Vector<Zenith_Profiling::Event>> g_xEvents;
static std::chrono::time_point<std::chrono::high_resolution_clock> g_xFrameStart;
static std::chrono::time_point<std::chrono::high_resolution_clock> g_xFrameEnd;

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
	static int ls_iMaxDepthToRenderSeparately = 1;
	static float ls_fTimelineZoom = 1.0f;
	static float ls_fTimelineScroll = 0.0f;
	static float ls_fVerticalScale = 1.0f;

	ImGui::SliderInt("Min Depth to Render", &ls_iMinDepthToRender, 0, 10);
	ImGui::SliderInt("Max Depth to Render", &ls_iMaxDepthToRender, 0, 20);
	ImGui::SliderInt("Max Depth to Render Separately", &ls_iMaxDepthToRenderSeparately, 0, 20);
	ImGui::SliderFloat("Zoom (X)", &ls_fTimelineZoom, 0.1f, 100.0f, "%.1fx");
	ImGui::SliderFloat("Scroll (X)", &ls_fTimelineScroll, 0.0f, 100000.0f, "%.0f px");
	ImGui::SliderFloat("Vertical Scale", &ls_fVerticalScale, 0.5f, 4.0f, "%.1fx");
	ImGui::Checkbox("Paused", &dbg_bPaused);

	ls_iMaxDepthToRender = std::max(ls_iMaxDepthToRender, ls_iMinDepthToRender);
	ls_iMaxDepthToRenderSeparately = std::clamp(ls_iMaxDepthToRenderSeparately, ls_iMinDepthToRender, ls_iMaxDepthToRender);

	constexpr float fBASE_ROW_HEIGHT = 20.0f;
	constexpr float fBASE_ROW_SPACING = 5.0f;
	constexpr float fTHREAD_SPACING = 30.0f;

	const float fRowHeight = fBASE_ROW_HEIGHT * ls_fVerticalScale;
	const float fRowSpacing = fBASE_ROW_SPACING * ls_fVerticalScale;

	const u_int uSeparateRowCount = ls_iMaxDepthToRenderSeparately - ls_iMinDepthToRender + 1;
	const u_int uRowsPerThread = uSeparateRowCount;
	const float fThreadHeight = uRowsPerThread * (fRowHeight + fRowSpacing) + fTHREAD_SPACING;

	const float fCanvasWidth = ImGui::GetContentRegionAvail().x;
	const float fTotalHeight = static_cast<float>(g_xEvents.size()) * fThreadHeight;

	ImGui::Dummy(ImVec2(fCanvasWidth, fTotalHeight));
	ImDrawList* const pxDrawList = ImGui::GetWindowDrawList();
	const ImVec2 xCanvasPos = ImGui::GetItemRectMin();

	const float fFrameDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(g_xFrameEnd - g_xFrameStart).count();

	for (const auto& [uThreadID, xEvents] : g_xEvents)
	{
		const float fThreadBaseY = xCanvasPos.y + uThreadID * fThreadHeight;

		char acLabel[64];
		snprintf(acLabel, sizeof(acLabel), "Thread %u", uThreadID);
		pxDrawList->AddText(ImVec2(xCanvasPos.x, fThreadBaseY), IM_COL32_WHITE, acLabel);

		for (u_int u = 0; u < xEvents.GetSize(); u++)
		{
			const Event& xEvent = xEvents.Get(xEvents.GetSize() - u - 1);

			if (xEvent.m_uDepth < ls_iMinDepthToRender || xEvent.m_uDepth > ls_iMaxDepthToRender)
				continue;

			const u_int uRowIndex = (xEvent.m_uDepth <= ls_iMaxDepthToRenderSeparately)
				? (xEvent.m_uDepth - ls_iMinDepthToRender)
				: (ls_iMaxDepthToRenderSeparately - ls_iMinDepthToRender);

			const float fStartPx = ((std::chrono::duration_cast<std::chrono::nanoseconds>(xEvent.m_xBegin - g_xFrameStart).count() / fFrameDuration) * fCanvasWidth * ls_fTimelineZoom) - ls_fTimelineScroll;
			const float fEndPx = ((std::chrono::duration_cast<std::chrono::nanoseconds>(xEvent.m_xEnd - g_xFrameStart).count() / fFrameDuration) * fCanvasWidth * ls_fTimelineZoom) - ls_fTimelineScroll;

			const ImVec2 xRectMin = ImVec2(xCanvasPos.x + fStartPx, fThreadBaseY + uRowIndex * (fRowHeight + fRowSpacing));
			const ImVec2 xRectMax = ImVec2(xCanvasPos.x + fEndPx, xRectMin.y + fRowHeight);

			Zenith_Maths::Vector3 xColour = IntToColour(xEvent.m_eIndex) * 255.f;
			const ImU32 xColor = IM_COL32((int)xColour.r, (int)xColour.g, (int)xColour.b, 255);
			pxDrawList->AddRectFilled(xRectMin, xRectMax, xColor, 3.0f);

			if (ImGui::CalcTextSize(g_aszProfileNames[xEvent.m_eIndex]).x <= (xRectMax.x - xRectMin.x))
			{
				pxDrawList->AddText(xRectMin, IM_COL32_WHITE, g_aszProfileNames[xEvent.m_eIndex]);
			}
		}
	}
	ImGui::End();
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
