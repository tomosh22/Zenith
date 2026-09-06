#pragma once

#ifdef ZENITH_TOOLS

#include "Editor/Zenith_UndoSystem.h"
#include "Flux/MeshAnimation/Flux_AnimationClip.h"
#include "AssetHandling/Zenith_AnimationAsset.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "Collections/Zenith_Vector.h"
#include "Collections/Zenith_HashMap.h"
#include "Maths/Zenith_Maths.h"
#include <string>

// Defined in Editor/Zenith_EditorAnimCommands.h. Forward-declared rather than
// included: that header includes THIS one, and the document only needs to hold
// a pointer to an open group (see BeginCompound).
class Zenith_AnimCommand_Compound;

//=============================================================================
// Zenith_AnimationDocument (WU-2.2) — the editable WORKING COPY of one .zanim,
// and the ONLY writer of it.
//
// ★ THE DOCUMENT OWNS A DEEP COPY, NOT THE LIVE CLIP (D20). Open() copies the
// asset's Flux_AnimationClip into m_xWorkingClip and every edit lands there.
// Editing the live clip in place would push half-finished keyframe state into
// whatever controller is sampling it — an editor drag would be visible to
// gameplay one frame at a time — and there would be nothing left to Discard
// back to. The live asset only ever moves at SAVE, and it moves through
// Zenith_AnimationAsset::ReloadFromDisk (WU-2.1), which replaces the clip's
// CONTENTS in place so every borrowed clip pointer keeps working.
//
// ★ EVERY EDIT GOES THROUGH A DOCUMENT METHOD. Nothing else may hold a mutable
// reference to the working clip — GetClip() is const — because each verb has
// three obligations beyond the mutation itself: keep the stable-ID maps
// coherent, mark the document dirty, and push an undo command. A caller that
// reached the clip directly would silently skip all three, and the first
// symptom would be a dope-sheet selection resolving to the wrong key.
//=============================================================================

//-----------------------------------------------------------------------------
// ★ A KEY IS ADDRESSED BY A STABLE ID, NEVER BY AN INDEX (D24).
//
// A keyframe index is not an identity: retiming one key past another REORDERS
// the track, so the index a selection (or a queued undo command) is holding
// starts naming a different key with no event to observe. Before/after
// snapshots cannot carry the difference either — they record the VALUES, and
// the values are what stayed the same.
//
// So the document hands out a u_int ID per key, unique within its track,
// allocated monotonically from a per-document counter and NEVER REUSED for the
// lifetime of the document object (the counter is not reset by Open, Save As
// or Discard). It is deliberately NOT SERIALIZED: it is a session identity for
// the editor's selection and undo stack, and writing it into the .zanim would
// invent a second authority for "which key is this" that a re-bake would
// immediately contradict.
//
// uINVALID_ANIM_KEY_ID is 0, so a zero-initialised id is never a live key.
//-----------------------------------------------------------------------------
constexpr u_int uINVALID_ANIM_KEY_ID = 0u;

// Returned by the index lookups when an id no longer resolves (the key was
// removed, or the id belongs to another track).
constexpr u_int uINVALID_ANIM_KEY_INDEX = 0xFFFFFFFFu;

//-----------------------------------------------------------------------------
// Which TRACK an edit addresses: one bone channel's position/rotation/scale
// array, or one of root motion's two delta arrays.
//
// Bone name + track, rather than a channel pointer, because a channel is
// DELETED when its last key goes (D14) and re-created by the undo — a pointer
// would not survive its own undo step.
//-----------------------------------------------------------------------------
struct Zenith_AnimTrackId
{
	std::string m_strBoneName;                            // ignored when m_bRootMotion
	Flux_AnimTrack m_eTrack = FLUX_ANIM_TRACK_POSITION;
	bool m_bRootMotion = false;

	static Zenith_AnimTrackId Bone(const std::string& strBoneName, Flux_AnimTrack eTrack);
	// Root motion has no scale track; FLUX_ANIM_TRACK_SCALE here is refused by
	// every document verb (assert + failure), matching Flux_RootMotion (D16).
	static Zenith_AnimTrackId RootMotion(Flux_AnimTrack eTrack);

	bool operator==(const Zenith_AnimTrackId& xOther) const;
	bool operator!=(const Zenith_AnimTrackId& xOther) const { return !(*this == xOther); }
};

