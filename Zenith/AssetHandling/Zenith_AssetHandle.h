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
	T* Get();

	/**
	 * Dereference operator
	 */
	T* operator->()
	{
		return Get();
	}

	/**
	 * Const dereference
	 */
	const T* operator->() const
	{
		return const_cast<Zenith_AssetHandle*>(this)->Get();
	}

	/**
	 * Bool conversion - true if path is set (doesn't check if asset exists)
	 */
	explicit operator bool() const
	{
		return !m_strPath.empty();
	}

	/**
	 * Check if path is set (alias for operator bool)
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
	 * Comparison operators
	 */
	bool operator==(const Zenith_AssetHandle& xOther) const
	{
		return m_strPath == xOther.m_strPath;
	}

	bool operator!=(const Zenith_AssetHandle& xOther) const
	{
		return m_strPath != xOther.m_strPath;
	}

	/**
	 * Serialization
	 */
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

private:
	std::string m_strPath;
	T* m_pxCached = nullptr;
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
