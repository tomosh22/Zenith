#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Core/Zenith_EnvironmentAuthority.h" // neutral atmosphere defaults for the capture snapshot

enum IBL_DebugMode : u_int
{
	IBL_DEBUG_NONE,
	IBL_DEBUG_IRRADIANCE_MAP,
	IBL_DEBUG_PREFILTERED_MIPS,
	IBL_DEBUG_BRDF_LUT,
	IBL_DEBUG_DIFFUSE_ONLY,
	IBL_DEBUG_SPECULAR_ONLY,
	IBL_DEBUG_FRESNEL,
	IBL_DEBUG_REFLECTION_VECTOR,
	IBL_DEBUG_PROBE_VOLUMES,
	IBL_DEBUG_PROBE_CAPTURE,
	IBL_DEBUG_ROUGHNESS_LOD,
	IBL_DEBUG_COUNT
};

// IBL regeneration state machine for frame-amortized updates.
enum IBL_RegenState : u_int
{
	IBL_REGEN_IDLE,
	IBL_REGEN_IRRADIANCE,
	IBL_REGEN_PREFILTER
};

namespace IBLConfig
{
	constexpr u_int uBRDF_LUT_SIZE = 512;
	constexpr u_int uIRRADIANCE_SIZE = 32;
	constexpr u_int uPREFILTER_SIZE = 128;
	constexpr u_int uPREFILTER_MIP_COUNT = 7;
	constexpr u_int uMAX_PROBES = 16;
	constexpr u_int uPASSES_PER_FRAME = 8;
	// Irradiance + prefiltered cubes are DOUBLE BUFFERED: a generation always
	// writes the back pair and the pair is published (swapped to front) only
	// once the whole generation is complete. Consumers therefore never sample a
	// half-updated cube -- see "Coherent publication" in Flux_IBLImpl below.
	constexpr u_int uBUFFER_COUNT = 2;
}

// =====================================================================
// One FROZEN environment capture. Every pass of one IBL generation reads
// exactly this struct, so a generation can never combine faces/mips
// convolved from two different skies -- which is what reading the live
// per-frame sun direction out of the VIEW set used to allow.
//
// The engine's single radiometric anchor is CAPTURED here, never authored:
// m_fSunIntensity is only ever filled from the read-only
// Flux_SkyboxImpl::GetSunIntensity() (AtmosphereConfig::fSUN_INTENSITY).
// Capturing it keeps "every pass of a generation used identical inputs" a
// property of one struct rather than of a scattered set of live getters; it
// does NOT make solar intensity mutable or scene-authorable.
// =====================================================================
struct Flux_IBLEnvironmentSnapshot
{
	// Direction the sunlight TRAVELS into the scene (normalised).
	Zenith_Maths::Vector3 m_xSunDirection       = Zenith_GetDefaultSunDirection();
	float                 m_fRayleighScale      = Zenith_GetDefaultAtmosphereRayleighScale();
	float                 m_fMieScale           = Zenith_GetDefaultAtmosphereMieScale();
	float                 m_fMieG               = Zenith_GetDefaultAtmosphereMieG();
	float                 m_fRayleighScaleHeight = Zenith_GetDefaultAtmosphereRayleighScaleHeight();
	float                 m_fMieScaleHeight     = Zenith_GetDefaultAtmosphereMieScaleHeight();
	float                 m_fGroundAlbedo       = Zenith_GetDefaultAtmosphereGroundAlbedo();
	float                 m_fSunIntensity       = 0.0f;
	// Quality, not medium: whether the capture folds in the Hillaire orders
	// 2..infinity. Captured with the rest so a generation cannot straddle a
	// toggle, and compared like a medium field so flipping it re-captures.
	bool                  m_bMultiScattering    = true;
};

