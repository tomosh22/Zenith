// Regression tests for five fixes filed together:
//
//   (A) `zagent update --category` was documented-shaped and read by
//       `update()` (write.mjs's `changedFields`), but refused by the flag
//       allowlist in legacy-pure/flags.mjs — the exact "closed vocabulary
//       refuses a flag the function beneath it accepts" shape that
//       comment block already names for `sprint --force`.
//   (B) `guard`'s success path never carried `gateVerdictFresh` — only the
//       override-to-failure branch did — so a passing guard offered no
//       positive evidence the freshness fold ever ran.
//   (C) `claim`/`next` never resolved which recorded rulings govern a
//       ticket's subject matter into durable scratch, although I3
//       (.claude/commands/tick.md) requires exactly that be pasted into
//       the worker prompt.
//   (D) `zagent decide` was silent (exit 0, "wrote a row") when the
//       ticket's category declares no decisionLog, even though such a row
//       is then invisible to a worker's rulings scan.
//   (F) `reviewDecision`'s `review.required` OR was missing a third term:
//       "the ticket's subject matter is governed by a recorded decision" —
//       previously only prose in tick.md, asking a human tick to remember.
//   (G) `client.mjs` treated every path-valued flag NAME the same across
//       every command, so `zagent sprint create <NAME> --goal "prose"`
//       tried to open the prose as a filename and crashed with ENOENT —
//       `--goal` is a file for `update`, prose for `sprint`.
//
// (E), the create-time DoD blast-radius scan, is covered in its own
// `describe` block below. (E2), the same scan wired into
// `update --dod`/`--body` (create()'s scan runs once, before `create`
// exists; `update()`'s runs after `runOperation` returns, over the
// composed body `changedFields` already built), is covered in the
// `describe` block right after it.
//
// (C) is tested against the pure renderer
// (`renderRulingsMarkdown`) and the shared matcher (`matchGoverningRulings`)
// directly — never by running a live `claim`; (G) is tested against
// `readInputFiles`/`pathValuedFlagsFor` directly — never against a live
// `sprint create`.
import assert from 'node:assert/strict';
import { promises as fs } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { describe, test } from 'vitest';

import { inspectArguments } from '../cli/arguments.mjs';
import {
  FILE_FLAGS,
  pathValuedFlagsFor,
  readInputFiles,
  renderRulingsMarkdown,
  runClient,
} from '../cli/client.mjs';
import { dispatchWrite } from '../cli/write.mjs';
import { matchGoverningRulings, reviewDecision, scanIdentifiers } from '../cli/legacy-pure/ticket.mjs';
import { loadVault } from '../server/main.js';
import { git } from '../server/operations.mjs';

async function fixture(t) {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'vault-claim-rulings-create-'));
  t.onTestFinished(() => fs.rm(root, { recursive: true, force: true }));
  await fs.cp(fileURLToPath(new URL('../fixtures/snapshot/vault/', import.meta.url)), root, { recursive: true });
  const configPath = path.join(root, 'vault.config.json');
  const config = JSON.parse(await fs.readFile(configPath, 'utf8'));
  config.writes = { enabled: true };
  await fs.writeFile(configPath, JSON.stringify(config));
  await fs.writeFile(path.join(root, '.gitignore'), '.zagent/\n');
  await git(root, ['init', '-q']);
  await git(root, ['config', 'user.name', 'Claim rulings test']);
  await git(root, ['config', 'user.email', 'test@example.invalid']);
  await git(root, ['add', '.']);
  await git(root, ['commit', '-qm', 'Snapshot']);
  return root;
}

// ═══ (A) zagent update --category ══════════════════════════════════════

