---
key: ZEN-7
type: BUG
title: ZENITH_ASSERT_EQ_FLOAT passes on NaN at every epsilon, so a degenerate float is indistinguishable from a correct one
category: Engine
parent: "ZEN-1"
sprint: null
status: To Do
assignee: null
priority: HIGH
complexity: MODERATE
risk: HIGH
storyPoints: 5
position: 3
labels:
  - tech-debt
fixVersions: []
blocks: []
links: []
source:
  id: w51x4uz7k2xgkknylk3ysgya
  risk: HIGH
  type: BUG
  title: ZENITH_ASSERT_EQ_FLOAT passes on NaN at every epsilon, so a degenerate float is indistinguishable from a correct one
  number: 7
  flagged: false
  due_date: null
  position: 3
  priority: HIGH
  severity: S2_MAJOR
  parent_id: null
  sprint_id: null
  status_id: q3spz9u36h2bf6qa942uqfz1
  complexity: MODERATE
  created_at: 2026-08-23T00:54:01.552949
  project_id: r1m7idj9n6rkv5b1iulqz2ho
  resolution: null
  updated_at: 2026-08-23T00:54:01.552949
  assignee_id: null
  category_id: xmu47blrutzwzsqblzurhc9p
  environment: null
  reporter_id: n9xhr52s8n49fa832egoqf4z
  resolved_at: null
  story_points: null
  reproducibility: ALWAYS
updatedAt: "2026-09-19T17:37:17.816Z"
---
## Goal

`ZENITH_ASSERT_EQ_FLOAT` passes on NaN, unconditionally, at every epsilon
including `0.0f`.

`Zenith/Core/Zenith_TestFramework.cpp` implements the comparison as:

```cpp
if (std::fabs(fA - fB) > fEps)   // → the failure branch
```

`NaN > fEps` is **false** for every `fEps`, so the failure branch is never taken
when either side is NaN. A test whose only guard on a float is
`ZENITH_ASSERT_EQ_FLOAT` therefore cannot distinguish "correct value" from
"degenerate value", which is the single case such an assertion is most often
reached for.

This is not theoretical for this repo. Float degeneracy is a recurring, expensive
failure mode here — ZM-D-179 and ZM-D-183 are both authored-transform drift, the
Dawnmere placement headers carry an explicit `fZM_..._UNMEASURED` sentinel chosen
to be finite *precisely so no accessor can hand a NaN to a transform*, and
`ZM_Tests_DawnmerePlacement.cpp` defends its W5 units with standalone
`std::isfinite` sweeps. So the convention of adding a separate finiteness clause
already exists in the places somebody thought about it. Nothing enforces it, and
nothing tells an author that the assertion they reached for does not do what its
name suggests.

Found while adding coverage for ZM-20's `ZM_GetDawnmereFromRoute1SpawnFeet()`:
the new unit had to open with an explicit finiteness clause *before* its three
exact-equality clauses, or a NaN would have satisfied all three.

**The blast radius is the interesting part, and it is why this is not a one-line
fix.** Making the macro reject NaN will red every test currently passing on one.
Each of those is either a latent defect this ticket has just surfaced, or a test
that deliberately compares NaN and needs re-writing to say so. Both outcomes are
wins; neither should be silently absorbed by loosening the fix.

Note the *non*-float `ZENITH_ASSERT_EQ` is unaffected — it compares with `==` on
its operand type and has no epsilon.

## Definition of Done

- [ ] `ZENITH_ASSERT_EQ_FLOAT` FAILS when either operand is NaN, at every
      epsilon including `0.0f`, and its message says NaN was the reason rather
      than reporting a magnitude comparison
- [ ] The same treatment is applied to any sibling float comparison in the test
      framework that shares the `fabs(a - b) > eps` shape, or each survivor is
      recorded with a reason it does not need it
- [ ] A test in the framework's own suite asserts the new behaviour in both
      directions — a NaN on either side fails, and a genuine near-equal pair
      inside epsilon still passes
- [ ] Every test the change reds is triaged in the work log as either a latent
      defect now caught or a deliberate NaN comparison rewritten to say so; none
      is fixed by widening the epsilon or reverting the guard
- [ ] Every affected game's pinned baseline bumped from an OBSERVED run
