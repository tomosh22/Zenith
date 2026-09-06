#pragma once

#ifdef ZENITH_TOOLS

#include "Editor/Zenith_UndoSystem.h"
#include "Editor/Zenith_AnimationDocument.h"
#include "Collections/Zenith_Vector.h"
#include "Flux/MeshAnimation/Flux_AnimationClip.h"
#include <string>

//=============================================================================
// Zenith_EditorAnimCommands — the undo layer for Zenith_AnimationDocument.
//
// ★ THESE DO NOT LOOK LIKE THE OTHER EDITOR COMMANDS, DELIBERATELY.
//
// Every command in Zenith_EditorCommands.h resolves its target by EntityID
// through g_xEngine.Scenes().GetSceneDataForEntity(...) — the base class holds
// no scene on purpose (Zenith_UndoSystem.h:33-42), because an entity can move
// between scenes underneath a queued command. A keyframe edit has no entity, no
// scene and no EntityID; copying that pattern would mean inventing one.
//
// The address here is (document, track, key id) — D29:
//
//   • DOCUMENT: a raw Zenith_AnimationDocument*. It cannot dangle, because the
//     ONLY stack these are pushed to is the document's own m_xUndoSystem, and
//     both Zenith_AnimationDocument::Close() and its destructor Clear() that
//     stack — which deletes every command in it. A command therefore cannot
//     outlive the document it points at; there is no ordering to remember and
//     no back-pointer to unregister.
//   • TRACK: (bone name, Flux_AnimTrack), or a root-motion delta track. A bone
//     NAME rather than a channel pointer, because removing a channel's last key
//     deletes the channel (D14) and the undo re-creates it.
//   • KEY ID: the document's stable, non-serialized per-key id (D24). An INDEX
//     would be wrong here in the one case undo exists for: a retime reorders the
//     track, so the index a command captured names a different key by the time
//     it is replayed.
//
// Every command is RECORDED, never Executed, on the way in: the document has
// already performed the edit (it has to — it is the only writer, and it needs
// the mutator's return value to update the id maps), so Zenith_UndoSystem::
// Record pushes the already-applied command. Execute() therefore only ever runs
// on a REDO.
//=============================================================================

//-----------------------------------------------------------------------------
// Shared base: the document pointer and the description string, so the eight
// leaves below carry only their own payload.
//-----------------------------------------------------------------------------
class Zenith_AnimCommandBase : public Zenith_UndoCommand
{
public:
	Zenith_AnimCommandBase(Zenith_AnimationDocument* pxDocument, const char* szDescription);

	const char* GetDescription() const override { return m_strDescription.c_str(); }

protected:
	Zenith_AnimationDocument* m_pxDocument = nullptr;
	std::string m_strDescription;
};

//-----------------------------------------------------------------------------
// A key was inserted into a FREE time slot.
//
// An insert onto an OCCUPIED slot is not this command — D25 keeps the existing
// key's id and replaces its value, so the document pushes a KeyValue instead.
// Using an insert command there would make the undo DELETE a key the user never
// created.
//-----------------------------------------------------------------------------
class Zenith_AnimCommand_KeyInsert : public Zenith_AnimCommandBase
{
public:
	Zenith_AnimCommand_KeyInsert(Zenith_AnimationDocument* pxDocument, const Zenith_AnimTrackId& xTrack,
		u_int uKeyId, float fTimeSeconds, const Zenith_AnimKeyValue& xValue);

	void Execute() override;
	void Undo() override;

private:
	Zenith_AnimTrackId m_xTrack;
	u_int m_uKeyId;
	float m_fTimeSeconds;
	Zenith_AnimKeyValue m_xValue;
};

//-----------------------------------------------------------------------------
// A key was removed. ★ THE UNDO RESTORES IT UNDER ITS ORIGINAL ID, so a
// selection (or a later command) still holding that id resolves to the same key
// afterwards. That is the entire reason the document keeps an
// insert-with-known-id path that the public API does not expose.
//-----------------------------------------------------------------------------
class Zenith_AnimCommand_KeyRemove : public Zenith_AnimCommandBase
{
public:
	Zenith_AnimCommand_KeyRemove(Zenith_AnimationDocument* pxDocument, const Zenith_AnimTrackId& xTrack,
		u_int uKeyId, float fTimeSeconds, const Zenith_AnimKeyValue& xValue);

	void Execute() override;
	void Undo() override;

private:
	Zenith_AnimTrackId m_xTrack;
	u_int m_uKeyId;
	float m_fTimeSeconds;
	Zenith_AnimKeyValue m_xValue;
};

//-----------------------------------------------------------------------------
// A key was retimed — the reordering case the stable id exists for.
//-----------------------------------------------------------------------------
class Zenith_AnimCommand_KeyTime : public Zenith_AnimCommandBase
{
public:
	Zenith_AnimCommand_KeyTime(Zenith_AnimationDocument* pxDocument, const Zenith_AnimTrackId& xTrack,
		u_int uKeyId, float fOldTimeSeconds, float fNewTimeSeconds);

	void Execute() override;
	void Undo() override;

private:
	Zenith_AnimTrackId m_xTrack;
	u_int m_uKeyId;
	float m_fOldTimeSeconds;
	float m_fNewTimeSeconds;
};

