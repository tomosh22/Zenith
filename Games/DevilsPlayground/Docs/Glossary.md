# Devil's Playground — Glossary

**Document purpose:** Authoritative definitions for terms used across the planning pack. New agents and the human reviewer reference this file when terminology is ambiguous. If a term in a doc means something different than its entry here, **the entry here is the source of truth** — update the doc to match, not the glossary.

**Added 2026-05-11** per peer-review consensus that load-bearing jargon was being used without a single definition source.

---

## Game / design terms

**Anchor (frost-anchor).** The world-space position from which the demon can possess. Initially the standing stones (Tuning.json `possession.anchor_initial_position`). After a possession ends in **death (burn-out OR apprehension)**, the anchor moves to where that body fell. **Voluntary switch causes a faint, not a death — the anchor does NOT move on faint** (semantics reconciled with GDD §4.1 on 2026-05-12 round-5 review; earlier version of this glossary entry incorrectly said "faint OR death"). Players see the anchor as a faint frost decal on the ground. The 15 m possession range is measured from this point. See GDD §4.1.

**Aelfric / The Pursuivant / The Witch-finder.** The antagonist. A single AI agent that hunts the demon by sight, sound, and (post-MVP) demon-scent + hounds. Voiced as Aelfric Crowe in narrative; in code, type name is `Priest_Behaviour`. The variants (Cautious, Cruel, Drunk) are post-MVP. See GDD §2.4 and §4.5.

**Apprehend.** Aelfric's loss-inducing action. When he comes within `priest.apprehend_range_m` of the possessed body for `priest.apprehend_channel_s`, he restrains them; the run ends with `DP_OnRunLost` cause = `Apprehended`. Interruptible by voluntary possession switch. See GDD §4.5; test in TestPlan §2.3.

**Archetype.** A villager class. Each villager entity in the world is one of 24 designed archetypes (4 in MVP scope). Archetypes share a body mesh but differ in life timer, movement speed, possession channel, abilities, and silhouette tint. Source of truth: `Config/Archetypes.json`. See GDD §6.2.

**Body / vessel.** A villager-controlled entity. Synonymous in casual prose; both refer to a `DPVillager_Behaviour` instance that can be possessed. "Pool" = the set of all currently-living vessels in the active Night.

**Burn-out / Burn.** The 30-second possession life timer expiring. Triggers `DP_OnVillagerDied`. Differs from voluntary switch (which causes a faint, not a death). Differs from apprehension (which is a run-loss event, not a body death). See GDD §4.1 and §4.7.

**Campaign.** The 7-Night narrative arc. **Post-MVP** — MVP ships with a single playable Night (one scene). See GDD §2.3.

**Channel.** A time-gated action that can be interrupted. Possession channel: 0.8 s for Devout archetype, 0 s for everyone else. Apprehension channel: 3 s. Pickup channel: 1 s for reagents, 0 s for tools. Inscription channel: 1.5 s at pentagram. Interaction with door/chest/forge/bell has its own channel. See Tuning.json `interactables` section.

**Demon-scent.** A per-body floating-point value in [0.0, 1.0]. Incremented by 0.3 on each successful possession of a given body. Decays at 0.05/sec. Hounds (post-MVP) bark within 6 m of a body whose scent exceeds 0.5. See GDD §4.5 and Tuning.json `possession.demon_scent_*`.

