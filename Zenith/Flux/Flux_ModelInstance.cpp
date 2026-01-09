#include "Zenith.h"
#include "Flux/Flux_ModelInstance.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "Flux/MeshGeometry/Flux_MeshInstance.h"
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"
#include "Flux/Flux_MaterialAsset.h"
#include "Flux/Flux_Graphics.h"

//------------------------------------------------------------------------------
// Destructor
//------------------------------------------------------------------------------

Flux_ModelInstance::~Flux_ModelInstance()
{
	Destroy();
}

//------------------------------------------------------------------------------
// Factory Methods
//------------------------------------------------------------------------------

Flux_ModelInstance* Flux_ModelInstance::CreateFromAsset(Zenith_ModelAsset* pxAsset)
{
	if (!pxAsset)
	{
		Zenith_Log(LOG_CATEGORY_MESH, "[ModelInstance] Cannot create instance from null asset");
		return nullptr;
	}

	Flux_ModelInstance* pxInstance = new Flux_ModelInstance();
	pxInstance->m_pxSourceAsset = pxAsset;

	// Load skeleton if the model has one
	if (pxAsset->HasSkeleton())
	{
		const std::string& strSkeletonPath = pxAsset->GetSkeletonPath();
		Zenith_SkeletonAsset* pxSkeletonAsset = Zenith_SkeletonAsset::LoadFromFile(strSkeletonPath.c_str());

		if (pxSkeletonAsset)
		{
			pxInstance->m_pxLoadedSkeletonAsset = pxSkeletonAsset;
			pxInstance->m_pxSkeleton = Flux_SkeletonInstance::CreateFromAsset(pxSkeletonAsset);

			if (!pxInstance->m_pxSkeleton)
			{
				Zenith_Log(LOG_CATEGORY_MESH, "[ModelInstance] Failed to create skeleton instance from: %s", strSkeletonPath.c_str());
			}
		}
		else
		{
			Zenith_Log(LOG_CATEGORY_MESH, "[ModelInstance] Failed to load skeleton: %s", strSkeletonPath.c_str());
		}
	}

	// Load each mesh and its materials
	uint32_t uNumMeshBindings = pxAsset->GetNumMeshes();
	for (uint32_t uMeshIdx = 0; uMeshIdx < uNumMeshBindings; uMeshIdx++)
	{
		const Zenith_ModelAsset::MeshMaterialBinding& xBinding = pxAsset->GetMeshBinding(uMeshIdx);

		// Get mesh path from the MeshRef
		std::string strMeshPath = xBinding.GetMeshPath();

		// Load the mesh asset
		Zenith_MeshAsset* pxMeshAsset = Zenith_MeshAsset::LoadFromFile(strMeshPath.c_str());
		if (!pxMeshAsset)
		{
			Zenith_Log(LOG_CATEGORY_MESH, "[ModelInstance] Failed to load mesh: %s", strMeshPath.c_str());
			continue;
		}
		pxInstance->m_xLoadedMeshAssets.PushBack(pxMeshAsset);

		// Create GPU mesh instance from the mesh asset
		// Pass skeleton for skinned meshes to apply bind pose transforms for static rendering
		Flux_MeshInstance* pxMeshInstance = Flux_MeshInstance::CreateFromAsset(pxMeshAsset, pxInstance->m_pxLoadedSkeletonAsset);
		if (!pxMeshInstance)
		{
			Zenith_Log(LOG_CATEGORY_MESH, "[ModelInstance] Failed to create mesh instance from: %s", strMeshPath.c_str());
			continue;
		}
		pxInstance->m_xMeshInstances.PushBack(pxMeshInstance);

		// If the model has a skeleton, also create a skinned mesh instance
		// for animated rendering (104-byte format with bone indices/weights)
		// IMPORTANT: Always push to m_xSkinnedMeshInstances to keep indices aligned with m_xMeshInstances
		if (pxInstance->m_pxSkeleton)
		{
			if (pxMeshAsset->HasSkinning())
			{
				Flux_MeshInstance* pxSkinnedMeshInstance = Flux_MeshInstance::CreateSkinnedFromAsset(pxMeshAsset);
				if (pxSkinnedMeshInstance)
				{
					pxInstance->m_xSkinnedMeshInstances.PushBack(pxSkinnedMeshInstance);
				}
				else
				{
					Zenith_Log(LOG_CATEGORY_MESH, "[ModelInstance] Failed to create skinned mesh instance from: %s", strMeshPath.c_str());
					// Push nullptr to keep indices in sync
					pxInstance->m_xSkinnedMeshInstances.PushBack(nullptr);
				}
			}
			else
			{
				// Mesh doesn't have skinning data - push nullptr to keep indices in sync
				pxInstance->m_xSkinnedMeshInstances.PushBack(nullptr);
			}
		}

		// Load materials for this mesh
		uint32_t uNumMaterials = static_cast<uint32_t>(xBinding.m_xMaterials.GetSize());
		for (uint32_t uMatIdx = 0; uMatIdx < uNumMaterials; uMatIdx++)
		{
			std::string strMaterialPath = xBinding.GetMaterialPath(uMatIdx);
			Flux_MaterialAsset* pxMaterial = Flux_MaterialAsset::LoadFromFile(strMaterialPath);

			if (!pxMaterial)
			{
				Zenith_Log(LOG_CATEGORY_MESH, "[ModelInstance] Failed to load material: %s", strMaterialPath.c_str());
				// Use blank material as fallback
				pxMaterial = Flux_Graphics::s_pxBlankMaterial;
			}

			pxInstance->m_xMaterials.PushBack(pxMaterial);
		}

		// If no materials were specified, add a blank material
		if (uNumMaterials == 0)
		{
			pxInstance->m_xMaterials.PushBack(Flux_Graphics::s_pxBlankMaterial);
		}
	}

	Zenith_Log(LOG_CATEGORY_MESH, "[ModelInstance] Created instance with %u meshes, %u materials%s",
		pxInstance->GetNumMeshes(),
		pxInstance->GetNumMaterials(),
		pxInstance->HasSkeleton() ? ", with skeleton" : "");

	return pxInstance;
}

