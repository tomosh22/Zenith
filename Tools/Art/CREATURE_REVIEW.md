# Creature development findings

The source inventory now has fifteen creature candidates (the grove-deer,
stone, fungal, lancefish and mineral-beetle lines, each with a shiny variant)
in runtime Assets and 137 creature rows without a GLB. All 152 creature
checkboxes remain unchecked: source presence does not establish the requested
photorealistic finish. Earlier Fernfawn studies remain development artifacts.

The earlier `create_fernfawn.py` study has a continuous remeshed body, 2K coat
maps, guard-hair geometry, shaped ears/hooves and an 18-bone GLB skin. The
196,738-triangle export passes the read-only skin/PBR audit. It is too expensive
for its visual result and still looks stylized rather than photorealistic.

The engine's new `LoadGlbMeshForSharedSkeleton` path preserves artist weights
and remaps joints by name. Blender's joint order differs substantially from
the engine order. Rest matrices, hierarchy and four normalized weights are
validated before the caller's mesh is changed. Production integration now uses
the explicit creature owner described below; static publication refuses skins.

`ZM_AuthoredCreatureStudyCapture_Test` imports to an isolated artifact bundle
and samples all six existing clips in an engine review world. Its success
means the import, runtime skeleton, clip sampling and screenshots worked. The
earlier captures reveal poor neck/shoulder deformation, especially Special and
Faint; a test result is not a visual-acceptance result.

Matching bind matrices alone does not establish anatomical fit. The shipped
Fernfawn Head pivot is at Y=0.530371 m while this study's eyes are at Y=0.738 m;
the front upper-leg pivots are at Y=0.315 m, substantially below its shoulder
mass. Future work must resolve that fit, either by fitting the source to the
existing pivots or by providing an anatomical rest rig and correctly retargeting
the six clips. Do not simply relax the bind-matrix check to allow a mismatch.

The CC0 CDmir deer is now adapted by `create_fernfawn_from_reference.py`. Its
source hash and licence are in `source_references.json`; the original file is
unchanged and opened with auto-execution disabled. The derived study shortens
the muzzle, narrows the trunk, subdivides the anatomical surface, repaints the
coat and hoof regions, adds silhouette fibres and a fern crown, and remaps the
skin to an anatomical rest rig. It exports five embedded glTF metallic/roughness
materials and 98,389 triangles under `creature_work/FernfawnReference`.

`LoadGlbMeshWithSkeletonTopology` is a separate, explicit rest-rig import path.
It preserves all 18 names and parents and accepts translated joints only when
both source and template have identity rest axes and unit scale. It derives
the new rig, then invokes the existing strict importer to validate source node
transforms, inverse binds and weights together. Inconsistent files leave both
outputs unchanged. The two new unit cases cover changed anatomical proportions
and refusal of an inconsistent inverse bind. The full suite passed: 4,619 ran,
4,617 passed, zero failed, two skipped (`anatomical_rig_units.log`).

The capture harness targets that reference-derived source explicitly. The five
non-Faint quadruped clips carry rotations only, so local rest translations remain
those of the artist rig. `ZM_AuthoredCreatureMotion.h` replaces Fernfawn's Faint
with a knee yield and side fall; the former cumulative spine pitch produced an
unacceptable nose-first fold. The new root position keys are measured from the
complete skinned mesh at 60 Hz, then checked at 120 Hz for ground clearance.
The revised end pose and two intermediate poses are captured in the engine.

`create_shiny_variant.py` cyclically permutes only embedded base-colour PNG RGB
channels: a 120-degree hue rotation with unchanged saturation, value and alpha.
The audit proves that geometry, skin, material parameters and non-colour image
bytes remain identical. The runtime models share the same mesh and six clips.

