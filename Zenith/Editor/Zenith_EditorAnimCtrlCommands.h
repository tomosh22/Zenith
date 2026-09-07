#pragma once

#ifdef ZENITH_TOOLS

#include "Editor/Zenith_UndoSystem.h"
#include "Editor/Zenith_AnimControllerDocument.h"
#include "Collections/Zenith_Vector.h"
#include <string>

//=============================================================================
// Zenith_EditorAnimCtrlCommands — the undo layer for
// Zenith_AnimControllerDocument (WU-6.5).
//
// ★ THESE DO NOT LOOK LIKE Zenith_EditorCommands.h, FOR THE SAME REASON THE
// CLIP COMMANDS DO NOT. Every command there resolves its target by EntityID
// through g_xEngine.Scenes().GetSceneDataForEntity(...), because an entity can
// move between scenes underneath a queued command. A state-machine edit has no
// entity, no scene and no EntityID; copying that pattern would mean inventing
// one.
//
// The address here is (document, machine, name) — D29's shape:
//
//   • DOCUMENT: a raw Zenith_AnimControllerDocument*. It cannot dangle: the
//     ONLY stack these are pushed to is that document's own m_xUndoSystem, and
//     both Close() and the destructor Clear() it, which deletes every command
//     in it.
//   • MACHINE: a LAYER ID, or uANIMCTRL_TOP_LEVEL_MACHINE. Never a layer
//     INDEX — inserting or removing a layer renumbers every index above it
//     (WU-6.3), so an index-addressed command would replay onto a different
//     layer with nothing to observe.
//   • STATE: a NAME. It is the machine's own key, it is what a transition
//     targets, and it is what survives a state being deleted and rebuilt by an
//     undo. A rename is therefore its own command rather than a field edit.
//
// Every command is RECORDED, never Executed, on the way in: the document has
// already performed the edit (it is the only writer, and it needs the def's
// own return values to validate). Execute() therefore only runs on a REDO.
//
// ★ A TRANSITION EDIT IS A WHOLE-LIST SNAPSHOT, and that is the one shape
// choice worth defending. A transition has NO identity — it is a struct in a
// Zenith_Vector addressed by index, and Flux_AnimationState::AddTransition
// inserts by PRIORITY, so an add or a remove renumbers the indices a
// finer-grained command would be holding. The clip document answers the same
// problem with a stable per-key id, which it can only do because it owns the
// keys' storage; here the storage is the engine's and the list is a handful of
// entries, so the snapshot is both cheaper and exact.
//=============================================================================

//-----------------------------------------------------------------------------
// Shared base: the document pointer, the machine id and the description.
//-----------------------------------------------------------------------------
class Zenith_AnimCtrlCommandBase : public Zenith_UndoCommand
{
public:
	Zenith_AnimCtrlCommandBase(Zenith_AnimControllerDocument* pxDocument, u_int uMachineId, const char* szDescription);

	const char* GetDescription() const override { return m_strDescription.c_str(); }
	u_int GetMachineId() const { return m_uMachineId; }

protected:
	Zenith_AnimControllerDocument* m_pxDocument = nullptr;
	u_int m_uMachineId = uANIMCTRL_TOP_LEVEL_MACHINE;
	std::string m_strDescription;
};

//-----------------------------------------------------------------------------
// A state was added.
//
// m_axStateBytes is the state's full Flux_AnimationState payload, captured
// AFTER the add — so a REDO restores whatever the state had grown by the time
// it was first undone rather than an empty shell. m_strPrevDefault is carried
// because AddState makes the FIRST state of a machine its default, and an undo
// that left that behind would point the machine at a state it had just deleted.
//-----------------------------------------------------------------------------
class Zenith_AnimCtrlCommand_StateAdd : public Zenith_AnimCtrlCommandBase
{
public:
	Zenith_AnimCtrlCommand_StateAdd(Zenith_AnimControllerDocument* pxDocument, u_int uMachineId,
		const std::string& strStateName, const Zenith_Vector<char>& axStateBytes,
		const std::string& strPrevDefault);

