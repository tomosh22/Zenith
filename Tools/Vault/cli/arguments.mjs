import { parseArgs } from './legacy-pure/args.mjs';
import { assertKnownFlags, isHelpRequest } from './legacy-pure/flags.mjs';

/** Validate the legacy closed flag vocabulary, with explicit local additions. */
export function inspectArguments(argv) {
  const parsed = parseArgs(argv);
  if (isHelpRequest(parsed)) return { help: true, error: null };
  const command = parsed.positional[0];
  // These flags supply local inputs which the old transport supplied out of band.
  if (command === 'guard') parsed.flags.delete('gates');
  if (command === 'board') parsed.flags.delete('roadmap');
  try {
    assertKnownFlags(parsed);
    if (command === 'docs') {
      const unsupported=[...parsed.flags.keys()].filter(name=>name!=='json');
      if (unsupported.length) return {help:false,error:`Local docs commands do not support ${unsupported.map(name=>'--'+name).join(', ')}. Configure explicit supportingFiles in vault.config.json and use local paths.`};
    }
    return { help: false, error: null };
  } catch (error) {
    return { help: false, error: error.message };
  }
}
