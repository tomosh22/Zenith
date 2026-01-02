#include "Zenith.h"
#include "Zenith_DebugBreak.h"

#include <intrin.h>

void Zenith_DebugBreak()
{
	__debugbreak();
}
