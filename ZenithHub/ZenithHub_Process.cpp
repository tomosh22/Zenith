#include "Zenith.h"
#include "ZenithHub_Process.h"

#include <windows.h>
#include <string>

namespace
{
	std::wstring Widen(const std::string& str)
	{
		if (str.empty()) { return std::wstring(); }
		int iLen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
		std::wstring strW(iLen, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &strW[0], iLen);
		return strW;
	}
}

std::wstring ZenithHub_Process::CliPrefix(const std::string& strRepoRoot)
{
	std::wstring strRootW = Widen(strRepoRoot);
	// Trailing slash tolerated (ZENITH_ROOT is baked as ".../").
	std::wstring strScript = strRootW;
	if (!strScript.empty() && strScript.back() != L'/' && strScript.back() != L'\\') { strScript += L"/"; }
	strScript += L"Tools/zenith.ps1";
	return L"powershell -NoProfile -ExecutionPolicy Bypass -File \"" + strScript + L"\"";
}

static bool LaunchInternal(const std::string& strRepoRoot, const std::wstring& strCliArgs, HANDLE* pProcessOut)
{
	std::wstring strCmd = ZenithHub_Process::CliPrefix(strRepoRoot) + L" " + strCliArgs;
	std::wstring strDir = Widen(strRepoRoot);

	// CreateProcessW needs a MUTABLE command-line buffer.
	std::wstring strMutable = strCmd;

	STARTUPINFOW xSi;
	ZeroMemory(&xSi, sizeof(xSi));
	xSi.cb = sizeof(xSi);
	PROCESS_INFORMATION xPi;
	ZeroMemory(&xPi, sizeof(xPi));

	BOOL bOk = CreateProcessW(
		nullptr, &strMutable[0], nullptr, nullptr, FALSE,
		CREATE_NO_WINDOW, nullptr,
		strDir.empty() ? nullptr : strDir.c_str(),
		&xSi, &xPi);
	if (!bOk) { return false; }

	CloseHandle(xPi.hThread);
	if (pProcessOut != nullptr)
	{
		*pProcessOut = xPi.hProcess;
	}
	else
	{
		CloseHandle(xPi.hProcess);
	}
	return true;
}

bool ZenithHub_Process::StartJob(HubJob& xJob, const std::string& strRepoRoot, const std::wstring& strCliArgs, const std::string& strLabel)
{
	HANDLE hProcess = nullptr;
	if (!LaunchInternal(strRepoRoot, strCliArgs, &hProcess)) { return false; }
	xJob.pProcessHandle = hProcess;
	xJob.bRunning = true;
	xJob.iExitCode = 0;
	xJob.strLabel = strLabel;
	return true;
}

void ZenithHub_Process::PollJob(HubJob& xJob)
{
	if (!xJob.bRunning || xJob.pProcessHandle == nullptr) { return; }
	HANDLE hProcess = static_cast<HANDLE>(xJob.pProcessHandle);
	DWORD dwExit = 0;
	if (GetExitCodeProcess(hProcess, &dwExit) && dwExit != STILL_ACTIVE)
	{
		xJob.iExitCode = static_cast<int>(dwExit);
		xJob.bRunning = false;
		CloseHandle(hProcess);
		xJob.pProcessHandle = nullptr;
	}
}

bool ZenithHub_Process::LaunchDetached(const std::string& strRepoRoot, const std::wstring& strCliArgs)
{
	return LaunchInternal(strRepoRoot, strCliArgs, nullptr);
}
