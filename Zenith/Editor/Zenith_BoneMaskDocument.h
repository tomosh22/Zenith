#pragma once

#ifdef ZENITH_TOOLS

#include "Editor/Zenith_UndoSystem.h"
#include "AssetHandling/Zenith_BoneMaskAsset.h"
#include "Flux/MeshAnimation/Flux_AnimationLayer.h"   // Flux_LayerBlendMode — LayerAcceptsMask
#include "Collections/Zenith_Vector.h"
#include <string>

class Zenith_SkeletonAsset;
// Defined below, after the document. Forward-declared here because the document
// holds a pointer to an open group (see BeginCompound) and the group holds a
// pointer back at the document.
class Zenith_BoneMaskCommand_Compound;

//=============================================================================
// Zenith_BoneMaskDocument (WU-7.1) — the editable WORKING COPY of one
// .zanimmask, and the ONLY writer of it.
//
// ★ IT IS Zenith_AnimControllerDocument'S SHAPE, DELIBERATELY, down to the
// enums: Open deep-copies the asset's content into m_xWorkingMask, every edit
// lands there, and the live asset only moves at SAVE. Editing the asset in
// place would push half-finished weights into whatever
// Flux_AnimationController::BuildFromControllerDef resolves next — a live
// character's arms fading in and out while a slider is dragged — and would
// leave nothing to Discard back to.
//
// ★ THE WORKING COPY IS A Zenith_BoneMaskAsset BY VALUE, filled through
// Zenith_BoneMaskAsset::CopyFrom. That copies the AUTHORED CONTENT only — the
// entries and the D47 flag — and never the Zenith_Asset base's registry path or
// refcount, so the working copy cannot be mistaken for the cached asset. It is
// an asset-typed object because Export() (the envelope writer, type id 8) lives
// on that class and a second serializer here would be a second format.
//
// ★ THE DOCUMENT HOLDS NO OWNING ASSET HANDLE, the same link-level fact
// Zenith_AnimControllerDocument records: Zenith_AssetHandle<T> is explicitly
// instantiated in AssetHandling/Zenith_AssetHandle.cpp and Zenith_BoneMaskAsset
// is not in that list, so a handle to one would not link. It does not need one —
// the working copy is a deep copy and the asset is reached transiently through
// Zenith_AssetRegistry::GetView at the two moments that need it.
//
// ★ A MASK NAMES BONES, NOT INDICES, AND THIS DOCUMENT NEVER SEES A SKELETON.
// Every verb below takes a bone NAME. The one exception is SetSubtreeWeight,
// which needs a hierarchy to know what a subtree IS and therefore takes the rig
// as a parameter rather than storing one — the panel supplies the dope sheet's
// session skeleton at the call site, and a document that cached one would go
// stale the moment the preview rig changed underneath it.
//=============================================================================

enum Zenith_BoneMaskDocOpenResult : u_int
{
	ZENITH_BONEMASKDOC_OPEN_OK,
	// The path did not resolve to a loadable .zanimmask.
	ZENITH_BONEMASKDOC_OPEN_FAILED_NO_ASSET,
	// This document already holds unsaved edits. Save, Discard or
	// CloseDiscardingChanges first.
	ZENITH_BONEMASKDOC_OPEN_REFUSED_DIRTY,
};

enum Zenith_BoneMaskDocSaveResult : u_int
{
	ZENITH_BONEMASKDOC_SAVE_OK,
	ZENITH_BONEMASKDOC_SAVE_FAILED_NO_DOCUMENT,
	// The bytes on disk afterwards are not the bytes we wrote.
	ZENITH_BONEMASKDOC_SAVE_FAILED_WRITE,
	// ★ The file changed underneath us since Open / the last Save. NOTHING was
	// written. SaveOverwritingExternal() proceeds anyway.
	ZENITH_BONEMASKDOC_SAVE_CONFLICT_EXTERNAL,
};

enum Zenith_BoneMaskDocCloseResult : u_int
{
	ZENITH_BONEMASKDOC_CLOSE_OK,
	// Unsaved edits. The document is UNCHANGED and still open.
	ZENITH_BONEMASKDOC_CLOSE_REFUSED_DIRTY,
};

