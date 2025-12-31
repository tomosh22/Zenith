#include "Zenith.h"
#include "AssetHandling/Zenith_ModelAsset.h"

//------------------------------------------------------------------------------
// MeshMaterialBinding Serialization
//------------------------------------------------------------------------------

void Zenith_ModelAsset::MeshMaterialBinding::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_strMeshPath;

	uint32_t uNumMaterials = static_cast<uint32_t>(m_strMaterialPaths.GetSize());
	xStream << uNumMaterials;
	for (uint32_t u = 0; u < uNumMaterials; u++)
	{
		xStream << m_strMaterialPaths.Get(u);
	}
}

void Zenith_ModelAsset::MeshMaterialBinding::ReadFromDataStream(Zenith_DataStream& xStream)
{
	xStream >> m_strMeshPath;

	uint32_t uNumMaterials;
	xStream >> uNumMaterials;
	for (uint32_t u = 0; u < uNumMaterials; u++)
	{
		std::string strPath;
		xStream >> strPath;
		m_strMaterialPaths.PushBack(strPath);
	}
}

//------------------------------------------------------------------------------
// Move Constructor / Assignment
//------------------------------------------------------------------------------

Zenith_ModelAsset::Zenith_ModelAsset(Zenith_ModelAsset&& xOther) noexcept
{
	m_strName = std::move(xOther.m_strName);
	m_xMeshBindings = std::move(xOther.m_xMeshBindings);
	m_strSkeletonPath = std::move(xOther.m_strSkeletonPath);
	m_xAnimationPaths = std::move(xOther.m_xAnimationPaths);
	m_strSourcePath = std::move(xOther.m_strSourcePath);
}

Zenith_ModelAsset& Zenith_ModelAsset::operator=(Zenith_ModelAsset&& xOther) noexcept
{
	if (this != &xOther)
	{
		m_strName = std::move(xOther.m_strName);
		m_xMeshBindings = std::move(xOther.m_xMeshBindings);
		m_strSkeletonPath = std::move(xOther.m_strSkeletonPath);
		m_xAnimationPaths = std::move(xOther.m_xAnimationPaths);
		m_strSourcePath = std::move(xOther.m_strSourcePath);
	}
	return *this;
}

//------------------------------------------------------------------------------
// Loading and Saving
//------------------------------------------------------------------------------

Zenith_ModelAsset* Zenith_ModelAsset::LoadFromFile(const char* szPath)
{
	Zenith_DataStream xStream;
	xStream.ReadFromFile(szPath);

	Zenith_ModelAsset* pxAsset = new Zenith_ModelAsset();
	pxAsset->ReadFromDataStream(xStream);
	pxAsset->m_strSourcePath = szPath;

	Zenith_Log("[ModelAsset] Loaded '%s' from %s with %u mesh bindings",
		pxAsset->m_strName.c_str(), szPath, pxAsset->GetNumMeshes());

	for (uint32_t u = 0; u < pxAsset->GetNumMeshes(); u++)
	{
		Zenith_Log("[ModelAsset]   Mesh %u: %s", u, pxAsset->GetMeshBinding(u).m_strMeshPath.c_str());
	}

	return pxAsset;
}

void Zenith_ModelAsset::Export(const char* szPath) const
{
	Zenith_Log("[ModelAsset] Exporting '%s' to %s with %u mesh bindings",
		m_strName.c_str(), szPath, GetNumMeshes());

	for (uint32_t u = 0; u < GetNumMeshes(); u++)
	{
		Zenith_Log("[ModelAsset]   Mesh %u: %s", u, m_xMeshBindings.Get(u).m_strMeshPath.c_str());
	}

	Zenith_DataStream xStream;
	WriteToDataStream(xStream);
	xStream.WriteToFile(szPath);
}

void Zenith_ModelAsset::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Version
	xStream << static_cast<uint32_t>(ZENITH_MODEL_ASSET_VERSION);

	// Name
	xStream << m_strName;

	// Mesh bindings
	uint32_t uNumMeshes = static_cast<uint32_t>(m_xMeshBindings.GetSize());
	xStream << uNumMeshes;
	for (uint32_t u = 0; u < uNumMeshes; u++)
	{
		m_xMeshBindings.Get(u).WriteToDataStream(xStream);
	}

	// Skeleton
	bool bHasSkeleton = HasSkeleton();
	xStream << bHasSkeleton;
	if (bHasSkeleton)
	{
		xStream << m_strSkeletonPath;
	}

	// Animations
	uint32_t uNumAnimations = static_cast<uint32_t>(m_xAnimationPaths.GetSize());
	xStream << uNumAnimations;
	for (uint32_t u = 0; u < uNumAnimations; u++)
	{
		xStream << m_xAnimationPaths.Get(u);
	}
}

void Zenith_ModelAsset::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Reset();

	// Version
	uint32_t uVersion;
	xStream >> uVersion;

	if (uVersion != ZENITH_MODEL_ASSET_VERSION)
	{
		Zenith_Log("[ModelAsset] Version mismatch: expected %u, got %u", ZENITH_MODEL_ASSET_VERSION, uVersion);
	}

	// Name
	xStream >> m_strName;

	// Mesh bindings
	uint32_t uNumMeshes;
	xStream >> uNumMeshes;
	for (uint32_t u = 0; u < uNumMeshes; u++)
	{
		MeshMaterialBinding xBinding;
		xBinding.ReadFromDataStream(xStream);
		m_xMeshBindings.PushBack(std::move(xBinding));
	}

	// Skeleton
	bool bHasSkeleton;
	xStream >> bHasSkeleton;
	if (bHasSkeleton)
	{
		xStream >> m_strSkeletonPath;
	}

	// Animations
	uint32_t uNumAnimations;
	xStream >> uNumAnimations;
	for (uint32_t u = 0; u < uNumAnimations; u++)
	{
		std::string strPath;
		xStream >> strPath;
		m_xAnimationPaths.PushBack(strPath);
	}
}

//------------------------------------------------------------------------------
// Model Building
//------------------------------------------------------------------------------

void Zenith_ModelAsset::AddMesh(const std::string& strMeshPath, const Zenith_Vector<std::string>& xMaterialPaths)
{
	MeshMaterialBinding xBinding;
	xBinding.m_strMeshPath = strMeshPath;
	xBinding.m_strMaterialPaths = xMaterialPaths;
	m_xMeshBindings.PushBack(std::move(xBinding));
}

void Zenith_ModelAsset::Reset()
{
	m_strName.clear();
	m_xMeshBindings.Clear();
	m_strSkeletonPath.clear();
	m_xAnimationPaths.Clear();
	m_strSourcePath.clear();
}
