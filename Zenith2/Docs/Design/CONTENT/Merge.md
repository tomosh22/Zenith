# CONTENT design: the structure-aware three-way merge driver

This file is part of the CONTENT design family; the entry document is `Zenith2/Docs/Design/CONTENT.md`. It specifies the semantic merge of text authoring documents: the merge engine in `ZenithContentCore` (`Zenith_MergeDocuments3`, `Zenith_MergeText3`), the per-type merge policies in `ZenithAuthoring`, the driver command `ZenithContent merge` that git invokes, the conflict-marker format, the scene, prefab, schema and environment rules, the text fallback and the git integration. Canonical serialization is SCENE's (`Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Canonical serialization"`), delivered in Stage 2 (inventory item Z2-I-CONTENT-28); this file consumes it.

## Scope of this file

| Topic | Heading |
|---|---|
| the C++ surface | "Merge API" |
| how git and the editor call it | "Driver command line", "Git integration" |
| what the inputs and outputs are | "Canonical inputs and outputs" |
| the algorithm | "Structural merge algorithm", "Keyed array merge", "Set merge" |
| per document type | "Merge policies", "Scene and prefab rules", "Schema rules", "Environment document rules" |
| conflicts | "Conflict markers", "Text fallback" |
| guarantees | "Merge determinism and limits" |

<!-- informative -->
A line-based merge of two branches that each add an entity to one scene already succeeds, because SCENE writes each identified child on its own sorted line. What a line merge gets wrong is everything structural: two edits to different fields of one component on adjacent lines conflict for no reason; an edit beside a deletion merges into a document whose child names a deleted parent; two branches that both bump a schema's version and add a field produce valid-looking JSON with one migration silently lost. The driver merges by the document's structure, so it resolves the first, and refuses with an explicit marker the cases a person must decide.
<!-- /informative -->

## Merge API

