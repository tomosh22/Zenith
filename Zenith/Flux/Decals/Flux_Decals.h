#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Maths/Zenith_Maths.h"

class Zenith_TextureAsset;

// Flux_Decals — deferred screen-space box decal renderer.
//
// Each decal is an oriented unit cube in world space. The Apply pass rasterizes
// the cube, samples the depth buffer per fragment, reconstructs world position,
// transforms to decal-local space, discards if outside the unit box, and
// writes blended values into all three G-buffer MRTs (diffuse / normalsAmbient
// / material). The G-buffer is consumed downstream by SSR, SSGI, SSAO, and
// DeferredShading as if the decal were stamped onto the original geometry, so
// reflections and lighting interact with the decal correctly.
//
// Two-pass decomposition: a "Decal Normals Copy" pass clones the live
// normalsAmbient MRT into a transient before Apply runs, because Apply needs
// to read the pre-decal scene normal (for the surface-alignment test) while
// also writing the live MRT. Flux's render graph doesn't expose Vulkan input
// attachments / subpass self-dependencies, so the copy is required.
//
// Game-side API: spawn a decal at a raycast hit point with SpawnDecal().
// Decals fade out over their lifetime and the oldest slot recycles when the
// 64-slot pool fills.
class Flux_Decals
{
public:
	static void Initialise();
	static void Shutdown();
	static void BuildPipelines();
	static void SetupRenderGraph(Flux_RenderGraph& xGraph);

	// Game-side API. Pool-recycles the oldest slot when full.
	// pxTexture is reserved for v2 (texture-array support) — v1 ignores it
	// and uses the procedural bullet-hole pattern.
	static void SpawnDecal(const Zenith_Maths::Vector3& xPosition,
	                       const Zenith_Maths::Vector3& xNormal,
	                       Zenith_TextureAsset*         pxTexture,
	                       float                        fSize,
	                       float                        fLifetime);

	static bool IsInitialised() { return s_bInitialised; }

	static constexpr u_int uMAX_DECALS = 64;

#ifdef ZENITH_TESTING
	// Test-only inspection of the CPU pool. Not gated on Flux GPU init —
	// the CPU pool is initialised eagerly so tests without a live render
	// loop can still call SpawnDecal and observe the bookkeeping.
	struct TestSlotView
	{
		Zenith_Maths::Vector3 m_xPosition;
		Zenith_Maths::Vector3 m_xNormal;
		float                 m_fRemainingLifetime;
		bool                  m_bActive;
	};
	static u_int        GetActiveCountForTest();
	static TestSlotView GetSlotForTest(u_int uSlotIndex);
	static void         ResetForTest();
#endif

private:
	static bool s_bInitialised;
};
