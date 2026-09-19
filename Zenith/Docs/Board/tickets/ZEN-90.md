---
id: "1714567e-da89-4984-90b5-c3afc605caae"
key: "ZEN-90"
type: "BUG"
title: "zagent decide writes decision rows that zagent rows decisions cannot read"
category: "Engine"
parent: null
sprint: null
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "MODERATE"
risk: "HIGH"
storyPoints: 5
position: 90
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-19T22:08:56.008Z"
flagged: false
severity: "S2_MAJOR"
reproducibility: null
dueDate: null
environment: null
---
## Goal

`zagent decide` persists rows that `zagent rows decisions` cannot read. The two
commands resolve different stores.

MEASURED on 2026-09-19, filing four decisions across ZEN-7 and ZEN-12:

- all four calls returned **exit 0** with a `rowId`, and produced four
  `board: decide` commits
- all four ids (`ZEN-D-001` … `ZEN-D-004`) are present in `metadata.md`
- `zagent rows decisions`, with and without `--project ZEN`, returns only the
  same **7 pre-existing rows** and none of the four

The 7 readable rows carry `database_id: null` — a legacy direct knowledge
record. The four written rows went into the notion database titled
`Agent · Decisions` (`gm1dzh0b1o8u1p38sosvoco4`). So the write path takes the
notion-database branch while the read path returns the direct record.

**Why this matters beyond tidiness.** The tick protocol REQUIRES a
`Durable findings` answer in every terminal work log, naming either "none" or a
`PROJ-D-N` reference, and `zagent finish` refuses without one. But `finish`
validates only the SHAPE of that reference (a regex on `PROJ-D-N`), never its
existence. A tick can therefore satisfy the mandatory check with a reference no
documented read command can retrieve — which is the same "silence is illegal but
the answer is unchecked" hole the Durable-findings mechanism was built to close,
displaced one level up.

Secondary, lower priority: the first `decide` of the session also reformatted
`metadata.md` from pretty-printed to compact (9573 → 3645 lines, 638662 →
622129 bytes; byte delta consistent with newline and indent removal, not data
loss, and every later `decide` grew the file). It makes any `board:` commit's
diff unreviewable.

## Definition of Done

- [ ] A decision written by `zagent decide` is returned by `zagent rows decisions`
      in the same checkout, with and without `--project`
- [ ] The write path and the read path resolve the SAME store, and which one is
      stated in a comment where the resolution happens
- [ ] Pre-existing rows in the other store are either migrated or still readable —
      no decision that exists today becomes unreadable
- [ ] A test covers the round trip: write a decision, read it back by id, and
      fail if the read returns nothing
- [ ] Every affected game's pinned baseline bumped from an OBSERVED run, if the
      change adds or removes a unit