//------------------------------------------------------------------------------
// Lifecycle
//------------------------------------------------------------------------------

void Flux_ModelInstance::Destroy()
{
	// Delete mesh instances (static format)
	for (uint32_t u = 0; u < m_xMeshInstances.GetSize(); u++)
	{
		Flux_MeshInstance* pxMeshInstance = m_xMeshInstances.Get(u);
		if (pxMeshInstance)
		{
			delete pxMeshInstance;
		}
	}
	m_xMeshInstances.Clear();

	// Delete skinned mesh instances (104-byte format)
	for (uint32_t u = 0; u < m_xSkinnedMeshInstances.GetSize(); u++)
	{
		Flux_MeshInstance* pxSkinnedMeshInstance = m_xSkinnedMeshInstances.Get(u);
		if (pxSkinnedMeshInstance)
		{
			delete pxSkinnedMeshInstance;
		}
	}
	m_xSkinnedMeshInstances.Clear();

	// Note: Materials are owned by the Flux_MaterialAsset cache and should not be deleted here
	// Just clear our references
	m_xMaterials.Clear();

	// Delete loaded mesh assets
	for (uint32_t u = 0; u < m_xLoadedMeshAssets.GetSize(); u++)
	{
		delete m_xLoadedMeshAssets.Get(u);
	}
	m_xLoadedMeshAssets.Clear();

	// Delete skeleton instance
	if (m_pxSkeleton)
	{
		delete m_pxSkeleton;
		m_pxSkeleton = nullptr;
	}

	// Delete loaded skeleton asset
	if (m_pxLoadedSkeletonAsset)
	{
		delete m_pxLoadedSkeletonAsset;
		m_pxLoadedSkeletonAsset = nullptr;
	}

	// Clear source asset reference (not owned by us)
	m_pxSourceAsset = nullptr;
}

//------------------------------------------------------------------------------
// Accessors
//------------------------------------------------------------------------------

Flux_MeshInstance* Flux_ModelInstance::GetMeshInstance(uint32_t uIndex) const
{
	if (uIndex >= m_xMeshInstances.GetSize())
	{
		return nullptr;
	}
	return m_xMeshInstances.Get(uIndex);
}

Flux_MeshInstance* Flux_ModelInstance::GetSkinnedMeshInstance(uint32_t uIndex) const
{
	if (uIndex >= m_xSkinnedMeshInstances.GetSize())
	{
		return nullptr;
	}
	return m_xSkinnedMeshInstances.Get(uIndex);
}

Flux_MaterialAsset* Flux_ModelInstance::GetMaterial(uint32_t uIndex) const
{
	if (uIndex >= m_xMaterials.GetSize())
	{
		return nullptr;
	}
	return m_xMaterials.Get(uIndex);
}

void Flux_ModelInstance::SetMaterial(uint32_t uIndex, Flux_MaterialAsset* pxMaterial)
{
	// Ensure array has enough elements
	while (m_xMaterials.GetSize() <= uIndex)
	{
		m_xMaterials.PushBack(nullptr);
	}
	m_xMaterials.Get(uIndex) = pxMaterial;
}

//------------------------------------------------------------------------------
// Animation
//------------------------------------------------------------------------------

void Flux_ModelInstance::UpdateAnimation()
{
	if (m_pxSkeleton)
	{
		m_pxSkeleton->ComputeSkinningMatrices();
		m_pxSkeleton->UploadToGPU();
	}
}
