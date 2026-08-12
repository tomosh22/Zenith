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

namespace
{
	// Caps how much child-process output a HubJob retains -- keeps the tail
	// (most relevant for a failure), drops the head once exceeded.
	constexpr size_t kMaxCapturedOutput = 64 * 1024;

	// Non-blocking drain of whatever the child has written so far. Safe to call
	// every frame, including after the process has already exited (drains any
	// remaining buffered bytes before the pipe is closed).
	void DrainPipe(HANDLE hRead, std::string& strOutput)
	{
		if (hRead == nullptr) { return; }
		for (;;)
		{
			DWORD dwAvailable = 0;
			if (!PeekNamedPipe(hRead, nullptr, 0, nullptr, &dwAvailable, nullptr) || dwAvailable == 0) { break; }
			char szBuf[4096];
			DWORD dwToRead = (dwAvailable < sizeof(szBuf)) ? dwAvailable : sizeof(szBuf);
			DWORD dwRead = 0;
			if (!ReadFile(hRead, szBuf, dwToRead, &dwRead, nullptr) || dwRead == 0) { break; }
			strOutput.append(szBuf, dwRead);
		}
		if (strOutput.size() > kMaxCapturedOutput)
		{
			strOutput.erase(0, strOutput.size() - kMaxCapturedOutput);
		}
	}
}

static bool LaunchInternal(const std::string& strRepoRoot, const std::wstring& strCliArgs, HANDLE* pProcessOut, HANDLE* pStdOutReadOut)
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

	HANDLE hChildStdOutRead = nullptr;
	HANDLE hChildStdOutWrite = nullptr;
	DWORD dwCreationFlags = CREATE_NO_WINDOW;
	BOOL bInheritHandles = FALSE;

	if (pStdOutReadOut != nullptr)
	{
		// Redirect the child's stdout+stderr into a pipe so a failing job (e.g.
		// regen hitting a descriptor validation error or a Sharpmake failure) is
		// visible in the UI, not just its exit code.
		SECURITY_ATTRIBUTES xSa;
		ZeroMemory(&xSa, sizeof(xSa));
		xSa.nLength = sizeof(xSa);
		xSa.bInheritHandle = TRUE;
		if (CreatePipe(&hChildStdOutRead, &hChildStdOutWrite, &xSa, 0))
		{
			// The parent's read end must NOT be inherited by the child, or the
			// pipe never sees EOF after the child exits (it still holds a write
			// handle open).
			SetHandleInformation(hChildStdOutRead, HANDLE_FLAG_INHERIT, 0);
			xSi.dwFlags |= STARTF_USESTDHANDLES;
			xSi.hStdOutput = hChildStdOutWrite;
			xSi.hStdError = hChildStdOutWrite;
			bInheritHandles = TRUE;
		}
	}

	BOOL bOk = CreateProcessW(
		nullptr, &strMutable[0], nullptr, nullptr, bInheritHandles,
		dwCreationFlags, nullptr,
		strDir.empty() ? nullptr : strDir.c_str(),
		&xSi, &xPi);

	// The write end is only needed by the child; the parent must close its copy
	// so DrainPipe sees EOF once the child exits.
	if (hChildStdOutWrite != nullptr) { CloseHandle(hChildStdOutWrite); }

	if (!bOk)
	{
		if (hChildStdOutRead != nullptr) { CloseHandle(hChildStdOutRead); }
		return false;
	}

	CloseHandle(xPi.hThread);
	if (pProcessOut != nullptr)
	{
		*pProcessOut = xPi.hProcess;
	}
	else
	{
		CloseHandle(xPi.hProcess);
	}
	if (pStdOutReadOut != nullptr) { *pStdOutReadOut = hChildStdOutRead; }
	return true;
}

bool ZenithHub_Process::StartJob(HubJob& xJob, const std::string& strRepoRoot, const std::wstring& strCliArgs, const std::string& strLabel)
{
	HANDLE hProcess = nullptr;
	HANDLE hStdOutRead = nullptr;
	if (!LaunchInternal(strRepoRoot, strCliArgs, &hProcess, &hStdOutRead)) { return false; }
	xJob.pProcessHandle = hProcess;
	xJob.pStdOutReadHandle = hStdOutRead;
	xJob.bRunning = true;
	xJob.iExitCode = 0;
	xJob.strLabel = strLabel;
	xJob.strOutput.clear();
	return true;
}

void ZenithHub_Process::PollJob(HubJob& xJob)
{
	if (!xJob.bRunning || xJob.pProcessHandle == nullptr) { return; }

	DrainPipe(static_cast<HANDLE>(xJob.pStdOutReadHandle), xJob.strOutput);

	HANDLE hProcess = static_cast<HANDLE>(xJob.pProcessHandle);
	DWORD dwExit = 0;
	if (GetExitCodeProcess(hProcess, &dwExit) && dwExit != STILL_ACTIVE)
	{
		// Final drain: catch whatever the child flushed right before exiting.
		DrainPipe(static_cast<HANDLE>(xJob.pStdOutReadHandle), xJob.strOutput);

		xJob.iExitCode = static_cast<int>(dwExit);
		xJob.bRunning = false;
		CloseHandle(hProcess);
		xJob.pProcessHandle = nullptr;

		if (xJob.pStdOutReadHandle != nullptr)
		{
			CloseHandle(static_cast<HANDLE>(xJob.pStdOutReadHandle));
			xJob.pStdOutReadHandle = nullptr;
		}
	}
}

bool ZenithHub_Process::LaunchDetached(const std::string& strRepoRoot, const std::wstring& strCliArgs)
{
	return LaunchInternal(strRepoRoot, strCliArgs, nullptr, nullptr);
}
