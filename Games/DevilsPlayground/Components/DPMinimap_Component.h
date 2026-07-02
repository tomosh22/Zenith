#pragma once
#include "Core/Zenith_Engine.h"
/**
 * DPMinimap_Component - 2D vector minimap panel (2026-07-01).
 *
 * Draws a schematic top-down plan of the procgen level in the bottom-right
 * corner: one Zenith_UIRect per room (revealed through the fog-memory
 * table, colour/alpha ageing on the SAME visibility curve the fog texture
 * uses), plus icons for the possessed villager (gold, always shown while
 * possessed) and every other villager whose position is remembered-visible.
 * The priest is deliberately NOT drawn — his position is the game's threat
 * information and the world fog already gates it.
 *
 * Lives on the GameManager entity next to DPHUDController (one component,
 * one responsibility). All pure maths lives in DP_Minimap (Source/); this
 * class is UI wiring only. UI elements are owned by the entity's
 * Zenith_UICanvas (heap-allocated, address-stable), so the cached element
 * pointers survive component-pool relocation and the implicit moves are
 * correct — this component deliberately holds no this-capturing
 * subscriptions and no singleton.
 *
 * Inert outside ProcLevel: without a DPProcLevelBootstrap layout (FrontEnd,
 * gyms) OnStart builds nothing and OnUpdate early-outs.
 */

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "DataStream/Zenith_DataStream.h"
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"

#include "Source/PublicInterfaces.h"
#include "Source/DP_Minimap.h"
#include "Source/DP_Tuning.h"
#include "../Components/DPProcLevelBootstrap_Component.h"  // Contract exception: reads the cached procgen layout (creation-time geometry, not gameplay state)
#include "../Components/DPVillager_Component.h"            // Contract exception: villager icons iterate DPVillager via DP_Query (type tag only)

#include <algorithm>
#include <cstdio>

class DPMinimap_Component ZENITH_FINAL
{
public:
	DPMinimap_Component() = delete;
	DPMinimap_Component(Zenith_Entity& xParentEntity)
		: m_xParentEntity(xParentEntity)
	{}

	void OnStart()
	{
		BuildElements();
	}

	void OnUpdate(const float /*fDt*/)
	{
		if (!m_bBuilt) return;
		RefreshRooms();
		RefreshVillagerIcons();
	}

	// Component contract: version-only payload (all state rebuilds from the
	// procgen layout + fog memory each run).
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
	void RenderPropertiesPanel() {}
#endif

	// Test observability.
	bool     IsBuilt() const { return m_bBuilt; }
	uint32_t GetRoomRectCount() const { return m_apxRoomRects.GetSize(); }
	const Zenith_UI::Zenith_UIRect* GetRoomRect(uint32_t u) const { return m_apxRoomRects.Get(u); }
	const Zenith_UI::Zenith_UIRect* GetPlayerIcon() const { return m_pxPlayerIcon; }
	const DP_Minimap::MapView& GetMapView() const { return m_xView; }

