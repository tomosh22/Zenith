#pragma once

#include <string>
#include "AssetHandling/Zenith_Asset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
class Zenith_TextureAsset;
class Zenith_MaterialAsset;
class Zenith_MeshAsset;
class Zenith_SkeletonAsset;
class Zenith_ModelAsset;
class Zenith_AnimationAsset;
class Zenith_MeshGeometryAsset;
class Zenith_FontAsset;
class Zenith_Prefab;
class Zenith_DataStream;

/**
 * Zenith_AssetHandle<T> - smart, ref-counted handle; THE way to reference assets.
 * Stores a prefixed path ("game:" / "engine:") and serializes by path.
 *   Resolve()   - file-backed: lazy-loads via the registry and caches. Default accessor.
 *   GetDirect() - procedural: returns the pointer stored via Set(), no registry call.
 * See AssetHandling/CLAUDE.md for the full accessor breakdown.
 */
// Ref-counts m_pxCached through the Zenith_Asset base. The cast is reinterpret_,
// not static_, because these inline bodies are instantiated in TUs where T is only
// forward-declared (a static_cast/upcast needs T complete). The T-derives-from-
// Zenith_Asset invariant the cast relies on is enforced by a static_assert in
// Get() (Zenith_AssetHandle.cpp), checked at each explicit instantiation where T
// is complete.

// Tag type for the registry-only adopt-reference constructor: the reference was
// already taken under the registry lock, so the handle stores it WITHOUT AddRef'ing.
struct Zenith_AdoptAssetRef {};

template<typename T>
class Zenith_AssetHandle
{
public:
	Zenith_AssetHandle() = default;

	/**
	 * Construct from path - does NOT load immediately
	 */
	explicit Zenith_AssetHandle(const std::string& strPath)
		: m_strPath(Zenith_AssetRegistry::NormalizeAssetPath(strPath))
		, m_pxCached(nullptr)
	{
	}

	/**
	 * Construct directly from an asset pointer (procedural assets created via registry.Create<T>()).
	 * Path is left empty; ref count is incremented via Set().
	 */
	explicit Zenith_AssetHandle(T* pxAsset)
	{
		Set(pxAsset);
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
	 * Get the directly stored pointer (for procedural assets set via Set()).
	 * Returns nullptr for file-based handles that have not been loaded.
	 * For file-based assets, use Resolve() (or Zenith_AssetRegistry::GetView<T>(GetPath()) for a raw view) instead.
	 */
	T* GetDirect() const { return m_pxCached; }

	/**
	 * Resolve the asset for this handle, loading from the registry on demand.
	 *
	 * - For file-based handles, returns the cached pointer if already loaded;
	 *   otherwise acquires it UNDER the registry lock (Acquire<T>, race-free) and
	 *   caches the result. The AddRef happens while the lock is held, so a
	 *   concurrent UnloadUnused can't reclaim the asset between load and AddRef.
	 * - For procedural handles populated via Set(), returns the stored pointer.
	 * - Returns nullptr if the path is empty AND no procedural pointer was set,
	 *   or if the registry load fails.
	 *
	 * This is the default accessor for game/component code. Prefer it over the
	 * two-step `Zenith_AssetRegistry::GetView<T>(handle.GetPath())` pattern.
	 */
	T* Resolve() const { return Get(); }

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
	 * True if this handle stores a (serializable) path.
	 * Note: procedural assets set via Set() have NO path — use operator bool()
	 * or IsResolved() for those. (Renamed from IsSet() for non-overlapping semantics.)
	 */
	bool HasPath() const
	{
		return !m_strPath.empty();
	}

	/**
	 * True if the asset pointer is currently resolved/cached (loaded or Set()).
	 * (Renamed from IsLoaded(); pairs with Resolve().)
	 */
	bool IsResolved() const
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
	 * Normalizes absolute paths to prefixed relative paths for cross-machine portability
	 */
	void SetPath(const std::string& strPath)
	{
		std::string strNormalized = Zenith_AssetRegistry::NormalizeAssetPath(strPath);
		if (m_strPath != strNormalized)
		{
			if (m_pxCached)
			{
				reinterpret_cast<Zenith_Asset*>(m_pxCached)->Release();
				m_pxCached = nullptr;
			}
			m_strPath = strNormalized;
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
	// Registry-only: adopt an already-AddRef'd pointer + retain the (normalized) path.
	// The ref was taken while the registry held its lock (Acquire/Create/AdoptOrGet),
	// so we do NOT AddRef again here — this is the race-free construction path.
	Zenith_AssetHandle(const std::string& strPath, T* pxAdoptedRef, Zenith_AdoptAssetRef)
		: m_strPath(Zenith_AssetRegistry::NormalizeAssetPath(strPath))
		, m_pxCached(pxAdoptedRef)
	{
	}

	friend class Zenith_AssetRegistry;

	T* Get() const;  // Internal lazy-loading accessor — defined in Zenith_AssetHandle.cpp

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
using FontHandle = Zenith_AssetHandle<Zenith_FontAsset>;
using PrefabHandle = Zenith_AssetHandle<Zenith_Prefab>;

// Get/WriteToDataStream/ReadFromDataStream are defined in Zenith_AssetHandle.cpp
// and explicitly instantiated there for each asset type above.

//--------------------------------------------------------------------------
// Zenith_AssetRegistry handle-returning factories.
// Defined here (not in Zenith_AssetRegistry.h) because they return a
// Zenith_AssetHandle<T> BY VALUE, which needs the handle type complete — the
// registry header only forward-declares it (the two headers form an include
// cycle). Each routes through an under-lock internal primitive that AddRefs while
// the registry mutex is held, then adopts that ref into the handle, closing the
// load-then-AddRef race. Any TU that calls Create/Acquire/AdoptOrGet therefore
// includes this header.
//--------------------------------------------------------------------------

template<typename T>
Zenith_AssetHandle<T> Zenith_AssetRegistry::Acquire(const std::string& strPath)
{
	Zenith_Asset* pxAsset = s_pxInstance->AcquireInternal(Zenith_TypeIndex::Of<T>(), strPath);
	return Zenith_AssetHandle<T>(strPath, static_cast<T*>(pxAsset), Zenith_AdoptAssetRef{});
}

template<typename T>
Zenith_AssetHandle<T> Zenith_AssetRegistry::Create()
{
	Zenith_Asset* pxAsset = s_pxInstance->CreateProceduralInternal(Zenith_TypeIndex::Of<T>());
	// Empty stored path: a no-arg procedural asset isn't path-serializable (matches
	// the historical Set(ptr)); the asset still carries its generated procedural:// id.
	return Zenith_AssetHandle<T>(std::string(), static_cast<T*>(pxAsset), Zenith_AdoptAssetRef{});
}

template<typename T>
Zenith_AssetHandle<T> Zenith_AssetRegistry::Create(const std::string& strPath)
{
	Zenith_Asset* pxAsset = s_pxInstance->GetOrCreateInternal(Zenith_TypeIndex::Of<T>(), strPath);
	return Zenith_AssetHandle<T>(strPath, static_cast<T*>(pxAsset), Zenith_AdoptAssetRef{});
}

template<typename T>
Zenith_AssetHandle<T> Zenith_AssetRegistry::AdoptOrGet(const std::string& strPath, T* pxPrebuilt)
{
	Zenith_Asset* pxAsset = s_pxInstance->AdoptOrGetInternal(strPath, static_cast<Zenith_Asset*>(pxPrebuilt));
	return Zenith_AssetHandle<T>(strPath, static_cast<T*>(pxAsset), Zenith_AdoptAssetRef{});
}
