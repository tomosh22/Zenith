//------------------------------------------------------------------------------
// Zenith_EditorPrefs unit tests (WU-2.4).
// Included at the bottom of Zenith_EditorPrefs.cpp.
//
// ★ THESE TEST Serialize/Parse DIRECTLY, NEVER Load/Save. Load() returns false
// and Save() is a no-op in an automated or headless run (PrefsAreEnabled), so a
// test routed through the file I/O would assert on defaults and pass for the
// wrong reason — it would be testing that the guard works, not that the record
// round-trips. Serialize and Parse are pure text transforms and are the whole of
// the persistence contract.
//
// All CPU-only, no device, no files. None is requiresGraphics.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"
#include "UnitTests/Zenith_UnitTests.h"

ZENITH_TEST(EditorPrefs, AnimRigChoiceRoundTripsThroughSerializeParse)
{
	Zenith_EditorPrefs xWritten;
	xWritten.SetAnimRigChoice("game:Anims/Run.zanim", "game:Rigs/Hero.zskel", "game:Meshes/Hero.zmodel");
	xWritten.SetAnimRigChoice("engine:Meshes/Bushes/Bush_Broad_Sway.zanim",
		"engine:Meshes/Bushes/Bush_Broad.zskel", "engine:Meshes/Bushes/Bush_Broad.zasset");
	ZENITH_ASSERT_EQ(xWritten.GetAnimRigChoiceCount(), 2u, "both choices should be stored");

	// A second write under the same key REPLACES rather than duplicating — the map
	// is the per-clip answer, not a history of answers.
	xWritten.SetAnimRigChoice("game:Anims/Run.zanim", "game:Rigs/Hero2.zskel", "game:Meshes/Hero2.zmodel");
	ZENITH_ASSERT_EQ(xWritten.GetAnimRigChoiceCount(), 2u, "re-choosing a rig must not add an entry");

	// The map must survive alongside the scalar fields, so move one of those too:
	// a record written between the recent-scene lines and the field table is
	// exactly where a parser that mis-handles an unknown repeated key breaks.
	xWritten.AddRecentScene("game:Scenes/Town.zscen");
	xWritten.m_fCameraMoveSpeed = 12.5f;

	Zenith_EditorPrefs xRead;
	xRead.Parse(xWritten.Serialize());

	ZENITH_ASSERT_EQ(xRead.GetAnimRigChoiceCount(), 2u, "both choices should survive the round trip");

	Zenith_EditorPrefs_AnimRigChoice xChoice;
	ZENITH_ASSERT_TRUE(xRead.TryGetAnimRigChoice("game:Anims/Run.zanim", xChoice), "Run's choice should be found");
	ZENITH_ASSERT_TRUE(xChoice.m_strSkeletonPath == "game:Rigs/Hero2.zskel", "the LAST skeleton chosen is the one stored");
	ZENITH_ASSERT_TRUE(xChoice.m_strPreviewModelPath == "game:Meshes/Hero2.zmodel", "model path should round-trip");

	// ★ A .zasset MESH IS A LEGAL PREVIEW PATH. The generated tree/bush sway clips
	// name one because those sets bake no .zmodel at all; the prefs layer stores
	// the string verbatim and does not police the extension.
	ZENITH_ASSERT_TRUE(xRead.TryGetAnimRigChoice("engine:Meshes/Bushes/Bush_Broad_Sway.zanim", xChoice),
		"the bush sway choice should be found");
	ZENITH_ASSERT_TRUE(xChoice.m_strPreviewModelPath == "engine:Meshes/Bushes/Bush_Broad.zasset",
		"a .zasset preview path round-trips verbatim");

	// The neighbours are unharmed.
	ZENITH_ASSERT_EQ(xRead.m_axRecentScenes.GetSize(), 1u, "the recent-scene list should still round-trip");
	ZENITH_ASSERT_EQ_FLOAT(xRead.m_fCameraMoveSpeed, 12.5f, 1e-4f, "scalar fields should still round-trip");

	// Parse resets first, so a second Parse of an EMPTY document must not leave
	// the previous run's choices behind.
	xRead.Parse("");
	ZENITH_ASSERT_EQ(xRead.GetAnimRigChoiceCount(), 0u, "Parse resets the map like every other field");
}

