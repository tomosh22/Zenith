//------------------------------------------------------------------------------
// Zenith_BoneMaskDocument unit tests (WU-7.1).
// Included at the bottom of Zenith_BoneMaskDocument.cpp.
//
// ★ THE HEADLINE PROPERTIES: one edit is one undo step, a SUBTREE paint is ONE
// compound however many bones it touched, and an additive layer accepts no mask
// at all. The last one is pure and is what the sub-panel gates its assignment
// control on; the first two are the difference between a usable mask editor and
// one where painting an arm takes eleven Ctrl+Z presses to take back.
//
// ★ AND ONE THAT IS EASY TO MISS: undoing the FIRST weight ever painted onto a
// bone has to REMOVE the entry, not write a zero. The two answer GetBoneWeight
// identically and serialize differently, so a document that zeroed instead would
// leave every experimentally-touched bone in the file forever, each one an
// explicit "this bone is masked out" nobody meant.
//
// Headless: real files in a private temp directory, an in-memory
// Zenith_SkeletonAsset, no device. None of it is requiresGraphics.
//------------------------------------------------------------------------------

#include "UnitTests/Zenith_UnitTests.h"

#include <filesystem>

namespace
{
	struct BoneMaskDocFixture
	{
		std::filesystem::path m_xDirectory;
		std::string m_strPath;
		std::string m_strAltPath;

		explicit BoneMaskDocFixture(const char* szLeafDirectory)
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
			m_strPath = (m_xDirectory / "doc.zanimmask").generic_string();
			m_strAltPath = (m_xDirectory / "doc_copy.zanimmask").generic_string();
		}

		~BoneMaskDocFixture()
		{
			Zenith_AssetRegistry::ForceUnload(m_strPath);
			Zenith_AssetRegistry::ForceUnload(m_strAltPath);
			std::error_code xError;
			std::filesystem::remove_all(m_xDirectory, xError);
		}

		BoneMaskDocFixture(const BoneMaskDocFixture&) = delete;
		BoneMaskDocFixture& operator=(const BoneMaskDocFixture&) = delete;
	};

	// Root -> Spine -> { ArmL, ArmR }, and Root -> Leg. The smallest rig with a
	// real subtree: painting Spine must reach the two arms and must NOT reach the
	// leg or the root, which a "set everything with a higher index" shortcut
	// would get wrong.
	void BoneMaskDocBuildRig(Zenith_SkeletonAsset& xSkeleton)
	{
		const Zenith_Maths::Quat xIdentity = glm::identity<Zenith_Maths::Quat>();
		const Zenith_Maths::Vector3 xUnitScale(1.0f);
		xSkeleton.AddBone("Root", -1, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), xIdentity, xUnitScale);
		xSkeleton.AddBone("Spine", 0, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), xIdentity, xUnitScale);
		xSkeleton.AddBone("ArmL", 1, Zenith_Maths::Vector3(-0.5f, 0.0f, 0.0f), xIdentity, xUnitScale);
		xSkeleton.AddBone("ArmR", 1, Zenith_Maths::Vector3(0.5f, 0.0f, 0.0f), xIdentity, xUnitScale);
		xSkeleton.AddBone("Leg", 0, Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f), xIdentity, xUnitScale);
		xSkeleton.ComputeBindPoseMatrices();
	}

	void BoneMaskDocWriteFile(const std::string& strPath, const char* szBone, float fWeight)
	{
		Zenith_BoneMaskAsset xMask;
		xMask.SetBoneWeight(szBone, fWeight);
		xMask.Export(strPath);
	}
}

