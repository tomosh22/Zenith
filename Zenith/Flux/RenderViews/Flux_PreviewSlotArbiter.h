#pragma once

// TOOLS-only, mirroring Flux_MaterialPreviewController's gating exactly: the
// preview render view is a tools-only feature, so the thing that arbitrates it
// is too. The vcxproj compiles this TU in every config — in non-tools builds the
// file is empty.
#ifdef ZENITH_TOOLS

#include <string>

//=============================================================================
// Flux_PreviewSlotArbiter — LAST-OPENED-WINS ownership of the MATERIAL preview
// render view (kuFluxViewSlotPreviewMaterial).
//
// ★ THE PREMISE THIS WAS WRITTEN ON IS GONE. It used to read "there is exactly
// ONE preview view slot and the constant is hard-coded in 18 render features, so
// adding a second is not a small change". Neither half survives. The preview
// class is now a RANGE of kuFluxViewNumPreviewSlots contiguous slots
// (kuFluxViewSlotPreviewMaterial and kuFluxViewSlotPreviewAnim); and no render
// feature hard-codes a preview slot into its PASS CHAIN any more — every one of
// the thirteen instantiates its chain per slot from
// ForEachActiveFullPipelineView, so a preview view added to the registry
// acquires the whole pipeline with no feature edit. What is left is two
// per-slot RESOURCE lists that name the slots they own an allocation for
// (Flux_Graphics.cpp's preview LDRs, Flux_Skybox.cpp's sky-view LUTs) and
// Flux_Translucency's external-item routing, which targets the material preview
// because that is where the material editor's item goes.
//
// So this class is NOT what makes two previews possible; it is what decides who
// owns ONE OF THEM while two claimants want the same one. It arbitrates a single
// named slot — the MATERIAL preview — and nothing else.
//
// ★ THE ANIMATION EDITOR IS NO LONGER A CLAIMANT (D3). It used to share this
// slot, and that sharing was the whole reason the class exists; it stages
// kuFluxViewSlotPreviewAnim now, which nothing else stages, so there is nothing
// left to arbitrate on its side and Zenith_AnimationPreviewSession does not name
// this class at all. What remains here are the two claimants of the MATERIAL
// slot: Flux_MaterialPreviewController's liveness window and the
// --preview-test-view diagnostic (which also runs through that controller, on
// its own edges). If a THIRD preview owner ever appears, the right change is to
// arbitrate PER SLOT (an owner per range index), not to widen this back to "the
// preview view".
//
// The rule that reads best to a user is the one a tabbed editor already implies:
// the thing you opened most recently is the thing you are looking at. The
// dispossessed owner is told WHO took it (by name) so its panel can say so and
// offer a reclaim.
//
// ★ IT LIVES IN Flux/RenderViews, NOT IN Editor/. That placement was originally
// FORCED — the two claimants sat on opposite sides of a layer boundary
// (Flux_MaterialPreviewController is Flux, Zenith_AnimationPreviewSession is
// Editor; Editor may include Flux, Flux may NOT include Editor), and an arbiter
// only one of them can reach arbitrates nothing. That reason is spent with the
// animation editor's departure, and the placement still stands on the merits:
// the resource it owns is a Flux render-view slot, not an editor concept, and
// the --preview-test-view diagnostic that claims it is Flux-side too.
//
// ★ IT IS DELIBERATELY OWNER-AGNOSTIC — a void* identity plus a display name —
// so anything that wants the slot participates with one Claim call and holds no
// reference to the other claimant's type. The pointer is an IDENTITY ONLY and is
// never dereferenced.
//
// ★ IT IS PROCESS-LEVEL STATE because the resource it arbitrates is: one fixed
// view slot in one renderer. That is also why it has a test-only reset — a unit
// that left it claimed would hand its claim to the next unit.
//
// THE RULE EVERY CLAIMANT FOLLOWS, and it is not symmetric with claiming: a
// claimant stages the view only while HasSlot(this), and DEACTIVATES the view
// only when GetOwner() == nullptr. A dispossessed claimant that deactivated on
// its own behalf would tear down the view the CURRENT owner is staging into that
// same frame, and the symptom is the other one's preview flickering black. Note
// that this rule is about the MATERIAL slot alone: the animation preview's slot
// has one stager, so it lowers its own view unconditionally.
//=============================================================================
class Flux_PreviewSlotArbiter
{
public:
	// Takes the slot for pxOwner, dispossessing whoever held it. Always succeeds
	// (last-opened-wins). Returns true iff the owner actually CHANGED.
	//
	// ★ CLAIM ON A TRANSITION, NEVER PER FRAME. A panel that re-claims every frame
	// it is visible cannot be dispossessed at all — it would silently steal the
	// slot back on the next frame and last-opened-wins would degrade to
	// last-drawn-wins.
	static bool Claim(const void* pxOwner, const std::string& strOwnerName);

	// Releases the slot IF pxOwner still holds it. A release by a dispossessed
	// owner is a no-op — tidying up must not take the slot away from whoever
	// legitimately holds it.
	static bool Release(const void* pxOwner);

	static bool HasSlot(const void* pxOwner) { return pxOwner != nullptr && pxOwner == s_pxOwner; }
	static const void* GetOwner() { return s_pxOwner; }
	// Empty when nothing owns the slot.
	static const std::string& GetOwnerName();

	// Test-only reset. See the process-level note above.
	static void ResetForTesting();

private:
	static const void* s_pxOwner;
	static std::string s_strOwnerName;
};

#endif // ZENITH_TOOLS
