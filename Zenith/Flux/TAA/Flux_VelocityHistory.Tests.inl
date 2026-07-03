#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/TAA/Flux_VelocityHistory.h"

#include <cstring>   // memcmp

// ============================================================================
// Flux_PrevTransformCache unit tests (Part 1 pure core). Double-buffered per-entity
// prev-model-matrix store driving static/foliage motion vectors. No GPU / renderer.
// ============================================================================

namespace
{
	Zenith_Maths::Matrix4 PTC_Translate(float fX, float fY, float fZ)
	{
		Zenith_Maths::Matrix4 xM(1.0f);
		xM[3] = Zenith_Maths::Vector4(fX, fY, fZ, 1.0f);
		return xM;
	}
	bool PTC_MatEq(const Zenith_Maths::Matrix4& xA, const Zenith_Maths::Matrix4& xB)
	{
		return memcmp(&xA, &xB, sizeof(Zenith_Maths::Matrix4)) == 0;
	}
}

ZENITH_TEST(PrevTransformCache, FreshCacheMisses)
{
	Flux_PrevTransformCache xCache;
	Zenith_Maths::Matrix4 xOut(1.0f);
	ZENITH_ASSERT_FALSE(xCache.TryGetPrev(42u, xOut), "fresh cache has no history");
	ZENITH_ASSERT_EQ(xCache.GetPrevCount(), 0u, "no prev entries");
}

ZENITH_TEST(PrevTransformCache, RecordThenNextFrameYieldsPrev)
{
	Flux_PrevTransformCache xCache;
	const Zenith_Maths::Matrix4 xM = PTC_Translate(1.0f, 2.0f, 3.0f);

	xCache.RecordCurrent(7u, xM);            // frame 0: record
	// Same frame: not yet visible as prev (it is THIS frame's current).
	Zenith_Maths::Matrix4 xOut(1.0f);
	ZENITH_ASSERT_FALSE(xCache.TryGetPrev(7u, xOut), "current-frame record is not prev yet");

	xCache.BeginFrame();                     // frame 1: last frame's current becomes prev
	ZENITH_ASSERT_TRUE(xCache.TryGetPrev(7u, xOut), "prev present after BeginFrame");
	ZENITH_ASSERT_TRUE(PTC_MatEq(xOut, xM), "prev matrix equals what was recorded");
}

ZENITH_TEST(PrevTransformCache, PrevTracksMostRecentPriorFrame)
{
	Flux_PrevTransformCache xCache;
	const Zenith_Maths::Matrix4 xM1 = PTC_Translate(1.0f, 0.0f, 0.0f);
	const Zenith_Maths::Matrix4 xM2 = PTC_Translate(2.0f, 0.0f, 0.0f);

	xCache.RecordCurrent(9u, xM1);
	xCache.BeginFrame();
	xCache.RecordCurrent(9u, xM2);           // moved this frame
	xCache.BeginFrame();

	Zenith_Maths::Matrix4 xOut(1.0f);
	ZENITH_ASSERT_TRUE(xCache.TryGetPrev(9u, xOut), "prev present");
	ZENITH_ASSERT_TRUE(PTC_MatEq(xOut, xM2), "prev is the most recent prior frame (M2), not M1");
}

ZENITH_TEST(PrevTransformCache, EntityIdZeroIsIgnored)
{
	Flux_PrevTransformCache xCache;
	xCache.RecordCurrent(0u, PTC_Translate(5.0f, 5.0f, 5.0f));
	ZENITH_ASSERT_EQ(xCache.GetCurrentCount(), 0u, "id-0 record is dropped");
	xCache.BeginFrame();
	Zenith_Maths::Matrix4 xOut(1.0f);
	ZENITH_ASSERT_FALSE(xCache.TryGetPrev(0u, xOut), "id-0 never has prev");
}

ZENITH_TEST(PrevTransformCache, UnrecordedEntityIsEvicted)
{
	Flux_PrevTransformCache xCache;
	xCache.RecordCurrent(3u, PTC_Translate(1.0f, 1.0f, 1.0f));
	xCache.BeginFrame();                      // 3 is now prev
	Zenith_Maths::Matrix4 xOut(1.0f);
	ZENITH_ASSERT_TRUE(xCache.TryGetPrev(3u, xOut), "3 present after first BeginFrame");

	// Frame with no record for 3, then advance: 3 falls out of history.
	xCache.BeginFrame();
	ZENITH_ASSERT_FALSE(xCache.TryGetPrev(3u, xOut), "3 evicted after a frame without a record");
}
