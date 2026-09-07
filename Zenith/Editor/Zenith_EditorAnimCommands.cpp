#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Editor/Zenith_EditorAnimCommands.h"

bool Zenith_EditorAnimCommands_ForceLink()
{
	return true;
}

//------------------------------------------------------------------------------
// ★ EVERY BODY HERE IS TWO CALLS: one non-recording Apply* primitive on the
// document, and MarkDirty(). Nothing in this file touches Flux_AnimationClip.
//
// That is not tidiness — it is the invariant the document header states: the
// document is the ONLY writer of the working clip, because every mutation has
// to re-map the stable ids in the same breath. A command that reached the clip
// directly would leave the id maps describing the track as it was, and the next
// selection lookup would resolve to the wrong key with nothing to observe.
//
// The Apply* primitives are private and this file is friended for them (see the
// friend block in Zenith_AnimationDocument.h). The commands must NOT call the
// public verbs: those push a command of their own, so a redo would grow the
// stack it is being replayed from.
//------------------------------------------------------------------------------

Zenith_AnimCommandBase::Zenith_AnimCommandBase(Zenith_AnimationDocument* pxDocument, const char* szDescription)
	: m_pxDocument(pxDocument)
	, m_strDescription(szDescription != nullptr ? szDescription : "Animation Edit")
{
	Zenith_Assert(m_pxDocument != nullptr,
		"Zenith_AnimCommandBase: an animation command with no document can never resolve its target");
}

//------------------------------------------------------------------------------
// Key insert / remove — mirror images, and both re-insert under the ORIGINAL id.
//------------------------------------------------------------------------------

Zenith_AnimCommand_KeyInsert::Zenith_AnimCommand_KeyInsert(Zenith_AnimationDocument* pxDocument,
	const Zenith_AnimTrackId& xTrack, u_int uKeyId, float fTimeSeconds, const Zenith_AnimKeyValue& xValue)
	: Zenith_AnimCommandBase(pxDocument, "Insert Keyframe")
	, m_xTrack(xTrack)
	, m_uKeyId(uKeyId)
	, m_fTimeSeconds(fTimeSeconds)
	, m_xValue(xValue)
{
}

void Zenith_AnimCommand_KeyInsert::Execute()
{
	if (m_pxDocument == nullptr)
	{
		return;
	}
	m_pxDocument->ApplyInsertKey(m_xTrack, m_fTimeSeconds, m_xValue, m_uKeyId);
	m_pxDocument->MarkDirty();
}

void Zenith_AnimCommand_KeyInsert::Undo()
{
	if (m_pxDocument == nullptr)
	{
		return;
	}
	m_pxDocument->ApplyRemoveKey(m_xTrack, m_uKeyId);
	m_pxDocument->MarkDirty();
}

Zenith_AnimCommand_KeyRemove::Zenith_AnimCommand_KeyRemove(Zenith_AnimationDocument* pxDocument,
	const Zenith_AnimTrackId& xTrack, u_int uKeyId, float fTimeSeconds, const Zenith_AnimKeyValue& xValue)
	: Zenith_AnimCommandBase(pxDocument, "Remove Keyframe")
	, m_xTrack(xTrack)
	, m_uKeyId(uKeyId)
	, m_fTimeSeconds(fTimeSeconds)
	, m_xValue(xValue)
{
}

void Zenith_AnimCommand_KeyRemove::Execute()
{
	if (m_pxDocument == nullptr)
	{
		return;
	}
	m_pxDocument->ApplyRemoveKey(m_xTrack, m_uKeyId);
	m_pxDocument->MarkDirty();
}

void Zenith_AnimCommand_KeyRemove::Undo()
{
	if (m_pxDocument == nullptr)
	{
		return;
	}
	// ★ The original id, not a fresh one. A dope-sheet selection held across the
	// undo has to resolve to the same key it did before the delete.
	m_pxDocument->ApplyInsertKey(m_xTrack, m_fTimeSeconds, m_xValue, m_uKeyId);
	m_pxDocument->MarkDirty();
}

//------------------------------------------------------------------------------
// Retime / revalue.
//------------------------------------------------------------------------------

Zenith_AnimCommand_KeyTime::Zenith_AnimCommand_KeyTime(Zenith_AnimationDocument* pxDocument,
	const Zenith_AnimTrackId& xTrack, u_int uKeyId, float fOldTimeSeconds, float fNewTimeSeconds)
	: Zenith_AnimCommandBase(pxDocument, "Move Keyframe")
	, m_xTrack(xTrack)
	, m_uKeyId(uKeyId)
	, m_fOldTimeSeconds(fOldTimeSeconds)
	, m_fNewTimeSeconds(fNewTimeSeconds)
{
}

