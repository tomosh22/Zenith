#pragma once
/**
 * TilePuzzleLevelData_Serialize.h - Binary serialization for TilePuzzleLevelData
 *
 * Uses Zenith_DataStream to serialize/deserialize level data to .tlvl files.
 * Shape definitions are serialized inline (type, draggable, cell offsets) since
 * pxDefinition pointers cannot be serialized directly.
 */

#include "DataStream/Zenith_DataStream.h"
#include "Collections/Zenith_Vector.h"
#include "TilePuzzle_Types.h"

static constexpr uint32_t s_uTLVL_MAGIC = 0x54504C56; // "TPLV"
static constexpr uint32_t s_uTLVL_VERSION = 2;
static constexpr uint32_t s_uMETA_MAGIC = 0x4D455441; // "META" - metadata header from TilePuzzleLevelGen

namespace TilePuzzleLevelSerialize
{
	[[maybe_unused]] static void Write(Zenith_DataStream& xStream, const TilePuzzleLevelData& xLevel)
	{
		// Header
		xStream << s_uTLVL_MAGIC;
		xStream << s_uTLVL_VERSION;

		// Grid
		xStream << xLevel.uGridWidth;
		xStream << xLevel.uGridHeight;
		xStream << xLevel.uMinimumMoves;

		// Cells
		uint32_t uNumCells = static_cast<uint32_t>(xLevel.aeCells.size());
		xStream << uNumCells;
		for (uint32_t i = 0; i < uNumCells; ++i)
		{
			uint8_t uCell = static_cast<uint8_t>(xLevel.aeCells[i]);
			xStream << uCell;
		}

		// Shapes (with inline definitions)
		uint32_t uNumShapes = static_cast<uint32_t>(xLevel.axShapes.size());
		xStream << uNumShapes;
		for (uint32_t i = 0; i < uNumShapes; ++i)
		{
			const TilePuzzleShapeInstance& xShape = xLevel.axShapes[i];
			xStream << xShape.iOriginX;
			xStream << xShape.iOriginY;
			uint8_t uColor = static_cast<uint8_t>(xShape.eColor);
			xStream << uColor;
			xStream << xShape.uUnlockThreshold;

			// Inline shape definition
			if (xShape.pxDefinition)
			{
				uint8_t uHasDef = 1;
				xStream << uHasDef;
				uint8_t uType = static_cast<uint8_t>(xShape.pxDefinition->eType);
				xStream << uType;
				uint8_t uDraggable = xShape.pxDefinition->bDraggable ? 1 : 0;
				xStream << uDraggable;
				uint32_t uNumDefCells = static_cast<uint32_t>(xShape.pxDefinition->axCells.size());
				xStream << uNumDefCells;
				for (uint32_t c = 0; c < uNumDefCells; ++c)
				{
					xStream << xShape.pxDefinition->axCells[c].iX;
					xStream << xShape.pxDefinition->axCells[c].iY;
				}
			}
			else
			{
				uint8_t uHasDef = 0;
				xStream << uHasDef;
			}
		}

		// Cats
		uint32_t uNumCats = static_cast<uint32_t>(xLevel.axCats.size());
		xStream << uNumCats;
		for (uint32_t i = 0; i < uNumCats; ++i)
		{
			const TilePuzzleCatData& xCat = xLevel.axCats[i];
			uint8_t uColor = static_cast<uint8_t>(xCat.eColor);
			xStream << uColor;
			xStream << xCat.iGridX;
			xStream << xCat.iGridY;
			uint8_t uOnBlocker = xCat.bOnBlocker ? 1 : 0;
			xStream << uOnBlocker;
		}

		// Solution (v2+)
		uint32_t uNumSolutionMoves = static_cast<uint32_t>(xLevel.axSolution.size());
		xStream << uNumSolutionMoves;
		for (uint32_t i = 0; i < uNumSolutionMoves; ++i)
		{
			xStream << xLevel.axSolution[i].uShapeIndex;
			xStream << xLevel.axSolution[i].iEndX;
			xStream << xLevel.axSolution[i].iEndY;
		}
	}

