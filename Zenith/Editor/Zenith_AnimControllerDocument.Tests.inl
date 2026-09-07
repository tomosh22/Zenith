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
// WU-7.2's layer tests do not stop at the document: the reordered def is saved,
// re-read by the real reader and BUILT into a real Flux_AnimationController, so
// the blend ORDER is judged where it actually matters.
#include "Flux/MeshAnimation/Flux_AnimationController.h"

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

//==============================================================================
// The layer list (WU-7.2)
//==============================================================================

ZENITH_TEST(AnimCtrlDoc, ReorderingLayersLeavesEveryEditBoundToItsOwnLayerId)
{
	// ★ THE FLAGSHIP, AND THE REASON D43 EXISTS. A layer's INDEX is its position
	// in the BLEND ORDER — the thing a reorder changes and the thing an insert
	// renumbers — so anything that addressed a layer by index would silently
	// start editing a different one the moment the list moved. Nothing below
	// stops at the document: the reordered def is saved, re-read by the real
	// reader and built into a real Flux_AnimationController, because "the editor
	// says the order changed" and "the runtime composes in that order" are two
	// different claims and only the second one animates anything.
	AnimCtrlDocFixture xFixture("zenith_animctrldoc_layerorder");
	const std::string strPath = xFixture.PathFor("layers.zanimctrl");

	Zenith_AnimControllerDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.OpenFresh(strPath) == ZENITH_ANIMCTRLDOC_OPEN_OK, "open fresh");

	const u_int uBase = xDoc.AddLayer("Base");
	const u_int uAim = xDoc.AddLayer("Aim");
	const u_int uFace = xDoc.AddLayer("Face");
	ZENITH_ASSERT_TRUE(uBase != uFLUX_INVALID_LAYER_ID, "Base gets an id");
	ZENITH_ASSERT_TRUE(uAim != uBase, "Aim's is a different one");
	ZENITH_ASSERT_TRUE(uFace != uAim && uFace != uBase, "and so is Face's");
	ZENITH_ASSERT_EQ(xDoc.GetLayerCount(), 3u, "three layers");

	ZENITH_ASSERT_TRUE(xDoc.SetLayerWeight(uBase, 1.0f), "Base at full weight");
	ZENITH_ASSERT_TRUE(xDoc.SetLayerWeight(uAim, 0.5f), "Aim at a half");
	ZENITH_ASSERT_TRUE(xDoc.SetLayerWeight(uFace, 0.25f), "Face at a quarter");

	u_int uIndex = 0;
	ZENITH_ASSERT_TRUE(xDoc.GetLayerIndex(uFace, uIndex), "Face has a position");
	ZENITH_ASSERT_EQ(uIndex, 2u, "at the end of the blend order");

	// ---- the move -----------------------------------------------------------
	ZENITH_ASSERT_TRUE(xDoc.MoveLayer(uFace, 0u), "move the last layer to the front");

	ZENITH_ASSERT_TRUE(xDoc.GetLayerIndex(uFace, uIndex) && uIndex == 0u, "Face is now the base");
	ZENITH_ASSERT_TRUE(xDoc.GetLayerIndex(uBase, uIndex) && uIndex == 1u, "Base moved up one");
	ZENITH_ASSERT_TRUE(xDoc.GetLayerIndex(uAim, uIndex) && uIndex == 2u, "Aim moved up one");

	// ★ EVERY ID STILL REPORTS ITS OWN VALUE. This is the assertion the whole
	// unit is for: three weights, three ids, and a reorder in between.
	float fWeight = 0.0f;
	ZENITH_ASSERT_TRUE(xDoc.GetLayerWeight(uBase, fWeight), "Base resolves");
	ZENITH_ASSERT_EQ_FLOAT(fWeight, 1.0f, 1e-6f, "★ and still carries ITS weight");
	ZENITH_ASSERT_TRUE(xDoc.GetLayerWeight(uAim, fWeight), "Aim resolves");
	ZENITH_ASSERT_EQ_FLOAT(fWeight, 0.5f, 1e-6f, "★ and still carries ITS weight");
	ZENITH_ASSERT_TRUE(xDoc.GetLayerWeight(uFace, fWeight), "Face resolves");
	ZENITH_ASSERT_EQ_FLOAT(fWeight, 0.25f, 1e-6f, "★ and still carries ITS weight");

	// ---- and the RUNTIME agrees ---------------------------------------------
	ZENITH_ASSERT_TRUE(xDoc.Save() == ZENITH_ANIMCTRLDOC_SAVE_OK, "it saves");

	Flux_AnimatorControllerDef xParsed;
	ZENITH_ASSERT_TRUE(AnimCtrlDocParseFile(strPath, xParsed), "the saved bytes parse as a .zanimctrl");
	ZENITH_ASSERT_EQ(xParsed.GetLayerCount(), 3u, "carrying all three layers");

	// Heap, not stack: a Flux_AnimationController carries two FLUX_MAX_BONES poses.
	Flux_AnimationController* pxController = new Flux_AnimationController();
	ZENITH_ASSERT_TRUE(pxController->BuildFromControllerDef(xParsed, nullptr),
		"and it builds — a false here is a dangling clip or mask reference, not a style point");
	ZENITH_ASSERT_EQ(pxController->GetLayerCount(), 3u, "three runtime layers");

	ZENITH_ASSERT_NOT_NULL(pxController->GetLayer(0), "there is a base layer");
	ZENITH_ASSERT_EQ(pxController->GetLayer(0)->GetName(), std::string("Face"),
		"★ the runtime composes in the order the editor left, base first");
	ZENITH_ASSERT_EQ(pxController->GetLayer(1)->GetName(), std::string("Base"), "then Base");
	ZENITH_ASSERT_EQ(pxController->GetLayer(2)->GetName(), std::string("Aim"), "then Aim");

	// ★ AND THE ID SURVIVED THE ROUND TRIP, which is what makes GetLayerById the
	// handle a game holds: BuildFromControllerDef ADOPTS the def's id rather than
	// minting a fresh one, so the number the editor used is the number gameplay
	// resolves through.
	const Flux_AnimationLayer* pxRuntimeBase = pxController->GetLayerById(uBase);
	const Flux_AnimationLayer* pxRuntimeAim = pxController->GetLayerById(uAim);
	const Flux_AnimationLayer* pxRuntimeFace = pxController->GetLayerById(uFace);
	ZENITH_ASSERT_NOT_NULL(pxRuntimeBase, "Base resolves by the id the editor minted");
	ZENITH_ASSERT_NOT_NULL(pxRuntimeAim, "and so does Aim");
	ZENITH_ASSERT_NOT_NULL(pxRuntimeFace, "and Face");
	if (pxRuntimeBase != nullptr && pxRuntimeAim != nullptr && pxRuntimeFace != nullptr)
	{
		ZENITH_ASSERT_EQ_FLOAT(pxRuntimeAim->GetWeight(), 0.5f, 1e-6f,
			"★ with the weight that id was given, wherever the reorder put it");
		ZENITH_ASSERT_EQ_FLOAT(pxRuntimeFace->GetWeight(), 0.25f, 1e-6f, "and so does Face's");
		ZENITH_ASSERT_EQ_FLOAT(pxRuntimeBase->GetWeight(), 1.0f, 1e-6f, "and Base's");
	}

	pxController->ReleaseAssetReferences();
	delete pxController;

	// ---- the undo restores the EXACT order ----------------------------------
	xDoc.Undo();
	u_int uIdAt = uFLUX_INVALID_LAYER_ID;
	ZENITH_ASSERT_TRUE(xDoc.GetLayerIdAt(0u, uIdAt) && uIdAt == uBase, "★ Base is the base again");
	ZENITH_ASSERT_TRUE(xDoc.GetLayerIdAt(1u, uIdAt) && uIdAt == uAim, "★ Aim is back at 1");
	ZENITH_ASSERT_TRUE(xDoc.GetLayerIdAt(2u, uIdAt) && uIdAt == uFace, "★ and Face at 2");
	ZENITH_ASSERT_TRUE(xDoc.GetLayerWeight(uFace, fWeight) && fWeight == 0.25f,
		"and the payloads came back with the positions — the snapshot is the whole layer, not its index");

	xDoc.CloseDiscardingChanges();
}

