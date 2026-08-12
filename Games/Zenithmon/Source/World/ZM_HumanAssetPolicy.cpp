#include "Zenith.h"

#include "Zenithmon/Source/World/ZM_HumanAssetPolicy.h"

#include "Zenithmon/Source/Gen/ZM_BakeManifest.h"   // ZM_BakeManifestCheck + ZM_ASSET_FAMILY_HUMANS
#include "Zenithmon/Source/Gen/ZM_HumanGen.h"       // ZM_BakeAllHumans (tools) / its non-tools no-op

#include <filesystem>

namespace
{
	bool ZM_DefaultHumansAreWarm()
	{
		// ★ GAME_ASSETS_DIR is "" on Android (AGDE resolves game assets through
		// AAssetManager by relative path, not a real filesystem tree -- see
		// Zenith/Android/CLAUDE.md). ZM_BakeManifestCheck FAILS OPEN on an empty
		// root ("any doubt returns false so the family re-bakes"), which is
		// exactly wrong here: there is no filesystem tree to stamp-check against,
		// and ZM_BakeAllHumans is an unconditional no-op on Android (tools-only),
        // so the "try to bake, then re-ask" fallback in ZM_AreHumanAssetsReady
		// can NEVER become warm -- every human on Android permanently rendered as
		// the cold-start palette block, regardless of whether Humans/*.zmodel was
		// actually baked and bundled into the APK. Android never iterates on a
		// local bake tree: whatever shipped in the APK IS the bake, so trust it
		// and let the ordinary (already null-safe) asset-load path be the one
		// place a genuinely missing file surfaces.
		if (GAME_ASSETS_DIR[0] == '\0')
		{
			return true;
		}
		return ZM_BakeManifestCheck(ZM_ASSET_FAMILY_HUMANS,
			std::filesystem::path(GAME_ASSETS_DIR));
	}

	bool ZM_DefaultTryBakeHumans()
	{
		// Tools-only in substance: ZM_BakeAllHumans is an inline `return false` in
		// every non-tools config, so there is no #ifdef to keep in step here. It also
		// carries its own per-family guard, so a warm tree costs a stamp check.
		return ZM_BakeAllHumans();
	}

	ZM_HumanAssetPolicy ZM_MakeDefaultHumanAssetPolicy()
	{
		ZM_HumanAssetPolicy xPolicy;
		xPolicy.m_pfnIsWarm      = &ZM_DefaultHumansAreWarm;
		xPolicy.m_pfnTryBake     = &ZM_DefaultTryBakeHumans;
		xPolicy.m_bBakeAttempted = false;
		return xPolicy;
	}

	// Function-local static: no static-init-order hazard, and the first reader
	// constructs it (the codebase-wide rule for module state that cannot live on
	// g_xEngine because it is game-side).
	ZM_HumanAssetPolicy& ZM_MutableHumanAssetPolicy()
	{
		static ZM_HumanAssetPolicy s_xPolicy = ZM_MakeDefaultHumanAssetPolicy();
		return s_xPolicy;
	}
}

const ZM_HumanAssetPolicy& ZM_GetHumanAssetPolicy()
{
	return ZM_MutableHumanAssetPolicy();
}

void ZM_SetHumanAssetPolicy(const ZM_HumanAssetPolicy& xPolicy)
{
	ZM_MutableHumanAssetPolicy() = xPolicy;
}

void ZM_ResetHumanAssetPolicy()
{
	ZM_MutableHumanAssetPolicy() = ZM_MakeDefaultHumanAssetPolicy();
}

ZM_HumanAssetPolicy ZM_MakeForcedColdHumanAssetPolicy()
{
	ZM_HumanAssetPolicy xPolicy;
	xPolicy.m_pfnIsWarm      = []() { return false; };
	xPolicy.m_pfnTryBake     = []() { return false; };
	xPolicy.m_bBakeAttempted = false;
	return xPolicy;
}

bool ZM_AreHumanAssetsReady()
{
	ZM_HumanAssetPolicy& xPolicy = ZM_MutableHumanAssetPolicy();

	// WARMTH IS RE-QUERIED, NEVER CACHED. A cached "cold" would outlive the very
	// bake below and leave the runtime disagreeing with the manifest on disk.
	if (xPolicy.m_pfnIsWarm != nullptr && xPolicy.m_pfnIsWarm())
	{
		return true;
	}

	// ONLY THE ATTEMPT IS LATCHED, and it is latched BEFORE the call so a bake that
	// fails (or asserts its way out) cannot be retried once per human, per scene
	// load, forever.
	if (!xPolicy.m_bBakeAttempted)
	{
		xPolicy.m_bBakeAttempted = true;
		if (xPolicy.m_pfnTryBake != nullptr)
		{
			xPolicy.m_pfnTryBake();
		}
	}

	// Re-ask rather than trusting TryBake's return: the manifest on disk is the
	// authority on whether a .zmodel can actually be loaded.
	return xPolicy.m_pfnIsWarm != nullptr && xPolicy.m_pfnIsWarm();
}
