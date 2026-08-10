// =====================================================================
// Flux_SkyboxVelocity — the sky's TAA motion vector, and nothing else.
//
// Subsystem: Skybox. Recorded by the "Skybox Velocity" pass, which exists only
// while the TAA velocity latch is on (main view). It writes the ONE optional
// 5th G-buffer target (MRT_INDEX_VELOCITY) at sky pixels and touches nothing
// else — the sky's three shading programs (SolidColour / Cubemap / Atmosphere)
// keep their untouched 4-target GBufferOut.
//
// ★ ONE PASS, NOT THREE SHADING VARIANTS. Sky velocity is a pure function of
// the VIEW RAY: whichever program painted the pixel, a point at infinity moves
// on screen for exactly one reason (the camera turned) and by exactly one
// amount. Duplicating the velocity write into all three shading programs would
// have triplicated the atmosphere raymarch to compute a value none of it feeds.
//
// ★ w = 0 IS THE WHOLE TRICK. Pushing the view ray through the view-proj with
// w = 0 drops the view matrix's translation column, so camera TRANSLATION
// contributes nothing — which is correct, and not an approximation: a point at
// infinity does not parallax. Only rotation moves the sky, which is why sky
// velocity is zero on a dollying camera and non-zero the instant you pan.
//
// Sky pixels are selected by the DEPTH TEST, not by a texture read + discard:
// the fullscreen triangle rasterises at NDC z = 1.0 and the pipeline runs
// LESSEQUAL with depth WRITE off, so it survives only where the stored depth is
// still the far-cleared 1.0 — i.e. exactly the pixels the sky itself painted,
// by exactly the same rule the sky pass uses (Flux_Skybox.cpp::BuildPipelines).
// =====================================================================

#include "Common/Bindings.slang"   // spine: GLOBAL/VIEW/BINDLESS at sets 0/1/2 + RayDir
import Common.Fullscreen;
import Common.Velocity;            // Flux_ClipToUV — the same UV convention the G-buffer writers use

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

// A velocity magnitude no real motion reaches (UV space, so 1.0 is a whole
// screen). Written when the direction was BEHIND the previous camera, where the
// perspective divide would fold it back onto a plausible-looking UV: the resolve
// then finds `uv - velocity` outside [0,1] and keeps the current frame, which is
// the honest answer for a pixel that had no history.
static const float fSKY_VELOCITY_NO_HISTORY = 2.0;

struct SkyVelocityOut
{
	// The pass has ONE attachment (the velocity target), so this is SV_Target0
	// here even though the same data is MRT_INDEX_VELOCITY (SV_Target4) inside
	// the 5-attachment G-buffer pass. The attachment INDEX is a property of the
	// pass, not of the value.
	float2 velocity : SV_Target0;
};

[shader("fragment")]
SkyVelocityOut fsMain(FullscreenVsOut input)
{
	// The world-space direction the CURRENT (jittered) projection maps to this
	// pixel. Taking it from the jittered inverse is deliberate: this fragment
	// exists because the jittered raster produced it, so this IS its direction.
	// Both clip positions below then come from that one direction through the
	// two UNJITTERED matrices, exactly as the G-buffer velocity writers push one
	// world position through them — so the motion vector is jitter-free at the
	// source and the resolve does no jitter subtraction.
	const float3 xRayDir = RayDir(input.uv);

	const float4 xClipCur  = mul(GetViewProjMatNoJitter(),     float4(xRayDir, 0.0));
	const float4 xClipPrev = mul(GetPrevViewProjMatNoJitter(), float4(xRayDir, 0.0));

	SkyVelocityOut o;
	if (xClipPrev.w <= 1e-6 || xClipCur.w <= 1e-6)
	{
		o.velocity = float2(fSKY_VELOCITY_NO_HISTORY, fSKY_VELOCITY_NO_HISTORY);
		return o;
	}

	o.velocity = Flux_ClipToUV(xClipCur) - Flux_ClipToUV(xClipPrev);
	return o;
}
