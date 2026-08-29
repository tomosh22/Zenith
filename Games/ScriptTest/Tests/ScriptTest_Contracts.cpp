#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

// ============================================================================
// ScriptTest_Contracts.cpp -- the three HERMETIC contract tests (C2, C3, C4).
//
// None of them loads a scene, runs physics or needs a graphics device. Each
// builds the graph it is about IN PROCESS from the same BuildGraph_ST_*
// function the tools boot writes the .bgraph from, so there is no asset on disk
// to go stale and no dependency on a prior tools run having left one behind.
//
//   ST_NoGameExtensionsContract -- the mechanical proof of this game's central
//                                  claim: it defines NO game ECS component and
//                                  NO game graph-node implementation.
//   ST_TrafficLightContract     -- the StateMachine + Wait cadence, read off the
//                                  blackboard rather than off a rendered lamp.
//   ST_PlayerMoveContract       -- the input -> blackboard half of the movement
//                                  chain, driven through the real device layer.
//
// ---------------------------------------------------------------------------
// WHAT ST_NoGameExtensionsContract CANNOT SEE. TWO PROPERTIES, BOTH REAL, BOTH
// ENFORCED BY REVIEW RATHER THAN BY THIS TEST. Stated here so nobody reads a
// green run as a stronger guarantee than it is.
//
//   (i) HOOK PURITY. The claim includes "the Project_* hooks make no runtime
//       decisions" -- they create materials, meshes, graphs and scenes and then
//       get out of the way. This test observes REGISTRIES, and a runtime
//       decision inside a hook registers nothing, so it is invisible here.
//       Note the exemption that makes this a judgement rather than a rule: the
//       constant project ACCESSORS are legitimately called every frame --
//       Zenith_EditorPanel_ContentBrowser.cpp:78 and :923 both call
//       Project_GetGameAssetsDirectory() from panel render code -- and a
//       constant returned per frame is not a decision.
//
//  (ii) NAME SHADOWING. Both provenance checks below compare NAMES, and a name
//       is not an identity. Zenith_ComponentMetaRegistry::RegisterComponent
//       ends in `m_xMetaByName[strTypeName] = xMeta;` (Zenith_ComponentMeta.h
//       :420) -- a same-name registration OVERWRITES the engine row rather than
//       being rejected. A game type registered as "Transform" would therefore
//       pass part (b) while having replaced the engine's Transform outright.
//       The graph-node registry does reject a duplicate by name (and logs it),
//       so (a) is safe from the same trick; the component half is not.
// ============================================================================

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "Collections/Zenith_Vector.h"
#include "Input/Zenith_Input.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"
#include "Maths/Zenith_Maths.h"
#include "Scripting/Zenith_BehaviourGraph.h"
#include "Scripting/Zenith_GraphBlackboard.h"
#include "Scripting/Zenith_GraphBuilder.h"
#include "Scripting/Zenith_GraphNode.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "ZenithECS/Zenith_ComponentMeta.h"

#include "ScriptTest/ScriptTest_Graphs.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace
{
	//=========================================================================
	// Named-check accumulator, shared by the three tests. Every check runs and
	// reports, so ONE run names EVERY clause that moved.
	//=========================================================================
	int g_iChecks = 0;
	int g_iFailures = 0;

	void ResetChecks()
	{
		g_iChecks = 0;
		g_iFailures = 0;
	}

	void CheckTrue(bool bCondition, const char* szWhat)
	{
		++g_iChecks;
		if (!bCondition)
		{
			++g_iFailures;
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[ScriptTestContract] FAILED: %s", szWhat);
		}
	}

	void CheckEqInt(int iActual, int iExpected, const char* szWhat)
	{
		++g_iChecks;
		if (iActual != iExpected)
		{
			++g_iFailures;
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[ScriptTestContract] FAILED: %s (expected %d, got %d)",
				szWhat, iExpected, iActual);
		}
	}

	void CheckEqFloat(float fActual, float fExpected, float fTolerance, const char* szWhat)
	{
		++g_iChecks;
		if (std::fabs(fActual - fExpected) > fTolerance)
		{
			++g_iFailures;
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[ScriptTestContract] FAILED: %s (expected %f, got %f)",
				szWhat, fExpected, fActual);
		}
	}

	void CheckEqStr(const char* szActual, const char* szExpected, const char* szWhat)
	{
		++g_iChecks;
		if (std::strcmp(szActual, szExpected) != 0)
		{
			++g_iFailures;
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[ScriptTestContract] FAILED: %s\n  expected: %s\n  actual:   %s",
				szWhat, szExpected, szActual);
		}
	}

	bool ReportChecks(const char* szTest)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[ScriptTestContract] %s: %d checks, %d failed",
			szTest, g_iChecks, g_iFailures);
		// A test that asserted nothing is a test that cannot fail.
		return g_iFailures == 0 && g_iChecks > 0;
	}

	bool ContainsName(const Zenith_Vector<std::string>& xNames, const std::string& strName)
	{
		for (u_int u = 0; u < xNames.GetSize(); ++u)
		{
			if (xNames.Get(u) == strName)
			{
				return true;
			}
		}
		return false;
	}

	void SnapshotNodeTypeNames(Zenith_Vector<std::string>& xOut)
	{
		xOut.Clear();
		const Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
		for (u_int u = 0; u < xRegistry.GetTypeCount(); ++u)
		{
			xOut.PushBack(xRegistry.GetTypeAt(u).m_strTypeName);
		}
	}
}