//-----------------------------------------------------------------------------
// One keyframe VALUE, whichever kind the track holds.
//
// A position/scale key is a Vector3 and a rotation key is a Quat; the mutators
// on Flux_BoneChannel overload on exactly that difference. The undo commands,
// though, have to STORE a value without knowing which track they will be
// replayed against at construction time, so they store this and the document
// asserts the tag against the track on the way in.
//-----------------------------------------------------------------------------
struct Zenith_AnimKeyValue
{
	Zenith_Maths::Vector3 m_xVector = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Quat m_xQuat = Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);
	bool m_bIsRotation = false;

	static Zenith_AnimKeyValue FromVector(const Zenith_Maths::Vector3& xValue);
	static Zenith_AnimKeyValue FromQuat(const Zenith_Maths::Quat& xRotation);
};

//-----------------------------------------------------------------------------
// Open / Save / Close results. Each refusal is its OWN value rather than a
// bare false, because the panel turns two of them into prompts and one of them
// into a promotion offer — and a bool cannot tell those apart.
//-----------------------------------------------------------------------------
enum Zenith_AnimDocOpenResult
{
	ZENITH_ANIMDOC_OPEN_OK,
	// The path did not resolve to a loaded animation asset holding a clip.
	ZENITH_ANIMDOC_OPEN_FAILED_NO_ASSET,
	// D21: the clip is GENERATED — rewritten in full on every tools boot, so an
	// in-place edit would be silently discarded by the next run. The panel
	// offers PromoteToAuthoredOverride() instead.
	ZENITH_ANIMDOC_OPEN_REFUSED_GENERATED,
	// This document already holds unsaved edits. Save, Discard or
	// CloseDiscardingChanges first.
	ZENITH_ANIMDOC_OPEN_REFUSED_DIRTY,
};

enum Zenith_AnimDocSaveResult
{
	ZENITH_ANIMDOC_SAVE_OK,
	ZENITH_ANIMDOC_SAVE_FAILED_NO_DOCUMENT,
	// The bytes on disk afterwards are not the bytes we wrote.
	ZENITH_ANIMDOC_SAVE_FAILED_WRITE,
	// ★ The file changed underneath us since Open/the last Save. NOTHING was
	// written. SaveOverwritingExternal() proceeds anyway.
	ZENITH_ANIMDOC_SAVE_CONFLICT_EXTERNAL,
};

enum Zenith_AnimDocCloseResult
{
	ZENITH_ANIMDOC_CLOSE_OK,
	// Unsaved edits. The document is UNCHANGED and still open; the panel raises
	// the prompt and then calls Save() or CloseDiscardingChanges().
	ZENITH_ANIMDOC_CLOSE_REFUSED_DIRTY,
};

//=============================================================================
// The document.
//=============================================================================
class Zenith_AnimationDocument
{
public:
	Zenith_AnimationDocument() = default;
	~Zenith_AnimationDocument();

	// ★ NON-COPYABLE, and that is load-bearing rather than incidental. Every
	// undo command this document pushed holds a Zenith_AnimationDocument* back
	// at it; a copy would produce a second document whose stack is full of
	// commands pointing at the FIRST one.
	Zenith_AnimationDocument(const Zenith_AnimationDocument&) = delete;
	Zenith_AnimationDocument& operator=(const Zenith_AnimationDocument&) = delete;

	//-------------------------------------------------------------------------
	// Lifecycle
	//-------------------------------------------------------------------------

	// Acquire the asset at strAssetPath (prefixed or plain filesystem path),
	// deep-copy its clip into the working copy, allocate fresh key/event ids and
	// record the file's fingerprint. Refuses a generated clip (D21) and refuses
	// to throw away unsaved edits.
	Zenith_AnimDocOpenResult Open(const std::string& strAssetPath);

	// D21's escape hatch. Copies the GENERATED clip at strSourceAssetPath to the
	// authored-override path (see ResolveAuthoredOverridePath), clears
	// m_bGenerated on the copy, writes it with the envelope and opens THAT. The
	// source file is not touched — the bake keeps owning it, and keeps rewriting
	// it, and the override is what the editor and the game then read.
	Zenith_AnimDocOpenResult PromoteToAuthoredOverride(const std::string& strSourceAssetPath);

	// Refuses while dirty (the panel prompts, then calls Save or the forced
	// close). On success the working copy is dropped, the asset handle released
	// and the undo history CLEARED — which is what guarantees no command
	// outlives the document it points at.
	Zenith_AnimDocCloseResult Close();
	void CloseDiscardingChanges();

