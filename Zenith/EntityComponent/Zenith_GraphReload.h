#pragma once

#ifdef ZENITH_TOOLS

#include <string>

//------------------------------------------------------------------------------
// Zenith_GraphReload - live hot reload of Behaviour Graph assets (TOOLS only).
//
// Sources of change:
//   - the Graph Editor panel's Save (calls NotifyAssetChanged directly), and
//   - external edits to game:Graphs/*.bgraph (a Core Zenith_FileWatcher on the
//     directory, pumped by Update()).
//
// Changes are QUEUED and drained by Update() at the main loop's safe point
// (before physics/scene update - the Flux_ShaderHotReload discipline), never
// mid-dispatch. A reload:
//   1. refreshes the registry-cached asset's definition from disk (no-op for
//      in-editor saves, which already edited the cached definition),
//   2. re-instantiates every live graph slot referencing the asset, migrating
//      blackboard state name+type-matched (type changes DROP, never
//      reinterpret),
//   3. records a designer-facing status line (shown in the Graph Editor).
//
// Atomicity: a failed load leaves the old definition and old live instances
// untouched.
//------------------------------------------------------------------------------
class Zenith_GraphReload
{
public:
	// Queue a reload of the given .bgraph asset (prefixed path, any separator
	// style). Safe to call any time on the main thread.
	static void NotifyAssetChanged(const char* szAssetPath);

	// Drain pending reloads + pump the directory watcher. Called once per frame
	// from Zenith_MainLoop before the scene update.
	static void Update();

	static u_int GetReloadCount();
	static const char* GetLastStatusLine();
};

#endif // ZENITH_TOOLS
