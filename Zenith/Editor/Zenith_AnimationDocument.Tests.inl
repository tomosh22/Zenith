//------------------------------------------------------------------------------
// Zenith_AnimationDocument unit tests (WU-2.2).
// Included at the bottom of Zenith_AnimationDocument.cpp.
//
// ★ THE HEADLINE PROPERTY: a selection held as KEY IDS survives an edit that
// REORDERS the track, and survives the undo and the redo of that edit. Every
// other test here pins an edge of the same machinery — the id an insert-on-
// occupied must NOT allocate, the id an undone remove must restore, and the two
// places (Save As, Discard) where every id is legitimately retired.
//
// All of it is CPU-only and runs headless under the Null backend: the clips are
// plain data, the files live in a private directory under the OS temp dir and
// are removed on the way out, and nothing here needs a device or a UI. None of
// these is requiresGraphics.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"
#include "UnitTests/Zenith_UnitTests.h"
#include "AssetHandling/Zenith_AssetTypeIds.h"

#include <filesystem>

namespace
{
	//--------------------------------------------------------------------------
	// Fixture — the shape Zenith_AnimationAsset.Tests.inl established: a private
	// temp directory removed on the way out, plus a ForceUnload of every registry
	// path the test caused to be loaded, so a throwaway asset never lingers in
	// the live registry the suite runs inside.
	//
	// ★ DECLARE THE FIXTURE BEFORE THE DOCUMENT IN EVERY TEST. The document holds
	// an OWNING asset handle; ForceUnload deletes the asset regardless of
	// refcount, so the document has to be destroyed first. Declaration order in
	// the test body is what guarantees that.
	//--------------------------------------------------------------------------
	struct AnimDocFixture
	{
		std::filesystem::path m_xDirectory;
		std::string m_strPath;
		Zenith_Vector<std::string> m_axTrackedPaths;

		explicit AnimDocFixture(const char* szLeafDirectory)
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
			m_strPath = (m_xDirectory / "doc.zanim").generic_string();
		}

		// A second path inside the sandbox, remembered so it is unloaded too.
		std::string PathFor(const char* szLeafName)
		{
			std::string strPath = (m_xDirectory / szLeafName).generic_string();
			m_axTrackedPaths.PushBack(strPath);
			return strPath;
		}

		void Track(const std::string& strPath)
		{
			m_axTrackedPaths.PushBack(strPath);
		}

		~AnimDocFixture()
		{
			Zenith_AssetRegistry::ForceUnload(m_strPath);
			for (u_int u = 0; u < m_axTrackedPaths.GetSize(); ++u)
			{
				Zenith_AssetRegistry::ForceUnload(m_axTrackedPaths.Get(u));
			}
			std::error_code xError;
			std::filesystem::remove_all(m_xDirectory, xError);
		}

		AnimDocFixture(const AnimDocFixture&) = delete;
		AnimDocFixture& operator=(const AnimDocFixture&) = delete;
	};

	//--------------------------------------------------------------------------
	// Probe clip: one bone, THREE position keys at 0 / 1 / 2 seconds with
	// DISTINCT values (y = 0, fScale, 2*fScale).
	//
	// Three keys with different values is the minimum that can catch a reorder:
	// with two, a swapped pair of ids and a correct one are indistinguishable,
	// and with equal values a wrong id resolves to the right number.
	//--------------------------------------------------------------------------
	void AnimDocBuildProbeClip(Flux_AnimationClip& xClip, const char* szName, float fScale, bool bGenerated)
	{
		xClip.SetName(szName);
		xClip.SetDuration(2.0f);
		xClip.GetMetadata().m_bGenerated = bGenerated;

		Flux_BoneChannel xHip;
		xHip.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
		xHip.AddPositionKeyframe(1.0f, Zenith_Maths::Vector3(0.0f, 1.0f * fScale, 0.0f));
		xHip.AddPositionKeyframe(2.0f, Zenith_Maths::Vector3(0.0f, 2.0f * fScale, 0.0f));
		xHip.SortKeyframes();
		xClip.AddBoneChannel("Hip", std::move(xHip));
	}

	void AnimDocWriteProbeFile(const std::string& strPath, const char* szName, float fScale, bool bGenerated)
	{
		Flux_AnimationClip xClip;
		AnimDocBuildProbeClip(xClip, szName, fScale, bGenerated);
		xClip.Export(strPath);
	}

	Zenith_AnimTrackId AnimDocHipPositionTrack()
	{
		return Zenith_AnimTrackId::Bone("Hip", FLUX_ANIM_TRACK_POSITION);
	}

	// The y of the Hip position key at fTimeSeconds, straight out of a clip. Used
	// to check what actually reached a FILE, which a key count cannot.
	float AnimDocSampleHipY(const Flux_AnimationClip* pxClip, float fTimeSeconds)
	{
		if (pxClip == nullptr)
		{
			return -1.0f;
		}
		const Flux_BoneChannel* pxHip = pxClip->GetBoneChannel("Hip");
		return pxHip != nullptr ? pxHip->SamplePosition(fTimeSeconds).y : -1.0f;
	}

	bool AnimDocReadFileBytes(const std::string& strPath, Zenith_Vector<uint8_t>& axOut)
	{
		axOut.Clear();
		uint64_t ulSize = 0;
		char* pData = Zenith_FileAccess::ReadFile(strPath.c_str(), ulSize);
		if (pData == nullptr)
		{
			return false;
		}
		axOut.Resize(static_cast<u_int>(ulSize));
		if (ulSize > 0)
		{
			memcpy(axOut.GetDataPointer(), pData, static_cast<size_t>(ulSize));
		}
		Zenith_FileAccess::FreeFileData(pData);
		return true;
	}

	bool AnimDocBytesEqual(const Zenith_Vector<uint8_t>& axA, const Zenith_Vector<uint8_t>& axB)
	{
		if (axA.GetSize() != axB.GetSize())
		{
			return false;
		}
		return axA.GetSize() == 0 || memcmp(axA.GetDataPointer(), axB.GetDataPointer(), axA.GetSize()) == 0;
	}

	// Parse a .zanim off disk with the real reader, so "the save wrote the
	// envelope" is checked by the thing that demands one rather than asserted
	// about the bytes.
	bool AnimDocParseFile(const std::string& strPath, Flux_AnimationClip& xOut)
	{
		Zenith_DataStream xStream;
		xStream.ReadFromFile(strPath.c_str());
		if (!xStream.IsValid())
		{
			return false;
		}
		xStream.SetCursor(0);
		return xOut.ParseStream(xStream).IsOk();
	}
}

