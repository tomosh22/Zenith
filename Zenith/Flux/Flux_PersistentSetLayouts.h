#pragma once

#include "Flux/Flux_Types.h"   // Flux_BindingGroupLayout / Flux_BindingGroupEntry
#include "Flux/Flux_Enums.h"   // FluxFrequencyClass / FluxResourceKind / FluxKindIs*

#include <string>

// =====================================================================
// Flux_PersistentSetLayouts
//
// The C++-authoritative description of the PERSISTENT spine sets — GLOBAL (0),
// VIEW (1), BINDLESS (2). These three sets are bound persistently in Phase 5
// (allocated once, written once per frame, bound only on a handle change), which
// REQUIRES their descriptor-set layout to be byte-identical across every pipeline
// (Vulkan pipeline-layout prefix-compatibility + a single shared descriptor set).
//
// That identity is already guaranteed structurally — the spine is declared once
// in Flux/Shaders/Common/Bindings.slang and #included (text, not `import`) by every
// program, so reflection reports the same canonical members at sets 0/1/2 even in
// programs that don't statically use them. This file is the single source of truth
// the engine ASSERTS that invariant against, shared by:
//   - reflection classification: Flux_ShaderReflection::PopulateLayout tags each
//     group's FluxFrequencyClass via ClassifyMember (below),
//   - the Vulkan RootSig boot-time compatibility assert (Phase 5.0 — ValidateCanonicalGroup),
//   - the Vulkan persistent-set layout creation + borrow (Phase 5.1).
//
// Pure data + pure comparators (no device, no backend) → unit-testable in isolation.
//
// AS THE SPINE GROWS, UPDATE THIS IN LOCKSTEP: Phase 5.3 adds g_axMaterials to
// GLOBAL and Phase 5.4 adds view-frequency SRVs to VIEW. The canonical layout here
// is what the boot assert enforces, so growing the spine block without growing this
// manifest (or vice-versa) fails the build at the first pipeline.
// =====================================================================
namespace Flux_PersistentSetLayouts
{
	// The canonical persistent set indices (mirror Flux_FrequencyTaxonomy's spine).
	inline constexpr u_int kuSetGlobal   = 0;
	inline constexpr u_int kuSetView     = 1;
	inline constexpr u_int kuSetBindless = 2;

	// Canonical binding-0 member name of each persistent set (mirrors the spine
	// ParameterBlocks in Common/Bindings.slang). Reflection tags a group's class by
	// matching the binding-0 member name to one of these.
	inline constexpr const char* kszGlobalMember0   = "g_xGlobal";
	inline constexpr const char* kszViewMember0     = "g_xView";
	inline constexpr const char* kszBindlessMember0 = "g_axTextures";

	// Classify a (set, member name) pair into its persistence class. Returns GENERIC
	// for anything that is not a canonical spine member — so a non-spine program (no
	// g_xGlobal/g_xView/g_axTextures) is never mis-tagged, and the GLOBAL/VIEW borrow
	// only ever fires on genuine spine sets (not e.g. a non-spine program that happens
	// to put an unrelated CB at set 0).
	inline FluxFrequencyClass ClassifyMember(u_int uSet, const std::string& strMemberName)
	{
		if (uSet == kuSetGlobal   && strMemberName == kszGlobalMember0)   return FLUX_FREQUENCY_CLASS_GLOBAL;
		if (uSet == kuSetView     && strMemberName == kszViewMember0)     return FLUX_FREQUENCY_CLASS_VIEW;
		if (uSet == kuSetBindless && strMemberName == kszBindlessMember0) return FLUX_FREQUENCY_CLASS_BINDLESS;
		return FLUX_FREQUENCY_CLASS_GENERIC;
	}

	// Validate that a reflected/spec binding group classified GLOBAL or VIEW matches
	// the canonical persistent layout. Pure (device-free). On mismatch fills strErrOut
	// and returns false. GENERIC and BINDLESS classes return true (BINDLESS is the
	// kind-detected unbounded table — validated by the taxonomy + its own borrow path).
	//
	// Canonical as of Phase 5.0 (single-member): GLOBAL = { binding 0: uniform buffer,
	// count 1 } and likewise VIEW. Phase 5.3/5.4 relax the "single-member" rule here in
	// lockstep with the spine growth (g_axMaterials into GLOBAL, view SRVs into VIEW).
	inline bool ValidateCanonicalGroup(FluxFrequencyClass eClass,
	                                   const Flux_BindingGroupLayout& xGroup,
	                                   std::string& strErrOut)
	{
		if (eClass != FLUX_FREQUENCY_CLASS_GLOBAL && eClass != FLUX_FREQUENCY_CLASS_VIEW)
		{
			return true;
		}

		const char* szSet = (eClass == FLUX_FREQUENCY_CLASS_GLOBAL) ? "GLOBAL(0)" : "VIEW(1)";

		// Binding 0 must be a single uniform buffer (g_xGlobal / g_xView CB).
		const Flux_BindingGroupEntry& x0 = xGroup.m_axBindings[0];
		if (!x0.m_bPresent || !FluxKindIsUniformBuffer(x0.m_eKind) || x0.m_uDescriptorCount != 1)
		{
			strErrOut = std::string("persistent ") + szSet
			          + " set: binding 0 must be a single uniform buffer (the spine constants CB)";
			return false;
		}

		// Single-member until Phase 5.3/5.4 grow the spine — any extra present binding
		// means the spine block and this manifest have drifted out of lockstep.
		for (u_int u = 1; u < FLUX_MAX_BINDINGS_PER_GROUP; u++)
		{
			if (xGroup.m_axBindings[u].m_bPresent)
			{
				strErrOut = std::string("persistent ") + szSet
				          + " set: unexpected binding at slot " + std::to_string(u)
				          + " — the spine GLOBAL/VIEW sets are single-member until Phase 5.3/5.4 grow them"
				            " in lockstep with Flux_PersistentSetLayouts";
				return false;
			}
		}
		return true;
	}
}