	// Re-copy the working clip from the live asset, throwing every unsaved edit
	// away. The undo history is cleared with it: every command addresses key ids
	// that this re-copy has just retired.
	bool DiscardChanges();

	// Write the working copy back to the document's own path, then reload the
	// LIVE asset from it so borrowed clip pointers observe the saved content.
	// Refuses with ZENITH_ANIMDOC_SAVE_CONFLICT_EXTERNAL when the file changed
	// underneath. Undo history SURVIVES a save (undoing past a save is a normal
	// thing to want, and it simply makes the document dirty again).
	Zenith_AnimDocSaveResult Save();
	Zenith_AnimDocSaveResult SaveOverwritingExternal();

	// Write to a NEW path and re-target the document at it. Undo history is
	// cleared: the commands describe edits to the file that was, and the
	// document is no longer pointing at it.
	Zenith_AnimDocSaveResult SaveAs(const std::string& strNewAssetPath);

	//-------------------------------------------------------------------------
	// State
	//-------------------------------------------------------------------------

	bool IsOpen() const { return m_bOpen; }
	bool IsDirty() const { return m_bDirty; }
	const std::string& GetAssetPath() const { return m_strAssetPath; }
	// The working copy. CONST: the document is the only writer (see the header
	// note above).
	const Flux_AnimationClip& GetClip() const { return m_xWorkingClip; }
	// The LIVE asset this document saves back into, or nullptr when closed.
	Zenith_AnimationAsset* GetAsset() const;

	// ★ Reads the file and compares its content hash against the one recorded at
	// Open / the last Save. A hash rather than a write time, because a write
	// time is a clock and the two things being compared are written by different
	// processes; a hash answers the question that is actually being asked.
	// Costs one file read, so call it on demand (panel focus, before a save) —
	// not once per frame.
	bool HasExternalModification() const;

	//-------------------------------------------------------------------------
	// Undo. ★ THE DOCUMENT OWNS ITS OWN STACK, and that is the whole reason no
	// command can dangle: the commands hold a raw Zenith_AnimationDocument*, the
	// only stack they are ever pushed to is this member, and Close() and the
	// destructor both Clear() it (which deletes every command). There is no
	// window in which a live command outlives its target.
	//
	// The shared editor undo system would have been the wrong home twice over: a
	// scene load Clear()s it (an animation edit has nothing to do with a scene),
	// and it holds commands for entities that a keyframe edit has none of.
	//-------------------------------------------------------------------------

	Zenith_UndoSystem& UndoSystem() { return m_xUndoSystem; }
	bool CanUndo() { return m_xUndoSystem.CanUndo(); }
	bool CanRedo() { return m_xUndoSystem.CanRedo(); }
	void Undo();
	void Redo();
	u_int GetUndoStackSize() { return m_xUndoSystem.GetUndoStackSize(); }
	u_int GetRedoStackSize() { return m_xUndoSystem.GetRedoStackSize(); }

	//-------------------------------------------------------------------------
	// MANY EDITS, ONE UNDO STEP (WU-3.3).
	//
	// ★ Zenith_UndoSystem HAS NO GROUPING — it is a flat LIFO with no
	// transaction of any kind — so the boundary has to live here, where the
	// commands are pushed. Between BeginCompound() and EndCompound() every
	// command the verbs above would have Recorded is ADOPTED by one
	// Zenith_AnimCommand_Compound instead, and that single command is what
	// reaches the stack. A dope-sheet drag over eleven keys is therefore ONE
	// Ctrl+Z, not eleven, and the ten intermediate states the user never saw are
	// never stops on the way back.
	//
	// ★ THE CALLER STILL USES THE ORDINARY VERBS. Nothing about InsertKey /
	// SetKeyTime / RemoveKey changes inside a group: they validate, mutate, mark
	// dirty and push exactly as before. That is deliberate — an operation that
	// had to call a second, group-aware API would be a second mutation path, and
	// the whole point of this class is that there is one.
	//
	// Nesting is refused (asserted): a group inside a group would make "one undo
	// step" mean two different things depending on who called first.
	bool BeginCompound();

