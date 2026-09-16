import React, { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { marked } from 'marked';
import DOMPurify from 'dompurify';
import { autocompletion, completionKeymap } from '@codemirror/autocomplete';
import { defaultKeymap, history, historyKeymap, indentWithTab } from '@codemirror/commands';
import { markdown } from '@codemirror/lang-markdown';
import { EditorState } from '@codemirror/state';
import { EditorView, keymap, lineNumbers } from '@codemirror/view';

marked.setOptions({ gfm: true, breaks: true });

export function sanitizeMarkdown(source) {
  const text = String(source ?? '').replace(/^(?:\uFEFF)?---[ \t]*\r?\n[\s\S]*?^(?:---|\.\.\.)[ \t]*(?:\r?\n|$)/m, '');
  return DOMPurify.sanitize(marked.parse(text), {
    USE_PROFILES: { html: true },
    FORBID_TAGS: ['style', 'script', 'iframe', 'object', 'embed'],
    FORBID_ATTR: ['style', 'onerror', 'onclick', 'onload'],
  });
}

function markdownWithWikiLinks(source) {
  return String(source ?? '').replace(/\[\[([^\]\n]+)\]\]/g, (_match, target) => {
    const raw = String(target).trim();
    const separator = raw.indexOf('|');
    const linkTarget = (separator < 0 ? raw : raw.slice(0, separator)).trim();
    const label = (separator < 0 ? raw : raw.slice(separator + 1)).trim() || linkTarget;
    return `[${label}](#wiki-${encodeURIComponent(linkTarget)})`;
  });
}

function bytesBase64(text) {
  const bytes = new TextEncoder().encode(text);
  let binary = '';
  for (let offset = 0; offset < bytes.length; offset += 0x8000) {
    binary += String.fromCharCode(...bytes.subarray(offset, offset + 0x8000));
  }
  return btoa(binary);
}

function displayName(file) {
  const metadata = file?.metadata ?? {};
  return metadata.key || metadata.title || file?.path?.split('/').at(-1) || file?.path;
}

export function ticketStatusOptions(file, currentStatus, statusesByProject) {
  const current = String(currentStatus ?? '').trim();
  const configured = file?.project && statusesByProject && typeof statusesByProject === 'object'
    ? statusesByProject[file.project]
    : undefined;
  const options = Array.isArray(configured)
    ? [...new Set(configured.map(value => String(value ?? '').trim()).filter(Boolean))]
    : [];
  return options.length ? options : (current ? [current] : []);
}

function groupFiles(files) {
  const groups = new Map();
  for (const file of files) {
    const project = file.project || 'Documents';
    if (!groups.has(project)) groups.set(project, []);
    groups.get(project).push(file);
  }
  return [...groups.entries()].sort(([a], [b]) => a.localeCompare(b));
}

function Editor({ value, onChange, completions }) {
  const host = useRef(null);
  const view = useRef(null);
  const callback = useRef(onChange);
  callback.current = onChange;
  const completionItems = useMemo(() => completions.map(label => ({ label, type: 'keyword', detail: 'wiki link' })), [completions]);

  useEffect(() => {
    if (!host.current) return undefined;
    const source = context => {
      const before = context.matchBefore(/[\w-]*$/);
      if (!before || (before.from === before.to && !context.explicit)) return null;
      return { from: before.from, options: completionItems, validFor: /^[\w-]*$/ };
    };
    const state = EditorState.create({
      doc: value,
      extensions: [
        EditorState.lineSeparator.of(value.includes('\r\n') ? '\r\n' : value.includes('\r') ? '\r' : '\n'),
        lineNumbers(),
        markdown(),
        history(),
        keymap.of([...defaultKeymap, ...historyKeymap, ...completionKeymap, indentWithTab]),
        autocompletion({ override: [source], activateOnTyping: true }),
        EditorView.lineWrapping,
        EditorView.updateListener.of(update => {
          if (update.docChanged) callback.current(update.state.sliceDoc());
        }),
        EditorView.theme({
          '&': { height: '100%', fontSize: '14px' },
          '.cm-scroller': { overflow: 'auto', fontFamily: 'ui-monospace, SFMono-Regular, Consolas, monospace' },
          '.cm-gutters': { backgroundColor: '#f8fafc', border: 0, color: '#94a3b8' },
        }),
      ],
    });
    view.current = new EditorView({ state, parent: host.current });
    return () => { view.current?.destroy(); view.current = null; };
    // The editor is recreated only when a document is selected.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  useEffect(() => {
    if (!view.current) return;
    const current = view.current.state.doc.toString();
    if (current !== value) view.current.dispatch({ changes: { from: 0, to: current.length, insert: value } });
  }, [value]);

  return <div className="editor-host" ref={host} data-testid="editor" />;
}

