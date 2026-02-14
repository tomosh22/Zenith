#pragma once

#include <vector>
#include <cstdint>
#include "EntityComponent/Zenith_Entity.h"

// ============================================================================
// TilePuzzle Game Types and Structures
// ============================================================================

// Cell types for the floor layer
enum TilePuzzleCellType : uint8_t
{
	TILEPUZZLE_CELL_EMPTY = 0,   // Out of bounds / void
	TILEPUZZLE_CELL_FLOOR,       // Valid floor cell
	TILEPUZZLE_CELL_COUNT
};

// Colors for shapes and cats
enum TilePuzzleColor : uint8_t
{
	TILEPUZZLE_COLOR_RED = 0,
	TILEPUZZLE_COLOR_GREEN,
	TILEPUZZLE_COLOR_BLUE,
	TILEPUZZLE_COLOR_YELLOW,
	TILEPUZZLE_COLOR_COUNT,
	TILEPUZZLE_COLOR_NONE        // For static blockers
};

// Movement directions
enum TilePuzzleDirection : uint8_t
{
	TILEPUZZLE_DIR_UP = 0,
	TILEPUZZLE_DIR_DOWN,
	TILEPUZZLE_DIR_LEFT,
	TILEPUZZLE_DIR_RIGHT,
	TILEPUZZLE_DIR_NONE
};

// Game states
enum TilePuzzleGameState : uint8_t
{
	TILEPUZZLE_STATE_MAIN_MENU = 0,
	TILEPUZZLE_STATE_PLAYING,
	TILEPUZZLE_STATE_SHAPE_SLIDING,
	TILEPUZZLE_STATE_CHECK_ELIMINATION,
	TILEPUZZLE_STATE_LEVEL_COMPLETE,
	TILEPUZZLE_STATE_GENERATING,
	TILEPUZZLE_STATE_LEVEL_SELECT
};

// Shape types (polyomino templates)
enum TilePuzzleShapeType : uint8_t
{
	TILEPUZZLE_SHAPE_SINGLE = 0,   // Single cell: [(0,0)]
	TILEPUZZLE_SHAPE_DOMINO,       // 2 cells horizontal: [(0,0), (1,0)]
	TILEPUZZLE_SHAPE_L,            // L-shape: [(0,0), (1,0), (2,0), (2,1)]
	TILEPUZZLE_SHAPE_T,            // T-shape: [(0,0), (1,0), (2,0), (1,1)]
	TILEPUZZLE_SHAPE_I,            // I-shape (3): [(0,0), (1,0), (2,0)]
	TILEPUZZLE_SHAPE_S,            // S-shape: [(1,0), (2,0), (0,1), (1,1)]
	TILEPUZZLE_SHAPE_Z,            // Z-shape: [(0,0), (1,0), (1,1), (2,1)]
	TILEPUZZLE_SHAPE_O,            // 2x2 square: [(0,0), (1,0), (0,1), (1,1)]
	TILEPUZZLE_SHAPE_COUNT
};

// Cell offset for shape definition
struct TilePuzzleCellOffset
{
	int32_t iX;
	int32_t iY;
};

// Shape definition (template)
struct TilePuzzleShapeDefinition
{
	TilePuzzleShapeType eType;
	std::vector<TilePuzzleCellOffset> axCells;  // Relative offsets from origin
	bool bDraggable;                             // true = player can move, false = static blocker
};

// Shape instance (runtime)
struct TilePuzzleShapeInstance
{
	const TilePuzzleShapeDefinition* pxDefinition;
	int32_t iOriginX;                            // Grid position X
	int32_t iOriginY;                            // Grid position Y
	TilePuzzleColor eColor;                      // Color (NONE for blockers)
	uint32_t uUnlockThreshold = 0;              // If > 0, shape can only move after this many cats are eliminated
	std::vector<Zenith_EntityID> axCubeEntityIDs; // Visual entities for each cell
};

// Cat data
struct TilePuzzleCatData
{
	TilePuzzleColor eColor;
	int32_t iGridX;
	int32_t iGridY;
	Zenith_EntityID uEntityID;
	bool bEliminated;
	bool bOnBlocker = false;     // If true, cat sits on a blocker cell and is eliminated by adjacency
	float fEliminationProgress;  // 0.0 to 1.0 for animation
};

// Level data
struct TilePuzzleLevelData
{
	uint32_t uGridWidth;
	uint32_t uGridHeight;
	std::vector<TilePuzzleCellType> aeCells;     // Floor layer (row-major)
	std::vector<TilePuzzleShapeInstance> axShapes;
	std::vector<TilePuzzleCatData> axCats;
	uint32_t uMinimumMoves;                      // For scoring
};

// ============================================================================
// Predefined Shape Templates
// ============================================================================
namespace TilePuzzleShapes
{
	// Single cell shape
	inline TilePuzzleShapeDefinition GetSingleShape(bool bDraggable = true)
	{
		TilePuzzleShapeDefinition xDef;
		xDef.eType = TILEPUZZLE_SHAPE_SINGLE;
		xDef.bDraggable = bDraggable;
		xDef.axCells = { {0, 0} };
		return xDef;
	}

