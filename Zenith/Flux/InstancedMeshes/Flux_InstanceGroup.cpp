#include "Zenith.h"

#include "Flux/InstancedMeshes/Flux_InstanceGroup.h"
#include "Core/Zenith_Engine.h"  // g_xEngine.Threading().IsMainThread() (WS7 mutation guard)
#include "Flux/Flux_RendererImpl.h"  // g_xEngine.FluxRenderer().RequestGraphRebuild() on buffer (re)init

//=============================================================================
// VkDrawIndexedIndirectCommand structure (matches Vulkan spec)
//=============================================================================
struct Flux_DrawIndexedIndirectCommand
{
	uint32_t m_uIndexCount;
	uint32_t m_uInstanceCount;
	uint32_t m_uFirstIndex;
	int32_t m_iVertexOffset;
	uint32_t m_uFirstInstance;
};
static_assert(sizeof(Flux_DrawIndexedIndirectCommand) == 20, "DrawIndexedIndirectCommand must be 20 bytes");

//=============================================================================
// std::vector::resize(n) shim for Zenith_Vector.
//
// The CPU SoA arrays below were resized to capacity with std::vector::resize so
// arbitrary slots could be index-assigned (m_axTransforms[uID] = ...). Zenith_Vector
// has Reserve (capacity-only) but no public size-growing Resize, so this local
// helper reproduces resize(n) EXACTLY: grow by default-constructing (PushBack),
// shrink by PopBack. Every call site here only ever grows (Reserve early-returns on
// shrink; SeedInstancesForTest fills from empty), but the shrink branch keeps the
// shim a faithful std::vector::resize so behaviour is preserved regardless.
//=============================================================================
template<typename T>
static void ResizeVectorTo(Zenith_Vector<T>& xVec, uint32_t uNewSize)
{
	if (uNewSize > xVec.GetSize())
	{
		xVec.Reserve(uNewSize);  // single allocation up to target, then fill
		while (xVec.GetSize() < uNewSize)
		{
			xVec.PushBack(T{});
		}
	}
	else
	{
		while (xVec.GetSize() > uNewSize)
		{
			xVec.PopBack();
		}
	}
}

//=============================================================================
// Constructor / Destructor
//=============================================================================

Flux_InstanceGroup::Flux_InstanceGroup()
{
	// Self-wire cross-subsystem deps ONCE (de-globalisation). One boundary reach
	// per dep; every other reach in this TU routes through the member pointer.
	// The *Impl objects are allocated up-front in Zenith_Engine::Initialise() and
	// groups are only constructed at runtime, so both are non-null here.
	m_pxVulkanMemory = &g_xEngine.FluxMemory();
	m_pxThreading = &g_xEngine.Threading();
}

Flux_InstanceGroup::~Flux_InstanceGroup()
{
	DestroyGPUBuffers();
}

//=============================================================================
// Configuration
//=============================================================================

void Flux_InstanceGroup::SetMesh(Flux_MeshInstance* pxMesh)
{
	// The indirect command's static fields (incl. indexCount) are written on
	// device by the GPU reset compute pass every frame, from the CURRENT mesh's
	// index count (see Flux_InstanceReset.slang / ExecuteCullReset). So a runtime
	// mesh swap needs no host upload here — the next frame's reset picks up the
	// new index count, and there is no host write to race the prior frame's
	// in-flight indirect draw.
	m_pxMesh = pxMesh;
}

void Flux_InstanceGroup::SetMaterial(Zenith_MaterialAsset* pxMaterial)
{
	m_xMaterial.Set(pxMaterial);
}

void Flux_InstanceGroup::SetAnimationTexture(Flux_AnimationTexture* pxAnimTex)
{
	m_pxAnimationTexture = pxAnimTex;
}

void Flux_InstanceGroup::SetBounds(const Flux_InstanceBounds& xBounds)
{
	m_xBounds = xBounds;
}

