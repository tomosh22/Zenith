#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"
#include "Windows/Zenith_Windows_Window.h"
#include "Collections/Zenith_Vector.h"
#include "CityBuilder/Components/CB_CityManager_Behaviour.h"
#include "CityBuilder/Source/CB_RoadController.h"
#include "CityBuilder/Source/CB_RoadGraph.h"
#include "CityBuilder/Source/CB_Zoning.h"
#include "CityBuilder/Source/CB_BuildingPlacement.h"
#include "CityBuilder/Source/CB_BuildingDefs.h"
#include "CityBuilder/Source/CB_TerrainHeightfield.h"
#include "CityBuilder/Source/CB_Districts.h"
#include "CityBuilder/Source/CB_TransitLines.h"
#include "CityBuilder/Source/CB_Conduits.h"
#include "CityBuilder/Source/CB_SimSpeed.h"
#include "CityBuilder/Source/CB_Traffic.h"
#include <cmath>

// ============================================================================
// CB_HumanSession — the headline windowed playthrough. A "human" builds a whole
// Cities: Skylines-style city and exercises EVERY mechanic, interacting with the
// game PURELY through Zenith_InputSimulator — mouse moves/clicks, the mouse wheel
// and key presses. There is NOT ONE direct subsystem call (no PlaceService /
// PaintZone / HandleClick / SaveToFile etc.); the game's own tool + hotkey paths
// do all the work, exactly as a player at the keyboard + mouse would.
//
// It is authored as a flat SCRIPT of input actions (g_xScript) processed one per
// frame, because a Step runs reentrantly inside Zenith_MainLoop and may only use
// the simulator STATE-SETTERS (SimulateMousePosition / SimulateMouseButtonDown-Up
// / SimulateMouseWheel / SimulateKeyPress / SetKeyHeld) — the frame-advancing
// verbs (SimulateMouseClick / StepFrame) would re-enter BeginFrame and deadlock.
//
// Hotkeys used (all real game bindings — CB_ToolSystem + CB_CityManager):
//   1 res  2 com  3 ind  4 park  5 road  6 service(re-press cycles type)
//   7 power  8 water  9 bulldoze  T terraform  B district  L transit  K conduit
//   R road-class  - / = tax  G loan  , / . speed  P pause  F5/F9 save/load
//   F8 new city  F ignite-fire  F1-F4 district policies
//
// PROBE actions snapshot game state at key moments; Verify asserts a sizeable,
// solvent, fully-served city AND that every interactive mechanic fired.
// m_bRequiresGraphics = true (the tools need the live camera-unproject picker).
// ============================================================================

namespace
{
	// ---- input script -------------------------------------------------------
	enum EActType : uint8_t
	{
		A_MOVE, A_LDOWN, A_LUP, A_RDOWN, A_RUP,
		A_KEY, A_HOLD, A_UNHOLD, A_WHEEL, A_WAIT, A_PROBE
	};
	struct Act { EActType e; double fx; double fy; int i; float f; };

	Zenith_Vector<Act> g_xScript;
	uint32_t           g_uAct = 0;

	// ---- probe ids ----------------------------------------------------------
	enum EProbe
	{
		P_LOAN_BEFORE, P_LOAN_AFTER, P_TAX_AFTER,
		P_ROADCLASS_AFTER, P_TERRAIN_BEFORE, P_TERRAIN_AFTER,
		P_PAUSE, P_RESUME, P_SAVE_BEFORE, P_SAVE_AFTER,
		P_BULLDOZE_BEFORE, P_BULLDOZE_AFTER,
		P_ROADGRAPH,       // crossing grid → expect ONE connected component + junctions (auto-intersections)
		P_TRAFFIC_BARE,    // roads down, no buildings → expect ZERO traffic (demand-driven)
		P_TRAFFIC_CITY,    // grown city → expect routed trips + congestion
		P_GHOSTS_ON,       // R/C/I tool active, nothing painted → expect placement ghosts on open frontage
		P_GHOSTS_OFF,      // no zone tool active → expect ZERO placement ghosts
		P_FINAL
	};

	// ---- captured state / mechanic-fired flags (asserted in Verify) ---------
	float    g_fTreasuryBefore = 0.0f;
	uint32_t g_uTerrDirtyBefore = 0;
	uint32_t g_uSegBeforeBull  = 0;
	uint32_t g_uBldBeforeSave  = 0;
	uint32_t g_uSvcBeforeSave  = 0;

