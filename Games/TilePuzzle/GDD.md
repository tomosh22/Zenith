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

**Peg Hit Visual Feedback:**
- On hit: peg swaps to flash material `(1.0, 0.8, 0.5)` with high emissive (intensity 3.0)
- Flash duration: 0.3s for hit-all-pegs gates, 1.0s for other gate types
- Hit-all gates: after flash, peg stays lit with hit material `(0.7, 0.5, 0.4)` (emissive 1.2) to show progress
- Other gates: peg reverts to normal material after flash and can be re-hit for additional points

**Pinball HUD:**

Each gate displays clear on-screen instructions telling the player what they need to do:

| HUD Element | Position | Color | Visibility | Content |
|-------------|----------|-------|------------|---------|
| `PinballObjective` | Top-left | Amber | Always | Pass criteria in plain English (e.g., "Score 1000 points", "Hit all 6 pegs", "Hit the target 5 times", "Score 3000 + Hit all pegs") |
| `PinballPegCount` | Below objective | Cyan | Hit-all-pegs / combined gates only | "Pegs: X/Y" progress counter |
| `PinballTargetCount` | Below peg count | Cyan | Target-hits / combined gates only | "Targets: X/Y" progress counter |
| `PinballBalls` | Top-right | Orange | Limited-ball gates only | "Balls: N" remaining counter |
| `PinballGateStatus` | Center | Yellow/Green/Red | Gate end | Pass/fail result display |
| `PinballGateNum` | Top-center | Light Blue | During gate | "Gate N" label |
| `PinballScore` | Top-left, below objective | White | Always | "Score: N" current session score |
| `PinballHighScore` | Below score | White | Always | "Total: N" lifetime pinball score |

After all 10 gates are cleared, the objective text shows "Freeplay - All gates cleared!" and the player can continue playing pinball for fun with cycling peg layouts.

**Quick Play Pinball:** Unlocked after clearing Gate 1 (level 10). Accessible from the main menu "Pinball" button.

**Gate Selection UI:** On entering Quick Play, a gate selection screen (`PINBALL_STATE_GATE_SELECT`) shows 10 gate buttons in a 5x2 grid:
- Cleared gates: blue, selectable
- Current/active gate: green, selectable
- Locked gates: gray, not selectable
- "Freeplay" button: visible after all 10 gates cleared
- "Back" button: returns to main menu scene

**Daily Pinball Bonus:** First gate completion of the day awards 25 bonus coins. Tracked via `uLastDailyPinballDate` (YYYYMMDD) in save data. Shows "+25 Daily Bonus!" text on award.

**Thematic Elements:**
- Pinball pegs use flattened sphere geometry with brown tint `(0.55, 0.35, 0.3)`
- Ball material uses orange base color instead of silver for cat-face color tint
- Collision remains circular (sphere) regardless of visual geometry

**Pinball Playfield:**
- Bounds: x=[-2.4, 2.4], y=[0.0, 8.0]
- Wall thickness: 0.3 units
- Ball radius: 0.15 units, max launch force: 35.0 units/sec
- Two angled wall pieces at the top redirect the ball from the launcher channel into the main play area (30deg and 10deg rotation)

---

## 3. Content & Progression

### 3.1 Level Count & Structure

- **100 puzzle levels** + **10 pinball gates**
- Levels are pre-generated offline via TilePuzzleLevelGen tool and stored as `.tlvl` binary files
- Each level awards exactly 1 cat (cat ID = level number - 1)

### 3.2 Difficulty Curve (6 Tiers)

| Tier | Levels | Grid | Colors | Cats/Color | Blockers | Shapes | Min Moves | Special Mechanics |
|------|--------|------|--------|-----------|----------|--------|-----------|-------------------|
| Tutorial | 1-10 | 5x5-6x6 | 1-2 | 1-2 | 0 | Single, Domino | 3-5 | None |
| Easy | 11-25 | 6x6-7x7 | 2-3 | 1-2 | 0-1 | +L, T shapes | 5-8 | Static blockers |
| Medium | 26-45 | 7x7-8x8 | 3 | 2 | 1-2 | +I, S shapes | 8-12 | Blocker-cats (0-1) |
| Hard | 46-65 | 8x8-9x9 | 3-4 | 2-3 | 1-2 | All 8 shapes | 10-15 | Conditional shapes |
| Expert | 66-80 | 9x9-10x10 | 4-5 | 2-3 | 2-3 | All 8 shapes | 15-20 | Multiple conditionals |
| Master | 81-100 | 9x9-10x10 | 4-5 | 3-4 | 2-3 | All 8 shapes | 18+ | All mechanics combined |

**Level Generation CLI:** Levels are generated per-tier using the offline tool:
```bash
tilepuzzlelevelgen --count 10 --tier tutorial --output Levels/
tilepuzzlelevelgen --count 15 --tier easy --output Levels/
tilepuzzlelevelgen --count 20 --tier medium --output Levels/
tilepuzzlelevelgen --count 20 --tier hard --output Levels/
tilepuzzlelevelgen --count 15 --tier expert --output Levels/
tilepuzzlelevelgen --count 20 --tier master --output Levels/
```

**Runtime Validation:** On game boot, all 100 levels are loaded into RAM. Each level is validated via `Zenith_Assert` against its tier's expected parameter ranges (grid size, color count, blocker count, shape complexity). This ensures the offline tool and game expectations stay in sync.

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
| Milestone: 10 cats rescued (first time) | +50 |
| Milestone: 25 cats rescued (first time) | +100 |
| Milestone: 50 cats rescued (first time) | +200 |
| Milestone: 75 cats rescued (first time) | +300 |
| Milestone: 100 cats rescued (first time) | +500 |

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
- **Loss trigger:** Exiting a puzzle without completing it costs 1 life. Exception: exiting before making any moves does not cost a life.
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
- Uncollected cats display "?" placeholder

---

## 6. Save Data System

### 6.1 Save Format

- **Binary serialization** via `Zenith_DataStream`
- **Save version:** 7 (`uGAME_SAVE_VERSION`)
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
| `uWeeklyChallengeType` | uint32 | 0=levels, 1=stars, 2=cats, 3=perfect |
| `uWeeklyChallengeTarget` | uint32 | Target count (e.g., 5 levels) |
| `uWeeklyChallengeProgress` | uint32 | Current progress toward target |
| `uWeeklyChallengeReward` | uint32 | Coin reward on completion |
| `uWeeklyChallengeStartDate` | uint32 | YYYYMMDD of challenge start Monday |
| `bWeeklyChallengeCompleted` | bool | Whether current challenge is done |
| `uAchievementFlags` | uint16 | Bitfield for 10 achievements |
| `uLastDailyPinballDate` | uint32 | YYYYMMDD for daily pinball bonus |

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
- **Keyboard/Gamepad Navigation:** Arrow keys navigate vertically between menu buttons (Continue ↕ LevelSelect ↕ NewGame ↕ Pinball ↕ ResetSave ↕ CatCafe ↕ DailyPuzzle ↕ Settings ↕ Achievements). Enter/Space activates the focused button.

### 7.2.1 Main Menu Visual Style

The main menu establishes Paws & Pins' cozy visual identity. Every element uses
the engine's UIStyle system for consistent styling.

**Background:**
- Vertical gradient: dark navy `(0.06, 0.06, 0.12)` at top to dark indigo
  `(0.10, 0.06, 0.18)` at bottom
- No border or shadow (fills entire screen)

**Title Area:**
- Title text: "Paws & Pins", 84pt, white with text shadow (2px offset, black 50% alpha)
- Subtitle text: "A Cat Puzzle Game", 24pt, muted lavender `(0.6, 0.6, 0.8, 0.7)` with text shadow

**Button Styling (all buttons):**
- Corner radius: 12px
- Drop shadow: 3px offset, 2px spread, black 30% alpha
- Border: 2px thickness, lighter tint of fill color
- Transition duration: 0.12s smooth color blend between states
- Text shadow: 1px offset, black 40% alpha
- Font size: 32pt, white text

**Button Color Palette:**

| Button | Normal | Hover | Pressed | Border |
|--------|--------|-------|---------|--------|
| Continue | `(0.18, 0.30, 0.55)` | `(0.22, 0.36, 0.65)` | `(0.12, 0.22, 0.42)` | `(0.30, 0.45, 0.70)` |
| Level Select | `(0.18, 0.30, 0.55)` | `(0.22, 0.36, 0.65)` | `(0.12, 0.22, 0.42)` | `(0.30, 0.45, 0.70)` |
| Cat Cafe | `(0.45, 0.22, 0.35)` | `(0.55, 0.28, 0.42)` | `(0.35, 0.16, 0.28)` | `(0.60, 0.35, 0.50)` |
| Daily Puzzle | `(0.22, 0.38, 0.22)` | `(0.28, 0.48, 0.28)` | `(0.16, 0.28, 0.16)` | `(0.35, 0.55, 0.35)` |
| Pinball | `(0.18, 0.35, 0.40)` | `(0.22, 0.42, 0.48)` | `(0.12, 0.26, 0.30)` | `(0.30, 0.50, 0.55)` |
| Settings | `(0.22, 0.22, 0.28)` | `(0.30, 0.30, 0.38)` | `(0.15, 0.15, 0.20)` | `(0.35, 0.35, 0.42)` |
| New Game | `(0.20, 0.25, 0.40)` | `(0.28, 0.33, 0.50)` | `(0.14, 0.18, 0.30)` | `(0.32, 0.38, 0.55)` |
| Reset Save | `(0.45, 0.15, 0.15)` | `(0.55, 0.20, 0.20)` | `(0.35, 0.10, 0.10)` | `(0.60, 0.25, 0.25)` |
| Refill Lives | `(0.50, 0.20, 0.20)` | `(0.60, 0.30, 0.30)` | `(0.35, 0.12, 0.12)` | `(0.65, 0.32, 0.32)` |