// Pure capture-scheduling predicate. Kept free of render-graph objects so the
// accumulate/coalesce semantics are directly unit-testable (mirrors
// Flux_IBLRegen below).
namespace Flux_IBLEnvironment
{
	// Historical angular tolerance on the captured sun direction (~0.081 deg),
	// now the DEFAULT of Zenith_GraphicsOptions::m_fIBLCaptureThresholdDegrees
	// rather than a hard constant -- a 120 s day and a 40-minute day want very
	// different answers, and one compile-time number served neither.
	//
	// It is measured against the last CAPTURED/scheduled/completed target, never
	// against the previous frame: under slow continuous motion a per-frame
	// baseline is re-armed every frame and the displacement can never accumulate
	// past the threshold, which is how a 120 s day at 60 FPS (~0.05 deg/frame)
	// used to leave the IBL frozen forever.
	inline constexpr float fDEFAULT_SUN_DIRECTION_DOT_EPSILON = 0.999999f;
	// Guard rails on the authored value. Below the floor the threshold is finer
	// than float precision on a normalised dot and every frame re-captures;
	// above the ceiling (~11.5 deg) the ambient visibly lags the sun.
	inline constexpr float fMIN_THRESHOLD_DEGREES = 0.0005f;
	inline constexpr float fMAX_THRESHOLD_DEGREES = 11.5f;

	// Authored degrees -> the dot-product epsilon the comparison uses.
	inline float CosThresholdFromDegrees(float fDegrees)
	{
		const float fClamped = (fDegrees < fMIN_THRESHOLD_DEGREES)
			? fMIN_THRESHOLD_DEGREES
			: ((fDegrees > fMAX_THRESHOLD_DEGREES) ? fMAX_THRESHOLD_DEGREES : fDegrees);
		return cosf(fClamped * 3.14159265358979323846f / 180.0f);
	}

	// Does the live environment differ materially from a captured target?
	// The atmosphere medium compares EXACTLY (authored floats, applied
	// verbatim); the radiometric anchor is deliberately not compared -- it is a
	// policy constant, not a runtime look control, so it can never differ.
	inline bool Differs(const Flux_IBLEnvironmentSnapshot& xTarget,
		const Flux_IBLEnvironmentSnapshot& xLive,
		float fCosThreshold = fDEFAULT_SUN_DIRECTION_DOT_EPSILON)
	{
		if (xTarget.m_fRayleighScale       != xLive.m_fRayleighScale)       return true;
		if (xTarget.m_fMieScale            != xLive.m_fMieScale)            return true;
		if (xTarget.m_fMieG                != xLive.m_fMieG)                return true;
		if (xTarget.m_fRayleighScaleHeight != xLive.m_fRayleighScaleHeight) return true;
		if (xTarget.m_fMieScaleHeight      != xLive.m_fMieScaleHeight)      return true;
		if (xTarget.m_fGroundAlbedo        != xLive.m_fGroundAlbedo)        return true;
		if (xTarget.m_bMultiScattering     != xLive.m_bMultiScattering)     return true;
		return glm::dot(xTarget.m_xSunDirection, xLive.m_xSunDirection) < fCosThreshold;
	}
}

// =====================================================================
// GPU pass-constant layouts + their PURE builders.
//
// These mirror IrradianceConstantsLayout / PrefilterConstantsLayout in
// Shaders/IBL/*.slang (sizes/offsets are static_asserted against the generated
// Slang reflection in Flux_IBL.cpp). Every field comes from the FROZEN
// snapshot -- the record callbacks read no live getter and the shaders read no
// per-frame spine value -- which is what makes "the authored atmosphere
// reaches both IBL convolutions" and "every pass of a generation used the same
// sky" structurally true, and testable without a GPU.
// =====================================================================
namespace Flux_IBLPassConstants
{
	struct Irradiance
	{
		float m_afSunDirection[4]; // direction light TRAVELS; w unused
		float m_fSunIntensity;
		float m_fRayleighScale;
		float m_fMieScale;
		float m_fMieG;
		float m_fRayleighScaleHeight;
		float m_fMieScaleHeight;
		float m_fGroundAlbedo;
		u_int m_uUseAtmosphere;
		u_int m_uMultiScatteringEnabled;
		u_int m_uFaceIndex;
		float m_fPad0;
		float m_fPad1;
	};
	static_assert(sizeof(Irradiance) == 64, "IBL irradiance constants must match the Slang layout");

	struct Prefilter
	{
		float m_afSunDirection[4];
		float m_fSunIntensity;
		float m_fRayleighScale;
		float m_fMieScale;
		float m_fMieG;
		float m_fRayleighScaleHeight;
		float m_fMieScaleHeight;
		float m_fGroundAlbedo;
		u_int m_uUseAtmosphere;
		u_int m_uMultiScatteringEnabled;
		u_int m_uFaceIndex;
		float m_fRoughness;
		float m_fPad0;
	};
	static_assert(sizeof(Prefilter) == 64, "IBL prefilter constants must match the Slang layout");

