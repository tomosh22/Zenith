# Paws & Pins - Game Design Document

## 1. Game Identity & Market Positioning

**Working Title:** Paws & Pins
**Genre:** Puzzle + Pinball Hybrid
**Target Platforms:** Android (Google Play launch), iOS (stretch goal)
**Target Audience:** Casual puzzle fans, cat lovers, ages 12+
**Session Length:** 2-5 minutes per puzzle level, 1-3 minutes per pinball gate

### 1.1 Elevator Pitch

Slide colorful polyomino shapes to rescue cats in 100 handcrafted puzzle levels, then celebrate with a pinball minigame every 10 levels. Collect all 100 cats to fill your Cat Cafe.

### 1.2 Unique Selling Points

1. **Dual Gameplay Loop** - Puzzle + pinball in one game; pinball gates break up puzzle fatigue
2. **Cat Collection** - 100 uniquely named and bred cats as persistent rewards
3. **Guaranteed Solvability** - Reverse-scramble generation ensures every puzzle has a solution
4. **Deep but Accessible** - Simple slide-to-match core with layered mechanics (blockers, conditional shapes, blocker-cats)

### 1.3 Competitive Landscape

| Competitor | Differentiator |
|-----------|---------------|
| Threes! | Paws & Pins adds polyomino variety + pinball |
| Cat Game (Purrfect Tale) | P&P has actual puzzle mechanics, not idle/sim |
| Peggle | P&P integrates pinball as progression gates, not standalone |

---

## 2. Core Game Loop

### 2.1 Primary Loop (Puzzle)

```
Select Level -> Solve Puzzle -> Star Rating -> Rescue Cat -> Next Level
                                                    |
                                              (every 10th level)
                                                    v
                                              Pinball Gate -> Clear Gate -> Continue
```

### 2.2 Puzzle Mechanics

**Grid:** Rectangular grid (5x5 to 10x10), cells are either Floor or Empty (void).

**Shapes:** Colored polyomino pieces placed on the grid. 8 shape types:

| Type | Cells | Shape |
|------|-------|-------|
| Single | 1 | Square |
| Domino | 2 | 1x2 horizontal |
| I-Shape | 3 | 1x3 line |
| L-Shape | 4 | L configuration |
| T-Shape | 4 | T configuration |
| S-Shape | 4 | S skew |
| Z-Shape | 4 | Z skew |
| O-Shape | 4 | 2x2 square |

**Movement:** Draggable shapes slide one cell at a time in cardinal directions (up/down/left/right). One "move" equals one shape pick, regardless of distance slid. Shapes cannot rotate during gameplay.

**Blocking Rules:**
- Shapes are blocked by grid boundaries, empty cells, other draggable shapes, static blockers, and different-color cats
- Shapes can move onto floor cells, same-color cats (eliminating them), and already-eliminated cats

**Cat Elimination:**
- Normal cats: eliminated when a same-color shape overlaps their cell
- Blocker-cats (`bOnBlocker = true`): sit atop static blockers, eliminated when a same-color shape is orthogonally adjacent (not overlapping)

**Win Condition:** Eliminate all cats on the grid.

### 2.3 Additional Mechanics

**Conditional (Locked) Shapes:**
- Shapes with `uUnlockThreshold > 0` cannot be moved until that many cats have been eliminated
- A yellow number above the shape indicates the unlock requirement
- Introduced at Hard tier (level 46+)

**Static Blockers:**
- Non-draggable shapes that act as obstacles
- Use `TILEPUZZLE_COLOR_NONE` (no elimination mechanic)

**Undo System:**
- First undo per level is free; additional undos cost 20 coins
- Full state snapshots stored (shape positions, cat elimination mask, removed flags)

**Hint System:**
- Costs 30 coins (first hint free)
- Highlights one shape and direction via BFS solver
- Auto-clears when the player makes any move

**Skip Level:**
- Offered after 3 resets on the same level
- Costs 100 coins

### 2.4 Pinball Gates

Every 10th level (levels 10, 20, ..., 100) triggers a pinball minigame. 10 total gates.

**Pinball Mechanics:**
- Physics-based ball (Jolt Physics engine) on a vertical playfield
- Plunger launcher: drag down and release to launch ball upward
- Pegs: 6-8 pegs per gate, procedurally placed per layout (6 layout variations)
- Target: bonus scoring zone (500 points per hit, 1.0s cooldown)
- Peg hit: 100 points per peg

**Gate Objectives:**

