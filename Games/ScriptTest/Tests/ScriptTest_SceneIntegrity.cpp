#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

// ============================================================================
// ScriptTest_SceneIntegrity -- WP-2b. The two scene-integrity tests, C5 and C6.
//
// ScriptTest carries zero gameplay C++, so a scene that lost an entity or a
// graph attachment produces a game that boots cleanly and does nothing. Neither
// half of that is visible to any other test:
//
//   C5  ST_SceneAssetIntegrity  asserts on the eight .zscen FILES -- the bytes
//                               ON DISK, which in a tools (*_True) build are the
//                               ones THIS BOOT just authored, not the committed
//                               ones. Whichever backend wrote them, they still
//                               name every entity and every attached .bgraph,
//                               and carry no prefab-scratch leakage. It never
//                               loads a scene -- so it reddens in the
//                               configuration that DAMAGED the asset, which is
//                               the whole point of asserting on the file (the
//                               RenderTest RT_SceneAssetIntegrity shape).
//
//                               ★ IT DOES NOT COMPARE AGAINST THE COMMITTED
//                               BYTES, AND CANNOT. The gated config re-authors
//                               all seven before any test runs, so a recipe that
//                               drifted away from what is checked in produces a
//                               file this test happily accepts. The cold-bake
//                               step in .github/workflows/st-tests.yml is what
//                               closes that: it DELETES the committed scenes,
//                               re-bakes them and diffs against git. C5 answers
//                               "is this file a real scene"; the workflow
//                               answers "is it the committed one".
//
//   C6  ST_AllScenesBoot        asserts on the eight scenes LOADED. Every build
//                               index 0..iCOUNT-1 is loaded in order and, once
//                               live, its key entities must resolve by name and
//                               every Zenith_GraphComponent slot in it must
//                               carry a resolved graph with zero unresolved
//                               nodes.
//
// The two are deliberately NOT redundant. A slot whose .bgraph cannot be loaded
// keeps its path + override bytes verbatim (the unresolved-slot contract in
// Zenith/EntityComponent/CLAUDE.md), so the PATH survives into the file and out
// of it again while the graph itself is null -- C5 would stay green on a scene
// whose every graph is dead. Conversely a scene the boot re-authored short of an
// entity loads perfectly well; only the expectation table below remembers what
// was meant to be there.
//
// SINGLE-SPELLING. Every path, index and name below comes from
// ScriptTest_Graphs.h. Nothing here restates a literal that header owns -- a
// test that restated one would prove only that the test agrees with itself.
// The one derived string is the graph-path NEEDLE (see ST_GraphNeedle).
//
// Headless-safe: neither test reads a pixel, so both leave m_bRequiresGraphics
// at its false default and both run in the Null gate.
// ============================================================================

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "FileAccess/Zenith_FileAccess.h"

#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_Query.h"
#include "EntityComponent/Components/Zenith_GraphComponent.h"

#include "ScriptTest/ScriptTest_Graphs.h"

#include <cstring>

namespace
{
	//==========================================================================
	// The expectation table -- ONE description of the eight scenes, shared by
	// both tests.
	//
	// Both lists are nullptr-TERMINATED rather than paired with a count: a count
	// is a second place to be wrong, and the graph list's length doubles as the
	// expected GRAPH SLOT COUNT (below), so it must not be able to disagree with
	// itself.
	//
	// The graph list is the list of ATTACHES, not of distinct assets -- which is
	// why ST_BellListener appears three times for Gym_Events. C5 searches the
	// same needle three times (harmless); C6 needs the length to be the number of
	// serialized slots, and three entities sharing one asset is three slots.
	//==========================================================================

	struct ST_SceneExpectation
	{
		int32_t            m_iBuildIndex;
		const char*        m_szPath;
		const char* const* m_apszEntities;	// nullptr-terminated
		const char* const* m_apszGraphs;	// nullptr-terminated, ATTACH order
	};

