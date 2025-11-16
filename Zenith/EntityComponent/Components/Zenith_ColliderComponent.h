#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Physics/Zenith_Physics.h"

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
	
	void Serialize(std::ofstream& xOut);
	JPH::Body* GetRigidBody() const { return m_pxRigidBody; }
	Zenith_EntityID GetEntityID() { return m_xParentEntity.GetEntityID(); }

	void AddCollider(CollisionVolumeType eVolumeType, RigidBodyType eRigidBodyType);
	
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
