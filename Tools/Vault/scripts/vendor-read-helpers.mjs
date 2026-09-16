import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { createHash } from 'node:crypto';
import { transform } from 'esbuild';

const legacy=path.resolve(process.argv[2]??'C:/dev/saas');
const out=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../cli/legacy-pure');
mkdirSync(out,{recursive:true});
const hashes={};
const source=name=>{
  const bytes=readFileSync(path.join(legacy,'packages/agent/src',name+'.ts'));
  hashes[name+'.ts']=createHash('sha256').update(bytes).digest('hex');
  return bytes.toString('utf8');
};
async function emit(name,text) {
  const transformed=await transform(text,{loader:'ts',format:'esm',target:'node22'});
  const code=transformed.code.replace(/from "\.\/(ticket|routing|guardrails|config)"/g,'from "./$1.mjs"');
  writeFileSync(path.join(out,name+'.mjs'),'// Generated from saas read-only policy helpers; see provenance.json. No DB or network imports.\n'+code);
}
for(const name of ['ticket','routing','guardrails']) await emit(name,source(name));
await emit('flags', source('flags').replace("import { BoardError } from './board';", 'class BoardError extends Error {}'));
const args=source('args');
const parserStart=args.indexOf('export function parseArgs(');
const parserEnd=args.indexOf('\nexport function flag(',parserStart);
if(parserStart<0||parserEnd<0) throw new Error('Legacy parseArgs boundaries changed; review vendor extraction.');
await emit('args',args.slice(parserStart,parserEnd));
const board=source('board');
const start=board.indexOf('export function evaluateContract(');
const end=board.indexOf('\nasync function buildPayload(',start);
if(start<0||end<0) throw new Error('Legacy evaluateContract boundaries changed; review vendor extraction.');
await emit('contract',`import {projectConfig} from './config';
import {categoryContractError,resolveRules} from './routing';
import {parseTicket,validateTicket,danglingBoardReferences,humanOnlyClaims} from './ticket';
import {protectedPathsMentioned} from './guardrails';
const NEEDS_HUMAN_LABEL='needs-human',HUMAN_GATE_LABEL='human-gate';
${board.slice(start,end)}`);
await emit('config',`export class ConfigError extends Error { constructor(message:string){super(message);this.name='ConfigError';} }
export function projectConfig(config:any,key:string){const p=config.projects[key];if(!p)throw new ConfigError('Project '+key+' is not mapped in agent.config.json. Known: '+(Object.keys(config.projects).join(', ')||'(none)'));return p;}`);
writeFileSync(path.join(out,'provenance.json'),JSON.stringify({source:'saas/packages/agent/src',hashes,notes:'ticket/routing/guardrails transpiled unchanged. evaluateContract and parseArgs extracted unchanged. flags uses a local Error subclass in place of the database module import. config shim only looks up already-resolved pinned policy. Regenerate with scripts/vendor-read-helpers.mjs; never imports legacy repo at runtime.'},null,2)+'\n');
console.log('Vendored read-only policy helpers.');