ZENITH_TEST(AnimCtrlDoc, AMaskPathIsRefusedOnAnAdditiveLayerAndSucceedsOnceItIsOverride)
{
	// ★ AN ADDITIVE LAYER IGNORES ITS MASK ENTIRELY — Flux_AnimationController's
	// layer loop tests LAYER_BLEND_ADDITIVE first and goes to
	// Flux_SkeletonPose::AdditiveBlend, whose signature has no mask in it. So an
	// accepted assignment here would let somebody author a whole mask, save it,
	// assign it and observe nothing, with every gate green and nothing to grep
	// for. The rule is stated ONCE (Zenith_BoneMaskDocument::LayerAcceptsMask)
	// and this asserts that the document asks it rather than restating it.
	AnimCtrlDocFixture xFixture("zenith_animctrldoc_layermask");
	Zenith_AnimControllerDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.OpenFresh(xFixture.PathFor("mask.zanimctrl")) == ZENITH_ANIMCTRLDOC_OPEN_OK, "open fresh");

	const u_int uLayer = xDoc.AddLayer("Overlay");
	ZENITH_ASSERT_TRUE(uLayer != uFLUX_INVALID_LAYER_ID, "a layer");
	ZENITH_ASSERT_TRUE(xDoc.SetLayerBlendMode(uLayer, LAYER_BLEND_ADDITIVE), "made additive");

	const u_int uDepth = xDoc.GetUndoStackSize();
	ZENITH_ASSERT_FALSE(xDoc.SetLayerMaskAssetPath(uLayer, "game:Anim/UpperBody.zanimmask"),
		"★ a mask path on it is REFUSED");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), uDepth, "and pushes no undo entry");
	std::string strPath;
	ZENITH_ASSERT_TRUE(xDoc.GetLayerMaskAssetPath(uLayer, strPath), "the layer still resolves");
	ZENITH_ASSERT_TRUE(strPath.empty(), "with no mask assigned — a refusal changes NOTHING");

	// ★ AND IT SAYS WHY, IN THE ONE WORDING. "Refused" would send a reader
	// looking for a broken path; the notice names the blend mode.
	ZENITH_ASSERT_EQ(xDoc.GetLastLayerDiagnostic(),
		std::string(Zenith_BoneMaskDocument::AdditiveLayerMaskNotice()),
		"★ and the diagnostic is the ONE wording of the rule, not a second copy of it");

	// ---- override: the same call succeeds ------------------------------------
	ZENITH_ASSERT_TRUE(xDoc.SetLayerBlendMode(uLayer, LAYER_BLEND_OVERRIDE), "back to override");
	ZENITH_ASSERT_TRUE(xDoc.SetLayerMaskAssetPath(uLayer, "game:Anim/UpperBody.zanimmask"),
		"★ and now the SAME assignment lands — the refusal was the blend mode, not the path");
	ZENITH_ASSERT_TRUE(xDoc.GetLayerMaskAssetPath(uLayer, strPath) && !strPath.empty(), "the path is stored");
	ZENITH_ASSERT_TRUE(xDoc.GetLastLayerDiagnostic().empty(), "and the diagnostic is cleared by the success");

	// ★ SWITCHING BACK TO ADDITIVE DOES NOT DELETE THE ASSIGNMENT. The runtime
	// stops consulting it and the UI says so; silently dropping an authored path
	// on a combo-box change would be an unrecoverable edit disguised as a toggle.
	ZENITH_ASSERT_TRUE(xDoc.SetLayerBlendMode(uLayer, LAYER_BLEND_ADDITIVE), "additive again");
	ZENITH_ASSERT_TRUE(xDoc.GetLayerMaskAssetPath(uLayer, strPath) && !strPath.empty(),
		"★ the mask path is STILL there");
	ZENITH_ASSERT_EQ(xDoc.GetLastLayerDiagnostic(),
		std::string(Zenith_BoneMaskDocument::AdditiveLayerMaskNotice()),
		"and the notice explains that nothing will read it");

	// Clearing is always allowed, whatever the blend mode.
	ZENITH_ASSERT_TRUE(xDoc.SetLayerMaskAssetPath(uLayer, std::string()),
		"★ CLEARING a mask on an additive layer is allowed — the refusal is about assigning one");

	xDoc.CloseDiscardingChanges();
}