	const char* const g_apszHUB_ENTITIES[] =
	{
		ScriptTest::Entities::szGAME_MANAGER,
		ScriptTest::Entities::szSUN,
		nullptr
	};
	const char* const g_apszHUB_GRAPHS[] =
	{
		ScriptTest::Graphs::szHUB_FLOW,
		nullptr
	};

	const char* const g_apszGYM_MOTION_ENTITIES[] =
	{
		ScriptTest::Entities::szGAME_MANAGER,
		ScriptTest::Entities::szSUN,
		ScriptTest::Entities::szKEY_LIGHT,
		ScriptTest::Entities::szFLOOR,
		ScriptTest::Entities::szSPINNER,
		ScriptTest::Entities::szPING_PONG,
		ScriptTest::Entities::szBOBBER,
		nullptr
	};
	const char* const g_apszGYM_MOTION_GRAPHS[] =
	{
		ScriptTest::Graphs::szESC_TO_HUB,
		ScriptTest::Graphs::szSPIN,
		ScriptTest::Graphs::szPING_PONG,
		ScriptTest::Graphs::szSINE_BOB,
		nullptr
	};

	const char* const g_apszGYM_INPUT_ENTITIES[] =
	{
		ScriptTest::Entities::szGAME_MANAGER,
		ScriptTest::Entities::szSUN,
		ScriptTest::Entities::szKEY_LIGHT,
		ScriptTest::Entities::szFLOOR,
		ScriptTest::Entities::szPLAYER_CUBE,
		nullptr
	};
	const char* const g_apszGYM_INPUT_GRAPHS[] =
	{
		ScriptTest::Graphs::szESC_TO_HUB,
		ScriptTest::Graphs::szPLAYER_MOVE,
		ScriptTest::Graphs::szJUMP,
		nullptr
	};

	const char* const g_apszGYM_PHYSICS_ENTITIES[] =
	{
		ScriptTest::Entities::szGAME_MANAGER,
		ScriptTest::Entities::szSUN,
		ScriptTest::Entities::szKEY_LIGHT,
		ScriptTest::Entities::szPLATFORM,
		ScriptTest::Entities::szSPAWNER,
		ScriptTest::Entities::szKILL_VOLUME,
		nullptr
	};
	const char* const g_apszGYM_PHYSICS_GRAPHS[] =
	{
		ScriptTest::Graphs::szESC_TO_HUB,
		ScriptTest::Graphs::szBALL_SPAWNER,
		ScriptTest::Graphs::szKILL_VOLUME,
		nullptr
	};

	const char* const g_apszGYM_EVENTS_ENTITIES[] =
	{
		ScriptTest::Entities::szGAME_MANAGER,
		ScriptTest::Entities::szSUN,
		ScriptTest::Entities::szKEY_LIGHT,
		ScriptTest::Entities::szFLOOR,
		ScriptTest::Entities::szPLAYER_CUBE,
		ScriptTest::Entities::szPRESSURE_PLATE,
		ScriptTest::Entities::szGYM_DOOR,
		ScriptTest::Entities::szBELL_LISTENER_A,
		ScriptTest::Entities::szBELL_LISTENER_B,
		ScriptTest::Entities::szBELL_LISTENER_C,
		nullptr
	};
	const char* const g_apszGYM_EVENTS_GRAPHS[] =
	{
		ScriptTest::Graphs::szESC_TO_HUB,		// GameManager slot 0
		ScriptTest::Graphs::szBELL_RING,		// GameManager slot 1
		ScriptTest::Graphs::szPLAYER_MOVE,		// PlayerCube  slot 0
		ScriptTest::Graphs::szJUMP,				// PlayerCube  slot 1
		ScriptTest::Graphs::szPRESSURE_PLATE,
		ScriptTest::Graphs::szDOOR,
		ScriptTest::Graphs::szBELL_LISTENER,	// one asset, three entities
		ScriptTest::Graphs::szBELL_LISTENER,
		ScriptTest::Graphs::szBELL_LISTENER,
		nullptr
	};