//=============================================================================
// Instance Management
//=============================================================================

uint32_t Flux_InstanceGroup::AddInstance()
{
	Zenith_Assert(m_uInstanceCount < uMAX_INSTANCES, "Instance group at maximum capacity");

	uint32_t uID;

	// Reuse a recycled ID if available
	if (m_auFreeIDs.GetSize() > 0)
	{
		uID = m_auFreeIDs.GetBack();
		m_auFreeIDs.PopBack();
	}
	else
	{
		// Allocate new slot
		uID = m_uInstanceCount;
		if (uID >= m_uCapacity)
		{
			Reserve(m_uCapacity == 0 ? 1024 : m_uCapacity * 2);
		}
	}

	// Initialize instance data
	m_axTransforms.Get(uID) = glm::identity<Zenith_Maths::Matrix4>();

	Flux_InstanceAnimData xAnimData = {};
	xAnimData.m_uAnimationIndex = 0;
	xAnimData.m_uFrameCount = 1;
	xAnimData.m_fAnimTime = 0.0f;
	xAnimData.m_uColorTint = 0xFFFFFFFF;  // White, full alpha
	xAnimData.m_uFlags = 1;               // Enabled
	m_axAnimData.Get(uID) = xAnimData;

	m_abDirty.Get(uID) = true;
	m_uInstanceCount++;
	m_bTransformsDirty = true;
	m_bAnimDataDirty = true;

	return uID;
}

void Flux_InstanceGroup::RemoveInstance(uint32_t uInstanceID)
{
	Zenith_Assert(uInstanceID < m_uCapacity, "Invalid instance ID");
	Zenith_Assert(m_uInstanceCount > 0, "Cannot remove from empty group");

	// Mark as disabled (won't be culled or rendered)
	m_axAnimData.Get(uInstanceID).m_uFlags = 0;
	m_abDirty.Get(uInstanceID) = true;
	m_bAnimDataDirty = true;

	// Add to free list for reuse
	m_auFreeIDs.PushBack(uInstanceID);
	m_uInstanceCount--;
}

void Flux_InstanceGroup::Clear()
{
	m_uInstanceCount = 0;
	m_uVisibleCount = 0;
	m_auFreeIDs.Clear();

	// Mark all as dirty for next upload
	m_bTransformsDirty = true;
	m_bAnimDataDirty = true;
}

void Flux_InstanceGroup::SetInstanceTransform(uint32_t uInstanceID, const Zenith_Maths::Matrix4& xMatrix)
{
	Zenith_Assert(uInstanceID < m_uCapacity, "Invalid instance ID");
	m_axTransforms.Get(uInstanceID) = xMatrix;
	MarkDirty(uInstanceID);
	m_bTransformsDirty = true;
}

void Flux_InstanceGroup::SetInstanceAnimation(uint32_t uInstanceID, uint32_t uAnimIndex, float fNormalizedTime, uint32_t uFrameCount)
{
	Zenith_Assert(uInstanceID < m_uCapacity, "Invalid instance ID");
	Flux_InstanceAnimData& xData = m_axAnimData.Get(uInstanceID);
	xData.m_uAnimationIndex = static_cast<uint16_t>(uAnimIndex);
	xData.m_uFrameCount = static_cast<uint16_t>(uFrameCount);
	xData.m_fAnimTime = fNormalizedTime;
	// Bit 1 is the per-instance VAT-enable the G-buffer vertex shader tests
	// ((xAnimData.w & 2u) != 0u) alongside the global texture toggle. Nothing
	// ever set it, so every "animated" instance silently rendered its static
	// bind pose. Assigning an animation is the opt-in. (The culling paths
	// test bit 0 / flags!=0, so the extra bit doesn't affect visibility.)
	xData.m_uFlags |= 2u;
	MarkDirty(uInstanceID);
	m_bAnimDataDirty = true;
}

