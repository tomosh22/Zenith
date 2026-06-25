// =====================================================================
// Flux_Grass — Slang module.
//
// Subsystem: Vegetation. Per-instance grass blade rendering with wind
// animation, distance-based fade, simple lit shading + translucency,
// and several debug visualisations. The blade quad is bent quadratically
// along height (h²) and modulated by per-instance scale, rotation, and
// world-space wind from CalculateWind below.
//
// Wind helpers ported inline from Flux_Wind.fxh (only the simple
// CalculateWind variant is used by grass; the turbulent / tree variants
// can be extracted to Common.Wind once another consumer needs them).
//
// Output is forward (RGBA blended), not G-buffer — grass writes directly
// to the HDR scene target and self-shades via translucency rather than
// participating in deferred lighting.
// =====================================================================

#include "Common/Bindings.slang"

struct GrassConstantsLayout
{
	float4 g_xWindParams;     // XY = direction, Z = strength, W = time
	float4 g_xGrassParams;    // X = density scale, Y = max distance, Z = debug mode, W = pad
	float4 g_xLODDistances;   // LOD0, LOD1, LOD2, MAX distances (linear, not squared)
};

struct GrassBladeInstance
{
	float3 m_xPosition;
	float  m_fRotation;
	float  m_fHeight;
	float  m_fWidth;
	float  m_fBend;
	uint   m_uColorTint;
};

// Pass-frequency resources (set 3). The structured buffer is read-only in the
// vertex stage — Slang's NonWritable decoration is required when
// vertexPipelineStoresAndAtomics is off.
struct PassParams
{
	ConstantBuffer<GrassConstantsLayout> GrassConstants;
	StructuredBuffer<GrassBladeInstance> InstanceBuffer;
};
ParameterBlock<PassParams> g_xPassSet;

struct VsIn
{
	float3 a_xPosition : POSITION;
	float2 a_xUV       : TEXCOORD0;
};

struct VsOut
{
	float4 position   : SV_Position;
	float3 worldPos   : TEXCOORD0;
	float3 normal     : TEXCOORD1;
	float2 uv         : TEXCOORD2;
	float4 colour     : TEXCOORD3;
	nointerpolation uint instanceId : TEXCOORD4;
};

float4 UnpackColor(uint uPacked)
{
	return float4(
		(float)((uPacked >> 0)  & 0xFFu) / 255.0,
		(float)((uPacked >> 8)  & 0xFFu) / 255.0,
		(float)((uPacked >> 16) & 0xFFu) / 255.0,
		(float)((uPacked >> 24) & 0xFFu) / 255.0);
}

// Y-axis rotation matrix (column-major to match the GLSL constructor:
// the GLSL `mat3(c,0,-s, 0,1,0, s,0,c)` writes columns; this Slang
// constructor takes the same nine elements but in row-major layout, so
// the result is the transpose. For a planar Y rotation that's just the
// inverse rotation — not what we want. Use explicit columns:
//   col0 = (c, 0, s)  col1 = (0, 1, 0)  col2 = (-s, 0, c)
// Slang's float3x3 constructor takes row-major elements, so to build a
// column-major matrix we transpose the literal.
float3x3 RotateY(float fAngle)
{
	float c = cos(fAngle);
	float s = sin(fAngle);
	return float3x3(c, 0.0, -s,
					0.0, 1.0, 0.0,
					s, 0.0,  c);
}

// Layered-wave wind displacement (large + medium + small scales) plus a
// quadratic gust modulator. World-space input — apply by adding to the
// world XZ position weighted by height (more deflection at the tip).
float2 CalculateWind(float2 xWorldPos, float fTime, float2 xWindDir, float fStrength)
{
	float fWave1 = sin(dot(xWorldPos, xWindDir)       * 0.1  + fTime * 1.5);
	float fWave2 = sin(dot(xWorldPos, xWindDir * 1.3) * 0.25 + fTime * 2.3) * 0.5;
	float fWave3 = sin(dot(xWorldPos, xWindDir * 0.7) * 0.4  + fTime * 4.1) * 0.25;

	float fGustPhase = dot(xWorldPos, xWindDir) * 0.02 + fTime * 0.3;
	float fGust      = pow(sin(fGustPhase) * 0.5 + 0.5, 2.0);

	float fCombined = (fWave1 * 0.5 + fWave2 + fWave3) * 0.333;
	fCombined      *= (1.0 + fGust * 0.5);
	return xWindDir * fStrength * fCombined;
}

