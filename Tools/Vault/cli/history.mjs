import {
  allTicketRecords,
  canonicalOf,
  clone,
  sourceOf,
  ticketFor,
} from './compat.mjs';

const ISSUE_KEY = /^[A-Za-z][A-Za-z0-9_]*-\d+$/;
const DEFAULT_LIMIT = 50;

function object(value) {
  return value && typeof value === 'object' && !Array.isArray(value) ? value : {};
}

function has(value, key) {
  return Object.prototype.hasOwnProperty.call(value ?? {}, key);
}

function first(value, names) {
  for (const name of names) {
    if (has(value, name)) return value[name];
  }
  return undefined;
}

function failure(error, exitCode = 1) {
  return { exitCode, payload: { error } };
}

function historyRecords(index) {
  if (index?.history instanceof Map) return [...index.history.values()];
  if (index?.historyFiles instanceof Map) return [...index.historyFiles.values()];
  if (Array.isArray(index?.history)) return index.history.slice();
  if (Array.isArray(index?.historyFiles)) return index.historyFiles.slice();
  if (index?.history && typeof index.history === 'object') return Object.values(index.history);
  return null;
}

function recordRows(record) {
  if (Array.isArray(record)) return record;
  const fields = object(record?.fields);
  if (Array.isArray(fields.rows)) return fields.rows;
  const metadata = object(record?.metadata);
  if (Array.isArray(metadata.rows)) return metadata.rows;
  if (Array.isArray(record?.rows)) return record.rows;
  return [];
}

function metadataRows(index, name) {
  const direct = index?.tables instanceof Map
    ? index.tables.get(name)
    : index?.tables?.[name];
  if (Array.isArray(direct)) return direct;

  const records = [];
  if (index?.metadataFiles instanceof Map) records.push(...index.metadataFiles.values());
  if (index?.files instanceof Map) records.push(...index.files.values());
  if (Array.isArray(index?.metadataFiles)) records.push(...index.metadataFiles);
  for (const record of records) {
    const value = object(record?.fields ?? record?.metadata ?? record);
    const rows = value.tables?.[name];
    if (Array.isArray(rows)) return rows;
  }
  return [];
}

function historyOrdering(index, issueId, issueKey) {
  const candidates = [];
  if (index?.ordering?.history) candidates.push(index.ordering.history);
  if (index?.metadata?.ordering?.history) candidates.push(index.metadata.ordering.history);
  if (index?.vaultConfig?.ordering?.history) candidates.push(index.vaultConfig.ordering.history);

  const records = [];
  if (index?.metadataFiles instanceof Map) records.push(...index.metadataFiles.values());
  if (index?.files instanceof Map) records.push(...index.files.values());
  if (Array.isArray(index?.metadataFiles)) records.push(...index.metadataFiles);
  for (const record of records) {
    const value = object(record?.fields ?? record?.metadata ?? record);
    if (value.ordering?.history) candidates.push(value.ordering.history);
  }

  for (const ordering of candidates) {
    if (!ordering || typeof ordering !== 'object' || Array.isArray(ordering)) continue;
    const values = ordering[String(issueId)] ?? ordering[String(issueKey ?? '')];
    if (Array.isArray(values)) {
      return new Map(values.map((value, position) => [String(value), position]));
    }
  }
  return new Map();
}

function userDisplayName(user) {
  const value = object(user);
  for (const field of ['name', 'display_name', 'displayName', 'email']) {
    const candidate = value[field];
    if (candidate !== undefined && candidate !== null && String(candidate) !== '') return candidate;
  }
  return null;
}

function usersById(index) {
  const users = new Map();
  for (const user of metadataRows(index, 'public.users')) {
    const id = first(user, ['id', 'user_id', 'userId']);
    if (id !== undefined && id !== null && id !== '') users.set(String(id), user);
  }
  return users;
}

