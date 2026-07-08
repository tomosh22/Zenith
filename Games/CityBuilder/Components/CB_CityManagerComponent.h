#pragma once
#include "Core/Zenith_Engine.h"

#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "DataStream/Zenith_DataStream.h"
#include "CityBuilder/Source/CB_TerrainHeightfield.h"
#include "CityBuilder/Source/CB_SimSpeed.h"
#include "CityBuilder/Source/CB_ToolSystem.h"
#include "CityBuilder/Source/CB_RoadController.h"
#include "CityBuilder/Source/CB_Zoning.h"
#include "CityBuilder/Source/CB_BuildingPlacement.h"
#include "CityBuilder/Source/CB_Districts.h"
#include "CityBuilder/Source/CB_TransitLines.h"
#include "CityBuilder/Source/CB_Conduits.h"
#include "CityBuilder/Source/CB_SaveLoadFreeform.h"   // F5/F9 quick save/load
#include "CityBuilder/Source/CB_Traffic.h"
#include "CityBuilder/Source/CB_TerrainGen.h"
#include "CityBuilder/Source/CB_RoadTerrain.h"
#include "Flux/Primitives/Flux_PrimitivesImpl.h"   // district overlay rings
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "CityBuilder/Source/CB_ToolIcons.h"   // toolbar icon filenames + hover tooltips
#include <cstring>                              // strlen (tooltip width estimate)
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "UI/Zenith_UIText.h"
#include "UI/Zenith_UIRect.h"
#include "UI/Zenith_UIButton.h"
#include "UI/Zenith_UIImage.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "AssetHandling/Zenith_MaterialAsset.h"                          // building material (per-zone albedo)
#include "EntityComponent/Components/Zenith_InstancedMeshComponent.h"   // GPU-instanced building cube meshes
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"                         // GenerateUnitCube
#include "Flux/MeshGeometry/Flux_MeshInstance.h"                         // CreateFromGeometry
#ifdef ZENITH_INPUT_SIMULATOR
#include "Input/Zenith_InputSimulator.h"
#endif
#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif
#include "Input/Zenith_Input.h"
#include "Core/Zenith_CommandLine.h"
#include "CityBuilder/Source/CB_Events.h"
#include "ZenithECS/Zenith_EventSystem.h"
#include <cstdio>
#include <utility>

// ============================================================================
// CB_CityManagerComponent — top-level city orchestrator.
//
// In later phases this owns the spatial grid, zone/road/building managers, the
// citizen pool, service coverage, the economy, and drives the fixed-timestep
// simulation tick. Sub-systems are stored as members, initialised in OnStart
// (the CityManager entity is deserialized from the saved scene, so OnStart —
// not OnAwake — is the reliable init point), torn down in OnDestroy.
//
// Phase 1: minimal lifecycle plus a liveness counter the CB_Boot gate asserts
// on (proves the scene loaded, the component attached, and frames are ticking).
// ============================================================================
class CB_CityManagerComponent
{
public:
	CB_CityManagerComponent() = delete;
	CB_CityManagerComponent(Zenith_Entity& xParentEntity)
		: m_xParentEntity(xParentEntity)
	{
	}

	// Component pools relocate components on resize / swap-and-pop / cross-scene
	// transfer (move-construct + destruct the source), so the moves are
	// hand-written: they re-wire every pointer that captured a MEMBER address of
	// the source — the published static accessors (tests reach the live city
	// through them), the sibling-subsystem pointers (m_xBuild holds &m_xDistricts
	// etc.), and the engine terrain stream hook (whose user pointer is
	// &m_xHeightfield). Copies deleted.
	CB_CityManagerComponent(const CB_CityManagerComponent&) = delete;
	CB_CityManagerComponent& operator=(const CB_CityManagerComponent&) = delete;

	CB_CityManagerComponent(CB_CityManagerComponent&& xOther) noexcept
		: m_xParentEntity(xOther.m_xParentEntity)
		, m_xHeightfield(std::move(xOther.m_xHeightfield))
		, m_eSpeed(xOther.m_eSpeed)
		, m_xTools(std::move(xOther.m_xTools))
		, m_xRoadCtrl(std::move(xOther.m_xRoadCtrl))
		, m_xZoning(std::move(xOther.m_xZoning))
		, m_xBuild(std::move(xOther.m_xBuild))
		, m_xDistricts(std::move(xOther.m_xDistricts))
		, m_xTransit(std::move(xOther.m_xTransit))
		, m_xConduits(std::move(xOther.m_xConduits))
		, m_xTraffic(std::move(xOther.m_xTraffic))
		, m_auTrafficOrigins(std::move(xOther.m_auTrafficOrigins))
		, m_auTrafficDests(std::move(xOther.m_auTrafficDests))
		, m_pxTerrain(xOther.m_pxTerrain)
		, m_uLastCarveSegs(xOther.m_uLastCarveSegs)
		, m_xCarveCtx(std::move(xOther.m_xCarveCtx))
		, m_pxBuildingInst(xOther.m_pxBuildingInst)
		, m_uTerraformTick(xOther.m_uTerraformTick)
		, m_bUIBuilt(xOther.m_bUIBuilt)
		, m_fUIBuiltW(xOther.m_fUIBuiltW)
		, m_fUIBuiltH(xOther.m_fUIBuiltH)
		, m_bPrevTransitLeft(xOther.m_bPrevTransitLeft)
		, m_bPrevTransitRight(xOther.m_bPrevTransitRight)
		, m_uMilestoneMask(xOther.m_uMilestoneMask)
	{
		AdoptFrom(xOther);
	}

	CB_CityManagerComponent& operator=(CB_CityManagerComponent&& xOther) noexcept
	{
		if (this != &xOther)
		{
			m_xParentEntity     = xOther.m_xParentEntity;
			m_xHeightfield      = std::move(xOther.m_xHeightfield);
			m_eSpeed            = xOther.m_eSpeed;
			m_xTools            = std::move(xOther.m_xTools);
			m_xRoadCtrl         = std::move(xOther.m_xRoadCtrl);
			m_xZoning           = std::move(xOther.m_xZoning);
			m_xBuild            = std::move(xOther.m_xBuild);
			m_xDistricts        = std::move(xOther.m_xDistricts);
			m_xTransit          = std::move(xOther.m_xTransit);
			m_xConduits         = std::move(xOther.m_xConduits);
			m_xTraffic          = std::move(xOther.m_xTraffic);
			m_auTrafficOrigins  = std::move(xOther.m_auTrafficOrigins);
			m_auTrafficDests    = std::move(xOther.m_auTrafficDests);
			m_pxTerrain         = xOther.m_pxTerrain;
			m_uLastCarveSegs    = xOther.m_uLastCarveSegs;
			m_xCarveCtx         = std::move(xOther.m_xCarveCtx);
			m_pxBuildingInst    = xOther.m_pxBuildingInst;
			m_uTerraformTick    = xOther.m_uTerraformTick;
			m_bUIBuilt          = xOther.m_bUIBuilt;
			m_fUIBuiltW         = xOther.m_fUIBuiltW;
			m_fUIBuiltH         = xOther.m_fUIBuiltH;
			m_bPrevTransitLeft  = xOther.m_bPrevTransitLeft;
			m_bPrevTransitRight = xOther.m_bPrevTransitRight;
			m_uMilestoneMask    = xOther.m_uMilestoneMask;
			AdoptFrom(xOther);
		}
		return *this;
	}

	// Component contract. Everything here is runtime state rebuilt in OnStart
	// (the old behaviour persisted no parameters); only the version tag persists.
	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		const u_int uVersion = 1;
		xStream << uVersion;
	}
	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		u_int uVersion = 0;
		xStream >> uVersion;
	}
