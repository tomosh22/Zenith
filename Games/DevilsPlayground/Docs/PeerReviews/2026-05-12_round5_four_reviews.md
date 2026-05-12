# Peer Reviews — Round 5, 2026-05-12

Four additional reviews after the round-4 reconciliation. Key new findings:

1. **MvpRoadmap §1.2 has a duplicate navmesh section.** The round-4 update added the new "integrate existing generator" version (lines 98–134) but did not delete the old "runtime generator from scratch" block (lines 135–153 onward). Both are labelled `#### Primary path` and define `MVP-1.2.0` with contradictory timeboxes (2-day vs 5-day). Three reviewers independently caught this. **It is the single most damaging bug** because an agent reading top-to-bottom encounters MVP-1.2.0 twice with different scopes.

2. **TestPlan Tier 2 is contaminated with post-MVP tests** — hounds (`Test_P2Hounds_*`), variants (`Test_P2Variant_*`), charms (`Test_P2Charm_*`), distractions (`Test_P2Distract_*`), Liminal (`Test_P2Liminal_*`) are all listed in Tier 2 without MVP/post-MVP tags. MVPScope §1.4 says only a *subset* of Tier 2 is in MVP. An agent will author tests for systems that don't exist yet, creating red CI.

3. **MVP-4.3.1 circular dependency** — task says "tune so the bot wins ~50%" but the bot is post-MVP backlog. The MVP acceptance gate references a system that's explicitly out of MVP scope.

4. **ManualSetupChecklist deadlock** — Phase 0.0's smoke PR requires branch protection (MVP-0.0.6) which requires `dp-build`/`dp-tests` workflows (MVP-0.0.2/0.0.3) to exist as named checks first. The current checklist asks humans to tick branch-protection BEFORE the workflows exist.

5. **OrchestratorPlaybook §8 still has a JsonReader residue** at line 721 in the DecisionLog example showing what the agent's wrap-up entry would say.

6. **AssetManifest §2.1.3 still says "~430 clips"** — round-4 fixed §0.1 to 482 but missed the inline §2.1.3 number.

7. **MEMORY.md line 3 still says "Non-tools build broken"** — the linked memory file was updated 2026-05-10 to "fixed" but the index summary wasn't.

8. **Glossary "anchor" contradicts GDD** — Glossary says anchor moves on faint OR death; GDD §4.1 says anchor moves on death only (voluntary switch is faint-not-death).

9. **Walk-quiet semantics ambiguous** — MVPScope §1.1 says walk-quiet "halves Aelfric's hearing range." Tuning.json's `walk_footstep_loudness_multiplier` halves the moving villager's footstep loudness instead. These compute to similar effects against this villager but mean different things for the perception system.

10. **`EmittedSound` spec missing `radius` (round-4 fixed MvpRoadmap but not TestPlan §0.4) and missing `source_entity_id`** — Tier 1 tests need the latter to distinguish villager footsteps from Aelfric's own.

11. **Synty rejection rationale is technically wrong** — round-4 framed it as "Unity-only licensing" but the actual Synty one-time EULA permits engine-agnostic commercial use. Real rejection reason is the no-external-spend policy.

12. **Substitution tests need gameplay-timing contracts**, not loose duration ranges — if pickup_low is 0.8 s in spec and gameplay code hardcodes 0.75 s, a final asset at 0.85 s passes the asset test but breaks hand-off chain timing.

13. **`DevilsPlayground.cpp` race risk for parallel subagents** — every new behaviour requires editing this file's includes; parallel implementers would corrupt it.

14. **Five rounds of consistent recommendation for a doc-linter in Phase 0** — the pattern of stale-reference drift is too consistent to defer further.

15. **No LFS in Phase 0** — Phase 3 binary imports will bloat the repo before LFS can be retroactively applied.

## Round-5 verdicts

- **R1:** "Qualified yes — mechanically playable in 18 months but 4-month MVP is dangerous optimism."
- **R2:** "Qualified no — 80 tasks landing at ~4 hours each with zero blockers is statistical fantasy."
- **R3:** "Qualified no — Tier 2 swollen with post-MVP tests, MVP-4.3.1 contradicts itself, build lock is enforced by convention not OS."
- **R4:** "Qualified no — autonomy chain deadlocks before task one. Schedule still fantasy-shaped."

## What this round fixes

This round focuses on **deletion of stale residue**: the duplicate navmesh block, the JsonReader DecisionLog example, the §2.1.3 anim count, the MEMORY.md non-tools line. Plus structural fixes:

- Tier 2 MVP/post-MVP tagging.
- MVP-4.3.1 acceptance gate redesigned to not depend on post-MVP bot.
- ManualSetupChecklist circular dependency resolved.
- `EmittedSound` spec aligned across docs.
- Anchor semantics reconciled with GDD.
- Walk-quiet semantics clarified.
- Synty rejection rationale corrected.
- Doc-linter promoted to Phase 0.3 (where it belongs).
- LFS strategy added to Phase 0.0.

See `DecisionLog.md` 2026-05-12 round-5 entry.
