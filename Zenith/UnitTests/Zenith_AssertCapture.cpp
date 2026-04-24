#include "Zenith.h"
#include "UnitTests/Zenith_AssertCapture.h"
#include "Zenith_DebugBreak.h"

Zenith_AssertCaptureScope::Zenith_AssertCaptureScope()
{
	m_bPrevActive   = g_bAssertCaptureActive.load(std::memory_order_acquire);
	m_uPrevHitCount = g_uAssertCaptureHitCount.load(std::memory_order_acquire);
	if (m_bPrevActive)
	{
	}
	g_uAssertCaptureHitCount.store(0, std::memory_order_release);
	g_bAssertCaptureActive.store(true, std::memory_order_release);
}

Zenith_AssertCaptureScope::~Zenith_AssertCaptureScope()
{
	g_bAssertCaptureActive.store(m_bPrevActive, std::memory_order_release);
	g_uAssertCaptureHitCount.store(m_uPrevHitCount, std::memory_order_release);
}

uint32_t Zenith_AssertCaptureScope::GetHitCount() const
{
	return g_uAssertCaptureHitCount.load(std::memory_order_acquire);
}

bool Zenith_AssertCaptureScope::DidAssertFire() const
{
	return GetHitCount() > 0;
}

void Zenith_AssertCaptureScope::ResetHitCount()
{
	g_uAssertCaptureHitCount.store(0, std::memory_order_release);
}
