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
// What is inside a state's blend tree.
//
// WU-6.5 authored a state whose tree is a single clip leaf and reported
// EVERYTHING else as COMPLEX. WU-7.3 splits the two BLEND SPACES out of that
// bucket, because they are now edited here: a state holding one is a sub-graph
// with points, an axis binding and an editor, not a refusal. What is left in
// COMPLEX is the set that genuinely has no editor — the composites
// (Blend / Additive / Masked / Select) and a container state's sub-machine —
// and it is still REPORTED rather than silently flattened, because an editor
// that "assigned a clip" to a nest would delete the whole thing and report
// success.
//
// ★ THE VALUES ARE NOT SERIALIZED ANYWHERE. This is a classification the
// document computes from the live tree on demand (ClassifyBlendTree), so
// inserting the two new kinds in the middle moves nothing on disk.
//-----------------------------------------------------------------------------
enum Zenith_AnimCtrlStateTreeKind : u_int
{
	// No blend tree at all — a state that has never been given a clip. Legal:
	// it poses the bind pose.
	ZENITH_ANIMCTRL_TREE_EMPTY,
	// Exactly one Flux_BlendTreeNode_Clip at the root.
	ZENITH_ANIMCTRL_TREE_SINGLE_CLIP,
	// A Flux_BlendTreeNode_BlendSpace1D at the root (WU-7.3).
	ZENITH_ANIMCTRL_TREE_BLENDSPACE_1D,
	// A Flux_BlendTreeNode_BlendSpace2D at the root (WU-7.3).
	ZENITH_ANIMCTRL_TREE_BLENDSPACE_2D,
	// A composite, or a container state's sub-machine. Refused here, with the
	// reason named.
	ZENITH_ANIMCTRL_TREE_COMPLEX,
};

//-----------------------------------------------------------------------------
// Which axis of a blend space a binding or a position addresses.
//
// ★ A 1D SPACE HAS ONLY AN X, and asking it for a Y is a caller ERROR rather
// than a value: it is refused, not answered with zero. The POSITIONS, by
// contrast, travel as a Vector2 on both — see AddBlendPoint.
//-----------------------------------------------------------------------------
enum Zenith_AnimCtrlBlendAxis : u_int
{
	ZENITH_ANIMCTRL_BLEND_AXIS_X,
	ZENITH_ANIMCTRL_BLEND_AXIS_Y,
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
// One layer's SCALAR fields — everything WU-7.2's list edits that is not the
// layer's state machine.
//
// ★ THEY TRAVEL AS A SET RATHER THAN ONE COMMAND PER FIELD, and the set
// deliberately STOPS SHORT of the state machine. Five small values with an exact
// inverse need no serialization at all, so a weight slider costs a struct rather
// than a walk of every blend tree in the layer; and because the layer is
// addressed by ID, no field command can go stale when the list is reordered
// underneath it. The heavyweight snapshot below is only for the three edits that
// genuinely change the LIST.
//-----------------------------------------------------------------------------
struct Zenith_AnimCtrlLayerFields
{
	std::string m_strName;
	float m_fWeight = 1.0f;
	Flux_LayerBlendMode m_eBlendMode = LAYER_BLEND_OVERRIDE;
	bool m_bEmitEvents = true;
	std::string m_strBoneMaskAssetPath;
};

//-----------------------------------------------------------------------------
// One layer captured WHOLE — its stable id, plus its entire
// Flux_AnimatorControllerLayerDef payload as bytes.
//
// ★ THE UNDO UNIT FOR add / remove / MOVE IS THE WHOLE LIST, for the reason the
// transition list is: a layer's POSITION is what those three edits change, and a
// position is not an identity. It is also the only inverse available —
// Flux_AnimatorControllerDef exposes AddLayer / RemoveLayer(index) and nothing
// that reorders, so "put the list back the way it was" IS rebuild-in-order.
//
// ★ THE PAYLOAD IS BYTES, not a copy, because Flux_AnimatorControllerLayerDef
// owns a Flux_AnimationStateMachineDef by value and that type is neither
// copyable nor movable. Write/ReadFromDataStream is the one faithful walk of a
// polymorphic blend tree that already exists and is already pinned by a test —
// the same argument Flux_AnimationStateMachineDef::CopyFrom makes.
//-----------------------------------------------------------------------------
struct Zenith_AnimCtrlLayerSnapshot
{
	u_int m_uLayerId = uFLUX_INVALID_LAYER_ID;
	Zenith_Vector<char> m_axBytes;
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

