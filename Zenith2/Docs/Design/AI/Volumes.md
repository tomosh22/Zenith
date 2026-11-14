# AI design: volume navigation

Section file of `Zenith2/Docs/Design/AI.md`. It specifies non-ground navigation: the sparse-voxel volume model shared by flying and underwater agents, its bake, its search, smoothing and line of sight, swimming on the navmesh surface, and the transitions between media (inventory items Z2-I-AI-16 flying navigation and Z2-I-AI-17 swimming navigation).

## Scope of this file

<!-- elaboration -->
Navmeshes describe walkable surfaces and cannot express free 3D space. Flying and underwater agents therefore use a second structure, a sparse voxel octree per volume region, after the "3D Flight Navigation Using Sparse Voxel Octrees" design (Game AI Pro 3, chapter 21): leaf bricks of 4 × 4 × 4 voxels stored as 64-bit masks, an octree above them, and an A* over nodes of mixed sizes whose open space is searched at the coarsest level that is entirely free. Recast/Detour has no equivalent, so this structure is AI's own whichever candidate D-SEL-NAV (open, Stage 1/2) selects; the harness measures the selected candidate together with this volume layer against the rule's "non-ground navigation" criterion (conformance scenario N12, `Zenith2/Docs/Design/AI/Navigation.md` → "Conformance scenarios"), recorded as a cross-area note for the ruling's wording.

## Volume model

- A volume class (settings document, at most 2) has a leaf voxel edge `v` (`m_fCellSize`, 0.25 … 4 m, default 0.5 m) and an agent radius `r`; solids are inflated by `r` at bake, so a free voxel is one the agent's centre may occupy.
- A volume region is an axis-aligned box whose edge is a power of two multiple of `4v` (at most 512 m per side at v = 0.5 m, `uAI_VOLUME_MAX_LEVELS` = 10 levels: level 0 bricks of 4v, level L nodes of 4v × 2^L); its absolute minimum corner is a `double` triple like a tile origin, so origin shifts are the tile rule (`Zenith2/Docs/Design/AI/Navigation.md` → "Origin shift").
- Each level-0 node (a brick) stores two 64-bit masks: `solid` (bit set = SOLID) and `water` (bit set = WATER where not solid); a voxel's medium is SOLID if its solid bit is set, else WATER if its water bit is set, else AIR. Bit index = x + 4y + 16z within the brick.
- A node at level L ≥ 1 is stored only when it is mixed (neither wholly free of one medium nor wholly solid); a wholly SOLID or wholly single-medium node is a leaf at its level with a medium tag and no children, so open sky and open water cost one node per largest free cube.
- Every stored node has six face-neighbour links (x−, x+, y−, y+, z−, z+) to the node of the same or a coarser level sharing that face, or none at the region boundary; links are 32-bit (level 4 bits, index 28 bits).
- Node record: 48 bytes (two masks 16, medium tag and level 4, first-child index 4, six links 24); a volume holds at most `uAI_VOLUME_MAX_NODES` (4,194,304) nodes, 192 MiB, which the bake refuses beyond (`AI_ERROR_VOLUME_CAPACITY`). A typical 256 m region over terrain and buildings at 0.5 m is 200,000–400,000 nodes (9–18 MiB), counted in Z2-B-AI-001.
- Several volume regions of one class may be loaded; regions never overlap within a class (the bake refuses overlapping regions in one navigation document), and adjacent regions are joined at query time by matching face nodes across the shared boundary.

## Volume bake

For each volume region of the navigation document (`Zenith2/Docs/Design/AI/Formats.md` → "Navigation document schema", `volumes`):

