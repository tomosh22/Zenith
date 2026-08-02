#include "Zenith.h"

// ============================================================================
// ZM_Tests_Interactable -- S6 item 3 SC4 unit tests for the live interaction
// wiring's HEADLESS half: the ZM_Interactable component's configuration /
// candidacy / serialization contract, the pure ZM_RaiseKindForRole map that
// decides WHICH ZM_UI_MenuStack seam a role talks through, and the
// ZM_InteractionRuntime latches the windowed tests (SC5-SC7) poll.
//
// Nothing here raises a screen, loads a scene or issues GPU work: the component is
// constructed against an INVALID entity handle (it stores the handle by value and
// never dereferences it for any of the surface tested here), the role -> seam map
// is a pure switch, and the runtime's latch surface is exercised through its own
// documented reset seam. So no RequestSkip is needed. Category ZM_Interaction --
// the same category as the SC1/SC2 logic units, since this is the same feature.
//
// The latch units matter more than they look: ZM_InteractionRuntime's latches are
// process-GLOBAL (the between-tests hook in Zenithmon.cpp can only reach ownerless
// state), so the reset unit deliberately POPULATES a latch first and only then
// resets -- a reset unit whose fixture was never populated passes vacuously and
// would keep passing after the reset it exists to police stopped working.
// ============================================================================

#include <cstring>   // strcmp (raise-kind name distinctness)
#include <limits>    // quiet_NaN (the radius sanitiser's fixture)

#include "Core/Multithreading/Zenith_Multithreading.h"
#include "Core/Zenith_Engine.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "Core/Zenith_TestFramework.h"
#include "DataStream/Zenith_DataStream.h"
#include "Flux/Primitives/Flux_PrimitivesImpl.h"
#include "Maths/Zenith_Maths.h"
#include "ZenithECS/Zenith_ComponentMeta.h"   // the registry the SC7 gate's NPCs deserialize through
#include "ZenithECS/Zenith_Entity.h"
#include "Zenithmon/Components/ZM_Interactable.h"
#include "Zenithmon/Source/Data/ZM_NpcData.h"
#include "Zenithmon/Source/Data/ZM_TrainerData.h"                 // ZM_TRAINER_* (the S7 SC6 sight units)
#include "Zenithmon/Source/Interaction/ZM_InteractionRuntime.h"
#include "Zenithmon/Source/Interaction/ZM_TrainerSightFsm.h"      // ZM_TRAINER_SIGHT_* (the observed state)

namespace
{
	constexpr float fTEST_EPSILON = 0.0001f;

	// An interactable built against an INVALID entity handle. Every member function
	// exercised below reads only the component's own PODs, so no scene is required.
	// (Interact() is the one member that reaches outside; it is covered by the
	// headless automated test ZM_NpcDispatch_Test, which has a real scene.)
	struct DetachedInteractable
	{
		Zenith_Entity   m_xEntity;
		ZM_Interactable m_xInteractable;

		DetachedInteractable() : m_xEntity(), m_xInteractable(m_xEntity) {}
	};

	// ---- Two SC7 fixtures, deliberately NOT one ----------------------------
	//
	// BEATS vs merely-PLACED. The consolidated gate (ZM_S6InteractGate_Test) proves
	// one BEAT per dispatch role -- talk / buy / heal -- and each beat presses E at
	// ONE specific authored NPC. Role coverage is asserted over THAT table only.
	//
	// The wider placed table must never be allowed to launder beat coverage, and
	// since S7 SC1 it demonstrably would: it carries the warden, a SECOND TALKER, so
	// "every role has a placed carrier" stays true after ZM_NPC_VILLAGER is re-rolled
	// to SHOPKEEP -- while the gate's talk beat silently starts raising a mart. That
	// is the exact hole splitting these two tables closes.

	// The gate's three beats. m_eExpectedKind is the seam that beat exists to press,
	// spelled HERE rather than read back off the row, so a re-rolled row disagrees
	// with the test instead of quietly agreeing with itself.
	struct GateBeat
	{
		ZM_NPC_ID         m_eNpc;
		const char*       m_szEntityName;    // the entity name Zenithmon.cpp authors
		const char*       m_szBeat;
		ZM_NPC_RAISE_KIND m_eExpectedKind;
	};

	constexpr u_int uGATE_BEAT_COUNT = 3u;
	const GateBeat axGATE_BEATS[uGATE_BEAT_COUNT] = {
		{ ZM_NPC_VILLAGER,         "Npc_Villager",       "talk", ZM_NPC_RAISE_DIALOGUE    },
		{ ZM_NPC_TRADE_POST_CLERK, "Npc_TradePostClerk", "buy",  ZM_NPC_RAISE_SHOP        },
		{ ZM_NPC_CARETAKER,        "Npc_Caretaker",      "heal", ZM_NPC_RAISE_CARE_CENTER },
	};

	// Every STATIONARY NPC Zenithmon.cpp stands in Dawnmere -- a SUPERSET of the beat
	// table (the warden is placed and walkable but carries no beat of his own yet).
	// ZM_NPC_WANDERER is excluded even though SC8 DOES author it
	// (ZM_QueueDawnmereWanderer places "Npc_Wanderer"): it MOVES, and everything here
	// is approached at a FIXED authored position, so reaching it takes the chase
	// machinery SC8's own walk-up test carries instead.
	//
	// S7 item 3 SC8 adds the rival. He IS a stationary placed NPC. This table is a
	// PURE DATA WALK (it asserts !m_bWanders and that every gate beat has a carrier)
	// -- it never approaches anything -- so adding him keeps the table's stated
	// meaning true.
	constexpr u_int uPLACED_NPC_COUNT = 5u;
	const ZM_NPC_ID aePLACED_NPCS[uPLACED_NPC_COUNT] = {
		ZM_NPC_VILLAGER,           // "Npc_Villager"       -- the gate's talk beat
		ZM_NPC_TRADE_POST_CLERK,   // "Npc_TradePostClerk" -- the gate's buy beat
		ZM_NPC_CARETAKER,          // "Npc_Caretaker"      -- the gate's heal beat
		ZM_NPC_ROUTE_WARDEN,       // "Npc_Warden"         -- S7 SC1's flag-gated talker
		ZM_NPC_RIVAL_VESPER,       // "Npc_RivalVesper"    -- S7 item 3 SC8's trainer
	};

	// The registered NAME of the interaction component, spelled exactly as
	// Zenithmon.cpp's ZENITH_REGISTER_COMPONENT line spells it.
	constexpr const char* szINTERACTABLE_META_NAME = "ZM_Interactable";
}

// ---- Component configuration ------------------------------------------------

// An entity that carries the component but was never configured must be inert: if
// it were a candidate it would absorb the interact press and leave the player
// standing in front of something with nothing to say.
ZENITH_TEST(ZM_Interaction, Interactable_DefaultsAreSafe)
{
	DetachedInteractable xFixture;
	ZENITH_ASSERT_EQ((u_int)xFixture.m_xInteractable.GetNpcId(), (u_int)ZM_NPC_NONE,
		"a default-constructed interactable names no NPC row");
	ZENITH_ASSERT_FALSE(xFixture.m_xInteractable.IsInteractable(),
		"a default-constructed interactable must not be a candidate");
	ZENITH_ASSERT_EQ_FLOAT(xFixture.m_xInteractable.GetRadius(),
		ZM_Interactable::fDEFAULT_RADIUS, fTEST_EPSILON,
		"the default reach BONUS is zero -- exactly the global reach, never more");
}

