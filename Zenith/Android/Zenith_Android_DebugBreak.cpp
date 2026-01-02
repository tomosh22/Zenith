#include "Zenith.h"
#include "Zenith_DebugBreak.h"

#include <signal.h>

void Zenith_DebugBreak()
{
	raise(SIGTRAP);
}
