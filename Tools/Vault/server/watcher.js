import path from 'node:path';

import chokidar from 'chokidar';

import { loadVault, readVaultFile, resolveVaultLayout, toPosix } from './scanner.js';

const chokidarWatch = chokidar?.watch ?? chokidar?.default?.watch;

function callbackFrom(value, options) {
  if (typeof value === 'function') return value;
  return options.onUpdate ?? options.onChange ?? (() => {});
}

function normalizeArgs(callbackOrOptions, maybeOptions) {
  if (typeof callbackOrOptions === 'function') {
    return { callback: callbackOrOptions, options: maybeOptions ?? {} };
  }
  const options = callbackOrOptions ?? {};
  return { callback: callbackFrom(options, options), options };
}

function boardRootFor(layout, absolutePath) {
  return layout.boardRoots
    .filter(root => {
      const relative = path.relative(root.absolutePath, absolutePath);
      return relative === '' || (relative !== '..' && !relative.startsWith(`..${path.sep}`) && !path.isAbsolute(relative));
    })
    .sort((a, b) => b.absolutePath.length - a.absolutePath.length)[0];
}

function supportingFileFor(layout, absolutePath) {
  return (layout.supportingFiles ?? []).find(value => path.resolve(value.absolutePath).toLowerCase() === path.resolve(absolutePath).toLowerCase());
}

function isConfigPath(layout, absolutePath) {
  return path.resolve(absolutePath).toLowerCase() === path.resolve(layout.configAbsolutePath).toLowerCase();
}

function eventObject(type, absolutePath, index, record, extra = {}) {
  return {
    type,
    path: absolutePath,
    relativePath: record?.relativePath ?? toPosix(path.relative(index.root, absolutePath)),
    record,
    index,
    diagnostics: index.diagnostics,
    ...extra,
  };
}

function isWatchedFile(layout, absolutePath) {
  if (isConfigPath(layout, absolutePath)) return true;
  return Boolean(supportingFileFor(layout, absolutePath));
}

function isWatchedStructuredPath(layout, absolutePath) {
  if (isWatchedFile(layout, absolutePath)) return true;
  return Boolean(boardRootFor(layout, absolutePath) && /\.(?:md|markdown)$/i.test(absolutePath));
}

/**
 * Resolve chokidar's native `raw` path.  On Windows the path is usually a
 * basename relative to details.watchedPath, and watchedPath can be either a
 * directory or the file itself.  Chokidar's public `change` event is kept as
 * a fallback; raw events only fill the gap when its 50ms change throttle
 * hides a later chunk of the same write.
 */
function rawEventPath(layout, rawPath, details) {
  if (typeof rawPath !== 'string' || rawPath.trim() === '') return null;
  const candidates = [];
  if (path.isAbsolute(rawPath)) candidates.push(path.resolve(rawPath));

  const watchedPath = details?.watchedPath;
  if (typeof watchedPath === 'string' && watchedPath.trim() !== '') {
    const watchedAbsolute = path.resolve(watchedPath);
    const rawBasename = path.basename(rawPath).toLowerCase();
    const watchedIsFile = isWatchedFile(layout, watchedAbsolute)
      || path.basename(watchedAbsolute).toLowerCase() === rawBasename;
    if (watchedIsFile) candidates.push(path.resolve(path.dirname(watchedAbsolute), rawPath));
    candidates.push(path.resolve(watchedAbsolute, rawPath));
    // Keep this candidate after the directory interpretation.  It is needed
    // for a raw event whose watchedPath is a file, but the basename check
    // above makes that interpretation win in the usual Windows shape.
    candidates.push(path.resolve(path.dirname(watchedAbsolute), rawPath));
  }

  const seen = new Set();
  for (const candidate of candidates) {
    const normalized = path.resolve(candidate);
    const key = normalized.toLowerCase();
    if (seen.has(key)) continue;
    seen.add(key);
    if (isWatchedStructuredPath(layout, normalized)) return normalized;
  }
  return null;
}

