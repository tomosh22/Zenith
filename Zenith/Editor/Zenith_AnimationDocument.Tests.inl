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
#include "UnitTests/Zenith_AssertCapture.h"   // B2: proving the AUTO refresh does NOT assert
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
	ZENITH_ASSERT_TRUE(xTangents.m_eInMode == Flux_TangentMode::LINEAR,
		"★ and every key of a clip nobody authored a tangent on is LINEAR — which since schema 3 is a "
		"STORED byte rather than a fact inferred from two zeroes, and is genuinely distinct from the "
		"FLAT the same zeroes could now be carrying");
	ZENITH_ASSERT_TRUE(xTangents.m_eOutMode == Flux_TangentMode::LINEAR, "on both ends");

	Flux_KeyTangents xEdited;
	xEdited.m_xInTangent = Zenith_Maths::Vector3(0.0f, 2.0f, 0.0f);
	xEdited.m_xOutTangent = Zenith_Maths::Vector3(0.0f, 2.0f, 0.0f);
	// ★ THE CALLER STATES THE MODE (B2). SetKeyTangents stores all four fields
	// verbatim, so a struct left on its LINEAR default would be an instruction to
	// IGNORE the two numbers beside it — an edit that reached the file and never
	// reached the pose.
	xEdited.m_eInMode = Flux_TangentMode::CUSTOM;
	xEdited.m_eOutMode = Flux_TangentMode::CUSTOM;
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
	ZENITH_ASSERT_TRUE(xTangents.m_eInMode == Flux_TangentMode::CUSTOM,
		"★ a DRAG marks the end it moved CUSTOM (B2) — the convenience fills the mode the caller has no "
		"struct to state it in");

	xDoc.Undo();
	xDoc.Undo();
	ZENITH_ASSERT_TRUE(xDoc.GetKeyTangents(xTrack, uMiddleId, xTangents), "the key still resolves after both undos");
	ZENITH_ASSERT_TRUE(xTangents.m_xInTangent == Zenith_Maths::Vector3(0.0f)
		&& xTangents.m_eInMode == Flux_TangentMode::LINEAR,
		"★ and the pair is back to EXACT zero AND to LINEAR — the sampler's bit-identical branch, not "
		"merely close, and not a zero vector left carrying a CUSTOM mode");
	ZENITH_ASSERT_TRUE(xTangents.m_xOutTangent == Zenith_Maths::Vector3(0.0f)
		&& xTangents.m_eOutMode == Flux_TangentMode::LINEAR, "on both ends");

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
		ZENITH_ASSERT_TRUE(xTangents.m_eInMode == Flux_TangentMode::AUTO
			&& xTangents.m_eOutMode == Flux_TangentMode::AUTO,
			"★ and the key reads back AUTO (B2), not CUSTOM — 'Auto' is a property this key HAS now, "
			"which is what lets the document recompute it when a neighbour moves");
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
		ZENITH_ASSERT_TRUE(xTangents.m_xInTangent == Zenith_Maths::Vector3(0.0f)
			&& xTangents.m_eInMode == Flux_TangentMode::LINEAR,
			"★ Linear writes exact ZEROES and the mode LINEAR beside them (B2) — the verb and the preset "
			"it calls finally have the same name, and the mode is STATED rather than inferred from the "
			"zeroes it happens to sit on");
		ZENITH_ASSERT_TRUE(xTangents.m_eOutMode == Flux_TangentMode::LINEAR, "on both ends");
	}

	// ★★ AND Flat WRITES THE SAME SIX FLOATS AND MEANS THE OPPOSITE (B2). This verb
	// did not exist while the mode was derived from the vector, because there was no
	// way for it to differ from Linear by anything a file could carry.
	ZENITH_ASSERT_TRUE(xDoc.SetTrackTangentsFlat(xTrack), "the Flat preset runs");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 3u, "as one more compound");
	for (u_int u = 0; u < 3u; ++u)
	{
		Flux_KeyTangents xTangents;
		ZENITH_ASSERT_TRUE(xDoc.GetKeyTangents(xTrack, xDoc.GetKeyIdAtIndex(xTrack, u), xTangents), "reads back");
		ZENITH_ASSERT_TRUE(xTangents.m_eInMode == Flux_TangentMode::FLAT
			&& xTangents.m_eOutMode == Flux_TangentMode::FLAT,
			"key %u is FLAT on both ends — a genuine zero derivative, the ease that used to be "
			"unrepresentable", u);
		ZENITH_ASSERT_TRUE(xTangents.m_xInTangent == Zenith_Maths::Vector3(0.0f)
			&& xTangents.m_xOutTangent == Zenith_Maths::Vector3(0.0f),
			"with the VERY SAME zero vectors the Linear preset wrote, on key %u — which is why a mode-blind "
			"comparison would have called this whole gesture a no-op", u);
	}
	// The pose is the proof the mode reached the sampler: on the probe's straight
	// line (y = 0/1/2 at t = 0/1/2) a lerp reads 0.25 at t = 0.25 and a both-ends-flat
	// Hermite reads 3u^2 - 2u^3 = 0.15625.
	ZENITH_ASSERT_EQ_FLOAT(AnimDocSampleHipY(&xDoc.GetClip(), 0.25f), 0.15625f, 1e-4f,
		"★ and the FLAT track EASES — the same numbers as Linear, a different curve");

	xDoc.Undo();
	ZENITH_ASSERT_EQ_FLOAT(AnimDocSampleHipY(&xDoc.GetClip(), 0.25f), 0.25f, 1e-4f,
		"and one Ctrl+Z puts the track back on the straight line");

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
		ZENITH_ASSERT_TRUE(xTangents.m_xOutTangent == Zenith_Maths::Vector3(0.0f)
			&& xTangents.m_eOutMode == Flux_TangentMode::LINEAR,
			"and a second undo is back at the LINEAR zero pair the file carried");
	}
}