`ZM_AuthoredCreature.h` now imports the pair as a bundle. It derives the named
anatomical rig from the generated topology, validates both skins, publishes
prerequisites before either model marker, and records output sizes/timestamps.
A warm check verifies both sources, every recorded output and all six clips.
An incomplete source pair or failed import owns the path and cannot fall back
to procedural geometry. The static GLB publisher now refuses skins before
writing anything, with a sentinel-output regression test. Explicit creature
and clip bakes and the tools resource-initialization step use the same owner.
The review capture imports through this production helper and confirms the
warm invocation leaves the model and Faint files unchanged.

The normal/shiny GLBs and editable blend have been copied into
`Assets/Creatures/Fernfawn` as candidates. The coat transition, face proportions
and overall visual finish remain below acceptance. Battle placement now uses
the loaded creature bounds and the platform's actual top, including switches;
the former fixed centre offset left grounded creatures above the slab.

`prepare_creature_study.py --directory <review-directory> --extended` requires
nine captures (six main poses, two intermediate Faint poses and shiny Idle),
preserves full PNGs, and produces the viewport contact sheet. Without `--extended`
it retains support for the older six-pose sets. The
earlier procedural source and its captures remain available in the separate
`creature_work/Fernfawn` directory for comparison.

The production-source boot suite passed: 4,620 ran, 4,618 passed, zero failed,
two skipped (`authored_creature_units.log`). The real grass encounter and battle
capture also passed (`ZM_AuthoredCreatureBattleCapture_Test`), including loaded
source identity, platform grounding, battle completion and overworld return.
The two framebuffer captures are in `screenshots/Fernfawn_battle*.tga`; the
gallery preserves full PNGs and viewport crops. The arena's existing void and
HUD overlap remain visible and are not model acceptance evidence.

Still required before Fernfawn's checkbox can be checked: accepted individual
design and photorealistic surface and final animation/art review. The other
137 source-less creatures still need their models, skins, materials, shiny
variants, animation review and runtime evidence.

The stone line uses original composed geometry and pinned CC0 photographic
Rock Face material maps from Poly Haven. Rubblet is a rounded cobble, Cairngo
a balanced cairn, and Monolode an upright fractured slab with side buttresses.
Each retains four blob joint names and hierarchy with translated pivots fitted
to its separate stone sections. The normal/shiny sources and editable scenes
are installed as candidates. The photographic maps are retained at original
precision for authoring; exported derivatives use byte-encoded 2K PNGs, reducing
each normal GLB from approximately 64 MB to 24 MB.

The owner fits ground contact across all six clips at 60 Hz and validates at
120 Hz. Faint rolls onto the side instead of deflating as a soft blob. The first
27-pose engine review passed in 3,151 frames (`stone_capture.log`). Surface
detail reads better than the procedural stone palette, while quartz integration,
eye treatment, section intersections and overall art finish remain review items.

After correcting the byte-image sRGB encoding, all three installed stone models
passed their real encounter/battle round trips, and the final 27-pose review
passed again (`stone_verified/*.json`, `stone_final_capture.log`). The normal
GLBs contain 35,504 / 54,160 / 54,902 triangles respectively; each has four
materials and four joints. Six audits against the production skeletons passed.
The final framebuffer gallery has 174 images, including six stone battle views
and the 27 stone motion poses. These checks establish runtime operation, not
photorealistic acceptance.

The fungal line adds Sporeling, Duskcap and Fungrove as normal/shiny candidates
with editable Blender scenes under their runtime asset folders. The source
recipe composes button, parasol and layered caps with modelled lamellae, velum
scales, tapered stipes and branching mycelium. Original synthesized pigment and
fibre maps supply embedded glTF base colour, normal and metallic/roughness slots;
no external imagery is used. Sporeling and Duskcap each contain 32,680 triangles,
and Fungrove contains 77,656. Each has five materials and ten floater joints.

