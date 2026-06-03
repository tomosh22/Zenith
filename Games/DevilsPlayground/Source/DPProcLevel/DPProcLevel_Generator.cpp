#include "Zenith.h"
#include "Source/DPProcLevel/DPProcLevel_Generator.h"

#include <cmath>
#include <cstdint>

// ============================================================================
// Deterministic procgen rewrite (2026-05-19).
//
// HISTORY: The previous implementation worked in float throughout. With
// /fp:fast (the engine default), the compiler is free to fuse multiply-add
// (FMA), reorder associative operations, and use different transcendental
// implementations between optimisation levels. At least one comparison
// in BuildWallSegments was sensitive enough that it flipped between Debug
// and Release builds for the SAME seed -- producing 48 vs 47 walls
// respectively. That single divergence cascaded: villager entity IDs
// shifted by 1, the bot's pathfinder built a slightly different walkable
// grid, and the gameplay outcomes diverged so severely that the same
// 8500-frame test budget produced 13 vs 41 events across configs.
//
// FIX: All decision-making inside the generator runs on INTEGER MATH at
// millimetre precision (1 unit = 1 mm). The public LevelLayout still uses
// float (consumers unchanged), but every comparison + classification that
// shapes the output is done in int32. Conversion to float happens once
// at the end, on values only -- never on decisions.
//
// Rotation is discrete: 32 evenly-spaced angles around the circle, with
// a precomputed integer sin/cos table (Q15 fixed-point scaled by 32768).
// The original code allowed continuous yaw in radians; the new internal
// rotation enum loses no visual variety but eliminates an entire family
// of FP-sensitive decisions.
//
// Doors carry an explicit EdgeSide tag from the moment they're created.
// Wall emission reads the tag directly instead of re-deriving the edge
// from coordinates -- closing the original cross-config divergence.
//
// RNG is xoshiro128**: 128-bit state, integer-only, byte-stable across
// every IEEE754 platform. No std::uniform_real_distribution (different
// stdlib versions produce different float draws from the same engine).
// ============================================================================

namespace DPProcLevel
{
	namespace
	{
		// --------------------------------------------------------------------
		// Foundation: units, types, RNG, rotation table.
		// --------------------------------------------------------------------

		using Coord = int32_t;                  // 1 unit = 1 millimetre
		constexpr Coord kMM_PER_M = 1000;
		constexpr Coord kCoordFromF(float f)    { return static_cast<Coord>(f * kMM_PER_M); }
		// MUST use multiply-by-reciprocal rather than divide. With /fp:fast
		// (the engine default) the compiler is free to substitute
		// `(float)c / 1000.0f` with `(float)c * (1/1000.0f)`. Debug builds
		// (which use /fp:precise) emit a real divide. The two produce
		// 1-ulp-different results for some c, and those ulp differences
		// cascade through the bot's position-driven gameplay. Pinning the
		// operation to an explicit multiply removes the substitution
		// freedom and makes the conversion bit-identical across configs.
		constexpr float kFFromCoord(Coord c)    { return static_cast<float>(c) * 0.001f; }

		// 32-step discrete rotation. Steps of 11.25 deg around +Y. Plenty
		// of visual variety for a procgen village; eliminates continuous
		// yaw + the cos/sin FP cascade that came with it.
		constexpr int32_t kROT_COUNT      = 32;
		constexpr int32_t kROT_FIXED_ONE  = 32768;   // Q15.16 fixed-point scale

		// Precomputed (cos, sin) lookup, Q15 fixed-point.
		// Values computed offline with the formula: round(cos(i*2pi/32)*32768).
		// Storing the table here keeps the generator's output identical
		// regardless of std::sin/cos implementation between compilers.
		struct RotEntry { int32_t cos; int32_t sin; };
		constexpr RotEntry kROT_TABLE[kROT_COUNT] = {
			{ 32768,     0 },   // 0:    0.000 rad
			{ 32138,  6393 },   // 1:    0.196
			{ 30274, 12540 },   // 2:    0.393
			{ 27246, 18205 },   // 3:    0.589
			{ 23170, 23170 },   // 4:    0.785
			{ 18205, 27246 },   // 5:    0.982
			{ 12540, 30274 },   // 6:    1.178
			{  6393, 32138 },   // 7:    1.374
			{     0, 32768 },   // 8:    1.571 (pi/2)
			{ -6393, 32138 },   // 9:    1.767
			{-12540, 30274 },   // 10:   1.963
			{-18205, 27246 },   // 11:   2.160
			{-23170, 23170 },   // 12:   2.356
			{-27246, 18205 },   // 13:   2.553
			{-30274, 12540 },   // 14:   2.749
			{-32138,  6393 },   // 15:   2.945
			{-32768,     0 },   // 16:   3.142 (pi)
			{-32138, -6393 },   // 17:   3.338
			{-30274,-12540 },   // 18:   3.534
			{-27246,-18205 },   // 19:   3.731
			{-23170,-23170 },   // 20:   3.927
			{-18205,-27246 },   // 21:   4.123
			{-12540,-30274 },   // 22:   4.320
			{ -6393,-32138 },   // 23:   4.516
			{     0,-32768 },   // 24:   4.712 (3pi/2)
			{  6393,-32138 },   // 25:   4.909
			{ 12540,-30274 },   // 26:   5.105
			{ 18205,-27246 },   // 27:   5.301
			{ 23170,-23170 },   // 28:   5.498
			{ 27246,-18205 },   // 29:   5.694
			{ 30274,-12540 },   // 30:   5.890
			{ 32138, -6393 },   // 31:   6.087
		};

		// Convert a discrete rotation index to its float yaw in radians.
		// Only used at the int -> float conversion at output time.
		float YawFromRot(int32_t iRot)
		{
			constexpr float kTwoPi = 6.28318530717958647692f;
			return (static_cast<float>(iRot) * kTwoPi) / static_cast<float>(kROT_COUNT);
		}

		// Rotate an integer 2D point by a discrete rotation index. The
		// rotation matrix matches PR #95's R_y convention:
		//   world.x = lx*cos + lz*sin
		//   world.z = -lx*sin + lz*cos
		// All integer; the >>15 shifts undo the Q15 scaling.
		void RotPoint(Coord lx, Coord lz, int32_t iRot, Coord& wx, Coord& wz)
		{
			const RotEntry& r = kROT_TABLE[iRot];
			// 64-bit accumulators so coord*cos doesn't overflow.
			const int64_t cx = static_cast<int64_t>(lx) * r.cos;
			const int64_t sx = static_cast<int64_t>(lx) * r.sin;
			const int64_t cz = static_cast<int64_t>(lz) * r.cos;
			const int64_t sz = static_cast<int64_t>(lz) * r.sin;
			// (>>15 to undo Q15 scaling. Arithmetic shift on negative ints
			//  is implementation-defined in C++17 but always-arithmetic
			//  in C++20 -- this codebase targets /std:c++20.)
			wx = static_cast<Coord>((cx + sz) >> 15);
			wz = static_cast<Coord>((-sx + cz) >> 15);
		}

		// Side of a room's local-frame axis-aligned box. Tagged onto every
		// DoorI at placement time so wall emission never has to re-classify.
		enum class EdgeSide : uint8_t
		{
			Bottom = 0,   // lz = -fHz, axis = lx
			Right  = 1,   // lx = +fHx, axis = lz
			Top    = 2,   // lz = +fHz, axis = lx
			Left   = 3,   // lx = -fHx, axis = lz
		};

		// --------------------------------------------------------------------
		// Deterministic RNG (xoshiro128**). 128-bit state; output is uint32_t.
		// Integer-only API so no platform/stdlib divergence on FP draws.
		// Reference: https://prng.di.unimi.it/xoshiro128starstar.c
		// --------------------------------------------------------------------
		struct Rng
		{
			uint32_t s0, s1, s2, s3;
		};

		static inline uint32_t Rotl(uint32_t x, int k)
		{
			return (x << k) | (x >> (32 - k));
		}

		uint32_t RngNext(Rng& rng)
		{
			const uint32_t result = Rotl(rng.s1 * 5u, 7) * 9u;
			const uint32_t t = rng.s1 << 9;
			rng.s2 ^= rng.s0;
			rng.s3 ^= rng.s1;
			rng.s1 ^= rng.s2;
			rng.s0 ^= rng.s3;
			rng.s2 ^= t;
			rng.s3 = Rotl(rng.s3, 11);
			return result;
		}

		Rng RngInit(uint64_t uSeed)
		{
			// splitmix64 to expand a uint64 seed into four uint32 state words.
			// Guarantees a non-zero state even for uSeed == 0.
			auto SplitMix = [](uint64_t& z) -> uint64_t
			{
				z += 0x9e3779b97f4a7c15ull;
				uint64_t v = z;
				v = (v ^ (v >> 30)) * 0xbf58476d1ce4e5b9ull;
				v = (v ^ (v >> 27)) * 0x94d049bb133111ebull;
				return v ^ (v >> 31);
			};
			uint64_t z = uSeed;
			const uint64_t a = SplitMix(z);
			const uint64_t b = SplitMix(z);
			Rng rng;
			rng.s0 = static_cast<uint32_t>(a);
			rng.s1 = static_cast<uint32_t>(a >> 32);
			rng.s2 = static_cast<uint32_t>(b);
			rng.s3 = static_cast<uint32_t>(b >> 32);
			// xoshiro requires a non-zero state. SplitMix64 from non-zero
			// input always produces non-zero output, but seed = 0 maps to
			// a non-zero state through the additive constant; assert as
			// belt-and-braces.
			if ((rng.s0 | rng.s1 | rng.s2 | rng.s3) == 0u) rng.s0 = 0xdeadbeefu;
			return rng;
		}

