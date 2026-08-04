#include "UnitTests/Zenith_UnitTests.h"
#include "Flux/Flux_SwapchainPolicy.h"

// These run in EVERY config, including the Null_* headless one CI executes --
// which is the whole reason the policy lives outside Zenith/Vulkan. The bugs
// they pin were all found on the Vulkan path, where no gate can reach them.

// --- SelectPreTransform ------------------------------------------------------

ZENITH_TEST(FluxSwapchainPolicy, PreTransformPrefersIdentityWhenOffered)
{
	// The ordinary desktop case: nothing rotated, IDENTITY offered -> IDENTITY.
	const u_int32 uDesktop = Flux_SurfaceTransform::uIDENTITY;
	ZENITH_ASSERT_EQ(
		Flux_SwapchainPolicy::SelectPreTransform(uDesktop, Flux_SurfaceTransform::uIDENTITY),
		Flux_SurfaceTransform::uIDENTITY);

	// ...and IDENTITY wins over a NON-rotation transform too, so the choice is
	// genuinely "prefer IDENTITY" and not "echo whatever currentTransform says".
	// Without this second case the test passes under the original bug.
	const u_int32 uMirrored = Flux_SurfaceTransform::uIDENTITY | Flux_SurfaceTransform::uHORIZONTAL_MIRROR;
	ZENITH_ASSERT_EQ(
		Flux_SwapchainPolicy::SelectPreTransform(uMirrored, Flux_SurfaceTransform::uHORIZONTAL_MIRROR),
		Flux_SurfaceTransform::uIDENTITY);
}

ZENITH_TEST(FluxSwapchainPolicy, PreTransformIsIdentityOnARotatedSurface)
{
	// THE REGRESSION THIS EXISTS FOR. A landscape activity on a portrait-native
	// panel reports currentTransform = ROTATE_90. Declaring that as preTransform
	// claims the app already rotated its content -- it has not -- and the image
	// lands sideways. We must ask for IDENTITY and let the compositor rotate.
	const u_int32 uSupported = Flux_SurfaceTransform::uIDENTITY | Flux_SurfaceTransform::uROTATE_90;
	ZENITH_ASSERT_EQ(
		Flux_SwapchainPolicy::SelectPreTransform(uSupported, Flux_SurfaceTransform::uROTATE_90),
		Flux_SurfaceTransform::uIDENTITY);
	ZENITH_ASSERT_FALSE(
		Flux_SwapchainPolicy::WillRenderRotated(uSupported, Flux_SurfaceTransform::uROTATE_90));
}

ZENITH_TEST(FluxSwapchainPolicy, PreTransformCoversEveryRotation)
{
	const u_int32 uSupported = Flux_SurfaceTransform::uIDENTITY
		| Flux_SurfaceTransform::uROTATE_90
		| Flux_SurfaceTransform::uROTATE_180
		| Flux_SurfaceTransform::uROTATE_270;
	const u_int32 auRotations[] = {
		Flux_SurfaceTransform::uROTATE_90,
		Flux_SurfaceTransform::uROTATE_180,
		Flux_SurfaceTransform::uROTATE_270,
	};
	for (u_int32 uRotation : auRotations)
	{
		ZENITH_ASSERT_EQ(
			Flux_SwapchainPolicy::SelectPreTransform(uSupported, uRotation),
			Flux_SurfaceTransform::uIDENTITY);
	}
}

ZENITH_TEST(FluxSwapchainPolicy, PreTransformFallsBackWhenIdentityUnsupported)
{
	// currentTransform is the one value the spec guarantees is supported, so a
	// surface that refuses IDENTITY must still get a swapchain -- sideways beats
	// a failed create. WillRenderRotated reports it so the log can say why.
	// The supported MASK deliberately carries more than one bit, so an
	// implementation that returned the mask itself rather than currentTransform
	// fails here instead of coincidentally passing.
	const u_int32 uSupported = Flux_SurfaceTransform::uROTATE_90 | Flux_SurfaceTransform::uROTATE_180;
	ZENITH_ASSERT_EQ(
		Flux_SwapchainPolicy::SelectPreTransform(uSupported, Flux_SurfaceTransform::uROTATE_180),
		Flux_SurfaceTransform::uROTATE_180);
	ZENITH_ASSERT_TRUE(
		Flux_SwapchainPolicy::WillRenderRotated(uSupported, Flux_SurfaceTransform::uROTATE_180));
}

