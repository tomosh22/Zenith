#pragma once

#ifdef ZENITH_TOOLS

#include "Editor/Zenith_UndoSystem.h"
#include "Flux/MeshAnimation/Flux_AnimatorControllerDef.h"
#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"
#include <string>

class Zenith_AnimatorControllerAsset;
// Defined in Editor/Zenith_EditorAnimCtrlCommands.h. Forward-declared rather
// than included: that header includes THIS one, and the document only needs to
// hold a pointer to an open group (see BeginCompound).
class Zenith_AnimCtrlCommand_Compound;

//=============================================================================
// Zenith_AnimControllerDocument (WU-6.5) — the editable WORKING COPY of one
// .zanimctrl, and the ONLY writer of it.
//
// ★ IT IS Zenith_AnimationDocument'S SHAPE, DELIBERATELY. Open deep-copies the
// asset's Flux_AnimatorControllerDef into m_xWorkingDef; every edit lands
// there; the live asset only moves at SAVE. Editing the asset's def in place
// would push half-finished graph state into whatever
// Flux_AnimationController::BuildFromControllerDef reads next, and there would
// be nothing left to Discard back to.
//
// ★ EVERY EDIT GOES THROUGH A DOCUMENT METHOD, for the same three reasons the
// clip document has: only the document can mark itself dirty, push exactly one
// undo command, and keep the def's own invariants (a removed state's inbound
// transitions, a renamed state's referrers). GetDef() is const.
//
// ★ THE DOCUMENT HOLDS NO OWNING ASSET HANDLE, and that is a link-level fact
// rather than a preference: Zenith_AssetHandle<T> is explicitly instantiated in
// AssetHandling/Zenith_AssetHandle.cpp and Zenith_AnimatorControllerAsset is
// NOT in that list, so a handle to one would not link. It does not need one —
// the working copy is a deep copy and the asset is reached transiently through
// Zenith_AssetRegistry::GetView at the two moments that need it (Open, and the
// in-place refresh a Save performs).
//=============================================================================

//-----------------------------------------------------------------------------
// WHICH state machine an edit addresses.
//
// A Flux_AnimatorControllerDef holds an OPTIONAL top-level machine AND N
// layers, each owning its own — and every layered game reaches its graph
// through a layer. So every verb takes a machine selector, and the selector is
// a LAYER ID rather than a layer INDEX (WU-6.3): inserting or removing a layer
// renumbers every index above it, while an id is minted once and never reused.
//
// uANIMCTRL_TOP_LEVEL_MACHINE is the def's own machine. It is deliberately the
// same 0xFFFFFFFF sentinel shape uFLUX_INVALID_LAYER_ID uses — nothing is ever
// minted with it, so it cannot collide with a real layer.
//-----------------------------------------------------------------------------
constexpr u_int uANIMCTRL_TOP_LEVEL_MACHINE = 0xFFFFFFFFu;

//-----------------------------------------------------------------------------
// What is inside a state's blend tree, as far as THIS unit is concerned.
//
// WU-6.5 authors a state whose tree is a single clip leaf; the blend-tree
// sub-graph editor is WU-7.3. A state holding anything else is REPORTED rather
// than silently flattened — an editor that "assigned a clip" to a 2D blend
// space would delete the space and every clip in it, and report success.
//-----------------------------------------------------------------------------
enum Zenith_AnimCtrlStateTreeKind : u_int
{
	// No blend tree at all — a state that has never been given a clip. Legal:
	// it poses the bind pose.
	ZENITH_ANIMCTRL_TREE_EMPTY,
	// Exactly one Flux_BlendTreeNode_Clip at the root. What this panel edits.
	ZENITH_ANIMCTRL_TREE_SINGLE_CLIP,
	// A composite, a blend space, or a container state's sub-machine. Refused
	// here, with the reason named.
	ZENITH_ANIMCTRL_TREE_COMPLEX,
};

//-----------------------------------------------------------------------------
// One parameter DECLARATION, flattened out of Flux_AnimationParameters.
//
// ★ THE DEFAULT IS THREE FIELDS AND NOT A UNION. Flux_AnimationParameters::
// Parameter overlays float/int32/bool, which is right for a live value read
// through a type-checked accessor and wrong for an undo record: a command has
// to store a value it will replay later against a declaration whose TYPE the
// intervening edits may have changed, and a union cannot say which member is
// live once it has been copied out of its owner.
//-----------------------------------------------------------------------------
struct Zenith_AnimCtrlParameterDecl
{
	std::string m_strName;
	Flux_AnimationParameters::ParamType m_eType = Flux_AnimationParameters::ParamType::Float;
	float m_fDefault = 0.0f;
	int32_t m_iDefault = 0;
	bool m_bDefault = false;