	// Closes the group.
	//
	//   bKeep == true  — push it as one command IF it collected anything. An
	//                    EMPTY group is deleted and NOTHING is pushed, so an
	//                    operation that turned out to be a no-op leaves no undo
	//                    entry to press Ctrl+Z through.
	//   bKeep == false — UNDO everything it collected (in reverse) and discard
	//                    it. What a multi-key operation calls when a mutation
	//                    refuses halfway: the partial application never reaches
	//                    the stack and never reaches the user.
	//
	// Returns true iff a command was pushed. NOTE that a rollback leaves the
	// document DIRTY even though the content is back where it started — the
	// verbs marked it, and re-deriving "is this file still identical" from a
	// command trail would be a second authority on a question the content hash
	// already answers.
	bool EndCompound(const char* szDescription, bool bKeep = true);

	bool IsCompoundOpen() const { return m_pxOpenCompound != nullptr; }

	//-------------------------------------------------------------------------
	// Track / key inspection (what a dope sheet draws from)
	//-------------------------------------------------------------------------

	// Bone names in ascending order — the same total order
	// Flux_AnimationClip::WriteToDataStream imposes (D5), so a row list and a
	// file diff agree.
	void GetBoneNamesSorted(Zenith_Vector<std::string>& axOut) const;

	bool TrackExists(const Zenith_AnimTrackId& xTrack) const;
	u_int GetKeyCount(const Zenith_AnimTrackId& xTrack) const;
	u_int GetKeyIdAtIndex(const Zenith_AnimTrackId& xTrack, u_int uIndex) const;
	u_int GetKeyIndexForId(const Zenith_AnimTrackId& xTrack, u_int uKeyId) const;
	// The id of the key at fTimeSeconds (within fANIM_TIME_EPSILON), or
	// uINVALID_ANIM_KEY_ID when that slot is free.
	u_int FindKeyIdAtTime(const Zenith_AnimTrackId& xTrack, float fTimeSeconds) const;
	bool GetKeyTime(const Zenith_AnimTrackId& xTrack, u_int uKeyId, float& fOutTimeSeconds) const;
	bool GetKeyValue(const Zenith_AnimTrackId& xTrack, u_int uKeyId, Zenith_AnimKeyValue& xOut) const;

	//-------------------------------------------------------------------------
	// Key mutation. Each: validates, calls the WU-1.3 mutator, re-maps the ids,
	// marks dirty and pushes ONE undo command. A refusal does none of those.
	//-------------------------------------------------------------------------

	// Returns the id of the key at fTimeSeconds afterwards, or
	// uINVALID_ANIM_KEY_ID on refusal.
	//
	// ★ D25: ON AN OCCUPIED TIME THIS IS A VALUE EDIT. Flux_BoneChannel::
	// InsertKeyframeAt replaces the value in place and keeps the slot, so the
	// existing key's ID is PRESERVED and the command pushed is a value edit —
	// not an insert whose undo would delete a key the user never created.
	u_int InsertKey(const Zenith_AnimTrackId& xTrack, float fTimeSeconds, const Zenith_AnimKeyValue& xValue);
	u_int InsertKey(const Zenith_AnimTrackId& xTrack, float fTimeSeconds, const Zenith_Maths::Vector3& xValue);
	u_int InsertKey(const Zenith_AnimTrackId& xTrack, float fTimeSeconds, const Zenith_Maths::Quat& xRotation);

	bool RemoveKey(const Zenith_AnimTrackId& xTrack, u_int uKeyId);

	// Refused (nothing changes, no command) when another key already sits within
	// fANIM_TIME_EPSILON of the new time — no silent merge (D11).
	bool SetKeyTime(const Zenith_AnimTrackId& xTrack, u_int uKeyId, float fNewTimeSeconds);

	bool SetKeyValue(const Zenith_AnimTrackId& xTrack, u_int uKeyId, const Zenith_AnimKeyValue& xValue);
	bool SetKeyValue(const Zenith_AnimTrackId& xTrack, u_int uKeyId, const Zenith_Maths::Vector3& xValue);
	bool SetKeyValue(const Zenith_AnimTrackId& xTrack, u_int uKeyId, const Zenith_Maths::Quat& xRotation);

	//-------------------------------------------------------------------------
	// Clip-level
	//-------------------------------------------------------------------------

	float GetDuration() const { return m_xWorkingClip.GetDuration(); }
	bool SetDuration(float fDurationSeconds);

	//-------------------------------------------------------------------------
	// Events. ★ SAME STABLE-ID SCHEME AS KEYS, and for the same reason:
	// Flux_AnimationClip::AddEvent SORTS the event list, so an event's index
	// moves whenever any other event is retimed.
	//-------------------------------------------------------------------------