		// Uniform integer in [lo, hi] (inclusive). Integer arithmetic only.
		int32_t RngRangeI(Rng& rng, int32_t lo, int32_t hi)
		{
			if (lo >= hi) return lo;
			const uint32_t range = static_cast<uint32_t>(hi - lo + 1);
			// Lemire's debiased bounded-int trick; deterministic, no
			// floating-point conversion.
			uint64_t m = static_cast<uint64_t>(RngNext(rng)) * range;
			uint32_t l = static_cast<uint32_t>(m);
			if (l < range)
			{
				const uint32_t t = (0u - range) % range;
				while (l < t)
				{
					m = static_cast<uint64_t>(RngNext(rng)) * range;
					l = static_cast<uint32_t>(m);
				}
			}
			return lo + static_cast<int32_t>(m >> 32);
		}

		// --------------------------------------------------------------------
		// Internal integer-coord level layout. The public LevelLayout (float)
		// is filled at the very end by ConvertToFloat.
		// --------------------------------------------------------------------
		struct PartitionI
		{
			Coord minX, minZ, maxX, maxZ;
			RoomId xLeafRoomId = kInvalidRoomId;
		};

		struct RoomI
		{
			RoomId  id      = kInvalidRoomId;
			Coord   cx      = 0;
			Coord   cz      = 0;
			Coord   hx      = 0;
			Coord   hz      = 0;
			int32_t iRot    = 0;     // index into kROT_TABLE
		};

		struct DoorI
		{
			Coord    x = 0;
			Coord    z = 0;
			RoomId   xRoomId = kInvalidRoomId;
			EdgeSide eEdge   = EdgeSide::Bottom;
			Coord    fLocalPos = 0;  // position along the edge's axis in room-local frame
			// 2026-05-25: discrete rotation index for the WALL direction this
			// door sits in (so the emitted DPDoor entity's "wide" local +Z
			// lies along the wall). Bottom/Top edges run along the room's
			// local +X, so wallRot = roomRot + π/2; Left/Right edges run
			// along local +Z, so wallRot = roomRot. Computed at door-creation
			// time so float conversion stays bit-deterministic.
			int32_t  iWallRot = 0;
		};

		struct WallSegmentI
		{
			Coord   cx, cz;
			Coord   hx, hz;
			int32_t iRot;
		};

		// --------------------------------------------------------------------
		// Geometry helpers (integer).
		// --------------------------------------------------------------------

		// Integer OBB-vs-OBB overlap test via separating-axis theorem.
		// Inputs are integer half-extents + rotation indices; the test is
		// deterministic regardless of compiler / FMA / optimisation level
		// because every operation is 64-bit integer arithmetic.
		//
		// We test the 4 axes formed by the two OBBs' local frames (the
		// "fastest separators" -- 4 axes are sufficient for 2D OBBs).
		bool OBBsOverlap_I(const RoomI& A, const RoomI& B)
		{
			const RotEntry& rA = kROT_TABLE[A.iRot];
			const RotEntry& rB = kROT_TABLE[B.iRot];

			// Axis vectors in Q15 fixed point. A's local +X axis maps to
			// world ( cosA, -sinA), local +Z maps to world ( sinA,  cosA).
			// (PR #95 R_y convention.)
			struct AxisQ15 { int32_t x, z; };
			const AxisQ15 ax[4] = {
				{ rA.cos,  -rA.sin },   // A's local +X
				{ rA.sin,   rA.cos },   // A's local +Z
				{ rB.cos,  -rB.sin },   // B's local +X
				{ rB.sin,   rB.cos },   // B's local +Z
			};

			// Centre-to-centre delta (mm)
			const int64_t dx = static_cast<int64_t>(B.cx - A.cx);
			const int64_t dz = static_cast<int64_t>(B.cz - A.cz);

			for (int a = 0; a < 4; ++a)
			{
				const int64_t nx = ax[a].x;   // Q15
				const int64_t nz = ax[a].z;   // Q15

				// Project each OBB's half-extents onto axis n. The
				// projection of half-extent vector (hx, 0) rotated by
				// OBB's local frame onto axis n is:
				//   |hx * dot(OBB_localX, n)|
				// In Q15 * mm = Q15 * Coord units.
				auto AbsInt64 = [](int64_t v) -> int64_t { return v < 0 ? -v : v; };

				// A's projection: hx * |dot(A_localX, n)| + hz * |dot(A_localZ, n)|
				const int64_t aDot_xX = static_cast<int64_t>(rA.cos) * nx + static_cast<int64_t>(-rA.sin) * nz;
				const int64_t aDot_xZ = static_cast<int64_t>(rA.sin) * nx + static_cast<int64_t>(rA.cos)  * nz;
				const int64_t projA = (
					AbsInt64(static_cast<int64_t>(A.hx) * aDot_xX) +
					AbsInt64(static_cast<int64_t>(A.hz) * aDot_xZ)
				);  // Q15 * mm * mm/(Q15*mm) = Q30 * mm

				// B's projection (same shape).
				const int64_t bDot_xX = static_cast<int64_t>(rB.cos) * nx + static_cast<int64_t>(-rB.sin) * nz;
				const int64_t bDot_xZ = static_cast<int64_t>(rB.sin) * nx + static_cast<int64_t>(rB.cos)  * nz;
				const int64_t projB = (
					AbsInt64(static_cast<int64_t>(B.hx) * bDot_xX) +
					AbsInt64(static_cast<int64_t>(B.hz) * bDot_xZ)
				);

				// Centre projection onto n
				const int64_t projC = AbsInt64(dx * nx + dz * nz);  // Q15 * mm

				// projC is Q15 * mm = scaled by 2^15
				// projA / projB are sum of (Coord * Q30) -- they're scaled
				// by 2^30. Bring projC to Q30 to compare:
				const int64_t projC_Q30 = projC * kROT_FIXED_ONE;

				if (projC_Q30 > projA + projB) return false;
			}
			return true;
		}

		// True iff all 4 corners of the (rotated) room sit inside the
		// integer world bounds. Used to reject yaw choices that would
		// rotate a room out of the level.
		bool IsRotatedRoomInBoundsI(const RoomI& R,
		                            Coord boundsMinX, Coord boundsMinZ,
		                            Coord boundsMaxX, Coord boundsMaxZ)
		{
			const Coord aLocal[4][2] = {
				{ -R.hx, -R.hz },
				{  R.hx, -R.hz },
				{  R.hx,  R.hz },
				{ -R.hx,  R.hz },
			};
			for (int i = 0; i < 4; ++i)
			{
				Coord wx, wz;
				RotPoint(aLocal[i][0], aLocal[i][1], R.iRot, wx, wz);
				wx += R.cx;
				wz += R.cz;
				if (wx < boundsMinX || wx > boundsMaxX) return false;
				if (wz < boundsMinZ || wz > boundsMaxZ) return false;
			}
			return true;
		}

		// --------------------------------------------------------------------
		// BSP partition (integer).
		// --------------------------------------------------------------------

		void BspRecurse_I(Rng& rng,
		                  const PartitionI& P,
		                  Coord minRoomSize,
		                  uint32_t uDepthRemaining,
		                  Zenith_Vector<PartitionI>& outLeaves)
		{
			const Coord W = P.maxX - P.minX;
			const Coord H = P.maxZ - P.minZ;
			const bool bCanSplitX = (W >= 2 * minRoomSize);
			const bool bCanSplitZ = (H >= 2 * minRoomSize);

			if (uDepthRemaining == 0u || (!bCanSplitX && !bCanSplitZ))
			{
				PartitionI leaf = P;
				leaf.xLeafRoomId = static_cast<RoomId>(outLeaves.GetSize());
				outLeaves.PushBack(leaf);
				return;
			}

			// Prefer the longer axis to avoid sliver partitions.
			const bool bSplitOnX = bCanSplitX && (!bCanSplitZ || (W >= H));

			PartitionI lo = P, hi = P;
			if (bSplitOnX)
			{
				// Split in the middle 50%: [25%, 75%] of the axis range.
				const Coord splitMin = P.minX + (W >> 2);
				const Coord splitMax = P.minX + (3 * W) / 4;
				const Coord split = static_cast<Coord>(RngRangeI(rng, splitMin, splitMax));
				lo.maxX = split;
				hi.minX = split;
			}
			else
			{
				const Coord splitMin = P.minZ + (H >> 2);
				const Coord splitMax = P.minZ + (3 * H) / 4;
				const Coord split = static_cast<Coord>(RngRangeI(rng, splitMin, splitMax));
				lo.maxZ = split;
				hi.minZ = split;
			}

			BspRecurse_I(rng, lo, minRoomSize, uDepthRemaining - 1, outLeaves);
			BspRecurse_I(rng, hi, minRoomSize, uDepthRemaining - 1, outLeaves);
		}

		// Two partitions are BSP-adjacent if they share a non-zero segment
		// of an edge. Integer equality check; no FP tolerance.
		bool PartitionsShareEdgeI(const PartitionI& A, const PartitionI& B)
		{
			// Vertical shared edge: one's right matches the other's left,
			// AND Z ranges overlap by at least 1mm.
			if (A.maxX == B.minX || B.maxX == A.minX)
			{
				const Coord zLo = (A.minZ > B.minZ) ? A.minZ : B.minZ;
				const Coord zHi = (A.maxZ < B.maxZ) ? A.maxZ : B.maxZ;
				return (zHi - zLo) > 0;
			}
			if (A.maxZ == B.minZ || B.maxZ == A.minZ)
			{
				const Coord xLo = (A.minX > B.minX) ? A.minX : B.minX;
				const Coord xHi = (A.maxX < B.maxX) ? A.maxX : B.maxX;
				return (xHi - xLo) > 0;
			}
			return false;
		}

