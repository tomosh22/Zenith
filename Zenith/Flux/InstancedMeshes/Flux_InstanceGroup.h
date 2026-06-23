#pragma once

#include "Flux/Flux_Fwd.h"
#include "Flux/Flux_Buffers.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "Flux/MeshGeometry/Flux_MeshInstance.h"
#include "Collections/Zenith_Vector.h"

class Flux_AnimationTexture;
class Zenith_Multithreading;

//=============================================================================
// Per-instance animation and color data (16 bytes, GPU-aligned)
//=============================================================================
struct Flux_InstanceAnimData
{
	uint16_t m_uAnimationIndex;    // Which animation clip (0-65535)
	uint16_t m_uFrameCount;        // Frames in this animation
	float m_fAnimTime;             // 0-1 normalized time within animation
	uint32_t m_uColorTint;         // RGBA8 packed color (premultiplied alpha)
	uint32_t m_uFlags;             // Visibility/active flags
};
static_assert(sizeof(Flux_InstanceAnimData) == 16, "Flux_InstanceAnimData must be 16 bytes");

//=============================================================================
// Bounding sphere for culling (16 bytes)
//=============================================================================
struct Flux_InstanceBounds
{
	Zenith_Maths::Vector3 m_xCenter = Zenith_Maths::Vector3(0.0f);  // Local-space center
	float m_fRadius = 10.0f;                                         // Local-space radius (default is generous to avoid culling)
};
static_assert(sizeof(Flux_InstanceBounds) == 16, "Flux_InstanceBounds must be 16 bytes");

//=============================================================================
// Flux_InstanceGroup
// Manages a collection of mesh instances that share geometry and material.
// Supports GPU frustum culling and indirect drawing for 100k+ instances.
//=============================================================================
class Flux_InstanceGroup
{
public:
	static constexpr uint32_t uMAX_INSTANCES = 131072;  // 128K max instances

	Flux_InstanceGroup();
	~Flux_InstanceGroup();

	// Non-copyable (owns GPU resources)
	Flux_InstanceGroup(const Flux_InstanceGroup&) = delete;
	Flux_InstanceGroup& operator=(const Flux_InstanceGroup&) = delete;

	//-------------------------------------------------------------------------
	// Configuration (call before adding instances)
	//-------------------------------------------------------------------------
	void SetMesh(Flux_MeshInstance* pxMesh);
	void SetMaterial(Zenith_MaterialAsset* pxMaterial);
	void SetAnimationTexture(Flux_AnimationTexture* pxAnimTex);
	void SetBounds(const Flux_InstanceBounds& xBounds);

	//-------------------------------------------------------------------------
	// Instance Management
	//-------------------------------------------------------------------------

	// Add a new instance, returns instance ID (0 to MAX_INSTANCES-1)
	uint32_t AddInstance();

	// Remove an instance by ID (uses swap-and-pop, so IDs may change)
	void RemoveInstance(uint32_t uInstanceID);

	// Remove all instances
	void Clear();

	// Set transform for an instance
	void SetInstanceTransform(uint32_t uInstanceID, const Zenith_Maths::Matrix4& xMatrix);

	// Set animation state for an instance
	void SetInstanceAnimation(uint32_t uInstanceID, uint32_t uAnimIndex, float fNormalizedTime, uint32_t uFrameCount);

	// Set color tint for an instance (RGBA, 0-1 range)
	void SetInstanceColor(uint32_t uInstanceID, const Zenith_Maths::Vector4& xColor);

	// Enable/disable an instance (disabled instances are not rendered)
	void SetInstanceEnabled(uint32_t uInstanceID, bool bEnabled);

	//-------------------------------------------------------------------------
	// Bulk Operations (more efficient for large updates)
	//-------------------------------------------------------------------------

	// Advance all instance animations by dt seconds
	void AdvanceAllAnimations(float fDt, float fAnimDuration);

	// Reserve capacity for expected instance count
	void Reserve(uint32_t uCapacity);

	//-------------------------------------------------------------------------
	// Per-Frame GPU Update
	//-------------------------------------------------------------------------

