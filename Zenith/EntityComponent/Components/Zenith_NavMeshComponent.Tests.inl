#include "Core/Zenith_TestFramework.h"

#ifdef ZENITH_TESTING

#include "AI/Navigation/Zenith_NavMeshBaker.h"
#include "DataStream/Zenith_DataStream.h"
#include "UnitTests/Zenith_AssertCapture.h"
#include "UnitTests/Zenith_TempScene.h"
#include "UnitTests/Zenith_UnitTests.h"

#include <filesystem>

// ============================================================================
// Zenith_NavMeshComponent tests (SC1b commit B, ZM-D-147)
//
// What these pin: the ref is the ONLY thing that serializes; OnStart loads and
// the component OWNS the result; a scene unload frees it; a pool relocation
// leaves exactly one owner; and a ref that will not load surfaces a FAILED state
// with a reason (which is what the editor panel displays) rather than pretending
// to be empty.
//
// TIMING: OnStart is deferred to the entity's first Update, so every unit that
// expects a load ticks the scene once.
// ============================================================================

namespace
{
	// Counts COMPONENTS, not meshes. A Zenith_NavMesh that is never deleted is
	// invisible to an ECS query, so this pins COMPONENT lifetime only -- the
	// mesh's exactly-one-owner invariant is what the move-relocation unit pins.
	u_int CountLoadedNavMeshComponents()
	{
		u_int uCount = 0u;
		g_xEngine.Scenes().QueryAllScenes<Zenith_NavMeshComponent>().ForEach(
			[&uCount](Zenith_EntityID, Zenith_NavMeshComponent&) { ++uCount; });
		return uCount;
	}

	// Bake a tiny real navmesh to a temp file so the component has something
	// genuine to load -- no fixture bytes, no hand-rolled file.
	struct NavMeshComponentTestAsset
	{
		std::string m_strPath;
		bool m_bBaked = false;

		explicit NavMeshComponentTestAsset(const char* szLeafName)
		{
			std::error_code xEC;
			std::filesystem::path xDir = std::filesystem::temp_directory_path(xEC);
			if (xEC)
			{
				xDir = ".";
			}
			m_strPath = (xDir / szLeafName).string();
			std::filesystem::remove(m_strPath, xEC);

			Zenith_Vector<Zenith_Maths::Vector3> axVertices;
			Zenith_Vector<uint32_t> auIndices;
			BuildFlatGround(axVertices, auIndices);

			NavMeshGenerationConfig xConfig;
			const Zenith_NavMeshBakeResult xResult =
				Zenith_NavMeshBaker::BakeToFile(axVertices, auIndices, xConfig, m_strPath);
			m_bBaked = xResult.m_bSuccess;
		}

		~NavMeshComponentTestAsset()
		{
			std::error_code xEC;
			std::filesystem::remove(m_strPath, xEC);
		}

		static void BuildFlatGround(Zenith_Vector<Zenith_Maths::Vector3>& axVertices,
			Zenith_Vector<uint32_t>& auIndices)
		{
			constexpr u_int uQUADS = 4u;
			for (u_int uZ = 0; uZ <= uQUADS; ++uZ)
			{
				for (u_int uX = 0; uX <= uQUADS; ++uX)
				{
					axVertices.PushBack(Zenith_Maths::Vector3(
						static_cast<float>(uX), 0.0f, static_cast<float>(uZ)));
				}
			}

			const u_int uStride = uQUADS + 1u;
			for (u_int uZ = 0; uZ < uQUADS; ++uZ)
			{
				for (u_int uX = 0; uX < uQUADS; ++uX)
				{
					const uint32_t uV0 = uZ * uStride + uX;
					const uint32_t uV1 = uV0 + 1u;
					const uint32_t uV2 = uV0 + uStride;
					const uint32_t uV3 = uV2 + 1u;
					auIndices.PushBack(uV0); auIndices.PushBack(uV2); auIndices.PushBack(uV3);
					auIndices.PushBack(uV0); auIndices.PushBack(uV3); auIndices.PushBack(uV1);
				}
			}
		}
	};
}