	void Execute() override;
	void Undo() override;

private:
	std::string m_strStateName;
	Zenith_Vector<char> m_axStateBytes;
	std::string m_strPrevDefault;
};

//-----------------------------------------------------------------------------
// A state was removed, together with every transition that TARGETED it.
//
// ★ THE INBOUND TRANSITIONS ARE PART OF THE COMMAND, not a side effect the undo
// re-derives. They are gone from lists this command does not otherwise touch,
// and "which transitions used to point here" is not recoverable from the state
// itself — so the removal captures every affected list whole and the undo puts
// each one back.
//-----------------------------------------------------------------------------
class Zenith_AnimCtrlCommand_StateRemove : public Zenith_AnimCtrlCommandBase
{
public:
	Zenith_AnimCtrlCommand_StateRemove(Zenith_AnimControllerDocument* pxDocument, u_int uMachineId,
		const std::string& strStateName, const Zenith_Vector<char>& axStateBytes,
		const std::string& strPrevDefault,
		const Zenith_Vector<Zenith_AnimCtrlTransitionList>& axPrevLists);

	void Execute() override;
	void Undo() override;

private:
	std::string m_strStateName;
	Zenith_Vector<char> m_axStateBytes;
	std::string m_strPrevDefault;
	// Every list as it stood BEFORE the removal — the removed state's own
	// included, because restoring it from bytes restores its outgoing list too
	// and these are what the OTHER states lost.
	Zenith_Vector<Zenith_AnimCtrlTransitionList> m_axPrevLists;
};

//-----------------------------------------------------------------------------
// A state was renamed. ★ SYMMETRIC BY CONSTRUCTION: the undo is the rename the
// other way round, and the document's rename retargets every transition that
// named the state — so the transitions come back for free rather than being a
// second thing to remember. That is the whole reason rename is not implemented
// as a remove plus an add.
//-----------------------------------------------------------------------------
class Zenith_AnimCtrlCommand_StateRename : public Zenith_AnimCtrlCommandBase
{
public:
	Zenith_AnimCtrlCommand_StateRename(Zenith_AnimControllerDocument* pxDocument, u_int uMachineId,
		const std::string& strOldName, const std::string& strNewName);

	void Execute() override;
	void Undo() override;

private:
	std::string m_strOldName;
	std::string m_strNewName;
};

class Zenith_AnimCtrlCommand_DefaultState : public Zenith_AnimCtrlCommandBase
{
public:
	Zenith_AnimCtrlCommand_DefaultState(Zenith_AnimControllerDocument* pxDocument, u_int uMachineId,
		const std::string& strOldName, const std::string& strNewName);

	void Execute() override;
	void Undo() override;

private:
	std::string m_strOldName;
	std::string m_strNewName;
};

//-----------------------------------------------------------------------------
// A state's clip leaf. An EMPTY clip name means "no blend tree at all", which
// is a distinct, legal state (it poses the bind pose) — so both fields are
// plain strings and neither needs a "has value" flag beside it.
//-----------------------------------------------------------------------------
class Zenith_AnimCtrlCommand_StateClip : public Zenith_AnimCtrlCommandBase
{
public:
	Zenith_AnimCtrlCommand_StateClip(Zenith_AnimControllerDocument* pxDocument, u_int uMachineId,
		const std::string& strStateName, const std::string& strOldClip, const std::string& strNewClip);

	void Execute() override;
	void Undo() override;

private:
	std::string m_strStateName;
	std::string m_strOldClip;
	std::string m_strNewClip;
};

//-----------------------------------------------------------------------------
// A node was dragged. Undoable like everything else — a layout is authored data
// here (Flux_AnimationState::m_xEditorPosition is serialized into the
// .zanimctrl), so a drag DIRTIES the document and belongs on the stack.
//-----------------------------------------------------------------------------
class Zenith_AnimCtrlCommand_StatePosition : public Zenith_AnimCtrlCommandBase
{
public:
	Zenith_AnimCtrlCommand_StatePosition(Zenith_AnimControllerDocument* pxDocument, u_int uMachineId,
		const std::string& strStateName,
		const Zenith_Maths::Vector2& xOld, const Zenith_Maths::Vector2& xNew);

