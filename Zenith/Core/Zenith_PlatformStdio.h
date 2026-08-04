#pragma once

#include <cstdio>

namespace Zenith_PlatformStdio
{
	// Platform implementations live in Zenith/Windows and Zenith/Android so
	// shared engine and game code never selects a C runtime with compiler guards.
	FILE* OpenFile(const char* szPath, const char* szMode);
	FILE* OpenTemporaryFile();
}
