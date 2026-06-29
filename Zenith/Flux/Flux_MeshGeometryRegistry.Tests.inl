#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/Flux_MeshGeometryRegistry.h"
#include <cstdint>   // uintptr_t

// ============================================================================
// Flux_MeshGeometryRegistry Stage-0b unit tests.
//
// Headless: the registry's id/refcount/topology orchestration is exercised with a
// MOCK Provider (counts build/destroy, returns a fake non-null handle), so no GPU
// or renderer boot is needed. The real provider (Flux_MakeRealMeshGeometryProvider)
// is GPU-touching and validated windowed in Stage 0e/1.
// ============================================================================

namespace
{
	int   g_iMeshRegBuilds   = 0;
	int   g_iMeshRegDestroys = 0;
	void* g_pvMeshRegLastDestroyed = nullptr;

	void MeshReg_ResetCounters()
	{
		g_iMeshRegBuilds = 0;
		g_iMeshRegDestroys = 0;
		g_pvMeshRegLastDestroyed = nullptr;
	}

	// Fake non-null handle derived from the identity (never dereferenced).
	bool MeshReg_MockBuild(const Flux_MeshGeometryKey& xKey, void*& pvOut)
	{
		++g_iMeshRegBuilds;
		pvOut = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(xKey.m_pvIdentity) | 0x1u);
		return true;
	}
	void MeshReg_MockDestroy(void*& pvBuilt)
	{
		++g_iMeshRegDestroys;
		g_pvMeshRegLastDestroyed = pvBuilt;
		pvBuilt = nullptr;
	}
	bool MeshReg_FailBuild(const Flux_MeshGeometryKey&, void*& pvOut)
	{
		++g_iMeshRegBuilds;
		pvOut = nullptr;
		return false;   // build failure => invalid id, no entry
	}

	Flux_MeshGeometryRegistry::Provider MeshReg_MockProvider()
	{
		Flux_MeshGeometryRegistry::Provider x;
		x.m_pfnBuild = &MeshReg_MockBuild;
		x.m_pfnDestroy = &MeshReg_MockDestroy;
		return x;
	}

	Flux_MeshGeometryKey MeshReg_Key(uintptr_t uId, u_int uKind = FLUX_MESH_GEOMETRY_SOURCE_ASSET)
	{
		Flux_MeshGeometryKey xKey;
		xKey.m_pvIdentity = reinterpret_cast<const void*>(uId);
		xKey.m_uKind = uKind;
		return xKey;
	}
}

ZENITH_TEST(MeshGeoRegistry, IdOnlyModeAssignsStableIdsWithoutBuilding)
{
	// Null provider -> id-only mode (the inert Stage-0 scaffold use).
	Flux_MeshGeometryRegistry xRegistry;
	MeshReg_ResetCounters();

	xRegistry.BeginSync();
	const u_int uA = xRegistry.Reference(MeshReg_Key(0x1000));
	const u_int uB = xRegistry.Reference(MeshReg_Key(0x2000));
	const u_int uA2 = xRegistry.Reference(MeshReg_Key(0x1000));
	xRegistry.EndSync();

	ZENITH_ASSERT_EQ(uA, uA2, "same mesh identity -> same id");
	ZENITH_ASSERT_NE(uA, uB, "distinct mesh identities -> distinct ids");
	ZENITH_ASSERT_EQ(xRegistry.GetLiveCount(), 2u, "two distinct meshes -> two live entries");
	ZENITH_ASSERT_EQ(g_iMeshRegBuilds, 0, "id-only mode must not invoke any build");
	ZENITH_ASSERT_TRUE(xRegistry.IsValidId(uA), "assigned id is valid");
	ZENITH_ASSERT_NULL(xRegistry.GetBuilt(uA), "id-only mode has a null built handle");
}

ZENITH_TEST(MeshGeoRegistry, BuildsOnceOnFirstReference)
{
	Flux_MeshGeometryRegistry xRegistry;
	xRegistry.SetProvider(MeshReg_MockProvider());
	MeshReg_ResetCounters();

	xRegistry.BeginSync();
	const u_int uId  = xRegistry.Reference(MeshReg_Key(0x1000));
	const u_int uId2 = xRegistry.Reference(MeshReg_Key(0x1000));   // same key again
	xRegistry.EndSync();

	ZENITH_ASSERT_EQ(uId, uId2, "same key -> same id");
	ZENITH_ASSERT_EQ(g_iMeshRegBuilds, 1, "geometry must be built exactly once for repeated references");
	ZENITH_ASSERT_EQ(xRegistry.GetRefcount(MeshReg_Key(0x1000)), 2u, "refcount counts referencing draw-items");
	ZENITH_ASSERT_NOT_NULL(xRegistry.GetBuilt(uId), "built handle is exposed for the draw path");
}

