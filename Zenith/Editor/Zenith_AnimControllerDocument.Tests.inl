//------------------------------------------------------------------------------
// Zenith_AnimControllerDocument unit tests (WU-6.5).
// Included at the bottom of Zenith_AnimControllerDocument.cpp.
//
// ★ THE HEADLINE PROPERTIES are the two an undo stack over a NAME-keyed graph
// can get silently wrong: a RENAME undone has to bring back the transitions that
// pointed at the old name, and a REMOVE undone has to bring back the ones that
// pointed at the removed state — which are stored in lists the removal does not
// otherwise touch. Everything else here pins an edge of the same machinery.
//
// All of it is CPU-only and runs headless under the Null backend: the defs are
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
	// Fixture — Zenith_AnimationDocument.Tests.inl's shape: a private temp
	// directory removed on the way out, plus a ForceUnload of every registry path
	// the test caused to be loaded, so a throwaway asset never lingers in the live
	// registry the suite runs inside.
	//
	// ★ DECLARE THE FIXTURE BEFORE THE DOCUMENT IN EVERY TEST. ForceUnload deletes
	// regardless of refcount, so anything holding a view of one of these assets
	// has to be destroyed first, and declaration order is what guarantees that.
	//--------------------------------------------------------------------------
	struct AnimCtrlDocFixture
	{
		std::filesystem::path m_xDirectory;
		std::string m_strPath;
		Zenith_Vector<std::string> m_axTrackedPaths;

		explicit AnimCtrlDocFixture(const char* szLeafDirectory)
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
			m_strPath = (m_xDirectory / "probe.zanimctrl").generic_string();
		}

		std::string PathFor(const char* szLeafName)
		{
			std::string strPath = (m_xDirectory / szLeafName).generic_string();
			m_axTrackedPaths.PushBack(strPath);
			return strPath;
		}

		~AnimCtrlDocFixture()
		{
			Zenith_AssetRegistry::ForceUnload(m_strPath);
			for (u_int u = 0; u < m_axTrackedPaths.GetSize(); ++u)
			{
				Zenith_AssetRegistry::ForceUnload(m_axTrackedPaths.Get(u));
			}
			std::error_code xError;
			std::filesystem::remove_all(m_xDirectory, xError);
		}

		AnimCtrlDocFixture(const AnimCtrlDocFixture&) = delete;
		AnimCtrlDocFixture& operator=(const AnimCtrlDocFixture&) = delete;
	};

	// A .zanimctrl with a top-level machine holding ONE state called "Idle".
	void AnimCtrlDocWriteProbeFile(const std::string& strPath)
	{
		Flux_AnimatorControllerDef xDef;
		xDef.SetName("Probe");
		Flux_AnimationStateMachineDef& xMachine = xDef.GetOrCreateStateMachineDef();
		xMachine.SetName("Base");
		xMachine.AddState("Idle");
		xDef.Export(strPath);
	}

	bool AnimCtrlDocParseFile(const std::string& strPath, Flux_AnimatorControllerDef& xOut)
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

	// The count of transitions in strFrom that target strTo. What a rename and a
	// remove are actually judged on.
	u_int AnimCtrlDocCountEdges(const Zenith_AnimControllerDocument& xDoc,
		const std::string& strFrom, const std::string& strTo)
	{
		u_int uCount = 0;
		for (u_int u = 0; u < xDoc.GetTransitionCount(strFrom); ++u)
		{
			Flux_StateTransition xTransition;
			if (xDoc.GetTransition(strFrom, u, xTransition) && xTransition.m_strTargetStateName == strTo)
			{
				++uCount;
			}
		}
		return uCount;
	}
}