//==============================================================================
// (1) Open deep-copies the clip and hands every key a distinct, non-zero id.
//==============================================================================
ZENITH_TEST(AnimDocument, OpenCopiesTheClipAndAllocatesStableKeyIds)
{
	AnimDocFixture xFixture("zenith_animdoc_open");
	AnimDocWriteProbeFile(xFixture.m_strPath, "DocProbe", 1.0f, false);

	Zenith_AnimationDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.Open(xFixture.m_strPath) == ZENITH_ANIMDOC_OPEN_OK, "a well-formed authored .zanim opens");
	ZENITH_ASSERT_TRUE(xDoc.IsOpen(), "and the document reports itself open");
	ZENITH_ASSERT_FALSE(xDoc.IsDirty(), "a freshly opened document is clean");
	ZENITH_ASSERT_FALSE(xDoc.CanUndo(), "and carries no history");

	const Zenith_AnimTrackId xTrack = AnimDocHipPositionTrack();
	ZENITH_ASSERT_EQ(xDoc.GetKeyCount(xTrack), 3u, "the three probe keys came across");

	const u_int uIdA = xDoc.GetKeyIdAtIndex(xTrack, 0);
	const u_int uIdB = xDoc.GetKeyIdAtIndex(xTrack, 1);
	const u_int uIdC = xDoc.GetKeyIdAtIndex(xTrack, 2);
	ZENITH_ASSERT_NE(uIdA, uINVALID_ANIM_KEY_ID, "every key has an id");
	ZENITH_ASSERT_NE(uIdB, uINVALID_ANIM_KEY_ID, "every key has an id");
	ZENITH_ASSERT_NE(uIdC, uINVALID_ANIM_KEY_ID, "every key has an id");
	ZENITH_ASSERT_NE(uIdA, uIdB, "ids are distinct within a track");
	ZENITH_ASSERT_NE(uIdB, uIdC, "ids are distinct within a track");

	ZENITH_ASSERT_EQ(xDoc.GetKeyIndexForId(xTrack, uIdB), 1u, "and resolve back to their index");

	float fTime = -1.0f;
	ZENITH_ASSERT_TRUE(xDoc.GetKeyTime(xTrack, uIdB, fTime), "an id resolves to a time");
	ZENITH_ASSERT_EQ_FLOAT(fTime, 1.0f, 1e-6f, "the middle key sits at 1 s");

	// ★ D20: the working copy is a DEEP copy. Editing it must not touch the live
	// clip that a controller could be sampling.
	Zenith_AnimationAsset* pxAsset = xDoc.GetAsset();
	ZENITH_ASSERT_NOT_NULL(pxAsset, "the document holds the live asset");
	if (pxAsset != nullptr)
	{
		ZENITH_ASSERT_TRUE(&xDoc.GetClip() != pxAsset->GetClip(), "the working copy is NOT the live clip");
		ZENITH_ASSERT_TRUE(xDoc.SetKeyValue(xTrack, uIdB, Zenith_Maths::Vector3(0.0f, 42.0f, 0.0f)), "an edit lands");
		ZENITH_ASSERT_EQ_FLOAT(AnimDocSampleHipY(pxAsset->GetClip(), 1.0f), 1.0f, 1e-5f,
			"and the LIVE clip is untouched until a save");
	}
}

//==============================================================================
// (2) ★ THE HEADLINE. A retime REORDERS the track; a selection held as ids
// still names the same keys, before, after, through an undo and through a redo.
//==============================================================================
ZENITH_TEST(AnimDocument, ARetimeReordersTheTrackAndIdsStillResolveAcrossUndoAndRedo)
{
	AnimDocFixture xFixture("zenith_animdoc_retime");
	AnimDocWriteProbeFile(xFixture.m_strPath, "DocProbe", 1.0f, false);

	Zenith_AnimationDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.Open(xFixture.m_strPath) == ZENITH_ANIMDOC_OPEN_OK, "the probe opens");

	const Zenith_AnimTrackId xTrack = AnimDocHipPositionTrack();
	const u_int uIdA = xDoc.GetKeyIdAtIndex(xTrack, 0);   // t = 0, y = 0
	const u_int uIdB = xDoc.GetKeyIdAtIndex(xTrack, 1);   // t = 1, y = 1
	const u_int uIdC = xDoc.GetKeyIdAtIndex(xTrack, 2);   // t = 2, y = 2

	// Move the FIRST key past both of the others: the one shape an index-based
	// selection cannot survive.
	ZENITH_ASSERT_TRUE(xDoc.SetKeyTime(xTrack, uIdA, 2.5f), "the retime is accepted (the slot is free)");
	ZENITH_ASSERT_TRUE(xDoc.IsDirty(), "an edit dirties the document");

	ZENITH_ASSERT_EQ(xDoc.GetKeyIndexForId(xTrack, uIdB), 0u, "B is now first");
	ZENITH_ASSERT_EQ(xDoc.GetKeyIndexForId(xTrack, uIdC), 1u, "C is now second");
	ZENITH_ASSERT_EQ(xDoc.GetKeyIndexForId(xTrack, uIdA), 2u, "A moved to the end");

	float fTime = 0.0f;
	Zenith_AnimKeyValue xValue;
	ZENITH_ASSERT_TRUE(xDoc.GetKeyTime(xTrack, uIdA, fTime) && xDoc.GetKeyValue(xTrack, uIdA, xValue), "A still resolves");
	ZENITH_ASSERT_EQ_FLOAT(fTime, 2.5f, 1e-6f, "at its new time");
	ZENITH_ASSERT_EQ_FLOAT(xValue.m_xVector.y, 0.0f, 1e-6f, "carrying its OWN value, not the value of whoever now sits at index 0");
	ZENITH_ASSERT_TRUE(xDoc.GetKeyValue(xTrack, uIdB, xValue), "B still resolves");
	ZENITH_ASSERT_EQ_FLOAT(xValue.m_xVector.y, 1.0f, 1e-6f, "with its own value");

	// ---- undo ----
	xDoc.Undo();
	ZENITH_ASSERT_EQ(xDoc.GetKeyIndexForId(xTrack, uIdA), 0u, "the undo puts A back at index 0");
	ZENITH_ASSERT_EQ(xDoc.GetKeyIndexForId(xTrack, uIdB), 1u, "B back at 1");
	ZENITH_ASSERT_EQ(xDoc.GetKeyIndexForId(xTrack, uIdC), 2u, "C back at 2");
	ZENITH_ASSERT_TRUE(xDoc.GetKeyTime(xTrack, uIdA, fTime), "A resolves after the undo");
	ZENITH_ASSERT_EQ_FLOAT(fTime, 0.0f, 1e-6f, "at its original time");
	ZENITH_ASSERT_TRUE(xDoc.GetKeyTime(xTrack, uIdC, fTime), "C resolves after the undo");
	ZENITH_ASSERT_EQ_FLOAT(fTime, 2.0f, 1e-6f, "at its original time");

	// ---- redo ----
	xDoc.Redo();
	ZENITH_ASSERT_EQ(xDoc.GetKeyIndexForId(xTrack, uIdA), 2u, "the redo reorders it again");
	ZENITH_ASSERT_TRUE(xDoc.GetKeyTime(xTrack, uIdA, fTime) && xDoc.GetKeyValue(xTrack, uIdA, xValue), "A still resolves");
	ZENITH_ASSERT_EQ_FLOAT(fTime, 2.5f, 1e-6f, "back at the retimed time");
	ZENITH_ASSERT_EQ_FLOAT(xValue.m_xVector.y, 0.0f, 1e-6f, "with its own value intact");
}

