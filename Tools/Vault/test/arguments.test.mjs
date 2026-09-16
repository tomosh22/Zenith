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