	// mip 0 = mirror-smooth, last mip = fully rough.
	inline float RoughnessForMip(u_int uMip)
	{
		return static_cast<float>(uMip) / static_cast<float>(IBLConfig::uPREFILTER_MIP_COUNT - 1u);
	}

	inline Irradiance BuildIrradiance(const Flux_IBLEnvironmentSnapshot& xEnv, u_int uFace)
	{
		Irradiance x = {};
		x.m_afSunDirection[0]      = xEnv.m_xSunDirection.x;
		x.m_afSunDirection[1]      = xEnv.m_xSunDirection.y;
		x.m_afSunDirection[2]      = xEnv.m_xSunDirection.z;
		x.m_afSunDirection[3]      = 0.0f;
		x.m_fSunIntensity          = xEnv.m_fSunIntensity;
		x.m_fRayleighScale         = xEnv.m_fRayleighScale;
		x.m_fMieScale              = xEnv.m_fMieScale;
		x.m_fMieG                  = xEnv.m_fMieG;
		x.m_fRayleighScaleHeight   = xEnv.m_fRayleighScaleHeight;
		x.m_fMieScaleHeight        = xEnv.m_fMieScaleHeight;
		x.m_fGroundAlbedo          = xEnv.m_fGroundAlbedo;
		x.m_uUseAtmosphere         = 1u;
		x.m_uMultiScatteringEnabled = xEnv.m_bMultiScattering ? 1u : 0u;
		x.m_uFaceIndex             = uFace;
		return x;
	}

	inline Prefilter BuildPrefilter(const Flux_IBLEnvironmentSnapshot& xEnv, u_int uMip, u_int uFace)
	{
		Prefilter x = {};
		x.m_afSunDirection[0]      = xEnv.m_xSunDirection.x;
		x.m_afSunDirection[1]      = xEnv.m_xSunDirection.y;
		x.m_afSunDirection[2]      = xEnv.m_xSunDirection.z;
		x.m_afSunDirection[3]      = 0.0f;
		x.m_fSunIntensity          = xEnv.m_fSunIntensity;
		x.m_fRayleighScale         = xEnv.m_fRayleighScale;
		x.m_fMieScale              = xEnv.m_fMieScale;
		x.m_fMieG                  = xEnv.m_fMieG;
		x.m_fRayleighScaleHeight   = xEnv.m_fRayleighScaleHeight;
		x.m_fMieScaleHeight        = xEnv.m_fMieScaleHeight;
		x.m_fGroundAlbedo          = xEnv.m_fGroundAlbedo;
		x.m_uUseAtmosphere         = 1u;
		x.m_uMultiScatteringEnabled = xEnv.m_bMultiScattering ? 1u : 0u;
		x.m_uFaceIndex             = uFace;
		x.m_fRoughness             = RoughnessForMip(uMip);
		return x;
	}

	// (The multi-scatter LUT bake constants live in
	// Flux/Skybox/Flux_AtmosphereTransmittance.h -- the Skybox and the IBL each
	// bake their OWN copy of that LUT from their own medium, so the struct
	// belongs with the shared atmosphere maths, not with the IBL pass data.)
}

// Per-frame regeneration work resolved by the state machine: which irradiance
// faces / prefilter (mip, face) slots run this frame, and whether they run for
// BOTH buffers (only a first generation seeds both; see Flux_IBLImpl).
struct Flux_IBLRegenFrameWork
{
	bool m_abIrradiance[6] = {};
	bool m_abPrefilter[IBLConfig::uPREFILTER_MIP_COUNT][6] = {};
	bool m_bWriteBothBuffers = false;

	// Does any convolution pass run this frame? Gates the multi-scatter LUT
	// bake, whose only readers are those passes.
	bool HasWork() const
	{
		for (u_int uFace = 0u; uFace < 6u; uFace++)
		{
			if (m_abIrradiance[uFace]) return true;
		}
		for (u_int uMip = 0u; uMip < IBLConfig::uPREFILTER_MIP_COUNT; uMip++)
		{
			for (u_int uFace = 0u; uFace < 6u; uFace++)
			{
				if (m_abPrefilter[uMip][uFace]) return true;
			}
		}
		return false;
	}
};

