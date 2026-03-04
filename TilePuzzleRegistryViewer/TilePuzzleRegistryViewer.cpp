#include "Zenith.h"
#pragma warning(disable: 4005) // APIENTRY macro redefinition
#include "Core/Memory/Zenith_MemoryManagement_Disabled.h"

#include "TilePuzzleLevelMetadata.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <filesystem>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <ctime>
#include <cstring>

// ============================================================================
// Stubs for standalone tool (same pattern as TilePuzzleLevelGen)
// ============================================================================
const char* Project_GetGameAssetsDirectory() { return ""; }
const char* Project_GetName() { return "TilePuzzleRegistryViewer"; }

#ifdef ZENITH_TOOLS
void Zenith_EditorAddLogMessage(const char*, int, Zenith_LogCategory) {}
#endif

// ============================================================================
// DX11 globals
// ============================================================================
static ID3D11Device*            g_pxDevice = nullptr;
static ID3D11DeviceContext*     g_pxDeviceContext = nullptr;
static IDXGISwapChain*          g_pxSwapChain = nullptr;
static bool                     g_bSwapChainOccluded = false;
static UINT                     g_uResizeWidth = 0, g_uResizeHeight = 0;
static ID3D11RenderTargetView*  g_pxRenderTargetView = nullptr;

// ============================================================================
// DX11 helpers
// ============================================================================
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
// Texture loading (stb_image -> DX11 SRV)
// ============================================================================
static ID3D11ShaderResourceView* LoadTextureFromFile(const char* szPath, int& iWidthOut, int& iHeightOut)
{
	int iChannels;
	unsigned char* pPixels = stbi_load(szPath, &iWidthOut, &iHeightOut, &iChannels, 4);
	if (!pPixels)
		return nullptr;

	D3D11_TEXTURE2D_DESC xTexDesc;
	ZeroMemory(&xTexDesc, sizeof(xTexDesc));
	xTexDesc.Width = iWidthOut;
	xTexDesc.Height = iHeightOut;
	xTexDesc.MipLevels = 1;
	xTexDesc.ArraySize = 1;
	xTexDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	xTexDesc.SampleDesc.Count = 1;
	xTexDesc.Usage = D3D11_USAGE_DEFAULT;
	xTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA xSubResource;
	xSubResource.pSysMem = pPixels;
	xSubResource.SysMemPitch = iWidthOut * 4;
	xSubResource.SysMemSlicePitch = 0;

	ID3D11Texture2D* pxTexture = nullptr;
	HRESULT hr = g_pxDevice->CreateTexture2D(&xTexDesc, &xSubResource, &pxTexture);
	stbi_image_free(pPixels);

	if (FAILED(hr))
		return nullptr;

	D3D11_SHADER_RESOURCE_VIEW_DESC xSrvDesc;
	ZeroMemory(&xSrvDesc, sizeof(xSrvDesc));
	xSrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	xSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	xSrvDesc.Texture2D.MipLevels = 1;

	ID3D11ShaderResourceView* pxSRV = nullptr;
	hr = g_pxDevice->CreateShaderResourceView(pxTexture, &xSrvDesc, &pxSRV);
	pxTexture->Release();

	if (FAILED(hr))
		return nullptr;

	return pxSRV;
}

// ============================================================================
// Registry viewer data
// ============================================================================
static constexpr float s_fThumbnailSize = 80.0f;

struct ViewerEntry
{
	char szFileName[128];
	char szFilePath[512];
	char szPngPath[512];
	TilePuzzleLevelMetadata xMeta;
	ID3D11ShaderResourceView* pxTextureSRV;
	int iImageWidth;
	int iImageHeight;
};

static std::vector<ViewerEntry> s_axEntries;
static bool s_bNeedsSort = true;

static void FreeAllTextures()
{
	for (auto& xEntry : s_axEntries)
	{
		if (xEntry.pxTextureSRV)
		{
			xEntry.pxTextureSRV->Release();
			xEntry.pxTextureSRV = nullptr;
		}
	}
}

