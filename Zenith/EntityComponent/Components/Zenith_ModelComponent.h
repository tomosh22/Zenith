#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Zenith_Entity.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
class Zenith_ModelComponent
{
public:
	Zenith_ModelComponent(Flux_MeshGeometry& xGeometry, Flux_Texture& xTexture, Zenith_Entity& xEntity)
	: m_xGeometry(xGeometry)
	, m_xTexture(xTexture)
	, m_xParentEntity(xEntity)
	{
	};

	~Zenith_ModelComponent() {}

	const Flux_MeshGeometry& GetMeshGeometry() const { return m_xGeometry; }
	const Flux_Texture& GetTexture() const { return m_xTexture; }
	Flux_Texture& GetTexture() { return m_xTexture; }

		
private:
	Zenith_Entity m_xParentEntity;

	Flux_MeshGeometry& m_xGeometry;
	Flux_Texture& m_xTexture;
};