// W3's marker is deliberately an asset-free Flux GAMEPLAY-primitive payload. This
// unit runs before the main loop, so the renderer cannot drain the CPU queues
// between the synchronous Add calls and inspection. It disables the debug channel
// while submitting, proves only the always-rendered queues grew, and cleans its own
// tail before asserting even when the payload is wrong.
ZENITH_TEST(ZM_Interaction, Interactable_SpottedIndicatorSubmitsOneReadableExclamationMark)
{
	Flux_PrimitivesImpl& xPrimitives = g_xEngine.Primitives();
	u_int uDebugSphereBefore = 0u;
	u_int uDebugCubeBefore = 0u;
	u_int uDebugLineBefore = 0u;
	u_int uDebugCapsuleBefore = 0u;
	u_int uDebugCylinderBefore = 0u;
	u_int uDebugTriangleBefore = 0u;
	u_int uGameplaySphereBefore = 0u;
	u_int uGameplayCylinderBefore = 0u;
	{
		Zenith_ScopedMutexLock xLock(xPrimitives.m_xInstanceMutex);
		uDebugSphereBefore = xPrimitives.m_xSphereInstances.GetSize();
		uDebugCubeBefore = xPrimitives.m_xCubeInstances.GetSize();
		uDebugLineBefore = xPrimitives.m_xLineInstances.GetSize();
		uDebugCapsuleBefore = xPrimitives.m_xCapsuleInstances.GetSize();
		uDebugCylinderBefore = xPrimitives.m_xCylinderInstances.GetSize();
		uDebugTriangleBefore = xPrimitives.m_xTriangleInstances.GetSize();
		uGameplaySphereBefore = xPrimitives.m_xGameplaySphereInstances.GetSize();
		uGameplayCylinderBefore = xPrimitives.m_xGameplayCylinderInstances.GetSize();
	}

	// A distinctive centre makes every expected coordinate below an independent
	// literal: the body half-height is compiled (0.9), so the model top is
	// -1.0 + 0.9 = -0.1 and each figure is that plus one authored offset. The tools
	// option is deliberately OFF: a gameplay cue must not ride that channel.
	Zenith_GraphicsOptions& xGraphicsOptions = Zenith_GraphicsOptions::Get();
	const bool bDebugPrimitivesEnabledBefore = xGraphicsOptions.m_bPrimitivesEnabled;
	xGraphicsOptions.m_bPrimitivesEnabled = false;
	const u_int uSubmitted = ZM_Interactable::SubmitTrainerSpottedIndicator(
		Zenith_Maths::Vector3(2.5f, -1.0f, 4.25f));
	xGraphicsOptions.m_bPrimitivesEnabled = bDebugPrimitivesEnabledBefore;

	u_int uDebugSphereAfter = 0u;
	u_int uDebugCubeAfter = 0u;
	u_int uDebugLineAfter = 0u;
	u_int uDebugCapsuleAfter = 0u;
	u_int uDebugCylinderAfter = 0u;
	u_int uDebugTriangleAfter = 0u;
	u_int uGameplaySphereAfter = 0u;
	u_int uGameplayCylinderAfter = 0u;
	u_int uDebugSphereRestored = 0u;
	u_int uDebugCubeRestored = 0u;
	u_int uDebugLineRestored = 0u;
	u_int uDebugCapsuleRestored = 0u;
	u_int uDebugCylinderRestored = 0u;
	u_int uDebugTriangleRestored = 0u;
	u_int uGameplaySphereRestored = 0u;
	u_int uGameplayCylinderRestored = 0u;
	bool bHaveGameplaySphere = false;
	bool bHaveGameplayCylinder = false;
	Flux_PrimitivesSphereInstance xSphere{};
	Flux_PrimitivesCylinderInstance xCylinder{};
	{
		Zenith_ScopedMutexLock xLock(xPrimitives.m_xInstanceMutex);
		uDebugSphereAfter = xPrimitives.m_xSphereInstances.GetSize();
		uDebugCubeAfter = xPrimitives.m_xCubeInstances.GetSize();
		uDebugLineAfter = xPrimitives.m_xLineInstances.GetSize();
		uDebugCapsuleAfter = xPrimitives.m_xCapsuleInstances.GetSize();
		uDebugCylinderAfter = xPrimitives.m_xCylinderInstances.GetSize();
		uDebugTriangleAfter = xPrimitives.m_xTriangleInstances.GetSize();
		uGameplaySphereAfter = xPrimitives.m_xGameplaySphereInstances.GetSize();
		uGameplayCylinderAfter = xPrimitives.m_xGameplayCylinderInstances.GetSize();
		if (uGameplaySphereAfter > uGameplaySphereBefore)
		{
			xSphere = xPrimitives.m_xGameplaySphereInstances.Get(uGameplaySphereBefore);
			bHaveGameplaySphere = true;
		}
		if (uGameplayCylinderAfter > uGameplayCylinderBefore)
		{
			xCylinder = xPrimitives.m_xGameplayCylinderInstances.Get(
				uGameplayCylinderBefore);
			bHaveGameplayCylinder = true;
		}

		while (xPrimitives.m_xSphereInstances.GetSize() > uDebugSphereBefore)
		{
			xPrimitives.m_xSphereInstances.PopBack();
		}
		while (xPrimitives.m_xCubeInstances.GetSize() > uDebugCubeBefore)
		{
			xPrimitives.m_xCubeInstances.PopBack();
		}
		while (xPrimitives.m_xLineInstances.GetSize() > uDebugLineBefore)
		{
			xPrimitives.m_xLineInstances.PopBack();
		}
		while (xPrimitives.m_xCapsuleInstances.GetSize() > uDebugCapsuleBefore)
		{
			xPrimitives.m_xCapsuleInstances.PopBack();
		}
		while (xPrimitives.m_xCylinderInstances.GetSize() > uDebugCylinderBefore)
		{
			xPrimitives.m_xCylinderInstances.PopBack();
		}
		while (xPrimitives.m_xTriangleInstances.GetSize() > uDebugTriangleBefore)
		{
			xPrimitives.m_xTriangleInstances.PopBack();
		}
		while (xPrimitives.m_xGameplaySphereInstances.GetSize()
			> uGameplaySphereBefore)
		{
			xPrimitives.m_xGameplaySphereInstances.PopBack();
		}
		while (xPrimitives.m_xGameplayCylinderInstances.GetSize()
			> uGameplayCylinderBefore)
		{
			xPrimitives.m_xGameplayCylinderInstances.PopBack();
		}

		uDebugSphereRestored = xPrimitives.m_xSphereInstances.GetSize();
		uDebugCubeRestored = xPrimitives.m_xCubeInstances.GetSize();
		uDebugLineRestored = xPrimitives.m_xLineInstances.GetSize();
		uDebugCapsuleRestored = xPrimitives.m_xCapsuleInstances.GetSize();
		uDebugCylinderRestored = xPrimitives.m_xCylinderInstances.GetSize();
		uDebugTriangleRestored = xPrimitives.m_xTriangleInstances.GetSize();
		uGameplaySphereRestored = xPrimitives.m_xGameplaySphereInstances.GetSize();
		uGameplayCylinderRestored = xPrimitives.m_xGameplayCylinderInstances.GetSize();
	}

	// ★ THE RETURN VALUE IS THE LIVE CONTRACT. The component's per-frame counter is
	// fed from it, so if the helper could report a submission it did not make, every
	// automated assertion downstream would be watching a proxy instead of Flux.
	ZENITH_ASSERT_EQ(uSubmitted, 1u,
		"a marker that reached both gameplay primitive queues must report exactly one "
		"submission");
	ZENITH_ASSERT_EQ(uGameplayCylinderAfter, uGameplayCylinderBefore + 1u,
		"one marker must append exactly one always-rendered gameplay cylinder");
	ZENITH_ASSERT_EQ(uGameplaySphereAfter, uGameplaySphereBefore + 1u,
		"one marker must append exactly one always-rendered gameplay sphere");
	ZENITH_ASSERT_EQ(uDebugSphereAfter, uDebugSphereBefore,
		"the marker must not append a debug sphere");
	ZENITH_ASSERT_EQ(uDebugCubeAfter, uDebugCubeBefore,
		"the marker must not append a debug cube");
	ZENITH_ASSERT_EQ(uDebugLineAfter, uDebugLineBefore,
		"the marker must not append a debug line");
	ZENITH_ASSERT_EQ(uDebugCapsuleAfter, uDebugCapsuleBefore,
		"the marker must not append a debug capsule");
	ZENITH_ASSERT_EQ(uDebugCylinderAfter, uDebugCylinderBefore,
		"the marker must not append a debug cylinder");
	ZENITH_ASSERT_EQ(uDebugTriangleAfter, uDebugTriangleBefore,
		"the marker must not append a debug triangle");
	ZENITH_ASSERT_TRUE(bHaveGameplayCylinder,
		"the appended gameplay cylinder payload must be inspectable");
	ZENITH_ASSERT_TRUE(bHaveGameplaySphere,
		"the appended gameplay sphere payload must be inspectable");

	if (bHaveGameplayCylinder)
	{
		ZENITH_ASSERT_EQ_FLOAT(xCylinder.m_xStart.x, 2.5f, fTEST_EPSILON);
		ZENITH_ASSERT_EQ_FLOAT(xCylinder.m_xStart.y, 0.45f, fTEST_EPSILON);
		ZENITH_ASSERT_EQ_FLOAT(xCylinder.m_xStart.z, 4.25f, fTEST_EPSILON);
		ZENITH_ASSERT_EQ_FLOAT(xCylinder.m_xEnd.x, 2.5f, fTEST_EPSILON);
		ZENITH_ASSERT_EQ_FLOAT(xCylinder.m_xEnd.y, 1.10f, fTEST_EPSILON);
		ZENITH_ASSERT_EQ_FLOAT(xCylinder.m_xEnd.z, 4.25f, fTEST_EPSILON);
		ZENITH_ASSERT_EQ_FLOAT(xCylinder.m_fRadius, 0.10f, fTEST_EPSILON);
		ZENITH_ASSERT_EQ_FLOAT(xCylinder.m_xColor.x, 1.0f, fTEST_EPSILON);
		ZENITH_ASSERT_EQ_FLOAT(xCylinder.m_xColor.y, 0.82f, fTEST_EPSILON);
		ZENITH_ASSERT_EQ_FLOAT(xCylinder.m_xColor.z, 0.08f, fTEST_EPSILON);
		ZENITH_ASSERT_GT(xCylinder.m_xEnd.y, xCylinder.m_xStart.y,
			"the exclamation stem must be vertical and point upward");
	}
	if (bHaveGameplaySphere)
	{
		ZENITH_ASSERT_EQ_FLOAT(xSphere.m_xCenter.x, 2.5f, fTEST_EPSILON);
		ZENITH_ASSERT_EQ_FLOAT(xSphere.m_xCenter.y, 0.15f, fTEST_EPSILON);
		ZENITH_ASSERT_EQ_FLOAT(xSphere.m_xCenter.z, 4.25f, fTEST_EPSILON);
		ZENITH_ASSERT_EQ_FLOAT(xSphere.m_fRadius, 0.13f, fTEST_EPSILON);
		ZENITH_ASSERT_EQ_FLOAT(xSphere.m_xColor.x, 1.0f, fTEST_EPSILON);
		ZENITH_ASSERT_EQ_FLOAT(xSphere.m_xColor.y, 0.82f, fTEST_EPSILON);
		ZENITH_ASSERT_EQ_FLOAT(xSphere.m_xColor.z, 0.08f, fTEST_EPSILON);
		ZENITH_ASSERT_GT(xSphere.m_xCenter.y - xSphere.m_fRadius, -0.10f,
			"the full dot must sit strictly above the body's top (-0.10)");
		if (bHaveGameplayCylinder)
		{
			ZENITH_ASSERT_LT(xSphere.m_xCenter.y + xSphere.m_fRadius,
				xCylinder.m_xStart.y,
				"the dot and stem need readable separation");
		}
	}

	ZENITH_ASSERT_EQ(uDebugSphereRestored, uDebugSphereBefore);
	ZENITH_ASSERT_EQ(uDebugCubeRestored, uDebugCubeBefore);
	ZENITH_ASSERT_EQ(uDebugLineRestored, uDebugLineBefore);
	ZENITH_ASSERT_EQ(uDebugCapsuleRestored, uDebugCapsuleBefore);
	ZENITH_ASSERT_EQ(uDebugCylinderRestored, uDebugCylinderBefore);
	ZENITH_ASSERT_EQ(uDebugTriangleRestored, uDebugTriangleBefore);
	ZENITH_ASSERT_EQ(uGameplaySphereRestored, uGameplaySphereBefore);
	ZENITH_ASSERT_EQ(uGameplayCylinderRestored, uGameplayCylinderBefore);

	// ---- The refused arm: a non-finite CENTRE submits nothing and reports zero ---
	// The live path cannot reach this (the pure sight predicate fails closed on a
	// non-finite position first), but the helper is public and static, so its own
	// refusal is pinned here rather than left to the caller.
	u_int uRefusedCylinderAfter = 0u;
	u_int uRefusedSphereAfter = 0u;
	const float fNaN = std::numeric_limits<float>::quiet_NaN();
	const u_int uRefused = ZM_Interactable::SubmitTrainerSpottedIndicator(
		Zenith_Maths::Vector3(2.5f, fNaN, 4.25f));
	{
		Zenith_ScopedMutexLock xLock(xPrimitives.m_xInstanceMutex);
		uRefusedCylinderAfter =
			xPrimitives.m_xGameplayCylinderInstances.GetSize();
		uRefusedSphereAfter = xPrimitives.m_xGameplaySphereInstances.GetSize();
		while (xPrimitives.m_xGameplayCylinderInstances.GetSize()
			> uGameplayCylinderBefore)
		{
			xPrimitives.m_xGameplayCylinderInstances.PopBack();
		}
		while (xPrimitives.m_xGameplaySphereInstances.GetSize()
			> uGameplaySphereBefore)
		{
			xPrimitives.m_xGameplaySphereInstances.PopBack();
		}
	}
	ZENITH_ASSERT_EQ(uRefused, 0u,
		"a non-finite marker centre must report ZERO submissions");
	ZENITH_ASSERT_EQ(uRefusedCylinderAfter, uGameplayCylinderBefore,
		"a refused marker must not append a gameplay cylinder");
	ZENITH_ASSERT_EQ(uRefusedSphereAfter, uGameplaySphereBefore,
		"a refused marker must not append a sphere");
}