ZENITH_TEST(NavMeshComponent, AssetRefRoundTripsWithoutLoading) { Zenith_UnitTests::TestNavMeshComponentAssetRefRoundTrip(); }
void Zenith_UnitTests::TestNavMeshComponentAssetRefRoundTrip()
{
	Zenith_TempScene xScene("TestNavMeshRefScene");

	Zenith_Entity xSourceEntity = xScene.CreateEntity("NavMeshRefSource");
	Zenith_NavMeshComponent& xSource = xSourceEntity.AddComponent<Zenith_NavMeshComponent>();
	xSource.m_strAssetRef = "game:Navmesh/Fixture" ZENITH_NAVMESH_EXT;

	Zenith_DataStream xStream;
	xSource.WriteToDataStream(xStream);
	xStream.SetCursor(0);

	Zenith_Entity xDestEntity = xScene.CreateEntity("NavMeshRefDest");
	Zenith_NavMeshComponent& xDest = xDestEntity.AddComponent<Zenith_NavMeshComponent>();
	xDest.ReadFromDataStream(xStream);

	ZENITH_ASSERT_STREQ(xDest.GetAssetRef().c_str(), xSource.GetAssetRef().c_str(), "The ref is what serializes");

	// Deserialization must NOT load: the load belongs to OnStart, once the whole
	// scene is in memory. (Loading here would also fire the missing-file assert
	// on a fixture path that never existed.)
	ZENITH_ASSERT_FALSE(xDest.IsLoaded(), "ReadFromDataStream must not trigger a load");
	ZENITH_ASSERT_EQ(static_cast<u_int>(xDest.GetLoadState()), static_cast<u_int>(NAVMESH_LOAD_STATE_UNLOADED),
		"A freshly deserialized component is UNLOADED, not FAILED");
}

ZENITH_TEST(NavMeshComponent, SetAssetRefLoadsAndOwns) { Zenith_UnitTests::TestNavMeshComponentSetAssetRefLoadsAndOwns(); }
void Zenith_UnitTests::TestNavMeshComponentSetAssetRefLoadsAndOwns()
{
	NavMeshComponentTestAsset xAsset("zenith_navmesh_component_load.znavmesh");
	ZENITH_ASSERT_TRUE(xAsset.m_bBaked, "The fixture navmesh must bake");

	Zenith_TempScene xScene("TestNavMeshLoadScene");
	Zenith_Entity xEntity = xScene.CreateEntity("NavMeshHolder");
	Zenith_NavMeshComponent& xComponent = xEntity.AddComponent<Zenith_NavMeshComponent>();

	ZENITH_ASSERT_FALSE(xComponent.IsLoaded(), "A fresh component holds nothing");

	// An ABSOLUTE path passes through ResolvePath untouched, so this exercises
	// the same load path an authored `game:` ref takes.
	ZENITH_ASSERT_TRUE(xComponent.SetAssetRef(xAsset.m_strPath), "SetAssetRef must load immediately");
	ZENITH_ASSERT_NOT_NULL(xComponent.GetNavMesh(), "The component owns the loaded mesh");
	ZENITH_ASSERT_EQ(static_cast<u_int>(xComponent.GetLoadState()), static_cast<u_int>(NAVMESH_LOAD_STATE_LOADED),
		"State is LOADED");
	ZENITH_ASSERT_GT(xComponent.GetNavMesh()->GetPolygonCount(), 0u, "The loaded mesh has polygons");
	ZENITH_ASSERT_TRUE(xComponent.GetFailureReason().empty(), "A successful load carries no failure reason");

	// Unload is explicit and complete; the ref survives so Reload can re-run.
	xComponent.Unload();
	ZENITH_ASSERT_NULL(xComponent.GetNavMesh(), "Unload frees the mesh");
	ZENITH_ASSERT_EQ(static_cast<u_int>(xComponent.GetLoadState()), static_cast<u_int>(NAVMESH_LOAD_STATE_UNLOADED),
		"State is UNLOADED after Unload");
	ZENITH_ASSERT_STREQ(xComponent.GetAssetRef().c_str(), xAsset.m_strPath.c_str(), "Unload keeps the ref");

	ZENITH_ASSERT_TRUE(xComponent.Reload(), "Reload re-runs the load path");
	ZENITH_ASSERT_NOT_NULL(xComponent.GetNavMesh(), "Reload restores the mesh");
}

