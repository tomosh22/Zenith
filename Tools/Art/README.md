# Zenithmon Blender source recipes

## Creature source development

`create_windseed_creatures.py` authors Puffseed, Dandelift and Zephyrbloom with
Blender's Python API. Run with `--background --factory-startup --disable-autoexec
--python-exit-code 1 --python Tools/Art/create_windseed_creatures.py`; optional
species names follow `--`. Sources are written under `creature_work/Windseed`.
The original meshes contain ribbed seed bodies, serrated leaves, curled roots,
individual achenes and geometric pappus filaments; Zephyrbloom adds flowering
side branches. Botanical references inform structure without contributing meshes
or textures. Embedded base colour, normal and metallic/roughness maps follow
glTF's G roughness / B metallic convention. Optical parameters are authored,
not measured tissue scattering. Normal/shiny sources retain matching skins.

The ten named floater joints use the restrained fungal wilt and adaptive
whole-surface clearance, retaining rotation knots where the lowest leaf changes.
The GLB importer converts winding per node transform, including reflections;
open leaf and petal surfaces must not be classified by signed mesh volume.
Creature import receipts use v4 to invalidate the earlier winding heuristic.
`ZM_AuthoredWindseedCapture_Test` reviews nine poses per working source; three
species-specific battle tests review installed sources in gameplay. This is a
development family requiring visual acceptance, including filament aliasing,
leaf thickness, botanical irregularity and the seed body's surface response.

`create_shoalfish.py` authors Minnet, Shoalfin and Torrentfin from the pinned
Artec Fish scan, with newly modeled fin membranes/rays and eye overlays. Run
with Blender `--background --factory-startup --disable-autoexec --python-exit-code 1`
and `--python Tools/Art/create_shoalfish.py`; optional species names follow `--`.
The recipe writes under `creature_work/Shoalfish`. Archive and extracted input
hashes, attribution and both publisher/archive license notices are recorded in
`source_references.json`; copyright metadata accompanies the GLB exports.

The 498,859-triangle scan is straightened, proportioned per stage and reduced
to about 100k body triangles. New cylindrical body UVs receive captured geometric
normals and tinted photographic colour by selected-to-active baking. A 2K bake
does not add resolution to the original 1K photograph or fully remove lighting.
The complete candidates contain 124,054 triangles and five PBR material sections.
The eight aquatic joints retain canonical names, hierarchy and identity axes;
the authored aquatic fitter supplies action bends, a held roll and clearance.

`ZM_AuthoredShoalfishCapture_Test` captures nine poses for each working source.
Per-species battle capture tests verify the installed sources. Fin membranes use
single, two-sided surfaces with textured glTF BLEND opacity; collagen rays retain
solid geometry. The scene translucent pass consumes the compute-skinned vertex
arena, and GLB import preserves alpha with BC3 sRGB. The capture checks both the
animated arena slice and the alpha-capable runtime texture. Alpha blending is
an approximation of a thin membrane, not measured tissue transmission.
Surface-lighting cleanup, fin-root finishing and animation polish remain open.
Technical checks and source installation do not establish photorealistic acceptance.

`create_scanned_mineral_beetles.py -- Prismite` (arguments after Blender's
`--python` script and `--` separator) adapts the pinned Artec beetle scan into
the revised Prismite candidate. It writes under `creature_work/MineralBeetlesScan`
and does not install automatically. The archive and extracted mesh, photo and
license hashes are checked before use. The original archive CC BY 3.0 notice
is retained under `licenses/`; the publisher page also offers CC BY 4.0.
Derivative folders and GLB copyright metadata carry attribution and changes.

The scan supplies geometric relief and photographic colour. The recipe reads
the photo as non-colour bytes, explicitly decodes sRGB for exposure adjustment,
then re-encodes it for the base-colour map. Authored dielectric roughness and a
neutral normal map complete the glTF material slots; these are not measured
PBR maps and the photograph is not fully de-lit. Blender automatic weights on
an anatomical proxy rig are transferred to the canonical 19-joint rig, keeping
the engine's identity joint axes. Original mineral clusters follow ray hits
on the scanned wing cases. The export has 300,576 triangles for Prismite.

`ZM_AuthoredScannedBeetleCapture_Test` provides a closer nine-pose review of
this working source. The installed Prismite pair supersedes the earlier
procedural candidate; the original working family and a runtime-source backup
remain available for comparison. Its final photorealistic acceptance remains
outstanding, particularly mineral integration, photographic lighting removal,
motion polish and triangle-budget review. Geoborer and Crystallisk retain
their earlier sources; additional recipe name options are unreviewed.

`create_fernfawn.py` is the earlier procedural Fernfawn recipe. It exports a
continuous remeshed body, texel-painted 2K coat, tangent-space coat normals,
glTF metallic/roughness maps, fine guard-hair geometry and a standard GLB skin.
The editable source and offline renders stay under
`Build/artifacts/authored_assets/creature_work/Fernfawn/`. These are development
artifacts, not a completed creature or a photorealism acceptance claim.

`audit_creature_glb.py <source.glb> <shared.zskel> --report <report.json>` checks
the source's joint names, hierarchy, full bind matrices, normalized weights,
indices, embedded PBR texture slots and geometry attributes. It reports the
glTF-to-engine joint remapping explicitly. Technical validity does not prove
anatomical fit, deformation quality, performance or artistic quality.

`create_fernfawn_from_reference.py` is the current anatomical development source.
Open the pinned CC0 deer with Blender `--disable-autoexec --background <source>`
and run this recipe with `--python`. It writes the adapted mesh, embedded PBR
maps, editable scene and provenance to `creature_work/FernfawnReference/`.
Its 18 joints keep the quadruped names/hierarchy but use anatomical rest points.
The separate `LoadGlbMeshWithSkeletonTopology` API validates that new rest rig;
the strict original shared-rig import still refuses mismatched binds.

The same anatomical authoring recipe accepts `-- Thicketbuck` or `-- Sylvastag`
after the Python script argument. These write separate candidates under
`creature_work/GroveDeer/<Name>/`. Adult proportions, fading juvenile spots,
broader crowns and separately composed fern-bearing antlers distinguish the
two later stages. Antler branch junctions are fused before UV mapping; all
parts use one named UV layer before joining, and the export refuses a collapsed
material atlas. Source facial lighting is equalised while retaining muzzle
detail. No default Fernfawn source is overwritten by these named invocations.

`ZM_AuthoredGroveDeerCapture_Test` captures the two adult sources in nine poses
each. Their explicit importer uses the grove-deer side-fall recipe and retains
the other quadruped clips. The family capture runner also supports the existing
three-member stone and fungal reviews. Source texture and skin validity remain
separate from photorealistic visual acceptance.

`ZM_AuthoredCreatureStudyCapture_Test` builds an isolated review bundle from
that reference-derived GLB, then captures
Idle, Walk, Attack, Special, Hit and Faint in an engine review world. It never
overwrites the runtime bundle directly. Both this harness and the production
bake now call `ZM_AuthoredCreature::Try`, which imports the normal/shiny pair,
anatomical rig, material sections and six clips before publishing model markers.
The normal/shiny sources have also been installed in `Assets/Creatures/Fernfawn`
as candidates. Its manifest checkbox remains unchecked pending visual finish.

`prepare_creature_study.py --directory <review-directory>` creates a six-pose review page and contact
sheet from those engine TGAs, preserving the full screenshots beside the crops.
The initial captures showed poor shoulder/neck deformation in the action poses;
successful import is not acceptance of that deformation. The anatomical source
improves shoulder fit; its new Faint recipe authors a grounded side fall.
Use `--extended` to include both intermediate fall poses and shiny Idle.
See `CREATURE_REVIEW.md` for measured results and outstanding acceptance items.

`create_shiny_variant.py <normal.glb> <Name>_shiny.glb` generates and audits the
hue-only variant. It leaves all original geometry/skin/relief bytes intact.
After copying the pair and editable blend to `Assets/Creatures/<Name>/`, a tools
boot imports them through the creature owner. Bump its receipt version when
changing the importer or motion recipe so old cached bundles cannot remain warm.

`source_references.json` records provenance and hashes for external source art
assessed during development. `inspect_deer_reference.py` examines the CC0 CDmir
deer and renders its legacy diffuse/normal materials with modern preview wiring.
Run Blender with `--disable-autoexec` when opening that source. Its assessment
render is an offline reference, not Zenithmon delivery evidence.

`create_stone_creatures.py` composes Rubblet, Cairngo and Monolode individually,
with rounded cobble, balanced cairn and fractured monolith forms. Run it with
Blender `--disable-autoexec --background --python Tools/Art/create_stone_creatures.py`.
An optional name after `--` limits the output to that species. The pinned CC0
Rock Face downloads are listed in `source_references.json`; derivatives use
2K base colour, OpenGL normal and packed AO/roughness/metallic maps. Original
downloads are retained, and the recipe writes only under `creature_work/StoneLine`.
Run `create_shiny_variant.py` for each exported normal GLB before installation.
Loaded sRGB image pixels are exposed by Blender as linear floats. The recipe
re-encodes those RGB values before writing a fresh byte image; copying them
directly would darken the diffuse map. Normal, displacement and packed material
channels remain non-colour data throughout this conversion.

These four-joint stone skins retain the blob names and hierarchy. The explicit
creature importer fits contact at 60 Hz and checks intermediate poses at 120 Hz
for all six clips; Faint is a held sideways topple. Receipt version 2 invalidates
earlier cached bundles. `ZM_AuthoredStoneLineCapture_Test` captures nine views
per species, including shiny Idle and two intermediate falls. Use
`prepare_creature_study.py --name <Name> --directory <directory> --extended`
to prepare each contact sheet. The per-species `ZM_Authored<Name>BattleCapture_Test`
then verifies installed sources in an actual encounter and completed battle.

`create_fungal_creatures.py` composes Sporeling, Duskcap and Fungrove under
`creature_work/FungalLine`, with button, parasol and layered canopy forms.
Use Blender `--disable-autoexec --background --python Tools/Art/create_fungal_creatures.py`;
an optional species name after `--` limits the output. The original geometry
includes lamellae, velum fragments and branching mycelium. Pigment, fibre, normal
and roughness maps are synthesized without external images and embedded in glTF
metallic/roughness materials. Ten named joints retain the floater hierarchy;
tendril axes account for Blender-to-glTF axis conversion. Generate the hue-only
shiny variants with the same audited helper used by the other creatures.

The explicit fungal clip fitter preserves the six actions, adds a restrained
Faint wilt and measures root clearance at 60 Hz with 120 Hz interpolation checks.
The runtime battle presenter supplies the hover offset. Both review and battle
capture harnesses use that same origin policy. `ZM_AuthoredFungalLineCapture_Test`
captures nine poses for each species; the three named battle capture tests
exercise actual grass encounters. Source and material validity do not establish
photorealistic visual acceptance.

## Architecture and cast pass (2026-09-22)

`create_architecture.py` writes thirty building sources and two interior shells.
It reads footprint, wall and door dimensions from the compiled roster/placement
headers and stops if their expected fields change. `create_human_cast.py` writes
the thirty-four missing cast sources under `Assets/Meshes/Humans/`, the directory
owned by the existing shared-rig binder. The engine Male source is preserved.
The cast reuses that supplied anatomical mesh with fitted costume and silhouette
changes; it is not a set of independently sculpted photorealistic faces.

Both recipes accept `-- --replace-authored --only=Name,Name`. Architecture uses
`InteriorPlayerHome` / `InteriorProfLab` as the interior selection names.
Sources and editable `.blend` files are local ignored assets. None of these
first-pass candidates has been marked artist-approved in the brief.

`ZM_AuthoredArchitectureCapture_Test` places all thirty buildings in a dedicated
in-engine review world. `ZM_AuthoredCastCapture_Test` imports missing/stale human
sources through the binder, checks skinned model loading and all five clips the
game consumes, then captures every character in Idle and Walk. The binder's rig
has 51 bones; its imported models do not self-list animation clips. Runtime
controllers explicitly load the shared engine clips.

`prepare_authored_gallery.py` collects actual engine screenshots into labelled
contact sheets and `Build/artifacts/authored_assets/gallery.html`. Original TGAs
remain alongside the captures. Create the requested test-results directory
before launching the executable; the test runner does not create it.

**Outstanding:** 137 primary creature sources and photorealistic visual sign-off
for the candidate deliveries, including Fernfawn.
`create_fernfawn_study.py` / `read_zenith_rig.py` retain an unapproved modelling
experiment outside runtime Assets. The study's embedded glTF skin matches the
existing rig, but the ordinary GLB importer is static and must not import it.
The rejected bulk-parametric approach was not applied to the creature roster.

## Lancefish source development

`create_lancefish.py` composes Finlet, Marlance and Tidesabre in Blender, with
an optional species name after `--`. Output stays under `creature_work/Lancefish`.
A continuous anatomical body, swept fins, modelled fin rays, gill seams and
progressively longer rostra use seven embedded glTF PBR material sections.
Scale and membrane maps are original synthesis. Tidesabre adds metallic lateral
scutes and a broad blade. Eight named aquatic joints retain the shared topology
with anatomical rest positions; blended fin roots follow the adjacent body.

`ZM_AuthoredLancefishCapture_Test` imports the working normal/shiny pair and
captures nine engine poses per species. The explicit creature importer fits
surface clearance across six clips and gives Faint a held whole-body roll.
Clearance sampling includes rotation knots and adaptively refines intervals
where the lowest point switches between a fin and the bill. Checks retain the
3 mm limit at 120 Hz and interval midpoints. The named battle captures verify
the installed models through actual encounters.
These sources require visual acceptance; successful import does not establish
photorealistic anatomy, surface finish or deformation quality.

## Mineral beetle source development

`create_mineral_beetles.py` authors Prismite, Geoborer and Crystallisk using
Blender's Python API, with an optional species name after `--`. Sources remain
under `creature_work/MineralBeetles` until installation. Original paired wing
cases, ventral plates, segmented legs/tarsi/claws, antennae, mandibles and tactile
setae use seven embedded glTF metallic/roughness material sections. Punctate
cuticle, compound-eye relief and mineral/deposition maps are synthesized from
fixed seeds. No external mesh or image asset is incorporated. The Australian
Museum anatomy reference is recorded in the provenance alongside each source.

The 19 named joints preserve the insectoid topology with horizontal anatomical
rest positions. `ZM_FitMineralBeetleClip` limits body bending, supplies horizontal
tripod hip sweeps and a held back fall with folded legs. Surface clearance uses
the same adaptive helper as the lancefish. The family capture covers nine views
per species; named battle captures exercise each installed source in gameplay.
Successful technical checks leave photorealistic visual acceptance outstanding.

Authored-creature battle captures use a healthy level-60 Fernfawn lead and
explicitly require victory, so their exact-return checks do not accidentally
exercise the legitimate loss/whiteout warp. They save and restore the prior
game state during verification and teardown. Run them only with the harness's
isolated test-save root. The ordinary gameplay loss/respawn path is unchanged.

## Missing-prop authoring pass (2026-09-22)

`create_missing_props.py` authors twenty props: LanternPost, FenceWood,
FenceStone, TownBoard, BridgePlank, BridgeStone, RockSmall, RockLarge, Boulder,
LedgeLow, LedgeHigh, the six Dressing biomes, ItemPhial, ItemOrb and ItemTaken.
It preserves the eight existing imports. Run with Blender's bundled Python:

```powershell
& 'C:/Program Files/Blender Foundation/Blender 5.2/blender.exe' --background --python Tools/Art/create_missing_props.py
```

To revise this script's deliveries, append `-- --replace-authored`. To rebuild
selected deliveries, also append `--only=FenceStone,LedgeLow,LedgeHigh`.
The replacement flag only reaches the twenty names owned by this recipe.
Geometry randomness is seeded by asset name so selection order cannot change it.
Each delivery includes a GLB and editable Blender scene under its standard prop
directory. Shared PNGs are retained under `Assets/Props/_AuthoredMaterials` and
embedded in each GLB; the GLB needs no external texture files.

All materials use embedded 1024-square sRGB base colour and linear normal and
metallic/roughness textures (roughness G, metallic B). The phial uses glTF BLEND
glass with a separate liquid mesh; this is alpha glass, not refractive glass.
These are procedural authored candidates, **not a photorealistic acceptance
claim**. The ArtBrief in-flight notes retain that distinction.

`audit_art_brief.py` reconciles all 243 model rows with actual local GLBs and
checks GLB containers, triangle primitives, normals, UVs, material texture
slots and embedded images. It writes review reports under
`Build/artifacts/authored_assets/`; those reports are derived, not a second
asset-status authority.

The existing C++ prop consumers load the new bundles at the same asset refs.
The Dawnmere table supplies outdoor placements, the battle arena supplies six
biome placements, and Route 1 ground-item components choose live/spent models.
LanternPost adds a measured light offset and a per-placement yaw. The two fence
runs move behind the buildings to avoid the old steeply stepped placements.
The three authored rocks override the shared engine-rock fallback in both the
runtime presenter and the authoring-time bounds fit. The showcase verifies the
actual loaded model path, so an engine substitute cannot count as a delivery.

Engine capture coverage:

- `ZM_ImportedPropShowcase_Test`: existing furniture and all new Dawnmere props.
- `ZM_AuthoredDressingCapture_Test`: six live battle-arena biome entities.
- `ZM_GroundItemPropCapture_Test`: Route 1 live pickup presentations.
- `ZM_SpentItemPropCapture_Test`: Route 1 spent state, restoring collected flags
  during teardown. Run tests with the harness's isolated test-save root.

`prepare_prop_screenshots.py` preserves original TGA captures, writes PNGs and
builds a contact sheet. Its optional-looking viewport crop is deliberately
specific to the observed 1280x720 editor layout; full screenshots are retained
beside it with `_editor` in the name.

## SignPost — AB-PROP-11

`create_signpost.py` authors the SignPost in a new Blender scene using `bpy`
and NumPy. Run it in Blender's Python environment. It preserves existing scenes
and writes the isolated editable scene and GLB to
`Games/Zenithmon/Assets/Props/SignPost/`.

The prop has two blank oak arrow boards, beveled edges, four iron bolt/washer
assemblies, a post cap and a foot ferrule. The blank faces deliberately carry
no lettering, as required by the art brief. Wood texture synthesis uses a fixed
seed, irregular growth bands and longitudinal pores.

Delivery: 0.90 m wide, 2.00 m tall, 0.1515 m deep; 4,768 triangles. glTF +Y is
up, the base is at Y=0, and the bolted faces point toward +Z. Separate weathered
oak and forged iron PBR materials each embed 1024-square PNG base colour,
tangent-space normal and metallic/roughness maps. Base colour is sRGB; normal
and metallic/roughness data are linear. Roughness occupies G and metallic
occupies B, following glTF 2.0. The GLB contains one mesh with two primitives,
exercising the renderer's per-section material and index-range handling.

The tools boot imports the GLB into the standard `.zmesh`, `.zmodel`, `.zmtrl`
and `.ztxtr` bundle. The generated fallback remains in `ZM_PropGen`.
Assets stay gitignored under the project's standing source-art policy; this
script is retained so the asset can be recreated.

World placements are `DawnmereSignHome` and `DawnmereSignRoute`, in
`ZM_DawnmereDressing.h`. The existing `ZM_ImportedPropShowcase_Test` captures
both placements, checks loaded model bounds and authored scale, asserts two
complete material sections with distinct materials, and validates the captured
TGA files. After a tools boot imports the GLB, run:

```powershell
./zenith.bat test Zenithmon --filter ZM_ImportedPropShowcase_Test
```

The test command skips tool exports by default. To import a newly exported
GLB, run a normal tools boot first, or use the test harness module's
`-NoSkipToolExports` option with a Null `-DiscoveryExe`.
# Fire biped sources

`create_ember_bipeds.py` authors Kindlet, Scorchel and Pyroclast in an isolated
Blender Python process. It unions and relaxes the body volumes before UV
unwrapping, then bakes object-space pigmentation, roughness and scale relief to
embedded 2048px glTF textures. Metallic is zero; roughness occupies green and
metallic blue. Eyes, claws, mouth seams and mineral ridges use separate material
sections. The fourteen joint names and parents match the canonical biped rig,
with anatomical rest positions and normalized weights.

Run with Blender's `--background --factory-startup --disable-autoexec
--python-exit-code 1 --python Tools/Art/create_ember_bipeds.py`; optional species
names follow `--`. Output lives under `creature_work/EmberBipeds`. The original
geometry and textures have no external source dependency. Shiny variants use
`create_shiny_variant.py` without changing geometry, skin, relief or alpha.

The explicit fire-family C++ motion adapter fits whole-surface ground contact
and replaces the placeholder faint with a side collapse. The authoring recipe
and exported sources remain development candidates: anatomy, scale variation,
mineral erosion and photorealistic appearance still require finishing. Texture
slot completeness and successful import are not visual acceptance.

## Lark sources

`create_lark_creatures.py` authors Pipwit, Trillark and Stratavis in Blender's
Python API. Use the same isolated Blender flags as above and pass optional
species names after `--`. Outputs are under `creature_work/LarkLine`.
Continuous body profiles carry layered contour feathers; folded wings use
primaries, secondaries and coverts, with twelve tail feathers, scaled toes,
claws, eyes and separate upper/lower bills. The thirteen canonical avian joint
names and parents are retained. The C++ adapter fits whole-surface contact
and a side-collapse faint; normal/shiny geometry and weights are identical.

The colour atlas in `textures/lark_feather_atlas.png` was generated with the
built-in image-generation tool; its exact prompt is retained beside it.
Its native resolution is 1254 square, supplying four 627-square colour tiles.
It is generated imagery, not a photograph or measured reflectance. Authored
normal and packed metallic/roughness maps supplement those tiles, using glTF
green roughness and blue metallic channels. All bird surfaces are dielectric.
Feather repetition, wing-root transitions, anatomical detail and appearance
still need art review. These sources remain development candidates.
