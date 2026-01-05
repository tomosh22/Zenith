#pragma once
#include "Core/Zenith_GUID.h"
#include "AssetHandling/Zenith_AssetDatabase.h"
#include <string>

class Zenith_DataStream;

// Forward declare load state (defined in Zenith_AsyncAssetLoader.h)
enum class AssetLoadState : uint8_t;

// Callback function types for async loading
using AssetLoadCompleteFn = void(*)(void* pxAsset, void* pxUserData);
using AssetLoadFailFn = void(*)(const char* szError, void* pxUserData);

/**
 * Zenith_AssetRef<T> - Type-safe reference to an asset by GUID
 *
 * This is the primary way to reference assets in components and other assets.
 * Instead of storing raw pointers or file paths, store an AssetRef which:
 * - Survives asset moves/renames (references by GUID, not path)
 * - Provides lazy loading (asset loaded on first access)
 * - Serializes automatically via DataStream
 * - Can be displayed in editor with drag-drop support
 *
 * Usage:
 *   // In component header
 *   TextureRef m_xDiffuseTexture;
 *   MaterialRef m_xMaterial;
 *
 *   // Set from path (looks up GUID from database)
 *   m_xDiffuseTexture.SetFromPath("Assets/Textures/diffuse.ztex");
 *
 *   // Set from GUID directly
 *   m_xMaterial.SetGUID(materialGUID);
 *
 *   // Get the asset (loads if needed)
 *   Flux_Texture* pTexture = m_xDiffuseTexture.Get();
 *
 *   // Serialization is automatic
 *   xStream << m_xDiffuseTexture;  // Writes GUID
 *   xStream >> m_xDiffuseTexture;  // Reads GUID
 */
template<typename T>
class Zenith_AssetRef
{
public:
	Zenith_AssetRef() = default;

	explicit Zenith_AssetRef(const Zenith_AssetGUID& xGUID)
		: m_xGUID(xGUID)
		, m_pxCached(nullptr)
	{
	}

	/**
	 * Get the referenced asset, loading if necessary
	 * @return Pointer to the asset, or nullptr if not found/loaded
	 */
	T* Get() const
	{
		if (!m_xGUID.IsValid())
		{
			return nullptr;
		}

		// Return cached pointer if still valid
		if (m_pxCached != nullptr)
		{
			return m_pxCached;
		}

		// Resolve GUID to path
		std::string strPath = Zenith_AssetDatabase::GetPathFromGUID(m_xGUID);
		if (strPath.empty())
		{
			Zenith_Log(LOG_CATEGORY_ASSET, "AssetRef: Failed to resolve GUID %s", m_xGUID.ToString().c_str());
			return nullptr;
		}

		// Load the asset - implementation depends on asset type
		// This is a template specialization point
		m_pxCached = LoadAsset(strPath);
		return m_pxCached;
	}

	/**
	 * Dereference operator for convenient access
	 */
	T* operator->() const
	{
		return Get();
	}

	/**
	 * Bool conversion - true if GUID is valid (doesn't check if asset exists)
	 */
	explicit operator bool() const
	{
		return m_xGUID.IsValid();
	}

	/**
	 * Check if the reference is set (has a valid GUID)
	 */
	bool IsSet() const
	{
		return m_xGUID.IsValid();
	}

	/**
	 * Check if the asset is currently loaded/cached
	 */
	bool IsLoaded() const
	{
		return m_pxCached != nullptr;
	}

	//--------------------------------------------------------------------------
	// Async Loading API
	//--------------------------------------------------------------------------

	/**
	 * Start async loading of the asset
	 * @param pfnOnComplete Callback when load completes (receives asset pointer)
	 * @param pxUserData User data passed to callback
	 * @param pfnOnFail Optional callback on failure
	 */
	void LoadAsync(
		AssetLoadCompleteFn pfnOnComplete = nullptr,
		void* pxUserData = nullptr,
		AssetLoadFailFn pfnOnFail = nullptr);

	/**
	 * Non-blocking get - returns nullptr if not yet loaded
	 * Use this in update loops when waiting for async load
	 * @return Asset pointer if loaded, nullptr otherwise
	 */
	T* TryGet() const
	{
		return m_pxCached;
	}

	/**
	 * Check if the asset is ready to use (fully loaded)
	 * @return true if asset is loaded and ready
	 */
	bool IsReady() const;

	/**
	 * Get the current load state of the asset
	 * @return Current AssetLoadState
	 */
	AssetLoadState GetLoadState() const;

