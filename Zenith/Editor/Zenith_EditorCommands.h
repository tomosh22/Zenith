#pragma once

#ifdef ZENITH_TOOLS

#include "Editor/Zenith_UndoSystem.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_Entity.h"
#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"
#include <string>

//=============================================================================
// Zenith_EditorCommands — everything the editor can undo beyond a gizmo drag.
//
// Three command shapes cover every editor mutation, each with ONE apply routine
// so Execute and Undo are the same code run with different arguments:
//
//   EntityLifetime  delete / create / duplicate — a serialised subtree snapshot
//                   that can be destroyed and rebuilt (with fresh EntityIDs)
//   EntityState     rename / enable / reparent — the three slot-level fields
//   ComponentBytes  add / remove / edit ONE component — its serialised payload
//                   before and after (an empty payload means "absent")
//
// Plus Zenith_EditorInspectorUndoTracker, which turns any inspector edit into a
// ComponentBytes (or TransformEdit) command without every property widget
// having to know about undo.
//=============================================================================

//-----------------------------------------------------------------------------
// Zenith_EditorEntitySnapshot — a serialised copy of one entity subtree.
//
// Records the root and every descendant (pre-order) as name / enabled /
// transient / parent-record-index / component bytes. Restore rebuilds them in
// the captured scene with NEW EntityIDs (slots are generation-counted, so the
// old IDs can never come back) and re-links the hierarchy from the record
// indices, not the dead IDs.
//-----------------------------------------------------------------------------
class Zenith_EditorEntitySnapshot
{
public:
	// Captures xRoot and its subtree. Returns false for an invalid entity.
	bool Capture(Zenith_Entity xRoot);

	// Rebuilds the subtree. The root is parented to xParentOverride when
	// bUseCapturedParent is false, otherwise to the parent it had at capture
	// (if that entity still exists; a root otherwise). Returns the new root ID.
	Zenith_EntityID Restore(Zenith_EntityID xParentOverride, bool bUseCapturedParent) const;

	bool IsValid() const { return m_axRecords.GetSize() > 0; }
	u_int GetEntityCount() const { return m_axRecords.GetSize(); }
	const std::string& GetRootName() const;
	void SetRootName(const std::string& strName);
	Zenith_EntityID GetCapturedRootParent() const { return m_xRootParent; }
	Zenith_Scene GetScene() const { return m_xScene; }
	bool RootWasMainCamera() const { return m_bRootWasMainCamera; }

	// Serialises every component of xEntity into axOut (the entity-record payload
	// the meta registry reads back with DeserializeEntityComponents).
	static void SerializeAllComponents(Zenith_Entity& xEntity, Zenith_Vector<uint8_t>& axOut);

private:
	struct Record
	{
		std::string m_strName;
		bool m_bEnabled = true;
		bool m_bTransient = false;
		// Index into m_axRecords of the parent, or uROOT for the root.
		u_int m_uParentRecord = uROOT;
		Zenith_Vector<uint8_t> m_axComponentBytes;
	};
	static constexpr u_int uROOT = 0xFFFFFFFFu;

	void CaptureRecursive(Zenith_Entity xEntity, u_int uParentRecord);

	Zenith_Vector<Record> m_axRecords;
	Zenith_Scene m_xScene;
	Zenith_EntityID m_xRootParent = INVALID_ENTITY_ID;
	bool m_bRootWasMainCamera = false;
};

//-----------------------------------------------------------------------------
// Delete / create / duplicate.
//-----------------------------------------------------------------------------
class Zenith_UndoCommand_EntityLifetime : public Zenith_UndoCommand
{
public:
	enum class Kind
	{
		Delete,   // Execute destroys the captured entity; Undo rebuilds it
		Create    // Execute rebuilds the snapshot; Undo destroys the result
	};

	// Delete: the snapshot is captured from xEntity NOW (before Execute runs).
	// Returns nullptr if the entity is invalid.
	static Zenith_UndoCommand_EntityLifetime* MakeDelete(Zenith_Entity xEntity);

	// Create: Execute restores xSnapshot under xParent (INVALID = scene root).
	static Zenith_UndoCommand_EntityLifetime* MakeCreate(const Zenith_EditorEntitySnapshot& xSnapshot,
		Zenith_EntityID xParent, const char* szDescription);

	void Execute() override;
	void Undo() override;
	const char* GetDescription() const override { return m_strDescription.c_str(); }

	// The live entity this command currently refers to (changes on every
	// restore, because restore allocates fresh IDs). INVALID while deleted.
	Zenith_EntityID GetCurrentEntityID() const { return m_xCurrentID; }
	// For a Create command recorded AFTER the entity was made live: tells the
	// command which entity its Undo should destroy.
	void SetCurrentEntityID(Zenith_EntityID xID) { m_xCurrentID = xID; }

private:
	Zenith_UndoCommand_EntityLifetime() = default;
	void Destroy();
	void Rebuild();

	Kind m_eKind = Kind::Delete;
	Zenith_EditorEntitySnapshot m_xSnapshot;
	Zenith_EntityID m_xCurrentID = INVALID_ENTITY_ID;
	Zenith_EntityID m_xParentOverride = INVALID_ENTITY_ID;
	bool m_bUseCapturedParent = true;
	std::string m_strDescription;
};

//-----------------------------------------------------------------------------
// Several commands as ONE undo step (multi-selection delete / duplicate).
// Execute runs the children in order; Undo runs them in reverse. Owns them.
//-----------------------------------------------------------------------------
class Zenith_UndoCommand_Composite : public Zenith_UndoCommand
{
public:
	explicit Zenith_UndoCommand_Composite(const char* szDescription) : m_strDescription(szDescription) {}
	~Zenith_UndoCommand_Composite() override;