#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel()
	{
		ImGui::Text("Population: %u  Jobs: %u", m_xBuild.GetPopulation(), m_xBuild.GetJobs());
		ImGui::Text("Buildings: %u  Services: %u", m_xBuild.GetActiveBuildings(), m_xBuild.GetActiveServices());
		ImGui::Text("Treasury: $%d  Tax: %d%%", static_cast<int>(m_xBuild.GetTreasury()), static_cast<int>(m_xBuild.GetTaxRate() * 100.0f));
		ImGui::Text("Road segments: %u", m_xRoadCtrl.GetGraph().GetActiveSegmentCount());
		ImGui::Text("Sim speed: %d", static_cast<int>(m_eSpeed));
	}
#endif

	void OnStart()
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
		m_xTools.SetTerrainField(&m_xHeightfield);   // terrain-aware cursor picking (ray vs hills, not y=0)

		s_pxActive = this;

		// Free-form spline road network (Cities: Skylines-style). Owns its own graph.
		m_xRoadCtrl.Reset();
		s_pxActiveRoadCtrl = &m_xRoadCtrl;

		// Road-relative zoning (frontage lots).
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

		// Demand-driven road traffic (SimCity/C:S OD-trip model — homes drive to jobs/shops).
		m_xTraffic.Reset();
		s_pxActiveTraffic = &m_xTraffic;

		// Buildings render as GPU-instanced cube MESHES with a lit PBR material, coloured per
		// zone (residential green / commercial blue / industrial yellow) via the per-instance
		// albedo tint — the DevilsPlayground material approach (real materials, no emissive),
		// NOT washed-out debug primitives. Windowed only (GPU upload + instanced render).
		m_pxBuildingInst = nullptr;
		if (!Zenith_CommandLine::IsHeadless())
		{
			EnsureBuildingMeshAssets();
			Zenith_Entity xBuildings = g_xEngine.Scenes().CreateEntity(m_xParentEntity.GetSceneData(), "CityBuildings");
			xBuildings.SetTransient(true);   // render-only; rebuilt each session, not serialized
			Zenith_InstancedMeshComponent& xInst = xBuildings.AddComponent<Zenith_InstancedMeshComponent>();
			xInst.SetMesh(s_pxBuildingCubeMesh);
			xInst.SetMaterial(s_pxBuildingMat);
			xInst.Reserve(256);
			m_pxBuildingInst = &xInst;
		}

		Zenith_Log(LOG_CATEGORY_GAMEPLAY, "[CityBuilder] CityManager started (free-form, %u road segments)",
			m_xRoadCtrl.GetGraph().GetActiveSegmentCount());
	}

	void OnUpdate(const float fDt)
	{
		++s_uUpdateCount;

		// P toggles pause.
		if (g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_P))
		{
			m_eSpeed = (m_eSpeed == CB_SIM_PAUSED) ? CB_SIM_NORMAL : CB_SIM_PAUSED;
			Zenith_EventDispatcher::Get().Dispatch(CB_OnPauseToggled{ m_eSpeed == CB_SIM_PAUSED });
		}

		// --- Keyboard shortcuts for the build / economy actions the HUD exposes only as
		//     buttons (or not at all), so the whole game is keyboard-operable. Tool selection
		//     is keys 1-9/0/T/B/L/K (CB_ToolSystem); these complete the set so a player — or
		//     the automated human test — can drive every mechanic from the keyboard. ---
		{
			Zenith_Input& xKb = g_xEngine.Input();
			if (xKb.WasKeyPressedThisFrame(ZENITH_KEY_F8)) { NewCity(); }                                 // start over
			if (xKb.WasKeyPressedThisFrame(ZENITH_KEY_R))                                                  // cycle road class
			{
				m_xRoadCtrl.SetRoadClass(static_cast<CB_ERoadClass>((m_xRoadCtrl.GetRoadClass() + 1) % CB_ROADCLASS_COUNT));
			}
			if (xKb.WasKeyPressedThisFrame(ZENITH_KEY_MINUS)) { m_xBuild.SetTaxRate(m_xBuild.GetTaxRate() - 0.1f); }  // tax down
			if (xKb.WasKeyPressedThisFrame(ZENITH_KEY_EQUAL)) { m_xBuild.SetTaxRate(m_xBuild.GetTaxRate() + 0.1f); }  // tax up
			if (xKb.WasKeyPressedThisFrame(ZENITH_KEY_G))     { m_xBuild.TakeLoan(20000.0f); }              // take a development loan
			if (xKb.WasKeyPressedThisFrame(ZENITH_KEY_COMMA)) { CycleSpeed(-1); }                          // slower
			if (xKb.WasKeyPressedThisFrame(ZENITH_KEY_PERIOD)){ CycleSpeed(+1); }                          // faster
			if (xKb.WasKeyPressedThisFrame(ZENITH_KEY_F5))    { SaveCity(); }                              // quick-save
			if (xKb.WasKeyPressedThisFrame(ZENITH_KEY_F9))    { LoadCity(); }                              // quick-load
			// Ignite a fire under the cursor (disaster drill: covering fire stations contain it).
			if (xKb.WasKeyPressedThisFrame(ZENITH_KEY_F))
			{
				float fFX = 0.0f, fFZ = 0.0f;
				if (m_xTools.PickGroundPoint(fFX, fFZ)) { m_xBuild.TriggerFireAt(fFX, fFZ); }
			}
		}

		m_xTools.Update();   // tool-selection hotkeys; the free-form tools are applied by m_xRoadCtrl below
		// Free-form spline road tool (draw / bulldoze curved roads, paint zones).
		// Headless-safe (no mouse input → no edits); GPU submit is in the render block.
		m_xRoadCtrl.Update(m_xTools, m_xHeightfield, m_xZoning, m_xBuild);
		// Reconcile frontage lots with the road graph (incremental, cheap).
		m_xZoning.SyncToGraph(m_xRoadCtrl.GetGraph(), m_xHeightfield);

		// Demand-driven building growth (self-rate-limits; paused respects speed).
		if (m_eSpeed != CB_SIM_PAUSED)
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
			// Terraform tool: raise (LMB) / lower (RMB) the ground under the cursor.
			if (m_xTools.GetTool() == CB_TOOL_TERRAFORM) { UpdateTerraform(); }
			// Districts + policy ordinances (paint with the district tool; F1-F4 toggle).
			UpdateDistrictTool();
			RenderDistrictOverlay();
			// Public-transport lines + utility conduits (place markers with the tools).
			UpdateTransitTool();
			RenderTransitOverlay();
			RenderConduitOverlay();
			m_xRoadCtrl.Render();     // free-form spline roads (terrain-following ribbons)
			m_xZoning.RenderOverlay();// zone colour overlay on unbuilt frontage lots
			// SimCity/C:S affordance: while an R/C/I zone tool is selected, ghost EVERY available
			// placement lot (open frontage) in the tool's colour so the player sees where a zone can
			// go. Telemetry: s_uLastGhostCount (the ghosts drawn this frame; 0 when no zone tool).
			{
				const CB_ETool eGhostTool = m_xTools.GetTool();
				if (eGhostTool >= CB_TOOL_ZONE_RES && eGhostTool <= CB_TOOL_ZONE_IND)
				{
					s_uLastGhostCount = m_xZoning.RenderPlacementGhosts(static_cast<CB_EZoneType>(eGhostTool));
					if ((s_uUpdateCount % 90u) == 0u)
					{
						Zenith_Log(LOG_CATEGORY_GAMEPLAY,
							"[CityBuilder] Zone tool %u active: %u available placement ghosts rendered",
							static_cast<uint32_t>(eGhostTool), s_uLastGhostCount);
					}
				}
				else
				{
					s_uLastGhostCount = 0;
				}
			}
			m_xBuild.RenderInstanced(m_xZoning, m_pxBuildingInst);// R/C/I = lit instanced cube meshes (green/blue/yellow); services = primitives
			if (m_eSpeed != CB_SIM_PAUSED)
			{
				BuildTrafficEndpoints();   // homes (origins) + jobs/shops (destinations) from the live city
				const uint32_t uTarget = m_xBuild.GetPopulation() / POP_PER_CAR;   // concurrent trips scale with population
				const float fTrafficDt = fDt * CB_SpeedMultiplier(m_eSpeed);   // cars speed up with the sim clock
				m_xTraffic.Update(m_xRoadCtrl.GetGraph(), m_xHeightfield, fTrafficDt, m_auTrafficOrigins, m_auTrafficDests, uTarget);
			}
			m_xTraffic.Render();       // cars driving their routed home→work/shop trips
			if (Zenith_UIComponent* pxUI = m_xParentEntity.TryGetComponent<Zenith_UIComponent>())
			{
				Zenith_UIComponent& xUI = *pxUI;
				// (Re)build the UI on first use AND whenever the window/canvas size changes, so the
				// pixel-anchored layout always matches the live framebuffer (handles maximize/DPI/resize).
				const Zenith_Maths::Vector2 xCanvasSz = xUI.GetCanvas().GetSize();
				const bool bSizeChanged = (xCanvasSz.x > 1.0f) &&
					(fabsf(static_cast<float>(xCanvasSz.x) - m_fUIBuiltW) > 1.0f || fabsf(static_cast<float>(xCanvasSz.y) - m_fUIBuiltH) > 1.0f);
				if (!m_bUIBuilt || bSizeChanged) { BuildGameUI(xUI); m_bUIBuilt = true; }
				UpdateGameUI(xUI);
			}
		}
	}

	static constexpr float TERRAFORM_RADIUS = 45.0f;   // world-space brush radius

	// Find + cache the scene's terrain component (the GPU carve / terraform target).
	// Build the shared unit-cube mesh + lit building material once (process lifetime). The cube is
	// uploaded to the GPU by GenerateUnitCube; the material is white (base colour 1,1,1) so the
	// per-instance albedo tint set per building IS the rendered colour. No emissive (DevilsPlayground
	// colours via the albedo, not a glow). Windowed only — GenerateUnitCube touches the GPU.
	static void EnsureBuildingMeshAssets()
	{
		if (s_pxBuildingCubeMesh != nullptr) { return; }
		s_pxBuildingCubeGeom = new Flux_MeshGeometry();
		Flux_MeshGeometry::GenerateUnitCube(*s_pxBuildingCubeGeom);
		s_pxBuildingCubeMesh = Flux_MeshInstance::CreateFromGeometry(s_pxBuildingCubeGeom);

		auto xhBuildingMat = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
		s_pxBuildingMat = xhBuildingMat.GetDirect();
		s_pxBuildingMat->AddRef();  // process-lifetime pin: this render singleton must survive UnloadUnused
		s_pxBuildingMat->SetName("CBBuildingMaterial");
		s_pxBuildingMat->SetBaseColor(Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f));   // white → per-instance tint is the colour
		s_pxBuildingMat->SetRoughness(0.65f);
		s_pxBuildingMat->SetMetallic(0.0f);
	}

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

	// --- Keyboard-shortcut helpers (driven by OnUpdate's hotkey block + the human test) ---
	void NewCity()
	{
		m_xRoadCtrl.Reset();
		m_xZoning.Reset();
		m_xBuild.Reset();
		m_xDistricts.Reset();
		m_xTransit.Reset();
		m_xConduits.Reset();
		m_uMilestoneMask = 0u;
		m_uLastCarveSegs = 0u;
	}
	void CycleSpeed(int iDir)
	{
		int iS = static_cast<int>(m_eSpeed) + iDir;
		if (iS < static_cast<int>(CB_SIM_PAUSED)) { iS = static_cast<int>(CB_SIM_PAUSED); }
		if (iS > static_cast<int>(CB_SIM_ULTRA))  { iS = static_cast<int>(CB_SIM_ULTRA); }
		m_eSpeed = static_cast<CB_ESimSpeed>(iS);
	}
	const char* QuickSavePath() const { return "cb_quicksave_freeform.dat"; }
	void SaveCity()
	{
		CB_SaveLoadFreeform::SaveToFile(m_xRoadCtrl.GetGraph(), m_xZoning, m_xBuild,
			m_xDistricts, m_xTransit, m_xConduits, QuickSavePath());
	}
	void LoadCity()
	{
		if (CB_SaveLoadFreeform::LoadFromFile(m_xRoadCtrl.GetGraph(), m_xZoning, m_xBuild,
			m_xDistricts, m_xTransit, m_xConduits, QuickSavePath()))
		{
			m_xRoadCtrl.RebuildMesh(m_xHeightfield);
		}
	}

	// Traffic trip endpoints from the live city: residential built lots are HOMES (trip origins),
	// commercial/industrial built lots are JOBS/SHOPS (destinations); each maps to its road
	// segment's node. Rebuilt each tick (cheap). No homes or no jobs ⇒ no trips ⇒ no traffic.
	static constexpr uint32_t POP_PER_CAR = 30;   // ~one concurrent trip per 30 residents
	void BuildTrafficEndpoints()
	{
		m_auTrafficOrigins.Clear();
		m_auTrafficDests.Clear();
		const CB_RoadGraph& xGraph   = m_xRoadCtrl.GetGraph();
		const uint32_t      uSegSlots = xGraph.GetSegmentSlotCount();
		const uint32_t      uLots     = m_xZoning.GetLotSlotCount();
		for (uint32_t i = 0; i < uLots; ++i)
		{
			const CB_Lot& xLot = m_xZoning.GetLot(i);
			if (!xLot.m_bActive || xLot.m_uBuildingId == CB_Zoning::INVALID) { continue; }   // built lots only
			if (xLot.m_uSegment >= uSegSlots || !xGraph.GetSegment(xLot.m_uSegment).m_bActive) { continue; }
			const uint32_t uNode = xGraph.GetSegment(xLot.m_uSegment).m_uNodeA;   // the lot's road-access node
			if (xLot.m_eZone == CB_ZONE_RESIDENTIAL)
			{
				m_auTrafficOrigins.PushBack(uNode);
			}
			else if (xLot.m_eZone == CB_ZONE_COMMERCIAL || xLot.m_eZone == CB_ZONE_INDUSTRIAL)
			{
				m_auTrafficDests.PushBack(uNode);
			}
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

	// ===================== Game UI (SimCity / Cities:Skylines-parity HUD) =====================
	// Built once at runtime (works in BOTH _True and _False — no editor automation needed) onto
	// the entity's UIComponent canvas. Top info bar + speed controls, RCI demand meter, a full
	// tool palette (zones/roads/utilities/services/garbage/sewage/transit/mail/districts/conduit/
	// terraform), and a live info panel. Buttons are real Zenith_UIButtons with click callbacks.

	// Set the active build tool (and, for the services category, the placed service sub-type).
	void SelectUITool(CB_ETool eTool, CB_EBuildingType eService)
	{
		m_xTools.SetTool(eTool);
		if (eTool == CB_TOOL_POLICE && eService != CB_BUILDING_NONE) { m_xRoadCtrl.SetServiceType(eService); }
	}
	void SetUISpeed(CB_ESimSpeed eSpeed) { m_eSpeed = eSpeed; }

	// Button callbacks (UIButtonCallback = void(*)(void*)). The tool + service sub-type are packed
	// into the userdata pointer (tool in byte 0, service in byte 1); the speed is the raw value.
	static void UICb_Tool(void* pUser)
	{
		const uintptr_t uCode = reinterpret_cast<uintptr_t>(pUser);
		if (CB_CityManagerComponent* pxMgr = GetActive())
		{
			pxMgr->SelectUITool(static_cast<CB_ETool>(uCode & 0xFFu), static_cast<CB_EBuildingType>((uCode >> 8) & 0xFFu));
		}
	}
	static void UICb_Speed(void* pUser)
	{
		if (CB_CityManagerComponent* pxMgr = GetActive())
		{
			pxMgr->SetUISpeed(static_cast<CB_ESimSpeed>(reinterpret_cast<uintptr_t>(pUser)));
		}
	}

	struct CB_UIToolDesc { CB_ETool eTool; CB_EBuildingType eService; const char* szLabel; float fR, fG, fB; };

	// The toolbar: one button per tool (services expanded to one button each). Static so both
	// the build and the per-frame active-highlight can read it.
	static const CB_UIToolDesc* ToolDescs(int& iCountOut)
	{
		static const CB_UIToolDesc s_axTools[] = {
			{ CB_TOOL_BULLDOZE,  CB_BUILDING_NONE,         "Bulldoze", 0.82f, 0.28f, 0.22f },
			{ CB_TOOL_ROAD,      CB_BUILDING_NONE,         "Road",     0.52f, 0.54f, 0.58f },
			{ CB_TOOL_ZONE_RES,  CB_BUILDING_NONE,         "Res",      0.24f, 0.72f, 0.32f },
			{ CB_TOOL_ZONE_COM,  CB_BUILDING_NONE,         "Com",      0.24f, 0.48f, 0.88f },
			{ CB_TOOL_ZONE_IND,  CB_BUILDING_NONE,         "Ind",      0.88f, 0.72f, 0.22f },
			{ CB_TOOL_ZONE_PARK, CB_BUILDING_NONE,         "Park",     0.30f, 0.66f, 0.34f },
			{ CB_TOOL_POWER,     CB_BUILDING_NONE,         "Power",    0.90f, 0.80f, 0.22f },
			{ CB_TOOL_WATER,     CB_BUILDING_NONE,         "Water",    0.30f, 0.66f, 0.90f },
			{ CB_TOOL_POLICE,    CB_BUILDING_POLICE,       "Police",   0.22f, 0.32f, 0.76f },
			{ CB_TOOL_POLICE,    CB_BUILDING_FIRE,         "Fire",     0.90f, 0.44f, 0.16f },
			{ CB_TOOL_POLICE,    CB_BUILDING_HOSPITAL,     "Health",   0.88f, 0.30f, 0.40f },
			{ CB_TOOL_POLICE,    CB_BUILDING_SCHOOL,       "School",   0.58f, 0.44f, 0.86f },
			{ CB_TOOL_POLICE,    CB_BUILDING_LANDFILL,     "Garbage",  0.52f, 0.44f, 0.30f },
			{ CB_TOOL_POLICE,    CB_BUILDING_SEWAGE_PLANT, "Sewage",   0.40f, 0.52f, 0.42f },
			{ CB_TOOL_POLICE,    CB_BUILDING_BUS_DEPOT,    "Transit",  0.16f, 0.56f, 0.62f },
			{ CB_TOOL_POLICE,    CB_BUILDING_POST_OFFICE,  "Mail",     0.26f, 0.36f, 0.80f },
			{ CB_TOOL_DISTRICT,  CB_BUILDING_NONE,         "District", 0.68f, 0.48f, 0.86f },
			{ CB_TOOL_TRANSIT,   CB_BUILDING_NONE,         "BusLine",  0.20f, 0.60f, 0.66f },
			{ CB_TOOL_CONDUIT,   CB_BUILDING_NONE,         "Conduit",  0.38f, 0.74f, 0.86f },
			{ CB_TOOL_TERRAFORM, CB_BUILDING_NONE,         "Terrain",  0.60f, 0.50f, 0.34f },
		};
		iCountOut = static_cast<int>(sizeof(s_axTools) / sizeof(s_axTools[0]));
		return s_axTools;
	}

#ifdef ZENITH_INPUT_SIMULATOR
	// Automation/showcase helper: park the simulated cursor at the centre of tool button
	// iTool so its hover tooltip appears (for a screenshot). Re-read each frame so it tracks
	// the live window/canvas size. Returns the tool's tooltip text (or nullptr).
	static const char* ShowcaseHoverTool(int iTool)
	{
		CB_CityManagerComponent* pxMgr = GetActive();
		if (pxMgr == nullptr) { return nullptr; }
		Zenith_UIComponent* pxUI = pxMgr->m_xParentEntity.TryGetComponent<Zenith_UIComponent>();
		if (pxUI == nullptr) { return nullptr; }
		Zenith_UIComponent& xUI = *pxUI;
		char acName[24]; snprintf(acName, sizeof(acName), "CB_Tool_%d", iTool);
		Zenith_UI::Zenith_UIButton* pxBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>(acName);
		if (pxBtn == nullptr) { return nullptr; }
		const Zenith_Maths::Vector4 xB = pxBtn->GetScreenBounds();
		Zenith_InputSimulator::SimulateMousePosition(static_cast<double>((xB.x + xB.z) * 0.5f), static_cast<double>((xB.y + xB.w) * 0.5f));
		int iN = 0; const CB_ToolIcons::Def* pxD = CB_ToolIcons::All(iN);
		return (iTool >= 0 && iTool < iN) ? pxD[iTool].szTooltip : nullptr;
	}
#endif

	void BuildGameUI(Zenith_UIComponent& xUI)
	{
		using namespace Zenith_UI;
		// SetReferenceResolution triggers UpdateSize so GetSize() returns the real window size;
		// then re-set the reference to that size so the scale factor is 1.0 and we lay out in raw
		// window pixels (full width, edges anchored correctly — no aspect-ratio surprises).
		xUI.GetCanvas().SetReferenceResolution(1920.0f, 1080.0f);
		const Zenith_Maths::Vector2 xCv = xUI.GetCanvas().GetSize();
		const float fW = (xCv.x > 200.0f) ? xCv.x : 1920.0f;
		const float fH = (xCv.y > 200.0f) ? xCv.y : 1080.0f;
		xUI.GetCanvas().SetReferenceResolution(fW, fH);
		m_fUIBuiltW = fW; m_fUIBuiltH = fH;   // remember the size we laid out for (rebuild on resize)
		xUI.ClearElements();   // drop the old text-only HUD

		const Zenith_Maths::Vector4 xBG    = { 0.07f, 0.09f, 0.12f, 0.95f };
		const Zenith_Maths::Vector4 xWhite = { 0.92f, 0.94f, 0.97f, 1.0f };
		const Zenith_Maths::Vector4 xDim   = { 0.70f, 0.73f, 0.78f, 1.0f };

		// ---------- Top info bar ----------
		Zenith_UIRect* pxTop = xUI.CreateRect("CB_TopBar");
		pxTop->SetAnchorAndPivot(AnchorPreset::TopLeft);
		pxTop->SetPosition(0.0f, 0.0f); pxTop->SetSize(fW, 50.0f);
		pxTop->SetColor(xBG); pxTop->SetSortOrder(50);

		Zenith_UIText* pxTitle = xUI.CreateText("CB_Title", "ZENITH CITY");
		pxTitle->SetAnchorAndPivot(AnchorPreset::TopLeft);
		pxTitle->SetPosition(22.0f, 11.0f); pxTitle->SetSize(250.0f, 30.0f);
		pxTitle->SetColor({ 0.96f, 0.84f, 0.42f, 1.0f }); pxTitle->SetFontSize(26.0f); pxTitle->SetSortOrder(50);

		auto MakeStat = [&](const char* szName, float fX, float fW)
		{
			Zenith_UIText* p = xUI.CreateText(szName, "");
			p->SetAnchorAndPivot(AnchorPreset::TopLeft);
			p->SetPosition(fX, 14.0f); p->SetSize(fW, 24.0f);
			p->SetColor(xWhite); p->SetFontSize(20.0f); p->SetSortOrder(50);
		};
		MakeStat("CB_UI_Money", 300.0f, 230.0f);
		MakeStat("CB_UI_Pop",   560.0f, 220.0f);
		MakeStat("CB_UI_Happy", 800.0f, 360.0f);

		// ---------- Speed controls (top-right) ----------
		struct SpeedBtn { const char* szName; const char* szLabel; CB_ESimSpeed eSpeed; };
		const SpeedBtn axSpeed[] = {
			{ "CB_UI_Pause", "II",   CB_SIM_PAUSED },
			{ "CB_UI_Play",  ">",    CB_SIM_NORMAL },
			{ "CB_UI_Fast",  ">>",   CB_SIM_FAST   },
		};
		for (int i = 0; i < 3; ++i)
		{
			Zenith_UIButton* p = xUI.CreateButton(axSpeed[i].szName, axSpeed[i].szLabel);
			p->SetAnchorAndPivot(AnchorPreset::TopRight);
			p->SetPosition(-(18.0f + (2 - i) * 46.0f), 8.0f); p->SetSize(42.0f, 34.0f);
			p->SetNormalColor({ 0.20f, 0.23f, 0.28f, 1.0f });
			p->SetHoverColor({ 0.30f, 0.35f, 0.42f, 1.0f });
			p->SetPressedColor({ 0.45f, 0.55f, 0.68f, 1.0f });
			p->SetCornerRadius(4.0f); p->SetFontSize(18.0f); p->SetTextColor(xWhite);
			p->SetOnClick(&UICb_Speed, reinterpret_cast<void*>(static_cast<uintptr_t>(axSpeed[i].eSpeed)));
			p->SetSortOrder(50);
		}

		// ---------- RCI demand meter (bottom-left, above the toolbar) ----------
		Zenith_UIRect* pxRCI = xUI.CreateRect("CB_RCIPanel");
		pxRCI->SetAnchorAndPivot(AnchorPreset::BottomLeft);
		pxRCI->SetPosition(14.0f, -108.0f); pxRCI->SetSize(122.0f, 96.0f);
		pxRCI->SetColor(xBG); pxRCI->SetCornerRadius(6.0f); pxRCI->SetSortOrder(50);
		{
			Zenith_UIText* p = xUI.CreateText("CB_RCILbl", "DEMAND");
			p->SetAnchorAndPivot(AnchorPreset::BottomLeft); p->SetPosition(26.0f, -100.0f); p->SetSize(100.0f, 18.0f);
			p->SetColor(xDim); p->SetFontSize(14.0f); p->SetSortOrder(50);
		}
		struct RCIBar { const char* szName; float fR, fG, fB; };
		const RCIBar axRCI[] = { { "CB_RCI_R", 0.26f, 0.74f, 0.34f }, { "CB_RCI_C", 0.26f, 0.50f, 0.90f }, { "CB_RCI_I", 0.90f, 0.74f, 0.24f } };
		for (int i = 0; i < 3; ++i)
		{
			Zenith_UIRect* p = xUI.CreateRect(axRCI[i].szName);
			p->SetAnchorAndPivot(AnchorPreset::BottomLeft);
			p->SetPosition(28.0f + i * 32.0f, -22.0f); p->SetSize(24.0f, 58.0f);
			p->SetColor({ axRCI[i].fR, axRCI[i].fG, axRCI[i].fB, 1.0f });
			p->SetFillDirection(FillDirection::BottomToTop); p->SetFillAmount(0.5f);
			p->SetCornerRadius(2.0f); p->SetSortOrder(50);
		}

		// ---------- Active-tool label (centred, above the toolbar) ----------
		{
			Zenith_UIText* p = xUI.CreateText("CB_UI_ToolLbl", "");
			p->SetAnchorAndPivot(AnchorPreset::BottomCenter);
			p->SetPosition(0.0f, -100.0f); p->SetSize(480.0f, 24.0f);
			p->SetColor({ 0.96f, 0.90f, 0.60f, 1.0f }); p->SetFontSize(20.0f); p->SetSortOrder(50);
		}

		// ---------- Info panel (top-right, below the bar) ----------
		Zenith_UIRect* pxInfo = xUI.CreateRect("CB_InfoPanel");
		pxInfo->SetAnchorAndPivot(AnchorPreset::TopRight);
		pxInfo->SetPosition(-14.0f, 60.0f); pxInfo->SetSize(290.0f, 178.0f);
		pxInfo->SetColor(xBG); pxInfo->SetCornerRadius(6.0f); pxInfo->SetSortOrder(50);
		{
			const char* aszInfo[] = { "CB_UI_Budget", "CB_UI_Power", "CB_UI_Water", "CB_UI_Env", "CB_UI_Svc" };
			for (int i = 0; i < 5; ++i)
			{
				Zenith_UIText* p = xUI.CreateText(aszInfo[i], "");
				p->SetAnchorAndPivot(AnchorPreset::TopRight);
				p->SetPosition(-26.0f, 72.0f + i * 30.0f); p->SetSize(264.0f, 26.0f);
				p->SetColor(xWhite); p->SetFontSize(18.0f); p->SetSortOrder(50);
			}
		}

		// ---------- Bottom tool palette ----------
		Zenith_UIRect* pxBar = xUI.CreateRect("CB_ToolBar");
		pxBar->SetAnchorAndPivot(AnchorPreset::BottomLeft);
		pxBar->SetPosition(0.0f, 0.0f); pxBar->SetSize(fW, 92.0f);
		pxBar->SetColor(xBG); pxBar->SetSortOrder(50);

		int iTools = 0;
		const CB_UIToolDesc* pxTools = ToolDescs(iTools);
		int iIcons = 0;
		const CB_ToolIcons::Def* pxIcons = CB_ToolIcons::All(iIcons);
		const float fGap = 4.0f;
		// Adaptive button width so all tools always fit the bar (shrinks on narrow windows).
		const float fFitW = (fW - 24.0f) / iTools - fGap;
		const float fBtnW = (fFitW < 90.0f) ? fFitW : 90.0f;
		const float fBtnH = 66.0f;
		const float fIcon = ((fBtnW < fBtnH) ? fBtnW : fBtnH) - 16.0f;   // square icon, centred
		const float fTotalW = iTools * fBtnW + (iTools - 1) * fGap;
		const float fStartX = (fW - fTotalW) * 0.5f;
		for (int i = 0; i < iTools; ++i)
		{
			char acName[24];
			snprintf(acName, sizeof(acName), "CB_Tool_%d", i);
			// Icon-only button (the procedural glyph reads the tool; hover shows the description).
			Zenith_UIButton* p = xUI.CreateButton(acName, "");
			p->SetAnchorAndPivot(AnchorPreset::BottomLeft);
			p->SetPosition(fStartX + i * (fBtnW + fGap), -13.0f); p->SetSize(fBtnW, fBtnH);
			p->SetNormalColor({ pxTools[i].fR * 0.55f, pxTools[i].fG * 0.55f, pxTools[i].fB * 0.55f, 1.0f });
			p->SetHoverColor({ pxTools[i].fR * 0.82f, pxTools[i].fG * 0.82f, pxTools[i].fB * 0.82f, 1.0f });
			p->SetPressedColor({ pxTools[i].fR, pxTools[i].fG, pxTools[i].fB, 1.0f });
			p->SetCornerRadius(5.0f); p->SetFontSize(15.0f); p->SetTextColor(xWhite);
			p->SetBorderColor({ 0.95f, 0.92f, 0.55f, 1.0f }); p->SetBorderThickness(0.0f);
			const uintptr_t uCode = static_cast<uintptr_t>(pxTools[i].eTool) | (static_cast<uintptr_t>(pxTools[i].eService) << 8);
			p->SetOnClick(&UICb_Tool, reinterpret_cast<void*>(uCode));
			p->SetSortOrder(50);

			// Icon: a UIImage overlay centred on the button (UIButton's own ICON_ONLY path does
			// not render the bindless texture; UIImage does + marks the texture bindless itself).
			if (i < iIcons)
			{
				char acIcon[28];
				snprintf(acIcon, sizeof(acIcon), "CB_ToolIcon_%d", i);
				Zenith_UIImage* pxImg = xUI.CreateImage(acIcon);
				pxImg->SetAnchorAndPivot(AnchorPreset::BottomLeft);
				pxImg->SetPosition(fStartX + i * (fBtnW + fGap) + (fBtnW - fIcon) * 0.5f, -13.0f - (fBtnH - fIcon) * 0.5f);
				pxImg->SetSize(fIcon, fIcon);
				pxImg->SetTexturePath(std::string(GAME_ASSETS_DIR) + "UI/Icons/cb_" + pxIcons[i].szIcon + ".ztxtr");
				pxImg->SetSortOrder(51);
			}
		}

		// ---------- Hover tooltip (positioned over the hovered tool button each frame) ----------
		{
			Zenith_UIRect* pxTip = xUI.CreateRect("CB_Tooltip");
			pxTip->SetAnchorAndPivot(AnchorPreset::TopLeft);
			pxTip->SetSize(240.0f, 34.0f);
			pxTip->SetColor({ 0.04f, 0.05f, 0.08f, 0.96f });
			pxTip->SetCornerRadius(5.0f);
			pxTip->SetBorderColor({ 0.95f, 0.92f, 0.55f, 0.9f }); pxTip->SetBorderThickness(1.5f);
			pxTip->SetSortOrder(60); pxTip->SetVisible(false);

			Zenith_UIText* pxTipTxt = xUI.CreateText("CB_TooltipTxt", "");
			pxTipTxt->SetAnchorAndPivot(AnchorPreset::TopLeft);
			pxTipTxt->SetSize(220.0f, 26.0f);
			pxTipTxt->SetColor({ 0.96f, 0.97f, 0.99f, 1.0f });
			pxTipTxt->SetFontSize(17.0f); pxTipTxt->SetSortOrder(61); pxTipTxt->SetVisible(false);
		}

	}

	// Show the hovered tool button's description in the tooltip, positioned above it.
	void UpdateToolTooltip(Zenith_UIComponent& xUI)
	{
		using namespace Zenith_UI;
		Zenith_UIRect* pxTip = xUI.FindElement<Zenith_UIRect>("CB_Tooltip");
		Zenith_UIText* pxTxt = xUI.FindElement<Zenith_UIText>("CB_TooltipTxt");
		if (!pxTip || !pxTxt) { return; }

		int iIcons = 0;
		const CB_ToolIcons::Def* pxIcons = CB_ToolIcons::All(iIcons);
		Zenith_UIButton* pxHover = nullptr;
		const char* szTip = nullptr;
		for (int i = 0; i < iIcons; ++i)
		{
			char acName[24]; snprintf(acName, sizeof(acName), "CB_Tool_%d", i);
			Zenith_UIButton* b = xUI.FindElement<Zenith_UIButton>(acName);
			if (b && b->GetState() == Zenith_UIButton::ButtonState::HOVERED) { pxHover = b; szTip = pxIcons[i].szTooltip; break; }
		}

		if (!pxHover || !szTip)
		{
			pxTip->SetVisible(false); pxTxt->SetVisible(false);
			return;
		}

		const float fFont = 17.0f;
		const float fTipW = 20.0f + static_cast<float>(strlen(szTip)) * fFont * 0.52f;
		const float fTipH = 32.0f;
		const Zenith_Maths::Vector4 xB = pxHover->GetScreenBounds();
		const Zenith_Maths::Vector2 xCanvas = xUI.GetCanvas().GetSize();
		float fTx = (xB.x + xB.z) * 0.5f - fTipW * 0.5f;
		if (fTx < 6.0f) { fTx = 6.0f; }
		if (fTx + fTipW > static_cast<float>(xCanvas.x) - 6.0f) { fTx = static_cast<float>(xCanvas.x) - 6.0f - fTipW; }
		const float fTy = xB.y - fTipH - 8.0f;

		pxTip->SetPosition(fTx, fTy); pxTip->SetSize(fTipW, fTipH); pxTip->SetVisible(true);
		pxTxt->SetPosition(fTx + 11.0f, fTy + 7.0f); pxTxt->SetSize(fTipW - 22.0f, fTipH - 12.0f);
		pxTxt->SetText(szTip); pxTxt->SetFontSize(fFont); pxTxt->SetVisible(true);
	}

	void UpdateGameUI(Zenith_UIComponent& xUI)
	{
		using namespace Zenith_UI;
		char acBuf[160];
		UpdateToolTooltip(xUI);

		if (Zenith_UIText* p = xUI.FindElement<Zenith_UIText>("CB_UI_Money"))
		{
			snprintf(acBuf, sizeof(acBuf), "$%d   Tax %d%%", static_cast<int>(m_xBuild.GetTreasury()), static_cast<int>(m_xBuild.GetTaxRate() * 100.0f));
			p->SetText(acBuf);
			p->SetColor(m_xBuild.GetTreasury() >= 0.0f ? Zenith_Maths::Vector4{ 0.55f, 0.90f, 0.55f, 1.0f } : Zenith_Maths::Vector4{ 0.95f, 0.45f, 0.45f, 1.0f });
		}
		if (Zenith_UIText* p = xUI.FindElement<Zenith_UIText>("CB_UI_Pop"))
		{
			snprintf(acBuf, sizeof(acBuf), "Pop %u  Jobs %u", m_xBuild.GetPopulation(), m_xBuild.GetJobs());
			p->SetText(acBuf);
		}
		if (Zenith_UIText* p = xUI.FindElement<Zenith_UIText>("CB_UI_Happy"))
		{
			snprintf(acBuf, sizeof(acBuf), "Happy %d%%   Bldgs %u   Svc %u", static_cast<int>(m_xBuild.GetHappiness() * 100.0f), m_xBuild.GetActiveBuildings(), m_xBuild.GetActiveServices());
			p->SetText(acBuf);
		}

		if (Zenith_UIRect* p = xUI.FindElement<Zenith_UIRect>("CB_RCI_R")) { p->SetFillAmount(m_xBuild.GetResDemand()); }
		if (Zenith_UIRect* p = xUI.FindElement<Zenith_UIRect>("CB_RCI_C")) { p->SetFillAmount(m_xBuild.GetComDemand()); }
		if (Zenith_UIRect* p = xUI.FindElement<Zenith_UIRect>("CB_RCI_I")) { p->SetFillAmount(m_xBuild.GetIndDemand()); }

		if (Zenith_UIText* p = xUI.FindElement<Zenith_UIText>("CB_UI_Budget"))
		{
			snprintf(acBuf, sizeof(acBuf), "Treasury $%d  Debt $%d", static_cast<int>(m_xBuild.GetTreasury()), static_cast<int>(m_xBuild.GetDebt()));
			p->SetText(acBuf);
		}
		if (Zenith_UIText* p = xUI.FindElement<Zenith_UIText>("CB_UI_Power"))
		{
			snprintf(acBuf, sizeof(acBuf), "Power %d / %d", static_cast<int>(m_xBuild.GetPowerConsumed()), static_cast<int>(m_xBuild.GetPowerProduced()));
			p->SetText(acBuf);
		}
		if (Zenith_UIText* p = xUI.FindElement<Zenith_UIText>("CB_UI_Water"))
		{
			snprintf(acBuf, sizeof(acBuf), "Water %d / %d", static_cast<int>(m_xBuild.GetWaterConsumed()), static_cast<int>(m_xBuild.GetWaterProduced()));
			p->SetText(acBuf);
		}
		if (Zenith_UIText* p = xUI.FindElement<Zenith_UIText>("CB_UI_Env"))
		{
			snprintf(acBuf, sizeof(acBuf), "Poll %d%%  Traffic %d%%  Fires %u", static_cast<int>(m_xBuild.GetPollution() * 100.0f), static_cast<int>(m_xBuild.GetCongestion() * 100.0f), m_xBuild.GetActiveFires());
			p->SetText(acBuf);
		}
		if (Zenith_UIText* p = xUI.FindElement<Zenith_UIText>("CB_UI_Svc"))
		{
			snprintf(acBuf, sizeof(acBuf), "Garbage %d%%  Sewage %d%%", static_cast<int>(m_xBuild.GetGarbage() * 100.0f), static_cast<int>(m_xBuild.GetSewage() * 100.0f));
			p->SetText(acBuf);
		}

		// Active-tool label + highlight the matching toolbar button (border on).
		const CB_ETool eTool = m_xTools.GetTool();
		const CB_EBuildingType eSvc = m_xRoadCtrl.GetServiceType();
		if (Zenith_UIText* p = xUI.FindElement<Zenith_UIText>("CB_UI_ToolLbl"))
		{
			snprintf(acBuf, sizeof(acBuf), "%s", eTool == CB_TOOL_NONE ? "" : CB_ToolSystem::ToolName(eTool));
			p->SetText(acBuf);
		}
		int iTools = 0;
		const CB_UIToolDesc* pxTools = ToolDescs(iTools);
		for (int i = 0; i < iTools; ++i)
		{
			char acName[24];
			snprintf(acName, sizeof(acName), "CB_Tool_%d", i);
			if (Zenith_UIButton* p = xUI.FindElement<Zenith_UIButton>(acName))
			{
				const bool bActive = (pxTools[i].eTool == eTool) &&
					(eTool != CB_TOOL_POLICE || pxTools[i].eService == eSvc);
				p->SetBorderThickness(bActive ? 3.0f : 0.0f);
			}
		}
	}

	void UpdateHUD()
	{
		Zenith_UIComponent* pxUI = m_xParentEntity.TryGetComponent<Zenith_UIComponent>();
		if (pxUI == nullptr)
		{
			return;
		}
		Zenith_UIComponent& xUI = *pxUI;
		char acBuf[192];
		if (Zenith_UI::Zenith_UIText* p = xUI.FindElement<Zenith_UI::Zenith_UIText>("CB_Pop"))    { snprintf(acBuf, sizeof(acBuf), "Population: %u   Jobs: %u   Bldgs: %u   Services: %u", m_xBuild.GetPopulation(), m_xBuild.GetJobs(), m_xBuild.GetActiveBuildings(), m_xBuild.GetActiveServices()); p->SetText(acBuf); }
		if (Zenith_UI::Zenith_UIText* p = xUI.FindElement<Zenith_UI::Zenith_UIText>("CB_Money"))  { snprintf(acBuf, sizeof(acBuf), "Treasury: $%d   Tax: %d%%   Debt: $%d", static_cast<int>(m_xBuild.GetTreasury()), static_cast<int>(m_xBuild.GetTaxRate() * 100.0f), static_cast<int>(m_xBuild.GetDebt())); p->SetText(acBuf); }
		if (Zenith_UI::Zenith_UIText* p = xUI.FindElement<Zenith_UI::Zenith_UIText>("CB_Happy"))  { snprintf(acBuf, sizeof(acBuf), "Happy %d%%  Poll %d%%  Traffic %d%%  Garbage %d%%  Sewage %d%%  Transit %d%%", static_cast<int>(m_xBuild.GetHappiness() * 100.0f), static_cast<int>(m_xBuild.GetPollution() * 100.0f), static_cast<int>(m_xBuild.GetCongestion() * 100.0f), static_cast<int>(m_xBuild.GetGarbage() * 100.0f), static_cast<int>(m_xBuild.GetSewage() * 100.0f), static_cast<int>(m_xBuild.GetTransitShare() * 100.0f)); p->SetText(acBuf); }
		if (Zenith_UI::Zenith_UIText* p = xUI.FindElement<Zenith_UI::Zenith_UIText>("CB_Demand")) { snprintf(acBuf, sizeof(acBuf), "Demand R%d C%d I%d  Pwr %d/%d Wtr %d/%d  Mail %d%% Freight %d%% Fires %u", static_cast<int>(m_xBuild.GetResDemand() * 100.0f), static_cast<int>(m_xBuild.GetComDemand() * 100.0f), static_cast<int>(m_xBuild.GetIndDemand() * 100.0f), static_cast<int>(m_xBuild.GetPowerConsumed()), static_cast<int>(m_xBuild.GetPowerProduced()), static_cast<int>(m_xBuild.GetWaterConsumed()), static_cast<int>(m_xBuild.GetWaterProduced()), static_cast<int>(m_xBuild.GetMail() * 100.0f), static_cast<int>(m_xBuild.GetFreightRatio() * 100.0f), m_xBuild.GetActiveFires()); p->SetText(acBuf); }
		if (Zenith_UI::Zenith_UIText* p = xUI.FindElement<Zenith_UI::Zenith_UIText>("CB_Speed"))
		{
			const uint32_t uPol = m_xDistricts.GetCityPolicyMask();
			snprintf(acBuf, sizeof(acBuf), "%s  [P]   Districts:%u  Policy:%s%s%s%s",
				m_eSpeed == CB_SIM_PAUSED ? "PAUSED" : "PLAYING", m_xDistricts.GetActiveCount(),
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

	void OnDestroy()
	{
		// Clear the stream-in carve hook first: the engine holds a raw pointer to m_xHeightfield,
		// which is about to be destroyed with this component. (Safe if the terrain is already gone.)
		if (m_pxTerrain != nullptr) { CB_RoadTerrain::UnregisterStreamHook(m_pxTerrain); }
		s_bStarted = false;
		if (s_pxActiveHeightfield == &m_xHeightfield)
		{
			s_pxActiveHeightfield = nullptr;
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
	CB_ESimSpeed                 GetSpeed() const       { return m_eSpeed; }
	void                         SetSpeed(CB_ESimSpeed eSpeed) { m_eSpeed = eSpeed; }
	CB_ToolSystem&               GetTools()             { return m_xTools; }
	CB_RoadController&           GetRoadController()    { return m_xRoadCtrl; }
	const CB_RoadController&     GetRoadController() const { return m_xRoadCtrl; }
	CB_Zoning&                   GetZoning()            { return m_xZoning; }
	const CB_Zoning&             GetZoning() const      { return m_xZoning; }
	CB_BuildingPlacement&        GetBuild()             { return m_xBuild; }
	const CB_BuildingPlacement&  GetBuild() const       { return m_xBuild; }

	// Live CityManager instance (null when none active).
	static CB_CityManagerComponent* GetActive() { return s_pxActive; }

	// Live free-form road controller (null when no CityManager is active).
	static CB_RoadController* GetActiveRoadController() { return s_pxActiveRoadCtrl; }
	static CB_Zoning*         GetActiveZoning()         { return s_pxActiveZoning; }
	static CB_BuildingPlacement* GetActiveBuild()       { return s_pxActiveBuild; }
	static CB_Districts*      GetActiveDistricts()      { return s_pxActiveDistricts; }
	static CB_TransitLines*   GetActiveTransit()        { return s_pxActiveTransit; }
	static CB_Conduits*       GetActiveConduits()       { return s_pxActiveConduits; }
	static CB_Traffic*        GetActiveTraffic()        { return s_pxActiveTraffic; }

	// Liveness accessors consumed by the CB_Boot automated test.
	static bool     WasStarted()     { return s_bStarted; }
	static uint32_t GetUpdateCount() { return s_uUpdateCount; }
	// Telemetry: number of available-placement-zone ghosts drawn last frame (0 unless an R/C/I
	// zone tool is active). Proves the placement-ghost affordance is live + tracks tool state.
	static uint32_t GetLastGhostCount() { return s_uLastGhostCount; }

	// Active-subsystem accessors for game-side bridges/tests (null when no
	// CityManager is live).
	static CB_TerrainHeightfield* GetActiveHeightfield() { return s_pxActiveHeightfield; }

private:
	// Shared move tail: re-wire every pointer that captured a MEMBER address of
	// the source (sibling subsystems, published statics, the engine stream hook),
	// then neuter the source so its destruction can't disturb the live wiring.
	void AdoptFrom(CB_CityManagerComponent& xOther) noexcept
	{
		// Sibling-subsystem wiring (these held &xOther.m_xDistricts etc.).
		m_xBuild.SetDistricts(&m_xDistricts);
		m_xBuild.SetTransitLines(&m_xTransit);
		m_xBuild.SetConduits(&m_xConduits);
		m_xTools.SetTerrainField(&m_xHeightfield);

		// Published static accessors (tests / automation reach the live city through these).
		if (s_pxActive == &xOther)                           { s_pxActive = this; }
		if (s_pxActiveHeightfield == &xOther.m_xHeightfield) { s_pxActiveHeightfield = &m_xHeightfield; }
		if (s_pxActiveRoadCtrl == &xOther.m_xRoadCtrl)       { s_pxActiveRoadCtrl = &m_xRoadCtrl; }
		if (s_pxActiveZoning == &xOther.m_xZoning)           { s_pxActiveZoning = &m_xZoning; }
		if (s_pxActiveBuild == &xOther.m_xBuild)             { s_pxActiveBuild = &m_xBuild; }
		if (s_pxActiveDistricts == &xOther.m_xDistricts)     { s_pxActiveDistricts = &m_xDistricts; }
		if (s_pxActiveTransit == &xOther.m_xTransit)         { s_pxActiveTransit = &m_xTransit; }
		if (s_pxActiveConduits == &xOther.m_xConduits)       { s_pxActiveConduits = &m_xConduits; }
		if (s_pxActiveTraffic == &xOther.m_xTraffic)         { s_pxActiveTraffic = &m_xTraffic; }

		// Engine terrain stream hook: its user pointer is the heightfield's address.
		// Re-register so it points at THIS object's field (idempotent + null-safe).
		if (m_pxTerrain != nullptr)
		{
			CB_RoadTerrain::RegisterStreamHook(m_pxTerrain, m_xHeightfield);
		}

		// Neuter the source: its OnDestroy/dtor must not unregister the live hook.
		xOther.m_pxTerrain = nullptr;
		xOther.m_pxBuildingInst = nullptr;
	}

	Zenith_Entity m_xParentEntity;
	CB_TerrainHeightfield m_xHeightfield;
	CB_ESimSpeed          m_eSpeed = CB_SIM_NORMAL;   // sim clock (pause/normal/fast/ultra); gates growth + scales traffic dt
	CB_ToolSystem         m_xTools;
	CB_RoadController     m_xRoadCtrl;             // free-form spline road network + draw tool
	CB_Zoning             m_xZoning;               // road-relative frontage lots + zone paint
	CB_BuildingPlacement  m_xBuild;                // demand-driven zoned building growth
	CB_Districts          m_xDistricts;            // named regions + city/district policy ordinances
	CB_TransitLines       m_xTransit;              // public-transport lines + stops (ridership reach)
	CB_Conduits           m_xConduits;             // utility conduit network (power/water reach)
	CB_Traffic            m_xTraffic;             // demand-driven OD-trip road traffic (home→work/shop)
	Zenith_Vector<uint32_t> m_auTrafficOrigins;   // home nodes (trip origins), rebuilt each tick
	Zenith_Vector<uint32_t> m_auTrafficDests;     // job/shop nodes (trip destinations), rebuilt each tick
	Zenith_TerrainComponent* m_pxTerrain   = nullptr;  // cached GPU terrain for the road carve
	uint32_t                 m_uLastCarveSegs = 0;     // re-carve when the road count changes
	CB_RoadTerrain::CarveContext m_xCarveCtx;          // road samples for the stream-in carve hook (engine holds a ptr to this)
	// Buildings render as instanced cube meshes (lit PBR material, per-instance albedo tint =
	// zone colour). The component lives on a render-only "CityBuildings" entity (created windowed
	// in OnStart); the shared unit-cube mesh + material are process-lifetime singletons.
	Zenith_InstancedMeshComponent* m_pxBuildingInst = nullptr;
	static inline Flux_MeshGeometry*    s_pxBuildingCubeGeom = nullptr;
	static inline Flux_MeshInstance*    s_pxBuildingCubeMesh = nullptr;
	static inline Zenith_MaterialAsset* s_pxBuildingMat      = nullptr;
	uint32_t                 m_uTerraformTick  = 0;    // throttles the terraform GPU re-upload while dragging
	bool                     m_bUIBuilt = false;       // the parity game UI is built on the first windowed frame
	float                    m_fUIBuiltW = 0.0f;       // canvas size the UI was last laid out for (rebuild on resize)
	float                    m_fUIBuiltH = 0.0f;
	bool                     m_bPrevTransitLeft  = false;  // transit-tool click edge latches
	bool                     m_bPrevTransitRight = false;

	uint32_t              m_uMilestoneMask = 0;   // population-threshold dispatch tracking

	static inline bool                       s_bStarted            = false;
	static inline uint32_t                   s_uUpdateCount        = 0;
	static inline uint32_t                   s_uLastGhostCount     = 0;   // available-placement ghosts drawn last frame
	static inline CB_TerrainHeightfield*     s_pxActiveHeightfield = nullptr;
	static inline CB_CityManagerComponent*  s_pxActive            = nullptr;
	static inline CB_RoadController*         s_pxActiveRoadCtrl    = nullptr;
	static inline CB_Zoning*                 s_pxActiveZoning      = nullptr;
	static inline CB_BuildingPlacement*      s_pxActiveBuild       = nullptr;
	static inline CB_Districts*              s_pxActiveDistricts   = nullptr;
	static inline CB_TransitLines*           s_pxActiveTransit     = nullptr;
	static inline CB_Conduits*               s_pxActiveConduits    = nullptr;
	static inline CB_Traffic*                s_pxActiveTraffic     = nullptr;
};
