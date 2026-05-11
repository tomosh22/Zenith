# Devil's Playground — Asset Manifest & Placeholder Strategy

**Document purpose:** A complete catalogue of every artist-authored asset *Devil's Playground* requires to ship, together with a per-asset placeholder strategy so engineers and designers are never blocked by missing art. Written so a producer can budget headcount, an outsourcing lead can write briefs, and a build engineer can wire up automation around what the pipeline expects.

**Author:** Art Direction (Claude)
**Companion docs:** [GameDesignDocument.md](GameDesignDocument.md) · [Shortfalls.md](Shortfalls.md) · [TestPlan.md](TestPlan.md)
**Last updated:** 2026-05-11

---

## 0. At-a-Glance

### 0.1 Headline counts

| Discipline | Final-quality asset count | Existing prototype state |
|---|---:|---|
| Concept art (key art + reference) | ~80 paintings | 0 |
| Character meshes (unique) | 27 (24 villager archetypes + Aelfric + Joan + demon-visual proxy) | 0 (capsule colliders) |
| Character animations | ~410 clips (avg 15/char + Aelfric 30 + hounds 20) | 0 |
| Environment meshes (modular kit) | ~180 pieces across 6 districts | ~30 blockout pieces from UE export |
| Prop meshes (items + interactables + dressing) | ~120 | 0 (tinted cubes via `DPMaterials::GetOrCreateColouredVariant`) |
| Materials (`.zmtrl`) | ~220 | ~15 procedural variants per-tag |
| Textures (`.ztxtr`, all channels) | ~900 (≈220 material slots × 4 typical channels + UI + decals) | ~24 imported from UE export |
| VFX configs (`.zdata` particles) | ~32 (matches GDD §8 ref to ~25 systems + variants) | 1 (PFX_Witch placeholder) |
| Decals | ~24 | 1 (existing bullet-hole proxy from `Flux/Decals`) |
| Skyboxes / HDRIs | 4 (clear / mist / rain / dawn) | Engine procedural atmosphere |
| Lighting set-pieces | ~110 hand-placed (24 torches + 30 candles + 8 lanterns + 4 hearths + 8 windows + 36 ambient fills) | ~24 torches from UE export, 1 directional |
| UI elements (sprites + icons + frames) | ~280 | ~6 raw text elements |
| Fonts | 3 (display, body, monospace-diagnostic) | None (engine default) |
| SFX | ~480 | 0 (no audio system) |
| Music | ~14 tracks (2 dynamic gameplay + 7 between-Night beds + menu + 3 cutscene) | 0 |
| VO lines | ~28 (Aelfric ~20 + cabal ~5 + demon Latin chorale ~3 stems) | 0 |
| Cutscenes | 14 (2 per Night) | 0 |

**Bottom line:** the prototype currently uses **< 3% of final asset content**, with the missing 97% concentrated in three buckets: character art (the biggest single line item), audio (which also requires the engine team to build the audio system itself — see §8), and environment / set dressing (volume but largely sourceable).

### 0.2 Three-stage asset quality

Every asset in this document moves through three explicit stages. The placeholder strategy is built around the stages so that **no asset gates engineering work** at any point in the dev cycle.

| Stage | When | Quality bar | Authoring path |
|---|---|---|---|
| **S0 — Sourced placeholder** | Day 1 onwards | "Visually distinct, semantically correct, gameplay-shippable for internal builds." | Bought / free / generated. Imported once, never edited. |
| **S1 — Blockout art** | Phase 1–2 (months 0–10) | "Right shape, right silhouette, monochrome materials. Final-quality lighting." | In-house (one art generalist, ~2 days/asset). |
| **S2 — Final** | Phase 3–4 (months 10–18) | "Ship-quality. Final texture, final material, final animation." | In-house leads + outsourced detail. |

Engineering, design, and QA all operate at S0+S1 quality from month one. The S2 pass is parallel and never blocks gameplay iteration.

### 0.3 Pipeline constraints (load-bearing)

These engine constraints (from `AssetHandling/`, `Flux/MeshAnimation/`, `Flux/Particles/`, `UI/`) bind every artist deliverable.

| Constraint | Value | Implication |
|---|---|---|
| Skeleton bone count | ≤ 100 | Character rigs must fit. Aelfric's full rig (face + cloth) is the tightest — see §2.2. |
| Bones per vertex | ≤ 4 | Standard. Auto-skinned tools default to this. |
| Texture compression formats | BC1, BC3, BC5, BC7, RGBA8, RGBA16F | Author at 2× target res; tool downsamples. Normal maps → BC5 (2-channel). Albedo with alpha → BC3 or BC7. HDR cubemap mips → RGBA16F. |
| Channel packing | Not supported | Materials use one texture per channel (base, metallic, roughness, normal, emissive, AO). 5–6 textures per material is normal. **Do not author packed ARM/ORM textures** — they will be unpacked on import and the second pack channel will be discarded. |
| Particle max count (default) | 256 per emitter | Larger systems need GPU compute mode. |
| Light types | Point, spot, directional | No area lights, no IES profiles. |
| UV channels | 1 (gameplay), 2 (lightmap if added) | Lightmap UVs are an open question for the lighting team. Default to single channel. |
| Async loading | **Not implemented** | All assets load synchronously through `Zenith_AssetRegistry`. Plan loading-screen budgets accordingly. |
| Audio system | **Does not exist** | Engine work prerequisite. See §8. |

### 0.4 Naming conventions

