# AI requirement catalogue: Stage 4 systems and Stage 6

## Purpose

The third file of the AI catalogue: the requirements from Z2-R-AI-097 onward (volumes, perception, blackboards, scripting and the Luau libraries, LOD, debugging, determinism, saves and failure handling, the Stage 6 qualification rows, and every requirement added after the first split, Z2-R-AI-163 onward), in the same format and the same `Z2-R-AI-NNN` numbering as `Zenith2/Docs/Requirements/AI.md`, which holds the purpose of the whole catalogue, the Stage 1 and Stage 2 requirements, the coverage of inventory items (over all three files) and the exclusions; Z2-R-AI-007 … Z2-R-AI-096 are `Zenith2/Docs/Requirements/AI/Stage4Plus.md`.

## Requirements

### Z2-R-AI-097 Volume model

- **Statement:** Volume classes must store sparse voxel octrees of 4 by 4 by 4 leaf bricks with solid and water masks, collapsed uniform nodes and six face links, at most uAI_VOLUME_MAX_NODES nodes per volume.
- **Covers:** Z2-I-AI-16, Z2-I-AI-17
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** Z2-B-AI-001
- **Acceptance evidence:** Test_NavVolume_CollapsedNodes and Test_NavFormats_VolumeSectionChecks in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Volumes.md → "Volume model"; Zenith2/Docs/Design/AI/Formats.md → "Volume section"

### Z2-R-AI-098 Volume bake

- **Statement:** The volume bake must rasterize solids inflated by the class radius, mark water from water prisms and collapse and link nodes deterministically.
- **Covers:** Z2-I-AI-16, Z2-I-AI-17
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_NavVolume_BakeSolidInflation and Test_NavVolume_WaterMedium in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Volumes.md → "Volume bake"

### Z2-R-AI-099 Volume path search

- **Statement:** Zenith_NavVolumeSearch must find paths through free units of the agent's medium with stable tie-breaks and the slicing rules of the polygon search.
- **Covers:** Z2-I-AI-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-AI-002
- **Acceptance evidence:** Test_NavVolume_PathAroundPillar and Test_NavVolume_SearchDeterministic in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Volumes.md → "Volume search"

### Z2-R-AI-100 Volume smoothing and line of sight

- **Statement:** Volume paths must be smoothed by greedy line of sight through voxels of the agent's medium, the DDA agreeing with a brute-force voxel walk.
- **Covers:** Z2-I-AI-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_NavVolume_LineOfSightDDA in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Volumes.md → "Volume path smoothing"; Zenith2/Docs/Design/AI/Volumes.md → "Volume line of sight"

### Z2-R-AI-101 Flying navigation

- **Statement:** Flying agents must plan through AIR voxels and cross obstacles ground classes cannot.
- **Covers:** Z2-I-AI-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Test_NavVolume_CanyonScenario in the Test step; Test_NavConformance_N12_Volume in ZenithAI_Conformance.
- **Design:** Zenith2/Docs/Design/AI/Volumes.md → "Volume search"; Zenith2/Docs/Design/AI/Volumes.md → "Swimming"

### Z2-R-AI-102 Three-dimensional avoidance

- **Statement:** Flying and underwater agents must avoid each other with the three-dimensional ORCA form.
- **Covers:** Z2-I-AI-16, Z2-I-AI-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_AIOrca3D_SpheresAvoid in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Three-dimensional avoidance"

### Z2-R-AI-103 Surface swimming

- **Statement:** A ground agent whose cost profile admits water must plan across swimmable polygons, and a walker's default profile must never cross water.
- **Covers:** Z2-I-AI-17
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** Test_NavPath_CostProfiles in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Volumes.md → "Swimming"; Zenith2/Docs/Design/AI/Navigation.md → "Area codes and costs"

### Z2-R-AI-104 Underwater navigation

- **Statement:** An underwater agent must plan through WATER voxels only, and an amphibious one through water and air with the medium-change cost.
- **Covers:** Z2-I-AI-17
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Test_NavVolume_DiverStaysInWater in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Volumes.md → "Swimming"; Zenith2/Docs/Design/AI/Volumes.md → "Volume search"

### Z2-R-AI-105 Medium transitions

- **Statement:** An agent must change medium only through a SET_MEDIUM command and must replan in the new structure at the change.
- **Covers:** Z2-I-AI-17, Z2-I-AI-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** Test_AIAgent_StateMachineTable (the medium-changed row) in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Volumes.md → "Medium transitions"; Zenith2/Docs/Design/AI/Agents.md → "Agent state machine"

### Z2-R-AI-106 Perception components

- **Statement:** Zenith.AI.PerceptionState must carry the perceiver configuration, summary state and a memory container of at most 16 entries, and Zenith.AI.StimulusSource the source team, visibility and size.
- **Covers:** Z2-I-AI-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** Test_AIPerception_ConeAndRange in the Test step.
- **Design:** Zenith2/Docs/Design/AI/PublicAPI.md → "Perception properties"; Zenith2/Docs/Design/AI/PublicAPI.md → "Zenith_AIComponents header"

### Z2-R-AI-107 Teams

- **Statement:** Perceivers must perceive only sources whose team bit is set in their PerceiveTeams mask and never their own entity, teams being named by ai.perception.teamNames.
- **Covers:** Z2-I-AI-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_AIPerception_TeamMask in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Senses"; Zenith2/Docs/Design/AI/PublicAPI.md → "Configuration keys"

### Z2-R-AI-108 Sight cones

- **Statement:** A source must be a sight candidate only inside the primary or peripheral cone and range of the perceiver's eye and forward.
- **Covers:** Z2-I-AI-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_AIPerception_ConeAndRange in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Sight algorithm"

### Z2-R-AI-109 Sight line of sight through physics

- **Statement:** Sight candidates must be confirmed by Zenith_PhysicsWorldResource ray casts against the SightBlockers layers, with up to two side rays for sources with a radius.
- **Covers:** Z2-I-AI-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-003
- **Budgets:** Z2-B-AI-002
- **Acceptance evidence:** Test_AIPerception_LineOfSightBlocked and Test_AIPerception_SideRaysAroundCorner in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Sight algorithm"

### Z2-R-AI-110 Sight ray budget

