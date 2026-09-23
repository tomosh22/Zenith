// Drives `executeIntegrate` directly (bypassing `cli/integrate.mjs`'s own
// read-only refusals/classification, exactly the way `operation-child.mjs`
// drives `executeOperation` directly) in a separate process so a
// `VAULT_TEST_CRASH_AFTER_INTEGRATE_*` hook's `SIGKILL` cannot take the test
// runner down with it.
import { executeIntegrate } from '../../server/operations.mjs';
const [root, key, project, baseBranch, tip, title] = process.argv.slice(2);
try {
  const result = await executeIntegrate(root, { key, project, baseBranch, tip, title });
  console.log(JSON.stringify(result));
} catch (e) { console.error(e.code, e.message); process.exitCode = 1; }