```cpp
// Zenith2/Tools/ContentCore/Zenith_ContentMerge.h
/// @contract enum Zenith_ContentMergeMode
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. Tool-side.
/// @endcontract
enum Zenith_ContentMergeMode : uint8
{
	CONTENT_MERGE_MODE_STRUCTURAL = 1,   // "Structural merge algorithm" over the policy's schema
	CONTENT_MERGE_MODE_WHOLE_FILE,       // either side unchanged takes the other; otherwise the whole file is one conflict
	CONTENT_MERGE_MODE_TEXT,             // "Text fallback"
};

/// @contract enum Zenith_ContentArrayPolicy
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. Tool-side.
/// @endcontract
enum Zenith_ContentArrayPolicy : uint8
{
	CONTENT_ARRAY_POLICY_ATOMIC = 1,     // the array is one value
	CONTENT_ARRAY_POLICY_KEYED,          // elements are objects identified by a key member ("Keyed array merge")
	CONTENT_ARRAY_POLICY_SET,            // elements are scalars forming a set ("Set merge")
};

/// @contract value Zenith_ContentArrayRule
/// Ownership: constexpr data of a policy.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: A JSON-pointer pattern: "/" separated; "*" one member or element; "**" any number of levels.
/// Errors: None; a pattern that matches no node does nothing.
/// Version: since Z2 Stage 4. Tool-side.
/// @endcontract
struct Zenith_ContentArrayRule
{
	const char* const m_szPointerPattern = nullptr; // "/fields", "/structs/*/fields", "/migrations", "/retiredFieldIds"
	const Zenith_ContentArrayPolicy m_ePolicy = {};
	const char* const m_szKeyMember = nullptr; // KEYED: "id", "name", "fromVersion"; null otherwise
};

/// @contract value Zenith_ContentMergePolicy
/// Ownership: constexpr data of the policy table, or built by Zenith_BuildContentMergePolicies; the schema pointer is borrowed.
/// Thread affinity: IMMUTABLE.
/// Invalidation: With the registry the policy was built from.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. Tool-side.
/// @endcontract
using Zenith_PfnContentMergeValidate = bool (*)(const Zenith_JsonDocument& xMerged, Zenith_Array<uint32>& auConflictNodesOut,
	Zenith_Arena& xArena);      // a semantic check: false with the nodes that must become conflicts
struct Zenith_ContentMergePolicy
{
	const char* const m_szName = nullptr; // the document type's stable name, or "json" for the default
	const char* const m_szSuffix = nullptr; // ".scene.json"
	const Zenith_ContentMergeMode m_eMode = {};
	const Zenith_CanonicalSchema* const m_pxSchema = nullptr; // STRUCTURAL: the type's canonical schema, or null (then "Canonical inputs and outputs" rule 4)
	const uint32 m_uSchemaVersion = 0;  // the registry's current version of the type; 0 for the default json policy (no version check)
	const char* const m_szVersionMember = nullptr; // the member holding a document's version ("schemaVersion" for most types); null with version 0
	const Zenith_Span<const Zenith_ContentArrayRule> m_axArrays = {};
	const Zenith_PfnContentMergeValidate m_pfnValidate = nullptr; // null: no semantic check
	const bool m_bInstanceRules = false; // the scene and prefab rules of "Scene and prefab rules"
};

/// @contract value Zenith_ContentMergeInput
/// Ownership: Plain value; the byte spans are borrowed for the call.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: None.
/// Version: since Z2 Stage 4. Tool-side.
/// @endcontract
struct Zenith_ContentMergeInput
{
	const Zenith_ByteSpan m_xBase = {};  // the common ancestor; empty for an add/add merge
	const Zenith_ByteSpan m_xOurs = {};
	const Zenith_ByteSpan m_xTheirs = {};
	const Zenith_StringView m_strPath = {}; // for messages
	const uint32 m_uMarkerSize = 7;     // git's %L
	const Zenith_ContentMergePolicy* const m_pxPolicy = nullptr; // null: the text fallback
	const Zenith_ContentClock m_xClock = {}; // the text fallback's time bound; null members: Zenith_GetSystemContentClock()
	const uint32 m_uMaxMilliseconds = 30000; // the text fallback's time bound ("Text fallback" rule 5); 0: no bound
};

/// @contract value Zenith_ContentMergeResult
/// Ownership: Plain value; the decision list points into the caller's arena.
/// Thread affinity: IMMUTABLE once returned.
/// Invalidation: With the arena.
/// Units: Counts.
/// Errors: None.
/// Version: since Z2 Stage 4. Tool-side.
/// @endcontract
struct Zenith_ContentMergeDecision { const Zenith_StringView m_strPointer = {}; const uint8 m_uOutcome = 0; };   // 1 ours, 2 theirs, 3 both, 4 deleted, 5 conflict
struct Zenith_ContentMergeResult
{
	const Zenith_ContentMergeMode m_eModeUsed = {}; // TEXT when the structural path fell back
	const uint32 m_uConflicts = 0;
	const uint32 m_uFallbackReason = 0;   // 0 none; 1 unparsable input; 2 newer schema version; 3 over a node limit; 4 no policy;
	                                       // 5 an input over 64 MiB or not UTF-8 (whole-file block); 6 the text merge's edit or time bound
	const Zenith_Span<const Zenith_ContentMergeDecision> m_axDecisions = {}; // for --explain, document-pointer order
};

/// @contract functions Zenith_ContentMergeFunctions
/// Ownership: The merged bytes are appended to the caller's array.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: CONTENT_ERROR_OUT_OF_MEMORY only. An input over 64 MiB or not valid UTF-8 is not an error: the output is one whole-file
///   conflict block (fallback reason 5, "Merge determinism and limits"); a structural failure falls back to the text merge. The
///   three inputs are at one schema version: the caller migrates them first ("Canonical inputs and outputs" rule 1), because
///   ContentCore holds no migration registry; an input whose m_szVersionMember is not an integer equal to m_uSchemaVersion
///   falls back to the text merge (reason 2), so a caller that skipped the migration gets a text merge, never a wrong tree.
/// Version: since Z2 Stage 4. Tool-side.
/// @endcontract
inline constexpr uint64 ulCONTENT_MERGE_MAX_INPUT_BYTES = 67108864ull;   // 64 MiB: above it, or not UTF-8, one whole-file block
inline constexpr uint32 uCONTENT_MERGE_MAX_EDIT_DISTANCE = 10000;        // "Text fallback" rule 5
Zenith_Result<Zenith_ContentMergeResult, Zenith_ContentError> Zenith_MergeDocuments3(const Zenith_ContentMergeInput& xInput,
	Zenith_Array<uint8>& auMergedOut, Zenith_Arena& xArena);
Zenith_Result<Zenith_ContentMergeResult, Zenith_ContentError> Zenith_MergeText3(const Zenith_ContentMergeInput& xInput,
	Zenith_Array<uint8>& auMergedOut, Zenith_Arena& xArena);

// Zenith2/Tools/Authoring/Zenith_AuthoringContent.h
/// @contract functions Zenith_ContentMergePolicyFunctions
/// Ownership: The policies are written into the caller's array; their schema pointers borrow the frozen registry.
/// Thread affinity: The registry's owner thread.
/// Invalidation: With the registry.
/// Units: None.
/// Errors: CONTENT_ERROR_OUT_OF_MEMORY.
/// Version: since Z2 Stage 4. Tool-side.
/// @endcontract
// Fills m_uSchemaVersion and m_szVersionMember of each type's policy from the registry's current version and version key.
Zenith_Result<void, Zenith_ContentError> Zenith_BuildContentMergePolicies(const Zenith_MigrationRegistry& xRegistry,
	Zenith_Array<Zenith_ContentMergePolicy>& axOut);
const Zenith_ContentMergePolicy* Zenith_FindContentMergePolicy(Zenith_Span<const Zenith_ContentMergePolicy> axPolicies,
	Zenith_StringView strPath);   // longest matching suffix; null when none
```

