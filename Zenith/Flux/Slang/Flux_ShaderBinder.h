#pragma once

#include "Flux/Slang/Flux_SlangCompiler.h"
#include "Flux/Flux_BackendTypes.h"

// Helper class for binding shader resources by name. Looks up the binding
// slot via the shader's reflection on every call; an internal pointer-identity
// cache (NAME_CACHE_SIZE entries, no hashing) absorbs the lookup cost when
// callers pass string literals. The resource-type assertion catches Bind*
// mix-ups (e.g. BindCBV called on a binding that reflection says is a
// texture) at the call site, which is far clearer than the eventual Vulkan
// validation-layer error.
//
// Usage:
//   Flux_ShaderBinder xBinder(*pxCmdBuf);
//   xBinder.BindCBV(g_xShader, "FrameConstants", &buffer.GetCBV());
//   xBinder.BindSRV(g_xShader, "g_xDiffuseTex", &texture.GetSRV());
//   xBinder.BindDrawConstants(g_xShader, "pushConstants", &xData, sizeof(xData));
//
// Cache scope: the cache is PER-INSTANCE. A new binder is constructed at the
// top of each pass's record callback, so each binder starts with an empty
// cache. The cache's real value shows up when a single pass performs many
// draws that repeatedly call BindDrawConstants / BindCBV with the same
// (shader, name-literal): the first call inside the pass is a miss, every
// subsequent call within the same pass hits. Cross-pass / cross-frame hits
// are NOT provided — each pass resolves names once.
//
// String-literal deduplication: pointer identity requires the compiler /
// linker to give the same string literal a single address at each call
// site. MSVC (/OPT:ICF) and GCC (-fmerge-constants) do this within a
// translation unit; the cache is therefore reliable when a pass's record
// callback lives in one TU. Cross-TU deduplication is implementation-
// defined — a miss on a duplicate literal is a perf event, not a
// correctness bug (the fallback reflection lookup still returns the right
// binding).
class Flux_ShaderBinder
{
public:
	Flux_ShaderBinder(Flux_CommandBuffer& xCmdBuf);

	// Bind a constant-buffer view to a uniform-buffer binding looked up by
	// name on xShader's reflection. Asserts the reflected binding type is
	// BINDING_TYPE_BUFFER.
	void BindCBV          (const Flux_Shader& xShader, const char* szName, const Flux_ConstantBufferView*           pxCBV);

	// Bind a sampled texture (SRV) to a texture binding. Asserts the reflected
	// binding type is BINDING_TYPE_TEXTURE. pxSampler is optional (uses a
	// default sampler when null, per backend convention).
	void BindSRV          (const Flux_Shader& xShader, const char* szName, const Flux_ShaderResourceView*           pxSRV, Flux_Sampler* pxSampler = nullptr);

	// Bind a storage-image UAV. Asserts the reflected type is BINDING_TYPE_STORAGE_IMAGE.
	void BindUAV_Texture  (const Flux_Shader& xShader, const char* szName, const Flux_UnorderedAccessView_Texture*  pxUAV);

	// Bind a storage-buffer UAV. Asserts the reflected type is BINDING_TYPE_STORAGE_BUFFER.
	void BindUAV_Buffer   (const Flux_Shader& xShader, const char* szName, const Flux_UnorderedAccessView_Buffer*   pxUAV);

	// Bind a read-only structured-buffer SSBO (StructuredBuffer<T> in Slang).
	// Asserts the reflected binding type is BINDING_TYPE_STORAGE_BUFFER. Calls
	// AssertBoundResourceDeclared with bIsWrite=false so the graph rejects a
	// missing RESOURCE_ACCESS_READ_BUFFER_SRV declaration — the read/write
	// distinction is enforced at the graph-access layer, since the underlying
	// Vulkan descriptor (eStorageBuffer) is shared with the read-write path.
	void BindSRV_Buffer   (const Flux_Shader& xShader, const char* szName, const Flux_ShaderResourceView_Buffer&    xSRV);

