# VFX design: the effect document and the cooked effect format

Section file of `Zenith2/Docs/Design/VFX.md`. It specifies the effect authoring document (`<Name>.vfx.json`, schema version 1) with every key, value form, module and limit; the closed module catalogue and the constants each module bakes; curve baking; the cooked effect file (`.zvfx`, magic `ZVFX`, format version 1) section by section with byte layouts; the effect constants image both the CPU and the GPU read; the reader's validation; and the document's migration. Effects are authored in code, with previews (Z2-D-079): the document is reviewable text naming built-in modules and optional Slang types, never a node graph.

## Scope of this file

| Topic | Heading |
|---|---|
| the authoring document, key by key | "Effect document" |
| every module, its identifier, its targets and its baked constants | "Module catalogue" |
| how curves become lookup tables | "Curve baking" |
| the cooked file | "Cooked effect format" |
| the constants image read by both simulators | "Effect constants image" |
| schema versions and migration | "Effect document migration" |

## Effect document

An effect document is an authoring document (Z2-D-038, Z2-D-048): UTF-8 JSON, read with `Zenith_JsonDocument` under `Zenith_JsonLimits` with duplicate keys refused, written by the editor through SCENE's canonical serialization and journaled write (`Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Canonical serialization"`), preserving unknown keys with `MIGRATION_WARNING_UNKNOWN_FIELD` ([Z2-N-CONTRACT-543]). Engine effects live under `Zenith2/Engine/ZenithVFX/Content/Effects/`, a game's under `Zenith2/Games/<Name>/Assets/VFX/`.

### Top-level keys

<!-- elaboration -->
| Key | JSON type | Required | Default | Rule |
|---|---|---|---|---|
| `schemaVersion` | integer | yes | — | 1 |
| `id` | string, persistent-ID form ([Z2-N-CONTRACT-067]) | yes | — | the effect's GUID; equals the cook item's GUID |
| `duration` | number | no | 0 | seconds of the effect's spawn timeline, 0 … 3,600; 0 is endless (emitters spawn until stopped) |
| `loop` | boolean | no | false | true wraps the timeline every `duration` seconds; true with `duration` 0 is `VFX_COOK_ERROR_SCHEMA` |
| `priority` | number | no | 1 | 0 … 16; multiplies the significance ("LOD and significance" of `Zenith2/Docs/Design/VFX/Simulation.md`) |
| `seed` | integer | no | 0 | 0 … 4,294,967,295; non-zero fixes the instance seed for every instance (a deterministic look for authored set pieces); 0 lets each instance derive its own |
| `cullBehaviour` | string | no | `kill` | `kill` or `pause`; an emitter may override it |
| `bounds` | object | no | `{ "mode": "baked" }` | `{ "mode": "baked" or "fixed", "min": [x, y, z], "max": [x, y, z], "bakeSeconds": number, "bakeSeeds": integer, "margin": number }`: `min`/`max` are required with `fixed` and refused with `baked`; `bakeSeconds` 0.5 … 30 (default 4, or `duration` + the longest lifetime when that is shorter), `bakeSeeds` 1 … 32 (default 8), `margin` 0 … 1 (default 0.1, a fraction of each half extent added on every side) |
| `lod` | object | no | one level at screen size 0 | `{ "levels": [ { "screenSize": number, "spawnScale": number, "disable": [string] } ], "cullScreenSize": number, "maxDistance": number, "hysteresis": number }`: 1 … 4 levels with strictly decreasing `screenSize` in (0, 10], `spawnScale` 0 … 1 (default 1), `disable` a list of `"<emitter>/<update module>"` names; `cullScreenSize` 0 … 1 (default 0.002); `maxDistance` ≥ 0 metres (default 0, no limit); `hysteresis` 0 … 0.5 (default 0.1) |
| `parameters` | array | no | none | at most 8 floats in total: `{ "name": string, "type": "float" or "float2" or "float3" or "colour", "default": number or array }`; a name matches `^[A-Za-z][A-Za-z0-9_]{0,31}$` and is unique; slots are assigned in array order |
| `emitters` | array | yes | — | 1 … 16 emitters ("Emitter keys") |
| `preview` | object | no | none | `{ "backdrop": "grid" or "sky" or "black", "environment": asset GUID, "cameraDistance": number, "loopPreview": boolean }`, read by the editor's preview alone |

### Emitter keys

| Key | JSON type | Required | Default | Rule |
|---|---|---|---|---|
| `name` | string | yes | — | `^[A-Za-z][A-Za-z0-9_]{0,31}$`, unique in the effect |
| `target` | string | no | `gpu` | `gpu` or `cpu` |
| `space` | string | no | `world` | `world`: particles keep their world position when the emitter moves; `local`: particles move with the emitter |
| `capacity` | integer or `"auto"` | no | `"auto"` | 64 … 262,144 (`gpu`) or 64 … 4,096 (`cpu`), a multiple of 64, and a power of two for an event target whose source is itself an event target; `"auto"` is sized by the cook ("Capacity sizing" of `Zenith2/Docs/Design/VFX/Authoring.md`) |
| `duration` | number | no | the effect's | seconds the emitter spawns per timeline cycle, measured from `startDelay` |
| `startDelay` | number | no | 0 | seconds of the timeline before the emitter's first spawn |
| `seedOffset` | integer | no | the emitter's index | mixed into the emitter seed instead of the index, so reordering emitters keeps their looks |
| `cullBehaviour` | string | no | the effect's | `kill` or `pause` |
| `spawn` | array | yes | — | spawn modules ("Spawn modules"); an `event` module excludes every other spawn module |
| `init` | array | no | none | initialisation modules ("Initialisation modules"), each kind at most once |
| `update` | array | no | none | update modules ("Update modules"), each kind at most once |
| `life` | array | no | none | over-life modules evaluated at draw time ("Update modules", table "Over-life modules") |
| `render` | object | no | none | one render module ("Render modules"); absent: the emitter is simulated and never drawn |
| `light` | object | no | none | the emitter's light ("Light and decal modules") |
| `decal` | object | no | none | the emitter's decal ("Light and decal modules") |
| `updateType` | object | no | none | `{ "module": repo-relative .slang path, "type": identifier, "parameters": object }`: a code-authored `Flux_ParticleUpdateType`; `gpu` only |
| `prewarm` | number | no | 0 | seconds, 0 … 2: on start, the rate spawns of that interval are created already aged ("Prewarm" of `Zenith2/Docs/Design/VFX/Simulation.md`) |