## Driver command line

```text
ZenithContent merge --ancestor <O> --ours <A> --theirs <B> [--marker-size <L>] [--path <P>] [--max-seconds <s>] [--explain]
ZenithContent merge --install [--repo <dir>]
ZenithContent merge --print-gitattributes
```

1. The merge form reads the three files whole (a missing ancestor file is an empty base; a file above 2 GiB is not read: see rule 2), builds the migration registry with `Zenith_RegisterEditorDocumentTypes` (pending EDIT, row CT18) and the policies with `Zenith_BuildContentMergePolicies`, finds the policy by `--path` (git's `%P`), migrates the three inputs to the registry's version ("Canonical inputs and outputs" rule 1), runs `Zenith_MergeDocuments3`, and writes the result over `<A>` atomically (`<A>.z2merge.tmp`, flush, replace), as git expects of a merge driver.
2. Exit code 0: merged with no conflict; 1: the result contains conflict markers (every input that is over 64 MiB or not valid UTF-8 included, as one whole-file block: `<A>` is never left holding ours alone, so a `git add` cannot drop theirs silently; a file above 2 GiB makes `<A>` the single block `<<<<<<< ours` / `file too large to merge (<bytes> bytes): resolve with git checkout --ours or --theirs` / `>>>>>>> theirs`); 2: bad command line. Git treats any non-zero exit as a conflict.
3. `--explain` prints each decision of `m_axDecisions` (`<pointer>: ours|theirs|both|deleted|conflict`) and the fallback reason to standard output; the editor's "Open merge tool" shows it (row CT18).
4. `--install` runs `git config --local merge.zenith-document.name "Zenith2 structure-aware document merge"` and `git config --local merge.zenith-document.driver "<absolute path of ZenithContent> merge --ancestor %O --ours %A --theirs %B --marker-size %L --path %P"` in the repository (`--repo`, default the repository containing the executable); exit 0, or 1 with git's message.
5. `--print-gitattributes` prints one line `*<suffix> merge=zenith-document` per policy suffix, sorted, the block "Git integration" requires, leaving out every suffix that `binaryExtensions` of `Zenith2/Tools/ZenithContent/Internal/Config/storage.config.json` lists (those are LFS-tracked, "Merge policies").

## Canonical inputs and outputs

1. **Migration is the driver's, not ContentCore's.** The driver (`ZenithContent merge`, and EDIT's "Open merge tool", which runs the same command) parses each input with `Zenith_JsonDocument` under the JSON limits and, for a document whose `schemaVersion` (or the type's version member) is older than the registry's, migrates it in memory through the type's chain with a call-local `Zenith_DocumentMigrator` over a `Zenith_MemoryDocumentStorage` holding the input (`Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Zenith_DocumentMigrator header"`), then hands `Zenith_MergeDocuments3` the migrated canonical bytes, so the three versions are compared at one version; ContentCore's merge depends on no registry. A version newer than the registry's falls back to the text merge (reason 2), because the tool cannot know the newer structure; an input whose migration fails falls back with reason 1 and the step's error in `--explain`.
2. Before comparison each input is re-serialized canonically and re-parsed, so formatting differences (an older tool's key order, a non-canonical file) are never conflicts.
3. The merged tree is written with `Zenith_WriteCanonicalJson` and the type's schema, so a merge introduces no formatting churn, and a merge of canonical inputs with disjoint edits is byte-identical to applying both edits through the editor (`Test_ContentMerge_DisjointEntityAddsEqualSequentialEdits`).
4. A structural policy without a canonical schema (`m_pxSchema` null; for example schema documents, "Schema rules") orders members as ours does, then members only theirs added, in theirs' order, and writes with CORE's pretty writer (`Zenith2/Docs/Design/CORE/Text.md → "JSON writer"`).
5. Unknown members are merged like known ones, by key; their position follows rule C3 of the canonical rules.

## Structural merge algorithm

`M(o, a, b)` merges three subtrees at one document pointer; `≡` is equality of canonical bytes; absent members are ⊥.

1. If a ≡ b: a. If a ≡ o: b. If b ≡ o: a. (Every node takes this shortcut first, so an untouched subtree costs one comparison.)
2. Otherwise by node kind (the rule of the schema at that pointer, or the JSON kind when there is no schema):
   - **OBJECT, ID_MAP, KEYED_MAP, FIELD_SET**: for each key k of keys(o) ∪ keys(a) ∪ keys(b), in output order:

     | o[k] | a[k] | b[k] | Result |
     |---|---|---|---|
     | ⊥ | x | ⊥ | x (ours added) |
     | ⊥ | ⊥ | y | y (theirs added) |
     | ⊥ | x | y | x when x ≡ y; else, when both are objects or maps, M(∅, x, y) with an empty base of the same kind; else conflict |
     | z | ⊥ | ⊥ | deleted |
     | z | x | ⊥ | deleted when x ≡ z; else conflict (modify/delete) |
     | z | ⊥ | y | deleted when y ≡ z; else conflict (delete/modify) |
     | z | x | y | M(z, x, y) |

   - **INLINE_ARRAY, VALUE, OPAQUE**, and an ORDERED_ARRAY whose policy is ATOMIC: conflict (the shortcut of step 1 has already taken every one-sided change).
   - **ORDERED_ARRAY** with a KEYED or SET policy: "Keyed array merge", "Set merge".
3. A conflict becomes a conflict node at that pointer carrying the three subtrees (any may be ⊥); the merge continues elsewhere, so one run reports every conflict.
4. After the walk, the policy's semantic check (`m_pfnValidate`) may turn further nodes into conflicts ("Scene and prefab rules"); then the output is written ("Conflict markers").

Complexity O(n log n) in the total member count (sorting map keys), memory O(n) for the three trees and the result; every node is visited once.

## Keyed array merge

An ordered array whose policy is KEYED, at a pointer matching a rule, merges its elements as a map keyed by the key member, and its order separately:

1. Every element of o, a and b must be an object with the key member, and keys must be unique within each array; otherwise the array is ATOMIC (and a conflict when changed on both sides).
2. Elements: the member-wise table of "Structural merge algorithm" over keys, recursing into element objects.
3. Order, over the surviving keys S: let `ord(x)` be array x's key sequence restricted to S.
   - If ord(a) restricted to keys present in o equals ord(o) restricted likewise (ours reordered nothing) and the same holds for b, the order is ord(o) with each key new in a inserted directly after its nearest preceding key of a present in the output (at the front when none), then each key new in b after its nearest preceding key of b; two insertions after one predecessor are placed ours first.
   - If exactly one side reordered common keys, its relative order of the common keys is kept and the other side's new keys are inserted as above.
   - If both sides reordered common keys differently, the whole array is one conflict node (an array-ordering conflict, inventory item Z2-I-CONTENT-29).
4. Complexity O(k log k) for k elements (key lookups through a sorted index).

## Set merge

An ordered array whose policy is SET (sorted sets of scalars: `removedEntities`, each `removedComponents` list, `retiredFieldIds`, `.meta`-style `labels`):

- x is in the result iff (x ∈ a and x ∈ b) or (x ∈ a and x ∉ o) or (x ∈ b and x ∉ o); the result is written sorted.
- A set merge never conflicts.

## Merge policies

`Zenith_BuildContentMergePolicies` produces one policy per registered document type with a suffix, plus the built-in rows below, and a default `json` policy (STRUCTURAL, no schema, no array rules) for any other `.json` the attributes route to the driver:

<!-- elaboration -->
| Type (suffix) | Mode | Schema | Array rules | Semantic check |
|---|---|---|---|---|
| `Zenith.SceneDocument` (`.scene.json`), `Zenith.PrefabDocument` (`.prefab.json`) | STRUCTURAL | SCENE's (`Zenith_GetSceneCanonicalSchema`, `Zenith_GetPrefabCanonicalSchema`) | `/**/removedEntities` SET; `/**/removedComponents/*` SET | "Scene and prefab rules" |
| component schemas (`.schema.json`) | STRUCTURAL | none (rule 4) | "Schema rules" | "Schema rules" |
| `Zenith.Environment.TerrainTileDocument` (`.terraintile.json`) | WHOLE_FILE | — | — | — |
| `Zenith.Environment.TerrainDocument` (`.terrain.json`) | STRUCTURAL | ENV's | "Environment document rules" | — |
| every other registered type (materials, banks, UI documents, presets, …) | STRUCTURAL | the type's | arrays of objects carrying `id`, else `name`, else `key`, else `guid`, in every element of all three versions, are KEYED by that member; other arrays ATOMIC | — |
| `Zenith.AssetMeta` (`.meta`) | STRUCTURAL | ASSET's | `/subAssets` KEYED by `key` (each entry's owner members, `exclude`, `materialRemap`, `annotation`, `rig`, merge member-wise as OBJECT members; two branches giving one new key different GUIDs conflict); `/labels` SET | — |
| `Zenith.World.WorldDocument`, `Zenith.World.GeneratorDocument` | STRUCTURAL | WORLD's | their keyed maps of layers, regions, generators and outputs, by WORLD's canonical schemas | — |
| `Zenith.World.LightBakeDocument`, `Zenith.World.LightPageDocument` | WHOLE_FILE | — | — | — (bake outputs are regenerated, never combined) |

The default "other arrays" rule is exactly ENV's request for its other documents (row EN20); the WORLD rows answer WORLD's rows WD16 and WD27 (`Zenith2/Docs/Design/WORLD/Editing.md → "Merge conflicts in WORLD documents"`; answered by rows CT29 and CT31; WORLD's derived navigation documents exist only inside the import and are never files, so no policy names them); the policy list is data, so a new document type is covered the day it registers.

A suffix that `storage.config.json`'s `binaryExtensions` lists (`Zenith2/Docs/Design/CONTENT/SharedCache.md → "Storage policy check"`) is LFS-tracked and never merged: `--print-gitattributes` leaves it out of the merge block even when a policy names it, so its one `.gitattributes` rule is the LFS rule. While WORLD's row CT31 is open, the two bake document types keep their WHOLE_FILE policy for checkouts whose attributes do not track them in LFS.

## Scene and prefab rules

The generic algorithm already gives SCENE's per-property granularity (`Zenith2/Docs/Design/SCENE/Prefabs.md → "Override semantics per property kind"`): entities and instances merge per ID, components per stable name, fields per property ID, struct fields per nested ID, override blocks per (target, stable name, property path); vectors, quaternions, colours, fixed arrays and containers are atomic values whose two-sided change is a conflict, written as an explicit textual-conflict marker. The policy adds:

| Rule | Detection | Result |
|---|---|---|
| S1 prefab swap with edited overrides | an instance whose `prefab` (or a prefab whose `variantOf`) differs between o and one side, while the other side changed that instance's `overrides` (or the `base` block) | the whole instance (or the `variantOf` and `base` members) is one conflict node: the overrides target the old prefab's resolved ID space, so no member-wise combination is meaningful (nested-prefab overrides fall back to a marker, Z2-I-CONTENT-29) |
| S2 conflicting nested override | two sides set different values for one (target, stable name, property path) of an override block, or one removes a target's override block member the other changes | a conflict node at that override member (the generic table) |
| S3 orphaned child | the merged document holds an entity or instance whose `parent` is an ID that o had, one side deleted, and the other side kept as a parent of a child it added or moved | the deleted parent's member becomes a conflict node (ours' and theirs' versions of it), so the person chooses to keep the parent or move the child |
| S4 duplicate sort keys | two siblings in the merged document with equal `sortKey` | not a conflict: equal keys are valid and canonical (`Zenith2/Docs/Design/SCENE/Format.md → "Sort keys"`); the driver's `--explain` notes it and the editor rebalances on the next placing edit |
| S5 root change | a prefab's `root` differs on both sides | conflict node (generic) |

`Test_ContentMerge_SceneRules` asserts one fixture per rule; `Test_ContentMerge_OverrideKindsMatchSceneTable` walks SCENE's per-property-kind table and asserts the outcome for each kind changed on one side, changed identically on both, and changed differently on both.

## Schema rules

Component schema documents (`Zenith2/Docs/Architecture/DataDefinedComponents.md → "The schema file"`) carry identity in array members:

| Pointer | Policy | Key |
|---|---|---|
| `/fields`, `/structs/*/fields` | KEYED | `id` |
| `/structs` | KEYED | `name` |
| `/migrations` | KEYED | `fromVersion` |
| `/retiredFieldIds` | SET | — |

Semantic check (S6): after the merge, a field `id` present in `retiredFieldIds` (one side retired it while the other edited it), or two fields with one `name`, or a `version` lower than the highest `fromVersion` + 1, makes the involved members conflict nodes; two sides that both bump `version` to the same value while adding different migrations conflict naturally on the keyed `migrations` element. A field ID reused for different content conflicts as a keyed element, so a merge can never reuse an ID silently (`Test_ContentMerge_SchemaRules`).

## Environment document rules

As ENV requests (row EN20; `Zenith2/Docs/Design/ENV/Bake.md → "Merging terrain documents"`): `*.terraintile.json` merges by file (WHOLE_FILE: a tile changed on both sides is one conflict resolved by taking one side whole, because its payloads are compressed blobs); `*.terrain.json` merges structurally with its tile, layer, foliage-type and rule-set arrays KEYED by tile coordinates (`x`, `z` concatenated as the key `"<x>,<z>"`), layer IDs, names and GUIDs, and an order change of those lists on both sides is an ordering conflict (`Test_ContentMerge_EnvironmentRules`).

<!-- assumption: pending ENV -->
The member names of the terrain document's lists (the tile key members `x` and `z`, layer `id`, foliage-type and rule-set `name`) are taken from ENV's row EN20; ENV's document schema is the authority, and the rule table follows it when it differs.
<!-- /assumption -->

## Conflict markers

The output with conflicts is the canonical document in which each conflict node is written as a block of lines:

```text
<<<<<<< ours
		"9b2e7c41-5d0a-4f3e-8c6b-2a1d9e7f0b54": {
			"name": "Sun",
			…
		},
||||||| base
		…
=======
		"9b2e7c41-5d0a-4f3e-8c6b-2a1d9e7f0b54": {
			"name": "Moon",
			…
		},
>>>>>>> theirs
```

1. The writer serializes the merged tree with a unique placeholder value at each conflict pointer (a conflict whose merged member is absent gets a placeholder member under its key, so the canonical writer places it at its sorted position), then replaces each placeholder's line with the block.
2. Each section is that side's subtree written by the canonical writer at the placeholder's indentation, prefixed with `"<key>": ` for a member (nothing for an array element), with a trailing comma exactly when the placeholder's line had one; a side where the node is ⊥ has an empty section.
3. Marker lines: `<` × L then ` ours`; `|` × L then ` base`; `=` × L; `>` × L then ` theirs`, with L the marker size (git's `%L`, default 7), each at column 0.
4. The document then fails to parse, so the editor opens it CONFLICTED (`Zenith2/Docs/Design/EDIT/Pipeline.md → "Diff revert and resolve"`), and git reports the conflict; the conflict count is the number of blocks.

## Text fallback

`Zenith_MergeText3` is diff3 over lines, used for a path with no policy, an input that does not parse, a newer schema version, an input over the limits, and WHOLE_FILE's two-sided case (as one block):

1. Split each input into lines (LF; a final line without LF kept).
2. Compute the longest-common-subsequence matchings o↔a and o↔b with Myers' O((N + M)·D) algorithm in its linear-space form (the middle-snake divide and conquer of Myers' paper, section 4b), which stores two diagonal vectors of 2D + 1 entries per recursion level instead of the whole trace, so memory is O(N + M) whatever D is; at D = 10,000 a stored trace would hold about 2 × 10⁸ entries (800 MB) inside git's synchronous merge. It is the same diff `Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Zenith_DocumentDiff header"` uses, implemented in ContentCore because the framework's diff is a two-way presentation type. The matching is a pure function of the two inputs because every choice among equal-cost matchings is fixed by these rules, which an implementation follows exactly (the goldens of `Test_ContentTextMerge_Diff3Cases` fix them):
   1. Over a range of o (length N) and of the other input (length M), the common prefix is matched first, then the common suffix; an empty remainder ends the range.
   2. Otherwise the range is split at the middle snake of Myers' section 4b, searched for d = 0, 1, … with a forward pass and then a reverse pass at each d. On diagonal k a pass extends from diagonal k + 1 (taking a line of the other input) when k = −d, or when k ≠ d and V[k − 1] < V[k + 1], and from diagonal k − 1 (taking a line of o) otherwise, so a tie goes to the deletion first; the reverse pass applies the same rule to lines counted from the ends of the range; both V arrays start with V[1] = 0, and V[k] holds the furthest x (lines of o consumed, from the start or from the end) on diagonal k. With Δ = N − M, the forward pass tests for overlap when Δ is odd, after each diagonal k with Δ − k in [−(d − 1), d − 1], and the reverse pass when Δ is even, after each diagonal k with Δ − k in [−d, d]; an overlap is V_forward[k_f] + V_reverse[k_r] ≥ N for a forward diagonal k_f and the reverse diagonal k_r = Δ − k_f, and the first overlap found makes the snake just walked by that pass the middle snake.
   3. The part before the snake is matched, then the snake's diagonal, then the part after, each part by rule 1.

   For example o = `a`, `b` against `b`, `a` matches the two `b` lines (`a` deleted, then `a` inserted), and o = `x`, `a`, `x` against `x` matches the first `x` (the common prefix).
3. Walk o: a line of o matched in both a and b is stable; between consecutive stable lines lie unstable chunks (o_i, a_i, b_i).
4. For each unstable chunk: a_i = o_i → b_i; b_i = o_i → a_i; a_i = b_i → a_i; otherwise a conflict block with the three chunks.
5. Bounds: Myers' algorithm costs O((N + M) · D), so it stops when D passes `uCONTENT_MERGE_MAX_EDIT_DISTANCE` = 10,000 lines, or when `m_uMaxMilliseconds` (default 30,000; `--max-seconds <s>` on the driver) has elapsed on the input's clock, read every 4,096 diagonal steps; either makes the whole file one conflict block (fallback reason 6). At the 64 MiB input limit (about 10⁶ lines per side) the edit bound caps the work at about 2 × 10¹⁰ steps (the linear-space form at most doubles it), and the time bound keeps git's synchronous wait at 30 s whatever the input.

## Git integration

- `Zenith2/.gitattributes` (an ordinary, unprotected file; Z2-D-108's `Docs/Board/** -text` line stays) carries the block `--print-gitattributes` prints, one `*<suffix> merge=zenith-document` line per policy suffix, between the comment lines `# BEGIN zenith-document merge` and `# END zenith-document merge`; `Test_ContentMerge_GitAttributesListEveryPolicySuffix` compares the block with the policies of a registry holding every registered type, minus the `binaryExtensions` suffixes.
- The driver command is per-clone git configuration (git never takes driver commands from tracked files); SHIP's setup script runs `ZenithContent merge --install` after building the tools (pending SHIP, row CT16). Without it, git falls back to its own line merge for those paths; the editor's "Open merge tool" runs the driver directly on `git show :1:<path>`, `:2:` and `:3:` (`Zenith2/Docs/Design/EDIT/Pipeline.md → "Diff revert and resolve"`).
- LFS-tracked binaries are never routed to the driver (their attributes are `merge=lfs` or `-text`).
- `Test_ContentPipeline_GitMergeDriverEndToEnd` creates a temporary repository with the attributes and the installed driver, makes two branches editing one scene (disjoint entity edits, one field change each on one component, and one S3 case in a second file), runs `git merge`, and asserts the first file merged cleanly and canonically and the second holds exactly one conflict block.

## Merge determinism and limits

- The output is a pure function of the three inputs, the policy and the marker size, with one exception: when the text fallback's time bound fires (reason 6 by time, rule 5 of "Text fallback"), the result is the one whole-file block, so whether a slow text merge completes depends on the clock; `--explain` reports that reason and the elapsed time, so a clock-dependent result is never silent, while the edit-distance bound is a function of the inputs alone. No host and no hash-map iteration order reaches the output (maps are walked in key order); swapping ours and theirs swaps only the sections of conflict blocks and the insertion order of rule 3 of "Keyed array merge" (`Test_ContentMerge_DeterministicAndSymmetric`).
- Idempotence: merging a document with itself as all three inputs returns its canonical bytes; merging (o, a, o) returns a's canonical bytes.
- Limits, one rule each: an input over 64 MiB (the JSON limit) or not valid UTF-8 makes the whole file one conflict block with ours and theirs sections and no base section (reason 5), exit 1, so the person always sees both sides; an input within 64 MiB whose tree exceeds the JSON node limits merges by the text fallback (reason 3); the text fallback's own bounds are rule 5 of "Text fallback" (reason 6).
- The time of a 16 MiB scene merge is the pending row "Semantic merge time of a 16 MiB scene" (`Zenith2/Docs/Design/CONTENT.md → "Pending new rows"`).
