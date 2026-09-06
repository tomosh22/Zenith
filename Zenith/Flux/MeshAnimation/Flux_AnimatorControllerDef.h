#pragma once
#include "Flux_AnimationStateMachineDef.h"
#include "Flux_AnimationLayer.h"          // Flux_LayerBlendMode
#include "Collections/Zenith_Vector.h"
#include "Core/Zenith_Result.h"           // Zenith_Status — the status-returning ParseStream
#include <string>

//=============================================================================
// Flux_AnimatorControllerDef (WU-6.2) — the AUTHORED form of a whole animator,
// and the payload of a .zanimctrl.
//
// ★ THERE WAS NO TOP-LEVEL CONTROLLER ASSET, AND A STATE-MACHINE DEF ALONE
// COULD NOT BECOME ONE. A Flux_AnimationController has an OPTIONAL top-level
// state machine AND N layers, each of which owns its own state machine — and
// every layered game (Zenithmon's humans, Combat's) reaches its graph THROUGH a
// layer, with m_pxStateMachine null. Persisting one Flux_AnimationStateMachineDef
// therefore persists, at best, the half nobody uses.
//
// ★ WHY THE STATE MACHINES EMBED AND THE MASKS DO NOT (D46/D47). A state
// machine's states name THIS controller's clips (by name, through its clip
// collection) and its parameters, so a shared SM def would be meaningless outside
// the controller that owns it — it embeds. A bone MASK names bones, so it is
// scoped to the SKELETON and is shared verbatim by every controller on that rig;
// a layer therefore carries a mask ASSET PATH and .zanimmask is its own type
// (AssetHandling/Zenith_BoneMaskAsset.h). Embedding the mask would give one rig's
// "upper body" as many independent copies as it has controllers.
//
// ★ THE CLIP LIST IS PART OF THE DEF. Clip references inside a state machine are
// by NAME and resolve through Flux_AnimationClipCollection, so a def that named
// no files would rebuild into a controller whose every leaf posed the bind pose.
// GetClipPaths() is the AddClipFromFile list, and it is what makes the def
// self-contained.
//
// ★ LAYER IDS ARE STABLE AND MONOTONIC, and the NEXT id is serialized with them.
// A layer's INDEX is not an identity: inserting a layer renumbers every one above
// it. WU-6.3 addresses layers by id, so the counter has to survive a round trip —
// otherwise a def reloaded and then extended would hand out an id an existing
// layer already holds.
//
// SERIALIZATION CARRIES THE SHARED ENVELOPE (type id 7, schema 1), unlike the
// state-machine def it embeds, which is deliberately payload-only so that there
// is exactly one magic/schema word in a .zanimctrl.
//=============================================================================

//=============================================================================
// Flux_AnimatorControllerLayerDef
//
// One layer's authored half. Held BY POINTER in the controller def, not by value:
// Flux_AnimationStateMachineDef has a user-declared destructor and deleted copy
// operations, so it is neither copyable nor movable and cannot live inside a
// Zenith_Vector element.
//=============================================================================
class Flux_AnimatorControllerLayerDef
{
public:
	Flux_AnimatorControllerLayerDef() = default;
	explicit Flux_AnimatorControllerLayerDef(const std::string& strName);
	~Flux_AnimatorControllerLayerDef() = default;

	// Owns a non-copyable def by value.
	Flux_AnimatorControllerLayerDef(const Flux_AnimatorControllerLayerDef&) = delete;
	Flux_AnimatorControllerLayerDef& operator=(const Flux_AnimatorControllerLayerDef&) = delete;

	// The stable id. Assigned by Flux_AnimatorControllerDef::AddLayer; a caller
	// setting it by hand owns keeping it unique (SetLayerId exists for the reader
	// and for an editor re-ordering layers, not for authoring a new one).
	u_int GetLayerId() const { return m_uLayerId; }
	void SetLayerId(u_int uId) { m_uLayerId = uId; }

	const std::string& GetName() const { return m_strName; }
	void SetName(const std::string& strName) { m_strName = strName; }

	float GetWeight() const { return m_fWeight; }
	void SetWeight(float fWeight) { m_fWeight = fWeight; }

	Flux_LayerBlendMode GetBlendMode() const { return m_eBlendMode; }
	void SetBlendMode(Flux_LayerBlendMode eMode) { m_eBlendMode = eMode; }

	bool GetEmitEvents() const { return m_bEmitEvents; }
	void SetEmitEvents(bool bEmit) { m_bEmitEvents = bEmit; }

	// The .zanimmask this layer masks with, as an asset path. EMPTY means "no
	// mask", which is a different thing from an all-zero mask — see
	// Zenith_BoneMaskAsset::HasAvatarMask.
	const std::string& GetBoneMaskAssetPath() const { return m_strBoneMaskAssetPath; }
	void SetBoneMaskAssetPath(const std::string& strPath) { m_strBoneMaskAssetPath = strPath; }

	Flux_AnimationStateMachineDef& GetStateMachineDef() { return m_xStateMachineDef; }
	const Flux_AnimationStateMachineDef& GetStateMachineDef() const { return m_xStateMachineDef; }

	// Payload only — the controller def owns the envelope.
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

private:
	u_int m_uLayerId = 0;
	std::string m_strName;
	float m_fWeight = 1.0f;
	Flux_LayerBlendMode m_eBlendMode = LAYER_BLEND_OVERRIDE;
	bool m_bEmitEvents = true;
	std::string m_strBoneMaskAssetPath;
	Flux_AnimationStateMachineDef m_xStateMachineDef;
};