### Value forms

Every numeric module parameter takes one of these forms; a parameter's row in the module tables states which forms it admits (`C` constant, `R` random, `K` curve, `RK` random curve, `P` parameter binding).

| Form | JSON | Meaning | Cooked kind (`Flux_ParticleValue::m_uKind`) |
|---|---|---|---|
| constant | a number, or an array of 2–4 numbers | the value | 0 `CONST` |
| random | `{ "random": [a, b] }`, a and b numbers or equal-length arrays | per particle, per component: `a + (b − a) × u`, u a fresh draw per component | 1 `RANDOM` |
| uniform random | `{ "randomUniform": [a, b] }` | one draw u shared by every component (keeps a size's aspect) | 2 `RANDOM_UNIFORM` |
| curve | `{ "curve": [[t, v], …], "interp": "linear" or "smooth", "scale": number, "offset": number }` | `Curve(x) × scale + offset`, x the module's input in [0, 1] (normalised age for over-life modules, the emitter's normalised timeline for spawn rate); 2 … 16 keys, t strictly increasing in [0, 1], first key at 0 and last at 1 | 3 `CURVE` |
| random curve | `{ "randomCurve": { "min": keys, "max": keys }, "interp": …, "scale": …, "offset": … }` | `lerp(Min(x), Max(x), u) × scale + offset`, u one draw per particle | 4 `RANDOM_CURVE` |
| gradient | `{ "gradient": [[t, r, g, b, a], …], "interp": "linear" or "smooth" }` | a colour curve (4 channels) | 3 `CURVE` |
| parameter binding | `{ "param": name, "scale": number, "offset": number }` | `parameter × scale + offset`, the parameter's slots in order | 5 `PARAM` |

Colours are linear Rec.709 RGBA; an `intensity` multiplier carries HDR brightness so that colours stay in [0, 1] for authoring. Angles in documents are degrees and are converted to radians by the cook; times are seconds, distances metres, rates per second.

### Spawn modules