static void ScanRegistry(const char* szRegistryDir)
{
	FreeAllTextures();
	s_axEntries.clear();

	if (!std::filesystem::exists(szRegistryDir))
	{
		printf("Registry directory does not exist: %s\n", szRegistryDir);
		return;
	}

	for (const auto& xEntry : std::filesystem::directory_iterator(szRegistryDir))
	{
		if (!xEntry.is_regular_file())
			continue;
		if (xEntry.path().extension() != ".tlvl")
			continue;

		ViewerEntry xViewer;
		memset(&xViewer, 0, sizeof(xViewer));

		std::string strPath = xEntry.path().string();
		std::string strName = xEntry.path().filename().string();

		snprintf(xViewer.szFilePath, sizeof(xViewer.szFilePath), "%s", strPath.c_str());
		snprintf(xViewer.szFileName, sizeof(xViewer.szFileName), "%s", strName.c_str());

		// Build PNG path (same name, .png extension)
		std::filesystem::path xPngPath = xEntry.path();
		xPngPath.replace_extension(".png");
		snprintf(xViewer.szPngPath, sizeof(xViewer.szPngPath), "%s", xPngPath.string().c_str());

		if (!ReadMetadataFromFile(xViewer.szFilePath, xViewer.xMeta))
			continue;

		// Load PNG texture
		if (std::filesystem::exists(xPngPath))
		{
			xViewer.pxTextureSRV = LoadTextureFromFile(xViewer.szPngPath, xViewer.iImageWidth, xViewer.iImageHeight);
		}

		s_axEntries.push_back(xViewer);
	}

	printf("Loaded %zu registry entries\n", s_axEntries.size());
	s_bNeedsSort = true;
}

// ============================================================================
// Sorting
// ============================================================================
enum SortColumn
{
	SORT_PREVIEW = 0,
	SORT_HASH,
	SORT_PAR_MOVES,
	SORT_GRID_W,
	SORT_GRID_H,
	SORT_COLORS,
	SORT_CATS_PER_COLOR,
	SORT_TOTAL_CATS,
	SORT_DRAGGABLE_SHAPES,
	SORT_BLOCKERS,
	SORT_BLOCKER_CATS,
	SORT_CONDITIONALS,
	SORT_MAX_THRESHOLD,
	SORT_MIN_COMPLEXITY,
	SORT_MAX_COMPLEXITY,
	SORT_FLOOR_CELLS,
	SORT_GEN_TIME,
	SORT_TIMESTAMP,
	SORT_TIMED_OUT,
	SORT_COLUMN_COUNT
};

static ImGuiSortDirection s_eSortDirection = ImGuiSortDirection_Ascending;
static int s_iSortColumn = SORT_PAR_MOVES;