//-----------------------------------------------------------------------------
// A key's VALUE changed in place (an inspector edit, or an insert onto an
// occupied slot).
//-----------------------------------------------------------------------------
class Zenith_AnimCommand_KeyValue : public Zenith_AnimCommandBase
{
public:
	Zenith_AnimCommand_KeyValue(Zenith_AnimationDocument* pxDocument, const Zenith_AnimTrackId& xTrack,
		u_int uKeyId, const Zenith_AnimKeyValue& xOld, const Zenith_AnimKeyValue& xNew, const char* szDescription);

	void Execute() override;
	void Undo() override;

private:
	Zenith_AnimTrackId m_xTrack;
	u_int m_uKeyId;
	Zenith_AnimKeyValue m_xOld;
	Zenith_AnimKeyValue m_xNew;
};

//-----------------------------------------------------------------------------
// The clip's duration. Not a key edit, but it belongs on the same stack: a
// duration change and a keyframe drag are one editing session to the user.
//-----------------------------------------------------------------------------
class Zenith_AnimCommand_Duration : public Zenith_AnimCommandBase
{
public:
	Zenith_AnimCommand_Duration(Zenith_AnimationDocument* pxDocument, float fOldSeconds, float fNewSeconds);

	void Execute() override;
	void Undo() override;

private:
	float m_fOldSeconds;
	float m_fNewSeconds;
};

//-----------------------------------------------------------------------------
// Events. Same stable-id story as keys — Flux_AnimationClip::AddEvent sorts the
// list, so an index is not an identity here either.
//-----------------------------------------------------------------------------
class Zenith_AnimCommand_EventAdd : public Zenith_AnimCommandBase
{
public:
	Zenith_AnimCommand_EventAdd(Zenith_AnimationDocument* pxDocument, u_int uEventId, const Flux_AnimationEvent& xEvent);

	void Execute() override;
	void Undo() override;

private:
	u_int m_uEventId;
	Flux_AnimationEvent m_xEvent;
};

class Zenith_AnimCommand_EventRemove : public Zenith_AnimCommandBase
{
public:
	Zenith_AnimCommand_EventRemove(Zenith_AnimationDocument* pxDocument, u_int uEventId, const Flux_AnimationEvent& xEvent);

	void Execute() override;
	void Undo() override;

private:
	u_int m_uEventId;
	Flux_AnimationEvent m_xEvent;
};

class Zenith_AnimCommand_EventEdit : public Zenith_AnimCommandBase
{
public:
	Zenith_AnimCommand_EventEdit(Zenith_AnimationDocument* pxDocument, u_int uEventId,
		const Flux_AnimationEvent& xOld, const Flux_AnimationEvent& xNew, const char* szDescription);

	void Execute() override;
	void Undo() override;

private:
	u_int m_uEventId;
	Flux_AnimationEvent m_xOld;
	Flux_AnimationEvent m_xNew;
};

//-----------------------------------------------------------------------------
// MANY EDITS, ONE UNDO STEP (WU-3.3).
//
// ★ WHY THIS EXISTS AT ALL: Zenith_UndoSystem HAS NO GROUPING. It is a flat
// LIFO of Zenith_UndoCommand* with Execute / Record / Undo / Redo and nothing
// resembling a transaction — so a dope-sheet drag that moved eleven keys would
// push eleven commands and take eleven Ctrl+Z presses to reverse, with the ten
// intermediate states being ones the user never saw. This is the whole of the
// mechanism: a command that OWNS other commands.
//
// ★ THE ORDER IS FORWARD ON EXECUTE AND REVERSE ON UNDO, and that is not
// cosmetic. A multi-key retime is applied in an order chosen so no intermediate
// state collides (D11 refuses a key landing on an occupied time), and the exact
// inverse of that order is the only order whose intermediate states are equally
// collision-free. Undoing the children in the order they were applied would put
// the first key back on top of the second one, the mutator would refuse, and
// the undo would silently half-work.
//
// ★ IT IS BUILT BY THE DOCUMENT, NOT BY A CALLER. Zenith_AnimationDocument::
// BeginCompound / EndCompound open and close one of these; while it is open,
// every command the document's ordinary verbs push is ADOPTED here instead of
// reaching the undo stack. That is what keeps the collection rule in one place:
// an operation calls the same public verbs it always did and does not know it
// is being grouped.
//-----------------------------------------------------------------------------
class Zenith_AnimCommand_Compound : public Zenith_AnimCommandBase
{
public:
	Zenith_AnimCommand_Compound(Zenith_AnimationDocument* pxDocument, const char* szDescription);
	// Deletes every child it owns — the compound is the sole owner from Adopt on.
	~Zenith_AnimCommand_Compound() override;

	// Takes ownership. A null child is ignored rather than stored.
	void Adopt(Zenith_UndoCommand* pxChild);
	u_int GetChildCount() const { return m_apxChildren.GetSize(); }
	// EndCompound names the group once it knows what it collected.
	void SetDescription(const char* szDescription);

	void Execute() override;
	void Undo() override;

private:
	Zenith_Vector<Zenith_UndoCommand*> m_apxChildren;
};

// Force-link anchor: Zenith_AnimationDocument_ForceLink calls this, so ONE call
// from Zenith_Editor::Initialise anchors both TUs against /OPT:REF and the
// ZENITH_TEST registrars at the bottom of this .cpp run. The document .cpp does
// already name every command constructor, but that is a reference the linker may
// satisfy and then strip; an explicit call chain is the repo's existing idiom
// (Zenith_BehaviourGraphAsset_ForceLink) and does not depend on that reasoning.
bool Zenith_EditorAnimCommands_ForceLink();

#endif // ZENITH_TOOLS