void Flux_InstanceGroup::SetInstanceColor(uint32_t uInstanceID, const Zenith_Maths::Vector4& xColor)
{
	Zenith_Assert(uInstanceID < m_uCapacity, "Invalid instance ID");
	m_axAnimData.Get(uInstanceID).m_uColorTint = PackColorRGBA8(xColor);
	MarkDirty(uInstanceID);
	m_bAnimDataDirty = true;
}

void Flux_InstanceGroup::SetInstanceEnabled(uint32_t uInstanceID, bool bEnabled)
{
	Zenith_Assert(uInstanceID < m_uCapacity, "Invalid instance ID");
	// Enable sets bit 0 while PRESERVING the VAT bit (bit 1); disable clears
	// ALL bits (matching RemoveInstance) so a disabled slot can never read as
	// live to the flags!=0 visibility bookkeeping.
	Flux_InstanceAnimData& xData = m_axAnimData.Get(uInstanceID);
	xData.m_uFlags = bEnabled ? (xData.m_uFlags | 1u) : 0u;
	MarkDirty(uInstanceID);
	m_bAnimDataDirty = true;
}

//=============================================================================
// Bulk Operations
//=============================================================================

void Flux_InstanceGroup::AdvanceAllAnimations(float fDt, float fAnimDuration)
{
	if (fAnimDuration <= 0.0f)
		return;

	float fNormalizedDt = fDt / fAnimDuration;

	for (uint32_t i = 0; i < m_uCapacity; ++i)
	{
		if (m_axAnimData.Get(i).m_uFlags != 0)  // Only active instances
		{
			float fNewTime = m_axAnimData.Get(i).m_fAnimTime + fNormalizedDt;
			m_axAnimData.Get(i).m_fAnimTime = fmod(fNewTime, 1.0f);
		}
	}

	m_bAnimDataDirty = true;
}

void Flux_InstanceGroup::Reserve(uint32_t uCapacity)
{
	if (uCapacity <= m_uCapacity)
		return;

	uCapacity = std::min(uCapacity, uMAX_INSTANCES);

	ResizeVectorTo(m_axTransforms, uCapacity);
	ResizeVectorTo(m_axAnimData, uCapacity);
	ResizeVectorTo(m_abDirty, uCapacity);

	// Initialize new slots
	for (uint32_t i = m_uCapacity; i < uCapacity; ++i)
	{
		m_axTransforms.Get(i) = glm::identity<Zenith_Maths::Matrix4>();
		m_axAnimData.Get(i) = {};
		m_abDirty.Get(i) = false;
	}

	m_uCapacity = uCapacity;

	// Recreate GPU buffers with new capacity
	if (m_bBuffersInitialised)
	{
		DestroyGPUBuffers();
	}
	InitialiseGPUBuffers();
}

//=============================================================================
// GPU Buffer Management
//=============================================================================

