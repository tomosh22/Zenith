//------------------------------------------------------------------------------
// Zenith_EditorAnimCtrlCommands unit tests (WU-6.5).
// Included at the bottom of Zenith_EditorAnimCtrlCommands.cpp.
//
// ★ THE DOCUMENT'S OWN TESTS DRIVE THE VERBS; THESE DRIVE THE COMMANDS. The two
// halves fail differently: a verb bug shows as "the edit did not happen", a
// command bug as "the edit happened and the undo did not put it back" — and the
// second only ever appears on a stack that has been walked in both directions,
// which is what every case here does.
//
// Headless, allocation-only, no device and no UI. None of these is
// requiresGraphics.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"
#include "UnitTests/Zenith_UnitTests.h"

#include <filesystem>

namespace
{
	// A document open on a throwaway path inside the OS temp dir. OpenFresh, so
	// nothing is read and nothing has to exist; nothing here ever saves.
	struct AnimCtrlCmdFixture
	{
		std::filesystem::path m_xDirectory;
		std::string m_strPath;

		explicit AnimCtrlCmdFixture(const char* szLeafDirectory)
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
			m_strPath = (m_xDirectory / "cmd.zanimctrl").generic_string();
		}

		~AnimCtrlCmdFixture()
		{
			std::error_code xError;
			std::filesystem::remove_all(m_xDirectory, xError);
		}

		AnimCtrlCmdFixture(const AnimCtrlCmdFixture&) = delete;
		AnimCtrlCmdFixture& operator=(const AnimCtrlCmdFixture&) = delete;
	};
}

ZENITH_TEST(AnimCtrlCommands, ATransitionEditIsAWholeListSnapshotSoIndicesCannotGoStale)
{
	// ★ THE CASE A FINER-GRAINED COMMAND GETS WRONG. A transition has no
	// identity — it is a struct in a vector addressed by index — so a command
	// holding "transition 1 of Idle" starts naming a different edge the moment
	// transition 0 is removed. The snapshot has no index to go stale.
	AnimCtrlCmdFixture xFixture("zenith_animctrlcmd_translist");
	Zenith_AnimControllerDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.OpenFresh(xFixture.m_strPath) == ZENITH_ANIMCTRLDOC_OPEN_OK, "open fresh");
	ZENITH_ASSERT_TRUE(xDoc.AddState("Idle"), "Idle");
	ZENITH_ASSERT_TRUE(xDoc.AddState("Walk"), "Walk");
	ZENITH_ASSERT_TRUE(xDoc.AddState("Run"), "Run");
	ZENITH_ASSERT_TRUE(xDoc.AddTransition("Idle", "Walk"), "Idle -> Walk is index 0");
	ZENITH_ASSERT_TRUE(xDoc.AddTransition("Idle", "Run"), "Idle -> Run is index 1");
	ZENITH_ASSERT_TRUE(xDoc.SetTransitionDuration("Idle", 1, 0.75f), "give index 1 a duration");

	// Removing index 0 renumbers the one behind it.
	ZENITH_ASSERT_TRUE(xDoc.RemoveTransition("Idle", 0), "remove index 0");
	Flux_StateTransition xTransition;
	ZENITH_ASSERT_TRUE(xDoc.GetTransition("Idle", 0, xTransition), "one edge is left");
	ZENITH_ASSERT_EQ(xTransition.m_strTargetStateName, std::string("Run"), "and it is the Run one");
	ZENITH_ASSERT_EQ_FLOAT(xTransition.m_fTransitionDuration, 0.75f, 1e-6f, "still carrying its duration");

	// Both undos replay list snapshots, so nothing has to know which index moved.
	xDoc.Undo();   // the remove
	ZENITH_ASSERT_EQ(xDoc.GetTransitionCount("Idle"), 2u, "both edges are back");
	ZENITH_ASSERT_TRUE(xDoc.GetTransition("Idle", 0, xTransition), "index 0 resolves");
	ZENITH_ASSERT_EQ(xTransition.m_strTargetStateName, std::string("Walk"), "★ and is the Walk one again");
	ZENITH_ASSERT_TRUE(xDoc.GetTransition("Idle", 1, xTransition), "index 1 resolves");
	ZENITH_ASSERT_EQ_FLOAT(xTransition.m_fTransitionDuration, 0.75f, 1e-6f, "★ with its duration intact");

	xDoc.Undo();   // the duration
	ZENITH_ASSERT_TRUE(xDoc.GetTransition("Idle", 1, xTransition), "index 1 still resolves");
	ZENITH_ASSERT_EQ_FLOAT(xTransition.m_fTransitionDuration, 0.15f, 1e-6f,
		"and the duration is back at the engine default");

	xDoc.CloseDiscardingChanges();
}