To match the existing `DevilsPlayground_Assets_*` prefix pattern from the UE export (preserved in the prototype's `DPLevelData.h`), every asset uses one of:

```
game:Meshes/dp_<category>_<subject>_<variant>.zmodel
game:Materials/dp_<category>_<subject>_<variant>.zmtrl
game:Textures/dp_<category>_<subject>_<channel>.ztxtr
game:Animations/dp_<archetype>_<verb>_<variant>.zanim
game:Skeletons/dp_<archetype>.zskel
game:Particles/dp_<category>_<subject>.zdata
game:UI/dp_<screen>_<element>.ztxtr
game:Audio/dp_<category>_<subject>_<variant>.zdata
game:Decals/dp_<category>_<subject>.zdata
```

- `<category>` ∈ {char, env, item, interactable, vfx, ui, ambient, music, vo, sfx}.
- `<variant>` ∈ {a, b, c, …} or a single descriptor like `winter`, `rust`, `closed`.
- All lowercase. Underscores for spacing. No spaces, no caps, no UE-style suffixes (`_SK_`, `_PROP_`, etc.).

A linter test (`Tier 3 Test_P3Inventory_AssetNamingConvention`) enforces this on every commit; see [TestPlan.md](TestPlan.md) §4.1.

---

## 1. Concept Art

The first-authored layer. Everything below begins as a painting.

### 1.1 Required deliverables

| Asset | Count | Purpose |
|---|---:|---|
| Key art (campaign promo) | 1 | Steam/console store, posters, press kit |
| Mood paintings (1 per district × 6 districts) | 6 | Anchors environment art's tone |
| Character concept sheets (1 per archetype + Aelfric + Joan + hounds + demon) | 28 | Three-quarter, front, back, silhouette swatch |
| Prop concept sheets (groups of 6 per sheet) | 20 | Item icon and prop modelling reference |
| Interactable concept sheets (1 per kind) | 12 | Door variants, chest variants, forge, pentagram, well, confessional, bell, hearth, lantern, noise machine variants |
| Cutscene storyboard panels | ~150 (~10/cutscene × 14 cutscenes) | Cutscene production |
| Lighting paintings (one per weather + time-of-night combination) | 8 | Reference for lighting artists |
| UI mockup paintings | 12 | Reference for UI artists |

### 1.2 Placeholder strategy

| Stage | What lives in-engine |
|---|---|
| **S0** | None — concept art has no in-engine placeholder. Its purpose is to drive *every other asset's* placeholder choice. Without concept art, S1 placeholders drift into incompatible directions. |
| **S1** | Same. |
| **S2** | Same. |

**Authoring note:** Concept art is the single highest-leverage asset class. **Hire the concept lead in month 0.** Without district mood paintings by month 3, environment art will fork stylistically across the team. Per GDD §8.1 the reference matrix is *Inside* (Playdead), *Darkwood*, *Carrion*, *The Witch (2015)*; pin those plus 2–3 paintings of period-correct 1670s English moor villages (recommend the National Trust archive and the British Folk Art collection at Compton Verney).

---

## 2. Characters

### 2.1 Villager archetypes (24)

Per GDD §6.2, 24 archetypes are required; 14–18 spawn per Night. Each is one mesh + one skeleton + one core animation set + N variants.

#### 2.1.1 Mesh deliverables (per archetype)

| Spec | Value | Rationale |
|---|---|---|
| Tri count | 8–14k tris LOD0; 4–6k LOD1; 1.5k LOD2 (LODs done in import tool, not authored separately by the artist) | Top-down camera at 80 m zoom — facial detail not visible. Silhouette priority. |
| Texture set | base + normal + roughness + metallic (very low) + AO + emissive (for demon-scent indicator) | PBR standard. |
| Texture resolution | 1024² LOD0, 512² LOD1, 256² LOD2 | Top-down view does not benefit from 2K. |
| Materials | 1 per archetype (uniform), plus 4 hat/colour swatches per archetype for crowd variation | Crowd legibility from above demands per-archetype silhouette uniformity but small colour shuffles. |
| Costume design | One per archetype. Differentiated by silhouette first, colour second. | Players must recognise archetype at a glance from 80 m. |

#### 2.1.2 Skeleton

Shared base biped — **dp_char_villager.zskel** — used by all archetypes via re-targeting. ~62 bones (matches Mixamo standard 65 minus 3 unused tail bones). Hat & cloth simulation handled by 8 extra bones authored per-archetype where needed (e.g. Sexton's robe sleeves).

**Constraint:** Hard cap of 100 bones means the most complex villager (the Bell-Ringer with rope-simulation bones) sits at ~78. Plenty of headroom.

#### 2.1.3 Animations per villager

Base set (shared across all archetypes via retargeting): **15 animations**.

| Verb | Length | Notes |
|---|---|---|
| idle_neutral | 4 s loop | Per-archetype variation (idle_sexton, idle_drunk, etc.) — see below. |
| idle_alert | 3 s loop | When a noise just occurred nearby. |
| walk | 1.2 s loop | 4 m/s pace. |
| jog | 0.9 s loop | 8 m/s pace. |
| sprint | 0.7 s loop | 12 m/s pace. |
| sprint_burning | 0.7 s loop | Sprint with the "life-cost" frost effect — same skeleton motion, blendable emissive overlay. |
| possessed_walk | 1.2 s loop | Subtle stagger / unnatural cadence. **Signature animation.** |
| possession_channel | 0.8 s | Devout archetype's resistance to possession — see GDD §4.1. |
| possession_enter | 0.4 s | One-shot. The "demon arrives" anim. |
| faint | 1.5 s | One-shot. After voluntary switch. |
| collapse | 2.0 s | One-shot. Burn-out death. Different from faint — visibly skeletal-collapsed pose. |
| pickup_low | 0.8 s | Items on ground. |
| pickup_high | 0.8 s | Items in chests / on shelves. |
| drop | 0.5 s | G-key drop verb. |
| interact_lever | 0.5 s | Doors, levers, bell-rope. |

Plus **per-archetype unique animations** (avg 3 per archetype):
- Devout: `pray_idle`, `recoil_from_demon`, `pray_loud`
- Smith's Apprentice: `forge_strike` (10 s loop), `bellows_pump`, `wipe_brow`
- Drunk: `idle_sway`, `stumble`, `vomit_recovery` (subtle, used once per Night max)
- Bell-Ringer: `bell_pull` (3 s loop), `cover_ears`
- Child: `child_idle_play`, `child_run`, `child_hide`
- Beggar: `sit`, `beg_idle`, `lie_down`
- … etc.

**Total animation count for villagers:** 24 archetypes × 15 base + 24 × ~3 unique = **~430 clips**. Sourceable from Mixamo or pre-built blockout libraries at S0; re-authored to spec at S2.

#### 2.1.4 Placeholder strategy — characters

| Stage | Placeholder |
|---|---|
| **S0 — Day 1** | **Mixamo standard rigged character** (Mixamo "Y-Bot" or "Erika"), 1 mesh, retargeted across all 24 archetypes. Materials use the existing `DPMaterials::GetOrCreateColouredVariant` system to colour-tag each archetype (Farmhand = brown, Smith = grey, Devout = white-grey, Sexton = black, Child = small + bright, etc.). Animation: Mixamo's free idle + walk + run packs (~ 200 clips available free under Mixamo's terms). |
| **S1 — Months 2–8** | Replace with **Synty POLYGON Fantasy** or **Synty Horror Mansion** pack characters (~$50 / pack, 30+ characters each, low-poly stylised). Re-skin per archetype with the colour-tag layer kept. Idle / walk / run / interact authored fresh on a Synty rig. Hat/silhouette variation real. |
| **S2 — Months 10+** | Final art. In-house lead + outsourced detail. Final rig (the in-engine biped used everywhere). Real cloth sim where it's worth it (Sexton's robe, Aelfric's coat). |

**Critical:** The Mixamo placeholder is a one-day setup. There is no reason to ship internal builds with capsule colliders past month 1.

### 2.2 Aelfric (the Pursuivant)

The most-authored character in the game. He is on screen ~80% of every Night.

#### 2.2.1 Mesh

| Spec | Value |
|---|---|
| Tri count LOD0 | 24k (highest of any character) |
| Costume layers | Inner shirt, leather doublet, long wool coat, boots, hat (steeple-crown), belt with flintlock holster, lantern hand-prop |
| Textures | 2048² base + normal + roughness + metallic (heavy on the lantern + belt buckle) + AO + emissive (lantern interior) |
| Materials | 6 (skin, coat, doublet, boots+hat, belt+metal, lantern glass) |

#### 2.2.2 Animations (~30)

| Category | Anims |
|---|---|
| Locomotion | idle_calm, idle_suspicious, walk_calm, walk_pursue (faster cadence), run, halt, turn_left, turn_right |
| Investigation | kneel_inspect_ground, sweep_lantern_arc, read_object (e.g. inspecting a closed chest), listen_pose, frown |
| Pursuit | raise_flintlock, aim, fire, lower_after_miss, reload (4 s) |
| Apprehend | reach_for_villager (2.5 s windup), restrain (3 s channel loop), restraint_complete |
| Special | bless_villager (one-shot, 4 s; mid-Night escalation), exorcise_pentagram (one-shot, 30 s; late-Night escalation), pray |

**Voice-acting:** ~20 lines from GDD §8.4, delivered at <8 m proximity. Lip-sync handled by the engine via a phoneme-blend-shape system. **This requires blend-shapes on Aelfric's face — the only character with them.** Spec: 12 visemes + 6 emotion blendshapes. (Other villagers don't talk; no blend-shapes needed.)

