import { readFileSync, writeFileSync, mkdirSync, existsSync } from 'node:fs';
import path from 'node:path';
import { createHash } from 'node:crypto';
import YAML from 'yaml';
import { validateInventory } from './validate-inventory.mjs';

export function exportVault(snapshot, output, policy) {
  const inventory = JSON.parse(readFileSync(new URL('../spec/field-inventory.json', import.meta.url),'utf8'));
  if (snapshot.columns) {
    const errors = validateInventory(snapshot.columns, inventory);
    if (errors.length) throw new Error(errors.join('\n'));
  }
  for (const entity of inventory.entities) {
    if (entity.disposition==='dropped-empty' && snapshot.tables[entity.id]?.length) {
      throw new Error(`Expected empty entity ${entity.id} has rows; review its mapping before exporting.`);
    }
  }
  const root = path.resolve(output);
  if (existsSync(root)) throw new Error('Export destination already exists; use a fresh isolated directory.');
  const t = snapshot.tables;
  const rows = name => t[name] ?? [];
  const byId = name => new Map(rows(name).map(row => [row.id, row]));
  const projects = byId('jira.projects');
  const issues = byId('jira.issues');
  const statuses = byId('jira.workflow_statuses');
  const categories = byId('jira.categories');
  const users = byId('public.users');
  const sprints = byId('jira.sprints');
  const labels = byId('jira.labels');
  const versions = byId('jira.versions');
  const keyOf = id => { const i = issues.get(id); return i ? `${projects.get(i.project_id).key}-${i.number}` : null; };
  const slugOf = s => `${projects.get(s.project_id).key.toLowerCase()}-${s.id}`;
  const markdown = (relative, frontmatter, body = '') => {
    const dest = path.resolve(root, relative);
    if (!dest.startsWith(root + path.sep)) throw new Error('Unsafe export path');
    mkdirSync(path.dirname(dest), { recursive: true });
    writeFileSync(dest, `---\n${YAML.stringify(frontmatter, { lineWidth: 0 })}---\n${body}`, { flag: 'wx' });
  };
  const usedProjects = [...projects.values()].filter(p => rows('jira.issues').some(i => i.project_id === p.id) || Object.hasOwn(policy.resolved.projects, p.key));
  for (const project of usedProjects) {
    if (!/^[A-Z0-9]+$/.test(project.key)) throw new Error(`Unsafe project key ${project.key}`);
    for (const i of rows('jira.issues').filter(i => i.project_id === project.id)) {
      const key = keyOf(i.id);
      const status = statuses.get(i.status_id);
      const source = { ...i }; if (i.description !== null) delete source.description;
      const links = rows('jira.issue_links').filter(l => l.source_issue_id === i.id);
      const descriptionBytes = i.description?.startsWith('\\x') ? Buffer.from(i.description.slice(2), 'hex') : Buffer.from(i.description ?? '', 'utf8');
      const body = descriptionBytes.toString('utf8');
      if (!Buffer.from(body,'utf8').equals(descriptionBytes)) throw new Error(`Description for ${key} is not valid UTF-8; refusing a lossy Markdown export.`);
      markdown(`${project.key}/tickets/${key}.md`, {
        key, type: i.type, title: i.title, category: categories.get(i.category_id)?.name ?? null,
        parent: keyOf(i.parent_id), sprint: sprints.has(i.sprint_id) ? slugOf(sprints.get(i.sprint_id)) : null,
        status: status?.name,
        assignee: users.get(i.assignee_id)?.email ?? null,
        priority: i.priority, complexity: i.complexity, risk: i.risk,
        storyPoints: i.story_points, position: i.position,
        labels: snapshot.labelOrder?.[i.id] ?? rows('jira.issue_labels').filter(l => l.issue_id === i.id).map(l => labels.get(l.label_id)?.name),
        fixVersions: rows('jira.issue_versions').filter(v => v.issue_id === i.id && v.kind === 'FIX').map(v => versions.get(v.version_id)?.name),
        blocks: links.filter(l => l.type === 'BLOCKS').map(l => ({ key: keyOf(l.target_issue_id), reason: l.reason })),
        links: links.filter(l => l.type !== 'BLOCKS').map(l => ({ key: keyOf(l.target_issue_id), type: l.type, reason: l.reason })),
        source,
      }, body);
    }
    for (const sprint of rows('jira.sprints').filter(s => s.project_id === project.id)) {
      markdown(`${project.key}/sprints/${slugOf(sprint)}.md`, { kind: 'sprint', slug: slugOf(sprint), name: sprint.name, status: sprint.status, continuous: sprint.continuous, source: sprint });
    }
    for (const [name, table] of [['labels','jira.labels'],['versions','jira.versions']]) markdown(`${project.key}/${name}.md`, { kind: name, rows: rows(table).filter(r => r.project_id === project.id) });
    const workflows = rows('jira.workflows').filter(w => w.project_id === project.id);
    const workflowIds = new Set(workflows.map(w => w.id));
    markdown(`${project.key}/workflow.md`, { kind: 'workflow', rows: workflows, statuses: rows('jira.workflow_statuses').filter(s => workflowIds.has(s.workflow_id)), transitions: rows('jira.workflow_transitions').filter(s => workflowIds.has(s.workflow_id)) });
    const issueIds = new Set(rows('jira.issues').filter(i => i.project_id === project.id).map(i => i.id));
    markdown(`${project.key}/history/pre-migration.md`, { kind: 'history', rows: rows('jira.issue_activity').filter(r => issueIds.has(r.issue_id)) }, '# Frozen pre-migration activity\n\nThese are original activity records from the captured database snapshot. No post-cutover history exists in this read-only prototype.\n');
  }
  const excluded = new Set(['jira.issues','jira.sprints','jira.labels','jira.versions','jira.workflows','jira.workflow_statuses','jira.workflow_transitions','jira.issue_activity']);
  // Relational support records retained losslessly, with no authentication material.
  const exportedProjectIds = new Set(usedProjects.map(p => p.id));
  const exportedWorkflowIds = new Set(rows('jira.workflows').filter(w => exportedProjectIds.has(w.project_id)).map(w => w.id));
  const archivedTables = {};
  for (const name of excluded) {
    const remainder = rows(name).filter(r => r.project_id ? !exportedProjectIds.has(r.project_id) : r.workflow_id ? !exportedWorkflowIds.has(r.workflow_id) : false);
    if (remainder.length) archivedTables[name] = remainder;
  }
  markdown('metadata.md', { kind: 'metadata', tables: Object.fromEntries(Object.entries(t).filter(([name]) => !excluded.has(name))), archivedTables, ordering: {links:snapshot.linkOrder??{},history:snapshot.historyOrder??{}} });
  writeFileSync(path.join(root, 'vault.config.json'), JSON.stringify({ formatVersion: 1, boardRoots: usedProjects.map(p => ({ project: p.key, path: p.key })), supportingFiles: ['metadata.md'], policy, snapshotHash: createHash('sha256').update(JSON.stringify(snapshot)).digest('hex') }, null, 2) + '\n');
  writeFileSync(path.join(root, 'manifest.json'), JSON.stringify({ formatVersion: 1, counts: Object.fromEntries(Object.entries(t).map(([n,r]) => [n,r.length])), issueCount: issues.size, projectKeys: usedProjects.map(p => p.key) }, null, 2) + '\n');
  return { root, issues: issues.size, projects: usedProjects.length };
}
if (process.argv[1] && path.resolve(process.argv[1]) === path.resolve(new URL(import.meta.url).pathname.replace(/^\/(\w:)/, '$1'))) {
  const [source, output, config] = process.argv.slice(2);
  console.log(exportVault(JSON.parse(readFileSync(source,'utf8')), output, JSON.parse(readFileSync(config,'utf8'))));
}
