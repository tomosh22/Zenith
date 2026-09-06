#include "Core/Zenith_TestFramework.h"
#include "UnitTests/Zenith_AssertCapture.h"   // the refused-stream paths assert on purpose
#include "Flux/MeshAnimation/Flux_AnimatorControllerDef.h"
#include "AssetHandling/Zenith_AssetTypeIds.h"
#include "DataStream/Zenith_StreamEnvelope.h"

#include <cstring>   // std::memcpy — poking the envelope's schema word

//==============================================================================
// WU-6.2 — Flux_AnimatorControllerDef, the .zanimctrl payload.
//
// ★ WHAT THESE EXIST TO CATCH. A state-machine def alone cannot describe a
// controller: the thing every layered game actually runs is the LAYER LIST, and
// each layer owns its own machine, its blend mode, its emit-events flag and a
// mask reference. So every test here asserts on a def with BOTH halves — a
// top-level machine AND layers — because a def that only round-tripped the
// top-level machine would pass a single-machine test and lose the whole graph
// of a real character.
//
// Everything is in memory and pure CPU: no files, no registry, no graphics.
//==============================================================================

namespace
{
	// One state with a NAMED clip leaf. The name is what a def carries (clip
	// POINTERS are resolved against a live collection and are not authored data),
	// so this is exactly the shape a real def holds.
	void WU62_AddClipState(Flux_AnimationStateMachineDef& xDef, const char* szState, const char* szClipName, float fRate)
	{
		Flux_AnimationState* pxState = xDef.AddState(szState);
		Flux_BlendTreeNode_Clip* pxLeaf = new Flux_BlendTreeNode_Clip();
		pxLeaf->SetClipName(szClipName);
		pxLeaf->SetPlaybackRate(fRate);
		pxState->SetBlendTree(pxLeaf);
	}

	// Idle -> Walk on Speed > 0.5, the smallest def that carries a state, a
	// transition, a condition and a parameter declaration all at once.
	void WU62_AuthorLocomotionDef(Flux_AnimationStateMachineDef& xDef, const char* szName)
	{
		xDef.SetName(szName);
		xDef.GetParameterDeclarations().AddFloat("Speed", 0.25f);

		WU62_AddClipState(xDef, "Idle", "Idle", 1.0f);
		WU62_AddClipState(xDef, "Walk", "Walk", 1.5f);
		xDef.SetDefaultState("Idle");

		Flux_StateTransition xToWalk;
		xToWalk.m_strTargetStateName = "Walk";
		xToWalk.m_fTransitionDuration = 0.3f;
		xToWalk.m_iPriority = 7;

		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "Speed";
		xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Greater;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Float;
		xCond.m_fThreshold = 0.5f;
		xToWalk.m_xConditions.PushBack(xCond);

		xDef.GetState("Idle")->AddTransition(xToWalk);
	}

	// A def with a top-level machine, two clip paths and two masked layers — the
	// full shape the acceptance case round-trips.
	void WU62_AuthorControllerDef(Flux_AnimatorControllerDef& xDef)
	{
		xDef.SetName("Humanoid");
		xDef.AddClipPath("game:Anims/Idle.zanim");
		xDef.AddClipPath("game:Anims/Walk.zanim");

		WU62_AuthorLocomotionDef(xDef.GetOrCreateStateMachineDef(), "Base");

		Flux_AnimatorControllerLayerDef* pxLower = xDef.AddLayer("Lower");
		pxLower->SetWeight(0.75f);
		pxLower->SetBlendMode(LAYER_BLEND_OVERRIDE);
		pxLower->SetEmitEvents(true);
		pxLower->SetBoneMaskAssetPath("game:Masks/LowerBody.zanimmask");
		WU62_AuthorLocomotionDef(pxLower->GetStateMachineDef(), "LowerGraph");

		Flux_AnimatorControllerLayerDef* pxUpper = xDef.AddLayer("Upper");
		pxUpper->SetWeight(0.25f);
		pxUpper->SetBlendMode(LAYER_BLEND_ADDITIVE);
		pxUpper->SetEmitEvents(false);
		pxUpper->SetBoneMaskAssetPath("game:Masks/UpperBody.zanimmask");
		WU62_AuthorLocomotionDef(pxUpper->GetStateMachineDef(), "UpperGraph");
	}

	const char* WU62_ClipNameOfState(const Flux_AnimationStateMachineDef& xDef, const char* szState)
	{
		const Flux_AnimationState* pxState = xDef.GetState(szState);
		if (pxState == nullptr || pxState->GetBlendTree() == nullptr)
		{
			return "";
		}
		return static_cast<const Flux_BlendTreeNode_Clip*>(pxState->GetBlendTree())->GetClipName().c_str();
	}

