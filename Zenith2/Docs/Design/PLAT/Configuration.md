# PLAT design: configuration layering

This file is part of the PLAT design family. The entry document is `Zenith2/Docs/Design/PLAT.md`. It specifies the configuration sources of the six layers, how and when they are applied to ZenithCore's store, key registration order, provenance and the provenance chain, the command-line and replay sources, runtime changes, persistence of the user layer, corrupt-configuration handling, the configuration dump and the keys PLAT registers. The SPEC baseline is `Zenith2/Docs/Architecture/PlatformLayer.md` → "Configuration layering"; the value types, key descriptors, schema, layered store, JSON layer parser and store dump are ZenithCore's and are specified in `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Configuration store", which this file cites and never restates.

## Scope of this file

| Topic | Heading here |
|---|---|
| what CORE owns and what PLAT owns | "Ownership split with CORE" |
| the layer table, sub-sources, quality-tier resolution | "Layers and sources" |
| key naming, registration order, libraries below ZenithPlatform, early and simulation keys | "Key registration" |
| building the store from the sources | "Resolution algorithm" |
| provenance, the chain, rejection reasons | "Provenance" |
| the file rules PLAT adds to CORE's format | "Configuration file format" |
| `--set` and replay headers | "Command-line layer" |
| user writes, tier changes, restart-required keys, OS accessibility preference changes | "Runtime changes" |
| the user file | "Persisting the user layer" |
| corrupt and invalid configuration | "Corrupt configuration handling" |
| `--config-dump` | "The configuration dump" |

The requirement each heading specifies is listed in `Zenith2/Docs/Design/PLAT.md` → "Requirement index".

## Ownership split with CORE

Configuration is a CORE inventory item (Z2-I-CORE-16, "configuration — layering itself is PLAT"). The split follows `Zenith2/Docs/Architecture/PlatformLayer.md` → "Configuration layering" and `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Configuration store":

| Piece | Owner | Header |
|---|---|---|
| `Zenith_ConfigLayer`, `Zenith_ConfigValueType`, `Zenith_ConfigValue`, `Zenith_ConfigKeyDesc`, `Zenith_ConfigProvenance`, `Zenith_ConfigError`, `Zenith_ConfigKey`, `Zenith_ConfigDiagnostic`, `Zenith_ConfigSchema`, `Zenith_ConfigStore`; the layer-file JSON format and its parser (`ApplyJsonLayer`), `--set` value parsing (`ApplyValueText`), per-key resolution and validation, staged user values, the store dump, the simulation-affecting list writer | CORE | `Zenith2/Engine/ZenithCore/Zenith_ConfigStore.h` |
| every source: which files make each layer and in what order they are applied, reading them through the virtual filesystem, the command line and replay sources, quality-tier selection, the provenance chain, restart-pending user values, persisting the user layer, corrupt-file handling, change notification for tier changes, the extended dump, PLAT's own keys; the `Zenith_Config` facade | PLAT | `Zenith2/Engine/ZenithPlatform/Zenith_Config.h` |

`Zenith_Config` owns the one `Zenith_ConfigStore` and is its only writer. Libraries at or above ZenithPlatform read through the facade or `GetStore()`; libraries below it receive their values in their descs, filled by the composition root. The store is `MAIN_THREAD` (`Zenith2/Docs/Design/CORE/Diagnostics.md` → "Configuration store"), so no job reads configuration: a value needed on another thread is copied into its owner's desc or re-read on the main thread when a change callback fires.

PLAT uses three store members of `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Configuration store" beyond reads and staging: `GetLayerEntry(xKey, eLayer)`, which returns the per-layer slot (`Zenith_ConfigLayerEntry`: value and provenance) or null, for the provenance chain and the user-file writer; `ClearLayer(eLayer)` for quality-tier reloads; and `ApplySimulationAffecting(xList, strSource)` for replay runs.

## Layers and sources

### Layer table

Lowest to highest precedence (`Zenith2/Docs/Architecture/PlatformLayer.md` → "Configuration layering"):

| Layer | `Zenith_ConfigLayer` | Sources, in application order (the last applied wins within the layer) | Applied at |
|---|---|---|---|
| 1 engine default | `CONFIG_LAYER_ENGINE_DEFAULT` | the default in each key's registration | schema registration, at the start of startup step 6 |
| 2 platform | `CONFIG_LAYER_PLATFORM` | `content:/Config/Platform/<windows\|android>.config.json`, then the OS accessibility preferences (source `os:accessibility`, `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Preference keys and the platform layer") | startup step 6; the OS sub-source again at prologue step 3 when the OS reports a change (§ "OS accessibility preference changes") |
| 3 game | `CONFIG_LAYER_GAME` | `content:/Config/game.config.json` (cooked from `Games/<Name>/Assets/Config/game.config.json`) | startup step 6 |
| 4 quality tier | `CONFIG_LAYER_QUALITY_TIER` | `content:/Engine/Config/Quality/<tier>.config.json`, then `content:/Config/Quality/<tier>.config.json`, for the active tier | startup step 6 (after the tier pass) and at every tier change |
| 5 user | `CONFIG_LAYER_USER` | `user:/config/user.config.json` | startup step 6; changed by `SetUserValue` |
| 6 command line | `CONFIG_LAYER_COMMAND_LINE` | every `--set <key>=<value>` in argument order, then, in `--replay` runs, the replay header's simulation-affecting list | startup step 6 (parsed at step 3) |

### Engine and game sub-sources

<!-- elaboration -->
- Layer 2 is one cooked file. The cook produces it by merging every engine library's fragment `Zenith2/Engine/<Library>/Internal/Config/Platform/<platform>.config.json` (a key defined by two fragments is a cook error) and then the game's `Games/<Name>/Assets/Config/Platform/<platform>.config.json`, whose members override the fragments' key by key; the cooked file's members are written in key byte order, so provenance names the cooked file and line.
- Layer 4 has two sub-sources: the engine tier file, cooked the same way from every library's `Internal/Config/Quality/<tier>.config.json` into `content:/Engine/Config/Quality/<tier>.config.json`, and the game's `content:/Config/Quality/<tier>.config.json`, applied in that order with two `ApplyJsonLayer` calls, so the game file wins key by key.
- A missing file is not an error: the layer contributes nothing from it (logged at debug level). A file that is present and fails as a whole is handled by "Corrupt configuration handling".

