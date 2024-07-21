#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Zenith_Entity.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
class Zenith_ModelComponent
{
public:
	Zenith_ModelComponent(Flux_MeshGeometry& xGeometry, Flux_Texture& xTexture, Zenith_TransformComponent& xTrans, Zenith_Entity& xEntity)
	: m_xGeometry(xGeometry)
	, m_xTexture(xTexture)
	, m_xTransRef(xTrans)
	, m_xParentEntity(xEntity)
	{
		Flux_MemoryManager::InitialiseVertexBuffer(m_xGeometry.GetVertexData(), m_xGeometry.GetVertexDataSize(), m_xVertexBuffer);
		Flux_MemoryManager::InitialiseIndexBuffer(m_xGeometry.GetIndexData(), m_xGeometry.GetIndexDataSize(), m_xIndexBuffer);
	};

	~Zenith_ModelComponent() {}

	const Flux_VertexBuffer& GetVertexBuffer() const { return m_xVertexBuffer; }
	const Flux_IndexBuffer& GetIndexBuffer() const { return m_xIndexBuffer; }
	const Flux_MeshGeometry& GetMeshGeometry() const { return m_xGeometry; }
	const Flux_Texture& GetTexture() const { return m_xTexture; }
	Flux_Texture& GetTexture() { return m_xTexture; }

		
private:
	Zenith_Entity m_xParentEntity;
	Zenith_TransformComponent& m_xTransRef;


	Flux_MeshGeometry& m_xGeometry;
	Flux_Texture& m_xTexture;

	Flux_VertexBuffer m_xVertexBuffer;
	Flux_IndexBuffer m_xIndexBuffer;
};