class Zenith_BoneMaskDocument
{
public:
	Zenith_BoneMaskDocument() = default;
	~Zenith_BoneMaskDocument();

	// ★ NON-COPYABLE, and load-bearing rather than incidental: every undo command
	// this document pushed holds a raw Zenith_BoneMaskDocument* back at it, and a
	// copy would produce a second document whose stack is full of commands
	// pointing at the first.
	Zenith_BoneMaskDocument(const Zenith_BoneMaskDocument&) = delete;
	Zenith_BoneMaskDocument& operator=(const Zenith_BoneMaskDocument&) = delete;

	//-------------------------------------------------------------------------
	// ★ THE ONE RULE ABOUT LAYERS THAT BELONGS HERE (WU-7.1).
	//
	// AN ADDITIVE LAYER IGNORES ITS MASK ENTIRELY, and that is not a nuance — it
	// is the first branch of Flux_AnimationController's layer composition:
	// LAYER_BLEND_ADDITIVE is tested FIRST and goes straight to
	// Flux_SkeletonPose::AdditiveBlend, which takes no mask. Only the OVERRIDE
	// branch reaches MaskedBlend. So a UI that offered a mask control on an
	// additive layer would let a user author a whole mask, save it, assign it,
	// and see absolutely no change — with every gate green and nothing to grep
	// for.
	//
	// It lives on the DOCUMENT rather than on the panel because WU-7.2's layer
	// list needs the same answer and two copies of a rule like this is exactly
	// how the two halves of a UI end up disagreeing. PURE: no document state is
	// read, so it is callable on a closed one and from a unit with no panel.
	//-------------------------------------------------------------------------
	static bool LayerAcceptsMask(Flux_LayerBlendMode eBlendMode);
	// The one wording of the refusal, so the sub-panel, WU-7.2's layer list and
	// the units cannot describe it three different ways.
	static const char* AdditiveLayerMaskNotice();

	//-------------------------------------------------------------------------
	// Lifecycle
	//-------------------------------------------------------------------------

	// Load the .zanimmask at strAssetPath through the registry and deep-copy its
	// content into the working copy. Refuses to throw away unsaved edits.
	Zenith_BoneMaskDocOpenResult Open(const std::string& strAssetPath);

	// Open an EMPTY mask TARGETED at strAssetPath, whether or not a file is
	// there — the boot-time authoring entry point, and the twin of
	// Zenith_AnimControllerDocument::OpenFresh.
	//
	// ★ A SEPARATE VERB RATHER THAN A FALLBACK INSIDE Open, for that document's
	// reason: "the file was not there" and "the path was typed wrong" are the
	// same observation, and a silent fresh start on a typo authors a whole mask
	// into a path nothing reads.
	Zenith_BoneMaskDocOpenResult OpenFresh(const std::string& strAssetPath);

	// Refuses while dirty (the panel prompts, then calls Save or the forced
	// close). On success the working copy is dropped and the undo history
	// CLEARED — which is what guarantees no command outlives its document.
	Zenith_BoneMaskDocCloseResult Close();
	void CloseDiscardingChanges();

	// Re-copy the working mask from the live asset, throwing every unsaved edit
	// away. The undo history goes with it.
	bool DiscardChanges();

	// Write the working copy back to the document's own path, then refresh the
	// LIVE asset's content in place so a later BuildFromControllerDef resolves
	// what was saved. Refuses with ..._CONFLICT_EXTERNAL when the file changed
	// underneath. Undo history SURVIVES a save.
	Zenith_BoneMaskDocSaveResult Save();
	Zenith_BoneMaskDocSaveResult SaveOverwritingExternal();

	// Write to a NEW path and re-target the document at it. Undo history is
	// cleared: the commands describe edits to the file that was.
	Zenith_BoneMaskDocSaveResult SaveAs(const std::string& strNewAssetPath);

	//-------------------------------------------------------------------------
	// State
	//-------------------------------------------------------------------------

