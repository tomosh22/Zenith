#pragma once

#include "Flux/Flux.h"

enum SSR_DebugMode : u_int
{
	SSR_DEBUG_NONE = 0,
	SSR_DEBUG_RAY_DIRECTIONS,      // View-space reflection direction (RGB)
	SSR_DEBUG_SCREEN_DIRECTIONS,   // Screen-space march direction (RG)
	SSR_DEBUG_HIT_POSITIONS,       // World-space hit position / 100
	SSR_DEBUG_REFLECTION_UVS,      // Screen UV of hit (RG)
	SSR_DEBUG_CONFIDENCE,          // Hit confidence mask (grayscale)
	SSR_DEBUG_DEPTH_COMPARISON,    // Ray vs scene depth (R=miss, G=hit, B=distance)
	SSR_DEBUG_EDGE_FADE,           // Screen edge fadeout mask
	SSR_DEBUG_MARCH_DISTANCE,      // How far along ray before hit (grayscale)
	SSR_DEBUG_FINAL_RESULT,        // Final reflection with confidence
	SSR_DEBUG_ROUGHNESS,           // Visualize GBuffer roughness values
	SSR_DEBUG_WORLD_NORMAL_Y,      // Visualize world normal Y component
	SSR_DEBUG_COUNT
};

class Flux_SSR
{
public:
	Flux_SSR() = delete;
	~Flux_SSR() = delete;

	static void Initialise();
	static void Shutdown();
	static void Reset();

	static void Render(void*);
	static void SubmitRenderTask();
	static void WaitForRenderTask();

	// For deferred shading to sample
	static Flux_ShaderResourceView& GetReflectionSRV();
	static bool IsEnabled();
	static bool IsInitialised();

private:
	// Render passes
	static void RenderRayMarch();
	static void RenderResolve();

	// Render target management
	static void CreateRenderTargets();
	static void DestroyRenderTargets();

	// Render targets
	static Flux_RenderAttachment s_xRayMarchResult;      // RGBA16F: RGB=reflection, A=confidence
	static Flux_RenderAttachment s_xResolvedReflection;  // RGBA16F: blurred reflection
	static Flux_TargetSetup s_xRayMarchTargetSetup;
	static Flux_TargetSetup s_xResolveTargetSetup;

	// Configuration
	static bool s_bEnabled;
	static bool s_bInitialised;
};