		// --------------------------------------------------------------------
		// Discrete aspect ratios for room sizing. The previous code sampled
		// a continuous aspect float in [fAspectMin, fAspectMax]; we use a
		// discrete table mapped to that range so integer hx/hz pairs are
		// deterministic. Five aspects gives visible rectangular variety
		// without sliver-shaped rooms.
		// --------------------------------------------------------------------
		struct AspectEntry
		{
			// hx and hz factors, scaled by kASPECT_SCALE, such that
			// hx_factor / kASPECT_SCALE = hx / safeR and similarly for hz.
			// Precomputed for aspect a: factor_x = 1/sqrt(1+a*a),
			// factor_z = a/sqrt(1+a*a). Same formula as the original code
			// but baked at compile time so no per-room sqrt is needed.
			int32_t hxFactor;
			int32_t hzFactor;
		};
		constexpr int32_t kASPECT_SCALE = 32768;  // Q15
		constexpr AspectEntry kASPECT_TABLE[] = {
			// a = 0.5  -> hx_factor = 1/sqrt(1.25) = 0.8944, hz = 0.4472
			{ 29309, 14654 },
			// a = 0.75 -> hx = 0.8000, hz = 0.6000
			{ 26214, 19661 },
			// a = 1.0  -> hx = 0.7071, hz = 0.7071
			{ 23170, 23170 },
			// a = 1.5  -> hx = 0.5547, hz = 0.8321
			{ 18176, 27263 },
			// a = 2.0  -> hx = 0.4472, hz = 0.8944
			{ 14654, 29309 },
		};
		constexpr int32_t kASPECT_COUNT = sizeof(kASPECT_TABLE) / sizeof(kASPECT_TABLE[0]);

		// Pick an aspect entry within [fAspectMin, fAspectMax]. Maps the
		// float range to a deterministic discrete subset; same RNG sequence
		// produces the same aspect.
		const AspectEntry& PickAspect_I(Rng& rng, float fMin, float fMax)
		{
			// Determine which table indices fall inside [fMin, fMax]. The
			// discrete aspects are 0.5, 0.75, 1.0, 1.5, 2.0 -- precompute
			// the eligibility table at compile time to keep it FP-free at
			// the comparison step.
			constexpr float kAspectValue[kASPECT_COUNT] = { 0.5f, 0.75f, 1.0f, 1.5f, 2.0f };
			int32_t aiOk[kASPECT_COUNT];
			int32_t nOk = 0;
			for (int32_t i = 0; i < kASPECT_COUNT; ++i)
			{
				// Comparison happens once at startup time per Generate() call;
				// the relative ordering of these floats is stable across
				// every IEEE754 platform (no FMA-sensitive products here).
				if (kAspectValue[i] >= fMin && kAspectValue[i] <= fMax)
				{
					aiOk[nOk++] = i;
				}
			}
			if (nOk == 0)
			{
				// Bounds entirely outside the table; fall back to closest.
				return kASPECT_TABLE[2];
			}
			const int32_t pick = RngRangeI(rng, 0, nOk - 1);
			return kASPECT_TABLE[aiOk[pick]];
		}

		// Build a room from a partition.
		RoomI PartitionToRoom_I(const PartitionI& P, RoomId xId,
		                        Coord maxRoomSize,
		                        const AspectEntry& aspect)
		{
			const Coord MARGIN = 500;  // 0.5 m
			const Coord pHX = (P.maxX - P.minX) / 2;
			const Coord pHZ = (P.maxZ - P.minZ) / 2;
			const Coord safeR = ((pHX < pHZ) ? pHX : pHZ) - MARGIN;
			Coord hx = static_cast<Coord>((static_cast<int64_t>(safeR) * aspect.hxFactor) / kASPECT_SCALE);
			Coord hz = static_cast<Coord>((static_cast<int64_t>(safeR) * aspect.hzFactor) / kASPECT_SCALE);
			const Coord maxDim = (hx > hz) ? hx : hz;
			if (maxDim > maxRoomSize)
			{
				// Scale down proportionally (integer divide; loses < 1mm).
				hx = static_cast<Coord>((static_cast<int64_t>(hx) * maxRoomSize) / maxDim);
				hz = static_cast<Coord>((static_cast<int64_t>(hz) * maxRoomSize) / maxDim);
			}
			RoomI R;
			R.id = xId;
			R.cx = (P.minX + P.maxX) / 2;
			R.cz = (P.minZ + P.maxZ) / 2;
			R.hx = hx;
			R.hz = hz;
			R.iRot = 0;
			return R;
		}

		// --------------------------------------------------------------------
		// Door point projection. Returns a DoorI with the EdgeSide explicitly
		// tagged -- so wall emission later doesn't have to re-classify from
		// world coordinates. This kills the original FP-sensitive
		// re-derivation.
		// --------------------------------------------------------------------
		DoorI ProjectDoorPoint_I(const RoomI& R, Coord worldX, Coord worldZ,
		                         Coord doorHalf, Coord halfThick)
		{
			// Translate to room-centred frame, then INVERSE rotate into the
			// room's local frame.
			const Coord dx = worldX - R.cx;
			const Coord dz = worldZ - R.cz;
			// Inverse of R_y(iRot) is R_y(-iRot). For an integer table we
			// either negate the rotation index or apply the inverse rotation
			// formula directly:
			//   localX = dx*cos + dz*(-sin)  (since R^-1 = R^T for rotations)
			//   localZ = dx*sin + dz*cos
			// (Compare to RotPoint which uses the forward rotation.)
			const RotEntry& r = kROT_TABLE[R.iRot];
			const int64_t lx64 = (static_cast<int64_t>(dx) * r.cos - static_cast<int64_t>(dz) * r.sin) >> 15;
			const int64_t lz64 = (static_cast<int64_t>(dx) * r.sin + static_cast<int64_t>(dz) * r.cos) >> 15;
			const Coord lx = static_cast<Coord>(lx64);
			const Coord lz = static_cast<Coord>(lz64);

			// Classify by which edge is "closer" -- the X-edge wins iff
			// |lx| / hx > |lz| / hz, i.e. |lx|*hz > |lz|*hx. Pure integer
			// comparison; no FMA-sensitive products.
			const Coord absLx = (lx < 0) ? -lx : lx;
			const Coord absLz = (lz < 0) ? -lz : lz;
			const int64_t lhs = static_cast<int64_t>(absLx) * R.hz;
			const int64_t rhs = static_cast<int64_t>(absLz) * R.hx;

			// Clamp the position-along-edge so the door's 2*doorHalf gap stays
			// at least halfThick away from each room corner. Without this, a
			// projected door near a corner carved a wall opening straight
			// through the corner, leaving the corner visually broken even
			// though the wall emitter closes the corner geometrically.
			auto ClampAwayFromCorner = [doorHalf, halfThick](Coord v, Coord axisMax) -> Coord
			{
				const Coord margin = doorHalf + halfThick;
				const Coord lo = (margin < axisMax) ? (-axisMax + margin) : 0;
				const Coord hi = (margin < axisMax) ? ( axisMax - margin) : 0;
				if (v < lo) return lo;
				if (v > hi) return hi;
				return v;
			};

			EdgeSide eEdge;
			Coord edgeLx, edgeLz, posAlongEdge;
			if (lhs > rhs)
			{
				// X-edge wins. Snap to ±hx; door slides along lz on this edge.
				const Coord clz = ClampAwayFromCorner(lz, R.hz);
				if (lx >= 0)
				{
					eEdge = EdgeSide::Right;
					edgeLx = R.hx;
				}
				else
				{
					eEdge = EdgeSide::Left;
					edgeLx = -R.hx;
				}
				edgeLz = clz;
				posAlongEdge = clz;
			}
			else
			{
				// Z-edge wins. Snap to ±hz; door slides along lx on this edge.
				const Coord clx = ClampAwayFromCorner(lx, R.hx);
				if (lz >= 0)
				{
					eEdge = EdgeSide::Top;
					edgeLz = R.hz;
				}
				else
				{
					eEdge = EdgeSide::Bottom;
					edgeLz = -R.hz;
				}
				edgeLx = clx;
				posAlongEdge = clx;
			}

			// Rotate the snapped local point back to world.
			Coord wx, wz;
			RotPoint(edgeLx, edgeLz, R.iRot, wx, wz);

			// 2026-05-25: derive the WALL direction's rotation index from
			// the room rotation + which edge we landed on. Bottom/Top edges
			// run along room-local +X (so wallRot = roomRot + π/2);
			// Left/Right edges run along room-local +Z (so wallRot =
			// roomRot). kROT_COUNT is 32 (a power of two), so the bitmask
			// wrap is exact.
			constexpr int32_t kQuarterTurn = kROT_COUNT / 4;   // 8
			const int32_t iQuarterOffset =
				(eEdge == EdgeSide::Bottom || eEdge == EdgeSide::Top) ? kQuarterTurn : 0;
			const int32_t iWallRot = (R.iRot + iQuarterOffset) & (kROT_COUNT - 1);

			DoorI D;
			D.x = R.cx + wx;
			D.z = R.cz + wz;
			D.xRoomId = R.id;
			D.eEdge = eEdge;
			D.fLocalPos = posAlongEdge;
			D.iWallRot = iWallRot;
			return D;
		}

		// --------------------------------------------------------------------
		// Wall segment emission. A building is a rotated rectangle; its
		// walls are the four edges, each a rotated cuboid of thickness
		// 2*halfThick. Each wall covers its edge from one OUTER corner to
		// the other -- i.e. it extends past the room's local axisMax by
		// halfThick on each side, so adjacent perpendicular walls overlap
		// at every corner and the outline closes without any special-case
		// corner geometry. Doors are gaps in those rectangles: each door
		// carves [doorPos - doorHalf, doorPos + doorHalf] out of its
		// edge's wall (clipped to the wall's full range so a door near a
		// corner can't carve outside the mesh).
		//
		// Each door's EdgeSide is set at door-creation time; this routine
		// reads the tag verbatim and never re-classifies from coordinates,
		// which kept the original FP-divergent re-derivation out of the
		// integer pipeline.
		// --------------------------------------------------------------------

		// Emit one wall sub-segment along an X- or Z-edge of room R,
		// spanning [A, B] on the edge axis with perpendicular centerline
		// at perpPos. Half-thickness perpendicular to the edge is
		// halfThick. No-op for degenerate (<1 mm) spans.
		void EmitOneWall_I(const RoomI& R, bool bIsXEdge,
		                   Coord perpPos, Coord A, Coord B, Coord halfThick,
		                   Zenith_Vector<WallSegmentI>& xOut)
		{
			const Coord segHalf = (B - A) / 2;
			if (segHalf < 1) return;
			const Coord segCentre = (A + B) / 2;

			Coord localCX, localCZ;
			if (bIsXEdge) { localCX = segCentre; localCZ = perpPos;   }
			else          { localCX = perpPos;   localCZ = segCentre; }

			Coord wx, wz;
			RotPoint(localCX, localCZ, R.iRot, wx, wz);

			WallSegmentI W;
			W.cx   = R.cx + wx;
			W.cz   = R.cz + wz;
			W.iRot = R.iRot;
			if (bIsXEdge) { W.hx = segHalf;   W.hz = halfThick; }
			else          { W.hx = halfThick; W.hz = segHalf;   }
			xOut.PushBack(W);
		}