function pathsEqual(left, right) {
  if (left.length !== right.length) return false;
  const normalize = value => path.resolve(value).toLowerCase();
  const a = left.map(normalize).sort();
  const b = right.map(normalize).sort();
  return a.every((value, index) => value === b[index]);
}

/**
 * A watcher updates one record at a time and rebuilds only the in-memory
 * relationship maps/diagnostics. A config change causes a cold reload because
 * it may add or remove board roots.
 */
export class VaultWatcher {
  constructor(index, fsWatcher, layout, callback, options = {}) {
    this.index = index;
    this.watcher = fsWatcher;
    this.layout = layout;
    this.callback = callback;
    this.options = options;
    this.paths = watchPaths(layout);
    this.closed = false;
    this._tail = Promise.resolve();
    this._closePromise = null;
    this._readyResolved = false;
    this.ready = new Promise(resolve => {
      this._resolveReady = resolve;
    });
    if (fsWatcher?.on) {
      // Chokidar's initial walk and native subscriptions complete together at
      // `ready`.  Rescan after that event so an edit between our cold scan and
      // the subscription cannot leave the in-memory index stale.  The rescan
      // is serialized with file events instead of relying on a timing delay.
      fsWatcher.on('ready', () => {
        this._enqueue(async () => {
          try {
            await this._synchronizeAfterReady();
          } finally {
            this._resolveReadyOnce();
          }
        });
      });
      fsWatcher.on('error', error => this._reportError(error));
    } else {
      this._resolveReadyOnce();
    }
  }

  _resolveReadyOnce() {
    if (this._readyResolved) return;
    this._readyResolved = true;
    this._resolveReady?.(this);
  }

  _reportError(error) {
    const callback = this.options.onError;
    if (typeof callback === 'function') callback(error);
  }

  async _emit(update) {
    try {
      await this.callback?.(update, this.index);
    } catch (error) {
      this._reportError(error);
    }
  }

  _enqueue(work) {
    this._tail = this._tail.then(async () => {
      if (this.closed) return;
      try {
        await work();
      } catch (error) {
        this._reportError(error);
      }
    });
    return this._tail;
  }

  async _synchronizeAfterReady() {
    if (this.closed) return;
    const nextIndex = await loadVault(this.index.root, this.options);
    const nextLayout = await resolveVaultLayout(this.index.root, this.options);
    const nextPaths = watchPaths(nextLayout);
    const previousPaths = this.paths;
    this.index = nextIndex;
    this.layout = nextLayout;
    if (this.watcher?.unwatch && this.watcher?.add && !pathsEqual(previousPaths, nextPaths)) {
      if (previousPaths.length) await this.watcher.unwatch(previousPaths);
      await this.watcher.add(nextPaths);
    }
    this.paths = nextPaths;
  }

  async _reload() {
    const nextIndex = await loadVault(this.index.root, this.options);
    const previous = this.index;
    this.index = nextIndex;
    this.layout = await resolveVaultLayout(this.index.root, this.options);
    if (this.watcher?.unwatch && this.watcher?.add) {
      const nextPaths = watchPaths(this.layout);
      const previousPaths = this.paths;
      if (previousPaths.length) await this.watcher.unwatch(previousPaths);
      await this.watcher.add(nextPaths);
      this.paths = nextPaths;
    }
    await this._emit({
      type: 'rescan',
      path: this.layout.configAbsolutePath,
      relativePath: toPosix(path.relative(this.index.root, this.layout.configAbsolutePath)),
      record: this.layout.configRecord,
      previousIndex: previous,
      index: this.index,
      diagnostics: this.index.diagnostics,
    });
  }