	// Upload dirty instance data to GPU buffers.
	//
	// Uploaded EVERY frame in BOTH culling modes:
	//   - Transform + anim buffers (the culling compute and the GBuffer/shadow
	//     vertex shaders read them).
	//   - The frame-indexed ENABLED-index buffer (m_xEnabledIndexBuffer): the list
	//     of every enabled slot, used by the CPU-fallback GBuffer draw and the
	//     shadow pass (off-screen casters must still cast). Frame-indexed → not
	//     graph-tracked → its host write never races a prior-frame draw.
	// The PERSISTENT visible-index buffer is NEVER host-uploaded here: on the GPU
	// path the culling compute writes it on device (a host upload would race the
	// prior frame's indirect draw read of it — the WRITE_AFTER_READ this refactor
	// removes), and on the CPU path it is unused.
	// m_uEnabledCount / m_uVisibleCount are maintained from the CPU bookkeeping in
	// both modes. bGPUCulling no longer changes WHAT is uploaded (retained for the
	// call-site contract); the caller still uses it to gate the culling-constants
	// upload + stats.
	void UpdateGPUBuffers(bool bGPUCulling);

	// Device-independent CPU bookkeeping shared by UpdateGPUBuffers and the
	// headless determinism test: collect the indices of every ENABLED instance
	// (m_uFlags != 0) in ascending slot order. This is the exact CPU computation
	// that feeds the visible-index buffer upload — factored out so the upload
	// (which needs a live allocator) and the pure bookkeeping (which doesn't) can
	// be exercised independently. Does NOT touch GPU buffers and does NOT mutate
	// m_uVisibleCount; the caller owns that.
	void ComputeVisibleIndices(Zenith_Vector<uint32_t>& xauVisibleOut) const;

	//-------------------------------------------------------------------------
	// Accessors
	//-------------------------------------------------------------------------

	uint32_t GetInstanceCount() const { return m_uInstanceCount; }
	uint32_t GetVisibleCount() const { return m_uVisibleCount; }
	// Count of ENABLED instances (every slot with flags != 0), in BOTH culling
	// modes. This is the draw count for the CPU-fallback GBuffer path and the
	// shadow path (which must include off-screen casters → no camera culling).
	// Maintained by UpdateGPUBuffers from the same ComputeVisibleIndices list that
	// fills m_xEnabledIndexBuffer.
	uint32_t GetEnabledCount() const { return m_uEnabledCount; }
	bool IsEmpty() const { return m_uInstanceCount == 0; }

	// Additive test accessor (determinism / thread-safety regression). Returns an
	// FNV-1a hash over m_uVisibleCount and the ComputeVisibleIndices() list so a
	// test can assert the CPU visibility bookkeeping is byte-identical across runs
	// (zero divergence == the cross-worker race is gone). Read-only; no GPU access.
	uint64_t HashVisibleStateForTest() const;

	// Additive test-only seeding helper. Populates the CPU instance arrays directly
	// (transforms + anim data + counts) with a deterministic pattern derived purely
	// from uSeed and the slot index, WITHOUT allocating GPU buffers — so the
	// determinism/thread-safety regression can run in the HEADLESS unit suite where
	// no Vulkan allocator exists (the normal AddInstance -> InitialiseGPUBuffers path
	// asserts there). Every uSeed produces a reproducible enabled/disabled mix so the
	// visible-index computation is exercised over a non-trivial layout. Mirrors the
	// per-slot init AddInstance performs for the CPU fields it touches.
	void SeedInstancesForTest(uint32_t uCount, uint32_t uSeed);

	// Access to CPU-side transform data (for serialization)
	const Zenith_Vector<Zenith_Maths::Matrix4>& GetTransforms() const { return m_axTransforms; }

	// READ accessors consumed inside the worker-thread record callbacks
	// (ExecuteCulling / ExecuteInstancedGBuffer / RenderToShadowMap). These stay
	// lock-free: the WS7 keystone makes the main-thread Prepare gather the SINGLE
	// writer of this group's per-frame state, so by the time the record callbacks
	// run that state is frozen and every accessor is a pure read. They legitimately
	// run on multiple worker threads concurrently (the culling + GBuffer record
	// tasks dispatch in parallel), so a reader-side TryLock sentinel would
	// false-positive across those concurrent readers — the thread-safety guarantee
	// is therefore enforced on the WRITE side instead (see UpdateGPUBuffers,
	// which asserts main-thread-only via AssertMainThreadMutation).
	Flux_MeshInstance* GetMesh() const { return m_pxMesh; }
	Zenith_MaterialAsset* GetMaterial() const { return m_xMaterial.GetDirect(); }
	Flux_AnimationTexture* GetAnimationTexture() const { return m_pxAnimationTexture; }
	const Flux_InstanceBounds& GetBounds() const { return m_xBounds; }