- **Statement:** The per-tick sight ray budget B must be divided among the n due perceivers before the parallel loop as min(8, floor(B / n)) each plus one more in ascending simulation ID to perceivers below 8 while the remainder lasts, never exceeding B or 8 per perceiver, and a perceiver or candidate without a ray must keep its awareness.
- **Covers:** Z2-I-AI-08, Z2-I-AI-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-AI-002
- **Acceptance evidence:** Test_AIPerception_RayBudgetDivision in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Sight algorithm"

### Z2-R-AI-111 Awareness gain and decay

- **Statement:** Visible candidates must gain awareness at the interpolated rate scaled by visibility and the update interval, and unsensed entries must decay, clamped to [0, 1].
- **Covers:** Z2-I-AI-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_AIPerception_GainCurve and Test_AIPerception_DecayAndForget in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Awareness model"; Zenith2/Docs/Design/AI/Agents.md → "Sight algorithm"

### Z2-R-AI-112 Hearing

- **Statement:** Noises recorded through AI.Noise must be heard by every perceiver within loudness times HearingScale in the phase 7 that consumes them, with the falloff of the hearing algorithm.
- **Covers:** Z2-I-AI-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** Test_AIPerception_HearingFalloff in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Hearing algorithm"

### Z2-R-AI-113 Noise capacity

- **Statement:** At most ai.perception.maxNoisesPerTick noises must be kept per tick, the excess dropped in merge order and counted.
- **Covers:** Z2-I-AI-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_AIPerception_NoiseCapacityDropsInMergeOrder in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Hearing algorithm"

### Z2-R-AI-114 Damage, touch and forced awareness

- **Statement:** DAMAGE and FORCE_AWARE stimuli must set awareness to 1 and TOUCH must add 0.5, with the instigator's position as last known, and FORGET must remove the entry.
- **Covers:** Z2-I-AI-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** Test_AIPerception_DamageForcesAlert in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Damage and touch"

### Z2-R-AI-115 Perception memory

- **Statement:** The memory container must keep at most 16 entries sorted by target simulation ID with the eviction, decay and destruction removal rules of "Perception memory", and maintain the primary target.
- **Covers:** Z2-I-AI-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_AIPerception_MemoryEviction and Test_AIPerception_DestroyedTargetForgotten in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Perception memory"

### Z2-R-AI-116 Perception events

- **Statement:** Every awareness level change must emit Zenith.AI.Stimulus and every removal Zenith.AI.TargetForgotten on the perceiver's simulation ID in entry order, derived from committed state against the perception ledger so a rejected buffer delays and never loses an event.
- **Covers:** Z2-I-AI-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** Z2-B-GAME-003
- **Acceptance evidence:** Test_AIPerception_LevelsEmitEvents and Test_AIPerception_RejectedEventsReemitted in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Perception events"; Zenith2/Docs/Design/AI/PublicAPI.md → "Zenith_AIEvents header"

### Z2-R-AI-117 Perception determinism

- **Statement:** Perception results must be identical across worker counts, each perceiver reading only committed state.
- **Covers:** Z2-I-AI-08, Z2-I-AI-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_AIPerception_DeterministicAcrossWorkers in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Determinism rules"; Zenith2/Docs/Design/AI/Agents.md → "Stimulus grid"

### Z2-R-AI-118 Blackboard store

- **Statement:** Zenith.AI.Blackboards must hold typed key-value records per owner entity of six value kinds, at most 64 unexpired keys per owner and 65,536 unexpired records per world, written only by AI.Blackboard command playback, refusing a SET whose interning fails.
- **Covers:** Z2-I-AI-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-SCRIPT-022
- **Acceptance evidence:** Test_AIBlackboard_SetGetEveryKind, Test_AIBlackboard_OwnerLimits, Test_AIBlackboard_InternFullRefused and Test_AIBlackboard_PositionAndVectorKinds in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Blackboard records"; Zenith2/Docs/Design/AI/Agents.md → "Blackboard commands"; Zenith2/Docs/Design/AI/PublicAPI.md → "Zenith_AIWorldResource header"

### Z2-R-AI-119 Blackboard visibility

- **Statement:** Blackboard reads must see the store as the latest sync point left it, a phase-2 write being visible to phases 3 to 10 of the same tick.
- **Covers:** Z2-I-AI-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** Test_AIBlackboard_VisibilityBySyncPoint in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Blackboard reads"

### Z2-R-AI-120 Blackboard expiry

- **Statement:** An expired record must read as absent, count toward no capacity, be pruned at the first blackboard playback of a tick, and be neither saved nor hashed, so a SET is accepted or refused alike live and after a restore.
- **Covers:** Z2-I-AI-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_AIBlackboard_ExpiryReadsAbsent in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Blackboard reads"

### Z2-R-AI-121 Shared blackboards

- **Statement:** A blackboard on a squad entity must be writable by several behaviours through commands applied in merge order, and an owner's entries must be removed when it is destroyed.
- **Covers:** Z2-I-AI-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-051
- **Budgets:** none
- **Acceptance evidence:** Test_AIBlackboard_SharedSquadEntity and Test_AIBlackboard_DestroyedOwnerPruned in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Blackboard records"; Zenith2/Docs/Design/AI/Agents.md → "Blackboard commands"

### Z2-R-AI-122 Blackboard writes are transactional

- **Statement:** A blackboard write from a behaviour invocation that fails must leave no entry, because the write is a command of the rolled-back sub-buffer.
- **Covers:** Z2-I-AI-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-051, D-SCRIPT-TXN refinement (open, Stage 1)
- **Budgets:** Z2-B-ECS-004
- **Acceptance evidence:** Test_AIBlackboard_RollbackDiscardsSet in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Blackboard commands"; Zenith2/Docs/Design/AI/Scripting.md → "Binding functions"

### Z2-R-AI-123 Blackboard payloads carry text

- **Statement:** Blackboard command payloads must carry key and name values as text interned at playback, never interned IDs, and stale entity values must read as nil.
- **Covers:** Z2-I-AI-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** Test_AIBlackboard_StaleEntityValueReadsNil and Test_AIBlackboard_SetGetEveryKind in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Formats.md → "Command payloads"; Zenith2/Docs/Design/AI/Agents.md → "Blackboard reads"

### Z2-R-AI-124 AI script bindings