ZENITH_TEST(AnimCtrlDoc, RemovingTheLayerWhoseMachineIsSelectedFallsBackToTheTopLevel)
{
	// A selector naming a layer that no longer exists makes FindMachine answer
	// null, and every verb afterwards refuses with nothing to point at — which
	// reads as "the editor stopped working" rather than as a dangling selection.
	AnimCtrlDocFixture xFixture("zenith_animctrldoc_layerselect");
	Zenith_AnimControllerDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.OpenFresh(xFixture.PathFor("select.zanimctrl")) == ZENITH_ANIMCTRLDOC_OPEN_OK, "open fresh");

	const u_int uKeep = xDoc.AddLayer("Keep");
	const u_int uDrop = xDoc.AddLayer("Drop");
	ZENITH_ASSERT_TRUE(uKeep != uFLUX_INVALID_LAYER_ID && uDrop != uFLUX_INVALID_LAYER_ID, "two layers");

	ZENITH_ASSERT_TRUE(xDoc.SelectMachine(uDrop), "select the doomed layer's machine");
	ZENITH_ASSERT_TRUE(xDoc.AddState("Swing"), "and author a state in it");
	ZENITH_ASSERT_EQ(xDoc.GetStateCount(), 1u, "which lands there");

	ZENITH_ASSERT_TRUE(xDoc.RemoveLayer(uDrop), "remove the selected layer");
	ZENITH_ASSERT_EQ(xDoc.GetSelectedMachineId(), uANIMCTRL_TOP_LEVEL_MACHINE,
		"★ the selection falls back to the top-level machine");
	ZENITH_ASSERT_TRUE(xDoc.AddState("Idle"), "★ and the document is still editable, which is the point");

	// The surviving layer is untouched, so the fallback is scoped to the removal.
	ZENITH_ASSERT_EQ(xDoc.GetLayerCount(), 1u, "one layer left");
	u_int uIdAt = uFLUX_INVALID_LAYER_ID;
	ZENITH_ASSERT_TRUE(xDoc.GetLayerIdAt(0u, uIdAt) && uIdAt == uKeep, "and it is the one that was kept");

	// ★ A REMOVED ID IS NEVER HANDED OUT AGAIN, which is what makes a stale id
	// resolve to nothing rather than to somebody else's layer.
	const u_int uNew = xDoc.AddLayer("Later");
	ZENITH_ASSERT_TRUE(uNew != uDrop, "★ the next layer does NOT inherit the removed layer's id");

	xDoc.CloseDiscardingChanges();
}