	bool g_bLoanWorked      = false;
	bool g_bTaxChanged      = false;
	bool g_bRoadClassWorked = false;
	bool g_bTerraformWorked = false;
	bool g_bPauseWorked     = false;
	bool g_bResumeWorked    = false;
	bool g_bSaveLoadWorked  = false;
	bool g_bBulldozeWorked  = false;

	uint32_t g_uFinalRoads      = 0;
	uint32_t g_uFinalBuildings  = 0;
	uint32_t g_uFinalPopulation = 0;
	uint32_t g_uFinalServices   = 0;
	uint32_t g_uFinalDistricts  = 0;
	uint32_t g_uFinalTransit    = 0;
	uint32_t g_uFinalConduits   = 0;
	float    g_fFinalTreasury   = 0.0f;
	float    g_fFinalHappy      = 0.0f;
	float    g_fFinalServed     = 0.0f;
	uint32_t g_uFinalFires      = 0;

	// Traffic telemetry — proves the SimCity / Cities:Skylines demand-driven model.
	bool     g_bTrafficBareEmpty = false;   // roads placed but no buildings → 0 cars
	uint32_t g_uTrafficActive    = 0;        // live trips once the city has grown
	uint32_t g_uTrafficStarted   = 0;        // cumulative trips dispatched (home→job/shop)
	uint32_t g_uTrafficCompleted = 0;        // cumulative trips that reached their destination
	uint32_t g_uTrafficMaxLoad   = 0;        // vehicles on the busiest segment
	uint32_t g_uTrafficCongested = 0;        // segments over capacity

	// Road-graph telemetry — proves auto-intersections connect a CROSSING grid (SimCity/C:S).
	uint32_t g_uRoadComponents = 99;         // connected components (1 = one fully-connected network)
	uint32_t g_uRoadJunctions  = 0;          // nodes where >=3 roads meet (intersections)
	uint32_t g_uRoadNodes      = 0;
	uint32_t g_uRoadSegments   = 0;

	// Placement-ghost telemetry — proves the R/C/I tool renders ghosts of available zones.
	uint32_t g_uGhostsOn  = 0;               // ghosts drawn while the residential tool is active (open frontage)
	uint32_t g_uGhostsOff = 999;             // ghosts drawn with NO zone tool (sentinel → must end == 0)

	const float fHS_FIXED_DT = 1.0f / 60.0f;

	// ---- script append helpers ---------------------------------------------
	void Mv(double x, double y) { g_xScript.PushBack(Act{ A_MOVE, x, y, 0, 0.0f }); }
	void Wt(int n)              { g_xScript.PushBack(Act{ A_WAIT, 0, 0, n, 0.0f }); }
	void Kp(int k)             { g_xScript.PushBack(Act{ A_KEY, 0, 0, k, 0.0f }); Wt(2); }   // press + settle
	void Hold(int k)           { g_xScript.PushBack(Act{ A_HOLD, 0, 0, k, 0.0f }); }
	void Unhold(int k)         { g_xScript.PushBack(Act{ A_UNHOLD, 0, 0, k, 0.0f }); }
	void Wheel(float w)        { g_xScript.PushBack(Act{ A_WHEEL, 0, 0, 0, w }); }
	void Probe(int id)         { g_xScript.PushBack(Act{ A_PROBE, 0, 0, id, 0.0f }); }

	// A left-click at a screen fraction: position, settle, press, release.
	void ClickL(double x, double y)
	{
		Mv(x, y); Wt(2);
		g_xScript.PushBack(Act{ A_LDOWN, 0, 0, 0, 0.0f }); Wt(1);
		g_xScript.PushBack(Act{ A_LUP, 0, 0, 0, 0.0f });   Wt(2);
	}
	// A right-click (the road tool's "finish road").
	void ClickR()
	{
		g_xScript.PushBack(Act{ A_RDOWN, 0, 0, 0, 0.0f }); Wt(1);
		g_xScript.PushBack(Act{ A_RUP, 0, 0, 0, 0.0f });   Wt(2);
	}
	// Draw a straight road between two screen fractions (2 node clicks + finish).
	void Road(double ax, double ay, double bx, double by)
	{
		ClickL(ax, ay); ClickL(bx, by); ClickR();
	}
	// Paint-drag the active zone brush along a poly-path (LMB held while moving).
	void ZoneDrag(const double* pPts, int iN)
	{
		Mv(pPts[0], pPts[1]); Wt(1);
		g_xScript.PushBack(Act{ A_LDOWN, 0, 0, 0, 0.0f }); Wt(2);
		for (int i = 1; i < iN; ++i) { Mv(pPts[i * 2], pPts[i * 2 + 1]); Wt(2); }
		g_xScript.PushBack(Act{ A_LUP, 0, 0, 0, 0.0f }); Wt(2);
	}

