#pragma once

#include "Flux/Slang/Flux_SlangCompiler.h"
#include "Flux/Flux_BackendTypes.h"

// Helper class for binding shader resources via typed binding handles. Each Bind*
// takes a generated `Flux_BindingHandle` (set / binding / kind / count, emitted into
// Flux/Shaders/Generated/<Feature>.h by the codegen) — there is no name lookup and no
// runtime name cache (both were removed in the binding-model overhaul). A debug
// resource-kind assertion catches Bind* mix-ups (e.g. BindCBV on a handle reflection
// says is a texture) at the call site, which is far clearer than the eventual Vulkan
// validation-layer error.
//
// Usage:
//   Flux_ShaderBinder xBinder(*pxCmdBuf);
//   namespace DS = Flux_Generated_DeferredShading::DeferredShading;
//   xBinder.BindCBV(DS::hg_xView, &buffer.GetCBV());
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

	// ---- Typed-handle overloads (compile-time resolved; no name lookup) ----
	// Each takes a generated Flux_BindingHandle (set + binding + kind + count,
	// emitted per binding into Flux/Shaders/Generated/<Subsystem>.h). The kind
	// carried by the handle is asserted against the overload, so a Bind* mix-up
	// still fails loudly at the call site — without any reflection lookup or the
	// per-instance name cache. These are the post-migration binding path.
	void BindCBV          (const Flux_BindingHandle& xHandle, const Flux_ConstantBufferView*          pxCBV);
	void BindSRV          (const Flux_BindingHandle& xHandle, const Flux_ShaderResourceView*          pxSRV, Flux_Sampler* pxSampler = nullptr);
	void BindUAV_Texture  (const Flux_BindingHandle& xHandle, const Flux_UnorderedAccessView_Texture* pxUAV);
	void BindUAV_Buffer   (const Flux_BindingHandle& xHandle, const Flux_UnorderedAccessView_Buffer*  pxUAV);
	void BindSRV_Buffer   (const Flux_BindingHandle& xHandle, const Flux_ShaderResourceView_Buffer&   xSRV);
	void BindDrawConstants(const Flux_BindingHandle& xHandle, const void* pData, u_int uSize);

private:
	// Resolve (reflection, name) to a handle and resource kind via a direct
	// reflection lookup (no cache — the 64-entry pointer-identity name cache was
	// removed with the binding-model overhaul). Asserts inside
	// Flux_ShaderReflection::GetBinding if the name is not present. Takes the
	// reflection pointer directly (not the shader) so unit tests can exercise the
	// resolver with a synthetic reflection — Flux_Shader itself needs a live
	// Vulkan device to construct. The name-based Bind* overloads remain for
	// out-of-tree (game) shaders that have no generated typed-handle header;
	// engine code binds through the compile-time Flux_BindingHandle overloads.
	struct ResolvedBinding
	{
		Flux_BindingHandle m_xHandle;
		FluxResourceKind   m_eKind = FLUX_RESOURCE_KIND_UNKNOWN;
	};
	ResolvedBinding ResolveNamedBinding(const Flux_ShaderReflection* pxReflection, const char* szName);

	// Builds the Flux_BindingSlot for a resolved (set, binding), flagging it to
	// reset the group when the descriptor set changes from the previous bind --
	// the slot-carried replacement for the old EnsureSet/BeginBind clear.
	Flux_BindingSlot MakeSlot(u_int uSet, u_int uBinding);

	Flux_CommandBuffer& m_xCmdBuf;
	u_int m_uCurrentSet = UINT32_MAX;

	// Unit tests call ResolveNamedBinding directly with a synthetic
	// Flux_ShaderReflection (no live Vulkan device required).
	friend class Zenith_UnitTests;
};
