#pragma once
/**
 * TilePuzzle_Solver.h - Layered BFS level solver with player-move counting
 *
 * Uses a two-level BFS to find the minimum number of player-moves.
 * A "move" = picking up a shape and dragging it any distance. Each shape
 * pick counts as 1 move regardless of how many cells the shape travels.
 *
 * Outer BFS: state = packed uint64_t (shape positions + eliminated cat mask)
 *   Each transition = pick one shape and drag it (inner BFS).
 *   Uniform cost 1 per transition. Level-by-level BFS finds minimum moves.
 *
 * Inner BFS: given a shape to drag, explore all reachable positions
 *   and elimination states. Handles intermediate cat eliminations
 *   during a drag (cats eliminated as the shape passes over them).
 *   Inner state packed as uint64_t for fast hashing and cache locality.
 *
 * Outer state packing (uint64_t):
 *   Bits [0..15]:  eliminated cat bitmask (supports up to 16 cats)
 *   Bits [16..23]: shape 0 packed position (upper nibble=X, lower nibble=Y), 0xFF=removed
 *   Bits [24..31]: shape 1 packed position
 *   Bits [32..39]: shape 2 packed position
 *   Bits [40..47]: shape 3 packed position
 *   Bits [48..55]: shape 4 packed position
 *   Bits [56..63]: unused
 *   Supports up to 5 draggable shapes with grid dimensions up to 15x15.
 *
 * Optimizations:
 *   - Open-addressing hash set with Fibonacci hashing (outer visited)
 *   - Pre-computed walkable grid (combines floor + static blocker checks)
 *   - Pre-computed cat-at-cell lookup (O(1) wrong-color cat blocking)
 *   - Inlined collision checking (no function call overhead in hot path)
 *
 * Win condition: all cats eliminated. Shapes are automatically removed when
 * all cats of their color are eliminated (per-color pool logic).
 */

#include <vector>
#include <unordered_set>
#include <cstdint>
#include <cstring>

#include "TilePuzzle_Types.h"
#include "TilePuzzle_Rules.h"

static constexpr uint32_t s_uTilePuzzleMaxSolverStates = 2000000;
static constexpr uint32_t s_uMaxSolverShapes = 5;
static constexpr uint32_t s_uMaxSolverCats = 16;
static constexpr uint32_t s_uMaxGridCells = 256; // 16x16 max

// ============================================================================
// TilePuzzleFlatHashSet - Open-addressing hash set for uint64_t keys
//
// Uses Fibonacci hashing for good distribution, linear probing, and
// UINT64_MAX as sentinel (safe because valid states never equal UINT64_MAX).
// 75% max load factor with automatic growth.
// ============================================================================
class TilePuzzleFlatHashSet
{
public:
	void Reserve(size_t uMinCapacity)
	{
		size_t uCap = 16;
		while (uCap * 3 < uMinCapacity * 4)
			uCap *= 2;
		m_auKeys.assign(uCap, s_uEMPTY);
		m_uMask = uCap - 1;
		m_uSize = 0;
	}

	// Returns true if newly inserted, false if already present
	bool Insert(uint64_t uKey)
	{
		if (m_uSize * 4 >= m_auKeys.size() * 3)
			Grow();
		size_t uSlot = Hash(uKey) & m_uMask;
		while (true)
		{
			uint64_t uExisting = m_auKeys[uSlot];
			if (uExisting == s_uEMPTY)
			{
				m_auKeys[uSlot] = uKey;
				m_uSize++;
				return true;
			}
			if (uExisting == uKey)
				return false;
			uSlot = (uSlot + 1) & m_uMask;
		}
	}

	size_t Size() const { return m_uSize; }

private:
	static constexpr uint64_t s_uEMPTY = UINT64_MAX;

	static size_t Hash(uint64_t uKey)
	{
		return static_cast<size_t>((uKey * 11400714819323198485ull) >> 32);
	}

	void Grow()
	{
		std::vector<uint64_t> auOld = std::move(m_auKeys);
		size_t uNewCap = auOld.size() * 2;
		if (uNewCap < 16) uNewCap = 16;
		m_auKeys.assign(uNewCap, s_uEMPTY);
		m_uMask = uNewCap - 1;
		m_uSize = 0;
		for (uint64_t uKey : auOld)
		{
			if (uKey != s_uEMPTY)
				Insert(uKey);
		}
	}

	std::vector<uint64_t> m_auKeys;
	size_t m_uMask = 0;
	size_t m_uSize = 0;
};