ZENITH_TEST(AnimCtrlCommands, StateAddUndoRestoresThePreviousDefault)
{
	// AddState makes the FIRST state of a machine its default (that is
	// Flux_AnimationStateMachineDef::AddState's own rule), and an undo that left
	// that behind would point the machine at a state it had just deleted —
	// which Update resolves to nullptr and answers with an empty pose, silently.
	AnimCtrlCmdFixture xFixture("zenith_animctrlcmd_default");
	Zenith_AnimControllerDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.OpenFresh(xFixture.m_strPath) == ZENITH_ANIMCTRLDOC_OPEN_OK, "open fresh");

	ZENITH_ASSERT_TRUE(xDoc.AddState("First"), "the first state");
	ZENITH_ASSERT_EQ(xDoc.GetDefaultStateName(), std::string("First"), "becomes the default");
	ZENITH_ASSERT_TRUE(xDoc.AddState("Second"), "the second does not");
	ZENITH_ASSERT_EQ(xDoc.GetDefaultStateName(), std::string("First"), "the default is unchanged");

	xDoc.Undo();
	ZENITH_ASSERT_EQ(xDoc.GetDefaultStateName(), std::string("First"),
		"undoing the second add leaves the default alone");
	xDoc.Undo();
	ZENITH_ASSERT_TRUE(xDoc.GetDefaultStateName().empty(),
		"★ and undoing the first takes the default with it");

	xDoc.Redo();
	ZENITH_ASSERT_EQ(xDoc.GetDefaultStateName(), std::string("First"), "the redo puts it back");

	xDoc.CloseDiscardingChanges();
}

ZENITH_TEST(AnimCtrlCommands, TheParameterTableSnapshotRestoresTypeAndDefaultTogether)
{
	// ★ NAME AND TYPE ARE ONE FACT. Flux_AnimationParameters::Parameter overlays
	// float / int32 / bool in a union, so a restore that got the name right and
	// the type wrong would hand a float 4.25 back as 1082130432 — and nothing
	// would assert.
	AnimCtrlCmdFixture xFixture("zenith_animctrlcmd_params");
	Zenith_AnimControllerDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.OpenFresh(xFixture.m_strPath) == ZENITH_ANIMCTRLDOC_OPEN_OK, "open fresh");

	ZENITH_ASSERT_TRUE(xDoc.AddParameter(Zenith_AnimCtrlParameterDecl::Float("Speed", 4.25f)), "a float");
	ZENITH_ASSERT_TRUE(xDoc.AddParameter(Zenith_AnimCtrlParameterDecl::Int("Ammo", 7)), "an int");
	ZENITH_ASSERT_TRUE(xDoc.AddParameter(Zenith_AnimCtrlParameterDecl::Trigger("Jump")), "and a trigger");
	ZENITH_ASSERT_TRUE(xDoc.RemoveParameter("Speed"), "remove the float");

	Zenith_AnimCtrlParameterDecl xDecl;
	ZENITH_ASSERT_FALSE(xDoc.GetParameter("Speed", xDecl), "it is gone");

	xDoc.Undo();
	ZENITH_ASSERT_TRUE(xDoc.GetParameter("Speed", xDecl), "★ and the undo brings it back");
	ZENITH_ASSERT_TRUE(xDecl.m_eType == Flux_AnimationParameters::ParamType::Float, "★ as a Float");
	ZENITH_ASSERT_EQ_FLOAT(xDecl.m_fDefault, 4.25f, 1e-6f, "★ with its default, out of the right union member");

	ZENITH_ASSERT_TRUE(xDoc.GetParameter("Ammo", xDecl), "and the int is still there");
	ZENITH_ASSERT_TRUE(xDecl.m_eType == Flux_AnimationParameters::ParamType::Int, "as an Int");
	ZENITH_ASSERT_EQ(xDecl.m_iDefault, 7, "with its own default");
	ZENITH_ASSERT_TRUE(xDoc.GetParameter("Jump", xDecl), "and so is the trigger");
	ZENITH_ASSERT_TRUE(xDecl.m_eType == Flux_AnimationParameters::ParamType::Trigger, "as a Trigger");

	xDoc.CloseDiscardingChanges();
}

