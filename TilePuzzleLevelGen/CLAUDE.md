# TilePuzzle Level Generator

Standalone offline tool that generates solvable TilePuzzle levels and saves them to disk. Follows the same pattern as FluxCompiler — console-only Windows executable that links against the Zenith static lib with minimal engine initialization.

## How It Works

### Generation Algorithm (Reverse Scramble)

Levels are generated backwards from a solved state:

1. **Grid creation**: NxM grid (randomized per attempt based on difficulty) with 1-cell empty border, (N-2)x(M-2) playable interior
2. **Blocker placement**: Static non-draggable shapes placed on random floor cells. Restricted to I-shape (3 cells), Domino (2 cells), and Single (1 cell) — these are the only entries in the `aeBlockerTypes` array in the generation code
3. **Cat placement**: Colored cats placed on unoccupied floor cells (no same-color cats adjacent, including diagonals). Optionally some cats placed on blockers ("blocker cats")
4. **Shape placement (solved state)**: Draggable shapes placed directly on their matching-color cats — this is the win condition
5. **Conditional marking**: Some draggable shapes are marked as conditional with incrementing unlock thresholds, forcing sequential elimination order
6. **Pre-scramble**: Conditional shapes are moved off their matching cats first (up to 20 random-direction attempts per shape) so they don't start in a trivially-solved position
7. **Scramble**: Shapes are moved away from cats using the shared rules engine. Three scramble modes are available (selected by `RandomizeDifficultyParams` based on min-moves target):
   - **RANDOM**: Two internal phases alternate — *Uncover* (when shapes cover same-color cats, prioritize moving those shapes away) and *Random* (random one-cell moves for displacement)
   - **GUIDED**: Same Uncover phase, but the Random phase is replaced by Manhattan-distance maximization — evaluates all valid moves and picks the one maximizing total Manhattan distance from solved positions. 15% epsilon-greedy: with probability `fGuidedEpsilon = 0.15`, falls back to random move instead of guided
   - **REVERSE_BFS**: BFS outward from solved state, picks the deepest reachable configuration. Used for smaller grids (<=49 cells) where branching factor is manageable
8. **Solver verification**: A BFS solver verifies the scrambled level is solvable and counts minimum player moves. Levels below the move threshold are rejected
9. **Post-generation validation**: `IsLevelValid` checks: at least 1 draggable shape and max 12 total blocker cells

This "reverse scramble" approach guarantees solvability by construction — the reverse of the scramble sequence is always a valid solution.

### Parallel "Keep Best" Strategy

Each level generation dispatches a configurable number of attempts (500-3000 depending on difficulty) across all CPU threads via `Zenith_TaskArray`. Each worker thread:

- Gets a unique RNG seed based on level number + worker index + seed offset
- Randomly samples parameter values from the configured ranges for each attempt (within the per-round params set by `RandomizeDifficultyParams`)
- Runs the full generate-scramble-solve pipeline
- Keeps only its best result (highest solver move count)

After all workers complete, the single best result across all workers is selected. This naturally biases output toward harder levels without requiring parameter fixing.

### Two-Level BFS Solver

The solver (`TilePuzzle_Solver.h`) counts minimum **player moves** (not cell movements):

- **Outer BFS**: State = all shape positions + eliminated cat bitmask (packed as `uint64_t`). Each transition = one shape pick (cost 1)
- **Inner BFS**: For a given shape pick, explores all reachable positions via cell-by-cell movement. Handles intermediate cat eliminations during a drag (cats eliminated as shapes pass over them)
- **State limit**: Configurable max states (default `s_uTilePuzzleMaxSolverStates = 2000000` for gameplay). If exceeded, returns -1 (unsolvable/too complex)
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
- Supports up to 5 draggable shapes with grid dimensions up to 15x15
- Constants: `s_uMaxSolverShapes = 5`, `s_uMaxSolverCats = 16`

**Inner state** — Single `uint64_t`: bits [47..40]=X, [39..32]=Y, [31..0]=eliminatedMask (32-bit mask to track mid-drag eliminations).

### Solver Optimizations

