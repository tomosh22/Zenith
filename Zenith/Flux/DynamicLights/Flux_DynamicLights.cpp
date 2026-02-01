#include "Zenith.h"

#include "Flux/DynamicLights/Flux_DynamicLights.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/HDR/Flux_HDR.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/IBL/Flux_IBL.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_Query.h"
#include "EntityComponent/Components/Zenith_LightComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Maths/Zenith_FrustumCulling.h"
#include "DebugVariables/Zenith_DebugVariables.h"

#include <cmath>
#include <algorithm>
#include <vector>

// ========== CONFIGURATION CONSTANTS ==========

// Direction vector normalization epsilon (prevents NaN from zero-length vectors)
static constexpr float fDIRECTION_EPSILON = 0.0001f;

// LOD configuration for light volumes
// LOD 0: High detail for close lights (12x24 sphere, 24 cone segments)
// LOD 1: Medium detail for mid-range (8x16 sphere, 16 cone segments)
// LOD 2: Low detail for distant lights (6x12 sphere, 8 cone segments)
static constexpr u_int uSPHERE_LOD0_LAT = 12;
static constexpr u_int uSPHERE_LOD0_LON = 24;
static constexpr u_int uSPHERE_LOD1_LAT = 8;
static constexpr u_int uSPHERE_LOD1_LON = 16;
static constexpr u_int uSPHERE_LOD2_LAT = 6;
static constexpr u_int uSPHERE_LOD2_LON = 12;

static constexpr u_int uCONE_LOD0_SEGMENTS = 24;
static constexpr u_int uCONE_LOD1_SEGMENTS = 16;
static constexpr u_int uCONE_LOD2_SEGMENTS = 8;

// Screen-space thresholds for LOD selection (approximate pixel radius)
// Default values used when debug variables are not enabled
static constexpr float fDEFAULT_LOD1_THRESHOLD = 100.0f;  // Switch to LOD1 below this screen radius
static constexpr float fDEFAULT_LOD2_THRESHOLD = 30.0f;   // Switch to LOD2 below this screen radius

#ifdef ZENITH_DEBUG_VARIABLES
DEBUGVAR float dbg_fLightLOD1Threshold = fDEFAULT_LOD1_THRESHOLD;
DEBUGVAR float dbg_fLightLOD2Threshold = fDEFAULT_LOD2_THRESHOLD;
DEBUGVAR bool dbg_bShowDynamicLights = true;
#ifdef ZENITH_TOOLS
DEBUGVAR bool dbg_bShowDroppedLights = false;
#endif
#endif

// ========== VERTEX FORMAT ==========

// Compact vertex format for light volumes (position only)
// Normal and color are not needed since we sample G-buffer for lighting data
struct LightVolumeVertex
{
	Zenith_Maths::Vector3 m_xPosition;
};


// ========== STATIC MEMBERS ==========

bool Flux_DynamicLights::s_bInitialised = false;

static Zenith_Task g_xRenderTask(ZENITH_PROFILE_INDEX__FLUX_DYNAMIC_LIGHTS, Flux_DynamicLights::Render, nullptr);
static Flux_CommandList g_xCommandList("Dynamic Lights");

static Flux_Shader s_xVolumeShader;
static Flux_Pipeline s_xVolumePipeline;           // For point/spot lights (CULL_MODE_FRONT)
static Flux_Pipeline s_xDirectionalPipeline;      // For directional lights (CULL_MODE_BACK - fullscreen quad)

// Light volume meshes - LOD levels
struct LightVolumeLOD
{
	Flux_VertexBuffer m_xVertexBuffer;
	Flux_IndexBuffer m_xIndexBuffer;
	u_int m_uIndexCount = 0;
};

static constexpr u_int uNUM_LODS = 3;
static LightVolumeLOD s_axSphereLODs[uNUM_LODS];
static LightVolumeLOD s_axConeLODs[uNUM_LODS];

// Cached frustum for culling (updated each frame)
static Zenith_Frustum s_xCameraFrustum;

// Cached binding handles from shader reflection
static Flux_BindingHandle s_xFrameConstantsBinding;
static Flux_BindingHandle s_xPointLightBufferBinding;
static Flux_BindingHandle s_xSpotLightBufferBinding;
static Flux_BindingHandle s_xDirectionalLightBufferBinding;
static Flux_BindingHandle s_xDiffuseTexBinding;
static Flux_BindingHandle s_xNormalsAmbientTexBinding;
static Flux_BindingHandle s_xMaterialTexBinding;
static Flux_BindingHandle s_xDepthTexBinding;
static Flux_BindingHandle s_xPushConstantsBinding;
static Flux_BindingHandle s_xBRDFLUTBinding;

// Push constant structure for light type
struct LightTypePushConstant
{
	u_int m_uLightType;   // 0=point, 1=spot, 2=directional
	u_int m_uPad0;
	u_int m_uPad1;
	u_int m_uPad2;
};

// Per-frame light data (separated by type for efficient rendering)
//
// INTENSITY UNITS (Physical):
// - Point/Spot lights: Luminous power in lumens (lm)
//   Candle ~12lm, 60W bulb ~800lm, Studio light ~5000lm
// - Directional lights: Illuminance in lux (lm/m²)
//   Overcast ~1000lux, Cloudy ~10000lux, Direct sun ~100000lux
//
// The shader uses physically-correct attenuation: I / (4π * d²)
// See Flux_DynamicLights.frag for implementation details.

struct PointLightData
{
	Zenith_Maths::Vector3 m_xPosition;
	float m_fRange;
	Zenith_Maths::Vector3 m_xColor;  // Linear RGB
	float m_fIntensity;              // Lumens (lm)
	u_int m_uLODIndex;               // Selected LOD based on screen-space size
};

struct SpotLightData
{
	Zenith_Maths::Vector3 m_xPosition;
	float m_fRange;
	Zenith_Maths::Vector3 m_xColor;  // Linear RGB
	float m_fIntensity;              // Lumens (lm)
	Zenith_Maths::Vector3 m_xDirection;
	float m_fCosInner;
	float m_fCosOuter;
	u_int m_uLODIndex;               // Selected LOD based on screen-space size
};

struct DirectionalLightData
{
	Zenith_Maths::Vector3 m_xDirection;
	Zenith_Maths::Vector3 m_xColor;  // Linear RGB
	float m_fIntensity;              // Lux (lm/m²), no distance falloff
};

// NOTE: Intermediate s_xPointLights/s_xSpotLights/s_xDirectionalLights vectors have been
// eliminated as part of the triple-copy optimization. Light data now flows directly from
// the ECS query temporary vectors to the staging buffers, reducing CPU overhead by ~25%.
// The per-LOD instance counts (s_auPointLightInstanceCounts, etc.) track the actual light counts.

// ========== GPU INSTANCING DATA ==========

