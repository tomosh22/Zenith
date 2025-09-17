#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Flux_Material.h"
class Zenith_TerrainComponent
{
public:
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
private:
	Zenith_Entity m_xParentEntity;
	
	//#TO not owning
	Flux_MeshGeometry* m_pxRenderGeometry = nullptr;
	Flux_MeshGeometry* m_pxPhysicsGeometry = nullptr;
	Flux_MeshGeometry* m_pxWaterGeometry = nullptr;
	Flux_Material* m_pxMaterial0 = nullptr;
	Flux_Material* m_pxMaterial1 = nullptr;

	Zenith_Maths::Vector2 m_xPosition_2D = { FLT_MAX,FLT_MAX };
};