	// GPU buffer access for rendering.
	//
	// Transform + anim buffers are HOST-uploaded every frame (UpdateGPUBuffers),
	// so they are frame-indexed (Flux_DynamicReadWriteBuffer): the upload writes
	// the current frame's physical buffer, never racing the prior frame's draw
	// read of a different physical buffer. They are NOT graph-tracked (a pointer
	// captured at SetupRenderGraph time would go stale — see Flux_Buffers.h's
	// render-graph contract). The culling/GBuffer/shadow passes bind them per
	// frame via GetUAV(), which resolves the current frame automatically.
	//
	// Visible-index / visible-count / indirect buffers are GPU-WRITTEN ONLY
	// (by the reset + culling compute) and GPU-read by the indirect draw, so
	// they stay PERSISTENT (single buffer) and ARE graph-tracked via
	// .ReadsBuffer/.WritesBuffer in SetupRenderGraph — the graph synthesises the
	// reset->cull->draw barriers. GPU-queue work is serialised across frames by
	// submission order, so a persistent GPU-only buffer has no cross-frame hazard.
	const Flux_DynamicReadWriteBuffer& GetTransformBuffer() const { return m_xTransformBuffer; }
	const Flux_DynamicReadWriteBuffer& GetAnimDataBuffer() const { return m_xAnimDataBuffer; }
	const Flux_ReadWriteBuffer& GetVisibleIndexBuffer() const { return m_xVisibleIndexBuffer; }
	const Flux_ReadWriteBuffer& GetBoundsBuffer() const { return m_xBoundsBuffer; }
	const Flux_IndirectBuffer& GetIndirectBuffer() const { return m_xIndirectBuffer; }
	const Flux_ReadWriteBuffer& GetVisibleCountBuffer() const { return m_xVisibleCountBuffer; }
	// All-enabled index list, frame-indexed (host-uploaded every frame, NOT
	// graph-tracked). The CPU-fallback GBuffer and the shadow pass bind its SRV
	// (StructuredBuffer<uint>); never bind it via the graph's ReadBuffer/WriteBuffer.
	const Flux_DynamicReadWriteBuffer& GetEnabledIndexBuffer() const { return m_xEnabledIndexBuffer; }

	Flux_DynamicReadWriteBuffer& GetTransformBuffer() { return m_xTransformBuffer; }
	Flux_DynamicReadWriteBuffer& GetAnimDataBuffer() { return m_xAnimDataBuffer; }
	Flux_ReadWriteBuffer& GetVisibleIndexBuffer() { return m_xVisibleIndexBuffer; }
	Flux_ReadWriteBuffer& GetBoundsBuffer() { return m_xBoundsBuffer; }
	Flux_IndirectBuffer& GetIndirectBuffer() { return m_xIndirectBuffer; }
	Flux_ReadWriteBuffer& GetVisibleCountBuffer() { return m_xVisibleCountBuffer; }
	Flux_DynamicReadWriteBuffer& GetEnabledIndexBuffer() { return m_xEnabledIndexBuffer; }

	// True once InitialiseGPUBuffers has run for this group (capacity > 0). The
	// SetupRenderGraph per-group declaration loop and the execute callbacks skip
	// groups whose buffers don't exist yet so a freshly-created-but-empty group
	// (declared after a RequestGraphRebuild) never binds an invalid handle.
	bool HasGPUBuffers() const { return m_bBuffersInitialised; }

private:
	//-------------------------------------------------------------------------
	// Helper functions
	//-------------------------------------------------------------------------
	void InitialiseGPUBuffers();
	void DestroyGPUBuffers();
	void MarkDirty(uint32_t uInstanceID);

	// Thread-safety sentinel for the per-frame GPU-sync mutator (UpdateGPUBuffers).
	// The WS7 keystone makes the main-thread .Prepare gather the
	// SINGLE writer of this state; record callbacks are pure readers. Asserting
	// main-thread-only here means a FUTURE attempt to mutate from a worker-thread
	// record callback (the latent race C1C2 removed) trips immediately. This is the
	// MEMORY "Mutating:" idiom — preferred over a reader-side TryLock because the
	// culling + GBuffer record passes read concurrently and would contend a reader
	// lock. Note the render-task-active window spans BOTH Prepare and record, so the
	// guard is thread-affinity (IsMainThread), NOT a !AreRenderTasksActive check.
	void AssertMainThreadMutation(const char* szWhat) const;

