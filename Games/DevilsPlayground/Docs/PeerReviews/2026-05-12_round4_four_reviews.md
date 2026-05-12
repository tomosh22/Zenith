# Peer Reviews — Round 4, 2026-05-12

Four additional reviews after the round-3 reconciliation. Convergence is on **claims still wrong in the prototype + plan that round-3 missed**, and on **the navmesh generator already existing in the engine** — which transforms the entire MVP schedule.

---

## Key validations (ground-truth confirmed)

| Claim | Verdict |
|---|---|
| `Zenith/AI/Navigation/Zenith_NavMeshGenerator.h/.cpp/.Tests.inl` EXISTS as a complete Recast-style pipeline (voxelisation → region growing → contour tracing → polygon mesh → adjacency) | ✅ R4 critical 3 confirmed |
| `Core/Zenith_JsonReader.h` does NOT exist (OrchestratorPlaybook §8 references it) | ✅ R3 critical 1 confirmed |
| `Source/PublicInterfaces.cpp:39,42,50` uses `std::unordered_map` in production code (convention violation) | ✅ R2 critical 4 confirmed |
| `Shortfalls.md:10` still says `DP_Fog.slang` "does not yet exist" — file actually exists at `Zenith/Flux/Shaders/Fog/DP_Fog.slang` | ✅ R3 critical 4 confirmed |
| `DevilsPlayground.cpp:492` + `DPVillager_Behaviour.h:5` still say "14 villagers" in comments | ✅ R4 inconsistency confirmed |
| Status.md / AgentBriefing §9 / OrchestratorPlaybook §8 still describe MVP-0.1.1 as failing-test-only with MVP-0.1.2 as implementation, contradicting MvpRoadmap §0.1 | ✅ R4 critical 2 confirmed |
| TestPlan §5.1 still mandates `ComputePathAStar` while MVP-1.2.9 says retire it | ✅ R4 inconsistency confirmed |

## Single biggest finding

**The navmesh generator already exists.** Round-2/3 panic about "6-12 weeks of net-new engine work for voxelisation/region-growing/contour-simplification" was based on the wrong premise. `Zenith_NavMeshGenerator::GenerateFromScene(Zenith_SceneData&, const NavMeshGenerationConfig&)` returns a `Zenith_NavMesh*` from scene static colliders, today. The task is integration (wire `DP_AI::GetOrBuildLevelNavMesh` to call it), not implementation.

This collapses the most-debated risk in the entire pack from a 6-week worry to a 2-3 day integration. Every prior decision and reviewer concern about "navmesh as schedule black hole" needs to be re-evaluated against this finding.

## Round-4 verdicts

- **R1:** "Qualified no. Will not ship MVP in 4 months. 18-month ship achievable only if navmesh spike succeeds in 5 days, Mixamo imports work first try, CI stabilises within first week." (R1 didn't know the generator exists.)
- **R2:** "Qualified yes. MVP can ship something playable in 18 months if navmesh spike converges within timebox." (R2 found `std::unordered_map` violations + Mixamo retargeting gap.)
- **R3:** "Qualified yes. Concrete autonomy chain break point: `Zenith_JsonReader` fiction in OrchestratorPlaybook §8 worked example."
- **R4:** "No, not as planned. Premium 18-month game assumes too much. Pack is impressively thorough but executability still below the bar." (R4 confirmed existing `Zenith_NavMeshGenerator`.)

## Reconciliation focus

This round's fixes:

1. **Re-scope MVP-1.2** entirely. Integration of existing `Zenith_NavMeshGenerator`, not building from scratch.
2. **Fix `Zenith_JsonReader` fiction** in OrchestratorPlaybook §8.
3. **Reconcile MVP-0.1.1 ordering** across Status, AgentBriefing, OrchestratorPlaybook (round-3 only fixed MvpRoadmap).
4. **Reconcile TestPlan §5.1** with MVP-1.2.9 (one says reuse grid A*, other says retire it).
5. **Add walk-quiet test + task** (MVP-1.1 missing).
6. **Fix `Zenith_AudioBus::EmitSound` signature** to include radius.
7. **Address `std::unordered_map` reality** in prototype code: clarify the rule allows it as grandfathered tech debt OR schedule a cleanup task.
8. **Sundries**: Shortfalls DP_Fog claim, DPVillager `14 villagers` comments, animation count 410 vs 430, freesound NC filter, gh CLI exact scopes, Mode A vs Mode B contradiction.

See `DecisionLog.md` 2026-05-12 round-4 entry for the per-change record.