// ============================================================================
// TilePuzzle_Solver
// ============================================================================
class TilePuzzle_Solver
{
public:
	/**
	 * SolveLevel - Find minimum player-moves to solve the level
	 *
	 * @param xLevel      The level data to solve
	 * @param uMaxStates  Maximum BFS states to explore before giving up
	 * @return Minimum player-moves to solve, or -1 if unsolvable/too complex
	 */
	static int32_t SolveLevel(const TilePuzzleLevelData& xLevel, uint32_t uMaxStates = s_uTilePuzzleMaxSolverStates)
	{
		// Pre-build cat state array once (cats never move)
		uint32_t uNumCats = static_cast<uint32_t>(xLevel.axCats.size());
		Zenith_Assert(uNumCats <= s_uMaxSolverCats, "Too many cats for solver (%u > %u)", uNumCats, s_uMaxSolverCats);

		uint32_t uGridWidth = xLevel.uGridWidth;
		uint32_t uGridHeight = xLevel.uGridHeight;

		int32_t aiCatX[s_uMaxSolverCats], aiCatY[s_uMaxSolverCats];
		TilePuzzleColor aeCatColors[s_uMaxSolverCats];
		bool abCatOnBlocker[s_uMaxSolverCats];
		for (uint32_t i = 0; i < uNumCats; ++i)
		{
			aiCatX[i] = xLevel.axCats[i].iGridX;
			aiCatY[i] = xLevel.axCats[i].iGridY;
			aeCatColors[i] = xLevel.axCats[i].eColor;
			abCatOnBlocker[i] = xLevel.axCats[i].bOnBlocker;
		}

		// Collect draggable shapes
		const TilePuzzleShapeDefinition* apxDefinitions[s_uMaxSolverShapes];
		TilePuzzleColor aeColors[s_uMaxSolverShapes];
		uint32_t auUnlockThresholds[s_uMaxSolverShapes];
		uint32_t auColorCatMasks[s_uMaxSolverShapes];
		int32_t aiInitX[s_uMaxSolverShapes], aiInitY[s_uMaxSolverShapes];
		uint32_t uNumDraggable = 0;

		for (size_t i = 0; i < xLevel.axShapes.size(); ++i)
		{
			if (xLevel.axShapes[i].pxDefinition && xLevel.axShapes[i].pxDefinition->bDraggable)
			{
				if (uNumDraggable >= s_uMaxSolverShapes)
					return -1;
				aiInitX[uNumDraggable] = xLevel.axShapes[i].iOriginX;
				aiInitY[uNumDraggable] = xLevel.axShapes[i].iOriginY;
				apxDefinitions[uNumDraggable] = xLevel.axShapes[i].pxDefinition;
				aeColors[uNumDraggable] = xLevel.axShapes[i].eColor;
				auUnlockThresholds[uNumDraggable] = xLevel.axShapes[i].uUnlockThreshold;
				uint32_t uMask = 0;
				for (uint32_t j = 0; j < uNumCats; ++j)
				{
					if (aeCatColors[j] == aeColors[uNumDraggable])
						uMask |= (1u << j);
				}
				auColorCatMasks[uNumDraggable] = uMask;
				uNumDraggable++;
			}
		}

		if (uNumDraggable == 0)
			return xLevel.axCats.empty() ? 0 : -1;

		// ================================================================
		// Pre-compute walkable grid (floor + no static blocker = true)
		// ================================================================
		bool abWalkable[s_uMaxGridCells];
		memset(abWalkable, 0, sizeof(abWalkable));
		for (uint32_t y = 0; y < uGridHeight; y++)
			for (uint32_t x = 0; x < uGridWidth; x++)
				abWalkable[y * uGridWidth + x] = (xLevel.aeCells[y * uGridWidth + x] == TILEPUZZLE_CELL_FLOOR);

		for (size_t i = 0; i < xLevel.axShapes.size(); ++i)
		{
			const TilePuzzleShapeInstance& xShape = xLevel.axShapes[i];
			if (!xShape.pxDefinition || xShape.pxDefinition->bDraggable)
				continue;
			for (size_t j = 0; j < xShape.pxDefinition->axCells.size(); ++j)
			{
				int32_t cx = xShape.iOriginX + xShape.pxDefinition->axCells[j].iX;
				int32_t cy = xShape.iOriginY + xShape.pxDefinition->axCells[j].iY;
				if (cx >= 0 && cy >= 0 &&
					static_cast<uint32_t>(cx) < uGridWidth &&
					static_cast<uint32_t>(cy) < uGridHeight)
				{
					abWalkable[cy * uGridWidth + cx] = false;
				}
			}
		}

		// ================================================================
		// Pre-compute cat-at-cell lookup (-1 = no cat)
		// ================================================================
		int8_t aiCatAtCell[s_uMaxGridCells];
		memset(aiCatAtCell, -1, sizeof(aiCatAtCell));
		for (uint32_t i = 0; i < uNumCats; ++i)
		{
			uint32_t uIdx = static_cast<uint32_t>(aiCatY[i]) * uGridWidth + static_cast<uint32_t>(aiCatX[i]);
			aiCatAtCell[uIdx] = static_cast<int8_t>(i);
		}

		// ================================================================
		// BFS
		// ================================================================

		uint64_t uInitPacked = PackOuterState(aiInitX, aiInitY, 0, auColorCatMasks, uNumDraggable);
		if (TilePuzzle_Rules::AreAllCatsEliminated(0, uNumCats))
			return 0;

		std::vector<uint64_t> axCurrentLevel, axNextLevel;
		axCurrentLevel.reserve(4096);
		axNextLevel.reserve(4096);
		TilePuzzleFlatHashSet xOuterVisited;
		xOuterVisited.Reserve(8192);

		axCurrentLevel.push_back(uInitPacked);
		xOuterVisited.Insert(uInitPacked);

		// Inner BFS containers (pre-allocated, reused)
		std::vector<uint64_t> axInnerQueue;
		std::unordered_set<uint64_t> xInnerVisited;
		axInnerQueue.reserve(512);
		xInnerVisited.reserve(512);

		int32_t aiDeltaX[] = {0, 0, -1, 1};
		int32_t aiDeltaY[] = {-1, 1, 0, 0};
		int32_t iMoves = 0;

		while (!axCurrentLevel.empty() && xOuterVisited.Size() < uMaxStates)
		{
			axNextLevel.clear();

			for (size_t uStateIdx = 0; uStateIdx < axCurrentLevel.size(); ++uStateIdx)
			{
				uint64_t uPacked = axCurrentLevel[uStateIdx];
				uint32_t uOuterMask = GetElimMask(uPacked);

				// Extract positions for non-dragged shape collision checking
				int32_t aiPosX[s_uMaxSolverShapes], aiPosY[s_uMaxSolverShapes];
				for (uint32_t i = 0; i < uNumDraggable; ++i)
					GetShapePos(uPacked, i, aiPosX[i], aiPosY[i]);

				for (uint32_t uDragShape = 0; uDragShape < uNumDraggable; ++uDragShape)
				{
					// Skip removed shapes
					if ((auColorCatMasks[uDragShape] & ~uOuterMask) == 0)
						continue;

					// Check unlock threshold
					if (auUnlockThresholds[uDragShape] > 0)
					{
						uint32_t uEliminatedCount = 0;
						uint32_t uTmp = uOuterMask;
						while (uTmp) { uEliminatedCount += uTmp & 1u; uTmp >>= 1; }
						if (uEliminatedCount < auUnlockThresholds[uDragShape])
							continue;
					}

					// Inner BFS: explore all positions reachable by dragging this shape
					axInnerQueue.clear();
					xInnerVisited.clear();

					int32_t iStartX = aiPosX[uDragShape];
					int32_t iStartY = aiPosY[uDragShape];
					uint64_t uStartKey = PackInnerState(iStartX, iStartY, uOuterMask);
					axInnerQueue.push_back(uStartKey);
					xInnerVisited.insert(uStartKey);

					size_t uInnerFront = 0;
					while (uInnerFront < axInnerQueue.size())
					{
						uint64_t uInnerKey = axInnerQueue[uInnerFront++];
						int32_t iCurX, iCurY;
						uint32_t uCurMask;
						UnpackInnerState(uInnerKey, iCurX, iCurY, uCurMask);

						// Enqueue new outer state for non-start positions
						if (uInnerKey != uStartKey)
						{
							uint64_t uNewOuter = uPacked;
							uNewOuter = SetShapePos(uNewOuter, uDragShape, iCurX, iCurY);
							uNewOuter = SetElimMask(uNewOuter, uCurMask);
							for (uint32_t i = 0; i < uNumDraggable; ++i)
							{
								if ((auColorCatMasks[i] & ~uCurMask) == 0 && !IsShapeRemoved(uNewOuter, i))
									uNewOuter = SetShapeRemoved(uNewOuter, i);
							}

							if (xOuterVisited.Insert(uNewOuter))
							{
								if (TilePuzzle_Rules::AreAllCatsEliminated(uCurMask, uNumCats))
									return iMoves + 1;

								axNextLevel.push_back(uNewOuter);

								if (xOuterVisited.Size() >= uMaxStates)
									return -1;
							}
						}

						// ================================================
						// Explore 4 directions with inlined collision checks
						// ================================================
						const std::vector<TilePuzzleCellOffset>& axMovingCells = apxDefinitions[uDragShape]->axCells;
						size_t uNumMovingCells = axMovingCells.size();

						for (int32_t iDir = 0; iDir < 4; ++iDir)
						{
							int32_t iNewX = iCurX + aiDeltaX[iDir];
							int32_t iNewY = iCurY + aiDeltaY[iDir];

							// --- Inline CanMoveShape (see TilePuzzle_Rules::CanMoveShape) ---
						// Cross-validated in ZENITH_ASSERT builds below
							bool bValid = true;
							for (size_t c = 0; c < uNumMovingCells; ++c)
							{
								int32_t iCellX = iNewX + axMovingCells[c].iX;
								int32_t iCellY = iNewY + axMovingCells[c].iY;

								// Bounds check
								if (iCellX < 0 || iCellY < 0 ||
									static_cast<uint32_t>(iCellX) >= uGridWidth ||
									static_cast<uint32_t>(iCellY) >= uGridHeight)
								{
									bValid = false;
									break;
								}

								uint32_t uCellIdx = static_cast<uint32_t>(iCellY) * uGridWidth + static_cast<uint32_t>(iCellX);

								// Pre-computed walkable check (floor + no static blocker)
								if (!abWalkable[uCellIdx])
								{
									bValid = false;
									break;
								}

								// Other draggable shapes collision
								for (uint32_t si = 0; si < uNumDraggable; ++si)
								{
									if (si == uDragShape) continue;
									if ((auColorCatMasks[si] & ~uCurMask) == 0) continue;

									const std::vector<TilePuzzleCellOffset>& axOtherCells = apxDefinitions[si]->axCells;
									for (size_t sc = 0; sc < axOtherCells.size(); ++sc)
									{
										if (aiPosX[si] + axOtherCells[sc].iX == iCellX &&
											aiPosY[si] + axOtherCells[sc].iY == iCellY)
										{
											bValid = false;
											break;
										}
									}
									if (!bValid) break;
								}
								if (!bValid) break;

								// Wrong-color cat check (O(1) lookup)
								int8_t iCatIdx = aiCatAtCell[uCellIdx];
								if (iCatIdx >= 0 &&
									!(uCurMask & (1u << static_cast<uint32_t>(iCatIdx))) &&
									aeCatColors[iCatIdx] != aeColors[uDragShape])
								{
									bValid = false;
									break;
								}
							}

							if (!bValid) continue;

							// --- Inline ComputeNewlyEliminatedCats (see TilePuzzle_Rules::ComputeNewlyEliminatedCats) ---
							// Cross-validated in ZENITH_ASSERT builds below
							uint32_t uNewlyEliminated = 0;
							for (uint32_t si = 0; si < uNumDraggable; ++si)
							{
								if ((auColorCatMasks[si] & ~uCurMask) == 0) continue;

								int32_t iShapeX = (si == uDragShape) ? iNewX : aiPosX[si];
								int32_t iShapeY = (si == uDragShape) ? iNewY : aiPosY[si];
								const std::vector<TilePuzzleCellOffset>& axShapeCells = apxDefinitions[si]->axCells;

								for (size_t c = 0; c < axShapeCells.size(); ++c)
								{
									int32_t iCellX = iShapeX + axShapeCells[c].iX;
									int32_t iCellY = iShapeY + axShapeCells[c].iY;

									for (uint32_t ci = 0; ci < uNumCats; ++ci)
									{
										if ((uCurMask | uNewlyEliminated) & (1u << ci)) continue;
										if (aeCatColors[ci] != aeColors[si]) continue;

										if (abCatOnBlocker[ci])
										{
											int32_t iDX = iCellX - aiCatX[ci];
											int32_t iDY = iCellY - aiCatY[ci];
											if ((iDX == 0 && (iDY == 1 || iDY == -1)) ||
												(iDY == 0 && (iDX == 1 || iDX == -1)))
											{
												uNewlyEliminated |= (1u << ci);
											}
										}
										else
										{
											if (aiCatX[ci] == iCellX && aiCatY[ci] == iCellY)
												uNewlyEliminated |= (1u << ci);
										}
									}
								}
							}

							// --- Cross-validate inlined logic against canonical Rules ---
#ifdef ZENITH_ASSERT
							{
								TilePuzzle_Rules::ShapeState axValidationShapes[s_uMaxSolverShapes];
								for (uint32_t vi = 0; vi < uNumDraggable; ++vi)
								{
									axValidationShapes[vi].pxDefinition = apxDefinitions[vi];
									axValidationShapes[vi].iOriginX = (vi == uDragShape) ? iCurX : aiPosX[vi];
									axValidationShapes[vi].iOriginY = (vi == uDragShape) ? iCurY : aiPosY[vi];
									axValidationShapes[vi].eColor = aeColors[vi];
									axValidationShapes[vi].uUnlockThreshold = auUnlockThresholds[vi];
									// Mark removed shapes (all same-color cats eliminated)
									// Skip the moving shape: it's still being dragged even if
									// it eliminated all its own cats during this inner BFS
									if (vi != uDragShape && (auColorCatMasks[vi] & ~uCurMask) == 0)
										axValidationShapes[vi].pxDefinition = nullptr;
								}

								TilePuzzle_Rules::CatState axValidationCats[s_uMaxSolverCats];
								for (uint32_t vi = 0; vi < uNumCats; ++vi)
								{
									axValidationCats[vi].iGridX = aiCatX[vi];
									axValidationCats[vi].iGridY = aiCatY[vi];
									axValidationCats[vi].eColor = aeCatColors[vi];
									axValidationCats[vi].bOnBlocker = abCatOnBlocker[vi];
								}

								// Validate CanMoveShape
								bool bRulesValid = TilePuzzle_Rules::CanMoveShape(
									xLevel,
									axValidationShapes, uNumDraggable,
									uDragShape, iNewX, iNewY,
									axValidationCats, uNumCats,
									uCurMask);
								Zenith_Assert(bRulesValid == bValid,
									"SolveLevel: inlined CanMoveShape disagrees with Rules");

								// Validate ComputeNewlyEliminatedCats
								axValidationShapes[uDragShape].iOriginX = iNewX;
								axValidationShapes[uDragShape].iOriginY = iNewY;
								uint32_t uRulesElim = TilePuzzle_Rules::ComputeNewlyEliminatedCats(
									axValidationShapes, uNumDraggable,
									axValidationCats, uNumCats,
									uCurMask);
								Zenith_Assert(uRulesElim == uNewlyEliminated,
									"SolveLevel: inlined ComputeNewlyEliminatedCats disagrees with Rules");
							}
#endif
							uint64_t uNextKey = PackInnerState(iNewX, iNewY, uCurMask | uNewlyEliminated);
							if (xInnerVisited.insert(uNextKey).second)
								axInnerQueue.push_back(uNextKey);
						}
					}
				}
			}

			axCurrentLevel.swap(axNextLevel);
			iMoves++;
		}

		return -1;
	}

