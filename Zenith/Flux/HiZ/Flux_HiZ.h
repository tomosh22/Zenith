#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

class Flux_HiZ
{
public:
	Flux_HiZ() = delete;
	~Flux_HiZ() = delete;

	static void Initialise();
	static void Shutdown();

	static void SetupRenderGraph(Flux_RenderGraph& xGraph);

	// Accessors for other systems (SSR, SSAO, etc.)
	static Flux_ShaderResourceView& GetHiZSRV();           // Full mip chain
	static u_int GetMipCount();
	static Flux_ShaderResourceView& GetMipSRV(u_int uMip); // Single mip access
	static Flux_UnorderedAccessView_Texture& GetMipUAV(u_int uMip); // For compute write

	static bool IsEnabled();

	// For resize callback access
	static constexpr u_int uHIZ_MAX_MIPS = 12;  // Supports up to 4096x4096
	static Flux_RenderAttachment s_xHiZBuffer;
	static u_int s_uMipCount;

private:
	static bool s_bEnabled;
	static bool s_bInitialised;
};
