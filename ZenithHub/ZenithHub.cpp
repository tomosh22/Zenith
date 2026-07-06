#include "Zenith.h"
#pragma warning(disable: 4005) // APIENTRY macro redefinition

#include "Core/Zenith_GraphicsOptions.h"
#include "ZenithHub_GameScan.h"
#include "ZenithHub_Process.h"
#include "ZenithHub_SelfTest.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

// ============================================================================
// Standalone-tool Project_* stubs. The hub has its own main() and never runs the
// engine loop, but it links zenith.lib -- whose Zenith_Main / Zenith_Engine objs
// reference the full Project_* contract -- so the whole set must resolve as
// (never-called) symbols.
// ============================================================================
const char* Project_GetGameAssetsDirectory() { return ""; }
const char* Project_GetName() { return "ZenithHub"; }
void Project_SetGraphicsOptions(Zenith_GraphicsOptions&) {}
void Project_RegisterGameComponents() {}
void Project_Shutdown() {}
void Project_InitializeResources() {}
void Project_RegisterEditorAutomationSteps() {}
void Project_LoadInitialScene() {}

// ============================================================================
// DX11 globals + helpers (cloned from TilePuzzleRegistryViewer).
// ============================================================================
static ID3D11Device*            g_pxDevice = nullptr;
static ID3D11DeviceContext*     g_pxDeviceContext = nullptr;
static IDXGISwapChain*          g_pxSwapChain = nullptr;
static bool                     g_bSwapChainOccluded = false;
static UINT                     g_uResizeWidth = 0, g_uResizeHeight = 0;
static ID3D11RenderTargetView*  g_pxRenderTargetView = nullptr;

static void CreateRenderTarget()
{
	ID3D11Texture2D* pxBackBuffer;
	g_pxSwapChain->GetBuffer(0, IID_PPV_ARGS(&pxBackBuffer));
	g_pxDevice->CreateRenderTargetView(pxBackBuffer, nullptr, &g_pxRenderTargetView);
	pxBackBuffer->Release();
}

static void CleanupRenderTarget()
{
	if (g_pxRenderTargetView) { g_pxRenderTargetView->Release(); g_pxRenderTargetView = nullptr; }
}

static bool CreateDeviceD3D(HWND hWnd)
{
	DXGI_SWAP_CHAIN_DESC xDesc;
	ZeroMemory(&xDesc, sizeof(xDesc));
	xDesc.BufferCount = 2;
	xDesc.BufferDesc.Width = 0;
	xDesc.BufferDesc.Height = 0;
	xDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	xDesc.BufferDesc.RefreshRate.Numerator = 60;
	xDesc.BufferDesc.RefreshRate.Denominator = 1;
	xDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	xDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	xDesc.OutputWindow = hWnd;
	xDesc.SampleDesc.Count = 1;
	xDesc.SampleDesc.Quality = 0;
	xDesc.Windowed = TRUE;
	xDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	UINT uCreateFlags = 0;
	D3D_FEATURE_LEVEL eFeatureLevel;
	const D3D_FEATURE_LEVEL aeFeatureLevels[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
	HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, uCreateFlags, aeFeatureLevels, 2, D3D11_SDK_VERSION, &xDesc, &g_pxSwapChain, &g_pxDevice, &eFeatureLevel, &g_pxDeviceContext);
	if (hr == DXGI_ERROR_UNSUPPORTED)
		hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, uCreateFlags, aeFeatureLevels, 2, D3D11_SDK_VERSION, &xDesc, &g_pxSwapChain, &g_pxDevice, &eFeatureLevel, &g_pxDeviceContext);
	if (hr != S_OK)
		return false;

	IDXGIFactory* pxFactory;
	if (SUCCEEDED(g_pxSwapChain->GetParent(IID_PPV_ARGS(&pxFactory))))
	{
		pxFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);
		pxFactory->Release();
	}

	CreateRenderTarget();
	return true;
}