//==============================================================================
// (1) THE LAYER RULE — pure, and the one the sub-panel gates on.
//==============================================================================
ZENITH_TEST(BoneMaskDoc, AnAdditiveLayerAcceptsNoMask)
{
	// ★ IT IS NOT THAT AN ADDITIVE LAYER'S MASK IS IGNORED "MOSTLY". It never
	// reaches a blend at all: Flux_AnimationController tests LAYER_BLEND_ADDITIVE
	// FIRST and goes to Flux_SkeletonPose::AdditiveBlend, whose signature has no
	// mask parameter. Only the OVERRIDE branch reaches MaskedBlend.
	ZENITH_ASSERT_TRUE(Zenith_BoneMaskDocument::LayerAcceptsMask(LAYER_BLEND_OVERRIDE),
		"an override layer is the one a mask means anything to");
	ZENITH_ASSERT_FALSE(Zenith_BoneMaskDocument::LayerAcceptsMask(LAYER_BLEND_ADDITIVE),
		"an additive layer ignores its mask entirely, so the UI must not offer one");

	// The notice is one string in one place, so the sub-panel, WU-7.2's layer
	// list and this test cannot word it three ways.
	ZENITH_ASSERT_NOT_NULL(Zenith_BoneMaskDocument::AdditiveLayerMaskNotice(), "there is a notice to show");
	ZENITH_ASSERT_STREQ(Zenith_BoneMaskDocument::AdditiveLayerMaskNotice(),
		"masks do not apply to additive layers", "and it says why, not just 'unavailable'");

	// PURE — callable with no document at all, which is what lets WU-7.2's layer
	// list ask it per row without opening anything.
	Zenith_BoneMaskDocument xClosed;
	ZENITH_ASSERT_FALSE(xClosed.IsOpen(), "the rule needs no open document");
	ZENITH_ASSERT_FALSE(Zenith_BoneMaskDocument::LayerAcceptsMask(LAYER_BLEND_ADDITIVE), "and answers the same");
}

//==============================================================================
// (2) Open deep-copies, edits stay in the working copy, and Save publishes.
//==============================================================================
ZENITH_TEST(BoneMaskDoc, EditsStayInTheWorkingCopyUntilSave)
{
	BoneMaskDocFixture xFixture("zenith_bonemaskdoc_worktopy");
	BoneMaskDocWriteFile(xFixture.m_strPath, "Spine", 1.0f);

	Zenith_BoneMaskDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.Open(xFixture.m_strPath) == ZENITH_BONEMASKDOC_OPEN_OK, "the mask opens");
	ZENITH_ASSERT_TRUE(xDoc.IsOpen(), "and the document reports itself open");
	ZENITH_ASSERT_FALSE(xDoc.IsDirty(), "a freshly opened document is clean");
	ZENITH_ASSERT_EQ_FLOAT(xDoc.GetBoneWeight("Spine"), 1.0f, 1e-5f, "with the file's content");

	ZENITH_ASSERT_TRUE(xDoc.SetBoneWeight("ArmL", 0.5f), "paint a second bone");
	ZENITH_ASSERT_TRUE(xDoc.IsDirty(), "which dirties the document");

	// ★ THE LIVE ASSET HAS NOT MOVED. Editing it in place would push half-finished
	// weights into whatever BuildFromControllerDef resolves next — a character's
	// arms fading in and out while a slider is dragged — and would leave nothing
	// to discard back to.
	const Zenith_BoneMaskAsset* pxLive = xDoc.GetAsset();
	ZENITH_ASSERT_NOT_NULL(pxLive, "the live asset is still cached");
	if (pxLive != nullptr)
	{
		ZENITH_ASSERT_FALSE(pxLive->HasBone("ArmL"), "and knows nothing about the unsaved edit");
	}

	ZENITH_ASSERT_TRUE(xDoc.Save() == ZENITH_BONEMASKDOC_SAVE_OK, "the save succeeds");
	ZENITH_ASSERT_FALSE(xDoc.IsDirty(), "and clears the dirty flag");

	// Now it HAS moved — in place, without a ForceUnload, so anything holding a
	// view of this asset sees the new content rather than freed memory.
	pxLive = xDoc.GetAsset();
	ZENITH_ASSERT_NOT_NULL(pxLive, "the same asset object is still there");
	if (pxLive != nullptr)
	{
		ZENITH_ASSERT_EQ_FLOAT(pxLive->GetBoneWeight("ArmL"), 0.5f, 1e-5f, "carrying what was saved");
	}

	// And the FILE carries it, which the in-place refresh alone would not prove.
	Zenith_BoneMaskAsset xFromDisk;
	Zenith_DataStream xStream;
	xStream.ReadFromFile(xFixture.m_strPath.c_str());
	ZENITH_ASSERT_TRUE(xFromDisk.ParseStream(xStream).IsOk(), "the written file parses");
	ZENITH_ASSERT_EQ_FLOAT(xFromDisk.GetBoneWeight("ArmL"), 0.5f, 1e-5f, "and holds the edit");

	// A dirty document refuses to close, and the forced close is the panel's
	// answer to its own prompt.
	ZENITH_ASSERT_TRUE(xDoc.SetBoneWeight("ArmR", 0.25f), "one more edit");
	ZENITH_ASSERT_TRUE(xDoc.Close() == ZENITH_BONEMASKDOC_CLOSE_REFUSED_DIRTY, "close refuses while dirty");
	ZENITH_ASSERT_TRUE(xDoc.IsOpen(), "and leaves everything open");
	xDoc.CloseDiscardingChanges();
	ZENITH_ASSERT_FALSE(xDoc.IsOpen(), "the forced close drops it");
}

