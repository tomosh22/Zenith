#include "Zenith.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/Flux_CommandList.h"
#include "Flux/Flux.h" // for Flux_ShaderResourceView / Flux_UnorderedAccessView_* field access
#include "Flux/RenderGraph/Flux_RenderGraph.h" // for render-graph bind-time assertions

Flux_ShaderBinder::Flux_ShaderBinder(Flux_CommandList& xCmdList)
	: m_xCmdList(xCmdList)
	, m_uCurrentSet(UINT32_MAX)
{
}

void Flux_ShaderBinder::EnsureSet(u_int uSet)
{
	if (m_uCurrentSet != uSet)
	{
		m_xCmdList.AddCommand<Flux_CommandBeginBind>(uSet);
		m_uCurrentSet = uSet;
	}
}

// Pointer-identity cache contract: entries are matched by pointer compare on
// (reflection-ptr, name-ptr). Cross-TU cache hits depend on the linker merging
// identical string literals (MSVC /OPT:ICF, GCC -fmerge-constants) — within a
// TU the typical caller pattern produces a 100% hit after the first call. A
// miss falls back to full reflection lookup which still returns correct data.
// A cache hit is a linear scan of 32 pointer pairs (~64 cycles, a few cache
// lines).
//
// The overflow assert fires when a single binder instance resolves more unique
// (reflection, name) pairs than NAME_CACHE_SIZE. Bump the constant if that
// ever fires legitimately; the assertion exists to turn silent cache-thrash
// into a loud failure with the offending name.
Flux_ShaderBinder::ResolvedBinding Flux_ShaderBinder::ResolveNamedBinding(const Flux_ShaderReflection* pxReflection, const char* szName)
{
	Zenith_Assert(pxReflection != nullptr, "Flux_ShaderBinder::ResolveNamedBinding: null reflection");

	// Cache lookup — pointer identity only, no hashing.
	for (u_int u = 0; u < NAME_CACHE_SIZE; u++)
	{
		const NameCacheEntry& xEntry = m_axNameCache[u];
		if (xEntry.m_pxReflection == pxReflection && xEntry.m_szName == szName)
		{
			ResolvedBinding xResult;
			xResult.m_xHandle = xEntry.m_xHandle;
			xResult.m_eType   = xEntry.m_eType;
			return xResult;
		}
	}

	// Cache miss — a new unique (reflection, name) pair. Assert the binder
	// hasn't resolved more unique pairs than the cache can hold before
	// thrashing — the offending name is the one that pushed us over.
	// Debug builds trip immediately; release/tools builds degrade gracefully
	// to round-robin eviction (still correct, just paying reflection lookups
	// on thrash), but we log one warning per overflow boundary so the perf
	// regression is at least visible in the log stream.
	Zenith_Assert(m_uUniqueResolves < NAME_CACHE_SIZE,
		"Flux_ShaderBinder: cache overflow resolving '%s' — binder has seen %u unique (reflection, name) pairs but NAME_CACHE_SIZE is %u. Bump NAME_CACHE_SIZE in Flux_ShaderBinder.h.",
		szName, m_uUniqueResolves, static_cast<u_int>(NAME_CACHE_SIZE));
	if (m_uUniqueResolves == NAME_CACHE_SIZE)
	{
		Zenith_Warning(LOG_CATEGORY_RENDERER,
			"Flux_ShaderBinder: cache at capacity (%u entries) — '%s' will force round-robin eviction, subsequent binds on this binder pay full reflection lookups. Bump NAME_CACHE_SIZE in Flux_ShaderBinder.h.",
			static_cast<u_int>(NAME_CACHE_SIZE), szName);
	}
	m_uUniqueResolves++;

	// Single O(1) lookup returning the full reflected binding.
	const Flux_ReflectedBinding* pxReflected = pxReflection->GetBinding(szName);
	Zenith_Assert(pxReflected != nullptr,
		"Flux_ShaderBinder::ResolveNamedBinding: '%s' not found in reflection",
		szName);

	Flux_BindingHandle xHandle;
	BindingType eType = BINDING_TYPE_MAX;
	if (pxReflected)
	{
		xHandle.m_uSet     = pxReflected->m_uSet;
		xHandle.m_uBinding = pxReflected->m_uBinding;
		eType              = pxReflected->m_eType;
	}

	NameCacheEntry& xSlot = m_axNameCache[m_uNextCacheSlot];
	xSlot.m_pxReflection = pxReflection;
	xSlot.m_szName       = szName;
	xSlot.m_xHandle      = xHandle;
	xSlot.m_eType        = eType;
	m_uNextCacheSlot = (m_uNextCacheSlot + 1) % NAME_CACHE_SIZE;

	ResolvedBinding xResult;
	xResult.m_xHandle = xHandle;
	xResult.m_eType   = eType;
	return xResult;
}

void Flux_ShaderBinder::BindCBV(const Flux_Shader& xShader, const char* szName, const Flux_ConstantBufferView* pxCBV)
{
	const ResolvedBinding xResolved = ResolveNamedBinding(&xShader.GetReflection(), szName);
	Zenith_Assert(xResolved.m_eType == BINDING_TYPE_BUFFER,
		"Flux_ShaderBinder::BindCBV: binding '%s' has reflected type %d, expected BINDING_TYPE_BUFFER (%d). Wrong Bind* overload for this binding?",
		szName, static_cast<int>(xResolved.m_eType), static_cast<int>(BINDING_TYPE_BUFFER));

	EnsureSet(xResolved.m_xHandle.m_uSet);
	m_xCmdList.AddCommand<Flux_CommandBindCBV>(pxCBV, xResolved.m_xHandle.m_uBinding);
}

