#pragma once

#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "CityBuilder/Source/CB_TerrainHeightfield.h"
#include "CityBuilder/Source/CB_CityGrid.h"
#include "CityBuilder/Source/CB_RoadNetwork.h"
#include "CityBuilder/Source/CB_BuildingManager.h"
#include "CityBuilder/Source/CB_ServiceManager.h"
#include "CityBuilder/Source/CB_EconomyManager.h"
#include "CityBuilder/Source/CB_CitizenManager.h"
#include "CityBuilder/Source/CB_SimulationTick.h"
#include "CityBuilder/Source/CB_ToolSystem.h"
#include "CityBuilder/Source/CB_RoadController.h"
#include "CityBuilder/Source/CB_Zoning.h"
#include "CityBuilder/Source/CB_BuildingPlacement.h"
#include "CityBuilder/Source/CB_Districts.h"
#include "CityBuilder/Source/CB_TransitLines.h"
#include "CityBuilder/Source/CB_Conduits.h"
#include "CityBuilder/Source/CB_Traffic.h"
#include "CityBuilder/Source/CB_TerrainGen.h"
#include "CityBuilder/Source/CB_RoadTerrain.h"
#include "Flux/Primitives/Flux_PrimitivesImpl.h"   // district overlay rings
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "CityBuilder/Source/CB_PresentationView.h"
#include "CityBuilder/Source/CB_Config.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "UI/Zenith_UIText.h"
#include "Input/Zenith_Input.h"
#include "Core/Zenith_CommandLine.h"
#include "CityBuilder/Source/CB_Events.h"
#include "ZenithECS/Zenith_EventSystem.h"
#include <cstdio>

