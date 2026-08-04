#include "Zenith.h"

#include "Core/Zenith_PlatformEnvironment.h"

#include <cstdlib>

namespace Zenith_PlatformEnvironment
{
	bool GetVariable(const char* szName, char* szValueOut, size_t uValueOutSize)
	{
		if (szName == nullptr || szValueOut == nullptr || uValueOutSize == 0)
		{
			return false;
		}

		size_t uRequiredSize = 0;
		const errno_t iError = ::getenv_s(&uRequiredSize, szValueOut, uValueOutSize, szName);
		if (iError != 0 || uRequiredSize == 0)
		{
			szValueOut[0] = '\0';
			return false;
		}
		return true;
	}
}
