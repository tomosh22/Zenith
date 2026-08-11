#pragma once

class Zenith_InputActions;

// ============================================================================
// Zenith_UserSettings — the engine-owned PLAYER PREFERENCE store (B12).
//
// One SaveData slot ("user_settings") per project, holding the settings the
// ENGINE owns rather than the game: today that is exactly one field, the input
// PROFILE OVERRIDE. A game's own progress lives in its own slots and never
// comes near this one.
//
// THE BOOT ORDER IS ENGINE-OWNED (Zenith_Engine::InitialiseProject):
//   1. Zenith_SaveData::Initialise(Project_GetName())  — the ONLY call in the
//      process; a game must never call it again (a second call re-enters the
//      cross-process residue wipe and would delete an automated-test batch's
//      slots mid-run).
//   2. UserSettings().Load()                           — read the slot.
//   3. Project_RegisterGameComponents()                — the game registers its
//      actions, bindings and PROFILES.
//   4. UserSettings().ApplyProfileOverride(Actions())  — validate the persisted
//      NAME against those profiles and force it, or stay on AUTO.
//
// Steps 2 and 4 are split precisely because a profile does not exist until
// step 3: the setting is read before the game is wired and validated after.
//
// PROFILES PERSIST BY STABLE NAME, NEVER BY NUMERIC ID. A game may reorder or
// renumber its RegisterProfile calls at any time; a saved setting must keep
// meaning the same profile, or quietly fall back to AUTO if that profile is
// gone. An id would silently reinterpret as whatever now holds that number.
//
// THE OVERRIDE APPLIES AT BOOT ONLY. Nothing re-applies it per frame, and the
// automated-test between-tests reset deliberately returns the action layer to
// AUTO. Automated-test runs are also isolated by construction: SaveData
// redirects the whole save root into the run's sandbox, so a test process
// never reads (or writes) the player's real user_settings.
//
// This is a PLAIN INSTANCE CLASS (the Zenith_InputActions / Zenith_Pointers
// pattern). The engine owns one — g_xEngine.UserSettings() — and unit tests
// construct LOCAL instances, so a test can never disturb the boot-loaded
// setting of the process it runs in.
// ============================================================================

class Zenith_UserSettings
{
public:
	Zenith_UserSettings() = default;
	~Zenith_UserSettings() = default;

	Zenith_UserSettings(const Zenith_UserSettings&) = delete;
	Zenith_UserSettings& operator=(const Zenith_UserSettings&) = delete;

	// The SaveData slot this store owns.
	static constexpr const char* kszSLOT_NAME = "user_settings";

	// Payload version, carried in the SaveData header's GAME-version field. A
	// file whose version is not this one is not a v1 blob and reads as defaults.
	static constexpr u_int32 uVERSION = 1;

	// v1 PAYLOAD LAYOUT — frozen. 32 bytes, no padding, no endianness (bytes):
	//   [0 .. 15]  acProfileOverrideName — ASCII, NUL-terminated, ZERO-PADDED to
	//              the full field. All-zero means AUTO (no override).
	//   [16 .. 31] reserved. Written as zero; IGNORED on read, so a later v1
	//              field can be added without invalidating existing files.
	// The capacity INCLUDES the terminator: a profile name of at most 15
	// characters can be persisted (every shipped profile name is far shorter).
	static constexpr u_int32 uPROFILE_NAME_CAPACITY = 16;
	static constexpr u_int32 uRESERVED_BYTES        = 16;
	static constexpr u_int32 uPAYLOAD_SIZE          = uPROFILE_NAME_CAPACITY + uRESERVED_BYTES;

	// Boot step 2. Reads the slot into memory. EVERY failure — absent file,
	// short or corrupt blob, a version this build does not know — leaves the
	// defaults (AUTO) and logs; nothing here can fail the boot.
	void Load();

	// Boot step 4. Resolves the persisted NAME against the profiles registered
	// on xActions and forces it via SetProfileOverride. An empty name (AUTO) or
	// a name no registered profile carries leaves the action layer alone, in
	// AUTO. Returns true iff an override was applied.
	bool ApplyProfileOverride(Zenith_InputActions& xActions);

	// "" when the setting is AUTO.
	const char* GetProfileOverrideName() const { return m_acProfileOverrideName; }
	bool HasProfileOverride() const { return m_acProfileOverrideName[0] != '\0'; }

	// Stores the setting and PERSISTS it immediately (one slot write). nullptr
	// or "" stores AUTO. A name too long for the field is REFUSED rather than
	// truncated — a truncated name would resolve to a different profile, or to
	// none, which is the one failure mode a name-keyed setting must not have.
	// Returns false (and changes nothing) on refusal.
	//
	// This writes the SETTING only; it does not touch the action layer. A
	// caller that wants the change to take effect now calls SetProfileOverride
	// itself — the two are deliberately separable, because the persisted value
	// is a BOOT preference and the live override is a frame-by-frame state.
	bool SetProfileOverrideName(const char* szName);

	// Back to AUTO in memory, WITHOUT writing to disk.
	void ResetToDefaults();

private:
	bool Save() const;

	char m_acProfileOverrideName[uPROFILE_NAME_CAPACITY] = {};
};