function FileTree({ files, selected, onSelect, query }) {
  const lowered = query.trim().toLocaleLowerCase();
  const filtered = files.filter(file => /\.(?:md|markdown)$/i.test(file.path)
    && (!lowered || `${file.path} ${file.title} ${file.metadata?.key ?? ''}`.toLocaleLowerCase().includes(lowered)));
  return <div className="file-tree" data-testid="file-tree">
    {groupFiles(filtered).map(([project, projectFiles]) => <section key={project} className="tree-group">
      <h3>{project}<span>{projectFiles.length}</span></h3>
      {projectFiles.map(file => <button
        key={file.path}
        className={`tree-file ${selected === file.path ? 'selected' : ''}`}
        onClick={() => onSelect(file.path)}
        title={file.path}
      >
        <span className={`kind-dot kind-${file.kind}`} />
        <span className="tree-label">{displayName(file)}</span>
        {file.status && <span className="tree-status">{file.status}</span>}
      </button>)}
    </section>)}
    {filtered.length === 0 && <p className="empty">No matching files.</p>}
  </div>;
}

function Board({ tickets, statuses, onOpen, onStatus }) {
  const columns = statuses.length ? statuses : ['To Do', 'In Progress', 'Done'];
  const byStatus = status => tickets.filter(ticket => ticket.status === status);
  return <div className="board" data-testid="board">
    {columns.map(status => <section
      className="board-column"
      key={status}
      onDragOver={event => event.preventDefault()}
      onDrop={event => { event.preventDefault(); const path = event.dataTransfer.getData('text/plain'); if (path) onStatus(path, status); }}
    >
      <header><h3>{status}</h3><span>{byStatus(status).length}</span></header>
      <div className="board-cards">
        {byStatus(status).map(ticket => <article
          className="ticket-card"
          key={ticket.path}
          draggable
          onDragStart={event => event.dataTransfer.setData('text/plain', ticket.path)}
          onClick={() => onOpen(ticket.path)}
          data-testid="ticket-card"
        >
          <strong>{ticket.metadata?.key || ticket.path}</strong>
          <p>{ticket.title || ticket.metadata?.title || 'Untitled ticket'}</p>
          <small>{ticket.project || 'Vault'} · {ticket.metadata?.priority || 'no priority'}</small>
        </article>)}
      </div>
    </section>)}
  </div>;
}

function Backlog({ tickets, onOpen }) {
  const priority = { CRITICAL: 0, HIGH: 1, MEDIUM: 2, LOW: 3, TRIVIAL: 4 };
  const backlog = tickets.filter(ticket => ticket.status !== 'Done').sort((left, right) => {
    const byPriority = (priority[left.metadata?.priority] ?? 9) - (priority[right.metadata?.priority] ?? 9);
    return byPriority || Number(left.metadata?.position ?? 9999) - Number(right.metadata?.position ?? 9999) || left.path.localeCompare(right.path);
  });
  return <div className="backlog" data-testid="backlog"><header><h2>Backlog</h2><span>{backlog.length} open tickets</span></header><div className="backlog-list">
    {backlog.map(ticket => <button key={ticket.path} onClick={() => onOpen(ticket.path)} className="backlog-row"><span className="priority">{ticket.metadata?.priority || 'UNSET'}</span><strong>{ticket.metadata?.key || ticket.path}</strong><span className="backlog-title">{ticket.title || 'Untitled ticket'}</span><span className="backlog-sprint">{ticket.metadata?.sprint || 'No sprint'}</span><span className="backlog-status">{ticket.status || 'No status'}</span></button>)}
    {!backlog.length && <p className="empty">No open tickets.</p>}
  </div></div>;
}