#### 2.2.3 Placeholder strategy — Aelfric

| Stage | Placeholder |
|---|---|
| **S0** | A taller, darker Mixamo character with a **stovepipe hat hat-only mesh** (5 min to model — a tapered cylinder) and a black robe colour-tag. Distinctly silhouetted as "the priest" from 80 m. Mixamo's free "gunslinger" anims cover raise/aim/fire reasonably; substitute hand-key restrain. |
| **S1** | Synty POLYGON Horror's Witch Hunter character (this character exists in the pack and is closer to the design than any other off-the-shelf option). Custom hat-attach. Lantern as a separate held prop with the lantern-emissive material. |
| **S2** | Final art. The single most-iterated character. Plan two full passes (months 11 and 16). |

### 2.3 Joan Trew

Non-possessable. Appears only from Night 2 onwards. Restrained in the longhouse.

| Spec | Value |
|---|---|
| Tri count LOD0 | 12k |
| Animations | 8 — restrained_idle, restrained_struggle, freed_recoil, freed_walk, freed_run, freed_hide, dialogue_idle (still no speech, just emotional pose), final_cutscene_pose |

#### Placeholder strategy

| Stage | Placeholder |
|---|---|
| **S0** | A Mixamo female character with half-shaved-head silhouette flag (a flat plane texture on the head representing shaved hair). Bound to a chair via parent constraint. |
| **S1** | Synty character + custom hair. |
| **S2** | Final. Hand-authored. |

### 2.4 The Demon (visual proxy)

The demon never has a body. But it has presence:
- A **frost-anchor decal** projected on the ground at the demon's last-death location (between possessions).
- A **frost-trail particle** during the 1.5-s possession cooldown (GDD §4.1).
- A **possession-transition VFX** when entering a body.

These are NOT character art — they're VFX. See §5.

### 2.5 The Hounds (Aram & Tobit)

Two instances of one mesh + one skeleton + one anim set. The hounds are distinct from each other only via material tint (Aram lighter, Tobit darker).

| Spec | Value |
|---|---|
| Tri count LOD0 | 8k |
| Skeleton | ~40 bones (quadruped with neck, jaw, tail) |
| Animations | 20 — idle, idle_alert, idle_sniff, walk, trot, run, sniff_track (5 s loop), bark_burst (1 s), bark_continued (2 s loop), lunge, hold, lie_down, get_up, head_turn_left, head_turn_right, eat_meat (3 s), pant_idle, drink_water, stretch, shake_off |

#### Placeholder strategy

| Stage | Placeholder |
|---|---|
| **S0** | A free wolf rig from Mixamo Marketplace (one available CC-licensed) or Unity Asset Store's "Wolf Animations" free download. Recolour to brown. |
| **S1** | Synty POLYGON Knights / Polygon Adventure has dogs. Adjust proportions slightly. |
| **S2** | Final mastiff / lurcher (period-correct breed). |

---

## 3. Environment

The 200×200 m hand-authored Vexholme map, six districts.

### 3.1 Modular building kit

Most of the village is built from a small kit, reused with material variants.

| Kit piece | Variants | Authoring notes |
|---|---:|---|
| Stone wall section (2 m long) | 4 (clean, mossy, cracked, slate-banded) | Bake corner / end variants from these. |
| Stone wall corner (L-shape) | 4 | Built from the section. |
| Wall doorway (with frame) | 3 | The doorway-frame piece is the door's pivot anchor. |
| Wall window (with frame, multiple sizes) | 6 | Three glazed (with translucent material), three boarded. |
| Stone wall buttress | 2 | Adds silhouette without complexity. |
| Slate roof section (gable / hip / flat / shed) | 8 | Roof tiling tiles 1 m². |
| Roof ridge | 1 | |
| Chimney stack | 4 | Two stone, two brick. |
| Door (single) | 4 | Wooden plank / iron-banded / cellar-hatch / chapel double-leaf component |
| Door (double) | 2 | |
| Floor tile (interior, 1 m²) | 6 | Wood plank, stone slab, packed earth, hearth surround, chapel marble (the chapel only), longhouse straw |
| Stair section | 2 | Stone, wooden |
| Fence section | 4 | Drystone, post-and-rail, hawthorn hedge, iron church railings |
| Gate | 3 | Garden, churchyard, longhouse |

**Total kit:** ~80 pieces.

#### Placeholder strategy — building kit

| Stage | Placeholder |
|---|---|
| **S0** | The existing UE-exported BuildingAssetKit blockout pieces (already present per `DPLevelData.h`). Re-author existing wall-sections to match the new kit's 2 m grid. Single grey untextured material. |
| **S1** | In-house generalist builds the full kit in ~4 weeks (2 days per piece × 80 = 16 weeks of work — split between 2 generalists). Single material per kit piece (stone, wood, slate, marble, etc.) — no per-piece texture authoring; texture comes from a small **trim sheet** library (12 trim sheets total at S1). |
| **S2** | Full per-piece detailing. Roughness variation, lichen, weathering. Outsourced to a single environment-art studio. |

### 3.2 Set dressing props

Non-interactable decorative props that fill the village.