ZENITH_TEST(AnimCtrlCommands, ACompoundUndoesItsChildrenInReverse)
{
	// ★ REVERSE IS NOT COSMETIC. The children were applied in an order whose
	// intermediate states are each legal; the exact inverse is the only order
	// whose intermediates are equally legal. Undoing "add state, add transition
	// to it" forwards would try to remove a state something still points at, and
	// the removal would take the transition with it — so the second child's undo
	// would find nothing and the graph would come back short an edge.
	AnimCtrlCmdFixture xFixture("zenith_animctrlcmd_compound");
	Zenith_AnimControllerDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.OpenFresh(xFixture.m_strPath) == ZENITH_ANIMCTRLDOC_OPEN_OK, "open fresh");
	ZENITH_ASSERT_TRUE(xDoc.AddState("Idle"), "a state to hang the group off");

	ZENITH_ASSERT_TRUE(xDoc.BeginCompound(), "open a group");
	ZENITH_ASSERT_TRUE(xDoc.AddState("Walk"), "add Walk inside it");
	ZENITH_ASSERT_TRUE(xDoc.AddTransition("Idle", "Walk"), "and an edge to it");
	ZENITH_ASSERT_TRUE(xDoc.IsCompoundOpen(), "the group is still open");
	ZENITH_ASSERT_TRUE(xDoc.EndCompound("Add Walk"), "and it pushes one command");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 2u, "two entries: the first state, and the group");

	xDoc.Undo();
	ZENITH_ASSERT_FALSE(xDoc.HasState("Walk"), "Walk is gone");
	ZENITH_ASSERT_EQ(xDoc.GetTransitionCount("Idle"), 0u, "and so is the edge");

	xDoc.Redo();
	ZENITH_ASSERT_TRUE(xDoc.HasState("Walk"), "★ the redo runs FORWARD and Walk is back");
	ZENITH_ASSERT_EQ(xDoc.GetTransitionCount("Idle"), 1u, "★ with the edge that needed it to exist first");

	xDoc.CloseDiscardingChanges();
}

ZENITH_TEST(AnimCtrlCommands, ARolledBackCompoundReachesNeitherTheStackNorTheGraph)
{
	AnimCtrlCmdFixture xFixture("zenith_animctrlcmd_rollback");
	Zenith_AnimControllerDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.OpenFresh(xFixture.m_strPath) == ZENITH_ANIMCTRLDOC_OPEN_OK, "open fresh");
	ZENITH_ASSERT_TRUE(xDoc.AddState("Idle"), "one state");
	const u_int uDepth = xDoc.GetUndoStackSize();

	ZENITH_ASSERT_TRUE(xDoc.BeginCompound(), "open a group");
	ZENITH_ASSERT_TRUE(xDoc.AddState("A"), "A");
	ZENITH_ASSERT_TRUE(xDoc.AddState("B"), "B");
	ZENITH_ASSERT_FALSE(xDoc.EndCompound("Discarded", /*bKeep*/ false), "roll it back");

	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), uDepth,
		"★ the partial application never reached the stack");
	ZENITH_ASSERT_EQ(xDoc.GetStateCount(), 1u, "★ and never reached the graph");
	ZENITH_ASSERT_TRUE(xDoc.HasState("Idle"), "what was there before is untouched");

	xDoc.CloseDiscardingChanges();
}

