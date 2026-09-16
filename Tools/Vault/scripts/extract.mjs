import postgres from 'postgres';
import { mkdirSync, writeFileSync, readFileSync } from 'node:fs';
import path from 'node:path';

const output = process.argv[2];
const capturedOrdering=process.argv[3]?JSON.parse(readFileSync(process.argv[3],'utf8')):null;
const url = new URL(process.env.DATABASE_URL ?? '');
if (!output || url.hostname !== '127.0.0.1' || url.port !== '55439' || url.pathname !== '/vault_snapshot' || url.username !== 'vault_reader') {
  throw new Error('extract requires OUTPUT and the dedicated read-only localhost snapshot DATABASE_URL.');
}
const sql = postgres(url.href, { max: 1 });
try {
  const data = await sql.begin('isolation level repeatable read read only', async tx => {
    const columns = await tx`SELECT table_schema, table_name, column_name, data_type, ordinal_position FROM information_schema.columns WHERE table_schema IN ('jira','notion','public') ORDER BY table_schema,table_name,ordinal_position`;
    const names = [...new Set(columns.filter(c => c.table_schema === 'jira' || (c.table_schema === 'notion' && ['projects','databases','database_properties','database_rows'].includes(c.table_name))).map(c => `${c.table_schema}.${c.table_name}`))];
    const tables = {};
    for (const name of names) {
      // to_jsonb represents bytea as hex so descriptions survive extraction exactly.
      const rows = await tx.unsafe(`SELECT to_jsonb(t) AS row FROM ${name.split('.').map(s => '"' + s + '"').join('.')} t`);
      tables[name] = rows.map(r => r.row).sort((a,b) => JSON.stringify(a).localeCompare(JSON.stringify(b)));
    }
    for (const [name, fields] of Object.entries({ users: 'id, name, email', organizations: 'id, name, slug', memberships: 'id, user_id, organization_id, role' })) {
      const rows = await tx.unsafe(`SELECT ${fields} FROM public.${name}`);
      tables[`public.${name}`] = rows;
    }
    const labelOrder = {}, linkOrder = {}, historyOrder = {};
    for (const issue of tables['jira.issues']) {
      const names = await tx`SELECT l.name FROM jira.issue_labels il JOIN jira.labels l ON l.id=il.label_id WHERE il.issue_id=${issue.id}`;
      labelOrder[issue.id] = names.map(r=>r.name);
      // Preserve the old UNION query's observable order without depending on
      // reference payloads or on PostgreSQL's physical order at runtime.
      const links=await tx`
        SELECT l.id, l.type::text AS "type", 'outgoing' AS "direction",
               p.key || '-' || o.number AS "key", o.title, ws.name AS "status",
               ws.category::text AS "statusCategory", l.reason AS "reason"
        FROM jira.issue_links l
        JOIN jira.issues o ON o.id=l.target_issue_id
        JOIN jira.projects p ON p.id=o.project_id
        JOIN jira.workflow_statuses ws ON ws.id=o.status_id
        WHERE l.source_issue_id=${issue.id}
        UNION ALL
        SELECT l.id, l.type::text, 'incoming',
               p.key || '-' || o.number, o.title, ws.name, ws.category::text, l.reason
        FROM jira.issue_links l
        JOIN jira.issues o ON o.id=l.source_issue_id
        JOIN jira.projects p ON p.id=o.project_id
        JOIN jira.workflow_statuses ws ON ws.id=o.status_id
        WHERE l.target_issue_id=${issue.id}`;
      linkOrder[issue.id]=links.map(r=>r.id);
      if(capturedOrdering) {
        const project=tables['jira.projects'].find(p=>p.id===issue.project_id);
        const key=`${project.key}-${issue.number}`;
        const captured=capturedOrdering.links[key];
        if(!Array.isArray(captured)||JSON.stringify([...captured].sort())!==JSON.stringify([...linkOrder[issue.id]].sort())) throw new Error(`Captured link ordering does not match snapshot identities for ${key}`);
        linkOrder[issue.id]=captured;
        const events=capturedOrdering.history?.[key]??[];
        const known=new Set(tables['jira.issue_activity'].filter(a=>a.issue_id===issue.id).map(a=>a.id));
        if(events.some(id=>!known.has(id)))throw new Error(`Captured history ordering does not match snapshot identities for ${key}`);
        historyOrder[issue.id]=events;
      }
    }
    return { formatVersion: 1, columns: columns.filter(c => tables[`${c.table_schema}.${c.table_name}`]), tables, labelOrder, linkOrder, historyOrder };
  });
  mkdirSync(path.dirname(path.resolve(output)), { recursive: true });
  writeFileSync(output, JSON.stringify(data, null, 2) + '\n', { flag: 'wx' });
  console.log(JSON.stringify(Object.fromEntries(Object.entries(data.tables).filter(([,r]) => r.length).map(([k,r]) => [k,r.length])), null, 2));
} finally { await sql.end(); }
