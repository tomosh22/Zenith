# ENV design: procedural placement

Section file of `Zenith2/Docs/Design/ENV.md`. It specifies `Zenith_ProceduralPlacement`, the one algorithm that scatters foliage and props over a surface: its rule document, its seeded counter-based streams (`Zenith2/Docs/Design/CORE/Maths.md → "Zenith_RandomSource"`), candidate generation, rule filters, spacing and exclusion, and the argument that its output is identical for any worker count and any cell processing order. The cook runs it per environment cell to produce foliage cells; the editor runs it for preview and for the scatter brush; games may run it at world load for reproducible procedural generation, whose orchestration (which rules apply to which partition, when regeneration happens, how results are baked and locked for concurrent editing) is WORLD's ("Boundary with WORLD procedural generation").

## Scope of this file

| Concern | Heading |
|---|---|
| inputs and outputs | "Placement model" |
| the rule document | "Placement rule document" |
| API | "Zenith_ProceduralPlacement header" |
| seeds and draws | "Seeded streams" |
| candidate points | "Candidate generation" |
| filters and acceptance | "Rule evaluation" |
| spacing, priorities, exclusion shapes | "Exclusion and priority" |
| neighbour cells | "Cross-cell determinism" |
| why the output is deterministic | "Determinism argument" |
| sorting and capacity | "Output ordering and capacity" |
| the three callers | "Runtime and editor use" |
| ENV versus WORLD | "Boundary with WORLD procedural generation" |

## Placement model

Placement runs over **placement cells**: the environment cells of 128 m × 128 m aligned with the terrain's tiles (`Zenith2/Docs/Design/ENV/Terrain.md → "Coordinates and tile grid"`), or, for a caller without a terrain, a grid of the same size anchored at the caller's origin. Inputs:

| Input | Source |
|---|---|
| the rule sets (at most `uENV_TERRAIN_MAX_PLACEMENT_RULESETS` = 16, each an ordered list of at most 64 rules) | `*.placement.json` documents, in the order the terrain document lists them; the rules of every set form one ordered list of **stages**, (set 0, rule 0), (set 0, rule 1), …, (set 1, rule 0), … ("Cross-rule exclusion") |
| a surface | a `Zenith_PlacementSurface` implementation: the terrain document (editor, cook), the runtime terrain resource (runtime), or a test surface |
| a seed | the terrain's `m_ulPlacementSeed`, or the world seed for runtime callers (WORLD) |
| exclusion shapes | authored boxes, discs and polylines (roads, rivers, building footprints), plus the tiles' exclusion masks |
| explicit instances | already placed instances that procedural rules must avoid (their footprints, as `Zenith_PlacementObstacle`) |
| foliage-type information | per entry of the terrain's foliage-type list: its GUID, footprint radius, scale range and normal alignment (`Zenith_PlacementFoliageTypeInfo`) |
| the layer palette | the terrain's layer IDs in palette order, against which a rule's `layers` resolve |

Output: per cell, a sorted array of `Zenith_PlacementInstance` (position, rotation, scale, rule-set index, rule index, candidate index, thinning rank), whose instance key ("Output ordering and capacity") is unique within the cell across every rule set.

## Placement rule document

`*.placement.json`, an authoring document:

```json
{
	"schemaVersion": 1,
	"id": "7e2a…",
	"rules": [
		{ "name": "Pines", "foliageType": "5b0f…", "densityPerSquareMetre": 0.004, "minDistance": 5.0,
		  "slopeDegrees": [ 0, 32 ], "heightRange": [ 5.0, 900.0 ],
		  "layers": [ { "layer": "c18e…", "minWeight": 0.4 } ], "useDensityMap": true,
		  "noise": { "wavelength": 180.0, "threshold": 0.45, "softness": 0.1 },
		  "avoidWater": true, "excludeEarlierRules": true, "exclusionRadius": 4.0,
		  "scaleRange": [ 0.8, 1.3 ], "alignToNormal": 0.1 },
		{ "name": "Rocks", "foliageType": "…", "densityPerSquareMetre": 0.002, "minDistance": 3.0, "…": "…" }
	]
}
```

