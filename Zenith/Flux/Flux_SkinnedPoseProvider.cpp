#include "Zenith.h"

// Real (GPU-touching) skinned-pose provider for Flux_SkinnedPoseRegistry. Relocated
// out of Flux.cpp (god-file decomposition) — same code, its own translation unit next
// to the skinning types it serves. Wired into the registry from Flux::LateInitialise
// via Flux_MakeRealSkinnedPoseProvider() (declared in Flux_Skinning.h).
#include "Flux/UnifiedMesh/Flux_Skinning.h"          // Flux_SkinnedPose{Key,Entry,Registry}, uFLUX_SKIN_INPUT_WORDS
#include "Flux/MeshGeometry/Flux_MeshInstance.h"      // CreateSkinnedFromAsset + Flux_InterleaveMeshVertices
#include "AssetHandling/Zenith_MeshAsset.h"           // GetNumVerts / HasSkinning / bind-pose vertex arrays

// ----------------------------------------------------------------------------
// Real skinned-pose provider (Stage 5) — the only GPU-touching part of the pose store.
// build   = Flux_MeshInstance::CreateSkinnedFromAsset + replicate its 104B interleave
//           (Flux_SkinInputVertex) into raw words (uFLUX_SKIN_INPUT_WORDS/vert), the
//           compute-skinning input. Returns false (no live entry) for an empty / non-skinned
//           asset. destroy = deferred-delete the shared IB/VB + free the heap entry.
// The persistent bind-pose POOL append + repack is owned by Flux_SkinnedPoseRegistry (the pool
// is ITS state); this provider only builds/destroys one entry. Wired in LateInitialise; runs
// windowed, never headless (the pure registry orchestration is mock-provider unit-tested).
// ----------------------------------------------------------------------------
namespace
{
	bool Flux_RealBuildSkinnedPose(const Flux_SkinnedPoseKey& xKey, Flux_SkinnedPoseEntry*& pxOut)
	{
		pxOut = nullptr;
		Zenith_MeshAsset* pxAsset = static_cast<Zenith_MeshAsset*>(const_cast<void*>(xKey.m_pvAsset));
		if (pxAsset == nullptr)
		{
			return false;
		}

		const u_int uNumVerts = pxAsset->GetNumVerts();
		if (uNumVerts == 0u || !pxAsset->HasSkinning())
		{
			return false;
		}

		Flux_MeshInstance* pxMesh = Flux_MeshInstance::CreateSkinnedFromAsset(pxAsset);
		if (pxMesh == nullptr)
		{
			return false;
		}

		Flux_SkinnedPoseEntry* pxEntry = new Flux_SkinnedPoseEntry();
		pxEntry->m_pxMesh        = pxMesh;
		pxEntry->m_uNumVerts     = uNumVerts;
		pxEntry->m_pvSourceAsset = pxAsset;

		// Interleave the 104B skinned vertex (uFLUX_SKIN_INPUT_WORDS=26 words/vert) via the
		// shared layout helper — the single source of the vertex layout + attribute defaults
		// (formerly hand-replicated here). The pool reads m_auBindPoseWords.GetSize(), so size
		// the word vector to its final count first, then let the helper overwrite it in place
		// (a word is the float's bit pattern, identical to the old FloatToWord path).
		const u_int uNumWords = uNumVerts * uFLUX_SKIN_INPUT_WORDS;
		pxEntry->m_auBindPoseWords.Clear();
		pxEntry->m_auBindPoseWords.Reserve(uNumWords);
		for (u_int w = 0; w < uNumWords; ++w) pxEntry->m_auBindPoseWords.PushBack(0u);
		Flux_InterleaveMeshVertices(reinterpret_cast<uint8_t*>(pxEntry->m_auBindPoseWords.GetDataPointer()),
			*pxAsset, uNumVerts, /*bSkinned*/ true);

		pxOut = pxEntry;
		return true;
	}

	void Flux_RealDestroySkinnedPose(Flux_SkinnedPoseEntry*& pxEntry)
	{
		if (pxEntry != nullptr)
		{
			if (pxEntry->m_pxMesh != nullptr)
			{
				pxEntry->m_pxMesh->Destroy();   // deferred-delete the shared IB/VB
				delete pxEntry->m_pxMesh;
			}
			delete pxEntry;
			pxEntry = nullptr;
		}
	}
}

Flux_SkinnedPoseRegistry::Provider Flux_MakeRealSkinnedPoseProvider()
{
	Flux_SkinnedPoseRegistry::Provider xProvider;
	xProvider.m_pfnBuild   = &Flux_RealBuildSkinnedPose;
	xProvider.m_pfnDestroy = &Flux_RealDestroySkinnedPose;
	return xProvider;
}