//==============================================================================
// (3) A removed key comes back under its ORIGINAL id.
//==============================================================================
ZENITH_TEST(AnimDocument, RemoveThenUndoRestoresTheKeyUnderItsOriginalId)
{
	AnimDocFixture xFixture("zenith_animdoc_remove");
	AnimDocWriteProbeFile(xFixture.m_strPath, "DocProbe", 1.0f, false);

	Zenith_AnimationDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.Open(xFixture.m_strPath) == ZENITH_ANIMDOC_OPEN_OK, "the probe opens");

	const Zenith_AnimTrackId xTrack = AnimDocHipPositionTrack();
	const u_int uIdB = xDoc.GetKeyIdAtIndex(xTrack, 1);

	ZENITH_ASSERT_TRUE(xDoc.RemoveKey(xTrack, uIdB), "the middle key is removed");
	ZENITH_ASSERT_EQ(xDoc.GetKeyCount(xTrack), 2u, "leaving two");
	ZENITH_ASSERT_EQ(xDoc.GetKeyIndexForId(xTrack, uIdB), uINVALID_ANIM_KEY_INDEX, "and its id no longer resolves");

	xDoc.Undo();
	ZENITH_ASSERT_EQ(xDoc.GetKeyCount(xTrack), 3u, "the undo puts it back");
	// ★ The SAME id, not a fresh one — a selection held across the delete has to
	// find its key again.
	ZENITH_ASSERT_EQ(xDoc.GetKeyIndexForId(xTrack, uIdB), 1u, "under its ORIGINAL id, at its original index");

	float fTime = 0.0f;
	Zenith_AnimKeyValue xValue;
	ZENITH_ASSERT_TRUE(xDoc.GetKeyTime(xTrack, uIdB, fTime) && xDoc.GetKeyValue(xTrack, uIdB, xValue), "and resolves");
	ZENITH_ASSERT_EQ_FLOAT(fTime, 1.0f, 1e-6f, "with its time restored");
	ZENITH_ASSERT_EQ_FLOAT(xValue.m_xVector.y, 1.0f, 1e-6f, "and its value restored");
}

//==============================================================================
// (4) D25: inserting onto an occupied time REPLACES the value and keeps the id.
//==============================================================================
ZENITH_TEST(AnimDocument, InsertOnAnOccupiedTimeKeepsTheExistingKeyId)
{
	AnimDocFixture xFixture("zenith_animdoc_occupied");
	AnimDocWriteProbeFile(xFixture.m_strPath, "DocProbe", 1.0f, false);

	Zenith_AnimationDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.Open(xFixture.m_strPath) == ZENITH_ANIMDOC_OPEN_OK, "the probe opens");

	const Zenith_AnimTrackId xTrack = AnimDocHipPositionTrack();
	const u_int uIdB = xDoc.GetKeyIdAtIndex(xTrack, 1);

	const u_int uReturned = xDoc.InsertKey(xTrack, 1.0f, Zenith_Maths::Vector3(0.0f, 7.0f, 0.0f));
	ZENITH_ASSERT_EQ(uReturned, uIdB, "the occupied slot's OWN id comes back — no new key, no new id");
	ZENITH_ASSERT_EQ(xDoc.GetKeyCount(xTrack), 3u, "and no key was added");

	Zenith_AnimKeyValue xValue;
	ZENITH_ASSERT_TRUE(xDoc.GetKeyValue(xTrack, uIdB, xValue), "the id still resolves");
	ZENITH_ASSERT_EQ_FLOAT(xValue.m_xVector.y, 7.0f, 1e-6f, "with the replacing value");

	// ★ And the undo restores the VALUE rather than deleting a key the user
	// never created — which is what the insert-vs-value command split is for.
	xDoc.Undo();
	ZENITH_ASSERT_EQ(xDoc.GetKeyCount(xTrack), 3u, "the undo does not delete the key");
	ZENITH_ASSERT_TRUE(xDoc.GetKeyValue(xTrack, uIdB, xValue), "the id still resolves after the undo");
	ZENITH_ASSERT_EQ_FLOAT(xValue.m_xVector.y, 1.0f, 1e-6f, "and the original value is back");
}

//==============================================================================
// (5) Save KEEPS the history; Save As clears it.
//==============================================================================
ZENITH_TEST(AnimDocument, SaveKeepsTheUndoHistoryAndSaveAsClearsIt)
{
	AnimDocFixture xFixture("zenith_animdoc_history");
	AnimDocWriteProbeFile(xFixture.m_strPath, "DocProbe", 1.0f, false);
	const std::string strSecondPath = xFixture.PathFor("copy.zanim");

	Zenith_AnimationDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.Open(xFixture.m_strPath) == ZENITH_ANIMDOC_OPEN_OK, "the probe opens");

	ZENITH_ASSERT_TRUE(xDoc.SetDuration(3.0f), "the duration edit lands");
	ZENITH_ASSERT_TRUE(xDoc.IsDirty(), "an edit dirties the document");
	ZENITH_ASSERT_TRUE(xDoc.CanUndo(), "and pushes a command");

	ZENITH_ASSERT_TRUE(xDoc.Save() == ZENITH_ANIMDOC_SAVE_OK, "the save succeeds");
	ZENITH_ASSERT_FALSE(xDoc.IsDirty(), "and clears the dirty flag");
	ZENITH_ASSERT_TRUE(xDoc.CanUndo(), "★ but the history SURVIVES a save");

	xDoc.Undo();
	ZENITH_ASSERT_EQ_FLOAT(xDoc.GetDuration(), 2.0f, 1e-6f, "undoing across the save works");
	ZENITH_ASSERT_TRUE(xDoc.IsDirty(), "and makes the document dirty again");

	xDoc.Redo();
	ZENITH_ASSERT_EQ_FLOAT(xDoc.GetDuration(), 3.0f, 1e-6f, "the redo re-applies it");
	ZENITH_ASSERT_TRUE(xDoc.Save() == ZENITH_ANIMDOC_SAVE_OK, "and it saves again");

	ZENITH_ASSERT_TRUE(xDoc.SaveAs(strSecondPath) == ZENITH_ANIMDOC_SAVE_OK, "Save As writes the new file");
	ZENITH_ASSERT_FALSE(xDoc.IsDirty(), "and the document is clean");
	ZENITH_ASSERT_FALSE(xDoc.CanUndo(), "★ but Save As CLEARS the history — it describes a file we no longer point at");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 0u, "the stack is empty");
	ZENITH_ASSERT_EQ(xDoc.GetAssetPath(), strSecondPath, "and the document is re-targeted at the new path");

	Flux_AnimationClip xParsed;
	ZENITH_ASSERT_TRUE(AnimDocParseFile(strSecondPath, xParsed), "the Save As file is a well-formed .zanim");
	ZENITH_ASSERT_EQ_FLOAT(xParsed.GetDuration(), 3.0f, 1e-6f, "carrying the edited duration");
}