// ============================================================================
// ST_NoGameExtensionsContract (C2)
//
// Three parts, all synchronous - the whole test runs in one Step.
// ============================================================================

namespace
{
	//-------------------------------------------------------------------------
	// (b)'s allowlist. TWO GROUPS, and the split is the documentation.
	//
	// PRODUCTION rows are exactly what Zenith_RegisterEngineComponents installs
	// (Zenith/EntityComponent/Zenith_ComponentMeta_Registration.cpp) plus AIAgent,
	// which that file registers through the Zenith_AI_RegisterComponents
	// forwarder so the registration TU need not pull the heavy AIAgent header.
	//
	// TESTING rows are the three components the engine's own unit suites declare
	// with ZENITH_REGISTER_COMPONENT. They are in EVERY build, not just test
	// runs: their .Tests.inl hosts sit behind `#ifdef ZENITH_TESTING`, and
	// Zenith.h defines ZENITH_TESTING UNCONDITIONALLY (Zenith/Core/Zenith.h:294).
	// They belong in the allowlist for that reason and no other -- they are not
	// this game's, and a build that stopped compiling them would fail the
	// exact-membership check below, which is the signal we want rather than a
	// silent narrowing.
	//-------------------------------------------------------------------------
	enum ComponentOwner : u_int
	{
		COMPONENT_OWNER_ENGINE_PRODUCTION = 0,
		COMPONENT_OWNER_ENGINE_TESTING,
	};

	struct EngineComponentRow
	{
		const char* m_szName;
		ComponentOwner m_eOwner;
	};

	const EngineComponentRow g_axEngineComponents[] =
	{
		// --- engine production set (serialization order in the comment) -------
		{ "Transform",      COMPONENT_OWNER_ENGINE_PRODUCTION },	//  0
		{ "Model",          COMPONENT_OWNER_ENGINE_PRODUCTION },	// 10
		{ "Tween",          COMPONENT_OWNER_ENGINE_PRODUCTION },	// 12
		{ "Animator",       COMPONENT_OWNER_ENGINE_PRODUCTION },	// 15
		{ "Camera",         COMPONENT_OWNER_ENGINE_PRODUCTION },	// 20
		{ "Light",          COMPONENT_OWNER_ENGINE_PRODUCTION },	// 25
		{ "Sun",            COMPONENT_OWNER_ENGINE_PRODUCTION },	// 26
		{ "Atmosphere",     COMPONENT_OWNER_ENGINE_PRODUCTION },	// 27
		{ "Terrain",        COMPONENT_OWNER_ENGINE_PRODUCTION },	// 40
		{ "Collider",       COMPONENT_OWNER_ENGINE_PRODUCTION },	// 50
		{ "Graph",          COMPONENT_OWNER_ENGINE_PRODUCTION },	// 60
		{ "UI",             COMPONENT_OWNER_ENGINE_PRODUCTION },	// 70
		{ "InstancedMesh",  COMPONENT_OWNER_ENGINE_PRODUCTION },	// 80
		{ "ParticleEmitter",COMPONENT_OWNER_ENGINE_PRODUCTION },	// 85
		{ "AIAgent",        COMPONENT_OWNER_ENGINE_PRODUCTION },	// 90 (AI forwarder)
		{ "Attachment",     COMPONENT_OWNER_ENGINE_PRODUCTION },	// 95
		{ "NavMesh",        COMPONENT_OWNER_ENGINE_PRODUCTION },	// 96

		// --- engine test-suite set (present in every build; see above) --------
		{ "PhysicsTest",    COMPONENT_OWNER_ENGINE_TESTING },		// 200
		{ "SerialCanary",   COMPONENT_OWNER_ENGINE_TESTING },		// 201
		{ "WorldResetProbe",COMPONENT_OWNER_ENGINE_TESTING },		// 202
	};

	constexpr u_int uENGINE_COMPONENT_ROWS =
		static_cast<u_int>(sizeof(g_axEngineComponents) / sizeof(g_axEngineComponents[0]));

	const EngineComponentRow* FindEngineComponentRow(const std::string& strName)
	{
		for (u_int u = 0; u < uENGINE_COMPONENT_ROWS; ++u)
		{
			if (strName == g_axEngineComponents[u].m_szName)
			{
				return &g_axEngineComponents[u];
			}
		}
		return nullptr;
	}

	//-------------------------------------------------------------------------
	// (c)'s table: every graph this game authors, keyed by its ASSET PATH, which
	// is the one spelling of a graph's identity ScriptTest_Graphs.h owns.
	//
	// A SIXTEENTH BUILDER ADDED WITHOUT A ROW HERE GOES UNCHECKED, and nothing
	// mechanical stops that: the builders are free functions, so there is no list
	// to enumerate at compile time and no count to compare against. The row goes
	// in with the builder, or the graph has no coverage.
	//-------------------------------------------------------------------------
	struct GraphBuilderRow
	{
		const char* m_szAssetPath;
		void (*m_pfnBuild)(Zenith_GraphBuilder&);
	};