function Roadmap({ tickets, onOpen }) {
  const epics = tickets.filter(ticket => String(ticket.metadata?.type ?? '').toUpperCase() === 'EPIC' || !ticket.metadata?.parent);
  const children = new Map();
  for (const ticket of tickets) {
    if (!ticket.metadata?.parent) continue;
    if (!children.has(ticket.metadata.parent)) children.set(ticket.metadata.parent, []);
    children.get(ticket.metadata.parent).push(ticket);
  }
  return <div className="roadmap" data-testid="roadmap"><header><h2>Roadmap</h2><span>Progress from ticket frontmatter</span></header><div className="roadmap-list">
    {epics.map(epic => { const items = children.get(epic.metadata?.key) ?? []; const done = items.filter(item => item.status === 'Done').length; const progress = items.length ? Math.round(done / items.length * 100) : (epic.status === 'Done' ? 100 : 0); return <article className="roadmap-card" key={epic.path}><div className="roadmap-card-head"><button onClick={() => onOpen(epic.path)}><strong>{epic.metadata?.key || epic.title}</strong><span>{epic.title || 'Untitled epic'}</span></button><b>{progress}%</b></div><div className="progress"><i style={{ width: `${progress}%` }} /></div><small>{done} of {items.length} child tickets done</small></article>; })}
    {!epics.length && <p className="empty">No epics or parentless roadmap items.</p>}
  </div></div>;
}

function DocumentHeader({ file, title = 'Open a document', mode, setMode, dirty, conflict, onSave, onReload, readOnly, saving, status, setStatus, statuses }) {
  if (!file) return <header className="document-header"><div className="document-title"><strong>{title}</strong><span>Zenith Vault read model</span></div></header>;
  const isTicket = file.kind === 'ticket';
  return <header className="document-header">
    <div className="document-title"><strong>{file.title || file.path}</strong><span>{file.path}</span></div>
    <div className="document-actions">
      {isTicket && <select aria-label="Ticket status" value={status ?? ''} onChange={event => setStatus(event.target.value)} disabled={readOnly || saving}>
        <option value="">Status</option>{statuses.map(value => <option key={value} value={value}>{value}</option>)}
      </select>}
      <button className={mode === 'preview' ? '' : 'active'} onClick={() => setMode('edit')}>Edit</button>
      <button className={mode === 'preview' ? 'active' : ''} onClick={() => setMode('preview')}>Preview</button>
      <button className="primary" onClick={onSave} disabled={readOnly || !dirty || saving}>{saving ? 'Saving…' : 'Save'}</button>
    </div>
    {conflict && <div className="conflict" role="alert"><span>File changed on disk. Your text is preserved.</span><button onClick={onReload}>Reload disk</button></div>}
  </header>;
}

function Backlinks({ file, files, onOpen }) {
  if (!file) return null;
  const identifiers = [...new Set([...(file.backlinks ?? []), ...(file.incoming ?? [])])];
  const sources = identifiers.map(identifier => files.find(item => item.path === identifier || item.metadata?.key === identifier)).filter(Boolean);
  return <section className="backlinks" data-testid="backlinks">
    <h3>Backlinks <span>{sources.length}</span></h3>
    {sources.length ? sources.map(source => <button key={source.path} onClick={() => onOpen(source.path)}>{source.metadata?.key || source.title || source.path}</button>) : <p>No incoming links.</p>}
  </section>;
}

