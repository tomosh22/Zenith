#pragma once

#include <cstddef>

namespace Zenith_PlatformEnvironment
{
	// Copies an environment variable, including its terminator, into the caller's
	// buffer. Returns false when it is absent, invalid, or does not fit.
	bool GetVariable(const char* szName, char* szValueOut, size_t uValueOutSize);

	// Sets (or, with szValue == nullptr or "", clears) an environment variable in
	// the current process. Returns false on invalid input or a platform-level
	// failure.
	bool SetVariable(const char* szName, const char* szValue);
}