	const GraphBuilderRow g_axGraphBuilders[] =
	{
		{ ScriptTest::Graphs::szESC_TO_HUB,     &BuildGraph_ST_EscToHub },
		{ ScriptTest::Graphs::szHUB_FLOW,       &BuildGraph_ST_HubFlow },
		{ ScriptTest::Graphs::szSPIN,           &BuildGraph_ST_Spin },
		{ ScriptTest::Graphs::szPING_PONG,      &BuildGraph_ST_PingPong },
		{ ScriptTest::Graphs::szSINE_BOB,       &BuildGraph_ST_SineBob },
		{ ScriptTest::Graphs::szPLAYER_MOVE,    &BuildGraph_ST_PlayerMove },
		{ ScriptTest::Graphs::szJUMP,           &BuildGraph_ST_Jump },
		{ ScriptTest::Graphs::szBALL_SPAWNER,   &BuildGraph_ST_BallSpawner },
		{ ScriptTest::Graphs::szKILL_VOLUME,    &BuildGraph_ST_KillVolume },
		{ ScriptTest::Graphs::szPRESSURE_PLATE, &BuildGraph_ST_PressurePlate },
		{ ScriptTest::Graphs::szDOOR,           &BuildGraph_ST_Door },
		{ ScriptTest::Graphs::szBELL_RING,      &BuildGraph_ST_BellRing },
		{ ScriptTest::Graphs::szBELL_LISTENER,  &BuildGraph_ST_BellListener },
		{ ScriptTest::Graphs::szTRAFFIC_LIGHT,  &BuildGraph_ST_TrafficLight },
		{ ScriptTest::Graphs::szUI_PLAYGROUND,  &BuildGraph_ST_UIPlayground },
	};

	constexpr u_int uGRAPH_BUILDER_ROWS =
		static_cast<u_int>(sizeof(g_axGraphBuilders) / sizeof(g_axGraphBuilders[0]));

	bool g_bNoGameExtensionsRan = false;