**Button Layout:**
- Vertical stack with 24px spacing
- Width: 300px, height: 80px per button

**Corner HUD Displays:**
- Each display (coins, lives, streak, stars) sits inside a pill-shaped
  background rect:
  - Corner radius: 16px
  - Fill: dark translucent `(0.05, 0.05, 0.10, 0.6)`
  - Border: 1px, subtle `(0.2, 0.2, 0.3, 0.4)`
  - Padding: 8px horizontal, 4px vertical around content

**Version Text:**
- "v1.0" text at bottom center
- 20pt, very muted `(0.4, 0.4, 0.5, 0.4)`

**Level Select & Cat Cafe:**
- Same background gradient as main menu
- Level grid buttons: corner radius 8px, shadow enabled, blue palette
- Cat card background rects: corner radius 10px, shadow enabled
- Navigation buttons: corner radius 8px
- All titles get text shadow

### 7.3 Level Select

- **20 levels per page** (4x5 grid)
- Color coding:
  - Gold = 3-star completed
  - Purple = Pinball gate level (every 10th)
  - Green = Current level (pulsing brightness animation, sine wave at 3 Hz)
  - Blue = Unlocked, not yet completed
  - Dim gray = Locked
- **Star progress bar:** "Stars: X / 300" text displayed at top of level select screen
- **Star display:** Each level button shows star rating using Unicode star characters (★ filled, ☆ empty)

### 7.4 Gameplay HUD

- Level number
- Move counter
- Cats remaining counter
- Progress bar (UIRect with fill direction)
- Buttons: Reset, Undo, Hint, Skip (offered after 3 resets), Menu
- **Keyboard/Gamepad Navigation:** Arrow keys navigate horizontally between HUD buttons (Menu ↔ Reset ↔ Undo ↔ Hint ↔ Skip)
- **Confirmation dialogs:** Modal overlay with dimmed background (`Zenith_UIOverlay`), styled Cancel and Accept buttons with hover states and fade animation. Used for Exit Level and Skip Level confirmations.

### 7.5 Victory Overlay

Staggered reveal sequence (~2.5s total):

| Time | Element | Animation |
|------|---------|-----------|
| 0.0s | Background | Fades in (alpha 0 -> 0.9, 0.25s) |
| 0.3s | "Level Complete!" title | Scales up with back-out easing (0.3s) |
| 0.6s | Star 1 | Scale bounce from 0 to 48px (back-out, 0.3s) + gold particle burst |
| 1.0s | Star 2 | Same animation |
| 1.4s | Star 3 | Same animation (filled or empty based on rating) |
| 1.8s | "Cat rescued: [Name]!" | Fades in + slides up (0.3s) |
| 2.2s | "+N coins" counter | Animated count-up from 0 to earned amount (0.4s) |
| 2.5s | "Next Level" button | Fades in (0.25s) |

- Star images use `star_filled`/`star_empty` textures (not text asterisks)
- Each star appearance triggers a gold particle burst at grid center

**Victory Celebration Intensity (by star count):**
- **1 star:** Modest celebration — smaller title text, no confetti, single brief zoom pulse
- **2 stars:** Standard celebration — normal title, confetti burst (40 particles), zoom pulse
- **3 stars:** Full celebration — large title with glow, full confetti burst (80 particles), extended zoom pulse, bonus particle effects per star

**"New Best!" Indicator:**
When replaying a level and achieving a better star rating or fewer moves than the previous best, display a "New Best!" callout text (gold, pulsing) below the star display. Shows the improvement: "2 moves under par!" or "Improved: 3 stars!"

### 7.6 Input

**Mouse/Touch (Primary):** Tap to select shape (emissive glow highlight + breathing pulse), drag to move shape in dominant direction, up to 4 cells per drag update, auto-snap on release. All menus are touch/click navigated.

**Keyboard (Development Only):** Escape key available in `#ifdef ZENITH_TOOLS` builds for quick menu return. No keyboard gameplay controls — the game targets mobile touch input exclusively.

### 7.7 Settings Screen

Accessible from main menu via settings gear button. Contains:

| Setting | Type | Default | Persisted |
|---------|------|---------|-----------|
| Sound Effects | `Zenith_UIToggle` widget | On | Yes |
| Music | `Zenith_UIToggle` widget | On | Yes |
| Haptics | `Zenith_UIToggle` widget | On | Yes |
| Credits/About | Button | — | — |
| Privacy Policy | Link | — | — |
| Reset Progress | Button (with confirmation dialog) | — | — |

Toggle widgets display on/off state with distinct colors (green=on, gray=off) and fire a callback on state change, replacing the previous manual text/color swapping approach.

**Keyboard/Gamepad Navigation:** Arrow keys navigate vertically between settings controls (Sound ↕ Music ↕ Haptics ↕ Credits ↕ Back).

**Credits/About:** Displayed as a `Zenith_UIOverlay` with dimmed background. Shows game title, credits text, and tap-to-dismiss instruction.

**Privacy Policy:** Opens the privacy policy URL in the system browser. Required by Google Play.

**Reset Progress:** Shows a confirmation dialog ("Are you sure? This will delete all progress, coins, and cat collection. This cannot be undone.") with "Cancel" and "Reset" buttons. On confirm, clears save data and returns to main menu.

### 7.8 UI Engine Features

The game uses the Zenith UI system, a canvas-based anchor/pivot widget toolkit. Available widgets and features:

**Widgets:**
| Widget | Class | Description |
|--------|-------|-------------|
| Button | `Zenith_UIButton` | Click/tap button with normal/hover/pressed styles, icon+label support |
| Text | `Zenith_UIText` | Monospace text rendering with alignment and multi-line support |
| Rect | `Zenith_UIRect` | Styled rectangle with fill direction, corner radius, gradient |
| Image | `Zenith_UIImage` | Texture display with UV support |
| Toggle | `Zenith_UIToggle` | On/off switch widget with distinct on/off colors and callback |
| Overlay | `Zenith_UIOverlay` | Modal popup with dimmed background, fade animation, input blocking |
| ScrollView | `Zenith_UIScrollView` | Scrollable content area with clipping and inertia |
| LayoutGroup | `Zenith_UILayoutGroup` | Horizontal/vertical automatic child layout |

**Features:**
- **Tween animations:** `TweenAlpha`, `TweenPosition`, `TweenColor`, `TweenSize` with easing (Linear, EaseIn, EaseOut, EaseInOut) and delay support
- **Focus navigation:** Keyboard/gamepad navigation between focusable elements with explicit directional links
- **Sort order:** Elements render in configurable sort order (overlays use sort order 100+)
- **StretchAll anchor:** Elements automatically fill parent bounds
- **UIRect fill:** Progress bar support with fill direction and fill amount
- **Screen management:** Centralized `ShowScreen()` / `ShowScreenAdditive()` for managing screen visibility
- **InputSimulator:** `SimulateClickOnUIElement(name)` for automated UI testing

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
| Shape slide | 0.15s | Ease-out cubic (`1 - (1-t)^3`) |
| Shape bounce on arrival | 0.1s | Scale 1.08x -> 1.0x, cubic-out |
| Shape pickup | Instant | Scale bump to 1.1x on select |
| Shape breathing pulse | 0.8s period | ±5% scale oscillation (sine), loops while selected |
| Cat elimination pop | 0.28s | Scale up 1.3x (0.08s quad-out), then shrink to 0 (0.2s back-in) |
| Screen shake (blocked) | 0.15s | ±0.05 unit XZ offset, dampened decay |
| Camera zoom pulse (victory) | 0.6s | 5% zoom-in (0.2s quad-out), ease back (0.4s quad-in-out) |
| Locked shape wiggle | 0.325s | ±2 degree Y rotation oscillation, decaying |
| Shape unlock celebration | 0.2s | Scale 1.15x -> 1.0x, elastic-out + golden particles |
| Hint flash | ~0.1s | On/off blink cycle |
| Victory stars | 0.3s each | Scale bounce (back-out), staggered at 0.4s intervals |
| Gate celebration | 2.5s | Success/failure display |

### 8.6 Particle Effects

| Effect | Trigger | Description |
|--------|---------|-------------|
| Elimination sparkle | Cat eliminated | 25 particles, color-matched to cat color, burst upward |
| Shape unlock flash | Conditional shape unlocked | 15 golden particles at shape position |
| Victory star sparkle | Star revealed in victory overlay | 10 gold particles at grid center |
| Victory confetti | Level complete | 80 confetti particles from above |

### 8.4 Camera

- Perspective overhead view, centered on grid, looking down -Y axis
- Camera height auto-adjusted per grid size to fit all cells in view
- Screen shake offset (XZ plane) on blocked moves
- Zoom pulse (Y position) on level complete

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