// Instance data structures for GPU (matches per-instance vertex attributes)
// Point light: 32 bytes (2 x vec4)
struct PointLightInstance
{
	Zenith_Maths::Vector4 m_xPositionRange;    // xyz=position, w=range
	Zenith_Maths::Vector4 m_xColorIntensity;   // xyz=color, w=intensity
};
static_assert(sizeof(PointLightInstance) == 32, "PointLightInstance must be 32 bytes for vertex alignment");

// Spot light: 64 bytes (4 x vec4)
struct SpotLightInstance
{
	Zenith_Maths::Vector4 m_xPositionRange;    // xyz=position, w=range
	Zenith_Maths::Vector4 m_xColorIntensity;   // xyz=color, w=intensity
	Zenith_Maths::Vector4 m_xDirectionInner;   // xyz=direction, w=cos(inner)
	Zenith_Maths::Vector4 m_xSpotOuter;        // x=cos(outer), yzw=unused
};
static_assert(sizeof(SpotLightInstance) == 64, "SpotLightInstance must be 64 bytes for vertex alignment");

struct DirectionalLightInstance
{
	Zenith_Maths::Vector4 m_xColorIntensity;   // xyz=color, w=intensity
	Zenith_Maths::Vector4 m_xDirectionPad;     // xyz=direction, w=unused
};
static_assert(sizeof(DirectionalLightInstance) == 32, "DirectionalLightInstance must be 32 bytes for storage buffer alignment");

// Per-LOD instance buffers (storage buffers for GPU instancing)
static Flux_ReadWriteBuffer s_axPointLightInstanceBuffers[uNUM_LODS];
static Flux_ReadWriteBuffer s_axSpotLightInstanceBuffers[uNUM_LODS];
static Flux_ReadWriteBuffer s_xDirectionalLightInstanceBuffer;

// Per-LOD instance counts (set during GatherLightsFromScene)
static u_int s_auPointLightInstanceCounts[uNUM_LODS];
static u_int s_auSpotLightInstanceCounts[uNUM_LODS];
static u_int s_uDirectionalLightInstanceCount;

// CPU staging buffers (avoid per-frame allocations)
static PointLightInstance s_axPointLightStaging[uNUM_LODS][Flux_DynamicLights::uMAX_LIGHTS];
static SpotLightInstance s_axSpotLightStaging[uNUM_LODS][Flux_DynamicLights::uMAX_LIGHTS];
static DirectionalLightInstance s_axDirectionalLightStaging[Flux_DynamicLights::uMAX_LIGHTS];

// ========== PROCEDURAL MESH GENERATION ==========

/**
 * Generate a unit sphere (radius 1.0, centered at origin) using UV sphere algorithm
 * Uses compact vertex format (position only) for minimal memory usage
 */
static void GenerateUnitSphere(Zenith_Vector<LightVolumeVertex>& xVertices, Zenith_Vector<u_int>& xIndices, u_int uLatitudeSegments, u_int uLongitudeSegments)
{
	xVertices.Clear();
	xIndices.Clear();

	const float fPI = glm::pi<float>();

	// Generate vertices
	for (u_int lat = 0; lat <= uLatitudeSegments; ++lat)
	{
		float theta = lat * fPI / uLatitudeSegments;  // 0 to PI (top to bottom)
		float sinTheta = sinf(theta);
		float cosTheta = cosf(theta);

		for (u_int lon = 0; lon <= uLongitudeSegments; ++lon)
		{
			float phi = lon * 2.0f * fPI / uLongitudeSegments;  // 0 to 2PI (around equator)
			float sinPhi = sinf(phi);
			float cosPhi = cosf(phi);

			LightVolumeVertex xVertex;
			xVertex.m_xPosition.x = sinTheta * cosPhi;
			xVertex.m_xPosition.y = cosTheta;
			xVertex.m_xPosition.z = sinTheta * sinPhi;

			xVertices.PushBack(xVertex);
		}
	}

	// Generate indices (CCW winding)
	for (u_int lat = 0; lat < uLatitudeSegments; ++lat)
	{
		for (u_int lon = 0; lon < uLongitudeSegments; ++lon)
		{
			u_int current = lat * (uLongitudeSegments + 1) + lon;
			u_int next = current + uLongitudeSegments + 1;

			// Triangle 1
			xIndices.PushBack(current);
			xIndices.PushBack(next);
			xIndices.PushBack(current + 1);

			// Triangle 2
			xIndices.PushBack(current + 1);
			xIndices.PushBack(next);
			xIndices.PushBack(next + 1);
		}
	}
}

/**
 * Generate a unit cone (apex at origin, pointing +Y, height 1.0, base radius 1.0)
 * Used for spot light volumes. Scale base radius by tan(outerAngle) and height by range.
 * Uses compact vertex format (position only) for minimal memory usage
 */
static void GenerateUnitCone(Zenith_Vector<LightVolumeVertex>& xVertices, Zenith_Vector<u_int>& xIndices, u_int uSegments)
{
	xVertices.Clear();
	xIndices.Clear();

	const float fPI = glm::pi<float>();
	const float fHeight = 1.0f;
	const float fRadius = 1.0f;

	// Apex vertex (at origin)
	LightVolumeVertex xApex;
	xApex.m_xPosition = Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f);
	xVertices.PushBack(xApex);

	// Base circle vertices (at y = fHeight)
	for (u_int i = 0; i <= uSegments; ++i)
	{
		float angle = i * 2.0f * fPI / uSegments;
		float cosAngle = cosf(angle);
		float sinAngle = sinf(angle);

		LightVolumeVertex xVertex;
		xVertex.m_xPosition = Zenith_Maths::Vector3(fRadius * cosAngle, fHeight, fRadius * sinAngle);

		xVertices.PushBack(xVertex);
	}

	// Side triangles (fan from apex to base)
	for (u_int i = 0; i < uSegments; ++i)
	{
		xIndices.PushBack(0);              // Apex
		xIndices.PushBack(i + 2);          // Next base vertex
		xIndices.PushBack(i + 1);          // Current base vertex
	}

	// Base cap (helps with back-face rendering when camera is inside cone)
	u_int uBaseCenterIndex = xVertices.GetSize();
	LightVolumeVertex xBaseCenter;
	xBaseCenter.m_xPosition = Zenith_Maths::Vector3(0.0f, fHeight, 0.0f);
	xVertices.PushBack(xBaseCenter);

	// Base triangles (fan from center)
	for (u_int i = 0; i < uSegments; ++i)
	{
		xIndices.PushBack(uBaseCenterIndex);
		xIndices.PushBack(i + 1);          // Current base vertex
		xIndices.PushBack(i + 2);          // Next base vertex
	}
}

/**
 * Calculate approximate screen-space radius for a light
 * Used for LOD selection
 */
