#pragma once

namespace Zenith_StringUtil
{
	static void ReplaceAllChars(char* szString, const char cFind, const char cReplacement)
	{
		while (true)
		{
			char cChar = *szString;
			if(cChar == '\0') return;
			if(cChar == cFind) *szString = cReplacement;
			szString++;
		}
	}
}