	[[maybe_unused]] static bool Read(Zenith_DataStream& xStream, TilePuzzleLevelData& xLevel, Zenith_Vector<TilePuzzleShapeDefinition>& axShapeDefsOut)
	{
		axShapeDefsOut.Clear();

		// Header - skip META header from TilePuzzleLevelGen if present
		uint32_t uMagic, uVersion;
		xStream >> uMagic;
		if (uMagic == s_uMETA_MAGIC)
		{
			uint32_t uMetaVersion;
			xStream >> uMetaVersion;
			// Skip v2 metadata fields in order:
			// 11 uint32_t, 1 uint64_t, 6 uint32_t, 1 uint64_t, 5 uint32_t
			for (uint32_t i = 0; i < 11; ++i) { uint32_t uSkip; xStream >> uSkip; }
			{ uint64_t ulSkip; xStream >> ulSkip; } // ulLayoutHash
			for (uint32_t i = 0; i < 6; ++i) { uint32_t uSkip; xStream >> uSkip; }
			{ uint64_t ulSkip; xStream >> ulSkip; } // ulGenerationTimestamp
			for (uint32_t i = 0; i < 5; ++i) { uint32_t uSkip; xStream >> uSkip; }
			xStream >> uMagic; // Now read TPLV magic
		}
		xStream >> uVersion;
		if (uMagic != s_uTLVL_MAGIC || uVersion > s_uTLVL_VERSION)
			return false;

		// Grid
		xStream >> xLevel.uGridWidth;
		xStream >> xLevel.uGridHeight;
		xStream >> xLevel.uMinimumMoves;

		// Cells
		uint32_t uNumCells;
		xStream >> uNumCells;
		xLevel.aeCells.resize(uNumCells);
		for (uint32_t i = 0; i < uNumCells; ++i)
		{
			uint8_t uCell;
			xStream >> uCell;
			xLevel.aeCells[i] = static_cast<TilePuzzleCellType>(uCell);
		}

		// Shapes
		uint32_t uNumShapes;
		xStream >> uNumShapes;
		xLevel.axShapes.resize(uNumShapes);
		for (uint32_t i = 0; i < uNumShapes; ++i)
		{
			TilePuzzleShapeInstance& xShape = xLevel.axShapes[i];
			xStream >> xShape.iOriginX;
			xStream >> xShape.iOriginY;
			uint8_t uColor;
			xStream >> uColor;
			xShape.eColor = static_cast<TilePuzzleColor>(uColor);
			xStream >> xShape.uUnlockThreshold;
			xShape.bRemoved = false;

			uint8_t uHasDef;
			xStream >> uHasDef;
			if (uHasDef)
			{
				TilePuzzleShapeDefinition xDef;
				uint8_t uType;
				xStream >> uType;
				xDef.eType = static_cast<TilePuzzleShapeType>(uType);
				uint8_t uDraggable;
				xStream >> uDraggable;
				xDef.bDraggable = uDraggable != 0;
				uint32_t uNumDefCells;
				xStream >> uNumDefCells;
				xDef.axCells.resize(uNumDefCells);
				for (uint32_t c = 0; c < uNumDefCells; ++c)
				{
					xStream >> xDef.axCells[c].iX;
					xStream >> xDef.axCells[c].iY;
				}
				axShapeDefsOut.PushBack(std::move(xDef));
				xShape.pxDefinition = &axShapeDefsOut.GetBack();
			}
			else
			{
				xShape.pxDefinition = nullptr;
			}
		}

		// Remap pointers (definitions may have been relocated during PushBack)
		uint32_t uDefIdx = 0;
		for (uint32_t i = 0; i < uNumShapes; ++i)
		{
			if (xLevel.axShapes[i].pxDefinition)
			{
				xLevel.axShapes[i].pxDefinition = &axShapeDefsOut.Get(uDefIdx);
				uDefIdx++;
			}
		}

		// Cats
		uint32_t uNumCats;
		xStream >> uNumCats;
		xLevel.axCats.resize(uNumCats);
		for (uint32_t i = 0; i < uNumCats; ++i)
		{
			TilePuzzleCatData& xCat = xLevel.axCats[i];
			uint8_t uColor;
			xStream >> uColor;
			xCat.eColor = static_cast<TilePuzzleColor>(uColor);
			xStream >> xCat.iGridX;
			xStream >> xCat.iGridY;
			uint8_t uOnBlocker;
			xStream >> uOnBlocker;
			xCat.bOnBlocker = uOnBlocker != 0;
			xCat.bEliminated = false;
			xCat.fEliminationProgress = 0.f;
		}

		// Solution (v2+)
		xLevel.axSolution.clear();
		if (uVersion >= 2)
		{
			uint32_t uNumSolutionMoves;
			xStream >> uNumSolutionMoves;
			xLevel.axSolution.resize(uNumSolutionMoves);
			for (uint32_t i = 0; i < uNumSolutionMoves; ++i)
			{
				xStream >> xLevel.axSolution[i].uShapeIndex;
				xStream >> xLevel.axSolution[i].iEndX;
				xStream >> xLevel.axSolution[i].iEndY;
			}
		}

		return true;
	}
}
