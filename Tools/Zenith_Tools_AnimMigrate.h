#pragma once

#include <filesystem>
#include <string>

// ============================================================================
// Zenith_Tools_AnimMigrate -- THE ONE SANCTIONED READER OF AN OLDER .zanim
// LAYOUT, and it exists for exactly one population of files.
//
// ★ READ THIS BEFORE ASSUMING IT CONTRADICTS THE RULING. It does not. The
// ruling in Zenith_StreamEnvelope.cpp says:
//
//     "Every asset file lives under the `**/Assets/**` gitignore and is BAKE
//      OUTPUT: a file in an older layout is a stale bake, and the fix is to
//      delete it and let the tools boot rewrite it. There is no such file
//      anywhere that is not reproducible from its source."
//
// That holds for all 931 generated clips and always will -- their fix is
// `del` plus a boot, and nothing here changes that. It stops holding for
// exactly one population: the clips under `Assets/Authored/`, which are
// re-included WHOLESALE by .gitignore (D33), committed, hand-edited in the
// Animation Editor (Zenith_AnimationDocument::PromoteToAuthoredOverride /
// SaveAs), and written by NO generator. Delete one and it is simply gone.
// So a schema bump that costs a generated clip nothing would destroy an
// authored one, and this is what carries it across instead (D22).
//
// ★ IT IS A BOOT PHASE, NOT A PROGRAM (D23). Like every other Zenith_Tools_*
// exporter it runs inside the tools boot, pre-Flux, with no device and no
// scene -- see Tools/CLAUDE.md. `Zenith_Tools_MigrateAuthoredClipsAtBoot()` is
// the phase entry point; it is CHEAP when the authored roots do not exist,
// which is the normal case today.
//
// ★ THE RUNTIME READER IS UNTOUCHED, AND MUST STAY SO. Flux_AnimationClip::
// ParseStream still refuses any schema that is not
// uZENITH_ANIMATION_SCHEMA_CURRENT (D2). The older layout is reached ONLY
// through Flux_AnimationClip::ParsePayload(stream, schema), whose non-current
// branch is compiled out entirely outside ZENITH_TOOLS. If you find yourself
// wanting a legacy branch in the runtime reader, the answer is a step in the
// chain below instead.
//
// ★ NO FILE IS EVER REWRITTEN IN PLACE. Each migrated clip is serialized to a
// sibling `.migrate.tmp`, RE-PARSED with the RUNTIME reader (ParseStream, at
// the current schema), and only then renamed over the original. A corrupt
// input therefore cannot produce a truncated output: the original is still
// there, byte for byte, and the file is reported as failed.
//
// ★ IT DOES NOT GO THROUGH Zenith_AssetRegistry. It reads and writes raw
// files. Loading a clip through the registry during a boot phase would cache a
// pre-migration asset that every later consumer would then resolve to.
// ============================================================================

//-----------------------------------------------------------------------------
// What one walk did. Every field is a COUNT OF FILES, and they always satisfy
// m_uScanned == m_uMigrated + m_uSkippedCurrent + m_uFailed (CountsAddUp) --
// a file that is neither carried forward, nor already current, nor refused
// would be a file the walk silently dropped, which is the failure mode this
// struct exists to make impossible to miss.
//-----------------------------------------------------------------------------
struct Zenith_Tools_AnimMigrateReport
{
	// .zanim files found under the root (recursively).
	u_int m_uScanned = 0u;
	// Read at an older schema, stepped forward, and rewritten at the current one.
	u_int m_uMigrated = 0u;
	// Already at the current schema. A NO-OP, not an error -- the file is not
	// opened for writing at all, so its bytes are untouched.
	u_int m_uSkippedCurrent = 0u;
	// Refused: not an animation envelope, a schema from a newer build, a payload
	// that does not parse, a ticks-per-second of zero, or a write that did not
	// re-parse. The file is left EXACTLY as it was found in every one of those.
	u_int m_uFailed = 0u;

	// The first refusal, so a caller can name something actionable without
	// re-walking. Every refusal is also logged as it happens.
	std::string m_strFirstFailurePath;
	std::string m_strFirstFailureReason;

	bool CountsAddUp() const
	{
		return m_uScanned == (m_uMigrated + m_uSkippedCurrent + m_uFailed);
	}

	void Accumulate(const Zenith_Tools_AnimMigrateReport& xOther)
	{
		m_uScanned += xOther.m_uScanned;
		m_uMigrated += xOther.m_uMigrated;
		m_uSkippedCurrent += xOther.m_uSkippedCurrent;
		m_uFailed += xOther.m_uFailed;
		if (m_strFirstFailurePath.empty() && !xOther.m_strFirstFailurePath.empty())
		{
			m_strFirstFailurePath = xOther.m_strFirstFailurePath;
			m_strFirstFailureReason = xOther.m_strFirstFailureReason;
		}
	}
};

// Walk *.zanim under xAuthoredRoot (recursively) and carry every one that is
// behind the current schema forward to it.
//
// Returns TRUE when nothing was refused -- i.e. every scanned file is now at
// uZENITH_ANIMATION_SCHEMA_CURRENT. A root that does not exist, is not a
// directory, or holds no .zanim is SUCCESS with an all-zero report: an absent
// authored tree is the normal state, not a failure (the same rule
// ImportGlbsInDirectory follows for a missing source directory).
//
// xOutReport is ACCUMULATED into, not cleared, so several roots can share one.
bool Zenith_Tools_MigrateAuthoredClips(const std::filesystem::path& xAuthoredRoot,
	Zenith_Tools_AnimMigrateReport& xOutReport);

// The boot phase: both authored roots -- ENGINE_ASSETS_DIR/Authored/ and the
// game's, resolved at RUNTIME via Zenith_AssetRegistry::ResolvePath("game:...")
// because GAME_ASSETS_DIR is a per-GAME define and does not exist in the engine
// library this file compiles into. Between them that is exactly where
// Zenith_AnimationDocument::BuildAuthoredAssetPath puts a promoted clip -- and
// one log line reporting the merged result.
void Zenith_Tools_MigrateAuthoredClipsAtBoot();