ZENITH_TEST(AnimCtrlDoc, EveryLayerEditIsOneUndoStepAndAnAssignmentNoOpPushesNothing)
{
	// The invariant the ASSIGNMENT-vs-CREATION rule is actually stated on: the
	// bool says whether the value asked for is in place, and the UNDO DEPTH says
	// whether an edit happened. A no-op is not an edit.
	AnimCtrlDocFixture xFixture("zenith_animctrldoc_layerundo");
	Zenith_AnimControllerDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.OpenFresh(xFixture.PathFor("undo.zanimctrl")) == ZENITH_ANIMCTRLDOC_OPEN_OK, "open fresh");

	const u_int uLayer = xDoc.AddLayer("Aim");
	ZENITH_ASSERT_TRUE(uLayer != uFLUX_INVALID_LAYER_ID, "a layer");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 1u, "adding one is a step");

	ZENITH_ASSERT_TRUE(xDoc.SetLayerWeight(uLayer, 0.4f), "set a weight");
	ZENITH_ASSERT_TRUE(xDoc.SetLayerBlendMode(uLayer, LAYER_BLEND_ADDITIVE), "set a blend mode");
	ZENITH_ASSERT_TRUE(xDoc.SetLayerEmitEvents(uLayer, false), "silence its events (D36)");
	ZENITH_ASSERT_TRUE(xDoc.RenameLayer(uLayer, "Overlay"), "rename it");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 5u, "five edits, five undo steps");

	// ★ RE-STATING EVERY ONE OF THEM IS SATISFACTION, NOT REFUSAL, and pushes
	// nothing. A recipe that says out loud what it wants must not fail because
	// the value was already there — that is the defect SetDefaultState cost a red
	// test for, one family over.
	ZENITH_ASSERT_TRUE(xDoc.SetLayerWeight(uLayer, 0.4f), "the weight it already has");
	ZENITH_ASSERT_TRUE(xDoc.SetLayerBlendMode(uLayer, LAYER_BLEND_ADDITIVE), "the mode it already has");
	ZENITH_ASSERT_TRUE(xDoc.SetLayerEmitEvents(uLayer, false), "the flag it already has");
	ZENITH_ASSERT_TRUE(xDoc.RenameLayer(uLayer, "Overlay"), "the name it already has");
	ZENITH_ASSERT_TRUE(xDoc.MoveLayer(uLayer, 0u), "the position it already holds");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 5u, "★ and NOT ONE of them pushed a step");

	// ★ A CLAMPED WEIGHT IS COMPARED AFTER THE CLAMP, so asking twice for 2.0 on
	// a layer already pinned at 1.0 is recognised as the no-op it is.
	ZENITH_ASSERT_TRUE(xDoc.SetLayerWeight(uLayer, 2.0f), "an out-of-range weight is clamped, not refused");
	float fWeight = 0.0f;
	ZENITH_ASSERT_TRUE(xDoc.GetLayerWeight(uLayer, fWeight), "it resolves");
	ZENITH_ASSERT_EQ_FLOAT(fWeight, 1.0f, 1e-6f, "to 1.0 — what Flux_AnimationLayer::SetWeight would store");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 6u, "that WAS an edit");
	ZENITH_ASSERT_TRUE(xDoc.SetLayerWeight(uLayer, 2.0f), "asking again is satisfied");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 6u, "★ and pushes nothing");

	// A miss is a genuine refusal on the two verbs that answer "did I create /
	// remove one", and on the index MoveLayer cannot reach.
	ZENITH_ASSERT_TRUE(xDoc.AddLayer("") == uFLUX_INVALID_LAYER_ID, "an unnamed layer is refused");
	ZENITH_ASSERT_FALSE(xDoc.RemoveLayer(uFLUX_INVALID_LAYER_ID), "so is removing an id nothing carries");
	ZENITH_ASSERT_FALSE(xDoc.MoveLayer(uLayer, 7u), "and a destination past the end is a caller error");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), 6u, "none of which touched the stack");

	// The undo of one field edit restores that field and leaves the others.
	xDoc.Undo();   // the clamp
	xDoc.Undo();   // the rename
	std::string strName;
	ZENITH_ASSERT_TRUE(xDoc.GetLayerName(uLayer, strName), "the layer resolves");
	ZENITH_ASSERT_EQ(strName, std::string("Aim"), "★ the rename came back");
	bool bEmit = true;
	ZENITH_ASSERT_TRUE(xDoc.GetLayerEmitEvents(uLayer, bEmit), "and so does its event flag");
	ZENITH_ASSERT_FALSE(bEmit, "★ which the rename's undo left alone");

	xDoc.CloseDiscardingChanges();
}

//==============================================================================
// The blend-tree sub-graph (WU-7.3)
//==============================================================================

ZENITH_TEST(AnimCtrlDoc, ABlendSpaceIsItsOwnKindAndOnlyANestIsStillComplex)
{
	// ★ THE REFUSAL DID NOT GO AWAY, IT GOT SMALLER — and that distinction is the
	// whole of this unit. WU-6.5 answered COMPLEX for a blend space, a composite
	// and a container alike; a blend space is now editable and the other two are
	// still refused BY NAME, because assigning anything to a nest would delete a
	// sub-graph nothing can reconstruct and report success.
	AnimCtrlDocFixture xFixture("zenith_animctrldoc_classify");
	Zenith_AnimControllerDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.OpenFresh(xFixture.PathFor("kinds.zanimctrl")) == ZENITH_ANIMCTRLDOC_OPEN_OK, "open fresh");

	ZENITH_ASSERT_TRUE(xDoc.ClassifyBlendTree(nullptr) == ZENITH_ANIMCTRL_TREE_EMPTY, "no tree is EMPTY");
	{
		Flux_BlendTreeNode_Clip xClip;
		ZENITH_ASSERT_TRUE(xDoc.ClassifyBlendTree(&xClip) == ZENITH_ANIMCTRL_TREE_SINGLE_CLIP, "a leaf is SINGLE_CLIP");
	}
	{
		Flux_BlendTreeNode_BlendSpace1D xSpace;
		ZENITH_ASSERT_TRUE(xDoc.ClassifyBlendTree(&xSpace) == ZENITH_ANIMCTRL_TREE_BLENDSPACE_1D,
			"★ a 1D space is its OWN kind now, not COMPLEX");
	}
	{
		Flux_BlendTreeNode_BlendSpace2D xSpace;
		ZENITH_ASSERT_TRUE(xDoc.ClassifyBlendTree(&xSpace) == ZENITH_ANIMCTRL_TREE_BLENDSPACE_2D,
			"★ and so is a 2D space");
	}
	{
		Flux_BlendTreeNode_Blend xNest;
		ZENITH_ASSERT_TRUE(xDoc.ClassifyBlendTree(&xNest) == ZENITH_ANIMCTRL_TREE_COMPLEX,
			"★ what is LEFT in COMPLEX is the nests");
	}

	// And a nest is still refused end to end, through a real state.
	ZENITH_ASSERT_TRUE(xDoc.AddState("Nested"), "a state");
	ZENITH_ASSERT_TRUE(xDoc.SetStateClip("Nested", "Idle"), "with a clip leaf to start");
	{
		// Reach past the document ONCE, to author the shape the document cannot
		// create — which is exactly the point of the refusal being tested.
		Flux_AnimationStateMachineDef* pxMachine =
			const_cast<Flux_AnimationStateMachineDef*>(xDoc.GetSelectedMachineDef());
		Flux_AnimationState* pxState = pxMachine->GetState("Nested");
		Flux_BlendTreeNode* pxOld = pxState->GetBlendTree();
		pxState->SetBlendTree(new Flux_BlendTreeNode_Additive());
		delete pxOld;
	}

	ZENITH_ASSERT_TRUE(xDoc.GetStateTreeKind("Nested") == ZENITH_ANIMCTRL_TREE_COMPLEX, "the state reads as COMPLEX");
	const u_int uDepth = xDoc.GetUndoStackSize();
	ZENITH_ASSERT_FALSE(xDoc.SetStateTreeKind("Nested", ZENITH_ANIMCTRL_TREE_BLENDSPACE_1D),
		"★ converting a nest is REFUSED");
	ZENITH_ASSERT_TRUE(xDoc.GetLastBlendTreeDiagnostic() == std::string(
		Zenith_AnimControllerDocument::BlendTreeRefusalText()), "with the ONE wording of the refusal");
	ZENITH_ASSERT_FALSE(xDoc.SetStateClip("Nested", "Idle"), "and so is assigning a clip to it");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), uDepth, "neither touched the stack");
	ZENITH_ASSERT_TRUE(xDoc.GetStateTreeKind("Nested") == ZENITH_ANIMCTRL_TREE_COMPLEX, "★ and the nest is still there");

	xDoc.CloseDiscardingChanges();
}