| Gate | Level | Objective | Score Target | Pegs | Max Balls |
|------|-------|-----------|-------------|------|-----------|
| 0 | 10 | Score Threshold | 1,000 | 6 | Unlimited |
| 1 | 20 | Score Threshold | 2,000 | 6 | Unlimited |
| 2 | 30 | Hit All Pegs | - | 6 | Unlimited |
| 3 | 40 | Score Threshold | 3,000 | 7 | 3 |
| 4 | 50 | Hit All Pegs | - | 8 | Unlimited |
| 5 | 60 | Score Threshold | 4,000 | 7 | 3 |
| 6 | 70 | Target Hits (5) | - | 7 | Unlimited |
| 7 | 80 | Score Threshold | 5,000 | 8 | 3 |
| 8 | 90 | Combined (Score + All Pegs) | 3,000 | 8 | Unlimited |
| 9 | 100 | Target Hits (10) | - | 8 | Unlimited |

**Pinball Playfield:**
- Bounds: x=[-2.4, 2.4], y=[0.0, 8.0]
- Wall thickness: 0.3 units
- Ball radius: 0.15 units, max launch force: 35.0 units/sec

---

## 3. Content & Progression

### 3.1 Level Count & Structure

- **100 puzzle levels** + **10 pinball gates**
- Levels are pre-generated offline via TilePuzzleLevelGen tool and stored as `.tlvl` binary files
- Each level awards exactly 1 cat (cat ID = level number - 1)

### 3.2 Difficulty Curve (6 Tiers)

| Tier | Levels | Grid | Colors | Cats/Color | Blockers | Shapes | Special Mechanics |
|------|--------|------|--------|-----------|----------|--------|-------------------|
| Tutorial | 1-10 | 5x5-6x6 | 1-2 | 1 | 0 | Single, Domino | None |
| Easy | 11-25 | 6x6-7x7 | 2 | 1-2 | 1-2 | +I-Shape | Static blockers |
| Medium | 26-45 | 7x7-8x8 | 2-3 | 2 | 1-2 | +L,T,S shapes | Blocker-cats (0-1) |
| Hard | 46-65 | 8x8 | 3 | 2-3 | 1-2 | All 8 shapes | Conditional shapes |
| Expert | 66-80 | 8x8-9x9 | 3-4 | 2-3 | 1-2 | All 8 shapes | Multiple conditionals |
| Master | 81-100 | 8x8 | 3 | 3 | 1-2 | All 8 shapes | All mechanics combined |

### 3.3 Level Generation Algorithm

**Reverse-Scramble Approach (guarantees solvability):**
1. Create grid with floor cells and 1-cell empty border
2. Place static blockers on random interior floor cells
3. Place cats on unoccupied floor cells (colored, plus blocker-cats on blockers)
4. Place draggable shapes overlapping their matching cats (solved position)
5. Randomly scramble via valid moves until all cats have been covered at least once and no cat is currently covered
6. The reverse of the scramble sequence is a valid solution

**Solver Verification:** BFS solver confirms solvability and computes minimum move count (par). State limit: 500,000 states.

### 3.4 Star Rating System

```
par = level.uMinimumMoves (from solver)
if moveCount <= par:       3 stars
if moveCount <= par + 2:   2 stars
else:                      1 star
```

### 3.5 Daily Puzzle System

- One puzzle per day, generated from seed = YYYYMMDD (deterministic)
- Difficulty equivalent to levels 50-70 (Hard tier)
- Awards 50 coins on completion
- Streak tracking: consecutive days increments streak counter

---

## 4. Coin Economy & Monetization

### 4.1 Coin Sources

| Source | Coins |
|--------|-------|
| Level complete | +10 |
| 3-star bonus | +5 |
| Pinball gate clear | +25 |
| Daily puzzle complete | +50 |

### 4.2 Coin Sinks

| Action | Cost |
|--------|------|
| Undo (2nd+ per level) | 20 |
| Hint | 30 |
| Skip level | 100 |
| Lives refill (5 lives) | 50 |

### 4.3 Lives System

- **Maximum:** 5 lives
- **Regeneration:** 1 life every 20 minutes (1,200 seconds)
- **Loss trigger:** Exiting a puzzle without completing it costs 1 life
- **Refill:** 50 coins for instant full refill

### 4.4 Monetization Strategy (Proposed)

**Rewarded Video Ads:**
- Watch ad to earn coins (suggested: 25 coins per ad)
- Watch ad for free undo/hint
- Watch ad to refill lives

