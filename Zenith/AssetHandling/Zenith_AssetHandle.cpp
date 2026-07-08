#include "Zenith.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "AssetHandling/Zenith_AnimationAsset.h"
#include "AssetHandling/Zenith_MeshGeometryAsset.h"
#include "AssetHandling/Zenith_FontAsset.h"
#include "Prefab/Zenith_Prefab.h"

#include <type_traits>

template<typename T>
T* Zenith_AssetHandle<T>::Get() const
{
	// Enforce the invariant the header's reinterpret_cast ref-counting relies on.
	// Checked here (not in the class body) because Get() is only instantiated at
	// the explicit instantiations below, where T is complete — a class-body assert
	// would also fire in TUs that instantiate handle members with a forward-
	// declared T, which the reinterpret_cast deliberately supports.
	static_assert(std::is_base_of_v<Zenith_Asset, T>,
		"Zenith_AssetHandle<T>: T must derive from Zenith_Asset");

	// Cached check must come first: procedural assets have a pointer but no path
	if (m_pxCached)
	{
		return m_pxCached;
	}

	if (m_strPath.empty())
	{
		return nullptr;
	}

	// Load + AddRef happen UNDER the registry lock inside Acquire (race-free); steal
	// that ref into our cache. The old GetView()+AddRef here AddRef'd OUTSIDE the
	// lock, leaving a window where a concurrent UnloadUnused could delete the 0-ref
	// asset between the load and the AddRef.
	Zenith_AssetHandle<T> xAcquired = Zenith_AssetRegistry::Acquire<T>(m_strPath);
	m_pxCached = xAcquired.m_pxCached;
	xAcquired.m_pxCached = nullptr;  // transfer ownership; suppress xAcquired's Release
	return m_pxCached;
}

template<typename T>
void Zenith_AssetHandle<T>::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << Zenith_AssetRegistry::NormalizeAssetPath(m_strPath);
}

template<typename T>
void Zenith_AssetHandle<T>::ReadFromDataStream(Zenith_DataStream& xStream)
{
	if (m_pxCached)
	{
		m_pxCached->Release();
		m_pxCached = nullptr;
	}
	xStream >> m_strPath;
	m_strPath = Zenith_AssetRegistry::NormalizeAssetPath(m_strPath);
}

template class Zenith_AssetHandle<Zenith_TextureAsset>;
template class Zenith_AssetHandle<Zenith_MaterialAsset>;
template class Zenith_AssetHandle<Zenith_MeshAsset>;
template class Zenith_AssetHandle<Zenith_SkeletonAsset>;
template class Zenith_AssetHandle<Zenith_ModelAsset>;
template class Zenith_AssetHandle<Zenith_AnimationAsset>;
template class Zenith_AssetHandle<Zenith_MeshGeometryAsset>;
template class Zenith_AssetHandle<Zenith_FontAsset>;
template class Zenith_AssetHandle<Zenith_Prefab>;
