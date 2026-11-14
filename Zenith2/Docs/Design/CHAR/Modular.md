# CHAR design: modular characters and hair cards

Section file of `Zenith2/Docs/Design/CHAR.md`. It specifies how a character is assembled from parts that share one skeleton: the assembly document, body regions and the rule that replaces mesh hiding, slots and loadouts, the generation of SCENE prefab documents (a base prefab and one variant per loadout) with stable derived entity IDs, the protection of generated documents against hand edits, the assembly validation rules, rigid attachments, the part caps and their cost, and the classification and authoring rules of hair alternatives. The runtime mechanism every part relies on is ANIM's pose follower (`Zenith2/Docs/Design/ANIM/Runtime.md → "Pose followers"`): one animator evaluation drives every part mesh; CHAR adds no runtime code.

## Scope of this file

| Topic | Heading |
|---|---|
| the assembly model | "Assembly model" |
| the document | "Assembly document" |
| covering the body without hiding triangles | "Body regions" |
| slots and loadouts | "Slots and loadouts" |
| what the generator writes | "Generated prefab layout", "Generated entity identity", "Loadout variants" |
| generating and regenerating | "Prefab generation algorithm", "Hand-edit protection", "Keep as variant algorithm" |
| checks | "Assembly validation rules" |
| props on sockets | "Rigid attachments" |
| caps and costs | "Part caps and cost" |
| switching outfits in a game | "Runtime part switching" |
| hair | "Hair alternatives classification", "Hair card authoring rules" |

## Assembly model