- **Open-addressing flat hash set** (`TilePuzzleFlatHashSet`) for outer visited set: Fibonacci hashing (`key * 11400714819323198485ull >> 32`), linear probing, sentinel `UINT64_MAX`, 75% load factor auto-resize. Much better cache locality than `std::unordered_set`
- **Pre-computed walkable grid**: `bool abWalkable[256]` combines floor + static blocker checks into a single O(1) lookup
- **Pre-computed cat-at-cell lookup**: `int8_t aiCatAtCell[256]` gives O(1) wrong-color cat blocking checks
- **Inlined collision checking**: CanMoveShape and ComputeNewlyEliminatedCats are inlined in the hot BFS loop. Cross-validated against canonical `TilePuzzle_Rules` in `ZENITH_ASSERT` builds
- Inner BFS uses `std::unordered_set<uint64_t>` (re-used per drag, cleared between drags)

### Two-Tier Solver Strategy

The tool uses a two-tier BFS to find deeper solutions. Limits scale with the `--min-moves` target:

| Min-Moves | Fast Limit | Deep Limit | Deep Verif/Worker |
|-----------|-----------|------------|-------------------|
| < 10 | 500K | 3-5M | 5 |
| 10-19 | 1M | 5M | 5 |
| >= 20 | 2M | 7M | 1 |

1. **Fast pass**: All candidates verified at the fast limit. Quickly identifies solutions up to ~12-15 moves
2. **Deep pass**: Unsolvable candidates (those exceeding the fast limit) are re-verified at the deep limit. These candidates have large BFS state spaces that may contain deep solutions the fast solver couldn't verify

This is controlled by `DifficultyParams::uDeepSolverStateLimit` and `uMaxDeepVerificationsPerWorker`. The game runtime uses `uDeepSolverStateLimit = 0` (disabled) for fast single-round generation.

### Retry Loop

The tool wraps generation in a retry loop (`GenerateSingleLevel`):
- Each "round" calls `RandomizeDifficultyParams()` for fresh random parameters, then runs `GenerateLevel()` with a unique `seedOffset`
- The best result across all rounds is kept (with shape definitions copied to local storage to avoid dangling pointers)
- Rounds repeat until `--min-moves` target is met or `--timeout` expires
- Fast rounds (more retry rounds) produce better results than slow rounds (fewer retries) because more seed regions are explored

### Level Registry

The tool maintains a level registry (`LevelRegistry/` directory) for caching and reusing previously generated levels:

- **On startup**: Scans registry directory for `.tlvl` files, loads metadata, deduplicates structural duplicates
- **Per level**: Before generating, checks the registry for a cached level meeting `uParMoves >= uMinMoves` that hasn't been used in this run. On cache hit, copies the `.tlvl` and `.png` to the output directory (no generation needed)
- **After generation**: Saves newly generated levels to the registry as `{layoutHash:016llx}.tlvl` + `.png`
- **Deduplication**: On scan, entries with the same layout hash are loaded and compared via full structural signature. True duplicates have their registry files deleted

### Duplicate Detection

Each generated level is checked for structural duplicates before acceptance:

- **Color-independent layout hash**: Fibonacci hashing of grid dimensions, cell layout, sorted shape entries (position + type + draggable flag), and sorted cat entries (position + blocker flag). Colors are excluded so recolored variants of the same layout are detected
- **Collision resolution**: On hash collision, full `LevelLayoutSignature` comparison (grid, cells, shapes, cats) distinguishes true duplicates from hash collisions
- **Retry**: If a duplicate is detected, generation retries with a new seed (up to 10 retries per level)

## File Structure

| File | Purpose |
|------|---------|
| `TilePuzzleLevelGen.cpp` | Main entry point, CLI parsing, retry loop, validation, duplicate detection, registry |
| `TilePuzzleLevelMetadata.h` | `TilePuzzleLevelMetadata` struct (v2), `WriteMetadataAndLevel()`, `ReadMetadataFromFile()` |
| `TilePuzzleLevelData_Image.h` | PNG image rendering via `stb_image_write.h` |
| `TilePuzzleLevelGen_Analytics.h` | Run-wide analytics accumulation and `analytics.txt` output |

