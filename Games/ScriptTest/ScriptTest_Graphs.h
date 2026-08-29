#pragma once

#include "FileAccess/Zenith_FileAccess.h"
#include "Maths/Zenith_Maths.h"

#include <cstdint>

// ============================================================================
// ScriptTest_Graphs.h -- the game's STRING CONTRACT plus the 20 Behaviour
// Graph builder declarations.
//
// ScriptTest carries zero gameplay C++: every behaviour is an engine-owned
// graph node, wired by the builders below and attached to boot-authored scene
// entities. That makes a whole class of strings load-bearing and unchecked --
// an entity name a FindEntityByName node looks up, a UI element name a
// SetUIText node addresses, a blackboard variable two nodes share, a custom
// event one graph fires and another listens for. None of them is validated at
// compile time; a typo produces a graph that builds cleanly and does nothing.
//
// So each is spelled EXACTLY ONCE, here, and shared by the two things that
// must agree: the builders + scene recipes in ScriptTest.cpp, and the tests
// that assert on the result. A test that restated a literal would prove only
// that the test agrees with itself.
//
// Deliberately light: only the extension defines and the maths types come in
// (the latter already arrives through the precompiled header, so it costs a
// TU nothing), and Zenith_GraphBuilder is forward-declared.
// ============================================================================

class Zenith_GraphBuilder;

namespace ScriptTest
{
	// --- Behaviour graph assets (attach order per entity = the B1 slot order) ---
	namespace Graphs
	{
		inline constexpr const char* szESC_TO_HUB     = "game:Graphs/ST_EscToHub"     ZENITH_BGRAPH_EXT;
		inline constexpr const char* szHUB_FLOW       = "game:Graphs/ST_HubFlow"      ZENITH_BGRAPH_EXT;
		inline constexpr const char* szSPIN           = "game:Graphs/ST_Spin"         ZENITH_BGRAPH_EXT;
		inline constexpr const char* szPING_PONG      = "game:Graphs/ST_PingPong"     ZENITH_BGRAPH_EXT;
		inline constexpr const char* szSINE_BOB       = "game:Graphs/ST_SineBob"      ZENITH_BGRAPH_EXT;
		inline constexpr const char* szPLAYER_MOVE    = "game:Graphs/ST_PlayerMove"   ZENITH_BGRAPH_EXT;
		inline constexpr const char* szJUMP           = "game:Graphs/ST_Jump"         ZENITH_BGRAPH_EXT;
		inline constexpr const char* szBALL_SPAWNER   = "game:Graphs/ST_BallSpawner"  ZENITH_BGRAPH_EXT;
		inline constexpr const char* szKILL_VOLUME    = "game:Graphs/ST_KillVolume"   ZENITH_BGRAPH_EXT;
		inline constexpr const char* szPRESSURE_PLATE = "game:Graphs/ST_PressurePlate" ZENITH_BGRAPH_EXT;
		inline constexpr const char* szDOOR           = "game:Graphs/ST_Door"         ZENITH_BGRAPH_EXT;
		inline constexpr const char* szBELL_RING      = "game:Graphs/ST_BellRing"     ZENITH_BGRAPH_EXT;
		inline constexpr const char* szBELL_LISTENER  = "game:Graphs/ST_BellListener" ZENITH_BGRAPH_EXT;
		inline constexpr const char* szTRAFFIC_LIGHT  = "game:Graphs/ST_TrafficLight" ZENITH_BGRAPH_EXT;
		inline constexpr const char* szUI_PLAYGROUND  = "game:Graphs/ST_UIPlayground" ZENITH_BGRAPH_EXT;
		inline constexpr const char* szDISPENSER      = "game:Graphs/ST_Dispenser"    ZENITH_BGRAPH_EXT;
		inline constexpr const char* szFLOW_SCORE     = "game:Graphs/ST_FlowScore"    ZENITH_BGRAPH_EXT;
		inline constexpr const char* szFLOW_PLATE     = "game:Graphs/ST_FlowPlate"    ZENITH_BGRAPH_EXT;
		inline constexpr const char* szNAV_WALKER     = "game:Graphs/ST_NavWalker"    ZENITH_BGRAPH_EXT;
		inline constexpr const char* szPREY           = "game:Graphs/ST_Prey"         ZENITH_BGRAPH_EXT;
	}

