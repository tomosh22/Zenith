#pragma once

#include "AssetHandling/Zenith_Asset.h"
#include "Flux/MeshAnimation/Flux_AnimatorControllerDef.h"
#include <string>

//------------------------------------------------------------------------------
// Zenith_AnimatorControllerAsset — the .zanimctrl asset (WU-6.2).
//
// Wraps ONE owned Flux_AnimatorControllerDef: the optional top-level state
// machine, the layer list (each with its own embedded state machine, its stable
// id and its bone-mask asset reference) and the clip paths the controller loads.
// See Flux/MeshAnimation/Flux_AnimatorControllerDef.h for why the state machines
// embed and the masks do not.
//
// ★ ENVELOPE-TYPED, NOT .zdata. The def carries the shared Zenith_StreamEnvelope
// (type id 7, schema 1) as its first four words, exactly like .zanim. This asset
// therefore declares NO ZENITH_ASSET_TYPE_NAME and is NOT registered with
// ZENITH_REGISTER_ASSET_TYPE: the .zdata path writes its own ZDATA magic + a
// null-terminated type name AHEAD of whatever WriteToDataStream produces, so
// using it here would put two headers in one file. Zenith_AssetRegistry::Save
// refuses an asset whose GetTypeName() is null, which is what makes that
// mechanical rather than a convention. It is registered in
// Zenith_AssetRegistry::Initialize() as a member-contract type through
// LoadAssetGeneric<T>, exactly like Zenith_AnimationAsset — and that registration
// is also what anchors this TU against /OPT:REF, so no _ForceLink() is needed.
//
// The def holds NO asset handles: it names clips and masks by path and pins
// nothing. Flux_AnimationController::BuildFromControllerDef is what acquires
// them, and the CONTROLLER is what owns the resulting references.
//------------------------------------------------------------------------------
class Zenith_AnimatorControllerAsset : public Zenith_Asset
{
public:
	Zenith_AnimatorControllerAsset() = default;

	Flux_AnimatorControllerDef& GetDef() { return m_xDef; }
	const Flux_AnimatorControllerDef& GetDef() const { return m_xDef; }

	// Write this asset's def to strPath (prefixed or plain — it goes through
	// Zenith_AssetRegistry::ResolvePath). The only writer; see the class comment
	// for why Zenith_AssetRegistry::Save is not it.
	bool Export(const std::string& strPath) const;

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel() override;
#endif

private:
	friend class Zenith_AssetRegistry;
	template<typename U> friend struct Zenith_AssetLoadTraits;   // DoLoad calls private LoadFromFile

	// Private, one-shot; the registry's LoadAssetGeneric<T> is the caller. Returns
	// whatever Flux_AnimatorControllerDef::ParseStream reported, so a refused file
	// is deleted by the registry rather than cached as an empty controller.
	Zenith_Status LoadFromFile(const std::string& strPath);

	Flux_AnimatorControllerDef m_xDef;
};
