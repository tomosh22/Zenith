#include "Zenith.h"

#include "Core/Zenith_PlatformStdio.h"

namespace Zenith_PlatformStdio
{
	FILE* OpenFile(const char* szPath, const char* szMode)
	{
		FILE* pxFile = nullptr;
		return ::fopen_s(&pxFile, szPath, szMode) == 0 ? pxFile : nullptr;
	}

	FILE* OpenTemporaryFile()
	{
		FILE* pxFile = nullptr;
		return ::tmpfile_s(&pxFile) == 0 ? pxFile : nullptr;
	}
}
