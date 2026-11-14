# AI requirement catalogue

## Purpose

The requirement catalogue of the AI area (AI & navigation, including the Appendix A "AI (extended)" row): every obligation of `Zenith2/Docs/Design/AI.md` and its section files, one testable requirement each, mapped to the inventory items `Z2-I-AI-01` … `Z2-I-AI-17` of `Zenith2/Docs/Requirements/Inventory.md` → "Inventory v1". Each requirement carries its statement, the items it covers, the stage whose acceptance it serves, its classification, its governing decisions, its budget rows, its acceptance evidence and the design headings that specify it. The requirement index of the design is `Zenith2/Docs/Design/AI.md` → "Requirement index". The catalogue is three files with one numbering: this file holds the purpose, the requirements of Stages 1 and 2, the coverage of inventory items (over all three files) and the exclusions; `Zenith2/Docs/Requirements/AI/Stage4Plus.md` holds Z2-R-AI-007 … Z2-R-AI-096 and `Zenith2/Docs/Requirements/AI/Stage4Systems.md` holds Z2-R-AI-097 onward (the rest of Stage 4 and Stage 6), in the same format.

## Requirements

### Z2-R-AI-001 Navigation selection harness

- **Statement:** The harness target Zenith_Bench_NavigationSelection under Zenith2/Benchmarks/NavigationSelection/ must build both D-SEL-NAV candidates behind the stage contracts and run conformance scenarios N1 to N14 on each, measuring bake time, tile bytes, runtime memory, query cost and the 100-agent replan cost, with a --smoke CTest run in the Test step.
- **Covers:** Z2-I-AI-01, Z2-I-AI-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-084, Z2-D-082
- **Budgets:** Z2-B-AI-001, Z2-B-AI-002, Z2-B-AI-003
- **Acceptance evidence:** Bench_NavigationSelection_Smoke passing in the Test step; the harness source reviewed against AI/Navigation.md "Harness".
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Harness"; Zenith2/Docs/Design/AI/Navigation.md → "Conformance scenarios"; Zenith2/Docs/Design/AI/Tests.md → "Benchmarks and device runs"

### Z2-R-AI-002 Navigation conformance scenarios defined

- **Statement:** The fourteen navigation conformance scenarios N1 to N14 must be generated fixtures whose expectations are derived from their geometry, never recorded from a candidate, and the same scenario functions must run against each candidate in the harness and against the selected implementation in ZenithAI_Conformance.
- **Covers:** Z2-I-AI-01, Z2-I-AI-13
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-084, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** The harness smoke run and, after the ruling, the Test_NavConformance_N01_FlatPlane to Test_NavConformance_N14_ReplanBudget tests in the Test step.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Conformance scenarios"; Zenith2/Docs/Design/AI/Tests.md → "Navigation conformance suite"; Zenith2/Docs/Design/AI/Tests.md → "Fixtures"

### Z2-R-AI-003 Recast/Detour candidate pinned for measurement

- **Statement:** The manifest entry recast-detour must be pinned with exact version, URL, SHA-256 and SPDX licence, owner the harness target and usage tools, before the harness measures it, and must never be linked into a runtime target before the ruling.
- **Covers:** Z2-I-AI-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-082, D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** The SHIP manifest validation step passing with the entry; the Libgraph step showing no runtime target linking recast-detour.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Harness"; Zenith2/Docs/Design/AI.md → "Libraries AI code depends on"

### Z2-R-AI-004 Navigation selection scope stated

- **Statement:** Once X277 (3) and X316 are applied, local avoidance, carving, volume navigation and the search loop with every navmesh query must be ZenithAI code whichever D-SEL-NAV candidate wins, the selection deciding only the bake stages and the runtime navmesh storage; until then the DecisionLog wording stands, the harness reports both scopes, and the research report must record why DetourTileCache and DetourCrowd were not measured.
- **Covers:** Z2-I-AI-06, Z2-I-AI-16
- **Stage:** S1
- **Classification:** required
- **Decisions:** D-SEL-NAV (open, Stage 1/2)
- **Budgets:** none
- **Acceptance evidence:** Review of the harness against AI/Navigation.md "Candidate mapping"; conformance scenario N12 in the harness smoke set; the research report section on the unmeasured Detour components.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Candidate mapping"; Zenith2/Docs/Design/AI/Volumes.md → "Scope of this file"; Zenith2/Docs/Design/AI.md → "Open decisions"