ZENITH_TEST(FluxSwapchainPolicy, PreTransformReportsRotatedOnlyWhenItActuallyIs)
{
	// Both directions, since one alone is satisfiable by a constant.
	// Rotated surface that DOES offer IDENTITY -> we pick IDENTITY -> not rotated.
	ZENITH_ASSERT_FALSE(Flux_SwapchainPolicy::WillRenderRotated(
		Flux_SurfaceTransform::uIDENTITY | Flux_SurfaceTransform::uROTATE_270,
		Flux_SurfaceTransform::uROTATE_270));
	// Unrotated surface -> not rotated.
	ZENITH_ASSERT_FALSE(Flux_SwapchainPolicy::WillRenderRotated(
		Flux_SurfaceTransform::uIDENTITY, Flux_SurfaceTransform::uIDENTITY));
	// Rotated surface that does NOT offer IDENTITY -> rotated.
	ZENITH_ASSERT_TRUE(Flux_SwapchainPolicy::WillRenderRotated(
		Flux_SurfaceTransform::uROTATE_90, Flux_SurfaceTransform::uROTATE_90));
}

ZENITH_TEST(FluxSwapchainPolicy, ExtentIsTransposedOnlyForQuarterTurns)
{
	// Only the fallback path can choose a rotated preTransform, and a 90/270 one
	// puts the images in display-native space -- so the extent must be swapped or
	// the image is aspect-mangled on top of being rotated.
	ZENITH_ASSERT_TRUE(Flux_SwapchainPolicy::ShouldTransposeExtent(Flux_SurfaceTransform::uROTATE_90));
	ZENITH_ASSERT_TRUE(Flux_SwapchainPolicy::ShouldTransposeExtent(Flux_SurfaceTransform::uROTATE_270));
	ZENITH_ASSERT_TRUE(Flux_SwapchainPolicy::ShouldTransposeExtent(Flux_SurfaceTransform::uHORIZONTAL_MIRROR_ROTATE_90));
	ZENITH_ASSERT_TRUE(Flux_SwapchainPolicy::ShouldTransposeExtent(Flux_SurfaceTransform::uHORIZONTAL_MIRROR_ROTATE_270));

	// A half turn preserves the aspect, and IDENTITY -- the normal path -- must
	// never transpose, or every ordinary swapchain would be built sideways.
	ZENITH_ASSERT_FALSE(Flux_SwapchainPolicy::ShouldTransposeExtent(Flux_SurfaceTransform::uIDENTITY));
	ZENITH_ASSERT_FALSE(Flux_SwapchainPolicy::ShouldTransposeExtent(Flux_SurfaceTransform::uROTATE_180));
	ZENITH_ASSERT_FALSE(Flux_SwapchainPolicy::ShouldTransposeExtent(Flux_SurfaceTransform::uHORIZONTAL_MIRROR));
}

// --- IsCurrentExtentUsable ---------------------------------------------------

ZENITH_TEST(FluxSwapchainPolicy, CurrentExtentUsableForRealSizes)
{
	ZENITH_ASSERT_TRUE(Flux_SwapchainPolicy::IsCurrentExtentUsable(1920u, 1080u));
	// 1x1 is the smallest legal swapchain -- the boundary between usable and the
	// zero case, asserted from both sides here so a constant-true implementation
	// cannot satisfy this test on its own.
	ZENITH_ASSERT_TRUE(Flux_SwapchainPolicy::IsCurrentExtentUsable(1u, 1u));
	ZENITH_ASSERT_FALSE(Flux_SwapchainPolicy::IsCurrentExtentUsable(0u, 1u));
	ZENITH_ASSERT_FALSE(Flux_SwapchainPolicy::IsCurrentExtentUsable(1u, 0u));
}

