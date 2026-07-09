# Zenithmon -- Game Design Document

**Working title:** Zenithmon
**Status:** v1.0 -- S0 reference GDD; the creative content (region, names, dex)
is authored here and becomes binding as each stage implements it
**Author:** Design (Claude), from the user-approved plan
(`zenithmon-pok-mon-nested-puddle.md`, locked 2026-07-08)
**Last updated:** 2026-07-09 (S0)
**Scope authority:** [Scope.md](Scope.md) is the binding in/out gate. This
document details the IN column; it cannot re-add anything Scope cuts.
**Update policy:** updated per phase (stage gates S1, S8, S9/S10, S11). Names
and numbers here are the source of truth for `Source/Data/*.cpp` tables; when
code and GDD disagree after S1, the code tables win and this doc gets a fix-up
in the same PR.

---

## 1. Vision

### 1.1 Elevator pitch

**Zenithmon** is a feature-complete, mainline-class monster-collecting RPG
built entirely on the Zenith engine: catch and train ~150 original species
across a mountain region of eleven settlements and fifteen routes, defeat
eight gym leaders, climb Victory Road, and take the Champion's seat -- then
prove it in the post-game Battle Tower. Every mesh, texture, animation and
terrain in the game is procedurally generated and baked to disk by tools
builds. Mainline MECHANICS are the reference; every name, creature, and place
is original (zero Nintendo IP). No audio, no networking, singles battles only.

### 1.2 Pillars

Every feature is tested against these four principles. If it serves none of
them, it gets cut.

1. **Faithful mainline mechanics.** The battle core is the real thing --
   Gen-III+ stat math with natures/IVs/EVs, Gen-V damage, 4-shake catches,
   priority-then-speed turn order, abilities, weather, breeding. Players who
   know the genre should feel zero friction; the depth is genuine, not
   approximated.
2. **Fully procedural asset pipeline.** No hand-made art. Creatures, humans,
   buildings, props, and terrain are all generated deterministically from
   seeds and baked by ZENITH_TOOLS builds. The generators ARE the art
   direction; a species' look is a recipe, and re-baking is byte-identical.
3. **Deterministic, testable systems.** The battle engine is a seeded,
   headless C++ state machine emitting an append-only event stream -- the
   same stream drives unit tests and on-screen presentation. Game data is
   compiled const tables. Everything rule-based runs (and is gated) headless
   in CI; a scripted bot can play the game from new-game to Champion.
4. **Classic 8-gym structure.** Home village, professor, starter, badges,
   Victory Road, League, post-game. A linear, legible, complete arc -- no
   open-world drift, no gimmick layer, no filler org-villain subplot. The
   rival and the League ARE the story.

---

## 2. The Region: Culmina

### 2.1 Overview