describe('A: zagent update --category', () => {
  test('the flag allowlist no longer refuses --category on update', () => {
    // Direct unit check of the exact gate that used to throw: real
    // `parseArgs` + `assertKnownFlags`, the same path client.mjs's
    // `runClient` calls before ever reaching the write dispatcher.
    const inspected = inspectArguments(['update', 'ZEN-74', '--category', 'Tooling']);
    assert.equal(inspected.error, null, 'update --category must not be refused as an unknown flag');
  });

  test('a stale --projectt style typo is still caught (the allowlist still does its job)', () => {
    const inspected = inspectArguments(['update', 'ZEN-74', '--categoryy', 'Tooling']);
    assert.match(inspected.error ?? '', /Unknown flag/);
  });

  test('end to end: the value lands in the ticket file and is read back by gate resolution', async t => {
    const root = await fixture(t);
    const invoke = args => runClient(['--vault', root, ...args, '--json'], { cwd: root });

    const before = await invoke(['show', 'ZEN-7']);
    assert.equal(before.exitCode, 0, JSON.stringify(before.payload));
    assert.notEqual(before.payload.category, 'Zenithmon');

    const updated = await invoke(['update', 'ZEN-7', '--category', 'Zenithmon', '--human']);
    assert.equal(updated.exitCode, 0, JSON.stringify(updated.payload));
    assert.ok(!/Unknown flag/i.test(JSON.stringify(updated.payload)), 'must not be the old flag refusal');
    assert.deepEqual(updated.payload.changed, ['category']);

    // Reload through the real client rather than trusting the mutation's own
    // report — `show` re-derives gate selection from the fresh category.
    const after = await invoke(['show', 'ZEN-7']);
    assert.equal(after.exitCode, 0, JSON.stringify(after.payload));
    assert.equal(after.payload.category, 'Zenithmon');
    assert.equal(after.payload.decisionLog, 'Games/Zenithmon/Docs/DecisionLog.md', 'gate/category resolution picked the new category up');
  });
});

// ═══ (B) guard's success path is self-describing ═══════════════════════

describe('B: guard emits gateVerdictFresh on a genuine pass, not only on the override', () => {
  async function claimAndPrepGuard(root) {
    const invoke = args => runClient(['--vault', root, ...args, '--json'], { cwd: root });
    const claim = await invoke(['claim', 'ZEN-7']);
    assert.equal(claim.exitCode, 0, JSON.stringify(claim.payload));
    const scratch = path.join(root, '.zagent/run/ZEN-7');
    await fs.writeFile(path.join(root, 'worker-change.txt'), 'guard freshness fixture\n');
    const gates = await invoke(['gates', 'ZEN-7']);
    assert.equal(gates.exitCode, 0, JSON.stringify(gates.payload));
    return { invoke, scratch };
  }

  test('a fresh, passing guard carries gateVerdictFresh: true', async t => {
    const root = await fixture(t);
    const { invoke, scratch } = await claimAndPrepGuard(root);
    const headSha = await git(root, ['rev-parse', 'HEAD']);
    await fs.writeFile(path.join(scratch, 'tick_gates.json'), JSON.stringify({ state: 'complete', exitCode: 0, headSha, steps: [], skipped: [] }));
    const guard = await invoke(['guard', 'ZEN-7']);
    assert.equal(guard.exitCode, 0, JSON.stringify(guard.payload));
    assert.equal(guard.payload.gateVerdictFresh, true, 'the success path must be self-describing, not merely absent a refusal');
  });

  test('a stale verdict is still turned into a refusal (the override path is unaffected)', async t => {
    const root = await fixture(t);
    const { invoke, scratch } = await claimAndPrepGuard(root);
    // headSha deliberately does not match current HEAD.
    await fs.writeFile(path.join(scratch, 'tick_gates.json'), JSON.stringify({ state: 'complete', exitCode: 0, headSha: '0'.repeat(40), steps: [], skipped: [] }));
    const guard = await invoke(['guard', 'ZEN-7']);
    assert.notEqual(guard.exitCode, 0);
    assert.equal(guard.payload.gateVerdictFresh, false);
  });
});

// ═══ (C) claim/next resolve governing rulings, tested against the pure ═══
// ═══     renderer and matcher — never against a live claim ══════════════