ZENITH_TEST(AnimCtrlDoc, OpenDeepCopiesTheDefAndTheAssetIsUntouchedUntilSave)
{
	AnimCtrlDocFixture xFixture("zenith_animctrldoc_open");
	AnimCtrlDocWriteProbeFile(xFixture.m_strPath);

	Zenith_AnimControllerDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.Open(xFixture.m_strPath) == ZENITH_ANIMCTRLDOC_OPEN_OK, "the probe opens");
	ZENITH_ASSERT_TRUE(xDoc.IsOpen(), "and is open");
	ZENITH_ASSERT_FALSE(xDoc.IsDirty(), "and clean");
	ZENITH_ASSERT_EQ(xDoc.GetStateCount(), 1u, "with the one state the file carried");

	ZENITH_ASSERT_TRUE(xDoc.AddState("Walk"), "a state is added to the WORKING copy");
	ZENITH_ASSERT_EQ(xDoc.GetStateCount(), 2u, "which now has two");
	ZENITH_ASSERT_TRUE(xDoc.IsDirty(), "and the document is dirty");

	// ★ THE LIVE ASSET HAS NOT MOVED. Editing the asset's own def in place would
	// push half-finished graph state into whatever BuildFromControllerDef reads
	// next, and leave nothing to Discard back to.
	const Zenith_AnimatorControllerAsset* pxAsset = xDoc.GetAsset();
	ZENITH_ASSERT_NOT_NULL(pxAsset, "the live asset is reachable");
	ZENITH_ASSERT_NOT_NULL(pxAsset->GetDef().GetStateMachineDef(), "and still has its machine");
	ZENITH_ASSERT_EQ(pxAsset->GetDef().GetStateMachineDef()->GetStates().GetSize(), 1u,
		"★ with ONE state — the edit landed in the working copy only");

	xDoc.CloseDiscardingChanges();
}

ZENITH_TEST(AnimCtrlDoc, SaveWritesTheEnvelopeAndRefreshesTheLiveAsset)
{
	AnimCtrlDocFixture xFixture("zenith_animctrldoc_save");
	AnimCtrlDocWriteProbeFile(xFixture.m_strPath);

	Zenith_AnimControllerDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.Open(xFixture.m_strPath) == ZENITH_ANIMCTRLDOC_OPEN_OK, "the probe opens");
	ZENITH_ASSERT_TRUE(xDoc.AddState("Walk"), "add a second state");
	ZENITH_ASSERT_TRUE(xDoc.Save() == ZENITH_ANIMCTRLDOC_SAVE_OK, "and save it");
	ZENITH_ASSERT_FALSE(xDoc.IsDirty(), "the document is clean afterwards");

	// The FILE re-parses through the same contract a cold load uses — envelope
	// (type id 7, schema 1) included, because ParseStream refuses anything else.
	Flux_AnimatorControllerDef xReloaded;
	ZENITH_ASSERT_TRUE(AnimCtrlDocParseFile(xFixture.m_strPath, xReloaded),
		"★ the saved bytes parse as a .zanimctrl — envelope and all");
	ZENITH_ASSERT_NOT_NULL(xReloaded.GetStateMachineDef(), "with its top-level machine");
	ZENITH_ASSERT_EQ(xReloaded.GetStateMachineDef()->GetStates().GetSize(), 2u, "and both states");

	// And the LIVE asset was refreshed IN PLACE — not ForceUnloaded, which would
	// delete an object other code may be holding a view of.
	const Zenith_AnimatorControllerAsset* pxAsset = xDoc.GetAsset();
	ZENITH_ASSERT_NOT_NULL(pxAsset, "the live asset is still there");
	ZENITH_ASSERT_EQ(pxAsset->GetDef().GetStateMachineDef()->GetStates().GetSize(), 2u,
		"and now agrees with the file");

	xDoc.CloseDiscardingChanges();
}

ZENITH_TEST(AnimCtrlDoc, OpenFreshTargetsAPathThatNeedNotExist)
{
	AnimCtrlDocFixture xFixture("zenith_animctrldoc_fresh");
	const std::string strPath = xFixture.PathFor("brand_new.zanimctrl");

	Zenith_AnimControllerDocument xDoc;
	// Open REFUSES a file that is not there — "the file was missing" and "the
	// path was typed wrong" are the same observation.
	ZENITH_ASSERT_TRUE(xDoc.Open(strPath) == ZENITH_ANIMCTRLDOC_OPEN_FAILED_NO_ASSET,
		"Open refuses a path with no file behind it");

	ZENITH_ASSERT_TRUE(xDoc.OpenFresh(strPath) == ZENITH_ANIMCTRLDOC_OPEN_OK, "OpenFresh takes it");
	ZENITH_ASSERT_TRUE(xDoc.IsOpen(), "and opens");
	ZENITH_ASSERT_TRUE(xDoc.IsDirty(), "★ DIRTY from the first frame — nothing on disk describes this yet");
	ZENITH_ASSERT_EQ(xDoc.GetStateCount(), 0u, "with an empty machine");
	ZENITH_ASSERT_NOT_NULL(xDoc.GetDef().GetStateMachineDef(),
		"but the top-level machine already exists, so a recipe's first AddState has somewhere to go");

	ZENITH_ASSERT_TRUE(xDoc.AddState("Idle"), "author a state");
	ZENITH_ASSERT_TRUE(xDoc.Save() == ZENITH_ANIMCTRLDOC_SAVE_OK, "and the first save creates the file");
	ZENITH_ASSERT_TRUE(std::filesystem::exists(std::filesystem::path(strPath)), "which is now on disk");

	xDoc.CloseDiscardingChanges();
}