//==============================================================================
// (6) Dirty flips on an edit and clears on a discard, and a discard drops the
// history with it.
//==============================================================================
ZENITH_TEST(AnimDocument, DirtyFlipsOnEditAndClearsOnDiscard)
{
	AnimDocFixture xFixture("zenith_animdoc_discard");
	AnimDocWriteProbeFile(xFixture.m_strPath, "DocProbe", 1.0f, false);

	Zenith_AnimationDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.Open(xFixture.m_strPath) == ZENITH_ANIMDOC_OPEN_OK, "the probe opens");
	ZENITH_ASSERT_FALSE(xDoc.IsDirty(), "clean on open");

	const Zenith_AnimTrackId xTrack = AnimDocHipPositionTrack();
	const u_int uIdB = xDoc.GetKeyIdAtIndex(xTrack, 1);
	ZENITH_ASSERT_TRUE(xDoc.SetKeyValue(xTrack, uIdB, Zenith_Maths::Vector3(0.0f, 99.0f, 0.0f)), "the edit lands");
	ZENITH_ASSERT_TRUE(xDoc.IsDirty(), "dirty after an edit");

	ZENITH_ASSERT_TRUE(xDoc.DiscardChanges(), "the discard succeeds");
	ZENITH_ASSERT_FALSE(xDoc.IsDirty(), "and the document is clean again");
	ZENITH_ASSERT_FALSE(xDoc.CanUndo(),
		"★ with no history: every command addresses ids this re-copy has just retired");

	// The ids were reallocated, so the row is addressed afresh — which is exactly
	// what a panel has to do after a discard.
	const u_int uFreshIdB = xDoc.GetKeyIdAtIndex(xTrack, 1);
	Zenith_AnimKeyValue xValue;
	ZENITH_ASSERT_TRUE(xDoc.GetKeyValue(xTrack, uFreshIdB, xValue), "the middle key is still there");
	ZENITH_ASSERT_EQ_FLOAT(xValue.m_xVector.y, 1.0f, 1e-6f, "back on the file's value");
	ZENITH_ASSERT_EQ(xDoc.GetKeyIndexForId(xTrack, uIdB), uINVALID_ANIM_KEY_INDEX,
		"and the retired id does NOT resolve — ids are never recycled");
}

//==============================================================================
// (7) A save writes the envelope, and the LIVE clip pointer observes it.
//==============================================================================
ZENITH_TEST(AnimDocument, SaveWritesTheEnvelopeAndTheLiveClipObservesIt)
{
	AnimDocFixture xFixture("zenith_animdoc_save");
	AnimDocWriteProbeFile(xFixture.m_strPath, "DocProbe", 1.0f, false);

	Zenith_AnimationDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.Open(xFixture.m_strPath) == ZENITH_ANIMDOC_OPEN_OK, "the probe opens");

	Zenith_AnimationAsset* pxAsset = xDoc.GetAsset();
	ZENITH_ASSERT_NOT_NULL(pxAsset, "the document holds the asset");
	if (pxAsset == nullptr)
	{
		return;
	}
	// ★ What a controller borrows (Flux_AnimationClipCollection::AddClipReference).
	Flux_AnimationClip* const pxLiveClip = pxAsset->GetClip();
	ZENITH_ASSERT_NOT_NULL(pxLiveClip, "and the asset holds a clip");

	const Zenith_AnimTrackId xTrack = AnimDocHipPositionTrack();
	const u_int uIdB = xDoc.GetKeyIdAtIndex(xTrack, 1);
	ZENITH_ASSERT_TRUE(xDoc.SetKeyValue(xTrack, uIdB, Zenith_Maths::Vector3(0.0f, 5.0f, 0.0f)), "the edit lands");
	ZENITH_ASSERT_EQ_FLOAT(AnimDocSampleHipY(pxLiveClip, 1.0f), 1.0f, 1e-5f, "and does NOT reach the live clip before the save");

	ZENITH_ASSERT_TRUE(xDoc.Save() == ZENITH_ANIMDOC_SAVE_OK, "the save succeeds");

	// The FILE, read back by the real reader — which refuses anything without a
	// current envelope, so this is what pins that the save wrote one.
	Flux_AnimationClip xParsed;
	ZENITH_ASSERT_TRUE(AnimDocParseFile(xFixture.m_strPath, xParsed), "the saved file parses as a .zanim");
	ZENITH_ASSERT_EQ_FLOAT(AnimDocSampleHipY(&xParsed, 1.0f), 5.0f, 1e-5f, "and carries the edited value");

	// ★ AND THE SAME LIVE POINTER SEES IT (D26). A force-unload + re-acquire would
	// have handed back a different address and left every borrower dangling.
	ZENITH_ASSERT_TRUE(pxAsset->GetClip() == pxLiveClip, "the save did not move the live clip");
	ZENITH_ASSERT_EQ_FLOAT(AnimDocSampleHipY(pxLiveClip, 1.0f), 5.0f, 1e-5f, "and that same pointer now samples the saved content");
}

//==============================================================================
// (8) An external rewrite is SURFACED, and a plain save refuses it.
//==============================================================================
ZENITH_TEST(AnimDocument, AnExternalRewriteIsSurfacedAndPlainSaveRefusesIt)
{
	AnimDocFixture xFixture("zenith_animdoc_conflict");
	AnimDocWriteProbeFile(xFixture.m_strPath, "DocProbe", 1.0f, false);

	Zenith_AnimationDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.Open(xFixture.m_strPath) == ZENITH_ANIMDOC_OPEN_OK, "the probe opens");
	ZENITH_ASSERT_FALSE(xDoc.HasExternalModification(), "nothing has touched the file yet");

	ZENITH_ASSERT_TRUE(xDoc.SetDuration(4.0f), "make a local edit worth protecting");

	// Somebody else rewrites the file — a re-bake, another editor, a git checkout.
	// SAME clip name (a rename is a separate, refused, thing) but different keys.
	AnimDocWriteProbeFile(xFixture.m_strPath, "DocProbe", 9.0f, false);
	ZENITH_ASSERT_TRUE(xDoc.HasExternalModification(), "the document notices");

	ZENITH_ASSERT_TRUE(xDoc.Save() == ZENITH_ANIMDOC_SAVE_CONFLICT_EXTERNAL,
		"★ a plain save REFUSES rather than silently replacing the other writer's work");
	ZENITH_ASSERT_TRUE(xDoc.IsDirty(), "and the document keeps its unsaved edit");

	Flux_AnimationClip xOnDisk;
	ZENITH_ASSERT_TRUE(AnimDocParseFile(xFixture.m_strPath, xOnDisk), "the file is still readable");
	ZENITH_ASSERT_EQ_FLOAT(AnimDocSampleHipY(&xOnDisk, 1.0f), 9.0f, 1e-5f, "and still holds the OTHER writer's content — nothing was written");

	// The explicit override is the panel's "overwrite anyway" button.
	ZENITH_ASSERT_TRUE(xDoc.SaveOverwritingExternal() == ZENITH_ANIMDOC_SAVE_OK, "the explicit overwrite goes through");
	ZENITH_ASSERT_FALSE(xDoc.IsDirty(), "and the document is clean");
	ZENITH_ASSERT_FALSE(xDoc.HasExternalModification(), "the recorded fingerprint moved with the save");

	ZENITH_ASSERT_TRUE(AnimDocParseFile(xFixture.m_strPath, xOnDisk), "the file re-parses");
	ZENITH_ASSERT_EQ_FLOAT(AnimDocSampleHipY(&xOnDisk, 1.0f), 1.0f, 1e-5f, "and now holds the document's working copy");
	ZENITH_ASSERT_EQ_FLOAT(xOnDisk.GetDuration(), 4.0f, 1e-6f, "including the local edit");
}