<!-- elaboration -->
A modular character is one skeleton, one animation graph and N part meshes, each skinned to that skeleton in its one rest pose (`Zenith2/Docs/Design/ANIM/Formats.md → "Skeleton validation"`, SV9; GEOM's mesh-side check, `Zenith2/Docs/Design/GEOM/Skinning.md → "Skinned mesh data"`). The generated prefab has a root entity holding the animator and every component that acts on the character as a whole, and one child entity per part holding the part's mesh instance and a pose follower naming the root. Consequences, each a deliberate rule:

| Rule | Reason |
|---|---|
| parts are separate mesh instances, never merged into one mesh | GEOM's skinning deforms each mesh with the palette of its own joints (`Zenith2/Docs/Design/GEOM/Skinning.md → "Skinning palette computation"`), so merging saves no skinning work; draws are GPU-driven, so a part costs one culling record, not a CPU draw call; and merging would bake a loadout into a mesh, which a game switches at run time |
| the body is split into region parts, and a garment replaces the regions it covers instead of hiding triangles | no CHAR runtime and no GEOM feature hides sub-mesh triangles per instance; a region part is a whole mesh instance that is made invisible, which the extractor already skips (`Zenith2/Docs/Design/ANIM/Runtime.md → "Extraction algorithm"` step 1) |
| the root has no mesh | every mesh is a follower, so every part is treated alike and a loadout can replace any of them |
| every part is a direct child of the root and follows the root only | ANIM refuses a follower chain deeper than one (`Zenith2/Docs/Design/ANIM/Runtime.md → "Pose followers"`) |

## Assembly document

File `*.character.json`, document type `Zenith.CharacterAssemblyDocument`, schema tag `zenith2.character`, version 1, canonical:

```json
{
	"$schema": "zenith2.character",
	"schemaVersion": 1,
	"id": "f41b…",
	"name": "Ranger",
	"class": "hero",
	"skeleton": "1b7e…",
	"graph": "6f1c…",
	"ragdoll": "8e22…",
	"regions": [ "Head", "Torso", "Arms", "Hands", "Legs", "Feet" ],
	"slots": [
		{ "name": "Body", "exclusive": false },
		{ "name": "Hair", "exclusive": true },
		{ "name": "Outfit", "exclusive": true },
		{ "name": "Boots", "exclusive": true },
		{ "name": "HandRight", "exclusive": true }
	],
	"parts": {
		"0c19…": { "name": "BodyHead", "slot": "Body", "bodyRegion": "Head", "mesh": "a1…" },
		"3d44…": { "name": "BodyTorso", "slot": "Body", "bodyRegion": "Torso", "mesh": "a2…" },
		"5b70…": { "name": "HairCards", "slot": "Hair", "mesh": "b1…", "hairCards": true },
		"7a02…": { "name": "Coat", "slot": "Outfit", "mesh": "c1…", "hidesRegions": [ "Torso", "Arms" ],
			"materialOverrides": [ "m1…", null ], "castsShadows": true },
		"9e31…": { "name": "Bow", "slot": "HandRight", "kind": "RigidAttachment", "mesh": "d1…", "socket": "Hand.R.Grip" }
	},
	"loadouts": {
		"1f00…": { "name": "Default", "parts": [ "0c19…", "3d44…", "5b70…" ] },
		"2a00…": { "name": "Travel", "parts": [ "0c19…", "3d44…", "5b70…", "7a02…", "9e31…" ] }
	},
	"defaultLoadout": "1f00…",
	"rootComponents": {
		"Zenith.Physics.CharacterController": { "version": 1, "fields": { "11": 2 } }
	},
	"generated": {
		"base": { "id": "44e1…", "path": "Ranger.prefab.json", "digest": "3b9f…" },
		"loadouts": { "2a00…": { "id": "45f0…", "path": "Ranger.Travel.prefab.json", "digest": "77c1…" } }
	}
}
```

| Member | Type | Rule |
|---|---|---|
| `name` | string | 1 … 63 bytes of `[A-Za-z0-9_]`; the generated file names derive from it |
| `class` | string | a class ID of the character project document (`Zenith2/Docs/Design/CHAR/Facial.md → "Character project settings document"`) |
| `skeleton` | persistent ID | the skeleton product every part is skinned to |
| `graph` | persistent ID | the `Animation.Graph` product the animator plays |
| `ragdoll` | persistent ID or `null` | a `Physics.Ragdoll` product of the same skeleton, or none |
| `regions` | ordered array (rule C8) | 0 … 16 unique names `^[A-Z][A-Za-z0-9]{0,31}$` |
| `slots` | ordered array | 1 … 16 slots; `name` unique `^[A-Z][A-Za-z0-9]{0,31}$`; `exclusive` true means a loadout holds at most one part of the slot |
| `parts` | persistent-ID map (rule C4) | 1 … 64 parts (`uCHAR_MAX_ASSEMBLY_PARTS`) |
| `parts.*.name` | string | unique, `^[A-Za-z][A-Za-z0-9]{0,31}$` |
| `parts.*.slot` | string | a slot name |
| `parts.*.kind` | `"Skinned"` (default) or `"RigidAttachment"` | "Rigid attachments" |
| `parts.*.mesh` | persistent ID | a `Flux.Mesh` product skinned to `skeleton` |
| `parts.*.bodyRegion` | string or absent | the region this part is the bare body of; at most one part per region |
| `parts.*.hidesRegions` | array of region names | regions this part replaces when visible |
| `parts.*.materialOverrides` | array of ≤ 8 material GUIDs or `null` | slot i maps to `MaterialOverride<i>` of `Zenith.Render.MeshInstance` (`Zenith2/Docs/Design/RPROD/Extractors.md → "Flux_RenderComponents header"`) |
| `parts.*.castsShadows` | bool, default true | `CastsShadows` of the mesh instance |
| `parts.*.hairCards` | bool, default false | the part is a hair-card mesh ("Hair card authoring rules") |
| `parts.*.socket` | string | `RigidAttachment` only: a socket of the rig document |
| `loadouts` | persistent-ID map | 1 … 32 loadouts; each `name` unique, `parts` an ordered array of part IDs |
| `defaultLoadout` | persistent ID | a loadout of `loadouts`; the base prefab shows it |
| `rootComponents` | keyed map stable name → component object | components copied verbatim onto the root entity (SCENE's component object form, `Zenith2/Docs/Design/SCENE/Format.md → "Scene document schema"`): the character controller, gameplay tags, data-defined components; never a component the generator writes itself (rule MV12): no `Zenith.Animation.*`, no `Zenith.Render.MeshInstance`, and no `Zenith.Physics.Ragdoll` while `ragdoll` is non-null (the generator writes that one from `ragdoll`; with `ragdoll` null an authored ragdoll component may be listed here) |
| `generated` | object | written only by the generator ("Prefab generation algorithm"): per generated document its GUID, its path relative to the assembly document and its digest, the SHA-256 of the document's canonical re-serialisation (step 3 of "Prefab generation algorithm") |

## Body regions

<!-- elaboration -->
For a loadout λ (its part set), part visibility is:

1. A part not in λ is invisible.
2. A part in λ that is not a body-region part is visible.
3. A body-region part of region ρ in λ is visible unless some visible part of rule 2 lists ρ in `hidesRegions`.

So a coat that `hidesRegions: ["Torso", "Arms"]` turns the bare torso and arms off, and the coat itself carries the torso and arms. A region that, in some loadout, has neither a visible body part nor a visible part hiding it is a hole: the warning `CHAR_DIAG_ASSEMBLY_REGION_HOLE` names the loadout and the region (a legitimate design for a character without legs is silenced by removing the region from `regions`). A region claimed by two visible parts' `hidesRegions` is the warning `CHAR_DIAG_ASSEMBLY_REGION_OVERLAP` (two garments over one region usually z-fight). `Test_CharAssembly_VisibilityRules` asserts the three rules and both diagnostics on a six-region fixture.

## Slots and loadouts

- A loadout holding two parts of one exclusive slot is the error `CHAR_DIAG_ASSEMBLY_SLOT_CONFLICT` naming the loadout, the slot and both parts; generation refuses the document.
- A loadout holds at most `uCHAR_MAX_VISIBLE_PARTS` (16) visible parts after "Body regions" ("Part caps and cost"); more is the error `CHAR_DIAG_ASSEMBLY_TOO_MANY_PARTS`.
- A part in no loadout is the warning `CHAR_DIAG_ASSEMBLY_PART_UNUSED`; it is still generated (invisible in every prefab), because a game may switch to it at run time ("Runtime part switching").

## Generated prefab layout

The generator writes SCENE prefab documents (`Zenith2/Docs/Design/SCENE/Format.md → "Prefab document schema"`) with SCENE's prefab canonical schema, canonical by construction:

| Entity | Parent | Name | Components |
|---|---|---|---|
| root | `null` | the assembly's `name` | `Zenith.LocalTransform` (identity); `Zenith.Animation.Animator` (1 Graph = `graph`, 2 PlayRate 1, 3 RootMotion `EXTRACT`, 4 Enabled true, 5 SignificanceBias = the class's `significanceBias`, 6 Flags = `ANIMATOR_FLAG_FORCE_FULL_RATE` when the class's `forceFullRate` and `ANIMATOR_FLAG_ALLOW_REDUCED_ROOT_MOTION` when its `allowReducedRootMotion`); `Zenith.Animation.RootMotion` and `Zenith.Animation.Pose` with no fields (ANIM's rule that an animated character lists all three, `Zenith2/Docs/Design/ANIM.md → "Component registration"`); `Zenith.Animation.IKGoals` when the graph document has an `ik` array; `Zenith.Physics.Ragdoll` with property 3 Definition = `ragdoll` when it is non-null (PHYS's component, `Zenith2/Docs/Design/PHYS/ComponentsAPI.md → "Zenith_Ragdoll header"`; its other properties keep their defaults); then every `rootComponents` entry verbatim, in stable-name byte order |
| one per part | the root | `Part.<slot>.<part name>` | `Zenith.LocalTransform` (identity); `Zenith.Render.MeshInstance` (1 Mesh, 2 … 9 MaterialOverride0 … 7 from `materialOverrides`, 13 CastsShadows, 15 Visible = the part's visibility in `defaultLoadout`); `Zenith.Animation.PoseFollower` (1 Leader = the root's entity ID) |

Sibling order is the parts' order by (slot order in `slots`, part name byte order), with `Zenith_MakeEvenSortKeys` (`Zenith2/Docs/Design/SCENE/Format.md → "Sort key insertion algorithm"`). The ENTITY-typed Leader value names the root by its ID in the prefab's resolved ID space, so a scene instance of the prefab resolves each follower's leader to its own instance's root (SCENE's resolution remaps IDs of an instance, `Zenith2/Docs/Design/SCENE/Prefabs.md → "Resolution algorithm"`).

<!-- assumption: pending PHYS (row CH8) -->
The ragdoll component's stable name and its property 3 Definition are PHYS's (`Zenith2/Docs/Design/PHYS/ComponentsAPI.md → "Zenith_Ragdoll header"`, PHYS revision 2); row CH8 asks PHYS to keep property 3 as the definition handle the generator writes.
<!-- /assumption -->

## Generated entity identity

Entity IDs are derived, so regeneration keeps every ID and every override a scene holds against them stays valid (`Zenith2/Docs/Design/SCENE/Prefabs.md → "Derivation"`):

- root: `Zenith_DeriveAuthoringID(prefab ID, L("root"))`;
- part p: `Zenith_DeriveAuthoringID(prefab ID, L("part/" ‖ the part's persistent ID in its 36-character form))`;

where `L(s)` is CHAR's local-ID function, built like the local ID of CONTENT's generated model prefab (`Zenith2/Docs/Design/CONTENT/Importers.md → "Generated model prefab"`) but with CHAR's own domain tag `ZCHR` in place of CONTENT's `ZCNK`, so the two never collide and no worker reuses CONTENT's function: bytes 0 … 15 of `Zenith_Sha256Digest("ZCHR" ‖ UTF-8 s)` read as `ulHigh` and `ulLow` little-endian, with SCENE's version-4 bit rule. It is `Zenith_CharMakeLocalID(Zenith_StringView)` in `Internal/CHAR/Zenith_CharAssemblyPrefabs.cpp`. A variant uses its base's IDs for the base's entities (SCENE's variant rule), so every loadout variant addresses the same part entities. Renaming a part changes no ID (the ID is the part's persistent ID); deleting a part retires its entity from every generated document at the next generation. `Test_CharAssembly_DerivedIDsStableAcrossRegeneration` regenerates after a rename, a reorder and a new part, and asserts every surviving entity ID is unchanged and every scene override in a fixture scene still resolves.

## Loadout variants

For every loadout λ other than `defaultLoadout`, the generator writes a variant `<name>.<loadout name>.prefab.json` with `variantOf` = the base prefab's ID and a `base` override block (`Zenith2/Docs/Design/SCENE/Format.md → "Override block"`) holding exactly one `properties` entry per part whose visibility in λ differs from the default loadout's: `{ "Zenith.Render.MeshInstance": { "version": 1, "fields": { "15": <visible> } } }`, in ascending entity ID. A variant adds no entity and removes none, so instances of different loadouts share every entity ID and a game switches loadout by visibility alone. `Test_CharAssembly_VariantOverridesOnlyVisibility` asserts the block for the fixture's two loadouts.

## Prefab generation algorithm

`character.assembly.generatePrefabs [assembly, force]` (one undo record; the files are written in one journaled batch, `Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Journaled write"`):

1. Run "Assembly validation rules"; any error refuses the command and writes nothing.
2. Compute the set G of documents to write: the base prefab and one variant per non-default loadout, each at `generated.*.path` when recorded, else at `<name>.prefab.json` / `<name>.<loadout name>.prefab.json` beside the assembly; a path already used by a document the generator did not record is refused (`CHAR_DIAG_ASSEMBLY_PATH_TAKEN`).
3. For each recorded document: parse it and re-serialise it with SCENE's prefab canonical schema (`Zenith_JsonDocument` written by SCENE's canonical writer, LF line endings and canonical member order by construction, `Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Canonical rules"`); its digest is the SHA-256 of those bytes, so a checkout that converted line endings or reformatted whitespace reads as unedited. When the digest ≠ `generated.*.digest`, or the file does not parse, the document was edited by hand: without `force`, refuse with `CHAR_ERROR_PREFAB_HAND_EDITED` naming every such file ("Hand-edit protection"); with `force`, include it.
4. Allocate GUIDs for documents not yet recorded (`Zenith_GeneratePersistentID`, the editor's asset-creation path, which writes the `.meta` with that GUID, `Zenith2/Docs/Design/EDIT/Pipeline.md → "Create and duplicate"`).
5. Build each document with `Zenith_CharBuildAssemblyPrefabs` (pure: the assembly, the class table, the graph's `ik` presence, the GUIDs → canonical JSON bytes per document, in G's order), using "Generated prefab layout", "Generated entity identity" and "Loadout variants".
6. Retire documents of loadouts that no longer exist: they are moved to the editor trash (`Zenith2/Docs/Design/EDIT/Pipeline.md → "Delete and the editor trash"`) in the same batch, and their `generated.loadouts` entries removed.
7. Write every document of G whose bytes changed, and the assembly's `generated` member with the new digests (each the SHA-256 of the bytes written, which are canonical, so equal to step 3's digest of them), in one batch.

`Zenith_CharBuildAssemblyPrefabs` is O(parts × loadouts). `Test_CharAssembly_GenerationIsCanonicalAndIdempotent` generates twice and asserts the second run writes nothing; `Test_CharAssembly_GeneratedPrefabResolves` resolves the base and a variant with SCENE's resolver and asserts the entity set, the leaders and the visibilities; `Test_CharAssembly_RetiredLoadoutTrashed` asserts step 6; `Test_CharAssembly_DigestIgnoresLineEndings` rewrites a generated prefab with CRLF line endings and asserts generation does not refuse it.

## Hand-edit protection

Generated prefabs are outputs of the assembly document and are not edited by hand: an edit is lost at the next generation, so the generator refuses to overwrite one (step 3 of "Prefab generation algorithm"). The assembly panel shows a generated document whose digest differs as "edited outside the assembly", with two actions: **Discard edits** (regenerate with `force`) and **Keep as variant** (`character.assembly.keepAsVariant`, "Keep as variant algorithm"), which moves the edits into a new prefab variant of the generated document and then regenerates it with `force`. Hand-authored customisation therefore lives in variants of generated prefabs, which regeneration never touches. `Test_CharAssembly_HandEditRefusedWithoutForce` and `Test_CharAssembly_KeepAsVariantPreservesEdit` cover both.

## Keep as variant algorithm

`character.assembly.keepAsVariant [assembly, document, path]` (one undo record, one journaled batch), for an edited generated document E (the base prefab or a loadout variant):

1. Build G, the bytes the generator would write for E now (step 5 of "Prefab generation algorithm"; E keeps its ID, so G and E share one ID space).
2. Resolve both with SCENE's resolver (`Zenith_ResolveDocument`, `Zenith2/Docs/Design/SCENE/SceneAPI.md → "Zenith_PrefabResolver header"`) into R_E and R_G, the flat entity sets. A loadout variant resolves through its base, so edits to its own `base` block are compared by their effect on the resolved entities.
3. Compare entity by entity, in ascending entity ID, and component by component, in stable-name byte order, and map every difference into the new variant V:

| Difference (R_E against R_G) | Written into V |
|---|---|
| an entity in R_E only | V's `entities`, with its `name`, `parent`, `sortKey` and components from R_E (a nested instance of E's `instances` goes to V's `instances` likewise) |
| an entity in R_G only | V's `base.removedEntities`, listing only entities whose parent is not removed too (a removal takes the subtree) |
| a component of a shared entity in R_E only | `base.addedComponents[entity][stable name]` = R_E's component object |
| a component of a shared entity in R_G only | `base.removedComponents[entity]`, the stable names sorted |
| a component of both with differing fields | `base.properties[entity][stable name]` = `{ "version": R_E's version, "fields": only the property IDs whose values differ }`; a fixed array or container property that differs anywhere is written whole (SCENE's override rule, `Zenith2/Docs/Design/SCENE/Format.md → "Override block"`) |

4. Refuse, writing nothing, when E holds an edit that no override block expresses, with the error `CHAR_ERROR_EDIT_UNREPRESENTABLE` and one `CHAR_DIAG_ASSEMBLY_EDIT_UNREPRESENTABLE` diagnostic per edit naming the entity (or the document) and the member: a shared entity whose `name`, `parent` or `sortKey` differs; a property ID present in R_G and absent in R_E (an override sets properties, it cannot remove one); a component whose `version` differs; a document-level `name`, `root` or `variantOf` that differs; an entity of R_E only whose parent exists in R_G only (it would hang from a removed entity). The author then discards the edits or writes the variant by hand.
5. Write V: `id` a new persistent ID (the editor's asset-creation path, `Zenith2/Docs/Design/EDIT/Pipeline.md → "Create and duplicate"`), `name` E's name with the suffix ` (variant)`, `root` E's root, `variantOf` E's ID, the block and the entities of step 3, at `path` (refused when taken, `CHAR_DIAG_ASSEMBLY_PATH_TAKEN`). V of a loadout variant forms the chain V → E → base, which counts toward `uSCENE_MAX_VARIANT_CHAIN` (16, `Zenith2/Docs/Design/SCENE/SceneAPI.md → "Zenith_SceneErrors header"`); a chain over the limit is refused before anything is written.
6. Regenerate E with `force` in the same batch.

Complexity O(entities × components). `Test_CharAssembly_KeepAsVariantPreservesEdit` keeps, from a hand-edited base prefab, an added entity, a removed part entity, an added and a removed component and one changed property; it asserts V's block holds exactly those, and that resolving V after the forced regeneration equals the edited R_E; a second case (a renamed part entity) is refused with `CHAR_ERROR_EDIT_UNREPRESENTABLE` naming the entity and `name`, and nothing is written. `Test_CharAssembly_KeepAsVariantOfLoadoutVariant` asserts the chain on a loadout variant.

## Assembly validation rules

`Zenith_CharValidateAssembly(const Zenith_CharAssemblyView&, Zenith_CharAssetQuery&, Zenith_Array<Zenith_CharDiagnostic>&)` runs these rules; `Zenith_CharAssetQuery` is CHAR's interface for facts about other assets (the editor implements it over the session's loaded assets, tests over fixtures; `Zenith2/Docs/Design/CHAR.md → "Asset query interface"`):

| Rule | Check | Severity | Diagnostic |
|---|---|---|---|
| MV1 | every part's mesh is skinned (`Zenith_CharAssetQuery::GetMeshSkeleton(xMesh, xOut, bSkinnedOut)` reports `bSkinnedOut` true), and the skeleton it returns equals `skeleton` (RigidAttachment: as "Rigid attachments") | error | `CHAR_DIAG_ASSEMBLY_SKELETON_MISMATCH` |
| MV2 | the graph's skeleton and the ragdoll's skeleton equal `skeleton` | error | `CHAR_DIAG_ASSEMBLY_SKELETON_MISMATCH` |
| MV3 | `class` exists in the project document | error | `CHAR_DIAG_ASSEMBLY_CLASS_UNKNOWN` |
| MV4 | slot exclusivity per loadout ("Slots and loadouts") | error | `CHAR_DIAG_ASSEMBLY_SLOT_CONFLICT` |
| MV5 | visible parts per loadout ≤ 16 | error | `CHAR_DIAG_ASSEMBLY_TOO_MANY_PARTS` |
| MV6 | at most one body part per region; every `bodyRegion` and `hidesRegions` name is in `regions` | error | `CHAR_DIAG_ASSEMBLY_REGION_INVALID` |
| MV7 | region holes and overlaps ("Body regions") | warning | `CHAR_DIAG_ASSEMBLY_REGION_HOLE`, `_REGION_OVERLAP` |
| MV8 | a RigidAttachment's socket exists in the skeleton | error | `CHAR_DIAG_ASSEMBLY_SOCKET_UNKNOWN` |
| MV9 | a `hairCards` part satisfies "Hair card authoring rules" | per rule | `CHAR_DIAG_HAIR_*` |
| MV10 | `materialOverrides` length ≤ 8 and each GUID is a `Flux.Material` | error | `CHAR_DIAG_ASSEMBLY_MATERIAL_INVALID` |
| MV11 | a part in no loadout | warning | `CHAR_DIAG_ASSEMBLY_PART_UNUSED` |
| MV12 | `rootComponents` holds no `Zenith.Animation.*` or `Zenith.Render.MeshInstance` component, and no `Zenith.Physics.Ragdoll` while `ragdoll` is non-null (the generator owns those; SCENE refuses two components of one stable name on an entity) | error | `CHAR_DIAG_ASSEMBLY_ROOT_COMPONENT_RESERVED` |
| MV13 | the generated root passes the character authority rules AU1 … AU8 (`Zenith2/Docs/Design/CHAR/Authority.md → "Authority lint rules"`) | per rule | `CHAR_DIAG_AUTHORITY_*` |

`Test_CharAssemblyValidation_EachRule` has one case per rule with a fixture that breaks only that rule (MV12 three times: an animator, a mesh instance, and a ragdoll component beside a non-null `ragdoll`; a ragdoll component with `ragdoll` null passes); `Test_CharAssemblyValidation_CleanFixturePasses` asserts zero diagnostics on the walkthrough's assembly (the derivation is in `Zenith2/Docs/Design/CHAR/Workflow.md → "Walkthrough rehearsal"`).

## Rigid attachments

A prop held in a hand (a bow, a lantern) moves with a joint, not with the root, so it cannot be a plain child entity. CHAR makes it a part like any other: a mesh rigidly skinned to one joint, driven by the follower mechanism.

<!-- assumption: pending GEOM (row CH6) -->
GEOM's mesh cook accepts the importer setting `skin.rigidAttach` on a static mesh product: `{ "skeleton": <skeleton GUID>, "joint": "<joint name>", "offset": { "position": [x, y, z], "rotation": [x, y, z, w] } }`. The cook then emits a skinned mesh with one mesh joint (the named joint, remapped to its skeleton index), every vertex weighted 1 to it, the inverse bind matrix equal to the skeleton's published `restModelInverse` of that joint (so SV9 holds by construction), and every vertex transformed at cook by `restModel_joint · TRS(offset)`; the posed-extent check uses the skeleton's maximum reach as for every skinned mesh. The setting is GEOM's (row CH6); CHAR writes it into the prop's `.meta` through the asset settings command (`asset.setSettings`, `Zenith2/Docs/Design/EDIT/Automation.md → "Built-in commands"`) when an attachment part is created, with the offset of the rig document's socket (`Zenith2/Docs/Design/ANIM/Formats.md → "Rig document schema"`, `sockets`). Until GEOM's setting lands, RigidAttachment parts are refused by validation (`CHAR_DIAG_ASSEMBLY_RIGID_UNAVAILABLE`) and the Stage 4 attachment ticket stays `draft`.
<!-- /assumption -->

A socket's offset changing re-writes the setting (the assembly panel offers it when the rig document's socket differs from the setting it recorded); the cook then re-cooks the prop through its setting's key. `Test_CharRigidAttachment_WritesSocketSetting` asserts the setting JSON for a fixture socket.

## Part caps and cost

| Quantity | Value | Basis |
|---|---|---|
| parts per assembly | 64 (`uCHAR_MAX_ASSEMBLY_PARTS`) | wardrobe size of a flagship hero; every part is an invisible entity when unused |
| visible parts per loadout | 16 (`uCHAR_MAX_VISIBLE_PARTS`) | per visible part ANIM's extractor writes `48 × J` bytes of joint matrices into the render world (`Zenith2/Docs/Design/ANIM/Runtime.md → "Extraction algorithm"` step 3): at J = 60 that is 2,880 bytes, 46,080 bytes for 16 parts |
| loadouts per assembly | 32 | variants per character |

<!-- elaboration -->
The extraction cost of modular characters is reported against the pending row "Modular part extraction overhead" ("Pending row needs" of `Zenith2/Docs/Design/CHAR.md`), measured in the W-Characters modular variant (each of the 100 characters assembled from 6 parts: 100 × 6 × 2,880 B ≈ 1.7 MB of joint matrices per frame), inside `Z2-B-RPROD-003`'s span; skinning cost is unchanged by assembly, because each part skins its own vertices once (`Z2-B-GEOM-004`).

## Runtime part switching

Switching a part during play is game logic (equipment is a game-side system, Z2-D-077): the game's script sets `Zenith.Render.MeshInstance` property 15 Visible of the part entities through commands, applying the visibility rules of "Body regions" itself, using the entity names `Part.<slot>.<part name>` the generator guarantees. CHAR's contract to the game is those names and rules; the flagship slices' equipment samples are GAME's (cross-area row CH4). `Test_CharIntegration_ModularVisibilitySwitch` (CHAR's integration suite) spawns the generated base prefab, switches the coat on through `SetProperty` commands in a test system, and asserts the extractor's counters: the body torso's pose is no longer extracted and the coat's is, with no follower refused (`anim.extract.follower_refused_delta` 0).

## Hair alternatives classification

Hair and groom simulation and rendering are excluded (Z2-D-043, Z2-D-078). Z2-D-078 places the classification of hair alternatives under CHAR; CHAR's classification, proposed as a dated amendment of Z2-D-078 (core-doc item X327):

| Alternative | Classification | Stage | Rationale |
|---|---|---|---|
| hair cards (textured strips skinned to the head and hair joints, alpha-tested) | required | S4 authoring rules and validation; S5 anisotropic highlight | CinematicSlice's sign-off names hair-card appearance ([Z2-N-BUDGET-435]); cards use only existing features: skinned parts, the `alphaTest` material domain, `twoSided`, and GEOM's anisotropy surface model (Stage 5) |
| hair shells (stacked offset copies of a surface, each alpha-tested) | excluded | — | each shell is a further alpha-tested layer over the same pixels, so N shells cost N times the alpha-tested overdraw the early-Z rules keep minimal (alpha-tested pipelines are the only ones allowed to discard, [Z2-N-REND-916]), which the S24's tile-based GPU pays in fragment bandwidth; no flagship slice needs fur; a short-fur look is achievable with cards and textures |
| strand-based hair, groom simulation | excluded | — | Z2-D-043, Z2-D-078 |

Secondary motion of hair-card joints is authored animation (clips and additive layers on the hair joints); a simulated motion depends on D-CLASS-SIM (open, Stage 2), which classifies cloth and soft bodies with CHAR named as a consumer for cloth; if it rules a simulation required, its owner area specifies the hair-joint drive, and CHAR's hair rules gain a row in the revalidation that ruling triggers.

## Hair card authoring rules

A part with `hairCards: true` is checked (rule MV9):

| Rule | Check | Severity | Diagnostic |
|---|---|---|---|
| HC1 | every material of the mesh (and every override) has `domain` `alphaTest` (`Zenith2/Docs/Design/GEOM/Materials.md → "Material document schema"`) | error | `CHAR_DIAG_HAIR_NOT_ALPHA_TESTED`; a `transparent` hair material is refused because sorted transparency per strand layer is not a supported hair technique |
| HC2 | every such material is `twoSided` | warning | `CHAR_DIAG_HAIR_NOT_TWO_SIDED` |
| HC3 | from Stage 5, the material's `surfaceModelFlags` holds `anisotropy` | warning | `CHAR_DIAG_HAIR_NO_ANISOTROPY` |
| HC4 | the part's slot is exclusive | warning | `CHAR_DIAG_HAIR_SLOT_NOT_EXCLUSIVE` |
| HC5 | the part casts shadows through the alpha-tested shadow path (`castsShadows` true) | warning | `CHAR_DIAG_HAIR_NO_SHADOW` |

Card triangle counts and texture budgets are ContentSpecs' content rules for the mesh and texture kinds (GEOM's), selected by the class label CHAR writes ("Content rule measures" of `Zenith2/Docs/Design/CHAR/Debugging.md`). `Test_CharHairRules_EachRule` covers HC1 to HC5. The appearance (no aliasing shimmer under TAA, plausible highlights) is a CinematicSlice sign-off item, not a test.
