#include "Zenith.h"

#include "Core/Zenith_PlatformEnvironment.h"

#include <cstdlib>
#include <cstring>

namespace Zenith_PlatformEnvironment
{
	bool GetVariable(const char* szName, char* szValueOut, size_t uValueOutSize)
	{
		if (szName == nullptr || szValueOut == nullptr || uValueOutSize == 0)
		{
			return false;
		}

		const char* szValue = std::getenv(szName);
		if (szValue == nullptr)
		{
			szValueOut[0] = '\0';
			return false;
		}

		const size_t uValueSize = std::strlen(szValue) + 1;
		if (uValueSize > uValueOutSize)
		{
			szValueOut[0] = '\0';
			return false;
		}

		std::memcpy(szValueOut, szValue, uValueSize);
		return true;
	}
}