The merge is ASSET's `plat.config` cook step, which applies exactly these rules (`Zenith2/Docs/Design/ASSET/Cook.md` → "Configuration merge": a key two fragments define is `COOK_ERROR_CONFIG_KEY_CONFLICT` naming both files, and the game file overrides key by key); PLAT specifies only the result the runtime reads. The tier list is cooked from ZenithPlatform's `Internal/Config/Quality/tiers.<platform>.json` to `content:/Engine/Config/Quality/tiers.<platform>.json` (§ "Quality tier resolution"), the path ASSET conforms to (row P19 of `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md`).

### Quality tier resolution

`quality.tier` selects the layer-4 files, but the layer that could set it would itself depend on it; it is therefore resolved in two passes:

1. Pass 1: apply layers 2, 3, 5 and 6 and read the resolved selection: `quality.activeTier` when it is non-empty, otherwise `quality.tier` (both registrations exclude layer 4 from their allowed layers).
2. Validate it against the platform's tier list, `content:/Engine/Config/Quality/tiers.<windows|android>.json` (`{ "schema": "zenith2.quality-tiers", "schemaVersion": 1, "tiers": [ … ] }`, highest first). Tier IDs have the form `QUALITY_TIER_<PLATFORM>_<NAME>` of `Zenith2/Docs/Budgets.md` → "Production-quality acceptance"; the lists themselves come from `Zenith2/Docs/QualityMatrix.md` (pending; its "Quality tiers" section), and the names used in this file's examples (`QUALITY_TIER_S24_HIGH`, `QUALITY_TIER_S24_LOW`, `QUALITY_TIER_WIN_HIGH`) are illustrative. An unknown tier falls back to `QUALITY_TIER_<PLATFORM>_DEFAULT`, or to the list's middle entry when that is not listed either, with a warning.
3. Pass 2: apply the tier's two files as layer 4. Because the store resolves by layer precedence, applying layer 4 after layers 5 and 6 still leaves 5 and 6 winning.
4. The ordered tier list is also the list thermal adaptation steps down and up through (`Zenith2/Docs/Design/PLAT/Android.md` → "Applying thermal actions").

## Key registration

### Key grammar

The key grammar is CORE's (`<area>.<name>`, lower camel case per segment, at most 128 bytes; `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Configuration store"). The first segment names the owning library or module; it is a naming convention checked in review:

<!-- elaboration -->
| Prefix | Owner |
|---|---|
| `platform`, `input`, `io`, `vfs`, `config`, `thermal`, `devtools`, `quality` | ZenithPlatform |
| `log`, `strings`, `memory` | ZenithCore |
| `jobs`, `sim` | ZenithJobs |
| `rhi`, `r`, `render` | ZenithRHI and ZenithFlux |
| `ecs`, `script`, `asset`, `save`, `runtime`, `content` | their libraries |
| one prefix per module, its `MODULE_NAME` in lower camel case (`audio`, `physics`, `ui`, `net`, `online`, `streaming`, …) | the module |

### Registration API

Keys are registered into ZenithCore's `Zenith_ConfigSchema` with CORE's `Zenith_ConfigKeyDesc` before the store exists; `Zenith_Config::Create` receives the frozen schema (`Zenith2/Docs/Architecture/PlatformLayer.md` → "Configuration layering"). Every library with keys provides one function of the CORE form, `Zenith_Result<void, Zenith_ConfigError> Zenith_Register<Library>ConfigKeys(Zenith_ConfigSchema&, Zenith_BuildPlatform)`; ZenithPlatform's is `Zenith_RegisterPlatformConfigKeys`, which registers the table of "Keys PLAT registers".

The order is `Zenith2/Docs/Architecture/CompositionRoot.md` → "Startup sequence", step 6 ("declarations first"): at the start of step 6, before `Zenith_Config::Create`, the composition root registers every engine library's keys in library-layer order, then the runtime's own keys, then calls each module's `m_pfnRegisterTypes`, which registers the module's types and, through `GetConfigSchema`, its configuration keys (a module has no configuration entry point of its own, X61); then it freezes the schema (`Zenith2/Docs/Design/MOD.md` → "Startup sequence with unwinding"). A registration failure is `RUNTIME_ERROR_CONFIGURATION` (exit code 2). Every key exists before the store is built.

`Zenith_Config::Create` checks PLAT's three schema rules over every registered key and fails with `CONFIG_SOURCE_ERROR_SCHEMA_RULE`, logging the key, when one is broken:

1. A simulation-affecting key must be restart-required (so it never changes during a run).
2. A restart-required key must not allow `CONFIG_LAYER_QUALITY_TIER`, so a tier reload never writes a restart-required key into the store and its pending value keeps one home (§ "Restart-required keys"); with rule 1 this also keeps every simulation-affecting key out of layer 4, so a tier change or thermal adaptation can never change simulation (Z2-D-019).
3. `quality.tier` and `quality.activeTier` must not allow `CONFIG_LAYER_QUALITY_TIER`.

### Keys of libraries below ZenithPlatform

Libraries below ZenithPlatform (ZenithCore, ZenithJobs and every other layer 0 to 2 library) register their keys through ZenithCore's schema without depending on ZenithPlatform, and read their values from their descs (`Zenith2/Docs/Design/CORE/Diagnostics.md` → "Configuration store"). No configuration-key file lives in ZenithRuntime: each library's registration function is its own, specified by that library's area.

### Early keys

Keys consumed before startup step 6 — the platform's queue capacities, read at step 4, and anything else the Player or `Zenith_Platform::Create` needs — can only come from the engine default and the command line: they register with `m_uAllowedLayerMask` = engine default | command line and `m_bRequiresRestart = true`, and the runtime fills `Zenith_PlatformDesc` from the parsed command line's `--set` values for those keys (through `Zenith_ConfigStore::ApplyValueText` on a scratch store over the same schema), so the value at step 4 equals the value the configuration reports after step 6.

### Simulation-affecting keys