void Flux_InstanceGroup::InitialiseGPUBuffers()
{
	if (m_uCapacity == 0)
		return;

	// Transform + anim buffers are HOST-uploaded every frame -> frame-indexed
	// (one physical buffer per frame-in-flight). InitialiseDynamicReadWriteBuffer
	// fills all MAX_FRAMES_IN_FLIGHT slots; passing nullptr leaves them empty and
	// the per-frame UpdateGPUBuffers upload fills the current frame's slot.
	const size_t ulTransformSize = m_uCapacity * sizeof(Zenith_Maths::Matrix4);
	m_pxVulkanMemory->InitialiseDynamicReadWriteBuffer(nullptr, ulTransformSize, m_xTransformBuffer);

	const size_t ulAnimDataSize = m_uCapacity * sizeof(Flux_InstanceAnimData);
	m_pxVulkanMemory->InitialiseDynamicReadWriteBuffer(nullptr, ulAnimDataSize, m_xAnimDataBuffer);

	// Visible index buffer (worst case: all visible). GPU-written by the culling
	// compute; persistent + graph-tracked (no host upload of it on the GPU path).
	const size_t ulVisibleIndexSize = m_uCapacity * sizeof(uint32_t);
	m_pxVulkanMemory->InitialiseReadWriteBuffer(nullptr, ulVisibleIndexSize, m_xVisibleIndexBuffer);

	// Enabled-index buffer (the list of every ENABLED slot, no camera culling).
	// HOST-uploaded every frame -> frame-indexed (one physical buffer per
	// frame-in-flight), so it is NEVER graph-tracked and its host write never
	// races the prior frame's draw. Used by the CPU-fallback GBuffer draw (in
	// place of the persistent visible-index buffer the GPU-cull compute owns) and
	// by the shadow pass (which must include off-screen casters, so it cannot use
	// the camera-culled GPU output).
	const size_t ulEnabledIndexSize = m_uCapacity * sizeof(uint32_t);
	m_pxVulkanMemory->InitialiseDynamicReadWriteBuffer(nullptr, ulEnabledIndexSize, m_xEnabledIndexBuffer);

	// Bounds buffer (single bounding sphere, replicated conceptually but stored once)
	// Actually we store per-instance bounds in case we want per-instance bounds later
	const size_t ulBoundsSize = sizeof(Flux_InstanceBounds);
	m_pxVulkanMemory->InitialiseReadWriteBuffer(&m_xBounds, ulBoundsSize, m_xBoundsBuffer);

	// Indirect draw command buffer. NO host data is uploaded into it: the GPU
	// reset compute pass writes ALL five VkDrawIndexedIndirectCommand words every
	// frame (indexCount from the current mesh, the rest zeroed) and the culling
	// compute then atomically increments instanceCount[1]. This removes the old
	// host seed of the static fields, which a runtime SetMesh swap would have
	// raced against the prior frame's in-flight indirect draw.
	m_pxVulkanMemory->InitialiseIndirectBuffer(sizeof(Flux_DrawIndexedIndirectCommand), m_xIndirectBuffer);

	// Visible count buffer (single uint32 atomic counter). Seeded to 0 here; the
	// GPU reset pass zeroes it each frame thereafter (no host re-upload).
	uint32_t uZero = 0;
	m_pxVulkanMemory->InitialiseReadWriteBuffer(&uZero, sizeof(uint32_t), m_xVisibleCountBuffer);

	m_bBuffersInitialised = true;
	m_bTransformsDirty = true;
	m_bAnimDataDirty = true;

	// The persistent visible-index/count/indirect buffers are graph-tracked by
	// their (stable) Flux_Buffer address; the per-group Read/Write declarations in
	// Flux_InstancedMeshesImpl::SetupRenderGraph are gated on HasGPUBuffers(). A
	// group is typically REGISTERED (which requests a rebuild) before its first
	// instance is added — i.e. before these buffers exist — so that earlier rebuild
	// ran while HasGPUBuffers() was still false and skipped this group. The graph
	// only needs re-declaring on the FIRST init, when these buffers transition from
	// no-VRAM to valid-VRAM so SetupRenderGraph's declarations start taking effect.
	//
	// A later Reserve-driven grow re-allocates VRAM but reuses the SAME stable
	// Flux_Buffer member objects (the graph keys barriers by the C++ object
	// address, not the VRAM handle), so its declarations are already in place — a
	// rebuild then would re-declare byte-identical pointers for no benefit while
	// thrashing the (cached-barrier) graph. So only request the rebuild ONCE, on
	// first initialisation. (Group register/unregister still requests a rebuild —
	// that's a SET change, handled in Register/UnregisterInstanceGroup.)
	if (!m_bEverRequestedRebuild)
	{
		m_bEverRequestedRebuild = true;
		g_xEngine.FluxRenderer().RequestGraphRebuild();
	}

	Zenith_Log(LOG_CATEGORY_RENDERER, "[InstanceGroup] Initialised GPU buffers for %u instances", m_uCapacity);
}