ZENITH_TEST(AnimCtrlDoc, EveryEditIsExactlyOneUndoStep)
{
	AnimCtrlDocFixture xFixture("zenith_animctrldoc_undo");
	Zenith_AnimControllerDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.OpenFresh(xFixture.PathFor("undo.zanimctrl")) == ZENITH_ANIMCTRLDOC_OPEN_OK, "open fresh");

	ZENITH_ASSERT_TRUE(xDoc.AddParameter(Zenith_AnimCtrlParameterDecl::Float("Speed", 0.0f)), "declare Speed");
	ZENITH_ASSERT_TRUE(xDoc.AddState("Idle"), "add Idle");
	ZENITH_ASSERT_TRUE(xDoc.AddState("Walk"), "add Walk");
	ZENITH_ASSERT_TRUE(xDoc.SetStateClip("Idle", "IdleClip"), "give Idle a clip");
	ZENITH_ASSERT_TRUE(xDoc.AddTransition("Idle", "Walk"), "connect them");
	ZENITH_ASSERT_TRUE(xDoc.AddCondition("Idle", 0, "Speed",
		Flux_TransitionCondition::CompareOp::Greater, 0.1f), "and condition it");

	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 6u, "★ six edits, six undo steps — never five, never seven");

	// Peeling them back one at a time lands on the empty machine it started from.
	xDoc.Undo();   // condition
	Flux_StateTransition xTransition;
	ZENITH_ASSERT_TRUE(xDoc.GetTransition("Idle", 0, xTransition), "the transition survives the condition's undo");
	ZENITH_ASSERT_EQ(xTransition.m_xConditions.GetSize(), 0u, "with no conditions on it");

	xDoc.Undo();   // transition
	ZENITH_ASSERT_EQ(xDoc.GetTransitionCount("Idle"), 0u, "the transition is gone");

	xDoc.Undo();   // clip
	std::string strClip;
	ZENITH_ASSERT_FALSE(xDoc.GetStateClipName("Idle", strClip), "and Idle has no clip leaf again");

	xDoc.Undo();   // Walk
	ZENITH_ASSERT_EQ(xDoc.GetStateCount(), 1u, "Walk is gone");
	xDoc.Undo();   // Idle
	ZENITH_ASSERT_EQ(xDoc.GetStateCount(), 0u, "and so is Idle");
	ZENITH_ASSERT_TRUE(xDoc.GetDefaultStateName().empty(),
		"★ and the default state went with it — AddState made Idle the default, and an undo that "
		"left it behind would point the machine at a state it no longer has");

	xDoc.Undo();   // parameter
	Zenith_Vector<std::string> axParams;
	xDoc.GetParameterNamesSorted(axParams);
	ZENITH_ASSERT_EQ(axParams.GetSize(), 0u, "the declaration is gone too");

	// And the redo walks all the way forward again.
	for (u_int u = 0; u < 6; ++u)
	{
		xDoc.Redo();
	}
	ZENITH_ASSERT_EQ(xDoc.GetStateCount(), 2u, "the redo restores both states");
	ZENITH_ASSERT_TRUE(xDoc.GetTransition("Idle", 0, xTransition), "and the transition");
	ZENITH_ASSERT_EQ(xTransition.m_xConditions.GetSize(), 1u, "with its condition");

	xDoc.CloseDiscardingChanges();
}