| Module (`"module"`) | Keys (forms) | Rule |
|---|---|---|
| `rate` | `rate` (C, K over the emitter's normalised timeline, P) | particles per second, 0 … 100,000 |
| `burst` | `time` (C), `count` (C, R integers), `cycles` (C integer, 0 = every timeline cycle), `interval` (C) | a burst list: up to 16 `burst` modules per emitter, each a row of the effect's burst table |
| `distance` | `spacing` (C, metres, > 0.001), `maxPerFrame` (C integer, default the capacity) | one particle per `spacing` metres the emitter's world origin travels |
| `event` | `source` (emitter name), `on` (`death` or `collision`), `count` (1 … 16), `maxCollisions` (1 … 3, `collision` only), `inheritVelocity` (0 … 1) | children of another emitter's particles; the source and target must share a target (`gpu` or `cpu`) and the event graph must be acyclic |

### Initialisation modules

| Module | Keys (forms) | Rule |
|---|---|---|
| `shape` | `shape` (`point`, `sphere`, `hemisphere`, `box`, `cone`, `circle`, `line`), `radius` (C), `radiusThickness` (C, 0 = surface … 1 = volume), `arc` (C, degrees, (0, 360]), `extents` (C, 3 numbers, box half extents), `angle` (C, degrees, cone half angle 0 … 90), `length` (C, cone height or line length), `emitFrom` (`base` or `volume`, cone), `alignToDirection` (boolean) | the spawn position and the shape direction in emitter space ("Initialisation algorithms" of `Zenith2/Docs/Design/VFX/Simulation.md`) |
| `velocity` | `speed` (C, R, P), `direction` (`shape` or `fixed`), `vector` (C, 3, with `fixed`), `randomVector` (R, 3, additive), `radial` (C, R: outward speed from the emitter origin) | m/s in emitter space |
| `lifetime` | `seconds` (C, R, P) | 0.01 … 60 |
| `size` | `size` (C, R, RANDOM_UNIFORM, 1 or 2 components; one component is square), | metres, 0.0001 … 1,000 |
| `rotation` | `angle` (C, R, degrees), `rate` (C, R, degrees per second) | |
| `colour` | `colour` (C, R, gradient sampled at a random t), `intensity` (C, R, P, 0 … 1,000) | the spawn colour × intensity in rgb |
| `subUVStart` | `random` (boolean) | a random start frame in [0, frames) |
| `inheritVelocity` | `factor` (C, 0 … 2) | adds `factor ×` the emitter's world velocity |

### Update modules

Update modules run in this fixed order, whatever their order in the document, and each kind appears at most once, so its index is its bit in the module mask:

| Index | Module | Keys (forms) | Targets |
|---|---|---|---|
| 0 | `gravity` | `scale` (C, P; default 1): `(0, −9.80665, 0) × scale` m/s² in world space | cpu, gpu |
| 1 | `force` | `acceleration` (C, R, P, 3 components), `space` (`world` or `emitter`) | cpu, gpu |
| 2 | `drag` | `coefficient` (C, P, ≥ 0, per second): `v ← v / (1 + coefficient × dt)` | cpu, gpu |
| 3 | `turbulence` | `strength` (C, P, m/s²), `frequency` (C, per metre), `scrollSpeed` (C, m/s along +Y), `octaves` (1 … 3) | cpu, gpu |
| 4 | `vortex` | `axis` (C, 3, emitter space, normalised by the cook), `centre` (C, 3), `strength` (C, P, rad/s), `pull` (C, m/s² towards the axis) | cpu, gpu |
| 5 | `attractor` | `position` (C, 3, emitter space), `strength` (C, P, m/s²), `radius` (C, metres; 0 = unlimited), `killRadius` (C, metres) | cpu, gpu |
| 6 | `wind` | `factor` (C, P, 0 … 4): adds `factor × Env_WindSample(position) − velocity` scaled by dt, a relaxation towards the wind | gpu |
| 7 | `speedLimit` | `maxSpeed` (C, P, m/s), `dampen` (C, 0 … 1 of the excess removed per second, 1 = clamp) | cpu, gpu |
| 8 | `collision` | `colliders` (boolean, default true), `depth` (boolean, default false; gpu, world space only), `mask` (integer), `restitution` (C, 0 … 1), `friction` (C, 0 … 1), `radiusScale` (C, 0 … 2), `depthThickness` (C, metres, default 0.5), `kill` (boolean), `killAfter` (C integer collisions, default 0 = never) | cpu (colliders), gpu (colliders, depth) |
| 9 | `killVolume` | `shape` (`sphere` or `box`), `centre` (C, 3, emitter space), `radius` (C), `extents` (C, 3), `invert` (boolean: kill outside) | cpu, gpu |
| 10 | `trail` | `samples` (2 … 8), `interval` (C, seconds between samples, ≥ 1/240) | cpu, gpu |
| 11 | `precipitationOcclusion` | none: kills a particle under cover (Stage 5, with the Environment module's occlusion capture) | gpu |
| 12 | `custom` | implied by `updateType`; runs after every built-in module | gpu |

| Over-life module (`life` array) | Keys (forms) | Evaluated |
|---|---|---|
| `colourOverLife` | `colour` (gradient, K over normalised age) | at draw: multiplies the spawn colour |
| `sizeOverLife` | `size` (K, 1 or 2 components) | at draw: multiplies the spawn size |
| `subUV` | `columns`, `rows` (1 … 16), `frames` (1 … columns × rows, at most 256), `mode` (`overLife` or `fps`), `fps` (C), `loops` (C, `overLife` repetitions), `blend` (boolean: blends to the next frame) | at draw |
| `colourBySpeed` | `colour` (gradient), `speedRange` (C, 2 numbers) | at draw: multiplies by the gradient at `saturate((|v| − min) / (max − min))` |
| `sizeBySpeed` | `size` (K), `speedRange` (C, 2) | at draw |

### Render modules

| Module | Keys | Rule |
|---|---|---|
| `sprite` | `texture` (texture asset GUID; absent: white), `blend` (`alpha`, `premultiplied`, `additive`; default `alpha`), `sort` (`depth`, `oldestFirst`, `newestFirst`; default `depth` for alpha and premultiplied, `oldestFirst` for additive), `facing` (`cameraPlane`, `cameraPosition`, `velocity`, `axis`, `world`; default `cameraPlane`), `axis` (3 numbers, `axis` and `world` facing), `stretch` (C, seconds: `velocity` facing lengthens the quad by `|v| × stretch`), `pivot` (2 numbers in [−1, 1]), `lighting` (`unlit`, `vertex`, `pixel`), `softDistance` (metres, 0 = off), `sortBias` (metres), `emissive` (C, 0 … 1,000), `shading` (`{ "module", "type", "parameters" }`: a code-authored `Flux_ParticleShadingType`) | `depth` sort with `additive` is `VFX_COOK_ERROR_SCHEMA` |
| `ribbon` | `texture`, `blend`, `facing` (`cameraPlane` or `axis`), `axis`, `uvMode` (`stretch`: u from 0 at the oldest to 1 at the newest; `tile`: u = distance from the newest / `tileLength`), `tileLength` (metres), `lighting`, `softDistance`, `emissive`, `tailFade` (boolean), `shading` | width from each particle's size x; `sort` is ignored (spawn order) |
| `mesh` | `mesh` (mesh asset GUID, required), `texture`, `blend`, `sort`, `alignToVelocity` (boolean), `lighting` (`unlit`, `vertex`, `pixel`), `emissive`, `shading` | the mesh's LOD 0 section 0, scaled by the particle size x on every axis (size y ignored) |
| `trail` | `texture`, `blend`, `lighting`, `width` (K over the sample index from newest 0 to oldest 1), `alpha` (K), `emissive`, `shading` | requires the `trail` update module |

### Light and decal modules

| Module | Keys | Rule |
|---|---|---|
| `light` | `colour` (C, 3, linear), `intensity` (C, K over the emitter's normalised timeline, P; candela, LIGHT's stored point-light unit), `range` (C, metres, > 0), `offset` (C, 3, emitter space), `sourceRadius` (C, metres), `channels` (integer, lighting-channel bits, default 1) | one point light per emitter instance while it spawns or has live particles; never shadow-casting |
| `decal` | `material` (a `decal`-domain material asset GUID), `size` (C, 3, metres), `at` (`start`, `stop`, `time`), `time` (C, with `time`), `lifetime` (C, seconds, > 0), `fadeOut` (C, seconds ≤ lifetime), `projection` (`down` or `emitterForward`), `random` (boolean: rotate about the projection axis by a random angle) | one decal per trigger per emitter instance, placed at the emitter origin |

### LOD keys

The effect's `lod` object (table "Top-level keys") is the only LOD authoring. Level i applies while the effect's screen size is at or above level i's `screenSize` and below level i − 1's; below the last level and down to `cullScreenSize` the last level applies; below `cullScreenSize`, or farther than `maxDistance`, the instance is culled. `disable` names update modules (`"Sparks/collision"`) that level turns off; render modules cannot be disabled, but `spawnScale` 0 stops a costly emitter's spawning entirely at that level.

### Event edges

An `event` spawn module creates one edge: (source emitter, target emitter, `on`, `count`, `maxCollisions`, `inheritVelocity`). The edges of an effect form a directed acyclic graph of at most 16 edges ("the effect graph"); a source has at most two outgoing edges (one per event kind), a target exactly one incoming edge, and an edge's source and target share their target and space. The cook orders emitters so that every source precedes its targets in the emitter table, which is also the order the GPU batches dispatch in.

### Document limits

| Quantity | Limit | Cook error beyond it |
|---|---|---|
| emitters per effect | 16 | `VFX_COOK_ERROR_LIMIT` |
| spawn modules per emitter | 18 (rate, distance and 16 bursts) | `VFX_COOK_ERROR_LIMIT` |
| update modules per emitter | 13 kinds, each once | `VFX_COOK_ERROR_SCHEMA` (a duplicate kind) |
| modules of every stage per emitter | 47 (`uFLUX_PARTICLE_MAX_MODULES`: 18 spawn, 8 initialisation, 13 update, 5 over-life, 1 render, 1 light, 1 decal) | `VFX_COOK_ERROR_LIMIT` |
| event records of a source emitter, `C_src × E` | 262,144 (`uFLUX_PARTICLE_MAX_EVENT_RECORDS`) | `VFX_COOK_ERROR_LIMIT` |
| curves per effect (after deduplication of identical sample tables) | 64 | `VFX_COOK_ERROR_LIMIT` |
| keys per curve | 16 | `VFX_COOK_ERROR_LIMIT` |
| user parameter floats | 8 | `VFX_COOK_ERROR_LIMIT` |
| LOD levels | 4 | `VFX_COOK_ERROR_LIMIT` |
| event edges | 16 | `VFX_COOK_ERROR_LIMIT` |
| distinct textures, meshes, decal materials | 8, 4, 4 | `VFX_COOK_ERROR_LIMIT` |
| an emitter's constants block | 4,096 bytes | `VFX_COOK_ERROR_LIMIT` |
| a code-authored type's `Params` | 256 bytes | `VFX_COOK_ERROR_CUSTOM_TYPE` |
| document size | 1 MiB | `VFX_COOK_ERROR_SCHEMA` (the JSON reader's limit) |

### Document validation

The effect cook step validates in this order and reports every error of a document in one pass (`Zenith2/Docs/Design/VFX/Authoring.md → "Effect cook step"`):

| # | Check | Error (`Zenith_VFXCookErrorCode`) |
|---|---|---|
| 1 | JSON syntax, `schemaVersion`, required keys, key types, enumeration spellings, numeric ranges, name patterns and uniqueness | `VFX_COOK_ERROR_SCHEMA` with line and column |
| 2 | every module name is in the catalogue; every module key is a key of that module | `VFX_COOK_ERROR_UNKNOWN_MODULE`, `VFX_COOK_ERROR_SCHEMA` |
| 3 | every module is available for the emitter's target (table "Module support by target") | `VFX_COOK_ERROR_TARGET_MODULE` |
| 4 | the value forms each parameter admits; a `param` binding names a declared parameter with matching channel count | `VFX_COOK_ERROR_VALUE_FORM`, `VFX_COOK_ERROR_UNKNOWN_PARAMETER` |
| 5 | the event graph: sources exist, acyclic, one incoming edge per target, shared target and space; an explicit `capacity` of a target whose source is itself an event target is a power of two | `VFX_COOK_ERROR_EVENT_GRAPH` |
| 6 | asset references: each texture, mesh and decal material resolves in the database to the right kind (a decal material's domain is `decal`); a mesh's raster stream-set is of the MESH family | `VFX_COOK_ERROR_REFERENCE` |
| 7 | code-authored types: the module's reflection exists, the type implements the interface, its `Params` fit 256 bytes, every `parameters` key matches a reflected field | `VFX_COOK_ERROR_CUSTOM_TYPE` |
| 8 | the limits of "Document limits" | `VFX_COOK_ERROR_LIMIT` |
| 9 | the bounds bake (baked mode) completes within its budget of 10⁸ particle-steps per effect | `VFX_COOK_ERROR_BOUNDS_BAKE` |

`Zenith_VFXCookErrorCode` (`Zenith2/Tools/ZenithCook/Internal/VFX/Zenith_VFXCookErrors.h`, values from 1 in this order: `SCHEMA`, `UNKNOWN_MODULE`, `TARGET_MODULE`, `VALUE_FORM`, `UNKNOWN_PARAMETER`, `EVENT_GRAPH`, `REFERENCE`, `CUSTOM_TYPE`, `LIMIT`, `BOUNDS_BAKE`) is reported as ASSET's `COOK_ERROR_STEP_FAILED` with the VFX code in `Zenith_CookError::m_uDetail` (`Zenith2/Docs/Design/ASSET/Cook.md → "Cook errors"`), each naming the document, the JSON pointer of the offending key and the expected form. Warnings (codes 1000 + index: `CURVE_RESOLUTION`, `CAPACITY_OVERFLOW`, `UNUSED_PARAMETER`, `SORT_COST`, `BURST_SPACING`, `BOUNDS_ESTIMATE`, appended in that order, so `BOUNDS_ESTIMATE` is 1005) never fail the cook; `BOUNDS_ESTIMATE` names an emitter whose composed bounds are an estimate rather than a proven bound (the estimate class of "Bounds" in `Zenith2/Docs/Design/VFX/Simulation.md`).

### Example effect document

```json
{
	"schemaVersion": 1,
	"id": "8d0f3c1e-5a52-4f0e-9e4b-6b1d2f0a7c11",
	"duration": 1.5,
	"loop": false,
	"priority": 2,
	"parameters": [ { "name": "Intensity", "type": "float", "default": 1.0 } ],
	"lod": { "levels": [ { "screenSize": 0.2 }, { "screenSize": 0.05, "spawnScale": 0.5, "disable": ["Sparks/collision"] } ], "cullScreenSize": 0.004 },
	"emitters": [
		{
			"name": "Sparks",
			"target": "gpu",
			"spawn": [ { "module": "burst", "time": 0.0, "count": { "random": [180, 240] } } ],
			"init": [
				{ "module": "shape", "shape": "sphere", "radius": 0.1, "radiusThickness": 1.0 },
				{ "module": "velocity", "speed": { "random": [4.0, 9.0] }, "direction": "shape" },
				{ "module": "lifetime", "seconds": { "random": [0.4, 0.9] } },
				{ "module": "size", "size": { "random": [0.02, 0.04] } },
				{ "module": "colour", "colour": [1.0, 0.62, 0.25, 1.0], "intensity": { "param": "Intensity", "scale": 40.0, "offset": 0.0 } }
			],
			"update": [ { "module": "gravity" }, { "module": "drag", "coefficient": 1.5 },
			            { "module": "collision", "depth": true, "restitution": 0.35, "friction": 0.2 } ],
			"life": [ { "module": "colourOverLife", "colour": { "gradient": [[0, 1, 1, 1, 1], [1, 1, 0.3, 0.1, 0]] } } ],
			"render": { "module": "sprite", "texture": "41c2…", "blend": "additive", "facing": "velocity", "stretch": 0.03 }
		},
		{
			"name": "Smoke",
			"target": "gpu",
			"spawn": [ { "module": "rate", "rate": { "curve": [[0, 60], [0.3, 20], [1, 0]] } } ],
			"init": [ { "module": "shape", "shape": "hemisphere", "radius": 0.3 }, { "module": "lifetime", "seconds": { "random": [1.5, 2.5] } },
			          { "module": "size", "size": { "random": [0.4, 0.7] } }, { "module": "rotation", "angle": { "random": [0, 360] }, "rate": { "random": [-30, 30] } } ],
			"update": [ { "module": "force", "acceleration": [0, 0.8, 0] }, { "module": "drag", "coefficient": 0.8 },
			            { "module": "turbulence", "strength": 0.6, "frequency": 0.7, "scrollSpeed": 0.4 } ],
			"life": [ { "module": "sizeOverLife", "size": { "curve": [[0, 0.5], [1, 2.5]], "interp": "smooth" } },
			          { "module": "colourOverLife", "colour": { "gradient": [[0, 0.3, 0.3, 0.3, 0], [0.1, 0.3, 0.3, 0.3, 0.6], [1, 0.2, 0.2, 0.2, 0]] } },
			          { "module": "subUV", "columns": 8, "rows": 8, "frames": 64, "mode": "overLife", "blend": true } ],
			"render": { "module": "sprite", "texture": "9e27…", "blend": "alpha", "sort": "depth", "lighting": "vertex", "softDistance": 0.5 }
		}
	]
}
```

## Module catalogue

<!-- elaboration -->
The plan names the effect features (Z2-I-VFX-01 … 13) and that effects are authored in code with previews (Z2-D-079); the closed module vocabulary, its identifiers and its parameter blocks below are VFX's design.

### Module identifiers

The identifiers of `Flux_ParticleModuleID` (`Zenith2/Docs/Design/VFX/PublicAPI.md → "Effect enumerations"`) map one to one onto the document's module names: stage byte 0x01 spawn (`rate`, `burst`, `distance`, `event`), 0x02 initialisation (`shape`, `velocity`, `lifetime`, `size`, `rotation`, `colour`, `subUVStart`, `inheritVelocity`), 0x03 update (the indices 0 … 12 of "Update modules" as 0x0301 … 0x030D), 0x04 over-life (`colourOverLife`, `sizeOverLife`, `subUV`, `colourBySpeed`, `sizeBySpeed`), 0x05 render (`sprite`, `ribbon`, `mesh`, `trail`), 0x06 effect-level (`light`, `decal`). The vocabulary is closed: a new module is a new identifier, a C++ and a Slang implementation in the same diff, a row in each table of this file, and a format-version bump.

### Module support by target

| Module | cpu | gpu | Why a target lacks it |
|---|---|---|---|
| every spawn, initialisation, over-life and render module | yes | yes | — |
| `gravity`, `force`, `drag`, `turbulence`, `vortex`, `attractor`, `speedLimit`, `killVolume`, `trail` | yes | yes | — |
| `collision` with colliders | yes | yes | — |
| `collision` with `depth` | no | yes, world space only | the depth buffer exists only on the GPU, and a local-space particle's depth test would need the emitter's motion between frames |
| `wind` | no | yes | the wind function is ENV's GPU-and-CPU pair in ZenithEnvironment, which ZenithVFX cannot depend on (no module-to-module edge, [Z2-N-ARCH-850]); the GPU reads `Env_WindParams` through Flux's frame environment |
| `precipitationOcclusion` | no | yes | ENV's occlusion capture is a GPU texture |
| `custom` (`updateType`) | no | yes | a Slang type cannot run on the CPU |
| `light`, `decal` | yes | yes | evaluated on the CPU for every target: they are per emitter, not per particle |

### Module parameter blocks

Every numeric parameter is cooked as one 48-byte `Flux_ParticleValue` ("Effect constants image"); booleans, enumerations and counts as `uint32` words. A module's block is its fields in exactly the order its row lists them: each value at the next 16-byte-aligned offset, and each run of consecutive words forming one 16-byte group, zero-padded, at the point the row lists it (so a row may start with words, as `shape` does, or end with them, as `light` does). These layouts are the contract between the C++ simulator, the Slang simulator and the cook:

| Module | Block, in order | Bytes |
|---|---|---|
| `rate` | V rate | 48 |
| `distance` | V spacing; u32 maxPerFrame, 3 × u32 zero | 64 |
| `burst` | none (the burst table) | 0 |
| `event` | none (the event-edge table) | 0 |
| `shape` | u32 shape, u32 flags (bit 0 alignToDirection, bit 1 cone emit from volume), 2 × u32 zero; V radius; V radiusThickness; V arc (radians); V extents; V angle (radians); V length | 304 |
| `velocity` | u32 direction mode, 3 × u32 zero; V speed; V vector; V randomVector; V radial | 208 |
| `lifetime` | V seconds | 48 |
| `size` | u32 channels (1 or 2), 3 × u32 zero; V size | 64 |
| `rotation` | V angle; V rate | 96 |
| `colour` | V colour; V intensity | 96 |
| `subUVStart` | u32 random, 3 × u32 zero | 16 |
| `inheritVelocity` | V factor | 48 |
| `gravity` | V scale | 48 |
| `force` | u32 space, 3 × u32 zero; V acceleration | 64 |
| `drag` | V coefficient | 48 |
| `turbulence` | u32 octaves, 3 × u32 zero; V strength; V frequency; V scrollSpeed | 160 |
| `vortex` | V axis; V centre; V strength; V pull | 192 |
| `attractor` | V position; V strength; V radius; V killRadius | 192 |
| `wind` | V factor | 48 |
| `speedLimit` | V maxSpeed; V dampen | 96 |
| `collision` | u32 flags (bit 0 colliders, bit 1 depth, bit 2 kill), u32 mask, u32 killAfter, u32 zero; V restitution; V friction; V radiusScale; V depthThickness | 208 |
| `killVolume` | u32 shape, u32 invert, 2 × u32 zero; V centre; V radius; V extents | 160 |
| `trail` | u32 samples, 3 × u32 zero; V interval | 64 |
| `precipitationOcclusion` | u32 zero × 4 | 16 |
| `custom` | the type's `Params`, packed by the type's reflection (≤ 256) | ≤ 256 |
| `colourOverLife` | V colour | 48 |
| `sizeOverLife` | u32 channels, 3 × u32 zero; V size | 64 |
| `subUV` | u32 columns, u32 rows, u32 frames, u32 flags (bit 0 fps mode, bit 1 blend); V fps; V loops | 112 |
| `colourBySpeed` | V colour; V speedRange (2 channels) | 96 |
| `sizeBySpeed` | V size; V speedRange | 96 |
| `sprite`, `ribbon`, `mesh`, `trail` (render) | u32 flags (the render flags below), u32 pivot (half2), u32 stretch (f32 bits), u32 tileLength (f32 bits); u32 axis x, u32 axis y, u32 axis z (f32 bits, emitter space, normalised by the cook; `(0, 1, 0)` when the document gives none), u32 zero; V emissive; V width (trail); V alpha (trail); then the shading type's `Params` (≤ 256) | 176 + Params |
| `light` | V colour; V intensity; V range; V offset; V sourceRadius; u32 channels, 3 × u32 zero | 256 |
| `decal` | u32 at, u32 projection, u32 materialIndex, u32 flags (bit 0 random); V size; V time; V lifetime; V fadeOut | 208 |

Render flags (the render block's first word; `Flux_ParticleRenderDesc::m_uRenderFlags`); every other bit is zero and the reader refuses a set one:

| Bit | Meaning | Document key |
|---|---|---|
| 0 | `uvMode` `tile` (clear: `stretch`) | ribbon `uvMode` |
| 1 | tail fade | ribbon `tailFade` |
| 2 | align to velocity | mesh `alignToVelocity` |
| 3 | the trail's `width` curve is present (clear: width 1) | trail `width` |
| 4 | the trail's `alpha` curve is present (clear: alpha 1) | trail `alpha` |

The facing, blend, sort, lighting, texture, mesh, sub-UV grid, soft distance and sort bias are emitter-record fields ("Emitter record section"), and the axis of `axis` and `world` facing is the block's axis words.

## Curve baking

Every curve, gradient and random-curve bound is baked into one lookup table of `uFLUX_PARTICLE_CURVE_SAMPLES` = 32 `float4` samples (512 bytes); sample i holds the authored curve at `x_i = i / 31`, unused channels zero.

| Interpolation | Authored function between keys (t_k, v_k), (t_{k+1}, v_{k+1}) |
|---|---|
| `linear` | `v_k + (v_{k+1} − v_k) × s`, `s = (x − t_k) / (t_{k+1} − t_k)` |
| `smooth` | monotone cubic Hermite (Fritsch–Carlson tangents), per channel |

Run time evaluates every table the same way on the CPU and the GPU: `s = saturate(x) × 31; k = min(floor(s), 30); f = s − k; v = L[k] + (L[k + 1] − L[k]) × f`, with `saturate` clamping to [0, 1] and every operation in `float`. The cook measures, at 1,024 evenly spaced x, the largest difference between the authored function and the table's evaluation; above 1% of the curve's value range it reports `VFX_COOK_WARNING_CURVE_RESOLUTION` naming the curve, and the table is used anyway. Identical tables (bit-equal samples) are stored once, so two modules sharing a gradient cost one table.

## Cooked effect format

The `.zvfx` file uses ASSET's 64-byte header and 40-byte section entries field for field (`Zenith2/Docs/Design/ASSET/Formats.md → "Header layout"`, `Zenith2/Docs/Design/ASSET/Formats.md → "Section table"`), with the kind `ASSET_KIND_PARTICLE_EFFECT` (19), magic `ZVFX` (`5A 56 46 58`) and format version `uFLUX_PARTICLE_EFFECT_FORMAT_VERSION` = 1. Every multi-byte field is little-endian and is written and read field by field (Z2-D-038). Sections are stored uncompressed (they are small and read once); a future version may compress them through COMPRESS's descriptor without a layout change.

The kind is CPU-resident: its loader creates no GPU resource, and the row of `Zenith2/Docs/Design/ASSET.md → "Asset kinds"`, which ASSET alone edits, already reads kind 19 "GPU resident: no" (applied by ASSET at VFX's request), so headless runs load effects through the real loader.

<!-- elaboration -->
The section layout below, the record layouts and the constants image are VFX's design of the cooked effect; ASSET's container (header, section table, kind) is cited, not changed.

### Effect file header

The file has exactly the ten sections of "Section kinds", each with index 0, in ascending kind order; a section whose table is empty (no bursts, no events) is present with zero stored bytes. Hard catalogue dependencies are the effect's textures, then its meshes, then its decal materials, in the order of the dependency section, so `Zenith_AssetLoadInfo::m_axDependencies` index i is dependency record i.

### Section kinds

| Kind | Name | Content | Element bytes |
|---|---|---|---|
| `0x5601` | `VFX_SECTION_EFFECT` | one effect header record | 128 |
| `0x5602` | `VFX_SECTION_EMITTERS` | emitter records in emitter-table order | 256 |
| `0x5603` | `VFX_SECTION_MODULES` | module records, grouped per emitter in document stage order | 16 |
| `0x5604` | `VFX_SECTION_CONSTANTS` | the effect constants image | 1 |
| `0x5605` | `VFX_SECTION_STRINGS` | UTF-8 names, each followed by one zero byte | 1 |
| `0x5606` | `VFX_SECTION_EVENTS` | event-edge records | 16 |
| `0x5607` | `VFX_SECTION_BURSTS` | burst records | 20 |
| `0x5608` | `VFX_SECTION_LOD` | LOD level records | 16 |
| `0x5609` | `VFX_SECTION_PARAMETERS` | user-parameter records | 32 |
| `0x560A` | `VFX_SECTION_DEPENDENCIES` | dependency records | 4 |

### Effect header section

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | emitter count (1 … 16) |
| 4 | 4 | module record count |
| 8 | 4 | event edge count |
| 12 | 4 | burst count |
| 16 | 4 | LOD level count (1 … 4) |
| 20 | 4 | parameter count |
| 24 | 4 | texture count |
| 28 | 4 | mesh count |
| 32 | 4 | decal material count |
| 36 | 4 | duration (f32, seconds) |
| 40 | 4 | priority (f32) |
| 44 | 4 | linger seconds (f32: the largest `m_fMaxLifetime`) |
| 48 | 4 | cull screen size (f32) |
| 52 | 4 | max distance (f32) |
| 56 | 4 | hysteresis (f32) |
| 60 | 4 | flags: bit 0 loop, bit 1 fixed seed, bit 2 baked bounds, bit 3 default cull behaviour pause; others zero |
| 64 | 4 | seed |
| 68 | 24 | local bounds: min x, y, z, max x, y, z (f32) |
| 92 | 4 | constants image bytes |
| 96 | 4 | string table bytes |
| 100 | 4 | curve count |
| 104 | 24 | reserved, zero |

### Emitter record section

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | name offset (into the string table) |
| 4 | 4 | name bytes (without the zero byte) |
| 8 | 1 | target (`Flux_ParticleTarget`) |
| 9 | 1 | space |
| 10 | 1 | cull behaviour |
| 11 | 1 | emitter index (equals its position) |
| 12 | 4 | capacity |
| 16 | 4 | event capacity |
| 20 | 4 | trail samples |
| 24 | 4 | max lifetime (f32) |
| 28 | 4 | duration (f32) |
| 32 | 4 | start delay (f32) |
| 36 | 24 | local bounds (6 × f32): `B_e`, or an event target's spawn-relative box `S_t` (bounds flag bit 1) |
| 60 | 4 | rate module index or `0xFFFFFFFF` |
| 64 | 4 | distance module index or `0xFFFFFFFF` |
| 68 | 4 | event edge index or `0xFFFFFFFF` |
| 72 | 4 | first burst |
| 76 | 4 | burst count |
| 80 | 4 | max rate (f32) |
| 84 | 4 | spacing (f32) |
| 88 | 4 | prewarm seconds (f32) |
| 92 | 4 | update module mask |
| 96 | 32 | 16 × u16 update module identifiers in index order, 0 for absent |
| 128 | 4 | first module record |
| 132 | 4 | module record count |
| 136 | 4 | constants offset (16-byte aligned) |
| 140 | 4 | constants bytes |
| 144 | 8 | update type ID (0: none) |
| 152 | 4 | light module index or `0xFFFFFFFF` |
| 156 | 4 | decal module index or `0xFFFFFFFF` |
| 160 | 1 | render kind |
| 161 | 1 | blend |
| 162 | 1 | sort mode |
| 163 | 1 | facing |
| 164 | 1 | lighting |
| 165 | 1 | texture index (`0xFF` none) |
| 166 | 1 | mesh index (`0xFF` none) |
| 167 | 1 | sub-UV columns |
| 168 | 1 | sub-UV rows |
| 169 | 1 | trail samples drawn |
| 170 | 2 | reserved, zero |
| 172 | 4 | soft distance (f32) |
| 176 | 4 | sort bias (f32) |
| 180 | 4 | reserved, zero |
| 184 | 8 | shading type ID (0: `ParticleShading_Standard`) |
| 192 | 4 | render constants offset |
| 196 | 4 | render constants bytes |
| 200 | 4 | seed offset (`Flux_DeriveEmitterSeed`'s second input; the emitter index unless the document sets `seedOffset`) |
| 204 | 24 | world displacement box `D`: min x, y, z, max x, y, z (f32; composed over the chain for an event target; "Bounds baking" of `Zenith2/Docs/Design/VFX/Authoring.md`) |
| 228 | 4 | inherited-velocity gain `J` (f32, seconds, 0 … 16,320: a chain of at most 16 emitters, the i-th inheriting a factor of at most 2i, each living at most 60 s, so `Σ 2i × 60`) |
| 232 | 4 | bounds flags: bit 0 compose `D` as a rotation-invariant cube; bit 1 event target (the local bounds are `S_t`); bit 2 estimate class (`VFX_COOK_WARNING_BOUNDS_ESTIMATE`); others zero |
| 236 | 4 | wind gain `G` (f32, metres per m/s, 0 … 32,640: `Σ 4i × 60` by the same chain argument with the `wind` factor at most 4; 0 without a `wind` module in the emitter or its chain) |
| 240 | 4 | bounds history `H` (f32, seconds, at most 16 × 60 + 15 / 15 = 961: `max lifetime` for an emitter that is no event target, `max lifetime + 1/15 + H_source` for a target) |
| 244 | 1 | bounds root emitter (the chain root's index for an event target; the emitter's own index otherwise) |
| 245 | 11 | reserved, zero |

### Module record section

| Offset | Size | Field |
|---|---|---|
| 0 | 2 | module identifier (`Flux_ParticleModuleID`) |
| 2 | 1 | stage (1 spawn, 2 initialisation, 3 update, 4 over-life, 5 render, 6 effect-level) |
| 3 | 1 | update index (0 … 12) or `0xFF` |
| 4 | 4 | parameter block offset in the constants image (16-byte aligned) |
| 8 | 4 | parameter block bytes |
| 12 | 4 | reserved, zero |

### Constants section

The section's bytes are the effect constants image, copied verbatim into the asset and, on first use, into the effect's constants buffer ("Effect constants image").

### String table section

Names in emitter order, then parameter names; each UTF-8, NFC, followed by one zero byte; offsets in records point at the first byte.

### Event edge section

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | source emitter |
| 1 | 1 | target emitter |
| 2 | 1 | on (`Flux_ParticleEventKind`) |
| 3 | 1 | children per event |
| 4 | 1 | max collision events |
| 5 | 3 | reserved, zero |
| 8 | 4 | inherit velocity (f32) |
| 12 | 4 | reserved, zero |

Burst records (20 bytes): time (f32), count min (u32), count max (u32), cycles (u32), interval (f32). LOD records (16 bytes): screen size (f32), spawn scale (f32), the byte offset in the constants image of the level's 16 disabled-module masks (u32, one mask per emitter), and a reserved u32. Parameter records (32 bytes): name offset (u32), first slot (u8), slot count (u8), 2 reserved bytes, 4 × f32 default, 8 reserved bytes. Dependency records (4 bytes): role (u8: 1 texture, 2 mesh, 3 decal material), index within the role (u8), 2 reserved bytes; the GUIDs themselves are the catalogue's dependency list.

### Reader validation

`Flux_ParticleEffectReader::Read` treats every byte as untrusted ([Z2-N-CONTRACT-472]) and refuses, with `FLUX_PARTICLE_ERROR_CORRUPT` unless stated:

1. a missing section kind, a section index other than 0, or a section size that is not its element count × element bytes (the counts come from the effect header);
2. a count above "Format limits" (`FLUX_PARTICLE_ERROR_LIMIT`);
3. any reserved field or flag bit not zero;
4. an enumeration value outside its type; a module identifier outside the catalogue; an update index that disagrees with the module identifier;
5. an offset or offset + size outside its section, with overflow-checked arithmetic ([Z2-N-ARCH-951]); a constants block not 16-byte aligned; two module blocks overlapping;
6. a non-finite float anywhere; a bounds minimum above its maximum (the local bounds or the world displacement box); a capacity that is zero, not a multiple of 64 or above its target's maximum; the capacity of an event target whose source is itself an event target (depth 2 or more) that is not a power of two (its 32-bit child ordinals wrap, and only a capacity dividing 2³² keeps its slots continuous, `Zenith2/Docs/Design/VFX/Simulation.md → "Event spawning"`); an event capacity other than `capacity × E` of the emitter's outgoing edges, or above 262,144; an inherited-velocity gain outside [0, 16,320], a wind gain outside [0, 32,640], or a bounds history below the emitter's max lifetime or above 961; bounds flag bit 1 set on an emitter that is no event target or clear on one that is; a bounds root emitter that is not the first ancestor of an event target that is no event target, or not the emitter's own index otherwise; a non-unit render axis (`|axis|² outside [0.98, 1.02]`);
7. LOD screen sizes not strictly decreasing; an event edge that is a self-loop, a cycle, a second incoming edge, or a source after its target;
8. a string offset not followed by a zero byte inside the table, or a name that is not valid UTF-8;
9. a dependency count that differs from `m_axDependencies`, or a dependency of the wrong kind;
10. a `Flux_ParticleValue` whose curve index is not below the curve count (with `RANDOM_CURVE`, index + 1 too), whose parameter slot + channels exceeds 8, or whose kind is outside 0 … 5.

A refused file never reaches a simulator: the asset fails with the code and the instance falls back as "Failure table" of `Zenith2/Docs/Design/VFX.md` states.

### Format limits

| Quantity | Limit |
|---|---|
| emitters | 16 |
| module records | 16 × 47 = 752 (47 per emitter) |
| curves | 64 |
| constants image | 98,560 bytes: 32,768 for 64 curve tables, 65,536 for 16 emitter blocks of at most 4,096 bytes, 256 for the LOD masks |
| event records per source emitter (`capacity × E`) | 262,144 |
| string table | 4,096 bytes |
| event edges, bursts, LOD levels, parameters, dependencies | 16, 256, 4, 8, 16 |
| capacity per emitter | 262,144 (gpu), 4,096 (cpu) |

Each limit is tested at the limit and at the limit + 1 ([Z2-N-ARCH-953]), and the fuzz target `Zenith_Fuzz_ParticleEffect` drives the reader with mutated files (`Zenith2/Docs/Design/VFX/Tests.md → "Failure and fault-injection tests"`).

## Effect constants image

<!-- elaboration -->
The image is one byte array, identical in the asset's CPU copy and the GPU buffer:

| Range | Content |
|---|---|
| `[0, 512 × curves)` | the curve tables, 32 `float4` each, in curve-index order |
| then, per emitter in emitter order, 16-byte aligned | the emitter block: a 160-byte emitter header, then its module parameter blocks in module-record order |
| last | the LOD disabled-module masks: 4 levels × 16 emitters × u32 |

The 160-byte emitter header:

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | update module mask |
| 4 | 4 | initialisation module mask (bit i: initialisation module i of the stage-0x02 order) |
| 8 | 4 | over-life module mask |
| 12 | 4 | flags: bit 0 local space, bit 1 has trail, bit 2 event source, bit 3 event target |
| 16 | 64 | 16 × u32 update module block offsets, relative to the emitter block, 0 for absent |
| 80 | 32 | 8 × u32 initialisation module block offsets |
| 112 | 32 | 8 × u32 over-life module block offsets (5 used) |
| 144 | 4 | render module block offset |
| 148 | 4 | max lifetime (f32) |
| 152 | 8 | reserved, zero |

`Flux_ParticleValue` (48 bytes, `[FluxShared]` in `Zenith2/Shaders/Common/Particles.slang`):

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | `uint m_uKind` (0 `CONST`, 1 `RANDOM`, 2 `RANDOM_UNIFORM`, 3 `CURVE`, 4 `RANDOM_CURVE`, 5 `PARAM`) |
| 4 | 4 | `uint m_uIndex` (curve index, or the first parameter slot) |
| 8 | 4 | `uint m_uChannels` (1 … 4) |
| 12 | 4 | `uint m_uDraw` (the random draw index this value consumes, "Particle random numbers") |
| 16 | 16 | `float4 m_xA` (constant; random lower bound; curve scale; binding scale) |
| 32 | 16 | `float4 m_xB` (random upper bound; curve offset; binding offset) |

`Flux_EvaluateValue(xValue, fX, uSeed, uOrdinal)` (both in `Zenith2/Shaders/Particles/ParticleCurves.slang` and in `Zenith2/Engine/ZenithVFX/Internal/Simulation/Zenith_VFXValues.cpp`, with identical operation order):

| Kind | Result per channel c |
|---|---|
| `CONST` | `A[c]` |
| `RANDOM` | `A[c] + (B[c] − A[c]) × U(m_uDraw + c)` |
| `RANDOM_UNIFORM` | `A[c] + (B[c] − A[c]) × U(m_uDraw)` |
| `CURVE` | `Curve(m_uIndex, x)[c] × A[c] + B[c]` |
| `RANDOM_CURVE` | `(Curve(i, x)[c] + (Curve(i + 1, x)[c] − Curve(i, x)[c]) × U(m_uDraw)) × A[c] + B[c]` |
| `PARAM` | `P[m_uIndex + c] × A[c] + B[c]` |

with `U(d) = Flux_ParticleRandomUnit(uSeed, uOrdinal, d)`.

## Effect document migration

The document type is `Zenith.VFX.EffectDocument`, schema version 1, registered tool-side by `Zenith_RegisterVFXDocumentTypes(Zenith_MigrationRegistry&)` (`Zenith2/Tools/ZenithCook/Internal/VFX/Zenith_VFXDocumentTypes.h`, linked by ZenithCook and ZenithEditor) into SCENE's registry (`Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Zenith_MigrationRegistry header"`), with its canonical schema; the row joins `Zenith2/Docs/Design/ASSET/Migration.md → "Registered document types"` with the registration ticket (cross-area row VX5). Version 1 has no migration step; each subsequent version adds exactly one step and a fixture pair `Zenith2/Tools/ZenithCook/Tests/Fixtures/Migration/VFX/v<N>.json` and `v<N>.expected.json` ([Z2-N-CONTRACT-548]). The cooked format is never migrated: a format change bumps `uFLUX_PARTICLE_EFFECT_FORMAT_VERSION` and every effect re-cooks ([Z2-N-CONTRACT-529]).