- **Statement:** Every binding of the binding table must be registered under the AI namespace with its effect class, contexts, side-effect mask and resources, and the table must seal.
- **Covers:** Z2-I-AI-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** Test_AIScript_BindingTableShape and Test_AIScript_QueriesNeedUses in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Scripting.md → "Binding registration"; Zenith2/Docs/Design/AI/Scripting.md → "Binding functions"

### Z2-R-AI-125 AI misuse codes

- **Statement:** AI binding misuse must raise the catalogued codes of the block ZS2500 to ZS2599 that MISUSE-BLOCKS-2 assigns to AI, named by offset in the design, never an assert, with each code's text.
- **Covers:** Z2-I-AI-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** Test_AIScript_MisuseCodes in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Scripting.md → "Misuse codes"

### Z2-R-AI-126 Replayable navigation randomness

- **Statement:** AI.RandomReachablePoint must draw from the invocation's counter-based random stream, so a re-run invocation draws the same point.
- **Covers:** Z2-I-AI-10, Z2-I-AI-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_AIScript_RandomPointUsesInvocationStream in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Scripting.md → "Binding functions"; Zenith2/Docs/Design/AI/Navigation.md → "Random reachable point"

### Z2-R-AI-127 AI Luau libraries

- **Statement:** ZenithAI must ship the strict, stateless library modules @ai/BehaviourTree, @ai/Utility, @ai/StateMachine, @ai/Agent, @ai/Blackboard and @ai/Trace under Zenith2/Engine/ZenithAI/Scripts/, available only to games selecting the AI module.
- **Covers:** Z2-I-AI-10, Z2-I-AI-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-016, D-LUAU-STATELESS (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** Test_AILibraries_StatelessAndStrict in the Test step; the cook type check of the libraries.
- **Design:** Zenith2/Docs/Design/AI/Scripting.md → "Luau library layout"

### Z2-R-AI-128 Behaviour tree semantics

- **Statement:** The behaviour tree library must implement every node kind of its table with the stated status rules, aborting running subtrees with their abort callbacks.
- **Covers:** Z2-I-AI-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** Test_AIBehaviourTree_SequenceSelectorSemantics and Test_AIBehaviourTree_ReactiveAbortCallsAbort in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Scripting.md → "Behaviour tree library"

### Z2-R-AI-129 Behaviour tree state is behaviour state

- **Statement:** A tree's whole runtime state must live in the behaviour's declared State through BT.StateField, so checkpoints and OnRecover resume it exactly.
- **Covers:** Z2-I-AI-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-023, Z2-D-016
- **Budgets:** Z2-B-SCRIPT-015
- **Acceptance evidence:** Test_AIBehaviourTree_StateSurvivesRecover in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Scripting.md → "Behaviour tree library"

### Z2-R-AI-130 Behaviour tree build limits

- **Statement:** BT.Build must refuse more than 256 nodes, a depth above 32 or a composite with more than 16 children at module load with the library's error text.
- **Covers:** Z2-I-AI-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** Test_AIBehaviourTree_BuildLimits in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Scripting.md → "Behaviour tree library"

### Z2-R-AI-131 Utility selection

- **Statement:** The utility library must score options by the compensated product of curve-mapped considerations and select with 10% momentum and option-order ties.
- **Covers:** Z2-I-AI-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** Test_AIUtility_ScoringAndMomentum in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Scripting.md → "Utility library"

### Z2-R-AI-132 Hierarchical state authoring

- **Statement:** The state machine library must implement nested states with exit and enter order through the common ancestor, at most 4 transitions per update, and the onChanged hook.
- **Covers:** Z2-I-AI-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** Test_AIStateMachine_EnterExitOrder, Test_AIStateMachine_TransitionLimit and Test_AIStateMachine_OnChangedMirrors in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Scripting.md → "State machine library"

### Z2-R-AI-133 State machine state is behaviour state

- **Statement:** A machine's active path and entry ticks must live in the behaviour's declared State through SM.StateField.
- **Covers:** Z2-I-AI-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-023
- **Budgets:** Z2-B-SCRIPT-015
- **Acceptance evidence:** Test_AIStateMachine_EnterExitOrder in the Test step (the state field round-trips a checkpoint).
- **Design:** Zenith2/Docs/Design/AI/Scripting.md → "State machine library"

### Z2-R-AI-134 Agent and blackboard helpers

- **Statement:** The agent and blackboard libraries must wrap the bindings and views as their tables specify, including the move-serial helpers for Run coroutines.
- **Covers:** Z2-I-AI-10, Z2-I-AI-09
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-016
- **Budgets:** none
- **Acceptance evidence:** Test_AIIntegration_LuauBrainEndToEnd in Zenith_Integration_AI.
- **Design:** Zenith2/Docs/Design/AI/Scripting.md → "Agent library"; Zenith2/Docs/Design/AI/Scripting.md → "Blackboard library"

### Z2-R-AI-135 End-to-end Luau brain

- **Statement:** A guard behaviour authored with the behaviour tree library, perception and blackboards must patrol, detect, chase, lose and return with the expected event sequence.
- **Covers:** Z2-I-AI-10, Z2-I-AI-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-016, Z2-D-021
- **Budgets:** none
- **Acceptance evidence:** Test_AIIntegration_LuauBrainEndToEnd in Zenith_Integration_AI.
- **Design:** Zenith2/Docs/Design/AI/Tests.md → "Integration suite"; Zenith2/Docs/Design/AI/Scripting.md → "Behaviour tree library"

### Z2-R-AI-136 Significance tiers

- **Statement:** AI.Significance must assign tiers 0 to 3 from the distance to the nearest of at most 4 LOD focus entities with 5% hysteresis, tier 0 when no focus exists.
- **Covers:** Z2-I-AI-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_AILod_TiersAndHysteresis in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Simulation LOD"

### Z2-R-AI-137 Spread update ticks

- **Statement:** An entity must run its full update when (tick + simulation ID) mod its interval is zero, spreading a population evenly over ticks.
- **Covers:** Z2-I-AI-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-AI-002
- **Acceptance evidence:** Test_AILod_DueTicksSpread in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Simulation LOD"

### Z2-R-AI-138 LOD reduces work by tier

- **Statement:** Perception rate, avoidance neighbours, path optimisation and locomotion updates must follow the per-tier table, scaled quantities using the interval, with a tier-3 kinematic agent still moved every tick by its held velocity.
- **Covers:** Z2-I-AI-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-AI-002
- **Acceptance evidence:** Test_AILod_ScaledAwareness and Test_AIAgent_KinematicTierThreeKeepsMoving in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Simulation LOD"; Zenith2/Docs/Design/AI/Agents.md → "Neighbour selection"

### Z2-R-AI-139 LOD is simulation state

- **Statement:** Tiers must be computed from simulation state alone, never a camera or render view, and must be identical across worker counts.
- **Covers:** Z2-I-AI-14, Z2-I-AI-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_AILod_DeterministicAcrossWorkers in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Simulation LOD"; Zenith2/Docs/Design/AI/Agents.md → "Determinism rules"

### Z2-R-AI-140 AI debug views

- **Statement:** Every bit of ai.debug.views must draw its view through Zenith_DebugDraw from the epilogue stage AI.DebugDraw in development builds, and nothing in Shipping.
- **Covers:** Z2-I-AI-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** Test_AIDebug_ViewsDrawRecords in the Test step.
- **Design:** Zenith2/Docs/Design/AI.md → "Debug views"; Zenith2/Docs/Design/AI/PublicAPI.md → "Zenith_AIDebug header"

### Z2-R-AI-141 AI dump command

- **Statement:** The console command ai.dump must write the world's AI state as sorted JSON with the canonical AI state hash.
- **Covers:** Z2-I-AI-12, Z2-I-AI-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** Test_AIScenario_ReproduceFromLog (the printed hash) in Zenith_Integration_AI; Test_AIDebug_DumpSorted in the Test step.
- **Design:** Zenith2/Docs/Design/AI.md → "AI dump command"

### Z2-R-AI-142 Decision trace

- **Statement:** AI.TraceDecision must append to the calling VM's staging buffer, merged once per tick into per-agent development rings observed by no simulation read, unsaved, compiled to an immediate return in Shipping, and every determinism suite must run with tracing off.
- **Covers:** Z2-I-AI-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_AIBehaviourTree_TraceOnlyWhenTracing and Test_AIDebug_TraceStagedPerVM in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Scripting.md → "Debug trace helpers"

### Z2-R-AI-143 Editor AI debugger

- **Statement:** The editor's AI debugger panel must show the selected agent's state, corridor, perception memory, blackboard and decision trace in play worlds.
- **Covers:** Z2-I-AI-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** Test_AIEditor_DebuggerPanelModel in the Test step (win-tools).
- **Design:** Zenith2/Docs/Design/AI.md → "Editor surfaces"

### Z2-R-AI-144 Editor navigation tools

- **Statement:** The editor must provide the navigation bake panel and the gizmos for off-mesh links, obstacles, area volumes and volume regions through the extension interface.
- **Covers:** Z2-I-AI-01, Z2-I-AI-04, Z2-I-AI-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** Test_AIEditor_ExtensionRegisters in the Test step (win-tools).
- **Design:** Zenith2/Docs/Design/AI.md → "Editor surfaces"

### Z2-R-AI-145 AI counters, zones and profiler rows

- **Statement:** Every counter, profiler zone and on-screen row of "Diagnostics and profiling" must be published with its kind each tick.
- **Covers:** Z2-I-AI-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** Z2-B-AI-002
- **Acceptance evidence:** Test_AIDebug_CountersPublished in the Test step.
- **Design:** Zenith2/Docs/Design/AI.md → "Counters"; Zenith2/Docs/Design/AI.md → "Profiler zones"; Zenith2/Docs/Design/AI.md → "On-screen profiler rows"

### Z2-R-AI-146 Canonical AI state hash

- **Statement:** Zenith_AIWorldResource::ComputeStateHash, called on the main thread with no frame running, must hash exactly the state the save section holds in a canonical order independent of slots, add order and insertion history.
- **Covers:** Z2-I-AI-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_AIState_HashCanonical in the Test step.
- **Design:** Zenith2/Docs/Design/AI.md → "Canonical AI state hash"

### Z2-R-AI-147 AI determinism replay matrix

- **Statement:** The AI scenario replayed with 1, 2, 8 and all workers, forced compaction and two chunk capacities on each ECS and physics backend must give equal per-tick world and AI hashes per backend pair.
- **Covers:** Z2-I-AI-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-039, Z2-D-003
- **Budgets:** none
- **Acceptance evidence:** Test_AIDeterminism_ReplayMatrix in Zenith_Integration_AI.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Determinism rules"; Zenith2/Docs/Design/AI/Tests.md → "Save and determinism tests"

### Z2-R-AI-148 AI determinism under script faults

- **Statement:** The AI scenario with injected behaviour errors, budget exhaustion and merge rejections must give equal hashes across the replay matrix.
- **Covers:** Z2-I-AI-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, D-SCRIPT-TXN refinement (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** Test_AIDeterminism_ScriptFaultMatrix in Zenith_Integration_AI.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Determinism rules"

### Z2-R-AI-149 Live AI runs agree

- **Statement:** Two live headless runs of the AI scenario on 2 and 8 workers, each with AI's completion gate releasing every navigation load at its issue tick plus a fixed delay of 8 ticks and no late release, must give equal world and AI hashes at every tick from tick 0, and divergence caused by real I/O timing must be covered by replay instead.
- **Covers:** Z2-I-AI-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_AIDeterminism_LiveRunsAgree in Zenith_Integration_AI; Test_NavRuntime_TileJoinReplayedAtRecordedTick in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Navigation asset requests"; Zenith2/Docs/Design/AI/Navigation.md → "Completion gate"; Zenith2/Docs/Design/AI/Agents.md → "Determinism rules"

### Z2-R-AI-150 Reproducible AI scenarios

- **Statement:** A recorded AI scenario log must replay to its recorded final AI hash, and ai.dump must print the same hash.
- **Covers:** Z2-I-AI-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_AIScenario_ReproduceFromLog in Zenith_Integration_AI.
- **Design:** Zenith2/Docs/Design/AI.md → "AI dump command"; Zenith2/Docs/Design/AI/Tests.md → "Save and determinism tests"

### Z2-R-AI-151 AI save participant

- **Statement:** The AI module must register the participant Zenith.AI.Navigation per play world, declaring its resource reads, re-issuing ASYNC_SOURCE_PATHFINDING and ASYNC_SOURCE_NAVIGATION requests under their recorded completion IDs, and failing the restore with SAVE_ERROR_CORRUPT when SAVE's async section contradicts a re-issue.
- **Covers:** Z2-I-AI-13, Z2-I-AI-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** Test_AISave_InFlightRequestsReissued in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Save and restore algorithm"; Zenith2/Docs/Design/AI/Internals.md → "Save participant class"

### Z2-R-AI-152 AI save section

- **Statement:** The AI save section must follow its thirteen-list layout exactly, refuse every corrupt form with SAVE_ERROR_CORRUPT and migrate older versions forward one step per version.
- **Covers:** Z2-I-AI-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** Test_AISave_SectionRoundTrip and Test_AISave_CorruptSectionRefused in the Test step; Fuzz_NavSaveSection under win-asan.
- **Design:** Zenith2/Docs/Design/AI/Formats.md → "Save section layout"

### Z2-R-AI-153 Exact AI restore

- **Statement:** Every AI capture must be EXACT, and a restore on the build and content that wrote it must rebuild tiles, carved tiles, overlays, links, agents, searches, the ledger and the blackboards from the saved state so the restored world continues bit-identically.
- **Covers:** Z2-I-AI-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_AISave_ActiveSearchStateRestored, Test_AISave_QueuedCarveIsExact and Test_AISave_RestoreContinuesBitIdentical.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Fidelity"; Zenith2/Docs/Design/AI/Agents.md → "Restore"

### Z2-R-AI-154 Restore-time coherent fallback

- **Statement:** Content that changed since the capture, a re-cooked or missing tile or a carved check that differs, must make the affected agents replan and the participant report the restore COHERENT through ReportCoherentRestore, the capture itself staying EXACT.
- **Covers:** Z2-I-AI-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_AISave_RecookedTileReplans and Test_AISave_NoSectionReportsCoherent in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Fidelity"; Zenith2/Docs/Design/AI/Agents.md → "Restore"

### Z2-R-AI-155 World transition discards AI requests

- **Statement:** At a world transition every in-flight path request of the old world must be discarded and never published into any world.
- **Covers:** Z2-I-AI-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** Test_AIIntegration_TransitionDiscardsPathRequests in Zenith_Integration_AI; GAME's Test_Scenario_WorldTransitionWithInFlightAsync.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Cancellation"

### Z2-R-AI-156 Corrupt and missing navigation content

- **Statement:** A corrupt or missing navigation file must fail its load under the FAIL policy, leaving its tiles absent and the agents over them suspended, with the failure counted and logged once per asset.
- **Covers:** Z2-I-AI-01, Z2-I-AI-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** Test_NavAsset_LoaderContract and Test_NavFormats_TileSectionChecks in the Test step.
- **Design:** Zenith2/Docs/Design/AI.md → "Failure table"

### Z2-R-AI-157 Allocation failure during navigation work

- **Statement:** A heap or arena failure during a carve or a search must keep the previous polygons or complete the request as INVALID_REQUEST, never leaving partial state.
- **Covers:** Z2-I-AI-03, Z2-I-AI-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-040, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** Test_AIModule_OutOfMemoryDuringCarve and Test_AIModule_OutOfMemoryDuringSearch in the Test step.
- **Design:** Zenith2/Docs/Design/AI.md → "Failure table"

### Z2-R-AI-158 Android build and device run

- **Statement:** ZenithAI must compile for android-arm64 on every ticket and ZenithAI_Tests must pass on the S24 in the Stage 4 acceptance device run.
- **Covers:** Z2-I-AI-01, Z2-I-AI-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** The Build -Preset android-arm64 step; the Stage 4 acceptance device run record of ZenithAI_Tests through MOD's test APK.
- **Design:** Zenith2/Docs/Design/AI/Tests.md → "Gate steps for AI diffs"; Zenith2/Docs/Design/AI.md → "ZENITH_SUPPORTED matrix"

### Z2-R-AI-159 AI rows confirmed at Stage 4

- **Statement:** Z2-B-AI-001, Z2-B-AI-002 and Z2-B-AI-003 must be first measured in W-Characters and W-OpenWorld budget runs at the Stage 4 acceptance on the reference desktop and the S24.
- **Covers:** Z2-I-AI-07, Z2-I-AI-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-065
- **Budgets:** Z2-B-AI-001, Z2-B-AI-002, Z2-B-AI-003
- **Acceptance evidence:** The Stage 4 acceptance evidence records of the W-Characters and W-OpenWorld budget runs.
- **Design:** Zenith2/Docs/Design/AI.md → "Rows owned"; Zenith2/Docs/Design/AI/Tests.md → "Benchmarks and device runs"

### Z2-R-AI-160 OpenWorldSlice AI at scale

- **Statement:** The OpenWorldSlice Stage 4 playable milestone must run its AI and navigation at scale (streamed navigation, crowds, perception-driven encounters) within the AI rows on both platforms.
- **Covers:** Z2-I-AI-07, Z2-I-AI-02, Z2-I-AI-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-021
- **Budgets:** Z2-B-AI-001, Z2-B-AI-002, Z2-B-AI-003
- **Acceptance evidence:** The OpenWorldSlice Stage 4 milestone acceptance run against its playthrough script.
- **Design:** Zenith2/Docs/Design/AI.md → "Stage table"; Zenith2/Docs/Design/AI.md → "Rows measured against"

### Z2-R-AI-161 Cooked reader fuzzing

- **Statement:** The tile, layers, volume, source-intermediate and save-section readers must each have a libFuzzer target that runs in the win-asan sanitizer step without a crash or an out-of-bounds read.
- **Covers:** Z2-I-AI-01
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-011, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** Fuzz_NavTileSection, Fuzz_NavLayersSection, Fuzz_NavVolumeSection, Fuzz_NavSourceIntermediate and Fuzz_NavSaveSection under the Sanitizers -Preset win-asan step and the Stage 6 acceptance.
- **Design:** Zenith2/Docs/Design/AI/Tests.md → "Failure injection tests"

### Z2-R-AI-162 AI budget qualification

- **Statement:** The three AI rows must be met on the S24 in its ordinary state and on the reference desktop in the Stage 6 qualification runs of W-Characters and W-OpenWorld.
- **Covers:** Z2-I-AI-07, Z2-I-AI-14
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-065
- **Budgets:** Z2-B-AI-001, Z2-B-AI-002, Z2-B-AI-003
- **Acceptance evidence:** The Stage 6 qualification evidence records.
- **Design:** Zenith2/Docs/Design/AI.md → "Rows owned"; Zenith2/Docs/Design/AI/Tests.md → "Benchmarks and device runs"

### Z2-R-AI-163 Navigation asset requests

- **Statement:** Every navigation file must reach simulation only through an ASYNC_SOURCE_NAVIGATION (value 13) publication of MOD's module publication sink: AI.NavIngest's merge node must allocate one completion ID per NEED supplier slot in collected order within ai.nav.maxLoadsInFlight, deferring a need the sink refuses with PUBLICATION_SINK_ERROR_FULL, every drained completion must pass AI's completion gate, which releases it at once in production, before its load record advances, each ID must be completed exactly once, and a tile must join in the phase 7 after its publication.
- **Covers:** Z2-I-AI-02, Z2-I-AI-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019
- **Budgets:** Z2-B-AI-001
- **Acceptance evidence:** Test_NavRuntime_TileJoinsAtPublication, Test_NavRuntime_TileJoinReplayedAtRecordedTick, Test_NavRuntime_AbandonedTileLoadCompletedOnce, Test_NavRuntime_FailedTileLoadNotRetried and Test_NavRuntime_LoadsInFlightCapped in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Navigation asset requests"; Zenith2/Docs/Design/AI/Navigation.md → "Completion gate"; Zenith2/Docs/Design/AI/Internals.md → "Navigation load records"

### Z2-R-AI-164 Restore waits listed

- **Statement:** While IsRestoreReady is false, ListRestoreWaits must list every restore-queue navigation load it waits for in ascending asset GUID, so a synchronous restore driver completes a resume before the first tick.
- **Covers:** Z2-I-AI-13, Z2-I-AI-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_AISave_SynchronousRestoreListsNavLoads in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Restore"; Zenith2/Docs/Design/AI/Internals.md → "Save participant class"

### Z2-R-AI-165 Search state saved whole

- **Statement:** Every active path and volume search must be saved as its whole state (nodes, open heap order, best node, expansions and tile or volume stamps) and rebuilt with references re-resolved by key, so its next step after a restore equals the uninterrupted run's whatever overlay or tile changes it straddled.
- **Covers:** Z2-I-AI-05, Z2-I-AI-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_NavPath_SnapshotRoundTrip, Test_AISave_ActiveSearchStateRestored, Test_AISave_SearchStraddlingOverlayChange and Test_AISave_ActiveVolumeSearchRestored in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Restore"; Zenith2/Docs/Design/AI/Formats.md → "Save section layout"; Zenith2/Docs/Design/AI/PublicAPI.md → "Zenith_NavQuery header"

### Z2-R-AI-166 Carve state restored exactly

- **Statement:** The carve queue with its first-queued ticks, each carved tile's version and last-rebuild obstacle set, and every obstacle's applied record must be saved and restored, dirty obstacles must be detected against their applied records never against change bits, and the queue must drain under the per-tick cap from the first restored tick.
- **Covers:** Z2-I-AI-03, Z2-I-AI-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_AISave_QueuedCarveIsExact, Test_AISave_CarvedTwiceRestoresExact and Test_NavObstacle_DirtyByAppliedRecord in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Carve procedure"; Zenith2/Docs/Design/AI/Navigation.md → "Mark procedure"; Zenith2/Docs/Design/AI/Agents.md → "Restore"

### Z2-R-AI-167 Every world-local AI position shifted

- **Statement:** At an origin shift, every world-local position AI holds must be shifted once, at the sync point: tile and volume origins, agent records, path descriptors and path-inbox payloads with active searches restarted, obstacle applied records, link registrations' entity positions, noises, POSITION blackboard values and every PerceivedTarget 3 of every Memory container by AI, and Agent 20 and 44 and PerceptionState 23 by SCENE through PROPERTY_FLAG_WORLD_POSITION, with carved records stored tile-local so a shift changes none of them.
- **Covers:** Z2-I-AI-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Test_NavRuntime_OriginShiftKeepsCorridors in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Origin shift"; Zenith2/Docs/Design/AI/PublicAPI.md → "Agent properties"; Zenith2/Docs/Design/AI/PublicAPI.md → "Perception properties"

### Z2-R-AI-168 Canonical neighbour order

- **Statement:** Each tile's external-link list must be kept sorted by (polygon, edge, neighbour key, neighbour polygon), and every neighbour iteration of the search, surface movement, raycast and random-point flood must use that order, so results are independent of tile add order.
- **Covers:** Z2-I-AI-05, Z2-I-AI-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_NavRuntime_TileConnectionBothDirections and Test_NavQuery_MoveAlongSurfaceIndependentOfAddOrder in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Tile connection procedure"; Zenith2/Docs/Design/AI/Navigation.md → "Surface movement"

### Z2-R-AI-169 Events re-derived after a rejection

- **Statement:** AI.Locomotion and AI.Perception must register merge functions that commit last-written and last-emitted values only for accepted buffers, so every command, MoveCompleted, Stimulus and TargetForgotten a rejected buffer lost is re-sent from committed state the next tick.
- **Covers:** Z2-I-AI-07, Z2-I-AI-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-051
- **Budgets:** Z2-B-GAME-003, Z2-B-ECS-004
- **Acceptance evidence:** Test_AIPerception_RejectedEventsReemitted, Test_AIAgent_RejectedCompletionReemitted and Test_AIAgent_RejectedBufferResends in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Frame-graph placement"; Zenith2/Docs/Design/AI/Agents.md → "Perception events"; Zenith2/Docs/Design/AI/Internals.md → "Perception ledger"

### Z2-R-AI-170 Root-motion agents

- **Statement:** An agent whose character controller is in a root-motion mode must record no controller velocity or yaw input, must write its intent to Agent 43, 44 and 48 every moving tick, must track the achieved position, and must strike as stuck only when its intended displacement in the window reaches 1 m.
- **Covers:** Z2-I-AI-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** Test_AIAgent_RootMotionControllerFollowsCorridor in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Locomotion output"; Zenith2/Docs/Design/AI/Agents.md → "Stuck detection"; Zenith2/Docs/Design/AI/Scripting.md → "Authoring conventions"

### Z2-R-AI-171 Tool-side authoring surface

- **Statement:** The functions and types of Zenith_AuthoringAI.h must live in ZenithAuthoring under Zenith2/Tools/Authoring/Internal/AI/ with their declared signatures, naming no ZenithCook or ZenithEditor type, with the cook-step registration in ZenithCook and the document types registered only through Zenith_RegisterAIDocumentTypes.
- **Covers:** Z2-I-AI-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-024, Z2-D-047
- **Budgets:** none
- **Acceptance evidence:** Test_NavCook_SourceGeometryFromEnv, Test_NavCook_CellContentRecords, Test_NavCook_DocumentMigrationChain and Test_NavCook_GatherInputsThroughSource in ZenithAuthoring_Tests (win-tools); the Libgraph step.
- **Design:** Zenith2/Docs/Design/AI/Internals.md → "Zenith_AuthoringAI header"; Zenith2/Docs/Design/AI.md → "Tool-side files"

### Z2-R-AI-172 Detail mesh limits representable

- **Statement:** A polygon must carry at most 127 detail vertices and 255 detail triangles, so every uint8 local detail index and count of the tile section can represent it, and the tile reader must refuse a polygon beyond either limit in check T7.
- **Covers:** Z2-I-AI-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Test_NavBuild_Limits and Test_NavFormats_TileSectionChecks in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Bake limits"; Zenith2/Docs/Design/AI/Formats.md → "Tile section"

### Z2-R-AI-173 Non-agent characters avoided

- **Statement:** Every character controller without Zenith.AI.Agent must enter the crowd grid as a non-avoiding neighbour with its own velocity, radius and height, so agents take full responsibility for avoiding it, and must receive no AI record or command.
- **Covers:** Z2-I-AI-06, Z2-I-AI-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_AIOrca_AvoidsPlayerController in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Crowd grid"; Zenith2/Docs/Design/AI/Agents.md → "ORCA constraints"

### Z2-R-AI-174 Path inbox and queue capacity

- **Statement:** Script path requests must enter a path inbox written only on the world's merge chain and drained by AI.NavRequests, and module start must refuse ai.path.maxQueuedRequests below ai.agent.maxAgents plus the pathfinding value of Z2-B-SCRIPT-026.
- **Covers:** Z2-I-AI-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** Z2-B-SCRIPT-026
- **Acceptance evidence:** Test_AIModule_Start_RejectsQueueSmallerThanDemand in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Request queue"; Zenith2/Docs/Design/AI/Internals.md → "Path request consumer"

### Z2-R-AI-175 Search expansion step

- **Statement:** Each Step pop must close the node, finish COMPLETE at the end polygon, and relax each neighbour in canonical order by the table of "Search state and slicing": create absent nodes until the pool's first exhaustion ends the search PARTIAL, replace the position, g and parent of an open node reached with a lower g and re-order the heap, reopen a closed node reached with a lower g, change nothing for an equal or higher g, close a popped dead link node without expanding it, and keep the best node minimal by (h, g, tie-break key) over polygon nodes only.
- **Covers:** Z2-I-AI-05, Z2-I-AI-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_NavPath_RelaxationRules, Test_NavPath_NodePoolExhaustion and Test_NavPath_SlicedEqualsUnsliced in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Search state and slicing"; Zenith2/Docs/Design/AI/Navigation.md → "Search graph"; Zenith2/Docs/Design/AI/Navigation.md → "Tie-breaking"

### Z2-R-AI-176 Search limits named

- **Statement:** A search's reach must be bounded only by its node pool, the tick cap and the corridor span: the tile and volume stamp tables must hold min(nodes, loaded tiles or volumes) entries so they never end a search, the arena block of B(N) = 40 N + 16 S + L(N) + L(S) bytes with both lookups reserved at Begin and RestoreSnapshot must never grow, every result must name the limit that ended it (NONE, NODE_POOL, TICK_CAP or CORRIDOR) in the C++ result and the script payload, and the N7 and N14 conformance expectations must follow from the cost oracle's asserted preconditions and, for long pairs, from limit consistency alone.
- **Covers:** Z2-I-AI-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-AI-001
- **Acceptance evidence:** Test_NavPath_StampTableNeverBinds, Test_NavPath_TruncatedCorridor and Test_PathService_CancelReleasesNodePool in the Test step; Test_NavConformance_N07_Terrain and Test_NavConformance_N14_ReplanBudget in ZenithAI_Conformance.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Search state and slicing"; Zenith2/Docs/Design/AI/Navigation.md → "Partial results"; Zenith2/Docs/Design/AI/Navigation.md → "Conformance scenarios"

### Z2-R-AI-177 Long moves continue

- **Statement:** A move whose plan is TRUNCATED, or PARTIAL because of the node pool or the tick cap, must be followed and continued by replans queued within 8 m of the corridor's end while MoveStatus stays MOVING, holding at the corridor's end without arriving until the result, whatever ALLOW_PARTIAL; only such a continuation replan must end at least 1 m nearer the target or fail UNREACHABLE, every other replan re-recording the distance, and the continuation state must be cleared on every MoveSerial change, teleport, medium change and ragdoll recovery.
- **Covers:** Z2-I-AI-05, Z2-I-AI-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Test_AIAgent_LongMoveContinues, Test_AIAgent_ContinuationStateCleared, Test_AIAgent_ContinuationOneMetreRuleScope and Test_AIAgent_StateMachineTable in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Agents.md → "Path corridor"; Zenith2/Docs/Design/AI/Agents.md → "Agent state machine"; Zenith2/Docs/Design/AI/Navigation.md → "Partial results"

### Z2-R-AI-178 Connection overflow is a set function

- **Statement:** Under list overflow a candidate connection must exist exactly when it ranks below the capacity in the canonical rankings of both of its tiles, recomputed for the changed tile and its four neighbour columns at every add and remove, so the lists are equal across add orders, removes and restores.
- **Covers:** Z2-I-AI-02, Z2-I-AI-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_NavRuntime_TileConnectionBothDirections and Test_NavConformance_N13_ReloadOrder in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Tile connection procedure"; Zenith2/Docs/Design/AI/Navigation.md → "Tile remove procedure"

### Z2-R-AI-179 Rebase bookkeeping saved

- **Statement:** AI must store the applied rebase count and the applied origin, apply a missed shift at ingest as the world origin minus the applied origin, save both in the section header and restore them before any tile is added, refusing with SAVE_ERROR_CORRUPT a section inconsistent with the restored world origin.
- **Covers:** Z2-I-AI-02, Z2-I-AI-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_AISave_RestoreAfterRebaseNoSecondShift and Test_NavRuntime_LateShiftUsesAppliedOrigin in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Origin shift"; Zenith2/Docs/Design/AI/Agents.md → "Restore"; Zenith2/Docs/Design/AI/Formats.md → "Save section layout"

### Z2-R-AI-180 Link nodes in saved corridors

- **Statement:** Zenith_NavPolyRef must reserve bit 63 for a link-node form with a link slot salt, and corridor entries, corner references, search nodes and the canonical hash must represent a link node by (link entity simulation ID, class) in the tagged 16-byte save form, so an agent saved with a link in its corridor continues bit-identically.
- **Covers:** Z2-I-AI-04, Z2-I-AI-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_NavPolyRef_PackUnpack, Test_AISave_SectionRoundTrip and Test_AISave_CorridorThroughLinkRestored in the Test step.
- **Design:** Zenith2/Docs/Design/AI/PublicAPI.md → "Zenith_AITypes header"; Zenith2/Docs/Design/AI/Formats.md → "Save section layout"; Zenith2/Docs/Design/AI/Agents.md → "Restore"

### Z2-R-AI-181 Volume search stamps

- **Statement:** A volume search must stamp each volume it creates a unit in by slot and salt in a table of min(nodes, 256) entries, become stale only when a stamped volume is removed or its slot re-used, and save each stamp as a volume index restored from the restored volumes.
- **Covers:** Z2-I-AI-16, Z2-I-AI-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_AISave_ActiveVolumeSearchRestored and Test_NavVolume_SearchDeterministic in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Volumes.md → "Volume search"; Zenith2/Docs/Design/AI/PublicAPI.md → "Zenith_NavVolume header"; Zenith2/Docs/Design/AI/Formats.md → "Save section layout"

### Z2-R-AI-182 Supplier records by value

- **Statement:** AI must keep a saved and hashed supplier record per navigation entity in an active region and decide needs, joins, failures and removals by comparing the current handle GUIDs with it by value, never by ECS change bits, so a manifest is requested once and a restored world keeps every supplier, tile and failed slot.
- **Covers:** Z2-I-AI-02, Z2-I-AI-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** Test_NavRuntime_ManifestRequestedOnce, Test_NavRuntime_FailedTileLoadNotRetried and Test_AISave_RestoreKeepsSuppliersWithChangeBitsSet in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Supplier records"; Zenith2/Docs/Design/AI/Navigation.md → "Tile add procedure"; Zenith2/Docs/Design/AI/Formats.md → "Save section layout"

### Z2-R-AI-183 Deterministic transcendentals

- **Statement:** No AI simulation path, bake or tracked output may call a CRT transcendental: the locomotion yaw and rotation, the sight cones, the volume pitch, the bake slope threshold and the utility logistic curve must use CORE's Zenith_Sin, Zenith_Cos, Zenith_Atan2 and Zenith_Exp (ruling X314-FMA3, pending CORE's X314 follow-up).
- **Covers:** Z2-I-AI-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_AIUtility_ScoringAndMomentum, Test_AIAgent_TurnRateLimit and Test_NavBake_TileBytesIdenticalAcrossRuns in the Test step; review of every AI simulation and bake source for a <cmath> transcendental call.
- **Design:** Zenith2/Docs/Design/AI.md → "Assumed interfaces"; Zenith2/Docs/Design/AI/Agents.md → "Locomotion output"; Zenith2/Docs/Design/AI/Agents.md → "Sight algorithm"

### Z2-R-AI-184 Navigation product declaration shared with WORLD

- **Statement:** AI must provide Zenith_AIDeclareNavigationProducts with a caller-given manifest key and product sink, accept scene product entries with a binary64 offset and volumes with an explicit index, and bake a cooked scene product with its offset added in binary64 before the tile-local translation, so WORLD's derived documents produce the same blobs as an equal document file.
- **Covers:** Z2-I-AI-01, Z2-I-AI-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** Test_NavCook_DeclareProductsInProcess in ZenithAuthoring_Tests and Test_NavCook_InputsFromCookedSceneWithOffset in ZenithCook_Tests (win-tools).
- **Design:** Zenith2/Docs/Design/AI/Internals.md → "Zenith_AuthoringAI header"; Zenith2/Docs/Design/AI/Formats.md → "Navigation document schema"; Zenith2/Docs/Design/AI/Navigation.md → "Bake inputs"

### Z2-R-AI-185 Placed entities by world pose

- **Statement:** Obstacles, link entities, LOD foci, move-target entities and stimulus sources must be placed by their world pose, a root's Zenith.LocalTransform and a child's previous-tick Zenith.WorldTransform, read through ReadAnyRow declarations.
- **Covers:** Z2-I-AI-03, Z2-I-AI-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** Test_NavObstacle_ChildObstacleWorldPose and Test_NavRuntime_OriginShiftKeepsCorridors in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Obstacle modes"; Zenith2/Docs/Design/AI/Agents.md → "Frame-graph placement"; Zenith2/Docs/Design/AI/Agents.md → "Path corridor"

### Z2-R-AI-186 Dead link nodes

- **Statement:** A link node whose link was unregistered while it was in a search or a corridor must be a dead link node: never a neighbour, closed without expansion when popped, ordered in the heap by a key read from its own record, cut from the corridor by Finish as a PARTIAL result flagged uNAV_PATH_RESULT_FLAG_DEAD_LINK that an agent plan re-queues, saved as node reference entry kind 3 or search node record kind 4 and restored as MakeLinkNode(0, 0), so a capture holding one stays EXACT.
- **Covers:** Z2-I-AI-04, Z2-I-AI-13
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** Test_NavPath_UnregisteredLinkNodeIsDead, Test_AISave_DeadLinkNodeRestored and Test_NavPolyRef_PackUnpack in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Search graph"; Zenith2/Docs/Design/AI/Navigation.md → "Search state and slicing"; Zenith2/Docs/Design/AI/Formats.md → "Save section layout"; Zenith2/Docs/Design/AI/Agents.md → "Restore"