	void Add(Zenith_UndoCommand* pxCommand) { if (pxCommand != nullptr) m_axCommands.PushBack(pxCommand); }
	u_int GetCount() const { return m_axCommands.GetSize(); }
	Zenith_UndoCommand* GetCommand(u_int u) const { return m_axCommands.Get(u); }

	void Execute() override;
	void Undo() override;
	const char* GetDescription() const override { return m_strDescription.c_str(); }

private:
	Zenith_Vector<Zenith_UndoCommand*> m_axCommands;
	std::string m_strDescription;
};

//-----------------------------------------------------------------------------
// Rename / enable / reparent.
//-----------------------------------------------------------------------------
class Zenith_UndoCommand_EntityState : public Zenith_UndoCommand
{
public:
	struct State
	{
		std::string m_strName;
		bool m_bEnabled = true;
		Zenith_EntityID m_xParent = INVALID_ENTITY_ID;
	};

	static State CaptureState(Zenith_Entity xEntity);
	// Writes every field of xState onto the entity (a no-op field is harmless).
	static void ApplyState(Zenith_EntityID xEntityID, const State& xState);

	Zenith_UndoCommand_EntityState(Zenith_EntityID xEntityID, const State& xOld, const State& xNew, const char* szDescription);

	void Execute() override { ApplyState(m_xEntityID, m_xNew); }
	void Undo() override { ApplyState(m_xEntityID, m_xOld); }
	const char* GetDescription() const override { return m_strDescription.c_str(); }

private:
	Zenith_EntityID m_xEntityID;
	State m_xOld;
	State m_xNew;
	std::string m_strDescription;
};

//-----------------------------------------------------------------------------
// Add / remove / edit ONE component, addressed by its meta-registry type name.
//-----------------------------------------------------------------------------
class Zenith_UndoCommand_ComponentBytes : public Zenith_UndoCommand
{
public:
	// Serialises the named component into axOut. Returns false (and leaves axOut
	// empty) when the entity has no such component — "empty" IS the absent state.
	static bool CaptureComponent(Zenith_Entity xEntity, const char* szTypeName, Zenith_Vector<uint8_t>& axOut);

	// Puts the component into the state axBytes describes: removes it when the
	// payload is empty, otherwise (re)creates it from the payload. Everything but
	// the Transform is rebuilt from scratch (remove + add + read) so a component
	// whose reader assumes a fresh instance never sees a populated one; the
	// Transform, which every entity must keep, is read in place.
	static void ApplyBytes(Zenith_EntityID xEntityID, const char* szTypeName, const Zenith_Vector<uint8_t>& axBytes);

	Zenith_UndoCommand_ComponentBytes(Zenith_EntityID xEntityID, const char* szTypeName,
		const Zenith_Vector<uint8_t>& axBefore, const Zenith_Vector<uint8_t>& axAfter, const char* szDescription);

	void Execute() override { ApplyBytes(m_xEntityID, m_strTypeName.c_str(), m_axAfter); }
	void Undo() override { ApplyBytes(m_xEntityID, m_strTypeName.c_str(), m_axBefore); }
	const char* GetDescription() const override { return m_strDescription.c_str(); }

private:
	Zenith_EntityID m_xEntityID;
	std::string m_strTypeName;
	Zenith_Vector<uint8_t> m_axBefore;
	Zenith_Vector<uint8_t> m_axAfter;
	std::string m_strDescription;
};

//-----------------------------------------------------------------------------
// Zenith_EditorInspectorUndoTracker
//
// Makes every edit made through the Properties panel undoable without the
// component inspectors knowing about it. The panel opens a "session" on any
// input that could start an edit (a click inside the panel, a Tab keypress),
// which snapshots every component of the entity; when the UI goes idle again
// (nothing active, no popup) the tracker diffs the live components against the
// snapshot and records one command per changed component. The Transform is
// diffed as position/rotation/scale and recorded through the setter-based
// TransformEdit command, so physics bodies follow an undo the same way they
// follow a gizmo drag.
//-----------------------------------------------------------------------------
class Zenith_EditorInspectorUndoTracker
{
public:
	// Call before the panel draws the entity. bTrigger: an input that may begin
	// an edit this frame. Switching entity discards any open session.
	void BeginFrame(Zenith_Entity xEntity, bool bTrigger);

	// Call after the panel drew the entity. bUIIdle: no item active, no popup open.
	void EndFrame(Zenith_Entity xEntity, bool bUIIdle);

	// Drop the open session without recording anything (the panel calls this
	// after pushing its own add/remove component command).
	void Cancel();

	bool HasOpenSession() const { return m_bOpen; }

	// PURE: the diff step, exposed for tests. Compares the snapshot taken at
	// BeginFrame against the entity's live components and records the changed
	// ones on the undo system. Returns the number of commands recorded.
	u_int CommitDiff(Zenith_Entity xEntity);

private:
	struct ComponentSnapshot
	{
		std::string m_strTypeName;
		Zenith_Vector<uint8_t> m_axBytes;
	};

	void Capture(Zenith_Entity xEntity);

	Zenith_EntityID m_xEntityID = INVALID_ENTITY_ID;
	Zenith_Vector<ComponentSnapshot> m_axBefore;
	Zenith_Maths::Vector3 m_xBeforePosition = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Quat m_xBeforeRotation = Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);
	Zenith_Maths::Vector3 m_xBeforeScale = Zenith_Maths::Vector3(1.0f);
	bool m_bHasTransform = false;
	bool m_bOpen = false;
};

#endif // ZENITH_TOOLS
