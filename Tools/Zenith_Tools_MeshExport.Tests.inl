#include "UnitTests/Zenith_UnitTests.h"

// ============================================================================
// Routing tests for the generic Assimp mesh walk. Compiled INTO
// Zenith_Tools_MeshExport.cpp (included as its last line) because the predicate
// under test is file-local -- Zenith_Tools_MeshExport.h declares nothing.
// ============================================================================

ZENITH_TEST(MeshExport, StickFigureRoutingIsByDirectory)
{
	// ★★ THE RULE THAT KEEPS THE GENERATOR'S RIG ITS OWN. ExportAllMeshes runs the
	// generic Assimp walk at Zenith_Engine.cpp:558 and GenerateTestAssets runs the
	// StickFigure generator at :573. The generator writes
	// Meshes/StickFigure/StickFigure.gltf as a Blender round-trip DEBUG EXPORT, so
	// on every boot after the first the walk re-imported it and wrote
	// StickFigure.zskel + StickFigure.zmodel to the generator's own paths -- correct
	// afterwards only because the generator overwrote them fifteen lines later, i.e.
	// by ORDERING alone -- plus a stray StickFigure_Mesh0_Mat0.zmesh that nothing
	// references and nothing sweeps (SweepGeneratedStickFigureClips sweeps .zanim).
	//
	// ★ TESTED AS A PREDICATE, not by watching a directory walk. Watching the walk
	// cannot fail for the reason this guard exists: the end state on disk is already
	// correct with the guard absent (the generator overwrites the two files it cares
	// about), and the one observable difference -- the stray .zmesh -- depends on a
	// gitignored asset tree a fresh clone does not have. A file-existence test would
	// therefore pass whether or not the skip is wired at all.
	ZENITH_ASSERT_TRUE(
		IsGeneratedStickFigureSourcePath("C:/dev/Zenith/Zenith/Assets/Meshes/StickFigure/StickFigure.gltf"),
		"the generator's own glTF, spelled with forward slashes, belongs to GenerateStickFigureAssets");

	// ★ THE MIXED-SEPARATOR FORM IS THE ONE THE WALK ACTUALLY PRODUCES (observed in
	// a real boot log): the root is spelled with forward slashes and
	// recursive_directory_iterator appends native ones. A predicate that only matched
	// the tidy form would be inert in production while every other case here passed.
	ZENITH_ASSERT_TRUE(
		IsGeneratedStickFigureSourcePath("c:/dev/zenith/Zenith/Assets/Meshes\\StickFigure\\StickFigure.gltf"),
		"...and so does the MIXED-separator path the real walk hands us");

	ZENITH_ASSERT_TRUE(
		IsGeneratedStickFigureSourcePath("C:\\dev\\Zenith\\Zenith\\Assets\\Meshes\\StickFigure\\StickFigure.gltf"),
		"...and the all-backslash Windows form");

	// ★ THE SKIP IS DELIBERATELY NARROW. ProceduralTree's Tree.gltf is
	// generator-written too, but the tree set writes NO .zmodel of its own: Tree.zmodel
	// and Tree_Mesh0_Mat0.zmesh exist ONLY because this walk re-imports that glTF, and
	// Tree_Mesh0_Mat0.zmesh is loaded by the engine. Skipping it would delete assets
	// that are actually read.
	ZENITH_ASSERT_TRUE(
		!IsGeneratedStickFigureSourcePath("C:/dev/Zenith/Zenith/Assets/Meshes/ProceduralTree/Tree.gltf"),
		"the tree generator's glTF must STILL be imported - the walk is what produces Tree.zmodel");

	// ArmChain.gltf is consumed the same way, by two engine unit tests
	// (Zenith_UnitTests.Tests.inl).
	ZENITH_ASSERT_TRUE(
		!IsGeneratedStickFigureSourcePath("C:/dev/Zenith/Zenith/Assets/Meshes/UnitTest/ArmChain.gltf"),
		"nor may the unit-test rig be skipped - two engine units read what this walk writes");

	// ★ THE RULE IS THE DIRECTORY, NOT THE FILE NAME. A model merely NAMED after the
	// stick figure, sitting somewhere else, is an ordinary source.
	ZENITH_ASSERT_TRUE(
		!IsGeneratedStickFigureSourcePath("C:/dev/Zenith/Zenith/Assets/Meshes/Props/StickFigure_Statue.gltf"),
		"a file merely named after the stick figure, outside Meshes/StickFigure/, is an ordinary source");

	// ★ THE TRAILING SLASH IN szSTICKFIGURE_DIR IS WHAT MAKES THIS NEGATIVE. Drop it
	// and every sibling directory sharing the prefix silently stops being exported,
	// with no error anywhere.
	ZENITH_ASSERT_TRUE(
		!IsGeneratedStickFigureSourcePath("C:/dev/Zenith/Zenith/Assets/Meshes/StickFigureOfSpeech/x.gltf"),
		"a sibling directory whose name merely starts the same way must not be skipped");
}
