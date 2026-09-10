// Explicit-instantiation implementation shared by AssetHandling's built-in
// assets and feature-owned asset types (for example Prefab).  Keeping it in an
// include lets the owning module instantiate its handle without making this
// module include that feature's concrete header.
#include <type_traits>

template<typename T>
T* Zenith_AssetHandle<T>::Get() const
{
	static_assert(std::is_base_of_v<Zenith_Asset, T>,
		"Zenith_AssetHandle<T>: T must derive from Zenith_Asset");
	if (m_pxCached)
	{
		return m_pxCached;
	}
	if (m_strPath.empty())
	{
		return nullptr;
	}
	Zenith_AssetHandle<T> xAcquired = Zenith_AssetRegistry::Acquire<T>(m_strPath);
	m_pxCached = xAcquired.m_pxCached;
	xAcquired.m_pxCached = nullptr;
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