### 10.3 Guided Onboarding (Target)

The current tutorial uses text overlays. The target onboarding experience uses contextual visual guidance:

- **Ghost hand animation:** An animated hand icon that points at the shape to drag, then traces the correct direction. Used for the first 3 levels.
- **Target cat highlight:** When the player picks up a shape on levels 1-5, all matching-color cats pulse with an emissive highlight (1.5x scale oscillation over 0.6s, color-matched). Highlight clears when shape is deselected.
- **Progressive button disclosure:** Undo button appears at level 3, Hint button at level 5, Skip button at level 10. Before these thresholds, the buttons are hidden entirely (not just disabled).
- **Stuck detection:** After 45 seconds of no moves on levels 1-10, a pulsing "Need a hint?" prompt appears. If the player still hasn't moved after 90 seconds, the free hint auto-triggers.

### 10.4 Hint System as Safety Net

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

### 11.2 Weekly Challenges

Time-limited goals that give players a medium-term engagement target beyond daily puzzles:

| Challenge Type | Example | Reward | Duration |
|---------------|---------|--------|----------|
| Level Completion | "Complete 5 levels this week" | 50 coins | 7 days |
| Star Collection | "Earn 10 stars this week" | 75 coins | 7 days |
| Cat Rescue | "Rescue 5 cats this week" | 100 coins | 7 days |
| Perfect Clear | "Get 3 stars on 3 levels" | 50 coins | 7 days |

- One active challenge at a time, refreshes every Monday at midnight UTC
- Challenge type deterministically selected from week number (YYYYMMDD / 7), cycling through the 4 types
- Challenge progress persisted in save data (type, target, progress, reward, start date, completed flag)
- UI: challenge banner on main menu showing challenge description, progress bar, and coin reward
- On completion: coins awarded automatically, "Challenge Complete!" toast displayed
- On new week: previous challenge replaced with new one, progress reset

### 11.3 Achievement System

10 persistent achievements stored as a `uint16_t` bitfield in save data. Each achievement unlocks once and persists permanently.

| ID | Name | Condition | Check Trigger |
|----|------|-----------|---------------|
| 0 | First Steps | Complete level 1 | OnLevelCompleted |
| 1 | Getting Started | Complete 10 levels | OnLevelCompleted |
| 2 | Halfway There | Complete 50 levels | OnLevelCompleted |
| 3 | Cat Master | Complete all 100 levels | OnLevelCompleted |
| 4 | Perfect Puzzle | 3-star any level | OnLevelCompleted |
| 5 | Speed Solver | 3-star 10 levels | OnLevelCompleted |
| 6 | Cat Lover | Collect 10 cats | OnLevelCompleted |
| 7 | Cat Collector | Collect 50 cats | OnLevelCompleted |
| 8 | Daily Regular | 7-day daily streak | OnDailyPuzzleCompleted |
| 9 | Pinball Pro | Clear 3 pinball gates | OnPinballGateCleared |

**Toast UI:** Gold banner at top of screen showing achievement name, auto-dismisses after 2 seconds.

**Achievement Screen:** Accessible from main menu via "Achievements" button. Uses `TILEPUZZLE_STATE_ACHIEVEMENTS` game state. Canvas-rendered list of 10 achievements showing name, description, and locked/unlocked state. Unlocked achievements use a gold background; locked achievements use a dark background. "Tap to return" prompt at the bottom dismisses the screen.

### 11.4 Future Considerations

- **Seasonal Events:** Holiday-themed cat skins and level backgrounds
- **Leaderboards:** Best moves/time per level (requires online infrastructure)

---

## 12. Platform Requirements

### 12.1 Android Launch Checklist

| Requirement | Status |
|------------|--------|
| Vulkan renderer | Implemented |
| Touch input | Implemented (touch-to-mouse emulation) |
| Pre-compiled shaders (.spv) | Pipeline exists (FluxCompiler) |
| Asset pipeline (.zmesh, .zmat, .ztex) | Implemented |
| Portrait orientation (720x1280) | Implemented |
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
  SETTINGS (8)
  ACHIEVEMENTS (9)

ConfirmDialogType:
  CONFIRM_RESET_SAVE (0)
  CONFIRM_EXIT_LEVEL (1)
  CONFIRM_SKIP_LEVEL (2)

PinballState:
  PINBALL_STATE_GATE_SELECT -- gate selection UI for Quick Play
  PINBALL_STATE_READY       -- waiting to launch
  PINBALL_STATE_LAUNCHING   -- plunger released
  PINBALL_STATE_PLAYING     -- ball in play
  PINBALL_STATE_BALL_LOST   -- ball drained
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

### Phase 1: Game Feel & Juice (COMPLETED)
1. ~~Fix shape mesh generation bugs + remove bottom faces~~ Done
2. ~~Animation easing & timing (ease-out cubic, bounce, cat pop, breathing pulse)~~ Done
3. ~~Remove keyboard input & floor cursor~~ Done
4. ~~Screen shake & emphasis (blocked shake, victory zoom, locked wiggle)~~ Done
5. ~~Particle effect enhancements (color-matched, unlock particles)~~ Done
6. ~~Victory overlay redesign (staggered reveal sequence)~~ Done

### Phase 2: Onboarding & Tutorial (COMPLETED)
1. ~~Contextual tutorial overlays (levels 1, 6, 11, 26, 46)~~ Done
2. ~~First-time user experience (progressive menu disclosure)~~ Done

### Phase 3: Visual Polish (COMPLETED)
1. ~~Per-color material variation~~ Done
2. ~~Background gradient per difficulty tier~~ Done
3. ~~Screen transitions between menus~~ Done

### Phase 4: Meta-Game Polish (COMPLETED)
1. ~~Cat Cafe collection system~~ Done
2. ~~Level select visual upgrade (star icons, lock icons, color coding)~~ Done
3. ~~Settings screen (sound, music, haptics toggles)~~ Done
4. ~~Victory overlay redesign~~ Done
5. ~~Milestone coin bonuses~~ Done

### Phase 5: Difficulty & Content
1. Add `--tier` argument to TilePuzzleLevelGen for tier-aware generation
2. Re-generate 100 levels with proper 6-tier difficulty curve
3. Add runtime level validation (Zenith_Assert per-tier checks at boot)
4. Implement lives gate (enforce lives deduction on level entry)
5. Verify daily puzzle generation and caching

### Phase 6: Retention & Engagement (IN PROGRESS)
1. Weekly challenge system (4 challenge types, save v7, main menu banner UI)
2. Achievement system (10 achievements, bitfield save, toast UI, dedicated screen)
3. Victory celebration scaling by star count (confetti: 0/40/80 particles)
4. "New Best!" pulsing gold animation in victory overlay
5. Target cat highlighting on levels 1-5
6. Stuck detection (45s prompt, 90s auto-hint on levels 1-10)
7. ~~Confirmation dialogs (reset save, exit level, skip level)~~ Done — uses `Zenith_UIOverlay`
8. ~~Credits/About screen in settings~~ Done — uses `Zenith_UIOverlay`
9. No life loss on zero-move exit
10. Level select UX polish (time-based pulse, star counter, Unicode stars)
11. Cat Cafe visual upgrade (tier borders, progress bar)
12. ~~Pinball gate selection UI for Quick Play~~ Done — widget-based buttons
13. Daily pinball bonus (25 coins/day)
14. Pinball thematic elements (cat-themed materials)
15. ~~Settings toggles~~ Done — uses `Zenith_UIToggle` widgets
16. ~~Screen management~~ Done — centralized `ShowScreen()` / `ShowScreenAdditive()`
17. ~~Focus/keyboard navigation~~ Done — all screens wired with directional nav links
18. ~~StretchAll backgrounds~~ Done — menu/settings/level-select/cat-cafe backgrounds use StretchAll anchor

### Phase 7: Visual Upgrade
1. Cat mesh upgrade (replace sphere with low-poly cat model)
2. PBR material polish (roughness/metallic per shape color)
3. Splash screen / logo

### Phase 8: Store Release
1. Create store listing assets (icon, feature graphic, screenshots)
2. Privacy policy creation
3. Content rating questionnaire
4. APK size optimization

---

## Appendix D: Test Plan

> **Coverage Summary:** Tests marked `[AUTO]` in sections D.6-D.26 are exercised by the automated test suite in `TilePuzzle_AutoTest.h`. Key automated UI tests:
> - **Test_UIAllMenuElements** — verifies existence and type of all ~60 named UI elements across all screens
> - **Test_UIAllScreenElements** — verifies ShowScreen/ShowScreenAdditive visibility logic for each screen
> - **Test_UIConfirmDialogFlow** — programmatically tests all 3 confirm dialog types (show/hide/text)
> - **Test_UILevelSelectPagination** — verifies page text updates on page changes
> - **Test_UIEconomyDisplay** — verifies coin/lives/stars text matches save data, tests economy updates
> - **Test_UIInteractionWalkthrough** — InputSimulator-driven button clicks for screen navigation and toggle interaction (16 steps)
>
> Sections without `[AUTO]` markers require manual testing (visual styling, animations, device-specific, gameplay interactions).

### D.1 Existing Automated Tests

These tests are implemented in `Games/TilePuzzle/Tests/TilePuzzle_AutoTest.h` and run via the `--autotest` flag.