	static Zenith_AnimCtrlParameterDecl Float(const std::string& strName, float fDefault);
	static Zenith_AnimCtrlParameterDecl Int(const std::string& strName, int32_t iDefault);
	static Zenith_AnimCtrlParameterDecl Bool(const std::string& strName, bool bDefault);
	static Zenith_AnimCtrlParameterDecl Trigger(const std::string& strName);
};

//-----------------------------------------------------------------------------
// One state's OUTGOING transition list, captured whole.
//
// ★ THE UNDO UNIT FOR EVERY TRANSITION EDIT IS THE WHOLE LIST, and that is a
// correctness choice rather than a lazy one. A transition is addressed by
// INDEX, and Flux_AnimationState::AddTransition inserts by PRIORITY — so an
// add, a remove and a condition edit each move indices that a
// finer-grained command would already be holding. A list snapshot has no index
// to go stale: it is the same argument the clip document makes for stable key
// ids, answered the other way round because a transition has no identity to
// give it.
//
// m_strOwnerStateName is EMPTY for the machine's any-state transition list.
//-----------------------------------------------------------------------------
struct Zenith_AnimCtrlTransitionList
{
	std::string m_strOwnerStateName;
	Zenith_Vector<Flux_StateTransition> m_xTransitions;
};

//-----------------------------------------------------------------------------
// Open / Save / Close results. Each refusal is its own value rather than a bare
// false, because the panel turns two of them into prompts.
//-----------------------------------------------------------------------------
enum Zenith_AnimCtrlDocOpenResult : u_int
{
	ZENITH_ANIMCTRLDOC_OPEN_OK,
	// The path did not resolve to a loadable .zanimctrl.
	ZENITH_ANIMCTRLDOC_OPEN_FAILED_NO_ASSET,
	// This document already holds unsaved edits. Save, Discard or
	// CloseDiscardingChanges first.
	ZENITH_ANIMCTRLDOC_OPEN_REFUSED_DIRTY,
};

enum Zenith_AnimCtrlDocSaveResult : u_int
{
	ZENITH_ANIMCTRLDOC_SAVE_OK,
	ZENITH_ANIMCTRLDOC_SAVE_FAILED_NO_DOCUMENT,
	// The bytes on disk afterwards are not the bytes we wrote.
	ZENITH_ANIMCTRLDOC_SAVE_FAILED_WRITE,
	// ★ The file changed underneath us since Open / the last Save. NOTHING was
	// written. SaveOverwritingExternal() proceeds anyway.
	ZENITH_ANIMCTRLDOC_SAVE_CONFLICT_EXTERNAL,
};

enum Zenith_AnimCtrlDocCloseResult : u_int
{
	ZENITH_ANIMCTRLDOC_CLOSE_OK,
	// Unsaved edits. The document is UNCHANGED and still open.
	ZENITH_ANIMCTRLDOC_CLOSE_REFUSED_DIRTY,
};

//=============================================================================
// The document.
//=============================================================================
class Zenith_AnimControllerDocument
{
public:
	Zenith_AnimControllerDocument() = default;
	~Zenith_AnimControllerDocument();

	// ★ NON-COPYABLE, and load-bearing rather than incidental: every undo
	// command this document pushed holds a raw Zenith_AnimControllerDocument*
	// back at it, and a copy would produce a second document whose stack is
	// full of commands pointing at the first.
	Zenith_AnimControllerDocument(const Zenith_AnimControllerDocument&) = delete;
	Zenith_AnimControllerDocument& operator=(const Zenith_AnimControllerDocument&) = delete;

	//-------------------------------------------------------------------------
	// Lifecycle
	//-------------------------------------------------------------------------

	// Load the .zanimctrl at strAssetPath through the registry and deep-copy its
	// def into the working copy. Refuses to throw away unsaved edits.
	Zenith_AnimCtrlDocOpenResult Open(const std::string& strAssetPath);

