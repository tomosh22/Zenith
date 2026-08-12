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

	bool SetVariable(const char* szName, const char* szValue)
	{
		if (szName == nullptr)
		{
			return false;
		}

		// _putenv_s with an empty value clears the variable, matching the
		// documented szValue == nullptr / "" clear contract.
		return ::_putenv_s(szName, szValue != nullptr ? szValue : "") == 0;
	}
}
