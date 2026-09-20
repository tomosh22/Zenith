//------------------------------------------------------------------------------
// Zenith_UserSettings unit tests (B12) — the persisted player-preference store
// and the engine-owned boot order that reads it. Included at the bottom of
// Zenith_UserSettings.cpp (the module-owns-its-tests pattern), which is what
// gives them access to the file-local v1 codec.
//
// TWO LEVELS, deliberately:
//   * the CODEC, against a FROZEN byte image. A settings file written by one
//     build is read by the next, so the v1 layout is a compatibility promise
//     and re-deriving the expected bytes from the writer would prove nothing.
//   * the STORE, against real disk I/O inside a Zenith_SaveData
//     ScopedSaveRootForTest, so every test owns its own directory and none of
//     them can see (or disturb) the setting this process loaded at boot.
//
// Every instance here is LOCAL — a local Zenith_UserSettings, and where a
// profile is needed a local Zenith_Input + Zenith_Pointers + Zenith_InputActions
// rig (B13). The engine's own store and action layer are never touched.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"
#include "Core/Zenith_CommandLine.h"
#include "Input/Zenith_Pointers.h"

#if defined(ZENITH_TESTING) && defined(ZENITH_INPUT_SIMULATOR)

#include <filesystem>

namespace
{
	// THE FROZEN v1 IMAGE for the name "P_GAMEPAD". Hand-written bytes, never
	// generated: this array IS the format promise.
	//   [0..8]   'P','_','G','A','M','E','P','A','D'
	//   [9..15]  the terminator + zero padding to the end of the 16-byte field
	//   [16..31] the reserved tail, zero
	const u_int8 g_auUserSettingsCannedV1[Zenith_UserSettings::uPAYLOAD_SIZE] = {
		0x50, 0x5F, 0x47, 0x41, 0x4D, 0x45, 0x50, 0x41,
		0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};
	const char* const g_szUserSettingsCannedName = "P_GAMEPAD";

	// A scratch directory under the OS temp dir, wiped on entry so a test never
	// inherits a previous run's file. Uniquified by the caller's tag.
	std::filesystem::path UserSettingsTest_ScratchDir(const char* szTag)
	{
		std::error_code xEC;
		const std::filesystem::path xRoot =
			std::filesystem::temp_directory_path(xEC) / "zenith_usersettings_tests" / szTag;
		std::filesystem::remove_all(xRoot, xEC);
		std::filesystem::create_directories(xRoot, xEC);
		return xRoot;
	}

	void UserSettingsTest_RemoveScratchDir(const std::filesystem::path& xRoot)
	{
		std::error_code xEC;
		std::filesystem::remove_all(xRoot, xEC);
	}

	// Writes an ARBITRARY payload into the user_settings slot through the real
	// SaveData path (header + CRC and all), so the corrupt-blob tests exercise
	// what a damaged file on a player's disk would actually do.
	struct UserSettingsTest_RawPayload
	{
		const u_int8* m_puBytes = nullptr;
		u_int64       m_ulSize  = 0;
	};

	void UserSettingsTest_WriteRawPayload(Zenith_DataStream& xStream, void* pxUserData)
	{
		const UserSettingsTest_RawPayload* pxRaw = static_cast<const UserSettingsTest_RawPayload*>(pxUserData);
		if (pxRaw->m_ulSize > 0)
		{
			xStream.WriteData(pxRaw->m_puBytes, pxRaw->m_ulSize);
		}
	}

	void UserSettingsTest_SaveRaw(const u_int8* puBytes, u_int64 ulSize, u_int32 uVersion)
	{
		UserSettingsTest_RawPayload xRaw;
		xRaw.m_puBytes = puBytes;
		xRaw.m_ulSize  = ulSize;
		Zenith_SaveData::Save(Zenith_UserSettings::kszSLOT_NAME, uVersion,
			&UserSettingsTest_WriteRawPayload, &xRaw);
	}

	// A LOCAL action layer with two named profiles — the thing a persisted name
	// has to resolve against.
	static constexpr u_int8 uUS_TEST_PROFILE_DESKTOP = 40;
	static constexpr u_int8 uUS_TEST_PROFILE_GAMEPAD = 41;

	struct UserSettingsTest_ActionRig
	{
		Zenith_Input        m_xInput;
		Zenith_Pointers     m_xPointers;
		Zenith_InputActions m_xActions;

