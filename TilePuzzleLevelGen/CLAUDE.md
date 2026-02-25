# TilePuzzle Level Generator

Standalone offline tool that generates solvable TilePuzzle levels and saves them to disk. Follows the same pattern as FluxCompiler — console-only Windows executable that links against the Zenith static lib with minimal engine initialization.

## How It Works

### Generation Algorithm (Reverse Scramble)

Levels are generated backwards from a solved state:

1. **Grid creation**: 9×9 grid with 1-cell empty border, 7×7 playable interior (49 floor cells)
2. **Blocker placement**: Static non-draggable shapes placed on random floor cells. Restricted to I-shape (3 cells), Domino (2 cells), and Single (1 cell) to ensure max 3 blocker cells
3. **Cat placement**: Colored cats placed on unoccupied floor cells (no same-color cats adjacent, including diagonals). Optionally some cats placed on blockers ("blocker cats")
4. **Shape placement (solved state)**: Draggable shapes placed directly on their matching-color cats — this is the win condition
5. **Conditional marking**: Some draggable shapes are marked as conditional with incrementing unlock thresholds, forcing sequential elimination order
6. **Pre-scramble**: Conditional shapes are moved off their matching cats first (up to 20 random-direction attempts per shape) so they don't start in a trivially-solved position
7. **Scramble**: Shapes are moved away from cats via random valid moves using the shared rules engine. Two modes alternate:
   - **Uncover mode**: When shapes still cover same-color cats, prioritize moving those shapes away
   - **Random mode**: Random moves for distance once all cats are uncovered
8. **Solver verification**: A BFS solver verifies the scrambled level is solvable and counts minimum player moves. Levels below the move threshold are rejected
9. **Post-generation validation**: Levels must have 4-5 draggable shapes, at least one with 2+ cells, and max 3 total blocker cells

This "reverse scramble" approach guarantees solvability by construction — the reverse of the scramble sequence is always a valid solution.

### Parallel "Keep Best" Strategy

Each level generation dispatches 3000 attempts across all CPU threads via `Zenith_TaskArray`. Each worker thread:

- Gets a unique RNG seed based on level number + worker index + seed offset
- Randomly samples parameter values from the configured ranges for each attempt
- Runs the full generate-scramble-solve pipeline
- Keeps only its best result (highest solver move count)

After all workers complete, the single best result across all workers is selected. This naturally biases output toward harder levels without requiring parameter fixing.

### Two-Level BFS Solver

The solver (`TilePuzzle_Solver.h`) counts minimum **player moves** (not cell movements):

- **Outer BFS**: State = all shape positions + eliminated cat bitmask (packed as `uint64_t`). Each transition = one shape pick (cost 1)
- **Inner BFS**: For a given shape pick, explores all reachable positions via cell-by-cell movement. Handles intermediate cat eliminations during a drag (cats eliminated as shapes pass over them)
- **State limit**: Configurable max states (default 2M for gameplay, 500K for generator fast pass). If exceeded, returns -1 (unsolvable/too complex)
- **Win condition**: All cats eliminated. Shapes auto-remove when all same-color cats are eliminated

### Solver State Packing

**Outer state** — Single `uint64_t`:
- Bits [0..15]: 16-bit eliminated cat bitmask (supports up to 16 cats)
- Bits [16..23]: shape 0 packed position (upper nibble=X, lower nibble=Y), `0xFF`=removed
- Bits [24..31]: shape 1 packed position
- Bits [32..39]: shape 2 packed position
- Bits [40..47]: shape 3 packed position
- Bits [48..55]: shape 4 packed position
- Bits [56..63]: unused
- Supports up to 5 draggable shapes with grid dimensions up to 15×15
- Constants: `s_uMaxSolverShapes = 5`, `s_uMaxSolverCats = 16`

**Inner state** — Single `uint64_t`: bits [47..40]=X, [39..32]=Y, [31..0]=eliminatedMask.

### Solver Optimizations

- **Open-addressing flat hash set** (`TilePuzzleFlatHashSet`) for outer visited set: Fibonacci hashing, linear probing, sentinel `UINT64_MAX`, 75% load factor auto-resize. Much better cache locality than `std::unordered_set`
- **Pre-computed walkable grid**: `bool abWalkable[256]` combines floor + static blocker checks into a single O(1) lookup
- **Pre-computed cat-at-cell lookup**: `int8_t aiCatAtCell[256]` gives O(1) wrong-color cat blocking checks
- **Inlined collision checking**: CanMoveShape and ComputeNewlyEliminatedCats are inlined in the hot BFS loop. Cross-validated against canonical `TilePuzzle_Rules` in `ZENITH_ASSERT` builds
- Inner BFS uses `std::unordered_set<uint64_t>` (re-used per drag, cleared between drags)