// Pure cursor logic behind the frame-amortised regeneration. Keeping the
// transition function independent of render-graph objects makes restart and
// convergence semantics directly unit-testable.
namespace Flux_IBLRegen
{
	enum PassType : u_int
	{
		PASS_NONE,
		PASS_IRRADIANCE,
		PASS_PREFILTER
	};

	struct Pass
	{
		PassType m_eType = PASS_NONE;
		u_int m_uMip = 0u;
		u_int m_uFace = 0u;
	};

	inline void MarkDirty(bool bFirstGeneration, bool& bDirty,
		IBL_RegenState& eState, u_int& uFace, u_int& uMip)
	{
		bDirty = true;
		if (!bFirstGeneration && eState != IBL_REGEN_IDLE)
		{
			// A second lighting change while old convolution work is in flight
			// must restart at irradiance face zero. Continuing would combine
			// faces generated from two different skies and then clear dirty.
			eState = IBL_REGEN_IRRADIANCE;
			uFace = 0u;
			uMip = 0u;
		}
	}

	inline Pass Next(bool& bDirty, IBL_RegenState& eState, u_int& uFace, u_int& uMip)
	{
		if (eState == IBL_REGEN_IDLE)
		{
			if (!bDirty)
			{
				return Pass();
			}
			eState = IBL_REGEN_IRRADIANCE;
			uFace = 0u;
			uMip = 0u;
		}

		if (eState == IBL_REGEN_IRRADIANCE)
		{
			Pass xPass;
			xPass.m_eType = PASS_IRRADIANCE;
			xPass.m_uFace = uFace++;
			if (uFace >= 6u)
			{
				eState = IBL_REGEN_PREFILTER;
				uFace = 0u;
				uMip = 0u;
			}
			return xPass;
		}

		Pass xPass;
		xPass.m_eType = PASS_PREFILTER;
		xPass.m_uMip = uMip;
		xPass.m_uFace = uFace++;
		if (uFace >= 6u)
		{
			uFace = 0u;
			uMip++;
			if (uMip >= IBLConfig::uPREFILTER_MIP_COUNT)
			{
				eState = IBL_REGEN_IDLE;
				bDirty = false;
			}
		}
		return xPass;
	}
}

// Phase 9: state + behaviour for IBL subsystem.
class Flux_IBLImpl
{
public:
	Flux_IBLImpl() = default;
	~Flux_IBLImpl() = default;

	Flux_IBLImpl(const Flux_IBLImpl&) = delete;
	Flux_IBLImpl& operator=(const Flux_IBLImpl&) = delete;

	void Initialise();
	void Shutdown();
	void Reset();
	void BuildPipelines();

	void GenerateBRDFLUT();
	void SetupRenderGraph(Flux_RenderGraph& xGraph);
	void UpdateGraphPassEnables(Flux_RenderGraph& xGraph);

	void MarkAllProbesDirty();

	// ---------------------------------------------------------------------
	// Runtime environment capture (the ONLY runtime IBL invalidation path).
	//
	// Called once per frame by Flux_GraphicsImpl with the live resolved
	// environment. Semantics:
	//   - displacement is measured against the last CAPTURED target, so slow
	//     continuous motion accumulates until it crosses the threshold;
	//   - a change arriving while a generation is IN FLIGHT never restarts it
	//     (restarting is what starved regeneration at low frame rates) --
	//     changes coalesce into ONE latest pending target;
	//   - when the generation completes, the latest pending target starts the
	//     next generation if it still differs materially.
	// ---------------------------------------------------------------------
	void RequestEnvironmentUpdate(const Flux_IBLEnvironmentSnapshot& xLive);

	// The frozen snapshot every pass of the current generation reads.
	const Flux_IBLEnvironmentSnapshot& GetActiveEnvironment() const { return m_xActiveEnvironment; }
	bool HasPendingEnvironment() const { return m_bHasPendingEnvironment; }
	const Flux_IBLEnvironmentSnapshot& GetPendingEnvironment() const { return m_xPendingEnvironment; }
	bool IsGenerationInFlight() const { return m_bSkyIBLDirty || m_eRegenState != IBL_REGEN_IDLE; }
	// Monotonic count of COMPLETED (fully coherent) generations. The direct
	// observation behind "generations keep completing under continuous motion".
	u_int GetCompletedGenerationCount() const { return m_uCompletedGenerations; }