	const char* const g_apszGYM_STATE_ENTITIES[] =
	{
		ScriptTest::Entities::szGAME_MANAGER,
		ScriptTest::Entities::szSUN,
		ScriptTest::Entities::szKEY_LIGHT,
		ScriptTest::Entities::szFLOOR,
		ScriptTest::Entities::szLAMP_RED,
		ScriptTest::Entities::szLAMP_AMBER,
		ScriptTest::Entities::szLAMP_GREEN,
		nullptr
	};
	// The three lamps carry NO graph: the StateMachine on the GameManager finds
	// them by name. Two slots for the whole scene is the assertion that keeps
	// that true.
	const char* const g_apszGYM_STATE_GRAPHS[] =
	{
		ScriptTest::Graphs::szESC_TO_HUB,
		ScriptTest::Graphs::szTRAFFIC_LIGHT,
		nullptr
	};

	const char* const g_apszGYM_UI_ENTITIES[] =
	{
		ScriptTest::Entities::szGAME_MANAGER,
		ScriptTest::Entities::szSUN,
		nullptr
	};
	const char* const g_apszGYM_UI_GRAPHS[] =
	{
		ScriptTest::Graphs::szESC_TO_HUB,
		ScriptTest::Graphs::szUI_PLAYGROUND,
		nullptr
	};

	const char* const g_apszGYM_FLOW_ENTITIES[] =
	{
		ScriptTest::Entities::szGAME_MANAGER,
		ScriptTest::Entities::szSUN,
		ScriptTest::Entities::szKEY_LIGHT,
		ScriptTest::Entities::szFLOOR,
		ScriptTest::Entities::szNOZZLE,
		ScriptTest::Entities::szPLATE,
		nullptr
	};
	// The Nozzle carries NO graph: ST_Dispenser's SwitchOnInt pins find it by
	// name and scale it. Three slots for the whole scene is what keeps that true.
	const char* const g_apszGYM_FLOW_GRAPHS[] =
	{
		ScriptTest::Graphs::szESC_TO_HUB,	// GameManager slot 0
		ScriptTest::Graphs::szDISPENSER,	// GameManager slot 1
		ScriptTest::Graphs::szFLOW_PLATE,	// Plate
		nullptr
	};
	// ST_FlowScore is deliberately absent: it is never ATTACHED to anything.
	// CallGraph resolves it by asset path from inside ST_Dispenser, so it is the
	// one graph this game authors that no scene slot references -- which is also
	// why C5/C6 cannot see it and ST_FlowGym_Test's `score` assertion must.

	// Build-index order. C6 walks this array in order and reads m_iBuildIndex
	// from the row rather than from its own loop counter, so the two can be
	// checked against each other rather than assumed equal.
	const ST_SceneExpectation g_axSCENES[] =
	{
		{ ScriptTest::Scenes::iHUB,         ScriptTest::Scenes::szHUB_PATH,         g_apszHUB_ENTITIES,         g_apszHUB_GRAPHS         },
		{ ScriptTest::Scenes::iGYM_MOTION,  ScriptTest::Scenes::szGYM_MOTION_PATH,  g_apszGYM_MOTION_ENTITIES,  g_apszGYM_MOTION_GRAPHS  },
		{ ScriptTest::Scenes::iGYM_INPUT,   ScriptTest::Scenes::szGYM_INPUT_PATH,   g_apszGYM_INPUT_ENTITIES,   g_apszGYM_INPUT_GRAPHS   },
		{ ScriptTest::Scenes::iGYM_PHYSICS, ScriptTest::Scenes::szGYM_PHYSICS_PATH, g_apszGYM_PHYSICS_ENTITIES, g_apszGYM_PHYSICS_GRAPHS },
		{ ScriptTest::Scenes::iGYM_EVENTS,  ScriptTest::Scenes::szGYM_EVENTS_PATH,  g_apszGYM_EVENTS_ENTITIES,  g_apszGYM_EVENTS_GRAPHS  },
		{ ScriptTest::Scenes::iGYM_STATE,   ScriptTest::Scenes::szGYM_STATE_PATH,   g_apszGYM_STATE_ENTITIES,   g_apszGYM_STATE_GRAPHS   },
		{ ScriptTest::Scenes::iGYM_UI,      ScriptTest::Scenes::szGYM_UI_PATH,      g_apszGYM_UI_ENTITIES,      g_apszGYM_UI_GRAPHS      },
		{ ScriptTest::Scenes::iGYM_FLOW,    ScriptTest::Scenes::szGYM_FLOW_PATH,    g_apszGYM_FLOW_ENTITIES,    g_apszGYM_FLOW_GRAPHS    },
	};