//==============================================================================
// (3) ONE EDIT, ONE UNDO STEP — and a no-op is not an edit.
//==============================================================================
ZENITH_TEST(BoneMaskDoc, EachWeightEditIsExactlyOneUndoStepAndANoOpIsNone)
{
	BoneMaskDocFixture xFixture("zenith_bonemaskdoc_undo");

	Zenith_BoneMaskDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.OpenFresh(xFixture.m_strPath) == ZENITH_BONEMASKDOC_OPEN_OK, "a fresh mask opens");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 0u, "with an empty stack");

	ZENITH_ASSERT_TRUE(xDoc.SetBoneWeight("Spine", 1.0f), "first edit");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 1u, "one step");
	ZENITH_ASSERT_TRUE(xDoc.SetBoneWeight("Spine", 0.5f), "second edit, same bone");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 2u, "two steps");
	ZENITH_ASSERT_TRUE(xDoc.SetBoneWeight("ArmL", 0.25f), "third edit, another bone");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 3u, "three steps");

	// ★ ASSIGNMENT SEMANTICS: asking for the value already in place is the
	// caller's intent SATISFIED. TRUE, and no step — which is the invariant to
	// assert on, because the bool deliberately does not report "changed".
	ZENITH_ASSERT_TRUE(xDoc.SetBoneWeight("ArmL", 0.25f), "re-stating a weight is satisfied, not refused");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 3u, "and pushes nothing");

	// Each undo takes back exactly its own edit.
	xDoc.Undo();
	ZENITH_ASSERT_FALSE(xDoc.HasBone("ArmL"), "the third edit's entry is gone");
	ZENITH_ASSERT_EQ_FLOAT(xDoc.GetBoneWeight("Spine"), 0.5f, 1e-5f, "and Spine is untouched by it");
	xDoc.Undo();
	ZENITH_ASSERT_EQ_FLOAT(xDoc.GetBoneWeight("Spine"), 1.0f, 1e-5f, "the second edit is taken back");
	xDoc.Undo();
	ZENITH_ASSERT_EQ(xDoc.GetEntryCount(), 0u, "and the first, leaving the mask as it was opened");

	// Redo walks back up through the same three.
	xDoc.Redo();
	ZENITH_ASSERT_EQ_FLOAT(xDoc.GetBoneWeight("Spine"), 1.0f, 1e-5f, "redo re-applies");
	xDoc.Redo();
	xDoc.Redo();
	ZENITH_ASSERT_EQ_FLOAT(xDoc.GetBoneWeight("ArmL"), 0.25f, 1e-5f, "all three");

	xDoc.CloseDiscardingChanges();
}

