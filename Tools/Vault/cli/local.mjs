import { checkProtectedPaths, docsMirrorNeeded } from './legacy-pure/guardrails.mjs';

const result=(payload,exitCode=0)=>({payload,exitCode});
const flag=(args,name)=>{const n=args.indexOf(name);return n<0?undefined:args[n+1];};
const fail=message=>result({error:message},1);
const mirrorPaths=(index,changed)=>index.vaultConfig?.writes?.enabled===true?[]:docsMirrorNeeded(changed);
const words=text=>new Set(text.toLowerCase().replace(/[^a-z0-9_\s]/g,' ').split(/\s+/).filter(w=>w.length>2));
function sharesSubject(line,title) {
  const a=words(title),b=words(line);if(!a.size)return true;
  const n=[...a].filter(w=>b.has(w)).length;return n>=Math.min(2,a.size)||n*3>=a.size;
}
export function dispatchLocal(index,args,files={},context={}) {
  const [command,sub]=args;
  if(command==='help'||command==='--help'||!command) return result({
    usage:'node cli/zagent.mjs --vault PATH COMMAND [--json]',
    readOnly:true,
    commands:['show','check','queue','owns','links','blocked','transitions','history','gates','guard','epic','sprint list','version list','report epic','report velocity','rows','suggestions','doctor','docs ls','docs read','docs search','docs status','board status'],
  });
  if(command==='doctor') return result({ok:index.valid,diagnostics:index.diagnostics,projects:index.boardRoots.map(r=>r.project),ticketCount:index.tickets.size},index.valid?0:1);
  if(command==='docs') {
    const docs=[...index.files.values()].filter(r=>r.kind==='document');
    if(sub==='ls') return result(docs.map(r=>({path:r.relativePath,title:r.metadata.title??r.relativePath})));
    if(sub==='status') return result({mode:'local',documents:docs.length,diagnostics:index.diagnostics},index.valid?0:1);
    if(sub==='read') {
      const record=docs.find(r=>r.relativePath===args[2]);
      return record?result({path:record.relativePath,markdown:record.rawText}):fail(`No local document ${args[2]??''}.`);
    }
    if(sub==='search') {
      const query=String(args[2]??'').toLowerCase();
      if(!query)return fail('Usage: docs search <TEXT>');
      return result(docs.filter(r=>r.rawText.toLowerCase().includes(query)).map(r=>({path:r.relativePath,title:r.metadata.title??r.relativePath})));
    }
    return fail(`docs ${sub??''} is unavailable in the read-only local vault; remote sync and writes are not implemented.`);
  }
  if(command==='board') {
    if(sub!=='status')return fail('Usage: board status [--project KEY] [--roadmap PATH]');
    const project=flag(args,'--project')??index.config.policy.client.file.project;
    const wanted=(flag(args,'--roadmap')??'Roadmap.md').replaceAll('\\','/').toLowerCase();
    const doc=[...index.files.values()].find(r=>r.kind==='document'&&r.relativePath.toLowerCase().endsWith(wanted));
    if(!doc)return fail(`No ${flag(args,'--roadmap')??'Roadmap.md'} in the configured local documents.`);
    const tickets=[...index.tickets.values()].filter(r=>r.metadata.key.startsWith(project+'-')).sort((a,b)=>Number(a.metadata.key.split('-').at(-1))-Number(b.metadata.key.split('-').at(-1)));
    const byKey=new Map(tickets.map(r=>[r.metadata.key,r.metadata]));
    const seen=new Set(),rows=[];
    if(!/^[A-Z0-9]+$/.test(project))return fail('Invalid project key');
    const regex=new RegExp(`\\[(${project}-\\d+)\\]`,'g');
    for(const line of doc.bodyText.split('\n')) {
      const keys=[...line.matchAll(regex)].map(m=>m[1]);
      for(const key of keys) {
        if(seen.has(key))continue;seen.add(key);const ticket=byKey.get(key);
        if(!ticket)rows.push({kind:'missing-issue',key,detail:`named in ${flag(args,'--roadmap')??'Roadmap.md'} but no such issue`});
        else if(keys.length===1&&!sharesSubject(line,ticket.title))rows.push({kind:'title-mismatch',key,detail:`board says "${ticket.title}"`});
      }
    }
    for(const {metadata:m}of tickets)if(['EPIC','STORY'].includes(m.type)&&!seen.has(m.key))rows.push({kind:'unlisted',key:m.key,detail:m.title});
    return result({source:doc.relativePath,keysInDoc:seen.size,issuesOnBoard:tickets.length,rows},rows.length?1:0);
  }
  if(command==='guard') {
    const raw=files.file??flag(args,'--text');
    if(typeof raw!=='string')return fail('No changed-file set was sent. The client computes this from its own working tree; pass --file <path> when driving the board CLI directly.');
    const changed=raw.split('\n').map(s=>s.trim()).filter(Boolean);
    const key=sub&&!sub.startsWith('--')?sub:null;
    let stale=null;
    if(key) {
      if(files.gates===undefined)stale=`No gate selection was recorded for ${key}. Run \`zagent gates ${key}\` before \`zagent guard\`, and run every line it prints: the gates a ticket needs follow the directories its diff touched, not the category it was filed under.`;
      else {
        let recorded;try{recorded=JSON.parse(files.gates);}catch{return result({...checkProtectedPaths(index.config.policy.resolved,changed),key,gateSelectionStale:`The gate selection recorded for ${key} is not valid JSON. Re-run \`zagent gates ${key}\`.`,docsMirrorNeeded:mirrorPaths(index,changed).length>0,docsMirrorPaths:mirrorPaths(index,changed)},1);}
        const selected=context.dispatchRead(index,['gates',key],{file:raw});
        if(selected.exitCode!==0)return selected;
        const current=selected.payload;
        const missing=current.gates.filter(g=>!(recorded.gates??[]).includes(g));
        if(missing.length) {
          const appeared=current.changed.filter(p=>!(recorded.changed??[]).includes(p));
          stale=[`The gate selection recorded for ${key} no longer describes this diff.`,appeared.length?`Changed since it was computed: ${appeared.join(', ')}`:null,'These gates were never run:',...missing.map(g=>`  ${g}`),`Re-run \`zagent gates ${key}\`, run every line it prints, then guard again.`].filter(Boolean).join('\n');
        } else {
          // Evaluated and clean: distinct from `null` below, which means the
          // fold never ran at all (no ticket key was passed to `guard`) — a
          // passing guard must be able to say "checked, not stale" rather
          // than leaving the same falsy value for "never checked".
          stale=false;
        }
      }
    }
    const protectedResult=checkProtectedPaths(index.config.policy.resolved,changed),mirror=mirrorPaths(index,changed);
    // `stale` is now three-valued: `null` (no key given — the fold never ran
    // at all), `false` (it ran and the selection is current), or a message
    // string (stale/unparsable/unrecorded). Both falsy non-string values
    // pass; only a message refuses.
    return result({...protectedResult,key,gateSelectionStale:stale,docsMirrorNeeded:mirror.length>0,docsMirrorPaths:mirror},protectedResult.ok&&!stale?0:1);
  }
  return null;
}
