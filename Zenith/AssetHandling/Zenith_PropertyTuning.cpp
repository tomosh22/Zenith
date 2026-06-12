#include "Zenith.h"
#include "AssetHandling/Zenith_PropertyTuning.h"
#include "DataStream/Zenith_DataStream.h"

#ifdef ZENITH_TOOLS
#include "Core/Zenith_FileWatcher.h"
#endif

#include <filesystem>

namespace
{
	// "ZTUN" - guards against applying a foreign file to a property table.
	constexpr u_int uTUNING_FILE_MAGIC = 0x4E55545Au;
}

Zenith_Vector<Zenith_PropertyTuning::Binding> Zenith_PropertyTuning::s_axBindings;
u_int Zenith_PropertyTuning::s_uNextHandle = 1;
#ifdef ZENITH_TOOLS
Zenith_Vector<Zenith_PropertyTuning::DirWatcher> Zenith_PropertyTuning::s_axWatchers;
#endif

//------------------------------------------------------------------------------
// Internal helpers
//------------------------------------------------------------------------------

std::string Zenith_PropertyTuning::NormalizePath(const char* szPath)
{
	std::string strNormalized(szPath ? szPath : "");
	for (char& c : strNormalized)
	{
		if (c == '\\')
		{
			c = '/';
		}
		else if (c >= 'A' && c <= 'Z')
		{
			c = static_cast<char>(c - 'A' + 'a');
		}
	}
	return strNormalized;
}

Zenith_PropertyTuning::Binding* Zenith_PropertyTuning::FindBinding(u_int uHandle)
{
	for (u_int u = 0; u < s_axBindings.GetSize(); ++u)
	{
		if (s_axBindings.Get(u).m_uHandle == uHandle)
		{
			return &s_axBindings.Get(u);
		}
	}
	return nullptr;
}

#ifdef ZENITH_TOOLS

void Zenith_PropertyTuning::OnWatchedFileChanged(void* pContext, const std::string& strRelativePath, FileChangeType eType)
{
	if (eType != FileChangeType::Modified && eType != FileChangeType::Created)
	{
		return;
	}

	// pContext is the owning Zenith_FileWatcher; the event path is relative to
	// its watched directory.
	Zenith_FileWatcher* pxWatcher = static_cast<Zenith_FileWatcher*>(pContext);
	const std::string strFullPath = NormalizePath((pxWatcher->GetWatchedDirectory() + "/" + strRelativePath).c_str());

	// Collect matching handles first - ReapplyBinding may not mutate the
	// binding list, but keep the loop robust against future changes.
	Zenith_Vector<u_int> auMatchingHandles;
	for (u_int u = 0; u < s_axBindings.GetSize(); ++u)
	{
		Binding& xBinding = s_axBindings.Get(u);
		if (xBinding.m_strNormalizedPath != strFullPath)
		{
			continue;
		}
		if (xBinding.m_bSuppressNextWatchEvent)
		{
			// Our own SaveBinding write bouncing back - swallow it once.
			xBinding.m_bSuppressNextWatchEvent = false;
			continue;
		}
		auMatchingHandles.PushBack(xBinding.m_uHandle);
	}

	for (u_int u = 0; u < auMatchingHandles.GetSize(); ++u)
	{
		ReapplyBinding(auMatchingHandles.Get(u));
	}
}

void Zenith_PropertyTuning::EnsureWatcherForFile(const std::string& strFilePath)
{
	std::error_code xEC;
	const std::filesystem::path xParent = std::filesystem::path(strFilePath).parent_path();
	const std::string strDir = xParent.string();
	if (strDir.empty())
	{
		return;
	}
	const std::string strNormalizedDir = NormalizePath(strDir.c_str());

	for (u_int u = 0; u < s_axWatchers.GetSize(); ++u)
	{
		if (s_axWatchers.Get(u).m_strNormalizedDir == strNormalizedDir)
		{
			return;	// already watching this directory
		}
	}

	Zenith_FileWatcher* pxWatcher = new Zenith_FileWatcher();
	// Non-recursive: tuning bindings name explicit files; we watch each file's
	// immediate parent. pContext = the watcher itself so the callback can
	// reconstruct full paths from its watched directory.
	if (!pxWatcher->Start(strDir, false, &Zenith_PropertyTuning::OnWatchedFileChanged, pxWatcher))
	{
		Zenith_Log(LOG_CATEGORY_ASSET,
			"Zenith_PropertyTuning: failed to watch '%s'; live reload inert for files there (manual ReapplyBinding still works)",
			strDir.c_str());
		delete pxWatcher;
		return;
	}

	DirWatcher xEntry;
	xEntry.m_strNormalizedDir = strNormalizedDir;
	xEntry.m_pxWatcher = pxWatcher;
	s_axWatchers.PushBack(xEntry);
}

void Zenith_PropertyTuning::PruneUnusedWatchers()
{
	for (u_int u = s_axWatchers.GetSize(); u > 0; --u)
	{
		const u_int uIndex = u - 1;
		const std::string& strDir = s_axWatchers.Get(uIndex).m_strNormalizedDir;

		bool bStillUsed = false;
		for (u_int uBinding = 0; uBinding < s_axBindings.GetSize(); ++uBinding)
		{
			const std::string& strPath = s_axBindings.Get(uBinding).m_strNormalizedPath;
			const size_t uSlash = strPath.find_last_of('/');
			const std::string strBindingDir = (uSlash == std::string::npos) ? std::string() : strPath.substr(0, uSlash);
			if (strBindingDir == strDir)
			{
				bStillUsed = true;
				break;
			}
		}

		if (!bStillUsed)
		{
			s_axWatchers.Get(uIndex).m_pxWatcher->Stop();
			delete s_axWatchers.Get(uIndex).m_pxWatcher;
			s_axWatchers.Remove(uIndex);
		}
	}
}