ZENITH_TEST(AnimCtrlDoc, ConvertingASingleClipSeedsTheFirstPointAndUndoRestoresTheTreeBytes)
{
	// ★ THE UNDO IS BYTE-EXACT, NOT APPROXIMATE, and that is why the command is a
	// whole-state snapshot: converting a clip leaf to a blend space throws the
	// leaf away — with its playback rate and its playhead — and nothing inside the
	// space could reconstruct them.
	AnimCtrlDocFixture xFixture("zenith_animctrldoc_convert");
	Zenith_AnimControllerDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.OpenFresh(xFixture.PathFor("convert.zanimctrl")) == ZENITH_ANIMCTRLDOC_OPEN_OK, "open fresh");
	ZENITH_ASSERT_TRUE(xDoc.AddState("Locomotion"), "a state");
	ZENITH_ASSERT_TRUE(xDoc.SetStateClip("Locomotion", "WalkClip"), "playing one clip");

	// Freeze the tree's bytes BEFORE the conversion. This is the same walk the
	// undo command captures through, so "identical afterwards" is the exact
	// property being claimed rather than a field-by-field approximation of it.
	Zenith_Vector<char> axBefore;
	{
		const Flux_AnimationState* pxState = xDoc.GetSelectedMachineDef()->GetState("Locomotion");
		Zenith_DataStream xStream(1);
		pxState->WriteToDataStream(xStream);
		const char* pcBytes = static_cast<const char*>(xStream.GetData());
		for (u_int64 ul = 0; ul < xStream.GetCursor(); ++ul)
		{
			axBefore.PushBack(pcBytes[ul]);
		}
	}
	ZENITH_ASSERT_GT(axBefore.GetSize(), 0u, "the state serializes to something");

	const u_int uDepth = xDoc.GetUndoStackSize();
	ZENITH_ASSERT_TRUE(xDoc.SetStateTreeKind("Locomotion", ZENITH_ANIMCTRL_TREE_BLENDSPACE_1D), "convert to 1D");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), uDepth + 1u, "as ONE undo step");
	ZENITH_ASSERT_TRUE(xDoc.GetStateTreeKind("Locomotion") == ZENITH_ANIMCTRL_TREE_BLENDSPACE_1D, "it is a 1D space");

	// ★ THE CLIP CAME WITH IT. A conversion that started from scratch would have
	// silently deleted the one thing the state was already playing.
	ZENITH_ASSERT_EQ(xDoc.GetBlendPointCount("Locomotion"), 1u, "★ seeded with ONE point");
	std::string strClip;
	Zenith_Maths::Vector2 xPosition(9.0f, 9.0f);
	ZENITH_ASSERT_TRUE(xDoc.GetBlendPoint("Locomotion", 0, strClip, xPosition), "which reads back");
	ZENITH_ASSERT_EQ(strClip, std::string("WalkClip"), "★ playing the clip the leaf played");
	ZENITH_ASSERT_EQ_FLOAT(xPosition.x, 0.0f, 1e-6f, "at the origin");
	ZENITH_ASSERT_EQ_FLOAT(xPosition.y, 0.0f, 1e-6f,
		"★ and a 1D read answers y = 0 rather than leaving the caller's value in place");

	// ASSIGNMENT: the kind it already holds is satisfied and pushes nothing.
	ZENITH_ASSERT_TRUE(xDoc.SetStateTreeKind("Locomotion", ZENITH_ANIMCTRL_TREE_BLENDSPACE_1D),
		"asking for the kind in place is satisfied");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), uDepth + 1u, "and pushes nothing");

	xDoc.Undo();
	ZENITH_ASSERT_TRUE(xDoc.GetStateTreeKind("Locomotion") == ZENITH_ANIMCTRL_TREE_SINGLE_CLIP,
		"the undo puts the clip leaf back");
	std::string strRestored;
	ZENITH_ASSERT_TRUE(xDoc.GetStateClipName("Locomotion", strRestored), "which names a clip");
	ZENITH_ASSERT_EQ(strRestored, std::string("WalkClip"), "the original one");

	Zenith_Vector<char> axAfter;
	{
		const Flux_AnimationState* pxState = xDoc.GetSelectedMachineDef()->GetState("Locomotion");
		Zenith_DataStream xStream(1);
		pxState->WriteToDataStream(xStream);
		const char* pcBytes = static_cast<const char*>(xStream.GetData());
		for (u_int64 ul = 0; ul < xStream.GetCursor(); ++ul)
		{
			axAfter.PushBack(pcBytes[ul]);
		}
	}
	ZENITH_ASSERT_EQ(axAfter.GetSize(), axBefore.GetSize(), "the restored state serializes to the same LENGTH");
	bool bIdentical = (axAfter.GetSize() == axBefore.GetSize());
	for (u_int u = 0; bIdentical && u < axAfter.GetSize(); ++u)
	{
		bIdentical = (axAfter.Get(u) == axBefore.Get(u));
	}
	ZENITH_ASSERT_TRUE(bIdentical, "★ and to the same BYTES — the undo is exact, not approximate");

	xDoc.CloseDiscardingChanges();
}