	void SetMouseFrac(double fXFrac, double fYFrac)
	{
		// Spread the WHOLE scripted layout out from screen-centre. Now that zones are placed with
		// correct clearance (no lots on the road / in intersections / overlapping each other), a
		// tight crossing grid leaves almost no buildable frontage between roads — so scale every
		// scripted click out so the grid's blocks are large enough for two-sided frontage and the
		// city grows to a sizeable size. Topology (the 3x3 crossing → 9 junctions) is unchanged.
		constexpr double SPREAD = 1.75;
		fXFrac = 0.5 + (fXFrac - 0.5) * SPREAD;
		fYFrac = 0.5 + (fYFrac - 0.5) * SPREAD;
		int32_t iW = 0, iH = 0;
		Zenith_Window::GetInstance()->GetSize(iW, iH);
		if (iW <= 0) { iW = 1280; }
		if (iH <= 0) { iH = 720; }
		Zenith_InputSimulator::SimulateMousePosition(fXFrac * static_cast<double>(iW),
		                                             fYFrac * static_cast<double>(iH));
	}

	// ---- the playthrough script --------------------------------------------
	// Screen-fraction layout around the default camera framing (target 2048,2048).
	// The picker unprojects each fraction to the ground; the city forms there.
	void BuildScript()
	{
		g_xScript.Clear();
		g_uAct = 0;

		// 0) Fresh city, then fund it with loans + raise the tax rate.
		Kp(ZENITH_KEY_F8); Wt(4);
		Probe(P_LOAN_BEFORE);
		Kp(ZENITH_KEY_G); Kp(ZENITH_KEY_G);                 // two development loans
		Probe(P_LOAN_AFTER);
		Kp(ZENITH_KEY_EQUAL); Kp(ZENITH_KEY_EQUAL);          // raise tax twice
		Probe(P_TAX_AFTER);

		// 1) A CROSSING road grid — horizontals + verticals that CROSS mid-span and do NOT share any
		//    endpoint. WITHOUT auto-intersections these are 6 disconnected roads; WITH them (SimCity/
		//    C:S), every crossing splits both roads + inserts a shared junction node, so the grid
		//    becomes ONE connected component — proven by the P_ROADGRAPH probe below. Horizontals
		//    small class, verticals medium (exercises the road-class hotkey R).
		Kp(ZENITH_KEY_5);
		const double aGY[3] = { 0.42, 0.50, 0.58 };   // horizontal road y-positions
		const double aGX[3] = { 0.42, 0.50, 0.58 };   // vertical road x-positions
		for (int i = 0; i < 3; ++i) { Road(0.34, aGY[i], 0.66, aGY[i]); }   // 3 horizontals (parallel, disjoint)
		Kp(ZENITH_KEY_R);                                                   // small -> medium
		for (int i = 0; i < 3; ++i) { Road(aGX[i], 0.38, aGX[i], 0.62); }   // 3 verticals → 9 crossings auto-junction
		Probe(P_ROADCLASS_AFTER);
		Probe(P_ROADGRAPH);   // the crossing grid must now be ONE connected component with junctions
		// Traffic is demand-driven: the road grid exists but NO buildings yet → there must be ZERO
		// cars (the old fixed pool would have spawned dozens here). Let the sim run, then probe.
		Wt(40);
		Probe(P_TRAFFIC_BARE);

		// 2) Zones — sweep the residential brush across the grid, then C/I clusters.
		Kp(ZENITH_KEY_1);
		// Placement-ghost affordance (SimCity/C:S): the residential tool is now active but NOTHING is
		// painted yet, so every open frontage lot ghosts green. Hold a good beat so it renders steadily
		// at the grid framing (a stable window for the screenshot) and probe the ghost count — it must
		// be > 0.
		Wt(360);
		Probe(P_GHOSTS_ON);
		const double aRes[] = { 0.36,0.44, 0.46,0.52, 0.56,0.44, 0.62,0.52, 0.50,0.56, 0.40,0.56 };
		ZoneDrag(aRes, 6);
		Kp(ZENITH_KEY_2);
		const double aCom[] = { 0.60,0.46, 0.64,0.52, 0.62,0.56 };
		ZoneDrag(aCom, 3);
		Kp(ZENITH_KEY_3);
		const double aInd[] = { 0.36,0.46, 0.34,0.52, 0.38,0.56 };
		ZoneDrag(aInd, 3);

		// 3) Utilities — power plants + water towers around the grid.
		Kp(ZENITH_KEY_7);
		ClickL(0.34, 0.40); ClickL(0.66, 0.40); ClickL(0.50, 0.62);
		Kp(ZENITH_KEY_8);
		ClickL(0.34, 0.60); ClickL(0.66, 0.60); ClickL(0.50, 0.39);

		// 4) Services — re-press 6 to cycle police->fire->hospital->school->landfill
		//    ->sewage->bus->post, placing each with a click.
		Kp(ZENITH_KEY_6); ClickL(0.44, 0.46);               // police
		Kp(ZENITH_KEY_6); ClickL(0.56, 0.54);               // fire
		Kp(ZENITH_KEY_6); ClickL(0.50, 0.48);               // hospital
		Kp(ZENITH_KEY_6); ClickL(0.46, 0.54);               // school
		Kp(ZENITH_KEY_6); ClickL(0.33, 0.44);               // landfill
		Kp(ZENITH_KEY_6); ClickL(0.67, 0.56);               // sewage
		Kp(ZENITH_KEY_6); ClickL(0.42, 0.50);               // bus depot
		Kp(ZENITH_KEY_6); ClickL(0.58, 0.50);               // post office

		// 5) Parks (key 4 places park amenities on click).
		Kp(ZENITH_KEY_4);
		ClickL(0.48, 0.45); ClickL(0.52, 0.55); ClickL(0.40, 0.52);

		// 6) Districts + ordinances (paint a district, toggle the four policies).
		Kp(ZENITH_KEY_B);
		ClickL(0.40, 0.50);
		Kp(ZENITH_KEY_F1); Kp(ZENITH_KEY_F2); Kp(ZENITH_KEY_F3); Kp(ZENITH_KEY_F4);

		// 7) Transit line (place a run of stops).
		Kp(ZENITH_KEY_L);
		ClickL(0.40, 0.50); ClickL(0.50, 0.50); ClickL(0.60, 0.50);

		// 8) Utility conduits (lay a chain).
		Kp(ZENITH_KEY_K);
		ClickL(0.46, 0.52); ClickL(0.50, 0.54); ClickL(0.54, 0.52);

		// 9) Terraform — raise (LMB held) then lower (RMB held) the ground.
		Kp(ZENITH_KEY_T);
		Probe(P_TERRAIN_BEFORE);
		Mv(0.36, 0.63); Wt(2);                               // lower-screen = near terrain → the picker lands
		g_xScript.PushBack(Act{ A_LDOWN, 0, 0, 0, 0.0f }); Wt(28);   // hold LMB → raise
		g_xScript.PushBack(Act{ A_LUP, 0, 0, 0, 0.0f });   Wt(2);
		Mv(0.64, 0.63); Wt(2);
		g_xScript.PushBack(Act{ A_RDOWN, 0, 0, 0, 0.0f }); Wt(28);   // hold RMB → lower
		g_xScript.PushBack(Act{ A_RUP, 0, 0, 0, 0.0f });   Wt(2);
		Probe(P_TERRAIN_AFTER);

		// 10) Speed up to ULTRA and watch the city grow. The camera is left UNTOUCHED through all
		//     building/editing so screen fractions keep mapping to the same world points (a mid-build
		//     orbit would move the ground out from under the later bulldoze/terraform clicks). The only
		//     camera move is the final tour, after every edit is done.
		Kp(ZENITH_KEY_0);                                    // deselect tool (no stray edits during growth)
		Wt(4);
		Probe(P_GHOSTS_OFF);   // no zone tool active → ZERO placement ghosts (affordance only shows for R/C/I)
		Kp(ZENITH_KEY_PERIOD); Kp(ZENITH_KEY_PERIOD);        // normal -> fast -> ultra
		Wt(720);

		// 11) Second growth wave — zone more residential, keep growing.
		Kp(ZENITH_KEY_1);
		const double aRes2[] = { 0.40,0.42, 0.50,0.42, 0.60,0.42, 0.40,0.58, 0.50,0.58, 0.60,0.58 };
		ZoneDrag(aRes2, 6);
		Wt(360);

		// 12) Disaster drill — ignite a fire at the covered centre; stations contain it.
		Mv(0.50, 0.50); Wt(2); Kp(ZENITH_KEY_F);
		Wt(180);

		// 13) Pause + resume; nudge the speed down.
		Kp(ZENITH_KEY_P); Wt(6); Probe(P_PAUSE);
		Kp(ZENITH_KEY_P); Wt(6); Probe(P_RESUME);
		Kp(ZENITH_KEY_COMMA);

		// 14) Bulldoze a road, then rebuild it.
		Kp(ZENITH_KEY_9);
		Probe(P_BULLDOZE_BEFORE);
		ClickL(aGX[2], 0.60);                                // bulldoze a vertical road's lower sub-segment
		Wt(4);
		Probe(P_BULLDOZE_AFTER);
		Kp(ZENITH_KEY_5);
		Road(aGX[2], 0.38, aGX[2], 0.62);                    // rebuild the vertical (re-junctions the crossings)

		// 15) Save + load round-trip (pause first so counts are stable across it).
		Kp(ZENITH_KEY_P); Wt(6);                             // pause
		Probe(P_SAVE_BEFORE);
		Kp(ZENITH_KEY_F5); Wt(6);                            // quick-save
		Kp(ZENITH_KEY_F9); Wt(8);                            // quick-load
		Probe(P_SAVE_AFTER);
		Kp(ZENITH_KEY_P); Wt(6);                             // resume

		// 16) Settle + a camera tour for the screenshots (zoom + orbit).
		Wt(150);
		Wheel(4.0f); Wt(20);
		Hold(ZENITH_KEY_E); Wt(150); Unhold(ZENITH_KEY_E);
		Wt(30);
		Probe(P_TRAFFIC_CITY);   // grown, populated city → routed home→job trips + congestion
		Probe(P_FINAL);
	}