### Z2-R-AI-005 AI per-system budget allocation

- **Statement:** The Stage 1 per-system budget ticket for AI must allocate Z2-B-AI-001, Z2-B-AI-002 and Z2-B-AI-003 from reference-engine and published data and record the pending row needs of "Pending row needs" with their defaults as their starting values.
- **Covers:** Z2-I-AI-05, Z2-I-AI-07
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-AI-001, Z2-B-AI-002, Z2-B-AI-003
- **Acceptance evidence:** The ruling ticket diff filling the three rows in Budgets.md, checked by MP-ALLOC (Check-Allocation.ps1 -Area AI) in the Stage 1 acceptance.
- **Design:** Zenith2/Docs/Design/AI.md → "Rows owned"; Zenith2/Docs/Design/AI.md → "Pending row needs"

### Z2-R-AI-006 Navigation selection ruling applied

- **Statement:** The D-SEL-NAV ruling must be recorded with its measurements under Zenith2/Docs/Research/Navigation/ and appended to DecisionLog.md with an explicit Z2-D-NNN in the ruling diff, which also moves or removes the recast-detour manifest entry and edits the candidate mapping, at the latest by the Stage 2 acceptance.
- **Covers:** Z2-I-AI-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-084, Z2-D-097, D-SEL-NAV (open, Stage 1/2)
- **Budgets:** Z2-B-AI-001, Z2-B-AI-002, Z2-B-AI-003
- **Acceptance evidence:** The ruling ticket diff (report, DecisionLog entry, manifest edit) checked by the Stage 2 acceptance coverage step in acceptance mode.
- **Design:** Zenith2/Docs/Design/AI/Navigation.md → "Ruling consequences"; Zenith2/Docs/Design/AI/Navigation.md → "Candidate mapping"; Zenith2/Docs/Design/AI.md → "Selection decisions AI owns"

## Coverage of inventory items

Requirements of all three catalogue files; Z2-R-AI-007 … Z2-R-AI-096 are in `Zenith2/Docs/Requirements/AI/Stage4Plus.md` and Z2-R-AI-097 onward in `Zenith2/Docs/Requirements/AI/Stage4Systems.md`.