	// Graph-free per-frame tick of the regeneration state machine. Everything
	// UpdateGraphPassEnables does except talking to the render graph, so the
	// scheduling behaviour is unit-testable without a GPU or a graph.
	void TickRegenerationForFrame(Flux_IBLRegenFrameWork& xWork);

	// Which buffer consumers sample / which one a generation writes.
	u_int GetFrontBufferIndex() const { return m_uFrontBufferIndex; }
	u_int GetBackBufferIndex() const { return 1u - m_uFrontBufferIndex; }

	// Per-game capture budget, read from Zenith_GraphicsOptions (set once at
	// boot by Project_SetGraphicsOptions) and clamped to a usable range.
	static float GetCaptureCosThreshold();
	static u_int GetPassesPerFrame();

	const Flux_ShaderResourceView& GetBRDFLUTSRV();
	const Flux_ShaderResourceView& GetIrradianceMapSRV();
	const Flux_ShaderResourceView& GetPrefilteredMapSRV();

	// Declare the graph Reads an IBL-sampling pass needs (BRDF LUT + BOTH
	// irradiance/prefiltered buffers). Both cubes must be declared because the
	// front index flips at generation boundaries while SetupRenderGraph runs
	// only on a graph rebuild -- the declared Read is what drives the
	// RTV->SRV barrier and what the read-declaration validator checks against
	// whichever SRV the persistent VIEW set resolved this frame.
	void DeclareConsumerReads(Flux_RenderGraph& xGraph, Flux_PassHandle xPass);

	// There is deliberately NO intensity scale on IBL. The irradiance and
	// prefiltered cubes are energy-correct integrals (E/pi convention) of the
	// SAME atmosphere the frame renders and the direct sun key derives from,
	// so any scale here can only break sun<->sky energy consistency. The old
	// 0.5 was a fudge compensating the auto-exposure metering of the day.

	bool IsEnabled() const;
	bool IsReady() const { return m_bIBLReady; }
	bool IsDiffuseEnabled() const;
	bool IsSpecularEnabled() const;
	bool IsShowBRDFLUT() const;
	bool IsForceRoughness() const;
	float GetForcedRoughness() const;

#ifdef ZENITH_TOOLS
	void RegisterDebugVariables();
#endif

	// Render-graph execute callbacks -- stay static so they satisfy
	// Flux_RenderGraph_OnRecordFunc (void(*)(Flux_CommandBuffer*, void*)).
	// They reach engine state via g_xEngine.IBL() at call time.
	static void ExecuteBRDFLUTPass(Flux_CommandBuffer* pxCmd, void* pUserData);
	static void ExecuteMultiScatterLUTPass(Flux_CommandBuffer* pxCmd, void* pUserData);
	static void ExecuteIrradianceFacePass(Flux_CommandBuffer* pxCmd, void* pUserData);
	static void ExecutePrefilterMipFacePass(Flux_CommandBuffer* pxCmd, void* pUserData);

	// Per-pass user data struct -- small POD holding the (mip, face) the pass
	// targets. Pointer-stable per-pass storage (m_axPrefilterPassData below)
	// lets the graph hold a &element as void* without lifetime concerns; the
	// owning Impl outlives the graph.
	struct IBLPrefilterPassData
	{
		u_int m_uMip;
		u_int m_uFace;
	};

private:
	void CreateRenderTargets();
	void DestroyRenderTargets();

	void ResetIBLRegenStateForRecompile();
	bool ResolveBRDFLUTRun();
	void RunFirstGenerationFrame(Flux_IBLRegenFrameWork& xWork);
	void AdvanceAmortizedRegen(Flux_IBLRegenFrameWork& xWork);
	void ApplyResolvedIBLEnables(Flux_RenderGraph& xGraph, bool bRunBRDF,
		const Flux_IBLRegenFrameWork& xWork);
	// Called the moment a generation reaches idle+clean. It only ARMS the
	// publication -- see PublishCompletedGeneration for why nothing may change
	// in the completing frame.
	void OnGenerationComplete();
	// Publishes the previous frame's completed generation: swaps front/back and
	// promotes the coalesced pending target. Deferred by one frame ON PURPOSE --
	// the completing frame's passes have not been recorded yet when
	// OnGenerationComplete runs, so mutating the snapshot or the buffer index
	// there would hand those passes the next generation's sky / the front cube.
	void PublishCompletedGeneration();

public:
	// State flags.
	bool                       m_bBRDFLUTGenerated = false;
	bool                       m_bSkyIBLDirty      = true;
	bool                       m_bIBLReady         = false;
	bool                       m_bFirstGeneration  = true;

