// =====================================================================
// Flux_DynamicLights — Slang module.
//
// Subsystem: DynamicLights. Deferred light volume rasterisation. Vertex
// shader scales/orients an instance volume (sphere for point, cone for
// spot, fullscreen for directional) and the fragment runs Cook-Torrance
// BRDF against the G-buffer with multi-scatter energy compensation.
//
// Imports Common.Frame for FrameConstants (camera, view×proj, recip
// screen dims) and Common.PBR for the shared BRDF helpers.
//
// Bindings (set 0):
//   0   FrameConstants
//   1   PointLightBuffer        (StructuredBuffer<PointLightInstance>)
//   2   g_xDiffuseTex
//   3   g_xNormalsAmbientTex
//   4   g_xMaterialTex
//   5   g_xDepthTex
//   6   SpotLightBuffer
//   7   DirectionalLightBuffer
//   8   PushConstants            (CB selecting light type)
//   9   g_xBRDFLUT
// =====================================================================

import Common.Frame;
import Common.PBR;

static const uint LIGHT_TYPE_POINT       = 0u;
static const uint LIGHT_TYPE_SPOT        = 1u;
static const uint LIGHT_TYPE_DIRECTIONAL = 2u;

// Numerical guards.
static const float LIGHT_CONTRIBUTION_THRESHOLD = 0.001;
static const float MIN_LIGHT_DISTANCE           = 0.01; // 1 cm minimum distance to avoid 1/d² blowup

struct PointLightInstance
{
	float4 m_xPositionRange;  // xyz position, w range
	float4 m_xColorIntensity; // rgb colour, w intensity
};
struct SpotLightInstance
{
	float4 m_xPositionRange;
	float4 m_xColorIntensity;
	float4 m_xDirectionInner; // xyz dir, w cos(inner)
	float4 m_xSpotOuter;      // x cos(outer), yzw unused
};
struct DirectionalLightInstance
{
	float4 m_xColorIntensity;
	float4 m_xDirectionPad;
};

// Read-only in vertex stage — NonWritable required since
// vertexPipelineStoresAndAtomics is off.
[[vk::binding(1, 0)]] StructuredBuffer<PointLightInstance>       PointLightBuffer;
[[vk::binding(6, 0)]] StructuredBuffer<SpotLightInstance>        SpotLightBuffer;
[[vk::binding(7, 0)]] StructuredBuffer<DirectionalLightInstance> DirectionalLightBuffer;

struct PushConstantsLayout
{
	uint m_uLightType;
	uint m_uPad0;
	uint m_uPad1;
	uint m_uPad2;
};
[[vk::binding(8, 0)]] ConstantBuffer<PushConstantsLayout> PushConstants;

// G-buffer + BRDF LUT (fragment stage).
[[vk::binding(2, 0)]] Sampler2D g_xDiffuseTex;
[[vk::binding(3, 0)]] Sampler2D g_xNormalsAmbientTex;
[[vk::binding(4, 0)]] Sampler2D g_xMaterialTex;
[[vk::binding(5, 0)]] Sampler2D g_xDepthTex;
[[vk::binding(9, 0)]] Sampler2D g_xBRDFLUT;

struct VsOut
{
	float4 position       : SV_Position;
	nointerpolation float4 positionRange  : TEXCOORD0;
	nointerpolation float4 colorIntensity : TEXCOORD1;
	nointerpolation float3 direction      : TEXCOORD2;
	nointerpolation float4 spotParams     : TEXCOORD3;
	nointerpolation uint   lightType      : TEXCOORD4;
};

