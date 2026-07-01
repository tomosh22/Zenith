#include "Zenith.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "AssetHandling/Zenith_AssetTypeIds.h"
#include "DataStream/Zenith_StreamEnvelope.h"
#include "Profiling/Zenith_Profiling.h"

//------------------------------------------------------------------------------
// MeshMaterialBinding Serialization
//------------------------------------------------------------------------------

std::string Zenith_ModelAsset::MeshMaterialBinding::GetMaterialPath(uint32_t uIndex) const
{
	if (uIndex >= m_xMaterials.GetSize())
	{
		return "";
	}
	return m_xMaterials.Get(uIndex).GetPath();
}

void Zenith_ModelAsset::MeshMaterialBinding::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Write mesh path
	m_xMesh.WriteToDataStream(xStream);

	// Write material paths
	uint32_t uNumMaterials = static_cast<uint32_t>(m_xMaterials.GetSize());
	xStream << uNumMaterials;
	for (uint32_t u = 0; u < uNumMaterials; u++)
	{
		m_xMaterials.Get(u).WriteToDataStream(xStream);
	}
}

void Zenith_ModelAsset::MeshMaterialBinding::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Read mesh path
	m_xMesh.ReadFromDataStream(xStream);

	// Read material paths
	uint32_t uNumMaterials;
	xStream >> uNumMaterials;
	for (uint32_t u = 0; u < uNumMaterials; u++)
	{
		MaterialHandle xMatHandle;
		xMatHandle.ReadFromDataStream(xStream);
		m_xMaterials.PushBack(xMatHandle);
	}
}

//------------------------------------------------------------------------------
// Move Constructor / Assignment
//------------------------------------------------------------------------------

Zenith_ModelAsset::Zenith_ModelAsset(Zenith_ModelAsset&& xOther)
{
	m_strName = std::move(xOther.m_strName);
	m_xMeshBindings = std::move(xOther.m_xMeshBindings);
	m_strSkeletonPath = std::move(xOther.m_strSkeletonPath);
	m_xAnimationPaths = std::move(xOther.m_xAnimationPaths);
	m_strSourcePath = std::move(xOther.m_strSourcePath);
}

Zenith_ModelAsset& Zenith_ModelAsset::operator=(Zenith_ModelAsset&& xOther)
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

Zenith_Result<Zenith_ModelAsset*> Zenith_ModelAsset::LoadFromFile(const char* szPath)
{
	ZENITH_PROFILE_SCOPE("Model Load + Parse");
	Zenith_DataStream xStream;
	xStream.ReadFromFile(szPath);

	// Validate file was loaded successfully
	if (!xStream.IsValid())
	{
		Zenith_Error(LOG_CATEGORY_ASSET, "LoadFromFile: Failed to read model file '%s'", szPath);
		return Zenith_ErrorCode::FILE_NOT_FOUND;
	}

	Zenith_ModelAsset* pxAsset = new Zenith_ModelAsset();
	Zenith_Status xStatus = pxAsset->ParseStream(xStream);
	if (!xStatus.IsOk())
	{
		delete pxAsset;
		return xStatus.Error();
	}
	pxAsset->m_strSourcePath = Zenith_AssetRegistry::NormalizeAssetPath(szPath);

	Zenith_Log(LOG_CATEGORY_ASSET, "Loaded model asset '%s' from %s with %u mesh bindings",
		pxAsset->m_strName.c_str(), szPath, pxAsset->GetNumMeshes());

	for (uint32_t u = 0; u < pxAsset->GetNumMeshes(); u++)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "  Mesh %u: %s", u, pxAsset->GetMeshBinding(u).GetMeshPath().c_str());
	}

	return pxAsset;
}

