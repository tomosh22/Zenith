# DP Decision Log

**Purpose:** Append-only record of every non-trivial decision made during DP development. Future agents grep this when investigating "why was X done this way?"

**Format:** One entry per decision. Newest entries at the top.

**What counts as non-trivial:** anything that took >15 minutes to think through, involved trade-offs, or changed a behaviour another part of the system depends on. Tuning-value changes go in the Tuning.json's git history, not here.

---

## 2026-05-11 — Initial planning session: ratified MVP scope and authoring framework

**Decision:** With user sign-off, locked the framing of *Devil's Playground* as a stealth-puzzle roguelite per the GDD. Authored five companion documents (Shortfalls, TestPlan, AssetManifest, AssetTestPlan, InputsForAutonomy) plus three for execution (MVPScope, MvpRoadmap, AgentBriefing). Authored three Config data files (Tuning, Archetypes, Reagents).

**Why:** The user wants autonomous execution. Autonomous agents need (a) a target (MVPScope), (b) a sequence of tasks (MvpRoadmap), (c) operating conventions (AgentBriefing), (d) structured data to reference (Config/*.json), and (e) async-communication artefacts (Status, Questions, DecisionLog). Without all five, sessions drift or stall.

**Trade-offs considered:**
- *Less documentation, faster start.* Rejected: with autonomous sessions, the document-front-loaded cost is recovered on session 2+. Without the docs, the next agent re-decides framing.
- *Full GDD execution, not MVP.* Rejected: user explicitly asked for MVP-first ("single MVP level with all gameplay elements functioning"). Scope discipline matters more than feature parity.

**Test that prevents regression:** N/A (planning artefact, not code). The MvpRoadmap's task-checklist itself is the regression guard — every task ticks because a test passed.

**Reversibility:** trivial. Documents are markdown; data files are JSON. Any line can be amended in a PR.