	bool IsOpen() const { return m_bOpen; }
	bool IsDirty() const { return m_bDirty; }
	const std::string& GetAssetPath() const { return m_strAssetPath; }
	const std::string& GetResolvedPath() const { return m_strResolvedPath; }
	// The working copy. CONST: the document is the only writer.
	const Zenith_BoneMaskAsset& GetMask() const { return m_xWorkingMask; }
	// The LIVE asset this document saves back into, or nullptr. A TRANSIENT view
	// (no refcount taken) — do not store it.
	Zenith_BoneMaskAsset* GetAsset() const;

	// ★ Reads the file and compares its CONTENT HASH against the one recorded at
	// Open / the last Save — a hash rather than a write time, because a write
	// time is a clock and the two things compared are written by two processes.
	// Costs one file read; call it on demand, not per frame.
	bool HasExternalModification() const;

	//-------------------------------------------------------------------------
	// Inspection
	//-------------------------------------------------------------------------

	// 0.0f for a bone the mask does not name — the same answer a RESOLVED
	// Flux_BoneMask gives for it, so the editor and the runtime agree.
	float GetBoneWeight(const std::string& strBoneName) const;
	bool HasBone(const std::string& strBoneName) const;
	u_int GetEntryCount() const { return m_xWorkingMask.GetEntryCount(); }
	bool HasAvatarMask() const { return m_xWorkingMask.HasAvatarMask(); }

	//-------------------------------------------------------------------------
	// Undo. ★ THE DOCUMENT OWNS ITS OWN STACK, exactly as the clip and
	// controller documents do and for the same two reasons: a scene load
	// Clear()s the shared editor stack (a mask edit has nothing to do with a
	// scene), and the shared stack holds commands for entities a mask edit has
	// none of.
	//-------------------------------------------------------------------------

	Zenith_UndoSystem& UndoSystem() { return m_xUndoSystem; }
	bool CanUndo() { return m_xUndoSystem.CanUndo(); }
	bool CanRedo() { return m_xUndoSystem.CanRedo(); }
	void Undo();
	void Redo();
	u_int GetUndoStackSize() { return m_xUndoSystem.GetUndoStackSize(); }
	u_int GetRedoStackSize() { return m_xUndoSystem.GetRedoStackSize(); }

	// MANY EDITS, ONE UNDO STEP — the same bracket the other two documents
	// carry, for the same reason (Zenith_UndoSystem has no grouping of any
	// kind). Nesting is refused.
	bool BeginCompound();
	// bKeep == false UNDOES everything collected and discards the group.
	// Returns true iff a command was pushed.
	bool EndCompound(const char* szDescription, bool bKeep = true);
	bool IsCompoundOpen() const { return m_pxOpenCompound != nullptr; }

	//-------------------------------------------------------------------------
	// Mutation. Each: validates, mutates, marks dirty and pushes ONE undo
	// command. A refusal does none of those and changes nothing.
	//
	// ★ THESE ARE ALL ASSIGNMENTS, so they follow the animator-controller
	// document's assignment rule verbatim: the bool answers "IS THE VALUE WHAT
	// YOU ASKED FOR", not "did I change something". Asking for the weight a bone
	// already carries is the caller's intent SATISFIED — it returns TRUE,
	// mutates nothing and pushes NO undo entry. "One edit, one undo step" is
	// unaffected and is the invariant to assert on: a no-op is not an edit, so
	// it contributes zero steps.
	//
	// That side was chosen for the reason SetDefaultState was: an authoring
	// recipe that paints a subtree to 1.0 and then re-states one of its bones is
	// completely ordinary, and under a "false means nothing changed" reading it
	// would trip the automation's checked wrapper at boot on a step that did
	// exactly what it was asked.
	//-------------------------------------------------------------------------

	// The weight is CLAMPED to [0,1] before anything else, matching
	// Flux_BoneMask::SetBoneWeight, so the document cannot hold a value the
	// resolve would quietly change. A non-finite weight is REFUSED (false), not
	// clamped: it is a caller's arithmetic slip and clamping it to 0 or 1 would
	// turn a bug into an authored value.
	bool SetBoneWeight(const std::string& strBoneName, float fWeight);