//==============================================================================
// (9) D21: a generated clip is refused; promotion produces an editable copy and
// leaves the source BYTE-IDENTICAL.
//==============================================================================
ZENITH_TEST(AnimDocument, AGeneratedClipIsRefusedAndPromotionLeavesTheSourceByteIdentical)
{
	AnimDocFixture xFixture("zenith_animdoc_generated");
	AnimDocWriteProbeFile(xFixture.m_strPath, "GenProbe", 1.0f, true);

	Zenith_Vector<uint8_t> axSourceBefore;
	ZENITH_ASSERT_TRUE(AnimDocReadFileBytes(xFixture.m_strPath, axSourceBefore), "the generated source is on disk");

	Zenith_AnimationDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.Open(xFixture.m_strPath) == ZENITH_ANIMDOC_OPEN_REFUSED_GENERATED,
		"★ a GENERATED clip is refused — the next tools boot would rewrite the edit away");
	ZENITH_ASSERT_FALSE(xDoc.IsOpen(), "and nothing was opened");

	// ★ The authored-root override keeps this test out of the tracked asset tree.
	// Without it the promotion would write into Zenith/Assets/Authored/, which is
	// not somewhere a unit test may leave a file.
	const std::string strAuthoredRoot = (xFixture.m_xDirectory / "Authored").generic_string();
	xDoc.SetAuthoredRootOverride(strAuthoredRoot);
	const std::string strPromoted = strAuthoredRoot + "/doc.zanim";
	xFixture.Track(strPromoted);

	ZENITH_ASSERT_EQ(xDoc.ResolveAuthoredOverridePath(xFixture.m_strPath), strPromoted, "the override decides where it lands");
	ZENITH_ASSERT_TRUE(xDoc.PromoteToAuthoredOverride(xFixture.m_strPath) == ZENITH_ANIMDOC_OPEN_OK, "the promotion opens the copy");
	ZENITH_ASSERT_TRUE(xDoc.IsOpen(), "the document is now on the override");
	ZENITH_ASSERT_EQ(xDoc.GetAssetPath(), strPromoted, "at the promoted path");
	ZENITH_ASSERT_FALSE(xDoc.GetClip().GetMetadata().m_bGenerated, "★ with the generated flag CLEARED, so it is editable");
	ZENITH_ASSERT_FALSE(xDoc.IsDirty(), "and clean");

	// ★ THE BAKE STILL OWNS THE SOURCE. Promotion copies; it must not edit,
	// truncate or re-serialize the file the generator writes.
	Zenith_Vector<uint8_t> axSourceAfter;
	ZENITH_ASSERT_TRUE(AnimDocReadFileBytes(xFixture.m_strPath, axSourceAfter), "the source is still there");
	ZENITH_ASSERT_TRUE(AnimDocBytesEqual(axSourceBefore, axSourceAfter), "and is byte-identical");

	// The promoted document takes edits like any other.
	const Zenith_AnimTrackId xTrack = AnimDocHipPositionTrack();
	ZENITH_ASSERT_EQ(xDoc.GetKeyCount(xTrack), 3u, "the keys came across");
	ZENITH_ASSERT_TRUE(xDoc.SetDuration(5.0f), "and the copy is writable");
	ZENITH_ASSERT_TRUE(xDoc.IsDirty(), "which dirties it");
	ZENITH_ASSERT_TRUE(xDoc.Save() == ZENITH_ANIMDOC_SAVE_OK, "and it saves back to the OVERRIDE");

	Flux_AnimationClip xParsed;
	ZENITH_ASSERT_TRUE(AnimDocParseFile(strPromoted, xParsed), "the override file parses");
	ZENITH_ASSERT_FALSE(xParsed.GetMetadata().m_bGenerated, "and its generated flag stays clear on disk");
	ZENITH_ASSERT_EQ_FLOAT(xParsed.GetDuration(), 5.0f, 1e-6f, "with the edit in it");

	ZENITH_ASSERT_TRUE(AnimDocReadFileBytes(xFixture.m_strPath, axSourceAfter), "the source survives the save too");
	ZENITH_ASSERT_TRUE(AnimDocBytesEqual(axSourceBefore, axSourceAfter), "still byte-identical");
}

//==============================================================================
// (10) PURE: the promoted path keeps the asset ROOT and the SUBDIRECTORY.
//==============================================================================
ZENITH_TEST(AnimDocument, TheAuthoredOverridePathKeepsTheRootAndTheSubdirectory)
{
	ZENITH_ASSERT_EQ(Zenith_AnimationDocument::BuildAuthoredAssetPath("engine:Meshes/StickFigure/Walk.zanim"),
		std::string("engine:Authored/Meshes/StickFigure/Walk.zanim"),
		"an engine source lands under Zenith/Assets/Authored/");
	ZENITH_ASSERT_EQ(Zenith_AnimationDocument::BuildAuthoredAssetPath("game:Anims/Run.zanim"),
		std::string("game:Authored/Anims/Run.zanim"),
		"a game source lands under Games/<Game>/Assets/Authored/ — with no game name written down");

	// ★ THE SUBDIRECTORY IS PRESERVED, not flattened to a leaf name. Two
	// generated sets routinely both hold a clip called "Walk"; flattening would
	// have one silently overwrite the other.
	ZENITH_ASSERT_NE(Zenith_AnimationDocument::BuildAuthoredAssetPath("engine:Meshes/Bushes/Walk.zanim"),
		Zenith_AnimationDocument::BuildAuthoredAssetPath("engine:Meshes/StickFigure/Walk.zanim"),
		"two clips with the same leaf name do not collide");

	ZENITH_ASSERT_EQ(Zenith_AnimationDocument::BuildAuthoredAssetPath("engine:Authored/Meshes/Walk.zanim"),
		std::string("engine:Authored/Meshes/Walk.zanim"),
		"promoting an override again is idempotent, not Authored/Authored/");

	ZENITH_ASSERT_TRUE(Zenith_AnimationDocument::BuildAuthoredAssetPath("C:/tmp/loose.zanim").empty(),
		"a path under no recognised asset root has no derivable override location");
}