**Devout.** A villager archetype that resists possession (0.8 s channel) but whom Aelfric trusts (won't interrogate). See `Config/Archetypes.json#Devout`.

**Dawn.** The Night's hard time limit. When the dawn timer expires (`night.dawn_timer_s` = 600 s in MVP), the run ends with `DP_OnRunLost` cause = `Dawn`. See GDD §5.1.

**Drop verb.** G-key action. Releases the possessed body's held item to the ground at the body's feet. Enables hand-off chains. See GDD §4.3.

**Faint.** The state a body enters when voluntarily switched away from. Different from death: 10-second recovery timer, then the body returns to the un-possessed villager pool. Does NOT trigger `DP_OnVillagerDied`. See GDD §4.1.

**Fog of war.** The visibility system. Per-frame clear-and-rebuild: each possessed villager carves an 8 m visibility hole, each un-possessed villager carves a 1.5 m hole (showing their position), each light source carves a hole sized to its range. Memory fog: tiles seen 0–10 s ago = visible, 10–30 s ago = dim, never-seen = grey. See GDD §4.6 and Tuning.json `fog_of_war`.

**Hand-off chain.** The core gameplay loop. A body picks up an item, walks toward a destination, drops the item (or burns out near it). A different body, possessed next, picks up the item from the ground and continues. Mastery of chains = mastery of the game. See GDD §3.

**Inscription / Inscribe.** The win action at the pentagram. A possessed body holding an objective reagent interacts with the pentagram; after the 1.5 s channel, the reagent is consumed and the win mask gains one bit. Five inscriptions = `DP_OnVictory`. See GDD §4.4 and `DPPentagram_Behaviour.h`.

**Knot.** The post-MVP roguelite meta-progression currency. Earned per reagent inscribed + chain bonus + hound-neutralisation. Spent at the Liminal hub. **Post-MVP only.** See GDD §5.4.

**Liminal.** The post-MVP roguelite meta-progression hub. Between-Night space where the three hermits offer permanent unlocks paid for in Knots. **Post-MVP only.** See GDD §5.4.

**Night.** One run. ~10 minutes in MVP (vs. GDD's 25–35 min target — tuning later). Starts when the player loads the gameplay scene; ends in win (victory), loss (apprehended/dawn/no-vessels), or quit. The MVP ships one Night design; the campaign's 7-Night arc is post-MVP. See GDD §5.

**Objective / Reagent.** The five items per Night that must be inscribed at the pentagram to win. 14 reagents designed total; 5 drawn per Night (procedural shuffle is post-MVP — MVP uses a fixed 5 per the canonical Night design). Source of truth: `Config/Reagents.json`. See GDD §6.3.

**Pentagram.** The win-condition entity. A single instance per Night, in the chapel. Interactable; consumes objective reagents on inscribe. See `DPPentagram_Behaviour.h`.

**Possession / possessed villager.** The act of the demon entering a body. Mechanically: `DP_Player::SetPossessedVillager(id)`. The possessed villager is the player's avatar — WASD-controlled, with the 30-second life timer ticking. Only one villager can be possessed at a time. See GDD §4.1.

**Possession cooldown.** A 1.5 s untethered period after voluntary switch, during which the demon cannot possess any body. Prevents click-spam strategies. See Tuning.json `possession.cooldown_after_voluntary_switch_s`.

**Possession range.** 15 m from the current anchor. Outside this radius villagers are unpossessable (visually greyed). See GDD §4.1.

**Reagent.** See "Objective."

**Run.** Synonymous with Night. One full play session ending in win or loss.

**Sprint.** Held-Shift movement at 12 m/s (vs. 8 m/s jog). Costs 3 s/s extra life-timer drain. See GDD §4.2.

**Vessel.** See "Body."

**Voluntary switch.** Player chose to leave the current body (faints it) and possess another. Right-click in MVP. Differs from burn-out (death) and apprehension (run-loss). See GDD §4.1.

**Walk-quiet.** Held-Ctrl walking at 4 m/s. Reduces the moving villager's footstep loudness by half (which halves the *effective* hearing range against that villager). See Tuning.json `movement.walk_footstep_loudness_multiplier`.

---

## Engine / engineering terms

**Audio bus.** `Zenith_AudioBus` — the engine's not-yet-implemented audio system. For MVP, only the test-instrumentation surface exists: `GetEmittedSoundsForTest()` records event emissions for test assertions; actual audio playback is post-MVP. See TestPlan §0.4.

**Behavior tree (BT).** Aelfric's AI architecture. Root = Selector → [Apprehend, Pursue, Investigate, Patrol]. First matching branch wins. Implemented via Zenith engine's BT system (`AI/BehaviorTree/`). See `Priest_Behaviour.h`.

**BT blackboard (BB).** Per-AI-agent key-value store. Aelfric's BB keys: `TargetWithDevil`, `InvestigatePos`, `HasInvestigatePos`, `PatrolTarget`, `SuspicionRadius`, `SelfActor`. See `Source/PublicInterfaces.h` and DP_AI namespace.

**Editor automation.** The declarative scene-authoring DSL. `Project_RegisterEditorAutomationSteps` queues `AddStep_*` calls that build a scene. Tools-build only. Non-tools loads pre-baked `.zscen` files. See AgentBriefing §4.9 and the engine's `Editor/Zenith_EditorAutomation.h`.

**`DP_*` namespaces.** The public-interface contract surface for cross-behaviour communication. `DP_Player`, `DP_Items`, `DP_Interactables`, `DP_AI`, `DP_Fog`, `DP_Win`, `DP_Query`. Behaviours talk only through these; they never reach into each other's headers. See `Source/PublicInterfaces.h`.

**Frame budget.** A test's `m_iMaxFrames`. The harness aborts after that many frames if Step hasn't returned false. Default 600 (10 s @ 60Hz fixed-dt).

**Gym scene.** A small focused test scene that exercises one subsystem. The prototype has Gym_Items, Gym_Noise, Gym_Doors, Gym_Forge. MVP adds Gym_Archetypes_MVP. See `Games/DevilsPlayground/CLAUDE.md`.

**Orchestrator.** The primary Claude Code instance in a fresh session. Single source of truth for state files; sole invoker of MSBuild/tests/game. Dispatches subagents for work. See [OrchestratorPlaybook.md](OrchestratorPlaybook.md).

**Render bus.** `Zenith_RenderBus::GetSubmittedDrawCallsForTest()` — surrogate render-output observability for tests. Records draw call submissions. See TestPlan §0.4.

**Save bus.** `Zenith_SaveSystem::GetWrittenBlobsForTest()` — surrogate disk observability for tests. Tests inject blobs via `SetReadbackBlob`, capture writes via `GetWrittenBlobs`. See TestPlan §0.4.

**Sharpmake.** The C# DSL Zenith uses to generate Visual Studio solution files. Must be re-run after adding new `.cpp` files: `cd Build && cmd /c '.\Sharpmake_Build.bat < nul'`.

**Subagent.** A spawned `Agent` tool invocation. Roles: Researcher (Explore type), Planner (Plan type), Implementer / Test Author / Reviewer / Linter / Doc Maintainer (general-purpose type). See [OrchestratorPlaybook.md](OrchestratorPlaybook.md) §3.

**Tinted-cube system.** `DPMaterials::GetOrCreateColouredVariant` — the existing placeholder strategy where items/villagers are visually distinguished by hue alone. Preserved as a debug mode even in shipping. See AssetManifest §15.

**`*_True` / `*_False` configurations.** Build configurations with/without `ZENITH_TOOLS` defined. Tools builds include the editor automation and scene authoring; non-tools builds load pre-baked `.zscen` files. Both are buildable as of 2026-05-10 per project memory.

**`.zmodel` / `.zmesh` / `.zskel` / `.zanim` / `.zmtrl` / `.ztxtr` / `.zdata` / `.zscen` / `.zaudio` / `.znavmesh`.** Zenith's asset file extensions. See AssetManifest §0.3.

**`ZENITH_INPUT_SIMULATOR` define.** Conditional-compile guard for test-build code. Every automated test wraps `#include` and registration in `#ifdef ZENITH_INPUT_SIMULATOR ... #endif`.