	void RunProbe(int id)
	{
		CB_BuildingPlacement*  pxB = CB_CityManager_Behaviour::GetActiveBuild();
		CB_RoadController*     pxC = CB_CityManager_Behaviour::GetActiveRoadController();
		CB_TerrainHeightfield* pxF = CB_CityManager_Behaviour::GetActiveHeightfield();
		CB_Districts*          pxD = CB_CityManager_Behaviour::GetActiveDistricts();
		CB_TransitLines*       pxT = CB_CityManager_Behaviour::GetActiveTransit();
		CB_Conduits*           pxK = CB_CityManager_Behaviour::GetActiveConduits();
		CB_CityManager_Behaviour* pxM = CB_CityManager_Behaviour::GetActive();
		if (pxB == nullptr || pxC == nullptr || pxF == nullptr || pxM == nullptr) { return; }

		switch (id)
		{
		case P_LOAN_BEFORE:    g_fTreasuryBefore = pxB->GetTreasury(); break;
		case P_LOAN_AFTER:     g_bLoanWorked = (pxB->GetTreasury() > g_fTreasuryBefore + 1000.0f); break;
		case P_TAX_AFTER:      g_bTaxChanged = (std::fabs(pxB->GetTaxRate() - 1.0f) > 0.05f); break;
		case P_ROADCLASS_AFTER:g_bRoadClassWorked = (pxC->GetRoadClass() != CB_ROADCLASS_SMALL); break;
		case P_TERRAIN_BEFORE: g_uTerrDirtyBefore = pxF->GetDirtyCount(); break;
		case P_TERRAIN_AFTER:  g_bTerraformWorked = (pxF->GetDirtyCount() > g_uTerrDirtyBefore); break;
		case P_PAUSE:          g_bPauseWorked  = (pxM->GetSpeed() == CB_SIM_PAUSED); break;
		case P_RESUME:         g_bResumeWorked = (pxM->GetSpeed() != CB_SIM_PAUSED); break;
		case P_SAVE_BEFORE:    g_uBldBeforeSave = pxB->GetActiveBuildings(); g_uSvcBeforeSave = pxB->GetActiveServices(); break;
		case P_SAVE_AFTER:     g_bSaveLoadWorked = (pxB->GetActiveBuildings() == g_uBldBeforeSave && pxB->GetActiveServices() == g_uSvcBeforeSave && g_uBldBeforeSave > 0u); break;
		case P_BULLDOZE_BEFORE:g_uSegBeforeBull = pxC->GetGraph().GetActiveSegmentCount(); break;
		case P_BULLDOZE_AFTER: g_bBulldozeWorked = (pxC->GetGraph().GetActiveSegmentCount() < g_uSegBeforeBull); break;
		case P_ROADGRAPH:
		{
			const CB_RoadGraph& xG = pxC->GetGraph();
			g_uRoadComponents = xG.CountConnectedComponents();
			g_uRoadJunctions  = xG.CountJunctions();
			g_uRoadNodes      = xG.GetActiveNodeCount();
			g_uRoadSegments   = xG.GetActiveSegmentCount();
			break;
		}
		case P_TRAFFIC_BARE:
		{
			// Roads exist but no buildings → a demand-driven model carries ZERO traffic.
			const CB_Traffic* pxTr = CB_CityManager_Behaviour::GetActiveTraffic();
			g_bTrafficBareEmpty = (pxTr != nullptr && pxTr->GetActiveVehicleCount() == 0u);
			break;
		}
		case P_TRAFFIC_CITY:
		{
			const CB_Traffic* pxTr = CB_CityManager_Behaviour::GetActiveTraffic();
			if (pxTr != nullptr)
			{
				const CB_TrafficStats& xS = pxTr->GetStats();
				g_uTrafficActive    = xS.m_uActive;
				g_uTrafficStarted   = xS.m_uTripsStarted;
				g_uTrafficCompleted = xS.m_uTripsCompleted;
				g_uTrafficMaxLoad   = xS.m_uMaxSegmentLoad;
				g_uTrafficCongested = xS.m_uCongestedSegs;
			}
			break;
		}
		case P_GHOSTS_ON:  g_uGhostsOn  = CB_CityManager_Behaviour::GetLastGhostCount(); break;
		case P_GHOSTS_OFF: g_uGhostsOff = CB_CityManager_Behaviour::GetLastGhostCount(); break;
		case P_FINAL:
			g_uFinalRoads      = pxC->GetGraph().GetActiveSegmentCount();
			g_uFinalBuildings  = pxB->GetActiveBuildings();
			g_uFinalPopulation = pxB->GetPopulation();
			g_uFinalServices   = pxB->GetActiveServices();
			g_uFinalDistricts  = (pxD != nullptr) ? pxD->GetSlotCount() : 0u;
			g_uFinalTransit    = (pxT != nullptr) ? pxT->GetStopCount() : 0u;
			g_uFinalConduits   = (pxK != nullptr) ? pxK->GetCount() : 0u;
			g_fFinalTreasury   = pxB->GetTreasury();
			g_fFinalHappy      = pxB->GetHappiness();
			g_fFinalServed     = pxB->GetServedFraction();
			g_uFinalFires      = pxB->GetFiresDestroyed();
			break;
		default: break;
		}
	}
}