		UserSettingsTest_ActionRig()
		{
			m_xActions.Initialise(m_xInput, m_xPointers);
			m_xActions.RegisterProfile(uUS_TEST_PROFILE_DESKTOP, "P_DESKTOP",
				uINPUT_SCHEME_MASK_KEYBOARD | uINPUT_SCHEME_MASK_MOUSE);
			m_xActions.RegisterProfile(uUS_TEST_PROFILE_GAMEPAD, "P_GAMEPAD",
				uINPUT_SCHEME_MASK_GAMEPAD);
		}
	};
}

// ============================================================================
// The v1 codec, against the frozen image
// ============================================================================

ZENITH_TEST(UserSettings, CannedV1BlobParsesToItsFields)
{
	// THE compatibility test: a byte image written by an earlier build must
	// still parse to the same fields. If this fails, every player's saved
	// preference has just been reinterpreted.
	char acName[Zenith_UserSettings::uPROFILE_NAME_CAPACITY] = {};
	ZENITH_ASSERT_TRUE(Zenith_UserSettings_ParseV1Payload(g_auUserSettingsCannedV1,
		Zenith_UserSettings::uPAYLOAD_SIZE, acName), "the canned v1 image must parse");
	ZENITH_ASSERT_STREQ(acName, g_szUserSettingsCannedName, "and yield the name it encodes");

	// ...and the WRITER still emits exactly those bytes. Both directions, or
	// the frozen image only pins half the format.
	u_int8 auBuilt[Zenith_UserSettings::uPAYLOAD_SIZE] = {};
	Zenith_UserSettings_BuildV1Payload(g_szUserSettingsCannedName, auBuilt);
	ZENITH_ASSERT_EQ(std::memcmp(auBuilt, g_auUserSettingsCannedV1, Zenith_UserSettings::uPAYLOAD_SIZE), 0,
		"the writer must reproduce the frozen v1 image byte for byte");

	// The RESERVED tail is ignored on read — that is what makes a later v1
	// field additive rather than a version bump.
	u_int8 auWithReserved[Zenith_UserSettings::uPAYLOAD_SIZE] = {};
	std::memcpy(auWithReserved, g_auUserSettingsCannedV1, Zenith_UserSettings::uPAYLOAD_SIZE);
	auWithReserved[Zenith_UserSettings::uPROFILE_NAME_CAPACITY] = 0x7F;
	auWithReserved[Zenith_UserSettings::uPAYLOAD_SIZE - 1]      = 0x01;
	char acReservedName[Zenith_UserSettings::uPROFILE_NAME_CAPACITY] = {};
	ZENITH_ASSERT_TRUE(Zenith_UserSettings_ParseV1Payload(auWithReserved,
		Zenith_UserSettings::uPAYLOAD_SIZE, acReservedName), "a used reserved tail must still parse");
	ZENITH_ASSERT_STREQ(acReservedName, g_szUserSettingsCannedName, "and must not disturb the name");

	// An empty name field is the persisted spelling of AUTO.
	u_int8 auAuto[Zenith_UserSettings::uPAYLOAD_SIZE] = {};
	char acAutoName[Zenith_UserSettings::uPROFILE_NAME_CAPACITY] = { 'x', '\0' };
	ZENITH_ASSERT_TRUE(Zenith_UserSettings_ParseV1Payload(auAuto,
		Zenith_UserSettings::uPAYLOAD_SIZE, acAutoName), "an all-zero blob is a VALID v1 blob");
	ZENITH_ASSERT_STREQ(acAutoName, "", "and means AUTO");
}

