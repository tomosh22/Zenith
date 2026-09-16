import { executeOperation } from '../../server/operations.mjs';
import { patchFields } from '../../server/text-edit.mjs';
const root = process.argv[2];
try {
  const result = await executeOperation(root, { id: process.argv[3], kind: 'child update', prepare: async index => {
    if (process.env.VAULT_TEST_HOLD === '1') { console.log('LOCK_HELD'); await new Promise(resolve => setTimeout(resolve, 60000)); }
    return ({
    changes: ['FD-1', 'FD-2'].map(key => {
      const ticket = index.getTicket(key);
      return { path: ticket.relativePath, bytes: patchFields(ticket.rawBytes, { title: 'After crash' }) };
    }), result: { ok: true },
  }); } });
  console.log(JSON.stringify(result));
} catch (e) { console.error(e.code, e.message); process.exitCode = 1; }