static void Setup_CB_HumanSession()
{
	g_uAct = 0;
	g_fTreasuryBefore = 0.0f; g_uTerrDirtyBefore = 0; g_uSegBeforeBull = 0;
	g_uBldBeforeSave = 0; g_uSvcBeforeSave = 0;
	g_bLoanWorked = g_bTaxChanged = g_bRoadClassWorked = g_bTerraformWorked = false;
	g_bPauseWorked = g_bResumeWorked = g_bSaveLoadWorked = g_bBulldozeWorked = false;
	g_uFinalRoads = g_uFinalBuildings = g_uFinalPopulation = g_uFinalServices = 0;
	g_uFinalDistricts = g_uFinalTransit = g_uFinalConduits = g_uFinalFires = 0;
	g_fFinalTreasury = g_fFinalHappy = g_fFinalServed = 0.0f;
	g_bTrafficBareEmpty = false;
	g_uTrafficActive = g_uTrafficStarted = g_uTrafficCompleted = g_uTrafficMaxLoad = g_uTrafficCongested = 0;
	g_uRoadComponents = 99; g_uRoadJunctions = g_uRoadNodes = g_uRoadSegments = 0;
	g_uGhostsOn = 0; g_uGhostsOff = 999;
	Zenith_InputSimulator::SetFixedDt(fHS_FIXED_DT);
	BuildScript();
}

