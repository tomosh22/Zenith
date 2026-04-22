#pragma once

#include "Collections/Zenith_HashMap.h"

// Zenith_HashSet is a thin wrapper over Zenith_HashMap with a minimal
// placeholder value type. Shares the HashMap's probing, rehashing, allocation,
// and iterator-invalidation semantics. Storage overhead is one byte per slot
// for the placeholder value (plus the HashMap's one-byte-per-slot metadata).
// If this overhead ever shows up in profiling, promote HashSet to a
// standalone implementation that omits the value array.
template<typename K, typename Hasher = Zenith_Hash<K>>
class Zenith_HashSet
{
public:
	Zenith_HashSet() = default;
	explicit Zenith_HashSet(u_int uInitialCapacity) : m_xMap(uInitialCapacity) {}

	u_int GetSize() const { return m_xMap.GetSize(); }
	u_int GetCapacity() const { return m_xMap.GetCapacity(); }
	bool IsEmpty() const { return m_xMap.IsEmpty(); }

	// Returns true if the key was newly inserted, false if it was already present.
	bool Insert(const K& xKey)
	{
		const bool bWasPresent = m_xMap.Contains(xKey);
		m_xMap.Insert(xKey, static_cast<u_int8>(0));
		return !bWasPresent;
	}

	bool Remove(const K& xKey) { return m_xMap.Remove(xKey); }
	void Clear() { m_xMap.Clear(); }
	void Reserve(u_int uNewCapacity) { m_xMap.Reserve(uNewCapacity); }

	bool Contains(const K& xKey) const { return m_xMap.Contains(xKey); }

	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		u_int uCount;
		xStream >> uCount;

		constexpr u_int uMAX_REASONABLE_SIZE = 100000000;
		Zenith_Assert(uCount <= uMAX_REASONABLE_SIZE,
			"Zenith_HashSet::ReadFromDataStream: Count %u exceeds reasonable limit", uCount);
		if (uCount > uMAX_REASONABLE_SIZE)
		{
			Zenith_Error(LOG_CATEGORY_CORE, "Zenith_HashSet::ReadFromDataStream: Count %u exceeds limit, aborting", uCount);
			return;
		}

		Clear();
		Reserve(uCount);
		for (u_int u = 0; u < uCount; u++)
		{
			if (xStream.GetCursor() >= xStream.GetSize())
			{
				Zenith_Error(LOG_CATEGORY_CORE, "Zenith_HashSet::ReadFromDataStream: Premature end of stream at entry %u of %u", u, uCount);
				Clear();
				return;
			}
			K xKey;
			xStream >> xKey;
			Insert(xKey);
		}
	}

	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		xStream << m_xMap.GetSize();
		typename Zenith_HashMap<K, u_int8, Hasher>::Iterator xIt(m_xMap);
		for (; !xIt.Done(); xIt.Next())
		{
			xStream << xIt.GetKey();
		}
	}

	class Iterator
	{
	public:
		explicit Iterator(const Zenith_HashSet& xSet) : m_xInner(xSet.m_xMap) {}
		void Next() { m_xInner.Next(); }
		bool Done() const { return m_xInner.Done(); }
		const K& GetKey() const { return m_xInner.GetKey(); }

	private:
		typename Zenith_HashMap<K, u_int8, Hasher>::Iterator m_xInner;
	};

private:
	Zenith_HashMap<K, u_int8, Hasher> m_xMap;
};