	//-------------------------------------------------------------------------
	// (a) NODE-REGISTRY PROVENANCE.
	//
	// The live registry is engine nodes PLUS whatever the game's project hooks
	// added; there is no flag on a row saying which. ResetForTests() clears the
	// type list and the initialized flag but KEEPS the installed registrar, so
	// re-running EnsureInitialized() re-derives the ENGINE-ONLY set from the
	// engine's own registrar (Zenith_Engine::Initialise installs
	// &Zenith_RegisterEngineGraphNodes). Anything in live-but-not-engine is a
	// game registration.
	//
	// ★ WHY TEARING DOWN A LIVE REGISTRY IS SAFE HERE, AND WHAT THE RESIDUAL IS.
	// Live Zenith_BehaviourGraph instances (the hub's ST_HubFlow, for one) cache
	// `const Zenith_GraphNodeTypeInfo*` per node. Zenith_Vector::Clear()
	// destructs the elements and zeroes the SIZE but keeps the buffer and its
	// capacity, so re-registering the same types in the same registrar order
	// reconstructs every engine row at the address it already had -- the cached
	// pointers stay valid and describe the same type. That holds whatever the
	// game did, because the engine registrar runs FIRST and in a fixed order, so
	// engine rows always reoccupy indices [0..N). The residual is a live node of
	// a GAME-registered type, whose row would not come back: that is exactly the
	// case this check FAILS on, so the report is delivered either way.
	//
	// ★ "SAME ORDER" IS A PREMISE, SO IT IS CHECKED RATHER THAN ASSUMED. The
	// buffer-reuse property that makes the addresses stable is exactly what makes
	// an address comparison unable to detect a REORDER: refilling the same buffer
	// with the same N types in a different sequence leaves every address where it
	// was while every cached pointer now names the wrong type -- strictly worse
	// than a dangling one, because nothing crashes. So the names are compared
	// index-wise as well, and it is that pairing (addresses AND order) that makes
	// tearing down a live registry safe here.
	//-------------------------------------------------------------------------
	void RunNodeProvenanceChecks(Zenith_Vector<std::string>& xEngineNamesOut)
	{
		Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
		xRegistry.EnsureInitialized();

		Zenith_Vector<std::string> xLiveNames;
		SnapshotNodeTypeNames(xLiveNames);
		CheckTrue(xLiveNames.GetSize() > 0, "the live node registry is populated at all");

		// The row ADDRESSES, so the pointer-stability argument above is a checked
		// property rather than a believed one. A live graph caches these.
		Zenith_Vector<const Zenith_GraphNodeTypeInfo*> apxLiveRows;
		for (u_int u = 0; u < xRegistry.GetTypeCount(); ++u)
		{
			apxLiveRows.PushBack(&xRegistry.GetTypeAt(u));
		}

		xRegistry.ResetForTests();
		xRegistry.EnsureInitialized();
		SnapshotNodeTypeNames(xEngineNamesOut);

		int iMovedRows = 0;
		const u_int uCommonRows = xRegistry.GetTypeCount() < apxLiveRows.GetSize()
			? xRegistry.GetTypeCount() : apxLiveRows.GetSize();
		for (u_int u = 0; u < uCommonRows; ++u)
		{
			if (&xRegistry.GetTypeAt(u) != apxLiveRows.Get(u))
			{
				++iMovedRows;
			}
		}
		CheckEqInt(iMovedRows, 0,
			"re-registration reoccupies the same rows (a live graph's cached type-info pointers survive the reset)");

		// ...and the row CONTENTS, index-wise, because the address check ALONE IS
		// BLIND TO A REORDER. Zenith_Vector::Clear() keeps the buffer and its
		// capacity, so registering the same N types in a DIFFERENT order refills
		// the same N addresses: every pointer above compares equal while every
		// cached m_pxTypeInfo in every live graph now describes a different node
		// type. Both name lists were captured in registry order -- xLiveNames
		// before the reset, xEngineNamesOut after it -- so a mismatch at any
		// common index IS that reorder, and naming both spellings says which two
		// types swapped. (Rows a GAME registered append after the engine's, so
		// they cannot shift this prefix; that case is the separate check below.)
		int iReorderedRows = 0;
		const u_int uCommonNames = xLiveNames.GetSize() < xEngineNamesOut.GetSize()
			? xLiveNames.GetSize() : xEngineNamesOut.GetSize();
		for (u_int u = 0; u < uCommonNames; ++u)
		{
			if (xLiveNames.Get(u) != xEngineNamesOut.Get(u))
			{
				++iReorderedRows;
				Zenith_Log(LOG_CATEGORY_UNITTEST,
					"[ScriptTestContract]   row %u REORDERED: live '%s', re-derived '%s'",
					u, xLiveNames.Get(u).c_str(), xEngineNamesOut.Get(u).c_str());
			}
		}
		CheckEqInt(iReorderedRows, 0,
			"re-registration refills those rows with the SAME types in the SAME order "
			"(a cached type-info pointer still describes its own type)");

		// If this fails the registrar was never installed and the reset above
		// just emptied the process's node registry - report it as the primary
		// failure so the cascade below is not read as the cause.
		CheckTrue(xEngineNamesOut.GetSize() > 0,
			"ResetForTests + EnsureInitialized re-derived the engine node set (the registrar survives a reset)");

		int iGameRegistered = 0;
		for (u_int u = 0; u < xLiveNames.GetSize(); ++u)
		{
			if (!ContainsName(xEngineNamesOut, xLiveNames.Get(u)))
			{
				++iGameRegistered;
				Zenith_Log(LOG_CATEGORY_UNITTEST,
					"[ScriptTestContract]   GAME-REGISTERED graph node type: '%s'", xLiveNames.Get(u).c_str());
			}
		}
		CheckEqInt(iGameRegistered, 0,
			"ScriptTest registers NO graph node type of its own (every live type is engine-derived)");

		// The other direction. Engine-but-not-live cannot happen through a game
		// hook, so a hit here means the re-derivation itself is not reproducing
		// the set - which would make the check above meaningless.
		int iMissingFromLive = 0;
		for (u_int u = 0; u < xEngineNamesOut.GetSize(); ++u)
		{
			if (!ContainsName(xLiveNames, xEngineNamesOut.Get(u)))
			{
				++iMissingFromLive;
				Zenith_Log(LOG_CATEGORY_UNITTEST,
					"[ScriptTestContract]   engine node type absent from the LIVE set: '%s'", xEngineNamesOut.Get(u).c_str());
			}
		}
		CheckEqInt(iMissingFromLive, 0,
			"the re-derived engine set is a subset of the live set (the reset reproduces the registrar exactly)");

		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ScriptTestContract] node registry: %u live, %u engine-derived",
			xLiveNames.GetSize(), xEngineNamesOut.GetSize());
	}

	//-------------------------------------------------------------------------
	// (b) COMPONENT-META PROVENANCE. The meta registry has no registrar-reset
	// equivalent, so the engine set is the documented table above rather than a
	// re-derivation. Membership is asserted BOTH ways: an extra name is a game
	// component, and a missing one means the table has gone stale against the
	// engine (a false green waiting to happen).
	//-------------------------------------------------------------------------
	void RunComponentProvenanceChecks()
	{
		const Zenith_Vector<const Zenith_ComponentMeta*>& xMetas =
			Zenith_ComponentMetaRegistry::Get().GetAllMetasSorted();

		CheckTrue(xMetas.GetSize() > 0, "the component-meta registry is populated at all");

		Zenith_Vector<std::string> xRegisteredNames;
		int iGameRegistered = 0;
		for (u_int u = 0; u < xMetas.GetSize(); ++u)
		{
			const Zenith_ComponentMeta* pxMeta = xMetas.Get(u);
			if (pxMeta == nullptr)
			{
				continue;
			}
			xRegisteredNames.PushBack(pxMeta->m_strTypeName);
			if (FindEngineComponentRow(pxMeta->m_strTypeName) == nullptr)
			{
				++iGameRegistered;
				Zenith_Log(LOG_CATEGORY_UNITTEST,
					"[ScriptTestContract]   GAME-REGISTERED component: '%s' (order %u)",
					pxMeta->m_strTypeName.c_str(), pxMeta->m_uSerializationOrder);
			}
		}
		CheckEqInt(iGameRegistered, 0,
			"ScriptTest registers NO ECS component of its own (every registered name is engine-owned)");

		int iMissingProduction = 0;
		int iMissingTesting = 0;
		for (u_int u = 0; u < uENGINE_COMPONENT_ROWS; ++u)
		{
			if (ContainsName(xRegisteredNames, g_axEngineComponents[u].m_szName))
			{
				continue;
			}
			if (g_axEngineComponents[u].m_eOwner == COMPONENT_OWNER_ENGINE_PRODUCTION)
			{
				++iMissingProduction;
			}
			else
			{
				++iMissingTesting;
			}
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ScriptTestContract]   allowlisted component NOT registered: '%s'", g_axEngineComponents[u].m_szName);
		}
		CheckEqInt(iMissingProduction, 0,
			"every allowlisted engine PRODUCTION component is registered (the table has not gone stale)");
		CheckEqInt(iMissingTesting, 0,
			"every allowlisted engine TESTING component is registered (ZENITH_TESTING is unconditional)");

		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ScriptTestContract] component metas: %u registered, %u allowlisted",
			xRegisteredNames.GetSize(), uENGINE_COMPONENT_ROWS);
	}

	//-------------------------------------------------------------------------
	// (c) BUILDER INTEGRITY. Each builder must Build() cleanly, name only node
	// types from the engine set derived in (a), and instantiate with nothing
	// unresolved. The last clause is the one that matters most: an unresolved
	// node loads, round-trips and silently fails its chain, so a graph can be
	// completely inert while every other signal says it is fine.
	//-------------------------------------------------------------------------
	void RunBuilderIntegrityChecks(const Zenith_Vector<std::string>& xEngineNames)
	{
		// A floor, honestly labelled: it catches a row DELETED from the table
		// above, not a sixteenth builder added without one. Nothing enumerates
		// the builders at compile time, so the only guard against a new graph
		// slipping past this test is the row going in with it.
		CheckEqInt(static_cast<int>(uGRAPH_BUILDER_ROWS), 15,
			"the builder table still lists all fifteen graphs ScriptTest authors");

		for (u_int uRow = 0; uRow < uGRAPH_BUILDER_ROWS; ++uRow)
		{
			const GraphBuilderRow& xRow = g_axGraphBuilders[uRow];
			char acWhat[256];

			Zenith_GraphDefinition xDefinition;
			{
				Zenith_GraphBuilder xBuilder(xDefinition);
				xRow.m_pfnBuild(xBuilder);
				const bool bBuilt = xBuilder.Build();
				std::snprintf(acWhat, sizeof(acWhat), "%s builds with no authoring error", xRow.m_szAssetPath);
				CheckTrue(bBuilt, acWhat);
			}

			std::snprintf(acWhat, sizeof(acWhat), "%s authored at least one node", xRow.m_szAssetPath);
			CheckTrue(xDefinition.GetNodeCount() > 0, acWhat);

			int iNonEngineNodes = 0;
			for (u_int uNode = 0; uNode < xDefinition.GetNodeCount(); ++uNode)
			{
				const Zenith_GraphNodeDef& xNode = xDefinition.GetNodeAt(uNode);
				if (!ContainsName(xEngineNames, xNode.m_strTypeName))
				{
					++iNonEngineNodes;
					Zenith_Log(LOG_CATEGORY_UNITTEST,
						"[ScriptTestContract]   %s names a NON-ENGINE node type: '%s'",
						xRow.m_szAssetPath, xNode.m_strTypeName.c_str());
				}
			}
			std::snprintf(acWhat, sizeof(acWhat), "%s names only engine-owned node types", xRow.m_szAssetPath);
			CheckEqInt(iNonEngineNodes, 0, acWhat);

			Zenith_BehaviourGraph xGraph;
			const bool bInstanced = xGraph.InitialiseFromDefinition(xDefinition);
			std::snprintf(acWhat, sizeof(acWhat), "%s instantiates", xRow.m_szAssetPath);
			CheckTrue(bInstanced, acWhat);

			std::snprintf(acWhat, sizeof(acWhat), "%s instantiates with ZERO unresolved nodes", xRow.m_szAssetPath);
			CheckEqInt(static_cast<int>(xGraph.GetUnresolvedCount()), 0, acWhat);

			xGraph.Shutdown();
		}
	}

	void Setup_NoGameExtensions()
	{
		ResetChecks();
		g_bNoGameExtensionsRan = false;
	}

	bool Step_NoGameExtensions(int /*iFrame*/)
	{
		Zenith_Vector<std::string> xEngineNodeNames;
		RunNodeProvenanceChecks(xEngineNodeNames);
		RunComponentProvenanceChecks();
		RunBuilderIntegrityChecks(xEngineNodeNames);
		g_bNoGameExtensionsRan = true;
		return false;	// entirely synchronous - one frame is all this needs
	}

	bool Verify_NoGameExtensions()
	{
		CheckTrue(g_bNoGameExtensionsRan, "the provenance checks ran");
		return ReportChecks("ST_NoGameExtensionsContract");
	}
}

