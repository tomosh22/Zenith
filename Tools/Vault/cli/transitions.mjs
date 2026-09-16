import { canonicalOf, sourceOf, ticketFor } from './compat.mjs';

/** The retained workflow graph; the empty legacy rules engine is intentionally dropped. */
export function dispatchTransitions(index, args) {
  if (args[0] !== 'transitions') return null;
  const requested=args[1];
  const fail=(error,exitCode=1)=>({exitCode,payload:{error}});
  if (!requested) return fail('Usage: zagent transitions <TICKET_KEY>');
  if (!/^[A-Za-z][A-Za-z0-9]*-\d+$/.test(requested)) return fail(`"${requested}" is not a ticket key (expected something like ZM-14).`);
  const project=requested.split('-')[0];
  if (!(index.tables.get('jira.projects')??[]).some(p=>p.key===project)) return fail(`No Jira project with key ${project}.`);
  const ticket=ticketFor(index,requested);
  if (!ticket) return fail(`No ticket ${requested}.`,3);
  const metadata=canonicalOf(ticket), source=sourceOf(ticket);
  const workflow=[...index.files.values()].find(record=>{
    const fields=record.fields??record.metadata;
    return fields?.kind==='workflow' && (record.project===project || fields.rows?.some(row=>row.project_id===source.project_id));
  });
  const fields=workflow?.fields??workflow?.metadata??{};
  const statuses=new Map((fields.statuses??[]).map(status=>[status.id,status]));
  const current=[...statuses.values()].find(status=>status.name===metadata.status);
  if (!current) return fail(`No workflow status ${metadata.status} for ${metadata.key}.`,4);
  const rules=index.tables?.get('jira.workflow_transition_rules')??[];
  const grants=index.tables?.get('jira.permission_grants')??[];
  if (rules.length || grants.length) return fail('Workflow rules and permission grants are outside the read-only vault graph specification.',4);
  const definitions=[...(fields.transitions??[])].sort((a,b)=>a.position-b.position||a.name.localeCompare(b.name));
  const applicable=definitions.filter(t=>(t.from_status_id===current.id||t.from_status_id===null)&&t.to_status_id!==current.id);
  const seen=new Set(), transitions=[];
  for (const candidate of applicable) {
    if (seen.has(candidate.to_status_id)) continue;
    seen.add(candidate.to_status_id);
    const chosen=applicable.find(t=>t.to_status_id===candidate.to_status_id&&t.from_status_id===current.id)??candidate;
    transitions.push({toStatus:statuses.get(chosen.to_status_id)?.name??'(unknown)',name:chosen.name,blockedReason:null});
  }
  return {exitCode:0,payload:{key:metadata.key,status:metadata.status,transitions}};
}