| Key | Range | Meaning |
|---|---|---|
| `rules` | 1 … 64, applied in order (the order is the priority); across the rule sets of one terrain, set order then rule order ("Cross-rule exclusion") | |
| `foliageType` | a foliage type GUID of the terrain's list | the type placed |
| `densityPerSquareMetre` | 0 < ρ ≤ 64 | target candidate density before filters |
| `minDistance` | 0 … 64 m, at most `fENV_PLACEMENT_CELL_SIZE / 2` (64 m) | same-rule spacing ("Same-rule spacing") |
| `slopeDegrees` | two values in [0, 90], min ≤ max | accepted surface slope |
| `heightRange` | two values, min ≤ max | accepted surface height (world Y) |
| `layers` | 0 … 16 entries `{ "layer": id, "minWeight": w }` | accepted splat layers, each named by its terrain palette entry's layer ID (`layers[].id` of the terrain document), with minimum weights in [0, 1]; empty accepts every layer |
| `useDensityMap` | bool | multiply acceptance by the painted density of `foliageType` |
| `noise` | `null` or `{wavelength 1 … 10,000 m, threshold 0 … 1, softness 0 … 1}` | modulates acceptance with value noise ("Noise modulation") |
| `avoidWater` | bool | rejects points under any water body |
| `excludeEarlierRules`, `exclusionRadius` | bool; 0 … 64 m; at most `uENV_PLACEMENT_MAX_EXCLUDING_STAGES` (8) stages of one placement run set it | "Cross-rule exclusion": the earlier stages are every earlier rule of this set and every rule of every earlier set |
| `scaleRange`, `alignToNormal` | as the foliage type | override the type's values when present |

## Zenith_ProceduralPlacement header