**Interstitial Ads:**
- Between levels (every 3-5 levels, skip for premium)
- After pinball gate attempts

**In-App Purchases:**
- Remove ads (one-time)
- Coin bundles (tiered: small/medium/large)
- Starter pack (coins + remove ads bundle)

---

## 5. Meta-Game: Cat Cafe

### 5.1 Collection System

- **100 cats** total, one per puzzle level
- **128-bit bitfield** tracks collected cats (supports up to 128)
- Cached count in `uCatsCollectedCount`

### 5.2 Cat Identity

Each cat has:
- **Name:** From pool of 100 unique names (Whiskers, Mittens, Shadow, Luna, Ginger, Oreo, Simba, Cleo, Felix, Mochi, Nala, Tiger, Smokey, Pepper, Coco, Daisy, Leo, Bella, Oliver, Willow, Pumpkin, Ziggy, Maple, Jasper, Clover, Biscuit, Pixel, Hazel, Sage, Tinker, Domino, Marbles, Nutmeg, Waffles, Sprout, Patches, Velvet, Pebble, Cricket, Buttons, Mocha, Truffle, Rosie, Chester, Fern, Juniper, Acorn, Bramble, Thistle, Starling, Ember, Quartz, Dusk, Flint, Sorrel, Briar, Marigold, Clementine, Ivy, Aster, Sable, Rusty, Cosmo, Pippin, Mango, Cashew, Raisin, Almond, Sesame, Taffy, Fudge, Toffee, Praline, Chai, Latte, Espresso, Barley, Opal, Onyx, Garnet, Topaz, Pearl, Coral, Jade, Sapphire, Amber, Ruby, Sterling, Copper, Marble, Slate, Storm, Frost, Blaze, Aurora, Nova, Eclipse, Zenith, Comet, Nebula)
- **Breed:** From pool of 20 breeds (Tabby, Calico, Siamese, Persian, Bengal, Maine Coon, Ragdoll, Sphynx, British Shorthair, Abyssinian, Scottish Fold, Russian Blue, Birman, Norwegian Forest, Turkish Angora, Burmese, Tonkinese, Somali, Chartreux, Devon Rex)
- Assignment: `name = s_aszCatNames[catID % 100]`, `breed = s_aszCatBreeds[catID % 20]`

### 5.3 Cat Cafe UI

- **8 cats per page**, paginated display
- **13 pages** for 100 cats
- Per-cat card shows: name, breed, level number, 3-star indicator
- Uncollected cats display "???" placeholder

---

## 6. Save Data System

### 6.1 Save Format

- **Binary serialization** via `Zenith_DataStream`
- **Save version:** 5 (`uGAME_SAVE_VERSION`)
- **Auto-save triggers:** Level complete, menu return, quit
- **Slot:** Single `autosave` slot

### 6.2 Persisted Data

| Field | Type | Description |
|-------|------|-------------|
| `uHighestLevelReached` | uint32 | Progression gate |
| `uCurrentLevel` | uint32 | Resume point |
| `axLevelRecords[100]` | struct[] | Per-level: completed, best moves, best time, best stars |
| `uPinballScore` | uint32 | Total accumulated pinball score |
| `uCoins` | uint32 | Currency balance |
| `auStarRatings[100]` | uint8[] | 0-3 stars per level |
| `uTotalStars` | uint32 | Cached sum |
| `abCatsCollected[16]` | uint8[] | 128-bit cat collection bitfield |
| `uCatsCollectedCount` | uint32 | Cached count |
| `uDailyStreak` | uint32 | Consecutive days played |
| `uLastDailyDate` | uint32 | YYYYMMDD format |
| `uPinballGateFlags` | uint16 | Bitmask for 10 gates |
| `uDailyPuzzleBestMoves` | uint32 | Today's best |
| `uLastDailyPuzzleDate` | uint32 | YYYYMMDD |
| `uLives` | uint32 | Current lives (0-5) |
| `uLastLifeRegenTime` | uint32 | Unix timestamp for regeneration |

---

## 7. User Interface

### 7.1 Game States

```
MAIN_MENU -> LEVEL_SELECT -> PLAYING -> SHAPE_SLIDING -> CHECK_ELIMINATION
                                ^            |                    |
                                |            v                    v
                                +---- LEVEL_COMPLETE ----> VICTORY_OVERLAY
                                |
                                +---- CAT_CAFE
```

### 7.2 Main Menu