ZENITH_TEST(AnimCtrlDoc, RenameUndoneRestoresTheTransitionsThatReferencedIt)
{
	AnimCtrlDocFixture xFixture("zenith_animctrldoc_rename");
	Zenith_AnimControllerDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.OpenFresh(xFixture.PathFor("rename.zanimctrl")) == ZENITH_ANIMCTRLDOC_OPEN_OK, "open fresh");

	ZENITH_ASSERT_TRUE(xDoc.AddState("Idle"), "Idle");
	ZENITH_ASSERT_TRUE(xDoc.AddState("Walk"), "Walk");
	ZENITH_ASSERT_TRUE(xDoc.AddState("Run"), "Run");
	ZENITH_ASSERT_TRUE(xDoc.AddTransition("Idle", "Walk"), "Idle -> Walk");
	ZENITH_ASSERT_TRUE(xDoc.AddTransition("Run", "Walk"), "Run -> Walk");
	ZENITH_ASSERT_TRUE(xDoc.AddTransition("Walk", "Run"), "Walk -> Run");
	ZENITH_ASSERT_TRUE(xDoc.SetDefaultState("Walk"), "and Walk is the default");

	ZENITH_ASSERT_TRUE(xDoc.RenameState("Walk", "Stride"), "the rename takes");
	ZENITH_ASSERT_FALSE(xDoc.HasState("Walk"), "Walk is gone");
	ZENITH_ASSERT_TRUE(xDoc.HasState("Stride"), "Stride is there");
	ZENITH_ASSERT_EQ(xDoc.GetDefaultStateName(), std::string("Stride"), "the default followed the rename");
	ZENITH_ASSERT_EQ(AnimCtrlDocCountEdges(xDoc, "Idle", "Stride"), 1u, "Idle's edge was retargeted");
	ZENITH_ASSERT_EQ(AnimCtrlDocCountEdges(xDoc, "Run", "Stride"), 1u, "and so was Run's");
	ZENITH_ASSERT_EQ(xDoc.GetTransitionCount("Stride"), 1u, "and the renamed state kept its own outgoing edge");

	xDoc.Undo();

	// ★ THE POINT OF THE TEST. The rename is undone as the inverse rename, which
	// retargets the referrers back for free — a remove-plus-add implementation
	// would have had to remember them separately, and would have been the thing
	// that silently did not.
	ZENITH_ASSERT_TRUE(xDoc.HasState("Walk"), "Walk is back");
	ZENITH_ASSERT_FALSE(xDoc.HasState("Stride"), "and Stride is gone");
	ZENITH_ASSERT_EQ(AnimCtrlDocCountEdges(xDoc, "Idle", "Walk"), 1u, "★ Idle points at Walk again");
	ZENITH_ASSERT_EQ(AnimCtrlDocCountEdges(xDoc, "Run", "Walk"), 1u, "★ and so does Run");
	ZENITH_ASSERT_EQ(AnimCtrlDocCountEdges(xDoc, "Walk", "Run"), 1u, "and Walk's own edge came with it");
	ZENITH_ASSERT_EQ(xDoc.GetDefaultStateName(), std::string("Walk"), "and the default came back too");

	xDoc.CloseDiscardingChanges();
}

ZENITH_TEST(AnimCtrlDoc, RemovingAStatePrunesInboundTransitionsAndTheUndoPutsThemBack)
{
	AnimCtrlDocFixture xFixture("zenith_animctrldoc_remove");
	Zenith_AnimControllerDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.OpenFresh(xFixture.PathFor("remove.zanimctrl")) == ZENITH_ANIMCTRLDOC_OPEN_OK, "open fresh");

	ZENITH_ASSERT_TRUE(xDoc.AddState("Idle"), "Idle");
	ZENITH_ASSERT_TRUE(xDoc.AddState("Walk"), "Walk");
	ZENITH_ASSERT_TRUE(xDoc.AddState("Run"), "Run");
	ZENITH_ASSERT_TRUE(xDoc.AddTransition("Idle", "Walk"), "Idle -> Walk");
	ZENITH_ASSERT_TRUE(xDoc.AddTransition("Run", "Walk"), "Run -> Walk");
	ZENITH_ASSERT_TRUE(xDoc.AddTransition("Walk", "Run"), "Walk -> Run");

	ZENITH_ASSERT_TRUE(xDoc.RemoveState("Walk"), "Walk is removed");
	// ★ Flux_AnimationStateMachineDef::RemoveState does NOT do this. Leaving the
	// inbound edges makes StartTransition resolve a name to nullptr every frame
	// the condition holds — and it simply does not transition, silently.
	ZENITH_ASSERT_EQ(xDoc.GetTransitionCount("Idle"), 0u, "★ Idle's edge to it went with it");
	ZENITH_ASSERT_EQ(xDoc.GetTransitionCount("Run"), 0u, "★ and so did Run's");

	xDoc.Undo();
	ZENITH_ASSERT_TRUE(xDoc.HasState("Walk"), "the state is back");
	ZENITH_ASSERT_EQ(AnimCtrlDocCountEdges(xDoc, "Idle", "Walk"), 1u, "★ with Idle's edge");
	ZENITH_ASSERT_EQ(AnimCtrlDocCountEdges(xDoc, "Run", "Walk"), 1u, "★ and Run's");
	ZENITH_ASSERT_EQ(AnimCtrlDocCountEdges(xDoc, "Walk", "Run"), 1u, "and its own");

	xDoc.CloseDiscardingChanges();
}

