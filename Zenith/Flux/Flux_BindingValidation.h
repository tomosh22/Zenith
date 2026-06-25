#pragma once

#include "Core/ZenithConfig.h"

// =====================================================================
// Pure, device-free pre-draw binding validator.
//
// Before a Draw/Dispatch the backend knows, per descriptor set:
//   - the ACTIVE mask: which bindings the bound pipeline's reflected layout
//     declares (bit b set => binding b is declared) — Zenith_Vulkan_RootSig::
//     m_auActiveBindingMask.
//   - the STAGED mask: which bindings the command buffer has actually recorded
//     a resource for this draw (SRV/CBV/UAV/SRV-buffer/scratch).
// A binding that is active but not staged means the shader will read an
// undefined descriptor — the exact "forgot to bind X" bug. This catches it at
// the call site with the offending (set, binding), which is clearer than the
// eventual Vulkan validation-layer error.
//
// The logic is a pure mask comparison so it is unit-tested without a live
// device; the backend supplies the two mask arrays.
// =====================================================================
struct Flux_StagedBindingCheck
{
	bool  m_bAllStaged      = true;
	u_int m_uMissingSet     = UINT32_MAX;
	u_int m_uMissingBinding = UINT32_MAX;
};

// Returns the first (set, binding) that is active but not staged, or
// m_bAllStaged == true if every active binding has a staged resource. Persistent
// sets (whose layouts are validated once, not per-draw) should be excluded by
// the caller via uSetMaskToSkip (a bitmask of set indices to ignore).
inline Flux_StagedBindingCheck Flux_ValidateStagedBindings(
	const u_int* auActiveMask, const u_int* auStagedMask, u_int uNumSets, u_int uSetMaskToSkip = 0)
{
	Flux_StagedBindingCheck xResult;
	for (u_int uSet = 0; uSet < uNumSets; uSet++)
	{
		if ((uSetMaskToSkip >> uSet) & 1u)
		{
			continue;
		}
		const u_int uMissing = auActiveMask[uSet] & ~auStagedMask[uSet];
		if (uMissing != 0u)
		{
			u_int uBit = 0;
			while (((uMissing >> uBit) & 1u) == 0u)
			{
				uBit++;
			}
			xResult.m_bAllStaged      = false;
			xResult.m_uMissingSet     = uSet;
			xResult.m_uMissingBinding = uBit;
			return xResult;
		}
	}
	return xResult;
}
