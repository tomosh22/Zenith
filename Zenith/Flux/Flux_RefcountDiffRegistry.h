#pragma once

#include "Collections/Zenith_Vector.h"
#include "Collections/Zenith_HashMap.h"
#include <utility>   // std::move

// ============================================================================
// Flux_RefcountDiffRegistry<Key, Payload> — the ONE refcount-diff sync registry.
//
// The unified GPU-driven opaque-mesh pipeline grew three structurally-identical
// "refcount-diff sync" registries (bucket / shared-mesh-geometry / skinned-instance
// id) plus a fourth generation-mark/sweep skinned-pose store. They all answer the
// same question once per frame — "which keyed entities are referenced THIS sync,
// and which dropped to zero so I can retire + recycle them?" — so they all share
// the same machinery: a key->slot map, a stable slot table with a free-list, and a
// BeginSync -> Reference* -> EndSync diff. This template is that machinery, factored
// out once; the four registries become thin instantiations that add only their own
// extras (topology signal + key iteration; provider build/destroy; an id-overflow
// assert; an owned bind-pose pool).
//
// Pure CPU (no GPU includes) -> the whole surface is headless-unit-testable. The
// only GPU-touching part is the optional Provider (captureless fn-ptr trampolines,
// the codebase idiom), injected at runtime so a mock provider exercises the
// id/refcount/topology orchestration with no renderer boot.
//
// LIFETIME MODEL (one sync):
//   BeginSync()              — zero every live entry's per-sync refcount.
//   Reference(key)           — find-or-create; ++refcount; returns the STABLE slot
//                              id (stable across frames for a live key; freed slots
//                              are recycled). On CREATE with a Provider, m_pfnBuild
//                              builds the payload; a build that returns false (e.g.
//                              an empty asset) registers NO entry and returns the
//                              invalid sentinel. Create flags a topology change.
//   EndSync()                — retire every entry whose refcount is still 0 (last
//                              reference gone): m_pfnDestroy tears the payload down,
//                              the slot is recycled, and a retire flags a topology
//                              change. Surviving entries commit their refcount.
// ============================================================================

// Returned by Reference() when a Provider build fails — the key is NOT registered.
inline constexpr u_int uFLUX_REFCOUNT_REGISTRY_INVALID_SLOT = 0xffffffffu;

// Empty payload for registries that allocate ids only (no per-entry data).
struct Flux_NoPayload {};

template<typename Key, typename Payload = Flux_NoPayload>
class Flux_RefcountDiffRegistry
{
public:
	// GPU build/teardown is delegated so the orchestration stays headless-testable.
	// Captureless trampolines (codebase idiom):
	//   m_pfnBuild   : build the payload for a freshly-created key. Return false to
	//                  signal BUILD FAILURE — the key is then NOT registered and
	//                  Reference returns uFLUX_REFCOUNT_REGISTRY_INVALID_SLOT.
	//   m_pfnDestroy : tear down a payload when its entry retires (last ref gone).
	// Both null = ID-ONLY mode (entries carry a default-constructed payload) — the
	// bucket + skinned-id registries, and pure tests, run this way.
	struct Provider
	{
		bool (*m_pfnBuild)(const Key& xKey, Payload& xPayloadOut) = nullptr;
		void (*m_pfnDestroy)(Payload& xPayload)                   = nullptr;
	};

	void SetProvider(const Provider& xProvider) { m_xProvider = xProvider; }

	// One refcount-diff sync = BeginSync() -> Reference() per referencing item -> EndSync().
	void BeginSync()
	{
		m_bTopologyChangedThisSync = false;
		m_bAnyRetiredThisSync      = false;
		for (u_int u = 0; u < m_axEntries.GetSize(); ++u)
		{
			m_axEntries.Get(u).m_uRefcountThisSync = 0u;
		}
	}

	// Find-or-create. Returns the stable slot id (or the invalid sentinel on build
	// failure). pbCreated, if given, reports whether THIS call created the entry
	// (the seam a subclass uses to do create-only side work, e.g. pool append).
	u_int Reference(const Key& xKey, bool* pbCreated = nullptr)
	{
		if (u_int* puSlot = m_xKeyToSlot.TryGet(xKey))
		{
			++m_axEntries.Get(*puSlot).m_uRefcountThisSync;
			if (pbCreated) { *pbCreated = false; }
			return *puSlot;
		}

		// First reference this sync -> build the payload. A non-null build fn that
		// returns false means the build failed: do NOT register the key.
		Payload xPayload{};
		if (m_xProvider.m_pfnBuild != nullptr)
		{
			if (!m_xProvider.m_pfnBuild(xKey, xPayload))
			{
				if (pbCreated) { *pbCreated = false; }
				return uFLUX_REFCOUNT_REGISTRY_INVALID_SLOT;
			}
		}

		const u_int uSlot = AllocateSlot();
		Entry& xEntry = m_axEntries.Get(uSlot);
		xEntry.m_xKey               = xKey;
		xEntry.m_xPayload           = std::move(xPayload);
		xEntry.m_uRefcountThisSync  = 1u;
		xEntry.m_uCommittedRefcount = 0u;
		xEntry.m_bAlive             = true;
		m_xKeyToSlot.Insert(xKey, uSlot);
		++m_uLiveCount;
		m_bTopologyChangedThisSync = true;
		if (pbCreated) { *pbCreated = true; }
		return uSlot;
	}