static int CompareEntries(const ViewerEntry& a, const ViewerEntry& b)
{
	const TilePuzzleLevelMetadata& xA = a.xMeta;
	const TilePuzzleLevelMetadata& xB = b.xMeta;

	int iResult = 0;
	switch (s_iSortColumn)
	{
	case SORT_HASH:             iResult = (xA.ulLayoutHash < xB.ulLayoutHash) ? -1 : (xA.ulLayoutHash > xB.ulLayoutHash) ? 1 : 0; break;
	case SORT_PAR_MOVES:        iResult = (int)xA.uParMoves - (int)xB.uParMoves; break;
	case SORT_GRID_W:           iResult = (int)xA.uGridWidth - (int)xB.uGridWidth; break;
	case SORT_GRID_H:           iResult = (int)xA.uGridHeight - (int)xB.uGridHeight; break;
	case SORT_COLORS:           iResult = (int)xA.uNumColors - (int)xB.uNumColors; break;
	case SORT_CATS_PER_COLOR:   iResult = (int)xA.uNumCatsPerColor - (int)xB.uNumCatsPerColor; break;
	case SORT_TOTAL_CATS:       iResult = (int)xA.uTotalCats - (int)xB.uTotalCats; break;
	case SORT_DRAGGABLE_SHAPES: iResult = (int)xA.uNumDraggableShapes - (int)xB.uNumDraggableShapes; break;
	case SORT_BLOCKERS:         iResult = (int)xA.uNumStaticBlockers - (int)xB.uNumStaticBlockers; break;
	case SORT_BLOCKER_CATS:     iResult = (int)xA.uNumBlockerCats - (int)xB.uNumBlockerCats; break;
	case SORT_CONDITIONALS:     iResult = (int)xA.uNumConditionalShapes - (int)xB.uNumConditionalShapes; break;
	case SORT_MAX_THRESHOLD:    iResult = (int)xA.uMaxConditionalThreshold - (int)xB.uMaxConditionalThreshold; break;
	case SORT_MIN_COMPLEXITY:   iResult = (int)xA.uMinShapeComplexity - (int)xB.uMinShapeComplexity; break;
	case SORT_MAX_COMPLEXITY:   iResult = (int)xA.uMaxShapeComplexity - (int)xB.uMaxShapeComplexity; break;
	case SORT_FLOOR_CELLS:      iResult = (int)xA.uNumFloorCells - (int)xB.uNumFloorCells; break;
	case SORT_GEN_TIME:         iResult = (int)xA.uGenerationTimeMs - (int)xB.uGenerationTimeMs; break;
	case SORT_TIMESTAMP:        iResult = (xA.ulGenerationTimestamp < xB.ulGenerationTimestamp) ? -1 : (xA.ulGenerationTimestamp > xB.ulGenerationTimestamp) ? 1 : 0; break;
	case SORT_TIMED_OUT:        iResult = (int)xA.bGenerationTimedOut - (int)xB.bGenerationTimedOut; break;
	}

	return (s_eSortDirection == ImGuiSortDirection_Descending) ? -iResult : iResult;
}

// ============================================================================
// Filters
// ============================================================================
struct ViewerFilters
{
	int iMinParMoves = 0;
	int iMaxParMoves = 999;
	int iMinGridW = 0;
	int iMaxGridW = 99;
	int iMinGridH = 0;
	int iMaxGridH = 99;
	int iMinColors = 0;
	int iMaxColors = 99;
};

static ViewerFilters s_xFilters;

static bool PassesFilter(const ViewerEntry& xEntry)
{
	const TilePuzzleLevelMetadata& xM = xEntry.xMeta;
	if ((int)xM.uParMoves < s_xFilters.iMinParMoves || (int)xM.uParMoves > s_xFilters.iMaxParMoves) return false;
	if ((int)xM.uGridWidth < s_xFilters.iMinGridW || (int)xM.uGridWidth > s_xFilters.iMaxGridW) return false;
	if ((int)xM.uGridHeight < s_xFilters.iMinGridH || (int)xM.uGridHeight > s_xFilters.iMaxGridH) return false;
	if ((int)xM.uNumColors < s_xFilters.iMinColors || (int)xM.uNumColors > s_xFilters.iMaxColors) return false;
	return true;
}