//==============================================================================
// (11) Close refuses while dirty; the forced close clears everything.
//==============================================================================
ZENITH_TEST(AnimDocument, CloseRefusesWhileDirtyAndTheForcedCloseClearsTheHistory)
{
	AnimDocFixture xFixture("zenith_animdoc_close");
	AnimDocWriteProbeFile(xFixture.m_strPath, "DocProbe", 1.0f, false);

	Zenith_AnimationDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.Open(xFixture.m_strPath) == ZENITH_ANIMDOC_OPEN_OK, "the probe opens");
	ZENITH_ASSERT_TRUE(xDoc.SetDuration(6.0f), "an edit lands");

	ZENITH_ASSERT_TRUE(xDoc.Close() == ZENITH_ANIMDOC_CLOSE_REFUSED_DIRTY, "closing a dirty document is refused");
	ZENITH_ASSERT_TRUE(xDoc.IsOpen(), "and changes nothing — the document is still open");
	ZENITH_ASSERT_TRUE(xDoc.IsDirty(), "and still dirty");
	ZENITH_ASSERT_EQ_FLOAT(xDoc.GetDuration(), 6.0f, 1e-6f, "with the edit intact");

	// Opening something else over unsaved work is refused for the same reason.
	ZENITH_ASSERT_TRUE(xDoc.Open(xFixture.m_strPath) == ZENITH_ANIMDOC_OPEN_REFUSED_DIRTY, "and so is re-opening over it");

	xDoc.CloseDiscardingChanges();
	ZENITH_ASSERT_FALSE(xDoc.IsOpen(), "the forced close closes it");
	ZENITH_ASSERT_FALSE(xDoc.IsDirty(), "clean");
	// ★ The commands are DELETED here, which is what makes their raw document
	// pointer safe: none of them can outlive the document.
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 0u, "and the undo stack is emptied");
	ZENITH_ASSERT_EQ(xDoc.GetRedoStackSize(), 0u, "redo too");

	// A closed document reopens.
	ZENITH_ASSERT_TRUE(xDoc.Open(xFixture.m_strPath) == ZENITH_ANIMDOC_OPEN_OK, "and it reopens");
	ZENITH_ASSERT_EQ_FLOAT(xDoc.GetDuration(), 2.0f, 1e-6f, "on the file's content, the discarded edit gone");
}

//==============================================================================
// (12) Event ids survive a retime that REORDERS the event list.
//==============================================================================
ZENITH_TEST(AnimDocument, EventIdsSurviveARetimeThatReordersTheEventList)
{
	AnimDocFixture xFixture("zenith_animdoc_events");
	AnimDocWriteProbeFile(xFixture.m_strPath, "DocProbe", 1.0f, false);

	Zenith_AnimationDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.Open(xFixture.m_strPath) == ZENITH_ANIMDOC_OPEN_OK, "the probe opens");
	ZENITH_ASSERT_EQ(xDoc.GetEventCount(), 0u, "the probe has no events");

	const u_int uFootstep = xDoc.AddEvent("Footstep", 0.25f, Zenith_Maths::Vector4(1.0f, 0.0f, 0.0f, 0.0f));
	const u_int uSwing = xDoc.AddEvent("Swing", 0.75f, Zenith_Maths::Vector4(2.0f, 0.0f, 0.0f, 0.0f));
	ZENITH_ASSERT_NE(uFootstep, uINVALID_ANIM_KEY_ID, "the first event has an id");
	ZENITH_ASSERT_NE(uSwing, uFootstep, "and the second a different one");
	ZENITH_ASSERT_EQ(xDoc.GetEventCount(), 2u, "both landed");
	ZENITH_ASSERT_EQ(xDoc.GetEventIndexForId(uFootstep), 0u, "Footstep is first (the clip sorts by time)");
	ZENITH_ASSERT_EQ(xDoc.GetEventIndexForId(uSwing), 1u, "Swing second");

	// Move Footstep past Swing — the same reorder problem keys have.
	ZENITH_ASSERT_TRUE(xDoc.SetEventTime(uFootstep, 0.9f), "the event retime lands");
	ZENITH_ASSERT_EQ(xDoc.GetEventIndexForId(uSwing), 0u, "Swing is now first");
	ZENITH_ASSERT_EQ(xDoc.GetEventIndexForId(uFootstep), 1u, "and Footstep second");

	Flux_AnimationEvent xEvent;
	ZENITH_ASSERT_TRUE(xDoc.GetEvent(uFootstep, xEvent), "the id still resolves");
	ZENITH_ASSERT_EQ(xEvent.m_strEventName, std::string("Footstep"), "to the same event, not to whoever now sits at index 1");
	ZENITH_ASSERT_EQ_FLOAT(xEvent.m_fNormalizedTime, 0.9f, 1e-6f, "at its new normalized time");

	xDoc.Undo();
	ZENITH_ASSERT_EQ(xDoc.GetEventIndexForId(uFootstep), 0u, "the undo puts it back first");
	ZENITH_ASSERT_TRUE(xDoc.GetEvent(uFootstep, xEvent), "and it still resolves");
	ZENITH_ASSERT_EQ_FLOAT(xEvent.m_fNormalizedTime, 0.25f, 1e-6f, "at its original time");

	// Remove + undo restores the event under its original id, exactly like a key.
	ZENITH_ASSERT_TRUE(xDoc.RemoveEvent(uSwing), "the second event is removed");
	ZENITH_ASSERT_EQ(xDoc.GetEventCount(), 1u, "leaving one");
	xDoc.Undo();
	ZENITH_ASSERT_EQ(xDoc.GetEventCount(), 2u, "the undo puts it back");
	ZENITH_ASSERT_TRUE(xDoc.GetEvent(uSwing, xEvent), "under its ORIGINAL id");
	ZENITH_ASSERT_EQ(xEvent.m_strEventName, std::string("Swing"), "and it is the same event");
}