	// Everything a def is supposed to carry, asserted in one place so the file
	// round trip and the CopyFrom deep copy are held to the SAME standard.
	// ★ THE MESSAGE IS A FORMAT STRING, NOT A VARIABLE. ZENITH_ASSERT_* splices
	// `"" __VA_ARGS__`, so the first vararg must be a string LITERAL — passing
	// szWhere directly does not compile. Hence "%s" everywhere below.
	void WU62_AssertMatchesAuthored(const Flux_AnimatorControllerDef& xDef, const char* szWhere)
	{
		ZENITH_ASSERT_TRUE(xDef.GetName() == "Humanoid", "def name, %s", szWhere);
		ZENITH_ASSERT_EQ(xDef.GetClipPaths().GetSize(), 2u, "clip count, %s", szWhere);
		if (xDef.GetClipPaths().GetSize() == 2u)
		{
			ZENITH_ASSERT_TRUE(xDef.GetClipPaths().Get(0) == "game:Anims/Idle.zanim", "clip 0, %s", szWhere);
			ZENITH_ASSERT_TRUE(xDef.GetClipPaths().Get(1) == "game:Anims/Walk.zanim", "clip 1, %s", szWhere);
		}

		ZENITH_ASSERT_TRUE(xDef.HasStateMachineDef(), "top-level machine present, %s", szWhere);
		if (xDef.HasStateMachineDef())
		{
			const Flux_AnimationStateMachineDef& xTop = *xDef.GetStateMachineDef();
			ZENITH_ASSERT_TRUE(xTop.GetName() == "Base", "top-level machine name, %s", szWhere);
			ZENITH_ASSERT_TRUE(xTop.HasState("Idle") && xTop.HasState("Walk"), "top-level states, %s", szWhere);
			ZENITH_ASSERT_TRUE(xTop.GetDefaultStateName() == "Idle", "top-level default state, %s", szWhere);
			ZENITH_ASSERT_TRUE(xTop.GetParameterDeclarations().HasParameter("Speed"), "parameter declared, %s", szWhere);
			ZENITH_ASSERT_EQ_FLOAT(xTop.GetParameterDeclarations().GetFloat("Speed"), 0.25f, 1e-5f,
				"parameter DEFAULT, %s", szWhere);
			ZENITH_ASSERT_STREQ(WU62_ClipNameOfState(xTop, "Walk"), "Walk", "clip leaf name, %s", szWhere);
		}

		ZENITH_ASSERT_EQ(xDef.GetLayerCount(), 2u, "layer count, %s", szWhere);
		if (xDef.GetLayerCount() != 2u)
		{
			return;
		}

		const Flux_AnimatorControllerLayerDef* pxLower = xDef.GetLayer(0);
		ZENITH_ASSERT_TRUE(pxLower->GetName() == "Lower", "layer 0 name, %s", szWhere);
		ZENITH_ASSERT_EQ(pxLower->GetLayerId(), 0u, "layer 0 id, %s", szWhere);
		ZENITH_ASSERT_EQ_FLOAT(pxLower->GetWeight(), 0.75f, 1e-5f, "layer 0 weight, %s", szWhere);
		ZENITH_ASSERT_TRUE(pxLower->GetBlendMode() == LAYER_BLEND_OVERRIDE, "layer 0 blend mode, %s", szWhere);
		ZENITH_ASSERT_TRUE(pxLower->GetEmitEvents(), "layer 0 emit flag, %s", szWhere);
		ZENITH_ASSERT_TRUE(pxLower->GetBoneMaskAssetPath() == "game:Masks/LowerBody.zanimmask",
			"layer 0 mask path, %s", szWhere);
		ZENITH_ASSERT_TRUE(pxLower->GetStateMachineDef().GetName() == "LowerGraph", "layer 0 machine name, %s", szWhere);
		ZENITH_ASSERT_TRUE(pxLower->GetStateMachineDef().HasState("Walk"), "layer 0 machine states, %s", szWhere);

		const Flux_AnimatorControllerLayerDef* pxUpper = xDef.GetLayer(1);
		ZENITH_ASSERT_TRUE(pxUpper->GetName() == "Upper", "layer 1 name, %s", szWhere);
		ZENITH_ASSERT_EQ(pxUpper->GetLayerId(), 1u, "layer 1 id, %s", szWhere);
		ZENITH_ASSERT_EQ_FLOAT(pxUpper->GetWeight(), 0.25f, 1e-5f, "layer 1 weight, %s", szWhere);
		ZENITH_ASSERT_TRUE(pxUpper->GetBlendMode() == LAYER_BLEND_ADDITIVE, "layer 1 blend mode, %s", szWhere);
		// ★ THE ONE FLAG A LAYER'S OWN SERIALIZER DOES NOT CARRY. m_bEmitEvents is
		// absent from Flux_AnimationLayer::WriteToDataStream (D41 changed no schema),
		// so a controller reloaded from a .zscen silently re-enables a silenced
		// overlay. The .zanimctrl is where it survives, and this is the assertion
		// that says so.
		ZENITH_ASSERT_FALSE(pxUpper->GetEmitEvents(), "layer 1 emit flag, %s", szWhere);
		ZENITH_ASSERT_TRUE(pxUpper->GetBoneMaskAssetPath() == "game:Masks/UpperBody.zanimmask",
			"layer 1 mask path, %s", szWhere);
		ZENITH_ASSERT_TRUE(pxUpper->GetStateMachineDef().GetName() == "UpperGraph", "layer 1 machine name, %s", szWhere);
	}
}