	static bool IsSolvable(const TilePuzzleLevelData& xLevel)
	{
		return SolveLevel(xLevel) >= 0;
	}

	/**
	 * SolveLevelWithPath - Find minimum-move solution AND extract the full move sequence.
	 *
	 * Same BFS as SolveLevel but tracks parent pointers so the solution path
	 * can be reconstructed. Each move in the output = one shape drag (pick shape,
	 * slide to destination). Uses more memory than SolveLevel due to parent tracking.
	 *
	 * @param xLevel          The level data to solve
	 * @param axPathOut       Output: ordered sequence of solution moves (cleared first)
	 * @param auDraggableMap  If non-null, receives mapping from draggable index → full shapes array index
	 * @param uMaxStates      Maximum BFS states to explore before giving up
	 * @return Minimum player-moves to solve, or -1 if unsolvable/too complex
	 */
	static int32_t SolveLevelWithPath(
		const TilePuzzleLevelData& xLevel,
		std::vector<TilePuzzleSolutionMove>& axPathOut,
		uint32_t uMaxStates = s_uTilePuzzleMaxSolverStates)
	{
		axPathOut.clear();

		// ================================================================
		// Setup (same as SolveLevel)
		// ================================================================
		uint32_t uNumCats = static_cast<uint32_t>(xLevel.axCats.size());
		if (uNumCats > s_uMaxSolverCats) return -1;

		uint32_t uGridWidth = xLevel.uGridWidth;
		uint32_t uGridHeight = xLevel.uGridHeight;

		int32_t aiCatX[s_uMaxSolverCats], aiCatY[s_uMaxSolverCats];
		TilePuzzleColor aeCatColors[s_uMaxSolverCats];
		bool abCatOnBlocker[s_uMaxSolverCats];
		for (uint32_t i = 0; i < uNumCats; ++i)
		{
			aiCatX[i] = xLevel.axCats[i].iGridX;
			aiCatY[i] = xLevel.axCats[i].iGridY;
			aeCatColors[i] = xLevel.axCats[i].eColor;
			abCatOnBlocker[i] = xLevel.axCats[i].bOnBlocker;
		}

		// Collect draggable shapes + build mapping to full shapes array
		const TilePuzzleShapeDefinition* apxDefinitions[s_uMaxSolverShapes];
		TilePuzzleColor aeColors[s_uMaxSolverShapes];
		uint32_t auUnlockThresholds[s_uMaxSolverShapes];
		uint32_t auColorCatMasks[s_uMaxSolverShapes];
		int32_t aiInitX[s_uMaxSolverShapes], aiInitY[s_uMaxSolverShapes];
		uint32_t auDraggableToFull[s_uMaxSolverShapes]; // Maps draggable index → full shapes array index
		uint32_t uNumDraggable = 0;

		for (size_t i = 0; i < xLevel.axShapes.size(); ++i)
		{
			if (xLevel.axShapes[i].pxDefinition && xLevel.axShapes[i].pxDefinition->bDraggable)
			{
				if (uNumDraggable >= s_uMaxSolverShapes)
					return -1;
				aiInitX[uNumDraggable] = xLevel.axShapes[i].iOriginX;
				aiInitY[uNumDraggable] = xLevel.axShapes[i].iOriginY;
				apxDefinitions[uNumDraggable] = xLevel.axShapes[i].pxDefinition;
				aeColors[uNumDraggable] = xLevel.axShapes[i].eColor;
				auUnlockThresholds[uNumDraggable] = xLevel.axShapes[i].uUnlockThreshold;
				auDraggableToFull[uNumDraggable] = static_cast<uint32_t>(i);
				uint32_t uMask = 0;
				for (uint32_t j = 0; j < uNumCats; ++j)
				{
					if (aeCatColors[j] == aeColors[uNumDraggable])
						uMask |= (1u << j);
				}
				auColorCatMasks[uNumDraggable] = uMask;
				uNumDraggable++;
			}
		}

		if (uNumDraggable == 0)
			return xLevel.axCats.empty() ? 0 : -1;

		// Pre-compute walkable grid
		bool abWalkable[s_uMaxGridCells];
		memset(abWalkable, 0, sizeof(abWalkable));
		for (uint32_t y = 0; y < uGridHeight; y++)
			for (uint32_t x = 0; x < uGridWidth; x++)
				abWalkable[y * uGridWidth + x] = (xLevel.aeCells[y * uGridWidth + x] == TILEPUZZLE_CELL_FLOOR);

		for (size_t i = 0; i < xLevel.axShapes.size(); ++i)
		{
			const TilePuzzleShapeInstance& xShape = xLevel.axShapes[i];
			if (!xShape.pxDefinition || xShape.pxDefinition->bDraggable)
				continue;
			for (size_t j = 0; j < xShape.pxDefinition->axCells.size(); ++j)
			{
				int32_t cx = xShape.iOriginX + xShape.pxDefinition->axCells[j].iX;
				int32_t cy = xShape.iOriginY + xShape.pxDefinition->axCells[j].iY;
				if (cx >= 0 && cy >= 0 &&
					static_cast<uint32_t>(cx) < uGridWidth &&
					static_cast<uint32_t>(cy) < uGridHeight)
				{
					abWalkable[cy * uGridWidth + cx] = false;
				}
			}
		}

		// Pre-compute cat-at-cell lookup
		int8_t aiCatAtCell[s_uMaxGridCells];
		memset(aiCatAtCell, -1, sizeof(aiCatAtCell));
		for (uint32_t i = 0; i < uNumCats; ++i)
		{
			uint32_t uIdx = static_cast<uint32_t>(aiCatY[i]) * uGridWidth + static_cast<uint32_t>(aiCatX[i]);
			aiCatAtCell[uIdx] = static_cast<int8_t>(i);
		}

		// ================================================================
		// BFS with parent tracking
		// ================================================================

		// Each state entry stores: packed state + parent info for path extraction
		struct StateEntry
		{
			uint64_t uPacked;
			uint32_t uParentIdx;   // UINT32_MAX = root
			uint32_t uDragShape;   // Which draggable shape was dragged
			int32_t iEndX, iEndY;  // Where the shape ended up
		};

		std::vector<StateEntry> axAllStates;
		axAllStates.reserve(8192);

		// Hash map: packed state → index in axAllStates (for dedup)
		std::unordered_map<uint64_t, uint32_t> xStateMap; // #TODO: Replace with engine hash map
		xStateMap.reserve(8192);

		uint64_t uInitPacked = PackOuterState(aiInitX, aiInitY, 0, auColorCatMasks, uNumDraggable);
		if (TilePuzzle_Rules::AreAllCatsEliminated(0, uNumCats))
			return 0;

		axAllStates.push_back({uInitPacked, UINT32_MAX, 0, 0, 0});
		xStateMap[uInitPacked] = 0;

		// BFS uses indices into axAllStates
		std::vector<uint32_t> axCurrentLevel, axNextLevel;
		axCurrentLevel.reserve(4096);
		axNextLevel.reserve(4096);
		axCurrentLevel.push_back(0);

		// Inner BFS containers (pre-allocated, reused)
		std::vector<uint64_t> axInnerQueue;
		std::unordered_set<uint64_t> xInnerVisited;
		axInnerQueue.reserve(512);
		xInnerVisited.reserve(512);

		int32_t aiDeltaX[] = {0, 0, -1, 1};
		int32_t aiDeltaY[] = {-1, 1, 0, 0};
		int32_t iMoves = 0;

		while (!axCurrentLevel.empty() && axAllStates.size() < uMaxStates)
		{
			axNextLevel.clear();

			for (size_t uIdx = 0; uIdx < axCurrentLevel.size(); ++uIdx)
			{
				uint32_t uStateIdx = axCurrentLevel[uIdx];
				uint64_t uPacked = axAllStates[uStateIdx].uPacked;
				uint32_t uOuterMask = GetElimMask(uPacked);

				int32_t aiPosX[s_uMaxSolverShapes], aiPosY[s_uMaxSolverShapes];
				for (uint32_t i = 0; i < uNumDraggable; ++i)
					GetShapePos(uPacked, i, aiPosX[i], aiPosY[i]);

				for (uint32_t uDragShape = 0; uDragShape < uNumDraggable; ++uDragShape)
				{
					if ((auColorCatMasks[uDragShape] & ~uOuterMask) == 0)
						continue;

					if (auUnlockThresholds[uDragShape] > 0)
					{
						uint32_t uEliminatedCount = 0;
						uint32_t uTmp = uOuterMask;
						while (uTmp) { uEliminatedCount += uTmp & 1u; uTmp >>= 1; }
						if (uEliminatedCount < auUnlockThresholds[uDragShape])
							continue;
					}

					// Inner BFS: explore all positions reachable by dragging this shape
					axInnerQueue.clear();
					xInnerVisited.clear();

					int32_t iStartX = aiPosX[uDragShape];
					int32_t iStartY = aiPosY[uDragShape];
					uint64_t uStartKey = PackInnerState(iStartX, iStartY, uOuterMask);
					axInnerQueue.push_back(uStartKey);
					xInnerVisited.insert(uStartKey);

					size_t uInnerFront = 0;
					while (uInnerFront < axInnerQueue.size())
					{
						uint64_t uInnerKey = axInnerQueue[uInnerFront++];
						int32_t iCurX, iCurY;
						uint32_t uCurMask;
						UnpackInnerState(uInnerKey, iCurX, iCurY, uCurMask);

						// Enqueue new outer state for non-start positions
						if (uInnerKey != uStartKey)
						{
							uint64_t uNewOuter = uPacked;
							uNewOuter = SetShapePos(uNewOuter, uDragShape, iCurX, iCurY);
							uNewOuter = SetElimMask(uNewOuter, uCurMask);
							for (uint32_t i = 0; i < uNumDraggable; ++i)
							{
								if ((auColorCatMasks[i] & ~uCurMask) == 0 && !IsShapeRemoved(uNewOuter, i))
									uNewOuter = SetShapeRemoved(uNewOuter, i);
							}

							if (xStateMap.find(uNewOuter) == xStateMap.end())
							{
								uint32_t uNewIdx = static_cast<uint32_t>(axAllStates.size());
								axAllStates.push_back({uNewOuter, uStateIdx, uDragShape, iCurX, iCurY});
								xStateMap[uNewOuter] = uNewIdx;

								if (TilePuzzle_Rules::AreAllCatsEliminated(uCurMask, uNumCats))
								{
									// Solution found - extract path with elimination masks
									uint32_t uTraceIdx = uNewIdx;
									while (axAllStates[uTraceIdx].uParentIdx != UINT32_MAX)
									{
										const StateEntry& xEntry = axAllStates[uTraceIdx];
										TilePuzzleSolutionMove xMove;
										xMove.uShapeIndex = auDraggableToFull[xEntry.uDragShape];
										xMove.iEndX = xEntry.iEndX;
										xMove.iEndY = xEntry.iEndY;
										xMove.uExpectedElimMask = GetElimMask(xEntry.uPacked);
										axPathOut.push_back(xMove);
										uTraceIdx = xEntry.uParentIdx;
									}
									std::reverse(axPathOut.begin(), axPathOut.end());
									return iMoves + 1;
								}

								axNextLevel.push_back(uNewIdx);

								if (axAllStates.size() >= uMaxStates)
									return -1;
							}
						}

						// Explore 4 directions (same inlined logic as SolveLevel)
						const std::vector<TilePuzzleCellOffset>& axMovingCells = apxDefinitions[uDragShape]->axCells;
						size_t uNumMovingCells = axMovingCells.size();

						for (int32_t iDir = 0; iDir < 4; ++iDir)
						{
							int32_t iNewX = iCurX + aiDeltaX[iDir];
							int32_t iNewY = iCurY + aiDeltaY[iDir];

							bool bValid = true;
							for (size_t c = 0; c < uNumMovingCells; ++c)
							{
								int32_t iCellX = iNewX + axMovingCells[c].iX;
								int32_t iCellY = iNewY + axMovingCells[c].iY;

								if (iCellX < 0 || iCellY < 0 ||
									static_cast<uint32_t>(iCellX) >= uGridWidth ||
									static_cast<uint32_t>(iCellY) >= uGridHeight)
								{ bValid = false; break; }

								uint32_t uCellIdx = static_cast<uint32_t>(iCellY) * uGridWidth + static_cast<uint32_t>(iCellX);
								if (!abWalkable[uCellIdx])
								{ bValid = false; break; }

								for (uint32_t si = 0; si < uNumDraggable; ++si)
								{
									if (si == uDragShape) continue;
									if ((auColorCatMasks[si] & ~uCurMask) == 0) continue;
									const std::vector<TilePuzzleCellOffset>& axOtherCells = apxDefinitions[si]->axCells;
									for (size_t sc = 0; sc < axOtherCells.size(); ++sc)
									{
										if (aiPosX[si] + axOtherCells[sc].iX == iCellX &&
											aiPosY[si] + axOtherCells[sc].iY == iCellY)
										{ bValid = false; break; }
									}
									if (!bValid) break;
								}
								if (!bValid) break;

								int8_t iCatIdx = aiCatAtCell[uCellIdx];
								if (iCatIdx >= 0 &&
									!(uCurMask & (1u << static_cast<uint32_t>(iCatIdx))) &&
									aeCatColors[iCatIdx] != aeColors[uDragShape])
								{ bValid = false; break; }
							}

							if (!bValid) continue;

							// Compute newly eliminated cats
							uint32_t uNewlyEliminated = 0;
							for (uint32_t si = 0; si < uNumDraggable; ++si)
							{
								if ((auColorCatMasks[si] & ~uCurMask) == 0) continue;
								int32_t iShapeX = (si == uDragShape) ? iNewX : aiPosX[si];
								int32_t iShapeY = (si == uDragShape) ? iNewY : aiPosY[si];
								const std::vector<TilePuzzleCellOffset>& axShapeCells = apxDefinitions[si]->axCells;
								for (size_t c = 0; c < axShapeCells.size(); ++c)
								{
									int32_t iCellX = iShapeX + axShapeCells[c].iX;
									int32_t iCellY = iShapeY + axShapeCells[c].iY;
									for (uint32_t ci = 0; ci < uNumCats; ++ci)
									{
										if ((uCurMask | uNewlyEliminated) & (1u << ci)) continue;
										if (aeCatColors[ci] != aeColors[si]) continue;
										if (abCatOnBlocker[ci])
										{
											int32_t iDX = iCellX - aiCatX[ci];
											int32_t iDY = iCellY - aiCatY[ci];
											if ((iDX == 0 && (iDY == 1 || iDY == -1)) ||
												(iDY == 0 && (iDX == 1 || iDX == -1)))
												uNewlyEliminated |= (1u << ci);
										}
										else
										{
											if (aiCatX[ci] == iCellX && aiCatY[ci] == iCellY)
												uNewlyEliminated |= (1u << ci);
										}
									}
								}
							}

							uint64_t uNextKey = PackInnerState(iNewX, iNewY, uCurMask | uNewlyEliminated);
							if (xInnerVisited.insert(uNextKey).second)
								axInnerQueue.push_back(uNextKey);
						}
					}
				}
			}

			axCurrentLevel.swap(axNextLevel);
			iMoves++;
		}

		return -1;
	}

private:
	// ========================================================================
	// Outer state packing helpers
	// ========================================================================