//==============================================================================
// (13) Root motion takes the same verbs, and refuses a scale track (D16).
//==============================================================================
ZENITH_TEST(AnimDocument, RootMotionTakesTheSameVerbsAndRefusesAScaleTrack)
{
	AnimDocFixture xFixture("zenith_animdoc_rootmotion");
	AnimDocWriteProbeFile(xFixture.m_strPath, "DocProbe", 1.0f, false);

	Zenith_AnimationDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.Open(xFixture.m_strPath) == ZENITH_ANIMDOC_OPEN_OK, "the probe opens");

	const Zenith_AnimTrackId xRootPos = Zenith_AnimTrackId::RootMotion(FLUX_ANIM_TRACK_POSITION);
	ZENITH_ASSERT_EQ(xDoc.GetKeyCount(xRootPos), 0u, "the probe carries no root motion");

	const u_int uIdA = xDoc.InsertKey(xRootPos, 0.0f, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	const u_int uIdB = xDoc.InsertKey(xRootPos, 1.0f, Zenith_Maths::Vector3(0.0f, 0.0f, 3.0f));
	ZENITH_ASSERT_NE(uIdA, uINVALID_ANIM_KEY_ID, "a root-motion key gets an id like any other");
	ZENITH_ASSERT_NE(uIdB, uIdA, "and a distinct one");
	ZENITH_ASSERT_EQ(xDoc.GetKeyCount(xRootPos), 2u, "both landed");

	Zenith_AnimKeyValue xValue;
	ZENITH_ASSERT_TRUE(xDoc.GetKeyValue(xRootPos, uIdB, xValue), "the id resolves");
	ZENITH_ASSERT_EQ_FLOAT(xValue.m_xVector.z, 3.0f, 1e-6f, "to its own delta");

	// ★ D16: root motion has a position and a rotation delta track and no third
	// one. The document refuses the scale track outright rather than passing it
	// down to be asserted on.
	const Zenith_AnimTrackId xRootScale = Zenith_AnimTrackId::RootMotion(FLUX_ANIM_TRACK_SCALE);
	ZENITH_ASSERT_FALSE(xDoc.TrackExists(xRootScale), "there is no root-motion scale track");
	ZENITH_ASSERT_EQ(xDoc.GetKeyCount(xRootScale), 0u, "it holds nothing");
	ZENITH_ASSERT_EQ(xDoc.InsertKey(xRootScale, 0.0f, Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f)), uINVALID_ANIM_KEY_ID,
		"and an insert onto it is refused");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 2u, "the refusal pushed no command");

	xDoc.Undo();
	ZENITH_ASSERT_EQ(xDoc.GetKeyCount(xRootPos), 1u, "the undo removes the second root-motion key");
	ZENITH_ASSERT_EQ(xDoc.GetKeyIndexForId(xRootPos, uIdA), 0u, "and leaves the first resolvable");
}

//==============================================================================
// (14) TANGENTS (WU-8.2): every key of a freshly opened clip reads as LINEAR,
// one edit is one undo step, and its undo restores the UNSET pair exactly.
//
// ★ THE "UNSET" HALF IS THE LOAD-BEARING ONE. WU-8.1's whole change rests on
// "exactly zero means linear, and a segment bounded by two of them runs the
// pre-tangent expression verbatim" — so an undo that restored anything OTHER
// than exact zeroes would take a clip off that branch permanently, with no data
// visibly wrong and every other unit green.
//==============================================================================
ZENITH_TEST(AnimDocument, TangentEditsAreOneStepEachAndUndoRestoresTheUnsetPairExactly)
{
	AnimDocFixture xFixture("zenith_animdoc_tangents");
	AnimDocWriteProbeFile(xFixture.m_strPath, "DocProbe", 1.0f, false);

	Zenith_AnimationDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.Open(xFixture.m_strPath) == ZENITH_ANIMDOC_OPEN_OK, "the probe opens");

	const Zenith_AnimTrackId xTrack = AnimDocHipPositionTrack();
	const u_int uMiddleId = xDoc.GetKeyIdAtIndex(xTrack, 1);

	Flux_KeyTangents xTangents;
	ZENITH_ASSERT_TRUE(xDoc.GetKeyTangents(xTrack, uMiddleId, xTangents), "a key's tangent pair reads back");
	ZENITH_ASSERT_TRUE(Flux_TangentIsUnset(xTangents.m_xInTangent),
		"★ and every key of a clip nobody authored a tangent on is UNSET — which is LINEAR, not flat");
	ZENITH_ASSERT_TRUE(Flux_TangentIsUnset(xTangents.m_xOutTangent), "on both ends");

	Flux_KeyTangents xEdited;
	xEdited.m_xInTangent = Zenith_Maths::Vector3(0.0f, 2.0f, 0.0f);
	xEdited.m_xOutTangent = Zenith_Maths::Vector3(0.0f, 2.0f, 0.0f);
	ZENITH_ASSERT_TRUE(xDoc.SetKeyTangents(xTrack, uMiddleId, xEdited), "one tangent edit lands");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 1u, "as exactly ONE undo step");
	ZENITH_ASSERT_TRUE(xDoc.IsDirty(), "and it dirties the document");

	// ★ THE ASSIGNMENT RULE: re-stating the same pair is SATISFIED, not refused,
	// and pushes nothing. The stack DEPTH is the invariant, not the bool.
	ZENITH_ASSERT_TRUE(xDoc.SetKeyTangents(xTrack, uMiddleId, xEdited), "re-stating it is satisfied");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 1u, "and pushes nothing");

	// The two conveniences are still one step each, and they touch one half only.
	ZENITH_ASSERT_TRUE(xDoc.SetKeyInTangent(xTrack, uMiddleId, Zenith_Maths::Vector3(0.0f, 5.0f, 0.0f)),
		"an in-only edit lands");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 2u, "as one more step");
	ZENITH_ASSERT_TRUE(xDoc.GetKeyTangents(xTrack, uMiddleId, xTangents), "and reads back");
	ZENITH_ASSERT_EQ_FLOAT(xTangents.m_xInTangent.y, 5.0f, 1e-6f, "with the in tangent moved");
	ZENITH_ASSERT_EQ_FLOAT(xTangents.m_xOutTangent.y, 2.0f, 1e-6f, "and the OUT one left exactly alone");

	xDoc.Undo();
	xDoc.Undo();
	ZENITH_ASSERT_TRUE(xDoc.GetKeyTangents(xTrack, uMiddleId, xTangents), "the key still resolves after both undos");
	ZENITH_ASSERT_TRUE(Flux_TangentIsUnset(xTangents.m_xInTangent),
		"★ and the pair is back to EXACT zero — the sampler's bit-identical linear branch, not merely close");
	ZENITH_ASSERT_TRUE(Flux_TangentIsUnset(xTangents.m_xOutTangent), "on both ends");

	xDoc.Redo();
	ZENITH_ASSERT_TRUE(xDoc.GetKeyTangents(xTrack, uMiddleId, xTangents), "and a redo re-applies");
	ZENITH_ASSERT_EQ_FLOAT(xTangents.m_xOutTangent.y, 2.0f, 1e-6f, "the pair it recorded");
}