static bool Step_CB_HumanSession(int /*iFrame*/)
{
	// Wait for the manager + every subsystem to come up before driving input.
	CB_CityManager_Behaviour* pxMgr = CB_CityManager_Behaviour::GetActive();
	if (pxMgr == nullptr
	    || CB_CityManager_Behaviour::GetActiveRoadController() == nullptr
	    || CB_CityManager_Behaviour::GetActiveBuild() == nullptr
	    || CB_CityManager_Behaviour::GetActiveHeightfield() == nullptr)
	{
		return true;   // keep waiting (bounded by maxFrames)
	}

	if (g_uAct >= g_xScript.GetSize())
	{
		return false;  // script complete
	}

	Act& a = g_xScript.Get(g_uAct);
	switch (a.e)
	{
	case A_MOVE:   SetMouseFrac(a.fx, a.fy); ++g_uAct; break;
	case A_LDOWN:  Zenith_InputSimulator::SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_LEFT);  ++g_uAct; break;
	case A_LUP:    Zenith_InputSimulator::SimulateMouseButtonUp(ZENITH_MOUSE_BUTTON_LEFT);    ++g_uAct; break;
	case A_RDOWN:  Zenith_InputSimulator::SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_RIGHT); ++g_uAct; break;
	case A_RUP:    Zenith_InputSimulator::SimulateMouseButtonUp(ZENITH_MOUSE_BUTTON_RIGHT);   ++g_uAct; break;
	case A_KEY:    Zenith_InputSimulator::SimulateKeyPress(a.i); ++g_uAct; break;
	case A_HOLD:   Zenith_InputSimulator::SetKeyHeld(a.i, true);  ++g_uAct; break;
	case A_UNHOLD: Zenith_InputSimulator::SetKeyHeld(a.i, false); ++g_uAct; break;
	case A_WHEEL:  Zenith_InputSimulator::SimulateMouseWheel(a.f); ++g_uAct; break;
	case A_WAIT:   if (a.i <= 0) { ++g_uAct; } else { --a.i; } break;
	case A_PROBE:  RunProbe(a.i); ++g_uAct; break;
	default:       ++g_uAct; break;
	}
	return true;
}