ZENITH_TEST(AnimCtrlDoc, ARefusalChangesNothingAndPushesNoUndoEntry)
{
	AnimCtrlDocFixture xFixture("zenith_animctrldoc_refusals");
	Zenith_AnimControllerDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.OpenFresh(xFixture.PathFor("refuse.zanimctrl")) == ZENITH_ANIMCTRLDOC_OPEN_OK, "open fresh");
	ZENITH_ASSERT_TRUE(xDoc.AddState("Idle"), "one state");
	const u_int uDepth = xDoc.GetUndoStackSize();

	ZENITH_ASSERT_FALSE(xDoc.AddState("Idle"), "a duplicate name is refused");
	ZENITH_ASSERT_FALSE(xDoc.AddState(""), "and so is an empty one");
	ZENITH_ASSERT_FALSE(xDoc.RemoveState("Nope"), "removing a state that is not there is refused");
	ZENITH_ASSERT_FALSE(xDoc.SetDefaultState("Nope"), "so is defaulting to one");
	ZENITH_ASSERT_FALSE(xDoc.AddTransition("Idle", "Nope"), "so is a transition to one");
	ZENITH_ASSERT_FALSE(xDoc.RemoveTransition("Idle", 0), "so is removing a transition that is not there");
	ZENITH_ASSERT_FALSE(xDoc.SetTransitionDuration("Idle", 0, 0.5f), "so is editing one");
	// ★ A CONDITION'S TYPE COMES FROM THE DECLARATION, so an UNDECLARED parameter
	// has no type to take and the condition is refused rather than defaulted to
	// Float — a condition whose type disagrees with its parameter reads the wrong
	// union member and is never true.
	ZENITH_ASSERT_TRUE(xDoc.AddTransition("Idle", "Idle"), "a self-transition IS allowed");
	ZENITH_ASSERT_FALSE(xDoc.AddCondition("Idle", 0, "Undeclared",
		Flux_TransitionCondition::CompareOp::Greater, 1.0f), "★ a condition on an undeclared parameter is refused");

	// ★ AN ASSIGNMENT THAT IS ALREADY SATISFIED IS SUCCESS, NOT A REFUSAL, and
	// this line is the one that pays for the distinction. AddState made "Idle"
	// the default because it was the FIRST state — so the most natural authoring
	// order there is (AddState, AddState, SetDefaultState) asks for a value that
	// is already in place, every single time. Returning false there made a
	// recipe that merely stated its intent explicitly assert at boot under
	// AnimSmActionChecked, and read in a unit as "the entry point could not be
	// set". What must stay true is the STACK DEPTH, not the bool: a no-op is not
	// an edit, so it contributes zero undo steps.
	ZENITH_ASSERT_TRUE(xDoc.SetDefaultState("Idle"),
		"★ setting the default to what it already is SUCCEEDS — the caller's intent is satisfied");
	ZENITH_ASSERT_EQ(xDoc.GetDefaultStateName(), std::string("Idle"), "and the default is what was asked for");

	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), uDepth + 1u,
		"★ and it pushed NOTHING: exactly one of those calls CHANGED anything (the self-transition), "
		"so exactly one undo entry exists — 'one edit, one undo step' counts EDITS, not calls");

	// The same rule on the other assignment verbs, so the family is pinned
	// rather than just its worst offender.
	const u_int uAfterDefault = xDoc.GetUndoStackSize();
	ZENITH_ASSERT_TRUE(xDoc.SetTransitionDuration("Idle", 0, 0.15f),
		"the engine-default duration re-stated is satisfied, not refused");
	ZENITH_ASSERT_TRUE(xDoc.SetTransitionInterruptible("Idle", 0, true),
		"and so is the engine-default interruptible flag");
	ZENITH_ASSERT_TRUE(xDoc.SetStateClip("Idle", std::string()),
		"and clearing a clip on a state that has none");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), uAfterDefault,
		"★ none of the three moved the stack");

	xDoc.CloseDiscardingChanges();
}

