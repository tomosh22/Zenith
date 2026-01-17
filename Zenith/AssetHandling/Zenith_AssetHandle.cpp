#include "Zenith.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "Prefab/Zenith_Prefab.h"
#include "DataStream/Zenith_DataStream.h"

//--------------------------------------------------------------------------
// Template specializations for Get()
//--------------------------------------------------------------------------

template<>
Zenith_TextureAsset* Zenith_AssetHandle<Zenith_TextureAsset>::Get()
{
	if (m_strPath.empty())
	{
		return nullptr;
	}

	// Check cache
	if (m_pxCached)
	{
		return m_pxCached;
	}

	// Load from registry
	m_pxCached = Zenith_AssetRegistry::Get().Get<Zenith_TextureAsset>(m_strPath);
	if (m_pxCached)
	{
		m_pxCached->AddRef();
	}
	return m_pxCached;
}

//--------------------------------------------------------------------------
// Serialization for all handle types
//--------------------------------------------------------------------------

template<>
void Zenith_AssetHandle<Zenith_TextureAsset>::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_strPath;
}

template<>
void Zenith_AssetHandle<Zenith_TextureAsset>::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Release old reference
	if (m_pxCached)
	{
		m_pxCached->Release();
		m_pxCached = nullptr;
	}

	xStream >> m_strPath;
}

//--------------------------------------------------------------------------
// Forward declarations for other asset types (to be implemented later)
//--------------------------------------------------------------------------

// These will be implemented as their respective asset classes are created

template<>
Zenith_MaterialAsset* Zenith_AssetHandle<Zenith_MaterialAsset>::Get()
{
	if (m_strPath.empty())
	{
		return nullptr;
	}

	// Check cache
	if (m_pxCached)
	{
		return m_pxCached;
	}

	// Load from registry
	m_pxCached = Zenith_AssetRegistry::Get().Get<Zenith_MaterialAsset>(m_strPath);
	if (m_pxCached)
	{
		m_pxCached->AddRef();
	}
	return m_pxCached;
}

template<>
Zenith_MeshAsset* Zenith_AssetHandle<Zenith_MeshAsset>::Get()
{
	if (m_strPath.empty())
	{
		return nullptr;
	}

	// Check cache
	if (m_pxCached)
	{
		return m_pxCached;
	}

	// Load from registry
	m_pxCached = Zenith_AssetRegistry::Get().Get<Zenith_MeshAsset>(m_strPath);
	if (m_pxCached)
	{
		m_pxCached->AddRef();
	}
	return m_pxCached;
}

template<>
Zenith_SkeletonAsset* Zenith_AssetHandle<Zenith_SkeletonAsset>::Get()
{
	if (m_strPath.empty())
	{
		return nullptr;
	}

	// Check cache
	if (m_pxCached)
	{
		return m_pxCached;
	}

	// Load from registry
	m_pxCached = Zenith_AssetRegistry::Get().Get<Zenith_SkeletonAsset>(m_strPath);
	if (m_pxCached)
	{
		m_pxCached->AddRef();
	}
	return m_pxCached;
}

template<>
Zenith_ModelAsset* Zenith_AssetHandle<Zenith_ModelAsset>::Get()
{
	if (m_strPath.empty())
	{
		return nullptr;
	}

	// Check cache
	if (m_pxCached)
	{
		return m_pxCached;
	}

	// Load from registry
	m_pxCached = Zenith_AssetRegistry::Get().Get<Zenith_ModelAsset>(m_strPath);
	if (m_pxCached)
	{
		m_pxCached->AddRef();
	}
	return m_pxCached;
}

// Serialization stubs for other types
template<>
void Zenith_AssetHandle<Zenith_MaterialAsset>::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_strPath;
}

template<>
void Zenith_AssetHandle<Zenith_MaterialAsset>::ReadFromDataStream(Zenith_DataStream& xStream)
{
	if (m_pxCached)
	{
		m_pxCached->Release();
		m_pxCached = nullptr;
	}
	xStream >> m_strPath;
}

template<>
void Zenith_AssetHandle<Zenith_MeshAsset>::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_strPath;
}

template<>
void Zenith_AssetHandle<Zenith_MeshAsset>::ReadFromDataStream(Zenith_DataStream& xStream)
{
	if (m_pxCached)
	{
		m_pxCached->Release();
		m_pxCached = nullptr;
	}
	xStream >> m_strPath;
}

template<>
void Zenith_AssetHandle<Zenith_SkeletonAsset>::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_strPath;
}

template<>
void Zenith_AssetHandle<Zenith_SkeletonAsset>::ReadFromDataStream(Zenith_DataStream& xStream)
{
	if (m_pxCached)
	{
		m_pxCached->Release();
		m_pxCached = nullptr;
	}
	xStream >> m_strPath;
}

template<>
void Zenith_AssetHandle<Zenith_ModelAsset>::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_strPath;
}

template<>
void Zenith_AssetHandle<Zenith_ModelAsset>::ReadFromDataStream(Zenith_DataStream& xStream)
{
	if (m_pxCached)
	{
		m_pxCached->Release();
		m_pxCached = nullptr;
	}
	xStream >> m_strPath;
}

//--------------------------------------------------------------------------
// Prefab specializations
//--------------------------------------------------------------------------

template<>
Zenith_Prefab* Zenith_AssetHandle<Zenith_Prefab>::Get()
{
	if (m_strPath.empty())
	{
		return nullptr;
	}

	// Check cache
	if (m_pxCached)
	{
		return m_pxCached;
	}

	// Load from registry
	m_pxCached = Zenith_AssetRegistry::Get().Get<Zenith_Prefab>(m_strPath);
	if (m_pxCached)
	{
		m_pxCached->AddRef();
	}
	return m_pxCached;
}

template<>
void Zenith_AssetHandle<Zenith_Prefab>::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_strPath;
}

template<>
void Zenith_AssetHandle<Zenith_Prefab>::ReadFromDataStream(Zenith_DataStream& xStream)
{
	if (m_pxCached)
	{
		m_pxCached->Release();
		m_pxCached = nullptr;
	}
	xStream >> m_strPath;
}