ZENITH_TEST(FluxSwapchainPolicy, CurrentExtentRejectsTheSentinel)
{
	// 0xFFFFFFFF means "you pick" -- the caller must query the window instead.
	const u_int32 uSentinel = Flux_SwapchainPolicy::uNO_CURRENT_EXTENT;
	ZENITH_ASSERT_FALSE(Flux_SwapchainPolicy::IsCurrentExtentUsable(uSentinel, uSentinel));
	ZENITH_ASSERT_FALSE(Flux_SwapchainPolicy::IsCurrentExtentUsable(uSentinel, 1080u));
	ZENITH_ASSERT_FALSE(Flux_SwapchainPolicy::IsCurrentExtentUsable(1920u, uSentinel));
}

ZENITH_TEST(FluxSwapchainPolicy, CurrentExtentRejectsZero)
{
	// A MINIMISED window reports 0x0. Creating a 0x0 swapchain is invalid usage
	// and DebugCallback turns that into a process kill, so this must be false.
	// The old code returned currentExtent whenever it was not the sentinel --
	// and on Windows it is never the sentinel -- so minimising killed the app.
	ZENITH_ASSERT_FALSE(Flux_SwapchainPolicy::IsCurrentExtentUsable(0u, 0u));
	ZENITH_ASSERT_FALSE(Flux_SwapchainPolicy::IsCurrentExtentUsable(1920u, 0u));
	ZENITH_ASSERT_FALSE(Flux_SwapchainPolicy::IsCurrentExtentUsable(0u, 1080u));
}

// --- ShouldRecreateForSuboptimal ---------------------------------------------

ZENITH_TEST(FluxSwapchainPolicy, SuboptimalRecreatesOnResize)
{
	ZENITH_ASSERT_TRUE(Flux_SwapchainPolicy::ShouldRecreateForSuboptimal(
		1280u, 720u, Flux_SurfaceTransform::uIDENTITY,
		1920u, 1080u, Flux_SurfaceTransform::uIDENTITY));
}

ZENITH_TEST(FluxSwapchainPolicy, SuboptimalRecreatesOnTransformChange)
{
	// Device rotation with no size change (a square surface, or a rotation the
	// compositor absorbs) still needs the swapchain rebuilt.
	ZENITH_ASSERT_TRUE(Flux_SwapchainPolicy::ShouldRecreateForSuboptimal(
		1080u, 1080u, Flux_SurfaceTransform::uROTATE_90,
		1080u, 1080u, Flux_SurfaceTransform::uIDENTITY));
}

ZENITH_TEST(FluxSwapchainPolicy, SuboptimalDoesNotThrashWhenNothingChanged)
{
	// THE LOOP GUARD. A driver that reports SUBOPTIMAL permanently would other-
	// wise have us tear down and rebuild the swapchain every single frame.
	ZENITH_ASSERT_FALSE(Flux_SwapchainPolicy::ShouldRecreateForSuboptimal(
		1920u, 1080u, Flux_SurfaceTransform::uIDENTITY,
		1920u, 1080u, Flux_SurfaceTransform::uIDENTITY));
}

ZENITH_TEST(FluxSwapchainPolicy, SuboptimalDefersWhenSurfaceSizeIsUnusable)
{
	// Sentinel or zero: nothing meaningful to compare, so do not rebuild here --
	// the OUT_OF_DATE path owns that case. Transform changes still win, because
	// they are comparable regardless of extent.
	const u_int32 uSentinel = Flux_SwapchainPolicy::uNO_CURRENT_EXTENT;
	ZENITH_ASSERT_FALSE(Flux_SwapchainPolicy::ShouldRecreateForSuboptimal(
		uSentinel, uSentinel, Flux_SurfaceTransform::uIDENTITY,
		1920u, 1080u, Flux_SurfaceTransform::uIDENTITY));
	ZENITH_ASSERT_FALSE(Flux_SwapchainPolicy::ShouldRecreateForSuboptimal(
		0u, 0u, Flux_SurfaceTransform::uIDENTITY,
		1920u, 1080u, Flux_SurfaceTransform::uIDENTITY));
	ZENITH_ASSERT_TRUE(Flux_SwapchainPolicy::ShouldRecreateForSuboptimal(
		0u, 0u, Flux_SurfaceTransform::uROTATE_180,
		1920u, 1080u, Flux_SurfaceTransform::uIDENTITY));
}