//==============================================================================
// (4) A SUBTREE PAINT IS ONE COMPOUND.
//
// ★ THE COUNT IS THE CONTRACT. Painting an arm chain to 1.0 must take ONE
// Ctrl+Z, not one per bone, and the intermediate states — half an arm masked —
// are states the user never saw and must not be able to stop on.
//==============================================================================
ZENITH_TEST(BoneMaskDoc, ASubtreePaintIsOneCompoundAndOneUndoTakesItAllBack)
{
	BoneMaskDocFixture xFixture("zenith_bonemaskdoc_subtree");

	Zenith_SkeletonAsset xSkeleton;
	BoneMaskDocBuildRig(xSkeleton);

	Zenith_BoneMaskDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.OpenFresh(xFixture.m_strPath) == ZENITH_BONEMASKDOC_OPEN_OK, "a fresh mask opens");

	ZENITH_ASSERT_TRUE(xDoc.SetSubtreeWeight(xSkeleton, "Spine", 1.0f), "paint the Spine subtree");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 1u, "THREE bones, ONE undo step");
	ZENITH_ASSERT_FALSE(xDoc.IsCompoundOpen(), "and the group is closed behind it");

	ZENITH_ASSERT_EQ_FLOAT(xDoc.GetBoneWeight("Spine"), 1.0f, 1e-5f, "the root of the subtree");
	ZENITH_ASSERT_EQ_FLOAT(xDoc.GetBoneWeight("ArmL"), 1.0f, 1e-5f, "and each descendant");
	ZENITH_ASSERT_EQ_FLOAT(xDoc.GetBoneWeight("ArmR"), 1.0f, 1e-5f, "and the other one");
	// ★ AND NOTHING ELSE. A "set every higher index" shortcut would have caught
	// Leg, which is Root's child and not Spine's, and the mask would silently
	// cover the legs of every character it was assigned to.
	ZENITH_ASSERT_FALSE(xDoc.HasBone("Leg"), "Leg is Root's child, not Spine's — untouched");
	ZENITH_ASSERT_FALSE(xDoc.HasBone("Root"), "and an ancestor is not part of a subtree");

	xDoc.Undo();
	ZENITH_ASSERT_EQ(xDoc.GetEntryCount(), 0u, "one undo takes the whole paint back");
	xDoc.Redo();
	ZENITH_ASSERT_EQ(xDoc.GetEntryCount(), 3u, "and one redo puts it all back");
	ZENITH_ASSERT_EQ_FLOAT(xDoc.GetBoneWeight("ArmR"), 1.0f, 1e-5f, "with its weights");

	// Clearing a subtree is the same gesture with the other value, and is still
	// one step. Note the entries SURVIVE at zero — an explicit zero is a decision
	// somebody made and is what makes "this layer overrides nothing here"
	// expressible (D47).
	ZENITH_ASSERT_TRUE(xDoc.SetSubtreeWeight(xSkeleton, "Spine", 0.0f), "clear the same subtree");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 2u, "one more step");
	ZENITH_ASSERT_EQ(xDoc.GetEntryCount(), 3u, "the entries remain, at zero");
	ZENITH_ASSERT_EQ_FLOAT(xDoc.GetBoneWeight("ArmL"), 0.0f, 1e-5f, "zeroed rather than removed");

	// A leaf bone has no descendants and is still a legal, single-bone paint.
	ZENITH_ASSERT_TRUE(xDoc.SetSubtreeWeight(xSkeleton, "Leg", 1.0f), "a leaf is a subtree of one");
	ZENITH_ASSERT_EQ_FLOAT(xDoc.GetBoneWeight("Leg"), 1.0f, 1e-5f, "and is painted");

	// A bone the rig does not carry is a refusal that changes nothing.
	const u_int uDepthBefore = xDoc.GetUndoStackSize();
	ZENITH_ASSERT_FALSE(xDoc.SetSubtreeWeight(xSkeleton, "Tail", 1.0f), "a bone this rig lacks is refused");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), uDepthBefore, "and pushes nothing");

	xDoc.CloseDiscardingChanges();
}