	void Execute() override;
	void Undo() override;

private:
	std::string m_strStateName;
	Zenith_Maths::Vector2 m_xOld;
	Zenith_Maths::Vector2 m_xNew;
};

//-----------------------------------------------------------------------------
// ONE state's outgoing transition list, before and after. Every transition verb
// — add, remove, duration, exit time, interruptible, add/remove condition —
// pushes exactly this. See the header note for why an index-addressed command
// would be wrong.
//-----------------------------------------------------------------------------
class Zenith_AnimCtrlCommand_Transitions : public Zenith_AnimCtrlCommandBase
{
public:
	Zenith_AnimCtrlCommand_Transitions(Zenith_AnimControllerDocument* pxDocument, u_int uMachineId,
		const Zenith_AnimCtrlTransitionList& xOld, const Zenith_AnimCtrlTransitionList& xNew,
		const char* szDescription);

	void Execute() override;
	void Undo() override;

private:
	Zenith_AnimCtrlTransitionList m_xOld;
	Zenith_AnimCtrlTransitionList m_xNew;
};

//-----------------------------------------------------------------------------
// The machine's parameter DECLARATION table, before and after.
//
// ★ THE WHOLE TABLE, for a reason the transition list does not share:
// Flux_AnimationParameters is a NAME-KEYED HASH MAP with no ordering and no
// clear verb, so "put this one declaration back where it was" is not an
// operation it offers. Replacing the table is the only faithful inverse, and
// the table is a handful of entries.
//-----------------------------------------------------------------------------
class Zenith_AnimCtrlCommand_Parameters : public Zenith_AnimCtrlCommandBase
{
public:
	Zenith_AnimCtrlCommand_Parameters(Zenith_AnimControllerDocument* pxDocument, u_int uMachineId,
		const Zenith_Vector<Zenith_AnimCtrlParameterDecl>& axOld,
		const Zenith_Vector<Zenith_AnimCtrlParameterDecl>& axNew,
		const char* szDescription);

	void Execute() override;
	void Undo() override;

private:
	Zenith_Vector<Zenith_AnimCtrlParameterDecl> m_axOld;
	Zenith_Vector<Zenith_AnimCtrlParameterDecl> m_axNew;
};

//-----------------------------------------------------------------------------
// The controller's clip-path list. CONTROLLER level, so it carries
// uANIMCTRL_TOP_LEVEL_MACHINE as its machine id and ignores it.
//-----------------------------------------------------------------------------
class Zenith_AnimCtrlCommand_ClipPaths : public Zenith_AnimCtrlCommandBase
{
public:
	Zenith_AnimCtrlCommand_ClipPaths(Zenith_AnimControllerDocument* pxDocument,
		const Zenith_Vector<std::string>& axOld, const Zenith_Vector<std::string>& axNew,
		const char* szDescription);

	void Execute() override;
	void Undo() override;

private:
	Zenith_Vector<std::string> m_axOld;
	Zenith_Vector<std::string> m_axNew;
};

//-----------------------------------------------------------------------------
// MANY EDITS, ONE UNDO STEP. Identical in shape and rationale to
// Zenith_AnimCommand_Compound: Zenith_UndoSystem is a flat LIFO with no
// transaction of any kind, so the bracket has to live where the commands are
// pushed. Forward on Execute, REVERSE on Undo.
//-----------------------------------------------------------------------------
class Zenith_AnimCtrlCommand_Compound : public Zenith_AnimCtrlCommandBase
{
public:
	Zenith_AnimCtrlCommand_Compound(Zenith_AnimControllerDocument* pxDocument, const char* szDescription);
	~Zenith_AnimCtrlCommand_Compound() override;

	void Adopt(Zenith_UndoCommand* pxChild);
	u_int GetChildCount() const { return m_apxChildren.GetSize(); }
	void SetDescription(const char* szDescription);

	void Execute() override;
	void Undo() override;

private:
	Zenith_Vector<Zenith_UndoCommand*> m_apxChildren;
};

// Force-link anchor — see Zenith_AnimControllerDocument_ForceLink.
bool Zenith_EditorAnimCtrlCommands_ForceLink();

#endif // ZENITH_TOOLS