// ============================================================================
// CB_CityManager_Behaviour — top-level city orchestrator.
//
// In later phases this owns the spatial grid, zone/road/building managers, the
// citizen pool, service coverage, the economy, and drives the fixed-timestep
// simulation tick. Sub-systems are stored as members, initialised in OnStart
// (the CityManager entity is deserialized from the saved scene, so OnStart —
// not OnAwake — is the reliable init point), torn down in OnDestroy.
//
// Phase 1: minimal lifecycle plus a liveness counter the CB_Boot gate asserts
// on (proves the scene loaded, the behaviour attached, and frames are ticking).
// ============================================================================
class CB_CityManager_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(CB_CityManager_Behaviour)

	CB_CityManager_Behaviour() = delete;
	CB_CityManager_Behaviour(Zenith_Entity& /*xParentEntity*/) {}

	void OnStart() ZENITH_FINAL override
	{
		// Reset per-run liveness state (the harness reloads scene 0 between
		// batched tests, so OnStart re-fires on a fresh CityManager).
		s_uUpdateCount = 0;
		s_bStarted = true;
		m_uMilestoneMask = 0;

		// Terrain heightfield covering the 4km world (257 samples @ 16m = 4096m,
		// origin 0). Shaped to the shared CB_TerrainGen rolling hills — the SAME
		// function the offline bake uses, at the bake's 512 height scale — so roads
		// + buildings (GetHeightAt) conform to the rendered GPU terrain mesh.
		// Runtime-editable + authoritative for all gameplay height queries.
		m_xHeightfield.Init(257, 257, 16.0f, 0.0f, 0.0f, CB_TerrainGen::HEIGHT_SCALE);
		for (uint32_t uZ = 0; uZ < m_xHeightfield.GetSamplesZ(); ++uZ)
		{
			for (uint32_t uX = 0; uX < m_xHeightfield.GetSamplesX(); ++uX)
			{
				m_xHeightfield.SetNormalized(uX, uZ,
					CB_TerrainGen::HillNorm(static_cast<float>(uX) * 16.0f, static_cast<float>(uZ) * 16.0f));
			}
		}
		s_pxActiveHeightfield = &m_xHeightfield;

		// City grid: 1024x1024 cells @ 4m = 4096m world, origin 0 (matches the
		// terrain footprint). 16MB. Authoritative spatial model for zones, roads,
		// buildings and derived data.
		m_xGrid.Initialize(1024, 1024, 4.0f, 0.0f, 0.0f);
		s_pxActiveGrid = &m_xGrid;

		m_xRoads.Initialize(&m_xGrid);
		s_pxActiveRoads = &m_xRoads;

		m_xBuildings.Initialize(&m_xGrid);
		m_xServices.Initialize(&m_xGrid);
		m_xEconomy.Initialize();
		m_xCitizens.Initialize(&m_xGrid, &m_xRoads);
		m_xSim.Initialize(&m_xGrid, &m_xRoads, &m_xBuildings, &m_xServices, &m_xEconomy, &m_xCitizens);

		s_pxActive = this;

		// Free-form spline road network (Cities: Skylines rebuild). Owns its own
		// graph; the legacy grid CB_RoadNetwork stays initialised but idle behind
		// CB_USE_LEGACY_GRID during the transition.
		m_xRoadCtrl.Reset();
		s_pxActiveRoadCtrl = &m_xRoadCtrl;

		// Road-relative zoning (frontage lots) — Cities: Skylines rebuild.
		m_xZoning.Reset();
		s_pxActiveZoning = &m_xZoning;

		// City districts + ordinances (Cities: Skylines policies). The sim reads them.
		m_xDistricts.Reset();
		m_xBuild.SetDistricts(&m_xDistricts);
		s_pxActiveDistricts = &m_xDistricts;

		// Public-transport lines (stops gate transit ridership reach).
		m_xTransit.Reset();
		m_xBuild.SetTransitLines(&m_xTransit);
		s_pxActiveTransit = &m_xTransit;

		// Utility conduits (extend power/water reach along connected chains).
		m_xConduits.Reset();
		m_xBuild.SetConduits(&m_xConduits);
		s_pxActiveConduits = &m_xConduits;

		// Demand-driven building growth in zoned lots.
		m_xBuild.Reset();
		m_xBuild.SetAutoDisasters(true);   // rare random fires (fire stations contain them)
		s_pxActiveBuild = &m_xBuild;

		// Visual road traffic.
		m_xTraffic.Reset();

		Zenith_Log(LOG_CATEGORY_GAMEPLAY, "[CityBuilder] CityManager started (grid %ux%u)",
			m_xGrid.GetWidth(), m_xGrid.GetHeight());
	}

	void OnUpdate(const float fDt) ZENITH_FINAL override
	{
		++s_uUpdateCount;

		// P toggles pause.
		if (g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_P))
		{
			m_xSim.SetSpeed(m_xSim.GetSpeed() == CB_SIM_PAUSED ? CB_SIM_NORMAL : CB_SIM_PAUSED);
			Zenith_EventDispatcher::Get().Dispatch(CB_OnPauseToggled{ m_xSim.GetSpeed() == CB_SIM_PAUSED });
		}

		m_xSim.Update(fDt);
		m_xTools.Update(m_xGrid, m_xRoads, m_xBuildings, m_xHeightfield);
		// Free-form spline road tool (draw / bulldoze curved roads, paint zones).
		// Headless-safe (no mouse input → no edits); GPU submit is in the render block.
		m_xRoadCtrl.Update(m_xTools, m_xHeightfield, m_xZoning, m_xBuild);
		// Reconcile frontage lots with the road graph (incremental, cheap).
		m_xZoning.SyncToGraph(m_xRoadCtrl.GetGraph(), m_xHeightfield);

		// Demand-driven building growth (self-rate-limits; paused respects speed).
		if (m_xSim.GetSpeed() != CB_SIM_PAUSED)
		{
			m_xBuild.SetRoadCapacity(m_xRoadCtrl.GetGraph().GetTotalActiveLength());
			m_xBuild.Tick(m_xZoning);
		}

		// Population milestones — fired once each as the free-form town grows, each
		// carrying a cash grant (Cities: Skylines-style unlock rewards).
		{
			static const uint32_t auThresholds[] = { 100u, 500u, 1000u, 2500u, 5000u };
			static const float     afRewards[]    = { 5000.0f, 15000.0f, 30000.0f, 60000.0f, 100000.0f };
			const uint32_t uPop = m_xBuild.GetPopulation();
			for (uint32_t uIdx = 0; uIdx < 5u; ++uIdx)
			{
				if ((m_uMilestoneMask & (1u << uIdx)) == 0u && uPop >= auThresholds[uIdx])
				{
					m_uMilestoneMask |= (1u << uIdx);
					m_xBuild.GrantFunds(afRewards[uIdx]);
					Zenith_EventDispatcher::Get().Dispatch(CB_OnMilestone{ uPop, static_cast<int32_t>(auThresholds[uIdx]) });
				}
			}
		}

		if (!Zenith_CommandLine::IsHeadless())
		{
			// Roads draw as terrain-following ribbons over the baked hilly terrain (the
			// heightfield stays the baked hill, so ribbons + frontage lots sit on the
			// surface and match the rendered GPU terrain).
			//
			// Roads CARVE the terrain via GPU vertex deformation, the RACE-FREE way: ALL carving goes
			// through the engine's terrain-streaming path, never an in-place write to a live, GPU-read
			// chunk (that corrupts distant terrain even behind a waitIdle — the streaming system isn't
			// built for mid-frame in-place edits of resident chunks). The stream-in hook
			// (RegisterStreamHook) re-shapes every HIGH chunk to the live heightfield as it loads;
			// ForceRestreamCarveChunks evicts the chunks already resident under a new road so they
			// re-load (and carve) too. The terraform tool uses the same path (RestreamTerraformRegion).
			const uint32_t uCarveSegs = m_xRoadCtrl.GetGraph().GetActiveSegmentCount();
			if (uCarveSegs != m_uLastCarveSegs)
			{
				m_uLastCarveSegs = uCarveSegs;
				CB_RoadTerrain::FlattenHeightfield(m_xRoadCtrl.GetGraph(), m_xHeightfield);
				m_xRoadCtrl.RebuildMesh(m_xHeightfield);
				CB_RoadTerrain::RebuildCarveContext(m_xRoadCtrl.GetGraph(), m_xCarveCtx);
				EnsureTerrainPtr();
				// RACE-FREE carve: install the stream-in hook (carves every HIGH chunk on load), then
				// evict the chunks already resident under the road so they re-stream + get carved via
				// the same safe path. NO in-place write to a live chunk (that corrupts distant terrain
				// even behind waitIdle). See the block comment above + CB_RoadTerrain.
				CB_RoadTerrain::RegisterStreamHook(m_pxTerrain, m_xHeightfield);
				CB_RoadTerrain::ForceRestreamCarveChunks(m_xCarveCtx, m_pxTerrain);
			}
			// Districts + policy ordinances (paint with the district tool; F1-F4 toggle).
			UpdateDistrictTool();
			RenderDistrictOverlay();
			// Public-transport lines + utility conduits (place markers with the tools).
			UpdateTransitTool();
			RenderTransitOverlay();
			RenderConduitOverlay();
#if CB_USE_LEGACY_GRID
			CB_PresentationView::Render(m_xGrid, m_xRoads, m_xBuildings, m_xHeightfield);
#endif
			m_xRoadCtrl.Render();     // free-form spline roads (terrain-following ribbons)
			m_xZoning.RenderOverlay();// zone colour overlay on unbuilt frontage lots
			m_xBuild.Render(m_xZoning);// road-facing building boxes
			if (m_xSim.GetSpeed() != CB_SIM_PAUSED) { m_xTraffic.Update(m_xRoadCtrl.GetGraph(), m_xHeightfield, fDt); }
			m_xTraffic.Render();       // cars driving the spline network
			UpdateHUD();
		}
	}

	static constexpr float TERRAFORM_RADIUS = 45.0f;   // world-space brush radius

	// Find + cache the scene's terrain component (the GPU carve / terraform target).
	void EnsureTerrainPtr()
	{
		if (m_pxTerrain != nullptr) { return; }
		Zenith_TerrainComponent* pxFound = nullptr;
		g_xEngine.Scenes().QueryAllScenes<Zenith_TerrainComponent>().ForEach(
			[&pxFound](Zenith_EntityID, Zenith_TerrainComponent& xT) { if (pxFound == nullptr) { pxFound = &xT; } });
		m_pxTerrain = pxFound;
	}

	// Safe terraform GPU update: re-stream the brushed chunks so the stream-in hook re-applies the
	// heightfield (race-free, exactly like the road carve). Replaces the old in-place re-upload,
	// which raced the in-flight GPU read and corrupted distant terrain. The hook reads the live
	// heightfield (hills + road + terraform), so the re-streamed chunks reload fully deformed.
	void RestreamTerraformRegion(float fWX, float fWZ)
	{
		EnsureTerrainPtr();
		if (m_pxTerrain == nullptr) { return; }
		CB_RoadTerrain::RegisterStreamHook(m_pxTerrain, m_xHeightfield);   // idempotent
		const float fR = TERRAFORM_RADIUS + 4.0f;
		CB_RoadTerrain::CarveContext xRegion;
		xRegion.m_bActive = true;
		xRegion.m_fMinX = fWX - fR; xRegion.m_fMaxX = fWX + fR;
		xRegion.m_fMinZ = fWZ - fR; xRegion.m_fMaxZ = fWZ + fR;
		CB_RoadTerrain::ForceRestreamCarveChunks(xRegion, m_pxTerrain);
	}

	// Terraform tool: hold LMB to raise / RMB to lower the ground under the cursor.
	// Edits the CPU heightfield every frame (gameplay queries stay current) and re-uploads
	// the affected GPU terrain chunks on a throttle (the re-upload loads baked chunk meshes).
	void UpdateTerraform()
	{
		Zenith_Input& xInput = g_xEngine.Input();
		const bool bRaise = xInput.IsMouseButtonHeld(ZENITH_MOUSE_BUTTON_LEFT);
		const bool bLower = xInput.IsMouseButtonHeld(ZENITH_MOUSE_BUTTON_RIGHT);
		if (!bRaise && !bLower) { m_uTerraformTick = 0u; return; }

		float fWX = 0.0f, fWZ = 0.0f;
		if (!m_xTools.PickGroundPoint(fWX, fWZ)) { return; }

		CB_TerrainBrush xBrush;
		xBrush.m_eMode     = bRaise ? CB_TERRAIN_BRUSH_RAISE : CB_TERRAIN_BRUSH_LOWER;
		xBrush.m_fCentreX  = fWX;
		xBrush.m_fCentreZ  = fWZ;
		xBrush.m_fRadius   = TERRAFORM_RADIUS;
		xBrush.m_fStrength = 0.02f;            // gentle per-frame; hold to sculpt
		m_xHeightfield.ApplyBrush(xBrush);

		if ((m_uTerraformTick++ % 6u) == 0u)   // refresh the GPU mesh a few times a second
		{
			RestreamTerraformRegion(fWX, fWZ);
		}
	}

	// District tool: paint districts (LMB) + toggle ordinances (F1-F4 → the current
	// district when the tool's active, else city-wide). Windowed (paint needs the picker).
	void UpdateDistrictTool()
	{
		Zenith_Input& xInput = g_xEngine.Input();
		const bool bDistrictTool = (m_xTools.GetTool() == CB_TOOL_DISTRICT);

		if (bDistrictTool && xInput.IsMouseButtonHeld(ZENITH_MOUSE_BUTTON_LEFT))
		{
			float fWX = 0.0f, fWZ = 0.0f;
			if (m_xTools.PickGroundPoint(fWX, fWZ)) { m_xDistricts.PaintDistrict(fWX, fWZ); }
		}

		const CB_EPolicy aePolicy[4] = { CB_POLICY_RECYCLING, CB_POLICY_FREE_TRANSIT, CB_POLICY_POLLUTION_CONTROL, CB_POLICY_PARKS_MANDATE };
		const int        aiKey[4]    = { ZENITH_KEY_F1, ZENITH_KEY_F2, ZENITH_KEY_F3, ZENITH_KEY_F4 };
		for (int i = 0; i < 4; ++i)
		{
			if (!xInput.WasKeyPressedThisFrame(aiKey[i])) { continue; }
			if (bDistrictTool && m_xDistricts.GetCurrent() != CB_Districts::INVALID)
			{
				m_xDistricts.ToggleDistrictPolicy(m_xDistricts.GetCurrent(), aePolicy[i]);
			}
			else
			{
				m_xDistricts.ToggleCityPolicy(aePolicy[i]);
			}
		}
	}

	// Draw each district as a ground ring — green if it carries ordinances, grey if not.
	void RenderDistrictOverlay()
	{
		const uint32_t uN = m_xDistricts.GetSlotCount();
		for (uint32_t i = 0; i < uN; ++i)
		{
			const CB_District& xD = m_xDistricts.Get(i);
			if (!xD.m_bActive) { continue; }
			const float fY = m_xHeightfield.GetHeightAt(xD.m_fCentreX, xD.m_fCentreZ) + 1.0f;
			const Zenith_Maths::Vector3 xColor = (xD.m_uPolicyMask != 0u)
				? Zenith_Maths::Vector3(0.20f, 0.80f, 0.35f) : Zenith_Maths::Vector3(0.70f, 0.70f, 0.75f);
			g_xEngine.Primitives().AddCircle(Zenith_Maths::Vector3(xD.m_fCentreX, fY, xD.m_fCentreZ), xD.m_fRadius, xColor);
		}
	}

	// Transit + conduit tools (both place markers on click): transit left-click adds a stop
	// (right starts a new line); conduit left-click lays a utility conduit node.
	void UpdateTransitTool()
	{
		const CB_ETool eTool = m_xTools.GetTool();
		if (eTool != CB_TOOL_TRANSIT && eTool != CB_TOOL_CONDUIT) { return; }
		Zenith_Input& xInput = g_xEngine.Input();
		const bool bL = xInput.IsMouseButtonHeld(ZENITH_MOUSE_BUTTON_LEFT);
		const bool bR = xInput.IsMouseButtonHeld(ZENITH_MOUSE_BUTTON_RIGHT);
		if (eTool == CB_TOOL_TRANSIT && bR && !m_bPrevTransitRight) { m_xTransit.StartLine(); }
		if (bL && !m_bPrevTransitLeft)
		{
			float fWX = 0.0f, fWZ = 0.0f;
			if (m_xTools.PickGroundPoint(fWX, fWZ))
			{
				if (eTool == CB_TOOL_TRANSIT) { m_xTransit.AddStop(fWX, fWZ); }
				else                          { m_xConduits.AddConduit(fWX, fWZ); }
			}
		}
		m_bPrevTransitLeft  = bL;
		m_bPrevTransitRight = bR;
	}

	// Draw conduits (energized = cyan, dead = grey) + links between connected nodes.
	void RenderConduitOverlay()
	{
		const uint32_t uN = m_xConduits.GetCount();
		const float fLink2 = CB_Conduits::LINK_DIST * CB_Conduits::LINK_DIST;
		for (uint32_t i = 0; i < uN; ++i)
		{
			const CB_Conduit& xC = m_xConduits.Get(i);
			const float fY = m_xHeightfield.GetHeightAt(xC.m_fX, xC.m_fZ) + 1.5f;
			const Zenith_Maths::Vector3 xCol = (xC.m_bPowered || xC.m_bWatered)
				? Zenith_Maths::Vector3(0.20f, 0.85f, 0.90f) : Zenith_Maths::Vector3(0.45f, 0.45f, 0.45f);
			g_xEngine.Primitives().AddCross(Zenith_Maths::Vector3(xC.m_fX, fY, xC.m_fZ), 6.0f, xCol);
			for (uint32_t j = i + 1; j < uN; ++j)
			{
				const CB_Conduit& xO = m_xConduits.Get(j);
				const float dx = xC.m_fX - xO.m_fX, dz = xC.m_fZ - xO.m_fZ;
				if (dx * dx + dz * dz <= fLink2)
				{
					const float fY2 = m_xHeightfield.GetHeightAt(xO.m_fX, xO.m_fZ) + 1.5f;
					g_xEngine.Primitives().AddLine(Zenith_Maths::Vector3(xC.m_fX, fY, xC.m_fZ),
						Zenith_Maths::Vector3(xO.m_fX, fY2, xO.m_fZ), xCol, 0.5f);
				}
			}
		}
	}

	// Draw each transit stop (amber disc) + a line between consecutive stops of a route.
	void RenderTransitOverlay()
	{
		const uint32_t uN = m_xTransit.GetStopCount();
		const Zenith_Maths::Vector3 xAmber(0.95f, 0.75f, 0.10f);
		for (uint32_t i = 0; i < uN; ++i)
		{
			const CB_TransitStop& xS = m_xTransit.GetStop(i);
			const float fY = m_xHeightfield.GetHeightAt(xS.m_fX, xS.m_fZ) + 2.0f;
			g_xEngine.Primitives().AddCircle(Zenith_Maths::Vector3(xS.m_fX, fY, xS.m_fZ), 8.0f, xAmber);
			if (i + 1 < uN && m_xTransit.GetStop(i + 1).m_uLine == xS.m_uLine)
			{
				const CB_TransitStop& xNxt = m_xTransit.GetStop(i + 1);
				const float fY2 = m_xHeightfield.GetHeightAt(xNxt.m_fX, xNxt.m_fZ) + 2.0f;
				g_xEngine.Primitives().AddLine(Zenith_Maths::Vector3(xS.m_fX, fY, xS.m_fZ),
					Zenith_Maths::Vector3(xNxt.m_fX, fY2, xNxt.m_fZ), xAmber, 1.0f);
			}
		}
	}

	void UpdateHUD()
	{
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
		{
			return;
		}
		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();
		char acBuf[192];
		if (Zenith_UI::Zenith_UIText* p = xUI.FindElement<Zenith_UI::Zenith_UIText>("CB_Pop"))    { snprintf(acBuf, sizeof(acBuf), "Population: %u   Jobs: %u   Bldgs: %u   Services: %u", m_xBuild.GetPopulation(), m_xBuild.GetJobs(), m_xBuild.GetActiveBuildings(), m_xBuild.GetActiveServices()); p->SetText(acBuf); }
		if (Zenith_UI::Zenith_UIText* p = xUI.FindElement<Zenith_UI::Zenith_UIText>("CB_Money"))  { snprintf(acBuf, sizeof(acBuf), "Treasury: $%d   Tax: %d%%   Debt: $%d", static_cast<int>(m_xBuild.GetTreasury()), static_cast<int>(m_xBuild.GetTaxRate() * 100.0f), static_cast<int>(m_xBuild.GetDebt())); p->SetText(acBuf); }
		if (Zenith_UI::Zenith_UIText* p = xUI.FindElement<Zenith_UI::Zenith_UIText>("CB_Happy"))  { snprintf(acBuf, sizeof(acBuf), "Happy %d%%  Poll %d%%  Traffic %d%%  Garbage %d%%  Sewage %d%%  Transit %d%%", static_cast<int>(m_xBuild.GetHappiness() * 100.0f), static_cast<int>(m_xBuild.GetPollution() * 100.0f), static_cast<int>(m_xBuild.GetCongestion() * 100.0f), static_cast<int>(m_xBuild.GetGarbage() * 100.0f), static_cast<int>(m_xBuild.GetSewage() * 100.0f), static_cast<int>(m_xBuild.GetTransitShare() * 100.0f)); p->SetText(acBuf); }
		if (Zenith_UI::Zenith_UIText* p = xUI.FindElement<Zenith_UI::Zenith_UIText>("CB_Demand")) { snprintf(acBuf, sizeof(acBuf), "Demand R%d C%d I%d  Pwr %d/%d Wtr %d/%d  Mail %d%% Freight %d%% Fires %u", static_cast<int>(m_xBuild.GetResDemand() * 100.0f), static_cast<int>(m_xBuild.GetComDemand() * 100.0f), static_cast<int>(m_xBuild.GetIndDemand() * 100.0f), static_cast<int>(m_xBuild.GetPowerConsumed()), static_cast<int>(m_xBuild.GetPowerProduced()), static_cast<int>(m_xBuild.GetWaterConsumed()), static_cast<int>(m_xBuild.GetWaterProduced()), static_cast<int>(m_xBuild.GetMail() * 100.0f), static_cast<int>(m_xBuild.GetFreightRatio() * 100.0f), m_xBuild.GetActiveFires()); p->SetText(acBuf); }
		if (Zenith_UI::Zenith_UIText* p = xUI.FindElement<Zenith_UI::Zenith_UIText>("CB_Speed"))
		{
			const uint32_t uPol = m_xDistricts.GetCityPolicyMask();
			snprintf(acBuf, sizeof(acBuf), "%s  [P]   Districts:%u  Policy:%s%s%s%s",
				m_xSim.GetSpeed() == CB_SIM_PAUSED ? "PAUSED" : "PLAYING", m_xDistricts.GetActiveCount(),
				(uPol & CB_PolicyBit(CB_POLICY_RECYCLING))         ? " Recycle" : "",
				(uPol & CB_PolicyBit(CB_POLICY_FREE_TRANSIT))      ? " FreeTransit" : "",
				(uPol & CB_PolicyBit(CB_POLICY_POLLUTION_CONTROL)) ? " PollCtrl" : "",
				(uPol & CB_PolicyBit(CB_POLICY_PARKS_MANDATE))     ? " Parks" : "");
			p->SetText(acBuf);
		}
		if (Zenith_UI::Zenith_UIText* p = xUI.FindElement<Zenith_UI::Zenith_UIText>("CB_Tool"))
		{
			const CB_ETool eHudTool = m_xTools.GetTool();
			if (eHudTool == CB_TOOL_POLICE)   // the services category cycles its sub-type
			{
				const char* pcSvc = "Police";
				switch (m_xRoadCtrl.GetServiceType())
				{
				case CB_BUILDING_FIRE:         pcSvc = "Fire";        break;
				case CB_BUILDING_HOSPITAL:     pcSvc = "Hospital";    break;
				case CB_BUILDING_SCHOOL:       pcSvc = "School";      break;
				case CB_BUILDING_LANDFILL:     pcSvc = "Landfill";    break;
				case CB_BUILDING_SEWAGE_PLANT: pcSvc = "Sewage Plant";break;
				case CB_BUILDING_BUS_DEPOT:    pcSvc = "Bus Depot";   break;
				case CB_BUILDING_POST_OFFICE:  pcSvc = "Post Office"; break;
				default:                       pcSvc = "Police";      break;
				}
				snprintf(acBuf, sizeof(acBuf), "Tool: Service: %s  [6 cycle]", pcSvc);
			}
			else
			{
				snprintf(acBuf, sizeof(acBuf), "Tool: %s  [1-9]", CB_ToolSystem::ToolName(eHudTool));
			}
			p->SetText(acBuf);
		}
	}

	void OnDestroy() ZENITH_FINAL override
	{
		// Clear the stream-in carve hook first: the engine holds a raw pointer to m_xCarveCtx,
		// which is about to be destroyed with this behaviour. (Safe if the terrain is already gone.)
		if (m_pxTerrain != nullptr) { CB_RoadTerrain::UnregisterStreamHook(m_pxTerrain); }
		s_bStarted = false;
		if (s_pxActiveHeightfield == &m_xHeightfield)
		{
			s_pxActiveHeightfield = nullptr;
		}
		if (s_pxActiveGrid == &m_xGrid)
		{
			s_pxActiveGrid = nullptr;
		}
		if (s_pxActiveRoads == &m_xRoads)
		{
			s_pxActiveRoads = nullptr;
		}
		if (s_pxActive == this)
		{
			s_pxActive = nullptr;
		}
		if (s_pxActiveRoadCtrl == &m_xRoadCtrl)
		{
			s_pxActiveRoadCtrl = nullptr;
		}
		if (s_pxActiveZoning == &m_xZoning)
		{
			s_pxActiveZoning = nullptr;
		}
		if (s_pxActiveBuild == &m_xBuild)
		{
			s_pxActiveBuild = nullptr;
		}
		if (s_pxActiveDistricts == &m_xDistricts)
		{
			s_pxActiveDistricts = nullptr;
		}
		if (s_pxActiveTransit == &m_xTransit)
		{
			s_pxActiveTransit = nullptr;
		}
		if (s_pxActiveConduits == &m_xConduits)
		{
			s_pxActiveConduits = nullptr;
		}
	}

	CB_TerrainHeightfield&       GetHeightfield()       { return m_xHeightfield; }
	const CB_TerrainHeightfield& GetHeightfield() const { return m_xHeightfield; }

	// Scripted terraform (automation / playthrough): raise or lower the ground at a world
	// point + re-upload the affected GPU chunks — the same path the interactive tool drives.
	void TerraformAt(float fWX, float fWZ, bool bRaise, float fStrength = 0.20f, int iApplications = 30)
	{
		CB_TerrainBrush xBrush;
		xBrush.m_eMode     = bRaise ? CB_TERRAIN_BRUSH_RAISE : CB_TERRAIN_BRUSH_LOWER;
		xBrush.m_fCentreX  = fWX; xBrush.m_fCentreZ = fWZ;
		xBrush.m_fRadius   = TERRAFORM_RADIUS; xBrush.m_fStrength = fStrength;
		for (int i = 0; i < iApplications; ++i) { m_xHeightfield.ApplyBrush(xBrush); }
		if (!Zenith_CommandLine::IsHeadless())
		{
			RestreamTerraformRegion(fWX, fWZ);
		}
	}
	CB_CityGrid&                 GetGrid()              { return m_xGrid; }
	const CB_CityGrid&           GetGrid() const        { return m_xGrid; }
	CB_RoadNetwork&              GetRoads()             { return m_xRoads; }
	const CB_RoadNetwork&        GetRoads() const       { return m_xRoads; }
	CB_BuildingManager&          GetBuildings()         { return m_xBuildings; }
	const CB_BuildingManager&    GetBuildings() const   { return m_xBuildings; }
	CB_ServiceManager&           GetServices()          { return m_xServices; }
	CB_EconomyManager&           GetEconomy()           { return m_xEconomy; }
	CB_CitizenManager&           GetCitizens()          { return m_xCitizens; }
	CB_SimulationTick&           GetSim()               { return m_xSim; }
	const CB_SimulationTick&     GetSim() const         { return m_xSim; }
	const CB_CityStats&          GetStats() const       { return m_xSim.GetStats(); }
	CB_ToolSystem&               GetTools()             { return m_xTools; }
	CB_RoadController&           GetRoadController()    { return m_xRoadCtrl; }
	const CB_RoadController&     GetRoadController() const { return m_xRoadCtrl; }
	CB_Zoning&                   GetZoning()            { return m_xZoning; }
	const CB_Zoning&             GetZoning() const      { return m_xZoning; }
	CB_BuildingPlacement&        GetBuild()             { return m_xBuild; }
	const CB_BuildingPlacement&  GetBuild() const       { return m_xBuild; }

	// Live CityManager instance (null when none active).
	static CB_CityManager_Behaviour* GetActive() { return s_pxActive; }

	// Live free-form road controller (null when no CityManager is active).
	static CB_RoadController* GetActiveRoadController() { return s_pxActiveRoadCtrl; }
	static CB_Zoning*         GetActiveZoning()         { return s_pxActiveZoning; }
	static CB_BuildingPlacement* GetActiveBuild()       { return s_pxActiveBuild; }
	static CB_Districts*      GetActiveDistricts()      { return s_pxActiveDistricts; }
	static CB_TransitLines*   GetActiveTransit()        { return s_pxActiveTransit; }
	static CB_Conduits*       GetActiveConduits()       { return s_pxActiveConduits; }

	// Liveness accessors consumed by the CB_Boot automated test.
	static bool     WasStarted()     { return s_bStarted; }
	static uint32_t GetUpdateCount() { return s_uUpdateCount; }

	// Active-subsystem accessors for game-side bridges/tests (null when no
	// CityManager is live).
	static CB_TerrainHeightfield* GetActiveHeightfield() { return s_pxActiveHeightfield; }
	static CB_CityGrid*           GetActiveGrid()        { return s_pxActiveGrid; }
	static CB_RoadNetwork*        GetActiveRoads()       { return s_pxActiveRoads; }