void Flux_InstanceGroup::DestroyGPUBuffers()
{
	if (!m_bBuffersInitialised)
		return;

	// Queue buffers for deferred deletion. The dynamic (frame-indexed) destroy
	// helpers free every frame-in-flight slot; they no-op on already-invalid
	// slots, so an unconditional call is safe.
	m_pxVulkanMemory->DestroyDynamicReadWriteBuffer(m_xTransformBuffer);
	m_pxVulkanMemory->DestroyDynamicReadWriteBuffer(m_xAnimDataBuffer);
	m_pxVulkanMemory->DestroyDynamicReadWriteBuffer(m_xEnabledIndexBuffer);

	if (m_xVisibleIndexBuffer.GetBuffer().m_xVRAMHandle.IsValid())
		m_pxVulkanMemory->DestroyReadWriteBuffer(m_xVisibleIndexBuffer);

	if (m_xBoundsBuffer.GetBuffer().m_xVRAMHandle.IsValid())
		m_pxVulkanMemory->DestroyReadWriteBuffer(m_xBoundsBuffer);

	if (m_xIndirectBuffer.GetBuffer().m_xVRAMHandle.IsValid())
		m_pxVulkanMemory->DestroyIndirectBuffer(m_xIndirectBuffer);

	if (m_xVisibleCountBuffer.GetBuffer().m_xVRAMHandle.IsValid())
		m_pxVulkanMemory->DestroyReadWriteBuffer(m_xVisibleCountBuffer);

	m_bBuffersInitialised = false;
}

void Flux_InstanceGroup::UpdateGPUBuffers(bool bGPUCulling)
{
	// WS7: single-writer guard. Relocated to the main-thread Prepare gather; a
	// future worker-thread record-callback mutation trips immediately.
	AssertMainThreadMutation("UpdateGPUBuffers");

	// bGPUCulling no longer changes WHAT is uploaded: the transform/anim buffers
	// and the frame-indexed enabled-index buffer are uploaded every frame in both
	// modes (see below). The persistent visible-index buffer is owned by the GPU
	// cull compute on device and is never host-uploaded here. The parameter is
	// retained for the documented contract / call-site clarity.
	(void)bGPUCulling;

	if (!m_bBuffersInitialised || m_uCapacity == 0)
		return;

	// Transform + anim buffers are frame-indexed (Flux_DynamicReadWriteBuffer):
	// GetBuffer() resolves the CURRENT frame's physical buffer, so this host
	// upload only ever writes the slot the GPU isn't reading this frame — no race
	// with the prior frame's draw, and no graph tracking required.
	//
	// NOTE: a frame-indexed buffer has a distinct physical buffer per frame, so a
	// dirty flag cleared after uploading frame N's slot would wrongly skip frame
	// N+1's slot. Upload every frame regardless of the dirty flags (the data is
	// small relative to the per-frame draw cost, and the previous single-buffer
	// dirty-skip optimisation is unsafe once the buffer is frame-indexed).
	{
		const size_t ulSize = m_uCapacity * sizeof(Zenith_Maths::Matrix4);
		m_pxVulkanMemory->UploadBufferData(
			m_xTransformBuffer.GetBuffer().m_xVRAMHandle,
			m_axTransforms.GetDataPointer(),
			ulSize);
		m_bTransformsDirty = false;
	}

	{
		const size_t ulSize = m_uCapacity * sizeof(Flux_InstanceAnimData);
		m_pxVulkanMemory->UploadBufferData(
			m_xAnimDataBuffer.GetBuffer().m_xVRAMHandle,
			m_axAnimData.GetDataPointer(),
			ulSize);
		m_bAnimDataDirty = false;
	}

	// CPU bookkeeping (which slots are ENABLED + the count) is factored into
	// ComputeVisibleIndices so the headless determinism test can exercise the EXACT
	// same computation without a live allocator. The list of enabled slots is
	// uploaded EVERY frame in BOTH culling modes into the frame-indexed
	// m_xEnabledIndexBuffer:
	//   - CPU-fallback GBuffer: reads this list instead of the persistent
	//     visible-index buffer (which only the GPU-cull compute writes).
	//   - Shadows (both modes): must include OFF-screen casters, so they use this
	//     all-enabled list rather than the camera-culled GPU output.
	// The persistent visible-index buffer is NEVER host-uploaded here — on the GPU
	// path the culling compute owns it on device (a host upload would race the
	// prior frame's indirect draw read of it, the WRITE_AFTER_READ this refactor
	// removes), and on the CPU path it is unused. m_xEnabledIndexBuffer is
	// frame-indexed, so its host write is never graph-tracked and never races.
	{
		Zenith_Vector<uint32_t> auEnabledIndices;
		ComputeVisibleIndices(auEnabledIndices);

		if (auEnabledIndices.GetSize() > 0)
		{
			const size_t ulSize = static_cast<size_t>(auEnabledIndices.GetSize()) * sizeof(uint32_t);
			m_pxVulkanMemory->UploadBufferData(
				m_xEnabledIndexBuffer.GetBuffer().m_xVRAMHandle,
				auEnabledIndices.GetDataPointer(),
				ulSize);
		}

		m_uEnabledCount = auEnabledIndices.GetSize();
		m_uVisibleCount = m_uEnabledCount;
	}

	// Clear dirty flags
	for (uint32_t i = 0; i < m_uCapacity; ++i)
	{
		m_abDirty.Get(i) = false;
	}
}