//==============================================================================
// (15b) B1 — TangentsEqual SEES THE MODES, and re-stating an edit still pushes
// nothing.
//
// ★ THE COMPARISON IS AGAINST THE RAW REQUEST NOW (B2), AND THAT IS THE CHANGE.
// It used to derive the modes on the requested pair first, because the channel
// setter was about to do the same and the comparison had to predict what would
// actually be stored. Nothing derives any more: what a caller asks for is what is
// stored, so comparing the request as given IS comparing against the future state —
// and a request whose modes differ from the stored ones is a REAL edit rather than
// a struct that had not been tidied up yet.
//==============================================================================
ZENITH_TEST(AnimDocument, TangentsEqualSeesTheModesAndReStatingAnEditStillPushesNothing)
{
	// ---- pure first: equal vectors, different modes are NOT equal --------------
	Flux_KeyTangents xLinearPair;
	Flux_KeyTangents xFlatPair;
	xFlatPair.m_eInMode = Flux_TangentMode::FLAT;
	ZENITH_ASSERT_TRUE(xLinearPair.m_xInTangent == xFlatPair.m_xInTangent
		&& xLinearPair.m_xOutTangent == xFlatPair.m_xOutTangent,
		"fixture: the two pairs carry IDENTICAL vectors");
	ZENITH_ASSERT_FALSE(Zenith_AnimationDocument::TangentsEqual(xLinearPair, xFlatPair),
		"★ a LINEAR end and a FLAT end with the same zero vector are NOT the same tangent — they sample "
		"differently, so a comparison that missed it would report a real edit as a no-op");

	Flux_KeyTangents xAutoPair;
	xAutoPair.m_xOutTangent = Zenith_Maths::Vector3(0.0f, 2.0f, 0.0f);
	xAutoPair.m_eOutMode = Flux_TangentMode::AUTO;
	Flux_KeyTangents xCustomPair;
	xCustomPair.m_xOutTangent = Zenith_Maths::Vector3(0.0f, 2.0f, 0.0f);
	xCustomPair.m_eOutMode = Flux_TangentMode::CUSTOM;
	ZENITH_ASSERT_FALSE(Zenith_AnimationDocument::TangentsEqual(xAutoPair, xCustomPair),
		"and AUTO differs from CUSTOM even though both read the same stored vector — the difference is "
		"PROVENANCE, and an undo has to be able to restore it");

	Flux_KeyTangents xSame;
	xSame.m_xOutTangent = Zenith_Maths::Vector3(0.0f, 2.0f, 0.0f);
	xSame.m_eOutMode = Flux_TangentMode::CUSTOM;
	ZENITH_ASSERT_TRUE(Zenith_AnimationDocument::TangentsEqual(xSame, xCustomPair),
		"two pairs agreeing on all four fields ARE equal");

	// ---- and through the document, where the request is stored as given ----------
	AnimDocFixture xFixture("zenith_animdoc_tangentmodes");
	AnimDocWriteProbeFile(xFixture.m_strPath, "DocProbe", 1.0f, false);

	Zenith_AnimationDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.Open(xFixture.m_strPath) == ZENITH_ANIMDOC_OPEN_OK, "the probe opens");

	const Zenith_AnimTrackId xTrack = AnimDocHipPositionTrack();
	const u_int uMiddleId = xDoc.GetKeyIdAtIndex(xTrack, 1);

	// A caller's struct with the mode STATED, which is what every production caller
	// does since B2 — the curve panel's Action_SetKeyTangents fills CUSTOM on both
	// ends, and its handle drag goes through SetKeyIn/OutTangent, which fills one.
	Flux_KeyTangents xRequested;
	xRequested.m_xInTangent  = Zenith_Maths::Vector3(0.0f, 3.0f, 0.0f);
	xRequested.m_xOutTangent = Zenith_Maths::Vector3(0.0f, 3.0f, 0.0f);
	xRequested.m_eInMode  = Flux_TangentMode::CUSTOM;
	xRequested.m_eOutMode = Flux_TangentMode::CUSTOM;

	ZENITH_ASSERT_TRUE(xDoc.SetKeyTangents(xTrack, uMiddleId, xRequested), "the edit lands");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 1u, "as one undo step");

	Flux_KeyTangents xStored;
	ZENITH_ASSERT_TRUE(xDoc.GetKeyTangents(xTrack, uMiddleId, xStored), "and reads back");
	ZENITH_ASSERT_TRUE(xStored.m_eInMode == Flux_TangentMode::CUSTOM
		&& xStored.m_eOutMode == Flux_TangentMode::CUSTOM,
		"★ with the modes STORED AS ASKED FOR (B2), not re-derived from the vectors on the way in");

	// ★ RE-STATING THE VERY SAME REQUEST IS SATISFIED AND PUSHES NOTHING. The
	// assignment rule is unchanged by B2; what changed is that the comparison no
	// longer has to predict a derivation to get here.
	ZENITH_ASSERT_TRUE(xDoc.SetKeyTangents(xTrack, uMiddleId, xRequested), "re-stating it is SATISFIED");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 1u, "★ and pushes NOTHING");

	// And the round trip through the stored pair is a no-op too.
	ZENITH_ASSERT_TRUE(xDoc.SetKeyTangents(xTrack, uMiddleId, xStored), "so is re-stating what was read back");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 1u, "still one step");

	// ★★ AND A MODE-ONLY EDIT IS A REAL EDIT. Same vectors, one end moved from
	// CUSTOM to FLAT: TangentsEqual has to see it, or the document would report the
	// request as already-in-place, push nothing, and leave the user's ease
	// unrecorded and unundoable.
	Flux_KeyTangents xModeOnly = xStored;
	xModeOnly.m_eOutMode = Flux_TangentMode::FLAT;
	ZENITH_ASSERT_TRUE(xModeOnly.m_xInTangent == xStored.m_xInTangent
		&& xModeOnly.m_xOutTangent == xStored.m_xOutTangent,
		"fixture: NOT ONE NUMBER has moved between the two pairs");
	ZENITH_ASSERT_TRUE(xDoc.SetKeyTangents(xTrack, uMiddleId, xModeOnly), "the mode-only edit lands");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 2u, "★ as a second undo step, on identical vectors");
}