- **Continue:** Resume from current level
- **Level Select:** Browse levels (20 per page, 4x5 grid)
- **Cat Cafe:** View cat collection
- **Daily Puzzle:** Play daily challenge
- **Pinball:** Jump to pinball scene
- **New Game / Reset Save:** Reset all progress
- **Displays:** Coins, lives (with regen timer), daily streak

### 7.3 Level Select

- **20 levels per page** (4x5 grid)
- Color coding:
  - Gold = 3-star completed
  - Purple = Pinball gate level (every 10th)
  - Green = Current level
  - Blue = Unlocked, not yet completed
  - Dim gray = Locked

### 7.4 Gameplay HUD

- Level number
- Move counter
- Cats remaining counter
- Progress bar
- Buttons: Reset (R), Undo (U), Hint (H), Skip (offered after 3 resets), Menu (Esc)

### 7.5 Victory Overlay

- Star rating (sequential appearance, 0.4s per star)
- Cat name announcement ("Cat rescued: [Name]!")
- Coins earned display
- Next Level button

### 7.6 Input

**Keyboard:** WASD/Arrows to move, Space to select/deselect, R reset, U undo, H hint, N next level, Esc menu

**Mouse/Touch:** Click to select shape (emissive glow highlight), drag to move shape in dominant direction, up to 4 cells per drag update, auto-snap on release

---

## 8. Visual Identity & Art Direction

### 8.1 Style: "Soft Geometric"

- Clean polyomino shapes rendered as colored cubes on a flat grid
- Cats represented as colored spheres (radius 0.35 units)
- Floor cells as thin cubes (height 0.05 units)
- Shapes as medium-height cubes (height 0.25 units)
- Emissive glow highlights for selection and hints

### 8.2 Color Palette

| Color | Use | Suggested Hex |
|-------|-----|--------------|
| Red | Shapes + Cats | #E63C3C |
| Green | Shapes + Cats | #3CC83C |
| Blue | Shapes + Cats | #3C6EE6 |
| Yellow | Shapes + Cats | #E6D23C |
| Purple | Shapes + Cats | #9B3CE6 |
| Dark Gray | Static Blockers | #503C1E |
| Light Gray | Floor | #4D4D59 |
| Black | Empty/Void | #191926 |

### 8.3 Animations

| Animation | Duration | Style |
|-----------|----------|-------|
| Shape slide | 0.15s | Linear interpolation |
| Cat elimination | 0.3s | Fade out (opacity driven by progress) |
| Hint flash | ~0.1s | On/off blink cycle |
| Victory stars | 0.4s each | Sequential appearance |
| Gate celebration | 2.5s | Success/failure display |

### 8.4 Camera

- Orthographic overhead view, centered on grid
- Zoom adjusted per grid size (max 12x12)
- Looking down at -Y axis

### 8.5 Art Direction Goals (Store Release)

**Current state:** Programmer art (solid-color cubes and spheres)

**Target state for release:**
- Pastel-toned materials with subtle PBR properties (roughness, metallic)
- Cat spheres replaced with simple cat-shaped meshes (or textured spheres with cat face)
- Particle effects for elimination (sparkles/confetti)
- Bloom post-processing on emissive highlights
- Consistent iconography for UI buttons
- Polished transitions between menus

---

## 9. Audio Design

### 9.1 Current State

**No audio system exists in the engine.** Audio integration required.

### 9.2 Recommended Approach

Integrate a lightweight audio library (OpenAL Soft, miniaudio, or FMOD) with cross-platform support (Windows + Android).

### 9.3 Required Sound Effects

| Event | Sound | Priority |
|-------|-------|----------|
| Shape select | Soft click/pop | Must-Have |
| Shape slide | Gentle whoosh | Must-Have |
| Shape blocked | Soft thud | Must-Have |
| Cat elimination | Happy meow + sparkle | Must-Have |
| Level complete | Celebratory jingle | Must-Have |
| Star earned | Chime (ascending pitch per star) | Must-Have |
| Undo | Rewind swoosh | Nice-to-Have |
| Hint activate | Gentle bell | Nice-to-Have |
| Button click | UI click | Must-Have |
| Pinball launch | Spring/plunger sound | Must-Have |
| Peg hit | Ping/ding (varied pitch) | Must-Have |
| Target hit | Cash register/bonus sound | Must-Have |
| Gate clear | Fanfare | Must-Have |
| Shape unlock | Lock breaking / click | Nice-to-Have |

### 9.4 Music