The tendril anchors account for the Blender/glTF axis conversion. All six clips
receive measured clearance at 60 Hz and validation at 120 Hz, with a restrained
Faint wilt replacing the placeholder's cumulative fold. The review-world test
passed in 3,151 frames and captured 27 views (`fungal_review/*.json`). Six audits
against those imported rigs passed, as did the exact hue-only shiny audits.
The pose sheets establish working skins and material imports. Surface finish,
organic irregularity, cap/branch intersections and the photorealistic quality
bar remain outstanding; no creature checkbox has been checked.

All three installed fungal sources then passed real encounter/battle round
trips in 216 / 214 / 218 frames respectively (`fungal_verified/*.json`). The
checks include source identity, skinned loading, hovering presentation offset,
battle completion and overworld return. The six normal/shiny production-rig
audits passed with zero joint-position and inverse-bind error. Six battle views
join the motion poses in the 207-image gallery. `fungal_battles_contact.jpg`
labels three separately captured battles; it is not a single combined scene.

Thicketbuck and Sylvastag now use the pinned CC0 deer anatomy with adult
proportions, coats, guard hairs and separately composed fern-bearing antlers.
The first render exposed ear-adjacent antler bases and a pale facial atlas seam;
the bases were moved onto the forehead and broad facial lighting was equalised.
Antler branch crotches were fused, and collapsed UVs introduced by remeshing
were caught during export inspection. The corrected recipe standardises the
UV layer name before joining and refuses a material with no UV extent.

The installed normal/shiny GLBs contain 189,847 and 235,274 triangles respectively,
six material sections and 18 named anatomical joints. Four audits against the
production skeletons passed with maximum bind and joint-position errors below
3e-8. Exact hue-only variants preserve all original geometry and non-colour
resource bytes. Source-copy hashes match the working files.

The initial two-species pose review passed in 2,101 frames, capturing 18 views.
Fernfawn's nine-pose regression also passed after the side-fall function was
generalised to the grove-deer family. Both installed adult sources passed real
grass encounter/battle round trips in 214 and 212 frames (`grove_deer_verified`).
The large Sylvastag framing exposed the other colour variant at one review
frame edge; the staging now separates subjects beyond the camera far plane.
The anatomy, fur/face finish, antler naturalism and action deformation still
need art refinement. Runtime validity is not a photorealistic acceptance claim.
The refreshed adult review passed again in 2,101 frames after that staging
change. Sylvastag's new shiny frame was inspected and contains only the intended
subject. The gallery's inventory counts are now derived from `inventory.json`
rather than copied into the gallery generator as numeric literals.
The same final batch also reran the three-fungus family after the two-deer
family and passed its 3,151-frame review. This checks family-size reset and
the separated normal/shiny staging. The refreshed gallery contains 229 actual
engine captures; the current Vulkan test listing has 90 entries.

Finlet, Marlance and Tidesabre are now installed as original lancefish source
candidates. Each has a continuous body, modelled fins/rays, gill seams, eyes and
rostrum, seven embedded PBR sections and eight aquatic joints with anatomical
rest points. The normal/shiny geometry is identical. Finlet and Marlance each
have 62,520 triangles; Tidesabre has 62,632 including its metallic scutes.
Six production skin/PBR audits passed with zero joint-position and bind errors.

The first review failed at Finlet's Attack clearance; after reducing the
placeholder's excessive cumulative pitch, Marlance exposed a 6.5 mm clearance
error as the lowest point switched from fin to bill. The final fitter includes
rotation knots and adaptively refines clearance intervals to 0.5 mm midpoint
error before checking the completed clip at 120 Hz and every interval midpoint
against the unchanged 3 mm limit. Faint uses a held whole-body roll.
The corrected family review passed in 3,151 frames with all 27 screenshots
(`lancefish_final_review`). Initial failures remain in the earlier logs.

The pose sheets show coherent fins and skins, but the fish still look smooth
and stylized. Scale/skin detail, fin translucency, facial anatomy, scute
integration and final motion review remain outstanding for photorealistic
acceptance. No creature delivery checkbox is closed by this installation.