//==============================================================================
// B2 helpers.
//==============================================================================
namespace
{
	// The y-RATE the position sampler actually produces between two times, by finite
	// difference.
	//
	// ★ A COPY OF Flux_AnimationClip.Tests.inl's TanMeasurePositionRateX, AND IT HAS
	// TO BE. That one is in an anonymous namespace in another TU, and this is the
	// only shape of measurement that can see a MODE: a FLAT end is a statement about
	// the DERIVATIVE at a key, and comparing sampled VALUES either side of the key
	// cannot see one — they agree there by construction however wrong the slope is.
	float AnimDocMeasureHipRateY(const Zenith_AnimationDocument& xDoc, float fFrom, float fTo)
	{
		const Flux_BoneChannel* pxHip = xDoc.GetClip().GetBoneChannel("Hip");
		if (pxHip == nullptr)
		{
			return 0.0f;
		}
		return (pxHip->SamplePosition(fTo).y - pxHip->SamplePosition(fFrom).y) / (fTo - fFrom);
	}

	// Every AUTO end on the Hip POSITION track agrees with what
	// Flux_BoneChannel::ComputeAutoTangentForKey answers about the track AS IT IS NOW.
	//
	// ★ THE ORACLE IS THE CLIP'S OWN FORMULA, ON PURPOSE. Re-typing the centred slope
	// here would invent a second authority for the number and the two would drift; the
	// claim under test is not "the arithmetic is right" (the clip's own units pin
	// that) but "the stored value was BROUGHT UP TO DATE after the track changed
	// shape". A hard number beside one call of this is what stops it being circular.
	bool AnimDocAutoEndsMatchAFreshCompute(const Zenith_AnimationDocument& xDoc)
	{
		const Flux_BoneChannel* pxHip = xDoc.GetClip().GetBoneChannel("Hip");
		if (pxHip == nullptr)
		{
			return false;
		}
		const Zenith_Vector<Flux_KeyTangents>& xTangents = pxHip->GetPositionTangents();
		for (u_int u = 0; u < xTangents.GetSize(); ++u)
		{
			const Flux_KeyTangents& xStored = xTangents.Get(u);
			const bool bInAuto  = xStored.m_eInMode  == Flux_TangentMode::AUTO;
			const bool bOutAuto = xStored.m_eOutMode == Flux_TangentMode::AUTO;
			if (!bInAuto && !bOutAuto)
			{
				continue;
			}
			Flux_KeyTangents xFresh;
			if (!pxHip->ComputeAutoTangentForKey(FLUX_ANIM_TRACK_POSITION, u, xFresh))
			{
				return false;
			}
			if (bInAuto && glm::length(xStored.m_xInTangent - xFresh.m_xInTangent) > 1e-5f)
			{
				return false;
			}
			if (bOutAuto && glm::length(xStored.m_xOutTangent - xFresh.m_xOutTangent) > 1e-5f)
			{
				return false;
			}
		}
		return true;
	}
}

