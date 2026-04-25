#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

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

	static void SetupRenderGraph(Flux_RenderGraph& xGraph);

	// Called every frame before Compile — detects runtime toggle of the
	// roughness-blur debug variable and requests a full graph rebuild via
	// Flux::RequestGraphRebuild(). A rebuild (not just MarkDirty) is required
	// because Flux_DeferredShading captures GetReflectionHandle() at
	// SetupRenderGraph time; the deferred pass's declared Read on the SSR
	// output must update when the toggle flips which transient serves as the
	// "output". MarkDirty alone would re-Compile on stale declarations.
	static void ApplyBlurSelectionToGraph(Flux_RenderGraph& xGraph);

	// For deferred shading to sample — returns the handle currently serving
	// as SSR's output (resolved if blur is on, raw if off). Consumers should
	// declare a Read on this handle; no runtime re-binding required.
	static Flux_TransientHandle GetReflectionHandle();

	// For deferred shading to sample — same selection as GetReflectionHandle
	// but returns the live SRV for BindSRV.
	static Flux_ShaderResourceView& GetReflectionSRV();
	static bool IsEnabled();
	static bool IsInitialised();

	// Attachment accessors
	static Flux_RenderAttachment& GetRayMarchAttachment();
	static Flux_RenderAttachment& GetResolvedAttachment();

private:
	static bool s_bInitialised;
};
