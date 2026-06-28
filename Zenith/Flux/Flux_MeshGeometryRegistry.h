#pragma once

#include "Collections/Zenith_Vector.h"
#include "Collections/Zenith_HashMap.h"
#include <cstddef>   // size_t

// ============================================================================
// Flux_MeshGeometryRegistry (Stage 0b — unified GPU-driven opaque-mesh pipeline)
//
// Owns ONE shared mesh geometry (VB/IB) per unique mesh identity, refcounted, and
// hands out a stable u_int meshGeometryId used as part of the GPU-scene bucket key
// (Flux_GPUSceneBucketKey). Without this, every repeated static model would get
// its own VB/IB (Flux_MeshInstance::CreateFromAsset allocates per instance) and so
// its own bucket — defeating indirect-draw batching.
//
// Lifetime model = the same refcount-diff sync the bucket registry uses, driven by
// the SAME per-frame snapshot walk (the adapter in Stage 0e):
//   BeginSync() -> Reference(key) per source submesh -> EndSync().
// First reference to a key BUILDS the shared geometry; the last reference going
// away RETIRES it (deferred-delete teardown). Geometry is built/torn-down through
// an injectable Provider so the registry's id/refcount/topology ORCHESTRATION is
// headless-unit-testable with a mock (the real provider touches the GPU via
// Flux_MeshInstance, so it can only run windowed).
//
// The key stores an opaque identity pointer (Zenith_MeshAsset* for asset meshes,
// Flux_MeshGeometry* for procedural meshes) + a kind tag, so the header stays free
// of mesh/GPU includes.
// ============================================================================

enum FluxMeshGeometrySourceKind : u_int
{
	FLUX_MESH_GEOMETRY_SOURCE_ASSET      = 0u,  // m_pvIdentity is a Zenith_MeshAsset*
	FLUX_MESH_GEOMETRY_SOURCE_PROCEDURAL = 1u,  // m_pvIdentity is a Flux_MeshGeometry*
};

inline constexpr u_int uFLUX_INVALID_MESH_GEOMETRY_ID = 0xffffffffu;

struct Flux_MeshGeometryKey
{
	const void* m_pvIdentity = nullptr;                          // stable per-mesh identity
	u_int       m_uKind      = FLUX_MESH_GEOMETRY_SOURCE_ASSET;  // FluxMeshGeometrySourceKind

	bool operator==(const Flux_MeshGeometryKey& xOther) const
	{
		return m_pvIdentity == xOther.m_pvIdentity && m_uKind == xOther.m_uKind;
	}
};

// FNV-1a over the identity pointer bits + kind.
template<>
struct Zenith_Hash<Flux_MeshGeometryKey>
{
	u_int64 operator()(const Flux_MeshGeometryKey& xKey) const noexcept
	{
		u_int64 uHash = 0xcbf29ce484222325ull;
		auto Bytes = [&uHash](const void* p, size_t n)
		{
			const u_int8* pb = static_cast<const u_int8*>(p);
			for (size_t i = 0; i < n; ++i) { uHash ^= pb[i]; uHash *= 0x100000001b3ull; }
		};
		Bytes(&xKey.m_pvIdentity, sizeof(xKey.m_pvIdentity));
		Bytes(&xKey.m_uKind,      sizeof(xKey.m_uKind));
		return uHash;
	}
};

class Flux_MeshGeometryRegistry
{
public:
	// GPU build/teardown is delegated so the registry's orchestration is
	// headless-testable with a mock. Captureless trampolines (codebase idiom):
	//   m_pfnBuild   : build the shared geometry for a key; return an opaque handle
	//                  the registry stores and passes back to m_pfnDestroy. Return
	//                  nullptr to signal BUILD FAILURE (the key is then NOT
	//                  registered and Reference returns uFLUX_INVALID_MESH_GEOMETRY_ID).
	//   m_pfnDestroy : tear down a handle previously returned by m_pfnBuild.
	// If m_pfnBuild is null the registry runs in ID-ONLY mode (entries are created
	// with a null built-handle) — used by the inert Stage-0 scaffold + pure tests.
	struct Provider
	{
		void* (*m_pfnBuild)(const Flux_MeshGeometryKey& xKey) = nullptr;
		void  (*m_pfnDestroy)(void* pvBuilt)                  = nullptr;
	};

	void SetProvider(const Provider& xProvider) { m_xProvider = xProvider; }

	// One refcount-diff sync = BeginSync() -> Reference() per source submesh -> EndSync().
	void  BeginSync();
	u_int Reference(const Flux_MeshGeometryKey& xKey);   // builds on create; invalid id on build-fail
	void  EndSync();                                      // tears down retired geometry

	void* GetBuilt(u_int uMeshGeometryId) const;          // provider handle (Flux_MeshInstance* in prod) or nullptr
	bool  IsValidId(u_int uMeshGeometryId) const;
	u_int GetLiveCount()      const { return m_uLiveCount; }
	u_int GetHighWaterSlots() const { return m_uHighWater; }

	// Read-only lookups (also exercised by tests).
	bool  TryGetId(const Flux_MeshGeometryKey& xKey, u_int& uOut) const;
	u_int GetRefcount(const Flux_MeshGeometryKey& xKey) const;

private:
	struct Entry
	{
		Flux_MeshGeometryKey m_xKey;
		void* m_pvBuilt            = nullptr;
		u_int m_uRefcountThisSync  = 0u;
		u_int m_uCommittedRefcount = 0u;
		bool  m_bAlive             = false;
	};

	u_int AllocateSlot();

	Zenith_HashMap<Flux_MeshGeometryKey, u_int> m_xKeyToSlot;  // key -> index into m_axEntries
	Zenith_Vector<Entry> m_axEntries;                          // indexed by meshGeometryId
	Zenith_Vector<u_int> m_auFreeSlots;                        // recycled ids
	u_int    m_uHighWater = 0u;
	u_int    m_uLiveCount = 0u;
	Provider m_xProvider;
};

// Production provider: build = Flux_MeshInstance::CreateFromAsset / CreateFromGeometry,
// destroy = Flux_MeshInstance::Destroy + delete. Touches the GPU, so it is wired by
// the renderer (Stage 0e) and exercised windowed, never headless. Defined in the .cpp.
Flux_MeshGeometryRegistry::Provider Flux_MakeRealMeshGeometryProvider();
