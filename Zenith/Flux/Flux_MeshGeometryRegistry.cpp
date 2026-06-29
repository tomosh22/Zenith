#include "Zenith.h"
#include "Flux/Flux_MeshGeometryRegistry.h"
#include "Flux/MeshGeometry/Flux_MeshInstance.h"   // real provider only (forward-decls Zenith_MeshAsset / Flux_MeshGeometry)

// ============================================================================
// Flux_MeshGeometryRegistry (Stage 0b). The id/refcount/topology orchestration now
// lives in Flux_RefcountDiffRegistry<Key, void*> (header) — pure, headless-testable
// with a mock provider (see the .Tests.inl hosted in an always-linked TU). The only
// thing left here is the real GPU-touching Provider, wired by the renderer (Stage 0e).
// ============================================================================
namespace
{
	bool Flux_RealBuildMeshGeometry(const Flux_MeshGeometryKey& xKey, void*& pvBuiltOut)
	{
		void* pvIdentity = const_cast<void*>(xKey.m_pvIdentity);
		Flux_MeshInstance* pxInstance = (xKey.m_uKind == FLUX_MESH_GEOMETRY_SOURCE_PROCEDURAL)
			? Flux_MeshInstance::CreateFromGeometry(static_cast<Flux_MeshGeometry*>(pvIdentity))
			: Flux_MeshInstance::CreateFromAsset(static_cast<Zenith_MeshAsset*>(pvIdentity));
		pvBuiltOut = pxInstance;
		return pxInstance != nullptr;   // false => empty/failed asset => no live entry/bucket
	}

	void Flux_RealDestroyMeshGeometry(void*& pvBuilt)
	{
		Flux_MeshInstance* pxInstance = static_cast<Flux_MeshInstance*>(pvBuilt);
		if (pxInstance)
		{
			pxInstance->Destroy();   // deferred-delete VB/IB (no-op for procedural proxies)
			delete pxInstance;
		}
		pvBuilt = nullptr;
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