	// Layer ids in BLEND ORDER (index order), which is the order the layer list
	// draws them in and the order the runtime composes them in.
	void GetLayerIds(Zenith_Vector<u_int>& auOut) const;
	bool GetLayerName(u_int uLayerId, std::string& strOut) const;

	//-------------------------------------------------------------------------
	// The LAYER LIST (WU-7.2).
	//
	// ★ EVERY VERB ADDRESSES A LAYER BY ITS STABLE ID, NEVER BY ITS INDEX
	// (D43). An index is a POSITION IN THE BLEND ORDER, which is precisely the
	// thing MoveLayer changes and the thing an insert or a remove renumbers —
	// so an index-addressed edit would land on a different layer with nothing to
	// observe. The one place an index legitimately appears is MoveLayer's
	// DESTINATION, which is a position by definition.
	//-------------------------------------------------------------------------

	u_int GetLayerCount() const;
	// Blend order -> id, and back. Both false for something out of range, so a
	// caller cannot mistake "layer 0" for "no layer".
	bool GetLayerIdAt(u_int uIndex, u_int& uOut) const;
	bool GetLayerIndex(u_int uLayerId, u_int& uOut) const;

	bool GetLayerWeight(u_int uLayerId, float& fOut) const;
	bool GetLayerBlendMode(u_int uLayerId, Flux_LayerBlendMode& eOut) const;
	bool GetLayerEmitEvents(u_int uLayerId, bool& bOut) const;
	bool GetLayerMaskAssetPath(u_int uLayerId, std::string& strOut) const;

	// Why the LAST layer verb refused, or empty. Set by the one refusal that is
	// not a caller error — a mask path on an additive layer — and cleared by
	// every layer verb on the way in, so it always describes the most recent
	// call rather than accumulating.
	const std::string& GetLastLayerDiagnostic() const { return m_strLastLayerDiagnostic; }

	// CREATION. Appends a layer at the END of the blend order carrying a FRESHLY
	// MINTED id, and returns it — uFLUX_INVALID_LAYER_ID on refusal (a closed
	// document, or an empty name). Layer NAMES are deliberately not required to
	// be unique: Flux_AnimationController::GetLayerByName documents itself as
	// "the FIRST layer with that name", and two "Overlay" layers are ordinary.
	u_int AddLayer(const std::string& strName);

	// REMOVAL, so a miss is a genuine refusal. Also drops the machine SELECTION
	// when it pointed at this layer — leaving it would make every subsequent
	// verb address a machine that no longer exists.
	bool RemoveLayer(u_int uLayerId);

