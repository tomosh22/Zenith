#include "Zenith.h"
#include "ZenithHub_GameScan.h"

#include <algorithm>
#include <array>
#include <bcrypt.h>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <limits>

namespace
{
	std::string ToUpper(const std::string& str)
	{
		std::string strOut = str;
		for (char& c : strOut) { c = static_cast<char>(std::toupper(static_cast<unsigned char>(c))); }
		return strOut;
	}

	std::string ToLower(const std::string& str)
	{
		std::string strOut = str;
		for (char& c : strOut) { c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
		return strOut;
	}

	bool ReadWholeFile(const std::filesystem::path& xPath, std::string& strOut)
	{
		strOut.clear();
		FILE* pFile = nullptr;
		if (_wfopen_s(&pFile, xPath.c_str(), L"rb") != 0 || pFile == nullptr)
		{
			return false;
		}

		if (_fseeki64(pFile, 0, SEEK_END) != 0)
		{
			fclose(pFile);
			return false;
		}
		const __int64 iSize = _ftelli64(pFile);
		if (iSize < 0 || static_cast<uint64_t>(iSize) > static_cast<uint64_t>((std::numeric_limits<size_t>::max)()))
		{
			fclose(pFile);
			return false;
		}
		if (_fseeki64(pFile, 0, SEEK_SET) != 0)
		{
			fclose(pFile);
			return false;
		}

		strOut.resize(static_cast<size_t>(iSize));
		const size_t uRead = strOut.empty() ? 0 : fread(strOut.data(), 1, strOut.size(), pFile);
		fclose(pFile);
		return uRead == strOut.size();
	}

	bool ComputeSHA256(const std::string& strBytes, std::string& strHexOut)
	{
		strHexOut.clear();
		if (strBytes.size() > static_cast<size_t>((std::numeric_limits<ULONG>::max)()))
		{
			return false;
		}

		BCRYPT_ALG_HANDLE hAlgorithm = nullptr;
		if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(
			&hAlgorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0)))
		{
			return false;
		}

		std::array<UCHAR, 32> auDigest{};
		const NTSTATUS iStatus = BCryptHash(
			hAlgorithm,
			nullptr,
			0,
			reinterpret_cast<PUCHAR>(const_cast<char*>(strBytes.data())),
			static_cast<ULONG>(strBytes.size()),
			auDigest.data(),
			static_cast<ULONG>(auDigest.size()));
		BCryptCloseAlgorithmProvider(hAlgorithm, 0);
		if (!BCRYPT_SUCCESS(iStatus))
		{
			return false;
		}

		static constexpr char szHEX[] = "0123456789ABCDEF";
		strHexOut.resize(auDigest.size() * 2);
		for (size_t u = 0; u < auDigest.size(); ++u)
		{
			strHexOut[u * 2] = szHEX[auDigest[u] >> 4];
			strHexOut[u * 2 + 1] = szHEX[auDigest[u] & 0x0F];
		}
		return true;
	}

	bool DescriptorMatchesManifest(
		const std::string& strManifest,
		const std::filesystem::path& xRepoRoot,
		const std::filesystem::path& xDescriptor)
	{
		std::string strDescriptorBytes;
		std::string strDigest;
		if (!ReadWholeFile(xDescriptor, strDescriptorBytes) ||
			!ComputeSHA256(strDescriptorBytes, strDigest))
		{
			return false;
		}

		const std::string strRelative = xDescriptor.lexically_relative(xRepoRoot).generic_string();
		const std::string strEntry = "new Entry(\"" + strRelative + "\", \"" + strDigest + "\")";
		return strManifest.find(strEntry) != std::string::npos;
	}
}