ZENITH_TEST(EditorPrefs, AnimRigChoiceMalformedRecordsAreDropped)
{
	// Every one of these is a line a hand edit could produce. None may reach the
	// map, and none may stop the rest of the file parsing.
	Zenith_EditorPrefs xPrefs;
	xPrefs.Parse(
		"anim_rig=no_separators_at_all\n"
		"anim_rig=only|one_separator\n"
		"anim_rig=a|b|c|d\n"          // one field too many
		"anim_rig=|skel.zskel|m.zmodel\n"  // empty clip key
		"anim_rig=\n"
		"camera_speed=33\n");

	ZENITH_ASSERT_EQ(xPrefs.GetAnimRigChoiceCount(), 0u, "no malformed record may reach the map");
	ZENITH_ASSERT_EQ_FLOAT(xPrefs.m_fCameraMoveSpeed, 33.0f, 1e-4f, "a malformed rig line must not stop the parse");

	// The one legal shape in the same family: an EMPTY model half is allowed (a
	// skeleton with no preview mesh is a real intermediate state).
	xPrefs.Parse("anim_rig=game:A.zanim|game:A.zskel|\n");
	Zenith_EditorPrefs_AnimRigChoice xChoice;
	ZENITH_ASSERT_TRUE(xPrefs.TryGetAnimRigChoice("game:A.zanim", xChoice), "a skeleton-only record is legal");
	ZENITH_ASSERT_TRUE(xChoice.m_strSkeletonPath == "game:A.zskel", "skeleton half read");
	ZENITH_ASSERT_TRUE(xChoice.m_strPreviewModelPath.empty(), "model half is empty");
}

ZENITH_TEST(EditorPrefs, AnimRigChoiceRemovalAndSeparatorRefusal)
{
	Zenith_EditorPrefs xPrefs;

	// A clip with no asset path has nothing to key on and is silently ignored.
	xPrefs.SetAnimRigChoice("", "s.zskel", "m.zmodel");
	ZENITH_ASSERT_EQ(xPrefs.GetAnimRigChoiceCount(), 0u, "an empty clip key stores nothing");

	// ★ CLEARING BOTH HALVES REMOVES THE RECORD rather than storing a choice that
	// resolves to nothing — otherwise the session would find a remembered answer,
	// fail to resolve it, and report "needs rig selection" forever with a stored
	// choice in the file claiming otherwise.
	xPrefs.SetAnimRigChoice("game:A.zanim", "s.zskel", "m.zmodel");
	ZENITH_ASSERT_EQ(xPrefs.GetAnimRigChoiceCount(), 1u, "stored");
	xPrefs.SetAnimRigChoice("game:A.zanim", "", "");
	ZENITH_ASSERT_EQ(xPrefs.GetAnimRigChoiceCount(), 0u, "clearing both halves removes the record");

	xPrefs.SetAnimRigChoice("game:A.zanim", "s.zskel", "m.zmodel");
	xPrefs.RemoveAnimRigChoice("game:B.zanim");   // absent key: no-op, no crash
	ZENITH_ASSERT_EQ(xPrefs.GetAnimRigChoiceCount(), 1u, "removing an absent key changes nothing");
	xPrefs.RemoveAnimRigChoice("game:A.zanim");
	ZENITH_ASSERT_EQ(xPrefs.GetAnimRigChoiceCount(), 0u, "removing a present key drops it");

	// A path carrying the field separator could not be read back as the record it
	// was written from, so it is refused at the setter instead of corrupting the
	// file (and every later record on the same line with it).
	xPrefs.SetAnimRigChoice("game:we|ird.zanim", "s.zskel", "m.zmodel");
	xPrefs.SetAnimRigChoice("game:A.zanim", "we|ird.zskel", "m.zmodel");
	xPrefs.SetAnimRigChoice("game:A.zanim", "s.zskel", "we|ird.zmodel");
	ZENITH_ASSERT_EQ(xPrefs.GetAnimRigChoiceCount(), 0u, "a separator in any field is refused");
}
