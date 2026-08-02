#pragma once

// ============================================================================
// ZM_HumanAssetPolicy -- THE INJECTABLE READINESS SEAM for the human bake.
//
// THE QUESTION IT ANSWERS: "can a human .zmodel be loaded right now?" The visual
// asks it once per human at start; the answer decides between the real model and
// the cold-start fallback block.
//
// ★ WHY IT IS A SEAM AND NOT A BARE CALL. A cold start in a TOOLS build bakes and
// thereby becomes warm, so the fallback branch is otherwise UNREACHABLE from an
// automated test -- and the alternative way to force it (renaming or deleting the
// shared baked files) would race every other test in the batch and corrupt a
// developer's tree. Tests install a forced-cold policy through the scoped guard
// below instead.
//
// ★ THE LATCH LIVES IN THE POLICY, NOT IN A FILE STATIC. That is what stops a
// forced-cold TryBake() returning false from consuming the PRODUCTION policy's one
// attempt and leaving the restored default permanently unable to bake. Warmth
// itself is RE-QUERIED every time -- only the bake ATTEMPT is latched -- so a later
// successful bake, or a test querying the manifest directly, can never disagree
// with what the runtime believes.
//
// This is NOT the unconditional full-family boot bake that ZM_BakeManifest.h:71-75
// objects to. It is humans only (a strict subset of the four-family bake CI
// already pays for in the asset-gallery test), and only when a scene carrying
// humans actually loads.
// ============================================================================

// Captureless function pointers rather than virtuals, matching the codebase's
// existing callback idiom (Zenith_ECSRuntimeHooks, Zenith_AIWorldHooks, ...).
struct ZM_HumanAssetPolicy
{
	// Is the humans family baked and complete RIGHT NOW? Default:
	// ZM_BakeManifestCheck(ZM_ASSET_FAMILY_HUMANS, GAME_ASSETS_DIR).
	bool (*m_pfnIsWarm)() = nullptr;
	// Try to make it warm. Default: ZM_BakeAllHumans(), which is a tools-only bake
	// and a no-op returning false everywhere else.
	bool (*m_pfnTryBake)() = nullptr;
	// Has this policy already spent its one bake attempt? Deliberately part of the
	// policy VALUE, so installing and removing an override cannot leak a spent
	// latch into the production default. Never consulted for warmth.
	bool m_bBakeAttempted = false;
};

// The live policy. Read-only: the bake latch is mutated internally by
// ZM_AreHumanAssetsReady, and a caller that wants to change behaviour installs a
// whole new policy.
const ZM_HumanAssetPolicy& ZM_GetHumanAssetPolicy();

// Install a policy verbatim, INCLUDING its m_bBakeAttempted latch.
void ZM_SetHumanAssetPolicy(const ZM_HumanAssetPolicy& xPolicy);

// Restore the production default, with a FRESH latch.
void ZM_ResetHumanAssetPolicy();

// THE runtime question. Warm -> true. Otherwise spend the policy's single bake
// attempt (if it has one left) and re-ask. Never caches the answer.
bool ZM_AreHumanAssetsReady();

// Install a policy for a scope and restore the PRODUCTION DEFAULT on the way out.
// Restoring the default rather than the captured previous policy is deliberate:
// the default is the only policy the game is ever meant to run with, and a test
// that leaked one would otherwise hand its override to whatever ran next.
class ZM_ScopedHumanAssetPolicy
{
public:
	explicit ZM_ScopedHumanAssetPolicy(const ZM_HumanAssetPolicy& xPolicy)
	{
		ZM_SetHumanAssetPolicy(xPolicy);
	}
	~ZM_ScopedHumanAssetPolicy() { ZM_ResetHumanAssetPolicy(); }

	ZM_ScopedHumanAssetPolicy(const ZM_ScopedHumanAssetPolicy&) = delete;
	ZM_ScopedHumanAssetPolicy& operator=(const ZM_ScopedHumanAssetPolicy&) = delete;
};

// The canned "cold tree, and never try to bake" policy every forced-cold test
// wants. Spelled here so no test has to hand-roll (and mis-spell) it.
ZM_HumanAssetPolicy ZM_MakeForcedColdHumanAssetPolicy();
