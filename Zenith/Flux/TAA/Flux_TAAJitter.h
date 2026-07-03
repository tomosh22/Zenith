#pragma once

#include "Maths/Zenith_Maths.h"
#pragma warning(push, 0)
#include <glm/gtc/packing.hpp>   // packHalf2x16 / unpackHalf2x16 — RG16F velocity round-trip mirror
#pragma warning(pop)
#include <cmath>                 // fabsf, expf

// ============================================================================
// TAA sub-pixel camera jitter + motion-vector encode — PURE CPU cores.
//
// These are the headless-testable foundation of the temporal AA path (Part 1):
//   - Halton(base) low-discrepancy sequence (the jitter pattern source).
//   - Per-phase sub-pixel jitter offset (centred, [-0.5, +0.5] pixel).
//   - Projection-matrix jitter injection (an NDC/screen-space translation applied
//     ONLY to the main view's GPU constant-buffer copy — the CPU matrices stay
//     unjittered so CSM texel-snapping / preview / culling / streaming are
//     jitter-free by construction).
//   - Clip -> UV and the velocity convention (velocity = uvCurrent - uvPrev, UV
//     space; history lookup = uv - velocity), plus the RG16F storage round-trip.
//
// Nothing here touches the GPU or the renderer — every function is a pure math
// mirror of the corresponding shader code, pinned by Flux_TAAJitter.Tests.inl
// (hosted in an already-linked TU per the dead-strip idiom).
//
// CONVENTIONS (mirror the shaders EXACTLY — see Common/Velocity.slang once it lands):
//   - Vulkan clip space, GLM_FORCE_DEPTH_ZERO_TO_ONE + GLM_FORCE_LEFT_HANDED, so a
//     perspective projection sets clip.w = view.z (M[2][3] == 1).
//   - UV = ndc * 0.5 + 0.5 on BOTH axes.
//   - Jitter offset is in PIXELS; positive x shifts the projected point right,
//     positive y shifts it down (Vulkan ndc.y and uv.y both increase downward).
// ============================================================================

// --- Halton radical inverse -------------------------------------------------
// Low-discrepancy sequence in [0,1). uBase >= 2; uIndex 0 => 0 (the degenerate
// corner — callers offset by +1 so phase 0 is a real sample). Deterministic.
inline float Flux_Halton(u_int uBase, u_int uIndex)
{
	const float fInvBase = 1.0f / static_cast<float>(uBase);
	float fResult   = 0.0f;
	float fFraction = fInvBase;
	u_int u         = uIndex;
	while (u > 0u)
	{
		fResult   += static_cast<float>(u % uBase) * fFraction;
		u         /= uBase;
		fFraction *= fInvBase;
	}
	return fResult;
}

// --- per-phase sub-pixel jitter offset (pixels, centred at 0) ---------------
// Halton(2,3) is the standard TAA pattern. uPhase wraps within [0,uPhaseCount);
// +1 on the Halton index so phase 0 is the first NON-degenerate sample. Result
// is in [-0.5, +0.5) pixel — half a pixel of sub-sample coverage each side.
inline Zenith_Maths::Vector2 Flux_TAAJitterOffsetPixels(u_int uPhase, u_int uPhaseCount)
{
	const u_int uIdx = ((uPhaseCount == 0u) ? 0u : (uPhase % uPhaseCount)) + 1u;
	return Zenith_Maths::Vector2(Flux_Halton(2u, uIdx) - 0.5f,
	                             Flux_Halton(3u, uIdx) - 0.5f);
}

// --- projection-matrix jitter injection -------------------------------------
// Adds a constant screen-space translation of xJitterPixels to the projected
// result, independent of depth. The pixel offset maps to an NDC offset of
// 2 * pixels / dims; injecting it into the projection's z-column (which is scaled
// by clip.w = view.z after the divide) yields a depth-independent NDC shift.
//
// ELEMENT INDICES ARE PINNED BY THE NDC-SHIFT UNIT TEST — do not "simplify" them.
// glm mat4 is column-major and operator[] returns a COLUMN, so [2][0]/[2][1] are
// column 2 (the z column), rows 0/1 (x/y). A jitter of (0,0) returns xProj
// byte-for-byte (the memcmp-identity test underwrites the TAA-off byte-identical gate).
inline Zenith_Maths::Matrix4 Flux_ApplyJitterToProjection(
	const Zenith_Maths::Matrix4& xProj, const Zenith_Maths::Vector2& xJitterPixels,
	u_int uWidth, u_int uHeight)
{
	Zenith_Maths::Matrix4 xJittered = xProj;
	const float fNdcX = (uWidth  > 0u) ? (2.0f * xJitterPixels.x / static_cast<float>(uWidth))  : 0.0f;
	const float fNdcY = (uHeight > 0u) ? (2.0f * xJitterPixels.y / static_cast<float>(uHeight)) : 0.0f;
	xJittered[2][0] += fNdcX;
	xJittered[2][1] += fNdcY;
	return xJittered;
}

// --- clip -> UV + the velocity convention -----------------------------------
// UV of a clip-space position after the perspective divide (ndc * 0.5 + 0.5).
inline Zenith_Maths::Vector2 Flux_ClipToUV(const Zenith_Maths::Vector4& xClip)
{
	const Zenith_Maths::Vector2 xNdc(xClip.x / xClip.w, xClip.y / xClip.w);
	return Zenith_Maths::Vector2(xNdc.x * 0.5f + 0.5f, xNdc.y * 0.5f + 0.5f);
}

// Motion vector: velocity = uvCurrent - uvPrev (both from UNJITTERED matrices, so
// velocity is jitter-free at the source and the resolve does no jitter subtraction).
// A static pixel (uvCur == uvPrev) encodes exactly (0,0). History lookup = uv - velocity.
inline Zenith_Maths::Vector2 Flux_EncodeVelocityUV(
	const Zenith_Maths::Vector2& xUVCurrent, const Zenith_Maths::Vector2& xUVPrevious)
{
	return xUVCurrent - xUVPrevious;
}

// The RG16F storage round-trip a velocity value survives (half-float pack/unpack).
// fp16 has ~10 mantissa bits: near-zero (typical per-frame) velocities are extremely
// precise; magnitude-~1 (near-full-screen) velocities degrade to ~2px at 4K — fine,
// as fast pixels are clamped/rejected by the resolve anyway.
inline Zenith_Maths::Vector2 Flux_VelocityFp16RoundTrip(const Zenith_Maths::Vector2& xVelocity)
{
	return glm::unpackHalf2x16(glm::packHalf2x16(xVelocity));
}