	// Panel top-left in the BottomRight-anchored coordinate space every
	// minimap element positions in (negative offsets from screen corner).
	static Zenith_Maths::Vector2 PanelOrigin()
	{
		return Zenith_Maths::Vector2(
			-DP_Minimap::fPANEL_MARGIN_PX - DP_Minimap::fPANEL_SIZE_PX,
			-DP_Minimap::fPANEL_MARGIN_PX - DP_Minimap::fPANEL_SIZE_PX);
	}

private:
	void BuildElements()
	{
		m_bBuilt = false;

		DPProcLevelBootstrap_Component* pxBootstrap = DPProcLevelBootstrap_Component::Instance();
		if (pxBootstrap == nullptr) return; // non-procgen scene: stay inert
		const DPProcLevel::LevelLayout& xLayout = pxBootstrap->GetLayout();
		if (xLayout.axRooms.GetSize() == 0) return;

		Zenith_UIComponent* pxUI = m_xParentEntity.TryGetComponent<Zenith_UIComponent>();
		if (pxUI == nullptr) return;

		m_xView = DP_Minimap::BuildMapView(
			xLayout.fBoundsMinX, xLayout.fBoundsMinZ,
			xLayout.fBoundsMaxX, xLayout.fBoundsMaxZ);

		// Hot tuning thresholds cached once (DP_Tuning doctrine). The
		// Mereworth's-Eye memory scale is fixed for the run (unlocks only
		// change between runs at the Liminal), so caching it here is safe
		// and keeps the minimap on the same curve as the fog texture.
		const float fMemoryScale = DP_MetaSave::GetFogMemoryScale();
		m_fVisibleS = DP_Tuning::Get<float>("fog_of_war.memory_visible_s") * fMemoryScale;
		m_fDimS     = DP_Tuning::Get<float>("fog_of_war.memory_dim_s") * fMemoryScale;
		m_fDimVis   = DP_Tuning::Get<float>("fog_of_war.memory_dim_visibility");
		m_fFadeS    = DP_Tuning::Get<float>("fog_of_war.memory_hidden_fade_s");

		const Zenith_Maths::Vector2 xOrigin = PanelOrigin();

		// Backdrop.
		Zenith_UI::Zenith_UIRect* pxBackdrop = pxUI->CreateRect("MinimapBackdrop");
		pxBackdrop->SetAnchorAndPivot(Zenith_UI::AnchorPreset::BottomRight);
		pxBackdrop->SetPivot(Zenith_UI::AnchorPreset::TopLeft);
		pxBackdrop->SetPosition(xOrigin.x, xOrigin.y);
		pxBackdrop->SetSize(DP_Minimap::fPANEL_SIZE_PX, DP_Minimap::fPANEL_SIZE_PX);
		pxBackdrop->SetColor(Zenith_Maths::Vector4(0.05f, 0.05f, 0.08f, 0.55f));
		pxBackdrop->SetSortOrder(100);

		// One rect per room; colour refreshed per frame from fog memory.
		m_apxRoomRects.Clear();
		const uint32_t uRooms = xLayout.axRooms.GetSize();
		for (uint32_t u = 0; u < uRooms; ++u)
		{
			const auto& xRoom = xLayout.axRooms.Get(u);
			Vec2 xTopLeft, xSize;
			DP_Minimap::RoomPanelRect(m_xView,
				xRoom.fCentreX, xRoom.fCentreZ,
				xRoom.fHalfExtentX, xRoom.fHalfExtentZ, xRoom.fYawRadians,
				xTopLeft, xSize);

			char szName[32];
			std::snprintf(szName, sizeof(szName), "MinimapRoom%u", u);
			Zenith_UI::Zenith_UIRect* pxRect = pxUI->CreateRect(szName);
			pxRect->SetAnchorAndPivot(Zenith_UI::AnchorPreset::BottomRight);
			pxRect->SetPivot(Zenith_UI::AnchorPreset::TopLeft);
			pxRect->SetPosition(xOrigin.x + xTopLeft.x, xOrigin.y + xTopLeft.y);
			pxRect->SetSize(xSize.x, xSize.y);
			pxRect->SetColor(Zenith_Maths::Vector4(0.0f, 0.0f, 0.0f, 0.0f));
			pxRect->SetSortOrder(101);
			m_apxRoomRects.PushBack(pxRect);
		}

		// Villager icon pool (one per spawn; hidden until used) + player icon.
		m_apxVillagerIcons.Clear();
		const uint32_t uVillagers = xLayout.axVillagerSpawns.GetSize();
		for (uint32_t u = 0; u < uVillagers; ++u)
		{
			char szName[32];
			std::snprintf(szName, sizeof(szName), "MinimapVillager%u", u);
			Zenith_UI::Zenith_UIRect* pxIcon = pxUI->CreateRect(szName);
			pxIcon->SetAnchorAndPivot(Zenith_UI::AnchorPreset::BottomRight);
			pxIcon->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxIcon->SetSize(5.0f, 5.0f);
			pxIcon->SetColor(Zenith_Maths::Vector4(0.85f, 0.85f, 0.92f, 0.9f));
			pxIcon->SetSortOrder(102);
			pxIcon->SetVisible(false);
			m_apxVillagerIcons.PushBack(pxIcon);
		}

		m_pxPlayerIcon = pxUI->CreateRect("MinimapPlayer");
		m_pxPlayerIcon->SetAnchorAndPivot(Zenith_UI::AnchorPreset::BottomRight);
		m_pxPlayerIcon->SetPivot(Zenith_UI::AnchorPreset::Center);
		m_pxPlayerIcon->SetSize(7.0f, 7.0f);
		m_pxPlayerIcon->SetColor(Zenith_Maths::Vector4(0.95f, 0.80f, 0.30f, 1.0f));
		m_pxPlayerIcon->SetSortOrder(103);
		m_pxPlayerIcon->SetVisible(false);

		m_bBuilt = true;
	}

