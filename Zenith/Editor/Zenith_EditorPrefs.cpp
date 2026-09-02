#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Editor/Zenith_EditorPrefs.h"
#include "Core/Zenith_CommandLine.h"
#include "Core/Zenith_ProjectHooks.h"
#include "FileAccess/Zenith_FileAccess.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>

#ifdef ZENITH_WINDOWS
#include "Core/Zenith_Win32.h"
#endif

namespace
{
	constexpr const char* szPREFS_FILENAME = "editor_prefs.txt";

	bool ParseBool(const std::string& strValue)
	{
		return strValue == "1" || strValue == "true";
	}

	float ParseFloat(const std::string& strValue, float fDefault)
	{
		char* pcEnd = nullptr;
		const float fParsed = static_cast<float>(strtod(strValue.c_str(), &pcEnd));
		return (pcEnd != nullptr && pcEnd != strValue.c_str()) ? fParsed : fDefault;
	}

	std::string Trim(const std::string& strIn)
	{
		const size_t uStart = strIn.find_first_not_of(" \t\r\n");
		if (uStart == std::string::npos)
		{
			return "";
		}
		const size_t uEnd = strIn.find_last_not_of(" \t\r\n");
		return strIn.substr(uStart, uEnd - uStart + 1);
	}

	void AppendLine(std::string& strOut, const char* szKey, const std::string& strValue)
	{
		strOut += szKey;
		strOut += '=';
		strOut += strValue;
		strOut += '\n';
	}

	std::string FloatToString(float fValue)
	{
		char acBuffer[32];
		snprintf(acBuffer, sizeof(acBuffer), "%g", fValue);
		return acBuffer;
	}

	//-------------------------------------------------------------------------
	// The key <-> member tables. Serialize and Parse BOTH walk these, so a new
	// preference is one row rather than two hand-written sites that can drift:
	// a key added to the writer and forgotten in the reader silently fails to
	// persist, which is the classic defect in a hand-rolled key=value file.
	// (`recent` is not here — it is a repeated key holding a list.)
	//-------------------------------------------------------------------------
	struct FloatField
	{
		const char* m_szKey;
		float Zenith_EditorPrefs::* m_pfMember;
	};

	struct BoolField
	{
		const char* m_szKey;
		bool Zenith_EditorPrefs::* m_pbMember;
	};

	constexpr FloatField axFLOAT_FIELDS[] = {
		{ "camera_speed",     &Zenith_EditorPrefs::m_fCameraMoveSpeed },
		// Key carries its UNIT: an editor_prefs.txt written while the look delta
		// was in raw device counts holds a "look_sensitivity" ~4.5x too small for
		// pixels. Renaming the key makes that stale value an unknown key, which
		// Parse ignores, so the file cannot quietly reinstate a retired unit.
		{ "look_sensitivity_px", &Zenith_EditorPrefs::m_fLookSensitivity },
		{ "snap_move",        &Zenith_EditorPrefs::m_fSnapMove },
		{ "snap_rotate",      &Zenith_EditorPrefs::m_fSnapRotateDegrees },
		{ "snap_scale",       &Zenith_EditorPrefs::m_fSnapScale },
	};

	constexpr BoolField axBOOL_FIELDS[] = {
		{ "snap_enabled",          &Zenith_EditorPrefs::m_bSnapEnabled },
		{ "gizmo_local",           &Zenith_EditorPrefs::m_bGizmoLocalSpace },
		{ "viewport_stats",        &Zenith_EditorPrefs::m_bShowViewportStats },
		{ "viewport_axes",         &Zenith_EditorPrefs::m_bShowViewportAxes },
		{ "selection_bounds",      &Zenith_EditorPrefs::m_bShowSelectionBounds },
		{ "clear_console_on_play", &Zenith_EditorPrefs::m_bClearConsoleOnPlay },
	};

	// Automated and headless runs must never depend on (or write) a user's
	// preferences: every test starts from the defaults.
	bool PrefsAreEnabled()
	{
		return !Zenith_IsNullRenderer() && !Zenith_CommandLine::IsAutomatedTestRun();
	}
}

void Zenith_EditorPrefs::AddRecentScene(const std::string& strPath)
{
	if (strPath.empty())
	{
		return;
	}
	RemoveRecentScene(strPath);
	// Most recent first: push then rotate to the front.
	m_axRecentScenes.PushBack(strPath);
	for (u_int u = m_axRecentScenes.GetSize() - 1; u > 0; --u)
	{
		std::string strTmp = m_axRecentScenes.Get(u);
		m_axRecentScenes.Get(u) = m_axRecentScenes.Get(u - 1);
		m_axRecentScenes.Get(u - 1) = strTmp;
	}
	while (m_axRecentScenes.GetSize() > uMAX_RECENT_SCENES)
	{
		m_axRecentScenes.PopBack();
	}
}

void Zenith_EditorPrefs::RemoveRecentScene(const std::string& strPath)
{
	// Copy first: the caller may pass a reference INTO the list (menu rows do),
	// and removing an element shifts what such a reference names.
	const std::string strTarget = strPath;
	for (u_int u = 0; u < m_axRecentScenes.GetSize(); )
	{
		if (m_axRecentScenes.Get(u) == strTarget)
		{
			m_axRecentScenes.Remove(u);
		}
		else
		{
			++u;
		}
	}
}

