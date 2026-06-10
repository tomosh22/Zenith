#pragma once

#include "Flux/Flux.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Maths/Zenith_Maths.h"

class Flux_CommandList;
class Flux_RenderGraph;
class Flux_VolumeFogImpl;
class FrameContext;
class Flux_GraphicsImpl;
class Flux_ShadowsImpl;

// Push constant structures (must match shader)
struct InjectConstants
{
	Zenith_Maths::Vector4 m_xFogParams;       // x = density, y = scattering, z = absorption, w = time
	Zenith_Maths::Vector4 m_xNoiseParams;     // x = scale, y = speed, z = detail, w = unused
	Zenith_Maths::Vector4 m_xHeightParams;    // x = base height, y = falloff, z = min height, w = max height
	Zenith_Maths::Vector4 m_xGridDimensions;  // x = width, y = height, z = depth, w = unused
	float m_fNearZ;
	float m_fFarZ;
	u_int m_uFrameIndex;
	float m_fPad0;
};

struct LightConstants
{
	Zenith_Maths::Vector4 m_xFogColour;
	Zenith_Maths::Vector4 m_xLightDirection;
	Zenith_Maths::Vector4 m_xLightColour;      // RGB = color, A = intensity
	Zenith_Maths::Vector4 m_xGridDimensions;
	float m_fScatteringCoeff;
	float m_fAbsorptionCoeff;
	float m_fPhaseG;
	u_int m_uDebugMode;
	// Volumetric shadow parameters (now runtime-adjustable)
	// Shadow bias prevents self-shadowing artifacts in fog
	// Cone radius controls softness of volumetric shadows
	float m_fVolShadowBias;
	float m_fVolShadowConeRadius;
	// Ambient irradiance ratio: fraction of sky light vs direct sun (0.15-0.6 typical)
	float m_fAmbientIrradianceRatio;
	float m_fPad0;  // Padding to maintain 16-byte alignment
};

struct ApplyConstants
{
	Zenith_Maths::Vector4 m_xGridDimensions;
	float m_fNearZ;
	float m_fFarZ;
	u_int m_uDebugMode;
	u_int m_uDebugSliceIndex;
};

// Phase 9: state + behaviour for FroxelFog subsystem.
class Flux_FroxelFogImpl
{
public:
	Flux_FroxelFogImpl() = default;
	~Flux_FroxelFogImpl() = default;

	Flux_FroxelFogImpl(const Flux_FroxelFogImpl&) = delete;
	Flux_FroxelFogImpl& operator=(const Flux_FroxelFogImpl&) = delete;

	// Injected engine-subsystem dependencies (de-globalization pass): the Fog
	// orchestrator passes these from Flux_FogImpl::Initialise.
	void Initialise(Flux_VolumeFogImpl& xVolumeFog, FrameContext& xFrame, Flux_GraphicsImpl& xFluxGraphics, Flux_ShadowsImpl& xShadows);
	void BuildPipelines();
	void Reset();

	void SetupTransients(Flux_RenderGraph& xGraph);

	void RenderInject(Flux_CommandList* pxCommandList);
	void RenderLight(Flux_CommandList* pxCommandList);
	void RenderApply(Flux_CommandList* pxCommandList);

	Flux_RenderAttachment& GetDensityGrid();
	Flux_RenderAttachment& GetLightingGrid();
	Flux_RenderAttachment& GetScatteringGrid();

	Flux_TransientHandle GetDensityGridHandle() const    { return m_xDensityGridHandle; }
	Flux_TransientHandle GetLightingGridHandle() const   { return m_xLightingGridHandle; }
	Flux_TransientHandle GetScatteringGridHandle() const { return m_xScatteringGridHandle; }

	Flux_RenderAttachment& GetDebugSliceTexture();

	float GetNearZ();
	float GetFarZ();

	// Pipelines (3 pass programs: inject / light / apply).
	Flux_Shader   m_xInjectShader;
	Flux_Pipeline m_xInjectPipeline;
	Flux_RootSig  m_xInjectRootSig;

	Flux_Shader   m_xLightShader;
	Flux_Pipeline m_xLightPipeline;
	Flux_RootSig  m_xLightRootSig;

	Flux_Shader   m_xApplyShader;
	Flux_Pipeline m_xApplyPipeline;

	// Graph-owned transient handles.
	Flux_TransientHandle m_xDensityGridHandle;
	Flux_TransientHandle m_xLightingGridHandle;
	Flux_TransientHandle m_xScatteringGridHandle;
	Flux_RenderGraph*    m_pxGraph = nullptr;

private:
	// Promoted from file-static free helpers so they self-route to members.
	Flux_RenderAttachment& GetDensityGridInternal();
	Flux_RenderAttachment& GetLightingGridInternal();
	Flux_RenderAttachment& GetScatteringGridInternal();

	// Injected engine-subsystem dependencies (de-globalization pass).
	Flux_VolumeFogImpl* m_pxVolumeFog    = nullptr;
	FrameContext*       m_pxFrame        = nullptr;
	Flux_GraphicsImpl*  m_pxFluxGraphics = nullptr;
	Flux_ShadowsImpl*   m_pxShadows      = nullptr;

	// Per-frame push-constant scratch PODs (relocated from module-scope statics).
	InjectConstants m_xInjectConstants;
	LightConstants  m_xLightConstants;
	ApplyConstants  m_xApplyConstants;
};