export function App() {
  const [session, setSession] = useState(null);
  const [data, setData] = useState({ files: [], tickets: [], documents: [], statuses: [], statusesByProject: {}, projects: [], diagnostics: [], valid: true });
  const [selectedPath, setSelectedPath] = useState(null);
  const [document, setDocument] = useState(null);
  const [text, setText] = useState('');
  const [savedText, setSavedText] = useState('');
  const [query, setQuery] = useState('');
  const [view, setView] = useState('files');
  const [mode, setMode] = useState('edit');
  const [message, setMessage] = useState('');
  const [conflict, setConflict] = useState(false);
  const [saving, setSaving] = useState(false);
  const [status, setStatus] = useState('');
  const [connected, setConnected] = useState(false);
  const draftsRef = useRef(new Map());

  const selectedFile = useMemo(() => data.files.find(file => file.path === selectedPath) ?? document?.record ?? null, [data.files, document, selectedPath]);
  const ticketStatuses = useMemo(() => ticketStatusOptions(selectedFile, status, data.statusesByProject), [selectedFile, status, data.statusesByProject]);
  const dirty = text !== savedText;
  const dirtyRef = useRef(dirty);
  const selectedPathRef = useRef(selectedPath);
  const savingRef = useRef(false);
  const textRef = useRef(text);
  dirtyRef.current = dirty;
  selectedPathRef.current = selectedPath;
  textRef.current = text;
  const wikiCompletions = useMemo(() => data.files.flatMap(file => [file.path.replace(/\.md(?:own)?$/i, ''), file.metadata?.key]).filter(Boolean), [data.files]);

  const reloadState = useCallback(async () => {
    const response = await fetch('/api/state');
    if (!response.ok) throw new Error(`State request failed (${response.status})`);
    setData(await response.json());
  }, []);

  const openDocument = useCallback(async (pathValue, { discardDraft = false } = {}) => {
    const previousPath = selectedPathRef.current;
    if (previousPath && dirtyRef.current) draftsRef.current.set(previousPath, textRef.current);
    if (discardDraft) draftsRef.current.delete(pathValue);
    setSelectedPath(pathValue);
    setConflict(false);
    setMessage('Loading…');
    const response = await fetch(`/api/document?path=${encodeURIComponent(pathValue)}`);
    if (!response.ok) { setMessage('Document could not be loaded.'); return; }
    const next = await response.json();
    const draft = discardDraft ? undefined : draftsRef.current.get(pathValue);
    setDocument(next);
    setText(draft ?? next.text);
    setSavedText(next.text);
    setConflict(Boolean(draft));
    setStatus(next.record?.status ?? '');
    setMessage('');
  }, []);

  useEffect(() => {
    let active = true;
    Promise.all([fetch('/api/session').then(response => response.json()), fetch('/api/state').then(response => response.json())])
      .then(([nextSession, nextData]) => { if (active) { setSession(nextSession); setData(nextData); } })
      .catch(error => { if (active) setMessage(error.message); });
    return () => { active = false; };
  }, []);

  useEffect(() => {
    const protocol = location.protocol === 'https:' ? 'wss:' : 'ws:';
    const socket = new WebSocket(`${protocol}//${location.host}/api/events`);
    socket.onopen = () => setConnected(true);
    socket.onclose = () => setConnected(false);
    socket.onerror = () => setConnected(false);
    socket.onmessage = async event => {
      let update;
      try { update = JSON.parse(event.data); } catch { return; }
      if (update.type === 'update') {
        const ownSave = savingRef.current;
        await reloadState().catch(() => {});
        if (update.path && update.path === selectedPathRef.current) {
          if (ownSave || savingRef.current) return;
          if (dirtyRef.current && !savingRef.current) setConflict(true);
          else openDocument(update.path).catch(() => {});
        }
      }
    };
    return () => socket.close();
  }, [openDocument, reloadState]);

  const save = async () => {
    if (!session || !document || !dirty) return;
    const savedPath = document.path;
    const savedText = text;
    const stillSelected = () => selectedPathRef.current === savedPath;
    savingRef.current = true;
    setSaving(true); setMessage('Saving…');
    try {
      const response = await fetch('/api/document', {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json', 'X-CSRF-Token': session.csrfToken },
        body: JSON.stringify({ path: document.path, bytesBase64: bytesBase64(text), revision: document.revision }),
      });
      const payload = await response.json();
      if (response.status === 409) {
        if (stillSelected()) { setConflict(true); setMessage('Save rejected because the document changed on disk.'); }
        return;
      }
      if (!response.ok) throw new Error(payload.error?.message || `Save failed (${response.status})`);
      draftsRef.current.delete(savedPath);
      if (stillSelected()) {
        setDocument(current => current?.path === savedPath ? { ...current, revision: payload.revision } : current);
        setSavedText(savedText); setConflict(false);
      }
      setMessage('Saved.'); await reloadState();
    } catch (error) {
      if (stillSelected()) setMessage(error.message);
    } finally { savingRef.current = false; setSaving(false); }
  };

  const changeStatus = async (pathValue, nextStatus) => {
    const file = data.files.find(item => item.path === pathValue);
    if (!file || !session || session.readOnly) return;
    setMessage(`Moving ${file.metadata?.key || pathValue}…`);
    const response = await fetch('/api/status', {
      method: 'POST', headers: { 'Content-Type': 'application/json', 'X-CSRF-Token': session.csrfToken },
      body: JSON.stringify({ path: pathValue, status: nextStatus, revision: { hash: file.hash, size: file.size, mtimeMs: file.mtimeMs } }),
    });
    const payload = await response.json();
    if (!response.ok) { setMessage(payload.error?.message || 'Status change failed.'); return; }
    setMessage('Status updated.'); await reloadState();
    if (selectedPath === pathValue) await openDocument(pathValue);
  };

  const onStatusSelect = nextStatus => {
    setStatus(nextStatus);
    if (selectedPath && nextStatus) changeStatus(selectedPath, nextStatus).catch(error => setMessage(error.message));
  };

  const viewFiles = view === 'roadmap' ? data.documents : data.files;
  const searchResults = query.trim() ? viewFiles.filter(file => `${file.path} ${file.title} ${file.body}`.toLocaleLowerCase().includes(query.trim().toLocaleLowerCase())) : viewFiles;
  const filteredTickets = query.trim() ? data.tickets.filter(ticket => searchResults.some(result => result.path === ticket.path)) : data.tickets;

  return <div className="app-shell">
    <aside className="sidebar">
      <div className="brand"><div className="brand-mark">Z</div><div><strong>Zenith Vault</strong><small>{session?.vault || 'Loading vault…'}</small></div></div>
      <div className="health-row"><span className={`health-dot ${connected ? 'online' : ''}`} />{connected ? 'Live' : 'Connecting…'}{session?.readOnly && <span className="read-only">Read only</span>}</div>
      <label className="search"><span>⌕</span><input placeholder="Search vault" value={query} onChange={event => setQuery(event.target.value)} aria-label="Search vault" /></label>
      <nav className="nav-tabs" aria-label="Vault views">
        {['files', 'board', 'backlog', 'roadmap'].map(value => <button key={value} className={view === value ? 'selected' : ''} onClick={() => setView(value)}>{value[0].toUpperCase() + value.slice(1)}</button>)}
      </nav>
      <FileTree files={searchResults} selected={selectedPath} onSelect={openDocument} query="" />
      {data.diagnostics.length > 0 && <div className="diagnostic-count" title={data.diagnostics.map(item => item.message).join('\n')}>{data.diagnostics.length} index diagnostic{data.diagnostics.length === 1 ? '' : 's'}</div>}
    </aside>
    <main className="workspace">
      <DocumentHeader file={view === 'files' ? selectedFile : null} title={view === 'board' ? 'Board' : view === 'backlog' ? 'Backlog' : view === 'roadmap' ? 'Roadmap' : undefined} mode={mode} setMode={setMode} dirty={dirty} conflict={conflict} onSave={save} onReload={() => openDocument(selectedPath, { discardDraft: true })} readOnly={Boolean(session?.readOnly)} saving={saving} status={status} setStatus={onStatusSelect} statuses={ticketStatuses} />
      {view === 'board' ? <Board tickets={filteredTickets} statuses={data.statuses} onOpen={pathValue => { setView('files'); openDocument(pathValue); }} onStatus={changeStatus} /> : view === 'backlog' ? <Backlog tickets={filteredTickets} onOpen={pathValue => { setView('files'); openDocument(pathValue); }} /> : view === 'roadmap' ? <Roadmap tickets={filteredTickets} onOpen={pathValue => { setView('files'); openDocument(pathValue); }} /> : selectedFile && document ? <div className="document-workspace">
        <section className={`editor-pane ${mode === 'preview' ? 'hidden' : ''}`}><Editor value={text} onChange={setText} completions={wikiCompletions} /></section>
        <section className={`preview-pane ${mode === 'edit' ? 'compact' : ''}`}><div className="preview-label">Preview</div><article className="markdown" onClick={event => {
          const anchor = event.target.closest('a');
          const href = anchor?.getAttribute('href') ?? '';
          if (!href.startsWith('#wiki-')) return;
          event.preventDefault();
          const target = decodeURIComponent(href.slice('#wiki-'.length));
          const destination = data.files.find(file => file.path.replace(/\.md(?:own)?$/i, '') === target || file.metadata?.key === target || file.title === target);
          if (destination) openDocument(destination.path);
        }} dangerouslySetInnerHTML={{ __html: sanitizeMarkdown(markdownWithWikiLinks(text)) }} /><Backlinks file={selectedFile} files={data.files} onOpen={openDocument} /></section>
      </div> : <div className="welcome"><h1>Open a document</h1><p>Choose a Markdown file from the sidebar. Wiki links, backlinks, board state and a live preview are kept in one workspace.</p></div>}
      <footer className="status-bar"><span>{message}</span>{selectedFile && <span>{dirty ? 'Unsaved changes' : 'All changes saved'} · {selectedFile.hash?.slice(0, 8) || 'new'}</span>}</footer>
    </main>
  </div>;
}