| Test | Coverage |
|------|----------|
| `Test_PuzzleLevel_StarRating` | Star calculation: 3-star at par, 2-star at par+2, 1-star at par+3 |
| `Test_PuzzleLevel_Undo` | Undo restores shape positions, cat states, and move count |
| `Test_PuzzleLevel_Hint` | BFS solver returns valid next move from current state |
| `Test_Pinball_LaunchAndScore` | Ball launches, pegs score correctly, ball drains |
| `Test_Pinball_GateObjectives` | All 10 gate types verify correctly |
| `Test_SaveLoad_Integrity` | Save -> load round-trip preserves all fields |
| `Test_CoinSystem` | Earning, spending, balance, coin sinks |
| `Test_UIStretchAll` | StretchAll anchor fills parent bounds |
| `Test_UITextMetrics` | Monospace text width/height calculation |
| `Test_UISortOrder` | Element sort order affects render order |
| `Test_UITweenSystem` | Property tweens with easing and delay |
| `Test_UIToggle` | Toggle widget on/off state and callback |
| `Test_UIButtonIcon` | Button icon placement and sizing |
| `Test_UIOverlay` | Overlay show/hide and dim background |
| `Test_UIFocusNavigation` | Directional navigation between elements |
| `Test_UIScrollView` | Scroll position, clamping, content size |
| `Test_UICanvasClipRect` | Clip rect push/pop stack |
| `Test_UIInputSimulatorClick` | Click UI element by name |
| `Test_UICachedPointers` | Cached FindElement pointers valid |
| `Test_UIRectFillAmount` | Rect fill direction and amount |
| `Test_UISettingsToggles` | Settings use toggle widgets correctly |
| `Test_UIScreenManagement` | ShowScreen/ShowScreenAdditive |
| `Test_UIConfirmOverlay` | Confirm/credits overlays exist and configured |
| `Test_UIMenuFocusNavigation` | Game-level nav links wired correctly |
| `Test_UIStretchAllBackgrounds` | Background rects use StretchAll |
| `Test_UIAllMenuElements` | All ~60 named UI elements exist with correct types (covers D.6-D.13) |
| `Test_UIAllScreenElements` | ShowScreen/ShowScreenAdditive visibility per screen (covers D.17) |
| `Test_UIConfirmDialogFlow` | All 3 confirm dialog types show/hide/text correctly (covers D.9) |
| `Test_UILevelSelectPagination` | Page text updates on page change (covers D.7) |
| `Test_UIEconomyDisplay` | Coin/lives/stars text matches save data (covers D.21) |
| `Test_UIInteractionWalkthrough` | InputSimulator clicks through Menu↔Settings↔LevelSelect↔CatCafe↔Achievements + toggle (covers D.17) |
| `Test_FullGame` | Visual playthrough of all 100 levels + 10 pinball gates |

### D.2 Manual Test Checklist: Game Feel

| ID | What to Verify | Pass Criteria |
|----|---------------|---------------|
| M-FEEL-01 | Shape slide uses ease-out | Shape decelerates on arrival, no constant speed |
| M-FEEL-02 | Shape bounce on arrival | Visible overshoot + spring-back after slide |
| M-FEEL-03 | Cat elimination pop | Cat scales up 1.3x then shrinks to 0 (not just fade) |
| M-FEEL-04 | Selected shape breathing pulse | Shape oscillates ±5% scale while selected |
| M-FEEL-05 | Screen shake on blocked move | Camera shakes when shape can't move |
| M-FEEL-06 | Locked shape wiggle | Conditional shape wiggles when tapped before unlocked |
| M-FEEL-07 | Color-matched elimination particles | Particle color matches eliminated cat |
| M-FEEL-08 | Touch-only input | All gameplay works via tap + drag, no keyboard needed |

### D.3 Manual Test Checklist: Victory & Rewards

| ID | What to Verify | Pass Criteria |
|----|---------------|---------------|
| M-VIC-01 | Staggered victory reveal | Elements appear sequentially, not all at once |
| M-VIC-02 | Star scale animation | Each star scales from 0 with bounce easing |
| M-VIC-03 | Coin count-up animation | Coin display ticks up from 0, not instant |
| M-VIC-04 | Camera zoom pulse on win | Camera briefly zooms in 5% then eases back |
| M-VIC-05 | Victory confetti particles | Confetti bursts from above on level complete |

### D.4 Regression Test Protocol

After any code change:
1. Run full automated test suite via `--autotest` flag
2. Build both Debug and Release: `msbuild zenith_win64.sln /p:Configuration=vs2022_Debug_Win64_True /p:Platform=x64`
3. Play levels 1, 25, 50, 75, 100 manually for visual correctness
4. Test save data migration: load previous version save, verify no data loss

### D.5 Performance Benchmarks

| Metric | Target |
|--------|--------|
| Puzzle frame rate | 60 FPS stable |
| Pinball frame rate | 60 FPS stable |
| Level load time | < 0.5s |
| Cold start time | < 3s |
| Memory usage | < 150 MB |
| APK size | < 100 MB |

### D.6 Test Checklist: Main Menu UI

> **Automation note:** `Test_UIAllMenuElements` verifies existence and type of all menu elements. `Test_UIEconomyDisplay` verifies coin/lives/stars text matches save data. `UpdateUIInteract` tests button click navigation via InputSimulator. Tests marked `[AUTO]` are covered by the autotest suite; unmarked tests require manual verification.

| ID | What to Verify | Pass Criteria |
|----|---------------|---------------|
| M-MENU-01 | `[AUTO]` Menu background exists | MenuBackground element exists and is correct type |
| M-MENU-02 | Title text displays | "Paws & Pins" at 84pt, white, centered horizontally with text shadow |
| M-MENU-03 | Subtitle text displays | "A Cat Puzzle Game" at 24pt, muted lavender, positioned below title with text shadow |
| M-MENU-04 | `[AUTO]` Continue button exists | ContinueButton element exists with Button type |
| M-MENU-05 | Continue button hover | Button transitions to hover color on mouse-over (0.12s blend) |
| M-MENU-06 | Continue button press | Button transitions to pressed color on click/tap (darker shade) |
| M-MENU-07 | Continue button navigates | Tapping Continue starts gameplay at current level |
| M-MENU-08 | `[AUTO]` Level Select button exists | LevelSelectButton element exists with Button type |
| M-MENU-09 | `[AUTO]` Level Select button navigates | Tapping Level Select transitions to level select screen (InputSimulator) |
| M-MENU-10 | `[AUTO]` Cat Cafe button exists | CatCafeButton element exists with Button type |
| M-MENU-11 | `[AUTO]` Cat Cafe button navigates | Tapping Cat Cafe transitions to Cat Cafe screen (InputSimulator) |
| M-MENU-12 | `[AUTO]` Daily Puzzle button exists | DailyPuzzleButton element exists with Button type |
| M-MENU-13 | Daily Puzzle button navigates | Tapping Daily Puzzle starts daily challenge level |
| M-MENU-14 | `[AUTO]` Pinball button exists | PinballButton element exists with Button type |
| M-MENU-15 | Pinball button navigates | Tapping Pinball transitions to pinball gate select |
| M-MENU-16 | `[AUTO]` Settings button exists | SettingsButton element exists with Button type |
| M-MENU-17 | `[AUTO]` Settings button navigates | Tapping Settings transitions to settings screen (InputSimulator) |
| M-MENU-18 | `[AUTO]` New Game button exists | NewGameButton element exists with Button type |
| M-MENU-19 | New Game button triggers confirm | Tapping New Game shows confirmation dialog |
| M-MENU-20 | `[AUTO]` Reset Save button exists | ResetSaveButton element exists with Button type |
| M-MENU-21 | Reset Save button triggers confirm | Tapping Reset Save shows confirmation dialog |
| M-MENU-22 | `[AUTO]` Refill Lives button exists | RefillLivesButton element exists with Button type |
| M-MENU-23 | Refill Lives button works | Tapping Refill Lives deducts 50 coins and restores 5 lives |
| M-MENU-24 | Refill Lives hidden at full lives | Button hidden or disabled when lives == 5 |
| M-MENU-25 | `[AUTO]` Achievements button exists & navigates | AchievementsButton element exists; InputSimulator verifies navigation |
| M-MENU-26 | `[AUTO]` Coin display shows balance | CoinText shows correct coin count matching save data |
| M-MENU-27 | `[AUTO]` Coin icon exists | CoinIcon element exists |
| M-MENU-28 | `[AUTO]` Lives display shows count | LivesText shows current lives count matching save data |
| M-MENU-29 | `[AUTO]` Heart icon exists | HeartIcon element exists |
| M-MENU-30 | Lives timer visible when < 5 | LivesTimerText shows countdown "MM:SS" when lives < 5 |
| M-MENU-31 | `[AUTO]` Lives timer hidden at full | LivesTimerText hidden when lives == 5 |
| M-MENU-32 | `[AUTO]` Daily streak group exists | StreakGroup and StreakText elements exist |
| M-MENU-33 | `[AUTO]` Total stars displays | TotalStarsText shows total star count matching save data |
| M-MENU-34 | `[AUTO]` Version text exists | VersionText element exists with Text type |
| M-MENU-35 | Button layout vertical stack | All buttons stacked vertically with 24px spacing |
| M-MENU-36 | Button corner radius | All buttons have 12px corner radius |
| M-MENU-37 | Button drop shadows | All buttons have visible drop shadow (3px offset) |
| M-MENU-38 | Button borders | All buttons have 2px border in lighter tint |
| M-MENU-39 | Button text shadow | All button text has 1px shadow at 40% alpha |
| M-MENU-40 | Corner HUD pill backgrounds | Coin/lives/streak/stars displays have pill-shaped background rect (16px radius, translucent) |
| M-MENU-41 | All buttons touch-responsive | Every button responds to tap/click without keyboard |