	// ASSIGNMENTS, all of them (see the mutation block below): re-stating a
	// value a layer already holds is the caller's intent SATISFIED — true, no
	// mutation, NO undo entry.
	bool RenameLayer(u_int uLayerId, const std::string& strName);
	// CLAMPED to [0,1] before the comparison, exactly as
	// Flux_AnimationLayer::SetWeight clamps at runtime; a non-finite weight is
	// refused rather than clamped.
	bool SetLayerWeight(u_int uLayerId, float fWeight);
	// ★ CHANGING A MASKED LAYER TO ADDITIVE IS ALLOWED AND DOES NOT CLEAR THE
	// MASK PATH. The runtime simply ignores it (LayerAcceptsMask), the UI says
	// so, and switching back to OVERRIDE has to bring the assignment back —
	// silently deleting an authored path on a mode toggle would be an
	// unrecoverable edit disguised as a combo box.
	bool SetLayerBlendMode(u_int uLayerId, Flux_LayerBlendMode eMode);
	bool SetLayerEmitEvents(u_int uLayerId, bool bEmitEvents);
	// ★ REFUSED FOR A NON-EMPTY PATH ON AN ADDITIVE LAYER, with
	// Zenith_BoneMaskDocument::AdditiveLayerMaskNotice() left in
	// GetLastLayerDiagnostic(). The rule itself is stated ONCE, in
	// Zenith_BoneMaskDocument::LayerAcceptsMask, and this asks that function —
	// an additive layer goes straight to Flux_SkeletonPose::AdditiveBlend, whose
	// signature has no mask in it, so an accepted assignment here would let
	// somebody author a whole mask, save it, assign it and observe nothing.
	// CLEARING the path (an empty string) is always allowed.
	bool SetLayerMaskAssetPath(u_int uLayerId, const std::string& strPath);
	// Move the layer to uNewIndex in the BLEND ORDER, shifting the rest along.
	// An index past the end is refused (it is a caller error, not a value);
	// asking for the index the layer already occupies is satisfied.
	bool MoveLayer(u_int uLayerId, u_int uNewIndex);

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

	//-------------------------------------------------------------------------
	// BLEND-SPACE inspection (WU-7.3). All false for a state whose tree is not a
	// blend space, so a caller cannot mistake "not a blend space" for "a blend
	// space with no points".
	//-------------------------------------------------------------------------

	u_int GetBlendPointCount(const std::string& strStateName) const;
	// ★ THE POSITION IS A Vector2 ON BOTH SPACES, AND A 1D SPACE IGNORES ITS Y.
	// One shape rather than an overload pair: the panel, the undo command and the
	// AddStep_AnimBlend* payload each carry ONE position, and a 1D/2D split would
	// duplicate all three to save a float. A 1D read always answers y = 0.
	bool GetBlendPoint(const std::string& strStateName, u_int uIndex,
		std::string& strOutClipName, Zenith_Maths::Vector2& xOutPosition) const;
	// The controller parameter this axis tracks, or empty for an unbound one.
	// False for a 1D space asked for its Y — it has no second axis.
	bool GetBlendSpaceParameterName(const std::string& strStateName, Zenith_AnimCtrlBlendAxis eAxis,
		std::string& strOut) const;

	// Why the LAST blend-tree verb refused, or empty. Set by the refusals that
	// are RULES rather than caller errors — a COMPLEX tree, and a binding to a
	// parameter that is not a declared Float — and cleared by every blend verb on
	// the way in, so it always describes the most recent call.
	const std::string& GetLastBlendTreeDiagnostic() const { return m_strLastBlendTreeDiagnostic; }
	// The ONE wording of "this tree has no editor here", so the panel's node
	// badge, its inspector and the units cannot disagree about what a refusal
	// says. Names the shapes rather than a work-unit number: a Blend/Additive/
	// Masked/Select nest is what is actually being refused.
	static const char* BlendTreeRefusalText();
	// The ONE wording of "that parameter cannot drive a blend axis".
	static const char* BlendParameterRefusalText();

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
	// removes the tree. Refused for a ZENITH_ANIMCTRL_TREE_COMPLEX state and for
	// a BLEND SPACE — a clip assignment onto a space would delete the space and
	// every point in it; SetStateTreeKind is the verb that converts. ASSIGNMENT.
	bool SetStateClip(const std::string& strStateName, const std::string& strClipName);

	//-------------------------------------------------------------------------
	// BLEND-SPACE editing (WU-7.3).
	//
	// ★ EVERY ONE OF THESE IS ONE UNDO STEP, AND THE STEP IS A WHOLE-STATE
	// SNAPSHOT. A blend tree has NO IDENTITY BELOW THE STATE — a point is a
	// struct in a Zenith_Vector addressed by index, a node is an owned raw
	// pointer with no id, and a 1D position edit RE-SORTS the list — so there is
	// nothing finer than the state for a command to address. The state's own
	// serializer is the one faithful walk of a polymorphic tree that already
	// exists and is already pinned, which is the same argument
	// Zenith_AnimCtrlCommand_StateAdd/Remove make.
	//
	// ★ AND THE SNAPSHOT IS WHAT MAKES "UNDO RESTORES THE SINGLE CLIP" EXACT:
	// converting a clip leaf to a blend space throws away the leaf's playback
	// rate and its playhead, and only bytes taken before the conversion can put
	// them back.
	//-------------------------------------------------------------------------

