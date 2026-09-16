import { test } from 'vitest';
import assert from 'node:assert/strict';
import { fileURLToPath } from 'node:url';
import { readFile } from 'node:fs/promises';
import { loadVault } from '../server/main.js';
import { dispatchTransitions } from '../cli/transitions.mjs';

test('workflow graph matches all captured transition choices and follows canonical status edits',async()=>{
  const fixture=new URL('../fixtures/snapshot/',import.meta.url);
  const index=await loadVault(fileURLToPath(new URL('vault',fixture)));
  const reference=JSON.parse(await readFile(new URL('reference.json',fixture),'utf8'));
  for (const c of reference.cases.filter(c=>c.argv[0]==='transitions')) {
    assert.deepEqual(dispatchTransitions(index,c.argv),{exitCode:c.exitCode,payload:c.payload},c.argv[1]);
  }
  const ticket=index.tickets.get('FD-18');
  ticket.metadata.status='Done';
  const response=dispatchTransitions(index,['transitions','FD-18']);
  assert.equal(response.payload.status,'Done');
  assert.ok(response.payload.transitions.some(t=>t.name==='Reopen'));
  assert.ok(response.payload.transitions.every(t=>t.toStatus!=='Done'));
  ticket.metadata.source={};ticket.source={};ticket.fields.source={};
  assert.deepEqual(dispatchTransitions(index,['transitions','FD-18']),response,'workflow lookup does not require a database identity');
});
