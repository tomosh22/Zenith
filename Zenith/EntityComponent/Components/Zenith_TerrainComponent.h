#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Flux_Material.h"
#include "Maths/Zenith_FrustumCulling.h"
class Zenith_TerrainComponent
{
public:
	// Default constructor for deserialization
	// ReadFromDataStream will populate all members from saved data
	Zenith_TerrainComponent(Zenith_Entity& xEntity)
		: m_xParentEntity(xEntity)
		, m_pxRenderGeometry(nullptr)
		, m_pxPhysicsGeometry(nullptr)
		, m_pxWaterGeometry(nullptr)
		, m_pxMaterial0(nullptr)
		, m_pxMaterial1(nullptr)
		, m_xPosition_2D(FLT_MAX, FLT_MAX)
	{
	};

	// Full constructor for runtime creation
	Zenith_TerrainComponent(Flux_MeshGeometry& xRenderGeometry, Flux_MeshGeometry& xPhysicsGeometry, Flux_MeshGeometry& xWaterGeometry, Flux_Material& xMaterial0, Flux_Material& xMaterial1, Zenith_Maths::Vector2 xPosition_2D, Zenith_Entity& xEntity)
		: m_pxRenderGeometry(&xRenderGeometry)
		, m_pxPhysicsGeometry(&xPhysicsGeometry)
		, m_pxWaterGeometry(&xWaterGeometry)
		, m_pxMaterial0(&xMaterial0)
		, m_pxMaterial1(&xMaterial1)
		, m_xPosition_2D(xPosition_2D)
		, m_xParentEntity(xEntity)
	{
	};

	~Zenith_TerrainComponent() {}

	const Flux_MeshGeometry& GetRenderMeshGeometry() const { return *m_pxRenderGeometry; }
	const Flux_MeshGeometry& GetPhysicsMeshGeometry() const { return *m_pxPhysicsGeometry; }
	const Flux_Material& GetMaterial0() const { return *m_pxMaterial0; }
	Flux_Material& GetMaterial0() { return *m_pxMaterial0; }
	const Flux_Material& GetMaterial1() const { return *m_pxMaterial1; }
	Flux_Material& GetMaterial1() { return *m_pxMaterial1; }

	const Flux_MeshGeometry& GetWaterGeometry() const { return *m_pxWaterGeometry; }
	Flux_MeshGeometry& GetWaterGeometry() { return *m_pxWaterGeometry; }

	Zenith_Entity GetParentEntity() const { return m_xParentEntity; }

	const Zenith_Maths::Vector2 GetPosition_2D() const { return m_xPosition_2D; }

	const bool IsVisible(const float fVisibilityMultiplier, const Zenith_CameraComponent& xCam) const;

	// AABB for frustum culling (cached)
	void SetAABB(const Zenith_AABB& xAABB) { m_xAABB = xAABB; m_bAABBValid = true; }
	const Zenith_AABB& GetAABB() const { return m_xAABB; }
	bool HasValidAABB() const { return m_bAABBValid; }

	// Serialization methods for Zenith_DataStream
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

private:
	Zenith_Entity m_xParentEntity;
	
	//#TO not owning
	Flux_MeshGeometry* m_pxRenderGeometry = nullptr;
	Flux_MeshGeometry* m_pxPhysicsGeometry = nullptr;
	Flux_MeshGeometry* m_pxWaterGeometry = nullptr;
	Flux_Material* m_pxMaterial0 = nullptr;
	Flux_Material* m_pxMaterial1 = nullptr;

	Zenith_Maths::Vector2 m_xPosition_2D = { FLT_MAX,FLT_MAX };

	// Cached AABB for frustum culling
	Zenith_AABB m_xAABB;
	bool m_bAABBValid = false;
};