ZENITH_TEST(AnimCtrlDoc, ABlendAxisBindsOnlyADeclaredFloatAndTheRefusalSaysWhy)
{
	// ★ THE RULE IS THE RUNTIME'S, NOT A PREFERENCE.
	// Flux_BlendTreeNode_BlendSpace1D::ResolveParameters reads its binding through
	// Flux_AnimationParameters::GetFloat, so an Int declaration would be read
	// through the wrong union member — and an UNDECLARED name is left at the
	// literal by the runtime, which is a binding that looks authored and does
	// nothing at all.
	AnimCtrlDocFixture xFixture("zenith_animctrldoc_bind");
	Zenith_AnimControllerDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.OpenFresh(xFixture.PathFor("bind.zanimctrl")) == ZENITH_ANIMCTRLDOC_OPEN_OK, "open fresh");
	ZENITH_ASSERT_TRUE(xDoc.AddParameter(Zenith_AnimCtrlParameterDecl::Float("Speed", 0.0f)), "a Float");
	ZENITH_ASSERT_TRUE(xDoc.AddParameter(Zenith_AnimCtrlParameterDecl::Int("Ammo", 3)), "and an Int");
	ZENITH_ASSERT_TRUE(xDoc.AddState("Locomotion"), "a state");
	ZENITH_ASSERT_TRUE(xDoc.SetStateTreeKind("Locomotion", ZENITH_ANIMCTRL_TREE_BLENDSPACE_1D), "as a 1D space");

	const u_int uDepth = xDoc.GetUndoStackSize();
	ZENITH_ASSERT_FALSE(xDoc.SetBlendSpaceParameter("Locomotion", ZENITH_ANIMCTRL_BLEND_AXIS_X, "Ammo"),
		"★ an INT parameter is refused");
	ZENITH_ASSERT_TRUE(xDoc.GetLastBlendTreeDiagnostic() == std::string(
		Zenith_AnimControllerDocument::BlendParameterRefusalText()), "with the ONE wording of the refusal");
	ZENITH_ASSERT_FALSE(xDoc.SetBlendSpaceParameter("Locomotion", ZENITH_ANIMCTRL_BLEND_AXIS_X, "NoSuchParam"),
		"★ and so is an UNDECLARED name — the runtime would leave it at the literal, silently");
	ZENITH_ASSERT_FALSE(xDoc.SetBlendSpaceParameter("Locomotion", ZENITH_ANIMCTRL_BLEND_AXIS_Y, "Speed"),
		"★ and a 1D space has no Y axis to bind");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), uDepth, "none of the three touched the stack");

	std::string strBound("unset");
	ZENITH_ASSERT_TRUE(xDoc.GetBlendSpaceParameterName("Locomotion", ZENITH_ANIMCTRL_BLEND_AXIS_X, strBound),
		"the X axis reads");
	ZENITH_ASSERT_TRUE(strBound.empty(), "★ and is still UNBOUND — a refusal changed nothing");
	ZENITH_ASSERT_FALSE(xDoc.GetBlendSpaceParameterName("Locomotion", ZENITH_ANIMCTRL_BLEND_AXIS_Y, strBound),
		"reading a 1D space's Y refuses rather than answering empty");

	ZENITH_ASSERT_TRUE(xDoc.SetBlendSpaceParameter("Locomotion", ZENITH_ANIMCTRL_BLEND_AXIS_X, "Speed"),
		"the declared Float binds");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), uDepth + 1u, "as ONE undo step");
	ZENITH_ASSERT_TRUE(xDoc.GetBlendSpaceParameterName("Locomotion", ZENITH_ANIMCTRL_BLEND_AXIS_X, strBound), "it reads");
	ZENITH_ASSERT_EQ(strBound, std::string("Speed"), "as the bound name");
	ZENITH_ASSERT_TRUE(xDoc.SetBlendSpaceParameter("Locomotion", ZENITH_ANIMCTRL_BLEND_AXIS_X, "Speed"),
		"re-stating it is satisfied");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), uDepth + 1u, "and pushes nothing");

	// UNBINDING is always allowed — a space on its authored literal is a
	// legitimate state (D48), and an empty name is the only way back to it.
	ZENITH_ASSERT_TRUE(xDoc.SetBlendSpaceParameter("Locomotion", ZENITH_ANIMCTRL_BLEND_AXIS_X, std::string()),
		"an empty name UNBINDS");
	ZENITH_ASSERT_TRUE(xDoc.GetBlendSpaceParameterName("Locomotion", ZENITH_ANIMCTRL_BLEND_AXIS_X, strBound), "it reads");
	ZENITH_ASSERT_TRUE(strBound.empty(), "back to unbound");

	// A 2D space binds each axis INDEPENDENTLY (D48): an X on "Speed" beside a Y
	// left on its literal is the common case.
	ZENITH_ASSERT_TRUE(xDoc.SetStateTreeKind("Locomotion", ZENITH_ANIMCTRL_TREE_BLENDSPACE_2D), "convert to 2D");
	ZENITH_ASSERT_TRUE(xDoc.SetBlendSpaceParameter("Locomotion", ZENITH_ANIMCTRL_BLEND_AXIS_Y, "Speed"),
		"★ a 2D space HAS a Y axis, and it takes the same Float");
	ZENITH_ASSERT_TRUE(xDoc.GetBlendSpaceParameterName("Locomotion", ZENITH_ANIMCTRL_BLEND_AXIS_X, strBound), "X reads");
	ZENITH_ASSERT_TRUE(strBound.empty(), "★ and X is untouched — the two axes are independent");

	xDoc.CloseDiscardingChanges();
}