	// --- Scenes: build index + on-disk path -----------------------------------
	// The INDEX is graph contract: every LoadSceneByIndex node in ST_HubFlow /
	// ST_EscToHub names one, and Project_LoadInitialScene registers the same
	// index against the same path. Keep the two columns in lockstep.
	namespace Scenes
	{
		inline constexpr int32_t iHUB         = 0;
		inline constexpr int32_t iGYM_MOTION  = 1;
		inline constexpr int32_t iGYM_INPUT   = 2;
		inline constexpr int32_t iGYM_PHYSICS = 3;
		inline constexpr int32_t iGYM_EVENTS  = 4;
		inline constexpr int32_t iGYM_STATE   = 5;
		inline constexpr int32_t iGYM_UI      = 6;
		inline constexpr int32_t iGYM_FLOW    = 7;
		inline constexpr int32_t iGYM_AI      = 8;
		inline constexpr int32_t iCOUNT       = 9;

		inline constexpr const char* szHUB_PATH         = GAME_ASSETS_DIR "Scenes/Hub"         ZENITH_SCENE_EXT;
		inline constexpr const char* szGYM_MOTION_PATH  = GAME_ASSETS_DIR "Scenes/Gym_Motion"  ZENITH_SCENE_EXT;
		inline constexpr const char* szGYM_INPUT_PATH   = GAME_ASSETS_DIR "Scenes/Gym_Input"   ZENITH_SCENE_EXT;
		inline constexpr const char* szGYM_PHYSICS_PATH = GAME_ASSETS_DIR "Scenes/Gym_Physics" ZENITH_SCENE_EXT;
		inline constexpr const char* szGYM_EVENTS_PATH  = GAME_ASSETS_DIR "Scenes/Gym_Events"  ZENITH_SCENE_EXT;
		inline constexpr const char* szGYM_STATE_PATH   = GAME_ASSETS_DIR "Scenes/Gym_State"   ZENITH_SCENE_EXT;
		inline constexpr const char* szGYM_UI_PATH      = GAME_ASSETS_DIR "Scenes/Gym_UI"      ZENITH_SCENE_EXT;
		inline constexpr const char* szGYM_FLOW_PATH    = GAME_ASSETS_DIR "Scenes/Gym_Flow"    ZENITH_SCENE_EXT;
		inline constexpr const char* szGYM_AI_PATH      = GAME_ASSETS_DIR "Scenes/Gym_AI"      ZENITH_SCENE_EXT;

		// The scratch scene the ST_Ball prefab is captured from. Deliberately
		// NEVER saved -- it exists only so CreatePrefabFromSelected has a live
		// entity to capture, and is unloaded immediately after.
		inline constexpr const char* szPREFAB_SCRATCH   = "PrefabScratch";
	}

	// --- Entity names ----------------------------------------------------------
	// Every one of these is looked up by a FindEntityByName node, or targeted by
	// a scene recipe's SelectEntity, or both.
	namespace Entities
	{
		inline constexpr const char* szGAME_MANAGER    = "GameManager";
		inline constexpr const char* szSUN             = "Sun";
		inline constexpr const char* szKEY_LIGHT       = "KeyLight";
		inline constexpr const char* szFLOOR           = "Floor";
		inline constexpr const char* szPLATFORM        = "Platform";
		inline constexpr const char* szSPINNER         = "Spinner";
		inline constexpr const char* szPING_PONG       = "PingPong";
		inline constexpr const char* szBOBBER          = "Bobber";
		inline constexpr const char* szPLAYER_CUBE     = "PlayerCube";
		inline constexpr const char* szSPAWNER         = "Spawner";
		inline constexpr const char* szKILL_VOLUME     = "KillVolume";
		inline constexpr const char* szPRESSURE_PLATE  = "PressurePlate";
		inline constexpr const char* szGYM_DOOR        = "Gym_Door";
		inline constexpr const char* szBELL_LISTENER_A = "BellListener_A";
		inline constexpr const char* szBELL_LISTENER_B = "BellListener_B";
		inline constexpr const char* szBELL_LISTENER_C = "BellListener_C";
		inline constexpr const char* szLAMP_RED        = "Lamp_Red";
		inline constexpr const char* szLAMP_AMBER      = "Lamp_Amber";
		inline constexpr const char* szLAMP_GREEN      = "Lamp_Green";
		inline constexpr const char* szNOZZLE          = "Nozzle";
		inline constexpr const char* szPLATE           = "Plate";
		inline constexpr const char* szNAVMESH_HOLDER  = "NavMeshHolder";
		inline constexpr const char* szWALKER          = "Walker";
		inline constexpr const char* szPREY            = "Prey";

