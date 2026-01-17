#include "Zenith.h"
#include "Flux/Flux_ModelInstance.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "Flux/MeshGeometry/Flux_MeshInstance.h"
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"
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
		// Load skeleton from registry (shared asset with ref counting)
		Zenith_SkeletonAsset* pxSkeletonAsset = Zenith_AssetRegistry::Get().Get<Zenith_SkeletonAsset>(strSkeletonPath);

		if (pxSkeletonAsset)
		{
			pxSkeletonAsset->AddRef();  // Add reference for this instance's usage
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

		// Load the mesh asset from registry (shared asset with ref counting)
		Zenith_MeshAsset* pxMeshAsset = Zenith_AssetRegistry::Get().Get<Zenith_MeshAsset>(strMeshPath);
		if (!pxMeshAsset)
		{
			Zenith_Log(LOG_CATEGORY_MESH, "[ModelInstance] Failed to load mesh: %s", strMeshPath.c_str());
			continue;
		}
		pxMeshAsset->AddRef();  // Add reference for this instance's usage
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
			Zenith_MaterialAsset* pxMaterial = Zenith_AssetRegistry::Get().Get<Zenith_MaterialAsset>(strMaterialPath);

			if (!pxMaterial)
			{
				Zenith_Log(LOG_CATEGORY_MESH, "[ModelInstance] Failed to load material: %s", strMaterialPath.c_str());
				// Use blank material as fallback - create new default material
				pxMaterial = Zenith_AssetRegistry::Get().Create<Zenith_MaterialAsset>();
			}

			if (pxMaterial)
			{
				pxMaterial->AddRef();  // Add reference for this instance's usage
			}
			pxInstance->m_xMaterials.PushBack(pxMaterial);
		}

		// If no materials were specified, add a blank material
		if (uNumMaterials == 0)
		{
			Zenith_MaterialAsset* pxBlank = Zenith_AssetRegistry::Get().Create<Zenith_MaterialAsset>();
			if (pxBlank)
			{
				pxBlank->AddRef();
			}
			pxInstance->m_xMaterials.PushBack(pxBlank);
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

	// Release loaded materials (ref-counted, managed by registry)
	for (uint32_t u = 0; u < m_xMaterials.GetSize(); u++)
	{
		Zenith_MaterialAsset* pxMaterial = m_xMaterials.Get(u);
		if (pxMaterial)
		{
			pxMaterial->Release();
		}
	}
	m_xMaterials.Clear();

	// Release loaded mesh assets (ref-counted, managed by registry)
	for (uint32_t u = 0; u < m_xLoadedMeshAssets.GetSize(); u++)
	{
		Zenith_MeshAsset* pxMeshAsset = m_xLoadedMeshAssets.Get(u);
		if (pxMeshAsset)
		{
			pxMeshAsset->Release();
		}
	}
	m_xLoadedMeshAssets.Clear();

	// Delete skeleton instance (per-entity state, owned by us)
	if (m_pxSkeleton)
	{
		delete m_pxSkeleton;
		m_pxSkeleton = nullptr;
	}

	// Release loaded skeleton asset (ref-counted, managed by registry)
	if (m_pxLoadedSkeletonAsset)
	{
		m_pxLoadedSkeletonAsset->Release();
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

Zenith_MaterialAsset* Flux_ModelInstance::GetMaterial(uint32_t uIndex) const
{
	if (uIndex >= m_xMaterials.GetSize())
	{
		return nullptr;
	}
	return m_xMaterials.Get(uIndex);
}

void Flux_ModelInstance::SetMaterial(uint32_t uIndex, Zenith_MaterialAsset* pxMaterial)
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
