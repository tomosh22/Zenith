import { test } from 'vitest';
import assert from 'node:assert/strict';
import { fileURLToPath } from 'node:url';
import { readFile } from 'node:fs/promises';
import { loadVault } from '../server/main.js';
import { dispatchReports } from '../cli/reports.mjs';

test('epic and velocity reports match the frozen capture and recalculate changed points',async()=>{
  const fixture=new URL('../fixtures/snapshot/',import.meta.url);
  const index=await loadVault(fileURLToPath(new URL('vault',fixture)));
  const reference=JSON.parse(await readFile(new URL('reference.json',fixture),'utf8'));
  for(const c of reference.cases.filter(c=>['epic','report'].includes(c.argv[0])))
    assert.deepEqual(dispatchReports(index,c.argv),{exitCode:c.exitCode,payload:c.payload},c.argv.join(' '));
  const child=[...index.tickets.values()].find(t=>t.metadata.parent&&t.metadata.storyPoints!==null);
  const key=child.metadata.parent;
  const before=dispatchReports(index,['epic',key]).payload;
  child.metadata.storyPoints+=10;
  assert.equal(dispatchReports(index,['epic',key]).payload.points,before.points+10);
});
