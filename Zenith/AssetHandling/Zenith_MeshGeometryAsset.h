#pragma once

#include "AssetHandling/Zenith_Asset.h"
#include <string>

// Forward declarations
class Flux_MeshGeometry;

/**
 * Zenith_MeshGeometryAsset - Mesh geometry asset
 *
 * Wrapper around Flux_MeshGeometry that provides registry integration,
 * reference counting, and caching. This is the primary way to load
 * and create mesh geometry in the engine.
 *
 * Usage:
 *   // Load from file
 *   Zenith_MeshGeometryAsset* pMesh = Zenith_AssetRegistry::Get().Get<Zenith_MeshGeometryAsset>("game:Meshes/level.zmesh");
 *   Flux_MeshGeometry* pGeom = pMesh->GetGeometry();
 *
 *   // Create primitive (cached by type)
 *   Zenith_MeshGeometryAsset* pCube = Zenith_MeshGeometryAsset::CreateUnitCube();
 *   Zenith_MeshGeometryAsset* pSphere = Zenith_MeshGeometryAsset::CreateUnitSphere(16);
 */
class Zenith_MeshGeometryAsset : public Zenith_Asset
{
public:
	Zenith_MeshGeometryAsset();
	~Zenith_MeshGeometryAsset();

	// Non-copyable
	Zenith_MeshGeometryAsset(const Zenith_MeshGeometryAsset&) = delete;
	Zenith_MeshGeometryAsset& operator=(const Zenith_MeshGeometryAsset&) = delete;

	//--------------------------------------------------------------------------
	// Accessors
	//--------------------------------------------------------------------------

	/**
	 * Get the underlying mesh geometry
	 * @return Pointer to the geometry, or nullptr if not loaded
	 */
	Flux_MeshGeometry* GetGeometry() { return m_pxGeometry; }
	const Flux_MeshGeometry* GetGeometry() const { return m_pxGeometry; }

	/**
	 * Check if the geometry is valid/loaded
	 */
	bool IsValid() const { return m_pxGeometry != nullptr; }

	//--------------------------------------------------------------------------
	// Procedural Geometry Support
	//--------------------------------------------------------------------------

	/**
	 * Set the geometry for procedural meshes
	 * Takes ownership of the geometry (will delete it on destruction)
	 * @param pxGeometry Pointer to geometry to take ownership of
	 */
	void SetGeometry(Flux_MeshGeometry* pxGeometry);

	/**
	 * Release ownership of the geometry without deleting it
	 * @return Pointer to the geometry (caller takes ownership)
	 */
	Flux_MeshGeometry* ReleaseGeometry();

	//--------------------------------------------------------------------------
	// Static Primitive Creators (return registry-managed assets)
	//--------------------------------------------------------------------------

	/**
	 * Create a fullscreen quad geometry (cached)
	 * @return Registry-managed asset containing fullscreen quad
	 */
	static Zenith_MeshGeometryAsset* CreateFullscreenQuad();

	/**
	 * Create a unit cube geometry (cached)
	 * @return Registry-managed asset containing unit cube
	 */
	static Zenith_MeshGeometryAsset* CreateUnitCube();

	/**
	 * Create a unit sphere geometry (cached per segment count)
	 * @param uSegments Number of latitude/longitude segments (default 16)
	 * @return Registry-managed asset containing unit sphere
	 */
	static Zenith_MeshGeometryAsset* CreateUnitSphere(uint32_t uSegments = 16);

	/**
	 * Create a unit capsule geometry (cached per segment count)
	 * @param uSegments Number of segments (default 16)
	 * @return Registry-managed asset containing unit capsule
	 */
	static Zenith_MeshGeometryAsset* CreateUnitCapsule(uint32_t uSegments = 16);

	/**
	 * Create a unit cylinder geometry (cached per segment count)
	 * @param uSegments Number of segments (default 16)
	 * @return Registry-managed asset containing unit cylinder
	 */
	static Zenith_MeshGeometryAsset* CreateUnitCylinder(uint32_t uSegments = 16);

	/**
	 * Create a unit cone geometry (cached per segment count)
	 * @param uSegments Number of segments (default 16)
	 * @return Registry-managed asset containing unit cone
	 */
	static Zenith_MeshGeometryAsset* CreateUnitCone(uint32_t uSegments = 16);

private:
	friend class Zenith_AssetRegistry;
	friend Zenith_Asset* LoadMeshGeometryAsset(const std::string&);

	/**
	 * Load geometry from file (private - use Zenith_AssetRegistry::Get)
	 * @param strPath Path to mesh file (.zmesh)
	 * @param uRetainAttributeBits Bitmask of attributes to retain in CPU memory
	 * @param bUploadToGPU Whether to upload to GPU
	 * @return true on success
	 */
	bool LoadFromFile(const std::string& strPath, uint32_t uRetainAttributeBits = 0, bool bUploadToGPU = true);

	Flux_MeshGeometry* m_pxGeometry = nullptr;
	bool m_bOwnsGeometry = true;
};