		void EmitWallSegments_I(const Zenith_Vector<RoomI>& axRooms,
		                        const Zenith_Vector<DoorI>& axDoors,
		                        Coord halfThick,
		                        Coord doorHalf,
		                        Zenith_Vector<WallSegmentI>& xOut)
		{
			xOut.Clear();

			// Edge layout: perpendicular position of each wall's centerline,
			// and whether the wall runs along lx (X-edge) or lz (Z-edge).
			//   0 = Bottom (lz = -hz, axis = lx)
			//   1 = Right  (lx = +hx, axis = lz)
			//   2 = Top    (lz = +hz, axis = lx)
			//   3 = Left   (lx = -hx, axis = lz)
			const bool abIsXEdge[4] = { true, false, true, false };

			for (uint32_t uR = 0; uR < axRooms.GetSize(); ++uR)
			{
				const RoomI& R = axRooms.Get(uR);

				const Coord aPerpPos[4] = { -R.hz, +R.hx, +R.hz, -R.hx };

				// Bucket doors by their pre-tagged edge.
				Zenith_Vector<Coord> aaDoorsAlongEdge[4];
				for (uint32_t uD = 0; uD < axDoors.GetSize(); ++uD)
				{
					const DoorI& D = axDoors.Get(uD);
					if (D.xRoomId != R.id) continue;
					aaDoorsAlongEdge[static_cast<int>(D.eEdge)].PushBack(D.fLocalPos);
				}

				for (int iE = 0; iE < 4; ++iE)
				{
					const Coord axisMax = abIsXEdge[iE] ? R.hx : R.hz;
					// Wall covers the full edge plus halfThick past each
					// corner -- this is the entire corner-closure mechanism.
					const Coord wallMin = -(axisMax + halfThick);
					const Coord wallMax = +(axisMax + halfThick);

					Zenith_Vector<Coord>& axDoorsOnE = aaDoorsAlongEdge[iE];

					// Bubble-sort (small N -- typically <= 2 doors per edge).
					for (uint32_t a = 0; a + 1 < axDoorsOnE.GetSize(); ++a)
					{
						for (uint32_t b = a + 1; b < axDoorsOnE.GetSize(); ++b)
						{
							if (axDoorsOnE.Get(b) < axDoorsOnE.Get(a))
							{
								const Coord t = axDoorsOnE.Get(a);
								axDoorsOnE.Get(a) = axDoorsOnE.Get(b);
								axDoorsOnE.Get(b) = t;
							}
						}
					}

					// Walk the wall span; emit a sub-rectangle between each
					// pair of door gaps. Door gaps are clipped to the room
					// edge [-axisMax, +axisMax] (NOT the full wall range)
					// so the halfThick corner-extension strip at each end
					// of the wall always survives, even when a door sits
					// close enough to a corner that its raw gap would
					// otherwise carve into that strip. This is what
					// guarantees every corner is closed regardless of
					// door placement.
					Coord cursor = wallMin;
					for (uint32_t uD = 0; uD < axDoorsOnE.GetSize(); ++uD)
					{
						const Coord doorPos = axDoorsOnE.Get(uD);
						const Coord rawStart = doorPos - doorHalf;
						const Coord rawEnd   = doorPos + doorHalf;
						const Coord gapStart = (rawStart < -axisMax) ? -axisMax : rawStart;
						const Coord gapEnd   = (rawEnd   > +axisMax) ? +axisMax : rawEnd;
						if (gapStart > cursor)
						{
							EmitOneWall_I(R, abIsXEdge[iE], aPerpPos[iE],
								cursor, gapStart, halfThick, xOut);
						}
						if (gapEnd > cursor) cursor = gapEnd;
					}
					if (cursor < wallMax)
					{
						EmitOneWall_I(R, abIsXEdge[iE], aPerpPos[iE],
							cursor, wallMax, halfThick, xOut);
					}
				}
			}
		}

		// --------------------------------------------------------------------
		// Graph helpers (already integer; pulled from the original code).
		// --------------------------------------------------------------------
		Zenith_Vector<bool> DfsReachable(const LevelLayout& xLayout, RoomId iStart, int32_t iSkipCorridor)
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

		// --------------------------------------------------------------------
		// Outdoor sampling -- integer rejection sampling.
		// --------------------------------------------------------------------
		bool PointInsideAnyRoomI(const Zenith_Vector<RoomI>& axRooms,
		                         Coord px, Coord pz, Coord margin)
		{
			for (uint32_t i = 0; i < axRooms.GetSize(); ++i)
			{
				const RoomI& R = axRooms.Get(i);
				// Inverse-rotate the world point into the room's local frame.
				const Coord dx = px - R.cx;
				const Coord dz = pz - R.cz;
				const RotEntry& r = kROT_TABLE[R.iRot];
				const Coord lx = static_cast<Coord>(
					(static_cast<int64_t>(dx) * r.cos - static_cast<int64_t>(dz) * r.sin) >> 15);
				const Coord lz = static_cast<Coord>(
					(static_cast<int64_t>(dx) * r.sin + static_cast<int64_t>(dz) * r.cos) >> 15);
				const Coord absLx = (lx < 0) ? -lx : lx;
				const Coord absLz = (lz < 0) ? -lz : lz;
				if (absLx <= R.hx + margin && absLz <= R.hz + margin) return true;
			}
			return false;
		}

		bool SampleOutdoorPointI(const Zenith_Vector<RoomI>& axRooms,
		                         Coord minX, Coord maxX, Coord minZ, Coord maxZ,
		                         Rng& rng, Coord margin, uint32_t uMaxRetries,
		                         Coord& outX, Coord& outZ)
		{
			const Coord lx = minX + margin;
			const Coord hx = maxX - margin;
			const Coord lz = minZ + margin;
			const Coord hz = maxZ - margin;
			if (lx >= hx || lz >= hz) return false;
			for (uint32_t i = 0; i < uMaxRetries; ++i)
			{
				const Coord x = static_cast<Coord>(RngRangeI(rng, lx, hx));
				const Coord z = static_cast<Coord>(RngRangeI(rng, lz, hz));
				if (!PointInsideAnyRoomI(axRooms, x, z, margin))
				{
					outX = x;
					outZ = z;
					return true;
				}
			}
			return false;
		}

		// --------------------------------------------------------------------
		// Int -> Float conversion of the final layout. Only happens here.
		// --------------------------------------------------------------------
		void ToFloat_Room(const RoomI& I, Room& F)
		{
			F.id = I.id;
			F.fCentreX     = kFFromCoord(I.cx);
			F.fCentreZ     = kFFromCoord(I.cz);
			F.fHalfExtentX = kFFromCoord(I.hx);
			F.fHalfExtentZ = kFFromCoord(I.hz);
			F.fYawRadians  = YawFromRot(I.iRot);
		}

		void ToFloat_Door(const DoorI& I, DoorPoint& F)
		{
			F.fX = kFFromCoord(I.x);
			F.fZ = kFFromCoord(I.z);
			F.xRoomId = I.xRoomId;
			F.fWallYawRadians = YawFromRot(I.iWallRot);
		}

		void ToFloat_Wall(const WallSegmentI& I, WallSegment& F)
		{
			F.fCentreX     = kFFromCoord(I.cx);
			F.fCentreZ     = kFFromCoord(I.cz);
			F.fHalfExtentX = kFFromCoord(I.hx);
			F.fHalfExtentZ = kFFromCoord(I.hz);
			F.fYawRadians  = YawFromRot(I.iRot);
		}