ZENITH_TEST(NavMeshComponent, OnStartLoadsTheAuthoredRef) { Zenith_UnitTests::TestNavMeshComponentOnStartLoads(); }
void Zenith_UnitTests::TestNavMeshComponentOnStartLoads()
{
	// The scene-load path: the ref arrives through deserialization and OnStart
	// -- NOT SetAssetRef -- performs the load.
	NavMeshComponentTestAsset xAsset("zenith_navmesh_component_onstart.znavmesh");
	ZENITH_ASSERT_TRUE(xAsset.m_bBaked, "The fixture navmesh must bake");

	Zenith_TempScene xScene("TestNavMeshOnStartScene");
	Zenith_Entity xEntity = xScene.CreateEntity("NavMeshOnStartHolder");
	Zenith_NavMeshComponent& xComponent = xEntity.AddComponent<Zenith_NavMeshComponent>();

	xComponent.m_strAssetRef = xAsset.m_strPath;
	ZENITH_ASSERT_FALSE(xComponent.IsLoaded(), "Setting the ref field alone loads nothing");

	xComponent.OnStart();

	ZENITH_ASSERT_NOT_NULL(xComponent.GetNavMesh(), "OnStart loads the authored ref");
	ZENITH_ASSERT_EQ(static_cast<u_int>(xComponent.GetLoadState()), static_cast<u_int>(NAVMESH_LOAD_STATE_LOADED),
		"State is LOADED after OnStart");
}

ZENITH_TEST(NavMeshComponent, OnStartWithNoRefIsInert) { Zenith_UnitTests::TestNavMeshComponentOnStartWithNoRefIsInert(); }
void Zenith_UnitTests::TestNavMeshComponentOnStartWithNoRefIsInert()
{
	// An unconfigured component is INERT, not broken -- it must not assert, so a
	// component added from code and configured a frame later is legal.
	Zenith_TempScene xScene("TestNavMeshInertScene");
	Zenith_Entity xEntity = xScene.CreateEntity("NavMeshInertHolder");
	Zenith_NavMeshComponent& xComponent = xEntity.AddComponent<Zenith_NavMeshComponent>();

	xComponent.OnStart();

	ZENITH_ASSERT_NULL(xComponent.GetNavMesh(), "No ref means no mesh");
	ZENITH_ASSERT_EQ(static_cast<u_int>(xComponent.GetLoadState()), static_cast<u_int>(NAVMESH_LOAD_STATE_UNLOADED),
		"No ref is UNLOADED, never FAILED");
}

ZENITH_TEST(NavMeshComponent, BadRefCapturesFailureReason) { Zenith_UnitTests::TestNavMeshComponentBadRefCapturesFailure(); }
void Zenith_UnitTests::TestNavMeshComponentBadRefCapturesFailure()
{
	Zenith_TempScene xScene("TestNavMeshBadRefScene");
	Zenith_Entity xEntity = xScene.CreateEntity("NavMeshBadRefHolder");
	Zenith_NavMeshComponent& xComponent = xEntity.AddComponent<Zenith_NavMeshComponent>();

	{
		// A ref pointing at nothing is a DEFECT (the asset is committed), so the
		// load path asserts by design -- captured here so it cannot take down the
		// boot gate.
		Zenith_AssertCaptureScope xCapture;
		const bool bLoaded = xComponent.SetAssetRef("game:Navmesh/DoesNotExist" ZENITH_NAVMESH_EXT);
		ZENITH_ASSERT_FALSE(bLoaded, "An unresolvable ref must not report success");
		ZENITH_ASSERT_GE(xCapture.GetHitCount(), 1u, "An unresolvable ref must assert");
	}

	ZENITH_ASSERT_NULL(xComponent.GetNavMesh(), "A failed load owns nothing");
	ZENITH_ASSERT_EQ(static_cast<u_int>(xComponent.GetLoadState()), static_cast<u_int>(NAVMESH_LOAD_STATE_FAILED),
		"A bad ref is FAILED, distinct from UNLOADED");
	// The panel renders this string, so an empty one would leave the editor
	// showing FAILED with no explanation.
	ZENITH_ASSERT_FALSE(xComponent.GetFailureReason().empty(), "A FAILED component carries a reason");
}

