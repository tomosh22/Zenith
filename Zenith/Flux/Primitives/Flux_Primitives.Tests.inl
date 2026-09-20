//------------------------------------------------------------------------------
// Flux_Primitives unit tests.
// Included at the bottom of Flux_Primitives.cpp (the module-owns-its-tests
// pattern — see Flux/HiZ/Flux_HiZ.Tests.inl / Flux/Decals/Flux_Decals.Tests.inl).
//
// ZEN-40: ExecuteGBuffer used to drain + Clear() the six DEBUG instance queues
// (m_xSphereInstances / m_xCubeInstances / m_xLineInstances /
// m_xCapsuleInstances / m_xCylinderInstances / m_xTriangleInstances) only while
// Zenith_GraphicsOptions::Get().m_bPrimitivesEnabled was true. With the option
// off, every Add*() call accumulated into those queues forever, and the whole
// backlog was drawn in one frame — thousands of BindDrawConstants calls in a
// single ExecuteGBuffer — the instant the option was re-enabled. On a
// photo-tour run that disables primitives for the tour's duration
// (Games/RenderTest/Tests/PhotoTour.cpp), that backlog overran a Vulkan
// worker's scratch allocator partition (see the ZEN-40 ticket body for the
// exact arithmetic).
//
// ConsumeFrameBoundary is the fix: the six debug queues (and the two gameplay
// ones, which were already unconditional) are Clear()'d on EVERY call,
// regardless of bDebugPrimitivesEnabled — only the copy-to-render-locals step
// is conditional. This is pure CPU-side Zenith_Vector bookkeeping with no
// device, no graph and no engine singleton, so the invariant is provable
// headless: construct a standalone Flux_PrimitivesImpl on the stack, submit
// through the public Add* API, and watch GetDebugInstanceQueueSizeForTesting()
// after each drain.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"

#ifdef ZENITH_TESTING

namespace
{
	// One frame's worth of drain-target locals, bundled so each ZENITH_TEST
	// body doesn't repeat all eight declarations.
	struct Flux_PrimitivesTestFrameLocals
	{
		Zenith_Vector<Flux_PrimitivesSphereInstance>   m_xSphere;
		Zenith_Vector<Flux_PrimitivesCubeInstance>     m_xCube;
		Zenith_Vector<Flux_PrimitivesLineInstance>     m_xLine;
		Zenith_Vector<Flux_PrimitivesCapsuleInstance>  m_xCapsule;
		Zenith_Vector<Flux_PrimitivesCylinderInstance> m_xCylinder;
		Zenith_Vector<Flux_PrimitivesTriangleInstance> m_xTriangle;
		Zenith_Vector<Flux_PrimitivesSphereInstance>   m_xGameplaySphere;
		Zenith_Vector<Flux_PrimitivesCylinderInstance> m_xGameplayCylinder;
	};
}

// The invariant that failed before the fix: repeatedly submitting through the
// debug channel while the option is OFF must never let the debug queues carry
// a backlog from one frame boundary to the next. Before ZEN-40, the drain this
// test calls (translated back into the pre-fix shape: Clear() only reachable
// from inside `if (bDebugPrimitivesEnabled)`) would leave every one of these
// ten submissions sitting in the queues forever — this assertion would fail
// on frame 0 already, and GetDebugInstanceQueueSizeForTesting() would climb by
// 2 every iteration instead of returning to 0.
ZENITH_TEST(FluxPrimitives, DebugQueuesDoNotAccumulateAcrossFrameBoundariesWhileDisabled)
{
	Flux_PrimitivesImpl xPrimitives;
	ZENITH_ASSERT_EQ(xPrimitives.GetDebugInstanceQueueSizeForTesting(), 0u,
		"a freshly constructed instance starts with empty debug queues");

	for (u_int uFrame = 0; uFrame < 10; ++uFrame)
	{
		xPrimitives.AddSphere(Zenith_Maths::Vector3(0.0f), 1.0f, Zenith_Maths::Vector3(1.0f));
		xPrimitives.AddCube(Zenith_Maths::Vector3(0.0f), Zenith_Maths::Vector3(1.0f), Zenith_Maths::Vector3(1.0f));

		Flux_PrimitivesTestFrameLocals xFrame;
		xPrimitives.ConsumeFrameBoundary(/*bDebugPrimitivesEnabled=*/false,
			xFrame.m_xSphere, xFrame.m_xCube, xFrame.m_xLine, xFrame.m_xCapsule,
			xFrame.m_xCylinder, xFrame.m_xTriangle,
			xFrame.m_xGameplaySphere, xFrame.m_xGameplayCylinder);

		// Disabled: nothing is copied out for drawing this frame...
		ZENITH_ASSERT_EQ(xFrame.m_xSphere.GetSize(), 0u, "disabled: no sphere copied to the render locals on frame %u", uFrame);
		ZENITH_ASSERT_EQ(xFrame.m_xCube.GetSize(), 0u, "disabled: no cube copied to the render locals on frame %u", uFrame);

		// ...but the underlying queues must not be carrying anything forward —
		// this is the assertion that catches the unbounded-accumulation defect.
		ZENITH_ASSERT_EQ(xPrimitives.GetDebugInstanceQueueSizeForTesting(), 0u,
			"debug queues must be fully drained at every frame boundary even while disabled (frame %u)", uFrame);
	}
}

