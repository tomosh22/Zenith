#pragma once

#include <string>
#include "AssetHandling/Zenith_Asset.h"

// Forward declarations
class Zenith_AssetRegistry;
class Zenith_TextureAsset;
class Zenith_MaterialAsset;
class Zenith_MeshAsset;
class Zenith_SkeletonAsset;
class Zenith_ModelAsset;
class Zenith_AnimationAsset;
class Zenith_MeshGeometryAsset;
class Zenith_Prefab;
class Zenith_DataStream;

/**
 * Zenith_AssetHandle<T> - Smart handle for referencing assets
 *
 * This is THE primary way to reference assets in components and other assets.
 * Instead of storing raw pointers, store an AssetHandle which:
 * - Manages reference counting automatically (AddRef on copy, Release on destroy)
 * - Loads assets on demand via the registry
 * - Serializes by path for scene save/load
 * - Uses prefixed paths for cross-machine portability
 *
 * Path Prefixes:
 *   game:   - Game assets (e.g., "game:Textures/diffuse.ztex")
 *   engine: - Engine assets (e.g., "engine:Materials/default.zmat")
 *
 * Usage:
 *   // In component header
 *   TextureHandle m_xDiffuseTexture;
 *   MeshHandle m_xMesh;
 *
 *   // Set from prefixed path
 *   m_xDiffuseTexture = TextureHandle("game:Textures/diffuse.ztex");
 *
 *   // Get the asset (loads if needed)
 *   Zenith_TextureAsset* pTexture = m_xDiffuseTexture.Get();
 *
 *   // Check if valid
 *   if (m_xMesh) { ... }
 */
template<typename T>
class Zenith_AssetHandle
{
public:
	Zenith_AssetHandle() = default;

	/**
	 * Construct from path - does NOT load immediately
	 */
	explicit Zenith_AssetHandle(const std::string& strPath)
		: m_strPath(strPath)
		, m_pxCached(nullptr)
	{
	}

	/**
	 * Copy constructor - increments ref count
	 */
	Zenith_AssetHandle(const Zenith_AssetHandle& xOther)
		: m_strPath(xOther.m_strPath)
		, m_pxCached(xOther.m_pxCached)
	{
		if (m_pxCached)
		{
			reinterpret_cast<Zenith_Asset*>(m_pxCached)->AddRef();
		}
	}

	/**
	 * Copy assignment - decrements old ref, increments new
	 */
	Zenith_AssetHandle& operator=(const Zenith_AssetHandle& xOther)
	{
		if (this != &xOther)
		{
			// Release old
			if (m_pxCached)
			{
				reinterpret_cast<Zenith_Asset*>(m_pxCached)->Release();
			}

			// Copy and acquire new
			m_strPath = xOther.m_strPath;
			m_pxCached = xOther.m_pxCached;

			if (m_pxCached)
			{
				reinterpret_cast<Zenith_Asset*>(m_pxCached)->AddRef();
			}
		}
		return *this;
	}

	/**
	 * Move constructor
	 */
	Zenith_AssetHandle(Zenith_AssetHandle&& xOther) noexcept
		: m_strPath(std::move(xOther.m_strPath))
		, m_pxCached(xOther.m_pxCached)
	{
		xOther.m_pxCached = nullptr;
	}

	/**
	 * Move assignment
	 */
	Zenith_AssetHandle& operator=(Zenith_AssetHandle&& xOther) noexcept
	{
		if (this != &xOther)
		{
			// Release old
			if (m_pxCached)
			{
				reinterpret_cast<Zenith_Asset*>(m_pxCached)->Release();
			}

			// Move and steal
			m_strPath = std::move(xOther.m_strPath);
			m_pxCached = xOther.m_pxCached;
			xOther.m_pxCached = nullptr;
		}
		return *this;
	}

	/**
	 * Destructor - decrements ref count
	 */
	~Zenith_AssetHandle()
	{
		if (m_pxCached)
		{
			reinterpret_cast<Zenith_Asset*>(m_pxCached)->Release();
			m_pxCached = nullptr;
		}
	}

	/**
	 * Get the asset, loading if necessary
	 * @return Pointer to the asset, or nullptr if path is empty or load fails
	 */
	T* Get() const;