	// Open an EMPTY controller def TARGETED at strAssetPath, whether or not a
	// file is there. This is the boot-time authoring entry point — the twin of
	// Zenith_GraphEditorPanel::OpenAssetFresh, and for the same reason: a game
	// that regenerates its controller every tools boot must not be made to
	// depend on the previous boot's output, and a recipe that appended to a
	// stale file would accumulate every revision of itself.
	//
	// ★ IT IS A SEPARATE VERB RATHER THAN A FALLBACK INSIDE Open. "The file was
	// not there" and "the path was typed wrong" are the same observation, and a
	// silent fresh-start on a typo authors a whole controller into a path
	// nothing reads.
	Zenith_AnimCtrlDocOpenResult OpenFresh(const std::string& strAssetPath);

	// Refuses while dirty (the panel prompts, then calls Save or the forced
	// close). On success the working copy is dropped and the undo history
	// CLEARED — which is what guarantees no command outlives its document.
	Zenith_AnimCtrlDocCloseResult Close();
	void CloseDiscardingChanges();

	// Re-copy the working def from the live asset, throwing every unsaved edit
	// away. The undo history goes with it.
	bool DiscardChanges();

	// Write the working copy back to the document's own path, then refresh the
	// LIVE asset's def in place so a later BuildFromControllerDef sees it.
	// Refuses with ..._CONFLICT_EXTERNAL when the file changed underneath.
	// Undo history SURVIVES a save.
	Zenith_AnimCtrlDocSaveResult Save();
	Zenith_AnimCtrlDocSaveResult SaveOverwritingExternal();

	// Write to a NEW path and re-target the document at it. Undo history is
	// cleared: the commands describe edits to the file that was.
	Zenith_AnimCtrlDocSaveResult SaveAs(const std::string& strNewAssetPath);

	//-------------------------------------------------------------------------
	// State
	//-------------------------------------------------------------------------

	bool IsOpen() const { return m_bOpen; }
	bool IsDirty() const { return m_bDirty; }
	const std::string& GetAssetPath() const { return m_strAssetPath; }
	const std::string& GetResolvedPath() const { return m_strResolvedPath; }
	// The working copy. CONST: the document is the only writer.
	const Flux_AnimatorControllerDef& GetDef() const { return m_xWorkingDef; }
	// The LIVE asset this document saves back into, or nullptr. A TRANSIENT
	// view (no refcount taken) — do not store it.
	Zenith_AnimatorControllerAsset* GetAsset() const;

	// ★ Reads the file and compares its CONTENT HASH against the one recorded
	// at Open / the last Save — a hash rather than a write time, because a write
	// time is a clock and the two things compared are written by two processes.
	// Costs one file read; call it on demand, not per frame.
	bool HasExternalModification() const;

	//-------------------------------------------------------------------------
	// Undo. ★ THE DOCUMENT OWNS ITS OWN STACK, exactly as
	// Zenith_AnimationDocument does and for the same two reasons: a scene load
	// Clear()s the shared editor stack (a controller edit has nothing to do
	// with a scene), and the shared stack holds commands for entities a graph
	// edit has none of.
	//-------------------------------------------------------------------------

	Zenith_UndoSystem& UndoSystem() { return m_xUndoSystem; }
	bool CanUndo() { return m_xUndoSystem.CanUndo(); }
	bool CanRedo() { return m_xUndoSystem.CanRedo(); }
	void Undo();
	void Redo();
	u_int GetUndoStackSize() { return m_xUndoSystem.GetUndoStackSize(); }
	u_int GetRedoStackSize() { return m_xUndoSystem.GetRedoStackSize(); }

	// MANY EDITS, ONE UNDO STEP — the same bracket the clip document carries,
	// for the same reason (Zenith_UndoSystem has no grouping of any kind).
	// Nesting is refused.
	bool BeginCompound();
	// bKeep == false UNDOES everything collected and discards the group.
	// Returns true iff a command was pushed.
	bool EndCompound(const char* szDescription, bool bKeep = true);
	bool IsCompoundOpen() const { return m_pxOpenCompound != nullptr; }

	//-------------------------------------------------------------------------
	// Machine selection — which graph the verbs below address.
	//-------------------------------------------------------------------------

	// uANIMCTRL_TOP_LEVEL_MACHINE, or a layer id the def carries. False for an
	// id no layer holds; the current selection is then unchanged.
	bool SelectMachine(u_int uLayerId);
	u_int GetSelectedMachineId() const { return m_uSelectedMachineId; }
	// Null when the selection is the top-level machine and the def has none.
	const Flux_AnimationStateMachineDef* GetSelectedMachineDef() const;

