const COMMANDS = new Set(['create', 'update', 'comment', 'move', 'link', 'unlink', 'parent', 'sprint', 'version', 'estimate', 'flag', 'resolve']);
const FLAGS = new Set(['project', 'title', 'description', 'type', 'status', 'priority', 'complexity', 'risk', 'points', 'resolution', 'severity', 'repro', 'due', 'environment', 'category', 'assignee', 'sprint', 'parent', 'label', 'fix-version', 'version', 'file', 'body', 'text', 'reason', 'goal', 'release-date', 'start-date', 'end-date', 'date', 'flag', 'clear', 'continuous', 'force', 'blocks', 'blocked-by']);
const BOOLEAN_FLAGS = new Set(['flag', 'clear', 'continuous', 'force']);
const RESERVED = /^--(?:actor|actor-email|human|attempt|claim-attempt|operation-id|op-id|vault|remote|commit)(?:=|$)/i;
function invalid(message, code = 'INVALID') { throw Object.assign(new Error(message), { code }); }
function plain(value, label) { if (!value || typeof value !== 'object' || Array.isArray(value)) invalid(`${label} must be an object`); return value; }

export function browserRevisions(input = {}) {
  const result = {};
  for (const [path, value] of Object.entries(plain(input, 'revisions'))) {
    if (!path || path.startsWith('/') || /^[A-Za-z]:/.test(path) || path.includes('\\') || path.includes('\0') || path.split('/').some(part => ['..', '.', '.git', '.zagent'].includes(part))) invalid('Revision paths must be vault-relative', 'PATH');
    if (value === null) { result[path] = null; continue; }
    plain(value, 'revision');
    if (typeof value.hash !== 'string' || !/^[a-f0-9]{64}$/i.test(value.hash)) invalid('A SHA-256 revision hash is required');
    if (!Number.isInteger(value.size) || value.size < 0) invalid('A non-negative revision size is required');
    result[path] = { hash: value.hash, size: value.size };
  }
  return result;
}

export function browserOperationId(value) {
  if (typeof value !== 'string' || !/^[A-Za-z0-9_.-]{1,160}$/.test(value)) invalid('A valid operationId is required');
  return value;
}

export function browserCommand(input, index) {
  const body = plain(input, 'command');
  for (const key of Object.keys(body)) if (!['args', 'files', 'revisions', 'operationId'].includes(key)) invalid(`Unknown command input: ${key}`);
  if (!Array.isArray(body.args) || !body.args.length || body.args.length > 256 || body.args.some(value => typeof value !== 'string' || value.length > 65536)) invalid('args must be an array of strings');
  const args = [...body.args];
  args[0] = args[0].toLowerCase();
  if (!COMMANDS.has(args[0])) invalid('This command is not available in the browser');
  if (args.some(value => RESERVED.test(value))) invalid('Actor, claim and operation flags are controlled by the server');
  for (let position = 1; position < args.length; position++) {
    const value = args[position];
    if (!value.startsWith('--')) continue;
    const [flag] = value.slice(2).split('=');
    if (!FLAGS.has(flag)) invalid(`Unknown browser flag: --${flag}`);
    if (!BOOLEAN_FLAGS.has(flag) && !value.includes('=')) {
      if (++position >= args.length) invalid(`--${flag} needs a value`);
    }
  }
  if (args[0] === 'sprint' && !['create', 'start', 'complete', 'add', 'remove', 'gate'].includes(args[1])) invalid('Unknown sprint command');
  if (args[0] === 'version' && !['create', 'release', 'set'].includes(args[1])) invalid('Unknown version command');
  const files = {};
  for (const [key, value] of Object.entries(plain(body.files ?? {}, 'files'))) {
    if (!['file', 'body', 'text', 'comment', 'reason', 'worklog'].includes(key) || typeof value !== 'string') invalid('files must contain allowlisted inline text');
    files[key] = value;
  }
  if (args.some(value => value === '--file' || value.startsWith('--file=')) && files.file === undefined) invalid('--file requires inline files.file content');
  const revisions = browserRevisions(body.revisions);
  const option = name => {
    const position = args.indexOf(`--${name}`);
    return position >= 0 ? args[position + 1] : args.find(value => value.startsWith(`--${name}=`))?.slice(name.length + 3);
  };
  const project = option('project')?.toUpperCase();
  if (['create', 'sprint', 'version'].includes(args[0]) && !project) invalid('An explicit project is required');
  const requireRevision = record => {
    if (record && !Object.hasOwn(revisions, record.relativePath)) invalid(`The opened revision is required for ${record.relativePath}`, 'STALE');
  };
  let key;
  if (['update', 'comment', 'move', 'link', 'unlink', 'parent', 'estimate', 'flag', 'resolve'].includes(args[0])) key = args[1];
  if ((args[0] === 'sprint' && ['add', 'remove'].includes(args[1])) || (args[0] === 'version' && args[1] === 'set')) key = args[3];
  if (key) {
    const record = index.getTicket(key);
    if (!record) invalid(`Issue ${key} was not found`, 'NOT_FOUND');
    requireRevision(record);
  }
  if (args[0] === 'version' && args[1] !== 'set') requireRevision([...index.configs.values()].find(record => record.project === project && record.relativePath.endsWith('/versions.md')));
  if (args[0] === 'sprint' && !['create', 'add', 'remove'].includes(args[1])) requireRevision([...index.sprints.values()].find(record => record.project === project && [record.metadata?.name, record.metadata?.slug].some(name => name?.toLowerCase() === args[2]?.toLowerCase())));
  return { args, files, revisions, operationId: browserOperationId(body.operationId) };
}
