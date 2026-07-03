#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/TAA/Flux_TAAJitter.h"

#include <cstring>   // memcmp

// ============================================================================
// TAA jitter + velocity-encode unit tests (Part 1 pure cores). No GPU, no
// renderer boot. Hosted in an already-linked TU (Flux_MaterialTable.cpp) so the
// static-init registrations survive dead-strip until the TAA path is wired.
//
// The NDC-shift test PINS the projection-jitter element indices/signs (the plan's
// R "do not trust the indices" guard); the zero-jitter memcmp test underwrites the
// TAA-off byte-identical capture gate.
// ============================================================================

// ---- Halton first-8 values pinned exactly ----------------------------------

ZENITH_TEST(TAAJitter, HaltonBase2First8)
{
	// index 1..8 -> 1/2, 1/4, 3/4, 1/8, 5/8, 3/8, 7/8, 1/16
	const float afExpected[8] = { 0.5f, 0.25f, 0.75f, 0.125f, 0.625f, 0.375f, 0.875f, 0.0625f };
	for (u_int u = 0; u < 8u; ++u)
	{
		ZENITH_ASSERT_EQ_FLOAT(Flux_Halton(2u, u + 1u), afExpected[u], 1e-6f,
			"Halton(2,%u)", u + 1u);
	}
	ZENITH_ASSERT_EQ_FLOAT(Flux_Halton(2u, 0u), 0.0f, 1e-6f, "Halton(2,0) is the degenerate 0");
}

ZENITH_TEST(TAAJitter, HaltonBase3First8)
{
	// index 1..8 -> 1/3, 2/3, 1/9, 4/9, 7/9, 2/9, 5/9, 8/9
	const float afExpected[8] = {
		1.0f/3.0f, 2.0f/3.0f, 1.0f/9.0f, 4.0f/9.0f, 7.0f/9.0f, 2.0f/9.0f, 5.0f/9.0f, 8.0f/9.0f };
	for (u_int u = 0; u < 8u; ++u)
	{
		ZENITH_ASSERT_EQ_FLOAT(Flux_Halton(3u, u + 1u), afExpected[u], 1e-6f,
			"Halton(3,%u)", u + 1u);
	}
}

// ---- jitter offset: centred half-pixel range + analytic phase 0 + wrap ------

ZENITH_TEST(TAAJitter, OffsetCentredHalfPixelRange)
{
	for (u_int u = 0; u < 16u; ++u)
	{
		const Zenith_Maths::Vector2 xOff = Flux_TAAJitterOffsetPixels(u, 16u);
		ZENITH_ASSERT_GE(xOff.x, -0.5f, "jitter.x below -0.5 at phase %u", u);
		ZENITH_ASSERT_LT(xOff.x,  0.5f, "jitter.x at/above +0.5 at phase %u", u);
		ZENITH_ASSERT_GE(xOff.y, -0.5f, "jitter.y below -0.5 at phase %u", u);
		ZENITH_ASSERT_LT(xOff.y,  0.5f, "jitter.y at/above +0.5 at phase %u", u);
	}
}

ZENITH_TEST(TAAJitter, OffsetPhaseZeroIsFirstHaltonSample)
{
	const Zenith_Maths::Vector2 xOff = Flux_TAAJitterOffsetPixels(0u, 8u);
	ZENITH_ASSERT_EQ_FLOAT(xOff.x, Flux_Halton(2u, 1u) - 0.5f, 1e-6f, "phase-0 x == Halton(2,1)-0.5");
	ZENITH_ASSERT_EQ_FLOAT(xOff.y, Flux_Halton(3u, 1u) - 0.5f, 1e-6f, "phase-0 y == Halton(3,1)-0.5");
}

ZENITH_TEST(TAAJitter, OffsetWrapsAtPeriod)
{
	for (u_int u = 0; u < 8u; ++u)
	{
		const Zenith_Maths::Vector2 xA = Flux_TAAJitterOffsetPixels(u,      8u);
		const Zenith_Maths::Vector2 xB = Flux_TAAJitterOffsetPixels(u + 8u, 8u);
		ZENITH_ASSERT_EQ_FLOAT(xA.x, xB.x, 1e-6f, "wrap mismatch x at phase %u", u);
		ZENITH_ASSERT_EQ_FLOAT(xA.y, xB.y, 1e-6f, "wrap mismatch y at phase %u", u);
	}
}

// ---- projection jitter: analytic NDC shift + zero-jitter identity ----------