```cpp
// Zenith2/Engine/ZenithEnvironment/Zenith_ProceduralPlacement.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithCore/Zenith_Quaternion.h"
#include "ZenithEnvironment/Zenith_EnvironmentErrors.h"
#include "ZenithEnvironment/Zenith_EnvironmentTypes.h"

class Zenith_JobSystem;
class Zenith_Arena;

/// @contract interface Zenith_PlacementSurface
/// Ownership: Implemented by the terrain document, the runtime terrain resource adapter and tests; borrowed for one
///   Place call.
/// Thread affinity: ANY_THREAD. Every member is a pure read, called concurrently from the placement sub-jobs.
/// Invalidation: The implementation must not change while a Place call runs.
/// Units: Metres; weights and densities in [0, 1].
/// Errors: A point with no surface returns false from Sample, and the candidate is rejected; nothing asserts.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 3. Source API.
/// @endcontract
class Zenith_PlacementSurface
{
public:
	virtual ~Zenith_PlacementSurface() = default;
	// Height, unit normal and the hole flag at (fX, fZ), and the exclusion value (0 … 1) that applies to rule set
	// uRuleSetIndex: max(that set's mask, the ALL mask) of the quad containing the point ("Surface filters"); false when
	// outside the surface.
	virtual bool Sample(float fX, float fZ, uint32 uRuleSetIndex, Zenith_PlacementSurfaceSample& xOut) const = 0;
	virtual float SampleLayerWeight(float fX, float fZ, uint32 uLayerIndex) const = 0;
	virtual float SampleDensity(float fX, float fZ, uint32 uFoliageType) const = 0;
	virtual bool IsUnderWater(float fX, float fZ) const = 0;
	// The inputs this surface cannot answer (uENV_PLACEMENT_SURFACE_MISSING_* bits); 0 for a surface over a document.
	virtual uint32 GetMissingInputs() const { return 0u; }
};
inline constexpr uint32 uENV_PLACEMENT_SURFACE_MISSING_LAYERS = 1u;            // SampleLayerWeight has no painted weights
inline constexpr uint32 uENV_PLACEMENT_SURFACE_MISSING_DENSITY = 2u;           // SampleDensity has no painted density
inline constexpr uint32 uENV_PLACEMENT_SURFACE_MISSING_EXCLUSION_MASKS = 4u;   // Sample's m_fExclusion has no painted mask

/// @contract value Zenith_PlacementInstance
/// Ownership: Plain value written into a caller-provided span.
/// Thread affinity: IMMUTABLE. Members non-const: an element of an output array that its producer fills in place.
/// Invalidation: None.
/// Units: World metres; unit quaternion; uniform scale.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_PlacementInstance
{
	Zenith_Vector3 m_xPosition;
	Zenith_Quaternion m_xRotation;
	float m_fScale = 1.0f;
	uint8 m_uRuleSetIndex = 0;         // 0 … 15: the set's position in Zenith_PlacementDesc::m_axRuleSets
	uint8 m_uRuleIndex = 0;            // 0 … 63: the rule's position in its set
	uint16 m_uThinningRank = 0;
	uint32 m_uCandidateIndex = 0;      // row-major index of the candidate in its rule's lattice of the cell, below 2^20
};

/// @contract value Zenith_PlacementDesc
/// Ownership: Plain value holding borrowed spans that outlive the Place call.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need before
///   passing it.
/// Invalidation: None.
/// Units: Metres.
/// Errors: Validated by Place (ENV_ERROR_PLACEMENT_RULE_INVALID with the rule index; ENV_ERROR_INVALID_DESC).
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_PlacementDesc
{
	Zenith_Span<const Zenith_PlacementRuleSet> m_axRuleSets;        // 1 … 16, the terrain's list order; loaded from *.placement.json
	Zenith_Span<const Zenith_PlacementFoliageTypeInfo> m_axFoliageTypes;   // the terrain's foliage-type list order
	Zenith_Span<const Zenith_PersistentID> m_axLayerPalette;        // the terrain's layer IDs in palette order
	const Zenith_PlacementSurface* m_pxSurface = nullptr;
	uint64 m_ulSeed = 0;
	Zenith_Vector3 m_xGridOrigin;                                   // the placement cell grid's minimum corner
	Zenith_Span<const Zenith_EnvCellCoord> m_axCells;               // the cells to produce, any order, no duplicates
	Zenith_Span<const Zenith_PlacementExclusion> m_axExclusions;    // authored shapes
	Zenith_Span<const Zenith_PlacementObstacle> m_axObstacles;      // explicit instances' footprints
	uint32 m_uMaxInstancesPerCell = 65536;
};

/// @contract type Zenith_PlacementResult
/// Ownership: Move-only; owns the per-cell instance arrays, allocated from the arena passed to Place.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: Invalid after the arena is reset.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_PlacementResult
{
public:
	uint32 GetCellCount() const;                                   // equals the desc's cell count
	Zenith_EnvCellCoord GetCell(uint32 uIndex) const;              // ascending (z, x)
	Zenith_Span<const Zenith_PlacementInstance> GetInstances(uint32 uIndex) const;
	const Zenith_PlacementStats& GetStats() const;                   // candidates, kept, rejections per filter
};

/// @contract functions Zenith_ProceduralPlacement
/// Ownership: Pure functions; scratch and results from the caller's arena.
/// Thread affinity: ANY_THREAD for the caller; Place fans out sub-jobs through pxJobs and joins them before returning.
/// Invalidation: None.
/// Units: Metres.
/// Errors: ENV_ERROR_PLACEMENT_RULE_INVALID, ENV_ERROR_INVALID_DESC, ENV_ERROR_PLACEMENT_CAPACITY (a cell exceeded
///   m_uMaxInstancesPerCell: the result holds the first instances in canonical order and the error names the cell),
///   ENV_ERROR_OUT_OF_MEMORY (the arena is exhausted; nothing is returned).
/// Version: since Z2 Stage 3. uENV_PLACEMENT_ALGORITHM_VERSION (2) enters every cook's derived-data key; any change to
///   an algorithm step of this file bumps it.
/// @endcontract
Zenith_Result<Zenith_PlacementResult, Zenith_EnvironmentError> Zenith_Place(const Zenith_PlacementDesc& xDesc,
	Zenith_JobSystem* pxJobs, Zenith_Arena& xArena);
Zenith_Result<Zenith_PlacementRuleSet, Zenith_EnvironmentError> Zenith_ReadPlacementRuleSet(
	Zenith_ByteSpan xJson, Zenith_Arena& xArena);
inline constexpr uint32 uENV_PLACEMENT_ALGORITHM_VERSION = 2;       // 2: rule-set keys and cross-set stages (revision 1)
inline constexpr uint32 uENV_PLACEMENT_MAX_RULES_PER_SET = 64;
inline constexpr uint32 uENV_PLACEMENT_MAX_EXCLUDING_STAGES = 8;
inline constexpr uint32 uENV_PLACEMENT_MAX_CANDIDATES_PER_CELL = 1u << 20;
inline constexpr uint32 uENV_EXPLICIT_INSTANCE_KEY_BIT = 0x80000000u;   // declared before the key functions that use it
// The instance key of "Output ordering and capacity".
constexpr uint32 Zenith_PlacementInstanceKey(const Zenith_PlacementInstance& xInstance)
{
	return (uint32(xInstance.m_uRuleSetIndex) << 27) | (uint32(xInstance.m_uRuleIndex) << 21) | xInstance.m_uCandidateIndex;
}
constexpr uint32 Zenith_ExplicitInstanceKey(uint32 uExplicitID) { return uENV_EXPLICIT_INSTANCE_KEY_BIT | uExplicitID; }
```

`Zenith_EnvCellCoord` is declared in `Zenith_EnvironmentTypes.h` (`Zenith2/Docs/Design/ENV.md → "Zenith_EnvironmentTypes header"`); the value types below are declared in this header, before `Zenith_PlacementSurface`, so it compiles alone.

### Placement value types

