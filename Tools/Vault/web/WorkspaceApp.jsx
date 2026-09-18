import React, { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { App } from './App.jsx';
import { createProjectApi } from './project-api.js';
import { IssuePanel, CreateIssueForm } from './issues/index.jsx';
import { PlanningWorkspace } from './planning/index.jsx';
import { CreateProjectForm } from './CreateProjectForm.jsx';
import { SettingsWorkspace } from './settings/index.jsx';
import './project.css';

const sections = ['board', 'backlog', 'roadmap', 'releases', 'reports', 'workflow', 'fields', 'board-settings'];
const sectionLabels = { 'board-settings': 'Board settings' };
function locationState() {
  const query = new URLSearchParams(window.location.search);
  return { project: query.get('project') || '', section: [...sections, 'projects'].includes(query.get('section')) ? query.get('section') : 'board', issue: query.get('issue') || '', documents: query.get('workspace') === 'documents', documentPath: query.get('path') || '' };
}
function initialTheme() {
  try { const saved = localStorage.getItem('vault.theme'); if (saved === 'dark' || saved === 'light') return saved; } catch { /* Storage may be disabled. */ }
  return window.matchMedia?.('(prefers-color-scheme: dark)').matches ? 'dark' : 'light';
}

export function WorkspaceApp() {
  const [route, setRoute] = useState(locationState);
  const [session, setSession] = useState(null);
  const [projects, setProjects] = useState([]);
  const [project, setProject] = useState(null);
  const [error, setError] = useState('');
  const [creating, setCreating] = useState(false);
  const [creatingProject, setCreatingProject] = useState(false);
  const [catalogRevisions, setCatalogRevisions] = useState({});
  const [query, setQuery] = useState('');
  const [search, setSearch] = useState({ results: [], error: '' });
  const [refresh, setRefresh] = useState(0);
  const [live, setLive] = useState('Connecting…');
  const [theme, setTheme] = useState(initialTheme);
  useEffect(() => { try { localStorage.setItem('vault.theme', theme); } catch { /* Theme remains usable without storage. */ } }, [theme]);
  const request = useRef(0);
  const dialog = useRef(null);
  const searchInput = useRef(null);
  const api = useMemo(() => createProjectApi(session?.csrfToken), [session?.csrfToken]);
  const changed = useCallback(() => setRefresh(value => value + 1), []);
  const navigate = useCallback((patch, { issueModal = false } = {}) => {
    const next = { ...locationState(), ...patch };
    const query = new URLSearchParams();
    if (next.documents) query.set('workspace', 'documents');
    if (next.documents && next.documentPath) query.set('path', next.documentPath);
    if (next.project) query.set('project', next.project);
    if (next.section !== 'board') query.set('section', next.section);
    if (next.issue) query.set('issue', next.issue);
    window.history.pushState({ issueModal }, '', `${window.location.pathname}${query.size ? `?${query}` : ''}`);
    setRoute({ ...next, issueModal }); setCreating(false); setCreatingProject(false);
  }, []);
  useEffect(() => {
    const shortcut = event => {
      if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 'k') { event.preventDefault(); searchInput.current?.focus(); }
      if (event.key === 'Escape' && document.activeElement === searchInput.current) setQuery('');
    };
    window.addEventListener('keydown', shortcut);
    return () => window.removeEventListener('keydown', shortcut);
  }, []);
  useEffect(() => {
    const pop = () => { setRoute({ ...locationState(), issueModal: Boolean(window.history.state?.issueModal) }); setCreating(false); setCreatingProject(false); };
    window.addEventListener('popstate', pop);
    return () => window.removeEventListener('popstate', pop);
  }, []);
  useEffect(() => {
    if (session) return;
    let active = true;
    api('/api/session').then(value => { if (active) setSession(value); }).catch(value => { if (active) setError(value.message); });
    return () => { active = false; };
  }, [api, session, refresh]);
  useEffect(() => {
    if (!session || route.documents) return;
    let active = true;
    api('/api/projects').then(value => { if (active) { setProjects(value.projects); setCatalogRevisions(value.revisions || {}); } }).catch(value => { if (active) setError(value.message); });
    return () => { active = false; };
  }, [api, session, refresh, route.documents]);
  const selected = route.project || projects.find(item => !item.readOnly)?.key || projects[0]?.key;
  useEffect(() => {
    const section = sectionLabels[route.section] || route.section[0].toUpperCase() + route.section.slice(1);
    document.title = [route.documents ? 'Documents' : route.issue || (route.section === 'projects' ? 'Projects' : section), route.documents || route.section === 'projects' ? null : selected, 'Zenith Vault'].filter(Boolean).join(' · ');
  }, [route.documents, route.issue, route.section, selected]);
  const openIssue = issue => {
    const key = typeof issue === 'string' ? issue : issue?.key;
    if (!key) return;
    const owner = key.replace(/-\d+$/, '');
    navigate({ project: projects.some(project => project.key === owner) ? owner : selected, issue: key }, { issueModal: true });
  };
  useEffect(() => {
    let active = true;
    if (!query.trim()) { setSearch({ results: [], error: '' }); return; }
    const timer = setTimeout(() => {
      api(`/api/search?q=${encodeURIComponent(query.trim())}`).then(value => {
        if (active) setSearch({ results: value.results.filter(item => !['config', 'metadata', 'history', 'sprint'].includes(item.kind)), error: '' });
      }).catch(value => { if (active) setSearch({ results: [], error: value.message }); });
    }, 180);
    return () => { active = false; clearTimeout(timer); };
  }, [api, query, refresh]);
  useEffect(() => {
    if (!session || !selected || route.documents) return;
    const version = ++request.current;
    setError('');
    api(`/api/projects/${encodeURIComponent(selected)}`).then(value => {
      if (version === request.current) setProject(value);
    }).catch(value => { if (version === request.current) { setProject(null); setError(value.message); } });
    return () => { request.current++; };
  }, [api, session, selected, refresh, route.documents]);
  useEffect(() => {
    if (!session || typeof WebSocket === 'undefined') return;
    let stopped = false, socket, timer;
    const connect = () => {
      const currentSocket = new WebSocket(`${window.location.protocol === 'https:' ? 'wss:' : 'ws:'}//${window.location.host}/api/events`);
      socket = currentSocket;
      socket.onopen = () => { if (!stopped) { setLive('Live'); changed(); } };
      socket.onmessage = event => {
        if (stopped) return;
        try {
          const message = JSON.parse(event.data);
          if (message.type === 'error') { setError(message.message || 'Live updates failed'); return; }
          if (message.type === 'ready' || message.type === 'pong') return;
        } catch { /* An unrecognized notification still warrants a refresh. */ }
        changed();
      };
      socket.onclose = () => { if (!stopped) { setLive('Reconnecting…'); timer = setTimeout(connect, 1000); } };
      socket.onerror = () => currentSocket.close();
    };
    connect();
    const visible = () => { if (document.visibilityState === 'visible') changed(); };
    document.addEventListener('visibilitychange', visible);
    return () => { stopped = true; clearTimeout(timer); socket?.close(); document.removeEventListener('visibilitychange', visible); };
  }, [session, changed]);
  useEffect(() => {
    if (!creating && !creatingProject && !(route.issue && route.issueModal)) return;
    const previous = document.activeElement;
    const container = dialog.current;
    if (!container) return;
    const previousOverflow = document.body.style.overflow;
    document.body.style.overflow = 'hidden';
    container.focus();
    const trap = event => {
      if (event.key === 'Escape' && route.issueModal && !creating && !creatingProject) { event.preventDefault(); navigate({ issue: '' }); return; }
      if (event.key !== 'Tab') return;
      const elements = [...container.querySelectorAll('button:not(:disabled), input:not(:disabled), select:not(:disabled), textarea:not(:disabled), a[href], [tabindex="0"]')];
      if (!elements.length) { event.preventDefault(); return; }
      if (event.shiftKey && (document.activeElement === elements[0] || document.activeElement === container)) { event.preventDefault(); elements.at(-1).focus(); }
      else if (!event.shiftKey && document.activeElement === elements.at(-1)) { event.preventDefault(); elements[0].focus(); }
    };
    container.addEventListener('keydown', trap);
    return () => { container.removeEventListener('keydown', trap); document.body.style.overflow = previousOverflow; previous?.focus?.(); };
  }, [creating, creatingProject, route.issue, route.issueModal, project?.key, navigate]);
  if (route.documents) return <><div className="workspace-switch"><button onClick={() => navigate({ documents: false })}>Projects</button></div><App initialPath={route.documentPath || null} /></>;
  const current = project?.key === selected ? project : null;
  const readOnly = Boolean(session?.readOnly || current?.readOnly);
  return <div className="project-app" data-theme={theme}>
    <header className="project-header"><a href="/" className="project-brand">Zenith Vault</a>
      <button onClick={() => navigate({ section: 'projects', issue: '' })}>Projects</button>
      <label>Project <select aria-label="Project" value={selected || ''} onChange={event => navigate({ project: event.target.value, issue: '', section: route.section === 'projects' ? 'board' : route.section })}>
        {projects.map(item => <option key={item.key} value={item.key}>{item.name || item.key} ({item.key})</option>)}
      </select></label>
      <div className="project-search"><input ref={searchInput} type="search" aria-label="Search issues and documents" placeholder="Search issues and documents…" value={query} onChange={event => setQuery(event.target.value)} />
        {query.trim() && <div className="project-search-results" aria-label="Workspace search results">{search.error ? <p role="alert">{search.error}</p> : search.results.length ? search.results.map(item => <button key={item.path} onClick={() => { setQuery(''); const key = item.key || item.metadata?.key; if (key && /^[A-Z][A-Z0-9_]*-\d+$/i.test(key)) openIssue(key); else navigate({ documents: true, documentPath: item.path, issue: '' }); }}>{item.key || item.metadata?.key || 'Document'} · {item.title}</button>) : <p>No matching results</p>}</div>}
      </div>
      <button onClick={() => navigate({ documents: true, issue: '' })}>Documents</button>
      <button aria-label={`Switch to ${theme === 'dark' ? 'light' : 'dark'} theme`} onClick={() => setTheme(value => value === 'dark' ? 'light' : 'dark')}>{theme === 'dark' ? 'Light' : 'Dark'} theme</button>
      <button disabled={!session || session.readOnly} onClick={() => setCreatingProject(true)}>New project</button>
      <span className="project-live" aria-live="polite">{live}</span>
      {readOnly && <span role="status">Read only</span>}
      <button disabled={!current || readOnly} onClick={() => setCreating(true)}>Create issue</button>
    </header>
    <nav className="project-navigation" aria-label="Project navigation">{sections.map(section => <button key={section} aria-current={route.section === section ? 'page' : undefined} onClick={() => navigate({ section, issue: '' })}>{sectionLabels[section] || section[0].toUpperCase() + section.slice(1)}</button>)}</nav>
    {error && <div role="alert" className="project-error">{error}<button onClick={changed}>Retry</button></div>}
    {!session ? <p role="status">Loading workspace…</p> : !projects.length && !error ? <p>No projects are available.</p> : !current && !error ? <p role="status">Loading project…</p> : null}
    {current && route.issue && !route.issueModal ? <main className="project-main project-full-issue"><IssuePanel key={route.issue} issueKey={route.issue} project={current} api={api} onChanged={changed} onOpenIssue={openIssue} onClose={() => navigate({ issue: '' })} readOnly={readOnly} /></main> : route.section === 'projects' ? <main className="project-main"><h1>Projects</h1><div className="project-grid">{projects.map(item => <button key={item.key} onClick={() => navigate({ project: item.key, section: 'board', issue: '' })}><strong>{item.name || item.key}</strong><span>{item.key}{item.readOnly ? ' · Read only' : ''}</span>{item.description && <p>{item.description}</p>}</button>)}</div></main> : current && <main className="project-main"><h1>{current.name || current.key}</h1>{['workflow', 'fields', 'board-settings'].includes(route.section) ? <SettingsWorkspace key={`${selected}:${route.section}`} section={route.section} project={current} api={api} onChanged={changed} readOnly={readOnly} /> : <PlanningWorkspace key={`${selected}:${route.section}`} section={route.section} project={current} api={api} onChanged={changed} onOpenIssue={openIssue} onCreateIssue={() => setCreating(true)} readOnly={readOnly} />}</main>}
    {creatingProject && <div className="project-overlay"><section ref={dialog} tabIndex={-1} className="project-dialog" role="dialog" aria-modal="true" aria-label="Create project"><CreateProjectForm api={api} revisions={catalogRevisions} onCancel={() => setCreatingProject(false)} onCreated={key => { setCreatingProject(false); changed(); navigate({ project: key, issue: '', section: 'board' }); }} /></section></div>}
    {current && (creating || route.issue && route.issueModal) && <div className="project-overlay"><section ref={dialog} tabIndex={-1} className="project-dialog" role="dialog" aria-modal="true" aria-label={creating ? 'Create issue' : `Issue ${route.issue}`}>
      {creating ? <CreateIssueForm project={current} api={api} onCreated={issue => { changed(); setCreating(false); openIssue(issue); }} onCancel={() => setCreating(false)} readOnly={readOnly} /> : <IssuePanel key={route.issue} issueKey={route.issue} project={current} api={api} onChanged={changed} onOpenIssue={openIssue} onClose={() => navigate({ issue: '' })} readOnly={readOnly} />}
    </section></div>}
  </div>;
}

