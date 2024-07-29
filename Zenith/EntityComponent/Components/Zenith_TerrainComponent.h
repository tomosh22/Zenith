#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Flux_Material.h"
class Zenith_TerrainComponent
{
public:
	Zenith_TerrainComponent(Flux_MeshGeometry& xGeometry, Flux_Material& xMaterial0, Flux_Material& xMaterial1, Zenith_Entity& xEntity)
	: m_xGeometry(xGeometry)
	, m_xMaterial0(xMaterial0)
	, m_xMaterial1(xMaterial1)
	, m_xParentEntity(xEntity)
	{
	};

	~Zenith_TerrainComponent() {}

	const Flux_MeshGeometry& GetMeshGeometry() const { return m_xGeometry; }
	const Flux_Material& GetMaterial0() const { return m_xMaterial0; }
	Flux_Material& GetMaterial0() { return m_xMaterial0; }
	const Flux_Material& GetMaterial1() const { return m_xMaterial1; }
	Flux_Material& GetMaterial1() { return m_xMaterial1; }

	Zenith_Entity GetParentEntity() const { return m_xParentEntity; }
private:
	Zenith_Entity m_xParentEntity;

	Flux_MeshGeometry& m_xGeometry;
	Flux_Material& m_xMaterial0;
	Flux_Material& m_xMaterial1;
};