		// --------------------------------------------------------------------
		// Game-element placement. Uses the same algorithm as the original:
		// pick rooms deterministically via BFS distance, place doors on every
		// pentagram-adjacent corridor, validate solvability. The math is all
		// integer (BFS is already integer); the only float math was the 6-
		// point circle offset for stacked elements, which is now done via
		// kROT_TABLE.
		// --------------------------------------------------------------------
		void PlaceGameElements_I(const Zenith_Vector<RoomI>& axRoomsI,
		                         LevelLayout& xLayout,
		                         const GenConfig& xConfig,
		                         uint64_t uSeed)
		{
			xLayout.axGameElements.Clear();
			const uint32_t uN = axRoomsI.GetSize();
			if (uN == 0u) return;

			// Per-room counter so multi-element rooms don't stack on the same
			// coordinate; subsequent elements distribute around a 6-point
			// circle at 50% of the room's min half-extent.
			Zenith_Vector<int32_t> aiRoomElemCount;
			for (uint32_t i = 0; i < uN; ++i) aiRoomElemCount.PushBack(0);

			auto RoomElement = [&axRoomsI, &aiRoomElemCount](GameElementType eType, RoomId xRoom) -> GameElement
			{
				GameElement xE;
				xE.eType   = eType;
				xE.xRoomId = xRoom;
				if (xRoom < 0 || xRoom >= static_cast<RoomId>(axRoomsI.GetSize()))
				{
					return xE;
				}
				const RoomI& R = axRoomsI.Get(static_cast<uint32_t>(xRoom));
				const int32_t iIdx = aiRoomElemCount.Get(static_cast<uint32_t>(xRoom));
				aiRoomElemCount.Get(static_cast<uint32_t>(xRoom)) = iIdx + 1;
				if (iIdx == 0)
				{
					xE.fX = kFFromCoord(R.cx);
					xE.fZ = kFFromCoord(R.cz);
				}
				else
				{
					// 6-point circle in the room's local frame.
					// Rotation index: i * (32/6) = i * 5.333... -> table mod 32.
					// To stay integer-deterministic, map slot i to:
					//   iRotOffset = (i - 1) * 5  (steps of 5/32 of full circle)
					// 5 isn't an exact divisor of 32 but it gives 6 distinct
					// directions in [0, 32): {5, 10, 15, 20, 25, 30}.
					const int32_t iRotOffset = ((iIdx - 1) * 5) & (kROT_COUNT - 1);
					// Offset distance = 50% of min(hx, hz).
					const Coord minHalf = (R.hx < R.hz) ? R.hx : R.hz;
					const Coord offset = minHalf / 2;
					// Local circle point at (offset, 0) rotated by iRotOffset.
					Coord lx, lz;
					RotPoint(offset, 0, iRotOffset, lx, lz);
					// Then rotate by the room's yaw into world.
					Coord wx, wz;
					RotPoint(lx, lz, R.iRot, wx, wz);
					xE.fX = kFFromCoord(R.cx + wx);
					xE.fZ = kFFromCoord(R.cz + wz);
				}
				return xE;
			};

			// Spawn at room 0.
			const RoomId xSpawnRoom = 0;
			xLayout.axGameElements.PushBack(RoomElement(GameElementType::SpawnPoint, xSpawnRoom));

			// Pentagram at deepest room.
			Zenith_Vector<int32_t> aiDistFromSpawn = BfsDistances(xLayout, xSpawnRoom);
			RoomId xPentRoom = 0;
			int32_t iMaxDist = -1;
			for (uint32_t i = 0; i < uN; ++i)
			{
				const int32_t iD = aiDistFromSpawn.Get(i);
				if (iD > iMaxDist) { iMaxDist = iD; xPentRoom = static_cast<RoomId>(i); }
			}
			xLayout.axGameElements.PushBack(RoomElement(GameElementType::Pentagram, xPentRoom));

			// Doors: TWO per corridor (one at each DoorPoint, anchored to
			// the wall of the corresponding endpoint room). The pentagram-
			// side door on a pentagram-incident corridor is the only door
			// LOCKED by default -- everything else opens on F-press alone.
			// This preserves today's one-key economy: one forge-produced
			// Key unlocks one pentagram-corridor traversal (the player
			// picks which).
			//
			// Each DoorPoint already carries its own wall-aligned yaw
			// (DoorPoint.fWallYawRadians, computed in integer math during
			// ProjectDoorPoint_I from R.iRot + edge classification). No
			// per-door FP atan2 needed; the door's broad face will lie
			// along the wall by construction, on every seed regardless of
			// room rotation.
			//
			// `axGatedCorridors` lists ALL corridors that contain >=1
			// locked door (de-duplicated). It's the input to
			// `BfsSkippingGated` below, which finds spawn-side rooms for
			// iron + forge placement.
			Zenith_Vector<int32_t> axGatedCorridors;
			for (uint32_t iC = 0; iC < xLayout.axCorridors.GetSize(); ++iC)
			{
				const Corridor& xC = xLayout.axCorridors.Get(iC);
				const DoorPoint& xDA = xLayout.axDoorPoints.Get(xC.iDoorA);
				const DoorPoint& xDB = xLayout.axDoorPoints.Get(xC.iDoorB);
				const bool bPent = (xDA.xRoomId == xPentRoom || xDB.xRoomId == xPentRoom);
				const DoorPoint* apxDp[2] = { &xDA, &xDB };
				for (uint32_t iDp = 0; iDp < 2; ++iDp)
				{
					const DoorPoint* pxDp = apxDp[iDp];
					GameElement xE;
					xE.eType        = GameElementType::Door;
					xE.fX           = pxDp->fX;
					xE.fZ           = pxDp->fZ;
					// Anchor each door to the ROOM whose wall it cuts. The
					// `iCorridorId` still names the corridor (so the test
					// invariants `doorCount == 2 * corridorCount` + per-
					// corridor symmetry checks stay simple).
					xE.xRoomId      = pxDp->xRoomId;
					xE.iCorridorId  = static_cast<int32_t>(iC);
					xE.fYawRadians  = pxDp->fWallYawRadians;
					// Lock ONLY the pentagram-side door of a pentagram-
					// incident corridor; everything else unlocked. Extra
					// random locks land via the lock-pass below.
					xE.bDoorLocked  = bPent && (pxDp->xRoomId == xPentRoom);
					xLayout.axGameElements.PushBack(xE);
				}
				// Push the corridor onto `axGatedCorridors` ONCE per
				// corridor (not per door) when it carries a pentagram-side
				// lock. The extra-lock pass below may also push corridors
				// here (de-duplicated via linear scan).
				if (bPent) axGatedCorridors.PushBack(static_cast<int32_t>(iC));
			}

			// Extra-lock pass: optionally promote non-pentagram-side doors
			// to locked. Uses a SEPARATE RNG stream derived from uSeed (not
			// consumed from the main rng) so two properties hold:
			//   1. Tuning-stable: changing fDoorLockedFraction does NOT
			//      shift chest / objective / villager / priest placement
			//      (those decisions all run on the main rng AFTER this
			//      function returns).
			//   2. Default-stable: fDoorLockedFraction == 0 -> the inner
			//      branch never fires, so the main rng is untouched and
			//      the downstream RNG stream is bit-identical to the case
			//      where this pass didn't exist.
			//
			// The integer-bucket (0..1023) trick keeps the comparison
			// /fp:fast-stable -- a direct `RngF < fClampedFrac` would mix
			// float math with the seeded integer rng.
			Rng xLockRng = RngInit(uSeed ^ 0xD009B100CCEDD00Aull);
			const float fClampedFrac = glm::clamp(xConfig.fDoorLockedFraction, 0.0f, 1.0f);
			// std::round (explicit) avoids /fp:fast FMA-ordering producing
			// 511 vs 512 on ulp-edge cases.
			const uint32_t uLockNumerator =
				static_cast<uint32_t>(std::round(fClampedFrac * 1024.0f));
			if (uLockNumerator > 0u)
			{
				for (uint32_t i = 0; i < xLayout.axGameElements.GetSize(); ++i)
				{
					GameElement& xE = xLayout.axGameElements.Get(i);
					if (xE.eType != GameElementType::Door) continue;
					if (xE.bDoorLocked) continue;   // already pentagram-locked
					if (RngRangeI(xLockRng, 0, 1023) < static_cast<int32_t>(uLockNumerator))
					{
						xE.bDoorLocked = true;
						const int32_t iC = xE.iCorridorId;
						bool bAlready = false;
						for (uint32_t g = 0; g < axGatedCorridors.GetSize(); ++g)
							if (axGatedCorridors.Get(g) == iC) { bAlready = true; break; }
						if (!bAlready) axGatedCorridors.PushBack(iC);
					}
				}
			}

			// BFS skipping all gated corridors, to find spawn-side rooms
			// for iron / forge placement.
			auto BfsSkippingGated = [&xLayout, &axGatedCorridors](RoomId iStart) -> Zenith_Vector<bool>
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
							if (axGatedCorridors.Get(iG) == static_cast<int32_t>(iC)) { bGated = true; break; }
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
			const Zenith_Vector<bool> abReachNoDoor = BfsSkippingGated(xSpawnRoom);

			Zenith_Vector<RoomId> axSpawnSide;
			Zenith_Vector<RoomId> axAnyNonCritical;
			for (uint32_t i = 0; i < uN; ++i)
			{
				const RoomId xR = static_cast<RoomId>(i);
				if (xR == xSpawnRoom || xR == xPentRoom) continue;
				axAnyNonCritical.PushBack(xR);
				if (abReachNoDoor.Get(i)) axSpawnSide.PushBack(xR);
			}

			auto PickNth = [](const Zenith_Vector<RoomId>& ax, uint32_t uIdx) -> RoomId
			{
				if (ax.GetSize() == 0) return kInvalidRoomId;
				return ax.Get(uIdx % ax.GetSize());
			};

			// Iron + forge placement. One Iron per locked door (the player
			// needs to forge one Key per locked traversal); safety floor
			// of 1 even when zero doors are locked so the forge isn't
			// input-starved. Slot assignments match the historical layout
			// for default tuning: Iron at slot 0, Forge at slot 1 -- so
			// the single-lock seed (today's common case) doesn't shift
			// downstream entity positions unrelated to this PR.
			uint32_t uLockedDoorCount = 0;
			for (uint32_t i = 0; i < xLayout.axGameElements.GetSize(); ++i)
			{
				const GameElement& xE = xLayout.axGameElements.Get(i);
				if (xE.eType == GameElementType::Door && xE.bDoorLocked) ++uLockedDoorCount;
			}
			const uint32_t uIronCount = (uLockedDoorCount > 0u) ? uLockedDoorCount : 1u;

			auto PickRoom = [&](uint32_t uSlot) -> RoomId
			{
				return PickNth((axSpawnSide.GetSize() > uSlot) ? axSpawnSide : axAnyNonCritical, uSlot);
			};

			// 2026-05-26 (Option B): pick the FORGE room as the spawn-
			// side room minimizing post-forge walk to the pentagram.
			// Pre-Option-B the forge was always at axSpawnSide[1] (the
			// 2nd-nearest-to-spawn room), which on layouts where pent
			// is far from spawn (seed 1: spawn->pent straight-line 86m,
			// forge->pent 79m) makes the bootstrap chain too long for
			// the bot to make 3 deliveries inside the villager-life
			// budget. The matrix balance criterion #2 (every seed
			// winnable) failed on seed 1 with 0 wins across 8
			// personalities.
			//
			// Option B picks whichever axSpawnSide room is closest to
			// the pent-room CENTER, breaking ties by lower-room-id for
			// determinism. The Iron stays at axSpawnSide[0] (nearest
			// to spawn) so the spawn -> iron leg is short; the change
			// only affects the iron -> forge -> pent leg.
			//
			// Determinism: the iteration order is the stable
			// axSpawnSide order, ties broken by index. Same seed
			// produces same room id every run.
			//
			// Risk: shifts forge position on most seeds, so the
			// matrix balance re-baselines. Existing tests with
			// forge-position assertions (none currently in Tests/)
			// would need updates.
			RoomId xForgeRoom = kInvalidRoomId;
			{
				const Room& xPentR = xLayout.axRooms.Get(static_cast<uint32_t>(xPentRoom));
				float fBestDist = 1e30f;
				for (uint32_t i = 0; i < axSpawnSide.GetSize(); ++i)
				{
					const RoomId xR = axSpawnSide.Get(i);
					const Room& xRoom = xLayout.axRooms.Get(static_cast<uint32_t>(xR));
					const float fDx = xRoom.fCentreX - xPentR.fCentreX;
					const float fDz = xRoom.fCentreZ - xPentR.fCentreZ;
					const float fDist = std::sqrt(fDx * fDx + fDz * fDz);
					if (fDist < fBestDist)
					{
						fBestDist = fDist;
						xForgeRoom = xR;
					}
				}
				// Fallback: if axSpawnSide is empty (degenerate layout)
				// fall back to the pre-Option-B slot-1 pick from
				// axAnyNonCritical so the forge still spawns.
				if (xForgeRoom == kInvalidRoomId)
				{
					xForgeRoom = PickRoom(1u);
				}
			}

			xLayout.axGameElements.PushBack(RoomElement(GameElementType::Iron,  PickRoom(0u)));
			xLayout.axGameElements.PushBack(RoomElement(GameElementType::Forge, xForgeRoom));
			for (uint32_t i = 1; i < uIronCount; ++i)
			{
				xLayout.axGameElements.PushBack(RoomElement(GameElementType::Iron, PickRoom(1u + i)));
			}

			const RoomId xChestRoom = PickNth(axAnyNonCritical, 2);
			const RoomId xNoiseRoom = PickNth(axAnyNonCritical, 3);
			xLayout.axGameElements.PushBack(RoomElement(GameElementType::Chest,        xChestRoom));
			xLayout.axGameElements.PushBack(RoomElement(GameElementType::NoiseMachine, xNoiseRoom));

			for (int iObj = 0; iObj < 5; ++iObj)
			{
				const RoomId xR = PickNth(axAnyNonCritical, static_cast<uint32_t>(4 + iObj));
				const GameElementType eT = static_cast<GameElementType>(
					static_cast<uint8_t>(GameElementType::Objective1) + iObj);
				xLayout.axGameElements.PushBack(RoomElement(eT, xR));
			}
		}

