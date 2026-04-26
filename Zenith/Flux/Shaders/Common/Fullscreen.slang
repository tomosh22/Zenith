// =====================================================================
// Common.Fullscreen — shared Slang module.
//
// Replaces the legacy Flux_Fullscreen_UV.vert by exposing a public
// vertex-stage helper that returns the standard fullscreen-triangle UV
// + clip position. Subsystems whose vsMain is "just a fullscreen pass"
// `import Common.Fullscreen` and write a one-line vsMain wrapper.
//
// Module path: imported as `Common.Fullscreen` (Slang dot-paths map to
// directories under the search root).
// =====================================================================

public struct FullscreenVsOut
{
	// Slang visibility: `public struct` exports the type name, but each
	// member needs its own `public` declaration for cross-module access.
	public float4 position : SV_Position;
	public float2 uv       : TEXCOORD0;
};

public FullscreenVsOut FullscreenVS(uint vertexId)
{
	uint xBit = vertexId & 1u;
	uint yBit = (vertexId >> 1u) & 1u;
	float xPos = float(xBit) * 2.0 - 1.0;
	float yPos = float(yBit) * 2.0 - 1.0;

	FullscreenVsOut o;
	o.position = float4(xPos, yPos, 1.0, 1.0);
	o.uv       = float2((xPos + 1.0) * 0.5, (yPos + 1.0) * 0.5);
	return o;
}