[shader("vertex")]
VsOut vsMain(float3 a_xPosition : POSITION, uint instanceId : SV_InstanceID)
{
	VsOut o;

	float3   xLightPos;
	float    fRange;
	float4x4 xModelMatrix;

	if (PushConstants.m_uLightType == LIGHT_TYPE_POINT)
	{
		PointLightInstance light = PointLightBuffer[instanceId];

		xLightPos = light.m_xPositionRange.xyz;
		fRange    = light.m_xPositionRange.w;

		// Translate-then-scale (column-major): unit sphere → range sphere
		// at world position.
		xModelMatrix = float4x4(
			fRange,        0.0,           0.0,           xLightPos.x,
			0.0,           fRange,        0.0,           xLightPos.y,
			0.0,           0.0,           fRange,        xLightPos.z,
			0.0,           0.0,           0.0,           1.0);

		o.positionRange  = light.m_xPositionRange;
		o.colorIntensity = light.m_xColorIntensity;
		o.direction      = float3(0.0, 0.0, 0.0);
		o.spotParams     = float4(0.0, 0.0, 0.0, 0.0);
		o.lightType      = LIGHT_TYPE_POINT;
	}
	else if (PushConstants.m_uLightType == LIGHT_TYPE_SPOT)
	{
		SpotLightInstance light = SpotLightBuffer[instanceId];

		xLightPos       = light.m_xPositionRange.xyz;
		fRange          = light.m_xPositionRange.w;
		float3 xDir     = light.m_xDirectionInner.xyz; // already normalised CPU-side
		float  fCosOuter = light.m_xSpotOuter.x;

		// Cone source axis is +Y. Build a Rodrigues rotation from +Y to
		// the light direction. Special-case the colinear cases to avoid
		// degenerate cross products at ±Y.
		float3 xSourceDir = float3(0.0, 1.0, 0.0);
		float  fDot       = dot(xSourceDir, xDir);
		float3x3 xRotation;
		if (fDot > 0.9999)
		{
			xRotation = float3x3(1, 0, 0,
								 0, 1, 0,
								 0, 0, 1);
		}
		else if (fDot < -0.9999)
		{
			// 180° flip — invert Y and Z.
			xRotation = float3x3( 1,  0,  0,
								  0, -1,  0,
								  0,  0, -1);
		}
		else
		{
			float3 xAxis = normalize(cross(xSourceDir, xDir));
			float fAngle = acos(clamp(fDot, -1.0, 1.0));
			float c = cos(fAngle);
			float s = sin(fAngle);
			float t = 1.0 - c;
			// Rodrigues formula. The float3x3 constructor takes elements
			// in row-major order; the GLSL original wrote columns, so the
			// transpose-of-transpose puts the same values in the matching
			// slots (Slang multiplies as M*v with v column-vector).
			xRotation = float3x3(
				t * xAxis.x * xAxis.x + c,           t * xAxis.x * xAxis.y - s * xAxis.z, t * xAxis.x * xAxis.z + s * xAxis.y,
				t * xAxis.x * xAxis.y + s * xAxis.z, t * xAxis.y * xAxis.y + c,           t * xAxis.y * xAxis.z - s * xAxis.x,
				t * xAxis.x * xAxis.z - s * xAxis.y, t * xAxis.y * xAxis.z + s * xAxis.x, t * xAxis.z * xAxis.z + c);
		}

		// Scale: Y = range (cone height), XZ = range·tan(outerAngle).
		// Clamp cos(outer) below 1 to keep the cone non-degenerate.
		float fOuterAngle = acos(clamp(fCosOuter, -1.0, 0.9999));
		float fBaseRadius = max(fRange * tan(fOuterAngle), 0.001);
		float3x3 xScale = float3x3(
			fBaseRadius, 0.0, 0.0,
			0.0, fRange,  0.0,
			0.0, 0.0,    fBaseRadius);
		float3x3 xScaleRotate = mul(xRotation, xScale);

		// Compose into a 4x4 with translation. Slang float4x4 row-major
		// constructor takes 16 values row-by-row; the upper-3x3 columns
		// come from xScaleRotate.
		xModelMatrix = float4x4(
			xScaleRotate._m00, xScaleRotate._m01, xScaleRotate._m02, xLightPos.x,
			xScaleRotate._m10, xScaleRotate._m11, xScaleRotate._m12, xLightPos.y,
			xScaleRotate._m20, xScaleRotate._m21, xScaleRotate._m22, xLightPos.z,
			0.0,               0.0,               0.0,               1.0);

		o.positionRange  = light.m_xPositionRange;
		o.colorIntensity = light.m_xColorIntensity;
		o.direction      = xDir;
		o.spotParams     = float4(light.m_xDirectionInner.w, light.m_xSpotOuter.x, 0.0, 0.0);
		o.lightType      = LIGHT_TYPE_SPOT;
	}
	else // LIGHT_TYPE_DIRECTIONAL
	{
		DirectionalLightInstance light = DirectionalLightBuffer[instanceId];

		// Directional lights use a fullscreen quad — identity model.
		xModelMatrix = float4x4(1, 0, 0, 0,
								0, 1, 0, 0,
								0, 0, 1, 0,
								0, 0, 0, 1);

		o.positionRange  = float4(0.0, 0.0, 0.0, 0.0);
		o.colorIntensity = light.m_xColorIntensity;
		o.direction      = light.m_xDirectionPad.xyz;
		o.spotParams     = float4(0.0, 0.0, 0.0, 0.0);
		o.lightType      = LIGHT_TYPE_DIRECTIONAL;
	}

	float4 xWorldPos = mul(xModelMatrix, float4(a_xPosition, 1.0));
	o.position       = mul(FrameConstants.g_xViewProjMat, xWorldPos);
	return o;
}

