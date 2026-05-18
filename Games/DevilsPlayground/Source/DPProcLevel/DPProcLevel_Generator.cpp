#include "Zenith.h"
#include "Source/DPProcLevel/DPProcLevel_Generator.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace DPProcLevel
{
	namespace
	{
		// Axis-aligned partition rectangle used during BSP recursion.
		struct Partition
		{
			float fMinX, fMinZ, fMaxX, fMaxZ;
			RoomId xLeafRoomId = kInvalidRoomId;  // assigned when this becomes a leaf
		};

		using Rng = std::mt19937_64;

		// Uniform random float in [fLo, fHi].
		float UniformF(Rng& xRng, float fLo, float fHi)
		{
			std::uniform_real_distribution<float> xDist(fLo, fHi);
			return xDist(xRng);
		}

		// 2D OBB intersection via separating axis theorem. We test the
		// 4 axes that the two OBBs' edges define; if any axis separates
		// them, they don't intersect.
		bool OBBsOverlap(const Room& xA, const Room& xB)
		{
			const float fCosA = std::cos(xA.fYawRadians);
			const float fSinA = std::sin(xA.fYawRadians);
			const float fCosB = std::cos(xB.fYawRadians);
			const float fSinB = std::sin(xB.fYawRadians);

			// Local-frame axis vectors for each OBB. Axis A0 = (cos, sin)
			// (rotated +X local axis), A1 = (-sin, cos) (rotated +Z local
			// axis). Use Right-handed R_y convention to match the
			// telemetry visualiser fix from PR #95:
			//   world.x = lx*cos + lz*sin
			//   world.z = -lx*sin + lz*cos
			// so axis A0 (lx=1, lz=0) is world (cos, -sin) and A1 (lx=0,
			// lz=1) is world (sin, cos).
			const float fA0x = fCosA;
			const float fA0z = -fSinA;
			const float fA1x = fSinA;
			const float fA1z = fCosA;
			const float fB0x = fCosB;
			const float fB0z = -fSinB;
			const float fB1x = fSinB;
			const float fB1z = fCosB;

			const float fDx = xB.fCentreX - xA.fCentreX;
			const float fDz = xB.fCentreZ - xA.fCentreZ;

			// Test against each of the 4 axes. For axis n (unit vector),
			// the projected separation must exceed the sum of projected
			// half-extents from each OBB.
			auto IsSeparating = [&](float nx, float nz) -> bool
			{
				// Project A's half-extents onto axis n.
				const float fAProj =
					xA.fHalfExtentX * std::fabs(fA0x * nx + fA0z * nz) +
					xA.fHalfExtentZ * std::fabs(fA1x * nx + fA1z * nz);
				const float fBProj =
					xB.fHalfExtentX * std::fabs(fB0x * nx + fB0z * nz) +
					xB.fHalfExtentZ * std::fabs(fB1x * nx + fB1z * nz);
				const float fCentreProj = std::fabs(fDx * nx + fDz * nz);
				return fCentreProj > (fAProj + fBProj);
			};

			if (IsSeparating(fA0x, fA0z)) return false;
			if (IsSeparating(fA1x, fA1z)) return false;
			if (IsSeparating(fB0x, fB0z)) return false;
			if (IsSeparating(fB1x, fB1z)) return false;
			return true;
		}

		// Recursive BSP. Splits the partition until either axis is below
		// 2 * fMinRoomSize or we hit uBspDepth depth. Leaves get an
		// xLeafRoomId assigned in DFS order.
		//
		// Returns the SUBTREE'S list of leaf indices (into axLeaves).
		Zenith_Vector<int32_t> BspRecurse(
			Rng& xRng, const GenConfig& xCfg,
			Partition xP, uint32_t uDepthRemaining,
			Zenith_Vector<Partition>& axLeavesOut)
		{
			Zenith_Vector<int32_t> axLeafIndices;

			const float fW = xP.fMaxX - xP.fMinX;
			const float fH = xP.fMaxZ - xP.fMinZ;

			// Stop splitting if either axis can't split into two valid
			// children, or we hit the depth limit.
			const bool bCanSplitX = (fW >= 2.0f * xCfg.fMinRoomSize);
			const bool bCanSplitZ = (fH >= 2.0f * xCfg.fMinRoomSize);

			if (uDepthRemaining == 0 || (!bCanSplitX && !bCanSplitZ))
			{
				xP.xLeafRoomId = static_cast<RoomId>(axLeavesOut.GetSize());
				axLeavesOut.PushBack(xP);
				axLeafIndices.PushBack(xP.xLeafRoomId);
				return axLeafIndices;
			}

			// Pick split axis. Prefer the longer dimension to avoid
			// degenerate sliver partitions.
			const bool bSplitOnX = bCanSplitX && (!bCanSplitZ || (fW >= fH));

			// Split position is in the middle 50%% of the dimension --
			// 25%% to 75%% -- so we get visible variety without producing
			// pathological strips.
			Partition xLow = xP, xHigh = xP;
			if (bSplitOnX)
			{
				const float fSplit = UniformF(xRng,
					xP.fMinX + 0.25f * fW, xP.fMinX + 0.75f * fW);
				xLow.fMaxX  = fSplit;
				xHigh.fMinX = fSplit;
			}
			else
			{
				const float fSplit = UniformF(xRng,
					xP.fMinZ + 0.25f * fH, xP.fMinZ + 0.75f * fH);
				xLow.fMaxZ  = fSplit;
				xHigh.fMinZ = fSplit;
			}

			Zenith_Vector<int32_t> axL = BspRecurse(xRng, xCfg, xLow, uDepthRemaining - 1, axLeavesOut);
			Zenith_Vector<int32_t> axH = BspRecurse(xRng, xCfg, xHigh, uDepthRemaining - 1, axLeavesOut);
			for (uint32_t i = 0; i < axL.GetSize(); ++i) axLeafIndices.PushBack(axL.Get(i));
			for (uint32_t i = 0; i < axH.GetSize(); ++i) axLeafIndices.PushBack(axH.Get(i));
			return axLeafIndices;
		}

		// Pick a room OBB inside a partition. Rotation-safe sizing: a
		// room of half-extents (hx, hz) has bounding-circle radius
		// sqrt(hx^2 + hz^2). For ANY rotation to keep the room inside
		// its partition (with margin), that radius must be <= the
		// partition's inscribed-circle radius.
		//
		// Given an aspect ratio a = hz/hx, the constraint becomes
		//   sqrt(hx^2 + (a*hx)^2) = hx * sqrt(1 + a^2) <= R
		// so hx = R / sqrt(1 + a^2) and hz = a*hx. This preserves the
		// bounding-circle-inside-inscribed-circle invariant for any
		// aspect, so rooms can be rectangular without leaking into
		// adjacent partitions after rotation.
		//
		// fMaxRoomSize clamps the long axis without distorting the
		// aspect: if either hx or hz exceeds the cap, both are scaled
		// down proportionally.
		Room PartitionToRoom(const Partition& xP, RoomId xId,
		                     const GenConfig& xCfg, float fAspect)
		{
			const float fMargin = 0.5f;
			const float fPartHX = 0.5f * (xP.fMaxX - xP.fMinX);
			const float fPartHZ = 0.5f * (xP.fMaxZ - xP.fMinZ);
			const float fSafeR  = std::min(fPartHX, fPartHZ) - fMargin;
			float fHx = fSafeR / std::sqrt(1.0f + fAspect * fAspect);
			float fHz = fAspect * fHx;
			const float fMaxDim = std::max(fHx, fHz);
			if (fMaxDim > xCfg.fMaxRoomSize)
			{
				const float fScale = xCfg.fMaxRoomSize / fMaxDim;
				fHx *= fScale;
				fHz *= fScale;
			}
			Room xRoom;
			xRoom.id           = xId;
			xRoom.fCentreX     = 0.5f * (xP.fMinX + xP.fMaxX);
			xRoom.fCentreZ     = 0.5f * (xP.fMinZ + xP.fMaxZ);
			xRoom.fHalfExtentX = fHx;
			xRoom.fHalfExtentZ = fHz;
			xRoom.fYawRadians  = 0.0f;
			return xRoom;
		}

		// Two partitions are BSP-adjacent if they share a non-zero
		// segment of edge. Used to seed the corridor graph.
		bool PartitionsShareEdge(const Partition& xA, const Partition& xB)
		{
			constexpr float kEps = 0.001f;
			// Vertical shared edge: A's right matches B's left or vice
			// versa, and Z ranges overlap.
			if (std::fabs(xA.fMaxX - xB.fMinX) < kEps ||
			    std::fabs(xB.fMaxX - xA.fMinX) < kEps)
			{
				const float fZLo = std::max(xA.fMinZ, xB.fMinZ);
				const float fZHi = std::min(xA.fMaxZ, xB.fMaxZ);
				return (fZHi - fZLo) > kEps;
			}
			// Horizontal shared edge.
			if (std::fabs(xA.fMaxZ - xB.fMinZ) < kEps ||
			    std::fabs(xB.fMaxZ - xA.fMinZ) < kEps)
			{
				const float fXLo = std::max(xA.fMinX, xB.fMinX);
				const float fXHi = std::min(xA.fMaxX, xB.fMaxX);
				return (fXHi - fXLo) > kEps;
			}
			return false;
		}

		// Build wall segments for the layout. Each room contributes 4
		// edges (bottom, right, top, left in local frame); each edge is
		// split into 1..N segments by the door gaps that fall on it.
		// Door points are projected back into each room's local frame so
		// we know which edge they belong to.
		//
		// Convention: emitted WallSegment uses (hx, hz) where hx is the
		// half-length along the wall's local +X and hz is half-thickness
		// along local +Z. For edges that run along the room's local +X
		// (top + bottom), this maps directly: wall hx = segment half-length,
		// wall hz = half-thickness, wall yaw = room yaw. For edges along
		// local +Z (left + right) we SWAP into (hx = thickness, hz = length)
		// so the wall still has yaw = room.yaw -- avoids a 90 deg yaw
		// offset that would otherwise mess with the visualiser's coord
		// expectations.
		void BuildWallSegments(const LevelLayout& xLayoutIn,
		                       const GenConfig& xCfg,
		                       Zenith_Vector<WallSegment>& xOut)
		{
			xOut.Clear();
			const float fHalfThick = xCfg.fWallHalfThickness;
			const float fDoorHalf  = xCfg.fDoorGapHalfWidth;

			for (uint32_t uR = 0; uR < xLayoutIn.axRooms.GetSize(); ++uR)
			{
				const Room& xRoom = xLayoutIn.axRooms.Get(uR);
				const float fCos = std::cos(xRoom.fYawRadians);
				const float fSin = std::sin(xRoom.fYawRadians);
				const float fHx  = xRoom.fHalfExtentX;
				const float fHz  = xRoom.fHalfExtentZ;

				// Door positions in this room's local frame, classified by
				// which edge they sit on. Index 0 = bottom (lz=-fHz),
				// 1 = right (lx=+fHx), 2 = top (lz=+fHz), 3 = left (lx=-fHx).
				Zenith_Vector<float> aaDoorsAlongEdge[4];

				const float kEdgeTol = 0.01f;  // 1 cm; rotation is exact for door projection
				for (uint32_t uD = 0; uD < xLayoutIn.axDoorPoints.GetSize(); ++uD)
				{
					const DoorPoint& xDP = xLayoutIn.axDoorPoints.Get(uD);
					if (xDP.xRoomId != xRoom.id) continue;
					// Inverse of R_y rotation (XZ): see PR #95 conventions.
					//   wx = cx + lx*cos + lz*sin
					//   wz = cz - lx*sin + lz*cos
					// Inverse:
					//   lx = dx*cos - dz*sin
					//   lz = dx*sin + dz*cos
					const float fDx = xDP.fX - xRoom.fCentreX;
					const float fDz = xDP.fZ - xRoom.fCentreZ;
					const float fLx = fDx * fCos - fDz * fSin;
					const float fLz = fDx * fSin + fDz * fCos;
					// Classify by closest edge (one of 4) using same logic as
					// ProjectDoorPoint -- the door is guaranteed to sit on an
					// edge because that's how it was authored.
					const float fAbsLx = std::fabs(fLx);
					const float fAbsLz = std::fabs(fLz);
					if (fAbsLx * fHz > fAbsLz * fHx)
					{
						// X-edge: door is on left or right
						const int iEdge = (fLx >= 0.0f) ? 1 : 3;  // right=1, left=3
						aaDoorsAlongEdge[iEdge].PushBack(fLz);
						(void)kEdgeTol;
					}
					else
					{
						// Z-edge: door is on bottom or top
						const int iEdge = (fLz >= 0.0f) ? 2 : 0;  // top=2, bottom=0
						aaDoorsAlongEdge[iEdge].PushBack(fLx);
					}
				}

				// For each edge, sort door positions along the edge axis,
				// then split [-half, +half] into segments with gaps.
				//
				// Edge 0 (bottom): axis = lx, fixed lz = -fHz
				// Edge 1 (right):  axis = lz, fixed lx = +fHx
				// Edge 2 (top):    axis = lx, fixed lz = +fHz
				// Edge 3 (left):   axis = lz, fixed lx = -fHx
				const float aLocalFixed[4][2] = {
					{ 0.0f, -fHz },   // bottom: local (0, -hz), axis = lx
					{ +fHx, 0.0f },   // right:  local (+hx, 0), axis = lz
					{ 0.0f, +fHz },   // top:    local (0, +hz), axis = lx
					{ -fHx, 0.0f },   // left:   local (-hx, 0), axis = lz
				};
				const bool abIsXEdge[4] = { true, false, true, false };

				for (int iE = 0; iE < 4; ++iE)
				{
					const float fAxisMax = abIsXEdge[iE] ? fHx : fHz;
					Zenith_Vector<float>& axDoors = aaDoorsAlongEdge[iE];

					// Bubble-sort the door positions (small N -- typically <=2
					// doors per edge in a BSP layout, so the cost is negligible).
					for (uint32_t a = 0; a + 1 < axDoors.GetSize(); ++a)
					{
						for (uint32_t b = a + 1; b < axDoors.GetSize(); ++b)
						{
							if (axDoors.Get(b) < axDoors.Get(a))
							{
								const float t = axDoors.Get(a);
								axDoors.Get(a) = axDoors.Get(b);
								axDoors.Get(b) = t;
							}
						}
					}

					// Walk along the edge axis, emitting wall segments around
					// the door gaps. A door at position d removes the interval
					// [d - fDoorHalf, d + fDoorHalf] from the edge; we emit a
					// wall for whatever's left.
					float fCursor = -fAxisMax;
					Zenith_Vector<float> axBreakpoints;  // pairs of (start, end)
					for (uint32_t uD = 0; uD < axDoors.GetSize(); ++uD)
					{
						const float fDoorPos = axDoors.Get(uD);
						const float fGapStart = fDoorPos - fDoorHalf;
						const float fGapEnd   = fDoorPos + fDoorHalf;
						if (fGapStart > fCursor)
						{
							axBreakpoints.PushBack(fCursor);
							axBreakpoints.PushBack(fGapStart);
						}
						if (fGapEnd > fCursor) fCursor = fGapEnd;
					}
					if (fCursor < fAxisMax)
					{
						axBreakpoints.PushBack(fCursor);
						axBreakpoints.PushBack(fAxisMax);
					}

					// Each pair in axBreakpoints is one wall segment.
					for (uint32_t uP = 0; uP + 1 < axBreakpoints.GetSize(); uP += 2)
					{
						const float fA = axBreakpoints.Get(uP);
						const float fB = axBreakpoints.Get(uP + 1);
						const float fSegHalf = 0.5f * (fB - fA);
						if (fSegHalf < 0.001f) continue;  // degenerate
						const float fSegAxisCentre = 0.5f * (fA + fB);

						// Wall's local-frame centre offset from room centre.
						float fLocalCX, fLocalCZ;
						if (abIsXEdge[iE])
						{
							fLocalCX = fSegAxisCentre;
							fLocalCZ = aLocalFixed[iE][1];
						}
						else
						{
							fLocalCX = aLocalFixed[iE][0];
							fLocalCZ = fSegAxisCentre;
						}

						// Rotate to world (R_y).
						WallSegment xW;
						xW.fCentreX = xRoom.fCentreX + fLocalCX * fCos + fLocalCZ * fSin;
						xW.fCentreZ = xRoom.fCentreZ - fLocalCX * fSin + fLocalCZ * fCos;
						xW.fYawRadians = xRoom.fYawRadians;
						// Length along wall's local +X if it's an X-edge;
						// along wall's local +Z if Z-edge. Swap hx/hz to
						// match (see header comment).
						if (abIsXEdge[iE])
						{
							xW.fHalfExtentX = fSegHalf;
							xW.fHalfExtentZ = fHalfThick;
						}
						else
						{
							xW.fHalfExtentX = fHalfThick;
							xW.fHalfExtentZ = fSegHalf;
						}
						xOut.PushBack(xW);
					}
				}
			}
		}

		// Returns rooms[id] reachable from rooms[iStart], walking the
		// corridor graph. If iSkipCorridor >= 0, that corridor is treated
		// as removed (used to BFS "what's reachable WITHOUT the key" by
		// removing the key-gated corridor before the search).
		Zenith_Vector<bool> BfsReachable(const LevelLayout& xLayout, RoomId iStart, int32_t iSkipCorridor)
		{
			Zenith_Vector<bool> abVisited;
			for (uint32_t i = 0; i < xLayout.axRooms.GetSize(); ++i) abVisited.PushBack(false);
			if (iStart < 0 || iStart >= static_cast<RoomId>(xLayout.axRooms.GetSize())) return abVisited;
			abVisited.Get(iStart) = true;
			Zenith_Vector<RoomId> axFrontier;
			axFrontier.PushBack(iStart);
			while (axFrontier.GetSize() > 0)
			{
				const RoomId xCur = axFrontier.Get(axFrontier.GetSize() - 1);
				axFrontier.Remove(axFrontier.GetSize() - 1);
				for (uint32_t iC = 0; iC < xLayout.axCorridors.GetSize(); ++iC)
				{
					if (static_cast<int32_t>(iC) == iSkipCorridor) continue;
					const Corridor& xC = xLayout.axCorridors.Get(iC);
					const RoomId xA = xLayout.axDoorPoints.Get(xC.iDoorA).xRoomId;
					const RoomId xB = xLayout.axDoorPoints.Get(xC.iDoorB).xRoomId;
					RoomId xOther = kInvalidRoomId;
					if      (xA == xCur) xOther = xB;
					else if (xB == xCur) xOther = xA;
					if (xOther == kInvalidRoomId) continue;
					if (abVisited.Get(static_cast<uint32_t>(xOther))) continue;
					abVisited.Get(static_cast<uint32_t>(xOther)) = true;
					axFrontier.PushBack(xOther);
				}
			}
			return abVisited;
		}

		// BFS distance from iStart through the corridor graph. Output
		// vector has one entry per room; unreachable rooms get -1.
		Zenith_Vector<int32_t> BfsDistances(const LevelLayout& xLayout, RoomId iStart)
		{
			Zenith_Vector<int32_t> aiDist;
			for (uint32_t i = 0; i < xLayout.axRooms.GetSize(); ++i) aiDist.PushBack(-1);
			if (iStart < 0 || iStart >= static_cast<RoomId>(xLayout.axRooms.GetSize())) return aiDist;
			aiDist.Get(iStart) = 0;
			Zenith_Vector<RoomId> axFrontier;
			axFrontier.PushBack(iStart);
			uint32_t uHead = 0;
			while (uHead < axFrontier.GetSize())
			{
				const RoomId xCur = axFrontier.Get(uHead++);
				for (uint32_t iC = 0; iC < xLayout.axCorridors.GetSize(); ++iC)
				{
					const Corridor& xC = xLayout.axCorridors.Get(iC);
					const RoomId xA = xLayout.axDoorPoints.Get(xC.iDoorA).xRoomId;
					const RoomId xB = xLayout.axDoorPoints.Get(xC.iDoorB).xRoomId;
					RoomId xOther = kInvalidRoomId;
					if      (xA == xCur) xOther = xB;
					else if (xB == xCur) xOther = xA;
					if (xOther == kInvalidRoomId) continue;
					if (aiDist.Get(static_cast<uint32_t>(xOther)) >= 0) continue;
					aiDist.Get(static_cast<uint32_t>(xOther)) = aiDist.Get(static_cast<uint32_t>(xCur)) + 1;
					axFrontier.PushBack(xOther);
				}
			}
			return aiDist;
		}

		// Place every gameplay element into the layout. Strategy:
		//   1. Spawn = room 0 (deterministic; the BSP DFS order makes
		//      this an arbitrary but predictable room).
		//   2. Pentagram = room with max BFS distance from spawn (i.e.
		//      "deepest" room). Ties broken by lowest id.
		//   3. Door = the corridor that, when removed, isolates the
		//      pentagram from the spawn -- typically the last corridor
		//      on the pentagram->spawn path. Found by walking the BFS
		//      parent chain back to the spawn until we hit a corridor
		//      whose removal disconnects the two.
		//   4. Iron + forge = rooms in the spawn-side reachable set
		//      (so the bot can reach them without the key).
		//   5. Chest + noise machine = any remaining rooms.
		//   6. 5 objectives = scattered across rooms, avoiding pentagram
		//      itself (since pickup-from-delivery-point is degenerate).
		//
		// Solvability is checked at the end -- key reachable without
		// door, pentagram + all objectives reachable WITH door.
		void PlaceGameElements(LevelLayout& xLayout)
		{
			xLayout.axGameElements.Clear();
			const uint32_t uN = xLayout.axRooms.GetSize();
			if (uN == 0) return;

			// Tracks how many elements have already landed in each room.
			// Multi-element rooms place subsequent elements on a small
			// circle around the room centre (50%% of room half-extent)
			// so they don't visually pile up on the same coordinate.
			Zenith_Vector<int32_t> aiRoomElemCount;
			for (uint32_t i = 0; i < uN; ++i) aiRoomElemCount.PushBack(0);

			auto RoomElement = [&xLayout, &aiRoomElemCount](GameElementType eType, RoomId xRoom) -> GameElement
			{
				GameElement xE;
				xE.eType  = eType;
				xE.xRoomId = xRoom;
				if (xRoom >= 0 && xRoom < static_cast<RoomId>(xLayout.axRooms.GetSize()))
				{
					const Room& xR = xLayout.axRooms.Get(static_cast<uint32_t>(xRoom));
					const int32_t iIdx = aiRoomElemCount.Get(static_cast<uint32_t>(xRoom));
					aiRoomElemCount.Get(static_cast<uint32_t>(xRoom)) = iIdx + 1;
					if (iIdx == 0)
					{
						// First element gets the room centre.
						xE.fX = xR.fCentreX;
						xE.fZ = xR.fCentreZ;
					}
					else
					{
						// Subsequent elements distribute on a circle in
						// the ROOM's local frame, then rotated to world
						// so the placement makes sense for rotated
						// rooms too.
						constexpr float kTwoPi = 6.28318530718f;
						const float fAngle = (iIdx - 1) * (kTwoPi / 6.0f);  // 6-point circle
						const float fMinHalf = std::min(xR.fHalfExtentX, xR.fHalfExtentZ);
						const float fOffset  = fMinHalf * 0.5f;
						const float fLocalX  = fOffset * std::cos(fAngle);
						const float fLocalZ  = fOffset * std::sin(fAngle);
						const float fCos = std::cos(xR.fYawRadians);
						const float fSin = std::sin(xR.fYawRadians);
						// World = roomCentre + R_y(roomYaw) * (lx, lz)
						xE.fX = xR.fCentreX + fLocalX * fCos + fLocalZ * fSin;
						xE.fZ = xR.fCentreZ - fLocalX * fSin + fLocalZ * fCos;
					}
				}
				return xE;
			};

			// 1. Spawn at room 0.
			const RoomId xSpawnRoom = 0;
			xLayout.axGameElements.PushBack(RoomElement(GameElementType::SpawnPoint, xSpawnRoom));

			// 2. Pentagram at deepest room. Pre-compute BFS distances
			//    from spawn (no door removed yet) so we can also find
			//    the door-corridor below.
			Zenith_Vector<int32_t> aiDistFromSpawn = BfsDistances(xLayout, xSpawnRoom);
			RoomId xPentRoom = 0;
			int32_t iMaxDist = -1;
			for (uint32_t i = 0; i < uN; ++i)
			{
				const int32_t iD = aiDistFromSpawn.Get(i);
				if (iD > iMaxDist) { iMaxDist = iD; xPentRoom = static_cast<RoomId>(i); }
			}
			xLayout.axGameElements.PushBack(RoomElement(GameElementType::Pentagram, xPentRoom));

			// 3. Doors: one on EVERY corridor incident to the pentagram
			//    room. The earlier "pick one cut edge" strategy left a
			//    bug where rooms with multiple incident corridors had
			//    only one gated -- the bot could bypass the door by
			//    taking the unlocked alternate route, defeating the
			//    pentagram puzzle.
			//
			//    Placing doors on every incident corridor turns the
			//    pentagram into a true gated room: the bot must unlock
			//    at least ONE door to enter, and any door's key
			//    satisfies the gate (mirrors how DPDoor_Behaviour
			//    treats keys as fungible).
			//
			//    All gated corridors get tracked in axGatedCorridors so
			//    the spawn-side BFS (next step) skips ALL of them, not
			//    just a single id.
			Zenith_Vector<int32_t> axGatedCorridors;
			for (uint32_t iC = 0; iC < xLayout.axCorridors.GetSize(); ++iC)
			{
				const Corridor& xC = xLayout.axCorridors.Get(iC);
				const RoomId xA = xLayout.axDoorPoints.Get(xC.iDoorA).xRoomId;
				const RoomId xB = xLayout.axDoorPoints.Get(xC.iDoorB).xRoomId;
				if (xA != xPentRoom && xB != xPentRoom) continue;
				const DoorPoint& xDA = xLayout.axDoorPoints.Get(xC.iDoorA);
				const DoorPoint& xDB = xLayout.axDoorPoints.Get(xC.iDoorB);
				GameElement xE;
				xE.eType        = GameElementType::Door;
				xE.fX           = 0.5f * (xDA.fX + xDB.fX);
				xE.fZ           = 0.5f * (xDA.fZ + xDB.fZ);
				xE.xRoomId      = kInvalidRoomId;
				xE.iCorridorId  = static_cast<int32_t>(iC);
				xLayout.axGameElements.PushBack(xE);
				axGatedCorridors.PushBack(static_cast<int32_t>(iC));
			}

			// Spawn-side reachable set: BFS from spawn, treating ALL
			// gated corridors as removed. Whatever's reachable here
			// counts as "before the key" -- iron + forge must live in
			// this set so the bot can fetch them without crossing a door.
			auto BfsReachableSkippingGated = [&xLayout, &axGatedCorridors](RoomId iStart) -> Zenith_Vector<bool>
			{
				Zenith_Vector<bool> abVisited;
				for (uint32_t i = 0; i < xLayout.axRooms.GetSize(); ++i) abVisited.PushBack(false);
				if (iStart < 0 || iStart >= static_cast<RoomId>(xLayout.axRooms.GetSize())) return abVisited;
				abVisited.Get(iStart) = true;
				Zenith_Vector<RoomId> axFrontier;
				axFrontier.PushBack(iStart);
				while (axFrontier.GetSize() > 0)
				{
					const RoomId xCur = axFrontier.Get(axFrontier.GetSize() - 1);
					axFrontier.Remove(axFrontier.GetSize() - 1);
					for (uint32_t iC = 0; iC < xLayout.axCorridors.GetSize(); ++iC)
					{
						bool bGated = false;
						for (uint32_t iG = 0; iG < axGatedCorridors.GetSize(); ++iG)
						{
							if (axGatedCorridors.Get(iG) == static_cast<int32_t>(iC)) { bGated = true; break; }
						}
						if (bGated) continue;
						const Corridor& xC = xLayout.axCorridors.Get(iC);
						const RoomId xA = xLayout.axDoorPoints.Get(xC.iDoorA).xRoomId;
						const RoomId xB = xLayout.axDoorPoints.Get(xC.iDoorB).xRoomId;
						RoomId xOther = kInvalidRoomId;
						if      (xA == xCur) xOther = xB;
						else if (xB == xCur) xOther = xA;
						if (xOther == kInvalidRoomId) continue;
						if (abVisited.Get(static_cast<uint32_t>(xOther))) continue;
						abVisited.Get(static_cast<uint32_t>(xOther)) = true;
						axFrontier.PushBack(xOther);
					}
				}
				return abVisited;
			};
			const Zenith_Vector<bool> abReachNoDoor = BfsReachableSkippingGated(xSpawnRoom);

			// Build a list of candidate rooms for each placement bucket.
			// Iron + forge: spawn-side rooms (not pentagram, not spawn).
			// Objectives + chest + noise: any non-spawn, non-pentagram room.
			Zenith_Vector<RoomId> axSpawnSide;
			Zenith_Vector<RoomId> axAnyNonCritical;
			for (uint32_t i = 0; i < uN; ++i)
			{
				const RoomId xR = static_cast<RoomId>(i);
				if (xR == xSpawnRoom || xR == xPentRoom) continue;
				axAnyNonCritical.PushBack(xR);
				if (abReachNoDoor.Get(i)) axSpawnSide.PushBack(xR);
			}

			// 4. Iron + forge: prefer spawn-side. If too few spawn-side
			//    rooms, fall back to any non-pentagram (which means the
			//    bot might end up in a "no iron" failure state -- the
			//    solvability check at the end catches this).
			auto PickNth = [](Zenith_Vector<RoomId>& ax, uint32_t uIdx) -> RoomId
			{
				if (ax.GetSize() == 0) return kInvalidRoomId;
				return ax.Get(uIdx % ax.GetSize());
			};
			const RoomId xIronRoom  = PickNth(axSpawnSide.GetSize() > 0 ? axSpawnSide : axAnyNonCritical, 0);
			const RoomId xForgeRoom = PickNth(axSpawnSide.GetSize() > 1 ? axSpawnSide : axAnyNonCritical, 1);
			xLayout.axGameElements.PushBack(RoomElement(GameElementType::Iron,  xIronRoom));
			xLayout.axGameElements.PushBack(RoomElement(GameElementType::Forge, xForgeRoom));

			// 5. Chest + noise machine: any non-critical.
			const RoomId xChestRoom = PickNth(axAnyNonCritical, 2);
			const RoomId xNoiseRoom = PickNth(axAnyNonCritical, 3);
			xLayout.axGameElements.PushBack(RoomElement(GameElementType::Chest,        xChestRoom));
			xLayout.axGameElements.PushBack(RoomElement(GameElementType::NoiseMachine, xNoiseRoom));

			// 6. Objectives 1..5: distribute through non-critical rooms.
			//    Wraps round-robin if there are fewer rooms than 5.
			for (int iObj = 0; iObj < 5; ++iObj)
			{
				const RoomId xR = PickNth(axAnyNonCritical, static_cast<uint32_t>(4 + iObj));
				const GameElementType eT = static_cast<GameElementType>(
					static_cast<uint8_t>(GameElementType::Objective1) + iObj);
				xLayout.axGameElements.PushBack(RoomElement(eT, xR));
			}
		}

		// Validate the placement: with ALL gated corridors removed, iron
		// + forge must be reachable from spawn (the bot can fetch the
		// key). With doors included, pentagram + all 5 objectives must
		// be reachable (the bot can deliver). Returns true if solvable.
		// Logs the first missing reachability for diagnosis.
		bool ValidateSolvability(const LevelLayout& xLayout)
		{
			// Find spawn + pentagram + all door corridor ids.
			RoomId  xSpawn = kInvalidRoomId;
			RoomId  xPent  = kInvalidRoomId;
			Zenith_Vector<int32_t> axGated;
			Zenith_Vector<RoomId>  axObjectiveRooms;
			RoomId xIron = kInvalidRoomId, xForge = kInvalidRoomId;
			for (uint32_t i = 0; i < xLayout.axGameElements.GetSize(); ++i)
			{
				const GameElement& xE = xLayout.axGameElements.Get(i);
				switch (xE.eType)
				{
					case GameElementType::SpawnPoint: xSpawn = xE.xRoomId; break;
					case GameElementType::Pentagram:  xPent  = xE.xRoomId; break;
					case GameElementType::Door:       axGated.PushBack(xE.iCorridorId); break;
					case GameElementType::Iron:       xIron  = xE.xRoomId; break;
					case GameElementType::Forge:      xForge = xE.xRoomId; break;
					case GameElementType::Objective1:
					case GameElementType::Objective2:
					case GameElementType::Objective3:
					case GameElementType::Objective4:
					case GameElementType::Objective5: axObjectiveRooms.PushBack(xE.xRoomId); break;
					default: break;
				}
			}
			if (xSpawn == kInvalidRoomId || xPent == kInvalidRoomId)
			{
				Zenith_Warning(LOG_CATEGORY_CORE,
					"DPProcLevel::ValidateSolvability: missing spawn or pentagram element");
				return false;
			}

			// BFS skipping ALL gated corridors at once. Mirrors the
			// PlaceGameElements helper.
			auto BfsSkippingMany = [&xLayout, &axGated](RoomId iStart) -> Zenith_Vector<bool>
			{
				Zenith_Vector<bool> abVisited;
				for (uint32_t i = 0; i < xLayout.axRooms.GetSize(); ++i) abVisited.PushBack(false);
				if (iStart < 0 || iStart >= static_cast<RoomId>(xLayout.axRooms.GetSize())) return abVisited;
				abVisited.Get(iStart) = true;
				Zenith_Vector<RoomId> axFrontier;
				axFrontier.PushBack(iStart);
				while (axFrontier.GetSize() > 0)
				{
					const RoomId xCur = axFrontier.Get(axFrontier.GetSize() - 1);
					axFrontier.Remove(axFrontier.GetSize() - 1);
					for (uint32_t iC = 0; iC < xLayout.axCorridors.GetSize(); ++iC)
					{
						bool bGated = false;
						for (uint32_t iG = 0; iG < axGated.GetSize(); ++iG)
							if (axGated.Get(iG) == static_cast<int32_t>(iC)) { bGated = true; break; }
						if (bGated) continue;
						const Corridor& xC = xLayout.axCorridors.Get(iC);
						const RoomId xA = xLayout.axDoorPoints.Get(xC.iDoorA).xRoomId;
						const RoomId xB = xLayout.axDoorPoints.Get(xC.iDoorB).xRoomId;
						RoomId xOther = kInvalidRoomId;
						if      (xA == xCur) xOther = xB;
						else if (xB == xCur) xOther = xA;
						if (xOther == kInvalidRoomId) continue;
						if (abVisited.Get(static_cast<uint32_t>(xOther))) continue;
						abVisited.Get(static_cast<uint32_t>(xOther)) = true;
						axFrontier.PushBack(xOther);
					}
				}
				return abVisited;
			};
			const Zenith_Vector<bool> abNoDoor   = BfsSkippingMany(xSpawn);
			const Zenith_Vector<bool> abWithDoor = BfsReachable(xLayout, xSpawn, -1);

			if (xIron != kInvalidRoomId && !abNoDoor.Get(static_cast<uint32_t>(xIron)))
			{
				Zenith_Warning(LOG_CATEGORY_CORE,
					"DPProcLevel::ValidateSolvability: iron room %d unreachable without key", xIron);
				return false;
			}
			if (xForge != kInvalidRoomId && !abNoDoor.Get(static_cast<uint32_t>(xForge)))
			{
				Zenith_Warning(LOG_CATEGORY_CORE,
					"DPProcLevel::ValidateSolvability: forge room %d unreachable without key", xForge);
				return false;
			}
			if (!abWithDoor.Get(static_cast<uint32_t>(xPent)))
			{
				Zenith_Warning(LOG_CATEGORY_CORE,
					"DPProcLevel::ValidateSolvability: pentagram room %d unreachable even with key", xPent);
				return false;
			}
			for (uint32_t i = 0; i < axObjectiveRooms.GetSize(); ++i)
			{
				const RoomId xR = axObjectiveRooms.Get(i);
				if (xR == kInvalidRoomId) continue;
				if (!abWithDoor.Get(static_cast<uint32_t>(xR)))
				{
					Zenith_Warning(LOG_CATEGORY_CORE,
						"DPProcLevel::ValidateSolvability: objective %u room %d unreachable", i, xR);
					return false;
				}
			}
			return true;
		}

		// Returns true if world point (fX, fZ) is inside the OBB of any
		// room in the layout, treating the room as expanded by fMargin
		// on each side (so outdoor samples sit at least fMargin away
		// from every wall and don't clip into furniture).
		bool PointInsideAnyRoom(const LevelLayout& xLayout, float fX, float fZ, float fMargin)
		{
			for (uint32_t i = 0; i < xLayout.axRooms.GetSize(); ++i)
			{
				const Room& xR = xLayout.axRooms.Get(i);
				const float fCos = std::cos(xR.fYawRadians);
				const float fSin = std::sin(xR.fYawRadians);
				// Inverse R_y rotation: world -> room-local.
				//   lx = dx*cos - dz*sin
				//   lz = dx*sin + dz*cos
				const float fDx = fX - xR.fCentreX;
				const float fDz = fZ - xR.fCentreZ;
				const float fLx = fDx * fCos - fDz * fSin;
				const float fLz = fDx * fSin + fDz * fCos;
				if (std::fabs(fLx) <= xR.fHalfExtentX + fMargin &&
				    std::fabs(fLz) <= xR.fHalfExtentZ + fMargin)
				{
					return true;
				}
			}
			return false;
		}

		// Sample a random world XZ position that's OUTDOOR -- inside
		// the world bounds but outside every room's OBB by at least
		// fMargin. Rejection-sampling: if the random point lands in a
		// room, try again. With ~50%% indoor coverage typical of the
		// BSP output, this finds a valid point in 1-2 tries on average;
		// uMaxRetries caps the loop at 64 just so we never spin forever
		// on a pathological layout that's all-indoor (shouldn't happen
		// given the BSP's partition margin, but defensive).
		//
		// Returns true on success. fOutX/fOutZ only meaningful then.
		bool SampleOutdoorPoint(const LevelLayout& xLayout, Rng& xRng,
		                       float fMargin, uint32_t uMaxRetries,
		                       float& fOutX, float& fOutZ)
		{
			// Shrink the world bounds by margin too so outdoor samples
			// don't kiss the outer rectangle.
			const float fMinX = xLayout.fBoundsMinX + fMargin;
			const float fMaxX = xLayout.fBoundsMaxX - fMargin;
			const float fMinZ = xLayout.fBoundsMinZ + fMargin;
			const float fMaxZ = xLayout.fBoundsMaxZ - fMargin;
			if (fMinX >= fMaxX || fMinZ >= fMaxZ) return false;
			for (uint32_t i = 0; i < uMaxRetries; ++i)
			{
				const float fX = UniformF(xRng, fMinX, fMaxX);
				const float fZ = UniformF(xRng, fMinZ, fMaxZ);
				if (!PointInsideAnyRoom(xLayout, fX, fZ, fMargin))
				{
					fOutX = fX;
					fOutZ = fZ;
					return true;
				}
			}
			return false;
		}

		// Distribute uTotal villagers across the rooms in axRooms, skipping
		// any room id in axSkip. Per-room counts proportional to room area
		// (hx * hz), with a minimum of 1 villager per non-skipped room and
		// spillover going to the largest room. Then sample positions inside
		// each room using a per-villager small offset so they don't overlap.
		//
		// Determinism: caller passes the same RNG that drove BSP + yaw;
		// since this function only consumes the RNG, same seed -> same
		// villager positions.
		void DistributeVillagers(LevelLayout& xLayout, Rng& xRng,
		                         uint32_t uTotal,
		                         const Zenith_Vector<RoomId>& axSkip)
		{
			xLayout.axVillagerSpawns.Clear();
			const uint32_t uN = xLayout.axRooms.GetSize();
			if (uN == 0u || uTotal == 0u) return;

			// Build allowed-room list + areas.
			Zenith_Vector<RoomId> axAllowed;
			Zenith_Vector<float>  afArea;
			float fSumArea = 0.0f;
			for (uint32_t i = 0; i < uN; ++i)
			{
				bool bSkip = false;
				for (uint32_t s = 0; s < axSkip.GetSize(); ++s)
				{
					if (axSkip.Get(s) == static_cast<RoomId>(i)) { bSkip = true; break; }
				}
				if (bSkip) continue;
				const Room& xR = xLayout.axRooms.Get(i);
				const float fA = 4.0f * xR.fHalfExtentX * xR.fHalfExtentZ;
				axAllowed.PushBack(static_cast<RoomId>(i));
				afArea.PushBack(fA);
				fSumArea += fA;
			}
			if (axAllowed.GetSize() == 0u) return;

			// Per-room base count: min 1 each, then spread the remainder by
			// proportional rounding to the largest area.
			Zenith_Vector<uint32_t> auPerRoom;
			uint32_t uAssigned = 0u;
			for (uint32_t i = 0; i < axAllowed.GetSize(); ++i)
			{
				auPerRoom.PushBack(1u);
				++uAssigned;
			}
			if (uAssigned > uTotal)
			{
				// More rooms than villagers -- drop the surplus minimums in
				// reverse area order until we fit. Tiny rooms are skipped
				// in the under-budget case.
				while (uAssigned > uTotal)
				{
					// Find smallest-area allowed room with count >= 1.
					int iSmallest = -1;
					float fMin = 1e30f;
					for (uint32_t i = 0; i < axAllowed.GetSize(); ++i)
					{
						if (auPerRoom.Get(i) == 0u) continue;
						if (afArea.Get(i) < fMin) { fMin = afArea.Get(i); iSmallest = static_cast<int>(i); }
					}
					if (iSmallest < 0) break;
					auPerRoom.Get(static_cast<uint32_t>(iSmallest)) = 0u;
					--uAssigned;
				}
			}
			// Spill remainder proportionally: give each extra villager to
			// the largest remaining room (greedy water-filling).
			while (uAssigned < uTotal)
			{
				int iLargest = 0;
				float fMax = -1.0f;
				for (uint32_t i = 0; i < axAllowed.GetSize(); ++i)
				{
					const float fScore = afArea.Get(i) / static_cast<float>(auPerRoom.Get(i) + 1u);
					if (fScore > fMax) { fMax = fScore; iLargest = static_cast<int>(i); }
				}
				auPerRoom.Get(static_cast<uint32_t>(iLargest))++;
				++uAssigned;
			}

			// Sample per-villager positions inside each room. Use a small
			// random offset from the centre, kept inside an inner box
			// 60%% the size of the room so villagers don't clip into walls.
			for (uint32_t iR = 0; iR < axAllowed.GetSize(); ++iR)
			{
				const RoomId xR = axAllowed.Get(iR);
				const uint32_t uCount = auPerRoom.Get(iR);
				if (uCount == 0u) continue;
				const Room& xRoom = xLayout.axRooms.Get(static_cast<uint32_t>(xR));
				const float fInnerHX = xRoom.fHalfExtentX * 0.6f;
				const float fInnerHZ = xRoom.fHalfExtentZ * 0.6f;
				const float fCos = std::cos(xRoom.fYawRadians);
				const float fSin = std::sin(xRoom.fYawRadians);
				for (uint32_t v = 0; v < uCount; ++v)
				{
					VillagerSpawn xS;
					xS.xRoomId = xR;
					const float fLx = UniformF(xRng, -fInnerHX, fInnerHX);
					const float fLz = UniformF(xRng, -fInnerHZ, fInnerHZ);
					// Rotate to world with R_y.
					xS.fX = xRoom.fCentreX + fLx * fCos + fLz * fSin;
					xS.fZ = xRoom.fCentreZ - fLx * fSin + fLz * fCos;
					xS.fYawRadians = UniformF(xRng, 0.0f, 6.28318530718f);
					xLayout.axVillagerSpawns.PushBack(xS);
				}
			}
		}

		// Pick the priest spawn room: a room that is neither spawn nor
		// pentagram, weighted toward "middle" rooms (closer to neither).
		// Falls back to any non-critical room if no middle exists.
		RoomId PickPriestRoom(const LevelLayout& xLayout, RoomId xSpawnRoom, RoomId xPentRoom)
		{
			const uint32_t uN = xLayout.axRooms.GetSize();
			if (uN == 0u) return kInvalidRoomId;
			const Zenith_Vector<int32_t> aiDistFromSpawn = BfsDistances(xLayout, xSpawnRoom);
			const Zenith_Vector<int32_t> aiDistFromPent  = BfsDistances(xLayout, xPentRoom);
			RoomId xBest  = kInvalidRoomId;
			int32_t iBest = -1;
			for (uint32_t i = 0; i < uN; ++i)
			{
				const RoomId xR = static_cast<RoomId>(i);
				if (xR == xSpawnRoom || xR == xPentRoom) continue;
				const int32_t dS = aiDistFromSpawn.Get(i);
				const int32_t dP = aiDistFromPent.Get(i);
				if (dS < 0 || dP < 0) continue;
				// Score: prefer rooms where min(d_spawn, d_pent) is highest
				// (middle of the level). Tiebreak by lowest room id for
				// determinism.
				const int32_t iScore = (dS < dP) ? dS : dP;
				if (iScore > iBest) { iBest = iScore; xBest = xR; }
			}
			// Fallback if BFS scoring failed.
			if (xBest == kInvalidRoomId)
			{
				for (uint32_t i = 0; i < uN; ++i)
				{
					const RoomId xR = static_cast<RoomId>(i);
					if (xR != xSpawnRoom && xR != xPentRoom) { xBest = xR; break; }
				}
			}
			return xBest;
		}

		// Walk the corridor graph starting from xPriestRoom and emit up
		// to uMaxNodes patrol waypoints, one per visited room (centre).
		// BFS order yields a cycle that hits diverse rooms; the caller
		// closes the cycle by having the priest loop from the last node
		// back to the first.
		void BuildPatrolCycle(LevelLayout& xLayout, RoomId xPriestRoom,
		                      RoomId xPentRoom, uint32_t uMaxNodes)
		{
			xLayout.axPatrolNodes.Clear();
			if (xPriestRoom == kInvalidRoomId) return;
			const Zenith_Vector<int32_t> aiDist = BfsDistances(xLayout, xPriestRoom);
			// Collect room ids reachable from priest (dist >= 0), sorted by
			// distance ascending. Skip pentagram so the priest doesn't
			// patrol into the win-condition room.
			Zenith_Vector<RoomId>  axReach;
			Zenith_Vector<int32_t> aiReachDist;
			for (uint32_t i = 0; i < xLayout.axRooms.GetSize(); ++i)
			{
				if (aiDist.Get(i) < 0) continue;
				if (static_cast<RoomId>(i) == xPentRoom) continue;
				axReach.PushBack(static_cast<RoomId>(i));
				aiReachDist.PushBack(aiDist.Get(i));
			}
			// Bubble-sort by distance (small N).
			for (uint32_t a = 0; a + 1 < axReach.GetSize(); ++a)
			{
				for (uint32_t b = a + 1; b < axReach.GetSize(); ++b)
				{
					if (aiReachDist.Get(b) < aiReachDist.Get(a))
					{
						const int32_t tDist = aiReachDist.Get(a);
						aiReachDist.Get(a) = aiReachDist.Get(b);
						aiReachDist.Get(b) = tDist;
						const RoomId tR = axReach.Get(a);
						axReach.Get(a) = axReach.Get(b);
						axReach.Get(b) = tR;
					}
				}
			}
			const uint32_t uTake = (axReach.GetSize() < uMaxNodes) ? axReach.GetSize() : uMaxNodes;
			for (uint32_t i = 0; i < uTake; ++i)
			{
				const RoomId xR = axReach.Get(i);
				const Room& xRoom = xLayout.axRooms.Get(static_cast<uint32_t>(xR));
				PatrolNode xN;
				xN.fX = xRoom.fCentreX;
				xN.fZ = xRoom.fCentreZ;
				xN.xRoomId = xR;
				xLayout.axPatrolNodes.PushBack(xN);
			}
		}

		// Top-level AI placement. Picks priest spawn, patrols, distributes
		// villagers, and runs basic validation (>= 1 villager total, priest
		// is placed, patrol has at least one node).
		void PlaceAI(LevelLayout& xLayout, Rng& xRng, const GenConfig& xConfig)
		{
			xLayout.axVillagerSpawns.Clear();
			xLayout.axPatrolNodes.Clear();
			xLayout.xPriestSpawn = PriestSpawn();

			// Find spawn + pentagram rooms from already-placed game
			// elements.
			RoomId xSpawn = kInvalidRoomId;
			RoomId xPent  = kInvalidRoomId;
			for (uint32_t i = 0; i < xLayout.axGameElements.GetSize(); ++i)
			{
				const GameElement& xE = xLayout.axGameElements.Get(i);
				if (xE.eType == GameElementType::SpawnPoint) xSpawn = xE.xRoomId;
				if (xE.eType == GameElementType::Pentagram)  xPent  = xE.xRoomId;
			}
			if (xSpawn == kInvalidRoomId || xPent == kInvalidRoomId) return;

			// Priest in a "middle" room.
			const RoomId xPriestRoom = PickPriestRoom(xLayout, xSpawn, xPent);
			if (xPriestRoom != kInvalidRoomId)
			{
				const Room& xR = xLayout.axRooms.Get(static_cast<uint32_t>(xPriestRoom));
				xLayout.xPriestSpawn.fX           = xR.fCentreX;
				xLayout.xPriestSpawn.fZ           = xR.fCentreZ;
				xLayout.xPriestSpawn.fYawRadians  = xR.fYawRadians;
				xLayout.xPriestSpawn.xRoomId      = xPriestRoom;
				xLayout.xPriestSpawn.bValid       = true;
			}

			// Patrol cycle starting at priest's room.
			BuildPatrolCycle(xLayout, xPriestRoom, xPent, xConfig.uPatrolNodeCount);

			// Villager distribution -- skip pentagram (puzzle room) and
			// the priest's room (villagers would be eaten on spawn).
			// Split between INDOOR (room-based) and OUTDOOR (street-based)
			// so a fraction of the population loiters between buildings
			// instead of all being stuck inside rooms.
			Zenith_Vector<RoomId> axSkip;
			axSkip.PushBack(xPent);
			if (xPriestRoom != kInvalidRoomId) axSkip.PushBack(xPriestRoom);
			const uint32_t uOutdoor = (xConfig.uOutdoorVillagerCount < xConfig.uVillagerCount)
				? xConfig.uOutdoorVillagerCount : xConfig.uVillagerCount;
			const uint32_t uIndoor  = xConfig.uVillagerCount - uOutdoor;
			DistributeVillagers(xLayout, xRng, uIndoor, axSkip);

			// Outdoor villagers: rejection-sample world points outside
			// every room. Yaw is random. If the sample fails (shouldn't),
			// fall back to placing the villager at the world centre --
			// loud but visible in the visualiser so the bug is obvious.
			for (uint32_t v = 0; v < uOutdoor; ++v)
			{
				float fX = 0.0f, fZ = 0.0f;
				const bool bOk = SampleOutdoorPoint(xLayout, xRng,
					xConfig.fOutdoorMargin, 64u, fX, fZ);
				VillagerSpawn xS;
				if (bOk)
				{
					xS.fX = fX;
					xS.fZ = fZ;
				}
				else
				{
					xS.fX = 0.5f * (xLayout.fBoundsMinX + xLayout.fBoundsMaxX);
					xS.fZ = 0.5f * (xLayout.fBoundsMinZ + xLayout.fBoundsMaxZ);
					Zenith_Warning(LOG_CATEGORY_CORE,
						"DPProcLevel: outdoor villager %u sample failed, using bounds centre", v);
				}
				xS.fYawRadians = UniformF(xRng, 0.0f, 6.28318530718f);
				xS.xRoomId     = kInvalidRoomId;
				xLayout.axVillagerSpawns.PushBack(xS);
			}
		}

		// Relocate the LAST uCount objective elements (Objective5, then
		// Objective4, ...) to outdoor positions. Called from
		// PlaceGameElements after the indoor pass so the objectives
		// always exist; we just override their (x, z) and clear xRoomId.
		// Solvability check treats roomId == -1 as "always reachable
		// from spawn" -- which is true because outdoor space is
		// always connected to every room's door.
		void OutdoorRelocateObjectives(LevelLayout& xLayout, Rng& xRng,
		                              uint32_t uCount, float fMargin)
		{
			if (uCount == 0u) return;
			// Walk axGameElements backwards looking for Objective1..5; move
			// the last uCount of those outdoor.
			uint32_t uMoved = 0;
			for (int i = static_cast<int>(xLayout.axGameElements.GetSize()) - 1; i >= 0 && uMoved < uCount; --i)
			{
				GameElement& xE = xLayout.axGameElements.Get(static_cast<uint32_t>(i));
				const uint8_t u = static_cast<uint8_t>(xE.eType);
				if (u < static_cast<uint8_t>(GameElementType::Objective1) ||
				    u > static_cast<uint8_t>(GameElementType::Objective5)) continue;
				float fX = 0.0f, fZ = 0.0f;
				if (!SampleOutdoorPoint(xLayout, xRng, fMargin, 64u, fX, fZ)) continue;
				xE.fX = fX;
				xE.fZ = fZ;
				xE.xRoomId = kInvalidRoomId;
				++uMoved;
			}
		}

		// Project the partition-edge midpoint onto a room's nearest edge
		// to get a door point. The room may be rotated, so we work in the
		// room's LOCAL frame: rotate the world midpoint into local, clamp
		// to the room's local AABB, rotate back to world.
		//
		// This is "fuzzy" -- after room rotation the door point may not
		// sit on a corridor partner's edge exactly. The corridor segment
		// then has both endpoints anchored to physically-real room walls,
		// which is what we want for the eventual wall+door entity
		// authoring (Phase 1).
		DoorPoint ProjectDoorPoint(const Room& xRoom, float fWorldX, float fWorldZ)
		{
			// Translate to room-centred frame.
			const float fLocalX = fWorldX - xRoom.fCentreX;
			const float fLocalZ = fWorldZ - xRoom.fCentreZ;
			// Rotate into the room's local frame: inverse of R_y(yaw)
			// applied to (fLocalX, fLocalZ). R_y inverse swaps the sign
			// of the off-diagonal sin terms.
			const float fCos = std::cos(xRoom.fYawRadians);
			const float fSin = std::sin(xRoom.fYawRadians);
			const float fRX  =  fLocalX * fCos - fLocalZ * fSin;
			const float fRZ  =  fLocalX * fSin + fLocalZ * fCos;
			// Clamp to the room's local half-extents (so the door sits
			// on the nearest edge). The clamp picks the edge whose
			// outward normal best matches the (fRX, fRZ) direction.
			const float fAbsRX = std::fabs(fRX);
			const float fAbsRZ = std::fabs(fRZ);
			float fEdgeX, fEdgeZ;
			if (fAbsRX * xRoom.fHalfExtentZ > fAbsRZ * xRoom.fHalfExtentX)
			{
				// X-edge is "closer" (in scaled coords).
				fEdgeX = (fRX >= 0.0f) ? xRoom.fHalfExtentX : -xRoom.fHalfExtentX;
				fEdgeZ = std::clamp(fRZ, -xRoom.fHalfExtentZ, xRoom.fHalfExtentZ);
			}
			else
			{
				fEdgeZ = (fRZ >= 0.0f) ? xRoom.fHalfExtentZ : -xRoom.fHalfExtentZ;
				fEdgeX = std::clamp(fRX, -xRoom.fHalfExtentX, xRoom.fHalfExtentX);
			}
			// Rotate back to world.
			DoorPoint xD;
			xD.fX = xRoom.fCentreX + fEdgeX * fCos + fEdgeZ * fSin;
			xD.fZ = xRoom.fCentreZ - fEdgeX * fSin + fEdgeZ * fCos;
			xD.xRoomId = xRoom.id;
			return xD;
		}
	}

	bool Generate(uint64_t uSeed, const GenConfig& xConfig, LevelLayout& xOut)
	{
		// Reset output.
		xOut = LevelLayout();
		xOut.uSeed       = uSeed;
		xOut.fBoundsMinX = xConfig.fBoundsMinX;
		xOut.fBoundsMinZ = xConfig.fBoundsMinZ;
		xOut.fBoundsMaxX = xConfig.fBoundsMaxX;
		xOut.fBoundsMaxZ = xConfig.fBoundsMaxZ;

		const float fW = xConfig.fBoundsMaxX - xConfig.fBoundsMinX;
		const float fH = xConfig.fBoundsMaxZ - xConfig.fBoundsMinZ;
		if (fW < 2.0f * xConfig.fMinRoomSize || fH < 2.0f * xConfig.fMinRoomSize)
		{
			Zenith_Warning(LOG_CATEGORY_CORE,
				"DPProcLevel::Generate: bounds %gx%g too small for fMinRoomSize=%g",
				fW, fH, xConfig.fMinRoomSize);
			return false;
		}

		Rng xRng(uSeed);

		// 1. BSP partition.
		Partition xRoot;
		xRoot.fMinX = xConfig.fBoundsMinX;
		xRoot.fMinZ = xConfig.fBoundsMinZ;
		xRoot.fMaxX = xConfig.fBoundsMaxX;
		xRoot.fMaxZ = xConfig.fBoundsMaxZ;

		Zenith_Vector<Partition> axLeaves;
		BspRecurse(xRng, xConfig, xRoot, xConfig.uBspDepth, axLeaves);

		// 2. Convert partitions to rooms, then assign yaws with
		//    overlap-rejection. Rotation moves the OBB's corners outside
		//    its axis-aligned footprint, so the retry loop checks both
		//    world-bounds containment AND overlap against rooms already
		//    placed. A yaw that fails either check is rejected; after
		//    uMaxYawRetries failures the room falls back to yaw=0 (which
		//    is always safe: the BSP partitions are non-overlapping and
		//    contained in the bounds by construction).
		auto IsRotatedRoomInBounds = [&xConfig](const Room& xR) -> bool
		{
			const float fCos = std::cos(xR.fYawRadians);
			const float fSin = std::sin(xR.fYawRadians);
			const float aLocal[4][2] = {
				{ -xR.fHalfExtentX, -xR.fHalfExtentZ },
				{  xR.fHalfExtentX, -xR.fHalfExtentZ },
				{  xR.fHalfExtentX,  xR.fHalfExtentZ },
				{ -xR.fHalfExtentX,  xR.fHalfExtentZ },
			};
			for (uint32_t i = 0; i < 4; ++i)
			{
				const float fLx = aLocal[i][0];
				const float fLz = aLocal[i][1];
				const float fWx = xR.fCentreX + fLx * fCos + fLz * fSin;
				const float fWz = xR.fCentreZ - fLx * fSin + fLz * fCos;
				if (fWx < xConfig.fBoundsMinX || fWx > xConfig.fBoundsMaxX) return false;
				if (fWz < xConfig.fBoundsMinZ || fWz > xConfig.fBoundsMaxZ) return false;
			}
			return true;
		};

		for (uint32_t i = 0; i < axLeaves.GetSize(); ++i)
		{
			// Sample aspect ratio per room. Drawn BEFORE any yaw retry
			// so the room's shape is fixed and only its orientation
			// gets re-rolled. Same seed -> same aspects -> same rooms.
			const float fAspect = UniformF(xRng, xConfig.fAspectMin, xConfig.fAspectMax);
			Room xRoom = PartitionToRoom(axLeaves.Get(i), static_cast<RoomId>(i), xConfig, fAspect);

			bool bAccepted = false;
			for (uint32_t uAttempt = 0; uAttempt < xConfig.uMaxYawRetries; ++uAttempt)
			{
				xRoom.fYawRadians = UniformF(xRng, 0.0f, 6.28318530718f);  // [0, 2pi)
				if (!IsRotatedRoomInBounds(xRoom)) continue;
				bool bOverlap = false;
				for (uint32_t j = 0; j < xOut.axRooms.GetSize(); ++j)
				{
					if (OBBsOverlap(xRoom, xOut.axRooms.Get(j))) { bOverlap = true; break; }
				}
				if (!bOverlap) { bAccepted = true; break; }
			}
			if (!bAccepted) xRoom.fYawRadians = 0.0f;

			xOut.axRooms.PushBack(xRoom);
		}

		// 3. Corridors: for every BSP-adjacent leaf pair, emit a
		//    Corridor between them. Door points are the midpoint of the
		//    shared edge, projected onto each room's nearest edge.
		for (uint32_t i = 0; i < axLeaves.GetSize(); ++i)
		{
			for (uint32_t j = i + 1; j < axLeaves.GetSize(); ++j)
			{
				const Partition& xA = axLeaves.Get(i);
				const Partition& xB = axLeaves.Get(j);
				if (!PartitionsShareEdge(xA, xB)) continue;

				// Shared edge midpoint -- average the overlapping range.
				float fMidX, fMidZ;
				if (std::fabs(xA.fMaxX - xB.fMinX) < 0.001f ||
				    std::fabs(xB.fMaxX - xA.fMinX) < 0.001f)
				{
					fMidX = std::fabs(xA.fMaxX - xB.fMinX) < 0.001f ? xA.fMaxX : xB.fMaxX;
					fMidZ = 0.5f * (std::max(xA.fMinZ, xB.fMinZ) + std::min(xA.fMaxZ, xB.fMaxZ));
				}
				else
				{
					fMidZ = std::fabs(xA.fMaxZ - xB.fMinZ) < 0.001f ? xA.fMaxZ : xB.fMaxZ;
					fMidX = 0.5f * (std::max(xA.fMinX, xB.fMinX) + std::min(xA.fMaxX, xB.fMaxX));
				}

				const DoorPoint xDA = ProjectDoorPoint(xOut.axRooms.Get(i), fMidX, fMidZ);
				const DoorPoint xDB = ProjectDoorPoint(xOut.axRooms.Get(j), fMidX, fMidZ);
				const int32_t iA = static_cast<int32_t>(xOut.axDoorPoints.GetSize());
				xOut.axDoorPoints.PushBack(xDA);
				const int32_t iB = static_cast<int32_t>(xOut.axDoorPoints.GetSize());
				xOut.axDoorPoints.PushBack(xDB);

				Corridor xC;
				xC.iDoorA = iA;
				xC.iDoorB = iB;
				xOut.axCorridors.PushBack(xC);
			}
		}

		// 4. Wall segments derived from rooms + door points. Runs last so
		//    every room + door point is finalised before we cut walls.
		BuildWallSegments(xOut, xConfig, xOut.axWallSegments);

		// 5. Game elements (pentagram, forge, door, key chain, 5 objectives,
		//    chest, noise machine, spawn). Picks rooms deterministically
		//    from the room graph + door corridor; then validates the
		//    placement is solvable (BFS w/ and w/o the door).
		PlaceGameElements(xOut);

		// 5b. Relocate some objectives outdoor -- a real village has
		//     pickups scattered between buildings as well as inside.
		//     Solvability check skips kInvalidRoomId objectives so they're
		//     implicitly "always reachable" (outdoor IS always connected
		//     to every room's door).
		OutdoorRelocateObjectives(xOut, xRng,
			xConfig.uOutdoorObjectiveCount,
			xConfig.fOutdoorMargin);

		if (!ValidateSolvability(xOut))
		{
			Zenith_Warning(LOG_CATEGORY_CORE,
				"DPProcLevel::Generate: seed %llu produced an unsolvable layout",
				static_cast<unsigned long long>(uSeed));
			// Return true anyway -- the caller can decide whether to
			// re-roll with a different seed. The warning + the missing
			// solvability is visible in the JSON dump so iteration can
			// see what went wrong.
		}

		// 6. AI placement (17 villager anchors + priest spawn + patrol
		//    cycle). Depends on the game elements having been placed
		//    first so we know which rooms to skip (pentagram, priest's).
		//    Splits villagers into indoor (room-based) and outdoor
		//    (between-building) buckets per uOutdoorVillagerCount.
		PlaceAI(xOut, xRng, xConfig);

		return true;
	}
}