```cpp
// Zenith2/Engine/ZenithEnvironment/Zenith_ProceduralPlacement.h (continued, declared before Zenith_PlacementSurface)
/// @contract enum Zenith_PlacementFilter
/// Ownership: Plain enumeration; the index of Zenith_PlacementStats::m_aulRejected.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3. Values are stable (the counters environment.placement.rejected_<filter> follow them).
/// @endcontract
enum Zenith_PlacementFilter : uint8
{
	PLACEMENT_FILTER_SURFACE = 0,        // Sample false, or a hole
	PLACEMENT_FILTER_EXCLUSION,          // the mask of the rule's set or the ALL mask, or an authored shape
	PLACEMENT_FILTER_HEIGHT,
	PLACEMENT_FILTER_SLOPE,
	PLACEMENT_FILTER_WATER,
	PLACEMENT_FILTER_LAYER,
	PLACEMENT_FILTER_ACCEPTANCE,         // the density and noise draw
	PLACEMENT_FILTER_SPACING,            // "Same-rule spacing"
	PLACEMENT_FILTER_CROSS_RULE,         // "Cross-rule exclusion", against earlier stages and obstacles
	PLACEMENT_FILTER_COUNT,
};

/// @contract value Zenith_PlacementRule
/// Ownership: Plain value inside a Zenith_PlacementRuleSet's arena storage; one rule of a *.placement.json document.
/// Thread affinity: IMMUTABLE after Zenith_ReadPlacementRuleSet returns. Members non-const: the document reader
///   default-constructs it and fills it field by field.
/// Invalidation: With the reader's arena.
/// Units: Metres; radians (converted from the document's degrees); instances per square metre.
/// Errors: Validated by the reader against "Placement rule document" (ENV_ERROR_PLACEMENT_RULE_INVALID, m_uIndex the rule
///   index); Zenith_Place additionally resolves m_xFoliageType against the desc's foliage types and each of m_axLayerIDs
///   against its layer palette (ENV_ERROR_PLACEMENT_RULE_INVALID, m_uIndex the global stage, m_uIndex2 the layer entry, or
///   0xFFFFFFFF for the foliage type).
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_PlacementRule
{
	uint64 m_ulStableID = 0;                 // "Seeded streams"; derived from the name and the set's ID by the reader
	Zenith_PersistentID m_xFoliageType;       // resolved by Zenith_Place against Zenith_PlacementDesc::m_axFoliageTypes
	float m_fDensityPerSquareMetre = 0.0f;    // (0, 64]
	float m_fMinDistance = 0.0f;              // [0, 64]
	float m_fSlopeMin = 0.0f;                 // radians, [0, π/2]
	float m_fSlopeMax = 1.5707964f;
	float m_fHeightMin = -8192.0f;            // world Y
	float m_fHeightMax = 8192.0f;
	Zenith_PersistentID m_axLayerIDs[16];     // the layer IDs of "layers"; the first m_uLayerCount are valid
	float m_afLayerMinWeight[16] = {};        // [0, 1]
	uint32 m_uLayerCount = 0;                // 0 accepts every layer
	bool m_bUseDensityMap = false;
	bool m_bNoiseEnabled = false;
	bool m_bAvoidWater = false;
	bool m_bExcludeEarlierRules = false;
	float m_fNoiseWavelength = 0.0f;          // [1, 10,000] when enabled
	float m_fNoiseThreshold = 0.0f;           // [0, 1]
	float m_fNoiseSoftness = 0.0f;            // [0, 1]
	float m_fExclusionRadius = 0.0f;          // [0, 64]
	float m_fScaleMin = 0.0f;                 // 0 with m_fScaleMax 0: use the foliage type's range
	float m_fScaleMax = 0.0f;
	float m_fAlignToNormal = -1.0f;           // −1: use the foliage type's value; else [0, 1]
};

/// @contract value Zenith_PlacementRuleSet
/// Ownership: Plain value returned by Zenith_ReadPlacementRuleSet; m_axRules borrows the reader's arena.
/// Thread affinity: IMMUTABLE. Members non-const: the document reader default-constructs it and fills it field by
///   field.
/// Invalidation: With the reader's arena.
/// Units: None.
/// Errors: None after the reader.
/// Version: since Z2 Stage 3. The document's schema version is m_uSchemaVersion; the in-memory form is source API.
/// @endcontract
struct Zenith_PlacementRuleSet
{
	Zenith_PersistentID m_xID;                            // the document's id; seeds every rule's stable ID
	uint32 m_uSchemaVersion = 0;
	Zenith_Span<const Zenith_PlacementRule> m_axRules;    // 1 … 64, document order
};

/// @contract value Zenith_PlacementFoliageTypeInfo
/// Ownership: Plain value; one per entry of the terrain's foliage-type list, filled by the caller from the foliage type
///   documents.
/// Thread affinity: IMMUTABLE. Members non-const: callers fill arrays of it element by element before passing them.
/// Invalidation: None.
/// Units: Metres; a fraction for the normal alignment.
/// Errors: None; the foliage type reader validated the values.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_PlacementFoliageTypeInfo
{
	Zenith_PersistentID m_xID;
	float m_fFootprintRadius = 0.0f;          // "footprintRadius" of the foliage type document
	float m_fScaleMin = 1.0f;
	float m_fScaleMax = 1.0f;
	float m_fAlignToNormal = 0.0f;
};

/// @contract value Zenith_PlacementSurfaceSample
/// Ownership: Plain value written by Zenith_PlacementSurface::Sample.
/// Thread affinity: IMMUTABLE. Members non-const: its producer writes it field by field into the caller's output.
/// Invalidation: None.
/// Units: Metres; m_xNormal unit; m_fExclusion in [0, 1].
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_PlacementSurfaceSample
{
	float m_fHeight = 0.0f;
	Zenith_Vector3 m_xNormal;                 // of the triangle containing the point
	float m_fExclusion = 0.0f;                // max(the rule set's mask, the ALL mask) / 255 at the containing quad; 0 when neither exists
	bool m_bHole = false;
};

/// @contract enum Zenith_PlacementExclusionShape
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; Zenith_Place rejects any other value (ENV_ERROR_INVALID_DESC, m_uIndex the exclusion index).
/// Version: since Z2 Stage 3. Values are stable (the exclusion component serializes them).
/// @endcontract
enum Zenith_PlacementExclusionShape : uint8 { PLACEMENT_EXCLUSION_BOX = 1, PLACEMENT_EXCLUSION_DISC, PLACEMENT_EXCLUSION_POLYLINE };

/// @contract value Zenith_PlacementExclusion
/// Ownership: Plain value; m_axPoints borrows caller storage that outlives the Place call.
/// Thread affinity: IMMUTABLE. Members non-const: callers fill arrays of it element by element before passing them.
/// Invalidation: None beyond the borrowed span.
/// Units: World metres; radians.
/// Errors: Validated by Zenith_Place: non-finite values, a negative extent, radius or width, or a polyline outside 2 … 1,024
///   points are ENV_ERROR_INVALID_DESC with the exclusion index.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_PlacementExclusion
{
	Zenith_PlacementExclusionShape m_eShape = PLACEMENT_EXCLUSION_DISC;
	float m_fCentreX = 0.0f;                  // box, disc
	float m_fCentreZ = 0.0f;
	float m_fHalfExtentX = 0.0f;              // box
	float m_fHalfExtentZ = 0.0f;
	float m_fYaw = 0.0f;                      // box: radians about +Y
	float m_fRadius = 0.0f;                   // disc
	float m_fWidth = 0.0f;                    // polyline
	Zenith_Span<const Zenith_Vector2> m_axPoints;   // polyline: 2 … 1,024 XZ points
};

/// @contract value Zenith_PlacementObstacle
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: callers fill arrays of it element by element before passing them.
/// Invalidation: None.
/// Units: World metres.
/// Errors: Validated by Zenith_Place (a non-finite value or a negative radius: ENV_ERROR_INVALID_DESC with the obstacle index).
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_PlacementObstacle
{
	float m_fX = 0.0f;
	float m_fZ = 0.0f;
	float m_fRadius = 0.0f;                   // the explicit instance's foliage-type footprint radius
};

/// @contract value Zenith_PlacementStats
/// Ownership: Plain value owned by a Zenith_PlacementResult; m_aulKeptPerStage borrows its arena.
/// Thread affinity: IMMUTABLE. Members non-const: its producer resets it and accumulates its counters in place.
/// Invalidation: With the result's arena.
/// Units: Counts over the requested cells (halo cells are not counted).
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_PlacementStats
{
	uint64 m_ulCandidates = 0;
	uint64 m_aulRejected[PLACEMENT_FILTER_COUNT] = {};    // by first failing filter
	uint64 m_ulKept = 0;
	uint64 m_ulSamplesWithoutMasks = 0;                   // samples taken over a surface missing exclusion masks
	Zenith_Span<const uint64> m_aulKeptPerStage;          // one per global stage; the cook report's placedPerRule
};
```