static bool Verify_CB_HumanSession()
{
	Zenith_Log(LOG_CATEGORY_GAMEPLAY,
		"CB_HumanSession: %u roads, %u buildings, pop %u, %u services, %u districts, %u stops, %u conduits, $%.0f, happy %.2f, served %.2f, firesLost %u",
		g_uFinalRoads, g_uFinalBuildings, g_uFinalPopulation, g_uFinalServices,
		g_uFinalDistricts, g_uFinalTransit, g_uFinalConduits, g_fFinalTreasury,
		g_fFinalHappy, g_fFinalServed, g_uFinalFires);
	Zenith_Log(LOG_CATEGORY_GAMEPLAY,
		"CB_HumanSession mechanics: loan=%d tax=%d roadClass=%d terraform=%d pause=%d resume=%d bulldoze=%d saveLoad=%d",
		g_bLoanWorked, g_bTaxChanged, g_bRoadClassWorked, g_bTerraformWorked,
		g_bPauseWorked, g_bResumeWorked, g_bBulldozeWorked, g_bSaveLoadWorked);
	Zenith_Log(LOG_CATEGORY_GAMEPLAY,
		"CB_HumanSession traffic: bareRoadsEmpty=%d active=%u tripsStarted=%u tripsCompleted=%u maxSegLoad=%u congestedSegs=%u",
		g_bTrafficBareEmpty, g_uTrafficActive, g_uTrafficStarted, g_uTrafficCompleted, g_uTrafficMaxLoad, g_uTrafficCongested);
	Zenith_Log(LOG_CATEGORY_GAMEPLAY,
		"CB_HumanSession roadgraph: components=%u junctions=%u nodes=%u segments=%u",
		g_uRoadComponents, g_uRoadJunctions, g_uRoadNodes, g_uRoadSegments);
	Zenith_Log(LOG_CATEGORY_GAMEPLAY,
		"CB_HumanSession ghosts: residentialToolGhosts=%u noToolGhosts=%u",
		g_uGhostsOn, g_uGhostsOff);

	bool bOk = true;
	auto Fail = [&](const char* szMsg) { Zenith_Log(LOG_CATEGORY_GAMEPLAY, "CB_HumanSession FAIL: %s", szMsg); bOk = false; };

	// A sizeable, working city, all built through the input path.
	if (g_uFinalRoads < 7)        { Fail("too few roads"); }
	if (g_uFinalBuildings < 30)   { Fail("city not sizeable"); }
	if (g_uFinalPopulation < 400) { Fail("low population"); }
	if (g_uFinalServices < 14)    { Fail("not all service types placed"); }
	if (g_uFinalDistricts < 1)    { Fail("no district painted"); }
	if (g_uFinalTransit < 2)      { Fail("no transit stops"); }
	if (g_uFinalConduits < 2)     { Fail("no conduits laid"); }
	if (g_fFinalServed < 0.3f)    { Fail("poor service coverage"); }
	// The disaster system is live: the F-key ignites a fire at the covered centre (stations
	// contain it) and auto-disasters burn the occasional UNCOVERED fringe building. So some loss
	// is expected + proves the mechanic runs; only a runaway count means fire response is broken.
	if (g_uFinalFires > 8)        { Fail("runaway fire losses (response broken)"); }

	// Every interactive mechanic fired (each driven only by simulated input).
	if (!g_bLoanWorked)      { Fail("loan (G) didn't add funds"); }
	if (!g_bTaxChanged)      { Fail("tax (=) didn't change the rate"); }
	if (!g_bRoadClassWorked) { Fail("road-class (R) didn't change"); }
	if (!g_bTerraformWorked) { Fail("terraform (T) didn't edit the ground"); }
	if (!g_bPauseWorked)     { Fail("pause (P) didn't pause"); }
	if (!g_bResumeWorked)    { Fail("resume (P) didn't resume"); }
	if (!g_bBulldozeWorked)  { Fail("bulldoze (9) didn't remove a road"); }
	if (!g_bSaveLoadWorked)  { Fail("save/load (F5/F9) didn't round-trip"); }

	// Traffic behaves like SimCity / Cities: Skylines — emergent, routed, congesting:
	if (!g_bTrafficBareEmpty)     { Fail("traffic spawned on bare roads (NOT demand-driven)"); }
	if (g_uTrafficActive < 1u)    { Fail("no traffic emerged in the grown city"); }
	if (g_uTrafficStarted < 1u)   { Fail("no home→job/shop trips were dispatched"); }
	if (g_uTrafficCompleted < 1u) { Fail("no trip ever reached its destination (routing broken)"); }
	if (g_uTrafficMaxLoad < 2u)   { Fail("cars never shared a road (no congestion forms)"); }

	// Road system parity (SimCity/C:S): the CROSSING grid auto-junctioned into ONE connected network.
	if (g_uRoadComponents != 1u)  { Fail("crossing grid did NOT auto-connect into one network (intersections broken)"); }
	if (g_uRoadJunctions < 4u)    { Fail("no intersections formed at the road crossings"); }

	// Placement-zone ghosts: selecting an R/C/I tool ghosts every open frontage lot (so the player
	// sees where a zone can go); deselecting the tool clears them. Both proven via GetLastGhostCount.
	if (g_uGhostsOn < 1u)   { Fail("no placement-zone ghosts rendered when the residential tool was active"); }
	if (g_uGhostsOff != 0u) { Fail("placement ghosts rendered with no zone tool active (should be none)"); }
	return bOk;
}

static const Zenith_AutomatedTest g_xHumanSession = { "CB_HumanSession", &Setup_CB_HumanSession, &Step_CB_HumanSession, &Verify_CB_HumanSession, 4000, true };
ZENITH_AUTOMATED_TEST_REGISTER(g_xHumanSession);

#endif // ZENITH_INPUT_SIMULATOR