	constexpr u_int uST_SCENE_ROWS = static_cast<u_int>(sizeof(g_axSCENES) / sizeof(g_axSCENES[0]));

	static_assert(uST_SCENE_ROWS == static_cast<u_int>(ScriptTest::Scenes::iCOUNT),
		"the expectation table must describe EVERY registered scene -- a scene added to "
		"ScriptTest_Graphs.h without a row here would be silently untested");

	u_int ST_CountList(const char* const* ppszList)
	{
		u_int uCount = 0;
		while (ppszList[uCount] != nullptr)
		{
			++uCount;
		}
		return uCount;
	}

	//==========================================================================
	// ST_GraphNeedle -- the substring of a graph asset path that is present in
	// BOTH spellings the slot could carry.
	//
	// AddStep_AttachGraph is handed the registry path ("game:Graphs/ST_Spin
	// .bgraph") and Zenith_GraphComponent::AddGraphByAssetPath stores whatever
	// NormalizeAssetPath returns -- which is the input verbatim for an
	// already-prefixed path, and the "game:"-prefixed form for an absolute one.
	// So the prefix is the only part that can legally vary; everything after it
	// is in the bytes either way. Dropping it is DERIVED from the header
	// constant rather than being a second spelling of it.
	//
	// The prefix is spelled here and nowhere else. Zenith_AssetRegistry exports
	// no constant for it (NormalizeAssetPath / MakeRelativePath spell it inline
	// too), so this is the fewest spellings available.
	//==========================================================================
	const char* ST_GraphNeedle(const char* szRegistryPath)
	{
		constexpr char szGAME_PREFIX[] = "game:";
		constexpr size_t ulPrefixLength = sizeof(szGAME_PREFIX) - 1;
		return (strncmp(szRegistryPath, szGAME_PREFIX, ulPrefixLength) == 0)
			? szRegistryPath + ulPrefixLength
			: szRegistryPath;
	}
}

// ============================================================================
// C5 -- ST_SceneAssetIntegrity
//
// Pure file inspection: reads each .zscen on disk as bytes and asserts on the
// strings the serializer writes. Reading NAMES out of the raw bytes deliberately
// avoids depending on which entities THIS run happens to have loaded -- a scene
// missing an entity cannot contain its name. (In a tools build those bytes are
// this boot's own authoring output; matching the COMMITTED bytes is the CI
// cold-bake step's job, not this test's -- see the file header.)
// ============================================================================

namespace
{
	// Every committed scene is >= 1.6 KB today. The floor only has to separate a
	// real scene from a header-only stub (which is what a failed authoring pass
	// would publish), so it sits well below the smallest real file rather than
	// tracking it.
	constexpr uint64_t ulST_MIN_SCENE_BYTES = 256;

	u_int g_uIntegrityFailures = 0;
	u_int g_uIntegrityScenesRead = 0;

