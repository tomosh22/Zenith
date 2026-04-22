#pragma once

// Save zone state and set up placement new protection
#ifdef ZENITH_PLACEMENT_NEW_ZONE
#define ZENITH_HASHMAP_ZONE_WAS_SET
#else
#define ZENITH_PLACEMENT_NEW_ZONE
#endif
#include "Memory/Zenith_MemoryManagement_Disabled.h"

#include "DataStream/Zenith_DataStream.h"
#include <functional>

// Default hash traits. Delegates to std::hash<K> so existing custom
// std::hash specialisations (e.g. std::hash<Flux_BarrierKey>) keep working
// after migration. Specialise Zenith_Hash<MyKey> to bypass the STL hash when
// a type does not ship a std::hash specialisation.
template<typename K>
struct Zenith_Hash
{
	u_int64 operator()(const K& xKey) const noexcept
	{
		return static_cast<u_int64>(std::hash<K>{}(xKey));
	}
};

template<typename K, typename V, typename Hasher = Zenith_Hash<K>>
class Zenith_HashMap
{
public:
	// Default construction is allocation-free; the first Insert lazily
	// allocates uDEFAULT_INITIAL_CAPACITY slots. Matches std::unordered_set's
	// "empty by default" behaviour, which matters for types that embed this
	// container as a member (e.g. Flux_RenderGraph) and construct a lot of
	// transient instances before any insert.
	Zenith_HashMap()
	: m_pxKeys(nullptr)
	, m_pxValues(nullptr)
	, m_puMeta(nullptr)
	, m_uSize(0)
	, m_uTombstones(0)
	, m_uCapacity(0)
	, m_uGeneration(0)
	{}

	explicit Zenith_HashMap(u_int uInitialCapacity)
	: m_pxKeys(nullptr)
	, m_pxValues(nullptr)
	, m_puMeta(nullptr)
	, m_uSize(0)
	, m_uTombstones(0)
	, m_uCapacity(0)
	, m_uGeneration(0)
	{
		if (uInitialCapacity > 0)
		{
			AllocateStorage(RoundUpToPowerOfTwo(uInitialCapacity));
		}
	}

	Zenith_HashMap(const Zenith_HashMap& xOther)
	: m_pxKeys(nullptr)
	, m_pxValues(nullptr)
	, m_puMeta(nullptr)
	, m_uSize(0)
	, m_uTombstones(0)
	, m_uCapacity(0)
	, m_uGeneration(0)
	{
		CopyFromOther(xOther);
	}

	Zenith_HashMap(Zenith_HashMap&& xOther)
	: m_pxKeys(xOther.m_pxKeys)
	, m_pxValues(xOther.m_pxValues)
	, m_puMeta(xOther.m_puMeta)
	, m_uSize(xOther.m_uSize)
	, m_uTombstones(xOther.m_uTombstones)
	, m_uCapacity(xOther.m_uCapacity)
	, m_uGeneration(0)
	{
		xOther.m_pxKeys = nullptr;
		xOther.m_pxValues = nullptr;
		xOther.m_puMeta = nullptr;
		xOther.m_uSize = 0;
		xOther.m_uTombstones = 0;
		xOther.m_uCapacity = 0;
		xOther.m_uGeneration++;
	}

	Zenith_HashMap& operator=(const Zenith_HashMap& xOther)
	{
		if (this == &xOther) return *this;
		DestroyAll();
		FreeStorage();
		CopyFromOther(xOther);
		m_uGeneration++;
		return *this;
	}

	Zenith_HashMap& operator=(Zenith_HashMap&& xOther)
	{
		if (this == &xOther) return *this;
		DestroyAll();
		FreeStorage();

		m_pxKeys = xOther.m_pxKeys;
		m_pxValues = xOther.m_pxValues;
		m_puMeta = xOther.m_puMeta;
		m_uSize = xOther.m_uSize;
		m_uTombstones = xOther.m_uTombstones;
		m_uCapacity = xOther.m_uCapacity;
		m_uGeneration++;

		xOther.m_pxKeys = nullptr;
		xOther.m_pxValues = nullptr;
		xOther.m_puMeta = nullptr;
		xOther.m_uSize = 0;
		xOther.m_uTombstones = 0;
		xOther.m_uCapacity = 0;
		xOther.m_uGeneration++;
		return *this;
	}

	~Zenith_HashMap()
	{
		DestroyAll();
		FreeStorage();
	}

