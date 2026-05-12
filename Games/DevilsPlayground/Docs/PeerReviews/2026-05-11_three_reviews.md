# Peer Reviews — 2026-05-11

Three independent peer reviews of the planning pack, run by the user against external AI agents on 2026-05-11. Saved verbatim; findings have been triaged and addressed in a follow-up PR. See `Docs/DecisionLog.md` for which findings were accepted, which were refuted, and the resulting document changes.

---

## Review 1

(Senior gamedev tech lead, founder-advisor framing. Most pragmatic / least padding.)

**Verdict:** *"This pack is internally consistent, well-structured, and dangerously optimistic. The fatal flaw is not documentation drift — it is schedule fantasy."*

Top critical findings:
1. Navmesh generator schedule fiction (6 weeks treated as 1)
2. Tier 4 playthrough test spec uses naive timed WASD holds (would flake)
3. Skeletal-mesh import pipeline unproven; needs a spike
4. Orchestrator build lock is racy and not crash-safe
5. CI auto-merge assumes infrastructure that doesn't exist

Top "what I'd change first":
- Spike the navmesh generator
- Rewrite Tier 4 spec to mandate A* pathfinding (per existing `Test_HumanPlaythrough.cpp`)
- Strip the 12-person/$3M budget fantasy from GDD §12

---

## Review 2

(Most detailed; cross-referenced actual codebase state. Found many concrete bugs.)

**Verdict:** *"A playable, ugly, PC-only MVP is plausible if the first month is spent fixing the execution chain and cutting scope hard; the 18-month premium multi-platform game described by the pack is not credible for one developer plus autonomous agents."*

Top critical findings:
1. Production model contradicts actual staffing reality (12 FTE plan, 1 person reality)
2. Autonomy chain not executable from task one (CI workflow builds wrong project, runner flags don't exist, MVP-0.3 creates files that exist)
3. Test plan relies on surrogate instrumentation that doesn't exist (Zenith_AudioBus, Zenith_RenderBus, Zenith_SaveSystem are mentioned in docs only)
4. MVP schedule is fantasy-sized for current base
5. Asset plan legally mostly OK but operationally and visually weak
6. Audio strategy contradicts itself
7. Orchestrator lock is advisory, not safe

Top inconsistencies (concrete, file-cited):
- Headcount differs across docs (12 vs 17 vs 1)
- 150 vs 410–430 villager animation count
- Audio MVP scope contradiction
- Fog shader scope contradiction
- TestPlan §1 lists 6 tests then says "These five tests"
- Runner flags don't exist
- 28 vs 34 test count drift
- Tuning vs Priest_Behaviour numeric drift (sight 25 vs 20, hearing 30 vs 25, FOV 110 vs 120)
- MVP-0.3 creates files that exist
- MVP-0.1.1 implements before MVP-0.1.2 tests
- DP_OnRunLost referenced in OrchestratorPlaybook before MvpRoadmap creates it
- Tier 4 MVP subset mismatch (3 vs 4)
- Zoom 150 vs 30-80
- Synty contradiction with no-spend policy

---

## Review 3

(Strongest on internal-consistency checks. Verified many claims against actual code.)

**Verdict:** *"The system of documents has critical alignment failures between GDD targets, MVP scope, data files, test specs, and prototype code. The schedule is sized for a 12-person team; the reality is 1 person + AI agents."*

Top critical findings:
1. Archetype count inconsistent across four documents (MVPScope: 4, TestPlan: 12, Archetypes.json: 24, GDD: 24 designed)
2. demon_scent_max in Tuning.json (1.5) contradicts GDD (0.0–1.0)
3. Aelfric peripheral FOV in Tuning.json (137.5) contradicts GDD (130)
4. Synty assets are Unity-licensed; cannot be used in Vulkan/custom engine

Top substantive gaps:
- Navmesh generator severely underspecified
- Fog implementation strategy contradictory (`GatherFogHolePositions` API doesn't exist)
- Villager count is wrong: code says 17, Shortfalls says 14
- Shortfalls §3.8 claims non-tools builds broken; memory says fixed 2026-05-10
- Tier 4 tests unimplementable without navmesh
- pwsh.exe availability unverified
- Auto-merge has no mandatory code-review gate

Top "what I'd change first":
1. Fix the 4-vs-12-vs-24 archetype mismatch in TestPlan §3.1
2. Reconcile Tuning.json scent max and Aelfric peripheral FOV with the GDD
3. Replace Synty with Mixamo+Kenney or in-house at S1
4. Add GatherFogHolePositions API to MVP-2.4 sub-tasks

---

## Validation summary

Sub-agent ground-truth check against the codebase confirmed:
- Priest hardcoded values (20/25/120) ✓
- Villager count = 17 (CLAUDE.md internally contradicts itself: says "14 villagers" in one comment, "17 villagers" in another) ✓
- Status/Questions/DecisionLog files exist ✓
- GitHub Actions workflow builds Games/Test, not DP ✓
- Runner flags missing ✓
- TestPlan §3.1 has 12 archetypes vs MVP's 4 ✓
- Tuning zoom_max = 150 ✓
- DP_OnRunLost not yet defined ✓
- Tier 4 MVP test count discrepancy ✓
- CLAUDE.md references missing pwsh.exe ✓

Refuted / partially-refuted:
- Test count: actually 24 .cpp files; CLAUDE.md says 28 (multi-test files account for the difference); reviewer 2's "34" appears to be wrong but the docs should standardise on a verified count
- TestPlan §1 "These five tests" phrase doesn't appear in current file (may have been mistaken reading)
- DPFogPass.cpp is substantially real, not just a stub (reviewer 2 was right about this)

Unverifiable from codebase alone:
- Non-tools build status (memory says fixed 2026-05-10; cannot re-verify without a clean build run)
- Synty's Unity-only EULA restriction (couldn't reach external license docs)
