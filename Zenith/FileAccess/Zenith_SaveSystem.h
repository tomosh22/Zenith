#pragma once

#include "Collections/Zenith_Vector.h"

#include <cstdint>
#include <string>

// ============================================================================
// Zenith_SaveSystem -- MVP-0.4.3 skeleton
//
// Engine save/load surface. The MVP skeleton doesn't touch disk yet -- it
// exposes write/read entry points and per-test recording so save-related
// tests can assert what would be persisted without actually serialising to
// the filesystem. The post-MVP implementation will route the same calls
// through the real Zenith_FileAccess platform path.
//
// Caller pattern (game code):
//   Zenith_SaveSystem::WriteBlob("villager.position", pData, ulSize);
//   const Blob* pxBlob = Zenith_SaveSystem::ReadBlob("villager.position");
//
// Test pattern (ZENITH_INPUT_SIMULATOR builds only):
//   Zenith_SaveSystem::ClearForTest();
//   // Stage a readback so the next ReadBlob returns this payload.
//   Zenith_SaveSystem::SetReadbackBlob("villager.position", payload, n);
//   ... run gameplay that calls Write/Read ...
//   const auto& xWrites = Zenith_SaveSystem::GetWrittenBlobsForTest();
//   Zenith_Assert(xWrites.GetSize() > 0, "Expected a write");
//
// Threading: WriteBlob / ReadBlob are main-thread only by contract. Async
// save lands post-MVP.
// ============================================================================
namespace Zenith_SaveSystem
{
	struct Blob
	{
		std::string m_strKey;
		Zenith_Vector<uint8_t> m_xData;
		u_int       m_uFrame = 0;
	};

	// Persist a payload under a key. MVP skeleton stores it in-memory keyed
	// by key (overwriting any previous value at the same key) AND -- in
	// test builds -- appends an entry to the per-frame write log so tests
	// can audit what was written and when.
	void WriteBlob(const char* szKey, const void* pData, size_t ulSize);

	// Retrieve a payload by key. Returns nullptr if no payload exists for
	// szKey. In test builds, SetReadbackBlob() stages a payload that
	// ReadBlob will return for that key.
	const Blob* ReadBlob(const char* szKey);

#ifdef ZENITH_INPUT_SIMULATOR
	// Returns the write log since the last ClearForTest. The reference is
	// valid until the next Clear / WriteBlob (vector may reallocate).
	const Zenith_Vector<Blob>& GetWrittenBlobsForTest();

	// Stage a payload so ReadBlob(szKey) returns it. Multiple SetReadback
	// calls for different keys are independent. The stash survives across
	// WriteBlob calls -- only ClearForTest wipes it.
	void SetReadbackBlob(const char* szKey, const void* pData, size_t ulSize);

	// Wipe the in-memory store, the write log, and the readback stash.
	void ClearForTest();

	// Tick the frame counter recorded in Blob::m_uFrame. Engine frame loop
	// calls this in test builds; game code does not.
	void AdvanceFrameForTest();
#endif
}
