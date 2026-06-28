#include "Zenith.h"
#include "Flux/Flux_MeshGeometryRegistry.h"
#include "Flux/MeshGeometry/Flux_MeshInstance.h"   // real provider only (forward-decls Zenith_MeshAsset / Flux_MeshGeometry)

// ============================================================================
// Flux_MeshGeometryRegistry (Stage 0b). The id/refcount/topology orchestration is
// pure (no GPU) — the GPU build/teardown is delegated to the injected Provider, so
// the whole surface below is headless-unit-testable with a mock provider (see the
// .Tests.inl hosted in an always-linked TU). The real provider at the bottom is
// the only GPU-touching part and is wired by the renderer (Stage 0e).
// ============================================================================

u_int Flux_MeshGeometryRegistry::AllocateSlot()
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

void Flux_MeshGeometryRegistry::BeginSync()
{
	for (u_int u = 0; u < m_axEntries.GetSize(); ++u)
	{
		m_axEntries.Get(u).m_uRefcountThisSync = 0u;
	}
}

u_int Flux_MeshGeometryRegistry::Reference(const Flux_MeshGeometryKey& xKey)
{
	if (u_int* puSlot = m_xKeyToSlot.TryGet(xKey))
	{
		++m_axEntries.Get(*puSlot).m_uRefcountThisSync;
		return *puSlot;
	}

	// First reference -> build the shared geometry. A non-null build fn that returns
	// nullptr means the build failed (e.g. an empty asset): do NOT register the key,
	// so a failed mesh never produces a live bucket.
	void* pvBuilt = nullptr;
	if (m_xProvider.m_pfnBuild != nullptr)
	{
		pvBuilt = m_xProvider.m_pfnBuild(xKey);
		if (pvBuilt == nullptr)
		{
			return uFLUX_INVALID_MESH_GEOMETRY_ID;
		}
	}

	const u_int uSlot = AllocateSlot();
	Entry& xEntry = m_axEntries.Get(uSlot);
	xEntry.m_xKey               = xKey;
	xEntry.m_pvBuilt            = pvBuilt;
	xEntry.m_uRefcountThisSync  = 1u;
	xEntry.m_uCommittedRefcount = 0u;
	xEntry.m_bAlive             = true;
	m_xKeyToSlot.Insert(xKey, uSlot);
	++m_uLiveCount;
	return uSlot;
}

void Flux_MeshGeometryRegistry::EndSync()
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
			// Last reference gone -> tear down the shared geometry + recycle the id.
			if (xEntry.m_pvBuilt != nullptr && m_xProvider.m_pfnDestroy != nullptr)
			{
				m_xProvider.m_pfnDestroy(xEntry.m_pvBuilt);
			}
			m_xKeyToSlot.Remove(xEntry.m_xKey);
			xEntry.m_pvBuilt            = nullptr;
			xEntry.m_bAlive             = false;
			xEntry.m_uCommittedRefcount = 0u;
			m_auFreeSlots.PushBack(u);
			--m_uLiveCount;
		}
		else
		{
			xEntry.m_uCommittedRefcount = xEntry.m_uRefcountThisSync;
		}
	}
}

void* Flux_MeshGeometryRegistry::GetBuilt(u_int uMeshGeometryId) const
{
	if (uMeshGeometryId >= m_axEntries.GetSize())
	{
		return nullptr;
	}
	const Entry& xEntry = m_axEntries.Get(uMeshGeometryId);
	return xEntry.m_bAlive ? xEntry.m_pvBuilt : nullptr;
}

bool Flux_MeshGeometryRegistry::IsValidId(u_int uMeshGeometryId) const
{
	return uMeshGeometryId < m_axEntries.GetSize() && m_axEntries.Get(uMeshGeometryId).m_bAlive;
}

bool Flux_MeshGeometryRegistry::TryGetId(const Flux_MeshGeometryKey& xKey, u_int& uOut) const
{
	const u_int* puSlot = m_xKeyToSlot.TryGet(xKey);
	if (!puSlot)
	{
		return false;
	}
	uOut = *puSlot;
	return true;
}

u_int Flux_MeshGeometryRegistry::GetRefcount(const Flux_MeshGeometryKey& xKey) const
{
	const u_int* puSlot = m_xKeyToSlot.TryGet(xKey);
	if (!puSlot)
	{
		return 0u;
	}
	return m_axEntries.Get(*puSlot).m_uCommittedRefcount;
}

// ----------------------------------------------------------------------------
// Production provider — the only GPU-touching code here. Builds a shared
// Flux_MeshInstance (which owns the VB/IB) from the keyed asset / procedural
// geometry, and tears it down through Flux_MeshInstance::Destroy (the deferred-
// delete wrappers). Wired by the renderer in Stage 0e; never runs headless.
// ----------------------------------------------------------------------------
namespace
{
	void* Flux_RealBuildMeshGeometry(const Flux_MeshGeometryKey& xKey)
	{
		void* pvIdentity = const_cast<void*>(xKey.m_pvIdentity);
		if (xKey.m_uKind == FLUX_MESH_GEOMETRY_SOURCE_PROCEDURAL)
		{
			return Flux_MeshInstance::CreateFromGeometry(static_cast<Flux_MeshGeometry*>(pvIdentity));
		}
		return Flux_MeshInstance::CreateFromAsset(static_cast<Zenith_MeshAsset*>(pvIdentity));
	}

	void Flux_RealDestroyMeshGeometry(void* pvBuilt)
	{
		Flux_MeshInstance* pxInstance = static_cast<Flux_MeshInstance*>(pvBuilt);
		if (pxInstance)
		{
			pxInstance->Destroy();   // deferred-delete VB/IB (no-op for procedural proxies)
			delete pxInstance;
		}
	}
}

Flux_MeshGeometryRegistry::Provider Flux_MakeRealMeshGeometryProvider()
{
	Flux_MeshGeometryRegistry::Provider xProvider;
	xProvider.m_pfnBuild   = &Flux_RealBuildMeshGeometry;
	xProvider.m_pfnDestroy = &Flux_RealDestroyMeshGeometry;
	return xProvider;
}

// NOTE: the tests (Flux_MeshGeometryRegistry.Tests.inl) are hosted at the end of
// Flux_MeshInstance.cpp (an always-linked TU). The renderer references this registry
// (Flux_MakeRealMeshGeometryProvider), but the test bodies are static-init
// registrations /OPT:REF can still dead-strip; hosting them in a TU whose surrounding
// code is referenced keeps them alive in every config — see the same pattern +
// rationale at the bottom of Flux_GPUScene.cpp.
