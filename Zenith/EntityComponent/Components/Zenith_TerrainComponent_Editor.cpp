#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#pragma warning(disable: 4530) // C++ exception handler used without /EHsc

#ifdef ZENITH_TOOLS

#include "Zenith_TerrainComponent.h"
#include "Core/Zenith_Win32.h"
#include "imgui.h"
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_Editor_MaterialUI.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_Image.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Terrain/Flux_TerrainImpl.h"
#include "Flux/Terrain/Flux_TerrainStreamingManagerImpl.h"
#include <filesystem>
#include <vector>

#ifdef ZENITH_TESTING
#include "UnitTests/Zenith_UnitTests.h"
#endif

// Windows file dialog support
#include <commdlg.h>
#pragma comment(lib, "Comdlg32.lib")

// Terrain export functionality (extern declarations to avoid include path issues)
extern void ExportHeightmapFromPaths(const std::string& strHeightmapPath, const std::string& strOutputDir);
extern void ExportHeightmapFromMat(const Zenith_Image& xHeightmap, const std::string& strOutputDir);

//=============================================================================
// TerrainComponent Editor UI
//
// Editor-only code for the terrain component properties panel.
// Separated from runtime code to improve maintainability.
//=============================================================================

// Static state for terrain creation UI
static char s_szHeightmapPath[512] = "";
static bool s_bTerrainExportInProgress = false;
static std::string s_strTerrainExportStatus = "";

static std::filesystem::path NormalizeDirectoryPathForComparison(
	const std::filesystem::path& xPath);

//-----------------------------------------------------------------------------
// Helper function to show Windows Open File dialog for terrain textures
// Supports .ztxtr (preferred) and PNG files
//-----------------------------------------------------------------------------
static std::string ShowHeightmapOpenFileDialog()
{
	char szFilePath[MAX_PATH] = { 0 };

	OPENFILENAMEA ofn = {};
	ofn.lStructSize = sizeof(OPENFILENAMEA);
	ofn.hwndOwner = nullptr;
	ofn.lpstrFilter = "Zenith Texture (*.ztxtr)\0*.ztxtr\0PNG Files (*.png)\0*.png\0All Files (*.*)\0*.*\0";
	ofn.lpstrFile = szFilePath;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrDefExt = "ztxtr";
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

	if (GetOpenFileNameA(&ofn))
	{
		return std::string(szFilePath);
	}
	return "";
}

//-----------------------------------------------------------------------------
// Helper: Render heightmap path input with drag-drop and browse button
//-----------------------------------------------------------------------------
static void RenderHeightmapPathInput(const char* szImGuiId)
{
	ImGui::Text("Heightmap Texture:");
	ImGui::PushItemWidth(300);
	char szInputId[64];
	snprintf(szInputId, sizeof(szInputId), "##%s", szImGuiId);
	ImGui::InputText(szInputId, s_szHeightmapPath, sizeof(s_szHeightmapPath), ImGuiInputTextFlags_ReadOnly);
	ImGui::PopItemWidth();

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_TEXTURE))
		{
			const DragDropFilePayload* pFilePayload =
				static_cast<const DragDropFilePayload*>(pPayload->Data);
			strncpy_s(s_szHeightmapPath, sizeof(s_szHeightmapPath), pFilePayload->m_szFilePath, _TRUNCATE);
			Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Dropped heightmap: %s", s_szHeightmapPath);
		}
		ImGui::EndDragDropTarget();
	}

	ImGui::SameLine();
	char szBrowseId[64];
	snprintf(szBrowseId, sizeof(szBrowseId), "Browse...##%s", szImGuiId);
	if (ImGui::Button(szBrowseId))
	{
		std::string strPath = ShowHeightmapOpenFileDialog();
		if (!strPath.empty())
		{
			strncpy_s(s_szHeightmapPath, sizeof(s_szHeightmapPath), strPath.c_str(), _TRUNCATE);
			Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Selected heightmap: %s", s_szHeightmapPath);
		}
	}
}

//-----------------------------------------------------------------------------
// Helper: Render terrain export status with color coding
//-----------------------------------------------------------------------------
static void RenderTerrainStatusDisplay()
{
	if (s_strTerrainExportStatus.empty())
		return;

	ImGui::Separator();
	if (s_bTerrainExportInProgress)
	{
		ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%s", s_strTerrainExportStatus.c_str());
	}
	else if (s_strTerrainExportStatus.find("success") != std::string::npos ||
	         s_strTerrainExportStatus.find("complete") != std::string::npos)
	{
		ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s", s_strTerrainExportStatus.c_str());
	}
	else
	{
		ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", s_strTerrainExportStatus.c_str());
	}
}

//-----------------------------------------------------------------------------
// RenderPropertiesPanel - Main editor UI. Delegates to per-section helpers so
// each section stays focused on its own concern.
//-----------------------------------------------------------------------------
void Zenith_TerrainComponent::RenderPropertiesPanel()
{
	if (!ImGui::CollapsingHeader("Terrain Component", ImGuiTreeNodeFlags_DefaultOpen))
		return;

	const bool bTerrainInitialized = m_pxStreamingState->m_ulUnifiedVertexBufferSize > 0;

	// Sculpting / painting lives in the dedicated terrain editor. Routed
	// through Zenith_Editor (not the terrain-editor header) — this TU sits in
	// the EntityComponent layer, which may not include Editor/TerrainEditor.
	// Keep this available for fresh components: the dedicated editor is where
	// the first validated, staged asset set is chosen before BakeFull commits it.
	if (ImGui::Button("Open Terrain Editor", ImVec2(200, 28)))
	{
		g_xEngine.Editor().OpenTerrainEditor(m_xParentEntity.GetEntityID());
	}
	ImGui::Separator();

	if (!bTerrainInitialized)
	{
		RenderTerrainCreationSection();
		ImGui::Separator();
	}
	else
	{
		RenderTerrainRegenerationSection();
		ImGui::Separator();
	}

	RenderTerrainStatisticsSection();
	RenderDebugVisualizationSection();

	ImGui::Separator();
	RenderMaterialPalette();

	ImGui::Separator();
	RenderSplatmapSlot();
}