### D.7 Test Checklist: Level Select UI

> **Automation note:** `Test_UIAllMenuElements` verifies element existence. `Test_UILevelSelectPagination` verifies page text updates. `UpdateUIInteract` tests Level Select button click navigation. Tests marked `[AUTO]` are covered by the autotest suite.

| ID | What to Verify | Pass Criteria |
|----|---------------|---------------|
| M-LSEL-01 | `[AUTO]` Level select background exists | LevelSelectBg element exists |
| M-LSEL-02 | `[AUTO]` Title exists | LevelSelectTitle element exists |
| M-LSEL-03 | 4x5 grid layout | 20 level buttons arranged in 4 columns by 5 rows |
| M-LSEL-04 | `[AUTO]` Page text shows correct page | PageText displays "Page X / Y" and updates on page change |
| M-LSEL-05 | `[AUTO]` Previous page button exists | PrevPageButton element exists |
| M-LSEL-06 | `[AUTO]` Next page button exists | NextPageButton element exists |
| M-LSEL-07 | `[AUTO]` Page wrapping | Pagination stays within valid page range |
| M-LSEL-08 | `[AUTO]` Back button returns to menu | BackButton navigates back to main menu (InputSimulator) |
| M-LSEL-09 | Gold 3-star level buttons | Completed levels with 3 stars show gold color |
| M-LSEL-10 | Purple pinball gate levels | Every 10th level (10,20,...,100) shows purple tint |
| M-LSEL-11 | Green current level | Current level button has pulsing green brightness (sine wave, 3 Hz) |
| M-LSEL-12 | Blue unlocked levels | Unlocked but incomplete levels show blue |
| M-LSEL-13 | Gray locked levels | Levels beyond highest reached show dim gray, not clickable |
| M-LSEL-14 | Star display on buttons | Each level button shows star rating (filled/empty star characters) |
| M-LSEL-15 | Star progress bar | "Stars: X / 300" text displayed at top of level select |
| M-LSEL-16 | Tap level starts game | Tapping an unlocked level button loads and starts that level |
| M-LSEL-17 | Tap locked level blocked | Tapping a locked level does nothing |
| M-LSEL-18 | Level number on buttons | Each button shows its level number (1-100) |
| M-LSEL-19 | Button corner radius | Level grid buttons have 8px corner radius |
| M-LSEL-20 | Button shadows | Level grid buttons have shadow enabled |
| M-LSEL-21 | Current level pulse animation | Pulsing brightness animation is smooth, continuous, and visible |
| M-LSEL-22 | Correct page for current level | Opening level select auto-navigates to the page containing current level |

### D.8 Manual Test Checklist: Gameplay HUD (no automation — HUD elements are on gameplay scene)

| ID | What to Verify | Pass Criteria |
|----|---------------|---------------|
| M-HUD-01 | Level number displays | LevelText shows "Level N" matching current level |
| M-HUD-02 | Move counter displays | MovesText shows current move count, starts at 0 |
| M-HUD-03 | Move counter increments | MovesText increments by 1 after each shape slide |
| M-HUD-04 | Cats remaining displays | CatsText shows number of cats remaining to eliminate |
| M-HUD-05 | Cats counter decrements | CatsText decrements when a cat is eliminated |
| M-HUD-06 | Coin display in HUD | HUDCoinsText shows current coin balance |
| M-HUD-07 | Reset button visible | ResetBtn displayed and labeled |
| M-HUD-08 | Reset button works | Tapping Reset restores level to initial state, resets moves to 0 |
| M-HUD-09 | Undo button visible | UndoBtn displayed and labeled |
| M-HUD-10 | Undo button works (free first) | First undo per level is free, restores previous state |
| M-HUD-11 | Undo button costs coins (2nd+) | Second+ undo deducts 20 coins |
| M-HUD-12 | Undo with no coins | Undo blocked or shows insufficient coins if balance < 20 (after first free) |
| M-HUD-13 | Undo with no moves | Undo disabled/no-op when no moves have been made |
| M-HUD-14 | Hint button visible | HintBtn displayed and labeled |
| M-HUD-15 | Hint button works (free first) | First hint per level is free, highlights shape + direction |
| M-HUD-16 | Hint button costs coins (2nd+) | Second+ hint deducts 30 coins |
| M-HUD-17 | Hint visual | Hint highlights a shape with blink animation indicating direction |
| M-HUD-18 | Hint clears on move | Hint highlight clears when player makes any move |
| M-HUD-19 | Skip button hidden initially | SkipBtn not visible until 3 resets on current level |
| M-HUD-20 | Skip button appears after 3 resets | SkipBtn becomes visible after 3 resets on the same level |
| M-HUD-21 | Skip button triggers confirm | Tapping Skip shows confirmation dialog for skip (100 coins) |
| M-HUD-22 | Menu button visible | MenuBtn displayed and labeled |
| M-HUD-23 | Menu button triggers confirm | Tapping Menu shows confirmation dialog (exit level, lose life) |
| M-HUD-24 | Menu confirm dialog text | Confirm dialog states player will lose a life if they exit |
| M-HUD-25 | Menu confirm cancel | Cancel button dismisses dialog, returns to gameplay |
| M-HUD-26 | Menu confirm accept | Accept button returns to main menu, deducts 1 life |
| M-HUD-27 | No life loss on 0 moves | Exiting before making any moves does NOT cost a life |
| M-HUD-28 | HUD button layout | Buttons arranged horizontally in HUDButtonGroup |
| M-HUD-29 | All HUD buttons touch-responsive | Every HUD button responds to tap without keyboard |
| M-HUD-30 | Progress bar background | CatProgressBg visible as dark background bar |
| M-HUD-31 | Progress bar fill | CatProgressFill shows progress based on cats eliminated vs total |
| M-HUD-32 | Progress bar updates | Fill amount updates in real-time as cats are eliminated |

### D.9 Test Checklist: Confirmation Dialogs

> **Automation note:** `Test_UIConfirmDialogFlow` programmatically tests all 3 dialog types (EXIT_LEVEL, SKIP_LEVEL, RESET_SAVE) — show/hide state, text content, and multiple sequential triggers. Tests marked `[AUTO]` are covered by the autotest suite.

| ID | What to Verify | Pass Criteria |
|----|---------------|---------------|
| M-CONF-01 | `[AUTO]` Overlay appears on trigger | ConfirmOverlay becomes visible (IsShowing) when ShowConfirmDialog called |
| M-CONF-02 | Dim background renders | Semi-transparent dark background covers full screen behind overlay |
| M-CONF-03 | Dim background fades in | Background alpha animates from 0 to 0.7 (not instant) |
| M-CONF-04 | Content container visible | Centered content container (400x200) visible on top of dim |
| M-CONF-05 | `[AUTO]` Confirm text displays | ConfirmText populated with non-empty text for each dialog type |
| M-CONF-06 | `[AUTO]` Exit level text correct | ConfirmText contains expected text for CONFIRM_EXIT_LEVEL |
| M-CONF-07 | `[AUTO]` Skip level text correct | ConfirmText contains expected text for CONFIRM_SKIP_LEVEL |
| M-CONF-08 | `[AUTO]` Reset save text correct | ConfirmText contains expected text for CONFIRM_RESET_SAVE |
| M-CONF-09 | `[AUTO]` Cancel button exists | ConfirmCancelBtn element exists |
| M-CONF-10 | `[AUTO]` Cancel/hide works | HideConfirmDialog sets IsShowing to false |
| M-CONF-11 | `[AUTO]` Accept button exists | ConfirmAcceptBtn element exists with context text |
| M-CONF-12 | Accept button works (exit) | Accept on exit level: returns to menu, deducts life |
| M-CONF-13 | Accept button works (skip) | Accept on skip: advances to next level, deducts 100 coins |
| M-CONF-14 | Accept button works (reset) | Accept on reset save: clears all data, returns to menu |
| M-CONF-15 | Overlay blocks input behind | Cannot tap buttons behind the overlay while it is showing |
| M-CONF-16 | Overlay dim fade out | Dim background fades out on dismiss (not instant) |
| M-CONF-17 | Sort order renders on top | Overlay renders above all other UI elements (sort order 100+) |
| M-CONF-18 | Text behind overlay occluded | Text elements behind the overlay content box are clipped by fragment shader |
| M-CONF-19 | Cancel button hover state | Cancel button shows hover color on mouse-over |
| M-CONF-20 | Accept button hover state | Accept button shows hover/pressed color on interaction |
| M-CONF-21 | Overlay exists in gameplay scene | ConfirmOverlay is created in the TilePuzzle gameplay scene (not just main menu) |
| M-CONF-22 | `[AUTO]` Multiple confirms work | Show/hide/show cycle works without issues for all dialog types |