- A key that can change simulation results (`sim.tickHz`, `sim.maxCatchUpTicks`, `physics.substeps`, and every other key its owner flags) is registered with `m_bSimulationAffecting = true`, subject to the schema rules above.
- The replay writer (SAVE) takes the list from `Zenith_ConfigStore::WriteSimulationAffecting` (CORE's byte layout) for the replay header (`Zenith2/Docs/Architecture.md` → "Multiplayer foundations").
- In a `--replay` run, `Zenith_Config::Create` applies the header's list with `ApplySimulationAffecting` after every `--set`, as the last command-line sub-source with source `replay:<file>`, so a replay always runs with its recorded values; a `--set` of the same key is superseded and PLAT records it in the chain as overridden by the replay with a warning `config: command line: <key>: replaced by the replay header`.

## Resolution algorithm

`Zenith_Config::Create(const Zenith_ConfigDesc&)` (startup step 6), over the frozen schema:

1. Create the store (`Zenith_ConfigStore::Create`); the engine defaults are its layer-1 slots.
2. Read the layer-2 and layer-3 files through the virtual filesystem (`ReadWhole`, at most 1 MiB each) and apply each with `ApplyJsonLayer(layer, bytes, "<virtual path>")`; after the layer-2 file, apply the reported OS accessibility preferences of `m_pxPlatform->GetAccessibility()` (when both exist) to `CONFIG_LAYER_PLATFORM` with `ApplyValueText(…, "os:accessibility", 0)` (`Zenith2/Docs/Design/PLAT/Accessibility.md` → "Preference keys and the platform layer").
3. Read the user file (at most 256 KiB) and apply it to `CONFIG_LAYER_USER`; restart-required keys in it apply normally at startup (the value the player chose last run takes effect now).
4. Apply every `--set` in argument order with `ApplyValueText(CONFIG_LAYER_COMMAND_LINE, key, value, "command line", 0)`; then, in replay runs, the replay list (§ "Simulation-affecting keys").
5. Quality tier pass (§ "Quality tier resolution"), applying the two tier files as layer 4.
6. `ValidateRequired()`; a missing required key stops startup with `CONFIG_SOURCE_ERROR_REQUIRED_KEY_MISSING` (exit code 2), naming the key.
7. Build the provenance chains from the store's layer entries and diagnostics (§ "Provenance chain"); log every unknown key once (the store's `CONFIG_ERROR_UNKNOWN_KEY` diagnostics, with source and line) and count them in `config.unknown_keys`.

Per-key validation — allowed layers, type, range, enum names, nested values — is the store's (`Zenith2/Docs/Design/CORE/Diagnostics.md` → "Resolution algorithm"): a rejected value is a diagnostic and a logged warning naming the key, the source and the line, and the next-lower layer's value stays in effect ([Z2-N-ARCH-929]). PLAT itself never stops startup for a rejected value; a key whose owner requires a valid value and rejects the resolved one at use (for example `sim.maxCatchUpTicks = 0`) is the owner's startup error.

Complexity: O(S) to parse S bytes of sources; O(K × 6) to build chains for K keys; no allocation after `Create` except the user-write queue, the chain updates of runtime changes and the dump.

## Provenance

### Provenance chain

- `GetProvenance(xKey)` returns the winning `Zenith_ConfigProvenance` (layer, source, line) of the key's chain, as `Zenith2/Docs/Architecture/PlatformLayer.md` → "Configuration layering" declares. For values loaded from sources the chain's entry is the store's; for a runtime user write it carries the writer's source string, which the store's staging does not record: `"settings"` for `SetUserValue(xKey, xValue)` (the settings UI and scripts) and the string passed to `SetUserValue(xKey, xValue, strSource)` otherwise, `"thermal"` for the thermal target's writes of `quality.activeTier`; the line is 0.
- `GetProvenanceChain(xKey)` returns every `Zenith_ConfigProvenanceEntry` for the key, lowest layer first: one accepted entry per layer whose slot is set (from `GetLayerEntry`), and one rejected entry per store diagnostic naming the key, placed at its layer, plus the replay override of § "Simulation-affecting keys". Within one layer only the last accepted sub-source is kept (the store holds one slot per layer); every rejection is kept. Exactly one entry is marked winning.
- Chains are built once in `Create`, kept for the process lifetime (at most 24 entries per key: the 8 sub-sources each contribute at most one accepted and one rejected entry, plus the replay override), and updated by user writes and tier changes, so the dump and the configuration panel explain every value.

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_Config.h
struct Zenith_ConfigProvenanceEntry
{
	Zenith_ConfigProvenance m_xSource;      // layer, source, line
	Zenith_ConfigValue m_xValue;            // the accepted value; default-constructed for a rejected entry
	Zenith_ConfigError m_eRejected;         // 0 when accepted; the store's diagnostic code otherwise
	bool m_bOverriddenByReplay = false;     // a --set of a simulation-affecting key replaced by the replay header
	bool m_bPendingRestart = false;         // a runtime user value of a restart-required key (§ "Restart-required keys")
	bool m_bWinning = false;
};
```

### Rejection reasons

The reasons are the store's diagnostic codes (`Zenith2/Docs/Design/CORE/Diagnostics.md` → "Configuration store"); each is logged with the store's message and recorded in the chain:

| Code | Example |
|---|---|
| `CONFIG_ERROR_LAYER_NOT_ALLOWED` | `sim.tickHz` in the user file |
| `CONFIG_ERROR_WRONG_TYPE` | `"audio.masterVolume": "loud"` |
| `CONFIG_ERROR_OUT_OF_RANGE` | `"jobs.workerCount": 500` |
| `CONFIG_ERROR_INVALID_ENUM_VALUE` | `"rhi.backend": "metal"` |
| `CONFIG_ERROR_NESTED_OBJECT` | `"render.shadows": { "cascades": 3 }` |
| `CONFIG_ERROR_UNKNOWN_KEY` | `"render.shadowCascade": 3` (not in the chain of any key; listed under `unknownKeys` in the dump) |
| replay override (`m_bOverriddenByReplay`) | `--set sim.tickHz=30` with a 60 Hz replay |

## Configuration file format

### Configuration file schema

Layer files are CORE's format: one JSON object of dotted keys and scalar values (`Zenith2/Docs/Design/CORE/Diagnostics.md` → "Configuration file format"), for example:

```json
{
	"rhi.backend": "vulkan",
	"sim.tickHz": 30,
	"render.shadowCascades": 3,
	"platform.windowMode": "borderless"
}
```

PLAT adds, for the files it reads:

- a size cap, checked before parsing: 1 MiB for content files, 256 KiB for the user file; a larger file is treated as corrupt (§ "Corrupt configuration handling");
- the files are read through the virtual filesystem (`content:/` and `user:/` paths only, `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Synchronous file access"), under the parser limits of `Zenith2/Docs/Architecture.md` → "Security architecture";
- the cook validates every game configuration file against the tools build's registered schema (entry document → "Configuration authoring"), so a shipped file meets no rejection at startup.