ZENITH_TEST(UserSettings, CorruptOrShortBlobParsesToDefaults)
{
	// Every malformed shape must land on AUTO with an empty name and no crash.
	// A settings file is the one file a player can corrupt by accident, so it
	// must never be able to stop the game booting.
	char acName[Zenith_UserSettings::uPROFILE_NAME_CAPACITY] = {};

	ZENITH_ASSERT_FALSE(Zenith_UserSettings_ParseV1Payload(nullptr,
		Zenith_UserSettings::uPAYLOAD_SIZE, acName), "a null blob must be refused");
	ZENITH_ASSERT_STREQ(acName, "", "and leave AUTO");

	// One byte short of the frozen payload is not a v1 blob.
	ZENITH_ASSERT_FALSE(Zenith_UserSettings_ParseV1Payload(g_auUserSettingsCannedV1,
		Zenith_UserSettings::uPAYLOAD_SIZE - 1, acName), "a short blob must be refused");
	ZENITH_ASSERT_STREQ(acName, "", "and leave AUTO");
	ZENITH_ASSERT_FALSE(Zenith_UserSettings_ParseV1Payload(g_auUserSettingsCannedV1, 0, acName),
		"a zero-length blob must be refused");

	// A name field with no terminator inside its own capacity.
	u_int8 auUnterminated[Zenith_UserSettings::uPAYLOAD_SIZE] = {};
	for (u_int32 u = 0; u < Zenith_UserSettings::uPROFILE_NAME_CAPACITY; u++)
	{
		auUnterminated[u] = 'A';
	}
	ZENITH_ASSERT_FALSE(Zenith_UserSettings_ParseV1Payload(auUnterminated,
		Zenith_UserSettings::uPAYLOAD_SIZE, acName), "an unterminated name must be refused");
	ZENITH_ASSERT_STREQ(acName, "", "and leave AUTO");

	// Non-printable bytes inside the name.
	u_int8 auBinary[Zenith_UserSettings::uPAYLOAD_SIZE] = {};
	std::memcpy(auBinary, g_auUserSettingsCannedV1, Zenith_UserSettings::uPAYLOAD_SIZE);
	auBinary[2] = 0x01;
	ZENITH_ASSERT_FALSE(Zenith_UserSettings_ParseV1Payload(auBinary,
		Zenith_UserSettings::uPAYLOAD_SIZE, acName), "a non-printable byte in the name must be refused");

	// Garbage AFTER the terminator: the field is zero-PADDED by contract, so a
	// non-zero pad byte means these bytes were not written by this engine.
	u_int8 auDirtyPad[Zenith_UserSettings::uPAYLOAD_SIZE] = {};
	std::memcpy(auDirtyPad, g_auUserSettingsCannedV1, Zenith_UserSettings::uPAYLOAD_SIZE);
	auDirtyPad[Zenith_UserSettings::uPROFILE_NAME_CAPACITY - 1] = 'Z';
	ZENITH_ASSERT_FALSE(Zenith_UserSettings_ParseV1Payload(auDirtyPad,
		Zenith_UserSettings::uPAYLOAD_SIZE, acName), "a non-zero pad byte must be refused");
	ZENITH_ASSERT_STREQ(acName, "", "and leave AUTO");
}

// ============================================================================
// The store, against real disk
// ============================================================================

ZENITH_TEST(UserSettings, AbsentFileLoadsDefaults)
{
	// A first run. The overwhelmingly common case, and it must be silent-clean:
	// no override, no crash, no file created by the read.
	const std::filesystem::path xScratch = UserSettingsTest_ScratchDir("absent_file");
	{
		Zenith_SaveData::ScopedSaveRootForTest xScope(xScratch.string().c_str());
		ZENITH_ASSERT_FALSE(Zenith_SaveData::SlotExists(Zenith_UserSettings::kszSLOT_NAME),
			"the scratch root starts with no settings slot");

		Zenith_UserSettings xSettings;
		xSettings.Load();
		ZENITH_ASSERT_FALSE(xSettings.HasProfileOverride(), "an absent file must leave AUTO");
		ZENITH_ASSERT_STREQ(xSettings.GetProfileOverrideName(), "", "with an empty name");
		ZENITH_ASSERT_FALSE(Zenith_SaveData::SlotExists(Zenith_UserSettings::kszSLOT_NAME),
			"and reading must not create the file");
	}
	UserSettingsTest_RemoveScratchDir(xScratch);
}