All three installed lancefish sources passed actual encounter/battle round
trips in 208 / 210 / 212 frames (`lancefish_verified`), including source identity,
skinned loading, hover placement, battle completion and overworld return.
The Vulkan build passed and the registry listing contains 94 entries. The
refreshed gallery contains 262 actual engine frames. `lancefish_battles_contact.jpg`
labels three separately captured battles; full images remain beside the crops.

Prismite, Geoborer and Crystallisk now have original beetle sources, editable
scenes, embedded PBR textures and exact hue-only shiny variants installed under
their runtime folders. Their 19-joint rigs retain the insectoid topology with
horizontal anatomical rest positions. Seven material sections cover cuticle,
joint membranes, compound eyes, claws/mandibles, mineral growth and setae.
The exports contain 106,032 / 106,864 / 107,824 triangles. All six working-source
skin/PBR audits passed; maximum bind error is below 6e-8.

The first render had overly round head/thorax forms and broad pink pigment;
the revised source flattens those plates, reduces pigment contrast, insets the
eyes and adds eye relief and mineral-root deposits. Geometry includes paired
elytra, ventrites, segmented tarsi, paired claws, tibial spurs, lamellate antennae
and tactile setae. The Australian Museum glossary informed anatomy only; no
external mesh or image asset is incorporated.

The initial 27-pose technical review passed but visual inspection rejected a
Faint pose balancing on bent legs. Reversing the fold and increasing the roll
settles the beetle on its back. The corrected review passed in 3,151 frames
(`beetle_final_review`); final and intermediate poses were inspected. The shared
adaptive clearance helper also passed the full 27-pose lancefish regression,
and all 18 lancefish animation files remained byte-identical after extraction.

The new beetles remain development candidates. Their regular crystal placement,
opaque mineral response, repeated surface detail and similar stage anatomy need
further artistic refinement. The corrected fall removes a visible pose defect;
it does not establish final photorealistic or animation acceptance.

All three installed beetles passed the final real-battle batch in 208 frames
each. Finlet followed in 208 frames, then the original battle round-trip test
passed in 93 frames (`beetle_final_verified`). The earlier batch failed exact
return checks for Prismite and Crystallisk with a TownCenter warp and disabled
movement, consistent with the legitimate loss/whiteout path. The capture
fixture now installs a healthy level-60 lead, requires an observed victory and
restores the prior game state during verification and teardown. No gameplay
loss/respawn code changed. Earlier failures are retained in `beetle_verified`.
All six production source audits passed with maximum bind error below 6e-8.
The latest Vulkan build passed; the registry listing contains 98 entries and
the gallery contains 295 actual engine captures. `beetle_battles_contact.jpg`
labels three separately captured encounters, with full frames preserved.

Prismite now has a second anatomical revision based on Artec 3D's attributed
Beetle scan. The archive and source mesh/photo/license hashes are pinned in
`source_references.json`. The archived CC BY 3.0 notice and the publisher's
CC BY 4.0 offer are both recorded; derivative folders retain the full notice,
attribution and modification description. Copyright metadata is embedded in
the normal and shiny GLBs. The earlier procedural runtime sources are backed
up under `prismite_before_scan`, and the original working family is retained.

`create_scanned_mineral_beetles.py` supplies 300,576 triangles, two embedded
material sections and an anatomical 19-joint rig. The 2K photo colour is
explicitly decoded from sRGB for exposure adjustment and re-encoded; roughness
is authored and metallic remains zero. Scanned geometry supplies surface relief,
with a neutral tangent normal map. This is not a claim of measured PBR data or
fully removed photographic lighting. Sparse original mineral clusters are
positioned by ray hits on the wing cases.

The initial surface-distance skin passed technical checks but visual review
rejected wing-edge deformation and a stretched front leg in Special. Blender
automatic weighting with an anatomical proxy rig removes those obvious defects;
the canonical export rig retains identity joint axes. The corrected nine-pose
review passed in 1,051 frames (`beetle_scan_final_review`) and all frames were
inspected. The final normal/shiny source audits passed with bind error below
4e-9. Attribution was then added to the JSON headers only; byte-identical BIN
chunks prove that the captured geometry, skin and textures were unchanged.