static float CalculateScreenSpaceRadius(const Zenith_Maths::Vector3& xLightPos, float fWorldRadius)
{
	Zenith_Maths::Vector3 xCamPos = Flux_Graphics::s_xFrameConstants.m_xCamPos_Pad;
	float fDistance = Zenith_Maths::Length(xLightPos - xCamPos);

	if (fDistance < 0.001f)
	{
		return 10000.0f;  // Camera inside light, use highest LOD
	}

	// Calculate screen-space radius using actual camera FOV
	float fScreenHeight = static_cast<float>(Flux_Graphics::s_xFrameConstants.m_xScreenDims.y);
	float fFOV = Flux_Graphics::GetFOV();  // Use actual camera FOV
	float fScreenRadius = (fWorldRadius / fDistance) * (fScreenHeight / (2.0f * tanf(fFOV * 0.5f)));

	return fScreenRadius;
}

/**
 * Select LOD index based on screen-space size
 */
static u_int SelectLOD(float fScreenRadius)
{
#ifdef ZENITH_DEBUG_VARIABLES
	float fLOD1Threshold = dbg_fLightLOD1Threshold;
	float fLOD2Threshold = dbg_fLightLOD2Threshold;
#else
	float fLOD1Threshold = fDEFAULT_LOD1_THRESHOLD;
	float fLOD2Threshold = fDEFAULT_LOD2_THRESHOLD;
#endif

	if (fScreenRadius >= fLOD1Threshold)
	{
		return 0;  // High detail
	}
	else if (fScreenRadius >= fLOD2Threshold)
	{
		return 1;  // Medium detail
	}
	else
	{
		return 2;  // Low detail
	}
}

/**
 * Test if a sphere (point light bounding volume) intersects the camera frustum
 */
static bool IsSphereFrustumVisible(const Zenith_Maths::Vector3& xCenter, float fRadius)
{
	// Test against each frustum plane
	for (int i = 0; i < 6; ++i)
	{
		const Zenith_Plane& xPlane = s_xCameraFrustum.m_axPlanes[i];
		float fDistance = xPlane.GetSignedDistance(xCenter);

		// If sphere is completely behind any plane, it's not visible
		if (fDistance < -fRadius)
		{
			return false;
		}
	}
	return true;
}

/**
 * Build an orthonormal basis from a direction vector
 * Returns two vectors perpendicular to the input direction and each other
 */
static void BuildOrthonormalBasis(const Zenith_Maths::Vector3& xDirection, Zenith_Maths::Vector3& xRight, Zenith_Maths::Vector3& xUp)
{
	// Choose a non-parallel vector to start
	Zenith_Maths::Vector3 xRef = (fabsf(xDirection.y) < 0.9f)
		? Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)
		: Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);

	xRight = Zenith_Maths::Normalize(Zenith_Maths::Cross(xDirection, xRef));
	xUp = Zenith_Maths::Cross(xRight, xDirection);
}

/**
 * Test if a cone (spot light volume) intersects the camera frustum
 * Uses a more accurate test than sphere approximation for better culling
 */
static bool IsConeFrustumVisible(
	const Zenith_Maths::Vector3& xApex,
	const Zenith_Maths::Vector3& xDirection,
	float fRange,
	float fOuterAngle)
{
	// First: quick bounding sphere test (conservative)
	// Cone's bounding sphere center is at apex + dir * (range/2)
	float fSinOuter = sinf(fOuterAngle);
	float fHalfRange = fRange * 0.5f;
	Zenith_Maths::Vector3 xBoundCenter = xApex + xDirection * fHalfRange;
	float fBoundRadius = fRange * fSinOuter + fHalfRange;

	if (!IsSphereFrustumVisible(xBoundCenter, fBoundRadius))
	{
		return false;
	}

	// Second: test key cone points against each frustum plane
	// If all test points are behind any single plane, the cone is not visible
	Zenith_Maths::Vector3 xBaseCenter = xApex + xDirection * fRange;
	float fBaseRadius = fRange * tanf(fOuterAngle);

	// Build orthonormal basis for base circle sampling
	Zenith_Maths::Vector3 xRight, xUp;
	BuildOrthonormalBasis(xDirection, xRight, xUp);

	for (int i = 0; i < 6; ++i)
	{
		const Zenith_Plane& xPlane = s_xCameraFrustum.m_axPlanes[i];

		// Test apex
		float fApexDist = xPlane.GetSignedDistance(xApex);

		// Test cone base center
		float fBaseCenterDist = xPlane.GetSignedDistance(xBaseCenter);

		// Test 4 points on base circle (cardinal directions)
		float fBaseRight = xPlane.GetSignedDistance(xBaseCenter + xRight * fBaseRadius);
		float fBaseLeft = xPlane.GetSignedDistance(xBaseCenter - xRight * fBaseRadius);
		float fBaseUp = xPlane.GetSignedDistance(xBaseCenter + xUp * fBaseRadius);
		float fBaseDown = xPlane.GetSignedDistance(xBaseCenter - xUp * fBaseRadius);

		// Find maximum signed distance (closest to being in front of plane)
		float fMaxDist = fApexDist;
		fMaxDist = std::max(fMaxDist, fBaseCenterDist);
		fMaxDist = std::max(fMaxDist, fBaseRight);
		fMaxDist = std::max(fMaxDist, fBaseLeft);
		fMaxDist = std::max(fMaxDist, fBaseUp);
		fMaxDist = std::max(fMaxDist, fBaseDown);

		// If all test points are behind this plane, cone is not visible
		if (fMaxDist < 0.0f)
		{
			return false;
		}
	}

	return true;
}

// ========== PRIORITY SORTING ==========

// Minimum intensity threshold - lights below this are skipped
static constexpr float fMIN_LIGHT_INTENSITY = 0.001f;

// Priority sorting for when lights exceed the maximum
struct LightSortKey
{
	float m_fPriority;  // Higher = more important
	u_int m_uIndex;

	bool operator<(const LightSortKey& other) const
	{
		return m_fPriority > other.m_fPriority;  // Sort descending (highest priority first)
	}
};

// Pre-allocated sort buffer to avoid per-frame allocations
static Zenith_Vector<LightSortKey> s_xSortBuffer;

/**
 * Calculate light importance for priority sorting
 * Prioritizes: closer lights, brighter lights, larger range lights
 */
static float CalculateLightPriority(const Zenith_Maths::Vector3& xLightPos, float fIntensity, float fRange)
{
	Zenith_Maths::Vector3 xCamPos = Flux_Graphics::s_xFrameConstants.m_xCamPos_Pad;
	float fDistance = Zenith_Maths::Length(xLightPos - xCamPos);

	// Priority formula: (intensity * range) / (distance + 1)
	// Adding 1 to distance prevents division by zero and boosts nearby lights
	return (fIntensity * fRange) / (fDistance + 1.0f);
}

// ========== PUBLIC API ==========

