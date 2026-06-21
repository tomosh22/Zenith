#pragma once
#include "ZenithECS/Zenith_Entity.h"
#include "DataStream/Zenith_DataStream.h"
#include "Maths/Zenith_Maths.h"

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

// Per-jetpack-entity data for the RenderTest jetpack testbed. A jetpack entity
// carries this alongside a Zenith_ModelComponent (its backpack mesh), a
// Zenith_TransformComponent, a Zenith_AttachmentComponent (binding it to the
// player's back bone), and a Zenith_ParticleEmitterComponent (the jet trail).
//
// The component is passive — it holds the geometry of the exhaust (the two
// nozzle points in the jetpack's own local frame + the exhaust direction) and a
// runtime "thrusting" flag for display. RenderTest_PlayerComponent owns all the
// jetpack logic: it resolves the jetpack via QueryAllScenes<RenderTest_JetpackComponent>,
// applies the upward thrust, and drives the particle emitter from the nozzle
// points read here. This mirrors RenderTest_GunComponent (passive data; the
// player owns the behaviour).
//
// The jetpack is now AUTHORED into RenderTest.zscen (attached to the player's Spine
// via a serialized Zenith_AttachmentComponent). This component is added
// default-constructed, and its default Spec (below) is the canonical nozzle geometry,
// so the stream only round-trips a version tag — there is no per-entity payload to
// persist.
class RenderTest_JetpackComponent
{
public:
	RenderTest_JetpackComponent(Zenith_Entity& xEntity)
		: m_xParentEntity(xEntity)
	{
	}

	// Exhaust geometry, all in the jetpack mesh's own local frame. The two
	// nozzles sit at the bottom of the backpack; the player alternates between
	// them each frame so the trail reads as a twin exhaust.
	struct Spec
	{
		// Canonical nozzle mouths, matching the two drawn nozzles in
		// RT_BuildJetpackMeshAsset. These are the DEFAULT now (the runtime spawn that
		// used to override them is gone): the jetpack is authored with a
		// default-constructed RenderTest_JetpackComponent, so the default Spec must be
		// the real geometry.
		Zenith_Maths::Vector3 m_axNozzleLocal[2] = {
			Zenith_Maths::Vector3(-0.085f, -0.28f, -0.10f),
			Zenith_Maths::Vector3( 0.085f, -0.28f, -0.10f),
		};
		// Exhaust direction in jetpack-local space (down + slightly back). The
		// player transforms this by the jetpack's world rotation each frame.
		Zenith_Maths::Vector3 m_xExhaustLocalDir = Zenith_Maths::Vector3(0.0f, -1.0f, -0.28f);
	};

	void Init(const Spec& xSpec) { m_xSpec = xSpec; }
	const Spec& GetSpec() const { return m_xSpec; }

	Zenith_Entity GetParentEntity() const { return m_xParentEntity; }

	bool IsThrusting() const { return m_bThrusting; }
	void SetThrusting(bool bThrusting) { m_bThrusting = bThrusting; }

	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		const u_int uVersion = 1;
		xStream << uVersion;
	}
	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		u_int uVersion = 0;
		xStream >> uVersion;
		m_bThrusting = false;
	}

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel()
	{
		if (ImGui::CollapsingHeader("Jetpack", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Text("Thrusting: %s", m_bThrusting ? "true" : "false");
		}
	}
#endif

private:
	Zenith_Entity m_xParentEntity;
	Spec          m_xSpec;
	bool          m_bThrusting = false;
};