static const Zenith_AutomatedTest g_xNoGameExtensionsContractTest = {
	"ST_NoGameExtensionsContract",
	&Setup_NoGameExtensions,
	&Step_NoGameExtensions,
	&Verify_NoGameExtensions,
	/*maxFrames*/ 8,
	/*bRequiresGraphics*/ false,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xNoGameExtensionsContractTest);

// ============================================================================
// The hermetic rig shared by C3 and C4: one production graph definition, one
// live instance, no scene and no entity.
//
// ★ NO ENTITY IS DELIBERATE, not a shortcut. Every node that would need one
// resolves through Zenith_GraphContext::ResolveTargetEntity(""), which returns
// the (invalid) self handle, and each such node then returns FAILURE and aborts
// its own chain -- LockRotation and SetVelocity in ST_PlayerMove, the lamp
// chains in ST_TrafficLight. Nothing asserts, and the blackboard writes these
// tests read happen BEFORE the failing node in every chain that matters.
// StateMachine's transition events are inert for the same reason: it bails out
// of FireTransitionEvent on an invalid self, so the TLEnter_*/TLExit_* chains
// never dispatch at all.
// ============================================================================

namespace
{
	struct ScriptTestGraphRig
	{
		Zenith_GraphDefinition m_xDefinition;
		Zenith_BehaviourGraph m_xGraph;
		bool m_bValid = false;