ZENITH_TEST(AnimCtrlDoc, EveryBlendPointEditIsOneUndoStepAndAnAssignmentNoOpPushesNothing)
{
	AnimCtrlDocFixture xFixture("zenith_animctrldoc_points");
	Zenith_AnimControllerDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.OpenFresh(xFixture.PathFor("points.zanimctrl")) == ZENITH_ANIMCTRLDOC_OPEN_OK, "open fresh");
	ZENITH_ASSERT_TRUE(xDoc.AddState("Locomotion"), "a state");
	ZENITH_ASSERT_TRUE(xDoc.SetStateTreeKind("Locomotion", ZENITH_ANIMCTRL_TREE_BLENDSPACE_2D), "as a 2D space");

	u_int uDepth = xDoc.GetUndoStackSize();
	u_int uIndexA = 0xFFFFFFFFu;
	u_int uIndexB = 0xFFFFFFFFu;
	ZENITH_ASSERT_TRUE(xDoc.AddBlendPoint("Locomotion", "WalkClip", Zenith_Maths::Vector2(0.0f, 0.0f), &uIndexA),
		"a point");
	ZENITH_ASSERT_TRUE(xDoc.AddBlendPoint("Locomotion", "RunClip", Zenith_Maths::Vector2(1.0f, 2.0f), &uIndexB),
		"and another");
	ZENITH_ASSERT_EQ(uIndexA, 0u, "the first is index 0");
	ZENITH_ASSERT_EQ(uIndexB, 1u, "the second is index 1");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), uDepth + 2u, "two edits, two undo steps");

	std::string strClip;
	Zenith_Maths::Vector2 xPosition(0.0f);
	ZENITH_ASSERT_TRUE(xDoc.GetBlendPoint("Locomotion", 1, strClip, xPosition), "point 1 reads");
	ZENITH_ASSERT_EQ(strClip, std::string("RunClip"), "with its clip");
	ZENITH_ASSERT_EQ_FLOAT(xPosition.y, 2.0f, 1e-6f, "★ and a 2D space KEEPS its y");

	uDepth = xDoc.GetUndoStackSize();
	// ★ ASSIGNMENT NO-OPS, DETECTED BY BYTE EQUALITY rather than by each verb
	// growing its own field comparison.
	ZENITH_ASSERT_TRUE(xDoc.SetBlendPointClip("Locomotion", 1, "RunClip"), "the clip it already plays");
	ZENITH_ASSERT_TRUE(xDoc.SetBlendPointPosition("Locomotion", 1, Zenith_Maths::Vector2(1.0f, 2.0f)),
		"the position it already holds");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), uDepth, "★ and NEITHER pushed a step");

	// CREATION / REMOVAL refusals.
	ZENITH_ASSERT_FALSE(xDoc.AddBlendPoint("Locomotion", std::string(), Zenith_Maths::Vector2(0.0f, 0.0f)),
		"★ a nameless clip is refused — a leaf with no name poses the bind pose and says nothing");
	ZENITH_ASSERT_FALSE(xDoc.RemoveBlendPoint("Locomotion", 7u), "removing a point past the end is a refusal");
	ZENITH_ASSERT_FALSE(xDoc.SetBlendPointPosition("Locomotion", 7u, Zenith_Maths::Vector2(0.0f, 0.0f)),
		"and so is moving one");
	ZENITH_ASSERT_EQ(xDoc.GetUndoStackSize(), uDepth, "none of which touched the stack");

	// A state that is not a blend space refuses the whole family, so a caller
	// cannot half-edit one.
	ZENITH_ASSERT_TRUE(xDoc.AddState("Plain"), "a second state");
	ZENITH_ASSERT_TRUE(xDoc.SetStateClip("Plain", "IdleClip"), "with a clip leaf");
	ZENITH_ASSERT_EQ(xDoc.GetBlendPointCount("Plain"), 0u, "it has no points");
	ZENITH_ASSERT_FALSE(xDoc.AddBlendPoint("Plain", "WalkClip", Zenith_Maths::Vector2(0.0f, 0.0f)),
		"★ and a clip leaf takes no blend point");
	ZENITH_ASSERT_FALSE(xDoc.GetBlendPoint("Plain", 0, strClip, xPosition),
		"reading one refuses rather than answering an empty point");

	// The removal, and its undo.
	uDepth = xDoc.GetUndoStackSize();
	ZENITH_ASSERT_TRUE(xDoc.RemoveBlendPoint("Locomotion", 0u), "remove point 0");
	ZENITH_ASSERT_EQ(xDoc.GetBlendPointCount("Locomotion"), 1u, "one is left");
	ZENITH_ASSERT_TRUE(xDoc.GetBlendPoint("Locomotion", 0, strClip, xPosition), "and it renumbered");
	ZENITH_ASSERT_EQ(strClip, std::string("RunClip"), "to the survivor");
	xDoc.Undo();
	ZENITH_ASSERT_EQ(xDoc.GetBlendPointCount("Locomotion"), 2u, "★ the undo brings the point back");
	ZENITH_ASSERT_TRUE(xDoc.GetBlendPoint("Locomotion", 0, strClip, xPosition), "at index 0");
	ZENITH_ASSERT_EQ(strClip, std::string("WalkClip"), "★ playing the clip it played");

	xDoc.CloseDiscardingChanges();
}