1. Gather the same inputs as a navmesh tile ("Bake inputs" of `Zenith2/Docs/Design/AI/Navigation.md`), every triangle regardless of slope, plus the water bodies: each water body gives a prism (its polygon or the ocean's extent, from its surface height down to the terrain), from the navigation document's `waterVolumes` boxes and, for a referenced terrain, ENV's water bodies (`Zenith2/Docs/Design/AI.md` → "Assumed interfaces", ENV row).
2. Rasterize solids conservatively into leaf voxels: a voxel is SOLID when any triangle intersects it after inflating by `r` (triangle–box overlap with the box grown by `r`, separating-axis test on 13 axes).
3. Flood the free voxels' medium: a free voxel is WATER when its centre lies inside a water prism, else AIR.
4. Build the octree bottom-up: bricks from voxels; a level-L node from its 8 children, collapsed when all 8 are leaves of one medium.
5. Link faces: for each stored node and face, the neighbour is the same-level node across the face if stored, else the coarsest ancestor-level node containing the across point.
6. Assemble the volume section (Formats.md "Volume section"), nodes in level order then Morton order, so the bytes are deterministic.

Complexity: O(triangles × voxels touched) for step 2, O(voxels) for steps 3–5. The bake's key and determinism rules are those of `Zenith2/Docs/Design/AI/Navigation.md` → "Bake determinism".

## Volume search

`Zenith_NavVolumeSearch`:

1. Locate start and end: the leaf voxel (or collapsed node) containing each point; if its medium is not the agent's, `FindNearestFree` searches voxels in expanding Chebyshev shells up to `m_fSearchRadius`, nearest by Euclidean distance then by (level, node index, voxel); none → NO_START or NO_END.
2. A* over search units: a free collapsed node at any level is one unit; a mixed brick contributes its free voxels as units. A unit's position is its centre; neighbours are the units across each face (for a face of a large node, every smaller free unit on the other side touching the face, enumerated in Morton order).
3. Cost: g += |Δ position| × (1.0, or 1.0 + `m_fMediumChangeCost` / |Δ| when the medium changes and the class is amphibious); h = |end − position| × 0.999.
4. Tie-break: (f, h, level, node index, voxel index), all stable within the volume file; across regions, the region's absolute minimum corner (x, y, z) orders first.
5. Slicing, node pool, relaxation (the expansion step, with search units in place of polygons and the unit's centre as its fixed position, so no relaxation moves a position) and the tick cap are exactly those of `Zenith_NavPathSearch` (`Zenith2/Docs/Design/AI/Navigation.md` → "Search state and slicing"), over the same structures: a pool of `Zenith_NavSearchNode` records whose `m_ulRef` holds the unit's `Zenith_NavVolumeRef` value, the open heap with its heap-index array, and a visited lookup (`Zenith_NavSearchLookup`, volume reference value → unit index) used for membership only (`Zenith2/Docs/Design/AI/PublicAPI.md` → "Zenith_NavVolume header"). The search's arena block for N units is 40 N + L(N) + 8 × min(N, 256) + 512 bytes: nodes, heap and heap indices 40 N, the lookup L(N) (the formula of "Search state and slicing"), the stamps 8 × min(N, 256), and a 256-entry `uint16` table from volume slot to stamp index + 1 (512 bytes), which makes stamp membership O(1) without a second map. `Begin` and `RestoreSnapshot` carve it all and call `TryReserve(N)` on the lookup first. At the path service's default N = 2,048 that is 81,920 + 65,536 + 2,048 + 512 = 150,016 bytes against the navmesh search's 245,760. For every admitted configuration (N ≥ 256, `ai.nav.maxTiles` ≥ 64) the navmesh block is at least 512 bytes larger, since 16 S + L(S) ≥ 3,072 > 8 × 256 + 512 = 2,560, so the service's one block size B(`ai.path.maxSearchNodes`) holds either kind.
6. **Volume stamps and staleness.** A loaded volume's content never changes (volumes are never carved), so a volume stamp carries no version: `Zenith_NavVolumeSearchStamp` (`Zenith2/Docs/Design/AI/PublicAPI.md` → "Zenith_NavVolume header") is the volume's slot in the volume set and that slot's 16-bit salt, added when a unit of a volume with no stamp is created (the start unit included), at most min(`m_uMaxNodes`, 256) stamps in the search's arena, so the table never binds before the node pool. A search is STALE only when a stamped volume was removed or its slot re-used (the salt differs). The save stores each stamp as the volume's index in the section's volume list (`Zenith2/Docs/Design/AI/Formats.md` → "Save section layout"), and a restore takes the restored volume's slot and salt, so a flying or diving agent's active search continues bit-identically (`Test_AISave_ActiveVolumeSearchRestored`).

## Volume path smoothing

The unit path's centres are reduced to corners by greedy line of sight: from the current corner, advance to the farthest subsequent unit centre (checking every subsequent centre, at most 64) whose segment has line of sight in the agent's medium; emit it; repeat. Then each corner is pulled toward the straight segment between its neighbours by at most `v/2` while line of sight holds, which shortens zig-zags through large nodes. The result holds at most `uAI_PATH_MAX_CORNERS` corners; more are truncated (status TRUNCATED).

## Volume line of sight

`HasLineOfSight` walks leaf voxels along the segment with a 3D DDA (Amanatides–Woo), skipping whole collapsed nodes in one step (exit distance of the node's box), and fails at the first voxel whose medium differs from the requested one. A segment longer than 512 m or leaving every loaded region fails. O(voxels crossed), at most 3 × 1,024 steps per 512 m.

## Swimming

| Agent | Medium | Structure | Rule |
|---|---|---|---|
| surface swimmer | GROUND | the navmesh | its cost profile gives `water` (4) a finite cost and includes `NAV_POLY_FLAG_SWIM`; it walks and swims one corridor; the locomotion output is unchanged (the character controller, or kinematic, Agents.md "Locomotion output") |
| walker | GROUND | the navmesh | profile 0 gives water +∞: paths go round water |
| diver | UNDERWATER | a volume class | the search occupies WATER voxels only; the agent's vertical motion is AI's (kinematic) |
| amphibious diver | UNDERWATER with the class's `amphibious` flag | a volume class | WATER and AIR voxels, with `m_fMediumChangeCost` at each change |
| flyer | FLYING | a volume class | AIR voxels only |

## Medium transitions

- An agent changes medium only through its `Medium` property (a command), never implicitly; at the change AI drops its corridor and replans in the new structure from its position (`Zenith2/Docs/Design/AI/Agents.md` → "Agent state machine", event "medium changed").
- A diver whose position is in a GROUND class's water polygon may switch to UNDERWATER; the switch is the game's decision (a behaviour records `AI.SetMedium`); AI reports `Zenith_NavVolumeSet::SampleMedium` to scripts through `AI.SampleMedium` so they can decide.
- An amphibious diver's path that leaves the water ends where it surfaces (a corner with the medium change), so a behaviour can switch it back to GROUND there.