### Value encoding per type

JSON values and `--set` text are converted by the store (`Zenith2/Docs/Design/CORE/Diagnostics.md` → "Configuration file format"): `BOOL` accepts `true` and `false`, `INT` integer text within `sint64`, `FLOAT` any number within `float` range, `STRING` a string (on the command line, the whole text after the first `=`), `ENUM` one of the key's names. PLAT adds nothing to the conversion; the values it writes into the user file use the same JSON forms.

## Command-line layer

- `Zenith_CommandLine::GetSetOptions()` yields each `--set` argument as `<key>=<value>` (`Zenith2/Docs/Architecture/CompositionRoot.md` → "Command-line options"); an argument without `=` or with an empty key is a command-line error (exit code 2) reported by the command-line parser.
- A key given twice is applied in argument order; the last one wins (the store keeps one command-line slot), and the earlier one is not in the chain.
- `--headless` is `runtime.headless=true` (`Zenith2/Docs/Architecture/CompositionRoot.md` → "Command-line options").
- An unknown key on the command line is a warning like any unknown key ([Z2-N-ARCH-929]); the process continues.

## Runtime changes

### User-layer writes

`Zenith_Config` keeps Architecture's rule: after `Create` it is immutable except for user-layer writes through `SetUserValue`, committed at the frame prologue (`Zenith2/Docs/Architecture/PlatformLayer.md` → "Configuration layering"). Every runtime change — a player's settings choice, a quality-tier change by the player or by thermal adaptation — is such a write.

