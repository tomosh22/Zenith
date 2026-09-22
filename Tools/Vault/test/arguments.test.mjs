import { test } from 'vitest';
import assert from 'node:assert/strict';
import { inspectArguments } from '../cli/arguments.mjs';

test('a misspelled read flag fails closed and suggests its intended spelling', () => {
  assert.match(inspectArguments(['queue','--projectt','ZM']).error, /did you mean --project/);
  assert.equal(inspectArguments(['queue','--project','ZM']).error, null);
});

test('help precedes flags and local file input additions are explicit', () => {
  assert.equal(inspectArguments(['claim','--help']).help, true);
  assert.equal(inspectArguments(['guard','FD-18','--gates','selection.json']).error, null);
  assert.match(inspectArguments(['queue','--gates','selection.json']).error, /Unknown flag/);
  assert.equal(inspectArguments(['board','status','--roadmap','Docs/Roadmap.md']).error, null);
  assert.match(inspectArguments(['docs','read','Docs/Notes.md','--live']).error,/Local docs commands do not support --live/);
});

// ★ REGRESSION. `--harness` had a validator, and unit tests for it, and was
// still refused by the real binary: `claim()` in write.mjs reads it on its
// first line, but the closed vocabulary in legacy-pure/flags.mjs did not list
// it, so every `zagent claim <KEY> --harness claude` — the exact invocation
// every harness entry point mandates at step 1 of the tick protocol — came
// back exit 1 "Unknown flag". The existing coverage exercised the VALIDATOR
// directly and never crossed this table, which is the whole gap: a green
// suite is not proof the client runs.
//
// This is the third instance of the shape flags.mjs's own header documents
// (`sprint --force`, `update --category`), so it is worth pinning rather than
// fixing again. `claim` and `next` are ONE function behind the vocabulary
// (queueMode), so both spellings must be listed or the queue path breaks
// while the targeted one works.
test('claim and next accept --harness, the flag every tick passes at step 1', () => {
  assert.equal(inspectArguments(['claim','ZEN-106','--harness','claude']).error, null);
  assert.equal(inspectArguments(['next','--project','ZEN','--harness','claude']).error, null);
});
