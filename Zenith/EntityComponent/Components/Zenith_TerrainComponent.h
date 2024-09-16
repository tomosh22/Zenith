#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Flux_Material.h"
class Zenith_TerrainComponent
{
public:
	Zenith_TerrainComponent(Flux_MeshGeometry& xGeometry, Flux_Material& xMaterial0, Flux_Material& xMaterial1, Zenith_Maths::Matrix4 xWaterTransform, Zenith_Maths::Vector2 xPosition_2D, Zenith_Entity& xEntity)
		: m_xGeometry(xGeometry)
		, m_xMaterial0(xMaterial0)
		, m_xMaterial1(xMaterial1)
		, m_xPosition_2D(xPosition_2D)
		, m_xParentEntity(xEntity)
	{
		Flux_MeshGeometry::GenerateFullscreenQuad(m_xWaterGeometry, xWaterTransform);
		Flux_MemoryManager::InitialiseVertexBuffer(m_xWaterGeometry.GetVertexData(), m_xWaterGeometry.GetVertexDataSize(), m_xWaterGeometry.GetVertexBuffer());
		Flux_MemoryManager::InitialiseIndexBuffer(m_xWaterGeometry.GetIndexData(), m_xWaterGeometry.GetIndexDataSize(), m_xWaterGeometry.GetIndexBuffer());
	};

	~Zenith_TerrainComponent() {}

	const Flux_MeshGeometry& GetMeshGeometry() const { return m_xGeometry; }
	const Flux_Material& GetMaterial0() const { return m_xMaterial0; }
	Flux_Material& GetMaterial0() { return m_xMaterial0; }
	const Flux_Material& GetMaterial1() const { return m_xMaterial1; }
	Flux_Material& GetMaterial1() { return m_xMaterial1; }

	const Flux_MeshGeometry& GetWaterGeometry() const { return m_xWaterGeometry; }
	Flux_MeshGeometry& GetWaterGeometry() { return m_xWaterGeometry; }

	Zenith_Entity GetParentEntity() const { return m_xParentEntity; }

	const Zenith_Maths::Vector2 GetPosition_2D() const { return m_xPosition_2D; }

	const bool IsVisible() const;
private:
	Zenith_Entity m_xParentEntity;

	Flux_MeshGeometry& m_xGeometry;
	Flux_Material& m_xMaterial0;
	Flux_Material& m_xMaterial1;

	//#TO owned by this, not a reference to a mesh from the asset handler
	Flux_MeshGeometry m_xWaterGeometry;

	Zenith_Maths::Vector2 m_xPosition_2D = { FLT_MAX,FLT_MAX };
};