	// Domino (2 horizontal cells)
	inline TilePuzzleShapeDefinition GetDominoShape(bool bDraggable = true)
	{
		TilePuzzleShapeDefinition xDef;
		xDef.eType = TILEPUZZLE_SHAPE_DOMINO;
		xDef.bDraggable = bDraggable;
		xDef.axCells = { {0, 0}, {1, 0} };
		return xDef;
	}

	// L-shape
	inline TilePuzzleShapeDefinition GetLShape(bool bDraggable = true)
	{
		TilePuzzleShapeDefinition xDef;
		xDef.eType = TILEPUZZLE_SHAPE_L;
		xDef.bDraggable = bDraggable;
		xDef.axCells = { {0, 0}, {1, 0}, {2, 0}, {2, 1} };
		return xDef;
	}

	// T-shape
	inline TilePuzzleShapeDefinition GetTShape(bool bDraggable = true)
	{
		TilePuzzleShapeDefinition xDef;
		xDef.eType = TILEPUZZLE_SHAPE_T;
		xDef.bDraggable = bDraggable;
		xDef.axCells = { {0, 0}, {1, 0}, {2, 0}, {1, 1} };
		return xDef;
	}

	// I-shape (3 cells)
	inline TilePuzzleShapeDefinition GetIShape(bool bDraggable = true)
	{
		TilePuzzleShapeDefinition xDef;
		xDef.eType = TILEPUZZLE_SHAPE_I;
		xDef.bDraggable = bDraggable;
		xDef.axCells = { {0, 0}, {1, 0}, {2, 0} };
		return xDef;
	}

	// S-shape
	inline TilePuzzleShapeDefinition GetSShape(bool bDraggable = true)
	{
		TilePuzzleShapeDefinition xDef;
		xDef.eType = TILEPUZZLE_SHAPE_S;
		xDef.bDraggable = bDraggable;
		xDef.axCells = { {1, 0}, {2, 0}, {0, 1}, {1, 1} };
		return xDef;
	}

	// Z-shape
	inline TilePuzzleShapeDefinition GetZShape(bool bDraggable = true)
	{
		TilePuzzleShapeDefinition xDef;
		xDef.eType = TILEPUZZLE_SHAPE_Z;
		xDef.bDraggable = bDraggable;
		xDef.axCells = { {0, 0}, {1, 0}, {1, 1}, {2, 1} };
		return xDef;
	}

	// O-shape (2x2 square)
	inline TilePuzzleShapeDefinition GetOShape(bool bDraggable = true)
	{
		TilePuzzleShapeDefinition xDef;
		xDef.eType = TILEPUZZLE_SHAPE_O;
		xDef.bDraggable = bDraggable;
		xDef.axCells = { {0, 0}, {1, 0}, {0, 1}, {1, 1} };
		return xDef;
	}

	// Get shape by type
	inline TilePuzzleShapeDefinition GetShape(TilePuzzleShapeType eType, bool bDraggable = true)
	{
		switch (eType)
		{
		case TILEPUZZLE_SHAPE_SINGLE: return GetSingleShape(bDraggable);
		case TILEPUZZLE_SHAPE_DOMINO: return GetDominoShape(bDraggable);
		case TILEPUZZLE_SHAPE_L: return GetLShape(bDraggable);
		case TILEPUZZLE_SHAPE_T: return GetTShape(bDraggable);
		case TILEPUZZLE_SHAPE_I: return GetIShape(bDraggable);
		case TILEPUZZLE_SHAPE_S: return GetSShape(bDraggable);
		case TILEPUZZLE_SHAPE_Z: return GetZShape(bDraggable);
		case TILEPUZZLE_SHAPE_O: return GetOShape(bDraggable);
		default: return GetSingleShape(bDraggable);
		}
	}
}

// ============================================================================
// Direction Utilities
// ============================================================================
namespace TilePuzzleDirections
{
	inline void GetDelta(TilePuzzleDirection eDir, int32_t& iDeltaX, int32_t& iDeltaY)
	{
		switch (eDir)
		{
		case TILEPUZZLE_DIR_UP:    iDeltaX = 0;  iDeltaY = -1; break;
		case TILEPUZZLE_DIR_DOWN:  iDeltaX = 0;  iDeltaY = 1;  break;
		case TILEPUZZLE_DIR_LEFT:  iDeltaX = -1; iDeltaY = 0;  break;
		case TILEPUZZLE_DIR_RIGHT: iDeltaX = 1;  iDeltaY = 0;  break;
		default:                   iDeltaX = 0;  iDeltaY = 0;  break;
		}
	}

	inline TilePuzzleDirection GetOpposite(TilePuzzleDirection eDir)
	{
		switch (eDir)
		{
		case TILEPUZZLE_DIR_UP:    return TILEPUZZLE_DIR_DOWN;
		case TILEPUZZLE_DIR_DOWN:  return TILEPUZZLE_DIR_UP;
		case TILEPUZZLE_DIR_LEFT:  return TILEPUZZLE_DIR_RIGHT;
		case TILEPUZZLE_DIR_RIGHT: return TILEPUZZLE_DIR_LEFT;
		default:                   return TILEPUZZLE_DIR_NONE;
		}
	}
}
