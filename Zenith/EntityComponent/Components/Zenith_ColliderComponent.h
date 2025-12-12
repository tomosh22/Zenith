#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Physics/Zenith_Physics.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#include "EntityComponent/Zenith_ComponentRegistry.h"
#endif

namespace JPH
{
	class Body;
	class Shape;
	class BodyID;
}

class Zenith_ColliderComponent {
public:
	Zenith_ColliderComponent() = delete;
	Zenith_ColliderComponent(Zenith_Entity& xEntity);
	~Zenith_ColliderComponent();

	// Serialization methods for Zenith_DataStream
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

	JPH::Body* GetRigidBody() const { return m_pxRigidBody; }
	Zenith_EntityID GetEntityID() { return m_xParentEntity.GetEntityID(); }

	void AddCollider(CollisionVolumeType eVolumeType, RigidBodyType eRigidBodyType);
	void RebuildCollider(); // Rebuild collider with current transform (e.g., after scale change)

#ifdef ZENITH_TOOLS
	//--------------------------------------------------------------------------
	// Editor UI - Renders component properties in the Properties panel
	//--------------------------------------------------------------------------
	void RenderPropertiesPanel()
	{
		if (ImGui::CollapsingHeader("Collider", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Text("Collider attached");
			// TODO: Add collider shape editing UI
		}
	}
#endif
	
private:
	Zenith_Entity m_xParentEntity;
	JPH::Body* m_pxRigidBody = nullptr;
	JPH::BodyID m_xBodyID;

	CollisionVolumeType m_eVolumeType;
	RigidBodyType m_eRigidBodyType;

	struct TerrainMeshData
	{
		float* m_pfVertices = nullptr;
		uint32_t* m_puIndices = nullptr;
		uint32_t m_uNumVertices = 0;
		uint32_t m_uNumIndices = 0;
	};
	TerrainMeshData* m_pxTerrainMeshData = nullptr;

public:
#ifdef ZENITH_TOOLS
	// Static registration function called by ComponentRegistry::Initialise()
	static void RegisterWithEditor()
	{
		Zenith_ComponentRegistry::Get().RegisterComponent<Zenith_ColliderComponent>("Collider");
	}
#endif
};