ZENITH_TEST(AnimCtrlDoc, AConditionTakesItsTypeAndItsThresholdSlotFromTheDeclaration)
{
	AnimCtrlDocFixture xFixture("zenith_animctrldoc_condtype");
	Zenith_AnimControllerDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.OpenFresh(xFixture.PathFor("cond.zanimctrl")) == ZENITH_ANIMCTRLDOC_OPEN_OK, "open fresh");
	ZENITH_ASSERT_TRUE(xDoc.AddState("A"), "A");
	ZENITH_ASSERT_TRUE(xDoc.AddState("B"), "B");
	ZENITH_ASSERT_TRUE(xDoc.AddTransition("A", "B"), "A -> B");
	ZENITH_ASSERT_TRUE(xDoc.AddParameter(Zenith_AnimCtrlParameterDecl::Int("Ammo", 0)), "declare an INT");
	ZENITH_ASSERT_TRUE(xDoc.AddParameter(Zenith_AnimCtrlParameterDecl::Bool("Armed", false)), "and a BOOL");

	ZENITH_ASSERT_TRUE(xDoc.AddCondition("A", 0, "Ammo", Flux_TransitionCondition::CompareOp::Greater, 3.0f),
		"an int condition");
	ZENITH_ASSERT_TRUE(xDoc.AddCondition("A", 0, "Armed", Flux_TransitionCondition::CompareOp::Equal, 1.0f),
		"and a bool one");

	Flux_StateTransition xTransition;
	ZENITH_ASSERT_TRUE(xDoc.GetTransition("A", 0, xTransition), "the transition reads back");
	ZENITH_ASSERT_EQ(xTransition.m_xConditions.GetSize(), 2u, "with both conditions");
	ZENITH_ASSERT_TRUE(xTransition.m_xConditions.Get(0).m_eParamType == Flux_AnimationParameters::ParamType::Int,
		"★ the first took Int from the declaration, not Float from the caller's literal");
	ZENITH_ASSERT_EQ(xTransition.m_xConditions.Get(0).m_iThreshold, 3,
		"and 3.0f landed in the INT slot of the union");
	ZENITH_ASSERT_TRUE(xTransition.m_xConditions.Get(1).m_eParamType == Flux_AnimationParameters::ParamType::Bool,
		"★ and the second took Bool");
	ZENITH_ASSERT_TRUE(xTransition.m_xConditions.Get(1).m_bThreshold, "with a non-zero literal reading as true");

	xDoc.CloseDiscardingChanges();
}

ZENITH_TEST(AnimCtrlDoc, EditorPositionsRoundTripThroughTheFile)
{
	// ★ A LAYOUT IS AUTHORED DATA HERE. Flux_AnimationState::m_xEditorPosition is
	// a serialized field of the .zanimctrl, which is why the panel needs no
	// side-car layout file and no Zenith_EditorPrefs entry — and why a node drag
	// dirties the document.
	AnimCtrlDocFixture xFixture("zenith_animctrldoc_layout");
	const std::string strPath = xFixture.PathFor("layout.zanimctrl");

	{
		Zenith_AnimControllerDocument xDoc;
		ZENITH_ASSERT_TRUE(xDoc.OpenFresh(strPath) == ZENITH_ANIMCTRLDOC_OPEN_OK, "open fresh");
		ZENITH_ASSERT_TRUE(xDoc.AddState("Idle"), "one state");
		ZENITH_ASSERT_TRUE(xDoc.SetStateEditorPosition("Idle", Zenith_Maths::Vector2(321.0f, 654.0f)),
			"place its node");
		ZENITH_ASSERT_TRUE(xDoc.Save() == ZENITH_ANIMCTRLDOC_SAVE_OK, "and save");
		xDoc.CloseDiscardingChanges();
	}

	Flux_AnimatorControllerDef xReloaded;
	ZENITH_ASSERT_TRUE(AnimCtrlDocParseFile(strPath, xReloaded), "the file parses");
	const Flux_AnimationState* pxState = xReloaded.GetStateMachineDef()->GetState("Idle");
	ZENITH_ASSERT_NOT_NULL(pxState, "the state is there");
	ZENITH_ASSERT_EQ_FLOAT(pxState->m_xEditorPosition.x, 321.0f, 1e-4f, "★ with its x");
	ZENITH_ASSERT_EQ_FLOAT(pxState->m_xEditorPosition.y, 654.0f, 1e-4f, "★ and its y");
}

