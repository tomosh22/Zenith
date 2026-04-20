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
		// Load skeleton from registry via handle (handles ref counting automatically)
		pxInstance->m_xLoadedSkeletonAsset.SetPath(strSkeletonPath);
		Zenith_SkeletonAsset* pxSkeletonAsset = pxInstance->m_xLoadedSkeletonAsset.Get();

		if (pxSkeletonAsset)
		{
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

	const uint32_t uNumMeshBindings = pxAsset->GetNumMeshes();
	for (uint32_t uMeshIdx = 0; uMeshIdx < uNumMeshBindings; uMeshIdx++)
	{
		pxInstance->BuildSubMeshInstance(uMeshIdx, pxAsset);
	}

	Zenith_Log(LOG_CATEGORY_MESH, "[ModelInstance] Created instance with %u meshes, %u materials%s",
		pxInstance->GetNumMeshes(),
		pxInstance->GetNumMaterials(),
		pxInstance->HasSkeleton() ? ", with skeleton" : "");

	return pxInstance;
}

void Flux_ModelInstance::BuildSubMeshInstance(uint32_t uMeshIdx, Zenith_ModelAsset* pxAsset)
{
	const Zenith_ModelAsset::MeshMaterialBinding& xBinding = pxAsset->GetMeshBinding(uMeshIdx);
	const std::string strMeshPath = xBinding.GetMeshPath();

	MeshHandle xMeshHandle(strMeshPath);
	Zenith_MeshAsset* pxMeshAsset = xMeshHandle.Get();
	if (!pxMeshAsset)
	{
		Zenith_Log(LOG_CATEGORY_MESH, "[ModelInstance] Failed to load mesh: %s", strMeshPath.c_str());
		return;
	}
	m_xLoadedMeshAssets.PushBack(std::move(xMeshHandle));

	// Pass skeleton for skinned meshes to apply bind pose transforms for static rendering.
	Flux_MeshInstance* pxMeshInstance = Flux_MeshInstance::CreateFromAsset(pxMeshAsset, m_xLoadedSkeletonAsset.Get());
	if (!pxMeshInstance)
	{
		Zenith_Log(LOG_CATEGORY_MESH, "[ModelInstance] Failed to create mesh instance from: %s", strMeshPath.c_str());
		return;
	}
	m_xMeshInstances.PushBack(pxMeshInstance);

	// Keep m_xSkinnedMeshInstances aligned with m_xMeshInstances — always push an
	// entry (possibly nullptr) for skeleton-bearing models so indices match up.
	if (m_pxSkeleton)
	{
		Flux_MeshInstance* pxSkinned = nullptr;
		if (pxMeshAsset->HasSkinning())
		{
			pxSkinned = Flux_MeshInstance::CreateSkinnedFromAsset(pxMeshAsset);
			if (!pxSkinned)
			{
				Zenith_Log(LOG_CATEGORY_MESH, "[ModelInstance] Failed to create skinned mesh instance from: %s", strMeshPath.c_str());
			}
		}
		m_xSkinnedMeshInstances.PushBack(pxSkinned);
	}

	const uint32_t uNumMaterials = static_cast<uint32_t>(xBinding.m_xMaterials.GetSize());
	for (uint32_t uMatIdx = 0; uMatIdx < uNumMaterials; uMatIdx++)
	{
		const std::string strMaterialPath = xBinding.GetMaterialPath(uMatIdx);
		MaterialHandle xMaterialHandle(strMaterialPath);

		if (!xMaterialHandle.Get())
		{
			Zenith_Log(LOG_CATEGORY_MESH, "[ModelInstance] Failed to load material: %s", strMaterialPath.c_str());
			xMaterialHandle.Set(Zenith_AssetRegistry::Get().Create<Zenith_MaterialAsset>());
		}

		m_xMaterials.PushBack(std::move(xMaterialHandle));
	}

	if (uNumMaterials == 0)
	{
		MaterialHandle xBlankHandle;
		xBlankHandle.Set(Zenith_AssetRegistry::Get().Create<Zenith_MaterialAsset>());
		m_xMaterials.PushBack(std::move(xBlankHandle));
	}
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

	// Clear materials (handles auto-release when cleared)
	m_xMaterials.Clear();

	// Clear loaded mesh assets (handles auto-release when cleared)
	m_xLoadedMeshAssets.Clear();

	// Delete skeleton instance (per-entity state, owned by us)
	if (m_pxSkeleton)
	{
		delete m_pxSkeleton;
		m_pxSkeleton = nullptr;
	}

	// Clear skeleton asset handle (auto-releases ref count)
	m_xLoadedSkeletonAsset.Clear();

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
	return m_xMaterials.Get(uIndex).Get();
}

void Flux_ModelInstance::SetMaterial(uint32_t uIndex, Zenith_MaterialAsset* pxMaterial)
{
	// Assert to catch null materials early - this would cause GBuffer rendering to skip the mesh
	Zenith_Assert(pxMaterial != nullptr,
		"SetMaterial called with nullptr for index %u - mesh will not render in GBuffer pass", uIndex);

	// Ensure array has enough elements
	while (m_xMaterials.GetSize() <= uIndex)
	{
		m_xMaterials.PushBack(MaterialHandle());
	}
	m_xMaterials.Get(uIndex).Set(pxMaterial);
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