	// Layer ids in BLEND ORDER (index order), which is the order a layer
	// dropdown lists them in. The list itself is WU-7.2's; this is what the
	// machine picker needs.
	void GetLayerIds(Zenith_Vector<u_int>& auOut) const;
	bool GetLayerName(u_int uLayerId, std::string& strOut) const;

	//-------------------------------------------------------------------------
	// Inspection (what the graph draws from). All against the SELECTED machine.
	//-------------------------------------------------------------------------

	// ★ SORTED, because Zenith_HashMap iteration is SLOT order — it depends on
	// the hash distribution and the current capacity, so a node list built from
	// it would reorder itself when a state was added. A dropdown, a node layout
	// and a unit's expectations all need one total order.
	void GetStateNamesSorted(Zenith_Vector<std::string>& axOut) const;
	u_int GetStateCount() const;
	bool HasState(const std::string& strStateName) const;
	const std::string& GetDefaultStateName() const;

	bool GetStateEditorPosition(const std::string& strStateName, Zenith_Maths::Vector2& xOut) const;
	Zenith_AnimCtrlStateTreeKind GetStateTreeKind(const std::string& strStateName) const;
	// The clip NAME on a single-clip-leaf state. False for every other tree
	// kind, so a caller cannot mistake "no clip" for "a clip called nothing".
	bool GetStateClipName(const std::string& strStateName, std::string& strOut) const;

	u_int GetTransitionCount(const std::string& strFromState) const;
	bool GetTransition(const std::string& strFromState, u_int uIndex, Flux_StateTransition& xOut) const;

	void GetParameterNamesSorted(Zenith_Vector<std::string>& axOut) const;
	bool GetParameter(const std::string& strName, Zenith_AnimCtrlParameterDecl& xOut) const;

	// Controller-level, not per machine: the AddClipFromFile list a rebuild
	// resolves every state's clip NAME through.
	u_int GetClipPathCount() const;
	bool GetClipPathAt(u_int uIndex, std::string& strOut) const;

	//-------------------------------------------------------------------------
	// Mutation. Each: validates, mutates, marks dirty and pushes ONE undo
	// command. A refusal does none of those and changes nothing.
	//
	// ★ WHAT THE BOOL MEANS DEPENDS ON THE VERB'S FAMILY, AND THE SPLIT IS
	// DELIBERATE.
	//
	//   • An ASSIGNMENT — SetDefaultState, SetStateClip, SetStateEditorPosition,
	//     SetTransition{Duration,ExitTime,Interruptible} — answers "IS THE VALUE
	//     WHAT YOU ASKED FOR". Asking for the value it already holds is the
	//     caller's intent SATISFIED: it returns TRUE, mutates nothing, and
	//     pushes NO undo entry.
	//   • A CREATION or REMOVAL — AddState, RemoveState, RenameState,
	//     Add/RemoveTransition, Add/RemoveCondition, Add/RemoveParameter,
	//     Add/RemoveClipPath — answers "DID I CREATE / REMOVE ONE". A duplicate
	//     name, a duplicate path or a miss is a genuine refusal and returns
	//     FALSE, because the caller expects a NEW thing and getting an existing
	//     one with different contents is a different outcome.
	//
	// ★ THIS COST A RED TEST, AND THE FAILURE SHAPE IS WHY THE RULE IS WRITTEN
	// DOWN. SetDefaultState used to return false for "already the default", on
	// the reasoning that there was nothing to undo — but
	// Flux_AnimationStateMachineDef::AddState makes the FIRST state of a machine
	// its default, so the most natural authoring order in existence
	// (AddState("Idle"); AddState("Walk"); SetDefaultState("Idle")) hit it every
	// single time. Under AnimSmActionChecked that asserts at boot, and in a unit
	// it reads as "the entry point could not be set" — neither of which is what
	// happened.
	//
	// ★ "ONE EDIT, ONE UNDO STEP" IS UNAFFECTED, which is the reason this is the
	// right side to give: a no-op is not an edit, so it contributes ZERO steps.
	// The invariant a caller can rely on is the stack depth, not the bool.
	//-------------------------------------------------------------------------

	// Refused for an empty name and for a name the machine already has. The
	// FIRST state added to a machine also becomes its default (that is
	// Flux_AnimationStateMachineDef::AddState's own rule) and the undo puts the
	// previous default back.
	bool AddState(const std::string& strStateName);

	// Also removes every transition — from any state, and from the any-state
	// list — that TARGETED it. The def's own RemoveState does not, which would
	// leave Flux_AnimationStateMachine::StartTransition resolving a name to
	// nullptr every frame the condition held.
	bool RemoveState(const std::string& strStateName);