### Two-Tier Solver Strategy

The tool uses a two-tier BFS to find deeper solutions:

1. **Fast pass (500K states)**: All candidates verified at 500K state limit. Quickly identifies solutions up to ~12-15 moves. ~80% of candidates that pass scramble are rejected as "unsolvable" (exceeding the limit)

2. **Deep pass (5M states)**: Up to 5 unsolvable candidates per worker thread (~80 total across 16 threads) are re-verified at 10× the fast limit. These "unsolvable" candidates have large BFS state spaces that may contain deep 18+ move solutions the fast solver couldn't verify

This is controlled by `DifficultyParams::uDeepSolverStateLimit` and `uMaxDeepVerificationsPerWorker`. The game runtime uses `uDeepSolverStateLimit = 0` (disabled) for fast single-round generation.

### Retry Loop

The tool wraps generation in a retry loop:
- Each "round" runs `GenerateLevel()` with the same level number but a different `seedOffset`
- The best result across all rounds is kept (with shape definitions copied to local storage to avoid dangling pointers)
- Rounds repeat until `--min-moves` target is met or `--timeout` expires
- Round time: ~8-10 min on a 16-thread machine with current parameters
- Fast rounds (more retry rounds) produce better results than slow rounds (fewer retries) because more seed regions are explored

## File Structure

| File | Purpose |
|------|---------|
| `TilePuzzleLevelGen.cpp` | Main entry point, CLI argument parsing, retry loop, post-generation validation |
| `TilePuzzleLevelData_Serialize.h` | Binary `.tlvl` serialization via `Zenith_DataStream` |
| `TilePuzzleLevelData_Json.h` | Human-readable JSON export with grid visualization and analytics |
| `TilePuzzleLevelData_Image.h` | PNG image rendering via `stb_image_write.h` |
| `TilePuzzleLevelGen_Analytics.h` | Run-wide analytics accumulation and `analytics.txt` output |

The actual generation logic lives in `Games/TilePuzzle/Components/TilePuzzle_LevelGenerator.h` (shared with the game runtime).

## Output Formats

Each generated level produces 3 files:

- **`.tlvl`** — Binary format (magic `0x54504C56` "TPLV", version 1). Contains grid, shapes with inline definitions, cats. Used by the game to load pre-generated levels
- **`.json`** — Human-readable JSON with grid visualization (`.`=empty, `#`=floor), shape/cat data, solver move count, winning parameter values, and full per-attempt analytics breakdown
- **`.png`** — 32px/cell top-down image. Colors match in-game materials: grey floor, brown blockers, colored shapes (2px inset), brighter diamond markers for cats

Each run also produces an **`analytics.txt`** with aggregate statistics (written at end of run or Ctrl+C).

### Output Directory Structure

```
TilePuzzleLevelGen/Output/
    Run0/
        level_0001.tlvl / .json / .png
        level_0002.tlvl / .json / .png
        ...
        analytics.txt
    Run1/
        ...
```

Each execution auto-increments the run number by scanning existing `Run*` directories.

## Building and Running

```batch
# Regenerate solution (from Build/ directory)
Sharpmake_Build.bat

# Build (Release recommended for generation speed)
msbuild Build/zenith_win64.sln /p:Configuration=vs2022_Release_Win64_True /p:Platform=x64 -maxCpuCount:1

# Run (generates continuously until Ctrl+C)
TilePuzzleLevelGen/output/win64/vs2022_release_win64_true/tilepuzzlelevelgen.exe

# Run with count limit and 30-minute timeout per level
tilepuzzlelevelgen.exe --count 10

# Custom output directory and seed
tilepuzzlelevelgen.exe --output path/to/output --count 50 --seed 100
```

**Important**: Always build with `-maxCpuCount:1` to avoid hanging compiler processes. Use Release builds for generation — Debug is significantly slower due to the heavy BFS solver.

### CLI Arguments

| Argument | Default | Description |
|----------|---------|-------------|
| `--count N` | 0 (infinite) | Number of levels to generate |
| `--output DIR` | `LEVELGEN_OUTPUT_DIR` | Output directory |
| `--timeout N` | 1800 (30 min) | Per-level time budget in seconds |
| `--min-moves N` | 20 | Minimum solver moves target (saves best on timeout) |
| `--solver-limit N` | 500000 | BFS state limit for fast pass |
| `--seed N` | 0 | Starting seed counter (each round increments) |

## Current Configuration

### Tool-Specific Parameters (in `TilePuzzleLevelGen.cpp`)

These override the game defaults at runtime:

| Parameter | Value | Description |
|-----------|-------|-------------|
| Grid | 9×9 fixed | 7×7 playable interior (49 floor cells) |
| Colors | 5 fixed | Red, Green, Blue, Yellow, Purple |
| Cats per color | 2 fixed | 10 total cats (2^10 = 1024 elimination states) |
| Shapes per color | 1 | 5 draggable shapes total |
| Blockers | 1 fixed | Restricted to I/Domino/Single (max 3 cells) |
| Blocker cats | 0-1 | Cats on blockers require adjacency elimination |
| Conditional shapes | 1-2 | Shapes requiring N eliminations to unlock |
| Conditional threshold | 1-4 | Per-attempt random unlock threshold |
| Shape complexity | 2-4 | Domino to L/T/S/Z/O range |
| Scramble moves | 30-1000 | Min successful moves / target moves |
| Attempts per round | 3000 | Distributed across all CPU threads |
| Fast solver limit | 500K | BFS states for fast pass |
| Deep solver limit | 5M | BFS states for deep verification |
| Deep verifications/worker | 5 | Max deep re-verifications per worker thread |
| Min solver moves (internal) | 6 | Per-attempt filter; outer loop enforces actual target |

### Game Defaults (in `TilePuzzle_LevelGenerator.h`)

The `static constexpr` values in `TilePuzzle_LevelGenerator.h` define defaults for in-game generation. The tool overrides most of these. Changing the constexpr values requires a rebuild.

### Shape Complexity Values

| Value | Shapes Available | Cell Count |
|-------|-----------------|------------|
| 1 | Single | 1 cell |
| 2 | Single, Domino | 1-2 cells |
| 3 | + L (4), T (4), I (3) | 3-4 cells |
| 4 | + S (4), Z (4), O (4) | 3-4 cells |

Shape placement uses a **priority fallback**: at complexity 4, first tries a random pick from L..O, then L..I, then Domino, then Single — each with all 4 rotations. Falls to simpler shapes only when complex ones don't fit on the grid.

### Available Colors

| Enum | Color | Shapes | Cats |
|------|-------|--------|------|
| `TILEPUZZLE_COLOR_RED` | Red | S, T, L, etc. | Diamond markers |
| `TILEPUZZLE_COLOR_GREEN` | Green | | |
| `TILEPUZZLE_COLOR_BLUE` | Blue | | |
| `TILEPUZZLE_COLOR_YELLOW` | Yellow | | |
| `TILEPUZZLE_COLOR_PURPLE` | Purple | | |
| `TILEPUZZLE_COLOR_NONE` | Brown | Blockers only | N/A |

### Blocker Types (Restricted)

The generator restricts blocker shapes to those with ≤ 3 cells to satisfy the post-generation validation constraint:

| Type | Cells | Description |
|------|-------|-------------|
| `TILEPUZZLE_SHAPE_I` | 3 | Linear wall |
| `TILEPUZZLE_SHAPE_DOMINO` | 2 | Small wall |
| `TILEPUZZLE_SHAPE_SINGLE` | 1 | Point obstacle |

L-shape (4 cells) and T-shape (4 cells) were removed from the blocker pool because a single L or T blocker exceeds the 3-cell limit.

## Failure Mode Breakdown

Every generation attempt can fail in one of three ways:

| Failure Mode | Typical Rate | Cause |
|-------------|-------------|-------|
| **Scramble failure** | ~68% | Shapes couldn't be moved far enough from cats. Higher with complex shapes (90% at complexity 4) due to more collision constraints |
| **Solver too easy** | ~1-2% | Scramble achieved fewer than `minScrambleMoves` moves, or level solved in fewer than 6 moves |
| **Solver unsolvable** | ~26% | Solver BFS exceeded 500K state limit. Level is technically solvable (by construction) but too complex for the fast solver. ~12% of these are recovered by deep verification |

Overall success rate is ~4.5%. The keep-best strategy deliberately generates many candidates to find exceptional ones.

### Per-Complexity Breakdown

| Complexity | Scramble Fail | Unsolvable | Success Rate |
|-----------|--------------|-----------|-------------|
| 2 (Domino/Single) | ~30% | ~63% | ~4.8% |
| 3 (L/T/I) | ~86% | ~9% | ~5.0% |
| 4 (all shapes) | ~90% | ~6% | ~3.9% |

Complexity 2 has the lowest scramble failure but highest unsolvable rate (small shapes have more reachable positions = wider BFS = harder to solve at 500K). Complexity 3-4 have high scramble failure but narrower BFS trees.

## Key Constraints and Tradeoffs

### Why 2 Cats Per Color (Not 3)

With 5 colors × 3 cats/color = 15 cats, the elimination bitmask has 2^15 = 32,768 states. This makes the BFS state space too large — **0% solver success rate** at any practical limit (tested 100K, 500K, 2M). The solver physically cannot find solutions.