//==============================================================================
// (5) Undoing the FIRST weight on a bone REMOVES the entry.
//
// ★ AN ENTRY AT 0.0 AND NO ENTRY ARE THE SAME NUMBER AND DIFFERENT FILES. If
// the undo wrote a zero instead of removing the row, every bone a user touched
// experimentally would stay in the .zanimmask forever as an explicit "masked
// out" — and the two states are indistinguishable through GetBoneWeight, which
// is exactly why this needs its own assertion on the ENTRY COUNT.
//==============================================================================
ZENITH_TEST(BoneMaskDoc, UndoingTheFirstWeightOnABoneRemovesItsEntryRatherThanZeroingIt)
{
	BoneMaskDocFixture xFixture("zenith_bonemaskdoc_firstweight");

	Zenith_BoneMaskDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.OpenFresh(xFixture.m_strPath) == ZENITH_BONEMASKDOC_OPEN_OK, "a fresh mask opens");
	ZENITH_ASSERT_EQ(xDoc.GetEntryCount(), 0u, "with no entries");

	ZENITH_ASSERT_TRUE(xDoc.SetBoneWeight("Spine", 0.75f), "the first weight ever painted onto Spine");
	ZENITH_ASSERT_EQ(xDoc.GetEntryCount(), 1u, "creates the entry");

	xDoc.Undo();
	ZENITH_ASSERT_EQ(xDoc.GetEntryCount(), 0u, "and the undo REMOVES it, rather than writing a zero");
	ZENITH_ASSERT_FALSE(xDoc.HasBone("Spine"), "so the file it would save has no row for that bone");
	ZENITH_ASSERT_EQ_FLOAT(xDoc.GetBoneWeight("Spine"), 0.0f, 1e-5f,
		"while the weight READS the same either way — which is why the count is the assertion");

	// A DELIBERATE zero is a different thing and survives.
	xDoc.Redo();
	ZENITH_ASSERT_TRUE(xDoc.SetBoneWeight("Spine", 0.0f), "explicitly zero it");
	ZENITH_ASSERT_EQ(xDoc.GetEntryCount(), 1u, "the row stays — an authored zero is a decision");

	// RemoveBone is the verb for actually dropping it, and it is a REMOVAL: a
	// miss is a genuine refusal, not a satisfied assignment.
	ZENITH_ASSERT_TRUE(xDoc.RemoveBone("Spine"), "RemoveBone drops the row");
	ZENITH_ASSERT_EQ(xDoc.GetEntryCount(), 0u, "and it is gone");
	ZENITH_ASSERT_FALSE(xDoc.RemoveBone("Spine"), "removing what is not there is refused");
	xDoc.Undo();
	ZENITH_ASSERT_EQ(xDoc.GetEntryCount(), 1u, "undoing a removal puts the row back");
	ZENITH_ASSERT_EQ_FLOAT(xDoc.GetBoneWeight("Spine"), 0.0f, 1e-5f, "at the weight it had");

	xDoc.CloseDiscardingChanges();
}

//==============================================================================
// (6) D47's flag is an ordinary, undoable, ASSIGNMENT-semantic edit.
//==============================================================================
ZENITH_TEST(BoneMaskDoc, TheHasAvatarMaskFlagIsUndoableAndSatisfiedByItsOwnValue)
{
	BoneMaskDocFixture xFixture("zenith_bonemaskdoc_flag");

	Zenith_BoneMaskDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.OpenFresh(xFixture.m_strPath) == ZENITH_BONEMASKDOC_OPEN_OK, "a fresh mask opens");
	// ★ A FRESH MASK IS A MASK. The alternative — an empty mask that is "not a
	// mask" — would make the first weight painted into it do nothing to the layer
	// until somebody found the checkbox.
	ZENITH_ASSERT_TRUE(xDoc.HasAvatarMask(), "and it is a mask from the first frame");

	ZENITH_ASSERT_TRUE(xDoc.SetHasAvatarMask(true), "setting it to what it already is is satisfied");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 0u, "and pushes nothing");

	ZENITH_ASSERT_TRUE(xDoc.SetHasAvatarMask(false), "turning it off is an edit");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 1u, "of exactly one step");
	ZENITH_ASSERT_FALSE(xDoc.HasAvatarMask(), "and it took");

	xDoc.Undo();
	ZENITH_ASSERT_TRUE(xDoc.HasAvatarMask(), "undo restores it");

	// It survives a save and reloads as data, not as a derivation from the
	// weights — which is D47's whole point, checked here at the document level.
	ZENITH_ASSERT_TRUE(xDoc.SetHasAvatarMask(false), "off again");
	ZENITH_ASSERT_TRUE(xDoc.Save() == ZENITH_BONEMASKDOC_SAVE_OK, "save it");
	xDoc.CloseDiscardingChanges();

	Zenith_AssetRegistry::ForceUnload(xFixture.m_strPath);
	Zenith_BoneMaskDocument xReopened;
	ZENITH_ASSERT_TRUE(xReopened.Open(xFixture.m_strPath) == ZENITH_BONEMASKDOC_OPEN_OK, "it reopens");
	ZENITH_ASSERT_FALSE(xReopened.HasAvatarMask(), "with the flag as it was saved");
	xReopened.CloseDiscardingChanges();
}