//==============================================================================
// (15c) B2 — A MODE-ONLY EDIT IS ONE UNDO STEP, DIRTIES, AND REACHES THE POSE.
//
// ★ NOT ONE NUMBER MOVES, AND THE CURVE CHANGES SHAPE. That is the whole of what
// putting the mode on the wire bought: before it, the only way to change a
// segment's shape was to change a vector, and "flat" was therefore unrepresentable
// because its vector is the same zero "linear" already meant. The measurement is a
// finite-difference RATE rather than a sampled value, because the two agree at the
// key by construction.
//==============================================================================
ZENITH_TEST(AnimDocument, AModeOnlyEditIsOneUndoStepAndReachesTheSampledDerivative)
{
	AnimDocFixture xFixture("zenith_animdoc_modeedit");
	// y = 0 / 1 / 2 at t = 0 / 1 / 2, so every honest slope on it is 1 unit/second
	// and a FLAT end has an unmistakable zero to be told apart from.
	AnimDocWriteProbeFile(xFixture.m_strPath, "DocProbe", 1.0f, false);

	Zenith_AnimationDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.Open(xFixture.m_strPath) == ZENITH_ANIMDOC_OPEN_OK, "the probe opens");

	const Zenith_AnimTrackId xTrack = AnimDocHipPositionTrack();
	const u_int uMiddleId = xDoc.GetKeyIdAtIndex(xTrack, 1);

	// The same step the clip's own FLAT units use: small enough that the O(h)
	// truncation is ~1e-2, large enough that float noise stays orders below it.
	const float fH = 2.0e-3f;

	ZENITH_ASSERT_EQ_FLOAT(AnimDocMeasureHipRateY(xDoc, 1.0f - fH, 1.0f), 1.0f, 1e-2f,
		"the control: two LINEAR ends arrive at the segment slope");
	ZENITH_ASSERT_EQ_FLOAT(AnimDocMeasureHipRateY(xDoc, 1.0f, 1.0f + fH), 1.0f, 1e-2f, "and leave at it");

	Flux_KeyTangents xBefore;
	ZENITH_ASSERT_TRUE(xDoc.GetKeyTangents(xTrack, uMiddleId, xBefore), "the key's pair reads back");
	ZENITH_ASSERT_TRUE(xBefore.m_xInTangent == Zenith_Maths::Vector3(0.0f)
		&& xBefore.m_xOutTangent == Zenith_Maths::Vector3(0.0f),
		"fixture: both vectors are exactly zero to begin with");

	ZENITH_ASSERT_TRUE(xDoc.SetKeyTangentMode(xTrack, uMiddleId, ZENITH_ANIM_TANGENT_END_BOTH,
		Flux_TangentMode::FLAT), "the mode verb lands");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 1u, "as exactly ONE undo step");
	ZENITH_ASSERT_TRUE(xDoc.IsDirty(), "and it dirties the document");

	Flux_KeyTangents xAfter;
	ZENITH_ASSERT_TRUE(xDoc.GetKeyTangents(xTrack, uMiddleId, xAfter), "and reads back");
	ZENITH_ASSERT_TRUE(xAfter.m_eInMode == Flux_TangentMode::FLAT
		&& xAfter.m_eOutMode == Flux_TangentMode::FLAT, "with both ends FLAT");
	ZENITH_ASSERT_TRUE(xAfter.m_xInTangent == xBefore.m_xInTangent
		&& xAfter.m_xOutTangent == xBefore.m_xOutTangent,
		"★ and NOT ONE NUMBER MOVED — a vector comparison, a byte comparison of the six floats and a "
		"content hash of the numbers alone would all call this a no-op");

	ZENITH_ASSERT_EQ_FLOAT(AnimDocMeasureHipRateY(xDoc, 1.0f - fH, 1.0f), 0.0f, 1e-2f,
		"★ the curve now ARRIVES at the key with a zero derivative — an ease-in authored by a MODE");
	ZENITH_ASSERT_EQ_FLOAT(AnimDocMeasureHipRateY(xDoc, 1.0f, 1.0f + fH), 0.0f, 1e-2f,
		"and LEAVES it with one");
	// A tangent bends a segment and never moves a key.
	ZENITH_ASSERT_EQ_FLOAT(AnimDocSampleHipY(&xDoc.GetClip(), 1.0f), 1.0f, 1e-5f,
		"the key itself is exactly where it was authored");

	// The assignment rule holds for the mode verb too.
	ZENITH_ASSERT_TRUE(xDoc.SetKeyTangentMode(xTrack, uMiddleId, ZENITH_ANIM_TANGENT_END_BOTH,
		Flux_TangentMode::FLAT), "re-stating the mode is SATISFIED");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 1u, "and pushes nothing");

	// ★ ONE END ONLY, which is what makes a BROKEN key expressible as a mode.
	ZENITH_ASSERT_TRUE(xDoc.SetKeyTangentMode(xTrack, uMiddleId, ZENITH_ANIM_TANGENT_END_IN,
		Flux_TangentMode::LINEAR), "the IN end alone goes back to LINEAR");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 2u, "as one more step");
	ZENITH_ASSERT_TRUE(xDoc.GetKeyTangents(xTrack, uMiddleId, xAfter), "and reads back");
	ZENITH_ASSERT_TRUE(xAfter.m_eInMode == Flux_TangentMode::LINEAR
		&& xAfter.m_eOutMode == Flux_TangentMode::FLAT,
		"★ with the OUT end left exactly as it was — the two ends are addressed separately");
	ZENITH_ASSERT_EQ_FLOAT(AnimDocMeasureHipRateY(xDoc, 1.0f - fH, 1.0f), 1.0f, 1e-2f,
		"and the segment ARRIVING at the key is linear again while the one leaving it still eases");

	xDoc.Undo();
	xDoc.Undo();
	ZENITH_ASSERT_TRUE(xDoc.GetKeyTangents(xTrack, uMiddleId, xAfter), "the key still resolves after both undos");
	ZENITH_ASSERT_TRUE(xAfter.m_eInMode == Flux_TangentMode::LINEAR
		&& xAfter.m_eOutMode == Flux_TangentMode::LINEAR,
		"★ and the MODE is restored, not merely the numbers — which never moved and so could not have "
		"told anyone anything");
	ZENITH_ASSERT_EQ_FLOAT(AnimDocMeasureHipRateY(xDoc, 1.0f, 1.0f + fH), 1.0f, 1e-2f,
		"with the pose back on the linear branch");
}

