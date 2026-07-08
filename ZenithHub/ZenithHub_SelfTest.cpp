#include "Zenith.h"
#include "ZenithHub_SelfTest.h"
#include "ZenithHub_GameScan.h"

#include <cstdio>
#include <string>
#include <vector>

namespace
{
	std::string Trim(const std::string& str)
	{
		size_t a = 0, b = str.size();
		while (a < b && (str[a] == ' ' || str[a] == '\t' || str[a] == '\r' || str[a] == '\n')) { a++; }
		while (b > a && (str[b - 1] == ' ' || str[b - 1] == '\t' || str[b - 1] == '\r' || str[b - 1] == '\n')) { b--; }
		return str.substr(a, b - a);
	}

	std::string NormalizeRoot(const char* szRoot)
	{
		std::string str = szRoot ? szRoot : "";
		while (!str.empty() && (str.back() == '/' || str.back() == '\\')) { str.pop_back(); }
		return str;
	}

	bool ReadWholeFile(const std::string& strPath, std::string& strOut)
	{
		FILE* pFile = nullptr;
		if (fopen_s(&pFile, strPath.c_str(), "rb") != 0 || pFile == nullptr) { return false; }
		fseek(pFile, 0, SEEK_END);
		long lSize = ftell(pFile);
		fseek(pFile, 0, SEEK_SET);
		if (lSize <= 0) { fclose(pFile); strOut.clear(); return true; }
		strOut.resize((size_t)lSize);
		size_t uRead = fread(&strOut[0], 1, (size_t)lSize, pFile);
		fclose(pFile);
		strOut.resize(uRead);
		return true;
	}
}

int ZenithHub_SelfTest::Run(const char* szRepoRoot)
{
	int iPass = 0;
	int iFail = 0;
	auto Check = [&](bool bCond, const char* szMsg)
	{
		if (bCond) { iPass++; printf("  PASS  %s\n", szMsg); }
		else { iFail++; printf("  FAIL  %s\n", szMsg); }
	};

	const std::string strRoot = NormalizeRoot(szRepoRoot);
	printf("ZenithHub --selftest (root: %s)\n", strRoot.c_str());

	// [1] Shared name-validation vectors.
	printf("\n[1] Name-validation vectors (shared file)\n");
	{
		std::string strFile = strRoot + "/Tools/ZenithCli/Tests/name_validation_cases.txt";
		std::string strAll;
		if (!ReadWholeFile(strFile, strAll))
		{
			Check(false, "read name_validation_cases.txt");
		}
		else
		{
			int iChecked = 0;
			int iMismatch = 0;
			size_t uStart = 0;
			while (uStart <= strAll.size())
			{
				size_t uNl = strAll.find('\n', uStart);
				std::string strLine = (uNl == std::string::npos) ? strAll.substr(uStart) : strAll.substr(uStart, uNl - uStart);
				uStart = (uNl == std::string::npos) ? strAll.size() + 1 : uNl + 1;

				// Strip trailing CR only (keep any spaces IN the name).
				while (!strLine.empty() && (strLine.back() == '\r' || strLine.back() == '\n')) { strLine.pop_back(); }
				std::string strLeading = Trim(strLine);
				if (strLeading.empty() || strLeading[0] == '#') { continue; }

				size_t uBar = strLine.find('|');
				if (uBar == std::string::npos) { continue; }
				std::string strExpected = Trim(strLine.substr(0, uBar));
				std::string strName = strLine.substr(uBar + 1);   // NOT trimmed -- names may contain spaces

				bool bWantValid = (strExpected == "valid");
				bool bGotValid = ZenithHub_GameScan::ValidateName(strName);
				iChecked++;
				if (bGotValid != bWantValid)
				{
					iMismatch++;
					printf("      mismatch: name='%s' expected=%s gotValid=%d\n", strName.c_str(), strExpected.c_str(), (int)bGotValid);
				}
			}
			Check(iChecked >= 20, "read >= 20 vectors from the shared file");
			Check(iMismatch == 0, "ValidateName agrees with every shared vector");
		}
	}

	// [2] Descriptor parsing (fixtures).
	printf("\n[2] Descriptor parsing (fixtures)\n");
	{
		std::string strName;
		bool bAndroid = false;
		std::string strAlpha = strRoot + "/Build/Tests/Fixtures/GoodGames/Alpha/Alpha.zproj";
		bool bOkA = ZenithHub_GameScan::ReadDescriptor(strAlpha, strName, bAndroid);
		Check(bOkA && strName == "Alpha" && !bAndroid, "Alpha.zproj -> name=Alpha, android=false");

		std::string strBeta = strRoot + "/Build/Tests/Fixtures/GoodGames/Beta/Beta.zproj";
		bool bOkB = ZenithHub_GameScan::ReadDescriptor(strBeta, strName, bAndroid);
		Check(bOkB && strName == "Beta" && bAndroid, "Beta.zproj -> name=Beta, android=true");
	}

	// [3] Scan the real Games tree.
	printf("\n[3] Games scan (real tree)\n");
	{
		std::vector<HubGame> axGames;
		ZenithHub_GameScan::ScanGames(strRoot, axGames);
		Check(axGames.size() >= 5, "ScanGames found >= 5 games");

		bool bCombat = false, bCombatAndroid = false;
		bool bCityBuilder = false, bCityBuilderAndroid = true;
		for (const HubGame& xGame : axGames)
		{
			if (xGame.strName == "Combat") { bCombat = true; bCombatAndroid = xGame.bAndroid; }
			if (xGame.strName == "CityBuilder") { bCityBuilder = true; bCityBuilderAndroid = xGame.bAndroid; }
		}
		Check(bCombat && bCombatAndroid, "Combat present, android:true");
		Check(bCityBuilder && !bCityBuilderAndroid, "CityBuilder present, android:false");
	}

	printf("\nZenithHub selftest: %d passed, %d failed\n", iPass, iFail);
	return iFail;
}