ZENITH_TEST(ZM_Interaction, Interactable_SetGetRoundTrip)
{
	DetachedInteractable xFixture;

	ZENITH_ASSERT_TRUE(xFixture.m_xInteractable.SetNpcId(ZM_NPC_TRADE_POST_CLERK));
	ZENITH_ASSERT_EQ((u_int)xFixture.m_xInteractable.GetNpcId(),
		(u_int)ZM_NPC_TRADE_POST_CLERK);

	ZENITH_ASSERT_TRUE(xFixture.m_xInteractable.SetRadius(1.25f));
	ZENITH_ASSERT_EQ_FLOAT(xFixture.m_xInteractable.GetRadius(), 1.25f, fTEST_EPSILON);

	xFixture.m_xInteractable.SetInteractable(true);
	ZENITH_ASSERT_TRUE(xFixture.m_xInteractable.IsInteractable(),
		"a configured row plus the authored flag makes a live candidate");
	xFixture.m_xInteractable.SetInteractable(false);
	ZENITH_ASSERT_FALSE(xFixture.m_xInteractable.IsInteractable());
}

// Fail CLOSED: a bad id clears the row rather than keeping the previous one, so a
// mis-authored value can only ever produce an INERT NPC, never a wrong conversation.
ZENITH_TEST(ZM_Interaction, Interactable_RejectsOutOfRangeNpcId)
{
	DetachedInteractable xFixture;
	ZENITH_ASSERT_TRUE(xFixture.m_xInteractable.SetNpcId(ZM_NPC_VILLAGER));
	xFixture.m_xInteractable.SetInteractable(true);
	ZENITH_ASSERT_TRUE(xFixture.m_xInteractable.IsInteractable());

	ZENITH_ASSERT_FALSE(xFixture.m_xInteractable.SetNpcId((ZM_NPC_ID)ZM_NPC_COUNT),
		"ZM_NPC_COUNT is not an NPC");
	ZENITH_ASSERT_EQ((u_int)xFixture.m_xInteractable.GetNpcId(), (u_int)ZM_NPC_NONE,
		"a rejected id must not leave the PREVIOUS row installed");
	ZENITH_ASSERT_FALSE(xFixture.m_xInteractable.IsInteractable(),
		"clearing the row also drops candidacy");

	ZENITH_ASSERT_FALSE(xFixture.m_xInteractable.SetNpcId((ZM_NPC_ID)9999u));
	ZENITH_ASSERT_EQ((u_int)xFixture.m_xInteractable.GetNpcId(), (u_int)ZM_NPC_NONE);
}

// A runaway radius would let one NPC swallow every interact press in the town, and
// a NaN one would poison the picker's distance comparison, so the setter clamps and
// sanitises rather than trusting its caller.
ZENITH_TEST(ZM_Interaction, Interactable_RadiusClampsAndSanitises)
{
	DetachedInteractable xFixture;

	ZENITH_ASSERT_FALSE(xFixture.m_xInteractable.SetRadius(-1.0f));
	ZENITH_ASSERT_EQ_FLOAT(xFixture.m_xInteractable.GetRadius(), 0.0f, fTEST_EPSILON,
		"a negative reach bonus clamps to zero, never shrinks reach below it");

	ZENITH_ASSERT_FALSE(xFixture.m_xInteractable.SetRadius(
		ZM_Interactable::fMAX_RADIUS + 100.0f));
	ZENITH_ASSERT_EQ_FLOAT(xFixture.m_xInteractable.GetRadius(),
		ZM_Interactable::fMAX_RADIUS, fTEST_EPSILON);

	ZENITH_ASSERT_TRUE(xFixture.m_xInteractable.SetRadius(0.5f));
	ZENITH_ASSERT_FALSE(xFixture.m_xInteractable.SetRadius(
		std::numeric_limits<float>::quiet_NaN()));
	ZENITH_ASSERT_EQ_FLOAT(xFixture.m_xInteractable.GetRadius(),
		ZM_Interactable::fDEFAULT_RADIUS, fTEST_EPSILON,
		"a non-finite radius resets to the default rather than propagating a NaN");
}

// The candidacy answer is the CONJUNCTION of the authored flag and a real row --
// flipping the flag on an unconfigured component must not make it a candidate.
ZENITH_TEST(ZM_Interaction, Interactable_InteractableRequiresAConfiguredRow)
{
	DetachedInteractable xFixture;
	xFixture.m_xInteractable.SetInteractable(true);
	ZENITH_ASSERT_FALSE(xFixture.m_xInteractable.IsInteractable(),
		"the flag alone, with no NPC row, is not enough to absorb the interact press");

	ZENITH_ASSERT_TRUE(xFixture.m_xInteractable.SetNpcId(ZM_NPC_CARETAKER));
	ZENITH_ASSERT_TRUE(xFixture.m_xInteractable.IsInteractable(),
		"configuring the row makes the already-set flag effective");
}

// OnStart re-validates whatever authoring / deserialization left behind.
// OnStart's row guard is REACHABLE, but not by the route its old name implied:
// ZM_NPC_NONE == ZM_NPC_COUNT, so `m_eNpcId >= ZM_NPC_COUNT` is true for any
// UNCONFIGURED component (and for one whose id SetNpcId refused). What the guard
// actually enforces is that such a row cannot be left live, which is what stops an
// unconfigured NPC entity from silently absorbing the player's interact press.
ZENITH_TEST(ZM_Interaction, Interactable_OnStartClearsAnUnconfiguredRow)
{
	DetachedInteractable xFixture;
	// Deliberately NOT SetNpcId: the row stays NONE, which is what an entity gets
	// when authoring forgets to assign one.
	xFixture.m_xInteractable.SetInteractable(true);
	xFixture.m_xInteractable.OnStart();

	ZENITH_ASSERT_FALSE(xFixture.m_xInteractable.IsInteractable(),
		"an unconfigured row must NOT survive OnStart as a live candidate");
	ZENITH_ASSERT_EQ((u_int)xFixture.m_xInteractable.GetNpcId(), (u_int)ZM_NPC_NONE);
}