		// --------------------------------------------------------------------
		// Solvability validation (already integer; unchanged).
		// --------------------------------------------------------------------
		bool ValidateSolvability(const LevelLayout& xLayout)
		{
			RoomId  xSpawn = kInvalidRoomId;
			RoomId  xPent  = kInvalidRoomId;
			Zenith_Vector<int32_t> axGated;
			Zenith_Vector<RoomId>  axObjectiveRooms;
			// 2026-05-25: track ALL iron rooms (was: a single xIron that
			// got overwritten on each Iron element, so only the last iron
			// was validated for reachability). With auto-scaling iron
			// count we need >= uLockedDoorCount iron rooms reachable
			// without crossing a locked door so the player can always
			// craft enough Keys.
			Zenith_Vector<RoomId>  axIronRooms;
			uint32_t uLockedDoorCount = 0;
			RoomId xForge = kInvalidRoomId;
			for (uint32_t i = 0; i < xLayout.axGameElements.GetSize(); ++i)
			{
				const GameElement& xE = xLayout.axGameElements.Get(i);
				switch (xE.eType)
				{
					case GameElementType::SpawnPoint: xSpawn = xE.xRoomId; break;
					case GameElementType::Pentagram:  xPent  = xE.xRoomId; break;
					case GameElementType::Door:
						// Only LOCKED doors gate; unlocked doors are pass-through
						// for the solvability BFS. Duplicates in axGated are fine
						// (BFS does a linear-scan membership test); uLockedDoorCount
						// is the canonical count for the iron-vs-locks comparison.
						if (xE.bDoorLocked)
						{
							++uLockedDoorCount;
							axGated.PushBack(xE.iCorridorId);
						}
						break;
					case GameElementType::Iron:       axIronRooms.PushBack(xE.xRoomId); break;
					case GameElementType::Forge:      xForge = xE.xRoomId; break;
					case GameElementType::Objective1:
					case GameElementType::Objective2:
					case GameElementType::Objective3:
					case GameElementType::Objective4:
					case GameElementType::Objective5: axObjectiveRooms.PushBack(xE.xRoomId); break;
					default: break;
				}
			}
			if (xSpawn == kInvalidRoomId || xPent == kInvalidRoomId) return false;

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
			const Zenith_Vector<bool> abWithDoor = DfsReachable(xLayout, xSpawn, -1);

			// Iron-reachability check scales with the locked-door count: each
			// locked door needs ONE iron pickup the player can reach without
			// already needing a Key. (Previously this checked a single xIron;
			// with multi-iron procgen we now require enough irons to forge
			// enough keys.)
			uint32_t uReachableIron = 0;
			for (uint32_t i = 0; i < axIronRooms.GetSize(); ++i)
			{
				const RoomId xR = axIronRooms.Get(i);
				if (xR != kInvalidRoomId && abNoDoor.Get(static_cast<uint32_t>(xR))) ++uReachableIron;
			}
			if (uReachableIron < uLockedDoorCount) return false;
			if (xForge != kInvalidRoomId && !abNoDoor.Get(static_cast<uint32_t>(xForge))) return false;
			if (!abWithDoor.Get(static_cast<uint32_t>(xPent))) return false;
			for (uint32_t i = 0; i < axObjectiveRooms.GetSize(); ++i)
			{
				const RoomId xR = axObjectiveRooms.Get(i);
				if (xR == kInvalidRoomId) continue;
				if (!abWithDoor.Get(static_cast<uint32_t>(xR))) return false;
			}
			return true;
		}

		// --------------------------------------------------------------------
		// AI placement: villagers (indoor + outdoor) + priest + patrol.
		// Integer throughout.
		// --------------------------------------------------------------------
		RoomId PickPriestRoom(const LevelLayout& xLayout, RoomId xSpawnRoom, RoomId xPentRoom)
		{
			const uint32_t uN = xLayout.axRooms.GetSize();
			if (uN == 0u) return kInvalidRoomId;
			const Zenith_Vector<int32_t> aiDistFromSpawn = BfsDistances(xLayout, xSpawnRoom);
			const Zenith_Vector<int32_t> aiDistFromPent  = BfsDistances(xLayout, xPentRoom);
			// 2026-05-25 v4: count each room's incident corridors. A
			// room with zero corridors would trap the priest forever
			// (no exit doors); reject it here so the priest's spawn
			// is guaranteed to have at least one unlocked door (every
			// non-pent room's incident corridor has an unlocked door
			// on this room's side -- only pent-side doors are locked).
			Zenith_Vector<uint32_t> auCorridorCount;
			for (uint32_t i = 0; i < uN; ++i) auCorridorCount.PushBack(0u);
			for (uint32_t iC = 0; iC < xLayout.axCorridors.GetSize(); ++iC)
			{
				const Corridor& xC = xLayout.axCorridors.Get(iC);
				const RoomId xA = xLayout.axDoorPoints.Get(xC.iDoorA).xRoomId;
				const RoomId xB = xLayout.axDoorPoints.Get(xC.iDoorB).xRoomId;
				if (xA != kInvalidRoomId) auCorridorCount.Get(static_cast<uint32_t>(xA))++;
				if (xB != kInvalidRoomId) auCorridorCount.Get(static_cast<uint32_t>(xB))++;
			}
			RoomId xBest  = kInvalidRoomId;
			int32_t iBest = -1;
			for (uint32_t i = 0; i < uN; ++i)
			{
				const RoomId xR = static_cast<RoomId>(i);
				if (xR == xSpawnRoom || xR == xPentRoom) continue;
				if (auCorridorCount.Get(i) == 0u) continue;  // no exit
				const int32_t dS = aiDistFromSpawn.Get(i);
				const int32_t dP = aiDistFromPent.Get(i);
				if (dS < 0 || dP < 0) continue;
				const int32_t iScore = (dS < dP) ? dS : dP;
				if (iScore > iBest) { iBest = iScore; xBest = xR; }
			}
			if (xBest == kInvalidRoomId)
			{
				// Fallback: any non-spawn, non-pent room with at least
				// one corridor. If even that's empty, give up (the
				// layout is so degenerate the priest can't sensibly
				// spawn anywhere -- ValidateSolvability would have
				// already rejected the seed).
				for (uint32_t i = 0; i < uN; ++i)
				{
					const RoomId xR = static_cast<RoomId>(i);
					if (xR != xSpawnRoom && xR != xPentRoom && auCorridorCount.Get(i) > 0u)
					{
						xBest = xR;
						break;
					}
				}
			}
			return xBest;
		}

