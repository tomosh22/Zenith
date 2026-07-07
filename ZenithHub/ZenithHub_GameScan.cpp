#include "Zenith.h"
#include "ZenithHub_GameScan.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <sys/stat.h>

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

		// Built win64 configs: Build/output/win64/<config>/<lower>.exe.
		std::string strLower = ToLower(xGame.strName);
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

	// Reserved engine prefixes.
	if (strUpper.rfind("ZENITH", 0) == 0) { return false; }
	if (strUpper.rfind("SENTINEL", 0) == 0) { return false; }

	// Reserved project names.
	static const char* aszReserved[] = {
		"FLUXCOMPILER", "FREETYPE", "MSDFGEN", "MSDFATLASGEN",
		"TILEPUZZLELEVELGEN", "TILEPUZZLEREGISTRYVIEWER", "ZENITHHUB"
	};
	for (const char* szR : aszReserved) { if (strUpper == szR) { return false; } }

	return true;
}
