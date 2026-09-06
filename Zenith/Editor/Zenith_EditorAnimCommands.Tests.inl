//------------------------------------------------------------------------------
// Zenith_EditorAnimCommands unit tests (WU-2.2).
// Included at the bottom of Zenith_EditorAnimCommands.cpp.
//
// Zenith_AnimationDocument.Tests.inl pins what each VERB does. These pin the
// COMMAND LAYER underneath them — the three properties that are about the undo
// stack rather than about any one edit:
//
//   • every command kind round-trips: undo the lot, and the document is back at
//     the state it opened in; redo the lot, and it is back at the edited state;
//   • a REDO re-applies rather than re-records (a command that pushed on redo
//     would grow the stack it is being replayed from, and the growth would only
//     show up after a few Ctrl+Y);
//   • closing the document DELETES every command it pushed, which is the thing
//     that makes the commands' raw Zenith_AnimationDocument* safe to hold.
//
// CPU-only and headless under the Null backend; no device, no UI, nothing
// requiresGraphics.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"
#include "UnitTests/Zenith_UnitTests.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "DataStream/Zenith_DataStream.h"

#include <filesystem>

namespace
{
	// Same shape as the document tests' fixture (a private temp directory plus a
	// ForceUnload on the way out). It is duplicated rather than shared because
	// each .inl lands in its own TU and an anonymous namespace does not cross
	// one — the alternative would be a test-support header for eleven lines.
	struct AnimCmdFixture
	{
		std::filesystem::path m_xDirectory;
		std::string m_strPath;

		explicit AnimCmdFixture(const char* szLeafDirectory)
		{
			std::error_code xError;
			std::filesystem::path xRoot = std::filesystem::temp_directory_path(xError);
			if (xError)
			{
				xRoot = ".";
			}
			m_xDirectory = xRoot / szLeafDirectory;
			std::filesystem::remove_all(m_xDirectory, xError);
			std::filesystem::create_directories(m_xDirectory, xError);
			m_strPath = (m_xDirectory / "cmd.zanim").generic_string();

			Flux_AnimationClip xClip;
			xClip.SetName("CmdProbe");
			xClip.SetDuration(2.0f);
			Flux_BoneChannel xHip;
			xHip.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
			xHip.AddPositionKeyframe(1.0f, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
			xHip.AddPositionKeyframe(2.0f, Zenith_Maths::Vector3(0.0f, 2.0f, 0.0f));
			xHip.SortKeyframes();
			xClip.AddBoneChannel("Hip", std::move(xHip));
			xClip.Export(m_strPath);
		}

		~AnimCmdFixture()
		{
			Zenith_AssetRegistry::ForceUnload(m_strPath);
			std::error_code xError;
			std::filesystem::remove_all(m_xDirectory, xError);
		}

		AnimCmdFixture(const AnimCmdFixture&) = delete;
		AnimCmdFixture& operator=(const AnimCmdFixture&) = delete;
	};

	Zenith_AnimTrackId AnimCmdHipTrack()
	{
		return Zenith_AnimTrackId::Bone("Hip", FLUX_ANIM_TRACK_POSITION);
	}
}

//==============================================================================
// (1) Every command kind undoes and redoes through the document.
//==============================================================================
ZENITH_TEST(AnimCommands, EveryCommandKindUndoesAndRedoesThroughTheDocument)
{
	AnimCmdFixture xFixture("zenith_animcmd_roundtrip");

	Zenith_AnimationDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.Open(xFixture.m_strPath) == ZENITH_ANIMDOC_OPEN_OK, "the probe opens");

	const Zenith_AnimTrackId xTrack = AnimCmdHipTrack();
	const u_int uIdA = xDoc.GetKeyIdAtIndex(xTrack, 0);   // t = 0.0, y = 0
	const u_int uIdB = xDoc.GetKeyIdAtIndex(xTrack, 1);   // t = 1.0, y = 1
	const u_int uIdC = xDoc.GetKeyIdAtIndex(xTrack, 2);   // t = 2.0, y = 2