ZENITH_TEST(MeshGeoRegistry, NoRebuildAcrossSyncs)
{
	Flux_MeshGeometryRegistry xRegistry;
	xRegistry.SetProvider(MeshReg_MockProvider());
	MeshReg_ResetCounters();

	for (int i = 0; i < 3; ++i)
	{
		xRegistry.BeginSync();
		xRegistry.Reference(MeshReg_Key(0x1000));
		xRegistry.EndSync();
	}

	ZENITH_ASSERT_EQ(g_iMeshRegBuilds, 1, "a persistent mesh is built once and reused across frames");
	ZENITH_ASSERT_EQ(g_iMeshRegDestroys, 0, "a persistently-referenced mesh is never torn down");
}

ZENITH_TEST(MeshGeoRegistry, TearsDownGeometryOnLastReference)
{
	Flux_MeshGeometryRegistry xRegistry;
	xRegistry.SetProvider(MeshReg_MockProvider());
	MeshReg_ResetCounters();

	// Frame 1: A + B built.
	xRegistry.BeginSync();
	xRegistry.Reference(MeshReg_Key(0x1000));
	const u_int uIdB = xRegistry.Reference(MeshReg_Key(0x2000));
	void* pvBuiltB = xRegistry.GetBuilt(uIdB);
	xRegistry.EndSync();
	ZENITH_ASSERT_EQ(g_iMeshRegBuilds, 2, "two meshes built in frame 1");

	// Frame 2: only A referenced -> B torn down.
	xRegistry.BeginSync();
	xRegistry.Reference(MeshReg_Key(0x1000));
	xRegistry.EndSync();

	ZENITH_ASSERT_EQ(g_iMeshRegDestroys, 1, "the unreferenced mesh is torn down");
	ZENITH_ASSERT_EQ(g_pvMeshRegLastDestroyed, pvBuiltB, "the correct (B's) handle is destroyed");
	ZENITH_ASSERT_EQ(xRegistry.GetLiveCount(), 1u, "only mesh A remains live");
}

ZENITH_TEST(MeshGeoRegistry, BuildFailureYieldsInvalidIdAndNoEntry)
{
	Flux_MeshGeometryRegistry xRegistry;
	Flux_MeshGeometryRegistry::Provider xFailing;
	xFailing.m_pfnBuild = &MeshReg_FailBuild;
	xFailing.m_pfnDestroy = &MeshReg_MockDestroy;
	xRegistry.SetProvider(xFailing);
	MeshReg_ResetCounters();

	xRegistry.BeginSync();
	const u_int uId = xRegistry.Reference(MeshReg_Key(0x1000));
	xRegistry.EndSync();

	ZENITH_ASSERT_EQ(uId, uFLUX_INVALID_MESH_GEOMETRY_ID, "a failed build returns the invalid id");
	ZENITH_ASSERT_EQ(xRegistry.GetLiveCount(), 0u, "a failed build registers no live entry");
	ZENITH_ASSERT_FALSE(xRegistry.IsValidId(uId), "the invalid id is not a valid mesh");
}

ZENITH_TEST(MeshGeoRegistry, AssetAndProceduralKindsAreDistinct)
{
	Flux_MeshGeometryRegistry xRegistry;
	MeshReg_ResetCounters();

	xRegistry.BeginSync();
	const u_int uAsset = xRegistry.Reference(MeshReg_Key(0x1000, FLUX_MESH_GEOMETRY_SOURCE_ASSET));
	const u_int uProc  = xRegistry.Reference(MeshReg_Key(0x1000, FLUX_MESH_GEOMETRY_SOURCE_PROCEDURAL));
	xRegistry.EndSync();

	ZENITH_ASSERT_NE(uAsset, uProc, "same identity pointer but different kind -> distinct meshes");
	ZENITH_ASSERT_EQ(xRegistry.GetLiveCount(), 2u, "asset and procedural kinds do not collide");
}

ZENITH_TEST(MeshGeoRegistry, RetiredIdIsRecycled)
{
	Flux_MeshGeometryRegistry xRegistry;
	xRegistry.SetProvider(MeshReg_MockProvider());
	MeshReg_ResetCounters();

	// Frame 1: A + B (high-water 2).
	xRegistry.BeginSync();
	xRegistry.Reference(MeshReg_Key(0x1000));
	const u_int uIdB = xRegistry.Reference(MeshReg_Key(0x2000));
	xRegistry.EndSync();
	ZENITH_ASSERT_EQ(xRegistry.GetHighWaterSlots(), 2u, "two meshes -> high-water 2");

	// Frame 2: drop B.
	xRegistry.BeginSync();
	xRegistry.Reference(MeshReg_Key(0x1000));
	xRegistry.EndSync();

	// Frame 3: new mesh C -> recycles B's freed id.
	xRegistry.BeginSync();
	xRegistry.Reference(MeshReg_Key(0x1000));
	const u_int uIdC = xRegistry.Reference(MeshReg_Key(0x3000));
	xRegistry.EndSync();

	ZENITH_ASSERT_EQ(uIdC, uIdB, "a new mesh recycles a retired mesh's freed id");
	ZENITH_ASSERT_EQ(xRegistry.GetHighWaterSlots(), 2u, "recycled id -> high-water does not grow");
}