static void CleanupDeviceD3D()
{
	CleanupRenderTarget();
	if (g_pxSwapChain) { g_pxSwapChain->Release(); g_pxSwapChain = nullptr; }
	if (g_pxDeviceContext) { g_pxDeviceContext->Release(); g_pxDeviceContext = nullptr; }
	if (g_pxDevice) { g_pxDevice->Release(); g_pxDevice = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LRESULT WINAPI WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
		return true;

	switch (uMsg)
	{
	case WM_SIZE:
		if (wParam == SIZE_MINIMIZED)
			return 0;
		g_uResizeWidth = (UINT)LOWORD(lParam);
		g_uResizeHeight = (UINT)HIWORD(lParam);
		return 0;
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU)
			return 0;
		break;
	case WM_DESTROY:
		::PostQuitMessage(0);
		return 0;
	}
	return ::DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

// ============================================================================
// Hub state
// ============================================================================
static const char*             s_szRoot = ZENITH_ROOT;
static std::vector<HubGame>     s_axGames;
static HubJob                   s_xJob;
static char                     s_szStatus[256] = "";
static std::vector<std::string> s_astrTemplates;

// New Game modal state.
static char s_szNewName[64] = "";
static bool s_bNewAndroid = false;
static int  s_iNewTemplate = 0;

static void RefreshGames()
{
	ZenithHub_GameScan::ScanGames(s_szRoot, s_axGames);
}

static void ScanTemplates()
{
	s_astrTemplates.clear();
	namespace fs = std::filesystem;
	std::error_code xEc;
	fs::path xDir = fs::path(s_szRoot) / "Build" / "Templates";
	if (fs::exists(xDir, xEc))
	{
		for (const auto& xE : fs::directory_iterator(xDir, xEc))
		{
			if (xE.is_directory())
			{
				s_astrTemplates.push_back(xE.path().filename().string());
			}
		}
	}
	if (s_astrTemplates.empty()) { s_astrTemplates.push_back("NewGame"); }
	std::sort(s_astrTemplates.begin(), s_astrTemplates.end());
}

static bool NameCollides(const char* szName)
{
	std::string strLower(szName);
	for (char& c : strLower) { c = (char)tolower((unsigned char)c); }
	for (const HubGame& xGame : s_axGames)
	{
		std::string strExisting = xGame.strName;
		for (char& c : strExisting) { c = (char)tolower((unsigned char)c); }
		if (strExisting == strLower) { return true; }
	}
	return false;
}

static std::wstring Widen(const std::string& str)
{
	if (str.empty()) { return std::wstring(); }
	int iLen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
	std::wstring strW(iLen, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &strW[0], iLen);
	return strW;
}

static void StartMutation(const std::wstring& strCliArgs, const std::string& strLabel)
{
	if (s_xJob.bRunning) { return; }   // one mutation at a time
	if (ZenithHub_Process::StartJob(s_xJob, s_szRoot, strCliArgs, strLabel))
	{
		snprintf(s_szStatus, sizeof(s_szStatus), "Running: %s ...", strLabel.c_str());
	}
	else
	{
		snprintf(s_szStatus, sizeof(s_szStatus), "Failed to start: %s", strLabel.c_str());
	}
}

// ============================================================================
// UI
// ============================================================================
static void DrawHubUI()
{
	const ImGuiViewport* pxViewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(pxViewport->WorkPos);
	ImGui::SetNextWindowSize(pxViewport->WorkSize);
	ImGui::Begin("Zenith Hub", nullptr,
		ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings);

	// Header controls.
	const bool bBusy = s_xJob.bRunning;
	ImGui::BeginDisabled(bBusy);
	if (ImGui::Button("New Game"))
	{
		s_szNewName[0] = '\0';
		s_bNewAndroid = false;
		s_iNewTemplate = 0;
		ImGui::OpenPopup("New Game");
	}
	ImGui::SameLine();
	if (ImGui::Button("Regen"))
	{
		StartMutation(L"regen", "regen");
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Rescan"))
	{
		RefreshGames();
	}
	ImGui::SameLine();
	if (bBusy) { ImGui::Text("[working] %s", s_szStatus); }
	else if (s_szStatus[0]) { ImGui::TextDisabled("%s", s_szStatus); }

	ImGui::Separator();
	ImGui::Text("%zu games", s_axGames.size());

	// Game table.
	const ImGuiTableFlags uFlags =
		ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
		ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV;
	if (ImGui::BeginTable("Games", 5, uFlags, ImVec2(0, 0)))
	{
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 180.0f);
		ImGui::TableSetupColumn("Android", ImGuiTableColumnFlags_WidthFixed, 70.0f);
		ImGui::TableSetupColumn("Built configs", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Newest build", ImGuiTableColumnFlags_WidthFixed, 140.0f);
		ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 130.0f);
		ImGui::TableHeadersRow();

		for (size_t i = 0; i < s_axGames.size(); ++i)
		{
			const HubGame& xGame = s_axGames[i];
			ImGui::TableNextRow();
			ImGui::PushID((int)i);

			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(xGame.strName.c_str());

			ImGui::TableSetColumnIndex(1);
			ImGui::TextUnformatted(xGame.bAndroid ? "Yes" : "No");

			ImGui::TableSetColumnIndex(2);
			ImGui::TextUnformatted(xGame.strBuiltConfigs.empty() ? "(none)" : xGame.strBuiltConfigs.c_str());

			ImGui::TableSetColumnIndex(3);
			ImGui::TextUnformatted(xGame.strNewestBuild.empty() ? "-" : xGame.strNewestBuild.c_str());

			ImGui::TableSetColumnIndex(4);
			ImGui::BeginDisabled(bBusy);
			if (ImGui::SmallButton("Open"))
			{
				ZenithHub_Process::LaunchDetached(s_szRoot, L"open " + Widen(xGame.strName));
			}
			ImGui::SameLine();
			ImGui::BeginDisabled(xGame.strBuiltConfigs.empty());
			if (ImGui::SmallButton("Run"))
			{
				ZenithHub_Process::LaunchDetached(s_szRoot, L"run " + Widen(xGame.strName));
			}
			ImGui::EndDisabled();
			ImGui::EndDisabled();

			ImGui::PopID();
		}
		ImGui::EndTable();
	}

	// New Game modal.
	if (ImGui::BeginPopupModal("New Game", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::InputText("Name", s_szNewName, sizeof(s_szNewName));
		const bool bSyntaxOk = ZenithHub_GameScan::ValidateName(s_szNewName);
		const bool bCollides = bSyntaxOk && NameCollides(s_szNewName);
		if (!bSyntaxOk && s_szNewName[0])
		{
			ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Invalid name (PascalCase, ASCII, <=64, not reserved).");
		}
		else if (bCollides)
		{
			ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "A game with that name already exists.");
		}
		else if (bSyntaxOk)
		{
			ImGui::TextColored(ImVec4(0.4f, 1, 0.4f, 1), "OK");
		}

		// Template dropdown.
		if (ImGui::BeginCombo("Template", s_astrTemplates.empty() ? "NewGame" : s_astrTemplates[s_iNewTemplate].c_str()))
		{
			for (int t = 0; t < (int)s_astrTemplates.size(); ++t)
			{
				bool bSel = (t == s_iNewTemplate);
				if (ImGui::Selectable(s_astrTemplates[t].c_str(), bSel)) { s_iNewTemplate = t; }
			}
			ImGui::EndCombo();
		}

		ImGui::Checkbox("Android", &s_bNewAndroid);

		const bool bCanCreate = bSyntaxOk && !bCollides;
		ImGui::BeginDisabled(!bCanCreate);
		if (ImGui::Button("Create"))
		{
			std::wstring strArgs = L"new " + Widen(s_szNewName);
			if (s_iNewTemplate < (int)s_astrTemplates.size())
			{
				strArgs += L" --template " + Widen(s_astrTemplates[s_iNewTemplate]);
			}
			if (!s_bNewAndroid) { strArgs += L" --no-android"; }
			strArgs += L" --no-open";
			StartMutation(strArgs, std::string("new ") + s_szNewName);
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) { ImGui::CloseCurrentPopup(); }
		ImGui::EndPopup();
	}

	ImGui::End();
}

// ============================================================================
// Main
// ============================================================================
int main(int argc, char** argv)
{
	// Minimal engine init (the hub links the engine lib; keep it happy).
	Zenith_MemoryManagement::Initialise();

	bool bSelfTest = false;
	for (int i = 1; i < argc; ++i)
	{
		if (strcmp(argv[i], "--selftest") == 0) { bSelfTest = true; }
	}
	if (bSelfTest)
	{
		return ZenithHub_SelfTest::Run(s_szRoot);
	}

	ScanTemplates();
	RefreshGames();

	// Window + D3D11.
	ImGui_ImplWin32_EnableDpiAwareness();
	float fScale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

	WNDCLASSEXW xWndClass = { sizeof(xWndClass), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ZenithHub", nullptr };
	::RegisterClassExW(&xWndClass);
	HWND hWnd = ::CreateWindowW(xWndClass.lpszClassName, L"Zenith Hub", WS_OVERLAPPEDWINDOW, 100, 100, (int)(1100 * fScale), (int)(720 * fScale), nullptr, nullptr, xWndClass.hInstance, nullptr);

	if (!CreateDeviceD3D(hWnd))
	{
		CleanupDeviceD3D();
		::UnregisterClassW(xWndClass.lpszClassName, xWndClass.hInstance);
		return 1;
	}

	::ShowWindow(hWnd, SW_SHOWDEFAULT);
	::UpdateWindow(hWnd);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& xIO = ImGui::GetIO();
	xIO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	xIO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	ImGui::StyleColorsDark();
	ImGuiStyle& xStyle = ImGui::GetStyle();
	xStyle.ScaleAllSizes(fScale);
	xStyle.FontScaleDpi = fScale;
	xIO.ConfigDpiScaleFonts = true;

	ImGui_ImplWin32_Init(hWnd);
	ImGui_ImplDX11_Init(g_pxDevice, g_pxDeviceContext);

	ImVec4 xClearColor = ImVec4(0.10f, 0.10f, 0.12f, 1.0f);

	bool bDone = false;
	while (!bDone)
	{
		MSG xMsg;
		while (::PeekMessage(&xMsg, nullptr, 0U, 0U, PM_REMOVE))
		{
			::TranslateMessage(&xMsg);
			::DispatchMessage(&xMsg);
			if (xMsg.message == WM_QUIT)
				bDone = true;
		}
		if (bDone)
			break;

		if (g_bSwapChainOccluded && g_pxSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
		{
			::Sleep(10);
			continue;
		}
		g_bSwapChainOccluded = false;

		if (g_uResizeWidth != 0 && g_uResizeHeight != 0)
		{
			CleanupRenderTarget();
			g_pxSwapChain->ResizeBuffers(0, g_uResizeWidth, g_uResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
			g_uResizeWidth = g_uResizeHeight = 0;
			CreateRenderTarget();
		}

		// Poll the running mutation job; refresh the tree when it completes.
		if (s_xJob.bRunning)
		{
			ZenithHub_Process::PollJob(s_xJob);
			if (!s_xJob.bRunning)
			{
				snprintf(s_szStatus, sizeof(s_szStatus), "%s: exit %d", s_xJob.strLabel.c_str(), s_xJob.iExitCode);
				RefreshGames();
			}
		}

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		DrawHubUI();

		ImGui::Render();
		const float afClearColor[4] = { xClearColor.x * xClearColor.w, xClearColor.y * xClearColor.w, xClearColor.z * xClearColor.w, xClearColor.w };
		g_pxDeviceContext->OMSetRenderTargets(1, &g_pxRenderTargetView, nullptr);
		g_pxDeviceContext->ClearRenderTargetView(g_pxRenderTargetView, afClearColor);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		HRESULT hr = g_pxSwapChain->Present(1, 0);
		g_bSwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
	}

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	CleanupDeviceD3D();
	::DestroyWindow(hWnd);
	::UnregisterClassW(xWndClass.lpszClassName, xWndClass.hInstance);

	return 0;
}