ZENITH_TEST(TAAJitter, ProjectionJitterShiftsNDCExactly)
{
	const Zenith_Maths::Matrix4 xProj = Zenith_Maths::PerspectiveProjection(60.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
	const u_int uW = 1920u, uH = 1080u;
	const Zenith_Maths::Vector2 xJit(0.37f, -0.21f);   // arbitrary sub-pixel offset
	const Zenith_Maths::Vector4 xView(2.0f, -1.5f, 5.0f, 1.0f);   // view-space point, z>0 (in front)

	const Zenith_Maths::Vector4 xClipBefore = xProj * xView;
	const Zenith_Maths::Vector2 xNdcBefore(xClipBefore.x / xClipBefore.w, xClipBefore.y / xClipBefore.w);

	const Zenith_Maths::Matrix4 xJittered = Flux_ApplyJitterToProjection(xProj, xJit, uW, uH);
	const Zenith_Maths::Vector4 xClipAfter = xJittered * xView;
	const Zenith_Maths::Vector2 xNdcAfter(xClipAfter.x / xClipAfter.w, xClipAfter.y / xClipAfter.w);

	// The projected point must move by exactly 2*jitterPixels/dims in NDC, independent of depth.
	ZENITH_ASSERT_EQ_FLOAT(xNdcAfter.x - xNdcBefore.x, 2.0f * xJit.x / static_cast<float>(uW), 1e-5f, "ndc.x shift");
	ZENITH_ASSERT_EQ_FLOAT(xNdcAfter.y - xNdcBefore.y, 2.0f * xJit.y / static_cast<float>(uH), 1e-5f, "ndc.y shift");
	// clip.w (depth) must be untouched.
	ZENITH_ASSERT_EQ_FLOAT(xClipAfter.w, xClipBefore.w, 1e-6f, "jitter must not touch clip.w");
}

ZENITH_TEST(TAAJitter, ZeroJitterIsByteIdenticalProjection)
{
	const Zenith_Maths::Matrix4 xProj = Zenith_Maths::PerspectiveProjection(50.0f, 4.0f / 3.0f, 0.05f, 500.0f);
	const Zenith_Maths::Matrix4 xOut  = Flux_ApplyJitterToProjection(xProj, Zenith_Maths::Vector2(0.0f, 0.0f), 1280u, 720u);
	ZENITH_ASSERT_EQ(memcmp(&xProj, &xOut, sizeof(Zenith_Maths::Matrix4)), 0,
		"zero jitter must return the projection byte-for-byte (underwrites the TAA-off byte-identical gate)");
}

// ---- velocity encode + fp16 round-trip -------------------------------------

ZENITH_TEST(TAAVelocity, EncodeStaticPixelIsExactlyZero)
{
	const Zenith_Maths::Vector2 xUV(0.42f, 0.63f);
	const Zenith_Maths::Vector2 xVel = Flux_EncodeVelocityUV(xUV, xUV);   // no motion
	ZENITH_ASSERT_EQ_FLOAT(xVel.x, 0.0f, 0.0f, "static pixel velocity.x exactly zero");
	ZENITH_ASSERT_EQ_FLOAT(xVel.y, 0.0f, 0.0f, "static pixel velocity.y exactly zero");
}

ZENITH_TEST(TAAVelocity, EncodeIsCurrentMinusPrev)
{
	const Zenith_Maths::Vector2 xCur(0.60f, 0.20f);
	const Zenith_Maths::Vector2 xPrev(0.50f, 0.35f);
	const Zenith_Maths::Vector2 xVel = Flux_EncodeVelocityUV(xCur, xPrev);
	ZENITH_ASSERT_EQ_FLOAT(xVel.x,  0.10f, 1e-6f, "velocity.x = cur.x - prev.x");
	ZENITH_ASSERT_EQ_FLOAT(xVel.y, -0.15f, 1e-6f, "velocity.y = cur.y - prev.y");
	// history lookup reproduces the previous UV.
	const Zenith_Maths::Vector2 xHist = xCur - xVel;
	ZENITH_ASSERT_EQ_FLOAT(xHist.x, xPrev.x, 1e-6f, "uv - velocity == uvPrev (x)");
	ZENITH_ASSERT_EQ_FLOAT(xHist.y, xPrev.y, 1e-6f, "uv - velocity == uvPrev (y)");
}

ZENITH_TEST(TAAVelocity, Fp16RoundTripExactZero)
{
	const Zenith_Maths::Vector2 xRT = Flux_VelocityFp16RoundTrip(Zenith_Maths::Vector2(0.0f, 0.0f));
	ZENITH_ASSERT_EQ_FLOAT(xRT.x, 0.0f, 0.0f, "fp16 round-trip of 0 is exactly 0 (x)");
	ZENITH_ASSERT_EQ_FLOAT(xRT.y, 0.0f, 0.0f, "fp16 round-trip of 0 is exactly 0 (y)");
}

ZENITH_TEST(TAAVelocity, Fp16RoundTripSubQuarterPixelAt4K)
{
	// Realistic per-frame velocities (|v| <= 0.1 UV ~= 384px at 4K) survive RG16F to < 0.25px.
	const float fW4K = 3840.0f, fH4K = 2160.0f;
	const float afMag[6] = { 0.001f, 0.01f, 0.05f, 0.1f, -0.08f, 0.02f };
	for (u_int u = 0; u < 6u; ++u)
	{
		const Zenith_Maths::Vector2 xVel(afMag[u], afMag[u] * 0.5f);
		const Zenith_Maths::Vector2 xRT = Flux_VelocityFp16RoundTrip(xVel);
		ZENITH_ASSERT_LT(fabsf(xRT.x - xVel.x) * fW4K, 0.25f, "fp16 velocity.x error < 0.25px at 4K (|v|=%f)", afMag[u]);
		ZENITH_ASSERT_LT(fabsf(xRT.y - xVel.y) * fH4K, 0.25f, "fp16 velocity.y error < 0.25px at 4K (|v|=%f)", afMag[u]);
	}
}