ZENITH_TEST(UserSettings, OverrideNameRoundTripsByteStable)
{
	// Write through the real API, then read back with a SECOND, independent
	// store instance -- and check the bytes that actually reached the slot are
	// the frozen image, not merely something this build happens to re-read.
	const std::filesystem::path xScratch = UserSettingsTest_ScratchDir("round_trip");
	{
		Zenith_SaveData::ScopedSaveRootForTest xScope(xScratch.string().c_str());

		Zenith_UserSettings xWriter;
		ZENITH_ASSERT_TRUE(xWriter.SetProfileOverrideName(g_szUserSettingsCannedName),
			"a name that fits the field must persist");
		ZENITH_ASSERT_TRUE(xWriter.HasProfileOverride(), "and be held in memory");
		ZENITH_ASSERT_STREQ(xWriter.GetProfileOverrideName(), g_szUserSettingsCannedName, "verbatim");
		ZENITH_ASSERT_TRUE(Zenith_SaveData::SlotExists(Zenith_UserSettings::kszSLOT_NAME),
			"the slot must exist on disk afterwards");

		// The recorded payload is the game-level bytes, header and CRC excluded.
		const Zenith_Vector<Zenith_SaveData::WrittenSlot>& xWritten = Zenith_SaveData::GetWrittenSlotsForTest();
		ZENITH_ASSERT_GT(xWritten.GetSize(), 0u, "the write must have been recorded");
		const Zenith_SaveData::WrittenSlot& xLast = xWritten.Get(xWritten.GetSize() - 1);
		ZENITH_ASSERT_STREQ(xLast.m_strSlotName.c_str(), Zenith_UserSettings::kszSLOT_NAME, "into our slot");
		ZENITH_ASSERT_EQ(xLast.m_uGameVersion, Zenith_UserSettings::uVERSION, "stamped v1");
		ZENITH_ASSERT_EQ(xLast.m_xPayload.GetSize(), Zenith_UserSettings::uPAYLOAD_SIZE,
			"a v1 payload is exactly the frozen size");
		ZENITH_ASSERT_EQ(std::memcmp(&xLast.m_xPayload.Get(0), g_auUserSettingsCannedV1,
			Zenith_UserSettings::uPAYLOAD_SIZE), 0, "and byte-identical to the frozen image");

		Zenith_UserSettings xReader;
		xReader.Load();
		ZENITH_ASSERT_STREQ(xReader.GetProfileOverrideName(), g_szUserSettingsCannedName,
			"a fresh store must read back exactly what was written");

		// ...and back to AUTO, which must also survive the round trip (an empty
		// name is a real setting, not "no setting").
		ZENITH_ASSERT_TRUE(xWriter.SetProfileOverrideName(nullptr), "nullptr stores AUTO");
		ZENITH_ASSERT_FALSE(xWriter.HasProfileOverride(), "which is no override");
		Zenith_UserSettings xAutoReader;
		xAutoReader.Load();
		ZENITH_ASSERT_FALSE(xAutoReader.HasProfileOverride(), "and reads back as AUTO");
	}
	UserSettingsTest_RemoveScratchDir(xScratch);
}

ZENITH_TEST(UserSettings, CorruptFileOnDiskLoadsDefaults)
{
	// The same malformed shapes as the codec test, but arriving through a real
	// .zsave with a valid header and CRC -- i.e. a file that IS ours and whose
	// PAYLOAD is wrong, which is what a truncated write leaves behind.
	const std::filesystem::path xScratch = UserSettingsTest_ScratchDir("corrupt_file");
	{
		Zenith_SaveData::ScopedSaveRootForTest xScope(xScratch.string().c_str());

		const u_int8 auTruncated[4] = { 'P', '_', 'G', 'A' };
		UserSettingsTest_SaveRaw(auTruncated, sizeof(auTruncated), Zenith_UserSettings::uVERSION);
		Zenith_UserSettings xShort;
		xShort.Load();
		ZENITH_ASSERT_FALSE(xShort.HasProfileOverride(), "a truncated payload must load as AUTO");

		// A payload that is fine but a VERSION this build does not know.
		UserSettingsTest_SaveRaw(g_auUserSettingsCannedV1, Zenith_UserSettings::uPAYLOAD_SIZE,
			Zenith_UserSettings::uVERSION + 1u);
		Zenith_UserSettings xFuture;
		xFuture.Load();
		ZENITH_ASSERT_FALSE(xFuture.HasProfileOverride(),
			"a future version must load as AUTO rather than be read as v1");

		// ...and the same bytes at v1 DO load, so the version gate above is the
		// thing that rejected it and not the payload.
		UserSettingsTest_SaveRaw(g_auUserSettingsCannedV1, Zenith_UserSettings::uPAYLOAD_SIZE,
			Zenith_UserSettings::uVERSION);
		Zenith_UserSettings xGood;
		xGood.Load();
		ZENITH_ASSERT_STREQ(xGood.GetProfileOverrideName(), g_szUserSettingsCannedName,
			"the identical payload at v1 must load");
	}
	UserSettingsTest_RemoveScratchDir(xScratch);
}

