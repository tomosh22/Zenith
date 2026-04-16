#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

enum SSGI_DebugMode : u_int
{
	SSGI_DEBUG_NONE = 0,
	SSGI_DEBUG_RAY_DIRECTIONS,      // View-space hemisphere direction (RGB)
	SSGI_DEBUG_HIT_POSITIONS,       // World-space hit position / 100
	SSGI_DEBUG_CONFIDENCE,          // Hit confidence mask (grayscale)
	SSGI_DEBUG_FINAL_RESULT,        // Final GI with confidence
	SSGI_DEBUG_COUNT
};

class Flux_SSGI
{
public:
	Flux_SSGI() = delete;
	~Flux_SSGI() = delete;

	static void Initialise();
	static void Shutdown();

	static void SetupRenderGraph(Flux_RenderGraph& xGraph);

	// For deferred shading to sample
	static Flux_ShaderResourceView& GetSSGISRV();
	static bool IsEnabled();
	static bool IsInitialised();

	// Configuration
	static bool s_bEnabled;

	// Attachment accessors (return transient or owned depending on current mode)
	static Flux_RenderAttachment& GetRawResultAttachment();
	static Flux_RenderAttachment& GetResolvedAttachment();
	static Flux_RenderAttachment& GetDenoisedAttachment();

private:
	static void CreateOwnedRenderTargets();
	static void DestroyOwnedRenderTargets();

	static bool s_bInitialised;
};