void Flux_ShaderBinder::BindSRV(const Flux_Shader& xShader, const char* szName, const Flux_ShaderResourceView* pxSRV, Flux_Sampler* pxSampler)
{
	const ResolvedBinding xResolved = ResolveNamedBinding(&xShader.GetReflection(), szName);
	Zenith_Assert(xResolved.m_eType == BINDING_TYPE_TEXTURE,
		"Flux_ShaderBinder::BindSRV: binding '%s' has reflected type %d, expected BINDING_TYPE_TEXTURE (%d). Wrong Bind* overload for this binding?",
		szName, static_cast<int>(xResolved.m_eType), static_cast<int>(BINDING_TYPE_TEXTURE));
	Zenith_Assert(pxSRV != nullptr, "Flux_ShaderBinder::BindSRV: null SRV pointer for binding '%s'", szName);

	// Cross-reference against the current render-graph pass (if we're inside a
	// pfnOnRecord callback). Catches "bound a resource I didn't declare with
	// Read()" — the exact class of bug that leaves the graph unable to emit
	// the correct layout transition.
	Flux_RenderGraph::AssertBoundResourceDeclared(pxSRV->m_xVRAMHandle, /*bIsWrite*/false, "BindSRV");

	EnsureSet(xResolved.m_xHandle.m_uSet);
	m_xCmdList.AddCommand<Flux_CommandBindSRV>(pxSRV, xResolved.m_xHandle.m_uBinding, pxSampler);
}

void Flux_ShaderBinder::BindUAV_Texture(const Flux_Shader& xShader, const char* szName, const Flux_UnorderedAccessView_Texture* pxUAV)
{
	const ResolvedBinding xResolved = ResolveNamedBinding(&xShader.GetReflection(), szName);
	Zenith_Assert(xResolved.m_eType == BINDING_TYPE_STORAGE_IMAGE,
		"Flux_ShaderBinder::BindUAV_Texture: binding '%s' has reflected type %d, expected BINDING_TYPE_STORAGE_IMAGE (%d). Wrong Bind* overload for this binding?",
		szName, static_cast<int>(xResolved.m_eType), static_cast<int>(BINDING_TYPE_STORAGE_IMAGE));
	Zenith_Assert(pxUAV != nullptr, "Flux_ShaderBinder::BindUAV_Texture: null UAV pointer for binding '%s'", szName);
	Flux_RenderGraph::AssertBoundResourceDeclared(pxUAV->m_xVRAMHandle, /*bIsWrite*/true, "BindUAV_Texture");

	EnsureSet(xResolved.m_xHandle.m_uSet);
	m_xCmdList.AddCommand<Flux_CommandBindUAV_Texture>(pxUAV, xResolved.m_xHandle.m_uBinding);
}

void Flux_ShaderBinder::BindUAV_Buffer(const Flux_Shader& xShader, const char* szName, const Flux_UnorderedAccessView_Buffer* pxUAV)
{
	const ResolvedBinding xResolved = ResolveNamedBinding(&xShader.GetReflection(), szName);
	Zenith_Assert(xResolved.m_eType == BINDING_TYPE_STORAGE_BUFFER,
		"Flux_ShaderBinder::BindUAV_Buffer: binding '%s' has reflected type %d, expected BINDING_TYPE_STORAGE_BUFFER (%d). Wrong Bind* overload for this binding?",
		szName, static_cast<int>(xResolved.m_eType), static_cast<int>(BINDING_TYPE_STORAGE_BUFFER));
	Zenith_Assert(pxUAV != nullptr, "Flux_ShaderBinder::BindUAV_Buffer: null UAV pointer for binding '%s'", szName);
	Flux_RenderGraph::AssertBoundResourceDeclared(pxUAV->m_xVRAMHandle, /*bIsWrite*/true, "BindUAV_Buffer");

	EnsureSet(xResolved.m_xHandle.m_uSet);
	m_xCmdList.AddCommand<Flux_CommandBindUAV_Buffer>(pxUAV, xResolved.m_xHandle.m_uBinding);
}

void Flux_ShaderBinder::BindDrawConstants(const Flux_Shader& xShader, const char* szName, const void* pData, u_int uSize)
{
	const ResolvedBinding xResolved = ResolveNamedBinding(&xShader.GetReflection(), szName);
	// The per-frame scratch UBO is reflected as BINDING_TYPE_BUFFER (it's a
	// uniform buffer slot the runtime fills with push-constant-equivalent data).
	Zenith_Assert(xResolved.m_eType == BINDING_TYPE_BUFFER,
		"Flux_ShaderBinder::BindDrawConstants: binding '%s' has reflected type %d, expected BINDING_TYPE_BUFFER (%d). Wrong Bind* overload for this binding?",
		szName, static_cast<int>(xResolved.m_eType), static_cast<int>(BINDING_TYPE_BUFFER));

	EnsureSet(xResolved.m_xHandle.m_uSet);
	m_xCmdList.AddCommand<Flux_CommandBindDrawConstants>(pData, uSize, xResolved.m_xHandle.m_uBinding);
}
