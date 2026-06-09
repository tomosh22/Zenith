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

template<typename T>
T* Zenith_AssetHandle<T>::Get() const
{
	// Cached check must come first: procedural assets have a pointer but no path
	if (m_pxCached)
	{
		return m_pxCached;
	}

	if (m_strPath.empty())
	{
		return nullptr;
	}

	m_pxCached = Zenith_AssetRegistry::Get<T>(m_strPath);
	if (m_pxCached)
	{
		m_pxCached->AddRef();
	}
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
