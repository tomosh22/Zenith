import React, { useRef, useState } from 'react';

export function CreateProjectForm({ api, revisions, onCreated, onCancel }) {
  const [name, setName] = useState('');
  const [key, setKey] = useState('');
  const [description, setDescription] = useState('');
  const [error, setError] = useState('');
  const [saving, setSaving] = useState(false);
  const keyEdited = useRef(false);
  const pending = useRef(null);
  const openedRevisions = useRef(revisions ?? {});
  async function submit(event) {
    event.preventDefault();
    if (saving) return;
    if (!name.trim()) { setError('Enter a project name.'); return; }
    if (!/^[A-Z][A-Z0-9]{0,9}$/.test(key)) { setError('Use 1–10 uppercase letters or numbers, starting with a letter.'); return; }
    const data = { key, name: name.trim(), description: description.trim() };
    const revisions = openedRevisions.current;
    const signature = JSON.stringify({ data, revisions });
    if (pending.current?.signature !== signature) pending.current = { signature, operationId: crypto.randomUUID() };
    setError(''); setSaving(true);
    try {
      const response = await api('/api/actions', { method: 'POST', body: { action: 'project.create', project: key, data, revisions, operationId: pending.current.operationId } });
      onCreated(response.result?.key || key);
    } catch (cause) { setError(cause.message); }
    finally { setSaving(false); }
  }
  return <form onSubmit={submit} className="create-project-form">
    <h2>Create project</h2>
    <label htmlFor="project-name">Name</label><input id="project-name" value={name} disabled={saving} maxLength={100} onChange={event => { setName(event.target.value); if (!keyEdited.current) setKey(event.target.value.toUpperCase().replace(/[^A-Z0-9]/g, '').slice(0, 10)); }} />
    <label htmlFor="project-key">Key</label><input id="project-key" value={key} disabled={saving} maxLength={10} onChange={event => { keyEdited.current = true; setKey(event.target.value.toUpperCase()); }} />
    <p>1–10 letters or numbers, starting with a letter. Used in issue keys.</p>
    <label htmlFor="project-description">Description</label><textarea id="project-description" value={description} disabled={saving} onChange={event => setDescription(event.target.value)} />
    {error && <p role="alert">{error}</p>}
    <div><button type="button" disabled={saving} onClick={onCancel}>Cancel</button> <button type="submit" disabled={saving}>{saving ? 'Creating…' : 'Create project'}</button></div>
  </form>;
}

