#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Flux_Material.h"
class Zenith_ModelComponent
{
public:
	Zenith_ModelComponent(Flux_MeshGeometry& xGeometry, Flux_Material& xMaterial, Zenith_Entity& xEntity)
	: m_xGeometry(xGeometry)
	, m_xMaterial(xMaterial)
	, m_xParentEntity(xEntity)
	{
	};

	~Zenith_ModelComponent() {}

	const Flux_MeshGeometry& GetMeshGeometry() const { return m_xGeometry; }
	const Flux_Material& GetMaterial() const { return m_xMaterial; }
	Flux_Material& GetMaterial() { return m_xMaterial; }

	Zenith_Entity GetParentEntity() const { return m_xParentEntity; }
private:
	Zenith_Entity m_xParentEntity;

	Flux_MeshGeometry& m_xGeometry;
	Flux_Material& m_xMaterial;
};
