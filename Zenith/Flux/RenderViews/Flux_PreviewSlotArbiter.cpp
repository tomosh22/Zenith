#include "Zenith.h"

// TOOLS-only (see the header): in non-tools configs this TU compiles empty.
#ifdef ZENITH_TOOLS

#include "Flux/RenderViews/Flux_PreviewSlotArbiter.h"

// ★ NO /OPT:REF ANCHOR HERE, ON PURPOSE. Unlike the animation-preview session,
// this TU is named by GENUINE callers that are themselves linked — all of them
// now in Flux_MaterialPreviewController, a registered tools feature: SetActive's
// two edges, Update's liveness-expiry release and the --preview-test-view
// diagnostic's own pair, ReleaseAssetReferences, HasSlot/GetPreviewSlotOwnerName
// and the destructor. A force-link anchor would be dead code pretending to be a
// safety net.
//
// There are no ZENITH_TESTs in this file either. The arbitration unit lives in
// Flux_MaterialPreviewController.Tests.inl, beside the claimant it is about
// (D5 moved it there from Zenith_AnimationPreviewSession.Tests.inl, where D3 had
// parked it while the animation editor was still a second claimant).

const void* Flux_PreviewSlotArbiter::s_pxOwner = nullptr;
std::string Flux_PreviewSlotArbiter::s_strOwnerName;

namespace
{
	// Returned by GetOwnerName when nothing owns the slot. A reference return needs
	// a real object; a function-local static keeps it out of static-init order.
	const std::string& EmptyOwnerName()
	{
		static const std::string ls_strEmpty;
		return ls_strEmpty;
	}
}

bool Flux_PreviewSlotArbiter::Claim(const void* pxOwner, const std::string& strOwnerName)
{
	Zenith_Assert(pxOwner != nullptr, "Flux_PreviewSlotArbiter::Claim: an owner must have an identity");
	if (pxOwner == nullptr)
	{
		return false;
	}

	const bool bChanged = (s_pxOwner != pxOwner);
	s_pxOwner = pxOwner;
	s_strOwnerName = strOwnerName;
	return bChanged;
}

bool Flux_PreviewSlotArbiter::Release(const void* pxOwner)
{
	if (pxOwner == nullptr || s_pxOwner != pxOwner)
	{
		return false;
	}
	s_pxOwner = nullptr;
	s_strOwnerName.clear();
	return true;
}

const std::string& Flux_PreviewSlotArbiter::GetOwnerName()
{
	return (s_pxOwner != nullptr) ? s_strOwnerName : EmptyOwnerName();
}

void Flux_PreviewSlotArbiter::ResetForTesting()
{
	s_pxOwner = nullptr;
	s_strOwnerName.clear();
}

#endif // ZENITH_TOOLS