The actual generation logic lives in `Games/TilePuzzle/Components/TilePuzzle_LevelGenerator.h` (shared with the game runtime). Binary serialization is in `Games/TilePuzzle/Components/TilePuzzleLevelData_Serialize.h`.

## Output Formats

Each generated level produces 2 files:

- **`.tlvl`** — Binary format. Starts with metadata header (magic `0x4D455441` "META", version 2) containing level structure, generation provenance, and registry-matching fields. Followed by standard TPLV level data (magic `0x54504C56` "TPLV", version 2) with grid, shapes with inline definitions, and cats. Used by the game to load pre-generated levels
- **`.png`** — 32px/cell top-down image. Colors match in-game materials: grey floor (`{77,77,89}`), brown blockers (`{80,50,30}`), colored shapes (2px inset), brighter diamond markers for cats

Each run also produces an **`analytics.txt`** with aggregate statistics (written at end of run).

### Output Directory Structure

Output goes directly to the specified output directory (default `LEVELGEN_OUTPUT_DIR`):

```
<output_dir>/
    level_0001.tlvl / .png
    level_0002.tlvl / .png
    ...
    analytics.txt
```

Generated levels are also cached in the registry directory (`LEVELGEN_REGISTRY_DIR`):

```
TilePuzzleLevelGen/LevelRegistry/
    {layoutHash}.tlvl / .png
    ...
```

## Building and Running

```batch
# Regenerate solution (from Build/ directory)
Sharpmake_Build.bat

# Build (Release recommended for generation speed)
msbuild Build/zenith_win64.sln /p:Configuration=vs2022_Release_Win64_True /p:Platform=x64 -maxCpuCount:1

# Run with required arguments
tilepuzzlelevelgen.exe --count 10 --min-moves 12

# Custom output directory, seed, and timeout
tilepuzzlelevelgen.exe --count 50 --min-moves 15 --output path/to/output --seed 100 --timeout 900
```

**Important**: Always build with `-maxCpuCount:1` to avoid hanging compiler processes. Use Release builds for generation — Debug is significantly slower due to the heavy BFS solver.

### CLI Arguments

| Argument | Default | Description |
|----------|---------|-------------|
| `--count N` | *required* | Number of levels to generate (must be > 0) |
| `--min-moves N` | *required* | Minimum solver moves target (saves best on timeout) |
| `--output DIR` | `LEVELGEN_OUTPUT_DIR` | Output directory |
| `--timeout N` | 1800 (30 min) | Per-level time budget in seconds |
| `--seed N` | random (`rand()`) | Starting seed counter (each round increments) |

## Parameter Ranges

### Randomized Parameters (in `RandomizeDifficultyParams`, `TilePuzzleLevelGen.cpp`)

All generation parameters are randomized per retry round, with ranges biased by the `--min-moves` target. Each attempt within a round further randomizes within the per-round ranges via the `LevelGenerator`'s own internal distributions.

| Parameter | min-moves < 5 | 5-9 | 10-19 | >= 20 |
|-----------|--------------|------|-------|-------|
| Grid size | 5-10 | 6-9 | 8-10 | 9-10 |
| Colors | 2-5 | 3-5 | 4-5 | 5 |
| Cats/color | 1-2 | 1-2 | 2 | 2 |
| Blockers | 0-3 | 0-2 | 1-2 | 2 |
| Blocker cats | 0-2 (capped at blockers) | 0-2 | 0-2 | 1-2 (min 1 if blockers>=1) |
| Shape complexity | 1-4 | 2-4 | 2-4 | 2-4 |
| Scramble moves | 100-1500 | 200-1000 | 300-1000 | 300-1000 |
| Cond shapes | 0-3 (capped at colors-1) | 0-3 | 1-3 | 1-3 |
| Cond threshold | 0 or 1-5 | 0 or 1-5 | 0 or 1-4 | 0 or 1-4 |
| Solver limit | 500K | 500K | 1M | 2M |
| Deep solver limit | 3-5M | 3-5M | 5M | 7M |
| Deep verif/worker | 5 | 5 | 5 | 1 |
| Attempts/round | 3000 | 3000 | 2000 | 500 |
| Scramble mode | RANDOM | RANDOM | REVERSE_BFS or GUIDED | GUIDED |
| Min solver moves | 4 | 4 | 4 | 4 |
| Shapes per color | 1 | 1 | 1 | 1 |