ZENITH_TEST(UserSettings, TooLongProfileNameIsRefusedNotTruncated)
{
	// A truncated name would resolve to a DIFFERENT profile, or to none. The
	// only safe answer is to refuse the write and keep the previous setting.
	const std::filesystem::path xScratch = UserSettingsTest_ScratchDir("name_too_long");
	{
		Zenith_SaveData::ScopedSaveRootForTest xScope(xScratch.string().c_str());

		Zenith_UserSettings xSettings;
		ZENITH_ASSERT_TRUE(xSettings.SetProfileOverrideName("P_DESKTOP"), "a short name persists");

		// 16 characters: one too many for a 16-byte field that must hold a
		// terminator. This is the exact boundary.
		ZENITH_ASSERT_FALSE(xSettings.SetProfileOverrideName("P_SIXTEEN_CHARS!"),
			"a name that needs the terminator's byte must be refused");
		ZENITH_ASSERT_STREQ(xSettings.GetProfileOverrideName(), "P_DESKTOP",
			"and the previous setting must survive the refusal");

		// 15 characters + terminator is the largest that fits.
		ZENITH_ASSERT_TRUE(xSettings.SetProfileOverrideName("P_FIFTEEN_CHAR"), "14 fits");
		ZENITH_ASSERT_TRUE(xSettings.SetProfileOverrideName("P_FIFTEEN_CHARS"), "15 fits exactly");
		ZENITH_ASSERT_STREQ(xSettings.GetProfileOverrideName(), "P_FIFTEEN_CHARS", "verbatim");
	}
	UserSettingsTest_RemoveScratchDir(xScratch);
}

// ============================================================================
// Boot step 4 — validate + apply BY NAME
// ============================================================================

ZENITH_TEST(UserSettings, KnownProfileNameIsForcedAtBoot)
{
	const std::filesystem::path xScratch = UserSettingsTest_ScratchDir("apply_known");
	{
		Zenith_SaveData::ScopedSaveRootForTest xScope(xScratch.string().c_str());

		UserSettingsTest_ActionRig xRig;
		ZENITH_ASSERT_FALSE(xRig.m_xActions.IsProfileOverridden(), "a fresh action layer is on AUTO");

		Zenith_UserSettings xSettings;
		xSettings.SetProfileOverrideName("P_GAMEPAD");

		ZENITH_ASSERT_TRUE(xSettings.ApplyProfileOverride(xRig.m_xActions),
			"a registered name must be applied");
		ZENITH_ASSERT_TRUE(xRig.m_xActions.IsProfileOverridden(), "the auto-switch is suspended");
		ZENITH_ASSERT_EQ(xRig.m_xActions.GetActiveProfile(), uUS_TEST_PROFILE_GAMEPAD,
			"and the named profile is the active one");

		// The NAME is the identity: the same name resolves to the id the game
		// happened to give it, whatever that number is.
		ZENITH_ASSERT_EQ(xRig.m_xActions.FindProfileByName("P_DESKTOP"), uUS_TEST_PROFILE_DESKTOP,
			"lookup by name resolves the other profile too");
		ZENITH_ASSERT_STREQ(xRig.m_xActions.GetProfileName(uUS_TEST_PROFILE_GAMEPAD), "P_GAMEPAD",
			"and the reverse lookup round-trips");
	}
	UserSettingsTest_RemoveScratchDir(xScratch);
}

ZENITH_TEST(UserSettings, UnknownProfileNameStaysAuto)
{
	// The renaming/reordering case. A saved name whose profile is gone must
	// degrade to AUTO -- never to "whatever now holds that slot", which is
	// exactly what persisting a numeric id would have done.
	const std::filesystem::path xScratch = UserSettingsTest_ScratchDir("apply_unknown");
	{
		Zenith_SaveData::ScopedSaveRootForTest xScope(xScratch.string().c_str());

		UserSettingsTest_ActionRig xRig;
		const u_int8 uProfileBefore = xRig.m_xActions.GetActiveProfile();

		Zenith_UserSettings xSettings;
		xSettings.SetProfileOverrideName("P_RETIRED");

		ZENITH_ASSERT_FALSE(xSettings.ApplyProfileOverride(xRig.m_xActions),
			"an unregistered name must not be applied");
		ZENITH_ASSERT_FALSE(xRig.m_xActions.IsProfileOverridden(), "the action layer stays on AUTO");
		ZENITH_ASSERT_EQ(xRig.m_xActions.GetActiveProfile(), uProfileBefore,
			"and the active profile is untouched");

		// The setting itself is NOT rewritten: a build where the profile is
		// temporarily absent must not silently discard the player's choice.
		ZENITH_ASSERT_STREQ(xSettings.GetProfileOverrideName(), "P_RETIRED",
			"the persisted name survives a failed resolve");

		// AUTO applies nothing, and says so.
		Zenith_UserSettings xAuto;
		ZENITH_ASSERT_FALSE(xAuto.ApplyProfileOverride(xRig.m_xActions), "AUTO applies no override");
		ZENITH_ASSERT_FALSE(xRig.m_xActions.IsProfileOverridden(), "and leaves the layer alone");

		// An empty name must never match a profile, even one with an empty name.
		ZENITH_ASSERT_EQ(xRig.m_xActions.FindProfileByName(""), Zenith_InputActions::uPROFILE_AUTO,
			"an empty name is AUTO, not a lookup key");
		ZENITH_ASSERT_EQ(xRig.m_xActions.FindProfileByName(nullptr), Zenith_InputActions::uPROFILE_AUTO,
			"and neither is null");
	}
	UserSettingsTest_RemoveScratchDir(xScratch);
}