// ============================================================================
// Main
// ============================================================================
int main(int argc, char** argv)
{
	// Parse CLI
	const char* szRegistryDir = LEVELGEN_REGISTRY_DIR;
	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "--registry") == 0 && i + 1 < argc)
		{
			szRegistryDir = argv[++i];
		}
	}

	printf("TilePuzzle Registry Viewer\n");
	printf("Registry: %s\n", szRegistryDir);

	// Create application window
	ImGui_ImplWin32_EnableDpiAwareness();
	float fScale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

	WNDCLASSEXW xWndClass = { sizeof(xWndClass), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"TilePuzzleRegistryViewer", nullptr };
	::RegisterClassExW(&xWndClass);
	HWND hWnd = ::CreateWindowW(xWndClass.lpszClassName, L"TilePuzzle Registry Viewer", WS_OVERLAPPEDWINDOW, 100, 100, (int)(1600 * fScale), (int)(900 * fScale), nullptr, nullptr, xWndClass.hInstance, nullptr);

	if (!CreateDeviceD3D(hWnd))
	{
		CleanupDeviceD3D();
		::UnregisterClassW(xWndClass.lpszClassName, xWndClass.hInstance);
		return 1;
	}

	::ShowWindow(hWnd, SW_SHOWDEFAULT);
	::UpdateWindow(hWnd);

	// Setup ImGui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& xIO = ImGui::GetIO();
	xIO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	xIO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	xIO.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	ImGui::StyleColorsDark();
	ImGuiStyle& xStyle = ImGui::GetStyle();
	xStyle.ScaleAllSizes(fScale);
	xStyle.FontScaleDpi = fScale;
	xIO.ConfigDpiScaleFonts = true;
	xIO.ConfigDpiScaleViewports = true;

	if (xIO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		xStyle.WindowRounding = 0.0f;
		xStyle.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	ImGui_ImplWin32_Init(hWnd);
	ImGui_ImplDX11_Init(g_pxDevice, g_pxDeviceContext);

	// Load registry
	ScanRegistry(szRegistryDir);

	ImVec4 xClearColor = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);

	// Main loop
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

		// New frame
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		// Fullscreen window covering entire viewport
		const ImGuiViewport* pxViewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(pxViewport->WorkPos);
		ImGui::SetNextWindowSize(pxViewport->WorkSize);
		ImGui::Begin("Registry Viewer", nullptr,
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoBringToFrontOnFocus |
			ImGuiWindowFlags_NoSavedSettings);

		// Controls bar
		if (ImGui::Button("Refresh"))
		{
			ScanRegistry(szRegistryDir);
		}
		ImGui::SameLine();

		// Count visible
		int iVisibleCount = 0;
		for (size_t i = 0; i < s_axEntries.size(); i++)
		{
			if (PassesFilter(s_axEntries[i]))
				iVisibleCount++;
		}
		ImGui::Text("Showing %d / %zu entries", iVisibleCount, s_axEntries.size());

		// Filters
		ImGui::SameLine();
		if (ImGui::TreeNode("Filters"))
		{
			ImGui::SliderInt("Min Par Moves", &s_xFilters.iMinParMoves, 0, 100);
			ImGui::SliderInt("Max Par Moves", &s_xFilters.iMaxParMoves, 0, 100);
			ImGui::SliderInt("Min Grid W", &s_xFilters.iMinGridW, 0, 20);
			ImGui::SliderInt("Max Grid W", &s_xFilters.iMaxGridW, 0, 20);
			ImGui::SliderInt("Min Grid H", &s_xFilters.iMinGridH, 0, 20);
			ImGui::SliderInt("Max Grid H", &s_xFilters.iMaxGridH, 0, 20);
			ImGui::SliderInt("Min Colors", &s_xFilters.iMinColors, 0, 10);
			ImGui::SliderInt("Max Colors", &s_xFilters.iMaxColors, 0, 10);
			ImGui::TreePop();
		}

		// Table
		const ImGuiTableFlags uTableFlags =
			ImGuiTableFlags_Sortable |
			ImGuiTableFlags_Resizable |
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_ScrollY |
			ImGuiTableFlags_BordersOuter |
			ImGuiTableFlags_BordersInnerV |
			ImGuiTableFlags_SizingFixedFit;

		if (ImGui::BeginTable("RegistryTable", SORT_COLUMN_COUNT, uTableFlags, ImVec2(0, 0)))
		{
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn("Preview",        ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed, s_fThumbnailSize + 8.0f, SORT_PREVIEW);
			ImGui::TableSetupColumn("Hash",           ImGuiTableColumnFlags_DefaultSort, 0.0f, SORT_HASH);
			ImGui::TableSetupColumn("Par Moves",      ImGuiTableColumnFlags_None, 0.0f, SORT_PAR_MOVES);
			ImGui::TableSetupColumn("Grid W",         ImGuiTableColumnFlags_None, 0.0f, SORT_GRID_W);
			ImGui::TableSetupColumn("Grid H",         ImGuiTableColumnFlags_None, 0.0f, SORT_GRID_H);
			ImGui::TableSetupColumn("Colors",         ImGuiTableColumnFlags_None, 0.0f, SORT_COLORS);
			ImGui::TableSetupColumn("Cats/Color",     ImGuiTableColumnFlags_None, 0.0f, SORT_CATS_PER_COLOR);
			ImGui::TableSetupColumn("Total Cats",     ImGuiTableColumnFlags_None, 0.0f, SORT_TOTAL_CATS);
			ImGui::TableSetupColumn("Shapes",         ImGuiTableColumnFlags_None, 0.0f, SORT_DRAGGABLE_SHAPES);
			ImGui::TableSetupColumn("Blockers",       ImGuiTableColumnFlags_None, 0.0f, SORT_BLOCKERS);
			ImGui::TableSetupColumn("Blk Cats",       ImGuiTableColumnFlags_None, 0.0f, SORT_BLOCKER_CATS);
			ImGui::TableSetupColumn("Conditionals",   ImGuiTableColumnFlags_None, 0.0f, SORT_CONDITIONALS);
			ImGui::TableSetupColumn("Max Thresh",     ImGuiTableColumnFlags_None, 0.0f, SORT_MAX_THRESHOLD);
			ImGui::TableSetupColumn("Min Cmplx",      ImGuiTableColumnFlags_None, 0.0f, SORT_MIN_COMPLEXITY);
			ImGui::TableSetupColumn("Max Cmplx",      ImGuiTableColumnFlags_None, 0.0f, SORT_MAX_COMPLEXITY);
			ImGui::TableSetupColumn("Floor Cells",    ImGuiTableColumnFlags_None, 0.0f, SORT_FLOOR_CELLS);
			ImGui::TableSetupColumn("Gen Time",       ImGuiTableColumnFlags_None, 0.0f, SORT_GEN_TIME);
			ImGui::TableSetupColumn("Timestamp",      ImGuiTableColumnFlags_None, 0.0f, SORT_TIMESTAMP);
			ImGui::TableSetupColumn("Timed Out",      ImGuiTableColumnFlags_None, 0.0f, SORT_TIMED_OUT);
			ImGui::TableHeadersRow();

			// Handle sorting
			if (ImGuiTableSortSpecs* pxSortSpecs = ImGui::TableGetSortSpecs())
			{
				if (pxSortSpecs->SpecsDirty || s_bNeedsSort)
				{
					if (pxSortSpecs->SpecsCount > 0)
					{
						s_iSortColumn = (int)pxSortSpecs->Specs[0].ColumnUserID;
						s_eSortDirection = pxSortSpecs->Specs[0].SortDirection;
					}
					std::sort(s_axEntries.begin(), s_axEntries.end(),
						[](const ViewerEntry& a, const ViewerEntry& b) { return CompareEntries(a, b) < 0; });
					pxSortSpecs->SpecsDirty = false;
					s_bNeedsSort = false;
				}
			}

			// Build visible index list
			static std::vector<int> s_aiVisibleIndices;
			s_aiVisibleIndices.clear();
			s_aiVisibleIndices.reserve(iVisibleCount);
			for (int i = 0; i < (int)s_axEntries.size(); i++)
			{
				if (PassesFilter(s_axEntries[i]))
					s_aiVisibleIndices.push_back(i);
			}

			// Rows with clipper (fixed row height for thumbnails)
			const float fRowHeight = s_fThumbnailSize + ImGui::GetStyle().CellPadding.y * 2.0f;
			ImGuiListClipper xClipper;
			xClipper.Begin(iVisibleCount, fRowHeight);
			while (xClipper.Step())
			{
				for (int iRow = xClipper.DisplayStart; iRow < xClipper.DisplayEnd; iRow++)
				{
					const ViewerEntry& xEntry = s_axEntries[s_aiVisibleIndices[iRow]];
					const TilePuzzleLevelMetadata& xM = xEntry.xMeta;

					ImGui::TableNextRow(ImGuiTableRowFlags_None, fRowHeight);

					// Preview image
					ImGui::TableSetColumnIndex(SORT_PREVIEW);
					if (xEntry.pxTextureSRV)
					{
						// Maintain aspect ratio within thumbnail
						float fAspect = (xEntry.iImageHeight > 0) ? (float)xEntry.iImageWidth / (float)xEntry.iImageHeight : 1.0f;
						float fDispW, fDispH;
						if (fAspect >= 1.0f)
						{
							fDispW = s_fThumbnailSize;
							fDispH = s_fThumbnailSize / fAspect;
						}
						else
						{
							fDispH = s_fThumbnailSize;
							fDispW = s_fThumbnailSize * fAspect;
						}
						ImGui::Image((ImTextureID)xEntry.pxTextureSRV, ImVec2(fDispW, fDispH));
					}

					ImGui::TableSetColumnIndex(SORT_HASH);
					ImGui::Text("%016llx", (unsigned long long)xM.ulLayoutHash);

					ImGui::TableSetColumnIndex(SORT_PAR_MOVES);
					ImGui::Text("%u", xM.uParMoves);

					ImGui::TableSetColumnIndex(SORT_GRID_W);
					ImGui::Text("%u", xM.uGridWidth);

					ImGui::TableSetColumnIndex(SORT_GRID_H);
					ImGui::Text("%u", xM.uGridHeight);

					ImGui::TableSetColumnIndex(SORT_COLORS);
					ImGui::Text("%u", xM.uNumColors);

					ImGui::TableSetColumnIndex(SORT_CATS_PER_COLOR);
					ImGui::Text("%u", xM.uNumCatsPerColor);

					ImGui::TableSetColumnIndex(SORT_TOTAL_CATS);
					ImGui::Text("%u", xM.uTotalCats);

					ImGui::TableSetColumnIndex(SORT_DRAGGABLE_SHAPES);
					ImGui::Text("%u", xM.uNumDraggableShapes);

					ImGui::TableSetColumnIndex(SORT_BLOCKERS);
					ImGui::Text("%u", xM.uNumStaticBlockers);

					ImGui::TableSetColumnIndex(SORT_BLOCKER_CATS);
					ImGui::Text("%u", xM.uNumBlockerCats);

					ImGui::TableSetColumnIndex(SORT_CONDITIONALS);
					ImGui::Text("%u", xM.uNumConditionalShapes);

					ImGui::TableSetColumnIndex(SORT_MAX_THRESHOLD);
					ImGui::Text("%u", xM.uMaxConditionalThreshold);

					ImGui::TableSetColumnIndex(SORT_MIN_COMPLEXITY);
					ImGui::Text("%u", xM.uMinShapeComplexity);

					ImGui::TableSetColumnIndex(SORT_MAX_COMPLEXITY);
					ImGui::Text("%u", xM.uMaxShapeComplexity);

					ImGui::TableSetColumnIndex(SORT_FLOOR_CELLS);
					ImGui::Text("%u", xM.uNumFloorCells);

					ImGui::TableSetColumnIndex(SORT_GEN_TIME);
					ImGui::Text("%.1fs", xM.uGenerationTimeMs / 1000.0f);

					ImGui::TableSetColumnIndex(SORT_TIMESTAMP);
					{
						time_t tTime = (time_t)xM.ulGenerationTimestamp;
						struct tm xTm;
						localtime_s(&xTm, &tTime);
						char szBuf[32];
						strftime(szBuf, sizeof(szBuf), "%Y-%m-%d %H:%M", &xTm);
						ImGui::Text("%s", szBuf);
					}

					ImGui::TableSetColumnIndex(SORT_TIMED_OUT);
					ImGui::Text("%s", xM.bGenerationTimedOut ? "Yes" : "No");
				}
			}

			ImGui::EndTable();
		}

		ImGui::End();

		// Render
		ImGui::Render();
		const float afClearColor[4] = { xClearColor.x * xClearColor.w, xClearColor.y * xClearColor.w, xClearColor.z * xClearColor.w, xClearColor.w };
		g_pxDeviceContext->OMSetRenderTargets(1, &g_pxRenderTargetView, nullptr);
		g_pxDeviceContext->ClearRenderTargetView(g_pxRenderTargetView, afClearColor);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		if (xIO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}

		HRESULT hr = g_pxSwapChain->Present(1, 0);
		g_bSwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
	}

	// Cleanup
	FreeAllTextures();
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	CleanupDeviceD3D();
	::DestroyWindow(hWnd);
	::UnregisterClassW(xWndClass.lpszClassName, xWndClass.hInstance);

	return 0;
}