bool ZenithHub_GameScan::ReadDescriptor(const std::string& strZprojPath, std::string& strNameOut, bool& bAndroidOut)
{
	strNameOut.clear();
	bAndroidOut = false;

	FILE* pFile = nullptr;
	if (fopen_s(&pFile, strZprojPath.c_str(), "rb") != 0 || pFile == nullptr)
	{
		return false;
	}
	char szBuf[8192];
	size_t uRead = fread(szBuf, 1, sizeof(szBuf) - 1, pFile);
	fclose(pFile);
	szBuf[uRead] = '\0';
	std::string strContent(szBuf, uRead);

	// "name": "<value>"
	size_t uNamePos = strContent.find("\"name\"");
	if (uNamePos == std::string::npos) { return false; }
	size_t uColon = strContent.find(':', uNamePos);
	if (uColon == std::string::npos) { return false; }
	size_t uQ1 = strContent.find('"', uColon);
	if (uQ1 == std::string::npos) { return false; }
	size_t uQ2 = strContent.find('"', uQ1 + 1);
	if (uQ2 == std::string::npos) { return false; }
	strNameOut = strContent.substr(uQ1 + 1, uQ2 - uQ1 - 1);

	// "android": true|false  -- first non-space token after the colon.
	size_t uAndPos = strContent.find("\"android\"");
	if (uAndPos != std::string::npos)
	{
		size_t uColon2 = strContent.find(':', uAndPos);
		if (uColon2 != std::string::npos)
		{
			size_t p = uColon2 + 1;
			while (p < strContent.size() && (strContent[p] == ' ' || strContent[p] == '\t' || strContent[p] == '\r' || strContent[p] == '\n')) { p++; }
			if (p < strContent.size() && (strContent[p] == 't' || strContent[p] == 'T')) { bAndroidOut = true; }
		}
	}

	return !strNameOut.empty();
}

void ZenithHub_GameScan::ScanGames(const std::string& strRepoRoot, std::vector<HubGame>& axOut)
{
	axOut.clear();
	namespace fs = std::filesystem;

	std::error_code xEc;
	fs::path xGamesDir = fs::path(strRepoRoot) / "Games";
	if (!fs::exists(xGamesDir, xEc)) { return; }

	// Regen emits a SHA-256 entry for every descriptor. Compare that content,
	// rather than mtimes: Sharpmake deliberately preserves unchanged .sln files,
	// while codegen rewrites this manifest on every run.
	const fs::path xRepoRoot(strRepoRoot);
	std::string strGeneratedManifest;
	ReadWholeFile(xRepoRoot / "Build" / "Sharpmake_GameInstances.generated.cs", strGeneratedManifest);

	std::vector<HubGame> axGames;
	for (const auto& xDir : fs::directory_iterator(xGamesDir, xEc))
	{
		if (!xDir.is_directory()) { continue; }

		// Find the single .zproj in this folder.
		std::string strZproj;
		for (const auto& xF : fs::directory_iterator(xDir.path(), xEc))
		{
			if (xF.is_regular_file() && xF.path().extension() == ".zproj")
			{
				strZproj = xF.path().string();
				break;
			}
		}
		if (strZproj.empty()) { continue; }

		HubGame xGame;
		if (!ReadDescriptor(strZproj, xGame.strName, xGame.bAndroid)) { continue; }
		const bool bDescriptorCurrent = DescriptorMatchesManifest(
			strGeneratedManifest, xRepoRoot, fs::path(strZproj));

		// Built win64 configs: Build/output/win64/<config>/<lower>.exe.
		std::string strLower = ToLower(xGame.strName);

		// Regen readiness -- see the field comments in ZenithHub_GameScan.h.
		{
			fs::path xWin64Sln = xDir.path() / (strLower + "_win64.sln");
			if (fs::exists(xWin64Sln, xEc))
			{
				xGame.bWin64SlnReady = true;
				xGame.bWin64SlnStale = !bDescriptorCurrent;
			}

			if (xGame.bAndroid)
			{
				fs::path xAgdeSln = xDir.path() / (strLower + "_agde.sln");
				if (fs::exists(xAgdeSln, xEc))
				{
					xGame.bAgdeSlnReady = true;
					xGame.bAgdeSlnStale = !bDescriptorCurrent;
				}
			}
			else
			{
				xGame.bAgdeSlnReady = true; // N/A -- game has no Android target
			}
		}

		fs::path xOutRoot = xDir.path() / "Build" / "output" / "win64";
		std::time_t tNewest = 0;
		if (fs::exists(xOutRoot, xEc))
		{
			for (const auto& xCfg : fs::directory_iterator(xOutRoot, xEc))
			{
				if (!xCfg.is_directory()) { continue; }
				fs::path xExe = xCfg.path() / (strLower + ".exe");
				struct _stat64 xStat;
				if (_stat64(xExe.string().c_str(), &xStat) == 0)
				{
					if (!xGame.strBuiltConfigs.empty()) { xGame.strBuiltConfigs += ", "; }
					const std::string strCfgDir = xCfg.path().filename().string();
					xGame.strBuiltConfigs += strCfgDir;
					if (strCfgDir == kRunConfigDirName) { xGame.bRunConfigBuilt = true; }
					if (xStat.st_mtime > tNewest) { tNewest = xStat.st_mtime; }
				}
			}
		}
		if (tNewest > 0)
		{
			struct tm xTm;
			localtime_s(&xTm, &tNewest);
			char szBuf[32];
			strftime(szBuf, sizeof(szBuf), "%Y-%m-%d %H:%M", &xTm);
			xGame.strNewestBuild = szBuf;
		}

		axGames.push_back(xGame);
	}

	std::sort(axGames.begin(), axGames.end(),
		[](const HubGame& a, const HubGame& b) { return a.strName < b.strName; });
	axOut = axGames;
}

