import { allTicketRecords, blockedBy, canonicalOf, policyOf, sourceOf, ticketFor } from './compat.mjs';

const ok=payload=>({exitCode:0,payload});
const fail=(error,exitCode=1)=>({exitCode,payload:{error}});
const number=t=>Number(canonicalOf(t).key.split('-').at(-1));
const aggregate=children=>({
  total:children.length,
  done:children.filter(c=>c.statusCategory==='DONE').length,
  points:children.reduce((n,c)=>n+(c.storyPoints??0),0),
  donePoints:children.filter(c=>c.statusCategory==='DONE').reduce((n,c)=>n+(c.storyPoints??0),0),
});

export function dispatchReports(index,args) {
  if (!['epic','report'].includes(args[0])) return null;
  const records=allTicketRecords(index);
  const childrenOf=key=>records.filter(t=>canonicalOf(t).parent===key).sort((a,b)=>number(a)-number(b));
  if (args[0]==='epic') {
    if(!args[1])return fail('Usage: zagent epic <TICKET_KEY>');
    if(!/^[A-Za-z][A-Za-z0-9]*-\d+$/.test(args[1]))return fail(`"${args[1]}" is not a ticket key (expected something like ZM-14).`);
    const project=args[1].split('-')[0];
    if(!(index.tables.get('jira.projects')??[]).some(p=>p.key===project))return fail(`No Jira project with key ${project}.`);
    const epic=ticketFor(index,args[1]);
    if(!epic)return fail(`No ticket ${args[1]}.`,3);
    const m=canonicalOf(epic);
    if(m.type!=='EPIC')return fail(`${m.key} is a ${m.type}, not an EPIC.`);
    const children=childrenOf(m.key).map(t=>{
      const c=canonicalOf(t);
      return {key:c.key,title:c.title,type:c.type,status:c.status,statusCategory:c.statusCategory,storyPoints:c.storyPoints??null,
        blockedBy:blockedBy(index,c.key).filter(b=>canonicalOf(b.source).statusCategory!=='DONE').map(b=>canonicalOf(b.source).key)};
    });
    return ok({key:m.key,title:m.title,status:m.status,...aggregate(children),children});
  }
  const projectAt=args.indexOf('--project');
  const project=projectAt<0?policyOf(index).clientFile.project:args[projectAt+1];
  if(!project)return fail('Pass --project <KEY>, or run inside a repo with zagent.project.json.');
  const projectRow=(index.tables.get('jira.projects')??[]).find(p=>p.key===project);
  if(!projectRow)return fail(`No Jira project with key ${project}.`);
  const sub=args[1]?.startsWith('--')? 'epic':args[1]??'epic';
  if(sub==='epic')return ok(records.filter(t=>canonicalOf(t).key.startsWith(project+'-')&&canonicalOf(t).type==='EPIC').sort((a,b)=>number(a)-number(b)).map(t=>{
    const m=canonicalOf(t),a=aggregate(childrenOf(m.key).map(canonicalOf));
    return {key:m.key,title:m.title,...a,percent:a.points>0?Math.round(a.donePoints/a.points*100):a.total>0?Math.round(a.done/a.total*100):0};
  }));
  if(sub==='velocity') {
    const completedAt=s=>Object.hasOwn(canonicalOf(s),'completedAt')?canonicalOf(s).completedAt:sourceOf(s).completed_at;
    const sprints=[...index.sprints.values()].filter(s=>(s.project===project||sourceOf(s).project_id===projectRow.id)&&canonicalOf(s).status==='COMPLETED').sort((a,b)=>{
      const left=completedAt(a),right=completedAt(b);
      return left===right?0:left==null?1:right==null?-1:String(left).localeCompare(String(right));
    });
    const rows=sprints.map(s=>{
      const m=canonicalOf(s);
      const members=records.filter(t=>canonicalOf(t).sprint===m.slug&&canonicalOf(t).type!=='EPIC').map(canonicalOf);
      const a=aggregate(members);
      return {sprint:m.name,committed:a.points,completed:a.donePoints};
    });
    return ok({rows,average:rows.length?Math.round(rows.reduce((n,r)=>n+r.completed,0)/rows.length*10)/10:0});
  }
  return fail('Usage: zagent report epic|velocity [--project KEY]');
}