// Karis 2013 windowed inverse-square. Flux→irradiance via 1/(4πd²),
// multiplied by saturate(1 - (d/r)⁴)² so the contribution reaches zero
// exactly at the range limit.
float ComputeAttenuation(float fDistance, float fRange)
{
	float d2 = fDistance * fDistance;
	float r2 = fRange    * fRange;
	float fDistanceRatio = d2 / r2;
	float fWindow        = saturate(1.0 - fDistanceRatio * fDistanceRatio);
	fWindow             *= fWindow;
	float fClampedDist  = max(fDistance, MIN_LIGHT_DISTANCE);
	return (1.0 / (4.0 * PI * fClampedDist * fClampedDist)) * fWindow;
}

// Smoothstep penumbra. fCosOuter < fCosInner (outer is the wider angle
// and therefore the smaller cosine).
float ComputeSpotConeFalloff(float3 xLightToFrag, float3 xSpotDir, float fCosInner, float fCosOuter)
{
	float fCosAngle = dot(xLightToFrag, xSpotDir);
	const float fConeEpsilon = 0.001;
	return smoothstep(fCosOuter - fConeEpsilon, fCosInner, fCosAngle);
}

// Cook-Torrance with multi-scatter compensation — exact same formula
// as the deferred shader so dynamic and ambient lights agree on energy.
float3 CookTorrance_DynamicLight(float3 xAlbedo,
								   float3 xNormal,
								   float3 xViewDir,
								   float3 xLightDir,
								   float3 xLightColor,
								   float  fMetallic,
								   float  fRoughness,
								   float  fAttenuation)
{
	float3 xHalfDir = normalize(xLightDir + xViewDir);
	float NdotL = max(dot(xNormal, xLightDir), 0.0);
	float NdotV = max(dot(xNormal, xViewDir),  PBR_EPSILON);
	float NdotH = max(dot(xNormal, xHalfDir),  0.0);
	float HdotV = max(dot(xHalfDir, xViewDir), 0.0);
	if (NdotL <= PBR_EPSILON) return float3(0.0, 0.0, 0.0);

	float fClampedRoughness = clamp(fRoughness, PBR_MIN_ROUGHNESS, 1.0);
	float3 F0 = lerp(PBR_DIELECTRIC_F0, xAlbedo, fMetallic);

	float  D = DistributionGGX(NdotH, fClampedRoughness);
	float  G = GeometrySmith_Direct(NdotV, NdotL, fClampedRoughness);
	float3 F = FresnelSchlick(HdotV, F0);

	float3 numerator   = D * G * F;
	float  denom       = 4.0 * NdotV * NdotL;
	float3 specular    = numerator / max(denom, PBR_EPSILON);

	// Multi-scatter compensation — keeps dynamic-light brightness in
	// line with IBL on rough surfaces. Same recipe as the deferred
	// shader's ComputeIBLAmbient.
	float fRoughnessForLUT = clamp(fClampedRoughness, PBR_BRDF_LUT_MIN_ROUGHNESS, PBR_BRDF_LUT_MAX_ROUGHNESS);
	float2 xBRDF = g_xBRDFLUT.Sample(float2(NdotV, fRoughnessForLUT)).rg;
	float3 xMulti = MultiscatterBRDF(F0, NdotV, fRoughnessForLUT, xBRDF);
	float3 xSingle = F0 * xBRDF.x + xBRDF.y;
	specular *= xMulti / max(xSingle, float3(PBR_EPSILON, PBR_EPSILON, PBR_EPSILON));

	float3 kS = F;
	float3 kD = (1.0 - kS) * (1.0 - fMetallic);

	float3 diffuse  = kD * xAlbedo / PI;
	float3 radiance = xLightColor * fAttenuation;
	return (diffuse + specular) * radiance * NdotL;
}