	static uint32_t PackColorRGBA8(const Zenith_Maths::Vector4& xColor);

	//-------------------------------------------------------------------------
	// CPU-side instance data (Structure of Arrays for cache efficiency)
	//-------------------------------------------------------------------------
	Zenith_Vector<Zenith_Maths::Matrix4> m_axTransforms;
	Zenith_Vector<Flux_InstanceAnimData> m_axAnimData;
	Zenith_Vector<bool> m_abDirty;            // Per-instance dirty flags
	Zenith_Vector<uint32_t> m_auFreeIDs;      // Recycled instance IDs

	uint32_t m_uInstanceCount = 0;
	uint32_t m_uVisibleCount = 0;
	// Count of enabled slots (flags != 0), recomputed every frame in
	// UpdateGPUBuffers alongside m_xEnabledIndexBuffer. Draw count for the
	// CPU-fallback GBuffer + the shadow pass.
	uint32_t m_uEnabledCount = 0;
	uint32_t m_uCapacity = 0;
	bool m_bBuffersInitialised = false;
	// True once this group has requested a render-graph rebuild (on its FIRST
	// GPU-buffer init, when the persistent buffers transition from no-VRAM to
	// valid-VRAM and SetupRenderGraph's declarations start taking effect). A later
	// Reserve-driven grow reuses the SAME stable Flux_Buffer objects (the graph
	// keys barriers by C++ object address, not VRAM handle), so it needs NO
	// rebuild — see InitialiseGPUBuffers. Prevents per-grow rebuild thrash.
	bool m_bEverRequestedRebuild = false;
	bool m_bTransformsDirty = false;
	bool m_bAnimDataDirty = false;

	//-------------------------------------------------------------------------
	// GPU Buffers
	//-------------------------------------------------------------------------
	// Host-uploaded every frame -> frame-indexed (see accessor comment above).
	Flux_DynamicReadWriteBuffer m_xTransformBuffer;  // mat4[] - per-instance transforms
	Flux_DynamicReadWriteBuffer m_xAnimDataBuffer;   // Flux_InstanceAnimData[]
	// All-enabled slot indices, host-uploaded every frame -> frame-indexed (NOT
	// graph-tracked). Feeds the CPU-fallback GBuffer draw and the shadow pass.
	Flux_DynamicReadWriteBuffer m_xEnabledIndexBuffer; // uint32[] - indices of enabled instances
	// GPU-written/read only -> persistent + graph-tracked.
	Flux_ReadWriteBuffer m_xVisibleIndexBuffer;   // uint32[] - indices of visible instances
	Flux_ReadWriteBuffer m_xBoundsBuffer;         // vec4[] - bounding spheres for culling (constant)
	Flux_IndirectBuffer m_xIndirectBuffer;        // VkDrawIndexedIndirectCommand
	Flux_ReadWriteBuffer m_xVisibleCountBuffer;   // uint32 - atomic counter for culling

	//-------------------------------------------------------------------------
	// References — m_xMaterial keeps the asset alive via handle ref counting.
	// m_pxMesh and m_pxAnimationTexture are still raw because their lifetimes
	// are managed by their owning subsystems (mesh manager, animation system),
	// not by the asset registry.
	//-------------------------------------------------------------------------
	Flux_MeshInstance* m_pxMesh = nullptr;
	MaterialHandle m_xMaterial;
	Flux_AnimationTexture* m_pxAnimationTexture = nullptr;
	Flux_InstanceBounds m_xBounds = {};

	//-------------------------------------------------------------------------
	// Self-wired cross-subsystem deps (de-globalisation). Flux_InstanceGroup is
	// NOT an engine singleton — it is heap-allocated per Zenith_InstancedMeshComponent
	// and has no engine-composition-root Initialise() seam to inject through, and
	// its constructor's caller (the component) is off-limits. So instead of an
	// Initialise(...) signature change + call-site wiring, recover each dep ONCE in
	// the constructor (one boundary reach per dep) and route every other reach
	// through the member. Safe regardless of init ordering: the *Impl objects are
	// allocated up-front in Zenith_Engine::Initialise(), groups are only ever
	// constructed long after boot (first instance added at runtime), and we only
	// store the pointer here — no method is called at construction time.
	//-------------------------------------------------------------------------
	Flux_MemoryManager* m_pxVulkanMemory = nullptr;
	Zenith_Multithreading* m_pxThreading = nullptr;
};