[shader("vertex")]
VsOut vsMain(VsIn input, uint instanceId : SV_InstanceID)
{
	VsOut o;

	GrassBladeInstance xInstance = g_xPassSet.InstanceBuffer[instanceId];

	// Local-space blade: scale × bend along height² × Y-axis rotation.
	float3 xLocalPos = input.a_xPosition;
	xLocalPos.x *= xInstance.m_fWidth;
	xLocalPos.y *= xInstance.m_fHeight;

	float fHeightFactor = input.a_xUV.y; // 0 base → 1 tip
	xLocalPos.z += xInstance.m_fBend * fHeightFactor * fHeightFactor;

	float3x3 xRotation = RotateY(xInstance.m_fRotation);
	xLocalPos = mul(xRotation, xLocalPos);

	// Wind in world XZ space, weighted by height² so base stays anchored.
	float2 xWindDir      = normalize(g_xPassSet.GrassConstants.g_xWindParams.xy);
	float  fWindStrength = g_xPassSet.GrassConstants.g_xWindParams.z;
	float  fTime         = g_xPassSet.GrassConstants.g_xWindParams.w;
	float2 xWindOffset   = CalculateWind(xInstance.m_xPosition.xz, fTime, xWindDir, fWindStrength);

	float3 xWorldPos = xInstance.m_xPosition + xLocalPos;
	xWorldPos.xz    += xWindOffset * fHeightFactor * fHeightFactor;

	// Tangent direction along blade (derivative of bend + wind in height).
	float  fBendTangentZ = 2.0 * xInstance.m_fBend * fHeightFactor;
	float2 fWindTangentXZ = 2.0 * xWindOffset       * fHeightFactor;

	float3 xLocalTangent = normalize(float3(0.0, 1.0, fBendTangentZ));
	float3 xWorldTangent = mul(xRotation, xLocalTangent);
	xWorldTangent.xz    += fWindTangentXZ;
	xWorldTangent        = normalize(xWorldTangent);

	// Width direction in world (X axis after rotation) → normal = width × tangent.
	float3 xWidthDir = mul(xRotation, float3(1.0, 0.0, 0.0));
	float3 xNormal   = normalize(cross(xWidthDir, xWorldTangent));

	// Distance-fade alpha for the LOD3→max-distance transition.
	float fDistToCamera = length(xWorldPos - g_xViewSet.g_xView.g_xCamPos_Pad.xyz);
	float fFade         = 1.0 - smoothstep(g_xPassSet.GrassConstants.g_xLODDistances.z,
											g_xPassSet.GrassConstants.g_xLODDistances.w,
											fDistToCamera);

	float4 xColorTint = UnpackColor(xInstance.m_uColorTint);

	o.worldPos   = xWorldPos;
	o.normal     = xNormal;
	o.uv         = input.a_xUV;
	o.colour     = float4(xColorTint.rgb, fFade);
	o.instanceId = instanceId;
	o.position   = mul(g_xViewSet.g_xView.g_xViewProjMat, float4(xWorldPos, 1.0));
	return o;
}

static const uint GRASS_DEBUG_NONE             = 0u;
static const uint GRASS_DEBUG_LOD_COLORS       = 1u;
static const uint GRASS_DEBUG_DENSITY_HEAT    = 3u;
static const uint GRASS_DEBUG_WIND_VECTORS    = 4u;
static const uint GRASS_DEBUG_HEIGHT_VARIATION = 7u;

float3 DebugHeatmap(float fValue)
{
	fValue = saturate(fValue);
	if (fValue < 0.5) return lerp(float3(0, 0, 1), float3(0, 1, 0), fValue * 2.0);
	return lerp(float3(0, 1, 0), float3(1, 0, 0), (fValue - 0.5) * 2.0);
}

float3 LODColor(uint uLOD)
{
	if (uLOD == 0u) return float3(0, 1, 0);
	if (uLOD == 1u) return float3(1, 1, 0);
	if (uLOD == 2u) return float3(1, 0.5, 0);
	return float3(1, 0, 0);
}

[shader("fragment")]
float4 fsMain(VsOut input) : SV_Target
{
	uint uDebugMode = (uint)g_xPassSet.GrassConstants.g_xGrassParams.z;

	// Distance-fade alpha test.
	float fAlpha = input.colour.a;
	if (fAlpha < 0.01) discard;

	// Tip-brightening blade base colour with per-instance tint.
	float3 xBase = float3(0.2, 0.5, 0.15);
	float3 xTip  = float3(0.4, 0.7, 0.25);
	float  fHeightFactor = input.uv.y;
	float3 xGrassColor = lerp(xBase, xTip, fHeightFactor * 0.7);
	xGrassColor *= input.colour.rgb;

	// Wrap lighting (no harsh shadows on grass) plus translucency for
	// blades back-lit by the sun. Ambient floor of 0.2 keeps shaded
	// blades visible.
	float3 xSunDir   = normalize(g_xGlobalSet.g_xGlobal.g_xSunDir_Pad.xyz);
	float3 xSunColor = g_xGlobalSet.g_xGlobal.g_xSunColour_Pad.rgb;
	float  fNdotL    = dot(input.normal, xSunDir) * 0.5 + 0.5;
	fNdotL           = fNdotL * 0.8 + 0.2;
	float  fTrans    = max(0.0, -dot(normalize(input.normal), xSunDir)) * fHeightFactor * 0.5;

	float3 xLit = xGrassColor * xSunColor * (fNdotL + fTrans);
	xLit       += float3(0.3, 0.4, 0.5) * 0.2 * xGrassColor; // sky ambient

	if (uDebugMode != GRASS_DEBUG_NONE)
	{
		switch (uDebugMode)
		{
			case GRASS_DEBUG_LOD_COLORS:
			{
				// Match CPU LOD selection: < threshold² → smaller LOD.
				float3 xToCam = input.worldPos - g_xViewSet.g_xView.g_xCamPos_Pad.xyz;
				float  fDistSq = dot(xToCam, xToCam);
				float  fLOD0Sq = g_xPassSet.GrassConstants.g_xLODDistances.x * g_xPassSet.GrassConstants.g_xLODDistances.x;
				float  fLOD1Sq = g_xPassSet.GrassConstants.g_xLODDistances.y * g_xPassSet.GrassConstants.g_xLODDistances.y;
				float  fLOD2Sq = g_xPassSet.GrassConstants.g_xLODDistances.z * g_xPassSet.GrassConstants.g_xLODDistances.z;
				uint   uLOD    = 3u;
				if (fDistSq < fLOD2Sq) uLOD = 2u;
				if (fDistSq < fLOD1Sq) uLOD = 1u;
				if (fDistSq < fLOD0Sq) uLOD = 0u;
				xLit = LODColor(uLOD);
				break;
			}
			case GRASS_DEBUG_HEIGHT_VARIATION:
				xLit = DebugHeatmap(fHeightFactor);
				break;
		}
	}

	return float4(xLit, fAlpha);
}