		void BuildPatrolCycle(LevelLayout& xLayout, RoomId xPriestRoom,
		                      RoomId xPentRoom, uint32_t uMaxNodes)
		{
			xLayout.axPatrolNodes.Clear();
			if (xPriestRoom == kInvalidRoomId) return;
			const Zenith_Vector<int32_t> aiDist = BfsDistances(xLayout, xPriestRoom);
			Zenith_Vector<RoomId>  axReach;
			Zenith_Vector<int32_t> aiReachDist;
			for (uint32_t i = 0; i < xLayout.axRooms.GetSize(); ++i)
			{
				if (aiDist.Get(i) < 0) continue;
				if (static_cast<RoomId>(i) == xPentRoom) continue;
				axReach.PushBack(static_cast<RoomId>(i));
				aiReachDist.PushBack(aiDist.Get(i));
			}
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

		void DistributeVillagersI(const Zenith_Vector<RoomI>& axRoomsI,
		                          LevelLayout& xLayout, Rng& rng,
		                          uint32_t uTotal,
		                          const Zenith_Vector<RoomId>& axSkip)
		{
			const uint32_t uN = axRoomsI.GetSize();
			if (uN == 0u || uTotal == 0u) return;

			Zenith_Vector<RoomId>  axAllowed;
			Zenith_Vector<int64_t> ai64Area;
			int64_t totalArea = 0;
			for (uint32_t i = 0; i < uN; ++i)
			{
				bool bSkip = false;
				for (uint32_t s = 0; s < axSkip.GetSize(); ++s)
					if (axSkip.Get(s) == static_cast<RoomId>(i)) { bSkip = true; break; }
				if (bSkip) continue;
				const RoomI& R = axRoomsI.Get(i);
				const int64_t a = static_cast<int64_t>(R.hx) * static_cast<int64_t>(R.hz) * 4;
				axAllowed.PushBack(static_cast<RoomId>(i));
				ai64Area.PushBack(a);
				totalArea += a;
			}
			if (axAllowed.GetSize() == 0u) return;

			Zenith_Vector<uint32_t> auPerRoom;
			uint32_t uAssigned = 0u;
			for (uint32_t i = 0; i < axAllowed.GetSize(); ++i) { auPerRoom.PushBack(1u); ++uAssigned; }
			if (uAssigned > uTotal)
			{
				while (uAssigned > uTotal)
				{
					int iSmallest = -1;
					int64_t iMin = 0x7FFFFFFFFFFFFFFFll;
					for (uint32_t i = 0; i < axAllowed.GetSize(); ++i)
					{
						if (auPerRoom.Get(i) == 0u) continue;
						if (ai64Area.Get(i) < iMin) { iMin = ai64Area.Get(i); iSmallest = static_cast<int>(i); }
					}
					if (iSmallest < 0) break;
					auPerRoom.Get(static_cast<uint32_t>(iSmallest)) = 0u;
					--uAssigned;
				}
			}
			while (uAssigned < uTotal)
			{
				int iLargest = 0;
				int64_t iMax = -1;
				for (uint32_t i = 0; i < axAllowed.GetSize(); ++i)
				{
					const int64_t score = ai64Area.Get(i) / static_cast<int64_t>(auPerRoom.Get(i) + 1u);
					if (score > iMax) { iMax = score; iLargest = static_cast<int>(i); }
				}
				auPerRoom.Get(static_cast<uint32_t>(iLargest))++;
				++uAssigned;
			}

			for (uint32_t iR = 0; iR < axAllowed.GetSize(); ++iR)
			{
				const RoomId xR = axAllowed.Get(iR);
				const uint32_t uCount = auPerRoom.Get(iR);
				if (uCount == 0u) continue;
				const RoomI& R = axRoomsI.Get(static_cast<uint32_t>(xR));
				const Coord innerHX = (R.hx * 3) / 5;   // 0.6
				const Coord innerHZ = (R.hz * 3) / 5;
				for (uint32_t v = 0; v < uCount; ++v)
				{
					const Coord lx = static_cast<Coord>(RngRangeI(rng, -innerHX, innerHX));
					const Coord lz = static_cast<Coord>(RngRangeI(rng, -innerHZ, innerHZ));
					Coord wx, wz;
					RotPoint(lx, lz, R.iRot, wx, wz);
					VillagerSpawn xS;
					xS.fX = kFFromCoord(R.cx + wx);
					xS.fZ = kFFromCoord(R.cz + wz);
					// Yaw: a random discrete rotation, converted to radians
					// for the public struct.
					xS.fYawRadians = YawFromRot(RngRangeI(rng, 0, kROT_COUNT - 1));
					xS.xRoomId = xR;
					xLayout.axVillagerSpawns.PushBack(xS);
				}
			}
		}

		void PlaceAI_I(const Zenith_Vector<RoomI>& axRoomsI,
		               LevelLayout& xLayout, Rng& rng, const GenConfig& xConfig)
		{
			xLayout.axVillagerSpawns.Clear();
			xLayout.axPatrolNodes.Clear();
			xLayout.xPriestSpawn = PriestSpawn();

			RoomId xSpawn = kInvalidRoomId;
			RoomId xPent  = kInvalidRoomId;
			for (uint32_t i = 0; i < xLayout.axGameElements.GetSize(); ++i)
			{
				const GameElement& xE = xLayout.axGameElements.Get(i);
				if (xE.eType == GameElementType::SpawnPoint) xSpawn = xE.xRoomId;
				if (xE.eType == GameElementType::Pentagram)  xPent  = xE.xRoomId;
			}
			if (xSpawn == kInvalidRoomId || xPent == kInvalidRoomId) return;

			const RoomId xPriestRoom = PickPriestRoom(xLayout, xSpawn, xPent);
			if (xPriestRoom != kInvalidRoomId)
			{
				const Room& xR = xLayout.axRooms.Get(static_cast<uint32_t>(xPriestRoom));
				// 2026-05-25 v4: shift the priest spawn from the room
				// center TOWARD one of the room's UNLOCKED doors. The
				// priest's runtime OpenNearbyDoorsFor uses a 2 m radius
				// from the priest position -- spawning at the room
				// center leaves the priest >2 m from every door on
				// procgen rooms larger than a 4x4 m closet, so the
				// priest's BT patrol cycles within its tiny initial
				// navmesh region (the spawn room before any door is
				// opened) for the entire run. Telemetry-confirmed on
				// seed 5: 27 unique priest positions over 200 s; the
				// priest never moved more than 5 m from spawn.
				//
				// Walk the priest's room's incident corridors, take the
				// first one whose door on THIS side is UNLOCKED, and
				// place the priest at a point 60% of the way from room
				// center to that DoorPoint. 60% keeps the priest a meter
				// or so off the doorway so its capsule doesn't spawn
				// embedded in the closed-door collider, while still
				// ensuring OpenNearbyDoorsFor's 2 m radius catches the
				// door from frame 1. Determinism preserved -- the
				// corridor iteration order is the layout's stable
				// axCorridors traversal.
				float fSpawnX = xR.fCentreX;
				float fSpawnZ = xR.fCentreZ;
				const uint32_t uCN = xLayout.axCorridors.GetSize();
				for (uint32_t iC = 0; iC < uCN; ++iC)
				{
					const Corridor& xC = xLayout.axCorridors.Get(iC);
					const DoorPoint& xDA = xLayout.axDoorPoints.Get(xC.iDoorA);
					const DoorPoint& xDB = xLayout.axDoorPoints.Get(xC.iDoorB);
					const DoorPoint* pxDp = nullptr;
					if      (xDA.xRoomId == xPriestRoom) pxDp = &xDA;
					else if (xDB.xRoomId == xPriestRoom) pxDp = &xDB;
					if (pxDp == nullptr) continue;
					// Find the door GameElement at this DoorPoint to check
					// its lock state. The bDoorLocked field is true only
					// on pent-side doors; for the priest's (non-pent)
					// room, all its doors are unlocked.
					bool bUnlocked = true;
					for (uint32_t iE = 0; iE < xLayout.axGameElements.GetSize(); ++iE)
					{
						const GameElement& xE = xLayout.axGameElements.Get(iE);
						if (xE.eType != GameElementType::Door) continue;
						const float fDx = xE.fX - pxDp->fX;
						const float fDz = xE.fZ - pxDp->fZ;
						if (fDx * fDx + fDz * fDz < 0.01f)  // same point (mm precision)
						{
							bUnlocked = !xE.bDoorLocked;
							break;
						}
					}
					if (!bUnlocked) continue;
					// 2026-05-26 v2: fixed 2.5 m inset from the doorpoint
					// along the line back to the room center. Previously
					// 1.5 m, but on seeds where the door's swing
					// direction happens to point toward the priest's
					// spawn (telemetry-confirmed on seed 55555), the
					// door's 2 m-wide arm sweeps through the priest's
					// capsule during the open animation. Jolt resolves
					// the overlap by pushing the priest UP -- the priest
					// ends up stranded at y ~ 2.5 m, off the navmesh,
					// permanently stuck (priest BT keeps Investigate
					// intent locked on the door logical centre, can't
					// reach it because the agent is floating).
					// 2.5 m sits outside the door's 2 m sweep radius
					// from the corner pivot but still inside the
					// AI's 4 m OpenNearbyDoorsFor radius, so the
					// door still opens from frame 1 -- just not while
					// sweeping the priest's capsule.
					const float fDx = pxDp->fX - xR.fCentreX;
					const float fDz = pxDp->fZ - xR.fCentreZ;
					const float fLen = std::sqrt(fDx * fDx + fDz * fDz);
					if (fLen > 0.01f)
					{
						const float fInset = 2.5f;
						// If the doorpoint is so close to the room
						// center that 2.5 m inset would overshoot
						// (room is small), clamp at room center.
						const float fT = (fLen > fInset)
							? (fLen - fInset) / fLen
							: 0.0f;
						fSpawnX = xR.fCentreX + fT * fDx;
						fSpawnZ = xR.fCentreZ + fT * fDz;
					}
					else
					{
						// Degenerate: doorpoint == center. Use center.
						fSpawnX = xR.fCentreX;
						fSpawnZ = xR.fCentreZ;
					}
					break;
				}
				xLayout.xPriestSpawn.fX           = fSpawnX;
				xLayout.xPriestSpawn.fZ           = fSpawnZ;
				xLayout.xPriestSpawn.fYawRadians  = xR.fYawRadians;
				xLayout.xPriestSpawn.xRoomId      = xPriestRoom;
				xLayout.xPriestSpawn.bValid       = true;
			}

			BuildPatrolCycle(xLayout, xPriestRoom, xPent, xConfig.uPatrolNodeCount);

			Zenith_Vector<RoomId> axSkip;
			axSkip.PushBack(xPent);
			if (xPriestRoom != kInvalidRoomId) axSkip.PushBack(xPriestRoom);
			const uint32_t uOutdoor = (xConfig.uOutdoorVillagerCount < xConfig.uVillagerCount)
				? xConfig.uOutdoorVillagerCount : xConfig.uVillagerCount;
			const uint32_t uIndoor = xConfig.uVillagerCount - uOutdoor;
			DistributeVillagersI(axRoomsI, xLayout, rng, uIndoor, axSkip);

			const Coord boundsMinX = kCoordFromF(xLayout.fBoundsMinX);
			const Coord boundsMinZ = kCoordFromF(xLayout.fBoundsMinZ);
			const Coord boundsMaxX = kCoordFromF(xLayout.fBoundsMaxX);
			const Coord boundsMaxZ = kCoordFromF(xLayout.fBoundsMaxZ);
			const Coord outdoorMarginMM = kCoordFromF(xConfig.fOutdoorMargin);
			for (uint32_t v = 0; v < uOutdoor; ++v)
			{
				Coord cx, cz;
				const bool bOk = SampleOutdoorPointI(axRoomsI,
					boundsMinX, boundsMaxX, boundsMinZ, boundsMaxZ,
					rng, outdoorMarginMM, 64u, cx, cz);
				VillagerSpawn xS;
				if (bOk) { xS.fX = kFFromCoord(cx); xS.fZ = kFFromCoord(cz); }
				else
				{
					xS.fX = 0.5f * (xLayout.fBoundsMinX + xLayout.fBoundsMaxX);
					xS.fZ = 0.5f * (xLayout.fBoundsMinZ + xLayout.fBoundsMaxZ);
				}
				xS.fYawRadians = YawFromRot(RngRangeI(rng, 0, kROT_COUNT - 1));
				xS.xRoomId = kInvalidRoomId;
				xLayout.axVillagerSpawns.PushBack(xS);
			}
		}

		void OutdoorRelocateObjectivesI(const Zenith_Vector<RoomI>& axRoomsI,
		                                LevelLayout& xLayout, Rng& rng,
		                                uint32_t uCount, Coord marginMM)
		{
			if (uCount == 0u) return;
			const Coord boundsMinX = kCoordFromF(xLayout.fBoundsMinX);
			const Coord boundsMinZ = kCoordFromF(xLayout.fBoundsMinZ);
			const Coord boundsMaxX = kCoordFromF(xLayout.fBoundsMaxX);
			const Coord boundsMaxZ = kCoordFromF(xLayout.fBoundsMaxZ);
			uint32_t uMoved = 0;
			for (int i = static_cast<int>(xLayout.axGameElements.GetSize()) - 1;
			     i >= 0 && uMoved < uCount; --i)
			{
				GameElement& xE = xLayout.axGameElements.Get(static_cast<uint32_t>(i));
				const uint8_t u = static_cast<uint8_t>(xE.eType);
				if (u < static_cast<uint8_t>(GameElementType::Objective1) ||
				    u > static_cast<uint8_t>(GameElementType::Objective5)) continue;
				Coord cx, cz;
				if (!SampleOutdoorPointI(axRoomsI,
					boundsMinX, boundsMaxX, boundsMinZ, boundsMaxZ,
					rng, marginMM, 64u, cx, cz)) continue;
				xE.fX = kFFromCoord(cx);
				xE.fZ = kFFromCoord(cz);
				xE.xRoomId = kInvalidRoomId;
				++uMoved;
			}
		}
	}