	// Render attachments.
	//
	// Coherent publication: the irradiance + prefiltered cubes are DOUBLE
	// BUFFERED. A 48-pass generation takes 6 frames at 8 passes/frame, and
	// DeferredShading / Translucency sample the cubes on every one of those
	// frames -- writing in place therefore exposed a cube whose mips/faces came
	// from two different skies for 5 frames out of every 6. The generation
	// writes the BACK pair; the pair is swapped to front only once the whole
	// generation is complete, so a sampled cube is always one coherent capture.
	Flux_RenderAttachment      m_xBRDFLUT;
	Flux_RenderAttachmentCube  m_axIrradianceMap[IBLConfig::uBUFFER_COUNT];
	Flux_RenderAttachmentCube  m_axPrefilteredMap[IBLConfig::uBUFFER_COUNT];
	// The capture's OWN multiple-scattering LUT, baked from the frozen
	// generation snapshot. Single-buffered: it is rebuilt (identically) on every
	// frame of a generation from an immutable snapshot, and nothing outside the
	// generation samples it, so there is no half-updated state to publish.
	Flux_RenderAttachment      m_xMultiScatterLUT;

	// Pipelines + shaders.
	Flux_Pipeline              m_xBRDFLUTPipeline;
	Flux_Pipeline              m_xMultiScatterLUTPipeline;
	Flux_Pipeline              m_xIrradianceConvolvePipeline;
	Flux_Pipeline              m_xPrefilterPipeline;
	Flux_Shader                m_xBRDFLUTShader;
	Flux_Shader                m_xMultiScatterLUTShader;
	Flux_Shader                m_xIrradianceConvolveShader;
	Flux_Shader                m_xPrefilterShader;

	// Regen state machine.
	IBL_RegenState             m_eRegenState = IBL_REGEN_IDLE;
	u_int                      m_uRegenFace  = 0;
	u_int                      m_uRegenMip   = 0;

	// Environment capture. m_xActiveEnvironment is the frozen target of the
	// in-flight generation (and, when idle, of the last COMPLETED one);
	// m_xPendingEnvironment coalesces every change that arrives mid-generation
	// into one latest target.
	Flux_IBLEnvironmentSnapshot m_xActiveEnvironment;
	Flux_IBLEnvironmentSnapshot m_xPendingEnvironment;
	bool                       m_bHasActiveEnvironment  = false;
	bool                       m_bHasPendingEnvironment = false;
	u_int                      m_uCompletedGenerations  = 0u;

	// Publication state. m_bPublishPending is armed when a generation completes
	// and consumed at the top of the NEXT tick (buffer swap + pending promotion).
	u_int                      m_uFrontBufferIndex = 0u;
	bool                       m_bPublishPending   = false;

	// Pass handles. Indexed [buffer][...] -- both buffers own a full pass set;
	// only the back buffer's are enabled during an amortised generation, while
	// a first generation seeds BOTH (which also guarantees the compile-time
	// validator sees an enabled writer for each cube consumers read).
	Flux_PassHandle            m_xBRDFLUTPassHandle = {};
	// Declared BEFORE the convolution passes so producer-before-consumer holds
	// inside IBL's own setup walk (the Skybox's copy is declared later, in its
	// own feature, and is never read from here).
	Flux_PassHandle            m_xMultiScatterLUTPassHandle = {};
	Flux_PassHandle            m_axIrradianceFacePassHandles[IBLConfig::uBUFFER_COUNT][6] = {};
	Flux_PassHandle            m_axPrefilterMipFacePassHandles[IBLConfig::uBUFFER_COUNT][IBLConfig::uPREFILTER_MIP_COUNT][6] = {};

	// Pointer-stable per-pass user data. Populated + registered with the graph
	// in SetupRenderGraph; the graph hands these back to the Execute*Pass
	// trampolines as void*. Members (not file-static) so they carry no
	// module-scope static; the Impl outlives any graph that points at them.
	u_int                      m_auIrradianceFaceData[6] = { 0, 1, 2, 3, 4, 5 };
	IBLPrefilterPassData       m_axPrefilterPassData[IBLConfig::uPREFILTER_MIP_COUNT][6] = {};
};