		// The prefab template entity (scratch scene only) and the runtime name
		// SpawnPrefab stamps on each ball it instantiates.
		inline constexpr const char* szBALL_TEMPLATE   = "ST_BallTemplate";
		inline constexpr const char* szBALL            = "Ball";
	}

	// --- UI element names ------------------------------------------------------
	// Addressed by SetUIText / SetUIColor / SetUIFillAmount / OnUIButtonClicked.
	namespace UINames
	{
		inline constexpr const char* szTITLE      = "Title";
		inline constexpr const char* szHINT       = "Hint";

		inline constexpr const char* szBTN_MOTION  = "Btn_Motion";
		inline constexpr const char* szBTN_INPUT   = "Btn_Input";
		inline constexpr const char* szBTN_PHYSICS = "Btn_Physics";
		inline constexpr const char* szBTN_EVENTS  = "Btn_Events";
		inline constexpr const char* szBTN_STATE   = "Btn_State";
		inline constexpr const char* szBTN_UI      = "Btn_UI";
		inline constexpr const char* szBTN_FLOW    = "Btn_Flow";
		inline constexpr const char* szBTN_AI      = "Btn_AI";

		inline constexpr const char* szSPAWNED    = "Spawned";
		inline constexpr const char* szKILLED     = "Killed";
		inline constexpr const char* szSTATE_NAME = "StateName";

		inline constexpr const char* szCOUNTER    = "Counter";
		inline constexpr const char* szCLOCK      = "Clock";
		inline constexpr const char* szBAR_FILL   = "BarFill";
		inline constexpr const char* szBTN_PLUS   = "BtnPlus";
		inline constexpr const char* szBTN_MINUS  = "BtnMinus";

		inline constexpr const char* szDISPENSED  = "Dispensed";
		inline constexpr const char* szNAV_STATE  = "NavState";
	}

	// --- UI colours ------------------------------------------------------------
	// The two RGBA values ST_UIPlayground's Branch paints onto BarFill. They live
	// here for the same reason every other literal in this file does: the builder
	// writes them into a SetUIColor node and ST_UIGym_Test reads them back off the
	// element, and a test that restated (1, 0.3, 0.2, 1) would prove only that the
	// test agrees with itself.
	namespace Colours
	{
		inline constexpr Zenith_Maths::Vector4 xBAR_HOT  = Zenith_Maths::Vector4(1.0f, 0.3f, 0.2f, 1.0f);
		inline constexpr Zenith_Maths::Vector4 xBAR_COOL = Zenith_Maths::Vector4(0.2f, 0.8f, 1.0f, 1.0f);
	}

	// --- Blackboard variable names ---------------------------------------------
	// Each is written by one node and read by another; a mismatch is silent
	// (the reader gets its default), which is exactly why they live here.
	namespace Vars
	{
		inline constexpr const char* szT            = "t";           // SineBob phase
		inline constexpr const char* szCOS_T        = "cosT";
		inline constexpr const char* szBOB_VEL      = "bobVel";

		inline constexpr const char* szMOVE_DIR     = "moveDir";     // PlayerMove
		inline constexpr const char* szMOVE_VEL     = "moveVel";

		inline constexpr const char* szUI_TARGET    = "uiTarget";    // packed EntityID
		inline constexpr const char* szSPAWN_COUNT  = "spawnCount";
		inline constexpr const char* szKILL_COUNT   = "killCount";
		inline constexpr const char* szOTHER        = "other";       // collision payload

		inline constexpr const char* szDOOR         = "door";        // packed EntityID
		inline constexpr const char* szLAMP         = "lamp";        // packed EntityID

		inline constexpr const char* szLIGHT        = "light";       // TrafficLight state

		inline constexpr const char* szCOUNT        = "count";       // UIPlayground
		inline constexpr const char* szCLOCK        = "clock";
		inline constexpr const char* szCYCLE        = "cycle";
		inline constexpr const char* szFILL01       = "fill01";
		inline constexpr const char* szHOT          = "hot";