ZENITH_TEST(AnimCtrlDoc, AnExternalModificationIsRefusedAndOverridable)
{
	AnimCtrlDocFixture xFixture("zenith_animctrldoc_conflict");
	AnimCtrlDocWriteProbeFile(xFixture.m_strPath);

	Zenith_AnimControllerDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.Open(xFixture.m_strPath) == ZENITH_ANIMCTRLDOC_OPEN_OK, "the probe opens");
	ZENITH_ASSERT_TRUE(xDoc.AddState("Walk"), "edit it");
	ZENITH_ASSERT_FALSE(xDoc.HasExternalModification(), "nothing has touched the file yet");

	// Somebody else writes it.
	{
		Flux_AnimatorControllerDef xOther;
		xOther.SetName("SomebodyElse");
		xOther.GetOrCreateStateMachineDef().AddState("Fly");
		xOther.Export(xFixture.m_strPath);
	}
	ZENITH_ASSERT_TRUE(xDoc.HasExternalModification(), "★ the content hash notices");
	ZENITH_ASSERT_TRUE(xDoc.Save() == ZENITH_ANIMCTRLDOC_SAVE_CONFLICT_EXTERNAL, "and the save is refused");
	ZENITH_ASSERT_TRUE(xDoc.IsDirty(), "leaving the document dirty");

	Flux_AnimatorControllerDef xOnDisk;
	ZENITH_ASSERT_TRUE(AnimCtrlDocParseFile(xFixture.m_strPath, xOnDisk), "the other writer's file parses");
	ZENITH_ASSERT_EQ(xOnDisk.GetName(), std::string("SomebodyElse"),
		"★ and it is still THEIRS — a refused save writes nothing");

	ZENITH_ASSERT_TRUE(xDoc.SaveOverwritingExternal() == ZENITH_ANIMCTRLDOC_SAVE_OK, "the forced save goes through");
	ZENITH_ASSERT_FALSE(xDoc.HasExternalModification(), "and the recorded hash moves with it");

	xDoc.CloseDiscardingChanges();
}

ZENITH_TEST(AnimCtrlDoc, ACompoundIsOneUndoStepAndAnEmptyOneIsNotAnEntry)
{
	AnimCtrlDocFixture xFixture("zenith_animctrldoc_compound");
	Zenith_AnimControllerDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.OpenFresh(xFixture.PathFor("compound.zanimctrl")) == ZENITH_ANIMCTRLDOC_OPEN_OK, "open fresh");

	ZENITH_ASSERT_TRUE(xDoc.BeginCompound(), "open a group");
	ZENITH_ASSERT_TRUE(xDoc.AddState("A"), "A");
	ZENITH_ASSERT_TRUE(xDoc.AddState("B"), "B");
	ZENITH_ASSERT_TRUE(xDoc.AddTransition("A", "B"), "A -> B");
	ZENITH_ASSERT_TRUE(xDoc.EndCompound("Author Pair"), "and close it");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 1u, "★ three edits, ONE undo step");

	xDoc.Undo();
	ZENITH_ASSERT_EQ(xDoc.GetStateCount(), 0u,
		"★ and one Ctrl+Z takes all three back — in REVERSE, so the transition goes before its states");

	// An empty group leaves nothing behind to press Ctrl+Z through.
	ZENITH_ASSERT_TRUE(xDoc.BeginCompound(), "open another");
	ZENITH_ASSERT_FALSE(xDoc.AddState(""), "do nothing legal in it");
	ZENITH_ASSERT_FALSE(xDoc.EndCompound("Nothing"), "and it reports that it pushed nothing");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 0u, "the stack is untouched");

	xDoc.CloseDiscardingChanges();
}