	// ============================================================================
	// Public entry point. The pipeline is identical in shape to the original
	// (BSP -> Rooms -> Doors+Corridors -> Walls -> GameElements -> AI), but every
	// step now runs on integer coordinates. The public LevelLayout still uses
	// float; we convert at the end of each step.
	// ============================================================================
	bool Generate(uint64_t uSeed, const GenConfig& xConfig, LevelLayout& xOut)
	{
		xOut = LevelLayout();
		xOut.uSeed       = uSeed;
		xOut.fBoundsMinX = xConfig.fBoundsMinX;
		xOut.fBoundsMinZ = xConfig.fBoundsMinZ;
		xOut.fBoundsMaxX = xConfig.fBoundsMaxX;
		xOut.fBoundsMaxZ = xConfig.fBoundsMaxZ;

		const Coord boundsMinX  = kCoordFromF(xConfig.fBoundsMinX);
		const Coord boundsMinZ  = kCoordFromF(xConfig.fBoundsMinZ);
		const Coord boundsMaxX  = kCoordFromF(xConfig.fBoundsMaxX);
		const Coord boundsMaxZ  = kCoordFromF(xConfig.fBoundsMaxZ);
		const Coord minRoomSize = kCoordFromF(xConfig.fMinRoomSize);
		const Coord maxRoomSize = kCoordFromF(xConfig.fMaxRoomSize);
		const Coord halfThick   = kCoordFromF(xConfig.fWallHalfThickness);
		const Coord doorHalf    = kCoordFromF(xConfig.fDoorGapHalfWidth);
		const Coord outdoorMargin = kCoordFromF(xConfig.fOutdoorMargin);

		const Coord W = boundsMaxX - boundsMinX;
		const Coord H = boundsMaxZ - boundsMinZ;
		if (W < 2 * minRoomSize || H < 2 * minRoomSize)
		{
			Zenith_Warning(LOG_CATEGORY_CORE,
				"DPProcLevel::Generate: bounds %d x %d mm too small for fMinRoomSize=%d mm",
				W, H, minRoomSize);
			return false;
		}

		Rng rng = RngInit(uSeed);

		// 1. BSP partition.
		PartitionI root{ boundsMinX, boundsMinZ, boundsMaxX, boundsMaxZ };
		Zenith_Vector<PartitionI> axLeaves;
		BspRecurse_I(rng, root, minRoomSize, xConfig.uBspDepth, axLeaves);

		// 2. Convert to rooms with yaw retries. Aspect drawn first per
		//    room (independent of yaw retry loop) so same seed = same shapes.
		Zenith_Vector<RoomI> axRoomsI;
		for (uint32_t i = 0; i < axLeaves.GetSize(); ++i)
		{
			const AspectEntry& aspect = PickAspect_I(rng, xConfig.fAspectMin, xConfig.fAspectMax);
			RoomI R = PartitionToRoom_I(axLeaves.Get(i), static_cast<RoomId>(i), maxRoomSize, aspect);

			bool bAccepted = false;
			for (uint32_t uAttempt = 0; uAttempt < xConfig.uMaxYawRetries; ++uAttempt)
			{
				R.iRot = RngRangeI(rng, 0, kROT_COUNT - 1);
				if (!IsRotatedRoomInBoundsI(R, boundsMinX, boundsMinZ, boundsMaxX, boundsMaxZ)) continue;
				bool bOverlap = false;
				for (uint32_t j = 0; j < axRoomsI.GetSize(); ++j)
				{
					if (OBBsOverlap_I(R, axRoomsI.Get(j))) { bOverlap = true; break; }
				}
				if (!bOverlap) { bAccepted = true; break; }
			}
			if (!bAccepted) R.iRot = 0;  // fallback to identity rotation
			axRoomsI.PushBack(R);

			Room F;
			ToFloat_Room(R, F);
			xOut.axRooms.PushBack(F);
		}

		// 3. Corridors between BSP-adjacent leaves; door points carry the
		//    EdgeSide tag.
		Zenith_Vector<DoorI> axDoorsI;
		for (uint32_t i = 0; i < axLeaves.GetSize(); ++i)
		{
			for (uint32_t j = i + 1; j < axLeaves.GetSize(); ++j)
			{
				const PartitionI& A = axLeaves.Get(i);
				const PartitionI& B = axLeaves.Get(j);
				if (!PartitionsShareEdgeI(A, B)) continue;

				// Shared edge midpoint, integer.
				Coord midX, midZ;
				if (A.maxX == B.minX || B.maxX == A.minX)
				{
					midX = (A.maxX == B.minX) ? A.maxX : B.maxX;
					const Coord zLo = (A.minZ > B.minZ) ? A.minZ : B.minZ;
					const Coord zHi = (A.maxZ < B.maxZ) ? A.maxZ : B.maxZ;
					midZ = (zLo + zHi) / 2;
				}
				else
				{
					midZ = (A.maxZ == B.minZ) ? A.maxZ : B.maxZ;
					const Coord xLo = (A.minX > B.minX) ? A.minX : B.minX;
					const Coord xHi = (A.maxX < B.maxX) ? A.maxX : B.maxX;
					midX = (xLo + xHi) / 2;
				}

				const DoorI dA = ProjectDoorPoint_I(axRoomsI.Get(i), midX, midZ, doorHalf, halfThick);
				const DoorI dB = ProjectDoorPoint_I(axRoomsI.Get(j), midX, midZ, doorHalf, halfThick);
				const int32_t iA = static_cast<int32_t>(axDoorsI.GetSize());
				axDoorsI.PushBack(dA);
				const int32_t iB = static_cast<int32_t>(axDoorsI.GetSize());
				axDoorsI.PushBack(dB);

				DoorPoint fA, fB;
				ToFloat_Door(dA, fA);
				ToFloat_Door(dB, fB);
				xOut.axDoorPoints.PushBack(fA);
				xOut.axDoorPoints.PushBack(fB);

				Corridor C;
				C.iDoorA = iA;
				C.iDoorB = iB;
				xOut.axCorridors.PushBack(C);
			}
		}

		// 4. Walls -- read door EdgeSide tags directly, no re-classification.
		Zenith_Vector<WallSegmentI> axWallsI;
		EmitWallSegments_I(axRoomsI, axDoorsI, halfThick, doorHalf, axWallsI);
		for (uint32_t i = 0; i < axWallsI.GetSize(); ++i)
		{
			WallSegment F;
			ToFloat_Wall(axWallsI.Get(i), F);
			xOut.axWallSegments.PushBack(F);
		}

		// 5. Game elements.
		PlaceGameElements_I(axRoomsI, xOut, xConfig, uSeed);

		// 5b. Relocate some objectives outdoor.
		OutdoorRelocateObjectivesI(axRoomsI, xOut, rng,
			xConfig.uOutdoorObjectiveCount, outdoorMargin);

		if (!ValidateSolvability(xOut))
		{
			Zenith_Warning(LOG_CATEGORY_CORE,
				"DPProcLevel::Generate: seed %llu produced an unsolvable layout",
				static_cast<unsigned long long>(uSeed));
			// Same behaviour as the original: don't fail Generate, let the
			// caller decide what to do with an unsolvable seed.
		}

		// 6. AI placement.
		PlaceAI_I(axRoomsI, xOut, rng, xConfig);

		return true;
	}
}