function ticketIdentity(ticket) {
  const metadata = canonicalOf(ticket);
  const source = sourceOf(ticket);
  const id = first(source, ['id', 'issue_id', 'issueId'])
    ?? first(metadata, ['id', 'issue_id', 'issueId'])
    ?? first(ticket, ['id', 'issue_id', 'issueId']);
  const key = first(metadata, ['key', 'issue_key', 'issueKey'])
    ?? first(ticket, ['key', 'issue_key', 'issueKey']);
  return { id, key: key === undefined || key === null ? null : String(key) };
}

function rowBelongsToTicket(row, identity) {
  const rowId = first(row, ['issue_id', 'issueId', 'ticket_id', 'ticketId']);
  if (rowId !== undefined && rowId !== null && rowId !== '') {
    return identity.id !== undefined
      && identity.id !== null
      && String(rowId) === String(identity.id);
  }
  const rowKey = first(row, ['issue_key', 'issueKey', 'ticket_key', 'ticketKey', 'key']);
  if (rowKey === undefined || rowKey === null || rowKey === '') return false;
  return identity.key !== null && String(rowKey).toUpperCase() === identity.key.toUpperCase();
}

/**
 * Parse a timestamp into integer microseconds since the Unix epoch.
 *
 * Exported history timestamps are PostgreSQL timestamps without a timezone,
 * so the vault treats those values as UTC. Keeping the fractional component
 * as an integer avoids Date's millisecond truncation changing event order.
 */
function timestampMicros(value) {
  if (value instanceof Date) {
    const milliseconds = value.getTime();
    return Number.isFinite(milliseconds) ? BigInt(milliseconds) * 1000n : null;
  }
  if (typeof value === 'number' && Number.isFinite(value)) {
    return BigInt(Math.trunc(value)) * 1000n;
  }

  const text = value === undefined || value === null ? '' : String(value).trim();
  const match = /^(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2})(?:\.(\d+))?(Z|[+-]\d{2}:?\d{2})?$/.exec(text);
  if (!match) return null;
  const zone = match[3] ?? 'Z';
  const milliseconds = Date.parse(`${match[1]}${zone}`);
  if (!Number.isFinite(milliseconds)) return null;
  const fraction = (match[2] ?? '').slice(0, 6).padEnd(6, '0');
  return BigInt(milliseconds) * 1000n + BigInt(fraction);
}

function timestampToISOString(value) {
  if (value === undefined || value === null) return null;
  if (value instanceof Date) {
    return Number.isFinite(value.getTime()) ? value.toISOString() : null;
  }

  const text = String(value).trim();
  const match = /^(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2})(?:\.(\d+))?(Z|[+-]\d{2}:?\d{2})?$/.exec(text);
  if (match) {
    const zone = match[3] ?? 'Z';
    const milliseconds = (match[2] ?? '').slice(0, 3).padEnd(3, '0');
    const parsed = new Date(Date.parse(`${match[1]}.${milliseconds}${zone}`));
    if (Number.isFinite(parsed.getTime())) return parsed.toISOString();
  }

  const parsed = new Date(text);
  return Number.isFinite(parsed.getTime()) ? parsed.toISOString() : value;
}

function compareTextDescending(left, right) {
  const a = String(left ?? '');
  const b = String(right ?? '');
  if (a === b) return 0;
  return a > b ? -1 : 1;
}