	bool ST_BytesContain(const char* pcData, uint64_t ulSize, const char* szNeedle)
	{
		const uint64_t ulNeedle = static_cast<uint64_t>(strlen(szNeedle));
		if (ulNeedle == 0 || ulSize < ulNeedle)
		{
			return false;
		}
		const uint64_t ulLast = ulSize - ulNeedle;
		for (uint64_t ul = 0; ul <= ulLast; ++ul)
		{
			if (memcmp(pcData + ul, szNeedle, static_cast<size_t>(ulNeedle)) == 0)
			{
				return true;
			}
		}
		return false;
	}

	void ST_FailIntegrity(const char* szScenePath, const char* szWhat, const char* szSubject)
	{
		++g_uIntegrityFailures;
		Zenith_Error(LOG_CATEGORY_UNITTEST, "[ST_SceneAssetIntegrity] %s: %s '%s'",
			szScenePath, szWhat, szSubject);
	}

	// Every clause runs and every failure is reported, so a break says WHICH
	// scene lost WHICH string rather than only that something moved.
	void ST_InspectSceneFile(const ST_SceneExpectation& xRow)
	{
		if (!Zenith_FileAccess::FileExists(xRow.m_szPath))
		{
			++g_uIntegrityFailures;
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ST_SceneAssetIntegrity] %s: the scene asset is not on disk at all "
				"(build index %d)", xRow.m_szPath, xRow.m_iBuildIndex);
			return;
		}

		uint64_t ulSize = 0;
		char* pcData = Zenith_FileAccess::ReadFile(xRow.m_szPath, ulSize);
		if (pcData == nullptr)
		{
			++g_uIntegrityFailures;
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ST_SceneAssetIntegrity] %s: exists but could not be read", xRow.m_szPath);
			return;
		}

		if (ulSize < ulST_MIN_SCENE_BYTES)
		{
			++g_uIntegrityFailures;
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ST_SceneAssetIntegrity] %s: only %llu bytes -- a boot published a stub "
				"over the committed scene", xRow.m_szPath, static_cast<unsigned long long>(ulSize));
			Zenith_FileAccess::FreeFileData(pcData);
			return;
		}

		// (ii) every entity the recipe authors is named in the bytes.
		for (const char* const* ppszName = xRow.m_apszEntities; *ppszName != nullptr; ++ppszName)
		{
			if (!ST_BytesContain(pcData, ulSize, *ppszName))
			{
				ST_FailIntegrity(xRow.m_szPath, "the asset carries no entity named", *ppszName);
			}
		}

		// (iii) every attached graph's asset path is in the bytes. A slot that
		// failed to resolve still round-trips its path, so this proves the
		// ATTACH survived -- not that the graph is alive. C6 owns that half.
		for (const char* const* ppszGraph = xRow.m_apszGraphs; *ppszGraph != nullptr; ++ppszGraph)
		{
			const char* szNeedle = ST_GraphNeedle(*ppszGraph);
			if (!ST_BytesContain(pcData, ulSize, szNeedle))
			{
				ST_FailIntegrity(xRow.m_szPath, "the asset carries no graph slot for", szNeedle);
			}
		}

		// (iv) the escape hatch, asserted INDEPENDENTLY of the table above. Every
		// gym has to be leaveable; the hub is the one scene that legitimately has
		// no ST_EscToHub, because it IS the hub. Kept as its own clause so a table
		// row that lost the row still reddens here.
		if (xRow.m_iBuildIndex != ScriptTest::Scenes::iHUB)
		{
			const char* szEscNeedle = ST_GraphNeedle(ScriptTest::Graphs::szESC_TO_HUB);
			if (!ST_BytesContain(pcData, ulSize, szEscNeedle))
			{
				ST_FailIntegrity(xRow.m_szPath,
					"a gym scene with no way back to the hub -- missing", szEscNeedle);
			}
		}

		// (v) the prefab scratch entity must NEVER reach a saved scene. It exists
		// only so CreatePrefabFromSelected has a live entity to capture, in a
		// scratch scene that is unloaded immediately afterwards; finding it here
		// means an authoring step captured the prefab into a tracked asset.
		if (ST_BytesContain(pcData, ulSize, ScriptTest::Entities::szBALL_TEMPLATE))
		{
			ST_FailIntegrity(xRow.m_szPath,
				"the prefab scratch entity was serialized into a committed scene",
				ScriptTest::Entities::szBALL_TEMPLATE);
		}

		++g_uIntegrityScenesRead;
		Zenith_FileAccess::FreeFileData(pcData);
	}

	void Setup_SceneAssetIntegrity()
	{
		g_uIntegrityFailures = 0;
		g_uIntegrityScenesRead = 0;
	}

	bool Step_SceneAssetIntegrity(int /*iFrame*/)
	{
		return false;	// pure file inspection -- nothing to simulate
	}

	bool Verify_SceneAssetIntegrity()
	{
		for (u_int uRow = 0; uRow < uST_SCENE_ROWS; ++uRow)
		{
			ST_InspectSceneFile(g_axSCENES[uRow]);
		}

		if (g_uIntegrityFailures != 0)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ST_SceneAssetIntegrity] %u failure(s); %u of %u committed scene files were "
				"read in full", g_uIntegrityFailures, g_uIntegrityScenesRead, uST_SCENE_ROWS);
			return false;
		}

		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ST_SceneAssetIntegrity] all %u committed scenes intact", uST_SCENE_ROWS);
		return true;
	}
}