| Prop group | Count |
|---|---:|
| Furniture (table, chair, stool, bench, bed, chest-of-drawers, kitchen counter, weaving loom, spinning wheel) | 18 |
| Containers (basket, sack, crate, barrel, urn) | 12 |
| Tools (scythe, axe, pitchfork, broom, hammer, anvil, bellows, water bucket) | 14 |
| Food / kitchen (loaves, hanging meat, herb bunches, pots, plates, cups, dried fish) | 16 |
| Religious (small altar, prayer book, candle on stand, cross-on-wall, rosary on hook) | 8 |
| Outdoor (cart, plough, horse trough, hitching post, beehive, well-cap, butter churn) | 12 |
| Vegetation (oak, hawthorn hedge, blackthorn bush, heather clump, grass tuft, moss patch, dead branch, leafy ground cover, fern, nettles) | 16 |

**Total dressing:** ~100 props.

#### Placeholder strategy — set dressing

| Stage | Placeholder |
|---|---|
| **S0** | Synty POLYGON Knights / Heist / Western prop packs (~$50 each). One-day mass import. Drop into scenes. |
| **S1** | In-house generalist (1 FTE) authors the prop pack at S1 quality — silhouette correct, single matte material, no surface detail. ~3 days per prop × 100 = ~60 weeks of work. **Outsource at least 60% of this.** |
| **S2** | Full surface detail, second-pass. Outsourced to an asset shop (e.g. an Eastern European or Indian studio that quotes ~$200–$400 per S2 prop). |

### 3.3 Terrain

The map's ground.

| Asset | Notes |
|---|---|
| Heightmap | 1 × 2048² (≈10 cm/texel for 200 m) authored in World Machine or Gaea. |
| Terrain texture splat | 8 surface types — short grass, long grass, mud, gravel, stone-flag, bog, heather, hard-packed earth. |
| Terrain decals | Wheel ruts, footpaths, ploughed-furrow patterns (~12 decals) |
| Edge cliffs / moor edges | Hand-sculpted in 3D, blended with terrain at the edges |

#### Placeholder strategy