**Culmina** is a wedge of land that rises, without ever quite levelling off,
from a warm southern coast to a single great summit -- **Mount Culmen** --
whose peak stone is said to sit directly beneath the sun at noon. Every road
in Culmina climbs. The journey begins in **Dawnmere Village**, a sea-level
meadow settlement where Professor Aster keeps her lab, and proceeds up
through farm country and kiln-warmed volcanic foothills, down briefly to the
western harbor and the canal flats of the industrial belt, then over the
central crossroads plain and into the high country: wind-scoured cliff towns,
fog-drowned moorland, the snowline, and the leeward canyon badlands. Above it
all, reached only through the switchback gauntlet of **Highcrown Pass** (the
region's Victory Road), sits **Zenith Gate** -- the League's seat at the
mountain's crown. The region's shape is the game's shape: you start at dawn,
at the bottom, and you finish at the zenith.

### 2.2 World table -- all scenes and build indices

Build indices are locked by the plan: 0 FrontEnd, 1 Battle, 2-12
towns/cities, 20-34 routes + Victory Road, 40+ interiors, 95 Battle Tower.
`ZM_WorldSpec` (S1) encodes this table; its integrity tests enforce that
every connection, spawn tag, gym, and encounter row below resolves.

#### Core scenes

| Index | Scene | Notes |
|---|---|---|
| 0 | FrontEnd | Title / continue / new game. Boot scene. |
| 1 | Battle | The one battle arena, at world offset (0, -2000, 0); per-biome backdrop dressing swapped at runtime from ~6 baked prop sets. Loaded ADDITIVE over a paused overworld scene. |

#### Towns and cities (indices 2-12)

| Index | Town | Identity | Gym / resident |
|---|---|---|---|
| 2 | Dawnmere Village | Sea-level meadow hamlet; the player's home and Professor Aster's lab; where every journey starts | No gym. Professor Aster, Mom (Maren), rival Vesper |
| 3 | Thornacre Town | Hedgerow farming town ringed by berry fields and drystone walls | Gym 1 -- Fenna (Grass) |
| 4 | Cinderfell Town | Kiln-and-pottery town on warm volcanic foothills; chimneys always smoking | Gym 2 -- Bram (Fire) |
| 5 | Tidegate City | The western harbor; sea-locks, fish markets, lighthouse pier | Gym 3 -- Maris (Water) |
| 6 | Gearspring City | Canal-fed industrial city of dynamos, gantries and powerline yards | Gym 4 -- Tessa (Electric) |
| 7 | Milldown Crossing | Crossroads market town on the central plain; home of the region's daycare | No gym. Daycare couple, move tutor |
| 8 | Skyshear Town | Cliff-edge town of rope bridges and wind turbines; everything is tied down | Gym 5 -- Aquilo (Sky) |
| 9 | Umbermoor Town | Peat-dark moorland town, half-sunk in fog; lanterns burn all day | Gym 6 -- Morwenna (Phantom) |
| 10 | Frostvale City | Alpine city above the snowline; carved-ice architecture | Gym 7 -- Halvard (Ice) |
| 11 | Stonereach City | Canyon city built into leeward badlands cliff faces | Gym 8 -- Vardis (Drake) |
| 12 | Zenith Gate | The League's summit citadel at Mount Culmen's crown; Care Center, League Halls, Battle Tower | No gym. Elite Four + Champion Elara |

#### Routes and Victory Road (indices 20-34)

Each route lists its biome and the character of its encounter fields (the
tall-grass patches that drive wild encounters; see `ZM_TallGrassSystem`).

| Index | Route | Connects | Biome / encounter-field character |
|---|---|---|---|
| 20 | Route 1 | Dawnmere - Thornacre | Coastal meadow; broad soft grass either side of a dirt lane -- the gentle tutorial fields |
| 21 | Route 2 | Thornacre - Cinderfell | Hedgerow scrub climbing into foothills; grass pockets between drystone walls and berry rows |
| 22 | Route 3 | Cinderfell - riverlands | Warm ash-soil descent along a river gorge; sparse singed grass, better pickings near the water |
| 23 | Route 4 | riverlands - Tidegate | Dune coast; marram-grass banks between tidepools, encounters lean aquatic near the surf |
| 24 | Route 5 | Tidegate - Gearspring | Canal towpath through wetland flats; dense reed fields, slow going off the path |
| 25 | Route 6 | Gearspring - Milldown | Powerline scrap-yards and substation fields; crackling grass strips between pylons, Electric-heavy |
| 26 | Route 7 | Milldown loop | Gentle farmland ring around the crossroads; long safe grass -- the egg-hatching circuit |
| 27 | Route 8 | Milldown - Skyshear | Cliff switchbacks; narrow wind-flattened grass shelves with long drops between them |
| 28 | Route 9 | Skyshear - Umbermoor | High moor under permanent fog; heather fields where visibility (and encounter rate) closes in |
| 29 | Route 10 | Umbermoor bog ring | Optional bogland loop; sucking marsh grass and will-o-wisp lights, rare Phantom spawns at the center |
| 30 | Route 11 | Umbermoor - Frostvale | Snowline ascent; grass gives way to powder-snow fields that serve the same encounter role |
| 31 | Route 12 | Frostvale glacier shelf | Optional glacier side-route; blue-ice fields and a frozen cave mouth, Ice rares |
| 32 | Route 13 | Frostvale - Stonereach | Wind-scoured saddle pass; hardy tussock clumps between bare crags |
| 33 | Route 14 | Stonereach - Highcrown | Canyon badlands approach; dry red-grass benches on the canyon floor, Earth/Stone country |
| 34 | Highcrown Pass | Stonereach/Route 14 - Zenith Gate | Victory Road: a cave-and-summit gauntlet of tunnels, ledges and the final open climb; the region's strongest wilds |

#### Interiors (indices 40+)

| Index | Interior | Where |
|---|---|---|
| 40 | Player home | Dawnmere Village |
| 41 | Aster's Lab | Dawnmere Village |
| 42 | Gym 1 interior (hedge-maze) | Thornacre Town |
| 43 | Gym 2 interior (kiln hall) | Cinderfell Town |
| 44 | Gym 3 interior (sea-lock platforms) | Tidegate City |
| 45 | Gym 4 interior (switchgear puzzle) | Gearspring City |
| 46 | Gym 5 interior (rope-bridge crossing) | Skyshear Town |
| 47 | Gym 6 interior (lantern dark-walk) | Umbermoor Town |
| 48 | Gym 7 interior (sliding ice floor) | Frostvale City |
| 49 | Gym 8 interior (canyon ascent) | Stonereach City |
| 50 | Daycare interior | Milldown Crossing |
| 51 | League Halls (four Elite rooms + Champion chamber, one scene) | Zenith Gate |
| 52 | Care Center (shared template, instanced per town) | All towns |
| 53 | Trade Post shop (shared template, instanced per town) | All towns |
| 95 | Battle Tower | Zenith Gate (post-game) |

Scene count: 2 core + 11 towns + 15 routes/Victory Road + 14 interiors +
1 tower = **43 scenes** (the plan's "~40").

---

## 3. Characters

### 3.1 Professor Aster

The region's monster professor, based in Dawnmere. Aster studies
**ascension** -- why Culmina's species change form as trainers carry them up
the mountain, and whether evolution and altitude are two faces of the same
pressure. She is brisk, kind, and permanently mid-fieldwork; she hands the
player and Vesper their starters not as a ceremony but as a research
assignment ("climb, and write down what changes"). She reappears at three or
four story beats (Tidegate, Umbermoor, Zenith Gate) to check the dex and nudge
the theme: the region rewards those who keep climbing. She never battles.

### 3.2 Mom (Maren)

The player's mother, a retired League trainer who stopped one badge short and
never talks about it -- until the post-game, when she finally does. She runs
the household in Dawnmere, heals the party when the player visits, and sends
the occasional supportive (and quietly badge-count-aware) letter to Care
Centers along the way. Her arc is small and warm: her final letter arrives
after the Champion battle, and the post-game opens with the player finding
her old trainer card on the mantel at home.

### 3.3 Rival: Vesper

Aster's other new trainee and the player's neighbor -- named for the evening
star, the counterpart to the player's climb toward noon. Vesper picks the
starter strong against the player's and sets off first, loudly certain the
badge run is a race. The arc is a classic rivalry with no villainy in it:
early battles are brash and sloppy (Route 1, Tidegate docks); the middle ones
show real craft and a first hard loss that stings (Milldown Crossing, the
Route 11 snowline); by Highcrown Pass Vesper battles like a leader and admits
the race stopped mattering somewhere around the moor. The final battle at
Zenith Gate's steps -- just before the League doors -- is fought as equals and
friends. Post-game, Vesper holds a standing rematch slot in the Battle Tower
lobby. Six battles total; teams anchor on the counter-starter family.

### 3.4 Gym leaders

| # | Leader | Type | Town | Badge | Teach-move reward |
|---|---|---|---|---|---|
| 1 | Fenna | Grass | Thornacre Town | Bloom Badge | Verdant Lash |
| 2 | Bram | Fire | Cinderfell Town | Kiln Badge | Cinder Rush |
| 3 | Maris | Water | Tidegate City | Tide Badge | Riptide |
| 4 | Tessa | Electric | Gearspring City | Coil Badge | Arc Surge |
| 5 | Aquilo | Sky | Skyshear Town | Gale Badge | Shearwind |
| 6 | Morwenna | Phantom | Umbermoor Town | Wisp Badge | Grave Chill |
| 7 | Halvard | Ice | Frostvale City | Rime Badge | Frostbind |
| 8 | Vardis | Drake | Stonereach City | Crest Badge | Wyrmsurge |

One-line identities:

- **Fenna** -- Thornacre's youngest-ever leader; grows her own gym maze and
  treats every challenger like a seedling to be tested, not trampled.
- **Bram** -- a master potter who fires his creatures and his clay in the same
  kilns; slow to anger, impossible to rattle.
- **Maris** -- Tidegate's harbormaster; runs her gym like a sea-lock, raising
  and lowering platforms mid-battle.
- **Tessa** -- Gearspring's chief engineer; rebuilt her gym's switchgear
  puzzle herself and thinks type charts are just circuit diagrams.
- **Aquilo** -- a glider pilot who commutes to his own gym by wind; battles
  fast, talks faster.
- **Morwenna** -- keeper of Umbermoor's lantern rows; soft-spoken, funny, and
  entirely at home among things that aren't there.
- **Halvard** -- Frostvale's mountain-rescue captain; patient, enormous, and
  fond of letting challengers slip on his ice floor before saying a word.
- **Vardis** -- warden of the Stonereach canyons and the League's gatekeeper
  in spirit; fights with the region's old drakes and expects to be climbed
  like the mountain itself.

### 3.5 Elite Four and Champion

| Seat | Name | Type identity |
|---|---|---|
| Elite 1 | Cassia | Venom -- a perfumer whose garden kills politely |
| Elite 2 | Torben | Brawl -- Highcrown's old trail-warden, all footwork and patience |
| Elite 3 | Lumen | Mind -- the League's archivist; claims to have read every battle before it happens |
| Elite 4 | Sable | Umbral -- night-watch of Zenith Gate; the last shadow before the crown |
| Champion | Elara | Balanced team crowned by a Drake/Sky ace (Zenithrax) -- the sitting Champion, an astronomer who took the seat "for the view" and kept it for a decade |

---

## 4. Starters

Three 3-stage families, gift-only (RARE tier), handed out in Aster's lab.
Vesper takes the family strong against the player's pick. The classic
grass/fire/water triangle maps to Grass/Fire/Water; each family gains a
second type at its final stage.

| Family | Stages | Archetype | Types | Identity |
|---|---|---|---|---|
| F01 | Fernfawn -> Thicketbuck -> Sylvastag | QUADRUPED | Grass -> Grass -> Grass/Earth | A meadow fawn whose antlers grow into a living grove; the final stage carries a small ecosystem on its head |
| F02 | Kindlet -> Scorchel -> Pyroclast | BIPED | Fire -> Fire -> Fire/Stone | A kiln salamander of soft clay that fires itself harder at each stage, ending as walking volcanic glass |
| F03 | Finlet -> Marlance -> Tidesabre | AQUATIC | Water -> Water -> Water/Iron | A darting billfish whose rostrum tempers into a fencing blade; the final stage duels with tide-current footwork |

---

## 5. The Dex

**56 families, 152 species** (the plan's "~50 families / ~150 species").
Rarity tiers drive encounter-slot weights (COMMON/UNCOMMON/RARE) and gate the
post-game (LEGENDARY). Archetype names the `ZM_CreatureGen` builder used for
the family (all stages of a family share archetype + family seed; each stage
adds one elaboration tier). Starters (F01-F03) are listed in section 4 and
counted here.

| Fam | Species (stage 1 -> 2 -> 3) | Archetype | Type(s) | Rarity | Identity |
|---|---|---|---|---|---|
| F01 | Fernfawn -> Thicketbuck -> Sylvastag | QUADRUPED | Grass, final +Earth | RARE | Starter: grove-antlered deer (sec. 4) |
| F02 | Kindlet -> Scorchel -> Pyroclast | BIPED | Fire, final +Stone | RARE | Starter: self-firing kiln salamander (sec. 4) |
| F03 | Finlet -> Marlance -> Tidesabre | AQUATIC | Water, final +Iron | RARE | Starter: blade-billed duelist fish (sec. 4) |
| F04 | Pipwit -> Trillark -> Stratavis | AVIAN | Normal/Sky | COMMON | The everywhere-songbird; the final stage sings above the clouds |
| F05 | Nibbin -> Hoardel -> Grainmaw | QUADRUPED | Normal | COMMON | Field vole line that hoards seeds in ever-larger cheek vaults |
| F06 | Wrigglet -> Cradlewrap -> Aurelwing | INSECTOID | Swarm, final +Sky | COMMON | Classic early butterfly: grub, gilded chrysalis, glass-winged imago |
| F07 | Strayling -> Wardhund | QUADRUPED | Normal | COMMON | Farm stray that grows into a gate-guarding hound |
| F08 | Puffseed -> Dandelift -> Zephyrbloom | FLOATER_PLANTOID | Grass/Sky | COMMON | Dandelion drifter; the final stage steers whole seed-storms |
| F09 | Sparkit -> Amptail -> Fulgurun | QUADRUPED | Electric | UNCOMMON | Storm-chasing fox whose tail arcs to ground lightning |
| F10 | Rubblet -> Cairngo -> Monolode | BLOB | Stone | COMMON | Cave rubble that stacks itself into a walking cairn, then a monolith |
| F11 | Minnet -> Shoalfin -> Torrentfin | AQUATIC | Water | COMMON | The universal river fish; every rod pulls one up eventually |
| F12 | Mirelet -> Bogbane -> Venomire | QUADRUPED | Venom/Water | UNCOMMON | Bog newt whose skin sweats the moor's own toxin |
| F13 | Scrapling -> Sparhare -> Grandguard | BIPED | Brawl | UNCOMMON | Boxing hare line; the final stage plants itself like a castle door |
| F14 | Wispet -> Glimmourn -> Mournlight | FLOATER_PLANTOID | Phantom | UNCOMMON | Marsh light that leads travelers astray, then mourns them home |
| F15 | Trancet -> Mesmerel -> Oraclyne | BIPED | Mind | RARE | Blindfolded seer that dreams tomorrow's weather |
| F16 | Frisket -> Snowlope -> Glacielk | QUADRUPED | Ice | UNCOMMON | Alpine elk whose antlers refreeze into new icicle crowns nightly |
| F17 | Wyrmling -> Cragwyrm -> Zenithrax | SERPENT | Drake, final +Sky | RARE | The region's pseudo-legendary: a wyrm that climbs as it evolves; Elara's ace |
| F18 | Slaglet -> Ferralump -> Smeltitan | BLOB | Iron | UNCOMMON | Foundry slag come alive; eats scrap, excretes ingots |
| F19 | Fayling -> Chimesprite -> Sylphara | FLOATER_PLANTOID | Fey | RARE | Ring-dancer of flower circles; its chimes are its heartbeat |
| F20 | Shadelet -> Duskstalk -> Nightreave | QUADRUPED | Umbral | UNCOMMON | Moor cat woven from dusk; hunts the minute after sunset |
| F21 | Loomite -> Silklurk -> Weavenom | INSECTOID | Swarm/Venom | COMMON | Hedge spider whose webs stitch whole hedgerows shut |
| F22 | Burrit -> Gravelow -> Terradrill | QUADRUPED | Earth | COMMON | Route-digger mole; the potholes on Route 6 are its signature |
| F23 | Squallet -> Galecrest -> Thunderoc | AVIAN | Sky/Electric | RARE | Storm-petrel that rides thunderheads and lands only to evolve |
| F24 | Floelet -> Pinnifloe -> Aurorca | AQUATIC | Water/Ice | UNCOMMON | Ice-floe seal line ending in an aurora-flanked orca |
| F25 | Cinderjack -> Ashenhowl | QUADRUPED | Fire/Umbral | UNCOMMON | Volcanic scavenger jackal; howls at eruptions like others howl at moons |
| F26 | Bladebud -> Mantisprig -> Verdantis | INSECTOID | Grass/Swarm | UNCOMMON | Hedgerow mantis whose forearms are grafted pruning blades |
| F27 | Shardscale -> Bouldrake | QUADRUPED | Stone/Drake | RARE | Canyon-dwelling drake mistaken for boulders until it stands |
| F28 | Fleecel -> Rambellow | QUADRUPED | Normal/Fey | UNCOMMON | Pasture sheep whose fleece stores dreams; shearing day is surreal |
| F29 | Rustshade -> Hauntplate -> Dreadarmet | BIPED | Phantom/Iron | RARE | Abandoned armor walked home by something patient; Morwenna's ace |
| F30 | Puffjel -> Nimbjel -> Stratojel | FLOATER_PLANTOID | Sky/Mind | COMMON | Updraft jellyfish of the highlands; navigates by reading intentions |
| F31 | Echolet -> Gloomwing -> Noctursong | AVIAN | Venom/Umbral | COMMON | Cave bat line; the Highcrown tunnels belong to it |
| F32 | Padlet -> Lilyturt -> Lotustle | AQUATIC | Water/Grass | UNCOMMON | Pond turtle whose shell blooms into a lotus throne |
| F33 | Glowmite -> Emberfly -> Pyrelume | INSECTOID | Fire/Swarm | UNCOMMON | Firefly line; Cinderfell's streetlights are hired swarms |
| F34 | Scutlet -> Pangrol -> Fortrescale | QUADRUPED | Earth/Stone | UNCOMMON | Canyon pangolin that curls into an unopenable keep |
| F35 | Voltnut -> Coilcore | BLOB | Electric/Iron | UNCOMMON | A living dynamo; Gearspring's grid browns out when they swarm |
| F36 | Chillshade -> Frostwraith -> Permafright | FLOATER_PLANTOID | Ice/Phantom | RARE | Blizzard wraith; the cold spot in every Frostvale ghost story |
| F37 | Pebblefist -> Cobblebrawn -> Monumentor | BIPED | Brawl/Stone | UNCOMMON | Quarry golem that boxes with load-bearing form |
| F38 | Sporeling -> Duskcap -> Fungrove | FLOATER_PLANTOID | Grass/Umbral | COMMON | Night mushroom that walks its fairy ring with it |
| F39 | Skimmet -> Pelagair | AVIAN | Water/Sky | COMMON | Coastal gull; will steal held items in flavor text only |
| F40 | Gloopet -> Gluttonub | BLOB | Normal | COMMON | Amiable ooze that eats anything and apologizes after |
| F41 | Oozel -> Miremass | BLOB | Venom | COMMON | City-drain sludge; Gearspring's canals bred it, now they regret it |
| F42 | Moonfoal -> Astramare | QUADRUPED | Fey/Mind | RARE | Foal that walks on starlight; only appears in clear-night grass |
| F43 | Cinderasp -> Magmaboa -> Pyrothon | SERPENT | Fire/Drake | RARE | Volcano serpent coiled in Cinderfell's deep kilns |
| F44 | Prismite -> Geoborer -> Crystallisk | INSECTOID | Swarm/Stone | RARE | Geode-boring beetle; its galleries line Highcrown with crystal |
| F45 | Peckit -> Longstride -> Craneclash | AVIAN | Sky/Brawl | UNCOMMON | Martial crane that duels rivals on one leg out of courtesy |
| F46 | Stinglet -> Barbtail -> Scorpicrag | INSECTOID | Earth/Venom | UNCOMMON | Badlands scorpion; its molted husks dot Route 14 like warnings |
| F47 | Glidekin -> Soarel | QUADRUPED | Normal/Sky | COMMON | Flying squirrel that treats cliff towns as one big tree |
| F48 | Flopfin -> Levianth | SERPENT | Water -> Water/Drake | RARE | Famously feeble pond fish -- until, famously, it isn't |
| F49 | Gravepup -> Dirgehound | QUADRUPED | Phantom/Umbral | UNCOMMON | Graveyard hound that escorts lost souls (and lost trainers) out of the moor |
| F50 | Cogling -> Cerebrass | BIPED | Mind/Iron | RARE | Clockwork oracle of unknown make; ticks faster when it's wrong |
| F51 | Corvit -> Omenrook -> Auguravan | AVIAN | Umbral/Mind | UNCOMMON | Augur raven; reads entrails, prefers not to say whose |
| F52 | Pollenet -> Combwing -> Hivemind | INSECTOID | Swarm/Mind | UNCOMMON | A hive that promoted itself to a single opinion |
| F53 | Lurelet -> Deepgleam -> Abyssveil | AQUATIC | Water/Phantom | RARE | Deep-lake angler whose lure shows you what you miss most |
| F54 | Auricorn | QUADRUPED | Iron/Fey | RARE | Single-stage: gold-horned antelope; one grazes each save, somewhere |
| F55 | Duneleon | QUADRUPED | Earth/Umbral | RARE | Single-stage: canyon chameleon indistinguishable from Stonereach's walls |
| F56 | Emberkoi | AQUATIC | Fire/Water | RARE | Single-stage: hot-spring koi that keeps Frostvale's one warm pool warm |
| L01 | Zenaris | AVIAN | Sky/Fire | LEGENDARY | The Noon Crown -- the sun-at-zenith bird; roosts atop Highcrown Pass post-game |
| L02 | Nadirath | SERPENT | Umbral/Earth | LEGENDARY | The Under-Coil -- the nadir to Zenaris' zenith, coiled beneath the region; post-game depths of Route 10's bog |
| L03 | Equinara | QUADRUPED | Fey/Mind | LEGENDARY | The Horizon-Walker -- the balance between the two; roams route grass post-game after both others are seen |

Counting: 40 three-stage families (120) + 13 two-stage (26) + 3 single-stage
rares + 3 legendaries = **152 species**. Archetype spread: QUADRUPED 18,
BIPED 6, AVIAN 7, SERPENT 4, AQUATIC 6, INSECTOID 7, BLOB 5,
FLOATER_PLANTOID 6 families. All 18 types appear on at least two families.
Shiny variants are hue-rotated albedo recolors of every species (same mesh,
child material) per the plan's generator spec.

---

## 6. The 18 types

Type names are generic elemental concepts (per scope: mechanics are the
reference, names are ours). Code enum: `ZM_TYPE_*` in `Source/Data/ZM_Types.h`.

| # | Type | # | Type | # | Type |
|---|---|---|---|---|---|
| 1 | NORMAL | 7 | BRAWL | 13 | STONE |
| 2 | FIRE | 8 | VENOM | 14 | PHANTOM |
| 3 | WATER | 9 | EARTH | 15 | DRAKE |
| 4 | GRASS | 10 | SKY | 16 | UMBRAL |
| 5 | ELECTRIC | 11 | MIND | 17 | IRON |
| 6 | ICE | 12 | SWARM | 18 | FEY |

**The effectiveness matrix itself is S1 code** (`ZM_TypeChart` in
`Source/Data/`, an 18x18 table of x0 / x0.5 / x1 / x2 multipliers, locked by
golden-matrix unit tests). This document specifies only the list above and
the design intent:

- The starter triangle holds: GRASS > WATER > FIRE > GRASS.
- DRAKE resists the elemental trio (FIRE/WATER/GRASS) and is checked hard by
  FEY and ICE -- the late-game answer must be earned mid-game.
- MIND / UMBRAL / BRAWL form a second triangle; PHANTOM and NORMAL share the
  classic mutual-immunity relationship.
- IRON is the broad defensive wall (many resists, weak to FIRE/BRAWL/EARTH);
  ELECTRIC cannot hit EARTH; EARTH cannot hit SKY.
- Every type has at least one offensive niche (something only it hits super
  effectively among common defensive cores) and no type is strictly dominated.
- Weather couples to the chart: rain/sun boost WATER/FIRE respectively, sand
  chips non-EARTH/STONE/IRON, snow chips non-ICE (details in section 7).

---

## 7. Battle mechanics spec

The battle engine (`ZM_BattleEngine`, S2) is headless, synchronous, and
seeded (`ZM_BattleRNG`, PCG32); it emits an append-only `ZM_BattleEvent`
stream that is the single source of truth for tests and presentation alike.
Formulas below are LOCKED by the plan; exact constants live in code with
golden-vector unit tests.

### 7.1 In (locked formulas)

- **Stats:** Gen-III+ formulas, integer math. HP and five stats from base
  stats, IVs (0-31), EVs (510 total cap, 252 per stat), level, and nature
  (25 natures, x11/10 boosted stat, x9/10 hindered, integer-exact).
- **Damage:** Gen-V formula -- level term, power, Atk/Def or SpA/SpD by the
  physical/special split, STAB x1.5, type effectiveness from the chart,
  crit 1/24 at x1.5, random roll 85-100, burn halves physical, weather
  x1.5/x0.5, screens, spread factor omitted (singles only).
- **Catching:** Gen-III/IV formula with the 4-shake check sequence; ball
  modifiers, status modifiers, HP term.
- **Exp/levels:** 4 exp-curve families (fast / medium-fast / medium-slow /
  slow) assigned per species; exp share, EV yields per species, level-up
  move learning mid-battle, evolution checked post-battle (pure `Evolve()`;
  the cutscene is presentation).
- **Turn order:** run/item/switch resolve first, then moves by priority
  bracket, then effective speed (stages, paralysis, item hooks), RNG
  tie-break.
- **Status majors** (persist on `ZM_Monster`): SLEEP, POISON, TOXIC, BURN,
  PARALYSIS, FREEZE. **Volatiles** (battle-local): CONFUSED, FLINCH,
  LEECH_SEED, PROTECT, CHARGE, SEMI_INVULN, RECHARGE, LOCK, TRAP, TAUNT.
- **Moves:** ~220 moves as data rows over a ~60-kind `ZM_MOVE_EFFECT` enum
  with one executor switch (`ZM_MoveExecutor`). PP, accuracy, priority,
  contact flag, crit-stage flags per row. ~25 TM items + 4 move tutors share
  one teach-move flow; the 8 badge reward moves (section 3.4) are TMs.
- **Abilities:** ~50, implemented as structs of function-pointer hooks
  (OnSwitchIn / OnModifyStat / OnModifyDamage / OnStatusTry / OnContact /
  OnTurnEnd / OnFaint / OnAccuracy / ...).
- **Weather:** rain / sun / sand / snow -- damage modifiers, end-of-turn
  chip, ability interactions; set by moves, abilities, and per-scene
  WorldSpec weather zones.
- **Trainer AI:** tiers RANDOM -> GREEDY (expected damage x accuracy) ->
  SMART (KO detection, status/setup valuation, switching, healing items) ->
  CHAMPION (2-ply minimax). Pure function of state + RNG.
- **Breeding** (post-Gym systems, daycare at Milldown): egg groups, offspring
  is mother's base evolution, 3-IV inheritance (5 with the Heirloom Knot
  item), nature lock via the Stasis Stone, egg steps and hatching.
- **Battle Tower** (post-game): level-50 clamp via `ZM_BattleConfig`
  override, rental teams, opponent generation scaling with streak, AI tier
  escalation, boss every 7th battle, streak persisted in save data.

### 7.2 Out (explicit cuts -- see Scope.md section 2)

- **Substitute, Encore, Transform, weight-based moves** -- the move-effect
  enum does not include them.
- **Doubles** (and any multi-battle format) -- singles only; struct layout
  does not preclude doubles later, but it is out of scope regardless.
- **Dynamax-analog** -- no battle gimmick of any kind (no Mega/Z/Tera analog).
- **Audio, networking/trading** -- engine/scope level cuts.

---

## 8. Progression curve

Badge count gates story roadblocks (route wardens, gate NPCs -- via
`ZM_StoryFlags`), traded-level obedience is NOT modeled (no trading); the
curve is enforced by wild/trainer levels alone. Bands are tuning targets, not
promises; the S11 balance pass (headless AI-vs-AI simulation) owns the final
numbers.

| Beat | Where | Leader ace / key level | Wild band |
|---|---|---|---|
| Gym 1 (Bloom) | Thornacre | L13 | Routes 1-2: L2-8 |
| Gym 2 (Kiln) | Cinderfell | L18 | Routes 2-3: L8-14 |
| Gym 3 (Tide) | Tidegate | L23 | Routes 3-5: L13-19 |
| Gym 4 (Coil) | Gearspring | L28 | Routes 5-7: L18-24 |
| Gym 5 (Gale) | Skyshear | L33 | Routes 7-8: L23-29 |
| Gym 6 (Wisp) | Umbermoor | L38 | Routes 9-10: L28-34 |
| Gym 7 (Rime) | Frostvale | L43 | Routes 11-12: L33-39 |
| Gym 8 (Crest) | Stonereach | L48 | Routes 13-14: L38-44 |
| Victory Road | Highcrown Pass | -- | L45-50 |
| Elite Four | Zenith Gate | L52-56 (one per seat) | -- |
| Champion Elara | Zenith Gate | L58-60 ace | -- |
| Post-game | region-wide | Champion rematch L65-70; legendaries L60-70 | route wilds +10 |
| Battle Tower | Zenith Gate | all battles clamped to L50 | -- |

**Rival battle beats (6):** Route 1 (L5, scripted first battle), Tidegate
docks (~L21, pre-Gym 3), Milldown Crossing (~L30), Route 11 snowline (~L40,
Vesper's first on-screen loss to the player that visibly lands), Highcrown
Pass gate (~L50), Zenith Gate steps (~L54, final -- fought as equals before
the League doors). Post-game: standing Tower-lobby rematch.

**Post-game:** Mom's trainer-card scene at home unlocks the Champion rematch
row; Zenaris/Nadirath become approachable (Highcrown summit / Route 10 bog
depths); seeing both wakes Equinara roaming route grass; the Battle Tower
(scene 95) opens with rentals, streaks, and escalating AI.

---

## 9. Story synopsis

The player grows up in Dawnmere Village, at the bottom of a region where
every road goes up. Professor Aster -- the mountain's resident scholar of
"ascension" -- takes on two new field assistants: the player and Vesper, the
neighbor kid named for the evening star. In her lab (scene 41) each picks a
starter -- Fernfawn, Kindlet, or Finlet -- and Vesper immediately claims the
one that beats it, declares the badge run a race, and is gone up Route 1
before the door shuts. Mom packs the player's bag, mentions -- too casually --
that she once collected seven badges herself, and waves from the gate.

The journey is the region: hedgerow farms and Fenna's growing maze in
Thornacre; Bram's kilns above the ash gorge in Cinderfell; down to salt air
and Maris' sea-locks at Tidegate, where Vesper's second challenge ends in
grumbling; the dynamo yards of Gearspring and Tessa's switchgear puzzle; the
crossroads at Milldown, where the daycare couple teach the player about eggs
and Vesper hands the player their third battle and first real fright. Then
the high country turns serious -- Aquilo's rope bridges over the Skyshear
drops, Morwenna's lantern-lit dark-walk in fog-drowned Umbermoor, Halvard's
ice floor above the Frostvale snowline (and on Route 11, the snowline battle
where Vesper finally loses composure and something better replaces it), and
last, Vardis and his old drakes in the Stonereach canyons, guarding the
region's crown like a final exam.

There is no villainous team in Culmina, and the region does not miss it. The
antagonist is the mountain: the badge gates, the weather, the wilds of
Highcrown Pass -- the cave-and-summit gauntlet every challenger must climb
alone -- and, always one switchback ahead or behind, Vesper. Their last
battle, on the steps of Zenith Gate with the League doors in view, is the
rivalry's summit: no stakes but pride, nothing left of the race but two
trainers who made each other better.

Inside the League Halls wait Cassia's polite poisons, Torben's patient
footwork, Lumen's read-ahead psychics, and Sable's long shadows -- then the
Champion's chamber, where Elara the astronomer has held the seat for ten
years with Zenithrax, the wyrm that climbed the whole mountain to grow its
wings. Defeating her puts the player's name on the crown of the region their
whole journey has been shaped like. The credits roll from the summit looking
down: village to gate, dawn to zenith.

Afterward the region opens its last doors: Mom finally tells her
seven-badge story, the Champion accepts rematches, the Noon Crown and the
Under-Coil surface for those who look, and the Battle Tower starts counting
streaks. The race is over; the climbing, pleasantly, is not.

---

## 10. Cross-references

- [Scope.md](Scope.md) -- the binding in/out gate this document details.
- [Roadmap.md](Roadmap.md) -- when each section becomes implementation
  (dex/data at S1, battle at S2, world at S3/S9/S10, post-game at S11).
- [TestPlan.md](TestPlan.md) -- how every system above is gated.
- [AssetManifest.md](AssetManifest.md) -- the baked-asset catalogue the dex
  and world tables imply.
- [Glossary.md](Glossary.md) -- term definitions (dex, whiteout, streak,
  terrain set, WorldSpec, event stream).

*This GDD is the reference document; it is updated at each stage gate. The
species table (section 5) and world table (section 2.2) are the canonical
inputs to `ZM_SpeciesData` and `ZM_WorldSpec` -- S1 transcribes them into
code, and from then on code is authoritative.*