	// CONVERT a state's tree. eKind must be SINGLE_CLIP, BLENDSPACE_1D or
	// BLENDSPACE_2D; EMPTY (use SetStateClip("")) and COMPLEX (nothing can
	// synthesise a nest) are refused.
	//
	// ★ THE CONVERSION CARRIES THE CLIPS ACROSS, which is the difference between
	// a conversion and a delete-and-start-again. A single clip leaf seeds the new
	// space's FIRST POINT at the origin; a space converted to a clip leaf keeps
	// its FIRST point's clip; one space converted to the other carries every
	// point (2D -> 1D drops the y, 1D -> 2D lands them on y = 0).
	//
	// ASSIGNMENT: asking for the kind the state already holds is satisfied —
	// true, no mutation, no undo entry. Refused (false, with a diagnostic) for a
	// COMPLEX state and for a state the machine does not have.
	bool SetStateTreeKind(const std::string& strStateName, Zenith_AnimCtrlStateTreeKind eKind);

	// Bind one axis of a blend space to a controller parameter. An EMPTY name
	// UNBINDS, which is always allowed and leaves the space on its literal.
	//
	// ★ THE NAME MUST BE A DECLARED **Float**, and that is a rule rather than a
	// convenience. Flux_BlendTreeNode_BlendSpace1D::ResolveParameters reads the
	// binding through Flux_AnimationParameters::GetFloat, so an Int or a Bool
	// would be read through the wrong union member, and an UNDECLARED name is
	// left at its literal by the runtime — a binding that silently does nothing.
	// Refused with BlendParameterRefusalText() in the diagnostic.
	//
	// ASSIGNMENT.
	bool SetBlendSpaceParameter(const std::string& strStateName, Zenith_AnimCtrlBlendAxis eAxis,
		const std::string& strParameterName);

	// CREATION: appends a point playing strClipName at xPosition (y ignored on a
	// 1D space). Refused for a state that is not a blend space and for an empty
	// clip name — a nameless leaf resolves to no clip and poses the bind pose.
	// puOutIndex receives the point's index AFTER the 1D sort.
	bool AddBlendPoint(const std::string& strStateName, const std::string& strClipName,
		const Zenith_Maths::Vector2& xPosition, u_int* puOutIndex = nullptr);
	// REMOVAL: a miss is a genuine refusal.
	bool RemoveBlendPoint(const std::string& strStateName, u_int uIndex);

	// ASSIGNMENTS.
	bool SetBlendPointClip(const std::string& strStateName, u_int uIndex, const std::string& strClipName);
	// ★ A 1D EDIT MAY RENUMBER, which is why this reports where the point went:
	// the runtime blends between ADJACENT points, so the list is kept sorted and
	// dragging one past another swaps their indices. A 2D edit never renumbers.
	bool SetBlendPointPosition(const std::string& strStateName, u_int uIndex,
		const Zenith_Maths::Vector2& xPosition, u_int* puOutIndex = nullptr);

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
	friend class Zenith_AnimCtrlCommand_StateTree;
	friend class Zenith_AnimCtrlCommand_StatePosition;
	friend class Zenith_AnimCtrlCommand_Transitions;
	friend class Zenith_AnimCtrlCommand_Parameters;
	friend class Zenith_AnimCtrlCommand_ClipPaths;
	friend class Zenith_AnimCtrlCommand_Layers;
	friend class Zenith_AnimCtrlCommand_LayerFields;
	// Zenith_AnimCtrlCommand_Compound is deliberately NOT a friend: it performs
	// no edit of its own.

