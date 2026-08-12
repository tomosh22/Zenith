#pragma once
#include <string>
#include <vector>

// A game as the hub sees it: name + android flag from the descriptor, plus which
// win64 configs are built and when.
struct HubGame
{
	std::string strName;
	bool        bAndroid = false;
	std::string strBuiltConfigs;   // comma-joined config dir names ("" if none built)
	std::string strNewestBuild;    // "YYYY-MM-DD HH:MM" of the newest built exe ("" if none)
	bool        bRunConfigBuilt = false; // true iff kRunConfigDirName's exe exists

	// Regen-readiness: does this game's per-game solution exist, and does the
	// descriptor's SHA-256 match Build/Sharpmake_GameInstances.generated.cs?
	// Built-exe presence alone (above) says nothing about whether the .sln/.vcxproj
	// backing it is current -- these two flags are what make
	// "Regen left every config intact and ready to use" a checkable Hub-side
	// postcondition instead of an assumption. bAgde* is only meaningful when
	// bAndroid is true; a single *_agde.sln covers BOTH the arm64_v8a (physical
	// device) and x86_64 (emulator) ABIs, so one flag pair covers both.
	bool        bWin64SlnReady = false;
	bool        bWin64SlnStale = false;
	bool        bAgdeSlnReady = false;  // true (N/A) when !bAndroid
	bool        bAgdeSlnStale = false;
};

namespace ZenithHub_GameScan
{
	// The single config the hub's Run button always launches (Release + tools,
	// so a game that has never had its scene-baking *_True build still runs).
	// kRunConfigName is the /p:Configuration= form passed to `zenith run --config`;
	// kRunConfigDirName is Sharpmake's lowercased output-dir form, used to check
	// whether that exact config's exe exists.
	inline constexpr const char* kRunConfigName    = "Vulkan_vs2022_Release_Win64_True";
	inline constexpr const char* kRunConfigDirName = "vulkan_vs2022_release_win64_true";

	// Minimal fixed-buffer .zproj reader: pulls "name" + "android" out WITHOUT a
	// full JSON parser (the C++ side never parses JSON structurally -- the SHA
	// manifest guard is the correctness gate). Returns false on read error / no name.
	bool ReadDescriptor(const std::string& strZprojPath, std::string& strNameOut, bool& bAndroidOut);

	// Scan <repoRoot>/Games for one descriptor per folder + its built win64 configs.
	// Sorted by name.
	void ScanGames(const std::string& strRepoRoot, std::vector<HubGame>& axOut);

	// Game-name syntax validator: the C++ mirror of the PowerShell
	// Test-ZenithGameNameSyntax, pinned by Tools/ZenithCli/Tests/name_validation_cases.txt.
	// Returns true iff the name is valid (syntax + reserved names -- NOT filesystem
	// collision, which is state-dependent).
	bool ValidateName(const std::string& strName);
}
