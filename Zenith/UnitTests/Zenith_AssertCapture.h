#pragma once

#include <cstdint>

// RAII scope for intercepting Zenith_Assert failures during unit tests.
//
// While a Zenith_AssertCaptureScope is alive, Zenith_DebugBreak() — the sink
// called by Zenith_Assert on failure — records a hit instead of halting. Tests
// use this to verify that an assert fires under a given condition without
// crashing the test runner.
//
// Usage:
//     {
//         Zenith_AssertCaptureScope xCapture;
//         DoSomethingThatShouldAssert();
//         Zenith_Assert(xCapture.DidAssertFire(), "Expected assert did not fire");
//     }
//
// Nesting is not supported. Entering a nested scope is itself an assertion
// failure — but because the outer scope swallows it, nested scopes log an
// error instead of asserting. The ctor snapshots the prior active/hit-count
// state and the dtor restores it, so a stray nested scope leaves the outer
// scope's ledger intact instead of silently disabling capture.
class Zenith_AssertCaptureScope
{
public:
	Zenith_AssertCaptureScope();
	~Zenith_AssertCaptureScope();

	Zenith_AssertCaptureScope(const Zenith_AssertCaptureScope&) = delete;
	Zenith_AssertCaptureScope& operator=(const Zenith_AssertCaptureScope&) = delete;

	uint32_t GetHitCount() const;
	bool     DidAssertFire() const;
	void     ResetHitCount();

private:
	bool     m_bPrevActive;
	uint32_t m_uPrevHitCount;
};