	bool ApplyAddState(u_int uMachineId, const std::string& strName, const Zenith_Vector<char>* pxStateBytes);
	bool ApplyRemoveState(u_int uMachineId, const std::string& strName);
	bool ApplyRenameState(u_int uMachineId, const std::string& strOldName, const std::string& strNewName);
	bool ApplySetDefaultState(u_int uMachineId, const std::string& strName);
	bool ApplySetStateClip(u_int uMachineId, const std::string& strName, const std::string& strClipName);
	// Restore ONE state's whole payload IN PLACE from bytes CaptureStateBytes
	// produced (WU-7.3's blend-tree undo unit).
	//
	// ★ IN PLACE, NOT REMOVE-AND-ADD, because the state has to keep its position
	// in every transition that names it and its entry in the machine's map. That
	// is safe precisely because Flux_AnimationState::ReadFromDataStream is a FULL
	// restore: it deletes the old blend tree, clears the transition list and
	// rewrites the name, so nothing of the previous contents survives to be
	// mixed with the restored ones.
	bool ApplyRestoreState(u_int uMachineId, const std::string& strName, const Zenith_Vector<char>& axBytes);
	bool ApplySetStatePosition(u_int uMachineId, const std::string& strName, const Zenith_Maths::Vector2& xPos);
	bool ApplySetTransitions(u_int uMachineId, const Zenith_AnimCtrlTransitionList& xList);
	bool ApplySetParameters(u_int uMachineId, const Zenith_Vector<Zenith_AnimCtrlParameterDecl>& axDecls);
	bool ApplySetClipPaths(const Zenith_Vector<std::string>& axPaths);
	// Rebuild the WHOLE layer list from snapshots, in the order given. Every
	// layer is destroyed and re-read from its bytes, so a caller holding a
	// Flux_AnimatorControllerLayerDef* across this is reading freed memory —
	// nothing in this document does, and the panel addresses layers by id.
	bool ApplySetLayers(const Zenith_Vector<Zenith_AnimCtrlLayerSnapshot>& axLayers);
	bool ApplySetLayerFields(u_int uLayerId, const Zenith_AnimCtrlLayerFields& xFields);
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

	// Every layer, in blend order, each as its own serialized payload.
	void CaptureLayers(Zenith_Vector<Zenith_AnimCtrlLayerSnapshot>& axOut) const;
	bool CaptureLayerFields(u_int uLayerId, Zenith_AnimCtrlLayerFields& xOut) const;
	// The one body every scalar layer setter shares: validate, compare against
	// what is there (an ASSIGNMENT no-op pushes nothing), apply, push ONE
	// command. szDescription is what Ctrl+Z's tooltip says.
	bool SetLayerFields(u_int uLayerId, const Zenith_AnimCtrlLayerFields& xNew, const char* szDescription);
	// The one body AddLayer / RemoveLayer / MoveLayer share.
	bool ApplyLayerListEdit(const Zenith_Vector<Zenith_AnimCtrlLayerSnapshot>& axOld,
		const Zenith_Vector<Zenith_AnimCtrlLayerSnapshot>& axNew, const char* szDescription);

	// The one body every WU-7.3 blend verb shares. The caller has ALREADY
	// mutated the state; this captures the new bytes, compares them with the
	// ones taken before, and pushes ONE Zenith_AnimCtrlCommand_StateTree if they
	// differ. Byte equality IS the assignment no-op test — a verb that re-stated
	// a value the tree already carried leaves the payload identical, so it marks
	// nothing dirty and contributes zero undo steps.
	bool CommitStateTreeEdit(const std::string& strStateName, const Zenith_Vector<char>& axOldBytes,
		const char* szDescription);
	// The selected machine's state, or null — plus the blend-space root when the
	// caller needs one. Both clear m_strLastBlendTreeDiagnostic's caller-error
	// cases by simply refusing.
	Flux_AnimationState* FindStateForBlendEdit(const std::string& strStateName);

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

	// WU-7.2. Why the last layer verb refused; see GetLastLayerDiagnostic.
	std::string m_strLastLayerDiagnostic;
	// WU-7.3. Why the last blend-tree verb refused; see GetLastBlendTreeDiagnostic.
	std::string m_strLastBlendTreeDiagnostic;

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