## Seeded streams

Every random value of placement is a counter-based draw, keyed so that it depends on what it describes and on nothing else:

`stream(rule, cell) = Zenith_Hash64U64(pack(cell), Zenith_Hash64U64(ruleStableID, ulENV_PLACEMENT_SALT))`, with `ruleStableID = Zenith_Hash64(rule name, Zenith_Hash64U64(setID.m_ulLow, setID.m_ulHigh))` (the rule's UTF-8 name hashed with a seed derived from its rule set's `Zenith_PersistentID`, so two sets that both name a rule "Pines" draw independent streams), `pack(cell) = (uint64)(uint32)cx << 32 | (uint32)cz` and `ulENV_PLACEMENT_SALT = 0x456E76506C616365` ("EnvPlace"); the source is `Zenith_RandomSource(seed, stream)` with its counter set to `candidateIndex × 8 + k` before draw `k`.

| k | Draw |
|---|---|
| 0 | priority, the `uint64` compared in "Same-rule spacing" |
| 1, 2 | jitter in X and Z, `NextUnitFloat` |
| 3 | yaw, `NextUnitDouble × 2π` |
| 4 | scale, `lerp(min, max, NextUnitFloat)` |
| 5 | acceptance, `NextUnitFloat`, against the acceptance probability |
| 6 | thinning rank, `NextU32 >> 16` |
| 7 | reserved (variant selection) |