ZENITH_TEST(ZM_Interaction, Interactable_OnStartKeepsAValidRow)
{
	DetachedInteractable xFixture;
	ZENITH_ASSERT_TRUE(xFixture.m_xInteractable.SetNpcId(ZM_NPC_WANDERER));
	xFixture.m_xInteractable.SetInteractable(true);
	xFixture.m_xInteractable.OnStart();
	ZENITH_ASSERT_TRUE(xFixture.m_xInteractable.IsInteractable(),
		"a VALID row survives OnStart untouched");
	ZENITH_ASSERT_EQ((u_int)xFixture.m_xInteractable.GetNpcId(), (u_int)ZM_NPC_WANDERER);
}

// ---- Serialization ----------------------------------------------------------

ZENITH_TEST(ZM_Interaction, Interactable_SerializeRoundTrip)
{
	DetachedInteractable xSource;
	ZENITH_ASSERT_TRUE(xSource.m_xInteractable.SetNpcId(ZM_NPC_TRADE_POST_CLERK));
	ZENITH_ASSERT_TRUE(xSource.m_xInteractable.SetRadius(1.75f));
	xSource.m_xInteractable.SetInteractable(true);

	Zenith_DataStream xStream;
	xSource.m_xInteractable.WriteToDataStream(xStream);
	xStream.SetCursor(0u);

	u_int uVersion = 0u;
	xStream >> uVersion;
	ZENITH_ASSERT_EQ(uVersion, ZM_Interactable::uSERIALIZATION_VERSION,
		"the component-contract leading version must be written first");

	xStream.SetCursor(0u);
	DetachedInteractable xTarget;
	xTarget.m_xInteractable.ReadFromDataStream(xStream);
	ZENITH_ASSERT_EQ((u_int)xTarget.m_xInteractable.GetNpcId(),
		(u_int)ZM_NPC_TRADE_POST_CLERK);
	ZENITH_ASSERT_EQ_FLOAT(xTarget.m_xInteractable.GetRadius(), 1.75f, fTEST_EPSILON);
	ZENITH_ASSERT_TRUE(xTarget.m_xInteractable.IsInteractable(),
		"every field survives the round trip");
}

// A payload from a future / stale schema must leave an INERT component rather than
// a half-configured one that reads garbage as an NPC id.
ZENITH_TEST(ZM_Interaction, Interactable_DeserializeRejectsForeignVersion)
{
	Zenith_DataStream xStream;
	const u_int uForeignVersion = ZM_Interactable::uSERIALIZATION_VERSION + 1u;
	xStream << uForeignVersion;
	xStream.SetCursor(0u);

	DetachedInteractable xTarget;
	ZENITH_ASSERT_TRUE(xTarget.m_xInteractable.SetNpcId(ZM_NPC_VILLAGER));
	xTarget.m_xInteractable.SetInteractable(true);
	xTarget.m_xInteractable.ReadFromDataStream(xStream);

	ZENITH_ASSERT_EQ((u_int)xTarget.m_xInteractable.GetNpcId(), (u_int)ZM_NPC_NONE);
	ZENITH_ASSERT_FALSE(xTarget.m_xInteractable.IsInteractable(),
		"an unreadable payload leaves an inert component, never a stale live one");
}

// ---- Trainer sight (S7 item 3 SC6) ------------------------------------------

// A freshly added, unconfigured component is BLIND by construction -- the same
// doctrine as m_bInteractable defaulting false. Nothing here needs a scene: the
// two new members are PODs the component owns outright.
ZENITH_TEST(ZM_Interaction, Interactable_TrainerSightDefaultsOff)
{
	DetachedInteractable xFixture;
	ZENITH_ASSERT_EQ((u_int)xFixture.m_xInteractable.GetTrainerId(),
		(u_int)ZM_TRAINER_NONE,
		"a default-constructed interactable names no trainer row");
	ZENITH_ASSERT_FALSE(xFixture.m_xInteractable.IsTrainerSightEnabled(),
		"an unconfigured component must not be watching anyone");
	ZENITH_ASSERT_EQ((u_int)xFixture.m_xInteractable.GetTrainerSightState(),
		(u_int)ZM_TRAINER_SIGHT_WATCHING,
		"the sight machine starts as a COLD watcher");
	ZENITH_ASSERT_EQ(xFixture.m_xInteractable.GetTrainerSightRaiseCount(), 0u,
		"nothing has been raised yet, so the monotonic raise count is zero");
}

// Fail CLOSED: a bad id CLEARS the row rather than keeping the previous one, so a
// mis-authored value can only ever produce a BLIND NPC -- never the WRONG
// trainer's battle.
ZENITH_TEST(ZM_Interaction, Interactable_ConfigureTrainerSightFailsClosedOnABadId)
{
	DetachedInteractable xFixture;
	ZENITH_ASSERT_TRUE(
		xFixture.m_xInteractable.ConfigureTrainerSight(ZM_TRAINER_RIVAL_VESPER),
		"a registered trainer row must be accepted");
	ZENITH_ASSERT_EQ((u_int)xFixture.m_xInteractable.GetTrainerId(),
		(u_int)ZM_TRAINER_RIVAL_VESPER);
	ZENITH_ASSERT_TRUE(xFixture.m_xInteractable.IsTrainerSightEnabled(),
		"the id IS the enable -- there is no second flag to set");

	// The SENTINEL. ZM_TRAINER_NONE aliases ZM_TRAINER_COUNT, so this is also the
	// "roster shrank underneath me" case.
	ZENITH_ASSERT_FALSE(
		xFixture.m_xInteractable.ConfigureTrainerSight(ZM_TRAINER_NONE),
		"ZM_TRAINER_NONE is not a trainer");
	ZENITH_ASSERT_EQ((u_int)xFixture.m_xInteractable.GetTrainerId(),
		(u_int)ZM_TRAINER_NONE,
		"a rejected id must not leave the PREVIOUS trainer installed");
	ZENITH_ASSERT_FALSE(xFixture.m_xInteractable.IsTrainerSightEnabled(),
		"clearing the row also blinds the watcher");

	// ...and outright garbage, from the same armed starting state, so the clause
	// above cannot be satisfied by a component that was already blind.
	ZENITH_ASSERT_TRUE(
		xFixture.m_xInteractable.ConfigureTrainerSight(ZM_TRAINER_RIVAL_VESPER));
	ZENITH_ASSERT_FALSE(
		xFixture.m_xInteractable.ConfigureTrainerSight((ZM_TRAINER_ID)9999u));
	ZENITH_ASSERT_EQ((u_int)xFixture.m_xInteractable.GetTrainerId(),
		(u_int)ZM_TRAINER_NONE);
	ZENITH_ASSERT_FALSE(xFixture.m_xInteractable.IsTrainerSightEnabled());

	// OnStart re-validates whatever authoring / deserialization left behind. Only ONE
	// half of that guard has a non-vacuous fixture reachable from here, and it is the
	// half asserted below.
	//
	// NOT ASSERTED HERE: "an UNREGISTERED id is CLEARED by OnStart". No public surface
	// can install one -- ConfigureTrainerSight fails CLOSED, so a refused id leaves
	// the component ALREADY at ZM_TRAINER_NONE (and ZM_TRAINER_NONE == ZM_TRAINER_COUNT,
	// so an unconfigured component is the same state). Calling OnStart on that fixture
	// and asserting NONE would restate the fixture back to itself and would stay green
	// with the whole `if (!ZM_IsRegisteredTrainer(m_eTrainerId))` block deleted.
	// Likewise OnStart's `m_xSightFsm.Reset()`: this component has no public stepper
	// (only OnUpdate, which needs a live scene, a player and physics), so the watcher
	// here is COLD whatever OnStart does. Reset on a genuinely DIRTY machine is covered
	// by Fsm_ResetReturnsAColdWatcher in ZM_Tests_TrainerSightFsm.cpp.
	//
	// WHAT IS ASSERTED, and it DOES red: a REGISTERED id must SURVIVE OnStart. An
	// OnStart that clears unconditionally, or one whose ZM_IsRegisteredTrainer test is
	// inverted, blinds a correctly authored trainer -- and SC8 authors exactly one --
	// which fails right here.
	ZENITH_ASSERT_TRUE(
		xFixture.m_xInteractable.ConfigureTrainerSight(ZM_TRAINER_RIVAL_VESPER));
	xFixture.m_xInteractable.OnStart();
	ZENITH_ASSERT_EQ((u_int)xFixture.m_xInteractable.GetTrainerId(),
		(u_int)ZM_TRAINER_RIVAL_VESPER,
		"a REGISTERED trainer id survives OnStart");
	ZENITH_ASSERT_TRUE(xFixture.m_xInteractable.IsTrainerSightEnabled());
}