		explicit ScriptTestGraphRig(void (*pfnBuild)(Zenith_GraphBuilder&))
		{
			bool bBuilt = false;
			{
				Zenith_GraphBuilder xBuilder(m_xDefinition);
				pfnBuild(xBuilder);
				bBuilt = xBuilder.Build();
			}
			const bool bInstanced = m_xGraph.InitialiseFromDefinition(m_xDefinition);
			m_bValid = bBuilt && bInstanced && m_xGraph.GetUnresolvedCount() == 0;
		}

		~ScriptTestGraphRig()
		{
			m_xGraph.Shutdown();
		}

		ScriptTestGraphRig(const ScriptTestGraphRig&) = delete;
		ScriptTestGraphRig& operator=(const ScriptTestGraphRig&) = delete;

		Zenith_GraphContext MakeContext(float fDt)
		{
			Zenith_GraphContext xContext;
			// m_xSelf stays the default (invalid) handle - see the note above.
			xContext.m_fDt = fDt;
			xContext.m_pxGraph = &m_xGraph;
			xContext.m_pxBlackboard = &m_xGraph.GetBlackboard();
			return xContext;
		}

		void Tick(float fDt)
		{
			Zenith_GraphContext xContext = MakeContext(fDt);
			m_xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
		}

		int32_t BBInt(const char* szName, int32_t iDefault) const
		{
			return m_xGraph.GetBlackboard().GetInt32(szName, iDefault);
		}

		Zenith_Maths::Vector3 BBVec3(const char* szName) const
		{
			return m_xGraph.GetBlackboard().GetVector3(szName, Zenith_Maths::Vector3(0.0f));
		}

		const char* TypeOfNode(u_int uNodeID) const
		{
			const Zenith_GraphNodeDef* pxDef = m_xDefinition.FindNodeDef(uNodeID);
			return pxDef != nullptr ? pxDef->m_strTypeName.c_str() : "<unknown>";
		}

		// The last ON_UPDATE dispatch's chain walk as "A>B>C". The trace is
		// cleared at the top of every ON_UPDATE FireEvent and appended by
		// RunChainFromPin, so it holds exactly that tick's chain IN EXECUTION
		// ORDER - and the SOURCE node is not in it (a chain hangs OFF the
		// source's pin 0), so the first entry is the source's successor.
		void FormatTrace(char* pcOut, size_t uCapacity) const
		{
			pcOut[0] = '\0';
			const Zenith_Vector<u_int>& auTrace = m_xGraph.GetRecentlyExecuted();
			for (u_int u = 0; u < auTrace.GetSize(); ++u)
			{
				const size_t uLength = std::strlen(pcOut);
				std::snprintf(pcOut + uLength, uCapacity - uLength, "%s%s",
					u == 0 ? "" : ">", TypeOfNode(auTrace.Get(u)));
			}
		}
	};
}

// ============================================================================
// ST_TrafficLightContract (C3)
//
// ST_TrafficLight is the only graph in the game whose behaviour is a CADENCE
// rather than a reaction, and its whole visible output is three lamp materials
// - unreadable without a GPU and a scene. The state it is really keeping is the
// blackboard int, so that is what this pins: the machine sits on Red for 3 s,
// Green for 3 s and Amber for 1 s, in that order, and rolls over.
//
// dt = 0.5 is an exact binary fraction, so the Wait accumulator's partial sums
// are exact and no boundary sits on a float knife edge: Red's sixth tick lands
// on exactly 3.0. The fire indices below follow from that (6 / 12 / 14), but
// each is asserted with a +/-1 window because whether the threshold tick is the
// one that CROSSES or the one AFTER it is a Wait-node detail this contract does
// not own. What it does own is the ORDER and the RELATIVE lengths, and those
// are asserted exactly.
// ============================================================================

namespace
{
	constexpr float k_fTrafficDt = 0.5f;
	constexpr int k_iTrafficFires = 18;		// past the third transition, with room to spare
	constexpr int k_iTrafficFireWindow = 1;	// +/- fires allowed on each boundary

	struct TrafficTransition
	{
		int m_iValue = -1;
		int m_iFire = -1;
	};

	// Red(3.0s) -> Green at fire 6, Green(3.0s) -> Amber at 12, Amber(1.0s) ->
	// Red at 14. The VALUES are the contract; the fires carry the window.
	const TrafficTransition k_axExpectedTransitions[] =
	{
		{ 1, 6 },
		{ 2, 12 },
		{ 0, 14 },
	};
	constexpr int k_iExpectedTransitionCount =
		static_cast<int>(sizeof(k_axExpectedTransitions) / sizeof(k_axExpectedTransitions[0]));

	bool g_bTrafficLightRan = false;