bool Zenith_TerrainComponent::IsTerrainInitializedForEditor() const
{
	return m_pxStreamingState != nullptr &&
		m_pxStreamingState->m_ulUnifiedVertexBufferSize > 0;
}

//-----------------------------------------------------------------------------
// Initial terrain creation from a heightmap (shown only when terrain is not
// yet initialised).
//-----------------------------------------------------------------------------
void Zenith_TerrainComponent::RenderTerrainCreationSection()
{
	if (!ImGui::TreeNode("Create Terrain From Heightmap"))
		return;

	ImGui::TextWrapped("Specify a heightmap texture to generate terrain geometry. Use .ztxtr files (preferred) or 16-bit PNG. Textures should be 4096x4096 single-channel (grayscale).");
	ImGui::Separator();

	RenderHeightmapPathInput("HeightmapPath");

	ImGui::Separator();

	const std::string strOutputDir = GetTerrainAssetDirectory();
	ImGui::Text("Output Directory: %s", strOutputDir.c_str());

	const bool bCanCreate = strlen(s_szHeightmapPath) > 0 && !s_bTerrainExportInProgress;

	if (!bCanCreate)
		ImGui::BeginDisabled();

	if (ImGui::Button("Create Terrain", ImVec2(200, 30)))
	{
		const std::string strTerrainRoot =
			(std::filesystem::path(Project_GetGameAssetsDirectory()) / "Terrain").string();
		bool bLeaseEntered = false;
		if (!WithPreparedTerrainAssetDirectory(m_strTerrainAssetSet, strTerrainRoot,
			[&](const std::string& strValidatedOutputDir)
			{
			bLeaseEntered = true;
			if (NormalizeDirectoryPathForComparison(strValidatedOutputDir) !=
				NormalizeDirectoryPathForComparison(strOutputDir))
			{
				return false;
			}
			s_bTerrainExportInProgress = true;
			s_strTerrainExportStatus = "Exporting terrain meshes...";

			Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Starting terrain export...");
			Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent]   Heightmap: %s", s_szHeightmapPath);
			Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent]   Output: %s", strValidatedOutputDir.c_str());

			ExportHeightmapFromPaths(s_szHeightmapPath, strValidatedOutputDir);

			s_strTerrainExportStatus = "Export complete. Initializing terrain...";
			Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Export complete. Initializing terrain...");

			// Create blank materials for initial rendering
			const std::string strEntityName = m_xParentEntity.GetName().empty()
				? ("Entity_" + std::to_string(m_xParentEntity.GetEntityID().m_uIndex))
				: m_xParentEntity.GetName();

			auto xhMat0 = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
			Zenith_MaterialAsset* pxMat0 = xhMat0.GetDirect();
			auto xhMat1 = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
			Zenith_MaterialAsset* pxMat1 = xhMat1.GetDirect();
			if (pxMat0) pxMat0->SetName(strEntityName + "_Terrain_Mat0");
			if (pxMat1) pxMat1->SetName(strEntityName + "_Terrain_Mat1");
			m_axMaterials[0].Set(pxMat0);
			m_axMaterials[1].Set(pxMat1);

			EnsureMaterialSlotsPopulated();
			InitializeRenderResources();
			LoadCombinedPhysicsGeometry();

			s_bTerrainExportInProgress = false;
			const bool bInitialized = IsTerrainInitializedForEditor() &&
				HasPhysicsGeometry() && !m_bTerrainGeometryUnusable;
			if (bInitialized)
			{
				s_strTerrainExportStatus = "Terrain created successfully!";
				Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Terrain creation complete!");
			}
			else
			{
				s_strTerrainExportStatus = "Terrain creation failed to initialize render/physics state.";
				Zenith_Warning(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Terrain creation did not produce complete live state");
			}
			return bInitialized;
			}))
		{
			if (!bLeaseEntered)
			{
				s_strTerrainExportStatus = "Terrain creation refused an unsafe asset-set target.";
			}
		}
	}

	if (!bCanCreate)
		ImGui::EndDisabled();

	RenderTerrainStatusDisplay();
	ImGui::TreePop();
}

//-----------------------------------------------------------------------------
// Regenerate an existing terrain from a new heightmap. Destroys prior GPU /
// physics / buffer state via CleanupPriorGenerationForRegenerate() before
// re-running the export.
//-----------------------------------------------------------------------------
static bool BuildExpectedCanonicalPathFromExistingAncestor(
	const std::filesystem::path& xIntendedPath, std::filesystem::path& xExpectedCanonicalOut)
{
	std::error_code xError;
	const std::filesystem::path xAbsoluteIntended =
		std::filesystem::absolute(xIntendedPath, xError).lexically_normal();
	if (xError)
	{
		return false;
	}

	// Anchor strictly above the intended path. If the intended path itself is
	// an existing junction, canonicalizing its parent and comparing below will
	// expose the redirect instead of accepting it as the anchor.
	std::filesystem::path xExistingAncestor = xAbsoluteIntended.parent_path();
	for (;;)
	{
		xError.clear();
		if (std::filesystem::exists(xExistingAncestor, xError))
		{
			break;
		}
		if (xError)
		{
			return false;
		}
		const std::filesystem::path xParent = xExistingAncestor.parent_path();
		if (xParent == xExistingAncestor || xParent.empty())
		{
			return false;
		}
		xExistingAncestor = xParent;
	}

	const std::filesystem::path xCanonicalAncestor =
		std::filesystem::canonical(xExistingAncestor, xError);
	if (xError || !std::filesystem::is_directory(xCanonicalAncestor, xError) || xError)
	{
		return false;
	}
	const std::filesystem::path xRelative = xAbsoluteIntended.lexically_relative(xExistingAncestor);
	if (xRelative.empty())
	{
		return false;
	}
	xExpectedCanonicalOut = (xCanonicalAncestor / xRelative).lexically_normal();

	xError.clear();
	if (std::filesystem::exists(xAbsoluteIntended, xError))
	{
		if (xError || !std::filesystem::is_directory(xAbsoluteIntended, xError) || xError)
		{
			return false;
		}
		const std::filesystem::path xCanonicalExisting =
			std::filesystem::canonical(xAbsoluteIntended, xError);
		if (xError || xCanonicalExisting != xExpectedCanonicalOut)
		{
			return false;
		}
	}
	return !xError;
}