### D.10 Manual Test Checklist: Victory Overlay (partial auto — ShowScreenAdditive tested in D.17)

| ID | What to Verify | Pass Criteria |
|----|---------------|---------------|
| M-VIC-06 | Victory overlay appears | VictoryBg fades in after level complete |
| M-VIC-07 | "Level Complete!" title | VictoryTitle displays with scale-up animation |
| M-VIC-08 | Star 1 animation | VictoryStar0 appears at 0.6s with scale bounce (back-out easing) |
| M-VIC-09 | Star 2 animation | VictoryStar1 appears at 1.0s with same animation |
| M-VIC-10 | Star 3 animation | VictoryStar2 appears at 1.4s (filled or empty based on rating) |
| M-VIC-11 | 1-star appearance | Only VictoryStar0 filled, stars 1 and 2 empty |
| M-VIC-12 | 2-star appearance | VictoryStar0 and VictoryStar1 filled, star 2 empty |
| M-VIC-13 | 3-star appearance | All three stars filled |
| M-VIC-14 | Cat rescue text | VictoryCatText shows "Cat rescued: [Name]!" with correct cat name |
| M-VIC-15 | Coin count-up | VictoryCoinsText animates counting up from 0 to earned coins |
| M-VIC-16 | Next Level button appears | NextLevelBtn fades in at ~2.5s after victory start |
| M-VIC-17 | Next Level button works | Tapping NextLevelBtn loads and starts the next level |
| M-VIC-18 | Gold particle burst per star | Each star appearance triggers gold particles at grid center |
| M-VIC-19 | Confetti by star count | 1-star: no confetti, 2-star: 40 particles, 3-star: 80 particles |
| M-VIC-20 | "New Best!" indicator | Shows gold pulsing text when beating previous best stars/moves |
| M-VIC-21 | "New Best!" not shown on first play | No "New Best!" on first completion of a level |
| M-VIC-22 | Camera zoom pulse | Camera zooms in 5% then eases back during victory |
| M-VIC-23 | Stagger timing correct | Elements appear at documented intervals (0.0, 0.3, 0.6, 1.0, 1.4, 1.8, 2.2, 2.5s) |
| M-VIC-24 | Victory on last level (100) | Victory works correctly on level 100 without crash |

### D.11 Test Checklist: Settings Screen

> **Automation note:** `Test_UIAllMenuElements` verifies all settings elements exist. `UpdateUIInteract` tests navigation to/from settings and clicking the sound toggle via InputSimulator (steps 0-3, 14-16). Tests marked `[AUTO]` are covered by the autotest suite.

| ID | What to Verify | Pass Criteria |
|----|---------------|---------------|
| M-SET-01 | `[AUTO]` Settings background exists | SettingsBg element exists |
| M-SET-02 | `[AUTO]` Settings title exists | SettingsTitle element exists |
| M-SET-03 | `[AUTO]` Sound toggle exists | SettingsSoundBtn element exists |
| M-SET-04 | Sound toggle on state | Green color when sound is on |
| M-SET-05 | Sound toggle off state | Gray color when sound is off |
| M-SET-06 | `[AUTO]` Sound toggle works | InputSimulator clicks toggle, verifies IsOn() flips |
| M-SET-07 | Sound toggle persists | Sound preference saved and restored across sessions |
| M-SET-08 | `[AUTO]` Music toggle exists | SettingsMusicBtn element exists |
| M-SET-09 | Music toggle on/off colors | Green=on, gray=off |
| M-SET-10 | Music toggle works | Tapping toggles music on/off |
| M-SET-11 | Music toggle persists | Music preference saved and restored |
| M-SET-12 | `[AUTO]` Haptics toggle exists | SettingsHapticsBtn element exists |
| M-SET-13 | Haptics toggle on/off colors | Green=on, gray=off |
| M-SET-14 | Haptics toggle works | Tapping toggles haptics on/off |
| M-SET-15 | Haptics toggle persists | Haptics preference saved and restored |
| M-SET-16 | `[AUTO]` Credits button exists | SettingsCreditsBtn element exists |
| M-SET-17 | Credits button opens overlay | Tapping Credits opens credits overlay |
| M-SET-18 | `[AUTO]` Back button exists | SettingsBackBtn element exists |
| M-SET-19 | `[AUTO]` Back button returns to menu | InputSimulator clicks back, verifies menu visible |
| M-SET-20 | Toggle initial state matches save | On opening settings, toggle states match saved preferences |
| M-SET-21 | All settings touch-responsive | Every toggle and button responds to tap |

### D.12 Test Checklist: Credits Overlay

> **Automation note:** `Test_UIAllMenuElements` verifies CreditsOverlay, CreditsTitleText, CreditsLine1, CreditsLine2, CreditsDismissText elements exist. Tests marked `[AUTO]` are covered by the autotest suite.

| ID | What to Verify | Pass Criteria |
|----|---------------|---------------|
| M-CRED-01 | `[AUTO]` Credits overlay exists | CreditsOverlay element exists |
| M-CRED-02 | Dim background | Semi-transparent dim background covers screen |
| M-CRED-03 | `[AUTO]` Credits title exists | CreditsTitleText element exists |
| M-CRED-04 | `[AUTO]` Credits content lines exist | CreditsLine1 and CreditsLine2 elements exist |
| M-CRED-05 | `[AUTO]` Dismiss instruction exists | CreditsDismissText element exists |
| M-CRED-06 | Tap to dismiss works | Tapping anywhere on overlay dismisses it |
| M-CRED-07 | Returns to settings | After dismiss, settings screen is visible and interactive |
| M-CRED-08 | Input blocked behind | Cannot interact with settings while credits shown |

### D.13 Test Checklist: Cat Cafe UI

> **Automation note:** `Test_UIAllMenuElements` verifies all cat cafe elements exist. `UpdateUIInteract` tests navigation to/from Cat Cafe via InputSimulator (steps 8-11). Tests marked `[AUTO]` are covered by the autotest suite.

| ID | What to Verify | Pass Criteria |
|----|---------------|---------------|
| M-CAFE-01 | `[AUTO]` Cat Cafe background exists | CatCafeBg element exists |
| M-CAFE-02 | `[AUTO]` Cat Cafe title exists | CatCafeTitle element exists |
| M-CAFE-03 | `[AUTO]` Cat count display exists | CatCafeCount element exists |
| M-CAFE-04 | 8 cards per page | Up to 8 cat cards visible per page |
| M-CAFE-05 | `[AUTO]` Previous page button exists | CatCafePrevPage element exists |
| M-CAFE-06 | `[AUTO]` Next page button exists | CatCafeNextPage element exists |
| M-CAFE-07 | Page bounds | Cannot navigate before page 1 or after page 13 |
| M-CAFE-08 | `[AUTO]` Back button returns to menu | CatCafeBackButton navigates to main menu (InputSimulator) |
| M-CAFE-09 | Collected cat card shows name | Cat card for collected cat shows cat name (e.g., "Whiskers") |
| M-CAFE-10 | Collected cat card shows breed | Cat card shows breed name (e.g., "Tabby") |
| M-CAFE-11 | Collected cat card shows level | Cat card shows level number where cat was rescued |
| M-CAFE-12 | Collected cat card shows stars | Cat card shows 3-star indicator for that level |
| M-CAFE-13 | Uncollected cat placeholder | Uncollected cats show "???" placeholder |
| M-CAFE-14 | Cat card backgrounds | CatCardBg_N rects have 10px corner radius and shadow |
| M-CAFE-15 | Correct cats on correct pages | Page 1 shows cats 0-7, page 2 shows cats 8-15, etc. |
| M-CAFE-16 | All cards touch-responsive | Cards don't require keyboard to view |

### D.14 Test Checklist: Achievements Screen

> **Automation note:** `UpdateUIInteract` (steps 12-13) tests navigating to achievements via InputSimulator and verifying game state changes. Tests marked `[AUTO]` are covered by the autotest suite.

| ID | What to Verify | Pass Criteria |
|----|---------------|---------------|
| M-ACH-01 | `[AUTO]` Achievements accessible | InputSimulator clicks AchievementsButton, verifies state == ACHIEVEMENTS |
| M-ACH-02 | All 10 achievements listed | All 10 achievements visible (scrollable if needed) |
| M-ACH-03 | Achievement names correct | Each achievement shows correct name (First Steps, Getting Started, etc.) |
| M-ACH-04 | Achievement descriptions correct | Each shows condition text |
| M-ACH-05 | Unlocked achievements gold border | Unlocked achievements have gold border visual |
| M-ACH-06 | Locked achievements dimmed | Locked achievements show as locked/dimmed |
| M-ACH-07 | Achievement state matches save | Unlocked states match uAchievementFlags in save data |
| M-ACH-08 | Back button works | Can return to main menu from achievements |
| M-ACH-09 | Achievement toast appears | When unlocking an achievement, gold banner appears at top |
| M-ACH-10 | Toast auto-dismisses | Achievement toast auto-dismisses after 2 seconds |

### D.15 Manual Test Checklist: Pinball Gate Selection (no automation)

