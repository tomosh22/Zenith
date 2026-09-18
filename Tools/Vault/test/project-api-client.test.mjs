import { describe, expect, it, vi } from 'vitest';
import { createProjectApi } from '../web/project-api.js';

describe('project API browser client', () => {
  it('sends inline JSON and CSRF for writes without changing the supplied draft', async () => {
    const fetcher = vi.fn().mockResolvedValue({ ok: true, json: async () => ({ result: { key: 'ZEN-24' } }) });
    const body = { action: 'issue.create', data: { title: 'A draft' } };
    const result = await createProjectApi('launch-token', fetcher)('/api/actions', { method: 'POST', body });
    expect(result.result.key).toBe('ZEN-24');
    const options = fetcher.mock.calls[0][1];
    expect(options.headers).toEqual({ 'Content-Type': 'application/json', 'X-CSRF-Token': 'launch-token' });
    expect(JSON.parse(options.body)).toMatchObject(body);
    expect(JSON.parse(options.body).operationId).toMatch(/^[a-f0-9-]{36}$/);
    expect(body.operationId).toBeUndefined();
  });
  it('retains an explicit operation ID for retrying the same mutation', async () => {
    const fetcher = vi.fn().mockResolvedValue({ ok: true, json: async () => ({}) });
    await createProjectApi('token', fetcher)('/api/actions', { method: 'POST', body: { operationId: 'stable-retry' } });
    expect(JSON.parse(fetcher.mock.calls[0][1].body).operationId).toBe('stable-retry');
  });
  it('returns structured conflict details so the UI can preserve a draft', async () => {
    const fetcher = vi.fn().mockResolvedValue({ ok: false, status: 409, json: async () => ({ error: { code: 'STALE', message: 'Reload this issue before saving.' } }) });
    await expect(createProjectApi('token', fetcher)('/api/actions', { method: 'POST', body: {} })).rejects.toMatchObject({ status: 409, code: 'STALE', message: 'Reload this issue before saving.' });
  });
  it('reads without a write token or fabricated body', async () => {
    const fetcher = vi.fn().mockResolvedValue({ ok: true, json: async () => ({ projects: [] }) });
    await expect(createProjectApi('token', fetcher)('/api/projects')).resolves.toEqual({ projects: [] });
    expect(fetcher.mock.calls[0][1]).toEqual({ method: 'GET', headers: {} });
  });
});