  async _handle(type, filePath) {
    if (this.closed) return;
    const absolutePath = path.resolve(filePath);
    if (isConfigPath(this.layout, absolutePath)) {
      await this._reload();
      return;
    }
    const boardRoot = boardRootFor(this.layout, absolutePath);
    const supporting = supportingFileFor(this.layout, absolutePath);
    if ((!boardRoot && !supporting) || (!supporting && !/\.(?:md|markdown)$/i.test(absolutePath))) return;
    if (type === 'unlink') {
      // Required supporting files must retain the same missing-file diagnostic
      // as a cold scan; simply removing the record would make the vault valid.
      if (supporting && !supporting.optional) {
        await this._reload();
        return;
      }
      const relative = toPosix(path.relative(this.index.root, absolutePath));
      const previous = this.index.removeFile(relative);
      await this._emit(eventObject(type, absolutePath, this.index, previous, { removed: true }));
      return;
    }
    const record = await readVaultFile(absolutePath, {
      root: this.index.root,
      boardRoots: this.layout.boardRoots,
      supportingFiles: this.layout.supportingFiles,
    });
    if (!record) return;
    const previous = this.index.getFile(record.relativePath);
    // A single logical write can produce several raw/native events (for
    // example, one for truncation and one for the final write).  Do not emit
    // duplicate updates when those events observe identical bytes, while
    // still processing each distinct byte state in order.
    if (previous?.hash && previous.hash === record.hash
      && previous.kind === record.kind
      && previous.project === record.project) return;
    this.index.replaceFile(record);
    await this._emit(eventObject(type, absolutePath, this.index, record));
  }

  handle(type, filePath) {
    return this._enqueue(() => this._handle(type, filePath));
  }

  async close() {
    if (this._closePromise) return this._closePromise;
    this._closePromise = (async () => {
      this.closed = true;
      this._resolveReadyOnce();
      await this._tail.catch(() => {});
      await this.watcher?.close?.();
    })();
    return this._closePromise;
  }
}

function watchPaths(layout) {
  return [layout.configAbsolutePath, ...layout.boardRoots.map(root => root.absolutePath), ...(layout.supportingFiles ?? []).map(file => file.absolutePath)];
}

/**
 * Start a watcher over an explicitly supplied vault root.
 *
 * The function is asynchronous so callers always receive a fully built cold
 * index. `await handle.ready` can then be used when tests or consumers need to
 * wait until chokidar has completed its initial directory subscription.
 */
export async function watchVault(vaultRoot, callbackOrOptions, maybeOptions) {
  if (typeof chokidarWatch !== 'function') throw new Error('chokidar.watch is unavailable');
  const { callback, options } = normalizeArgs(callbackOrOptions, maybeOptions);
  const index = await loadVault(vaultRoot, options);
  const layout = await resolveVaultLayout(vaultRoot, options);
  const fsWatcher = chokidarWatch(watchPaths(layout), {
    ignoreInitial: true,
    followSymlinks: false,
    persistent: true,
    awaitWriteFinish: options.awaitWriteFinish ?? {
      stabilityThreshold: 75,
      pollInterval: 10,
    },
    ignored: options.ignored,
  });
  const handle = new VaultWatcher(index, fsWatcher, layout, callback, options);
  for (const type of ['add', 'change', 'unlink']) fsWatcher.on(type, filePath => handle.handle(type, filePath));
  // Chokidar throttles high-level `change` events for 50ms.  Its raw native
  // stream still reports every write chunk, which lets the serialized handler
  // observe the final bytes when a caller explicitly disables awaitWriteFinish.
  if (options.awaitWriteFinish === false && typeof fsWatcher.on === 'function') {
    fsWatcher.on('raw', (event, rawPath, details) => {
      if (event !== 'change') return;
      const absolutePath = rawEventPath(handle.layout, rawPath, details);
      if (absolutePath) handle.handle('change', absolutePath);
    });
  }
  return handle;
}

export const createVaultWatcher = watchVault;
export const startVaultWatcher = watchVault;
export const watch = watchVault;

export default watchVault;
