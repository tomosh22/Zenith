#pragma once

#include "Core/Zenith_PropertySystem.h"
#include <string>

//------------------------------------------------------------------------------
// Zenith_PropertyTuning - live tuning-file bindings for reflected properties.
//
// Binds an instance + its Zenith_PropertyTable to an on-disk tuning file
// (.ztune: a magic word followed by a Zenith_PropertySystem property blob).
// In ZENITH_TOOLS builds, a Zenith_FileWatcher (Core/Zenith_FileWatcher.h -
// the instance watcher that also powers Flux_ShaderHotReload) watches each
// bound file's directory; a Modified event re-applies the file to the live
// instance at the next Update() pump (called from Zenith_MainLoop before the
// scene update - the safe-point reload discipline). Edit the file, save, see
// the change without restarting.
//
// SaveBinding writes the instance's current values back out, suppressing the
// bounce-back watch event for the saving binding (other bindings on the same
// file DO re-apply - that's the authoring flow: one binding saves, live
// consumers pick it up).
//
// Outside ZENITH_TOOLS the watcher machinery compiles out; bindings still
// work for manual SaveBinding / ReapplyBinding.
//
// Class-static state: process-level asset-side helper, same shape as the
// FileWatcher/ShaderHotReload precedents.
//------------------------------------------------------------------------------

#ifdef ZENITH_TOOLS
class Zenith_FileWatcher;
enum class FileChangeType;
#endif

class Zenith_PropertyTuning
{
public:
	static constexpr u_int uINVALID_BINDING = 0;

	// Binds and, if the file exists, applies it immediately (firing
	// pfnOnChanged per changed property). Returns a handle (uINVALID_BINDING on
	// failure). The instance and table must outlive the binding.
	static u_int BindFile(const char* szAbsolutePath, void* pxInstance, const Zenith_PropertyTable& xTable,
		Zenith_PropertySystem::PropertyChangedFn pfnOnChanged = nullptr, void* pxUserData = nullptr);

	static void Unbind(u_int uHandle);
	static void UnbindAll();

	// Serialize the bound instance's current values to its file.
	static void SaveBinding(u_int uHandle);

	// Re-read the bound file and apply it. This is what a watch event drives;
	// public so tests and tools can invoke the reload path directly.
	static void ReapplyBinding(u_int uHandle);

	// Pumps the directory watchers (dispatches pending file-change events on
	// the calling thread). Called once per frame from Zenith_MainLoop, before
	// the scene update. No-op outside ZENITH_TOOLS or with no bindings.
	static void Update();

	static u_int GetBindingCount();

private:
	struct Binding
	{
		u_int m_uHandle = uINVALID_BINDING;
		std::string m_strPath;				// as given
		std::string m_strNormalizedPath;	// lowercase, forward slashes - for event matching
		void* m_pxInstance = nullptr;
		const Zenith_PropertyTable* m_pxTable = nullptr;
		Zenith_PropertySystem::PropertyChangedFn m_pfnOnChanged = nullptr;
		void* m_pxUserData = nullptr;
		bool m_bSuppressNextWatchEvent = false;	// set by SaveBinding to ignore our own write
	};

	static std::string NormalizePath(const char* szPath);
	static Binding* FindBinding(u_int uHandle);

	static Zenith_Vector<Binding> s_axBindings;
	static u_int s_uNextHandle;

#ifdef ZENITH_TOOLS
	// One watcher per distinct bound-file directory (non-recursive).
	struct DirWatcher
	{
		std::string m_strNormalizedDir;
		Zenith_FileWatcher* m_pxWatcher = nullptr;
	};

	static void EnsureWatcherForFile(const std::string& strFilePath);
	static void PruneUnusedWatchers();
	static void OnWatchedFileChanged(void* pContext, const std::string& strRelativePath, FileChangeType eType);

	static Zenith_Vector<DirWatcher> s_axWatchers;
#endif
};
