#pragma once

#ifdef ZENITH_TOOLS

#include "Collections/Zenith_Vector.h"
#include <string>

//=============================================================================
// Zenith_EditorPrefs — per-user, per-game editor preferences.
//
// The small set of things a user expects the editor to remember between runs:
// recent scenes, the fly-camera speed, snapping and gizmo space, overlay
// toggles. Stored as `key=value` lines in
//   %LOCALAPPDATA%/Zenith/<GameName>/editor_prefs.txt
// next to imgui.ini (Zenith_EditorPrefs::GetUserDataDirectory is the one
// resolver for that directory). Never read in automated / headless runs, so
// tests always start from the defaults below.
//
// Parse / Serialize are pure text transforms with no I/O so they can be unit
// tested on a string.
//=============================================================================
// Look-sensitivity bounds, shared by the Edit > Camera slider and Parse's clamp.
constexpr float fMIN_LOOK_SENSITIVITY = 0.002f;
constexpr float fMAX_LOOK_SENSITIVITY = 0.500f;

struct Zenith_EditorPrefs
{
	static constexpr u_int uMAX_RECENT_SCENES = 8;

	Zenith_Vector<std::string> m_axRecentScenes;   // most recent FIRST
	float m_fCameraMoveSpeed = 50.0f;
	// Degrees of yaw/pitch per PIXEL of pointer movement. The editor does not
	// capture the cursor to look, so this is the OS-processed pointer delta,
	// bounded by the desktop and shaped by the pointer curve. 0.1 is the value
	// the editor shipped with; it is a preference only so it can be tuned
	// (Edit > Camera), not because the default was wrong.
	float m_fLookSensitivity = 0.1f;
	bool  m_bSnapEnabled = false;
	float m_fSnapMove = 0.5f;
	float m_fSnapRotateDegrees = 15.0f;
	float m_fSnapScale = 0.1f;
	bool  m_bGizmoLocalSpace = false;
	bool  m_bShowViewportStats = true;
	bool  m_bShowViewportAxes = true;
	bool  m_bShowSelectionBounds = true;
	bool  m_bClearConsoleOnPlay = false;

	// Applies one parsed key=value pair; an unknown key is ignored.
	void ApplyKeyValue(const std::string& strKey, const std::string& strValue);
	// Replaces any value a hand-edited file left unusable with its default.
	void ClampToUsableRanges();

	// Moves strPath to the front (deduplicated, capped at uMAX_RECENT_SCENES).
	void AddRecentScene(const std::string& strPath);
	void RemoveRecentScene(const std::string& strPath);

	// Text form. Parse resets to defaults first, then applies every recognised
	// line; unknown keys and malformed lines are ignored.
	std::string Serialize() const;
	void Parse(const std::string& strText);

	// I/O. Load returns false (leaving defaults) when the file is absent or the
	// run is automated. Save is a no-op in the same cases.
	bool Load();
	void Save() const;

	// %LOCALAPPDATA%/Zenith/<GameName> (created on demand). Empty when the
	// location cannot be resolved (no LOCALAPPDATA, directory creation failed).
	static std::string GetUserDataDirectory();
	static std::string GetPrefsFilePath();
};

#endif // ZENITH_TOOLS
