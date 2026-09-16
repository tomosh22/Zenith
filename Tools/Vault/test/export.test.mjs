import { test } from 'vitest';
import assert from 'node:assert/strict';
import { mkdtemp, readFile, readdir, rm } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { exportVault } from '../scripts/export.mjs';
import { parseMarkdown } from '../server/parser.js';
import { auditExport } from '../scripts/audit-export.mjs';
import { validateInventory } from '../scripts/validate-inventory.mjs';
import { fileURLToPath } from 'node:url';

const sample = () => ({ tables: {
  'jira.projects': [{id:'p',key:'FD'}],
  'jira.issues': [{id:'i',project_id:'p',number:1,title:'Title',type:'TASK',status_id:'s',priority:'HIGH',complexity:'SIMPLE',risk:'LOW',position:1,description:'\\x'+Buffer.from('<!--keep-->\r\n## Goal\r\nHello\n').toString('hex')}],
  'jira.workflow_statuses': [{id:'s',workflow_id:'w',name:'To Do',category:'TODO'}],
  'jira.workflows': [{id:'w',project_id:'p',name:'Default'}],
} });
const policy = {resolved:{projects:{FD:{}}}};
test('committed export reconstructs every captured entity without field loss', async()=>{
  const fixture=fileURLToPath(new URL('../fixtures/snapshot/',import.meta.url));
  const expected=JSON.parse(await readFile(path.join(fixture,'entity-digests.json'),'utf8'));
  assert.deepEqual(auditExport(path.join(fixture,'vault'),expected),[]);
});
test('inventory covers captured schema and rejects an unreviewed column', async()=>{
  const inventory=JSON.parse(await readFile(new URL('../spec/field-inventory.json',import.meta.url),'utf8'));
  const columns=JSON.parse(await readFile(new URL('../fixtures/snapshot/schema.json',import.meta.url),'utf8'));
  assert.deepEqual(validateInventory(columns,inventory),[]);
  assert.deepEqual(validateInventory([...columns,{table_schema:'jira',table_name:'issues',column_name:'unreviewed_new_field'}],inventory),['Unmapped column: jira.issues.unreviewed_new_field']);
});
test('export preserves original description bytes and unknown source columns', async () => {
  const temp = await mkdtemp(path.join(os.tmpdir(),'vault-export-'));
  try {
    const input=sample(); input.tables['jira.issues'][0].future_field='retain me';
    input.tables['jira.versions']=[{id:'fix',project_id:'p',name:'Fix release'},{id:'affects',project_id:'p',name:'Affected release'}];
    input.tables['jira.issue_versions']=[{issue_id:'i',version_id:'fix',kind:'FIX'},{issue_id:'i',version_id:'affects',kind:'AFFECTS'}];
    exportVault(input,path.join(temp,'one'),policy);
    const bytes=await readFile(path.join(temp,'one/FD/tickets/FD-1.md'));
    const parsed=parseMarkdown(bytes);
    assert.equal(parsed.fields.source.future_field,'retain me');
    assert.deepEqual(parsed.fields.fixVersions,['Fix release']);
    const supporting=parseMarkdown(await readFile(path.join(temp,'one/metadata.md')));
    assert.deepEqual(supporting.fields.tables['jira.issue_versions'],input.tables['jira.issue_versions']);
    assert.ok(bytes.subarray(bytes.indexOf(Buffer.from('<!--keep-->'))).equals(Buffer.from('<!--keep-->\r\n## Goal\r\nHello\n')));
    assert.throws(()=>exportVault(input,path.join(temp,'one'),policy),/already exists/);
    input.tables['jira.issues'][0].description='\\xff';
    assert.throws(()=>exportVault(input,path.join(temp,'invalid'),policy),/not valid UTF-8/);
  } finally { await rm(temp,{recursive:true,force:true}); }
});
test('fresh exports of the same snapshot have identical files', async () => {
  const temp=await mkdtemp(path.join(os.tmpdir(),'vault-export-'));
  try {
    exportVault(sample(),path.join(temp,'one'),policy);
    exportVault(sample(),path.join(temp,'two'),policy);
    const files=await readdir(path.join(temp,'one'),{recursive:true,withFileTypes:true});
    for(const file of files.filter(f=>f.isFile())) {
      const rel=path.relative(path.join(temp,'one'),path.join(file.parentPath,file.name));
      assert.deepEqual(await readFile(path.join(temp,'one',rel)),await readFile(path.join(temp,'two',rel)),rel);
    }
  } finally { await rm(temp,{recursive:true,force:true}); }
});

test('export refuses unexpected rows in entities declared empty',()=>{
  const input=sample();
  input.tables['jira.workflow_transition_rules']=[{id:'new-rule'}];
  assert.throws(()=>exportVault(input,'unused-refused-destination',policy),/Expected empty entity jira.workflow_transition_rules has rows/);
});