	u_int GetEventCount() const;
	u_int GetEventIdAtIndex(u_int uIndex) const;
	u_int GetEventIndexForId(u_int uEventId) const;
	bool GetEvent(u_int uEventId, Flux_AnimationEvent& xOut) const;

	// fNormalizedTime is a [0,1] fraction of the clip (D4), NOT seconds.
	u_int AddEvent(const std::string& strName, float fNormalizedTime, const Zenith_Maths::Vector4& xPayload);
	bool RemoveEvent(u_int uEventId);
	bool SetEventTime(u_int uEventId, float fNormalizedTime);
	bool SetEventName(u_int uEventId, const std::string& strName);
	bool SetEventPayload(u_int uEventId, const Zenith_Maths::Vector4& xPayload);

	//-------------------------------------------------------------------------
	// Authored-override location
	//-------------------------------------------------------------------------

	// PURE. The promoted path for a source asset path, expressed as an asset
	// path: the source's ROOT PREFIX is kept and "Authored/" is inserted
	// directly under it, preserving the rest of the relative path.
	//
	//   engine:Meshes/StickFigure/Walk.zanim -> engine:Authored/Meshes/StickFigure/Walk.zanim
	//   game:Anims/Run.zanim                 -> game:Authored/Anims/Run.zanim
	//
	// So engine sources land in Zenith/Assets/Authored/... and game sources in
	// Games/<Game>/Assets/Authored/... with no game name written down anywhere.
	// The SUBDIRECTORY is preserved rather than flattened to a leaf name because
	// two generated sets routinely hold a clip called "Walk", and a flattened
	// name would have one silently overwrite the other.
	//
	// Returns an empty string for a path with no recognised root prefix — see
	// ResolveAuthoredOverridePath for what happens then.
	static std::string BuildAuthoredAssetPath(const std::string& strSourceAssetPath);

	// The FILESYSTEM path PromoteToAuthoredOverride writes. Normally
	// ResolvePath(BuildAuthoredAssetPath(source)); when an authored-root
	// override is set it is <override>/<leaf name>, which is what lets a unit
	// test promote into a temp directory instead of the tracked asset tree.
	std::string ResolveAuthoredOverridePath(const std::string& strSourceAssetPath) const;

	// Empty (the default) means "derive the root from the source's prefix".
	void SetAuthoredRootOverride(const std::string& strAbsoluteDirectory);
	const std::string& GetAuthoredRootOverride() const { return m_strAuthoredRootOverride; }

private:
	//-------------------------------------------------------------------------
	// The undo commands are the ONLY things allowed at the non-recording
	// primitives below. They must not push commands of their own (a redo that
	// pushed would grow the stack it is being replayed from), and they must be
	// able to re-insert a key under its ORIGINAL id, which the public verbs
	// deliberately cannot do.
	//-------------------------------------------------------------------------
	friend class Zenith_AnimCommand_KeyInsert;
	friend class Zenith_AnimCommand_KeyRemove;
	friend class Zenith_AnimCommand_KeyTime;
	friend class Zenith_AnimCommand_KeyValue;
	friend class Zenith_AnimCommand_Duration;
	friend class Zenith_AnimCommand_EventAdd;
	friend class Zenith_AnimCommand_EventRemove;
	friend class Zenith_AnimCommand_EventEdit;
	// ★ Zenith_AnimCommand_Compound is deliberately NOT in this list. It performs
	// no edit of its own — it is a bracket around the commands above — so it needs
	// none of the primitives, and adding it would widen the private surface for
	// nothing.

	// uForcedKeyId != uINVALID_ANIM_KEY_ID re-uses that id (the undo of a
	// remove); otherwise a fresh one is allocated.
	u_int ApplyInsertKey(const Zenith_AnimTrackId& xTrack, float fTimeSeconds, const Zenith_AnimKeyValue& xValue, u_int uForcedKeyId);
	bool ApplyRemoveKey(const Zenith_AnimTrackId& xTrack, u_int uKeyId);
	bool ApplySetKeyTime(const Zenith_AnimTrackId& xTrack, u_int uKeyId, float fNewTimeSeconds);
	bool ApplySetKeyValue(const Zenith_AnimTrackId& xTrack, u_int uKeyId, const Zenith_AnimKeyValue& xValue);
	bool ApplySetDuration(float fDurationSeconds);
	u_int ApplyAddEvent(const Flux_AnimationEvent& xEvent, u_int uForcedEventId);
	bool ApplyRemoveEvent(u_int uEventId);
	bool ApplySetEvent(u_int uEventId, const Flux_AnimationEvent& xEvent);
	void MarkDirty() { m_bDirty = true; }