//=============================================================================
// Flux_AnimatorControllerDef
//=============================================================================
class Flux_AnimatorControllerDef
{
public:
	Flux_AnimatorControllerDef() = default;
	~Flux_AnimatorControllerDef();

	// Owns its layers (and, through them, their state-machine defs).
	Flux_AnimatorControllerDef(const Flux_AnimatorControllerDef&) = delete;
	Flux_AnimatorControllerDef& operator=(const Flux_AnimatorControllerDef&) = delete;

	// Delete every layer, the top-level state machine and the clip list, and
	// reset the layer-id counter.
	void Clear();

	// ★ A DEEP COPY THROUGH THIS DEF'S OWN SERIALIZER, for the same reason
	// Flux_AnimationStateMachineDef::CopyFrom is: a blend tree is a polymorphic
	// hierarchy with no clone verb, and Write/Read is the one faithful walk of it
	// that already exists and is already pinned by a test. A field added to a node
	// is carried by the copy the day it is carried by the file.
	void CopyFrom(const Flux_AnimatorControllerDef& xSource);

	// Name — purely descriptive; nothing keys on it.
	const std::string& GetName() const { return m_strName; }
	void SetName(const std::string& strName) { m_strName = strName; }

	//=========================================================================
	// Clips
	//=========================================================================

	// Append a clip asset path. A path already in the list is IGNORED rather than
	// duplicated: Flux_AnimationController::AddClipFromFile pushes one owning
	// handle per call and adds the clip to a NAME-keyed collection, so a duplicate
	// buys a second refcount and nothing else.
	void AddClipPath(const std::string& strPath);
	const Zenith_Vector<std::string>& GetClipPaths() const { return m_xClipPaths; }
	void ClearClipPaths() { m_xClipPaths.Clear(); }

	//=========================================================================
	// Top-level state machine (OPTIONAL — a layered controller has none)
	//=========================================================================

	bool HasStateMachineDef() const { return m_pxStateMachineDef != nullptr; }
	// Creates the def on first call; this is the authoring entry point.
	Flux_AnimationStateMachineDef& GetOrCreateStateMachineDef();
	Flux_AnimationStateMachineDef* GetStateMachineDef() { return m_pxStateMachineDef; }
	const Flux_AnimationStateMachineDef* GetStateMachineDef() const { return m_pxStateMachineDef; }
	void ClearStateMachineDef();

	//=========================================================================
	// Layers
	//=========================================================================

	// Appends a layer carrying a FRESH id (the monotonic counter). Never reuses an
	// id a removed layer held.
	Flux_AnimatorControllerLayerDef* AddLayer(const std::string& strName);
	u_int GetLayerCount() const { return m_xLayers.GetSize(); }
	Flux_AnimatorControllerLayerDef* GetLayer(u_int uIndex);
	const Flux_AnimatorControllerLayerDef* GetLayer(u_int uIndex) const;
	// By stable id, not index. Null when no layer carries it.
	Flux_AnimatorControllerLayerDef* FindLayerById(u_int uLayerId);
	const Flux_AnimatorControllerLayerDef* FindLayerById(u_int uLayerId) const;
	void RemoveLayer(u_int uIndex);

	// Give a layer an id chosen elsewhere (an export re-stating the ids the live
	// controller already carries) AND keep the counter monotonic past it.
	//
	// ★ Flux_AnimatorControllerLayerDef::SetLayerId ALONE DOES NOT DO THAT, and
	// the difference is a duplicate id. AddLayer hands out 0,1,2…; an export that
	// then overwrote those with the runtime's 5,6,7 would leave the counter at 3,
	// and the next AddLayer would mint 3 — unique today, and a collision the
	// moment those layers are renumbered again.
	void AssignLayerId(Flux_AnimatorControllerLayerDef& xLayer, u_int uLayerId);

	// The id AddLayer will hand out next. Serialized, so ids stay unique across a
	// save/load/extend cycle.
	u_int GetNextLayerId() const { return m_uNextLayerId; }

	//=========================================================================
	// Serialization — the ENVELOPE lives here (type id 7, schema 1)
	//=========================================================================

	// Envelope + payload. The state-machine defs it embeds write payload only.
	void WriteToDataStream(Zenith_DataStream& xStream) const;

	// The load contract, mirroring Flux_AnimationClip::ParseStream exactly: no
	// envelope (or a stream too short for one) -> BAD_MAGIC; another asset's type
	// id -> INVALID_ARGUMENT; a newer envelope or any schema that is not
	// uZENITH_ANIMCTRL_SCHEMA_CURRENT -> VERSION_MISMATCH. Every refusal asserts
	// EXACTLY once and leaves this def CLEARED, never half-parsed.
	Zenith_Status ParseStream(Zenith_DataStream& xStream);

	// Kept only for Zenith_DataStream's `<<`/`>>` dispatch; delegates to
	// ParseStream and drops the status. New code calls ParseStream.
	void ReadFromDataStream(Zenith_DataStream& xStream);

	// WriteToDataStream + WriteToFile. Returns false only on an empty path.
	bool Export(const std::string& strPath) const;

private:
	std::string m_strName;
	Zenith_Vector<std::string> m_xClipPaths;
	Flux_AnimationStateMachineDef* m_pxStateMachineDef = nullptr;   // Owned, optional
	Zenith_Vector<Flux_AnimatorControllerLayerDef*> m_xLayers;      // Owned
	u_int m_uNextLayerId = 0;
};