//==============================================================================
// (15d) B2 — FLAT AND AUTO SURVIVE A SAVE AND A REOPEN.
//
// ★ THIS IS THE ONE THE SCHEMA BUMP EXISTS FOR. Every other assertion about FLAT
// and AUTO in this file is about an in-MEMORY clip, and an in-memory mode was
// exactly what B1 already had: the failure it could not rule out was a mode that
// changed the pose in the editor and was gone the next time the file was opened,
// with no data visibly wrong.
//==============================================================================
ZENITH_TEST(AnimDocument, FlatAndAutoSurviveASaveAndAReopen)
{
	AnimDocFixture xFixture("zenith_animdoc_moderoundtrip");
	AnimDocWriteProbeFile(xFixture.m_strPath, "DocProbe", 1.0f, false);

	const Zenith_AnimTrackId xTrack = AnimDocHipPositionTrack();

	Zenith_AnimationDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.Open(xFixture.m_strPath) == ZENITH_ANIMDOC_OPEN_OK, "the probe opens");

	const u_int uFirstId  = xDoc.GetKeyIdAtIndex(xTrack, 0);
	const u_int uMiddleId = xDoc.GetKeyIdAtIndex(xTrack, 1);

	ZENITH_ASSERT_TRUE(xDoc.SetKeyTangentMode(xTrack, uFirstId, ZENITH_ANIM_TANGENT_END_BOTH,
		Flux_TangentMode::FLAT), "key 0 is made FLAT");
	ZENITH_ASSERT_TRUE(xDoc.SetKeyTangentsAuto(xTrack, uMiddleId), "and key 1 is made AUTO");

	Flux_KeyTangents xAutoBefore;
	ZENITH_ASSERT_TRUE(xDoc.GetKeyTangents(xTrack, uMiddleId, xAutoBefore), "the auto pair reads back");
	ZENITH_ASSERT_TRUE(xAutoBefore.m_eInMode == Flux_TangentMode::AUTO, "as AUTO before the save");
	ZENITH_ASSERT_EQ_FLOAT(xAutoBefore.m_xInTangent.y, 1.0f, 1e-5f, "carrying the line's slope");

	ZENITH_ASSERT_TRUE(xDoc.Save() == ZENITH_ANIMDOC_SAVE_OK, "the save succeeds");
	xDoc.CloseDiscardingChanges();
	ZENITH_ASSERT_TRUE(xDoc.Open(xFixture.m_strPath) == ZENITH_ANIMDOC_OPEN_OK, "and the file reopens");

	// The ids are freshly allocated by Open — they are a SESSION identity and are
	// never serialized (D24) — so the keys are addressed by index again here.
	Flux_KeyTangents xFlatAfter;
	ZENITH_ASSERT_TRUE(xDoc.GetKeyTangents(xTrack, xDoc.GetKeyIdAtIndex(xTrack, 0), xFlatAfter), "key 0 reads back");
	ZENITH_ASSERT_TRUE(xFlatAfter.m_eInMode == Flux_TangentMode::FLAT
		&& xFlatAfter.m_eOutMode == Flux_TangentMode::FLAT,
		"★ FLAT came off the DISK — over two zero vectors, which is the pair that used to read back "
		"LINEAR and silently discard the ease");
	ZENITH_ASSERT_TRUE(xFlatAfter.m_xInTangent == Zenith_Maths::Vector3(0.0f),
		"with the zero vectors it was saved with");

	Flux_KeyTangents xAutoAfter;
	ZENITH_ASSERT_TRUE(xDoc.GetKeyTangents(xTrack, xDoc.GetKeyIdAtIndex(xTrack, 1), xAutoAfter), "key 1 reads back");
	ZENITH_ASSERT_TRUE(xAutoAfter.m_eInMode == Flux_TangentMode::AUTO
		&& xAutoAfter.m_eOutMode == Flux_TangentMode::AUTO,
		"★ and AUTO came off the disk as AUTO, not as the CUSTOM a derivation would have called it — "
		"which is what lets a NEW session go on maintaining a key an OLD one marked");
	ZENITH_ASSERT_EQ_FLOAT(xAutoAfter.m_xInTangent.y, 1.0f, 1e-5f, "with its computed vector intact");

	// The untouched key is still LINEAR, so the round trip is not simply writing one
	// mode everywhere.
	Flux_KeyTangents xUntouched;
	ZENITH_ASSERT_TRUE(xDoc.GetKeyTangents(xTrack, xDoc.GetKeyIdAtIndex(xTrack, 2), xUntouched), "key 2 reads back");
	ZENITH_ASSERT_TRUE(xUntouched.m_eInMode == Flux_TangentMode::LINEAR
		&& xUntouched.m_eOutMode == Flux_TangentMode::LINEAR, "still LINEAR, as it was authored");
}