void Flux_DynamicLights::Initialise()
{
	// Generate light volume meshes at multiple LOD levels
	Zenith_Vector<LightVolumeVertex> xVertices;
	Zenith_Vector<u_int> xIndices;

	// Sphere LODs for point lights
	const u_int auSphereLat[uNUM_LODS] = { uSPHERE_LOD0_LAT, uSPHERE_LOD1_LAT, uSPHERE_LOD2_LAT };
	const u_int auSphereLon[uNUM_LODS] = { uSPHERE_LOD0_LON, uSPHERE_LOD1_LON, uSPHERE_LOD2_LON };

	for (u_int i = 0; i < uNUM_LODS; ++i)
	{
		GenerateUnitSphere(xVertices, xIndices, auSphereLat[i], auSphereLon[i]);
		s_axSphereLODs[i].m_uIndexCount = xIndices.GetSize();
		Flux_MemoryManager::InitialiseVertexBuffer(xVertices.GetDataPointer(), xVertices.GetSize() * sizeof(LightVolumeVertex), s_axSphereLODs[i].m_xVertexBuffer);
		Flux_MemoryManager::InitialiseIndexBuffer(xIndices.GetDataPointer(), xIndices.GetSize() * sizeof(u_int), s_axSphereLODs[i].m_xIndexBuffer);
	}

	// Cone LODs for spot lights
	const u_int auConeSegments[uNUM_LODS] = { uCONE_LOD0_SEGMENTS, uCONE_LOD1_SEGMENTS, uCONE_LOD2_SEGMENTS };

	for (u_int i = 0; i < uNUM_LODS; ++i)
	{
		GenerateUnitCone(xVertices, xIndices, auConeSegments[i]);
		s_axConeLODs[i].m_uIndexCount = xIndices.GetSize();
		Flux_MemoryManager::InitialiseVertexBuffer(xVertices.GetDataPointer(), xVertices.GetSize() * sizeof(LightVolumeVertex), s_axConeLODs[i].m_xVertexBuffer);
		Flux_MemoryManager::InitialiseIndexBuffer(xIndices.GetDataPointer(), xIndices.GetSize() * sizeof(u_int), s_axConeLODs[i].m_xIndexBuffer);
	}

	// Initialize instance buffers for GPU instancing (per-LOD)
	// These are storage buffers for reading instance data in shaders via gl_InstanceIndex
	const u_int64 ulPointInstanceBufferSize = Flux_DynamicLights::uMAX_LIGHTS * sizeof(PointLightInstance);
	const u_int64 ulSpotInstanceBufferSize = Flux_DynamicLights::uMAX_LIGHTS * sizeof(SpotLightInstance);
	const u_int64 ulDirInstanceBufferSize = Flux_DynamicLights::uMAX_LIGHTS * sizeof(DirectionalLightInstance);

	// Zero-initialize buffers to prevent garbage data on frame 0
	std::vector<PointLightInstance> xZeroedPointLights(Flux_DynamicLights::uMAX_LIGHTS);
	std::vector<SpotLightInstance> xZeroedSpotLights(Flux_DynamicLights::uMAX_LIGHTS);
	std::vector<DirectionalLightInstance> xZeroedDirLights(Flux_DynamicLights::uMAX_LIGHTS);

	for (u_int i = 0; i < uNUM_LODS; ++i)
	{
		Flux_MemoryManager::InitialiseReadWriteBuffer(xZeroedPointLights.data(), ulPointInstanceBufferSize, s_axPointLightInstanceBuffers[i]);
		Flux_MemoryManager::InitialiseReadWriteBuffer(xZeroedSpotLights.data(), ulSpotInstanceBufferSize, s_axSpotLightInstanceBuffers[i]);
		s_auPointLightInstanceCounts[i] = 0;
		s_auSpotLightInstanceCounts[i] = 0;
	}
	Flux_MemoryManager::InitialiseReadWriteBuffer(xZeroedDirLights.data(), ulDirInstanceBufferSize, s_xDirectionalLightInstanceBuffer);
	s_uDirectionalLightInstanceCount = 0;

	// Pre-allocate sort buffer to avoid per-frame allocations during priority sorting
	s_xSortBuffer.Reserve(Flux_DynamicLights::uMAX_LIGHTS * 2);

	// Load volume shaders
	s_xVolumeShader.Initialise("DynamicLights/Flux_DynamicLights.vert", "DynamicLights/Flux_DynamicLights.frag");

	// Cache binding handles from shader reflection
	const Flux_ShaderReflection& xReflection = s_xVolumeShader.GetReflection();

	// Define vertex layout (Position only - compact format)
	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_TRIANGLES;
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);  // Position only
	xVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();

	// Base pipeline specification (shared settings)
	Flux_PipelineSpecification xPipelineSpec;
	xPipelineSpec.m_pxTargetSetup = &Flux_HDR::GetHDRSceneTargetSetup();
	xPipelineSpec.m_pxShader = &s_xVolumeShader;
	xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

	// Pipeline layout from shader reflection
	xReflection.PopulateLayout(xPipelineSpec.m_xPipelineLayout);

	// ADDITIVE BLENDING - adds light contribution to existing deferred output
	xPipelineSpec.m_axBlendStates[0].m_bBlendEnabled = true;
	xPipelineSpec.m_axBlendStates[0].m_eSrcBlendFactor = BLEND_FACTOR_ONE;
	xPipelineSpec.m_axBlendStates[0].m_eDstBlendFactor = BLEND_FACTOR_ONE;

	// Depth testing disabled because we need to sample the depth buffer as a texture
	// to reconstruct world position. Using it as both depth attachment and shader resource
	// causes Vulkan layout conflicts. The shader's range check handles pixel rejection.
	xPipelineSpec.m_bDepthTestEnabled = false;
	xPipelineSpec.m_bDepthWriteEnabled = false;

	// PIPELINE 1: Point/Spot lights - Front-face culling (render back faces only)
	// When camera is outside: back faces render, shader samples G-buffer depth
	// When camera is inside: back faces are visible, same shader logic applies
	xPipelineSpec.m_eCullMode = CULL_MODE_FRONT;
	Flux_PipelineBuilder::FromSpecification(s_xVolumePipeline, xPipelineSpec);

	// PIPELINE 2: Directional lights - Back-face culling (render front faces)
	// Fullscreen quads have front faces toward camera, so we cull back faces
	xPipelineSpec.m_eCullMode = CULL_MODE_BACK;
	Flux_PipelineBuilder::FromSpecification(s_xDirectionalPipeline, xPipelineSpec);
	s_xFrameConstantsBinding = xReflection.GetBinding("FrameConstants");
	s_xPointLightBufferBinding = xReflection.GetBinding("PointLightBuffer");
	s_xSpotLightBufferBinding = xReflection.GetBinding("SpotLightBuffer");
	s_xDirectionalLightBufferBinding = xReflection.GetBinding("DirectionalLightBuffer");
	s_xDiffuseTexBinding = xReflection.GetBinding("g_xDiffuseTex");
	s_xNormalsAmbientTexBinding = xReflection.GetBinding("g_xNormalsAmbientTex");
	s_xMaterialTexBinding = xReflection.GetBinding("g_xMaterialTex");
	s_xDepthTexBinding = xReflection.GetBinding("g_xDepthTex");
	s_xPushConstantsBinding = xReflection.GetBinding("pushConstants");
	s_xBRDFLUTBinding = xReflection.GetBinding("g_xBRDFLUT");

	// Validate binding handles (fail early if shader bindings are incorrect)
	Zenith_Assert(s_xFrameConstantsBinding.IsValid(), "Failed to find FrameConstants binding in DynamicLights shader");
	Zenith_Assert(s_xPointLightBufferBinding.IsValid(), "Failed to find PointLightBuffer binding in DynamicLights shader");
	Zenith_Assert(s_xSpotLightBufferBinding.IsValid(), "Failed to find SpotLightBuffer binding in DynamicLights shader");
	Zenith_Assert(s_xDirectionalLightBufferBinding.IsValid(), "Failed to find DirectionalLightBuffer binding in DynamicLights shader");
	Zenith_Assert(s_xDiffuseTexBinding.IsValid(), "Failed to find g_xDiffuseTex binding in DynamicLights shader");
	Zenith_Assert(s_xNormalsAmbientTexBinding.IsValid(), "Failed to find g_xNormalsAmbientTex binding in DynamicLights shader");
	Zenith_Assert(s_xMaterialTexBinding.IsValid(), "Failed to find g_xMaterialTex binding in DynamicLights shader");
	Zenith_Assert(s_xDepthTexBinding.IsValid(), "Failed to find g_xDepthTex binding in DynamicLights shader");
	Zenith_Assert(s_xPushConstantsBinding.IsValid(), "Failed to find PushConstants binding in DynamicLights shader");
	Zenith_Assert(s_xBRDFLUTBinding.IsValid(), "Failed to find g_xBRDFLUT binding in DynamicLights shader");

	s_bInitialised = true;

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "Dynamic Lights" }, dbg_bShowDynamicLights);
	Zenith_DebugVariables::AddFloat({ "Render", "Dynamic Lights", "LOD1 Threshold" }, dbg_fLightLOD1Threshold, 10.0f, 500.0f);
	Zenith_DebugVariables::AddFloat({ "Render", "Dynamic Lights", "LOD2 Threshold" }, dbg_fLightLOD2Threshold, 5.0f, 200.0f);