ZENITH_TEST(AnimCtrlDoc, A1DPointMovedPastAnotherRenumbersAndTheDocumentSaysWhereItWent)
{
	// ★ THE ONE PROPERTY AN INDEX-ADDRESSED BLEND API CAN GET SILENTLY WRONG.
	// Flux_BlendTreeNode_BlendSpace1D::Evaluate brackets the parameter between
	// ADJACENT points, so the list is kept sorted — which means a drag past a
	// neighbour SWAPS two indices, and a caller that went on using the old one
	// would be editing the point it just dragged past.
	AnimCtrlDocFixture xFixture("zenith_animctrldoc_renumber");
	Zenith_AnimControllerDocument xDoc;
	ZENITH_ASSERT_TRUE(xDoc.OpenFresh(xFixture.PathFor("sorted.zanimctrl")) == ZENITH_ANIMCTRLDOC_OPEN_OK, "open fresh");
	ZENITH_ASSERT_TRUE(xDoc.AddState("Locomotion"), "a state");
	ZENITH_ASSERT_TRUE(xDoc.SetStateTreeKind("Locomotion", ZENITH_ANIMCTRL_TREE_BLENDSPACE_1D), "as a 1D space");
	ZENITH_ASSERT_TRUE(xDoc.AddBlendPoint("Locomotion", "WalkClip", Zenith_Maths::Vector2(0.0f, 0.0f)), "walk at 0");
	ZENITH_ASSERT_TRUE(xDoc.AddBlendPoint("Locomotion", "RunClip", Zenith_Maths::Vector2(4.0f, 0.0f)), "run at 4");

	std::string strClip;
	Zenith_Maths::Vector2 xPosition(0.0f);
	ZENITH_ASSERT_TRUE(xDoc.GetBlendPoint("Locomotion", 0, strClip, xPosition), "point 0 reads");
	ZENITH_ASSERT_EQ(strClip, std::string("WalkClip"), "and is the walk");

	u_int uNewIndex = 0xFFFFFFFFu;
	ZENITH_ASSERT_TRUE(xDoc.SetBlendPointPosition("Locomotion", 1, Zenith_Maths::Vector2(-3.0f, 0.0f), &uNewIndex),
		"drag the run BELOW the walk");
	ZENITH_ASSERT_EQ(uNewIndex, 0u, "★ and the document says the point is now index 0");
	ZENITH_ASSERT_TRUE(xDoc.GetBlendPoint("Locomotion", 0, strClip, xPosition), "index 0 reads");
	ZENITH_ASSERT_EQ(strClip, std::string("RunClip"), "★ which is the point that moved");
	ZENITH_ASSERT_EQ_FLOAT(xPosition.x, -3.0f, 1e-6f, "at the dropped position");
	ZENITH_ASSERT_TRUE(xDoc.GetBlendPoint("Locomotion", 1, strClip, xPosition), "index 1 reads");
	ZENITH_ASSERT_EQ(strClip, std::string("WalkClip"), "★ and is now the walk — the two swapped");

	// ★ ADDING A POINT SORTS TOO, so the reported index is the SORTED one rather
	// than "the end of the list". A recipe that assumed append order would be
	// naming a different point from its very next step.
	u_int uInserted = 0xFFFFFFFFu;
	ZENITH_ASSERT_TRUE(xDoc.AddBlendPoint("Locomotion", "IdleClip", Zenith_Maths::Vector2(-9.0f, 0.0f), &uInserted),
		"add one below both");
	ZENITH_ASSERT_EQ(uInserted, 0u, "★ and it reports index 0, not 2");
	ZENITH_ASSERT_TRUE(xDoc.GetBlendPoint("Locomotion", 0, strClip, xPosition), "index 0 reads");
	ZENITH_ASSERT_EQ(strClip, std::string("IdleClip"), "as the new point");

	xDoc.CloseDiscardingChanges();
}