| Stage | Placeholder |
|---|---|
| **S0** | Flat plane at y=0 (the prototype's current state). Single grey checkerboard material so the eye can read scale. |
| **S1** | Coarse heightmap from a quick World Machine session (~half a day). Two splat layers (grass + path). |
| **S2** | Final heightmap, 8-splat, decal-dressed. |

### 3.4 Skybox / atmosphere

The engine supports both procedural physical atmosphere (Rayleigh + Mie) and cubemap-based skies. *Devil's Playground* uses **procedural for clear/dawn**, **cubemap for mist/rain** (atmospheric haze is easier to art-direct in a painted cubemap).

| Asset | Notes |
|---|---|
| Clear-night procedural params | One Rayleigh / Mie tuning, moon parameters. |
| Mist cubemap | 1024² × 6 faces × RGBA16F. Hand-painted. |
| Rain cubemap | 1024² × 6 faces × RGBA16F. |
| Dawn cubemap | 1024² × 6 faces × RGBA16F. Final-cutscene only. |
| IBL diffuse cube | 32² × 6 × RGBA16F, baked from each. |
| IBL specular cube | 128² × 6 × 5 mips × RGBA16F, baked from each. |
| BRDF LUT | 512² × RGBA16F, shared. |

#### Placeholder strategy

| Stage | Placeholder |
|---|---|
| **S0** | Engine default procedural sky. |
| **S1** | One real night-time HDRI from PolyHaven (CC0). Bake the IBL chain from it. |
| **S2** | Final hand-painted cubemaps per weather. |

### 3.5 Vegetation (separate from set-dressing flora)

Procedural placement of grass and moss patches handled by the engine's vegetation system (per `Flux/Vegetation/CLAUDE.md`). Per the engine summary, this is a grass-specific system.

| Asset | Notes |
|---|---|
| Grass cluster meshes | 4 variants. Low-poly (8–20 tri). |
| Moss patch meshes | 3 variants. |
| Heather clump meshes | 4 variants (the moor edge needs this). |
| Vegetation textures | 1 atlas, 1024², for all clusters. |

**Placeholder:** the engine's default grass clutter (per `Flux/Vegetation`); zero work needed at S0.

### 3.6 Districts — environment per area

GDD §6.1 names six districts. Each has unique signature props on top of the shared kit.

| District | Unique props beyond kit |
|---|---|
| Standing Stones | 5 standing stones (hand-modelled), 1 cairn (variant × 2), 1 wind-tattered banner |
| Yards & Cottages | 1 dovecote, 1 well-cover, 1 wooden gate-stand, 1 laundry pole, 1 sheep pen, 1 chicken coop |
| Smithy & Mill | 1 forge body (the interactable — see §4), 1 hammer-stand, 1 anvil, 1 bellows, 1 millwheel (animated), 1 sluice gate, 1 grain hopper |
| Chapel & Graveyard | 1 chapel altar (the win-condition pentagram interactable — see §4), 1 lectern, 1 pew (instanced), 1 communion-rail, 4 hand-modelled gravestones (instanced w/ variants), 1 yew tree, 1 lych-gate |
| Longhouse | 1 longhouse hearth, 1 longhouse table (the largest table in the game, runs the length of the building), 1 long bench (instanced), 1 mead-cask rack, 1 trophy mount (boar's skull) |
| Moor Edge | 1 hanging tree, 1 hangman's noose (animated), 1 cairn, 1 weather-vane, 1 stone marker |

**Total district signatures:** ~30 unique props on top of the 100 generic dressing props.

---

## 4. Items, Interactables, Props

GDD §4.3 enumerates the gameplay items; GDD §4.4 the interactables.

### 4.1 Items

| Category | Items | Notes |
|---|---|---|
| Reagents | 14 (per GDD §6.3) | Each is a small tactile prop — caul, hare's tongue, three drops of bog-water (sealed phial), bell's soul, burial-coin, plus ~9 more |
| Tools | Iron, Wood, Lantern, Bell-Wax | Lantern is a held prop with emissive material |
| Keys | Brass Key, Skeleton Key | Two distinct mesh variants |
| Distractions | Wind-Up Music Box, Tin Whistle, Tinder-Bundle, Raw Meat (for hounds) | Each has its own affordance / silhouette |
| Charms | Rowan Sprig, St. George Medal, Salt Pouch | Held + dropped versions identical |

**Spec per item:**

| Field | Value |
|---|---|
| Tri count | 200–1500 (small props) |
| Texture | 256² or 512² base + normal + roughness; no metallic for organic items, with metallic for keys/medal |
| Materials | 1 per item |
| Held-attachment socket | Either right-hand or two-handed (defined in `dp_char_villager.zskel`); each item declares which |
| Drop position | At villager's feet socket |

#### Placeholder strategy — items

| Stage | Placeholder |
|---|---|
| **S0** | The current prototype: **a single tinted cube** per item-tag via `DPMaterials::GetOrCreateColouredVariant`. Already shipping. The prototype's RGB scheme (Iron=grey 0.5/0.5/0.55, Key=gold 1.0/0.85/0.2, SkeletonKey=purple 0.7/0.3/0.9, Objective=red 0.95/0.15/0.15) extends cleanly to 14 reagent hues. **This is the cheapest single placeholder in the game and should be preserved as a debug-mode option even in the shipping build.** |
| **S1** | Kenney.nl "Survival Kit" + "Medieval Kit" free models — low-poly, single-material, semantically correct. ~30 of the 25 items match directly. The Wind-Up Music Box and the Tin Whistle require custom blockouts (~1 day each). |
| **S2** | Hand-modelled, hand-textured, hand-iconned per item. Outsourceable per-item at ~$300/piece. |

### 4.2 Interactables

Mostly already enumerated in the prototype's `Components/DP*_Behaviour.h`.

| Interactable | Mesh count | Notes |
|---|---:|---|
| Door, single | 4 variants (wooden plank, iron-banded, cellar-hatch, longhouse — see §3.1 kit) | Pivot bone authored. Animator state machine drives 0→1 lerp. |
| Door, double | 2 variants (chapel main + longhouse main) | Two pivot bones (Leaf_L, Leaf_R per prototype's `DPDoubleDoor_Behaviour.h`). |
| Chest | 4 variants (wooden, ironbound, ornate, cellar) | Lid pivot is a child entity per `DPChest_Behaviour.h`. |
| Forge | 1 hand-authored variant | The visual centrepiece of the smithy district. With glow material driven by `Flux/Particles/`. |
| Pentagram | 1 (chapel only) | Floor decal-style mesh, hand-modelled with carved-stone normal-map detail. Glow material driven by win-progress mask (0/5 → 5/5). |
| Noise machine (period-correct: wind chime, hung pots, dovecote) | 3 variants | The prototype's "DummyNoiseMachine" gets period skinning. |
| Bell-rope (chapel) | 1 | Held-prop animation. |
| Hearth | 4 variants (cottage, longhouse, smithy, chapel) | Each carries a perpetual fire VFX. |
| Lantern (held by villager) | 1 | Held-attachment prop with point-light child. |
| Confessional | 1 | Chapel-interior only. |
| Well | 1 | Outdoor; the "drown" verb's anchor. |

**Total:** ~22 unique interactable meshes.

#### Placeholder strategy — interactables

| Stage | Placeholder |
|---|---|
| **S0** | The current scaled-cube approach is shippable. Each interactable is a colour-tagged box with the right approximate dimensions. The `DPInteractable_Behaviour` proximity range already works at this fidelity. |
| **S1** | Synty POLYGON Knights / Vikings include doors, chests, hearths, even a forge. Re-skin per kit-piece variant. |
| **S2** | Hand-authored. Outsourced ~$500/piece for final-detail interactables. |

---

## 5. VFX

Per GDD §8.3 ~25 distinct particle systems. The engine's `Flux/Particles/` supports config-driven CPU and GPU emitters. The prototype has one placeholder (PFX_Witch).

### 5.1 VFX inventory

| VFX | Purpose | Emitter | Notes |
|---|---|---|---|
| `dp_vfx_possession_enter` | Signature — silver particles spiral up from ground into the body, eyes flash white-blue. | CPU, 1-shot, ~80 particles | Plays on every possession. **The most important single VFX in the game.** |
| `dp_vfx_possession_exit_faint` | Voluntary switch — body slumps, demon-frost leaves as a cool wisp upward. | CPU, 1-shot, ~30 particles | Lighter than die. |
| `dp_vfx_possession_exit_die` | Burn-out — body collapses, frost spreads on ground (decal §6), vapour rises 8 m. | CPU, 1-shot, ~120 particles + decal | The single most-watched-by-Aelfric event. Must be visually loud. |
| `dp_vfx_frost_trail_anchor` | The 1.5-s untethered cooldown — a cold wisp follows the demon's anchor. | CPU, looping while active | Tied to GDD §4.1. |
| `dp_vfx_demon_scent` | Per-villager. Faint soot-coloured particle plume rising from a high-scent body. | CPU, looping while scent > 0.3 | Visual telegraph for the demon-scent mechanic. |
| `dp_vfx_lantern_dust` | Slow drifting motes inside Aelfric's lantern cone. | GPU, ambient | Gives the lantern presence; cheap. |
| `dp_vfx_hearth_smoke` | Vertical smoke column from each hearth. | GPU, ambient | One config; instanced per hearth. |
| `dp_vfx_forge_sparks` | Forge's burst when smith hammers / when crafting completes. | CPU, 1-shot per strike | Tied to forge interaction events. |
| `dp_vfx_torch_flame` | Torch flame. | GPU, ambient | One config; instanced 24× per Night. |
| `dp_vfx_candle_flame` | Smaller flame. | GPU, ambient | Instanced 30+× per Night. |
| `dp_vfx_chapel_candle_choir` | Chapel candle cluster (8 candles). | GPU, ambient | A "set" preset of candles. |
| `dp_vfx_bell_wave` | Audio-visual wave that emanates when chapel bell rings. | GPU, 1-shot | Ties to the gameplay-meaningful bell-ring event. |
| `dp_vfx_hound_breath` | Cold breath plume from each hound when they exhale (every ~3 s during idle, every breath during pursuit). | CPU, periodic | Reads the air as cold. |
| `dp_vfx_villager_breath` | Same, for possessed villagers (highlights them as the player's body). | CPU, periodic | |
| `dp_vfx_fog_wisp_ambient` | Drifting fog tendrils across the moor edge. | GPU, ambient | Sets the folk-horror atmosphere. |
| `dp_vfx_fog_wisp_dense` | Mist-weather variant; thicker, more frequent. | GPU, ambient | Triggered by weather state. |
| `dp_vfx_rain_drops` | Rain particle layer. | GPU, ambient | Rain-weather only. |
| `dp_vfx_blood_splatter` | When Aelfric apprehends (or fires his flintlock and hits). | CPU, 1-shot | Used ~3 times per Night. |
| `dp_vfx_pentagram_glow` | Increasing intensity as reagents are inscribed (0/5 → 5/5). | GPU, ambient + ramped | 5 intensity states. |
| `dp_vfx_pentagram_ignite` | Win — pentagram detonates upward. | GPU + CPU, 1-shot | Climactic. The end of a successful run. |
| `dp_vfx_tinder_fire` | Distraction-prop fire. | CPU + GPU mix | 30-s lifetime. |
| `dp_vfx_music_box_chime` | Audio-visual chime ring. | CPU, periodic | 1 ring/s for 8 s. |
| `dp_vfx_bog_water_evaporate` | The reagent evaporating after drop. | CPU, periodic during 8-s decay | Telegraph the timer mechanic. |
| `dp_vfx_witch_summon` | Cutscene-only — the Order summoning the demon. | CPU + GPU mix | Cinematic. |
| `dp_vfx_dawn_burn` | Loss-by-dawn — sun breaks, frost evaporates from anchor. | GPU, 1-shot | Punctuates a loss. |

**Total: 25 active configs + ~7 ambient/instanced reuses = ~32.**

### 5.2 Placeholder strategy — VFX

| Stage | Placeholder |
|---|---|
| **S0** | The existing PFX_Witch placeholder generalises. Author 4–6 generic emitter configs (sparkle_warm, sparkle_cool, smoke_grey, smoke_white, flame_orange, fog_drift) using engine defaults. Re-use them for ALL VFX positions on the map. Visually wrong but mechanically present — the game *plays* with VFX hooks already firing. |
| **S1** | A junior VFX artist authors final-shape placeholders for the 25-30 unique configs. Shape and colour correct; particle textures stock (free, e.g. Unity's free particle pack imported as PNGs). |
| **S2** | Final particle textures + final tuning. The Possession-Enter signature gets a 3-week iteration loop with the design lead. |

---

## 6. Lighting & Decals

### 6.1 Hand-placed lights

| Light | Count per Night | Notes |
|---|---:|---|
| Torch (point, 1000 lm, warm 1.0/0.906/0.569 — matches prototype's `DPLevelData.h`) | 24 | Already in prototype's level data. |
| Candle cluster (point, 200 lm) | 30 chapel + 8 longhouse + ~10 cottages = ~48 | |
| Lantern (point with attached sphere collider — actually moves with bodies) | 8 (Aelfric's lantern + 4 villager-held + 3 dropped) | Per-Night variation. |
| Hearth (point, 800 lm, flickering 0.85–1.0) | 4 (one per major hearth-bearing building) | Flicker handled by an artist-authored animation curve. |
| Window-spill (spot, 400 lm, cool white inside contrast) | 8 | Subtle interior glow telegraphing inhabited buildings. |
| Directional (moon, 10 lux, cool 0.235/0.216/0.404 — matches prototype) | 1 | Whole map. |
| Ambient fill / corner-tuck (point, 50 lm) | ~36 | Necessary to keep the top-down camera readable. |

#### Placeholder strategy — lighting

| Stage | Placeholder |
|---|---|
| **S0** | Prototype's existing 24 torches + 1 directional. Game is too dark; intentional placeholder. |
| **S1** | A lighting artist (could be the env-art lead wearing a hat) places the full count using grey-box VFX. Tune per-district. |
| **S2** | Full lighting pass with bounce, IBL contributions, per-Night weather adjustments. Final-quality light cookies authored for windows / lantern cones. |

### 6.2 Decals

The engine's decal system (`Flux/Decals/`) supports box decals. Recent "Decal bullet holes" commit confirms it's production-ready.

| Decal | Count | Purpose |
|---|---:|---|
| Frost ground (1 m²) | 1 mesh, instanced | Marks the demon's last-death anchor. Visible to player only. |
| Footprint (single) | 6 variants (clean, mud, blood) | Shows villager / Aelfric paths. |
| Wheel rut | 2 variants | Terrain dressing. |
| Blood splatter | 4 variants | Aelfric kills. |
| Pentagram inscription progress (5 stages) | 5 | Visible inscriptions of each inscribed reagent on the pentagram floor. |
| Cracks (cobblestone, wall, plaster) | 6 | Set dressing. |

**Total decals:** ~24.

#### Placeholder strategy

| Stage | Placeholder |
|---|---|
| **S0** | A single procedural box-decal config (matches the existing scorched bullet-hole proxy in `Flux/Decals/`) re-used in different colours per use. |
| **S1** | Junior 2D artist authors one PBR decal per kind. |
| **S2** | Final, with per-decal normal+roughness for crevice / surface interaction. |

---

## 7. UI / 2D

GDD §7.1 specifies the HUD; §7.2 the menus.

### 7.1 HUD elements

| Element | Type | Notes |
|---|---|---|
| Sun-gauge | Sprite + dynamic fill (`Zenith_UIRect::SetFillAmount`) | Top-left. Custom curve background. |
| Reagent slot (×5) | Sprite frame + sealed-state sprite + wax-seal stamp | Top-right cluster. |
| Life-candle | Sprite frame + dynamic flame VFX (in UI canvas — engine supports?) + dynamic burn-down | Bottom-left. Replaces the prototype's text bar. |
| Body-name | `Zenith_UIText` | Handwritten-script font. |
| Demon-scent indicator | Sprite overlay on life-candle | Soot stain that grows. |
| Held item glyph | `Zenith_UIImage` | Bottom-centre. Per-item icon. |
| Held item name | `Zenith_UIText` | |
| Aelfric awareness icon | Sprite (5 states: calm / suspicious / pursuing / apprehending / lost) | Bottom-right. |
| Whisper line | `Zenith_UIText` | Italic, bottom-centre. Configurable visibility. |

### 7.2 Menus

| Menu | Elements | Count |
|---|---|---:|
| Main menu | Background painting + 3 buttons + version stamp | 6 |
| Liminal hub | Background painting + 3 hermit shrines + Knot counter + unlock-tree per hermit (~12 nodes/hermit) | ~45 |
| Pause | Translucent overlay + 4 buttons | 5 |
| Settings (graphics / audio / controls / accessibility) | ~80 elements (sliders, toggles, dropdowns) | ~80 |
| Night-start screen | Procedural seed display + Aelfric-variant card + weather card + reagent silhouettes | ~14 |
| Daily Vexholme | Seed display + leaderboard | ~12 |
| Run-results screen | Knots earned + breakdown + ending text + back-button | ~15 |
| Cutscene letterbox + skip prompt | 2 | 2 |

### 7.3 Icons

| Icon set | Count |
|---|---:|
| Item icons | 25 (14 reagents + 8 tools/keys + 3 distractions) |
| Archetype portraits (for whisper-line + selection telegraph) | 24 |
| Aelfric awareness states | 5 |
| Hermit portraits | 3 |
| Achievement icons | ~20 |
| Settings-screen icons | ~25 |

**Total UI sprite count:** ~280 elements (after deduplication).

### 7.4 Fonts

| Font | Use | Style |
|---|---|---|
| Display (large headings, cutscene cards) | Title screen, Night labels | Hand-drawn period-style serif. Licensed. |
| Body (HUD, menus, subtitles) | Everything readable | Open-source humanist serif (e.g. Source Serif Pro). |
| Diagnostic | Whisper line, debug overlay | Open-source monospace (e.g. JetBrains Mono). |

**License note:** display font must be either custom-licensed or commissioned. Open-source body+mono fonts cover EU + most of Asia at no cost.

### 7.5 Placeholder strategy — UI

| Stage | Placeholder |
|---|---|
| **S0** | The current prototype's text-only HUD (per `DPHUDController_Behaviour.h`). Already shippable for internal builds. ASCII life-bar, named buttons, no painted frames. |
| **S1** | Wireframe / monochrome UI: every element has a final-shape sprite but is rendered in a single colour (white-on-black). The HUD reads correctly, just not pretty. Authored by an in-house UI generalist over ~6 weeks. |
| **S2** | Final painted UI. Outsourced or in-house lead. Includes per-state animations (hover, press, transition). |

---

## 8. Audio (engine work required first)

**⚠️ The Zenith engine has no audio system.** This is the single largest *engine* gap blocking shipping audio. The system is net-new work, ~3 engineering months (audio thread, mixer, 3D positional audio, streaming, format support) before any artist deliverable can integrate.

### 8.1 Audio system requirements (engineering scope, not asset scope)

- File format: Vorbis (.ogg) for music; uncompressed PCM (.wav) for short SFX; or a single in-engine `.zaudio` wrapper.
- 3D positional audio with distance attenuation curves matched to perception-system loudness/radius parameters (so that what Aelfric "hears" matches what the player hears).
- Buses: master, music, ambient, SFX, VO. Per-bus volume.
- Footstep system: per-material surface footstep events firing on bone-event in the walk/jog/sprint anims.
- Music state-machine: 5 states (calm / suspicious / pursue / apprehend / climax), cross-fading 1.5-s transitions.

### 8.2 Audio asset inventory

#### 8.2.1 Music

| Track | Length | Notes |
|---|---|---|
| Title screen | 3:00 loop | Pärt-adjacent choral drone. |
| Night ambient bed (per Night × 7) | 5:00 each, looping | Each Night escalates from prior. Night 7 is the heaviest. |
| Gameplay dynamic — calm | 4:00 loop | Underbed during exploration. |
| Gameplay dynamic — suspicious | 2:00 loop | Triggered when Aelfric is suspicious. Cross-fades. |
| Gameplay dynamic — pursuit | 1:30 loop | Triggered when Aelfric pursues. Higher tempo. |
| Liminal hub | 2:30 loop | Otherworldly. |
| Cutscene scores (3) | 1:00 each | Variable. |

**Total music duration:** ~45 minutes of distinct material, looped during play. Outsourced to a composer over ~4 months.

#### 8.2.2 SFX bible (~480 SFX)

| Category | Count | Notes |
|---|---:|---|
| Footsteps × 8 surfaces × 4 villager weights × 4 paces (walk/jog/sprint/sneak) | 128 | The single largest sub-category. Sourceable; doesn't need bespoke recording. |
| Doors | 12 (open/close × 4 variants × 3 distances) | |
| Chests | 4 | |
| Forge | 6 (hammer single, hammer loop, bellows, crafting complete, idle hum, smithy ambient bed) | |
| Bell | 3 (single tone, ringing loop, distant) | The chapel bell is the loudest single sound; sample a real bell on field-recording day. |
| Forge / hearth fires | 6 ambient | |
| Wind | 8 ambient (per-district × per-weather) | |
| Rain | 6 ambient | |
| Distraction props | 12 (music box, tin whistle, tinder) | The music box plays a real period-correct tune (commission, ~$500). |
| Possession transition | 8 (enter, exit-faint, exit-die, frost-trail loop, etc.) | The signature audio of the game. |
| Aelfric VO mutters | ~20 lines × 3 takes each = 60 takes | See §8.2.3. |
| Hounds | 16 (bark variants, pant, sniff, whine, eat, etc.) | |
| Villager idle vocalisations | 24 (one set per archetype — a sigh, a cough, a whistle) | Adds life without dialogue. |
| Cloth + leather + chain | 18 | Per-archetype rustle. |
| UI clicks / hovers | 12 | |
| Misc combat (Aelfric flintlock fire, reload, etc.) | 8 | |
| Misc world (cart wheels, well chain, bell rope) | 14 | |
| Cutscene SFX | ~120 (a busy category; one-shot per cutscene moment) | |

**Total SFX:** ~480.

#### 8.2.3 VO

| Speaker | Lines |
|---|---:|
| Aelfric | 20 |
| Cabal hermits (Wynstan, Mereworth, Old Bett — interstitial Liminal dialogue) | 5 each = 15 |
| Joan Trew | 4 (Night 5 cutscene) |
| Demon Latin chorale (whispers, not dialogue) | 6 stems |
| Choral background (used as music + ambient hybrid) | 8 stems |

**Total VO:** ~53 recorded takes (each in 3 alternate readings = ~160 individual recordings).

### 8.3 Placeholder strategy — audio

| Stage | Placeholder |
|---|---|
| **S0** | The engine has no audio. Build a minimal `Zenith_AudioBus` with `EmittedSounds` recording (already required by [TestPlan.md](TestPlan.md) §0.4) — no actual playback, just an event-bus pattern. Designers and engineers see audio fire in logs and tests pass. The game is silent on the speaker. |
| **S0.5** | Add a single tone-generator playback (a 440 Hz sine for each emission). Awful to play but proves the spatial-audio chain works. |
| **S1** | Source temp SFX from freesound.org (CC0 / CC-BY). Temp music from FreeMusicArchive (CC-BY, attribute). Aelfric VO: text-to-speech (engine-side hookup; Microsoft Azure TTS has a usable voice). The game is fully audible in S1; the audio is just temp. |
| **S2** | Final composer score. Real VO recording session. Curated SFX library + bespoke field recordings (the bell, the millrace, sheep, a real forge). Final mix by an external mix engineer over ~6 weeks. |

---

## 9. Cinematics

GDD §2.3 — 14 cutscenes (2 per Night, win + lose).

### 9.1 Cinematic deliverables

| Asset | Count | Notes |
|---|---:|---|
| Cutscene storyboards | 14 × ~10 panels = ~140 | Concept-art deliverable; see §1. |
| Cutscene animations | 14 × ~30 s avg = ~7 min cinematic | Pre-rendered or in-engine cinematic. *Recommend in-engine* — cheaper, scales with character art quality. |
| Cutscene-only character meshes (Joan in chains, ceremony robes for cabal) | 6 | Subset of §2; just additional outfits. |
| Cutscene-only props | ~25 | Subset of §3.2. |
| Cutscene VFX | 14 unique cinematic VFX bursts (overlapping §5 inventory) | |
| Cutscene SFX | ~120 (per §8.2.2) | |
| Cutscene VO | ~30 lines | Per §8.2.3. |

### 9.2 Placeholder strategy — cinematics

| Stage | Placeholder |
|---|---|
| **S0** | A single black screen with white text per cutscene, holding 8 s, advancing on Esc. Plain narrative. Hookable into the engine in 1 engineering day. **All gameplay tests can run with this in place** (the cutscene's effect on game state is what matters, not its presentation). |
| **S1** | Static storyboard images, one per panel, shown in sequence with text below. 10–15 s per cutscene. Captures the dramatic intent. |
| **S2** | Final in-engine animated cinematic. Authored by a cinematics director with a small team. **Months 14–18.** |

---

## 10. Animation Pipeline (cross-cutting)

The engine supports skeletal animation with state machines, IK, and animation layers (per `Flux/MeshAnimation/`).

### 10.1 Pipeline

1. Animator works in Maya / Blender / 3ds Max, exports FBX.
2. `Tools/Zenith_Tools_MeshExport` converts FBX → `.zmesh` + `.zskel` + per-anim `.zanim`.
3. Animator authors the **animation state machine** for each character via a state-machine asset format. (Editor support TBD — currently must be authored in code per the `Flux_AnimationStateMachine` API.) **Recommendation for shipping: invest in a visual state-machine editor (Phase 2 engine work, ~6 weeks).**
4. IK is applied as a runtime layer for head-look and foot-IK (already used on the RenderTest player per recent git history).

### 10.2 Animation events

Animation events are the hook by which footstep SFX, attack frames, particle spawns, and other gameplay-relevant moments fire from inside an anim clip. The engine's anim system doesn't currently have a documented event mechanism — **engineering scope: add anim events**, ~2 engineering weeks. Bound to Phase 1 audio engine work since footstep SFX is the canonical use case.

---

## 11. Marketing / external assets (out of scope but flagged)

These don't ship inside the game but are required for launch.

| Asset | Count | Notes |
|---|---:|---|
| Steam capsule (header / library / small) | ~6 variants | Outsourced. ~$3k. |
| Console store assets (cert-spec) | ~25 per platform × 3 platforms | Per first-party spec sheets. |
| Trailer (~90 s announce + ~60 s release + ~30 s gameplay loop) | 3 trailers | Outsourced. ~$50k–$150k. |
| Press kit | 12 screenshots, 4 GIFs, fact sheet | In-house. |
| Soundtrack album art | 1 | Outsourced. ~$1.5k. |

---

## 12. Roll-out schedule

How asset stages map to the 18-month dev runway (matches GDD §12 phasing).

| Month | Stage in-flight | What ships in builds |
|---|---|---|
| 0 | Concept-art kickoff. S0 placeholders sourced. | Capsule colliders + tinted cubes (existing prototype + extensions). |
| 1–2 | Mixamo placeholders integrated. S0 buildings kit-bashed from UE export. Audio system engine work begins. | Recognisable characters and buildings, no audio. |
| 3–4 | District mood paintings completed. S1 props pipeline standing up. Composer kickoff. | First wireframe UI; temp music starting; placeholder VO via TTS. |
| 5–8 | S1 props + S1 characters in. S0 → S1 swap for Aelfric. SFX-source pass. | "Animatic-quality" game. Internally demoable. |
| 9–10 | S2 begins on hero assets: Aelfric, Joan, demon-effects, pentagram. | Vertical-slice quality on hero assets; S1 everywhere else. |
| 11–13 | S2 sweep through villager archetypes (~3/week). Final UI pass. Audio session bookings. | "Looks like a real game" milestone. |
| 14–16 | S2 environment outsource delivery. Final lighting pass. Final SFX + VO + music. | Beta-quality. |
| 17–18 | Polish. LQA. Cert. Day-1 patch art. | Ship-quality. |

---

## 13. Ownership matrix

Who owns what across the team.

| Asset class | Owner | Notes |
|---|---|---|
| Concept art | Concept Lead (1 FTE month 0+) | |
| Characters | Character Lead (1 FTE month 2+) + 1 outsource shop | |
| Environment | Env Lead (1 FTE month 1+) + 1 generalist + 1 outsource shop | |
| VFX | Tech Art Lead (1 FTE month 4+) | |
| UI | UI Generalist (0.5 FTE month 3+) → UI Lead (1 FTE month 8+) | |
| Lighting | Env Lead doubles as lighting lead | Specialist hire month 12 if budget. |
| Audio | Audio Director (1 FTE month 2+) + composer + mix engineer + VO director on contracts | Engine team owns audio system itself. |
| Cinematics | Director + Storyboard artist + animator | Months 14–18 heavy. |

**Total art / audio headcount:** 5 FTE at peak + 4 contractor relationships. (Combines with the 12-person engineering team from GDD §12 for a ~17-headcount production at peak.)

---

## 14. Anti-patterns

Lessons learned across the industry; *do not* fall into these.

1. **Skipping the placeholder layer.** Teams that "just wait" for final art block engineering for months. The S0 layer must be ready by week 2.
2. **Authoring packed PBR textures.** The Zenith pipeline does *not* support ARM/ORM packs; channels are separate textures. Train artists on this on day one.
3. **Above-budget poly counts.** Top-down camera at 80 m zoom does *not* benefit from 4K textures or 50k-tri characters. Police this in import.
4. **Inconsistent silhouettes across archetypes.** The single biggest crowd-legibility failure mode at 80 m view. Concept-art sheets must include a silhouette swatch.
5. **Aelfric without iteration time.** He is the antagonist; he gets two full passes. Schedule them.
6. **Volumetric fog at full settings on Switch.** The engine offers 4 fog techniques; Switch may need the cheap "simple exponential" mode. Budget a per-platform fog quality switch.
7. **Audio system as a Phase 4 task.** It must land Phase 1 — every level of gameplay tuning depends on it.

---

## 15. Summary

The current prototype's tinted-cubes + capsule-colliders approach is **the cheapest, most coherent placeholder strategy a game at this stage could have**. The `DPMaterials::GetOrCreateColouredVariant` system encodes semantic identity into visual identity; preserve it as a debug mode forever. From here, the asset roadmap is:

1. **Month 0–1:** Source Mixamo + Synty + Kenney packs. The game looks like a "Polygon Pack stealth game" — workable.
2. **Month 2–10:** In-house S1 pass. The game looks like a coherent stylised prototype.
3. **Month 10–18:** S2 pass with heavy outsourcing. The game looks like the shipping product.

The biggest art-side risk is **Aelfric**: he is on screen for ~80% of every run and is the antagonist. Schedule two full character-art passes and one VO session re-do (~30 days of audio booth time across the campaign).

The biggest engine-side risk is **audio**: the system does not exist. Budget Phase 1 engineering for the audio system before any audio artist deliverable can integrate.

The cumulative asset count at ship is **~2,400 individual files** across meshes, textures, animations, particles, decals, UI, fonts, music, SFX, VO, and decals. Of those, the prototype contains roughly 60 (~2.5%). The shipping bar is reachable in 18 months with the schedule described in §12.
