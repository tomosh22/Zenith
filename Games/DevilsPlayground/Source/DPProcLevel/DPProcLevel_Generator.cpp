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

		return true;
	}
}
