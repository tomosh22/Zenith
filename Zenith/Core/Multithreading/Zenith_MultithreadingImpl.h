#pragma once

#include <atomic>

// Per-Engine multithreading registry. Owns the shared state that used
// to live as module-scope globals in
// Zenith_Windows_Multithreading.cpp and Zenith_Android_Multithreading.cpp:
//   - thread-ID allocator (was ls_uThreadID function-local static)
//   - main-thread ID (was g_uMainThreadID)
//
// The per-thread thread-local state (tl_g_uThreadID, tl_g_acThreadName)
// stays thread-local because it's OS thread state -- threads don't
// belong to an Engine, only their registration index does. Carve-out
// per the refactor plan.
class Zenith_MultithreadingImpl
{
public:
	Zenith_MultithreadingImpl() = default;
	~Zenith_MultithreadingImpl() = default;

	Zenith_MultithreadingImpl(const Zenith_MultithreadingImpl&) = delete;
	Zenith_MultithreadingImpl& operator=(const Zenith_MultithreadingImpl&) = delete;

	// Called from the platform-specific Platform_RegisterThread.
	// Returns a unique ID for the calling thread. If bMainThread is
	// true, also stores the returned ID as the main-thread ID.
	u_int AllocateThreadID(bool bMainThread);

	// Main-thread ID accessor. Inline so hot-path IsMainThread reads
	// stay a single load. Returns ~0u until the main thread has
	// registered.
	u_int GetMainThreadID() const { return m_uMainThreadID; }

private:
	std::atomic<u_int> m_uNextThreadID{0};
	u_int m_uMainThreadID = ~0u;
};