static const Zenith_AutomatedTest g_xScriptTestSceneAssetIntegrity = {
	"ST_SceneAssetIntegrity",
	&Setup_SceneAssetIntegrity,
	&Step_SceneAssetIntegrity,
	&Verify_SceneAssetIntegrity,
	/*maxFrames*/ 1	// the files are already on disk; this only reads them
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xScriptTestSceneAssetIntegrity);

// ============================================================================
// C6 -- ST_AllScenesBoot
//
// Loads every build index in order and asserts the loaded world. Two clauses per
// scene:
//
//   (i)  the key entities resolve by name;
//   (ii) every Zenith_GraphComponent slot in the scene holds a resolved graph
//        with zero unresolved nodes, and the scene holds EXACTLY as many slots
//        as the recipe attaches.
//
// The slot COUNT is what stops (ii) being vacuous. "Every slot resolved" is
// trivially true of a scene whose graph components all vanished, which is
// precisely the regression a zero-gameplay-C++ game cannot otherwise see.
// ============================================================================

namespace
{
	// Loads are synchronous unless issued mid-update / mid-load, in which case
	// they are stashed and drained as the outer pass returns -- so the wait is a
	// poll, not an assumption. 120 frames is ~2 s at fixed 60 Hz and is a
	// harness-fault budget, not a timing one.
	constexpr int iST_LOAD_WAIT_FRAMES = 120;

	// Slots instantiate during ReadFromDataStream, so the checks below are valid
	// the instant the scene is LOADED. The settle is for the OnStart pass Unity
	// semantics defer to the next frame, so a failure can never be "we looked one
	// frame early".
	constexpr int iST_SETTLE_FRAMES = 4;

	enum class ST_BootPhase { Load, Wait, Settle, Check, Done };

	ST_BootPhase g_eBootPhase = ST_BootPhase::Load;
	u_int        g_uBootRow = 0;
	int          g_iBootPhaseFrame = 0;
	u_int        g_uBootFailures = 0;
	u_int        g_uBootScenesChecked = 0;
	bool         g_bBootAborted = false;
	Zenith_Scene g_xBootSceneBeforeLoad;

