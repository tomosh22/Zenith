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
			// Collider configuration section (always shown)
			const char* szVolumeTypes[] = { "AABB", "OBB", "Sphere", "Capsule", "Terrain", "Model Mesh" };
			const char* szRigidBodyTypes[] = { "Dynamic", "Static" };

			// If no collider exists, show add collider UI
			if (!m_pxRigidBody)
			{
				ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No collider attached");
				ImGui::Separator();

				static int s_iSelectedVolumeType = COLLISION_VOLUME_TYPE_SPHERE;
				static int s_iSelectedRigidBodyType = RIGIDBODY_TYPE_DYNAMIC;

				ImGui::Combo("Volume Type", &s_iSelectedVolumeType, szVolumeTypes, 6);
				ImGui::Combo("Body Type", &s_iSelectedRigidBodyType, szRigidBodyTypes, 2);

				if (ImGui::Button("Add Collider"))
				{
					AddCollider(static_cast<CollisionVolumeType>(s_iSelectedVolumeType), 
					            static_cast<RigidBodyType>(s_iSelectedRigidBodyType));
					Zenith_Log("[ColliderComponent] Added %s collider (%s)", 
					           szVolumeTypes[s_iSelectedVolumeType], 
					           szRigidBodyTypes[s_iSelectedRigidBodyType]);
				}
			}
			else
			{
				// Display current collider info
				ImGui::Text("Body ID: %u", m_xBodyID.GetIndexAndSequenceNumber());

				// Volume type (display current and allow change)
				int iCurrentVolumeType = static_cast<int>(m_eVolumeType);
				if (iCurrentVolumeType < 6)
				{
					ImGui::Text("Volume Type: %s", szVolumeTypes[iCurrentVolumeType]);
				}

				// Rigid body type (display current)
				int iCurrentRigidBodyType = static_cast<int>(m_eRigidBodyType);
				if (iCurrentRigidBodyType < 2)
				{
					ImGui::Text("Body Type: %s", szRigidBodyTypes[iCurrentRigidBodyType]);
				}

				// Gravity toggle for dynamic bodies
				if (m_eRigidBodyType == RIGIDBODY_TYPE_DYNAMIC)
				{
					ImGui::Separator();
					// Check current gravity state
					static bool s_bGravityEnabled = true;
					if (ImGui::Checkbox("Gravity Enabled", &s_bGravityEnabled))
					{
						Zenith_Physics::SetGravityEnabled(m_pxRigidBody, s_bGravityEnabled);
						Zenith_Log("[ColliderComponent] Gravity %s", s_bGravityEnabled ? "enabled" : "disabled");
					}
				}

				// Display terrain mesh data info if present
				if (m_pxTerrainMeshData)
				{
					ImGui::Separator();
					ImGui::Text("Terrain Mesh Collider:");
					ImGui::Text("  Vertices: %u", m_pxTerrainMeshData->m_uNumVertices);
					ImGui::Text("  Indices: %u", m_pxTerrainMeshData->m_uNumIndices);
					ImGui::Text("  Triangles: %u", m_pxTerrainMeshData->m_uNumIndices / 3);
				}

				ImGui::Separator();

				// Reconfigure collider section
				if (ImGui::TreeNode("Reconfigure Collider"))
				{
					static int s_iNewVolumeType = COLLISION_VOLUME_TYPE_SPHERE;
					static int s_iNewRigidBodyType = RIGIDBODY_TYPE_DYNAMIC;

					ImGui::Combo("New Volume Type", &s_iNewVolumeType, szVolumeTypes, 6);
					ImGui::Combo("New Body Type", &s_iNewRigidBodyType, szRigidBodyTypes, 2);

					if (ImGui::Button("Rebuild Collider"))
					{
						// Remove existing collider
						if (m_xBodyID.IsInvalid() == false)
						{
							JPH::BodyInterface& xBodyInterface = Zenith_Physics::s_pxPhysicsSystem->GetBodyInterface();
							xBodyInterface.RemoveBody(m_xBodyID);
							xBodyInterface.DestroyBody(m_xBodyID);
							m_xBodyID = JPH::BodyID();
							m_pxRigidBody = nullptr;
						}
						if (m_pxTerrainMeshData != nullptr)
						{
							delete[] m_pxTerrainMeshData->m_pfVertices;
							delete[] m_pxTerrainMeshData->m_puIndices;
							delete m_pxTerrainMeshData;
							m_pxTerrainMeshData = nullptr;
						}

						// Create new collider
						AddCollider(static_cast<CollisionVolumeType>(s_iNewVolumeType), 
						            static_cast<RigidBodyType>(s_iNewRigidBodyType));
						Zenith_Log("[ColliderComponent] Rebuilt collider: %s (%s)", 
						           szVolumeTypes[s_iNewVolumeType], 
						           szRigidBodyTypes[s_iNewRigidBodyType]);
					}

					ImGui::TreePop();
				}

				// Remove collider button
				if (ImGui::Button("Remove Collider"))
				{
					if (m_xBodyID.IsInvalid() == false)
					{
						JPH::BodyInterface& xBodyInterface = Zenith_Physics::s_pxPhysicsSystem->GetBodyInterface();
						xBodyInterface.RemoveBody(m_xBodyID);
						xBodyInterface.DestroyBody(m_xBodyID);
						m_xBodyID = JPH::BodyID();
						m_pxRigidBody = nullptr;
					}
					if (m_pxTerrainMeshData != nullptr)
					{
						delete[] m_pxTerrainMeshData->m_pfVertices;
						delete[] m_pxTerrainMeshData->m_puIndices;
						delete m_pxTerrainMeshData;
						m_pxTerrainMeshData = nullptr;
					}
					Zenith_Log("[ColliderComponent] Removed collider");
				}
			}
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

};