//==============================================================================
// (15e) B2 — EVERY KEY MUTATION RECOMPUTES THE TRACK'S AUTO ENDS, INSIDE THE SAME
//        UNDO ENTRY.
//
// ★ AN AUTO END THAT IS NEVER RECOMPUTED IS JUST A CUSTOM ONE WITH A MISLEADING
// NAME. The mode's entire reason to exist beside CUSTOM is that the document can
// tell "this slope was computed" from "somebody dragged this", and act on the
// difference when the track changes shape.
//
// ★ AND THE UNDO ENTRY IS THE HALF THAT IS EASY TO GET WRONG. Undoing a retime
// that re-shaped four neighbours has to put all five keys back; a refresh pushed
// as its own command would leave the user pressing Ctrl+Z twice, the first press
// landing on a pose nobody ever saw.
//==============================================================================
ZENITH_TEST(AnimDocument, EveryKeyMutationRecomputesTheTracksAutoTangentsInOneUndoStep)
{
	AnimDocFixture xFixture("zenith_animdoc_automaintenance");
	AnimDocWriteProbeFile(xFixture.m_strPath, "DocProbe", 1.0f, false);

	Zenith_AnimationDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.Open(xFixture.m_strPath) == ZENITH_ANIMDOC_OPEN_OK, "the probe opens");

	const Zenith_AnimTrackId xTrack = AnimDocHipPositionTrack();
	const Zenith_AnimTrackId xScaleTrack = Zenith_AnimTrackId::Bone("Hip", FLUX_ANIM_TRACK_SCALE);

	// A SECOND track on the same bone, also Auto, so "keys on other tracks are
	// untouched" is a measurement rather than an omission.
	ZENITH_ASSERT_NE(xDoc.InsertKey(xScaleTrack, 0.0f, Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f)),
		uINVALID_ANIM_KEY_ID, "a scale key lands");
	ZENITH_ASSERT_NE(xDoc.InsertKey(xScaleTrack, 2.0f, Zenith_Maths::Vector3(1.0f, 3.0f, 1.0f)),
		uINVALID_ANIM_KEY_ID, "and a second one");
	ZENITH_ASSERT_TRUE(xDoc.SetTrackTangentsAuto(xScaleTrack), "the scale track goes Auto");
	ZENITH_ASSERT_TRUE(xDoc.SetTrackTangentsAuto(xTrack), "and so does the position track");

	Flux_KeyTangents xScaleBefore;
	ZENITH_ASSERT_TRUE(xDoc.GetKeyTangents(xScaleTrack, xDoc.GetKeyIdAtIndex(xScaleTrack, 0), xScaleBefore),
		"the scale track's first pair reads back");

	const u_int uDepthAfterSetup = xDoc.GetUndoStackSize();
	const u_int uMiddleId = xDoc.GetKeyIdAtIndex(xTrack, 1);
	const u_int uLastId   = xDoc.GetKeyIdAtIndex(xTrack, 2);

	Flux_KeyTangents xMiddle;
	ZENITH_ASSERT_TRUE(xDoc.GetKeyTangents(xTrack, uMiddleId, xMiddle), "the middle pair reads back");
	ZENITH_ASSERT_EQ_FLOAT(xMiddle.m_xInTangent.y, 1.0f, 1e-5f, "at the line's slope, before anything moves");

	// ---- (a) A VALUE EDIT ------------------------------------------------------
	// y at t=2 goes 2 -> 10, so the centred slope through key 1 becomes
	// (10 - 0) / (2 - 0) = 5. A hard number, so this test is not merely comparing the
	// document against the same formula it used.
	ZENITH_ASSERT_TRUE(xDoc.SetKeyValue(xTrack, uLastId, Zenith_Maths::Vector3(0.0f, 10.0f, 0.0f)),
		"a value edit lands");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), uDepthAfterSetup + 1u,
		"★ as ONE undo step — the edit and every tangent it invalidated, together");
	ZENITH_ASSERT_TRUE(xDoc.GetKeyTangents(xTrack, uMiddleId, xMiddle), "the middle pair reads back");
	ZENITH_ASSERT_EQ_FLOAT(xMiddle.m_xInTangent.y, 5.0f, 1e-5f,
		"★ and its AUTO slope was RECOMPUTED from the new neighbour value");
	ZENITH_ASSERT_EQ_FLOAT(xMiddle.m_xOutTangent.y, 5.0f, 1e-5f, "on both ends, as a SMOOTH key");
	ZENITH_ASSERT_TRUE(xMiddle.m_eInMode == Flux_TangentMode::AUTO,
		"and it is STILL AUTO — a refresh must not quietly promote a key to CUSTOM");
	ZENITH_ASSERT_TRUE(AnimDocAutoEndsMatchAFreshCompute(xDoc), "every AUTO end on the track is up to date");

	// The other track did not move.
	Flux_KeyTangents xScaleNow;
	ZENITH_ASSERT_TRUE(xDoc.GetKeyTangents(xScaleTrack, xDoc.GetKeyIdAtIndex(xScaleTrack, 0), xScaleNow),
		"the scale pair still reads back");
	ZENITH_ASSERT_TRUE(Zenith_AnimationDocument::TangentsEqual(xScaleBefore, xScaleNow),
		"★ and a mutation on the POSITION track left the SCALE track's Auto keys alone");

	// ---- one Ctrl+Z puts the value AND every tangent back ----------------------
	xDoc.Undo();
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), uDepthAfterSetup, "one undo, one step");
	ZENITH_ASSERT_TRUE(xDoc.GetKeyTangents(xTrack, uMiddleId, xMiddle), "the middle pair reads back");
	ZENITH_ASSERT_EQ_FLOAT(xMiddle.m_xInTangent.y, 1.0f, 1e-5f,
		"★ with the tangent restored as well as the value — not left describing a curve the clip no "
		"longer has");

	// ---- (b) A RETIME ----------------------------------------------------------
	ZENITH_ASSERT_TRUE(xDoc.SetKeyTime(xTrack, uMiddleId, 1.5f), "a retime lands");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), uDepthAfterSetup + 1u, "as one undo step");
	ZENITH_ASSERT_TRUE(AnimDocAutoEndsMatchAFreshCompute(xDoc),
		"★ a retime changes a SPAN, and every AUTO end that depended on it was recomputed");
	xDoc.Undo();
	ZENITH_ASSERT_TRUE(AnimDocAutoEndsMatchAFreshCompute(xDoc), "and the undo leaves the track consistent");

	// ---- (c) AN INSERT ---------------------------------------------------------
	const u_int uInsertedId = xDoc.InsertKey(xTrack, 0.5f, Zenith_Maths::Vector3(0.0f, 0.25f, 0.0f));
	ZENITH_ASSERT_NE(uInsertedId, uINVALID_ANIM_KEY_ID, "an insert lands");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), uDepthAfterSetup + 1u, "as one undo step");
	Flux_KeyTangents xInserted;
	ZENITH_ASSERT_TRUE(xDoc.GetKeyTangents(xTrack, uInsertedId, xInserted), "the NEW key's pair reads back");
	ZENITH_ASSERT_TRUE(xInserted.m_eInMode == Flux_TangentMode::LINEAR
		&& xInserted.m_eOutMode == Flux_TangentMode::LINEAR,
		"★ and the new key is LINEAR, not AUTO — the refresh maintains ends that are ALREADY Auto and "
		"does not enrol new ones");
	ZENITH_ASSERT_TRUE(AnimDocAutoEndsMatchAFreshCompute(xDoc),
		"while its AUTO neighbours, whose spans just changed, were recomputed");

	// ---- (d) A REMOVAL ---------------------------------------------------------
	ZENITH_ASSERT_TRUE(xDoc.RemoveKey(xTrack, uInsertedId), "removing it again lands");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), uDepthAfterSetup + 2u, "as one more undo step");
	ZENITH_ASSERT_TRUE(AnimDocAutoEndsMatchAFreshCompute(xDoc),
		"and the neighbours are recomputed again on the way back");
	ZENITH_ASSERT_TRUE(xDoc.GetKeyTangents(xTrack, uMiddleId, xMiddle), "the middle key still resolves");
	ZENITH_ASSERT_EQ_FLOAT(xMiddle.m_xInTangent.y, 1.0f, 1e-5f,
		"at the original slope, because the track is the shape it started as");
}

