// Scenario 13: test-only. Included only from LibA.cpp, and reaches LibC only
// through this file -- so LibA -> LibC should classify as test-only, and
// LibA.txt must carry no UNDEF resolving to a LibC symbol.
#include "Shared/LibC_Api.h"

static void LibA_Tests_UsesLibC()
{
	LibC_Api_Touch();
}