	// Push small inline constants via the per-frame scratch UBO system. The
	// scratch slot is identified by a binding name in xShader's reflection
	// (typically "pushConstants" or similar). Asserts the reflected type is
	// BINDING_TYPE_BUFFER (the per-frame UBO is a uniform buffer slot).
	void BindDrawConstants(const Flux_Shader& xShader, const char* szName, const void* pData, u_int uSize);

private:
	// Resolve (reflection, name) to a handle and reflected type via the
	// pointer-identity cache. Asserts inside Flux_ShaderReflection::GetBinding
	// if the name is not present. Takes the reflection pointer directly (not
	// the shader) so unit tests can exercise the resolver with a synthetic
	// reflection — Flux_Shader itself needs a live Vulkan device to construct.
	struct ResolvedBinding
	{
		Flux_BindingHandle m_xHandle;
		BindingType        m_eType = BINDING_TYPE_MAX;
	};
	ResolvedBinding ResolveNamedBinding(const Flux_ShaderReflection* pxReflection, const char* szName);

	// Builds the Flux_BindingSlot for a resolved (set, binding), flagging it to
	// reset the group when the descriptor set changes from the previous bind --
	// the slot-carried replacement for the old EnsureSet/BeginBind clear.
	Flux_BindingSlot MakeSlot(u_int uSet, u_int uBinding);

	Flux_CommandBuffer& m_xCmdBuf;
	u_int m_uCurrentSet = UINT32_MAX;

	// Pointer-identity name cache. Entries are matched by pointer compare on
	// (reflection-ptr, name-ptr) — no hashing, so cannot produce a false hit.
	// Cache hits require the compiler/linker to deduplicate identical string
	// literals at the call site; MSVC (/OPT:ICF) and GCC (-fmerge-constants)
	// do this within a TU, and typically across TUs at link time, but the
	// guarantee is implementation-defined. A miss falls back to full reflection
	// lookup which still returns correct data — cross-TU misses are a perf
	// issue, not a correctness issue.
	//
	// Size is set above the largest binding count any known pass currently
	// touches, with headroom for reshuffles. The overflow assert in
	// ResolveNamedBinding trips if a binder is asked to resolve more unique
	// pairs than NAME_CACHE_SIZE — bump the constant rather than ship a
	// thrashing cache.
	//
	// Current worst case: deferred lighting pass binds ~26 unique names (frame
	// constants + 4 GBuffer SRVs incl. the emissive MRT + depth SRV + 4 CSM
	// SRVs + 4 ShadowMatrix CBVs + 3 IBL SRVs + SSR + SSGI + cluster buffers
	// + deferred-shading constants + draw constants).
	// 64 gives >50% headroom for future passes or bind reshuffles; bumping from
	// the original 32 because the refactored deferred path sat at 78% capacity
	// one pass away from overflowing the assert.
	struct NameCacheEntry
	{
		const Flux_ShaderReflection* m_pxReflection = nullptr;
		const char*                  m_szName       = nullptr;
		Flux_BindingHandle           m_xHandle;
		BindingType                  m_eType        = BINDING_TYPE_MAX;
	};
	static constexpr u_int NAME_CACHE_SIZE = 64;
	// Floor below which any single known pass's bind count would push the cache
	// into thrash-via-eviction territory. Tracks the real worst-case pass so a
	// future reduction of NAME_CACHE_SIZE fails the build instead of silently
	// regressing hot-path performance.
	static constexpr u_int NAME_CACHE_WORST_CASE_PASS_BINDINGS = 26;
	static_assert(NAME_CACHE_SIZE >= NAME_CACHE_WORST_CASE_PASS_BINDINGS * 2,
		"Flux_ShaderBinder NAME_CACHE_SIZE must leave at least 2x headroom over the worst-case pass — bump the constant.");
	NameCacheEntry m_axNameCache[NAME_CACHE_SIZE];
	u_int          m_uNextCacheSlot = 0;
	// Counts distinct (reflection, name) pairs resolved on this binder. The
	// overflow assert in ResolveNamedBinding trips when the count would push
	// past NAME_CACHE_SIZE, turning "cache thrashes silently under too many
	// unique bindings" into a loud failure with the offending name.
	u_int          m_uUniqueResolves = 0;

	// Unit tests poke at m_axNameCache and call ResolveNamedBinding directly
	// with a synthetic Flux_ShaderReflection (no live Vulkan device required).
	friend class Zenith_UnitTests;
};