#endif // ZENITH_TOOLS

//------------------------------------------------------------------------------
// Public API
//------------------------------------------------------------------------------

u_int Zenith_PropertyTuning::BindFile(const char* szAbsolutePath, void* pxInstance, const Zenith_PropertyTable& xTable,
	Zenith_PropertySystem::PropertyChangedFn pfnOnChanged, void* pxUserData)
{
	if (!szAbsolutePath || szAbsolutePath[0] == '\0' || !pxInstance)
	{
		Zenith_Error(LOG_CATEGORY_ASSET, "Zenith_PropertyTuning::BindFile: invalid path or instance");
		return uINVALID_BINDING;
	}

	Binding xBinding;
	xBinding.m_uHandle = s_uNextHandle++;
	xBinding.m_strPath = szAbsolutePath;
	xBinding.m_strNormalizedPath = NormalizePath(szAbsolutePath);
	xBinding.m_pxInstance = pxInstance;
	xBinding.m_pxTable = &xTable;
	xBinding.m_pfnOnChanged = pfnOnChanged;
	xBinding.m_pxUserData = pxUserData;
	s_axBindings.PushBack(xBinding);

#ifdef ZENITH_TOOLS
	EnsureWatcherForFile(xBinding.m_strPath);
#endif

	// Apply existing on-disk values immediately so a bound instance starts in
	// sync with its tuning file.
	std::error_code xEC;
	if (std::filesystem::exists(szAbsolutePath, xEC))
	{
		ReapplyBinding(xBinding.m_uHandle);
	}

	return xBinding.m_uHandle;
}

void Zenith_PropertyTuning::Unbind(u_int uHandle)
{
	for (u_int u = 0; u < s_axBindings.GetSize(); ++u)
	{
		if (s_axBindings.Get(u).m_uHandle == uHandle)
		{
			s_axBindings.Remove(u);
			break;
		}
	}

#ifdef ZENITH_TOOLS
	PruneUnusedWatchers();
#endif
}

void Zenith_PropertyTuning::UnbindAll()
{
	s_axBindings.Clear();
#ifdef ZENITH_TOOLS
	for (u_int u = 0; u < s_axWatchers.GetSize(); ++u)
	{
		s_axWatchers.Get(u).m_pxWatcher->Stop();
		delete s_axWatchers.Get(u).m_pxWatcher;
	}
	s_axWatchers.Clear();
#endif
}

void Zenith_PropertyTuning::SaveBinding(u_int uHandle)
{
	Binding* pxBinding = FindBinding(uHandle);
	if (!pxBinding)
	{
		Zenith_Error(LOG_CATEGORY_ASSET, "Zenith_PropertyTuning::SaveBinding: unknown handle %u", uHandle);
		return;
	}

	Zenith_DataStream xStream;
	xStream << uTUNING_FILE_MAGIC;
	Zenith_PropertySystem::WriteProperties(pxBinding->m_pxInstance, *pxBinding->m_pxTable, xStream);

	// Our own write will bounce back as a Modified event; suppress it for THIS
	// binding only - other bindings on the same file (live consumers of an
	// authoring instance's save) should re-apply.
	pxBinding->m_bSuppressNextWatchEvent = true;
	xStream.WriteToFile(pxBinding->m_strPath.c_str());
}

void Zenith_PropertyTuning::ReapplyBinding(u_int uHandle)
{
	Binding* pxBinding = FindBinding(uHandle);
	if (!pxBinding)
	{
		Zenith_Error(LOG_CATEGORY_ASSET, "Zenith_PropertyTuning::ReapplyBinding: unknown handle %u", uHandle);
		return;
	}

	Zenith_DataStream xStream;
	xStream.ReadFromFile(pxBinding->m_strPath.c_str());
	if (!xStream.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "Zenith_PropertyTuning: tuning file '%s' missing or empty; values unchanged",
			pxBinding->m_strPath.c_str());
		return;
	}

	u_int uMagic = 0;
	xStream >> uMagic;
	if (uMagic != uTUNING_FILE_MAGIC)
	{
		Zenith_Error(LOG_CATEGORY_ASSET, "Zenith_PropertyTuning: '%s' is not a tuning file (bad magic 0x%08X); values unchanged",
			pxBinding->m_strPath.c_str(), uMagic);
		return;
	}

	Zenith_PropertySystem::ReadProperties(pxBinding->m_pxInstance, *pxBinding->m_pxTable, xStream,
		pxBinding->m_pfnOnChanged, pxBinding->m_pxUserData);
}

void Zenith_PropertyTuning::Update()
{
#ifdef ZENITH_TOOLS
	for (u_int u = 0; u < s_axWatchers.GetSize(); ++u)
	{
		s_axWatchers.Get(u).m_pxWatcher->Update();
	}
#endif
}

u_int Zenith_PropertyTuning::GetBindingCount()
{
	return s_axBindings.GetSize();
}

#include "AssetHandling/Zenith_PropertyTuning.Tests.inl"
