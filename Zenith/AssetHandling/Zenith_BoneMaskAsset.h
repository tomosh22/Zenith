#pragma once

#include "AssetHandling/Zenith_Asset.h"
#include "Collections/Zenith_Vector.h"
#include <string>

// References only — the Flux mask type and the skeleton are complete in the .cpp.
class Flux_BoneMask;
class Zenith_SkeletonAsset;

//------------------------------------------------------------------------------
// Zenith_BoneMaskAsset — the .zanimmask asset (WU-6.2).
//
// ★ A MASK IS SKELETON-SCOPED, NOT CONTROLLER-SCOPED, WHICH IS WHY IT IS ITS OWN
// FILE (D46). A state machine's states name their controller's clips and
// parameters, so a .zanimctrl embeds them; a mask names BONES, so "upper body on
// the StickFigure rig" is one object that every controller on that rig shares.
// Embedding it would give one rig as many independent copies of the same mask as
// it has controllers, and no way to fix them all at once.
//
// ★ WEIGHTS ARE STORED BY BONE NAME, AND Flux_BoneMask IS BY INDEX. Flux_BoneMask
// is a flat FLUX_MAX_BONES weight array whose meaning depends entirely on which
// skeleton produced the indices — so it is not something to persist. The asset
// keeps names and resolves them against a specific Zenith_SkeletonAsset through
// ResolveTo. (WU-7.1 owns any change to Flux_BoneMask itself; nothing here
// touches it.)
//
// ★ m_bHasAvatarMask IS EXPLICIT, AND THAT IS THE POINT (D47). The layer
// deserializer infers "this layer has a mask" from `any weight > 0`, so an
// all-zero mask — a perfectly meaningful one, "this layer overrides nothing yet"
// — round-trips as NO MASK and the layer silently starts overriding the whole
// skeleton. The flag is written, so an all-zero mask stays a mask.
//
// Envelope: type id 8 (uZENITH_ANIMMASK_ASSET_TYPE_ID), schema 1. This is an
// ENVELOPE-TYPED asset like .zanim, NOT a .zdata serializable — it deliberately
// declares no ZENITH_ASSET_TYPE_NAME, so Zenith_AssetRegistry::Save (which would
// wrap a second ZDATA magic + type-name header around this one) refuses it and
// Export is the only writer. One extension, one format.
//------------------------------------------------------------------------------

// One authored entry. A name the skeleton does not carry is REPORTED by
// ResolveTo, never silently dropped.
struct Zenith_BoneMaskEntry
{
	std::string m_strBoneName;
	float m_fWeight = 0.0f;

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);
};

class Zenith_BoneMaskAsset : public Zenith_Asset
{
public:
	Zenith_BoneMaskAsset() = default;

	//--------------------------------------------------------------------------
	// Authoring
	//--------------------------------------------------------------------------

	// Add the bone, or REPLACE the weight of one already present. Case-sensitive,
	// matching Zenith_SkeletonAsset::GetBoneIndex.
	void SetBoneWeight(const std::string& strBoneName, float fWeight);
	// 0.0f when the mask does not name the bone — which is the same answer a
	// resolved Flux_BoneMask gives, so the two agree.
	float GetBoneWeight(const std::string& strBoneName) const;
	bool HasBone(const std::string& strBoneName) const;
	void RemoveBone(const std::string& strBoneName);
	void ClearEntries() { m_xEntries.Clear(); }

	const Zenith_Vector<Zenith_BoneMaskEntry>& GetEntries() const { return m_xEntries; }
	u_int GetEntryCount() const { return m_xEntries.GetSize(); }

	// D47. TRUE by default: a .zanimmask that exists IS a mask, whatever its
	// weights sum to. Serialized rather than inferred.
	bool HasAvatarMask() const { return m_bHasAvatarMask; }
	void SetHasAvatarMask(bool bHas) { m_bHasAvatarMask = bHas; }

	//--------------------------------------------------------------------------
	// Resolution
	//--------------------------------------------------------------------------

	// Names -> indices against xSkeleton, writing into xOutMask (which starts
	// fully zeroed, so a bone this mask does not name gets weight 0).
	//
	// ★ AN UNRESOLVABLE NAME IS REPORTED, NOT DROPPED. Returns FALSE and logs each
	// offending name with Zenith_Error. A mask silently losing a bone is a layer
	// that silently stops (or starts) overriding it — a difference that only shows
	// up as an animation looking wrong, with nothing to grep for. Every RESOLVABLE
	// entry is still written, so the caller may choose to carry on with a partial
	// mask; it just cannot do so unknowingly.
	bool ResolveTo(const Zenith_SkeletonAsset& xSkeleton, Flux_BoneMask& xOutMask) const;

	//--------------------------------------------------------------------------
	// Serialization — the ENVELOPE lives here (type id 8, schema 1)
	//--------------------------------------------------------------------------

	void WriteToDataStream(Zenith_DataStream& xStream) const override;
	// Kept for Zenith_DataStream's `<<`/`>>` dispatch; delegates to ParseStream.
	void ReadFromDataStream(Zenith_DataStream& xStream) override;

	// The load contract: no envelope -> BAD_MAGIC, another type id ->
	// INVALID_ARGUMENT, any schema that is not current -> VERSION_MISMATCH. Every
	// refusal asserts EXACTLY once and leaves the asset EMPTY, never half-parsed.
	Zenith_Status ParseStream(Zenith_DataStream& xStream);

	// WriteToDataStream + WriteToFile. Returns false only on an empty path.
	bool Export(const std::string& strPath) const;

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel() override;
#endif

private:
	friend class Zenith_AssetRegistry;
	template<typename U> friend struct Zenith_AssetLoadTraits;   // DoLoad calls private LoadFromFile

	// Private, one-shot; the registry's LoadAssetGeneric<T> is the caller.
	Zenith_Status LoadFromFile(const std::string& strPath);

	Zenith_Vector<Zenith_BoneMaskEntry> m_xEntries;
	bool m_bHasAvatarMask = true;
};