void Zenith_ModelAsset::Export(const char* szPath) const
{
	Zenith_Log(LOG_CATEGORY_ASSET, "Exporting model asset '%s' to %s with %u mesh bindings",
		m_strName.c_str(), szPath, GetNumMeshes());

	for (uint32_t u = 0; u < GetNumMeshes(); u++)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "  Mesh %u: %s", u, m_xMeshBindings.Get(u).GetMeshPath().c_str());
	}

	Zenith_DataStream xStream;
	WriteToDataStream(xStream);
	xStream.WriteToFile(szPath);
}

void Zenith_ModelAsset::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Typed-asset envelope (magic + type id + current schema). Legacy pre-envelope
	// .zmodel files still load via the BAD_MAGIC rewind path in ParseStream.
	Zenith_WriteStreamHeader(xStream, uZENITH_MODEL_ASSET_TYPE_ID, uZENITH_MODEL_SCHEMA_CURRENT);

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
		xStream << Zenith_AssetRegistry::NormalizeAssetPath(m_strSkeletonPath);
	}

	// Animations
	uint32_t uNumAnimations = static_cast<uint32_t>(m_xAnimationPaths.GetSize());
	xStream << uNumAnimations;
	for (uint32_t u = 0; u < uNumAnimations; u++)
	{
		xStream << Zenith_AssetRegistry::NormalizeAssetPath(m_xAnimationPaths.Get(u));
	}
}

Zenith_Status Zenith_ModelAsset::ParseStream(Zenith_DataStream& xStream)
{
	Reset();

	// Shared envelope preamble (see Zenith_ReadAssetStreamVersion).
	uint32_t uVersion = 0;
	const Zenith_Status xVerStatus = Zenith_ReadAssetStreamVersion(xStream, uZENITH_MODEL_ASSET_TYPE_ID, uVersion);
	if (!xVerStatus.IsOk())
	{
		Zenith_Error(LOG_CATEGORY_ASSET, "Zenith_ModelAsset: unsupported envelope");
		return xVerStatus.Error();
	}

	if (uVersion != uZENITH_MODEL_SCHEMA_CURRENT)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "ERROR: Unsupported model asset version %u (expected %u). Please re-export the asset.", uVersion, uZENITH_MODEL_SCHEMA_CURRENT);
		return Zenith_ErrorCode::VERSION_MISMATCH;
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
		m_strSkeletonPath = Zenith_AssetRegistry::NormalizeAssetPath(m_strSkeletonPath);
	}

	// Animations
	uint32_t uNumAnimations;
	xStream >> uNumAnimations;
	for (uint32_t u = 0; u < uNumAnimations; u++)
	{
		std::string strPath;
		xStream >> strPath;
		strPath = Zenith_AssetRegistry::NormalizeAssetPath(strPath);
		m_xAnimationPaths.PushBack(strPath);
	}

	return true;
}

void Zenith_ModelAsset::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// The void virtual is kept for DataStream <</>> dispatch; the file-load error
	// contract lives in ParseStream (called by the static LoadFromFile).
	(void)ParseStream(xStream);
}

//------------------------------------------------------------------------------
// Model Building
//------------------------------------------------------------------------------

void Zenith_ModelAsset::AddMesh(const MeshHandle& xMesh, const Zenith_Vector<MaterialHandle>& xMaterials)
{
	MeshMaterialBinding xBinding;
	xBinding.m_xMesh = xMesh;
	xBinding.m_xMaterials = xMaterials;
	m_xMeshBindings.PushBack(std::move(xBinding));
}

void Zenith_ModelAsset::AddMeshByPath(const std::string& strMeshPath, const Zenith_Vector<std::string>& xMaterialPaths)
{
	MeshMaterialBinding xBinding;
	xBinding.m_xMesh.SetPath(strMeshPath);

	for (u_int u = 0; u < xMaterialPaths.GetSize(); ++u)
	{
		MaterialHandle xMatHandle;
		xMatHandle.SetPath(xMaterialPaths.Get(u));
		xBinding.m_xMaterials.PushBack(xMatHandle);
	}

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

#include "AssetHandling/Zenith_ModelAsset.Tests.inl"
