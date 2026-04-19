#include "Zenith.h"
#include "Zenith_DebugBreak.h"

#include <signal.h>

std::atomic<uint32_t> g_uAssertCaptureHitCount{0};
std::atomic<bool>     g_bAssertCaptureActive{false};

void Zenith_DebugBreak()
{
	if (g_bAssertCaptureActive.load(std::memory_order_acquire))
	{
		g_uAssertCaptureHitCount.fetch_add(1, std::memory_order_acq_rel);
		return;
	}
	raise(SIGTRAP);
}