void Flux_InstanceGroup::ComputeVisibleIndices(Zenith_Vector<uint32_t>& xauVisibleOut) const
{
	// Pure CPU bookkeeping — NO GPU access, NO member mutation. Walk every slot
	// up to capacity in ascending order and collect the ENABLED ones (m_uFlags
	// != 0). Deterministic: same CPU state -> same list, every run. Shared by
	// UpdateGPUBuffers (which then uploads it) and the determinism test (which
	// only hashes it).
	xauVisibleOut.Clear();
	xauVisibleOut.Reserve(m_uInstanceCount);

	for (uint32_t i = 0; i < m_uCapacity; ++i)
	{
		if (m_axAnimData.Get(i).m_uFlags != 0)
		{
			xauVisibleOut.PushBack(i);
		}
	}
}

uint64_t Flux_InstanceGroup::HashVisibleStateForTest() const
{
	// FNV-1a over m_uVisibleCount followed by the ComputeVisibleIndices list.
	// Byte-feeding methodology: no struct memcpy, so no uninitialised padding
	// ever feeds the hash.
	uint64_t uHash = 0xcbf29ce484222325ull;
	auto Bytes = [&uHash](const void* p, size_t n)
	{
		const uint8_t* pb = static_cast<const uint8_t*>(p);
		for (size_t i = 0; i < n; ++i) { uHash ^= pb[i]; uHash *= 0x100000001b3ull; }
	};

	Bytes(&m_uVisibleCount, sizeof(m_uVisibleCount));

	Zenith_Vector<uint32_t> auVisibleIndices;
	ComputeVisibleIndices(auVisibleIndices);
	const uint32_t uCount = auVisibleIndices.GetSize();
	Bytes(&uCount, sizeof(uCount));
	for (uint32_t i = 0; i < uCount; ++i)
	{
		const uint32_t uIndex = auVisibleIndices.Get(i);
		Bytes(&uIndex, sizeof(uIndex));
	}

	return uHash;
}