static std::filesystem::path NormalizeDirectoryPathForComparison(
	const std::filesystem::path& xPath)
{
	std::filesystem::path xNormalized = xPath.lexically_normal();
	while (xNormalized.has_relative_path() && !xNormalized.has_filename())
	{
		xNormalized = xNormalized.parent_path();
	}
	return xNormalized;
}

namespace
{
	class TerrainPreparedDirectoryLease
	{
	public:
		TerrainPreparedDirectoryLease() = default;
		TerrainPreparedDirectoryLease(const TerrainPreparedDirectoryLease&) = delete;
		TerrainPreparedDirectoryLease& operator=(const TerrainPreparedDirectoryLease&) = delete;

		~TerrainPreparedDirectoryLease()
		{
			ReleaseHandles();
			if (!m_bKeepCreatedDirectories)
			{
				for (auto xIt = m_axCreatedDirectories.rbegin();
					xIt != m_axCreatedDirectories.rend(); ++xIt)
				{
					std::error_code xRemoveError;
					std::filesystem::remove(*xIt, xRemoveError);
				}
			}
		}

		bool PrepareTerrainTarget(const std::string& strAssetSet,
			const std::string& strTerrainRoot)
		{
			if (!Zenith_TerrainComponent::IsValidTerrainAssetSetName(strAssetSet))
			{
				return false;
			}

			std::error_code xError;
			const std::filesystem::path xRoot = NormalizeDirectoryPathForComparison(
				std::filesystem::absolute(std::filesystem::path(strTerrainRoot), xError));
			if (xError || xRoot.filename().empty())
			{
				return false;
			}
			std::filesystem::path xAssets;
			std::filesystem::path xFinalAssets;
			if (!PrepareAssetsDirectoryFromTerrainRoot(
				xRoot, xAssets, xFinalAssets))
			{
				return false;
			}

			std::filesystem::path xFinalRoot;
			if (!PrepareCheckedChildDirectory(
				xAssets, xFinalAssets, xRoot.filename(), xRoot, xFinalRoot))
			{
				return false;
			}

			std::filesystem::path xTarget = xRoot;
			if (!strAssetSet.empty())
			{
				xTarget /= strAssetSet;
				std::filesystem::path xFinalTarget;
				if (!PrepareCheckedChildDirectory(
					xRoot, xFinalRoot, std::filesystem::path(strAssetSet),
					xTarget, xFinalTarget))
				{
					return false;
				}
			}

			SetDirectory(xTarget);
			return true;
		}

		bool PrepareLegacyTextureTarget(const std::string& strTerrainRoot)
		{
			std::error_code xError;
			const std::filesystem::path xRoot = NormalizeDirectoryPathForComparison(
				std::filesystem::absolute(std::filesystem::path(strTerrainRoot), xError));
			if (xError || xRoot.filename().empty())
			{
				return false;
			}
			std::filesystem::path xAssets;
			std::filesystem::path xFinalAssets;
			if (!PrepareAssetsDirectoryFromTerrainRoot(
				xRoot, xAssets, xFinalAssets))
			{
				return false;
			}

			const std::filesystem::path xTextures = xAssets / "Textures";
			std::filesystem::path xFinalTextures;
			if (!PrepareCheckedChildDirectory(
				xAssets, xFinalAssets, std::filesystem::path("Textures"),
				xTextures, xFinalTextures))
			{
				return false;
			}

			const std::filesystem::path xTextureTarget = xTextures / "Terrain";
			std::filesystem::path xFinalTextureTarget;
			if (!PrepareCheckedChildDirectory(
				xTextures, xFinalTextures, std::filesystem::path("Terrain"),
				xTextureTarget, xFinalTextureTarget))
			{
				return false;
			}

			SetDirectory(xTextureTarget);
			return true;
		}

		bool Run(const Zenith_TerrainComponent::TerrainDirectoryOperation& xOperation)
		{
			if (!xOperation || m_strDirectory.empty())
			{
				return false;
			}
			const bool bSucceeded = xOperation(m_strDirectory);
			m_bKeepCreatedDirectories = bSucceeded;
			return bSucceeded;
		}