	static uint64_t PackOuterState(const int32_t* aiX, const int32_t* aiY, uint32_t uElimMask,
		const uint32_t* auColorCatMasks, uint32_t uNumDraggable)
	{
		uint64_t uPacked = static_cast<uint64_t>(uElimMask & 0xFFFF);
		for (uint32_t i = 0; i < s_uMaxSolverShapes; ++i)
		{
			uint32_t uPosByte;
			if (i >= uNumDraggable)
				uPosByte = 0xFF;
			else if ((auColorCatMasks[i] & ~uElimMask) == 0)
				uPosByte = 0xFF;
			else
				uPosByte = ((static_cast<uint32_t>(aiX[i]) & 0xF) << 4) | (static_cast<uint32_t>(aiY[i]) & 0xF);
			uPacked |= static_cast<uint64_t>(uPosByte) << (16 + i * 8);
		}
		return uPacked;
	}

	static uint32_t GetElimMask(uint64_t uPacked)
	{
		return static_cast<uint32_t>(uPacked & 0xFFFF);
	}

	static uint64_t SetElimMask(uint64_t uPacked, uint32_t uMask)
	{
		return (uPacked & ~0xFFFFull) | static_cast<uint64_t>(uMask & 0xFFFF);
	}

	static void GetShapePos(uint64_t uPacked, uint32_t uShapeIdx, int32_t& iX, int32_t& iY)
	{
		uint32_t uByte = static_cast<uint32_t>((uPacked >> (16 + uShapeIdx * 8)) & 0xFF);
		if (uByte == 0xFF) { iX = -1; iY = -1; }
		else { iX = static_cast<int32_t>((uByte >> 4) & 0xF); iY = static_cast<int32_t>(uByte & 0xF); }
	}

