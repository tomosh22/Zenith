// =====================================================================
// Flux_Decals_Apply — Slang module.
//
// Subsystem: Decals. Deferred screen-space box decal apply pass. For
// each fragment of an oriented unit cube (one instance per active decal):
//   1. Reconstruct the underlying scene's world position from the depth
//      buffer.
//   2. Transform that world position into decal-local cube space.
//   3. Discard if outside the unit box [-0.5, 0.5]^3.
//   4. Discard if the (cloned, pre-decal) scene normal at this pixel
//      doesn't align with the decal's projection axis (gates leakage
//      onto perpendicular surfaces inside the box).
//   5. Output a procedural bullet-hole pattern (dark scorch, slightly
//      concave normal, raised roughness) into all three G-buffer MRTs.
//
// MRT 0 (diffuse)        — alpha-blend RGB; alpha-channel write masked.
// MRT 1 (normalsAmbient) — alpha-blend world-space normal RGB; alpha
//                          (AO) preserved.
// MRT 2 (material)       — alpha-blend roughness/metallic/specular;
//                          alpha (packed shading flags) preserved via the
//                          0x7 colour write mask — do NOT widen it.
// MRT 3 (emissive)       — not attached by the decal pass; untouched.
//
// All three MRTs use the same fAlpha output for the SrcAlpha factor.
// =====================================================================

// Spine via preprocessor #include (declares GLOBAL/VIEW/BINDLESS at sets 0/1/2),
// replacing `import Common.Frame;` and providing the view-reconstruction helpers
// (GetWorldPosFromDepthTex etc.) sourced from the VIEW set.
#include "Common/Bindings.slang"

struct DecalInstance
{
	float4x4 m_xWorld;
	float4x4 m_xWorldInverse;
	float4   m_xAxisOpacity;   // xyz = unit-length world-space projection axis, w = fade opacity
	float4   m_xParams;        // x = normal-alignment threshold, y = mode (0 = bullet hole, 1 = textured brush), z = brush bindless index (mode 1; asuint), w reserved
	float4   m_xColour;        // brush-indicator tint (rgb) + master alpha (w); unused by mode 0
};

// PASS frequency — this pass's own resources. Declared AFTER the spine #include,
// so it lands at set 3 (the first set past the persistent 0/1/2). Member NAMES
// are preserved exactly so the C++ binds (h<member>) still resolve. The brush
// mask now lives in the bindless table (set 2 g_xBindlessSet.g_axTextures),
// indexed per-decal by asuint(m_xParams.z).
struct PassParams
{
	Sampler2D                          g_xDepthTex;
	Sampler2D                          g_xNormalsCopyTex;  // pre-decal normalsAmbient clone
	StructuredBuffer<DecalInstance>    DecalBuffer;
};
ParameterBlock<PassParams> g_xPassSet;

// 36-element cube-corner index table — 12 triangles, 3 verts each.
// Front-face winding is CCW; combined with CULL_MODE_FRONT in the
// pipeline spec we render only the back faces, which guarantees fragment
// coverage even when the camera is inside the decal volume.
static const float3 CUBE_CORNERS[36] = {
	// -X face (left)
	float3(-0.5, -0.5, -0.5), float3(-0.5, -0.5,  0.5), float3(-0.5,  0.5,  0.5),
	float3(-0.5, -0.5, -0.5), float3(-0.5,  0.5,  0.5), float3(-0.5,  0.5, -0.5),
	// +X face (right)
	float3( 0.5, -0.5, -0.5), float3( 0.5,  0.5, -0.5), float3( 0.5,  0.5,  0.5),
	float3( 0.5, -0.5, -0.5), float3( 0.5,  0.5,  0.5), float3( 0.5, -0.5,  0.5),
	// -Y face (bottom)
	float3(-0.5, -0.5, -0.5), float3( 0.5, -0.5, -0.5), float3( 0.5, -0.5,  0.5),
	float3(-0.5, -0.5, -0.5), float3( 0.5, -0.5,  0.5), float3(-0.5, -0.5,  0.5),
	// +Y face (top)
	float3(-0.5,  0.5, -0.5), float3(-0.5,  0.5,  0.5), float3( 0.5,  0.5,  0.5),
	float3(-0.5,  0.5, -0.5), float3( 0.5,  0.5,  0.5), float3( 0.5,  0.5, -0.5),
	// -Z face (back)
	float3(-0.5, -0.5, -0.5), float3(-0.5,  0.5, -0.5), float3( 0.5,  0.5, -0.5),
	float3(-0.5, -0.5, -0.5), float3( 0.5,  0.5, -0.5), float3( 0.5, -0.5, -0.5),
	// +Z face (front)
	float3(-0.5, -0.5,  0.5), float3( 0.5, -0.5,  0.5), float3( 0.5,  0.5,  0.5),
	float3(-0.5, -0.5,  0.5), float3( 0.5,  0.5,  0.5), float3(-0.5,  0.5,  0.5),
};

struct VsOut
{
	float4               m_xSvPos       : SV_Position;
	nointerpolation uint m_uInstanceID  : INSTANCE_ID;
};

