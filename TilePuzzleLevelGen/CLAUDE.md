# TilePuzzle Level Generator

Standalone offline tool that generates solvable TilePuzzle levels and saves them to disk. Follows the same pattern as FluxCompiler — console-only Windows executable that links against the Zenith static lib with minimal engine initialization.

## How It Works

### Generation Algorithm (Reverse Scramble)

Levels are generated backwards from a solved state:

1. **Grid creation**: 8x8 grid with 1-cell empty border, interior filled with floor cells
2. **Blocker placement**: Static non-draggable shapes placed on random floor cells
3. **Cat placement**: Colored cats placed on unoccupied floor cells (no same-color cats adjacent, including diagonals). Optionally some cats placed on blockers ("blocker cats")
4. **Shape placement (solved state)**: Draggable shapes placed directly on their matching-color cats — this is the win condition
5. **Scramble**: Shapes are moved away from cats via random valid moves using the shared rules engine. Two modes alternate:
   - **Uncover mode**: When shapes still cover same-color cats, prioritize moving those shapes away
   - **Random mode**: Random moves for distance once all cats are uncovered
6. **Solver verification**: A BFS solver verifies the scrambled level is solvable and counts minimum player moves. Levels below the move threshold are rejected

This "reverse scramble" approach guarantees solvability by construction — the reverse of the scramble sequence is always a valid solution.

### Parallel "Keep Best" Strategy

Each level generation dispatches `s_iTilePuzzleMaxGenerationAttempts` (currently 1000) attempts across all CPU threads via `Zenith_TaskArray`. Each worker thread:

- Gets a unique RNG seed based on level number + worker index
- Randomly samples parameter values from the configured ranges for each attempt
- Runs the full generate-scramble-solve pipeline
- Keeps only its best result (highest solver move count)

After all workers complete, the single best result across all workers is selected. This naturally biases output toward harder levels without requiring parameter fixing.

### Two-Level BFS Solver

The solver (`TilePuzzle_Solver.h`) counts minimum **player moves** (not cell movements):

- **Outer BFS**: State = all shape positions + eliminated cat bitmask. Each transition = one shape pick (cost 1)
- **Inner BFS**: For a given shape pick, explores all reachable positions via cell-by-cell movement. Handles intermediate cat eliminations during a drag (cats eliminated as shapes pass over them)
- **State limit**: Configurable max states (default 2M for gameplay, 500K for generator). If exceeded, returns -1 (unsolvable/too complex)
- **Win condition**: All cats eliminated. Shapes auto-remove when all same-color cats are eliminated

### Solver Optimizations

The solver is heavily optimized for throughput since it runs thousands of times per generated level:

**Packed uint64_t outer state** — The outer BFS state is a single `uint64_t` instead of a struct:
- Bits [63..32]: 4 shape positions, 8 bits each (4-bit X + 4-bit Y), sentinel `0xFF` = removed
- Bits [31..0]: 32-bit eliminated cat bitmask
- Supports grids up to 15×15 and up to 32 cats
- `TilePuzzleSolverPacking` namespace provides inline pack/unpack helpers
- Constants: `s_uMaxPackedShapes = 4`, `s_uMaxSolverShapeCells = 8`, `s_uMaxBitmaskGridSize = 16`

**Open-addressing flat hash set** (`TilePuzzleFlatHashSet`) — Replaces `std::unordered_set` for both outer and inner visited sets:
- Fibonacci hashing (`key * 0x9E3779B97F4A7C15`) with power-of-2 capacity
- Linear probing, sentinel `UINT64_MAX` for empty slots
- `malloc`/`memset(0xFF)` initialization — no per-element construction
- Auto-resizes at 75% load factor
- Much better cache locality than `std::unordered_set` (flat array vs. linked-list buckets)

**GridBitmask collision detection** (`TilePuzzleGridBitmask`) — 256-bit bitmask (4 × `uint64_t`) for 16×16 grids:
- Pre-computed at solver start: `xFloorMask` (valid floor cells), `xBlockerMask` (static shapes)
- Per-shape bitmask computed once per position, reused for all collision checks
- `Overlaps()` / `OverlapsComplement()` — single bitwise AND across 4 words replaces nested cell-by-cell loops
- Used for: floor bounds check, blocker collision, other-shape collision, cat overlap detection