Eight source/credit files replace the former Prismite candidate under its
runtime folder. Source presence and the improved anatomical detail do not close
its art checkbox: mineral integration, residual photo lighting, motion polish
and the 300k triangle budget still require review. The missing-primary count
remains 137. Geoborer and Crystallisk retain their prior procedural candidates.

The installed scan revision passed the actual Prismite battle capture in 208
frames, followed by the original battle round-trip test in 93 frames
(`beetle_scan_verified`). Both production source audits passed with bind error
below 4e-9. The build passed, and nine additional review-world frames bring the
gallery to 304 images; Prismite's two gameplay captures now show this revision.

Minnet, Shoalfin and Torrentfin now have separate authored sources from the
pinned Artec Fish scan. Each model retains the aquatic eight-joint topology,
with individualized width/depth, dorsal height and tail fork. The source's fins
are incomplete, so new membranes, fin rays and eye overlays supplement the
scanned body. The photo's asymmetrical eye and pectoral positions are handled
per side; the first mirrored eye placement was rejected before installation.

The 498,859-triangle body source is decimated, remapped onto new cylindrical
UVs and baked from the high-resolution surface. Each complete export contains
129,550 triangles and five embedded PBR sections. The original photo is 1K;
the 2K derivative does not add photographic resolution. The bake preserves
surface detail in tangent normals, while roughness is separately authored.
The archive's CC BY 3.0 notice, publisher's CC BY 4.0 offer, attribution and
modification description are retained, including GLB copyright metadata.

All six normal/shiny source audits passed with zero joint and bind error.
The original 27-pose engine review passed in 3,151 frames; a closer side-view
review also passed in 3,151 frames (`shoalfish_final_review`), and every pose
was inspected. The sources use the existing authored aquatic motion fitter
and measured clearance. Twenty-one source/credit files were installed with
matching SHA-256 hashes; no pre-existing artist file was overwritten.

These three remain art candidates. Photographic lighting, fin-root integration,
face detail and animation polish need further work. Fin membranes are opaque:
`Flux_TranslucencyImpl::GatherDrawPacket` currently skips animated translucent
submeshes explicitly. Supporting skinned translucency and reviewing the result
is an outstanding renderer requirement for convincing fins, not a completed
photorealistic delivery. The missing primary-creature count is now 134.

All three installed shoalfish passed their actual battle captures in 208 frames
each, followed by the original battle round-trip test in 93 frames
(`shoalfish_verified`). All six production source audits passed with zero joint
and bind error. The updated Vulkan build passed and the gallery now contains
337 engine captures. `shoalfish_battles_contact.jpg` labels three separately
captured battles; full frames and original TGAs remain available.

The shoalfish fin revision uses single two-sided membrane surfaces and glTF
BLEND opacity textures, reducing each source to 124,054 triangles. The engine
forward pass now reads the same compute-skinned arena as opaque creature
sections, with an explicit render-graph vertex-buffer dependency. GLB base
colour imports use alpha-preserving BC3 sRGB for non-opaque materials; the
creature cache receipt is v3 so older exports cannot hide that change.

The six working source audits include decoding the actual BC3 opacity blocks:
source alpha ranges are 79-221/222, with a mean compression error of 0.451 out
of 255. Normal/shiny geometry and opacity still match. The 27-pose alpha review
passed in 3,151 frames (`translucent_alpha_review`) and every image was inspected.
These are improved candidates, not final photorealistic sign-off: fin-root and
ray detail, photographic lighting, facial anatomy and motion still need work.
The core alpha blend approximates membrane opacity, not measured tissue
transmission. External animation-preview translucent skins remain unsupported.