// ★ THE SCENE-BYTE GUARD. SC6 changes ZERO bytes in the five committed
// ZM_Interactable payloads inside Dawnmere.zscen, BY CONSTRUCTION: both new
// members are runtime-only and uSERIALIZATION_VERSION stays at 2u. The per-
// component size prefix is computed from what is actually written, so ONE added
// field would grow five payloads, move five size prefixes and leave a windowed
// boot with a modified Dawnmere.zscen in git status -- which Games/Zenithmon/
// CLAUDE.md calls a REGRESSION of boot-shape independence, not churn.
//
// Anyone who later appends a trainer field, or bumps the version, reds THIS unit
// before they ever notice a dirty scene file. SC8 owns persistence and its two
// routes; see the ConfigureTrainerSight header comment.
ZENITH_TEST(ZM_Interaction, Interactable_TrainerSightIsNotSerialized)
{
	// A and B are identical apart from the trainer, so the length comparison below
	// isolates exactly one variable.
	DetachedInteractable xUnconfigured;
	ZENITH_ASSERT_TRUE(xUnconfigured.m_xInteractable.SetNpcId(ZM_NPC_VILLAGER));
	ZENITH_ASSERT_TRUE(xUnconfigured.m_xInteractable.SetRadius(0.4f));
	xUnconfigured.m_xInteractable.SetInteractable(true);

	DetachedInteractable xConfigured;
	ZENITH_ASSERT_TRUE(xConfigured.m_xInteractable.SetNpcId(ZM_NPC_VILLAGER));
	ZENITH_ASSERT_TRUE(xConfigured.m_xInteractable.SetRadius(0.4f));
	xConfigured.m_xInteractable.SetInteractable(true);
	// NON-VACUITY: the stream below is written by a component that genuinely HAS a
	// trainer installed, so "the bytes did not move" is a real observation.
	ZENITH_ASSERT_TRUE(
		xConfigured.m_xInteractable.ConfigureTrainerSight(ZM_TRAINER_RIVAL_VESPER));
	ZENITH_ASSERT_TRUE(xConfigured.m_xInteractable.IsTrainerSightEnabled());

	Zenith_DataStream xUnconfiguredStream;
	xUnconfigured.m_xInteractable.WriteToDataStream(xUnconfiguredStream);
	Zenith_DataStream xConfiguredStream;
	xConfigured.m_xInteractable.WriteToDataStream(xConfiguredStream);

	// (1a) THE ABSOLUTE BYTE PIN, and it is the load-bearing half. Spelled out from
	// the fields ZM_Interactable::WriteToDataStream actually writes, IN ITS ORDER --
	// symbolically, so it stays readable and follows uMAX_WAYPOINTS rather than
	// freezing a magic number. GetCursor() is the bytes WRITTEN (GetCapacity() would
	// be the allocation, which doubles in 1 KB steps and would hide a small field).
	//
	// Clause (1b) below CANNOT catch an appended field on its own: the v2 payload is
	// fixed-width and UNCONDITIONAL (even the waypoint loop is a constant-trip
	// uMAX_WAYPOINTS, not m_uCount), so a new field grows BOTH streams equally and
	// their cursors stay equal by construction. Only an absolute expectation reds.
	constexpr u_int uEXPECTED_V2_PAYLOAD_BYTES = (u_int)(
		sizeof(ZM_Interactable::uSERIALIZATION_VERSION)              // leading version
		+ sizeof(u_int)                                              // m_eNpcId, widened to u_int
		+ sizeof(float)                                              // m_fRadius
		+ sizeof(bool)                                               // m_bInteractable
		+ sizeof(bool)                                               // m_bWanderEnabled
		+ sizeof(u_int)                                              // m_xWalkerWaypoints.m_uCount
		+ sizeof(float) * 3u * ZM_WalkerWaypoints::uMAX_WAYPOINTS    // EVERY waypoint slot
		+ sizeof(float) * 3u);                                       // speed / arrive / dwell
	ZENITH_ASSERT_EQ((u_int)xConfiguredStream.GetCursor(),
		uEXPECTED_V2_PAYLOAD_BYTES,
		"ZM_Interactable wrote %u bytes, not the %u the v2 layout accounts for -- a "
		"field was added to (or removed from) WriteToDataStream, so five payloads "
		"inside the COMMITTED Dawnmere.zscen move, five size prefixes move with them, "
		"and a boot leaves the scene modified in git status",
		(u_int)xConfiguredStream.GetCursor(), uEXPECTED_V2_PAYLOAD_BYTES);

	// (1b) ...and IDENTICAL byte length with and without a trainer installed. This
	// clause is NOT redundant with (1a) and neither subsumes the other: (1a) catches
	// an UNCONDITIONALLY written field (which keeps the two cursors equal), (1b)
	// catches a CONDITIONALLY written one -- `if (IsTrainerSightEnabled()) xStream <<
	// ...` -- which would keep the total at uEXPECTED_V2_PAYLOAD_BYTES for the
	// unconfigured NPCs Dawnmere actually holds while still moving bytes the moment
	// SC8 authors a trainer. Both are needed; do not delete either as a duplicate.
	ZENITH_ASSERT_EQ((u_int)xConfiguredStream.GetCursor(),
		(u_int)xUnconfiguredStream.GetCursor(),
		"configuring a trainer changed the serialized size (%u vs %u bytes) -- SC6 "
		"must add NO trainer-conditional field to ZM_Interactable::WriteToDataStream",
		(u_int)xConfiguredStream.GetCursor(), (u_int)xUnconfiguredStream.GetCursor());

	// (2) the leading version is untouched.
	ZENITH_ASSERT_EQ(ZM_Interactable::uSERIALIZATION_VERSION, 2u,
		"ZM_Interactable::uSERIALIZATION_VERSION moved off 2u -- every committed "
		"scene's payloads change with it, so the bump and the re-bake + re-commit of "
		"Dawnmere.zscen belong in ONE commit (SC8's version route)");

	// (3) a component read back from the CONFIGURED stream has NO trainer: the reset
	//     block in ReadFromDataStream owns that, and a reloaded scene must be
	//     indistinguishable from a fresh component.
	//
	//     The target is ARMED WITH A REAL ROSTER ID FIRST, and that is what gives the
	//     clause teeth: deleting `m_eTrainerId = ZM_TRAINER_NONE;` from
	//     ReadFromDataStream leaves Vesper installed on a component that just loaded a
	//     scene payload naming nobody, and this reds. Asserting NONE on a target that
	//     was already NONE would restate the fixture.
	xConfiguredStream.SetCursor(0u);
	DetachedInteractable xTarget;
	ZENITH_ASSERT_TRUE(
		xTarget.m_xInteractable.ConfigureTrainerSight(ZM_TRAINER_RIVAL_VESPER),
		"arm the target FIRST, so the read below has something to clear");
	ZENITH_ASSERT_TRUE(xTarget.m_xInteractable.IsTrainerSightEnabled(),
		"...and the arming really took, or the clause below is vacuous");
	xTarget.m_xInteractable.ReadFromDataStream(xConfiguredStream);
	ZENITH_ASSERT_EQ((u_int)xTarget.m_xInteractable.GetTrainerId(),
		(u_int)ZM_TRAINER_NONE,
		"deserialization must leave NO trainer installed -- the id is runtime-only");
	ZENITH_ASSERT_FALSE(xTarget.m_xInteractable.IsTrainerSightEnabled());
	// NOT ASSERTED HERE: that ReadFromDataStream's `m_xSightFsm.Reset()` ran. This
	// component exposes no public stepper (only OnUpdate, which needs a live scene, a
	// player and physics), so xTarget's watcher is COLD whatever the read does, and a
	// state/raise-count assertion here would pass with that Reset deleted -- a
	// tautology claiming coverage it does not have. Reset on a genuinely DIRTY
	// machine -- ENGAGED, confirmed, with a populated accumulator and a non-zero raise
	// count -- is covered by Fsm_ResetReturnsAColdWatcher in
	// Games/Zenithmon/Tests/ZM_Tests_TrainerSightFsm.cpp.
	//
	// The rest of the payload still round-trips, so clause (1a) is not passing
	// because the write silently stopped writing anything.
	ZENITH_ASSERT_EQ((u_int)xTarget.m_xInteractable.GetNpcId(), (u_int)ZM_NPC_VILLAGER);
	ZENITH_ASSERT_TRUE(xTarget.m_xInteractable.IsInteractable());
}

// ---- S7 item 3 SC8: the ZERO-BYTE persistence route --------------------------