	// Set strRootBoneName AND every descendant of it to fWeight, as ONE undo
	// step through the compound. False for a bone the rig does not carry, a
	// non-finite weight, or a closed document.
	//
	// ★ THE RIG IS A PARAMETER. A mask is skeleton-SCOPED but not
	// skeleton-BOUND: the same file is meant to be opened against whichever rig
	// the dope sheet is previewing, and a hierarchy cached at Open would answer
	// with the previous rig's parents after the preview changed.
	//
	// A bone with no descendants is still one edit, not a refusal.
	bool SetSubtreeWeight(const Zenith_SkeletonAsset& xSkeleton, const std::string& strRootBoneName, float fWeight);

	// D47's flag. ASSIGNMENT, like the rest.
	bool SetHasAvatarMask(bool bHasAvatarMask);

	// Drop a bone's entry entirely. A REMOVAL, so it answers "did I remove one"
	// and returns false for a bone the mask does not name. Note the difference
	// from SetBoneWeight(name, 0): an entry at weight zero is still an entry,
	// still round-trips, and is what makes an explicitly-zeroed bone visible in
	// the file as a decision somebody made.
	bool RemoveBone(const std::string& strBoneName);

private:
	//-------------------------------------------------------------------------
	// The undo commands are the ONLY callers of the primitives below. They must
	// not push commands of their own (a redo that pushed would grow the stack it
	// is being replayed from).
	//-------------------------------------------------------------------------
	friend class Zenith_BoneMaskCommand_Weight;
	friend class Zenith_BoneMaskCommand_HasAvatar;
	// Zenith_BoneMaskCommand_Compound is deliberately NOT a friend: it performs
	// no edit of its own.

	// bHasEntry false means "there was no entry for this bone" — which is what
	// an undo of the FIRST weight ever written to a bone has to restore, and is
	// not the same state as an entry at 0.
	void ApplyWeight(const std::string& strBoneName, float fWeight, bool bHasEntry);
	void ApplyHasAvatarMask(bool bHasAvatarMask);
	void MarkDirty() { m_bDirty = true; }

	void PushCommand(Zenith_UndoCommand* pxCommand);
	void ResetToClosed();

	static bool HashFileContents(const std::string& strResolvedPath, u_int64& ulOutHash);
	static u_int64 HashBytes(const void* pData, u_int64 ulSize);
	bool WriteWorkingMaskToFile(const std::string& strResolvedPath, u_int64& ulOutHash) const;
	// Copies the working content onto the LIVE asset, so a controller built after
	// a save resolves what was saved without a ForceUnload (which ignores
	// refcounts) and without a second parse of the file we just wrote.
	void RefreshLiveAssetFromWorkingMask() const;

	Zenith_BoneMaskAsset m_xWorkingMask;
	std::string m_strAssetPath;      // normalized, prefixed where possible
	std::string m_strResolvedPath;   // the filesystem path saves go to

	Zenith_UndoSystem m_xUndoSystem;
	Zenith_BoneMaskCommand_Compound* m_pxOpenCompound = nullptr;

	u_int64 m_ulRecordedFileHash = 0;
	bool m_bHasRecordedFile = false;
	bool m_bOpen = false;
	bool m_bDirty = false;
};

//=============================================================================
// The document's OWN undo commands.
//
// ★ THEY LIVE IN THIS FILE RATHER THAN IN Zenith_EditorAnimCommands.h, and that
// is not a style choice: every command there takes a Zenith_AnimationDocument*
// and would have to be widened to a second document type to be reused here,
// which is a change to a file this unit does not own and a template parameter
// nothing else wants. There are two leaves and a group; the whole thing is
// smaller than the argument for sharing.
//
// ★ THE DOCUMENT POINTER CANNOT DANGLE, for Zenith_EditorAnimCommands' reason:
// the ONLY stack these are pushed to is that document's own m_xUndoSystem, and
// both Close() and the destructor Clear() it — which deletes every command in
// it. A command therefore cannot outlive the document it points at.
//
// ★ RECORDED, NEVER EXECUTED, ON THE WAY IN. The document has already performed
// the edit (it is the only writer), so Zenith_UndoSystem::Record pushes the
// already-applied command and Execute() only ever runs on a REDO.
//=============================================================================

