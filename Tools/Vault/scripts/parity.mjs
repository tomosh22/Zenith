import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import path from 'node:path';
import { isDeepStrictEqual } from 'node:util';
import { fileURLToPath } from 'node:url';

const home = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const fixture = path.resolve(process.argv[2] ?? path.join(home, 'fixtures/snapshot'));
const { loadVault } = await import('../server/main.js');
const { dispatchRead } = await import('../cli/read.js');
const index = await loadVault(path.join(fixture, 'vault'));
const reference = JSON.parse(readFileSync(path.join(fixture, 'reference.json'), 'utf8'));
const priority = ['CRITICAL','HIGH','MEDIUM','LOW','TRIVIAL'];
function normalize(value, argv) {
  const out = JSON.parse(JSON.stringify(value));
  if (argv[0] === 'queue' && Array.isArray(out)) {
    // Only the declared queue tie-break difference is normalized.
    out.sort((a,b) => priority.indexOf(a.priority) - priority.indexOf(b.priority)
      || (index.tickets.get(a.key)?.metadata.position ?? 0) - (index.tickets.get(b.key)?.metadata.position ?? 0)
      || a.key.localeCompare(b.key));
  }
  return out;
}
const failures = [];
for (const c of reference.cases) {
  try {
    const got = await dispatchRead(index, c.argv, c.files);
    // Only the frozen queue is given the declared deterministic ordering.
    // Never sort the implementation's output: that would hide ordering bugs.
    if (got.exitCode !== c.exitCode || !isDeepStrictEqual(JSON.parse(JSON.stringify(got.payload)), normalize(c.payload,c.argv))) {
      failures.push({ argv:c.argv, expected:c.exitCode, actual:got.exitCode, expectedPayload:c.payload, actualPayload:got.payload });
    }
  } catch (error) { failures.push({argv:c.argv,error:error.message}); }
}
const summary = { total:reference.cases.length, passed:reference.cases.length-failures.length, failed:failures.length };
mkdirSync(path.join(home,'artifacts'),{recursive:true});
writeFileSync(path.join(home,'artifacts/parity-result.json'), JSON.stringify({ ...summary, failures },null,2)+'\n');
console.log(JSON.stringify(summary));
if (failures.length) { console.error(JSON.stringify(failures.slice(0,2),null,2)); process.exitCode=1; }