// THE DERIVATION. OnStart is safe on a DETACHED component: it touches only its own
// PODs and returns before TryConfigureWanderBody via `if (!m_bWanderEnabled)`.
ZENITH_TEST(ZM_Interaction, Interactable_OnStartDerivesTheTrainerFromItsNpcRow)
{
	// (A) the rival's row arms the cone.
	{
		DetachedInteractable xFixture;
		ZENITH_ASSERT_TRUE(xFixture.m_xInteractable.SetNpcId(ZM_NPC_RIVAL_VESPER));
		// NON-VACUITY: nothing is armed BEFORE OnStart, so the assertion after it is
		// a real observation rather than a restatement of the fixture.
		ZENITH_ASSERT_EQ((u_int)xFixture.m_xInteractable.GetTrainerId(),
			(u_int)ZM_TRAINER_NONE, "a freshly configured NPC has no trainer yet");
		ZENITH_ASSERT_FALSE(xFixture.m_xInteractable.IsTrainerSightEnabled());

		xFixture.m_xInteractable.OnStart();

		ZENITH_ASSERT_EQ((u_int)xFixture.m_xInteractable.GetTrainerId(),
			(u_int)ZM_TRAINER_RIVAL_VESPER,
			"OnStart must derive the trainer from the NPC row -- this is the whole "
			"zero-byte persistence route");
		ZENITH_ASSERT_TRUE(xFixture.m_xInteractable.IsTrainerSightEnabled());
		ZENITH_ASSERT_EQ(xFixture.m_xInteractable.GetTrainerSightRaiseCount(), 0u,
			"a freshly started trainer is a COLD watcher");
	}
	// (B) THE ROW-DRIVEN NEGATIVE: a non-trainer row derives nothing, so the
	//     derivation is driven by the COLUMN and not merely by "has an npc row".
	{
		DetachedInteractable xFixture;
		ZENITH_ASSERT_TRUE(xFixture.m_xInteractable.SetNpcId(ZM_NPC_VILLAGER));
		xFixture.m_xInteractable.OnStart();
		ZENITH_ASSERT_EQ((u_int)xFixture.m_xInteractable.GetTrainerId(),
			(u_int)ZM_TRAINER_NONE, "the villager's row names no trainer");
		ZENITH_ASSERT_FALSE(xFixture.m_xInteractable.IsTrainerSightEnabled());
	}
	// (C) THE CLAMP. SetNpcId fails closed to ZM_NPC_NONE; the derivation must not
	//     index past ZM_NPC_COUNT, where ZM_GetNpcData's Zenith_Assert would end the
	//     ENTIRE boot-unit run rather than fail one test.
	{
		DetachedInteractable xFixture;
		ZENITH_ASSERT_FALSE(xFixture.m_xInteractable.SetNpcId((ZM_NPC_ID)9999u));
		xFixture.m_xInteractable.OnStart();
		ZENITH_ASSERT_EQ((u_int)xFixture.m_xInteractable.GetTrainerId(),
			(u_int)ZM_TRAINER_NONE);
	}
}

// FILL-IF-EMPTY. Runtime configuration WINS over the authored row. This is the
// clause that protects ZM_TrainerSightWalkUp_Test, whose runtime fixture has NO npc
// row and is configured BEFORE OnStart dispatches -- and whose phases 7a2/8/9
// reconfigure the SAME component onto the rambler row mid-test.
ZENITH_TEST(ZM_Interaction, Interactable_DerivedTrainerNeverOverwritesARuntimeConfiguredOne)
{
	DetachedInteractable xFixture;
	ZENITH_ASSERT_TRUE(xFixture.m_xInteractable.SetNpcId(ZM_NPC_RIVAL_VESPER));
	ZENITH_ASSERT_TRUE(
		xFixture.m_xInteractable.ConfigureTrainerSight(ZM_TRAINER_ROUTE1_RAMBLER));
	// NON-VACUITY: the runtime row really took before OnStart runs.
	ZENITH_ASSERT_EQ((u_int)xFixture.m_xInteractable.GetTrainerId(),
		(u_int)ZM_TRAINER_ROUTE1_RAMBLER);

	xFixture.m_xInteractable.OnStart();

	ZENITH_ASSERT_EQ((u_int)xFixture.m_xInteractable.GetTrainerId(),
		(u_int)ZM_TRAINER_ROUTE1_RAMBLER,
		"the authored row overwrote a RUNTIME-configured trainer -- the derivation "
		"must be fill-if-empty, or the shipped windowed sight gate goes blind");
	ZENITH_ASSERT_TRUE(xFixture.m_xInteractable.IsTrainerSightEnabled());
}

// THE PERSISTENCE UNIT, and the reason uSERIALIZATION_VERSION can stay at 2u.
// The BYTES carry no trainer; the ROW supplies it. Both halves asserted in one
// place so nobody can "fix" one by breaking the other.
ZENITH_TEST(ZM_Interaction, Interactable_TrainerIdIsRederivedFromTheRowAfterASceneRoundTrip)
{
	DetachedInteractable xSource;
	ZENITH_ASSERT_TRUE(xSource.m_xInteractable.SetNpcId(ZM_NPC_RIVAL_VESPER));
	ZENITH_ASSERT_TRUE(xSource.m_xInteractable.SetRadius(0.4f));
	xSource.m_xInteractable.SetInteractable(true);
	ZENITH_ASSERT_TRUE(
		xSource.m_xInteractable.ConfigureTrainerSight(ZM_TRAINER_RIVAL_VESPER));

	Zenith_DataStream xStream;
	xSource.m_xInteractable.WriteToDataStream(xStream);

	// Corroborates the shipped absolute pin from the OTHER side of the feature: SC8
	// added a persistence route that must cost ZERO bytes, so re-derive the v2
	// payload size symbolically HERE too. (Expected to red alongside
	// Interactable_TrainerSightIsNotSerialized clause (1a); that is correct, and the
	// mutation log should say so.)
	constexpr u_int uEXPECTED_V2_PAYLOAD_BYTES = (u_int)(
		sizeof(ZM_Interactable::uSERIALIZATION_VERSION)
		+ sizeof(u_int) + sizeof(float) + sizeof(bool) + sizeof(bool) + sizeof(u_int)
		+ sizeof(float) * 3u * ZM_WalkerWaypoints::uMAX_WAYPOINTS
		+ sizeof(float) * 3u);
	ZENITH_ASSERT_EQ((u_int)xStream.GetCursor(), uEXPECTED_V2_PAYLOAD_BYTES,
		"SC8's persistence route must cost ZERO serialized bytes");

	xStream.SetCursor(0u);
	DetachedInteractable xTarget;
	// ARM THE TARGET FIRST, exactly as Interactable_TrainerSightIsNotSerialized clause
	// (3) does, and for the same reason: a FRESH fixture is ALREADY ZM_TRAINER_NONE, so
	// the "THE BYTES CARRY NO TRAINER" clause below would restate the fixture and stay
	// green with `m_eTrainerId = ZM_TRAINER_NONE;` deleted from ReadFromDataStream. A
	// DIFFERENT roster id is used on purpose: it also makes the post-OnStart clause
	// distinguish "the ROW supplied Vesper" from "Vesper was simply never cleared".
	ZENITH_ASSERT_TRUE(
		xTarget.m_xInteractable.ConfigureTrainerSight(ZM_TRAINER_ROUTE1_RAMBLER),
		"arm the target FIRST, so the read below has something to clear");
	ZENITH_ASSERT_TRUE(xTarget.m_xInteractable.IsTrainerSightEnabled(),
		"...and the arming really took, or both clauses below are vacuous");
	xTarget.m_xInteractable.ReadFromDataStream(xStream);
	// THE BYTES CARRY NO TRAINER.
	ZENITH_ASSERT_EQ((u_int)xTarget.m_xInteractable.GetNpcId(),
		(u_int)ZM_NPC_RIVAL_VESPER, "the npc id is the ONLY thing on disk");
	ZENITH_ASSERT_EQ((u_int)xTarget.m_xInteractable.GetTrainerId(),
		(u_int)ZM_TRAINER_NONE,
		"deserialization must leave NO trainer installed -- the ARMED rambler had to "
		"be cleared by the read, not merely absent from the bytes");
	// ...AND THE ROW SUPPLIES IT.
	xTarget.m_xInteractable.OnStart();
	ZENITH_ASSERT_EQ((u_int)xTarget.m_xInteractable.GetTrainerId(),
		(u_int)ZM_TRAINER_RIVAL_VESPER,
		"an authored rival came back from a scene round trip BLIND -- the row is "
		"what makes his identity survive, not the bytes");
	ZENITH_ASSERT_TRUE(xTarget.m_xInteractable.IsTrainerSightEnabled());
}

// ---- Role -> seam mapping (pure; nothing is raised) --------------------------

ZENITH_TEST(ZM_Interaction, RaiseKind_TalkerMapsToDialogue)
{
	ZENITH_ASSERT_EQ((u_int)ZM_RaiseKindForRole(ZM_NPC_ROLE_TALKER),
		(u_int)ZM_NPC_RAISE_DIALOGUE,
		"a talker talks through ZM_UI_MenuStack::TryPushDialogue");
}

ZENITH_TEST(ZM_Interaction, RaiseKind_ShopkeepMapsToShop)
{
	ZENITH_ASSERT_EQ((u_int)ZM_RaiseKindForRole(ZM_NPC_ROLE_SHOPKEEP),
		(u_int)ZM_NPC_RAISE_SHOP,
		"a shopkeep opens the mart through ZM_UI_MenuStack::TryOpenShop");
}

ZENITH_TEST(ZM_Interaction, RaiseKind_CaretakerMapsToCareCenter)
{
	ZENITH_ASSERT_EQ((u_int)ZM_RaiseKindForRole(ZM_NPC_ROLE_CARETAKER),
		(u_int)ZM_NPC_RAISE_CARE_CENTER,
		"a caretaker raises the heal yes/no prompt");
}