		// --- Gym_Flow (ST_Dispenser + ST_FlowScore + ST_FlowPlate) ------------
		// Grouped rather than scattered because the flow gym is the one graph in
		// this game whose variables are read by a dozen different node families;
		// each comment names the node that WRITES it, since that is the half a
		// typo silently disables.
		inline constexpr const char* szBONUS        = "bonus";        // Once
		inline constexpr const char* szDISPENSED    = "dispensed";    // the Cooldown/Gate chain
		inline constexpr const char* szSCORE        = "score";        // the CallGraph CHILD, into the caller's board
		inline constexpr const char* szMODE         = "mode";         // the M key
		inline constexpr const char* szLABEL        = "label";        // SwitchOnInt's pins
		inline constexpr const char* szLABEL_INDEX  = "labelIndex";   // SwitchOnString's pins
		inline constexpr const char* szNOZZLE_REF   = "nozzleRef";    // packed EntityID
		inline constexpr const char* szMANAGER_REF  = "managerRef";   // packed EntityID (the plate's target)

		inline constexpr const char* szREADY        = "ready";        // the PlateArmed event
		inline constexpr const char* szARMED        = "armed";        // WaitForCondition's chain
		inline constexpr const char* szJAMMED       = "jammed";       // the J key
		inline constexpr const char* szNOT_JAMMED   = "notJammed";    // LogicBlackboardBool, m_bInvert = NOT
		inline constexpr const char* szCAN_DISPENSE = "canDispense";  // LogicBlackboardBool, N-ary AND
		inline constexpr const char* szALARM        = "alarm";        // the A key

		inline constexpr const char* szBAG          = "bag";          // ListAdd -- a LIST, not a value
		inline constexpr const char* szBAG_COUNT    = "bagCount";     // GetListCount
		inline constexpr const char* szITEM         = "item";         // ForEach's element
		inline constexpr const char* szIDX          = "idx";          // ForEach's index
		inline constexpr const char* szVISITED      = "visited";      // ForEach's body
		inline constexpr const char* szHEAD         = "head";         // GetListElement after ListRemoveAt
		inline constexpr const char* szSENTINEL     = "sentinel";     // the node AFTER a failed GetListElement

		inline constexpr const char* szNORMAL_RUNS  = "normalRuns";   // Selector pin 1
		inline constexpr const char* szALARM_RUNS   = "alarmRuns";    // Selector pin 0

		// --- Gym_AI (ST_NavWalker + ST_Prey) ---------------------------------
		inline constexpr const char* szNAV_READY    = "navReady";     // EnsureNavAgent succeeded
		inline constexpr const char* szGO           = "go";           // gates the NavMoveTo chain
		inline constexpr const char* szDEST         = "dest";         // vec3, also FindRandomReachablePoint's OUTPUT
		inline constexpr const char* szNAV_STATE    = "navState";     // ReadNavState: 0 none/1 pending/2 moving/3 arrived
		inline constexpr const char* szNAV_LEFT     = "navLeft";      // ReadNavState remaining distance
		inline constexpr const char* szNAV_VEL      = "navVel";       // ReadNavState velocity

		inline constexpr const char* szPREY_REF     = "preyRef";      // packed EntityID
		inline constexpr const char* szPERCEIVED    = "perceived";    // LIST of packed EntityIDs
		inline constexpr const char* szPERCEIVED_N  = "perceivedCount";
		inline constexpr const char* szPRIMARY      = "primary";      // QueryPrimaryPerceivedTarget
		inline constexpr const char* szPRIMARY_SEEN = "primarySeen";  // true only on a frame that query SUCCEEDED
		inline constexpr const char* szAWARENESS    = "awareness";
		inline constexpr const char* szHEARD_POS    = "heardPos";
		inline constexpr const char* szHEARD_SOURCE = "heardSource";
		inline constexpr const char* szFIRST_TARGET = "firstTarget";  // GetListElement(perceived, 0)
	}

	// --- Gym_Flow mode labels ---------------------------------------------------
	// SwitchOnInt writes one of these into Vars::szLABEL; SwitchOnString matches
	// the same value back to an index.
	//
	// ★ THERE IS NO szMODE_CASES CONSTANT HERE, DELIBERATELY. The comma list
	// SwitchOnString takes is COMPOSED in the builder from the three names
	// below. Events::szTL_STATE_NAMES is the older shape -- a hand-written
	// "Red,Green,Amber" beside six event spellings that "have to agree character
	// for character" -- and a list parsed VERBATIM (no trimming) is exactly the
	// place where agreeing-by-inspection fails quietly: an unmatched case takes
	// the DEFAULT pin, which is a legitimate outcome, so nothing errors.
	namespace Labels
	{
		inline constexpr const char* szRED   = "Red";
		inline constexpr const char* szGREEN = "Green";
		inline constexpr const char* szBLUE  = "Blue";
		inline constexpr const char* szNONE  = "none";
	}

