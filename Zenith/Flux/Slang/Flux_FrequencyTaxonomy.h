#pragma once

#include "Flux/Slang/Flux_SlangCompiler.h"  // Flux_ShaderReflection, Flux_ReflectedBinding
#include "Flux/Flux_Enums.h"                 // FluxResourceKind + FluxKindIs* predicates
#include "Core/ZenithConfig.h"               // FLUX_MAX_BINDING_GROUPS

#include <string>

// =====================================================================
// Flux_FrequencyTaxonomy
//
// The canonical frequency-set spine + a PURE validator that asserts every
// compiled program obeys it. The spine is declared once in
// Flux/Shaders/Common/Bindings.slang and #included (text, not `import`) by
// every shader, so the persistent sets 0/1/2 are byte-identical across the
// whole shader set (the precondition for Phase-5 persistent descriptor
// tables + Vulkan prefix-compatible pipeline layouts).
//
//     set 0 = GLOBAL   (view-invariant: g_xGlobal CB; grows: g_axMaterials)
//     set 1 = VIEW     (per-camera: g_xView CB; grows: view-frequency SRVs)
//     set 2 = BINDLESS (the unbounded g_axTextures table — nothing else)
//     set 3 = PASS     (per-pass, per-program)
//     set 4 = DRAW     (per-draw, per-program)
//     set 5 = reserved (future TLAS / acceleration structures)
//
// ValidateReflection is backend-free + device-free (operates on a
// Flux_ShaderReflection only), so it is unit-testable with a synthetic
// reflection AND callable from FluxCompiler after each program compiles.
// =====================================================================
namespace Flux_FrequencyTaxonomy
{
	enum FluxFrequencySet : u_int
	{
		FLUX_FREQUENCY_SET_GLOBAL   = 0,
		FLUX_FREQUENCY_SET_VIEW     = 1,
		FLUX_FREQUENCY_SET_BINDLESS = 2,
		FLUX_FREQUENCY_SET_PASS     = 3,
		FLUX_FREQUENCY_SET_DRAW     = 4,
		FLUX_FREQUENCY_SET_RESERVED = 5,
	};

	// Canonical spine member names (the ConstantBuffer / table members inside the
	// spine ParameterBlocks — match Common/Bindings.slang). Each is pinned to
	// binding 0 of its set; the generated handles are h<name> (hg_xGlobal etc.).
	inline constexpr const char* kszGlobalConstants = "g_xGlobal";
	inline constexpr const char* kszViewConstants   = "g_xView";
	inline constexpr const char* kszBindlessTable   = "g_axTextures";

	// Pure check over ONE program's reflection. Returns true if the program obeys
	// the taxonomy; on false, strErrOut names the first offending binding + reason.
	//
	// Invariants (each PERMANENTLY correct — they hold through Phases 3-6 as the
	// GLOBAL/VIEW sets grow, because growth happens inside the shared spine file):
	//   (1) no descriptor set index >= FLUX_MAX_BINDING_GROUPS.
	//   (2) unbounded arrays (count 0 / unbounded kind) live ONLY in BINDLESS (2).
	//   (3) fixed descriptor arrays (count > 1) live ONLY in BINDLESS (2) — per-
	//       frequency blocks hold single descriptors (CSM-array collapse uses a
	//       count-1 array IMAGE, not a descriptor array, so it is unaffected).
	//   (4) the three universal spine members, IF present, sit at their fixed sets
	//       (g_xGlobal@0 / g_xView@1 / g_axTextures@2). A bare global resource —
	//       the exact thing the spine forbids — makes Slang reserve space 0 for it
	//       and shift every ParameterBlock up one space, which moves g_xGlobal off
	//       set 0 and trips this check. So (4) is the bare-global tripwire.
	//   (5) the BINDLESS set (2) holds ONLY the unbounded table — nothing else may
	//       colonise it.
	inline bool ValidateReflection(const Flux_ShaderReflection& xReflection,
	                               const char* szProgramName, std::string& strErrOut)
	{
		const Zenith_Vector<Flux_ReflectedBinding>& axBindings = xReflection.GetBindings();
		for (u_int u = 0; u < axBindings.GetSize(); u++)
		{
			const Flux_ReflectedBinding& xB = axBindings.Get(u);
			const std::string& strName = xB.m_strName;

			auto fnFail = [&](const std::string& strReason) -> bool
			{
				strErrOut = std::string("[FrequencyTaxonomy] ")
				          + (szProgramName ? szProgramName : "<program>")
				          + ": binding '" + strName + "' (set " + std::to_string(xB.m_uSet)
				          + ", binding " + std::to_string(xB.m_uBinding) + ") " + strReason;
				return false;
			};

			// (1) Set index within the spine range.
			if (xB.m_uSet >= FLUX_MAX_BINDING_GROUPS)
				return fnFail("uses a descriptor set >= FLUX_MAX_BINDING_GROUPS");

			// (2) Unbounded arrays only in BINDLESS.
			const bool bUnbounded = (xB.m_uDescriptorCount == 0) || FluxKindIsUnboundedArray(xB.m_eResourceKind);
			if (bUnbounded && xB.m_uSet != FLUX_FREQUENCY_SET_BINDLESS)
				return fnFail("is an unbounded array outside the BINDLESS set (2)");

			// (3) Fixed descriptor arrays only in BINDLESS.
			if (!bUnbounded && xB.m_uDescriptorCount > 1 && xB.m_uSet != FLUX_FREQUENCY_SET_BINDLESS)
				return fnFail("is a descriptor array (count > 1) outside the BINDLESS set (2)");

			// (4) Canonical spine members at their fixed sets (the bare-global tripwire).
			if (strName == kszGlobalConstants && xB.m_uSet != FLUX_FREQUENCY_SET_GLOBAL)
				return fnFail("g_xGlobal must occupy the GLOBAL set (0) — a bare global resource may have shifted the spine");
			if (strName == kszViewConstants && xB.m_uSet != FLUX_FREQUENCY_SET_VIEW)
				return fnFail("g_xView must occupy the VIEW set (1) — a bare global resource may have shifted the spine");
			if (strName == kszBindlessTable && xB.m_uSet != FLUX_FREQUENCY_SET_BINDLESS)
				return fnFail("g_axTextures must occupy the BINDLESS set (2) — a bare global resource may have shifted the spine");

			// (5) BINDLESS set holds only the table.
			if (xB.m_uSet == FLUX_FREQUENCY_SET_BINDLESS && strName != kszBindlessTable)
				return fnFail("occupies the BINDLESS set (2) but is not the g_axTextures table");
		}
		return true;
	}
}