// Totality: EVERY enumerated role has a real seam behind it. A role added without a
// dispatch arm would otherwise ship as an NPC that silently does nothing.
ZENITH_TEST(ZM_Interaction, RaiseKind_EveryRoleMapsToARealSeam)
{
	ZENITH_ASSERT_GT((u_int)ZM_NPC_ROLE_COUNT, 0u,
		"the role enum must be non-empty, or the walk below is vacuous");
	for (u_int u = 0u; u < (u_int)ZM_NPC_ROLE_COUNT; ++u)
	{
		const ZM_NPC_RAISE_KIND eKind = ZM_RaiseKindForRole((ZM_NPC_ROLE)u);
		ZENITH_ASSERT_NE((u_int)eKind, (u_int)ZM_NPC_RAISE_NONE,
			"role %u has no raise seam", u);
		ZENITH_ASSERT_LT((u_int)eKind, (u_int)ZM_NPC_RAISE_COUNT,
			"role %u mapped outside the raise-kind enum", u);
	}
}

ZENITH_TEST(ZM_Interaction, RaiseKind_OutOfRangeRoleMapsToNone)
{
	ZENITH_ASSERT_EQ((u_int)ZM_RaiseKindForRole((ZM_NPC_ROLE)ZM_NPC_ROLE_COUNT),
		(u_int)ZM_NPC_RAISE_NONE,
		"ZM_NPC_ROLE_COUNT is not a role -- it must fall through to NONE, which LOGS");
	ZENITH_ASSERT_EQ((u_int)ZM_RaiseKindForRole((ZM_NPC_ROLE)4242u),
		(u_int)ZM_NPC_RAISE_NONE);
}

ZENITH_TEST(ZM_Interaction, RaiseKind_NamesAreTotalAndDistinct)
{
	for (u_int u = 0u; u < (u_int)ZM_NPC_RAISE_COUNT; ++u)
	{
		const char* szName = ZM_NpcRaiseKindName((ZM_NPC_RAISE_KIND)u);
		ZENITH_ASSERT_NOT_NULL(szName, "raise kind %u has no name", u);
		ZENITH_ASSERT_TRUE(szName[0] != '\0', "raise kind %u has an EMPTY name", u);
		for (u_int v = 0u; v < u; ++v)
		{
			ZENITH_ASSERT_TRUE(
				std::strcmp(szName, ZM_NpcRaiseKindName((ZM_NPC_RAISE_KIND)v)) != 0,
				"raise kinds %u and %u share a name", u, v);
		}
	}
	ZENITH_ASSERT_STREQ(ZM_NpcRaiseKindName((ZM_NPC_RAISE_KIND)ZM_NPC_RAISE_COUNT),
		"UNKNOWN", "the name formatter is TOTAL -- it never indexes off the end");
}

// Content x mapping: every AUTHORED roster row lands on a real seam. This is the
// join the two halves of the feature meet at, and neither table alone can catch a
// row whose role has no arm.
// S7 item 2 SC1: the warden is the ONE live consumer of story-flag gating, and
// gating selects CONTENT -- it never re-routes which seam a role talks through. So
// his row has to keep landing on the dialogue seam specifically: given any other
// role he would open a mart or a heal prompt and his refusal lines, which are the
// whole demonstration, would never be spoken.
ZENITH_TEST(ZM_Interaction, Interactable_WardenRowIsDispatchableAsDialogue)
{
	const ZM_NpcData& xRow = ZM_GetNpcData(ZM_NPC_ROUTE_WARDEN);
	ZENITH_ASSERT_EQ((u_int)ZM_RaiseKindForRole(xRow.m_eRole),
		(u_int)ZM_NPC_RAISE_DIALOGUE,
		"'%s' has role %u, which does not talk through ZM_UI_MenuStack::TryPushDialogue",
		xRow.m_szDisplayName, (u_int)xRow.m_eRole);
}

ZENITH_TEST(ZM_Interaction, RaiseKind_EveryAuthoredNpcHasASeam)
{
	ZENITH_ASSERT_GT(ZM_GetNpcCount(), 0u,
		"an empty roster would make this walk vacuous");
	for (u_int u = 0u; u < ZM_GetNpcCount(); ++u)
	{
		const ZM_NpcData& xRow = ZM_GetNpcData((ZM_NPC_ID)u);
		ZENITH_ASSERT_NE((u_int)ZM_RaiseKindForRole(xRow.m_eRole),
			(u_int)ZM_NPC_RAISE_NONE,
			"authored NPC %u ('%s') has no raise seam", u, xRow.m_szDisplayName);
	}
}

// ---- ZM_InteractionRuntime latches ------------------------------------------

// The latches are process-GLOBAL, so "start" here means "after the documented
// reset seam" -- which is exactly the state the between-tests hook installs.
ZENITH_TEST(ZM_Interaction, Runtime_LatchesStartCleared)
{
	ZM_InteractionRuntime::ResetRuntimeStateForTests();
	const ZM_InteractionRuntime xRuntime{};
	ZENITH_ASSERT_FALSE(xRuntime.HasLatchedResult(),
		"a cleared runtime reports that nothing has been attempted");
	ZENITH_ASSERT_EQ((u_int)xRuntime.GetLastResult(),
		(u_int)ZM_INTERACT_REJECT_NO_INPUT_EDGE);
	ZENITH_ASSERT_EQ(xRuntime.GetLastTarget(), INVALID_ENTITY_ID,
		"a cleared runtime names no target");
}

ZENITH_TEST(ZM_Interaction, Runtime_RaiseCountStartsZero)
{
	ZM_InteractionRuntime::ResetRuntimeStateForTests();
	const ZM_InteractionRuntime xRuntime{};
	ZENITH_ASSERT_EQ(xRuntime.GetRaiseCount(), 0u,
		"nothing has been raised yet, so the monotonic raise count is zero");
}

// POPULATE, then reset, then assert cleared. Ticking with no interact edge is a
// legitimate decision (NO_INPUT_EDGE) and still sets the has-run flag, which is the
// observable that separates "cleared" from "ran and decided nothing" -- without it
// this unit would assert the reset against a fixture that was never dirty.
ZENITH_TEST(ZM_Interaction, Runtime_ResetClearsAPopulatedLatch)
{
	ZM_InteractionRuntime::ResetRuntimeStateForTests();
	ZM_InteractionRuntime xRuntime;
	ZENITH_ASSERT_FALSE(xRuntime.HasLatchedResult());

	xRuntime.Tick(Zenith_Maths::Vector3(0.0f),
		Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f));
	ZENITH_ASSERT_TRUE(xRuntime.HasLatchedResult(),
		"Tick must latch its decision EITHER WAY -- rejects are what walk-up tests poll");

	ZM_InteractionRuntime::ResetRuntimeStateForTests();
	ZENITH_ASSERT_FALSE(xRuntime.HasLatchedResult(),
		"the reset seam clears a latch that was genuinely populated");
	ZENITH_ASSERT_EQ((u_int)xRuntime.GetLastResult(),
		(u_int)ZM_INTERACT_REJECT_NO_INPUT_EDGE);
	ZENITH_ASSERT_EQ(xRuntime.GetRaiseCount(), 0u);
}

// The probe cap has to comfortably clear the authored roster, or the live site
// would be truncating candidates in a shipped town.
ZENITH_TEST(ZM_Interaction, Runtime_ProbeCapClearsTheAuthoredRoster)
{
	// NB this is a LOWER bound only. The cap bounds interactables present in one
	// SCENE, which is not the same quantity as the roster size -- a town may place
	// several entities per roster row -- so it is a sanity floor, not a proof the
	// cap is adequate. The `== 64u` assertion that used to sit here was removed: it
	// restated the constant back to itself, so it failed on ANY edit, right or
	// wrong, and carried no correctness signal.
	ZENITH_ASSERT_GT(uZM_MAX_INTERACT_PROBES, ZM_GetNpcCount(),
		"the probe cap must exceed the whole authored NPC roster");
}

// ---- SC7 gate preconditions -------------------------------------------------