	// --- Custom event names ----------------------------------------------------
	// The TL* names are NOT free-form: StateMachine composes them as
	// "<m_strEventPrefix>Enter_<stateName>" / "<prefix>Exit_<stateName>", so
	// szTL_PREFIX + szTL_STATE_NAMES and the six event strings below have to
	// agree character for character.
	namespace Events
	{
		inline constexpr const char* szOPEN_DOOR  = "OpenDoor";
		inline constexpr const char* szCLOSE_DOOR = "CloseDoor";
		inline constexpr const char* szBELL       = "Bell";
		inline constexpr const char* szPLATE_ARMED = "PlateArmed";

		inline constexpr const char* szTL_PREFIX      = "TL";
		inline constexpr const char* szTL_STATE_NAMES = "Red,Green,Amber";

		inline constexpr const char* szTL_ENTER_RED   = "TLEnter_Red";
		inline constexpr const char* szTL_EXIT_RED    = "TLExit_Red";
		inline constexpr const char* szTL_ENTER_GREEN = "TLEnter_Green";
		inline constexpr const char* szTL_EXIT_GREEN  = "TLExit_Green";
		inline constexpr const char* szTL_ENTER_AMBER = "TLEnter_Amber";
		inline constexpr const char* szTL_EXIT_AMBER  = "TLExit_Amber";
	}

	// --- Materials (registry paths) --------------------------------------------
	// Created unconditionally in Project_RegisterGameComponents -- NOT under
	// ZENITH_TOOLS -- because a committed scene serializes these paths and a
	// _False boot has to resolve them with no authoring pass.
	namespace Materials
	{
		inline constexpr const char* szFLOOR  = "game:Materials/ST_Floor"  ZENITH_MATERIAL_EXT;
		inline constexpr const char* szPLAYER = "game:Materials/ST_Player" ZENITH_MATERIAL_EXT;
		inline constexpr const char* szBALL   = "game:Materials/ST_Ball"   ZENITH_MATERIAL_EXT;
		inline constexpr const char* szPROP   = "game:Materials/ST_Prop"   ZENITH_MATERIAL_EXT;
		inline constexpr const char* szRED    = "game:Materials/ST_Red"    ZENITH_MATERIAL_EXT;
		inline constexpr const char* szAMBER  = "game:Materials/ST_Amber"  ZENITH_MATERIAL_EXT;
		inline constexpr const char* szGREEN  = "game:Materials/ST_Green"  ZENITH_MATERIAL_EXT;
	}

	// --- Generated meshes ------------------------------------------------------
	// The .zasset is the CPU mesh; the .zmodel is the bundle AddStep_LoadModel
	// references and the saved scene persists. Both are written by
	// Project_InitializeResources on a tools boot only.
	namespace Meshes
	{
		inline constexpr const char* szUNIT_CUBE_ASSET   = GAME_ASSETS_DIR "Meshes/UnitCube"   ZENITH_MESH_ASSET_EXT;
		inline constexpr const char* szUNIT_CUBE_MODEL   = GAME_ASSETS_DIR "Meshes/UnitCube"   ZENITH_MODEL_EXT;
		inline constexpr const char* szUNIT_SPHERE_ASSET = GAME_ASSETS_DIR "Meshes/UnitSphere" ZENITH_MESH_ASSET_EXT;
		inline constexpr const char* szUNIT_SPHERE_MODEL = GAME_ASSETS_DIR "Meshes/UnitSphere" ZENITH_MODEL_EXT;
	}

	// --- Navmesh ---------------------------------------------------------------
	// ★ THIS FILE IS COMMITTED, exactly like the eight .zscen. `.gitignore`
	// re-admits `**/*.znavmesh` under `Assets/` at any depth (ZM-D-145) because a
	// baked navmesh is small, byte-deterministic and loadable with no GPU -- so
	// it is a first-class tracked asset rather than a bake product, and the
	// cold-bake CI guard checks it the same way it checks the scenes.
	//
	// Two spellings, as usual: the ABSOLUTE path Project_InitializeResources
	// bakes to, and the `game:` ref AddStep_SetNavMeshAsset stores and the saved
	// scene persists.
	namespace Navmesh
	{
		inline constexpr const char* szBAKE_PATH = GAME_ASSETS_DIR "Navmesh/ST_Gym" ZENITH_NAVMESH_EXT;
		inline constexpr const char* szASSET     = "game:Navmesh/ST_Gym"            ZENITH_NAVMESH_EXT;