void Zenith_AnimCommand_KeyTime::Execute()
{
	if (m_pxDocument == nullptr)
	{
		return;
	}
	m_pxDocument->ApplySetKeyTime(m_xTrack, m_uKeyId, m_fNewTimeSeconds);
	m_pxDocument->MarkDirty();
}

void Zenith_AnimCommand_KeyTime::Undo()
{
	if (m_pxDocument == nullptr)
	{
		return;
	}
	m_pxDocument->ApplySetKeyTime(m_xTrack, m_uKeyId, m_fOldTimeSeconds);
	m_pxDocument->MarkDirty();
}

Zenith_AnimCommand_KeyValue::Zenith_AnimCommand_KeyValue(Zenith_AnimationDocument* pxDocument,
	const Zenith_AnimTrackId& xTrack, u_int uKeyId, const Zenith_AnimKeyValue& xOld,
	const Zenith_AnimKeyValue& xNew, const char* szDescription)
	: Zenith_AnimCommandBase(pxDocument, szDescription != nullptr ? szDescription : "Edit Keyframe Value")
	, m_xTrack(xTrack)
	, m_uKeyId(uKeyId)
	, m_xOld(xOld)
	, m_xNew(xNew)
{
}

void Zenith_AnimCommand_KeyValue::Execute()
{
	if (m_pxDocument == nullptr)
	{
		return;
	}
	m_pxDocument->ApplySetKeyValue(m_xTrack, m_uKeyId, m_xNew);
	m_pxDocument->MarkDirty();
}

void Zenith_AnimCommand_KeyValue::Undo()
{
	if (m_pxDocument == nullptr)
	{
		return;
	}
	m_pxDocument->ApplySetKeyValue(m_xTrack, m_uKeyId, m_xOld);
	m_pxDocument->MarkDirty();
}

//------------------------------------------------------------------------------
// Tangents (WU-8.2). Same two-call body as every other command here, against the
// document's own non-recording primitive — the tangent arrays are parallel to the
// key arrays and only the document knows how to reach the right slot from a
// stable id.
//------------------------------------------------------------------------------

Zenith_AnimCommand_KeyTangents::Zenith_AnimCommand_KeyTangents(Zenith_AnimationDocument* pxDocument,
	const Zenith_AnimTrackId& xTrack, u_int uKeyId, const Flux_KeyTangents& xOld, const Flux_KeyTangents& xNew,
	const char* szDescription)
	: Zenith_AnimCommandBase(pxDocument, szDescription != nullptr ? szDescription : "Edit Key Tangents")
	, m_xTrack(xTrack)
	, m_uKeyId(uKeyId)
	, m_xOld(xOld)
	, m_xNew(xNew)
{
}

void Zenith_AnimCommand_KeyTangents::Execute()
{
	if (m_pxDocument == nullptr)
	{
		return;
	}
	m_pxDocument->ApplySetKeyTangents(m_xTrack, m_uKeyId, m_xNew);
	m_pxDocument->MarkDirty();
}

void Zenith_AnimCommand_KeyTangents::Undo()
{
	if (m_pxDocument == nullptr)
	{
		return;
	}
	// ★ THE EXACT PREVIOUS PAIR, INCLUDING AN UNSET ONE. Restoring "auto" or
	// "linear" as a recomputation would be a second authority on what the track
	// looked like; the numbers that were there are the only faithful inverse, and
	// an all-zero pair restored here is what puts a key back on the sampler's
	// bit-identical linear branch.
	m_pxDocument->ApplySetKeyTangents(m_xTrack, m_uKeyId, m_xOld);
	m_pxDocument->MarkDirty();
}

//------------------------------------------------------------------------------
// Duration.
//------------------------------------------------------------------------------

Zenith_AnimCommand_Duration::Zenith_AnimCommand_Duration(Zenith_AnimationDocument* pxDocument,
	float fOldSeconds, float fNewSeconds)
	: Zenith_AnimCommandBase(pxDocument, "Set Clip Duration")
	, m_fOldSeconds(fOldSeconds)
	, m_fNewSeconds(fNewSeconds)
{
}

void Zenith_AnimCommand_Duration::Execute()
{
	if (m_pxDocument == nullptr)
	{
		return;
	}
	m_pxDocument->ApplySetDuration(m_fNewSeconds);
	m_pxDocument->MarkDirty();
}

void Zenith_AnimCommand_Duration::Undo()
{
	if (m_pxDocument == nullptr)
	{
		return;
	}
	m_pxDocument->ApplySetDuration(m_fOldSeconds);
	m_pxDocument->MarkDirty();
}

//------------------------------------------------------------------------------
// Events.
//------------------------------------------------------------------------------

Zenith_AnimCommand_EventAdd::Zenith_AnimCommand_EventAdd(Zenith_AnimationDocument* pxDocument,
	u_int uEventId, const Flux_AnimationEvent& xEvent)
	: Zenith_AnimCommandBase(pxDocument, "Add Animation Event")
	, m_uEventId(uEventId)
	, m_xEvent(xEvent)
{
}