	//-------------------------------------------------------------------------
	// Per-track id bookkeeping.
	//
	// m_auIdByIndex is the AUTHORITY — it is exactly as long as the track and is
	// edited in lockstep with it (insert at the sorted index, erase at the
	// removed index, move the entry with the key on a retime). m_xIndexById is a
	// pure lookup accelerator, REBUILT from that vector after every mutation, so
	// the two cannot drift: there is one place that writes the map and it copies
	// the authority wholesale.
	//-------------------------------------------------------------------------
	struct TrackIds
	{
		Zenith_Vector<u_int> m_auIdByIndex;
		Zenith_HashMap<u_int, u_int> m_xIndexById;
	};

	static std::string MakeTrackKey(const Zenith_AnimTrackId& xTrack);
	static bool IsTrackAddressable(const Zenith_AnimTrackId& xTrack);

	TrackIds& GetOrAddTrackIds(const Zenith_AnimTrackId& xTrack);
	const TrackIds* FindTrackIds(const Zenith_AnimTrackId& xTrack) const;
	static void RebuildIndexById(TrackIds& xIds);
	static void InsertIdAt(Zenith_Vector<u_int>& auIds, u_int uIndex, u_int uId);

	// Throws away every id map and allocates a fresh, dense set from the working
	// clip. Called by Open / Promote / Discard — anything that replaces the
	// whole clip.
	void RebuildAllIdsFromWorkingClip();

	// The clip's event list re-derived into m_auEventIdByIndex after a rewrite,
	// by greedily matching each stored event against the ordered list the
	// document intended. Identical events are interchangeable by construction,
	// so a greedy match is exact.
	struct EventRecord
	{
		Flux_AnimationEvent m_xEvent;
		u_int m_uId = uINVALID_ANIM_KEY_ID;
	};
	void ReadEventRecords(Zenith_Vector<EventRecord>& axOut) const;
	void WriteEventRecords(const Zenith_Vector<EventRecord>& axRecords);

	u_int AllocateKeyId() { return m_uNextKeyId++; }
	u_int AllocateEventId() { return m_uNextEventId++; }

	void PushCommand(Zenith_UndoCommand* pxCommand);
	void ResetToClosed();

	// The fingerprint of the file as of Open / the last successful Save.
	static bool HashFileContents(const std::string& strResolvedPath, u_int64& ulOutHash);
	static u_int64 HashBytes(const void* pData, u_int64 ulSize);
	bool WriteWorkingClipToFile(const std::string& strResolvedPath, u_int64& ulOutHash) const;

	Flux_AnimationClip m_xWorkingClip;
	AnimationHandle m_xAsset;
	std::string m_strAssetPath;         // normalized, prefixed where possible
	std::string m_strResolvedPath;      // the filesystem path the saves go to
	std::string m_strAuthoredRootOverride;

	Zenith_HashMap<std::string, TrackIds> m_xTrackIds;
	Zenith_Vector<u_int> m_auEventIdByIndex;
	Zenith_HashMap<u_int, u_int> m_xEventIndexById;

	Zenith_UndoSystem m_xUndoSystem;

	// The group currently collecting pushed commands, or null. Owned while open;
	// ownership passes to m_xUndoSystem (or to a delete) at EndCompound, and
	// ResetToClosed discards an abandoned one rather than leaking it.
	Zenith_AnimCommand_Compound* m_pxOpenCompound = nullptr;

	// ★ NOT reset by Open / Save As / Discard. An id retired by one of those
	// must never come back inside the same document object, or a selection that
	// survived the reload would resolve to a key it never named.
	u_int m_uNextKeyId = 1;
	u_int m_uNextEventId = 1;

	u_int64 m_ulRecordedFileHash = 0;
	bool m_bHasRecordedFile = false;
	bool m_bOpen = false;
	bool m_bDirty = false;
};

// Force-link anchor: Zenith_Editor::Initialise calls this so this TU (and the
// commands TU it in turn anchors) survives /OPT:REF - the ZENITH_TEST registrars
// at the bottom of the .cpp only run if the .obj is linked (the MSVC dead-strip
// pitfall). Nothing references either TU for real until the dope-sheet panel
// lands; until then this one call is what keeps their units in the gate.
bool Zenith_AnimationDocument_ForceLink();

#endif // ZENITH_TOOLS