	void RunTrafficLightChecks()
	{
		ScriptTestGraphRig xRig(&BuildGraph_ST_TrafficLight);
		CheckTrue(xRig.m_bValid, "the rig built ST_TrafficLight with no unresolved nodes");
		if (!xRig.m_bValid)
		{
			return;
		}

		// The declared variable, before anything runs. Read with a default of -1
		// so "declared as 0" and "not declared at all" are distinguishable.
		CheckEqInt(xRig.BBInt(ScriptTest::Vars::szLIGHT, -1), 0,
			"the light variable is declared and starts on Red (0)");

		TrafficTransition axObserved[8];
		int iObserved = 0;
		int iOutOfRangeValue = -1;
		int iOutOfRangeAtFire = -1;
		int32_t iPrevious = xRig.BBInt(ScriptTest::Vars::szLIGHT, -1);

		for (int iFire = 1; iFire <= k_iTrafficFires; ++iFire)
		{
			xRig.Tick(k_fTrafficDt);
			const int32_t iLight = xRig.BBInt(ScriptTest::Vars::szLIGHT, -1);

			if ((iLight < 0 || iLight > 2) && iOutOfRangeAtFire < 0)
			{
				iOutOfRangeValue = static_cast<int>(iLight);
				iOutOfRangeAtFire = iFire;
			}
			if (iLight != iPrevious)
			{
				if (iObserved < static_cast<int>(sizeof(axObserved) / sizeof(axObserved[0])))
				{
					axObserved[iObserved].m_iValue = static_cast<int>(iLight);
					axObserved[iObserved].m_iFire = iFire;
				}
				++iObserved;
				iPrevious = iLight;
			}
		}

		CheckEqInt(iOutOfRangeAtFire, -1, "the light never takes a value outside {Red, Green, Amber}");
		if (iOutOfRangeAtFire >= 0)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ScriptTestContract]   light was %d at fire %d", iOutOfRangeValue, iOutOfRangeAtFire);
		}

		CheckEqInt(iObserved, k_iExpectedTransitionCount,
			"the machine makes exactly three transitions in 18 fires at dt=0.5 (Red->Green->Amber->Red)");

		const int iCompare = iObserved < k_iExpectedTransitionCount ? iObserved : k_iExpectedTransitionCount;
		for (int i = 0; i < iCompare; ++i)
		{
			char acWhat[192];
			std::snprintf(acWhat, sizeof(acWhat), "transition %d lands on state %d",
				i + 1, k_axExpectedTransitions[i].m_iValue);
			CheckEqInt(axObserved[i].m_iValue, k_axExpectedTransitions[i].m_iValue, acWhat);

			const int iDelta = axObserved[i].m_iFire - k_axExpectedTransitions[i].m_iFire;
			std::snprintf(acWhat, sizeof(acWhat),
				"transition %d fires at tick %d +/-%d (observed %d)",
				i + 1, k_axExpectedTransitions[i].m_iFire, k_iTrafficFireWindow, axObserved[i].m_iFire);
			CheckTrue(iDelta >= -k_iTrafficFireWindow && iDelta <= k_iTrafficFireWindow, acWhat);
		}

		// The relative lengths, independent of where the first boundary sits:
		// Amber is the SHORT one (1 s against Green's 3 s), which is the clause a
		// swapped Wait property would move while every absolute window above
		// still passed.
		if (iObserved >= k_iExpectedTransitionCount)
		{
			const int iGreenLength = axObserved[1].m_iFire - axObserved[0].m_iFire;
			const int iAmberLength = axObserved[2].m_iFire - axObserved[1].m_iFire;
			CheckEqInt(iGreenLength, 6, "Green lasts 3.0 s (6 fires at dt=0.5)");
			CheckEqInt(iAmberLength, 2, "Amber lasts 1.0 s (2 fires at dt=0.5)");
		}
	}

	void Setup_TrafficLightContract()
	{
		ResetChecks();
		g_bTrafficLightRan = false;
	}

	bool Step_TrafficLightContract(int /*iFrame*/)
	{
		RunTrafficLightChecks();
		g_bTrafficLightRan = true;
		return false;	// the graph is driven by hand - one frame is all this needs
	}

	bool Verify_TrafficLightContract()
	{
		CheckTrue(g_bTrafficLightRan, "the traffic-light checks ran");
		return ReportChecks("ST_TrafficLightContract");
	}
}

static const Zenith_AutomatedTest g_xTrafficLightContractTest = {
	"ST_TrafficLightContract",
	&Setup_TrafficLightContract,
	&Step_TrafficLightContract,
	&Verify_TrafficLightContract,
	/*maxFrames*/ 8,
	/*bRequiresGraphics*/ false,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xTrafficLightContractTest);

// ============================================================================
// ST_PlayerMoveContract (C4)
//
// The half of ST_PlayerMove that a hermetic rig can see end to end: a held key
// reaches ReadMovementAxis through the real device layer, and the direction it
// writes is scaled into the velocity variable. The SetVelocity node at the end
// of the chain needs a physics body and correctly FAILS here - but it runs
// LAST, after both blackboard writes, so nothing this test reads depends on it.
//
// ★ THE AXIS SIGN IS THE ENGINE'S, NOT THIS TEST'S GUESS.
// Zenith_GraphNode_ReadMovementAxis (Zenith/EntityComponent/
// Zenith_GraphNode_Registration_Input.cpp) does `if (IsKeyDown(m_iKeyForward))
// { xDirection.z += 1.0f; }`, so W is +Z: forward is POSITIVE Z in this node's
// convention. (Note that it is NOT the action layer's ResolveMoveComposite
// convention of "+y is forward" -- that is a Vector2 and a different node.)
//
// ★ SetKeyHeld IS THE RIGHT SIMULATOR VERB HERE, and it is the wrong one three
// files away. It writes only the simulator's LEVEL table, so a key "held" with
// it reaches Zenith_Input::IsKeyDown and NOTHING ELSE - no transition, no action
// edge. ReadMovementAxis polls IsKeyDown, so that is exactly enough; a test
// steering an ACTION (Combat, RenderTest) must use SimulateKeyDown/Up instead.
// The non-vacuity guard below asserts the key really did reach IsKeyDown, so a
// run with the simulator disabled reports its own cause rather than a zero
// vector.
// ============================================================================