	/**
	 * Get the GUID of the referenced asset
	 */
	const Zenith_AssetGUID& GetGUID() const
	{
		return m_xGUID;
	}

	/**
	 * Set the GUID directly
	 */
	void SetGUID(const Zenith_AssetGUID& xGUID)
	{
		if (m_xGUID != xGUID)
		{
			m_xGUID = xGUID;
			m_pxCached = nullptr;  // Invalidate cache
		}
	}

	/**
	 * Set the reference from an asset path
	 * Looks up the GUID from the asset database
	 * @param strPath Path to the asset
	 * @return true if the asset was found
	 */
	bool SetFromPath(const std::string& strPath)
	{
		Zenith_AssetGUID xGUID = Zenith_AssetDatabase::GetGUIDFromPath(strPath);
		if (!xGUID.IsValid())
		{
			Zenith_Log(LOG_CATEGORY_ASSET, "AssetRef: No GUID found for path %s", strPath.c_str());
			return false;
		}
		SetGUID(xGUID);
		return true;
	}

	/**
	 * Get the asset path (resolves GUID via database)
	 * @return Asset path, or empty string if not found
	 */
	std::string GetPath() const
	{
		if (!m_xGUID.IsValid())
		{
			return "";
		}
		return Zenith_AssetDatabase::GetPathFromGUID(m_xGUID);
	}

	/**
	 * Clear the reference
	 */
	void Clear()
	{
		m_xGUID = Zenith_AssetGUID::INVALID;
		m_pxCached = nullptr;
	}

	/**
	 * Invalidate the cached pointer (forces reload on next Get())
	 * Called when the underlying asset has been reloaded
	 */
	void InvalidateCache()
	{
		m_pxCached = nullptr;
	}

	/**
	 * Set the cached pointer directly (for assets that manage their own loading)
	 * Use with caution - the pointer must remain valid
	 */
	void SetCachedPointer(T* pAsset)
	{
		m_pxCached = pAsset;
	}

	// Comparison operators
	bool operator==(const Zenith_AssetRef<T>& xOther) const
	{
		return m_xGUID == xOther.m_xGUID;
	}

	bool operator!=(const Zenith_AssetRef<T>& xOther) const
	{
		return m_xGUID != xOther.m_xGUID;
	}

	// Serialization
	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		xStream << m_xGUID;
	}

	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		xStream >> m_xGUID;
		m_pxCached = nullptr;  // Invalidate cache after load
	}

private:
	/**
	 * Load the asset from path
	 * This is specialized for each asset type
	 */
	T* LoadAsset(const std::string& strPath) const;

	Zenith_AssetGUID m_xGUID;
	mutable T* m_pxCached = nullptr;
};

//--------------------------------------------------------------------------
// Forward declarations for common asset types
//--------------------------------------------------------------------------

struct Flux_Texture;
class Flux_MaterialAsset;
class Flux_MeshGeometry;
class Zenith_ModelAsset;
class Zenith_Prefab;

//--------------------------------------------------------------------------
// Type aliases for common asset references
//--------------------------------------------------------------------------

using TextureRef = Zenith_AssetRef<Flux_Texture>;
using MaterialRef = Zenith_AssetRef<Flux_MaterialAsset>;
using MeshRef = Zenith_AssetRef<Flux_MeshGeometry>;
using ModelRef = Zenith_AssetRef<Zenith_ModelAsset>;
using PrefabRef = Zenith_AssetRef<Zenith_Prefab>;

//--------------------------------------------------------------------------
// Template specializations for LoadAsset
// These are defined in Zenith_AssetRef.cpp
//--------------------------------------------------------------------------

// Texture loading
template<>
Flux_Texture* Zenith_AssetRef<Flux_Texture>::LoadAsset(const std::string& strPath) const;

// Material loading
template<>
Flux_MaterialAsset* Zenith_AssetRef<Flux_MaterialAsset>::LoadAsset(const std::string& strPath) const;

// Mesh loading
template<>
Flux_MeshGeometry* Zenith_AssetRef<Flux_MeshGeometry>::LoadAsset(const std::string& strPath) const;

// Model loading
template<>
Zenith_ModelAsset* Zenith_AssetRef<Zenith_ModelAsset>::LoadAsset(const std::string& strPath) const;

// Prefab loading
template<>
Zenith_Prefab* Zenith_AssetRef<Zenith_Prefab>::LoadAsset(const std::string& strPath) const;