std::string Zenith_EditorPrefs::Serialize() const
{
	std::string strOut;
	strOut += "# Zenith editor preferences (generated; safe to delete)\n";
	for (u_int u = 0; u < m_axRecentScenes.GetSize(); ++u)
	{
		AppendLine(strOut, "recent", m_axRecentScenes.Get(u));
	}
	for (const FloatField& xField : axFLOAT_FIELDS)
	{
		AppendLine(strOut, xField.m_szKey, FloatToString(this->*xField.m_pfMember));
	}
	for (const BoolField& xField : axBOOL_FIELDS)
	{
		AppendLine(strOut, xField.m_szKey, (this->*xField.m_pbMember) ? "1" : "0");
	}
	return strOut;
}

void Zenith_EditorPrefs::Parse(const std::string& strText)
{
	*this = Zenith_EditorPrefs();

	// "recent" lines arrive most-recent-first; collect then insert in reverse so
	// AddRecentScene's front-insertion reproduces the original order.
	Zenith_Vector<std::string> axRecent;

	size_t uPos = 0;
	while (uPos < strText.size())
	{
		size_t uEnd = strText.find('\n', uPos);
		if (uEnd == std::string::npos) uEnd = strText.size();
		const std::string strLine = Trim(strText.substr(uPos, uEnd - uPos));
		uPos = uEnd + 1;

		if (strLine.empty() || strLine[0] == '#') continue;
		const size_t uEq = strLine.find('=');
		if (uEq == std::string::npos) continue;
		const std::string strKey = Trim(strLine.substr(0, uEq));
		const std::string strValue = Trim(strLine.substr(uEq + 1));

		if (strKey == "recent")
		{
			axRecent.PushBack(strValue);
			continue;
		}
		ApplyKeyValue(strKey, strValue);
	}

	for (u_int u = axRecent.GetSize(); u > 0; --u)
	{
		AddRecentScene(axRecent.Get(u - 1));
	}

	ClampToUsableRanges();
}

// One key=value pair, dispatched through the same tables Serialize writes.
// An unknown key is ignored, which is what lets a file written by a newer
// build load in an older one.
void Zenith_EditorPrefs::ApplyKeyValue(const std::string& strKey, const std::string& strValue)
{
	for (const FloatField& xField : axFLOAT_FIELDS)
	{
		if (strKey == xField.m_szKey)
		{
			this->*xField.m_pfMember = ParseFloat(strValue, this->*xField.m_pfMember);
			return;
		}
	}
	for (const BoolField& xField : axBOOL_FIELDS)
	{
		if (strKey == xField.m_szKey)
		{
			this->*xField.m_pbMember = ParseBool(strValue);
			return;
		}
	}
}

// Defensive clamps: a hand-edited file must not produce a frozen camera or a
// zero snap step (SnapValue treats <= 0 as "no snapping", but the UI would then
// show a nonsense value).
void Zenith_EditorPrefs::ClampToUsableRanges()
{
	if (!(m_fCameraMoveSpeed > 0.01f)) m_fCameraMoveSpeed = 50.0f;
	// Outside this range the camera is either frozen or unusable; the bounds
	// match the Edit > Camera slider so a hand-edited file cannot exceed it.
	if (!(m_fLookSensitivity >= fMIN_LOOK_SENSITIVITY && m_fLookSensitivity <= fMAX_LOOK_SENSITIVITY)) m_fLookSensitivity = 0.1f;
	if (!(m_fSnapMove > 0.0f)) m_fSnapMove = 0.5f;
	if (!(m_fSnapRotateDegrees > 0.0f)) m_fSnapRotateDegrees = 15.0f;
	if (!(m_fSnapScale > 0.0f)) m_fSnapScale = 0.1f;
}

std::string Zenith_EditorPrefs::GetUserDataDirectory()
{
#ifdef ZENITH_WINDOWS
	char acLocalAppData[ZENITH_MAX_PATH_LENGTH] = {};
	const DWORD uLen = GetEnvironmentVariableA("LOCALAPPDATA", acLocalAppData, sizeof(acLocalAppData));
	if (uLen == 0 || uLen >= sizeof(acLocalAppData))
	{
		return "";
	}
	std::string strDir = std::string(acLocalAppData) + "/Zenith/" + Project_GetName();
	std::error_code xError;
	std::filesystem::create_directories(strDir, xError);
	if (xError)
	{
		Zenith_Warning(LOG_CATEGORY_EDITOR, "Failed to create editor user-data dir '%s' (%s)", strDir.c_str(), xError.message().c_str());
		return "";
	}
	return strDir;
#else
	return "";
#endif
}

std::string Zenith_EditorPrefs::GetPrefsFilePath()
{
	const std::string strDir = GetUserDataDirectory();
	return strDir.empty() ? "" : strDir + "/" + szPREFS_FILENAME;
}

bool Zenith_EditorPrefs::Load()
{
	if (!PrefsAreEnabled())
	{
		return false;
	}
	const std::string strPath = GetPrefsFilePath();
	if (strPath.empty() || !Zenith_FileAccess::FileExists(strPath.c_str()))
	{
		return false;
	}
	uint64_t ulSize = 0;
	char* pcData = Zenith_FileAccess::ReadFile(strPath.c_str(), ulSize);
	if (pcData == nullptr)
	{
		return false;
	}
	Parse(std::string(pcData, static_cast<size_t>(ulSize)));
	Zenith_FileAccess::FreeFileData(pcData);
	return true;
}

void Zenith_EditorPrefs::Save() const
{
	if (!PrefsAreEnabled())
	{
		return;
	}
	const std::string strPath = GetPrefsFilePath();
	if (strPath.empty())
	{
		return;
	}
	const std::string strText = Serialize();
	Zenith_FileAccess::WriteFile(strPath.c_str(), strText.data(), strText.size());
}

#endif // ZENITH_TOOLS
