#include "Zenith.h"

#include "FileAccess/Zenith_SaveSystem.h"

#include <cstring>
#include <vector>

namespace
{
	// In-memory store, write log, and readback stash. All live in the
	// anonymous namespace so they aren't visible outside this TU. The
	// store is keyed by string so ReadBlob(szKey) can find what
	// WriteBlob(szKey, ...) put in.
	//
	// Active in BOTH shipping and test builds for the WriteBlob in-memory
	// path -- the MVP skeleton doesn't touch disk yet, and game code
	// reading-then-writing-and-reading-back during a session needs to
	// round-trip via this in-memory map. The write log + readback stash
	// (recording APIs) are test-build-only.
	std::vector<Zenith_SaveSystem::Blob> s_xStore;

#ifdef ZENITH_INPUT_SIMULATOR
	Zenith_Vector<Zenith_SaveSystem::Blob> s_xWriteLog;
	std::vector<Zenith_SaveSystem::Blob>   s_xReadbackStash;
	u_int                                  s_uCurrentFrame = 0;
#endif

	// Linear-search helper -- key counts are small for MVP gameplay (well
	// under 100), no need for a hash map.
	Zenith_SaveSystem::Blob* FindByKey(std::vector<Zenith_SaveSystem::Blob>& xStore,
	                                    const char* szKey)
	{
		if (szKey == nullptr) return nullptr;
		for (auto& xBlob : xStore)
		{
			if (xBlob.m_strKey == szKey) return &xBlob;
		}
		return nullptr;
	}
}

namespace Zenith_SaveSystem
{
	void WriteBlob(const char* szKey, const void* pData, size_t ulSize)
	{
		if (szKey == nullptr) return;

		// Update or insert in the in-memory store.
		Blob* pxExisting = FindByKey(s_xStore, szKey);
		Blob xNew;
		xNew.m_strKey = szKey;
		xNew.m_xData.Clear();
		if (pData != nullptr && ulSize > 0)
		{
			xNew.m_xData.Reserve(static_cast<u_int>(ulSize));
			const uint8_t* pxBytes = static_cast<const uint8_t*>(pData);
			for (size_t u = 0; u < ulSize; ++u)
			{
				xNew.m_xData.PushBack(pxBytes[u]);
			}
		}
#ifdef ZENITH_INPUT_SIMULATOR
		xNew.m_uFrame = s_uCurrentFrame;
#endif

		if (pxExisting != nullptr)
		{
			*pxExisting = xNew;
		}
		else
		{
			s_xStore.push_back(xNew);
		}

#ifdef ZENITH_INPUT_SIMULATOR
		// Always append to the write log so tests see every write, not just
		// the final state of each key.
		s_xWriteLog.PushBack(xNew);
#endif
	}

	const Blob* ReadBlob(const char* szKey)
	{
		if (szKey == nullptr) return nullptr;

#ifdef ZENITH_INPUT_SIMULATOR
		// Readback stash takes precedence over the in-memory store -- it's
		// how tests inject "what disk would have returned" without actually
		// writing first.
		Blob* pxStash = FindByKey(s_xReadbackStash, szKey);
		if (pxStash != nullptr) return pxStash;
#endif

		Blob* pxFound = FindByKey(s_xStore, szKey);
		return pxFound;
	}

#ifdef ZENITH_INPUT_SIMULATOR
	const Zenith_Vector<Blob>& GetWrittenBlobsForTest()
	{
		return s_xWriteLog;
	}

	void SetReadbackBlob(const char* szKey, const void* pData, size_t ulSize)
	{
		if (szKey == nullptr) return;
		Blob xBlob;
		xBlob.m_strKey = szKey;
		if (pData != nullptr && ulSize > 0)
		{
			const uint8_t* pxBytes = static_cast<const uint8_t*>(pData);
			xBlob.m_xData.Reserve(static_cast<u_int>(ulSize));
			for (size_t u = 0; u < ulSize; ++u)
			{
				xBlob.m_xData.PushBack(pxBytes[u]);
			}
		}
		xBlob.m_uFrame = s_uCurrentFrame;

		// Update or insert in the readback stash.
		Blob* pxExisting = FindByKey(s_xReadbackStash, szKey);
		if (pxExisting != nullptr)
		{
			*pxExisting = xBlob;
		}
		else
		{
			s_xReadbackStash.push_back(xBlob);
		}
	}

	void ClearForTest()
	{
		s_xStore.clear();
		s_xWriteLog.Clear();
		s_xReadbackStash.clear();
	}

	void AdvanceFrameForTest()
	{
		++s_uCurrentFrame;
	}
#endif
}