namespace
{
	constexpr float k_fPlayerMoveDt = 1.0f / 60.0f;
	constexpr float k_fMoveSpeed = 6.0f;			// the graph's MathBlackboardVector3 scalar
	constexpr float k_fVectorTolerance = 1e-5f;
	constexpr const char* k_szPlayerMoveTrace = "ReadMovementAxis>MathBlackboardVector3>SetVelocity";

	bool g_bPlayerMoveRan = false;

	void CheckVector3(const Zenith_Maths::Vector3& xActual, const Zenith_Maths::Vector3& xExpected, const char* szWhat)
	{
		char acWhat[224];
		std::snprintf(acWhat, sizeof(acWhat), "%s .x", szWhat);
		CheckEqFloat(xActual.x, xExpected.x, k_fVectorTolerance, acWhat);
		std::snprintf(acWhat, sizeof(acWhat), "%s .y", szWhat);
		CheckEqFloat(xActual.y, xExpected.y, k_fVectorTolerance, acWhat);
		std::snprintf(acWhat, sizeof(acWhat), "%s .z", szWhat);
		CheckEqFloat(xActual.z, xExpected.z, k_fVectorTolerance, acWhat);
	}

	void RunPlayerMoveChecks()
	{
		ScriptTestGraphRig xRig(&BuildGraph_ST_PlayerMove);
		CheckTrue(xRig.m_bValid, "the rig built ST_PlayerMove with no unresolved nodes");
		if (!xRig.m_bValid)
		{
			return;
		}

		// ---- W held: forward, at the authored speed -------------------------
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, true);
		CheckTrue(g_xEngine.Input().IsKeyDown(ZENITH_KEY_W),
			"the held W reaches the device layer (the simulator is active and SetKeyHeld took)");

		xRig.Tick(k_fPlayerMoveDt);

		const Zenith_Maths::Vector3 xHeldDir = xRig.BBVec3(ScriptTest::Vars::szMOVE_DIR);
		const Zenith_Maths::Vector3 xHeldVel = xRig.BBVec3(ScriptTest::Vars::szMOVE_VEL);
		CheckVector3(xHeldDir, Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f),
			"W held writes unit FORWARD (+Z) into the direction variable");
		CheckVector3(xHeldVel, Zenith_Maths::Vector3(0.0f, 0.0f, k_fMoveSpeed),
			"the velocity variable is the direction scaled by 6");

		// The scale relationship stated as a relationship, so a re-tune of the
		// speed constant moves ONE check rather than reddening the axis clause.
		CheckEqFloat(xHeldVel.z, xHeldDir.z * k_fMoveSpeed, k_fVectorTolerance,
			"velocity == direction * the authored scalar");

		// ---- the chain ran, in the authored order ---------------------------
		// The source anchor is not in the trace (a chain hangs off its pin 0), so
		// this is the whole of the OnUpdate chain. SetVelocity is present and
		// FAILED - it is the last node, and it appears because the trace records
		// entry, not outcome.
		char acTrace[256];
		xRig.FormatTrace(acTrace, sizeof(acTrace));
		CheckEqStr(acTrace, k_szPlayerMoveTrace,
			"the OnUpdate chain is read -> scale -> drive, in that order");

		// ---- nothing held: the read is LIVE, not a constant ------------------
		// Without this the whole test would pass against a node that ignored
		// input and wrote a fixed forward vector.
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, false);
		CheckTrue(!g_xEngine.Input().IsKeyDown(ZENITH_KEY_W), "the held W was released");

		xRig.Tick(k_fPlayerMoveDt);

		CheckVector3(xRig.BBVec3(ScriptTest::Vars::szMOVE_DIR), Zenith_Maths::Vector3(0.0f),
			"no key held writes a ZERO direction");
		CheckVector3(xRig.BBVec3(ScriptTest::Vars::szMOVE_VEL), Zenith_Maths::Vector3(0.0f),
			"no key held writes a ZERO velocity");
	}

	void Setup_PlayerMoveContract()
	{
		ResetChecks();
		g_bPlayerMoveRan = false;
	}

	bool Step_PlayerMoveContract(int /*iFrame*/)
	{
		RunPlayerMoveChecks();
		g_bPlayerMoveRan = true;
		return false;
	}

	bool Verify_PlayerMoveContract()
	{
		CheckTrue(g_bPlayerMoveRan, "the player-move checks ran");
		return ReportChecks("ST_PlayerMoveContract");
	}

	// The held key is released inside the checks above, and the harness
	// normalises input before every test anyway. This is the belt to that
	// braces: a check that returns early (an invalid rig) must not leave W down
	// for whatever runs next.
	void Teardown_PlayerMoveContract()
	{
		Zenith_InputSimulator::ClearHeldKeys();
	}
}

static const Zenith_AutomatedTest g_xPlayerMoveContractTest = {
	"ST_PlayerMoveContract",
	&Setup_PlayerMoveContract,
	&Step_PlayerMoveContract,
	&Verify_PlayerMoveContract,
	/*maxFrames*/ 8,
	/*bRequiresGraphics*/ false,
	/*bManualOnly*/ false,
	&Teardown_PlayerMoveContract,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xPlayerMoveContractTest);

#endif // ZENITH_INPUT_SIMULATOR
