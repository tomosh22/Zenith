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

	// VIEW-set binding indices (must match the ViewParams member declaration order in
	// Common/Bindings.slang — reflection assigns bindings by order). The backend
	// builds the VIEW descriptor-set layout + writes its descriptors against these,
	// and ValidateCanonicalGroup asserts the reflected layout matches. Phase 5.4
	// grows this list as view-frequency SRVs are promoted into the persistent set.
	inline constexpr u_int kuViewBinding_View = 0;   // g_xView  (ConstantBuffer)
	inline constexpr u_int kuViewBinding_CSM  = 1;   // g_xCSM   (Sampler2DArray — cascaded shadows)
	inline constexpr u_int kuViewBindingCount = 2;   // number of VIEW bindings currently in the spine

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
	// Canonical layout (kept in lockstep with the spine blocks in Common/Bindings.slang):
	//   GLOBAL = { binding 0: uniform buffer (g_xGlobal), binding 1: structured buffer
	//             (g_axMaterials, Phase 5.3) }
	//   VIEW   = { binding 0: uniform buffer (g_xView), binding 1: combined image
	//             sampler (g_xCSM cascaded-shadow array, Phase 5.4) }
	inline bool ValidateCanonicalGroup(FluxFrequencyClass eClass,
	                                   const Flux_BindingGroupLayout& xGroup,
	                                   std::string& strErrOut)
	{
		if (eClass != FLUX_FREQUENCY_CLASS_GLOBAL && eClass != FLUX_FREQUENCY_CLASS_VIEW)
		{
			return true;
		}

		const char* szSet = (eClass == FLUX_FREQUENCY_CLASS_GLOBAL) ? "GLOBAL(0)" : "VIEW(1)";

		// Binding 0 must be a single uniform buffer (g_xGlobal / g_xView CB) — both classes.
		const Flux_BindingGroupEntry& x0 = xGroup.m_axBindings[0];
		if (!x0.m_bPresent || !FluxKindIsUniformBuffer(x0.m_eKind) || x0.m_uDescriptorCount != 1)
		{
			strErrOut = std::string("persistent ") + szSet
			          + " set: binding 0 must be a single uniform buffer (the spine constants CB)";
			return false;
		}

		// GLOBAL carries the material-table SSBO at binding 1 (Phase 5.3). VIEW carries
		// the cascaded-shadow array g_xCSM at binding 1 (Phase 5.4). Bindings past the
		// canonical members must be absent — a stray one means the spine block and this
		// manifest have drifted apart.
		u_int uFirstUnexpected = 1u;
		if (eClass == FLUX_FREQUENCY_CLASS_GLOBAL)
		{
			const Flux_BindingGroupEntry& x1 = xGroup.m_axBindings[1];
			if (!x1.m_bPresent || !FluxKindIsStorageBuffer(x1.m_eKind) || x1.m_uDescriptorCount != 1)
			{
				strErrOut = "persistent GLOBAL(0) set: binding 1 must be the single g_axMaterials structured buffer";
				return false;
			}
			uFirstUnexpected = 2u;
		}
		else // VIEW
		{
			const Flux_BindingGroupEntry& x1 = xGroup.m_axBindings[kuViewBinding_CSM];
			if (!x1.m_bPresent || !FluxKindIsSampledTexture(x1.m_eKind) || x1.m_uDescriptorCount != 1)
			{
				strErrOut = "persistent VIEW(1) set: binding 1 must be the single g_xCSM cascaded-shadow sampler (Phase 5.4)";
				return false;
			}
			uFirstUnexpected = kuViewBindingCount;
		}

		for (u_int u = uFirstUnexpected; u < FLUX_MAX_BINDINGS_PER_GROUP; u++)
		{
			if (xGroup.m_axBindings[u].m_bPresent)
			{
				strErrOut = std::string("persistent ") + szSet
				          + " set: unexpected binding at slot " + std::to_string(u)
				          + " — the spine block and Flux_PersistentSetLayouts have drifted out of lockstep";
				return false;
			}
		}
		return true;
	}

	// Phase 5.1: pure gate for the command-buffer persistent-set bind loop. Only
	// GLOBAL/VIEW are bound through the handle-tracked persistent path (BINDLESS
	// goes through UseBindlessTextures; GENERIC sets are owned and allocated per
	// worker). A persistent set is (re)bound iff its class is GLOBAL/VIEW AND the
	// currently-tracked descriptor handle differs from the desired one — a
	// pipeline switch alone never forces a rebind, only a handle change does. The
	// caller does the (opaque) handle comparison and passes the result, so this
	// decision is device-free and unit-tested without a live descriptor set.
	inline bool ShouldRebindPersistentSet(FluxFrequencyClass eClass, bool bTrackedMatchesDesired)
	{
		if (eClass != FLUX_FREQUENCY_CLASS_GLOBAL && eClass != FLUX_FREQUENCY_CLASS_VIEW)
		{
			return false;
		}
		return !bTrackedMatchesDesired;
	}
}