The final regression rerun (`translucent_alpha_final_review`) passed in 3,151
frames / 84,841.773 ms. It checks real animated draw offsets, BC3 sRGB textures,
skinning with zero opaque draws, and clearing the dispatch when jobs disappear.
The Vulkan build and unit suite passed: 4,620 ran, 4,618 passed, zero failed,
two skipped (`translucent_skin_units.log`). No boot unit or registry count changed.
All six installed source/texture audits pass with the same 0.451/255 mean alpha
error as the working exports. Twenty-one source and attribution files were
hash-verified against the previous installation before replacement, with backups
under `shoalfish_before_translucency`.

The full engine complexity/lint gate remains red on existing graph, normal-lint,
editor-singleton and unmapped `.zagent/run` findings. The changed renderer
functions have no reported gate violations after separating scene gather from
external-item gathering; no allowlist was weakened. The final report is
`translucent_skin_complexity_final/analysis_report.md`.

All three revised production fish passed fresh real battle captures in 208
frames each; the original battle round-trip regression passed in 93 frames
(`translucent_skin_verified`). The refreshed gallery retains 337 engine frames,
including the 27 revised shoalfish poses and six updated gameplay captures.
`shoalfish_battles_contact.jpg` labels three separate battle screenshots.
No missing primary source was added in this renderer/material refinement:
134 creature primaries remain absent, and no art checkbox was closed.

## Windseed botanical family

`create_windseed_creatures.py` authors Puffseed, Dandelift and Zephyrbloom as
original Blender sources. Six serrated leaves, curled root filaments, ribbed
seed bodies, individual achenes/beaks and open pappus structures use modeled
geometry. Zephyrbloom carries two flowering branches. Kew's dandelion page and
the published pappus study inform botanical structure; no downloaded geometry
or images enter these sources. Textures are original base colour, tangent
normal and packed metallic/roughness maps, with dielectric tissue settings.

The normal/shiny technical audits pass with ten canonical floater joints and
zero joint/bind error. Counts are 101,524 triangles / 77,141 exported vertices
for Puffseed, 182,556 / 143,947 for Dandelift, and 291,084 / 230,725 for
Zephyrbloom. The hue-only shiny variant retains all geometry and skin data.
Their triangle budgets and filament aliasing still require optimization.

The first capture failed while fitting Dandelift's Hit clearance. Windseed now
opts into rotation-knot-aware adaptive whole-surface clearance; existing fungal
clips retain their original sampler. The corrected 27-pose motion test passed
in 3,151 frames / 124,845.992 ms. Visual inspection then found that Zephyrbloom's
pod and leaves appeared almost black in the review lighting. Albedo and normal
debug captures isolated an importer winding defect: a signed-volume heuristic
left Zephyrbloom's open surfaces facing the wrong way. The source sum is
-0.006097 cubic metres because leaves and petals are open sheets; it does not
describe their facing. The importer now converts glTF winding per node's
transform determinant, including mirrored nodes. A regression covers open
triangles at negative, zero and positive offsets with both transform parities.
The creature receipt is v4 so earlier exports cannot conceal the correction.

An index comparison found no changes to the 18 existing creature meshes or the
53 comparable static meshes. The separate humanoid `.zasset` bundles and eight
compressed retained sources were outside that comparison. These models remain
candidates, not accepted photorealistic deliveries.

The winding-corrected 27-pose review passed in 3,151 frames / 129,380.562 ms,
and all final captures were inspected. Fifteen previously absent source files
were installed with hash checks (`windseed_install_audit.json`). Six production
skin/PBR audits passed, with every runtime triangle index checked against the
required GLB conversion. Puffseed, Dandelift and Zephyrbloom each passed their
real battle capture in 208 frames, followed by the original round-trip test in
93 frames (`windseed_verified`). The gallery now contains 370 engine frames;
131 creature primary sources remain missing. No art checkbox was closed.

