import { readFileSync, writeFileSync, mkdirSync, existsSync } from 'node:fs';
import path from 'node:path';
import { pathToFileURL } from 'node:url';
import { captureOrdering } from './capture-ordering.mjs';

// Run with the legacy repository's tsx loader. No legacy files are edited.
const [legacyRoot, fixtureRoot, clientRoot] = process.argv.slice(2);
if (!legacyRoot || !fixtureRoot || !clientRoot) throw new Error('Usage: capture.ts LEGACY_ROOT FIXTURE_ROOT CLIENT_ROOT');
if (existsSync(path.join(fixtureRoot,'reference.json'))) throw new Error('Reference capture already exists; use a fresh fixture directory.');
const url = new URL(process.env.DATABASE_URL ?? '');
if (url.hostname !== '127.0.0.1' || url.port !== '55439' || url.pathname !== '/vault_snapshot' || url.username !== 'vault_reader') {
  throw new Error('Reference capture requires the dedicated local read-only snapshot account.');
}
const fromLegacy = (relative: string) => import(pathToFileURL(path.join(legacyRoot, relative)).href);
const { dispatch } = await fromLegacy('packages/agent/src/dispatch.ts');
const { loadBoardConfig, requestConfig, clientProjectSchema } = await fromLegacy('packages/agent/src/config.ts');
const { closeDb, db } = await fromLegacy('packages/database/src/index.ts');
const { sql } = await import(pathToFileURL(path.join(legacyRoot, 'packages/agent/node_modules/drizzle-orm/index.js')).href);
mkdirSync(fixtureRoot, { recursive: true });
const board = loadBoardConfig();
const client = { repo: clientRoot.replaceAll('\\', '/'), file: clientProjectSchema.parse(JSON.parse(readFileSync(path.join(clientRoot, 'zagent.project.json'), 'utf8'))) };
const config = requestConfig(board, client);
writeFileSync(path.join(fixtureRoot, 'config.json'), JSON.stringify({ board, client, resolved: config }, null, 2) + '\n');
try {
  const issues = await db.execute(sql.raw('SELECT p.key || \'-\' || i.number AS key, i.type FROM jira.issues i JOIN jira.projects p ON p.id=i.project_id ORDER BY p.key, i.number'));
  const projects = [...new Set(issues.map((r: any) => String(r.key).split('-')[0]))];
  const cases: any[] = [];
  const capture = async (argv: string[], files: Record<string,string> = {}) => {
    const result = await dispatch({ argv: [...argv, '--json'], files, client, board });
    // Do not apply result.writes (gates returns scratch-file suggestions).
    cases.push({ argv, files, exitCode: result.exitCode, payload: result.payload });
  };
  for (const row of issues) {
    for (const command of ['show', 'check', 'owns', 'links', 'transitions', 'history']) await capture([command, row.key]);
    await capture(['gates', row.key], { file: '' });
    if (row.type === 'EPIC') await capture(['epic', row.key]);
  }
  for (const project of projects as string[]) {
    for (const command of [['queue'], ['blocked'], ['sprint', 'list'], ['version', 'list'], ['report', 'velocity'], ['report', 'epic']]) {
      await capture([...command, '--project', project]);
    }
  }
  for (const name of ['decisions', 'questions', 'shortfalls', 'changelog', 'suggestions']) await capture(['rows', name]);
  await capture(['suggestions']);
  await capture(['suggestions', '--all']);
  await capture(['rows', 'questions', '--limit', '1']);
  await capture(['rows', 'suggestions', '--where', 'Status=new']);
  for (const key of ['FD-18','ZM-1','DP-1']) {
    await capture(['history',key,'--limit','1']);
    await capture(['gates',key], { file:'Zenith/Core/Z_Core.h\nGames/Foundry/Docs/Status.md' });
  }
  for (const file of ['', 'Zenith/Core/Z_Core.h', '.github/workflows/check.yml\nGames/Foundry/Docs/Roadmap.md']) await capture(['guard'],{file});
  await capture(['guard','FD-18'],{file:''});
  await capture(['guard']);
  await capture(['guard','FD-18'],{file:'',gates:'not json'});
  await capture(['guard','FD-18'],{file:'Zenith/Core/Z_Core.h',gates:JSON.stringify({gates:[],changed:[]})});
  const recorded = cases.find(c => c.argv[0]==='gates' && c.argv[1]==='FD-18' && c.files.file==='').payload;
  await capture(['guard','FD-18'],{file:'',gates:JSON.stringify(recorded)});
  for (const argv of [['queue'],['blocked'],['sprint','list'],['version','list'],['report','epic'],['report','velocity']]) {
    await capture(argv);
    await capture([...argv,'--project','MISSINGPROJECT']);
  }
  for (const command of ['show','check','owns','links','transitions','history','epic','gates']) {
    for (const tail of [[],['ZEN-999999'],['invalid-key']]) await capture([command,...tail],{file:''});
  }
  await capture(['queue','--projectt','ZM']);
  await capture(['rows','not-a-database']);
  await capture(['history','FD-18','--limit','invalid']);
  writeFileSync(path.join(fixtureRoot, 'reference.json'), JSON.stringify({ formatVersion: 1, issueCount: issues.length, cases }, null, 2) + '\n');
  // The old links UNION has no ORDER BY. Preserve only its observed identity
  // order as export metadata, not response fields, so readers remain dynamic.
  const activities=await db.execute(sql.raw(`SELECT a.id, p.key || '-' || i.number AS key, a.field,
    a.from_value AS "fromValue", a.to_value AS "toValue", a.created_at::text AS "createdAt", u.name AS actor
    FROM jira.issue_activity a JOIN jira.issues i ON i.id=a.issue_id JOIN jira.projects p ON p.id=i.project_id
    LEFT JOIN public.users u ON u.id=a.actor_id`));
  writeFileSync(path.join(fixtureRoot, 'ordering.json'), JSON.stringify(captureOrdering(cases,activities),null,2)+'\n');
  console.log(`Captured ${cases.length} reference cases for ${issues.length} issues.`);
} finally { await closeDb(); }