**Pre-computed data for cache locality**:
- Cell offsets cached in flat arrays `aiCellOffX[shape][cell]` / `aiCellOffY[shape][cell]`
- Pre-computed bounding boxes `aiMinOffX/aiMaxOffX/aiMinOffY/aiMaxOffY` per shape for quick bounds rejection
- Cat positions, colors, and grid cells pre-cached in flat arrays

**Cached other-shapes mask** — The combined bitmask of all non-dragged shapes is built once per drag and only rebuilt when an elimination during the inner BFS causes a shape removal (cache key = current elimination mask).

**`__popcnt` intrinsic** — Used for unlock threshold checks (`auUnlockThresholds[shape]`) instead of bit-counting loops.

**Inner BFS state packing** — Inner state is also a `uint64_t`: bits [47..40]=X, [39..32]=Y, [31..0]=eliminatedMask. Avoids struct overhead for the high-volume inner BFS.

## File Structure

| File | Purpose |
|------|---------|
| `TilePuzzleLevelGen.cpp` | Main entry point, CLI argument parsing, run loop |
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
TilePuzzleLevelGen/obj/win64/vs2022_release_win64_true/tilepuzzlelevelgen.exe

# Run with count limit
tilepuzzlelevelgen.exe --count 10

# Custom output directory
tilepuzzlelevelgen.exe --output path/to/output --count 50
```

**Important**: Always build with `-maxCpuCount:1` to avoid hanging compiler processes. Use Release builds for generation — Debug is significantly slower due to the heavy BFS solver.

## Generation Parameters

All parameters are `static constexpr` in `TilePuzzle_LevelGenerator.h` (lines 37-59). Change values and rebuild to tune generation.

### Current Configuration (Difficulty-Focused)

| Parameter | Value | Description |
|-----------|-------|-------------|
| `s_iTilePuzzleMaxGenerationAttempts` | 1000 | Total attempts distributed across all CPU threads |
| `s_uGenMinGridWidth/Height` | 8 | Fixed 8x8 grid (6x6 playable interior) |
| `s_uGenMaxGridWidth/Height` | 8 | |
| `s_uGenMinColors` | 3 | Fixed 3 colors (Red, Green, Blue) |
| `s_uGenMaxColors` | 3 | |
| `s_uGenMinCatsPerColor` | 3 | Fixed 3 cats per color (9 total) |
| `s_uGenMaxCatsPerColor` | 3 | |
| `s_uGenNumShapesPerColor` | 1 | 1 draggable shape per color (3 total) |
| `s_uGenMinBlockers` | 1 | 1-2 static blocker shapes |
| `s_uGenMaxBlockers` | 2 | |
| `s_uGenMinShapeComplexity` | 3 | Tromino to tetromino range |
| `s_uGenMaxShapeComplexity` | 4 | |
| `s_uGenScrambleMoves` | 600 | Target scramble moves (more = more displacement) |
| `s_uGenMinBlockerCats` | 0 | 0-1 cats placed on blockers |
| `s_uGenMaxBlockerCats` | 1 | |
| `s_uGenMinConditionalShapes` | 0 | 0-1 shapes requiring cat eliminations to unlock |
| `s_uGenMaxConditionalShapes` | 1 | |
| `s_uGenMaxConditionalThreshold` | 2 | Max cat eliminations needed to unlock conditional shapes |
| `s_uGenMinSolverMoves` | 6 | Reject levels with fewer than 6 solver moves |
| `s_uGenSolverStateLimit` | 500000 | BFS state limit for generation (500K) |
| `s_uGenMinScrambleMoves` | 100 | Reject levels where scramble achieved < 100 successful moves |

### Shape Complexity Values

| Value | Shapes Available | Cell Count |
|-------|-----------------|------------|
| 1 | Single | 1 cell |
| 2 | Single, Domino | 1-2 cells |
| 3 | L, T, I (tromino) | 3-4 cells |
| 4 | L, T, I, S, Z, O (tetromino) | 3-4 cells |

When complexity is a range (e.g., 3-4), each attempt randomly picks a value. The keep-best strategy naturally selects complexity 4 winners more often since they tend to produce harder puzzles.

### Tool-Specific Overrides

The standalone tool overrides these DifficultyParams at runtime (does NOT modify the game defaults):

| Override | Tool Value | Game Default | Purpose |
|----------|-----------|-------------|---------|
| `uMinSolverMoves` | 6 | `s_uGenMinSolverMoves` (6) | Internal per-attempt filter; kept low so candidates survive for retry loop |
| `uSolverStateLimit` | 500000 | `s_uGenSolverStateLimit` | Fast pass limit (CLI: `--solver-limit`) |
| `uDeepSolverStateLimit` | 5000000 | 0 (disabled) | Deep re-verification limit (10× fast pass) |
| `uMaxDeepVerificationsPerWorker` | 8 | 0 (disabled) | Deep-verify up to 8 unsolvable candidates per worker per round |

## Parameter Influence on Difficulty

These insights come from extensive A/B testing across hundreds of generated levels (Run17 baseline vs Run30 difficulty-focused).

### Key Difficulty Drivers (Ranked by Impact)

1. **Blocker cats** (`blockerCats`): **Strongest driver** (+0.64 avg solver moves when present). Cats on blockers require adjacency elimination instead of direct overlap, creating more complex routing puzzles. Range 0-1 is optimal.

2. **Shape complexity** (`shapeComplexity`): Complexity 4 (tetrominoes) produces harder levels than complexity 3 (trominoes). Average 10.0 moves at complexity=4 + blockers=2 vs ~8.5 for simpler configs. However, **do not fix to complexity=4** — see pitfalls below.

3. **Minimum solver moves** (`minSolverMoves`): Raising from 5 to 6 cut the easy tail without significantly reducing throughput. Directly controls the quality floor. Higher values increase generation time exponentially.

4. **Scramble moves** (`scrambleMoves`): Increasing from 400 to 600 improved average difficulty by ~10%. More scramble = shapes end up further from their target cats = more moves to solve. Diminishing returns above ~600.

5. **Number of blockers** (`blockers`): More blockers restrict shape movement, but the relationship with difficulty is non-linear. 1-2 blockers is the sweet spot. See pitfalls for why.

### Parameter Interactions

- **complexity=4 + blockers=2**: Strongest combination (avg 10.0 solver moves). Tetrominoes are harder to navigate around blockers
- **blockerCats + high complexity**: Multiplicative difficulty increase. Blocker cats create mandatory detours that are harder to navigate with larger shapes

### What Doesn't Significantly Affect Difficulty

- **Grid size**: Fixed at 8x8. Larger grids don't necessarily produce harder puzzles, just sparser ones
- **Colors/CatsPerColor**: Fixed at 3/3. These define puzzle scope but don't independently drive difficulty when the keep-best strategy is in play. Both always show ~5% success rate in analytics
- **Conditional shapes**: Add strategic depth but don't consistently raise solver move count

### Failure Mode Breakdown

Every generation attempt can fail in one of three ways:

| Failure Mode | Typical Rate | Cause |
|-------------|-------------|-------|
| **Scramble failure** | ~53% | Shapes couldn't be moved far enough from cats. Happens when shapes get stuck in corners or blocked by other shapes |
| **Solver too easy** | ~19% | Level solved in fewer than `minSolverMoves` moves, or scramble achieved fewer than `minScrambleMoves` successful moves |
| **Solver unsolvable** | ~23% | Solver BFS exceeded state limit. Level is technically solvable (by construction) but too complex for the solver to prove it within the state budget |

Overall success rate is typically 1.7-5.3% depending on parameter settings. This is expected — the keep-best strategy deliberately generates many candidates to find exceptional ones.

## Pitfalls and Gotchas

### Fixed Blockers=1 Causes Solver State Space Explosion

**Never fix `blockers` to exactly 1 with no range.** Fewer blockers = more free cells = shapes can reach exponentially more positions per drag = inner BFS explores vastly more states. With blockers=1, a single level can take 10+ minutes instead of ~90 seconds. Always use a range (1-2) so some attempts get 2 blockers.

### Fixed Complexity=4 Causes Memory Contention

**Never fix `shapeComplexity` to exactly 4.** Tetromino-only generation with high attempt counts causes excessive memory usage per thread (large BFS state spaces). Combined with 1000+ attempts across 16 threads, this creates memory contention and thrashing. Use a range (3-4) and let keep-best naturally select complexity 4 winners.

### stdout Buffering in Background Processes

When running the generator as a background process (e.g., from a script or CI), `fflush(stdout)` may not be sufficient to see real-time progress. **Monitor the output directory for new files instead of watching stdout.** The generator writes `.json` files immediately on level completion — if `level_0001.json` exists, the level was generated successfully regardless of what stdout shows.

### Release Build Required for Practical Use

Debug builds are ~5-10x slower due to the heavy BFS solver running thousands of times per level. Always use `vs2022_Release_Win64_True` for actual generation runs. The Zenith library must also be compiled in Release — rebuilding just the generator project in Release while Zenith is Debug does not help.

### RNG Determinism

Worker RNG seeds are derived from `levelNumber * 7919 + 104729 + workerIndex * 31337 + seedOffset * 999983`. The `seedOffset` varies per retry round, ensuring each round explores a different seed space. Task system scheduling is non-deterministic, so which worker finishes first (and thus which "best" is selected when multiple workers tie) may vary between runs.

## Two-Tier Solver

The tool uses a two-tier BFS strategy to find deeper solutions:

1. **Fast pass (500K states)**: All 1000 candidates are verified at 500K state limit. Quickly identifies solutions up to ~12 moves. ~23% of candidates are rejected as "unsolvable" (exceeding the limit).

2. **Deep pass (5M states)**: Up to 8 unsolvable candidates per worker thread (~128 total across 16 threads) are re-verified at 10× the fast limit. These "unsolvable" candidates have large BFS state spaces that may contain deep 15+ move solutions the fast solver couldn't verify.

This is controlled by `DifficultyParams::uDeepSolverStateLimit` and `uMaxDeepVerificationsPerWorker`. The game runtime uses `uDeepSolverStateLimit = 0` (disabled) for fast single-round generation. The tool sets non-zero values to enable deep verification.

### Retry Loop

The tool wraps generation in a retry loop:
- Each "round" runs `GenerateLevel()` with the same level number but a different `seedOffset`
- The best result across all rounds is kept
- Rounds repeat until `--min-moves` target is met or `--timeout` expires
- Round time depends on solver speed (see Solver Optimizations); ~230s per round pre-optimization on a 16-thread machine

### CLI Arguments

| Argument | Default | Description |
|----------|---------|-------------|
| `--count N` | 0 (infinite) | Number of levels to generate |
| `--output DIR` | `LEVELGEN_OUTPUT_DIR` | Output directory |
| `--timeout N` | 900 | Per-level time budget in seconds |
| `--min-moves N` | 15 | Minimum solver moves target |
| `--solver-limit N` | 500000 | BFS state limit for fast pass |

### Why Not Random Seed Levels

Tested using random "virtual level numbers" per retry round (instead of fixed level number + seed offset) to maximize seed diversity. This was WORSE: some random seeds explored poor parameter regions, while the fixed approach benefits from consistent seed-space exploration across rounds.

### Why Not Increased Scramble

Tested scramble=1000 (up from 600): WORSE performance. More scramble moves cause shapes to go back and forth without increasing net displacement. 600 is optimal. Above 600 shows diminishing returns.

## Benchmark Results

### Run17 (Baseline: minSolverMoves=5, scrambleMoves=400)
- 10 levels, avg 8.2 solver moves, range 7-9
- Avg 91.0s per level, 5.3% success rate

### Run30 (Difficulty-Focused: minSolverMoves=6, scrambleMoves=600)
- 85 levels in ~2 hours, avg 9.0 solver moves, range 7-16
- Avg 86.0s per level, 1.7% success rate
- 29.4% of levels at 10+ moves, 10.6% at 12+ moves

### Run43 (Two-Tier Solver: 500K fast + 5M deep, 8 deep/worker, --min-moves 15)
- Level 1: **15 moves** in 1 round (231s) — deep verification found solution fast solver missed
- Level 2: **12 moves** in 6 rounds (1069s, TIMEOUT) — seed space doesn't contain 15+ solutions
- ~230s per round on 16-thread machine
- Roughly 30-50% of levels reach 15+ within 15 minutes; remainder plateau at 10-12

### Expected Performance by Target

| Min Moves Target | Hit Rate (15 min) | Typical Time | Notes |
|------------------|--------------------|-------------|-------|
| 10 | ~100% | 1 round (~230s) | Fast-pass reliably finds 10+ |
| 12 | ~90%+ | 1-3 rounds (~230-700s) | Most seeds have 12+ in unsolvable bucket |
| 15 | ~30-50% | 1-6 rounds (~230-1400s) | Depends on seed; some seeds plateau at 12 |

The 15-move hit rate is fundamentally limited by the parameter space (8×8 grid, 3 colors, 3 cats, complexity 3-4). 15-move solutions are rare in this space. Expanding parameters (more colors, larger grid) would shift the distribution but changes the puzzle design.
