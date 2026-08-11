#include "Zenith.h"

#include "Core/Zenith_UserSettings.h"

#include "Input/Zenith_InputActions.h"
#include "SaveData/Zenith_SaveData.h"

#include <cstring>

// ============================================================================
// The v1 CODEC — pure, and deliberately file-local.
//
// Both halves are total functions over bytes: no engine state, no I/O. That is
// what lets a unit pin the FROZEN byte image against a canned fixture, and what
// makes "write then read back" a byte-stability claim rather than a round-trip
// coincidence. (The Tests.inl at the bottom of this TU is the only other
// caller, exactly as Zenith_SaveData does with its sandbox helpers.)
// ============================================================================

// Writes the full 32-byte v1 payload. ZEROES FIRST: the name field is
// zero-PADDED and the reserved tail is zero by contract, so the emitted bytes
// are a pure function of the name — nothing of a previous, longer name can
// survive in the padding.
static void Zenith_UserSettings_BuildV1Payload(const char* szName,
	u_int8 (&auOut)[Zenith_UserSettings::uPAYLOAD_SIZE])
{
	std::memset(auOut, 0, Zenith_UserSettings::uPAYLOAD_SIZE);
	if (szName == nullptr)
	{
		return;
	}
	const size_t uLength = std::strlen(szName);
	// A name that does not fit is a caller error caught by SetProfileOverrideName;
	// emitting AUTO here rather than a truncation keeps the codec total.
	if (uLength >= Zenith_UserSettings::uPROFILE_NAME_CAPACITY)
	{
		return;
	}
	std::memcpy(auOut, szName, uLength);
}

// Parses a v1 payload. Returns false — leaving acOut EMPTY, i.e. AUTO — for
// every malformed input, because a settings file is the one file a player can
// corrupt by accident and it must never be able to stop the game booting.
static bool Zenith_UserSettings_ParseV1Payload(const u_int8* puBytes, u_int64 ulSize,
	char (&acOut)[Zenith_UserSettings::uPROFILE_NAME_CAPACITY])
{
	std::memset(acOut, 0, Zenith_UserSettings::uPROFILE_NAME_CAPACITY);

	// A blob shorter than the frozen payload is not a v1 blob. A LONGER one is
	// accepted and its tail ignored, so a future v1 field is additive.
	if (puBytes == nullptr || ulSize < Zenith_UserSettings::uPAYLOAD_SIZE)
	{
		return false;
	}

	// The name must terminate INSIDE its own field...
	u_int32 uLength = 0;
	while (uLength < Zenith_UserSettings::uPROFILE_NAME_CAPACITY && puBytes[uLength] != 0)
	{
		uLength++;
	}
	if (uLength == Zenith_UserSettings::uPROFILE_NAME_CAPACITY)
	{
		return false;
	}

	// ...be printable ASCII up to the terminator (a profile name is authored
	// source text, so anything else is corruption, not an exotic name)...
	for (u_int32 u = 0; u < uLength; u++)
	{
		if (puBytes[u] < 0x20 || puBytes[u] > 0x7E)
		{
			return false;
		}
	}

	// ...and be ZERO-PADDED to the end of the field. Nothing this engine writes
	// leaves a non-zero byte there, so one means the bytes were not ours.
	for (u_int32 u = uLength + 1; u < Zenith_UserSettings::uPROFILE_NAME_CAPACITY; u++)
	{
		if (puBytes[u] != 0)
		{
			return false;
		}
	}

	std::memcpy(acOut, puBytes, uLength);
	return true;
}

namespace
{
	struct UserSettingsReadContext
	{
		char m_acName[Zenith_UserSettings::uPROFILE_NAME_CAPACITY] = {};
		bool m_bParsed = false;
	};

	void WriteUserSettingsPayload(Zenith_DataStream& xStream, void* pxUserData)
	{
		xStream.WriteData(pxUserData, Zenith_UserSettings::uPAYLOAD_SIZE);
	}