		bool RenameChildFileAtomically(const std::string& strSourceFilename,
			const std::string& strDestinationFilename)
		{
#ifdef ZENITH_WINDOWS
			const std::filesystem::path xSourceFilename(strSourceFilename);
			const std::filesystem::path xDestinationFilename(strDestinationFilename);
			const auto IsSimpleFilename = [](const std::filesystem::path& xFilename)
			{
				return !xFilename.empty() && !xFilename.has_root_path() &&
					!xFilename.has_parent_path() && xFilename == xFilename.filename() &&
					xFilename != "." && xFilename != "..";
			};
			if (m_strDirectory.empty() || m_ahDirectories.empty() ||
				!IsSimpleFilename(xSourceFilename) ||
				!IsSimpleFilename(xDestinationFilename) ||
				xSourceFilename == xDestinationFilename)
			{
				return false;
			}

			const std::filesystem::path xSource =
				std::filesystem::path(m_strDirectory) / xSourceFilename;
			HANDLE hSource = CreateFileW(xSource.c_str(), DELETE | SYNCHRONIZE,
				FILE_SHARE_READ, nullptr, OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
			if (hSource == INVALID_HANDLE_VALUE)
			{
				return false;
			}

			BY_HANDLE_FILE_INFORMATION xSourceInfo = {};
			if (!GetFileInformationByHandle(hSource, &xSourceInfo) ||
				(xSourceInfo.dwFileAttributes &
					(FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0)
			{
				CloseHandle(hSource);
				return false;
			}

			// The Win32 FileRenameInfo wrapper resolves a simple name against the
			// process current directory. The native same-directory form instead
			// resolves it from the already-open source file object, so it neither
			// reopens the leased parent for write nor consults a mutable pathname.
			// This preserves the directory's no-write/no-delete share lease while
			// keeping completion-marker publication atomic.
			struct TerrainIoStatusBlock
			{
				union
				{
					LONG m_iStatus;
					PVOID m_pPointer;
				};
				ULONG_PTR m_ulInformation;
			};
			struct TerrainFileRenameInformation
			{
				BOOLEAN m_bReplaceIfExists;
				HANDLE m_hRootDirectory;
				ULONG m_uFileNameLength;
				WCHAR m_awcFileName[1];
			};
			using NtSetInformationFileFn = LONG (NTAPI*)(HANDLE,
				TerrainIoStatusBlock*, PVOID, ULONG, ULONG);
			constexpr ULONG uFILE_RENAME_INFORMATION_CLASS = 10u;

			const std::wstring strDestination = xDestinationFilename.wstring();
			const size_t ulFilenameBytes = strDestination.size() * sizeof(WCHAR);
			if (ulFilenameBytes == 0u || ulFilenameBytes > static_cast<size_t>(ULONG_MAX))
			{
				CloseHandle(hSource);
				return false;
			}
			std::vector<u_int8> auRenameBuffer(
				sizeof(TerrainFileRenameInformation) + ulFilenameBytes, 0u);
			auto* pxRename = reinterpret_cast<TerrainFileRenameInformation*>(
				auRenameBuffer.data());
			pxRename->m_bReplaceIfExists = FALSE;
			pxRename->m_hRootDirectory = nullptr;
			pxRename->m_uFileNameLength = static_cast<ULONG>(ulFilenameBytes);
			memcpy(pxRename->m_awcFileName, strDestination.data(), ulFilenameBytes);

			HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
			FARPROC pfnRaw = hNtdll == nullptr
				? nullptr
				: GetProcAddress(hNtdll, "NtSetInformationFile");
			NtSetInformationFileFn pfnNtSetInformationFile = nullptr;
			static_assert(sizeof(pfnNtSetInformationFile) == sizeof(pfnRaw));
			memcpy(&pfnNtSetInformationFile, &pfnRaw, sizeof(pfnNtSetInformationFile));
			if (pfnNtSetInformationFile == nullptr)
			{
				CloseHandle(hSource);
				return false;
			}

			TerrainIoStatusBlock xIoStatus = {};
			const LONG iStatus = pfnNtSetInformationFile(hSource, &xIoStatus,
				pxRename, static_cast<ULONG>(auRenameBuffer.size()),
				uFILE_RENAME_INFORMATION_CLASS);
			CloseHandle(hSource);
			return iStatus >= 0;
#else
			(void)strSourceFilename;
			(void)strDestinationFilename;
			return false;
#endif
		}

	private:
		bool PrepareAssetsDirectoryFromTerrainRoot(
			const std::filesystem::path& xTerrainRoot,
			std::filesystem::path& xAssetsOut,
			std::filesystem::path& xFinalAssetsOut)
		{
			// A fresh game checkout need not contain its ignored Assets directory.
			// Anchor at the existing game directory, then establish exactly the one
			// direct child named by the supplied Assets path. The shared checked-child
			// primitive rejects reparse points and keeps both handles leased before a
			// Terrain or named-set segment can be created.
			xAssetsOut = NormalizeDirectoryPathForComparison(
				xTerrainRoot.parent_path());
			if (xAssetsOut.empty() || xAssetsOut.filename().empty())
			{
				return false;
			}

			const std::filesystem::path xGameRoot =
				NormalizeDirectoryPathForComparison(xAssetsOut.parent_path());
			if (xGameRoot.empty() || xGameRoot == xAssetsOut ||
				xGameRoot.filename().empty())
			{
				return false;
			}

			std::filesystem::path xFinalGameRoot;
			if (!OpenCheckedDirectory(xGameRoot, nullptr, xFinalGameRoot))
			{
				return false;
			}
			return PrepareCheckedChildDirectory(
				xGameRoot, xFinalGameRoot, xAssetsOut.filename(),
				xAssetsOut, xFinalAssetsOut);
		}

		void SetDirectory(const std::filesystem::path& xDirectory)
		{
			m_strDirectory = xDirectory.generic_string();
			while (!m_strDirectory.empty() && m_strDirectory.back() == '/')
			{
				m_strDirectory.pop_back();
			}
			m_strDirectory += '/';
		}

#ifdef ZENITH_WINDOWS
		static bool PathsEqual(const std::filesystem::path& xLeft,
			const std::filesystem::path& xRight)
		{
			const std::wstring strLeft =
				NormalizeDirectoryPathForComparison(xLeft).wstring();
			const std::wstring strRight =
				NormalizeDirectoryPathForComparison(xRight).wstring();
			return CompareStringOrdinal(strLeft.c_str(), -1, strRight.c_str(), -1, TRUE) == CSTR_EQUAL;
		}

		static bool GetFinalDirectoryPath(HANDLE hDirectory,
			std::filesystem::path& xFinalPathOut)
		{
			const DWORD uFlags = FILE_NAME_NORMALIZED | VOLUME_NAME_DOS;
			const DWORD uRequired = GetFinalPathNameByHandleW(
				hDirectory, nullptr, 0, uFlags);
			if (uRequired == 0)
			{
				return false;
			}
			std::wstring strFinalPath(static_cast<size_t>(uRequired), L'\0');
			const DWORD uWritten = GetFinalPathNameByHandleW(
				hDirectory, strFinalPath.data(), uRequired, uFlags);
			if (uWritten == 0 || uWritten >= uRequired)
			{
				return false;
			}
			strFinalPath.resize(uWritten);
			constexpr wchar_t wszUNC_PREFIX[] = L"\\\\?\\UNC\\";
			constexpr wchar_t wszPATH_PREFIX[] = L"\\\\?\\";
			if (strFinalPath.rfind(wszUNC_PREFIX, 0) == 0)
			{
				strFinalPath = L"\\\\" + strFinalPath.substr(8);
			}
			else if (strFinalPath.rfind(wszPATH_PREFIX, 0) == 0)
			{
				strFinalPath.erase(0, 4);
			}
			xFinalPathOut = NormalizeDirectoryPathForComparison(
				std::filesystem::path(strFinalPath));
			return true;
		}

		bool OpenCheckedDirectory(const std::filesystem::path& xDirectory,
			const std::filesystem::path* pxExpectedFinalPath,
			std::filesystem::path& xFinalPathOut)
		{
			// FILE_READ_ATTRIBUTES alone does not participate in Windows read/write
			// share accounting, so a competing GENERIC_WRITE directory open can still
			// acquire FSCTL_SET_REPARSE_POINT while this handle is alive. Request
			// FILE_LIST_DIRECTORY (the directory form of FILE_READ_DATA) to make this
			// a real read-share lease: nested readers remain compatible, while the
			// absence of FILE_SHARE_WRITE/DELETE pins the directory against reparse
			// mutation and replacement for the callback lifetime.
			HANDLE hDirectory = CreateFileW(xDirectory.c_str(),
				FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES,
				FILE_SHARE_READ, nullptr, OPEN_EXISTING,
				FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
			if (hDirectory == INVALID_HANDLE_VALUE)
			{
				return false;
			}

			BY_HANDLE_FILE_INFORMATION xInfo = {};
			if (!GetFileInformationByHandle(hDirectory, &xInfo) ||
				(xInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
				(xInfo.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 ||
				!GetFinalDirectoryPath(hDirectory, xFinalPathOut))
			{
				CloseHandle(hDirectory);
				return false;
			}

			std::error_code xError;
			const std::filesystem::path xCanonical = NormalizeDirectoryPathForComparison(
				std::filesystem::canonical(xDirectory, xError));
			if (xError || !PathsEqual(xCanonical, xFinalPathOut) ||
				(pxExpectedFinalPath != nullptr &&
					!PathsEqual(*pxExpectedFinalPath, xFinalPathOut)))
			{
				CloseHandle(hDirectory);
				return false;
			}

			m_ahDirectories.push_back(hDirectory);
			return true;
		}

		bool PrepareCheckedChildDirectory(const std::filesystem::path& xParent,
			const std::filesystem::path& xFinalParent, const std::filesystem::path& xSegment,
			const std::filesystem::path& xChild, std::filesystem::path& xFinalChildOut)
		{
			if (xSegment.empty() || xSegment.has_root_path() || xSegment.has_parent_path() ||
				xSegment == "." || xSegment == ".." ||
				NormalizeDirectoryPathForComparison(xParent / xSegment) !=
					NormalizeDirectoryPathForComparison(xChild))
			{
				return false;
			}

			if (CreateDirectoryW(xChild.c_str(), nullptr))
			{
				m_axCreatedDirectories.push_back(xChild);
			}
			else if (GetLastError() != ERROR_ALREADY_EXISTS)
			{
				return false;
			}

			const std::filesystem::path xExpectedFinal =
				NormalizeDirectoryPathForComparison(xFinalParent / xSegment);
			return OpenCheckedDirectory(xChild, &xExpectedFinal, xFinalChildOut);
		}

		void ReleaseHandles()
		{
			for (auto xIt = m_ahDirectories.rbegin(); xIt != m_ahDirectories.rend(); ++xIt)
			{
				CloseHandle(*xIt);
			}
			m_ahDirectories.clear();
		}

		std::vector<HANDLE> m_ahDirectories;
#else
		bool OpenCheckedDirectory(const std::filesystem::path& xDirectory,
			const std::filesystem::path* pxExpectedFinalPath,
			std::filesystem::path& xFinalPathOut)
		{
			(void)xDirectory;
			(void)pxExpectedFinalPath;
			(void)xFinalPathOut;
			// Tools mutations are Windows-only. Without handle-relative writers,
			// pathname validation cannot provide the same lifetime guarantee.
			return false;
		}

		bool PrepareCheckedChildDirectory(const std::filesystem::path& xParent,
			const std::filesystem::path& xFinalParent, const std::filesystem::path& xSegment,
			const std::filesystem::path& xChild, std::filesystem::path& xFinalChildOut)
		{
			(void)xParent;
			(void)xFinalParent;
			(void)xSegment;
			(void)xChild;
			(void)xFinalChildOut;
			return false;
		}

		void ReleaseHandles() {}
#endif

		std::vector<std::filesystem::path> m_axCreatedDirectories;
		std::string m_strDirectory;
		bool m_bKeepCreatedDirectories = false;
	};
}

bool Zenith_TerrainComponent::ValidateTerrainAssetSetTarget(const std::string& strAssetSet,
	const std::string& strTerrainRoot, const std::string& strResolvedTarget)
{
	if (!IsValidTerrainAssetSetName(strAssetSet))
	{
		return false;
	}

	std::error_code xError;
	const std::filesystem::path xAbsoluteRoot = NormalizeDirectoryPathForComparison(
		std::filesystem::absolute(std::filesystem::path(strTerrainRoot), xError));
	if (xError)
	{
		return false;
	}
	const std::filesystem::path xAbsoluteTarget = NormalizeDirectoryPathForComparison(
		std::filesystem::absolute(std::filesystem::path(strResolvedTarget), xError));
	if (xError)
	{
		return false;
	}
	const std::filesystem::path xExpectedLexicalTarget = strAssetSet.empty()
		? xAbsoluteRoot
		: (xAbsoluteRoot / strAssetSet).lexically_normal();
	if (xAbsoluteTarget != xExpectedLexicalTarget)
	{
		return false;
	}

	std::filesystem::path xExpectedCanonicalRoot;
	std::filesystem::path xExpectedCanonicalTarget;
	if (!BuildExpectedCanonicalPathFromExistingAncestor(xAbsoluteRoot, xExpectedCanonicalRoot) ||
		!BuildExpectedCanonicalPathFromExistingAncestor(xAbsoluteTarget, xExpectedCanonicalTarget))
	{
		return false;
	}
	const std::filesystem::path xRequiredCanonicalTarget = strAssetSet.empty()
		? xExpectedCanonicalRoot
		: (xExpectedCanonicalRoot / strAssetSet).lexically_normal();
	return xExpectedCanonicalTarget == xRequiredCanonicalTarget;
}

bool Zenith_TerrainComponent::WithPreparedTerrainAssetDirectory(
	const std::string& strAssetSet, const std::string& strTerrainRoot,
	const TerrainDirectoryOperation& xOperation)
{
	TerrainPreparedDirectoryLease xLease;
	if (!xLease.PrepareTerrainTarget(strAssetSet, strTerrainRoot))
	{
		return false;
	}
	return xLease.Run(xOperation);
}

bool Zenith_TerrainComponent::RenamePreparedTerrainAssetFileAtomically(
	const std::string& strAssetSet, const std::string& strTerrainRoot,
	const std::string& strSourceFilename, const std::string& strDestinationFilename)
{
	TerrainPreparedDirectoryLease xLease;
	if (!xLease.PrepareTerrainTarget(strAssetSet, strTerrainRoot))
	{
		return false;
	}
	return xLease.Run([&](const std::string&)
		{
			return xLease.RenameChildFileAtomically(
				strSourceFilename, strDestinationFilename);
		});
}

bool Zenith_TerrainComponent::WithPreparedTerrainTextureDirectory(
	const std::string& strAssetSet, const std::string& strTerrainRoot,
	const TerrainDirectoryOperation& xOperation)
{
	if (!IsValidTerrainAssetSetName(strAssetSet))
	{
		return false;
	}
	if (!strAssetSet.empty())
	{
		return WithPreparedTerrainAssetDirectory(
			strAssetSet, strTerrainRoot, xOperation);
	}

	TerrainPreparedDirectoryLease xLease;
	if (!xLease.PrepareLegacyTextureTarget(strTerrainRoot))
	{
		return false;
	}
	return xLease.Run(xOperation);
}

// Production cleanup wrapper re-runs the non-destructive canonical check
// immediately before deletion as defense in depth against target replacement.
bool Zenith_TerrainComponent::DeleteExistingTerrainFilesForAssetSet(
	const std::string& strAssetSet, const std::string& strResolvedDirectory)
{
	const std::string strTerrainRoot =
		(std::filesystem::path(Project_GetGameAssetsDirectory()) / "Terrain").string();
	return WithPreparedTerrainAssetDirectory(strAssetSet, strTerrainRoot,
		[&](const std::string& strValidatedDirectory)
		{
			if (!ValidateTerrainAssetSetTarget(
				strAssetSet, strTerrainRoot, strResolvedDirectory) ||
				NormalizeDirectoryPathForComparison(strValidatedDirectory) !=
					NormalizeDirectoryPathForComparison(strResolvedDirectory))
			{
				return false;
			}
			return DeleteExistingTerrainFilesInDirectory(strValidatedDirectory);
		});
}

// Private non-recursive core. Named sets also keep Height/Splatmap/GrassDensity
// textures here, so only direct generated .zmesh files are removed. Production
// reaches this only through the canonical wrapper; the friend test seam may use
// an arbitrary Build/artifacts sandbox.
bool Zenith_TerrainComponent::DeleteExistingTerrainFilesInDirectory(const std::string& strDirectory)
{
	Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Deleting existing terrain meshes in %s", strDirectory.c_str());
	try
	{
		if (std::filesystem::exists(strDirectory))
		{
			for (const auto& entry : std::filesystem::directory_iterator(strDirectory))
			{
				if (entry.is_regular_file() && entry.path().extension().string() == ZENITH_MESH_EXT)
				{
					std::filesystem::remove(entry.path());
				}
			}
			Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Deleted existing terrain meshes");
		}
		return true;
	}
	catch (const std::exception& e)
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Warning: Failed to delete some terrain files: %s", e.what());
		return false;
	}
}

#ifdef ZENITH_TESTING
void Zenith_UnitTests::DeleteExistingTerrainFilesForTest(const std::string& strOutputDir)
{
	Zenith_TerrainComponent::DeleteExistingTerrainFilesInDirectory(strOutputDir);
}
#endif

// Allocate a fresh material asset into any empty slot, named after the
// owning entity. Called between physics-geometry load and render-init so
// the upcoming InitializeRenderResources sees a fully-populated slot table.
void Zenith_TerrainComponent::EnsureMaterialSlotsPopulated()
{
	const std::string strEntityName = m_xParentEntity.GetName().empty()
		? ("Entity_" + std::to_string(m_xParentEntity.GetEntityID().m_uIndex))
		: m_xParentEntity.GetName();
	for (u_int u = 0; u < TERRAIN_MATERIAL_COUNT; u++)
	{
		if (m_axMaterials[u].GetDirect()) continue;
		auto xhMat = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
		Zenith_MaterialAsset* pxMat = xhMat.GetDirect();
		if (pxMat) pxMat->SetName(strEntityName + "_Terrain_Mat" + std::to_string(u));
		m_axMaterials[u].Set(pxMat);
	}
}

// End-to-end execution of the Regenerate button: cleanup → delete files →
// export → reload physics → re-init render. Each step updates the export
// status string so the editor footer reflects progress.
bool Zenith_TerrainComponent::RunTerrainRegeneration(const std::string& strOutputDir)
{
	return RunTerrainRegenerationInternal(strOutputDir, nullptr);
}

// Terrain-editor bake entry point: identical pipeline, but the export source
// is the editor's live in-memory heightfield instead of a heightmap file.
bool Zenith_TerrainComponent::RegenerateFromHeightfield(const Zenith_Image& xHeightfield)
{
	const std::string strOutputDir = GetTerrainAssetDirectory();
	return RunTerrainRegenerationInternal(strOutputDir, &xHeightfield);
}

bool Zenith_TerrainComponent::RunTerrainRegenerationInternal(const std::string& strOutputDir, const Zenith_Image* pxHeightfield)
{
	const std::string strTerrainRoot =
		(std::filesystem::path(Project_GetGameAssetsDirectory()) / "Terrain").string();
	return RunTerrainRegenerationInternalForTerrainRoot(
		strTerrainRoot, strOutputDir, pxHeightfield);
}

bool Zenith_TerrainComponent::RunTerrainRegenerationInternalForTerrainRoot(
	const std::string& strTerrainRoot, const std::string& strOutputDir,
	const Zenith_Image* pxHeightfield)
{
	// The lease owns the complete synchronous cleanup/delete/export/reinitialize
	// operation. Its no-delete-share handles prevent the checked Assets, Terrain,
	// and target directories from being renamed or replaced mid-regeneration.
	bool bLeaseEntered = false;
	const bool bSucceeded = WithPreparedTerrainAssetDirectory(
		m_strTerrainAssetSet, strTerrainRoot,
		[&](const std::string& strValidatedDirectory)
		{
		bLeaseEntered = true;
		if (!ValidateTerrainAssetSetTarget(
			m_strTerrainAssetSet, strTerrainRoot, strOutputDir) ||
			NormalizeDirectoryPathForComparison(strValidatedDirectory) !=
				NormalizeDirectoryPathForComparison(strOutputDir))
		{
			return false;
		}

	s_bTerrainExportInProgress = true;
	s_strTerrainExportStatus = "Cleaning up existing terrain...";
	Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Starting terrain regeneration...");

	CleanupPriorGenerationForRegenerate();

	s_strTerrainExportStatus = "Deleting existing terrain meshes...";
	if (!DeleteExistingTerrainFilesInDirectory(strValidatedDirectory))
	{
		s_bTerrainExportInProgress = false;
		s_strTerrainExportStatus = "Terrain regeneration failed while cleaning existing meshes.";
		return false;
	}

	s_strTerrainExportStatus = "Exporting new terrain meshes...";
	Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Exporting new terrain...");
	Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent]   Output: %s", strValidatedDirectory.c_str());
	if (pxHeightfield != nullptr)
	{
		ExportHeightmapFromMat(*pxHeightfield, strValidatedDirectory);
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent]   Heightmap: %s", s_szHeightmapPath);
		ExportHeightmapFromPaths(s_szHeightmapPath, strValidatedDirectory);
	}

	s_strTerrainExportStatus = "Initializing render resources...";
	Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Reinitializing render resources...");
	EnsureMaterialSlotsPopulated();
	InitializeRenderResources();

	s_strTerrainExportStatus = "Loading physics geometry...";
	Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Loading new physics geometry...");
	LoadCombinedPhysicsGeometry();

	s_bTerrainExportInProgress = false;
	const bool bInitialized = IsTerrainInitializedForEditor() &&
		HasPhysicsGeometry() && !m_bTerrainGeometryUnusable;
	if (bInitialized)
	{
		s_strTerrainExportStatus = "Terrain regenerated successfully!";
		Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Terrain regeneration complete!");
	}
	else
	{
		s_strTerrainExportStatus = "Terrain regeneration failed to initialize complete render/physics state.";
		Zenith_Warning(LOG_CATEGORY_TERRAIN,
			"[TerrainComponent] Terrain regeneration did not produce complete live state");
	}
	return bInitialized;
		});
	if (!bLeaseEntered)
	{
		s_strTerrainExportStatus = "Terrain regeneration refused an unvalidated output directory.";
		Zenith_Warning(LOG_CATEGORY_TERRAIN,
			"[TerrainComponent] Refusing terrain regeneration outside the handle-bound component asset set");
	}
	return bSucceeded;
}

void Zenith_TerrainComponent::RenderTerrainRegenerationSection()
{
	if (!ImGui::TreeNode("Regenerate Terrain"))
		return;

	ImGui::TextWrapped("Regenerate terrain from new heightmap and material interpolation textures. This will delete existing terrain files and recreate all chunks.");
	ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Warning: This operation cannot be undone!");
	ImGui::Separator();

	RenderHeightmapPathInput("RegenHeightmapPath");

	ImGui::Separator();

	const std::string strOutputDir = GetTerrainAssetDirectory();
	ImGui::Text("Output Directory: %s", strOutputDir.c_str());

	const bool bCanRegenerate = strlen(s_szHeightmapPath) > 0 && !s_bTerrainExportInProgress;

	if (!bCanRegenerate) ImGui::BeginDisabled();

	if (ImGui::Button("Regenerate Terrain", ImVec2(200, 30)))
	{
		RunTerrainRegeneration(strOutputDir);
	}

	if (!bCanRegenerate) ImGui::EndDisabled();

	RenderTerrainStatusDisplay();
	ImGui::TreePop();
}

// Teardown sequence used by the Regenerate button. The order matters:
// culling resources -> streaming manager -> unified buffers -> physics geometry.
// Keeping this together makes the contract visible to readers.
void Zenith_TerrainComponent::CleanupPriorGenerationForRegenerate()
{
	Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Destroying existing culling resources...");
	DestroyCullingResources();

	Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Unregistering terrain buffers from streaming manager...");
	// Component-aware overload — the legacy no-arg form resolves through
	// s_pxPrimary, which on a non-primary terrain would unregister the
	// wrong state and leave THIS component still in the registry pointing
	// at buffers we're about to destroy.
	g_xEngine.TerrainStreaming().UnregisterTerrainBuffers(m_pxStreamingState);

	Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Destroying existing unified buffers...");
	g_xEngine.FluxMemory().DestroyVertexBuffer(m_pxStreamingState->m_xUnifiedVertexBuffer);
	g_xEngine.FluxMemory().DestroyIndexBuffer(m_pxStreamingState->m_xUnifiedIndexBuffer);
	m_pxStreamingState->m_ulUnifiedVertexBufferSize = 0;
	m_pxStreamingState->m_ulUnifiedIndexBufferSize = 0;

	if (m_pxPhysicsGeometry)
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Destroying existing physics geometry...");
		delete m_pxPhysicsGeometry;
		m_pxPhysicsGeometry = nullptr;
	}
}

void Zenith_TerrainComponent::RenderTerrainStatisticsSection()
{
	if (!ImGui::TreeNode("Statistics"))
		return;

	ImGui::Text("Chunks: %d x %d", CHUNK_GRID_SIZE, CHUNK_GRID_SIZE);
	ImGui::Text("Total Chunks: %d", TOTAL_CHUNKS);
	ImGui::Text("LOD Count: %d", LOD_COUNT);
	ImGui::Text("Vertex Buffer Size: %.2f MB", m_pxStreamingState->m_ulUnifiedVertexBufferSize / (1024.0f * 1024.0f));
	ImGui::Text("Index Buffer Size: %.2f MB", m_pxStreamingState->m_ulUnifiedIndexBufferSize / (1024.0f * 1024.0f));
	ImGui::Text("LOW LOD Vertices: %u", m_pxStreamingState->m_uLowLODVertexCount);
	ImGui::Text("LOW LOD Indices: %u", m_pxStreamingState->m_uLowLODIndexCount);
	bool bTemp = m_pxStreamingState->m_bCullingResourcesInitialized;
	ImGui::Checkbox("Culling Resources Initialized", &bTemp);
	ImGui::TreePop();
}

void Zenith_TerrainComponent::RenderDebugVisualizationSection()
{
	if (!ImGui::TreeNode("Debug Visualization"))
		return;

	static const char* aszDebugModeNames[] = {
		"Off", "LOD Level", "World Normals", "UVs", "Material Blend",
		"Roughness", "Metallic", "Occlusion", "World Position", "Chunk Grid",
		"Tangent", "Bitangent Sign", "Source Chunk Hash"
	};
	u_int& uDebugMode = g_xEngine.Terrain().GetDebugMode();
	int iDebugMode = static_cast<int>(uDebugMode);
	if (ImGui::Combo("Visualization Mode", &iDebugMode, aszDebugModeNames, IM_ARRAYSIZE(aszDebugModeNames)))
	{
		uDebugMode = static_cast<u_int>(iDebugMode);
	}

	bool& bWireframe = g_xEngine.Terrain().GetWireframeMode();
	ImGui::Checkbox("Wireframe", &bWireframe);

	ImGui::Separator();

	ImGui::Text("Streaming Statistics");
	// Component-aware: pull this terrain's own stats. Falls back to a zeroed
	// snapshot if streaming state isn't yet initialised (component just
	// constructed, not yet registered).
	static const Flux_TerrainStreamingManagerImpl::StreamingStats kxZeroStats{};
	const Flux_TerrainStreamingState* pxState = m_pxStreamingState;
	const Flux_TerrainStreamingManagerImpl::StreamingStats& xStats = pxState ? pxState->m_xStats : kxZeroStats;

	ImGui::Text("HIGH LOD Chunks: %u / %u", xStats.m_uHighLODChunksResident, TOTAL_CHUNKS);
	ImGui::Text("Streams This Frame: %u", xStats.m_uStreamsThisFrame);
	ImGui::Text("Evictions This Frame: %u", xStats.m_uEvictionsThisFrame);

	ImGui::Separator();

	const float fVertexUsage = xStats.m_uVertexBufferTotalMB > 0
		? static_cast<float>(xStats.m_uVertexBufferUsedMB) / static_cast<float>(xStats.m_uVertexBufferTotalMB)
		: 0.0f;
	char szVertexLabel[64];
	snprintf(szVertexLabel, sizeof(szVertexLabel), "Vertex Buffer: %u / %u MB", xStats.m_uVertexBufferUsedMB, xStats.m_uVertexBufferTotalMB);
	ImGui::ProgressBar(fVertexUsage, ImVec2(-1, 0), szVertexLabel);
	ImGui::Text("Vertex Fragments: %u", xStats.m_uVertexFragments);

	const float fIndexUsage = xStats.m_uIndexBufferTotalMB > 0
		? static_cast<float>(xStats.m_uIndexBufferUsedMB) / static_cast<float>(xStats.m_uIndexBufferTotalMB)
		: 0.0f;
	char szIndexLabel[64];
	snprintf(szIndexLabel, sizeof(szIndexLabel), "Index Buffer: %u / %u MB", xStats.m_uIndexBufferUsedMB, xStats.m_uIndexBufferTotalMB);
	ImGui::ProgressBar(fIndexUsage, ImVec2(-1, 0), szIndexLabel);
	ImGui::Text("Index Fragments: %u", xStats.m_uIndexFragments);

	ImGui::TreePop();
}

void Zenith_TerrainComponent::RenderMaterialPalette()
{
	static const char* aszMaterialNames[] = { "Material 0", "Material 1", "Material 2", "Material 3" };
	for (u_int u = 0; u < TERRAIN_MATERIAL_COUNT; u++)
	{
		Zenith_MaterialAsset* pxMat = m_axMaterials[u].GetDirect();
		if (!pxMat)
		{
			ImGui::TextDisabled("%s: (not set)", aszMaterialNames[u]);
			continue;
		}

		char szLabel[64];
		snprintf(szLabel, sizeof(szLabel), "%s##TerrainMat%u", aszMaterialNames[u], u);
		if (!ImGui::TreeNode(szLabel))
			continue;

		ImGui::Text("Name: %s", pxMat->GetName().c_str());

		char szImGuiId[32];
		snprintf(szImGuiId, sizeof(szImGuiId), "TerrainMat%u", u);
		g_xEngine.EditorMaterialUI().RenderMaterialProperties(pxMat, szImGuiId);

		ImGui::Separator();
		ImGui::Text("Textures:");
		g_xEngine.EditorMaterialUI().RenderAllTextureSlots(*pxMat, false);

		ImGui::TreePop();
	}
}

void Zenith_TerrainComponent::RenderSplatmapSlot()
{
	if (!ImGui::TreeNode("Splatmap Texture"))
		return;

	if (Zenith_TextureAsset* pxSplatmap = m_xSplatmap.Resolve())
	{
		Flux_ImGuiTextureHandle xSplatmapHandle = g_xEngine.EditorMaterialUI().GetOrCreateTexturePreviewHandle(pxSplatmap);
		if (xSplatmapHandle.IsValid())
		{
			ImGui::Image(
				(ImTextureID)Flux_ImGuiIntegration::GetImTextureID(xSplatmapHandle),
				ImVec2(128, 128)
			);
		}
		ImGui::TextWrapped("%s", m_xSplatmap.GetPath().c_str());
	}
	else
	{
		ImGui::TextDisabled("(not set)");
	}

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_TEXTURE))
		{
			const DragDropFilePayload* pFilePayload =
				static_cast<const DragDropFilePayload*>(pPayload->Data);
			m_xSplatmap.SetPath(pFilePayload->m_szFilePath);
			Zenith_Log(LOG_CATEGORY_TERRAIN, "[TerrainComponent] Set splatmap: %s", pFilePayload->m_szFilePath);
		}
		ImGui::EndDragDropTarget();
	}

	ImGui::TreePop();
}

#endif // ZENITH_TOOLS