	void EndSync()
	{
		for (u_int u = 0; u < m_axEntries.GetSize(); ++u)
		{
			Entry& xEntry = m_axEntries.Get(u);
			if (!xEntry.m_bAlive)
			{
				continue;
			}

			if (xEntry.m_uRefcountThisSync == 0u)
			{
				// Last reference gone -> retire: tear the payload down + recycle the slot.
				if (m_xProvider.m_pfnDestroy != nullptr)
				{
					m_xProvider.m_pfnDestroy(xEntry.m_xPayload);
				}
				m_xKeyToSlot.Remove(xEntry.m_xKey);
				xEntry.m_bAlive             = false;
				xEntry.m_uCommittedRefcount = 0u;
				m_auFreeSlots.PushBack(u);
				--m_uLiveCount;
				m_bTopologyChangedThisSync = true;
				m_bAnyRetiredThisSync      = true;
			}
			else
			{
				xEntry.m_uCommittedRefcount = xEntry.m_uRefcountThisSync;
			}
		}
	}

	// ---- shared read-only surface (re-exposed under domain names by subclasses) --
	bool  WasTopologyChangedThisSync() const { return m_bTopologyChangedThisSync; }
	bool  WasAnyRetiredThisSync()      const { return m_bAnyRetiredThisSync; }
	u_int GetLiveCount()               const { return m_uLiveCount; }
	u_int GetHighWaterSlots()          const { return m_uHighWater; }
	u_int GetSlotCount()               const { return m_axEntries.GetSize(); }

	bool TryGetId(const Key& xKey, u_int& uOut) const
	{
		const u_int* puSlot = m_xKeyToSlot.TryGet(xKey);
		if (!puSlot) { return false; }
		uOut = *puSlot;
		return true;
	}

	bool HasKey(const Key& xKey) const { return m_xKeyToSlot.Contains(xKey); }

	// Committed refcount of a key (0 if absent). The committed count is last sync's
	// reference total — distinct from the in-progress m_uRefcountThisSync.
	u_int GetCommittedRefcount(const Key& xKey) const
	{
		const u_int* puSlot = m_xKeyToSlot.TryGet(xKey);
		if (!puSlot) { return 0u; }
		return m_axEntries.Get(*puSlot).m_uCommittedRefcount;
	}

	bool IsSlotAlive(u_int uSlot) const
	{
		return uSlot < m_axEntries.GetSize() && m_axEntries.Get(uSlot).m_bAlive;
	}

	const Key* TryGetKey(u_int uSlot) const
	{
		if (!IsSlotAlive(uSlot)) { return nullptr; }
		return &m_axEntries.Get(uSlot).m_xKey;
	}

	const Payload* TryGetPayload(u_int uSlot) const
	{
		if (!IsSlotAlive(uSlot)) { return nullptr; }
		return &m_axEntries.Get(uSlot).m_xPayload;
	}

	Payload* TryGetPayloadMutable(u_int uSlot)
	{
		if (!IsSlotAlive(uSlot)) { return nullptr; }
		return &m_axEntries.Get(uSlot).m_xPayload;
	}

protected:
	struct Entry
	{
		Key     m_xKey               = Key{};
		Payload m_xPayload           = Payload{};
		u_int   m_uRefcountThisSync  = 0u;
		u_int   m_uCommittedRefcount = 0u;
		bool    m_bAlive             = false;
	};

	u_int AllocateSlot()
	{
		if (m_auFreeSlots.GetSize() > 0u)
		{
			const u_int uSlot = m_auFreeSlots.Get(m_auFreeSlots.GetSize() - 1u);
			m_auFreeSlots.PopBack();
			return uSlot;
		}
		const u_int uSlot = m_axEntries.GetSize();
		m_axEntries.PushBack(Entry{});
		m_uHighWater = m_axEntries.GetSize();
		return uSlot;
	}

	Zenith_HashMap<Key, u_int> m_xKeyToSlot;   // key -> index into m_axEntries
	Zenith_Vector<Entry>       m_axEntries;    // indexed by the stable slot id
	Zenith_Vector<u_int>       m_auFreeSlots;  // recycled slot ids
	u_int    m_uHighWater = 0u;                // max slots ever allocated
	u_int    m_uLiveCount = 0u;
	bool     m_bTopologyChangedThisSync = false;  // a create OR retire happened this sync
	bool     m_bAnyRetiredThisSync      = false;  // a retire specifically happened this sync
	Provider m_xProvider;
};