	static uint64_t SetShapePos(uint64_t uPacked, uint32_t uShapeIdx, int32_t iX, int32_t iY)
	{
		uint32_t uByte = ((static_cast<uint32_t>(iX) & 0xF) << 4) | (static_cast<uint32_t>(iY) & 0xF);
		uint64_t uClearMask = ~(0xFFull << (16 + uShapeIdx * 8));
		return (uPacked & uClearMask) | (static_cast<uint64_t>(uByte) << (16 + uShapeIdx * 8));
	}

	static bool IsShapeRemoved(uint64_t uPacked, uint32_t uShapeIdx)
	{
		return ((uPacked >> (16 + uShapeIdx * 8)) & 0xFF) == 0xFF;
	}

	static uint64_t SetShapeRemoved(uint64_t uPacked, uint32_t uShapeIdx)
	{
		uint64_t uClearMask = ~(0xFFull << (16 + uShapeIdx * 8));
		return (uPacked & uClearMask) | (0xFFull << (16 + uShapeIdx * 8));
	}

	// ========================================================================
	// Inner state packing helpers
	// ========================================================================

	static uint64_t PackInnerState(int32_t iX, int32_t iY, uint32_t uElimMask)
	{
		return (static_cast<uint64_t>(iX & 0xFF) << 40) |
			(static_cast<uint64_t>(iY & 0xFF) << 32) |
			static_cast<uint64_t>(uElimMask);
	}

	static void UnpackInnerState(uint64_t uKey, int32_t& iX, int32_t& iY, uint32_t& uElimMask)
	{
		iX = static_cast<int32_t>((uKey >> 40) & 0xFF);
		iY = static_cast<int32_t>((uKey >> 32) & 0xFF);
		uElimMask = static_cast<uint32_t>(uKey);
	}
};
