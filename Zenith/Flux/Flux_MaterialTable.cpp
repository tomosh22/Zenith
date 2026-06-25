#include "Zenith.h"
#include "Flux/Flux_MaterialTable.h"

#include "Core/Zenith_Engine.h"
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
	if (!pxMaterial)
	{
		return Flux_BindlessAllocator::uRESERVED_DEFAULT_WHITE;   // 0 — never drawn
	}

	u_int         uIndex       = pxMaterial->GetMaterialTableIndex();
	const u_int64 uStamp       = pxMaterial->GetEditStamp();
	const u_int64 uBindlessGen = g_xEngine.FluxGraphics().BindlessAllocator().GetGeneration();

	if (uIndex == uFLUX_INVALID_MATERIAL_INDEX)
	{
		uIndex = m_xIndexAllocator.Allocate();   // >= 1 (slot 0 reserved)
		pxMaterial->SetMaterialTableIndex(uIndex);
		if (uIndex > m_uMaxIndex)
		{
			m_uMaxIndex = uIndex;
		}
		BuildRecord(uIndex, pxMaterial);
		m_xRecordStamp[uIndex]       = uStamp;
		m_xRecordBindlessGen[uIndex] = uBindlessGen;
	}
	else if (m_xRecordStamp[uIndex] != uStamp || m_xRecordBindlessGen[uIndex] != uBindlessGen)
	{
		// Material edited, or a bindless texture slot was freed/reallocated since the
		// record was built → rebuild (re-resolves the texture indices).
		BuildRecord(uIndex, pxMaterial);
		m_xRecordStamp[uIndex]       = uStamp;
		m_xRecordBindlessGen[uIndex] = uBindlessGen;
	}

	return uIndex;
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