[shader("fragment")]
float4 fsMain(VsOut input, float4 fragCoord : SV_Position) : SV_Target
{
	float2 xUV = fragCoord.xy * FrameConstants.g_xRcpScreenDims;

	float fDepth = g_xDepthTex.Sample(xUV).r;
	if (fDepth >= 1.0) discard; // sky pixel

	float3 xWorldPos = GetWorldPosFromDepthTex(g_xDepthTex, xUV);

	float4 xDiffuse       = g_xDiffuseTex.Sample(xUV);
	float4 xNormalAmbient = g_xNormalsAmbientTex.Sample(xUV);
	float4 xMaterial      = g_xMaterialTex.Sample(xUV);
	float3 xNormal        = normalize(xNormalAmbient.xyz);
	float  fRoughness     = xMaterial.x;
	float  fMetallic      = xMaterial.y;
	float3 xViewDir       = normalize(FrameConstants.g_xCamPos_Pad.xyz - xWorldPos);

	float3 xLightPos      = input.positionRange.xyz;
	float  fRange         = input.positionRange.w;
	float3 xLightColor    = input.colorIntensity.xyz;
	float  fIntensity     = input.colorIntensity.w;
	float3 xLightDirStored = input.direction;
	uint   uLightType     = input.lightType;

	float3 xLightDir;
	float  fAttenuation = fIntensity;

	if (uLightType == LIGHT_TYPE_POINT)
	{
		float3 xToLight = xLightPos - xWorldPos;
		float  fDistance = length(xToLight);
		// Light volume should already cull this, but the discard is cheap.
		if (fDistance > fRange) discard;
		xLightDir    = xToLight / max(fDistance, MIN_LIGHT_DISTANCE);
		fAttenuation *= ComputeAttenuation(fDistance, fRange);
	}
	else if (uLightType == LIGHT_TYPE_SPOT)
	{
		float3 xToLight = xLightPos - xWorldPos;
		float  fDistance = length(xToLight);
		if (fDistance > fRange) discard;
		xLightDir    = xToLight / max(fDistance, MIN_LIGHT_DISTANCE);

		float fCosInner = input.spotParams.x;
		float fCosOuter = input.spotParams.y;
		fAttenuation *= ComputeAttenuation(fDistance, fRange);

		// xLightDirStored points FROM light INTO scene; cone test wants
		// light-to-fragment so we negate xLightDir.
		float3 xLightToFrag = -xLightDir;
		fAttenuation *= ComputeSpotConeFalloff(xLightToFrag, xLightDirStored, fCosInner, fCosOuter);
	}
	else // LIGHT_TYPE_DIRECTIONAL
	{
		// Direction stored is light-to-scene; BRDF wants fragment-to-light → negate.
		xLightDir = -xLightDirStored;
		// No distance falloff — directional intensity is already lux.
	}

	if (fAttenuation < LIGHT_CONTRIBUTION_THRESHOLD) discard;

	float3 xResult = CookTorrance_DynamicLight(
		xDiffuse.rgb, xNormal, xViewDir, xLightDir, xLightColor,
		fMetallic, fRoughness, fAttenuation);

	// Alpha 0 — additive blend onto the HDR scene target.
	return float4(xResult, 0.0);
}
