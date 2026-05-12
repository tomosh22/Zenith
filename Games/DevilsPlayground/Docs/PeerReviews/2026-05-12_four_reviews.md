# Peer Reviews — Round 2, 2026-05-12

Four additional peer reviews ran the day after the round-1 reconciliation landed. Convergence across reviewers is high: the round-1 fixes addressed surface inconsistencies but did not fix the structural risks (navmesh schedule, instrumentation-hook ownership, autonomy chain bootstrap). All four reviews are saved verbatim above; this file captures the actionable consensus.

---

## Cross-reviewer convergence

| Theme | R1 | R2 | R3 | R4 |
|---|:--:|:--:|:--:|:--:|
| Navmesh fallback should be default | C1 | C2 | C2 | C3 |
| Instrumentation hooks need an owner | — | C1 | C1 | C4 |
| CI bootstrap can't self-bootstrap | C2 | C3 | C3 | C1 |
| Test-first ordering breaks compile | C3 | — | — | — |
| Build lock is policy not enforcement | C4 | S6 | — | C6 |
| Mixamo silhouette at 80m unreadable | — | S5 | — | C5 |
| Sexton has no MVP gameplay | — | S4 | — | — |
| Schedule still overclaims | V | V | V | V |
| `DP_Fog::GatherFogHolePositions` already exists | I | S6 | I | I |
| `DP_Fog.slang` already exists at `Zenith/Flux/Shaders/Fog/DP_Fog.slang` | — | — | — | I |
| Worked examples in AgentBriefing/Playbook contradict reordered roadmap | — | — | — | C1 |
| InputsForAutonomy still has 12 FTE / 18 mo anchors | I | — | — | I |
| AgentBriefing CI check names contradict MvpRoadmap | — | — | — | I |
| Memory file still references Synty | — | — | I | — |

V = stated in verdict. C = critical. S = substantive. I = inconsistency.

## Round-2 verdicts

- **R1:** "Will not ship in 4 months. 7-9 months plausible with hard scope cuts."
- **R2:** "Qualified no. PC-only MVP plausible in 7-9 months. 18-month premium product not credible."
- **R3:** "Qualified yes — playable MVP achievable in 18 months, but 4-month estimate is fiction."
- **R4:** "Qualified no. Several docs still describe a coherent production story while the executable chain, test instrumentation, navmesh, audio, and asset pipeline are not yet real."

## What this round changes

The round-1 reconciliation accepted the framing that the plan was "internally consistent but dangerously optimistic." Round-2 reviewers now agree that:

1. The internal consistency improved.
2. The optimism was not corrected. Navmesh, instrumentation hooks, and CI bootstrap remain structural risks dressed as routine tasks.

The round-2 reconciliation must address those structural risks directly:

- Make the **hand-authored `.znavmesh` the default**; runtime generator becomes post-MVP stretch.
- Reassign **instrumentation hooks to the human owner** (Tomos) as Engine work, not agent tasks.
- Add a **ManualSetupChecklist.md** that gates the very first agent session on human pre-flight actions (branch protection, `pwsh.exe`, Vulkan SDK pin, smoke build).
- Acknowledge **autonomous verification has limits** — visual legibility and gameplay readability need human eyes, no matter how many surrogate buses exist.

See `DecisionLog.md` for the per-change record.