#endif

#if defined(ZENITH_TOOLS) && defined(ZENITH_DEBUG_VARIABLES)
	Zenith_DebugVariables::AddBoolean({ "Render", "Dynamic Lights", "Show Dropped Lights" }, dbg_bShowDroppedLights);
#endif

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_DynamicLights initialised (light volume rendering with %u LOD levels)", uNUM_LODS);
}

void Flux_DynamicLights::Shutdown()
{
	if (!s_bInitialised)
	{
		return;
	}

	// Clean up all LOD buffers
	for (u_int i = 0; i < uNUM_LODS; ++i)
	{
		Flux_MemoryManager::DestroyVertexBuffer(s_axSphereLODs[i].m_xVertexBuffer);
		Flux_MemoryManager::DestroyIndexBuffer(s_axSphereLODs[i].m_xIndexBuffer);
		Flux_MemoryManager::DestroyVertexBuffer(s_axConeLODs[i].m_xVertexBuffer);
		Flux_MemoryManager::DestroyIndexBuffer(s_axConeLODs[i].m_xIndexBuffer);

		// Clean up instance buffers
		Flux_MemoryManager::DestroyReadWriteBuffer(s_axPointLightInstanceBuffers[i]);
		Flux_MemoryManager::DestroyReadWriteBuffer(s_axSpotLightInstanceBuffers[i]);
	}
	Flux_MemoryManager::DestroyReadWriteBuffer(s_xDirectionalLightInstanceBuffer);

	s_bInitialised = false;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_DynamicLights shut down");
}

void Flux_DynamicLights::Reset()
{
	g_xCommandList.Reset(true);
	// Instance counts are reset in GatherLightsFromScene()
}

#ifdef ZENITH_TOOLS
// Storage for dropped light positions (for debug visualization)
static Zenith_Vector<Zenith_Maths::Vector3> s_xDroppedPointLightPositions;
static Zenith_Vector<Zenith_Maths::Vector3> s_xDroppedSpotLightPositions;
#endif