// The SC7 consolidated gate (ZM_S6InteractGate_Test) proves one beat per ROLE, and
// because it is m_bRequiresGraphics it SKIPS (== passes) in headless CI -- so a role
// that quietly lost its beat is reported by NOTHING there. This unit is the headless
// stand-in, and it walks the BEAT table specifically.
//
// It deliberately does NOT walk the placed roster: that table carries the warden, a
// second TALKER, so coverage over it survives the very mutation this unit exists to
// catch. Nor is it a duplicate of ZM_Tests_NpcData's Npc_RolesCoverEveryDispatchArm,
// which walks the WHOLE roster (wanderer and warden included) and stays green in
// exactly the same case.
//
// The beats' ids are spelled as enumerators, so "names a real row" is true by
// construction and no range walk is written for it -- that assertion could not fail,
// and this file has already had one such tautology removed
// (Runtime_ProbeCapClearsTheAuthoredRoster).
ZENITH_TEST(ZM_Interaction, GateRoster_BeatNpcsCoverEveryRole)
{
	// REDS ON: adding a ZM_NPC_ROLE enumerator (or deleting a row from axGATE_BEATS).
	// One beat per role is what "the gate covers the dispatch switch" means, and a
	// new role with no beat is a shipped arm nothing ever presses. This also subsumes
	// the usual non-vacuity guard: an EMPTY role enum fails here rather than making
	// the walk below pass silently.
	ZENITH_ASSERT_EQ(uGATE_BEAT_COUNT, (u_int)ZM_NPC_ROLE_COUNT,
		"the gate authors %u beats for %u dispatch roles -- some role has no beat, and "
		"the gate SKIPS in headless CI so nothing else would report it",
		uGATE_BEAT_COUNT, (u_int)ZM_NPC_ROLE_COUNT);

	// REDS ON: re-rolling a beat NPC's m_eRole in ZM_NpcData.cpp -- precisely the
	// ZM_NPC_VILLAGER -> ZM_NPC_ROLE_SHOPKEEP mutation. The talk beat would walk up to
	// Npc_Villager, press E and be handed a MART; here that reads as the row's seam
	// (SHOP) disagreeing with the seam the beat was written against (DIALOGUE).
	for (u_int u = 0u; u < uGATE_BEAT_COUNT; ++u)
	{
		const GateBeat& xBeat = axGATE_BEATS[u];
		const ZM_NpcData& xRow = ZM_GetNpcData(xBeat.m_eNpc);
		const ZM_NPC_RAISE_KIND eActual = ZM_RaiseKindForRole(xRow.m_eRole);
		ZENITH_ASSERT_EQ((u_int)eActual, (u_int)xBeat.m_eExpectedKind,
			"the gate's %s beat walks up to %s ('%s', role %u), which now raises %s "
			"instead of %s", xBeat.m_szBeat, xBeat.m_szEntityName, xRow.m_szDisplayName,
			(u_int)xRow.m_eRole, ZM_NpcRaiseKindName(eActual),
			ZM_NpcRaiseKindName(xBeat.m_eExpectedKind));
	}

	// REDS ON: that same re-roll "fixed" by editing m_eExpectedKind above to agree
	// with the mutated row. The kinds would line up again, but the role the NPC
	// VACATED would then be carried by no beat at all, and this walk still reports it.
	//
	// Together with the count equality above, this also forces the three beats onto
	// three DISTINCT roles -- three carriers cannot double up on a role without
	// leaving another uncovered -- so no separate distinctness assertion is written.
	for (u_int uRole = 0u; uRole < (u_int)ZM_NPC_ROLE_COUNT; ++uRole)
	{
		bool bCovered = false;
		for (u_int u = 0u; u < uGATE_BEAT_COUNT && !bCovered; ++u)
		{
			bCovered = ((u_int)ZM_GetNpcData(axGATE_BEATS[u].m_eNpc).m_eRole == uRole);
		}
		ZENITH_ASSERT_TRUE(bCovered,
			"role %u is carried by NO gate BEAT, so the SC7 walk-up gate never presses E "
			"at an NPC that uses it -- give a beat NPC that role, or add a beat",
			uRole);
	}
}

// What the wider PLACED table still legitimately proves, now that role coverage has
// been taken off it. Every assertion here is about "an NPC standing in the town that
// a walk-up test can reach"; none of them speaks about roles as a SET, so none of
// them can stand in for the beat coverage above.
ZENITH_TEST(ZM_Interaction, GateRoster_PlacedNpcsAreStationaryAndIncludeEveryBeat)
{
	// REDS ON: emptying aePLACED_NPCS. Both walks below are bounded by this count, so
	// an empty table would make them pass while proving nothing.
	ZENITH_ASSERT_GT(uPLACED_NPC_COUNT, 0u,
		"the placed-NPC table is empty, so the walks below are vacuous");

	// REDS ON: setting m_bWanders on one of these rows in ZM_NpcData.cpp. Every entry
	// here is approached at a FIXED authored position; a row that started patrolling
	// would walk out from under that approach, and the walk-up test would time out
	// naming a distance rather than the patrol. ZM_Tests_NpcData's
	// Npc_ExactlyOneRowWanders cannot see this: MOVING the flag from the wanderer onto
	// the villager keeps its count at exactly one.
	for (u_int u = 0u; u < uPLACED_NPC_COUNT; ++u)
	{
		const ZM_NpcData& xRow = ZM_GetNpcData(aePLACED_NPCS[u]);
		ZENITH_ASSERT_FALSE(xRow.m_bWanders,
			"placed NPC '%s' now WANDERS, but this table's entries are walked up to at a "
			"fixed authored position", xRow.m_szDisplayName);
	}

	// REDS ON: dropping an NPC from aePLACED_NPCS while a beat still names it -- which
	// is exactly what removing its ZM_QueueDawnmereNpc call from Zenithmon.cpp is
	// supposed to look like here. A beat that presses E at an NPC nobody placed is a
	// gate beat that can only ever time out.
	for (u_int uBeat = 0u; uBeat < uGATE_BEAT_COUNT; ++uBeat)
	{
		bool bPlaced = false;
		for (u_int u = 0u; u < uPLACED_NPC_COUNT && !bPlaced; ++u)
		{
			bPlaced = (aePLACED_NPCS[u] == axGATE_BEATS[uBeat].m_eNpc);
		}
		ZENITH_ASSERT_TRUE(bPlaced,
			"the gate's %s beat walks up to %s, which no longer appears in the placed "
			"roster -- nothing stands in Dawnmere for that beat to reach",
			axGATE_BEATS[uBeat].m_szBeat, axGATE_BEATS[uBeat].m_szEntityName);
	}
}

// The gate reaches every NPC by loading the BAKED Dawnmere scene and resolving
// entities by name, which only yields an armed ZM_Interactable if the component
// deserializes -- and it only deserializes if it is in the component-meta registry
// under exactly this name (DeserializeEntityComponents keys on the type NAME and
// merely LOGS + skips an unknown one). Deleting the ZENITH_REGISTER_COMPONENT line
// therefore turns every NPC in the town silently inert, which the windowed gate
// reports as a boot timeout hundreds of frames later -- and never reports at all in
// headless CI, where it skips.
//
// The ORDER VALUE is deliberately NOT asserted equal to 113. Serialization is keyed
// by NAME (see above), the component header says in so many words that the number is
// a within-entity tiebreak that must not be "fixed" by renumbering, and an `== 113u`
// clause would restate Zenithmon.cpp's literal back to itself -- exactly the
// tautology that was already removed from Runtime_ProbeCapClearsTheAuthoredRoster.
// What IS asserted is the part the engine genuinely does not police: Finalize sorts
// by order and logs every pair but has NO duplicate detection, so two components
// silently sharing an order sort arbitrarily against each other.
ZENITH_TEST(ZM_Interaction, GateRoster_InteractableIsRegisteredExactlyOnce)
{
	const Zenith_ComponentMetaRegistry& xRegistry = Zenith_ComponentMetaRegistry::Get();
	ZENITH_ASSERT_TRUE(xRegistry.IsInitialized(),
		"the component-meta registry is not sealed yet -- the walk below would be over "
		"an empty list and the uniqueness check would pass vacuously");

	const Zenith_ComponentMeta* pxMeta = xRegistry.GetMetaByName(szINTERACTABLE_META_NAME);
	ZENITH_ASSERT_NOT_NULL(pxMeta,
		"'%s' is not in the component-meta registry -- it cannot deserialize out of the "
		"baked Dawnmere scene, so every authored NPC would be inert",
		szINTERACTABLE_META_NAME);
	// A failed assert RECORDS and CONTINUES, so the null case has to be guarded here
	// rather than trusted.
	if (pxMeta == nullptr)
	{
		return;
	}

	// ZM components claim the 100+ band (Zenithmon.cpp). Registering a game component
	// down in the engine's band is how an order collision gets introduced in the first
	// place.
	ZENITH_ASSERT_GE(pxMeta->m_uSerializationOrder, 100u,
		"'%s' is registered at order %u, below the 100+ band ZM components claim",
		szINTERACTABLE_META_NAME, pxMeta->m_uSerializationOrder);

	// OnStart is the hook that de-arms an unconfigured row (see
	// Interactable_OnStartClearsAnUnconfiguredRow). Concept detection dropping it
	// would leave a mis-authored NPC live and absorbing the player's interact press.
	ZENITH_ASSERT_NOT_NULL(pxMeta->m_pfnOnStart,
		"'%s' registered without its OnStart hook -- an unconfigured NPC row would stay "
		"a live interact candidate",
		szINTERACTABLE_META_NAME);

	const Zenith_Vector<const Zenith_ComponentMeta*>& xSorted = xRegistry.GetAllMetasSorted();
	ZENITH_ASSERT_GT(xSorted.GetSize(), 0u,
		"the sorted meta list is empty, so the uniqueness count below would be vacuous");
	u_int uAtSameOrder = 0u;
	for (u_int u = 0u; u < xSorted.GetSize(); ++u)
	{
		const Zenith_ComponentMeta* pxOther = xSorted.Get(u);
		if (pxOther == nullptr)
		{
			continue;
		}
		if (pxOther->m_uSerializationOrder == pxMeta->m_uSerializationOrder)
		{
			++uAtSameOrder;
		}
	}
	// Only the ORDER count is asserted: the registry stores metas in a map KEYED BY TYPE
	// NAME (RegisterComponent overwrites by name and Finalize pushes exactly one pointer
	// per map entry), so a by-NAME count is structurally 1 once GetMetaByName has already
	// resolved above -- it could not fail. Duplicate ORDERS are genuinely unpoliced.
	ZENITH_ASSERT_EQ(uAtSameOrder, 1u,
		"%u components share serialization order %u with '%s' -- the registry sorts "
		"duplicates arbitrarily against each other and warns about nothing",
		uAtSameOrder, pxMeta->m_uSerializationOrder, szINTERACTABLE_META_NAME);
}