**Constraints enforced after randomization**:
- `colors * catsPerColor + blockerCats <= 16` (solver bitmask limit)
- `blockerCats <= blockers`
- `conditionalShapes <= colors - 1`

### Game Defaults (in `TilePuzzle_LevelGenerator.h`)

The `static constexpr` values in `TilePuzzle_LevelGenerator.h` define defaults for in-game generation (8x8 grid, 3 colors, 3 cats/color, etc.). The tool's `RandomizeDifficultyParams` overrides these entirely. The `GetDifficultyForLevel()` function provides a 6-tier progressive difficulty curve for in-game use (Tutorial through Master).

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

### Blocker Types

The generator restricts blocker shapes to three types via the `aeBlockerTypes` array in the generation code. These produce compact obstacles that create walls/corridors reducing per-drag reachable positions:

| Type | Cells | Description |
|------|-------|-------------|
| `TILEPUZZLE_SHAPE_I` | 3 | Linear wall |
| `TILEPUZZLE_SHAPE_DOMINO` | 2 | Small wall |
| `TILEPUZZLE_SHAPE_SINGLE` | 1 | Point obstacle |

## Key Constraints and Tradeoffs

### Why 16-Bit Cat Mask (Not 32-Bit)

The solver state was repacked to support 5 shapes: 16-bit cat mask (bits [0..15]) + 5 x 8-bit positions (bits [16..55]). This limits cats to 16 maximum (sufficient for 5 colors x 3 = 15). The old 32-bit mask supported only 4 shapes.

### Why High Cat Counts Are Problematic

With 5 colors x 3 cats/color = 15 cats, the elimination bitmask has 2^15 = 32,768 states. This makes the BFS state space too large — solver success rate drops to near 0% at practical limits. With 2 cats/color = 10 cats, 2^10 = 1,024 elimination states — tractable.

### Dangling Pointer Prevention

When saving the best level across retry rounds, shape definitions must be copied to local storage (`axBestLevelDefs` vector) because subsequent `GenerateLevel()` calls clear the static definition vector. All shape instance pointers are then remapped to the local copies.

## Historical Benchmark Results

These benchmarks were gathered under specific fixed parameter configurations (before the switch to randomized parameters). They demonstrate the tool's capabilities and tradeoffs but exact rates may differ with current randomized settings.

### Run127 (5 Colors x 2 Cats, Solver Repack Validation)
- 1 level, min-moves 1, 500K solver, 300s timeout
- **12 moves** in 1 round (326s). solver=12, verify=12 — confirmed 5-shape packing is correct
- 55 successes / 2000 attempts = 2.8% success rate

### Run129 (5 Colors x 2 Cats, Restricted Blockers, Min 20)
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

## Pitfalls and Gotchas

### stdout Buffering in Background Processes

When running the generator as a background process, `fflush(stdout)` may not be sufficient to see real-time progress. **Monitor the output directory for new files instead of watching stdout.** The generator writes `.tlvl` and `.png` files on level completion.

### Release Build Required for Practical Use

Debug builds are ~5-10x slower due to the heavy BFS solver running thousands of times per level plus `ZENITH_ASSERT` cross-validation of inlined solver logic. Always use `vs2022_Release_Win64_True` for actual generation runs.

### RNG Determinism

Worker RNG seeds are derived from `levelNumber * 7919 + 104729 + workerIndex * 31337 + seedOffset * 999983`. The `seedOffset` (global seed counter) increments per retry round, ensuring each round explores a different seed space. Task system scheduling is non-deterministic, so exact results may vary between runs even with the same seed.

### Long-Running Processes

Each level can take up to the timeout duration (default 1800s / 30 min). The timeout is checked after each round completes, so actual time may exceed the timeout by one round duration. For multi-level runs, total time = levels x (timeout + ~1 round overhead).