| Context | Style | Priority |
|---------|-------|----------|
| Main menu | Calm, lofi/jazzy | Must-Have |
| Puzzle gameplay | Light ambient, non-distracting | Must-Have |
| Pinball | Upbeat, energetic | Must-Have |
| Cat cafe | Cozy, warm | Nice-to-Have |
| Victory | Short celebratory sting | Must-Have |

---

## 10. Onboarding & Tutorial

### 10.1 Tutorial Levels (Tier 1: Levels 1-10)

Tutorial tier uses simplified parameters:
- Small grids (5x5 to 6x6)
- 1-2 colors only
- Single and Domino shapes only
- No blockers, no blocker-cats, no conditional shapes
- Low par moves (2+)

### 10.2 Progressive Mechanic Introduction

| Level Range | New Mechanic | Contextual Hint |
|-------------|-------------|-----------------|
| 1-5 | Basic movement + single shapes | "Slide shapes to cover same-color cats" |
| 6-10 | Domino shapes + 2 colors | "Larger shapes cover more ground" |
| 11-15 | Static blockers | "Dark shapes can't be moved" |
| 16-20 | Multiple cats per color | "Match all cats of each color" |
| 26-30 | Blocker-cats | "Some cats sit on blockers - get adjacent!" |
| 46-50 | Conditional shapes | "Locked shapes unlock after eliminating cats" |

### 10.3 Hint System as Safety Net

- First hint per level is free
- BFS solver computes optimal next move from current state
- Highlights shape and direction with blink animation
- Fallback message if solver times out

---

## 11. Live Operations (Proposed)

### 11.1 Daily Puzzles

Already implemented:
- One puzzle per day (seed = YYYYMMDD)
- Hard-tier difficulty (equivalent to levels 50-70)
- 50 coin reward
- Streak tracking

### 11.2 Future Considerations

- **Weekly Challenges:** Themed puzzle packs (e.g., "Only L-shapes this week")
- **Seasonal Events:** Holiday-themed cat skins and level backgrounds
- **Leaderboards:** Best moves/time per level (requires online infrastructure)
- **Push Notifications:** Daily puzzle reminder, life refill notification

---

## 12. Platform Requirements

### 12.1 Android Launch Checklist

| Requirement | Status |
|------------|--------|
| Vulkan renderer | Implemented |
| Touch input | Implemented (touch-to-mouse emulation) |
| Pre-compiled shaders (.spv) | Pipeline exists (FluxCompiler) |
| Asset pipeline (.zmesh, .zmat, .ztex) | Implemented |
| Portrait orientation | Needs implementation (currently landscape) |
| App icon (512x512) | Needs creation |
| Feature graphic (1024x500) | Needs creation |
| Screenshots (min 2) | Needs creation |
| Privacy policy | Needs creation |
| Content rating questionnaire | Needs completion |
| Target API level | Must match current Google Play requirements |

### 12.2 iOS (Stretch Goal)

- MoltenVK for Vulkan-on-Metal
- Apple Developer Program enrollment
- App Store review guidelines compliance

### 12.3 Technical Constraints

- No runtime shader compilation on mobile (pre-compile via FluxCompiler)
- No `std::function` (engine convention: function pointers only)
- No Assimp on mobile (use pre-baked .zmesh/.zmat/.zanim formats)
- `std::unordered_map` usage should be migrated to engine hash map when available

---

## 13. KPIs & Success Metrics

### 13.1 Retention Targets

| Metric | Target |
|--------|--------|
| D1 Retention | > 40% |
| D7 Retention | > 20% |
| D30 Retention | > 10% |

### 13.2 Engagement

| Metric | Target |
|--------|--------|
| Avg session length | 5-10 minutes |
| Sessions per day | 2-3 |
| Level completion rate (Tutorial) | > 95% |
| Level completion rate (Master) | > 60% |
| Daily puzzle engagement | > 30% of DAU |

### 13.3 Monetization

| Metric | Target |
|--------|--------|
| ARPDAU | $0.05-0.15 |
| IAP conversion rate | > 3% |
| Ad fill rate | > 95% |

### 13.4 Technical

| Metric | Target |
|--------|--------|
| Crash-free rate | > 99.5% |
| ANR rate | < 0.5% |
| Cold start time | < 3 seconds |
| Frame rate (puzzle) | 60 FPS |
| Frame rate (pinball) | 60 FPS |
| APK size | < 100 MB |

---

## Appendix A: Technical Architecture

### A.1 State Machine

