#pragma once

#include "Core/ZenithConfig.h"                 // FLUX_MAX_SPEC_CONSTANTS
#include "Flux/Slang/Flux_SlangCompiler.h"     // Flux_SpecConstantHandle, Flux_ShaderReflection
#include <cstring>                             // strcmp

// ============================================================================
// Specialization-constant value table + resolver (Flux Shader System Overhaul —
// Stage 3a). A feature stages the spec-constant VALUES it wants baked into a
// pipeline variant into a Flux_SpecConstantTable (a member of
// Flux_PipelineSpecification) by NAME — resolved to the shader's runtime
// constant_id via reflection at pipeline-build. Keeping the table name-keyed (not
// id-keyed) means a shader hot-reload that renumbers spec-constant IDs stays
// correct with no rebuild of the C++ side.
//
// Stage 3a lands this plumbing with EMPTY tables everywhere (no [SpecializationConstant]
// shader decls exist yet), so every resolve is a no-op and pipeline creation is
// byte-identical to today. Stage 3b adds the shader decls + non-empty tables.
// ============================================================================

// One staged spec-constant value, keyed by the string-literal name from the
// codegen handle (stable pointer identity — the literal has static duration).
struct Flux_SpecConstantEntry
{
	const char* m_szName           = nullptr;
	u_int       m_uValue           = 0;          // 4-byte value (bool: 0/1, uint: raw)
	u_int       m_uBakedConstantId = UINT32_MAX; // codegen-baked id for the drift assert (UINT32_MAX = unknown)
};

// Fixed-capacity, trivially-copyable value table carried inside Flux_PipelineSpecification.
struct Flux_SpecConstantTable
{
	Flux_SpecConstantEntry m_axEntries[FLUX_MAX_SPEC_CONSTANTS];
	u_int                  m_uCount = 0;

	bool IsEmpty() const { return m_uCount == 0; }

	// Name-keyed adds (no baked id → the drift assert is skipped for this entry).
	void AddUInt(const char* szName, u_int uValue) { AddRaw(szName, uValue, UINT32_MAX); }
	void AddBool(const char* szName, bool bValue)  { AddRaw(szName, bValue ? 1u : 0u, UINT32_MAX); }

	// Handle-keyed adds (carry the baked id so the resolver can assert no drift).
	void AddUInt(const Flux_SpecConstantHandle& xHandle, u_int uValue) { AddRaw(xHandle.m_szName, uValue, xHandle.m_uConstantId); }
	void AddBool(const Flux_SpecConstantHandle& xHandle, bool bValue)  { AddRaw(xHandle.m_szName, bValue ? 1u : 0u, xHandle.m_uConstantId); }

	const Flux_SpecConstantEntry* Find(const char* szName) const
	{
		if (!szName) return nullptr;
		for (u_int u = 0; u < m_uCount; u++)
		{
			if (m_axEntries[u].m_szName && strcmp(m_axEntries[u].m_szName, szName) == 0)
			{
				return &m_axEntries[u];
			}
		}
		return nullptr;
	}

private:
	void AddRaw(const char* szName, u_int uValue, u_int uBakedId)
	{
		Zenith_Assert(szName != nullptr, "Flux_SpecConstantTable: null spec-constant name");
		Zenith_Assert(m_uCount < FLUX_MAX_SPEC_CONSTANTS, "Flux_SpecConstantTable: capacity FLUX_MAX_SPEC_CONSTANTS exceeded");
		Zenith_Assert(Find(szName) == nullptr, "Flux_SpecConstantTable: duplicate spec constant '%s'", szName ? szName : "(null)");
		if (m_uCount >= FLUX_MAX_SPEC_CONSTANTS || !szName || Find(szName)) return;
		Flux_SpecConstantEntry& xEntry = m_axEntries[m_uCount++];
		xEntry.m_szName           = szName;
		xEntry.m_uValue           = uValue;
		xEntry.m_uBakedConstantId = uBakedId;
	}
};

// One resolved (name → runtime id) spec-constant ready for the backend to pack
// into a VkSpecializationInfo (or the D3D12 null-backend equivalent).
struct Flux_ResolvedSpecConstant
{
	u_int m_uConstantId = 0;
	u_int m_uSize       = 4;
	u_int m_uValue      = 0;
};

// Resolve a name-keyed table against a shader's reflection into backend-ready
// entries. Pure + backend-free. Entries whose name is absent from the reflection
// are skipped (Vulkan ignores a constant_id absent from a module). Returns the
// number written (<= uMaxOut). Asserts the baked id matches the reflected id when
// the entry carries one (a "rerun FluxCompiler" hot-reload drift tripwire).
u_int Flux_ResolveSpecConstants(const Flux_SpecConstantTable& xTable,
								const Flux_ShaderReflection& xReflection,
								Flux_ResolvedSpecConstant* paxOut, u_int uMaxOut);
