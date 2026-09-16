import { readFileSync, readdirSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { fileURLToPath } from 'node:url';
import { parseMarkdown } from '../server/parser.js';

export function canonical(value) {
  if (Array.isArray(value)) return value.map(canonical);
  if (value && typeof value === 'object') return Object.fromEntries(Object.keys(value).sort().map(k=>[k,canonical(value[k])]));
  return value;
}
export function tableDigests(tables) {
  return Object.fromEntries(Object.entries(tables).map(([name,rows])=>[name,{
    count:rows.length,
    sha256:createHash('sha256').update(JSON.stringify(rows.map(r=>JSON.stringify(canonical(r))).sort())).digest('hex')
  }]));
}
export function reconstructExport(root) {
  const tables={};
  const add=(name,rows)=>{ (tables[name]??=[]).push(...rows); };
  function walk(dir) {
    for(const entry of readdirSync(dir,{withFileTypes:true})) {
      const file=path.join(dir,entry.name);
      if(entry.isDirectory()) { walk(file); continue; }
      if(!entry.name.endsWith('.md')) continue;
      const p=parseMarkdown(readFileSync(file)); const m=p.fields;
      if(m.kind==='metadata') {
        for(const [name,rows] of Object.entries(m.tables??{})) add(name,rows);
        for(const [name,rows] of Object.entries(m.archivedTables??{})) add(name,rows);
      } else if(m.key && m.source) {
        // The source row excludes description: Markdown body is the only copy.
        add('jira.issues',[{...m.source,description:m.source.description === null && p.bodyRaw.length === 0 ? null : '\\x'+p.bodyRaw.toString('hex')}]);
      } else if(m.kind==='sprint') add('jira.sprints',[m.source]);
      else if(m.kind==='labels') add('jira.labels',m.rows);
      else if(m.kind==='versions') add('jira.versions',m.rows);
      else if(m.kind==='workflow') {
        add('jira.workflows',m.rows); add('jira.workflow_statuses',m.statuses); add('jira.workflow_transitions',m.transitions);
      } else if(m.kind==='history') add('jira.issue_activity',m.rows);
    }
  }
  walk(root);
  return tables;
}
export function auditExport(root,expected) {
  const actual=tableDigests(reconstructExport(root));
  return [...new Set([...Object.keys(actual),...Object.keys(expected)])].filter(name=>JSON.stringify(actual[name])!==JSON.stringify(expected[name])).map(name=>({table:name,expected:expected[name],actual:actual[name]}));
}
if(process.argv[1] && path.resolve(process.argv[1])===fileURLToPath(import.meta.url)) {
  const [root,source,output]=process.argv.slice(2);
  const expected=tableDigests(JSON.parse(readFileSync(source,'utf8')).tables);
  const differences=auditExport(root,expected);
  if(output) writeFileSync(output,JSON.stringify(expected,null,2)+'\n');
  console.log(JSON.stringify({tables:Object.keys(expected).length,differences},null,2));
  if(differences.length) process.exitCode=1;
}
