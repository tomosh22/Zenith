#pragma once
#include <string>

// A single background CLI job the hub is waiting on (Regen / New Game). The hub
// runs ONE mutation job at a time (matching the mspdbsrv "sequential builds" rule).
struct HubJob
{
	void*       pProcessHandle = nullptr;   // HANDLE (opaque here to keep windows.h out of the header)
	bool        bRunning = false;
	int         iExitCode = 0;
	std::string strLabel;
};

namespace ZenithHub_Process
{
	// Build the base "powershell -NoProfile -ExecutionPolicy Bypass -File
	// <root>/Tools/zenith.ps1" command-line prefix.
	std::wstring CliPrefix(const std::string& strRepoRoot);

	// Start a polled CLI job: `zenith <strCliArgs>`. Returns false if the process
	// couldn't be launched. On success, job.bRunning == true; poll with PollJob.
	bool StartJob(HubJob& xJob, const std::string& strRepoRoot, const std::wstring& strCliArgs, const std::string& strLabel);

	// Update a running job's state (non-blocking). Sets bRunning=false + iExitCode
	// when the process exits and closes the handle.
	void PollJob(HubJob& xJob);

	// Fire-and-forget launch of `zenith <strCliArgs>` (Open / Run -- these spawn a
	// long-lived VS / game the hub does not wait on).
	bool LaunchDetached(const std::string& strRepoRoot, const std::wstring& strCliArgs);
}