	// Retargets every transition that named the old state, so the undo restores
	// them by being the exact inverse rename. Refused for an empty new name, a
	// name already in use, and a CONTAINER state (its sub-machine cannot be
	// moved between two Flux_AnimationState objects — there is no setter — and
	// a rename that silently dropped it would delete a whole nested graph).
	bool RenameState(const std::string& strOldName, const std::string& strNewName);

	// ASSIGNMENT (see the block above): TRUE when strStateName IS the default
	// afterwards, whether or not this call is what made it so. FALSE only for a
	// state the machine does not have.
	bool SetDefaultState(const std::string& strStateName);

	// Give a state's tree a single clip leaf naming strClipName. An EMPTY name
	// removes the tree. Refused for a ZENITH_ANIMCTRL_TREE_COMPLEX state — see
	// the enum. ASSIGNMENT.
	bool SetStateClip(const std::string& strStateName, const std::string& strClipName);

	// UI-only, and persisted: Flux_AnimationState::m_xEditorPosition is already
	// a serialized field of the .zanimctrl, so a laid-out graph survives a round
	// trip with no side-car file and no Zenith_EditorPrefs entry.
	bool SetStateEditorPosition(const std::string& strStateName, const Zenith_Maths::Vector2& xPosition);

	// Appends a transition from -> to with the engine defaults (0.15 s, no exit
	// time, interruptible, priority 0). Refused when either state is missing.
	// A self-transition IS allowed: an any-state graph uses them.
	bool AddTransition(const std::string& strFromState, const std::string& strToState);
	bool RemoveTransition(const std::string& strFromState, u_int uIndex);

	bool SetTransitionDuration(const std::string& strFromState, u_int uIndex, float fSeconds);
	// The two are ONE verb because they are one decision: m_fExitTime is only
	// read when m_bHasExitTime, and a UI that could set the time without the
	// flag would show a value nothing uses.
	bool SetTransitionExitTime(const std::string& strFromState, u_int uIndex, bool bHasExitTime, float fNormalizedExitTime);
	bool SetTransitionInterruptible(const std::string& strFromState, u_int uIndex, bool bInterruptible);

	// The condition's PARAMETER TYPE is taken from the declaration, not from the
	// caller: Flux_TransitionCondition::Evaluate switches on m_eParamType, so a
	// condition whose type disagrees with its parameter reads the wrong union
	// member and compares 1082130432 against 4.25. An UNDECLARED parameter is
	// refused for the same reason.
	bool AddCondition(const std::string& strFromState, u_int uIndex,
		const std::string& strParameterName, Flux_TransitionCondition::CompareOp eCompareOp, float fThreshold);
	bool RemoveCondition(const std::string& strFromState, u_int uIndex, u_int uConditionIndex);

	bool AddParameter(const Zenith_AnimCtrlParameterDecl& xDecl);
	bool RemoveParameter(const std::string& strName);

	// Controller-level. A path already in the list is refused rather than
	// duplicated (the def ignores a duplicate, so pushing an undo entry for it
	// would give Ctrl+Z something that reverses nothing).
	bool AddClipPath(const std::string& strPath);
	bool RemoveClipPath(const std::string& strPath);

	//-------------------------------------------------------------------------
	// PURE helpers, exposed so the panel and the units read one definition.
	//-------------------------------------------------------------------------

	// Which kind a blend-tree root is, without a document. Null -> EMPTY.
	static Zenith_AnimCtrlStateTreeKind ClassifyBlendTree(const Flux_BlendTreeNode* pxRoot);
	static const char* CompareOpName(Flux_TransitionCondition::CompareOp eOp);
	static const char* ParamTypeName(Flux_AnimationParameters::ParamType eType);

private:
	//-------------------------------------------------------------------------
	// The undo commands are the ONLY callers of the primitives below. They must
	// not push commands of their own (a redo that pushed would grow the stack it
	// is being replayed from).
	//-------------------------------------------------------------------------
	friend class Zenith_AnimCtrlCommand_StateAdd;
	friend class Zenith_AnimCtrlCommand_StateRemove;
	friend class Zenith_AnimCtrlCommand_StateRename;
	friend class Zenith_AnimCtrlCommand_DefaultState;
	friend class Zenith_AnimCtrlCommand_StateClip;
	friend class Zenith_AnimCtrlCommand_StatePosition;
	friend class Zenith_AnimCtrlCommand_Transitions;
	friend class Zenith_AnimCtrlCommand_Parameters;
	friend class Zenith_AnimCtrlCommand_ClipPaths;
	// Zenith_AnimCtrlCommand_Compound is deliberately NOT a friend: it performs
	// no edit of its own.