[shader("vertex")]
VsOut vsMain(uint uVertexID : SV_VertexID, uint uInstanceID : SV_InstanceID)
{
	float3 xLocalCorner = CUBE_CORNERS[uVertexID % 36];
	DecalInstance d = g_xPassSet.DecalBuffer[uInstanceID];

	float4 xWorldPos = mul(d.m_xWorld, float4(xLocalCorner, 1.0));

	VsOut o;
	o.m_xSvPos      = mul(g_xViewSet.g_xView.g_xViewProjMat, xWorldPos);
	o.m_uInstanceID = uInstanceID;
	return o;
}

struct ApplyOut
{
	float4 m_xDiffuse  : SV_Target0;
	float4 m_xNormal   : SV_Target1;
	float4 m_xMaterial : SV_Target2;
};

[shader("fragment")]
ApplyOut fsMain(VsOut input)
{
	float2 uv = input.m_xSvPos.xy * g_xViewSet.g_xView.g_xRcpScreenDims;

	// Sky / background — no scene geometry to project onto.
	float fDepth = g_xPassSet.g_xDepthTex.Sample(uv).r;
	if (fDepth >= 1.0)
		discard;

	float3 xWorldPos = GetWorldPosFromDepthTex(g_xPassSet.g_xDepthTex, uv);

	DecalInstance d = g_xPassSet.DecalBuffer[input.m_uInstanceID];

	// Box test in decal-local cube space.
	float3 xLocal = mul(d.m_xWorldInverse, float4(xWorldPos, 1.0)).xyz;
	if (any(abs(xLocal) > 0.5))
		discard;

	// Normal-alignment test against the cloned (pre-decal) normalsAmbient
	// MRT. Bilinear filtering on neighbouring texels can drift the magnitude;
	// normalize defensively before the dot.
	float3 sceneN        = normalize(g_xPassSet.g_xNormalsCopyTex.Sample(uv).rgb);
	float3 decalAxisWorld = d.m_xAxisOpacity.xyz;
	if (dot(sceneN, decalAxisWorld) < d.m_xParams.x)
		discard;

	// Soft cutoff at the box's XY edge (in decal-local space). 0.4..0.5
	// gives a 0.1-wide fade band.
	float fEdge = 1.0 - smoothstep(0.4, 0.5, max(abs(xLocal.x), abs(xLocal.y)));

	// ===== Mode 1: textured brush indicator (editor terrain-brush cursor) =====
	// Samples the bound brush mask, tints it per tool, and writes DIFFUSE
	// ONLY — alpha 0 on the normal/material MRTs leaves surface shading
	// untouched (an indicator must not stamp a dent or roughness change).
	if (d.m_xParams.y > 0.5)
	{
		float2 xBrushUV = xLocal.xy + 0.5;
		// Per-decal brush index → non-uniform across the instanced draw.
		float4 xBrush = g_xBindlessSet.g_axTextures[NonUniformResourceIndex(asuint(d.m_xParams.z))].Sample(xBrushUV);
		float fBrushAlpha = xBrush.a * d.m_xColour.a * fEdge * d.m_xAxisOpacity.w;
		if (fBrushAlpha <= 0.0)
			discard;

		ApplyOut oBrush;
		oBrush.m_xDiffuse  = float4(xBrush.rgb * d.m_xColour.rgb, fBrushAlpha);
		oBrush.m_xNormal   = float4(0.0, 0.0, 0.0, 0.0);
		oBrush.m_xMaterial = float4(0.0, 0.0, 0.0, 0.0);
		return oBrush;
	}

	// Procedural bullet-hole alpha — fades to 0 at the box's XY radius
	// 0.5, so k_fDecalSize reads as the visible diameter end-to-end.
	float fRadial = length(xLocal.xy);
	float fAlpha  = (1.0 - smoothstep(0.45, 0.5, fRadial)) * fEdge * d.m_xAxisOpacity.w;
	if (fAlpha <= 0.0)
		discard;

	// Decal-space tangent normal — small inward concavity along +Z gives
	// the bullet hole a subtle dent. Then transform to world space using
	// the basis vectors stripped of scale (matrix columns include the
	// per-axis scale factor; normalize to recover unit basis).
	float3 xTangentN = normalize(float3(xLocal.x * 0.4, xLocal.y * 0.4, 1.0));

	float3 xBasisX = normalize(float3(d.m_xWorld[0].x, d.m_xWorld[1].x, d.m_xWorld[2].x));
	float3 xBasisY = normalize(float3(d.m_xWorld[0].y, d.m_xWorld[1].y, d.m_xWorld[2].y));
	float3 xBasisZ = decalAxisWorld;

	float3 xWorldNormal = normalize(
		xBasisX * xTangentN.x +
		xBasisY * xTangentN.y +
		xBasisZ * xTangentN.z);

	const float3 xDecalDiffuse  = float3(0.05, 0.04, 0.03);  // dark scorch
	const float  fDecalRoughness = 0.85;                     // matte
	const float  fDecalMetallic  = 0.0;
	const float  fDecalSpecular  = 0.5;                      // material.b = specular (default 4% F0)

	ApplyOut o;
	o.m_xDiffuse  = float4(xDecalDiffuse,                                       fAlpha);
	o.m_xNormal   = float4(xWorldNormal,                                         fAlpha);
	o.m_xMaterial = float4(fDecalRoughness, fDecalMetallic, fDecalSpecular,      fAlpha);
	return o;
}