	// One of each command shape, in an order that makes each one's inverse
	// observable: an insert, a remove, a retime that REORDERS, a value edit, a
	// duration change, and the three event commands.
	const u_int uIdNew = xDoc.InsertKey(xTrack, 0.5f, Zenith_Maths::Vector3(0.0f, 10.0f, 0.0f));
	ZENITH_ASSERT_NE(uIdNew, uINVALID_ANIM_KEY_ID, "the insert produced a key");
	ZENITH_ASSERT_TRUE(xDoc.RemoveKey(xTrack, uIdC), "the remove lands");
	ZENITH_ASSERT_TRUE(xDoc.SetKeyTime(xTrack, uIdA, 1.5f), "the retime lands");
	ZENITH_ASSERT_TRUE(xDoc.SetKeyValue(xTrack, uIdB, Zenith_Maths::Vector3(0.0f, 20.0f, 0.0f)), "the value edit lands");
	ZENITH_ASSERT_TRUE(xDoc.SetDuration(7.0f), "the duration edit lands");
	const u_int uEventId = xDoc.AddEvent("Beat", 0.5f, Zenith_Maths::Vector4(0.0f, 0.0f, 0.0f, 0.0f));
	ZENITH_ASSERT_NE(uEventId, uINVALID_ANIM_KEY_ID, "the event was added");
	ZENITH_ASSERT_TRUE(xDoc.SetEventName(uEventId, "BeatRenamed"), "the event edit lands");
	ZENITH_ASSERT_TRUE(xDoc.RemoveEvent(uEventId), "the event remove lands");

	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 8u, "eight edits pushed exactly eight commands");

	// ---- undo everything ----
	for (u_int u = 0; u < 8u; ++u)
	{
		xDoc.Undo();
	}
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 0u, "the undo stack drained");
	ZENITH_ASSERT_EQ(xDoc.GetRedoStackSize(), 8u, "into the redo stack");

	ZENITH_ASSERT_EQ(xDoc.GetKeyCount(xTrack), 3u, "back to the three keys the file has");
	ZENITH_ASSERT_EQ(xDoc.GetKeyIndexForId(xTrack, uIdNew), uINVALID_ANIM_KEY_INDEX, "the inserted key is gone again");
	ZENITH_ASSERT_EQ(xDoc.GetKeyIndexForId(xTrack, uIdC), 2u, "and the removed one is back, under its original id");
	ZENITH_ASSERT_EQ_FLOAT(xDoc.GetDuration(), 2.0f, 1e-6f, "the duration is back");
	ZENITH_ASSERT_EQ(xDoc.GetEventCount(), 0u, "and the event is gone");

	float fTime = -1.0f;
	Zenith_AnimKeyValue xValue;
	ZENITH_ASSERT_TRUE(xDoc.GetKeyTime(xTrack, uIdA, fTime), "A resolves");
	ZENITH_ASSERT_EQ_FLOAT(fTime, 0.0f, 1e-6f, "at its original time");
	ZENITH_ASSERT_TRUE(xDoc.GetKeyValue(xTrack, uIdB, xValue), "B resolves");
	ZENITH_ASSERT_EQ_FLOAT(xValue.m_xVector.y, 1.0f, 1e-6f, "with its original value");

	// ---- redo everything ----
	for (u_int u = 0; u < 8u; ++u)
	{
		xDoc.Redo();
	}
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 8u, "the redo stack drained back");
	ZENITH_ASSERT_EQ(xDoc.GetRedoStackSize(), 0u, "leaving nothing to redo");

	ZENITH_ASSERT_EQ(xDoc.GetKeyCount(xTrack), 3u, "the edited key count is back");
	ZENITH_ASSERT_NE(xDoc.GetKeyIndexForId(xTrack, uIdNew), uINVALID_ANIM_KEY_INDEX, "the inserted key is back under its original id");
	ZENITH_ASSERT_EQ(xDoc.GetKeyIndexForId(xTrack, uIdC), uINVALID_ANIM_KEY_INDEX, "and the removed one is gone again");
	ZENITH_ASSERT_EQ_FLOAT(xDoc.GetDuration(), 7.0f, 1e-6f, "the duration edit is back");
	ZENITH_ASSERT_EQ(xDoc.GetEventCount(), 0u, "and so is the event removal");

	ZENITH_ASSERT_TRUE(xDoc.GetKeyTime(xTrack, uIdA, fTime), "A still resolves");
	ZENITH_ASSERT_EQ_FLOAT(fTime, 1.5f, 1e-6f, "at the retimed time");
	ZENITH_ASSERT_TRUE(xDoc.GetKeyValue(xTrack, uIdB, xValue), "B still resolves");
	ZENITH_ASSERT_EQ_FLOAT(xValue.m_xVector.y, 20.0f, 1e-6f, "with the edited value");
	ZENITH_ASSERT_EQ(xDoc.GetKeyIndexForId(xTrack, uIdA), 2u, "and the reorder is back too");
}

