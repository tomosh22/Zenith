import { test } from 'vitest';
import assert from 'node:assert/strict';
import { loadVault } from '../server/main.js';
import { dispatchLocal } from '../cli/local.mjs';
import { fileURLToPath } from 'node:url';

test('doctor reports actual indexed fixture counts',async()=>{
  const index=await loadVault(fileURLToPath(new URL('../fixtures/snapshot/vault',import.meta.url)));
  const r=dispatchLocal(index,['doctor']);
  assert.equal(r.exitCode,0);assert.equal(r.payload.ticketCount,244);assert.deepEqual(r.payload.diagnostics,[]);
});
test('guard evaluates changed files without executing gates',()=>{
  const index={config:{policy:{resolved:{guardrails:{protectedPaths:['.github/**']}}}}};
  assert.equal(dispatchLocal(index,['guard'],{file:'.github/workflows/test.yml'}).exitCode,1);
  assert.equal(dispatchLocal(index,['guard'],{file:'Zenith/Core/Z_Core.cpp'}).exitCode,0);
});

test('mutable local guard does not request a retired remote docs mirror',()=>{
  const index={vaultConfig:{writes:{enabled:true}},config:{policy:{resolved:{guardrails:{protectedPaths:[]}}}}};
  const response=dispatchLocal(index,['guard'],{file:'Games/Foundry/Docs/Status.md'});
  assert.equal(response.exitCode,0);
  assert.equal(response.payload.docsMirrorNeeded,false);
  assert.deepEqual(response.payload.docsMirrorPaths,[]);
});
test('local docs read/search preserve text and refuse writes',()=>{
  const doc={kind:'document',relativePath:'Docs/Notes.md',rawText:'<!-- keep -->\nhello world',metadata:{}};
  const index={files:new Map([['Docs/Notes.md',doc]]),valid:true,diagnostics:[]};
  assert.equal(dispatchLocal(index,['docs','read','Docs/Notes.md']).payload.markdown,doc.rawText);
  assert.equal(dispatchLocal(index,['docs','search','hello']).payload.length,1);
  assert.equal(dispatchLocal(index,['docs','write','Docs/Notes.md'],{file:'bad'}).exitCode,1);
});

test('local board status finds missing references, changed subjects, and unlisted epics',()=>{
  const doc={kind:'document',relativePath:'Docs/Roadmap.md',bodyText:'[FD-1] Brick oven\n[FD-99] Gone ticket'};
  const tickets=[
    {metadata:{key:'FD-1',type:'STORY',title:'Water simulation'}},
    {metadata:{key:'FD-2',type:'EPIC',title:'Lighting system'}},
    {metadata:{key:'FD-3',type:'TASK',title:'Implementation detail'}},
  ];
  const index={files:new Map([['roadmap',doc]]),tickets:new Map(tickets.map(t=>[t.metadata.key,t]))};
  const response=dispatchLocal(index,['board','status','--project','FD']);
  assert.equal(response.exitCode,1);
  assert.deepEqual(response.payload.rows.map(r=>[r.kind,r.key]),[
    ['title-mismatch','FD-1'],['missing-issue','FD-99'],['unlisted','FD-2'],
  ]);
  assert.equal(response.payload.issuesOnBoard,3);
});