//==============================================================================
// (1) The whole def — top-level machine AND layers — round-trips, under an
//     envelope that identifies it as a .zanimctrl.
//==============================================================================
ZENITH_TEST(AnimatorControllerDef, DefRoundTripsUnderItsOwnEnvelope)
{
	Flux_AnimatorControllerDef xAuthored;
	WU62_AuthorControllerDef(xAuthored);

	Zenith_DataStream xStream;
	xAuthored.WriteToDataStream(xStream);

	// The envelope FIRST: a payload that round-trips inside the wrong header is a
	// file another loader will accept.
	xStream.SetCursor(0);
	Zenith_Result<Zenith_StreamHeader> xHeader = Zenith_ReadStreamHeader(xStream, uZENITH_ANIMCTRL_ASSET_TYPE_ID);
	ZENITH_ASSERT_TRUE(xHeader.IsOk(), "a controller def must lead with the shared stream envelope");
	if (xHeader.IsOk())
	{
		ZENITH_ASSERT_EQ(xHeader.Value().m_uAssetTypeId, uZENITH_ANIMCTRL_ASSET_TYPE_ID, ".zanimctrl envelope type id");
		ZENITH_ASSERT_EQ(xHeader.Value().m_uSchemaVersion, uZENITH_ANIMCTRL_SCHEMA_CURRENT, ".zanimctrl envelope schema");
	}

	xStream.SetCursor(0);
	Flux_AnimatorControllerDef xLoaded;
	ZENITH_ASSERT_TRUE(xLoaded.ParseStream(xStream).IsOk(), "ParseStream must accept its own output");
	WU62_AssertMatchesAuthored(xLoaded, "after a stream round trip");

	// The counter travels with the ids, so the next layer added after a load does
	// not collide with one already in the file.
	ZENITH_ASSERT_EQ(xLoaded.GetNextLayerId(), 2u, "the layer-id counter round-trips");
	ZENITH_ASSERT_EQ(xLoaded.AddLayer("Face")->GetLayerId(), 2u, "and the next id continues from it");
}

//==============================================================================
// (2) A refused stream is refused ONCE and leaves the def empty, not half-read.
//==============================================================================
ZENITH_TEST(AnimatorControllerDef, ARefusedStreamLeavesTheDefEmpty)
{
	// (a) No envelope at all — four bytes of nothing.
	{
		Zenith_DataStream xStream;
		const u_int uGarbage = 0xDEADBEEFu;
		xStream << uGarbage;
		xStream.SetCursor(0);

		Flux_AnimatorControllerDef xDef;
		WU62_AuthorControllerDef(xDef);
		{
			Zenith_AssertCaptureScope xCapture;
			ZENITH_ASSERT_FALSE(xDef.ParseStream(xStream).IsOk(), "a stream with no envelope is refused");
			ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "and asserts exactly once");
		}
		ZENITH_ASSERT_EQ(xDef.GetLayerCount(), 0u, "a refusal CLEARS the def rather than half-filling it");
		ZENITH_ASSERT_FALSE(xDef.HasStateMachineDef(), "including its top-level machine");
	}

	// (b) Another asset's type id. This is the case "one extension, one format"
	// exists to make impossible, and it must not be readable as a controller.
	{
		Zenith_DataStream xStream;
		Zenith_WriteStreamHeader(xStream, uZENITH_SKELETON_ASSET_TYPE_ID, uZENITH_SKELETON_SCHEMA_CURRENT);
		xStream.SetCursor(0);

		Flux_AnimatorControllerDef xDef;
		{
			Zenith_AssertCaptureScope xCapture;
			ZENITH_ASSERT_FALSE(xDef.ParseStream(xStream).IsOk(), "a .zskel envelope is not a .zanimctrl");
			ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "and asserts exactly once");
		}
	}

	// (c) A well-formed file from a newer tool.
	{
		Flux_AnimatorControllerDef xSource;
		WU62_AuthorControllerDef(xSource);
		Zenith_DataStream xStream;
		xSource.WriteToDataStream(xStream);

		// The schema word is the FOURTH u_int of Zenith_StreamHeader, written in
		// declaration order by Zenith_WriteStreamHeader.
		const u_int uFutureSchema = uZENITH_ANIMCTRL_SCHEMA_CURRENT + 1u;
		std::memcpy(static_cast<uint8_t*>(xStream.GetData()) + (3 * sizeof(u_int)), &uFutureSchema, sizeof(u_int));
		xStream.SetCursor(0);

		Flux_AnimatorControllerDef xDef;
		{
			Zenith_AssertCaptureScope xCapture;
			ZENITH_ASSERT_FALSE(xDef.ParseStream(xStream).IsOk(), "a future schema is refused, not parsed as current");
			ZENITH_ASSERT_EQ(xCapture.GetHitCount(), 1u, "and asserts exactly once");
		}
		ZENITH_ASSERT_EQ(xDef.GetLayerCount(), 0u, "and leaves nothing behind");
	}
}