1. `SetUserValue(xKey, xValue)` or `SetUserValue(xKey, xValue, strSource)` (main thread; the settings UI and scripts' settings bindings call the first, the runtime's thermal target the second with `"thermal"`) returns the store's validation error (`CONFIG_ERROR_LAYER_NOT_ALLOWED`, `WRONG_TYPE`, `OUT_OF_RANGE`, `INVALID_ENUM_VALUE`) without queueing anything.
2. A key that is not restart-required is staged with `StageUserValue`; staging holds at most one value per key (a second write of the same key in one frame replaces the first), so the pending set is bounded by the key count and needs no queue policy.
3. A restart-required key is never staged: its value goes to PLAT's pending-restart table (§ "Restart-required keys").
4. Prologue step 3 (`Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Frame structure") calls `ApplyQueuedUserWrites()`: `CommitStagedUserValues` resolves the staged values; when `quality.tier` or `quality.activeTier` is among them, the quality-tier layer is reloaded in the same call (§ "Quality tier changes"); PLAT collects the changed keys from the store's user change registry and the tier reload, updates their chains, reports them through `GetChangedKeys()` for this frame, and dispatches its change registry once per changed key, on the main thread, in commit order (`Zenith2/Docs/Contracts.md` → "Threading and concurrency contract", "Callback lifetime").
5. The user file is saved per "Persisting the user layer".

### Quality tier changes

Two STRING keys select the tier, both user-layer writes and both excluded from layer 4:

| Key | Written by | Persisted | Meaning |
|---|---|---|---|
| `quality.tier` | the settings UI (the player's choice), and the platform and game layers | yes | the chosen tier |
| `quality.activeTier` | the runtime's `Zenith_ThermalAdaptationTarget` (`Zenith2/Docs/Design/PLAT/Android.md` → "Applying thermal actions") | never: the user-file writer omits it | the tier in effect while thermal adaptation holds a lower one; empty (the default) means "the chosen tier" |

A player's choice always wins over thermal adaptation's: when `quality.tier` is among the committed keys, `ApplyQueuedUserWrites` also clears `quality.activeTier` (it stages the empty string with source `"settings"` and commits it in the same call), so the new choice takes effect at once, and thermal adaptation re-bases from it at its next evaluation (`Zenith2/Docs/Design/PLAT/Android.md` → "Applying thermal actions").

When a commit changes either key, `ApplyQueuedUserWrites`:

1. Resolves the selection (`quality.activeTier` when non-empty, else `quality.tier`) and validates it against the tier list; an unknown tier keeps the current layer 4 and logs a warning.
2. Records the resolved value of every key that allows layer 4; `ClearLayer(CONFIG_LAYER_QUALITY_TIER)`; applies the selected tier's two files (a missing or corrupt file contributes nothing, as at startup).
3. Compares, and reports every key whose value changed with the user write's changes; no changed key is restart-required, because schema rule 2 keeps every restart-required key out of layer 4.
4. Presentation consumers (the renderer's quality settings, the dynamic-resolution controller) apply their changed values at their own next frame; no simulation-affecting key can change (the schema rule), so a tier change never affects simulation (Z2-D-019).

A thermal write made at prologue step 5 is committed at the next frame's step 3, one frame after it was made; thermal adaptation's step cadence (seconds) makes the frame irrelevant.

### OS accessibility preference changes

<!-- elaboration -->
The OS sub-source of layer 2 is the one runtime change that is not a user-layer write. `ApplyQueuedUserWrites` first compares the accessibility service's state revision with the one it last applied; when it differs, it applies each reported preference whose text differs from the layer-2 slot (`ApplyValueText(CONFIG_LAYER_PLATFORM, key, text, "os:accessibility", 0)` for `platform.accessibility.reducedMotion`, `.highContrast` and `.textScale`), then commits the staged user values as § "User-layer writes" describes, and reports every key whose resolved value changed, from either step, through `GetChangedKeys()` and one change callback each. The three keys are neither restart-required nor simulation-affecting, so the change reaches presentation consumers only (Z2-D-019), never the user file, and a `--set` of a key still wins. The Architecture text this extends is cross-document item X269 (assumption marked in `Zenith2/Docs/Design/PLAT/Accessibility.md` → "Preference keys and the platform layer").

### Restart-required keys

A restart-required key's effective value never changes during a run, and its pending value has one home:

- **Where it lives:** PLAT's pending-restart table, one entry per key (value, source, line), and nowhere else; the store is never written for it after `Create` (user writes are not staged, and schema rule 2 keeps tier reloads away from it), so its slots keep the values applied at startup.
- **`Get`** reads the store, and so returns the startup value for the whole run.
- **`IsPendingRestart(xKey)`** reads the table, so the settings UI can tell the player a restart is needed.
- **The user-file writer** merges the store's user slots with the table, the table winning for its keys, so the next launch applies the pending value as an ordinary user-layer value.

## Persisting the user layer

- The user file is `user:/config/user.config.json` in CORE's format, holding exactly the user layer: every key the player set (the pending-restart table's persisted entries included), plus every member present in the file when it was read that the running build does not register (unknown keys are kept verbatim, so a newer build's settings survive a run of an older one). `quality.activeTier` is never written.
- PLAT writes it with `Zenith_JsonWriter`: the store's user-layer slots (`GetLayerEntry(xKey, CONFIG_LAYER_USER)`) merged with the pending-restart table's persisted entries (the table wins) and the preserved members, sorted by key bytes, in CORE's byte format, so the file is byte-stable.
- It is written through `Zenith_AsyncIO::WriteAtomic` (`Zenith2/Docs/Design/PLAT/Filesystem.md` → "Atomic writes"), always at `IO_PRIORITY_LOW` so its writes land in submission order, after `config.userSaveDebounceMs` (1,000) without further changes, on entering the background, and at shutdown.
- On entering the background the runtime's `Zenith_AppLifecycleListener::OnEnterBackground` calls `SubmitUserLayer()` (`Zenith2/Docs/Design/PLAT/Android.md` → "Lifecycle state machine"), which submits the pending write and returns at once; the platform's background pump loop observes the operation's handle as it does the resume write's, so the main thread never waits on disk inside `Platform.Pump`. At shutdown `FlushUserLayer()` submits and waits on the main thread for the operation to complete.
- `FlushUserLayer` must run while `Zenith_AsyncIO` is alive, in Contracts step 8 before `Zenith_AsyncIO` is destroyed: the shutdown order is `Zenith2/Docs/Design/PLAT.md` → "Threads PLAT owns and shutdown". If the I/O system has already begun shutting down (`IO_ERROR_SHUTTING_DOWN`), it returns `CONFIG_SOURCE_ERROR_IO_UNAVAILABLE` and logs `config: user settings not saved: I/O shut down`; the previous file is intact, and the unsaved values are lost.
- A user value rejected on load (out of range after a range change) is preserved in the file until the player sets that key again, and is re-warned once per load.
- A write refused for low disk is retried at the next debounce expiry after `PLATFORM_EVENT_LOW_DISK_CLEARED`; the in-memory values stay in effect meanwhile.

## Corrupt configuration handling

| Source | Failure | Behaviour | Surfaced |
|---|---|---|---|
| a content file (platform, game, tier) | over the size cap, or `ApplyJsonLayer` returns `CONFIG_ERROR_PARSE` (not valid JSON, not an object, a parser limit) | the whole file contributes nothing; every key falls back to the lower layers; one warning with the file and the parser's position; `config.files_rejected` + 1 | the dump lists the file under `rejectedFiles`; in development builds the configuration panel highlights it |
| any file | an invalid value for one key | that value is rejected by the store; the rest of the file applies | the chain entry with its code |
| the user file | over the size cap or `CONFIG_ERROR_PARSE` | renamed to `user.config.json.corrupt-<UTC>` with CORE's compact UTC timestamp (`Zenith_FormatUtcTimestampCompact`; the newest three kept, older ones deleted); the user layer is empty; a fresh file is written at the next save | `PLATFORM_EVENT_CONFIG_RESET` with `CONFIG_LAYER_USER` and `CONFIG_RESET_REASON_USER_FILE_CORRUPT`, world-visible, so the game can tell the player their settings were reset |
| the user file | an invalid value for one key | rejected per key, preserved in the file | the chain entry |
| the tier list | absent or invalid | the only tier is the resolved `quality.tier` value (or `QUALITY_TIER_<PLATFORM>_DEFAULT`); thermal tier steps are disabled | a warning |

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_Config.h
enum Zenith_ConfigResetReason : uint8
{
	CONFIG_RESET_REASON_USER_FILE_CORRUPT = 1,
	CONFIG_RESET_REASON_INPUT_BINDINGS_CORRUPT,
};
```

The provenance of every value always names the source that actually supplied it, so after any of these fallbacks the dump shows why a key has its value. A corrupt content file never stops startup: the cook validated it, and falling back to the lower layers keeps the game running while the dump and the warning name the file.

## The configuration dump

`WriteDump(Zenith_JsonWriter&)` (Architecture's signature) writes the document below; `--config-dump <path>` writes it through a command-line host file after startup step 6 (`Zenith2/Docs/Design/PLAT/Filesystem.md` → "Command-line host files"); the on-screen profiler's configuration panel reads the same data (entry document → "On-screen profiler rows").

```json
{
	"schema": "zenith2.config-dump",
	"schemaVersion": 1,
	"activeQualityTier": "QUALITY_TIER_WIN_HIGH",
	"keys": { "…": "Zenith_ConfigStore::WriteDump output, unchanged" },
	"chains": {
		"rhi.backend": [
			{ "layer": "engineDefault", "source": "default", "line": 0, "value": "vulkan", "rejected": null, "winning": false },
			{ "layer": "platform", "source": "content:/Config/Platform/windows.config.json", "line": 4, "value": "vulkan", "rejected": null, "winning": false },
			{ "layer": "commandLine", "source": "command line", "line": 0, "value": "d3d12", "rejected": null, "winning": true }
		]
	},
	"pendingRestart": [ "platform.displayIndex" ],
	"unknownKeys": [ { "key": "render.shadowCascade", "source": "content:/Config/game.config.json", "line": 9 } ],
	"rejectedFiles": [],
	"inputBindings": [
		{ "context": "Gameplay", "action": "Gameplay.Jump", "binding": 3, "controls": ["/keyboard/space", "/gamepad/south"],
		  "chain": [ { "layer": "game", "source": "content:/Input/Game.bindings.json", "line": 27 } ] }
	]
}
```

- `keys` is the store's own dump (every key with value, type, layer, source, line, restart and simulation flags, in key byte order); PLAT does not restate its members.
- `chains` holds one array per key in key byte order, lowest layer first; `rejected` is the `Zenith_ToString` text of the code or null; layer names are the lower camel case of the enumerator.
- `inputBindings` comes from `Zenith2/Docs/Design/PLAT/Input.md` → "Binding layers and provenance", sorted by (context, action, binding ID).
- The document is byte-stable for identical inputs.

## Keys PLAT registers

<!-- elaboration -->
"Early" means engine default and command line only; "all but tier" means every layer except `CONFIG_LAYER_QUALITY_TIER`.

| Key | Type | Default | Range or values | Allowed layers | Restart |
|---|---|---|---|---|---|
| `platform.windowMode` | ENUM | `windowed` (Windows), `borderless` (Android) | `windowed`, `borderless`, `exclusive` | all but tier | no |
| `platform.windowWidth`, `platform.windowHeight` | INT | 1280, 720 | 320–16384 points | all but tier | no |
| `platform.refreshHz` | INT | 0 (the display's) | 0–480 | all but tier | no |
| `platform.displayIndex` | INT | 0 | 0–15 | all but tier | yes |
| `platform.eventQueueCapacity`, `platform.eventQueueHardCapacity` | INT | 4096, 16384 | 256–65536 | early | yes |
| `platform.clipboardMaxBytes` | INT | 1048576 | 4096–16777216 | engine default, platform, game | yes |
| `platform.resumeOnRelaunch` | BOOL | true | — | engine default, platform, game, command line | yes |
| `input.transitionCapacity` | INT | 8192 | 1024–65536 | early | yes |
| `input.carryCapacity` | INT | 1024 | 64–8192 | engine default, platform, command line | yes |
| `input.reconnectWindowSeconds` | INT | 30 | 0–600 | engine default, platform, game | no |
| `input.gamepadSensors` | BOOL | false | — | all but tier | no |
| `input.emulateTouchWithMouse` | BOOL | false | — (ignored in Shipping) | engine default, user, command line | no |
| `input.hapticsIntensity` | FLOAT | 1.0 | 0.0–1.0 | all but tier | no |
| `input.hapticsStopOnFocusLoss` | BOOL | true | — | engine default, platform, game | no |
| `input.lookSensitivity` | FLOAT | 1.0 | 0.05–10.0 | all but tier | no |
| `input.schemeSwitchThreshold`, `input.actuationThreshold` | FLOAT | 0.5, 0.1 | 0.01–1.0 | engine default, platform, game | no |
| `input.bindingsSaveDebounceMs` | INT | 1000 | 0–60000 | engine default, command line | no |
| `input.gesture.*` | INT or FLOAT | the table of `Zenith2/Docs/Design/PLAT/Input.md` → "Gesture thresholds" | 1–10000 (ms), 1–1000 (dp), 1–90 (degrees) | engine default, platform, game, user | no |
| `io.threadCount` | INT | 2 | 1–4 | engine default, platform, command line | yes |
| `io.queueCapacity` | INT | 1024 | 64–65536 | engine default, platform, command line | yes |
| `io.chunkBytes` | INT | 1048576 | 65536–16777216, a power of two | engine default, platform, command line | yes |
| `io.lowDiskMarginBytes` | INT | 67108864 | 1048576–4294967296 | engine default, platform, game, command line | no |
| `io.lowDiskPollSeconds`, `io.lowDiskClearFactor` | INT | 30, 2 | 1–3600, 1–16 | engine default, platform | no |
| `io.maxWriteBytes` | INT | 268435456 | 1048576–4294967296 | engine default, platform, game | yes |
| `vfs.resolveCacheEntries` | INT | 16384 | 0–1048576 | engine default, platform, command line | yes |
| `vfs.syncReadMaxBytes` | INT | 67108864 | 1048576–1073741824 | engine default, platform | yes |
| `config.userSaveDebounceMs` | INT | 1000 | 0–60000 | engine default, command line | no |
| `quality.tier` | STRING | `QUALITY_TIER_<PLATFORM>_DEFAULT` (`QUALITY_TIER_WIN_DEFAULT`, `QUALITY_TIER_S24_DEFAULT`) | a tier ID of the tier list | all but tier | no |
| `quality.activeTier` | STRING | empty | empty or a tier ID of the tier list | engine default, user | no |
| `thermal.drsHeadroom`, `thermal.tierHeadroom`, `thermal.recoverHeadroom` | FLOAT | 0.85, 0.95, 0.70 | 0.0–2.0 | engine default, platform, game, command line | no |
| `thermal.recoverSeconds` | INT | 30 | 1–600 | engine default, platform, game, command line | no |
| `thermal.drsStep` | FLOAT | 0.05 | 0.01–0.25 | engine default, platform, game | no |
| `thermal.tierCooldownSeconds` | INT | 10 | 0–600 | engine default, platform, game | no |
| `thermal.enabled` | BOOL | true | — | all but tier | no |
| `devtools.bindAddress` | STRING | `127.0.0.1` | an IPv4 or IPv6 literal | command line only | yes |
| `platform.accessibility.backend` | ENUM | `auto` | `auto`, `os`, `recording`, `none` (`Zenith2/Docs/Design/PLAT/Accessibility.md` → "Backend table") | early | yes |
| `platform.accessibility.reducedMotion`, `platform.accessibility.highContrast` | BOOL | false, false | — | engine default, platform, command line | no |
| `platform.accessibility.textScale` | FLOAT | 1.0 | 1.0–2.0 | engine default, platform, command line | no |

The thermal keys and `platform.resumeOnRelaunch` are named by `Zenith2/Docs/Architecture/PlatformLayer.md` → "Thermal adaptation" and "Process death and restore"; `io.queueCapacity` and `io.lowDiskMarginBytes` by "Asynchronous I/O" and "Low-disk behaviour" of that file. Every description string names the key's units.

## Zenith_Config API

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_Config.h
#include "ZenithCore/Zenith_ConfigStore.h"

/// @contract interface Zenith_ConfigSource
/// Ownership: Implemented by tests (memory sources) and passed in the desc; Zenith_Config's own file, command-line
///   and replay sources are internal.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: The bytes returned stay valid until Load is called again or the source is destroyed.
/// Units: Bytes of one layer file in CORE's format.
/// Errors: Load returns CONFIG_SOURCE_ERROR_READ for a source that cannot supply its bytes.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
class Zenith_ConfigSource
{
public:
	virtual ~Zenith_ConfigSource() = default;
	virtual Zenith_ConfigLayer GetLayer() const = 0;
	virtual Zenith_StringView GetDescription() const = 0;   // the provenance source string
	virtual Zenith_Result<Zenith_ByteSpan, Zenith_ConfigSourceError> Load() = 0;
};

struct Zenith_ConfigDesc
{
	const Zenith_ConfigSchema* m_pxSchema = nullptr;                   // frozen
	Zenith_VirtualFileSystem* m_pxFiles = nullptr;
	Zenith_AsyncIO* m_pxIO = nullptr;                                  // user-file saves
	Zenith_Platform* m_pxPlatform = nullptr;                           // PLATFORM_EVENT_CONFIG_RESET
	Zenith_Span<const Zenith_StringView> m_astrSetOptions;               // from Zenith_CommandLine::GetSetOptions
	Zenith_ByteSpan m_xReplaySimulationList;                           // --replay: the header's list; empty otherwise
	Zenith_StringView m_strReplaySource;                               // "replay:<file>"
	Zenith_BuildPlatform m_ePlatform;                                  // selects the platform and tier files
	Zenith_Span<Zenith_ConfigSource* const> m_apxTestSources;           // tests: replaces the file and command-line sources
	Zenith_Heap* m_pxHeap = nullptr;
	Zenith_FaultInjector* m_pxFaultInjector = nullptr;
};

/// @contract type Zenith_Config
/// Ownership: Created by Zenith_Config::Create in startup step 6, owned by Zenith_Runtime until after Contracts shutdown step 10;
///   owns its Zenith_ConfigStore, chains, pending-restart table and user-file state.
/// Thread affinity: MAIN_THREAD (Architecture's contract); values reach other threads through their owners' descs.
///   IMMUTABLE after Create except user-layer writes through SetUserValue, committed by ApplyQueuedUserWrites in the
///   frame prologue (a quality-tier change is such a write of quality.tier or quality.activeTier).
/// Invalidation: Views returned by Get (string values), GetProvenance and GetProvenanceChain are valid until the key's
///   value next changes in a prologue.
/// Units: Values carry the units of their key's description.
/// Errors: Create returns Zenith_ConfigSourceError for a schema rule, a required key or allocation; key operations
///   return the store's Zenith_ConfigError; invalid source data never fails a call: it is rejected per key with a
///   warning, and the lower layer stays in effect.
/// Reentrancy: ALLOWED(Get, GetStore, GetProvenance, GetProvenanceChain, IsPendingRestart) from inside change
///   callbacks; every other member follows NONE.
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
class Zenith_Config
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_Config>, Zenith_ConfigSourceError> Create(const Zenith_ConfigDesc& xDesc);

	// Architecture's members.
	const Zenith_ConfigStore& GetStore() const;
	Zenith_Result<Zenith_ConfigKey, Zenith_ConfigError> FindKey(Zenith_StringView strKey) const;
	const Zenith_ConfigValue& Get(Zenith_ConfigKey xKey) const;
	Zenith_ConfigProvenance GetProvenance(Zenith_ConfigKey xKey) const;
	Zenith_Result<void, Zenith_ConfigError> SetUserValue(Zenith_ConfigKey xKey, const Zenith_ConfigValue& xValue);   // source "settings"
	Zenith_Result<void, Zenith_JsonWriteError> WriteDump(Zenith_JsonWriter& xWriter) const;

	// Added by this design.
	Zenith_Result<void, Zenith_ConfigError> SetUserValue(Zenith_ConfigKey xKey, const Zenith_ConfigValue& xValue,
		Zenith_StringView strSource);                                                       // the chain's source for this write
	Zenith_Span<const Zenith_ConfigProvenanceEntry> GetProvenanceChain(Zenith_ConfigKey xKey) const;
	bool IsPendingRestart(Zenith_ConfigKey xKey) const;
	void ApplyQueuedUserWrites();                                                          // prologue step 3
	Zenith_Span<const Zenith_ConfigKey> GetChangedKeys() const;            // changed at this prologue
	Zenith_CallbackRegistry<void(Zenith_ConfigKey)>& GetChangeRegistry();   // user commits and tier changes
	Zenith_StringView GetActiveQualityTier() const;                         // the tier layer 4 was loaded for
	Zenith_Span<const Zenith_StringView> GetQualityTiers() const;          // highest first
	void SubmitUserLayer();                                                               // background entry; never waits
	Zenith_Result<void, Zenith_ConfigSourceError> FlushUserLayer();        // shutdown; waits
};
```

`Zenith_JsonWriter`, `Zenith_CallbackRegistry` and `Zenith_BuildPlatform` are ZenithCore's (`Zenith2/Docs/Design/CORE.md` → "Public API").

## Configuration error enumerations

Key-level errors are CORE's `Zenith_ConfigError` (`Zenith2/Docs/Design/CORE/Diagnostics.md` → "Configuration store"). The facade's own failures, which concern sources rather than keys, have their own enumeration:

```cpp
// Zenith2/Engine/ZenithPlatform/Zenith_Config.h
enum Zenith_ConfigSourceError : uint8
{
	CONFIG_SOURCE_ERROR_OUT_OF_MEMORY = 1,
	CONFIG_SOURCE_ERROR_BAD_DESC,               // a null schema, an unfrozen schema, or a missing file system
	CONFIG_SOURCE_ERROR_SCHEMA_RULE,            // a simulation-affecting or quality.tier key breaks PLAT's schema rules
	CONFIG_SOURCE_ERROR_REQUIRED_KEY_MISSING,   // ValidateRequired failed; the key is logged
	CONFIG_SOURCE_ERROR_READ,                   // a test source could not supply its bytes
	CONFIG_SOURCE_ERROR_UNKNOWN_TIER,
	CONFIG_SOURCE_ERROR_IO_UNAVAILABLE,         // FlushUserLayer after Zenith_AsyncIO began shutting down
};
constexpr const char* Zenith_ToString(Zenith_ConfigSourceError eError);
```

The runtime maps every `Create` failure to `RUNTIME_ERROR_CONFIGURATION` (exit code 2, `Zenith2/Docs/Architecture/CompositionRoot.md` → "Exit codes") except `CONFIG_SOURCE_ERROR_OUT_OF_MEMORY`, which follows the runtime's allocation-failure mapping.

## Configuration tests

Tests live in `Zenith2/Engine/ZenithPlatform/Tests/Zenith_Config_Tests.cpp`; they use memory sources (`m_apxTestSources`) over a test schema and run in the `Test` step on every host preset. The first three are `Zenith2/Docs/Architecture/PlatformLayer.md` → "Required tests" rows; the store's own behaviour (per-key validation, the JSON parser, the store dump, the simulation list's bytes) is tested by CORE (`Zenith2/Docs/Design/CORE/Diagnostics.md` → "Diagnostics tests").

| Test | Asserts |
|---|---|
| `Test_Config_LayerPrecedence` | each layer's source overrides the one below through the facade; provenance names the winning layer, file and line |
| `Test_Config_DisallowedLayerIgnored` | a key set in a disallowed layer's file keeps the lower value, logs a warning naming key, source and line, and appears in the chain with `CONFIG_ERROR_LAYER_NOT_ALLOWED` |
| `Test_Config_DumpContainsProvenance` | the dump carries the store's keys, a chain per key, pending-restart keys, unknown keys, rejected files and binding provenance, byte-stable across two runs |
| `Test_Config_TierSubSourceOrderGameOverEngine` | within layer 4 the game tier file overrides the engine tier file per key |
| `Test_Config_SchemaRulesEnforced` | a simulation-affecting key that is not restart-required, a restart-required key that allows layer 4, and a `quality.tier` or `quality.activeTier` that allows layer 4, each fail `Create` with `CONFIG_SOURCE_ERROR_SCHEMA_RULE` naming the key |
| `Test_Config_EarlyKeysRejectFileLayers` | an early key set in the platform, game or user file is rejected with `CONFIG_ERROR_LAYER_NOT_ALLOWED`; set on the command line it applies, and the desc value used at step 4 equals the configured value |
| `Test_Config_PlatKeysRegistered` | `Zenith_RegisterPlatformConfigKeys` registers every key of "Keys PLAT registers" with the listed type, default, range, layers and restart flag |
| `Test_Config_EachRejectionInChain` | one fixture per row of "Rejection reasons" produces its chain entry and warning |
| `Test_Config_QualityTierTwoPass` | `quality.tier` from the user layer selects the tier files; an unknown tier falls back with a warning; `quality.tier` inside a tier file is rejected |
| `Test_Config_UnknownKeysWarnedWithSource` | every unknown key is warned exactly once with source and line and listed in the dump |
| `Test_Config_RequiredKeyMissingFailsCreate` | a required key no layer supplies fails `Create` with `CONFIG_SOURCE_ERROR_REQUIRED_KEY_MISSING`, naming the key |
| `Test_Config_ReplayListOverridesSet` | in replay mode the replay list wins over `--set`, the chain marks the override, and a warning names the key |
| `Test_Config_UserWriteAppliedAtPrologue` | `SetUserValue` changes nothing until `ApplyQueuedUserWrites`; then the value, the chain, `GetChangedKeys` and one callback each change |
| `Test_Config_UserWriteSameKeyTwiceInFrame` | two writes of one key before a prologue commit only the second, with one change callback |
| `Test_Config_RestartRequiredPending` | a restart-required user write leaves the store's user slot unchanged (`GetLayerEntry` returns the startup entry), `Get` keeps the startup value, `IsPendingRestart` is true, the saved file holds the new value, and a fresh `Create` over it applies it |
| `Test_Config_FlushBeforeIOShutdown` | a pending user write flushed by `FlushUserLayer` before `Zenith_AsyncIO` is destroyed lands on disk; after the I/O system began shutting down the flush returns `CONFIG_SOURCE_ERROR_IO_UNAVAILABLE`, logs, and the old file is intact |
| `Test_Config_TierChangeReresolves` | a user write of `quality.tier`, and one of `quality.activeTier`, each reload exactly the layer-4 keys at the next `ApplyQueuedUserWrites`; `Get` of every restart-required key and every simulation key is unchanged across the reload; `quality.activeTier` never reaches the user file, and its chain entry carries the source `thermal` |
| `Test_Config_PlayerTierChangeClearsActiveTier` | with `quality.activeTier` set by the thermal target, a player write of `quality.tier` commits the new tier and clears `quality.activeTier` in the same `ApplyQueuedUserWrites`, so layer 4 is the player's new tier at that prologue |
| `Test_Config_BackgroundSubmitDoesNotWait` | `SubmitUserLayer` returns before the write completes (a fake volume that stalls the write), and the write lands when the stall ends |
| `Test_Config_UserFilePersistedDebouncedAtomic` | three writes within the debounce produce one `WriteAtomic`; the file is sorted and byte-stable; unknown members survive |
| `Test_Config_UserFileLowDiskRetried` | a low-disk refusal keeps the values in memory and retries after the cleared event |
| `Test_Config_CorruptUserFileQuarantined` | the file is renamed with the timestamp (three kept), defaults apply, and `PLATFORM_EVENT_CONFIG_RESET` is raised with the user layer and reason |
| `Test_Config_CorruptContentFileIgnoredWhole` | a truncated game file contributes nothing; the dump lists it; other layers apply; `Create` succeeds |
| `Test_Config_OversizedFileRejected` | a content file over 1 MiB and a user file over 256 KiB are treated as corrupt without being parsed |
| `Test_Config_Create_FailsAtEachStep_Unwinds` | the fault sweep over store creation, source reading and chain building |
| `Test_Config_OsAccessibilityPreferencesInPlatformLayer` | with a recording accessibility service reporting reduced motion, high contrast and text scale 1.5, `Create` resolves the three keys from the platform layer with source `os:accessibility` over a cooked platform file setting them otherwise; an unreported preference keeps the cooked value; a `--set` wins; the user file never holds them |
| `Test_Config_OsAccessibilityChangeAppliedAtPrologue` | a test-access preference change reaches the store only at the next `ApplyQueuedUserWrites`, lists exactly the changed keys in `GetChangedKeys`, dispatches one callback per key, and an unchanged revision applies nothing |