//==============================================================================
// (15f) B2 — THE REFRESH ADOPTS INTO A COMPOUND THE CALLER ALREADY OPENED, AND
//        DOES NOT OPEN A NESTED ONE.
//
// ★★ THIS IS THE ONE THAT WOULD HAVE BEEN A ROLLBACK, NOT A FAILED ASSERTION.
// BeginCompound REFUSES a nested group and asserts; the refusal returns false, and
// a refresh that then called EndCompound anyway would close the CALLER'S group —
// so a multi-key dope-sheet drag would have its whole gesture terminated halfway
// through by the first key that happened to sit beside an Auto tangent. Seven
// production sites hold a compound open around these verbs.
//==============================================================================
ZENITH_TEST(AnimDocument, TheAutoRefreshAdoptsIntoAnOpenCompoundWithoutNesting)
{
	AnimDocFixture xFixture("zenith_animdoc_autocompound");
	AnimDocWriteProbeFile(xFixture.m_strPath, "DocProbe", 1.0f, false);

	Zenith_AnimationDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.Open(xFixture.m_strPath) == ZENITH_ANIMDOC_OPEN_OK, "the probe opens");

	const Zenith_AnimTrackId xTrack = AnimDocHipPositionTrack();
	ZENITH_ASSERT_TRUE(xDoc.SetTrackTangentsAuto(xTrack), "the track goes Auto");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 1u, "one compound so far");

	const u_int uFirstId  = xDoc.GetKeyIdAtIndex(xTrack, 0);
	const u_int uMiddleId = xDoc.GetKeyIdAtIndex(xTrack, 1);

	{
		Zenith_AssertCaptureScope xCapture;
		ZENITH_ASSERT_TRUE(xDoc.BeginCompound(), "the CALLER's own group opens");
		ZENITH_ASSERT_TRUE(xDoc.SetKeyTime(xTrack, uMiddleId, 1.25f), "a retime inside it lands");
		ZENITH_ASSERT_TRUE(xDoc.SetKeyValue(xTrack, uFirstId, Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f)),
			"and a value edit beside it");
		ZENITH_ASSERT_TRUE(xDoc.IsCompoundOpen(), "★ the caller's group is STILL the open one");
		ZENITH_ASSERT_TRUE(xDoc.EndCompound("Dope Sheet Drag", /*bKeep*/ true), "and the caller closes it");
		ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 0u,
			"★ and NOTHING asserted along the way — the refresh adopted through PushCommand instead of "
			"calling BeginCompound, which would have been refused and would have rolled the gesture back");
	}

	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 2u,
		"★ the caller's whole gesture — two edits and every tangent they invalidated — is ONE more step");
	ZENITH_ASSERT_TRUE(AnimDocAutoEndsMatchAFreshCompute(xDoc),
		"with the AUTO ends recomputed for the track as it now is");

	xDoc.Undo();
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 1u, "one Ctrl+Z takes the whole gesture back");
	ZENITH_ASSERT_TRUE(AnimDocAutoEndsMatchAFreshCompute(xDoc), "leaving the track consistent again");
	Flux_KeyTangents xMiddle;
	ZENITH_ASSERT_TRUE(xDoc.GetKeyTangents(xTrack, uMiddleId, xMiddle), "the middle key still resolves");
	ZENITH_ASSERT_EQ_FLOAT(xMiddle.m_xInTangent.y, 1.0f, 1e-5f, "at the slope the Auto preset first wrote");
}