//==============================================================================
// (3) Layer ids are an IDENTITY, not a position.
//==============================================================================
ZENITH_TEST(AnimatorControllerDef, LayerIdsAreStableAndAreNeverReused)
{
	Flux_AnimatorControllerDef xDef;
	ZENITH_ASSERT_EQ(xDef.AddLayer("A")->GetLayerId(), 0u, "ids start at 0");
	ZENITH_ASSERT_EQ(xDef.AddLayer("B")->GetLayerId(), 1u, "and are monotonic");
	ZENITH_ASSERT_EQ(xDef.AddLayer("C")->GetLayerId(), 2u, "and are monotonic");

	// ★ REMOVING THE MIDDLE LAYER MOVES C'S INDEX AND MUST NOT MOVE ITS ID. This is
	// the whole reason the id exists: anything holding "layer 2" by INDEX now names
	// C where it used to name B, and nothing anywhere reports that.
	xDef.RemoveLayer(1u);
	ZENITH_ASSERT_EQ(xDef.GetLayerCount(), 2u, "one layer went");
	ZENITH_ASSERT_EQ(xDef.GetLayer(1u)->GetLayerId(), 2u, "C keeps id 2 although it is now at index 1");
	ZENITH_ASSERT_TRUE(xDef.FindLayerById(2u) == xDef.GetLayer(1u), "and is found by that id");
	ZENITH_ASSERT_NULL(xDef.FindLayerById(1u), "the removed layer's id resolves to nothing");

	// The counter does NOT rewind: reusing B's id would silently re-point anything
	// that still held it at a different layer.
	ZENITH_ASSERT_EQ(xDef.AddLayer("D")->GetLayerId(), 3u, "a removed id is never handed out again");

	// AssignLayerId (what an export uses) keeps the counter monotonic past an id
	// chosen elsewhere; a bare SetLayerId would leave the counter at 4 and let the
	// next AddLayer mint an id that is already live once these are renumbered.
	Flux_AnimatorControllerLayerDef* pxD = xDef.GetLayer(2u);
	xDef.AssignLayerId(*pxD, 40u);
	ZENITH_ASSERT_EQ(pxD->GetLayerId(), 40u, "the chosen id is applied");
	ZENITH_ASSERT_EQ(xDef.AddLayer("E")->GetLayerId(), 41u, "and the counter moved past it");
}

//==============================================================================
// (4) CopyFrom is a DEEP copy, layers and nested blend trees included.
//==============================================================================
ZENITH_TEST(AnimatorControllerDef, CopyFromIsADeepCopy)
{
	Flux_AnimatorControllerDef xSource;
	WU62_AuthorControllerDef(xSource);

	Flux_AnimatorControllerDef xCopy;
	xCopy.CopyFrom(xSource);
	WU62_AssertMatchesAuthored(xCopy, "after CopyFrom");

	// Mutate the SOURCE afterwards. A shallow copy would share the layer objects
	// (and, through them, the blend trees), so every one of these would be visible
	// in the copy — and a double free would follow at destruction.
	xSource.GetLayer(0)->SetWeight(0.01f);
	xSource.GetLayer(0)->GetStateMachineDef().RemoveState("Walk");
	xSource.GetOrCreateStateMachineDef().SetName("Mutated");
	xSource.AddClipPath("game:Anims/Run.zanim");

	ZENITH_ASSERT_EQ_FLOAT(xCopy.GetLayer(0)->GetWeight(), 0.75f, 1e-5f, "the copy's layer weight is its own");
	ZENITH_ASSERT_TRUE(xCopy.GetLayer(0)->GetStateMachineDef().HasState("Walk"),
		"the copy's nested state machine is its own");
	ZENITH_ASSERT_TRUE(xCopy.GetStateMachineDef()->GetName() == "Base", "the copy's top-level machine is its own");
	ZENITH_ASSERT_EQ(xCopy.GetClipPaths().GetSize(), 2u, "and so is its clip list");
}