void Flux_DynamicLights::GatherLightsFromScene()
{
#ifdef ZENITH_TOOLS
	s_xDroppedPointLightPositions.Clear();
	s_xDroppedSpotLightPositions.Clear();
#endif

	// Clear instance counts
	for (u_int i = 0; i < uNUM_LODS; ++i)
	{
		s_auPointLightInstanceCounts[i] = 0;
		s_auSpotLightInstanceCounts[i] = 0;
	}
	s_uDirectionalLightInstanceCount = 0;

	// Update frustum for culling
	s_xCameraFrustum.ExtractFromViewProjection(Flux_Graphics::GetViewProjMatrix());

	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Temporary storage for visible lights (used during ECS query, then staged directly)
	// OPTIMIZATION: Light data flows directly from these vectors to staging buffers,
	// eliminating the previous intermediate static vectors (s_xPointLights, etc.)
	Zenith_Vector<PointLightData> xAllPointLights;
	Zenith_Vector<SpotLightData> xAllSpotLights;
	Zenith_Vector<DirectionalLightData> xAllDirectionalLights;

	// First pass: Collect ALL visible lights (no limit check yet)
	xScene.Query<Zenith_LightComponent, Zenith_TransformComponent>()
		.ForEach([&xAllPointLights, &xAllSpotLights, &xAllDirectionalLights](Zenith_EntityID uID, Zenith_LightComponent& xLight, Zenith_TransformComponent& xTransform)
		{
			LIGHT_TYPE eType = xLight.GetLightType();

			// Validate light type
			Zenith_Assert(eType < LIGHT_TYPE_COUNT, "Invalid light type: %u", static_cast<u_int>(eType));

			const Zenith_Maths::Vector3& xColor = xLight.GetColor();
			float fIntensity = xLight.GetIntensity();

			// Skip lights with negligible intensity
			if (fIntensity < fMIN_LIGHT_INTENSITY)
			{
				return;
			}

			if (eType == LIGHT_TYPE_POINT)
			{
				Zenith_Maths::Vector3 xPosition = xLight.GetWorldPosition();
				float fRange = xLight.GetRange();

				// Frustum culling: skip lights whose bounding sphere is completely outside view
				if (!IsSphereFrustumVisible(xPosition, fRange))
				{
					return;
				}

				// Calculate LOD based on screen-space size
				float fScreenRadius = CalculateScreenSpaceRadius(xPosition, fRange);
				u_int uLODIndex = SelectLOD(fScreenRadius);
				Zenith_Assert(uLODIndex < uNUM_LODS, "LOD index %u out of bounds (max %u)", uLODIndex, uNUM_LODS);

				PointLightData xData;
				xData.m_xPosition = xPosition;
				xData.m_fRange = fRange;
				xData.m_xColor = xColor;
				xData.m_fIntensity = fIntensity;
				xData.m_uLODIndex = uLODIndex;
				xAllPointLights.PushBack(xData);
			}
			else if (eType == LIGHT_TYPE_SPOT)
			{
				Zenith_Maths::Vector3 xPosition = xLight.GetWorldPosition();
				float fRange = xLight.GetRange();

				// Get spot angles (needed for cone culling)
				float fInnerAngle = xLight.GetSpotInnerAngle();
				float fOuterAngle = xLight.GetSpotOuterAngle();
				Zenith_Assert(fInnerAngle <= fOuterAngle,
					"Spot light inner angle (%.2f) must be <= outer angle (%.2f)", fInnerAngle, fOuterAngle);
				Zenith_Assert(fOuterAngle > 0.0f && fOuterAngle < glm::pi<float>(),
					"Spot light outer angle (%.2f) out of valid range", fOuterAngle);

				// Validate direction on CPU (needed for cone culling and avoids per-pixel validation in shader)
				Zenith_Maths::Vector3 xDirection = xLight.GetWorldDirection();
				float fDirLength = Zenith_Maths::Length(xDirection);
				if (fDirLength < fDIRECTION_EPSILON)
				{
					Zenith_Log(LOG_CATEGORY_RENDERER, "Skipping spot light with zero-length direction (Entity %u)", uID);
					return;
				}
				xDirection /= fDirLength;  // Normalize once on CPU

				// Frustum culling using cone test (more accurate than sphere approximation)
				if (!IsConeFrustumVisible(xPosition, xDirection, fRange, fOuterAngle))
				{
					return;
				}

				// Calculate LOD based on screen-space size
				float fScreenRadius = CalculateScreenSpaceRadius(xPosition, fRange);
				u_int uLODIndex = SelectLOD(fScreenRadius);
				Zenith_Assert(uLODIndex < uNUM_LODS, "LOD index %u out of bounds (max %u)", uLODIndex, uNUM_LODS);

				SpotLightData xData;
				xData.m_xPosition = xPosition;
				xData.m_fRange = fRange;
				xData.m_xColor = xColor;
				xData.m_fIntensity = fIntensity;
				xData.m_xDirection = xDirection;
				xData.m_fCosInner = cosf(fInnerAngle);
				xData.m_fCosOuter = cosf(fOuterAngle);
				xData.m_uLODIndex = uLODIndex;
				xAllSpotLights.PushBack(xData);
			}
			else if (eType == LIGHT_TYPE_DIRECTIONAL)
			{
				// Validate direction on CPU (avoids per-pixel validation in shader)
				Zenith_Maths::Vector3 xDirection = xLight.GetWorldDirection();
				float fDirLength = Zenith_Maths::Length(xDirection);
				if (fDirLength < fDIRECTION_EPSILON)
				{
					Zenith_Log(LOG_CATEGORY_RENDERER, "Skipping directional light with zero-length direction (Entity %u)", uID);
					return;
				}
				xDirection /= fDirLength;  // Normalize once on CPU

				DirectionalLightData xData;
				xData.m_xDirection = xDirection;
				xData.m_xColor = xColor;
				xData.m_fIntensity = fIntensity;
				xAllDirectionalLights.PushBack(xData);
			}
		});

	// Helper lambda: stage a point light directly to staging buffer
	auto StagePointLight = [](const PointLightData& xLight)
	{
		u_int uLOD = xLight.m_uLODIndex;
		u_int uIdx = s_auPointLightInstanceCounts[uLOD];

		Zenith_Assert(uIdx < Flux_DynamicLights::uMAX_LIGHTS,
			"Point light LOD %u overflow: %u lights (max %u)", uLOD, uIdx, Flux_DynamicLights::uMAX_LIGHTS);

		s_axPointLightStaging[uLOD][uIdx].m_xPositionRange =
			{ xLight.m_xPosition.x, xLight.m_xPosition.y, xLight.m_xPosition.z, xLight.m_fRange };
		s_axPointLightStaging[uLOD][uIdx].m_xColorIntensity =
			{ xLight.m_xColor.x, xLight.m_xColor.y, xLight.m_xColor.z, xLight.m_fIntensity };
		s_auPointLightInstanceCounts[uLOD]++;
	};

	// Helper lambda: stage a spot light directly to staging buffer
	auto StageSpotLight = [](const SpotLightData& xLight)
	{
		u_int uLOD = xLight.m_uLODIndex;
		u_int uIdx = s_auSpotLightInstanceCounts[uLOD];

		Zenith_Assert(uIdx < Flux_DynamicLights::uMAX_LIGHTS,
			"Spot light LOD %u overflow: %u lights (max %u)", uLOD, uIdx, Flux_DynamicLights::uMAX_LIGHTS);

		s_axSpotLightStaging[uLOD][uIdx].m_xPositionRange =
			{ xLight.m_xPosition.x, xLight.m_xPosition.y, xLight.m_xPosition.z, xLight.m_fRange };
		s_axSpotLightStaging[uLOD][uIdx].m_xColorIntensity =
			{ xLight.m_xColor.x, xLight.m_xColor.y, xLight.m_xColor.z, xLight.m_fIntensity };
		s_axSpotLightStaging[uLOD][uIdx].m_xDirectionInner =
			{ xLight.m_xDirection.x, xLight.m_xDirection.y, xLight.m_xDirection.z, xLight.m_fCosInner };
		s_axSpotLightStaging[uLOD][uIdx].m_xSpotOuter =
			{ xLight.m_fCosOuter, 0.0f, 0.0f, 0.0f };
		s_auSpotLightInstanceCounts[uLOD]++;
	};

	// Helper lambda: stage a directional light directly to staging buffer
	auto StageDirectionalLight = [](const DirectionalLightData& xLight)
	{
		u_int uIdx = s_uDirectionalLightInstanceCount;

		Zenith_Assert(uIdx < Flux_DynamicLights::uMAX_LIGHTS,
			"Directional light overflow: %u lights (max %u)", uIdx, Flux_DynamicLights::uMAX_LIGHTS);

		s_axDirectionalLightStaging[uIdx].m_xColorIntensity =
			{ xLight.m_xColor.x, xLight.m_xColor.y, xLight.m_xColor.z, xLight.m_fIntensity };
		s_axDirectionalLightStaging[uIdx].m_xDirectionPad =
			{ xLight.m_xDirection.x, xLight.m_xDirection.y, xLight.m_xDirection.z, 0.0f };
		s_uDirectionalLightInstanceCount++;
	};

	// Second pass: Stage lights directly to GPU buffers (with priority sorting if over limit)
	// OPTIMIZATION: Direct staging eliminates the previous intermediate copy step

	// Point lights
	u_int uDroppedPointLights = 0;
	if (xAllPointLights.GetSize() > Flux_DynamicLights::uMAX_LIGHTS)
	{
		// Sort by priority (highest first) - reuse pre-allocated buffer
		s_xSortBuffer.Clear();
		for (u_int i = 0; i < xAllPointLights.GetSize(); ++i)
		{
			const PointLightData& xLight = xAllPointLights.Get(i);
			float fPriority = CalculateLightPriority(xLight.m_xPosition, xLight.m_fIntensity, xLight.m_fRange);
			s_xSortBuffer.PushBack({ fPriority, i });
		}
		std::sort(s_xSortBuffer.GetDataPointer(), s_xSortBuffer.GetDataPointer() + s_xSortBuffer.GetSize());

		// Take top N lights - stage directly to GPU buffer
		for (u_int i = 0; i < Flux_DynamicLights::uMAX_LIGHTS; ++i)
		{
			StagePointLight(xAllPointLights.Get(s_xSortBuffer.Get(i).m_uIndex));
		}

		// Track dropped lights for debug visualization
		uDroppedPointLights = xAllPointLights.GetSize() - Flux_DynamicLights::uMAX_LIGHTS;
#ifdef ZENITH_TOOLS
		for (u_int i = Flux_DynamicLights::uMAX_LIGHTS; i < xAllPointLights.GetSize(); ++i)
		{
			s_xDroppedPointLightPositions.PushBack(xAllPointLights.Get(s_xSortBuffer.Get(i).m_uIndex).m_xPosition);
		}
#endif
	}
	else
	{
		// Under limit - stage all directly to GPU buffer (no intermediate copy)
		for (u_int i = 0; i < xAllPointLights.GetSize(); ++i)
		{
			StagePointLight(xAllPointLights.Get(i));
		}
	}

	// Spot lights
	u_int uDroppedSpotLights = 0;
	if (xAllSpotLights.GetSize() > Flux_DynamicLights::uMAX_LIGHTS)
	{
		// Sort by priority (highest first) - reuse pre-allocated buffer
		s_xSortBuffer.Clear();
		for (u_int i = 0; i < xAllSpotLights.GetSize(); ++i)
		{
			const SpotLightData& xLight = xAllSpotLights.Get(i);
			float fPriority = CalculateLightPriority(xLight.m_xPosition, xLight.m_fIntensity, xLight.m_fRange);
			s_xSortBuffer.PushBack({ fPriority, i });
		}
		std::sort(s_xSortBuffer.GetDataPointer(), s_xSortBuffer.GetDataPointer() + s_xSortBuffer.GetSize());

		// Take top N lights - stage directly to GPU buffer
		for (u_int i = 0; i < Flux_DynamicLights::uMAX_LIGHTS; ++i)
		{
			StageSpotLight(xAllSpotLights.Get(s_xSortBuffer.Get(i).m_uIndex));
		}

		// Track dropped lights for debug visualization
		uDroppedSpotLights = xAllSpotLights.GetSize() - Flux_DynamicLights::uMAX_LIGHTS;
#ifdef ZENITH_TOOLS
		for (u_int i = Flux_DynamicLights::uMAX_LIGHTS; i < xAllSpotLights.GetSize(); ++i)
		{
			s_xDroppedSpotLightPositions.PushBack(xAllSpotLights.Get(s_xSortBuffer.Get(i).m_uIndex).m_xPosition);
		}
#endif
	}
	else
	{
		// Under limit - stage all directly to GPU buffer (no intermediate copy)
		for (u_int i = 0; i < xAllSpotLights.GetSize(); ++i)
		{
			StageSpotLight(xAllSpotLights.Get(i));
		}
	}

	// Directional lights (simple limit, no priority sorting - rarely have many)
	u_int uDroppedDirLights = 0;
	u_int uMaxDirLights = std::min(static_cast<u_int>(xAllDirectionalLights.GetSize()), Flux_DynamicLights::uMAX_LIGHTS);
	for (u_int i = 0; i < uMaxDirLights; ++i)
	{
		StageDirectionalLight(xAllDirectionalLights.Get(i));
	}
	if (xAllDirectionalLights.GetSize() > Flux_DynamicLights::uMAX_LIGHTS)
	{
		uDroppedDirLights = xAllDirectionalLights.GetSize() - Flux_DynamicLights::uMAX_LIGHTS;
	}

	// Log warnings for dropped lights (includes count for better diagnostics)
	if (uDroppedPointLights > 0)
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "Dropped %u point lights (limit: %u, kept highest priority)", uDroppedPointLights, Flux_DynamicLights::uMAX_LIGHTS);
	}
	if (uDroppedSpotLights > 0)
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "Dropped %u spot lights (limit: %u, kept highest priority)", uDroppedSpotLights, Flux_DynamicLights::uMAX_LIGHTS);
	}
	if (uDroppedDirLights > 0)
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "Dropped %u directional lights (limit: %u)", uDroppedDirLights, Flux_DynamicLights::uMAX_LIGHTS);
	}

	// Upload staged instance data to GPU buffers
	// NOTE: Flux_MemoryManager::UploadBufferData() handles memory barriers internally,
	// ensuring transfer writes complete before shader reads. This is required because
	// these buffers are read in the vertex/fragment shaders during Render().
	for (u_int i = 0; i < uNUM_LODS; ++i)
	{
		if (s_auPointLightInstanceCounts[i] > 0)
		{
			Flux_MemoryManager::UploadBufferData(
				s_axPointLightInstanceBuffers[i].GetBuffer().m_xVRAMHandle,
				s_axPointLightStaging[i],
				s_auPointLightInstanceCounts[i] * sizeof(PointLightInstance));
		}

		if (s_auSpotLightInstanceCounts[i] > 0)
		{
			Flux_MemoryManager::UploadBufferData(
				s_axSpotLightInstanceBuffers[i].GetBuffer().m_xVRAMHandle,
				s_axSpotLightStaging[i],
				s_auSpotLightInstanceCounts[i] * sizeof(SpotLightInstance));
		}
	}

	if (s_uDirectionalLightInstanceCount > 0)
	{
		Flux_MemoryManager::UploadBufferData(
			s_xDirectionalLightInstanceBuffer.GetBuffer().m_xVRAMHandle,
			s_axDirectionalLightStaging,
			s_uDirectionalLightInstanceCount * sizeof(DirectionalLightInstance));
	}
}

