#pragma once

#include "Flux/Flux_Buffers.h"            // Flux_DynamicReadWriteBuffer (frame-indexed, graph-invisible)
#include "Flux/Flux_BindlessAllocator.h"  // dense index allocator (slot 0 reserved)
#include "Flux/Flux_MaterialGPU.h"        // Flux_MaterialGPU record
#include <vector>

class Zenith_MaterialAsset;

// Capacity of the GPU material table (distinct material records). Generous — far
// beyond any realistic scene's distinct-material count; exhaustion asserts loudly
// in the index allocator. (Not grown dynamically: a fixed cap keeps the per-frame
// upload + buffer lifetime trivial, and 4096 distinct materials is unreachable in
// practice.)
inline constexpr u_int uFLUX_MATERIAL_TABLE_CAPACITY = 4096u;

// ============================================================================
// Flux_MaterialTable — the GPU material record store.
//
// Holds one Flux_MaterialGPU per distinct material in a per-frame-in-flight,
// host-visible+coherent StructuredBuffer bound as g_axMaterials at the GLOBAL set
// (Common/Bindings.slang). A draw selects its record by index
// (MeshDrawConstants::m_uMaterialIndex). The 9 texture slots of each record are
// bindless indices into g_axTextures[] (set 2) — material textures are made
// bindless (with repeat addressing) when their record is built.
//
// Lifecycle per frame:
//   - GetOrCreateIndex(material) is called from each material subsystem's GATHER
//     (Prepare, MAIN THREAD) for every material it will draw — assigns a stable
//     index + (re)builds the record when the material edit-stamp or the global
//     bindless generation changed. Worker record paths then just read the index
//     off the material asset (lock-free).
//   - Upload() copies the active records into the current frame buffer ONCE, on
//     the main thread, after all gathers and before any pass records
//     (Flux_RendererImpl::RecordFrame). Frame-indexed + host-coherent → no graph
//     barrier (see the Flux_FrameIndexedBufferBase render-graph contract).
// ============================================================================
class Flux_MaterialTable
{
public:
	void Initialise();
	void Shutdown();

	// MAIN THREAD ONLY (mutates the index allocator + writes bindless descriptors).
	// Returns the reserved slot 0 for a null material (never drawn — draw paths
	// substitute the engine blank material).
	u_int GetOrCreateIndex(Zenith_MaterialAsset* pxMaterial);

	// Upload the active record range into the current frame-in-flight buffer.
	void Upload();

	// SRV to bind as g_axMaterials (GLOBAL set, current frame's view).
	const Flux_ShaderResourceView_Buffer& GetSRV() const { return m_xBuffer.GetSRV(); }

private:
	void BuildRecord(u_int uIndex, Zenith_MaterialAsset* pxMaterial);

	Flux_DynamicReadWriteBuffer   m_xBuffer;
	Flux_BindlessAllocator        m_xIndexAllocator;
	std::vector<Flux_MaterialGPU> m_xRecords;            // CPU mirror, indexed by table index
	std::vector<u_int64>          m_xRecordStamp;        // material edit-stamp at last build (per index)
	std::vector<u_int64>          m_xRecordBindlessGen;  // bindless generation at last build (per index)
	u_int                         m_uMaxIndex    = 0;    // high-water of assigned indices (upload range)
	bool                          m_bInitialised = false;
};