	u_int GetSize() const { return m_uSize; }
	u_int GetCapacity() const { return m_uCapacity; }
	bool IsEmpty() const { return m_uSize == 0; }

	void Clear()
	{
		DestroyAll();
		if (m_puMeta != nullptr)
		{
			for (u_int u = 0; u < m_uCapacity; u++) m_puMeta[u] = uMETA_EMPTY;
		}
		m_uSize = 0;
		m_uTombstones = 0;
		m_uGeneration++;
	}

	void Reserve(u_int uNewCapacity)
	{
		u_int uRequired = RoundUpToPowerOfTwo(uNewCapacity);
		if (uRequired <= m_uCapacity) return;
		Rehash(uRequired);
	}

	V& Insert(const K& xKey, const V& xValue)
	{
		EnsureCapacityForInsert();
		u_int uSlot;
		bool bIsNew = LocateForInsert(xKey, uSlot);
		if (bIsNew)
		{
			new (&m_pxKeys[uSlot]) K(xKey);
			new (&m_pxValues[uSlot]) V(xValue);
			m_puMeta[uSlot] = uMETA_OCCUPIED;
			m_uSize++;
		}
		else
		{
			m_pxValues[uSlot].~V();
			new (&m_pxValues[uSlot]) V(xValue);
		}
		return m_pxValues[uSlot];
	}

	V& Insert(const K& xKey, V&& xValue)
	{
		EnsureCapacityForInsert();
		u_int uSlot;
		bool bIsNew = LocateForInsert(xKey, uSlot);
		if (bIsNew)
		{
			new (&m_pxKeys[uSlot]) K(xKey);
			new (&m_pxValues[uSlot]) V(std::move(xValue));
			m_puMeta[uSlot] = uMETA_OCCUPIED;
			m_uSize++;
		}
		else
		{
			m_pxValues[uSlot].~V();
			new (&m_pxValues[uSlot]) V(std::move(xValue));
		}
		return m_pxValues[uSlot];
	}

	template<typename... Args>
	V& Emplace(const K& xKey, Args&&... args)
	{
		EnsureCapacityForInsert();
		u_int uSlot;
		bool bIsNew = LocateForInsert(xKey, uSlot);
		if (bIsNew)
		{
			new (&m_pxKeys[uSlot]) K(xKey);
			new (&m_pxValues[uSlot]) V(std::forward<Args>(args)...);
			m_puMeta[uSlot] = uMETA_OCCUPIED;
			m_uSize++;
		}
		else
		{
			m_pxValues[uSlot].~V();
			new (&m_pxValues[uSlot]) V(std::forward<Args>(args)...);
		}
		return m_pxValues[uSlot];
	}

	bool Remove(const K& xKey)
	{
		u_int uSlot;
		if (!LocateExisting(xKey, uSlot)) return false;
		m_pxKeys[uSlot].~K();
		m_pxValues[uSlot].~V();
		m_puMeta[uSlot] = uMETA_TOMBSTONE;
		m_uSize--;
		m_uTombstones++;
		return true;
	}

	V& Get(const K& xKey)
	{
		u_int uSlot;
		Zenith_Assert(LocateExisting(xKey, uSlot), "Zenith_HashMap::Get: key not found");
		return m_pxValues[uSlot];
	}

	const V& Get(const K& xKey) const
	{
		u_int uSlot;
		Zenith_Assert(LocateExisting(xKey, uSlot), "Zenith_HashMap::Get: key not found");
		return m_pxValues[uSlot];
	}

	V* TryGet(const K& xKey)
	{
		u_int uSlot;
		if (!LocateExisting(xKey, uSlot)) return nullptr;
		return &m_pxValues[uSlot];
	}

	const V* TryGet(const K& xKey) const
	{
		u_int uSlot;
		if (!LocateExisting(xKey, uSlot)) return nullptr;
		return &m_pxValues[uSlot];
	}

	bool Contains(const K& xKey) const
	{
		u_int uSlot;
		return LocateExisting(xKey, uSlot);
	}

	// std::unordered_map-style convenience. Default-constructs V on first access
	// of a missing key. Requires V to be default-constructible.
	V& operator[](const K& xKey)
	{
		EnsureCapacityForInsert();
		u_int uSlot;
		bool bIsNew = LocateForInsert(xKey, uSlot);
		if (bIsNew)
		{
			new (&m_pxKeys[uSlot]) K(xKey);
			new (&m_pxValues[uSlot]) V();
			m_puMeta[uSlot] = uMETA_OCCUPIED;
			m_uSize++;
		}
		return m_pxValues[uSlot];
	}

	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		u_int uCount;
		xStream >> uCount;