function historyRows(index, ticket) {
  const records = historyRecords(index);
  let rows;
  if (records !== null) {
    // History files are the frozen source of truth. In particular, do not
    // silently substitute a stale exported SQL table when a history file is
    // present but empty.
    rows = records.flatMap(recordRows);
  } else {
    // Keep small hand-authored indexes useful while retaining the same
    // read-only shape as a scanned vault.
    rows = metadataRows(index, 'jira.issue_activity');
  }

  const identity = ticketIdentity(ticket);
  const observedOrder = historyOrdering(index, identity.id, identity.key);
  const users = usersById(index);
  return rows
    .filter(row => row && typeof row === 'object' && rowBelongsToTicket(row, identity))
    .map((row, ordinal) => {
      const actorId = first(row, ['actor_id', 'actorId']);
      const actorUser = actorId === undefined || actorId === null
        ? undefined
        : users.get(String(actorId));
      const actorFallback = first(row, ['actor', 'actor_name', 'actorName']);
      const actor = actorUser
        ? userDisplayName(actorUser)
        : typeof actorFallback === 'object'
          ? userDisplayName(actorFallback)
          : actorFallback ?? null;
      const createdAt = first(row, ['created_at', 'createdAt', 'timestamp', 'time']);
      return {
        field: first(row, ['field', 'name']) ?? null,
        fromValue: first(row, ['from_value', 'fromValue']) ?? null,
        toValue: first(row, ['to_value', 'toValue']) ?? null,
        createdAt: timestampToISOString(createdAt),
        actor: actor ?? null,
        _createdAt: createdAt,
        _id: first(row, ['id', 'activity_id', 'activityId']) ?? '',
        _ordinal: ordinal,
      };
    })
    .sort((left, right) => {
      const leftMicros = timestampMicros(left._createdAt);
      const rightMicros = timestampMicros(right._createdAt);
      if (leftMicros !== null && rightMicros !== null && leftMicros !== rightMicros) {
        return leftMicros > rightMicros ? -1 : 1;
      }
      if (leftMicros !== null && rightMicros === null) return -1;
      if (leftMicros === null && rightMicros !== null) return 1;
      if (leftMicros === null && rightMicros === null) {
        const timestampOrder = compareTextDescending(left._createdAt, right._createdAt);
        if (timestampOrder !== 0) return timestampOrder;
      }
      const idOrder = compareTextDescending(left._id, right._id);
      if (leftMicros !== null && leftMicros === rightMicros) {
        const leftPosition = observedOrder.get(String(left._id));
        const rightPosition = observedOrder.get(String(right._id));
        if (leftPosition !== undefined && rightPosition !== undefined && leftPosition !== rightPosition) {
          return leftPosition - rightPosition;
        }
      }
      return idOrder !== 0 ? idOrder : right._ordinal - left._ordinal;
    })
    .map(({ _createdAt, _id, _ordinal, ...row }) => row);
}

function option(argv, name) {
  const args = Array.isArray(argv) ? argv : [];
  const exact = args.indexOf(name);
  if (exact >= 0 && exact + 1 < args.length && !String(args[exact + 1]).startsWith('--')) {
    return args[exact + 1];
  }
  const prefix = args.find(value => typeof value === 'string' && value.startsWith(`${name}=`));
  return prefix ? prefix.slice(name.length + 1) : undefined;
}

function parseLimit(argv) {
  const value = option(argv, '--limit');
  if (value === undefined) return { limit: DEFAULT_LIMIT };
  const parsed = Number(value);
  if (!Number.isInteger(parsed) || parsed < 0) {
    return {
      error: `--limit must be a whole number — got "${value}".`,
    };
  }
  return { limit: parsed };
}

function findTicket(index, requested) {
  const direct = ticketFor(index, requested);
  if (direct) return direct;
  const upper = String(requested).toUpperCase();
  if (upper !== String(requested)) {
    const normalized = ticketFor(index, upper);
    if (normalized) return normalized;
  }
  return allTicketRecords(index).find(record => {
    const key = canonicalOf(record).key ?? record?.key;
    return String(key ?? '').toUpperCase() === upper;
  });
}

/** Dispatch the read-only history query. Return null for other commands. */
export function dispatchHistoryRead(index, args = []) {
  if (!Array.isArray(args) || args[0] !== 'history') return null;

  const requested = args[1];
  if (requested === undefined || requested === null || requested === '') {
    return failure('Usage: zagent history <TICKET_KEY>');
  }
  const key = String(requested);
  if (!ISSUE_KEY.test(key)) {
    return failure(`"${key}" is not a ticket key (expected something like ZM-14).`);
  }

  const parsedLimit = parseLimit(args);
  if (parsedLimit.error) return failure(parsedLimit.error);

  const ticket = findTicket(index, key);
  if (!ticket) return failure(`No ticket ${key}.`, 3);

  const values = historyRows(index, ticket);
  return { exitCode: 0, payload: clone(values.slice(0, parsedLimit.limit)) };
}

export {
  historyRows,
  timestampMicros,
  timestampToISOString,
};

export default dispatchHistoryRead;