void Flux_InstanceGroup::SeedInstancesForTest(uint32_t uCount, uint32_t uSeed)
{
	// Device-independent: fill the CPU SoA arrays only. No GPU buffer init (the
	// allocator-backed path asserts in headless). Deterministic by (uSeed, slot).
	uCount = std::min(uCount, uMAX_INSTANCES);

	ResizeVectorTo(m_axTransforms, uCount);
	ResizeVectorTo(m_axAnimData, uCount);
	ResizeVectorTo(m_abDirty, uCount);
	m_auFreeIDs.Clear();

	for (uint32_t i = 0; i < uCount; ++i)
	{
		// A simple deterministic transform stamp (translation by index) so the
		// transform array is non-uniform; the visible-index computation itself
		// only depends on the enabled flag, but this keeps the seed realistic.
		Zenith_Maths::Matrix4 xTransform = glm::identity<Zenith_Maths::Matrix4>();
		xTransform[3][0] = static_cast<float>(i);
		m_axTransforms.Get(i) = xTransform;

		Flux_InstanceAnimData xAnimData = {};
		xAnimData.m_uAnimationIndex = 0;
		xAnimData.m_uFrameCount = 1;
		xAnimData.m_fAnimTime = 0.0f;
		xAnimData.m_uColorTint = 0xFFFFFFFF;
		// Deterministic enabled/disabled mix: a fixed LCG-style hash of (seed,index)
		// leaves roughly 3/4 of slots enabled, so ComputeVisibleIndices has a
		// non-trivial, reproducible visible set to collect. Slots 0 and 1 are pinned
		// disabled/enabled respectively so the visible subset is GUARANTEED to be
		// strictly between 0 and uCount for any seed (keeps the test's degeneracy
		// guard non-vacuous without depending on the hash distribution).
		const uint32_t uMix = (uSeed * 2654435761u) ^ (i * 40503u + 0x9E3779B9u);
		uint32_t uFlags = ((uMix & 3u) != 0u) ? 1u : 0u;
		if (i == 0) uFlags = 0u;
		else if (i == 1) uFlags = 1u;
		xAnimData.m_uFlags = uFlags;
		m_axAnimData.Get(i) = xAnimData;

		m_abDirty.Get(i) = true;
	}

	m_uCapacity = uCount;
	m_uInstanceCount = uCount;
	m_uVisibleCount = 0;
	m_bTransformsDirty = true;
	m_bAnimDataDirty = true;
	// Intentionally leave m_bBuffersInitialised false — no GPU buffers exist.
}

//=============================================================================
// Helper Functions
//=============================================================================

void Flux_InstanceGroup::AssertMainThreadMutation(const char* szWhat) const
{
	// MEMORY "Mutating:" idiom. The WS7 keystone runs all per-frame GPU-sync
	// mutation from the main-thread .Prepare gather (GatherInstancedPacket). A
	// future call from a worker-thread record callback (the latent race C1C2
	// removed) trips here. NOT a !AreRenderTasksActive check: the render-task
	// window spans both Prepare (main thread) and record (workers), so only
	// thread affinity distinguishes the legitimate single writer.
	Zenith_Assert(m_pxThreading->IsMainThread(),
		"Flux_InstanceGroup::%s must run on the main thread (WS7 Prepare gather is the single writer)", szWhat);
}

void Flux_InstanceGroup::MarkDirty(uint32_t uInstanceID)
{
	if (uInstanceID < m_uCapacity)
	{
		m_abDirty.Get(uInstanceID) = true;
	}
}

uint32_t Flux_InstanceGroup::PackColorRGBA8(const Zenith_Maths::Vector4& xColor)
{
	uint8_t uR = static_cast<uint8_t>(glm::clamp(xColor.r, 0.0f, 1.0f) * 255.0f);
	uint8_t uG = static_cast<uint8_t>(glm::clamp(xColor.g, 0.0f, 1.0f) * 255.0f);
	uint8_t uB = static_cast<uint8_t>(glm::clamp(xColor.b, 0.0f, 1.0f) * 255.0f);
	uint8_t uA = static_cast<uint8_t>(glm::clamp(xColor.a, 0.0f, 1.0f) * 255.0f);

	return (uA << 24) | (uB << 16) | (uG << 8) | uR;
}
