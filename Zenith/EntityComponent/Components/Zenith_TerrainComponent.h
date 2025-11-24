#pragma once

#define TERRAIN_EXPORT_DIMS 64

//#TO_TODO: these need to be in a header file for tools terrain export

#define MAX_TERRAIN_HEIGHT 2048
//#TO width/height that heightmap is divided into
#define TERRAIN_SIZE 64
//#TO multiplier for vertex positions
#define TERRAIN_SCALE 8

#include "EntityComponent/Zenith_Entity.h"
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
		, m_pxMaterial0(nullptr)
		, m_pxMaterial1(nullptr)
	{
	};

	// Full constructor for runtime creation
	Zenith_TerrainComponent(Flux_Material& xMaterial0, Flux_Material& xMaterial1, Zenith_Entity& xEntity);

	~Zenith_TerrainComponent();

	const Flux_MeshGeometry& GetRenderMeshGeometry() const { return *m_pxRenderGeometry; }
	const Flux_MeshGeometry& GetPhysicsMeshGeometry() const { return *m_pxPhysicsGeometry; }
	const Flux_Material& GetMaterial0() const { return *m_pxMaterial0; }
	Flux_Material& GetMaterial0() { return *m_pxMaterial0; }
	const Flux_Material& GetMaterial1() const { return *m_pxMaterial1; }
	Flux_Material& GetMaterial1() { return *m_pxMaterial1; }

	Zenith_Entity GetParentEntity() const { return m_xParentEntity; }

	const bool IsVisible(const float fVisibilityMultiplier, const Zenith_CameraComponent& xCam) const;

	// Serialization methods for Zenith_DataStream
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

private:
	Zenith_Entity m_xParentEntity;
	
	//#TO not owning
	Flux_MeshGeometry* m_pxRenderGeometry = nullptr;
	Flux_MeshGeometry* m_pxPhysicsGeometry = nullptr;
	Flux_Material* m_pxMaterial0 = nullptr;
	Flux_Material* m_pxMaterial1 = nullptr;
};