describe('C: rulings resolution at claim time (renderRulingsMarkdown / matchGoverningRulings)', () => {
  test('category declares no decisionLog/decisionIdPrefix — the file says so plainly, not silently empty', () => {
    const markdown = renderRulingsMarkdown('ZEN-74', { goal: 'Bump a pin.', definitionOfDone: [] }, null);
    assert.match(markdown, /declares no decision log/i);
    assert.match(markdown, /ZEN-74/);
  });

  test('decisionLog declared but unreadable — distinguishable from "declares none" and from "found nothing"', () => {
    const markdown = renderRulingsMarkdown(
      'ZM-23',
      { decisionLog: 'Games/Zenithmon/Docs/DecisionLog.md', decisionIdPrefix: 'ZM-D-', goal: 'x', definitionOfDone: [] },
      null, // simulates a read failure — the caller never had text to scan
    );
    assert.match(markdown, /could not be read/i);
  });

  test('decisionLog declared, nothing in it matches the Goal/DoD — reads as an unverified scan, not "clean"', () => {
    const decisionLogText = '## 2026-01-01 -- ZM-D-1 -- an entirely unrelated font-atlas decision\n\nNothing to do with trainers.\n';
    const markdown = renderRulingsMarkdown(
      'ZEN-1',
      {
        decisionLog: 'Games/Zenithmon/Docs/DecisionLog.md',
        decisionIdPrefix: 'ZM-D-',
        goal: 'Rename a local variable in the shader compiler for clarity.',
        definitionOfDone: [{ text: 'The rename compiles and every existing test still passes.', done: false }],
      },
      decisionLogText,
    );
    assert.match(markdown, /No entry matched/);
    assert.doesNotMatch(markdown, /ZM-D-1\b/);
  });

  test('decisionLog declared, an entry matches the Goal/DoD — listed as a candidate with its ID and text', () => {
    // Three shared identifiers (ZM_TRAINER_ROUTE1_RAMBLER, ZM_STORY_FLAG_ID,
    // ZM_TrainerEngagementLatch) plus a direct `ZM-D-208` citation in the
    // goal — the shape a real ticket citing its governing ruling by name
    // takes, and needed to clear RULINGS_FILE_MIN_SCORE now that a bare
    // shared common word no longer does (see the recalibration note above
    // `matchGoverningRulings`, `Tools/Vault/cli/legacy-pure/ticket.mjs`).
    const decisionLogText = [
      '## 2026-08-24 -- ZM-D-208 -- trainer prize-money flags',
      '',
      'Both ZM_TRAINER_ROUTE1_RAMBLER and the new trainer get their own ZM_STORY_FLAG_ID; a flagless '
      + 'trainer\'s prize money is farmable across restarts because ZM_TrainerEngagementLatch never persists.',
      '',
    ].join('\n');
    const markdown = renderRulingsMarkdown(
      'ZM-23',
      {
        decisionLog: 'Games/Zenithmon/Docs/DecisionLog.md',
        decisionIdPrefix: 'ZM-D-',
        goal: 'Give ZM_TRAINER_ROUTE1_RAMBLER its own ZM_STORY_FLAG_ID (ZM-D-208) instead of relying on '
          + 'ZM_TrainerEngagementLatch, so its rematch reward cannot be farmed.',
        definitionOfDone: [{ text: 'A repeat battle against the same trainer pays out only once.', done: false }],
      },
      decisionLogText,
    );
    assert.match(markdown, /candidate ruling/i);
    assert.match(markdown, /## ZM-D-208/);
    assert.match(markdown, /ZM_STORY_FLAG_ID/);
  });

  test('matchGoverningRulings: recall threshold (rulings.md, minScore 1) is looser than the review threshold (minScore 2)', () => {
    // Exactly one shared distinctive noun ("farmable"), zero shared
    // identifiers — score 1. Clears (C)'s threshold, not (F)'s.
    const decisionLogText = [
      '## 2026-01-01 -- ZM-D-100 -- trainer economy ruling',
      '',
      'Every trainer stays farmable unless a flag stops the repeat win from paying out again.',
      '',
    ].join('\n');
    const subject = 'Confirm the shop payout cannot be farmable a second time.';
    const rulingsFileHits = matchGoverningRulings(decisionLogText, 'ZM-D-', subject, { minScore: 1 });
    const reviewHits = matchGoverningRulings(decisionLogText, 'ZM-D-', subject, { minScore: 2 });
    assert.ok(rulingsFileHits.some(hit => hit.id === 'ZM-D-100'), 'a single shared noun clears the recall (rulings.md) threshold');
    assert.equal(reviewHits.length, 0, 'but not the tighter review threshold');
  });

  test('matchGoverningRulings: a single shared identifier clears both thresholds on its own (weighted double)', () => {
    const decisionLogText = '## 2026-01-01 -- ZM-D-9 -- unrelated heading\n\nMentions ZM_UNIQUE_FLAG_TOKEN once, nothing else in common.\n';
    const subject = 'Totally different wording, but also cites ZM_UNIQUE_FLAG_TOKEN.';
    const rulingsFileHits = matchGoverningRulings(decisionLogText, 'ZM-D-', subject, { minScore: 1 });
    const reviewHits = matchGoverningRulings(decisionLogText, 'ZM-D-', subject, { minScore: 2 });
    assert.ok(rulingsFileHits.some(hit => hit.id === 'ZM-D-9'));
    assert.ok(reviewHits.some(hit => hit.id === 'ZM-D-9'), 'one identifier hit alone must clear the tighter threshold too');
  });

  test('matchGoverningRulings returns nothing for empty/undeclared inputs, never throws', () => {
    assert.deepEqual(matchGoverningRulings('', 'ZM-D-', 'subject text'), []);
    assert.deepEqual(matchGoverningRulings('## ZM-D-1 heading\ntext', '', 'subject text'), []);
    assert.deepEqual(matchGoverningRulings('## ZM-D-1 heading\ntext', 'ZM-D-', ''), []);
    assert.deepEqual(matchGoverningRulings(undefined, undefined, undefined), []);
  });
});

// ═══ (D) zagent decide warns (never refuses) when the category declares ═
// ═══     no decisionLog ══════════════════════════════════════════════════

describe('D: zagent decide warns when the resolved category has no decisionLog', () => {
  test('a ticket in a category with no decisionLog (Engine, per the fixture) gets a warning, exit 0', async t => {
    const root = await fixture(t);
    const index = await loadVault(root);
    // ZEN-7 ships with category: Engine, which this fixture's config gives
    // no decisionLog/decisionIdPrefix — see vault.config.json.
    const res = await dispatchWrite(index, ['decide', '--ticket', 'ZEN-7', '--text', 'A decision recorded against an Engine ticket.'], {});
    assert.equal(res.exitCode, 0, JSON.stringify(res.payload));
    assert.ok(res.payload.warning, 'expected a warning field on the payload');
    assert.match(res.payload.warning, /Engine/);
    assert.match(res.payload.warning, /decisionLog/);
  });

  test('a category with a decisionLog configured (Zenithmon, per the fixture) gets no warning', async t => {
    const root = await fixture(t);
    const index = await loadVault(root);
    const res = await dispatchWrite(index, ['decide', '--project', 'ZEN', '--category', 'Zenithmon', '--text', 'A decision recorded against Zenithmon.'], {});
    assert.equal(res.exitCode, 0, JSON.stringify(res.payload));
    assert.equal(res.payload.warning, undefined);
  });

  test('an unmapped project/category does not crash the warning check', async t => {
    const root = await fixture(t);
    const index = await loadVault(root);
    const res = await dispatchWrite(index, ['decide', '--project', 'NOPE', '--category', 'Nonexistent', '--text', 'Whatever.'], {});
    assert.equal(res.exitCode, 0, JSON.stringify(res.payload));
    // No decisionLog is configured for an unmapped project either, so this
    // still warns — the important property is that it does not throw.
    assert.ok(res.payload.warning);
  });
});

// ═══ (F) reviewDecision's third OR term: a recorded ruling governs the ═══
// ═══     ticket's subject matter ═════════════════════════════════════════

describe('F: reviewDecision requires review when a recorded ruling governs the subject matter', () => {
  test('a LOW-risk ticket whose Goal/DoD matches a recorded ruling still requires review', () => {
    // Same strengthened fixture as (C) above: three shared identifiers plus
    // a direct `ZM-D-208` citation, needed to clear REVIEW_RULING_MIN_SCORE
    // now that a bare shared common word no longer does on its own.
    const decisionLogText = [
      '## 2026-08-24 -- ZM-D-208 -- trainer prize-money flags',
      '',
      'Both ZM_TRAINER_ROUTE1_RAMBLER and the new trainer get their own ZM_STORY_FLAG_ID; a flagless '
      + 'trainer\'s prize money is farmable across restarts because ZM_TrainerEngagementLatch never persists.',
      '',
    ].join('\n');
    const result = reviewDecision({
      reviewOn: ['HIGH', 'MEDIUM'], // LOW is not in reviewOn
      risk: 'LOW',
      definitionOfDone: [{ text: 'The Route1 trainer gets its own ZM_STORY_FLAG_ID so the reward cannot repeat.', done: false }],
      goal: 'Stop ZM_TRAINER_ROUTE1_RAMBLER\'s prize money from being farmable (ZM-D-208) instead of relying '
        + 'on ZM_TrainerEngagementLatch.',
      decisionLogText,
      decisionIdPrefix: 'ZM-D-',
    });
    assert.equal(result.required, true, 'ZM-23\'s shape: filed LOW, still governed by a ruling');
    assert.ok(result.reasons.some(reason => reason.includes('ZM-D-208')));
    assert.deepEqual(result.rulingHits, ['ZM-D-208']);
  });

  test('a ticket that matches nothing in the decision log is not forced into review', () => {
    const decisionLogText = '## 2026-08-24 -- ZM-D-208 -- trainer prize-money flags\n\nBoth get their own ZM_STORY_FLAG_ID.\n';
    const result = reviewDecision({
      reviewOn: [],
      risk: 'LOW',
      definitionOfDone: [{ text: 'Rename a completely unrelated local variable in the font atlas exporter.', done: false }],
      goal: 'Cosmetic rename only, no behaviour change.',
      decisionLogText,
      decisionIdPrefix: 'ZM-D-',
    });
    assert.equal(result.required, false);
    assert.equal(result.rulingHits, undefined, 'rulingHits is omitted, not [], when nothing matched — see the frozen-parity note below');
  });

  test('the two older OR terms (reviewOn, DoD assertion language) still work unmodified', () => {
    const byRisk = reviewDecision({ reviewOn: ['HIGH'], risk: 'HIGH', definitionOfDone: [] });
    assert.equal(byRisk.required, true);
    const byAssertion = reviewDecision({
      reviewOn: [], risk: 'LOW',
      definitionOfDone: [{ text: 'The recorder is provably unchanged.', done: false }],
    });
    assert.equal(byAssertion.required, true);
  });

  test('a category with no decision log at all does not crash and does not force review on every ticket', () => {
    const result = reviewDecision({
      reviewOn: [],
      risk: 'LOW',
      definitionOfDone: [{ text: 'Any old text, no decisionLog configured for this category.', done: false }],
      goal: 'Whatever the ticket is about.',
      // decisionLogText / decisionIdPrefix deliberately omitted.
    });
    assert.equal(result.required, false);
    assert.deepEqual(result.reasons, []);
    assert.equal(result.rulingHits, undefined);
  });

  test('the existing compat.mjs call shape (no decisionLogText/decisionIdPrefix at all) is unaffected', () => {
    // Mirrors exactly what compat.mjs's reviewOf() passes today — proof the
    // new term is additive and never crashes the call site it has not been
    // wired into yet.
    const result = reviewDecision({ reviewOn: ['HIGH', 'MEDIUM'], risk: 'LOW', definitionOfDone: [] });
    assert.equal(result.required, false);
  });

  test('the return shape is exactly {required, reasons} when nothing matched — rulingHits must not appear as an empty array', () => {
    // This is the regression `test/parity.test.mjs`'s frozen fixtures would
    // have caught: a field present-but-empty is still a shape change to
    // every existing reader, including a byte-for-byte JSON parity check.
    const result = reviewDecision({ reviewOn: [], risk: 'LOW', definitionOfDone: [] });
    assert.deepEqual(Object.keys(result).sort(), ['reasons', 'required']);
  });
});

// ═══ (E) create() warns, never refuses, when a DoD identifier's blast ═══
// ═══     radius already reaches outside the declared/cited file list ═══

describe('E: create() DoD blast-radius scan', () => {
  async function fixtureWithHit(t) {
    const root = await fixture(t);
    await fs.mkdir(path.join(root, 'hits'), { recursive: true });
    await fs.writeFile(
      path.join(root, 'hits', 'BlastRadiusHit.cpp'),
      '// pre-existing file the DoD identifier already appears in\nconst int ZM_TEST_BLAST_RADIUS_TOKEN = 1;\n',
    );
    await git(root, ['add', '.']);
    await git(root, ['commit', '-qm', 'blast radius fixture file']);
    return root;
  }

  test('warns and names the file when the identifier already lives outside the cited file list', async t => {
    const root = await fixtureWithHit(t);
    const index = await loadVault(root);
    const body = [
      '## Goal', '', 'Do the thing.', '',
      '## Definition of Done', '', '- [ ] `ZM_TEST_BLAST_RADIUS_TOKEN` behaves correctly', '',
    ].join('\n');
    const res = await dispatchWrite(index, ['create', '--project', 'ZEN', '--title', 'Blast radius warn test', '--text', body], {});
    assert.equal(res.exitCode, 0, JSON.stringify(res.payload));
    assert.ok(res.payload.dodBlastRadiusWarning, 'expected an advisory warning, not a refusal');
    assert.match(res.payload.dodBlastRadiusWarning, /ZM_TEST_BLAST_RADIUS_TOKEN/);
    assert.match(res.payload.dodBlastRadiusWarning, /hits\/BlastRadiusHit\.cpp/);
    assert.equal(res.payload.created, true, 'the ticket is created regardless — this is advisory only');
  });

  test('stays quiet when the hit file is already cited in the ticket body', async t => {
    const root = await fixtureWithHit(t);
    const index = await loadVault(root);
    const body = [
      '## Goal', '', 'See `hits/BlastRadiusHit.cpp` for context.', '',
      '## Definition of Done', '', '- [ ] `ZM_TEST_BLAST_RADIUS_TOKEN` behaves correctly', '',
    ].join('\n');
    const res = await dispatchWrite(index, ['create', '--project', 'ZEN', '--title', 'Blast radius cited test', '--text', body], {});
    assert.equal(res.exitCode, 0, JSON.stringify(res.payload));
    assert.equal(res.payload.dodBlastRadiusWarning, undefined, 'the file is already on the declared/cited list');
  });

  test('stays quiet when the DoD names no CONSTANT_CASE/Prefixed_PascalCase identifier at all', async t => {
    const root = await fixtureWithHit(t);
    const index = await loadVault(root);
    const body = ['## Goal', '', 'Cosmetic only.', '', '## Definition of Done', '', '- [ ] Tidy up a comment', ''].join('\n');
    const res = await dispatchWrite(index, ['create', '--project', 'ZEN', '--title', 'No identifiers', '--text', body], {});
    assert.equal(res.exitCode, 0, JSON.stringify(res.payload));
    assert.equal(res.payload.dodBlastRadiusWarning, undefined);
  });

  test('stays quiet when the identifier does not appear anywhere in the repo', async t => {
    const root = await fixtureWithHit(t);
    const index = await loadVault(root);
    const body = [
      '## Goal', '', 'Add something brand new.', '',
      '## Definition of Done', '', '- [ ] `ZM_TOTALLY_NOVEL_IDENTIFIER_XYZ` is introduced', '',
    ].join('\n');
    const res = await dispatchWrite(index, ['create', '--project', 'ZEN', '--title', 'Novel identifier', '--text', body], {});
    assert.equal(res.exitCode, 0, JSON.stringify(res.payload));
    assert.equal(res.payload.dodBlastRadiusWarning, undefined);
  });

  test('scanIdentifiers extracts CONSTANT_CASE and Prefixed_PascalCase shapes and dedupes', () => {
    const found = scanIdentifiers('Touches `ZM_TRAINER_ROUTE1_RAMBLER` and Zenith_Entity, and ZM_TRAINER_ROUTE1_RAMBLER again.');
    assert.deepEqual(found, ['ZM_TRAINER_ROUTE1_RAMBLER', 'Zenith_Entity']);
  });
});

// ═══ (E2) update --dod / update --body run the same blast-radius scan ═══
// ═══      create() does; update() previously never called it at all ═════
//
// `create()`'s scan (`dodBlastRadiusWarning`) runs BEFORE the ticket file
// is written, so `git grep` — which reads the tracked working tree, not a
// frozen HEAD — never sees the new ticket's own text. `update()`'s scan
// runs AFTER `runOperation` returns, so by scan time `applyJournal` has
// already written the ticket's own file into the worktree carrying whatever
// identifier the `--dod`/`--body` text just introduced — and `git grep`
// searches the tracked WORKING TREE, so it finds it. Left alone, EVERY
// update introducing a new identifier would name the ticket itself as its
// own blast radius; measured before the fix, the text read "1 file(s) using
// them outside this ticket's cited file list: ZEN/tickets/ZEN-7.md".
//
// `update` therefore passes the subject ticket's own `relativePath` to
// `dodBlastRadiusWarning` as `selfPath`, which adds it to the declared set.
// The "stays quiet when cited" test below consequently cites ONLY the
// pre-existing hit file, and the test after it pins the exclusion directly.
// A warning that fires on ~100% of updates is not a signal: the reader
// learns to skip the line, and the genuine hits go with it.

describe('E2: update --dod / update --body run the DoD blast-radius scan', () => {
  async function fixtureWithHit(t) {
    const root = await fixture(t);
    await fs.mkdir(path.join(root, 'hits'), { recursive: true });
    await fs.writeFile(
      path.join(root, 'hits', 'BlastRadiusHit.cpp'),
      '// pre-existing file the DoD identifier already appears in\nconst int ZM_TEST_BLAST_RADIUS_TOKEN = 1;\n',
    );
    await git(root, ['add', '.']);
    await git(root, ['commit', '-qm', 'blast radius fixture file']);
    return root;
  }

  test('update --dod warns and names the file when the identifier lives outside the cited list', async t => {
    const root = await fixtureWithHit(t);
    const index = await loadVault(root);
    const dod = '- [ ] `ZM_TEST_BLAST_RADIUS_TOKEN` behaves correctly';
    const res = await dispatchWrite(index, ['update', 'ZEN-7', '--dod', dod, '--human'], {});
    assert.equal(res.exitCode, 0, JSON.stringify(res.payload));
    // Single source change that reds this test: deleting the whole
    // post-`runOperation` scan block in `update()` (write.mjs), or the
    // `--dod` disjunct of `dodMayHaveChanged` — either leaves
    // `dodBlastRadiusWarning` undefined here.
    // NOT deleting `dodMayHaveChanged` wholesale: that makes the scan run
    // unconditionally, so this test still gets its warning and still passes.
    // Measured — that mutation reds exactly one test, the goal-only one below.
    assert.ok(res.payload.dodBlastRadiusWarning, 'expected an advisory warning on update --dod, mirroring create()');
    assert.match(res.payload.dodBlastRadiusWarning, /ZM_TEST_BLAST_RADIUS_TOKEN/);
    assert.match(res.payload.dodBlastRadiusWarning, /hits\/BlastRadiusHit\.cpp/);
  });

  test('update --dod stays quiet when the hit file is cited', async t => {
    const root = await fixtureWithHit(t);
    const index = await loadVault(root);
    // Cites ONLY the pre-existing hit file. The ticket's own path is not cited
    // and must not need to be — see the next test.
    const dod = '- [ ] `ZM_TEST_BLAST_RADIUS_TOKEN` behaves correctly (see `hits/BlastRadiusHit.cpp`)';
    const res = await dispatchWrite(index, ['update', 'ZEN-7', '--dod', dod, '--human'], {});
    assert.equal(res.exitCode, 0, JSON.stringify(res.payload));
    // Single source change that reds this test: dropping the `!declared.has(value)`
    // filter inside `dodBlastRadiusWarning`.
    assert.equal(res.payload.dodBlastRadiusWarning, undefined, 'the only hit file is cited');
  });

  // ★ THE TICKET IS NEVER ITS OWN BLAST RADIUS, and this is the one case that
  // does not arise for create(). create() scans BEFORE the ticket file exists;
  // update() can only scan AFTER runOperation, which COMMITS — so `git grep`,
  // reading the committed tree, finds the ticket file now carrying the very
  // identifier the new DoD just introduced. Before the `selfPath` exclusion the
  // warning read: "1 file(s) using them outside this ticket's cited file list:
  // ZEN/tickets/ZEN-7.md" on an update whose blast radius was genuinely empty.
  // A warning that fires on ~100% of updates teaches the reader to skip the
  // line, and takes the real hits with it.
  test('update --dod never reports the ticket\'s OWN file as blast radius', async t => {
    const root = await fixtureWithHit(t);
    const index = await loadVault(root);
    // A token that exists NOWHERE in the fixture repo, so the ticket's own
    // just-committed file is the only thing that can possibly match it.
    const dod = '- [ ] `ZM_TEST_SELF_MATCH_ONLY_TOKEN` is handled';
    const res = await dispatchWrite(index, ['update', 'ZEN-7', '--dod', dod, '--human'], {});
    assert.equal(res.exitCode, 0, JSON.stringify(res.payload));
    // Single source change that reds this test: dropping the `selfPath`
    // argument at dodBlastRadiusWarning's call site in update(), or the
    // `declared.add(selfPath)` line inside it.
    assert.equal(
      res.payload.dodBlastRadiusWarning, undefined,
      'the only possible hit is the ticket\'s own committed file, which is not blast radius');
  });

  test('an update that does not touch the DoD runs no scan at all, even though the ticket already carries a blast-radius hit', async t => {
    const root = await fixtureWithHit(t);
    // Seed the ticket's Definition of Done with the (uncited) blast-radius
    // identifier first — the test above proves this alone produces a
    // warning when the DoD update runs the scan.
    const seeded = await dispatchWrite(await loadVault(root), [
      'update', 'ZEN-7', '--dod', '- [ ] `ZM_TEST_BLAST_RADIUS_TOKEN` behaves correctly', '--human',
    ], {});
    assert.equal(seeded.exitCode, 0, JSON.stringify(seeded.payload));
    assert.ok(seeded.payload.dodBlastRadiusWarning, 'sanity: the seeded DoD does warn when the DoD update runs the scan');

    // Now touch only --goal. The composed body still carries the same
    // (uncited, outside-hitting) DoD text unchanged, so a warning here
    // would mean the scan ran on an update that never touched the DoD.
    const res = await dispatchWrite(await loadVault(root), [
      'update', 'ZEN-7', '--goal', 'A new goal, unrelated to the DoD.', '--human',
    ], {});
    assert.equal(res.exitCode, 0, JSON.stringify(res.payload));
    // Single source change that reds this test: removing the
    // `dodMayHaveChanged` gate entirely (always scanning `composedBody`
    // whenever it is defined) in `update()` (write.mjs) — the test above
    // proves the scan WOULD warn here if it ran.
    assert.equal(res.payload.dodBlastRadiusWarning, undefined, '--goal alone must not trigger the DoD scan');
  });
});

// ═══ (G) --goal (and every other path-valued flag) is resolved PER ═══════
// ═══     COMMAND, not per flag name ══════════════════════════════════════

describe('G: path-valued flags are resolved per command', () => {
  test('sprint create --goal "prose" does not attempt a file read, even when the text looks path-like', async () => {
    // Pre-fix this threw "Cannot read --goal …: ENOENT" — `readInputFiles`
    // tried to open the prose as a file because `--goal` was file-valued
    // for every command. An unhandled rejection here fails the test.
    const files = await readInputFiles(['sprint', 'create', 'S1', '--goal', 'This is prose, not a path — reading it as a file would ENOENT.'], {});
    assert.equal(files.goal, undefined, 'sprint has no file-valued flags; write.mjs\'s sprint() reads --goal straight off argv');
  });

  test('sprint gate --goal is likewise never treated as a file (same command, different subcommand)', async () => {
    const files = await readInputFiles(['sprint', 'gate', 'S1', 'ZEN-1', '--goal', 'Also prose.'], {});
    assert.equal(files.goal, undefined);
  });

  test('update --goal <file> still resolves the file\'s contents — the documented ticket-form behaviour', async t => {
    const dir = await fs.mkdtemp(path.join(os.tmpdir(), 'goal-file-'));
    t.onTestFinished(() => fs.rm(dir, { recursive: true, force: true }));
    const filePath = path.join(dir, 'goal.md');
    await fs.writeFile(filePath, 'The real Goal section contents.');
    const files = await readInputFiles(['update', 'ZEN-1', '--goal', filePath], { cwd: dir });
    assert.equal(files.goal, 'The real Goal section contents.');
  });

  test('update --goal falls back to inline text when the path does not exist (lenient, unlike gates/guard)', async () => {
    const files = await readInputFiles(['update', 'ZEN-1', '--goal', 'Not a real path on disk.'], {});
    assert.equal(files.goal, 'Not a real path on disk.');
  });

  test('audit: every command that carries a file/comment/worklog/body/goal/dod flag resolves it the documented way', () => {
    const flagsOf = command => [...pathValuedFlagsFor(command).flags].sort();

    assert.deepEqual(flagsOf('update'), ['body', 'dod', 'goal']);
    assert.equal(pathValuedFlagsFor('update').mode, 'lenient');

    assert.deepEqual(flagsOf('create'), ['body', 'file']);
    assert.equal(pathValuedFlagsFor('create').mode, 'lenient');

    assert.deepEqual(flagsOf('finish'), ['comment', 'worklog']);
    assert.equal(pathValuedFlagsFor('finish').mode, 'lenient');

    assert.deepEqual(flagsOf('comment'), ['comment', 'file']);
    assert.equal(pathValuedFlagsFor('comment').mode, 'lenient');

    for (const command of ['ask', 'decide', 'shortfall', 'changelog', 'suggest']) {
      assert.deepEqual(flagsOf(command), ['file'], `${command} --file`);
      assert.equal(pathValuedFlagsFor(command).mode, 'lenient', `${command} --file should fall back to inline text`);
    }

    // Strict: a typo here must never silently weaken a verdict or write the
    // wrong page — never fall back to treating the typo as literal content.
    for (const command of ['gates', 'guard', 'docs']) {
      assert.deepEqual(flagsOf(command), ['file'], `${command} --file`);
      assert.equal(pathValuedFlagsFor(command).mode, 'strict', `${command} --file must stay strict`);
    }

    // No file-valued flags at all — `--goal` on `sprint` is the case that
    // was broken; the rest are here so the audit is of the whole table, not
    // just the one flag that happened to be reported.
    for (const command of ['next', 'claim', 'move', 'link', 'unlink', 'parent', 'estimate', 'flag', 'resolve', 'sprint', 'version']) {
      assert.deepEqual(flagsOf(command), [], `${command} should have no file-valued flags`);
    }
  });

  test('--repro is never path-valued on any command (sanity check: it is a scalar enum, always prose)', () => {
    assert.ok(!FILE_FLAGS.includes('repro'));
    for (const command of ['create', 'update']) {
      assert.ok(!pathValuedFlagsFor(command).flags.includes('repro'), `${command} --repro must stay a scalar`);
    }
  });

  test('an unknown/future command name resolves to no file-valued flags rather than throwing', () => {
    const result = pathValuedFlagsFor('not-a-real-command');
    assert.deepEqual([...result.flags], []);
    assert.equal(result.mode, 'lenient');
  });
});
