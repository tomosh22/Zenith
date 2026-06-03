#include "Zenith.h"

// =============================================================================
// SentinelECS — leaf-proof executable for the ZenithECS extraction (Phase 9b).
//
// This exe links ONLY zenithecs.lib + zenithbase.lib (see
// Build/Sharpmake_SentinelECS.cs). Its single job is to drive the ECS core
// end-to-end WITHOUT the engine: no g_xEngine, no Flux, no Physics, no concrete
// component. If it links and runs, the ECS core has no undefined engine
// externals — i.e. ZenithECS really is a self-contained leaf on top of
// ZenithBase. A new engine-coupling leak would surface here as an unresolved
// external at link time (caught by the orchestrator's build gate).
//
// Bootstrap mirrors the real Zenith_Engine::Initialise order, minus the
// engine-side steps the leaf does not need:
//   1. construct Zenith_SceneSystem  -> publishes Get() singleton + allocates
//                                        the process-wide Zenith_EntityStore
//   2. SetComponentRegistrar + EnsureInitialized -> drain registrar + seal the
//                                        component-meta registry
//   3. InitialiseSubsystems          -> create the persistent scene
//   4. CreateEmptyScene + SetActiveScene -> a real, engine-free scene to build into
//   5. CreateEntityBare              -> bare entity (NO Transform — that is the
//                                        engine-side default-components hook,
//                                        which we deliberately never install)
//   6. AddComponent<SentinelComponent>
//   7. Query<SentinelComponent>      -> assert exactly one match
//   8. DestroyImmediate              -> assert zero matches
//   9. ShutdownSubsystems + delete   -> clean teardown
//
// NOTE: the engine installs runtime hooks (SetRuntimeHooks) after step 3; we
// SKIP that. With no hooks installed the leaf's documented null-semantics make
// Zenith_ECS_IsMainThread() return true (so the main-thread asserts pass on this
// single-threaded driver) and m_pfnAddDefaultComponents stays null (so
// CreateEntity would add no components — we use CreateEntityBare regardless).
// =============================================================================

#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_ComponentMeta.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_Query.h"
// The component-meta serialize/deserialize wrappers (instantiated by
// RegisterComponent<SentinelComponent>) call SentinelComponent::WriteToDataStream /
// ReadFromDataStream, which use Zenith_DataStream's templated operator<< / >>.
// The ECS headers only forward-declare Zenith_DataStream, so pull the full
// definition in here. DataStream lives in the L0 ZenithBase lib (already linked).
#include "DataStream/Zenith_DataStream.h"

//==============================================================================
// SentinelComponent — the minimal type that satisfies the component-meta
// contract (Zenith_ComponentMetaRegistry::RegisterComponent<T> + the
// Zenith_Component concept):
//   * constructible from Zenith_Entity&   (concept + ComponentCreateWrapper)
//   * destructible                         (concept)
//   * move-constructible                   (the dense pool move-constructs on
//                                           Grow / swap-and-pop)
//   * WriteToDataStream / ReadFromDataStream (serialize/deserialize wrappers
//                                           instantiated by RegisterComponent<T>)
//   * RenderPropertiesPanel()              (concept, ZENITH_TOOLS builds only)
//
// It references NO engine / Flux / Physics / concrete-component type. The single
// data member exists only to give the serialize round-trip something to carry
// and to prove the instance is real.
//==============================================================================
class SentinelComponent
{
public:
	explicit SentinelComponent(Zenith_Entity& xParent)
		: m_xParentEntity(xParent)
	{
	}

	~SentinelComponent() = default;

	// Move-only-friendly: the component pool move-constructs live elements on
	// Grow() and swap-and-pop removal, so the move ctor must exist. Copy is
	// neither needed nor used by the pool; delete it to match engine components.
	SentinelComponent(SentinelComponent&& xOther) noexcept
		: m_xParentEntity(xOther.m_xParentEntity)
		, m_uValue(xOther.m_uValue)
	{
	}

	SentinelComponent& operator=(SentinelComponent&& xOther) noexcept
	{
		if (this != &xOther)
		{
			m_xParentEntity = xOther.m_xParentEntity;
			m_uValue = xOther.m_uValue;
		}
		return *this;
	}

	SentinelComponent(const SentinelComponent&) = delete;
	SentinelComponent& operator=(const SentinelComponent&) = delete;

	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		xStream << m_uValue;
	}

	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		xStream >> m_uValue;
	}

	u_int GetValue() const { return m_uValue; }
	void SetValue(u_int uValue) { m_uValue = uValue; }

#ifdef ZENITH_TOOLS
	// Required by the Zenith_Component concept in ZENITH_TOOLS builds. The
	// sentinel is headless and never opens an editor panel, so this is an
	// intentional no-op — its only job is to satisfy the concept so the True
	// configs compile.
	void RenderPropertiesPanel() {}
#endif

private:
	Zenith_Entity m_xParentEntity;
	u_int m_uValue = 0;
};

//==============================================================================
// Registrar drained by Zenith_ComponentMetaRegistry::EnsureInitialized(). The
// engine installs Zenith_RegisterEngineComponents here; the sentinel installs
// this leaf-only registrar instead, so the registry knows exactly one type.
//==============================================================================
static void RegisterSentinelComponents()
{
	Zenith_ComponentMetaRegistry::Get().RegisterComponent<SentinelComponent>("SentinelComponent", 1000u);
}

//==============================================================================
// main — returns 0 on success, non-zero on the first failed expectation.
//==============================================================================