// The option-ON path: submissions are copied out to the render locals AND the
// source queues are still cleared, so toggling the option back off on the
// very next frame cannot replay anything either.
ZENITH_TEST(FluxPrimitives, DebugQueuesDrainToLocalsAndClearWhenEnabled)
{
	Flux_PrimitivesImpl xPrimitives;

	xPrimitives.AddSphere(Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f), 2.0f, Zenith_Maths::Vector3(0.5f));
	xPrimitives.AddLine(Zenith_Maths::Vector3(0.0f), Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(1.0f));

	Flux_PrimitivesTestFrameLocals xFrame;
	xPrimitives.ConsumeFrameBoundary(/*bDebugPrimitivesEnabled=*/true,
		xFrame.m_xSphere, xFrame.m_xCube, xFrame.m_xLine, xFrame.m_xCapsule,
		xFrame.m_xCylinder, xFrame.m_xTriangle,
		xFrame.m_xGameplaySphere, xFrame.m_xGameplayCylinder);

	ZENITH_ASSERT_EQ(xFrame.m_xSphere.GetSize(), 1u, "enabled: the one submitted sphere is copied out");
	ZENITH_ASSERT_EQ(xFrame.m_xLine.GetSize(), 1u, "enabled: the one submitted line is copied out");
	ZENITH_ASSERT_EQ(xPrimitives.GetDebugInstanceQueueSizeForTesting(), 0u,
		"the source queues are cleared even though this frame drew from them");

	// A second boundary right after, still enabled, with nothing re-submitted:
	// must not replay the frame just drawn.
	Flux_PrimitivesTestFrameLocals xNextFrame;
	xPrimitives.ConsumeFrameBoundary(/*bDebugPrimitivesEnabled=*/true,
		xNextFrame.m_xSphere, xNextFrame.m_xCube, xNextFrame.m_xLine, xNextFrame.m_xCapsule,
		xNextFrame.m_xCylinder, xNextFrame.m_xTriangle,
		xNextFrame.m_xGameplaySphere, xNextFrame.m_xGameplayCylinder);
	ZENITH_ASSERT_EQ(xNextFrame.m_xSphere.GetSize(), 0u, "nothing replays on the frame after a drain");
	ZENITH_ASSERT_EQ(xNextFrame.m_xLine.GetSize(), 0u, "nothing replays on the frame after a drain");
}

// The gameplay channel is unconditional regardless of the debug option, and
// was already correct before ZEN-40 — pinned here so a future change to
// ConsumeFrameBoundary can't quietly couple the two channels' gating.
ZENITH_TEST(FluxPrimitives, GameplayQueueDrainsRegardlessOfDebugOption)
{
	Flux_PrimitivesImpl xPrimitives;

	const u_int uGrew = xPrimitives.SubmitGameplayCylinderAndSphere(
		Zenith_Maths::Vector3(0.0f), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), 0.1f,
		Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), 0.2f,
		Zenith_Maths::Vector3(1.0f, 0.82f, 0.08f));
	ZENITH_ASSERT_EQ(uGrew, 1u, "both gameplay queues observed to grow");

	Flux_PrimitivesTestFrameLocals xFrame;
	xPrimitives.ConsumeFrameBoundary(/*bDebugPrimitivesEnabled=*/false,
		xFrame.m_xSphere, xFrame.m_xCube, xFrame.m_xLine, xFrame.m_xCapsule,
		xFrame.m_xCylinder, xFrame.m_xTriangle,
		xFrame.m_xGameplaySphere, xFrame.m_xGameplayCylinder);

	ZENITH_ASSERT_EQ(xFrame.m_xGameplaySphere.GetSize(), 1u,
		"gameplay sphere is copied out even though the debug option is off");
	ZENITH_ASSERT_EQ(xFrame.m_xGameplayCylinder.GetSize(), 1u,
		"gameplay cylinder is copied out even though the debug option is off");
}

#endif // ZENITH_TESTING
