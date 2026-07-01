#pragma once
#include "ZenithECS/Zenith_Entity.h"
#include "DataStream/Zenith_DataStream.h"
#include "RenderTest/RenderTest_Guns.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

// Per-gun-entity data for the FPS pickup/drop testbed. A gun entity carries this
// alongside a Zenith_ModelComponent (its mesh), a Zenith_TransformComponent, and a
// Zenith_AttachmentComponent (idle until picked up). The component is passive — it
// holds the gun's spec (mount/foregrip/muzzle/ammo) plus runtime ammo + held flag.
// The player component owns the pickup/drop/IK/fire logic and reads this for the
// held gun. See RenderTest_Guns.h for the design overview.
//
// The guns are now AUTHORED into RenderTest.zscen (one entity per type), so this
// component IS serialized: the v2 stream persists the gun type + ammo (set at author
// time via RenderTest_ApplyTestbedEntityConfig) and ReadFromDataStream rebuilds the
// full spec from the type, so an authored gun reloads as the right weapon.
class RenderTest_GunComponent
{
public:
	RenderTest_GunComponent(Zenith_Entity& xEntity)
		: m_xParentEntity(xEntity)
	{
	}

	// Seed the gun from its type spec. Called by the spawn right after the
	// component is added.
	void Init(const RenderTest_Guns::GunSpec& xSpec)
	{
		m_xSpec = xSpec;
		m_uAmmoInClip = xSpec.m_uMagSize;
		m_uReserve = xSpec.m_uReserve;
		m_bHeld = false;
	}

	const RenderTest_Guns::GunSpec& GetSpec() const { return m_xSpec; }
	RenderTest_Guns::GunType GetType() const { return m_xSpec.m_eType; }
	const char* GetName() const { return m_xSpec.m_szName; }
	bool IsTwoHanded() const { return m_xSpec.m_bTwoHanded; }

	bool IsHeld() const { return m_bHeld; }
	void SetHeld(bool bHeld) { m_bHeld = bHeld; }

	// Ammo lives on the gun so picking the same gun back up resumes its state.
	uint32_t GetAmmoInClip() const { return m_uAmmoInClip; }
	uint32_t GetReserve() const { return m_uReserve; }
	void SetAmmo(uint32_t uClip, uint32_t uReserve) { m_uAmmoInClip = uClip; m_uReserve = uReserve; }

	Zenith_Entity GetParentEntity() const { return m_xParentEntity; }

	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		// v2: persist the gun type + ammo so an authored gun reloads as the right
		// weapon (the runtime spawn that used to seed these is gone). v1 (version tag
		// only) loads back as a default pistol.
		const u_int uVersion = 2;
		xStream << uVersion;
		const uint32_t uType = static_cast<uint32_t>(m_xSpec.m_eType);
		xStream << uType;
		xStream << m_uAmmoInClip;
		xStream << m_uReserve;
	}
	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		u_int uVersion = 0;
		xStream >> uVersion;
		m_bHeld = false;
		if (uVersion < 2)
		{
			return;   // v1 carried no payload — keep the default-constructed spec.
		}
		uint32_t uType = 0;
		xStream >> uType;
		if (uType >= static_cast<uint32_t>(RenderTest_Guns::GunType::COUNT))
		{
			uType = 0;   // corrupt / forward-version type -> default
		}
		// Rebuild the full spec from the persisted type, then restore the saved ammo.
		Init(RenderTest_Guns::GetSpec(static_cast<RenderTest_Guns::GunType>(uType)));
		uint32_t uClip = 0;
		uint32_t uReserve = 0;
		xStream >> uClip;
		xStream >> uReserve;
		SetAmmo(uClip, uReserve);
	}

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel()
	{
		if (ImGui::CollapsingHeader("Gun", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Text("Type: %s", m_xSpec.m_szName);
			ImGui::Text("Two-handed: %s", m_xSpec.m_bTwoHanded ? "true" : "false");
			ImGui::Text("Held: %s", m_bHeld ? "true" : "false");
			ImGui::Text("Ammo: %u / %u", m_uAmmoInClip, m_uReserve);
		}
	}
#endif

private:
	Zenith_Entity m_xParentEntity;
	RenderTest_Guns::GunSpec m_xSpec;
	uint32_t m_uAmmoInClip = 0;
	uint32_t m_uReserve = 0;
	bool m_bHeld = false;
};