Setting the counter explicitly makes each candidate's draws independent of how many candidates came before it, of which were rejected, and of the order cells are processed in (Z2-D-019). Renaming a rule changes its stable ID and therefore its results; the editor warns before a rename.

## Candidate generation

For rule `r` with density `ρ` in cell `(cx, cz)`:

1. Lattice spacing `a = 1 / sqrt(ρ)`; the rule's lattice is anchored at the grid origin (not the cell), so lattice points of adjacent cells continue one grid.
2. The candidates of the cell are the lattice squares whose lower corner lies in the cell's half-open square `[cellMin, cellMin + 128)`; square `(p, q)` of the cell is candidate index `q × n_x + p` in row-major order, `n_x` = the number of squares per row in the cell.
3. The candidate's position is the square's lower corner plus `(jx × a, jz × a)` with the draws 1 and 2.

A cell of 128 m holds at most `ceil(128 / a)²` candidates, 1,048,576 at the maximum density of 64 per m²; `m_uMaxInstancesPerCell` bounds what is kept.

## Rule evaluation

### Surface filters

A candidate is rejected, in this order, recording the first failing filter in `Zenith_PlacementStats`:

1. outside the surface (`Sample` false) or over a hole;
2. `m_fExclusion ≥ 0.5` from `Sample(x, z, ruleSetIndex, …)` (the texel of the candidate's rule set's mask or of the ALL mask, whichever is larger, divided by 255: a texel of 128 or more rejects; there is no filtering between texels), or inside an authored exclusion shape;
3. height outside `heightRange`;
4. slope `acos(normal.y)` outside `slopeDegrees`;
5. `avoidWater` and `IsUnderWater`;
6. `layers` non-empty and no listed layer with `SampleLayerWeight(x, z, palette index) ≥ minWeight`, the palette index being the entry of `m_axLayerPalette` equal to the layer's ID, which `Zenith_Place` resolves once per stage before any candidate (an unknown ID fails the call, so a palette reorder changes no result).

### Density acceptance

The acceptance probability is `P = D × N`, with `D` the painted density (`SampleDensity`, [0, 1]) when `useDensityMap`, else 1, and `N` the noise factor below, else 1; the candidate survives when draw 5 `< P`.

### Noise modulation

`N = saturate((v − (threshold − softness)) / (2 × softness))` for `softness > 0`, else `v ≥ threshold ? 1 : 0`, where `v` is value noise at `position / wavelength` with lattice values `Zenith_RandomToUnitFloat(Zenith_Hash64U64(pack(L), seed ^ ruleStableID))` and smoothstep interpolation (the same value noise as the terrain NOISE brush, `Zenith2/Docs/Design/ENV/Terrain.md → "Brush kinds"`).

## Exclusion and priority

### Same-rule spacing

Among the candidates of one rule that pass every filter and the acceptance draw (the **survivors**), a survivor is **spaced** when no other survivor of the same rule within `minDistance` has a higher priority (draw 0; ties broken by the lower (cell z, cell x, candidate index)). Spacing is decided among the survivors alone, before cross-rule exclusion: a survivor that cross-rule exclusion then removes still takes part in spacing (it still suppresses a lower-priority neighbour), so the spaced set of a (rule, cell) is a function of that rule's survivors only. "Within r" of two points a and b is always the test `dx × dx + dz × dz < r × r` with `dx = a.x − b.x`, `dz = a.z − b.z`, evaluated in `float` in that order with no contraction (`Zenith2/Docs/Architecture/BuildSystem.md → "Compile and link flags"`); because IEEE subtraction gives `b.x − a.x = −(a.x − b.x)` exactly and squaring removes the sign, the test is symmetric, which the order-independence below rests on. This is a single round of a local-maximum independent set: every spaced pair is at least `minDistance` apart, and whether a survivor is spaced depends only on the survivors within `minDistance` of it, never on processing order. It keeps fewer points than a greedy Poisson-disk pass; the achieved density per rule is reported (`placedPerRule` in the cook report) so an artist tunes `densityPerSquareMetre` against what was achieved. A survivor that is not spaced counts `PLACEMENT_FILTER_SPACING`.

### Cross-rule exclusion