void Zenith_AnimCommand_EventAdd::Execute()
{
	if (m_pxDocument == nullptr)
	{
		return;
	}
	m_pxDocument->ApplyAddEvent(m_xEvent, m_uEventId);
	m_pxDocument->MarkDirty();
}

void Zenith_AnimCommand_EventAdd::Undo()
{
	if (m_pxDocument == nullptr)
	{
		return;
	}
	m_pxDocument->ApplyRemoveEvent(m_uEventId);
	m_pxDocument->MarkDirty();
}

Zenith_AnimCommand_EventRemove::Zenith_AnimCommand_EventRemove(Zenith_AnimationDocument* pxDocument,
	u_int uEventId, const Flux_AnimationEvent& xEvent)
	: Zenith_AnimCommandBase(pxDocument, "Remove Animation Event")
	, m_uEventId(uEventId)
	, m_xEvent(xEvent)
{
}

void Zenith_AnimCommand_EventRemove::Execute()
{
	if (m_pxDocument == nullptr)
	{
		return;
	}
	m_pxDocument->ApplyRemoveEvent(m_uEventId);
	m_pxDocument->MarkDirty();
}

void Zenith_AnimCommand_EventRemove::Undo()
{
	if (m_pxDocument == nullptr)
	{
		return;
	}
	m_pxDocument->ApplyAddEvent(m_xEvent, m_uEventId);
	m_pxDocument->MarkDirty();
}

Zenith_AnimCommand_EventEdit::Zenith_AnimCommand_EventEdit(Zenith_AnimationDocument* pxDocument,
	u_int uEventId, const Flux_AnimationEvent& xOld, const Flux_AnimationEvent& xNew, const char* szDescription)
	: Zenith_AnimCommandBase(pxDocument, szDescription != nullptr ? szDescription : "Edit Animation Event")
	, m_uEventId(uEventId)
	, m_xOld(xOld)
	, m_xNew(xNew)
{
}

void Zenith_AnimCommand_EventEdit::Execute()
{
	if (m_pxDocument == nullptr)
	{
		return;
	}
	m_pxDocument->ApplySetEvent(m_uEventId, m_xNew);
	m_pxDocument->MarkDirty();
}

void Zenith_AnimCommand_EventEdit::Undo()
{
	if (m_pxDocument == nullptr)
	{
		return;
	}
	m_pxDocument->ApplySetEvent(m_uEventId, m_xOld);
	m_pxDocument->MarkDirty();
}

//------------------------------------------------------------------------------
// Compound — the ONLY command here that does not call an Apply* primitive,
// because it has no edit of its own: it is a bracket around other commands.
//------------------------------------------------------------------------------

Zenith_AnimCommand_Compound::Zenith_AnimCommand_Compound(Zenith_AnimationDocument* pxDocument, const char* szDescription)
	: Zenith_AnimCommandBase(pxDocument, szDescription != nullptr ? szDescription : "Animation Edit")
{
}

Zenith_AnimCommand_Compound::~Zenith_AnimCommand_Compound()
{
	for (u_int u = 0; u < m_apxChildren.GetSize(); ++u)
	{
		delete m_apxChildren.Get(u);
	}
	m_apxChildren.Clear();
}

void Zenith_AnimCommand_Compound::Adopt(Zenith_UndoCommand* pxChild)
{
	if (pxChild == nullptr)
	{
		return;
	}
	m_apxChildren.PushBack(pxChild);
}

void Zenith_AnimCommand_Compound::SetDescription(const char* szDescription)
{
	if (szDescription == nullptr || szDescription[0] == '\0')
	{
		return;
	}
	m_strDescription = szDescription;
}

void Zenith_AnimCommand_Compound::Execute()
{
	// FORWARD. A redo has to reproduce the order the operation originally chose,
	// which is the order that kept every intermediate state collision-free.
	for (u_int u = 0; u < m_apxChildren.GetSize(); ++u)
	{
		m_apxChildren.Get(u)->Execute();
	}
}

void Zenith_AnimCommand_Compound::Undo()
{
	// ★ REVERSE, and this is the load-bearing half. Undoing a multi-key retime in
	// application order puts the first key back on top of the second one; D11
	// refuses that, the mutator returns false, and the undo half-works with
	// nothing raised. The exact inverse order is the only one whose intermediate
	// states are as collision-free as the forward pass's were.
	for (u_int u = m_apxChildren.GetSize(); u-- > 0;)
	{
		m_apxChildren.Get(u)->Undo();
	}
}

#ifdef ZENITH_TESTING
#include "Editor/Zenith_EditorAnimCommands.Tests.inl"
#endif

#endif // ZENITH_TOOLS