//==============================================================================
// (2) A redo RE-APPLIES; it does not re-record.
//==============================================================================
ZENITH_TEST(AnimCommands, ARedoDoesNotGrowTheStackItIsReplayedFrom)
{
	AnimCmdFixture xFixture("zenith_animcmd_redo");

	Zenith_AnimationDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.Open(xFixture.m_strPath) == ZENITH_ANIMDOC_OPEN_OK, "the probe opens");

	ZENITH_ASSERT_TRUE(xDoc.SetDuration(3.0f), "one edit");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 1u, "one command");

	// ★ The document RECORDS an already-applied command rather than Executing it,
	// so Execute() only ever runs on a redo. If a command's Execute pushed a
	// command of its own the stack would grow by one on every Ctrl+Y — which
	// looks like nothing at all until the fourth or fifth press.
	for (u_int u = 0; u < 3u; ++u)
	{
		xDoc.Undo();
		ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 0u, "the undo moves the one command across");
		ZENITH_ASSERT_EQ(xDoc.GetRedoStackSize(), 1u, "onto the redo stack");
		ZENITH_ASSERT_EQ_FLOAT(xDoc.GetDuration(), 2.0f, 1e-6f, "and reverses the edit");

		xDoc.Redo();
		ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 1u, "the redo moves it back — it does NOT add a second");
		ZENITH_ASSERT_EQ(xDoc.GetRedoStackSize(), 0u, "with nothing left to redo");
		ZENITH_ASSERT_EQ_FLOAT(xDoc.GetDuration(), 3.0f, 1e-6f, "and re-applies the edit");
	}
}

//==============================================================================
// (3) Closing the document deletes every command it pushed.
//==============================================================================
ZENITH_TEST(AnimCommands, ClosingTheDocumentDeletesEveryCommandItPushed)
{
	AnimCmdFixture xFixture("zenith_animcmd_close");

	Zenith_AnimationDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.Open(xFixture.m_strPath) == ZENITH_ANIMDOC_OPEN_OK, "the probe opens");

	const Zenith_AnimTrackId xTrack = AnimCmdHipTrack();
	ZENITH_ASSERT_TRUE(xDoc.SetDuration(3.0f), "an edit");
	ZENITH_ASSERT_TRUE(xDoc.RemoveKey(xTrack, xDoc.GetKeyIdAtIndex(xTrack, 0)), "another");
	xDoc.Undo();
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 1u, "one on the undo stack");
	ZENITH_ASSERT_EQ(xDoc.GetRedoStackSize(), 1u, "one on the redo stack");

	// ★ THIS is what makes the commands' raw Zenith_AnimationDocument* safe: the
	// document's own stack is the only place they live, and closing it deletes
	// them. No command can outlive the document it addresses.
	xDoc.CloseDiscardingChanges();
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 0u, "the close emptied the undo stack");
	ZENITH_ASSERT_EQ(xDoc.GetRedoStackSize(), 0u, "and the redo stack");

	// The same clearing happens in the destructor, for a document that is never
	// closed. Nothing survives this scope to be observed afterwards — that IS the
	// property — so what this pins is that the path runs at all.
	{
		Zenith_AnimationDocument xScoped;
		ZENITH_ASSERT_TRUE(xScoped.Open(xFixture.m_strPath) == ZENITH_ANIMDOC_OPEN_OK, "a second document opens the same file");
		ZENITH_ASSERT_TRUE(xScoped.SetDuration(9.0f), "with an edit outstanding");
		ZENITH_ASSERT_TRUE(xScoped.IsDirty(), "and left dirty and unclosed");
	}

	// And the first document reopens cleanly afterwards.
	ZENITH_ASSERT_TRUE(xDoc.Open(xFixture.m_strPath) == ZENITH_ANIMDOC_OPEN_OK, "the file reopens");
	ZENITH_ASSERT_FALSE(xDoc.CanUndo(), "with a fresh, empty history");
}