class Zenith_BoneMaskCommandBase : public Zenith_UndoCommand
{
public:
	Zenith_BoneMaskCommandBase(Zenith_BoneMaskDocument* pxDocument, const char* szDescription);

	const char* GetDescription() const override { return m_strDescription.c_str(); }

protected:
	Zenith_BoneMaskDocument* m_pxDocument = nullptr;
	std::string m_strDescription;
};

//-----------------------------------------------------------------------------
// One bone's entry moved from (old weight, had an entry) to (new weight, has an
// entry). Both a weight edit and a RemoveBone are this command; a removal is
// simply the case where the "after" side has no entry.
//
// ★ "HAS AN ENTRY" IS PART OF THE STATE ON BOTH SIDES, and is why this is not
// two floats. A mask with no entry for a bone and a mask with an entry at 0.0
// answer GetBoneWeight identically and SERIALIZE DIFFERENTLY — one writes a row,
// the other does not. Undoing the first weight ever painted onto a bone has to
// REMOVE the row, or the file grows an entry the user undid; and undoing a
// removal has to put the row back rather than write a zero.
//-----------------------------------------------------------------------------
class Zenith_BoneMaskCommand_Weight : public Zenith_BoneMaskCommandBase
{
public:
	Zenith_BoneMaskCommand_Weight(Zenith_BoneMaskDocument* pxDocument, const std::string& strBoneName,
		float fOldWeight, bool bHadEntry, float fNewWeight, bool bHasEntry, const char* szDescription);

	void Execute() override;
	void Undo() override;

private:
	std::string m_strBoneName;
	float m_fOldWeight;
	bool m_bHadEntry;
	float m_fNewWeight;
	bool m_bHasEntry;
};

//-----------------------------------------------------------------------------
// D47's flag.
//-----------------------------------------------------------------------------
class Zenith_BoneMaskCommand_HasAvatar : public Zenith_BoneMaskCommandBase
{
public:
	Zenith_BoneMaskCommand_HasAvatar(Zenith_BoneMaskDocument* pxDocument, bool bOld, bool bNew);

	void Execute() override;
	void Undo() override;

private:
	bool m_bOld;
	bool m_bNew;
};

//-----------------------------------------------------------------------------
// MANY EDITS, ONE UNDO STEP.
//
// ★ THE ORDER IS FORWARD ON EXECUTE AND REVERSE ON UNDO, matching
// Zenith_AnimCommand_Compound. Nothing here can collide the way a multi-key
// retime can, but a subtree paint may legitimately touch one bone twice (a
// re-stated root, say), and only the exact inverse order puts the earlier value
// back.
//-----------------------------------------------------------------------------
class Zenith_BoneMaskCommand_Compound : public Zenith_BoneMaskCommandBase
{
public:
	Zenith_BoneMaskCommand_Compound(Zenith_BoneMaskDocument* pxDocument, const char* szDescription);
	// Deletes every child it owns — the compound is the sole owner from Adopt on.
	~Zenith_BoneMaskCommand_Compound() override;

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

// Force-link anchor, the Zenith_AnimControllerDocument_ForceLink idiom.
//
// ★ CALLED FROM Zenith_EditorPanel_Animation'S CONSTRUCTOR, not from
// Zenith_Editor::Initialise where its two siblings are called — this unit does
// not own Zenith_Editor.cpp, and the panel is the only thing that holds one of
// these anyway. What it buys is what it buys everywhere: an .obj the linker
// never pulls in takes its ZENITH_TEST registrars with it, the unit count moves
// by zero, and nothing goes red to say so.
bool Zenith_BoneMaskDocument_ForceLink();

#endif // ZENITH_TOOLS