//==============================================================================
// (15) Auto on three COLLINEAR keys yields the slope; Linear zeroes; each is ONE
// compound whose undo restores every key's previous pair.
//
// ★ COLLINEAR IS THE FIXTURE THAT MAKES THE ANSWER PREDICTABLE AT BOTH ENDS.
// The centred slope through key 1 and the one-sided slopes at keys 0 and 2 are
// all the same number only when the three keys are on a line — so the same
// expectation covers the interior case and the two endpoint cases, and a wrong
// endpoint rule fails here rather than passing by looking plausible.
//==============================================================================
ZENITH_TEST(AnimDocument, AutoTangentsOnCollinearKeysAreTheSlopeAndLinearZeroesThem)
{
	AnimDocFixture xFixture("zenith_animdoc_autotangents");
	// y = 0 / 1 / 2 at t = 0 / 1 / 2, so the slope is exactly 1 unit per second.
	AnimDocWriteProbeFile(xFixture.m_strPath, "DocProbe", 1.0f, false);

	Zenith_AnimationDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.Open(xFixture.m_strPath) == ZENITH_ANIMDOC_OPEN_OK, "the probe opens");
	const Zenith_AnimTrackId xTrack = AnimDocHipPositionTrack();

	ZENITH_ASSERT_TRUE(xDoc.SetTrackTangentsAuto(xTrack), "the whole-track Auto preset runs");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 1u,
		"★ as ONE compound, not one step per key — a preset is one gesture");

	for (u_int u = 0; u < 3u; ++u)
	{
		Flux_KeyTangents xTangents;
		ZENITH_ASSERT_TRUE(xDoc.GetKeyTangents(xTrack, xDoc.GetKeyIdAtIndex(xTrack, u), xTangents),
			"each key's pair reads back");
		ZENITH_ASSERT_EQ_FLOAT(xTangents.m_xInTangent.y, 1.0f, 1e-5f,
			"the centred slope through a collinear key IS the line's slope — and so is the one-sided "
			"slope at either end");
		ZENITH_ASSERT_EQ_FLOAT(xTangents.m_xOutTangent.y, 1.0f, 1e-5f, "in == out: Auto is a SMOOTH key");
	}

	// Per-key Auto agrees with the whole-track preset. It has to: the panel's
	// selection verb uses the per-key one, and a second formula that disagreed
	// would make "Auto" mean two things depending on which control was clicked.
	ZENITH_ASSERT_TRUE(xDoc.SetKeyTangentsAuto(xTrack, xDoc.GetKeyIdAtIndex(xTrack, 1)),
		"per-key Auto is satisfied on a key the preset already did");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 1u, "and pushes nothing, because nothing changed");

	ZENITH_ASSERT_TRUE(xDoc.SetTrackTangentsLinear(xTrack), "the Linear preset runs");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 2u, "as one more compound");
	for (u_int u = 0; u < 3u; ++u)
	{
		Flux_KeyTangents xTangents;
		ZENITH_ASSERT_TRUE(xDoc.GetKeyTangents(xTrack, xDoc.GetKeyIdAtIndex(xTrack, u), xTangents), "reads back");
		ZENITH_ASSERT_TRUE(Flux_TangentIsUnset(xTangents.m_xInTangent),
			"★ Linear writes exact ZEROES — which is the sampler's linear branch, and is why the control "
			"may not be labelled 'Flat': a flat handle is unrepresentable");
		ZENITH_ASSERT_TRUE(Flux_TangentIsUnset(xTangents.m_xOutTangent), "on both ends");
	}

	// ★ THE UNDO RESTORES EVERY KEY'S PREVIOUS PAIR, which is the whole reason the
	// preset captures them per key instead of re-deriving Auto on the way back.
	xDoc.Undo();
	for (u_int u = 0; u < 3u; ++u)
	{
		Flux_KeyTangents xTangents;
		ZENITH_ASSERT_TRUE(xDoc.GetKeyTangents(xTrack, xDoc.GetKeyIdAtIndex(xTrack, u), xTangents), "reads back");
		ZENITH_ASSERT_EQ_FLOAT(xTangents.m_xInTangent.y, 1.0f, 1e-5f, "the auto slope is back on every key");
	}
	xDoc.Undo();
	for (u_int u = 0; u < 3u; ++u)
	{
		Flux_KeyTangents xTangents;
		ZENITH_ASSERT_TRUE(xDoc.GetKeyTangents(xTrack, xDoc.GetKeyIdAtIndex(xTrack, u), xTangents), "reads back");
		ZENITH_ASSERT_TRUE(Flux_TangentIsUnset(xTangents.m_xOutTangent),
			"and a second undo is back at the unset pair the file carried");
	}
}

//==============================================================================
// (16) ROOT MOTION HAS NO TANGENTS, AND EVERY VERB REFUSES IT.
//
// ★ THIS IS THE ONE THAT STOPS A CURVE EDITOR OFFERING A CONTROL WITH NOTHING
// BEHIND IT. Flux_RootMotion carries no parallel Flux_KeyTangents array (D17) and
// is still sampled linearly after WU-8.1 — so a tangent authored on one of its
// two delta tracks would be a value nothing ever reads, saved into a file and
// visible in a UI, with every gate green.
//==============================================================================
ZENITH_TEST(AnimDocument, RootMotionRefusesEveryTangentVerb)
{
	AnimDocFixture xFixture("zenith_animdoc_rootmotiontangents");
	AnimDocWriteProbeFile(xFixture.m_strPath, "DocProbe", 1.0f, false);

	Zenith_AnimationDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.Open(xFixture.m_strPath) == ZENITH_ANIMDOC_OPEN_OK, "the probe opens");

	const Zenith_AnimTrackId xRootPos = Zenith_AnimTrackId::RootMotion(FLUX_ANIM_TRACK_POSITION);
	const u_int uRootKeyId = xDoc.InsertKey(xRootPos, 0.0f, Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));
	ZENITH_ASSERT_NE(uRootKeyId, uINVALID_ANIM_KEY_ID, "a root-motion key exists to aim at");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 1u, "one step so far");

	Flux_KeyTangents xTangents;
	ZENITH_ASSERT_FALSE(xDoc.GetKeyTangents(xRootPos, uRootKeyId, xTangents),
		"★ reading a root-motion tangent is a REFUSAL, not an all-zero answer — 'this track's tangents "
		"are zero' and 'this track cannot hold one' are different facts");

	Flux_KeyTangents xEdited;
	xEdited.m_xOutTangent = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
	ZENITH_ASSERT_FALSE(xDoc.SetKeyTangents(xRootPos, uRootKeyId, xEdited), "and writing one is refused");
	ZENITH_ASSERT_FALSE(xDoc.SetKeyInTangent(xRootPos, uRootKeyId, Zenith_Maths::Vector3(1.0f)), "as is the in half");
	ZENITH_ASSERT_FALSE(xDoc.SetKeyOutTangent(xRootPos, uRootKeyId, Zenith_Maths::Vector3(1.0f)), "and the out half");
	ZENITH_ASSERT_FALSE(xDoc.SetKeyTangentsAuto(xRootPos, uRootKeyId), "and per-key Auto");
	ZENITH_ASSERT_FALSE(xDoc.SetTrackTangentsAuto(xRootPos), "and the whole-track presets");
	ZENITH_ASSERT_FALSE(xDoc.SetTrackTangentsLinear(xRootPos), "both of them");

	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 1u, "★ and not one of those refusals pushed a command");
}