		constexpr u_int uMAX_REASONABLE_SIZE = 100000000;
		Zenith_Assert(uCount <= uMAX_REASONABLE_SIZE,
			"Zenith_HashMap::ReadFromDataStream: Count %u exceeds reasonable limit", uCount);
		if (uCount > uMAX_REASONABLE_SIZE)
		{
			Zenith_Error(LOG_CATEGORY_CORE, "Zenith_HashMap::ReadFromDataStream: Count %u exceeds limit, aborting", uCount);
			return;
		}

		Clear();
		Reserve(uCount);

		for (u_int u = 0; u < uCount; u++)
		{
			if (xStream.GetCursor() >= xStream.GetSize())
			{
				Zenith_Error(LOG_CATEGORY_CORE, "Zenith_HashMap::ReadFromDataStream: Premature end of stream at entry %u of %u", u, uCount);
				Clear();
				return;
			}
			K xKey;
			V xValue;
			xStream >> xKey;
			xStream >> xValue;
			Insert(xKey, std::move(xValue));
		}
	}

	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		xStream << m_uSize;
		for (u_int u = 0; u < m_uCapacity; u++)
		{
			if (m_puMeta[u] == uMETA_OCCUPIED)
			{
				xStream << m_pxKeys[u];
				xStream << m_pxValues[u];
			}
		}
	}

	class Iterator
	{
	public:
		explicit Iterator(const Zenith_HashMap& xMap)
		: m_xMap(xMap)
		, m_uIndex(0)
		, m_uGeneration(xMap.m_uGeneration)
		{
			AdvanceToOccupied();
		}

		void Next()
		{
			Zenith_Assert(m_uGeneration == m_xMap.m_uGeneration,
				"Zenith_HashMap::Iterator invalidated: map was rehashed/cleared during iteration");
			Zenith_Assert(m_uIndex < m_xMap.m_uCapacity,
				"Zenith_HashMap::Iterator: Next past end");
			m_uIndex++;
			AdvanceToOccupied();
		}

		bool Done() const
		{
			Zenith_Assert(m_uGeneration == m_xMap.m_uGeneration,
				"Zenith_HashMap::Iterator invalidated: map was rehashed/cleared during iteration");
			return m_uIndex >= m_xMap.m_uCapacity;
		}

		const K& GetKey() const
		{
			Zenith_Assert(m_uGeneration == m_xMap.m_uGeneration, "Zenith_HashMap::Iterator invalidated");
			Zenith_Assert(m_uIndex < m_xMap.m_uCapacity, "Zenith_HashMap::Iterator: out of range");
			return m_xMap.m_pxKeys[m_uIndex];
		}

		const V& GetValue() const
		{
			Zenith_Assert(m_uGeneration == m_xMap.m_uGeneration, "Zenith_HashMap::Iterator invalidated");
			Zenith_Assert(m_uIndex < m_xMap.m_uCapacity, "Zenith_HashMap::Iterator: out of range");
			return m_xMap.m_pxValues[m_uIndex];
		}

		V& GetValueMutable() const
		{
			Zenith_Assert(m_uGeneration == m_xMap.m_uGeneration, "Zenith_HashMap::Iterator invalidated");
			Zenith_Assert(m_uIndex < m_xMap.m_uCapacity, "Zenith_HashMap::Iterator: out of range");
			return m_xMap.m_pxValues[m_uIndex];
		}

	private:
		void AdvanceToOccupied()
		{
			while (m_uIndex < m_xMap.m_uCapacity && m_xMap.m_puMeta[m_uIndex] != uMETA_OCCUPIED)
			{
				m_uIndex++;
			}
		}

		const Zenith_HashMap& m_xMap;
		u_int m_uIndex;
		u_int m_uGeneration;
	};