private:
	CB_TerrainHeightfield m_xHeightfield;
	CB_CityGrid           m_xGrid;
	CB_RoadNetwork        m_xRoads;
	CB_BuildingManager    m_xBuildings;
	CB_ServiceManager     m_xServices;
	CB_EconomyManager     m_xEconomy;
	CB_CitizenManager     m_xCitizens;
	CB_SimulationTick     m_xSim;
	CB_ToolSystem         m_xTools;
	CB_RoadController     m_xRoadCtrl;             // free-form spline road network + draw tool
	CB_Zoning             m_xZoning;               // road-relative frontage lots + zone paint
	CB_BuildingPlacement  m_xBuild;                // demand-driven zoned building growth
	CB_Districts          m_xDistricts;            // named regions + city/district policy ordinances
	CB_TransitLines       m_xTransit;              // public-transport lines + stops (ridership reach)
	CB_Conduits           m_xConduits;             // utility conduit network (power/water reach)
	CB_Traffic            m_xTraffic;             // visual road traffic (cars on the spline network)
	Zenith_TerrainComponent* m_pxTerrain   = nullptr;  // cached GPU terrain for the road carve
	uint32_t                 m_uLastCarveSegs = 0;     // re-carve when the road count changes
	CB_RoadTerrain::CarveContext m_xCarveCtx;          // road samples for the stream-in carve hook (engine holds a ptr to this)
	uint32_t                 m_uTerraformTick  = 0;    // throttles the terraform GPU re-upload while dragging
	bool                     m_bPrevTransitLeft  = false;  // transit-tool click edge latches
	bool                     m_bPrevTransitRight = false;

	uint32_t              m_uMilestoneMask = 0;   // population-threshold dispatch tracking

	static inline bool                       s_bStarted            = false;
	static inline uint32_t                   s_uUpdateCount        = 0;
	static inline CB_TerrainHeightfield*     s_pxActiveHeightfield = nullptr;
	static inline CB_CityGrid*               s_pxActiveGrid        = nullptr;
	static inline CB_RoadNetwork*            s_pxActiveRoads       = nullptr;
	static inline CB_CityManager_Behaviour*  s_pxActive            = nullptr;
	static inline CB_RoadController*         s_pxActiveRoadCtrl    = nullptr;
	static inline CB_Zoning*                 s_pxActiveZoning      = nullptr;
	static inline CB_BuildingPlacement*      s_pxActiveBuild       = nullptr;
	static inline CB_Districts*              s_pxActiveDistricts   = nullptr;
	static inline CB_TransitLines*           s_pxActiveTransit     = nullptr;
	static inline CB_Conduits*               s_pxActiveConduits    = nullptr;
};
