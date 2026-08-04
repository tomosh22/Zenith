#pragma once

// Pure swapchain-configuration decisions, kept OUT of the Vulkan backend so they
// are testable in every config. Zenith/Vulkan is excluded from the Null_* builds
// (Sharpmake_Common.cs SourceFilesBuildExcludeRegex), and Null_* is the only
// config CI runs -- so anything living in the backend is untestable by the gate
// BY CONSTRUCTION. These functions take plain integers, the backend passes its
// vk:: values straight in, and static_asserts there pin the bit values.

// Mirrors VkSurfaceTransformFlagBitsKHR. Values are fixed by the Vulkan spec;
// Flux_SwapchainPolicy_StaticAsserts in the Vulkan backend pins them.
namespace Flux_SurfaceTransform
{
	static constexpr u_int32 uIDENTITY                     = 0x00000001;
	static constexpr u_int32 uROTATE_90                    = 0x00000002;
	static constexpr u_int32 uROTATE_180                   = 0x00000004;
	static constexpr u_int32 uROTATE_270                   = 0x00000008;
	static constexpr u_int32 uHORIZONTAL_MIRROR            = 0x00000010;
	static constexpr u_int32 uHORIZONTAL_MIRROR_ROTATE_90  = 0x00000020;
	static constexpr u_int32 uHORIZONTAL_MIRROR_ROTATE_180 = 0x00000040;
	static constexpr u_int32 uHORIZONTAL_MIRROR_ROTATE_270 = 0x00000080;
	static constexpr u_int32 uINHERIT                      = 0x00000100;
}

namespace Flux_SwapchainPolicy
{
	// Which transform to put in VkSwapchainCreateInfoKHR::preTransform.
	//
	// preTransform declares the rotation the APPLICATION has already baked into
	// its rendered content. Zenith bakes none -- the projection is built with no
	// knowledge of the surface transform -- so declaring currentTransform on a
	// rotated surface is a LIE and the output lands sideways. That is exactly
	// what happened to any Android activity whose screenOrientation differs from
	// the panel's native orientation.
	//
	// So: ask for IDENTITY whenever the surface offers it, and the presentation
	// engine performs the rotation itself. Correct on every orientation, at the
	// cost of a compositor pass on devices that would otherwise have handled the
	// rotation for free. (Baking the rotation into the projection -- "pre-
	// rotation" -- is the zero-cost alternative, but it has to be threaded
	// through every projection, viewport and scissor in shared render code.)
	//
	// If IDENTITY is not offered, fall back to currentTransform: it is the only
	// value guaranteed to be supported, and a sideways image beats a failed
	// swapchain creation.
	inline u_int32 SelectPreTransform(u_int32 uSupportedTransforms, u_int32 uCurrentTransform)
	{
		if ((uSupportedTransforms & Flux_SurfaceTransform::uIDENTITY) != 0u)
		{
			return Flux_SurfaceTransform::uIDENTITY;
		}
		return uCurrentTransform;
	}

	// True when the app will be presenting through a transform it has not baked
	// in -- i.e. the image will be visibly rotated. Only possible when the
	// surface refuses IDENTITY; used to emit one honest warning rather than
	// leaving a sideways image unexplained.
	inline bool WillRenderRotated(u_int32 uSupportedTransforms, u_int32 uCurrentTransform)
	{
		const u_int32 uChosen = SelectPreTransform(uSupportedTransforms, uCurrentTransform);
		return uChosen != Flux_SurfaceTransform::uIDENTITY;
	}

	// Whether the swapchain's imageExtent must be TRANSPOSED for a chosen
	// preTransform.
	//
	// A 90/270 preTransform means the swapchain images live in the display's
	// native space, which is the window space with width and height swapped --
	// so the extent has to be swapped to match, or the image is stretched to the
	// wrong aspect on top of being rotated. Only ever true on the fallback path
	// (a surface that refuses IDENTITY); with IDENTITY the images live in window
	// space and currentExtent is already correct.
	inline bool ShouldTransposeExtent(u_int32 uChosenTransform)
	{
		return uChosenTransform == Flux_SurfaceTransform::uROTATE_90
			|| uChosenTransform == Flux_SurfaceTransform::uROTATE_270
			|| uChosenTransform == Flux_SurfaceTransform::uHORIZONTAL_MIRROR_ROTATE_90
			|| uChosenTransform == Flux_SurfaceTransform::uHORIZONTAL_MIRROR_ROTATE_270;
	}

	// Whether VkSurfaceCapabilitiesKHR::currentExtent can be used as-is.
	//
	// uNO_CURRENT_EXTENT (0xFFFFFFFF) is the "surface size is up to you" sentinel
	// -- the caller must pick from the window instead. A ZERO extent means the
	// surface is momentarily unusable (a MINIMISED window); creating a 0x0
	// swapchain is invalid usage and the validation layer's debug callback kills
	// the process, so the caller must wait for a real size instead.
	static constexpr u_int32 uNO_CURRENT_EXTENT = 0xFFFFFFFFu;

	inline bool IsCurrentExtentUsable(u_int32 uWidth, u_int32 uHeight)
	{
		if (uWidth == uNO_CURRENT_EXTENT || uHeight == uNO_CURRENT_EXTENT)
		{
			return false;
		}
		return uWidth != 0u && uHeight != 0u;
	}

	// Whether a swapchain reported SUBOPTIMAL should actually be recreated.
	//
	// SUBOPTIMAL means "still usable, but no longer an ideal match for the
	// surface" -- typically the surface resized or rotated without ever going
	// OUT_OF_DATE. Recreating unconditionally risks a rebuild every frame if a
	// driver reports SUBOPTIMAL permanently, so only rebuild when something the
	// swapchain is actually built FROM has changed. A surface that is suboptimal
	// for a reason we cannot act on is left alone; it renders, just not ideally.
	inline bool ShouldRecreateForSuboptimal(
		u_int32 uSurfaceWidth, u_int32 uSurfaceHeight, u_int32 uSurfaceTransform,
		u_int32 uActiveWidth, u_int32 uActiveHeight, u_int32 uActiveTransform)
	{
		if (uSurfaceTransform != uActiveTransform)
		{
			return true;
		}
		if (!IsCurrentExtentUsable(uSurfaceWidth, uSurfaceHeight))
		{
			// Sentinel or zero: the surface size is not something we can compare
			// against, so leave the decision to the OUT_OF_DATE path.
			return false;
		}
		return uSurfaceWidth != uActiveWidth || uSurfaceHeight != uActiveHeight;
	}
}