	void RefreshRooms()
	{
		DPProcLevelBootstrap_Component* pxBootstrap = DPProcLevelBootstrap_Component::Instance();
		if (pxBootstrap == nullptr) return;
		const DPProcLevel::LevelLayout& xLayout = pxBootstrap->GetLayout();
		const uint32_t uRooms = std::min(xLayout.axRooms.GetSize(), m_apxRoomRects.GetSize());

		for (uint32_t u = 0; u < uRooms; ++u)
		{
			const auto& xRoom = xLayout.axRooms.Get(u);
			// Sample a centre + mid-ring + near-corner spread and keep the
			// freshest memory — a villager crossing a room (including a
			// wall-hugging perimeter walk) should reveal it without having
			// to touch the exact centre cell. Probes are axis-aligned;
			// for yawed rooms this is the same AABB approximation the
			// drawing uses (a coarse but consistent v1 trade-off).
			const float fQX = xRoom.fHalfExtentX * 0.5f;
			const float fQZ = xRoom.fHalfExtentZ * 0.5f;
			const float fEX = xRoom.fHalfExtentX * 0.85f;
			const float fEZ = xRoom.fHalfExtentZ * 0.85f;
			const Zenith_Maths::Vector3 axProbes[9] = {
				{ xRoom.fCentreX,       0.0f, xRoom.fCentreZ       },
				{ xRoom.fCentreX + fQX, 0.0f, xRoom.fCentreZ + fQZ },
				{ xRoom.fCentreX - fQX, 0.0f, xRoom.fCentreZ + fQZ },
				{ xRoom.fCentreX + fQX, 0.0f, xRoom.fCentreZ - fQZ },
				{ xRoom.fCentreX - fQX, 0.0f, xRoom.fCentreZ - fQZ },
				{ xRoom.fCentreX + fEX, 0.0f, xRoom.fCentreZ + fEZ },
				{ xRoom.fCentreX - fEX, 0.0f, xRoom.fCentreZ + fEZ },
				{ xRoom.fCentreX + fEX, 0.0f, xRoom.fCentreZ - fEZ },
				{ xRoom.fCentreX - fEX, 0.0f, xRoom.fCentreZ - fEZ },
			};

			uint8_t uBestVis = 0;
			bool bAnySeen = false;
			for (int iP = 0; iP < 9; ++iP)
			{
				const float fAge = DP_Fog::GetMemoryAgeAt(axProbes[iP]);
				if (fAge < 0.0f) continue;
				bAnySeen = true;
				const uint8_t uVis = DP_Fog::MemoryVisibilityForAge(
					fAge, m_fVisibleS, m_fDimS, m_fDimVis, m_fFadeS);
				if (uVis > uBestVis) uBestVis = uVis;
			}

			const DP_Fog::MemoryTileState eState = bAnySeen
				? (uBestVis == 255u ? DP_Fog::MemoryTileState::VisitedVisible
				                    : (uBestVis > 0u ? DP_Fog::MemoryTileState::VisitedDim
				                                     : DP_Fog::MemoryTileState::VisitedHidden))
				: DP_Fog::MemoryTileState::NeverSeen;

			Zenith_UI::Zenith_UIRect* pxRect = m_apxRoomRects.Get(u);
			const Vec4 xColour = DP_Minimap::ColourForMemoryState(eState, uBestVis);
			pxRect->SetVisible(xColour.w > 0.0f);
			pxRect->SetColor(Zenith_Maths::Vector4(xColour.x, xColour.y, xColour.z, xColour.w));
		}
	}

	void RefreshVillagerIcons()
	{
		const Zenith_Maths::Vector2 xOrigin = PanelOrigin();
		const Zenith_EntityID xPossessed = DP_Player::GetPossessedVillager();

		uint32_t uIconIdx = 0;
		bool bPlayerPlaced = false;
		DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
			[&](Zenith_EntityID xId, DPVillager_Component&)
			{
				Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xId);
				if (!xEnt.IsValid()) return;
				Zenith_TransformComponent* pxT = xEnt.TryGetComponent<Zenith_TransformComponent>();
				if (pxT == nullptr) return;
				Zenith_Maths::Vector3 xPos;
				pxT->GetPosition(xPos);
				const Vec2 xPanel = DP_Minimap::WorldToPanel(m_xView, xPos.x, xPos.z);

				const bool bIsPossessed = xPossessed.IsValid()
					&& xId.m_uIndex == xPossessed.m_uIndex
					&& xId.m_uGeneration == xPossessed.m_uGeneration;
				if (bIsPossessed)
				{
					m_pxPlayerIcon->SetVisible(true);
					m_pxPlayerIcon->SetPosition(xOrigin.x + xPanel.x, xOrigin.y + xPanel.y);
					bPlayerPlaced = true;
					return;
				}

				// Other villagers only while their position is remembered-
				// visible — matches the world fog (their per-frame reveals
				// keep them visible in practice; the gate future-proofs
				// against fog-design changes).
				if (DP_Fog::GetMemoryStateAt(xPos) != DP_Fog::MemoryTileState::VisitedVisible) return;
				if (uIconIdx >= m_apxVillagerIcons.GetSize()) return;
				Zenith_UI::Zenith_UIRect* pxIcon = m_apxVillagerIcons.Get(uIconIdx++);
				pxIcon->SetVisible(true);
				pxIcon->SetPosition(xOrigin.x + xPanel.x, xOrigin.y + xPanel.y);
			});

		for (uint32_t u = uIconIdx; u < m_apxVillagerIcons.GetSize(); ++u)
		{
			m_apxVillagerIcons.Get(u)->SetVisible(false);
		}
		if (!bPlayerPlaced)
		{
			m_pxPlayerIcon->SetVisible(false);
		}
	}

	Zenith_Entity m_xParentEntity;

	bool m_bBuilt = false;
	DP_Minimap::MapView m_xView;

	// Hot tuning thresholds, cached at build time.
	float m_fVisibleS = 10.0f;
	float m_fDimS     = 30.0f;
	float m_fDimVis   = 0.4f;
	float m_fFadeS    = 5.0f;

	// Canvas-owned (heap-stable) element pointers.
	Zenith_Vector<Zenith_UI::Zenith_UIRect*> m_apxRoomRects;
	Zenith_Vector<Zenith_UI::Zenith_UIRect*> m_apxVillagerIcons;
	Zenith_UI::Zenith_UIRect* m_pxPlayerIcon = nullptr;
};