	bool ApplyAddState(u_int uMachineId, const std::string& strName, const Zenith_Vector<char>* pxStateBytes);
	bool ApplyRemoveState(u_int uMachineId, const std::string& strName);
	bool ApplyRenameState(u_int uMachineId, const std::string& strOldName, const std::string& strNewName);
	bool ApplySetDefaultState(u_int uMachineId, const std::string& strName);
	bool ApplySetStateClip(u_int uMachineId, const std::string& strName, const std::string& strClipName);
	bool ApplySetStatePosition(u_int uMachineId, const std::string& strName, const Zenith_Maths::Vector2& xPos);
	bool ApplySetTransitions(u_int uMachineId, const Zenith_AnimCtrlTransitionList& xList);
	bool ApplySetParameters(u_int uMachineId, const Zenith_Vector<Zenith_AnimCtrlParameterDecl>& axDecls);
	bool ApplySetClipPaths(const Zenith_Vector<std::string>& axPaths);
	void MarkDirty() { m_bDirty = true; }

	//-------------------------------------------------------------------------
	// Internals
	//-------------------------------------------------------------------------

	Flux_AnimationStateMachineDef* FindMachine(u_int uMachineId);
	const Flux_AnimationStateMachineDef* FindMachine(u_int uMachineId) const;
	// The selected machine, CREATING the top-level one if that is what is
	// selected and the def has none. Only the mutating verbs call this.
	Flux_AnimationStateMachineDef* EnsureSelectedMachine();

	// Serialize / restore ONE state through Flux_AnimationState's own stream
	// walk — the same argument Flux_AnimationStateMachineDef::CopyFrom makes:
	// a blend tree is a polymorphic hierarchy with no clone verb, and Write/Read
	// is the one faithful walk of it that already exists and is already pinned.
	static void CaptureStateBytes(const Flux_AnimationState& xState, Zenith_Vector<char>& axOut);

	// Every transition list in the machine (each state's, plus the any-state
	// list under an empty owner name).
	void CaptureAllTransitionLists(u_int uMachineId, Zenith_Vector<Zenith_AnimCtrlTransitionList>& axOut) const;
	bool ReadTransitionList(u_int uMachineId, const std::string& strOwner, Zenith_AnimCtrlTransitionList& xOut) const;
	void CaptureParameters(u_int uMachineId, Zenith_Vector<Zenith_AnimCtrlParameterDecl>& axOut) const;
	void CaptureClipPaths(Zenith_Vector<std::string>& axOut) const;

	void PushCommand(Zenith_UndoCommand* pxCommand);
	void ResetToClosed();

	static bool HashFileContents(const std::string& strResolvedPath, u_int64& ulOutHash);
	static u_int64 HashBytes(const void* pData, u_int64 ulSize);
	bool WriteWorkingDefToFile(const std::string& strResolvedPath, u_int64& ulOutHash) const;
	// Copies the working def onto the LIVE asset's def, so a controller built
	// after a save sees what was saved without a ForceUnload (which ignores
	// refcounts) and without a second parse of the file we just wrote.
	void RefreshLiveAssetFromWorkingDef() const;

	Flux_AnimatorControllerDef m_xWorkingDef;
	std::string m_strAssetPath;      // normalized, prefixed where possible
	std::string m_strResolvedPath;   // the filesystem path saves go to

	Zenith_UndoSystem m_xUndoSystem;
	Zenith_AnimCtrlCommand_Compound* m_pxOpenCompound = nullptr;

	u_int m_uSelectedMachineId = uANIMCTRL_TOP_LEVEL_MACHINE;

	u_int64 m_ulRecordedFileHash = 0;
	bool m_bHasRecordedFile = false;
	bool m_bOpen = false;
	bool m_bDirty = false;
};

// Force-link anchor, the Zenith_AnimationDocument_ForceLink idiom: one call
// from Zenith_Editor::Initialise anchors this TU and, through it, the commands
// TU, so their ZENITH_TEST registrars survive /OPT:REF even before the panel
// references them for real.
bool Zenith_AnimControllerDocument_ForceLink();

#endif // ZENITH_TOOLS