| ID | What to Verify | Pass Criteria |
|----|---------------|---------------|
| M-GATE-01 | Gate select background | GateSelectBg renders full-screen |
| M-GATE-02 | Gate select title | GateSelectTitle shows "Select Gate" |
| M-GATE-03 | Gate buttons display | Gate buttons (GateBtn_0 through GateBtn_N) in grid layout |
| M-GATE-04 | Cleared gates blue + checkmark | Gates already cleared show blue color with check indicator |
| M-GATE-05 | Locked gates gray | Gates not yet reached show gray, not selectable |
| M-GATE-06 | Tap cleared gate starts pinball | Tapping a cleared gate starts pinball at that gate |
| M-GATE-07 | Tap locked gate blocked | Tapping a locked gate does nothing |
| M-GATE-08 | Freeplay button visibility | GateFreeplayBtn visible only after all 10 gates cleared |
| M-GATE-09 | Freeplay button works | Tapping Freeplay starts pinball in freeplay mode |
| M-GATE-10 | Back button | GateBackBtn returns to main menu |
| M-GATE-11 | Gate buttons hover states | Gate buttons show hover/pressed visual feedback |
| M-GATE-12 | All gates touch-responsive | Every gate button responds to tap |

### D.16 Manual Test Checklist: Pinball HUD (no automation)

| ID | What to Verify | Pass Criteria |
|----|---------------|---------------|
| M-PIN-01 | Score display | PinballScore shows current session score, starts at 0 |
| M-PIN-02 | Score updates on peg hit | Score increases by 100 when ball hits a peg |
| M-PIN-03 | Score updates on target hit | Score increases by 500 when ball hits target |
| M-PIN-04 | High score / total display | PinballHighScore shows lifetime total score |
| M-PIN-05 | Launch hint text | PinballLaunchHint shows "Drag plunger to launch" before first launch |
| M-PIN-06 | Objective text (score gates) | PinballObjective shows "Score N points" for score-based gates |
| M-PIN-07 | Objective text (peg gates) | PinballObjective shows "Hit all N pegs" for peg-based gates |
| M-PIN-08 | Objective text (target gates) | PinballObjective shows "Hit the target N times" for target gates |
| M-PIN-09 | Objective text (combined) | PinballObjective shows combined criteria for gate 8 |
| M-PIN-10 | Objective text (freeplay) | Shows "Freeplay - All gates cleared!" after all gates |
| M-PIN-11 | Peg counter visible | PinballPegCount visible on hit-all-pegs and combined gates |
| M-PIN-12 | Peg counter updates | "Pegs: X/Y" updates when a peg is hit |
| M-PIN-13 | Target counter visible | PinballTargetCount visible on target-hits gates |
| M-PIN-14 | Target counter updates | "Targets: X/Y" updates when target is hit |
| M-PIN-15 | Balls remaining visible | PinballBalls visible on limited-ball gates only |
| M-PIN-16 | Balls remaining decrements | Ball count decreases when ball drains |
| M-PIN-17 | Gate status pass | PinballGateStatus shows green "Pass" when gate cleared |
| M-PIN-18 | Gate status fail | PinballGateStatus shows red "Fail" when failed (limited balls) |
| M-PIN-19 | Gate number display | PinballGateNum shows "Gate N" during play |
| M-PIN-20 | Back button | PinballBackBtn exits pinball and returns to menu/gate select |
| M-PIN-21 | Daily bonus text | "+25 Daily Bonus!" shown on first gate completion of the day |
| M-PIN-22 | Daily bonus not repeated | Daily bonus not shown on subsequent gate completions same day |
| M-PIN-23 | Peg/target counters hidden on score-only gates | PinballPegCount and PinballTargetCount hidden when not applicable |

### D.17 Test Checklist: Screen Transitions

> **Automation note:** `Test_UIAllScreenElements` programmatically tests ShowScreen/ShowScreenAdditive visibility logic. `UpdateUIInteract` tests actual button-click-driven transitions via InputSimulator (Menu↔Settings, Menu↔LevelSelect, Menu↔CatCafe, Menu→Achievements). Tests marked `[AUTO]` are covered by the autotest suite.

| ID | What to Verify | Pass Criteria |
|----|---------------|---------------|
| M-TRAN-01 | `[AUTO]` Menu to Level Select | InputSimulator clicks LevelSelectButton, verifies LevelSelectBg visible, MenuBg hidden |
| M-TRAN-02 | `[AUTO]` Level Select to Menu | InputSimulator clicks BackButton, verifies MenuBg visible |
| M-TRAN-03 | `[AUTO]` Menu to Settings | InputSimulator clicks SettingsButton, verifies SettingsBg visible, MenuBg hidden |
| M-TRAN-04 | `[AUTO]` Settings to Menu | InputSimulator clicks SettingsBackBtn, verifies MenuBg visible |
| M-TRAN-05 | `[AUTO]` Menu to Cat Cafe | InputSimulator clicks CatCafeButton, verifies CatCafeBg visible |
| M-TRAN-06 | `[AUTO]` Cat Cafe to Menu | InputSimulator clicks CatCafeBackButton, verifies MenuBg visible |
| M-TRAN-07 | `[AUTO]` Menu to Achievements | InputSimulator clicks AchievementsButton, verifies state change |
| M-TRAN-08 | Achievements to Menu | Back button returns to menu |
| M-TRAN-09 | Menu to Gameplay | Continue/Level Select button loads level, shows HUD |
| M-TRAN-10 | Gameplay to Menu (via confirm) | Confirm dialog accept returns to menu, HUD hidden |
| M-TRAN-11 | Gameplay to Victory | Level complete triggers victory overlay on top of HUD |
| M-TRAN-12 | Victory to Next Level | Next Level loads next level seamlessly |
| M-TRAN-13 | Menu to Pinball Gate Select | Pinball button shows gate selection |
| M-TRAN-14 | Gate Select to Pinball Gameplay | Gate button starts pinball, shows pinball HUD |
| M-TRAN-15 | Pinball to Menu | Back button returns to menu from pinball |
| M-TRAN-16 | No orphan UI elements | After each transition, no elements from previous screen remain visible |
| M-TRAN-17 | `[AUTO]` ShowScreen hides other screens | ShowScreen(X) makes X visible, hides all others |
| M-TRAN-18 | `[AUTO]` ShowScreenAdditive for overlays | ShowScreenAdditive(VICTORY) keeps HUD visible |

### D.18 Manual Test Checklist: Focus & Keyboard Navigation (no automation)

| ID | What to Verify | Pass Criteria |
|----|---------------|---------------|
| M-NAV-01 | Main menu vertical nav | Arrow keys navigate vertically between all menu buttons |
| M-NAV-02 | Focused button visual | Focused button shows distinct visual indicator (highlight/glow) |
| M-NAV-03 | Enter/Space activates button | Pressing Enter or Space on focused button triggers its callback |
| M-NAV-04 | HUD horizontal nav | Arrow keys navigate horizontally: Menu, Reset, Undo, Hint, Skip |
| M-NAV-05 | Settings vertical nav | Arrow keys navigate vertically: Sound, Music, Haptics, Credits, Back |
| M-NAV-06 | Enter on toggle | Enter/Space on focused toggle flips its state |
| M-NAV-07 | Focus skips invisible elements | Navigation skips hidden/invisible elements |
| M-NAV-08 | Focus skips disabled elements | Navigation skips non-interactable elements |
| M-NAV-09 | Initial focus on screen enter | First button gets focus when entering a screen |
| M-NAV-10 | Level select 2D grid nav | Arrow keys navigate 4x5 grid (left/right within rows, up/down between rows) |
| M-NAV-11 | All nav works without mouse | Full navigation possible using only keyboard |

### D.19 Manual Test Checklist: Touch Input — Mobile (no automation)

| ID | What to Verify | Pass Criteria |
|----|---------------|---------------|
| M-TOUCH-01 | All buttons respond to tap | Every button across all screens responds to single tap |
| M-TOUCH-02 | All toggles respond to tap | Every toggle switches state on single tap |
| M-TOUCH-03 | No double-tap required | Single tap sufficient for all interactions |
| M-TOUCH-04 | Drag to select shape | Touch-drag on a shape selects and slides it |
| M-TOUCH-05 | Drag direction detection | Shape slides in dominant drag direction (up/down/left/right) |
| M-TOUCH-06 | Shape release snaps | Shape snaps to grid position on finger lift |
| M-TOUCH-07 | Multi-touch ignored | Only single touch tracked, additional fingers ignored |
| M-TOUCH-08 | Pinball plunger drag | Touch-drag down on plunger charges launch |
| M-TOUCH-09 | Pinball plunger release | Lifting finger launches ball |
| M-TOUCH-10 | Overlay tap-to-dismiss | Credits overlay dismisses on tap anywhere |
| M-TOUCH-11 | Confirm dialog buttons touchable | Cancel and Accept buttons in confirm dialog respond to tap |
| M-TOUCH-12 | No hover-only features | No functionality requires mouse hover (hover is enhancement only) |
| M-TOUCH-13 | Touch on overlays blocked behind | Cannot accidentally tap buttons behind a visible overlay |

### D.20 Manual Test Checklist: UI Widget Rendering (no automation — visual verification only)