private:
	static constexpr u_int uDEFAULT_INITIAL_CAPACITY = 16;
	static constexpr u_int8 uMETA_EMPTY = 0;
	static constexpr u_int8 uMETA_OCCUPIED = 1;
	static constexpr u_int8 uMETA_TOMBSTONE = 2;

	static u_int RoundUpToPowerOfTwo(u_int uValue)
	{
		if (uValue < uDEFAULT_INITIAL_CAPACITY) return uDEFAULT_INITIAL_CAPACITY;
		u_int uResult = 1;
		while (uResult < uValue) uResult <<= 1;
		return uResult;
	}

	void AllocateStorage(u_int uCapacity)
	{
		Zenith_Assert((uCapacity & (uCapacity - 1)) == 0, "Zenith_HashMap capacity must be power of two");

		constexpr u_int uMAX_SAFE_CAPACITY_K = UINT_MAX / sizeof(K);
		constexpr u_int uMAX_SAFE_CAPACITY_V = UINT_MAX / sizeof(V);
		const u_int uMaxSafe = uMAX_SAFE_CAPACITY_K < uMAX_SAFE_CAPACITY_V ? uMAX_SAFE_CAPACITY_K : uMAX_SAFE_CAPACITY_V;
		Zenith_Assert(uCapacity <= uMaxSafe,
			"Zenith_HashMap capacity %u would overflow (sizeof(K)=%zu, sizeof(V)=%zu)",
			uCapacity, sizeof(K), sizeof(V));

		m_pxKeys = static_cast<K*>(Zenith_MemoryManagement::Allocate(uCapacity * sizeof(K)));
		m_pxValues = static_cast<V*>(Zenith_MemoryManagement::Allocate(uCapacity * sizeof(V)));
		m_puMeta = static_cast<u_int8*>(Zenith_MemoryManagement::Allocate(uCapacity * sizeof(u_int8)));
		Zenith_Assert(m_pxKeys != nullptr && m_pxValues != nullptr && m_puMeta != nullptr,
			"Zenith_HashMap allocation failed for capacity %u", uCapacity);

		for (u_int u = 0; u < uCapacity; u++) m_puMeta[u] = uMETA_EMPTY;
		m_uCapacity = uCapacity;
	}

	void FreeStorage()
	{
		if (m_pxKeys != nullptr) Zenith_MemoryManagement::Deallocate(m_pxKeys);
		if (m_pxValues != nullptr) Zenith_MemoryManagement::Deallocate(m_pxValues);
		if (m_puMeta != nullptr) Zenith_MemoryManagement::Deallocate(m_puMeta);
		m_pxKeys = nullptr;
		m_pxValues = nullptr;
		m_puMeta = nullptr;
		m_uCapacity = 0;
	}

	void DestroyAll()
	{
		if (m_puMeta == nullptr) return;
		for (u_int u = 0; u < m_uCapacity; u++)
		{
			if (m_puMeta[u] == uMETA_OCCUPIED)
			{
				m_pxKeys[u].~K();
				m_pxValues[u].~V();
			}
		}
	}

	void EnsureCapacityForInsert()
	{
		if (m_uCapacity == 0)
		{
			AllocateStorage(uDEFAULT_INITIAL_CAPACITY);
			return;
		}
		// Load factor 0.75: rehash when (size + tombstones) * 4 >= capacity * 3
		if ((m_uSize + m_uTombstones) * 4 >= m_uCapacity * 3)
		{
			Rehash(m_uCapacity * 2);
		}
	}

	void Rehash(u_int uNewCapacity)
	{
		K* pxOldKeys = m_pxKeys;
		V* pxOldValues = m_pxValues;
		u_int8* puOldMeta = m_puMeta;
		u_int uOldCapacity = m_uCapacity;

		m_pxKeys = nullptr;
		m_pxValues = nullptr;
		m_puMeta = nullptr;
		m_uCapacity = 0;
		AllocateStorage(uNewCapacity);

		m_uSize = 0;
		m_uTombstones = 0;
		m_uGeneration++;

		if (puOldMeta != nullptr)
		{
			for (u_int u = 0; u < uOldCapacity; u++)
			{
				if (puOldMeta[u] == uMETA_OCCUPIED)
				{
					u_int uSlot;
					// New table has no duplicates and enough room - always a fresh slot
					LocateForInsert(pxOldKeys[u], uSlot);
					new (&m_pxKeys[uSlot]) K(std::move(pxOldKeys[u]));
					new (&m_pxValues[uSlot]) V(std::move(pxOldValues[u]));
					m_puMeta[uSlot] = uMETA_OCCUPIED;
					m_uSize++;
					pxOldKeys[u].~K();
					pxOldValues[u].~V();
				}
			}
			Zenith_MemoryManagement::Deallocate(pxOldKeys);
			Zenith_MemoryManagement::Deallocate(pxOldValues);
			Zenith_MemoryManagement::Deallocate(puOldMeta);
		}
	}

	// Finds the slot for xKey. Returns true if this will be a new insert
	// (slot is EMPTY or TOMBSTONE); returns false if the key already exists
	// (slot is OCCUPIED with a matching key). uSlotOut is the target slot in
	// either case.
	bool LocateForInsert(const K& xKey, u_int& uSlotOut) const
	{
		Zenith_Assert(m_uCapacity > 0, "Zenith_HashMap::LocateForInsert on empty table");
		const u_int64 ulHash = Hasher{}(xKey);
		const u_int uMask = m_uCapacity - 1;
		u_int uSlot = static_cast<u_int>(ulHash) & uMask;
		u_int uFirstTombstone = m_uCapacity; // sentinel = none

		for (u_int uProbe = 0; uProbe < m_uCapacity; uProbe++)
		{
			const u_int8 uState = m_puMeta[uSlot];
			if (uState == uMETA_EMPTY)
			{
				// First empty slot ends the probe chain. If we saw a tombstone
				// earlier, prefer it to keep the chain short.
				uSlotOut = (uFirstTombstone != m_uCapacity) ? uFirstTombstone : uSlot;
				return true;
			}
			if (uState == uMETA_OCCUPIED && m_pxKeys[uSlot] == xKey)
			{
				uSlotOut = uSlot;
				return false;
			}
			if (uState == uMETA_TOMBSTONE && uFirstTombstone == m_uCapacity)
			{
				uFirstTombstone = uSlot;
			}
			uSlot = (uSlot + 1) & uMask;
		}

		// Table is saturated with OCCUPIED + TOMBSTONE — fall back to tombstone
		// if we saw one. A saturated all-OCCUPIED table cannot happen because
		// EnsureCapacityForInsert rehashes before that point.
		Zenith_Assert(uFirstTombstone != m_uCapacity,
			"Zenith_HashMap::LocateForInsert: probe exhausted with no empty or tombstone slot");
		uSlotOut = uFirstTombstone;
		return true;
	}

	// Finds an OCCUPIED slot with a matching key. Returns false if not present.
	bool LocateExisting(const K& xKey, u_int& uSlotOut) const
	{
		if (m_uCapacity == 0) return false;
		const u_int64 ulHash = Hasher{}(xKey);
		const u_int uMask = m_uCapacity - 1;
		u_int uSlot = static_cast<u_int>(ulHash) & uMask;

		for (u_int uProbe = 0; uProbe < m_uCapacity; uProbe++)
		{
			const u_int8 uState = m_puMeta[uSlot];
			if (uState == uMETA_EMPTY) return false;
			if (uState == uMETA_OCCUPIED && m_pxKeys[uSlot] == xKey)
			{
				uSlotOut = uSlot;
				return true;
			}
			uSlot = (uSlot + 1) & uMask;
		}
		return false;
	}

	void CopyFromOther(const Zenith_HashMap& xOther)
	{
		if (xOther.m_uCapacity == 0)
		{
			m_pxKeys = nullptr;
			m_pxValues = nullptr;
			m_puMeta = nullptr;
			m_uSize = 0;
			m_uTombstones = 0;
			m_uCapacity = 0;
			return;
		}
		AllocateStorage(xOther.m_uCapacity);
		for (u_int u = 0; u < xOther.m_uCapacity; u++)
		{
			if (xOther.m_puMeta[u] == uMETA_OCCUPIED)
			{
				new (&m_pxKeys[u]) K(xOther.m_pxKeys[u]);
				new (&m_pxValues[u]) V(xOther.m_pxValues[u]);
				m_puMeta[u] = uMETA_OCCUPIED;
			}
			else
			{
				// Tombstones are lost on copy — not a correctness concern because
				// lookup on tombstoned slots returns miss either way, and copy
				// starts with a fresh generation anyway.
				m_puMeta[u] = uMETA_EMPTY;
			}
		}
		m_uSize = xOther.m_uSize;
		m_uTombstones = 0;
	}

	K* m_pxKeys;
	V* m_pxValues;
	u_int8* m_puMeta;
	u_int m_uSize;
	u_int m_uTombstones;
	u_int m_uCapacity;
	u_int m_uGeneration;
};

// Restore zone state - only undefine if we defined it ourselves
#ifndef ZENITH_HASHMAP_ZONE_WAS_SET
#undef ZENITH_PLACEMENT_NEW_ZONE
#endif
#undef ZENITH_HASHMAP_ZONE_WAS_SET
#include "Memory/Zenith_MemoryManagement_Enabled.h"