namespace
{
	int s_iFailures = 0;

	void Expect(bool bCondition, const char* szWhat)
	{
		if (!bCondition)
		{
			++s_iFailures;
			Zenith_Error(LOG_CATEGORY_UNITTEST, "SentinelECS FAIL: %s", szWhat);
		}
		else
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "SentinelECS ok:   %s", szWhat);
		}
	}
}

int main()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "SentinelECS: ECS leaf-proof starting (zenithecs.lib + zenithbase.lib only)");

	// 1. Bring up the SceneSystem. The ctor publishes the Get() singleton and
	//    allocates the process-wide Zenith_EntityStore — everything below reaches
	//    the store + the main-thread predicate through Get().
	Zenith_SceneSystem* pxScenes = new Zenith_SceneSystem();
	Expect(pxScenes != nullptr, "Zenith_SceneSystem constructed");
	Expect(&Zenith_SceneSystem::Get() == pxScenes, "Zenith_SceneSystem::Get() resolves to the live instance");

	// 2. Install the leaf-only registrar and force the one-time drain + seal.
	//    With null runtime hooks Zenith_ECS_IsMainThread() returns true, so the
	//    registry's asserts pass on this single-threaded driver.
	Zenith_ComponentMetaRegistry::Get().SetComponentRegistrar(&RegisterSentinelComponents);
	Zenith_ComponentMetaRegistry::Get().EnsureInitialized();
	Expect(Zenith_ComponentMetaRegistry::Get().IsInitialized(), "component-meta registry initialised");
	Expect(Zenith_ComponentMetaRegistry::Get().GetMetaByName("SentinelComponent") != nullptr,
		"SentinelComponent registered in the meta registry");

	// 3. Create the persistent ("DontDestroyOnLoad") scene (engine-free bootstrap).
	Zenith_SceneSystem::InitialiseSubsystems();

	// 4. Create a real scene and make it active (engine-free path). The bare
	//    empty-scene entry point is LoadScene(name, ADDITIVE_WITHOUT_LOADING) — it
	//    allocates an empty scene with no file read and auto-activates when no
	//    active scene exists yet, but assert it to be sure.
	Zenith_Scene xScene = Zenith_SceneSystem::Get().LoadScene("SentinelScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Expect(xScene.IsValid(), "LoadScene(ADDITIVE_WITHOUT_LOADING) returned a valid scene");
	Expect(Zenith_SceneSystem::Get().GetActiveScene() == xScene, "SentinelScene is the active scene");

	// 5. Create a BARE entity (no default components — the Transform is engine-side
	//    and we never installed the default-components hook, so a bare entity has
	//    exactly the components we add explicitly).
	Zenith_Entity xEntity = Zenith_SceneSystem::Get().CreateEntityBare(xScene, "Sentinel");
	Expect(xEntity.IsValid(), "CreateEntityBare returned a valid entity");
	Expect(!xEntity.HasComponent<SentinelComponent>(), "bare entity has no SentinelComponent yet");

	// 6. Add the stub component and round-trip a value through it.
	SentinelComponent& xComp = xEntity.AddComponent<SentinelComponent>();
	xComp.SetValue(0x5E2417u);
	Expect(xEntity.HasComponent<SentinelComponent>(), "AddComponent<SentinelComponent> succeeded");
	Expect(xEntity.GetComponent<SentinelComponent>().GetValue() == 0x5E2417u,
		"component value reads back through GetComponent");

	// 7. Query<SentinelComponent> must see exactly one entity, and the callback
	//    must receive the same component instance carrying the same value.
	Zenith_SceneData* pxSceneData = Zenith_SceneSystem::Get().GetSceneData(xScene);
	Expect(pxSceneData != nullptr, "scene data resolved for query");
	if (pxSceneData != nullptr)
	{
		u_int uCount = pxSceneData->Query<SentinelComponent>().Count();
		Expect(uCount == 1u, "Query<SentinelComponent>().Count() == 1");

		u_int uForEachHits = 0;
		u_int uSeenValue = 0;
		pxSceneData->Query<SentinelComponent>().ForEach(
			[&uForEachHits, &uSeenValue](Zenith_EntityID, SentinelComponent& xC)
			{
				++uForEachHits;
				uSeenValue = xC.GetValue();
			});
		Expect(uForEachHits == 1u, "Query<SentinelComponent>().ForEach visited exactly one entity");
		Expect(uSeenValue == 0x5E2417u, "ForEach saw the component's value");
	}

	// 8. Destroy the entity and confirm the query now sees nothing.
	xEntity.DestroyImmediate();
	Expect(!xEntity.IsValid(), "entity handle invalid after DestroyImmediate");
	if (pxSceneData != nullptr)
	{
		Expect(pxSceneData->Query<SentinelComponent>().Count() == 0u,
			"Query<SentinelComponent>().Count() == 0 after destroy");
	}

	// 9. Clean teardown: tear down the scene system, drain the entity store, then
	//    free the SceneSystem (mirrors Zenith_Engine::Shutdown's delete m_pxScenes).
	Zenith_SceneSystem::ShutdownSubsystems();
	delete pxScenes;
	pxScenes = nullptr;

	if (s_iFailures == 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "SentinelECS: PASS (ECS leaf links + runs with no engine externals)");
		return 0;
	}

	Zenith_Error(LOG_CATEGORY_UNITTEST, "SentinelECS: FAIL (%d expectation(s) failed)", s_iFailures);
	return 1;
}