bool ZenithHub_GameScan::ValidateName(const std::string& strName)
{
	// Regex ^[A-Z][A-Za-z0-9]{0,63}$
	if (strName.empty() || strName.size() > 64) { return false; }
	char c0 = strName[0];
	if (!(c0 >= 'A' && c0 <= 'Z')) { return false; }
	for (size_t i = 1; i < strName.size(); ++i)
	{
		char c = strName[i];
		bool bAlnum = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
		if (!bAlnum) { return false; }
	}

	std::string strUpper = ToUpper(strName);

	// Reserved Windows device names.
	static const char* aszDevices[] = {
		"CON", "PRN", "AUX", "NUL",
		"COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
		"LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
	};
	for (const char* szD : aszDevices) { if (strUpper == szD) { return false; } }

	// Reserved engine namespace: "Zenith"/"Sentinel" alone or followed by a new
	// PascalCase word (uppercase/digit) -- the shape of every engine project
	// (ZenithECS, ZenithHub, SentinelAI). A lowercase continuation is a
	// different word (e.g. "Zenithmon") and is allowed.
	static const struct { const char* szUpper; size_t uLen; } s_axEnginePrefixes[] = {
		{ "ZENITH", 6 }, { "SENTINEL", 8 }
	};
	for (const auto& xPrefix : s_axEnginePrefixes)
	{
		if (strUpper.compare(0, xPrefix.uLen, xPrefix.szUpper) != 0) { continue; }
		if (strName.size() == xPrefix.uLen) { return false; }
		const char cNext = strName[xPrefix.uLen];
		if ((cNext >= 'A' && cNext <= 'Z') || (cNext >= '0' && cNext <= '9')) { return false; }
	}

	// Reserved project names.
	static const char* aszReserved[] = {
		"FLUXCOMPILER", "FREETYPE", "MSDFGEN", "MSDFATLASGEN",
		"TILEPUZZLELEVELGEN", "TILEPUZZLEREGISTRYVIEWER", "ZENITHHUB"
	};
	for (const char* szR : aszReserved) { if (strUpper == szR) { return false; } }

	return true;
}