ZENITH_TEST(NavMeshComponent, MoveRelocationLeavesOneOwner) { Zenith_UnitTests::TestNavMeshComponentMoveRelocation(); }
void Zenith_UnitTests::TestNavMeshComponentMoveRelocation()
{
	// Component pools relocate by MOVE (swap-and-pop / Grow). If both the source
	// and the destination freed the mesh this would be a double free; if neither
	// did, a leak.
	NavMeshComponentTestAsset xAsset("zenith_navmesh_component_move.znavmesh");
	ZENITH_ASSERT_TRUE(xAsset.m_bBaked, "The fixture navmesh must bake");

	Zenith_TempScene xScene("TestNavMeshMoveScene");
	Zenith_Entity xEntity = xScene.CreateEntity("NavMeshMoveHolder");

	{
		Zenith_NavMeshComponent xSource(xEntity);
		ZENITH_ASSERT_TRUE(xSource.SetAssetRef(xAsset.m_strPath), "Fixture must load");
		const Zenith_NavMesh* pxOriginal = xSource.GetNavMesh();

		Zenith_NavMeshComponent xMoved(std::move(xSource));

		ZENITH_ASSERT_TRUE(xSource.m_bMovedOut, "The move source is marked moved-out");
		ZENITH_ASSERT_NULL(xSource.GetNavMesh(), "The move source no longer points at the mesh");
		ZENITH_ASSERT_EQ(static_cast<u_int>(xSource.GetLoadState()), static_cast<u_int>(NAVMESH_LOAD_STATE_UNLOADED),
			"The move source reports UNLOADED");
		ZENITH_ASSERT_TRUE(xMoved.GetNavMesh() == pxOriginal, "The destination owns the SAME mesh");
		ZENITH_ASSERT_STREQ(xMoved.GetAssetRef().c_str(), xAsset.m_strPath.c_str(), "The ref moves with the component");

		// Move-ASSIGN over a component that already owns a mesh must release the
		// old one rather than leak it.
		Zenith_NavMeshComponent xTarget(xEntity);
		ZENITH_ASSERT_TRUE(xTarget.SetAssetRef(xAsset.m_strPath), "Target loads its own mesh first");
		xTarget = std::move(xMoved);
		ZENITH_ASSERT_TRUE(xTarget.GetNavMesh() == pxOriginal, "Move-assign transfers ownership");
		ZENITH_ASSERT_NULL(xMoved.GetNavMesh(), "The assigned-from component is neutralised");
		ZENITH_ASSERT_TRUE(xMoved.m_bMovedOut, "The assigned-from component is marked moved-out");
	}
	// Leaving the scope destroys all three; exactly one delete must have run per
	// mesh. A double free would fault here.
}

ZENITH_TEST(NavMeshComponent, SceneUnloadFreesTheMesh) { Zenith_UnitTests::TestNavMeshComponentSceneUnloadFrees(); }
void Zenith_UnitTests::TestNavMeshComponentSceneUnloadFrees()
{
	// Lifetime rides the SCENE (there is no cache to invalidate and no hook to
	// remember): unloading the scene must run OnDestroy / the destructor and free
	// the mesh.
	NavMeshComponentTestAsset xAsset("zenith_navmesh_component_scene.znavmesh");
	ZENITH_ASSERT_TRUE(xAsset.m_bBaked, "The fixture navmesh must bake");

	// OnDestroy is idempotent with the destructor -- both check the same pointer,
	// so whichever order the scene tears down in cannot double-free.
	{
		Zenith_TempScene xScene("TestNavMeshOnDestroyIdempotent");
		Zenith_Entity xEntity = xScene.CreateEntity("NavMeshDestroyHolder");
		Zenith_NavMeshComponent& xComponent = xEntity.AddComponent<Zenith_NavMeshComponent>();
		ZENITH_ASSERT_TRUE(xComponent.SetAssetRef(xAsset.m_strPath), "Fixture must load");

		xComponent.OnDestroy();
		ZENITH_ASSERT_NULL(xComponent.GetNavMesh(), "OnDestroy frees the mesh");
		xComponent.OnDestroy();
		ZENITH_ASSERT_NULL(xComponent.GetNavMesh(), "OnDestroy is idempotent");
	}

	// Lifetime rides the SCENE. This half deliberately does NOT pre-destroy the
	// component: the scene's forced unload must be what takes it away, which is
	// the property that lets scene switching free navmeshes with no cache and no
	// teardown hook.
	const u_int uBaseline = CountLoadedNavMeshComponents();
	{
		Zenith_TempScene xScene("TestNavMeshSceneLifetime");
		Zenith_Entity xEntity = xScene.CreateEntity("NavMeshSceneHolder");
		Zenith_NavMeshComponent& xComponent = xEntity.AddComponent<Zenith_NavMeshComponent>();
		ZENITH_ASSERT_TRUE(xComponent.SetAssetRef(xAsset.m_strPath), "Fixture must load");
		ZENITH_ASSERT_NOT_NULL(xComponent.GetNavMesh(), "Loaded before teardown");
		ZENITH_ASSERT_EQ(CountLoadedNavMeshComponents(), uBaseline + 1u,
			"The live component is visible to an ECS query");
	}

	ZENITH_ASSERT_EQ(CountLoadedNavMeshComponents(), uBaseline,
		"Unloading the scene took the component with it");
}

#endif // ZENITH_TESTING