```
TilePuzzleGameState:
  MAIN_MENU (0)
  PLAYING (1)
  SHAPE_SLIDING (2)       -- animation playing
  CHECK_ELIMINATION (3)   -- compute newly eliminated cats
  LEVEL_COMPLETE (4)
  LEVEL_SELECT (5)
  CAT_CAFE (6)
  VICTORY_OVERLAY (7)
```

### A.2 Scene Structure

- **Scene 0 (Menu):** GameManager + UI + TilePuzzle_Behaviour
- **Scene 1 (Puzzle):** Dynamic puzzle scene (floor, shapes, cats entities)
- **Scene 2 (Pinball):** Dynamic pinball scene (walls, pegs, ball, target entities)

### A.3 Key Constants

```
Gameplay:
  Cell size:              1.0 unit
  Floor height:           0.05 units
  Shape height:           0.25 units
  Cat height:             0.35 units
  Cat radius:             0.35 units
  Slide animation:        0.15 seconds
  Elimination duration:   0.3 seconds
  Max grid size:          12

Economy:
  Undo cost:              20 coins
  Hint cost:              30 coins
  Skip cost:              100 coins
  Life refill cost:       50 coins
  Level complete reward:  10 coins
  3-star bonus:           5 coins
  Gate clear reward:      25 coins
  Daily puzzle reward:    50 coins

Lives:
  Max lives:              5
  Regen interval:         1200 seconds (20 min)
  Resets before skip:     3

Pinball:
  Max pegs:               8
  Max layouts:            6
  Peg score:              100 points
  Target score:           500 points
  Gate celebration:       2.5 seconds
  Max launch force:       35.0 units/sec
  Ball radius:            0.15 units
```

### A.4 Level File Format (.tlvl)

```
[Optional META header]
  META_MAGIC (0x4D455441) | META_VERSION (1) | 11 uint32 metadata fields | 1 uint64 hash

[TPLV data]
  TPLV_MAGIC (0x54504C56) | TPLV_VERSION (2)
  Grid: width, height, minimumMoves
  Cells: count + cell types (uint8 each)
  Shapes: count + per-shape (origin, color, unlockThreshold, inline definition)
  Cats: count + per-cat (color, gridX, gridY, onBlocker)
  Solution (v2+): count + per-move (shapeIndex, endX, endY)
```

## Appendix B: Critical File Paths

| File | Purpose |
|------|---------|
| `Games/TilePuzzle/Components/TilePuzzle_Behaviour.h` | Main game component (includes MetaGame, Pinball) |
| `Games/TilePuzzle/Components/TilePuzzle_Types.h` | All type definitions, enums, data structures |
| `Games/TilePuzzle/Components/TilePuzzle_Rules.h` | Movement validation, elimination computation |
| `Games/TilePuzzle/Components/TilePuzzle_MetaGame.h` | Star rating, coins, lives, cat cafe, save |
| `Games/TilePuzzle/Components/TilePuzzle_SaveData.h` | Save data schema, versioned serialization |
| `Games/TilePuzzle/Components/TilePuzzle_LevelGenerator.h` | Level generation, 6-tier difficulty system |
| `Games/TilePuzzle/Components/TilePuzzle_Pinball.h` | Pinball gate mechanics |
| `Games/TilePuzzle/Components/TilePuzzleLevelData_Serialize.h` | Binary serialization for .tlvl files |
| `Games/TilePuzzle/Tests/TilePuzzle_AutoTest.h` | Autotest suite (all 100 levels + 10 gates) |
| `TilePuzzleLevelGen/TilePuzzleLevelGen.cpp` | Offline level generation tool |

## Appendix C: Implementation Priority

### Phase 1: Art Polish (Pre-Store)
1. Replace placeholder cubes/spheres with stylized meshes (cats, shapes)
2. Add particle effects (elimination sparkles, victory confetti)
3. Create consistent UI icon set
4. Implement portrait orientation support
5. Create store listing assets (icon, feature graphic, screenshots)

### Phase 2: Audio Integration
1. Integrate audio library (miniaudio or OpenAL Soft)
2. Add SFX for all gameplay events
3. Add background music (menu, puzzle, pinball, cafe)

### Phase 3: Monetization
1. Integrate ad SDK (AdMob)
2. Add rewarded video ad placements
3. Add interstitial ad placements
4. Integrate Google Play Billing for IAPs
5. Implement remove-ads purchase

### Phase 4: Polish & Launch
1. Onboarding tutorial overlays
2. Settings screen (sound, music, haptics toggles)
3. Localization support
4. Crash reporting integration
5. Analytics integration
6. Store listing optimization (ASO)
