import { readFileSync, writeFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { execFileSync } from 'node:child_process';
import path from 'node:path';
const [legacyRoot, source, fixture] = process.argv.slice(2);
if (!fixture) throw new Error('Usage: provenance.mjs LEGACY_ROOT SOURCE_JSON FIXTURE_ROOT');
const sha = file => createHash('sha256').update(readFileSync(file)).digest('hex');
const snapshot = JSON.parse(readFileSync(source,'utf8'));
const files = ['packages/agent/src/dispatch.ts','packages/agent/src/board.ts','packages/agent/src/agile.ts','packages/agent/src/notion.ts','packages/agent/src/ticket.ts','packages/agent/src/routing.ts','packages/agent/src/config.ts','packages/agent/src/guardrails.ts','packages/agent/src/args.ts','packages/agent/src/flags.ts','packages/database/src/schema/jira.ts','packages/database/src/schema/notion.ts'];
writeFileSync(path.join(fixture,'provenance.json'), JSON.stringify({
  formatVersion:1,
  legacyCommit:execFileSync('git',['-c',`safe.directory=${legacyRoot.replaceAll('\\','/')}`,'-C',legacyRoot,'rev-parse','HEAD'],{encoding:'utf8'}).trim(),
  legacySourceHashes:Object.fromEntries(files.map(file=>[file,sha(path.join(legacyRoot,file))])),
  sourceSha256:sha(source),
  referenceSha256:sha(path.join(fixture,'reference.json')),
  orderingSha256:sha(path.join(fixture,'ordering.json')),
  configSha256:sha(path.join(fixture,'config.json')),
  issueCount:snapshot.tables['jira.issues'].length,
  counts:Object.fromEntries(Object.entries(snapshot.tables).map(([name,rows])=>[name,rows.length])),
  note:'Read-only legacy dispatcher capture and export use the same isolated restored database. SHA hashes record actual source files, including uncommitted changes. No credentials or authentication tables are exported.'
},null,2)+'\n');
writeFileSync(path.join(fixture,'schema.json'),JSON.stringify(snapshot.columns,null,2)+'\n');