The Vulkan build and all three Null builds passed. Separate unit runs observed
Zenithmon 4,584 ran / 4,582 passed / zero failed / two skipped, Combat 2,731 /
2,730 / zero / one, and RenderTest 2,834 / 2,833 / zero / one. The RenderTest
fixture-write sandbox denial was resolved by rerunning with the required access.
These observed counts are pinned; the Vulkan listing reports 108 automated
tests (`windseed_registry.log`). The earlier unrelated full engine lint and
complexity gate findings remain outside this change.

## Kindlet / Scorchel / Pyroclast — original fire biped sources

`create_ember_bipeds.py` authors a continuous voxel-unioned dermis, clawed limbs,
short balancing tail, facial detail and mineralised dorsal crest. Object-space
pigmentation, scale relief and roughness are baked onto a UV atlas before GLB
export. The sources embed metallic/roughness PBR textures and retain fourteen
canonical joint names/parents with anatomical rest positions. All geometry and
maps are original; no external mesh or image is used.

The first review identified overly pale pigmentation, excess geometry and
abrupt hip/shoulder skin transitions. Revisions reduced the dermis density,
added knee volume, varied adult proportions and smoothed limb-to-trunk weights.
The C++ adapter fits contact against the complete skinned surface and authors
a side collapse. The material values are artistic estimates, not measurements.
The rounded anatomy, hip/shoulder folds, uniform scale detail, eye seating and
mineral erosion still need finishing. No photorealistic acceptance is claimed.

Final exported normal/shiny geometry:

| Species | Exported vertices | Triangles | Joints | Materials |
|---|---:|---:|---:|---:|
| Kindlet | 48,644 | 84,494 | 14 | 7 |
| Scorchel | 49,330 | 85,452 | 14 | 7 |
| Pyroclast | 51,387 | 88,252 | 14 | 7 |

The final 27-frame pose/shiny review passed in 3,151 frames / 79,283.062 ms
(`ember_review/result.json`); its captures were inspected. Six working-source
skin/PBR audits passed before the guarded installation of fifteen previously
absent source files. Six production audits then verified the runtime positions,
triangle winding, weights and joint remapping against both GLBs. All fifteen
installed hashes remain unchanged (`ember_production_audits.json`,
`ember_install_audit.json`). Hue variants preserve geometry, relief and alpha.

Each real battle capture passed in 208 frames: Kindlet 15,333.704 ms,
Scorchel 14,528.511 ms, Pyroclast 14,376.066 ms. The original battle round trip
also passed in 93 frames / 11,446.759 ms (`ember_verified`). The labelled
`ember_battles_contact.jpg` contains three separate actual engine captures.

Vulkan and Null Zenithmon builds passed. The Null unit run observed 4,584 ran,
4,582 passed, zero failures and two skips (`ember_null_units.log`), unchanged
from the pinned baseline. The clean registry run reports 112 entries. Its first
attempt hit a profile-write denial during shutdown; rerunning with an isolated
workspace profile exited successfully. No shared engine code changed this turn.

Inventory now contains 143 GLBs: 110 new primary candidates, 24 shiny variants
and nine retained sources. The gallery references 403 engine frames. There are
128 missing creature primary GLBs, and all creature art checkboxes stay open.
The prior unrelated full-engine lint/complexity gate findings remain unresolved.

## Pipwit / Trillark / Stratavis — original lark sources

`create_lark_creatures.py` authors continuous body profiles, layered contour
feathers, folded primary/secondary/coverts, twelve tail feathers, scaled feet,
claws and separate horn mandibles. Thirteen canonical avian joints carry
anatomical rest positions. A dedicated C++ adapter fits whole-surface clearance
and a side-collapse faint while preserving the shared joint contract.

The first segmented body was replaced by a continuous profile and the exposed
wing core was reduced and feathered. The final 27-pose/shiny engine review
passed in 3,151 frames / 140,813.062 ms (`lark_review/result.json`); all contact
sheets were inspected. Six working-source skin/PBR audits passed before
installing fifteen previously absent source files (`lark_install_audit.json`).

