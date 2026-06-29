#include "Zenith.h"
#include "Flux/Flux_MaterialTable.h"

#include "Core/Zenith_Engine.h"
#include "Core/Multithreading/Zenith_Multithreading.h"  // IsMainThread() for the gather-thread contract
#include "Flux/Flux_GraphicsImpl.h"     // BindlessAllocator() + FluxMemory/FluxGraphics reach-in
#include "Flux/Flux_BackendTypes.h"     // Flux_MemoryManager full def for the neutral buffer calls
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"

void Flux_MaterialTable::Initialise()
{
	m_xRecords.assign(uFLUX_MATERIAL_TABLE_CAPACITY, Flux_MaterialGPU{});
	m_xRecordStamp.assign(uFLUX_MATERIAL_TABLE_CAPACITY, ~0ull);
	m_xRecordBindlessGen.assign(uFLUX_MATERIAL_TABLE_CAPACITY, ~0ull);
	m_xIndexAllocator.Initialise(uFLUX_MATERIAL_TABLE_CAPACITY);
	m_uMaxIndex = 0;

	// Zero-initialise the GPU buffer. Index 0 is the reserved/unused default slot —
	// never drawn (draw paths substitute the engine blank material for a null
	// material, which gets its own real index), so a zeroed record there is safe.
	const size_t uSize = static_cast<size_t>(uFLUX_MATERIAL_TABLE_CAPACITY) * sizeof(Flux_MaterialGPU);
	g_xEngine.FluxMemory().InitialiseDynamicReadWriteBuffer(m_xRecords.data(), uSize, m_xBuffer);

	m_bInitialised = true;
}

void Flux_MaterialTable::Shutdown()
{
	if (m_bInitialised)
	{
		g_xEngine.FluxMemory().DestroyDynamicReadWriteBuffer(m_xBuffer);
	}
	m_xRecords.clear();
	m_xRecordStamp.clear();
	m_xRecordBindlessGen.clear();
	m_uMaxIndex    = 0;
	m_bInitialised = false;
}

u_int Flux_MaterialTable::GetOrCreateIndex(Zenith_MaterialAsset* pxMaterial)
{
	// Contract: called only from the per-subsystem GATHER (Prepare) on the main
	// thread — it mutates the index allocator + the record mirrors and resolves
	// bindless texture slots. Workers later read the assigned index lock-free.
	Zenith_Assert(g_xEngine.Threading().IsMainThread(),
		"Flux_MaterialTable::GetOrCreateIndex: main-thread only (mutates the index allocator + writes bindless descriptors)");

	if (!pxMaterial)
	{
		return Flux_BindlessAllocator::uRESERVED_DEFAULT_WHITE;   // 0 — never drawn
	}

	const u_int   uStored      = pxMaterial->GetMaterialTableIndex();
	const u_int64 uStamp       = pxMaterial->GetEditStamp();
	const u_int64 uBindlessGen = g_xEngine.FluxGraphics().BindlessAllocator().GetGeneration();

	// Pure decision (allocate / rebuild / reuse + mirror/high-water bookkeeping).
	const Flux_MaterialSlotDecision xDecision = Flux_DecideMaterialSlot(
		m_xIndexAllocator, m_xRecordStamp, m_xRecordBindlessGen, m_uMaxIndex,
		uStored, uFLUX_INVALID_MATERIAL_INDEX, uStamp, uBindlessGen);

	if (uStored != xDecision.m_uIndex)
	{
		pxMaterial->SetMaterialTableIndex(xDecision.m_uIndex);
	}
	if (xDecision.m_bNeedsBuild)
	{
		// (Re)build re-resolves the 9 texture slots into bindless indices.
		BuildRecord(xDecision.m_uIndex, pxMaterial);
	}

	return xDecision.m_uIndex;
}

void Flux_MaterialTable::BuildRecord(u_int uIndex, Zenith_MaterialAsset* pxMaterial)
{
	// Resolve each slot's texture (falls back to the slot's pinned default) and make
	// it bindless with REPEAT addressing — material textures tile via UV transform.
	u_int auTexIdx[MATERIAL_TEXTURE_SLOT_COUNT];
	for (u_int u = 0; u < MATERIAL_TEXTURE_SLOT_COUNT; u++)
	{
		Zenith_TextureAsset* pxTex = pxMaterial->GetResolvedTexture(static_cast<MaterialTextureSlot>(u));
		if (pxTex)
		{
			pxTex->MarkAsBindless(/*bRepeatAddressing*/ true);
			auTexIdx[u] = pxTex->m_xSRV.m_uBindlessIndex;
		}
		else
		{
			// GetResolvedTexture falls back to defaults, so this is not expected; slot 0
			// (reserved default-white) is the safe sentinel.
			auTexIdx[u] = Flux_BindlessAllocator::uRESERVED_DEFAULT_WHITE;
		}
	}

	Flux_PackMaterialGPU(m_xRecords[uIndex], pxMaterial->GetResolved(), auTexIdx);
}

void Flux_MaterialTable::Upload()
{
	if (!m_bInitialised)
	{
		return;
	}
	// Whole active range each frame (host-coherent → no barrier). Cheap: a few hundred
	// 176-byte records at most.
	const size_t uActiveBytes = static_cast<size_t>(m_uMaxIndex + 1) * sizeof(Flux_MaterialGPU);
	g_xEngine.FluxMemory().UploadBufferData(m_xBuffer.GetBuffer().m_xVRAMHandle, m_xRecords.data(), uActiveBytes);
}

#include "Flux/Flux_MaterialTable.Tests.inl"

// The shared refcount-diff registry the unified-mesh registries all instantiate. Pure
// (synthetic key + mock provider), hosted here (always-linked core TU) so its static-init
// test registrations are never dead-stripped — same rationale as the GPU-scene tests below.
#include "Flux/Flux_RefcountDiffRegistry.Tests.inl"

// Stage-0 unified-mesh GPU-scene tests. Hosted here (an always-linked core TU)
// rather than in Flux_GPUScene.cpp because that TU is inert until Stage 0e
// references it — see the NOTE at the bottom of Flux_GPUScene.cpp. The test
// bodies call Flux_BuildGPUScene, which pulls Flux_GPUScene.obj into the link.
#include "Flux/Flux_GPUScene.Tests.inl"

// Stage-5 compute-skinning core tests. Hosted here for the same reason — the pure
// Flux_SkinVertexCPU mirror is header-inline and not yet referenced by engine code
// (the GPU skinning pass arrives in Stage 5d), so its static-init test registrations
// would be dead-stripped from any inert host TU. This already-linked TU keeps them live.
#include "Flux/UnifiedMesh/Flux_Skinning.Tests.inl"