	void ReadUserSettingsPayload(Zenith_DataStream& xStream, uint32_t uVersionInFile, void* pxUserData)
	{
		UserSettingsReadContext* pxContext = static_cast<UserSettingsReadContext*>(pxUserData);
		pxContext->m_bParsed = false;

		// The SaveData header's GAME version IS the payload version. A file this
		// build does not understand reads as defaults rather than as garbage.
		if (uVersionInFile != Zenith_UserSettings::uVERSION)
		{
			Zenith_Warning(LOG_CATEGORY_CORE,
				"UserSettings: slot '%s' is version %u, this build knows version %u -- using defaults (AUTO)",
				Zenith_UserSettings::kszSLOT_NAME, uVersionInFile, Zenith_UserSettings::uVERSION);
			return;
		}

		const u_int64 ulAvailable = xStream.GetCapacity() - xStream.GetCursor();
		if (ulAvailable < Zenith_UserSettings::uPAYLOAD_SIZE)
		{
			Zenith_Warning(LOG_CATEGORY_CORE,
				"UserSettings: slot '%s' holds %llu payload bytes, a v1 blob is %u -- using defaults (AUTO)",
				Zenith_UserSettings::kszSLOT_NAME, ulAvailable, Zenith_UserSettings::uPAYLOAD_SIZE);
			return;
		}

		u_int8 auBytes[Zenith_UserSettings::uPAYLOAD_SIZE] = {};
		xStream.ReadData(auBytes, Zenith_UserSettings::uPAYLOAD_SIZE);
		pxContext->m_bParsed = Zenith_UserSettings_ParseV1Payload(auBytes,
			Zenith_UserSettings::uPAYLOAD_SIZE, pxContext->m_acName);
	}
}

void Zenith_UserSettings::Load()
{
	ResetToDefaults();

	UserSettingsReadContext xContext;
	const Zenith_Status xStatus = Zenith_SaveData::LoadEx(kszSLOT_NAME, &ReadUserSettingsPayload, &xContext);
	if (!xStatus.IsOk())
	{
		// A first run has no settings file, and that is the overwhelmingly
		// common case — not an error. LoadEx already logged the specifics.
		return;
	}
	if (!xContext.m_bParsed)
	{
		Zenith_Warning(LOG_CATEGORY_CORE,
			"UserSettings: slot '%s' did not parse as a v1 blob -- using defaults (AUTO)", kszSLOT_NAME);
		return;
	}

	// The parser zero-fills its whole field, so a straight copy carries the
	// terminator and the padding with it.
	std::memcpy(m_acProfileOverrideName, xContext.m_acName, uPROFILE_NAME_CAPACITY);
	if (HasProfileOverride())
	{
		Zenith_Log(LOG_CATEGORY_CORE, "UserSettings: persisted input profile override '%s'", m_acProfileOverrideName);
	}
}

bool Zenith_UserSettings::ApplyProfileOverride(Zenith_InputActions& xActions)
{
	if (!HasProfileOverride())
	{
		return false;
	}

	// Resolution is BY NAME against whatever the game registered at step 3.
	const u_int8 uProfileId = xActions.FindProfileByName(m_acProfileOverrideName);
	if (uProfileId == Zenith_InputActions::uPROFILE_AUTO)
	{
		// The saved profile no longer exists (renamed, removed, or the file came
		// from another game). AUTO is always a working answer, so say so and
		// carry on — and deliberately do NOT rewrite the file: the player's
		// choice survives a build in which the profile is temporarily absent.
		Zenith_Warning(LOG_CATEGORY_CORE,
			"UserSettings: persisted input profile '%s' is not registered by this game -- staying on AUTO",
			m_acProfileOverrideName);
		return false;
	}

	xActions.SetProfileOverride(uProfileId);
	Zenith_Log(LOG_CATEGORY_CORE, "UserSettings: forced input profile '%s' (id %u) from the persisted setting",
		m_acProfileOverrideName, uProfileId);
	return true;
}

bool Zenith_UserSettings::SetProfileOverrideName(const char* szName)
{
	const char* szStore = (szName != nullptr) ? szName : "";
	const u_int32 uLength = static_cast<u_int32>(std::strlen(szStore));
	if (uLength >= uPROFILE_NAME_CAPACITY)
	{
		Zenith_Warning(LOG_CATEGORY_CORE,
			"UserSettings: refusing to persist profile name '%s' -- it needs %u bytes and the v1 field holds %u "
			"(a truncated name would resolve to a DIFFERENT profile, or to none)",
			szStore, uLength + 1u, uPROFILE_NAME_CAPACITY);
		return false;
	}

	ResetToDefaults();
	std::memcpy(m_acProfileOverrideName, szStore, uLength);
	return Save();
}

void Zenith_UserSettings::ResetToDefaults()
{
	std::memset(m_acProfileOverrideName, 0, uPROFILE_NAME_CAPACITY);
}

bool Zenith_UserSettings::Save() const
{
	u_int8 auPayload[uPAYLOAD_SIZE] = {};
	Zenith_UserSettings_BuildV1Payload(m_acProfileOverrideName, auPayload);
	return Zenith_SaveData::Save(kszSLOT_NAME, uVERSION, &WriteUserSettingsPayload, auPayload);
}

#include "Core/Zenith_UserSettings.Tests.inl"