| Species | Exported vertices | Triangles | Joints | Materials |
|---|---:|---:|---:|---:|
| Pipwit | 159,425 | 203,228 | 13 | 12 |
| Trillark | 159,165 | 202,924 | 13 | 12 |
| Stratavis | 159,535 | 203,484 | 13 | 12 |

The colour atlas was generated using the built-in image-generation tool;
`textures/lark_feather_atlas.prompt.txt` retains the exact prompt. The saved
1254-square atlas supplies four 627-square tiles; authored normal/roughness
maps supplement these. It is generated imagery, not photography or measured
reflectance. All materials are dielectric and use glTF metallic/roughness
channel conventions. The original geometry uses references only for structure.

The contour pattern still reads as regular tiles at close range. Wing roots,
bill and eye anatomy, feather variation, the roughly 203k-triangle budgets and
motion polish need further work. These are development candidates; no
photorealistic acceptance or completed creature art checkbox is claimed.

Six production audits verify runtime positions, converted triangle indices,
joint remapping and weights against both source GLBs. All fifteen installed
source hashes are unchanged. The audit initially needed its uint16 per-section
indices widened to int64 before adding global offsets; this was a verifier
overflow, not a geometry change. The final report is
`lark_production_audits.json`.

All three real battle captures passed in 208 frames each: Pipwit 21,982.596 ms,
Trillark 23,092.160 ms and Stratavis 21,470.928 ms. The original battle round
trip passed in 93 frames / 17,062.350 ms (`lark_verified`). The labelled
`lark_battles_contact.jpg` retains three separate actual battle screenshots.
The refreshed gallery contains 436 actual engine frames. Inventory now has
149 GLBs: 113 new primary candidates, 27 shiny variants and nine retained
sources. There are 125 missing creature primary GLBs.

The Vulkan/Null Zenithmon builds passed, and the Null unit run reports 4,584
ran / 4,582 passed / zero failed / two skipped. The clean Vulkan registry lists
116 automated tests. The user requested a stop after this family and a commit
of the accumulated work; no further family was started. The full photorealistic
goal remains incomplete.

## Stop and commit verification

The required engine/cross-project builds and Zenithmon's D3D12 link proof
passed (`stop_gate/build_results.json`, `zenithmon_d3d12_build.log`). Combat's
full headless suite passed: 17 results, zero failures, one graphics skip.
The full unit gates passed with exports enabled: Combat 2,731 ran / 2,730
passed / zero failed / one skipped, and Zenithmon 4,584 / 4,582 / zero / two.

The first full Zenithmon headless run returned 114 passed and two failed out
of 116, including 60 graphics skips. The CLI adds the batch's nonzero exit as
a third summary failure; it is not a third failing test. The two checks still
expected generated NPC paths and one mesh per generated interior surface.
They now independently resolve authored NPC paths and require skeletons, and
validate each room's authored model identity and every material section with
explicit recipe counts. Generated fallback checks remain in place. Final
Vulkan and Null builds pass. Both corrected tests passed without skips:
InteriorTint in 17 frames / 6,217.240 ms and RivalVesperAuthored in 401 frames /
29,516.740 ms (`stop_gate/corrected`). The full 116-test batch was not repeated
after these two test-only corrections. Both game documentation lints passed.

The full engine lint/complexity run still reports the same twelve existing
findings in graph validation, normal-decode lint, the material editor singleton
and an unmapped `.zagent/run` scratch file. None is in this change's files, and
no threshold or allowlist was relaxed. This is not an all-green full gate.
Python recipes parse, and staged whitespace checks pass excluding the two
upstream license notices, whose original CR/CR/LF bytes and SHA-256 are retained.

The commit includes code, recipes, tests, provenance, documentation and the
generated feather atlas. Ignored GLB/Blender exports and captures remain local
under the standing source-art policy. Dawnmere's already-dirty scene is left
uncommitted, preserving the documented cross-configuration drift instead of
republishing it. No new asset family was started after the stop request.