With 2 cats/color = 10 cats, 2^10 = 1,024 elimination states — tractable. The solver finds 12-19 move solutions at 500K-5M limits.

### Why 16-Bit Cat Mask (Not 32-Bit)

The solver state was repacked to support 5 shapes: 16-bit cat mask (bits [0..15]) + 5 × 8-bit positions (bits [16..55]). This limits cats to 16 maximum (sufficient for 5 colors × 3 = 15, though only 10-11 are used currently). The old 32-bit mask supported only 4 shapes.

### Why Fast Rounds Beat Deep Rounds

Tested 10M deep solver with 8 verifications per worker: rounds took ~16 min, limiting to ~2 rounds per level. Result: best 15 moves. Tested 5M deep solver with 5 per worker: rounds took ~10 min, allowing ~3 rounds. Result: best 19 moves. **More rounds exploring different seed regions finds deeper solutions than fewer rounds with a more powerful solver.**

### Why Fixed 9×9 Grid

- 7×7 (25 playable cells): Too tight for 10 cats + 5 shapes + blocker
- 8×8 (36 playable cells): Workable but tighter, less variety
- 9×9 (49 playable cells): Good balance of space for shapes to move, proven 18-19 move results
- 10×10 (64 playable cells): Tested — all candidates unsolvable at 500K (wider BFS from more positions)

### Dangling Pointer Prevention

When saving the best level across retry rounds, shape definitions must be copied to local storage (`axBestLevelDefs` vector) because subsequent `GenerateLevel()` calls clear the static definition vector. All shape instance pointers are then remapped to the local copies.

## Benchmark Results

### Run127 (5 Colors × 2 Cats, Solver Repack Validation)
- 1 level, min-moves 1, 500K solver, 300s timeout
- **12 moves** in 1 round (326s). solver=12, verify=12 — confirmed 5-shape packing is correct
- 55 successes / 2000 attempts = 2.8% success rate

### Run129 (5 Colors × 2 Cats, Restricted Blockers, Min 20)
- 2 levels, 500K fast + 5M deep (3 per worker), 2000 attempts/round, 1800s timeout
- **Level 1**: 18 moves (6 rounds, 35 min TIMEOUT)
- **Level 2**: 16 moves (5 rounds, 30 min TIMEOUT)
- 441 successes across level 1 rounds, 4.4% success rate

### Run131 (Tuned Deep Verification, Min 20)
- 2 levels, 500K fast + 5M deep (5 per worker), 3000 attempts/round, 1800s timeout
- **Level 1**: 18 moves (5 rounds, 42 min TIMEOUT)
- **Level 2**: **19 moves** (4 rounds, 31 min TIMEOUT) — best result achieved
- 950 total successes across both levels, 4.5% success rate
- ~130 candidates per round, ~10 min per round

### Expected Performance

| Min Moves Target | Typical Result | Typical Time | Notes |
|------------------|---------------|-------------|-------|
| 12 | 12-15 moves | 1 round (~10 min) | Fast pass reliably finds 12+ |
| 15 | 15-18 moves | 2-3 rounds (~20-30 min) | Deep verify needed for 15+ |
| 18 | 18-19 moves | 3-5 rounds (~30-50 min) | Usually hits 18; 19 is lucky |
| 20 | 18-19 moves (TIMEOUT) | 30+ min | 20 not reached; 19 is practical ceiling |

The practical ceiling of 18-19 moves is limited by the BFS state budget (500K fast / 5M deep) with 5 shapes on a 9×9 grid. The branching factor (each shape can be dragged to many positions) consumes the state budget before reaching depth 20.

## Pitfalls and Gotchas

### stdout Buffering in Background Processes

When running the generator as a background process, `fflush(stdout)` may not be sufficient to see real-time progress. **Monitor the output directory for new files instead of watching stdout.** The generator writes `.json` files immediately on level completion.

### Release Build Required for Practical Use

Debug builds are ~5-10× slower due to the heavy BFS solver running thousands of times per level plus `ZENITH_ASSERT` cross-validation of inlined solver logic. Always use `vs2022_Release_Win64_True` for actual generation runs.

### RNG Determinism

Worker RNG seeds are derived from `levelNumber * 7919 + 104729 + workerIndex * 31337 + seedOffset * 999983`. The `seedOffset` (global seed counter) increments per retry round, ensuring each round explores a different seed space. Task system scheduling is non-deterministic, so exact results may vary between runs even with the same seed.

### Long-Running Processes

Each level can take 30-50 minutes. The per-level timeout (default 1800s) is checked after each round completes, so actual time may exceed the timeout by one round duration (~10 min). For multi-level runs, total time = levels × (timeout + ~1 round overhead).