//==============================================================================
// (15g) B2 — A HANDLE DRAG CLAIMS ONE END AND LEAVES THE OTHER'S MODE ALONE, AND
//        THE END IT CLAIMED STOPS BEING MAINTAINED.
//
// ★ THE SECOND HALF IS THE POINT OF THE FIRST. If a drag promoted BOTH ends to
// CUSTOM, the opposite handle would silently stop tracking the curve — a change
// the user did not make, to a handle they did not touch, visible only later as a
// key that "went stale". And if it promoted NEITHER, the next mutation on the
// track would overwrite the number they just dragged.
//==============================================================================
ZENITH_TEST(AnimDocument, ADragClaimsOneEndAsCustomAndOnlyThatEndStopsBeingMaintained)
{
	AnimDocFixture xFixture("zenith_animdoc_dragclaims");
	AnimDocWriteProbeFile(xFixture.m_strPath, "DocProbe", 1.0f, false);

	Zenith_AnimationDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.Open(xFixture.m_strPath) == ZENITH_ANIMDOC_OPEN_OK, "the probe opens");

	const Zenith_AnimTrackId xTrack = AnimDocHipPositionTrack();
	ZENITH_ASSERT_TRUE(xDoc.SetTrackTangentsAuto(xTrack), "the track goes Auto");

	const u_int uMiddleId = xDoc.GetKeyIdAtIndex(xTrack, 1);
	const u_int uLastId   = xDoc.GetKeyIdAtIndex(xTrack, 2);

	ZENITH_ASSERT_TRUE(xDoc.SetKeyOutTangent(xTrack, uMiddleId, Zenith_Maths::Vector3(0.0f, 7.0f, 0.0f)),
		"a drag on the OUT handle lands");

	Flux_KeyTangents xPair;
	ZENITH_ASSERT_TRUE(xDoc.GetKeyTangents(xTrack, uMiddleId, xPair), "the pair reads back");
	ZENITH_ASSERT_TRUE(xPair.m_eOutMode == Flux_TangentMode::CUSTOM,
		"★ the dragged end is CUSTOM — a hand authorship claim on ONE handle");
	ZENITH_ASSERT_TRUE(xPair.m_eInMode == Flux_TangentMode::AUTO,
		"★ and the OTHER end's MODE is untouched — promoting it too would silently cancel an Auto the "
		"user never touched");
	ZENITH_ASSERT_EQ_FLOAT(xPair.m_xInTangent.y, 1.0f, 1e-5f, "with its computed vector still there");

	// Now change the track's shape. The AUTO end must move; the CUSTOM one must not.
	ZENITH_ASSERT_TRUE(xDoc.SetKeyValue(xTrack, uLastId, Zenith_Maths::Vector3(0.0f, 10.0f, 0.0f)),
		"a value edit on the neighbour lands");
	ZENITH_ASSERT_TRUE(xDoc.GetKeyTangents(xTrack, uMiddleId, xPair), "the pair reads back");
	ZENITH_ASSERT_EQ_FLOAT(xPair.m_xInTangent.y, 5.0f, 1e-5f,
		"★ the AUTO end followed the new centred slope (10 - 0) / (2 - 0)");
	ZENITH_ASSERT_EQ_FLOAT(xPair.m_xOutTangent.y, 7.0f, 1e-6f,
		"★ and the hand-dragged CUSTOM end is EXACTLY where it was left — a maintenance pass that "
		"re-smoothed it would be the edit nobody asked for");
	ZENITH_ASSERT_TRUE(xPair.m_eInMode == Flux_TangentMode::AUTO
		&& xPair.m_eOutMode == Flux_TangentMode::CUSTOM, "with both modes unchanged by the refresh");
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
	ZENITH_ASSERT_FALSE(xDoc.SetTrackTangentsLinear(xRootPos), "all three of them");
	ZENITH_ASSERT_FALSE(xDoc.SetTrackTangentsFlat(xRootPos), "including the Flat one B2 added");
	// ★ AND THE MODE VERB TOO, ON EVERY END AND EVERY MODE. A new verb is exactly
	// where this refusal gets forgotten: it is stated once in GetKeyTangents, which
	// SetKeyTangentMode goes through first precisely so it cannot be.
	ZENITH_ASSERT_FALSE(xDoc.SetKeyTangentMode(xRootPos, uRootKeyId, ZENITH_ANIM_TANGENT_END_BOTH,
		Flux_TangentMode::FLAT), "the mode verb refuses root motion");
	ZENITH_ASSERT_FALSE(xDoc.SetKeyTangentMode(xRootPos, uRootKeyId, ZENITH_ANIM_TANGENT_END_IN,
		Flux_TangentMode::AUTO), "on one end as well as both, and for AUTO as well as FLAT");

	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 1u, "★ and not one of those refusals pushed a command");
}