| Item | Text | Classification | Stage | Requirements |
|---|---|---|---|---|
| Z2-I-AI-01 | navmesh baking | required | S4 | Z2-R-AI-001, Z2-R-AI-002, Z2-R-AI-003, Z2-R-AI-006, Z2-R-AI-007, Z2-R-AI-008, Z2-R-AI-009, Z2-R-AI-012, Z2-R-AI-013, Z2-R-AI-014, Z2-R-AI-015, Z2-R-AI-016, Z2-R-AI-018, Z2-R-AI-019, Z2-R-AI-020, Z2-R-AI-021, Z2-R-AI-025, Z2-R-AI-026, Z2-R-AI-027, Z2-R-AI-028, Z2-R-AI-029, Z2-R-AI-030, Z2-R-AI-031, Z2-R-AI-032, Z2-R-AI-033, Z2-R-AI-034, Z2-R-AI-035, Z2-R-AI-036, Z2-R-AI-037, Z2-R-AI-038, Z2-R-AI-039, Z2-R-AI-040, Z2-R-AI-042, Z2-R-AI-144, Z2-R-AI-156, Z2-R-AI-158, Z2-R-AI-161, Z2-R-AI-171, Z2-R-AI-172, Z2-R-AI-184 |
| Z2-I-AI-02 | navmesh streaming | required | S4 | Z2-R-AI-014, Z2-R-AI-018, Z2-R-AI-021, Z2-R-AI-043, Z2-R-AI-044, Z2-R-AI-045, Z2-R-AI-046, Z2-R-AI-047, Z2-R-AI-048, Z2-R-AI-049, Z2-R-AI-050, Z2-R-AI-059, Z2-R-AI-066, Z2-R-AI-156, Z2-R-AI-160, Z2-R-AI-163, Z2-R-AI-164, Z2-R-AI-167, Z2-R-AI-178, Z2-R-AI-179, Z2-R-AI-182, Z2-R-AI-184 |
| Z2-I-AI-03 | dynamic obstacles | required | S4 | Z2-R-AI-041, Z2-R-AI-051, Z2-R-AI-052, Z2-R-AI-053, Z2-R-AI-054, Z2-R-AI-055, Z2-R-AI-056, Z2-R-AI-057, Z2-R-AI-157, Z2-R-AI-166, Z2-R-AI-185 |
| Z2-I-AI-04 | off-mesh links | required | S4 | Z2-R-AI-058, Z2-R-AI-059, Z2-R-AI-060, Z2-R-AI-061, Z2-R-AI-144, Z2-R-AI-180, Z2-R-AI-185, Z2-R-AI-186 |
| Z2-I-AI-05 | async pathfinding | required | S4 | Z2-R-AI-001, Z2-R-AI-005, Z2-R-AI-010, Z2-R-AI-012, Z2-R-AI-017, Z2-R-AI-022, Z2-R-AI-023, Z2-R-AI-057, Z2-R-AI-062, Z2-R-AI-063, Z2-R-AI-064, Z2-R-AI-065, Z2-R-AI-066, Z2-R-AI-067, Z2-R-AI-068, Z2-R-AI-069, Z2-R-AI-070, Z2-R-AI-071, Z2-R-AI-072, Z2-R-AI-073, Z2-R-AI-074, Z2-R-AI-075, Z2-R-AI-076, Z2-R-AI-077, Z2-R-AI-084, Z2-R-AI-151, Z2-R-AI-155, Z2-R-AI-157, Z2-R-AI-158, Z2-R-AI-159, Z2-R-AI-165, Z2-R-AI-168, Z2-R-AI-174, Z2-R-AI-175, Z2-R-AI-176, Z2-R-AI-177 |
| Z2-I-AI-06 | avoidance | required | S4 | Z2-R-AI-004, Z2-R-AI-015, Z2-R-AI-078, Z2-R-AI-080, Z2-R-AI-086, Z2-R-AI-087, Z2-R-AI-088, Z2-R-AI-102, Z2-R-AI-173 |
| Z2-I-AI-07 | crowds | required | S4 | Z2-R-AI-005, Z2-R-AI-069, Z2-R-AI-078, Z2-R-AI-079, Z2-R-AI-080, Z2-R-AI-081, Z2-R-AI-082, Z2-R-AI-083, Z2-R-AI-084, Z2-R-AI-085, Z2-R-AI-089, Z2-R-AI-090, Z2-R-AI-091, Z2-R-AI-092, Z2-R-AI-093, Z2-R-AI-094, Z2-R-AI-095, Z2-R-AI-159, Z2-R-AI-160, Z2-R-AI-162, Z2-R-AI-169, Z2-R-AI-170, Z2-R-AI-173, Z2-R-AI-177 |
| Z2-I-AI-08 | perception | required | S4 | Z2-R-AI-106, Z2-R-AI-107, Z2-R-AI-108, Z2-R-AI-109, Z2-R-AI-110, Z2-R-AI-111, Z2-R-AI-112, Z2-R-AI-113, Z2-R-AI-114, Z2-R-AI-115, Z2-R-AI-116, Z2-R-AI-117, Z2-R-AI-160, Z2-R-AI-169 |
| Z2-I-AI-09 | blackboards | required | S4 | Z2-R-AI-017, Z2-R-AI-118, Z2-R-AI-119, Z2-R-AI-120, Z2-R-AI-121, Z2-R-AI-122, Z2-R-AI-123, Z2-R-AI-134 |
| Z2-I-AI-10 | behaviour authoring | required | S4 | Z2-R-AI-124, Z2-R-AI-125, Z2-R-AI-126, Z2-R-AI-127, Z2-R-AI-128, Z2-R-AI-129, Z2-R-AI-130, Z2-R-AI-131, Z2-R-AI-134, Z2-R-AI-135 |
| Z2-I-AI-11 | state authoring | required | S4 | Z2-R-AI-127, Z2-R-AI-132, Z2-R-AI-133, Z2-R-AI-135 |
| Z2-I-AI-12 | AI debugging | required | S4 | Z2-R-AI-011, Z2-R-AI-042, Z2-R-AI-140, Z2-R-AI-141, Z2-R-AI-142, Z2-R-AI-143, Z2-R-AI-144, Z2-R-AI-145 |
| Z2-I-AI-13 | reproducible AI scenarios | required | S4 | Z2-R-AI-002, Z2-R-AI-023, Z2-R-AI-038, Z2-R-AI-056, Z2-R-AI-063, Z2-R-AI-071, Z2-R-AI-088, Z2-R-AI-117, Z2-R-AI-126, Z2-R-AI-139, Z2-R-AI-141, Z2-R-AI-146, Z2-R-AI-147, Z2-R-AI-148, Z2-R-AI-149, Z2-R-AI-150, Z2-R-AI-151, Z2-R-AI-152, Z2-R-AI-153, Z2-R-AI-154, Z2-R-AI-163, Z2-R-AI-164, Z2-R-AI-165, Z2-R-AI-166, Z2-R-AI-168, Z2-R-AI-175, Z2-R-AI-178, Z2-R-AI-179, Z2-R-AI-180, Z2-R-AI-181, Z2-R-AI-182, Z2-R-AI-183, Z2-R-AI-186 |
| Z2-I-AI-14 | AI simulation LOD | required | S4 | Z2-R-AI-010, Z2-R-AI-110, Z2-R-AI-136, Z2-R-AI-137, Z2-R-AI-138, Z2-R-AI-139, Z2-R-AI-162 |
| Z2-I-AI-15 | navigation for different agent sizes | required | S4 | Z2-R-AI-024, Z2-R-AI-026, Z2-R-AI-031, Z2-R-AI-096 |
| Z2-I-AI-16 | flying navigation | required | S4 | Z2-R-AI-004, Z2-R-AI-092, Z2-R-AI-097, Z2-R-AI-098, Z2-R-AI-099, Z2-R-AI-100, Z2-R-AI-101, Z2-R-AI-102, Z2-R-AI-105, Z2-R-AI-181 |
| Z2-I-AI-17 | swimming navigation | required | S4 | Z2-R-AI-025, Z2-R-AI-097, Z2-R-AI-098, Z2-R-AI-103, Z2-R-AI-104, Z2-R-AI-105 |

## Excluded and deferred

- No AI inventory item is excluded or deferred: all seventeen are required at Stage 4 (`Zenith2/Docs/Requirements/Inventory.md` → "Inventory v1"); the Stage 1 and Stage 2 requirements above are the selection and budget producers those Stage 4 items depend on, and the Stage 6 requirements are their qualification.
- Squad and formation systems, cover selection and tactical point queries are not AI engine features: they are Luau gameplay over blackboards and events (`Zenith2/Docs/Design/AI/Scripting.md` → "Authoring conventions"), as quest, dialogue, inventory and interaction are game-side (Z2-D-077).
- A visual behaviour or state editor is not provided: AI behaviours and states are authored in Luau code with the AI libraries (Z2-D-079 gives visual authoring to animation, terrain, UI and cinematics); AI's visual surfaces are the debugger and the navigation tools.
- Networked replication of AI state is NET's deferred Stage 7 scope (Z2-D-073); AI's components carry the authority metadata it will need, and AI adds no requirement for it here.
- DetourCrowd and DetourTileCache are not used whichever D-SEL-NAV (open, Stage 1/2) candidate wins (`Zenith2/Docs/Design/AI/Navigation.md` → "Candidate mapping").