void Flux_DynamicLights::SubmitRenderTask()
{
	Zenith_TaskSystem::SubmitTask(&g_xRenderTask);
}

void Flux_DynamicLights::WaitForRenderTask()
{
	g_xRenderTask.WaitUntilComplete();
}

void Flux_DynamicLights::Render(void*)
{
	if (!s_bInitialised)
	{
		return;
	}

#ifdef ZENITH_DEBUG_VARIABLES
	if (!dbg_bShowDynamicLights)
	{
		return;
	}
#endif

	GatherLightsFromScene();

	// Calculate total lights from instance counts (replaces old vector size checks)
	u_int uTotalPointLights = 0;
	u_int uTotalSpotLights = 0;
	for (u_int i = 0; i < uNUM_LODS; ++i)
	{
		uTotalPointLights += s_auPointLightInstanceCounts[i];
		uTotalSpotLights += s_auSpotLightInstanceCounts[i];
	}

	// Skip if no lights
	u_int uTotalLights = uTotalPointLights + uTotalSpotLights + s_uDirectionalLightInstanceCount;
	if (uTotalLights == 0)
	{
		return;
	}

	g_xCommandList.Reset(false);  // Don't clear - we're adding to existing scene

	// Use shader binder for named bindings
	Flux_ShaderBinder xBinder(g_xCommandList);

	// Bind frame constants (shared by all lights)
	xBinder.BindCBV(s_xFrameConstantsBinding, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());

	// Bind G-buffer textures (shared by all lights)
	xBinder.BindSRV(s_xDiffuseTexBinding, Flux_Graphics::GetGBufferSRV(MRT_INDEX_DIFFUSE));
	xBinder.BindSRV(s_xNormalsAmbientTexBinding, Flux_Graphics::GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));
	xBinder.BindSRV(s_xMaterialTexBinding, Flux_Graphics::GetGBufferSRV(MRT_INDEX_MATERIAL));
	xBinder.BindSRV(s_xDepthTexBinding, Flux_Graphics::GetDepthStencilSRV());

	// Bind BRDF LUT for multiscatter energy compensation
	// This ensures rough metals have consistent brightness between IBL and dynamic lights
	xBinder.BindSRV(s_xBRDFLUTBinding, &Flux_IBL::GetBRDFLUTSRV());

	// Initial binding of instance buffers to satisfy Vulkan validation
	// The shader statically references all three buffers, so they must all be bound.
	// NOTE: Point and spot light buffers are rebound per-LOD in the rendering loops below.
	xBinder.BindUAV_Buffer(s_xPointLightBufferBinding, &s_axPointLightInstanceBuffers[0].GetUAV());
	xBinder.BindUAV_Buffer(s_xSpotLightBufferBinding, &s_axSpotLightInstanceBuffers[0].GetUAV());
	xBinder.BindUAV_Buffer(s_xDirectionalLightBufferBinding, &s_xDirectionalLightInstanceBuffer.GetUAV());

	// ========== RENDER POINT LIGHTS (INSTANCED) ==========
	// Use volume pipeline with front-face culling (render back faces)
	// One instanced draw call per LOD level
	{
		bool bAnyPointLights = false;
		for (u_int uLOD = 0; uLOD < uNUM_LODS; ++uLOD)
		{
			if (s_auPointLightInstanceCounts[uLOD] > 0)
			{
				bAnyPointLights = true;
				break;
			}
		}

		if (bAnyPointLights)
		{
			g_xCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xVolumePipeline);

			// Push light type constant (0 = point)
			LightTypePushConstant xTypeConstant;
			xTypeConstant.m_uLightType = 0;  // LIGHT_TYPE_POINT
			xTypeConstant.m_uPad0 = 0;
			xTypeConstant.m_uPad1 = 0;
			xTypeConstant.m_uPad2 = 0;
			xBinder.PushConstant(s_xPushConstantsBinding, &xTypeConstant, sizeof(LightTypePushConstant));

			for (u_int uLOD = 0; uLOD < uNUM_LODS; ++uLOD)
			{
				if (s_auPointLightInstanceCounts[uLOD] == 0)
				{
					continue;
				}

				// Bind storage buffer for this LOD's point lights
				xBinder.BindUAV_Buffer(s_xPointLightBufferBinding, &s_axPointLightInstanceBuffers[uLOD].GetUAV());

				// Bind geometry for this LOD
				g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&s_axSphereLODs[uLOD].m_xVertexBuffer);
				g_xCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&s_axSphereLODs[uLOD].m_xIndexBuffer);

				// Instanced draw: one draw call for all lights at this LOD
				g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(
					s_axSphereLODs[uLOD].m_uIndexCount,
					s_auPointLightInstanceCounts[uLOD]);
			}
		}
	}

	// ========== RENDER SPOT LIGHTS (INSTANCED) ==========
	// Use volume pipeline with front-face culling (render back faces)
	// One instanced draw call per LOD level
	{
		bool bAnySpotLights = false;
		for (u_int uLOD = 0; uLOD < uNUM_LODS; ++uLOD)
		{
			if (s_auSpotLightInstanceCounts[uLOD] > 0)
			{
				bAnySpotLights = true;
				break;
			}
		}

		if (bAnySpotLights)
		{
			g_xCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xVolumePipeline);

			// Push light type constant (1 = spot)
			LightTypePushConstant xTypeConstant;
			xTypeConstant.m_uLightType = 1;  // LIGHT_TYPE_SPOT
			xTypeConstant.m_uPad0 = 0;
			xTypeConstant.m_uPad1 = 0;
			xTypeConstant.m_uPad2 = 0;
			xBinder.PushConstant(s_xPushConstantsBinding, &xTypeConstant, sizeof(LightTypePushConstant));

			for (u_int uLOD = 0; uLOD < uNUM_LODS; ++uLOD)
			{
				if (s_auSpotLightInstanceCounts[uLOD] == 0)
				{
					continue;
				}

				// Bind storage buffer for this LOD's spot lights
				xBinder.BindUAV_Buffer(s_xSpotLightBufferBinding, &s_axSpotLightInstanceBuffers[uLOD].GetUAV());

				// Bind geometry for this LOD
				g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&s_axConeLODs[uLOD].m_xVertexBuffer);
				g_xCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&s_axConeLODs[uLOD].m_xIndexBuffer);

				// Instanced draw: one draw call for all lights at this LOD
				g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(
					s_axConeLODs[uLOD].m_uIndexCount,
					s_auSpotLightInstanceCounts[uLOD]);
			}
		}
	}

	// ========== RENDER DIRECTIONAL LIGHTS (INSTANCED) ==========
	// Directional lights use fullscreen quad (they affect all pixels)
	// Use directional pipeline with back-face culling (render front faces of quad)
	if (s_uDirectionalLightInstanceCount > 0)
	{
		g_xCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xDirectionalPipeline);

		// Push light type constant (2 = directional)
		LightTypePushConstant xTypeConstant;
		xTypeConstant.m_uLightType = 2;  // LIGHT_TYPE_DIRECTIONAL
		xTypeConstant.m_uPad0 = 0;
		xTypeConstant.m_uPad1 = 0;
		xTypeConstant.m_uPad2 = 0;
		xBinder.PushConstant(s_xPushConstantsBinding, &xTypeConstant, sizeof(LightTypePushConstant));

		// Bind storage buffer for directional lights
		xBinder.BindUAV_Buffer(s_xDirectionalLightBufferBinding, &s_xDirectionalLightInstanceBuffer.GetUAV());

		// Bind fullscreen quad geometry
		g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer());
		g_xCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());

		// Instanced draw: one draw call for all directional lights
		g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(6, s_uDirectionalLightInstanceCount);
	}

	// Submit at RENDER_ORDER_POINT_LIGHTS (after APPLY_LIGHTING)
	Flux::SubmitCommandList(&g_xCommandList, Flux_HDR::GetHDRSceneTargetSetup(), RENDER_ORDER_POINT_LIGHTS);
}