Every rule of every rule set of the run is one **stage**; stages are ordered by (rule-set index, rule index), so the global stage index of rule r of set s is the number of rules in sets 0 … s − 1 plus r (at most 16 × 64 = 1,024 stages). After spacing, a stage whose rule sets `excludeEarlierRules` removes each spaced instance (`PLACEMENT_FILTER_CROSS_RULE`) that lies within `exclusionRadius + footprint(earlier)` of any kept instance of any earlier stage, whichever rule set that stage belongs to, or within `exclusionRadius + obstacle radius` of an obstacle; `footprint(earlier)` is the earlier instance's foliage-type footprint radius (`Zenith2/Docs/Design/ENV/Foliage.md → "Foliage type document"`). The spaced instances that remain are the stage's **kept** instances, the only ones the subsequent stages and the output see; a removed instance leaves its place empty, never refilled by a lower-priority survivor. So a rule set listed after another yields to every set listed before it, and within a set each rule yields to the rules before it, exactly as when the sets were one document. Because a stage starts only after the previous stage has finished every cell, each stage reads complete, fixed results.

At most `uENV_PLACEMENT_MAX_EXCLUDING_STAGES` (8) stages of one run may set `excludeEarlierRules`; `Zenith_Place` refuses a ninth with `ENV_ERROR_PLACEMENT_RULE_INVALID` naming its global stage index. The bound keeps the lazy halo of "Cross-cell determinism" small: a single-cell editor preview then touches at most (2 × 9 + 1)² = 361 cells in its earliest stage, so a run of 1,024 stages costs at most 1,024 × 361 (stage, cell) evaluations for one requested cell, never a halo that grows one ring per rule.

### Authored exclusion shapes

| Shape | Fields | Inside when |
|---|---|---|
| box | centre, half extents (XZ), yaw | the point in the box's frame is within the half extents |
| disc | centre, radius | the distance is below the radius |
| polyline | up to 1,024 XZ points, width | the distance to any segment is below half the width |

Shapes come from the caller: the cook gathers them from the scene's `Zenith_PlacementExclusionComponent` entities (roads and footprints authored in the world, WORLD's layers) and the terrain document's exclusion masks.

## Cross-cell determinism

Spacing and exclusion look across cell borders. To keep each cell's result a function of the inputs alone, each stage works on a ring of cells around the requested cells, computed before any stage runs:

1. The reach of stage g is `ρ(g) = ceil(max(minDistance(g), exclusionRadius(g) + F) / 128)`, where F is the largest footprint radius of the run's foliage types (at most 64 m) and the exclusion term is 0 when the stage does not exclude; with the document limits (`minDistance` ≤ 64 m, `exclusionRadius` ≤ 64 m) ρ ≤ 1 for every stage.
2. The kept-ring radius `K(g)` is the number of rings around the requested cells over which stage g's **kept** instances are needed: `K(last) = 0`; walking the stages backwards, `K(g) = max(K(g + 1), max over subsequent excluding stages t of (K(t) + ρ(t)))`, so an excluding stage t needs every earlier stage's kept instances out to its own ring plus its reach.
3. Stage g computes survivors on `K(g) + ρ(g)` rings (a spacing decision needs the survivors within `minDistance`), and spacing decisions and cross-rule removals on `K(g)` rings (a removal needs the earlier stages' kept instances within `exclusionRadius + F`, which their larger K covers). The bound rests on the order of "Same-rule spacing": spacing first, then exclusion. Were exclusion applied first, a spacing decision on ring K(t) + ρ(t) would need the earlier stages' kept instances out to K(t) + 2ρ(t) rings, and the bound would grow to 17 rings.
4. Each (stage, cell) result is computed once and memoised in the arena; results outside the requested cells are discarded after the last stage.

With at most 8 excluding stages of reach 1, `K ≤ 8` and a stage's work spans at most 9 rings. Every cell a stage computes is a function of the same inputs whether or not it was requested, so a requested cell's output never depends on which other cells were requested.

## Determinism argument

For a fixed build, the output for a cell is identical for any worker count, any order of `m_axCells` and any partition of the work, because:

1. every random value is a counter-based draw keyed by (seed, rule set, rule, cell, candidate, k) ("Seeded streams"), so no draw depends on another draw's consumption;
2. the survivors of a (rule, cell) are a pure function of that cell's candidates and the surface, which is a pure read;
3. the kept decision of a survivor depends on the survivors within a bounded radius, compared by a total order (priority, then position in the grid), never on visit order;
4. each rule stage reads only the complete results of earlier stages, finished before it starts (a join after each stage's `ParallelFor`);
5. each cell's instances are written into that cell's own output slot and sorted by instance key, which orders by (rule set, rule, candidate), before return;
6. floating-point operations per candidate are the same sequence on every worker under the fixed floating-point environment ([Z2-N-CONTRACT-360], [Z2-N-ARCH-883]).

`Test_Placement_WorkerCountInvariant` checks the consequence with 1, 2, 8 and all workers and shuffled cell orders; `Test_Placement_CellSubsetMatchesFullRun` checks that placing one cell alone equals that cell of a full run, including a fixture where a survivor removed by cross-rule exclusion suppresses a neighbour by spacing across a cell border. Equality across platforms or builds is not promised ([Z2-N-ARCH-882]); the cook runs on Windows, so cooked foliage is identical everywhere it ships.

## Output ordering and capacity

- The instance key of a placed instance is `Zenith_PlacementInstanceKey`, a `uint32` of disjoint bit fields; an explicit instance's key is `Zenith_ExplicitInstanceKey(id)`:

| Bits | Field | Range | Why it fits |
|---|---|---|---|
| 31 | explicit flag | 0 placed, 1 explicit | explicit IDs are below 2^31 (an `m_uNextExplicitID` reaching 2^31 refuses the next scatter with `ENV_ERROR_CAPACITY`) |
| 27–30 | rule-set index | 0 … 15 | `uENV_TERRAIN_MAX_PLACEMENT_RULESETS` = 16 |
| 21–26 | rule index within the set | 0 … 63 | `uENV_PLACEMENT_MAX_RULES_PER_SET` = 64 |
| 0–20 | candidate index | 0 … 2^21 − 1 | a cell holds at most `ceil(128 / a)²` ≤ 1,048,576 = 2^20 candidates (a ≥ 0.125 m at 64 per m²) |

- The key is collision-free within a cell: the fields are disjoint, so two placed instances with equal keys have equal (set, rule, candidate), which names one candidate of one stage, and a candidate is kept at most once; a placed key has bit 31 clear and an explicit key has it set, and explicit IDs are unique within their tile, which is the cell. The render key (`Zenith2/Docs/Design/ENV/Foliage.md → "Render instance keys"`) adds the cell and the world tag, so it is unique per world up to the hash's collision probability.
- A cell's instances are sorted by instance key.
- A cell exceeding `m_uMaxInstancesPerCell` keeps its first instances in that order, sets `ENV_ERROR_PLACEMENT_CAPACITY` naming the cell, and the cook fails the import with `COOK_ERROR_IMPORT_FAILED` and a diagnostic naming `ENV_ERROR_PLACEMENT_CAPACITY` and the cell (content must be fixed, not truncated silently); runtime callers receive the truncated result with the error and decide.

## Runtime and editor use

| Caller | Surface | Seed | Cells | Output |
|---|---|---|---|---|
| the foliage cook step | the terrain document | the terrain's `m_ulPlacementSeed` | every tile of the terrain, in one `Zenith_Place` over all the terrain's rule sets | foliage cell files ("Foliage cook" in Bake.md) |
| the editor's placement preview | the terrain document | the same | the tiles in view, re-run on dirty regions | debug draws and the preview instance deltas |
| the scatter brush | the scatter surface adapter over the terrain document (`Zenith2/Docs/Design/ENV/Terrain.md → "Scatter brush"`) | `Zenith_Hash64U64(dabIndex, m_ulNoiseSeed)` | the cells the dab's disc touches | explicit instances |
| a game at world load (WORLD orchestration) | the runtime terrain resource | the world seed | the cells WORLD requests | instances handed back to WORLD, which spawns or bakes them |

Every caller uses the same `Zenith_Place`, so the editor preview shows exactly what the cook bakes.

The runtime terrain resource keeps only heights, holes and the dominant physical material per quad (the cooked tile data), so its surface adapter `Zenith_TerrainRuntimePlacementSurface` answers fail-visibly instead of inventing values: `GetMissingInputs` returns `LAYERS | DENSITY | EXCLUSION_MASKS`, and `Zenith_Place` refuses, before any candidate, a stage whose rule lists layers over a surface missing `LAYERS` or sets `useDensityMap` over a surface missing `DENSITY` (`ENV_ERROR_PLACEMENT_RULE_INVALID`, `m_uIndex` the global stage, `m_uIndex2` the missing bit). A surface missing exclusion masks is accepted, because a runtime caller passes its roads and pads as `m_axExclusions` shapes; every sample taken over it counts in `m_ulSamplesWithoutMasks`, which WORLD reports. `IsUnderWater` reads the world's `Zenith_WaterWorldResource` (`Zenith_SampleGerstnerSurface` height against the point's terrain height) when the adapter is given one, and is false for a world with no water body.

## Boundary with WORLD procedural generation

| ENV owns | WORLD owns |
|---|---|
| the placement algorithm, its determinism and its tests | which rule sets apply to which world partitions and data layers |
| the rule and foliage-type documents | when placement re-runs after an edit, and which partition locks it takes under concurrent editing |
| the surface, exclusion and obstacle inputs' semantics | baking placement results into partition content and the bake workflow's UI |
| per-cell outputs and their canonical order | reproducibility of whole-world generation from a world seed (the orchestration that calls `Zenith_Place` with it) |
