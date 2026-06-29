#pragma once

#include "Flux/Flux_RefcountDiffRegistry.h"
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
// This is a thin instantiation of Flux_RefcountDiffRegistry<Key, void*>: the shared
// refcount-diff sync machinery (BeginSync -> Reference per source submesh -> EndSync,
// build-on-create / teardown-on-retire, id recycling) lives in the base; this class
// adds only the mesh-domain accessor names + the GPU build/destroy Provider. First
// reference to a key BUILDS the shared geometry; the last reference going away RETIRES
// it (deferred-delete teardown). Geometry is built/torn-down through the injectable
// Provider so the orchestration is headless-unit-testable with a mock (the real
// provider touches the GPU via Flux_MeshInstance, so it can only run windowed).
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

// A failed build returns the registry's invalid-slot sentinel.
inline constexpr u_int uFLUX_INVALID_MESH_GEOMETRY_ID = uFLUX_REFCOUNT_REGISTRY_INVALID_SLOT;

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

// Payload = the opaque provider build handle (Flux_MeshInstance* in prod; a fake
// non-null handle in tests). The base stores/recycles it; this class just exposes it.
class Flux_MeshGeometryRegistry : public Flux_RefcountDiffRegistry<Flux_MeshGeometryKey, void*>
{
public:
	// BeginSync / Reference / EndSync / SetProvider / GetLiveCount / GetHighWaterSlots /
	// TryGetId are inherited. Reference returns uFLUX_INVALID_MESH_GEOMETRY_ID on build
	// failure (the base's invalid sentinel). Below: mesh-domain accessor names.
	void* GetBuilt(u_int uMeshGeometryId) const            // provider handle, or nullptr
	{
		void* const* ppBuilt = TryGetPayload(uMeshGeometryId);
		return ppBuilt ? *ppBuilt : nullptr;
	}
	bool  IsValidId(u_int uMeshGeometryId) const { return IsSlotAlive(uMeshGeometryId); }
	u_int GetRefcount(const Flux_MeshGeometryKey& xKey) const { return GetCommittedRefcount(xKey); }
};

// Production provider: build = Flux_MeshInstance::CreateFromAsset / CreateFromGeometry,
// destroy = Flux_MeshInstance::Destroy + delete. Touches the GPU, so it is wired by
// the renderer (Stage 0e) and exercised windowed, never headless. Defined in the .cpp.
Flux_MeshGeometryRegistry::Provider Flux_MakeRealMeshGeometryProvider();