	void ST_CheckLoadedScene(const ST_SceneExpectation& xRow)
	{
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetActiveSceneData();
		if (pxSceneData == nullptr)
		{
			++g_uBootFailures;
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ST_AllScenesBoot] %s: no active scene data after the load reported LOADED",
				xRow.m_szPath);
			return;
		}

		// (i) the key entities.
		for (const char* const* ppszName = xRow.m_apszEntities; *ppszName != nullptr; ++ppszName)
		{
			if (!pxSceneData->FindEntityByName(*ppszName).IsValid())
			{
				++g_uBootFailures;
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ST_AllScenesBoot] %s: no entity named '%s' in the loaded scene",
					xRow.m_szPath, *ppszName);
			}
		}

		// (ii) the graph slots. Every slot is inspected and every fault reported,
		// so a break names the entity, the slot index and its asset path.
		u_int uSlotsSeen = 0;
		g_xEngine.Scenes().QueryActiveScene<Zenith_GraphComponent>().ForEach(
			[&uSlotsSeen, &xRow](Zenith_EntityID xEntityID, Zenith_GraphComponent& xGraphs)
			{
				Zenith_Entity xOwner = g_xEngine.Scenes().ResolveEntity(xEntityID);
				const char* szOwner = xOwner.IsValid() ? xOwner.GetName().c_str() : "<unresolved entity>";

				for (u_int uSlot = 0; uSlot < xGraphs.GetGraphCount(); ++uSlot)
				{
					++uSlotsSeen;

					const char* szSlotPath = xGraphs.GetGraphAssetPathAt(uSlot);
					Zenith_BehaviourGraph* pxGraph = xGraphs.GetGraphAt(uSlot);
					if (pxGraph == nullptr)
					{
						// The unresolved-slot contract: the path round-trips even
						// when the .bgraph could not be loaded, so this is exactly
						// the failure the committed bytes cannot show.
						++g_uBootFailures;
						Zenith_Error(LOG_CATEGORY_UNITTEST,
							"[ST_AllScenesBoot] %s: '%s' slot %u ('%s') did not resolve to a graph",
							xRow.m_szPath, szOwner, uSlot, szSlotPath);
						continue;
					}

					const u_int uUnresolvedNodes = pxGraph->GetUnresolvedCount();
					if (uUnresolvedNodes != 0)
					{
						++g_uBootFailures;
						Zenith_Error(LOG_CATEGORY_UNITTEST,
							"[ST_AllScenesBoot] %s: '%s' slot %u ('%s') instantiated with %u "
							"unresolved node(s) -- a node type the .bgraph names is not registered",
							xRow.m_szPath, szOwner, uSlot, szSlotPath, uUnresolvedNodes);
					}
				}
			});

		const u_int uExpectedSlots = ST_CountList(xRow.m_apszGraphs);
		if (uSlotsSeen != uExpectedSlots)
		{
			++g_uBootFailures;
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ST_AllScenesBoot] %s: expected %u graph slot(s), found %u -- an attach was "
				"lost, added, or the expectation table is stale",
				xRow.m_szPath, uExpectedSlots, uSlotsSeen);
		}

		++g_uBootScenesChecked;
	}

	void Setup_AllScenesBoot()
	{
		g_eBootPhase = ST_BootPhase::Load;
		g_uBootRow = 0;
		g_iBootPhaseFrame = 0;
		g_uBootFailures = 0;
		g_uBootScenesChecked = 0;
		g_bBootAborted = false;
		g_xBootSceneBeforeLoad = Zenith_Scene::INVALID_SCENE;
	}

	bool Step_AllScenesBoot(int /*iFrame*/)
	{
		switch (g_eBootPhase)
		{
		case ST_BootPhase::Load:
		{
			const ST_SceneExpectation& xRow = g_axSCENES[g_uBootRow];
			// Remembered so the wait below can require a DIFFERENT scene handle.
			// Without that, row 0 would pass on the boot scene the harness already
			// loaded -- same build index, no load ever performed.
			g_xBootSceneBeforeLoad = g_xEngine.Scenes().GetActiveScene();
			g_xEngine.Scenes().LoadSceneByIndex(xRow.m_iBuildIndex, SCENE_LOAD_SINGLE);
			g_iBootPhaseFrame = 0;
			g_eBootPhase = ST_BootPhase::Wait;
			return true;
		}

		case ST_BootPhase::Wait:
		{
			const ST_SceneExpectation& xRow = g_axSCENES[g_uBootRow];
			const Zenith_Scene xActive = g_xEngine.Scenes().GetActiveScene();
			const Zenith_SceneInfo xInfo = g_xEngine.Scenes().GetSceneInfo(xActive);
			if (xInfo.m_bLoaded
				&& xInfo.m_iBuildIndex == xRow.m_iBuildIndex
				&& xActive != g_xBootSceneBeforeLoad)
			{
				g_iBootPhaseFrame = 0;
				g_eBootPhase = ST_BootPhase::Settle;
				return true;
			}
			if (++g_iBootPhaseFrame > iST_LOAD_WAIT_FRAMES)
			{
				++g_uBootFailures;
				g_bBootAborted = true;
				Zenith_Error(LOG_CATEGORY_UNITTEST,
					"[ST_AllScenesBoot] %s: build index %d never became the active loaded scene "
					"within %d frames (active build index %d, loaded %d)",
					xRow.m_szPath, xRow.m_iBuildIndex, iST_LOAD_WAIT_FRAMES,
					xInfo.m_iBuildIndex, xInfo.m_bLoaded ? 1 : 0);
				g_eBootPhase = ST_BootPhase::Done;
				return false;
			}
			return true;
		}

		case ST_BootPhase::Settle:
			if (++g_iBootPhaseFrame < iST_SETTLE_FRAMES)
			{
				return true;
			}
			g_eBootPhase = ST_BootPhase::Check;
			return true;

		case ST_BootPhase::Check:
			ST_CheckLoadedScene(g_axSCENES[g_uBootRow]);
			if (++g_uBootRow >= uST_SCENE_ROWS)
			{
				g_eBootPhase = ST_BootPhase::Done;
				return false;
			}
			g_eBootPhase = ST_BootPhase::Load;
			return true;

		case ST_BootPhase::Done:
			return false;
		}
		return false;
	}

	bool Verify_AllScenesBoot()
	{
		// A run that timed out mid-walk is distinguished from one that failed
		// assertions: the first means the remaining scenes were never LOOKED at.
		if (g_bBootAborted)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ST_AllScenesBoot] aborted after %u of %u scenes -- the rest were never checked",
				g_uBootScenesChecked, uST_SCENE_ROWS);
			return false;
		}

		if (g_uBootScenesChecked != uST_SCENE_ROWS)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ST_AllScenesBoot] only %u of %u scenes were checked -- the walk ran out of "
				"frames before it finished",
				g_uBootScenesChecked, uST_SCENE_ROWS);
			return false;
		}

		if (g_uBootFailures != 0)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ST_AllScenesBoot] %u failure(s) across %u scenes",
				g_uBootFailures, uST_SCENE_ROWS);
			return false;
		}

		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ST_AllScenesBoot] all %u scenes loaded with their entities present and every "
			"graph slot resolved", uST_SCENE_ROWS);
		return true;
	}
}

static const Zenith_AutomatedTest g_xScriptTestAllScenesBoot = {
	"ST_AllScenesBoot",
	&Setup_AllScenesBoot,
	&Step_AllScenesBoot,
	&Verify_AllScenesBoot,
	// 7 scenes x (1 load + up to 120 wait + 4 settle + 1 check) plus margin. The
	// harness reloads the boot scene itself once this returns, so the test leaves
	// the world on Gym_UI deliberately.
	/*maxFrames*/ 1200
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xScriptTestAllScenesBoot);

#endif // ZENITH_INPUT_SIMULATOR