// ============================================================================
// Boot step 1 — the single-init contract
// ============================================================================

ZENITH_TEST(UserSettings, BootInitDoesNotWipeAutomatedTestSandbox)
{
	// THE contract this WP moved SaveData::Initialise for. Zenith_SaveData
	// wipes stale .zsave files out of an automated-test sandbox from INSIDE
	// Initialise -- correct exactly once, at boot, to clear a previous run's
	// residue. A SECOND Initialise re-enters that wipe mid-run and deletes
	// every slot the batch has written so far, silently.
	//
	// Zenith_Engine::InitialiseProject is now the only caller (the three
	// game-owned calls in Zenithmon / DevilsPlayground / TilePuzzle were
	// removed in the same change), so the count is the structural pin: re-add
	// one anywhere and this fails.
	ZENITH_ASSERT_EQ(Zenith_SaveData::GetInitialiseCallCount(), 1u,
		"SaveData::Initialise must run exactly ONCE per process, from engine boot -- "
		"a second call re-enters the sandbox wipe and would delete a live test batch's slots");

	// ...and boot picked the right root. In an automated-test process the whole
	// save directory IS the run's sandbox, which is what isolates user_settings
	// (and every other slot) from the player's real save data; in any other run
	// there is no sandbox at all.
	if (Zenith_CommandLine::IsAutomatedTestRun())
	{
		ZENITH_ASSERT_TRUE(Zenith_SaveData::IsUsingTestSandbox(),
			"an automated-test run must have been sandboxed by the boot Initialise -- "
			"otherwise this run is reading and writing the player's real user_settings");
	}
	else
	{
		ZENITH_ASSERT_FALSE(Zenith_SaveData::IsUsingTestSandbox(),
			"a plain boot establishes no sandbox");
		// Which means the wipe path cannot reach ANY file from here: it refuses
		// to act unless this process established the sandbox itself.
		ZENITH_ASSERT_EQ(Zenith_SaveData::WipeTestSandbox(), 0u,
			"with no sandbox established the wipe must delete nothing");
	}

	ZENITH_ASSERT_TRUE(Zenith_SaveData::GetSaveDirectory()[0] != '\0',
		"engine boot leaves every game with a live save root, including the three that never called Initialise");

	// The coexistence half: an ESTABLISHED root full of slots is untouched by
	// the boot that already happened, and stays readable across scopes.
	const std::filesystem::path xScratch = UserSettingsTest_ScratchDir("boot_coexist");
	{
		Zenith_SaveData::ScopedSaveRootForTest xScope(xScratch.string().c_str());
		Zenith_UserSettings xSettings;
		ZENITH_ASSERT_TRUE(xSettings.SetProfileOverrideName("P_DESKTOP"), "establish a slot");
	}
	{
		Zenith_SaveData::ScopedSaveRootForTest xScope(xScratch.string().c_str());
		ZENITH_ASSERT_TRUE(Zenith_SaveData::SlotExists(Zenith_UserSettings::kszSLOT_NAME),
			"the established slot must still be there");
		Zenith_UserSettings xSettings;
		xSettings.Load();
		ZENITH_ASSERT_STREQ(xSettings.GetProfileOverrideName(), "P_DESKTOP", "and still readable");
	}
	UserSettingsTest_RemoveScratchDir(xScratch);
}

#endif // ZENITH_TESTING && ZENITH_INPUT_SIMULATOR
