#pragma once

#include <cstdint>

// Which of Flux_TerrainImpl's four G-buffer pipeline objects the "Terrain GBuffer"
// pass must bind this frame, as a pure function of two independent booleans:
//
//   * bVelocity  — Flux_GraphicsImpl::IsVelocityMRTActive(), the TAA velocity latch.
//                  Decides whether the pass framebuffer is 4 or 5 colour attachments
//                  (SetupRenderGraph reads the SAME latch when declaring the pass, and
//                  SetupTransients freezes it for the whole graph build).
//   * bWireframe — the Render/Terrain/Wireframe debug var, written by the terrain
//                  component's "Wireframe" checkbox and by --rendertest-wireframe.
//
// HISTORY — why this is a header and not a ternary. The record-time selection used to
// be nested, with velocity unconditionally beating wireframe, on the belief that a
// wireframe pipeline had to be 4-attachment. It does not: m_bWireframe is rasterizer
// state (vk::PolygonMode::eLine), orthogonal to the attachment set. Because TAA ships
// ON, that collapse left the wireframe branch unreachable and the checkbox did nothing
// in every default run. The two axes are genuinely independent, so this is a 2x2 and
// every combination gets its own prebuilt pipeline.
//
// Kept dependency-light (<cstdint> only, no Flux headers) so it is unit-testable in
// every configuration — same shape as Flux_TerrainExportRect.h in this directory.
enum class Flux_TerrainGBufferVariant : uint8_t
{
	SOLID = 0,           // 4 attachments, base G-buffer shader, eFill
	WIREFRAME,           // 4 attachments, base G-buffer shader, eLine
	VELOCITY_SOLID,      // 5 attachments, velocity shader,      eFill
	VELOCITY_WIREFRAME,  // 5 attachments, velocity shader,      eLine
	COUNT
};

// PURE. Total over both booleans — neither axis suppresses the other.
constexpr Flux_TerrainGBufferVariant Flux_TerrainSelectGBufferVariant(bool bVelocity, bool bWireframe)
{
	if (bVelocity)
	{
		return bWireframe ? Flux_TerrainGBufferVariant::VELOCITY_WIREFRAME
		                  : Flux_TerrainGBufferVariant::VELOCITY_SOLID;
	}
	return bWireframe ? Flux_TerrainGBufferVariant::WIREFRAME
	                  : Flux_TerrainGBufferVariant::SOLID;
}

// The colour-attachment count a variant's pipeline declares. This is the framebuffer
// contract the render-graph pass declaration must agree with — a mismatch between the
// pass's .Writes count and the bound pipeline's m_uNumColourAttachments is a Vulkan
// render-pass-compatibility error.
//
// Stating it as a function makes the load-bearing invariant testable: the WIREFRAME
// axis must NEVER move this number, only the VELOCITY axis may. The literals are
// static_asserted against uFLUX_MRT_CORE_COUNT / MRT_INDEX_COUNT in Flux_Terrain.cpp,
// which is where those constants are in scope.
constexpr uint32_t Flux_TerrainGBufferAttachmentCountForVariant(Flux_TerrainGBufferVariant eVariant)
{
	return (eVariant == Flux_TerrainGBufferVariant::VELOCITY_SOLID ||
	        eVariant == Flux_TerrainGBufferVariant::VELOCITY_WIREFRAME) ? 5u : 4u;
}

constexpr bool Flux_TerrainGBufferVariantIsWireframe(Flux_TerrainGBufferVariant eVariant)
{
	return eVariant == Flux_TerrainGBufferVariant::WIREFRAME ||
	       eVariant == Flux_TerrainGBufferVariant::VELOCITY_WIREFRAME;
}

constexpr bool Flux_TerrainGBufferVariantIsVelocity(Flux_TerrainGBufferVariant eVariant)
{
	return eVariant == Flux_TerrainGBufferVariant::VELOCITY_SOLID ||
	       eVariant == Flux_TerrainGBufferVariant::VELOCITY_WIREFRAME;
}