| ID | What to Verify | Pass Criteria |
|----|---------------|---------------|
| M-WIDG-01 | UIText alignment | Text elements are correctly aligned (left/center/right per element) |
| M-WIDG-02 | UIText multi-line | Multi-line text wraps and displays correctly |
| M-WIDG-03 | UIText font sizes | Different font sizes render at correct scale (20pt, 24pt, 28pt, 32pt, 84pt used) |
| M-WIDG-04 | UIText color | Text colors match specified RGBA values |
| M-WIDG-05 | UIText shadow | Text shadows render at correct offset and alpha |
| M-WIDG-06 | UIRect corner radius | Rects with corner radius show rounded corners (0, 8, 10, 12, 16px used) |
| M-WIDG-07 | UIRect fill color | Rect fill colors match specified RGBA values |
| M-WIDG-08 | UIRect gradient | Gradient rects render smooth color transition |
| M-WIDG-09 | UIRect border | Bordered rects show border at correct thickness and color |
| M-WIDG-10 | UIRect shadow | Rects with shadow show drop shadow at correct offset |
| M-WIDG-11 | UIRect fill direction | Progress bars fill in correct direction (left-to-right) |
| M-WIDG-12 | UIRect fill amount | Fill amount correctly represents 0.0 to 1.0 range |
| M-WIDG-13 | UIImage renders texture | UIImage elements display their assigned textures |
| M-WIDG-14 | UIButton 3-state colors | Buttons show distinct normal/hover/pressed colors |
| M-WIDG-15 | UIButton transition speed | Color transitions between states are smooth (0.12s) |
| M-WIDG-16 | UIToggle distinct on/off | Toggle widgets show clearly different on (green) vs off (gray) state |
| M-WIDG-17 | UIOverlay dim alpha | Overlay dim background renders at correct alpha (0.7) |
| M-WIDG-18 | UIOverlay fade animation | Show/hide uses fade animation (not instant) at 0.2s duration |
| M-WIDG-19 | UILayoutGroup spacing | Layout groups space children correctly (vertical or horizontal) |
| M-WIDG-20 | StretchAll anchoring | Elements with StretchAll anchor fill their parent's full bounds |
| M-WIDG-21 | Sort order rendering | Elements with higher sort order render on top of lower ones |

### D.21 Test Checklist: Lives & Economy UI

> **Automation note:** `Test_UIEconomyDisplay` verifies CoinText, LivesText, TotalStarsText match save data, and tests coin economy updates. Also verifies lives timer visibility logic at max lives. Tests marked `[AUTO]` are covered by the autotest suite.

| ID | What to Verify | Pass Criteria |
|----|---------------|---------------|
| M-ECON-01 | Coins update on level complete | Coin display increases by 10 after completing a level |
| M-ECON-02 | 3-star bonus coins | Coin display increases by additional 5 when earning 3 stars |
| M-ECON-03 | Coins decrease on undo | Coin display decreases by 20 on paid undo |
| M-ECON-04 | Coins decrease on hint | Coin display decreases by 30 on paid hint |
| M-ECON-05 | Coins decrease on skip | Coin display decreases by 100 on skip level |
| M-ECON-06 | Coins decrease on life refill | Coin display decreases by 50 on refill |
| M-ECON-07 | Lives decrease on exit | Lives count decreases by 1 when exiting level with moves made |
| M-ECON-08 | `[AUTO]` Lives preserved on 0-move exit | Lives unchanged when exiting before any moves |
| M-ECON-09 | Lives regeneration timer | Timer counts down correctly, adds 1 life when reaching 0 |
| M-ECON-10 | Lives cap at 5 | Lives never exceed 5 |
| M-ECON-11 | Can't play with 0 lives | Cannot start a level with 0 lives (or appropriate behavior) |
| M-ECON-12 | `[AUTO]` Lives timer hidden at max | LivesTimerText hidden when lives == 5 |
| M-ECON-13 | Pinball gate coins | +25 coins displayed on gate clear |
| M-ECON-14 | Daily puzzle coins | +50 coins displayed on daily puzzle complete |
| M-ECON-15 | `[AUTO]` Coin display matches save data | CoinText matches m_xSaveData.uCoins; updates after modification |
| M-ECON-16 | Insufficient coins feedback | Some visual feedback when trying to spend more coins than available |

### D.22 Test Checklist: Daily Puzzle & Streak UI (logic automated via Test_DailyStreak)

| ID | What to Verify | Pass Criteria |
|----|---------------|---------------|
| M-DAILY-01 | Daily Puzzle button on menu | DailyPuzzleButton visible on main menu |
| M-DAILY-02 | Daily puzzle loads | Tapping Daily Puzzle starts a puzzle at Hard tier difficulty |
| M-DAILY-03 | Daily streak increments | Completing daily puzzle on consecutive days increments streak |
| M-DAILY-04 | Streak display updates | StreakGroup shows updated streak count after completion |
| M-DAILY-05 | Daily puzzle reward | +50 coins awarded on daily puzzle completion |
| M-DAILY-06 | Same puzzle per day | Re-entering daily puzzle on same day gives same puzzle |

### D.23 Test Checklist: Weekly Challenge UI (logic automated via Test_WeeklyChallenge_*)

| ID | What to Verify | Pass Criteria |
|----|---------------|---------------|
| M-WEEK-01 | Challenge banner on menu | Weekly challenge description visible on main menu |
| M-WEEK-02 | Challenge progress bar | Progress bar shows current progress toward target |
| M-WEEK-03 | Challenge reward display | Coin reward amount visible on challenge banner |
| M-WEEK-04 | Challenge completion toast | "Challenge Complete!" toast shown on reaching target |
| M-WEEK-05 | Challenge refreshes weekly | New challenge appears on Monday |

### D.24 Test Checklist: Save Data & Persistence

> **Automation note:** `Test_SaveLoad_Integrity`, `Test_SaveLoad_VersionMigration`, and `Test_StarRatingPersistence` cover serialization round-trip and data integrity. Tests marked `[AUTO]` are covered by the autotest suite.

| ID | What to Verify | Pass Criteria |
|----|---------------|---------------|
| M-SAVE-01 | `[AUTO]` Progress saved on level complete | Save/load round-trip preserves level records, coins, stars |
| M-SAVE-02 | Progress saved on menu return | State saved when returning to menu |
| M-SAVE-03 | Progress restored on relaunch | All save data present after closing and reopening game |
| M-SAVE-04 | `[AUTO]` Cat collection persisted | Cat collection flags survive save/load cycle |
| M-SAVE-05 | `[AUTO]` Settings persisted | Sound/Music/Haptics flags survive save/load cycle |
| M-SAVE-06 | `[AUTO]` Pinball gate flags persisted | Gate clear flags survive save/load cycle |
| M-SAVE-07 | `[AUTO]` Achievement flags persisted | Achievement flags survive save/load cycle |
| M-SAVE-08 | Reset save clears all | After reset save, all data returns to defaults |

### D.25 Test Checklist: Edge Cases & Error Handling

> **Automation note:** `Test_WinCondition` tests level completion. `Test_LivesNoLossOnZeroMoves` tests zero-move exit. Tests marked `[AUTO]` are covered by the autotest suite.

| ID | What to Verify | Pass Criteria |
|----|---------------|---------------|
| M-EDGE-01 | Level 1 with no progress | Fresh game shows only Continue and basic buttons, no stars |
| M-EDGE-02 | `[AUTO]` Level 100 victory | Completing level 100 works without crash, no "next level" overflow |
| M-EDGE-03 | 0 coins economy | All paid features gracefully handle 0 coin balance |
| M-EDGE-04 | 0 lives state | Game handles 0 lives correctly (blocks play or shows refill) |
| M-EDGE-05 | Rapid button taps | Quickly tapping same button multiple times doesn't double-trigger |
| M-EDGE-06 | Overlay while overlay | Cannot open a second overlay while one is showing |
| M-EDGE-07 | `[AUTO]` Undo on first move | Undo after exactly 1 move restores to initial state |
| M-EDGE-08 | Skip at level 100 | Skip on last level doesn't go beyond level 100 |
| M-EDGE-09 | Screen resize / rotation | UI adapts to different screen sizes without element overlap |
| M-EDGE-10 | Very long cat names | UI handles longest cat names without text overflow |
| M-EDGE-11 | 999+ coins display | Large coin values don't overflow their display container |
| M-EDGE-12 | All 100 cats collected | Cat Cafe shows all 100 cats correctly across all 13 pages |
| M-EDGE-13 | All 10 gates cleared | Gate select shows all gates blue + freeplay button |
| M-EDGE-14 | Max stars (300) | Star progress shows "300 / 300" correctly |

### D.26 Manual Test Checklist: Android-Specific (no automation — device only)

| ID | What to Verify | Pass Criteria |
|----|---------------|---------------|
| M-ANDR-01 | Touch input functional | All touch interactions work on Android device |
| M-ANDR-02 | Portrait orientation | App displays in portrait (720x1280) |
| M-ANDR-03 | Back button behavior | Android back button functions appropriately (e.g., navigates back) |
| M-ANDR-04 | App resume preserves state | Returning from background preserves game state |
| M-ANDR-05 | Vulkan rendering | All UI renders correctly via Vulkan on Android |
| M-ANDR-06 | Pre-compiled shaders | No shader compilation stalls, all .spv files loaded |
| M-ANDR-07 | Frame rate | 60 FPS during UI navigation and gameplay |
| M-ANDR-08 | No ANR on transitions | No "App Not Responding" during screen transitions |
