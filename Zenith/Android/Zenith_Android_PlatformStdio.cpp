#include "Zenith.h"

#include "Core/Zenith_PlatformStdio.h"

#include <cstdlib>
#include <unistd.h>

namespace Zenith_PlatformStdio
{
	FILE* OpenFile(const char* szPath, const char* szMode)
	{
		return std::fopen(szPath, szMode);
	}

	FILE* OpenTemporaryFile()
	{
		if (FILE* pxFile = std::tmpfile())
		{
			return pxFile;
		}

		// Android's tmpfile() can target a process-wide temp directory that an
		// app sandbox cannot write. android_main sets the working directory to
		// internalDataPath, so fall back to an anonymous file created there.
		char acPath[] = "./zenith_tmp_XXXXXX";
		const int iFileDescriptor = ::mkstemp(acPath);
		if (iFileDescriptor < 0)
		{
			return nullptr;
		}

		// POSIX keeps an unlinked open file alive until fclose, matching tmpfile.
		// If anonymity cannot be established, fail rather than leave test residue.
		if (::unlink(acPath) != 0)
		{
			::close(iFileDescriptor);
			std::remove(acPath);
			return nullptr;
		}
		FILE* pxFile = ::fdopen(iFileDescriptor, "w+b");
		if (pxFile == nullptr)
		{
			::close(iFileDescriptor);
		}
		return pxFile;
	}
}