//==============================================================================
// (4) Every pushed command names itself, which is what the Edit menu reads.
//==============================================================================
ZENITH_TEST(AnimCommands, EachPushedCommandCarriesADescription)
{
	AnimCmdFixture xFixture("zenith_animcmd_description");

	Zenith_AnimationDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.Open(xFixture.m_strPath) == ZENITH_ANIMDOC_OPEN_OK, "the probe opens");

	const Zenith_AnimTrackId xTrack = AnimCmdHipTrack();
	const u_int uIdB = xDoc.GetKeyIdAtIndex(xTrack, 1);

	ZENITH_ASSERT_TRUE(xDoc.SetKeyTime(xTrack, uIdB, 1.25f), "a retime");
	ZENITH_ASSERT_STREQ(xDoc.UndoSystem().GetUndoDescription(), "Move Keyframe", "the retime names itself");

	ZENITH_ASSERT_TRUE(xDoc.SetKeyValue(xTrack, uIdB, Zenith_Maths::Vector3(0.0f, 4.0f, 0.0f)), "a value edit");
	ZENITH_ASSERT_STREQ(xDoc.UndoSystem().GetUndoDescription(), "Edit Keyframe Value", "the value edit names itself");

	ZENITH_ASSERT_NE(xDoc.InsertKey(xTrack, 1.75f, Zenith_Maths::Vector3(0.0f, 6.0f, 0.0f)), uINVALID_ANIM_KEY_ID, "an insert");
	ZENITH_ASSERT_STREQ(xDoc.UndoSystem().GetUndoDescription(), "Insert Keyframe", "the insert names itself");

	// ★ An insert onto an OCCUPIED time is a VALUE edit (D25), and it says so —
	// which is the user-visible half of the same decision the id preservation is
	// the mechanical half of.
	ZENITH_ASSERT_NE(xDoc.InsertKey(xTrack, 1.75f, Zenith_Maths::Vector3(0.0f, 8.0f, 0.0f)), uINVALID_ANIM_KEY_ID, "an insert onto that slot");
	ZENITH_ASSERT_STREQ(xDoc.UndoSystem().GetUndoDescription(), "Replace Keyframe", "reads as a replace, not an insert");

	ZENITH_ASSERT_TRUE(xDoc.RemoveKey(xTrack, uIdB), "a remove");
	ZENITH_ASSERT_STREQ(xDoc.UndoSystem().GetUndoDescription(), "Remove Keyframe", "the remove names itself");

	ZENITH_ASSERT_TRUE(xDoc.SetDuration(5.0f), "a duration change");
	ZENITH_ASSERT_STREQ(xDoc.UndoSystem().GetUndoDescription(), "Set Clip Duration", "the duration change names itself");

	ZENITH_ASSERT_NE(xDoc.AddEvent("Beat", 0.25f, Zenith_Maths::Vector4(0.0f, 0.0f, 0.0f, 0.0f)), uINVALID_ANIM_KEY_ID, "an event add");
	ZENITH_ASSERT_STREQ(xDoc.UndoSystem().GetUndoDescription(), "Add Animation Event", "the event add names itself");
}