		// The baked footprint, half-extent in metres, centred on the origin. The
		// scene's floor prop is sized from this and ST_AIGym_Test asserts a
		// wander point lands inside it, so all three read the same number.
		inline constexpr float fHALF_EXTENT = 8.0f;
	}

	// --- Prefabs ---------------------------------------------------------------
	// szBALL_NAME is the prefab's logical name; szBALL_SAVE_PATH is where the
	// authoring pass writes it; szBALL_ASSET is the registry path the
	// SpawnPrefab node resolves at runtime. Three spellings of one artifact.
	namespace Prefabs
	{
		inline constexpr const char* szBALL_NAME      = "ST_Ball";
		inline constexpr const char* szBALL_SAVE_PATH = GAME_ASSETS_DIR "Prefabs/ST_Ball" ZENITH_PREFAB_EXT;
		inline constexpr const char* szBALL_ASSET     = "game:Prefabs/ST_Ball"            ZENITH_PREFAB_EXT;
	}
}

// ============================================================================
// The 20 graph builders. Each authors ONE .bgraph through a
// Zenith_EngineGraphBuilder over the plain builder it is handed.
//
// Deliberately NOT tools-gated -- unlike Combat's, which are static and live
// inside #ifdef ZENITH_TOOLS. ZENITH_INPUT_SIMULATOR is unconditional, so the
// hermetic tests reference every builder in _False configurations too;
// tools-gating them would break the D3D12_*_False link gate. They are also the
// SINGLE definition of each graph: the tools boot writes the .bgraph from them,
// and the three HERMETIC contract tests (C2/C3/C4) build them in-process, so
// THOSE THREE depend on no asset on disk.
//
// ★ THAT SCOPE IS THE WHOLE CLAIM -- the other nine tests DO depend on a prior
// authoring pass. They load the committed .zscen, whose slots reference the
// twenty .bgraph, the two generated meshes and the ball prefab, and all of
// those are gitignored BAKE PRODUCTS that only a *_True boot writes. That is
// why a fresh checkout's first build + run must be a tools config.
// ============================================================================
void BuildGraph_ST_EscToHub(Zenith_GraphBuilder& xBuilder);
void BuildGraph_ST_HubFlow(Zenith_GraphBuilder& xBuilder);
void BuildGraph_ST_Spin(Zenith_GraphBuilder& xBuilder);
void BuildGraph_ST_PingPong(Zenith_GraphBuilder& xBuilder);
void BuildGraph_ST_SineBob(Zenith_GraphBuilder& xBuilder);
void BuildGraph_ST_PlayerMove(Zenith_GraphBuilder& xBuilder);
void BuildGraph_ST_Jump(Zenith_GraphBuilder& xBuilder);
void BuildGraph_ST_BallSpawner(Zenith_GraphBuilder& xBuilder);
void BuildGraph_ST_KillVolume(Zenith_GraphBuilder& xBuilder);
void BuildGraph_ST_PressurePlate(Zenith_GraphBuilder& xBuilder);
void BuildGraph_ST_Door(Zenith_GraphBuilder& xBuilder);
void BuildGraph_ST_BellRing(Zenith_GraphBuilder& xBuilder);
void BuildGraph_ST_BellListener(Zenith_GraphBuilder& xBuilder);
void BuildGraph_ST_TrafficLight(Zenith_GraphBuilder& xBuilder);
void BuildGraph_ST_UIPlayground(Zenith_GraphBuilder& xBuilder);
void BuildGraph_ST_Dispenser(Zenith_GraphBuilder& xBuilder);
void BuildGraph_ST_FlowScore(Zenith_GraphBuilder& xBuilder);
void BuildGraph_ST_FlowPlate(Zenith_GraphBuilder& xBuilder);
void BuildGraph_ST_NavWalker(Zenith_GraphBuilder& xBuilder);
void BuildGraph_ST_Prey(Zenith_GraphBuilder& xBuilder);