	/**
	 * Dereference operator
	 */
	T* operator->() const
	{
		return Get();
	}

	/**
	 * Bool conversion - true if handle references a valid asset
	 * Returns true if either:
	 * - A path is set (file-based asset)
	 * - A cached pointer exists (procedural asset via Set())
	 */
	explicit operator bool() const
	{
		return !m_strPath.empty() || m_pxCached != nullptr;
	}

	/**
	 * Check if path is set (for serialization purposes)
	 * Note: For procedural assets created via Set(), use operator bool() or IsLoaded() instead
	 */
	bool IsSet() const
	{
		return !m_strPath.empty();
	}

	/**
	 * Check if the asset is currently loaded
	 */
	bool IsLoaded() const
	{
		return m_pxCached != nullptr;
	}

	/**
	 * Get the path
	 */
	const std::string& GetPath() const
	{
		return m_strPath;
	}

	/**
	 * Set the path (releases current asset if any)
	 */
	void SetPath(const std::string& strPath)
	{
		if (m_strPath != strPath)
		{
			if (m_pxCached)
			{
				reinterpret_cast<Zenith_Asset*>(m_pxCached)->Release();
				m_pxCached = nullptr;
			}
			m_strPath = strPath;
		}
	}

	/**
	 * Clear the handle
	 */
	void Clear()
	{
		if (m_pxCached)
		{
			reinterpret_cast<Zenith_Asset*>(m_pxCached)->Release();
			m_pxCached = nullptr;
		}
		m_strPath.clear();
	}

	/**
	 * Set the handle from an asset pointer directly
	 * Used for procedural assets created via registry.Create<T>()
	 * Note: For procedural assets, the path is not stored - use SetPath() for serializable references
	 * @param pxAsset Pointer to asset (must be from registry)
	 */
	void Set(T* pxAsset)
	{
		// Release old
		if (m_pxCached)
		{
			reinterpret_cast<Zenith_Asset*>(m_pxCached)->Release();
		}

		if (pxAsset)
		{
			// For procedural assets, we hold the pointer but path is not stored
			// This is intentional - procedural assets aren't serializable via path
			m_strPath.clear();
			m_pxCached = pxAsset;
			reinterpret_cast<Zenith_Asset*>(m_pxCached)->AddRef();
		}
		else
		{
			m_strPath.clear();
			m_pxCached = nullptr;
		}
	}

	/**
	 * Comparison operators
	 * For path-based assets, compares paths. For procedural assets (empty paths), compares pointers.
	 */
	bool operator==(const Zenith_AssetHandle& xOther) const
	{
		// If either has a path, compare by path
		if (!m_strPath.empty() || !xOther.m_strPath.empty())
		{
			return m_strPath == xOther.m_strPath;
		}
		// Both are procedural (no path), compare by cached pointer
		return m_pxCached == xOther.m_pxCached;
	}

	bool operator!=(const Zenith_AssetHandle& xOther) const
	{
		return !(*this == xOther);
	}

	/**
	 * Serialization
	 */
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

private:
	std::string m_strPath;
	mutable T* m_pxCached = nullptr;  // mutable: lazy loading doesn't change logical state
};

//--------------------------------------------------------------------------
// Type aliases for common asset handles
//--------------------------------------------------------------------------

using TextureHandle = Zenith_AssetHandle<Zenith_TextureAsset>;
using MaterialHandle = Zenith_AssetHandle<Zenith_MaterialAsset>;
using MeshHandle = Zenith_AssetHandle<Zenith_MeshAsset>;
using SkeletonHandle = Zenith_AssetHandle<Zenith_SkeletonAsset>;
using ModelHandle = Zenith_AssetHandle<Zenith_ModelAsset>;
using AnimationHandle = Zenith_AssetHandle<Zenith_AnimationAsset>;
using MeshGeometryHandle = Zenith_AssetHandle<Zenith_MeshGeometryAsset>;
using PrefabHandle = Zenith_AssetHandle<Zenith_Prefab>;

//--------------------------------------------------------------------------
// Template implementations that need registry access are in the .cpp
//--------------------------------------------------------------------------

// Get() implementations are specialized per type in Zenith_AssetHandle.cpp