ZENITH_TEST(AnimCtrlCommands, TheLayerSnapshotCarriesEachLayersWholeMachineThroughAReorderAndBack)
{
	// ★ THE CASE THE CHEAP INVERSE GETS WRONG. A reorder's obvious undo is "move
	// it back", which is exact for ONE move and wrong the moment a second edit
	// renumbers the destination — so the command carries the whole list, and the
	// whole list means each layer's ENTIRE payload, its state machine included.
	// A snapshot that carried only ids and weights would restore the ORDER and
	// silently empty every graph, which no order assertion would notice.
	AnimCtrlCmdFixture xFixture("zenith_animctrlcmd_layers");
	Zenith_AnimControllerDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.OpenFresh(xFixture.m_strPath) == ZENITH_ANIMCTRLDOC_OPEN_OK, "open fresh");

	const u_int uBase = xDoc.AddLayer("Base");
	const u_int uAim = xDoc.AddLayer("Aim");
	ZENITH_ASSERT_TRUE(uBase != uFLUX_INVALID_LAYER_ID && uAim != uFLUX_INVALID_LAYER_ID, "two layers");

	// Author something INSIDE each layer's machine, so the round trip has
	// content to lose.
	ZENITH_ASSERT_TRUE(xDoc.SelectMachine(uBase), "select Base's machine");
	ZENITH_ASSERT_TRUE(xDoc.AddState("Idle"), "Idle");
	ZENITH_ASSERT_TRUE(xDoc.AddState("Walk"), "Walk");
	ZENITH_ASSERT_TRUE(xDoc.AddTransition("Idle", "Walk"), "Idle -> Walk");
	ZENITH_ASSERT_TRUE(xDoc.SelectMachine(uAim), "select Aim's machine");
	ZENITH_ASSERT_TRUE(xDoc.AddState("Aiming"), "one state in the overlay");
	ZENITH_ASSERT_TRUE(xDoc.SetLayerWeight(uAim, 0.5f), "and a weight on it");
	ZENITH_ASSERT_TRUE(xDoc.SetLayerBlendMode(uAim, LAYER_BLEND_ADDITIVE), "and a blend mode");

	// ---- reorder -------------------------------------------------------------
	ZENITH_ASSERT_TRUE(xDoc.MoveLayer(uAim, 0u), "the overlay becomes the base layer");
	u_int uIdAt = uFLUX_INVALID_LAYER_ID;
	ZENITH_ASSERT_TRUE(xDoc.GetLayerIdAt(0u, uIdAt) && uIdAt == uAim, "it is index 0");

	// ★ THE MACHINES CAME THROUGH THE BYTES. This is what a field-only snapshot
	// would have destroyed, and the ONLY way to see it is to look inside.
	ZENITH_ASSERT_TRUE(xDoc.SelectMachine(uBase), "look inside Base");
	ZENITH_ASSERT_EQ(xDoc.GetStateCount(), 2u, "★ both of its states survived the rebuild");
	ZENITH_ASSERT_EQ(xDoc.GetTransitionCount("Idle"), 1u, "★ and so did its transition");
	ZENITH_ASSERT_TRUE(xDoc.SelectMachine(uAim), "and inside Aim");
	ZENITH_ASSERT_EQ(xDoc.GetStateCount(), 1u, "★ whose one state is also there");

	// ---- and back ------------------------------------------------------------
	xDoc.Undo();
	ZENITH_ASSERT_TRUE(xDoc.GetLayerIdAt(0u, uIdAt) && uIdAt == uBase, "★ the exact previous order is back");
	ZENITH_ASSERT_TRUE(xDoc.GetLayerIdAt(1u, uIdAt) && uIdAt == uAim, "id for id");

	Flux_LayerBlendMode eMode = LAYER_BLEND_OVERRIDE;
	float fWeight = 0.0f;
	ZENITH_ASSERT_TRUE(xDoc.GetLayerBlendMode(uAim, eMode) && eMode == LAYER_BLEND_ADDITIVE,
		"★ and payload for payload — the blend mode rode the bytes");
	ZENITH_ASSERT_TRUE(xDoc.GetLayerWeight(uAim, fWeight), "the weight resolves");
	ZENITH_ASSERT_EQ_FLOAT(fWeight, 0.5f, 1e-6f, "and is the one that was set");

	// A redo replays the snapshot forwards, so the two directions are the same
	// operation with two arguments rather than two implementations.
	xDoc.Redo();
	ZENITH_ASSERT_TRUE(xDoc.GetLayerIdAt(0u, uIdAt) && uIdAt == uAim, "★ redo puts the reorder back");
	ZENITH_ASSERT_TRUE(xDoc.SelectMachine(uBase), "and Base is still whole");
	ZENITH_ASSERT_EQ(xDoc.GetStateCount(), 2u, "with both states");

	xDoc.CloseDiscardingChanges();
}
